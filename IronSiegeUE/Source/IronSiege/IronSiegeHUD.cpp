#include "IronSiegeHUD.h"
#include "IronVehicle.h"
#include "VehicleHealthComponent.h"
#include "MachineGunComponent.h"
#include "MineLayerComponent.h"
#include "FlamethrowerComponent.h"
#include "RocketLauncherComponent.h"
#include "IronSiegePlayerController.h"
#include "VehicleClassRules.h"
#include "IronSiegeGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "IronSupplyCrate.h"
#include "EngineUtils.h"
#include "IronCityStreet.h"
#include "UpgradeRules.h"
#include "IronSiegeUserSettings.h"
#include "SettingsRules.h"
#include "IronSiegeText.h"
#include "WarVehiclePawn.h"
#include "Camera/PlayerCameraManager.h"
#include "FlareRules.h"
#include "EnergyWeapons.h"
#include "MachineGunComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontCache.h"
#include "Rendering/SlateRenderer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "IronMissionDirector.h"
#include "IronTeams.h"
#include "IronBossComponent.h"

void AIronSiegeHUD::DrawHUD()
{
	// Accessibility: one text/marker scale and one colour-role palette for the whole frame.
	if (const UIronSiegeUserSettings* Access = UIronSiegeUserSettings::Get())
	{
		HudScale = Access->HudScale();
		EnemyColor = Access->RoleColor(IronSettings::ColorEnemy);
		FriendlyColor = Access->RoleColor(IronSettings::ColorFriendly);
		WarningColor = Access->RoleColor(IronSettings::ColorWarning);
		TextOpacity = IronSettings::HudOpacity(Access->HudOpacity);
	}
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	TickRadio();
	if (AIronSiegePlayerController* PC = Cast<AIronSiegePlayerController>(GetOwningPlayerController()))
	{
		switch (PC->GetScreen())
		{
		case EIronScreen::ModeSelect: DrawModeSelect(PC); return;
		case EIronScreen::MapSelect: DrawMainMenu(PC); return;
		case EIronScreen::MissionSelect: DrawMissionSelect(PC); return;
		case EIronScreen::Briefing: DrawBriefing(PC); return;
		case EIronScreen::DriverSelect: DrawDriverSelect(PC); return;
		case EIronScreen::VehicleSelect: DrawVehicleSelect(PC); return;
		default: break;
		}
	}

	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	const AIronMissionDirector* Director = GameMode ? GameMode->GetDirector() : nullptr;
	if (GameMode && GameMode->IsGameOver())
	{
		if (GameMode->IsMissionMode())
		{
			DrawMissionResult(GameMode, Cast<AIronSiegePlayerController>(GetOwningPlayerController()));
		}
		else
		{
			DrawGameOver(GameMode);
		}
		DrawRadio(); // The last words of the mission (or of the driver).
		return;
	}
	if (Director)
	{
		DrawMissionStatus(Director);
		DrawMissionMarkers(Director);
		DrawBossBar(GameMode);
	}
	else if (GameMode)
	{
		DrawWaveStatus(GameMode);
		DrawBossBar(GameMode);
	}

	DrawNotices();
	DrawRadio();

	IIronVehicle* Vehicle = Cast<IIronVehicle>(GetOwningPawn());
	if (!Vehicle)
	{
		return;
	}
	const UIronSiegeUserSettings* UserSettings = UIronSiegeUserSettings::Get();
	DrawSupplyMarkers();
	if (!UserSettings || UserSettings->bShowEnemyHealthBars)
	{
		DrawEnemyHealthBars();
	}
	if (!UserSettings || UserSettings->bShowMinimap)
	{
		DrawMinimap();
	}
	DrawCrosshair(Vehicle);
	DrawLockOn(Cast<AWarVehiclePawn>(GetOwningPawn()));
	DrawMissileWarning(Cast<AWarVehiclePawn>(GetOwningPawn()));
	DrawRailWarning(Cast<AWarVehiclePawn>(GetOwningPawn()));
	DrawBossWarning(GameMode);
	if (!UserSettings || UserSettings->bDamageNumbers)
	{
		DrawDamageNumbers();
	}
	if (UserSettings && UserSettings->bShowFps)
	{
		DrawFps();
	}
	if (GameMode && GameMode->IsShopOpen())
	{
		DrawShop(GameMode, Cast<AIronSiegePlayerController>(GetOwningPlayerController()));
	}

	float Y = 40.f;
	const float X = 40.f;
	const float LineHeight = 24.f * HudScale;

	auto DrawLine = [&](const FString& Text)
	{
		DrawTextLine(FVector2D(X, Y), Text, FLinearColor::White);
		Y += LineHeight;
	};

	const bool bMph = UserSettings && UserSettings->bMph;
	DrawLine(FString::Printf(TEXT("%s: %.0f %s"), *IronText::Str(TEXT("HudSpeed"), TEXT("Speed")), IronSettings::SpeedInUnits(Vehicle->GetSpeedKph(), bMph),
		bMph ? TEXT("mph") : *IronText::Str(TEXT("HudKph"), TEXT("km/h"))));
	if (UVehicleHealthComponent* HealthComp = Vehicle->GetHealthComponent())
	{
		DrawLine(FString::Printf(TEXT("%s: %.0f"), *IronText::Str(TEXT("HudHealth"), TEXT("Health")), HealthComp->GetHealth()));
		DrawLine(FString::Printf(TEXT("%s: %.0f"), *IronText::Str(TEXT("HudArmor"), TEXT("Armor")), HealthComp->GetArmor()));
	}
	if (UFlamethrowerComponent* Flame = Vehicle->GetFlamerComponent(); Flame && Flame->bUnlocked)
	{
		DrawLine(FString::Printf(TEXT("%s: %d / %d"), *IronText::Str(TEXT("HudFuel"), TEXT("Fuel")), Flame->GetAmmo(), Flame->GetReserve()));
	}
	if (UMineLayerComponent* Mines = Vehicle->GetMineWeaponComponent(); Mines && Mines->bUnlocked)
	{
		DrawLine(FString::Printf(TEXT("%s: %d / %d"), *IronText::Str(TEXT("HudMines"), TEXT("Mines")), Mines->GetAmmo(), Mines->GetReserve()));
	}
	if (UMachineGunComponent* Primary = Vehicle->GetPrimaryWeaponComponent())
	{
		DrawLine(FString::Printf(TEXT("%s: %d / %d%s"), *IronText::Str(TEXT("HudMg"), TEXT("MG Ammo")), Primary->GetAmmo(), Primary->GetReserve(),
			Primary->IsReloading() ? *IronText::Str(TEXT("HudReloading"), TEXT(" (reloading)")) : TEXT("")));
	}
	if (URocketLauncherComponent* Secondary = Vehicle->GetSecondaryWeaponComponent())
	{
		DrawLine(FString::Printf(TEXT("%s: %d / %d%s"), *IronText::Str(TEXT("HudRockets"), TEXT("Rockets")), Secondary->GetAmmo(), Secondary->GetReserve(),
			Secondary->IsReloading() ? *IronText::Str(TEXT("HudReloading"), TEXT(" (reloading)")) : TEXT("")));
	}

	if (const AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(GetOwningPawn()))
	{
		DrawLine(FString::Printf(TEXT("%s: %d / %d"), *IronText::Str(TEXT("HudFlares"), TEXT("Flares")), Car->GetFlareCharges(), IronFlares::Tuning().MaxCharges));
		if (const URailgunComponent* Rail = Car->GetRailgun(); Rail && Rail->bUnlocked)
		{
			DrawLine(FString::Printf(TEXT("%s: %d / %d%s"), *IronText::Str(TEXT("HudRailgun"), TEXT("Railgun")), Rail->GetAmmo(), Rail->GetReserve(),
				Car->IsRailgunCharging() ? *IronText::Str(TEXT("HudCharging"), TEXT("  (charging)")) : (Rail->IsReloading() ? *IronText::Str(TEXT("HudReloading"), TEXT(" (reloading)")) : TEXT(""))));
		}
		if (const UTeslaComponent* Coil = Car->GetTesla(); Coil && Coil->bUnlocked)
		{
			DrawLine(FString::Printf(TEXT("%s: %d / %d%s"), *IronText::Str(TEXT("HudTesla"), TEXT("Tesla")), Coil->GetAmmo(), Coil->GetReserve(),
				Coil->IsReloading() ? *IronText::Str(TEXT("HudReloading"), TEXT(" (reloading)")) : TEXT("")));
		}
	}

	DrawLine(TEXT(""));
	DrawLine(IronText::Str(TEXT("HudSettingsHint"), TEXT("Esc / F1: Settings")));
	// Under the readout, wherever it ends: every weapon bought adds a line to it.
	DrawVehicleBars(Cast<AWarVehiclePawn>(GetOwningPawn()), X, Y);
}

