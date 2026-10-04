#include "IronMissionDirector.h"
#include "CrewRules.h"
#include "IronArena.h"
#include "IronCityStreet.h"
#include "IronMissionActors.h"
#include "IronSiegeAIController.h"
#include "IronSiegeGameMode.h"
#include "IronSiegeHUD.h"
#include "IronSiegeStory.h"
#include "IronSiegeText.h"
#include "IronTeams.h"
#include "VehicleHealthComponent.h"
#include "WarVehiclePawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
using IronMissions::Objective;

// Room a stage's spot needs around it, by what stands there.
float ClearanceFor(Objective Kind)
{
	switch (Kind)
	{
	case Objective::DestroyTargets:
	case Objective::Defend: return 750.f;
	default: return 450.f;
	}
}
}

AIronMissionDirector::AIronMissionDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

const IronMissions::Stage& AIronMissionDirector::GetStage() const
{
	const IronMissions::Mission& M = GetMission();
	return M.Stages[FMath::Clamp(StageIndex, 0, M.NumStages - 1)];
}

AIronSiegeHUD* AIronMissionDirector::Hud() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetHUD<AIronSiegeHUD>() : nullptr;
}

void AIronMissionDirector::Say(const IronMissions::Radio& Line) const
{
	if (AIronSiegeHUD* HUD = Hud())
	{
		HUD->Say(Line.Speaker, Line.Mood, Line.Key, Line.Text, true);
	}
}

void AIronMissionDirector::Notice(const TCHAR* Key, const TCHAR* English, const FLinearColor& Color, const FString& Suffix) const
{
	if (AIronSiegeHUD* HUD = Hud())
	{
		HUD->ShowNotice(IronText::Str(Key, English) + Suffix, Color);
	}
}

void AIronMissionDirector::Begin(int32 InMissionIndex, APawn* Player)
{
	UWorld* World = GetWorld();
	MissionIndex = FMath::Clamp(InMissionIndex, 0, IronMissions::Count - 1);
	PlayerPawn = Player;
	GameMode = World ? World->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	for (TActorIterator<AIronArena> It(World); It; ++It)
	{
		Obstacles = It->GetObstacles();
		if (It->GetHalfExtent() > 0.f)
		{
			HalfExtent = It->GetHalfExtent();
		}
	}
	for (TActorIterator<AIronCityStreet> It(World); It; ++It)
	{
		RoadPoints = It->GetRoadSpawnPoints();
	}
	Result = EIronMissionResult::Running;
	Elapsed = 0.f;
	const IronMissions::Mission& M = GetMission();
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d (%s) begins - %d stage(s), half extent %.0f, %d obstacle(s), %d road point(s)"),
		MissionIndex + 1, ANSI_TO_TCHAR(M.Name), M.NumStages, HalfExtent, Obstacles.Num(), RoadPoints.Num());
	StartStage(0);
	// The driver answers the commander (queued behind her line).
	if (AIronSiegeHUD* HUD = Hud())
	{
		HUD->Bark(static_cast<int32>(IronCrew::Bark::Deploy), true);
	}
}

float AIronMissionDirector::GroundZ(const FVector2D& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronMissionGround), false);
	const FVector Top(At.X, At.Y, 2500.f), Bottom(At.X, At.Y, -1000.f);
	return GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? float(Hit.ImpactPoint.Z) : 0.f;
}

