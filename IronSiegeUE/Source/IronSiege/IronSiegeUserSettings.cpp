#include "IronSiegeUserSettings.h"
#include "SettingsRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"

namespace
{
const TCHAR* GAudioDir = TEXT("/Game/IronSiege/Audio/");

template <typename T>
T* LoadAudioAsset(const TCHAR* Name)
{
	return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s%s.%s"), GAudioDir, Name, Name));
}
}

UIronSiegeUserSettings::UIronSiegeUserSettings()
{
	ResetBindings();
	ResetGamepad();
}

UIronSiegeUserSettings* UIronSiegeUserSettings::Get()
{
	return Cast<UIronSiegeUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UIronSiegeUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	ResetAudio();
	Brightness = 1.f;
	bMotionBlur = false;
	ResetCamera();
	ResetControls();
	ResetBindings();
	ResetGameplay();
	ResetVoice();
	ResetGamepad();
	ResetAccessibility();
}

const TArray<FIronBindableAction>& UIronSiegeUserSettings::GetBindableActions()
{
	// Pad layout: triggers drive, left stick steers (one axis covers both directions, so only the
	// "steer right" row carries it), shoulders shoot, face buttons reload and swap view.
	static const TArray<FIronBindableAction> Actions = {
		{ TEXT("Accelerate"),     NSLOCTEXT("IronSiege", "KAccel", "Accelerate"),               EKeys::W,                 EKeys::Gamepad_RightTriggerAxis },
		{ TEXT("Brake"),          NSLOCTEXT("IronSiege", "KBrake", "Brake / Reverse"),          EKeys::S,                 EKeys::Gamepad_LeftTriggerAxis },
		{ TEXT("SteerLeft"),      NSLOCTEXT("IronSiege", "KLeft", "Steer Left"),                EKeys::A,                 EKeys::Invalid },
		{ TEXT("SteerRight"),     NSLOCTEXT("IronSiege", "KRight", "Steer Right"),              EKeys::D,                 EKeys::Gamepad_LeftX },
		{ TEXT("Handbrake"),      NSLOCTEXT("IronSiege", "KHand", "Handbrake"),                 EKeys::SpaceBar,          EKeys::Gamepad_FaceButton_Bottom },
		{ TEXT("Boost"),          NSLOCTEXT("IronSiege", "KBoost", "Nitro Boost"),             EKeys::LeftShift,         EKeys::Gamepad_LeftThumbstick },
		{ TEXT("DeployMine"),     NSLOCTEXT("IronSiege", "KMine", "Drop Mine"),                EKeys::F,                 EKeys::Gamepad_DPad_Down },
		{ TEXT("Flamer"),         NSLOCTEXT("IronSiege", "KFlame", "Flamethrower"),            EKeys::G,                 EKeys::Gamepad_DPad_Left },
		{ TEXT("Flares"),         NSLOCTEXT("IronSiege", "KFlares", "Flares (vs. missiles)"),   EKeys::Q,                 EKeys::Gamepad_DPad_Up },
		{ TEXT("Railgun"),        NSLOCTEXT("IronSiege", "KRail", "Railgun"),                   EKeys::Z,                 EKeys::Gamepad_DPad_Right },
		{ TEXT("Tesla"),          NSLOCTEXT("IronSiege", "KTesla", "Tesla Coil"),               EKeys::X,                 EKeys::Invalid },
		{ TEXT("Ability"),        NSLOCTEXT("IronSiege", "KAbility", "Driver Ability"),         EKeys::E,                 EKeys::Gamepad_RightThumbstick },
		{ TEXT("FirePrimary"),    NSLOCTEXT("IronSiege", "KFire1", "Fire Machine Gun"),         EKeys::LeftMouseButton,   EKeys::Gamepad_RightShoulder },
		{ TEXT("FireSecondary"),  NSLOCTEXT("IronSiege", "KFire2", "Fire Rockets"),             EKeys::RightMouseButton,  EKeys::Gamepad_LeftShoulder },
		{ TEXT("ReloadPrimary"),  NSLOCTEXT("IronSiege", "KRel1", "Reload Machine Gun"),        EKeys::R,                 EKeys::Gamepad_FaceButton_Left },
		{ TEXT("ReloadSecondary"),NSLOCTEXT("IronSiege", "KRel2", "Reload Rockets"),            EKeys::T,                 EKeys::Gamepad_FaceButton_Top },
		{ TEXT("ToggleCamera"),   NSLOCTEXT("IronSiege", "KCam", "Toggle Camera View"),         EKeys::C,                 EKeys::Gamepad_FaceButton_Right },
		{ TEXT("PushToTalk"),     NSLOCTEXT("IronSiege", "KPtt", "Push to Talk"),               EKeys::V,                 EKeys::Invalid },
	};
	return Actions;
}

