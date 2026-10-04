#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IronSiegeEditorUtils.generated.h"

// Small helpers for the Content/Python setup scripts: things Python's reflection cannot do in bulk.
UCLASS()
class IRONSIEGE_API UIronSiegeEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Copies every editable (Details-panel) property value from Source to Dest. Both must be the
	// same class or Dest a subclass of Source's class. Used to carry the Vehicle Template's whole
	// tuned Chaos setup (wheels, engine, transmission, steering, suspension...) onto our pawns.
	// Returns the number of properties copied.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Editor")
	static int32 CopyEditableProperties(UObject* Source, UObject* Dest);

	// Points a UFont asset at font-face assets as its Regular/Bold typefaces. Python cannot reach
	// into FCompositeFont (its typeface entries are not exposed), so the UI font is wired up here.
	// Returns false if the font or the regular face is missing.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Editor")
	static bool SetupCompositeFont(class UFont* Font, UObject* RegularFace, UObject* BoldFace);

	// Removes every node from a Blueprint's event graphs and recompiles it. Vehicle Variety Pack cars
	// read their own input in their event graph and set the throttle/steering every tick, which
	// overrides AWarVehiclePawn's (player and AI) inputs once reparented. Returns nodes removed.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Editor")
	static int32 ClearEventGraphs(class UBlueprint* Blueprint);
};
