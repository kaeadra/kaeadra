#pragma once
#include "CoreMinimal.h"
#include "MissionRules.h"
#include "RankRules.h"

class UTexture2D;

// The parts of the campaign that need the engine: the Arabic for the story's text keys, the
// characters' portraits and colours, and the saved campaign progress.
namespace IronStory
{
// Arabic for a campaign or crew text key (MissionRules.h, CrewRules.h, the campaign screens), or
// null if it has none. IronText::Str / Get fall back to this after their own table.
const FString* FindArabic(const FString& Key);

IRONSIEGE_API FString MissionName(int32 MissionIndex);

// Name shown over a radio line, and the colour of that character's frame and name.
IRONSIEGE_API FString SpeakerName(int32 Speaker);
IRONSIEGE_API FLinearColor SpeakerColor(int32 Speaker);

// A character's portrait for a mood: Content/IronSiege/Characters/<Key>.png, or <Key>_happy.png /
// <Key>_angry.png when that picture exists. Read from disk the first time it is asked for and kept
// for the session; null when there is no picture at all (the HUD then draws a plain panel).
// Replacing a file with new art of the same name is all it takes to change a face.
IRONSIEGE_API UTexture2D* Portrait(int32 Speaker, int32 Mood);

// Campaign progress and the last driver / vehicle chosen, kept in Game.ini beside the survival records.
IRONSIEGE_API IronMissions::Progress LoadProgress();
IRONSIEGE_API void SaveProgress(const IronMissions::Progress& Progress);
IRONSIEGE_API void LoadLastChoice(int32& Driver, int32& Vehicle);
IRONSIEGE_API void SaveLastChoice(int32 Driver, int32 Vehicle);

// Every driver's experience (RankRules.h), kept in Game.ini beside the campaign progress.
IRONSIEGE_API IronRanks::Roster LoadRoster();
IRONSIEGE_API void SaveRoster(const IronRanks::Roster& Roster);
}