FKey UIronSiegeUserSettings::GetKeyFor(FName Action) const
{
	for (const FIronKeyBinding& B : KeyBindings)
	{
		if (B.Action == Action && B.Key.IsValid())
		{
			return B.Key;
		}
	}
	for (const FIronBindableAction& A : GetBindableActions())
	{
		if (A.Id == Action)
		{
			return A.DefaultKey;
		}
	}
	return EKeys::Invalid;
}

void UIronSiegeUserSettings::SetKeyFor(FName Action, const FKey& Key)
{
	// A key drives one control only: whoever had it before loses it.
	for (FIronKeyBinding& B : KeyBindings)
	{
		if (B.Key == Key && B.Action != Action)
		{
			B.Key = EKeys::Invalid;
		}
	}
	for (FIronKeyBinding& B : KeyBindings)
	{
		if (B.Action == Action)
		{
			B.Key = Key;
			return;
		}
	}
	KeyBindings.Add({ Action, Key });
}

FKey UIronSiegeUserSettings::GetPadKeyFor(FName Action) const
{
	for (const FIronKeyBinding& B : PadBindings)
	{
		if (B.Action == Action)
		{
			return B.Key;
		}
	}
	for (const FIronBindableAction& A : GetBindableActions())
	{
		if (A.Id == Action)
		{
			return A.DefaultPadKey;
		}
	}
	return EKeys::Invalid;
}

void UIronSiegeUserSettings::SetPadKeyFor(FName Action, const FKey& Key)
{
	for (FIronKeyBinding& B : PadBindings)
	{
		if (B.Key == Key && B.Action != Action)
		{
			B.Key = EKeys::Invalid;
		}
	}
	for (FIronKeyBinding& B : PadBindings)
	{
		if (B.Action == Action)
		{
			B.Key = Key;
			return;
		}
	}
	PadBindings.Add({ Action, Key });
}

FLinearColor UIronSiegeUserSettings::RoleColor(int32 Role) const
{
	const IronSettings::Rgb C = IronSettings::RoleColor(ColorblindMode, Role);
	return FLinearColor(C.R, C.G, C.B);
}

float UIronSiegeUserSettings::HudScale() const
{
	return IronSettings::HudScale(HudScaleIndex);
}

// ---- Profiles: each is this object's whole config written to its own .ini, so switching profile
// is a load + apply and nothing else in the game needs to know.

FString UIronSiegeUserSettings::ProfileFilePath(const FString& Name)
{
	return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("Profiles") / (Name + TEXT(".ini"));
}

TArray<FString> UIronSiegeUserSettings::GetProfileNames()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("Profiles") / TEXT("*.ini")), true, false);
	TArray<FString> Names;
	for (const FString& File : Files)
	{
		Names.Add(FPaths::GetBaseFilename(File));
	}
	Names.Sort();
	return Names;
}

void UIronSiegeUserSettings::SaveProfile(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return;
	}
	ProfileName = Name;
	const FString Path = ProfileFilePath(Name);
	SaveConfig(CPF_Config, *Path);
	GConfig->Flush(false, Path);
	SaveSettings();
}

bool UIronSiegeUserSettings::LoadProfile(const FString& Name)
{
	const FString Path = ProfileFilePath(Name);
	if (!IFileManager::Get().FileExists(*Path))
	{
		return false;
	}
	LoadConfig(GetClass(), *Path);
	ProfileName = Name;
	ApplySettings(false);
	SaveSettings();
	return true;
}

void UIronSiegeUserSettings::DeleteProfile(const FString& Name)
{
	const FString Path = ProfileFilePath(Name);
	GConfig->UnloadFile(Path);
	IFileManager::Get().Delete(*Path, false, true);
}

void UIronSiegeUserSettings::ResetCategory(ECategory Category)
{
	switch (Category)
	{
	case ECategory::Video:
		SetOverallScalabilityLevel(2);
		SetFullscreenMode(EWindowMode::WindowedFullscreen);
		SetVSyncEnabled(false);
		SetFrameRateLimit(0.f);
		SetResolutionScaleNormalized(1.f);
		Brightness = 1.f;
		bMotionBlur = false;
		AntiAliasingMethod = 3;
		Sharpness = 0.25f;
		bBloom = bAmbientOcclusion = bReflections = bVolumetricFog = true;
		break;
	case ECategory::Audio: ResetAudio(); break;
	case ECategory::Controls: ResetControls(); ResetGamepad(); break;
	case ECategory::Camera: ResetCamera(); break;
	case ECategory::Voice: ResetVoice(); break;
	case ECategory::Shortcuts: ResetBindings(); break;
	case ECategory::Gameplay: ResetGameplay(); break;
	case ECategory::Accessibility: ResetAccessibility(); break;
	case ECategory::Profiles: break; // Nothing to reset: profiles are files, not values.
	}
}

