#include "IronSiegePlayerController.h"
#include "IronSiegeText.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "IronVehicle.h"
#include "IronSiegeGameMode.h"
#include "VehicleClassRules.h"
#include "UpgradeRules.h"
#include "IronSiegeCheatManager.h"
#include "IronSiegeUserSettings.h"
#include "SIronSettingsMenu.h"
#include "WarVehiclePawn.h"
#include "MachineGunComponent.h"
#include "RocketLauncherComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "SettingsRules.h"
#include "IronSiegeHUD.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CrewRules.h"
#include "MissionRules.h"
#include "IronSiegeStory.h"
#include "IronMissionDirector.h"

namespace
{
	const TArray<FString> GMapNames = { TEXT("Arctic"), TEXT("Desert"), TEXT("CityRuins"), TEXT("Coast") };
}

AIronSiegePlayerController::AIronSiegePlayerController()
{
	CheatClass = UIronSiegeCheatManager::StaticClass();
}

void AIronSiegePlayerController::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	// Development builds get the Debug* console commands (UIronSiegeCheatManager).
	AddCheats(true);
#endif

	// Arriving from the menu's own map change carries SkipMenu=1, so the player lands on the next
	// screen (the mission's briefing, or the driver select) instead of at the top of the menus again.
	const IronMissions::Progress Progress = IronStory::LoadProgress();
	MissionCursor = Progress.NextMission();
	IronStory::LoadLastChoice(PendingDriverIndex, PendingClassIndex);
	PendingDriverIndex = FMath::Clamp(PendingDriverIndex, 0, IronCrew::DriverCount - 1);
	if (!IronMissions::IsDriverUnlocked(Progress, static_cast<IronCrew::Driver>(PendingDriverIndex)))
	{
		PendingDriverIndex = 0;
	}
	PendingClassIndex = FMath::Clamp(PendingClassIndex, 0, IronVehicles::Count - 1);
	if (UWorld* World = GetWorld())
	{
		const FString Options = World->GetAuthGameMode() ? World->GetAuthGameMode()->OptionsString : FString();
		const AIronSiegeGameMode* GameMode = GetIronGameMode();
		if (GameMode && GameMode->IsMissionMode())
		{
			ModeIndex = 0;
			MissionCursor = GameMode->GetMissionIndex();
			// A retry has heard the briefing already.
			Screen = UGameplayStatics::ParseOption(Options, TEXT("Retry")).IsEmpty() ? EIronScreen::Briefing : EIronScreen::DriverSelect;
		}
		else if (!UGameplayStatics::ParseOption(Options, TEXT("SkipMenu")).IsEmpty())
		{
			ModeIndex = 1;
			Screen = EIronScreen::DriverSelect;
		}
		const FString CurrentMap = World->GetMapName();
		for (int32 i = 0; i < GMapNames.Num(); ++i)
		{
			if (CurrentMap.Contains(GMapNames[i]))
			{
				SelectedMapIndex = i;
				break;
			}
		}
	}

	ApplyUserSettings();
}

void AIronSiegePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ThrottleAction)
		{
			EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AIronSiegePlayerController::OnThrottle);
			// Triggered only fires while a key is held; releasing it must zero the input, or the
			// Chaos car keeps the last throttle (runs away) / steering (keeps circling).
			EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AIronSiegePlayerController::OnThrottleReleased);
			EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Canceled, this, &AIronSiegePlayerController::OnThrottleReleased);
		}
		if (SteerAction)
		{
			EnhancedInput->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AIronSiegePlayerController::OnSteer);
			EnhancedInput->BindAction(SteerAction, ETriggerEvent::Completed, this, &AIronSiegePlayerController::OnSteerReleased);
			EnhancedInput->BindAction(SteerAction, ETriggerEvent::Canceled, this, &AIronSiegePlayerController::OnSteerReleased);
		}
		if (FlamerAction)
		{
			// Triggered, not Started: the flame burns for as long as the key is held.
			EnhancedInput->BindAction(FlamerAction, ETriggerEvent::Triggered, this, &AIronSiegePlayerController::OnFlamer);
		}
		// Actions added after the input assets were authored are created here instead.
		auto RuntimeAction = [this](TObjectPtr<UInputAction>& Action, const TCHAR* Name)
		{
			if (!Action)
			{
				Action = NewObject<UInputAction>(this, Name);
				Action->ValueType = EInputActionValueType::Boolean;
			}
		};
		RuntimeAction(FlaresAction, TEXT("IA_Flares_Runtime"));
		RuntimeAction(RailgunAction, TEXT("IA_Railgun_Runtime"));
		RuntimeAction(TeslaAction, TEXT("IA_Tesla_Runtime"));
		RuntimeAction(AbilityAction, TEXT("IA_Ability_Runtime"));
		EnhancedInput->BindAction(FlaresAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnFlares);
		EnhancedInput->BindAction(RailgunAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnRailgun);
		EnhancedInput->BindAction(TeslaAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnTesla);
		EnhancedInput->BindAction(AbilityAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnAbility);
		if (DeployMineAction)
		{
			EnhancedInput->BindAction(DeployMineAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnDeployMine);
		}
		if (BoostAction)
		{
			EnhancedInput->BindAction(BoostAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnBoost);
			EnhancedInput->BindAction(BoostAction, ETriggerEvent::Completed, this, &AIronSiegePlayerController::OnBoostReleased);
			EnhancedInput->BindAction(BoostAction, ETriggerEvent::Canceled, this, &AIronSiegePlayerController::OnBoostReleased);
		}
		if (HandbrakeAction)
		{
			EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnHandbrakePressed);
			EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &AIronSiegePlayerController::OnHandbrakeReleased);
		}
		if (FirePrimaryAction)
		{
			// Triggered, not Started: the machine gun is automatic (IronWeapons::Spec), so it fires for
			// as long as the trigger is held, paced by its own fire interval and stopped by the heat
			// lockout. Bound to Started it fired one round per press and the heat/spin-up never came in.
			EnhancedInput->BindAction(FirePrimaryAction, ETriggerEvent::Triggered, this, &AIronSiegePlayerController::OnFirePrimary);
		}
		if (FireSecondaryAction)
		{
			EnhancedInput->BindAction(FireSecondaryAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnFireSecondary);
		}
		if (ReloadPrimaryAction)
		{
			EnhancedInput->BindAction(ReloadPrimaryAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnReloadPrimary);
		}
		if (ReloadSecondaryAction)
		{
			EnhancedInput->BindAction(ReloadSecondaryAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnReloadSecondary);
		}
		if (ToggleCameraAction)
		{
			EnhancedInput->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AIronSiegePlayerController::OnToggleCamera);
		}
	}

	// Legacy key bindings for the vehicle-select screen and settings menu - kept separate from
	// Enhanced Input so this works immediately without needing new IA_/IMC_ assets. Left/Right/
	// Enter are shared: they drive vehicle-select first, then the settings menu once a vehicle
	// has been chosen (see OnLeftPressed/OnRightPressed/OnEnterPressed).
	// F-row keys throughout, not arrow keys: Left/Right/Up/Down are intercepted by Slate's
	// built-in UI navigation before reaching the PlayerController's InputComponent (confirmed
	// live - Enter, F1 and F2 all worked, Left/Right did nothing), even with no UMG widgets on
	// screen. F-keys are not part of that navigation set and are already proven to work here.
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AIronSiegePlayerController::ToggleSettingsMenu);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AIronSiegePlayerController::ToggleSettingsMenu);
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AIronSiegePlayerController::ToggleSettingsMenu);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AIronSiegePlayerController::TogglePhotoMode);
	InputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AIronSiegePlayerController::TogglePhotoMode);
	InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &AIronSiegePlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::F4, IE_Pressed, this, &AIronSiegePlayerController::OnRightPressed);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AIronSiegePlayerController::OnEnterPressed);
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AIronSiegePlayerController::OnBackPressed);
	// Not consumed: in play these same buttons belong to the Enhanced Input actions.
	InputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AIronSiegePlayerController::OnPadLeft).bConsumeInput = false;
	InputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AIronSiegePlayerController::OnPadRight).bConsumeInput = false;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AIronSiegePlayerController::OnPadConfirm).bConsumeInput = false;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AIronSiegePlayerController::OnPadBack).bConsumeInput = false;
}

IIronVehicle* AIronSiegePlayerController::GetControlledVehicle() const
{
	return Cast<IIronVehicle>(GetPawn());
}

void AIronSiegePlayerController::OnThrottle(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->MoveForward(Value.Get<float>());
	}
}

