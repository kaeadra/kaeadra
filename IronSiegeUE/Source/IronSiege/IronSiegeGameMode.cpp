#include "IronSiegeGameMode.h"
#include "IronSiegePlayerController.h"
#include "IronSiegeHUD.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "IronCityStreet.h"
#include "EngineUtils.h"
#include "IronSupplyCrate.h"
#include "WarVehiclePawn.h"
#include "IronSiegeUserSettings.h"
#include "IronSiegeAIController.h"
#include "SettingsRules.h"
#include "MachineGunComponent.h"
#include "RocketLauncherComponent.h"
#include "WaveRules.h"
#include "IronArena.h"
#include "EnergyWeapons.h"
#include "Engine/OverlapResult.h"
#include "IronSiegeText.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "CrewRules.h"
#include "IronMissionDirector.h"
#include "IronSiegeStory.h"
#include "MissionRules.h"

namespace
{
	const TCHAR* GRecordsSection = TEXT("IronSiege.Records");
}

AIronSiegeGameMode::AIronSiegeGameMode()
{
	// Left null on purpose - see header comment: the vehicle-select screen picks the class.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AIronSiegePlayerController::StaticClass();
	SupplyCrateClass = AIronSupplyCrate::StaticClass();
	HUDClass = AIronSiegeHUD::StaticClass();
}

void AIronSiegeGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Greybox maps become themed battlefields (AIronArena) unless one was placed by hand.
	EIronArenaTheme ArenaTheme{};
	if (UWorld* World = GetWorld(); World && AIronArena::ThemeForMap(World->GetMapName(), ArenaTheme))
	{
		bool bPlaced = false;
		for (TActorIterator<AIronArena> It(World); It; ++It)
		{
			bPlaced = true;
		}
		if (!bPlaced)
		{
			FActorSpawnParameters Params;
			Params.bDeferConstruction = true;
			if (AIronArena* Arena = World->SpawnActor<AIronArena>(AIronArena::StaticClass(), FTransform::Identity, Params))
			{
				Arena->Theme = ArenaTheme;
				Arena->FinishSpawning(FTransform::Identity);
			}
		}
	}
	if (GConfig)
	{
		GConfig->GetInt(GRecordsSection, TEXT("BestWave"), BestWave, GGameIni);
		GConfig->GetInt(GRecordsSection, TEXT("BestKills"), BestKills, GGameIni);
		GConfig->GetInt(GRecordsSection, TEXT("BestScore"), BestScore, GGameIni);
	}
}

void AIronSiegeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// "?Mission=3" plays the third campaign mission on this level instead of the survival waves.
	const int32 Number = UGameplayStatics::GetIntOption(Options, TEXT("Mission"), 0);
	MissionIndex = Number >= 1 && Number <= IronMissions::Count ? Number - 1 : -1;
}

AActor* AIronSiegeGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (IsMissionMode())
	{
		// The mission is laid out from one corner: deploy from the start nearest the one it names.
		const IronMissions::Spot& Want = IronMissions::Get(MissionIndex).Start;
		const FVector2D Wanted(Want.X * 9500.f, Want.Y * 9500.f);
		APlayerStart* Best = nullptr;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (!Best || FVector2D::DistSquared(FVector2D(It->GetActorLocation()), Wanted) < FVector2D::DistSquared(FVector2D(Best->GetActorLocation()), Wanted))
			{
				Best = *It;
			}
		}
		if (Best)
		{
			return Best;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AIronSiegeGameMode::SpawnSelectedVehicle(AController* Controller, int32 ClassIndex, int32 DriverIndex)
{
	if (!Controller || !VehicleClasses.IsValidIndex(ClassIndex) || !VehicleClasses[ClassIndex])
	{
		return;
	}
	DefaultPawnClass = VehicleClasses[ClassIndex];
	RestartPlayer(Controller);

	PlayerPawn = Controller->GetPawn();
	Wave = 0;
	Kills = 0;
	Score = IronScore::ScoreState();
	Upgrades = IronUpgrades::Loadout();
	bShopOpen = false;
	bWaveCleared = false;
	bGameOver = false;
	bMissionWon = false;
	LiveEnemies.Reset();
	PlayerDriver = DriverIndex >= 0 && DriverIndex < IronCrew::DriverCount ? DriverIndex : -1;
	XpGained = 0;
	RankReached = 0;
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(PlayerPawn.Get());
	if (Car)
	{
		Car->ApplyDriver(DriverIndex, IronStory::LoadRoster().Rank(DriverIndex));
	}
	if (IsMissionMode())
	{
		// No shop in the campaign: the car comes with what the story has issued so far.
		Upgrades = IronMissions::LoadoutFor(MissionIndex, IronStory::LoadProgress().TotalStars());
		if (Car)
		{
			Car->ApplyUpgrades(Upgrades);
		}
		Director = GetWorld()->SpawnActor<AIronMissionDirector>();
		if (Director)
		{
			Director->Begin(MissionIndex, PlayerPawn.Get());
		}
	}
	else
	{
		StartNextWave();
		PlayerBark(static_cast<int32>(IronCrew::Bark::Deploy));
	}
	GetWorldTimerManager().SetTimer(MatchStateTimer, this, &AIronSiegeGameMode::CheckMatchState, 0.25f, true);
}

void AIronSiegeGameMode::PlayerBark(int32 Kind) const
{
	const APawn* Player = PlayerPawn.Get();
	if (APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->Bark(Kind);
		}
	}
}

