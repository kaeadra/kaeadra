#include "SIronSettingsMenu.h"
#include "IronSiegePlayerController.h"
#include "IronSiegeUserSettings.h"
#include "SettingsRules.h"
#include "IronSiegeText.h"
#include "AudioCaptureCore.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "IronSettings"

namespace
{
const FLinearColor Accent(1.f, 0.55f, 0.12f);
const FLinearColor TextColor(0.93f, 0.94f, 0.96f);
const FLinearColor Muted(0.55f, 0.58f, 0.64f);

FSlateFontInfo Font(int32 Size, const char* Weight = "Regular")
{
	return IronText::Font(Size, FCStringAnsi::Strcmp(Weight, "Bold") == 0);
}

FButtonStyle MakeButtonStyle(const FLinearColor& Normal, const FLinearColor& Hover, const FLinearColor& Pressed, float Radius)
{
	FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	Style.SetNormal(FSlateRoundedBoxBrush(Normal, Radius));
	Style.SetHovered(FSlateRoundedBoxBrush(Hover, Radius));
	Style.SetPressed(FSlateRoundedBoxBrush(Pressed, Radius));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f));
	return Style;
}

const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };

// Compact, language-neutral labels for pad buttons, as printed on the controller.
const TCHAR* ShortPadName(const FKey& Key)
{
	static const TPair<FKey, const TCHAR*> Names[] = {
		{ EKeys::Gamepad_RightTriggerAxis, TEXT("RT") }, { EKeys::Gamepad_LeftTriggerAxis, TEXT("LT") },
		{ EKeys::Gamepad_RightTrigger, TEXT("RT") }, { EKeys::Gamepad_LeftTrigger, TEXT("LT") },
		{ EKeys::Gamepad_RightShoulder, TEXT("RB") }, { EKeys::Gamepad_LeftShoulder, TEXT("LB") },
		{ EKeys::Gamepad_FaceButton_Bottom, TEXT("A") }, { EKeys::Gamepad_FaceButton_Right, TEXT("B") },
		{ EKeys::Gamepad_FaceButton_Left, TEXT("X") }, { EKeys::Gamepad_FaceButton_Top, TEXT("Y") },
		{ EKeys::Gamepad_LeftThumbstick, TEXT("LS") }, { EKeys::Gamepad_RightThumbstick, TEXT("RS") },
		{ EKeys::Gamepad_LeftX, TEXT("LS X") }, { EKeys::Gamepad_LeftY, TEXT("LS Y") },
		{ EKeys::Gamepad_RightX, TEXT("RS X") }, { EKeys::Gamepad_RightY, TEXT("RS Y") },
		{ EKeys::Gamepad_DPad_Up, TEXT("D-Pad Up") }, { EKeys::Gamepad_DPad_Down, TEXT("D-Pad Down") },
		{ EKeys::Gamepad_DPad_Left, TEXT("D-Pad Left") }, { EKeys::Gamepad_DPad_Right, TEXT("D-Pad Right") },
		{ EKeys::Gamepad_Special_Left, TEXT("Back") }, { EKeys::Gamepad_Special_Right, TEXT("Start") },
	};
	for (const TPair<FKey, const TCHAR*>& N : Names)
	{
		if (N.Key == Key)
		{
			return N.Value;
		}
	}
	return nullptr;
}

TArray<FText> QualityOptions()
{
	TArray<FText> Out;
	for (const TCHAR* Name : QualityNames) Out.Add(FText::FromString(IronText::Name(TEXT("Q"), Name)));
	return Out;
}
}

UIronSiegeUserSettings* SIronSettingsMenu::Settings() const
{
	return UIronSiegeUserSettings::Get();
}

void SIronSettingsMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	TabStyle = MakeButtonStyle(FLinearColor(0, 0, 0, 0), FLinearColor(1, 1, 1, 0.06f), FLinearColor(1, 1, 1, 0.1f), 8.f);
	TabActiveStyle = MakeButtonStyle(Accent * FLinearColor(1, 1, 1, 0.22f), Accent * FLinearColor(1, 1, 1, 0.3f), Accent * FLinearColor(1, 1, 1, 0.36f), 8.f);
	ButtonStyle = MakeButtonStyle(FLinearColor(0.09f, 0.1f, 0.125f), FLinearColor(0.14f, 0.15f, 0.185f), FLinearColor(0.18f, 0.19f, 0.23f), 8.f);
	AccentButtonStyle = MakeButtonStyle(Accent * 0.85f, Accent, Accent * 0.7f, 8.f);

	if (!MicCapture)
	{
		MicCapture = MakeUnique<Audio::FAudioCapture>();
	}

	TSharedRef<SVerticalBox> Tabs = SNew(SVerticalBox);
	const TPair<ETab, FText> TabDefs[] = {
		{ ETab::Video, TLOC("TVideo", "Video") }, { ETab::Audio, TLOC("TAudio", "Audio") },
		{ ETab::Controls, TLOC("TControls", "Controls") }, { ETab::Camera, TLOC("TCamera", "Camera") },
		{ ETab::Voice, TLOC("TVoice", "Voice & Mic") }, { ETab::Shortcuts, TLOC("TKeys", "Shortcuts") },
		{ ETab::Gameplay, TLOC("TGameplay", "Gameplay & HUD") },
		{ ETab::Accessibility, TLOC("TAccess", "Accessibility") },
		{ ETab::Profiles, TLOC("TProfiles", "Profiles") },
	};
	for (const TPair<ETab, FText>& Def : TabDefs)
	{
		const ETab Tab = Def.Key;
		Tabs->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(&HighlightBrush)
			.Padding(0.f)
			.BorderBackgroundColor_Lambda([this, Tab]() { return FSlateColor(CurrentTab == Tab ? Accent * FLinearColor(1, 1, 1, 0.2f) : FLinearColor(0, 0, 0, 0)); })
			[
				SNew(SButton)
				.ButtonStyle(&TabStyle)
				.OnClicked_Lambda([this, Tab]() { SelectTab(Tab); return FReply::Handled(); })
				.ContentPadding(FMargin(16.f, 11.f))
				[
					SNew(STextBlock).Text(Def.Value).Font(Font(13, "Bold"))
					.ColorAndOpacity_Lambda([this, Tab]() { return FSlateColor(CurrentTab == Tab ? Accent : TextColor); })
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(&DimBrush)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1180.f).HeightOverride(700.f)
			[
				SNew(SBorder).BorderImage(&PanelBrush).Padding(0.f)
				[
					SNew(SHorizontalBox)
					// Sidebar
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBorder).BorderImage(&SidebarBrush).Padding(FMargin(18.f, 26.f))
						[
							SNew(SBox).WidthOverride(230.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 0.f, 4.f)
								[
									SNew(STextBlock).Text(TLOC("Title", "SETTINGS")).Font(Font(22, "Bold")).ColorAndOpacity(Accent)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 0.f, 24.f)
								[
									SNew(STextBlock).Text(TLOC("Sub", "Iron Siege")).Font(Font(11)).ColorAndOpacity(Muted)
								]
								+ SVerticalBox::Slot().FillHeight(1.f)[Tabs]
								+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 12.f, 0.f, 0.f)
								[
									SNew(STextBlock).Text(TLOC("EscHint", "Esc  Discard & close")).Font(Font(10)).ColorAndOpacity(Muted)
								]
							]
						]
					]
					// Content
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(30.f, 26.f, 30.f, 22.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SAssignNew(ContentScroll, SScrollBox)
							+ SScrollBox::Slot()[SAssignNew(Content, SVerticalBox)]
						]
						// Footer
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								MakeButton(TLOC("ResetTab", "Reset this tab"), [this]()
								{
									UIronSiegeUserSettings::ECategory Cat = static_cast<UIronSiegeUserSettings::ECategory>(CurrentTab);
									if (UIronSiegeUserSettings* S = Settings()) S->ResetCategory(Cat);
									OnSettingChanged();
									RebuildContent();
								})
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f)
							[
								SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Accent)
								.Text_Lambda([this]() { return FPlatformTime::Seconds() < StatusUntil ? StatusText : FText::GetEmpty(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
							[
								MakeButton(TLOC("Discard", "Discard"), [this]() { Discard(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
							[
								MakeButton(TLOC("Apply", "Apply"), [this]() { ApplyAll(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
							[
								MakeButton(TLOC("SaveClose", "Save & Close"), [this]() { ApplyAll(); Close(); }, true)
							]
						]
					]
				]
			]
		]
		// Key capture overlay (visible only while waiting for a new key).
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&DimBrush)
			.Visibility_Lambda([this]() { return ListeningAction.IsNone() ? EVisibility::Collapsed : EVisibility::Visible; })
			.OnMouseButtonDown(this, &SIronSettingsMenu::OnCaptureMouse)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(40.f, 28.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(18, "Bold")).ColorAndOpacity(Accent)
						.Text_Lambda([this]() { return bListeningPad ? TLOC("PressPad", "Press a gamepad button") : TLOC("PressKey", "Press a key or mouse button"); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(TLOC("PressKeyHint", "Esc to cancel")).Font(Font(11)).ColorAndOpacity(Muted)
					]
				]
			]
		]
	];

	SelectTab(ETab::Video);
}

SIronSettingsMenu::~SIronSettingsMenu()
{
	StopMicTest();
}

TSharedRef<SWidget> SIronSettingsMenu::MakeButton(TAttribute<FText> Text, TFunction<void()> OnClick, bool bAccent, float MinWidth)
{
	return SNew(SBox).MinDesiredWidth(MinWidth)
	[
		SNew(SButton)
		.ButtonStyle(bAccent ? &AccentButtonStyle : &ButtonStyle)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(18.f, 9.f))
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Text).Font(Font(12, "Bold")).ColorAndOpacity(bAccent ? FLinearColor(0.04f, 0.03f, 0.02f) : TextColor)
		]
	];
}

void SIronSettingsMenu::ScrollToEnd()
{
	if (ContentScroll.IsValid())
	{
		ContentScroll->ScrollToEnd();
	}
}

void SIronSettingsMenu::SelectTab(ETab Tab)
{
	if (CurrentTab == ETab::Voice && Tab != ETab::Voice)
	{
		StopMicTest();
	}
	CurrentTab = Tab;
	ListeningAction = NAME_None;
	RebuildContent();
}

void SIronSettingsMenu::RebuildContent()
{
	if (!Content.IsValid() || !Settings())
	{
		return;
	}
	Content->ClearChildren();
	switch (CurrentTab)
	{
	case ETab::Video: BuildVideo(); break;
	case ETab::Audio: BuildAudio(); break;
	case ETab::Controls: BuildControls(); break;
	case ETab::Camera: BuildCamera(); break;
	case ETab::Voice: BuildVoice(); break;
	case ETab::Shortcuts: BuildShortcuts(); break;
	case ETab::Gameplay: BuildGameplay(); break;
	case ETab::Accessibility: BuildAccessibility(); break;
	case ETab::Profiles: BuildProfiles(); break;
	default: break;
	}
}

void SIronSettingsMenu::OnSettingChanged()
{
	if (AIronSiegePlayerController* PC = Owner.Get())
	{
		PC->ApplyUserSettings();
	}
}

void SIronSettingsMenu::ApplyAll()
{
	if (UIronSiegeUserSettings* S = Settings())
	{
		S->ApplySettings(false); // Resolution, window mode, scalability, v-sync, frame cap - and saves.
		S->SaveSettings();
	}
	OnSettingChanged();
	StatusText = TLOC("Saved", "Settings applied and saved");
	StatusUntil = FPlatformTime::Seconds() + 2.5;
}

