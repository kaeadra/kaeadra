#pragma once
// Engine-independent crew: the six drivers the player can put behind the wheel (a passive perk and
// one active ability each), the story cast that talks on the radio, and the short lines a driver
// throws out during a fight. No Unreal includes on purpose: covered offline by
// Tests/crew_rules_test.cpp.
//
// Text here is the English source; the Arabic lives in IronSiegeStoryText.cpp under the same keys.
// A character's Key is also its portrait file name (Content/IronSiege/Characters/<Key>.png).

namespace IronCrew
{
inline constexpr int DriverCount = 6;

enum class Driver { Rin, Layla, Kenji, Yuki, Zara, Omar };

// Everyone with a face on the radio: the six drivers first (same indices as Driver), then the cast.
inline constexpr int SpeakerHana = 6;   // Commander of the Siege Breakers ("Overwatch").
inline constexpr int SpeakerRaven = 7;  // The Legion's railgun ace.
inline constexpr int SpeakerVarga = 8;  // General Varga, the Iron Baron.
inline constexpr int SpeakerCount = 9;

// Portrait moods; a missing mood picture falls back to Neutral.
inline constexpr int MoodNeutral = 0;
inline constexpr int MoodHappy = 1;
inline constexpr int MoodAngry = 2;

enum class Ability { Overdrive, Deadeye, IronWall, FieldRepair, Barrage, Emp };

// Passive multipliers on the car, 1 = unchanged.
struct Perk
{
	float BoostRecharge; // Nitro refill speed.
	float GunHeat;       // Barrel heat per round.
	float MaxArmor;
	float SupplyGain;    // What a supply crate gives.
	float RocketReload;  // Reload time (lower = faster).
	float FlareRecharge; // Seconds per flare charge (lower = faster).
};

struct DriverDef
{
	const char* Key;
	const char* Name;
	const char* Callsign;
	const char* Blurb;
	const char* PerkText;
	const char* AbilityName;
	const char* AbilityText;
	Perk Passive;
	Ability Active;
	float AbilitySeconds;  // How long the effect lasts; 0 = instant.
	float AbilityCooldown; // From activation to ready again.
	int UnlockAfter;       // Campaign missions completed before this driver joins (0 = from the start).
};

// What each ability does while it runs (applied by AWarVehiclePawn).
struct AbilityTuning
{
	float OverdriveTorque = 1.35f;     // On top of the nitro, which is refilled and does not drain.
	float DeadeyeDamage = 1.5f;        // Machine-gun damage; the barrel also stays cold and tight.
	float IronWallDamageTaken = 0.25f; // Incoming damage multiplier.
	float IronWallArmorRestore = 0.3f; // Fraction of max armor restored on activation.
	float RepairHealth = 0.4f;         // Fractions of the maxima restored at once.
	float RepairArmor = 0.25f;
	float BarrageDamage = 1.5f;        // Rocket damage; the pod is also refilled on activation.
	float BarrageInterval = 0.45f;     // Rocket fire-interval multiplier.
	float EmpRadiusCm = 3000.f;        // Enemy engines inside this cut out...
	float EmpStunSeconds = 3.f;        // ...for this long, and missiles homing on the car burst.
};

inline const DriverDef& Get(Driver D)
{
	static const DriverDef Defs[DriverCount] = {
		{ "Rin", "RIN KUROSAWA", "COMET", "Street racer turned scout. Fast, loud, never brakes first.",
			"Nitro refills 30% faster", "OVERDRIVE", "Full nitro that does not drain, and more engine, for 4 s",
			{ 1.3f, 1.f, 1.f, 1.f, 1.f, 1.f }, Ability::Overdrive, 4.f, 30.f, 0 },
		{ "Layla", "LAYLA HADDAD", "FALCON", "Marksman from the harbour militia. Calm, exact, patient.",
			"Gun barrel heats 20% slower", "DEADEYE", "No spread, no heat and +50% gun damage for 5 s",
			{ 1.f, .8f, 1.f, 1.f, 1.f, 1.f }, Ability::Deadeye, 5.f, 32.f, 0 },
		{ "Kenji", "KENJI MORI", "BULWARK", "Former rescue driver. Puts his car between you and the fire.",
			"+15% max armor", "IRON WALL", "Takes a quarter damage for 6 s and restores 30% armor",
			{ 1.f, 1.f, 1.15f, 1.f, 1.f, 1.f }, Ability::IronWall, 6.f, 38.f, 0 },
		{ "Yuki", "YUKI SHIRANE", "FROST", "Engineer prodigy. Fixes the car while it is still moving.",
			"Supply crates give 50% more", "FIELD REPAIR", "Restores 40% health and 25% armor at once",
			{ 1.f, 1.f, 1.f, 1.5f, 1.f, 1.f }, Ability::FieldRepair, 0.f, 45.f, 2 },
		{ "Zara", "ZARA AL-NOUR", "EMBER", "Demolitions expert. Believes every problem has a blast radius.",
			"Rockets reload 25% faster", "BARRAGE", "Refills the pod; rockets fire twice as fast, +50% damage, 6 s",
			{ 1.f, 1.f, 1.f, 1.f, .75f, 1.f }, Ability::Barrage, 6.f, 40.f, 4 },
		{ "Omar", "OMAR NASSER", "GHOST", "Signals hacker. The Legion's radios go quiet when he smiles.",
			"Flares recharge 40% faster", "EMP PULSE", "Cuts enemy engines within 30 m for 3 s, bursts missiles",
			{ 1.f, 1.f, 1.f, 1.f, 1.f, .6f }, Ability::Emp, 0.f, 36.f, 6 },
	};
	const int Index = static_cast<int>(D);
	return Defs[Index >= 0 && Index < DriverCount ? Index : 0];
}

// Cycling on the driver-select screen (wraps both ways).
inline int NextIndex(int Index) { return ((Index + 1) % DriverCount + DriverCount) % DriverCount; }
inline int PrevIndex(int Index) { return ((Index - 1) % DriverCount + DriverCount) % DriverCount; }

// Portrait/text key and display name of anyone who talks on the radio.
inline const char* SpeakerKey(int Speaker)
{
	if (Speaker >= 0 && Speaker < DriverCount) return Get(static_cast<Driver>(Speaker)).Key;
	switch (Speaker)
	{
	case SpeakerHana: return "Hana";
	case SpeakerRaven: return "Raven";
	case SpeakerVarga: return "Varga";
	default: return "Hana";
	}
}

inline const char* SpeakerName(int Speaker)
{
	if (Speaker >= 0 && Speaker < DriverCount) return Get(static_cast<Driver>(Speaker)).Callsign;
	switch (Speaker)
	{
	case SpeakerHana: return "OVERWATCH";
	case SpeakerRaven: return "RAVEN";
	case SpeakerVarga: return "GENERAL VARGA";
	default: return "OVERWATCH";
	}
}

// The ability clock: ready -> active for AbilitySeconds -> cooling down -> ready.
struct AbilityState
{
	float ActiveLeft = 0.f;
	float CooldownLeft = 0.f;

