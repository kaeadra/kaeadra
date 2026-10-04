#include "WarVehiclePawn.h"
#include "IronSiegeText.h"
#include "MachineGunComponent.h"
#include "RocketLauncherComponent.h"
#include "VehicleHealthComponent.h"
#include "ChaosVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "IronExplosion.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "SettingsRules.h"
#include "ChaosVehicleWheel.h"
#include "IronSiegePlayerController.h"
#include "MineLayerComponent.h"
#include "FlamethrowerComponent.h"
#include "IronPuffEmitter.h"
#include "IronVehicleKit.h"
#include "MissileRules.h"
#include "EngineUtils.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "IronSiegeHUD.h"
#include "IronFlare.h"
#include "IronSiegeUserSettings.h"
#include "EnergyWeapons.h"
#include "RocketProjectile.h"
#include "GameFramework/PlayerController.h"
#include "AimRules.h"
#include "IronTeams.h"

AWarVehiclePawn::AWarVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	PrimaryWeapon = CreateDefaultSubobject<UMachineGunComponent>(TEXT("PrimaryWeapon"));
	SecondaryWeapon = CreateDefaultSubobject<URocketLauncherComponent>(TEXT("SecondaryWeapon"));
	Health = CreateDefaultSubobject<UVehicleHealthComponent>(TEXT("Health"));

	// The Chaos rig's origin is at ground level under the car, so everything below is placed
	// relative to the road surface; the setup script moves them per car (roof height, bumper).
	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(GetMesh());
	MuzzlePoint->SetRelativeLocation(FVector(250.f, 0.f, 70.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunFinder(TEXT("/Game/IronSiege/Weapons/SM_AssaultRifle_MG.SM_AssaultRifle_MG"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RocketFinder(TEXT("/Game/IronSiege/Weapons/SM_SniperRifle_RocketStandin.SM_SniperRifle_RocketStandin"));
	PrimaryWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrimaryWeaponMesh"));
	PrimaryWeaponMesh->SetupAttachment(GetMesh());
	PrimaryWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimaryWeaponMesh->SetRelativeScale3D(FVector(30.f)); // Quaternius OBJ imported ~30x too small.
	PrimaryWeaponMesh->SetRelativeLocation(FVector(-5.f, 0.f, 132.f));
	if (GunFinder.Succeeded()) PrimaryWeaponMesh->SetStaticMesh(GunFinder.Object);
	SecondaryWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryWeaponMesh"));
	SecondaryWeaponMesh->SetupAttachment(GetMesh());
	SecondaryWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SecondaryWeaponMesh->SetRelativeScale3D(FVector(16.5f));
	SecondaryWeaponMesh->SetRelativeLocation(FVector(-75.f, 0.f, 130.f));
	if (RocketFinder.Succeeded()) SecondaryWeaponMesh->SetStaticMesh(RocketFinder.Object);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetMesh());
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	SpringArm->TargetArmLength = 760.f;
	SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeSize = 16.f;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;
	// Follow the car's heading but not its body roll/pitch over kerbs and bumps.
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 6.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(GetMesh());

	SkidAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SkidAudio"));
	SkidAudio->SetupAttachment(GetMesh());
	SkidAudio->bAutoActivate = false;
	SkidAudio->SetVolumeMultiplier(0.f);

	SkidSmoke = CreateDefaultSubobject<UIronPuffEmitter>(TEXT("SkidSmoke"));
	SkidSmoke->SetupAttachment(GetMesh());
	DamageFx = CreateDefaultSubobject<UIronPuffEmitter>(TEXT("DamageFx"));
	DamageFx->SetupAttachment(GetMesh());

	MineWeapon = CreateDefaultSubobject<UMineLayerComponent>(TEXT("MineWeapon"));
	Flamer = CreateDefaultSubobject<UFlamethrowerComponent>(TEXT("Flamer"));
	Railgun = CreateDefaultSubobject<URailgunComponent>(TEXT("Railgun"));
	Tesla = CreateDefaultSubobject<UTeslaComponent>(TEXT("Tesla"));
	EngineAudio->SetRelativeLocation(FVector(120.f, 0.f, 60.f));
	EngineAudio->bAutoActivate = false;
}

void AWarVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	const IronVehicles::ClassStats& Stats = IronVehicles::Get(static_cast<IronVehicles::VehicleClass>(VehicleClass));
	if (Health)
	{
		Health->ConfigureFromClassStats(Stats.MaxHealth * DurabilityMultiplier, Stats.MaxArmor * DurabilityMultiplier, Stats.ArmorDamageReduction);
		Health->OnVehicleDestroyed.AddDynamic(this, &AWarVehiclePawn::HandleDestroyed);
		Health->OnVehicleDamaged.AddDynamic(this, &AWarVehiclePawn::HandleDamaged);
	}
	PreviousLocation = GetActorLocation();
	LastSkidLocation = PreviousLocation;
	if (USoundBase* Screech = SkidSound.LoadSynchronous())
	{
		SkidAudio->SetSound(Screech);
		SkidAudio->Play();
		SkidAudio->SetVolumeMultiplier(0.f);
	}
	// Vehicle Variety Pack cars bring their own camera rig; only this class's chase camera may be live.
	TArray<UCameraComponent*> Cameras;
	GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Other : Cameras)
	{
		if (Other != Camera)
		{
			Other->SetActive(false);
			Other->bAutoActivate = false;
		}
	}
	// Bolt the class's war kit on (VehicleKitRules.h); enemy variants are rolled here.
	BodyBox = IronKitBuilder::LocalBounds(GetMesh());
	const IronKits::Kit KitId = static_cast<IronKits::Kit>(Kit);
	const int32 Variant = IronKits::ResolveVariant(KitId, KitVariant, FMath::Rand());
	IronKitBuilder::Build(GetMesh(), KitId, Variant);
	if (bUnarmed)
	{
		// A cargo truck: nothing on the roof (the weapon components stay, unused).
		for (UStaticMeshComponent* StandIn : { PrimaryWeaponMesh.Get(), SecondaryWeaponMesh.Get() })
		{
			if (StandIn)
			{
				StandIn->SetVisibility(false);
			}
		}
	}
	else
	{
		BuildWeaponRigs();
	}
	ApplyClassTraits();

	UMaterialInterface* SoftSmoke = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft"));
	UMaterialInterface* SoftFlame = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft"));
	SkidSmoke->Init(SoftSmoke, 10);
	ExhaustFx = NewObject<UIronPuffEmitter>(this, TEXT("ExhaustFx"));
	ExhaustFx->SetupAttachment(GetMesh());
	ExhaustFx->RegisterComponent();
	ExhaustFx->Init(SoftFlame ? SoftFlame : SoftSmoke, 12);
	// What the wheels kick up depends on the battlefield: sand in the desert and on the coast, snow
	// in the arctic; the city streets are paved.
	const FString Map = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (Map.Contains(TEXT("Desert")))
	{
		GroundDustColor = FLinearColor(0.62f, 0.5f, 0.34f, 0.7f);
	}
	else if (Map.Contains(TEXT("Coast")))
	{
		GroundDustColor = FLinearColor(0.62f, 0.56f, 0.45f, 0.55f);
	}
	else if (Map.Contains(TEXT("Arctic")))
	{
		GroundDustColor = FLinearColor(0.9f, 0.93f, 0.97f, 0.7f);
	}
	if (GroundDustColor.A > 0.f)
	{
		WheelDust = NewObject<UIronPuffEmitter>(this, TEXT("WheelDust"));
		WheelDust->SetupAttachment(GetMesh());
		WheelDust->RegisterComponent();
		WheelDust->Init(SoftSmoke, 32);
	}
	// Damage smoke and flames share one pool; each puff carries its own colour.
	DamageFx->Init(SoftFlame ? SoftFlame : SoftSmoke, 22); // Also carries the flames of a car set alight.

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		// Ram damage needs hit events from the physics body.
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &AWarVehiclePawn::HandleMeshHit);
	}
	if (USoundBase* Sound = EngineSound.LoadSynchronous())
	{
		EngineAudio->SetSound(Sound);
		EngineAudio->Play();
	}
	if (USoundBase* High = UVehicleWeaponComponent::LoadIfPresent(EngineHighSound))
	{
		EngineHighAudio = NewObject<UAudioComponent>(this, TEXT("EngineHighAudio"));
		EngineHighAudio->SetupAttachment(GetMesh());
		EngineHighAudio->SetRelativeLocation(EngineAudio->GetRelativeLocation());
		EngineHighAudio->bAutoActivate = false;
		EngineHighAudio->RegisterComponent();
		EngineHighAudio->SetSound(High);
		EngineHighAudio->SetVolumeMultiplier(0.f);
		EngineHighAudio->Play();
	}
	if (const UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		BaseEngineTorque = Wheeled->EngineSetup.MaxTorque;
	}
}

void AWarVehiclePawn::MoveForward(float Value)
{
	// Chaos throttle is 0..1; negative input is brake, which becomes reverse once stopped
	// (the template's "reverse as brake" behaviour).
	if (UChaosVehicleMovementComponent* Movement = GetVehicleMovement())
	{
		// A tesla strike kills the engine for a moment (braking still works).
		const float V = FMath::Clamp(Value, -1.f, StunTimeLeft > 0.f ? 0.f : 1.f);
		RequestedThrottle = FMath::Max(V, 0.f);
		RequestedBrake = FMath::Max(-V, 0.f);
		Movement->SetThrottleInput(IronHandling::GovernThrottle(RequestedThrottle, GetSpeedKph(), GetTopSpeedLimitKph()));
		Movement->SetBrakeInput(IronHandling::ReverseInput(RequestedBrake, Movement->GetForwardSpeed() * 0.036f, ClassReverseKph));
	}
}

void AWarVehiclePawn::Steer(float Value)
{
	// Applied gradually in Tick (see SteeringResponse / SteeringScale).
	TargetSteer = FMath::Clamp(Value, -1.f, 1.f);
}

void AWarVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UChaosVehicleMovementComponent* Movement = GetVehicleMovement();
	if (!Movement)
	{
		return;
	}
	IronHandling::SteeringTuning Tuning;
	Tuning.Scale = SteeringScale * SteeringSensitivity;
	Tuning.HighSpeedScale = HighSpeedSteeringScale;
	Tuning.HighSpeedKph = HighSpeedKph;
	Tuning.Response = SteeringResponse;
	SmoothedSteer = IronHandling::EaseSteer(SmoothedSteer, TargetSteer, DeltaSeconds, Tuning.Response);
	Movement->SetSteeringInput(IronHandling::SteeringOutput(SmoothedSteer, GetSpeedKph(), Tuning));

	// Nitro: extra torque while the meter lasts, plus a camera FOV kick so speed reads on screen.
	IronBoost::Tick(Boost, bBoostHeld, DeltaSeconds, BoostTuning);
	if (Boost.Active != bBoostWasActive)
	{
		SetEngineTorque();
		if (Boost.Active)
		{
			if (USoundBase* Sound = BoostSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
			}
			RumbleController(0.35f, 0.25f);
		}
		bBoostWasActive = Boost.Active;
	}
	if (Camera && !bCockpitView)
	{
		Camera->SetFieldOfView(ViewFov + IronBoost::FovBonus(Boost));
	}
	// The throttle is re-governed every frame: the speed changes even when the pedal does not.
	if (!Health || !Health->IsDestroyed())
	{
		Movement->SetThrottleInput(IronHandling::GovernThrottle(StunTimeLeft > 0.f ? 0.f : RequestedThrottle, GetSpeedKph(), GetTopSpeedLimitKph()));
		Movement->SetBrakeInput(IronHandling::ReverseInput(RequestedBrake, Movement->GetForwardSpeed() * 0.036f, ClassReverseKph));
	}
	if (WheelSettingsClock >= 0.f)
	{
		const float Before = WheelSettingsClock;
		WheelSettingsClock += DeltaSeconds;
		if ((Before < 0.25f && WheelSettingsClock >= 0.25f) || (Before < 1.f && WheelSettingsClock >= 1.f))
		{
			ApplyWheelSettings();
		}
		if (WheelSettingsClock >= 1.f)
		{
			WheelSettingsClock = -1.f;
		}
	}
	TickAssists(DeltaSeconds);
	TickAimAssist(DeltaSeconds);
	TickTurret(DeltaSeconds);
	TickFlipRecovery(DeltaSeconds);
	TickSkid(DeltaSeconds);
	TickDamageFx(DeltaSeconds);
	TickMissileRack();
	TickLockOn(DeltaSeconds);
	Flares.Tick(DeltaSeconds, FlareTuning);
	TickThreats(DeltaSeconds);
	TickAbility(DeltaSeconds);
	TickBurn(DeltaSeconds);
	TickEnergyWeapons(DeltaSeconds);

	TickEngineAudio(DeltaSeconds);
	TickCarFx(DeltaSeconds);
}

void AWarVehiclePawn::TickCarFx(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !BodyBox.IsValid || (Health && Health->IsDestroyed()))
	{
		return;
	}
	const FTransform& Frame = Body->GetComponentTransform();
	const FVector Half = BodyBox.GetExtent();
	// Nitro: blue-cored flame licking out of twin exhausts at the tail, low down.
	if (Boost.Active && ExhaustFx)
	{
		ExhaustTimer -= DeltaSeconds;
		if (ExhaustTimer <= 0.f)
		{
			ExhaustTimer = 0.035f;
			FIronPuffStyle Flame;
			Flame.Life = 0.2f;
			Flame.StartScale = 0.28f;
			Flame.EndScale = 0.07f;
			Flame.Drift = FVector::ZeroVector;
			Flame.StartColor = FLinearColor(1.2f, 2.2f, 7.f);
			Flame.EndColor = FLinearColor(4.f, 1.3f, 0.2f);
			Flame.PeakOpacity = 0.95f;
			Flame.bSwell = false;
			const FVector Back = -GetActorForwardVector();
			for (const float Side : { -0.28f, 0.28f })
			{
				const FVector Pipe = Frame.TransformPosition(FVector(BodyBox.Min.X + 4.f, BodyBox.GetCenter().Y + Side * Half.Y, BodyBox.Min.Z + Half.Z * 0.35f));
				ExhaustFx->Emit(Pipe, Back * FMath::FRandRange(300.f, 500.f) + GetVelocity(), Flame, FMath::FRandRange(0.8f, 1.2f));
			}
		}
	}
	// Dust: off the rear wheels at speed, more and bigger the faster, alternating sides.
	if (WheelDust && GetWheelsOnGround() >= 2)
	{
		const float Kph = GetSpeedKph();
		if (Kph > 25.f)
		{
			DustTimer -= DeltaSeconds;
			if (DustTimer <= 0.f)
			{
				const float Pace = FMath::Clamp((Kph - 25.f) / 100.f, 0.f, 1.f);
				DustTimer = FMath::Lerp(0.08f, 0.04f, Pace);
				FIronPuffStyle Dust;
				Dust.Life = FMath::Lerp(0.9f, 1.4f, Pace);
				Dust.StartScale = 0.5f;
				Dust.EndScale = FMath::Lerp(1.6f, 3.f, Pace);
				Dust.Drift = FVector(0.f, 0.f, 50.f);
				Dust.StartColor = Dust.EndColor = FLinearColor(GroundDustColor.R, GroundDustColor.G, GroundDustColor.B);
				Dust.PeakOpacity = GroundDustColor.A * FMath::Lerp(0.6f, 1.f, Pace);
				// Both rear wheels throw a plume, lifted a little and falling back behind the car.
				for (const float Side : { -0.8f, 0.8f })
				{
					const FVector Wheel = Frame.TransformPosition(FVector(BodyBox.Min.X + Half.X * 0.35f, BodyBox.GetCenter().Y + Side * Half.Y, BodyBox.Min.Z + 15.f));
					WheelDust->Emit(Wheel, -GetActorForwardVector() * 200.f + FVector(0.f, 0.f, 80.f), Dust);
				}
			}
		}
	}
}

void AWarVehiclePawn::TickEngineAudio(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Wheeled || !EngineAudio || !EngineAudio->IsPlaying())
	{
		if (EngineHighAudio && (!EngineAudio || !EngineAudio->IsPlaying()))
		{
			EngineHighAudio->SetVolumeMultiplier(0.f);
		}
		return;
	}
	// Engine note in two layers: the idle loop carries low revs, the harsh high-rev loop takes over
	// as the revs climb under throttle. Pitch follows RPM scaled by the class's voice (a truck sits
	// low, a sports car screams); enemies are a bit quieter than the player.
	const float MaxRpm = FMath::Max(Wheeled->GetEngineMaxRotationSpeed(), EngineAudioMaxRpm * 0.5f);
	const float RpmAlpha = FMath::Clamp(Wheeled->GetEngineRotationSpeed() / MaxRpm, 0.f, 1.f);
	const float Throttle = FMath::Clamp(Wheeled->GetThrottleInput(), 0.f, 1.f);
	const float Boosted = Boost.Active ? 1.08f : 1.f;
	const float Loud = IsPlayerControlled() ? 1.f : 0.6f;
	const float HighMix = FMath::SmoothStep(0.3f, 0.78f, RpmAlpha) * (0.45f + 0.55f * Throttle);
	EngineAudio->SetPitchMultiplier(ClassEnginePitch * (0.7f + 1.2f * RpmAlpha) * Boosted);
	EngineAudio->SetVolumeMultiplier((0.45f + 0.55f * Throttle) * (1.f - 0.65f * HighMix) * Loud);
	if (EngineHighAudio)
	{
		// The high loop is recorded an octave up, so it starts from half pitch.
		EngineHighAudio->SetPitchMultiplier(ClassEnginePitch * (0.5f + 0.75f * RpmAlpha) * Boosted);
		EngineHighAudio->SetVolumeMultiplier(HighMix * 0.9f * Loud);
	}
	// Upshift under power: the clutch clunk and blow-off.
	const int32 Gear = Wheeled->GetCurrentGear();
	if (Gear > LastGear && LastGear >= 1 && Throttle > 0.3f)
	{
		if (USoundBase* Shift = UVehicleWeaponComponent::LoadIfPresent(GearShiftSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Shift, GetActorLocation(), 0.7f * Loud, ClassEnginePitch);
		}
	}
	LastGear = Gear;
}

void AWarVehiclePawn::SetHandbrake(bool bEngaged)
{
	bHandbrakeHeld = bEngaged;
	if (UChaosVehicleMovementComponent* Movement = GetVehicleMovement())
	{
		Movement->SetHandbrakeInput(bEngaged);
	}
}

FVector AWarVehiclePawn::GetAimOrigin() const
{
	return MuzzlePoint ? MuzzlePoint->GetComponentLocation() : GetActorLocation();
}

FVector AWarVehiclePawn::GetAimDirection() const
{
	// Level shot along the camera's heading (the chase camera looks down at the car).
	FVector Direction = Camera ? Camera->GetForwardVector() : GetActorForwardVector();
	Direction.Z = 0.f;
	return Direction.IsNearlyZero() ? GetActorForwardVector() : Direction.GetSafeNormal();
}

void AWarVehiclePawn::FirePrimary()
{
	if (PrimaryWeapon && PrimaryWeapon->TryFire(GetAimOrigin(), GetFireDirection()))
	{
		RumbleController(0.25f, 0.08f);
		Recoil = 1.f;
		LastGunShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	else if (PrimaryWeapon && (PrimaryWeapon->GetAmmo() <= 0 || PrimaryWeapon->IsOverheated() || PrimaryWeapon->IsReloading()))
	{
		PrimaryWeapon->DryFire();
	}
}

void AWarVehiclePawn::FirePrimaryAt(const FVector& TargetLocation)
{
	if (PrimaryWeapon)
	{
		const FVector Origin = GetAimOrigin();
		AiAimDirection = (TargetLocation - Origin).GetSafeNormal();
		AiAimTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (PrimaryWeapon->TryFire(Origin, AiAimDirection))
		{
			Recoil = 1.f;
			LastGunShotTime = AiAimTime;
		}
	}
}

void AWarVehiclePawn::FireSecondary()
{
	if (SecondaryWeapon && PodMuzzle)
	{
		// The player's missiles home on whatever the pod has locked; a hunter's always home on the player.
		AActor* Guide = LockTarget.Get();
		if (bAIGuidedMissiles && !IsPlayerControlled())
		{
			Guide = UGameplayStatics::GetPlayerPawn(this, 0);
		}
		SecondaryWeapon->SetLaunchPoint(PodMuzzle->GetComponentLocation(), Guide);
	}
	if (SecondaryWeapon && SecondaryWeapon->TryFire(GetAimOrigin(), GetAimDirection()))
	{
		RumbleController(0.6f, 0.2f);
	}
}

void AWarVehiclePawn::ReloadPrimary()
{
	if (PrimaryWeapon)
	{
		PrimaryWeapon->StartReload();
	}
}

void AWarVehiclePawn::ReloadSecondary()
{
	if (SecondaryWeapon)
	{
		SecondaryWeapon->StartReload();
	}
}

void AWarVehiclePawn::ToggleCameraView()
{
	bCockpitView = !bCockpitView;
	if (SpringArm)
	{
		if (ChaseArmLength <= 0.f)
		{
			ChaseArmLength = SpringArm->TargetArmLength;
		}
		SpringArm->TargetArmLength = bCockpitView ? 0.f : ChaseArmLength;
	}
}

FVector AWarVehiclePawn::GetAimPoint() const
{
	// Where the primary weapon would hit right now: first blocking hit along the aim, else max range.
	const FVector Origin = GetAimOrigin();
	const FVector End = Origin + GetFireDirection() * 6000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeAimPoint), false, this);
	return GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