FVector AIronMissionDirector::Resolve(const IronMissions::Spot& Spot, float Clearance) const
{
	FVector2D P(Spot.X * HalfExtent, Spot.Y * HalfExtent);
	if (RoadPoints.Num() > 0)
	{
		// City: everything happens on the streets.
		FVector Best = RoadPoints[0];
		for (const FVector& Road : RoadPoints)
		{
			if (FVector2D::DistSquared(FVector2D(Road), P) < FVector2D::DistSquared(FVector2D(Best), P))
			{
				Best = Road;
			}
		}
		return FVector(Best.X, Best.Y, GroundZ(FVector2D(Best)));
	}
	// Open arenas: push the spot out of any prop it landed in (a few passes, since leaving one rock
	// can land it in the next), and keep it off the walls.
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		for (const FVector& Prop : Obstacles)
		{
			const FVector2D Centre(Prop.X, Prop.Y);
			const float Need = float(Prop.Z) + Clearance;
			const FVector2D Away = P - Centre;
			const float Distance = float(Away.Size());
			if (Distance < Need)
			{
				P = Centre + (Distance > 1.f ? Away / Distance : FVector2D(1.f, 0.f)) * Need;
			}
		}
	}
	const float Limit = HalfExtent - 1100.f;
	P.X = FMath::Clamp(P.X, -Limit, Limit);
	P.Y = FMath::Clamp(P.Y, -Limit, Limit);
	return FVector(P.X, P.Y, GroundZ(P));
}

void AIronMissionDirector::ClearZones()
{
	for (AIronCaptureZone* Zone : Zones)
	{
		if (Zone)
		{
			Zone->Destroy();
		}
	}
	Zones.Reset();
}

void AIronMissionDirector::StartStage(int32 Index)
{
	UWorld* World = GetWorld();
	APawn* Player = PlayerPawn.Get();
	if (!World || !Player || !GameMode)
	{
		return;
	}
	ClearZones();
	Targets.Reset(); // Wrecks of the last stage stay on the field; they are just no longer objectives.
	Trucks.Reset();
	TruckDone.Reset();
	TruckWarned.Reset();
	bRelayWarned = false;
	StageIndex = Index;
	State = IronMissions::StageState();
	Spawned = SentAtAsset = 0;
	AssetAttackers.Reset();
	ZonesTaken = TargetsDown = 0;
	Boss.Reset();
	const IronMissions::Stage& S = GetStage();
	Points.Reset();
	for (int32 i = 0; i < S.NumSpots; ++i)
	{
		Points.Add(Resolve(S.Spots[i], ClearanceFor(S.Kind)));
	}

	switch (S.Kind)
	{
	case Objective::Reach:
	{
		// Of the spots offered, the one farthest from where the player is: a drive, whichever start they got.
		FVector Far = Points.Num() > 0 ? Points[0] : Player->GetActorLocation();
		for (const FVector& P : Points)
		{
			if (FVector::DistSquared2D(P, Player->GetActorLocation()) > FVector::DistSquared2D(Far, Player->GetActorLocation()))
			{
				Far = P;
			}
		}
		Zones.Add(AIronCaptureZone::Spawn(World, Far, false));
		break;
	}
	case Objective::Capture:
		for (const FVector& P : Points)
		{
			Zones.Add(AIronCaptureZone::Spawn(World, P, true));
		}
		break;
	case Objective::DestroyTargets:
	case Objective::Defend:
		for (const FVector& P : Points)
		{
			Targets.Add(AIronMissionTarget::Spawn(World, S.Structure, P, FMath::FRandRange(0.f, 360.f)));
		}
		break;
	case Objective::Escort:
		SpawnConvoy(true);
		break;
	case Objective::Intercept:
		SpawnConvoy(false);
		break;
	case Objective::Boss:
	{
		const TArray<FVector> Where = GameMode->PickSpawnPoints(Player->GetActorLocation(), 3000.f, GameMode->EnemySpawnRadius * 1.8f, 1);
		Boss = GameMode->SpawnMissionBoss(S.Boss, Where[0], (Player->GetActorLocation() - Where[0]).GetSafeNormal2D().Rotation());
		break;
	}
	default:
		break;
	}

	// Resupply between stages (the campaign has no shop): a repair kit and an ammo crate near the
	// player, and a second kit before a boss.
	if (Index > 0)
	{
		GameMode->DropSupplies(S.Kind == Objective::Boss ? 2 : 1, 1);
	}
	// Whoever survived the last stage is still on the field: the new arrivals fill up to the stage's
	// cap rather than doubling the fight (always at least a pair, so a stage never opens empty).
	SpawnSquad(IronMissions::Fit(S.Opening, FMath::Max(S.MaxAlive - GameMode->GetEnemiesAlive(), 2)), S.bGuardSpots);
	for (int32 i = 0; i < S.NumLines; ++i)
	{
		Say(S.Lines[i]);
	}
	if (AIronSiegeHUD* HUD = Hud())
	{
		const FString Head = Index == 0
			? FString::Printf(TEXT("%s %d: %s"), *IronText::Str(TEXT("MisMission"), TEXT("MISSION")), MissionIndex + 1, *IronStory::MissionName(MissionIndex))
			: IronText::Str(TEXT("MisNew"), TEXT("NEW OBJECTIVE"));
		HUD->ShowNotice(Head, FLinearColor(1.f, 0.8f, 0.3f));
		HUD->ShowNotice(IronText::Str(ANSI_TO_TCHAR(S.TextKey), ANSI_TO_TCHAR(S.Text)), FLinearColor::White);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d stage %d/%d - %s (%d zone(s), %d structure(s), %d truck(s), %d enemy(ies))"), MissionIndex + 1, Index + 1,
		GetMission().NumStages, ANSI_TO_TCHAR(S.Text), Zones.Num(), Targets.Num(), Trucks.Num(), Spawned);
}

