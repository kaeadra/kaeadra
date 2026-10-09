#include "IronTeams.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "IronMissionActors.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "WarVehiclePawn.h"

namespace IronTeams
{
bool IsPlayerSide(const AActor* Actor)
{
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (Pawn->IsPlayerControlled())
		{
			return true;
		}
		const AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(Pawn);
		return Car && Car->bPlayerSide;
	}
	const AIronMissionTarget* Structure = Cast<AIronMissionTarget>(Actor);
	return Structure && Structure->bFriendly;
}

bool IsPlayerSideController(const AController* Controller)
{
	if (!Controller)
	{
		return false;
	}
	return Controller->IsPlayerController() || IsPlayerSide(Controller->GetPawn());
}

bool IsAlive(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	if (const IIronVehicle* Vehicle = Cast<IIronVehicle>(Actor))
	{
		const UVehicleHealthComponent* Health = Vehicle->GetHealthComponent();
		return Health && !Health->IsDestroyed();
	}
	if (const AIronMissionTarget* Structure = Cast<AIronMissionTarget>(Actor))
	{
		return !Structure->IsDestroyed();
	}
	return false;
}
}
