#include "IronFlare.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "IronPuffEmitter.h"
#include "FlareRules.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AIronFlare::AIronFlare()
{
	PrimaryActorTick.bCanEverTick = true;

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	RootComponent = Core;
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);
	Core->SetRelativeScale3D(FVector(0.22f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlameFinder(TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft"));
	if (SphereFinder.Succeeded()) Core->SetStaticMesh(SphereFinder.Object);
	if (FlameFinder.Succeeded()) Core->SetMaterial(0, FlameFinder.Object);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Core);
	Glow->SetIntensity(30000.f);
	Glow->SetAttenuationRadius(1400.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.85f, 0.6f));
	Glow->SetCastShadows(false);

	Trail = CreateDefaultSubobject<UIronPuffEmitter>(TEXT("Trail"));
	Trail->SetupAttachment(Core);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.3f;
	Movement->bShouldBounce = false;
	Movement->Friction = 0.f;

	// Burns for FlareSeconds, then the actor lingers while its last smoke fades out.
	InitialLifeSpan = IronFlares::Tuning().FlareSeconds + 1.8f;
}

AIronFlare* AIronFlare::Spawn(UWorld* World, const FVector& Location, const FVector& Velocity)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIronFlare* Flare = World->SpawnActor<AIronFlare>(AIronFlare::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Flare)
	{
		Flare->Movement->Velocity = Velocity;
	}
	return Flare;
}

void AIronFlare::BeginPlay()
{
	Super::BeginPlay();
	Trail->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft")), 40);
	// Burning flares are hot white; the soft flame material takes the colour as HDR, so push it.
	Core->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(6.f, 5.f, 3.5f));
}

void AIronFlare::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Life = IronFlares::Tuning().FlareSeconds;
	// Magnesium sputters: the ball and its light flicker hard, and die down over the last second.
	const float Fade = FMath::Clamp(Life - Age, 0.f, 1.f);
	const float Flicker = FMath::FRandRange(0.7f, 1.3f);
	Core->SetRelativeScale3D(FVector(0.22f * Flicker * (0.4f + 0.6f * Fade)));
	Core->SetVisibility(Fade > 0.f);
	Glow->SetIntensity(30000.f * Flicker * Fade);

	TrailTimer -= DeltaSeconds;
	if (TrailTimer <= 0.f && Fade > 0.1f)
	{
		TrailTimer = 0.05f;
		FIronPuffStyle Smoke;
		Smoke.Life = 1.8f;
		Smoke.StartScale = 0.15f;
		Smoke.EndScale = 1.1f;
		Smoke.Drift = FVector(0.f, 0.f, 40.f);
		Smoke.StartColor = Smoke.EndColor = FLinearColor(0.55f, 0.55f, 0.56f);
		Smoke.PeakOpacity = 0.45f;
		Trail->Emit(GetActorLocation(), FVector::ZeroVector, Smoke);
	}
}
