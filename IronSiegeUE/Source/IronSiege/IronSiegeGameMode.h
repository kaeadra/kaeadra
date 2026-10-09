#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ScoreRules.h"
#include "UpgradeRules.h"
#include "IronSiegeGameMode.generated.h"

// No DefaultPawnClass is set at construction on purpose - the player picks a class on the
// AIronSiegeHUD/AIronSiegePlayerController vehicle-select screen first, and that choice calls
// SpawnSelectedVehicle() to assign DefaultPawnClass and spawn the pawn. Until then GameModeBase's
// normal auto-restart finds a null DefaultPawnClass and simply spawns nothing, which is exactly
// the "no pawn yet, show the select screen" state we want.
//
// Match flow once the player has spawned: enemies come in waves. Clearing a wave starts a short
// break, then the next wave arrives with more enemies. The match ends when the player's vehicle
// is destroyed; the best wave/kill count is kept in Game.ini across sessions.
//
// Campaign: a level opened with ?Mission=N plays that story mission instead (MissionRules.h). The
// waves and the shop are off; an AIronMissionDirector runs the mission's stages and asks this class
// for its enemies, and the match ends when the director says the mission is won or lost.
class AIronSupplyCrate;
class AIronMissionDirector;
class AWarVehiclePawn;

UCLASS()
class IRONSIEGE_API AIronSiegeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIronSiegeGameMode();

	// Index-aligned with EIronVehicleClass (Scout=0, Assault=1, Heavy=2, Artillery=3).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Vehicles")
	TArray<TSubclassOf<APawn>> VehicleClasses;

	UFUNCTION(BlueprintCallable, Category = "IronSiege|Vehicles")
	void SpawnSelectedVehicle(AController* Controller, int32 ClassIndex, int32 DriverIndex = 0);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// ---- Campaign
	bool IsMissionMode() const { return MissionIndex >= 0; }
	int32 GetMissionIndex() const { return MissionIndex; }
	AIronMissionDirector* GetDirector() const { return Director; }
	bool WasMissionWon() const { return bMissionWon; }

	// Ends the match with the mission won or lost (the director calls this).
	void EndMission(bool bWon);

	// Driver experience from the match just ended (RankRules.h): who drove, what they earned, and
	// the rank reached if it went up (0 if not). For the result screens.
	int32 GetPlayerDriver() const { return PlayerDriver; }
	int32 GetXpGained() const { return XpGained; }
	int32 GetRankReached() const { return RankReached; }

	// One enemy of a kind: 0 buggy, 1 raider, 2 missile hunter, 3 tesla stormer, 4 railgun lancer.
	APawn* SpawnEnemyOfKind(int32 Kind, const FVector& Location, const FRotator& Rotation);

	// A mission boss (IronMissions::BossRaven / BossBaron), shown on the boss bar.
	APawn* SpawnMissionBoss(int32 Boss, const FVector& Location, const FRotator& Rotation);

	// A cargo truck with an AI driver and no guns: ours to escort, or the Legion's to stop.
	AWarVehiclePawn* SpawnTruck(bool bFriendly, const FVector& Location, const FRotator& Rotation, float Durability);

	// Spawn candidates: road points MinDist..MaxDist from Origin (shuffled) on street maps,
	// otherwise Count points on a ring around Origin, inside the arena's walls.
	TArray<FVector> PickSpawnPoints(const FVector& Origin, float MinDist, float MaxDist, int32 Count) const;

	// Name on the boss bar (the Juggernaut of the survival waves, or a mission's boss).
	FString GetBossName() const;

	// Repair kits and ammo crates dropped near the player (after a wave, between a mission's stages).
	void DropSupplies(int32 RepairKits, int32 AmmoCrates);

	// AI-controlled hostile vehicles, spawned once the player's own vehicle exists (see
	// SpawnSelectedVehicle). The class should set AIControllerClass = AIronSiegeAIController and
	// AutoPossessAI = PlacedInWorldOrSpawned so SpawnActor alone hands it an AI controller.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	TSubclassOf<APawn> EnemyVehicleClass;

	// Juggernaut boss added to every fifth wave (IronWaves::IsBossWave).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	TSubclassOf<APawn> BossEnemyVehicleClass;

	// Railgun lancer chassis (IronWaves::LancersForWave); a player car Blueprint is fine - the AI
	// driver is assigned at spawn. Falls back to FastEnemyVehicleClass.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	TSubclassOf<APawn> LancerVehicleClass;

	// Guided-missile hunter chassis (IronWaves::HuntersForWave); falls back to EnemyVehicleClass.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	TSubclassOf<APawn> HunterVehicleClass;

	// Fast, lightly armoured "raider" enemy mixed in from wave 3 (IronWaves::FastEnemiesForWave).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	TSubclassOf<APawn> FastEnemyVehicleClass;

	// Enemies in the first wave.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	int32 EnemyCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float EnemySpawnRadius = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Waves")
	int32 EnemiesAddedPerWave = 1;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Waves")
	int32 MaxEnemiesPerWave = 8;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Waves")
	float WaveBreakSeconds = 5.f;

	UFUNCTION(BlueprintCallable, Category = "IronSiege|AI")
	void SpawnEnemies(APawn* AroundPawn);

	// Supply crates dropped on the road near the player each time a wave is cleared
	// (counts from IronWaves::SuppliesAfterWave).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Waves")
	TSubclassOf<AIronSupplyCrate> SupplyCrateClass;

	// Played (2D) as each wave begins.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Waves")
	TSoftObjectPtr<class USoundBase> WaveStartSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_WaveSiren.S_WaveSiren")));

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	int32 GetWave() const { return Wave; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Score")
	int32 GetScore() const { return Score.Score; }

	// ---- Upgrade shop (open between waves; see IronUpgrades) ----
	UFUNCTION(BlueprintPure, Category = "IronSiege|Shop")
	bool IsShopOpen() const { return bShopOpen; }

	// Seconds before the shop closes on its own.
	UFUNCTION(BlueprintPure, Category = "IronSiege|Shop")
	float GetShopTimeLeft() const;

	const IronUpgrades::Loadout& GetUpgrades() const { return Upgrades; }

	// Buys upgrade Index (IronUpgrades::Upgrade) for the player and applies it. False if unaffordable/maxed.
	bool BuyUpgrade(int32 Index);

	// Leaves the shop; the next wave starts after WaveBreakSeconds.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Shop")
	void CloseShop();

	// How long the shop stays open if the player doesn't leave it.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Shop")
	float ShopSeconds = 30.f;

	// Testing: the next wave to start will be wave N; and free credits.
	void DebugSetNextWave(int32 N) { Wave = FMath::Max(N - 1, 0); }
	void DebugAddCredits(int32 Amount) { Upgrades.Credits += Amount; }

	// Testing: a missile hunter right now, Distance cm from the player toward the middle of the map;
	// and any specialist (0 hunter, 1 tesla stormer, 2 railgun lancer).
	void DebugSpawnHunter(float Distance) { DebugSpawnSpecialist(0, Distance); }
	void DebugSpawnSpecialist(int32 Kind, float Distance);

	// Living boss, if any (for the HUD's boss bar).
	APawn* GetBoss() const { return BossPawn.Get(); }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Score")
	int32 GetBestScore() const { return BestScore; }

	// Current kill-combo multiplier (0 once the combo window has lapsed).
	UFUNCTION(BlueprintPure, Category = "IronSiege|Score")
	int32 GetActiveCombo() const;

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	int32 GetEnemiesAlive() const { return LiveEnemies.Num(); }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	bool IsGameOver() const { return bGameOver; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	int32 GetBestWave() const { return BestWave; }

	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	int32 GetBestKills() const { return BestKills; }

	// Seconds until the next wave arrives, or -1 while a wave is in progress.
	UFUNCTION(BlueprintPure, Category = "IronSiege|Waves")
	float GetNextWaveCountdown() const;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnEnemyGroup(APawn* AroundPawn, int32 Count);
	// Specialist enemies, refitted at spawn: 0 missile hunter (stands off, homing missiles the player
	// flares or out-turns), 1 tesla stormer (rushes in, chain lightning that stalls engines),
	// 2 railgun lancer (hangs back, charges, fires through everything in line).
	APawn* SpawnSpecialist(int32 Kind, const FVector& Location, const FRotator& Rotation);
	void SpawnSupplies(int32 ClearedWave);
	void StartNextWave();
	void CheckMatchState();
	void EndMatch();
	static bool IsVehicleDestroyed(const APawn* Pawn);

	TArray<TWeakObjectPtr<APawn>> LiveEnemies;
	TWeakObjectPtr<APawn> PlayerPawn;
	int32 Wave = 0;
	int32 Kills = 0;
	int32 BestWave = 0;
	int32 BestKills = 0;
	int32 BestScore = 0;
	IronScore::ScoreState Score;
	IronUpgrades::Loadout Upgrades;
	bool bShopOpen = false;
	bool bWaveCleared = false;
	FTimerHandle ShopTimer;
	TWeakObjectPtr<APawn> BossPawn;
	void OpenShop();
	void ApplyDifficulty(APawn* Enemy) const;
	void NotifyPlayer(const FString& Text, const FLinearColor& Color) const;
	bool bGameOver = false;
	// Campaign: which mission this level was opened for (-1: survival waves), its director, and how
	// it ended.
	int32 MissionIndex = -1;
	int32 BossKind = 0;
	bool bMissionWon = false;
	UPROPERTY()
	TObjectPtr<AIronMissionDirector> Director;
	// The driver's one-liners on the radio (IronCrew::Bark).
	void PlayerBark(int32 Kind) const;
	// Experience for the driver once the match is over (EndMatch).
	void AwardDriverXp();
	// Gives a freshly spawned boss its phases and signature attacks (UIronBossComponent).
	void ArmBoss(APawn* Boss, int32 MissionBoss);
	int32 PlayerDriver = -1;
	int32 XpGained = 0;
	int32 RankReached = 0;
	FTimerHandle MatchStateTimer;
	FTimerHandle NextWaveTimer;
};
