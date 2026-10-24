#include "IndividualProgressionAttunement.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "IndividualProgression.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"

namespace
{
    constexpr uint32 QUEST_DRAKEFIRE_AMULET                   = 6502; // Alliance
    constexpr uint32 QUEST_BLOOD_OF_THE_BLACK_DRAGON_CHAMPION = 6602; // Horde
    constexpr uint32 QUEST_SEAL_OF_ASCENSION                  = 4743;
    constexpr uint32 ITEM_SEAL_OF_ASCENSION_KEY               = 12344;

    // Read at config load: the checks run on map threads, and the config manager's environment
    // variable cache isn't safe to fill from several threads at once.
    uint32 onyxiaAchievement = 0;
    uint32 upperBlackrockSpireAchievement = 0;
    bool accountWide = false;

    bool HasAchievement(Player* player, uint32 id)
    {
        return id && player->HasAchieved(id);
    }

    bool OnyxiaQuestDone(Player* player)
    {
        return player->GetQuestRewardStatus(QUEST_DRAKEFIRE_AMULET) || player->GetQuestRewardStatus(QUEST_BLOOD_OF_THE_BLACK_DRAGON_CHAMPION);
    }

    bool UpperBlackrockSpireQuestDone(Player* player)
    {
        return player->GetQuestRewardStatus(QUEST_SEAL_OF_ASCENSION);
    }

    // The core awards the achievement when the quest is handed in (its criteria are "complete quest").
    // This catches characters who handed it in before the achievement existed.
    void AwardIfQuestDone(Player* player, uint32 id, bool questDone)
    {
        if (!id || !questDone || player->HasAchieved(id))
            return;

        AchievementEntry const* entry = sAchievementStore.LookupEntry(id);
        if (!entry)
        {
            LOG_ERROR("module", "IndividualProgression.Attunement: no achievement {}", id);
            return;
        }

        player->CompletedAchievement(entry);
    }

    // Alts: when another character on the account has the attunement achievement, this one gets it too.
    // Only the achievement is looked at: a character who did the quest gets it at its own login.
    void AwardFromAccount(Player* player)
    {
        std::string ids;
        for (uint32 id : { onyxiaAchievement, upperBlackrockSpireAchievement })
            if (id && !player->HasAchieved(id))
                ids += (ids.empty() ? "" : ",") + std::to_string(id);

        if (ids.empty())
            return;

        // The callback belongs to the session, so the session outlives it; the player may not (logout).
        WorldSession* session = player->GetSession();
        ObjectGuid guid = player->GetGUID();
        session->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
            "SELECT DISTINCT a.achievement FROM character_achievement a JOIN characters c ON c.guid = a.guid "
            "WHERE c.account = {} AND c.guid <> {} AND a.achievement IN ({})",
            session->GetAccountId(), guid.GetCounter(), ids)).WithCallback([session, guid](QueryResult result)
        {
            if (!result)
                return;

            // Not ObjectAccessor::FindPlayer: that misses a player in the middle of a teleport.
            Player* player = session->GetPlayer();
            if (!player || player->GetGUID() != guid)
                return;

            do
                AwardIfQuestDone(player, result->Fetch()[0].Get<uint16>(), true);
            while (result->NextRow());
        }));
    }
}

bool IPAttunement::Onyxia(Player* player)
{
    return OnyxiaQuestDone(player) || HasAchievement(player, onyxiaAchievement) || player->HasItemCount(ITEM_DRAKEFIRE_AMULET);
}

bool IPAttunement::UpperBlackrockSpire(Player* player)
{
    return UpperBlackrockSpireQuestDone(player) || HasAchievement(player, upperBlackrockSpireAchievement) || player->HasItemCount(ITEM_SEAL_OF_ASCENSION_KEY);
}

class IndividualProgression_Attunement_WorldScript : public WorldScript
{
public:
    IndividualProgression_Attunement_WorldScript() : WorldScript("IndividualProgression_Attunement_WorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        onyxiaAchievement = sConfigMgr->GetOption<uint32>("IndividualProgression.Attunement.OnyxiaAchievement", 0);
        upperBlackrockSpireAchievement = sConfigMgr->GetOption<uint32>("IndividualProgression.Attunement.UpperBlackrockSpireAchievement", 0);
        accountWide = sConfigMgr->GetOption<bool>("IndividualProgression.Attunement.AccountWide", false);
    }
};

class IndividualProgression_Attunement_PlayerScript : public PlayerScript
{
public:
    IndividualProgression_Attunement_PlayerScript() : PlayerScript("IndividualProgression_Attunement_PlayerScript", { PLAYERHOOK_ON_LOGIN }) { }

    void OnPlayerLogin(Player* player) override
    {
        // Random bots are attuned by the quest anyway; awarding them would only spam their guilds.
        if (!sIndividualProgression->enabled || player->IsGameMaster() || sIndividualProgression->isBotAccount(player))
            return;

        // A second after login, like the core's own login achievement check: while the client is
        // still loading it wouldn't show the "achievement earned" toast.
        player->m_Events.AddEventAtOffset([player]
        {
            AwardIfQuestDone(player, onyxiaAchievement, OnyxiaQuestDone(player));
            AwardIfQuestDone(player, upperBlackrockSpireAchievement, UpperBlackrockSpireQuestDone(player));

            if (accountWide)
                AwardFromAccount(player);
        }, 1s);
    }
};

void AddSC_mod_individual_progression_attunement()
{
    new IndividualProgression_Attunement_WorldScript();
    new IndividualProgression_Attunement_PlayerScript();
}
