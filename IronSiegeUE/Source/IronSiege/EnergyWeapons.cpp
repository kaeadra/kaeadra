#include "EnergyWeapons.h"
#include "BeamRules.h"
#include "IronBeamFx.h"
#include "IronExplosion.h"
#include "IronTeams.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "WarVehiclePawn.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

namespace
{
IronBeams::Vec ToVec(const FVector& V) { return { float(V.X), float(V.Y), float(V.Z) }; }

// Live enemy vehicles other than Owner, and their centres (a little above the road-level origin).
void GatherTargets(UWorld* World, const AActor* Owner, TArray<APawn*>& OutPawns, TArray<IronBeams::Vec>& OutCentres)
{
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(Pawn);
		const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (Pawn == Owner || !Health || Health->IsDestroyed())
		{
			continue;
		}
		// Sides: the player's weapons strike enemies, an enemy's strike only the player's side - an
		// enemy tesla must not chain through (and stall) its own wingmen, nor the player's an escorted truck.
		if (IronTeams::IsPlayerSide(Pawn) == IronTeams::IsPlayerSide(Owner))
		{
			continue;
		}
		OutPawns.Add(Pawn);
		OutCentres.Add(ToVec(Pawn->GetActorLocation() + FVector(0.f, 0.f, 80.f)));
	}
}

USceneComponent* FxParent(AActor* Owner)
{
	return Owner ? Owner->GetRootComponent() : nullptr;
}
}

// ---------------------------------------------------------------- Railgun

void URailgunComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	USceneComponent* Root = FxParent(Owner);
	if (!Root)
	{
		return;
	}
	Beam = NewObject<UIronBeamFx>(Owner, TEXT("RailBeam"));
	Beam->SetupAttachment(Root);
	Beam->RegisterComponent();
	Beam->Init(4);

	ChargeGlow = NewObject<UPointLightComponent>(Owner, TEXT("RailChargeGlow"));
	ChargeGlow->SetupAttachment(Root);
	ChargeGlow->SetLightColor(FLinearColor(0.35f, 0.8f, 1.f));
	ChargeGlow->SetAttenuationRadius(500.f);
	ChargeGlow->SetCastShadows(false);
	ChargeGlow->SetIntensity(0.f);
	ChargeGlow->RegisterComponent();
}

void URailgunComponent::SetChargeProgress(float Progress)
{
	if (!ChargeGlow)
	{
		return;
	}
	if (USceneComponent* Muzzle = VisualMuzzle.Get())
	{
		ChargeGlow->SetWorldLocation(Muzzle->GetComponentLocation());
	}
	ChargeGlow->SetIntensity(Progress > 0.f ? 2000.f + 40000.f * Progress * Progress : 0.f);
}

void URailgunComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !bUnlocked)
	{
		return;
	}
	const FVector Direction = AimDirection.GetSafeNormal();
	const FVector Start = VisualMuzzle.IsValid() ? VisualMuzzle->GetComponentLocation() : Origin;
	FVector End = Origin + Direction * Spec.Range;

	// Walls and buildings stop the slug; cars do not (that is the point of a railgun).
	FCollisionObjectQueryParams Statics(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronRailgun), false, Owner);
	FHitResult WallHit;
	if (World->LineTraceSingleByObjectType(WallHit, Origin, End, Statics, Params))
	{
		End = WallHit.ImpactPoint;
	}

	TArray<APawn*> Pawns;
	TArray<IronBeams::Vec> Centres;
	GatherTargets(World, Owner, Pawns, Centres);
	TArray<int32> Order;
	Order.SetNumZeroed(FMath::Max(Pawns.Num(), 1));
	const int32 Hits = IronBeams::PiercedTargets(ToVec(Origin), ToVec(End), Centres.GetData(), Centres.Num(), Order.GetData());
	for (int32 i = 0; i < Hits; ++i)
	{
		APawn* Target = Pawns[Order[i]];
		UGameplayStatics::ApplyDamage(Target, IronBeams::PierceDamage(Spec.Damage * DamageMultiplier, i), GetOwnerController(), Owner, UDamageType::StaticClass());
		AIronExplosion::Spawn(World, Target->GetActorLocation() + FVector(0.f, 0.f, 90.f), 0.35f, 300.f, 250.f, Owner);
	}

	// Beam: a wide soft cyan glow around a thin white-hot core, fading over half a second.
	if (Beam)
	{
		Beam->DrawSegment(Start, End, 34.f, FLinearColor(0.2f, 1.2f, 4.f), 0.5f);
		Beam->DrawSegment(Start, End, 7.f, FLinearColor(4.f, 5.f, 6.f), 0.3f);
	}
	SetChargeProgress(0.f);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: railgun from %s - pierced %d car(s), beam %.0f m"), *Owner->GetName(), Hits, (End - Start).Size() / 100.f);
}

// ---------------------------------------------------------------- Tesla coil

void UTeslaComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (USceneComponent* Root = FxParent(Owner))
	{
		Bolts = NewObject<UIronBeamFx>(Owner, TEXT("TeslaBolts"));
		Bolts->SetupAttachment(Root);
		Bolts->RegisterComponent();
		Bolts->Init(60); // Four jumps x two bolts x six kinks, with room to spare.
	}
}

void UTeslaComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !bUnlocked)
	{
		return;
	}
	const IronBeams::TeslaTuning Tuning;
	const FVector Coil = VisualMuzzle.IsValid() ? VisualMuzzle->GetComponentLocation() : Origin;
	TArray<APawn*> Pawns;
	TArray<IronBeams::Vec> Centres;
	GatherTargets(World, Owner, Pawns, Centres);
	int32 Chain[8] = {};
	LastChain = IronBeams::BuildChain(ToVec(Coil), ToVec(AimDirection.GetSafeNormal2D()), Centres.GetData(), Centres.Num(), Chain, Tuning);

	const FLinearColor Arc(1.2f, 2.f, 10.f);
	FVector From = Coil;
	for (int32 Link = 0; Link < LastChain; ++Link)
	{
		APawn* Target = Pawns[Chain[Link]];
		const FVector To = Target->GetActorLocation() + FVector(0.f, 0.f, 80.f);
		UGameplayStatics::ApplyDamage(Target, IronBeams::ChainDamage(Spec.Damage * DamageMultiplier, Link, Tuning), GetOwnerController(), Owner, UDamageType::StaticClass());
		if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(Target))
		{
			// The player gets a shorter stall than AI cars: long enough to hurt, not to feel unfair.
			Car->Stun(Car->IsPlayerControlled() ? Tuning.StunSeconds * 0.6f : Tuning.StunSeconds);
		}
		if (Bolts)
		{
			// Two crossing jagged bolts per jump read as crackling lightning rather than a laser.
			Bolts->DrawBolt(From, To, 6, 70.f, Arc, 0.35f);
			Bolts->DrawBolt(From, To, 6, 55.f, FLinearColor(3.f, 3.5f, 6.f), 0.2f);
		}
		From = To;
	}
	if (LastChain == 0 && Bolts)
	{
		// Nothing in reach: the coil fizzles a short arc into the air ahead.
		Bolts->DrawBolt(Coil, Coil + AimDirection.GetSafeNormal() * 700.f + FVector(0.f, 0.f, 150.f), 5, 60.f, Arc, 0.25f);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: tesla from %s - chain of %d"), *Owner->GetName(), LastChain);
}
