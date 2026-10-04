#include "MachineGunComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "IronPuffEmitter.h"
#include "IronVehicleKit.h"

namespace
{
constexpr float TracerSpeed = 90000.f;  // cm/s: what the eye follows, not the (instant) hit.
constexpr float TracerLength = 320.f;
constexpr float CasingLife = 0.9f;

TSoftObjectPtr<USoundBase> Sound(const TCHAR* Name)
{
	return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/IronSiege/Audio/%s.%s"), Name, Name)));
}
}

UMachineGunComponent::UMachineGunComponent()
{
	WeaponType = EIronWeaponType::MachineGun;
	FireSound = Sound(TEXT("S_Gunshot"));
	FireSoundVariants = { Sound(TEXT("S_Gunshot2")), Sound(TEXT("S_Gunshot3")), Sound(TEXT("S_Gunshot4")) };
	FireSoundFarVariants = { Sound(TEXT("S_GunshotFar")), Sound(TEXT("S_GunshotFar2")) };
	RicochetSounds = { Sound(TEXT("S_Ricochet1")), Sound(TEXT("S_Ricochet2")), Sound(TEXT("S_Ricochet3")) };
	FlybySounds = { Sound(TEXT("S_Flyby1")), Sound(TEXT("S_Flyby2")) };
	DryFireSound = Sound(TEXT("S_DryFire"));
	OverheatSound = Sound(TEXT("S_Overheat"));
	SpinDownSound = Sound(TEXT("S_SpinDown"));
}

void UMachineGunComponent::EnsureFx()
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (bFxReady || !Root)
	{
		return;
	}
	bFxReady = true;
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_Fire.MI_FX_Fire"));
	auto MakeMesh = [&](UStaticMesh* Mesh, UMaterialInterface* Material) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetVisibility(false);
		Component->SetupAttachment(Root);
		Component->RegisterComponent();
		Component->SetAbsolute(true, true, true); // Lives in the world, not on the moving car.
		return Component;
	};

	// Tracers: the modelled glow streak if imported, else a thin cylinder.
	UStaticMesh* TracerShape = IronKitBuilder::RigMesh("SM_FX_Tracer");
	bTracerModelled = TracerShape != nullptr;
	UStaticMesh* Cylinder = IronKitBuilder::ShapeMesh(IronKits::Shape::Cylinder);
	for (int32 i = 0; i < 8; ++i)
	{
		TracerMeshes.Add(MakeMesh(TracerShape ? TracerShape : Cylinder, Glow));
	}
	Tracers.SetNum(TracerMeshes.Num());

	// Muzzle flash: the modelled star of flame planes, else a cone.
	UStaticMesh* FlashShape = IronKitBuilder::RigMesh("SM_FX_MuzzleFlash");
	bFlashModelled = FlashShape != nullptr;
	FlashMesh = MakeMesh(FlashShape ? FlashShape : IronKitBuilder::ShapeMesh(IronKits::Shape::Cone), Glow);
	MuzzleFlash = NewObject<UPointLightComponent>(Owner);
	MuzzleFlash->SetupAttachment(Root);
	MuzzleFlash->SetIntensity(9000.f);
	MuzzleFlash->SetAttenuationRadius(600.f);
	MuzzleFlash->SetLightColor(FLinearColor(1.f, 0.7f, 0.35f));
	MuzzleFlash->SetCastShadows(false);
	MuzzleFlash->SetVisibility(false);
	MuzzleFlash->RegisterComponent();
	MuzzleFlash->SetAbsolute(true, true, true);

	Sparks = NewObject<UIronPuffEmitter>(Owner);
	Sparks->SetupAttachment(Root);
	Sparks->RegisterComponent();
	Sparks->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft")), 12);
	Dust = NewObject<UIronPuffEmitter>(Owner);
	Dust->SetupAttachment(Root);
	Dust->RegisterComponent();
	Dust->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft")), 10);

	// Brass for the player's gun only - nobody sees an enemy's cases at 50 m.
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		UStaticMesh* CaseShape = IronKitBuilder::RigMesh("SM_FX_Casing");
		bCasingModelled = CaseShape != nullptr;
		for (int32 i = 0; i < 10; ++i)
		{
			CasingMeshes.Add(MakeMesh(CaseShape ? CaseShape : Cylinder, IronKitBuilder::FinishMaterial(IronKits::Finish::Brass)));
			CasingMeshes.Last()->SetCastShadow(false);
		}
		Casings.SetNum(CasingMeshes.Num());
	}
}

void UMachineGunComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!World || !Owner)
	{
		return;
	}
	EnsureFx();

	// Hold the trigger and the barrel heats up, opening the cone the rounds go into; short bursts
	// stay accurate (IronWeapons::Loadout::SpreadDeg, tested offline).
	const FVector Direction = FMath::VRandCone(AimDirection.GetSafeNormal(), FMath::DegreesToRadians(Loadout.SpreadDeg()));
	const FVector End = Origin + Direction * Spec.Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeMachineGunTrace));
	Params.AddIgnoredActor(Owner);
	Params.bReturnPhysicalMaterial = false;

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params);
	const FVector ImpactAt = bHit ? FVector(Hit.ImpactPoint) : End;
	const FVector TracerStart = VisualMuzzle.IsValid() ? VisualMuzzle->GetComponentLocation() : Origin;
	LaunchTracer(TracerStart, ImpactAt);

	// Flash: a different size and twist every round, so a burst flickers like a real gun.
	if (FlashMesh)
	{
		FRotator Facing = (ImpactAt - TracerStart).GetSafeNormal().Rotation();
		const bool bModelled = bFlashModelled;
		if (!bModelled)
		{
			Facing = FRotationMatrix::MakeFromZ((ImpactAt - TracerStart).GetSafeNormal()).Rotator();
		}
		Facing.Roll += FMath::FRandRange(0.f, 360.f);
		const float Size = FMath::FRandRange(0.75f, 1.25f);
		FlashMesh->SetWorldLocationAndRotation(TracerStart, Facing);
		FlashMesh->SetWorldScale3D(bModelled ? FVector(Size, Size * 0.9f, Size * 0.9f) : FVector(0.18f, 0.18f, 0.32f) * Size);
		FlashMesh->SetVisibility(true);
		FlashTimeLeft = 0.035f;
	}
	if (MuzzleFlash)
	{
		MuzzleFlash->SetWorldLocation(TracerStart);
		MuzzleFlash->SetIntensity(FMath::FRandRange(7000.f, 11000.f));
		MuzzleFlash->SetVisibility(true);
	}
	EjectCasing();

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (bHit)
	{
		const bool bVehicle = HitActor && HitActor->IsA<APawn>();
		ShowImpact(Hit, bVehicle, Direction);
		if (bVehicle)
		{
			if (USoundBase* Ping = ImpactSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(this, Ping, Hit.ImpactPoint, 0.8f, FMath::FRandRange(0.85f, 1.2f));
			}
		}
		else if (RicochetSounds.Num() > 0 && FMath::FRand() < 0.35f)
		{
			if (USoundBase* Whine = LoadIfPresent(RicochetSounds[FMath::RandHelper(RicochetSounds.Num())]))
			{
				UGameplayStatics::PlaySoundAtLocation(this, Whine, Hit.ImpactPoint, 0.6f, FMath::FRandRange(0.9f, 1.15f));
			}
		}
		if (HitActor)
		{
			UGameplayStatics::ApplyPointDamage(HitActor, ShotDamage(Spec), Direction, Hit, GetOwnerController(), Owner, UDamageType::StaticClass());
		}
	}
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn || !OwnerPawn->IsPlayerControlled())
	{
		PlayFlybyForPlayer(TracerStart, ImpactAt, HitActor);
	}
	++ShotCount;
	LastShotTime = World->GetTimeSeconds();
	bSpinningDown = false;
}

void UMachineGunComponent::LaunchTracer(const FVector& Start, const FVector& End)
{
	if (TracerMeshes.Num() == 0)
	{
		return;
	}
	FTracer& T = Tracers[NextTracer];
	T.Start = Start;
	T.Direction = (End - Start).GetSafeNormal();
	T.Length = FVector::Dist(Start, End);
	T.Age = 0.f;
	NextTracer = (NextTracer + 1) % TracerMeshes.Num();
}

