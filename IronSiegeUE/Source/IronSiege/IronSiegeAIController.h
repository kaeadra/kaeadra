#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "AIRules.h"
#include "RouteRules.h"
#include "TacticsRules.h"
#include "IronSiegeAIController.generated.h"

class IIronVehicle;

// Simple Tick-driven chase-and-shoot AI (no Behavior Tree/Blackboard assets needed): steers and
// throttles toward the nearest player-controlled IIronVehicle pawn, firing the primary weapon
// once roughly facing the target within FireRange. Possesses any IIronVehicle-implementing pawn
// (AWarVehiclePawn, the Chaos-physics war car). Campaign missions can send it after something
// else (a truck, the relay), or have it drive a truck along a route instead of fighting.
UCLASS()
class IRONSIEGE_API AIronSiegeAIController : public AAIController
{
	GENERATED_BODY()

public:
	AIronSiegeAIController();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float FireRange = 3000.f;

	// Cosine of the max angle off-boresight the AI will still fire at (0.7 ~= 45 degrees).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float FireAimDotThreshold = 0.7f;

	// Half-angle of the random aim error cone, in degrees.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float AimSpreadDeg = 5.f;

	// Fires in bursts (BurstSeconds on, BurstPauseSeconds off) rather than a continuous stream.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float BurstSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float BurstPauseSeconds = 2.4f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	float StoppingDistance = 1200.f;

	// Guided-missile hunters: seconds between launches, and the range band they fire in.
	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	float MissileInterval = 4.5f;

	// Obstacle avoidance: three probes (ahead, and angled to each side) decide whether to keep
	// steering at the target or swing away from whatever is in the way.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float AvoidProbeLength = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	float AvoidProbeAngleDeg = 35.f;

	// Campaign: go for this instead of the player (an escorted truck, the relay) for as long as it
	// lasts; then back to the player.
	void SetPreferredTarget(AActor* Target) { PreferredTarget = Target; }
	AActor* GetPreferredTarget() const { return PreferredTarget.Get(); }

	// Campaign trucks: drive these points in order at a steady speed and stop at the last one. The
	// truck does not fight; it only steers round what is in its way.
	void DriveRoute(const TArray<FVector>& Points, float SpeedKph);
	bool HasArrived() const { return bConvoyArrived; }

private:
	IIronVehicle* GetControlledVehicle() const;
	AActor* FindTarget() const;
	void TickConvoy(float DeltaSeconds, IIronVehicle* Vehicle, APawn* MyPawn);

	TWeakObjectPtr<AActor> PreferredTarget;
	TArray<FVector> ConvoyPoints;
	int32 ConvoyIndex = 0;
	float ConvoySpeedKph = 24.f;
	bool bConvoyArrived = false;

	// Returns 0 when the path ahead is clear, otherwise -1/+1 for the direction to steer toward.
	// ProbeLength: how far ahead to look. PreferredSign: which way to dodge when either side would
	// do (0 = per-car default) - the street route passes the side it is heading for.
	float ComputeAvoidanceSteer(APawn* MyPawn, float ProbeLength, float PreferredSign) const;

	// Decision logic lives in AIRules.h (tested offline); these hold its per-car state.
	IronAI::BurstClock Burst;
	// Stuck recovery for physics cars (no pathfinding): a stall triggers a reverse-and-turn
	// manoeuvre for ReverseSeconds.
	IronAI::StuckDetector Stuck;
	float ReverseTimeLeft = 0.f;
	float ReverseSteer = 1.f;
	static constexpr float ReverseSeconds = 1.6f;
	// Missile hunters: one guided missile per interval once roughly facing the player.
	float MissileCooldown = 2.f;
	// Energy specialists: time until the next railgun charge / tesla discharge.
	float EnergyCooldown = 2.5f;

