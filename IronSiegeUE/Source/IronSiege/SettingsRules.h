#pragma once
// Engine-independent mappings behind the settings menu: what a slider position or option index
// actually means in game terms. No Unreal includes on purpose: covered offline by
// Tests/settings_rules_test.cpp.

namespace IronSettings
{
inline float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
inline float Lerp(float A, float B, float T) { return A + (B - A) * T; }

// ---- Difficulty (0 Easy, 1 Normal, 2 Hard): how hard enemy fire hits and how accurate it is.
inline constexpr int DifficultyCount = 3;
inline const char* DifficultyName(int D)
{
	static const char* Names[DifficultyCount] = { "Easy", "Normal", "Hard" };
	return Names[D < 0 ? 0 : (D >= DifficultyCount ? DifficultyCount - 1 : D)];
}
inline float EnemyDamageMultiplier(int D)
{
	static const float V[DifficultyCount] = { 0.6f, 1.f, 1.5f };
	return V[D < 0 ? 0 : (D >= DifficultyCount ? DifficultyCount - 1 : D)];
}
inline float EnemyAimSpreadDeg(int D)
{
	static const float V[DifficultyCount] = { 7.f, 5.f, 3.5f };
	return V[D < 0 ? 0 : (D >= DifficultyCount ? DifficultyCount - 1 : D)];
}

// ---- Frame-rate cap options (0 = unlimited).
inline constexpr int FrameCapCount = 7;
inline float FrameCapValue(int Index)
{
	static const float V[FrameCapCount] = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 0.f };
	return V[Index < 0 ? 0 : (Index >= FrameCapCount ? FrameCapCount - 1 : Index)];
}
inline int FrameCapIndex(float Limit)
{
	int Best = FrameCapCount - 1;
	for (int i = 0; i < FrameCapCount; ++i)
	{
		if (FrameCapValue(i) == Limit) return i;
		if (Limit > 0.f && FrameCapValue(i) > 0.f && FrameCapValue(i) < Limit) Best = i;
	}
	return Limit <= 0.f ? FrameCapCount - 1 : Best;
}

// ---- Camera.
inline float ClampFieldOfView(float Fov) { return Clamp(Fov, 70.f, 110.f); }
// Smoothing 0 = stiff camera (fast lag catch-up), 1 = floaty (slow catch-up).
inline float CameraLagSpeed(float Smoothing) { return Lerp(20.f, 3.f, Clamp(Smoothing, 0.f, 1.f)); }
inline float CameraRotationLagSpeed(float Smoothing) { return Lerp(16.f, 2.5f, Clamp(Smoothing, 0.f, 1.f)); }

// ---- Controls.
// Steering smoothing 0 = snappy (fast ease-in), 1 = gentle; returns full-lock-per-second.
inline float SteeringResponse(float Smoothing) { return Lerp(9.f, 2.5f, Clamp(Smoothing, 0.f, 1.f)); }

// ---- Video: brightness slider (0.5..1.5, 1 = default) to display gamma (2.2 default).
inline float DisplayGamma(float Brightness) { return 2.2f * Clamp(Brightness, 0.5f, 1.5f); }

// ---- Microphone: peak sample level (0..1, after gain) to meter fill with a gentle curve so
// normal speech reads mid-scale.
inline float MicMeter(float Peak, float Gain)
{
	const float Level = Clamp(Peak * Gain, 0.f, 1.f);
	return Clamp(1.f - (1.f - Level) * (1.f - Level) * (1.f - Level), 0.f, 1.f);
}

// ---- Gamepad.
// Analog sticks rest slightly off centre, so anything inside the dead zone reads as zero and the
// rest is rescaled to 0..1 - otherwise the car creeps or steers on its own.
inline float ApplyDeadzone(float Raw, float Deadzone)
{
	const float Dz = Clamp(Deadzone, 0.f, 0.9f);
	const float Abs = Raw < 0.f ? -Raw : Raw;
	if (Abs <= Dz) return 0.f;
	const float Scaled = (Abs - Dz) / (1.f - Dz);
	return Raw < 0.f ? -Scaled : Scaled;
}
// Force-feedback amplitude for an event of a given strength, scaled by the player's slider
// (0 = vibration off).
inline float RumbleAmplitude(float EventStrength, float UserScale)
{
	return Clamp(EventStrength, 0.f, 1.f) * Clamp(UserScale, 0.f, 1.f);
}

