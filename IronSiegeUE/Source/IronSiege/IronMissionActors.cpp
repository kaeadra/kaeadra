#include "IronMissionActors.h"
#include "IronExplosion.h"
#include "IronPuffEmitter.h"
#include "IronSiegeAIController.h"
#include "IronSiegeHUD.h"
#include "IronTeams.h"
#include "IronVehicleKit.h"
#include "WarVehiclePawn.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
using IronKits::Finish;
using IronKits::Shape;

int32 S(Shape Value) { return static_cast<int32>(Value); }
int32 F(Finish Value) { return static_cast<int32>(Value); }
}

// ---------------------------------------------------------------- Structures

AIronMissionTarget::AIronMissionTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AIronMissionTarget* AIronMissionTarget::Spawn(UWorld* World, int32 InStructure, const FVector& GroundLocation, float Yaw)
{
	if (!World)
	{
		return nullptr;
	}
	const FTransform At(FRotator(0.f, Yaw, 0.f), GroundLocation);
	AIronMissionTarget* Target = World->SpawnActorDeferred<AIronMissionTarget>(StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Target)
	{
		Target->Structure = InStructure;
		Target->bFriendly = InStructure == IronMissions::StructRelay;
		Target->FinishSpawning(At);
	}
	return Target;
}

UStaticMeshComponent* AIronMissionTarget::AddPart(int32 InShape, int32 InFinish, const FVector& Location, const FRotator& Rotation, const FVector& SizeCm, bool bHull)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetStaticMesh(IronKitBuilder::ShapeMesh(static_cast<Shape>(InShape)));
	if (UMaterialInterface* Material = IronKitBuilder::FinishMaterial(static_cast<Finish>(InFinish)))
	{
		Part->SetMaterial(0, Material);
	}
	// The hull stops cars, bullets and rockets; as a dynamic object it is also what an explosion's
	// radial damage finds. Everything else is dressing.
	Part->SetCollisionProfileName(bHull ? TEXT("BlockAllDynamic") : TEXT("NoCollision"));
	Part->SetupAttachment(RootComponent);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(SizeCm / 100.f); // Engine basic shapes are 100 cm across.
	Part->RegisterComponent();
	Parts.Add(Part);
	return Part;
}

