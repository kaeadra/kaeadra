#pragma once
#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "VehicleClassRules.h"
#include "IronVehicleClass.h"
#include "IronVehicle.h"
#include "BoostRules.h"
#include "SkidRules.h"
#include "HandlingRules.h"
#include "UpgradeRules.h"
#include "FlareRules.h"
#include "DamageRules.h"
#include "BeamRules.h"
#include "CrewRules.h"
#include "WarVehiclePawn.generated.h"

class UMachineGunComponent;
class URocketLauncherComponent;
class UVehicleHealthComponent;
class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UAudioComponent;
class USoundBase;

// Real Chaos-physics war car. Chaos wheels need a skeletal mesh with wheel bones plus a tuned
// Vehicle Movement setup; the Blueprints under /Game/IronSiege/Vehicles/Chaos get both from Epic's
// Vehicle Template (SportsCar / OffroadCar - see Content/Python/setup_chaos_vehicles.py): the
// template Blueprint is duplicated (keeping its body/glass/wheel parts attached to the rig's
// bones) and reparented onto this class, then the template's movement tuning is copied over.
// Which war kit (VehicleKitRules.h) is bolted onto this car. Mirrors IronKits::Kit.
UENUM(BlueprintType)
enum class EIronKit : uint8
{
	None,
	Scout,
	Assault,
	Heavy,
	Artillery,
	Raider,
	Brute,
	Juggernaut,
	Hunter,
	Interceptor,
	Dune,
	Stormer,
	Lancer
};

UCLASS()
class IRONSIEGE_API AWarVehiclePawn : public AWheeledVehiclePawn, public IIronVehicle
{
	GENERATED_BODY()

public:
	AWarVehiclePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Steering feel (maths in HandlingRules.h, tested offline). Keyboard steering is all-or-nothing,
	// so the requested value is eased in (SteeringResponse = full lock per second), scaled down
	// overall (SteeringScale), and scaled down further with speed so high-speed turns stay
	// controllable (HighSpeedSteeringScale is the multiplier reached at HighSpeedKph and above).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Handling")
	float SteeringScale = 0.85f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Handling")
	float HighSpeedSteeringScale = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Handling")
	float HighSpeedKph = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Handling")
	float SteeringResponse = 4.5f;

	// Looping engine sound; pitch follows engine RPM, volume follows throttle.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> EngineSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_EngineLoop.S_EngineLoop")));

	// Nitro whoosh, and the crunch of ramming something.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> BoostSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Boost.S_Boost")));

	// Tyre screech, looping while the car slides.
	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> SkidSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Skid.S_Skid")));

