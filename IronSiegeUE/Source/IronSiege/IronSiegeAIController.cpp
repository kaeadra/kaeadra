#include "IronSiegeAIController.h"
#include "IronVehicle.h"
#include "MachineGunComponent.h"
#include "RocketLauncherComponent.h"
#include "WarVehiclePawn.h"
#include "EnergyWeapons.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IronCityStreet.h"
#include "IronArena.h"
#include "IronTeams.h"
#include "MissionRules.h"
#include "WheeledVehiclePawn.h"
#include "ChaosVehicleMovementComponent.h"

bool AIronSiegeAIController::bStreetRoutingEnabled = true;

namespace
{
IronRoute::Vec2 To2D(const FVector& V) { return { float(V.X), float(V.Y) }; }
}

AIronSiegeAIController::AIronSiegeAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

IIronVehicle* AIronSiegeAIController::GetControlledVehicle() const
{
	return Cast<IIronVehicle>(GetPawn());
}

AActor* AIronSiegeAIController::FindTarget() const
{
	if (AActor* Preferred = PreferredTarget.Get(); IronTeams::IsAlive(Preferred))
	{
		return Preferred;
	}
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AIronSiegeAIController::DriveRoute(const TArray<FVector>& Points, float SpeedKph)
{
	ConvoyPoints = Points;
	ConvoyIndex = 0;
	ConvoySpeedKph = SpeedKph;
	bConvoyArrived = false;
}

void AIronSiegeAIController::TickConvoy(float DeltaSeconds, IIronVehicle* Vehicle, APawn* MyPawn)
{
	const FVector Here = MyPawn->GetActorLocation();
	const float Reach = IronMissions::ConvoyTuning().WaypointRadius;
	while (ConvoyIndex < ConvoyPoints.Num() && FVector::Dist2D(Here, ConvoyPoints[ConvoyIndex]) < Reach)
	{
		++ConvoyIndex;
	}
	if (ConvoyIndex >= ConvoyPoints.Num())
	{
		bConvoyArrived = true;
		Vehicle->Steer(0.f);
		Vehicle->MoveForward(0.f);
		Vehicle->SetHandbrake(true);
		return;
	}
	if (ReverseTimeLeft > 0.f)
	{
		// Backing off whatever it ran into, wheels turned so the nose swings back toward the route.
		ReverseTimeLeft -= DeltaSeconds;
		Vehicle->Steer(ReverseSteer);
		Vehicle->MoveForward(-1.f);
		return;
	}
	const FVector ToGoal = (ConvoyPoints[ConvoyIndex] - Here).GetSafeNormal2D();
	const FVector Forward = MyPawn->GetActorForwardVector().GetSafeNormal2D();
	const float SteerSign = FVector::CrossProduct(Forward, ToGoal).Z >= 0.f ? 1.f : -1.f;
	const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(Forward, ToGoal)), -1.f, 1.f)));
	const float AvoidSteer = ComputeAvoidanceSteer(MyPawn, 700.f, SteerSign);
	LastAvoidSteer = AvoidSteer;
	Vehicle->Steer(AvoidSteer != 0.f ? AvoidSteer : IronRoute::SteerForAngle(Degrees * SteerSign));
	// Steady pace, slower through the corners of the route and round obstacles.
	float DesiredKph = IronRoute::CornerSpeedKph(Degrees, ConvoySpeedKph);
	if (AvoidSteer != 0.f)
	{
		DesiredKph = FMath::Min(DesiredKph, 16.f);
	}
	const float SpeedKph = Vehicle->GetSpeedKph();
	Vehicle->MoveForward(IronAI::ThrottleFor(SpeedKph, DesiredKph, IronAI::DriveTuning()));
	if (Stuck.Update(DesiredKph, SpeedKph, DeltaSeconds))
	{
		ReverseTimeLeft = ReverseSeconds;
		ReverseSteer = -SteerSign;
	}
}

