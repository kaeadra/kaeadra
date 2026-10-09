# Shared task board

Update this file when you take, finish or hand over a task (pull first, push right after).
Status: `todo` · `taking: <session>` · `needs build` · `needs play-test` · `done`

## Waiting on a local build

| Change | Commit | Status | Notes |
|---|---|---|---|
| Machine gun fires full-auto while held | 3c489ec | needs build | `IronSiegePlayerController.cpp` binds FirePrimary to Triggered |
| Wrecked car cannot fire a charging railgun | 3c489ec | needs build | `WarVehiclePawn.cpp` TickEnergyWeapons |
| No mission win while the player's car is down | 3c489ec | needs build | `IronMissionDirector.cpp` Tick |
| Campaign Act II (missions 9-12, Iron Crown boss) | 92b26a6 | needs build + play-test | Play each new mission once; check spots/routes on the maps |
| Driver ranks and XP | 92b26a6 | needs build + play-test | `DebugDriverXp -1 3000` to see the rank screens |
| Smarter enemy AI (squad tactics) | see `git log` | needs build + play-test | Survival wave 4+ or any mission. Watch for: enemies arriving from different sides, cutting you off when you drive past, swerving while in your sights, not shooting through a wingman, a badly hurt one fleeing then showing REPAIRING over its health bar. `DebugTactics 0` / `1` to compare with the old chase. Check that city streets still route well |
| Boss phases and signature attacks | 18d30e3 | needs build + play-test | `DebugSpawnBoss 2 4000` (0 Juggernaut, 1 Raven, 2 Baron, 3 Crown), then `DebugBossHealth 0.5` / `0.2` to step through the phases. Check: shield, escorts, radio line, slam/EMP warning banner, barrage, Raven's second rail shot and mines |

## Ideas / next up

| Task | Status | Owner |
|---|---|---|
| Story scenes between missions (portraits + dialogue) | todo | |
| New modes: combat race, daily challenges | todo | |
| Disable the stray plugins in `IronSiege.uproject` (Adjust, APVDecoderElectra, AESHandlerComponent, AESGCMHandlerComponent, ActorPalette, ActorModifier, ActorModifierCore, AccumulationDOF) - check nothing uses them | todo | local |
| Railgun and tesla do not damage mission structures (decide if intended) | todo | |

## Log

- 2026-10-04 cloud `session_018pYbz3pUwGwawu7qyYT8Sd`: reviewed the code, fixed 3 bugs, added Act II and driver ranks, set up this board.
- 2026-10-09 cloud `session_018pYbz3pUwGwawu7qyYT8Sd`: boss fights - three phases (shield, armour patch, escorts, radio line, fiercer AI) and signature attacks (ground slam, rocket barrage, Crown EMP, Raven's double rail and mines). `BossRules.h` + `Tests/boss_rules_test.cpp`, `UIronBossComponent`, HUD phase bar and warning banner, `DebugSpawnBoss` / `DebugBossHealth`. Next for cloud: smarter enemy AI, unless someone claims it.
- 2026-10-09 cloud `session_018pYbz3pUwGwawu7qyYT8Sd`: smarter enemy AI - `TacticsRules.h` + `Tests/tactics_rules_test.cpp` (intercept, pincer lanes, weaving in the player's sights, line-of-fire discipline, fall back and repair once), wired into `AIronSiegeAIController`; re-aligns a car parked facing away; REPAIRING label on the enemy health bar; `DebugTactics`. Next free tasks: story scenes, new modes.
