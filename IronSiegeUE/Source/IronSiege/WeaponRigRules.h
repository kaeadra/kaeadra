#pragma once
// Engine-independent weapon mounts: the roof-mounted rotary autocannon and the four-missile pod.
// Parts are placed in centimetres from the mount point on the roof (X forward, Y right, Z up).
// Missile parts carry a slot number so the pod can show how many missiles are still loaded.
//
// Each rig comes in two builds: the detailed one uses the modelled meshes from Tools/meshgen
// (imported as /Game/IronSiege/Weapons/Rigs/SM_Rig_*), the basic one the engine's basic shapes with
// the kit finishes (VehicleKitRules.h) - used until the meshes are imported, so the game always has
// guns. Parts also say how they move: fixed to the roof, turning with the turret (yaw and pitch
// toward the aim, AimRules.h), or spinning with the rotary barrels.
// No Unreal includes on purpose: covered offline by Tests/weapon_rig_rules_test.cpp.

#include "VehicleKitRules.h"

namespace IronRigs
{
enum class Rig : unsigned char { Autocannon, MissilePod };
enum class Moves : unsigned char { Fixed, Turret, Barrels };

struct Part
{
	IronKits::Shape Form;
	IronKits::Finish Look;
	float X, Y, Z;             // Centimetres from the mount.
	float SizeX, SizeY, SizeZ; // Basic shapes: centimetres. Meshes: scale (1 = as modelled).
	float Pitch, Yaw, Roll;    // Degrees.
	int MissileSlot;           // -1 = structure; 0..N-1 = belongs to loaded missile N.
	Moves Group;
	const char* Mesh;          // Detailed build: the SM_Rig_* mesh; nullptr = basic shape.
};

struct PartList
{
	const Part* Parts;
	int Count;
};

// Where the moving parts hinge, from the mount: the turret turns about a point on its ring, the
// barrel cluster spins about its own axis (which runs along X through this point).
struct Pivots
{
	float TurretX, TurretY, TurretZ;
	float BarrelX, BarrelY, BarrelZ;
};

inline Pivots GetPivots(Rig R, bool bDetailed)
{
	if (R != Rig::Autocannon) return { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	return bDetailed ? Pivots{ 0.f, 0.f, 9.f, 22.f, 0.f, 24.f } : Pivots{ 0.f, 0.f, 8.f, 88.f, 0.f, 18.f };
}

inline PartList Get(Rig R, bool bDetailed = false)
{
	using S = IronKits::Shape;
	using F = IronKits::Finish;
	using M = Moves;
	// ---- Basic build (engine shapes).
	// Rotary autocannon: turret ring and block, a shroud with three barrels, muzzle brake and a
	// side ammo drum. Barrels are cylinders laid along X (pitch -90).
	static const Part Autocannon[] = {
		{ S::Cylinder, F::Steel,  0.f,   0.f,  4.f,  50.f, 50.f,  8.f,   0.f, 0.f, 0.f, -1, M::Fixed,   nullptr },
		{ S::Cube,     F::Steel,  0.f,   0.f, 17.f,  42.f, 34.f, 18.f,   0.f, 0.f, 0.f, -1, M::Turret,  nullptr },
		{ S::Cylinder, F::Black, 45.f,   0.f, 18.f,  13.f, 13.f, 55.f, -90.f, 0.f, 0.f, -1, M::Turret,  nullptr },
		{ S::Cylinder, F::Black, 88.f,   0.f, 22.f,   3.f,  3.f, 40.f, -90.f, 0.f, 0.f, -1, M::Barrels, nullptr },
		{ S::Cylinder, F::Black, 88.f,   3.5f,16.f,   3.f,  3.f, 40.f, -90.f, 0.f, 0.f, -1, M::Barrels, nullptr },
		{ S::Cylinder, F::Black, 88.f,  -3.5f,16.f,   3.f,  3.f, 40.f, -90.f, 0.f, 0.f, -1, M::Barrels, nullptr },
		{ S::Cylinder, F::Steel,106.f,   0.f, 18.f,  10.f, 10.f,  6.f, -90.f, 0.f, 0.f, -1, M::Barrels, nullptr },
		{ S::Cylinder, F::Hazard, -4.f, 22.f, 16.f,  18.f, 18.f, 12.f,  0.f, 0.f, 90.f, -1, M::Turret,  nullptr },
		{ S::Sphere,   F::Glow,  -14.f,  0.f, 28.f,   5.f,  5.f,  5.f,   0.f, 0.f, 0.f, -1, M::Turret,  nullptr },
	};
	// Missile pod: a post, a tilted launcher box with side rails, and four missiles (white body,
	// red nose) poking out of a 2x2 tube grid.
	static const Part MissilePod[] = {
		{ S::Cylinder, F::Steel,  0.f,   0.f,-12.f,  18.f, 18.f, 56.f,   0.f, 0.f, 0.f, -1, M::Fixed, nullptr },
		{ S::Cube,     F::Black,  0.f,   0.f, 30.f,  78.f, 50.f, 34.f,  -6.f, 0.f, 0.f, -1, M::Fixed, nullptr },
		{ S::Cube,     F::Hazard, 0.f,  26.f, 30.f,  70.f,  3.f,  8.f,  -6.f, 0.f, 0.f, -1, M::Fixed, nullptr },
		{ S::Cube,     F::Hazard, 0.f, -26.f, 30.f,  70.f,  3.f,  8.f,  -6.f, 0.f, 0.f, -1, M::Fixed, nullptr },
		{ S::Sphere,   F::Glow,  -32.f,  0.f, 49.f,   6.f,  6.f,  6.f,   0.f, 0.f, 0.f, -1, M::Fixed, nullptr },
		{ S::Cylinder, F::White, 30.f,  11.f, 40.f,   8.f,  8.f, 44.f, -96.f, 0.f, 0.f,  0, M::Fixed, nullptr },
		{ S::Cone,     F::Red,   57.f,  11.f, 43.f,   8.f,  8.f, 12.f, -96.f, 0.f, 0.f,  0, M::Fixed, nullptr },
		{ S::Cylinder, F::White, 30.f, -11.f, 40.f,   8.f,  8.f, 44.f, -96.f, 0.f, 0.f,  1, M::Fixed, nullptr },
		{ S::Cone,     F::Red,   57.f, -11.f, 43.f,   8.f,  8.f, 12.f, -96.f, 0.f, 0.f,  1, M::Fixed, nullptr },
		{ S::Cylinder, F::White, 32.f,  11.f, 25.f,   8.f,  8.f, 44.f, -96.f, 0.f, 0.f,  2, M::Fixed, nullptr },
		{ S::Cone,     F::Red,   59.f,  11.f, 28.f,   8.f,  8.f, 12.f, -96.f, 0.f, 0.f,  2, M::Fixed, nullptr },
		{ S::Cylinder, F::White, 32.f, -11.f, 25.f,   8.f,  8.f, 44.f, -96.f, 0.f, 0.f,  3, M::Fixed, nullptr },
		{ S::Cone,     F::Red,   59.f, -11.f, 28.f,   8.f,  8.f, 12.f, -96.f, 0.f, 0.f,  3, M::Fixed, nullptr },
	};
	// ---- Detailed build (Tools/meshgen meshes, modelled in cm around their own origin).
	// A remote weapon station: a bolted ring, an armoured housing with a sloped glacis, sight box and
	// ammo can that turns and elevates, and a six-barrel cluster with clamp rings that spins.
	static const Part AutocannonDetailed[] = {
		{ S::Cylinder, F::Steel,  0.f, 0.f,  0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, -1, M::Fixed,   "SM_Rig_TurretRing" },
		{ S::Cube,     F::Steel,  0.f, 0.f,  9.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, -1, M::Turret,  "SM_Rig_CannonHousing" },
		{ S::Cylinder, F::Black, 22.f, 0.f, 24.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, -1, M::Barrels, "SM_Rig_BarrelCluster" },
	};
	// Missile pod: a yoke on a post and a four-tube launcher; the missiles sit in the tubes with
	// their noses showing, and go one by one as they are fired.
	static const Part MissilePodDetailed[] = {
		{ S::Cylinder, F::Steel,  0.f,   0.f,  0.f,  1.f, 1.f, 1.f,  0.f, 0.f, 0.f, -1, M::Fixed, "SM_Rig_PodMount" },
		{ S::Cube,     F::Black,  0.f,   0.f, 32.f,  1.f, 1.f, 1.f, -6.f, 0.f, 0.f, -1, M::Fixed, "SM_Rig_PodBox" },
		{ S::Cylinder, F::White, 14.8f,  11.f, 39.f,  1.f, 1.f, 1.f, -6.f, 0.f, 0.f,  0, M::Fixed, "SM_Rig_Missile" },
		{ S::Cylinder, F::White, 14.8f, -11.f, 39.f,  1.f, 1.f, 1.f, -6.f, 0.f, 0.f,  1, M::Fixed, "SM_Rig_Missile" },
		{ S::Cylinder, F::White, 13.f,  11.f, 22.1f, 1.f, 1.f, 1.f, -6.f, 0.f, 0.f,  2, M::Fixed, "SM_Rig_Missile" },
		{ S::Cylinder, F::White, 13.f, -11.f, 22.1f, 1.f, 1.f, 1.f, -6.f, 0.f, 0.f,  3, M::Fixed, "SM_Rig_Missile" },
	};
	switch (R)
	{
	case Rig::Autocannon:
		return bDetailed ? PartList{ AutocannonDetailed, int(sizeof(AutocannonDetailed) / sizeof(Part)) }
						 : PartList{ Autocannon, int(sizeof(Autocannon) / sizeof(Part)) };
	case Rig::MissilePod:
		return bDetailed ? PartList{ MissilePodDetailed, int(sizeof(MissilePodDetailed) / sizeof(Part)) }
						 : PartList{ MissilePod, int(sizeof(MissilePod) / sizeof(Part)) };
	default: return { nullptr, 0 };
	}
}

// How many missiles the rig displays.
inline int MissileSlots(Rig R, bool bDetailed = false)
{
	const PartList L = Get(R, bDetailed);
	int Highest = -1;
	for (int i = 0; i < L.Count; ++i)
	{
		if (L.Parts[i].MissileSlot > Highest) Highest = L.Parts[i].MissileSlot;
	}
	return Highest + 1;
}

// A slot is drawn while that many missiles are still in the magazine (slot 0 is fired last).
inline bool IsMissileVisible(int Slot, int AmmoInMagazine)
{
	return Slot < AmmoInMagazine;
}

// Where the barrel tip / front of the tubes is, from the mount - tracers and missiles start here.
inline void MuzzleOffset(Rig R, float& OutX, float& OutY, float& OutZ, bool bDetailed = false)
{
	OutY = 0.f;
	if (R == Rig::Autocannon)
	{
		OutX = bDetailed ? 98.f : 110.f;
		OutZ = bDetailed ? 24.f : 18.f;
	}
	else
	{
		// The tube mouths: the modelled launcher is shorter than the basic box.
		OutX = bDetailed ? 41.f : 64.f;
		OutZ = bDetailed ? 28.f : 34.f;
	}
}
}
