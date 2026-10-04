// The campaign and crew side of AIronSiegeHUD: the mode, mission, briefing and driver screens, the
// in-mission objective line and markers, the radio box with the speaker's portrait, and the mission
// result screen. Same Canvas drawing as the rest of the HUD; kept in its own file for size.
#include "IronSiegeHUD.h"
#include "CanvasItem.h"
#include "CrewRules.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "GlobalRenderResources.h"
#include "IronMissionDirector.h"
#include "IronSiegeGameMode.h"
#include "IronSiegePlayerController.h"
#include "IronSiegeStory.h"
#include "IronSiegeText.h"
#include "IronSiegeUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "MissionRules.h"
#include "Sound/SoundBase.h"
#include "TextureResource.h"
#include "UpgradeRules.h"
#include "VehicleWeaponComponent.h"

namespace
{
const FLinearColor Gold(1.f, 0.8f, 0.25f);
const FLinearColor Dim(0.62f, 0.64f, 0.68f);
const FLinearColor Good(0.45f, 1.f, 0.5f);
const FLinearColor Bad(1.f, 0.35f, 0.25f);

FString Clock(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
}

// A campaign text key with its English fallback.
FString Line(const char* Key, const char* English)
{
	return IronText::Str(ANSI_TO_TCHAR(Key), ANSI_TO_TCHAR(English));
}

// What the campaign hands out for a mission, as a readable list ("Mine layer, Armor plating 2").
FString LoadoutList(const IronUpgrades::Loadout& Now, const IronUpgrades::Loadout* Before)
{
	TArray<FString> Items;
	for (int32 i = 0; i < IronUpgrades::Count; ++i)
	{
		const int32 Level = Now.Levels[i];
		if (Level <= 0 || (Before && Before->Levels[i] >= Level))
		{
			continue;
		}
		const FString Name = IronText::Name(TEXT("Up"), ANSI_TO_TCHAR(IronUpgrades::Get(static_cast<IronUpgrades::Upgrade>(i)).Name));
		Items.Add(Level > 1 ? FString::Printf(TEXT("%s %d"), *Name, Level) : Name);
	}
	return FString::Join(Items, IronText::IsArabic() ? TEXT("، ") : TEXT(", "));
}
}

// ---------------------------------------------------------------- Drawing helpers

void AIronSiegeHUD::DrawPanel(const FVector2D& TopLeft, const FVector2D& Size, float Alpha)
{
	FCanvasTileItem Back(TopLeft, Size, FLinearColor(0.01f, 0.012f, 0.018f, Alpha));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	const FVector2D Corners[4] = { TopLeft, TopLeft + FVector2D(Size.X, 0.f), TopLeft + Size, TopLeft + FVector2D(0.f, Size.Y) };
	for (int32 i = 0; i < 4; ++i)
	{
		FCanvasLineItem Edge(Corners[i], Corners[(i + 1) % 4]);
		Edge.SetColor(FLinearColor(1.f, 1.f, 1.f, 0.22f));
		Edge.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Edge);
	}
}

void AIronSiegeHUD::DrawBar(const FVector2D& TopLeft, const FVector2D& Size, float Fraction, const FLinearColor& Fill)
{
	FCanvasTileItem Back(TopLeft - FVector2D(1.f, 1.f), Size + FVector2D(2.f, 2.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	FCanvasTileItem Fore(TopLeft, FVector2D(Size.X * FMath::Clamp(Fraction, 0.f, 1.f), Size.Y), Fill);
	Canvas->DrawItem(Fore);
}

void AIronSiegeHUD::DrawStar(const FVector2D& Centre, float Radius, bool bEarned)
{
	// Five points and the pentagon between them, as filled triangles.
	const FLinearColor Color = bEarned ? Gold : FLinearColor(0.2f, 0.2f, 0.22f);
	FVector2D Outer[5], Inner[5];
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = -HALF_PI + i * 2.f * PI / 5.f;
		Outer[i] = Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
		const float B = A + PI / 5.f;
		Inner[i] = Centre + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius * 0.42f;
	}
	for (int32 i = 0; i < 5; ++i)
	{
		FCanvasTriangleItem Point(Inner[(i + 4) % 5], Outer[i], Inner[i], GWhiteTexture);
		Point.SetColor(Color);
		Canvas->DrawItem(Point);
		FCanvasTriangleItem Core(Centre, Inner[(i + 4) % 5], Inner[i], GWhiteTexture);
		Core.SetColor(Color);
		Canvas->DrawItem(Core);
	}
}

void AIronSiegeHUD::DrawPortrait(int32 Speaker, int32 Mood, const FVector2D& TopLeft, float Size, float Alpha)
{
	const FLinearColor Frame = IronStory::SpeakerColor(Speaker);
	FCanvasTileItem Back(TopLeft, FVector2D(Size, Size), FLinearColor(0.04f, 0.05f, 0.07f, 0.92f * Alpha));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	const UTexture2D* Picture = IronStory::Portrait(Speaker, Mood);
	if (Picture && Picture->GetResource())
	{
		// Already square (IronStory::Portrait crops it when it reads the file).
		FCanvasTileItem Face(TopLeft, Picture->GetResource(), FVector2D(Size, Size), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), FLinearColor(1.f, 1.f, 1.f, Alpha));
		Face.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Face);
	}
	else
	{
		// No picture yet: the character's colour and name hold the place.
		FCanvasTileItem Tint(TopLeft + FVector2D(4.f, 4.f), FVector2D(Size - 8.f, Size - 8.f), FLinearColor(Frame.R * 0.25f, Frame.G * 0.25f, Frame.B * 0.25f, Alpha));
		Tint.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tint);
		const FString Name = IronStory::SpeakerName(Speaker);
		const FVector2D NameSize = MeasureTextLine(Name, 1);
		DrawTextLine(TopLeft + FVector2D((Size - NameSize.X) * 0.5f, (Size - NameSize.Y) * 0.5f), Name, FLinearColor(Frame.R, Frame.G, Frame.B, Alpha), 1);
	}
	const FVector2D Corners[4] = { TopLeft, TopLeft + FVector2D(Size, 0.f), TopLeft + FVector2D(Size, Size), TopLeft + FVector2D(0.f, Size) };
	for (int32 i = 0; i < 4; ++i)
	{
		FCanvasLineItem Edge(Corners[i], Corners[(i + 1) % 4]);
		Edge.SetColor(FLinearColor(Frame.R, Frame.G, Frame.B, Alpha));
		Edge.LineThickness = 3.f;
		Edge.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Edge);
	}
}

