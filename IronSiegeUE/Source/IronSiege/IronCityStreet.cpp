#include "IronCityStreet.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"
#include "Misc/PackageName.h"

namespace
{
constexpr float SlabThickness = 10.f;
constexpr float MarkingLift = 1.f;    // Paint sits this far above the asphalt to avoid z-fighting.
constexpr float GroundFloorH = 450.f;
constexpr float FloorH = 350.f;

UMaterialInterface* LoadCityMaterial(const TCHAR* Name)
{
	return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/IronSiege/City/Materials/%s.%s"), Name, Name));
}

// Splits [From, To] into the pieces left after cutting out Center +- HalfGap for every centre.
TArray<FVector2D> CutRanges(float From, float To, const TArray<float>& Centers, float HalfGap)
{
	TArray<FVector2D> Out = { FVector2D(From, To) };
	for (float C : Centers)
	{
		TArray<FVector2D> Next;
		for (const FVector2D& R : Out)
		{
			if (C + HalfGap <= R.X || C - HalfGap >= R.Y)
			{
				Next.Add(R);
				continue;
			}
			if (C - HalfGap > R.X) Next.Add(FVector2D(R.X, C - HalfGap));
			if (C + HalfGap < R.Y) Next.Add(FVector2D(C + HalfGap, R.Y));
		}
		Out = MoveTemp(Next);
	}
	return Out;
}
}

AIronCityStreet::AIronCityStreet()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void AIronCityStreet::ResolveMaterials()
{
	auto Fill = [](TObjectPtr<UMaterialInterface>& Slot, const TCHAR* Name)
	{
		if (!Slot) Slot = LoadCityMaterial(Name);
	};
	Fill(AsphaltMaterial, TEXT("MI_City_Asphalt"));
	Fill(PavingMaterial, TEXT("MI_City_Paving"));
	Fill(CurbMaterial, TEXT("MI_City_Curb"));
	Fill(GlassMaterial, TEXT("MI_City_Glass"));
	Fill(FrameMaterial, TEXT("MI_City_FrameDark"));
	Fill(SilverMaterial, TEXT("MI_City_Silver"));
	Fill(LineWhiteMaterial, TEXT("MI_City_LineWhite"));
	Fill(LineYellowMaterial, TEXT("MI_City_LineYellow"));
	Fill(FoliageMaterial, TEXT("MI_City_Foliage"));
	Fill(FoliageDarkMaterial, TEXT("MI_City_FoliageDark"));
	if (!FoliageDarkMaterial) FoliageDarkMaterial = FoliageMaterial;
	Fill(BarkMaterial, TEXT("MI_City_Bark"));
	// Brick fronts among the stone and plaster ones (added to a street saved before they existed).
	for (const TCHAR* Name : { TEXT("MI_Facade_Brick"), TEXT("MI_Facade_BrickDark"), TEXT("MI_Facade_BrickTan") })
	{
		if (FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/IronSiege/City/Materials/%s"), Name)))
		{
			if (UMaterialInterface* Brick = LoadCityMaterial(Name)) FacadeMaterials.AddUnique(Brick);
		}
	}
	Fill(SignalMaterial, TEXT("MI_City_Signal"));
	if (FacadeMaterials.Num() == 0)
	{
		for (const TCHAR* Name : { TEXT("MI_Facade_White"), TEXT("MI_Facade_Beige"), TEXT("MI_Facade_Gray"), TEXT("MI_Facade_Sand"), TEXT("MI_Facade_BlueGray"), TEXT("MI_Facade_Terracotta") })
		{
			if (UMaterialInterface* M = LoadCityMaterial(Name)) FacadeMaterials.Add(M);
		}
	}
	if (AwningMaterials.Num() == 0)
	{
		for (const TCHAR* Name : { TEXT("MI_Awning_Red"), TEXT("MI_Awning_Green"), TEXT("MI_Awning_Navy"), TEXT("MI_Awning_Cream"), TEXT("MI_Awning_Orange") })
		{
			if (UMaterialInterface* M = LoadCityMaterial(Name)) AwningMaterials.Add(M);
		}
	}
}

UStaticMesh* AIronCityStreet::Part(const TCHAR* Name) const
{
	const TObjectPtr<UStaticMesh>* Found = Parts.Find(FName(Name));
	return Found ? Found->Get() : nullptr;
}

UHierarchicalInstancedStaticMeshComponent* AIronCityStreet::Pool(EShape Shape, UMaterialInterface* Material, uint8 Flags)
{
	return MeshPool(Shape == EShape::Cube ? CubeMesh.Get() : Shape == EShape::Cylinder ? CylinderMesh.Get() : SphereMesh.Get(), Material, Flags);
}

UHierarchicalInstancedStaticMeshComponent* AIronCityStreet::MeshPool(UStaticMesh* Mesh, UMaterialInterface* Material, uint8 Flags)
{
	for (UHierarchicalInstancedStaticMeshComponent* Existing : Pools)
	{
		if (Existing && Existing->GetStaticMesh() == Mesh && (!Material || Existing->GetMaterial(0) == Material)
			&& Existing->ComponentTags.Contains(FName(*FString::Printf(TEXT("Flags%d"), Flags))))
		{
			return Existing;
		}
	}

	UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("CityPool")));
	C->CreationMethod = EComponentCreationMethod::Instance;
	C->ComponentTags.Add(FName(*FString::Printf(TEXT("Flags%d"), Flags)));
	C->SetStaticMesh(Mesh);
	if (Material)
	{
		C->SetMaterial(0, Material);
	}
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(RootComponent);
	if (Flags & Collide)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else if (Flags & GroundOnly)
	{
		// Road/sidewalk slabs are real ground for the Chaos cars (suspension traces and body
		// collision), so wheels climb the 15cm kerb; only the camera ignores them.
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->SetCastShadow(!(Flags & NoShadow));
	C->RegisterComponent();
	AddInstanceComponent(C);
	Pools.Add(C);
	return C;
}

void AIronCityStreet::Box(UMaterialInterface* Material, const FVector& Center, const FVector& Size, uint8 Flags, const FRotator& Rot)
{
	if (Size.X <= 0.f || Size.Y <= 0.f || Size.Z <= 0.f) return;
	if (bForceNoShadow) Flags |= NoShadow;
	Pool(EShape::Cube, Material, Flags)->AddInstance(FTransform(Rot, Center, Size / 100.f) * Frame);
}

