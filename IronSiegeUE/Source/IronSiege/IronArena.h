#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronArena.generated.h"

namespace IronArena { struct Layout; }

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

// One named material slot of a modelled prop and what to dress it in.
struct FIronPropSlot
{
	const TCHAR* Name;
	UMaterialInterface* Material;
};

UENUM(BlueprintType)
enum class EIronArenaTheme : uint8
{
	Desert,
	Arctic,
	Port,
	City // Laid along the CityRuins street network (AIronCityStreet) instead of scattered.
};

// Turns a greybox map into a themed battlefield at load time: the layout comes from ArenaRules.h
// (tested offline) - a desert plateau with ramps and rock fields, a frozen lake ringed by ice
// spires, or a port's container maze - built from instanced meshes. The props are modelled
// (Tools/meshgen arena props, imported by Content/Python/import_arena_props.py): boulders, a mesa,
// cliffs along the perimeter walls, ice shards, fuel tanks, ramps, barriers, sandbags, tank traps,
// rubble and containers; each keeps a simple hidden shape as its collision, and until the meshes
// are imported those shapes are drawn instead. It also re-skins the ground and
// walls, clears the old placeholder cubes, and sets the theme's sun, fog, sky and post-process look.
// The game mode spawns one on the Desert, Arctic, Coast and CityRuins maps (AIronSiegeGameMode::BeginPlay);
// on the city map it turns the streets into a war zone - burning wrecks, barricades, sandbags, tank
// traps and rubble in a slalom that always leaves a lane open.
UCLASS()
class IRONSIEGE_API AIronArena : public AActor
{
	GENERATED_BODY()

public:
	AIronArena();

	UPROPERTY(EditAnywhere, Category = "IronSiege|Arena")
	EIronArenaTheme Theme = EIronArenaTheme::Desert;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Arena")
	int32 Seed = 7;

	// Theme for a map by name (Desert/Arctic/Coast/CityRuins); false for maps that keep their own dressing.
	static bool ThemeForMap(const FString& MapName, EIronArenaTheme& OutTheme);

	// Footprints of everything built that a car can hit: (X, Y, radius) in cm. The street AI weaves
	// round these (IronRoute::ChooseLane) instead of finding them with its bumper.
	const TArray<FVector>& GetObstacles() const { return Obstacles; }

	// Half the side of the walled square the arena was built in (cm, centred on the origin); 0 on the
	// city map, where the streets bound the play area instead.
	float GetHalfExtent() const { return PlayHalfExtent; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ClearPlaceholders(float& OutHalfExtent, float& OutGroundZ);
	void Build(float HalfExtent, float GroundZ);
	void BuildLayout(const IronArena::Layout& Layout, float GroundZ);
	void BuildCity();
	void BuildCliffs(float GroundZ);
	void ApplyLook();

	// One instanced mesh per (mesh, material) pair, created on demand. bUnseen: collision only
	// (the shape under a modelled prop).
	UInstancedStaticMeshComponent* Batch(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide = true, bool bUnseen = false);
	void AddBox(UMaterialInterface* Material, const FVector& Centre, const FRotator& Rotation, const FVector& SizeCm, bool bCollide = true, bool bUnseen = false);
	void AddShape(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Centre, const FRotator& Rotation, const FVector& SizeCm, bool bCollide = true, bool bUnseen = false);
	// A modelled prop: drawn only (its collision is a hidden shape), each named slot in its material.
	void AddProp(UStaticMesh* Mesh, const FTransform& Where, TConstArrayView<FIronPropSlot> Slots);

	UMaterialInterface* Solid(const FLinearColor& Color, float Roughness, float Metallic = 0.f);
	// Painted or bare steel with the kit's scanned grain, grime and scratches (M_Kit_Worn).
	UMaterialInterface* Worn(const FLinearColor& Color, float Metallic, float Roughness);
	UMaterialInterface* Textured(bool bFacade, const TCHAR* TextureSet, const FLinearColor& Tint, float TileSize, float Desaturation);

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	// Smoke and flames rising off the burning wrecks (city); each entry is one fire.
	UPROPERTY()
	TArray<TObjectPtr<class UIronPuffEmitter>> Fires;
	TArray<FVector> FireSpots;
	TArray<FVector> Obstacles;
	TArray<FBox> WallBoxes; // The perimeter walls found in the map, for the cliffs.
	float PlayHalfExtent = 0.f;
	float FireTimer = 0.f;
};
