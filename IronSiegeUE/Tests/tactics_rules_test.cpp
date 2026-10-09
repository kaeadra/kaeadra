// Offline checks for the enemy squad tactics (TacticsRules.h). No Unreal needed:
//   g++ -std=c++20 -Wall -Wextra -DPI=3.14159f -I../Source/IronSiege tactics_rules_test.cpp -o t && ./t
#include "TacticsRules.h"
#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { ++Failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); } } while (0)
static bool Near(float A, float B, float Eps = 1e-3f) { return std::fabs(A - B) < Eps; }

using namespace IronTactics;

static void TestIntercept()
{
	const Tuning T;
	// A target standing still (or crawling) is met where it is.
	const Vec2 Still = InterceptPoint({ 0.f, 0.f }, { 3000.f, 0.f }, { 100.f, 0.f }, 2000.f);
	CHECK(Near(Still.X, 3000.f) && Near(Still.Y, 0.f));
	// A target crossing ahead is met further along its path: 3000 cm at 2000 cm/s is 1.5 s ahead.
	const Vec2 Cross = InterceptPoint({ 0.f, 0.f }, { 3000.f, 0.f }, { 0.f, 1000.f }, 2000.f);
	CHECK(Near(Cross.X, 3000.f) && Near(Cross.Y, 1500.f));
	// A slow chaser still leads (it assumes the minimum closing speed), but never more than the cap.
	const Vec2 Far = InterceptPoint({ 0.f, 0.f }, { 20000.f, 0.f }, { 0.f, 2000.f }, 100.f);
	CHECK(Near(Far.Y, 2000.f * T.MaxLeadSeconds));
}

static void TestPincer()
{
	CHECK(LaneFor(0, 5) == 0.f && LaneFor(3, 1) == 0.f);
	CHECK(LaneFor(1, 3) == -1.f && LaneFor(2, 3) == 1.f && LaneFor(3, 5) == -0.5f && LaneFor(4, 5) == 0.5f);
	CHECK(LaneFor(6, 8) == -1.f); // The pattern repeats for a big squad.

	CHECK(Near(LaneRamp(500.f), 0.f) && Near(LaneRamp(9000.f), 1.f) && Near(LaneRamp(3300.f), 0.5f));

	// Coming from the west (target due +X): a left lane swings the goal to the target's left (-Y),
	// a right lane to +Y, and close in there is no offset at all.
	const Vec2 Me{ -8000.f, 0.f }, Aim{ 0.f, 0.f };
	const Vec2 Left = PincerGoal(Me, Aim, -1.f), Right = PincerGoal(Me, Aim, 1.f), Centre = PincerGoal(Me, Aim, 0.f);
	CHECK(Near(Left.Y, -1400.f) && Near(Right.Y, 1400.f) && Near(Centre.Y, 0.f));
	CHECK(Near(Left.X, 0.f) && Near(Right.X, 0.f));
	const Vec2 Close = PincerGoal({ -1000.f, 0.f }, Aim, 1.f);
	CHECK(Near(Close.X, 0.f) && Near(Close.Y, 0.f));
	CHECK(Near(RightOf({ 1.f, 0.f }).Y, 1.f) && Near(RightOf({ 0.f, 1.f }).X, -1.f));
}

static void TestSightsAndWeave()
{
	// Player at the origin looking down +X.
	CHECK(InSights({ 0.f, 0.f }, { 1.f, 0.f }, { 3000.f, 300.f }));
	CHECK(!InSights({ 0.f, 0.f }, { 1.f, 0.f }, { 3000.f, 2000.f }));
	CHECK(!InSights({ 0.f, 0.f }, { 1.f, 0.f }, { -3000.f, 0.f }));
	CHECK(ShouldWeave(true, 2000.f) && !ShouldWeave(false, 2000.f) && !ShouldWeave(true, 500.f) && !ShouldWeave(true, 9000.f));
	const Tuning T;
	CHECK(Near(WeaveSteer(0.f, 0.f), 0.f));
	CHECK(Near(WeaveSteer(T.WeavePeriod * 0.25f, 0.f), T.WeaveAmplitude));
	CHECK(Near(WeaveSteer(T.WeavePeriod * 0.75f, 0.f), -T.WeaveAmplitude));
	CHECK(!Near(WeaveSteer(0.f, 1.f), WeaveSteer(0.f, 2.f))); // Staggered cars swerve out of step.
}

static void TestLineBlocked()
{
	const Vec2 From{ 0.f, 0.f }, To{ 4000.f, 0.f };
	const Vec2 InTheWay[] = { { 2000.f, 150.f } };
	const Vec2 Aside[] = { { 2000.f, 600.f } };
	const Vec2 BesideTarget[] = { { 3900.f, 50.f } };
	const Vec2 Behind[] = { { -500.f, 0.f } };
	CHECK(LineBlocked(From, To, InTheWay, 1));
	CHECK(!LineBlocked(From, To, Aside, 1));
	CHECK(!LineBlocked(From, To, BesideTarget, 1)); // Hugging the target: the shot still lands.
	CHECK(!LineBlocked(From, To, Behind, 1));
	CHECK(!LineBlocked(From, From, InTheWay, 1));
	CHECK(!LineBlocked(From, To, nullptr, 0));
}

static void TestMorale()
{
	const Tuning T;
	Morale M;
	CHECK(M.Update(0.8f, 1000.f, 0.1f, true) == 0.f && M.State == Mode::Engage);
	CHECK(M.Update(0.2f, 1000.f, 0.1f, false) == 0.f && M.State == Mode::Engage); // Bosses never run.
	M.Update(0.2f, 1000.f, 0.1f, true);
	CHECK(M.State == Mode::Retreat && M.bUsed);
	M.Update(0.2f, 3000.f, 1.f, true);
	CHECK(M.State == Mode::Retreat);
	M.Update(0.2f, T.SafeDistance + 1.f, 0.1f, true);
	CHECK(M.State == Mode::Patch);
	// Patching restores PatchHealShare over PatchSeconds, then it is back in the fight.
	float Healed = 0.f;
	for (int i = 0; i < 100 && M.State == Mode::Patch; ++i) Healed += M.Update(0.3f, 6000.f, 0.1f, true);
	CHECK(Near(Healed, T.PatchHealShare, 1e-3f));
	CHECK(M.State == Mode::Engage);
	// Once only.
	CHECK(M.Update(0.1f, 1000.f, 0.1f, true) == 0.f && M.State == Mode::Engage);

	// Retreat runs out of time and patches where it is; the player closing in stops the patch.
	Morale R;
	R.Update(0.2f, 1000.f, 0.1f, true);
	for (int i = 0; i < 80; ++i) R.Update(0.2f, 2000.f, 0.1f, true);
	CHECK(R.State == Mode::Patch);
	CHECK(R.Update(0.2f, 1000.f, 0.1f, true) == 0.f && R.State == Mode::Engage);

	// A wreck does not retreat.
	Morale W;
	W.Update(0.f, 1000.f, 0.1f, true);
	CHECK(W.State == Mode::Engage && !W.bUsed);
}

int main()
{
	TestIntercept();
	TestPincer();
	TestSightsAndWeave();
	TestLineBlocked();
	TestMorale();
	if (Failures == 0) std::printf("tactics_rules_test: all checks passed\n");
	return Failures == 0 ? 0 : 1;
}
