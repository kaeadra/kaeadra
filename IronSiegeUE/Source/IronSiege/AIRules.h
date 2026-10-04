#pragma once
// Engine-independent enemy driving decisions (speed management, stuck detection, burst fire).
// No Unreal includes on purpose: covered offline by Tests/ai_rules_test.cpp.

namespace IronAI
{
struct DriveTuning
{
	float StoppingDistance = 1200.f; // cm: aim to come to rest this far from the target.
	float CmPerKph = 30.f;           // Desired speed rises 1 km/h per this many cm beyond StoppingDistance.
	float MaxKph = 90.f;
	float BrakeMargin = 8.f;         // Only brake when this many km/h over the desired speed.
	float MinBrakeSpeed = 3.f;       // Never brake below this (brake at standstill would reverse).
	float BrakeGain = 30.f;
	float MinBrake = .3f;
	float ThrottleGain = 20.f;
	float MinThrottle = .35f;
};

inline float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

// Target speed for the current gap: fast from afar, zero inside the stopping distance.
inline float DesiredSpeedKph(float DistanceCm, const DriveTuning& T = DriveTuning())
{
	return Clamp((DistanceCm - T.StoppingDistance) / T.CmPerKph, 0.f, T.MaxKph);
}

// Throttle in [-1, 1] (negative = brake) to move SpeedKph toward DesiredKph. Never asks for
// braking while (nearly) stationary, so the car cannot slip into reverse by accident.
inline float ThrottleFor(float SpeedKph, float DesiredKph, const DriveTuning& T = DriveTuning())
{
	if (SpeedKph > DesiredKph + T.BrakeMargin && SpeedKph > T.MinBrakeSpeed)
	{
		return -Clamp((SpeedKph - DesiredKph) / T.BrakeGain, T.MinBrake, 1.f);
	}
	if (SpeedKph < DesiredKph)
	{
		return Clamp((DesiredKph - SpeedKph) / T.ThrottleGain, T.MinThrottle, 1.f);
	}
	return 0.f;
}

// Reports "stuck" once the car has wanted to move but stayed (nearly) stationary for
// StuckSeconds; then resets, so a recovery manoeuvre is triggered once per stall.
struct StuckDetector
{
	float StuckSeconds = 2.f;
	float WantsMoveKph = 10.f;
	float StationaryKph = 2.f;
	float Elapsed = 0.f;

	bool Update(float DesiredKph, float SpeedKph, float DeltaSeconds)
	{
		Elapsed = (DesiredKph > WantsMoveKph && SpeedKph < StationaryKph) ? Elapsed + DeltaSeconds : 0.f;
		if (Elapsed > StuckSeconds)
		{
			Elapsed = 0.f;
			return true;
		}
		return false;
	}
};

// Fire in bursts: OnSeconds firing, then OffSeconds pause, repeating.
struct BurstClock
{
	float Clock = 0.f;

	bool Update(float DeltaSeconds, float OnSeconds, float OffSeconds)
	{
		float Period = OnSeconds + OffSeconds;
		if (Period < .01f) Period = .01f;
		Clock += DeltaSeconds;
		while (Clock >= Period) Clock -= Period;
		return Clock < OnSeconds;
	}
};

// Three-point turn: when the way on lies behind the car in a street too narrow for a U-turn on full
// lock, brake to a stop, go forward on full lock toward the goal side until the kerb is close,
// reverse on opposite lock (which swings the nose further round) until the back is close, and repeat
// - up to MaxCycles - until the goal is within DoneDeg of straight ahead.
struct ThreePointTurnTuning
{
	float StartDeg = 110.f;     // Goal this far off the nose (behind-ish) starts a turn...
	float MaxStartKph = 30.f;   // ...if not going faster than this (at speed, sweep round instead).
	float DoneDeg = 45.f;       // Finished once the goal is within this of straight ahead.
	float StopKph = 4.f;        // Brake until below this before the first leg.
	float ForwardThrottle = 0.7f;   // A heavy Chaos car at 0.5 from a standstill barely creeps (2 m a leg).
	float ReverseThrottle = -0.5f;  // ...but backs up briskly: at -0.7 the reverse leg ran 9 m.
	float MaxLegSeconds = 2.f; // A leg ends on a blocked probe or after this long.
	float MaxLegKph = 12.f;    // Coast above this: a leg is a short shunt, and speed only means braking later.
	float MinLegSeconds = 0.35f;// ...but not before this (a probe hit at the start is the kerb it just left).
	int MaxCycles = 5;          // Safety cap on forward+reverse pairs before handing back to normal driving.
};

struct ThreePointTurn
{
	enum class Phase { Idle, Brake, Forward, Reverse };

