#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CrewRules.h"
#include "DialogueRules.h"
#include "IronSiegeHUD.generated.h"

class AIronSiegePlayerController;
class AIronSiegeGameMode;
class AIronMissionDirector;

// Canvas-drawn placeholder HUD (speed/health/armor/ammo as text) so the vehicle is playable
// before any UMG widget assets exist. Replace with a real HUD widget once art is available.
UCLASS()
class IRONSIEGE_API AIronSiegeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// Brief centre-screen message (e.g. a collected supply crate), fades after a couple of seconds.
	void ShowNotice(const FString& Text, const FLinearColor& Color);

	// The player's shot damaged an enemy (bKill: it destroyed it) - crosshair marker + sound.
	void NotifyHitMarker(bool bKill);

	// Floating damage number over Victim (hits on the same car in quick succession add up).
	void NotifyDamage(AActor* Victim, float Amount, bool bKill);

	// Radio (DialogueRules.h): a line in the box at the bottom-left, beside the speaker's portrait.
	// Story lines queue and are always said; anything else is dropped while the radio is busy.
	void Say(int32 Speaker, int32 Mood, const char* Key, const char* Text, bool bStory);

	// One of the player's driver's own lines (IronCrew::Bark), if it is not too soon to say it again.
	// bStory queues it behind whatever is on air instead of dropping it.
	void Bark(int32 Kind, bool bStory = false);

	UPROPERTY(EditAnywhere, Category = "IronSiege|HUD")
	TSoftObjectPtr<class USoundBase> HitSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_HitMarker.S_HitMarker")));

private:
	void DrawMainMenu(AIronSiegePlayerController* PC);
	void DrawVehicleSelect(AIronSiegePlayerController* PC);
	void DrawWaveStatus(const AIronSiegeGameMode* GameMode);
	void DrawGameOver(const AIronSiegeGameMode* GameMode);
	// The driver's experience from the match just ended, at (X, Y) across W; moves Y past it.
	void DrawDriverXp(const AIronSiegeGameMode* GameMode, float X, float& Y, float W);
	void DrawCrosshair(const class IIronVehicle* Vehicle);
	void DrawSupplyMarkers();
	void DrawNotices();
	void DrawEnemyHealthBars();
	void DrawMinimap();
	void DrawFps();

	// Missile lock: a red diamond bracket and LOCK tag around the enemy the pod is locked onto.
	void DrawLockOn(const class AWarVehiclePawn* Car);

	// Incoming guided missile: a flashing banner with the flare key, and a marker on the missile
	// (or an arrow at the screen edge pointing to it when it is off screen).
	void DrawMissileWarning(const class AWarVehiclePawn* Car);

	// An enemy railgun is charging at the player: a flashing cyan banner and a bracket on the shooter.
	void DrawRailWarning(const class AWarVehiclePawn* Car);
	// The boss winding up a slam or an EMP with the player inside its reach: get clear.
	void DrawBossWarning(const AIronSiegeGameMode* GameMode);

	// Arrowhead at the edge of a ring around the screen centre, pointing at something off screen.
	void DrawEdgeArrow(const FVector& WorldLocation, const FLinearColor& Color);

	// Nitro meter and machine-gun barrel heat, stacked under the text readout.
	void DrawVehicleBars(const class AWarVehiclePawn* Car, float X, float Y);

	// Every HUD string goes through these two, so text scale and Arabic shaping live in one place.
	// SizeKind: 0 small, 1 medium, 2 large.
	void DrawTextLine(const FVector2D& Position, const FString& Text, const FLinearColor& Color, int32 SizeKind = 0);
	FVector2D MeasureTextLine(const FString& Text, int32 SizeKind = 0) const;
	float SmoothedFps = 0.f;
	float TextOpacity = 1.f;   // Settings: HUD text opacity.

	struct FDamageNumber
	{
		TWeakObjectPtr<AActor> Victim;
		FVector Anchor = FVector::ZeroVector;
		float Amount = 0.f;
		double Born = 0.0;
		double LastHit = 0.0;
		bool bKill = false;
	};
	TArray<FDamageNumber> DamageNumbers;
	void DrawDamageNumbers();

	// Accessibility, refreshed each frame from UIronSiegeUserSettings.
	float HudScale = 1.f;
	FLinearColor EnemyColor{ 1.f, 0.18f, 0.12f };
	FLinearColor FriendlyColor{ 0.25f, 0.95f, 0.35f };
	FLinearColor WarningColor{ 1.f, 0.72f, 0.15f };
	void DrawShop(const AIronSiegeGameMode* GameMode, const AIronSiegePlayerController* PC);
	void DrawBossBar(const AIronSiegeGameMode* GameMode);

	// ---- Campaign and crew (IronSiegeHUDCampaign.cpp)
	void DrawModeSelect(AIronSiegePlayerController* PC);
	void DrawMissionSelect(AIronSiegePlayerController* PC);
	void DrawBriefing(AIronSiegePlayerController* PC);
	void DrawDriverSelect(AIronSiegePlayerController* PC);
	void DrawMissionStatus(const AIronMissionDirector* Director);
	void DrawMissionMarkers(const AIronMissionDirector* Director);
	void DrawMissionResult(const AIronSiegeGameMode* GameMode, const AIronSiegePlayerController* PC);
	void DrawRadio();
	void TickRadio();

	// A character's picture in a framed square (their colour); a plain panel with their name if
	// there is no picture yet.
	void DrawPortrait(int32 Speaker, int32 Mood, const FVector2D& TopLeft, float Size, float Alpha = 1.f);
	void DrawPanel(const FVector2D& TopLeft, const FVector2D& Size, float Alpha = 0.8f);
	void DrawStar(const FVector2D& Centre, float Radius, bool bEarned);
	void DrawBar(const FVector2D& TopLeft, const FVector2D& Size, float Fraction, const FLinearColor& Fill);

	// One line at X, or ending at X + Width when the UI is Arabic (right to left).
	void DrawAligned(float X, float Y, float Width, const FString& Text, const FLinearColor& Color, int32 SizeKind = 0);
	// One line centred on the screen.
	void DrawCentred(float Y, const FString& Text, const FLinearColor& Color, int32 SizeKind = 0);
	// Text wrapped to Width (right-aligned in Arabic); returns the Y under the last line.
	float DrawWrapped(const FString& Text, float X, float Y, float Width, const FLinearColor& Color, int32 SizeKind = 0);
	const TArray<FString>& WrapLines(const FString& Text, float Width, int32 SizeKind);
	TMap<FString, TArray<FString>> WrapCache;

	// While set, text goes through the UI font whatever the language: the campaign's screens and
	// radio box are read, not glanced at, and the engine's bitmap fonts are too small for that.
	bool bMenuFont = false;
	// While set, DrawTextLine draws nothing: a screen lays its text out once unseen to learn how
	// tall it is before drawing the panel behind it.
	bool bMeasureOnly = false;

	IronDialogue::Queue Radio;
	IronCrew::BarkClock BarkClock;
	const char* RadioLineOnAir = nullptr; // Key of the line whose arrival chirp has been played.
	double LastRadioTick = 0.0;

	double LastHitTime = -10.0;
	double LastHitSoundTime = -10.0;
	bool bLastHitKill = false;

	struct FNotice
	{
		FString Text;
		FLinearColor Color;
		double ExpireTime;
	};
	TArray<FNotice> Notices;
};
