#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronCityStreet.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

// A road centre line (world space, XY) and its full width, for maps/minimaps.
USTRUCT(BlueprintType)
struct FIronRoadSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "IronSiege|City")
	FVector2D A = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "IronSiege|City")
	FVector2D B = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "IronSiege|City")
	float Width = 0.f;
};

// Procedural modern-downtown street kit: a main avenue along X with cross streets and a ring road,
// raised sidewalks with curbs, lane markings and crosswalks, street furniture, and rows of
// buildings (glass curtain-wall towers, punched-window blocks, ribbon-window blocks) with
// storefronts and awnings.
//
// Everything is built from engine basic shapes into a handful of hierarchical instanced mesh
// pools (one per mesh+material), so the whole street costs a few dozen draw calls. The pools are
// created as saved instance components by Rebuild() (run from the editor / Python), not in the
// construction script, so a cooked or -game run just loads them. Layout is deterministic per Seed.
UCLASS()
class IRONSIEGE_API AIronCityStreet : public AActor
{
	GENERATED_BODY()

public:
	AIronCityStreet();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "IronSiege|City")
	void Rebuild();

	// Road centre-line points suitable for spawning vehicles, in world space.
	UFUNCTION(BlueprintPure, Category = "IronSiege|City")
	TArray<FVector> GetRoadSpawnPoints() const;

	// Every carriageway (avenue, cross streets, ring road) as world-space centre-line segments.
	UFUNCTION(BlueprintPure, Category = "IronSiege|City")
	TArray<FIronRoadSegment> GetRoadSegments() const;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	int32 Seed = 7;

	// Main avenue runs along X from -HalfLength to +HalfLength (where it meets the ring road).
	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float HalfLength = 7300.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float RoadHalfWidth = 700.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float SidewalkWidth = 500.f;

	// X positions of the cross streets.
	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	TArray<float> CrossStreets = { -3000.f, 3200.f };

	// Ring road centre distance from the origin (square loop), 0 disables it.
	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float RingRoadCenter = 7800.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float RingRoadHalfWidth = 500.f;

	// Z of the road surface; the sidewalk sits CurbHeight above it.
	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float RoadZ = -12.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float CurbHeight = 15.f;

	// Material overrides; any left empty fall back to the /Game/IronSiege/City/Materials defaults.
	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> PavingMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> CurbMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TArray<TObjectPtr<UMaterialInterface>> FacadeMaterials;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> FrameMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> SilverMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TArray<TObjectPtr<UMaterialInterface>> AwningMaterials;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> LineWhiteMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> LineYellowMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> FoliageMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> FoliageDarkMaterial;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> BarkMaterial;

	// Backdrop towers beyond the play area's perimeter walls, so every street ends in more city
	// instead of a blank wall. Distance of their street-facing line from the origin; 0 disables.
	UPROPERTY(EditAnywhere, Category = "IronSiege|City")
	float SkylineLine = 9400.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|City|Materials")
	TObjectPtr<UMaterialInterface> SignalMaterial;

private:
	enum class EShape : uint8 { Cube, Cylinder, Sphere };
	// Flags for a pool: whether instances block vehicles/weapons, and whether they cast shadows.
	enum EPoolFlags : uint8 { None = 0, Collide = 1, NoShadow = 2, GroundOnly = 4 };

	UHierarchicalInstancedStaticMeshComponent* Pool(EShape Shape, UMaterialInterface* Material, uint8 Flags);
	// The pool for any mesh; Material null leaves the mesh its own materials.
	UHierarchicalInstancedStaticMeshComponent* MeshPool(UStaticMesh* Mesh, UMaterialInterface* Material, uint8 Flags);

	// Axis-aligned-in-local-space box: Center/Size in cm, Yaw in degrees around the box centre.
	void Box(UMaterialInterface* Material, const FVector& Center, const FVector& Size, uint8 Flags = None, const FRotator& Rot = FRotator::ZeroRotator);
	void Cylinder(UMaterialInterface* Material, const FVector& Center, float Diameter, float Height, uint8 Flags = None);
	void Sphere(UMaterialInterface* Material, const FVector& Center, const FVector& Size, uint8 Flags = None);

	void ResolveMaterials();
	void BuildRoads();
	void BuildMarkings();
	void BuildSidewalks();
	void BuildBuildingRows();
	void BuildBuilding(float X0, float X1, float Side, float Depth, bool bExposedLeft, bool bExposedRight, bool bSkyline = false);
	void BuildSkyline();
	void BuildRingTrees();
	void BuildStorefronts(float X0, float X1, float Side, float FrontY, bool bAwnings);
	void BuildStreetFurniture();
	void BuildTrafficLights();

	void Lamp(const FVector& Base, float Side);
	void Tree(const FVector& Base);

	// Main-street X ranges between cross streets (and ring road ends), excluding intersections.
	TArray<FVector2D> MainBlocks() const;
	float SidewalkTop() const { return RoadZ + CurbHeight; }
	float BuildingLine() const { return LineOverride > 0.f ? LineOverride : RoadHalfWidth + SidewalkWidth; }

	// While building the skyline: every instance is placed through Frame (rotating a row to face
	// each side of the map), BuildingLine() is moved out to the skyline, and shadows are dropped
	// so distant towers never darken the arena.
	FTransform Frame = FTransform::Identity;
	float LineOverride = -1.f;
	bool bForceNoShadow = false;

	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Pools;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	// Modelled tree crowns (create_nature_fx.py); empty until imported, and the trees then keep
	// their crowns of spheres.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> TreeCrowns;

	// Modelled building parts by name (SM_Bld_*, create_building_assets.py): windows in their
	// surrounds, balconies, roof plant, fire escapes. A part that is not imported is null, and the
	// building falls back to the boxes it used before.
	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> Parts;
	UStaticMesh* Part(const TCHAR* Name) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	FRandomStream Rng;
};