void AIronMissionDirector::SpawnConvoy(bool bFriendly)
{
	const IronMissions::Stage& S = GetStage();
	if (Points.Num() < 2)
	{
		return;
	}
	const IronMissions::ConvoyTuning Tuning;
	bFriendlyConvoy = bFriendly;
	// Nose to tail behind the first point of the route, facing down its first leg.
	const FVector Dir = (Points[1] - Points[0]).GetSafeNormal2D();
	for (int32 i = 0; i < S.Count; ++i)
	{
		FVector At = Points[0] - Dir * Tuning.Spacing * i;
		At.Z = GroundZ(FVector2D(At)) + 60.f;
		AWarVehiclePawn* Truck = GameMode->SpawnTruck(bFriendly, At, Dir.Rotation(), bFriendly ? Tuning.FriendlyDurability : Tuning.EnemyDurability);
		if (!Truck)
		{
			continue;
		}
		if (AIronSiegeAIController* Driver = Cast<AIronSiegeAIController>(Truck->GetController()))
		{
			Driver->DriveRoute(Points, bFriendly ? Tuning.EscortKph : Tuning.InterceptKph);
		}
		Trucks.Add(Truck);
		TruckDone.Add(false);
		TruckWarned.Add(false);
	}
	// A truck that could not be spawned must not leave the stage waiting for it for ever.
	State.Progress += S.Count - Trucks.Num();
	// Where the route ends, marked on the ground.
	Zones.Add(AIronCaptureZone::Spawn(GetWorld(), Points.Last(), false));
}

AActor* AIronMissionDirector::FindAsset() const
{
	const IronMissions::Stage& S = GetStage();
	if (S.Kind == Objective::Defend)
	{
		return Targets.Num() > 0 ? Targets[0].Get() : nullptr;
	}
	if (S.Kind == Objective::Escort)
	{
		// Spread the attackers over the trucks still driving.
		TArray<AActor*> Live;
		for (int32 i = 0; i < Trucks.Num(); ++i)
		{
			if (!TruckDone[i] && IronTeams::IsAlive(Trucks[i]))
			{
				Live.Add(Trucks[i]);
			}
		}
		return Live.Num() > 0 ? Live[SentAtAsset % Live.Num()] : nullptr;
	}
	return nullptr;
}

