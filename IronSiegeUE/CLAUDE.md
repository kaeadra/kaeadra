# Iron Siege - working notes for every Claude session

Iron Siege is a vehicular-combat game in **Unreal Engine 5.8** (C++ module `IronSiege`): armed
Chaos-physics cars, a twelve-mission story campaign in two acts, a survival wave mode, six drivers
with anime-style portraits, perks, abilities and ranks, and a full Arabic translation.

Several Claude sessions work on this project at once. Read `COLLAB.md` before starting and update
it when you finish. The user writes in Arabic: answer them in Arabic.

## Where the project lives

- GitHub: `kaeadra/kaeadra`, branch `claude/ironsiegeue-review-dev-6p9ibx`, folder `IronSiegeUE/`.
- On the user's PC the working project is `C:\Users\ibrah\Desktop\kaeadra\IronSiegeUE`
  (a git clone; `Content/` is local only - see `.gitignore`). Engine: `C:\Program Files\Epic Games\UE_5.8`.

## Who does what

| Session | Can | Owns |
|---|---|---|
| Local (Claude Code on the user's PC, desktop app "Local" or `claude` in the folder) | Build, run the editor and `-game`, edit Content, run `Content/Python` scripts | Compiling, fixing build errors, assets/Blueprints/maps, in-game testing, screenshots |
| Cloud (claude.ai/code sessions on the GitHub branch) | Edit C++/config/Python, compile the pure `*Rules.h` headers and run `Tests/` with g++ | Gameplay systems, rules + tests, Arabic text |

A cloud session cannot compile Unreal code: everything it pushes is marked **needs build** in
`COLLAB.md` until a local session has built it and played it.

## Workflow (every session)

1. `git pull` before you start; claim the task in `COLLAB.md` (commit + push that change at once).
2. Small commits with clear messages; `git pull --rebase` then push when done.
3. Local session, after pulling C++ changes: build, then report the result in `COLLAB.md`
   (build OK / errors with the compiler output) and fix build errors it can.
4. Never commit `Binaries/`, `Intermediate/`, `Saved/` or `Content/` assets (only `Content/Python`
   and `Content/IronSiege/Characters`).

## Build and test

Build the editor target (close the editor first, or use Live Coding):
```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" IronSiegeEditor Win64 Development "C:\Users\ibrah\Desktop\kaeadra\IronSiegeUE\IronSiege.uproject" -waitmutex
```
Offline rules tests (any machine with g++/clang, from `IronSiegeUE/Tests`):
```
g++ -std=c++20 -Wall -Wextra -I../Source/IronSiege campaign_ranks_test.cpp -o t && ./t
```
Useful console commands in a development build: `DebugCampaign unlock 3`, `DebugDriverXp -1 3000`,
and the other `Debug*` commands in `IronSiegeCheatManager.h`.

## Code conventions

- Game rules live in engine-independent headers `Source/IronSiege/*Rules.h` (no Unreal includes);
  the Unreal classes only apply them. New rules get a test in `Tests/`.
- Comments explain *why*, in full sentences, like the existing code. Match the surrounding style.
- Every player-visible string goes through `IronText::Str` / `TLOC` with a key; its Arabic goes in
  `IronSiegeText.cpp` (UI) or `IronSiegeStory.cpp` (campaign, crew, story screens).
- The HUD is Canvas-drawn (`IronSiegeHUD*.cpp`), menus are Slate (`SIronSettingsMenu.cpp`).
- Campaign data: `MissionRules.h`; drivers, barks: `CrewRules.h`; ranks: `RankRules.h`.
