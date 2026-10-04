#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronExplosion.generated.h"

class UIronPuffEmitter;
class UPointLightComponent;
class UMaterialInterface;
class USoundBase;

// Short-lived explosion effect (no particle assets needed), drawn with the same puffs as the rest
// of the game's smoke and fire (UIronPuffEmitter): a fireball that bursts outward and cools from
// white-hot to dull red, burning fragments thrown clear, then dark smoke that rolls up and
// spreads; plus a bright light flash and the explosion sound. Also shoves nearby physics bodies
// (Chaos cars, wrecks) outward.
// Spawn with AIronExplosion::Spawn(World, Location, Scale); it removes itself when done.
UCLASS()
class IRONSIEGE_API AIronExplosion : public AActor
{
	GENERATED_BODY()

public:
	AIronExplosion();

	// Scale 1 = rocket impact (~6m fireball), ~1.6 = vehicle destroyed. ImpulseRadius 0 = no shove.
	static AIronExplosion* Spawn(UWorld* World, const FVector& Location, float Scale, float ImpulseRadius, float ImpulseStrength, AActor* IgnoreActor = nullptr);

	virtual void Tick(float DeltaSeconds) override;

	// The puffs' look until the smoke sheet is imported (see UIronPuffEmitter::Init).
	UPROPERTY(EditAnywhere, Category = "IronSiege|FX")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|FX")
	TSoftObjectPtr<USoundBase> Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Explosion.S_Explosion")));

protected:
	virtual void BeginPlay() override;

private:
	void ApplyImpulse(float Radius, float Strength, AActor* IgnoreActor);

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|FX")
	TObjectPtr<USceneComponent> Root;

	// The fireball and fragments, all at once (on the first tick: the scale is set after spawning).
	void Burst();

	static constexpr int32 FirePuffs = 5;
	static constexpr int32 Fragments = 6;
	static constexpr int32 SmokePuffs = 6;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|FX")
	TObjectPtr<UIronPuffEmitter> Puffs;

	bool bBurst = false;
	int32 SmokeSent = 0; // The smoke follows the fireball a puff at a time.

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|FX")
	TObjectPtr<UPointLightComponent> Flash;

	float EffectScale = 1.f;
	float Age = 0.f;
	static constexpr float Duration = 2.8f; // Until the last smoke has faded.
};
