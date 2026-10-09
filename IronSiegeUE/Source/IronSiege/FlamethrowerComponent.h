#pragma once
#include "CoreMinimal.h"
#include "VehicleWeaponComponent.h"
#include "FlamethrowerComponent.generated.h"

// Short-range cone of fire: every tick of held trigger burns fuel, heats the nozzle (the same
// IronWeapons::Loadout heat lockout the machine gun uses, tuned to bite after ~2s) and damages
// every vehicle inside the cone. The flame itself is a pool of translucent puffs pushed out from
// the muzzle, so no particle assets are needed. Bought from the shop (IronUpgrades::Upgrade::Flamer).
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UFlamethrowerComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	UFlamethrowerComponent()
	{
		WeaponType = EIronWeaponType::Flamethrower;
		FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Boost.S_Boost")));
	}

	virtual void BeginPlay() override;

	// Locked until the flame tank is bought in the shop.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	bool bUnlocked = false;

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	float GetHeat() const { return Loadout.Heat; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	bool IsOverheated() const { return Loadout.Overheated; }

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;

private:
	// The flame jet: soft translucent puffs thrown out of the muzzle (UIronPuffEmitter).
	UPROPERTY()
	TObjectPtr<class UIronPuffEmitter> Jet;
};
