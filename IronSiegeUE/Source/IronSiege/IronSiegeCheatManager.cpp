#include "IronSiegeCheatManager.h"
#include "IronMissionDirector.h"
#include "IronSiegeHUD.h"
#include "IronSiegeStory.h"
#include "MissionRules.h"
#include "AimRules.h"
#include "IronSiegeAIController.h"
#include "EnergyWeapons.h"
#include "SIronSettingsMenu.h"
#include "IronSiegeUserSettings.h"
#include "SettingsRules.h"
#include "WarVehiclePawn.h"
#include "IronSiegeGameMode.h"
#include "MineLayerComponent.h"
#include "FlamethrowerComponent.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "IronSiegePlayerController.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "IronSupplyCrate.h"
#include "IronSiegeGameMode.h"
#include "EngineUtils.h"

AIronSiegePlayerController* UIronSiegeCheatManager::GetIronController() const
{
	return Cast<AIronSiegePlayerController>(GetOuterAPlayerController());
}

IIronVehicle* UIronSiegeCheatManager::GetVehicle() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return PC ? Cast<IIronVehicle>(PC->GetPawn()) : nullptr;
}

void UIronSiegeCheatManager::DebugSelectVehicle(int32 ClassIndex)
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->DeployVehicle(ClassIndex);
	}
}

void UIronSiegeCheatManager::DebugKillSelf()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (APawn* MyPawn = PC ? PC->GetPawn() : nullptr)
	{
		UGameplayStatics::ApplyDamage(MyPawn, 100000.f, PC, nullptr, UDamageType::StaticClass());
	}
}

void UIronSiegeCheatManager::DebugGod()
{
	if (IIronVehicle* Vehicle = GetVehicle())
	{
		if (UVehicleHealthComponent* Health = Vehicle->GetHealthComponent())
		{
			Health->bInvulnerable = true;
		}
	}
}

void UIronSiegeCheatManager::DebugLockInput()
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		PC->DisableInput(PC);
	}
}

void UIronSiegeCheatManager::DebugClearEnemies()
{
	const APlayerController* PC = GetOuterAPlayerController();
	TArray<AActor*> Pawns;
	UGameplayStatics::GetAllActorsOfClass(this, APawn::StaticClass(), Pawns);
	for (AActor* A : Pawns)
	{
		if (!PC || A != PC->GetPawn())
		{
			A->Destroy();
		}
	}
}

void UIronSiegeCheatManager::DebugDrive(float Throttle, float SteerValue, float Seconds)
{
	DriveThrottle = Throttle;
	DriveSteer = SteerValue;
	DriveRemaining = Seconds;
}

void UIronSiegeCheatManager::DebugLogPos()
{
	const APlayerController* PC = GetOuterAPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}
	const IIronVehicle* Vehicle = Cast<IIronVehicle>(MyPawn);
	FString Drive;
	if (const AWheeledVehiclePawn* Wheeled = Cast<AWheeledVehiclePawn>(MyPawn))
	{
		if (UChaosWheeledVehicleMovementComponent* M = Cast<UChaosWheeledVehicleMovementComponent>(Wheeled->GetVehicleMovementComponent()))
		{
			Drive = FString::Printf(TEXT(" gear %d rpm %.0f throttle %.2f brake %.2f handbrake %d"), M->GetCurrentGear(), M->GetEngineRotationSpeed(), M->GetThrottleInput(), M->GetBrakeInput(), M->GetHandbrakeInput() ? 1 : 0);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: player at %s rot %s speed %.1f km/h%s"), *MyPawn->GetActorLocation().ToString(), *MyPawn->GetActorRotation().ToString(), Vehicle ? Vehicle->GetSpeedKph() : 0.f, *Drive);
	TArray<AActor*> Others;
	UGameplayStatics::GetAllActorsOfClass(this, APawn::StaticClass(), Others);
	for (AActor* A : Others)
	{
		if (A != MyPawn)
		{
			const IIronVehicle* Other = Cast<IIronVehicle>(A);
			UE_LOG(LogTemp, Log, TEXT("IronSiege:   other %s at %s speed %.1f km/h"), *A->GetName(), *A->GetActorLocation().ToString(), Other ? Other->GetSpeedKph() : 0.f);
		}
	}
}

void UIronSiegeCheatManager::DebugDumpPawn()
{
	const APlayerController* PC = GetOuterAPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}
	TArray<UPrimitiveComponent*> Prims;
	MyPawn->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* P : Prims)
	{
		const USceneComponent* Parent = P->GetAttachParent();
		UE_LOG(LogTemp, Log, TEXT("IronSiege: part %s (%s) collision=%d profile=%s simulate=%d parent=%s socket=%s loc=%s"),
			*P->GetName(), *P->GetClass()->GetName(), (int32)P->GetCollisionEnabled(), *P->GetCollisionProfileName().ToString(),
			P->IsSimulatingPhysics() ? 1 : 0, Parent ? *Parent->GetName() : TEXT("-"), *P->GetAttachSocketName().ToString(),
			*P->GetComponentLocation().ToString());
	}
}

void UIronSiegeCheatManager::DebugSpawnClass(const FString& ClassPath)
{
	UClass* Cls = LoadClass<AActor>(nullptr, *ClassPath);
	const APlayerController* PC = GetOuterAPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Cls || !MyPawn || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("IronSiege: DebugSpawnClass could not load %s"), *ClassPath);
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector At = MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * 1500.f + FVector(0.f, 0.f, 60.f);
	AActor* Spawned = World->SpawnActor<AActor>(Cls, At, MyPawn->GetActorRotation(), Params);
	if (!Spawned)
	{
		return;
	}
	TWeakObjectPtr<AActor> Weak = Spawned;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Weak, At]()
	{
		if (Weak.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("IronSiege: spawned %s from %s is now at %s"), *Weak->GetName(), *At.ToString(), *Weak->GetActorLocation().ToString());
		}
	}), 4.f, false);
}

void UIronSiegeCheatManager::DebugDelayedCommand(float Seconds, const FString& Command)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// '+' stands in for spaces so the whole command survives as one -ExecCmds token.
	const FString Resolved = Command.Replace(TEXT("+"), TEXT(" "));
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Resolved]()
	{
		if (APlayerController* PC = GetOuterAPlayerController())
		{
			PC->ConsoleCommand(Resolved);
		}
	}), FMath::Max(Seconds, 0.01f), false);
}

void UIronSiegeCheatManager::DebugViewFrom(float X, float Y, float Z, float Pitch, float Yaw)
{
	APlayerController* PC = GetOuterAPlayerController();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(FVector(X, Y, Z), FRotator(Pitch, Yaw, 0.f)))
	{
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		PC->SetViewTarget(Cam);
		PC->bAutoManageActiveCameraTarget = false;
	}
}

void UIronSiegeCheatManager::DebugViewCar(float Distance, float AngleDeg, float Height, float TargetUp)
{
	const APlayerController* PC = GetOuterAPlayerController();
	const APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}
	const FVector Target = MyPawn->GetActorLocation() + FVector(0.f, 0.f, 40.f + TargetUp);
	const FRotator Facing(0.f, MyPawn->GetActorRotation().Yaw + AngleDeg, 0.f);
	const FVector From = Target + Facing.Vector() * Distance + FVector(0.f, 0.f, Height);
	const FRotator Look = (Target - From).Rotation();
	DebugViewFrom(From.X, From.Y, From.Z, Look.Pitch, Look.Yaw);
}