	// Dark rubber mark stamped on the road while drifting.
	UPROPERTY(EditAnywhere, Category = "IronSiege|FX")
	TSoftObjectPtr<class UMaterialInterface> SkidMarkMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/M_SkidMark.M_SkidMark")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> RamSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Impact.S_Impact")));

	// RPM treated as "full pitch" (the Vehicle Template engines redline at 7000).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Audio")
	float EngineAudioMaxRpm = 7000.f;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Audio")
	TObjectPtr<UAudioComponent> EngineAudio;

	// Second engine layer: the harsh high-rev note, crossfaded in over the idle loop with RPM and
	// throttle; and the clutch clunk on an upshift.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> EngineHighSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_EngineHigh.S_EngineHigh")));

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> GearShiftSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_GearShift.S_GearShift")));

	UPROPERTY()
	TObjectPtr<UAudioComponent> EngineHighAudio;

	// Applied to every body part when the car is destroyed (charred wreck).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Visual")
	TSoftObjectPtr<UMaterialInterface> WreckMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/IronSiege/City/Materials/MI_City_FrameDark.MI_City_FrameDark")));

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Class")
	EIronVehicleClass VehicleClass = EIronVehicleClass::Assault;

	// Bolt-on armour and silhouette for this car (IronKitBuilder). Enemies with several variants
	// pick one at random when KitVariant is 0, so a wave is not a row of clones.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Visual")
	EIronKit Kit = EIronKit::None;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Visual")
	int32 KitVariant = 0;

	// Showroom/testing: force a variant before the pawn finishes spawning.
	void SetKitVariantForPreview(int32 Variant) { KitVariant = Variant; }

	// Multiplies the class's max health and armor (the Juggernaut boss uses ~4).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Class")
	float DurabilityMultiplier = 1.f;

	// When AI-driven, also fire rockets (the boss).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|AI")
	bool bAIFiresRockets = false;

	// When AI-driven, fire guided missiles at the player from stand-off range (the "hunter"
	// enemy; set by the game mode on spawn).
	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	bool bAIGuidedMissiles = false;

	// Late-wave energy specialists (set by the game mode on spawn): a railgun "lancer" charges and
	// fires at where the player was when the charge began; a tesla "stormer" rushes in to zap.
	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	bool bAIRailgun = false;

	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	bool bAITesla = false;

	// Campaign trucks (set by the game mode on spawn): driven by the AI but on the player's side
	// (IronTeams) - the Legion shoots at it, the player cannot hurt it.
	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	bool bPlayerSide = false;

	// A truck carries cargo, not guns: no roof weapons are fitted.
	UPROPERTY(EditAnywhere, Category = "IronSiege|AI")
	bool bUnarmed = false;

	// The driver behind the wheel (CrewRules.h): a passive perk, and one ability on a cooldown.
	// -1 for cars nobody in particular drives (every AI car).
	void ApplyDriver(int32 InDriverIndex);
	int32 GetDriverIndex() const { return DriverIndex; }
	bool UseAbility();
	const IronCrew::AbilityState& GetAbility() const { return Ability; }
	float GetSupplyGain() const { return DriverIndex >= 0 ? IronCrew::Get(static_cast<IronCrew::Driver>(DriverIndex)).Passive.SupplyGain : 1.f; }

	// Player settings: FOV, chase distance/height multipliers, camera smoothing (0..1).
	// bReduceMotion stiffens the camera for players who find the lag uncomfortable.
	void ApplyViewSettings(float FieldOfView, float DistanceScale, float HeightScale, float Smoothing, bool bReduceMotion = false);

	virtual void DeployMine() override;
	virtual void FireFlamer() override;
	virtual class UFlamethrowerComponent* GetFlamerComponent() const override { return Flamer; }
	virtual class UMineLayerComponent* GetMineWeaponComponent() const override { return MineWeapon; }

	// Missile lock (MissileRules.h): the enemy the pod would guide onto right now, if any.
	AActor* GetLockTarget() const { return LockTarget.Get(); }

	// Countermeasures (FlareRules.h): a rack of flares that recharges, and the nearest guided
	// missile homing on this car (for the HUD warning), with its distance.
	virtual void FireFlares() override;
	int32 GetFlareCharges() const { return Flares.Charges; }
	float GetFlareRecharge() const { return Flares.RechargeFraction(FlareTuning); }
	AActor* GetIncomingMissile() const { return IncomingMissile.Get(); }
	float GetIncomingMissileDistance() const { return IncomingDistance; }

	// Energy weapons (EnergyWeapons.h), unlocked in the shop.
	virtual void FireRailgun() override;
	virtual void FireTesla() override;
	class URailgunComponent* GetRailgun() const { return Railgun; }
	class UTeslaComponent* GetTesla() const { return Tesla; }
	bool IsRailgunCharging() const { return RailCharge.IsCharging(); }

	// AI: start a railgun charge aimed at a fixed point (the target's position right now), so the
	// charge time is the player's window to swerve out of the line.
	void FireRailgunAt(const FVector& TargetLocation);
	bool IsRailAimFixed() const { return bRailAimFixed; }

	// Struck by a tesla coil: the engine cuts out (no throttle) for Seconds, with sparks.
	void Stun(float Seconds);
	bool IsStunned() const { return StunTimeLeft > 0.f; }

	// Set alight by a flamethrower (IronDamage::BurnState): burns for a few seconds, credited to By.
	void Ignite(float Dps, AController* By);
	bool IsBurning() const { return Burn.IsBurning(); }

	// Nitro boost (IronBoost): held down for extra engine torque and a camera FOV kick, drains a
	// meter that refills after a pause and on kills.
	void SetBoost(bool bEngaged) { bBoostHeld = bEngaged; }
	float GetBoostCharge() const { return Boost.Charge; }
	bool IsBoosting() const { return Boost.Active; }
	void AddBoostCharge(float Amount);

	// Controller vibration, if this car is the local player's and vibration is on.
	void RumbleController(float Strength, float Duration);

	// Ramming: our mesh hitting another car at speed hurts both of us (IronDamage::RamDamage).
	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Flips a car that has been sitting on its roof back onto its wheels.
	void TickFlipRecovery(float DeltaSeconds);

	// Player setting: 0 arcade, 1 balanced, 2 simulation - steering feel plus tyre grip,
	// brake and handbrake torque on the Chaos wheels.
	void ApplyDrivingStyle(int32 Style);

	// Player settings: steering strength multiplier and smoothing (0..1).
	void ApplyControlSettings(float Sensitivity, float Smoothing);

	// Applies shop upgrades: armor plating, weapon damage, engine torque.
	void ApplyUpgrades(const IronUpgrades::Loadout& Upgrades);

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Weapons")
	TObjectPtr<UMachineGunComponent> PrimaryWeapon;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Weapons")
	TObjectPtr<URocketLauncherComponent> SecondaryWeapon;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Health")
	TObjectPtr<UVehicleHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Weapons")
	TObjectPtr<USceneComponent> MuzzlePoint;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Visual")
	TObjectPtr<UStaticMeshComponent> PrimaryWeaponMesh;

	UPROPERTY(VisibleAnywhere, Category = "IronSiege|Visual")
	TObjectPtr<UStaticMeshComponent> SecondaryWeaponMesh;

	virtual void MoveForward(float Value) override;
	virtual void Steer(float Value) override;
	virtual void SetHandbrake(bool bEngaged) override;
	virtual void FirePrimary() override;
	virtual void FireSecondary() override;
	virtual void FirePrimaryAt(const FVector& TargetLocation) override;
	virtual void ReloadPrimary() override;
	virtual void ReloadSecondary() override;
	virtual void ToggleCameraView() override;
	virtual float GetSpeedKph() const override;
	virtual FVector GetAimPoint() const override;
	virtual UVehicleHealthComponent* GetHealthComponent() const override { return Health; }
	virtual UMachineGunComponent* GetPrimaryWeaponComponent() const override { return PrimaryWeapon; }
	virtual URocketLauncherComponent* GetSecondaryWeaponComponent() const override { return SecondaryWeapon; }

