#pragma once
// Engine-independent wave progression and between-wave supply drops.
// No Unreal includes on purpose: covered offline by Tests/wave_rules_test.cpp.

namespace IronWaves
{
struct WaveTuning
{
	int FirstWaveEnemies = 3;
	int EnemiesAddedPerWave = 1;
	int MaxEnemiesPerWave = 8;
};

// Enemies in wave N (1-based): grows by EnemiesAddedPerWave, capped at MaxEnemiesPerWave.
inline int EnemiesForWave(int Wave, const WaveTuning& T = WaveTuning())
{
	if (Wave < 1) Wave = 1;
	const int Count = T.FirstWaveEnemies + (Wave - 1) * T.EnemiesAddedPerWave;
	return Count > T.MaxEnemiesPerWave ? T.MaxEnemiesPerWave : (Count < 1 ? 1 : Count);
}

// Fast "raider" enemies (sports cars) mixed into wave N out of Total: none before wave 3, then one
// more every second wave, never more than half the wave.
inline int FastEnemiesForWave(int Wave, int Total)
{
	if (Wave < 3) return 0;
	const int Fast = (Wave - 1) / 2;
	const int Cap = Total / 2;
	return Fast > Cap ? Cap : Fast;
}

// Missile "hunters" (guided-missile buggies that hang back and fire at the player) mixed into wave
// N out of Total, on top of the raiders: none before wave 4, then one more every third wave, never
// more than a third of the wave - and never more than what the raiders leave free.
inline int HuntersForWave(int Wave, int Total)
{
	if (Wave < 4) return 0;
	const int Hunters = 1 + (Wave - 4) / 3;
	const int Cap = Total / 3;
	const int Free = Total - FastEnemiesForWave(Wave, Total);
	int N = Hunters > Cap ? Cap : Hunters;
	N = N > Free ? Free : N;
	return N < 0 ? 0 : N;
}

// Energy-weapon specialists in the late waves, taken from what raiders and hunters leave free:
// tesla "stormers" (rush in and chain lightning that kills engines) from wave 6, railgun "lancers"
// (hang back, charge, fire a slug through everything in line) from wave 7. One more of each every
// few waves, each capped at a quarter of the wave.
inline int StormersForWave(int Wave, int Total)
{
	if (Wave < 6) return 0;
	const int Wanted = 1 + (Wave - 6) / 3;
	const int Cap = Total / 4;
	const int Free = Total - FastEnemiesForWave(Wave, Total) - HuntersForWave(Wave, Total);
	int N = Wanted > Cap ? Cap : Wanted;
	N = N > Free ? Free : N;
	return N < 0 ? 0 : N;
}

inline int LancersForWave(int Wave, int Total)
{
	if (Wave < 7) return 0;
	const int Wanted = 1 + (Wave - 7) / 4;
	const int Cap = Total / 4;
	const int Free = Total - FastEnemiesForWave(Wave, Total) - HuntersForWave(Wave, Total) - StormersForWave(Wave, Total);
	int N = Wanted > Cap ? Cap : Wanted;
	N = N > Free ? Free : N;
	return N < 0 ? 0 : N;
}

// Every fifth wave adds a Juggernaut boss on top of the normal enemies.
inline bool IsBossWave(int Wave)
{
	return Wave > 0 && Wave % 5 == 0;
}

// Supply drops after clearing wave N: always one repair kit and one ammo crate, plus an extra
// repair kit from wave 4 on, when waves get big enough to wear the player down.
struct SupplyDrop
{
	int RepairKits;
	int AmmoCrates;
};

inline SupplyDrop SuppliesAfterWave(int ClearedWave)
{
	return { ClearedWave >= 4 ? 2 : 1, 1 };
}
}
