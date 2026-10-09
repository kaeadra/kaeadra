#include "IronArena.h"
#include "ArenaRules.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "IronCityStreet.h"
#include "IronPuffEmitter.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/PackageName.h"

namespace
{
UStaticMesh* Shape(const TCHAR* Name)
{
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
}

UStaticMesh* KitMesh(const TCHAR* Name)
{
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/IronSiege/Environment/CityKitIndustrial/%s.%s"), Name, Name));
}

// A modelled arena prop if it has been imported (import_arena_props.py), else null.
UStaticMesh* PropMesh(const TCHAR* Name)
{
	const FString Package = FString::Printf(TEXT("/Game/IronSiege/Arena/Meshes/%s"), Name);
	return FPackageName::DoesPackageExist(Package) ? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, Name)) : nullptr;
}
}

AIronArena::AIronArena()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only the city's burning wrecks need it.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool AIronArena::ThemeForMap(const FString& MapName, EIronArenaTheme& OutTheme)
{
	if (MapName.Contains(TEXT("Desert"))) { OutTheme = EIronArenaTheme::Desert; return true; }
	if (MapName.Contains(TEXT("Arctic"))) { OutTheme = EIronArenaTheme::Arctic; return true; }
	if (MapName.Contains(TEXT("Coast"))) { OutTheme = EIronArenaTheme::Port; return true; }
	if (MapName.Contains(TEXT("CityRuins"))) { OutTheme = EIronArenaTheme::City; return true; }
	return false;
}

void AIronArena::BeginPlay()
{
	Super::BeginPlay();
	if (Theme == EIronArenaTheme::City)
	{
		BuildCity(); // The street keeps its own ground, buildings and walls.
		ApplyLook();
		return;
	}
	float HalfExtent = 9500.f, GroundZ = -15.f;
	ClearPlaceholders(HalfExtent, GroundZ);
	PlayHalfExtent = HalfExtent;
	Build(HalfExtent, GroundZ);
	ApplyLook();
}

UMaterialInterface* AIronArena::Solid(const FLinearColor& Color, float Roughness, float Metallic)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/M_City_Solid.M_City_Solid"));
	UMaterialInstanceDynamic* Mid = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (Mid)
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		Materials.Add(Mid);
	}
	return Mid;
}

UMaterialInterface* AIronArena::Textured(bool bFacade, const TCHAR* TextureSet, const FLinearColor& Tint, float TileSize, float Desaturation)
{
	// World-projected materials from create_city_materials.py: the ground one maps top-down, the
	// facade one box-projects (so boulders and walls get texture on every side).
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, bFacade
		? TEXT("/Game/IronSiege/City/Materials/M_City_Facade.M_City_Facade")
		: TEXT("/Game/IronSiege/City/Materials/M_City_Ground.M_City_Ground"));
	UMaterialInstanceDynamic* Mid = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (!Mid)
	{
		return nullptr;
	}
	auto Tex = [TextureSet](const TCHAR* Suffix) {
		return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/IronSiege/City/Textures/T_%s_%s.T_%s_%s"), TextureSet, Suffix, TextureSet, Suffix));
	};
	if (UTexture* D = Tex(TEXT("D"))) Mid->SetTextureParameterValue(TEXT("Diffuse"), D);
	if (UTexture* N = Tex(TEXT("N"))) Mid->SetTextureParameterValue(TEXT("Normal"), N);
	if (!bFacade)
	{
		if (UTexture* R = Tex(TEXT("R"))) Mid->SetTextureParameterValue(TEXT("Roughness"), R);
		Mid->SetScalarParameterValue(TEXT("Desaturation"), Desaturation);
	}
	Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
	Mid->SetScalarParameterValue(TEXT("TileSize"), TileSize);
	Materials.Add(Mid);
	return Mid;
}

UMaterialInterface* AIronArena::Worn(const FLinearColor& Color, float Metallic, float Roughness)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/Vehicles/Kit/M_Kit_Worn.M_Kit_Worn"));
	UMaterialInstanceDynamic* Mid = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (!Mid)
	{
		return Solid(Color, Roughness, Metallic);
	}
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	// These are metres across, not a gun's centimetres: larger stains, coarser grain.
	Mid->SetScalarParameterValue(TEXT("WearTiling"), 0.22f);
	Mid->SetScalarParameterValue(TEXT("DetailTiling"), 0.35f);
	// Mostly sound paint: light grime, bare metal only in the deepest scratches (at the kit's
	// settings a tank reads as marble and a container as wicker).
	Mid->SetScalarParameterValue(TEXT("Wear"), 0.55f);
	Mid->SetScalarParameterValue(TEXT("ScratchLevel"), 0.2f);
	Mid->SetScalarParameterValue(TEXT("Detail"), 0.45f);
	Materials.Add(Mid);
	return Mid;
}

void AIronArena::AddProp(UStaticMesh* Mesh, const FTransform& Where, TConstArrayView<FIronPropSlot> Slots)
{
	if (!Mesh)
	{
		return;
	}
	FString Key = FString::Printf(TEXT("prop|%s"), *Mesh->GetName());
	for (const FIronPropSlot& Slot : Slots)
	{
		Key += FString::Printf(TEXT("|%p"), Slot.Material);
	}
	TObjectPtr<UInstancedStaticMeshComponent>* Found = Batches.Find(Key);
	UInstancedStaticMeshComponent* Ism = Found ? Found->Get() : nullptr;
	if (!Ism)
	{
		Ism = NewObject<UInstancedStaticMeshComponent>(this);
		Ism->SetStaticMesh(Mesh);
		const TArray<FStaticMaterial>& MeshSlots = Mesh->GetStaticMaterials();
		for (int32 i = 0; i < MeshSlots.Num(); ++i)
		{
			// A slot nobody named takes the first material given rather than the importer's placeholder,
			// which cannot be drawn instanced.
			UMaterialInterface* Dress = Slots.Num() > 0 ? Slots[0].Material : nullptr;
			for (const FIronPropSlot& Slot : Slots)
			{
				if (Slot.Material && MeshSlots[i].MaterialSlotName.ToString().Contains(Slot.Name))
				{
					Dress = Slot.Material;
					break;
				}
			}
			if (Dress)
			{
				Ism->SetMaterial(i, Dress);
			}
		}
		Ism->SetCollisionProfileName(TEXT("NoCollision"));
		Ism->SetupAttachment(RootComponent);
		Ism->RegisterComponent();
		Batches.Add(Key, Ism);
	}
	Ism->AddInstance(Where, true);
}