protected:
	UFUNCTION()
	void HandleDestroyed();

	UFUNCTION()
	void HandleDamaged(float Damage, AController* InstigatedBy);

	bool bCockpitView = false;

private:
	FVector GetAimOrigin() const;
	FVector GetAimDirection() const;

	float ChaseArmLength = 0.f;
	float TargetSteer = 0.f;
	float BaseEngineTorque = 0.f;
	float BaseArmLength = 0.f;
	float BaseArmHeight = 0.f;
	float SteeringSensitivity = 1.f;
	// Drift (IronSkid): smoke puffs off the rear tyres, skid marks on the road and a screech loop,
	// all driven by how far sideways the car is actually travelling.
	void TickSkid(float DeltaSeconds);

	// Battle damage you can read at a glance: smoke from under the bonnet below half health, fire
	// as well below a quarter, and a smouldering wreck once destroyed.
	void TickDamageFx(float DeltaSeconds);

	IronBoost::State Boost;

	// Tyre smoke, and engine smoke/fire - soft translucent puffs (UIronPuffEmitter).
	UPROPERTY()
	TObjectPtr<class UIronPuffEmitter> SkidSmoke;

	UPROPERTY()
	TObjectPtr<class UIronPuffEmitter> DamageFx;

	FBox BodyBox;              // Chassis bounds in its own space, from the kit fit.

	// Roof weapons (WeaponRigRules.h): where each rig sits on the chassis, and the pod missiles
	// that are hidden one by one as they are fired.
	void BuildWeaponRigs();
	void TickMissileRack();
	void TickLockOn(float DeltaSeconds);
	void TickThreats(float DeltaSeconds);
	void TickEnergyWeapons(float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<class URailgunComponent> Railgun;

	UPROPERTY()
	TObjectPtr<class UTeslaComponent> Tesla;

	IronBeams::ChargeClock RailCharge;
	bool bRailAimFixed = false;
	FVector RailAimPoint = FVector::ZeroVector;
	float StunTimeLeft = 0.f;
	float StunSparkTimer = 0.f;
	void TickBurn(float DeltaSeconds);

	IronFlares::State Flares;
	IronFlares::Tuning FlareTuning;

	// Driver: the perk and ability on top of the class traits and upgrades.
	void TickAbility(float DeltaSeconds);
	void RefreshDriverTraits();
	void SetEngineTorque();
	void EmpPulse();
	int32 DriverIndex = -1;
	IronCrew::AbilityState Ability;
	bool bAbilityWasActive = false;
	float AbilityTorqueScale = 1.f;
	float UpgradeArmorScale = 1.f;
	float AbilityPuffTimer = 0.f;
	TWeakObjectPtr<AActor> IncomingMissile;
	float IncomingDistance = -1.f;
	float WarnBeepTimer = 0.f;

	IronDamage::BurnState Burn;
	TWeakObjectPtr<AController> BurnInstigator;
	float BurnPending = 0.f;    // Burn damage accumulated but not yet applied (sent in chunks).
	float BurnPuffTimer = 0.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> FlareSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Flare.S_Flare")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> MissileWarnSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_MissileWarn.S_MissileWarn")));
	FVector CannonMount = FVector::ZeroVector;
	FVector PodMount = FVector::ZeroVector;
	TArray<TPair<int32, UStaticMeshComponent*>> PodMissiles;
	// Models of the unlockable weapons (railgun, tesla coil, flamethrower, mine rack), shown once bought.
	TArray<TWeakObjectPtr<UStaticMeshComponent>> ExtraWeaponModels;
	int32 ShownMissiles = -1;

	UPROPERTY()
	TObjectPtr<USceneComponent> CannonMuzzle;

	UPROPERTY()
	TObjectPtr<USceneComponent> PodMuzzle;

	TWeakObjectPtr<AActor> LockTarget;
	float LockScanTimer = 0.f;

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> LockOnSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_LockOn.S_LockOn")));
	float DamageFxTimer = 0.f;
	float WreckSmokeSeconds = 0.f;

	UPROPERTY()
	TObjectPtr<class UAudioComponent> SkidAudio;

	UPROPERTY()
	TObjectPtr<class UMineLayerComponent> MineWeapon;

	UPROPERTY()
	TObjectPtr<class UFlamethrowerComponent> Flamer;

	float SkidPuffTimer = 0.f;
	float SkidMarkDistance = 0.f;
	FVector LastSkidLocation = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	float LateralKph = 0.f;
	bool bHandbrakeHeld = false;
	bool bBoostHeld = false;
	float ViewFov = 90.f;          // Camera FOV before the boost kick.
	float UpgradeTorqueScale = 1.f;// Engine upgrade multiplier, reapplied when boost turns on/off.
	bool bBoostWasActive = false;
	float UprightSeconds = 0.f;    // How long we have been lying on our roof.
	TMap<TWeakObjectPtr<AActor>, double> LastRamTimes;
	TArray<float> BaseBrakeTorques;
	TArray<float> BaseHandbrakeTorques;
	TArray<float> BaseFrictions;  // Each wheel's own FrictionForceMultiplier: the setter replaces it, so scale from this.
	float SmoothedSteer = 0.f;

	// Class physics, weapon trait and driving assists (VehicleClassRules.h, HandlingRules.h): the
	// top speed is a real limit, the chassis gets its grip, centre of mass and downforce, and the
	// assists catch spins and level the car in the air.
	void ApplyClassTraits();
	void TickAssists(float DeltaSeconds);
	IronBoost::Tuning BoostTuning;
	IronHandling::AssistTuning Assist;
	float ClassTopSpeedKph = 0.f;
	float ClassGrip = 1.f;
	float ClassAirControl = 1.f;
	float RequestedThrottle = 0.f;
	float WheelbaseCm = 260.f;
	float MaxSteerDeg = 35.f;
	float AirborneSeconds = 0.f;
	FQuat PreviousRotation = FQuat::Identity;
	bool bHasPreviousRotation = false;
	float ClassEnginePitch = 1.f;
	float ClassBrake = 1.f;
	float ClassDownforce = 0.f;

	// Arcade handling (HandlingRules.h): turn-in and grip help, handbrake drifts, a reverse limit.
	IronHandling::ArcadeTuning Arcade;
	IronHandling::DriftState Drift;
	float ClassTurn = 1.f;
	float ClassReverseKph = 40.f;
	float RequestedBrake = 0.f;
	FVector AssistPrevPos = FVector::ZeroVector;
	FVector SmoothedVelocity = FVector::ZeroVector;
	TArray<bool> RearWheels;
	int32 LastGear = 0;

	// The wheel-level physics settings (grip, brake and handbrake torque, ABS, downforce) live in the
	// Chaos simulation, and resetting the vehicle (UChaosVehicleMovementComponent::ResetVehicle)
	// puts them back to the Blueprint's values; they are applied again shortly after every change.
	void ApplyWheelSettings();
	float WheelFriction = 1.f;
	float WheelBrake = 1.f;
	float WheelHandbrake = 1.f;
	float WheelSettingsClock = -1.f;
	void TickEngineAudio(float DeltaSeconds);

	// The car in motion: flame out of the exhausts while the nitro burns, and a trail of whatever
	// the wheels are throwing up (sand, snow) at speed off the paved city.
	void TickCarFx(float DeltaSeconds);
	UPROPERTY()
	TObjectPtr<class UIronPuffEmitter> ExhaustFx;
	UPROPERTY()
	TObjectPtr<class UIronPuffEmitter> WheelDust;
	float ExhaustTimer = 0.f;
	float DustTimer = 0.f;
	FLinearColor GroundDustColor = FLinearColor::Transparent; // Alpha 0: paved map, no dust.