void UIronSiegeCheatManager::DebugGoToSupply()
{
	const APlayerController* PC = GetOuterAPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}
	AIronSupplyCrate* Nearest = nullptr;
	for (TActorIterator<AIronSupplyCrate> It(GetWorld()); It; ++It)
	{
		// Skip a crate we're already parked on (e.g. a repair kit refused at full health).
		if (FVector::Dist(It->GetActorLocation(), MyPawn->GetActorLocation()) <= It->PickupRadius)
		{
			continue;
		}
		if (!Nearest || FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), MyPawn->GetActorLocation()))
		{
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: moving to %s supply at %s"), *Nearest->GetLabel(), *Nearest->GetActorLocation().ToString());
		MyPawn->SetActorLocation(Nearest->GetActorLocation() + FVector(0.f, 0.f, 20.f), false, nullptr, ETeleportType::ResetPhysics);
	}
}

void UIronSiegeCheatManager::DebugFire(int32 Weapon, float Seconds)
{
	FireWeapon = Weapon;
	FireRemaining = Seconds;
}

void UIronSiegeCheatManager::DebugKillNearestEnemy()
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}
	APawn* Nearest = nullptr;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(*It);
		const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (*It == MyPawn || !Health || Health->IsDestroyed())
		{
			continue;
		}
		if (!Nearest || FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), MyPawn->GetActorLocation()))
		{
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: killing %s at %s"), *Nearest->GetName(), *Nearest->GetActorLocation().ToString());
		UGameplayStatics::ApplyDamage(Nearest, 100000.f, PC, MyPawn, UDamageType::StaticClass());
	}
}

static AIronSiegeGameMode* IronGameMode(const UObject* Context)
{
	return Context && Context->GetWorld() ? Context->GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
}

void UIronSiegeCheatManager::DebugAddCredits(int32 Amount)
{
	if (AIronSiegeGameMode* GM = IronGameMode(this)) GM->DebugAddCredits(Amount);
}

void UIronSiegeCheatManager::DebugBuy(int32 Index)
{
	if (AIronSiegeGameMode* GM = IronGameMode(this))
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: buy %d -> %s, credits left %d"), Index, GM->BuyUpgrade(Index) ? TEXT("ok") : TEXT("refused"), GM->GetUpgrades().Credits);
	}
}

void UIronSiegeCheatManager::DebugCloseShop()
{
	if (AIronSiegeGameMode* GM = IronGameMode(this)) GM->CloseShop();
}

void UIronSiegeCheatManager::DebugSetNextWave(int32 N)
{
	if (AIronSiegeGameMode* GM = IronGameMode(this)) GM->DebugSetNextWave(N);
}

void UIronSiegeCheatManager::DebugMeasureFps(float Seconds)
{
	FpsRemaining = FMath::Max(Seconds, 0.1f);
	FpsTotal = 0.f;
	FpsWorst = 0.f;
	FpsFrames = 0;
}

void UIronSiegeCheatManager::DebugSettingsTour(float Interval)
{
	AIronSiegePlayerController* PC = GetIronController();
	if (!PC)
	{
		return;
	}
	PC->OpenSettingsMenu();
	TWeakObjectPtr<AIronSiegePlayerController> WeakPC = PC;
	int32 Step = -2; // The first screenshot after opening lands a tab late: take a throwaway one.
	float Elapsed = 0.f;
	const float Wait = FMath::Max(Interval, 0.5f);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, Step, Elapsed, Wait](float Dt) mutable
	{
		AIronSiegePlayerController* Owner = WeakPC.Get();
		TSharedPtr<SIronSettingsMenu> Menu = Owner ? Owner->GetSettingsMenu() : nullptr;
		if (!Menu.IsValid())
		{
			return false;
		}
		Elapsed += Dt;
		if (Elapsed < Wait)
		{
			return true;
		}
		Elapsed = 0.f;
		// Four steps per tab: switch to it, shoot the top, scroll to the bottom, shoot that. (The
		// throwaway steps before 0 just take the lagging first screenshot.)
		if (Step >= 4 * SIronSettingsMenu::NumTabs())
		{
			Owner->CloseSettingsMenu();
			return false;
		}
		const int32 Phase = Step < 0 ? 1 : Step % 4;
		if (Phase == 0)
		{
			Menu->ShowTab(Step / 4);
		}
		else if (Phase == 2)
		{
			Menu->ScrollToEnd();
		}
		else
		{
			Owner->ConsoleCommand(TEXT("Shot showui"));
		}
		++Step;
		return true;
	}));
}

void UIronSiegeCheatManager::DebugDelayedCommandRealtime(float Seconds, const FString& Command)
{
	const FString Resolved = Command.Replace(TEXT("+"), TEXT(" "));
	float Remaining = FMath::Max(Seconds, 0.f);
	// Run by whoever is the player controller when the time comes, not by this cheat manager: a
	// level change in between (the menus travelling to a mission's map) replaces both.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Resolved, Remaining](float Dt) mutable
	{
		Remaining -= Dt;
		if (Remaining > 0.f)
		{
			return true;
		}
		UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			PC->ConsoleCommand(Resolved);
		}
		return false;
	}));
}

void UIronSiegeCheatManager::DebugPhotoMode()
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->TogglePhotoMode();
	}
}

void UIronSiegeCheatManager::DebugBenchmark(float Seconds, float TargetFps)
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->RunBenchmark(Seconds > 0.f ? Seconds : 8.f, TargetFps > 0.f ? TargetFps : 60.f);
	}
}

void UIronSiegeCheatManager::DebugSetHealth(float Fraction)
{
	if (IIronVehicle* Vehicle = GetVehicle())
	{
		if (UVehicleHealthComponent* Health = Vehicle->GetHealthComponent())
		{
			Health->SetHealthFractionForTest(Fraction);
			UE_LOG(LogTemp, Log, TEXT("IronSiege: health set to %.0f / %.0f"), Health->GetHealth(), Health->GetMaxHealth());
		}
	}
}