void AIronSiegePlayerController::OnSteer(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->Steer(Value.Get<float>());
	}
}

void AIronSiegePlayerController::OnThrottleReleased(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->MoveForward(0.f);
	}
}

void AIronSiegePlayerController::OnSteerReleased(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->Steer(0.f);
	}
}

void AIronSiegePlayerController::OnFlamer(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->FireFlamer();
	}
}

void AIronSiegePlayerController::OnRailgun(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->FireRailgun();
	}
}

void AIronSiegePlayerController::OnTesla(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->FireTesla();
	}
}

void AIronSiegePlayerController::OnAbility(const FInputActionValue& Value)
{
	if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetPawn()))
	{
		Car->UseAbility();
	}
}

void AIronSiegePlayerController::OnFlares(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->FireFlares();
	}
}

void AIronSiegePlayerController::OnDeployMine(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->DeployMine();
	}
}

void AIronSiegePlayerController::OnBoost(const FInputActionValue& Value)
{
	if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetPawn()))
	{
		Car->SetBoost(true);
	}
}

void AIronSiegePlayerController::OnBoostReleased(const FInputActionValue& Value)
{
	if (AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetPawn()))
	{
		Car->SetBoost(false);
	}
}

void AIronSiegePlayerController::OnHandbrakePressed(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->SetHandbrake(true);
	}
}

void AIronSiegePlayerController::OnHandbrakeReleased(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->SetHandbrake(false);
	}
}

void AIronSiegePlayerController::OnFirePrimary(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
		const UMachineGunComponent* Gun = Vehicle->GetPrimaryWeaponComponent();
		if (S && S->bAutoReload && Gun && Gun->GetAmmo() <= 0)
		{
			Vehicle->ReloadPrimary();
		}
		Vehicle->FirePrimary();
	}
}

void AIronSiegePlayerController::OnFireSecondary(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
		const URocketLauncherComponent* Launcher = Vehicle->GetSecondaryWeaponComponent();
		if (S && S->bAutoReload && Launcher && Launcher->GetAmmo() <= 0)
		{
			Vehicle->ReloadSecondary();
		}
		Vehicle->FireSecondary();
	}
}

void AIronSiegePlayerController::OnReloadPrimary(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->ReloadPrimary();
	}
}

void AIronSiegePlayerController::OnReloadSecondary(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->ReloadSecondary();
	}
}

void AIronSiegePlayerController::OnToggleCamera(const FInputActionValue& Value)
{
	if (IIronVehicle* Vehicle = GetControlledVehicle())
	{
		Vehicle->ToggleCameraView();
	}
}

void AIronSiegePlayerController::ToggleSettingsMenu()
{
	if (IsSettingsMenuOpen())
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

void AIronSiegePlayerController::OpenSettingsMenu()
{
	if (SettingsWidget.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	SAssignNew(SettingsWidget, SIronSettingsMenu).Owner(this);
	GEngine->GameViewport->AddViewportWidgetContent(SettingsWidget.ToSharedRef(), 100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(SettingsWidget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	if (GetPawn())
	{
		SetPause(true);
	}
}

void AIronSiegePlayerController::CloseSettingsMenu()
{
	if (!SettingsWidget.IsValid())
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SettingsWidget.ToSharedRef());
	}
	SettingsWidget.Reset();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	SetPause(false);
}

void AIronSiegePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ApplyUserSettings();
}

void AIronSiegePlayerController::ApplyUserSettings()
{
	UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	if (!S)
	{
		return;
	}
	S->ApplyRuntimeSettings(GetWorld());
	ApplyKeyBindings();
	AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetPawn());
	if (!Car)
	{
		Car = Cast<AWarVehiclePawn>(PhotoReturnPawn); // Photo mode: keep tuning the parked car.
	}
	if (Car)
	{
		Car->ApplyViewSettings(S->FieldOfView, S->CameraDistance, S->CameraHeight, S->CameraSmoothing, S->bReduceMotion);
		Car->ApplyControlSettings(S->SteeringSensitivity, S->SteeringSmoothing);
		Car->ApplyDrivingStyle(S->DrivingStyle);
	}
}