FString AIronSiegeGameMode::GetBossName() const
{
	if (BossKind != IronMissions::BossNone)
	{
		const IronMissions::BossDef& Def = IronMissions::GetBoss(BossKind);
		return IronText::Str(ANSI_TO_TCHAR(Def.NameKey), ANSI_TO_TCHAR(Def.Name));
	}
	return IronText::Str(TEXT("HudBoss"), TEXT("JUGGERNAUT"));
}

APawn* AIronSiegeGameMode::SpawnEnemyOfKind(int32 Kind, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (Kind >= 2)
	{
		return SpawnSpecialist(Kind - 2, Location, Rotation);
	}
	const TSubclassOf<APawn> Class = Kind == 1 && FastEnemyVehicleClass ? FastEnemyVehicleClass : EnemyVehicleClass;
	if (!Class)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Enemy = World->SpawnActor<APawn>(Class, Location, Rotation, Params);
	if (Enemy)
	{
		ApplyDifficulty(Enemy);
		LiveEnemies.Add(Enemy);
	}
	return Enemy;
}

APawn* AIronSiegeGameMode::SpawnMissionBoss(int32 Boss, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	const IronMissions::BossDef& Def = IronMissions::GetBoss(Boss);
	// Raven drives the lancers' black sedan (twin rails); the Iron Baron the Juggernaut truck.
	const bool bRaven = Boss == IronMissions::BossRaven;
	const TSubclassOf<APawn> Chassis = bRaven ? (LancerVehicleClass ? LancerVehicleClass : FastEnemyVehicleClass) : BossEnemyVehicleClass;
	if (!World || !Chassis || !Chassis->IsChildOf(AWarVehiclePawn::StaticClass()))
	{
		return nullptr;
	}
	AWarVehiclePawn* Car = World->SpawnActorDeferred<AWarVehiclePawn>(Chassis, FTransform(Rotation, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Car)
	{
		return nullptr;
	}
	Car->DurabilityMultiplier = Def.Durability;
	Car->Tags.Add(TEXT("Boss"));
	Car->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Car->AIControllerClass = AIronSiegeAIController::StaticClass();
	if (bRaven)
	{
		Car->Kit = EIronKit::Lancer;
		Car->KitVariant = 1;
		Car->bAIRailgun = true;
		Car->bAIFiresRockets = true;
	}
	Car->FinishSpawning(FTransform(Rotation, Location));
	ApplyDifficulty(Car);
	if (bRaven && Car->GetRailgun())
	{
		const UIronSiegeUserSettings* UserSettings = UIronSiegeUserSettings::Get();
		Car->GetRailgun()->bUnlocked = true;
		Car->GetRailgun()->DamageMultiplier = 0.6f * IronSettings::EnemyDamageMultiplier(UserSettings ? UserSettings->Difficulty : 1);
	}
	if (AIronSiegeAIController* AI = Cast<AIronSiegeAIController>(Car->GetController()))
	{
		AI->StoppingDistance = bRaven ? 2600.f : 1200.f;
	}
	LiveEnemies.Add(Car);
	BossPawn = Car;
	BossKind = Boss;
	NotifyPlayer(FString::Printf(TEXT("%s!"), *GetBossName()), FLinearColor(1.f, 0.3f, 0.2f));
	PlayerBark(static_cast<int32>(IronCrew::Bark::Boss));
	return Car;
}

AWarVehiclePawn* AIronSiegeGameMode::SpawnTruck(bool bFriendly, const FVector& Location, const FRotator& Rotation, float Durability)
{
	UWorld* World = GetWorld();
	// The Heavy class's box truck, without its war kit and guns.
	const TSubclassOf<APawn> Chassis = VehicleClasses.IsValidIndex(2) ? VehicleClasses[2] : TSubclassOf<APawn>();
	if (!World || !Chassis || !Chassis->IsChildOf(AWarVehiclePawn::StaticClass()))
	{
		return nullptr;
	}
	AWarVehiclePawn* Truck = World->SpawnActorDeferred<AWarVehiclePawn>(Chassis, FTransform(Rotation, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Truck)
	{
		return nullptr;
	}
	Truck->Kit = bFriendly ? EIronKit::None : EIronKit::Brute;
	Truck->KitVariant = 1;
	Truck->bUnarmed = true;
	Truck->bPlayerSide = bFriendly;
	Truck->DurabilityMultiplier = Durability;
	Truck->Tags.Add(TEXT("Truck"));
	Truck->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Truck->AIControllerClass = AIronSiegeAIController::StaticClass();
	Truck->FinishSpawning(FTransform(Rotation, Location));
	if (!bFriendly)
	{
		LiveEnemies.Add(Truck);
	}
	return Truck;
}

void AIronSiegeGameMode::EndMission(bool bWon)
{
	if (bGameOver)
	{
		return;
	}
	bMissionWon = bWon;
	if (bWon)
	{
		// The fight is over: the Legion's drivers give up the chase and the player's car is safe.
		if (const IIronVehicle* Vehicle = Cast<IIronVehicle>(PlayerPawn.Get()))
		{
			if (UVehicleHealthComponent* Health = Vehicle->GetHealthComponent())
			{
				Health->bInvulnerable = true;
			}
		}
		for (const TWeakObjectPtr<APawn>& Enemy : LiveEnemies)
		{
			IIronVehicle* Vehicle = Cast<IIronVehicle>(Enemy.Get());
			AController* Driver = Enemy.IsValid() ? Enemy->GetController() : nullptr;
			if (Vehicle && Driver)
			{
				Driver->UnPossess();
				Driver->Destroy();
				Vehicle->MoveForward(0.f);
				Vehicle->Steer(0.f);
				Vehicle->SetHandbrake(true);
			}
		}
	}
	EndMatch();
}

void AIronSiegeGameMode::SpawnEnemies(APawn* AroundPawn)
{
	SpawnEnemyGroup(AroundPawn, EnemyCount);
}

void AIronSiegeGameMode::SpawnEnemyGroup(APawn* AroundPawn, int32 Count)
{
	if (!EnemyVehicleClass || !AroundPawn)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Origin = AroundPawn->GetActorLocation();
	const TArray<FVector> Points = PickSpawnPoints(Origin, 1800.f, EnemySpawnRadius * 1.6f, Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector SpawnLocation = Points[i];
		// Face the player on arrival so a fresh wave closes in instead of first driving away.
		const FRotator SpawnRotation = (Origin - SpawnLocation).GetSafeNormal2D().Rotation();
		// Slots in order: raiders, then hunters, stormers and lancers, the rest plain buggies.
		const int32 Raiders = FastEnemyVehicleClass ? IronWaves::FastEnemiesForWave(Wave, Count) : 0;
		const int32 Hunters = IronWaves::HuntersForWave(Wave, Count);
		const int32 Stormers = IronWaves::StormersForWave(Wave, Count);
		const int32 Lancers = IronWaves::LancersForWave(Wave, Count);
		const bool bRaider = i < Raiders;
		int32 Special = -1;
		if (!bRaider && i < Raiders + Hunters) Special = 0;
		else if (!bRaider && i < Raiders + Hunters + Stormers) Special = 1;
		else if (!bRaider && i < Raiders + Hunters + Stormers + Lancers) Special = 2;
		APawn* Enemy = SpawnEnemyOfKind(Special >= 0 ? Special + 2 : (bRaider ? 1 : 0), SpawnLocation, SpawnRotation);
		static const TCHAR* SpecialNames[] = { TEXT("hunter"), TEXT("stormer"), TEXT("lancer") };
		UE_LOG(LogTemp, Log, TEXT("IronSiege: wave %d spawned %s %d/%d at %s (controller: %s)"), Wave, bRaider ? TEXT("raider") : (Special >= 0 ? SpecialNames[Special] : TEXT("enemy")), i + 1, Count, *SpawnLocation.ToString(), Enemy && Enemy->GetController() ? *Enemy->GetController()->GetName() : TEXT("NONE"));
	}
}

APawn* AIronSiegeGameMode::SpawnSpecialist(int32 Kind, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	Kind = FMath::Clamp(Kind, 0, 2);
	TSubclassOf<APawn> Chassis = EnemyVehicleClass;
	if (Kind == 0 && HunterVehicleClass) Chassis = HunterVehicleClass;
	if (Kind == 2) Chassis = LancerVehicleClass ? LancerVehicleClass : FastEnemyVehicleClass;
	if (!World || !Chassis || !Chassis->IsChildOf(AWarVehiclePawn::StaticClass()))
	{
		return nullptr;
	}
	// Deferred, so the kit, the role and the AI driver are set before BeginPlay bolts the kit on.
	AWarVehiclePawn* Car = World->SpawnActorDeferred<AWarVehiclePawn>(Chassis, FTransform(Rotation, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Car)
	{
		return nullptr;
	}
	static const EIronKit Kits[] = { EIronKit::Hunter, EIronKit::Stormer, EIronKit::Lancer };
	static const TCHAR* RoleTags[] = { TEXT("Hunter"), TEXT("Stormer"), TEXT("Lancer") };
	Car->Kit = Kits[Kind];
	Car->KitVariant = 1;
	Car->bAIFiresRockets = false;
	Car->bAIGuidedMissiles = Kind == 0;
	Car->bAITesla = Kind == 1;
	Car->bAIRailgun = Kind == 2;
	Car->Tags.Add(RoleTags[Kind]);
	// Player-class Blueprints (the lancer's sedan) have no AI driver set up; give them one.
	Car->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Car->AIControllerClass = AIronSiegeAIController::StaticClass();
	Car->FinishSpawning(FTransform(Rotation, Location));
	ApplyDifficulty(Car);
	const UIronSiegeUserSettings* UserSettings = UIronSiegeUserSettings::Get();
	const float EnemyScale = IronSettings::EnemyDamageMultiplier(UserSettings ? UserSettings->Difficulty : 1);
	if (Kind == 0)
	{
		if (URocketLauncherComponent* Launcher = Car->GetSecondaryWeaponComponent())
		{
			// Guided means it will usually connect, so each missile hits softer than a boss rocket and
			// turns lazier than the player's (a hard swerve up close can still shake it).
			Launcher->DamageMultiplier *= 0.6f;
			Launcher->GuidedTurnScale = 0.45f;
		}
	}
	else if (Kind == 1 && Car->GetTesla())
	{
		Car->GetTesla()->bUnlocked = true;
		Car->GetTesla()->DamageMultiplier = 0.7f * EnemyScale;
	}
	else if (Kind == 2 && Car->GetRailgun())
	{
		Car->GetRailgun()->bUnlocked = true;
		Car->GetRailgun()->DamageMultiplier = 0.5f * EnemyScale;
	}
	if (AIronSiegeAIController* AI = Cast<AIronSiegeAIController>(Car->GetController()))
	{
		static const float Standoff[] = { 3200.f, 500.f, 4000.f };
		AI->StoppingDistance = Standoff[Kind];
	}
	LiveEnemies.Add(Car);
	return Car;
}

void AIronSiegeGameMode::DebugSpawnSpecialist(int32 Kind, float Distance)
{
	APawn* Player = PlayerPawn.Get();
	if (!Player)
	{
		return;
	}
	// Toward the middle of the map (the world origin): the player starts sit near the corners, and
	// "straight ahead" can be the far side of a boundary wall.
	const FVector ToCentre = (-Player->GetActorLocation()).GetSafeNormal2D();
	const FVector At = Player->GetActorLocation() + (ToCentre.IsNearlyZero() ? Player->GetActorForwardVector() : ToCentre) * FMath::Abs(Distance) + FVector(0.f, 0.f, 60.f);
	const APawn* Car = SpawnSpecialist(Kind, At, (Player->GetActorLocation() - At).GetSafeNormal2D().Rotation());
	UE_LOG(LogTemp, Log, TEXT("IronSiege: debug specialist %d %s at %s"), Kind, Car ? *Car->GetName() : TEXT("FAILED"), *At.ToString());
}

void AIronSiegeGameMode::StartNextWave()
{
	APawn* Player = PlayerPawn.Get();
	if (bGameOver || !Player)
	{
		return;
	}
	++Wave;
	bWaveCleared = false;
	IronWaves::WaveTuning Tuning;
	Tuning.FirstWaveEnemies = EnemyCount;
	Tuning.EnemiesAddedPerWave = EnemiesAddedPerWave;
	Tuning.MaxEnemiesPerWave = MaxEnemiesPerWave;
	SpawnEnemyGroup(Player, IronWaves::EnemiesForWave(Wave, Tuning));
	if (IronWaves::IsBossWave(Wave) && BossEnemyVehicleClass)
	{
		const TArray<FVector> Points = PickSpawnPoints(Player->GetActorLocation(), 3000.f, EnemySpawnRadius * 1.8f, 1);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Facing = (Player->GetActorLocation() - Points[0]).GetSafeNormal2D().Rotation();
		if (APawn* Boss = GetWorld()->SpawnActor<APawn>(BossEnemyVehicleClass, Points[0], Facing, Params))
		{
			ApplyDifficulty(Boss);
			LiveEnemies.Add(Boss);
			BossPawn = Boss;
			BossKind = IronMissions::BossNone;
			NotifyPlayer(IronText::Str(TEXT("NoticeBoss"), TEXT("JUGGERNAUT INBOUND")), FLinearColor(1.f, 0.3f, 0.2f));
			PlayerBark(static_cast<int32>(IronCrew::Bark::Boss));
		}
	}

	if (USoundBase* Siren = WaveStartSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Siren);
	}
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->ShowNotice(FString::Format(*IronText::Str(TEXT("NoticeWave"), TEXT("WAVE {0} INCOMING")), { Wave }), FLinearColor(1.f, 0.8f, 0.3f));
		}
	}
	if (const int32 Hunters = IronWaves::HuntersForWave(Wave, IronWaves::EnemiesForWave(Wave, Tuning)); Hunters > 0)
	{
		NotifyPlayer(FString::Printf(TEXT("%s: %d"), *IronText::Str(TEXT("NoticeHunters"), TEXT("MISSILE HUNTERS - USE FLARES")), Hunters), FLinearColor(1.f, 0.45f, 0.2f));
	}
	const int32 WaveSize = IronWaves::EnemiesForWave(Wave, Tuning);
	if (const int32 Stormers = IronWaves::StormersForWave(Wave, WaveSize); Stormers > 0)
	{
		NotifyPlayer(FString::Printf(TEXT("%s: %d"), *IronText::Str(TEXT("NoticeStormers"), TEXT("TESLA STORMERS - KEEP YOUR DISTANCE")), Stormers), FLinearColor(0.5f, 0.75f, 1.f));
	}
	if (const int32 Lancers = IronWaves::LancersForWave(Wave, WaveSize); Lancers > 0)
	{
		NotifyPlayer(FString::Printf(TEXT("%s: %d"), *IronText::Str(TEXT("NoticeLancers"), TEXT("RAILGUN LANCERS - SWERVE WHEN THEY CHARGE")), Lancers), FLinearColor(0.4f, 0.95f, 1.f));
	}
}