void UIronSiegeCheatManager::DebugShowroom(int32 Stage)
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	const AIronSiegeGameMode* GameMode = World ? World->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	if (!Me || !GameMode)
	{
		return;
	}
	DebugClearEnemies();

	struct FEntry { TSubclassOf<APawn> Class; int32 Variant; EIronKit KitOverride = EIronKit::None; };
	TArray<FEntry> Lineup;
	if (Stage == 0)
	{
		for (const TSubclassOf<APawn>& Class : GameMode->VehicleClasses)
		{
			Lineup.Add({ Class, 1 });
		}
	}
	else if (Stage == 2)
	{
		Lineup.Add({ GameMode->HunterVehicleClass ? GameMode->HunterVehicleClass : GameMode->EnemyVehicleClass, 1, EIronKit::Hunter }); // Just the missile hunter.
	}
	else
	{
		for (int32 v = 1; v <= 3; ++v) Lineup.Add({ GameMode->EnemyVehicleClass, v });
		for (int32 v = 1; v <= 3; ++v) Lineup.Add({ GameMode->FastEnemyVehicleClass, v });
		Lineup.Add({ GameMode->BossEnemyVehicleClass, 1 });
		Lineup.Add({ GameMode->HunterVehicleClass ? GameMode->HunterVehicleClass : GameMode->EnemyVehicleClass, 1, EIronKit::Hunter });
	}

	// Parked down the street ahead in two staggered lanes, all facing the same way, then a camera
	// visits each one for a three-quarter portrait (core ticker, so it works whatever the game is doing).
	// Toward the middle of the map: the player starts sit near the corners, close to the walls.
	const FVector ToCentre = (-Me->GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = ToCentre.IsNearlyZero() ? Me->GetActorForwardVector().GetSafeNormal2D() : ToCentre;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	TArray<TWeakObjectPtr<APawn>> Parked;
	for (int32 i = 0; i < Lineup.Num(); ++i)
	{
		if (!Lineup[i].Class)
		{
			continue;
		}
		const FVector At = Me->GetActorLocation() + Forward * (1500.f + i * 800.f) + Right * ((i % 2 == 0) ? -220.f : 220.f) + FVector(0.f, 0.f, 40.f);
		const FTransform Where(Forward.Rotation(), At);
		// Deferred so the kit variant can be forced before BeginPlay bolts the kit on.
		APawn* Car = World->SpawnActorDeferred<APawn>(Lineup[i].Class, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (AWarVehiclePawn* WarCar = Cast<AWarVehiclePawn>(Car))
		{
			WarCar->AutoPossessAI = EAutoPossessAI::Disabled; // Parked, not driving off.
			WarCar->SetKitVariantForPreview(Lineup[i].Variant);
			if (Lineup[i].KitOverride != EIronKit::None)
			{
				WarCar->Kit = Lineup[i].KitOverride;
			}
		}
		if (Car)
		{
			Car->FinishSpawning(Where);
			Parked.Add(Car);
		}
	}
	TWeakObjectPtr<APlayerController> WeakPC(PC);
	int32 Index = 0;
	float Wait = 1.5f; // Let the cars settle on their suspension first.
	bool bShotPending = false;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, Parked, Index, Wait, bShotPending](float Dt) mutable
	{
		APlayerController* Owner = WeakPC.Get();
		UWorld* Level = Owner ? Owner->GetWorld() : nullptr;
		if (!Level || Index >= Parked.Num())
		{
			return false;
		}
		Wait -= Dt;
		if (Wait > 0.f)
		{
			return true;
		}
		if (bShotPending)
		{
			Owner->ConsoleCommand(TEXT("HighResShot 1600x900"));
			bShotPending = false;
			++Index;
			Wait = 0.6f;
			return Index < Parked.Num();
		}
		APawn* Car = Parked[Index].Get();
		if (!Car)
		{
			++Index;
			return Index < Parked.Num();
		}
		// Front-left three-quarter view, a little above bonnet height.
		const FVector Target = Car->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const FVector CamAt = Target + Car->GetActorForwardVector() * 480.f - Car->GetActorRightVector() * 420.f + FVector(0.f, 0.f, 170.f);
		FActorSpawnParameters CamParams;
		CamParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACameraActor* Cam = Level->SpawnActor<ACameraActor>(CamAt, (Target - CamAt).Rotation(), CamParams))
		{
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			Cam->GetCameraComponent()->SetFieldOfView(70.f);
			Owner->bAutoManageActiveCameraTarget = false;
			Owner->SetViewTarget(Cam);
		}
		bShotPending = true;
		Wait = 0.8f;
		return true;
	}));
	UE_LOG(LogTemp, Log, TEXT("IronSiege: showroom stage %d, %d vehicles"), Stage, Lineup.Num());
}

void UIronSiegeCheatManager::DebugFlamer(float Seconds)
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (UFlamethrowerComponent* Flame = Car ? Car->GetFlamerComponent() : nullptr)
	{
		Flame->bUnlocked = true; // Skip the shop for the test.
		FlamerRemaining = FMath::Max(Seconds, 0.5f);
	}
}

void UIronSiegeCheatManager::DebugMineTest()
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	UMineLayerComponent* Rack = Car ? Car->GetMineWeaponComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Rack || !World)
	{
		return;
	}
	Rack->bUnlocked = true;
	Car->DeployMine();
	const FVector MineSpot = Car->GetActorLocation() - Car->GetActorForwardVector() * Rack->DropDistance;
	TWeakObjectPtr<AWarVehiclePawn> WeakCar(Car);
	float Wait = 2.f; // Longer than IronMines::Tuning::ArmSeconds, so the mine is live.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, MineSpot, Wait](float Dt) mutable
	{
		Wait -= Dt;
		if (Wait > 0.f)
		{
			return true;
		}
		AWarVehiclePawn* Player = WeakCar.Get();
		UWorld* Level = Player ? Player->GetWorld() : nullptr;
		if (!Level)
		{
			return false;
		}
		AWarVehiclePawn* Victim = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<AWarVehiclePawn> It(Level); It; ++It)
		{
			if (*It == Player || It->IsPlayerControlled())
			{
				continue;
			}
			const float Distance = FVector::Dist(MineSpot, It->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Victim = *It;
			}
		}
		if (!Victim)
		{
			UE_LOG(LogTemp, Log, TEXT("IronSiege: mine test - no enemy to lure"));
			return false;
		}
		Victim->SetActorLocation(MineSpot + FVector(0.f, 0.f, 80.f), false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("IronSiege: mine test - moved %s onto the mine"), *Victim->GetName());
		return false;
	}));
}

void UIronSiegeCheatManager::DebugDrift(float Seconds)
{
	DriftRemaining = FMath::Max(Seconds, 2.f);
	bDriftSliding = false;
}

void UIronSiegeCheatManager::DebugDropMines(int32 CountToDrop)
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	UMineLayerComponent* Rack = Car ? Car->GetMineWeaponComponent() : nullptr;
	if (!Rack)
	{
		return;
	}
	Rack->bUnlocked = true; // Skip the shop for the test.
	int32 Remaining = FMath::Max(CountToDrop, 1);
	float Cooldown = 0.f;
	TWeakObjectPtr<AWarVehiclePawn> WeakCar(Car);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, Remaining, Cooldown](float Dt) mutable
	{
		AWarVehiclePawn* Dropper = WeakCar.Get();
		if (!Dropper || Remaining <= 0)
		{
			return false;
		}
		Cooldown -= Dt;
		if (Cooldown > 0.f)
		{
			return true;
		}
		Dropper->DeployMine();
		Cooldown = 1.3f;
		--Remaining;
		UMineLayerComponent* Mines = Dropper->GetMineWeaponComponent();
		UE_LOG(LogTemp, Log, TEXT("IronSiege: dropped mine, %d left in rack (%d spare)"),
			Mines ? Mines->GetAmmo() : -1, Mines ? Mines->GetReserve() : -1);
		return Remaining > 0;
	}));
}

void UIronSiegeCheatManager::DebugBoost(float Seconds)
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (!Car)
	{
		return;
	}
	Car->SetBoost(Seconds > 0.f);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: boost %s, charge %.2f"), Seconds > 0.f ? TEXT("on") : TEXT("off"), Car->GetBoostCharge());
	if (Seconds > 0.f)
	{
		TWeakObjectPtr<AWarVehiclePawn> WeakCar(Car);
		float Remaining = Seconds;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, Remaining](float Dt) mutable
		{
			AWarVehiclePawn* Boosting = WeakCar.Get();
			if (!Boosting)
			{
				return false;
			}
			Remaining -= Dt;
			if (Remaining > 0.f)
			{
				return true;
			}
			Boosting->SetBoost(false);
			UE_LOG(LogTemp, Log, TEXT("IronSiege: boost off, charge %.2f"), Boosting->GetBoostCharge());
			return false;
		}));
	}
}