void UIronSiegeUserSettings::ResetAudio()
{
	MasterVolume = 1.f;
	EffectsVolume = 1.f;
	EngineVolume = 0.8f;
	InterfaceVolume = 1.f;
	VoiceVolume = 1.f;
	bMuteWhenUnfocused = true;
	bHitSounds = true;
}

void UIronSiegeUserSettings::ResetCamera()
{
	FieldOfView = 90.f;
	CameraDistance = 1.f;
	CameraHeight = 1.f;
	CameraSmoothing = 0.4f;
}

void UIronSiegeUserSettings::ResetControls()
{
	SteeringSensitivity = 1.f;
	SteeringSmoothing = 0.45f;
	bAutoReload = true;
	DrivingStyle = 1;
	bAutoFlares = false;
}

void UIronSiegeUserSettings::ResetGamepad()
{
	PadBindings.Reset();
	for (const FIronBindableAction& A : GetBindableActions())
	{
		PadBindings.Add({ A.Id, A.DefaultPadKey });
	}
	GamepadDeadzone = 0.18f;
	VibrationStrength = 0.8f;
}

void UIronSiegeUserSettings::ResetAccessibility()
{
	Language = TEXT("en");
	HudScaleIndex = 1;
	ColorblindMode = 0;
	bReduceMotion = false;
}

void UIronSiegeUserSettings::ResetBindings()
{
	KeyBindings.Reset();
	for (const FIronBindableAction& A : GetBindableActions())
	{
		KeyBindings.Add({ A.Id, A.DefaultKey });
	}
}

void UIronSiegeUserSettings::ResetGameplay()
{
	Difficulty = 1;
	AimAssist = 1;
	CrosshairStyle = 0;
	bShowMinimap = true;
	bShowEnemyHealthBars = true;
	bShowFps = false;
	bDamageNumbers = true;
	bMph = false;
	MinimapZoom = 1;
	HudOpacity = 1.f;
}

void UIronSiegeUserSettings::ResetVoice()
{
	MicDeviceName.Reset();
	MicGain = 1.f;
	bPushToTalk = true;
	VoiceVolume = 1.f;
}

void UIronSiegeUserSettings::ApplyRuntimeSettings(UWorld* World) const
{
	// Audio: one sound mix overriding each category's class volume (master folded in).
	if (World)
	{
		if (USoundMix* Mix = LoadAudioAsset<USoundMix>(TEXT("SM_IronVolumes")))
		{
			const TPair<const TCHAR*, float> Classes[] = {
				{ TEXT("SC_Effects"), EffectsVolume }, { TEXT("SC_Engine"), EngineVolume },
				{ TEXT("SC_Interface"), InterfaceVolume }, { TEXT("SC_Voice"), VoiceVolume },
			};
			for (const TPair<const TCHAR*, float>& C : Classes)
			{
				if (USoundClass* Class = LoadAudioAsset<USoundClass>(C.Key))
				{
					UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, FMath::Clamp(C.Value * MasterVolume, 0.f, 1.f), 1.f, 0.05f, true);
				}
			}
			UGameplayStatics::PushSoundMixModifier(World, Mix);
		}
	}
	// Language: the UI rebuilds itself from the active culture next time it is drawn.
	{
		const FString Wanted = Language.IsEmpty() ? TEXT("en") : Language;
		if (FInternationalization::Get().GetCurrentCulture()->GetName() != Wanted)
		{
			FInternationalization::Get().SetCurrentCulture(Wanted);
		}
	}
	// Brightness via display gamma; motion blur via its quality cvar.
	if (GEngine)
	{
		GEngine->DisplayGamma = IronSettings::DisplayGamma(Brightness);
	}
	if (IConsoleVariable* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		Blur->Set(bMotionBlur ? 3 : 0, ECVF_SetByGameSetting);
	}
	// Image options. "On" values match what the High preset would pick, so turning one back on does
	// not leave it weaker than before it was turned off.
	auto SetCvar = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	};
	SetCvar(TEXT("r.AntiAliasingMethod"), static_cast<float>(IronSettings::AaMethodCvar(AntiAliasingMethod)));
	SetCvar(TEXT("r.Tonemapper.Sharpen"), IronSettings::SharpenCvar(Sharpness));
	SetCvar(TEXT("r.BloomQuality"), bBloom ? 4.f : 0.f);
	SetCvar(TEXT("r.AmbientOcclusionLevels"), bAmbientOcclusion ? -1.f : 0.f);
	SetCvar(TEXT("r.SSR.Quality"), bReflections ? 2.f : 0.f);
	SetCvar(TEXT("r.VolumetricFog"), bVolumetricFog ? 1.f : 0.f);
	// Audio in the background.
	FApp::SetUnfocusedVolumeMultiplier(bMuteWhenUnfocused ? 0.f : 1.f);
}