void AIronSiegePlayerController::ApplyKeyBindings()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	if (!Subsystem)
	{
		return;
	}
	if (!S || !ThrottleAction || !SteerAction)
	{
		// No action assets wired: fall back to the authored mapping context.
		if (VehicleMappingContext)
		{
			Subsystem->AddMappingContext(VehicleMappingContext, 0);
		}
		return;
	}
	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(this);
	auto MapKey = [&](UInputAction* Action, const FKey& Key, bool bNegate)
	{
		if (!Action || !Key.IsValid())
		{
			return;
		}
		FEnhancedActionKeyMapping& Mapping = Ctx->MapKey(Action, Key);
		// A stick axis already reports both directions, so it is mapped once (on the "positive"
		// control) and never negated; a trigger axis needs the dead zone and, for braking, a flip.
		if (Key.IsAxis1D())
		{
			UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Ctx);
			DeadZone->LowerThreshold = FMath::Clamp(S->GamepadDeadzone, 0.f, 0.9f);
			Mapping.Modifiers.Add(DeadZone);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Ctx));
		}
	};
	auto Map = [&](UInputAction* Action, FName Id, bool bNegate)
	{
		MapKey(Action, S->GetKeyFor(Id), bNegate);
		const FKey PadKey = S->GetPadKeyFor(Id);
		// Steering comes off one stick axis: negating it too would cancel the other direction.
		MapKey(Action, PadKey, bNegate && !PadKey.IsAxis1D());
	};
	Map(ThrottleAction, TEXT("Accelerate"), false);
	Map(ThrottleAction, TEXT("Brake"), true);
	Map(SteerAction, TEXT("SteerRight"), false);
	Map(SteerAction, TEXT("SteerLeft"), true);
	Map(HandbrakeAction, TEXT("Handbrake"), false);
	Map(BoostAction, TEXT("Boost"), false);
	Map(DeployMineAction, TEXT("DeployMine"), false);
	Map(FlamerAction, TEXT("Flamer"), false);
	Map(FlaresAction, TEXT("Flares"), false);
	Map(RailgunAction, TEXT("Railgun"), false);
	Map(TeslaAction, TEXT("Tesla"), false);
	Map(AbilityAction, TEXT("Ability"), false);
	Map(FirePrimaryAction, TEXT("FirePrimary"), false);
	Map(FireSecondaryAction, TEXT("FireSecondary"), false);
	Map(ReloadPrimaryAction, TEXT("ReloadPrimary"), false);
	Map(ReloadSecondaryAction, TEXT("ReloadSecondary"), false);
	Map(ToggleCameraAction, TEXT("ToggleCamera"), false);
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(Ctx, 0);
	RuntimeMappingContext = Ctx;
	UE_LOG(LogTemp, Log, TEXT("IronSiege: applied %d key mappings from user settings"), Ctx->GetMappings().Num());
}

AIronSiegeGameMode* AIronSiegePlayerController::GetIronGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
}

TArray<int32> AIronSiegePlayerController::GetResultOptions() const
{
	TArray<int32> Options;
	const AIronSiegeGameMode* GameMode = GetIronGameMode();
	if (GameMode && GameMode->WasMissionWon() && GameMode->GetMissionIndex() + 1 < IronMissions::Count)
	{
		Options.Add(0);
	}
	Options.Add(1);
	Options.Add(2);
	return Options;
}

void AIronSiegePlayerController::OnLeftPressed()
{
	switch (Screen)
	{
	case EIronScreen::ModeSelect: ModeIndex = 1 - ModeIndex; break;
	case EIronScreen::MapSelect: SelectedMapIndex = (SelectedMapIndex + GMapNames.Num() - 1) % GMapNames.Num(); break;
	case EIronScreen::MissionSelect: MissionCursor = (MissionCursor + IronMissions::Count - 1) % IronMissions::Count; break;
	case EIronScreen::DriverSelect: PendingDriverIndex = IronCrew::PrevIndex(PendingDriverIndex); break;
	case EIronScreen::VehicleSelect: PendingClassIndex = IronVehicles::PrevIndex(PendingClassIndex); break;
	case EIronScreen::Playing:
		if (GetShopGameMode())
		{
			ShopIndex = (ShopIndex + IronUpgrades::Count) % (IronUpgrades::Count + 1);
		}
		else if (IsMatchOver())
		{
			const int32 Count = GetResultOptions().Num();
			ResultIndex = (ResultIndex + Count - 1) % Count;
		}
		break;
	default: break;
	}
}

