#pragma once
// Engine-independent energy weapons: the railgun's charge-up and the targets its slug punches
// through, and the tesla coil's lightning chain (which cars it jumps to, and how hard each is hit).
// No Unreal includes on purpose: covered offline by Tests/beam_rules_test.cpp.

#include <cmath>

namespace IronBeams
{
struct Vec
{
	float X = 0.f, Y = 0.f, Z = 0.f;
};

inline Vec Sub(const Vec& A, const Vec& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
inline float Dot(const Vec& A, const Vec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
inline float LengthSq(const Vec& A) { return Dot(A, A); }

// ---- Railgun

struct RailTuning
{
	float ChargeSeconds = 0.45f;  // Whine before the shot - telegraphs it and punishes spam.
	float HitRadiusCm = 170.f;    // How close to the line a car's centre must be to be struck.
	float FalloffPerHit = 0.8f;   // Each car the slug passes through keeps this share for the next.
};

// Charge-up: Start() on the trigger, Tick() every frame; Tick returns true once, on the frame
// the shot should leave the barrel.
struct ChargeClock
{
	float Remaining = -1.f;

	bool IsCharging() const { return Remaining >= 0.f; }
	void Start(const RailTuning& T = RailTuning()) { if (!IsCharging()) Remaining = T.ChargeSeconds; }

	bool Tick(float DeltaSeconds)
	{
		if (!IsCharging()) return false;
		Remaining -= DeltaSeconds;
		if (Remaining > 0.f) return false;
		Remaining = -1.f;
		return true;
	}

	// 0..1 for the charge glow and the HUD.
	float Progress(const RailTuning& T = RailTuning()) const
	{
		if (!IsCharging() || T.ChargeSeconds <= 0.f) return 0.f;
		const float P = 1.f - Remaining / T.ChargeSeconds;
		return P < 0.f ? 0.f : (P > 1.f ? 1.f : P);
	}
};

// Distance along the segment From->To of the point closest to P (negative / beyond the length if
// P lies before the start or past the end), and the squared distance from P to that closest point.
inline float ClosestAlong(const Vec& From, const Vec& To, const Vec& P, float& OutDistSq)
{
	const Vec D = Sub(To, From);
	const float Len2 = LengthSq(D);
	float T = Len2 > 1e-6f ? Dot(Sub(P, From), D) / Len2 : 0.f;
	const float Clamped = T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
	const Vec C{ From.X + D.X * Clamped, From.Y + D.Y * Clamped, From.Z + D.Z * Clamped };
	OutDistSq = LengthSq(Sub(P, C));
	return T * (Len2 > 0.f ? std::sqrt(Len2) : 0.f);
}

// Which of Count targets the slug strikes, nearest first: writes their indices to OutOrder and
// returns how many. Targets behind the muzzle or beyond the range are never hit.
inline int PiercedTargets(const Vec& From, const Vec& To, const Vec* Targets, int Count, int* OutOrder, const RailTuning& T = RailTuning())
{
	int N = 0;
	float Along[64];
	for (int i = 0; i < Count && N < 64; ++i)
	{
		float DistSq = 0.f;
		const float A = ClosestAlong(From, To, Targets[i], DistSq);
		const float Total = std::sqrt(LengthSq(Sub(To, From)));
		if (A < 0.f || A > Total || DistSq > T.HitRadiusCm * T.HitRadiusCm) continue;
		// Insertion sort by distance along the line.
		int j = N++;
		while (j > 0 && Along[j - 1] > A)
		{
			Along[j] = Along[j - 1];
			OutOrder[j] = OutOrder[j - 1];
			--j;
		}
		Along[j] = A;
		OutOrder[j] = i;
	}
	return N;
}

// Damage for the Nth car pierced (0 = first).
inline float PierceDamage(float Base, int HitIndex, const RailTuning& T = RailTuning())
{
	float D = Base;
	for (int i = 0; i < HitIndex; ++i) D *= T.FalloffPerHit;
	return D;
}

// ---- Tesla coil

struct TeslaTuning
{
	float FirstRangeCm = 2200.f;  // Reach from the coil to the first target...
	float FirstConeDot = 0.5f;    // ...which must be within ~60 degrees of the aim.
	float JumpRangeCm = 1100.f;   // Each further jump reaches this far from the last target.
	int MaxTargets = 4;
	float DamageKeep = 0.7f;      // Each jump carries on with this share of the damage.
	float StunSeconds = 1.2f;     // Engines of struck cars cut out for this long.
};

// Builds the lightning chain: first the target nearest the coil inside the aim cone and range,
// then repeatedly the nearest not-yet-struck target within jump range of the last one. Writes
// target indices to OutChain (up to MaxTargets) and returns the chain length.
inline int BuildChain(const Vec& Coil, const Vec& AimDir, const Vec* Targets, int Count, int* OutChain, const TeslaTuning& T = TeslaTuning())
{
	int N = 0;
	bool Used[64] = {};
	const int Limit = Count < 64 ? Count : 64;
	Vec From = Coil;
	for (int Link = 0; Link < T.MaxTargets; ++Link)
	{
		const float Range = Link == 0 ? T.FirstRangeCm : T.JumpRangeCm;
		int Best = -1;
		float BestSq = Range * Range;
		for (int i = 0; i < Limit; ++i)
		{
			if (Used[i]) continue;
			const Vec To = Sub(Targets[i], From);
			const float Sq = LengthSq(To);
			if (Sq > BestSq || Sq < 1.f) continue;
			if (Link == 0)
			{
				const float Len = std::sqrt(Sq);
				const float AimLen = std::sqrt(LengthSq(AimDir));
				if (AimLen < 1e-6f || Dot(To, AimDir) / (Len * AimLen) < T.FirstConeDot) continue;
			}
			Best = i;
			BestSq = Sq;
		}
		if (Best < 0) break;
		Used[Best] = true;
		OutChain[N++] = Best;
		From = Targets[Best];
	}
	return N;
}

inline float ChainDamage(float Base, int LinkIndex, const TeslaTuning& T = TeslaTuning())
{
	float D = Base;
	for (int i = 0; i < LinkIndex; ++i) D *= T.DamageKeep;
	return D;
}
}
