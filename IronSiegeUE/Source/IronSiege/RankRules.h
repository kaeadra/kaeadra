#pragma once
// Engine-independent driver ranks: every driver earns experience from the fights they drive in and
// climbs five ranks, each one sharpening their perk and shortening their ability's cooldown. No
// Unreal includes on purpose: covered offline by Tests/rank_rules_test.cpp.

#include "CrewRules.h"

namespace IronRanks
{
inline constexpr int MaxRank = 5;

// Experience needed to reach each rank (rank 1 is where every driver starts).
inline int Threshold(int Rank)
{
	static const int Xp[MaxRank] = { 0, 300, 800, 1600, 3000 };
	return Xp[Rank < 1 ? 0 : (Rank > MaxRank ? MaxRank - 1 : Rank - 1)];
}

inline int RankFor(int Xp)
{
	int Rank = 1;
	while (Rank < MaxRank && Xp >= Threshold(Rank + 1)) ++Rank;
	return Rank;
}

// 0..1 of the way from this rank to the next (1 at the top rank), for the HUD bar.
inline float ProgressToNext(int Xp)
{
	const int Rank = RankFor(Xp);
	if (Rank >= MaxRank) return 1.f;
	const int From = Threshold(Rank), To = Threshold(Rank + 1);
	const float F = static_cast<float>(Xp - From) / static_cast<float>(To - From);
	return F < 0.f ? 0.f : (F > 1.f ? 1.f : F);
}

struct Tuning
{
	int PerKill = 10;
	int PerWave = 25;          // Survival: each wave reached.
	int MissionWin = 120;      // Campaign: a mission completed...
	int PerStar = 40;          // ...plus this per star earned.
	int MissionLoss = 30;      // Showing up still teaches something.
	float PerkGrowth = 0.15f;  // Each rank above 1 strengthens the perk's effect by this share.
	float CooldownCut = 0.05f; // Each rank above 1 takes this share off the ability cooldown.
};

// Experience from one match. Stars count only when the mission was won.
inline int MatchXp(int Kills, int WavesReached, bool bMission, bool bWon, int Stars, const Tuning& T = Tuning())
{
	int Xp = (Kills > 0 ? Kills : 0) * T.PerKill;
	if (bMission)
	{
		Xp += bWon ? T.MissionWin + (Stars > 0 ? Stars : 0) * T.PerStar : T.MissionLoss;
	}
	else
	{
		Xp += (WavesReached > 0 ? WavesReached : 0) * T.PerWave;
	}
	return Xp;
}

// A perk multiplier at a rank: the distance from 1 grows by PerkGrowth per rank, so a +30% nitro
// perk becomes +48% at rank 5 and a -20% gun heat perk becomes -32%. Never below a fifth.
inline float ScalePerkValue(float Value, int Rank, const Tuning& T = Tuning())
{
	const int R = Rank < 1 ? 1 : (Rank > MaxRank ? MaxRank : Rank);
	const float Scaled = 1.f + (Value - 1.f) * (1.f + T.PerkGrowth * static_cast<float>(R - 1));
	return Scaled < 0.2f ? 0.2f : Scaled;
}

inline IronCrew::Perk RankedPerk(const IronCrew::Perk& P, int Rank, const Tuning& T = Tuning())
{
	return { ScalePerkValue(P.BoostRecharge, Rank, T), ScalePerkValue(P.GunHeat, Rank, T), ScalePerkValue(P.MaxArmor, Rank, T),
		ScalePerkValue(P.SupplyGain, Rank, T), ScalePerkValue(P.RocketReload, Rank, T), ScalePerkValue(P.FlareRecharge, Rank, T) };
}

// Multiplier on the ability cooldown at a rank (1 at rank 1, 0.8 at rank 5).
inline float CooldownScale(int Rank, const Tuning& T = Tuning())
{
	const int R = Rank < 1 ? 1 : (Rank > MaxRank ? MaxRank : Rank);
	return 1.f - T.CooldownCut * static_cast<float>(R - 1);
}

// Every driver's experience, as saved.
struct Roster
{
	int Xp[IronCrew::DriverCount] = {};

	int Rank(int Driver) const { return Driver >= 0 && Driver < IronCrew::DriverCount ? RankFor(Xp[Driver]) : 1; }

	// Adds experience; returns the new rank if it went up, else 0.
	int Award(int Driver, int Amount)
	{
		if (Driver < 0 || Driver >= IronCrew::DriverCount || Amount <= 0) return 0;
		const int Before = RankFor(Xp[Driver]);
		const int Cap = 1000000;
		Xp[Driver] = Xp[Driver] > Cap - Amount ? Cap : Xp[Driver] + Amount;
		const int After = RankFor(Xp[Driver]);
		return After > Before ? After : 0;
	}
};
}