	// Street routing (RouteRules.h) on maps with an AIronCityStreet: when a building stands between
	// this car and the player, drive the roads to them instead of into the wall. The graph is built
	// once from the street's road segments; the route is refreshed twice a second.
	void UpdateRoute(APawn* MyPawn, AActor* Target, float DeltaSeconds);
	TUniquePtr<IronRoute::Graph> StreetGraph;
	TArray<IronRoute::Obstacle> StreetObstacles; // The city battlefield's props (AIronArena).
	bool bStreetChecked = false;
	IronRoute::Path Route;
	int32 RouteIndex = 0;
	float RouteTimer = 0.f;
	bool bFollowingRoute = false;
	FVector PlannedFor = FVector::ZeroVector; // Where the player was when the route was planned.

	// Three-point turn (AIRules.h) when the route leads back the way the car is facing.
	IronAI::ThreePointTurn KTurn;
	// Front (+1) or rear (-1) bumper about to hit something, or past the kerb of the carriageway.
	bool IsBumperBlocked(APawn* MyPawn, float Direction) const;

	// Squad tactics (TacticsRules.h): the target's own velocity (for intercepting it), this car's
	// lane in the squad coming at the same target, whether it sits in the player's sights, the
	// wingmen that may block its shot, and falling back to repair when badly hurt.
	void TickTactics(float DeltaSeconds, APawn* MyPawn, AActor* Target, IIronVehicle* Vehicle);
	void RefreshSquad(APawn* MyPawn, AActor* Target);
	FVector TacticalGoal(APawn* MyPawn, AActor* Target, float Distance, float SpeedKph) const;
	bool IsShotBlocked(APawn* MyPawn, AActor* Target) const;
	void AnnounceRetreat(APawn* MyPawn) const;
	IronTactics::Morale Morale;
	TWeakObjectPtr<AActor> TrackedTarget;
	FVector TrackedPrevious = FVector::ZeroVector;
	FVector TargetVelocity = FVector::ZeroVector;
	TArray<TWeakObjectPtr<APawn>> Wingmen;
	float Lane = 0.f;
	float SquadTimer = 0.f;
	float LaneOverride = 0.f;       // Side to swing out to while a wingman blocks the shot...
	float LaneOverrideLeft = 0.f;   // ...for this long.
	float BlockedSeconds = 0.f;
	float ParkedInSightsSeconds = 0.f;
	float OffLineSeconds = 0.f;     // Parked with the nose off the target (it can only turn while moving).
	float WeaveClock = 0.f;
	float WeavePhase = -1.f;
	bool bInPlayerSights = false;

public:
	// Testing: "wp 3/7 (x, y) avoid 1" - where the car is heading on its route.
	FString DescribeRoute() const
	{
		static const TCHAR* Phases[] = { TEXT(""), TEXT(" K-TURN brake"), TEXT(" K-TURN forward"), TEXT(" K-TURN reverse") };
		const TCHAR* Turn = Phases[static_cast<int32>(KTurn.Stage)];
		if (!bFollowingRoute || Route.Count == 0) return FString::Printf(TEXT("direct%s"), Turn);
		return FString::Printf(TEXT("wp %d/%d (%.0f, %.0f) avoid %.0f%s"), RouteIndex, Route.Count, Route.Points[RouteIndex].X, Route.Points[RouteIndex].Y, LastAvoidSteer, Turn);
	}
	float LastAvoidSteer = 0.f;

	// Testing: street routing on/off for every AI car (to compare against the old straight chase).
	static bool bStreetRoutingEnabled;

	// Testing: squad tactics on/off for every AI car (UIronSiegeCheatManager::DebugTactics), to
	// compare against the plain chase.
	static bool bTacticsEnabled;

	// Falling back or patching up (the HUD marks a car that is patching), rather than fighting.
	bool IsFallingBack() const { return Morale.State != IronTactics::Mode::Engage; }
	bool IsPatching() const { return Morale.State == IronTactics::Mode::Patch; }
};