UInstancedStaticMeshComponent* AIronArena::Batch(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide, bool bUnseen)
{
	if (!Mesh)
	{
		return nullptr;
	}
	const FString Key = FString::Printf(TEXT("%s|%p|%d|%d"), *Mesh->GetName(), Material, bCollide ? 1 : 0, bUnseen ? 1 : 0);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Batches.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
	Ism->SetStaticMesh(Mesh);
	if (Material)
	{
		for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i)
		{
			Ism->SetMaterial(i, Material);
		}
	}
	Ism->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	Ism->SetVisibility(!bUnseen);
	Ism->SetupAttachment(RootComponent);
	Ism->RegisterComponent();
	Batches.Add(Key, Ism);
	return Ism;
}

void AIronArena::AddShape(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Centre, const FRotator& Rotation, const FVector& SizeCm, bool bCollide, bool bUnseen)
{
	if (bUnseen && !bCollide)
	{
		return; // Neither seen nor felt.
	}
	if (UInstancedStaticMeshComponent* Ism = Batch(Mesh, Material, bCollide, bUnseen))
	{
		// Engine basic shapes are 100 cm across; kit meshes are scaled by the caller.
		Ism->AddInstance(FTransform(Rotation, Centre, SizeCm / 100.f), true);
	}
}

void AIronArena::AddBox(UMaterialInterface* Material, const FVector& Centre, const FRotator& Rotation, const FVector& SizeCm, bool bCollide, bool bUnseen)
{
	AddShape(Shape(TEXT("Cube")), Material, Centre, Rotation, SizeCm, bCollide, bUnseen);
}

void AIronArena::ClearPlaceholders(float& OutHalfExtent, float& OutGroundZ)
{
	// The greybox builder's placeholder obstacles are plain engine cubes of roughly equal sides; the
	// ground slab and the four walls are the long ones. Re-skin those, remove the rest.
	UStaticMesh* Cube = Shape(TEXT("Cube"));
	AStaticMeshActor* Ground = nullptr;
	TArray<AStaticMeshActor*> Walls;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (!Mesh || Mesh->GetStaticMesh() != Cube)
		{
			continue;
		}
		const FVector S = It->GetActorScale3D();
		const float Longest = S.GetMax(), Shortest = S.GetMin();
		if (Longest > 50.f && S.Z < 1.f)
		{
			Ground = *It;
		}
		else if (Longest > 50.f)
		{
			Walls.Add(*It);
			WallBoxes.Add(It->GetComponentsBoundingBox());
		}
		else if (Longest / FMath::Max(Shortest, 0.01f) < 1.3f && Longest < 10.f)
		{
			It->Destroy();
		}
	}
	if (Ground)
	{
		FVector Origin, Extent;
		Ground->GetActorBounds(false, Origin, Extent);
		OutHalfExtent = FMath::Min(Extent.X, Extent.Y);
		OutGroundZ = Origin.Z + Extent.Z;
	}

	// Ground and wall skins per theme.
	UMaterialInterface* GroundSkin = nullptr;
	UMaterialInterface* WallSkin = nullptr;
	switch (Theme)
	{
	case EIronArenaTheme::Desert:
		GroundSkin = Textured(false, TEXT("Sand"), FLinearColor(1.f, 1.f, 1.f), 500.f, 0.f);
		WallSkin = Textured(true, TEXT("Rock"), FLinearColor(0.95f, 0.62f, 0.4f), 600.f, 0.f);
		break;
	case EIronArenaTheme::Arctic:
		GroundSkin = Textured(false, TEXT("Snow"), FLinearColor(1.f, 1.f, 1.f), 600.f, 0.f);
		WallSkin = Textured(true, TEXT("Rock"), FLinearColor(0.62f, 0.68f, 0.76f), 600.f, 0.f);
		break;
	case EIronArenaTheme::Port:
		GroundSkin = Textured(false, TEXT("Asphalt"), FLinearColor(0.9f, 0.9f, 0.92f), 450.f, 0.6f);
		WallSkin = Textured(true, TEXT("Concrete"), FLinearColor(0.6f, 0.6f, 0.62f), 400.f, 0.f);
		break;
	}
	if (Ground && GroundSkin)
	{
		Ground->GetStaticMeshComponent()->SetMaterial(0, GroundSkin);
	}
	for (AStaticMeshActor* Wall : Walls)
	{
		if (WallSkin)
		{
			Wall->GetStaticMeshComponent()->SetMaterial(0, WallSkin);
		}
	}
}