void AIronMissionTarget::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = Health = IronMissions::StructureHealth(Structure);

	auto MakeSpinner = [this](const FVector& Location)
	{
		Spinner = NewObject<USceneComponent>(this);
		Spinner->SetMobility(EComponentMobility::Movable);
		Spinner->SetupAttachment(RootComponent);
		Spinner->SetRelativeLocation(Location);
		Spinner->RegisterComponent();
	};
	auto OnSpinner = [this](UStaticMeshComponent* Part, const FVector& Location, const FRotator& Rotation)
	{
		Part->AttachToComponent(Spinner, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeLocationAndRotation(Location, Rotation);
		Fragile.Add(Part);
	};
	auto MakeGlow = [this](const FVector& Location, const FLinearColor& Color, float Intensity)
	{
		Glow = NewObject<UPointLightComponent>(this);
		Glow->SetupAttachment(RootComponent);
		Glow->SetRelativeLocation(Location);
		Glow->SetLightColor(Color);
		Glow->SetIntensity(Intensity);
		Glow->SetAttenuationRadius(1200.f);
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	};

	const FRotator Flat = FRotator::ZeroRotator;
	switch (Structure)
	{
	case IronMissions::StructRadar:
	{
		// A control hut with a mast and a dish that keeps turning.
		TopZ = 1080.f;
		AddPart(S(Shape::Cube), F(Finish::Black), FVector(0.f, 0.f, 12.f), Flat, FVector(620.f, 620.f, 24.f));
		AddPart(S(Shape::Cube), F(Finish::Olive), FVector(0.f, 0.f, 210.f), Flat, FVector(380.f, 380.f, 420.f), true);
		AddPart(S(Shape::Cube), F(Finish::Hazard), FVector(0.f, 0.f, 400.f), Flat, FVector(386.f, 386.f, 30.f));
		Fragile.Add(AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector(0.f, 0.f, 690.f), Flat, FVector(56.f, 56.f, 540.f)));
		for (int32 i = 0; i < 3; ++i)
		{
			const float Yaw = 120.f * i;
			const FVector Out = FRotator(0.f, Yaw, 0.f).Vector();
			// Guy struts from the roof corners up to the mast.
			Fragile.Add(AddPart(S(Shape::Cube), F(Finish::Steel), FVector(0.f, 0.f, 560.f) + Out * 95.f, FRotator(-62.f, Yaw, 0.f), FVector(300.f, 14.f, 14.f)));
		}
		MakeSpinner(FVector(0.f, 0.f, 980.f));
		OnSpinner(AddPart(S(Shape::Sphere), F(Finish::White), FVector::ZeroVector, Flat, FVector(90.f, 440.f, 300.f)), FVector(40.f, 0.f, 0.f), FRotator(20.f, 0.f, 0.f));
		OnSpinner(AddPart(S(Shape::Cone), F(Finish::Black), FVector::ZeroVector, Flat, FVector(40.f, 40.f, 120.f)), FVector(110.f, 0.f, 20.f), FRotator(-90.f, 0.f, 0.f));
		Fragile.Add(AddPart(S(Shape::Sphere), F(Finish::Glow), FVector(0.f, 0.f, 430.f), Flat, FVector(36.f, 36.f, 36.f)));
		MakeGlow(FVector(0.f, 0.f, 470.f), FLinearColor(1.f, 0.15f, 0.08f), 2500.f);
		break;
	}
	case IronMissions::StructGenerator:
	{
		// A drum base with four pylons leaning in over a glowing core.
		TopZ = 700.f;
		AddPart(S(Shape::Cylinder), F(Finish::Black), FVector(0.f, 0.f, 12.f), Flat, FVector(760.f, 760.f, 24.f));
		AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector(0.f, 0.f, 210.f), Flat, FVector(520.f, 520.f, 420.f), true);
		AddPart(S(Shape::Cylinder), F(Finish::Hazard), FVector(0.f, 0.f, 330.f), Flat, FVector(532.f, 532.f, 36.f));
		for (int32 i = 0; i < 4; ++i)
		{
			const float Yaw = 45.f + 90.f * i;
			const FVector Out = FRotator(0.f, Yaw, 0.f).Vector();
			AddPart(S(Shape::Cube), F(Finish::Black), FVector(0.f, 0.f, 330.f) + Out * 300.f, FRotator(-78.f, Yaw, 0.f), FVector(660.f, 60.f, 60.f));
			Fragile.Add(AddPart(S(Shape::Cube), F(Finish::Hazard), FVector(0.f, 0.f, 650.f) + Out * 232.f, FRotator(-78.f, Yaw, 0.f), FVector(40.f, 66.f, 66.f)));
		}
		MakeSpinner(FVector(0.f, 0.f, 540.f));
		OnSpinner(AddPart(S(Shape::Sphere), F(Finish::Glow), FVector::ZeroVector, Flat, FVector(250.f, 250.f, 250.f)), FVector::ZeroVector, Flat);
		OnSpinner(AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector::ZeroVector, Flat, FVector(330.f, 330.f, 18.f)), FVector::ZeroVector, FRotator(25.f, 0.f, 0.f));
		MakeGlow(FVector(0.f, 0.f, 540.f), FLinearColor(0.3f, 0.75f, 1.f), 9000.f);
		break;
	}
	case IronMissions::StructRelay:
	{
		// Ours: a boxy relay station in olive with a mast, a dish and a steady blue lamp.
		TopZ = 900.f;
		AddPart(S(Shape::Cube), F(Finish::Black), FVector(0.f, 0.f, 12.f), Flat, FVector(820.f, 520.f, 24.f));
		AddPart(S(Shape::Cube), F(Finish::Olive), FVector(0.f, 0.f, 200.f), Flat, FVector(600.f, 340.f, 400.f), true);
		AddPart(S(Shape::Cube), F(Finish::Olive), FVector(170.f, 0.f, 470.f), Flat, FVector(220.f, 300.f, 140.f));
		AddPart(S(Shape::Cube), F(Finish::Glass), FVector(284.f, 0.f, 470.f), Flat, FVector(8.f, 240.f, 90.f));
		AddPart(S(Shape::Cube), F(Finish::White), FVector(-60.f, 172.f, 250.f), Flat, FVector(260.f, 6.f, 110.f));
		AddPart(S(Shape::Cube), F(Finish::White), FVector(-60.f, -172.f, 250.f), Flat, FVector(260.f, 6.f, 110.f));
		Fragile.Add(AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector(-170.f, 0.f, 620.f), Flat, FVector(46.f, 46.f, 440.f)));
		MakeSpinner(FVector(-170.f, 0.f, 840.f));
		OnSpinner(AddPart(S(Shape::Sphere), F(Finish::White), FVector::ZeroVector, Flat, FVector(70.f, 320.f, 230.f)), FVector(30.f, 0.f, 0.f), FRotator(25.f, 0.f, 0.f));
		MakeGlow(FVector(170.f, 0.f, 600.f), FLinearColor(0.25f, 0.6f, 1.f), 4000.f);
		break;
	}
	default:
	{
		// Fuel depot: a big red tank with a domed cap, a smaller one beside it and the pipe between.
		TopZ = 760.f;
		AddPart(S(Shape::Cube), F(Finish::Black), FVector(120.f, 40.f, 12.f), Flat, FVector(1150.f, 900.f, 24.f));
		AddPart(S(Shape::Cylinder), F(Finish::Red), FVector(0.f, 0.f, 290.f), Flat, FVector(600.f, 600.f, 580.f), true);
		Fragile.Add(AddPart(S(Shape::Sphere), F(Finish::Red), FVector(0.f, 0.f, 580.f), Flat, FVector(600.f, 600.f, 260.f)));
		AddPart(S(Shape::Cylinder), F(Finish::Hazard), FVector(0.f, 0.f, 190.f), Flat, FVector(612.f, 612.f, 44.f));
		AddPart(S(Shape::Cylinder), F(Finish::Rust), FVector(470.f, 120.f, 190.f), Flat, FVector(300.f, 300.f, 380.f));
		Fragile.Add(AddPart(S(Shape::Sphere), F(Finish::Rust), FVector(470.f, 120.f, 380.f), Flat, FVector(300.f, 300.f, 130.f)));
		AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector(280.f, 70.f, 110.f), FRotator(90.f, 14.f, 0.f), FVector(44.f, 44.f, 260.f));
		AddPart(S(Shape::Cylinder), F(Finish::Steel), FVector(-150.f, -330.f, 300.f), Flat, FVector(36.f, 36.f, 600.f));
		break;
	}
	}

	Smoke = NewObject<UIronPuffEmitter>(this);
	Smoke->SetupAttachment(RootComponent);
	Smoke->RegisterComponent();
	Smoke->Init(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft")), 28);
}