void AIronCityStreet::Cylinder(UMaterialInterface* Material, const FVector& Center, float Diameter, float Height, uint8 Flags)
{
	if (bForceNoShadow) Flags |= NoShadow;
	Pool(EShape::Cylinder, Material, Flags)->AddInstance(FTransform(FRotator::ZeroRotator, Center, FVector(Diameter, Diameter, Height) / 100.f) * Frame);
}

void AIronCityStreet::Sphere(UMaterialInterface* Material, const FVector& Center, const FVector& Size, uint8 Flags)
{
	if (bForceNoShadow) Flags |= NoShadow;
	Pool(EShape::Sphere, Material, Flags)->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f) * Frame);
}

void AIronCityStreet::BuildSkyline()
{
	if (SkylineLine <= 0.f)
	{
		return;
	}
	LineOverride = SkylineLine;
	bForceNoShadow = true;
	for (int32 Quarter = 0; Quarter < 4; ++Quarter)
	{
		Frame = FTransform(FRotator(0.f, 90.f * Quarter, 0.f));
		// Rows on two opposite sides run long enough to also fill the corners.
		const float Extent = (Quarter % 2 == 0) ? SkylineLine + 2600.f : SkylineLine - 200.f;
		float X = -Extent;
		while (X < Extent - 900.f)
		{
			const float W = FMath::Min(Rng.FRandRange(1500.f, 3000.f), Extent - X);
			BuildBuilding(X, X + W, 1.f, Rng.FRandRange(1800.f, 2600.f), false, false, true);
			X += W + Rng.FRandRange(0.f, 400.f);
		}
	}
	Frame = FTransform::Identity;
	LineOverride = -1.f;
	bForceNoShadow = false;
}

void AIronCityStreet::BuildRingTrees()
{
	if (RingRoadCenter <= 0.f)
	{
		return;
	}
	const float Line = RingRoadCenter - RingRoadHalfWidth - 180.f;
	TArray<float> YStreets = { 0.f };
	for (const FVector2D& R : CutRanges(-Line + 300.f, Line - 300.f, CrossStreets, RoadHalfWidth + 400.f))
	{
		for (float X = R.X; X <= R.Y; X += 1000.f)
		{
			Tree(FVector(X, Line, RoadZ));
			Tree(FVector(X, -Line, RoadZ));
		}
	}
	for (const FVector2D& R : CutRanges(-Line + 300.f, Line - 300.f, YStreets, RoadHalfWidth + 400.f))
	{
		for (float Y = R.X; Y <= R.Y; Y += 1000.f)
		{
			Tree(FVector(Line, Y, RoadZ));
			Tree(FVector(-Line, Y, RoadZ));
		}
	}
}

void AIronCityStreet::Rebuild()
{
	for (UHierarchicalInstancedStaticMeshComponent* C : Pools)
	{
		if (C)
		{
			RemoveInstanceComponent(C);
			C->DestroyComponent();
		}
	}
	Pools.Reset();

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TreeCrowns.Reset();
	for (const TCHAR* Name : { TEXT("SM_TreeCrown_A"), TEXT("SM_TreeCrown_B") })
	{
		const FString Package = FString::Printf(TEXT("/Game/IronSiege/City/Meshes/%s"), Name);
		UStaticMesh* Crown = FPackageName::DoesPackageExist(Package) ? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, Name)) : nullptr;
		if (Crown)
		{
			TreeCrowns.Add(Crown);
		}
	}
	Parts.Reset();
	for (const TCHAR* Name : { TEXT("SM_Bld_Window_A"), TEXT("SM_Bld_Window_B"), TEXT("SM_Bld_Window_C"), TEXT("SM_Bld_Window_D"), TEXT("SM_Bld_Balcony"),
							   TEXT("SM_Bld_RoofUnit"), TEXT("SM_Bld_RoofTank"), TEXT("SM_Bld_Bulkhead"), TEXT("SM_Bld_Antenna"), TEXT("SM_Bld_FireEscape"),
							   TEXT("SM_Bld_LampHead"), TEXT("SM_Bld_Bench"), TEXT("SM_Bld_Bin"), TEXT("SM_Bld_Bollard"), TEXT("SM_Bld_Hydrant"),
							   TEXT("SM_Bld_SignalArm"), TEXT("SM_Bld_ShopDoor") })
	{
		const FString Package = FString::Printf(TEXT("/Game/IronSiege/City/Meshes/%s"), Name);
		if (FPackageName::DoesPackageExist(Package))
		{
			Parts.Add(FName(Name), LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, Name)));
		}
	}
	ResolveMaterials();
	if (!CubeMesh || !CylinderMesh || !SphereMesh || FacadeMaterials.Num() == 0 || AwningMaterials.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("IronCityStreet: missing meshes or city materials - run Content/Python/create_city_materials.py first."));
		return;
	}

	Rng.Initialize(Seed);
	BuildRoads();
	BuildMarkings();
	BuildSidewalks();
	BuildBuildingRows();
	BuildStreetFurniture();
	BuildTrafficLights();
	BuildRingTrees();
	BuildSkyline();

	int32 Instances = 0;
	for (UHierarchicalInstancedStaticMeshComponent* C : Pools)
	{
		Instances += C->GetInstanceCount();
		C->BuildTreeIfOutdated(false, true);
	}
	UE_LOG(LogTemp, Log, TEXT("IronCityStreet: rebuilt %d pools, %d instances"), Pools.Num(), Instances);
}

TArray<FVector2D> AIronCityStreet::MainBlocks() const
{
	return CutRanges(-HalfLength, HalfLength, CrossStreets, BuildingLine());
}

void AIronCityStreet::BuildRoads()
{
	const float Zc = RoadZ - SlabThickness * 0.5f;
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;

	Box(AsphaltMaterial, FVector(0.f, 0.f, Zc), FVector(2.f * HalfLength, 2.f * RoadHalfWidth, SlabThickness), GroundOnly);
	for (float Cx : CrossStreets)
	{
		for (float S : { -1.f, 1.f })
		{
			const float Len = RingInner - RoadHalfWidth;
			Box(AsphaltMaterial, FVector(Cx, S * (RoadHalfWidth + Len * 0.5f), Zc), FVector(2.f * RoadHalfWidth, Len, SlabThickness), GroundOnly);
		}
	}
	if (RingRoadCenter > 0.f)
	{
		const float Outer = RingRoadCenter + RingRoadHalfWidth;
		for (float S : { -1.f, 1.f })
		{
			Box(AsphaltMaterial, FVector(0.f, S * RingRoadCenter, Zc), FVector(2.f * Outer, 2.f * RingRoadHalfWidth, SlabThickness), GroundOnly);
			Box(AsphaltMaterial, FVector(S * RingRoadCenter, 0.f, Zc), FVector(2.f * RingRoadHalfWidth, 2.f * RingInner, SlabThickness), GroundOnly);
		}
	}
}