void UMachineGunComponent::ShowImpact(const FHitResult& Hit, bool bVehicle, const FVector& Direction)
{
	const FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? -Direction : FVector(Hit.ImpactNormal);
	if (bVehicle && Sparks)
	{
		// Sparks glance off the armour: mostly along the reflected path, scattered.
		const FVector Bounce = Direction - 2.f * FVector::DotProduct(Direction, Normal) * Normal;
		FIronPuffStyle Spark;
		Spark.Life = 0.13f;
		Spark.StartScale = 0.05f;
		Spark.EndScale = 0.012f;
		Spark.Drift = FVector::ZeroVector;
		Spark.StartColor = FLinearColor(7.f, 4.5f, 1.4f);
		Spark.EndColor = FLinearColor(2.f, 0.7f, 0.15f);
		Spark.PeakOpacity = 1.f;
		Spark.bSwell = false;
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Kick = (Bounce + FMath::VRand() * 0.6f).GetSafeNormal() * FMath::FRandRange(500.f, 1100.f);
			Sparks->Emit(Hit.ImpactPoint + Normal * 4.f, Kick, Spark);
		}
	}
	else if (Dust)
	{
		FIronPuffStyle Puff;
		Puff.Life = 0.6f;
		Puff.StartScale = 0.08f;
		Puff.EndScale = 0.55f;
		Puff.Drift = FVector(0.f, 0.f, 60.f);
		Puff.StartColor = Puff.EndColor = FLinearColor(0.3f, 0.27f, 0.22f);
		Puff.PeakOpacity = 0.55f;
		Dust->Emit(Hit.ImpactPoint + Normal * 6.f, Normal * 120.f, Puff);
		if (Sparks && FMath::FRand() < 0.4f)
		{
			// Now and then a round skips off stone or steel with a spark.
			FIronPuffStyle Spark;
			Spark.Life = 0.1f;
			Spark.StartScale = 0.04f;
			Spark.EndScale = 0.01f;
			Spark.Drift = FVector::ZeroVector;
			Spark.StartColor = FLinearColor(6.f, 4.f, 1.2f);
			Spark.EndColor = FLinearColor(1.5f, 0.6f, 0.1f);
			Spark.PeakOpacity = 1.f;
			Spark.bSwell = false;
			Sparks->Emit(Hit.ImpactPoint + Normal * 3.f, (Normal + FMath::VRand() * 0.5f).GetSafeNormal() * 700.f, Spark);
		}
	}
}

void UMachineGunComponent::EjectCasing()
{
	if (CasingMeshes.Num() == 0 || !VisualMuzzle.IsValid())
	{
		return;
	}
	// Out of the right side of the receiver, up and back, tumbling.
	const FTransform Gun = VisualMuzzle->GetComponentTransform();
	const FVector Right = Gun.GetUnitAxis(EAxis::Y), Forward = Gun.GetUnitAxis(EAxis::X), Up = Gun.GetUnitAxis(EAxis::Z);
	const AActor* Owner = GetOwner();
	const FVector CarVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	FCasing& C = Casings[NextCasing];
	C.Age = 0.f;
	C.Velocity = CarVelocity + Right * FMath::FRandRange(180.f, 260.f) + Up * FMath::FRandRange(150.f, 240.f) - Forward * FMath::FRandRange(20.f, 80.f);
	C.Spin = FRotator(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f));
	UStaticMeshComponent* Mesh = CasingMeshes[NextCasing];
	Mesh->SetWorldLocationAndRotation(Gun.GetLocation() - Forward * 55.f + Right * 8.f, Gun.Rotator());
	Mesh->SetWorldScale3D(bCasingModelled ? FVector(1.f) : FVector(0.025f, 0.025f, 0.075f));
	Mesh->SetVisibility(true);
	NextCasing = (NextCasing + 1) % CasingMeshes.Num();
}

