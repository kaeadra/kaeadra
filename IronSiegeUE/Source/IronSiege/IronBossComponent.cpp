#include "IronBossComponent.h"
#include "EnergyWeapons.h"
#include "IronExplosion.h"
#include "IronPuffEmitter.h"
#include "IronSiegeAIController.h"
#include "IronSiegeGameMode.h"
#include "IronSiegeHUD.h"
#include "IronSiegeText.h"
#include "IronSiegeUserSettings.h"
#include "IronTeams.h"
#include "MineLayerComponent.h"
#include "RocketLauncherComponent.h"
#include "RocketProjectile.h"
#include "SettingsRules.h"
#include "VehicleHealthComponent.h"
#include "VehicleWeaponComponent.h"
#include "WarVehiclePawn.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace
{
const FLinearColor SlamColor(4.f, 1.4f, 0.2f);
const FLinearColor EmpColor(1.f, 2.f, 8.f);
const FLinearColor ShieldColor(0.6f, 1.4f, 4.f);
}

UIronBossComponent::UIronBossComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

AWarVehiclePawn* UIronBossComponent::GetCar() const
{
	return Cast<AWarVehiclePawn>(GetOwner());
}

void UIronBossComponent::Setup(IronBoss::Kind InKind)
{
	Kind = InKind;
	Phases = IronBoss::PhaseTracker();
	AWarVehiclePawn* Car = GetCar();
	if (!Car)
	{
		return;
	}
	// The boss's own glow: the warning ring of a slam or EMP, the shield, the blast rings.
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft"));
	if (!Glow)
	{
		Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_SmokeSoft.MI_FX_SmokeSoft"));
	}
	Fx = NewObject<UIronPuffEmitter>(Car, TEXT("BossFx"));
	Fx->SetupAttachment(Car->GetRootComponent());
	Fx->RegisterComponent();
	Fx->Init(Glow, 96);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: %s fights as boss kind %d"), *Car->GetName(), static_cast<int32>(Kind));
}

float UIronBossComponent::HealthFraction() const
{
	const AWarVehiclePawn* Car = GetCar();
	const UVehicleHealthComponent* Health = Car ? Car->GetHealthComponent() : nullptr;
	if (!Health)
	{
		return 1.f;
	}
	// Health and armour together, as the boss bar shows it.
	const float Total = Health->GetMaxHealth() + Health->GetMaxArmor();
	return Total > 0.f ? FMath::Clamp((Health->GetHealth() + Health->GetArmor()) / Total, 0.f, 1.f) : 0.f;
}

void UIronBossComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWarVehiclePawn* Car = GetCar();
	UVehicleHealthComponent* Health = Car ? Car->GetHealthComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Car || !Health || !World)
	{
		return;
	}
	if (Health->IsDestroyed())
	{
		// A wreck winds nothing up (the HUD warning goes out with it) and drops its shield.
		Slam = IronBoss::WindUp();
		Emp = IronBoss::WindUp();
		FollowUpLeft = -1.f;
		Health->DamageTakenScale = 1.f;
		return;
	}

	// The AI driver's pace, once it has one (the difficulty is applied right after the spawn).
	if (!bHaveBase)
	{
		if (const AIronSiegeAIController* AI = Cast<AIronSiegeAIController>(Car->GetController()))
		{
			BaseBurstSeconds = AI->BurstSeconds;
			BaseBurstPause = AI->BurstPauseSeconds;
			BaseMissileInterval = AI->MissileInterval;
			BaseAimSpread = AI->AimSpreadDeg;
			bHaveBase = true;
			ApplyPhaseTuning();
		}
	}

	const int32 From = Phases.Phase;
	if (const int32 Entered = Phases.Update(HealthFraction(), Kind); Entered > 0)
	{
		EnterPhase(From, Entered);
	}
	Phases.Tick(DeltaTime);
	Health->DamageTakenScale = Phases.IsShielded() ? Attacks.ShieldDamageTaken : 1.f;
	if (Phases.IsShielded() && Fx)
	{
		// Shielded: blue sparks crawl over the hull, so the player sees why the hits do little.
		ShieldSparkTimer -= DeltaTime;
		if (ShieldSparkTimer <= 0.f)
		{
			ShieldSparkTimer = 0.05f;
			FIronPuffStyle Spark;
			Spark.Life = 0.25f;
			Spark.StartScale = 0.2f;
			Spark.EndScale = 0.05f;
			Spark.Drift = FVector(0.f, 0.f, 80.f);
			Spark.StartColor = ShieldColor;
			Spark.EndColor = ShieldColor * 0.3f;
			Spark.PeakOpacity = 0.9f;
			Spark.bSwell = false;
			const FVector Offset(FMath::FRandRange(-220.f, 220.f), FMath::FRandRange(-110.f, 110.f), FMath::FRandRange(-40.f, 90.f));
			Fx->Emit(Car->GetBodyCenter() + Car->GetActorRotation().RotateVector(Offset), FVector::ZeroVector, Spark);
		}
	}

	// The player's car is the target; nothing to aim at while it is down (or in photo mode).
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	AActor* Target = IronTeams::IsAlive(Player) ? Player : nullptr;
	const float Distance = Target ? float(FVector::Dist2D(Car->GetActorLocation(), Target->GetActorLocation())) : TNumericLimits<float>::Max();
	const IronBoss::PhaseTuning& Tuning = IronBoss::Tuning(Kind, Phases.Phase);

	RingTimer -= DeltaTime;
	const bool bSlamWasWinding = Slam.IsWinding();
	if (Slam.Tick(DeltaTime, Target && Distance <= Attacks.SlamTriggerCm, Tuning.SlamInterval, Attacks.SlamWindUp))
	{
		StrikeSlam();
	}
	else if (Slam.IsWinding())
	{
		if (!bSlamWasWinding)
		{
			WarnPlayer(Target, Attacks.SlamRadiusCm);
		}
		// The danger zone, flickering on the ground at the blast's edge until it lands.
		if (RingTimer <= 0.f)
		{
			RingTimer = 0.15f;
			EmitRing(Car->GetActorLocation(), Attacks.SlamRadiusCm, 0.f, SlamColor * (0.4f + 0.6f * Slam.Progress(Attacks.SlamWindUp)), 28);
		}
	}

	const bool bEmpWasWinding = Emp.IsWinding();
	if (Emp.Tick(DeltaTime, Target && Distance <= Attacks.EmpRadiusCm * 0.8f, Tuning.EmpInterval, Attacks.EmpWindUp))
	{
		StrikeEmp(Target);
	}
	else if (Emp.IsWinding())
	{
		if (!bEmpWasWinding)
		{
			WarnPlayer(Target, Attacks.EmpRadiusCm);
		}
		if (RingTimer <= 0.f)
		{
			RingTimer = 0.15f;
			EmitRing(Car->GetBodyCenter(), Attacks.EmpRadiusCm * Emp.Progress(Attacks.EmpWindUp), 0.f, EmpColor, 28);
		}
	}

	// Barrages wait for a moment the boss is not already winding something up.
	if (Barrage.Tick(DeltaTime, Tuning.BarrageInterval, Target && Distance <= 7000.f && !Slam.IsWinding() && !Emp.IsWinding()))
	{
		FireBarrage(Target, Tuning.BarrageRockets);
	}
	if (Kind == IronBoss::Kind::Raven)
	{
		TickRaven(DeltaTime, Target, Distance);
	}
}

