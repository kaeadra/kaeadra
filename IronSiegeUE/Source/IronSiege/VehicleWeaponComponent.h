#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponRules.h"
#include "VehicleWeaponComponent.generated.h"

UENUM(BlueprintType)
enum class EIronWeaponType : uint8
{
	MachineGun,
	RocketLauncher,
	MineLayer,
	Flamethrower,
	Railgun,
	Tesla
};

// Base for a vehicle-mounted weapon. Ammo/cooldown/reload accounting lives in the engine-independent
// IronWeapons::Loadout (see WeaponRules.h, covered by Tests/weapon_rules_test.cpp); subclasses only
// need to implement what actually happens on a successful shot (DoFire).
UCLASS(Abstract, ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UVehicleWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleWeaponComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	EIronWeaponType WeaponType = EIronWeaponType::MachineGun;

	// Attempts to fire from Origin toward AimDirection. Returns false if on cooldown, reloading, or out of ammo.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Weapon")
	bool TryFire(const FVector& Origin, const FVector& AimDirection);

	UFUNCTION(BlueprintCallable, Category = "IronSiege|Weapon")
	bool StartReload();

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	int32 GetAmmo() const { return Loadout.Ammo; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	int32 GetReserve() const { return Loadout.Reserve; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Weapon")
	bool IsReloading() const { return Loadout.Reloading; }

	// Whether TryFire would succeed right now (ammo, cooldown, reload, heat) - for weapons that
	// charge up before the shot and must not start a charge they cannot finish.
	bool CanFire() const { return !Loadout.Reloading && !Loadout.Overheated && Loadout.Cooldown <= 1e-4f && Loadout.Ammo > 0; }

	// Ammo crate (IronWeapons::Loadout::AddReserve): capped reserve top-up; returns rounds added.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Weapon")
	int32 AddReserveAmmo(int32 Amount) { return Loadout.AddReserve(Amount); }

	// Scales every hit's damage (shop upgrades). 1 = stock.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	float DamageMultiplier = 1.f;

	// Played at the muzzle for every shot fired, with a little pitch variation so sustained fire
	// does not sound like one sample on repeat.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> FireSound;

	// Other takes of the same shot; each round picks one of these or FireSound at random, so a
	// burst never repeats one sample.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TArray<TSoftObjectPtr<class USoundBase>> FireSoundVariants;

	// The same shot recorded from a distance, played instead when the listener is further than
	// FarSoundDistance: a gun across the arena is a different sound, not only a quieter one.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TArray<TSoftObjectPtr<class USoundBase>> FireSoundFarVariants;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	float FarSoundDistance = 2000.f;

	// The sound if its asset exists (optional sounds are only there once imported), without the
	// load warning a missing asset would log on every shot.
	static class USoundBase* LoadIfPresent(const TSoftObjectPtr<class USoundBase>& Sound);

	// Mechanical clack when a reload starts.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Weapon")
	TSoftObjectPtr<class USoundBase> ReloadSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Reload.S_Reload")));

	const IronWeapons::Spec& WeaponSpec() const { return IronWeapons::Get(static_cast<IronWeapons::WeaponType>(WeaponType)); }

	// Vehicle class trait (VehicleClassRules.h): damage, rate of fire, barrel heat and reload time.
	void SetClassTraits(float DamageScale, float RateScale, float HeatScale, float ReloadScale)
	{
		ClassDamageMultiplier = DamageScale;
		Loadout.RateScale = RateScale;
		Loadout.HeatScale = HeatScale;
		Loadout.ReloadScale = ReloadScale;
	}

	// Driver abilities (CrewRules.h): a full magazine right now, and a cold barrel.
	void RefillMagazine()
	{
		Loadout.Ammo = WeaponSpec().Capacity;
		Loadout.Reloading = false;
		Loadout.ReloadRemaining = 0.f;
	}
	void ClearHeat()
	{
		Loadout.Heat = 0.f;
		Loadout.Overheated = false;
	}

	// What one hit does right now: the weapon's spec, the shop upgrades and the class trait.
	float ShotDamage(const IronWeapons::Spec& Spec) const { return Spec.Damage * DamageMultiplier * ClassDamageMultiplier; }

protected:
	// Called once TryFire has already confirmed ammo/cooldown/reload allow the shot.
	virtual void DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
	{
	}

	// Controller credited with this weapon's damage (the owning vehicle's driver, player or AI).
	AController* GetOwnerController() const;

	IronWeapons::Loadout Loadout;
	float ClassDamageMultiplier = 1.f;
};
