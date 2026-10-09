#pragma once
// Engine-independent guided missile behaviour: which enemy the launcher locks onto, how the missile
// accelerates off the rail, and how fast it may turn toward its target. No Unreal includes on
// purpose: covered offline by Tests/missile_rules_test.cpp.

#include <cmath>

namespace IronMissiles
{
struct Vec
{
	float X = 0.f, Y = 0.f, Z = 0.f;
};

inline float Dot(const Vec& A, const Vec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
inline float Length(const Vec& A) { return std::sqrt(Dot(A, A)); }
inline Vec Scale(const Vec& A, float S) { return { A.X * S, A.Y * S, A.Z * S }; }
inline Vec Add(const Vec& A, const Vec& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
inline Vec Cross(const Vec& A, const Vec& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
inline Vec Normalize(const Vec& A)
{
	const float L = Length(A);
	return L > 1e-6f ? Scale(A, 1.f / L) : Vec{ 1.f, 0.f, 0.f };
}

struct Tuning
{
	float LockConeDeg = 9.f;         // Half-angle around the aim a target must sit in to lock.
	float LockRangeCm = 16000.f;     // 160 m.
	float TurnRateDeg = 110.f;       // Degrees per second the missile can swing its nose.
	float SeekDelay = 0.15f;         // Flies straight off the rail before it starts steering.
	float LaunchSpeed = 2500.f;      // cm/s leaving the pod...
	float CruiseSpeed = 6500.f;      // ...and after the motor has burnt up to speed.
	float BoostSeconds = 0.6f;
};

// Angle in degrees between the aim and the direction to a target (both need not be normalised).
inline float AngleDeg(const Vec& Aim, const Vec& ToTarget)
{
	const float C = Dot(Normalize(Aim), Normalize(ToTarget));
	const float Clamped = C < -1.f ? -1.f : (C > 1.f ? 1.f : C);
	return std::acos(Clamped) * 57.2957795f;
}

inline bool InLockCone(const Vec& Aim, const Vec& ToTarget, const Tuning& T = Tuning())
{
	const float Distance = Length(ToTarget);
	return Distance > 1.f && Distance <= T.LockRangeCm && AngleDeg(Aim, ToTarget) <= T.LockConeDeg;
}

// Of Count candidates (vectors from the launcher to each target), the one closest to the aim that
// is inside the lock cone, or -1 if none is.
inline int PickTarget(const Vec& Aim, const Vec* ToTargets, int Count, const Tuning& T = Tuning())
{
	int Best = -1;
	float BestAngle = 1e9f;
	for (int i = 0; i < Count; ++i)
	{
		if (!InLockCone(Aim, ToTargets[i], T)) continue;
		const float A = AngleDeg(Aim, ToTargets[i]);
		if (A < BestAngle)
		{
			BestAngle = A;
			Best = i;
		}
	}
	return Best;
}

// Turns Current toward Desired by at most MaxTurnDeg, keeping it a unit vector (a slerp capped at
// the turn rate). Straight behind is handled by turning about an arbitrary perpendicular axis.
inline Vec Steer(const Vec& Current, const Vec& Desired, float MaxTurnDeg)
{
	const Vec A = Normalize(Current);
	const Vec B = Normalize(Desired);
	const float Angle = AngleDeg(A, B) / 57.2957795f;
	const float MaxTurn = (MaxTurnDeg < 0.f ? 0.f : MaxTurnDeg) / 57.2957795f;
	if (Angle <= MaxTurn || Angle < 1e-5f) return Angle <= MaxTurn ? B : A;
	Vec Axis = Cross(A, B);
	if (Length(Axis) < 1e-5f)
	{
		// Opposite directions: any perpendicular will do.
		Axis = Cross(A, std::fabs(A.Z) < 0.9f ? Vec{ 0.f, 0.f, 1.f } : Vec{ 1.f, 0.f, 0.f });
	}
	Axis = Normalize(Axis);
	// Rodrigues rotation of A about Axis by MaxTurn.
	const float C = std::cos(MaxTurn), S = std::sin(MaxTurn);
	const Vec Rotated = Add(Add(Scale(A, C), Scale(Cross(Axis, A), S)), Scale(Axis, Dot(Axis, A) * (1.f - C)));
	return Normalize(Rotated);
}

// Motor burn: from launch speed up to cruise over BoostSeconds, then steady.
inline float Speed(float AgeSeconds, const Tuning& T = Tuning())
{
	if (AgeSeconds <= 0.f) return T.LaunchSpeed;
	if (AgeSeconds >= T.BoostSeconds) return T.CruiseSpeed;
	return T.LaunchSpeed + (T.CruiseSpeed - T.LaunchSpeed) * (AgeSeconds / T.BoostSeconds);
}

inline bool IsSeeking(float AgeSeconds, const Tuning& T = Tuning())
{
	return AgeSeconds >= T.SeekDelay;
}
}
