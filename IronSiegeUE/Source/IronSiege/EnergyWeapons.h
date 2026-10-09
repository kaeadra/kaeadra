#pragma once
#include "CoreMinimal.h"
#include "VehicleWeaponComponent.h"
#include "EnergyWeapons.generated.h"

class UIronBeamFx;
class UPointLightComponent;

// Railgun (shop unlock): the car charges for a moment (IronBeams::ChargeClock, run by the pawn),
// then a slug leaves the cannon and goes through every car on its line - each one it passes
// through takes a little less (IronBeams::PiercedTargets / PierceDamage). The beam is cut short by
// walls and buildings. A cyan beam with a white core hangs in the air for a moment.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API URailgunComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	URailgunComponent()
	{
		WeaponType = EIronWeaponType::Railgun;
		FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Railgun.S_Railgun")));
	}

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	bool bUnlocked = false;

	// Visual start of the beam (the roof cannon's barrel tip).
	void SetVisualMuzzle(USceneComponent* Muzzle) { VisualMuzzle = Muzzle; }

	// Charge glow at the muzzle, 0..1 (0 hides it).
	void SetChargeProgress(float Progress);

	// Rising whine played when the charge starts (the shot's crack is FireSound).
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<USoundBase> ChargeSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_RailCharge.S_RailCharge")));

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;

private:
	UPROPERTY()
	TObjectPtr<UIronBeamFx> Beam;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> ChargeGlow;

	TWeakObjectPtr<USceneComponent> VisualMuzzle;
};

// Tesla coil (shop unlock): a lightning bolt to the nearest enemy in front, which then jumps to
// up to three more nearby (IronBeams::BuildChain). Every car struck loses its engine for a moment
// (AWarVehiclePawn::Stun) and each jump hits weaker.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UTeslaComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	UTeslaComponent()
	{
		WeaponType = EIronWeaponType::Tesla;
		FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Tesla.S_Tesla")));
	}

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	bool bUnlocked = false;

	void SetVisualMuzzle(USceneComponent* Muzzle) { VisualMuzzle = Muzzle; }

	// Number of cars the last discharge struck (for the HUD and tests).
	int32 GetLastChainLength() const { return LastChain; }

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;

private:
	UPROPERTY()
	TObjectPtr<UIronBeamFx> Bolts;

	TWeakObjectPtr<USceneComponent> VisualMuzzle;
	int32 LastChain = 0;
};
