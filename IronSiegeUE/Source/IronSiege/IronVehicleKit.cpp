#include "IronVehicleKit.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"


namespace IronKitBuilder
{
UStaticMesh* ShapeMesh(IronKits::Shape Shape)
{
	static const TCHAR* Paths[] = {
		TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("/Engine/BasicShapes/Sphere.Sphere"),
	};
	return LoadObject<UStaticMesh>(nullptr, Paths[static_cast<int32>(Shape)]);
}

UStaticMesh* RigMesh(const char* Name);

namespace
{
// The kit's own cube and cylinder (chamfered, see Tools/meshgen KitBlock / KitDrum) once they are
// imported; the engine's until then.
UStaticMesh* KitShape(IronKits::Shape Shape)
{
	UStaticMesh* Modelled = Shape == IronKits::Shape::Cube ? RigMesh("SM_Kit_Block") : (Shape == IronKits::Shape::Cylinder ? RigMesh("SM_Kit_Drum") : nullptr);
	return Modelled ? Modelled : ShapeMesh(Shape);
}
}

UMaterialInterface* FinishMaterial(IronKits::Finish Finish)
{
	static const TCHAR* Paths[] = {
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Steel.MI_Kit_Steel"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Rust.MI_Kit_Rust"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Hazard.MI_Kit_Hazard"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Black.MI_Kit_Black"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Glow.MI_Kit_Glow"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_White.MI_Kit_White"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Red.MI_Kit_Red"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Brass.MI_Kit_Brass"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Olive.MI_Kit_Olive"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Edge.MI_Kit_Edge"),
		TEXT("/Game/IronSiege/Vehicles/Kit/MI_Kit_Glass.MI_Kit_Glass"),
	};
	return LoadObject<UMaterialInterface>(nullptr, Paths[static_cast<int32>(Finish)]);
}

namespace
{
// One finish over a kit shape. A modelled block has a second slot, its chamfers, which show the
// worn metal under the paint - except on lamps and glass, which are one material throughout.
void ApplyFinish(UStaticMeshComponent* Component, IronKits::Finish Finish)
{
	UMaterialInterface* Material = FinishMaterial(Finish);
	if (!Material)
	{
		return;
	}
	const bool bPlain = Finish == IronKits::Finish::Glow || Finish == IronKits::Finish::Glass || Finish == IronKits::Finish::Edge;
	UMaterialInterface* Worn = bPlain ? nullptr : FinishMaterial(IronKits::Finish::Edge);
	const int32 EdgeSlot = Component->GetMaterialIndex(TEXT("Edge"));
	for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
	{
		Component->SetMaterial(i, (i == EdgeSlot && Worn) ? Worn : Material);
	}
}
}

FBox LocalBounds(USceneComponent* Chassis)
{
	const FBox Fallback(FVector(-220.f, -100.f, 0.f), FVector(220.f, 100.f, 120.f));
	if (!Chassis)
	{
		return Fallback;
	}
	// The Chaos skeletal mesh is only a rig - its bounds are a couple of centimetres across. The
	// car's shape is the Blueprint's body mesh, so fit to the largest static mesh the Blueprint
	// itself built (not wheels, weapons or the runtime FX pools), expressed in chassis space.
	FBox Best(ForceInit);
	double BestVolume = 0.0;
	TArray<USceneComponent*> Children;
	Chassis->GetChildrenComponents(true, Children);
	const FTransform ChassisToWorld = Chassis->GetComponentTransform();
	for (USceneComponent* Child : Children)
	{
		const UStaticMeshComponent* Visual = Cast<UStaticMeshComponent>(Child);
		if (!Visual || !Visual->GetStaticMesh() || Visual->CreationMethod != EComponentCreationMethod::SimpleConstructionScript)
		{
			continue;
		}
		const FTransform InChassis = Visual->GetComponentTransform().GetRelativeTransform(ChassisToWorld);
		const FBox Box = Visual->CalcBounds(InChassis).GetBox();
		const double Volume = Box.GetVolume();
		if (Volume > BestVolume)
		{
			BestVolume = Volume;
			Best = Box;
		}
	}
	if (BestVolume > 1000.0)
	{
		return Best;
	}
	// Cars whose body is part of the skeletal mesh itself (Vehicle Variety Pack): the mesh's own
	// bounds are the car's shape. A bare template rig is only centimetres across, so size-gate it.
	// Use the mesh asset's render bounds: the component's own bounds come from the simplified
	// physics bodies, which stop well short of a box truck's roof (the rigs ended up inside it).
	const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Chassis);
	const USkinnedAsset* Asset = Skinned ? Skinned->GetSkinnedAsset() : nullptr;
	const FBox SkeletalBox = Asset ? Asset->GetBounds().GetBox() : Chassis->CalcBounds(FTransform::Identity).GetBox();
	return SkeletalBox.IsValid && SkeletalBox.GetVolume() > 100000.0 ? SkeletalBox : Fallback;
}