float AWarVehiclePawn::GetSpeedKph() const
{
	const UChaosVehicleMovementComponent* Movement = GetVehicleMovement();
	return Movement ? FMath::Abs(Movement->GetForwardSpeed()) * 0.036f : 0.f;
}

void AWarVehiclePawn::HandleDamaged(float Damage, AController* InstigatedBy)
{
	// Being hit shakes the pad harder the bigger the bite out of us.
	RumbleController(FMath::Clamp(Damage / 60.f, 0.15f, 1.f), 0.25f);
	// Down to a third of its health, the driver says so (the HUD keeps it from repeating).
	if (IsPlayerControlled() && Health && !Health->IsDestroyed() && Health->GetHealth() < Health->GetMaxHealth() * 0.35f)
	{
		if (const APlayerController* Me = Cast<APlayerController>(GetController()))
		{
			if (AIronSiegeHUD* HUD = Me->GetHUD<AIronSiegeHUD>())
			{
				HUD->Bark(static_cast<int32>(IronCrew::Bark::Hurt));
			}
		}
	}
	// Tell the player who landed the hit (hit marker + sound); kill confirmed if this finished us.
	APlayerController* Shooter = Cast<APlayerController>(InstigatedBy);
	if (Shooter && Shooter->GetPawn() != this)
	{
		if (AIronSiegeHUD* HUD = Shooter->GetHUD<AIronSiegeHUD>())
		{
			HUD->NotifyHitMarker(Health && Health->IsDestroyed());
			HUD->NotifyDamage(this, Damage, Health && Health->IsDestroyed());
		}
		if (Health && Health->IsDestroyed())
		{
			if (AWarVehiclePawn* KillerCar = Cast<AWarVehiclePawn>(Shooter->GetPawn()))
			{
				KillerCar->AddBoostCharge(IronBoost::Tuning().KillReward);
			}
		}
	}
}

void AWarVehiclePawn::HandleDestroyed()
{
	if (EngineAudio)
	{
		EngineAudio->FadeOut(0.3f, 0.f);
	}
	if (EngineHighAudio)
	{
		EngineHighAudio->FadeOut(0.3f, 0.f);
	}
	// Blow up: fireball, sound, and a shove that throws this wreck (and anything close) into the air.
	AIronExplosion::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f), 1.6f, 900.f, 650.f);

	if (UChaosVehicleMovementComponent* Movement = GetVehicleMovement())
	{
		Movement->SetThrottleInput(0.f);
		RequestedThrottle = 0.f;
		RequestedBrake = 0.f;
		TargetSteer = 0.f;
		Movement->SetHandbrakeInput(true);
	}
	// Charred wreck: body parts go near-black; glass and guns vanish; tyres stay as they are.
	UMaterialInterface* Charred = WreckMaterial.LoadSynchronous();
	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		const FString Name = Part->GetName();
		if (Part == PrimaryWeaponMesh || Part == SecondaryWeaponMesh || Name.Contains(TEXT("Glass")))
		{
			Part->SetVisibility(false);
			continue;
		}
		if (Charred && !Name.Contains(TEXT("Wheel")) && !Name.Contains(TEXT("Tire")))
		{
			for (int32 i = 0; i < Part->GetNumMaterials(); ++i)
			{
				Part->SetMaterial(i, Charred);
			}
		}
	}
	// Cars whose body lives in the skeletal mesh char the same way, except glass and tyres slots.
	if (USkeletalMeshComponent* Body = GetMesh(); Body && Charred)
	{
		const TArray<FName> Slots = Body->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const FString Slot = Slots[i].ToString();
			if (!Slot.Contains(TEXT("Glass")) && !Slot.Contains(TEXT("Window")) && !Slot.Contains(TEXT("Wheel")) && !Slot.Contains(TEXT("Tire")) && !Slot.Contains(TEXT("Tyre")))
			{
				Body->SetMaterial(i, Charred);
			}
		}
	}
	DetachFromControllerPendingDestroy();
}

void AWarVehiclePawn::ApplyUpgrades(const IronUpgrades::Loadout& Upgrades)
{
	if (Health)
	{
		// The driver's armor perk rides on top of the plating bought in the shop.
		UpgradeArmorScale = Upgrades.ArmorMultiplier();
		Health->SetArmorScale(UpgradeArmorScale * GetDriverPerk().MaxArmor);
	}
	if (PrimaryWeapon)
	{
		PrimaryWeapon->DamageMultiplier = Upgrades.MachineGunDamageMultiplier();
	}
	if (SecondaryWeapon)
	{
		SecondaryWeapon->DamageMultiplier = Upgrades.RocketDamageMultiplier();
	}
	if (Flamer)
	{
		Flamer->bUnlocked = Upgrades.HasFlamer();
		Flamer->DamageMultiplier = Upgrades.FlamerDamageMultiplier();
	}
	if (Railgun)
	{
		Railgun->bUnlocked = Upgrades.HasRailgun();
		Railgun->DamageMultiplier = Upgrades.RailgunDamageMultiplier();
	}
	if (Tesla)
	{
		Tesla->bUnlocked = Upgrades.HasTesla();
		Tesla->DamageMultiplier = Upgrades.TeslaDamageMultiplier();
	}
	if (MineWeapon)
	{
		// Buying the rack unlocks it and stocks spare mines; the rockets' damage upgrade covers
		// explosives generally, so mines ride along with it.
		const bool bWasLocked = !MineWeapon->bUnlocked;
		MineWeapon->bUnlocked = Upgrades.HasMines();
		MineWeapon->DamageMultiplier = Upgrades.RocketDamageMultiplier();
		if (bWasLocked && MineWeapon->bUnlocked)
		{
			MineWeapon->AddReserveAmmo(Upgrades.MineReserveBonus());
		}
	}
	UpgradeTorqueScale = Upgrades.EngineTorqueMultiplier();
	SetEngineTorque();
}

void AWarVehiclePawn::SetEngineTorque()
{
	UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (Wheeled && BaseEngineTorque > 0.f)
	{
		Wheeled->SetMaxEngineTorque(BaseEngineTorque * UpgradeTorqueScale * AbilityTorqueScale * IronBoost::TorqueMultiplier(Boost, BoostTuning));
	}
}

void AWarVehiclePawn::ApplyViewSettings(float FieldOfView, float DistanceScale, float HeightScale, float Smoothing, bool bReduceMotion)
{
	if (!SpringArm || !Camera)
	{
		return;
	}
	if (BaseArmLength <= 0.f)
	{
		// First call: remember this car's authored chase framing.
		BaseArmLength = ChaseArmLength > 0.f ? ChaseArmLength : SpringArm->TargetArmLength;
		BaseArmHeight = SpringArm->GetRelativeLocation().Z;
	}
	ChaseArmLength = BaseArmLength * DistanceScale;
	if (!bCockpitView)
	{
		SpringArm->TargetArmLength = ChaseArmLength;
	}
	FVector Loc = SpringArm->GetRelativeLocation();
	Loc.Z = BaseArmHeight * HeightScale;
	SpringArm->SetRelativeLocation(Loc);
	// Reduce motion: treat the smoothing slider as if it were much lower, so the camera tracks the
	// car tightly instead of swinging behind it.
	const float EffectiveSmoothing = bReduceMotion ? Smoothing * IronSettings::ShakeScale(true) : Smoothing;
	SpringArm->CameraLagSpeed = IronSettings::CameraLagSpeed(EffectiveSmoothing);
	SpringArm->CameraRotationLagSpeed = IronSettings::CameraRotationLagSpeed(EffectiveSmoothing);
	ViewFov = IronSettings::ClampFieldOfView(FieldOfView);
	Camera->SetFieldOfView(ViewFov + IronBoost::FovBonus(Boost));
}

void AWarVehiclePawn::ApplyControlSettings(float Sensitivity, float Smoothing)
{
	SteeringSensitivity = FMath::Clamp(Sensitivity, 0.25f, 2.f);
	SteeringResponse = IronSettings::SteeringResponse(Smoothing);
}

void AWarVehiclePawn::ApplyDrivingStyle(int32 Style)
{
	const IronSettings::DrivingFeel Feel = IronSettings::DrivingStyleFeel(Style);
	SteeringScale = Feel.SteeringScale;
	HighSpeedSteeringScale = Feel.HighSpeedScale;
	SteeringResponse = Feel.Response;
	Assist.YawGain = Feel.SpinAssist;
	Assist.RollGain = Feel.SpinAssist * 10.f; // Anti-roll with the same assists: off on simulation.
	Arcade = IronHandling::ArcadeTuning();
	Arcade.CornerG = Feel.CornerG * ClassTurn;
	Arcade.GripGain *= Feel.GripAssist;
	Arcade.GripMaxAccel *= Feel.GripAssist;
	// The class sets how nimble the slide is too: a truck swings round slower than a sports car.
	Arcade.DriftYawRate *= Feel.DriftAssist * ClassTurn;
	Arcade.HandbrakeYawRate *= Feel.DriftAssist * ClassTurn;
	Arcade.DriftKickDeg *= Feel.DriftAssist;
	Arcade.DriftThrust *= Feel.DriftAssist;
	Assist.AirLevelGain = Feel.AirControl * ClassAirControl;
	Assist.AirDamping = Feel.AirControl > 0.f ? 2.5f * ClassAirControl : 0.f;
	UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	if (!Wheeled)
	{
		return;
	}
	if (BaseBrakeTorques.Num() != Wheeled->WheelSetups.Num())
	{
		// Remember the Blueprint's own torques and grip once, so styles scale them instead of
		// stacking up - and instead of replacing them: the Chaos setters take absolute values.
		BaseBrakeTorques.Reset();
		BaseHandbrakeTorques.Reset();
		BaseFrictions.Reset();
		for (const FChaosWheelSetup& Setup : Wheeled->WheelSetups)
		{
			const UChaosVehicleWheel* Wheel = Setup.WheelClass ? Setup.WheelClass->GetDefaultObject<UChaosVehicleWheel>() : nullptr;
			BaseBrakeTorques.Add(Wheel ? Wheel->MaxBrakeTorque : 1500.f);
			BaseHandbrakeTorques.Add(Wheel ? Wheel->MaxHandBrakeTorque : 3000.f);
			BaseFrictions.Add(Wheel ? Wheel->FrictionForceMultiplier : 1.f);
		}
	}
	WheelFriction = Feel.FrictionMultiplier * ClassGrip;
	WheelBrake = Feel.BrakeMultiplier * ClassBrake;
	WheelHandbrake = Feel.HandbrakeMultiplier;
	ApplyWheelSettings();
	WheelSettingsClock = 0.f;
}

