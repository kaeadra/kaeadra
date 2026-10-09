#pragma once
// Engine-independent scoring: points per kill with a combo multiplier for quick successive kills,
// plus a bonus for clearing each wave. No Unreal includes on purpose: covered offline by
// Tests/score_rules_test.cpp.

namespace IronScore
{
struct ScoreTuning
{
	int EnemyKill = 100;
	int RaiderKill = 150;          // Fast raiders are harder to hit.
	int HunterKill = 200;
	int StormerKill = 180;         // Tesla specialists (late waves).
	int LancerKill = 220;          // Railgun specialists hang back behind the pack.          // Missile hunters hang back and have to be chased down.
	int BossKill = 1000;           // Juggernaut boss (every fifth wave).
	int WaveClearPerWave = 250;    // Clearing wave N pays N * this.
	float ComboWindowSeconds = 4.f; // A kill within this long of the last one raises the combo.
	int MaxCombo = 5;
};

struct ScoreState
{
	int Score = 0;
	int Combo = 0;
	float LastKillTime = -1000.f;

	// Records a kill worth BasePoints at time Now; returns the points awarded (base x combo).
	int AddKillWorth(int BasePoints, float Now, const ScoreTuning& T = ScoreTuning())
	{
		Combo = (Now - LastKillTime <= T.ComboWindowSeconds) ? (Combo < T.MaxCombo ? Combo + 1 : T.MaxCombo) : 1;
		LastKillTime = Now;
		const int Points = BasePoints * Combo;
		Score += Points;
		return Points;
	}

	// Records a regular (or raider) kill at time Now; returns the points it was worth.
	int AddKill(bool bRaider, float Now, const ScoreTuning& T = ScoreTuning())
	{
		return AddKillWorth(bRaider ? T.RaiderKill : T.EnemyKill, Now, T);
	}

	// Records clearing wave N; returns the bonus.
	int AddWaveClear(int Wave, const ScoreTuning& T = ScoreTuning())
	{
		const int Bonus = (Wave > 0 ? Wave : 0) * T.WaveClearPerWave;
		Score += Bonus;
		return Bonus;
	}

	// Combo still live (for the HUD) at time Now.
	int ActiveCombo(float Now, const ScoreTuning& T = ScoreTuning()) const
	{
		return (Now - LastKillTime <= T.ComboWindowSeconds) ? Combo : 0;
	}
};
}