void AIronSiegePlayerController::OnRightPressed()
{
	switch (Screen)
	{
	case EIronScreen::ModeSelect: ModeIndex = 1 - ModeIndex; break;
	case EIronScreen::MapSelect: SelectedMapIndex = (SelectedMapIndex + 1) % GMapNames.Num(); break;
	case EIronScreen::MissionSelect: MissionCursor = (MissionCursor + 1) % IronMissions::Count; break;
	case EIronScreen::DriverSelect: PendingDriverIndex = IronCrew::NextIndex(PendingDriverIndex); break;
	case EIronScreen::VehicleSelect: PendingClassIndex = IronVehicles::NextIndex(PendingClassIndex); break;
	case EIronScreen::Playing:
		if (GetShopGameMode())
		{
			ShopIndex = (ShopIndex + 1) % (IronUpgrades::Count + 1);
		}
		else if (IsMatchOver())
		{
			ResultIndex = (ResultIndex + 1) % GetResultOptions().Num();
		}
		break;
	default: break;
	}
}

void AIronSiegePlayerController::OnEnterPressed()
{
	const AIronSiegeGameMode* GameMode = GetIronGameMode();
	switch (Screen)
	{
	case EIronScreen::ModeSelect:
		Screen = ModeIndex == 0 ? EIronScreen::MissionSelect : EIronScreen::MapSelect;
		break;
	case EIronScreen::MapSelect:
		ConfirmMainMenu();
		break;
	case EIronScreen::MissionSelect:
		// Missions open in order; a locked one stays shut.
		if (IronStory::LoadProgress().IsUnlocked(MissionCursor))
		{
			OpenMission(MissionCursor, false);
		}
		break;
	case EIronScreen::Briefing:
		Screen = EIronScreen::DriverSelect;
		break;
	case EIronScreen::DriverSelect:
		if (IronMissions::IsDriverUnlocked(IronStory::LoadProgress(), static_cast<IronCrew::Driver>(PendingDriverIndex)))
		{
			Screen = EIronScreen::VehicleSelect;
		}
		break;
	case EIronScreen::VehicleSelect:
		ConfirmVehicleSelection();
		break;
	case EIronScreen::Playing:
		if (AIronSiegeGameMode* Shop = GetShopGameMode())
		{
			if (ShopIndex >= IronUpgrades::Count)
			{
				Shop->CloseShop();
				ShopIndex = 0;
			}
			else
			{
				Shop->BuyUpgrade(ShopIndex);
			}
		}
		else if (IsMatchOver())
		{
			if (!GameMode || !GameMode->IsMissionMode())
			{
				RestartMatch();
				break;
			}
			const TArray<int32> Options = GetResultOptions();
			switch (Options[FMath::Clamp(ResultIndex, 0, Options.Num() - 1)])
			{
			case 0: OpenMission(GameMode->GetMissionIndex() + 1, false); break;
			case 1: OpenMission(GameMode->GetMissionIndex(), true); break;
			default: OpenMainMenu(); break;
			}
		}
		break;
	default: break;
	}
}

void AIronSiegePlayerController::OnBackPressed()
{
	const AIronSiegeGameMode* GameMode = GetIronGameMode();
	switch (Screen)
	{
	case EIronScreen::MapSelect:
	case EIronScreen::MissionSelect:
		Screen = EIronScreen::ModeSelect;
		break;
	case EIronScreen::Briefing:
		Screen = EIronScreen::MissionSelect;
		break;
	case EIronScreen::DriverSelect:
		Screen = GameMode && GameMode->IsMissionMode() ? EIronScreen::Briefing : EIronScreen::MapSelect;
		break;
	case EIronScreen::VehicleSelect:
		Screen = EIronScreen::DriverSelect;
		break;
	case EIronScreen::Playing:
		// From the survival game-over screen: back to the main menu.
		if (IsMatchOver())
		{
			if (!GameMode || !GameMode->IsMissionMode())
			{
				OpenMainMenu();
			}
		}
		else if (!GetShopGameMode())
		{
			// Mid-run it costs the run, so it takes two presses.
			const double Now = FPlatformTime::Seconds();
			if (Now - LeaveAskedAt < 3.0)
			{
				OpenMainMenu();
			}
			else if (AIronSiegeHUD* Hud = Cast<AIronSiegeHUD>(GetHUD()))
			{
				LeaveAskedAt = Now;
				Hud->ShowNotice(IronText::Str(TEXT("NoticeLeave"), TEXT("F2 AGAIN TO LEAVE FOR THE MAIN MENU")), FLinearColor(1.f, 0.8f, 0.3f));
			}
		}
		break;
	default: break;
	}
}