// ---- Accessibility.
inline constexpr int HudScaleCount = 4;
inline float HudScale(int Index)
{
	static const float V[HudScaleCount] = { 0.85f, 1.f, 1.2f, 1.45f };
	return V[Index < 0 ? 0 : (Index >= HudScaleCount ? HudScaleCount - 1 : Index)];
}

// Colour-blind palettes. The game codes information by colour in three places - enemies (red),
// health/repair (green) and warnings/ammo (amber) - and red/green is exactly the pair that is hard
// to tell apart, so each mode keeps the meanings but moves them to distinguishable hues:
// protanopia/deuteranopia (red-green) -> blue vs orange, tritanopia (blue-yellow) -> magenta vs cyan.
inline constexpr int ColorModeCount = 4;
enum EColorRole { ColorEnemy = 0, ColorFriendly = 1, ColorWarning = 2, ColorRoleCount = 3 };
struct Rgb { float R, G, B; };
inline Rgb RoleColor(int Mode, int Role)
{
	static const Rgb Table[ColorModeCount][ColorRoleCount] = {
		// Off: the stock palette.
		{ { 1.f, 0.18f, 0.12f }, { 0.25f, 0.95f, 0.35f }, { 1.f, 0.72f, 0.15f } },
		// Protanopia.
		{ { 1.f, 0.45f, 0.f },   { 0.25f, 0.6f, 1.f },    { 1.f, 0.95f, 0.35f } },
		// Deuteranopia.
		{ { 1.f, 0.4f, 0.05f },  { 0.3f, 0.65f, 1.f },    { 0.98f, 0.92f, 0.4f } },
		// Tritanopia.
		{ { 1.f, 0.2f, 0.65f },  { 0.f, 0.85f, 0.9f },    { 1.f, 0.6f, 0.75f } },
	};
	const int M = Mode < 0 ? 0 : (Mode >= ColorModeCount ? ColorModeCount - 1 : Mode);
	const int R = Role < 0 ? 0 : (Role >= ColorRoleCount ? ColorRoleCount - 1 : Role);
	return Table[M][R];
}
// Screen shake and camera lag both make motion sickness worse, so "reduce motion" stiffens the
// camera and scales shake down to a fraction.
inline float ShakeScale(bool bReduceMotion) { return bReduceMotion ? 0.25f : 1.f; }

// ---- Driving feel (0 arcade, 1 balanced, 2 simulation).
inline constexpr int DrivingStyleCount = 3;
struct DrivingFeel
{
	float SteeringScale;      // Overall steering strength (HandlingRules::SteeringTuning::Scale).
	float HighSpeedScale;     // Steering left at high speed.
	float Response;           // Steering ease-in rate.
	float FrictionMultiplier; // Tyre grip: high = sticks to the road, low = slides.
	float BrakeMultiplier;    // Brake torque.
	float HandbrakeMultiplier;// Handbrake torque (drifting).
	float SpinAssist;         // IronHandling::AssistTuning::YawGain - catching a spin (0 = off).
	float AirControl;         // IronHandling::AssistTuning::AirLevelGain - landing jumps on the wheels.
	float CornerG;            // IronHandling::ArcadeTuning::CornerG - how hard the car may corner on turn-in (0 = off).
	float GripAssist;         // Scale on the arcade grip assist (0 = off).
	float DriftAssist;        // Scale on the handbrake drift (0 = the tyres alone).
};
inline DrivingFeel DrivingStyleFeel(int Style)
{
	static const DrivingFeel V[DrivingStyleCount] = {
		// Arcade: forgiving - grippy, strong brakes, quick steering that stays usable flat out, spins
		// caught early and jumps levelled out.
		{ 1.f,   0.75f, 10.f, 1.35f, 1.3f, 1.f,  6.f, 560.f, 1.85f, 1.3f, 1.15f },
		// Balanced: full steering that stays usable at speed, turn-in and grip help, drifts on the
		// handbrake, a light spin assist.
		{ 1.f,   0.65f, 8.f,  1.f,   1.f,  1.f,  3.f, 420.f, 1.55f, 1.f,  1.f },
		// Simulation: less grip, weaker brakes, slower steering, no assists at all - the tyres alone.
		{ 0.95f, 0.5f,  5.5f, 0.78f, 0.8f, 1.35f, 0.f, 0.f,   0.f,   0.f,  0.f },
	};
	return V[Style < 0 ? 0 : (Style >= DrivingStyleCount ? DrivingStyleCount - 1 : Style)];
}

