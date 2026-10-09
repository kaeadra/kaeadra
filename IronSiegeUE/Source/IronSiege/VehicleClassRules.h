#pragma once
// Engine-independent vehicle class stats. No Unreal includes here on purpose:
// this header must compile and run in Tests/vehicle_class_rules_test.cpp and
// Tests/class_balance_test.cpp without linking Unreal, so class balance can be checked before
// opening the editor.
//
// Every number here is live: the top speed is enforced (HandlingRules::GovernThrottle), the mass is
// what the Chaos body weighs (setup_garage_vehicles.py writes it), the chassis feel is applied to the
// car, and the weapon trait scales the stock weapons (WeaponRules.h).

namespace IronVehicles
{
inline constexpr int Count = 6;

enum class VehicleClass { Scout, Assault, Heavy, Artillery, Interceptor, Dune };

// What the class does with the shared weapons: multipliers on the stock specs.
struct WeaponTrait
{
	float GunDamage;     // Machine-gun damage per round.
	float GunRate;       // Rounds per second (the fire interval is divided by this).
	float GunHeat;       // Barrel heat per round (lower = longer bursts before it locks up).
	float RocketDamage;
	float RocketReload;  // Reload time multiplier (lower = faster).
	float BoostRecharge; // Nitro refill speed.
};

// How the chassis sits on the road.
struct ChassisFeel
{
	float Grip;      // Tyre friction multiplier, on top of the driving style's.
	float ComDropCm; // Centre of mass lowered by this much: tall bodies roll over without it.
	float Downforce; // Chaos downforce coefficient: keeps fast cars planted at speed.
	float AirControl;// Multiplier on the driving style's air control (levelling the car mid-jump).
	float EnginePitch; // Engine note: a big diesel truck sits low, a sports car screams.
	float BrakeScale;  // Brake torque: the heavy bodies need more to stop in a sane distance.
	float Turn;        // Multiplier on the driving style's cornering (the arcade turn-in).
	float ReverseKph;  // Reverse speed limit.
};

struct ClassStats
{
	float TopSpeedKph;
	float Mass;                 // kg.
	float MaxHealth;
	float MaxArmor;
	float ArmorDamageReduction; // Fraction of incoming raw damage an intact armor pool absorbs first.
	WeaponTrait Weapons;
	ChassisFeel Chassis;
	const char* Name;  // Display name (vehicle-select screen).
	const char* Blurb; // One-line role description.
	const char* Trait; // One-line weapon trait (vehicle-select screen).
};

inline const ClassStats& Get(VehicleClass InClass)
{
	static const ClassStats Specs[Count] = {
		/* Scout     */ { 190.f, 1500.f, 125.f,  45.f, .25f, { .85f, 1.15f, 1.f,  .85f, 1.f,  1.4f }, { 1.08f, 10.f, .35f, 1.f,  1.15f, 1.f,  1.1f,  45.f },
			"SCOUT", "Fast and light - hit and run.", "Fast-firing gun, quick nitro refill" },
		/* Assault   */ { 150.f, 2400.f, 160.f, 100.f, .40f, { 1.f,  1.f,   1.f,  1.f,  1.f,  1.f },  { 1.f,   30.f, .2f,  1.f,  .92f,  1.1f, 1.f,   40.f },
			"ASSAULT", "Balanced all-rounder.", "Stock weapons, no weak spot" },
		/* Heavy     */ { 105.f, 4800.f, 260.f, 220.f, .55f, { 1.2f, .85f,  1.f,  1.f,  1.15f, .8f }, { 1.05f, 50.f, .15f, .8f,  .72f,  2.f,  .8f,   30.f },
			"HEAVY", "Slow, armored, hits hard.", "Heavy rounds, rams hard" },
		/* Artillery */ {  95.f, 3800.f, 170.f,  90.f, .35f, { .85f, 1.f,   1.f,  1.5f, .65f, .9f },  { 1.f,   50.f, .15f, .8f,  .8f,   1.2f, .75f,  32.f },
			"ARTILLERY", "Support - rocket-focused.", "Rockets +50% damage, faster reload" },
		/* Interceptor */ { 175.f, 1850.f, 135.f, 60.f, .30f, { 1.05f, 1.f,  .85f, 1.f,  1.f,  1.1f }, { 1.12f, 12.f, .4f,  1.f,  1.06f, 1.f,  1.15f, 42.f },
			"INTERCEPTOR", "Sports sedan - quick, and tougher than it looks.", "Cool-running gun: longer bursts" },
		/* Dune      */ { 165.f, 1500.f, 130.f,  70.f, .35f, { 1.f,  1.05f, 1.f,  .9f,  1.f,  1.3f }, { .92f,  15.f, .1f,  1.6f, 1.f,   1.f,  1.05f, 45.f },
			"DUNE", "Off-road buggy - jumps, drifts, dodges.", "Strong nitro, lands its jumps" },
	};
	const int Index = static_cast<int>(InClass);
	return Specs[Index >= 0 && Index < Count ? Index : 0];
}

// Cycling through classes on the select screen (wraps both ways).
inline int NextIndex(int Index) { return ((Index + 1) % Count + Count) % Count; }
inline int PrevIndex(int Index) { return ((Index - 1) % Count + Count) % Count; }
}
