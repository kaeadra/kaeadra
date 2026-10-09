#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageRules.h"
#include "VehicleHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVehicleDestroyed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleDamaged, float, Damage, AController*, InstigatedBy);

// Wraps IronDamage::HealthPool (see DamageRules.h, covered by Tests/damage_rules_test.cpp) and hooks
// AActor::OnTakeAnyDamage so any existing damage-dealing code (traces, radial damage, Blueprint
// "Apply Damage") drives it without further wiring.
UCLASS(ClassGroup = (IronSiege), meta = (BlueprintSpawnableComponent))
class IRONSIEGE_API UVehicleHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleHealthComponent();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Health")
	float MaxHealth = 150.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Health")
	float MaxArmor = 100.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Health")
	float ArmorDamageReduction = 0.4f;

	// Overrides the EditAnywhere defaults above, e.g. from AWarVehiclePawn's selected vehicle class.
	void ConfigureFromClassStats(float InMaxHealth, float InMaxArmor, float InArmorDamageReduction);

	UFUNCTION(BlueprintPure, Category = "IronSiege|Health")
	float GetHealth() const { return State.Health; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Health")
	float GetArmor() const { return State.Armor; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Health")
	bool IsDestroyed() const { return State.Health <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Health")
	float GetMaxHealth() const { return State.MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Health")
	float GetMaxArmor() const { return State.MaxArmor; }

	// Scales max armor relative to the class base (armor plating upgrade); the added armor is
	// granted immediately. Scale 1 = stock.
	void SetArmorScale(float Scale);

	// Repair kit (IronDamage::Repair): capped at the maxima, no effect on a wreck. True if anything was restored.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Health")
	bool Repair(float HealthAmount, float ArmorAmount) { return IronDamage::Repair(State, HealthAmount, ArmorAmount); }

	UPROPERTY(BlueprintAssignable, Category = "IronSiege|Health")
	FOnVehicleDestroyed OnVehicleDestroyed;

	// Every hit that got through (after armor), with who dealt it - drives hit markers.
	UPROPERTY(BlueprintAssignable, Category = "IronSiege|Health")
	FOnVehicleDamaged OnVehicleDamaged;

	// Testing aid (UIronSiegeCheatManager::DebugSetHealth): armour off, health at a fraction of max,
	// without killing the car or going through the damage events.
	void SetHealthFractionForTest(float Fraction)
	{
		State.Armor = 0.f;
		State.Health = FMath::Max(1.f, State.MaxHealth * FMath::Clamp(Fraction, 0.f, 1.f));
	}

	// Testing aid (see UIronSiegeCheatManager::DebugGod): ignore all incoming damage.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Health")
	bool bInvulnerable = false;

	// Multiplies incoming damage before armor (a driver's Iron Wall drops it to a quarter). 1 = normal.
	float DamageTakenScale = 1.f;

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	IronDamage::HealthPool State;
	float BaseMaxArmor = 0.f;
	bool bDestroyedNotified = false;
};
