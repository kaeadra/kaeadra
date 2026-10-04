#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IronSiegePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class IIronVehicle;

// Which canvas screen the HUD draws and the menu keys (F3/F4 choose, Enter confirms, F2 goes back)
// drive. A level opened from the menus arrives part-way down this list.
UENUM()
enum class EIronScreen : uint8
{
	ModeSelect,    // Campaign or survival.
	MapSelect,     // Survival: which battlefield.
	MissionSelect, // Campaign: which mission.
	Briefing,      // Campaign: the commander's briefing for this level's mission.
	DriverSelect,
	VehicleSelect,
	Playing
};

// Enhanced Input actions/mapping context are asset references assigned in a Blueprint child of
// this class (or in this class's defaults once the assets exist) - see
// Content/Python/setup_input_and_maps.py for a script that creates them, and Docs/GDD_AR.md
// for the manual Content Browser steps if that script's asset factory names drift.
UCLASS()
class IRONSIEGE_API AIronSiegePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AIronSiegePlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> BoostAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> DeployMineAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> FlamerAction;

	// Countermeasure flares. Created at runtime if no asset is assigned (the runtime mapping
	// context maps it from the user's key bindings like every other action).
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> FlaresAction;

	// Shop energy weapons; created at runtime like FlaresAction.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> RailgunAction;

	// The driver's ability (CrewRules.h); created at runtime like FlaresAction.
	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> AbilityAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> TeslaAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> FirePrimaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> FireSecondaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> ReloadPrimaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> ReloadSecondaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "IronSiege|Input")
	TObjectPtr<UInputAction> ToggleCameraAction;

	// Settings menu (SIronSettingsMenu, Slate): Esc / F1 opens it from anywhere; pauses gameplay.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Settings")
	bool IsSettingsMenuOpen() const { return SettingsWidget.IsValid(); }

	void OpenSettingsMenu();
	TSharedPtr<class SIronSettingsMenu> GetSettingsMenu() const { return SettingsWidget; }
	void CloseSettingsMenu();

	// Pushes UIronSiegeUserSettings into the running game: audio mix, brightness, key bindings,
	// camera and steering on the possessed car. Called on start, possession and every menu change.
	void ApplyUserSettings();

	// Controller vibration for a game event (0..1 strength), scaled by the player's setting.
	void PlayRumble(float Strength, float Duration);

	// Free camera for screenshots: hides the HUD, slows time to a crawl and hands control to a
	// flying camera pawn. Toggled with P (or the pad's Back button).
	void TogglePhotoMode();
	bool IsPhotoMode() const { return PhotoPawn != nullptr; }

	// Measures average FPS for a few seconds of real gameplay, then moves the quality preset
	// toward the target frame rate (see IronSettings::RecommendedQuality).
	void RunBenchmark(float Seconds, float TargetFps);

	// The menus (canvas-drawn by AIronSiegeHUD): mode, then battlefield or mission and its briefing,
	// then driver and vehicle. Choosing a map or mission travels there with "?SkipMenu=1" (and
	// "?Mission=N") so the player lands on the next screen instead of at the top again.
	EIronScreen GetScreen() const { return Screen; }
	int32 GetModeIndex() const { return ModeIndex; }             // 0 campaign, 1 survival.
	int32 GetMissionCursor() const { return MissionCursor; }
	int32 GetPendingDriverIndex() const { return PendingDriverIndex; }
	int32 GetResultIndex() const { return ResultIndex; }         // Cursor on the mission result screen.

	// Options on the mission result screen, in order: 0 next mission, 1 retry, 2 main menu (a lost
	// mission, and the last one, have no "next").
	TArray<int32> GetResultOptions() const;

	// Testing: jump to a menu screen, press a menu key (0 F3, 1 F4, 2 Enter, 3 F2), choose the driver
	// the next deploy uses.
	void DebugSetScreen(int32 InScreen) { Screen = static_cast<EIronScreen>(FMath::Clamp(InScreen, 0, static_cast<int32>(EIronScreen::Playing))); }
	void DebugMenuKey(int32 Key);
	void SetPendingDriver(int32 Driver);

	UFUNCTION(BlueprintCallable, Category = "IronSiege|MainMenu")
	bool IsMainMenuActive() const { return Screen == EIronScreen::ModeSelect || Screen == EIronScreen::MapSelect; }

	UFUNCTION(BlueprintCallable, Category = "IronSiege|MainMenu")
	FString GetSelectedMapName() const;

	// Vehicle class select screen (canvas-drawn by AIronSiegeHUD), shown before any pawn is
	// spawned. Left/Right cycles Scout/Assault/Heavy/Artillery, Enter confirms and spawns.
	UFUNCTION(BlueprintCallable, Category = "IronSiege|VehicleSelect")
	bool IsVehicleSelected() const { return Screen == EIronScreen::Playing; }

	UFUNCTION(BlueprintCallable, Category = "IronSiege|VehicleSelect")
	int32 GetPendingClassIndex() const { return PendingClassIndex; }

	// True once the player's vehicle is destroyed; Enter then restarts via RestartMatch().
	UFUNCTION(BlueprintCallable, Category = "IronSiege|Match")
	bool IsMatchOver() const;

	// Skips the menus and deploys the given class (0 Scout .. 3 Artillery). Used by the
	// DebugSelectVehicle console command (UIronSiegeCheatManager) for automated runs.
	void DeployVehicle(int32 ClassIndex);

	// Upgrade shop cursor: 0..IronUpgrades::Count-1 are items, IronUpgrades::Count is "CONTINUE".
	int32 GetShopIndex() const { return ShopIndex; }

