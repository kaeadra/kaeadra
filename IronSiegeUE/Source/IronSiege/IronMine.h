#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MineRules.h"
#include "IronMine.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

// A proximity mine dropped behind the car by UMineLayerComponent. It arms after a short delay (so
// the car that laid it can drive clear), blinks while it waits, detonates on the first enemy inside
// its trigger radius with a radial blast, and goes inert at the end of its life. The timing and
// radius live in MineRules.h, checked offline by Tests/mine_rules_test.cpp.
UCLASS()
class IRONSIEGE_API AIronMine : public AActor
{
	GENERATED_BODY()

public:
	AIronMine();

	virtual void Tick(float DeltaSeconds) override;

	// Damage and blast radius come from the weapon that dropped it (IronWeapons::Spec, scaled by
	// the shop's rocket-pod-style multiplier); Owner is credited with any kills.
	void Arm(float InDamage, float InRadius, AController* InInstigator, AActor* InOwnerVehicle);

	UPROPERTY(EditAnywhere, Category = "IronSiege|FX")
	TSoftObjectPtr<UMaterialInterface> BodyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/MI_FX_RocketBody.MI_FX_RocketBody")));

protected:
	virtual void BeginPlay() override;

private:
	void Detonate();

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Mine")
	TObjectPtr<UStaticMeshComponent> Body;

	// Blinks slowly while arming, urgently once live - the only warning an enemy (or you) gets.
	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Mine")
	TObjectPtr<UPointLightComponent> Beacon;

	UPROPERTY()
	TObjectPtr<AActor> OwnerVehicle;

	UPROPERTY()
	TObjectPtr<AController> InstigatorController;

	IronMines::Tuning Tuning;
	float Age = 0.f;
	float Damage = 150.f;
	float Radius = 550.f;
	bool bSpent = false;
};
