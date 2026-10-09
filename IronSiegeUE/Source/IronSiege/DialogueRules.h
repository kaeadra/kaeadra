#pragma once
// Engine-independent radio dialogue: a short queue of lines shown one at a time in the HUD's radio
// box, each for as long as it takes to read. Story lines always get said, in order; a driver's
// barks only play when the radio is quiet. No Unreal includes on purpose: covered offline by
// Tests/crew_rules_test.cpp.

namespace IronDialogue
{
inline constexpr int PriorityBark = 0;
inline constexpr int PriorityStory = 1;

struct Line
{
	int Speaker = 0;             // IronCrew speaker index.
	int Mood = 0;
	const char* Key = nullptr;   // Translation key.
	const char* Text = nullptr;  // English source.
	int Priority = PriorityBark;
};

// Reading time for a line: a base plus a bit per character, within sane limits.
inline float SecondsFor(const char* Text)
{
	int Chars = 0;
	for (const char* P = Text; P && *P; ++P) ++Chars;
	const float Seconds = 1.6f + 0.05f * Chars;
	return Seconds < 2.2f ? 2.2f : (Seconds > 6.f ? 6.f : Seconds);
}

struct Queue
{
	static constexpr int Capacity = 8;
	static constexpr float FadeSeconds = 0.25f;

	Line Lines[Capacity];
	int Count = 0;
	float Elapsed = 0.f;  // Of the line on air (Lines[0]).
	float Duration = 0.f;

	const Line* Current() const { return Count > 0 ? &Lines[0] : nullptr; }

	// False if the line was dropped: a bark while anything is on air or waiting, or a story line
	// when the queue is full of story lines. A story line cuts off a bark that is on air.
	bool Push(const Line& L)
	{
		if (!L.Text) return false;
		if (L.Priority == PriorityBark)
		{
			if (Count > 0) return false;
		}
		else
		{
			if (Count > 0 && Lines[0].Priority == PriorityBark) Count = 0;
			if (Count >= Capacity) return false;
		}
		Lines[Count++] = L;
		if (Count == 1) Start();
		return true;
	}

	void Tick(float DeltaSeconds)
	{
		if (Count == 0) return;
		Elapsed += DeltaSeconds;
		if (Elapsed >= Duration) Skip();
	}

	// Drops the line on air and moves to the next.
	void Skip()
	{
		if (Count == 0) return;
		for (int i = 1; i < Count; ++i) Lines[i - 1] = Lines[i];
		--Count;
		Start();
	}

	void Clear()
	{
		Count = 0;
		Elapsed = Duration = 0.f;
	}

	// 0..1 opacity of the radio box: fades in and out at the ends of each line.
	float Alpha() const
	{
		if (Count == 0) return 0.f;
		const float In = Elapsed / FadeSeconds;
		const float Out = (Duration - Elapsed) / FadeSeconds;
		const float A = In < Out ? In : Out;
		return A < 0.f ? 0.f : (A > 1.f ? 1.f : A);
	}

private:
	void Start()
	{
		Elapsed = 0.f;
		Duration = Count > 0 ? SecondsFor(Lines[0].Text) : 0.f;
	}
};
}