void AIronCityStreet::BuildMarkings()
{
	const float Z = RoadZ + MarkingLift * 0.5f;
	const float LineT = MarkingLift;
	const uint8 F = NoShadow;
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;

	// Dashes along X between A and B at lateral offset Y.
	auto DashesX = [&](float A, float B, float Y, float Width, float Dash, float Gap)
	{
		for (float X = A + Gap * 0.5f; X + Dash <= B; X += Dash + Gap)
		{
			Box(LineWhiteMaterial, FVector(X + Dash * 0.5f, Y, Z), FVector(Dash, Width, LineT), F);
		}
	};
	auto DashesY = [&](float A, float B, float X, float Width, float Dash, float Gap)
	{
		for (float Y = A + Gap * 0.5f; Y + Dash <= B; Y += Dash + Gap)
		{
			Box(LineWhiteMaterial, FVector(X, Y + Dash * 0.5f, Z), FVector(Width, Dash, LineT), F);
		}
	};

	// Main avenue: double yellow centre line, dashed lane dividers, solid edge lines.
	const float Margin = RoadHalfWidth + 520.f;
	for (const FVector2D& R : CutRanges(-HalfLength, HalfLength, CrossStreets, Margin))
	{
		const float Len = R.Y - R.X, Mid = (R.X + R.Y) * 0.5f;
		for (float Y : { -12.f, 12.f })
		{
			Box(LineYellowMaterial, FVector(Mid, Y, Z), FVector(Len, 12.f, LineT), F);
		}
		for (float S : { -1.f, 1.f })
		{
			DashesX(R.X, R.Y, S * RoadHalfWidth * 0.5f, 12.f, 300.f, 600.f);
			Box(LineWhiteMaterial, FVector(Mid, S * (RoadHalfWidth - 45.f), Z), FVector(Len, 15.f, LineT), F);
		}
	}

	for (float Cx : CrossStreets)
	{
		// Zebra crossings over the avenue either side of the junction, plus stop lines.
		for (float Sx : { -1.f, 1.f })
		{
			const float BandX = Cx + Sx * (RoadHalfWidth + 250.f);
			for (float Y = -RoadHalfWidth + 75.f; Y <= RoadHalfWidth - 75.f; Y += 100.f)
			{
				Box(LineWhiteMaterial, FVector(BandX, Y, Z), FVector(400.f, 50.f, LineT), F);
			}
			// Traffic approaching this junction from the Sx side drives on the -Sx*... lane half;
			// keep it simple and mark the stop line across the full approach half-road.
			Box(LineWhiteMaterial, FVector(Cx + Sx * (RoadHalfWidth + 500.f), Sx * RoadHalfWidth * 0.5f, Z), FVector(40.f, RoadHalfWidth, LineT), F);
		}
		// Cross street: zebra near the avenue, dashed centre line out to the ring road.
		for (float Sy : { -1.f, 1.f })
		{
			const float BandY = Sy * (RoadHalfWidth + SidewalkWidth + 250.f);
			for (float X = Cx - RoadHalfWidth + 75.f; X <= Cx + RoadHalfWidth - 75.f; X += 100.f)
			{
				Box(LineWhiteMaterial, FVector(X, BandY, Z), FVector(50.f, 400.f, LineT), F);
			}
			const float A = RoadHalfWidth + SidewalkWidth + 520.f, B = RingInner - 100.f;
			if (Sy > 0.f) DashesY(A, B, Cx, 12.f, 300.f, 600.f);
			else DashesY(-B, -A, Cx, 12.f, 300.f, 600.f);
			for (float Sx : { -1.f, 1.f })
			{
				const float EdgeX = Cx + Sx * (RoadHalfWidth - 45.f);
				const float Len = B - A;
				Box(LineWhiteMaterial, FVector(EdgeX, Sy * (A + Len * 0.5f), Z), FVector(15.f, Len, LineT), F);
			}
		}
	}

	if (RingRoadCenter > 0.f)
	{
		for (float S : { -1.f, 1.f })
		{
			DashesX(-RingInner, RingInner, S * RingRoadCenter, 12.f, 300.f, 600.f);
			DashesY(-RingInner, RingInner, S * RingRoadCenter, 12.f, 300.f, 600.f);
		}
	}
}

void AIronCityStreet::BuildSidewalks()
{
	const float Top = SidewalkTop();
	const float Thick = CurbHeight + SlabThickness;
	const float Zc = Top - Thick * 0.5f;
	const float CurbW = 20.f;
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;

	// Main avenue sidewalks, interrupted only by the cross-street carriageways.
	for (const FVector2D& R : CutRanges(-HalfLength, HalfLength, CrossStreets, RoadHalfWidth))
	{
		const float Len = R.Y - R.X, Mid = (R.X + R.Y) * 0.5f;
		for (float S : { -1.f, 1.f })
		{
			const float W = SidewalkWidth - CurbW;
			Box(PavingMaterial, FVector(Mid, S * (RoadHalfWidth + CurbW + W * 0.5f), Zc), FVector(Len, W, Thick), GroundOnly);
			Box(CurbMaterial, FVector(Mid, S * (RoadHalfWidth + CurbW * 0.5f), Zc + 0.5f), FVector(Len, CurbW, Thick + 1.f), GroundOnly);
		}
	}
	// Cross-street sidewalks from the avenue's building line out to the ring road.
	for (float Cx : CrossStreets)
	{
		for (float Sx : { -1.f, 1.f })
		{
			for (float Sy : { -1.f, 1.f })
			{
				const float A = BuildingLine(), B = RingInner;
				const float Len = B - A;
				const float W = SidewalkWidth - CurbW;
				Box(PavingMaterial, FVector(Cx + Sx * (RoadHalfWidth + CurbW + W * 0.5f), Sy * (A + Len * 0.5f), Zc), FVector(W, Len, Thick), GroundOnly);
				const float CurbLen = B - RoadHalfWidth;
				Box(CurbMaterial, FVector(Cx + Sx * (RoadHalfWidth + CurbW * 0.5f), Sy * (RoadHalfWidth + CurbLen * 0.5f), Zc + 0.5f), FVector(CurbW, CurbLen, Thick + 1.f), GroundOnly);
			}
		}
	}
}