void UIronBossComponent::EnterPhase(int32 From, int32 To)
{
	AWarVehiclePawn* Car = GetCar();
	UWorld* World = GetWorld();
	if (!Car || !World)
	{
		return;
	}
	const IronBoss::PhaseTuning& Tuning = IronBoss::Tuning(Kind, To);
	if (UVehicleHealthComponent* Health = Car->GetHealthComponent())
	{
		Health->Repair(0.f, Health->GetMaxArmor() * Tuning.ArmorRestore);
	}
	ApplyPhaseTuning();
	if (Kind == IronBoss::Kind::Raven)
	{
		// Her duelling kit comes out: the rail cycles fast enough for a second shot, and mines.
		if (URailgunComponent* Rail = Car->GetRailgun())
		{
			Rail->SetClassTraits(1.f, 2.5f, 1.f, 1.f);
		}
		if (UMineLayerComponent* Layer = Car->GetMineWeaponComponent(); Layer && !Layer->bUnlocked)
		{
			Layer->bUnlocked = true;
			Layer->AddReserveAmmo(9);
		}
	}

	// Help arrives round the boss, facing the player.
	AIronSiegeGameMode* GameMode = World->GetAuthGameMode<AIronSiegeGameMode>();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const int32 Escorts = IronBoss::EscortsEntering(Kind, From, To);
	if (GameMode && Escorts > 0)
	{
		const FVector Here = Car->GetActorLocation();
		const FVector FaceTo = Player ? Player->GetActorLocation() : Here;
		const TArray<FVector> Points = GameMode->PickSpawnPoints(Here, 1200.f, 3200.f, Escorts);
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			GameMode->SpawnEnemyOfKind(IronBoss::EscortKind(Kind, i), Points[i], (FaceTo - Points[i]).GetSafeNormal2D().Rotation());
		}
	}

	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (AIronSiegeHUD* HUD = PC ? PC->GetHUD<AIronSiegeHUD>() : nullptr)
	{
		const IronBoss::PhaseLine& Line = IronBoss::LineFor(Kind, To);
		HUD->Say(Line.Speaker, Line.Mood, Line.Key, Line.Text, true);
		const FString Name = GameMode ? GameMode->GetBossName() : FString();
		HUD->ShowNotice(FString::Format(*IronText::Str(TEXT("NoticeBossPhase"), TEXT("{0}: PHASE {1}")), { Name, To }), FLinearColor(1.f, 0.3f, 0.2f));
	}
	EmitRing(Car->GetBodyCenter(), 150.f, 1800.f, ShieldColor, 20);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: boss %s enters phase %d (from %d) - %d escort(s), shield %.1f s"), *Car->GetName(), To, From, Escorts, Tuning.ShieldSeconds);
}

void UIronBossComponent::ApplyPhaseTuning()
{
	AWarVehiclePawn* Car = GetCar();
	AIronSiegeAIController* AI = Car ? Cast<AIronSiegeAIController>(Car->GetController()) : nullptr;
	if (!AI || !bHaveBase)
	{
		return;
	}
	// Fiercer, not faster to die: longer bursts, shorter pauses, tighter aim.
	const IronBoss::PhaseTuning& Tuning = IronBoss::Tuning(Kind, Phases.Phase);
	AI->BurstSeconds = BaseBurstSeconds * Tuning.Aggression;
	AI->BurstPauseSeconds = BaseBurstPause / Tuning.Aggression;
	AI->MissileInterval = BaseMissileInterval / Tuning.Aggression;
	AI->AimSpreadDeg = BaseAimSpread * Tuning.AimSpread;
}