void AIronSiegeHUD::DrawCentred(float Y, const FString& Text, const FLinearColor& Color, int32 SizeKind)
{
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Text, SizeKind).X) * 0.5f, Y), Text, Color, SizeKind);
}

void AIronSiegeHUD::DrawAligned(float X, float Y, float Width, const FString& Text, const FLinearColor& Color, int32 SizeKind)
{
	const float At = IronText::IsArabic() ? X + Width - MeasureTextLine(Text, SizeKind).X : X;
	DrawTextLine(FVector2D(At, Y), Text, Color, SizeKind);
}

const TArray<FString>& AIronSiegeHUD::WrapLines(const FString& Text, float Width, int32 SizeKind)
{
	// Measuring shapes the text (Arabic), so the split is remembered rather than redone each frame.
	const FString CacheKey = FString::Printf(TEXT("%d|%d|%d|%d|%s"), FMath::RoundToInt(Width), SizeKind, IronText::IsArabic() ? 1 : 0, FMath::RoundToInt(HudScale * 100.f), *Text);
	if (const TArray<FString>* Found = WrapCache.Find(CacheKey))
	{
		return *Found;
	}
	if (WrapCache.Num() > 96)
	{
		WrapCache.Reset();
	}
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Current;
	for (const FString& Word : Words)
	{
		const FString Longer = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
		if (!Current.IsEmpty() && MeasureTextLine(Longer, SizeKind).X > Width)
		{
			Lines.Add(Current);
			Current = Word;
		}
		else
		{
			Current = Longer;
		}
	}
	if (!Current.IsEmpty())
	{
		Lines.Add(Current);
	}
	return WrapCache.Add(CacheKey, MoveTemp(Lines));
}

float AIronSiegeHUD::DrawWrapped(const FString& Text, float X, float Y, float Width, const FLinearColor& Color, int32 SizeKind)
{
	const float LineHeight = MeasureTextLine(TEXT("Ag"), SizeKind).Y + 7.f * HudScale;
	for (const FString& Each : WrapLines(Text, Width, SizeKind))
	{
		DrawAligned(X, Y, Width, Each, Color, SizeKind);
		Y += LineHeight;
	}
	return Y;
}

// ---------------------------------------------------------------- Radio

void AIronSiegeHUD::Say(int32 Speaker, int32 Mood, const char* Key, const char* Text, bool bStory)
{
	IronDialogue::Line NewLine;
	NewLine.Speaker = Speaker;
	NewLine.Mood = Mood;
	NewLine.Key = Key;
	NewLine.Text = Text;
	NewLine.Priority = bStory ? IronDialogue::PriorityStory : IronDialogue::PriorityBark;
	Radio.Push(NewLine);
}

void AIronSiegeHUD::Bark(int32 Kind, bool bStory)
{
	const AIronSiegePlayerController* PC = Cast<AIronSiegePlayerController>(GetOwningPlayerController());
	if (!PC || !GetWorld() || Kind < 0 || Kind >= IronCrew::BarkCount)
	{
		return;
	}
	const IronCrew::Bark Which = static_cast<IronCrew::Bark>(Kind);
	if (!BarkClock.Allow(Which, GetWorld()->GetTimeSeconds()))
	{
		return;
	}
	const int32 Driver = PC->GetPendingDriverIndex();
	const IronCrew::BarkLine& Said = IronCrew::BarkFor(static_cast<IronCrew::Driver>(Driver), Which);
	Say(Driver, Said.Mood, Said.Key, Said.Text, bStory);
}