float AIronMissionTarget::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Only the other side's fire counts: stray Legion rounds do not chew up their own depot, and the
	// player cannot shoot the relay they are defending.
	if (IsDestroyed() || IronTeams::IsPlayerSideController(EventInstigator) == bFriendly)
	{
		return 0.f;
	}
	// Ours (the relay) is only worn down by the enemies sent at it. Fire aimed at the player's car
	// parked beside it does not count: defending it up close must not be worse than leaving it.
	if (bFriendly)
	{
		const AIronSiegeAIController* Attacker = Cast<AIronSiegeAIController>(EventInstigator);
		if (!Attacker || Attacker->GetPreferredTarget() != this)
		{
			return 0.f;
		}
	}
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Actual <= 0.f)
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - Actual);
	if (const APlayerController* Shooter = Cast<APlayerController>(EventInstigator))
	{
		if (AIronSiegeHUD* HUD = Shooter->GetHUD<AIronSiegeHUD>())
		{
			HUD->NotifyHitMarker(IsDestroyed());
			HUD->NotifyDamage(this, Actual, IsDestroyed());
		}
	}
	if (IsDestroyed())
	{
		Die();
	}
	return Actual;
}

void AIronMissionTarget::Die()
{
	AIronExplosion::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, TopZ * 0.4f), 2.2f, 1400.f, 700.f, this);
	UMaterialInterface* Charred = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_City_FrameDark.MI_City_FrameDark"));
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part && Charred)
		{
			Part->SetMaterial(0, Charred);
		}
	}
	for (UStaticMeshComponent* Part : Fragile)
	{
		if (Part)
		{
			Part->SetVisibility(false);
		}
	}
	if (Glow)
	{
		Glow->SetVisibility(false);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission structure %d destroyed at %s"), Structure, *GetActorLocation().ToCompactString());
}

void AIronMissionTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (!IsDestroyed())
	{
		if (Spinner)
		{
			// The generator's core tumbles; a dish sweeps round.
			const bool bCore = Structure == IronMissions::StructGenerator;
			Spinner->AddLocalRotation(FRotator(0.f, (bCore ? 90.f : 40.f) * DeltaSeconds, 0.f));
		}
		if (Glow && Structure == IronMissions::StructGenerator)
		{
			Glow->SetIntensity(9000.f * (0.75f + 0.25f * FMath::Sin(Age * 5.f)));
		}
	}
	// Black smoke off a damaged structure, thicker the worse it is; a wreck burns, then smoulders out.
	const float Fraction = GetHealthFraction();
	if (IsDestroyed())
	{
		WreckSeconds += DeltaSeconds;
	}
	if (!Smoke || Fraction > 0.6f || WreckSeconds > 45.f)
	{
		return;
	}
	SmokeTimer -= DeltaSeconds;
	if (SmokeTimer > 0.f)
	{
		return;
	}
	const float Severity = FMath::Clamp((0.6f - Fraction) / 0.6f, 0.f, 1.f);
	SmokeTimer = FMath::Lerp(0.3f, 0.09f, Severity);
	const FVector Top = GetActorLocation() + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), TopZ * 0.65f);
	FIronPuffStyle Puff;
	Puff.Life = 2.4f;
	Puff.StartScale = 0.7f;
	Puff.EndScale = FMath::Lerp(2.2f, 4.5f, Severity);
	Puff.Drift = FVector(50.f, 25.f, 280.f);
	Puff.StartColor = Puff.EndColor = FLinearColor(0.04f, 0.038f, 0.037f);
	Puff.PeakOpacity = FMath::Lerp(0.4f, 0.75f, Severity);
	Smoke->Emit(Top, FVector::ZeroVector, Puff);
	if (IsDestroyed() && WreckSeconds < 20.f)
	{
		FIronPuffStyle Fire;
		Fire.Life = 0.55f;
		Fire.StartScale = 0.7f;
		Fire.EndScale = 1.5f;
		Fire.Drift = FVector(0.f, 0.f, 300.f);
		Fire.StartColor = FLinearColor(4.f, 1.4f, 0.2f);
		Fire.EndColor = FLinearColor(0.4f, 0.1f, 0.03f);
		Fire.PeakOpacity = 0.9f;
		Fire.bSwell = false;
		Smoke->Emit(Top - FVector(0.f, 0.f, TopZ * 0.25f), FVector::ZeroVector, Fire);
	}
}

