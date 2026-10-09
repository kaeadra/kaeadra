#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionRules.h"
#include "IronMissionActors.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UIronPuffEmitter;

// A structure a mission stands on the battlefield (IronMissions::StructDepot..StructRelay): a Legion
// fuel depot, radar mast or shield generator to destroy, or our own uplink relay to defend. Built
// from engine basic shapes in the kit finishes, like the war kits. One solid hull takes the hits;
// the rest is dressing. Only the other side can damage it. Destroyed, it blows up and is left as a
// charred, smoking wreck.
UCLASS()
class IRONSIEGE_API AIronMissionTarget : public AActor
{
	GENERATED_BODY()

public:
	AIronMissionTarget();

	static AIronMissionTarget* Spawn(UWorld* World, int32 InStructure, const FVector& GroundLocation, float Yaw);

	UPROPERTY()
	int32 Structure = IronMissions::StructDepot;

	// Ours (the relay): the Legion attacks it and the player cannot hurt it.
	UPROPERTY()
	bool bFriendly = false;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsDestroyed() const { return Health <= 0.f; }
	float GetHealthFraction() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }

	// Above the roof, where the HUD hangs its marker and health bar.
	FVector GetMarkerLocation() const { return GetActorLocation() + FVector(0.f, 0.f, TopZ + 140.f); }

protected:
	virtual void BeginPlay() override;

private:
	UStaticMeshComponent* AddPart(int32 Shape, int32 Finish, const FVector& Location, const FRotator& Rotation, const FVector& SizeCm, bool bHull = false);
	void Die();

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	// Dish, mast, core: gone once the structure is destroyed.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Fragile;

	// The radar dish / relay dish turning, or the generator's core.
	UPROPERTY()
	TObjectPtr<USceneComponent> Spinner;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY()
	TObjectPtr<UIronPuffEmitter> Smoke;

	float Health = 1.f;
	float MaxHealth = 1.f;
	float TopZ = 600.f;
	float Age = 0.f;
	float SmokeTimer = 0.f;
	float WreckSeconds = 0.f;
};

// A ring on the ground the player has to drive into. As a capture zone it fills while the player is
// inside and no enemy is (IronMissions::TickCapture); as a waypoint it is taken the moment the
// player arrives. Glowing posts round the edge and a tall beacon show where it is and whose it is.
UCLASS()
class IRONSIEGE_API AIronCaptureZone : public AActor
{
	GENERATED_BODY()

public:
	AIronCaptureZone();

	static AIronCaptureZone* Spawn(UWorld* World, const FVector& GroundLocation, bool bInCapture);

	virtual void Tick(float DeltaSeconds) override;

	bool IsTaken() const { return State.bCaptured; }
	float GetProgress() const { return State.Progress; }
	bool IsContested() const { return bContested; }

	UPROPERTY()
	bool bCapture = true;

	UPROPERTY()
	float Radius = 800.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Posts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Paint;

	IronMissions::CaptureState State;
	bool bContested = false;
	float Age = 0.f;
};
