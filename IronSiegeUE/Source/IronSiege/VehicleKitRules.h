#pragma once
// Engine-independent "war kits": the bolt-on armour, weapons housings, wings, ploughs and spikes that
// give each vehicle class its own silhouette. The game only ships two chassis (a sports car and an
// off-road buggy), so without these the Scout and Assault are the same car in different paint, and
// the Heavy, Artillery, enemy buggy and boss are all one buggy. No Unreal includes on purpose:
// covered offline by Tests/vehicle_kit_rules_test.cpp.
//
// Parts are positioned as fractions of the chassis' half-extents (X forward, Y right, Z up, origin
// at the centre of its bounds), so one kit fits whichever chassis it is bolted to. Sizes are in cm,
// matching the engine's 100 cm basic shapes.

namespace IronKits
{
enum class Shape : unsigned char { Cube, Cylinder, Cone, Sphere };
// White/Red: missile bodies and noses (basic build); Brass: spent cases and belted rounds; Olive:
// painted military hardware; Edge: chamfers worn through to bare metal; Glass: lenses, seeker domes.
enum class Finish : unsigned char { Steel, Rust, Hazard, Black, Glow, White, Red, Brass, Olive, Edge, Glass };
enum class Kit : unsigned char { None, Scout, Assault, Heavy, Artillery, Raider, Brute, Juggernaut, Hunter, Interceptor, Dune, Stormer, Lancer, Count };

struct Part
{
	Shape Form;
	Finish Look;
	float X, Y, Z;             // Fractions of the chassis half-extents.
	float SizeX, SizeY, SizeZ; // Centimetres.
	float Pitch, Yaw, Roll;    // Degrees.
	bool Mirror;               // Also place a copy on the other side.
	int Variant;               // 0 = always; N = only in variant N (enemy variety).
};

struct PartList
{
	const Part* Parts;
	int Count;
};

// Enemies come in a few variants so a wave is not a row of clones; the player's cars have one look.
inline int VariantCount(Kit K)
{
	return (K == Kit::Raider || K == Kit::Brute) ? 3 : 1;
}

inline PartList Get(Kit K)
{
	using S = Shape;
	using F = Finish;
	// Street racer: low wing, skirts, splitter, a stripe on the bonnet.
	static const Part Scout[] = {
		{ S::Cube, F::Black,  -0.90f, 0.00f,  0.85f,  40.f, 175.f,   5.f,  -8.f, 0.f,   0.f, false, 0 },
		{ S::Cube, F::Black,  -0.88f, 0.35f,  0.60f,   8.f,   5.f,  30.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube, F::Black,   0.00f, 0.98f, -0.55f, 250.f,   6.f,  10.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube, F::Black,   1.00f, 0.00f, -0.62f,  25.f, 180.f,   4.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube, F::Hazard,  0.55f, 0.00f,  0.40f,  90.f,  18.f,   2.f,  -8.f, 0.f,   0.f, false, 0 },
	};
	// Interceptor: ram bar with uprights, hazard stripe and side armour strips (the roof weapons are
	// the autocannon and missile pod every car carries - WeaponRigRules.h).
	static const Part Assault[] = {
		{ S::Cube,     F::Steel,  1.04f, 0.00f, -0.30f,  14.f, 190.f,  16.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Steel,  1.02f, 0.35f, -0.05f,  12.f,  12.f,  45.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Steel,  0.00f, 1.00f, -0.20f, 220.f,   6.f,  28.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Hazard, 1.08f, 0.00f, -0.30f,   4.f, 120.f,  10.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Black, -0.95f, 0.00f, -0.45f,  12.f, 170.f,  12.f,   0.f, 0.f,   0.f, false, 0 },
	};
	// Armoured brick: slab sides, a raked plough, a roof plate and stacks.
	static const Part Heavy[] = {
		{ S::Cube,     F::Steel, -0.05f, 1.00f,  0.05f, 240.f,   8.f,  70.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Rust,   1.05f, 0.00f, -0.35f,  30.f, 240.f,  60.f,  25.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Hazard, 1.12f, 0.00f, -0.30f,   4.f, 200.f,  10.f,  25.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Steel, -0.10f, 0.00f,  1.02f, 170.f, 180.f,   8.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Black, -0.75f, 0.62f,  0.80f,  14.f,  14.f,  80.f,   0.f, 0.f,   0.f, true,  0 },
	};
	// Rocket carrier: rear stabiliser legs, antenna, radar dome and armoured flanks.
	static const Part Artillery[] = {
		{ S::Cylinder, F::Black, -1.00f, 0.80f, -0.45f,  10.f,  10.f,  70.f,  30.f, 0.f,   0.f, true,  0 },
		{ S::Cylinder, F::Black, -0.80f,-0.60f,  1.35f,   3.f,   3.f, 130.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Sphere,   F::Steel,  0.45f, 0.00f,  1.08f,  40.f,  40.f,  12.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Steel, -0.05f, 1.00f,  0.00f, 200.f,   8.f,  50.f,   0.f, 0.f,   0.f, true,  0 },
	};
	// Enemy sports car: a tall striped wing always, then side spikes, a roof scoop or a spiked nose.
	static const Part Raider[] = {
		{ S::Cube,   F::Hazard, -0.92f, 0.00f,  1.00f,  45.f, 180.f,   6.f, -12.f, 0.f,   0.f, false, 0 },
		{ S::Cube,   F::Black,  -0.88f, 0.35f,  0.70f,   8.f,   5.f,  55.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,   F::Black,   0.00f, 0.98f, -0.55f, 250.f,   6.f,  10.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cone,   F::Rust,    0.10f, 1.05f, -0.30f,  14.f,  14.f,  45.f,   0.f, 0.f, -90.f, true,  1 },
		{ S::Cone,   F::Rust,   -0.40f, 1.05f, -0.30f,  14.f,  14.f,  45.f,   0.f, 0.f, -90.f, true,  1 },
		{ S::Cube,   F::Black,   0.20f, 0.00f,  1.05f,  60.f,  40.f,  18.f,   0.f, 0.f,   0.f, false, 2 },
		{ S::Sphere, F::Glow,    0.20f, 0.00f,  1.20f,  12.f,  12.f,  12.f,   0.f, 0.f,   0.f, false, 2 },
		{ S::Cone,   F::Rust,    1.08f, 0.40f, -0.30f,  14.f,  14.f,  45.f, -90.f, 0.f,   0.f, true,  3 },
		{ S::Cone,   F::Rust,    1.10f, 0.00f, -0.30f,  16.f,  16.f,  55.f, -90.f, 0.f,   0.f, false, 3 },
	};
	// Enemy buggy: a spiked nose always, then scrap armour bolted on wherever it fitted.
	static const Part Brute[] = {
		{ S::Cone,     F::Rust,   1.12f, 0.00f, -0.20f,  18.f,  18.f,  60.f, -90.f, 0.f,   0.f, false, 0 },
		{ S::Cone,     F::Rust,   1.10f, 0.45f, -0.20f,  16.f,  16.f,  50.f, -90.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Rust,   1.02f, 0.00f, -0.40f,  12.f, 200.f,  14.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Rust,   0.00f, 1.00f,  0.00f, 180.f,   8.f,  60.f,   0.f, 0.f,   6.f, false, 1 },
		{ S::Sphere,   F::Glow,   0.20f, 0.00f,  1.05f,  16.f,  16.f,  16.f,   0.f, 0.f,   0.f, false, 1 },
		{ S::Cube,     F::Rust,   0.00f,-1.00f,  0.00f, 180.f,   8.f,  60.f,   0.f, 0.f,  -6.f, false, 2 },
		{ S::Cube,     F::Hazard, 1.05f, 0.00f, -0.45f,  12.f, 220.f,  14.f,   0.f, 0.f,   0.f, false, 2 },
		{ S::Cube,     F::Rust,  -0.10f, 0.00f,  1.02f, 150.f, 170.f,   8.f,   0.f, 0.f,   0.f, false, 3 },
		{ S::Cylinder, F::Black, -0.75f, 0.60f,  0.80f,  14.f,  14.f,  75.f,   0.f, 0.f,   0.f, true,  3 },
	};
	// Boss: a hazard-striped plough with spikes, slab armour, roof plate, red light bar, big stacks.
	static const Part Juggernaut[] = {
		{ S::Cube,     F::Hazard, 1.08f, 0.00f, -0.30f,  40.f, 270.f,  90.f,  20.f, 0.f,   0.f, false, 0 },
		{ S::Cone,     F::Steel,  1.20f, 0.50f, -0.25f,  20.f,  20.f,  55.f, -90.f, 0.f,   0.f, true,  0 },
		{ S::Cone,     F::Steel,  1.22f, 0.00f, -0.25f,  22.f,  22.f,  65.f, -90.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Steel, -0.05f, 1.02f,  0.05f, 260.f,  14.f,  90.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Steel, -0.10f, 0.00f,  1.05f, 190.f, 200.f,  14.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Glow,   0.30f, 0.00f,  1.12f,  16.f, 150.f,   8.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Black, -0.80f, 0.55f,  0.90f,  20.f,  20.f, 110.f,   0.f, 0.f,   0.f, true,  0 },
	};

	// Missile hunter (enemy buggy): a tracking radar dish on a mast (front corner, clear of the
	// missile pod), a whip antenna, a glowing
	// sensor eye up front and hazard flanks - it reads as "the one that locks on" from a distance.
	static const Part Hunter[] = {
		{ S::Cylinder, F::Black,  0.45f, 0.62f,  1.05f,   6.f,   6.f,  60.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Sphere,   F::Steel,  0.45f, 0.62f,  1.38f,  60.f,  60.f,  12.f,   0.f, 0.f, -30.f, false, 0 },
		{ S::Cylinder, F::Black, -0.90f,-0.55f,  1.30f,   3.f,   3.f, 110.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Sphere,   F::Glow,   1.03f, 0.00f,  0.10f,  18.f,  18.f,  18.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Hazard, 0.00f, 1.00f, -0.25f, 200.f,   6.f,  14.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Black,  1.04f, 0.00f, -0.35f,  14.f, 200.f,  18.f,   0.f, 0.f,   0.f, false, 0 },
	};

	// Sports sedan: low ducktail wing, splitter, bonnet scoop, twin white racing stripes, skirts.
	static const Part Interceptor[] = {
		{ S::Cube,   F::Black, -0.93f, 0.00f,  0.55f,  30.f, 170.f,   4.f,  -6.f, 0.f,   0.f, false, 0 },
		{ S::Cube,   F::Black,  0.95f, 0.00f, -0.56f,  24.f, 170.f,   4.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,   F::Black,  0.62f, 0.00f,  0.28f,  50.f,  40.f,   8.f,  -4.f, 0.f,   0.f, false, 0 },
		{ S::Cube,   F::White,  0.66f, 0.14f,  0.25f, 130.f,  14.f,   1.f,  -5.f, 0.f,   0.f, true,  0 },
		{ S::Cube,   F::Black,  0.00f, 0.86f, -0.52f, 260.f,   6.f,  10.f,   0.f, 0.f,   0.f, true,  0 },
	};
	// Desert buggy: roof light bar, bull bar, spare wheel on the tail, snorkel, mud flaps.
	static const Part Dune[] = {
		{ S::Cube,     F::Black,  0.35f, 0.00f,  1.02f,  10.f, 150.f,  10.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Sphere,   F::Glow,   0.40f, 0.30f,  1.02f,  12.f,  12.f,  12.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Steel,  1.08f, 0.00f, -0.20f,  10.f, 170.f,  50.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Black, -1.08f, 0.00f,  0.10f,  70.f,  70.f,  25.f,   0.f, 0.f,  90.f, false, 0 },
		{ S::Cylinder, F::Black,  0.60f, 0.95f,  0.60f,   8.f,   8.f,  90.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Hazard,-0.95f, 0.80f, -0.60f,   4.f,  35.f,  30.f,   0.f, 0.f,   0.f, true,  0 },
	};

	// Tesla stormer (enemy buggy): a coil tower on the tail - stacked rings round a post with a glowing
	// ball on top - plus hazard nose and side plates.
	static const Part Stormer[] = {
		{ S::Cylinder, F::Black, -0.70f, 0.00f,  1.10f,  14.f,  14.f,  90.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Steel, -0.70f, 0.00f,  1.00f,  46.f,  46.f,   6.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Steel, -0.70f, 0.00f,  1.18f,  38.f,  38.f,   6.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cylinder, F::Steel, -0.70f, 0.00f,  1.34f,  30.f,  30.f,   6.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Sphere,   F::Glow,  -0.70f, 0.00f,  1.40f,  30.f,  30.f,  30.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Hazard, 1.05f, 0.00f, -0.30f,  12.f, 190.f,  20.f,   0.f, 0.f,   0.f, false, 0 },
		{ S::Cube,     F::Black,  0.00f, 1.00f,  0.00f, 170.f,   8.f,  50.f,   0.f, 0.f,   0.f, true,  0 },
	};
	// Railgun lancer (enemy sedan): twin rail barrels along the roof edges ending past the bonnet,
	// glowing capacitor packs behind them.
	static const Part Lancer[] = {
		{ S::Cube,     F::Black,  0.35f, 0.55f,  1.02f, 300.f,  10.f,  12.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Glow,   0.35f, 0.55f,  1.10f, 260.f,   3.f,   3.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Steel, -0.55f, 0.55f,  1.02f,  60.f,  22.f,  24.f,   0.f, 0.f,   0.f, true,  0 },
		{ S::Cube,     F::Hazard,-0.93f, 0.00f,  0.55f,  30.f, 170.f,   5.f,  -6.f, 0.f,   0.f, false, 0 },
	};

	switch (K)
	{
	case Kit::Scout: return { Scout, int(sizeof(Scout) / sizeof(Part)) };
	case Kit::Assault: return { Assault, int(sizeof(Assault) / sizeof(Part)) };
	case Kit::Heavy: return { Heavy, int(sizeof(Heavy) / sizeof(Part)) };
	case Kit::Artillery: return { Artillery, int(sizeof(Artillery) / sizeof(Part)) };
	case Kit::Raider: return { Raider, int(sizeof(Raider) / sizeof(Part)) };
	case Kit::Brute: return { Brute, int(sizeof(Brute) / sizeof(Part)) };
	case Kit::Juggernaut: return { Juggernaut, int(sizeof(Juggernaut) / sizeof(Part)) };
	case Kit::Hunter: return { Hunter, int(sizeof(Hunter) / sizeof(Part)) };
	case Kit::Interceptor: return { Interceptor, int(sizeof(Interceptor) / sizeof(Part)) };
	case Kit::Dune: return { Dune, int(sizeof(Dune) / sizeof(Part)) };
	case Kit::Stormer: return { Stormer, int(sizeof(Stormer) / sizeof(Part)) };
	case Kit::Lancer: return { Lancer, int(sizeof(Lancer) / sizeof(Part)) };
	default: return { nullptr, 0 };
	}
}

// Whether a part belongs on a car with this variant (1-based; variant parts only on their own).
inline bool IsActive(const Part& P, int Variant)
{
	return P.Variant == 0 || P.Variant == Variant;
}

// A mirrored copy sits on the other side, with its sideways rotations flipped to match.
inline Part Mirrored(const Part& P)
{
	Part M = P;
	M.Y = -P.Y;
	M.Yaw = -P.Yaw;
	M.Roll = -P.Roll;
	M.Mirror = false;
	return M;
}

// How many meshes a car ends up wearing, counting mirrored copies.
inline int PlacedCount(Kit K, int Variant)
{
	const PartList L = Get(K);
	int N = 0;
	for (int i = 0; i < L.Count; ++i)
	{
		if (IsActive(L.Parts[i], Variant)) N += L.Parts[i].Mirror ? 2 : 1;
	}
	return N;
}

// Clamps an incoming variant (0 or out of range means "pick for me") using a caller-supplied roll,
// so the builder stays deterministic under test.
inline int ResolveVariant(Kit K, int Requested, int RandomRoll)
{
	const int N = VariantCount(K);
	if (Requested >= 1 && Requested <= N) return Requested;
	const int R = RandomRoll < 0 ? -RandomRoll : RandomRoll;
	return 1 + R % N;
}
}
