#include "RocketProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "IronExplosion.h"
#include "IronPuffEmitter.h"
#include "IronVehicleKit.h"
#include "MissileRules.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
IronMissiles::Vec ToVec(const FVector& V) { return { float(V.X), float(V.Y), float(V.Z) }; }
FVector ToFVector(const IronMissiles::Vec& V) { return FVector(V.X, V.Y, V.Z); }
}

ARocketProjectile::ARocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(12.f);
	// "Projectile" is not a profile this project defines (it fell back to overlap-only, so rockets
	// flew through everything); block like any dynamic object instead.
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComponent;

	auto MakePart = [this](const TCHAR* Name, const FVector& Location, const FRotator& Rotation, const FVector& SizeCm) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeRotation(Rotation);
		Part->SetRelativeScale3D(SizeCm / 100.f);
		return Part;
	};
	// Flight direction is +X; cylinders and cones are Z-up, so pitch them -90 to lie along it.
	const FRotator AlongX(-90.f, 0.f, 0.f);
	VisualMesh = MakePart(TEXT("VisualMesh"), FVector::ZeroVector, AlongX, FVector(12.f, 12.f, 80.f));
	Nose = MakePart(TEXT("Nose"), FVector(47.f, 0.f, 0.f), AlongX, FVector(12.f, 12.f, 16.f));
	// Cross-shaped tail fins: two flat pairs, one spreading sideways and one up/down.
	Fins.Add(MakePart(TEXT("FinRight"), FVector(-32.f, 10.f, 0.f), FRotator::ZeroRotator, FVector(16.f, 10.f, 1.2f)));
	Fins.Add(MakePart(TEXT("FinLeft"), FVector(-32.f, -10.f, 0.f), FRotator::ZeroRotator, FVector(16.f, 10.f, 1.2f)));
	Fins.Add(MakePart(TEXT("FinUp"), FVector(-32.f, 0.f, 10.f), FRotator::ZeroRotator, FVector(16.f, 1.2f, 10.f)));
	Fins.Add(MakePart(TEXT("FinDown"), FVector(-32.f, 0.f, -10.f), FRotator::ZeroRotator, FVector(16.f, 1.2f, 10.f)));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlameFinder(TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft"));
	Exhaust = MakePart(TEXT("Exhaust"), FVector(-58.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(34.f, 13.f, 13.f));
	Exhaust->SetCastShadow(false);
	if (SphereFinder.Succeeded()) Exhaust->SetStaticMesh(SphereFinder.Object);
	if (FlameFinder.Succeeded()) Exhaust->SetMaterial(0, FlameFinder.Object);

	MotorGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("MotorGlow"));
	MotorGlow->SetupAttachment(RootComponent);
	MotorGlow->SetRelativeLocation(FVector(-70.f, 0.f, 0.f));
	MotorGlow->SetIntensity(6000.f);
	MotorGlow->SetAttenuationRadius(450.f);
	MotorGlow->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	MotorGlow->SetCastShadows(false);

	Trail = CreateDefaultSubobject<UIronPuffEmitter>(TEXT("Trail"));
	Trail->SetupAttachment(RootComponent);

	MotorSound = CreateDefaultSubobject<UAudioComponent>(TEXT("MotorSound"));
	MotorSound->SetupAttachment(RootComponent);
	MotorSound->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> LoopFinder(TEXT("/Game/IronSiege/Audio/S_MissileLoop.S_MissileLoop"));
	if (LoopFinder.Succeeded()) MotorSound->SetSound(LoopFinder.Object);

	const IronMissiles::Tuning Tuning;
	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->InitialSpeed = Tuning.LaunchSpeed;
	MovementComponent->MaxSpeed = Tuning.CruiseSpeed;
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->ProjectileGravityScale = 0.f;

	InitialLifeSpan = 8.f; // Self-destruct if nothing is hit (no explosion, matches an empty miss).

	CollisionComponent->OnComponentHit.AddDynamic(this, &ARocketProjectile::OnProjectileHit);
}

void ARocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	// The body parts take the kit finishes (white body, red nose, black fins) - loaded here rather
	// than in the constructor so the kit materials stay optional assets.
	if (UStaticMesh* Cylinder = IronKitBuilder::ShapeMesh(IronKits::Shape::Cylinder)) VisualMesh->SetStaticMesh(Cylinder);
	if (UStaticMesh* Cone = IronKitBuilder::ShapeMesh(IronKits::Shape::Cone)) Nose->SetStaticMesh(Cone);
	VisualMesh->SetMaterial(0, IronKitBuilder::FinishMaterial(IronKits::Finish::White));
	Nose->SetMaterial(0, IronKitBuilder::FinishMaterial(IronKits::Finish::Red));
	UStaticMesh* Cube = IronKitBuilder::ShapeMesh(IronKits::Shape::Cube);
	for (UStaticMeshComponent* Fin : Fins)
	{
		if (Cube) Fin->SetStaticMesh(Cube);
		Fin->SetMaterial(0, IronKitBuilder::FinishMaterial(IronKits::Finish::Black));
	}
	// The modelled missile (the same one the pod shows loaded), when imported: one mesh with its own
	// nose and fins, modelled along +X around its middle, a little larger in flight to read at range.
	if (UStaticMesh* Modelled = IronKitBuilder::RigMesh("SM_Rig_Missile"))
	{
		VisualMesh->SetStaticMesh(Modelled);
		VisualMesh->SetRelativeRotation(FRotator::ZeroRotator);
		VisualMesh->SetRelativeScale3D(FVector(1.35f));
		IronKitBuilder::ApplyFinishesBySlot(VisualMesh);
		Nose->SetVisibility(false);
		for (UStaticMeshComponent* Fin : Fins)
		{
			Fin->SetVisibility(false);
		}
	}
	Trail->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft")), 48);
	MotorSound->Play();
	if (ProjectileInstigatorActor)
	{
		CollisionComponent->IgnoreActorWhenMoving(ProjectileInstigatorActor, true);
	}
}

