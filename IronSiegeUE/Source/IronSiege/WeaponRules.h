#pragma once
// Engine-independent weapon balance and ammo/reload state machine. No Unreal includes here
// on purpose: this header must compile and run in Tests/weapon_rules_test.cpp without
// linking Unreal, mirroring how CoastCity keeps its combat math testable offline.

namespace IronWeapons
{
inline constexpr int Count = 6;

enum class WeaponType { MachineGun, RocketLauncher, MineLayer, Flamethrower, Railgun, Tesla };

struct Spec
{
	int Capacity;
	int InitialReserve;
	float Damage;
	float FireInterval;    // Seconds between shots.
	float ReloadTime;      // Seconds to refill Capacity from Reserve.
	float Range;           // Unreal units (cm).
	float ExplosionRadius; // 0 for non-explosive weapons.
	bool Automatic;
	// Heat: holding the trigger heats the barrel, which widens the cone the rounds go into and,
	// at 1.0, locks the weapon until it has cooled back to CoolToFire. Tapping stays accurate.
	float HeatPerShot;
	float CoolPerSecond;
	float SpreadMinDeg;    // Cone half-angle when cold.
	float SpreadMaxDeg;    // ...and when glowing.
};

inline constexpr float CoolToFire = 0.35f; // Heat an overheated weapon must fall back to.

inline const Spec& Get(WeaponType InType)
{
	static const Spec Specs[Count] = {
		/* MachineGun     */ { 200, 600,   6.f, .08f, 2.5f, 12000.f,   0.f, true,  .055f, .38f, 0.4f, 4.5f },
		/* RocketLauncher */ {   4,  16, 140.f, 1.1f, 3.2f, 20000.f, 600.f, false, 0.f,   1.f,  0.6f, 0.6f },
		// Mines are dropped at the car's tail, so they have no range and no aim cone; the damage is
		// the blast (see MineRules.h for arming, trigger radius and lifetime).
		/* MineLayer      */ {   3,   6, 150.f, 1.2f, 4.f,      0.f, 550.f, false, 0.f,   1.f,  0.f,  0.f },
		// Flamethrower: a short cone of fire, cheap per tick but relentless. It burns fuel fast and
		// heats the nozzle even faster, so it works in bursts - the same heat lockout as the gun,
		// just tuned to bite sooner (about two seconds of held trigger).
		/* Flamethrower   */ { 120, 240,   7.f, .05f, 3.f,   1500.f,   0.f, true,  .026f, .30f, 9.f,  14.f },
		// Railgun: a charged slug that goes through every car on its line (BeamRules.h) - few
		// rounds, a long gap between shots, heavy hits. The gap is the cooldown after the charge.
		/* Railgun        */ {   4,   8, 120.f, 1.6f, 3.5f, 25000.f,   0.f, false, 0.f,   1.f,  0.f,  0.f },
		// Tesla coil: lightning that jumps between up to four cars and cuts their engines; damage
		// is per first target, each jump carries on weaker.
		/* Tesla          */ {   6,  12,  45.f, 1.0f, 3.f,   2200.f,   0.f, false, 0.f,   1.f,  0.f,  0.f },
	};
	const int Index = static_cast<int>(InType);
	return Specs[Index >= 0 && Index < Count ? Index : 0];
}

// Per-weapon runtime ammo/cooldown/reload state, deliberately independent of any Actor/Component.
struct Loadout
{
	WeaponType Type = WeaponType::MachineGun;
	int Ammo = 0;
	int Reserve = 0;
	float Cooldown = 0.f;
	float ReloadRemaining = 0.f;
	bool Reloading = false;
	float Heat = 0.f;        // 0..1
	bool Overheated = false; // Locked out until Heat falls to CoolToFire.
	// Vehicle class traits (VehicleClassRules.h): faster/slower fire, cooler/hotter barrel, quicker
	// reload. Kept across Init so a class trait survives the weapon being reset.
	float RateScale = 1.f;
	float HeatScale = 1.f;
	float ReloadScale = 1.f;

	void Init(WeaponType InType)
	{
		Type = InType;
		const Spec& S = Get(Type);
		Ammo = S.Capacity;
		Reserve = S.InitialReserve;
		Cooldown = 0.f;
		ReloadRemaining = 0.f;
		Reloading = false;
		Heat = 0.f;
		Overheated = false;
	}

	void Tick(float DeltaSeconds)
	{
		const Spec& HeatSpec = Get(Type);
		Heat -= HeatSpec.CoolPerSecond * DeltaSeconds;
		if (Heat < 0.f) Heat = 0.f;
		if (Overheated && Heat <= CoolToFire) Overheated = false;
		if (Cooldown > 0.f)
		{
			Cooldown -= DeltaSeconds;
			if (Cooldown < 0.f) Cooldown = 0.f;
		}
		if (Reloading)
		{
			ReloadRemaining -= DeltaSeconds;
			if (ReloadRemaining <= 0.f)
			{
				const Spec& S = Get(Type);
				const int Needed = S.Capacity - Ammo;
				const int Transfer = Needed < Reserve ? Needed : Reserve;
				Ammo += Transfer;
				Reserve -= Transfer;
				Reloading = false;
				ReloadRemaining = 0.f;
			}
		}
	}

	bool Fire()
	{
		// Epsilon guards against sub-frame float residue: real gameplay ticks with a variable
		// DeltaTime that will almost never land exactly on zero at the end of a cooldown.
		if (Reloading || Overheated || Cooldown > 1e-4f || Ammo <= 0) return false;
		--Ammo;
		const Spec& S = Get(Type);
		Cooldown = S.FireInterval / (RateScale > 0.05f ? RateScale : 0.05f);
		Heat += S.HeatPerShot * HeatScale;
		if (Heat >= 1.f)
		{
			Heat = 1.f;
			Overheated = true;
		}
		return true;
	}

	// Ammo crate: adds to the reserve, capped at twice the weapon's starting reserve so crates
	// cannot be hoarded indefinitely. Returns the amount actually added.
	int AddReserve(int Amount)
	{
		const int Cap = Get(Type).InitialReserve * 2;
		int Added = Amount;
		if (Reserve + Added > Cap) Added = Cap - Reserve;
		if (Added < 0) Added = 0;
		Reserve += Added;
		return Added;
	}

	// Cone half-angle for the next shot, in degrees: tight when cold, wide when the barrel is hot.
	float SpreadDeg() const
	{
		const Spec& S = Get(Type);
		const float H = Heat < 0.f ? 0.f : (Heat > 1.f ? 1.f : Heat);
		return S.SpreadMinDeg + (S.SpreadMaxDeg - S.SpreadMinDeg) * H;
	}

	bool StartReload()
	{
		if (Reloading || Ammo >= Get(Type).Capacity || Reserve <= 0) return false;
		Reloading = true;
		ReloadRemaining = Get(Type).ReloadTime * ReloadScale;
		return true;
	}
};
}