void AIronSiegePlayerController::OnPadLeft()
{
	if (Screen != EIronScreen::Playing || IsMatchOver() || GetShopGameMode())
	{
		OnLeftPressed();
	}
}

void AIronSiegePlayerController::OnPadRight()
{
	if (Screen != EIronScreen::Playing || IsMatchOver() || GetShopGameMode())
	{
		OnRightPressed();
	}
}

void AIronSiegePlayerController::OnPadConfirm()
{
	if (Screen != EIronScreen::Playing || IsMatchOver())
	{
		OnEnterPressed();
	}
}

void AIronSiegePlayerController::OnPadBack()
{
	if (Screen != EIronScreen::Playing || IsMatchOver())
	{
		OnBackPressed();
	}
}

void AIronSiegePlayerController::DebugMenuKey(int32 Key)
{
	switch (Key)
	{
	case 0: OnLeftPressed(); break;
	case 1: OnRightPressed(); break;
	case 2: OnEnterPressed(); break;
	default: OnBackPressed(); break;
	}
}

void AIronSiegePlayerController::SetPendingDriver(int32 Driver)
{
	PendingDriverIndex = FMath::Clamp(Driver, 0, IronCrew::DriverCount - 1);
}

void AIronSiegePlayerController::OpenMission(int32 MissionIndex, bool bRetry)
{
	if (MissionIndex < 0 || MissionIndex >= IronMissions::Count)
	{
		return;
	}
	// Always a fresh load, even onto the map already open: the level has to start in mission mode.
	const FString Level = FString::Printf(TEXT("Map_%s"), ANSI_TO_TCHAR(IronMissions::Get(MissionIndex).Map));
	const FString Options = FString::Printf(TEXT("SkipMenu=1?Mission=%d%s"), MissionIndex + 1, bRetry ? TEXT("?Retry=1") : TEXT(""));
	UGameplayStatics::OpenLevel(this, FName(*Level), true, Options);
}

void AIronSiegePlayerController::OpenMainMenu()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, FString());
}

AIronSiegeGameMode* AIronSiegePlayerController::GetShopGameMode() const
{
	AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	return GameMode && GameMode->IsShopOpen() && !GameMode->IsGameOver() ? GameMode : nullptr;
}

bool AIronSiegePlayerController::IsMatchOver() const
{
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	return GameMode && GameMode->IsGameOver();
}

void AIronSiegePlayerController::RestartMatch()
{
	// Reloading the map is the simplest full reset (wrecks, projectiles, timers). SkipMenu lands
	// the player back on vehicle select so they can switch class between runs.
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, TEXT("SkipMenu=1"));
}

FString AIronSiegePlayerController::GetSelectedMapName() const
{
	return GMapNames.IsValidIndex(SelectedMapIndex) ? GMapNames[SelectedMapIndex] : TEXT("?");
}

void AIronSiegePlayerController::ConfirmMainMenu()
{
	UWorld* World = GetWorld();
	if (!World || !GMapNames.IsValidIndex(SelectedMapIndex))
	{
		return;
	}
	// Already on the chosen map (and not loaded for a campaign mission): on to the driver select
	// without a level change.
	const AIronSiegeGameMode* GameMode = GetIronGameMode();
	if (World->GetMapName().Contains(GMapNames[SelectedMapIndex]) && (!GameMode || !GameMode->IsMissionMode()))
	{
		Screen = EIronScreen::DriverSelect;
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*FString::Printf(TEXT("Map_%s"), *GMapNames[SelectedMapIndex])), true, TEXT("SkipMenu=1"));
}

void AIronSiegePlayerController::DeployVehicle(int32 ClassIndex)
{
	if (Screen == EIronScreen::Playing)
	{
		return;
	}
	PendingClassIndex = ClassIndex;
	ConfirmVehicleSelection();
}