void AIronMissionDirector::SpawnSquad(const IronMissions::Squad& Squad, bool bGuard)
{
	APawn* Player = PlayerPawn.Get();
	if (!Player || !GameMode)
	{
		return;
	}
	TArray<int32> Kinds;
	const int32 Counts[5] = { Squad.Buggies, Squad.Raiders, Squad.Hunters, Squad.Stormers, Squad.Lancers };
	for (int32 Kind = 0; Kind < 5; ++Kind)
	{
		for (int32 i = 0; i < Counts[Kind]; ++i)
		{
			Kinds.Add(Kind);
		}
	}
	if (Kinds.Num() == 0)
	{
		return;
	}
	const IronMissions::Stage& S = GetStage();
	AActor* Asset = S.AssetShare > 0.f ? FindAsset() : nullptr;
	// Where they come from: posted round the stage's spots (guards), or arriving round whatever
	// they are after - the trucks, the relay, otherwise the player.
	TArray<FVector> Where;
	if (bGuard && Points.Num() > 0)
	{
		for (int32 i = 0; i < Kinds.Num(); ++i)
		{
			Where.Append(GameMode->PickSpawnPoints(Points[i % Points.Num()], 500.f, 1700.f, 1));
		}
	}
	else
	{
		const FVector Origin = Asset ? Asset->GetActorLocation() : Player->GetActorLocation();
		Where = GameMode->PickSpawnPoints(Origin, 1800.f, GameMode->EnemySpawnRadius * 1.6f, Kinds.Num());
	}
	for (int32 i = 0; i < Kinds.Num() && i < Where.Num(); ++i)
	{
		const FVector Facing = Player->GetActorLocation() - Where[i];
		APawn* Enemy = GameMode->SpawnEnemyOfKind(Kinds[i], Where[i], Facing.GetSafeNormal2D().Rotation());
		++Spawned;
		// Gun cars only (kinds 0 and 1): the specialists' weapons are made for the player's car - a
		// hunter's missiles home on it whatever the hunter is told to chase.
		if (!Enemy || Kinds[i] >= 2 || S.AssetShare <= 0.f || SentAtAsset >= Spawned * S.AssetShare)
		{
			continue;
		}
		// No more on the trucks / relay at once than the stage allows, whoever has been destroyed so far.
		AssetAttackers.RemoveAll([](const TWeakObjectPtr<APawn>& Car) { return !IronTeams::IsAlive(Car.Get()); });
		if (AssetAttackers.Num() >= IronMissions::MaxOnAsset(S))
		{
			continue;
		}
		if (AIronSiegeAIController* Driver = Cast<AIronSiegeAIController>(Enemy->GetController()))
		{
			if (AActor* Wanted = FindAsset())
			{
				Driver->SetPreferredTarget(Wanted);
				AssetAttackers.Add(Enemy);
				++SentAtAsset;
			}
		}
	}
}

void AIronMissionDirector::NotifyEnemyKilled(APawn* Enemy)
{
	if (Result == EIronMissionResult::Running && StageIndex >= 0 && GetStage().Kind == Objective::Eliminate && Enemy && !Enemy->ActorHasTag(TEXT("Truck")))
	{
		++State.Progress;
	}
}

void AIronMissionDirector::NotifyPlayerDestroyed()
{
	if (Result == EIronMissionResult::Running)
	{
		Result = EIronMissionResult::Failed;
		UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d failed - the player's car was destroyed (stage %d, %.0f s)"), MissionIndex + 1, StageIndex + 1, Elapsed);
	}
}