bool AIronSiegeGameMode::IsVehicleDestroyed(const APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return true;
	}
	const IIronVehicle* Vehicle = Cast<IIronVehicle>(Pawn);
	const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
	return Health && Health->IsDestroyed();
}

void AIronSiegeGameMode::CheckMatchState()
{
	if (bGameOver)
	{
		return;
	}
	if (IsVehicleDestroyed(PlayerPawn.Get()))
	{
		PlayerBark(static_cast<int32>(IronCrew::Bark::Defeat));
		if (Director)
		{
			Director->NotifyPlayerDestroyed();
		}
		EndMatch();
		return;
	}

	for (int32 i = LiveEnemies.Num() - 1; i >= 0; --i)
	{
		APawn* Enemy = LiveEnemies[i].Get();
		if (IsVehicleDestroyed(Enemy))
		{
			// Only count real kills - an enemy that vanished some other way (fell out of the
			// world, etc.) just leaves the wave.
			if (IsValid(Enemy))
			{
				++Kills;
				const bool bRaider = FastEnemyVehicleClass && Enemy->GetClass()->IsChildOf(FastEnemyVehicleClass);
				const bool bBoss = BossEnemyVehicleClass && Enemy->GetClass()->IsChildOf(BossEnemyVehicleClass);
				const IronScore::ScoreTuning ScoreTuning;
				int32 Base = bRaider ? ScoreTuning.RaiderKill : ScoreTuning.EnemyKill;
				if (Enemy->ActorHasTag(TEXT("Hunter"))) Base = ScoreTuning.HunterKill;
				if (Enemy->ActorHasTag(TEXT("Stormer"))) Base = ScoreTuning.StormerKill;
				if (Enemy->ActorHasTag(TEXT("Lancer"))) Base = ScoreTuning.LancerKill;
				if (bBoss) Base = ScoreTuning.BossKill;
				const int32 Points = Score.AddKillWorth(Base, GetWorld()->GetTimeSeconds());
				Upgrades.Credits += Points;
				NotifyPlayer(Score.Combo > 1 ? FString::Printf(TEXT("+%d   x%d %s"), Points, Score.Combo, *IronText::Str(TEXT("HudCombo"), TEXT("COMBO"))) : FString::Printf(TEXT("+%d"), Points), FLinearColor(1.f, 0.9f, 0.4f));
				Enemy->SetLifeSpan(10.f); // Leave the wreck on the field for a while.
				PlayerBark(static_cast<int32>(Score.Combo >= 3 ? IronCrew::Bark::Streak : IronCrew::Bark::Kill));
				if (Director)
				{
					Director->NotifyEnemyKilled(Enemy);
				}
			}
			LiveEnemies.RemoveAtSwap(i);
		}
	}

	if (!IsMissionMode() && LiveEnemies.Num() == 0 && !bWaveCleared)
	{
		bWaveCleared = true;
		SpawnSupplies(Wave);
		const int32 Bonus = Score.AddWaveClear(Wave);
		Upgrades.Credits += Bonus;
		NotifyPlayer(FString::Format(*IronText::Str(TEXT("NoticeCleared"), TEXT("WAVE {0} CLEARED   +{1}")), { Wave, Bonus }), FLinearColor(0.45f, 1.f, 0.5f));
		OpenShop();
	}
}

