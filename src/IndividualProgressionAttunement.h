#ifndef AZEROTHCORE_INDIVIDUALPROGRESSION_ATTUNEMENT_H
#define AZEROTHCORE_INDIVIDUALPROGRESSION_ATTUNEMENT_H

class Player;

// Onyxia's Lair (level 60) and Upper Blackrock Spire let a character in once they have finished the
// attunement quest, not only while the key is in their bags. The key still counts (".ip attune onyxia"
// hands it out), and so does the achievement set in IndividualProgression.Attunement.*Achievement.
// With IndividualProgression.Attunement.AccountWide, alts get those achievements from the account at login.
namespace IPAttunement
{
    bool Onyxia(Player* player);
    bool UpperBlackrockSpire(Player* player);
}

#endif
