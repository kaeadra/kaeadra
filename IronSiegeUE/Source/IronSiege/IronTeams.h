#pragma once
#include "CoreMinimal.h"

class AActor;
class AController;

// Two sides: the player's (their car, the trucks they escort, the relay they defend) and the
// Legion's (everything else the AI drives or that a mission stands up as a target).
namespace IronTeams
{
IRONSIEGE_API bool IsPlayerSide(const AActor* Actor);

// Side of whoever dealt some damage: the player's controller, or an AI driving a friendly truck.
IRONSIEGE_API bool IsPlayerSideController(const AController* Controller);

// Still worth shooting at: a vehicle that has not been destroyed, or a mission structure still standing.
IRONSIEGE_API bool IsAlive(const AActor* Actor);
}
