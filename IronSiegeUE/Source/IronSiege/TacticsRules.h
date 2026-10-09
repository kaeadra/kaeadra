#pragma once
// Engine-independent squad tactics for the enemy drivers, on top of the driving rules in AIRules.h:
// they cut the player off instead of tail-chasing (intercept), come at the player down different
// lanes instead of in a single file (pincer), weave while they are in the player's sights, hold
// fire rather than shoot a wingman in the back, and fall back to patch themselves up when badly
// hurt, then return to the fight. No Unreal includes on purpose: covered offline by
// Tests/tactics_rules_test.cpp. Distances are in cm, speeds in cm/s unless named Kph.

#include <cmath>

namespace IronTactics
{
struct Vec2
{
	float X = 0.f;
	float Y = 0.f;
};

inline Vec2 Add(const Vec2& A, const Vec2& B) { return { A.X + B.X, A.Y + B.Y }; }
inline Vec2 Sub(const Vec2& A, const Vec2& B) { return { A.X - B.X, A.Y - B.Y }; }
inline Vec2 Scale(const Vec2& A, float S) { return { A.X * S, A.Y * S }; }
inline float Dot(const Vec2& A, const Vec2& B) { return A.X * B.X + A.Y * B.Y; }
inline float Length(const Vec2& A) { return std::sqrt(Dot(A, A)); }
inline Vec2 Normal(const Vec2& A)
{
	const float L = Length(A);
	return L > 1e-4f ? Scale(A, 1.f / L) : Vec2{ 0.f, 0.f };
}
// The right-hand side of a heading, in Unreal's axes (X forward, Y right).
inline Vec2 RightOf(const Vec2& Forward) { return { -Forward.Y, Forward.X }; }

struct Tuning
{
	// Intercept: aim where the target will be, up to this far ahead in time.
	float MaxLeadSeconds = 1.8f;
	float LeadMinSpeed = 400.f;     // A target slower than this is treated as standing still.
	float MinClosingSpeed = 1500.f; // Assumed approach speed for a car that is still slow.

	// Pincer: each car is given a lane off the straight line to the target, widest far out and
	// closing to nothing near the target, so they arrive from different sides but end up facing it.
	float LaneOffset = 1400.f;
	float LaneFullDistance = 5000.f; // Full offset from here out...
	float LaneZeroDistance = 1600.f; // ...nothing from here in.

	// Weave while in the player's sights on the way in.
	float SightsDot = 0.94f;        // Within about 20 degrees of the player's aim.
	float WeaveMinDistance = 1000.f;
	float WeaveMaxDistance = 4500.f;
	float WeaveAmplitude = 0.55f;   // Steering added at the peak of a swerve.
	float WeavePeriod = 1.6f;       // Seconds for a full left-right swerve.
	float ParkedInSightsSeconds = 2.5f; // Parked in front of the player's gun this long: back off and come again.

	// Line of fire: a wingman this close to the line blocks the shot.
	float BlockRadius = 220.f;
	float BlockedLaneSeconds = 3.f; // Swing out to a side lane this long to clear the line.

