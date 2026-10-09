#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "MissionRules.h"
#include "IronMissionDirector.generated.h"

class AIronMissionTarget;
class AIronCaptureZone;
class AIronSiegeGameMode;
class AIronSiegeHUD;
class AWarVehiclePawn;

// Something the HUD points the player at: a place to go, a structure, a truck.
struct FIronMissionMarker
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	float Fraction = -1.f;  // Health or capture progress, 0..1; negative: no bar.
	bool bFriendly = false; // Ours to protect, rather than theirs to destroy or a place to reach.
	bool bAlert = false;    // In trouble right now: a contested zone, a truck or relay below half.
};

UENUM()
enum class EIronMissionResult : uint8
{
	Running,
	Complete,
	Failed
};

// Plays one campaign mission (MissionRules.h, tested offline) in the level: stands up each stage's
// structures, zones and trucks, brings in its enemies through the game mode, puts the radio lines
// on the HUD, watches the objective and moves on to the next stage - then scores the run, saves the
// progress and tells the game mode the match is over. The game mode spawns one when the level was
// opened with ?Mission=N and the player deploys.
UCLASS()
class IRONSIEGE_API AIronMissionDirector : public AInfo
{
	GENERATED_BODY()

public:
	AIronMissionDirector();

	void Begin(int32 InMissionIndex, APawn* Player);
	virtual void Tick(float DeltaSeconds) override;

	int32 GetMissionIndex() const { return MissionIndex; }
	const IronMissions::Mission& GetMission() const { return IronMissions::Get(MissionIndex); }
	int32 GetStageIndex() const { return StageIndex; }
	const IronMissions::Stage& GetStage() const;
	const IronMissions::StageState& GetStageState() const { return State; }
	EIronMissionResult GetResult() const { return Result; }

	// Result screen: the run's time, stars, trucks lost, and what finishing it opened up.
	float GetElapsed() const { return Elapsed; }
	int32 GetStars() const { return Stars; }
	int32 GetAssetsLost() const { return AssetsLost; }
	float GetEndHealthFraction() const { return EndHealth; }
	bool IsNewBest() const { return bNewBest; }
	int32 GetUnlockedDriver() const { return UnlockedDriver; } // -1: none this time.

	// HUD: the objective line with its count ("Destroy the fuel depots  1/3"), the stage clock
	// (negative when it has none), and what to point at.
	FString GetObjectiveText() const;
	float GetTimeLeft() const { return StageIndex >= 0 ? IronMissions::TimeLeft(GetStage(), State) : -1.f; }
	void GetMarkers(TArray<FIronMissionMarker>& Out) const;

	// From the game mode: an enemy was destroyed; the player's car was.
	void NotifyEnemyKilled(APawn* Enemy);
	void NotifyPlayerDestroyed();

	// Testing: finish the current stage now / lose the mission now.
	void DebugSkipStage() { bDebugSkip = true; }
	void DebugFail() { Fail(); }

private:
	void StartStage(int32 Index);
	void FinishStage();
	void Win();
	void Fail();
	void ClearZones();
	void SpawnConvoy(bool bFriendly);
	void SpawnSquad(const IronMissions::Squad& Squad, bool bGuard);
	void PollObjective();
	AActor* FindAsset() const;

	// A stage spot in the world: scaled to the arena, moved off any prop it lands on (or onto the
	// nearest road in the city), on the ground.
	FVector Resolve(const IronMissions::Spot& Spot, float Clearance) const;
	float GroundZ(const FVector2D& At) const;

	AIronSiegeHUD* Hud() const;
	void Say(const IronMissions::Radio& Line) const;
	void Notice(const TCHAR* Key, const TCHAR* English, const FLinearColor& Color, const FString& Suffix = FString()) const;

	int32 MissionIndex = 0;
	int32 StageIndex = -1;
	IronMissions::StageState State;
	EIronMissionResult Result = EIronMissionResult::Running;
	float Elapsed = 0.f;      // Mission time, the breaks between stages not counted.
	float StageBreak = 0.f;   // Seconds left before the next stage starts.
	int32 PendingStage = -1;
	int32 Stars = 0;
	int32 AssetsLost = 0;
	float EndHealth = 0.f;
	bool bNewBest = false;
	int32 UnlockedDriver = -1;
	bool bDebugSkip = false;
	int32 Spawned = 0;        // Enemies this stage, and how many of them were sent at the asset.
	int32 SentAtAsset = 0;
	TArray<TWeakObjectPtr<APawn>> AssetAttackers; // The ones sent at the trucks / relay (to cap the live count).

	float HalfExtent = 9500.f;
	TArray<FVector> Obstacles;  // (X, Y, radius) of the arena's props.
	TArray<FVector> RoadPoints; // City map: stage spots snap to these.
	TArray<FVector> Points;     // The current stage's spots in the world.

	TWeakObjectPtr<APawn> PlayerPawn;

	UPROPERTY()
	TObjectPtr<AIronSiegeGameMode> GameMode;

	UPROPERTY()
	TArray<TObjectPtr<AIronMissionTarget>> Targets;

	UPROPERTY()
	TArray<TObjectPtr<AIronCaptureZone>> Zones;

	// Convoy trucks, and which of them are already accounted for (home, stopped, lost or away).
	UPROPERTY()
	TArray<TObjectPtr<AWarVehiclePawn>> Trucks;
	TArray<bool> TruckDone;
	TArray<bool> TruckWarned;  // The "badly damaged" notice has been shown for this truck.
	bool bRelayWarned = false;
	bool bFriendlyConvoy = false;
	int32 ZonesTaken = 0;
	int32 TargetsDown = 0;

	TWeakObjectPtr<APawn> Boss;
};