void SIronSettingsMenu::Discard()
{
	if (UIronSiegeUserSettings* S = Settings())
	{
		S->LoadSettings(true); // Back to what's on disk, undoing live previews.
	}
	OnSettingChanged();
	Close();
}

void SIronSettingsMenu::Close()
{
	StopMicTest();
	if (AIronSiegePlayerController* PC = Owner.Get())
	{
		PC->CloseSettingsMenu();
	}
}

// ------------------------------------------------------------------ rows

TSharedRef<SWidget> SIronSettingsMenu::MakeRow(const FText& Label, TSharedRef<SWidget> Control)
{
	return SNew(SBorder).BorderImage(&RowBrush).Padding(FMargin(18.f, 10.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Label).Font(Font(13)).ColorAndOpacity(TextColor)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
		[
			Control
		]
	];
}

void SIronSettingsMenu::AddSection(const FText& Title)
{
	Content->AddSlot().AutoHeight().Padding(2.f, Content->NumSlots() == 0 ? 0.f : 18.f, 0.f, 8.f)
	[
		SNew(STextBlock).Text(Title.ToUpper()).Font(Font(11, "Bold")).ColorAndOpacity(Accent)
	];
}

void SIronSettingsMenu::AddNote(const FText& Text)
{
	Content->AddSlot().AutoHeight().Padding(4.f, 2.f, 4.f, 8.f)
	[
		SNew(STextBlock).Text(Text).Font(Font(11)).ColorAndOpacity(Muted).AutoWrapText(true)
	];
}

