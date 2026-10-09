#pragma once
// Engine-independent countermeasures: the flare charges a car carries, how they recharge, which
// incoming guided missiles a flare burst pulls off, and how urgent the missile warning is. No
// Unreal includes on purpose: covered offline by Tests/flare_rules_test.cpp.

namespace IronFlares
{
struct Tuning
{
	int MaxCharges = 3;
	float RechargeSeconds = 9.f;   // One charge back every this long while below the maximum.
	float CooldownSeconds = 0.8f;  // Between two bursts, so one key-mash does not empty the rack.
	float DecoyRangeCm = 7000.f;   // Missiles closer than this to the car bite on the flares...
	float WarnRangeCm = 9000.f;    // ...and the warning tone starts this far out.
	float FlareSeconds = 3.f;      // How long a flare burns (and keeps a seduced missile busy).
};

struct State
{
	int Charges = 3;
	float RechargeTimer = 0.f;
	float Cooldown = 0.f;

	void Tick(float DeltaSeconds, const Tuning& T = Tuning())
	{
		Cooldown = Cooldown > DeltaSeconds ? Cooldown - DeltaSeconds : 0.f;
		if (Charges >= T.MaxCharges)
		{
			RechargeTimer = 0.f;
			return;
		}
		RechargeTimer += DeltaSeconds;
		while (RechargeTimer >= T.RechargeSeconds && Charges < T.MaxCharges)
		{
			RechargeTimer -= T.RechargeSeconds;
			++Charges;
		}
	}

	// Spends one charge if one is ready; false (nothing happens) if empty or cooling down.
	bool TryFire(const Tuning& T = Tuning())
	{
		if (Charges <= 0 || Cooldown > 0.f) return false;
		--Charges;
		Cooldown = T.CooldownSeconds;
		return true;
	}

	// 0..1 progress toward the next charge (1 when full), for the HUD.
	float RechargeFraction(const Tuning& T = Tuning()) const
	{
		if (Charges >= T.MaxCharges || T.RechargeSeconds <= 0.f) return 1.f;
		const float F = RechargeTimer / T.RechargeSeconds;
		return F < 0.f ? 0.f : (F > 1.f ? 1.f : F);
	}
};

// A burst decoys a missile homing on this car if it is inside the decoy range. Flares fired too
// early (missile still far off) do nothing - timing matters.
inline bool IsDecoyed(float MissileDistanceCm, const Tuning& T = Tuning())
{
	return MissileDistanceCm >= 0.f && MissileDistanceCm <= T.DecoyRangeCm;
}

// Seconds between warning beeps for the nearest incoming missile: slow at the edge of the warning
// range, rapid when it is about to hit. Negative when there is nothing to warn about.
inline float WarningBeepInterval(float MissileDistanceCm, const Tuning& T = Tuning())
{
	if (MissileDistanceCm < 0.f || MissileDistanceCm > T.WarnRangeCm) return -1.f;
	const float Closeness = 1.f - MissileDistanceCm / T.WarnRangeCm; // 0 far .. 1 on top of us.
	return 0.5f - 0.4f * Closeness;                                   // 0.5 s .. 0.1 s.
}
}
