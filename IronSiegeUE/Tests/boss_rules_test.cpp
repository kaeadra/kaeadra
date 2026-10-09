// Offline checks for the boss phases and signature attacks (BossRules.h). No Unreal needed:
//   g++ -std=c++20 -Wall -Wextra -I../Source/IronSiege boss_rules_test.cpp -o boss_rules_test && ./boss_rules_test
// Also build it with -DPI=3.14159f: Unreal defines PI as a macro, so no rules header may use the name.
#include "BossRules.h"
#include "MissionRules.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { ++Failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); } } while (0)
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-4f; }

using namespace IronBoss;

static void TestPhases()
{
	CHECK(PhaseFor(1.f) == 1 && PhaseFor(0.67f) == 1 && PhaseFor(0.66f) == 2 && PhaseFor(0.34f) == 2);
	CHECK(PhaseFor(0.33f) == 3 && PhaseFor(0.f) == 3 && PhaseFor(-1.f) == 3);

	// The campaign's boss ids map onto the right kinds.
	CHECK(KindForMissionBoss(IronMissions::BossNone) == Kind::Juggernaut);
	CHECK(KindForMissionBoss(IronMissions::BossRaven) == Kind::Raven);
	CHECK(KindForMissionBoss(IronMissions::BossBaron) == Kind::Baron);
	CHECK(KindForMissionBoss(IronMissions::BossCrown) == Kind::Crown);
	CHECK(KindForMissionBoss(42) == Kind::Juggernaut);

	PhaseTracker T;
	CHECK(T.Update(0.9f, Kind::Baron) == 0 && T.Phase == 1 && !T.IsShielded());
	CHECK(T.Update(0.5f, Kind::Baron) == 2 && T.IsShielded() && Near(T.ShieldLeft, Tuning(Kind::Baron, 2).ShieldSeconds));
	CHECK(T.Update(0.5f, Kind::Baron) == 0);
	CHECK(T.Update(0.9f, Kind::Baron) == 0 && T.Phase == 2); // Patched armour does not undo a phase.
	T.Tick(10.f);
	CHECK(!T.IsShielded());
	CHECK(T.Update(0.1f, Kind::Baron) == 3);

	PhaseTracker Skip;
	CHECK(Skip.Update(0.2f, Kind::Crown) == 3); // One huge hit skips phase 2...
	CHECK(EscortsEntering(Kind::Crown, 1, 3) == Tuning(Kind::Crown, 2).Escorts + Tuning(Kind::Crown, 3).Escorts); // ...but not its escort.
	CHECK(EscortsEntering(Kind::Raven, 2, 2) == 0 && EscortsEntering(Kind::Raven, 3, 9) == 0);
}