void SIronSettingsMenu::AddSelector(const FText& Label, TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	const int32 Num = Options.Num();
	if (Num == 0)
	{
		return;
	}
	TSharedRef<SWidget> Control = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(FText::FromString(TEXT("<")), [Get, Set, Num]() { Set((FMath::Clamp(Get(), 0, Num - 1) + Num - 1) % Num); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(190.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(13, "Bold")).ColorAndOpacity(TextColor)
				.Text_Lambda([Options, Get]() { return Options[FMath::Clamp(Get(), 0, Options.Num() - 1)]; })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(FText::FromString(TEXT(">")), [Get, Set, Num]() { Set((FMath::Clamp(Get(), 0, Num - 1) + 1) % Num); })
		];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

void SIronSettingsMenu::AddSlider(const FText& Label, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText(float)> Format)
{
	TSharedRef<SWidget> Control = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(300.f)
			[
				SNew(SSlider)
				.SliderBarColor(FLinearColor(0.2f, 0.21f, 0.25f))
				.SliderHandleColor(Accent)
				.Value_Lambda([Get, Min, Max]() { return (Get() - Min) / FMath::Max(Max - Min, KINDA_SMALL_NUMBER); })
				.OnValueChanged_Lambda([this, Set, Min, Max](float V) { Set(Min + V * (Max - Min)); OnSettingChanged(); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).WidthOverride(70.f)
			[
				SNew(STextBlock).Font(Font(13, "Bold")).ColorAndOpacity(Accent)
				.Text_Lambda([Get, Format]() { return Format(Get()); })
			]
		];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

void SIronSettingsMenu::AddToggle(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set, const FText& OnText, const FText& OffText)
{
	TSharedRef<SWidget> Control = SNew(SBox).WidthOverride(150.f)
	[
		SNew(SBorder)
		.BorderImage(&HighlightBrush)
		.Padding(0.f)
		.BorderBackgroundColor_Lambda([Get]() { return FSlateColor(Get() ? Accent * 0.9f : FLinearColor(0.09f, 0.1f, 0.125f)); })
		[
			SNew(SButton)
			.ButtonStyle(&TabStyle)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(12.f, 9.f))
			.OnClicked_Lambda([this, Get, Set]() { Set(!Get()); OnSettingChanged(); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(Font(12, "Bold"))
				.Text_Lambda([Get, OnText, OffText]() { return Get() ? OnText : OffText; })
				.ColorAndOpacity_Lambda([Get]() { return FSlateColor(Get() ? FLinearColor(0.04f, 0.03f, 0.02f) : TextColor); })
			]
		]
	];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

void SIronSettingsMenu::AddKeyRow(const FText& Label, FName Action)
{
	auto KeyText = [this, Action](bool bPad)
	{
		return TAttribute<FText>::CreateLambda([this, Action, bPad]()
		{
			const UIronSiegeUserSettings* S = Settings();
			const FKey Key = !S ? EKeys::Invalid : (bPad ? S->GetPadKeyFor(Action) : S->GetKeyFor(Action));
			if (!Key.IsValid())
			{
				return TLOC("Unbound", "- unbound -");
			}
			// Pad buttons by their short standard names (RT, LB, A...): the engine's own names run to
			// a whole sentence in Arabic and pushed the control names off their rows.
			if (const TCHAR* Short = ShortPadName(Key))
			{
				return FText::FromString(Short);
			}
			// Stick and trigger axes are wired to whole controls, not single buttons.
			return Key.IsAxis1D() ? FText::Format(TLOC("AxisKey", "{0} (axis)"), Key.GetDisplayName()) : Key.GetDisplayName();
		});
	};
	TSharedRef<SWidget> Control = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(KeyText(false), [this, Action]() { BeginListening(Action, false); }, false, 190.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
		[
			MakeButton(KeyText(true), [this, Action]() { BeginListening(Action, true); }, false, 190.f)
		];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

void SIronSettingsMenu::AddFixedKeyRow(const FText& Label, const FText& Keys)
{
	TSharedRef<SWidget> Control = SNew(SBox).WidthOverride(200.f).HAlign(HAlign_Center)
	[
		SNew(STextBlock).Text(Keys).Font(Font(12, "Bold")).ColorAndOpacity(Muted)
	];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

void SIronSettingsMenu::AddButtonRow(const FText& Label, const FText& ButtonText, TFunction<void()> OnClick)
{
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, MakeButton(ButtonText, OnClick, false, 200.f))];
}

void SIronSettingsMenu::AddMeter(const FText& Label)
{
	TSharedRef<SWidget> Control = SNew(SBox).WidthOverride(384.f).HeightOverride(14.f)
	[
		SNew(SProgressBar)
		.FillColorAndOpacity_Lambda([this]() { return FSlateColor(MicDisplay > 0.85f ? FLinearColor(1.f, 0.3f, 0.2f) : FLinearColor(0.35f, 0.9f, 0.45f)); })
		.Percent_Lambda([this]() { return TOptional<float>(MicDisplay); })
	];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(Label, Control)];
}

// ------------------------------------------------------------------ tabs

void SIronSettingsMenu::BuildVideo()
{
	UIronSiegeUserSettings* S = Settings();

	AddSection(TLOC("Display", "Display"));
	AddSelector(TLOC("WindowMode", "Window mode"),
		{ TLOC("Fullscreen", "Fullscreen"), TLOC("Borderless", "Borderless window"), TLOC("Windowed", "Windowed") },
		[S]() { return static_cast<int32>(S->GetFullscreenMode()); },
		[S](int32 V) { S->SetFullscreenMode(static_cast<EWindowMode::Type>(V)); });

	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0)
	{
		Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) };
	}
	TArray<FText> ResNames;
	for (const FIntPoint& R : Resolutions) ResNames.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));
	AddSelector(TLOC("Resolution", "Resolution"), ResNames,
		[S, Resolutions]() { const int32 I = Resolutions.IndexOfByKey(S->GetScreenResolution()); return I == INDEX_NONE ? Resolutions.Num() - 1 : I; },
		[S, Resolutions](int32 V) { S->SetScreenResolution(Resolutions[V]); });

	AddSlider(TLOC("ResScale", "Render scale"), 0.f, 1.f,
		[S]() { float Normalized = 1.f, Current = 100.f, Min = 0.f, Max = 100.f; S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max); return Normalized; },
		[S](float V) { S->SetResolutionScaleNormalized(V); },
		[S](float) { float Normalized = 1.f, Current = 100.f, Min = 0.f, Max = 100.f; S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max); return FText::FromString(FString::Printf(TEXT("%.0f%%"), Current)); });

	TArray<FText> Caps;
	for (int32 i = 0; i < IronSettings::FrameCapCount; ++i)
	{
		const float V = IronSettings::FrameCapValue(i);
		Caps.Add(V > 0.f ? FText::FromString(FString::Printf(TEXT("%.0f FPS"), V)) : TLOC("Unlimited", "Unlimited"));
	}
	AddSelector(TLOC("FrameCap", "Frame rate limit"), Caps,
		[S]() { return IronSettings::FrameCapIndex(S->GetFrameRateLimit()); },
		[S](int32 V) { S->SetFrameRateLimit(IronSettings::FrameCapValue(V)); });
	AddToggle(TLOC("VSync", "V-Sync"), [S]() { return S->IsVSyncEnabled(); }, [S](bool V) { S->SetVSyncEnabled(V); }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddSlider(TLOC("Brightness", "Brightness"), 0.5f, 1.5f,
		[S]() { return S->Brightness; }, [S](float V) { S->Brightness = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });

	AddSection(TLOC("Quality", "Graphics quality"));
	AddButtonRow(TLOC("AutoDetect", "Detect best settings for this PC"), TLOC("RunBenchmark", "Auto-detect"), [this, S]()
	{
		S->RunHardwareBenchmark();
		S->ApplyHardwareBenchmarkResults();
		StatusText = FText::Format(TLOC("Benchmarked", "Detected: overall quality {0}"), FText::FromString(IronText::Name(TEXT("Q"), QualityNames[FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 4)])));
		StatusUntil = FPlatformTime::Seconds() + 4.0;
		RebuildContent();
	});
	AddButtonRow(TLOC("Benchmark", "Benchmark this scene (8s of play)"), TLOC("RunBench", "Run benchmark"), [this, S]()
	{
		if (AIronSiegePlayerController* PC = Owner.Get())
		{
			const float Target = S->GetFrameRateLimit() > 0.f ? S->GetFrameRateLimit() : 60.f;
			PC->RunBenchmark(8.f, Target);
		}
	});
	TArray<FText> Presets = QualityOptions();
	Presets.Add(TLOC("Custom", "Custom"));
	AddSelector(TLOC("Overall", "Overall preset"), Presets,
		[S]() { const int32 L = S->GetOverallScalabilityLevel(); return L < 0 ? 5 : L; },
		[this, S](int32 V) { if (V <= 4) { S->SetOverallScalabilityLevel(V); } });

	struct FGroup { FText Label; TFunction<int32()> Get; TFunction<void(int32)> Set; };
	const FGroup Groups[] = {
		{ TLOC("ViewDist", "View distance"), [S]() { return S->GetViewDistanceQuality(); }, [S](int32 V) { S->SetViewDistanceQuality(V); } },
		{ TLOC("AA", "Anti-aliasing"), [S]() { return S->GetAntiAliasingQuality(); }, [S](int32 V) { S->SetAntiAliasingQuality(V); } },
		{ TLOC("Shadows", "Shadows"), [S]() { return S->GetShadowQuality(); }, [S](int32 V) { S->SetShadowQuality(V); } },
		{ TLOC("GI", "Global illumination"), [S]() { return S->GetGlobalIlluminationQuality(); }, [S](int32 V) { S->SetGlobalIlluminationQuality(V); } },
		{ TLOC("Reflections", "Reflections"), [S]() { return S->GetReflectionQuality(); }, [S](int32 V) { S->SetReflectionQuality(V); } },
		{ TLOC("PostFx", "Post-processing"), [S]() { return S->GetPostProcessingQuality(); }, [S](int32 V) { S->SetPostProcessingQuality(V); } },
		{ TLOC("Textures", "Textures"), [S]() { return S->GetTextureQuality(); }, [S](int32 V) { S->SetTextureQuality(V); } },
		{ TLOC("Effects", "Effects"), [S]() { return S->GetVisualEffectQuality(); }, [S](int32 V) { S->SetVisualEffectQuality(V); } },
		{ TLOC("Foliage", "Foliage"), [S]() { return S->GetFoliageQuality(); }, [S](int32 V) { S->SetFoliageQuality(V); } },
		{ TLOC("Shading", "Shading"), [S]() { return S->GetShadingQuality(); }, [S](int32 V) { S->SetShadingQuality(V); } },
	};
	for (const FGroup& G : Groups)
	{
		AddSelector(G.Label, QualityOptions(), G.Get, G.Set);
	}
	AddToggle(TLOC("MotionBlur", "Motion blur"), [S]() { return S->bMotionBlur; }, [S](bool V) { S->bMotionBlur = V; }, TLOC("On", "On"), TLOC("Off", "Off"));

	AddSection(TLOC("Image", "Image"));
	AddSelector(TLOC("AaMethod", "Anti-aliasing method"),
		{ TLOC("AaOff", "Off"), FText::FromString(TEXT("FXAA")), FText::FromString(TEXT("TAA")), TLOC("AaTsr", "TSR (best, heavier)") },
		[S]() { return S->AntiAliasingMethod; }, [S](int32 V) { S->AntiAliasingMethod = V; });
	AddSlider(TLOC("Sharpness", "Sharpening"), 0.f, 1.f, [S]() { return S->Sharpness; }, [S](float V) { S->Sharpness = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddToggle(TLOC("Bloom", "Bloom (glow around lights)"), [S]() { return S->bBloom; }, [S](bool V) { S->bBloom = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddToggle(TLOC("Ao", "Ambient occlusion"), [S]() { return S->bAmbientOcclusion; }, [S](bool V) { S->bAmbientOcclusion = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddToggle(TLOC("Ssr", "Screen-space reflections"), [S]() { return S->bReflections; }, [S](bool V) { S->bReflections = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddToggle(TLOC("VolFog", "Volumetric fog"), [S]() { return S->bVolumetricFog; }, [S](bool V) { S->bVolumetricFog = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddNote(TLOC("ImageNote", "On a 4 GB graphics card, TSR and reflections cost the most; FXAA and reflections off gain the most frames."));
	AddNote(TLOC("VideoNote", "Display mode, resolution and quality take effect when you press Apply. Brightness and motion blur preview immediately."));
}

void SIronSettingsMenu::BuildAudio()
{
	UIronSiegeUserSettings* S = Settings();
	auto Pct = [](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); };
	AddSection(TLOC("Volume", "Volume"));
	AddSlider(TLOC("Master", "Master"), 0.f, 1.f, [S]() { return S->MasterVolume; }, [S](float V) { S->MasterVolume = V; }, Pct);
	AddSlider(TLOC("EffectsVol", "Weapons & explosions"), 0.f, 1.f, [S]() { return S->EffectsVolume; }, [S](float V) { S->EffectsVolume = V; }, Pct);
	AddSlider(TLOC("EngineVol", "Vehicle engines"), 0.f, 1.f, [S]() { return S->EngineVolume; }, [S](float V) { S->EngineVolume = V; }, Pct);
	AddSlider(TLOC("UiVol", "Interface & alerts"), 0.f, 1.f, [S]() { return S->InterfaceVolume; }, [S](float V) { S->InterfaceVolume = V; }, Pct);
	AddSection(TLOC("AudioOptions", "Options"));
	AddToggle(TLOC("MuteUnfocused", "Mute when the game is in the background"), [S]() { return S->bMuteWhenUnfocused; }, [S](bool V) { S->bMuteWhenUnfocused = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddToggle(TLOC("HitSounds", "Hit confirmation sound"), [S]() { return S->bHitSounds; }, [S](bool V) { S->bHitSounds = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddNote(TLOC("AudioNote", "Changes are heard immediately. Voice chat volume is on the Voice & Mic tab."));
}

void SIronSettingsMenu::BuildControls()
{
	UIronSiegeUserSettings* S = Settings();
	AddSection(TLOC("Steering", "Steering"));
	AddSlider(TLOC("SteerSens", "Steering sensitivity"), 0.5f, 1.5f, [S]() { return S->SteeringSensitivity; }, [S](float V) { S->SteeringSensitivity = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddSlider(TLOC("SteerSmooth", "Steering smoothing"), 0.f, 1.f, [S]() { return S->SteeringSmoothing; }, [S](float V) { S->SteeringSmoothing = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddSection(TLOC("Feel", "Driving feel"));
	AddSelector(TLOC("DriveStyle", "Handling model"),
		{ TLOC("Arcade", "Arcade"), TLOC("Balanced", "Balanced"), TLOC("Sim", "Simulation") },
		[S]() { return S->DrivingStyle; }, [S](int32 V) { S->DrivingStyle = V; });
	AddNote(TLOC("DriveStyleNote", "Arcade grips the road and brakes hard; Simulation has less grip, weaker brakes and a livelier handbrake for drifting. Applies to your car immediately."));

	AddSection(TLOC("Gamepad", "Gamepad"));
	AddSlider(TLOC("Deadzone", "Stick dead zone"), 0.f, 0.5f, [S]() { return S->GamepadDeadzone; }, [S](float V) { S->GamepadDeadzone = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddSlider(TLOC("Vibration", "Vibration strength"), 0.f, 1.f, [S]() { return S->VibrationStrength; }, [S](float V) { S->VibrationStrength = V; },
		[](float V) { return V <= 0.f ? TLOC("Off", "Off") : FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddButtonRow(TLOC("TestRumble", "Test vibration"), TLOC("TestRumbleBtn", "Buzz the pad"), [this]()
	{
		if (AIronSiegePlayerController* PC = Owner.Get())
		{
			PC->PlayRumble(1.f, 0.5f);
		}
	});
	AddNote(TLOC("PadNote", "Triggers drive, left stick steers, shoulders fire, Start opens this menu. Buttons can be changed on the Shortcuts tab."));

	AddSection(TLOC("Weapons", "Weapons"));
	AddToggle(TLOC("AutoReload", "Auto-reload when empty"), [S]() { return S->bAutoReload; }, [S](bool V) { S->bAutoReload = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddToggle(TLOC("AutoFlares", "Automatic flares"), [S]() { return S->bAutoFlares; }, [S](bool V) { S->bAutoFlares = V; }, TLOC("On", "On"), TLOC("Off", "Off"));
	AddNote(TLOC("AutoFlaresNote", "Fires a flare burst by itself when a homing missile gets close. Easier, but it spends charges you might have timed better."));
	AddNote(TLOC("ControlsNote", "Key assignments are on the Shortcuts tab."));
}

void SIronSettingsMenu::BuildCamera()
{
	UIronSiegeUserSettings* S = Settings();
	AddSection(TLOC("ChaseCam", "Chase camera"));
	AddSlider(TLOC("Fov", "Field of view"), 70.f, 110.f, [S]() { return S->FieldOfView; }, [S](float V) { S->FieldOfView = FMath::RoundToFloat(V); },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f°"), V)); });
	auto Pct = [](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); };
	AddSlider(TLOC("CamDist", "Distance"), 0.7f, 1.5f, [S]() { return S->CameraDistance; }, [S](float V) { S->CameraDistance = V; }, Pct);
	AddSlider(TLOC("CamHeight", "Height"), 0.7f, 1.5f, [S]() { return S->CameraHeight; }, [S](float V) { S->CameraHeight = V; }, Pct);
	AddSlider(TLOC("CamSmooth", "Smoothing"), 0.f, 1.f, [S]() { return S->CameraSmoothing; }, [S](float V) { S->CameraSmoothing = V; }, Pct);
	AddNote(TLOC("CamNote", "Changes preview on your car immediately. Toggle cockpit view in game with the Toggle Camera key."));
}

void SIronSettingsMenu::BuildAccessibility()
{
	UIronSiegeUserSettings* S = Settings();
	AddSection(TLOC("Language", "Language"));
	AddSelector(TLOC("UiLanguage", "Interface language"),
		{ TLOC("English", "English"), FText::FromString(TEXT("العربية")) },
		[S]() { return S->Language == TEXT("ar") ? 1 : 0; },
		[this, S](int32 V) { S->Language = V == 1 ? TEXT("ar") : TEXT("en"); OnSettingChanged(); RebuildContent(); });

	AddSection(TLOC("Readability", "Readability"));
	TArray<FText> Scales;
	for (int32 i = 0; i < IronSettings::HudScaleCount; ++i)
	{
		Scales.Add(FText::FromString(FString::Printf(TEXT("%.0f%%"), IronSettings::HudScale(i) * 100.f)));
	}
	AddSelector(TLOC("HudSize", "HUD text & marker size"), Scales,
		[S]() { return S->HudScaleIndex; }, [S](int32 V) { S->HudScaleIndex = V; });
	AddSelector(TLOC("Colorblind", "Colour-blind mode"),
		{ TLOC("CbOff", "Off"), TLOC("CbProt", "Protanopia (red-blind)"), TLOC("CbDeut", "Deuteranopia (green-blind)"), TLOC("CbTrit", "Tritanopia (blue-blind)") },
		[S]() { return S->ColorblindMode; }, [S](int32 V) { S->ColorblindMode = V; });
	AddNote(TLOC("CbNote", "Enemies, repair kits and ammo are colour-coded; these palettes move them to hues that stay distinguishable."));

	AddSection(TLOC("Motion", "Motion"));
	AddToggle(TLOC("ReduceMotion", "Reduce camera motion"), [S]() { return S->bReduceMotion; }, [S](bool V) { S->bReduceMotion = V; },
		TLOC("On", "On"), TLOC("Off", "Off"));
	AddNote(TLOC("MotionNote", "Keeps the chase camera tight behind the car and damps shake, which helps with motion sickness."));
}

void SIronSettingsMenu::BuildProfiles()
{
	UIronSiegeUserSettings* S = Settings();
	AddSection(TLOC("ActiveProfile", "Active profile"));
	AddFixedKeyRow(TLOC("CurrentProfile", "Current"), FText::FromString(S->ProfileName));

	TSharedRef<SWidget> NameRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(230.f)
			[
				SAssignNew(ProfileNameBox, SEditableTextBox)
				.Text(FText::FromString(S->ProfileName))
				.Font(Font(12))
				.HintText(TLOC("ProfileHint", "Profile name"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
		[
			MakeButton(TLOC("SaveProfile", "Save"), [this, S]()
			{
				FString Name = ProfileNameBox.IsValid() ? ProfileNameBox->GetText().ToString().TrimStartAndEnd() : FString();
				// Profile names become file names, so keep them to safe characters.
				Name = FPaths::MakeValidFileName(Name);
				if (Name.IsEmpty())
				{
					return;
				}
				S->SaveProfile(Name);
				StatusText = FText::Format(TLOC("ProfileSaved", "Saved profile \"{0}\""), FText::FromString(Name));
				StatusUntil = FPlatformTime::Seconds() + 3.0;
				RebuildContent();
			}, true, 110.f)
		];
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(TLOC("SaveAs", "Save current settings as"), NameRow)];

	AddSection(TLOC("SavedProfiles", "Saved profiles"));
	const TArray<FString> Names = UIronSiegeUserSettings::GetProfileNames();
	if (Names.Num() == 0)
	{
		AddNote(TLOC("NoProfiles", "No saved profiles yet. Set the game up the way you like it, type a name above and press Save."));
	}
	for (const FString& Name : Names)
	{
		TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(TLOC("LoadProfile", "Load"), [this, S, Name]()
				{
					if (S->LoadProfile(Name))
					{
						OnSettingChanged();
						StatusText = FText::Format(TLOC("ProfileLoaded", "Loaded profile \"{0}\""), FText::FromString(Name));
						StatusUntil = FPlatformTime::Seconds() + 3.0;
						RebuildContent();
					}
				}, false, 110.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
			[
				MakeButton(TLOC("DeleteProfile", "Delete"), [this, Name]()
				{
					UIronSiegeUserSettings::DeleteProfile(Name);
					RebuildContent();
				}, false, 110.f)
			];
		Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeRow(FText::FromString(Name), Buttons)];
	}
	AddNote(TLOC("ProfileNote", "A profile stores every setting in this menu - video, audio, controls, key bindings, camera and accessibility - so two players on one PC can swap between their own setups."));
}

void SIronSettingsMenu::ShowBenchmarkResult(float AverageFps, float TargetFps, int32 Preset)
{
	SelectTab(ETab::Video);
	StatusText = FText::Format(TLOC("BenchResult", "Measured {0} FPS (target {1}) - quality set to {2}"),
		FText::AsNumber(FMath::RoundToInt(AverageFps)), FText::AsNumber(FMath::RoundToInt(TargetFps)),
		FText::FromString(IronText::Name(TEXT("Q"), QualityNames[FMath::Clamp(Preset, 0, 4)])));
	StatusUntil = FPlatformTime::Seconds() + 8.0;
}

void SIronSettingsMenu::BuildVoice()
{
	UIronSiegeUserSettings* S = Settings();
	AddNote(TLOC("VoiceIntro", "Iron Siege is single-player today: these settings are saved for voice chat when online play arrives. The microphone test below uses your real device now."));

	AddSection(TLOC("Microphone", "Microphone"));
	MicDevices.Reset();
	if (MicCapture)
	{
		TArray<Audio::FCaptureDeviceInfo> Devices;
		MicCapture->GetCaptureDevicesAvailable(Devices);
		for (const Audio::FCaptureDeviceInfo& D : Devices) MicDevices.Add(D.DeviceName);
	}
	TArray<FText> DeviceNames = { TLOC("DefaultDevice", "System default") };
	for (const FString& D : MicDevices) DeviceNames.Add(FText::FromString(D.Left(34)));
	AddSelector(TLOC("InputDevice", "Input device"), DeviceNames,
		[this, S]() { const int32 I = MicDevices.IndexOfByKey(S->MicDeviceName); return I == INDEX_NONE ? 0 : I + 1; },
		[this, S](int32 V) { S->MicDeviceName = V == 0 ? FString() : MicDevices[V - 1]; if (MicCapture && MicCapture->IsStreamOpen()) { StopMicTest(); StartMicTest(); } });
	AddSlider(TLOC("MicGain", "Input gain"), 0.f, 3.f, [S]() { return S->MicGain; }, [S](float V) { S->MicGain = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
	AddButtonRow(TLOC("MicTest", "Test microphone"),
		TLOC("MicTestToggle", "Start / stop test"),
		[this]() { if (MicCapture && MicCapture->IsStreamOpen()) StopMicTest(); else StartMicTest(); });
	AddMeter(TLOC("MicLevel", "Input level"));

	AddSection(TLOC("VoiceChat", "Voice chat"));
	AddToggle(TLOC("TalkMode", "Transmit mode"), [S]() { return S->bPushToTalk; }, [S](bool V) { S->bPushToTalk = V; }, TLOC("PTT", "Push to talk"), TLOC("OpenMic", "Open mic"));
	AddKeyRow(TLOC("PttKey", "Push to talk key"), TEXT("PushToTalk"));
	AddSlider(TLOC("VoiceVol", "Voice chat volume"), 0.f, 1.f, [S]() { return S->VoiceVolume; }, [S](float V) { S->VoiceVolume = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
}

void SIronSettingsMenu::BuildShortcuts()
{
	AddSection(TLOC("Driving", "Driving & combat"));
	for (const FIronBindableAction& A : UIronSiegeUserSettings::GetBindableActions())
	{
		if (A.Id != TEXT("PushToTalk"))
		{
			AddKeyRow(IronText::Get(*FTextInspector::GetKey(A.Label).Get(FString()), A.Label), A.Id);
		}
	}
	AddNote(TLOC("RebindNote", "Left column: keyboard and mouse. Right column: gamepad. Click either to change it - assigning something another action uses moves it here. Steering and the triggers are fixed to the sticks."));
	AddSection(TLOC("Fixed", "Menus (fixed)"));
	AddFixedKeyRow(TLOC("OpenSettings", "Open / close settings"), TLOC("EscF1", "Esc  /  F1"));
	AddFixedKeyRow(TLOC("MenuPick", "Menus & upgrade shop: choose"), TLOC("F3F4", "F3  /  F4"));
	AddFixedKeyRow(TLOC("MenuConfirm", "Menus & upgrade shop: confirm"), TLOC("EnterKey", "Enter"));
}

void SIronSettingsMenu::BuildGameplay()
{
	UIronSiegeUserSettings* S = Settings();
	AddSection(TLOC("Challenge", "Challenge"));
	TArray<FText> Diffs;
	for (int32 i = 0; i < IronSettings::DifficultyCount; ++i) Diffs.Add(FText::FromString(IronText::Name(TEXT("Diff"), ANSI_TO_TCHAR(IronSettings::DifficultyName(i)))));
	AddSelector(TLOC("Difficulty", "Difficulty"), Diffs, [S]() { return S->Difficulty; }, [this, S](int32 V) { S->Difficulty = V; OnSettingChanged(); });
	AddNote(TLOC("DiffNote", "Changes how hard enemy fire hits and how accurate it is. Applies to enemies that spawn from now on."));
	AddSelector(TLOC("AimAssist", "Aim assist"), { TLOC("Off", "Off"), TLOC("AimNormal", "Normal"), TLOC("AimStrong", "Strong") },
		[S]() { return S->AimAssist; }, [S](int32 V) { S->AimAssist = V; });
	AddNote(TLOC("AimNote", "The roof gun aims up and down at the car nearest the crosshair, and a little sideways. Strong widens the sideways help."));

	AddSection(TLOC("Hud", "HUD"));
	AddSelector(TLOC("Crosshair", "Crosshair"), { TLOC("Cross", "Cross"), TLOC("Dot", "Dot"), TLOC("Circle", "Circle"), TLOC("Hidden", "Hidden") },
		[S]() { return S->CrosshairStyle; }, [S](int32 V) { S->CrosshairStyle = V; });
	AddToggle(TLOC("Minimap", "Radar / minimap"), [S]() { return S->bShowMinimap; }, [S](bool V) { S->bShowMinimap = V; }, TLOC("Shown", "Shown"), TLOC("HiddenT", "Hidden"));
	AddToggle(TLOC("EnemyBars", "Enemy health bars"), [S]() { return S->bShowEnemyHealthBars; }, [S](bool V) { S->bShowEnemyHealthBars = V; }, TLOC("Shown", "Shown"), TLOC("HiddenT", "Hidden"));
	AddToggle(TLOC("Fps", "Frame rate counter"), [S]() { return S->bShowFps; }, [S](bool V) { S->bShowFps = V; }, TLOC("Shown", "Shown"), TLOC("HiddenT", "Hidden"));
	AddToggle(TLOC("DmgNumbers", "Damage numbers"), [S]() { return S->bDamageNumbers; }, [S](bool V) { S->bDamageNumbers = V; }, TLOC("Shown", "Shown"), TLOC("HiddenT", "Hidden"));
	AddSelector(TLOC("SpeedUnits", "Speed units"), { FText::FromString(TEXT("km/h")), FText::FromString(TEXT("mph")) },
		[S]() { return S->bMph ? 1 : 0; }, [S](int32 V) { S->bMph = V == 1; });
	AddSelector(TLOC("RadarZoom", "Radar range"), { TLOC("ZoomClose", "Close (30 m)"), TLOC("ZoomNormal", "Normal (45 m)"), TLOC("ZoomWide", "Wide (70 m)") },
		[S]() { return S->MinimapZoom; }, [S](int32 V) { S->MinimapZoom = V; });
	AddSlider(TLOC("HudOpacity", "HUD text opacity"), 0.3f, 1.f, [S]() { return S->HudOpacity; }, [S](float V) { S->HudOpacity = V; },
		[](float V) { return FText::FromString(FString::Printf(TEXT("%.0f%%"), V * 100.f)); });
}

// ------------------------------------------------------------------ input

void SIronSettingsMenu::BeginListening(FName Action, bool bPad)
{
	ListeningAction = Action;
	bListeningPad = bPad;
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}

void SIronSettingsMenu::FinishListening(const FKey& Key)
{
	if (!ListeningAction.IsNone() && Key.IsValid() && Settings())
	{
		if (bListeningPad)
		{
			Settings()->SetPadKeyFor(ListeningAction, Key);
		}
		else
		{
			Settings()->SetKeyFor(ListeningAction, Key);
		}
		OnSettingChanged();
		StatusText = FText::Format(TLOC("Bound", "Assigned {0}"), Key.GetDisplayName());
		StatusUntil = FPlatformTime::Seconds() + 2.0;
	}
	ListeningAction = NAME_None;
}

FReply SIronSettingsMenu::OnCaptureMouse(const FGeometry& Geometry, const FPointerEvent& Event)
{
	FinishListening(Event.GetEffectingButton());
	return FReply::Handled();
}

FReply SIronSettingsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!ListeningAction.IsNone())
	{
		if (Key == EKeys::Escape)
		{
			ListeningAction = NAME_None; // Cancel rebinding only.
		}
		else
		{
			FinishListening(Key);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::F1)
	{
		Discard();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ------------------------------------------------------------------ microphone

int32 SIronSettingsMenu::FindMicDeviceIndex() const
{
	const UIronSiegeUserSettings* S = Settings();
	const int32 I = S ? MicDevices.IndexOfByKey(S->MicDeviceName) : INDEX_NONE;
	return I == INDEX_NONE ? Audio::DefaultDeviceIndex : I;
}

void SIronSettingsMenu::StartMicTest()
{
	if (!MicCapture || MicCapture->IsStreamOpen())
	{
		return;
	}
	Audio::FAudioCaptureDeviceParams Params;
	Params.DeviceIndex = FindMicDeviceIndex();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const bool bOpened = MicCapture->OpenCaptureStream(Params, [this](const float* Audio, int32 NumFrames, int32 NumChannels, int32, double, bool)
	{
		float Peak = 0.f;
		for (int32 i = 0; i < NumFrames * NumChannels; ++i) Peak = FMath::Max(Peak, FMath::Abs(Audio[i]));
		MicPeak.store(FMath::Max(MicPeak.load(), Peak));
	}, 1024);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (bOpened && MicCapture->StartStream())
	{
		StatusText = TLOC("MicOn", "Microphone test running - speak now");
	}
	else
	{
		StatusText = TLOC("MicFail", "Could not open the microphone");
		MicCapture->CloseStream();
	}
	StatusUntil = FPlatformTime::Seconds() + 3.0;
}

void SIronSettingsMenu::StopMicTest()
{
	if (MicCapture && MicCapture->IsStreamOpen())
	{
		MicCapture->StopStream();
		MicCapture->CloseStream();
	}
	MicPeak.store(0.f);
	MicDisplay = 0.f;
}

void SIronSettingsMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Meter: jump up to new peaks, fall back smoothly.
	const float Gain = Settings() ? Settings()->MicGain : 1.f;
	const float Target = IronSettings::MicMeter(MicPeak.exchange(0.f), Gain);
	MicDisplay = Target > MicDisplay ? Target : FMath::Max(0.f, MicDisplay - InDeltaTime * 1.5f);
}

#undef LOCTEXT_NAMESPACE