// ---------------------------------------------------------------- Zones

AIronCaptureZone::AIronCaptureZone()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AIronCaptureZone* AIronCaptureZone::Spawn(UWorld* World, const FVector& GroundLocation, bool bInCapture)
{
	if (!World)
	{
		return nullptr;
	}
	const FTransform At(GroundLocation);
	AIronCaptureZone* Zone = World->SpawnActorDeferred<AIronCaptureZone>(StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Zone)
	{
		Zone->bCapture = bInCapture;
		Zone->Radius = IronMissions::CaptureTuning().RadiusCm;
		Zone->FinishSpawning(At);
	}
	return Zone;
}

void AIronCaptureZone::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Cylinder = IronKitBuilder::ShapeMesh(Shape::Cylinder);
	UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/M_FX_Soft.M_FX_Soft"));
	Paint = Soft ? UMaterialInstanceDynamic::Create(Soft, this) : nullptr;
	auto MakePost = [&](const FVector& Location, const FVector& SizeCm)
	{
		UStaticMeshComponent* Post = NewObject<UStaticMeshComponent>(this);
		Post->SetMobility(EComponentMobility::Movable);
		Post->SetStaticMesh(Cylinder);
		if (Paint)
		{
			Post->SetMaterial(0, Paint);
		}
		Post->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Post->SetCastShadow(false);
		Post->SetupAttachment(RootComponent);
		Post->SetRelativeLocation(Location);
		Post->SetRelativeScale3D(SizeCm / 100.f);
		Post->RegisterComponent();
		return Post;
	};
	// Soft glowing posts round the rim, and a column over the middle that shows across the map.
	const int32 Count = 18;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = 2.f * PI * i / Count;
		Posts.Add(MakePost(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 130.f), FVector(34.f, 34.f, 260.f)));
	}
	Beacon = MakePost(FVector(0.f, 0.f, 1500.f), FVector(46.f, 46.f, 3000.f));
}

void AIronCaptureZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bInside = Player && IronTeams::IsAlive(Player) && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= Radius;
	if (!State.bCaptured)
	{
		if (!bCapture)
		{
			// A waypoint: arriving is enough.
			if (bInside)
			{
				State.Progress = 1.f;
				State.bCaptured = true;
			}
		}
		else
		{
			int32 Enemies = 0;
			for (TActorIterator<AWarVehiclePawn> It(GetWorld()); It; ++It)
			{
				if (!IronTeams::IsPlayerSide(*It) && IronTeams::IsAlive(*It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius)
				{
					++Enemies;
				}
			}
			bContested = bInside && Enemies > 0;
			IronMissions::TickCapture(State, bInside, Enemies, DeltaSeconds);
		}
	}
	else
	{
		bContested = false;
	}
	if (Paint)
	{
		// Amber while it waits, filling toward green as it is taken, flashing red while contested.
		const FLinearColor Idle(1.6f, 1.05f, 0.2f), Ours(0.25f, 1.9f, 0.45f), Fight(2.4f, 0.25f, 0.15f);
		FLinearColor Color = bCapture ? FMath::Lerp(Idle, Ours, State.Progress) : FLinearColor(0.4f, 1.3f, 2.2f);
		float Opacity = 0.5f + 0.2f * FMath::Sin(Age * 3.f);
		if (bContested)
		{
			Color = Fight;
			Opacity = FMath::Fmod(Age, 0.4f) < 0.2f ? 0.85f : 0.3f;
		}
		else if (State.bCaptured)
		{
			Opacity = 0.6f;
		}
		Paint->SetVectorParameterValue(TEXT("Color"), Color);
		Paint->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
}