void AIronSiegeGameMode::EndMatch()
{
	bGameOver = true;
	GetWorldTimerManager().ClearTimer(MatchStateTimer);
	GetWorldTimerManager().ClearTimer(NextWaveTimer);
	GetWorldTimerManager().ClearTimer(ShopTimer);
	bShopOpen = false;

	BestWave = FMath::Max(BestWave, Wave);
	BestKills = FMath::Max(BestKills, Kills);
	BestScore = FMath::Max(BestScore, Score.Score);
	if (GConfig)
	{
		GConfig->SetInt(GRecordsSection, TEXT("BestWave"), BestWave, GGameIni);
		GConfig->SetInt(GRecordsSection, TEXT("BestKills"), BestKills, GGameIni);
		GConfig->SetInt(GRecordsSection, TEXT("BestScore"), BestScore, GGameIni);
		GConfig->Flush(false, GGameIni);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: match over - wave %d, kills %d"), Wave, Kills);
	AwardDriverXp();
}

void AIronSiegeGameMode::AwardDriverXp()
{
	if (PlayerDriver < 0)
	{
		return;
	}
	const int32 Stars = IsMissionMode() && Director ? Director->GetStars() : 0;
	XpGained = IronRanks::MatchXp(Kills, Wave, IsMissionMode(), bMissionWon, Stars);
	IronRanks::Roster Roster = IronStory::LoadRoster();
	RankReached = Roster.Award(PlayerDriver, XpGained);
	IronStory::SaveRoster(Roster);
	UE_LOG(LogTemp, Log, TEXT("IronSiege: driver %d earned %d xp (total %d, rank %d%s)"), PlayerDriver, XpGained, Roster.Xp[PlayerDriver],
		Roster.Rank(PlayerDriver), RankReached > 0 ? TEXT(", rank up") : TEXT(""));
}

float AIronSiegeGameMode::GetNextWaveCountdown() const
{
	return GetWorldTimerManager().IsTimerActive(NextWaveTimer) ? GetWorldTimerManager().GetTimerRemaining(NextWaveTimer) : -1.f;
}

TArray<FVector> AIronSiegeGameMode::PickSpawnPoints(const FVector& Origin, float MinDist, float MaxDist, int32 Count) const
{
	// On a street map, use the road network (never inside a building), shuffled.
	TArray<FVector> Points;
	for (TActorIterator<AIronCityStreet> It(GetWorld()); It; ++It)
	{
		for (const FVector& P : It->GetRoadSpawnPoints())
		{
			const float D = FVector::Dist2D(P, Origin);
			if (D >= MinDist && D <= MaxDist)
			{
				Points.Add(P);
			}
		}
	}
	for (int32 i = Points.Num() - 1; i > 0; --i)
	{
		Points.Swap(i, FMath::RandRange(0, i));
	}
	// Open maps (or too few road points): a ring around the origin, skipping spots where the arena's
	// rocks, ramps and containers stand - a car dropped onto a boulder hangs there, wheels in the air.
	auto IsClear = [this](const FVector& At)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeSpawnClear));
		TArray<FOverlapResult> Hits;
		GetWorld()->OverlapMultiByObjectType(Hits, At + FVector(0.f, 0.f, 120.f), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(450.f), Params);
		for (const FOverlapResult& Hit : Hits)
		{
			if (Hit.GetActor() && Hit.GetActor()->IsA<AIronArena>())
			{
				return false;
			}
		}
		return true;
	};
	// ...and well inside the walls: a player start sits near a corner, so part of a ring round it
	// reaches the wall (or, round a mission's truck by the perimeter, past it). Nine metres clear.
	float Limit = 0.f;
	for (TActorIterator<AIronArena> It(GetWorld()); It; ++It)
	{
		Limit = FMath::Max(It->GetHalfExtent() - 900.f, 0.f);
	}
	auto IsInside = [Limit](const FVector& At)
	{
		return Limit <= 0.f || (FMath::Abs(At.X) <= Limit && FMath::Abs(At.Y) <= Limit);
	};
	for (int32 i = Points.Num(); i < Count; ++i)
	{
		FVector Best = FVector::ZeroVector;
		bool bFound = false;
		for (int32 Try = 0; Try < 16 && !bFound; ++Try)
		{
			const float Angle = FMath::DegreesToRadians((360.f / FMath::Max(Count, 1)) * i + FMath::FRandRange(-20.f, 20.f) + Try * 23.f);
			const float Dist = FMath::FRandRange(FMath::Max(MinDist, MaxDist * 0.45f), MaxDist * 0.65f);
			Best = Origin + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 100.f);
			bFound = IsInside(Best) && IsClear(Best);
		}
		if (!bFound && Limit > 0.f)
		{
			Best.X = FMath::Clamp(Best.X, -Limit, Limit);
			Best.Y = FMath::Clamp(Best.Y, -Limit, Limit);
		}
		Points.Add(Best);
	}
	Points.SetNum(Count);
	return Points;
}

