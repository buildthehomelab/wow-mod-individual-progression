# About this fork

This is [buildthehomelab](https://github.com/buildthehomelab)'s fork of
[ZhengPeiRu21/mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression).
The realm runs it instead of upstream so that changes to the module don't have to wait for an
upstream release, and it is set up so that upstream's work keeps flowing in.

## Branches

| Branch | What it is |
| --- | --- |
| `master` | An exact copy of upstream's `master`. Nothing of ours is ever committed to it. |
| `main` | `master` plus our changes. The default branch, and the one the server pins. |

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
CI checks that the result still compiles against the server's core. Afterwards bump the pin in
`wow-server` and `wow-ci` as for any other module.

## Rules for our changes

They exist to keep that merge boring.

- New code goes in **new files** (one line in `src/IndividualProgression_loader.cpp` registers
  a script). Don't reformat or reorganise upstream's files.
- Edit upstream's SQL in place only where the row itself is wrong or incomplete, and keep the
  edit as small as the row allows.
- Anything that isn't specific to our realm also goes upstream as a PR, from a branch based on
  `master`. Once upstream merges it, the next sync makes our copy of it disappear from the
  difference.

## What `main` carries on top of upstream

| Change | Files | Upstream |
| --- | --- | --- |
| War Effort Commanders report a player's progress; text for 'Complete the War Effort' | `src/vanillaScripts/aq_we_progress.cpp`, `data/sql/world/base/aq_war_effort.sql` | [#1307](https://github.com/ZhengPeiRu21/mod-individual-progression/pull/1307) |
| Bot and excluded account checks are answered from a per-account cache instead of a login database query and a regex compile per damage or heal event | `src/IndividualProgression.cpp`, `src/IndividualProgression.h`, `src/IndividualProgressionPlayer.cpp`, `conf/individualProgression.conf.dist` | [#1308](https://github.com/ZhengPeiRu21/mod-individual-progression/pull/1308) |
| Level 60 Onyxia hovers at and flies along the line her Deep Breath burns (the core moved the trails) | `src/vanillaScripts/boss_onyxia.cpp` | not sent yet (branch `claude/onyxia-deep-breath-trails-upstream`) |
| `.ip power`: damage and healing factor per instance, set in game (no restart). When merging upstream, check that every read of the four `*PowerAdjustment` / `*HealingAdjustment` values in the hooks is still wrapped in `IPInstancePower::Power` / `Healing` | `src/IndividualProgressionInstancePower.*`, `src/IndividualProgressionPlayer.cpp` (the eight reads), `data/sql/world/base/cs_individualProgression_power.sql`, `conf/individualProgression.conf.dist` (`InstancePower.GMOnly`) | fork only |
| Prayer of Fortitude, Gift of the Wild and Arcane Brilliance stay on the trainers at their vanilla levels instead of level 61, because playerbots only learn spells from trainers | `data/sql/world/base/class_trainers.sql` (5 rows) | fork only (realm runs bot raids) |
| This file and the workflows | `FORK.md`, `.github/workflows/` | fork only |