void AIronCityStreet::BuildBuildingRows()
{
	for (float Side : { -1.f, 1.f })
	{
		for (const FVector2D& Block : MainBlocks())
		{
			float X = Block.X;
			bool bPrevGap = true;
			while (X < Block.Y - 700.f)
			{
				float W = Rng.FRandRange(1100.f, 2400.f);
				if (Block.Y - (X + W) < 1000.f)
				{
					W = Block.Y - X;
				}
				const bool bGap = (X + W < Block.Y - 1) && Rng.FRand() < 0.3f;
				const float Depth = Rng.FRandRange(1400.f, 2200.f);
				BuildBuilding(X, X + W, Side, Depth, bPrevGap, bGap || X + W >= Block.Y - 1.f);
				bPrevGap = bGap;
				X += W + (bGap ? 350.f : 0.f);
			}
		}
	}
}

void AIronCityStreet::BuildBuilding(float X0, float X1, float Side, float Depth, bool bExposedLeft, bool bExposedRight, bool bSkyline)
{
	const float Base = SidewalkTop();
	const float BL = BuildingLine();
	const int32 Style = Rng.RandRange(0, 99) < (bSkyline ? 45 : 25) ? 0 : (Rng.RandRange(0, 99) < 55 ? 1 : 2);
	int32 Floors = Style == 0 ? Rng.RandRange(9, 16) : Style == 1 ? Rng.RandRange(4, 8) : Rng.RandRange(5, 10);
	if (bSkyline)
	{
		Floors = Style == 0 ? Rng.RandRange(16, 30) : Rng.RandRange(8, 15);
	}
	const float Top = Base + GroundFloorH + Floors * FloorH;
	UMaterialInterface* Facade = FacadeMaterials[Rng.RandRange(0, FacadeMaterials.Num() - 1)];
	UMaterialInterface* BodyMat = Style == 0 ? GlassMaterial.Get() : Facade;
	const float Width = X1 - X0;

	// Main mass, sunk below the road so no gap shows at the pavement.
	const float Bottom = RoadZ - 20.f;
	Box(BodyMat, FVector((X0 + X1) * 0.5f, Side * (BL + Depth * 0.5f), (Bottom + Top) * 0.5f), FVector(Width, Depth, Top - Bottom), Collide);

	// Face-relative panel: U runs along the face (front: world X; sides: distance back from the
	// building line), Protrude is how far it stands proud of the wall (it is also sunk 2cm into it).
	enum class EFace : uint8 { Front, Left, Right };
	auto Panel = [&](EFace Face, UMaterialInterface* M, float U0, float U1, float Z0, float Z1, float Protrude, uint8 Flags)
	{
		const float Zc = (Z0 + Z1) * 0.5f, H = Z1 - Z0, T = Protrude + 2.f;
		switch (Face)
		{
		case EFace::Front:
			Box(M, FVector((U0 + U1) * 0.5f, Side * (BL + (2.f - Protrude) * 0.5f), Zc), FVector(U1 - U0, T, H), Flags);
			break;
		case EFace::Left:
			Box(M, FVector(X0 + (2.f - Protrude) * 0.5f, Side * (BL + (U0 + U1) * 0.5f), Zc), FVector(T, U1 - U0, H), Flags);
			break;
		case EFace::Right:
			Box(M, FVector(X1 - (2.f - Protrude) * 0.5f, Side * (BL + (U0 + U1) * 0.5f), Zc), FVector(T, U1 - U0, H), Flags);
			break;
		}
	};

	// A modelled part on a wall: they are made facing -Y with X along the wall, so each face turns
	// them its own way. U and Z are where the part's origin goes (see Panel for U).
	auto WallPart = [&](EFace Face, UStaticMesh* Mesh, float U, float Z, uint8 Flags)
	{
		if (bForceNoShadow) Flags |= NoShadow;
		FTransform Where;
		switch (Face)
		{
		case EFace::Front:
			Where = FTransform(FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), FVector(U, Side * BL, Z));
			break;
		case EFace::Left:
			Where = FTransform(FRotator(0.f, -90.f, 0.f), FVector(X0, Side * (BL + U), Z));
			break;
		case EFace::Right:
			Where = FTransform(FRotator(0.f, 90.f, 0.f), FVector(X1, Side * (BL + U), Z));
			break;
		}
		MeshPool(Mesh, nullptr, Flags)->AddInstance(Where * Frame);
	};
	// The dressing has its own dice, so adding to it never moves the buildings themselves.
	FRandomStream Deco(HashCombine(GetTypeHash(Seed), HashCombine(GetTypeHash(FMath::RoundToInt(X0)), GetTypeHash(FMath::RoundToInt(Side * BL)))));
	UStaticMesh* WindowKinds[] = { Part(TEXT("SM_Bld_Window_A")), Part(TEXT("SM_Bld_Window_B")), Part(TEXT("SM_Bld_Window_C")), Part(TEXT("SM_Bld_Window_D")) };
	const bool bWindows = WindowKinds[0] && WindowKinds[1] && WindowKinds[2] && WindowKinds[3];
	UStaticMesh* BalconyMesh = Part(TEXT("SM_Bld_Balcony"));
	const bool bBalconies = BalconyMesh && !bSkyline && Deco.FRand() < 0.4f;
	const int32 BalconyEvery = Deco.RandRange(2, 3);

	struct FFace { EFace Face; float U0; float U1; };
	TArray<FFace> Faces = { { EFace::Front, X0, X1 } };
	if (bExposedLeft) Faces.Add({ EFace::Left, 0.f, Depth });
	if (bExposedRight) Faces.Add({ EFace::Right, 0.f, Depth });

	const float WindowPitch = Rng.FRandRange(250.f, 320.f);
	const float MullionPitch = Rng.FRandRange(140.f, 180.f);

	for (const FFace& F : Faces)
	{
		const float Len = F.U1 - F.U0;
		const float UpperBase = Base + GroundFloorH;

		if (Style == 0)
		{
			// Curtain wall: silver spandrel band at every floor line, dark vertical mullions.
			for (int32 k = 0; k <= Floors; ++k)
			{
				const float Z = UpperBase + k * FloorH;
				Panel(F.Face, SilverMaterial, F.U0, F.U1, Z - 22.f, Z + 22.f, 6.f, NoShadow);
			}
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / MullionPitch));
			for (int32 i = 0; i <= N; ++i)
			{
				const float U = F.U0 + Len * i / N;
				Panel(F.Face, FrameMaterial, U - 4.f, U + 4.f, UpperBase, Top, 10.f, NoShadow);
			}
			// Stone crown and podium bands frame the glass.
			Panel(F.Face, Facade, F.U0 - 1.f, F.U1 + 1.f, Top - 110.f, Top, 14.f, None);
			if (F.Face != EFace::Front)
			{
				Panel(F.Face, Facade, F.U0, F.U1, Base, UpperBase, 4.f, None);
			}
		}
		else if (Style == 1)
		{
			// Punched windows: glass pane over a dark frame border, projecting sill, string courses.
			const int32 N = FMath::Max(1, FMath::FloorToInt((Len - 80.f) / WindowPitch));
			const float Start = F.U0 + (Len - N * WindowPitch) * 0.5f + WindowPitch * 0.5f;
			for (int32 k = 0; k < Floors; ++k)
			{
				const float Zf = UpperBase + k * FloorH;
				Panel(F.Face, Facade, F.U0, F.U1, Zf - 8.f, Zf + 8.f, 5.f, NoShadow);
				for (int32 i = 0; i < N; ++i)
				{
					const float U = Start + i * WindowPitch;
					if (bWindows)
					{
						// A window in its surround: clear, dark or curtained, the odd one with an air
						// conditioner; on some buildings every second or third column is balconies.
						if (bBalconies && F.Face == EFace::Front && i % BalconyEvery == 1)
						{
							WallPart(F.Face, BalconyMesh, U, Zf + 62.f, None);
							continue;
						}
						const int32 Roll = Deco.RandRange(0, 99);
						WallPart(F.Face, WindowKinds[Roll < 45 ? 0 : (Roll < 68 ? 1 : (Roll < 88 ? 2 : 3))], U, Zf + 62.f, NoShadow);
						continue;
					}
					Panel(F.Face, FrameMaterial, U - 78.f, U + 78.f, Zf + 72.f, Zf + 288.f, 2.f, NoShadow);
					Panel(F.Face, GlassMaterial, U - 70.f, U + 70.f, Zf + 80.f, Zf + 280.f, 3.5f, NoShadow);
					Panel(F.Face, FrameMaterial, U - 2.f, U + 2.f, Zf + 80.f, Zf + 280.f, 4.5f, NoShadow);
					Panel(F.Face, Facade, U - 88.f, U + 88.f, Zf + 62.f, Zf + 72.f, 11.f, None);
				}
			}
			Panel(F.Face, Facade, F.U0 - 12.f, F.U1 + 12.f, Top - 55.f, Top, 22.f, None);
			if (F.Face != EFace::Front)
			{
				Panel(F.Face, Facade, F.U0, F.U1, UpperBase - 12.f, UpperBase + 12.f, 8.f, None);
			}
			// A fire escape down the front of some of the lower ones: a landing and stair per floor.
			UStaticMesh* Escape = Part(TEXT("SM_Bld_FireEscape"));
			if (Escape && F.Face == EFace::Front && !bSkyline && Floors <= 8 && Len > 900.f && Deco.FRand() < 0.35f)
			{
				const float U = Deco.FRand() < 0.5f ? F.U0 + 190.f : F.U1 - 190.f;
				for (int32 k = 1; k < Floors; ++k)
				{
					WallPart(F.Face, Escape, U, UpperBase + k * FloorH + 30.f, None);
				}
			}
		}
		else
		{
			// Ribbon windows: continuous glazing per floor between projecting slab-edge bands.
			for (int32 k = 0; k < Floors; ++k)
			{
				const float Zf = UpperBase + k * FloorH;
				Panel(F.Face, Facade, F.U0, F.U1, Zf - 12.f, Zf + 22.f, 14.f, None);
				Panel(F.Face, FrameMaterial, F.U0 + 34.f, F.U1 - 34.f, Zf + 84.f, Zf + 266.f, 2.f, NoShadow);
				Panel(F.Face, GlassMaterial, F.U0 + 40.f, F.U1 - 40.f, Zf + 90.f, Zf + 260.f, 3.5f, NoShadow);
				const int32 N = FMath::Max(1, FMath::RoundToInt((Len - 80.f) / MullionPitch));
				for (int32 i = 1; i < N; ++i)
				{
					const float U = F.U0 + 40.f + (Len - 80.f) * i / N;
					Panel(F.Face, FrameMaterial, U - 3.f, U + 3.f, Zf + 90.f, Zf + 260.f, 5.f, NoShadow);
				}
			}
			Panel(F.Face, Facade, F.U0 - 1.f, F.U1 + 1.f, Top - 40.f, Top, 18.f, None);
		}
	}

	// Street-level frontage (hidden behind the perimeter wall for the skyline, so skipped there).
	if (bSkyline)
	{
	}
	else if (Style == 0)
	{
		// Tower lobby: full-height glass with a stone plinth and a cantilevered entrance canopy.
		Panel(EFace::Front, Facade, X0, X1, Base, Base + 40.f, 6.f, None);
		const int32 N = FMath::Max(1, FMath::RoundToInt(Width / 300.f));
		for (int32 i = 0; i <= N; ++i)
		{
			const float U = X0 + Width * i / N;
			Panel(EFace::Front, FrameMaterial, U - 5.f, U + 5.f, Base + 40.f, Base + GroundFloorH, 8.f, NoShadow);
		}
		const float Mid = (X0 + X1) * 0.5f;
		const float CanopyW = FMath::Min(Width * 0.5f, 900.f);
		Box(SilverMaterial, FVector(Mid, Side * (BL - 110.f), Base + 380.f), FVector(CanopyW, 220.f, 14.f), None);
	}
	else
	{
		BuildStorefronts(X0, X1, Side, BL, true);
	}

	// Roof: parapet, and a few mechanical units / a water tank.
	const float ParapetH = 80.f, ParapetT = 25.f;
	const float Yf = Side * BL, Yb = Side * (BL + Depth), Ym = (Yf + Yb) * 0.5f;
	UMaterialInterface* ParapetMat = Style == 0 ? SilverMaterial.Get() : Facade;
	Box(ParapetMat, FVector((X0 + X1) * 0.5f, Yf + Side * ParapetT * 0.5f, Top + ParapetH * 0.5f), FVector(Width, ParapetT, ParapetH), None);
	Box(ParapetMat, FVector((X0 + X1) * 0.5f, Yb - Side * ParapetT * 0.5f, Top + ParapetH * 0.5f), FVector(Width, ParapetT, ParapetH), None);
	Box(ParapetMat, FVector(X0 + ParapetT * 0.5f, Ym, Top + ParapetH * 0.5f), FVector(ParapetT, Depth, ParapetH), None);
	Box(ParapetMat, FVector(X1 - ParapetT * 0.5f, Ym, Top + ParapetH * 0.5f), FVector(ParapetT, Depth, ParapetH), None);
	const int32 Units = Rng.RandRange(1, 3);
	for (int32 i = 0; i < Units; ++i)
	{
		const FVector Size(Rng.FRandRange(180.f, 420.f), Rng.FRandRange(180.f, 380.f), Rng.FRandRange(110.f, 220.f));
		const float Ux = Rng.FRandRange(X0 + 150.f + Size.X * 0.5f, FMath::Max(X0 + 151.f + Size.X * 0.5f, X1 - 150.f - Size.X * 0.5f));
		const float Uy = Side * (BL + Rng.FRandRange(300.f + Size.Y * 0.5f, FMath::Max(301.f + Size.Y * 0.5f, Depth - 200.f - Size.Y * 0.5f)));
		if (UStaticMesh* Unit = Part(TEXT("SM_Bld_RoofUnit")))
		{
			// An air handler, modelled 2.2 x 1.6 x 1.2 m and stretched to this one's size.
			const FTransform Where(FRotator(0.f, Deco.RandRange(0, 1) * 180.f, 0.f), FVector(Ux, Uy, Top), Size / FVector(220.f, 160.f, 120.f));
			MeshPool(Unit, nullptr, bForceNoShadow ? NoShadow : None)->AddInstance(Where * Frame);
			continue;
		}
		Box(SilverMaterial, FVector(Ux, Uy, Top + Size.Z * 0.5f), Size, None);
	}
	if (Style != 0 && Rng.FRand() < 0.3f)
	{
		const FVector P((X0 + X1) * 0.5f + Rng.FRandRange(-200.f, 200.f), Side * (BL + Depth * 0.6f), Top);
		if (UStaticMesh* Tank = Part(TEXT("SM_Bld_RoofTank")))
		{
			MeshPool(Tank, nullptr, bForceNoShadow ? NoShadow : None)->AddInstance(FTransform(FRotator(0.f, Deco.FRandRange(0.f, 90.f), 0.f), P) * Frame);
		}
		else
		{
			for (float Dx : { -90.f, 90.f })
				for (float Dy : { -90.f, 90.f })
					Cylinder(FrameMaterial, P + FVector(Dx, Dy, 150.f), 12.f, 300.f, None);
			Cylinder(BarkMaterial, P + FVector(0.f, 0.f, 420.f), 300.f, 260.f, None);
		}
	}
	// Where the stair comes out on the roof, and on some an antenna mast.
	if (UStaticMesh* Hut = Part(TEXT("SM_Bld_Bulkhead")))
	{
		if (Width > 900.f && Depth > 900.f && Deco.FRand() < 0.7f)
		{
			const FVector At(X1 - 260.f, Side * (BL + Depth - 240.f), Top);
			MeshPool(Hut, nullptr, bForceNoShadow ? NoShadow : None)->AddInstance(FTransform(FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), At) * Frame);
		}
	}
	if (UStaticMesh* Mast = Part(TEXT("SM_Bld_Antenna")))
	{
		if (Deco.FRand() < 0.3f)
		{
			const FVector At(X0 + 160.f, Side * (BL + Depth * 0.45f), Top);
			MeshPool(Mast, nullptr, NoShadow)->AddInstance(FTransform(FRotator(0.f, Deco.FRandRange(0.f, 360.f), 0.f), At) * Frame);
		}
	}
}