void AIronArena::Build(float HalfExtent, float GroundZ)
{
	IronArena::Tuning Tuning;
	Tuning.HalfExtent = HalfExtent;
	Tuning.StartOffset = HalfExtent * 0.6f; // Where the greybox builder put the four player starts.
	const IronArena::Layout Layout = IronArena::Generate(static_cast<IronArena::Theme>(Theme), static_cast<unsigned>(Seed), Tuning);
	BuildLayout(Layout, GroundZ);
	BuildCliffs(GroundZ);
	// The port's skyline: two ship-to-shore cranes on the quay beyond the far wall, booms out to sea.
	if (UStaticMesh* Crane = (Theme == EIronArenaTheme::Port ? PropMesh(TEXT("SM_Prop_GantryCrane")) : nullptr))
	{
		UMaterialInterface* CranePaint = Worn(FLinearColor(0.42f, 0.1f, 0.04f), 0.3f, 0.55f);
		UMaterialInterface* CraneSteel = Worn(FLinearColor(0.06f, 0.065f, 0.07f), 0.8f, 0.45f);
		UMaterialInterface* CraneCab = Worn(FLinearColor(0.62f, 0.38f, 0.03f), 0.1f, 0.55f);
		UMaterialInterface* CraneGlass = Solid(FLinearColor(0.02f, 0.03f, 0.04f), 0.1f, 0.6f);
		for (float X : { -0.42f, 0.36f })
		{
			AddProp(Crane, FTransform(FRotator(0.f, 90.f, 0.f), FVector(X * HalfExtent, HalfExtent + 1500.f, GroundZ)),
				{ { TEXT("Paint"), CranePaint }, { TEXT("Steel"), CraneSteel }, { TEXT("Hazard"), CraneCab }, { TEXT("Dark"), CraneGlass } });
		}
	}
	// Containers placed in the map by hand are the art kit's toy ones: draw the modelled container
	// over each (same place, stretched to the same size) and keep the old one only as its collision.
	if (UStaticMesh* Box = PropMesh(TEXT("SM_Container")))
	{
		UMaterialInterface* Paint = Worn(FLinearColor(0.3f, 0.09f, 0.05f), 0.3f, 0.6f);
		UMaterialInterface* Steel = Worn(FLinearColor(0.05f, 0.055f, 0.06f), 0.8f, 0.45f);
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			UStaticMeshComponent* Old = It->GetStaticMeshComponent();
			UStaticMesh* Mesh = Old ? Old->GetStaticMesh() : nullptr;
			if (!Mesh || !Mesh->GetName().Contains(TEXT("shipping_container")))
			{
				continue;
			}
			const FBoxSphereBounds B = Mesh->GetBounds();
			const bool bLongY = B.BoxExtent.Y > B.BoxExtent.X;
			const FVector Size = 2.f * B.BoxExtent * It->GetActorScale3D();
			const FVector Foot = It->GetActorTransform().TransformPosition(FVector(B.Origin.X, B.Origin.Y, B.Origin.Z - B.BoxExtent.Z));
			const FRotator Turn = It->GetActorRotation() + FRotator(0.f, bLongY ? 90.f : 0.f, 0.f);
			AddProp(Box, FTransform(Turn, Foot, FVector((bLongY ? Size.Y : Size.X) / 1219.f, (bLongY ? Size.X : Size.Y) / 244.f, Size.Z / 259.f)),
				{ { TEXT("Paint"), Paint }, { TEXT("Steel"), Steel }, { TEXT("Edge"), Steel } });
			Old->SetVisibility(false);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: arena theme %d built - %d props, %d mesh batches, half extent %.0f"), static_cast<int32>(Theme), Layout.Count, Batches.Num(), HalfExtent);
}

void AIronArena::BuildCity()
{
	AIronCityStreet* Street = nullptr;
	for (TActorIterator<AIronCityStreet> It(GetWorld()); It; ++It)
	{
		Street = *It;
		break;
	}
	if (!Street)
	{
		UE_LOG(LogTemp, Warning, TEXT("IronSiege: city arena found no AIronCityStreet"));
		return;
	}
	TArray<IronArena::Road> Roads;
	for (const FIronRoadSegment& Seg : Street->GetRoadSegments())
	{
		Roads.Add({ float(Seg.A.X), float(Seg.A.Y), float(Seg.B.X), float(Seg.B.Y), Seg.Width });
	}
	TArray<float> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(It->GetActorLocation().X);
		Starts.Add(It->GetActorLocation().Y);
	}
	const IronArena::Layout Layout = IronArena::GenerateCity(Roads.GetData(), Roads.Num(), Starts.GetData(), Starts.Num() / 2, static_cast<unsigned>(Seed));
	BuildLayout(Layout, Street->GetActorLocation().Z + Street->RoadZ);
	SetActorTickEnabled(Fires.Num() > 0);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: city arena built - %d props on %d roads, %d fires, %d mesh batches"), Layout.Count, Roads.Num(), Fires.Num(), Batches.Num());
}

