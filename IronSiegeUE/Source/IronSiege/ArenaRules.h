#pragma once
// Engine-independent battlefield layouts: the themed set pieces (jump ramps, rock clusters, a
// plateau with ramps up to it, a frozen lake ringed by ice, a container maze...) AIronArena builds
// on the greybox maps. Deterministic for a given seed, and every layout keeps the player starts and
// the perimeter clear. No Unreal includes on purpose: covered offline by Tests/arena_rules_test.cpp.

#include <cmath>

namespace IronArena
{
enum class Theme : unsigned char { Desert, Arctic, Port, City, Count };

enum class PropKind : unsigned char
{
	Ramp,        // Tilted slab to launch cars; Scale = length factor.
	Rock,        // Boulder (desert rock / arctic ice rock).
	Mesa,        // Flat-topped plateau; ramps lead up to it.
	Container,   // Shipping container.
	Tank,        // Upright fuel tank.
	Barrier,     // Concrete jersey barrier.
	IceSpire,    // Tall ice shard.
	Tower,       // Water tower.
	LakeIce,     // Flat frozen lake disc (low friction look; purely visual).
	Wreck,       // Burnt-out car (Level 1: still burning).
	Sandbags,    // Sandbag wall.
	TankTrap,    // Steel "hedgehog" anti-vehicle obstacle.
	Rubble,      // Heap of broken concrete.
	Count
};

struct Prop
{
	PropKind Kind;
	float X, Y;   // cm, map centre at the origin.
	float Yaw;    // degrees
	float Scale;  // 1 = stock size for the kind
	int Level;    // Stack level (containers) - 0 on the ground.
};

inline constexpr int MaxProps = 220;

struct Layout
{
	Prop Props[MaxProps];
	int Count = 0;