void AIronCityStreet::BuildStorefronts(float X0, float X1, float Side, float FrontY, bool bAwnings)
{
	const float Base = SidewalkTop();
	const float Width = X1 - X0;
	const int32 Units = FMath::Max(1, FMath::RoundToInt(Width / 650.f));
	const float UnitW = Width / Units;
	UMaterialInterface* Pier = FacadeMaterials[Rng.RandRange(0, FacadeMaterials.Num() - 1)];

	auto Front = [&](UMaterialInterface* M, float U0, float U1, float Z0, float Z1, float Protrude, uint8 Flags)
	{
		Box(M, FVector((U0 + U1) * 0.5f, Side * (FrontY + (2.f - Protrude) * 0.5f), (Z0 + Z1) * 0.5f), FVector(U1 - U0, Protrude + 2.f, Z1 - Z0), Flags);
	};

	for (int32 i = 0; i < Units; ++i)
	{
		const float U0 = X0 + i * UnitW, U1 = U0 + UnitW, Mid = (U0 + U1) * 0.5f;
		Front(Pier, U0, U0 + 30.f, Base, Base + GroundFloorH, 14.f, None);
		if (i == Units - 1) Front(Pier, U1 - 30.f, U1, Base, Base + GroundFloorH, 14.f, None);

		Front(FrameMaterial, U0 + 30.f, U1 - 30.f, Base, Base + 40.f, 6.f, NoShadow);         // Kick plate.
		Front(FrameMaterial, U0 + 30.f, U1 - 30.f, Base + 40.f, Base + 335.f, 2.f, NoShadow);  // Frame border.
		Front(GlassMaterial, U0 + 38.f, U1 - 38.f, Base + 46.f, Base + 328.f, 3.5f, NoShadow); // Shop window.
		if (UStaticMesh* Door = Part(TEXT("SM_Bld_ShopDoor")))
		{
			// A glazed door between the two panes.
			MeshPool(Door, nullptr, NoShadow)->AddInstance(FTransform(FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), FVector(Mid, Side * FrontY, Base + 40.f)) * Frame);
		}
		else
		{
			Front(FrameMaterial, Mid - 4.f, Mid + 4.f, Base + 46.f, Base + 328.f, 5.f, NoShadow);  // Door/window split.
		}
		// Fascia band for the shop sign.
		Front(Rng.FRand() < 0.5f ? FrameMaterial.Get() : AwningMaterials[Rng.RandRange(0, AwningMaterials.Num() - 1)].Get(), U0 + 30.f, U1 - 30.f, Base + 345.f, Base + 420.f, 7.f, None);

		if (bAwnings && Rng.FRand() < 0.55f)
		{
			UMaterialInterface* Cloth = AwningMaterials[Rng.RandRange(0, AwningMaterials.Num() - 1)];
			const float Depth = 150.f, Tilt = 18.f;
			const float W = UnitW - 70.f;
			const float Zc = Base + 355.f;
			// Slopes down toward the street; the outer edge carries a short valance.
			Box(Cloth, FVector(Mid, Side * (FrontY - Depth * 0.5f * FMath::Cos(FMath::DegreesToRadians(Tilt))), Zc), FVector(W, Depth, 5.f), None, FRotator(0.f, 0.f, -Side * Tilt));
			const float OuterZ = Zc - Depth * 0.5f * FMath::Sin(FMath::DegreesToRadians(Tilt));
			Box(Cloth, FVector(Mid, Side * (FrontY - Depth * FMath::Cos(FMath::DegreesToRadians(Tilt))), OuterZ - 14.f), FVector(W, 3.f, 28.f), None);
		}
	}
}