void UIronSiegeCheatManager::DebugRamNearest()
{
	APlayerController* PC = GetOuterAPlayerController();
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(PC ? PC->GetPawn() : nullptr);
	UWorld* World = GetWorld();
	if (!Car || !World)
	{
		return;
	}
	// Point at the nearest enemy and floor it; the ram happens on contact.
	AActor* Nearest = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AWarVehiclePawn> It(World); It; ++It)
	{
		if (*It == Car || It->IsPlayerControlled())
		{
			continue;
		}
		const float Distance = FVector::Dist(Car->GetActorLocation(), It->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Nearest = *It;
		}
	}
	if (!Nearest)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: no enemy to ram"));
		return;
	}
	const FVector Ahead = Car->GetActorLocation() + Car->GetActorForwardVector() * 3500.f;
	Nearest->SetActorLocation(FVector(Ahead.X, Ahead.Y, Nearest->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	Car->SetBoost(true);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: ramming %s, moved to 35 m ahead (was %.0f m)"), *Nearest->GetName(), BestDistance / 100.f);
	// Steer at the target every frame (it is driving too) and hold the throttle down for a few
	// seconds, so the test actually connects instead of chasing where the enemy used to be.
	TWeakObjectPtr<AWarVehiclePawn> WeakCar(Car);
	TWeakObjectPtr<AActor> WeakTarget(Nearest);
	float Remaining = 8.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, WeakTarget, Remaining](float Dt) mutable
	{
		AWarVehiclePawn* Chaser = WeakCar.Get();
		AActor* Target = WeakTarget.Get();
		Remaining -= Dt;
		if (!Chaser || !Target || Remaining <= 0.f)
		{
			if (Chaser)
			{
				Chaser->MoveForward(0.f);
				Chaser->Steer(0.f);
				Chaser->SetBoost(false);
			}
			return false;
		}
		const FVector ToTarget = Target->GetActorLocation() - Chaser->GetActorLocation();
		const float YawError = FMath::FindDeltaAngleDegrees(Chaser->GetActorRotation().Yaw, ToTarget.Rotation().Yaw);
		Chaser->Steer(FMath::Clamp(YawError / 25.f, -1.f, 1.f));
		Chaser->MoveForward(1.f);
		return true;
	}));
}

void UIronSiegeCheatManager::DebugProfile(const FString& Action, const FString& Name)
{
	UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	if (!S)
	{
		return;
	}
	if (Action == TEXT("save"))
	{
		S->SaveProfile(Name);
		UE_LOG(LogTemp, Log, TEXT("IronSiege: saved profile '%s' to %s"), *Name, *UIronSiegeUserSettings::ProfileFilePath(Name));
	}
	else if (Action == TEXT("load"))
	{
		const bool bLoaded = S->LoadProfile(Name);
		if (AIronSiegePlayerController* PC = GetIronController())
		{
			PC->ApplyUserSettings();
		}
		UE_LOG(LogTemp, Log, TEXT("IronSiege: load profile '%s' -> %s (difficulty %d, style %d, language %s)"),
			*Name, bLoaded ? TEXT("ok") : TEXT("missing"), S->Difficulty, S->DrivingStyle, *S->Language);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: profiles [%s], active '%s'"),
			*FString::Join(UIronSiegeUserSettings::GetProfileNames(), TEXT(", ")), *S->ProfileName);
	}
}

void UIronSiegeCheatManager::DebugDrivingStyle(int32 Style)
{
	UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	AIronSiegePlayerController* PC = GetIronController();
	if (!S || !PC)
	{
		return;
	}
	S->DrivingStyle = FMath::Clamp(Style, 0, IronSettings::DrivingStyleCount - 1);
	PC->ApplyUserSettings();
	UE_LOG(LogTemp, Log, TEXT("IronSiege: driving style %d (%s)"), S->DrivingStyle,
		S->DrivingStyle == 0 ? TEXT("arcade") : (S->DrivingStyle == 1 ? TEXT("balanced") : TEXT("simulation")));
}

void UIronSiegeCheatManager::Tick(float DeltaTime)
{
	if (DriveRemaining > 0.f)
	{
		DriveRemaining -= DeltaTime;
		if (IIronVehicle* Vehicle = GetVehicle())
		{
			const bool bActive = DriveRemaining > 0.f;
			Vehicle->MoveForward(bActive ? DriveThrottle : 0.f);
			Vehicle->Steer(bActive ? DriveSteer : 0.f);
		}
	}
	if (DriftRemaining > 0.f)
	{
		DriftRemaining -= DeltaTime;
		if (IIronVehicle* Vehicle = GetVehicle())
		{
			const bool bActive = DriftRemaining > 0.f;
			// Straight line until there is enough speed to break traction, then full lock and
			// handbrake: the tyre smoke, marks and screech all hang off that slide.
			if (!bDriftSliding && Vehicle->GetSpeedKph() > 45.f)
			{
				bDriftSliding = true;
				UE_LOG(LogTemp, Log, TEXT("IronSiege: drift started at %.0f kph"), Vehicle->GetSpeedKph());
			}
			Vehicle->MoveForward(bActive ? 1.f : 0.f);
			Vehicle->Steer(bActive && bDriftSliding ? 1.f : 0.f);
			Vehicle->SetHandbrake(bActive && bDriftSliding);
		}
	}
	if (FlamerRemaining > 0.f)
	{
		FlamerRemaining -= DeltaTime;
		if (IIronVehicle* Vehicle = GetVehicle())
		{
			Vehicle->FireFlamer();
		}
	}
	if (FireRemaining > 0.f)
	{
		FireRemaining -= DeltaTime;
		if (IIronVehicle* Vehicle = GetVehicle())
		{
			if (FireWeapon == 2) Vehicle->FireSecondary();
			else Vehicle->FirePrimary();
		}
	}
	if (FpsRemaining > 0.f)
	{
		FpsTotal += DeltaTime;
		FpsWorst = FMath::Max(FpsWorst, DeltaTime);
		++FpsFrames;
		FpsRemaining -= DeltaTime;
		if (FpsRemaining <= 0.f && FpsFrames > 0)
		{
			const float AvgMs = 1000.f * FpsTotal / FpsFrames;
			UE_LOG(LogTemp, Log, TEXT("IronSiege: FPS measure - %d frames, avg %.2f ms (%.1f fps), worst %.2f ms"), FpsFrames, AvgMs, 1000.f / AvgMs, FpsWorst * 1000.f);
		}
	}
}

void UIronSiegeCheatManager::DebugSpawnHunter(float Distance)
{
	if (AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr)
	{
		GameMode->DebugSpawnHunter(Distance != 0.f ? Distance : 4000.f);
	}
}

void UIronSiegeCheatManager::DebugFlares()
{
	const APlayerController* PC = GetOuterAPlayerController();
	if (IIronVehicle* Vehicle = PC ? Cast<IIronVehicle>(PC->GetPawn()) : nullptr)
	{
		Vehicle->FireFlares();
	}
}