void AIronArena::BuildLayout(const IronArena::Layout& Layout, float GroundZ)
{
	for (int32 i = 0; i < Layout.Count; ++i)
	{
		const IronArena::Prop& P = Layout.Props[i];
		if (P.Kind != IronArena::PropKind::LakeIce) // The lake is a flat decal, drivable.
		{
			Obstacles.Add(FVector(P.X, P.Y, IronArena::Radius(P)));
		}
	}

	const bool bDesert = Theme == EIronArenaTheme::Desert, bArctic = Theme == EIronArenaTheme::Arctic;
	UMaterialInterface* RockSkin = bDesert ? Textured(true, TEXT("Rock"), FLinearColor(0.95f, 0.58f, 0.36f), 300.f, 0.f)
		: Textured(true, TEXT("Rock"), FLinearColor(0.6f, 0.65f, 0.72f), 300.f, 0.f);
	UMaterialInterface* MesaSkin = Textured(true, TEXT("Rock"), FLinearColor(1.f, 0.64f, 0.4f), 420.f, 0.f);
	UMaterialInterface* RampSteel = Solid(FLinearColor(0.05f, 0.055f, 0.06f), 0.45f, 0.7f);
	UMaterialInterface* Hazard = Solid(FLinearColor(0.9f, 0.62f, 0.02f), 0.5f);
	UMaterialInterface* TankPaint = Solid(bArctic ? FLinearColor(0.75f, 0.12f, 0.08f) : FLinearColor(0.72f, 0.72f, 0.7f), 0.35f, 0.6f);
	UMaterialInterface* Concrete = Textured(true, TEXT("Concrete"), FLinearColor(0.85f, 0.85f, 0.83f), 200.f, 0.f);
	UMaterialInterface* Ice = Solid(FLinearColor(0.55f, 0.78f, 0.95f), 0.06f, 0.f);
	UMaterialInterface* LakeIce = Solid(FLinearColor(0.35f, 0.55f, 0.72f), 0.03f, 0.f);
	UStaticMesh* Cylinder = Shape(TEXT("Cylinder"));
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	UStaticMesh* Cone = Shape(TEXT("Cone"));
	UStaticMesh* ContainerA = KitMesh(TEXT("SM_shipping_container_a"));
	UStaticMesh* ContainerB = KitMesh(TEXT("SM_shipping_container_b"));
	UStaticMesh* WaterTower = KitMesh(TEXT("SM_water_tower"));
	UStaticMesh* WreckBodies[] = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body")),
	};
	UStaticMesh* WreckWheel = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
	UMaterialInterface* Charred = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_City_FrameDark.MI_City_FrameDark"));
	UMaterialInterface* Burlap = Solid(FLinearColor(0.42f, 0.36f, 0.24f), 0.95f);
	UMaterialInterface* RustSteel = Solid(FLinearColor(0.16f, 0.09f, 0.05f), 0.7f, 0.5f);
	UMaterialInterface* Debris = Textured(true, TEXT("Concrete"), FLinearColor(0.62f, 0.6f, 0.57f), 180.f, 0.f);
	UMaterialInterface* Scorch = Solid(FLinearColor(0.012f, 0.011f, 0.01f), 0.95f);
	UMaterialInterface* SmokeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft"));
	IronArena::Rng Jitter(static_cast<unsigned>(Seed) * 31u + 5u);

	// The modelled props. Where one is there, the simple shape under it stays as its collision,
	// hidden; where it is not, the shape is drawn as before.
	UStaticMesh* RockMeshes[] = { PropMesh(TEXT("SM_Rock_A")), PropMesh(TEXT("SM_Rock_B")), PropMesh(TEXT("SM_Rock_C")) };
	UStaticMesh* RubbleMeshes[] = { PropMesh(TEXT("SM_Rubble_A")), PropMesh(TEXT("SM_Rubble_B")) };
	UStaticMesh* SpireMeshes[] = { PropMesh(TEXT("SM_IceSpire_A")), PropMesh(TEXT("SM_IceSpire_B")) };
	UStaticMesh* MesaMesh = PropMesh(TEXT("SM_Mesa"));
	UStaticMesh* TankMesh = PropMesh(TEXT("SM_FuelTank"));
	UStaticMesh* BarrierMesh = PropMesh(TEXT("SM_Barrier"));
	UStaticMesh* RampPlateau = PropMesh(TEXT("SM_Ramp_Plateau"));
	UStaticMesh* RampKicker = PropMesh(TEXT("SM_Ramp_Kicker"));
	UStaticMesh* SandbagMesh = PropMesh(TEXT("SM_Sandbag"));
	UStaticMesh* TrapMesh = PropMesh(TEXT("SM_TankTrap"));
	UStaticMesh* ContainerMesh = PropMesh(TEXT("SM_Container"));
	const bool bRocks = RockMeshes[0] && RockMeshes[1] && RockMeshes[2];
	const bool bRubble = RubbleMeshes[0] && RubbleMeshes[1];
	const bool bSpires = SpireMeshes[0] && SpireMeshes[1];
	UMaterialInterface* SteelWorn = Worn(FLinearColor(0.05f, 0.055f, 0.06f), 0.8f, 0.45f);
	UMaterialInterface* DeckWorn = Worn(FLinearColor(0.035f, 0.04f, 0.045f), 0.7f, 0.55f);
	UMaterialInterface* EdgeWorn = Worn(FLinearColor(0.22f, 0.22f, 0.21f), 1.f, 0.4f);
	UMaterialInterface* HazardWorn = Worn(FLinearColor(0.62f, 0.38f, 0.03f), 0.1f, 0.55f);
	UMaterialInterface* TankWorn = Worn(bArctic ? FLinearColor(0.42f, 0.07f, 0.05f) : FLinearColor(0.55f, 0.55f, 0.52f), 0.3f, 0.45f);
	UMaterialInterface* RustWorn = Worn(FLinearColor(0.17f, 0.085f, 0.045f), 0.45f, 0.8f);
	UMaterialInterface* DarkGap = Solid(FLinearColor(0.012f, 0.012f, 0.012f), 0.9f);
	UMaterialInterface* ContainerPaints[] = {
		Worn(FLinearColor(0.32f, 0.07f, 0.04f), 0.3f, 0.6f),   // Oxide red.
		Worn(FLinearColor(0.04f, 0.12f, 0.28f), 0.3f, 0.55f),  // Blue.
		Worn(FLinearColor(0.07f, 0.2f, 0.12f), 0.3f, 0.6f),    // Green.
		Worn(FLinearColor(0.45f, 0.25f, 0.04f), 0.3f, 0.6f),   // Orange.
		Worn(FLinearColor(0.38f, 0.38f, 0.36f), 0.3f, 0.55f),  // Grey.
	};
	// A boulder standing in for a box of this size: the mesh is 2 m across, and a little over the
	// box so its corners do not poke through thin air.
	auto Boulder = [&](int32 Which, const FVector& Centre, const FRotator& Turn, const FVector& BoxSize)
	{
		AddProp(RockMeshes[Which % 3], FTransform(Turn, Centre, BoxSize / 200.f * 1.3f), { { TEXT("Rock"), RockSkin } });
	};

	// Kit meshes come in at whatever size they were modelled; scale them to real dimensions.
	auto FitTo = [](UStaticMesh* Mesh, float TargetLongCm) {
		const FVector Ext = Mesh ? Mesh->GetBounds().BoxExtent : FVector(50.f);
		return TargetLongCm / FMath::Max(2.f * FMath::Max(Ext.X, Ext.Y), 1.f);
	};

	for (int32 i = 0; i < Layout.Count; ++i)
	{
		const IronArena::Prop& P = Layout.Props[i];
		const FVector Base(P.X, P.Y, GroundZ);
		const FRotator Facing(0.f, P.Yaw, 0.f);
		switch (P.Kind)
		{
		case IronArena::PropKind::Ramp:
		{
			// A steel deck on struts with hazard-striped edges. Plateau ramps (Level 1) are long and
			// steep enough to reach the top; field jumps are short kickers.
			const bool bPlateau = P.Level == 1;
			const float Length = bPlateau ? 1150.f : 650.f * P.Scale;
			const float Pitch = bPlateau ? 24.f : 16.f;
			const float Rise = Length * FMath::Sin(FMath::DegreesToRadians(Pitch));
			const FVector Along = Facing.Vector();
			const FVector Centre = Base + FVector(0.f, 0.f, Rise * 0.5f + 10.f);
			const FRotator Deck(Pitch, P.Yaw, 0.f);
			UStaticMesh* RampMesh = bPlateau ? RampPlateau : RampKicker;
			const bool bModelled = RampMesh != nullptr;
			AddBox(RampSteel, Centre, Deck, FVector(Length, 420.f, 30.f), true, bModelled);
			const FVector Side = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
			for (int32 s = -1; s <= 1; s += 2)
			{
				AddBox(Hazard, Centre + Side * (s * 200.f) + FVector(0.f, 0.f, 18.f), Deck, FVector(Length, 18.f, 14.f), false, bModelled);
			}
			if (bModelled)
			{
				// Modelled at stock length; a longer kicker is stretched along its run and rise only.
				const float Stretch = bPlateau ? 1.f : P.Scale;
				AddProp(RampMesh, FTransform(Facing, Base, FVector(Stretch, 1.f, Stretch)),
					{ { TEXT("Deck"), DeckWorn }, { TEXT("Edge"), EdgeWorn }, { TEXT("Steel"), SteelWorn }, { TEXT("Hazard"), HazardWorn } });
			}
			// Struts under the high end so it reads as a built ramp, not a floating slab.
			const float HalfRun = Length * 0.5f * FMath::Cos(FMath::DegreesToRadians(Pitch));
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Foot = Base + Along * (HalfRun - 40.f) + Side * (s * 170.f);
				AddBox(RampSteel, Foot + FVector(0.f, 0.f, Rise * 0.5f), Facing, FVector(30.f, 30.f, Rise), true, bModelled);
			}
			break;
		}
		case IronArena::PropKind::Rock:
		{
			// A cluster of tilted, half-buried blocks - angular faces read as broken rock, where
			// scaled spheres looked like dough.
			const float S = 260.f * P.Scale;
			const FRotator Lean(Jitter.Range(-25.f, 25.f), P.Yaw, Jitter.Range(-25.f, 25.f));
			const FVector Big(S * 1.8f, S * 1.3f, S * 1.2f);
			AddBox(RockSkin, Base + FVector(0.f, 0.f, S * 0.35f), Lean, Big, true, bRocks);
			if (bRocks)
			{
				Boulder(i, Base + FVector(0.f, 0.f, S * 0.35f), Lean, Big);
			}
			for (int32 k = 0; k < 3; ++k)
			{
				const FVector Off = FRotator(0.f, P.Yaw + 110.f * (k + 1) + Jitter.Range(-20.f, 20.f), 0.f).Vector() * S * 1.f;
				const float Size = S * Jitter.Range(0.45f, 0.8f);
				const FRotator Tumble(Jitter.Range(-35.f, 35.f), Jitter.Range(0.f, 360.f), Jitter.Range(-35.f, 35.f));
				const FVector Small(Size, Size * 0.85f, Size * 0.75f);
				AddBox(RockSkin, Base + Off + FVector(0.f, 0.f, Size * 0.2f), Tumble, Small, true, bRocks);
				if (bRocks)
				{
					Boulder(i + k + 1, Base + Off + FVector(0.f, 0.f, Size * 0.2f), Tumble, Small);
				}
			}
			break;
		}
		case IronArena::PropKind::Mesa:
		{
			// Flat-topped plateau, 4.5 m high, with a stepped rim of boulders.
			const float R = 1800.f * P.Scale;
			AddShape(Cylinder, MesaSkin, Base + FVector(0.f, 0.f, 225.f), FRotator::ZeroRotator, FVector(R * 2.f, R * 2.f, 450.f), true, MesaMesh != nullptr);
			if (MesaMesh)
			{
				// Modelled with a top of radius 10 m at the collision cylinder's height.
				AddProp(MesaMesh, FTransform(Facing, Base, FVector(R / 1000.f, R / 1000.f, 1.f)), { { TEXT("Rock"), MesaSkin } });
			}
			for (int32 k = 0; k < 10; ++k)
			{
				if (k % 5 == 1) continue; // Leave the ramp approaches open.
				const float A = FMath::DegreesToRadians(36.f * k + 18.f);
				const FVector At = Base + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 110.f);
				const FRotator Lean(Jitter.Range(-18.f, 18.f), 36.f * k, Jitter.Range(-18.f, 18.f));
				AddBox(RockSkin, At, Lean, FVector(420.f, 330.f, 280.f), true, bRocks);
				if (bRocks)
				{
					Boulder(i + k, At, Lean, FVector(420.f, 330.f, 280.f));
				}
			}
			break;
		}
		case IronArena::PropKind::Container:
		{
			UStaticMesh* Mesh = (Jitter.Unit() < 0.5f) ? ContainerA : ContainerB;
			if (ContainerMesh)
			{
				// A real 40 ft box in worn paint, one of five liveries; a hidden block is its collision.
				const FVector Size(1219.f, 244.f, 259.f);
				AddBox(nullptr, Base + FVector(0.f, 0.f, (P.Level + 0.5f) * Size.Z), Facing, Size, true, true);
				AddProp(ContainerMesh, FTransform(Facing, Base + FVector(0.f, 0.f, P.Level * Size.Z)),
					{ { TEXT("Paint"), ContainerPaints[(i * 7 + P.Level * 3) % 5] }, { TEXT("Steel"), SteelWorn }, { TEXT("Edge"), EdgeWorn }, { TEXT("Dark"), DarkGap } });
				break;
			}
			if (!Mesh)
			{
				AddBox(Solid(FLinearColor(0.5f, 0.12f, 0.06f), 0.6f), Base + FVector(0.f, 0.f, 130.f + P.Level * 260.f), Facing, FVector(610.f, 244.f, 260.f));
				break;
			}
			const float Scale = FitTo(Mesh, 1220.f); // A 40 ft container.
			const FBoxSphereBounds B = Mesh->GetBounds();
			const float Height = 2.f * B.BoxExtent.Z * Scale;
			const float Bottom = (B.Origin.Z - B.BoxExtent.Z) * Scale;
			if (UInstancedStaticMeshComponent* Ism = Batch(Mesh, nullptr))
			{
				Ism->AddInstance(FTransform(Facing, Base + FVector(0.f, 0.f, P.Level * Height - Bottom), FVector(Scale)), true);
			}
			break;
		}
		case IronArena::PropKind::Tank:
		{
			// Upright fuel tank with a domed cap and a hazard band.
			const float R = 220.f * P.Scale, H = 650.f * P.Scale;
			const bool bModelled = TankMesh != nullptr;
			AddShape(Cylinder, TankPaint, Base + FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, FVector(R * 2.f, R * 2.f, H), true, bModelled);
			AddShape(Sphere, TankPaint, Base + FVector(0.f, 0.f, H), FRotator::ZeroRotator, FVector(R * 2.f, R * 2.f, R * 0.9f), true, bModelled);
			AddShape(Cylinder, Hazard, Base + FVector(0.f, 0.f, H * 0.3f), FRotator::ZeroRotator, FVector(R * 2.04f, R * 2.04f, 40.f), false, bModelled);
			if (bModelled)
			{
				AddProp(TankMesh, FTransform(Facing, Base, FVector(P.Scale)),
					{ { TEXT("Paint"), TankWorn }, { TEXT("Steel"), SteelWorn }, { TEXT("Edge"), EdgeWorn }, { TEXT("Hazard"), HazardWorn } });
			}
			break;
		}
		case IronArena::PropKind::Barrier:
		{
			AddBox(Concrete, Base + FVector(0.f, 0.f, 55.f), Facing, FVector(360.f, 70.f, 110.f), true, BarrierMesh != nullptr);
			AddBox(Hazard, Base + FVector(0.f, 0.f, 90.f), Facing, FVector(362.f, 72.f, 14.f), false, BarrierMesh != nullptr);
			if (BarrierMesh)
			{
				AddProp(BarrierMesh, FTransform(Facing, Base), { { TEXT("Concrete"), Concrete }, { TEXT("Hazard"), HazardWorn }, { TEXT("Dark"), DarkGap } });
			}
			break;
		}
		case IronArena::PropKind::IceSpire:
		{
			const float H = 950.f * P.Scale, R = 170.f * P.Scale;
			AddShape(Cone, Ice, Base + FVector(0.f, 0.f, H * 0.45f), FRotator(Jitter.Range(-12.f, 12.f), P.Yaw, Jitter.Range(-12.f, 12.f)), FVector(R * 2.f, R * 1.6f, H), true, bSpires);
			if (bSpires)
			{
				AddProp(SpireMeshes[i % 2], FTransform(Facing, Base, FVector(P.Scale)), { { TEXT("Ice"), Ice } });
			}
			break;
		}
		case IronArena::PropKind::Tower:
		{
			if (UStaticMesh* Tower = PropMesh(TEXT("SM_Prop_WaterTower")))
			{
				// The modelled tower, with a hidden column for a car to hit.
				AddShape(Cylinder, nullptr, Base + FVector(0.f, 0.f, 700.f), FRotator::ZeroRotator, FVector(560.f, 560.f, 1400.f), true, true);
				AddProp(Tower, FTransform(Facing, Base), { { TEXT("Paint"), TankWorn }, { TEXT("Steel"), SteelWorn }, { TEXT("Edge"), EdgeWorn }, { TEXT("Hazard"), HazardWorn } });
				break;
			}
			if (WaterTower)
			{
				const FBoxSphereBounds B = WaterTower->GetBounds();
				const float Scale = 1400.f / FMath::Max(2.f * B.BoxExtent.Z, 1.f);
				if (UInstancedStaticMeshComponent* Ism = Batch(WaterTower, nullptr))
				{
					Ism->AddInstance(FTransform(Facing, Base + FVector(0.f, 0.f, -(B.Origin.Z - B.BoxExtent.Z) * Scale), FVector(Scale)), true);
				}
			}
			break;
		}
		case IronArena::PropKind::Wreck:
		{
			// A burnt-out car shell on the rims (tyres gone), some still burning.
			UStaticMesh* Body = WreckBodies[Jitter.Unit() < 0.6f ? 0 : 1];
			if (Body)
			{
				const FRotator Tilt(Jitter.Range(-4.f, 4.f), P.Yaw, Jitter.Range(-6.f, 6.f));
				if (UInstancedStaticMeshComponent* Ism = Batch(Body, Charred))
				{
					Ism->AddInstance(FTransform(Tilt, Base + FVector(0.f, 0.f, 18.f)), true);
				}
				const FVector Right = FRotationMatrix(FRotator(0.f, P.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
				const FVector Fwd = FRotator(0.f, P.Yaw, 0.f).Vector();
				for (int32 w = 0; w < 4; ++w)
				{
					if (WreckWheel && Jitter.Unit() < 0.7f)
					{
						const FVector At = Base + Fwd * ((w < 2) ? 140.f : -130.f) + Right * ((w % 2) ? 85.f : -85.f) + FVector(0.f, 0.f, 25.f);
						AddShape(WreckWheel, Charred, At, FRotator(0.f, P.Yaw, 0.f), FVector(100.f), false);
					}
				}
			}
			// Scorched tarmac under every wreck.
			AddShape(Shape(TEXT("Cylinder")), Scorch, Base + FVector(0.f, 0.f, 2.f), FRotator(0.f, P.Yaw, 0.f), FVector(560.f, 420.f, 2.f), false);
			if (P.Level == 1 && SmokeMat)
			{
				UIronPuffEmitter* Fire = NewObject<UIronPuffEmitter>(this);
				Fire->SetupAttachment(RootComponent);
				Fire->RegisterComponent();
				Fire->Init(SmokeMat, 26);
				Fires.Add(Fire);
				FireSpots.Add(Base + FVector(0.f, 0.f, 90.f));
			}
			break;
		}
		case IronArena::PropKind::Sandbags:
		{
			// Two staggered rows of bags plus a top row - a waist-high firing position.
			const FVector Along = FRotator(0.f, P.Yaw, 0.f).Vector();
			for (int32 Row = 0; Row < 3; ++Row)
			{
				const int32 Bags = Row < 2 ? 6 : 5;
				for (int32 b = 0; b < Bags; ++b)
				{
					const float X = (b - (Bags - 1) * 0.5f) * 68.f + (Row == 1 ? 34.f : 0.f);
					const FVector At = Base + Along * X + FVector(0.f, 0.f, 18.f + Row * 30.f);
					const FRotator Lie(0.f, P.Yaw + Jitter.Range(-6.f, 6.f), 0.f);
					AddShape(Sphere, Burlap, At, Lie, FVector(72.f, 44.f, 32.f), Row == 0, SandbagMesh != nullptr);
					if (SandbagMesh)
					{
						AddProp(SandbagMesh, FTransform(Lie, At), { { TEXT("Burlap"), Burlap } });
					}
				}
			}
			break;
		}
		case IronArena::PropKind::TankTrap:
		{
			// Czech hedgehog: three steel beams crossed at right angles.
			const FVector Centre = Base + FVector(0.f, 0.f, 75.f);
			AddBox(RustSteel, Centre, FRotator(45.f, P.Yaw, 0.f), FVector(220.f, 22.f, 22.f), true, TrapMesh != nullptr);
			AddBox(RustSteel, Centre, FRotator(45.f, P.Yaw + 90.f, 0.f), FVector(220.f, 22.f, 22.f), true, TrapMesh != nullptr);
			AddBox(RustSteel, Centre, FRotator(-45.f, P.Yaw + 45.f, 0.f), FVector(220.f, 22.f, 22.f), true, TrapMesh != nullptr);
			if (TrapMesh)
			{
				AddProp(TrapMesh, FTransform(Facing, Base), { { TEXT("Rust"), RustWorn }, { TEXT("Edge"), EdgeWorn } });
			}
			break;
		}
		case IronArena::PropKind::Rubble:
		{
			const float S = 150.f * P.Scale;
			AddShape(Shape(TEXT("Cylinder")), Scorch, Base + FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator, FVector(S * 3.f, S * 3.f, 2.f), false);
			for (int32 k = 0; k < 5; ++k)
			{
				const FVector Off = FRotator(0.f, P.Yaw + 72.f * k, 0.f).Vector() * S * Jitter.Range(0.f, 0.9f);
				const float Size = S * Jitter.Range(0.5f, 1.1f);
				const FRotator Tumble(Jitter.Range(-40.f, 40.f), Jitter.Range(0.f, 360.f), Jitter.Range(-40.f, 40.f));
				const FVector Chunk(Size, Size * 0.7f, Size * 0.5f);
				AddBox(Debris, Base + Off + FVector(0.f, 0.f, Size * 0.2f), Tumble, Chunk, k < 2, bRubble);
				if (bRubble)
				{
					AddProp(RubbleMeshes[(i + k) % 2], FTransform(Tumble, Base + Off + FVector(0.f, 0.f, Size * 0.2f), Chunk / 200.f * 1.3f),
						{ { TEXT("Concrete"), Debris }, { TEXT("Steel"), RustWorn } });
				}
			}
			break;
		}
		case IronArena::PropKind::LakeIce:
		{
			// Glassy frozen lake: a thin disc just above the ground; cars drive over the ground under it.
			const float R = 2600.f * P.Scale;
			AddShape(Cylinder, LakeIce, Base + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector(R * 2.f, R * 2.f, 4.f), false);
			break;
		}
		default:
			break;
		}
	}
}

