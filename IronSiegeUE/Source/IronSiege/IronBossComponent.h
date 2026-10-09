#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossRules.h"
#include "IronBossComponent.generated.h"

class AWarVehiclePawn;
class UIronPuffEmitter;
class USoundBase;

// Runs a boss fight (BossRules.h, tested offline) on the car it is added to: the three phases with
// their shield, armour patch, escort and radio line, and the boss's signature attacks - the ground
// slam and the rocket barrage of the heavy trucks, the Iron Crown's EMP, Raven's second rail shot
// and her mines. The AI controller keeps driving and shooting as usual; this component makes it
// fiercer phase by phase and layers the boss's own attacks on top. AIronSiegeGameMode adds one to
// every boss it spawns; the HUD reads the phase and the wind-ups from it.
UCLASS(ClassGroup = (IronSiege))
class IRONSIEGE_API UIronBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIronBossComponent();

	void Setup(IronBoss::Kind InKind);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// For the HUD: the phase, the shield, and the telegraphed attacks winding up right now.
	IronBoss::Kind GetKind() const { return Kind; }
	int32 GetPhase() const { return Phases.Phase; }
	bool IsShielded() const { return Phases.IsShielded(); }
	bool IsSlamWinding() const { return Slam.IsWinding(); }
	bool IsEmpWinding() const { return Emp.IsWinding(); }
	float GetSlamProgress() const { return Slam.Progress(Attacks.SlamWindUp); }
	float GetEmpProgress() const { return Emp.Progress(Attacks.EmpWindUp); }
	float GetSlamRadius() const { return Attacks.SlamRadiusCm; }
	float GetEmpRadius() const { return Attacks.EmpRadiusCm; }

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> WarnSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_MissileWarn.S_MissileWarn")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> BarrageSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_RocketLaunch.S_RocketLaunch")));

	UPROPERTY(EditAnywhere, Category = "IronSiege|Audio")
	TSoftObjectPtr<USoundBase> EmpSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Tesla.S_Tesla")));

private:
	AWarVehiclePawn* GetCar() const;
	float HealthFraction() const;

	void EnterPhase(int32 From, int32 To);
	void ApplyPhaseTuning();
	void StrikeSlam();
	void StrikeEmp(AActor* Target);
	void FireBarrage(const AActor* Target, int32 Rockets);
	void TickRaven(float DeltaSeconds, AActor* Target, float Distance);

	// A ring of puffs thrown out flat from Centre: the slam's warning and blast, the shield, the EMP.
	void EmitRing(const FVector& Centre, float Radius, float Speed, const FLinearColor& Color, int32 Count);
	void WarnPlayer(AActor* Target, float Range) const;

	IronBoss::Kind Kind = IronBoss::Kind::Juggernaut;
	IronBoss::PhaseTracker Phases;
	IronBoss::AttackTuning Attacks;
	IronBoss::WindUp Slam;
	IronBoss::WindUp Emp;
	IronBoss::Repeat Barrage;
	IronBoss::Repeat Mines;

	// Raven's second shot: armed when a shot leaves, fired when it runs out.
	bool bWasRailCharging = false;
	bool bFollowUpCharging = false; // The charge under way is already the second shot.
	float FollowUpLeft = -1.f;

	float RingTimer = 0.f;
	float ShieldSparkTimer = 0.f;

	// The AI driver's own pace, captured once, so each phase scales it rather than compounding.
	bool bHaveBase = false;
	float BaseBurstSeconds = 0.f;
	float BaseBurstPause = 0.f;
	float BaseMissileInterval = 0.f;
	float BaseAimSpread = 0.f;

	UPROPERTY()
	TObjectPtr<UIronPuffEmitter> Fx;
};
