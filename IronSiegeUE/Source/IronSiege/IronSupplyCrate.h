#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronSupplyCrate.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EIronSupplyType : uint8
{
	Repair,
	Ammo
};

// Between-wave supply drop (see IronWaves::SuppliesAfterWave): a floating crate with a tall
// light beacon. Driving the player's car within PickupRadius collects it - a repair kit restores
// health/armor (IronDamage::Repair), an ammo crate tops up both weapons' reserves
// (IronWeapons::Loadout::AddReserve). A crate the player cannot use right now (already full)
// stays put. Collection is a distance check in Tick rather than an overlap event, so it works
// with the Chaos cars' physics body regardless of its overlap settings.
UCLASS()
class IRONSIEGE_API AIronSupplyCrate : public AActor
{
	GENERATED_BODY()

public:
	AIronSupplyCrate();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	EIronSupplyType SupplyType = EIronSupplyType::Repair;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	float RepairHealth = 60.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	float RepairArmor = 40.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	int32 MachineGunRounds = 200;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	int32 Rockets = 4;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	float PickupRadius = 320.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	TSoftObjectPtr<UMaterialInterface> RepairMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/MI_Supply_Repair.MI_Supply_Repair")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	TSoftObjectPtr<UMaterialInterface> AmmoMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/MI_Supply_Ammo.MI_Supply_Ammo")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Supply")
	TSoftObjectPtr<class USoundBase> PickupSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Pickup.S_Pickup")));

	// Short HUD label, e.g. for the on-screen marker.
	FString GetLabel() const { return SupplyType == EIronSupplyType::Repair ? TEXT("REPAIR") : TEXT("AMMO"); }

private:
	// Applies the supply to the player's vehicle; returns the notice text, empty if nothing applied.
	FString TryApply(APawn* PlayerPawn) const;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Supply")
	TObjectPtr<UStaticMeshComponent> Crate;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Supply")
	TObjectPtr<UStaticMeshComponent> Beacon;

	FVector BaseLocation;
	float Age = 0.f;
};