void AIronMissionDirector::PollObjective()
{
	const IronMissions::Stage& S = GetStage();
	const FLinearColor Good(0.45f, 1.f, 0.5f), Bad(1.f, 0.35f, 0.25f);
	switch (S.Kind)
	{
	case Objective::Reach:
		State.Progress = Zones.Num() > 0 && Zones[0] && Zones[0]->IsTaken() ? 1 : 0;
		break;
	case Objective::Capture:
	{
		int32 Taken = 0;
		for (const AIronCaptureZone* Zone : Zones)
		{
			Taken += Zone && Zone->IsTaken() ? 1 : 0;
		}
		if (Taken > ZonesTaken)
		{
			Notice(TEXT("MisSiteTaken"), TEXT("SITE CAPTURED"), Good, FString::Printf(TEXT("  %d/%d"), Taken, S.Count));
		}
		ZonesTaken = State.Progress = Taken;
		break;
	}
	case Objective::DestroyTargets:
	{
		int32 Down = 0;
		for (const AIronMissionTarget* Target : Targets)
		{
			Down += !Target || Target->IsDestroyed() ? 1 : 0;
		}
		if (Down > TargetsDown)
		{
			Notice(TEXT("MisTargetDown"), TEXT("TARGET DESTROYED"), Good, FString::Printf(TEXT("  %d/%d"), Down, S.Count));
		}
		TargetsDown = State.Progress = Down;
		break;
	}
	case Objective::Defend:
		if (!State.bAssetLost && (Targets.Num() == 0 || !Targets[0] || Targets[0]->IsDestroyed()))
		{
			State.bAssetLost = true;
			++AssetsLost;
			Notice(TEXT("MisRelayLost"), TEXT("THE RELAY IS LOST"), Bad);
		}
		else if (!bRelayWarned && Targets.Num() > 0 && Targets[0] && Targets[0]->GetHealthFraction() < 0.5f)
		{
			bRelayWarned = true;
			Notice(TEXT("MisRelayHalf"), TEXT("RELAY AT HALF STRENGTH"), Bad);
		}
		break;
	case Objective::Escort:
	case Objective::Intercept:
		for (int32 i = 0; i < Trucks.Num(); ++i)
		{
			if (TruckDone[i])
			{
				continue;
			}
			AWarVehiclePawn* Truck = Trucks[i];
			const AIronSiegeAIController* Driver = Truck ? Cast<AIronSiegeAIController>(Truck->GetController()) : nullptr;
			if (bFriendlyConvoy && !TruckWarned[i] && IronTeams::IsAlive(Truck))
			{
				const UVehicleHealthComponent* Health = Truck->GetHealthComponent();
				if (Health->GetHealth() + Health->GetArmor() < 0.5f * (Health->GetMaxHealth() + Health->GetMaxArmor()))
				{
					TruckWarned[i] = true;
					Notice(TEXT("MisTruckHalf"), TEXT("A TRUCK IS BADLY DAMAGED"), Bad);
				}
			}
			if (!IronTeams::IsAlive(Truck))
			{
				TruckDone[i] = true;
				if (bFriendlyConvoy)
				{
					++State.Lost;
					++AssetsLost;
					Notice(TEXT("MisTruckLost"), TEXT("TRUCK LOST"), Bad);
				}
				else
				{
					++State.Progress;
					Notice(TEXT("MisTruckStopped"), TEXT("TRUCK STOPPED"), Good, FString::Printf(TEXT("  %d/%d"), State.Progress, S.Need));
				}
			}
			else if (Driver && Driver->HasArrived())
			{
				TruckDone[i] = true;
				if (bFriendlyConvoy)
				{
					++State.Progress;
					Notice(TEXT("MisTruckHome"), TEXT("TRUCK THROUGH THE GATE"), Good);
					Truck->SetLifeSpan(2.5f);
				}
				else
				{
					++State.Lost;
					Notice(TEXT("MisTruckAway"), TEXT("A TRUCK GOT AWAY"), Bad);
					Truck->Destroy();
				}
			}
		}
		break;
	case Objective::Boss:
		State.Progress = IronTeams::IsAlive(Boss.Get()) ? 0 : 1;
		break;
	default:
		break; // Eliminate counts through NotifyEnemyKilled; Survive only needs the clock.
	}
}

void AIronMissionDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Result != EIronMissionResult::Running || StageIndex < 0 || !GameMode)
	{
		return;
	}
	if (StageBreak > 0.f)
	{
		// A breath between stages, while the radio catches up.
		StageBreak -= DeltaSeconds;
		if (StageBreak <= 0.f)
		{
			StartStage(PendingStage);
		}
		return;
	}
	Elapsed += DeltaSeconds;
	PollObjective();
	const IronMissions::Stage& S = GetStage();
	const IronMissions::Squad More = IronMissions::Tick(S, State, DeltaSeconds, GameMode->GetEnemiesAlive());
	if (More.Total() > 0)
	{
		SpawnSquad(More, false);
	}
	const IronMissions::Outcome Outcome = bDebugSkip ? IronMissions::Outcome::Complete : IronMissions::Evaluate(S, State);
	bDebugSkip = false;
	if (Outcome == IronMissions::Outcome::Complete)
	{
		FinishStage();
	}
	else if (Outcome == IronMissions::Outcome::Failed)
	{
		Fail();
	}
}

void AIronMissionDirector::FinishStage()
{
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d stage %d complete at %.0f s (progress %d, lost %d)"), MissionIndex + 1, StageIndex + 1, Elapsed, State.Progress, State.Lost);
	ClearZones();
	if (StageIndex + 1 >= GetMission().NumStages)
	{
		Win();
		return;
	}
	Notice(TEXT("MisObjDone"), TEXT("OBJECTIVE COMPLETE"), FLinearColor(0.45f, 1.f, 0.5f));
	PendingStage = StageIndex + 1;
	StageBreak = 2.5f;
}

void AIronMissionDirector::Win()
{
	Result = EIronMissionResult::Complete;
	const IronMissions::Mission& M = GetMission();
	const IIronVehicle* Vehicle = Cast<IIronVehicle>(PlayerPawn.Get());
	const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
	EndHealth = Health && Health->GetMaxHealth() > 0.f ? Health->GetHealth() / Health->GetMaxHealth() : 0.f;
	Stars = IronMissions::Stars(true, Elapsed, M.ParSeconds, EndHealth, AssetsLost);

	// Keep the best result; a first clear may bring a new driver into the squad.
	IronMissions::Progress Progress = IronStory::LoadProgress();
	bool bHadDriver[IronCrew::DriverCount];
	for (int32 d = 0; d < IronCrew::DriverCount; ++d)
	{
		bHadDriver[d] = IronMissions::IsDriverUnlocked(Progress, static_cast<IronCrew::Driver>(d));
	}
	bNewBest = Progress.Record(MissionIndex, Stars);
	if (bNewBest)
	{
		IronStory::SaveProgress(Progress);
	}
	for (int32 d = 0; d < IronCrew::DriverCount; ++d)
	{
		if (!bHadDriver[d] && IronMissions::IsDriverUnlocked(Progress, static_cast<IronCrew::Driver>(d)))
		{
			UnlockedDriver = d;
		}
	}
	for (int32 i = 0; i < M.NumOutro; ++i)
	{
		Say(M.Outro[i]);
	}
	if (AIronSiegeHUD* HUD = Hud())
	{
		HUD->Bark(static_cast<int32>(IronCrew::Bark::Victory), true);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d complete - %.0f s (par %.0f), health %.0f%%, %d asset(s) lost, %d star(s)%s"), MissionIndex + 1, Elapsed, M.ParSeconds,
		EndHealth * 100.f, AssetsLost, Stars, bNewBest ? TEXT(", new best") : TEXT(""));
	GameMode->EndMission(true);
}

void AIronMissionDirector::Fail()
{
	if (Result != EIronMissionResult::Running)
	{
		return;
	}
	Result = EIronMissionResult::Failed;
	if (AIronSiegeHUD* HUD = Hud())
	{
		HUD->Bark(static_cast<int32>(IronCrew::Bark::Defeat), true);
	}
	UE_LOG(LogTemp, Log, TEXT("IronSiege: mission %d failed at stage %d, %.0f s (progress %d, lost %d)"), MissionIndex + 1, StageIndex + 1, Elapsed, State.Progress, State.Lost);
	GameMode->EndMission(false);
}

