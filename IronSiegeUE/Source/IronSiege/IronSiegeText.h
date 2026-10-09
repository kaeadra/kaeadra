#pragma once
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

// Arabic UI without a localization pipeline.
//
// The game's UI strings live in the source as English LOCTEXT. Rather than run the GatherText
// commandlet and ship .locres files (which would need an editor build step before every run),
// this module keeps a translation table keyed by the same LOCTEXT key and swaps the string in when
// the player picks Arabic in the settings. The menu rebuilds itself on a language change, so the
// swap is immediate.
//
// Arabic also needs a font with Arabic glyphs: Slate's default (Roboto) has none. Content/Python/
// create_ui_fonts.py imports one as /Game/IronSiege/UI/F_IronUI; if it is missing everything falls
// back to the engine font, which shows boxes for Arabic but never crashes.
namespace IronText
{
// Translation for Key if the UI language is Arabic and the key is in the table, else English.
IRONSIEGE_API FText Get(const TCHAR* Key, const FText& English);

// Same, for the Canvas HUD which builds its strings with FString::Printf.
IRONSIEGE_API FString Str(const TCHAR* Key, const TCHAR* English);

IRONSIEGE_API bool IsArabic();

// Same, for a name from a rules table or list ("SCOUT", "Arctic"...): the key is Prefix + the
// English name, so the tables themselves stay in English.
IRONSIEGE_API FString Name(const TCHAR* Prefix, const FString& English);

// Menu font: the imported UI font when it exists (needed for Arabic), else the Slate default.
IRONSIEGE_API FSlateFontInfo Font(int32 Size, bool bBold);

// Canvas/HUD font for measuring text (UCanvas::StrLen only takes a UFont).
IRONSIEGE_API class UFont* CanvasFont(int32 SizeKind = 0);

// Point size the HUD draws a given SizeKind at when it goes through a Slate font (Arabic).
IRONSIEGE_API int32 CanvasFontSize(int32 SizeKind);
}

// Looks a translation up by its LOCTEXT key, falling back to the English source text.
// Usage is identical to LOCTEXT, so the menu reads the same as before.
#define TLOC(Key, Text) IronText::Get(TEXT(Key), LOCTEXT(Key, Text))