float AIronSiegeAIController::ComputeAvoidanceSteer(APawn* MyPawn, float ProbeLength, float PreferredSign) const
{
	UWorld* World = GetWorld();
	if (!World || !MyPawn)
	{
		return 0.f;
	}

	// Probe at bonnet height: Chaos cars have their origin on the ground, where kerbs would read as walls.
	const FVector Origin = MyPawn->GetActorLocation() + FVector(0.f, 0.f, 90.f);
	const FVector Forward = MyPawn->GetActorForwardVector();
	const FVector Up = FVector::UpVector;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeAIAvoid));
	Params.AddIgnoredActor(MyPawn);

	// A sphere a car's width across swept low, not a thin line: a line at bonnet height slipped
	// between the beams of the city's tank traps and the car drove into them and stuck.
	// Centred 1.2 m up with a 70 cm radius: 50-190 cm above the road, clear of kerbs and the
	// pavement surface, low enough for sandbags and the beams of a tank trap.
	const FCollisionShape Probe = FCollisionShape::MakeSphere(70.f);
	const FVector Low(0.f, 0.f, 30.f);
	auto ProbeBlocked = [&](float AngleDeg) -> bool
	{
		const FVector Direction = Forward.RotateAngleAxis(AngleDeg, Up);
		FHitResult Hit;
		return World->SweepSingleByChannel(Hit, Origin + Low + Direction * 150.f, Origin + Low + Direction * ProbeLength, FQuat::Identity, ECC_Visibility, Probe, Params);
	};

	if (!ProbeBlocked(0.f))
	{
		return 0.f;
	}

	const bool bLeftBlocked = ProbeBlocked(-AvoidProbeAngleDeg);
	const bool bRightBlocked = ProbeBlocked(AvoidProbeAngleDeg);
	if (bLeftBlocked && !bRightBlocked)
	{
		return 1.f;
	}
	if (bRightBlocked && !bLeftBlocked)
	{
		return -1.f;
	}
	// Both sides blocked (or both clear while dead ahead is not): commit to one side rather than
	// oscillating - toward the route if there is one, else picked per-pawn so a cluster of enemies
	// doesn't all swerve the same way.
	if (PreferredSign != 0.f)
	{
		return PreferredSign > 0.f ? 1.f : -1.f;
	}
	return (GetUniqueID() % 2 == 0) ? 1.f : -1.f;
}

void AIronSiegeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	IIronVehicle* Vehicle = GetControlledVehicle();
	APawn* MyPawn = GetPawn();
	if (!Vehicle || !MyPawn)
	{
		return;
	}

	if (ConvoyPoints.Num() > 0)
	{
		TickConvoy(DeltaSeconds, Vehicle, MyPawn);
		return;
	}

	AActor* Target = FindTarget();
	if (!Target || !IsValid(Target))
	{
		Vehicle->MoveForward(0.f);
		Vehicle->Steer(0.f);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - MyPawn->GetActorLocation();
	const float Distance = ToTarget.Size();
	const FVector ToTargetDir = ToTarget.GetSafeNormal();
	const FVector Forward = MyPawn->GetActorForwardVector();

	// Weapons aim at the player; the wheels head for the next street waypoint while a building is
	// in the way, otherwise straight for the player too.
	UpdateRoute(MyPawn, Target, DeltaSeconds);
	FVector Goal = Target->GetActorLocation();
	float DriveDistance = Distance;
	bool bWeaving = false;
	if (bFollowingRoute && Route.Count > 0)
	{
		// The carrot on the road's centre line, moved sideways round any wreck or barricade between
		// here and there (a car's half-width plus a margin of clearance, staying on the carriageway).
		const IronRoute::Vec2 Car2D = To2D(MyPawn->GetActorLocation());
		// Clearance covers half the car's width plus the slide of a physics car off its line (160 was
		// not enough: a buggy at 43 km/h clipped a tank trap with its corner).
		const IronRoute::Vec2 Centre = IronRoute::Carrot(Route, RouteIndex, Car2D, 1200.f);
		const IronRoute::Vec2 W = IronRoute::ChooseLane(Car2D, Centre, StreetObstacles.GetData(), StreetObstacles.Num(), 240.f, 500.f);
		bWeaving = IronRoute::Dist(W, Centre) > 1.f;
		Goal = FVector(W.X, W.Y, MyPawn->GetActorLocation().Z);
		// Speed from what is left to drive, not the straight-line gap through the building.
		DriveDistance = FVector::Dist2D(MyPawn->GetActorLocation(), Goal);
		for (int32 i = RouteIndex; i + 1 < Route.Count; ++i)
		{
			DriveDistance += IronRoute::Dist(Route.Points[i], Route.Points[i + 1]);
		}
	}
	const FVector ToGoalDir = (Goal - MyPawn->GetActorLocation()).GetSafeNormal2D();
	const float GoalDot = FVector::DotProduct(Forward.GetSafeNormal2D(), ToGoalDir);

	const float FacingDot = FVector::DotProduct(Forward, ToTargetDir);
	const float SteerSign = (FVector::CrossProduct(Forward, ToGoalDir).Z >= 0.f) ? 1.f : -1.f;
	// Steer harder the more the goal is off to the side, ease off once nearly facing it.
	const float SteerMagnitude = FMath::Clamp(1.f - GoalDot, 0.f, 1.f);

	// Avoidance overrides the chase steer while something is in the way, so the AI drives around
	// obstacles instead of grinding into them on the way to the player.
	// On a street route only what is right ahead in the road matters (the route itself keeps the car
	// off the buildings); dodge toward the side the route is heading.
	const float AvoidSteer = bFollowingRoute ? ComputeAvoidanceSteer(MyPawn, 650.f, SteerSign) : ComputeAvoidanceSteer(MyPawn, AvoidProbeLength, 0.f);
	LastAvoidSteer = AvoidSteer;

	// The way on lies behind in a street too narrow to swing round: three-point turn - forward on
	// full lock to the kerb, back on opposite lock, repeat - instead of grinding into the pavement.
	IronAI::ThreePointTurn::Output Turn;
	if (!bFollowingRoute)
	{
		KTurn = IronAI::ThreePointTurn(); // Close in directly: any turn in progress is moot.
	}
	else if (ReverseTimeLeft <= 0.f)
	{
		const float GoalDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(GoalDot, -1.f, 1.f))) * SteerSign;
		// Signed speed: the turn needs to know which way the car is still rolling between legs.
		const AWheeledVehiclePawn* Wheeled = Cast<AWheeledVehiclePawn>(MyPawn);
		const UChaosVehicleMovementComponent* Movement = Wheeled ? Wheeled->GetVehicleMovementComponent() : nullptr;
		const float SignedKph = Movement ? Movement->GetForwardSpeed() * 0.036f : Vehicle->GetSpeedKph();
		Turn = KTurn.Tick(DeltaSeconds, GoalDeg, SignedKph, IsBumperBlocked(MyPawn, 1.f), IsBumperBlocked(MyPawn, -1.f));
	}
	if (Turn.bActive)
	{
		Vehicle->Steer(Turn.Steer);
		Vehicle->MoveForward(Turn.Throttle);
		Stuck = IronAI::StuckDetector(); // Slow on purpose: not a stall.
	}
	else
	{
		if (ReverseTimeLeft > 0.f)
		{
			// Backing away from whatever pinned us, wheels turned so the nose swings toward the target.
			ReverseTimeLeft -= DeltaSeconds;
			Vehicle->Steer(ReverseSteer);
			Vehicle->MoveForward(-1.f);
			return;
		}

		float ChaseSteer = SteerSign * SteerMagnitude;
		if (bFollowingRoute)
		{
			// Proportional steering onto the carrot keeps the car on the road's centre line.
			const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(GoalDot, -1.f, 1.f))) * SteerSign;
			ChaseSteer = IronRoute::SteerForAngle(Degrees);
		}
		Vehicle->Steer(AvoidSteer != 0.f ? AvoidSteer : ChaseSteer);

		// Speed-managed approach (AIRules.h): fast from afar, slowing as the gap closes and braking
		// before the stopping distance, so physics cars engage instead of ramming the player.
		IronAI::DriveTuning Tuning;
		Tuning.StoppingDistance = StoppingDistance;
		float DesiredKph = IronAI::DesiredSpeedKph(DriveDistance, Tuning);
		if (bFollowingRoute)
		{
			// Brake for the corner instead of sliding into the building on it.
			DesiredKph = IronRoute::CornerSpeedKph(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(GoalDot, -1.f, 1.f))), DesiredKph);
			// Weaving round a wreck or barricade: slow enough to steer precisely.
			if (bWeaving)
			{
				DesiredKph = FMath::Min(DesiredKph, 35.f);
			}
			// ...and brake ahead of the next junction turn, not in it (at 70 km/h a car that starts
			// turning at the junction overshoots onto the far pavement).
			DesiredKph = FMath::Min(DesiredKph, IronRoute::RouteSpeedLimitKph(Route, RouteIndex, To2D(MyPawn->GetActorLocation()), DesiredKph));
		}
		const float SpeedKph = Vehicle->GetSpeedKph();
		Vehicle->MoveForward(IronAI::ThrottleFor(SpeedKph, DesiredKph, Tuning));

		if (Stuck.Update(DesiredKph, SpeedKph, DeltaSeconds))
		{
			ReverseTimeLeft = ReverseSeconds;
			ReverseSteer = -SteerSign; // Reversing inverts steering, so this turns the nose toward the target.
		}
		// Parked at its stand-off but not lined up on a structure: the player drives into a stopped
		// car's sights sooner or later, a relay never does. Same manoeuvre: back off on opposite lock
		// and come in again.
		else if (DesiredKph <= 0.f && SpeedKph < 3.f && FacingDot < FireAimDotThreshold && !Target->IsA<APawn>())
		{
			ReverseTimeLeft = ReverseSeconds;
			ReverseSteer = -SteerSign;
		}
	}

	if (UMachineGunComponent* Gun = Vehicle->GetPrimaryWeaponComponent())
	{
		if (Gun->GetAmmo() <= 0)
		{
			Vehicle->ReloadPrimary();
		}
	}

	const bool bInBurst = Burst.Update(DeltaSeconds, BurstSeconds, BurstPauseSeconds);

	if (bInBurst && Distance <= FireRange && FacingDot >= FireAimDotThreshold)
	{
		// Aim at the target with a random cone of error so the AI is beatable, not a laser.
		const FVector Spread = FMath::VRandCone(ToTargetDir, FMath::DegreesToRadians(AimSpreadDeg));
		Vehicle->FirePrimaryAt(MyPawn->GetActorLocation() + Spread * Distance);
	}

	AWarVehiclePawn* War = Cast<AWarVehiclePawn>(MyPawn);

	// Missile hunter: hangs back (its StoppingDistance is long) and lobs one guided missile at a
	// time - its pod needs only a rough bearing, the missile does the rest.
	if (War && War->bAIGuidedMissiles)
	{
		MissileCooldown -= DeltaSeconds;
		URocketLauncherComponent* Launcher = Vehicle->GetSecondaryWeaponComponent();
		if (Launcher && Launcher->GetAmmo() <= 0)
		{
			Vehicle->ReloadSecondary();
		}
		if (MissileCooldown <= 0.f && Distance >= 1500.f && Distance <= 9000.f && FacingDot >= 0.75f)
		{
			Vehicle->FireSecondary();
			MissileCooldown = MissileInterval * FMath::FRandRange(0.85f, 1.2f);
		}
	}

	// Railgun lancer: from well back, lined up, charges at where the player is now - the charge whine
	// and glow are the tell, and a swerve during it makes the slug miss.
	if (War && War->bAIRailgun && War->GetRailgun())
	{
		EnergyCooldown -= DeltaSeconds;
		URailgunComponent* Rail = War->GetRailgun();
		if (Rail->GetAmmo() <= 0)
		{
			Rail->StartReload();
		}
		if (EnergyCooldown <= 0.f && Distance >= 1500.f && Distance <= 12000.f && FacingDot >= 0.9f && !War->IsRailgunCharging())
		{
			War->FireRailgunAt(Target->GetActorLocation() + FVector(0.f, 0.f, 80.f));
			EnergyCooldown = FMath::FRandRange(5.f, 6.5f);
		}
	}
	// Tesla stormer: rushes in (short stopping distance) and zaps once in reach.
	if (War && War->bAITesla && War->GetTesla())
	{
		EnergyCooldown -= DeltaSeconds;
		UTeslaComponent* Coil = War->GetTesla();
		if (Coil->GetAmmo() <= 0)
		{
			Coil->StartReload();
		}
		if (EnergyCooldown <= 0.f && Distance <= 1900.f && FacingDot >= 0.6f)
		{
			War->FireTesla();
			EnergyCooldown = FMath::FRandRange(3.2f, 4.2f);
		}
	}

	// Rocket-armed AI (the boss): fire when lined up, reload when empty.
	if (War && War->bAIFiresRockets)
	{
		if (URocketLauncherComponent* Launcher = Vehicle->GetSecondaryWeaponComponent())
		{
			if (Launcher->GetAmmo() <= 0)
			{
				Vehicle->ReloadSecondary();
			}
		}
		if (Distance <= 4500.f && FacingDot >= 0.96f)
		{
			Vehicle->FireSecondary();
		}
	}
}