void AIronArena::BuildCliffs(float GroundZ)
{
	// Desert and arctic: the perimeter walls become rock faces - cliff sections stood side by side
	// along each wall's inner face, in front of the flat slab (which stays as the collision).
	if (Theme != EIronArenaTheme::Desert && Theme != EIronArenaTheme::Arctic)
	{
		return;
	}
	UStaticMesh* Cliffs[] = { PropMesh(TEXT("SM_Cliff_A")), PropMesh(TEXT("SM_Cliff_B")) };
	if (!Cliffs[0] || !Cliffs[1])
	{
		return;
	}
	UMaterialInterface* Skin = Theme == EIronArenaTheme::Desert
		? Textured(true, TEXT("Rock"), FLinearColor(0.95f, 0.6f, 0.38f), 520.f, 0.f)
		: Textured(true, TEXT("Rock"), FLinearColor(0.6f, 0.66f, 0.74f), 520.f, 0.f);
	IronArena::Rng Jitter(static_cast<unsigned>(Seed) * 53u + 11u);
	int32 Count = 0;
	for (const FBox& Wall : WallBoxes)
	{
		const FVector Centre = Wall.GetCenter(), Extent = Wall.GetExtent();
		const bool bAlongX = Extent.X > Extent.Y;
		// The cliff model faces -Y; turn it to face the middle of the arena.
		const float Inward = bAlongX ? (Centre.Y > 0.f ? -1.f : 1.f) : (Centre.X > 0.f ? -1.f : 1.f);
		const float Yaw = bAlongX ? (Inward < 0.f ? 0.f : 180.f) : (Inward < 0.f ? -90.f : 90.f);
		const float Face = (bAlongX ? Centre.Y : Centre.X) + Inward * (bAlongX ? Extent.Y : Extent.X);
		const float Half = bAlongX ? Extent.X : Extent.Y;
		const float Mid = bAlongX ? Centre.X : Centre.Y;
		for (float Along = -Half + 560.f; Along < Half; Along += 1140.f)
		{
			// Set back so the rock's bulges, not its hollows, meet the wall's face.
			const float Across = Face - Inward * 90.f;
			const FVector At = bAlongX ? FVector(Mid + Along, Across, GroundZ) : FVector(Across, Mid + Along, GroundZ);
			AddProp(Cliffs[Count % 2], FTransform(FRotator(0.f, Yaw, 0.f), At, FVector(1.f, 1.f, Jitter.Range(0.85f, 1.25f))), { { TEXT("Rock"), Skin } });
			++Count;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: %d cliff sections along %d walls"), Count, WallBoxes.Num());
}

void AIronArena::ApplyLook()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Sun: angle, strength and colour temperature per theme. The maps use plain (non-physical) sun
	// intensity around 4 with histogram auto-exposure (simplify_lighting.py / improve_lighting.py),
	// so the values stay in that range.
	struct FLook
	{
		FRotator Sun;
		float SunLux, SunTemp;
		float FogDensity, FogFalloff;
		FLinearColor FogColor;
		float Saturation, Contrast, WhiteTemp, Exposure, Bloom;
	};
	FLook Look{};
	switch (Theme)
	{
	case EIronArenaTheme::Desert: // High, hot afternoon sun; dusty warm haze.
		Look = { FRotator(-48.f, -35.f, 0.f), 5.2f, 5600.f, 0.012f, 0.18f, FLinearColor(0.85f, 0.62f, 0.4f), 1.12f, 1.08f, 6200.f, 0.6f, 0.55f };
		break;
	case EIronArenaTheme::Arctic: // Low cold sun, blue mist.
		Look = { FRotator(-18.f, 25.f, 0.f), 4.f, 8200.f, 0.028f, 0.12f, FLinearColor(0.62f, 0.72f, 0.88f), 0.92f, 1.05f, 7200.f, 0.7f, 0.7f };
		break;
	case EIronArenaTheme::City: // Late afternoon through the smoke of a burning city: low amber sun, brown haze, muted colour.
		Look = { FRotator(-22.f, 60.f, 0.f), 4.2f, 4200.f, 0.016f, 0.14f, FLinearColor(0.55f, 0.42f, 0.32f), 0.82f, 1.12f, 5800.f, 0.7f, 0.75f };
		break;
	case EIronArenaTheme::Port: // Golden hour over the water, sea haze.
		Look = { FRotator(-14.f, -120.f, 0.f), 4.6f, 4300.f, 0.005f, 0.15f, FLinearColor(0.6f, 0.5f, 0.43f), 1.05f, 1.1f, 5600.f, 0.5f, 0.6f };
		break;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		It->SetActorRotation(Look.Sun);
		if (UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			Sun->SetIntensity(Look.SunLux);
			Sun->SetTemperature(Look.SunTemp);
			Sun->bUseTemperature = true;
			Sun->SetDynamicShadowDistanceMovableLight(15000.f);
			Sun->MarkRenderStateDirty();
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->GetComponent())
		{
			Fog->SetFogDensity(Look.FogDensity);
			Fog->SetFogHeightFalloff(Look.FogFalloff);
			Fog->SetFogInscatteringColor(Look.FogColor);
			Fog->SetStartDistance(1500.f);
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			Sky->RecaptureSky();
		}
	}

	// Grade: an unbound post-process volume with the theme's colour, bloom, AO and vignette.
	FActorSpawnParameters Params;
	Params.Owner = this;
	if (APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(Params))
	{
		Volume->bUnbound = true;
		Volume->Priority = 1.f;
		FPostProcessSettings& S = Volume->Settings;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(Look.Saturation, Look.Saturation, Look.Saturation, 1.f);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(Look.Contrast, Look.Contrast, Look.Contrast, 1.f);
		S.bOverride_WhiteTemp = true;
		S.WhiteTemp = Look.WhiteTemp;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = Look.Exposure;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Look.Bloom;
		S.bOverride_AmbientOcclusionIntensity = true;
		S.AmbientOcclusionIntensity = 0.65f;
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.3f;
		S.bOverride_ScreenSpaceReflectionIntensity = true;
		S.ScreenSpaceReflectionIntensity = 100.f;
		S.bOverride_ScreenSpaceReflectionQuality = true;
		S.ScreenSpaceReflectionQuality = 60.f;
	}
}

void AIronArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Burning wrecks: flames licking up and a column of dark smoke drifting off each one.
	FireTimer -= DeltaSeconds;
	if (FireTimer > 0.f)
	{
		return;
	}
	FireTimer = 0.09f;
	for (int32 i = 0; i < Fires.Num(); ++i)
	{
		UIronPuffEmitter* Fire = Fires[i];
		if (!Fire)
		{
			continue;
		}
		FIronPuffStyle Flame;
		Flame.Life = 0.5f;
		Flame.StartScale = 0.45f;
		Flame.EndScale = 0.9f;
		Flame.Drift = FVector(0.f, 0.f, 220.f);
		Flame.StartColor = FLinearColor(4.f, 1.4f, 0.2f);
		Flame.EndColor = FLinearColor(0.4f, 0.1f, 0.03f);
		Flame.PeakOpacity = 0.85f;
		Flame.bSwell = false;
		Fire->Emit(FireSpots[i] + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-40.f, 40.f), 0.f), FVector::ZeroVector, Flame);
		FIronPuffStyle Smoke;
		Smoke.Life = 3.2f;
		Smoke.StartScale = 0.7f;
		Smoke.EndScale = 3.2f;
		Smoke.Drift = FVector(60.f, 30.f, 260.f);
		Smoke.StartColor = Smoke.EndColor = FLinearColor(0.035f, 0.033f, 0.032f);
		Smoke.PeakOpacity = 0.55f;
		Fire->Emit(FireSpots[i] + FVector(0.f, 0.f, 120.f), FVector::ZeroVector, Smoke);
	}
}
