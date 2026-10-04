#pragma once
// Engine-independent story campaign: eight missions across the four battlefields, each a short
// sequence of stages with one objective (reach a point, destroy the patrol, burn the depots, escort
// the trucks, take the jammer sites, hold the relay, survive, stop the convoy, beat the boss), the
// enemies that come with it, the radio lines, star ratings and the saved progress that unlocks the
// next mission, new drivers and new weapons. No Unreal includes on purpose: covered offline by
// Tests/mission_rules_test.cpp. AIronMissionDirector plays these out in the game.
//
// Text here is the English source; the Arabic lives in IronSiegeStory.cpp under the same keys.

#include "CrewRules.h"
#include "UpgradeRules.h"
#include <initializer_list>

namespace IronMissions
{
inline constexpr int Count = 8;
inline constexpr int MaxStages = 3;
inline constexpr int MaxSpots = 6;
inline constexpr int MaxLines = 2;
inline constexpr int BriefLines = 3;

enum class Objective
{
	Reach,          // Drive into a marked zone (the farthest of the stage's spots from the player).
	Eliminate,      // Destroy Count enemies.
	DestroyTargets, // Destroy the structures standing on the stage's spots.
	Escort,         // Friendly trucks drive the spots as a route; Need of them must arrive.
	Capture,        // Hold each spot's zone until it is taken.
	Defend,         // Keep the structure on the first spot alive for Seconds.
	Survive,        // Stay alive for Seconds.
	Intercept,      // Enemy trucks drive the spots as a route; stop Need of them before they get out.
	Boss            // Destroy the stage's boss.
};
inline constexpr int ObjectiveCount = 9;

// Enemies by kind (the same kinds the survival waves use).
struct Squad
{
	int Buggies = 0;
	int Raiders = 0;
	int Hunters = 0;
	int Stormers = 0;
	int Lancers = 0;

