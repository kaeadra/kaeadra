#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "IronPuffEmitter.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// How one puff lives: how long, how it grows, which way it drifts, and how its colour and opacity
// change. Each puff keeps its own copy, so one emitter can mix styles (e.g. smoke and flame).
struct FIronPuffStyle
{
	float Life = 0.7f;
	float StartScale = 0.3f;   // Basic-shape scale (1 = 100 cm sphere).
	float EndScale = 1.1f;
	FVector Drift = FVector(0.f, 0.f, 150.f); // cm/s, added to the launch velocity (smoke rises).
	FLinearColor StartColor = FLinearColor(0.07f, 0.068f, 0.072f);
	FLinearColor EndColor = FLinearColor(0.07f, 0.068f, 0.072f);
	float PeakOpacity = 0.75f;
	bool bSwell = true;        // true: fade in then out (smoke); false: start solid and fade (flame).
};

// A fixed pool of puffs reused round-robin: tyre smoke, dust, engine damage smoke and flame, sparks
// and the flamethrower jet all come from one of these. No particle system, no spawned actors -
// Emit() just recycles the oldest puff. A puff is a card that faces the camera and shows one frame
// of a smoke sheet (M_FX_Puff, built by create_nature_fx.py), turned, mirrored and started on a
// different frame each time and billowing through the sheet as it ages; until that material is
// imported, a soft translucent sphere (the Material given to Init).
UCLASS(ClassGroup = (IronSiege))
class IRONSIEGE_API UIronPuffEmitter : public USceneComponent
{
	GENERATED_BODY()

public:
	UIronPuffEmitter();

	// Builds the pool. Call once (BeginPlay); Material is the sphere fallback's and should be a child
	// of M_FX_Soft so it has Color and Opacity parameters.
	void Init(UMaterialInterface* Material, int32 PoolSize);

	void Emit(const FVector& WorldLocation, const FVector& Velocity, const FIronPuffStyle& Style, float SizeScale = 1.f);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FPuff
	{
		float Age = -1.f; // Negative = free.
		FVector Velocity = FVector::ZeroVector;
		float SizeScale = 1.f;
		FIronPuffStyle Style;
		// Cards only: turn about the view axis (degrees), mirrored or not, and the first sheet frame.
		float Roll = 0.f;
		float Flip = 1.f;
		float Frame = 0.f;
	};

	// Sizes puff Index and, for a card, turns it to the camera.
	void Place(int32 Index, float Scale);
	void UpdateFacing();

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TArray<FPuff> Puffs;
	int32 Next = 0;
	bool bCards = false;
	FQuat Facing = FQuat::Identity; // A card's rotation that faces the camera, upright.
};
