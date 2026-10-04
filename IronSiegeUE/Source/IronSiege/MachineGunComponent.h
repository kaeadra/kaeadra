#pragma once
#include "CoreMinimal.h"
#include "VehicleWeaponComponent.h"
#include "MachineGunComponent.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UIronPuffEmitter;

// Hitscan weapon: instant line trace, no travel time for the damage. What you see is drawn from
// small fixed pools owned by this component (nothing spawned per shot at 12 rounds a second):
// tracers that fly out along the shot, a muzzle flash that changes shape every round, sparks off
// metal and dust off the ground, and brass kicked out of the side of the gun.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UMachineGunComponent : public UVehicleWeaponComponent
{
	GENERATED_BODY()

public:
	UMachineGunComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec) override;

public:
	// Barrel heat 0..1 (IronWeapons::Loadout), for the HUD.
	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	float GetHeat() const { return Loadout.Heat; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	bool IsOverheated() const { return Loadout.Overheated; }

	// Current cone half-angle in degrees - the HUD sizes the crosshair from it.
	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	float GetSpreadDeg() const { return Loadout.SpreadDeg(); }

	// Where tracers and the muzzle flash start (the autocannon's barrel tip). The shot itself is
	// still traced from the gameplay muzzle so aiming does not change.
	void SetVisualMuzzle(USceneComponent* Muzzle) { VisualMuzzle = Muzzle; }

	// Trigger held with nothing to fire (empty, overheated, reloading): a dry click, now and then.
	void DryFire();

	// Metal ping when a round lands on a vehicle, and a ricochet off everything else.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> ImpactSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_BulletImpact.S_BulletImpact")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TArray<TSoftObjectPtr<class USoundBase>> RicochetSounds;

	// Near-miss whiz heard by the player when an enemy round passes close.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TArray<TSoftObjectPtr<class USoundBase>> FlybySounds;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> DryFireSound;

	// Steam hiss when the barrel locks up.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> OverheatSound;

	// Whirr of the barrels running down after a burst.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> SpinDownSound;

private:
	void EnsureFx();
	void LaunchTracer(const FVector& Start, const FVector& End);
	void ShowImpact(const FHitResult& Hit, bool bVehicle, const FVector& Direction);
	void EjectCasing();
	void PlayFlybyForPlayer(const FVector& Start, const FVector& End, const AActor* HitActor);

	struct FTracer
	{
		FVector Start = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		float Length = 0.f;
		float Age = -1.f;
	};
	struct FCasing
	{
		FVector Velocity = FVector::ZeroVector;
		FRotator Spin = FRotator::ZeroRotator;
		float Age = -1.f;
	};

	bool bFxReady = false;
	bool bTracerModelled = false;  // Which build of each pooled effect is in use (modelled mesh or basic shape).
	bool bFlashModelled = false;
	bool bCasingModelled = false;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TracerMeshes;
	TArray<FTracer> Tracers;
	int32 NextTracer = 0;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlashMesh;
	float FlashTimeLeft = 0.f;

	// A split-second light at the muzzle per round: lights the car and the road around it.
	UPROPERTY()
	TObjectPtr<UPointLightComponent> MuzzleFlash;

	UPROPERTY()
	TObjectPtr<UIronPuffEmitter> Sparks;
	UPROPERTY()
	TObjectPtr<UIronPuffEmitter> Dust;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CasingMeshes;
	TArray<FCasing> Casings;
	int32 NextCasing = 0;

	int32 ShotCount = 0;
	double LastShotTime = -10.0;
	double LastDryFireTime = -10.0;
	double LastFlybyTime = -10.0;
	bool bWasOverheated = false;
	bool bSpinningDown = true;
	TWeakObjectPtr<USceneComponent> VisualMuzzle;
};
