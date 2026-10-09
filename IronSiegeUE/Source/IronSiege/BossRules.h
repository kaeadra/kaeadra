#pragma once
// Engine-independent boss fights: every boss fights in three phases set by how much of its health
// and armour is left. Entering a new phase raises a brief shield, patches some armour, calls in an
// escort and makes the boss fiercer, and each boss has signature attacks that open up as it gets
// desperate: the heavy trucks slam the ground (a telegraphed shockwave), fire rocket barrages and,
// for the Iron Crown, an EMP that stalls the player's engine; Raven fires her railgun twice and mines
// her own wake. No Unreal includes on purpose: covered offline by Tests/boss_rules_test.cpp.

#include "CrewRules.h"

namespace IronBoss
{
enum class Kind { Juggernaut, Raven, Baron, Crown };
inline constexpr int KindCount = 4;
inline constexpr int PhaseCount = 3;

// From the campaign's boss ids (IronMissions::BossRaven = 1, BossBaron = 2, BossCrown = 3); anything
// else - the survival waves' boss - is the Juggernaut.
inline Kind KindForMissionBoss(int MissionBoss)
{
	switch (MissionBoss)
	{
	case 1: return Kind::Raven;
	case 2: return Kind::Baron;
	case 3: return Kind::Crown;
	default: return Kind::Juggernaut;
	}
}

// Phase from what is left of health + armour (the same fraction the boss bar shows).
inline int PhaseFor(float Fraction)
{
	if (Fraction > 2.f / 3.f) return 1;
	if (Fraction > 1.f / 3.f) return 2;
	return 3;
}

struct PhaseTuning
{
	float Aggression;      // Shorter pauses between bursts and missiles (1 = the boss's own pace).
	float AimSpread;       // Multiplier on the AI's aim cone (lower = tighter).
	int Escorts;           // Enemies called in on entering the phase.
	float ShieldSeconds;   // Damage cut right after entering the phase.
	float ArmorRestore;    // Share of max armour patched on entering the phase.
	float SlamInterval;    // Seconds between ground slams (0 = none in this phase).
	float BarrageInterval; // Seconds between rocket barrages (0 = none).
	int BarrageRockets;
	float RailFollowUp;    // Raven: seconds after a rail shot before a second one (0 = none).
	float MineInterval;    // Raven: seconds between mines dropped with the player behind her (0 = none).
	float EmpInterval;     // Iron Crown: seconds between EMP pulses (0 = none).
};

inline const PhaseTuning& Tuning(Kind K, int Phase)
{
	//                         aggr  aim   esc shield armor slam  barr  n  rail  mine  emp
	static const PhaseTuning Table[KindCount][PhaseCount] = {
		{ // Juggernaut: the survival waves' boss - slams once wounded, rockets when desperate.
			{ 1.f,  1.f,  0, 0.f,  0.f,  0.f,  0.f,  0, 0.f,  0.f,  0.f },
			{ 1.3f, .8f,  2, 3.f,  .3f,  9.f,  0.f,  0, 0.f,  0.f,  0.f },
			{ 1.6f, .6f,  2, 3.f,  .3f,  7.f,  8.f,  4, 0.f,  0.f,  0.f },
		},
		{ // Raven: a duellist - the second rail shot and the mines make her hard to chase.
			{ 1.f,  1.f,  0, 0.f,  0.f,  0.f,  0.f,  0, 0.f,  0.f,  0.f },
			{ 1.25f, .8f, 2, 2.5f, .4f,  0.f,  0.f,  0, 1.f,  6.f,  0.f },
			{ 1.5f, .6f,  2, 2.5f, .4f,  0.f, 10.f,  3, .7f,  4.f,  0.f },
		},
		{ // The Iron Baron: slams from the start, barrages from the second phase.
			{ 1.f,  1.f,  0, 0.f,  0.f, 10.f,  0.f,  0, 0.f,  0.f,  0.f },
			{ 1.3f, .8f,  2, 3.f,  .3f,  8.f,  9.f,  4, 0.f,  0.f,  0.f },
			{ 1.6f, .6f,  3, 3.f,  .3f,  6.f,  7.f,  5, 0.f,  0.f,  0.f },
		},
		{ // The Iron Crown: everything the Baron had, sooner, and an EMP once it is hurt.
			{ 1.1f, .9f,  0, 0.f,  0.f,  9.f, 10.f,  4, 0.f,  0.f,  0.f },
			{ 1.4f, .7f,  3, 3.5f, .35f, 7.f,  8.f,  5, 0.f,  0.f, 14.f },
			{ 1.7f, .55f, 3, 3.5f, .35f, 5.5f, 6.f,  6, 0.f,  0.f, 10.f },
		},
	};
	const int KI = static_cast<int>(K);
	const int PhaseIndex = Phase < 1 ? 0 : (Phase > PhaseCount ? PhaseCount - 1 : Phase - 1);
	return Table[KI >= 0 && KI < KindCount ? KI : 0][PhaseIndex];
}

struct AttackTuning
{
	float SlamRadiusCm = 1500.f;
	float SlamDamage = 55.f;         // At the centre; falls off to the edge.
	float SlamImpulse = 950.f;       // Throws cars in range (AIronExplosion's shove).
	float SlamWindUp = 1.4f;         // The warning before it lands: time to get clear.
	float SlamTriggerCm = 1200.f;    // Starts winding up when the player is this close.
	float EmpRadiusCm = 2200.f;
	float EmpWindUp = 1.6f;
	float EmpStunSeconds = 2.f;
	float ShieldDamageTaken = 0.3f;  // Incoming damage multiplier while the phase shield is up.
	float BarrageSpreadDeg = 14.f;   // Half-width of the fan.
	float BarrageDamage = 0.6f;      // Each barrage rocket against a normal one.
	float MineTriggerCm = 2600.f;    // Raven drops a mine with the player behind her and this close.
};

// A telegraphed attack: ready -> winding up (shown on the HUD; the strike lands where the boss is,
// so driving clear is the dodge) -> strike -> cooldown. The first one also waits a full interval,
// so a phase never opens with an instant hit.
struct WindUp
{
	float Cooldown = -1.f; // Negative until the attack is first armed.
	float Winding = -1.f;  // Seconds into the wind-up; negative when not winding.

