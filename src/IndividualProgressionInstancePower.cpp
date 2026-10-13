#include "IndividualProgressionInstancePower.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "IndividualProgression.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
    constexpr float MIN_FACTOR = 0.01f;
    constexpr float MAX_FACTOR = 10.0f;

    // A factor that is not set leaves that side on the realm settings.
    struct InstancePower
    {
        Optional<float> power;
        Optional<float> healing;
    };

    // Damage and heal hooks read this on map threads; the command writes it on the world thread.
    std::shared_mutex instancePowerLock;
    std::unordered_map<uint32, InstancePower> instancePowerStore; // by instance id
    std::atomic<bool> instancePowerInUse{false};

    InstancePower Find(uint32 instanceId)
    {
        if (!instanceId || !instancePowerInUse.load(std::memory_order_relaxed))
            return {};

        std::shared_lock<std::shared_mutex> guard(instancePowerLock);
        auto itr = instancePowerStore.find(instanceId);
        if (itr == instancePowerStore.end())
            return {};

        return itr->second;
    }

    void Set(uint32 instanceId, InstancePower values)
    {
        std::unique_lock<std::shared_mutex> guard(instancePowerLock);
        instancePowerStore[instanceId] = values;
        instancePowerInUse.store(true, std::memory_order_relaxed);
    }

    bool Remove(uint32 instanceId)
    {
        std::unique_lock<std::shared_mutex> guard(instancePowerLock);
        bool removed = instancePowerStore.erase(instanceId) > 0;
        instancePowerInUse.store(!instancePowerStore.empty(), std::memory_order_relaxed);
        return removed;
    }

    bool IsValidFactor(float factor)
    {
        return factor >= MIN_FACTOR && factor <= MAX_FACTOR;
    }

    std::string Describe(Optional<float> factor)
    {
        return factor ? Acore::StringFormat("|cff00ffff{:.2f}|r", *factor) : "realm setting";
    }

    // Everyone in the instance is told: the change is theirs to live with.
    void TellInstance(Map* map, std::string const& text)
    {
        map->DoForAllPlayers([&text](Player* member)
        {
            if (!member->GetSession()->IsHeadless()) // bots
                ChatHandler(member->GetSession()).SendSysMessage(text);
        });
    }
}

float IPInstancePower::Power(Player const* player, float configured)
{
    return Find(player->GetInstanceId()).power.value_or(configured);
}

float IPInstancePower::Healing(Player const* player, float configured)
{
    return Find(player->GetInstanceId()).healing.value_or(configured);
}

class IndividualProgression_InstancePower_GlobalScript : public GlobalScript
{
public:
    IndividualProgression_InstancePower_GlobalScript() : GlobalScript("IndividualProgression_InstancePower_GlobalScript") { }

    // The instance is gone for good (reset, or its lockout ran out), so its factors are too.
    void OnInstanceIdRemoved(uint32 instanceId) override
    {
        Remove(instanceId);
    }
};

class individualProgression_instancePower_commandscript : public CommandScript
{
public:
    individualProgression_instancePower_commandscript() : CommandScript("individualProgression_instancePower_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        // Joins the "ip" table of cs_individualProgression.cpp.
        static ChatCommandTable commandTable =
        {
            { "ip power", HandlePowerCommand, SEC_PLAYER, Console::No },
        };

        return commandTable;
    }

    static bool HandlePowerCommand(ChatHandler* handler, Optional<Variant<EXACT_SEQUENCE("reset"), float>> powerArg, Optional<float> healingArg)
    {
        Player* player = handler->GetPlayer();
        Map* map = player ? player->FindMap() : nullptr;
        if (!map || !map->IsDungeon())
        {
            handler->SendSysMessage("You need to be inside a dungeon or raid to use .ip power.");
            return true;
        }

        uint32 instanceId = map->GetInstanceId();
        char const* affected = sIndividualProgression->BotOnlyAdjustments ? "bots" : "bots and players";
        InstancePower current = Find(instanceId);

        if (!powerArg)
        {
            handler->PSendSysMessage("{} (instance {}): damage {}, healing {} for {}.",
                map->GetMapName(), instanceId, Describe(current.power), Describe(current.healing), affected);
            handler->PSendSysMessage("Realm settings: Vanilla damage {:.2f}, healing {:.2f} (at level 60, fading out toward level 10); TBC damage {:.2f}, healing {:.2f}.",
                sIndividualProgression->vanillaPowerAdjustment, sIndividualProgression->vanillaHealingAdjustment,
                sIndividualProgression->tbcPowerAdjustment, sIndividualProgression->tbcHealingAdjustment);
            handler->SendSysMessage("Change it here: .ip power $damage [$healing], or .ip power reset.");
            return true;
        }

        // Looking is open to everyone; IndividualProgression.InstancePower.GMOnly decides who may change it.
        // No log for a missing key: the option is ours, and a .conf made from upstream's file does not have it.
        if (sConfigMgr->GetOption<bool>("IndividualProgression.InstancePower.GMOnly", false, false) && handler->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        {
            handler->SendSysMessage("Only game masters can change the power settings of an instance on this realm.");
            return true;
        }

        if (powerArg->holds_alternative<EXACT_SEQUENCE("reset")>())
        {
            if (!Remove(instanceId))
            {
                handler->PSendSysMessage("{} (instance {}) already uses the realm settings.", map->GetMapName(), instanceId);
                return true;
            }

            LOG_INFO("module", "IndividualProgression: {} put {} (instance {}) back on the realm power settings.",
                player->GetName(), map->GetMapName(), instanceId);
            TellInstance(map, Acore::StringFormat("{} put damage and healing for {} in this instance back on the realm settings.",
                player->GetName(), affected));
            return true;
        }

        // Healing left out: it stays as it is in this instance.
        InstancePower values;
        values.power = powerArg->get<float>();
        values.healing = healingArg ? healingArg : current.healing;

        if (!IsValidFactor(*values.power) || (healingArg && !IsValidFactor(*healingArg)))
        {
            handler->PSendSysMessage("Damage and healing must be between {} and {} (1 = full strength).", MIN_FACTOR, MAX_FACTOR);
            return true;
        }

        Set(instanceId, values);

        LOG_INFO("module", "IndividualProgression: {} set power {:.2f}, healing {} in {} (instance {}).",
            player->GetName(), *values.power, values.healing ? Acore::StringFormat("{:.2f}", *values.healing) : "unchanged", map->GetMapName(), instanceId);
        TellInstance(map, Acore::StringFormat("{} set damage {}, healing {} for {} in this instance.",
            player->GetName(), Describe(values.power), Describe(values.healing), affected));
        return true;
    }
};

void AddSC_mod_individual_progression_instance_power()
{
    new IndividualProgression_InstancePower_GlobalScript();
    new individualProgression_instancePower_commandscript();
}
