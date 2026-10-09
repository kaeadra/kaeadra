#pragma once
// Engine-independent drift detection: when do the tyres let go, how hard, and how often should
// smoke puffs and skid marks be laid down. No Unreal includes on purpose: covered offline by
// Tests/skid_rules_test.cpp.

namespace IronSkid
{
struct Tuning
{
	float MinSpeedKph = 25.f;      // Crawling out of a parking spot is not a drift.
	float SlipStartKph = 10.f;     // Sideways speed at which the tyres start to complain.
	float SlipFullKph = 45.f;      // ...and at which they are fully lit up.
	float HandbrakeSlipKph = 14.f; // A yanked handbrake counts as this much extra slip.
	float MarkSpacingCm = 90.f;    // Distance between skid decals at full strength.
	float PuffInterval = 0.05f;    // Seconds between smoke puffs at full strength.
};

inline float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

// How hard the car is sliding, 0..1. LateralKph is the sideways component of its velocity.
inline float Strength(float SpeedKph, float LateralKph, bool bHandbrake, const Tuning& T = Tuning())
{
	if (SpeedKph < T.MinSpeedKph) return 0.f;
	const float Slip = (LateralKph < 0.f ? -LateralKph : LateralKph) + (bHandbrake ? T.HandbrakeSlipKph : 0.f);
	if (Slip <= T.SlipStartKph) return 0.f;
	return Clamp01((Slip - T.SlipStartKph) / (T.SlipFullKph - T.SlipStartKph));
}

// Skid marks are laid by distance, not by time, so they do not bunch up at low speed. Weak slides
// leave a sparser trail.
inline bool ShouldDropMark(float DistanceSinceLastCm, float SkidStrength, const Tuning& T = Tuning())
{
	if (SkidStrength <= 0.f) return false;
	const float Spacing = T.MarkSpacingCm / (0.35f + 0.65f * SkidStrength);
	return DistanceSinceLastCm >= Spacing;
}

// Seconds until the next smoke puff: dense while the tyres are screaming, sparse in a light slide.
inline float PuffDelay(float SkidStrength, const Tuning& T = Tuning())
{
	if (SkidStrength <= 0.f) return 1e9f;
	return T.PuffInterval / (0.3f + 0.7f * SkidStrength);
}

// Tyre screech volume (and, scaled, its pitch) for the looping skid sound.
inline float ScreechVolume(float SkidStrength)
{
	return Clamp01(SkidStrength) * 0.9f;
}
}