int32 Build(USceneComponent* Chassis, IronKits::Kit Kit, int32 Variant, TArray<UStaticMeshComponent*>* OutParts)
{
	AActor* Owner = Chassis ? Chassis->GetOwner() : nullptr;
	const IronKits::PartList List = IronKits::Get(Kit);
	if (!Owner || List.Count == 0)
	{
		return 0;
	}
	const FBox Box = LocalBounds(Chassis);
	const FVector Centre = Box.GetCenter();
	const FVector Half = Box.GetExtent();

	int32 Placed = 0;
	auto Place = [&](const IronKits::Part& P)
	{
		UStaticMesh* Mesh = KitShape(P.Form);
		if (!Mesh)
		{
			return;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		ApplyFinish(Component, P.Look);
		// Purely visual: the Chaos body is the skeletal mesh, and a bolt-on with collision would
		// fight it (the template's own visual meshes had to be NoCollision for the same reason).
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(Chassis);
		Component->SetRelativeLocation(Centre + FVector(P.X * Half.X, P.Y * Half.Y, P.Z * Half.Z));
		Component->SetRelativeRotation(FRotator(P.Pitch, P.Yaw, P.Roll));
		Component->SetRelativeScale3D(FVector(P.SizeX, P.SizeY, P.SizeZ) / 100.f);
		Component->RegisterComponent();
		if (OutParts)
		{
			OutParts->Add(Component);
		}
		++Placed;
	};
	for (int32 i = 0; i < List.Count; ++i)
	{
		const IronKits::Part& P = List.Parts[i];
		if (!IronKits::IsActive(P, Variant))
		{
			continue;
		}
		Place(P);
		if (P.Mirror)
		{
			Place(IronKits::Mirrored(P));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: kit %d variant %d on %s - %d parts, body %s"), static_cast<int32>(Kit), Variant, *Owner->GetName(), Placed, *Half.ToString());
	return Placed;
}

UStaticMesh* RigMesh(const char* Name)
{
	if (!Name)
	{
		return nullptr;
	}
	const FString Asset(ANSI_TO_TCHAR(Name));
	// Quiet lookup first: a missing mesh is expected until the import script has been run.
	const FString Path = FString::Printf(TEXT("/Game/IronSiege/Weapons/Rigs/%s.%s"), *Asset, *Asset);
	if (UStaticMesh* Loaded = FindObject<UStaticMesh>(nullptr, *Path))
	{
		return Loaded;
	}
	return FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/IronSiege/Weapons/Rigs/%s"), *Asset)) ? LoadObject<UStaticMesh>(nullptr, *Path) : nullptr;
}

void ApplyFinishesBySlot(UStaticMeshComponent* Component)
{
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	static const TPair<const TCHAR*, IronKits::Finish> Names[] = {
		{ TEXT("Steel"), IronKits::Finish::Steel }, { TEXT("Rust"), IronKits::Finish::Rust }, { TEXT("Hazard"), IronKits::Finish::Hazard },
		{ TEXT("Black"), IronKits::Finish::Black }, { TEXT("Glow"), IronKits::Finish::Glow }, { TEXT("White"), IronKits::Finish::White },
		{ TEXT("Red"), IronKits::Finish::Red }, { TEXT("Brass"), IronKits::Finish::Brass }, { TEXT("Olive"), IronKits::Finish::Olive },
		{ TEXT("Edge"), IronKits::Finish::Edge }, { TEXT("Glass"), IronKits::Finish::Glass },
	};
	const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString Slot = Slots[i].MaterialSlotName.ToString();
		for (const TPair<const TCHAR*, IronKits::Finish>& N : Names)
		{
			if (Slot.Contains(N.Key))
			{
				if (UMaterialInterface* Material = FinishMaterial(N.Value))
				{
					Component->SetMaterial(i, Material);
				}
				break;
			}
		}
	}
}

TArray<UStaticMeshComponent*> BuildExtraWeapons(USceneComponent* Chassis, USceneComponent* Turret, const FVector& RearLocal)
{
	struct FExtra { const char* Mesh; bool bOnTurret; };
	static const FExtra Extras[] = { { "SM_Rig_Railgun", true }, { "SM_Rig_TeslaCoil", true }, { "SM_Rig_Flamer", true }, { "SM_Rig_MineRack", false } };
	TArray<UStaticMeshComponent*> Models;
	AActor* Owner = Chassis ? Chassis->GetOwner() : nullptr;
	for (const FExtra& Extra : Extras)
	{
		USceneComponent* Parent = Extra.bOnTurret ? Turret : Chassis;
		UStaticMesh* Mesh = (Owner && Parent) ? RigMesh(Extra.Mesh) : nullptr;
		if (!Mesh)
		{
			Models.Add(nullptr);
			continue;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		ApplyFinishesBySlot(Component);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(Parent);
		Component->SetRelativeLocation(Extra.bOnTurret ? FVector::ZeroVector : RearLocal);
		Component->SetVisibility(false);
		Component->RegisterComponent();
		Models.Add(Component);
	}
	return Models;
}

FRigBuild BuildRig(USceneComponent* Chassis, const FVector& MountLocal, IronRigs::Rig Rig, TArray<TPair<int32, UStaticMeshComponent*>>* OutMissiles)
{
	FRigBuild Build;
	AActor* Owner = Chassis ? Chassis->GetOwner() : nullptr;
	if (!Owner)
	{
		return Build;
	}
	// The modelled build only if every one of its meshes is there; otherwise the basic shapes.
	const IronRigs::PartList Detailed = IronRigs::Get(Rig, true);
	Build.bDetailed = Detailed.Count > 0;
	for (int32 i = 0; i < Detailed.Count && Build.bDetailed; ++i)
	{
		Build.bDetailed = RigMesh(Detailed.Parts[i].Mesh) != nullptr;
	}
	const IronRigs::PartList List = IronRigs::Get(Rig, Build.bDetailed);
	const IronRigs::Pivots Hinge = IronRigs::GetPivots(Rig, Build.bDetailed);
	const FVector TurretAt(Hinge.TurretX, Hinge.TurretY, Hinge.TurretZ);
	const FVector BarrelAt(Hinge.BarrelX, Hinge.BarrelY, Hinge.BarrelZ);

	auto MakeHinge = [Owner](USceneComponent* Parent, const FVector& Where) -> USceneComponent*
	{
		USceneComponent* Hinge = NewObject<USceneComponent>(Owner);
		Hinge->SetupAttachment(Parent);
		Hinge->SetRelativeLocation(Where);
		Hinge->RegisterComponent();
		return Hinge;
	};
	for (int32 i = 0; i < List.Count; ++i)
	{
		const IronRigs::Part& P = List.Parts[i];
		UStaticMesh* Mesh = P.Mesh ? RigMesh(P.Mesh) : KitShape(P.Form);
		if (!Mesh)
		{
			continue;
		}
		// Moving parts hang off their hinge, placed relative to it.
		USceneComponent* Parent = Chassis;
		FVector Local = MountLocal + FVector(P.X, P.Y, P.Z);
		if (P.Group != IronRigs::Moves::Fixed)
		{
			if (!Build.Turret)
			{
				Build.Turret = MakeHinge(Chassis, MountLocal + TurretAt);
			}
			Parent = Build.Turret;
			Local = FVector(P.X, P.Y, P.Z) - TurretAt;
			if (P.Group == IronRigs::Moves::Barrels)
			{
				if (!Build.Barrels)
				{
					Build.Barrels = MakeHinge(Build.Turret, BarrelAt - TurretAt);
				}
				Parent = Build.Barrels;
				Local = FVector(P.X, P.Y, P.Z) - BarrelAt;
			}
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		if (P.Mesh)
		{
			ApplyFinishesBySlot(Component);
		}
		else
		{
			ApplyFinish(Component, P.Look);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(Parent);
		Component->SetRelativeLocation(Local);
		Component->SetRelativeRotation(FRotator(P.Pitch, P.Yaw, P.Roll));
		Component->SetRelativeScale3D(P.Mesh ? FVector(P.SizeX, P.SizeY, P.SizeZ) : FVector(P.SizeX, P.SizeY, P.SizeZ) / 100.f);
		Component->RegisterComponent();
		if (OutMissiles && P.MissileSlot >= 0)
		{
			OutMissiles->Add(TPair<int32, UStaticMeshComponent*>(P.MissileSlot, Component));
		}
		++Build.Parts;
	}
	return Build;
}
}
