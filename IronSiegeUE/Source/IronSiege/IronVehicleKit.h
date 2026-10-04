#pragma once
#include "CoreMinimal.h"
#include "VehicleKitRules.h"
#include "WeaponRigRules.h"

class USceneComponent;
class UStaticMeshComponent;

// Bolts a war kit (VehicleKitRules.h) onto a chassis: one engine basic shape per part, placed as a
// fraction of the chassis bounds, dressed in the kit finishes (MI_Kit_* materials) and with
// collision off so the Chaos physics body is untouched. Returns how many meshes were attached.
namespace IronKitBuilder
{
IRONSIEGE_API int32 Build(USceneComponent* Chassis, IronKits::Kit Kit, int32 Variant, TArray<UStaticMeshComponent*>* OutParts = nullptr);

// The chassis bounds the kit is fitted to, in its own space - also where engine smoke comes from.
IRONSIEGE_API FBox LocalBounds(USceneComponent* Chassis);

// What BuildRig made: the hinges the car turns and spins (null for a rig that does not move), and
// whether the modelled meshes were used.
struct FRigBuild
{
	USceneComponent* Turret = nullptr;
	USceneComponent* Barrels = nullptr;
	bool bDetailed = false;
	int32 Parts = 0;
};

// Builds a weapon mount (WeaponRigRules.h) at MountLocal on the chassis: the modelled meshes when
// they have been imported (Tools/meshgen), else the basic-shape build. OutMissiles receives one
// entry per missile part together with its slot, so the car can hide missiles as they are fired.
IRONSIEGE_API FRigBuild BuildRig(USceneComponent* Chassis, const FVector& MountLocal, IronRigs::Rig Rig, TArray<TPair<int32, UStaticMeshComponent*>>* OutMissiles = nullptr);

// The unlockable weapons as models (Tools/meshgen weapon parts): the railgun, the tesla coil and
// the flamethrower ride the cannon's Turret, the mine rack hangs at RearLocal on the chassis.
// Returned in that order and hidden - the car shows each once it is unlocked; an entry is null
// while its mesh is not imported or it has nowhere to go.
IRONSIEGE_API TArray<UStaticMeshComponent*> BuildExtraWeapons(USceneComponent* Chassis, USceneComponent* Turret, const FVector& RearLocal);

// A modelled weapon mesh (/Game/IronSiege/Weapons/Rigs/<Name>), or null if not imported yet.
IRONSIEGE_API class UStaticMesh* RigMesh(const char* Name);

// Dresses a modelled mesh: each material slot named after a kit finish ("Steel", "Black", "Hazard",
// "Glow", "White", "Red", "Rust") gets that finish.
IRONSIEGE_API void ApplyFinishesBySlot(UStaticMeshComponent* Component);

// The engine basic shape and kit finish material behind a part (shared with the missile itself).
IRONSIEGE_API class UStaticMesh* ShapeMesh(IronKits::Shape Shape);
IRONSIEGE_API class UMaterialInterface* FinishMaterial(IronKits::Finish Finish);
}