// ---- Benchmark: measured average FPS against a target, into a quality preset (0 Low .. 4 Cinematic).
// One preset step per ~35% below target, so a machine at a third of the target drops two presets.
inline int RecommendedQuality(int CurrentPreset, float AverageFps, float TargetFps)
{
	if (AverageFps <= 0.f || TargetFps <= 0.f) return CurrentPreset;
	const float Ratio = AverageFps / TargetFps;
	int Steps = 0;
	if (Ratio < 0.95f)      Steps = -1 - static_cast<int>((0.95f - Ratio) / 0.35f);
	else if (Ratio > 1.35f) Steps = 1 + static_cast<int>((Ratio - 1.35f) / 0.5f);
	const int Wanted = CurrentPreset + Steps;
	return Wanted < 0 ? 0 : (Wanted > 4 ? 4 : Wanted);
}

// ---- Image options (settings menu, Video tab)

// Anti-aliasing method shown in the menu -> r.AntiAliasingMethod (0 none, 1 FXAA, 2 TAA, 4 TSR).
inline constexpr int AaMethodCount = 4;
inline int AaMethodCvar(int Index)
{
	static const int Values[AaMethodCount] = { 0, 1, 2, 4 };
	return Values[Index < 0 ? 0 : (Index >= AaMethodCount ? AaMethodCount - 1 : Index)];
}

// Sharpening slider 0..1 -> r.Tonemapper.Sharpen 0..2 (much above 2 rings around edges).
inline float SharpenCvar(float Slider) { return 2.f * Clamp(Slider, 0.f, 1.f); }

// ---- HUD options (Gameplay & HUD tab)

// Speed readout: km/h or mph.
inline float SpeedInUnits(float Kph, bool bMph) { return bMph ? Kph * 0.621371f : Kph; }

// Radar zoom: how many cm of world the radar shows from its centre to its edge.
inline constexpr int MinimapZoomCount = 3;
inline float MinimapRangeCm(int Index)
{
	static const float Values[MinimapZoomCount] = { 3000.f, 4500.f, 7000.f }; // Close, normal, wide.
	return Values[Index < 0 ? 0 : (Index >= MinimapZoomCount ? MinimapZoomCount - 1 : Index)];
}

// HUD text opacity: never fully invisible from the menu.
inline float HudOpacity(float Slider) { return Clamp(Slider, 0.3f, 1.f); }

// Floating damage numbers: hits on the same car within MergeSeconds add up into one number, which
// rises and fades over LifeSeconds.
struct DamageNumberTuning
{
	float MergeSeconds = 0.45f;
	float LifeSeconds = 1.1f;
	float RiseCmPerSecond = 120.f;
};

inline float DamageNumberAlpha(float Age, const DamageNumberTuning& T = DamageNumberTuning())
{
	if (Age < 0.f || Age >= T.LifeSeconds) return 0.f;
	const float FadeStart = T.LifeSeconds * 0.6f;
	return Age < FadeStart ? 1.f : 1.f - (Age - FadeStart) / (T.LifeSeconds - FadeStart);
}

// ---- Countermeasures (Controls tab): auto-flares fire when a homing missile is this close.
inline bool ShouldAutoFlare(bool bEnabled, float MissileDistanceCm, float DecoyRangeCm)
{
	return bEnabled && MissileDistanceCm >= 0.f && MissileDistanceCm <= DecoyRangeCm * 0.75f;
}
}
