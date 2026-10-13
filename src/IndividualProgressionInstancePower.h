#ifndef AZEROTHCORE_INDIVIDUALPROGRESSION_INSTANCE_POWER_H
#define AZEROTHCORE_INDIVIDUALPROGRESSION_INSTANCE_POWER_H

class Player;

// Per-instance damage and healing factors, set in game with ".ip power". They take the place of the
// factor the *PowerAdjustment / *HealingAdjustment config values give. They are kept in memory only.
namespace IPInstancePower
{
    // The factor to use for this player: the one set for the instance they are in, else `configured`.
    float Power(Player const* player, float configured);
    float Healing(Player const* player, float configured);
}

#endif