void AIronSiegeHUD::TickRadio()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Once per frame, on game time: the radio holds its line while the settings menu pauses the game.
	const double Now = World->GetTimeSeconds();
	Radio.Tick(static_cast<float>(FMath::Clamp(Now - LastRadioTick, 0.0, 0.25)));
	LastRadioTick = Now;
	const IronDialogue::Line* OnAir = Radio.Current();
	if (OnAir && OnAir->Key != RadioLineOnAir)
	{
		// The squelch of a radio opening, for each new line.
		static const TSoftObjectPtr<USoundBase> Chirp(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_Radio.S_Radio")));
		static const TSoftObjectPtr<USoundBase> Fallback(FSoftObjectPath(TEXT("/Game/IronSiege/Audio/S_LockOn.S_LockOn")));
		if (USoundBase* Sound = UVehicleWeaponComponent::LoadIfPresent(Chirp))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.7f);
		}
		else if (USoundBase* Beep = UVehicleWeaponComponent::LoadIfPresent(Fallback))
		{
			UGameplayStatics::PlaySound2D(this, Beep, 0.25f, 1.6f);
		}
	}
	RadioLineOnAir = OnAir ? OnAir->Key : nullptr;
}

void AIronSiegeHUD::DrawRadio()
{
	const IronDialogue::Line* OnAir = Radio.Current();
	if (!OnAir || !Canvas)
	{
		return;
	}
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const float Alpha = Radio.Alpha();
	const float Face = 132.f * HudScale, Pad = 14.f * HudScale;
	const float Width = FMath::Min(760.f * HudScale, Canvas->ClipX - 80.f);
	const FVector2D TopLeft(40.f, Canvas->ClipY - Face - 2.f * Pad - 40.f);
	FCanvasTileItem Back(TopLeft, FVector2D(Width, Face + 2.f * Pad), FLinearColor(0.f, 0.f, 0.f, 0.68f * Alpha));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	// The face sits on the side the text starts from: left for English, right for Arabic.
	const bool bRtl = IronText::IsArabic();
	const float FaceX = bRtl ? TopLeft.X + Width - Pad - Face : TopLeft.X + Pad;
	DrawPortrait(OnAir->Speaker, OnAir->Mood, FVector2D(FaceX, TopLeft.Y + Pad), Face, Alpha);
	const float TextX = bRtl ? TopLeft.X + Pad : TopLeft.X + 2.f * Pad + Face;
	const float TextW = Width - Face - 3.f * Pad;
	FLinearColor NameColor = IronStory::SpeakerColor(OnAir->Speaker);
	NameColor.A = Alpha;
	float Y = TopLeft.Y + Pad;
	DrawAligned(TextX, Y, TextW, IronStory::SpeakerName(OnAir->Speaker), NameColor, 1);
	Y += MeasureTextLine(TEXT("Ag"), 1).Y + 8.f * HudScale;
	DrawWrapped(Line(OnAir->Key, OnAir->Text), TextX, Y, TextW, FLinearColor(1.f, 1.f, 1.f, Alpha), 0);
}

// ---------------------------------------------------------------- Menus

void AIronSiegeHUD::DrawModeSelect(AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const FVector2D Size(940.f, 500.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);
	const FString Title = TEXT("I R O N   S I E G E");
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Title, 2).X) * 0.5f, TopLeft.Y + 34.f), Title, FLinearColor::Yellow, 2);

	const IronMissions::Progress Progress = IronStory::LoadProgress();
	const AIronSiegeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AIronSiegeGameMode>() : nullptr;
	const FString Notes[2] = {
		FString::Format(*IronText::Str(TEXT("MenuProgress"), TEXT("Missions {0}/{1}     Stars {2}/{3}")), { Progress.Completed(), IronMissions::Count, Progress.TotalStars(), IronMissions::Count * 3 }),
		FString::Format(*IronText::Str(TEXT("MenuBest"), TEXT("Best: {0} pts, wave {1}")), { GameMode ? GameMode->GetBestScore() : 0, GameMode ? GameMode->GetBestWave() : 0 }),
	};
	const FString Names[2] = { IronText::Str(TEXT("MenuCampaign"), TEXT("CAMPAIGN")), IronText::Str(TEXT("MenuSurvival"), TEXT("SURVIVAL")) };
	const FString Subs[2] = {
		IronText::Str(TEXT("MenuCampaignSub"), TEXT("Break the siege of Marsa: eight story missions")),
		IronText::Str(TEXT("MenuSurvivalSub"), TEXT("Endless waves, the upgrade shop, your best score")),
	};
	const FVector2D Card(410.f, 250.f);
	for (int32 i = 0; i < 2; ++i)
	{
		const bool bSelected = PC->GetModeIndex() == i;
		// The first choice sits where reading starts: on the right in Arabic.
		const int32 Slot = IronText::IsArabic() ? 1 - i : i;
		const FVector2D At(TopLeft.X + 40.f + Slot * (Card.X + 40.f), TopLeft.Y + 120.f);
		DrawPanel(At, Card, bSelected ? 0.9f : 0.5f);
		if (bSelected)
		{
			const FVector2D Corners[4] = { At, At + FVector2D(Card.X, 0.f), At + Card, At + FVector2D(0.f, Card.Y) };
			for (int32 e = 0; e < 4; ++e)
			{
				FCanvasLineItem Edge(Corners[e], Corners[(e + 1) % 4]);
				Edge.SetColor(Gold);
				Edge.LineThickness = 3.f;
				Canvas->DrawItem(Edge);
			}
		}
		const float X = At.X + 24.f, W = Card.X - 48.f;
		DrawAligned(X, At.Y + 26.f, W, Names[i], bSelected ? Gold : Dim, 2);
		const float Y = DrawWrapped(Subs[i], X, At.Y + 84.f, W, bSelected ? FLinearColor::White : Dim, 0);
		DrawAligned(X, FMath::Max(Y + 16.f, At.Y + Card.Y - 56.f), W, Notes[i], bSelected ? Good : Dim, 0);
	}
	const FString Keys = IronText::Str(TEXT("MenuKeysTop"), TEXT("F3/F4: Choose     Enter: Confirm     Esc / F1: Settings"));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Keys).X) * 0.5f, TopLeft.Y + Size.Y - 56.f), Keys, Dim);
}