void AIronCityStreet::Lamp(const FVector& Base, float Side)
{
	if (UStaticMesh* Head = Part(TEXT("SM_Bld_LampHead")))
	{
		// The pole is the street's own cylinder (what a car hits); foot, arm and lantern are modelled.
		Cylinder(FrameMaterial, Base + FVector(0.f, 0.f, 400.f), 16.f, 800.f, Collide);
		MeshPool(Head, nullptr, bForceNoShadow ? NoShadow : None)->AddInstance(FTransform(FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), Base) * Frame);
		return;
	}
	Cylinder(FrameMaterial, Base + FVector(0.f, 0.f, 30.f), 30.f, 60.f, None);
	Cylinder(FrameMaterial, Base + FVector(0.f, 0.f, 400.f), 16.f, 800.f, Collide);
	Box(FrameMaterial, Base + FVector(0.f, -Side * 90.f, 792.f), FVector(12.f, 180.f, 10.f), None);
	Box(SilverMaterial, Base + FVector(0.f, -Side * 175.f, 782.f), FVector(42.f, 75.f, 14.f), None);
}

void AIronCityStreet::Tree(const FVector& Base)
{
	Box(FrameMaterial, Base + FVector(0.f, 0.f, 0.6f), FVector(130.f, 130.f, 2.f), NoShadow);
	const float TrunkH = Rng.FRandRange(280.f, 340.f);
	Cylinder(BarkMaterial, Base + FVector(0.f, 0.f, TrunkH * 0.5f), 24.f, TrunkH, Collide);
	if (TreeCrowns.Num() > 0)
	{
		// A modelled crown - limbs and leaf cards - on the trunk, each tree turned and sized its own way.
		UStaticMesh* Crown = TreeCrowns[Rng.RandRange(0, TreeCrowns.Num() - 1)];
		const FTransform Where(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Base + FVector(0.f, 0.f, TrunkH), FVector(Rng.FRandRange(0.9f, 1.2f)));
		MeshPool(Crown, nullptr, bForceNoShadow ? NoShadow : None)->AddInstance(Where * Frame);
		return;
	}
	// Two branches forking off the trunk, then a loose crown of small leaf clusters in two greens
	// (darker ones low and inside) - reads far less like a lollipop than one or two big spheres.
	for (float Dir : { -1.f, 1.f })
	{
		Box(BarkMaterial, Base + FVector(Dir * 30.f, 0.f, TrunkH - 20.f), FVector(10.f, 10.f, 110.f), None, FRotator(0.f, Rng.FRandRange(0.f, 180.f), Dir * 28.f));
	}
	const int32 Blobs = Rng.RandRange(7, 10);
	for (int32 i = 0; i < Blobs; ++i)
	{
		const float R = Rng.FRandRange(95.f, 165.f);
		const float Ang = Rng.FRandRange(0.f, 2.f * PI);
		const float Rad = Rng.FRandRange(20.f, 115.f);
		const float Up = Rng.FRandRange(40.f, 230.f);
		const FVector Offset(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, TrunkH + Up);
		UMaterialInterface* Leaves = (Up < 110.f || Rad < 50.f) ? FoliageDarkMaterial.Get() : FoliageMaterial.Get();
		Sphere(Leaves, Base + Offset, FVector(R, R, R * 0.85f), None);
	}
}