void UIronBossComponent::StrikeSlam()
{
	AWarVehiclePawn* Car = GetCar();
	UWorld* World = GetWorld();
	if (!Car || !World)
	{
		return;
	}
	// The truck slams the ground: a blast round it that hurts and throws whatever is close. It lands
	// where the boss is, so the wind-up is the player's cue to drive clear.
	const FVector At = Car->GetActorLocation();
	const UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get();
	const float Damage = Attacks.SlamDamage * IronSettings::EnemyDamageMultiplier(Settings ? Settings->Difficulty : 1);
	TArray<AActor*> Ignored;
	Ignored.Add(Car);
	UGameplayStatics::ApplyRadialDamage(World, Damage, At + FVector(0.f, 0.f, 80.f), Attacks.SlamRadiusCm, UDamageType::StaticClass(), Ignored, Car, Car->GetController(), false);
	AIronExplosion::Spawn(World, At + FVector(0.f, 0.f, 40.f), 1.8f, Attacks.SlamRadiusCm, Attacks.SlamImpulse, Car);
	EmitRing(At, 200.f, 3200.f, SlamColor, 28);
	if (AWarVehiclePawn* PlayerCar = Cast<AWarVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0)); PlayerCar && FVector::Dist2D(PlayerCar->GetActorLocation(), At) <= Attacks.SlamRadiusCm)
	{
		PlayerCar->RumbleController(0.9f, 0.45f);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: boss %s slams (%.0f damage, %.0f cm)"), *Car->GetName(), Damage, Attacks.SlamRadiusCm);
}