void AIronSiegeHUD::DrawMissionSelect(AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const FVector2D Size(1120.f, 640.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);
	const IronMissions::Progress Progress = IronStory::LoadProgress();
	const int32 Cursor = PC->GetMissionCursor();
	DrawAligned(TopLeft.X + 40.f, TopLeft.Y + 30.f, Size.X - 80.f, IronText::Str(TEXT("MenuMissions"), TEXT("CHOOSE A MISSION")), FLinearColor::Yellow, 2);

	// The list, one row a mission: number and name, the battlefield, the best stars.
	const float ListX = TopLeft.X + 40.f, ListW = 520.f, RowH = 54.f;
	float Y = TopLeft.Y + 100.f;
	for (int32 i = 0; i < IronMissions::Count; ++i, Y += RowH)
	{
		const bool bOpen = Progress.IsUnlocked(i), bSelected = i == Cursor;
		if (bSelected)
		{
			FCanvasTileItem Row(FVector2D(ListX - 10.f, Y - 8.f), FVector2D(ListW + 20.f, RowH - 6.f), FLinearColor(1.f, 0.8f, 0.25f, 0.16f));
			Row.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Row);
		}
		const FLinearColor Color = !bOpen ? FLinearColor(0.35f, 0.35f, 0.38f) : (bSelected ? Gold : FLinearColor::White);
		const FString Name = bOpen ? IronStory::MissionName(i) : IronText::Str(TEXT("MenuLocked"), TEXT("LOCKED"));
		DrawAligned(ListX, Y, ListW - 110.f, FString::Printf(TEXT("%d.  %s"), i + 1, *Name), Color, 1);
		if (bOpen)
		{
			for (int32 s = 0; s < 3; ++s)
			{
				const float StarX = IronText::IsArabic() ? ListX + 14.f + s * 30.f : ListX + ListW - 80.f + s * 30.f;
				DrawStar(FVector2D(StarX, Y + 14.f), 12.f, s < Progress.Best[i]);
			}
		}
	}

	// The mission under the cursor.
	const IronMissions::Mission& M = IronMissions::Get(Cursor);
	const float InfoX = TopLeft.X + 610.f, InfoW = Size.X - 650.f;
	float InfoY = TopLeft.Y + 100.f;
	if (Progress.IsUnlocked(Cursor))
	{
		DrawAligned(InfoX, InfoY, InfoW, IronStory::MissionName(Cursor), Gold, 2);
		InfoY += MeasureTextLine(TEXT("Ag"), 2).Y + 16.f;
		DrawAligned(InfoX, InfoY, InfoW, FString::Printf(TEXT("%s: %s"), *IronText::Str(TEXT("HudBattlefield"), TEXT("Battlefield")), *IronText::Name(TEXT("Map"), ANSI_TO_TCHAR(M.Map))), Good);
		InfoY += 34.f;
		DrawAligned(InfoX, InfoY, InfoW, FString::Printf(TEXT("%s: %s"), *IronText::Str(TEXT("MenuPar"), TEXT("Par time")), *Clock(M.ParSeconds)), Dim);
		InfoY += 46.f;
		InfoY = DrawWrapped(Line(M.Brief[0].Key, M.Brief[0].Text), InfoX, InfoY, InfoW, FLinearColor::White, 0) + 14.f;
		for (int32 s = 0; s < M.NumStages; ++s)
		{
			InfoY = DrawWrapped(FString::Printf(TEXT("%d. %s"), s + 1, *Line(M.Stages[s].TextKey, M.Stages[s].Text)), InfoX, InfoY, InfoW, Dim, 0);
		}
	}
	else
	{
		DrawAligned(InfoX, InfoY, InfoW, IronText::Str(TEXT("MenuLocked"), TEXT("LOCKED")), Bad, 2);
		DrawWrapped(IronText::Str(TEXT("MenuLockedHint"), TEXT("Complete the mission before it to unlock this one.")), InfoX, InfoY + 60.f, InfoW, Dim, 0);
	}
	const FString Bonus = FString::Printf(TEXT("%s %d/%d   -   %s"), *IronText::Str(TEXT("MenuStars"), TEXT("Stars")), Progress.TotalStars(), IronMissions::Count * 3,
		*IronText::Str(TEXT("MenuStarBonus"), TEXT("at 8 stars: armor plating   at 16: gun and rocket upgrade")));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Bonus).X) * 0.5f, TopLeft.Y + Size.Y - 86.f), Bonus, Progress.TotalStars() >= 8 ? Good : Dim);
	const FString Keys = IronText::Str(TEXT("MenuKeys"), TEXT("F3/F4: Choose     Enter: Confirm     F2: Back"));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Keys).X) * 0.5f, TopLeft.Y + Size.Y - 50.f), Keys, Dim);
}