void AIronSiegeAIController::UpdateRoute(APawn* MyPawn, AActor* Target, float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!bStreetChecked && World)
	{
		bStreetChecked = true;
		for (TActorIterator<AIronCityStreet> It(World); It; ++It)
		{
			TArray<IronRoute::Road> Roads;
			for (const FIronRoadSegment& Seg : It->GetRoadSegments())
			{
				Roads.Add({ { float(Seg.A.X), float(Seg.A.Y) }, { float(Seg.B.X), float(Seg.B.Y) }, Seg.Width });
			}
			StreetGraph = MakeUnique<IronRoute::Graph>();
			*StreetGraph = IronRoute::Build(Roads.GetData(), Roads.Num());
			break;
		}
		for (TActorIterator<AIronArena> It(World); It; ++It)
		{
			for (const FVector& O : It->GetObstacles())
			{
				StreetObstacles.Add({ { float(O.X), float(O.Y) }, float(O.Z) });
			}
		}
	}
	if (!StreetGraph || !bStreetRoutingEnabled || !World)
	{
		bFollowingRoute = false;
		return;
	}
	RouteTimer -= DeltaSeconds;
	if (RouteTimer <= 0.f)
	{
		RouteTimer = 0.5f;
		// Is there a building between us? Only static geometry counts - other cars do not.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeAIRoute), false, MyPawn);
		Params.AddIgnoredActor(Target);
		const FVector Eye(0.f, 0.f, 150.f);
		const bool bBlocked = World->LineTraceTestByObjectType(MyPawn->GetActorLocation() + Eye, Target->GetActorLocation() + Eye, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		// On a street map keep to the roads for any long chase, even with a clear view down the
		// avenue - the straight chase clips kerbs and props; only close in directly when near.
		const bool bRoute = bBlocked || FVector::Dist2D(MyPawn->GetActorLocation(), Target->GetActorLocation()) > 2500.f;
		// Replan only when the player has moved on or the car has left the route (knocked off it,
		// or recovering from a stall) - keeping the route keeps the corner it just took in view, so
		// the corner speed holds until the car is out of the turn.
		const IronRoute::Vec2 Car = To2D(MyPawn->GetActorLocation());
		const bool bOffRoute = Route.Count > 0 && RouteIndex > 0
			&& IronRoute::DistToSegment(Route.Points[RouteIndex - 1], Route.Points[RouteIndex], Car) > 900.f;
		const bool bReplan = !bFollowingRoute || Route.Count == 0 || bOffRoute
			|| FVector::Dist2D(Target->GetActorLocation(), PlannedFor) > 800.f;
		bFollowingRoute = bRoute;
		if (bRoute && bReplan)
		{
			Route = IronRoute::FindPath(*StreetGraph, Car, To2D(Target->GetActorLocation()));
			RouteIndex = 0;
			PlannedFor = Target->GetActorLocation();
		}
	}
	if (bFollowingRoute && Route.Count > 0)
	{
		RouteIndex = IronRoute::Advance(Route, RouteIndex, To2D(MyPawn->GetActorLocation()), 900.f);
	}
}

bool AIronSiegeAIController::IsBumperBlocked(APawn* MyPawn, float Direction) const
{
	UWorld* World = GetWorld();
	if (!World || !MyPawn)
	{
		return false;
	}
	const FVector Axis = MyPawn->GetActorForwardVector().GetSafeNormal2D() * Direction;
	const FVector Bumper = MyPawn->GetActorLocation() + Axis * 230.f;
	// The kerb is only 15 cm high - no probe feels it - so "blocked" also means the bumper is about
	// to leave the carriageway (the road graph knows each road's width).
	if (StreetGraph)
	{
		const IronRoute::Vec2 Ahead = To2D(Bumper + Axis * 120.f);
		const IronRoute::Anchor A = IronRoute::Locate(*StreetGraph, Ahead);
		if (A.Road >= 0 && A.Distance > StreetGraph->Roads[A.Road].Width * 0.5f - 40.f)
		{
			return true;
		}
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSiegeAIBumper), false, MyPawn);
	FHitResult Hit;
	const FVector Up(0.f, 0.f, 100.f);
	return World->SweepSingleByChannel(Hit, Bumper + Up, Bumper + Up + Axis * 200.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(60.f), Params);
}