void AWarVehiclePawn::ApplyWheelSettings()
{
	UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	if (!Wheeled || BaseBrakeTorques.Num() != Wheeled->WheelSetups.Num() || BaseFrictions.Num() != Wheeled->WheelSetups.Num())
	{
		return;
	}
	for (int32 i = 0; i < Wheeled->WheelSetups.Num(); ++i)
	{
		const bool bRear = RearWheels.IsValidIndex(i) && RearWheels[i];
		Wheeled->SetWheelFrictionMultiplier(i, BaseFrictions[i] * WheelFriction * (Drift.bActive && bRear ? Arcade.DriftRearGrip : 1.f));
		Wheeled->SetWheelMaxBrakeTorque(i, BaseBrakeTorques[i] * WheelBrake);
		Wheeled->SetWheelHandbrakeTorque(i, BaseHandbrakeTorques[i] * WheelHandbrake);
		// ABS on every wheel, and every wheel braking: a locked tyre slides and cannot steer.
		Wheeled->SetAffectedByBrake(i, true);
		Wheeled->SetABSEnabled(i, true);
	}
	Wheeled->SetDownforceCoefficient(ClassDownforce);
}
void AWarVehiclePawn::RumbleController(float Strength, float Duration)
{
	if (AIronSiegePlayerController* PC = Cast<AIronSiegePlayerController>(GetController()))
	{
		PC->PlayRumble(Strength, Duration);
	}
}

void AWarVehiclePawn::AddBoostCharge(float Amount)
{
	IronBoost::AddCharge(Boost, Amount);
}

void AWarVehiclePawn::TickFlipRecovery(float DeltaSeconds)
{
	// Upside down and going nowhere: a wreck that can still drive should not end the run.
	const bool bOnRoof = GetActorUpVector().Z < 0.1f && GetSpeedKph() < 8.f;
	UprightSeconds = bOnRoof ? UprightSeconds + DeltaSeconds : 0.f;
	if (UprightSeconds < 2.5f)
	{
		return;
	}
	UprightSeconds = 0.f;
	const FRotator Upright(0.f, GetActorRotation().Yaw, 0.f);
	SetActorLocationAndRotation(GetActorLocation() + FVector(0.f, 0.f, 120.f), Upright, false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->ShowNotice(IronText::Str(TEXT("NoticeRecovered"), TEXT("RECOVERED")), FLinearColor(0.6f, 0.9f, 1.f));
		}
	}
}

void AWarVehiclePawn::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AWarVehiclePawn* Other = Cast<AWarVehiclePawn>(OtherActor);
	UWorld* World = GetWorld();
	if (!Other || Other == this || !World || !Health || Health->IsDestroyed())
	{
		return;
	}
	// Enemies do not ram each other into scrap: only hits involving the player's car count.
	if (!IsPlayerControlled() && !Other->IsPlayerControlled())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const IronDamage::RamTuning Tuning;
	if (const double* Last = LastRamTimes.Find(Other); Last && Now - *Last < Tuning.Cooldown)
	{
		return;
	}
	LastRamTimes.Add(Other, Now);

	// Closing speed along the impact normal, in kph, and how much heavier we are than them.
	const FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal() : -Hit.ImpactNormal;
	// Neither AActor::GetVelocity() nor the body's physics velocity reads back reliably on a Chaos
	// vehicle from the game thread, so build the closing speed from each car's own speedometer
	// (the same number the HUD shows) along the way it is pointing.
	const FVector Relative = GetActorForwardVector() * GetSpeedKph() - Other->GetActorForwardVector() * Other->GetSpeedKph();
	const float ClosingKph = FMath::Abs(FVector::DotProduct(Relative, Normal));
	const float MyMass = GetMesh() ? GetMesh()->GetMass() : 1.f;
	const float TheirMass = Other->GetMesh() ? Other->GetMesh()->GetMass() : 1.f;
	const float Damage = IronDamage::RamDamage(ClosingKph, TheirMass > 1.f ? MyMass / TheirMass : 1.f, Tuning);
	if (Damage <= 0.f)
	{
		return;
	}
	UGameplayStatics::ApplyPointDamage(Other, Damage, Normal, Hit, GetController(), this, UDamageType::StaticClass());
	// The rammer keeps a share of it, so charging a Juggernaut head-on is still a bad idea.
	UGameplayStatics::ApplyPointDamage(this, IronDamage::RamSelfDamage(Damage, Tuning), -Normal, Hit, nullptr, Other, UDamageType::StaticClass());
	if (USoundBase* Sound = RamSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint, FMath::Clamp(Damage / 60.f, 0.4f, 1.3f));
	}
	RumbleController(FMath::Clamp(Damage / 70.f, 0.2f, 1.f), 0.3f);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: ram %s -> %s at %.0f kph for %.0f damage"), *GetName(), *Other->GetName(), ClosingKph, Damage);
}

void AWarVehiclePawn::TickSkid(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return;
	}
	// Chaos vehicles do not report a usable velocity on the game thread, so measure it: where the
	// car actually went this frame, split into "along the nose" and "sideways".
	const FVector Location = GetActorLocation();
	const FVector Step = Location - PreviousLocation;
	PreviousLocation = Location;
	const FVector Velocity = Step / DeltaSeconds;
	LateralKph = FVector::DotProduct(Velocity, GetActorRightVector()) * 0.036f;
	const float SpeedKph = GetSpeedKph();
	const float Skid = IronSkid::Strength(SpeedKph, LateralKph, bHandbrakeHeld, IronSkid::Tuning());

	// Screech follows the slide.
	if (SkidAudio)
	{
		SkidAudio->SetVolumeMultiplier(IronSkid::ScreechVolume(Skid) * (IsPlayerControlled() ? 1.f : 0.5f));
		SkidAudio->SetPitchMultiplier(0.85f + 0.35f * Skid);
	}

	// Rear tyre positions, roughly: behind the middle and out to each side.
	// Just above the road rather than at the contact patch, or the puffs sit half-buried in it.
	const FVector Back = Location - GetActorForwardVector() * 160.f - FVector(0.f, 0.f, 20.f);
	const FVector Side = GetActorRightVector() * 85.f;

	if (Skid > 0.f)
	{
		SkidPuffTimer -= DeltaSeconds;
		if (SkidPuffTimer <= 0.f)
		{
			SkidPuffTimer = IronSkid::PuffDelay(Skid, IronSkid::Tuning());
			FIronPuffStyle Tyre;
			Tyre.Life = 0.7f;
			Tyre.StartScale = 0.3f * (0.6f + 0.4f * Skid);
			Tyre.EndScale = 1.15f;
			Tyre.PeakOpacity = 0.75f;
			for (const FVector& Wheel : { Back - Side, Back + Side })
			{
				SkidSmoke->Emit(Wheel + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), 0.f), FVector::ZeroVector, Tyre);
			}
		}
		SkidMarkDistance += FVector::Dist2D(Location, LastSkidLocation);
		LastSkidLocation = Location;
		if (IronSkid::ShouldDropMark(SkidMarkDistance, Skid, IronSkid::Tuning()))
		{
			SkidMarkDistance = 0.f;
			if (UMaterialInterface* Mark = SkidMarkMaterial.LoadSynchronous())
			{
				const FRotator Facing(-90.f, GetActorRotation().Yaw, 0.f); // Project straight down.
				// Depth, half-width, half-length: a narrow streak along the car's heading.
				const FVector Size(60.f, 13.f, 50.f);
				UGameplayStatics::SpawnDecalAtLocation(this, Mark, Size, Back - Side, Facing, 14.f);
				UGameplayStatics::SpawnDecalAtLocation(this, Mark, Size, Back + Side, Facing, 14.f);
			}
		}
	}
	else
	{
		LastSkidLocation = Location;
		SkidMarkDistance = 0.f;
	}
}


void AWarVehiclePawn::DeployMine()
{
	if (MineWeapon && MineWeapon->TryFire(GetActorLocation(), GetActorForwardVector()))
	{
		RumbleController(0.3f, 0.12f);
	}
}

void AWarVehiclePawn::FireFlamer()
{
	// Held, not tapped: the weapon's own fire interval throttles it, and the heat lockout stops it.
	if (Flamer && Flamer->TryFire(GetAimOrigin(), GetAimDirection()))
	{
		RumbleController(0.12f, 0.08f);
	}
}

void AWarVehiclePawn::TickDamageFx(float DeltaSeconds)
{
	if (!Health || !DamageFx)
	{
		return;
	}
	const float Total = Health->GetMaxHealth();
	const float Fraction = Total > 0.f ? FMath::Clamp(Health->GetHealth() / Total, 0.f, 1.f) : 1.f;
	const bool bWreck = Health->IsDestroyed();
	if (bWreck)
	{
		// A wreck smoulders for a while, then goes quiet so old fights do not fog the map.
		WreckSmokeSeconds += DeltaSeconds;
		if (WreckSmokeSeconds > 25.f)
		{
			return;
		}
	}
	else if (Fraction >= 0.5f)
	{
		return;
	}
	// Worse damage = denser smoke: from a lazy wisp at half health to a thick column when dead.
	const float Severity = bWreck ? 1.f : FMath::Clamp((0.5f - Fraction) / 0.5f, 0.f, 1.f);
	DamageFxTimer -= DeltaSeconds;
	if (DamageFxTimer > 0.f)
	{
		return;
	}
	DamageFxTimer = FMath::Lerp(0.22f, 0.06f, Severity);

	// Under the bonnet: front half of the chassis, just above its top.
	const FVector Centre = BodyBox.GetCenter();
	const FVector Half = BodyBox.GetExtent();
	const FVector Bonnet = GetMesh()->GetComponentTransform().TransformPosition(Centre + FVector(Half.X * 0.45f, 0.f, Half.Z * 0.6f));

	FIronPuffStyle Smoke;
	Smoke.Life = 1.3f;
	Smoke.StartScale = 0.35f;
	Smoke.EndScale = FMath::Lerp(1.3f, 2.2f, Severity);
	Smoke.Drift = FVector(0.f, 0.f, 180.f);
	Smoke.StartColor = Smoke.EndColor = FLinearColor(0.05f, 0.048f, 0.05f);
	Smoke.PeakOpacity = FMath::Lerp(0.35f, 0.8f, Severity);
	DamageFx->Emit(Bonnet + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), 0.f), FVector::ZeroVector, Smoke);

	// Below a quarter health (and on a fresh wreck) the engine is on fire.
	if (Fraction < 0.25f && WreckSmokeSeconds < 12.f)
	{
		FIronPuffStyle Fire;
		Fire.Life = 0.45f;
		Fire.StartScale = 0.25f;
		Fire.EndScale = 0.75f;
		Fire.Drift = FVector(0.f, 0.f, 260.f);
		Fire.StartColor = FLinearColor(3.5f, 1.2f, 0.15f);
		Fire.EndColor = FLinearColor(0.4f, 0.1f, 0.02f);
		Fire.PeakOpacity = 0.9f;
		Fire.bSwell = false;
		DamageFx->Emit(Bonnet + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), -10.f), FVector::ZeroVector, Fire);
	}
}