void AIronSiegeHUD::DrawBriefing(AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const int32 Index = PC->GetMissionCursor();
	const IronMissions::Mission& M = IronMissions::Get(Index);
	const bool bRtl = IronText::IsArabic();
	const float Width = 1120.f;
	const float Face = 250.f;
	const float W = Width - Face - 120.f;

	// What the commander says, the objectives, the kit and the controls; returns where the text ends.
	auto DrawBody = [&](float X, float Top) -> float
	{
		DrawAligned(X, Top + 30.f, W, FString::Printf(TEXT("%s %d: %s"), *IronText::Str(TEXT("MisMission"), TEXT("MISSION")), Index + 1, *IronStory::MissionName(Index)), FLinearColor::Yellow, 2);
		float Y = Top + 100.f;
		for (const IronMissions::Radio& Paragraph : M.Brief)
		{
			Y = DrawWrapped(Line(Paragraph.Key, Paragraph.Text), X, Y, W, FLinearColor::White, 1) + 12.f;
		}
		Y += 8.f;
		DrawAligned(X, Y, W, IronText::Str(TEXT("MenuObjectives"), TEXT("OBJECTIVES")), Gold, 1);
		Y += MeasureTextLine(TEXT("Ag"), 1).Y + 10.f;
		for (int32 s = 0; s < M.NumStages; ++s)
		{
			Y = DrawWrapped(FString::Printf(TEXT("%d. %s"), s + 1, *Line(M.Stages[s].TextKey, M.Stages[s].Text)), X, Y, W, FLinearColor::White, 0);
		}
		Y += 14.f;
		DrawAligned(X, Y, W, IronText::Str(TEXT("MenuIssued"), TEXT("ISSUED FOR THIS MISSION")), Gold, 1);
		Y += MeasureTextLine(TEXT("Ag"), 1).Y + 10.f;
		const FString Kit = LoadoutList(IronMissions::LoadoutFor(Index, IronStory::LoadProgress().TotalStars()), nullptr);
		const FString Stock = IronText::Str(TEXT("MenuStock"), TEXT("Machine gun and rockets"));
		Y = DrawWrapped(Kit.IsEmpty() ? Stock : Stock + (bRtl ? TEXT("، ") : TEXT(", ")) + Kit, X, Y, W, Good, 0) + 14.f;

		// The controls that matter, from the player's own key bindings.
		if (const UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get())
		{
			auto Key = [Settings](const TCHAR* Action) { return Settings->GetKeyFor(Action).GetDisplayName().ToString(); };
			const TPair<const TCHAR*, const TCHAR*> Rows[] = {
				{ TEXT("FirePrimary"), TEXT("KFire1") }, { TEXT("FireSecondary"), TEXT("KFire2") }, { TEXT("Boost"), TEXT("KBoost") },
				{ TEXT("Ability"), TEXT("KAbility") }, { TEXT("Flares"), TEXT("KFlares") }, { TEXT("Handbrake"), TEXT("KHand") },
			};
			static const TCHAR* English[] = { TEXT("Fire Machine Gun"), TEXT("Fire Rockets"), TEXT("Nitro Boost"), TEXT("Driver Ability"), TEXT("Flares (vs. missiles)"), TEXT("Handbrake") };
			FString Controls = FString::Printf(TEXT("%s/%s/%s/%s: %s"), *Key(TEXT("Accelerate")), *Key(TEXT("Brake")), *Key(TEXT("SteerLeft")), *Key(TEXT("SteerRight")),
				*IronText::Str(TEXT("Driving"), TEXT("Driving")));
			for (int32 i = 0; i < UE_ARRAY_COUNT(Rows); ++i)
			{
				Controls += FString::Printf(TEXT("   |   %s: %s"), *Key(Rows[i].Key), *IronText::Str(Rows[i].Value, English[i]));
			}
			DrawAligned(X, Y, W, IronText::Str(TEXT("TControls"), TEXT("CONTROLS")), Gold, 1);
			Y += MeasureTextLine(TEXT("Ag"), 1).Y + 10.f;
			Y = DrawWrapped(Controls, X, Y, W, Dim, 0);
		}
		return Y;
	};

	// The panel is as tall as its text (longer in some missions and languages than others): the
	// body is laid out once unseen to find out, and never shorter than the portrait needs.
	float Height = 0.f;
	{
		TGuardValue<bool> Measure(bMeasureOnly, true);
		Height = FMath::Max(DrawBody(0.f, 0.f) + 76.f, 100.f + Face + 130.f);
	}
	const FVector2D Size(Width, Height);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);

	// The commander on one side, what she says on the other (mirrored for Arabic).
	const float FaceX = bRtl ? TopLeft.X + Size.X - 40.f - Face : TopLeft.X + 40.f;
	DrawPortrait(M.Brief[0].Speaker, IronCrew::MoodNeutral, FVector2D(FaceX, TopLeft.Y + 100.f), Face);
	const FString Who = IronStory::SpeakerName(M.Brief[0].Speaker);
	DrawTextLine(FVector2D(FaceX + (Face - MeasureTextLine(Who, 1).X) * 0.5f, TopLeft.Y + 100.f + Face + 12.f), Who, IronStory::SpeakerColor(M.Brief[0].Speaker), 1);
	DrawBody(bRtl ? TopLeft.X + 40.f : TopLeft.X + 40.f + Face + 40.f, TopLeft.Y);

	const FString Keys = IronText::Str(TEXT("MenuContinue"), TEXT("Enter: Continue     F2: Back"));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Keys).X) * 0.5f, TopLeft.Y + Size.Y - 50.f), Keys, Dim);
}

