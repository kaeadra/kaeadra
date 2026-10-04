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

## Ideas / next up

| Task | Status | Owner |
|---|---|---|
| Boss fights with phases and signature attacks | todo | |
| Smarter enemy AI (flanking, focus fire, retreat to repair) | todo | |
| Story scenes between missions (portraits + dialogue) | todo | |
| New modes: combat race, daily challenges | todo | |
| Disable the stray plugins in `IronSiege.uproject` (Adjust, APVDecoderElectra, AESHandlerComponent, AESGCMHandlerComponent, ActorPalette, ActorModifier, ActorModifierCore, AccumulationDOF) - check nothing uses them | todo | local |
| Railgun and tesla do not damage mission structures (decide if intended) | todo | |

## Log

- 2026-10-04 cloud `session_018pYbz3pUwGwawu7qyYT8Sd`: reviewed the code, fixed 3 bugs, added Act II and driver ranks, set up this board.