void AWarVehiclePawn::BuildWeaponRigs()
{
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!Chassis)
	{
		return;
	}
	// The rifle stand-ins stay only as mount markers: their X along the roof is where each rig goes,
	// and the rigs sit on the top of the body mesh.
	const float Roof = BodyBox.Max.Z + 2.f;
	CannonMount = FVector(PrimaryWeaponMesh ? PrimaryWeaponMesh->GetRelativeLocation().X : 0.f, 0.f, Roof);
	PodMount = FVector((SecondaryWeaponMesh ? SecondaryWeaponMesh->GetRelativeLocation().X : -75.f) - 20.f, 0.f, Roof);
	for (UStaticMeshComponent* Old : { PrimaryWeaponMesh.Get(), SecondaryWeaponMesh.Get() })
	{
		if (Old)
		{
			Old->SetVisibility(false);
		}
	}
	const IronKitBuilder::FRigBuild Cannon = IronKitBuilder::BuildRig(Chassis, CannonMount, IronRigs::Rig::Autocannon);
	TurretHinge = Cannon.Turret;
	BarrelHinge = Cannon.Barrels;
	BarrelRest = BarrelHinge ? BarrelHinge->GetRelativeLocation() : FVector::ZeroVector;
	PodMissiles.Reset();
	const IronKitBuilder::FRigBuild Pod = IronKitBuilder::BuildRig(Chassis, PodMount, IronRigs::Rig::MissilePod, &PodMissiles);

	// Muzzle markers the weapons fire (and draw tracers) from; the cannon's turns with its turret.
	auto MakeMuzzle = [this, Chassis](const FVector& Mount, IronRigs::Rig Rig, bool bDetailed, USceneComponent* Turret, const TCHAR* Name)
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
		IronRigs::MuzzleOffset(Rig, X, Y, Z, bDetailed);
		const IronRigs::Pivots Hinge = IronRigs::GetPivots(Rig, bDetailed);
		USceneComponent* Muzzle = NewObject<USceneComponent>(this, Name);
		Muzzle->SetupAttachment(Turret ? Turret : Chassis);
		Muzzle->SetRelativeLocation(Turret ? FVector(X - Hinge.TurretX, Y - Hinge.TurretY, Z - Hinge.TurretZ) : Mount + FVector(X, Y, Z));
		Muzzle->RegisterComponent();
		return Muzzle;
	};
	CannonMuzzle = MakeMuzzle(CannonMount, IronRigs::Rig::Autocannon, Cannon.bDetailed, TurretHinge, TEXT("CannonMuzzle"));
	PodMuzzle = MakeMuzzle(PodMount, IronRigs::Rig::MissilePod, Pod.bDetailed, nullptr, TEXT("PodMuzzle"));
	UE_LOG(LogTemp, Log, TEXT("IronSiege: %s rigs - cannon %s (%d parts), pod %s (%d parts)"), *GetName(),
		Cannon.bDetailed ? TEXT("modelled") : TEXT("basic"), Cannon.Parts, Pod.bDetailed ? TEXT("modelled") : TEXT("basic"), Pod.Parts);
	if (PrimaryWeapon)
	{
		PrimaryWeapon->SetVisualMuzzle(CannonMuzzle);
	}
	if (Railgun)
	{
		Railgun->SetVisualMuzzle(CannonMuzzle);
	}
	if (Tesla)
	{
		Tesla->SetVisualMuzzle(CannonMuzzle);
	}
	ExtraWeaponModels.Reset();
	const FVector Rear(BodyBox.Min.X, 0.f, BodyBox.Min.Z + 0.4f * (BodyBox.Max.Z - BodyBox.Min.Z));
	for (UStaticMeshComponent* Model : IronKitBuilder::BuildExtraWeapons(Chassis, TurretHinge, Rear))
	{
		ExtraWeaponModels.Add(Model);
	}
}

void AWarVehiclePawn::TickMissileRack()
{
	// A weapon bought in the shop (or built into an enemy) appears on the car.
	const bool bUnlocked[] = { Railgun && Railgun->bUnlocked, Tesla && Tesla->bUnlocked, Flamer && Flamer->bUnlocked, MineWeapon && MineWeapon->bUnlocked };
	for (int32 i = 0; i < ExtraWeaponModels.Num() && i < UE_ARRAY_COUNT(bUnlocked); ++i)
	{
		UStaticMeshComponent* Model = ExtraWeaponModels[i].Get();
		if (Model && Model->IsVisible() != bUnlocked[i])
		{
			Model->SetVisibility(bUnlocked[i]);
		}
	}
	// The pod shows one missile per round left in the magazine; a reload puts them all back.
	const int32 Loaded = SecondaryWeapon ? SecondaryWeapon->GetAmmo() : 0;
	if (Loaded == ShownMissiles)
	{
		return;
	}
	ShownMissiles = Loaded;
	for (const TPair<int32, UStaticMeshComponent*>& Missile : PodMissiles)
	{
		if (Missile.Value)
		{
			Missile.Value->SetVisibility(IronRigs::IsMissileVisible(Missile.Key, Loaded));
		}
	}
}

void AWarVehiclePawn::TickLockOn(float DeltaSeconds)
{
	// Only the player's launcher locks on: enemy rockets stay unguided so they can be dodged.
	const bool bCanLock = IsPlayerControlled() && SecondaryWeapon && SecondaryWeapon->GetAmmo() > 0 && !SecondaryWeapon->IsReloading();
	if (!bCanLock)
	{
		LockTarget.Reset();
		return;
	}
	LockScanTimer -= DeltaSeconds;
	if (LockScanTimer > 0.f)
	{
		return;
	}
	LockScanTimer = 0.1f;
	const FVector From = PodMuzzle ? PodMuzzle->GetComponentLocation() : GetAimOrigin();
	TArray<AActor*> Candidates;
	TArray<IronMissiles::Vec> ToTargets;
	for (TActorIterator<AWarVehiclePawn> It(GetWorld()); It; ++It)
	{
		AWarVehiclePawn* Other = *It;
		if (Other == this || IronTeams::IsPlayerSide(Other) || !Other->GetHealthComponent() || Other->GetHealthComponent()->IsDestroyed())
		{
			continue;
		}
		// Bearing only: car origins sit on the road and the pod on the roof, so up close the height
		// difference alone would push a target dead ahead out of the cone. The missile fixes height.
		const FVector To = Other->GetActorLocation() - From;
		Candidates.Add(Other);
		ToTargets.Add({ float(To.X), float(To.Y), 0.f });
	}
	const FVector Aim = GetAimDirection().GetSafeNormal2D();
	const int32 Pick = IronMissiles::PickTarget({ float(Aim.X), float(Aim.Y), float(Aim.Z) }, ToTargets.GetData(), ToTargets.Num());
	AActor* NewTarget = Pick >= 0 ? Candidates[Pick] : nullptr;
	if (NewTarget && NewTarget != LockTarget.Get())
	{
		if (USoundBase* Beep = LockOnSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Beep, 0.8f);
		}
	}
	LockTarget = NewTarget;
}

void AWarVehiclePawn::FireFlares()
{
	UWorld* World = GetWorld();
	if (!World || (Health && Health->IsDestroyed()) || !Flares.TryFire())
	{
		return;
	}
	// A fan of four thrown high and wide off the rear of the roof (carried along with part of the
	// car's own speed): a decoyed missile then climbs after them and bursts in the air well clear of
	// the car, instead of diving onto the road beside it.
	USkeletalMeshComponent* Chassis = GetMesh();
	const FVector Local = BodyBox.IsValid ? FVector(BodyBox.Min.X + 30.f, 0.f, BodyBox.Max.Z) : FVector(-150.f, 0.f, 150.f);
	const FVector From = Chassis ? Chassis->GetComponentTransform().TransformPosition(Local) : GetActorLocation() + FVector(0.f, 0.f, 150.f);
	const float Forward = GetVehicleMovement() ? GetVehicleMovement()->GetForwardSpeed() : 0.f;
	const FVector Carry = GetActorForwardVector() * Forward * 0.6f;
	TArray<AActor*> Decoys;
	for (int32 i = 0; i < 4; ++i)
	{
		const float Side = (i - 1.5f) / 1.5f;
		const FVector Throw = -GetActorForwardVector() * 900.f + GetActorRightVector() * Side * 1100.f + FVector(0.f, 0.f, 1800.f);
		if (AIronFlare* Flare = AIronFlare::Spawn(World, From, Throw + Carry))
		{
			Decoys.Add(Flare);
		}
	}
	if (USoundBase* Pop = FlareSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Pop, From);
	}
	RumbleController(0.3f, 0.15f);

	// Every missile homing on us from inside the decoy range bites on one of the flares; ones still
	// far out are not fooled, so flares fired too early are wasted.
	int32 Pulled = 0;
	for (TActorIterator<ARocketProjectile> It(World); It; ++It)
	{
		if (Decoys.Num() > 0 && It->IsLive() && It->GetTarget() == this && IronFlares::IsDecoyed(FVector::Dist(It->GetActorLocation(), GetActorLocation())))
		{
			It->Redirect(Decoys[FMath::RandRange(0, Decoys.Num() - 1)]);
			++Pulled;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: flares fired - %d missile(s) decoyed, %d charge(s) left"), Pulled, Flares.Charges);
}

void AWarVehiclePawn::TickThreats(float DeltaSeconds)
{
	// Missile warning for the player: the nearest guided missile homing on us, and a beep that
	// quickens as it closes (IronFlares::WarningBeepInterval).
	IncomingMissile.Reset();
	IncomingDistance = -1.f;
	if (!IsPlayerControlled() || !GetWorld())
	{
		return;
	}
	ARocketProjectile* Nearest = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<ARocketProjectile> It(GetWorld()); It; ++It)
	{
		if (It->IsLive() && It->GetTarget() == this)
		{
			const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (D < Best)
			{
				Best = D;
				Nearest = *It;
			}
		}
	}
	if (!Nearest)
	{
		WarnBeepTimer = 0.f;
		return;
	}
	IncomingMissile = Nearest;
	IncomingDistance = Best;
	// Settings: automatic flares once the missile is well inside the decoy range.
	const UIronSiegeUserSettings* UserSettings = UIronSiegeUserSettings::Get();
	if (IronSettings::ShouldAutoFlare(UserSettings && UserSettings->bAutoFlares, Best, IronFlares::Tuning().DecoyRangeCm))
	{
		FireFlares(); // No-op while cooling down or out of charges.
	}
	const float Interval = IronFlares::WarningBeepInterval(Best);
	WarnBeepTimer -= DeltaSeconds;
	if (Interval > 0.f && WarnBeepTimer <= 0.f)
	{
		WarnBeepTimer = Interval;
		if (USoundBase* Beep = MissileWarnSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Beep, 0.45f); // A square-ish chirp: loud per unit volume.
		}
	}
}