void AIronSiegeHUD::DrawDriverSelect(AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const FVector2D Size(1040.f, 600.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f);
	DrawPanel(TopLeft, Size);
	const int32 Index = PC->GetPendingDriverIndex();
	const IronCrew::Driver Who = static_cast<IronCrew::Driver>(Index);
	const IronCrew::DriverDef& Def = IronCrew::Get(Who);
	const bool bOpen = IronMissions::IsDriverUnlocked(IronStory::LoadProgress(), Who);
	const FLinearColor Theirs = IronStory::SpeakerColor(Index);
	const FString Stem = FString(TEXT("Driver")) + ANSI_TO_TCHAR(Def.Key);

	const bool bRtl = IronText::IsArabic();
	const float Face = 340.f;
	const float FaceX = bRtl ? TopLeft.X + Size.X - 40.f - Face : TopLeft.X + 40.f;
	DrawPortrait(Index, IronCrew::MoodNeutral, FVector2D(FaceX, TopLeft.Y + 100.f), Face, bOpen ? 1.f : 0.35f);
	const FString Count = FString::Printf(TEXT("%d / %d"), Index + 1, IronCrew::DriverCount);
	DrawTextLine(FVector2D(FaceX + (Face - MeasureTextLine(Count).X) * 0.5f, TopLeft.Y + 100.f + Face + 12.f), Count, Dim);

	const float X = bRtl ? TopLeft.X + 40.f : TopLeft.X + 40.f + Face + 40.f;
	const float W = Size.X - Face - 120.f;
	DrawAligned(X, TopLeft.Y + 30.f, W, IronText::Str(TEXT("MenuDriver"), TEXT("CHOOSE YOUR DRIVER")), FLinearColor::Yellow, 2);
	float Y = TopLeft.Y + 100.f;
	DrawAligned(X, Y, W, FString::Printf(TEXT("<  %s  >"), *IronText::Str(*(Stem + TEXT("Name")), ANSI_TO_TCHAR(Def.Name))), Theirs, 2);
	Y += MeasureTextLine(TEXT("Ag"), 2).Y + 12.f;
	DrawAligned(X, Y, W, FString::Printf(TEXT("\"%s\""), *IronStory::SpeakerName(Index)), Theirs, 1);
	Y += MeasureTextLine(TEXT("Ag"), 1).Y + 18.f;
	Y = DrawWrapped(IronText::Str(*(Stem + TEXT("Blurb")), ANSI_TO_TCHAR(Def.Blurb)), X, Y, W, FLinearColor::White, 1) + 20.f;

	DrawAligned(X, Y, W, IronText::Str(TEXT("MenuPerk"), TEXT("Perk")), Gold, 1);
	Y += MeasureTextLine(TEXT("Ag"), 1).Y + 8.f;
	Y = DrawWrapped(IronText::Str(*(Stem + TEXT("Perk")), ANSI_TO_TCHAR(Def.PerkText)), X, Y, W, Good, 0) + 16.f;

	const UIronSiegeUserSettings* Settings = UIronSiegeUserSettings::Get();
	const FString Key = Settings ? Settings->GetKeyFor(TEXT("Ability")).GetDisplayName().ToString() : TEXT("E");
	DrawAligned(X, Y, W, FString::Printf(TEXT("%s [%s]:  %s"), *IronText::Str(TEXT("MenuAbility"), TEXT("Ability")), *Key, *IronText::Name(TEXT("Ability"), ANSI_TO_TCHAR(Def.AbilityName))), Gold, 1);
	Y += MeasureTextLine(TEXT("Ag"), 1).Y + 8.f;
	Y = DrawWrapped(IronText::Str(*(Stem + TEXT("Ability")), ANSI_TO_TCHAR(Def.AbilityText)), X, Y, W, Good, 0);
	DrawAligned(X, Y + 4.f, W, FString::Format(*IronText::Str(TEXT("MenuCooldown"), TEXT("cooldown {0} s")), { FMath::RoundToInt(Def.AbilityCooldown) }), Dim, 0);
	if (!bOpen)
	{
		DrawAligned(X, Y + 50.f, W, FString::Format(*IronText::Str(TEXT("MenuDriverLocked"), TEXT("Joins after mission {0}")), { Def.UnlockAfter }), Bad, 1);
	}
	const FString Keys = IronText::Str(TEXT("MenuKeys"), TEXT("F3/F4: Choose     Enter: Confirm     F2: Back"));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Keys).X) * 0.5f, TopLeft.Y + Size.Y - 50.f), Keys, Dim);
}

// ---------------------------------------------------------------- In the mission