void UIronSiegeCheatManager::DebugAutoFlares(float Distance)
{
	TWeakObjectPtr<APlayerController> WeakPC = GetOuterAPlayerController();
	const float Trigger = Distance > 0.f ? Distance : 5500.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, Trigger](float Dt)
	{
		APlayerController* PC = WeakPC.Get();
		AWarVehiclePawn* Car = PC ? Cast<AWarVehiclePawn>(PC->GetPawn()) : nullptr;
		if (!PC)
		{
			return false;
		}
		if (!Car || !Car->GetIncomingMissile() || Car->GetIncomingMissileDistance() > Trigger)
		{
			return true; // Keep waiting.
		}
		// Capture the warning first; the flares go a few frames later, once the shot has been taken.
		UE_LOG(LogTemp, Log, TEXT("IronSiege: auto-flares - missile at %.0f cm"), Car->GetIncomingMissileDistance());
		PC->ConsoleCommand(TEXT("Shot showui"));
		// ...and a second shot shortly after, to catch the decoyed missile bursting.
		TWeakObjectPtr<AWarVehiclePawn> WeakCar(Car);
		float Clock = 0.f;
		bool bFired = false;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, WeakPC, Clock, bFired](float Step) mutable
		{
			Clock += Step;
			AWarVehiclePawn* Target = WeakCar.Get();
			if (!Target)
			{
				return false;
			}
			if (!bFired && Clock >= 0.12f)
			{
				Target->FireFlares();
				bFired = true;
			}
			if (Clock >= 0.72f)
			{
				if (APlayerController* Viewer = WeakPC.Get())
				{
					Viewer->ConsoleCommand(TEXT("HighResShot 1600x900"));
				}
				return false;
			}
			return true;
		}));
		return false;
	}));
}

void UIronSiegeCheatManager::DebugRailgun()
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (Car && Car->GetRailgun())
	{
		Car->GetRailgun()->bUnlocked = true; // Skip the shop for the test.
		Car->FireRailgun();
	}
}

void UIronSiegeCheatManager::DebugTesla()
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (Car && Car->GetTesla())
	{
		Car->GetTesla()->bUnlocked = true;
		Car->FireTesla();
	}
}

void UIronSiegeCheatManager::DebugSpawnLine(int32 Count, float Spacing)
{
	APawn* Me = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	const AIronSiegeGameMode* GameMode = World ? World->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	if (!Me || !GameMode || !GameMode->EnemyVehicleClass)
	{
		return;
	}
	const FVector Forward = Me->GetActorForwardVector().GetSafeNormal2D();
	for (int32 i = 0; i < FMath::Clamp(Count, 1, 8); ++i)
	{
		// Slightly staggered so a tesla chain has to jump sideways too.
		const FVector At = Me->GetActorLocation() + Forward * (1200.f + i * Spacing) + FVector::CrossProduct(FVector::UpVector, Forward) * ((i % 2) ? 150.f : -150.f) + FVector(0.f, 0.f, 60.f);
		const FTransform Where(Forward.Rotation(), At);
		APawn* Car = World->SpawnActorDeferred<APawn>(GameMode->EnemyVehicleClass, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Car)
		{
			Car->AutoPossessAI = EAutoPossessAI::Disabled;
			Car->FinishSpawning(Where);
		}
	}
}

void UIronSiegeCheatManager::DebugSpawnSpecialist(int32 Kind, float Distance)
{
	if (AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr)
	{
		GameMode->DebugSpawnSpecialist(Kind, Distance != 0.f ? Distance : 4000.f);
	}
}

void UIronSiegeCheatManager::DebugShotOnRailCharge()
{
	TWeakObjectPtr<APlayerController> WeakPC = GetOuterAPlayerController();
	float Wait = 0.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, Wait](float Dt) mutable
	{
		APlayerController* PC = WeakPC.Get();
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}
		for (TActorIterator<AWarVehiclePawn> It(World); It; ++It)
		{
			if (!It->IsPlayerControlled() && It->IsRailgunCharging() && It->IsRailAimFixed())
			{
				Wait += Dt;
				if (Wait < 0.15f)
				{
					return true; // Let the charge glow build up a little first.
				}
				PC->ConsoleCommand(TEXT("Shot showui"));
				return false;
			}
		}
		return true;
	}));
}

void UIronSiegeCheatManager::DebugSpawnEnemyAt(float X, float Y)
{
	UWorld* World = GetWorld();
	const AIronSiegeGameMode* GameMode = World ? World->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	APawn* Me = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
	if (!GameMode || !GameMode->EnemyVehicleClass || !Me)
	{
		return;
	}
	const FVector At(X, Y, Me->GetActorLocation().Z + 100.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const APawn* Car = World->SpawnActor<APawn>(GameMode->EnemyVehicleClass, At, (Me->GetActorLocation() - At).GetSafeNormal2D().Rotation(), Params);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: debug enemy %s at %s"), Car ? *Car->GetName() : TEXT("FAILED"), *At.ToString());
}

void UIronSiegeCheatManager::DebugStreetRouting(int32 bEnabled)
{
	AIronSiegeAIController::bStreetRoutingEnabled = bEnabled != 0;
	UE_LOG(LogTemp, Log, TEXT("IronSiege: street routing %s"), bEnabled ? TEXT("on") : TEXT("off"));
}

void UIronSiegeCheatManager::DebugTrackEnemies(float Seconds)
{
	TWeakObjectPtr<APlayerController> WeakPC = GetOuterAPlayerController();
	float Left = Seconds, Tick = 0.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, Left, Tick](float Dt) mutable
	{
		APlayerController* PC = WeakPC.Get();
		APawn* Me = PC ? PC->GetPawn() : nullptr;
		Left -= Dt;
		Tick -= Dt;
		if (!Me || Left <= 0.f)
		{
			return false;
		}
		if (Tick > 0.f)
		{
			return true;
		}
		Tick = 1.f;
		FString Line;
		for (TActorIterator<AWarVehiclePawn> It(Me->GetWorld()); It; ++It)
		{
			if (*It != Me)
			{
				const AIronSiegeAIController* AI = Cast<AIronSiegeAIController>(It->GetController());
				const UVehicleHealthComponent* Hp = It->GetHealthComponent();
				Line += FString::Printf(TEXT(" %s %.0fm %.0fkph at (%.0f, %.0f) hp %.0f+%.0f %s |"), *It->GetName(), FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) / 100.f, It->GetSpeedKph(),
					It->GetActorLocation().X, It->GetActorLocation().Y, Hp ? Hp->GetHealth() : 0.f, Hp ? Hp->GetArmor() : 0.f, AI ? *AI->DescribeRoute() : TEXT("-"));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("IronSiege: track [me %.0f, %.0f]%s"), Me->GetActorLocation().X, Me->GetActorLocation().Y, *Line);
		return true;
	}));
}

void UIronSiegeCheatManager::DebugTeleport(float X, float Y, float Yaw)
{
	APawn* Me = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
	if (!Me)
	{
		return;
	}
	// A Chaos car keeps its own physics state: move the body itself, zero its velocity and reset the
	// vehicle simulation, or the physics thread puts the car back where it was a few seconds later.
	Me->SetActorLocationAndRotation(FVector(X, Y, Me->GetActorLocation().Z + 60.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Me->GetRootComponent()))
	{
		Body->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (AWheeledVehiclePawn* Wheeled = Cast<AWheeledVehiclePawn>(Me))
	{
		if (UChaosVehicleMovementComponent* Movement = Wheeled->GetVehicleMovementComponent())
		{
			Movement->ResetVehicle();
		}
	}
	// The reset also put the wheels back to the Blueprint's grip and brakes.
	if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(Me))
	{
		Car->RefreshWheelSettings();
	}
}

void UIronSiegeCheatManager::DebugSpawnEnemyFacing(float X, float Y, float Yaw)
{
	UWorld* World = GetWorld();
	const AIronSiegeGameMode* GameMode = World ? World->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	APawn* Me = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
	if (!GameMode || !GameMode->EnemyVehicleClass || !Me)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const APawn* Car = World->SpawnActor<APawn>(GameMode->EnemyVehicleClass, FVector(X, Y, Me->GetActorLocation().Z + 100.f), FRotator(0.f, Yaw, 0.f), Params);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: debug enemy %s at (%.0f, %.0f) facing %.0f"), Car ? *Car->GetName() : TEXT("FAILED"), X, Y, Yaw);
}