	bool IsWinding() const { return Winding >= 0.f; }

	// True on the frame the strike lands.
	bool Tick(float DeltaSeconds, bool bTargetInRange, float Interval, float WindUpSeconds)
	{
		if (Interval <= 0.f)
		{
			Winding = -1.f;
			Cooldown = -1.f;
			return false;
		}
		if (Cooldown < 0.f) Cooldown = Interval;
		if (Winding >= 0.f)
		{
			Winding += DeltaSeconds;
			if (Winding < WindUpSeconds) return false;
			Winding = -1.f;
			Cooldown = Interval;
			return true;
		}
		// A hair of float left over from summing frame times counts as done (as Loadout does).
		Cooldown = Cooldown - DeltaSeconds > 1e-4f ? Cooldown - DeltaSeconds : 0.f;
		if (Cooldown <= 0.f && bTargetInRange) Winding = 0.f;
		return false;
	}

	// 0..1 through the wind-up, for the HUD and the warning ring.
	float Progress(float WindUpSeconds) const
	{
		if (Winding < 0.f || WindUpSeconds <= 0.f) return 0.f;
		const float P = Winding / WindUpSeconds;
		return P > 1.f ? 1.f : P;
	}
};

// An untelegraphed repeating attack (barrages, mines): fires when the interval is up and the moment
// is right, else holds at "due" until it is.
struct Repeat
{
	float Left = -1.f;

	bool Tick(float DeltaSeconds, float Interval, bool bAllowed)
	{
		if (Interval <= 0.f)
		{
			Left = -1.f;
			return false;
		}
		if (Left < 0.f) Left = Interval;
		Left = Left - DeltaSeconds > 1e-4f ? Left - DeltaSeconds : 0.f;
		if (Left > 0.f || !bAllowed) return false;
		Left = Interval;
		return true;
	}
};

// Which phase the boss is in and the shield it raised entering it. Phases only ever move forward:
// armour patched on entering a phase does not send the boss back to the one before.
struct PhaseTracker
{
	int Phase = 1;
	float ShieldLeft = 0.f;