void AWarVehiclePawn::Ignite(float Dps, AController* By)
{
	if (Health && Health->IsDestroyed())
	{
		return;
	}
	if (!Burn.IsBurning())
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: %s set alight (%.0f hp)"), *GetName(), Health ? Health->GetHealth() : 0.f);
	}
	Burn.Ignite(Dps);
	BurnInstigator = By;
}

void AWarVehiclePawn::TickBurn(float DeltaSeconds)
{
	if (!Burn.IsBurning())
	{
		return;
	}
	if (Health && Health->IsDestroyed())
	{
		Burn = IronDamage::BurnState();
		BurnPending = 0.f;
		return;
	}
	// Damage goes out in small chunks rather than every frame, so hit markers do not flood.
	BurnPending += Burn.Tick(DeltaSeconds);
	if (BurnPending >= 5.f || !Burn.IsBurning())
	{
		AController* By = BurnInstigator.Get();
		UGameplayStatics::ApplyDamage(this, BurnPending, By, By ? By->GetPawn() : nullptr, UDamageType::StaticClass());
		BurnPending = 0.f;
		if (!Burn.IsBurning())
		{
			UE_LOG(LogTemp, Log, TEXT("IronSiege: %s fire out (%.0f hp)"), *GetName(), Health ? Health->GetHealth() : 0.f);
		}
	}
	// Flames licking up off random spots on the body while it burns.
	BurnPuffTimer -= DeltaSeconds;
	USkeletalMeshComponent* Chassis = GetMesh();
	if (BurnPuffTimer <= 0.f && DamageFx && Chassis && BodyBox.IsValid)
	{
		BurnPuffTimer = 0.05f;
		const FVector Local(FMath::FRandRange(BodyBox.Min.X, BodyBox.Max.X) * 0.8f, FMath::FRandRange(BodyBox.Min.Y, BodyBox.Max.Y) * 0.8f, BodyBox.Max.Z - 20.f);
		FIronPuffStyle Fire;
		Fire.Life = 0.4f;
		Fire.StartScale = 0.3f;
		Fire.EndScale = 0.8f;
		Fire.Drift = FVector(0.f, 0.f, 280.f);
		Fire.StartColor = FLinearColor(3.5f, 1.3f, 0.2f);
		Fire.EndColor = FLinearColor(0.35f, 0.1f, 0.03f);
		Fire.PeakOpacity = 0.85f;
		Fire.bSwell = false;
		DamageFx->Emit(Chassis->GetComponentTransform().TransformPosition(Local), FVector::ZeroVector, Fire);
	}
}

void AWarVehiclePawn::FireRailgun()
{
	if (!Railgun || !Railgun->bUnlocked || RailCharge.IsCharging() || !Railgun->CanFire() || (Health && Health->IsDestroyed()))
	{
		return;
	}
	RailCharge.Start();
	if (USoundBase* Whine = Railgun->ChargeSound.LoadSynchronous())
	{
		UGameplayStatics::SpawnSoundAttached(Whine, GetMesh());
	}
}

void AWarVehiclePawn::FireRailgunAt(const FVector& TargetLocation)
{
	if (!Railgun || !Railgun->bUnlocked || RailCharge.IsCharging() || !Railgun->CanFire())
	{
		return;
	}
	FireRailgun();
	if (RailCharge.IsCharging())
	{
		bRailAimFixed = true;
		RailAimPoint = TargetLocation;
	}
}

void AWarVehiclePawn::FireTesla()
{
	if (Tesla && Tesla->bUnlocked && Tesla->TryFire(GetAimOrigin(), GetFireDirection()))
	{
		RumbleController(0.4f, 0.2f);
	}
}

void AWarVehiclePawn::Stun(float Seconds)
{
	if (Health && Health->IsDestroyed())
	{
		return;
	}
	StunTimeLeft = FMath::Max(StunTimeLeft, Seconds);
	if (UChaosVehicleMovementComponent* Movement = GetVehicleMovement())
	{
		Movement->SetThrottleInput(0.f);
	}
}

void AWarVehiclePawn::TickEnergyWeapons(float DeltaSeconds)
{
	// Railgun: the slug leaves once the charge completes, along wherever the car aims by then.
	// A car destroyed mid-charge drops it: a wreck does not fire.
	if (Railgun && RailCharge.IsCharging() && Health && Health->IsDestroyed())
	{
		RailCharge = IronBeams::ChargeClock();
		bRailAimFixed = false;
		Railgun->SetChargeProgress(0.f);
	}
	if (Railgun && RailCharge.IsCharging())
	{
		if (RailCharge.Tick(DeltaSeconds))
		{
			const FVector Direction = bRailAimFixed ? (RailAimPoint - GetAimOrigin()).GetSafeNormal() : GetFireDirection();
			bRailAimFixed = false;
			if (Railgun->TryFire(GetAimOrigin(), Direction))
			{
				RumbleController(0.8f, 0.3f);
			}
			Railgun->SetChargeProgress(0.f);
		}
		else
		{
			Railgun->SetChargeProgress(RailCharge.Progress());
		}
	}

	// Stunned: blue sparks crawl over the body until the engine comes back.
	if (StunTimeLeft > 0.f)
	{
		StunTimeLeft -= DeltaSeconds;
		StunSparkTimer -= DeltaSeconds;
		USkeletalMeshComponent* Chassis = GetMesh();
		if (StunSparkTimer <= 0.f && DamageFx && Chassis && BodyBox.IsValid)
		{
			StunSparkTimer = 0.06f;
			const FVector Local(FMath::FRandRange(BodyBox.Min.X, BodyBox.Max.X), FMath::FRandRange(BodyBox.Min.Y, BodyBox.Max.Y), FMath::FRandRange(BodyBox.GetCenter().Z, BodyBox.Max.Z));
			FIronPuffStyle Spark;
			Spark.Life = 0.18f;
			Spark.StartScale = 0.14f;
			Spark.EndScale = 0.02f;
			Spark.Drift = FVector::ZeroVector;
			Spark.StartColor = FLinearColor(1.5f, 2.5f, 8.f);
			Spark.EndColor = FLinearColor(0.3f, 0.6f, 3.f);
			Spark.PeakOpacity = 1.f;
			Spark.bSwell = false;
			DamageFx->Emit(Chassis->GetComponentTransform().TransformPosition(Local), FVector::ZeroVector, Spark);
		}
	}
}

void AWarVehiclePawn::ApplyClassTraits()
{
	const IronVehicles::ClassStats& Stats = IronVehicles::Get(static_cast<IronVehicles::VehicleClass>(VehicleClass));
	ClassTopSpeedKph = Stats.TopSpeedKph;
	ClassGrip = Stats.Chassis.Grip;
	ClassAirControl = Stats.Chassis.AirControl;
	ClassEnginePitch = Stats.Chassis.EnginePitch;
	ClassBrake = Stats.Chassis.BrakeScale;
	ClassDownforce = Stats.Chassis.Downforce;
	ClassTurn = Stats.Chassis.Turn;
	ClassReverseKph = Stats.Chassis.ReverseKph;
	BoostTuning.RechargePerSecond *= Stats.Weapons.BoostRecharge;
	if (PrimaryWeapon)
	{
		PrimaryWeapon->SetClassTraits(Stats.Weapons.GunDamage, Stats.Weapons.GunRate, Stats.Weapons.GunHeat, 1.f);
	}
	if (SecondaryWeapon)
	{
		SecondaryWeapon->SetClassTraits(Stats.Weapons.RocketDamage, 1.f, 1.f, Stats.Weapons.RocketReload);
	}

	// Chassis: tall bodies roll over in hard turns unless the centre of mass comes down; fast ones
	// get pressed into the road. The mass itself is authored on the Blueprint (setup_garage_vehicles.py)
	// because the suspension is sized for it when the physics is created.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetCenterOfMass(FVector(0.f, 0.f, -Stats.Chassis.ComDropCm));
	}
	if (UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		// Wheelbase and steering lock, for the spin assist's model of how fast the car should turn.
		float FrontX = -1e6f, RearX = 1e6f;
		MaxSteerDeg = 0.f;
		for (const FChaosWheelSetup& Setup : Wheeled->WheelSetups)
		{
			const FVector At = GetMesh()->GetBoneLocation(Setup.BoneName, EBoneSpaces::ComponentSpace);
			FrontX = FMath::Max(FrontX, float(At.X));
			RearX = FMath::Min(RearX, float(At.X));
			if (const UChaosVehicleWheel* Wheel = Setup.WheelClass ? Setup.WheelClass->GetDefaultObject<UChaosVehicleWheel>() : nullptr)
			{
				MaxSteerDeg = FMath::Max(MaxSteerDeg, Wheel->MaxSteerAngle);
			}
		}
		WheelbaseCm = FrontX - RearX > 50.f ? FrontX - RearX : FMath::Max(float(BodyBox.GetSize().X) * 0.6f, 150.f);
		RearWheels.Reset();
		for (const FChaosWheelSetup& Setup : Wheeled->WheelSetups)
		{
			RearWheels.Add(GetMesh()->GetBoneLocation(Setup.BoneName, EBoneSpaces::ComponentSpace).X < 0.5f * (FrontX + RearX));
		}
		if (MaxSteerDeg <= 1.f)
		{
			MaxSteerDeg = 35.f;
		}
	}
	// AI drivers never get a style from the settings: give them the balanced one (with the class
	// grip); the player's controller applies the chosen style on possession.
	ApplyDrivingStyle(1);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: %s class %d - top %.0f kph, mass %.0f kg, grip %.2f, wheelbase %.0f cm, lock %.0f deg"),
		*GetName(), static_cast<int32>(VehicleClass), ClassTopSpeedKph, GetMesh() ? GetMesh()->GetMass() : 0.f, ClassGrip, WheelbaseCm, MaxSteerDeg);
}

