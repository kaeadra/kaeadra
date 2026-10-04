#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RocketProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class UIronPuffEmitter;

// Guided missile: a white body with a red nose and cross tail fins (engine basic shapes, kit
// finishes), a flickering motor flame and glow, a smoke trail that hangs in the air after it has
// gone, and a motor sound. It boosts off the rail and, when fired with a lock, steers toward its
// target at a capped turn rate (MissileRules.h, tested offline). On impact: radial damage and an
// AIronExplosion; the trail is left to fade out.
UCLASS()
class IRONSIEGE_API ARocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARocketProjectile();

	// Called by the firing weapon component right after spawning this actor. Target may be null
	// (unguided: flies straight along Direction).
	void Launch(const FVector& Direction, float InDamage, float InExplosionRadius, AActor* InInstigatorActor, AController* InInstigatorController, AActor* InTarget = nullptr);

	virtual void Tick(float DeltaSeconds) override;

	// What the missile is homing on (null: unguided), and whether it is still flying.
	AActor* GetTarget() const { return Target.Get(); }
	bool IsLive() const { return !bExploded; }

	// Countermeasures: switch the homing target (a flare pulls the missile off its car).
	// A decoyed missile bursts near its flare, or on a short fuse if it cannot catch it.
	void Redirect(AActor* NewTarget)
	{
		Target = NewTarget;
		DecoyFuse = 1.5f;
	}

	// EMP: the warhead is dead - the missile bursts where it is without its blast.
	void Fizzle()
	{
		ExplosionRadius = 0.f;
		Explode();
	}

	// Degrees per second the nose may swing; enemy launchers fire lazier missiles than the player's.
	void SetTurnRate(float DegreesPerSecond) { TurnRateDeg = DegreesPerSecond; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Explode();

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UProjectileMovementComponent> MovementComponent;

	// Body, nose and fins - hidden together when the missile detonates.
	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TArray<TObjectPtr<UStaticMeshComponent>> Fins;

	// Motor flame, its light, the smoke trail and the motor roar.
	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UStaticMeshComponent> Exhaust;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UPointLightComponent> MotorGlow;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UIronPuffEmitter> Trail;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Projectile")
	TObjectPtr<UAudioComponent> MotorSound;

	UPROPERTY()
	TObjectPtr<AController> InstigatorController;

	UPROPERTY()
	TObjectPtr<AActor> ProjectileInstigatorActor;

	TWeakObjectPtr<AActor> Target;
	FVector Heading = FVector::ForwardVector;
	float Age = 0.f;
	float TrailTimer = 0.f;
	float TurnRateDeg = -1.f; // Negative: IronMissiles::Tuning's default.
	float DecoyFuse = -1.f;   // Seconds left once pulled onto a flare; negative when not decoyed.
	float Damage = 0.f;
	float ExplosionRadius = 0.f;
	bool bExploded = false;
};
