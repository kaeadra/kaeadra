#include "FlamethrowerComponent.h"
#include "WarVehiclePawn.h"
#include "IronPuffEmitter.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void UFlamethrowerComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	Jet = NewObject<UIronPuffEmitter>(Owner);
	Jet->SetupAttachment(Root);
	Jet->RegisterComponent();
	Jet->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft")), 18);
}

void UFlamethrowerComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !bUnlocked)
	{
		return;
	}
	const FVector Direction = AimDirection.GetSafeNormal();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(Spec.SpreadMaxDeg * 2.f));

	// Everything inside the cone takes a tick of burn - no trace per target, the flame does not
	// care about cover at this range.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Target = *It;
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(Target);
		const UVehicleHealthComponent* TargetHealth = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (Target == Owner || !TargetHealth || TargetHealth->IsDestroyed())
		{
			continue;
		}
		const FVector ToTarget = Target->GetActorLocation() - Origin;
		const float Distance = ToTarget.Size();
		if (Distance > Spec.Range || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (FVector::DotProduct(ToTarget / Distance, Direction) < CosLimit)
		{
			continue;
		}
		UGameplayStatics::ApplyDamage(Target, Spec.Damage * DamageMultiplier, GetOwnerController(), Owner, UDamageType::StaticClass());
		// And sets it alight: it keeps burning for a few seconds after the jet moves off.
		if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(Target))
		{
			Car->Ignite(IronDamage::BurnTuning().Dps * DamageMultiplier, GetOwnerController());
		}
	}

	// Two puffs per tick, thrown forward with a little spread, cooling from yellow to smoke, so the
	// jet looks continuous.
	if (Jet)
	{
		FIronPuffStyle Flame;
		Flame.Life = 0.35f;
		Flame.StartScale = 0.2f;
		Flame.EndScale = 1.05f;
		Flame.Drift = FVector(0.f, 0.f, 60.f);
		Flame.StartColor = FLinearColor(3.5f, 1.1f, 0.15f);
		Flame.EndColor = FLinearColor(0.5f, 0.2f, 0.15f);
		Flame.PeakOpacity = 0.9f;
		Flame.bSwell = false;
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Spread = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spec.SpreadMaxDeg));
			Jet->Emit(Origin + Direction * 60.f, Spread * FMath::FRandRange(1400.f, 1900.f), Flame);
		}
	}
}
