#pragma once
// Engine-independent between-wave upgrade shop: what each upgrade does, what it costs, and the
// player's owned levels and spendable credits. No Unreal includes on purpose: covered offline by
// Tests/upgrade_rules_test.cpp.

namespace IronUpgrades
{
enum class Upgrade { Armor, MachineGun, Rockets, Engine, Mines, Flamer, Railgun, Tesla, Repair };
inline constexpr int Count = 9;

struct UpgradeDef
{
	const char* Name;
	const char* Effect;
	int MaxLevel;  // 0 = consumable (can be bought any number of times, no level).
	int BaseCost;
	int CostStep;  // Extra cost per level already owned.
};

inline const UpgradeDef& Get(Upgrade U)
{
	static const UpgradeDef Defs[Count] = {
		/* Armor      */ { "ARMOR PLATING", "+25% max armor per level",     3, 400, 300 },
		/* MachineGun */ { "MACHINE GUN",   "+20% bullet damage per level",  3, 500, 350 },
		/* Rockets    */ { "ROCKET POD",    "+25% rocket damage per level",  3, 500, 400 },
		/* Engine     */ { "ENGINE TUNE",   "+15% engine power per level",   3, 400, 300 },
		/* Mines      */ { "MINE LAYER",    "Unlocks mines, +2 spare each",  3, 600, 350 },
		/* Flamer     */ { "FLAME TANK",    "Unlocks flamer, +25% burn each",3, 700, 400 },
		/* Railgun    */ { "RAILGUN",       "Unlocks railgun, +25% dmg each",3, 800, 450 },
		/* Tesla      */ { "TESLA COIL",    "Unlocks tesla, +25% shock each",3, 750, 400 },
		/* Repair     */ { "FIELD REPAIR",  "Full health and armor now",     0, 300, 0 },
	};
	const int Index = static_cast<int>(U);
	return Defs[Index >= 0 && Index < Count ? Index : 0];
}

struct Loadout
{
	int Levels[Count] = {};
	int Credits = 0;

	int Level(Upgrade U) const { return Levels[static_cast<int>(U)]; }

	// Price of the next purchase, or -1 if already at max level.
	int CostOf(Upgrade U) const
	{
		const UpgradeDef& D = Get(U);
		if (D.MaxLevel == 0) return D.BaseCost;
		const int L = Level(U);
		return L >= D.MaxLevel ? -1 : D.BaseCost + D.CostStep * L;
	}

	bool CanBuy(Upgrade U) const
	{
		const int Cost = CostOf(U);
		return Cost >= 0 && Credits >= Cost;
	}

	// Spends credits and raises the level (consumables only spend). False if unaffordable/maxed.
	bool Buy(Upgrade U)
	{
		if (!CanBuy(U)) return false;
		Credits -= CostOf(U);
		if (Get(U).MaxLevel > 0) ++Levels[static_cast<int>(U)];
		return true;
	}

	float ArmorMultiplier() const { return 1.f + 0.25f * Level(Upgrade::Armor); }
	float MachineGunDamageMultiplier() const { return 1.f + 0.2f * Level(Upgrade::MachineGun); }
	float RocketDamageMultiplier() const { return 1.f + 0.25f * Level(Upgrade::Rockets); }
	// Mines are bought, not fitted as standard: level 0 means the car has no mine rack at all.
	bool HasMines() const { return Level(Upgrade::Mines) > 0; }

	// Spare mines carried beyond the rack's own load, +2 per level owned.
	int MineReserveBonus() const { return Level(Upgrade::Mines) * 2; }

	// The flame tank is a shop unlock too, and each level burns hotter.
	bool HasFlamer() const { return Level(Upgrade::Flamer) > 0; }
	float FlamerDamageMultiplier() const { return 1.f + 0.25f * Level(Upgrade::Flamer); }

	bool HasRailgun() const { return Level(Upgrade::Railgun) > 0; }
	float RailgunDamageMultiplier() const { return 1.f + 0.25f * (Level(Upgrade::Railgun) > 1 ? Level(Upgrade::Railgun) - 1 : 0); }
	bool HasTesla() const { return Level(Upgrade::Tesla) > 0; }
	float TeslaDamageMultiplier() const { return 1.f + 0.25f * (Level(Upgrade::Tesla) > 1 ? Level(Upgrade::Tesla) - 1 : 0); }

	float EngineTorqueMultiplier() const { return 1.f + 0.15f * Level(Upgrade::Engine); }
};
}