void ARocketProjectile::Launch(const FVector& Direction, float InDamage, float InExplosionRadius, AActor* InInstigatorActor, AController* InInstigatorController, AActor* InTarget)
{
	InstigatorController = InInstigatorController;
	Damage = InDamage;
	ExplosionRadius = InExplosionRadius;
	ProjectileInstigatorActor = InInstigatorActor;
	Target = InTarget;
	Heading = Direction.GetSafeNormal();
	MovementComponent->Velocity = Heading * IronMissiles::Speed(0.f);
	if (InInstigatorActor)
	{
		CollisionComponent->IgnoreActorWhenMoving(InInstigatorActor, true);
	}
}

void ARocketProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded)
	{
		return;
	}
	Age += DeltaSeconds;
	const IronMissiles::Tuning Tuning;

	// Guidance: off the rail first, then swing toward the target's centre at the capped rate.
	if (AActor* Locked = Target.Get(); Locked && IronMissiles::IsSeeking(Age, Tuning))
	{
		// Cars are aimed at above their road-level origin; a flare is the point itself.
		const bool bVehicle = Locked->IsA<APawn>();
		const FVector Aim = Locked->GetActorLocation() + FVector(0.f, 0.f, bVehicle ? 60.f : 0.f) - GetActorLocation();
		const float Rate = TurnRateDeg >= 0.f ? TurnRateDeg : Tuning.TurnRateDeg;
		Heading = ToFVector(IronMissiles::Steer(ToVec(Heading), ToVec(Aim), Rate * DeltaSeconds));
		// Proximity fuze for decoys: a flare has no collision, so the missile bursts on reaching it.
		if (!bVehicle && Aim.SizeSquared() < FMath::Square(400.f))
		{
			Explode();
			return;
		}
	}
	if (DecoyFuse >= 0.f)
	{
		DecoyFuse -= DeltaSeconds;
		if (DecoyFuse < 0.f)
		{
			Explode(); // Fooled and out of reach: burst in the air rather than circle the flares.
			return;
		}
	}
	MovementComponent->Velocity = Heading * IronMissiles::Speed(Age, Tuning);

	// Motor flame flickers; the glow breathes with it.
	const float Flicker = FMath::FRandRange(0.8f, 1.2f);
	Exhaust->SetRelativeScale3D(FVector(0.34f * Flicker, 0.13f, 0.13f));
	MotorGlow->SetIntensity(6000.f * Flicker);

	// Smoke trail, laid by time so it stays dense at any speed; puffs outlive the missile.
	TrailTimer -= DeltaSeconds;
	if (TrailTimer <= 0.f)
	{
		TrailTimer = 0.022f;
		FIronPuffStyle Smoke;
		Smoke.Life = 1.6f;
		Smoke.StartScale = 0.14f;
		Smoke.EndScale = 0.95f;
		Smoke.Drift = FVector(0.f, 0.f, 25.f);
		Smoke.StartColor = Smoke.EndColor = FLinearColor(0.2f, 0.2f, 0.21f);
		Smoke.PeakOpacity = 0.5f;
		Trail->Emit(GetActorLocation() - Heading * 75.f, FVector::ZeroVector, Smoke);
	}
}

void ARocketProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode();
}

void ARocketProjectile::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	UE_LOG(LogTemp, Log, TEXT("IronSiege: missile from %s burst at %s (homing on %s)"), ProjectileInstigatorActor ? *ProjectileInstigatorActor->GetName() : TEXT("?"),
		*GetActorLocation().ToCompactString(), Target.IsValid() ? *Target->GetName() : TEXT("nothing"));
	if (ExplosionRadius > 0.f)
	{
		TArray<AActor*> IgnoreActors;
		if (ProjectileInstigatorActor)
		{
			IgnoreActors.Add(ProjectileInstigatorActor);
		}
		// bDoFullDamage=false: let the engine fall off damage with distance to the radius edge,
		// matching the design in DamageRules.h's RadialFalloff (see Tests/damage_rules_test.cpp).
		UGameplayStatics::ApplyRadialDamage(this, Damage, GetActorLocation(), ExplosionRadius, UDamageType::StaticClass(), IgnoreActors, this, InstigatorController, false);
	}
	AIronExplosion::Spawn(GetWorld(), GetActorLocation(), 1.f, ExplosionRadius * 1.2f, 450.f, ProjectileInstigatorActor);

	// The missile itself is gone, but its smoke trail is left hanging to fade out.
	MovementComponent->StopMovementImmediately();
	MovementComponent->Deactivate();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (UStaticMeshComponent* Part : { VisualMesh.Get(), Nose.Get(), Exhaust.Get() })
	{
		Part->SetVisibility(false);
	}
	for (UStaticMeshComponent* Fin : Fins)
	{
		Fin->SetVisibility(false);
	}
	MotorGlow->SetVisibility(false);
	MotorSound->FadeOut(0.15f, 0.f);
	SetLifeSpan(1.7f);
}
