#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Tickable.h"
#include "IronSiegeCheatManager.generated.h"

class AIronSiegePlayerController;
class IIronVehicle;

// Development/automation console commands (all prefixed Debug*), kept out of the player
// controller's interface. UCheatManager is Unreal's intended home for these: the controller only
// creates it in non-shipping builds (see AIronSiegePlayerController::BeginPlay), so none of this
// ships. Typical automated run: -ExecCmds="DebugSelectVehicle 0,DebugGod,DebugLockInput,
// DebugDelayedCommand 5 DebugDrive+1+0+4,DebugDelayedCommand 10 HighResShot+1280x720".
UCLASS()
class IRONSIEGE_API UIronSiegeCheatManager : public UCheatManager, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Picks class index (0 Scout .. 3 Artillery) and deploys immediately, skipping the menus.
	UFUNCTION(Exec)
	void DebugSelectVehicle(int32 ClassIndex);

	// Destroys the player's vehicle (reaches the game-over screen).
	UFUNCTION(Exec)
	void DebugKillSelf();

	// The player's vehicle ignores all damage.
	UFUNCTION(Exec)
	void DebugGod();

	// Ignore keyboard/mouse for this controller (stray keystrokes reaching the game window during
	// automated runs). Console / -ExecCmds commands still work.
	UFUNCTION(Exec)
	void DebugLockInput();

	// Removes every pawn except the player's (isolates physics tests).
	UFUNCTION(Exec)
	void DebugClearEnemies();

	// Drives the player's vehicle with fixed inputs for Seconds (works with DebugLockInput on).
	UFUNCTION(Exec)
	void DebugDrive(float Throttle, float SteerValue, float Seconds);

	// Logs the player's location/rotation/speed (plus gear, rpm and inputs for Chaos cars) and
	// every other pawn's location and speed.
	UFUNCTION(Exec)
	void DebugLogPos();

	// Logs every primitive component of the player's pawn: collision, simulation, attach socket.
	UFUNCTION(Exec)
	void DebugDumpPawn();

	// Spawns ClassPath 15m ahead of the player and logs where it is 4s later.
	UFUNCTION(Exec)
	void DebugSpawnClass(const FString& ClassPath);

	// Runs Command after Seconds; write spaces as '+' ("DebugDelayedCommand 20 HighResShot+1280x720").
	UFUNCTION(Exec)
	void DebugDelayedCommand(float Seconds, const FString& Command);

	// Fixed world camera, for before/after screenshots that line up exactly.
	UFUNCTION(Exec)
	void DebugViewFrom(float X, float Y, float Z, float Pitch, float Yaw);

	// Camera looking at the player's vehicle from Distance at AngleDeg (0 = front, 90 = right side).
	// TargetUp raises the point it looks at (0 = the body; about 170 = the roof weapons).
	UFUNCTION(Exec)
	void DebugViewCar(float Distance, float AngleDeg, float Height, float TargetUp);

	// Teleports the player's vehicle onto the nearest supply crate it is not already on (tests pickup end to end).
	UFUNCTION(Exec)
	void DebugGoToSupply();

	// Holds the trigger of weapon 1 (machine gun) or 2 (rockets) for Seconds.
	UFUNCTION(Exec)
	void DebugFire(int32 Weapon, float Seconds);

	// Destroys the nearest enemy as if the player had shot it (hit marker, kill notice, explosion).
	UFUNCTION(Exec)
	void DebugKillNearestEnemy();

	// Upgrade shop testing: add credits, buy item Index (0 armor .. 4 repair), leave the shop.
	UFUNCTION(Exec)
	void DebugAddCredits(int32 Amount);

	UFUNCTION(Exec)
	void DebugBuy(int32 Index);

	UFUNCTION(Exec)
	void DebugCloseShop();

	// The next wave to start will be wave N (e.g. 5 for the first boss).
	UFUNCTION(Exec)
	void DebugSetNextWave(int32 N);

	// Logs average/worst frame time over the next Seconds.
	UFUNCTION(Exec)
	void DebugMeasureFps(float Seconds);

	// Opens the settings menu and screenshots every tab, one every Interval seconds
	// (runs on the core ticker because the menu pauses the game world).
	UFUNCTION(Exec)
	void DebugSettingsTour(float Interval);

	// Like DebugDelayedCommand, but counts real seconds on the core ticker, so it still fires while
	// the game is paused (settings menu) or in photo mode's slow motion.
	UFUNCTION(Exec)
	void DebugDelayedCommandRealtime(float Seconds, const FString& Command);

	// Toggles the free-camera photo mode (same as pressing P).
	UFUNCTION(Exec)
	void DebugPhotoMode();

	// Runs the in-game benchmark and applies its recommendation.
	UFUNCTION(Exec)
	void DebugBenchmark(float Seconds, float TargetFps);

	// Puts the player's car at a fraction of its health (armour off) - for the damage smoke/fire.
	UFUNCTION(Exec)
	void DebugSetHealth(float Fraction);

	// Lines every vehicle class up beside the player (no drivers) and frames them with a camera,
	// to compare the war kits. Stage: 0 = the four player classes, 1 = the enemies (three variants
	// of each so the variety shows), the boss and the missile hunter, 2 = the hunter alone.
	UFUNCTION(Exec)
	void DebugShowroom(int32 Stage);

	// Unlocks the flame tank and holds the trigger for Seconds.
	UFUNCTION(Exec)
	void DebugFlamer(float Seconds);

	// Drops one mine and, once it has armed, drives the nearest enemy onto it - the whole mine
	// path (arming delay, trigger radius, blast) in one command.
	UFUNCTION(Exec)
	void DebugMineTest();

	// Accelerates to speed, then yanks the handbrake into a full-lock turn: a drift, for testing
	// the tyre smoke, skid marks and screech.
	UFUNCTION(Exec)
	void DebugDrift(float Seconds);

	// Unlocks the mine rack and drops Count mines behind the car.
	UFUNCTION(Exec)
	void DebugDropMines(int32 CountToDrop);

	// Holds nitro for Seconds (or releases it with 0), so boost can be tested headlessly.
	UFUNCTION(Exec)
	void DebugBoost(float Seconds);

	// Drives the player's car into the nearest enemy to test ram damage.
	UFUNCTION(Exec)
	void DebugRamNearest();

	// Settings profiles without the mouse: Action is save / load / list.
	UFUNCTION(Exec)
	void DebugProfile(const FString& Action, const FString& Name);

	// Switches the handling model: 0 arcade, 1 balanced, 2 simulation.
	UFUNCTION(Exec)
	void DebugDrivingStyle(int32 Style);

	// Missiles and countermeasures: a guided-missile hunter Distance cm from the player (toward the
	// middle of the map), and a flare burst.
	UFUNCTION(Exec)
	void DebugSpawnHunter(float Distance);

	UFUNCTION(Exec)
	void DebugFlares();

	// Any specialist enemy: 0 missile hunter, 1 tesla stormer, 2 railgun lancer.
	UFUNCTION(Exec)
	void DebugSpawnSpecialist(int32 Kind, float Distance);

	// Test run: takes one HUD screenshot the moment an enemy railgun starts charging at the player.
	UFUNCTION(Exec)
	void DebugShotOnRailCharge();

	// Street routing tests: a plain enemy buggy at (X, Y) on the map; routing on/off for every AI
	// car; and a log line every second of how far each enemy is from the player.
	UFUNCTION(Exec)
	void DebugSpawnEnemyAt(float X, float Y);

	// Same, but facing Yaw degrees (to test turning round when the way on is behind).
	UFUNCTION(Exec)
	void DebugSpawnEnemyFacing(float X, float Y, float Yaw);

	UFUNCTION(Exec)
	void DebugStreetRouting(int32 bEnabled);

	UFUNCTION(Exec)
	void DebugTeleport(float X, float Y, float Yaw);

	UFUNCTION(Exec)
	void DebugTrackEnemies(float Seconds);

	// Test run: once a guided missile homing on the player is within Distance cm, take a HUD
	// screenshot (the warning) and fire flares, once.
	UFUNCTION(Exec)
	void DebugAutoFlares(float Distance);

	// Energy weapons: unlock and fire the railgun / tesla coil now; and park Count enemy buggies
	// (no driver) in a row ahead of the player, Spacing cm apart, to shoot at.
	UFUNCTION(Exec)
	void DebugRailgun();

	UFUNCTION(Exec)
	void DebugTesla();

	UFUNCTION(Exec)
	void DebugSpawnLine(int32 Count, float Spacing);

	// Handling telemetry: full throttle in a straight line for Seconds (logging the 0-50 and 0-100
	// times and the top speed reached against the class limit), then full brakes to a stop (logging
	// the stopping distance). Start it on a long clear straight.
	UFUNCTION(Exec)
	void DebugDragTest(float Seconds);

	// Air control check: lifts the car Height cm, rolled RollDeg and pitched PitchDeg, drops it and
	// logs how upright it is at touchdown and one second later (1 = on its wheels, -1 = on its roof).
	UFUNCTION(Exec)
	void DebugAirDrop(float Height, float RollDeg, float PitchDeg);

	// Handling diagnostics: sets every wheel's max brake torque on the player's car directly (to
	// check Chaos takes the setter at runtime); the wheel log of DebugDragTest shows what is used.
	UFUNCTION(Exec)
	void DebugWheelBrake(float Torque);

	// Handling test drives, each logged as one "IronSiege: handling ..." line (start on open ground):
	// 0 turn - hold 50 km/h, then full lock: seconds to turn 90 and 180 degrees, turning circle.
	// 1 reverse - from 30 km/h forward, full reverse: seconds until rolling backward at 10 km/h,
	//   and the reverse speed after 4 s.
	// 2 drift - at 70 km/h, handbrake and full lock for 0.6 s, then throttle with half counter-lock:
	//   peak slide angle, seconds sliding past 15 degrees, speed kept, heading change.
	// 3 u-turn - from a standstill, reverse on full lock then drive out on the other lock: seconds
	//   to face the other way.
	// 4 handbrake turn - at 45 km/h, handbrake and full lock held: seconds to face the other way.
	// 5 J-turn - reverse to 24 km/h, then handbrake and full lock: seconds to face the other way.
	UFUNCTION(Exec)
	void DebugHandlingTest(int32 Kind);

	// Aim assist level for this session (0 off, 1 normal, 2 strong), for A/B tests.
	UFUNCTION(Exec)
	void DebugAimAssist(int32 Level);

	// Campaign and crew. A mission is played by opening its level with ?Mission=N (1-based), e.g.
	// "/Game/Maps/Map_Desert?Mission=2" on the command line; these drive it from there.
	// The driver the next DebugSelectVehicle deploys with (0 Rin .. 5 Omar); use the ability now.
	UFUNCTION(Exec)
	void DebugDriver(int32 Driver);

	UFUNCTION(Exec)
	void DebugAbility();

	// Finishes the mission's current stage / fails the mission, and logs where the mission stands.
	UFUNCTION(Exec)
	void DebugMissionSkip();

	UFUNCTION(Exec)
	void DebugMissionFail();

	UFUNCTION(Exec)
	void DebugMissionLog();

	// Menus without the keyboard: show a screen (0 mode, 1 battlefield, 2 missions, 3 briefing,
	// 4 driver, 5 vehicle), press a menu key (0 F3, 1 F4, 2 Enter, 3 F2).
	UFUNCTION(Exec)
	void DebugScreen(int32 Screen);

	UFUNCTION(Exec)
	void DebugMenuKey(int32 Key);

	// Saved campaign progress: "reset" clears it, "unlock" marks every mission done with N stars.
	UFUNCTION(Exec)
	void DebugCampaign(const FString& Action, int32 Stars);

	// Puts a story line on the radio as Speaker (0-5 the drivers, 6 Hana, 7 Raven, 8 Varga), to
	// check the radio box and that speaker's portrait.
	UFUNCTION(Exec)
	void DebugRadio(int32 Speaker, int32 Mood);

	// Teleports the player's car beside the mission's first marker (a depot, a zone, a truck).
	UFUNCTION(Exec)
	void DebugGoToObjective(float Distance);

	// FTickableGameObject: only ticks while a DebugDrive or DebugMeasureFps is running.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return DriveRemaining > 0.f || FpsRemaining > 0.f || FireRemaining > 0.f || DriftRemaining > 0.f || FlamerRemaining > 0.f; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UIronSiegeCheatManager, STATGROUP_Tickables); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

private:
	AIronSiegePlayerController* GetIronController() const;
	IIronVehicle* GetVehicle() const;

	float DriveRemaining = 0.f;
	float DriftRemaining = 0.f;
	float FlamerRemaining = 0.f;
	bool bDriftSliding = false;
	float DriveThrottle = 0.f;
	float DriveSteer = 0.f;

	float FireRemaining = 0.f;
	int32 FireWeapon = 1;

	float FpsRemaining = 0.f;
	float FpsTotal = 0.f;
	float FpsWorst = 0.f;
	int32 FpsFrames = 0;
};