void AIronSiegePlayerController::ConfirmVehicleSelection()
{
	if (Screen == EIronScreen::Playing)
	{
		return;
	}
	Screen = EIronScreen::Playing;
	ResultIndex = 0;
	IronStory::SaveLastChoice(PendingDriverIndex, PendingClassIndex);
	if (AIronSiegeGameMode* GameMode = GetIronGameMode())
	{
		GameMode->SpawnSelectedVehicle(this, PendingClassIndex, PendingDriverIndex);
	}
}

void AIronSiegePlayerController::PlayRumble(float Strength, float Duration)
{
	const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	const float Amplitude = IronSettings::RumbleAmplitude(Strength, S ? S->VibrationStrength : 0.f);
	if (Amplitude <= 0.f)
	{
		return;
	}
	FForceFeedbackValues Values;
	Values.LeftLarge = Values.RightLarge = Amplitude;
	Values.LeftSmall = Values.RightSmall = Amplitude * 0.6f;
	FLatentActionInfo Unused;
	PlayDynamicForceFeedback(Amplitude, Duration, true, true, true, true, EDynamicForceFeedbackAction::Start, Unused);
}

void AIronSiegePlayerController::TogglePhotoMode()
{
	UWorld* World = GetWorld();
	if (!World || IsSettingsMenuOpen())
	{
		return;
	}
	if (PhotoPawn)
	{
		if (PhotoReturnPawn)
		{
			Possess(PhotoReturnPawn);
		}
		PhotoPawn->Destroy();
		PhotoPawn = nullptr;
		PhotoReturnPawn = nullptr;
		World->GetWorldSettings()->SetTimeDilation(1.f);
		if (AHUD* Hud = GetHUD())
		{
			Hud->bShowHUD = true;
		}
		return;
	}
	APawn* Current = GetPawn();
	if (!Current)
	{
		return;
	}
	FVector CamLocation;
	FRotator CamRotation;
	GetPlayerViewPoint(CamLocation, CamRotation);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADefaultPawn* Camera = World->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), CamLocation, CamRotation, Params);
	if (!Camera)
	{
		return;
	}
	Camera->GetMovementComponent()->SetActive(true);
	PhotoReturnPawn = Current;
	PhotoPawn = Camera;
	Possess(Camera);
	// Not a full pause: a trickle of time keeps engines, fire and smoke alive in the shot.
	World->GetWorldSettings()->SetTimeDilation(0.05f);
	if (AHUD* Hud = GetHUD())
	{
		Hud->bShowHUD = false;
	}
}

void AIronSiegePlayerController::RunBenchmark(float Seconds, float TargetFps)
{
	UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	if (!S)
	{
		return;
	}
	CloseSettingsMenu(); // Measure the game, not a paused menu.
	if (AIronSiegeHUD* Hud = Cast<AIronSiegeHUD>(GetHUD()))
	{
		Hud->ShowNotice(IronText::Str(TEXT("NoticeBench"), TEXT("BENCHMARK RUNNING - DRIVE NORMALLY")), FLinearColor(1.f, 0.8f, 0.2f));
	}
	TWeakObjectPtr<AIronSiegePlayerController> WeakThis(this);
	const float Window = FMath::Max(Seconds, 2.f);
	float Elapsed = 0.f;
	int32 Frames = 0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Window, TargetFps, Elapsed, Frames](float Dt) mutable
	{
		AIronSiegePlayerController* PC = WeakThis.Get();
		UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get();
		if (!PC || !Settings)
		{
			return false;
		}
		Elapsed += Dt;
		++Frames;
		if (Elapsed < Window)
		{
			return true;
		}
		const float AverageFps = Frames / FMath::Max(Elapsed, KINDA_SMALL_NUMBER);
		const int32 Current = FMath::Clamp(Settings->GetOverallScalabilityLevel() < 0 ? 2 : Settings->GetOverallScalabilityLevel(), 0, 4);
		const int32 Recommended = IronSettings::RecommendedQuality(Current, AverageFps, TargetFps);
		Settings->SetOverallScalabilityLevel(Recommended);
		Settings->ApplySettings(false);
		PC->OpenSettingsMenu();
		if (SIronSettingsMenu* Menu = PC->GetSettingsMenu().Get())
		{
			Menu->ShowBenchmarkResult(AverageFps, TargetFps, Recommended);
		}
		return false;
	}));
}
