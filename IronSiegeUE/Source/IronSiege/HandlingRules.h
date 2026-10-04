#pragma once
// Engine-independent steering feel for the Chaos cars: keyboard steering is all-or-nothing, so
// the request is eased in, scaled down overall and scaled down further with speed. Also the top
// speed limit and the driving assists (catching a spin, levelling the car in the air).
// No Unreal includes on purpose: covered offline by Tests/handling_rules_test.cpp.

#include <cmath>

namespace IronHandling
{
struct SteeringTuning
{
	float Scale = .85f;          // Overall steering strength.
	float HighSpeedScale = .55f; // Extra multiplier reached at HighSpeedKph and above.
	float HighSpeedKph = 120.f;
	float Response = 4.5f;       // Full lock per second when turning in (centring is twice as fast).
};

// Moves Current toward Target at Response per second - twice as fast when heading back toward
// centre or reversing direction, so releasing a key straightens the wheels promptly.
inline float EaseSteer(float Current, float Target, float DeltaSeconds, float Response)
{
	const float AbsC = Current < 0.f ? -Current : Current;
	const float AbsT = Target < 0.f ? -Target : Target;
	const bool bReturning = AbsT < AbsC || (Current > 0.f && Target < 0.f) || (Current < 0.f && Target > 0.f);
	const float Step = Response * (bReturning ? 2.f : 1.f) * DeltaSeconds;
	if (Target > Current) return Current + Step > Target ? Target : Current + Step;
	return Current - Step < Target ? Target : Current - Step;
}

// Steering multiplier for the current speed (1 at standstill, HighSpeedScale from HighSpeedKph).
inline float SpeedScale(float SpeedKph, const SteeringTuning& T = SteeringTuning())
{
	if (T.HighSpeedKph <= 0.f) return 1.f;
	float Alpha = SpeedKph / T.HighSpeedKph;
	Alpha = Alpha < 0.f ? 0.f : (Alpha > 1.f ? 1.f : Alpha);
	return 1.f + (T.HighSpeedScale - 1.f) * Alpha;
}

// Final steering input to send to the vehicle for an eased steer value at a given speed.
inline float SteeringOutput(float EasedSteer, float SpeedKph, const SteeringTuning& T = SteeringTuning())
{
	return EasedSteer * T.Scale * SpeedScale(SpeedKph, T);
}

// ---- Top speed: each class's top speed is a real limit (VehicleClassRules.h), not just a number on
// the select screen. The throttle fades out over the last FadeKph instead of cutting dead, so the
// car settles on the limit rather than surging against it.
inline float GovernThrottle(float Throttle, float SpeedKph, float TopSpeedKph, float FadeKph = 6.f)
{
	if (Throttle <= 0.f || TopSpeedKph <= 0.f) return Throttle;
	const float Room = FadeKph > 0.f ? (TopSpeedKph - SpeedKph) / FadeKph : (SpeedKph < TopSpeedKph ? 1.f : 0.f);
	return Throttle * (Room < 0.f ? 0.f : (Room > 1.f ? 1.f : Room));
}

// ---- Driving assists, as angular accelerations (deg/s^2) for the game to apply to the body.
struct AssistTuning
{
	float YawGain = 0.f;         // Per second: how hard a spin is caught (0 = off).
	float SpinMarginDeg = 25.f;  // Yaw rate (deg/s) beyond what the steering asks for that is left alone.
	float MinSpeedKph = 20.f;    // Below this the steering model means nothing: no yaw assist.
	float MaxYawAccel = 240.f;
	float AirLevelGain = 0.f;    // In the air: pull toward upright (deg/s^2 per unit of tilt; 0 = off).
	float AirDamping = 0.f;      // ...and damp the tumble (per second).
	float MaxAirAccel = 400.f;
	float RollGain = 0.f;        // On the ground: deg/s^2 per degree of lean past RollFreeDeg (0 = off).
	float RollFreeDeg = 5.f;     // Body roll the suspension is allowed before the assist leans back.
	float RollDamp = 4.f;        // Per second, against the roll rate.
	float MaxRollAccel = 600.f;
};

// The yaw rate (deg/s) a car turning with this steering would have if the tyres held: the bicycle
// model, v / wheelbase * tan(wheel angle). Signed like the steering; reversing flips it.
inline float ExpectedYawRate(float SignedSpeedKph, float SteerInput, float WheelbaseCm, float MaxSteerDeg)
{
	if (WheelbaseCm <= 1.f) return 0.f;
	const float Pi = 3.14159265f;
	const float SpeedCm = SignedSpeedKph / 0.036f;
	const float Angle = SteerInput * MaxSteerDeg * Pi / 180.f;
	return SpeedCm / WheelbaseCm * std::tan(Angle) * 180.f / Pi;
}

// Catching a spin: when the car rotates faster than the steering asks for (the rear stepping out,
// or a hit spinning it round), push back against the excess. A drift on the handbrake is left
// alone, and so is anything inside the margin - the car can still slide and be thrown about.
inline float YawAssist(float YawRateDeg, float ExpectedDeg, float SpeedKph, bool bHandbrake, const AssistTuning& T)
{
	if (bHandbrake || T.YawGain <= 0.f || (SpeedKph < 0.f ? -SpeedKph : SpeedKph) < T.MinSpeedKph) return 0.f;
	const float AbsRate = YawRateDeg < 0.f ? -YawRateDeg : YawRateDeg;
	const float AbsExpected = ExpectedDeg < 0.f ? -ExpectedDeg : ExpectedDeg;
	const float Excess = AbsRate - AbsExpected - T.SpinMarginDeg;
	if (Excess <= 0.f) return 0.f;
	float Accel = T.YawGain * Excess;
	if (Accel > T.MaxYawAccel) Accel = T.MaxYawAccel;
	return YawRateDeg > 0.f ? -Accel : Accel;
}

// Anti-roll: the turn-in and grip help corner harder than a tall body can take (the camper went
// onto its side at 50 km/h), so past a few degrees of lean the body is pushed back upright.
// LeanDeg is the lean about the car's forward axis, positive when a positive roll acceleration
// about that axis would right it; RollRateDeg the roll rate about the same axis.
inline float RollAssist(float LeanDeg, float RollRateDeg, const AssistTuning& T)
{
	if (T.RollGain <= 0.f) return 0.f;
	const float Abs = LeanDeg < 0.f ? -LeanDeg : LeanDeg;
	if (Abs <= T.RollFreeDeg) return 0.f;
	const float Past = LeanDeg > 0.f ? LeanDeg - T.RollFreeDeg : LeanDeg + T.RollFreeDeg;
	float A = T.RollGain * Past - T.RollDamp * RollRateDeg;
	if (A > T.MaxRollAccel) A = T.MaxRollAccel;
	if (A < -T.MaxRollAccel) A = -T.MaxRollAccel;
	return A;
}

struct Vec3
{
	float X, Y, Z;
};

// In the air: turn the car's up axis back toward the sky and damp its tumble, so a jump off a ramp
// or a rocket blast lands it on its wheels. Up is the car's up axis in world space, AngularDeg its
// angular velocity (deg/s); the result is a world-space angular acceleration.
inline Vec3 AirLevel(const Vec3& Up, const Vec3& AngularDeg, const AssistTuning& T)
{
	if (T.AirLevelGain <= 0.f && T.AirDamping <= 0.f) return { 0.f, 0.f, 0.f };
	// Up x WorldUp: the axis that rotates Up toward (0,0,1), its length sin(tilt). Past 90 degrees
	// that length shrinks again, so upside down use the full-strength unit axis instead.
	float AX = Up.Y, AY = -Up.X;
	const float Len = std::sqrt(AX * AX + AY * AY);
	if (Up.Z < 0.f)
	{
		// Exactly upside down there is no preferred axis: roll over the side.
		AX = Len > 1e-4f ? AX / Len : 1.f;
		AY = Len > 1e-4f ? AY / Len : 0.f;
	}
	Vec3 A{ AX * T.AirLevelGain - AngularDeg.X * T.AirDamping,
			AY * T.AirLevelGain - AngularDeg.Y * T.AirDamping,
			-AngularDeg.Z * T.AirDamping * 0.5f };
	const float Mag = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
	if (Mag > T.MaxAirAccel && Mag > 0.f)
	{
		const float S = T.MaxAirAccel / Mag;
		A = { A.X * S, A.Y * S, A.Z * S };
	}
	return A;
}

// ---- Arcade handling: the layer on top of the Chaos tyres that makes the cars turn where they are
// pointed, drift on the handbrake and change direction quickly. Measured before it existed
// (DebugHandlingTest): 15-35 m turning circles at 50 km/h, 3-8 s for a 180, no slide at all on the
// handbrake, 1.2-3 s to go from 30 km/h forward to reversing, and the box truck could not reverse.
// Signs: yaw rate and steering positive to the right; lateral speed positive moving right.
struct ArcadeTuning
{
	float CornerG = 1.5f;         // Cornering the car may hold on turn-in (0 = turn assist off).
	float TurnGain = 6.f;         // Per second: how quickly the yaw rate is brought to the target.
	float MaxTurnAccel = 450.f;   // deg/s^2.
	float TurnMinKph = 2.f;       // Turn assist fades in between these speeds.
	float TurnFullKph = 8.f;
	float PivotBoost = 1.7f;      // At a crawl the car may rotate this much faster than its steering
	float PivotFadeKph = 40.f;    // geometry alone (turning round in a street); gone by this speed.
	float PivotMinRate = 42.f;    // ...and on full lock it turns at least this fast (deg/s) once it is rolling.
	float GripGain = 3.5f;        // Per second: sideways speed taken out (0 = grip assist off).
	float GripMaxAccel = 650.f;   // cm/s^2.
	float DriftMinKph = 22.f;     // Handbrake + steering above this starts a drift (forwards or in reverse).
	float DriftKickDeg = 45.f;    // Yaw rate (deg/s) thrown in when the drift starts.
	float DriftYawRate = 95.f;    // deg/s the car rotates at on full lock while drifting.
	float HandbrakeYawRate = 230.f; // ...and with the handbrake held: a handbrake turn, or a J-turn in reverse
	                                // (fast: sideways, the tyres stop the car within a second).
	float HandbrakeMaxAccel = 900.f;
	float SpinFullKph = 40.f;     // The handbrake spins the car round at up to this speed; faster than
	float SpinNoneKph = 65.f;     // SpinNoneKph it only drifts (a 180 at motorway speed is a crash).
	float DriftGain = 5.f;
	float DriftMaxAccel = 520.f;  // deg/s^2.
	float DriftMaxSlipDeg = 55.f; // Past this the rotation is pulled back instead of spinning out.
	float DriftRearGrip = 0.45f;  // Rear tyre grip while drifting.
	float DriftGrip = 0.1f;       // Share of the grip assist left while sliding.
	float DriftThrust = 350.f;    // cm/s^2 along the nose at full throttle while drifting.
	float DriftExitSlipDeg = 8.f; // Handbrake released and slide below this for DriftExitSeconds: grip again.
	float DriftExitSeconds = 0.3f;
};

// Angle (deg) between where the car points and where it is going; negative when sliding left
// (the tail out to the right, as in a right-hand drift).
inline float SlipAngleDeg(float ForwardCmS, float LateralCmS)
{
	if (ForwardCmS * ForwardCmS + LateralCmS * LateralCmS < 1.f) return 0.f;
	return std::atan2(LateralCmS, ForwardCmS < 0.f ? -ForwardCmS : ForwardCmS) * 180.f / 3.14159265f;
}

// The yaw rate (deg/s) the car is asked to hold: the steering geometry's rate (boosted at a crawl,
// so turning round does not take a car park), but no more than the cornering CornerG can hold at
// this speed (v * yaw rate = lateral acceleration). Signed like the steering, flipped in reverse.
inline float TargetYawRate(float SignedKph, float Steer, float WheelbaseCm, float MaxSteerDeg, const ArcadeTuning& T)
{
	float Geometry = ExpectedYawRate(SignedKph, Steer, WheelbaseCm, MaxSteerDeg);
	const float AbsKph = SignedKph < 0.f ? -SignedKph : SignedKph;
	const float SpeedCm = AbsKph / 0.036f;
	if (T.CornerG <= 0.f || SpeedCm < 1.f) return Geometry;
	if (T.PivotFadeKph > 0.f && AbsKph < T.PivotFadeKph)
	{
		Geometry *= 1.f + (T.PivotBoost - 1.f) * (1.f - AbsKph / T.PivotFadeKph);
		// The floor: at walking pace the geometry gives next to nothing, and a three-point turn
		// stalls every time the car changes direction.
		const float AbsSteer = Steer < 0.f ? -Steer : Steer;
		const float Floor = T.PivotMinRate * AbsSteer * (AbsKph < 4.f ? AbsKph / 4.f : 1.f);
		const float AbsGeometry = Geometry < 0.f ? -Geometry : Geometry;
		if (AbsGeometry < Floor)
		{
			Geometry = ((Steer > 0.f) == (SignedKph > 0.f)) ? Floor : -Floor;
		}
	}
	const float Limit = T.CornerG * 981.f / SpeedCm * 180.f / 3.14159265f;
	return Geometry > Limit ? Limit : (Geometry < -Limit ? -Limit : Geometry);
}

// Turn-in: when the car rotates slower than asked (it ploughs on, understeering), push the rotation
// up to the target. Never slows a rotation down - that is the spin catch's job (YawAssist).
inline float TurnInAssist(float YawRateDeg, float TargetDeg, float SpeedKph, const ArcadeTuning& T)
{
	const float Abs = SpeedKph < 0.f ? -SpeedKph : SpeedKph;
	if (T.CornerG <= 0.f || T.TurnGain <= 0.f || Abs <= T.TurnMinKph || TargetDeg == 0.f) return 0.f;
	const bool bUnder = TargetDeg > 0.f ? YawRateDeg < TargetDeg : YawRateDeg > TargetDeg;
	if (!bUnder) return 0.f;
	float Fade = (Abs - T.TurnMinKph) / (T.TurnFullKph - T.TurnMinKph);
	Fade = Fade > 1.f ? 1.f : Fade;
	float A = T.TurnGain * (TargetDeg - YawRateDeg) * Fade;
	if (A > T.MaxTurnAccel) A = T.MaxTurnAccel;
	if (A < -T.MaxTurnAccel) A = -T.MaxTurnAccel;
	return A;
}

// Grip: take the sideways speed out, so the car goes where it points (cm/s^2, along the car's
// right). Mostly off while drifting, so the slide can live.
inline float GripAssist(float LateralCmS, float SpeedKph, bool bDrifting, const ArcadeTuning& T)
{
	if (T.GripGain <= 0.f || (SpeedKph < 0.f ? -SpeedKph : SpeedKph) < 8.f) return 0.f;
	const float Max = T.GripMaxAccel * (bDrifting ? T.DriftGrip : 1.f);
	float A = -T.GripGain * LateralCmS * (bDrifting ? T.DriftGrip : 1.f);
	if (A > Max) A = Max;
	if (A < -Max) A = -Max;
	return A;
}

struct DriftState
{
	bool bActive = false;
	float Time = 0.f;
	float Calm = 0.f; // How long the slide has been small with the handbrake off.
	// Which way the steering turns the nose, fixed when the slide starts: -1 if it began rolling
	// backward (a J-turn). Halfway round a handbrake turn the car is going backward relative to its
	// nose - re-reading the direction then would turn it back the way it came.
	float Direction = 1.f;
};

// One frame of the drift. Returns true on the frame a drift starts (the moment to kick the tail out).
inline bool UpdateDrift(DriftState& S, bool bHandbrake, float SpeedKph, float SlipDeg, float Steer, float Dt, const ArcadeTuning& T)
{
	const float Abs = SpeedKph < 0.f ? -SpeedKph : SpeedKph;
	if (!S.bActive)
	{
		if (bHandbrake && Abs >= T.DriftMinKph && (Steer > 0.2f || Steer < -0.2f) && T.DriftYawRate > 0.f)
		{
			S = DriftState();
			S.bActive = true;
			S.Direction = SpeedKph < 0.f ? -1.f : 1.f;
			return true;
		}
		return false;
	}
	S.Time += Dt;
	const float AbsSlip = SlipDeg < 0.f ? -SlipDeg : SlipDeg;
	S.Calm = (!bHandbrake && AbsSlip < T.DriftExitSlipDeg) ? S.Calm + Dt : 0.f;
	// Held, the handbrake turn carries on until the car has all but stopped.
	if (S.Calm >= T.DriftExitSeconds || Abs < (bHandbrake ? 1.5f : 15.f) || (!bHandbrake && S.Time > 8.f))
	{
		S.bActive = false;
	}
	return false;
}

// While drifting the steering sets the rotation: into the slide turns tighter, straight holds the
// angle, counter-steer straightens up and ends it. Past the slide limit the car is turned back
// toward where it is going instead of spinning out - unless the handbrake is held, which is the
// driver asking to spin round (a handbrake turn; rolling backward, a J-turn, where the same
// steering swings the nose the other way).
inline float DriftYaw(float YawRateDeg, float SlipDeg, float Steer, float Direction, bool bHandbrake, float TravelKph, const ArcadeTuning& T)
{
	// How much of a spin the handbrake is asking for: all of it at town speed, none of it flat out.
	const float AbsKph = TravelKph < 0.f ? -TravelKph : TravelKph;
	float Spin = !bHandbrake ? 0.f : (T.SpinNoneKph - AbsKph) / (T.SpinNoneKph - T.SpinFullKph);
	Spin = Spin < 0.f ? 0.f : (Spin > 1.f ? 1.f : Spin);
	float Target = Steer * (T.DriftYawRate + (T.HandbrakeYawRate - T.DriftYawRate) * Spin) * Direction;
	const float AbsSlip = SlipDeg < 0.f ? -SlipDeg : SlipDeg;
	// A rightward rotation deepens a slide that points left of the nose (negative slip).
	const bool bDeepening = (Target > 0.f && SlipDeg < 0.f) || (Target < 0.f && SlipDeg > 0.f);
	if (Spin < 0.5f && AbsSlip > T.DriftMaxSlipDeg && (bDeepening || Target == 0.f))
	{
		Target = SlipDeg < 0.f ? -25.f : 25.f;
	}
	const float Max = T.DriftMaxAccel + (T.HandbrakeMaxAccel - T.DriftMaxAccel) * Spin;
	float A = T.DriftGain * (Target - YawRateDeg);
	if (A > Max) A = Max;
	if (A < -Max) A = -Max;
	return A;
}

// Reverse: full reverse pedal until the car rolls back at the class's reverse limit, faded over the
// last few km/h. Moving forward the pedal is the brake and is never reduced.
inline float ReverseInput(float Input, float SignedKph, float ReverseMaxKph)
{
	if (Input <= 0.f || SignedKph > -1.f || ReverseMaxKph <= 0.f) return Input;
	const float Room = (ReverseMaxKph + SignedKph) / 5.f;
	return Input * (Room < 0.f ? 0.f : (Room > 1.f ? 1.f : Room));
}
}
