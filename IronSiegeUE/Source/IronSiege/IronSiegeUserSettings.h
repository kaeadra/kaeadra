#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "IronSiegeUserSettings.generated.h"

// One rebindable control and the key currently assigned to it.
USTRUCT()
struct FIronKeyBinding
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FName Action;

	UPROPERTY(Config)
	FKey Key;
};

// Static description of a rebindable control (see UIronSiegeUserSettings::GetBindableActions).
struct FIronBindableAction
{
	FName Id;
	FText Label;
	FKey DefaultKey;
	FKey DefaultPadKey; // Gamepad equivalent; invalid for controls with no pad button.
};

// Every player-facing option in the game, saved to GameUserSettings.ini. Extends the engine's
// UGameUserSettings (which already owns resolution, window mode, v-sync, frame cap, resolution
// scale and the scalability groups) with audio mix, camera, controls/key bindings, HUD, difficulty
// and microphone options. Registered as the engine's settings class in DefaultEngine.ini.
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class IRONSIEGE_API UIronSiegeUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UIronSiegeUserSettings();

	static UIronSiegeUserSettings* Get();

	virtual void SetToDefaults() override;

	// ---- Audio (0..1)
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float EffectsVolume = 1.f;   // Weapons, explosions.
	UPROPERTY(Config) float EngineVolume = 0.8f;   // Vehicle engines.
	UPROPERTY(Config) float InterfaceVolume = 1.f; // Hit markers, siren, pickups.
	UPROPERTY(Config) float VoiceVolume = 1.f;     // Voice chat playback (multiplayer).
	UPROPERTY(Config) bool bMuteWhenUnfocused = true; // Silence the game while alt-tabbed.
	UPROPERTY(Config) bool bHitSounds = true;         // The tick when your shots land.

	// ---- Video extras (the rest lives in UGameUserSettings)
	UPROPERTY(Config) float Brightness = 1.f;      // 0.5..1.5, see IronSettings::DisplayGamma.
	UPROPERTY(Config) bool bMotionBlur = false;
	// Image (Video tab): anti-aliasing method (IronSettings::AaMethodCvar), sharpening 0..1, and
	// switches for the heavier post effects.
	UPROPERTY(Config) int32 AntiAliasingMethod = 3; // 0 off, 1 FXAA, 2 TAA, 3 TSR.
	UPROPERTY(Config) float Sharpness = 0.25f;
	UPROPERTY(Config) bool bBloom = true;
	UPROPERTY(Config) bool bAmbientOcclusion = true;
	UPROPERTY(Config) bool bReflections = true;
	UPROPERTY(Config) bool bVolumetricFog = true;

	// ---- Camera
	UPROPERTY(Config) float FieldOfView = 90.f;
	UPROPERTY(Config) float CameraDistance = 1.f;  // Multiplier on each car's chase distance.
	UPROPERTY(Config) float CameraHeight = 1.f;    // Multiplier on each car's chase height.
	UPROPERTY(Config) float CameraSmoothing = 0.4f;

	// ---- Controls
	UPROPERTY(Config) float SteeringSensitivity = 1.f; // Multiplier on steering strength.
	UPROPERTY(Config) float SteeringSmoothing = 0.45f;
	UPROPERTY(Config) bool bAutoReload = true;
	UPROPERTY(Config) TArray<FIronKeyBinding> KeyBindings;
	UPROPERTY(Config) int32 DrivingStyle = 1;      // 0 arcade, 1 balanced, 2 simulation.
	UPROPERTY(Config) bool bAutoFlares = false;    // Throw flares by themselves when a missile closes in.

	// ---- Gamepad
	UPROPERTY(Config) TArray<FIronKeyBinding> PadBindings;
	UPROPERTY(Config) float GamepadDeadzone = 0.18f;
	UPROPERTY(Config) float VibrationStrength = 0.8f; // 0 = vibration off.

	// ---- Accessibility
	UPROPERTY(Config) FString Language = TEXT("en"); // "en" or "ar".
	UPROPERTY(Config) int32 HudScaleIndex = 1;       // See IronSettings::HudScale.
	UPROPERTY(Config) int32 ColorblindMode = 0;      // 0 off, 1 protan, 2 deutan, 3 tritan.
	UPROPERTY(Config) bool bReduceMotion = false;    // Stiffer camera, weaker shake.

	// ---- Profiles (the active one; the rest live as .ini files under Saved/Config/Profiles)
	UPROPERTY(Config) FString ProfileName = TEXT("Default");

	// ---- Gameplay / HUD
	UPROPERTY(Config) int32 Difficulty = 1;
	UPROPERTY(Config) int32 AimAssist = 1;         // 0 off, 1 normal, 2 strong (AimRules.h).
	UPROPERTY(Config) int32 CrosshairStyle = 0;    // 0 cross, 1 dot, 2 circle, 3 off.
	UPROPERTY(Config) bool bShowMinimap = true;
	UPROPERTY(Config) bool bShowEnemyHealthBars = true;
	UPROPERTY(Config) bool bShowFps = false;
	UPROPERTY(Config) bool bDamageNumbers = true;  // Floating numbers over the cars you hit.
	UPROPERTY(Config) bool bMph = false;           // Speed readout in mph instead of km/h.
	UPROPERTY(Config) int32 MinimapZoom = 1;       // IronSettings::MinimapRangeCm.
	UPROPERTY(Config) float HudOpacity = 1.f;      // HUD text opacity (IronSettings::HudOpacity).

	// ---- Microphone / voice
	UPROPERTY(Config) FString MicDeviceName;       // Empty = system default.
	UPROPERTY(Config) float MicGain = 1.f;
	UPROPERTY(Config) bool bPushToTalk = true;

	// Rebindable controls, in display order.
	static const TArray<FIronBindableAction>& GetBindableActions();
	FKey GetKeyFor(FName Action) const;
	void SetKeyFor(FName Action, const FKey& Key);
	FKey GetPadKeyFor(FName Action) const;
	void SetPadKeyFor(FName Action, const FKey& Key);

	// Colour for a HUD role (IronSettings::ColorEnemy/Friendly/Warning) under the current
	// colour-blind mode.
	FLinearColor RoleColor(int32 Role) const;
	float HudScale() const;

	// ---- Profiles: every option above, saved under a name so several players can share a PC.
	static TArray<FString> GetProfileNames();
	static FString ProfileFilePath(const FString& Name);
	void SaveProfile(const FString& Name);
	bool LoadProfile(const FString& Name);
	static void DeleteProfile(const FString& Name);

	enum class ECategory : uint8 { Video, Audio, Controls, Camera, Voice, Shortcuts, Gameplay, Accessibility, Profiles };
	void ResetCategory(ECategory Category);

	// Applies the non-video settings that act on the running game: audio mix, brightness,
	// motion blur. (Camera/controls/bindings are applied by the player controller.)
	void ApplyRuntimeSettings(UWorld* World) const;

private:
	void ResetAudio();
	void ResetCamera();
	void ResetControls();
	void ResetBindings();
	void ResetGameplay();
	void ResetVoice();
	void ResetGamepad();
	void ResetAccessibility();
};
