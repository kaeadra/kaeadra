#pragma once
#include "CoreMinimal.h"
#include "VehicleWeaponComponent.h"
#include "MineLayerComponent.generated.h"

class AIronMine;

// Drops an AIronMine on the road behind the car instead of shooting anything: the aim direction is
// ignored, the mine is placed at the tail and arms itself a moment later (MineRules.h).
// Bought from the upgrade shop (IronUpgrades::Upgrade::Mines), so it starts locked.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UMineLayerComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	UMineLayerComponent()
	{
		WeaponType = EIronWeaponType::MineLayer;
		FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Pickup.S_Pickup")));
	}

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSubclassOf<AIronMine> MineClass;

	// How far behind the car's origin the mine is dropped.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	float DropDistance = 280.f;

	// Locked until the mine rack is bought in the shop; TryFire does nothing while locked.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	bool bUnlocked = false;

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;
};
