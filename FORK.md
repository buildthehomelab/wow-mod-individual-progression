# About this fork

This is [buildthehomelab](https://github.com/buildthehomelab)'s fork of
[ZhengPeiRu21/mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression).
It exists so that changes to the module don't have to wait for an upstream release, and it is
set up so that upstream's work keeps flowing in.

## Branches

| Branch | What it is |
| --- | --- |
| `master` | An exact copy of upstream's `master`. Nothing of the fork's is ever committed to it. |
| `main` | `master` plus the fork's changes. The default branch, and the one server builds pin. |

## Pulling upstream in

**Actions → Upstream sync** runs every morning (and by hand). It fast-forwards `master` to
upstream and, when `main` is missing commits, opens an issue with the label `upstream`: the
new commits and whether they merge into `main` without conflicts. The issue closes by itself
once `main` has them.

To pull them in:

```bash
git fetch origin
git checkout -b merge-upstream origin/main
git merge origin/master        # resolve conflicts here, if the issue listed any
git push -u origin merge-upstream
```

Open a PR into `main` and merge it with a **merge commit**. Not squash and not rebase: those
rewrite upstream's commits into new ones, and every later merge then conflicts with itself.
CI ([wow-ci](https://github.com/buildthehomelab/wow-ci)) checks that the result still compiles
against the core it builds with. Afterwards bump the pin in wow-ci, and in the server build that
pins this fork, as for any other module.

## Rules for the fork's changes

They exist to keep that merge boring.

- New code goes in **new files** (one line in `src/IndividualProgression_loader.cpp` registers
  a script). Don't reformat or reorganise upstream's files.
- Edit upstream's SQL in place only where the row itself is wrong or incomplete, and keep the
  edit as small as the row allows.
- Anything that isn't specific to this fork also goes upstream as a PR, from a branch based on
  `master`. Once upstream merges it, the next sync makes the fork's copy of it disappear from the
  difference.

## What `main` carries on top of upstream

| Change | Files | Upstream |
| --- | --- | --- |
| Every existing account is matched against the bot and excluded account patterns once, in one query when the config loads, so logins and the damage and heal hooks only wait for accounts created after the last config load (upstream's per-account cache, [#1308](https://github.com/ZhengPeiRu21/mod-individual-progression/pull/1308), is merged) | `src/IndividualProgression.cpp`, `conf/individualProgression.conf.dist` | fork only |
| `.ip set`, `tele`, `setbot`, `setrep` and `attune` stop early instead of crashing the server when run from the worldserver console or SOAP (null session); `set` and `tele` use the named player instead of ignoring it | `src/cs_individualProgression.cpp` | fork only ([#1309](https://github.com/ZhengPeiRu21/mod-individual-progression/pull/1309) was closed without merging) |
| Level 60 Onyxia hovers at and flies along the line her Deep Breath burns (the core moved the trails) | `src/vanillaScripts/boss_onyxia.cpp` | not sent yet (branch `claude/onyxia-deep-breath-trails-upstream`) |
| `.ip power`: damage and healing factor per instance, set in game (no restart). When merging upstream, check that every read of the four `*PowerAdjustment` / `*HealingAdjustment` values in the hooks is still wrapped in `IPInstancePower::Power` / `Healing` | `src/IndividualProgressionInstancePower.*`, `src/IndividualProgressionPlayer.cpp` (the eight reads), `data/sql/world/base/cs_individualProgression_power.sql`, `conf/individualProgression.conf.dist` (`InstancePower.GMOnly`) | fork only |
| Prayer of Fortitude, Gift of the Wild and Arcane Brilliance stay on the trainers at their vanilla levels instead of level 61, because playerbots only learn spells from trainers | `data/sql/world/base/class_trainers.sql` (5 rows) | fork only (for servers that run bot raids) |
| Level 60 Onyxia's Lair and Upper Blackrock Spire let you in once the attunement quest is done (or with an achievement set in the config), instead of only with the key in your bags; the configured achievement is awarded at login to characters who did the quest before. With `Attunement.AccountWide`, alts get the achievement at login when another character on the account has it, so they skip the attunement | `src/IndividualProgressionAttunement.*`, the key checks in `src/IndividualProgressionPlayer.cpp`, `src/IndividualProgression.cpp`, `src/cs_individualProgression.cpp`, `src/vanillaScripts/instance_onyxias_lair.cpp`, `src/vanillaScripts/instance_blackrock_spire.cpp`, `conf/individualProgression.conf.dist` | not sent yet |
| Recipes sold by NPCs are in unlimited stock (`maxcount`/`incrtime` 0) instead of 1 per few hours. A server that unlimits the core's recipe rows with its own SQL would otherwise get these back as limited whenever this file changes and re-applies | `data/sql/world/base/vanilla_vendors.sql` (21 rows) | fork only |
| Bleeds (Rip, Rake, Rupture, Garrote, Rend, Deep Wounds, Lacerate, Pounce) work on every creature except the 70 that AzerothCore itself makes bleed-immune (Uldaman's Earthen, Web Wrap, Ice Tomb, Spirit Guides, a few Northrend rares, pets and objects). The block at the end of the vanilla immunities file gives those 70 a copy of their row with BLEED, moves everything else off the core's BLEED rows onto copies without it (copies at ID - 20000, -20001..-39999), and clears BLEED from this file's rows. Every other immunity stays. It runs again whenever upstream changes the file and it re-applies; keep it the file's last block when merging upstream. After a core pin bump: `SELECT COUNT(*) FROM creature_template ct JOIN creature_immunities ci ON ci.ID = ct.CreatureImmunitiesId WHERE ci.MechanicsMask & 32768` must be 70 (more = the core made a new creature bleed-immune: add it to the list; fewer = the core dropped or deleted one: take it out), and `SELECT f.ID FROM creature_immunities f LEFT JOIN creature_immunities s ON s.ID = f.ID + 20000 WHERE f.ID BETWEEN -39999 AND -20001 AND (s.ID IS NULL OR f.MechanicsMask | 32768 <> s.MechanicsMask | 32768 OR f.SchoolMask <> s.SchoolMask OR f.DispelTypeMask <> s.DispelTypeMask OR f.Effects <> s.Effects OR f.Auras <> s.Auras OR f.ImmuneAoE <> s.ImmuneAoE OR f.ImmuneChain <> s.ImmuneChain)` must return nothing (a row = the core changed a row we copied; the copy refreshes when this file next re-applies) | `data/sql/world/base/vanilla_creature_immunities.sql` (last block) | fork only |
| This file and the workflows | `FORK.md`, `.github/workflows/` | fork only |
