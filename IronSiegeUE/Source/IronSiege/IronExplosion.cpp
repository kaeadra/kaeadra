#include "IronExplosion.h"
#include "IronPuffEmitter.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"

AIronExplosion::AIronExplosion()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = Duration + 0.2f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Puffs = CreateDefaultSubobject<UIronPuffEmitter>(TEXT("Puffs"));
	Puffs->SetupAttachment(Root);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Root);
	Flash->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Flash->SetAttenuationRadius(2500.f);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0.f);
}

AIronExplosion* AIronExplosion::Spawn(UWorld* World, const FVector& Location, float Scale, float ImpulseRadius, float ImpulseStrength, AActor* IgnoreActor)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIronExplosion* Explosion = World->SpawnActor<AIronExplosion>(AIronExplosion::StaticClass(), FTransform(Location), Params);
	if (Explosion)
	{
		Explosion->EffectScale = Scale;
		if (ImpulseRadius > 0.f)
		{
			Explosion->ApplyImpulse(ImpulseRadius, ImpulseStrength, IgnoreActor);
		}
	}
	return Explosion;
}

void AIronExplosion::BeginPlay()
{
	Super::BeginPlay();
	Puffs->Init(SmokeMaterial.LoadSynchronous(), FirePuffs + Fragments + SmokePuffs + 2);
	if (USoundBase* Snd = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation());
	}
}

void AIronExplosion::ApplyImpulse(float Radius, float Strength, AActor* IgnoreActor)
{
	// Velocity-change impulse on every simulating body in range, falling off with distance, with an
	// upward bias so cars lift and tumble rather than just slide.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllDynamicObjects);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronExplosionImpulse), false, IgnoreActor);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);
	TSet<UPrimitiveComponent*> Done;
	for (const FOverlapResult& O : Overlaps)
	{
		UPrimitiveComponent* Prim = O.GetComponent();
		if (!Prim || !Prim->IsSimulatingPhysics() || Done.Contains(Prim))
		{
			continue;
		}
		Done.Add(Prim);
		const FVector Delta = Prim->GetComponentLocation() - GetActorLocation();
		const float Falloff = FMath::Clamp(1.f - Delta.Size() / Radius, 0.f, 1.f);
		const FVector Dir = (Delta.GetSafeNormal2D() + FVector(0.f, 0.f, 0.8f)).GetSafeNormal();
		Prim->AddImpulse(Dir * Strength * Falloff, NAME_None, true);
		Prim->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 120.f * Falloff, NAME_None, true);
	}
}

void AIronExplosion::Burst()
{
	const float S = EffectScale;
	const FVector Here = GetActorLocation();
	// Fireball: a core and bulges thrown out from it, white-hot, cooling to dull red as they fade.
	FIronPuffStyle Fire;
	Fire.StartScale = 1.3f;
	Fire.EndScale = 3.6f;
	Fire.StartColor = FLinearColor(7.f, 3.2f, 0.7f);
	Fire.EndColor = FLinearColor(1.1f, 0.22f, 0.03f);
	Fire.PeakOpacity = 0.95f;
	Fire.bSwell = false;
	Fire.Drift = FVector(0.f, 0.f, 140.f);
	for (int32 i = 0; i < FirePuffs; ++i)
	{
		Fire.Life = FMath::FRandRange(0.5f, 0.75f);
		const FVector Dir = i == 0 ? FVector::ZeroVector : (FMath::VRand() * FVector(1.f, 1.f, 0.5f) + FVector(0.f, 0.f, 0.3f));
		Puffs->Emit(Here + Dir * 60.f * S, Dir * 330.f * S, Fire, (i == 0 ? 1.f : FMath::FRandRange(0.55f, 0.85f)) * S);
	}
	// Burning fragments thrown clear.
	FIronPuffStyle Fragment;
	Fragment.StartScale = 0.35f;
	Fragment.EndScale = 0.12f;
	Fragment.StartColor = FLinearColor(8.f, 4.f, 1.f);
	Fragment.EndColor = FLinearColor(2.f, 0.4f, 0.05f);
	Fragment.PeakOpacity = 1.f;
	Fragment.bSwell = false;
	Fragment.Drift = FVector(0.f, 0.f, -350.f);
	for (int32 i = 0; i < Fragments; ++i)
	{
		Fragment.Life = FMath::FRandRange(0.45f, 0.8f);
		const FVector Dir = (FMath::VRand() * FVector(1.f, 1.f, 0.6f) + FVector(0.f, 0.f, 0.6f)).GetSafeNormal();
		Puffs->Emit(Here, Dir * FMath::FRandRange(900.f, 1600.f) * S, Fragment, S);
	}
}

void AIronExplosion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float S = EffectScale;
	if (!bBurst)
	{
		bBurst = true;
		Burst();
	}
	// Smoke: one puff after another out of the dying fireball, drifting out and rolling upward.
	while (SmokeSent < SmokePuffs && Age >= 0.08f + 0.07f * SmokeSent)
	{
		FIronPuffStyle Smoke;
		Smoke.Life = FMath::FRandRange(1.6f, 2.1f);
		Smoke.StartScale = 1.5f;
		Smoke.EndScale = 4.4f;
		Smoke.StartColor = FLinearColor(0.035f, 0.03f, 0.028f);
		Smoke.EndColor = FLinearColor(0.11f, 0.105f, 0.1f);
		Smoke.PeakOpacity = 0.85f;
		Smoke.Drift = FVector(0.f, 0.f, 230.f * S);
		const FVector Flat = (FMath::VRand() * FVector(1.f, 1.f, 0.f)).GetSafeNormal() * FMath::FRandRange(0.3f, 1.f);
		Puffs->Emit(GetActorLocation() + FVector(0.f, 0.f, 80.f * S) + Flat * 60.f * S, Flat * 130.f * S, Smoke, FMath::FRandRange(0.7f, 1.f) * S);
		++SmokeSent;
	}

	Flash->SetIntensity(400000.f * S * FMath::Exp(-Age / 0.12f));
}