void AIronSiegeHUD::NotifyDamage(AActor* Victim, float Amount, bool bKill)
{
	if (!Victim || Amount <= 0.f || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const IronSettings::DamageNumberTuning Tuning;
	for (FDamageNumber& N : DamageNumbers)
	{
		// The machine gun lands a dozen hits a second: add them into the car's live number.
		if (N.Victim.Get() == Victim && Now - N.LastHit < Tuning.MergeSeconds)
		{
			N.Amount += Amount;
			N.LastHit = Now;
			N.Born = Now; // Keep it fresh while the hits keep coming.
			N.bKill |= bKill;
			N.Anchor = Victim->GetActorLocation() + FVector(0.f, 0.f, 230.f);
			return;
		}
	}
	FDamageNumber N;
	N.Victim = Victim;
	N.Anchor = Victim->GetActorLocation() + FVector(0.f, 0.f, 230.f);
	N.Amount = Amount;
	N.Born = N.LastHit = Now;
	N.bKill = bKill;
	DamageNumbers.Add(N);
}

void AIronSiegeHUD::DrawDamageNumbers()
{
	if (!Canvas || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const IronSettings::DamageNumberTuning Tuning;
	DamageNumbers.RemoveAll([&](const FDamageNumber& N) { return Now - N.Born >= Tuning.LifeSeconds; });
	for (const FDamageNumber& N : DamageNumbers)
	{
		const float Age = static_cast<float>(Now - N.Born);
		const FVector World = N.Anchor + FVector(0.f, 0.f, Tuning.RiseCmPerSecond * Age);
		const FVector Screen = Canvas->Project(World);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		FLinearColor Color = N.bKill ? FLinearColor(1.f, 0.35f, 0.2f) : FLinearColor(1.f, 0.9f, 0.4f);
		Color.A = IronSettings::DamageNumberAlpha(Age, Tuning);
		const FString Text = FString::Printf(TEXT("%.0f"), N.Amount);
		const FVector2D Size = MeasureTextLine(Text, 1);
		DrawTextLine(FVector2D(Screen.X - Size.X * 0.5f, Screen.Y), Text, Color, N.bKill ? 2 : 1);
	}
}

void AIronSiegeHUD::DrawMainMenu(class AIronSiegePlayerController* PC)
{
	// Survival: which battlefield.
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const FVector2D Size(760.f, 400.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);
	const FLinearColor Dim(0.62f, 0.64f, 0.68f);
	DrawCentred(TopLeft.Y + 34.f, IronText::Str(TEXT("MenuSurvival"), TEXT("SURVIVAL")), FLinearColor::Yellow, 2);
	DrawCentred(TopLeft.Y + 120.f, IronText::Str(TEXT("HudBattlefield"), TEXT("Battlefield")), Dim, 1);
	DrawCentred(TopLeft.Y + 165.f, FString::Printf(TEXT("<   %s   >"), *IronText::Name(TEXT("Map"), PC->GetSelectedMapName())), FLinearColor(0.4f, 1.f, 0.4f), 2);
	if (const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr)
	{
		DrawCentred(TopLeft.Y + 250.f, FString::Format(*IronText::Str(TEXT("MenuBest"), TEXT("Best: {0} pts, wave {1}")), { GameMode->GetBestScore(), GameMode->GetBestWave() }), Dim);
	}
	DrawCentred(TopLeft.Y + Size.Y - 50.f, IronText::Str(TEXT("MenuKeys"), TEXT("F3/F4: Choose     Enter: Confirm     F2: Back")), Dim);
}

void AIronSiegeHUD::DrawWaveStatus(const AIronSiegeGameMode* GameMode)
{
	// Top-centre status line; the break countdown replaces the enemy count between waves.
	const float Countdown = GameMode->IsShopOpen() ? GameMode->GetShopTimeLeft() : GameMode->GetNextWaveCountdown();
	const FString KillsAndScore = FString::Printf(TEXT("   |   %s: %d   |   %s: %d"),
		*IronText::Str(TEXT("HudKills"), TEXT("Kills")), GameMode->GetKills(), *IronText::Str(TEXT("HudScore"), TEXT("Score")), GameMode->GetScore());
	const FString Status = GameMode->IsShopOpen()
		? FString::Format(*IronText::Str(TEXT("HudWaveClearedShop"), TEXT("WAVE {0} CLEARED  -  SHOP OPEN")), { GameMode->GetWave() }) + KillsAndScore
		: Countdown >= 0.f
		? FString::Format(*IronText::Str(TEXT("HudWaveClearedNext"), TEXT("WAVE {0} CLEARED  -  next wave in {1}")), { GameMode->GetWave(), FMath::CeilToInt(Countdown) }) + KillsAndScore
		: FString::Printf(TEXT("%s %d   |   %s: %d   |   %s: %d   |   %s: %d"),
			*IronText::Str(TEXT("HudWave"), TEXT("WAVE")), GameMode->GetWave(),
			*IronText::Str(TEXT("HudEnemies"), TEXT("Enemies")), GameMode->GetEnemiesAlive(),
			*IronText::Str(TEXT("HudKills"), TEXT("Kills")), GameMode->GetKills(),
			*IronText::Str(TEXT("HudScore"), TEXT("Score")), GameMode->GetScore());

	const FVector2D StatusSize = MeasureTextLine(Status);
	const float TextW = StatusSize.X, TextH = StatusSize.Y;
	const float X = (Canvas->ClipX - TextW) * 0.5f;
	const float Y = 30.f;
	FCanvasTileItem Tile(FVector2D(X - 16.f, Y - 8.f), FVector2D(TextW + 32.f, TextH + 16.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
	DrawTextLine(FVector2D(X, Y), Status, Countdown >= 0.f ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor::White);

	// Live combo under the status bar.
	if (const int32 Combo = GameMode->GetActiveCombo(); Combo > 1)
	{
		const FString ComboText = FString::Printf(TEXT("%s x%d"), *IronText::Str(TEXT("HudCombo"), TEXT("COMBO")), Combo);
		const float CW = MeasureTextLine(ComboText, 1).X;
		DrawTextLine(FVector2D((Canvas->ClipX - CW) * 0.5f, Y + TextH + 14.f), ComboText, FLinearColor(1.f, 0.8f, 0.2f), 1);
	}
}

void AIronSiegeHUD::DrawGameOver(const AIronSiegeGameMode* GameMode)
{
	// Survival: the run's result against the best one, and what the driver earned.
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const FVector2D Size(760.f, 490.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f - 40.f);
	DrawPanel(TopLeft, Size, 0.86f);
	const FLinearColor Dim(0.62f, 0.64f, 0.68f);
	DrawCentred(TopLeft.Y + 34.f, IronText::Str(TEXT("HudDestroyed"), TEXT("VEHICLE DESTROYED")), FLinearColor(1.f, 0.3f, 0.2f), 2);
	float Y = TopLeft.Y + 120.f;
	auto Row = [&](const TCHAR* Key, const TCHAR* English, int32 Value, const FLinearColor& Color)
	{
		DrawCentred(Y, FString::Printf(TEXT("%s:  %d"), *IronText::Str(Key, English), Value), Color, 1);
		Y += 42.f;
	};
	Row(TEXT("HudReachedWave"), TEXT("Reached wave"), GameMode->GetWave(), FLinearColor::White);
	Row(TEXT("HudKills"), TEXT("Kills"), GameMode->GetKills(), FLinearColor::White);
	Row(TEXT("HudScore"), TEXT("Score"), GameMode->GetScore(), FLinearColor(1.f, 0.9f, 0.4f));
	DrawCentred(Y + 8.f, IronText::Str(TEXT("HudBest"), TEXT("Best")) + TEXT(":  ") + FString::Format(*IronText::Str(TEXT("HudBestLine"), TEXT("{0} pts  (wave {1}, {2} kills)")),
		{ GameMode->GetBestScore(), GameMode->GetBestWave(), GameMode->GetBestKills() }), FLinearColor::Yellow);
	float XpY = Y + 56.f;
	DrawDriverXp(GameMode, TopLeft.X + 50.f, XpY, Size.X - 100.f);
	DrawCentred(TopLeft.Y + Size.Y - 60.f, IronText::Str(TEXT("HudRedeploy"), TEXT("Enter: Redeploy")) + TEXT("     ") + IronText::Str(TEXT("HudMenuBack"), TEXT("F2: Main menu")), Dim);
}

void AIronSiegeHUD::DrawVehicleSelect(class AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const int32 Index = PC->GetPendingClassIndex();
	const IronVehicles::ClassStats& Stats = IronVehicles::Get(static_cast<IronVehicles::VehicleClass>(Index));
	const FVector2D Size(900.f, 560.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);
	const FLinearColor Dim(0.62f, 0.64f, 0.68f);
	const float X = TopLeft.X + 60.f, W = Size.X - 120.f;
	DrawAligned(X, TopLeft.Y + 30.f, W, IronText::Str(TEXT("HudChooseCar"), TEXT("CHOOSE YOUR VEHICLE")), FLinearColor::Yellow, 2);
	float Y = TopLeft.Y + 100.f;
	DrawAligned(X, Y, W, FString::Printf(TEXT("<  %s  >"), *IronText::Name(TEXT("Car"), ANSI_TO_TCHAR(Stats.Name))), FLinearColor(0.4f, 1.f, 0.4f), 2);
	Y += 52.f;
	DrawAligned(X, Y, W, IronText::Str(*(FString(TEXT("Blurb")) + ANSI_TO_TCHAR(Stats.Name)), ANSI_TO_TCHAR(Stats.Blurb)), FLinearColor::White, 1);
	Y += 60.f;

	// The class's numbers as bars against the best in the garage, so a glance compares two cars.
	float MaxSpeed = 1.f, MaxHealth = 1.f, MaxArmor = 1.f, MaxMass = 1.f;
	for (int32 i = 0; i < IronVehicles::Count; ++i)
	{
		const IronVehicles::ClassStats& Other = IronVehicles::Get(static_cast<IronVehicles::VehicleClass>(i));
		MaxSpeed = FMath::Max(MaxSpeed, Other.TopSpeedKph);
		MaxHealth = FMath::Max(MaxHealth, Other.MaxHealth);
		MaxArmor = FMath::Max(MaxArmor, Other.MaxArmor);
		MaxMass = FMath::Max(MaxMass, Other.Mass);
	}
	const bool bRtl = IronText::IsArabic();
	const float LabelW = 200.f, BarW = 340.f, Gap = 20.f;
	auto Stat = [&](const TCHAR* Key, const TCHAR* English, float Fraction, const FString& Value, const FLinearColor& Fill)
	{
		const FString Label = IronText::Str(Key, English);
		const float BarX = bRtl ? X + W - LabelW - Gap - BarW : X + LabelW + Gap;
		DrawAligned(bRtl ? X + W - LabelW : X, Y, LabelW, Label, FLinearColor::White, 1);
		DrawBar(FVector2D(BarX, Y + 6.f), FVector2D(BarW, 14.f), Fraction, Fill);
		DrawTextLine(FVector2D(bRtl ? BarX - Gap - MeasureTextLine(Value, 1).X : BarX + BarW + Gap, Y), Value, Dim, 1);
		Y += 44.f;
	};
	Stat(TEXT("HudTopSpeed"), TEXT("Top Speed"), Stats.TopSpeedKph / MaxSpeed, FString::Printf(TEXT("%.0f %s"), Stats.TopSpeedKph, *IronText::Str(TEXT("HudKph"), TEXT("km/h"))), FLinearColor(0.3f, 0.75f, 1.f));
	Stat(TEXT("HudHealth"), TEXT("Health"), Stats.MaxHealth / MaxHealth, FString::Printf(TEXT("%.0f"), Stats.MaxHealth), FLinearColor(0.35f, 0.9f, 0.45f));
	Stat(TEXT("HudArmor"), TEXT("Armor"), Stats.MaxArmor / MaxArmor, FString::Printf(TEXT("%.0f"), Stats.MaxArmor), FLinearColor(0.45f, 0.6f, 1.f));
	Stat(TEXT("HudMass"), TEXT("Weight"), Stats.Mass / MaxMass, FString::Printf(TEXT("%.1f %s"), Stats.Mass / 1000.f, *IronText::Str(TEXT("HudTonnes"), TEXT("t"))), FLinearColor(0.75f, 0.7f, 0.6f));
	DrawAligned(X, Y + 8.f, W, IronText::Str(*(FString(TEXT("Trait")) + ANSI_TO_TCHAR(Stats.Name)), ANSI_TO_TCHAR(Stats.Trait)), FLinearColor(1.f, 0.8f, 0.35f), 1);
	DrawCentred(TopLeft.Y + Size.Y - 50.f, IronText::Str(TEXT("MenuKeys"), TEXT("F3/F4: Choose     Enter: Confirm     F2: Back")), Dim);
}

void AIronSiegeHUD::ShowNotice(const FString& Text, const FLinearColor& Color)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Notices.Add({ Text, Color, Now + 2.5 });
}

void AIronSiegeHUD::DrawNotices()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Notices.RemoveAll([Now](const FNotice& N) { return N.ExpireTime <= Now; });
	float Y = Canvas->ClipY * 0.8f;
	for (const FNotice& N : Notices)
	{
		const FVector2D Size = MeasureTextLine(N.Text, 2);
		FLinearColor Color = N.Color;
		Color.A = FMath::Clamp(static_cast<float>(N.ExpireTime - Now) / 0.5f, 0.f, 1.f); // Fade out over the last 0.5s.
		DrawTextLine(FVector2D((Canvas->ClipX - Size.X) * 0.5f, Y), N.Text, Color, 2);
		Y += Size.Y + 6.f;
	}
}

void AIronSiegeHUD::DrawCrosshair(const IIronVehicle* Vehicle)
{
	const FVector AimPoint = Vehicle->GetAimPoint();
	if (AimPoint.IsZero())
	{
		return;
	}
	const FVector Screen = Canvas->Project(AimPoint);
	if (Screen.Z <= 0.f)
	{
		return; // Behind the camera.
	}
	const FVector2D C(Screen.X, Screen.Y);
	FLinearColor Color(1.f, 1.f, 1.f, 0.85f);
	auto Line = [&](const FVector2D& A, const FVector2D& B)
	{
		FCanvasLineItem Item(A, B);
		Item.SetColor(Color);
		Item.LineThickness = 2.f;
		Canvas->DrawItem(Item);
	};
	const UIronSiegeUserSettings* Style = UIronSiegeUserSettings::Get();
	const int32 CrosshairStyle = Style ? Style->CrosshairStyle : 0;
	// Cone half-angle -> pixels at the crosshair: tan(spread) scaled by the projection (with the
	// camera's real field of view: the setting and the nitro kick both change it).
	const float HalfWidth = Canvas->ClipX * 0.5f;
	const float HalfFovRad = FMath::DegreesToRadians(0.5f * (PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f));
	const float PixelsPerRadian = HalfWidth / FMath::Tan(HalfFovRad);
	float SpreadPixels = 0.f;
	if (const UMachineGunComponent* Gun = Vehicle->GetPrimaryWeaponComponent())
	{
		SpreadPixels = FMath::Tan(FMath::DegreesToRadians(Gun->GetSpreadDeg())) * PixelsPerRadian;
		SpreadPixels = FMath::Clamp(SpreadPixels, 0.f, 90.f);
		if (Gun->IsOverheated())
		{
			Color = FLinearColor(1.f, 0.3f, 0.2f, Color.A);
		}
	}
	if (CrosshairStyle == 0)
	{
		const float Gap = 6.f + SpreadPixels, Len = 12.f;
		Line(C + FVector2D(Gap, 0.f), C + FVector2D(Gap + Len, 0.f));
		Line(C - FVector2D(Gap, 0.f), C - FVector2D(Gap + Len, 0.f));
		Line(C + FVector2D(0.f, Gap), C + FVector2D(0.f, Gap + Len));
		Line(C - FVector2D(0.f, Gap), C - FVector2D(0.f, Gap + Len));
	}
	else if (CrosshairStyle == 2)
	{
		const int32 Segments = 20;
		const float Radius = 11.f + SpreadPixels;
		for (int32 i = 0; i < Segments; ++i)
		{
			const float A0 = 2.f * PI * i / Segments, A1 = 2.f * PI * (i + 1) / Segments;
			Line(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius);
		}
	}
	if (CrosshairStyle != 3)
	{
		const float DotSize = CrosshairStyle == 1 ? 6.f : 3.f;
		FCanvasTileItem Dot(C - FVector2D(DotSize * 0.5f, DotSize * 0.5f), FVector2D(DotSize, DotSize), Color);
		Canvas->DrawItem(Dot);
	}

	// Aim assist: corner brackets round the car the gun is helping onto, sized to it on screen.
	const AWarVehiclePawn* Me = Cast<AWarVehiclePawn>(GetOwningPawn());
	if (const AWarVehiclePawn* Target = Me ? Cast<AWarVehiclePawn>(Me->GetAimTarget()) : nullptr)
	{
		const FVector At = Canvas->Project(Target->GetBodyCenter());
		const float Dist = FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation());
		if (At.Z > 0.f && Dist > 1.f)
		{
			const float Half = FMath::Clamp(PixelsPerRadian * 160.f / Dist, 14.f, 90.f);
			const float Arm = Half * 0.35f;
			const FLinearColor Bracket(EnemyColor.R, EnemyColor.G, EnemyColor.B, 0.9f);
			for (const FVector2D& S : { FVector2D(1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f) })
			{
				const FVector2D Corner(At.X + S.X * Half, At.Y + S.Y * Half * 0.7f);
				for (const FVector2D& Along : { FVector2D(-S.X * Arm, 0.f), FVector2D(0.f, -S.Y * Arm) })
				{
					FCanvasLineItem Item(Corner, Corner + Along);
					Item.SetColor(Bracket);
					Item.LineThickness = 2.f;
					Canvas->DrawItem(Item);
				}
			}
		}
	}

	// Hit marker: diagonal ticks for a moment after a hit, red on a kill.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float HitAge = static_cast<float>(Now - LastHitTime);
	if (HitAge < (bLastHitKill ? 0.45f : 0.18f))
	{
		const FLinearColor HitColor = bLastHitKill ? FLinearColor(1.f, 0.2f, 0.15f) : FLinearColor::White;
		for (const FVector2D& D : { FVector2D(1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f) })
		{
			FCanvasLineItem Item(C + D * 9.f, C + D * 18.f);
			Item.SetColor(HitColor);
			Item.LineThickness = 2.5f;
			Canvas->DrawItem(Item);
		}
	}
}

void AIronSiegeHUD::DrawSupplyMarkers()
{
	const APawn* Pawn = GetOwningPawn();
	if (!Pawn)
	{
		return;
	}
	for (TActorIterator<AIronSupplyCrate> It(GetWorld()); It; ++It)
	{
		const FVector Loc = It->GetActorLocation() + FVector(0.f, 0.f, 160.f);
		const FVector Screen = Canvas->Project(Loc);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const bool bRepair = It->SupplyType == EIronSupplyType::Repair;
		const FLinearColor Color = bRepair ? FriendlyColor : WarningColor;
		// Keep off-screen markers pinned to the screen edge so drops are always findable.
		const float X = FMath::Clamp(Screen.X, 40.f, Canvas->ClipX - 120.f);
		const float Y = FMath::Clamp(Screen.Y, 90.f, Canvas->ClipY - 60.f);
		const FString Label = FString::Printf(TEXT("%s %.0f %s"), *IronText::Name(TEXT("Crate"), It->GetLabel()), FVector::Dist(Pawn->GetActorLocation(), It->GetActorLocation()) / 100.f,
			*IronText::Str(TEXT("HudMetres"), TEXT("m")));
		DrawTextLine(FVector2D(X, Y), Label, Color);
	}
}

void AIronSiegeHUD::NotifyHitMarker(bool bKill)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastHitTime = Now;
	bLastHitKill = bKill;
	// The machine gun lands ~12 hits a second; keep the tick audible without machine-gunning it.
	const UIronSiegeUserSettings* HitSettings = UIronSiegeUserSettings::Get();
	if ((!HitSettings || HitSettings->bHitSounds) && (Now - LastHitSoundTime > 0.07 || bKill))
	{
		LastHitSoundTime = Now;
		if (USoundBase* Sound = HitSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Sound, bKill ? 1.3f : 1.f, bKill ? 0.7f : 1.f);
		}
	}
}

