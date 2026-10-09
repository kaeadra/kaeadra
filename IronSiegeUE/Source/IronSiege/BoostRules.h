#pragma once
// Engine-independent nitro boost: a charge meter that drains while boosting, refills on its own
// after a pause and tops up on kills, plus what the charge does to engine torque and camera FOV.
// No Unreal includes on purpose: covered offline by Tests/boost_rules_test.cpp.

namespace IronBoost
{
struct Tuning
{
	float DrainPerSecond = 0.45f;     // Full meter lasts ~2.2s of continuous boost.
	float RechargePerSecond = 0.14f;  // ~7s from empty back to full once it starts refilling.
	float RechargeDelay = 1.2f;       // Quiet seconds after releasing boost before it refills.
	float MinChargeToStart = 0.15f;   // Stops the meter being tapped dry a frame at a time.
	float KillReward = 0.35f;         // Aggression pays for itself.
	float TorqueMultiplier = 1.7f;    // Engine torque while boosting.
	float FovBonusDeg = 14.f;         // Camera pulls back for a sense of speed.
	float FovBlendPerSecond = 3.f;    // How fast the FOV eases in and out.
	float TopSpeedBonus = 1.12f;      // The class top speed limit is lifted this much while boosting.
};

struct State
{
	float Charge = 1.f;       // 0..1
	float Cooldown = 0.f;     // Seconds left before recharging resumes.
	float FovAlpha = 0.f;     // 0..1 eased version of Active, for the camera.
	bool Active = false;
};

inline float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

// One frame of the meter. bWantBoost is the player holding the boost control; boosting also
// needs charge in the tank (and more than MinChargeToStart to get going from released).
inline void Tick(State& S, bool bWantBoost, float DeltaSeconds, const Tuning& T = Tuning())
{
	const bool bCanStart = S.Active ? S.Charge > 0.f : S.Charge >= T.MinChargeToStart;
	S.Active = bWantBoost && bCanStart;
	if (S.Active)
	{
		S.Charge = Clamp01(S.Charge - T.DrainPerSecond * DeltaSeconds);
		S.Cooldown = T.RechargeDelay;
	}
	else if (S.Cooldown > 0.f)
	{
		S.Cooldown -= DeltaSeconds;
		if (S.Cooldown < 0.f) S.Cooldown = 0.f;
	}
	else
	{
		S.Charge = Clamp01(S.Charge + T.RechargePerSecond * DeltaSeconds);
	}
	// Ease the camera so the FOV kick does not snap on and off with the key.
	const float Target = S.Active ? 1.f : 0.f;
	const float Step = T.FovBlendPerSecond * DeltaSeconds;
	S.FovAlpha = Target > S.FovAlpha ? (S.FovAlpha + Step > Target ? Target : S.FovAlpha + Step)
									 : (S.FovAlpha - Step < Target ? Target : S.FovAlpha - Step);
}

// Kill reward (or a pickup): tops the meter up without letting it overflow.
inline void AddCharge(State& S, float Amount)
{
	S.Charge = Clamp01(S.Charge + (Amount < 0.f ? 0.f : Amount));
}

inline float TorqueMultiplier(const State& S, const Tuning& T = Tuning())
{
	return S.Active ? T.TorqueMultiplier : 1.f;
}

// Multiplier on the class top speed limit (HandlingRules::GovernThrottle): nitro can pass it.
inline float TopSpeedMultiplier(const State& S, const Tuning& T = Tuning())
{
	return S.Active ? T.TopSpeedBonus : 1.f;
}

inline float FovBonus(const State& S, const Tuning& T = Tuning())
{
	return T.FovBonusDeg * Clamp01(S.FovAlpha);
}
}