	// Falling back to repair.
	float RetreatBelow = 0.3f;      // Health fraction that sends a car back.
	float SafeDistance = 5000.f;    // Far enough from the player to start patching up.
	float MaxRetreatSeconds = 7.f;  // Patches up wherever it is after this long.
	float PatchSeconds = 4.f;
	float PatchHealShare = 0.3f;    // Share of max health restored over a full patch.
	float PatchAbortDistance = 1500.f; // The player came for it: it fights back.
};

// Where to drive to meet a moving target: its position plus its velocity times the time it takes
// to get there (capped). A slow target is met where it is.
inline Vec2 InterceptPoint(const Vec2& Me, const Vec2& TargetPos, const Vec2& TargetVel, float MySpeed, const Tuning& T = Tuning())
{
	if (Length(TargetVel) < T.LeadMinSpeed) return TargetPos;
	const float Closing = MySpeed > T.MinClosingSpeed ? MySpeed : T.MinClosingSpeed;
	float Seconds = Length(Sub(TargetPos, Me)) / Closing;
	if (Seconds > T.MaxLeadSeconds) Seconds = T.MaxLeadSeconds;
	return Add(TargetPos, Scale(TargetVel, Seconds));
}

// Lane for squad member Index (0..Count-1): the first takes the centre, the next go left and right
// alternately, then the half lanes; -1 = full left, +1 = full right.
inline float LaneFor(int Index, int Count)
{
	if (Count <= 1 || Index <= 0) return 0.f;
	static const float Pattern[] = { 0.f, -1.f, 1.f, -0.5f, 0.5f };
	return Pattern[Index % 5];
}

// How much of the lane offset applies at this distance from the target (1 far out, 0 close in).
inline float LaneRamp(float Distance, const Tuning& T = Tuning())
{
	if (Distance <= T.LaneZeroDistance) return 0.f;
	if (Distance >= T.LaneFullDistance) return 1.f;
	return (Distance - T.LaneZeroDistance) / (T.LaneFullDistance - T.LaneZeroDistance);
}

// The point to steer for: Aim, pushed sideways off the line from Me by the lane.
inline Vec2 PincerGoal(const Vec2& Me, const Vec2& Aim, float Lane, const Tuning& T = Tuning())
{
	const Vec2 To = Sub(Aim, Me);
	const float Distance = Length(To);
	if (Distance < 1.f || Lane == 0.f) return Aim;
	return Add(Aim, Scale(RightOf(Normal(To)), Lane * T.LaneOffset * LaneRamp(Distance, T)));
}

// Is Me inside the shooter's aim cone?
inline bool InSights(const Vec2& ShooterPos, const Vec2& ShooterForward, const Vec2& Me, const Tuning& T = Tuning())
{
	const Vec2 To = Normal(Sub(Me, ShooterPos));
	return Dot(Normal(ShooterForward), To) >= T.SightsDot;
}

// Extra steering for a swerve at Time seconds; Phase staggers the cars so they do not swerve in step.
inline float WeaveSteer(float Time, float Phase, const Tuning& T = Tuning())
{
	const float TwoPi = 6.28318530718f;
	return T.WeaveAmplitude * std::sin(TwoPi * Time / T.WeavePeriod + Phase);
}

inline bool ShouldWeave(bool bInSights, float Distance, const Tuning& T = Tuning())
{
	return bInSights && Distance >= T.WeaveMinDistance && Distance <= T.WeaveMaxDistance;
}

// True if any of Others sits within BlockRadius of the line From -> To (not counting the ends:
// the shooter's own spot and cars right beside the target).
inline bool LineBlocked(const Vec2& From, const Vec2& To, const Vec2* Others, int Count, const Tuning& T = Tuning())
{
	const Vec2 Line = Sub(To, From);
	const float Length2 = Dot(Line, Line);
	if (Length2 < 1.f) return false;
	for (int i = 0; i < Count; ++i)
	{
		const Vec2 Rel = Sub(Others[i], From);
		const float Along = Dot(Rel, Line) / Length2;
		if (Along <= 0.05f || Along >= 0.95f) continue;
		const Vec2 Closest = Add(From, Scale(Line, Along));
		if (Length(Sub(Others[i], Closest)) < T.BlockRadius) return true;
	}
	return false;
}

// Falling back to repair: Engage -> (badly hurt) Retreat -> (far enough, or out of time) Patch ->
// (patched, or the player came for it) Engage. Once per car: a car that has patched up once fights
// to the end.
enum class Mode { Engage, Retreat, Patch };

struct Morale
{
	Mode State = Mode::Engage;
	bool bUsed = false;
	float Timer = 0.f;

	// Returns the share of max health to restore this frame (only while patching).
	float Update(float HealthFraction, float ThreatDistance, float DeltaSeconds, bool bAllowed, const Tuning& T = Tuning())
	{
		switch (State)
		{
		case Mode::Engage:
			if (bAllowed && !bUsed && HealthFraction > 0.f && HealthFraction < T.RetreatBelow)
			{
				State = Mode::Retreat;
				bUsed = true;
				Timer = 0.f;
			}
			return 0.f;
		case Mode::Retreat:
			Timer += DeltaSeconds;
			if (ThreatDistance >= T.SafeDistance || Timer >= T.MaxRetreatSeconds)
			{
				State = Mode::Patch;
				Timer = 0.f;
			}
			return 0.f;
		case Mode::Patch:
		{
			if (ThreatDistance < T.PatchAbortDistance)
			{
				State = Mode::Engage;
				return 0.f;
			}
			const float Step = Timer + DeltaSeconds > T.PatchSeconds ? T.PatchSeconds - Timer : DeltaSeconds;
			Timer += DeltaSeconds;
			if (Timer >= T.PatchSeconds) State = Mode::Engage;
			return Step > 0.f ? T.PatchHealShare * Step / T.PatchSeconds : 0.f;
		}
		}
		return 0.f;
	}
};
}
