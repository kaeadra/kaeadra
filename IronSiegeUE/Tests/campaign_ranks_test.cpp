// Offline checks for the twelve-mission campaign data (MissionRules.h) and the driver ranks
// (RankRules.h). No Unreal needed:
//   g++ -std=c++20 -Wall -Wextra -I../Source/IronSiege campaign_ranks_test.cpp -o campaign_ranks_test && ./campaign_ranks_test
#include "MissionRules.h"
#include "RankRules.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { ++Failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); } } while (0)
static bool Near(float A, float B) { return std::fabs(A - B) < 1e-4f; }

static void TestMissions()
{
	using namespace IronMissions;
	std::set<std::string> Keys;
	auto Key = [&](const char* K, const char* Text)
	{
		CHECK(K != nullptr && Text != nullptr && std::strlen(Text) > 0);
		if (K) CHECK(Keys.insert(K).second); // Every text key is unique.
	};
	const char* Maps[] = { "Desert", "Coast", "CityRuins", "Arctic" };
	for (int i = 0; i < Count; ++i)
	{
		const Mission& M = Get(i);
		CHECK(M.Key && M.Name && M.Map);
		bool bKnownMap = false;
		for (const char* Map : Maps) bKnownMap |= std::strcmp(Map, M.Map) == 0;
		CHECK(bKnownMap);
		CHECK(M.NumStages >= 1 && M.NumStages <= MaxStages);
		CHECK(M.ParSeconds > 60.f);
		CHECK(M.NumOutro >= 1 && M.NumOutro <= MaxLines);
		for (const Radio& R : M.Brief) Key(R.Key, R.Text);
		for (int o = 0; o < M.NumOutro; ++o) Key(M.Outro[o].Key, M.Outro[o].Text);
		for (int s = 0; s < M.NumStages; ++s)
		{
			const Stage& S = M.Stages[s];
			Key(S.TextKey, S.Text);
			for (int l = 0; l < S.NumLines; ++l)
			{
				Key(S.Lines[l].Key, S.Lines[l].Text);
				CHECK(S.Lines[l].Speaker >= 0 && S.Lines[l].Speaker < IronCrew::SpeakerCount);
			}
			for (int p = 0; p < S.NumSpots; ++p)
			{
				CHECK(std::fabs(S.Spots[p].X) <= 1.f && std::fabs(S.Spots[p].Y) <= 1.f);
			}
			CHECK(S.MaxAlive > 0 && S.MaxAlive <= 8);
			switch (S.Kind)
			{
			case Objective::Reach: CHECK(S.NumSpots >= 1); break;
			case Objective::Capture:
			case Objective::DestroyTargets: CHECK(S.NumSpots == S.Count); break;
			case Objective::Defend: CHECK(S.NumSpots >= 1 && S.Seconds > 0.f && S.Structure == StructRelay); break;
			case Objective::Survive: CHECK(S.Seconds > 0.f); break;
			case Objective::Escort:
			case Objective::Intercept: CHECK(S.NumSpots >= 2 && S.Need >= 1 && S.Need <= S.Count); break;
			case Objective::Boss: CHECK(S.Boss > BossNone && S.Boss < BossCount); break;
			case Objective::Eliminate: CHECK(S.Count > 0 && S.Opening.Total() + S.Reinforce.Total() > 0); break;
			}
			// A stage that waits on enemies must be able to field them.
			if (S.Kind == Objective::Eliminate) CHECK(S.Reinforce.Total() > 0 && S.ReinforceEvery > 0.f);
		}
	}
	// Mission names are keyed by the mission key + "Name": no clash with the other keys.
	for (int i = 0; i < Count; ++i) CHECK(Keys.insert(std::string(Get(i).Key) + "Name").second);

	CHECK(Count == 12 && ActOneCount == 8);
	CHECK(ActOf(0) == 1 && ActOf(7) == 1 && ActOf(8) == 2 && ActOf(11) == 2);
	CHECK(IsActFinale(7) && IsActFinale(11) && !IsActFinale(8) && !IsActFinale(0));
	CHECK(GetBoss(BossCrown).Durability > GetBoss(BossBaron).Durability);
	CHECK(GetBoss(99).Durability == 1.f);

	// Loadouts never pass an upgrade's max level and never shrink as the campaign goes on.
	for (int Stars = 0; Stars <= Count * 3; Stars += 4)
	{
		IronUpgrades::Loadout Before;
		for (int i = 0; i < Count; ++i)
		{
			const IronUpgrades::Loadout L = LoadoutFor(i, Stars);
			for (int u = 0; u < IronUpgrades::Count; ++u)
			{
				CHECK(L.Levels[u] <= IronUpgrades::Get(static_cast<IronUpgrades::Upgrade>(u)).MaxLevel);
				CHECK(L.Levels[u] >= Before.Levels[u]);
			}
			Before = L;
		}
	}
	CHECK(LoadoutFor(8, 0).Level(IronUpgrades::Upgrade::Railgun) == 2);
	CHECK(LoadoutFor(10, 0).Level(IronUpgrades::Upgrade::Armor) == 3);
	CHECK(LoadoutFor(6, 24).Level(IronUpgrades::Upgrade::Engine) == 2);

	// Progress: Act II opens only after Act I's finale.
	Progress P;
	for (int i = 0; i < 7; ++i) P.Record(i, 2);
	CHECK(P.IsUnlocked(7) && !P.IsUnlocked(8));
	P.Record(7, 1);
	CHECK(P.IsUnlocked(8) && P.NextMission() == 8);
}

