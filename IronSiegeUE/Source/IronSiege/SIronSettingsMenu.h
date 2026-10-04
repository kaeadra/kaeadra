#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include <atomic>

class AIronSiegePlayerController;
class SVerticalBox;
class UIronSiegeUserSettings;
#include "AudioCaptureCore.h"

// Full-screen settings menu (pure Slate, no widget assets): a tab sidebar (Video, Audio, Controls,
// Camera, Voice, Shortcuts, Gameplay) and a scrolling list of rows - selectors, sliders, on/off
// pills, key-rebind buttons and a live microphone level meter. Works with mouse and keyboard.
//
// Settings are edited live on UIronSiegeUserSettings so audio/camera/HUD changes preview
// immediately; video mode/resolution/quality changes take effect on Apply. Discard reloads the
// saved file (undoing previews); Save & Close applies, saves and closes. Esc = Discard.
class SIronSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SIronSettingsMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AIronSiegePlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SIronSettingsMenu() override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	// Jumps to a tab by index (0 = Video ... 6 = Gameplay); used by the settings tour cheat.
	static int32 NumTabs() { return static_cast<int32>(ETab::Count); }
	void ShowTab(int32 Index) { SelectTab(static_cast<ETab>(FMath::Clamp(Index, 0, static_cast<int32>(ETab::Count) - 1))); }

	// Scrolls the current tab's rows to the bottom (the settings tour screenshots both halves).
	void ScrollToEnd();

	// Shows what a finished benchmark measured and which preset it picked.
	void ShowBenchmarkResult(float AverageFps, float TargetFps, int32 Preset);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// Must stay in the same order as UIronSiegeUserSettings::ECategory ("Reset this tab" casts across).
	enum class ETab : uint8 { Video, Audio, Controls, Camera, Voice, Shortcuts, Gameplay, Accessibility, Profiles, Count };

	void SelectTab(ETab Tab);
	void RebuildContent();
	void OnSettingChanged();   // Live preview of non-video settings.
	void ApplyAll();           // Apply video + runtime settings and save.
	void Discard();            // Reload the saved file and close.
	void Close();

	void BuildVideo();
	void BuildAudio();
	void BuildControls();
	void BuildCamera();
	void BuildVoice();
	void BuildShortcuts();
	void BuildGameplay();
	void BuildAccessibility();
	void BuildProfiles();

	// ---- row builders
	void AddSection(const FText& Title);
	void AddNote(const FText& Text);
	void AddSelector(const FText& Label, TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set);
	void AddSlider(const FText& Label, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText(float)> Format);
	void AddToggle(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set, const FText& OnText, const FText& OffText);
	// One control, its keyboard key and its gamepad button - either can be clicked to rebind.
	void AddKeyRow(const FText& Label, FName Action);
	void AddFixedKeyRow(const FText& Label, const FText& Keys);
	void AddButtonRow(const FText& Label, const FText& ButtonText, TFunction<void()> OnClick);
	void AddMeter(const FText& Label);
	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Control);
	TSharedRef<SWidget> MakeButton(TAttribute<FText> Text, TFunction<void()> OnClick, bool bAccent = false, float MinWidth = 0.f);

	// ---- key rebinding
	void BeginListening(FName Action, bool bPad);
	void FinishListening(const FKey& Key);
	FReply OnCaptureMouse(const FGeometry& Geometry, const FPointerEvent& Event);

	// ---- microphone test
	void StartMicTest();
	void StopMicTest();
	int32 FindMicDeviceIndex() const;

	UIronSiegeUserSettings* Settings() const;

	TWeakObjectPtr<AIronSiegePlayerController> Owner;
	ETab CurrentTab = ETab::Video;
	TSharedPtr<SVerticalBox> Content;
	TSharedPtr<class SScrollBox> ContentScroll;
	FName ListeningAction = NAME_None;
	bool bListeningPad = false;
	TSharedPtr<class SEditableTextBox> ProfileNameBox;
	FText StatusText;
	double StatusUntil = 0.0;

	TUniquePtr<Audio::FAudioCapture> MicCapture;
	std::atomic<float> MicPeak{ 0.f };
	float MicDisplay = 0.f;
	TArray<FString> MicDevices;

	// Styles/brushes (must outlive the widgets that point at them).
	FSlateRoundedBoxBrush PanelBrush{ FLinearColor(0.025f, 0.03f, 0.04f, 0.97f), 14.f };
	FSlateRoundedBoxBrush SidebarBrush{ FLinearColor(0.012f, 0.015f, 0.022f, 1.f), 14.f };
	FSlateRoundedBoxBrush RowBrush{ FLinearColor(0.045f, 0.052f, 0.066f, 1.f), 8.f };
	FSlateColorBrush DimBrush{ FLinearColor(0.f, 0.f, 0.f, 0.55f) };
	// White rounded brush tinted per state (active tab, "on" toggles) via BorderBackgroundColor.
	FSlateRoundedBoxBrush HighlightBrush{ FLinearColor::White, 8.f };
	FButtonStyle TabStyle, TabActiveStyle, ButtonStyle, AccentButtonStyle;
};