int32 AWarVehiclePawn::GetWheelsOnGround() const
{
	const UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Wheeled)
	{
		return 0;
	}
	int32 OnGround = 0;
	for (int32 i = 0; i < Wheeled->WheelSetups.Num() && i < Wheeled->Wheels.Num(); ++i)
	{
		OnGround += Wheeled->GetWheelState(i).bInContact ? 1 : 0;
	}
	return OnGround;
}

void AWarVehiclePawn::TickAssists(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	UChaosVehicleMovementComponent* Movement = GetVehicleMovement();
	if (!Body || !Movement || DeltaSeconds <= 0.f || (Health && Health->IsDestroyed()))
	{
		bHasPreviousRotation = false;
		return;
	}
	// Angular velocity from how the body turned since last frame (world space, deg/s): the physics
	// body's own velocity does not read back reliably on the game thread (see TickSkid).
	const FQuat Now = GetActorQuat();
	FVector AngularDeg = FVector::ZeroVector;
	const bool bHadPrevious = bHasPreviousRotation;
	if (bHadPrevious)
	{
		FVector Axis;
		double Angle = 0.0;
		(Now * PreviousRotation.Inverse()).ToAxisAndAngle(Axis, Angle);
		if (Angle > UE_DOUBLE_PI)
		{
			Angle -= 2.0 * UE_DOUBLE_PI;
		}
		AngularDeg = Axis * FMath::RadiansToDegrees(Angle) / DeltaSeconds;
	}
	PreviousRotation = Now;
	bHasPreviousRotation = true;
	// Velocity from the position change, smoothed: Chaos does not report it reliably on the game thread.
	const FVector Pos = GetActorLocation();
	if (bHadPrevious)
	{
		SmoothedVelocity = FMath::Lerp(SmoothedVelocity, (Pos - AssistPrevPos) / DeltaSeconds, FMath::Min(1.f, DeltaSeconds * 20.f));
	}
	AssistPrevPos = Pos;
	if (!bHadPrevious)
	{
		return;
	}
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D(), Right = GetActorRightVector().GetSafeNormal2D();
	const float SignedKph = Movement->GetForwardSpeed() * 0.036f;
	const float Lateral = float(FVector::DotProduct(SmoothedVelocity, Right));
	const float Slip = IronHandling::SlipAngleDeg(float(FVector::DotProduct(SmoothedVelocity, Fwd)), Lateral);
	const bool bWasDrifting = Drift.bActive;
	// How fast the car is travelling, whichever way it points: sideways in the middle of a handbrake
	// turn its forward speed reads zero while it is still sliding at 40 km/h.
	const float TravelKph = float(SmoothedVelocity.Size2D()) * 0.036f * (SignedKph < 0.f ? -1.f : 1.f);
	if (IronHandling::UpdateDrift(Drift, bHandbrakeHeld, TravelKph, Slip, TargetSteer, DeltaSeconds, Arcade))
	{
		// The flick: throw the tail out toward the corner.
		// (Rolling backward the same steering swings the nose the other way.)
		const bool bNoseRight = (TargetSteer > 0.f) == (SignedKph >= 0.f);
		Body->AddAngularImpulseInDegrees(FVector(0.f, 0.f, bNoseRight ? Arcade.DriftKickDeg : -Arcade.DriftKickDeg), NAME_None, true);
	}
	if (Drift.bActive != bWasDrifting)
	{
		ApplyWheelSettings(); // Rear grip down for the slide, back when it ends.
	}

	const int32 OnGround = GetWheelsOnGround();
	AirborneSeconds = OnGround == 0 ? AirborneSeconds + DeltaSeconds : 0.f;
	if (AirborneSeconds > 0.15f)
	{
		// Mid-air: bring the wheels back under the car before it lands.
		const FVector Up = GetActorUpVector();
		const IronHandling::Vec3 A = IronHandling::AirLevel({ float(Up.X), float(Up.Y), float(Up.Z) }, { float(AngularDeg.X), float(AngularDeg.Y), float(AngularDeg.Z) }, Assist);
		if (A.X != 0.f || A.Y != 0.f || A.Z != 0.f)
		{
			Body->AddTorqueInDegrees(FVector(A.X, A.Y, A.Z), NAME_None, true);
		}
		return;
	}
	if (OnGround >= 1)
	{
		// Anti-roll (even on one wheel): the lean about the nose, taken the same way the air levelling
		// reads tilt (Up x sky is the axis that rights the car; its part along the nose is the roll).
		const FVector Up = GetActorUpVector(), Nose = GetActorForwardVector();
		const float Lean = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(FVector::DotProduct(FVector::CrossProduct(Up, FVector::UpVector), Nose)), -1.f, 1.f)));
		const float Roll = IronHandling::RollAssist(Lean, float(FVector::DotProduct(AngularDeg, Nose)), Assist);
		if (Roll != 0.f)
		{
			Body->AddTorqueInDegrees(Nose * Roll, NAME_None, true);
		}
	}
	if (OnGround >= 2)
	{
		float Yaw = 0.f;
		if (Drift.bActive)
		{
			// Drifting: the steering sets the rotation, the throttle keeps the speed up.
			Yaw = IronHandling::DriftYaw(float(AngularDeg.Z), Slip, TargetSteer, Drift.Direction, bHandbrakeHeld, TravelKph, Arcade);
			const float Thrust = Arcade.DriftThrust * FMath::Clamp(RequestedThrottle, 0.f, 1.f);
			if (Thrust > 0.f)
			{
				Body->AddForce(Fwd * Thrust, NAME_None, true);
			}
		}
		else
		{
			// Gripping: turn in as sharply as the class may corner; otherwise catch a spin the
			// steering did not ask for.
			// (Not on the handbrake: the wheels are locked, the steering is not driving the car.)
			const float Steer = Movement->GetSteeringInput();
			const float Target = IronHandling::TargetYawRate(SignedKph, Steer, WheelbaseCm, MaxSteerDeg, Arcade);
			Yaw = bHandbrakeHeld ? 0.f : IronHandling::TurnInAssist(float(AngularDeg.Z), Target, SignedKph, Arcade);
			if (Yaw == 0.f)
			{
				const float Expected = IronHandling::ExpectedYawRate(SignedKph, Steer, WheelbaseCm, MaxSteerDeg);
				Yaw = IronHandling::YawAssist(float(AngularDeg.Z), Expected, SignedKph, bHandbrakeHeld, Assist);
			}
		}
		if (Yaw != 0.f)
		{
			Body->AddTorqueInDegrees(FVector(0.f, 0.f, Yaw), NAME_None, true);
		}
		// Grip: the car goes where it points (a handbrake turn at a crawl is left to the tyres).
		const float Grip = bHandbrakeHeld && !Drift.bActive ? 0.f : IronHandling::GripAssist(Lateral, SignedKph, Drift.bActive, Arcade);
		if (Grip != 0.f)
		{
			Body->AddForce(Right * Grip, NAME_None, true);
		}
	}
}

FVector AWarVehiclePawn::GetBodyCenter() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	return Body && BodyBox.IsValid ? Body->GetComponentTransform().TransformPosition(BodyBox.GetCenter()) : GetActorLocation() + FVector(0.f, 0.f, 80.f);
}

FVector AWarVehiclePawn::GetFireDirection() const
{
	const FVector Level = GetAimDirection();
	const AWarVehiclePawn* Target = Cast<AWarVehiclePawn>(AimTarget.Get());
	const UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get();
	const IronAim::Tuning Tuning = IronAim::ForLevel(Settings ? Settings->AimAssist : 1);
	if (!Target || !Tuning.bEnabled || !IsPlayerControlled())
	{
		return Level;
	}
	const FVector From = GetAimOrigin(), To = Target->GetBodyCenter();
	const IronAim::Vec D = IronAim::AssistedDirection({ float(Level.X), float(Level.Y), float(Level.Z) },
		{ float(From.X), float(From.Y), float(From.Z) }, { float(To.X), float(To.Y), float(To.Z) }, Tuning);
	return FVector(D.X, D.Y, D.Z);
}

void AWarVehiclePawn::TickAimAssist(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get();
	const IronAim::Tuning Tuning = IronAim::ForLevel(Settings ? Settings->AimAssist : 1);
	if (!World || !IsPlayerControlled() || !Tuning.bEnabled || (Health && Health->IsDestroyed()))
	{
		AimTarget.Reset();
		return;
	}
	if (const AWarVehiclePawn* Current = Cast<AWarVehiclePawn>(AimTarget.Get()); Current && Current->GetHealthComponent() && Current->GetHealthComponent()->IsDestroyed())
	{
		AimTarget.Reset();
	}
	AimScanTimer -= DeltaSeconds;
	if (AimScanTimer > 0.f)
	{
		return;
	}
	AimScanTimer = 0.066f;
	TArray<AWarVehiclePawn*> Cars;
	TArray<IronAim::Vec> Points;
	int32 Previous = -1;
	for (TActorIterator<AWarVehiclePawn> It(World); It; ++It)
	{
		AWarVehiclePawn* Other = *It;
		if (Other == this || IronTeams::IsPlayerSide(Other) || !Other->GetHealthComponent() || Other->GetHealthComponent()->IsDestroyed())
		{
			continue;
		}
		if (Other == AimTarget.Get())
		{
			Previous = Cars.Num();
		}
		const FVector C = Other->GetBodyCenter();
		Cars.Add(Other);
		Points.Add({ float(C.X), float(C.Y), float(C.Z) });
	}
	const FVector From = GetAimOrigin(), Forward = GetAimDirection();
	const int32 Pick = IronAim::Pick({ float(From.X), float(From.Y), float(From.Z) }, { float(Forward.X), float(Forward.Y), float(Forward.Z) },
		Points.GetData(), Points.Num(), Previous, Tuning);
	AWarVehiclePawn* Chosen = Pick >= 0 ? Cars[Pick] : nullptr;
	if (Chosen)
	{
		// Only what the gun can actually see: not through a building or over a hill.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeAimAssist), false, this);
		Params.AddIgnoredActor(Chosen);
		if (World->LineTraceTestByChannel(From, Chosen->GetBodyCenter(), ECC_Visibility, Params))
		{
			Chosen = nullptr;
		}
	}
	AimTarget = Chosen;
}