	bool Add(const Prop& P)
	{
		if (Count >= MaxProps) return false;
		Props[Count++] = P;
		return true;
	}
	int CountOf(PropKind K) const
	{
		int N = 0;
		for (int i = 0; i < Count; ++i) N += Props[i].Kind == K ? 1 : 0;
		return N;
	}
};

struct Tuning
{
	float HalfExtent = 9500.f;      // Playable square is +-HalfExtent (the greybox walls sit there).
	float WallMargin = 900.f;       // Nothing closer than this to a wall.
	float StartClearRadius = 2600.f;// Player starts must stay open to drive off.
	float StartOffset = 6600.f;     // The four starts sit at (+-StartOffset, +-StartOffset).
	float ScatterSpacing = 1100.f;  // Minimum gap between scattered (non-composite) props.
};

// Rough footprint radius of a prop, for spacing and clearance checks.
inline float Radius(const Prop& P)
{
	switch (P.Kind)
	{
	case PropKind::Ramp: return 450.f * P.Scale;
	case PropKind::Rock: return 250.f * P.Scale;
	case PropKind::Mesa: return 1800.f * P.Scale;
	case PropKind::Container: return 620.f; // 40 ft (12.2 m) container.
	case PropKind::Tank: return 220.f * P.Scale;
	case PropKind::Barrier: return 170.f;
	case PropKind::IceSpire: return 150.f * P.Scale;
	case PropKind::Tower: return 350.f;
	case PropKind::LakeIce: return 2600.f * P.Scale;
	case PropKind::Wreck: return 260.f;
	case PropKind::Sandbags: return 230.f;
	case PropKind::TankTrap: return 120.f;
	case PropKind::Rubble: return 220.f * P.Scale;
	default: return 200.f;
	}
}

// Small deterministic generator so a map looks the same every time it loads.
struct Rng
{
	unsigned State;
	explicit Rng(unsigned Seed) : State(Seed * 2654435761u + 12345u) {}
	unsigned Next() { State = State * 1664525u + 1013904223u; return State >> 8; }
	float Unit() { return (Next() & 0xFFFF) / 65535.f; }
	float Range(float Lo, float Hi) { return Lo + (Hi - Lo) * Unit(); }
};

inline float Sqr(float V) { return V * V; }

// Whether a prop may go here: inside the walls with margin, off the player starts, and clear of
// every scattered prop already placed (composites - the plateau, the lake, stacked containers -
// are allowed to touch their own parts, so only the scatter checks spacing).
inline bool IsFree(const Layout& L, const Prop& P, const Tuning& T, bool bCheckSpacing = true)
{
	const float R = Radius(P);
	const float Limit = T.HalfExtent - T.WallMargin - R;
	if (P.X < -Limit || P.X > Limit || P.Y < -Limit || P.Y > Limit) return false;
	for (int sx = -1; sx <= 1; sx += 2)
		for (int sy = -1; sy <= 1; sy += 2)
			if (Sqr(P.X - sx * T.StartOffset) + Sqr(P.Y - sy * T.StartOffset) < Sqr(T.StartClearRadius + R)) return false;
	if (!bCheckSpacing) return true;
	for (int i = 0; i < L.Count; ++i)
	{
		const Prop& O = L.Props[i];
		if (Sqr(P.X - O.X) + Sqr(P.Y - O.Y) < Sqr(R + Radius(O) + T.ScatterSpacing * 0.5f)) return false;
	}
	return true;
}

// Tries up to Attempts random spots in the playable square for a prop of Kind.
inline int Scatter(Layout& L, Rng& R, PropKind Kind, int Wanted, float ScaleLo, float ScaleHi, const Tuning& T, int Attempts = 400)
{
	int Placed = 0;
	const float Span = T.HalfExtent - T.WallMargin;
	for (int a = 0; a < Attempts && Placed < Wanted; ++a)
	{
		const Prop P{ Kind, R.Range(-Span, Span), R.Range(-Span, Span), R.Range(0.f, 360.f), R.Range(ScaleLo, ScaleHi), 0 };
		if (IsFree(L, P, T) && L.Add(P)) ++Placed;
	}
	return Placed;
}

// ---- City battlefield: props laid along an existing road network rather than scattered.

struct Road
{
	float AX, AY, BX, BY; // Centre line, cm.
	float Width;          // Carriageway width, cm.
};

// Distance from P to a road's centre line (segment), and the signed sideways offset.
inline float DistanceToRoad(const Road& R, float X, float Y, float* OutAlong = nullptr)
{
	const float DX = R.BX - R.AX, DY = R.BY - R.AY;
	const float Len2 = DX * DX + DY * DY;
	float T = Len2 > 1e-3f ? ((X - R.AX) * DX + (Y - R.AY) * DY) / Len2 : 0.f;
	if (OutAlong) *OutAlong = T;
	T = T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
	const float CX = R.AX + DX * T, CY = R.AY + DY * T;
	return std::sqrt(Sqr(X - CX) + Sqr(Y - CY));
}

struct CityTuning
{
	float Step = 1100.f;          // Roughly one set piece per this much road.
	float StartClear = 1000.f;    // Around each player start (the city starts sit on the avenue).
	float Spacing = 900.f;        // Minimum gap between props.
	float SideMin = 0.24f;        // Sideways offset as a fraction of the road width...
	float SideMax = 0.34f;        // ...so the other half of the road always stays open.
};

// Obstacles every Step along each road, alternating (mostly) from one side to the other so the
// street becomes a slalom with a lane always open, kept out of junctions (a prop may not sit inside
// another road's carriageway) and off the player starts. Starts is Count (x, y) pairs.
inline Layout GenerateCity(const Road* Roads, int NumRoads, const float* Starts, int NumStarts, unsigned Seed, const CityTuning& T = CityTuning())
{
	Layout L;
	Rng R(Seed * 97u + 13u);
	for (int r = 0; r < NumRoads; ++r)
	{
		const Road& Rd = Roads[r];
		const float DX = Rd.BX - Rd.AX, DY = Rd.BY - Rd.AY;
		const float Len = std::sqrt(DX * DX + DY * DY);
		if (Len < 1.f) continue;
		const float UX = DX / Len, UY = DY / Len;       // Along the road.
		const float NX = -UY, NY = UX;                   // Across it.
		const float Yaw = std::atan2(UY, UX) * 57.2957795f;
		float Side = R.Unit() < 0.5f ? -1.f : 1.f;
		for (float D = T.Step * 0.5f; D < Len - T.Step * 0.3f; D += T.Step * R.Range(0.8f, 1.2f))
		{
			Side = R.Unit() < 0.8f ? -Side : Side;
			const float Roll = R.Unit();
			Prop P{ PropKind::Wreck, 0.f, 0.f, 0.f, 1.f, 0 };
			float SideFrac = R.Range(T.SideMin, T.SideMax);
			if (Roll < 0.3f) { P.Kind = PropKind::Wreck; P.Yaw = Yaw + R.Range(-35.f, 35.f) + (R.Unit() < 0.5f ? 180.f : 0.f); P.Level = R.Unit() < 0.4f ? 1 : 0; }
			else if (Roll < 0.5f) { P.Kind = PropKind::Barrier; P.Yaw = Yaw + R.Range(-30.f, 30.f); }
			else if (Roll < 0.65f) { P.Kind = PropKind::Sandbags; P.Yaw = Yaw + 90.f + R.Range(-15.f, 15.f); }
			else if (Roll < 0.8f) { P.Kind = PropKind::TankTrap; P.Yaw = R.Range(0.f, 360.f); }
			else if (Roll < 0.93f) { P.Kind = PropKind::Rubble; P.Yaw = R.Range(0.f, 360.f); P.Scale = R.Range(0.8f, 1.3f); }
			else { P.Kind = PropKind::Ramp; P.Yaw = Yaw + (R.Unit() < 0.5f ? 0.f : 180.f); P.Scale = 1.f; SideFrac = 0.25f; }
			const float Off = Side * SideFrac * Rd.Width;
			P.X = Rd.AX + UX * D + NX * Off;
			P.Y = Rd.AY + UY * D + NY * Off;
			// Never in a junction: inside another road's carriageway (plus a car's width).
			bool bOk = true;
			for (int o = 0; o < NumRoads && bOk; ++o)
			{
				if (o != r && DistanceToRoad(Roads[o], P.X, P.Y) < Roads[o].Width * 0.5f + 500.f) bOk = false;
			}
			for (int s = 0; s < NumStarts && bOk; ++s)
			{
				if (Sqr(P.X - Starts[2 * s]) + Sqr(P.Y - Starts[2 * s + 1]) < Sqr(T.StartClear + Radius(P))) bOk = false;
			}
			for (int i = 0; i < L.Count && bOk; ++i)
			{
				if (Sqr(P.X - L.Props[i].X) + Sqr(P.Y - L.Props[i].Y) < Sqr(Radius(P) + Radius(L.Props[i]) + T.Spacing * 0.3f)) bOk = false;
			}
			if (bOk) L.Add(P);
		}
	}
	return L;
}

inline Layout Generate(Theme Th, unsigned Seed, const Tuning& T = Tuning())
{
	Layout L;
	Rng R(Seed + static_cast<unsigned>(Th) * 7919u);
	switch (Th)
	{
	case Theme::Desert:
	{
		// A plateau in the middle with four ramps climbing onto it, rock clusters and jumps.
		L.Add({ PropKind::Mesa, 0.f, 0.f, 0.f, 1.f, 0 });
		for (int i = 0; i < 4; ++i)
		{
			const float Yaw = 45.f + 90.f * i;
			const float Rad = Yaw * 0.0174533f;
			const float D = Radius(L.Props[0]) + 520.f;
			// Level 1 marks a plateau ramp: steeper and long enough to reach the top.
			L.Add({ PropKind::Ramp, D * std::cos(Rad), D * std::sin(Rad), Yaw + 180.f, 1.4f, 1 });
		}
		Scatter(L, R, PropKind::Rock, 26, 0.8f, 2.2f, T);
		Scatter(L, R, PropKind::Ramp, 7, 0.9f, 1.2f, T);
		Scatter(L, R, PropKind::Tank, 4, 0.9f, 1.1f, T);
		break;
	}
	case Theme::Arctic:
	{
		// A frozen lake ringed by ice spires, a fuel depot, barrier lines and jumps.
		L.Add({ PropKind::LakeIce, 0.f, 0.f, 0.f, 1.f, 0 });
		for (int i = 0; i < 12; ++i)
		{
			const float Rad = (30.f * i + R.Range(-8.f, 8.f)) * 0.0174533f;
			const float D = Radius(L.Props[0]) + R.Range(150.f, 450.f);
			L.Add({ PropKind::IceSpire, D * std::cos(Rad), D * std::sin(Rad), R.Range(0.f, 360.f), R.Range(0.9f, 1.8f), 0 });
		}
		Scatter(L, R, PropKind::Tank, 6, 0.9f, 1.3f, T);
		Scatter(L, R, PropKind::Barrier, 14, 1.f, 1.f, T);
		Scatter(L, R, PropKind::Rock, 14, 0.8f, 1.8f, T);
		Scatter(L, R, PropKind::Ramp, 6, 0.9f, 1.2f, T);
		break;
	}
	case Theme::Port:
	{
		// A container maze in a grid of yards (some stacked two high), a water tower and tanks.
		L.Add({ PropKind::Tower, 0.f, 0.f, 0.f, 1.f, 0 });
		for (int gx = -2; gx <= 2; ++gx)
		{
			for (int gy = -2; gy <= 2; ++gy)
			{
				if ((gx == 0 && gy == 0) || R.Unit() < 0.25f) continue;
				const bool bAlongX = R.Unit() < 0.5f;
				const Prop C{ PropKind::Container, gx * 2600.f + R.Range(-300.f, 300.f), gy * 2600.f + R.Range(-300.f, 300.f), bAlongX ? 0.f : 90.f, 1.f, 0 };
				if (!IsFree(L, C, T, false)) continue;
				L.Add(C);
				if (R.Unit() < 0.45f) L.Add({ PropKind::Container, C.X, C.Y, C.Yaw, 1.f, 1 });
			}
		}
		Scatter(L, R, PropKind::Tank, 6, 0.9f, 1.4f, T);
		Scatter(L, R, PropKind::Barrier, 10, 1.f, 1.f, T);
		Scatter(L, R, PropKind::Ramp, 5, 0.9f, 1.1f, T);
		break;
	}
	default:
		break;
	}
	return L;
}
}