	using Tuning = ThreePointTurnTuning;


	struct Output
	{
		bool bActive = false;
		float Throttle = 0.f; // -1..1, negative = brake/reverse.
		float Steer = 0.f;    // -1..1, positive = right.
	};

	Phase Stage = Phase::Idle;
	float Side = 1.f;   // Which way the nose is being swung (+ = right).
	float Timer = 0.f;
	int Cycles = 0;

	// GoalDeg: signed angle from the car's heading to where it wants to go (+ = to the right).
	// SpeedKph: signed, + forward / - backward. BlockedAhead / BlockedBehind: short probes off the
	// front and back bumpers.
	Output Tick(float Dt, float GoalDeg, float SpeedKph, bool BlockedAhead, bool BlockedBehind, const Tuning& T = Tuning())
	{
		Output O;
		const float Abs = GoalDeg < 0.f ? -GoalDeg : GoalDeg;
		const float Speed = SpeedKph < 0.f ? -SpeedKph : SpeedKph;
		if (Stage == Phase::Idle)
		{
			if (Abs < T.StartDeg || Speed > T.MaxStartKph) return O;
			Side = GoalDeg >= 0.f ? 1.f : -1.f;
			Stage = Speed > T.StopKph ? Phase::Brake : Phase::Forward;
			Timer = 0.f;
			Cycles = 0;
		}
		if (Abs < T.DoneDeg || Cycles >= T.MaxCycles)
		{
			Stage = Phase::Idle;
			return O;
		}
		// Stopped: the first leg starts this very tick.
		if (Stage == Phase::Brake && Speed <= T.StopKph) { Stage = Phase::Forward; Timer = 0.f; }
		Timer += Dt;
		O.bActive = true;
		switch (Stage)
		{
		case Phase::Brake:
			O.Throttle = -1.f; // Moving forward, so this brakes (it only reverses once stopped).
			break;
		case Phase::Forward:
			O.Steer = Side;
			if (SpeedKph < -T.StopKph)
			{
				// Still rolling back from the last leg: stop it hard first (throttle while reversing
				// brakes); the leg's clock only starts once the car is going the right way.
				O.Throttle = 1.f;
				Timer = 0.f;
				break;
			}
			O.Throttle = SpeedKph > T.MaxLegKph ? 0.f : T.ForwardThrottle;
			if (Timer > T.MaxLegSeconds || (Timer > T.MinLegSeconds && BlockedAhead)) { Stage = Phase::Reverse; Timer = 0.f; }
			break;
		case Phase::Reverse:
			// Rolling backwards, opposite lock swings the nose the same way the forward leg did.
			O.Steer = -Side;
			if (SpeedKph > T.StopKph)
			{
				O.Throttle = -1.f; // Full brake off the forward leg's momentum first.
				Timer = 0.f;
				break;
			}
			O.Throttle = SpeedKph < -T.MaxLegKph ? 0.f : T.ReverseThrottle;
			if (Timer > T.MaxLegSeconds || (Timer > T.MinLegSeconds && BlockedBehind)) { Stage = Phase::Forward; Timer = 0.f; ++Cycles; }
			break;
		default:
			break;
		}
		return O;
	}

	bool IsActive() const { return Stage != Phase::Idle; }
};
}
