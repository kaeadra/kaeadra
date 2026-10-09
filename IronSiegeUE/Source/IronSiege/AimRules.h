#pragma once
// Engine-independent aiming: aim assist for the roof guns and the turret that shows it.
//
// The guns fire along the camera's heading and the player cannot aim up or down, so without help a
// car on a ramp or a slope is out of reach, and a car a few degrees off the crosshair is a miss. The
// assist picks the enemy nearest the crosshair inside a narrow cone and aims the shot at it: fully in
// height (the player has no other way to aim there), and sideways only within the cone, fading out
// toward its edge so the aim never snaps. The turret on the roof swivels to where the gun is aiming.
// No Unreal includes on purpose: covered offline by Tests/aim_rules_test.cpp.

#include <cmath>

namespace IronAim
{
struct Vec
{
	float X, Y, Z;
};

inline Vec Sub(const Vec& A, const Vec& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
inline Vec Add(const Vec& A, const Vec& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
inline Vec Scale(const Vec& A, float S) { return { A.X * S, A.Y * S, A.Z * S }; }
inline float Dot(const Vec& A, const Vec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
inline float Length(const Vec& A) { return std::sqrt(Dot(A, A)); }
inline Vec Normal(const Vec& A)
{
	const float L = Length(A);
	return L > 1e-6f ? Scale(A, 1.f / L) : Vec{ 1.f, 0.f, 0.f };
}

constexpr float Pi = 3.14159265f;
inline float Deg(float Rad) { return Rad * 180.f / Pi; }
inline float Rad(float Degrees) { return Degrees * Pi / 180.f; }

// Assist strength by setting: 0 off, 1 normal, 2 strong.
inline constexpr int LevelCount = 3;

struct Tuning
{
	float ConeYawDeg = 6.f;     // Sideways: how far off the crosshair a car can be and still be helped.
	float ConePitchDeg = 25.f;  // Up/down: generous, the gun has no other way to aim there.
	float MaxRange = 9000.f;    // cm.
	float StickyScale = 1.6f;   // The current target keeps its lock over a wider cone (no flicking).
	bool bEnabled = true;
};

inline Tuning ForLevel(int Level)
{
	Tuning T;
	T.bEnabled = Level > 0;
	T.ConeYawDeg = Level >= 2 ? 10.f : 6.f;
	return T;
}

// Sideways (yaw) and up/down (pitch) angle of Dir relative to a level aim Forward, in degrees.
inline void AnglesFrom(const Vec& Forward, const Vec& Dir, float& OutYawDeg, float& OutPitchDeg)
{
	const Vec F = Normal({ Forward.X, Forward.Y, 0.f });
	const Vec Right{ -F.Y, F.X, 0.f };
	const float Along = Dir.X * F.X + Dir.Y * F.Y;
	const float Side = Dot(Dir, Right);
	OutYawDeg = Deg(std::atan2(Side, Along));
	OutPitchDeg = Deg(std::atan2(Dir.Z, std::sqrt(Along * Along + Side * Side)));
}

// The target to help aim at: in front, in range, inside the cone (wider for the one already
// tracked), nearest the crosshair sideways. Aim points are the targets' body centres. Returns -1
// when none qualifies.
inline int Pick(const Vec& Origin, const Vec& Forward, const Vec* AimPoints, int Count, int Previous, const Tuning& T)
{
	if (!T.bEnabled) return -1;
	int Best = -1;
	float BestScore = 1e9f;
	for (int i = 0; i < Count; ++i)
	{
		const Vec To = Sub(AimPoints[i], Origin);
		const float Dist = Length(To);
		if (Dist < 1.f || Dist > T.MaxRange) continue;
		float Yaw = 0.f, Pitch = 0.f;
		AnglesFrom(Forward, To, Yaw, Pitch);
		const float Wide = i == Previous ? T.StickyScale : 1.f;
		const float AbsYaw = std::fabs(Yaw);
		if (AbsYaw > T.ConeYawDeg * Wide || std::fabs(Pitch) > T.ConePitchDeg * Wide) continue;
		// Nearest the crosshair wins; distance only breaks near-ties (a car right behind another).
		const float Score = AbsYaw / Wide + Dist * 0.0002f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = i;
		}
	}
	return Best;
}

// The direction to fire: toward the target, but sideways only as far as the cone allows, easing
// off toward its edge (a car at the very edge of the cone gets little help, one near the centre
// is hit dead on). Height is always corrected.
inline Vec AssistedDirection(const Vec& Forward, const Vec& Origin, const Vec& AimPoint, const Tuning& T)
{
	const Vec Level = Normal({ Forward.X, Forward.Y, 0.f });
	const Vec To = Sub(AimPoint, Origin);
	float Yaw = 0.f, Pitch = 0.f;
	AnglesFrom(Level, To, Yaw, Pitch);
	const float Edge = T.ConeYawDeg * T.StickyScale;
	float Help = Edge > 0.f ? 1.f - std::fabs(Yaw) / Edge : 0.f;
	Help = Help < 0.f ? 0.f : (Help > 1.f ? 1.f : Help);
	// Full correction in the inner two-thirds of the cone, easing to none at its edge.
	Help = Help > 1.f / 3.f ? 1.f : Help * 3.f;
	const float UseYaw = Rad(Yaw * Help);
	const float UsePitch = Rad(Pitch);
	const Vec Right{ -Level.Y, Level.X, 0.f };
	const float C = std::cos(UsePitch);
	return Normal(Add(Add(Scale(Level, std::cos(UseYaw) * C), Scale(Right, std::sin(UseYaw) * C)), Vec{ 0.f, 0.f, std::sin(UsePitch) }));
}

// Lead for a projectile of speed Speed (cm/s) against a target moving at Velocity: the point where
// they meet, solving |P + V t| = Speed t. False (and the target's current position) if it cannot catch it.
inline bool Lead(const Vec& Origin, const Vec& Target, const Vec& Velocity, float Speed, Vec& Out)
{
	Out = Target;
	const Vec P = Sub(Target, Origin);
	const float A = Dot(Velocity, Velocity) - Speed * Speed;
	const float B = 2.f * Dot(P, Velocity);
	const float C = Dot(P, P);
	float Time = -1.f;
	if (std::fabs(A) < 1e-3f)
	{
		if (std::fabs(B) > 1e-6f) Time = -C / B;
	}
	else
	{
		const float Disc = B * B - 4.f * A * C;
		if (Disc >= 0.f)
		{
			const float R = std::sqrt(Disc);
			const float T1 = (-B - R) / (2.f * A), T2 = (-B + R) / (2.f * A);
			const float Lo = T1 < T2 ? T1 : T2, Hi = T1 < T2 ? T2 : T1;
			Time = Lo > 0.f ? Lo : Hi;
		}
	}
	if (Time <= 0.f) return false;
	Out = Add(Target, Scale(Velocity, Time));
	return true;
}

// ---- The turret: turns toward the aim within its limits, at a capped rate.
struct TurretLimits
{
	float MaxYawDeg = 35.f;
	float MinPitchDeg = -12.f;
	float MaxPitchDeg = 25.f;
	float RateDegPerSec = 220.f;
};

inline float MoveToward(float Current, float Target, float MaxStep)
{
	const float D = Target - Current;
	if (D > MaxStep) return Current + MaxStep;
	if (D < -MaxStep) return Current - MaxStep;
	return Target;
}

// LocalDir is the aim direction in the car's own frame (X forward, Y right, Z up). Updates the
// turret's yaw and pitch (degrees) one step toward it.
inline void TurnTurret(const Vec& LocalDir, float DeltaSeconds, const TurretLimits& L, float& InOutYawDeg, float& InOutPitchDeg)
{
	const float Flat = std::sqrt(LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y);
	float Yaw = Deg(std::atan2(LocalDir.Y, LocalDir.X));
	float Pitch = Deg(std::atan2(LocalDir.Z, Flat));
	Yaw = Yaw < -L.MaxYawDeg ? -L.MaxYawDeg : (Yaw > L.MaxYawDeg ? L.MaxYawDeg : Yaw);
	Pitch = Pitch < L.MinPitchDeg ? L.MinPitchDeg : (Pitch > L.MaxPitchDeg ? L.MaxPitchDeg : Pitch);
	const float Step = L.RateDegPerSec * (DeltaSeconds > 0.f ? DeltaSeconds : 0.f);
	InOutYawDeg = MoveToward(InOutYawDeg, Yaw, Step);
	InOutPitchDeg = MoveToward(InOutPitchDeg, Pitch, Step);
}

// ---- Rotary barrels: spin up while firing, run down after.
struct SpinTuning
{
	float MaxRpm = 1200.f;
	float SpinUpPerSec = 3000.f;   // rpm gained per second of fire.
	float SpinDownPerSec = 900.f;  // rpm lost per second once the trigger is released.
};

inline float SpinRpm(float Rpm, bool bFiring, float DeltaSeconds, const SpinTuning& T = SpinTuning())
{
	Rpm += (bFiring ? T.SpinUpPerSec : -T.SpinDownPerSec) * DeltaSeconds;
	return Rpm < 0.f ? 0.f : (Rpm > T.MaxRpm ? T.MaxRpm : Rpm);
}
}
