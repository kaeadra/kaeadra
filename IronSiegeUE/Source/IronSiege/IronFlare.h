#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronFlare.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UIronPuffEmitter;

// One countermeasure flare: a blinding ball of burning magnesium thrown off the car, arcing down
// under light gravity and trailing white smoke for IronFlares::Tuning::FlareSeconds. Guided missiles
// that bite on it steer at it instead of the car (see AWarVehiclePawn::FireFlares).
UCLASS()
class IRONSIEGE_API AIronFlare : public AActor
{
	GENERATED_BODY()

public:
	AIronFlare();

	static AIronFlare* Spawn(UWorld* World, const FVector& Location, const FVector& Velocity);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Flare")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Flare")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Flare")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Flare")
	TObjectPtr<UIronPuffEmitter> Trail;

	float Age = 0.f;
	float TrailTimer = 0.f;
};
