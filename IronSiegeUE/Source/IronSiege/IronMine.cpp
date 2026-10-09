#include "IronMine.h"
#include "IronExplosion.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "IronVehicleKit.h"
#include "Misc/PackageName.h"

AIronMine::AIronMine()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	// A squat disc on the tarmac: visible enough to dodge, low enough to drive over.
	if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		Body->SetStaticMesh(Cylinder);
	}
	Body->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.06f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Detonation is a distance check, not a hit.
	Body->SetCastShadow(false);

	Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Body);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Beacon->SetIntensity(2500.f);
	Beacon->SetAttenuationRadius(500.f);
	Beacon->SetLightColor(FLinearColor(1.f, 0.25f, 0.1f));
	Beacon->SetCastShadows(false);
}

void AIronMine::BeginPlay()
{
	Super::BeginPlay();
	// The modelled mine (Tools/meshgen Mine) in the kit's finishes once it is imported; the plain
	// disc until then.
	const TCHAR* Model = TEXT("/Game/IronSiege/Arena/Meshes/SM_Prop_Mine");
	UStaticMesh* Mine = FPackageName::DoesPackageExist(Model) ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IronSiege/Arena/Meshes/SM_Prop_Mine.SM_Prop_Mine")) : nullptr;
	if (Mine)
	{
		Body->SetStaticMesh(Mine);
		Body->SetRelativeScale3D(FVector(1.f));
		IronKitBuilder::ApplyFinishesBySlot(Body);
	}
	else if (UMaterialInterface* Material = BodyMaterial.LoadSynchronous())
	{
		Body->SetMaterial(0, Material);
	}
}

void AIronMine::Arm(float InDamage, float InRadius, AController* InInstigator, AActor* InOwnerVehicle)
{
	Damage = InDamage;
	Radius = InRadius;
	InstigatorController = InInstigator;
	OwnerVehicle = InOwnerVehicle;
}

void AIronMine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bSpent)
	{
		return;
	}
	Age += DeltaSeconds;

	// Blink: on for the first half of each period, off for the second.
	const float Period = IronMines::BlinkPeriod(Age, Tuning);
	const bool bLit = FMath::Fmod(Age, Period) < Period * 0.5f;
	Beacon->SetVisibility(bLit);

	if (IronMines::HasExpired(Age, Tuning))
	{
		Destroy();
		return;
	}
	if (!IronMines::IsArmed(Age, Tuning))
	{
		return;
	}

	// Nearest vehicle that is not the car that laid this mine: your own mines never trigger on you,
	// and never catch you in the blast either (see Detonate).
	float NearestCm = -1.f;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Other = *It;
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(Other);
		const UVehicleHealthComponent* OtherHealth = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (Other == OwnerVehicle || !OtherHealth || OtherHealth->IsDestroyed())
		{
			continue;
		}
		const float Distance = FVector::Dist(GetActorLocation(), Other->GetActorLocation());
		if (NearestCm < 0.f || Distance < NearestCm)
		{
			NearestCm = Distance;
		}
	}
	if (IronMines::ShouldDetonate(Age, NearestCm, Tuning))
	{
		Detonate();
	}
}

void AIronMine::Detonate()
{
	bSpent = true;
	UWorld* World = GetWorld();
	if (!World)
	{
		Destroy();
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mine detonated after %.1fs for %.0f damage in %.0f cm"), Age, Damage, Radius);
	AIronExplosion::Spawn(World, GetActorLocation() + FVector(0.f, 0.f, 40.f), 1.2f, Radius, 750.f, nullptr);
	// The car that laid it is spared: the blast radius is wider than the drop distance, so otherwise
	// dropping a mine while stopped would always take a bite out of you.
	TArray<AActor*> Ignored;
	if (OwnerVehicle)
	{
		Ignored.Add(OwnerVehicle);
	}
	UGameplayStatics::ApplyRadialDamage(World, Damage, GetActorLocation(), Radius, UDamageType::StaticClass(),
		Ignored, this, InstigatorController, true);
	Destroy();
}