FString AIronMissionDirector::GetObjectiveText() const
{
	if (StageIndex < 0)
	{
		return FString();
	}
	const IronMissions::Stage& S = GetStage();
	FString Text = IronText::Str(ANSI_TO_TCHAR(S.TextKey), ANSI_TO_TCHAR(S.Text));
	switch (S.Kind)
	{
	case Objective::Eliminate:
	case Objective::DestroyTargets:
	case Objective::Capture:
	case Objective::Escort:
		Text += FString::Printf(TEXT("   %d/%d"), FMath::Min(State.Progress, S.Count), S.Count);
		break;
	case Objective::Intercept:
		Text += FString::Printf(TEXT("   %d/%d"), FMath::Min(State.Progress, S.Need), S.Need);
		break;
	default:
		break;
	}
	return Text;
}

void AIronMissionDirector::GetMarkers(TArray<FIronMissionMarker>& Out) const
{
	if (Result != EIronMissionResult::Running || StageIndex < 0 || StageBreak > 0.f)
	{
		return;
	}
	const IronMissions::Stage& S = GetStage();
	const bool bConvoy = S.Kind == Objective::Escort || S.Kind == Objective::Intercept;
	for (const AIronMissionTarget* Target : Targets)
	{
		if (!Target || Target->IsDestroyed())
		{
			continue;
		}
		FIronMissionMarker M;
		M.Location = Target->GetMarkerLocation();
		M.bFriendly = Target->bFriendly;
		M.Label = Target->bFriendly ? IronText::Str(TEXT("MarkDefend"), TEXT("DEFEND")) : IronText::Str(TEXT("MarkTarget"), TEXT("TARGET"));
		M.Fraction = Target->GetHealthFraction();
		M.bAlert = Target->bFriendly && M.Fraction < 0.5f;
		Out.Add(M);
	}
	for (int32 i = 0; i < Trucks.Num(); ++i)
	{
		const AWarVehiclePawn* Truck = Trucks[i];
		const UVehicleHealthComponent* Health = Truck ? Truck->GetHealthComponent() : nullptr;
		if (TruckDone[i] || !Health || Health->IsDestroyed())
		{
			continue;
		}
		FIronMissionMarker M;
		M.Location = Truck->GetActorLocation() + FVector(0.f, 0.f, 420.f);
		M.bFriendly = bFriendlyConvoy;
		M.Label = bFriendlyConvoy ? IronText::Str(TEXT("MarkEscort"), TEXT("ESCORT")) : IronText::Str(TEXT("MarkTarget"), TEXT("TARGET"));
		const float Total = Health->GetMaxHealth() + Health->GetMaxArmor();
		M.Fraction = Total > 0.f ? (Health->GetHealth() + Health->GetArmor()) / Total : 0.f;
		M.bAlert = bFriendlyConvoy && M.Fraction < 0.5f;
		Out.Add(M);
	}
	for (const AIronCaptureZone* Zone : Zones)
	{
		if (!Zone || Zone->IsTaken())
		{
			continue;
		}
		FIronMissionMarker M;
		M.Location = Zone->GetActorLocation() + FVector(0.f, 0.f, 350.f);
		if (S.Kind == Objective::Capture)
		{
			M.bAlert = Zone->IsContested();
			M.Label = M.bAlert ? IronText::Str(TEXT("MarkContested"), TEXT("CONTESTED")) : IronText::Str(TEXT("MarkCapture"), TEXT("CAPTURE"));
			M.Fraction = Zone->GetProgress();
		}
		else
		{
			M.Label = bConvoy ? IronText::Str(TEXT("MarkGate"), TEXT("GATE")) : IronText::Str(TEXT("MarkGo"), TEXT("GO HERE"));
		}
		Out.Add(M);
	}
}