void UIronSiegeCheatManager::DebugDragTest(float Seconds)
{
	TWeakObjectPtr<AWarVehiclePawn> WeakCar = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (!WeakCar.IsValid())
	{
		return;
	}
	struct FRun
	{
		float T = 0.f, T50 = -1.f, T100 = -1.f, Top = 0.f, BrakeFrom = 0.f, BrakeT = 0.f, NextLog = 1.f;
		FVector BrakeStart = FVector::ZeroVector;
		bool bBraking = false;
	};
	TSharedRef<FRun> Run = MakeShared<FRun>();
	const float Accel = FMath::Max(Seconds, 1.f);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, Run, Accel](float Dt) mutable
	{
		AWarVehiclePawn* Car = WeakCar.Get();
		if (!Car)
		{
			return false;
		}
		Run->T += Dt;
		const float Kph = Car->GetSpeedKph();
		if (!Run->bBraking)
		{
			Car->MoveForward(1.f);
			Car->Steer(0.f);
			if (Run->T50 < 0.f && Kph >= 50.f) Run->T50 = Run->T;
			if (Run->T100 < 0.f && Kph >= 100.f) Run->T100 = Run->T;
			Run->Top = FMath::Max(Run->Top, Kph);
			if (Run->T >= Run->NextLog)
			{
				// The acceleration curve second by second: where it stalls says why (a gear, the rev
				// limit, drag, the class limit).
				Run->NextLog += 1.f;
				const UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovementComponent());
				UE_LOG(LogTemp, Log, TEXT("IronSiege: drag t=%.0fs %.0f kph gear %d rpm %.0f/%.0f wheels %d"), Run->T, Kph, Wheeled ? Wheeled->GetCurrentGear() : 0,
					Wheeled ? Wheeled->GetEngineRotationSpeed() : 0.f, Wheeled ? Wheeled->GetEngineMaxRotationSpeed() : 0.f, Car->GetWheelsOnGround());
			}
			if (Run->T >= Accel)
			{
				Run->bBraking = true;
				Run->BrakeFrom = Kph;
				Run->BrakeStart = Car->GetActorLocation();
				Run->BrakeT = 0.f;
			}
			return true;
		}
		Car->MoveForward(-1.f);
		Run->BrakeT += Dt;
		if (Run->BrakeT >= Run->NextLog - Accel)
		{
			// What each wheel is doing under braking: the torque it gets, and whether it slides.
			Run->NextLog += 0.5f;
			if (const UChaosWheeledVehicleMovementComponent* Wheeled = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovementComponent()))
			{
				FString Wheels;
				for (int32 i = 0; i < Wheeled->WheelSetups.Num() && i < Wheeled->Wheels.Num(); ++i)
				{
					const FWheelStatus& W = Wheeled->GetWheelState(i);
					Wheels += FString::Printf(TEXT(" [%d brake %.0f skid %d abs %d slip %.2f]"), i, W.BrakeTorque, W.bIsSkidding ? 1 : 0, W.bABSActivated ? 1 : 0, W.SlipMagnitude);
				}
				UE_LOG(LogTemp, Log, TEXT("IronSiege: brake t=%.1fs %.0f kph brake input %.2f%s"), Run->BrakeT, Kph, const_cast<UChaosWheeledVehicleMovementComponent*>(Wheeled)->GetBrakeInput(), *Wheels);
			}
		}
		if (Kph < 1.f || Run->BrakeT > 12.f)
		{
			Car->MoveForward(0.f);
			UE_LOG(LogTemp, Log, TEXT("IronSiege: drag %s - 0-50 %.2fs, 0-100 %.2fs, top %.0f kph (limit %.0f), brakes %.0f-0 kph in %.1f m / %.2fs, mass %.0f kg"),
				*Car->GetName(), Run->T50, Run->T100, Run->Top, Car->GetTopSpeedLimitKph(), Run->BrakeFrom,
				FVector::Dist2D(Run->BrakeStart, Car->GetActorLocation()) / 100.f, Run->BrakeT, Car->GetMesh() ? Car->GetMesh()->GetMass() : 0.f);
			return false;
		}
		return true;
	}));
}

void UIronSiegeCheatManager::DebugAirDrop(float Height, float RollDeg, float PitchDeg)
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (!Car)
	{
		return;
	}
	const FRotator Tilted(PitchDeg, Car->GetActorRotation().Yaw, RollDeg);
	Car->SetActorLocationAndRotation(Car->GetActorLocation() + FVector(0.f, 0.f, Height), Tilted, false, nullptr, ETeleportType::ResetPhysics);
	if (USkeletalMeshComponent* Body = Car->GetMesh())
	{
		Body->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	// As in DebugTeleport: without resetting the vehicle simulation the physics thread puts the car
	// straight back on the ground.
	if (UChaosVehicleMovementComponent* Movement = Car->GetVehicleMovementComponent())
	{
		Movement->ResetVehicle();
	}
	Car->RefreshWheelSettings();
	UE_LOG(LogTemp, Log, TEXT("IronSiege: air drop %s from %.0f cm, roll %.0f pitch %.0f (up.z %.2f)"), *Car->GetName(), Height, RollDeg, PitchDeg, Car->GetActorUpVector().Z);
	TWeakObjectPtr<AWarVehiclePawn> WeakCar = Car;
	float Airborne = 0.f, AfterLanding = -1.f, Total = 0.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, Airborne, AfterLanding, Total](float Dt) mutable
	{
		AWarVehiclePawn* Car = WeakCar.Get();
		if (!Car)
		{
			return false;
		}
		Total += Dt;
		const int32 OnGround = Car->GetWheelsOnGround();
		if (AfterLanding < 0.f)
		{
			Airborne += Dt;
			if ((OnGround > 0 && Airborne > 0.2f) || Total > 8.f)
			{
				AfterLanding = 0.f;
				UE_LOG(LogTemp, Log, TEXT("IronSiege: air drop touchdown after %.2fs - up.z %.2f, %d wheels down"), Airborne, Car->GetActorUpVector().Z, OnGround);
			}
			return true;
		}
		AfterLanding += Dt;
		if (AfterLanding >= 1.f)
		{
			UE_LOG(LogTemp, Log, TEXT("IronSiege: air drop settled - up.z %.2f, %d wheels down"), Car->GetActorUpVector().Z, OnGround);
			return false;
		}
		return true;
	}));
}

void UIronSiegeCheatManager::DebugWheelBrake(float Torque)
{
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	UChaosWheeledVehicleMovementComponent* Wheeled = Car ? Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovementComponent()) : nullptr;
	if (!Wheeled)
	{
		return;
	}
	for (int32 i = 0; i < Wheeled->WheelSetups.Num(); ++i)
	{
		Wheeled->SetWheelMaxBrakeTorque(i, Torque);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: wheel brake torque set to %.0f on %d wheels"), Torque, Wheeled->WheelSetups.Num());
}

void UIronSiegeCheatManager::DebugAimAssist(int32 Level)
{
	if (UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get())
	{
		S->AimAssist = FMath::Clamp(Level, 0, IronAim::LevelCount - 1);
		UE_LOG(LogTemp, Log, TEXT("IronSiege: aim assist %d"), S->AimAssist);
	}
}