void UIronBossComponent::StrikeEmp(AActor* Target)
{
	AWarVehiclePawn* Car = GetCar();
	if (!Car)
	{
		return;
	}
	EmitRing(Car->GetBodyCenter(), 150.f, 4200.f, EmpColor, 28);
	if (USoundBase* Sound = UVehicleWeaponComponent::LoadIfPresent(EmpSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Car->GetActorLocation());
	}
	AWarVehiclePawn* PlayerCar = Cast<AWarVehiclePawn>(Target);
	const bool bCaught = PlayerCar && FVector::Dist2D(PlayerCar->GetActorLocation(), Car->GetActorLocation()) <= Attacks.EmpRadiusCm;
	if (bCaught)
	{
		PlayerCar->Stun(Attacks.EmpStunSeconds);
		PlayerCar->RumbleController(0.6f, 0.3f);
		const APlayerController* PC = Cast<APlayerController>(PlayerCar->GetController());
		if (AIronSiegeHUD* HUD = PC ? PC->GetHUD<AIronSiegeHUD>() : nullptr)
		{
			HUD->ShowNotice(IronText::Str(TEXT("NoticeEmpHit"), TEXT("ENGINE STALLED BY EMP")), FLinearColor(0.5f, 0.75f, 1.f));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: boss %s EMP - player %s"), *Car->GetName(), bCaught ? TEXT("stalled") : TEXT("clear"));
}

void UIronBossComponent::FireBarrage(const AActor* Target, int32 Rockets)
{
	AWarVehiclePawn* Car = GetCar();
	UWorld* World = GetWorld();
	URocketLauncherComponent* Launcher = Car ? Car->GetSecondaryWeaponComponent() : nullptr;
	if (!Car || !World || !Launcher || !Target || Rockets <= 0)
	{
		return;
	}
	// A fan of unguided rockets at the player, a little softer each than a single aimed one: the
	// gaps in the fan are the way through.
	const IronWeapons::Spec& Spec = Launcher->WeaponSpec();
	const float Damage = Launcher->ShotDamage(Spec) * Attacks.BarrageDamage;
	const FVector From = Car->GetBodyCenter() + FVector(0.f, 0.f, 150.f);
	const FVector Aim = (Target->GetActorLocation() + FVector(0.f, 0.f, 60.f) - From).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Aim).GetSafeNormal();
	for (int32 i = 0; i < Rockets; ++i)
	{
		const FVector Direction = Aim.RotateAngleAxis(IronBoss::FanYaw(i, Rockets, Attacks.BarrageSpreadDeg), FVector::UpVector);
		// Side by side across the roof: rockets launched from one point would hit each other.
		const FVector Start = From + Side * (70.f * (i - 0.5f * (Rockets - 1))) + Direction * 120.f;
		FActorSpawnParameters Params;
		Params.Owner = Car;
		Params.Instigator = Car->GetInstigator();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ARocketProjectile* Rocket = World->SpawnActor<ARocketProjectile>(ARocketProjectile::StaticClass(), FTransform(Direction.Rotation(), Start), Params))
		{
			Rocket->Launch(Direction, Damage, Spec.ExplosionRadius, Car, Car->GetController(), nullptr);
		}
	}
	if (USoundBase* Sound = UVehicleWeaponComponent::LoadIfPresent(BarrageSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, From, 1.2f, 0.9f);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: boss %s fires a barrage of %d"), *Car->GetName(), Rockets);
}

void UIronBossComponent::TickRaven(float DeltaSeconds, AActor* Target, float Distance)
{
	AWarVehiclePawn* Car = GetCar();
	if (!Car)
	{
		return;
	}
	const IronBoss::PhaseTuning& Tuning = IronBoss::Tuning(Kind, Phases.Phase);

	// One rail for you, one for your shadow: when her own shot leaves, a second charge follows at
	// wherever the player has swerved to - only one, never a chain.
	const bool bCharging = Car->IsRailgunCharging();
	if (bWasRailCharging && !bCharging)
	{
		if (bFollowUpCharging)
		{
			bFollowUpCharging = false;
		}
		else if (Tuning.RailFollowUp > 0.f)
		{
			FollowUpLeft = Tuning.RailFollowUp;
		}
	}
	bWasRailCharging = bCharging;
	if (FollowUpLeft >= 0.f)
	{
		FollowUpLeft -= DeltaSeconds;
		URailgunComponent* Rail = Car->GetRailgun();
		if (FollowUpLeft < 0.f && Target && Rail && !bCharging)
		{
			if (Rail->GetAmmo() <= 0)
			{
				Rail->StartReload();
			}
			else
			{
				Car->FireRailgunAt(Target->GetActorLocation() + FVector(0.f, 0.f, 80.f));
				bFollowUpCharging = Car->IsRailgunCharging();
			}
		}
	}

	// Mines in her wake when the player sits on her tail.
	const FVector ToTarget = Target ? (Target->GetActorLocation() - Car->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	const bool bOnHerTail = Target && Distance <= Attacks.MineTriggerCm && FVector::DotProduct(Car->GetActorForwardVector().GetSafeNormal2D(), ToTarget) < -0.3f;
	if (Mines.Tick(DeltaSeconds, Tuning.MineInterval, bOnHerTail))
	{
		if (UMineLayerComponent* Layer = Car->GetMineWeaponComponent())
		{
			if (Layer->GetAmmo() <= 0)
			{
				Layer->StartReload();
			}
			Car->DeployMine();
		}
	}
}

void UIronBossComponent::EmitRing(const FVector& Centre, float Radius, float Speed, const FLinearColor& Color, int32 Count)
{
	if (!Fx || Count <= 0)
	{
		return;
	}
	FIronPuffStyle Ring;
	Ring.Life = Speed > 0.f ? 0.45f : 0.2f;
	Ring.StartScale = 0.6f;
	Ring.EndScale = Speed > 0.f ? 1.4f : 0.7f;
	Ring.Drift = FVector::ZeroVector;
	Ring.StartColor = Color;
	Ring.EndColor = Color * 0.3f;
	Ring.PeakOpacity = 0.9f;
	Ring.bSwell = false;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = 2.f * UE_PI * i / Count;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		Fx->Emit(Centre + Out * Radius + FVector(0.f, 0.f, 30.f), Out * Speed, Ring);
	}
}

void UIronBossComponent::WarnPlayer(AActor* Target, float Range) const
{
	const AWarVehiclePawn* Car = GetCar();
	if (!Car || !Target || FVector::Dist2D(Car->GetActorLocation(), Target->GetActorLocation()) > Range * 1.1f)
	{
		return;
	}
	if (USoundBase* Beep = UVehicleWeaponComponent::LoadIfPresent(WarnSound))
	{
		UGameplayStatics::PlaySound2D(this, Beep, 0.7f);
	}
}