void AIronSiegeGameMode::SpawnSupplies(int32 ClearedWave)
{
	const IronWaves::SupplyDrop Drop = IronWaves::SuppliesAfterWave(ClearedWave);
	DropSupplies(Drop.RepairKits, Drop.AmmoCrates);
}

void AIronSiegeGameMode::DropSupplies(int32 RepairKits, int32 AmmoCrates)
{
	APawn* Player = PlayerPawn.Get();
	UWorld* World = GetWorld();
	const int32 Total = RepairKits + AmmoCrates;
	if (!SupplyCrateClass || !Player || !World || Total <= 0)
	{
		return;
	}
	const TArray<FVector> Points = PickSpawnPoints(Player->GetActorLocation(), 700.f, 3500.f, Total);
	for (int32 i = 0; i < Total; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		AIronSupplyCrate* Crate = World->SpawnActor<AIronSupplyCrate>(SupplyCrateClass, Points[i], FRotator::ZeroRotator, Params);
		if (Crate)
		{
			Crate->SupplyType = i < RepairKits ? EIronSupplyType::Repair : EIronSupplyType::Ammo;
			Crate->FinishSpawning(FTransform(Points[i]));
		}
	}
}

int32 AIronSiegeGameMode::GetActiveCombo() const
{
	return GetWorld() ? Score.ActiveCombo(GetWorld()->GetTimeSeconds()) : 0;
}