void UIronSiegeCheatManager::DebugHandlingTest(int32 Kind)
{
	TWeakObjectPtr<AWarVehiclePawn> WeakCar = Cast<AWarVehiclePawn>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (!WeakCar.IsValid())
	{
		return;
	}
	struct FRun
	{
		int32 Kind = 0;
		int32 Phase = 0;
		float T = 0.f, PhaseT = 0.f;
		float StartYaw = 0.f, Turned = 0.f, LastYaw = 0.f;
		float T90 = -1.f, T180 = -1.f, Path = 0.f, PeakSlip = 0.f, SlipTime = 0.f, EntryKph = 0.f, MinUp = 1.f;
		FVector LastPos = FVector::ZeroVector;
	};
	TSharedRef<FRun> Run = MakeShared<FRun>();
	Run->Kind = FMath::Clamp(Kind, 0, 5);
	Run->LastPos = WeakCar->GetActorLocation();
	Run->LastYaw = WeakCar->GetActorRotation().Yaw;
	static const TCHAR* Names[] = { TEXT("turn"), TEXT("reverse"), TEXT("drift"), TEXT("u-turn"), TEXT("handbrake turn"), TEXT("J-turn") };
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCar, Run](float Dt) mutable
	{
		AWarVehiclePawn* Car = WeakCar.Get();
		if (!Car || Dt <= 0.f)
		{
			return false;
		}
		Run->T += Dt;
		Run->PhaseT += Dt;
		Run->MinUp = FMath::Min(Run->MinUp, float(Car->GetActorUpVector().Z));
		const float Kph = Car->GetSpeedKph();
		const UChaosVehicleMovementComponent* Movement = Car->GetVehicleMovementComponent();
		const float SignedKph = Movement ? Movement->GetForwardSpeed() * 0.036f : Kph;
		// Heading change since the manoeuvre began (unwrapped), and the slide angle between where
		// the nose points and where the car is actually going.
		const float Yaw = Car->GetActorRotation().Yaw;
		const float DYaw = FMath::FindDeltaAngleDegrees(Run->LastYaw, Yaw);
		Run->LastYaw = Yaw;
		const FVector Pos = Car->GetActorLocation();
		const FVector Step = (Pos - Run->LastPos) / Dt;
		Run->LastPos = Pos;
		float Slip = 0.f;
		if (Step.Size2D() > 300.f)
		{
			const FVector Fwd = Car->GetActorForwardVector().GetSafeNormal2D();
			const FVector Dir = Step.GetSafeNormal2D();
			Slip = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(FVector::DotProduct(Fwd, Dir)), 0.f, 1.f)));
		}
		auto Finish = [&](const FString& Result)
		{
			Car->MoveForward(0.f);
			Car->Steer(0.f);
			Car->SetHandbrake(false);
			UE_LOG(LogTemp, Log, TEXT("IronSiege: handling %s %s - %s (worst lean: upright %.2f, %d wheels down now)"), Names[Run->Kind], *Car->GetName(), *Result, Run->MinUp, Car->GetWheelsOnGround());
			return false;
		};
		auto Measure = [&]()
		{
			Run->Turned += DYaw;
			Run->Path += Step.Size2D() * Dt;
			const float A = FMath::Abs(Run->Turned);
			if (Run->T90 < 0.f && A >= 90.f) Run->T90 = Run->PhaseT;
			if (Run->T180 < 0.f && A >= 180.f) Run->T180 = Run->PhaseT;
		};
		switch (Run->Kind)
		{
		case 0: // Turn at 50 km/h.
			if (Run->Phase == 0)
			{
				Car->Steer(0.f);
				Car->MoveForward(Kph < 50.f ? 1.f : 0.3f);
				if (Kph >= 50.f && Run->T > 1.f) { Run->Phase = 1; Run->PhaseT = 0.f; Run->EntryKph = Kph; }
				if (Run->T > 12.f) return Finish(TEXT("never reached 50 km/h"));
				return true;
			}
			Car->Steer(1.f);
			Car->MoveForward(Kph < 50.f ? 0.8f : 0.3f);
			Measure();
			if (Run->T180 >= 0.f || Run->PhaseT > 8.f)
			{
				const float Radius = Run->Turned != 0.f ? Run->Path / FMath::DegreesToRadians(FMath::Abs(Run->Turned)) / 100.f : 0.f;
				return Finish(FString::Printf(TEXT("entry %.0f km/h, 90 deg in %.2fs, 180 deg in %.2fs, circle radius %.1f m, now %.0f km/h"), Run->EntryKph, Run->T90, Run->T180, Radius, Kph));
			}
			return true;
		case 1: // Reverse from 30 km/h.
			if (Run->Phase == 0)
			{
				Car->Steer(0.f);
				Car->MoveForward(Kph < 30.f ? 1.f : 0.2f);
				if (Kph >= 30.f && Run->T > 1.f) { Run->Phase = 1; Run->PhaseT = 0.f; Run->EntryKph = Kph; }
				if (Run->T > 12.f) return Finish(TEXT("never reached 30 km/h"));
				return true;
			}
			Car->MoveForward(-1.f);
			if (Run->Phase == 1 && SignedKph <= -10.f) { Run->T90 = Run->PhaseT; Run->Phase = 2; }
			if (Run->PhaseT >= 4.f + FMath::Max(Run->T90, 0.f) || Run->PhaseT > 10.f)
			{
				return Finish(FString::Printf(TEXT("from %.0f km/h forward: rolling back at 10 km/h after %.2fs, reverse speed %.0f km/h 4s later"), Run->EntryKph, Run->T90, -SignedKph));
			}
			return true;
		case 2: // Drift at 70 km/h.
			if (Run->Phase == 0)
			{
				Car->Steer(0.f);
				Car->MoveForward(1.f);
				if (Kph >= 70.f && Run->T > 1.f) { Run->Phase = 1; Run->PhaseT = 0.f; Run->EntryKph = Kph; }
				if (Run->T > 14.f) return Finish(TEXT("never reached 70 km/h"));
				return true;
			}
			Measure();
			Run->PeakSlip = FMath::Max(Run->PeakSlip, Slip);
			if (Slip > 15.f) Run->SlipTime += Dt;
			if (Run->PhaseT < 0.6f)
			{
				Car->Steer(1.f);
				Car->SetHandbrake(true);
				Car->MoveForward(0.6f);
			}
			else
			{
				Car->SetHandbrake(false);
				Car->Steer(-0.5f);
				Car->MoveForward(1.f);
			}
			if (Run->PhaseT > 3.f)
			{
				return Finish(FString::Printf(TEXT("entry %.0f km/h: peak slide %.0f deg, %.2fs past 15 deg, exit %.0f km/h, heading changed %.0f deg"), Run->EntryKph, Run->PeakSlip, Run->SlipTime, Kph, FMath::Abs(Run->Turned)));
			}
			return true;
		case 4: // Handbrake turn at 45 km/h.
		case 5: // J-turn: reverse to 30 km/h, then spin round on the handbrake.
		{
			const bool bJ = Run->Kind == 5;
			if (Run->Phase == 0)
			{
				Car->Steer(0.f);
				Car->MoveForward(bJ ? -1.f : (Kph < 45.f ? 1.f : 0.3f));
				const bool bReady = bJ ? SignedKph <= -24.f : Kph >= 45.f;
				if (bReady && Run->T > 1.f) { Run->Phase = 1; Run->PhaseT = 0.f; Run->EntryKph = SignedKph; }
				if (Run->T > 14.f) return Finish(TEXT("never reached the entry speed"));
				return true;
			}
			Measure();
			Car->Steer(1.f);
			Car->SetHandbrake(Run->T180 < 0.f);
			Car->MoveForward(bJ ? 0.f : 0.5f);
			if (Run->T180 >= 0.f || Run->PhaseT > 4.f)
			{
				return Finish(FString::Printf(TEXT("entry %.0f km/h: 90 deg in %.2fs, facing the other way after %.2fs, turned %.0f deg, now %.0f km/h"), Run->EntryKph, Run->T90, Run->T180, FMath::Abs(Run->Turned), SignedKph));
			}
			return true;
		}
		default: // U-turn from a standstill.
			Measure();
			if (Run->Phase == 0)
			{
				Car->Steer(1.f);
				Car->MoveForward(-1.f);
				if (Run->PhaseT > 1.6f) { Run->Phase = 1; }
			}
			else
			{
				// Like a driver: hold the lock until the car has stopped rolling back, then swing it.
				Car->Steer(SignedKph < -2.f ? 1.f : -1.f);
				Car->MoveForward(1.f);
			}
			if (Run->T180 >= 0.f || Run->T > 10.f)
			{
				return Finish(FString::Printf(TEXT("facing the other way after %.2fs (90 deg at %.2fs), heading changed %.0f deg"), Run->T180, Run->T90, FMath::Abs(Run->Turned)));
			}
			return true;
		}
	}));
}