void AIronCityStreet::BuildStreetFurniture()
{
	const float Top = SidewalkTop();
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;

	for (float Side : { -1.f, 1.f })
	{
		for (const FVector2D& R : CutRanges(-HalfLength, HalfLength, CrossStreets, BuildingLine() + 250.f))
		{
			// Lamps every 24m, trees midway between them, a bench and a bin beside every other tree.
			int32 Index = 0;
			for (float X = R.X + 300.f; X <= R.Y - 300.f; X += 1200.f, ++Index)
			{
				if (Index % 2 == 0)
				{
					Lamp(FVector(X, Side * (RoadHalfWidth + 60.f), Top), Side);
					if (UStaticMesh* Plug = Part(TEXT("SM_Bld_Hydrant")); Plug && Index % 4 == 2)
					{
						MeshPool(Plug, nullptr, None)->AddInstance(FTransform(FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), FVector(X + 190.f, Side * (RoadHalfWidth + 48.f), Top)) * Frame);
					}
				}
				else
				{
					Tree(FVector(X, Side * (RoadHalfWidth + 115.f), Top));
					if (Index % 4 == 1)
					{
						const FVector B(X + 260.f, Side * (RoadHalfWidth + 150.f), Top);
						const FRotator Facing(0.f, Side > 0.f ? 0.f : 180.f, 0.f); // Modelled furniture faces -Y: turn it to the road.
						if (UStaticMesh* Seat = Part(TEXT("SM_Bld_Bench")))
						{
							MeshPool(Seat, nullptr, None)->AddInstance(FTransform(Facing, B) * Frame);
						}
						else
						{
							Box(BarkMaterial, B + FVector(0.f, 0.f, 45.f), FVector(180.f, 45.f, 7.f), None);
							Box(BarkMaterial, B + FVector(0.f, Side * 22.f, 75.f), FVector(180.f, 6.f, 40.f), None);
							for (float Dx : { -75.f, 75.f })
								Box(FrameMaterial, B + FVector(Dx, 0.f, 21.f), FVector(8.f, 40.f, 42.f), NoShadow);
						}
						if (UStaticMesh* Litter = Part(TEXT("SM_Bld_Bin")))
						{
							MeshPool(Litter, nullptr, None)->AddInstance(FTransform(Facing, FVector(X - 260.f, Side * (RoadHalfWidth + 70.f), Top)) * Frame);
						}
						else
						{
							Cylinder(FrameMaterial, FVector(X - 260.f, Side * (RoadHalfWidth + 70.f), Top + 45.f), 50.f, 90.f, None);
						}
					}
				}
			}
			// Bollards guarding the junction corners.
			for (float Edge : { R.X, R.Y })
			{
				if (FMath::Abs(Edge) >= HalfLength - 1.f) continue;
				const float Dir = Edge == R.X ? 1.f : -1.f;
				for (int32 i = 0; i < 3; ++i)
				{
					const FVector At(Edge + Dir * (60.f + i * 120.f), Side * (RoadHalfWidth + 45.f), Top);
					if (UStaticMesh* Post = Part(TEXT("SM_Bld_Bollard")))
					{
						MeshPool(Post, nullptr, NoShadow)->AddInstance(FTransform(At) * Frame);
					}
					else
					{
						Cylinder(SilverMaterial, At + FVector(0.f, 0.f, 45.f), 18.f, 90.f, None);
					}
				}
			}
		}
	}

	for (float Cx : CrossStreets)
	{
		for (float Sx : { -1.f, 1.f })
		{
			for (float Sy : { -1.f, 1.f })
			{
				int32 Index = 0;
				for (float D = BuildingLine() + 900.f; D <= RingInner - 300.f; D += 1200.f, ++Index)
				{
					const FVector P(Cx + Sx * (RoadHalfWidth + 60.f), Sy * D, Top);
					if (Index % 2 == 0)
					{
						// Lamp arm points across the carriageway (-Sx in X).
						if (UStaticMesh* Head = Part(TEXT("SM_Bld_LampHead")))
						{
							Cylinder(FrameMaterial, P + FVector(0.f, 0.f, 400.f), 16.f, 800.f, Collide);
							MeshPool(Head, nullptr, None)->AddInstance(FTransform(FRotator(0.f, Sx > 0.f ? -90.f : 90.f, 0.f), P) * Frame);
							continue;
						}
						Cylinder(FrameMaterial, P + FVector(0.f, 0.f, 30.f), 30.f, 60.f, None);
						Cylinder(FrameMaterial, P + FVector(0.f, 0.f, 400.f), 16.f, 800.f, Collide);
						Box(FrameMaterial, P + FVector(-Sx * 90.f, 0.f, 792.f), FVector(180.f, 12.f, 10.f), None);
						Box(SilverMaterial, P + FVector(-Sx * 175.f, 0.f, 782.f), FVector(75.f, 42.f, 14.f), None);
					}
					else
					{
						Tree(P + FVector(Sx * 55.f, 0.f, 0.f));
					}
				}
			}
		}
	}
}

