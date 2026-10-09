#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IronVehicle.generated.h"

class UVehicleHealthComponent;
class UMachineGunComponent;
class URocketLauncherComponent;

UINTERFACE(BlueprintType)
class IRONSIEGE_API UIronVehicle : public UInterface
{
	GENERATED_BODY()
};

// Shared control surface for every drivable war car in the project, so AIronSiegePlayerController
// and AIronSiegeHUD do not need to know which concrete Pawn class they are driving. Implemented by
// AWarVehiclePawn (Chaos wheel physics on Epic's Vehicle Template cars).
class IRONSIEGE_API IIronVehicle
{
	GENERATED_BODY()

public:
	virtual void MoveForward(float Value) = 0;
	virtual void Steer(float Value) = 0;
	virtual void SetHandbrake(bool bEngaged) = 0;
	virtual void FirePrimary() = 0;
	virtual void FireSecondary() = 0;
	// Fires the primary weapon at a world point rather than along the vehicle's own aim - used by
	// the AI, which has no camera to aim with. Falls back to plain FirePrimary where not overridden.
	virtual void FirePrimaryAt(const FVector& TargetLocation) { FirePrimary(); }
	// Drops a proximity mine behind the car, if the mine rack has been bought (shop upgrade).
	virtual void DeployMine() {}

	// One tick of flame while the trigger is held, if the flame tank has been bought.
	virtual void FireFlamer() {}

	// Throws a burst of countermeasure flares that pull nearby guided missiles off the car.
	virtual void FireFlares() {}

	// Shop energy weapons: the railgun charges then fires a piercing slug; the tesla coil chains
	// lightning between nearby enemies. Both do nothing until bought.
	virtual void FireRailgun() {}
	virtual void FireTesla() {}

	virtual void ReloadPrimary() = 0;
	virtual void ReloadSecondary() = 0;
	virtual void ToggleCameraView() = 0;
	virtual float GetSpeedKph() const = 0;
	// World point the primary weapon is currently aimed at (drives the HUD crosshair).
	virtual FVector GetAimPoint() const { return FVector::ZeroVector; }
	virtual UVehicleHealthComponent* GetHealthComponent() const = 0;
	virtual UMachineGunComponent* GetPrimaryWeaponComponent() const = 0;
	virtual URocketLauncherComponent* GetSecondaryWeaponComponent() const = 0;

	// Null until the mine rack is bought; the HUD only shows mines when this has one.
	virtual class UMineLayerComponent* GetMineWeaponComponent() const { return nullptr; }

	// Null until the flame tank is bought.
	virtual class UFlamethrowerComponent* GetFlamerComponent() const { return nullptr; }
};