// ---------------------------------------------------------------- Campaign and crew

void UIronSiegeCheatManager::DebugDriver(int32 Driver)
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->SetPendingDriver(Driver);
	}
}

void UIronSiegeCheatManager::DebugAbility()
{
	const APlayerController* PC = GetOuterAPlayerController();
	if (AWarVehiclePawn* Car = PC ? Cast<AWarVehiclePawn>(PC->GetPawn()) : nullptr)
	{
		const bool bUsed = Car->UseAbility();
		UE_LOG(LogTemp, Log, TEXT("IronSiege: DebugAbility %s (driver %d)"), bUsed ? TEXT("fired") : TEXT("not ready"), Car->GetDriverIndex());
	}
}

void UIronSiegeCheatManager::DebugMissionSkip()
{
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	if (AIronMissionDirector* Director = GameMode ? GameMode->GetDirector() : nullptr)
	{
		Director->DebugSkipStage();
	}
}

void UIronSiegeCheatManager::DebugMissionFail()
{
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	if (AIronMissionDirector* Director = GameMode ? GameMode->GetDirector() : nullptr)
	{
		Director->DebugFail();
	}
}

void UIronSiegeCheatManager::DebugMissionLog()
{
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	const AIronMissionDirector* Director = GameMode ? GameMode->GetDirector() : nullptr;
	if (!Director)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: mission log - no mission running (mission mode %d)"), GameMode && GameMode->IsMissionMode() ? 1 : 0);
		return;
	}
	TArray<FIronMissionMarker> Markers;
	Director->GetMarkers(Markers);
	const IronMissions::StageState& State = Director->GetStageState();
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission log - mission %d stage %d result %d, \"%s\", progress %d lost %d, stage %.0f s, mission %.0f s, enemies %d, markers %d, stars %d"),
		Director->GetMissionIndex() + 1, Director->GetStageIndex() + 1, static_cast<int32>(Director->GetResult()), *Director->GetObjectiveText(), State.Progress, State.Lost,
		State.Elapsed, Director->GetElapsed(), GameMode->GetEnemiesAlive(), Markers.Num(), Director->GetStars());
	for (const FIronMissionMarker& M : Markers)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege:   marker %s at %s fraction %.2f friendly %d"), *M.Label, *M.Location.ToCompactString(), M.Fraction, M.bFriendly ? 1 : 0);
	}
	// The enemies sent at a truck or the relay: how far off they are and whether they are lined up on it.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const AIronSiegeAIController* Driver = Cast<AIronSiegeAIController>(It->GetController());
		const AActor* Wanted = Driver ? Driver->GetPreferredTarget() : nullptr;
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(*It);
		if (Wanted && Vehicle)
		{
			const FVector To = Wanted->GetActorLocation() - It->GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("IronSiege:   attacker %s after %s: %.0f m off, facing %.2f, %.0f km/h"), *It->GetName(), *Wanted->GetName(), To.Size() / 100.f,
				FVector::DotProduct(It->GetActorForwardVector(), To.GetSafeNormal()), Vehicle->GetSpeedKph());
		}
	}
}

void UIronSiegeCheatManager::DebugScreen(int32 Screen)
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->DebugSetScreen(Screen);
	}
}

void UIronSiegeCheatManager::DebugMenuKey(int32 Key)
{
	if (AIronSiegePlayerController* PC = GetIronController())
	{
		PC->DebugMenuKey(Key);
	}
}

void UIronSiegeCheatManager::DebugCampaign(const FString& Action, int32 Stars)
{
	IronMissions::Progress Progress;
	if (Action.Equals(TEXT("unlock"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 0; i < IronMissions::Count; ++i)
		{
			Progress.Best[i] = FMath::Clamp(Stars, 1, 3);
		}
	}
	else if (!Action.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		Progress = IronStory::LoadProgress();
		UE_LOG(LogTemp, Log, TEXT("IronSiege: campaign - %d mission(s) done, %d star(s)"), Progress.Completed(), Progress.TotalStars());
		return;
	}
	IronStory::SaveProgress(Progress);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: campaign %s - %d mission(s) done, %d star(s)"), *Action, Progress.Completed(), Progress.TotalStars());
}

void UIronSiegeCheatManager::DebugDriverXp(int32 Driver, int32 Xp)
{
	IronRanks::Roster Roster = IronStory::LoadRoster();
	for (int32 d = 0; d < IronCrew::DriverCount; ++d)
	{
		if (Driver < 0 || d == Driver)
		{
			Roster.Xp[d] = FMath::Clamp(Xp, 0, 1000000);
		}
	}
	IronStory::SaveRoster(Roster);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: driver %d xp set to %d (rank %d)"), Driver, Xp, IronRanks::RankFor(FMath::Max(Xp, 0)));
}

void UIronSiegeCheatManager::DebugRadio(int32 Speaker, int32 Mood)
{
	const APlayerController* PC = GetOuterAPlayerController();
	if (AIronSiegeHUD* HUD = PC ? PC->GetHUD<AIronSiegeHUD>() : nullptr)
	{
		// The second briefing line of the first mission: long enough to wrap.
		const IronMissions::Radio& Sample = IronMissions::Get(0).Brief[1];
		HUD->Say(Speaker, Mood, Sample.Key, Sample.Text, true);
	}
}

void UIronSiegeCheatManager::DebugGoToObjective(float Distance)
{
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	const AIronMissionDirector* Director = GameMode ? GameMode->GetDirector() : nullptr;
	const APlayerController* PC = GetOuterAPlayerController();
	const APawn* Car = PC ? PC->GetPawn() : nullptr;
	TArray<FIronMissionMarker> Markers;
	if (Director)
	{
		Director->GetMarkers(Markers);
	}
	if (!Car || Markers.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("IronSiege: DebugGoToObjective - nothing to go to"));
		return;
	}
	// Stop short of it on the side the car is already on, facing it.
	const FVector To = Markers[0].Location;
	FVector Away = (Car->GetActorLocation() - To).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector(1.f, 0.f, 0.f);
	}
	const FVector At = To + Away * FMath::Max(Distance, 0.f);
	DebugTeleport(At.X, At.Y, (-Away).Rotation().Yaw);
}
