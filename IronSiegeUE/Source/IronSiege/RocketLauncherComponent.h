#pragma once
#include "CoreMinimal.h"
#include "VehicleWeaponComponent.h"
#include "RocketLauncherComponent.generated.h"

class ARocketProjectile;

// Spawns an ARocketProjectile per shot. Assign ProjectileClass to a Blueprint child of
// ARocketProjectile once a rocket mesh/FX exist; falls back to the bare C++ class otherwise.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API URocketLauncherComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	URocketLauncherComponent()
	{
		WeaponType = EIronWeaponType::RocketLauncher;
		FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_RocketLaunch.S_RocketLaunch")));
	}

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSubclassOf<ARocketProjectile> ProjectileClass;

	// Set just before TryFire: missiles leave from the pod on the roof (converging on the gun's aim
	// line further out, so an unguided shot still flies where the crosshair points), and home on
	// Target if the car had a lock.
	void SetLaunchPoint(const FVector& InLaunchPoint, AActor* InTarget)
	{
		LaunchPoint = InLaunchPoint;
		bHasLaunchPoint = true;
		LockedTarget = InTarget;
	}

	// Guided missiles' turn rate relative to the player's (MissileRules.h); the enemy hunters fire
	// with about half, so a hard swerve at close range can still shake one off.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	float GuidedTurnScale = 1.f;

private:
	FVector LaunchPoint = FVector::ZeroVector;
	bool bHasLaunchPoint = false;
	TWeakObjectPtr<AActor> LockedTarget;

protected:

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;
};
