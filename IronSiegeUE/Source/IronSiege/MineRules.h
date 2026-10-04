#pragma once
// Engine-independent proximity-mine behaviour: the arming delay that stops you blowing yourself up
// as you drop one, the trigger radius, and how long a mine sits there before it goes inert.
// No Unreal includes on purpose: covered offline by Tests/mine_rules_test.cpp.

namespace IronMines
{
struct Tuning
{
	float ArmSeconds = 1.2f;       // Dead until this long after being dropped.
	float TriggerRadiusCm = 280.f; // An enemy this close sets it off.
	float LifetimeSeconds = 30.f;  // Then it fizzles out, so the map does not fill with mines.
	float BlinkIdle = 0.9f;        // Seconds per blink before arming...
	float BlinkArmed = 0.3f;       // ...and after.
};

inline bool IsArmed(float AgeSeconds, const Tuning& T = Tuning())
{
	return AgeSeconds >= T.ArmSeconds;
}

inline bool HasExpired(float AgeSeconds, const Tuning& T = Tuning())
{
	return AgeSeconds >= T.LifetimeSeconds;
}

// NearestEnemyCm < 0 means "nothing in range".
inline bool ShouldDetonate(float AgeSeconds, float NearestEnemyCm, const Tuning& T = Tuning())
{
	if (!IsArmed(AgeSeconds, T) || HasExpired(AgeSeconds, T)) return false;
	return NearestEnemyCm >= 0.f && NearestEnemyCm <= T.TriggerRadiusCm;
}

// Seconds per blink of the mine's light: slow while it is still arming, urgent once it is live.
inline float BlinkPeriod(float AgeSeconds, const Tuning& T = Tuning())
{
	return IsArmed(AgeSeconds, T) ? T.BlinkArmed : T.BlinkIdle;
}
}