void AIronSiegeHUD::DrawEnemyHealthBars()
{
	const APawn* Me = GetOwningPawn();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Other = *It;
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(Other);
		const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (Other == Me || !Health || Health->IsDestroyed() || IronTeams::IsPlayerSide(Other) || (Me && FVector::Dist(Me->GetActorLocation(), Other->GetActorLocation()) > 7000.f))
		{
			continue;
		}
		const FVector Screen = Canvas->Project(Other->GetActorLocation() + FVector(0.f, 0.f, 260.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float W = 80.f * HudScale, H = 7.f * HudScale;
		const FVector2D TopLeft(Screen.X - W * 0.5f, Screen.Y);
		const float HealthFrac = Health->GetMaxHealth() > 0.f ? FMath::Clamp(Health->GetHealth() / Health->GetMaxHealth(), 0.f, 1.f) : 0.f;
		const float ArmorFrac = Health->GetMaxArmor() > 0.f ? FMath::Clamp(Health->GetArmor() / Health->GetMaxArmor(), 0.f, 1.f) : 0.f;
		FCanvasTileItem Back(TopLeft - FVector2D(1.f, 1.f), FVector2D(W + 2.f, H + 5.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		FCanvasTileItem Bar(TopLeft, FVector2D(W * HealthFrac, H), FMath::Lerp(EnemyColor, FriendlyColor, HealthFrac));
		Canvas->DrawItem(Bar);
		FCanvasTileItem Armor(TopLeft + FVector2D(0.f, H + 1.f), FVector2D(W * ArmorFrac, 2.f), FLinearColor(0.35f, 0.65f, 1.f));
		Canvas->DrawItem(Armor);
	}
}

namespace
{
// Liang-Barsky: clips segment P0-P1 to the axis-aligned box [Min, Max]; false if fully outside.
bool ClipSegment(FVector2D& P0, FVector2D& P1, const FVector2D& Min, const FVector2D& Max)
{
	const FVector2D D = P1 - P0;
	float T0 = 0.f, T1 = 1.f;
	const float P[4] = { -D.X, D.X, -D.Y, D.Y };
	const float Q[4] = { P0.X - Min.X, Max.X - P0.X, P0.Y - Min.Y, Max.Y - P0.Y };
	for (int32 i = 0; i < 4; ++i)
	{
		if (FMath::IsNearlyZero(P[i]))
		{
			if (Q[i] < 0.f) return false;
			continue;
		}
		const float R = Q[i] / P[i];
		if (P[i] < 0.f) T0 = FMath::Max(T0, R);
		else T1 = FMath::Min(T1, R);
		if (T0 > T1) return false;
	}
	const FVector2D A = P0 + D * T0, B = P0 + D * T1;
	P0 = A;
	P1 = B;
	return true;
}
}

void AIronSiegeHUD::DrawMinimap()
{
	const APawn* Me = GetOwningPawn();
	if (!Me)
	{
		return;
	}
	// Heading-up radar in the top-right corner: 90m across.
	const float Size = 190.f, Margin = 24.f;
	const FVector2D Min(Canvas->ClipX - Size - Margin, Margin + 50.f);
	const FVector2D Max = Min + FVector2D(Size, Size);
	const FVector2D Centre = (Min + Max) * 0.5f;
	const UIronSiegeUserSettings* RadarSettings = UIronSiegeUserSettings::Get();
	const float WorldRadius = IronSettings::MinimapRangeCm(RadarSettings ? RadarSettings->MinimapZoom : 1);
	const float Scale = (Size * 0.5f) / WorldRadius;
	const FVector Origin = Me->GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(Me->GetActorRotation().Yaw);
	const float CosY = FMath::Cos(Yaw), SinY = FMath::Sin(Yaw);
	// World XY -> radar screen: forward is up, right is right.
	auto ToRadar = [&](const FVector2D& World) -> FVector2D
	{
		const FVector2D Rel(World.X - Origin.X, World.Y - Origin.Y);
		const float Forward = Rel.X * CosY + Rel.Y * SinY;
		const float Right = -Rel.X * SinY + Rel.Y * CosY;
		return Centre + FVector2D(Right, -Forward) * Scale;
	};

	FCanvasTileItem Back(Min, Max - Min, FLinearColor(0.01f, 0.015f, 0.025f, 0.82f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);

	// Roads.
	for (TActorIterator<AIronCityStreet> It(GetWorld()); It; ++It)
	{
		for (const FIronRoadSegment& Seg : It->GetRoadSegments())
		{
			FVector2D A = ToRadar(Seg.A), B = ToRadar(Seg.B);
			const float Thickness = FMath::Max(Seg.Width * Scale * 0.8f, 2.f);
			const FVector2D Inset(Thickness * 0.5f + 1.f, Thickness * 0.5f + 1.f);
			if (ClipSegment(A, B, Min + Inset, Max - Inset))
			{
				FCanvasLineItem Road(A, B);
				Road.SetColor(FLinearColor(0.32f, 0.35f, 0.4f, 1.f));
				Road.LineThickness = Thickness;
				Canvas->DrawItem(Road);
			}
		}
	}

	// Blips: clamped to the radar edge when out of range so threats stay visible.
	auto Blip = [&](const FVector& World, const FLinearColor& Color, float Radius)
	{
		FVector2D P = ToRadar(FVector2D(World.X, World.Y));
		const bool bOutside = P.X < Min.X || P.X > Max.X || P.Y < Min.Y || P.Y > Max.Y;
		P.X = FMath::Clamp(P.X, Min.X + 4.f, Max.X - 4.f);
		P.Y = FMath::Clamp(P.Y, Min.Y + 4.f, Max.Y - 4.f);
		const float R = bOutside ? Radius * 0.7f : Radius;
		FCanvasTileItem Dot(P - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), Color);
		Canvas->DrawItem(Dot);
	};
	for (TActorIterator<AIronSupplyCrate> It(GetWorld()); It; ++It)
	{
		Blip(It->GetActorLocation(), It->SupplyType == EIronSupplyType::Repair ? FriendlyColor : WarningColor, 4.f);
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const IIronVehicle* Vehicle = Cast<IIronVehicle>(*It);
		const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
		if (*It != Me && Health && !Health->IsDestroyed())
		{
			Blip(It->GetActorLocation(), IronTeams::IsPlayerSide(*It) ? FriendlyColor : EnemyColor, 4.5f);
		}
	}
	if (const AIronSiegeGameMode* RadarMode = GetWorld()->GetAuthGameMode<AIronSiegeGameMode>(); RadarMode && RadarMode->GetDirector())
	{
		TArray<FIronMissionMarker> Markers;
		RadarMode->GetDirector()->GetMarkers(Markers);
		for (const FIronMissionMarker& M : Markers)
		{
			Blip(M.Location, M.bFriendly ? FriendlyColor : WarningColor, 5.5f);
		}
	}

	// The player: a small arrow pointing up (heading-up radar).
	const FLinearColor Me_Color(0.4f, 0.85f, 1.f);
	auto Edge = [&](const FVector2D& A, const FVector2D& B)
	{
		FCanvasLineItem L(A, B);
		L.SetColor(Me_Color);
		L.LineThickness = 2.5f;
		Canvas->DrawItem(L);
	};
	const FVector2D Tip = Centre + FVector2D(0.f, -9.f), LeftTail = Centre + FVector2D(-6.f, 7.f), RightTail = Centre + FVector2D(6.f, 7.f);
	Edge(Tip, LeftTail);
	Edge(Tip, RightTail);
	Edge(LeftTail, RightTail);

	// Frame.
	const FLinearColor Frame(1.f, 1.f, 1.f, 0.35f);
	for (const TPair<FVector2D, FVector2D>& S : { TPair<FVector2D, FVector2D>(Min, FVector2D(Max.X, Min.Y)), TPair<FVector2D, FVector2D>(FVector2D(Max.X, Min.Y), Max),
		TPair<FVector2D, FVector2D>(Max, FVector2D(Min.X, Max.Y)), TPair<FVector2D, FVector2D>(FVector2D(Min.X, Max.Y), Min) })
	{
		FCanvasLineItem L(S.Key, S.Value);
		L.SetColor(Frame);
		Canvas->DrawItem(L);
	}
}

void AIronSiegeHUD::DrawShop(const AIronSiegeGameMode* GameMode, const AIronSiegePlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const IronUpgrades::Loadout& Up = GameMode->GetUpgrades();
	const float W = 620.f, LineH = 30.f;
	const float X = 40.f, Y0 = Canvas->ClipY * 0.5f - 150.f;
	FCanvasTileItem Back(FVector2D(X - 18.f, Y0 - 18.f), FVector2D(W, (IronUpgrades::Count + 4) * LineH + 30.f), FLinearColor(0.f, 0.f, 0.f, 0.72f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);

	float Y = Y0;
	auto Line = [&](const FString& Text, const FLinearColor& Color)
	{
		DrawTextLine(FVector2D(X, Y), Text, Color);
		Y += LineH;
	};
	Line(FString::Printf(TEXT("=== %s ===    %s: %d    (%.0f%s)"), *IronText::Str(TEXT("HudShop"), TEXT("UPGRADE SHOP")), *IronText::Str(TEXT("HudCredits"), TEXT("Credits")),
		Up.Credits, GameMode->GetShopTimeLeft(), *IronText::Str(TEXT("HudSec"), TEXT("s"))), FLinearColor::Yellow);
	const bool bRtl = IronText::IsArabic();
	const float Inner = W - 36.f;
	auto Cell = [&](float Offset, const FString& Text, const FLinearColor& Color)
	{
		const float At = bRtl ? X + Inner - Offset - MeasureTextLine(Text).X : X + Offset;
		DrawTextLine(FVector2D(At, Y), Text, Color);
	};
	for (int32 i = 0; i < IronUpgrades::Count; ++i)
	{
		const IronUpgrades::Upgrade Item = static_cast<IronUpgrades::Upgrade>(i);
		const IronUpgrades::UpgradeDef& Def = IronUpgrades::Get(Item);
		const int32 Cost = Up.CostOf(Item);
		const FString Level = Def.MaxLevel > 0 ? FString::Printf(TEXT("%s %d/%d"), *IronText::Str(TEXT("HudLv"), TEXT("Lv")), Up.Level(Item), Def.MaxLevel) : TEXT("");
		const FString Price = Cost < 0 ? IronText::Str(TEXT("HudMax"), TEXT("MAX")) : FString::Printf(TEXT("%d"), Cost);
		const bool bSelected = PC->GetShopIndex() == i;
		const FLinearColor Color = Cost < 0 ? FLinearColor(0.5f, 0.5f, 0.5f) : (Up.CanBuy(Item) ? FLinearColor(0.45f, 1.f, 0.5f) : FLinearColor(1.f, 0.45f, 0.4f));
		// Fixed columns (name, level, price, effect): text widths differ too much between the two
		// languages for space padding to line anything up. Arabic mirrors them, name on the right.
		const FLinearColor Shade = bSelected ? Color : Color * 0.8f;
		Cell(0.f, bSelected ? TEXT(">") : TEXT(" "), Shade);
		Cell(18.f, IronText::Name(TEXT("Up"), ANSI_TO_TCHAR(Def.Name)), Shade);
		Cell(178.f, Level, Shade);
		Cell(252.f, Price, Shade);
		Cell(300.f, IronText::Name(TEXT("Up"), ANSI_TO_TCHAR(Def.Effect)), Shade);
		Y += LineH;
	}
	Line(FString::Printf(TEXT("%s %s"), PC->GetShopIndex() >= IronUpgrades::Count ? TEXT(">") : TEXT(" "), *IronText::Str(TEXT("HudContinue"), TEXT("CONTINUE TO NEXT WAVE"))), FLinearColor::White);
	Line(IronText::Str(TEXT("HudShopKeys"), TEXT("F3/F4: Select     Enter: Buy / Continue")), FLinearColor::Gray);
}

void AIronSiegeHUD::DrawBossBar(const AIronSiegeGameMode* GameMode)
{
	const APawn* Boss = GameMode->GetBoss();
	const IIronVehicle* Vehicle = Cast<IIronVehicle>(Boss);
	const UVehicleHealthComponent* Health = Vehicle ? Vehicle->GetHealthComponent() : nullptr;
	if (!Health || Health->IsDestroyed())
	{
		return;
	}
	const float W = 420.f, H = 14.f;
	const FVector2D TopLeft((Canvas->ClipX - W) * 0.5f, 104.f);
	const float Total = Health->GetMaxHealth() + Health->GetMaxArmor();
	const float Frac = Total > 0.f ? FMath::Clamp((Health->GetHealth() + Health->GetArmor()) / Total, 0.f, 1.f) : 0.f;
	FCanvasTileItem Back(TopLeft - FVector2D(2.f, 2.f), FVector2D(W + 4.f, H + 4.f), FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	// The phase shows in the bar: blue while its shield is up, red in the last phase.
	const UIronBossComponent* Brain = Boss->FindComponentByClass<UIronBossComponent>();
	FLinearColor Fill(0.95f, 0.55f, 0.1f);
	if (Brain && Brain->IsShielded())
	{
		Fill = FLinearColor(0.35f, 0.7f, 1.f);
	}
	else if (Brain && Brain->GetPhase() >= IronBoss::PhaseCount)
	{
		Fill = FLinearColor(1.f, 0.25f, 0.12f);
	}
	FCanvasTileItem Bar(TopLeft, FVector2D(W * Frac, H), Fill);
	Canvas->DrawItem(Bar);
	FString Title = GameMode->GetBossName();
	if (Brain)
	{
		// A notch where each phase begins.
		for (int32 i = 1; i < IronBoss::PhaseCount; ++i)
		{
			const float NotchX = TopLeft.X + W * (1.f - static_cast<float>(i) / IronBoss::PhaseCount);
			FCanvasLineItem Notch(FVector2D(NotchX, TopLeft.Y - 3.f), FVector2D(NotchX, TopLeft.Y + H + 3.f));
			Notch.SetColor(FLinearColor::White);
			Notch.LineThickness = 2.f;
			Canvas->DrawItem(Notch);
		}
		Title += FString::Printf(TEXT("   %s %d/%d"), *IronText::Str(TEXT("HudBossPhase"), TEXT("PHASE")), Brain->GetPhase(), IronBoss::PhaseCount);
		if (Brain->IsShielded())
		{
			Title += TEXT("   ") + IronText::Str(TEXT("HudBossShield"), TEXT("SHIELDED"));
		}
	}
	DrawTextLine(TopLeft + FVector2D(0.f, -20.f), Title, FLinearColor(1.f, 0.6f, 0.2f));
}

void AIronSiegeHUD::DrawBossWarning(const AIronSiegeGameMode* GameMode)
{
	const APawn* Boss = GameMode ? GameMode->GetBoss() : nullptr;
	const UIronBossComponent* Brain = Boss ? Boss->FindComponentByClass<UIronBossComponent>() : nullptr;
	const APawn* Me = GetOwningPawn();
	if (!Brain || !Me || !Canvas)
	{
		return;
	}
	const float Distance = FVector::Dist2D(Boss->GetActorLocation(), Me->GetActorLocation());
	FString Banner;
	FLinearColor Color;
	float Progress = 0.f;
	if (Brain->IsSlamWinding() && Distance <= Brain->GetSlamRadius() * 1.1f)
	{
		Banner = IronText::Str(TEXT("HudSlamWarn"), TEXT("SHOCKWAVE - GET CLEAR!"));
		Color = FLinearColor(1.f, 0.55f, 0.15f);
		Progress = Brain->GetSlamProgress();
	}
	else if (Brain->IsEmpWinding() && Distance <= Brain->GetEmpRadius() * 1.1f)
	{
		Banner = IronText::Str(TEXT("HudEmpWarn"), TEXT("EMP CHARGING - GET CLEAR!"));
		Color = FLinearColor(0.45f, 0.75f, 1.f);
		Progress = Brain->GetEmpProgress();
	}
	else
	{
		return;
	}
	// Below the railgun warning, with a bar that fills until it lands.
	const bool bFlashOn = FMath::Fmod(FPlatformTime::Seconds(), 0.2) < 0.12;
	const FVector2D Size = MeasureTextLine(Banner, 2);
	const FVector2D At((Canvas->ClipX - Size.X) * 0.5f, Canvas->ClipY * 0.35f);
	FCanvasTileItem Strip(FVector2D(At.X - 16.f, At.Y - 6.f), FVector2D(Size.X + 32.f, Size.Y + 26.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Strip.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Strip);
	DrawTextLine(At, Banner, bFlashOn ? Color : FLinearColor::White, 2);
	DrawBar(FVector2D(At.X, At.Y + Size.Y + 6.f), FVector2D(Size.X, 6.f), Progress, Color);
}

void AIronSiegeHUD::DrawVehicleBars(const AWarVehiclePawn* Car, float X, float Y)
{
	if (!Car || !Canvas)
	{
		return;
	}
	const float W = 190.f * HudScale, H = 9.f * HudScale;
	auto Bar = [&](float Fraction, const FLinearColor& Fill, const FString& Label, float Row)
	{
		const FVector2D TopLeft(X, Y + Row * (H + 16.f * HudScale));
		FCanvasTileItem Back(TopLeft - FVector2D(1.f, 1.f), FVector2D(W + 2.f, H + 2.f), FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		FCanvasTileItem Fore(TopLeft, FVector2D(W * FMath::Clamp(Fraction, 0.f, 1.f), H), Fill);
		Canvas->DrawItem(Fore);
		DrawTextLine(TopLeft + FVector2D(W + 10.f * HudScale, -3.f * HudScale), Label, Fill);
	};

	// Nitro: pulses while it is being burned so the meter draining is obvious in the corner of the eye.
	const float Charge = Car->GetBoostCharge();
	const bool bBoosting = Car->IsBoosting();
	const float Pulse = bBoosting ? 0.75f + 0.25f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds()) * 18.f) : 1.f;
	Bar(Charge, FLinearColor(0.3f, 0.75f, 1.f, 1.f) * Pulse,
		IronText::Str(TEXT("HudNitro"), TEXT("NITRO")), 0.f);

	float Row = 1.f;
	// The driver's ability: fills as it comes off cooldown, gold and named with its key when ready,
	// bright while it runs.
	if (Car->GetDriverIndex() >= 0)
	{
		const IronCrew::DriverDef& Def = IronCrew::Get(static_cast<IronCrew::Driver>(Car->GetDriverIndex()));
		const IronCrew::AbilityState& State = Car->GetAbility();
		const UIronSiegeUserSettings* KeySettings = UIronSiegeUserSettings::Get();
		const FString Key = KeySettings ? KeySettings->GetKeyFor(TEXT("Ability")).GetDisplayName().ToString() : TEXT("E");
		const FString Name = IronText::Name(TEXT("Ability"), ANSI_TO_TCHAR(Def.AbilityName));
		const float Beat = 0.75f + 0.25f * FMath::Sin(static_cast<float>(FMath::Fmod(FPlatformTime::Seconds(), 100.0)) * 14.f);
		if (State.IsActive())
		{
			Bar(Def.AbilitySeconds > 0.f ? State.ActiveLeft / Def.AbilitySeconds : 1.f, FLinearColor(1.f, 1.f, 0.6f) * Beat, Name, Row);
		}
		else if (State.IsReady())
		{
			Bar(1.f, FLinearColor(1.f, 0.8f, 0.25f), FString::Printf(TEXT("[%s] %s"), *Key, *Name), Row);
		}
		else
		{
			Bar(State.ReadyFraction(Def, Car->GetAbilityCooldownScale()), FLinearColor(0.55f, 0.5f, 0.4f), FString::Printf(TEXT("%s  %.0f"), *Name, State.CooldownLeft), Row);
		}
		Row += 1.f;
	}
	// Barrel and nozzle heat: only worth screen space once they are warm, red and named when locked.
	auto HeatBar = [&](float Heat, bool bLocked, const FString& WarmLabel)
	{
		if (Heat <= 0.05f && !bLocked)
		{
			return;
		}
		Bar(Heat, bLocked ? FLinearColor(1.f, 0.25f, 0.15f) : FLinearColor(1.f, 0.65f, 0.2f),
			bLocked ? IronText::Str(TEXT("HudOverheat"), TEXT("OVERHEATED")) : WarmLabel, Row);
		Row += 1.f;
	};
	if (const UMachineGunComponent* Gun = Car->GetPrimaryWeaponComponent())
	{
		HeatBar(Gun->GetHeat(), Gun->IsOverheated(), IronText::Str(TEXT("HudHeat"), TEXT("HEAT")));
	}
	if (const UFlamethrowerComponent* Flame = Car->GetFlamerComponent(); Flame && Flame->bUnlocked)
	{
		HeatBar(Flame->GetHeat(), Flame->IsOverheated(), IronText::Str(TEXT("HudFlameHeat"), TEXT("NOZZLE")));
	}
}

void AIronSiegeHUD::DrawLockOn(const AWarVehiclePawn* Car)
{
	const AActor* Target = Car ? Car->GetLockTarget() : nullptr;
	if (!Target || !Canvas)
	{
		return;
	}
	const FVector Screen = Canvas->Project(Target->GetActorLocation() + FVector(0.f, 0.f, 80.f));
	if (Screen.Z <= 0.f)
	{
		return;
	}
	// A slowly turning diamond reads as "tracking" even on a still frame.
	const FVector2D C(Screen.X, Screen.Y);
	const float R = 34.f * HudScale;
	// Wrapped in double first: the raw clock is large enough that a float loses the quarter turns
	// between corners (the diamond collapsed into a triangle).
	const float Spin = static_cast<float>(FMath::Fmod(FPlatformTime::Seconds() * 1.5, 2.0 * UE_DOUBLE_PI));
	const FLinearColor LockColor = EnemyColor;
	for (int32 i = 0; i < 4; ++i)
	{
		const float A0 = Spin + i * HALF_PI, A1 = Spin + (i + 1) * HALF_PI;
		FCanvasLineItem Edge(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R);
		Edge.SetColor(LockColor);
		Edge.LineThickness = 2.5f;
		Canvas->DrawItem(Edge);
	}
	DrawTextLine(C + FVector2D(R + 6.f, -8.f), IronText::Str(TEXT("HudLock"), TEXT("LOCK")), LockColor);
}

void AIronSiegeHUD::DrawMissileWarning(const AWarVehiclePawn* Car)
{
	const AActor* Missile = Car ? Car->GetIncomingMissile() : nullptr;
	if (!Missile || !Canvas)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const bool bFlashOn = FMath::Fmod(Now, 0.3) < 0.18;
	const FLinearColor Alarm = EnemyColor;

	// Banner: "MISSILE INCOMING  [Q] Flares", flashing, over a dark strip so it reads on snow.
	const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	const FString Key = S ? S->GetKeyFor(TEXT("Flares")).GetDisplayName().ToString() : TEXT("Q");
	const FString Banner = FString::Printf(TEXT("%s   [%s] %s (%d)"), *IronText::Str(TEXT("HudMissileWarn"), TEXT("MISSILE INCOMING")), *Key,
		*IronText::Str(TEXT("HudFlares"), TEXT("Flares")), Car->GetFlareCharges());
	const FVector2D Size = MeasureTextLine(Banner, 2);
	const FVector2D At((Canvas->ClipX - Size.X) * 0.5f, Canvas->ClipY * 0.16f);
	FCanvasTileItem Strip(FVector2D(At.X - 16.f, At.Y - 6.f), FVector2D(Size.X + 32.f, Size.Y * 2.f + 22.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Strip.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Strip);
	DrawTextLine(At, Banner, bFlashOn ? Alarm : FLinearColor(1.f, 1.f, 1.f), 2);
	const float Metres = Car->GetIncomingMissileDistance() / 100.f;
	const FString Range = FString::Printf(TEXT("%.0f %s"), Metres, *IronText::Str(TEXT("HudMetres"), TEXT("m")));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Range).X) * 0.5f, At.Y + Size.Y + 10.f), Range, Alarm);

	// Marker on the missile itself, or an arrow on a ring around the centre pointing at it.
	const FVector Screen = Canvas->Project(Missile->GetActorLocation());
	const bool bOnScreen = Screen.Z > 0.f && Screen.X > 0.f && Screen.X < Canvas->ClipX && Screen.Y > 0.f && Screen.Y < Canvas->ClipY;
	auto Line = [&](const FVector2D& A, const FVector2D& B, float Thickness)
	{
		FCanvasLineItem Item(A, B);
		Item.SetColor(Alarm);
		Item.LineThickness = Thickness;
		Canvas->DrawItem(Item);
	};
	if (bOnScreen)
	{
		const FVector2D P(Screen.X, Screen.Y);
		const float R = 22.f * HudScale;
		for (int32 i = 0; i < 3; ++i)
		{
			const float A0 = -HALF_PI + i * 2.f * PI / 3.f, A1 = -HALF_PI + (i + 1) * 2.f * PI / 3.f;
			Line(P + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, P + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, 2.5f);
		}
		return;
	}
	DrawEdgeArrow(Missile->GetActorLocation(), Alarm);
}

void AIronSiegeHUD::DrawEdgeArrow(const FVector& WorldLocation, const FLinearColor& Color)
{
	// Off screen (usually behind): the direction in camera space, flattened onto the screen, as an
	// arrowhead on a ring around the centre.
	FVector CamLoc;
	FRotator CamRot;
	if (!PlayerOwner || !Canvas)
	{
		return;
	}
	PlayerOwner->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Local = CamRot.UnrotateVector(WorldLocation - CamLoc);
	FVector2D Dir(Local.Y, -Local.Z);
	if (Local.X < 0.f && FMath::Abs(Dir.Y) < 0.2f * Dir.Size())
	{
		Dir.Y = FMath::Abs(Dir.Size()) * 0.6f; // Straight behind: point down, "it is on your tail".
	}
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.f, 1.f);
	}
	const FVector2D Centre(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Ring = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.36f;
	const FVector2D Tip = Centre + Dir * (Ring + 26.f * HudScale);
	const FVector2D Base = Centre + Dir * Ring;
	const FVector2D Side(-Dir.Y, Dir.X);
	const float W = 16.f * HudScale;
	auto Line = [&](const FVector2D& A, const FVector2D& B)
	{
		FCanvasLineItem Item(A, B);
		Item.SetColor(Color);
		Item.LineThickness = 3.f;
		Canvas->DrawItem(Item);
	};
	Line(Tip, Base + Side * W);
	Line(Tip, Base - Side * W);
	Line(Base + Side * W, Base - Side * W);
}

void AIronSiegeHUD::DrawRailWarning(const AWarVehiclePawn* Car)
{
	if (!Car || !Canvas || !GetWorld())
	{
		return;
	}
	const AWarVehiclePawn* Shooter = nullptr;
	for (TActorIterator<AWarVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (*It != Car && !It->IsPlayerControlled() && It->IsRailgunCharging() && It->IsRailAimFixed()
			&& FVector::Dist(It->GetActorLocation(), Car->GetActorLocation()) < 15000.f)
		{
			Shooter = *It;
			break;
		}
	}
	if (!Shooter)
	{
		return;
	}
	const FLinearColor Cyan(0.35f, 0.9f, 1.f);
	const bool bFlashOn = FMath::Fmod(FPlatformTime::Seconds(), 0.2) < 0.12;
	const FString Banner = IronText::Str(TEXT("HudRailWarn"), TEXT("ENEMY RAILGUN CHARGING!"));
	const FVector2D Size = MeasureTextLine(Banner, 2);
	const FVector2D At((Canvas->ClipX - Size.X) * 0.5f, Canvas->ClipY * 0.27f);
	FCanvasTileItem Strip(FVector2D(At.X - 16.f, At.Y - 6.f), FVector2D(Size.X + 32.f, Size.Y + 12.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Strip.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Strip);
	DrawTextLine(At, Banner, bFlashOn ? Cyan : FLinearColor::White, 2);
	const FVector Screen = Canvas->Project(Shooter->GetActorLocation() + FVector(0.f, 0.f, 100.f));
	const bool bOnScreen = Screen.Z > 0.f && Screen.X > 0.f && Screen.X < Canvas->ClipX && Screen.Y > 0.f && Screen.Y < Canvas->ClipY;
	if (!bOnScreen)
	{
		DrawEdgeArrow(Shooter->GetActorLocation(), Cyan);
	}
	else
	{
		const float R = 40.f * HudScale;
		const FVector2D C(Screen.X, Screen.Y);
		const FVector2D Corners[] = { { -R, -R }, { R, -R }, { R, R }, { -R, R } };
		for (int32 i = 0; i < 4; ++i)
		{
			FCanvasLineItem Edge(C + Corners[i], C + Corners[(i + 1) % 4]);
			Edge.SetColor(Cyan);
			Edge.LineThickness = 3.f;
			Canvas->DrawItem(Edge);
		}
	}
}

void AIronSiegeHUD::DrawFps()
{
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (Delta > 0.f)
	{
		SmoothedFps = SmoothedFps <= 0.f ? 1.f / Delta : FMath::Lerp(SmoothedFps, 1.f / Delta, 0.08f);
	}
	const FString Text = FString::Printf(TEXT("%.0f FPS"), SmoothedFps);
	DrawTextLine(FVector2D(Canvas->ClipX - 230.f * HudScale, 20.f), Text, FLinearColor(0.6f, 1.f, 0.6f));
}

void AIronSiegeHUD::DrawTextLine(const FVector2D& Position, const FString& Text, const FLinearColor& InColor, int32 SizeKind)
{
	if (!Canvas || bMeasureOnly)
	{
		return;
	}
	FLinearColor Color = InColor;
	Color.A *= TextOpacity;
	// Arabic has to be shaped (letters join up and the line runs right-to-left). The Canvas only
	// does that through a shaped glyph sequence from the Slate font cache; Latin text keeps the
	// engine's bitmap fonts, which are sharper at HUD sizes.
	if ((IronText::IsArabic() || bMenuFont) && FSlateApplication::IsInitialized())
	{
		const FSlateFontInfo FontInfo = IronText::Font(FMath::RoundToInt(IronText::CanvasFontSize(SizeKind) * HudScale), SizeKind >= 1);
		const TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
		const FShapedGlyphSequenceRef Shaped = FontCache->ShapeBidirectionalText(
			Text, FontInfo, 1.f, IronText::IsArabic() ? TextBiDi::ETextDirection::RightToLeft : TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
		FCanvasShapedTextItem Item(Position, Shaped, Color);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A));
		Canvas->DrawItem(Item);
		return;
	}
	FCanvasTextItem Item(Position, FText::FromString(Text), IronText::CanvasFont(SizeKind), Color);
	Item.Scale = FVector2D(HudScale, HudScale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A));
	Canvas->DrawItem(Item);
}

FVector2D AIronSiegeHUD::MeasureTextLine(const FString& Text, int32 SizeKind) const
{
	if (!Canvas)
	{
		return FVector2D::ZeroVector;
	}
	if ((IronText::IsArabic() || bMenuFont) && FSlateApplication::IsInitialized())
	{
		const FSlateFontInfo FontInfo = IronText::Font(FMath::RoundToInt(IronText::CanvasFontSize(SizeKind) * HudScale), SizeKind >= 1);
		const TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
		const FShapedGlyphSequenceRef Shaped = FontCache->ShapeBidirectionalText(
			Text, FontInfo, 1.f, IronText::IsArabic() ? TextBiDi::ETextDirection::RightToLeft : TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
		return FVector2D(Shaped->GetMeasuredWidth(), Shaped->GetMaxTextHeight());
	}
	float W = 0.f, H = 0.f;
	Canvas->StrLen(IronText::CanvasFont(SizeKind), Text, W, H);
	return FVector2D(W * HudScale, H * HudScale);
}