void AWarVehiclePawn::TickTurret(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	const UWorld* World = GetWorld();
	if (!TurretHinge || !Body || !World || (Health && Health->IsDestroyed()))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// Where the gun is pointing: the player's (assisted) aim, the AI's last shot for a moment after
	// it fires, otherwise straight ahead.
	FVector Want = GetActorForwardVector();
	if (IsPlayerControlled())
	{
		Want = GetFireDirection();
	}
	else if (Now - AiAimTime < 1.5)
	{
		Want = AiAimDirection;
	}
	const FVector Local = Body->GetComponentTransform().InverseTransformVectorNoScale(Want);
	IronAim::TurnTurret({ float(Local.X), float(Local.Y), float(Local.Z) }, DeltaSeconds, IronAim::TurretLimits(), TurretYaw, TurretPitch);
	TurretHinge->SetRelativeRotation(FRotator(TurretPitch, TurretYaw, 0.f));
	if (BarrelHinge)
	{
		BarrelRpm = IronAim::SpinRpm(BarrelRpm, Now - LastGunShotTime < 0.15, DeltaSeconds);
		BarrelRoll = FMath::Fmod(BarrelRoll + BarrelRpm * 6.f * DeltaSeconds, 360.f);
		Recoil = FMath::Max(0.f, Recoil - DeltaSeconds * 14.f);
		BarrelHinge->SetRelativeLocationAndRotation(BarrelRest - FVector(3.5f * Recoil, 0.f, 0.f), FRotator(0.f, 0.f, BarrelRoll));
	}
}

// ---------------------------------------------------------------- Driver (CrewRules.h)

void AWarVehiclePawn::ApplyDriver(int32 InDriverIndex, int32 InRank)
{
	DriverIndex = InDriverIndex >= 0 && InDriverIndex < IronCrew::DriverCount ? InDriverIndex : -1;
	DriverRank = FMath::Clamp(InRank, 1, IronRanks::MaxRank);
	Ability = IronCrew::AbilityState();
	bAbilityWasActive = false;
	RefreshDriverTraits();
	if (DriverIndex >= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: %s driven by %s (rank %d)"), *GetName(), ANSI_TO_TCHAR(IronCrew::Get(static_cast<IronCrew::Driver>(DriverIndex)).Key), DriverRank);
	}
}

void AWarVehiclePawn::RefreshDriverTraits()
{
	// Class trait x driver perk x whatever the ability is doing right now, written as absolute values
	// (the setters replace, they do not stack).
	const IronVehicles::ClassStats& Stats = IronVehicles::Get(static_cast<IronVehicles::VehicleClass>(VehicleClass));
	const IronCrew::DriverDef* Def = DriverIndex >= 0 ? &IronCrew::Get(static_cast<IronCrew::Driver>(DriverIndex)) : nullptr;
	const IronCrew::Perk Perk = GetDriverPerk();
	const IronCrew::AbilityTuning Tuning;
	const bool bActive = Def && Ability.IsActive();
	const bool bDeadeye = bActive && Def->Active == IronCrew::Ability::Deadeye;
	const bool bBarrage = bActive && Def->Active == IronCrew::Ability::Barrage;
	const bool bWall = bActive && Def->Active == IronCrew::Ability::IronWall;
	const bool bOverdrive = bActive && Def->Active == IronCrew::Ability::Overdrive;
	if (PrimaryWeapon)
	{
		PrimaryWeapon->SetClassTraits(Stats.Weapons.GunDamage * (bDeadeye ? Tuning.DeadeyeDamage : 1.f), Stats.Weapons.GunRate,
			bDeadeye ? 0.f : Stats.Weapons.GunHeat * Perk.GunHeat, 1.f);
	}
	if (SecondaryWeapon)
	{
		SecondaryWeapon->SetClassTraits(Stats.Weapons.RocketDamage * (bBarrage ? Tuning.BarrageDamage : 1.f), bBarrage ? 1.f / Tuning.BarrageInterval : 1.f,
			1.f, Stats.Weapons.RocketReload * Perk.RocketReload);
	}
	BoostTuning.RechargePerSecond = IronBoost::Tuning().RechargePerSecond * Stats.Weapons.BoostRecharge * Perk.BoostRecharge;
	FlareTuning.RechargeSeconds = IronFlares::Tuning().RechargeSeconds * Perk.FlareRecharge;
	if (Health)
	{
		Health->SetArmorScale(UpgradeArmorScale * Perk.MaxArmor);
		Health->DamageTakenScale = bWall ? Tuning.IronWallDamageTaken : 1.f;
	}
	AbilityTorqueScale = bOverdrive ? Tuning.OverdriveTorque : 1.f;
	SetEngineTorque();
}

bool AWarVehiclePawn::UseAbility()
{
	if (DriverIndex < 0 || !Health || Health->IsDestroyed())
	{
		return false;
	}
	const IronCrew::DriverDef& Def = IronCrew::Get(static_cast<IronCrew::Driver>(DriverIndex));
	if (!Ability.Activate(Def, GetAbilityCooldownScale()))
	{
		return false;
	}
	const IronCrew::AbilityTuning Tuning;
	const TCHAR* SoundName = TEXT("S_Pickup");
	FLinearColor Burst(1.f, 0.8f, 0.3f);
	switch (Def.Active)
	{
	case IronCrew::Ability::Overdrive:
		Boost.Charge = 1.f;
		Boost.Cooldown = 0.f;
		SoundName = TEXT("S_Boost");
		Burst = FLinearColor(4.f, 1.6f, 0.3f);
		break;
	case IronCrew::Ability::Deadeye:
		if (PrimaryWeapon) PrimaryWeapon->ClearHeat();
		SoundName = TEXT("S_LockOn");
		Burst = FLinearColor(3.f, 3.f, 2.f);
		break;
	case IronCrew::Ability::IronWall:
		Health->Repair(0.f, Health->GetMaxArmor() * Tuning.IronWallArmorRestore);
		SoundName = TEXT("S_Impact");
		Burst = FLinearColor(0.6f, 1.2f, 3.f);
		break;
	case IronCrew::Ability::FieldRepair:
		Health->Repair(Health->GetMaxHealth() * Tuning.RepairHealth, Health->GetMaxArmor() * Tuning.RepairArmor);
		Burst = FLinearColor(0.4f, 3.f, 0.7f);
		break;
	case IronCrew::Ability::Barrage:
		if (SecondaryWeapon) SecondaryWeapon->RefillMagazine();
		SoundName = TEXT("S_Reload");
		Burst = FLinearColor(4.f, 0.6f, 0.2f);
		break;
	case IronCrew::Ability::Emp:
		EmpPulse();
		SoundName = TEXT("S_Tesla");
		Burst = FLinearColor(1.f, 2.f, 8.f);
		break;
	}
	RefreshDriverTraits();
	bAbilityWasActive = Ability.IsActive();
	RumbleController(0.5f, 0.25f);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/IronSiege/Audio/%s.%s"), SoundName, SoundName)))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
	// A ring of light thrown off the car, so the moment reads from the chase camera.
	if (DamageFx)
	{
		const bool bPulse = Def.Active == IronCrew::Ability::Emp;
		FIronPuffStyle Flash;
		Flash.Life = bPulse ? 0.45f : 0.35f;
		Flash.StartScale = 0.5f;
		Flash.EndScale = bPulse ? 1.6f : 0.9f;
		Flash.Drift = FVector::ZeroVector;
		Flash.StartColor = Burst;
		Flash.EndColor = Burst * 0.3f;
		Flash.PeakOpacity = 0.9f;
		Flash.bSwell = false;
		const FVector Centre = GetBodyCenter();
		const int32 Count = 14;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Angle = 2.f * PI * i / Count;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			DamageFx->Emit(Centre + Out * 160.f, Out * (bPulse ? 5500.f : 900.f), Flash);
		}
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->ShowNotice(IronText::Name(TEXT("Ability"), ANSI_TO_TCHAR(Def.AbilityName)), FLinearColor(1.f, 0.85f, 0.3f));
			HUD->Bark(static_cast<int32>(IronCrew::Bark::Ability));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: %s used %s"), ANSI_TO_TCHAR(Def.Key), ANSI_TO_TCHAR(Def.AbilityName));
	return true;
}

void AWarVehiclePawn::EmpPulse()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const IronCrew::AbilityTuning Tuning;
	int32 Stalled = 0, Fizzled = 0;
	for (TActorIterator<AWarVehiclePawn> It(World); It; ++It)
	{
		if (*It != this && IronTeams::IsPlayerSide(*It) != IronTeams::IsPlayerSide(this) && IronTeams::IsAlive(*It)
			&& FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= Tuning.EmpRadiusCm)
		{
			It->Stun(Tuning.EmpStunSeconds);
			++Stalled;
		}
	}
	for (TActorIterator<ARocketProjectile> It(World); It; ++It)
	{
		if (It->IsLive() && It->GetTarget() == this)
		{
			It->Fizzle();
			++Fizzled;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: EMP from %s - %d engine(s) stalled, %d missile(s) fizzled"), *GetName(), Stalled, Fizzled);
}

void AWarVehiclePawn::TickAbility(float DeltaSeconds)
{
	if (DriverIndex < 0)
	{
		return;
	}
	Ability.Tick(DeltaSeconds);
	const IronCrew::DriverDef& Def = IronCrew::Get(static_cast<IronCrew::Driver>(DriverIndex));
	const bool bActive = Ability.IsActive() && Health && !Health->IsDestroyed();
	if (bActive && Def.Active == IronCrew::Ability::Overdrive)
	{
		Boost.Charge = 1.f; // The nitro does not drain while it lasts.
		Boost.Cooldown = 0.f;
	}
	if (bAbilityWasActive && !Ability.IsActive())
	{
		RefreshDriverTraits();
	}
	bAbilityWasActive = Ability.IsActive();
	// While it runs, the car wears it: a shimmer of sparks over the body in the ability's colour.
	if (!bActive || !DamageFx || !BodyBox.IsValid || !GetMesh())
	{
		return;
	}
	AbilityPuffTimer -= DeltaSeconds;
	if (AbilityPuffTimer > 0.f)
	{
		return;
	}
	AbilityPuffTimer = 0.09f;
	FLinearColor Color(3.f, 3.f, 2.f);
	switch (Def.Active)
	{
	case IronCrew::Ability::Overdrive: Color = FLinearColor(4.f, 1.6f, 0.3f); break;
	case IronCrew::Ability::IronWall: Color = FLinearColor(0.6f, 1.2f, 3.f); break;
	case IronCrew::Ability::Barrage: Color = FLinearColor(4.f, 0.6f, 0.2f); break;
	default: break;
	}
	FIronPuffStyle Spark;
	Spark.Life = 0.3f;
	Spark.StartScale = 0.16f;
	Spark.EndScale = 0.03f;
	Spark.Drift = FVector(0.f, 0.f, 120.f);
	Spark.StartColor = Color;
	Spark.EndColor = Color * 0.4f;
	Spark.PeakOpacity = 0.9f;
	Spark.bSwell = false;
	const FVector Local(FMath::FRandRange(BodyBox.Min.X, BodyBox.Max.X), FMath::FRandRange(BodyBox.Min.Y, BodyBox.Max.Y), BodyBox.Max.Z);
	DamageFx->Emit(GetMesh()->GetComponentTransform().TransformPosition(Local), FVector::ZeroVector, Spark);
}