void AIronSiegeGameMode::NotifyPlayer(const FString& Text, const FLinearColor& Color) const
{
	const APawn* Player = PlayerPawn.Get();
	if (APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->ShowNotice(Text, Color);
		}
	}
}

void AIronSiegeGameMode::OpenShop()
{
	bShopOpen = true;
	GetWorldTimerManager().SetTimer(ShopTimer, this, &AIronSiegeGameMode::CloseShop, ShopSeconds, false);
}

void AIronSiegeGameMode::CloseShop()
{
	if (!bShopOpen)
	{
		return;
	}
	bShopOpen = false;
	GetWorldTimerManager().ClearTimer(ShopTimer);
	GetWorldTimerManager().SetTimer(NextWaveTimer, this, &AIronSiegeGameMode::StartNextWave, WaveBreakSeconds, false);
}

float AIronSiegeGameMode::GetShopTimeLeft() const
{
	return bShopOpen ? GetWorldTimerManager().GetTimerRemaining(ShopTimer) : 0.f;
}

bool AIronSiegeGameMode::BuyUpgrade(int32 Index)
{
	if (!bShopOpen || Index < 0 || Index >= IronUpgrades::Count)
	{
		return false;
	}
	const IronUpgrades::Upgrade Item = static_cast<IronUpgrades::Upgrade>(Index);
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(PlayerPawn.Get());
	if (!Car || !Upgrades.Buy(Item))
	{
		return false;
	}
	if (Item == IronUpgrades::Upgrade::Repair)
	{
		if (UVehicleHealthComponent* Health = Car->GetHealthComponent())
		{
			Health->Repair(100000.f, 100000.f);
		}
	}
	Car->ApplyUpgrades(Upgrades);
	NotifyPlayer(FString::Printf(TEXT("%s  %s"), *IronText::Name(TEXT("Up"), ANSI_TO_TCHAR(IronUpgrades::Get(Item).Name)),
		IronUpgrades::Get(Item).MaxLevel > 0 ? *FString::Printf(TEXT("%s %d"), *IronText::Str(TEXT("NoticeLv"), TEXT("LV")), Upgrades.Level(Item)) : *IronText::Str(TEXT("NoticeDone"), TEXT("DONE"))), FLinearColor(0.5f, 0.85f, 1.f));
	return true;
}

void AIronSiegeGameMode::ApplyDifficulty(APawn* Enemy) const
{
	const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	const int32 Difficulty = S ? S->Difficulty : 1;
	if (const IIronVehicle* Vehicle = Cast<IIronVehicle>(Enemy))
	{
		const float DamageScale = IronSettings::EnemyDamageMultiplier(Difficulty);
		if (UMachineGunComponent* Gun = Vehicle->GetPrimaryWeaponComponent()) Gun->DamageMultiplier = DamageScale;
		if (URocketLauncherComponent* Launcher = Vehicle->GetSecondaryWeaponComponent()) Launcher->DamageMultiplier = DamageScale;
	}
	if (AIronSiegeAIController* AI = Cast<AIronSiegeAIController>(Enemy->GetController()))
	{
		AI->AimSpreadDeg = IronSettings::EnemyAimSpreadDeg(Difficulty);
	}
}