void AIronSiegeHUD::DrawMissionStatus(const AIronMissionDirector* Director)
{
	if (Director->GetStageIndex() < 0)
	{
		return;
	}
	TGuardValue<bool> MenuFont(bMenuFont, true);
	// Top-centre, where the wave status sits in survival: the objective (and its clock), with the
	// mission and its time against par underneath.
	FString Objective = Director->GetObjectiveText();
	const float Left = Director->GetTimeLeft();
	if (Left >= 0.f)
	{
		Objective += FString::Printf(TEXT("   |   %s"), *Clock(Left));
	}
	const FString Sub = FString::Printf(TEXT("%s %d: %s   |   %s / %s"), *IronText::Str(TEXT("MisMission"), TEXT("MISSION")), Director->GetMissionIndex() + 1,
		*IronStory::MissionName(Director->GetMissionIndex()), *Clock(Director->GetElapsed()), *Clock(Director->GetMission().ParSeconds));
	const FVector2D Size = MeasureTextLine(Objective, 1), SubSize = MeasureTextLine(Sub);
	const float Width = FMath::Max(Size.X, SubSize.X) + 40.f;
	const float Y = 26.f;
	FCanvasTileItem Tile(FVector2D((Canvas->ClipX - Width) * 0.5f, Y - 8.f), FVector2D(Width, Size.Y + SubSize.Y + 26.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
	const bool bUrgent = Left >= 0.f && Left < 15.f && FMath::Fmod(FPlatformTime::Seconds(), 0.6) < 0.3;
	DrawTextLine(FVector2D((Canvas->ClipX - Size.X) * 0.5f, Y), Objective, bUrgent ? WarningColor : FLinearColor::White, 1);
	DrawTextLine(FVector2D((Canvas->ClipX - SubSize.X) * 0.5f, Y + Size.Y + 8.f), Sub, Director->GetElapsed() <= Director->GetMission().ParSeconds ? Dim : FLinearColor(0.75f, 0.5f, 0.4f));
}

void AIronSiegeHUD::DrawMissionMarkers(const AIronMissionDirector* Director)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const APawn* Me = GetOwningPawn();
	TArray<FIronMissionMarker> Markers;
	Director->GetMarkers(Markers);
	int32 Arrows = 0;
	for (const FIronMissionMarker& M : Markers)
	{
		const FLinearColor Color = M.bAlert ? EnemyColor : (M.bFriendly ? FriendlyColor : WarningColor);
		const FVector Screen = Canvas->Project(M.Location);
		const bool bOnScreen = Screen.Z > 0.f && Screen.X > 0.f && Screen.X < Canvas->ClipX && Screen.Y > 0.f && Screen.Y < Canvas->ClipY;
		if (!bOnScreen)
		{
			// Somewhere behind or beside: an arrow on the ring round the centre (a few at most).
			if (Arrows++ < 3)
			{
				DrawEdgeArrow(M.Location, Color);
			}
			continue;
		}
		const FVector2D At(Screen.X, FMath::Max(Screen.Y, 120.f));
		// A diamond over the spot, its name and distance, and its health or capture bar.
		const float R = 9.f * HudScale;
		const FVector2D Diamond[4] = { At + FVector2D(0.f, -R), At + FVector2D(R, 0.f), At + FVector2D(0.f, R), At + FVector2D(-R, 0.f) };
		for (int32 i = 0; i < 4; ++i)
		{
			FCanvasLineItem Edge(Diamond[i], Diamond[(i + 1) % 4]);
			Edge.SetColor(Color);
			Edge.LineThickness = 2.5f;
			Canvas->DrawItem(Edge);
		}
		const float Metres = Me ? FVector::Dist(Me->GetActorLocation(), M.Location) / 100.f : 0.f;
		const FString Label = FString::Printf(TEXT("%s  %.0f %s"), *M.Label, Metres, *IronText::Str(TEXT("HudMetres"), TEXT("m")));
		const FVector2D LabelSize = MeasureTextLine(Label);
		DrawTextLine(FVector2D(At.X - LabelSize.X * 0.5f, At.Y - R - LabelSize.Y - 6.f), Label, Color);
		if (M.Fraction >= 0.f)
		{
			const FVector2D BarSize(90.f * HudScale, 7.f * HudScale);
			DrawBar(FVector2D(At.X - BarSize.X * 0.5f, At.Y + R + 6.f), BarSize, M.Fraction, Color);
		}
	}
}

void AIronSiegeHUD::DrawMissionResult(const AIronSiegeGameMode* GameMode, const AIronSiegePlayerController* PC)
{
	TGuardValue<bool> MenuFont(bMenuFont, true);
	const AIronMissionDirector* Director = GameMode->GetDirector();
	if (!Director || !PC)
	{
		return;
	}
	const bool bWon = GameMode->WasMissionWon();
	const int32 Index = Director->GetMissionIndex();
	const IronMissions::Mission& M = Director->GetMission();
	const FVector2D Size(900.f, 580.f);
	const FVector2D TopLeft((Canvas->ClipX - Size.X) * 0.5f, (Canvas->ClipY - Size.Y) * 0.5f - 40.f);
	DrawPanel(TopLeft, Size, 0.86f);
	const float X = TopLeft.X + 50.f, W = Size.X - 100.f;
	float Y = TopLeft.Y + 34.f;

	const bool bCampaignDone = bWon && Index + 1 >= IronMissions::Count;
	const FString Title = bCampaignDone ? IronText::Str(TEXT("MisCampaignDone"), TEXT("THE SIEGE IS BROKEN"))
		: (bWon ? IronText::Str(TEXT("MisComplete"), TEXT("MISSION COMPLETE")) : IronText::Str(TEXT("MisFailed"), TEXT("MISSION FAILED")));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Title, 2).X) * 0.5f, Y), Title, bWon ? Good : Bad, 2);
	Y += MeasureTextLine(TEXT("Ag"), 2).Y + 12.f;
	const FString Name = FString::Printf(TEXT("%s %d: %s"), *IronText::Str(TEXT("MisMission"), TEXT("MISSION")), Index + 1, *IronStory::MissionName(Index));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Name, 1).X) * 0.5f, Y), Name, FLinearColor::White, 1);
	Y += MeasureTextLine(TEXT("Ag"), 1).Y + 24.f;

	if (bWon)
	{
		for (int32 s = 0; s < 3; ++s)
		{
			DrawStar(FVector2D(Canvas->ClipX * 0.5f + (s - 1) * 84.f, Y + 34.f), 34.f, s < Director->GetStars());
		}
		Y += 92.f;
		// What each star was for, ticked off in colour.
		const bool bInTime = Director->GetElapsed() <= M.ParSeconds;
		const bool bCareful = Director->GetEndHealthFraction() >= 0.5f && Director->GetAssetsLost() == 0;
		const float RowH = MeasureTextLine(TEXT("Ag")).Y + 12.f;
		DrawAligned(X, Y, W, IronText::Str(TEXT("MisStarDone"), TEXT("Mission complete")), Good);
		Y += RowH;
		DrawAligned(X, Y, W, FString::Printf(TEXT("%s:  %s %s  /  %s"), *IronText::Str(TEXT("MisStarTime"), TEXT("Inside the par time")),
			*IronText::Str(TEXT("MisTime"), TEXT("Time")), *Clock(Director->GetElapsed()), *Clock(M.ParSeconds)), bInTime ? Good : Dim);
		Y += RowH;
		DrawAligned(X, Y, W, FString::Printf(TEXT("%s:  %s %.0f%%,  %s %d"), *IronText::Str(TEXT("MisStarCare"), TEXT("Health 50% or more, nothing lost")),
			*IronText::Str(TEXT("MisHealth"), TEXT("Car health")), Director->GetEndHealthFraction() * 100.f, *IronText::Str(TEXT("MisLost"), TEXT("Losses (trucks / relay)")), Director->GetAssetsLost()), bCareful ? Good : Dim);
		Y += RowH + 8.f;
		if (Director->IsNewBest())
		{
			DrawAligned(X, Y, W, IronText::Str(TEXT("MisNewBest"), TEXT("NEW BEST")), Gold, 1);
			Y += MeasureTextLine(TEXT("Ag"), 1).Y + 10.f;
		}
		if (const int32 Driver = Director->GetUnlockedDriver(); Driver >= 0)
		{
			const IronCrew::DriverDef& Def = IronCrew::Get(static_cast<IronCrew::Driver>(Driver));
			const FString DriverName = IronText::Str(*(FString(TEXT("Driver")) + ANSI_TO_TCHAR(Def.Key) + TEXT("Name")), ANSI_TO_TCHAR(Def.Name));
			DrawAligned(X, Y, W, FString::Printf(TEXT("%s: %s"), *IronText::Str(TEXT("MisNewDriver"), TEXT("NEW DRIVER")), *DriverName), IronStory::SpeakerColor(Driver), 1);
			Y += MeasureTextLine(TEXT("Ag"), 1).Y + 10.f;
		}
		if (Index + 1 < IronMissions::Count)
		{
			const int32 TotalStars = IronStory::LoadProgress().TotalStars();
			const IronUpgrades::Loadout Now = IronMissions::LoadoutFor(Index, TotalStars);
			const FString Issue = LoadoutList(IronMissions::LoadoutFor(Index + 1, TotalStars), &Now);
			if (!Issue.IsEmpty())
			{
				DrawWrapped(FString::Printf(TEXT("%s: %s"), *IronText::Str(TEXT("MisNextIssue"), TEXT("Issued for the next mission")), *Issue), X, Y, W, Gold);
			}
		}
	}
	else
	{
		DrawAligned(X, Y + 20.f, W, Director->GetObjectiveText(), Dim, 1);
	}

	// The choices along the bottom.
	const TArray<int32> Options = PC->GetResultOptions();
	const TCHAR* Keys[3] = { TEXT("MisNext"), TEXT("MisRetry"), TEXT("MisMenu") };
	const TCHAR* English[3] = { TEXT("NEXT MISSION"), TEXT("RETRY"), TEXT("MAIN MENU") };
	const float Slot = W / Options.Num();
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		const bool bSelected = i == FMath::Clamp(PC->GetResultIndex(), 0, Options.Num() - 1);
		const FString Text = FString::Printf(TEXT("%s%s"), bSelected ? TEXT("> ") : TEXT(""), *IronText::Str(Keys[Options[i]], English[Options[i]]));
		const float TextW = MeasureTextLine(Text, 1).X;
		const int32 Place = IronText::IsArabic() ? Options.Num() - 1 - i : i;
		DrawTextLine(FVector2D(X + Slot * Place + (Slot - TextW) * 0.5f, TopLeft.Y + Size.Y - 96.f), Text, bSelected ? Gold : Dim, 1);
	}
	const FString Short = IronText::Str(TEXT("MenuKeysShort"), TEXT("F3/F4: Choose     Enter: Confirm"));
	DrawTextLine(FVector2D((Canvas->ClipX - MeasureTextLine(Short).X) * 0.5f, TopLeft.Y + Size.Y - 46.f), Short, Dim);
}
