#pragma once
#include "CoreMinimal.h"
#include "IronVehicleClass.generated.h"

// Mirrors IronVehicles::VehicleClass (VehicleClassRules.h) as a UENUM so it can be an
// EditDefaultsOnly property; ordinal order must stay identical to the plain-C++ enum.
// Used by AWarVehiclePawn.
UENUM(BlueprintType)
enum class EIronVehicleClass : uint8
{
	Scout,
	Assault,
	Heavy,
	Artillery,
	Interceptor,
	Dune
};