void UMachineGunComponent::PlayFlybyForPlayer(const FVector& Start, const FVector& End, const AActor* HitActor)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager || FlybySounds.Num() == 0 || (HitActor && HitActor == PC->GetPawn()))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastFlybyTime < 0.12)
	{
		return;
	}
	const FVector Ear = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Closest = FMath::ClosestPointOnSegment(Ear, Start, End);
	const float Miss = FVector::Dist(Ear, Closest);
	if (Miss > 450.f || FVector::DistSquared(Closest, End) < 1.f)
	{
		return; // Not close, or the round stopped short of the player.
	}
	if (USoundBase* Whiz = LoadIfPresent(FlybySounds[FMath::RandHelper(FlybySounds.Num())]))
	{
		LastFlybyTime = Now;
		const float Volume = float(FMath::GetMappedRangeValueClamped(FVector2D(80.0, 450.0), FVector2D(1.0, 0.35), double(Miss)));
		UGameplayStatics::PlaySoundAtLocation(this, Whiz, Closest, Volume, FMath::FRandRange(0.9f, 1.12f));
	}
}

void UMachineGunComponent::DryFire()
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner || World->GetTimeSeconds() - LastDryFireTime < 0.35)
	{
		return;
	}
	LastDryFireTime = World->GetTimeSeconds();
	if (USoundBase* Click = LoadIfPresent(DryFireSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Click, Owner->GetActorLocation(), 0.8f);
	}
}

void UMachineGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	// The barrel locking up hisses; a burst ending lets the barrels whirr down.
	if (Loadout.Overheated && !bWasOverheated)
	{
		if (USoundBase* Hiss = LoadIfPresent(OverheatSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Hiss, Owner->GetActorLocation(), 0.9f);
		}
	}
	bWasOverheated = Loadout.Overheated;
	if (!bSpinningDown && Now - LastShotTime > 0.18)
	{
		bSpinningDown = true;
		if (USoundBase* Whirr = LoadIfPresent(SpinDownSound))
		{
			const FVector At = VisualMuzzle.IsValid() ? VisualMuzzle->GetComponentLocation() : Owner->GetActorLocation();
			UGameplayStatics::PlaySoundAtLocation(this, Whirr, At, 0.7f);
		}
	}

	if (FlashTimeLeft > 0.f)
	{
		FlashTimeLeft -= DeltaTime;
		if (FlashTimeLeft <= 0.f)
		{
			if (FlashMesh) FlashMesh->SetVisibility(false);
			if (MuzzleFlash) MuzzleFlash->SetVisibility(false);
		}
	}

	// Tracers fly from the muzzle to the impact, a short glowing streak.
	for (int32 i = 0; i < Tracers.Num(); ++i)
	{
		FTracer& T = Tracers[i];
		if (T.Age < 0.f)
		{
			continue;
		}
		T.Age += DeltaTime;
		const float Head = FMath::Min(T.Age * TracerSpeed, T.Length);
		const float Tail = FMath::Max(0.f, T.Age * TracerSpeed - TracerLength);
		UStaticMeshComponent* Mesh = TracerMeshes[i];
		if (Tail >= T.Length || Head - Tail < 1.f)
		{
			T.Age = -1.f;
			Mesh->SetVisibility(false);
			continue;
		}
		const FVector Mid = T.Start + T.Direction * (0.5f * (Head + Tail));
		const float Span = Head - Tail;
		if (!bTracerModelled)
		{
			Mesh->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(T.Direction).Rotator());
			Mesh->SetWorldScale3D(FVector(0.045f, 0.045f, Span / 100.f));
		}
		else
		{
			// The modelled streak is 100 cm along X, centred on its origin.
			Mesh->SetWorldLocationAndRotation(Mid, T.Direction.Rotation());
			Mesh->SetWorldScale3D(FVector(Span / 100.f, 1.f, 1.f));
		}
		Mesh->SetVisibility(true);
	}

	// Brass arcs away and is gone before anyone could count it.
	for (int32 i = 0; i < Casings.Num(); ++i)
	{
		FCasing& C = Casings[i];
		if (C.Age < 0.f)
		{
			continue;
		}
		C.Age += DeltaTime;
		UStaticMeshComponent* Mesh = CasingMeshes[i];
		if (C.Age > CasingLife)
		{
			C.Age = -1.f;
			Mesh->SetVisibility(false);
			continue;
		}
		C.Velocity.Z -= 980.f * DeltaTime;
		Mesh->AddWorldOffset(C.Velocity * DeltaTime);
		Mesh->AddWorldRotation(C.Spin * DeltaTime);
	}
}