static void TestTuning()
{
	for (int k = 0; k < KindCount; ++k)
	{
		const Kind K = static_cast<Kind>(k);
		for (int p = 1; p <= PhaseCount; ++p)
		{
			const PhaseTuning& T = Tuning(K, p);
			CHECK(T.Aggression >= 1.f && T.AimSpread > 0.f && T.AimSpread <= 1.f);
			CHECK(T.Escorts >= 0 && T.Escorts <= 3);
			CHECK(T.ArmorRestore >= 0.f && T.ArmorRestore < 1.f);
			CHECK(T.BarrageInterval <= 0.f || T.BarrageRockets >= 2);
			if (p > 1)
			{
				// Every phase is at least as fierce as the one before.
				const PhaseTuning& Before = Tuning(K, p - 1);
				CHECK(T.Aggression >= Before.Aggression && T.AimSpread <= Before.AimSpread);
				CHECK(T.ShieldSeconds > 0.f && T.Escorts > 0);
			}
			else
			{
				CHECK(T.ShieldSeconds == 0.f && T.Escorts == 0); // Nothing to "enter" in phase 1.
			}
			for (int i = 0; i < 6; ++i)
			{
				const int E = EscortKind(K, i);
				CHECK(E >= 0 && E <= 4);
			}
		}
		// The second and third phase each have a line, keyed uniquely, said by someone with a face.
		for (int p = 2; p <= PhaseCount; ++p)
		{
			const PhaseLine& L = LineFor(K, p);
			CHECK(L.Key && L.Text && std::strlen(L.Text) > 0);
			CHECK(L.Speaker >= 0 && L.Speaker < IronCrew::SpeakerCount);
		}
	}
	// Signature attacks: only Raven duels with rails and mines, only the Crown has the EMP, and only
	// the heavy trucks slam.
	CHECK(Tuning(Kind::Raven, 3).RailFollowUp > 0.f && Tuning(Kind::Raven, 3).MineInterval > 0.f && Tuning(Kind::Raven, 3).SlamInterval == 0.f);
	CHECK(Tuning(Kind::Crown, 3).EmpInterval > 0.f && Tuning(Kind::Baron, 3).EmpInterval == 0.f);
	CHECK(Tuning(Kind::Juggernaut, 1).SlamInterval == 0.f && Tuning(Kind::Juggernaut, 2).SlamInterval > 0.f);
	CHECK(Tuning(Kind::Baron, 1).SlamInterval > 0.f);
	CHECK(Tuning(Kind::Raven, 0).Aggression == Tuning(Kind::Raven, 1).Aggression);  // Clamped phase.
	CHECK(Tuning(Kind::Raven, 99).Aggression == Tuning(Kind::Raven, 3).Aggression);

	std::set<std::string> Keys;
	for (int k = 0; k < KindCount; ++k)
	{
		Keys.insert(LineFor(static_cast<Kind>(k), 2).Key);
		Keys.insert(LineFor(static_cast<Kind>(k), 3).Key);
	}
	CHECK(Keys.size() == static_cast<size_t>(KindCount * 2));

	const AttackTuning A;
	CHECK(A.SlamTriggerCm < A.SlamRadiusCm); // Winding up only when the player is well inside the blast.
	CHECK(A.ShieldDamageTaken > 0.f && A.ShieldDamageTaken < 1.f);
}

static void TestAttackClocks()
{
	// A slam waits a full interval, winds up, lands, then cools down again.
	WindUp W;
	CHECK(!W.Tick(0.1f, true, 5.f, 1.f) && !W.IsWinding());
	for (int i = 0; i < 49; ++i) CHECK(!W.Tick(0.1f, true, 5.f, 1.f));
	CHECK(W.IsWinding());
	CHECK(Near(W.Progress(1.f), 0.f));
	bool bLanded = false;
	for (int i = 0; i < 9; ++i) bLanded |= W.Tick(0.1f, false, 5.f, 1.f); // Out of range: still lands.
	CHECK(!bLanded && W.IsWinding() && W.Progress(1.f) > 0.8f);
	CHECK(W.Tick(0.11f, false, 5.f, 1.f));
	CHECK(!W.IsWinding() && Near(W.Cooldown, 5.f));

	// Ready but the player is far away: holds, then winds up the moment they come in range.
	WindUp Wait;
	for (int i = 0; i < 100; ++i) Wait.Tick(0.1f, false, 2.f, 1.f);
	CHECK(!Wait.IsWinding());
	Wait.Tick(0.1f, true, 2.f, 1.f);
	CHECK(Wait.IsWinding());

	// A phase without the attack switches it off, even mid wind-up.
	Wait.Tick(0.1f, true, 0.f, 1.f);
	CHECK(!Wait.IsWinding());

	Repeat R;
	CHECK(!R.Tick(1.f, 3.f, true) && !R.Tick(1.f, 3.f, true));
	CHECK(!R.Tick(1.f, 3.f, false)); // Due, but not the moment.
	CHECK(R.Tick(0.1f, 3.f, true));
	CHECK(!R.Tick(0.1f, 3.f, true));
	CHECK(!R.Tick(1.f, 0.f, true));

	CHECK(Near(FanYaw(0, 1, 14.f), 0.f));
	CHECK(Near(FanYaw(0, 5, 14.f), -14.f) && Near(FanYaw(2, 5, 14.f), 0.f) && Near(FanYaw(4, 5, 14.f), 14.f));
}

int main()
{
	TestPhases();
	TestTuning();
	TestAttackClocks();
	if (Failures == 0) std::printf("boss_rules_test: all checks passed\n");
	return Failures == 0 ? 0 : 1;
}