public:
	// Handling telemetry: whether the car is in a handbrake drift right now.
	bool IsDrifting() const { return Drift.bActive; }

	// Telemetry and tests: the enforced top speed, and how many wheels touch the ground.
	float GetTopSpeedLimitKph() const { return ClassTopSpeedKph * IronBoost::TopSpeedMultiplier(Boost, BoostTuning); }
	int32 GetWheelsOnGround() const;
	// Call after resetting the vehicle simulation (a teleport): puts the class and style wheel
	// settings back.
	void RefreshWheelSettings()
	{
		ApplyWheelSettings();
		WheelSettingsClock = 0.f;
	}

	// Aim assist (AimRules.h): the enemy the player's gun is helping onto right now, where a car's
	// body centre is (what the assist and the lock aim at), and the direction a shot leaves in.
	AActor* GetAimTarget() const { return AimTarget.Get(); }
	FVector GetBodyCenter() const;
	FVector GetFireDirection() const;

private:
	// The roof turret swivels toward the aim, the rotary barrels spin up while firing and kick back
	// on every round (hinges from IronKitBuilder::BuildRig).
	void TickAimAssist(float DeltaSeconds);
	void TickTurret(float DeltaSeconds);
	TWeakObjectPtr<AActor> AimTarget;
	float AimScanTimer = 0.f;
	UPROPERTY()
	TObjectPtr<USceneComponent> TurretHinge;
	UPROPERTY()
	TObjectPtr<USceneComponent> BarrelHinge;
	FVector BarrelRest = FVector::ZeroVector;
	float TurretYaw = 0.f;
	float TurretPitch = 0.f;
	float BarrelRpm = 0.f;
	float BarrelRoll = 0.f;
	float Recoil = 0.f;
	double LastGunShotTime = -10.0;
	FVector AiAimDirection = FVector::ForwardVector;
	double AiAimTime = -10.0;
};