	bool IsReady() const { return CooldownLeft <= 0.f; }
	bool IsActive() const { return ActiveLeft > 0.f; }

	// True if the ability fired (the caller then applies its instant effect).
	bool Activate(const DriverDef& D)
	{
		if (!IsReady()) return false;
		ActiveLeft = D.AbilitySeconds;
		CooldownLeft = D.AbilityCooldown;
		return true;
	}

	void Tick(float DeltaSeconds)
	{
		ActiveLeft = ActiveLeft > DeltaSeconds ? ActiveLeft - DeltaSeconds : 0.f;
		CooldownLeft = CooldownLeft > DeltaSeconds ? CooldownLeft - DeltaSeconds : 0.f;
	}

	// 0 just fired .. 1 ready, for the HUD meter.
	float ReadyFraction(const DriverDef& D) const
	{
		if (D.AbilityCooldown <= 0.f) return 1.f;
		const float F = 1.f - CooldownLeft / D.AbilityCooldown;
		return F < 0.f ? 0.f : (F > 1.f ? 1.f : F);
	}
};

// ---- Barks: one-liners the driver throws out in a fight.

enum class Bark { Deploy, Kill, Streak, Hurt, Ability, Boss, Victory, Defeat, Count };
inline constexpr int BarkCount = static_cast<int>(Bark::Count);

struct BarkLine
{
	const char* Key;
	const char* Text;
	int Mood;
};

inline const BarkLine& BarkFor(Driver D, Bark B)
{
	static const BarkLine Lines[DriverCount][BarkCount] = {
		{ // Rin
			{ "BarkRinDeploy", "Comet rolling. Try to keep up!", MoodHappy },
			{ "BarkRinKill", "Too slow!", MoodHappy },
			{ "BarkRinStreak", "Line them up, I'll knock them down!", MoodHappy },
			{ "BarkRinHurt", "Tch - she's taking hits! Hold together!", MoodAngry },
			{ "BarkRinAbility", "Overdrive! Eat my dust!", MoodHappy },
			{ "BarkRinBoss", "Big one. Good - I was getting bored.", MoodNeutral },
			{ "BarkRinVictory", "First across the line. Always.", MoodHappy },
			{ "BarkRinDefeat", "No... I never lose a race...", MoodAngry },
		},
		{ // Layla
			{ "BarkLaylaDeploy", "Falcon on station. Wind is steady.", MoodNeutral },
			{ "BarkLaylaKill", "Target down.", MoodNeutral },
			{ "BarkLaylaStreak", "One breath, one shot. Again.", MoodHappy },
			{ "BarkLaylaHurt", "Armor is failing. I need distance.", MoodAngry },
			{ "BarkLaylaAbility", "Deadeye. Nobody moves.", MoodNeutral },
			{ "BarkLaylaBoss", "Large target. Easier to hit.", MoodNeutral },
			{ "BarkLaylaVictory", "Clean work. The harbour sleeps tonight.", MoodHappy },
			{ "BarkLaylaDefeat", "I missed... the one that mattered.", MoodAngry },
		},
		{ // Kenji
			{ "BarkKenjiDeploy", "Bulwark here. Get behind me.", MoodNeutral },
			{ "BarkKenjiKill", "That one won't hurt anybody now.", MoodNeutral },
			{ "BarkKenjiStreak", "Keep coming. I have armor to spare!", MoodHappy },
			{ "BarkKenjiHurt", "Plates are cracking... not yet!", MoodAngry },
			{ "BarkKenjiAbility", "Iron Wall! You shall not pass!", MoodAngry },
			{ "BarkKenjiBoss", "Now that is a wall worth breaking.", MoodNeutral },
			{ "BarkKenjiVictory", "Everyone still breathing? Then it's a good day.", MoodHappy },
			{ "BarkKenjiDefeat", "Sorry... I couldn't hold the line.", MoodAngry },
		},
		{ // Yuki
			{ "BarkYukiDeploy", "Frost online. All systems green... mostly.", MoodHappy },
			{ "BarkYukiKill", "Their engine was badly tuned anyway.", MoodNeutral },
			{ "BarkYukiStreak", "Scrap, scrap and more scrap. Spare parts!", MoodHappy },
			{ "BarkYukiHurt", "That was my good gearbox! Stop that!", MoodAngry },
			{ "BarkYukiAbility", "Patching on the move. Don't try this at home.", MoodHappy },
			{ "BarkYukiBoss", "Whoever welded that thing has no taste.", MoodNeutral },
			{ "BarkYukiVictory", "Mission done and nothing fell off. New record!", MoodHappy },
			{ "BarkYukiDefeat", "I can fix... no. I can't fix this one.", MoodAngry },
		},
		{ // Zara
			{ "BarkZaraDeploy", "Ember ready. Somebody say 'boom'.", MoodHappy },
			{ "BarkZaraKill", "Beautiful fireball!", MoodHappy },
			{ "BarkZaraStreak", "More fuel for the fire!", MoodHappy },
			{ "BarkZaraHurt", "Careful! There are rockets in here!", MoodAngry },
			{ "BarkZaraAbility", "Barrage! Paint the sky!", MoodHappy },
			{ "BarkZaraBoss", "Oh, that will make a lovely crater.", MoodHappy },
			{ "BarkZaraVictory", "Smoke on the horizon. My favourite view.", MoodHappy },
			{ "BarkZaraDefeat", "Heh... at least it was a big one...", MoodAngry },
		},
		{ // Omar
			{ "BarkOmarDeploy", "Ghost in the net. They can't see us.", MoodNeutral },
			{ "BarkOmarKill", "Signal lost. Permanently.", MoodNeutral },
			{ "BarkOmarStreak", "Their whole channel is screaming.", MoodHappy },
			{ "BarkOmarHurt", "They found me. That's not supposed to happen.", MoodAngry },
			{ "BarkOmarAbility", "EMP out. Lights off, everyone.", MoodHappy },
			{ "BarkOmarBoss", "That thing has more firewalls than brains.", MoodNeutral },
			{ "BarkOmarVictory", "And I was never here.", MoodHappy },
			{ "BarkOmarDefeat", "Connection... lost...", MoodAngry },
		},
	};
	const int DI = static_cast<int>(D), BI = static_cast<int>(B);
	return Lines[DI >= 0 && DI < DriverCount ? DI : 0][BI >= 0 && BI < BarkCount ? BI : 0];
}

// Keeps the driver from repeating themselves: each kind of bark has its own minimum gap, and the
// chatty ones (kills, hits) also wait for a quiet moment after any line.
struct BarkClock
{
	float LastTime[BarkCount];
	float LastAny = -1000.f;

	BarkClock()
	{
		for (int i = 0; i < BarkCount; ++i) LastTime[i] = -1000.f;
	}

	static float MinGap(Bark B)
	{
		switch (B)
		{
		case Bark::Kill: return 14.f;
		case Bark::Streak: return 12.f;
		case Bark::Hurt: return 25.f;
		case Bark::Ability: return 4.f;
		default: return 0.f; // Once-a-mission moments are never held back.
		}
	}

	// True (and remembered) if the bark may play at time Now.
	bool Allow(Bark B, float Now)
	{
		const int Index = static_cast<int>(B);
		if (Index < 0 || Index >= BarkCount) return false;
		const float Gap = MinGap(B);
		if (Now - LastTime[Index] < Gap) return false;
		if (Gap > 0.f && Now - LastAny < 5.f) return false;
		LastTime[Index] = Now;
		LastAny = Now;
		return true;
	}
};
}