static void TestRanks()
{
	using namespace IronRanks;
	CHECK(RankFor(0) == 1 && RankFor(299) == 1 && RankFor(300) == 2 && RankFor(2999) == 4 && RankFor(3000) == 5 && RankFor(999999) == 5);
	CHECK(Near(ProgressToNext(0), 0.f) && Near(ProgressToNext(550), 0.5f) && Near(ProgressToNext(5000), 1.f));
	CHECK(MatchXp(5, 0, true, true, 3) == 50 + 120 + 120);
	CHECK(MatchXp(2, 0, true, false, 3) == 20 + 30);
	CHECK(MatchXp(10, 6, false, false, 0) == 100 + 150);
	CHECK(MatchXp(-3, -1, false, false, 0) == 0);
	CHECK(Near(ScalePerkValue(1.3f, 1), 1.3f) && Near(ScalePerkValue(1.3f, 5), 1.48f));
	CHECK(Near(ScalePerkValue(0.8f, 5), 0.68f) && Near(ScalePerkValue(1.f, 5), 1.f));
	CHECK(ScalePerkValue(0.1f, 5) >= 0.2f);
	CHECK(Near(CooldownScale(1), 1.f) && Near(CooldownScale(5), 0.8f) && Near(CooldownScale(9), 0.8f));
	const IronCrew::Perk Rin = RankedPerk(IronCrew::Get(IronCrew::Driver::Rin).Passive, 3);
	CHECK(Near(Rin.BoostRecharge, 1.39f) && Near(Rin.GunHeat, 1.f));

	Roster R;
	CHECK(R.Rank(0) == 1 && R.Rank(-1) == 1 && R.Rank(IronCrew::DriverCount) == 1);
	CHECK(R.Award(0, 299) == 0);
	CHECK(R.Award(0, 1) == 2);
	CHECK(R.Award(0, 10000) == 5);
	CHECK(R.Award(0, 10) == 0 && R.Rank(0) == 5);
	CHECK(R.Award(9, 100) == 0 && R.Award(1, 0) == 0);

	// A ranked driver's ability comes back sooner, and the HUD meter agrees.
	const IronCrew::DriverDef& Layla = IronCrew::Get(IronCrew::Driver::Layla);
	IronCrew::AbilityState A;
	CHECK(A.Activate(Layla, CooldownScale(5)));
	CHECK(Near(A.CooldownLeft, Layla.AbilityCooldown * 0.8f));
	A.Tick(Layla.AbilityCooldown * 0.4f);
	CHECK(Near(A.ReadyFraction(Layla, CooldownScale(5)), 0.5f));
	IronCrew::AbilityState Plain;
	CHECK(Plain.Activate(Layla) && Near(Plain.CooldownLeft, Layla.AbilityCooldown));
}

int main()
{
	TestMissions();
	TestRanks();
	if (Failures == 0) std::printf("campaign_ranks_test: all checks passed\n");
	return Failures == 0 ? 0 : 1;
}