void AIronCityStreet::BuildTrafficLights()
{
	const float Top = SidewalkTop();
	for (float Cx : CrossStreets)
	{
		for (float Sx : { -1.f, 1.f })
		{
			for (float Sy : { -1.f, 1.f })
			{
				const FVector P(Cx + Sx * (RoadHalfWidth + 70.f), Sy * (RoadHalfWidth + 70.f), Top);
				Cylinder(FrameMaterial, P + FVector(0.f, 0.f, 260.f), 14.f, 520.f, Collide);
				if (UStaticMesh* Signal = Part(TEXT("SM_Bld_SignalArm")))
				{
					// Modelled mast arm (it reaches along -Y) with its heads; the pole above is the collision.
					MeshPool(Signal, nullptr, None)->AddInstance(FTransform(FRotator(0.f, Sy > 0.f ? 0.f : 180.f, 0.f), P) * Frame);
					continue;
				}
				// Mast arm over the avenue lanes on this corner's side, with a signal head.
				Box(FrameMaterial, P + FVector(0.f, -Sy * 250.f, 500.f), FVector(10.f, 500.f, 10.f), None);
				const FVector Head = P + FVector(0.f, -Sy * 420.f, 440.f);
				Box(SignalMaterial, Head, FVector(34.f, 32.f, 100.f), None);
				int32 Lit = 0;
				for (UMaterialInterface* LightMat : { AwningMaterials[0].Get(), AwningMaterials[4 % AwningMaterials.Num()].Get(), AwningMaterials[1 % AwningMaterials.Num()].Get() })
				{
					Sphere(LightMat, Head + FVector(Sx * 17.f, 0.f, 30.f - Lit * 30.f), FVector(20.f, 20.f, 20.f), NoShadow);
					++Lit;
				}
				Box(SignalMaterial, P + FVector(0.f, 0.f, 330.f), FVector(28.f, 26.f, 80.f), None);
			}
		}
	}
}

TArray<FVector> AIronCityStreet::GetRoadSpawnPoints() const
{
	TArray<FVector> Local;
	const float Z = RoadZ + 110.f;
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;
	for (const FVector2D& R : CutRanges(-HalfLength + 400.f, HalfLength - 400.f, CrossStreets, RoadHalfWidth + 200.f))
	{
		for (float X = R.X; X <= R.Y; X += 900.f)
		{
			Local.Add(FVector(X, -RoadHalfWidth * 0.5f, Z));
			Local.Add(FVector(X, RoadHalfWidth * 0.5f, Z));
		}
	}
	for (float Cx : CrossStreets)
	{
		for (float Y = BuildingLine() + 600.f; Y <= RingInner - 400.f; Y += 900.f)
		{
			Local.Add(FVector(Cx, Y, Z));
			Local.Add(FVector(Cx, -Y, Z));
		}
	}
	TArray<FVector> World;
	for (const FVector& P : Local)
	{
		World.Add(GetActorTransform().TransformPosition(P));
	}
	return World;
}

TArray<FIronRoadSegment> AIronCityStreet::GetRoadSegments() const
{
	TArray<FIronRoadSegment> Out;
	const FTransform& T = GetActorTransform();
	auto Add = [&](const FVector2D& A, const FVector2D& B, float Width)
	{
		FIronRoadSegment Seg;
		const FVector WA = T.TransformPosition(FVector(A, 0.f));
		const FVector WB = T.TransformPosition(FVector(B, 0.f));
		Seg.A = FVector2D(WA.X, WA.Y);
		Seg.B = FVector2D(WB.X, WB.Y);
		Seg.Width = Width;
		Out.Add(Seg);
	};
	const float RingInner = RingRoadCenter > 0.f ? RingRoadCenter - RingRoadHalfWidth : HalfLength;
	Add(FVector2D(-HalfLength, 0.f), FVector2D(HalfLength, 0.f), 2.f * RoadHalfWidth);
	for (float Cx : CrossStreets)
	{
		Add(FVector2D(Cx, -RingInner), FVector2D(Cx, RingInner), 2.f * RoadHalfWidth);
	}
	if (RingRoadCenter > 0.f)
	{
		const float C = RingRoadCenter, W = 2.f * RingRoadHalfWidth;
		Add(FVector2D(-C, -C), FVector2D(C, -C), W);
		Add(FVector2D(C, -C), FVector2D(C, C), W);
		Add(FVector2D(C, C), FVector2D(-C, C), W);
		Add(FVector2D(-C, C), FVector2D(-C, -C), W);
	}
	return Out;
}
