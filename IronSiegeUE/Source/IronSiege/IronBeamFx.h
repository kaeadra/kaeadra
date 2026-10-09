#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "IronBeamFx.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// Glowing line segments that fade out: the railgun's beam and the tesla coil's lightning. A small
// pool of cylinder meshes placed in world space, each with its own dynamic instance of the soft FX
// material (unlit, HDR colour, fading opacity), so no particle system or texture is needed.
UCLASS(ClassGroup = (IronSiege))
class IRONSIEGE_API UIronBeamFx : public USceneComponent
{
	GENERATED_BODY()

public:
	UIronBeamFx();

	void Init(int32 PoolSize);

	// A segment from A to B, ThicknessCm across, fading from Color over Life seconds.
	void DrawSegment(const FVector& A, const FVector& B, float ThicknessCm, const FLinearColor& Color, float Life);

	// A jagged bolt from A to B: Kinks segments with random sideways offsets, plus a thin bright core.
	void DrawBolt(const FVector& A, const FVector& B, int32 Kinks, float JitterCm, const FLinearColor& Color, float Life);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FSegment
	{
		float Age = -1.f; // Negative: free.
		float Life = 0.f;
		FLinearColor Color;
	};

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TArray<FSegment> Segments;
	int32 Next = 0;
};