	int Total() const { return Buggies + Raiders + Hunters + Stormers + Lancers; }
};

// A place on the battlefield as a fraction of the arena's half extent (-1..1, centre at 0), so the
// same mission data works whatever the map's size. The director moves a spot off any prop it
// lands on, and onto the nearest road on the city map.
struct Spot
{
	float X = 0.f;
	float Y = 0.f;
};

struct Radio
{
	int Speaker = IronCrew::SpeakerHana;
	int Mood = IronCrew::MoodNeutral;
	const char* Key = nullptr;
	const char* Text = nullptr;
};

// What stands on the spots of a DestroyTargets / Defend stage.
inline constexpr int StructDepot = 0;     // Legion fuel depot.
inline constexpr int StructRadar = 1;     // Legion radar mast.
inline constexpr int StructGenerator = 2; // Legion shield generator.
inline constexpr int StructRelay = 3;     // Our uplink relay (the one to defend).

inline constexpr int BossNone = 0;
inline constexpr int BossRaven = 1; // Railgun ace in a black sedan.
inline constexpr int BossBaron = 2; // The Iron Baron's Juggernaut.

struct Stage
{
	Objective Kind = Objective::Eliminate;
	int Count = 0;            // Kills, structures, zones or trucks.
	int Need = 0;             // Escort / Intercept: trucks that must arrive / be stopped.
	float Seconds = 0.f;      // Survive / Defend: how long to hold. Otherwise a time limit (0 = none).
	Squad Opening;            // Arrives as the stage starts.
	Squad Reinforce;          // Arrives every ReinforceEvery seconds while there is room.
	float ReinforceEvery = 0.f;
	int MaxAlive = 6;         // Reinforcements wait while this many enemies are still alive.
	float AssetShare = 0.f;   // Fraction of the enemies that go for the trucks / relay, not the player.
	int Boss = BossNone;
	int Structure = StructDepot;
	bool bGuardSpots = false; // The opening squad starts around the spots instead of around the player.
	const char* TextKey = nullptr;
	const char* Text = nullptr; // The objective line on the HUD.
	Spot Spots[MaxSpots];
	int NumSpots = 0;
	Radio Lines[MaxLines];    // Said on the radio as the stage starts.
	int NumLines = 0;
};

struct Mission
{
	const char* Key = nullptr;
	const char* Name = nullptr;
	const char* Map = nullptr;  // Desert, Coast, CityRuins or Arctic (the level is Map_<Map>).
	Spot Start;                 // The player deploys from the player start nearest this.
	Radio Brief[BriefLines];    // The briefing screen, top to bottom.
	Stage Stages[MaxStages];
	int NumStages = 0;
	Radio Outro[MaxLines];      // Said once the last stage is done.
	int NumOutro = 0;
	float ParSeconds = 0.f;     // Finishing inside this earns a star.
};

namespace Detail
{
inline Stage MakeStage(Objective Kind, const char* Key, const char* Text, int InCount, float Seconds)
{
	Stage S;
	S.Kind = Kind;
	S.TextKey = Key;
	S.Text = Text;
	S.Count = InCount;
	S.Need = InCount;
	S.Seconds = Seconds;
	return S;
}

inline void Spots(Stage& S, std::initializer_list<Spot> List)
{
	for (const Spot& P : List)
	{
		if (S.NumSpots < MaxSpots) S.Spots[S.NumSpots++] = P;
	}
}

inline void Say(Stage& S, int Speaker, int Mood, const char* Key, const char* Text)
{
	if (S.NumLines < MaxLines) S.Lines[S.NumLines++] = { Speaker, Mood, Key, Text };
}

inline void Waves(Stage& S, Squad Opening, Squad Reinforce, float Every, int MaxAlive)
{
	S.Opening = Opening;
	S.Reinforce = Reinforce;
	S.ReinforceEvery = Every;
	S.MaxAlive = MaxAlive;
}

inline void Brief(Mission& M, const char* K0, const char* T0, const char* K1, const char* T1, const char* K2, const char* T2)
{
	M.Brief[0] = { IronCrew::SpeakerHana, IronCrew::MoodNeutral, K0, T0 };
	M.Brief[1] = { IronCrew::SpeakerHana, IronCrew::MoodNeutral, K1, T1 };
	M.Brief[2] = { IronCrew::SpeakerHana, IronCrew::MoodNeutral, K2, T2 };
}

inline void Outro(Mission& M, int Speaker, int Mood, const char* Key, const char* Text)
{
	if (M.NumOutro < MaxLines) M.Outro[M.NumOutro++] = { Speaker, Mood, Key, Text };
}

using namespace IronCrew;

inline Mission FirstSortie()
{
	Mission M;
	M.Key = "M1";
	M.Name = "FIRST SORTIE";
	M.Map = "Desert";
	M.ParSeconds = 150.f;
	M.Start = { -.6f, -.6f };
	Brief(M, "M1Brief0", "The Rust Legion has ringed the free city of Marsa with steel. Nothing gets in, nothing gets out.",
		"M1Brief1", "You are the newest driver of the Siege Breakers. Today we find out if you can drive and shoot at the same time.",
		"M1Brief2", "Reach the rally point, then clear the Legion patrol sniffing around our fuel cache.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Reach, "M1Obj0", "Reach the rally point", 1, 0.f);
	Spots(A, { { 0.f, .45f }, { 0.f, -.45f } });
	Say(A, SpeakerHana, MoodNeutral, "M1S0L0", "Overwatch to Breaker. Engine warm? Follow the marker to the rally point.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Eliminate, "M1Obj1", "Destroy the Legion patrol", 5, 0.f);
	Waves(B, { 3 }, { 2 }, 14.f, 4);
	Say(B, SpeakerHana, MoodAngry, "M1S1L0", "Contacts! Legion buggies closing on you. Weapons free.");
	Outro(M, SpeakerHana, MoodHappy, "M1Out0", "Patrol is scrap. Not bad for a first sortie. Come home, Breaker.");
	return M;
}

inline Mission FuelLines()
{
	Mission M;
	M.Key = "M2";
	M.Name = "FUEL LINES";
	M.Map = "Desert";
	M.ParSeconds = 300.f;
	M.Start = { -.6f, -.6f };
	Brief(M, "M2Brief0", "The Legion's armor drinks fuel by the tanker. They stockpile it in three depots out in this desert.",
		"M2Brief1", "Burn all three. Expect guards, and a response force once they see the smoke.",
		"M2Brief2", "New kit for this run: a mine layer. Drop mines behind you when they give chase.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::DestroyTargets, "M2Obj0", "Destroy the fuel depots", 3, 0.f);
	Spots(A, { { -.45f, .35f }, { .5f, .1f }, { .05f, -.55f } });
	Waves(A, { 3 }, { 2 }, 22.f, 5);
	A.bGuardSpots = true;
	Say(A, SpeakerHana, MoodNeutral, "M2S0L0", "Three depots marked. Rockets do the most work on those tanks.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Eliminate, "M2Obj1", "Destroy the response force", 6, 0.f);
	Waves(B, { 2, 3 }, { 1, 1 }, 12.f, 6);
	Say(B, SpeakerHana, MoodAngry, "M2S1L0", "They saw the smoke. Raiders inbound, fast ones. Use your mines.");
	Outro(M, SpeakerHana, MoodHappy, "M2Out0", "Three columns of smoke on the horizon. Their tanks go thirsty tonight.");
	return M;
}

inline Mission HarborRun()
{
	Mission M;
	M.Key = "M3";
	M.Name = "HARBOR RUN";
	M.Map = "Coast";
	M.ParSeconds = 240.f;
	M.Start = { -.6f, .6f };
	Brief(M, "M3Brief0", "Two trucks of medicine are waiting on the quay. The city needs them tonight.",
		"M3Brief1", "The Legion owns the container yard. Escort the trucks around it to the gate.",
		"M3Brief2", "Stay close: they will go for the trucks, not for you. At least one truck must make it.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Escort, "M3Obj0", "Escort the supply trucks to the gate", 2, 0.f);
	A.Need = 1;
	// The open band between the container yard and the perimeter wall, round three sides of the port.
	Spots(A, { { -.78f, .3f }, { -.78f, -.78f }, { .78f, -.78f }, { .78f, .6f } });
	Waves(A, { 3 }, { 2, 1 }, 13.f, 6);
	A.AssetShare = .7f;
	Say(A, SpeakerHana, MoodNeutral, "M3S0L0", "Convoy is rolling. Keep the Legion off those trucks.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Eliminate, "M3Obj1", "Clear the pursuers", 5, 0.f);
	Waves(B, { 2, 2 }, { 1, 1 }, 12.f, 6);
	Say(B, SpeakerHana, MoodAngry, "M3S1L0", "Trucks are through the gate. Now turn around and finish the ones chasing.");
	Outro(M, SpeakerHana, MoodHappy, "M3Out0", "The medicine is in the city. You just saved more lives than you will ever count.");
	return M;
}

inline Mission DeadAir()
{
	Mission M;
	M.Key = "M4";
	M.Name = "DEAD AIR";
	M.Map = "Coast";
	M.ParSeconds = 330.f;
	M.Start = { -.6f, -.6f };
	Brief(M, "M4Brief0", "Three Legion jammers are drowning every frequency in the harbour. We are deaf and blind.",
		"M4Brief1", "Take each jammer site and hold it until our link kicks in. Then they will come for our relay.",
		"M4Brief2", "Protect the relay until the uplink completes. Flame tank fitted: good for close work around the sites.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Capture, "M4Obj0", "Capture the jammer sites", 3, 0.f);
	// Lane crossings of the container yard (the yards sit on a 0.27 grid, the lanes halfway between).
	Spots(A, { { -.41f, .41f }, { .41f, .41f }, { .137f, -.41f } });
	Waves(A, { 4 }, { 2, 1 }, 18.f, 6);
	A.bGuardSpots = true;
	Say(A, SpeakerHana, MoodNeutral, "M4S0L0", "Park inside the ring and keep it clear. The capture stalls while they contest it.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Defend, "M4Obj1", "Defend the uplink relay", 1, 90.f);
	Spots(B, { { .137f, .137f } });
	B.Structure = StructRelay;
	Waves(B, { 3, 2 }, { 2, 1, 1 }, 14.f, 7);
	B.AssetShare = .6f;
	Say(B, SpeakerHana, MoodAngry, "M4S1L0", "Uplink started. Ninety seconds. They are heading straight for the relay!");
	Outro(M, SpeakerHana, MoodNeutral, "M4Out0", "Signal is back. I can hear the whole city... and a Legion callsign I hoped never to hear again. Raven.");
	return M;
}

inline Mission StreetByStreet()
{
	Mission M;
	M.Key = "M5";
	M.Name = "STREET BY STREET";
	M.Map = "CityRuins";
	M.ParSeconds = 300.f;
	M.Start = { -.7f, 0.f };
	Brief(M, "M5Brief0", "Marsa's outer district. Our evacuation column is stuck two blocks from the Legion's line.",
		"M5Brief1", "Drive to the junction and hold it while the column pulls out. Missile hunters are in the streets: watch for the warning and use your flares.",
		"M5Brief2", "Hold for two and a half minutes. Do not chase. Hold.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Reach, "M5Obj0", "Reach the evacuation junction", 1, 0.f);
	// The two junctions of the avenue with the cross streets.
	Spots(A, { { .337f, 0.f }, { -.316f, 0.f } });
	Waves(A, { 3 }, { 2 }, 20.f, 4);
	Say(A, SpeakerHana, MoodNeutral, "M5S0L0", "Take the avenue. Wrecks and tank traps everywhere: keep a lane open.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Survive, "M5Obj1", "Hold until the column is clear", 1, 150.f);
	Waves(B, { 3, 1, 1 }, { 2, 1, 1, 1 }, 15.f, 7);
	Say(B, SpeakerHana, MoodAngry, "M5S1L0", "Column is moving. Everything they have is coming down those streets. Hold!");
	Say(B, SpeakerRaven, MoodNeutral, "M5S1L1", "So you are the new Breaker. I will remember the sound of your engine.");
	Outro(M, SpeakerHana, MoodNeutral, "M5Out0", "Column is clear. That voice on the radio was Raven. Next time she will not just talk.");
	return M;
}

inline Mission RavenDuel()
{
	Mission M;
	M.Key = "M6";
	M.Name = "RAVEN";
	M.Map = "CityRuins";
	M.ParSeconds = 300.f;
	M.Start = { .7f, 0.f };
	Brief(M, "M6Brief0", "Raven is the Legion's railgun ace. Eleven of our drivers are gone because of her.",
		"M6Brief1", "She hunts with an escort. Strip the escort first, then she will come for you herself.",
		"M6Brief2", "Her rail charges before it fires, and you will hear it. Swerve the moment it whines. A railgun is fitted to your car too.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Eliminate, "M6Obj0", "Destroy Raven's escort", 6, 0.f);
	Waves(A, { 2, 2, 1, 1 }, { 1, 1 }, 14.f, 6);
	Say(A, SpeakerRaven, MoodHappy, "M6S0L0", "Come on then. Let us see what Hana's new pet can do.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Boss, "M6Obj1", "Defeat Raven", 1, 0.f);
	Waves(B, {}, { 1, 1 }, 25.f, 4);
	B.Boss = BossRaven;
	Say(B, SpeakerHana, MoodAngry, "M6S1L0", "That is her: black sedan, twin rails. Do not drive straight!");
	Say(B, SpeakerRaven, MoodAngry, "M6S1L1", "Hold still. This only hurts once.");
	Outro(M, SpeakerRaven, MoodNeutral, "M6Out0", "...Not bad. Tell Hana I never liked the General anyway.");
	Outro(M, SpeakerHana, MoodHappy, "M6Out1", "Raven is down. The Legion just lost its sharpest blade.");
	return M;
}

inline Mission ColdTrail()
{
	Mission M;
	M.Key = "M7";
	M.Name = "COLD TRAIL";
	M.Map = "Arctic";
	M.ParSeconds = 300.f;
	M.Start = { -.6f, -.6f };
	Brief(M, "M7Brief0", "With Raven gone, General Varga is pulling his shield generators north across the ice.",
		"M7Brief1", "A convoy carries the control cores. Stop the trucks before they leave the lake. We can afford to lose one, no more.",
		"M7Brief2", "Then blind his radar masts so he never sees our final push. Tesla coil fitted.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::Intercept, "M7Obj0", "Stop the Legion convoy", 3, 0.f);
	A.Need = 2;
	// Round the lake, outside the ring of ice spires. The trucks line up behind the first point, so
	// it sits far enough along the wall to leave them room.
	Spots(A, { { -.5f, -.78f }, { .78f, -.78f }, { .78f, .78f }, { -.78f, .78f }, { -.78f, -.2f } });
	Waves(A, { 2, 2, 0, 1 }, { 1, 1 }, 18.f, 6);
	Say(A, SpeakerHana, MoodAngry, "M7S0L0", "Convoy on the ice, heading for the north gate. Do not let them off the lake!");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::DestroyTargets, "M7Obj1", "Destroy the radar masts", 2, 0.f);
	Spots(B, { { -.5f, .42f }, { .52f, -.4f } });
	B.Structure = StructRadar;
	Waves(B, { 2, 1, 1, 1, 1 }, { 2, 1 }, 18.f, 7);
	B.bGuardSpots = true;
	Say(B, SpeakerHana, MoodNeutral, "M7S1L0", "Cores secured. Now the radar: two masts.");
	Outro(M, SpeakerHana, MoodHappy, "M7Out0", "He is blind, and his shields are ours to break. One more push, Breaker.");
	return M;
}

inline Mission BreakTheSiege()
{
	Mission M;
	M.Key = "M8";
	M.Name = "BREAK THE SIEGE";
	M.Map = "Arctic";
	M.ParSeconds = 420.f;
	M.Start = { -.6f, -.6f };
	Brief(M, "M8Brief0", "This is it. Varga's command fortress sits on the ice behind three shield generators.",
		"M8Brief1", "Break the generators, hold against his last reserve, and the Iron Baron himself will roll out.",
		"M8Brief2", "Everything we have is on your car. End the siege.");
	Stage& A = M.Stages[M.NumStages++] = MakeStage(Objective::DestroyTargets, "M8Obj0", "Destroy the shield generators", 3, 0.f);
	Spots(A, { { 0.f, .5f }, { -.45f, -.3f }, { .45f, -.3f } });
	A.Structure = StructGenerator;
	Waves(A, { 3, 1, 1, 1, 1 }, { 2, 1 }, 16.f, 7);
	A.bGuardSpots = true;
	Say(A, SpeakerVarga, MoodNeutral, "M8S0L0", "A single car? Hana sends me a child to die on my ice.");
	Stage& B = M.Stages[M.NumStages++] = MakeStage(Objective::Survive, "M8Obj1", "Hold against the counterattack", 1, 60.f);
	Waves(B, { 2, 2, 1, 1, 1 }, { 2, 1, 1 }, 12.f, 8);
	Say(B, SpeakerHana, MoodAngry, "M8S1L0", "Shields are down! His reserve is charging you. Hold on!");
	Stage& C = M.Stages[M.NumStages++] = MakeStage(Objective::Boss, "M8Obj2", "Destroy the Iron Baron", 1, 0.f);
	Waves(C, {}, { 1, 1, 0, 1 }, 22.f, 5);
	C.Boss = BossBaron;
	Say(C, SpeakerVarga, MoodAngry, "M8S2L0", "Enough. I will crush you myself.");
	Say(C, SpeakerHana, MoodAngry, "M8S2L1", "That is the Juggernaut. Hit it with everything!");
	Outro(M, SpeakerVarga, MoodAngry, "M8Out0", "Impossible... my siege...");
	Outro(M, SpeakerHana, MoodHappy, "M8Out1", "It is over. The road to Marsa is open. Welcome home, Siege Breaker.");
	return M;
}
}

inline const Mission& Get(int Index)
{
	static const Mission Table[Count] = {
		Detail::FirstSortie(), Detail::FuelLines(), Detail::HarborRun(), Detail::DeadAir(),
		Detail::StreetByStreet(), Detail::RavenDuel(), Detail::ColdTrail(), Detail::BreakTheSiege(),
	};
	return Table[Index >= 0 && Index < Count ? Index : 0];
}

// ---- Bosses and trucks

struct BossDef
{
	const char* NameKey;
	const char* Name;
	float Durability; // Multiplier on the chassis' health and armor.
};

inline const BossDef& GetBoss(int Boss)
{
	static const BossDef Defs[3] = {
		{ "BossNone", "", 1.f },
		{ "BossRaven", "RAVEN", 3.f },
		{ "BossBaron", "THE IRON BARON", 5.f },
	};
	return Defs[Boss >= 0 && Boss < 3 ? Boss : 0];
}

struct ConvoyTuning
{
	float EscortKph = 24.f;      // The friendly trucks crawl, so the fight stays around them.
	float InterceptKph = 22.f;   // The Legion's trucks are loaded.
	float WaypointRadius = 700.f;// A truck turns for the next waypoint this close to the current one.
	float Spacing = 1100.f;      // Gap between trucks at the start (they line up behind the first point).
	float EnemyDurability = 0.7f;// The Legion's trucks are soft-skinned next to a war car.
	float FriendlyDurability = 0.3f; // Ours are plain cargo trucks. Measured with nobody defending: at 0.5 the lead truck reached the gate with 48% left, the other with 82%.
};

// Health of the structures a stage puts on its spots (StructDepot..StructRelay). Only the enemies
// sent at the relay damage it (IronMissions::MaxOnAsset of them at most); a gun car lined up on it
// does about 14 a second (measured in -game runs). Sized so that the pair the opening squad sends,
// left alone, brings it down inside the ninety seconds (measured: at 70-80 s), and a defender who
// keeps destroying them does not lose it (measured: 57% left with one destroyed every 7 s).
inline float StructureHealth(int Structure)
{
	static const float Health[4] = { 420.f, 380.f, 520.f, 1400.f };
	return Health[Structure >= 0 && Structure < 4 ? Structure : 0];
}

// ---- A stage being played

struct StageState
{
	float Elapsed = 0.f;
	int Progress = 0;        // Kills, structures down, zones taken, trucks home / stopped.
	int Lost = 0;            // Friendly trucks destroyed / enemy trucks that got away.
	bool bAssetLost = false; // Defend: the structure was destroyed.
	float ReinforceClock = 0.f;
};

enum class Outcome { Running, Complete, Failed };

inline Outcome Evaluate(const Stage& S, const StageState& St)
{
	switch (S.Kind)
	{
	case Objective::Survive:
		return St.Elapsed >= S.Seconds ? Outcome::Complete : Outcome::Running;
	case Objective::Defend:
		if (St.bAssetLost) return Outcome::Failed;
		return St.Elapsed >= S.Seconds ? Outcome::Complete : Outcome::Running;
	case Objective::Escort:
	case Objective::Intercept:
		// Too many lost to ever reach Need: over at once. Otherwise wait for every truck to be
		// accounted for, so the stage does not end with one still on the road.
		if (St.Lost > S.Count - S.Need) return Outcome::Failed;
		return St.Progress + St.Lost >= S.Count ? Outcome::Complete : Outcome::Running;
	default:
		if (St.Progress >= S.Count) return Outcome::Complete;
		return S.Seconds > 0.f && St.Elapsed >= S.Seconds ? Outcome::Failed : Outcome::Running;
	}
}

// Seconds left on the stage's clock, or -1 when it has none.
inline float TimeLeft(const Stage& S, const StageState& St)
{
	if (S.Seconds <= 0.f) return -1.f;
	const float Left = S.Seconds - St.Elapsed;
	return Left > 0.f ? Left : 0.f;
}

// As much of Wanted as fits in Room, kind by kind in the order they are listed.
inline Squad Fit(const Squad& Wanted, int Room)
{
	Squad Out;
	int* Slots[5] = { &Out.Buggies, &Out.Raiders, &Out.Hunters, &Out.Stormers, &Out.Lancers };
	const int Asked[5] = { Wanted.Buggies, Wanted.Raiders, Wanted.Hunters, Wanted.Stormers, Wanted.Lancers };
	for (int i = 0; i < 5 && Room > 0; ++i)
	{
		const int Take = Asked[i] < Room ? Asked[i] : Room;
		*Slots[i] = Take > 0 ? Take : 0;
		Room -= *Slots[i];
	}
	return Out;
}

// One frame of the stage clock. Returns the reinforcements to spawn now (empty most frames): a
// group every ReinforceEvery seconds, cut down to the room left under MaxAlive. While the field is
// full the clock holds at "due", so the group arrives the moment there is room.
inline Squad Tick(const Stage& S, StageState& St, float DeltaSeconds, int EnemiesAlive)
{
	St.Elapsed += DeltaSeconds;
	if (S.ReinforceEvery <= 0.f || S.Reinforce.Total() <= 0) return Squad();
	St.ReinforceClock += DeltaSeconds;
	if (St.ReinforceClock < S.ReinforceEvery) return Squad();
	const int Room = S.MaxAlive - EnemiesAlive;
	if (Room <= 0)
	{
		St.ReinforceClock = S.ReinforceEvery;
		return Squad();
	}
	St.ReinforceClock = 0.f;
	return Fit(S.Reinforce, Room);
}

// How many enemies may be after the stage's trucks or relay at the same time: its share of a full
// field, to the nearest car. The rest hunt the player.
inline int MaxOnAsset(const Stage& S)
{
	const float Wanted = S.MaxAlive * S.AssetShare;
	return Wanted <= 0.f ? 0 : static_cast<int>(Wanted + 0.5f);
}

// ---- Capture zones

struct CaptureTuning
{
	float Seconds = 8.f;          // Uncontested time to take a zone.
	float DecayPerSecond = 0.06f; // Progress drains when the player leaves before it is taken.
	float RadiusCm = 800.f;
};

struct CaptureState
{
	float Progress = 0.f; // 0..1
	bool bCaptured = false;
};

// True on the frame the zone is taken. Enemies inside freeze the progress (contested); once taken
// a zone stays taken.
inline bool TickCapture(CaptureState& Z, bool bPlayerInside, int EnemiesInside, float DeltaSeconds, const CaptureTuning& T = CaptureTuning())
{
	if (Z.bCaptured) return false;
	if (bPlayerInside)
	{
		if (EnemiesInside <= 0) Z.Progress += DeltaSeconds / T.Seconds;
	}
	else
	{
		Z.Progress -= T.DecayPerSecond * DeltaSeconds;
	}
	if (Z.Progress < 0.f) Z.Progress = 0.f;
	if (Z.Progress >= 1.f)
	{
		Z.Progress = 1.f;
		Z.bCaptured = true;
		return true;
	}
	return false;
}

// ---- Stars and saved progress

// One for finishing, one for beating the par time, one for bringing the car home with at least
// half its health and without losing a truck.
inline int Stars(bool bComplete, float Seconds, float ParSeconds, float HealthFraction, int AssetsLost)
{
	if (!bComplete) return 0;
	return 1 + (Seconds <= ParSeconds ? 1 : 0) + (HealthFraction >= 0.5f && AssetsLost == 0 ? 1 : 0);
}

struct Progress
{
	int Best[Count] = {}; // Best star count per mission, 0 = not completed.

	bool IsCompleted(int Index) const { return Index >= 0 && Index < Count && Best[Index] > 0; }

	// Missions open in order: the first is always open, the rest once the one before is done.
	bool IsUnlocked(int Index) const
	{
		if (Index < 0 || Index >= Count) return false;
		return Index == 0 || Best[Index - 1] > 0;
	}

	int Completed() const
	{
		int N = 0;
		for (int i = 0; i < Count; ++i) N += Best[i] > 0 ? 1 : 0;
		return N;
	}

	int TotalStars() const
	{
		int N = 0;
		for (int i = 0; i < Count; ++i) N += Best[i];
		return N;
	}

	// Keeps the best result. True if this run improved on it.
	bool Record(int Index, int InStars)
	{
		if (Index < 0 || Index >= Count || InStars <= Best[Index]) return false;
		Best[Index] = InStars > 3 ? 3 : InStars;
		return true;
	}

	// The mission the campaign menu opens on: the first one not completed yet (the last if all are).
	int NextMission() const
	{
		for (int i = 0; i < Count; ++i)
		{
			if (Best[i] <= 0) return i;
		}
		return Count - 1;
	}
};

inline bool IsDriverUnlocked(const Progress& P, IronCrew::Driver D)
{
	return P.Completed() >= IronCrew::Get(D).UnlockAfter;
}

// The car the campaign hands out: weapons arrive mission by mission (there is no shop between
// stages), and stars buy a little extra - one level of armor at 8 stars, one of each gun at 16.
inline IronUpgrades::Loadout LoadoutFor(int MissionIndex, int TotalStars)
{
	using IronUpgrades::Upgrade;
	IronUpgrades::Loadout L;
	auto Set = [&L](Upgrade U, int Level)
	{
		const int Max = IronUpgrades::Get(U).MaxLevel;
		int& Slot = L.Levels[static_cast<int>(U)];
		Slot = Level > Max ? Max : (Level > Slot ? Level : Slot);
	};
	if (MissionIndex >= 1) Set(Upgrade::Mines, 1);
	if (MissionIndex >= 2) Set(Upgrade::Armor, 1);
	if (MissionIndex >= 3) Set(Upgrade::Flamer, 1);
	if (MissionIndex >= 4) { Set(Upgrade::MachineGun, 1); Set(Upgrade::Rockets, 1); }
	if (MissionIndex >= 5) Set(Upgrade::Railgun, 1);
	if (MissionIndex >= 6) { Set(Upgrade::Tesla, 1); Set(Upgrade::Engine, 1); }
	if (MissionIndex >= 7) { Set(Upgrade::Armor, 2); Set(Upgrade::MachineGun, 2); Set(Upgrade::Rockets, 2); Set(Upgrade::Mines, 2); }
	if (TotalStars >= 8) Set(Upgrade::Armor, L.Level(Upgrade::Armor) + 1);
	if (TotalStars >= 16)
	{
		Set(Upgrade::MachineGun, L.Level(Upgrade::MachineGun) + 1);
		Set(Upgrade::Rockets, L.Level(Upgrade::Rockets) + 1);
	}
	return L;
}
}