	bool IsShielded() const { return ShieldLeft > 0.f; }

	// The phase just entered (one heavy hit can skip a phase), or 0 when nothing changed.
	int Update(float Fraction, Kind K)
	{
		const int Want = PhaseFor(Fraction);
		if (Want <= Phase) return 0;
		Phase = Want;
		ShieldLeft = Tuning(K, Phase).ShieldSeconds;
		return Phase;
	}

	void Tick(float DeltaSeconds) { ShieldLeft = ShieldLeft > DeltaSeconds ? ShieldLeft - DeltaSeconds : 0.f; }
};

// Escorts called in going from phase From to phase To: every phase passed brings its own.
inline int EscortsEntering(Kind K, int From, int To)
{
	int N = 0;
	for (int P = From + 1; P <= To && P <= PhaseCount; ++P) N += Tuning(K, P).Escorts;
	return N;
}

// The enemy kind of escort number Index (the game mode's kinds: 0 buggy, 1 raider, 2 missile
// hunter, 3 tesla stormer, 4 railgun lancer): each boss calls its own kind of help.
inline int EscortKind(Kind K, int Index)
{
	static const int Pattern[KindCount][3] = {
		{ 0, 1, 0 }, // Juggernaut: whatever is at hand.
		{ 4, 1, 1 }, // Raven: lancers and fast raiders.
		{ 0, 2, 0 }, // Baron: buggies and missile hunters.
		{ 3, 4, 2 }, // Crown: the Remnant's specialists.
	};
	const int KI = static_cast<int>(K);
	const int I = Index < 0 ? 0 : Index % 3;
	return Pattern[KI >= 0 && KI < KindCount ? KI : 0][I];
}

// Yaw, in degrees off the line to the target, of rocket Index out of Count in a barrage fan.
inline float FanYaw(int Index, int Count, float SpreadDeg)
{
	if (Count <= 1) return 0.f;
	return -SpreadDeg + 2.f * SpreadDeg * static_cast<float>(Index) / static_cast<float>(Count - 1);
}

// What the boss (or the commander, for the Juggernaut, which has no voice) says entering a phase.
struct PhaseLine
{
	int Speaker;
	int Mood;
	const char* Key;
	const char* Text;
};

inline const PhaseLine& LineFor(Kind K, int Phase)
{
	using namespace IronCrew;
	static const PhaseLine Lines[KindCount][2] = {
		{
			{ SpeakerHana, MoodAngry, "BossJugP2", "The Juggernaut is shrugging off hits and calling for help. Stay out of reach when it slams!" },
			{ SpeakerHana, MoodAngry, "BossJugP3", "It is coming apart - and firing everything it has. Finish it!" },
		},
		{
			{ SpeakerRaven, MoodAngry, "BossRavenP2", "Fine. No more warning shots - one rail for you, one for your shadow." },
			{ SpeakerRaven, MoodAngry, "BossRavenP3", "You want to chase me? Then drive through my mines." },
		},
		{
			{ SpeakerVarga, MoodAngry, "BossBaronP2", "You scratched the paint. Now the Baron's guns come out." },
			{ SpeakerVarga, MoodAngry, "BossBaronP3", "Enough! Every gun, every man - bury that car!" },
		},
		{
			{ SpeakerVarga, MoodAngry, "BossCrownP2", "The Crown has more than armour. Feel your engine die." },
			{ SpeakerVarga, MoodAngry, "BossCrownP3", "I will not fall twice! Burn it all!" },
		},
	};
	const int KI = static_cast<int>(K);
	const int PhaseIndex = Phase <= 2 ? 0 : 1;
	return Lines[KI >= 0 && KI < KindCount ? KI : 0][PhaseIndex];
}
}