private:
	void OnThrottle(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnThrottleReleased(const FInputActionValue& Value);
	void OnSteerReleased(const FInputActionValue& Value);
	void OnHandbrakePressed(const FInputActionValue& Value);
	void OnHandbrakeReleased(const FInputActionValue& Value);
	void OnFirePrimary(const FInputActionValue& Value);
	void OnFireSecondary(const FInputActionValue& Value);
	void OnBoost(const FInputActionValue& Value);
	void OnDeployMine(const FInputActionValue& Value);
	void OnFlamer(const FInputActionValue& Value);
	void OnFlares(const FInputActionValue& Value);
	void OnRailgun(const FInputActionValue& Value);
	void OnTesla(const FInputActionValue& Value);
	void OnAbility(const FInputActionValue& Value);
	void OnBoostReleased(const FInputActionValue& Value);
	void OnReloadPrimary(const FInputActionValue& Value);
	void OnReloadSecondary(const FInputActionValue& Value);
	void OnToggleCamera(const FInputActionValue& Value);

	IIronVehicle* GetControlledVehicle() const;

	void ToggleSettingsMenu();
	void ApplyKeyBindings();
	virtual void OnPossess(APawn* InPawn) override;

	// F3/F4/Enter drive whichever canvas screen is current: main menu, vehicle select, upgrade
	// shop, game over. (The settings menu is mouse/keyboard driven on its own.)
	void OnLeftPressed();
	void OnRightPressed();
	void OnEnterPressed();
	void OnBackPressed();
	// Gamepad in the menus: the d-pad chooses, A confirms, B goes back. Only while a menu is up -
	// in play the same buttons are weapons and the handbrake.
	void OnPadLeft();
	void OnPadRight();
	void OnPadConfirm();
	void OnPadBack();
	void ConfirmVehicleSelection();
	void ConfirmMainMenu();
	void RestartMatch();
	void OpenMission(int32 MissionIndex, bool bRetry);
	void OpenMainMenu();
	class AIronSiegeGameMode* GetIronGameMode() const;

	TSharedPtr<class SIronSettingsMenu> SettingsWidget;

	UPROPERTY()
	TObjectPtr<APawn> PhotoPawn;

	UPROPERTY()
	TObjectPtr<APawn> PhotoReturnPawn;

	// Built from the player's key bindings (replaces VehicleMappingContext at runtime).
	UPROPERTY()
	TObjectPtr<UInputMappingContext> RuntimeMappingContext;

	EIronScreen Screen = EIronScreen::ModeSelect;
	int32 ModeIndex = 0;
	int32 MissionCursor = 0;
	int32 PendingDriverIndex = 0;
	int32 ResultIndex = 0;
	int32 PendingClassIndex = 1; // Default highlight: Assault.
	double LeaveAskedAt = -100.0; // When F2 was last pressed mid-run (real seconds): a second press confirms.

	int32 SelectedMapIndex = 0;

	int32 ShopIndex = 0;
	class AIronSiegeGameMode* GetShopGameMode() const;
};
