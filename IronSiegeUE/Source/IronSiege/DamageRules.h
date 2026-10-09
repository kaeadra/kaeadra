#pragma once
// Engine-independent health/armor damage math. No Unreal includes here on purpose: see
// Tests/damage_rules_test.cpp for the offline check that runs without opening the editor.

namespace IronDamage
{
// Named HealthPool, not Health: a member also called Health inside a struct named Health
// conflicts with constructor-name lookup under Unreal's C++20 build settings (MSVC error
// C2461/C7624), even though it compiles fine as a bare standalone /std:c++17 translation unit.
struct HealthPool
{
	float Health = 0.f;
	float MaxHealth = 0.f;
	float Armor = 0.f;
	float MaxArmor = 0.f;
};

// Applies RawDamage to a Health/Armor pool. ArmorDamageReduction (0..1) is the fraction of the
// hit that the intact armor pool absorbs before the remainder spills onto Health. Returns the
// portion of RawDamage that actually reached Health (0 if fully absorbed by armor).
inline float ApplyDamage(HealthPool& InOut, float RawDamage, float ArmorDamageReduction)
{
	if (RawDamage <= 0.f) return 0.f;
	float Remaining = RawDamage;
	if (InOut.Armor > 0.f && ArmorDamageReduction > 0.f)
	{
		const float Requested = RawDamage * ArmorDamageReduction;
		const float Absorbed = Requested < InOut.Armor ? Requested : InOut.Armor;
		InOut.Armor -= Absorbed;
		Remaining -= Absorbed;
	}
	if (Remaining < 0.f) Remaining = 0.f;
	InOut.Health -= Remaining;
	if (InOut.Health < 0.f) InOut.Health = 0.f;
	return Remaining;
}

// Ramming. A car doing 90 kph into a stationary one hurts; a nudge in traffic should not. Damage
// starts at MinSpeedKph and grows with the square of the speed above it (kinetic energy), scaled by
// how much heavier the rammer is - a light buggy bouncing off a Juggernaut barely scratches it.
struct RamTuning
{
	float MinSpeedKph = 25.f;   // Below this a bump does nothing.
	float DamagePerKph2 = 0.03f;
	float MaxDamage = 120.f;
	float SelfDamageFraction = 0.3f; // What the rammer takes for its own trouble.
	float Cooldown = 0.6f;      // Per target, so one long scrape is not a machine gun.
};

// ClosingSpeedKph is how fast the two cars are approaching along the impact normal; MassRatio is
// rammer mass / target mass (1 = same weight).
inline float RamDamage(float ClosingSpeedKph, float MassRatio, const RamTuning& T = RamTuning())
{
	const float Over = ClosingSpeedKph - T.MinSpeedKph;
	if (Over <= 0.f) return 0.f;
	const float Ratio = MassRatio < 0.25f ? 0.25f : (MassRatio > 3.f ? 3.f : MassRatio);
	float Damage = Over * Over * T.DamagePerKph2 * Ratio;
	if (Damage > T.MaxDamage) Damage = T.MaxDamage;
	return Damage;
}

// What the car doing the ramming takes from the same impact.
inline float RamSelfDamage(float DamageDealt, const RamTuning& T = RamTuning())
{
	return DamageDealt * T.SelfDamageFraction;
}

// Repair kit: restores up to HealthAmount health and ArmorAmount armor, capped at the pool's
// maxima. Does nothing to a destroyed vehicle. Returns true if anything was restored.
inline bool Repair(HealthPool& InOut, float HealthAmount, float ArmorAmount)
{
	if (InOut.Health <= 0.f) return false;
	const float OldHealth = InOut.Health, OldArmor = InOut.Armor;
	InOut.Health = InOut.Health + HealthAmount > InOut.MaxHealth ? InOut.MaxHealth : InOut.Health + HealthAmount;
	InOut.Armor = InOut.Armor + ArmorAmount > InOut.MaxArmor ? InOut.MaxArmor : InOut.Armor + ArmorAmount;
	return InOut.Health > OldHealth || InOut.Armor > OldArmor;
}

inline bool IsDestroyed(const HealthPool& InHealth)
{
	return InHealth.Health <= 0.f;
}

// Linear falloff from BaseDamage at Distance==0 to 0 at Distance>=Radius.
inline float RadialFalloff(float Distance, float Radius, float BaseDamage)
{
	if (Radius <= 0.f || Distance >= Radius) return 0.f;
	if (Distance < 0.f) Distance = 0.f;
	return BaseDamage * (1.f - Distance / Radius);
}

// Burning (the flamethrower sets cars alight): each hit relights the fire for BurnSeconds, and a
// burning car takes Dps until it goes out. Re-igniting refreshes the timer and keeps the hotter
// of the two fires; it never stacks, so holding the flame on a car is not doubly rewarded.
struct BurnTuning
{
	float BurnSeconds = 3.f;
	float Dps = 14.f;
};

struct BurnState
{
	float SecondsLeft = 0.f;
	float Dps = 0.f;

	void Ignite(float InDps, const BurnTuning& T = BurnTuning())
	{
		SecondsLeft = T.BurnSeconds;
		Dps = InDps > Dps ? InDps : Dps;
	}

	bool IsBurning() const { return SecondsLeft > 0.f; }

	// Advances the fire; returns the damage dealt over this step (only while it still burns).
	float Tick(float DeltaSeconds)
	{
		if (SecondsLeft <= 0.f || DeltaSeconds <= 0.f) return 0.f;
		const float Step = DeltaSeconds < SecondsLeft ? DeltaSeconds : SecondsLeft;
		SecondsLeft -= Step;
		const float Dealt = Dps * Step;
		if (SecondsLeft <= 0.f)
		{
			SecondsLeft = 0.f;
			Dps = 0.f;
		}
		return Dealt;
	}
};
}
