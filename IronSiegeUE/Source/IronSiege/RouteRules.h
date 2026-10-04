#pragma once
// Engine-independent street routing for the AI: turns the city's road segments into a graph
// (junctions where roads cross, and road ends that stop just short of another road snapped onto
// it), finds the shortest way along the roads between two points, and walks a car through the
// resulting waypoints. No Unreal includes on purpose: covered offline by Tests/route_rules_test.cpp.

#include <cmath>

namespace IronRoute
{
struct Vec2
{
	float X = 0.f, Y = 0.f;
};

inline Vec2 Sub(const Vec2& A, const Vec2& B) { return { A.X - B.X, A.Y - B.Y }; }
inline float Dot(const Vec2& A, const Vec2& B) { return A.X * B.X + A.Y * B.Y; }
inline float Dist(const Vec2& A, const Vec2& B) { return std::sqrt(Dot(Sub(A, B), Sub(A, B))); }

struct Road
{
	Vec2 A, B;    // Centre line.
	float Width;  // Carriageway width.
};

// Parameter t (0..1) of the point on segment A-B closest to P.
inline float ClosestT(const Vec2& A, const Vec2& B, const Vec2& P)
{
	const Vec2 D = Sub(B, A);
	const float Len2 = Dot(D, D);
	float T = Len2 > 1e-3f ? Dot(Sub(P, A), D) / Len2 : 0.f;
	return T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
}

inline Vec2 At(const Road& R, float T) { return { R.A.X + (R.B.X - R.A.X) * T, R.A.Y + (R.B.Y - R.A.Y) * T }; }

inline constexpr int MaxNodes = 96;
inline constexpr int MaxEdges = 8;
inline constexpr int MaxRoads = 24;
inline constexpr int MaxPath = 64;

// Road graph. Nodes are points on the roads (ends, crossings, snaps); edges join neighbouring nodes
// along a road, and a snapped road end to the point it snapped onto.
struct Graph
{
	Road Roads[MaxRoads];
	int NumRoads = 0;
	Vec2 Nodes[MaxNodes];
	int NumNodes = 0;
	int Adj[MaxNodes][MaxEdges];
	float AdjCost[MaxNodes][MaxEdges];
	int Degree[MaxNodes] = {};
	// Per road: its nodes sorted along it (t values alongside), for locating a point on the road.
	int RoadNodes[MaxRoads][MaxNodes];
	float RoadNodeT[MaxRoads][MaxNodes];
	int RoadNodeCount[MaxRoads] = {};

	int AddNode(const Vec2& P)
	{
		// Merge with an existing node closer than a car length.
		for (int i = 0; i < NumNodes; ++i)
			if (Dist(Nodes[i], P) < 150.f) return i;
		if (NumNodes >= MaxNodes) return -1;
		Nodes[NumNodes] = P;
		return NumNodes++;
	}

	void Link(int A, int B)
	{
		if (A < 0 || B < 0 || A == B) return;
		for (int e = 0; e < Degree[A]; ++e)
			if (Adj[A][e] == B) return;
		const float C = Dist(Nodes[A], Nodes[B]);
		if (Degree[A] < MaxEdges) { Adj[A][Degree[A]] = B; AdjCost[A][Degree[A]++] = C; }
		if (Degree[B] < MaxEdges) { Adj[B][Degree[B]] = A; AdjCost[B][Degree[B]++] = C; }
	}

	void AddToRoad(int R, int Node, float T)
	{
		for (int i = 0; i < RoadNodeCount[R]; ++i)
			if (RoadNodes[R][i] == Node) return;
		int i = RoadNodeCount[R]++;
		while (i > 0 && RoadNodeT[R][i - 1] > T)
		{
			RoadNodes[R][i] = RoadNodes[R][i - 1];
			RoadNodeT[R][i] = RoadNodeT[R][i - 1];
			--i;
		}
		RoadNodes[R][i] = Node;
		RoadNodeT[R][i] = T;
	}
};

// Where two segments cross (strictly inside or at the ends of both); false if parallel or apart.
inline bool Crossing(const Road& P, const Road& Q, float& OutTP, float& OutTQ)
{
	const Vec2 R = Sub(P.B, P.A), S = Sub(Q.B, Q.A);
	const float Den = R.X * S.Y - R.Y * S.X;
	if (std::fabs(Den) < 1e-3f) return false;
	const Vec2 QP = Sub(Q.A, P.A);
	const float T = (QP.X * S.Y - QP.Y * S.X) / Den;
	const float U = (QP.X * R.Y - QP.Y * R.X) / Den;
	if (T < -1e-4f || T > 1.f + 1e-4f || U < -1e-4f || U > 1.f + 1e-4f) return false;
	OutTP = T;
	OutTQ = U;
	return true;
}

inline Graph Build(const Road* Roads, int NumRoads)
{
	Graph G;
	G.NumRoads = NumRoads < MaxRoads ? NumRoads : MaxRoads;
	for (int r = 0; r < G.NumRoads; ++r) G.Roads[r] = Roads[r];

	for (int r = 0; r < G.NumRoads; ++r)
	{
		G.AddToRoad(r, G.AddNode(G.Roads[r].A), 0.f);
		G.AddToRoad(r, G.AddNode(G.Roads[r].B), 1.f);
	}
	// Crossings become shared junction nodes.
	for (int a = 0; a < G.NumRoads; ++a)
		for (int b = a + 1; b < G.NumRoads; ++b)
		{
			float Ta, Tb;
			if (Crossing(G.Roads[a], G.Roads[b], Ta, Tb))
			{
				const int N = G.AddNode(At(G.Roads[a], Ta));
				G.AddToRoad(a, N, Ta);
				G.AddToRoad(b, N, Tb);
			}
		}
	// A road that ends just short of another (a cross street stopping at the ring road's kerb)
	// is connected to it: its end joins the nearest point of that road.
	for (int a = 0; a < G.NumRoads; ++a)
		for (int End = 0; End < 2; ++End)
		{
			const Vec2 P = End == 0 ? G.Roads[a].A : G.Roads[a].B;
			for (int b = 0; b < G.NumRoads; ++b)
			{
				if (b == a) continue;
				const float T = ClosestT(G.Roads[b].A, G.Roads[b].B, P);
				const Vec2 Q = At(G.Roads[b], T);
				const float Gap = Dist(P, Q);
				if (Gap < 1.f || Gap > 0.5f * (G.Roads[a].Width + G.Roads[b].Width) + 200.f) continue;
				const int N = G.AddNode(Q);
				G.AddToRoad(b, N, T);
				G.Link(G.AddNode(P), N);
			}
		}
	// Neighbours along each road.
	for (int r = 0; r < G.NumRoads; ++r)
		for (int i = 0; i + 1 < G.RoadNodeCount[r]; ++i)
			G.Link(G.RoadNodes[r][i], G.RoadNodes[r][i + 1]);
	return G;
}

// The road nearest P, the point on it, and the two nodes along that road on either side of it.
struct Anchor
{
	int Road = -1;
	Vec2 Point;
	float Distance = 1e30f;
	int Before = -1, After = -1;
};

inline Anchor Locate(const Graph& G, const Vec2& P)
{
	Anchor Best;
	for (int r = 0; r < G.NumRoads; ++r)
	{
		const float T = ClosestT(G.Roads[r].A, G.Roads[r].B, P);
		const Vec2 Q = At(G.Roads[r], T);
		const float D = Dist(P, Q);
		if (D >= Best.Distance) continue;
		Best.Road = r;
		Best.Point = Q;
		Best.Distance = D;
		Best.Before = Best.After = -1;
		for (int i = 0; i < G.RoadNodeCount[r]; ++i)
		{
			if (G.RoadNodeT[r][i] <= T) Best.Before = G.RoadNodes[r][i];
			if (G.RoadNodeT[r][i] >= T && Best.After < 0) Best.After = G.RoadNodes[r][i];
		}
	}
	return Best;
}

struct Path
{
	Vec2 Points[MaxPath];
	int Count = 0;
	float Length = 0.f;
};

// Shortest way along the roads from From to To: onto the nearest road, junction to junction
// (Dijkstra - the graph is a few dozen nodes), off at the point nearest To, then To itself.
inline Path FindPath(const Graph& G, const Vec2& From, const Vec2& To)
{
	Path Out;
	const Anchor S = Locate(G, From), E = Locate(G, To);
	auto Push = [&Out](const Vec2& P)
	{
		if (Out.Count > 0 && Dist(Out.Points[Out.Count - 1], P) < 50.f) return;
		if (Out.Count < MaxPath) Out.Points[Out.Count++] = P;
	};
	if (S.Road < 0 || E.Road < 0 || G.NumNodes == 0)
	{
		Push(To);
		return Out;
	}
	// Same stretch of the same road: no junction in between, just drive along it.
	if (S.Road == E.Road && S.Before == E.Before && S.After == E.After)
	{
		Push(S.Point);
		Push(E.Point);
		Push(To);
		for (int i = 0; i + 1 < Out.Count; ++i) Out.Length += Dist(Out.Points[i], Out.Points[i + 1]);
		return Out;
	}
	float Cost[MaxNodes];
	int Prev[MaxNodes];
	bool Done[MaxNodes] = {};
	for (int i = 0; i < G.NumNodes; ++i) { Cost[i] = 1e30f; Prev[i] = -1; }
	const int StartNodes[2] = { S.Before, S.After };
	for (int N : StartNodes)
		if (N >= 0) Cost[N] = Dist(S.Point, G.Nodes[N]) < Cost[N] ? Dist(S.Point, G.Nodes[N]) : Cost[N];
	for (;;)
	{
		int U = -1;
		for (int i = 0; i < G.NumNodes; ++i)
			if (!Done[i] && Cost[i] < 1e29f && (U < 0 || Cost[i] < Cost[U])) U = i;
		if (U < 0) break;
		Done[U] = true;
		for (int e = 0; e < G.Degree[U]; ++e)
		{
			const int V = G.Adj[U][e];
			if (Cost[U] + G.AdjCost[U][e] < Cost[V])
			{
				Cost[V] = Cost[U] + G.AdjCost[U][e];
				Prev[V] = U;
			}
		}
	}
	// Leave the network at whichever of the end stretch's two nodes gives the shorter total.
	int Exit = -1;
	float Best = 1e30f;
	const int EndNodes[2] = { E.Before, E.After };
	for (int N : EndNodes)
	{
		if (N < 0 || Cost[N] > 1e29f) continue;
		const float Total = Cost[N] + Dist(G.Nodes[N], E.Point);
		if (Total < Best) { Best = Total; Exit = N; }
	}
	if (Exit < 0)
	{
		Push(To); // Unreachable: fall back to heading straight for it.
		return Out;
	}
	int Chain[MaxNodes];
	int Len = 0;
	for (int N = Exit; N >= 0 && Len < MaxNodes; N = Prev[N]) Chain[Len++] = N;
	Push(S.Point);
	for (int i = Len - 1; i >= 0; --i) Push(G.Nodes[Chain[i]]);
	Push(E.Point);
	Push(To);
	for (int i = 0; i + 1 < Out.Count; ++i) Out.Length += Dist(Out.Points[i], Out.Points[i + 1]);
	return Out;
}

// Which waypoint to steer for now, starting from Current: skip every waypoint already within
// Reach, and a waypoint we are already past (closer to the one after it than it is itself).
inline int Advance(const Path& P, int Current, const Vec2& Car, float Reach)
{
	int I = Current < 0 ? 0 : Current;
	while (I < P.Count - 1)
	{
		const float ToThis = Dist(Car, P.Points[I]);
		const float Hop = Dist(P.Points[I], P.Points[I + 1]);
		if (ToThis < Reach || Dist(Car, P.Points[I + 1]) < Hop) { ++I; continue; }
		break;
	}
	return I < P.Count ? I : P.Count - 1;
}

// Pure pursuit: the point LookAhead further along the route from where the car is, measured along
// the route line itself (the car's position is first projected onto the leg it is driving, the one
// ending at waypoint Index). Steering at it keeps the car on the road's centre line instead of
// cutting a straight line to a junction far away - through the pavement and into the building.
inline Vec2 Carrot(const Path& P, int Index, const Vec2& Car, float LookAhead)
{
	if (P.Count == 0) return Car;
	if (Index <= 0 || Index >= P.Count) return P.Points[Index <= 0 ? 0 : P.Count - 1];
	const Vec2 A = P.Points[Index - 1], B = P.Points[Index];
	const float T = ClosestT(A, B, Car);
	Vec2 From{ A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T };
	float Left = LookAhead;
	for (int i = Index; i < P.Count; ++i)
	{
		const float Leg = Dist(From, P.Points[i]);
		if (Leg >= Left && Leg > 1e-3f)
		{
			const float F = Left / Leg;
			return { From.X + (P.Points[i].X - From.X) * F, From.Y + (P.Points[i].Y - From.Y) * F };
		}
		Left -= Leg;
		From = P.Points[i];
	}
	return P.Points[P.Count - 1];
}

// Steering for the angle (degrees, + = to the right) between heading and the carrot: proportional,
// full lock from FullLockDeg on. The chase logic's (1 - cos) curve barely corrects small angles,
// which is what let cars drift off the road.
inline float SteerForAngle(float Degrees, float FullLockDeg = 35.f)
{
	const float S = Degrees / (FullLockDeg > 1.f ? FullLockDeg : 1.f);
	return S < -1.f ? -1.f : (S > 1.f ? 1.f : S);
}

// Corner speed: slow down to take a turn of Degrees (angle between the car's heading and the
// waypoint) instead of sliding into the building on the corner.
inline float CornerSpeedKph(float Degrees, float CruiseKph)
{
	const float A = Degrees < 0.f ? -Degrees : Degrees;
	if (A < 20.f) return CruiseKph;
	// A right angle at ~22 km/h: faster and the physics buggies slide wide into the kerb-side
	// traffic lights (seen in a -game run at 48 km/h).
	const float Slowest = 22.f;
	if (A > 80.f) return CruiseKph < Slowest ? CruiseKph : Slowest;
	const float T = (A - 20.f) / 60.f;
	const float Kph = CruiseKph + (Slowest - CruiseKph) * T;
	return Kph < CruiseKph ? Kph : CruiseKph;
}

// Turn angle (degrees, 0 = straight on) at waypoint Index: between the leg arriving at it and the
// leg leaving it. 0 at the ends of the route.
inline float TurnAt(const Path& P, int Index)
{
	if (Index <= 0 || Index >= P.Count - 1) return 0.f;
	const Vec2 In = Sub(P.Points[Index], P.Points[Index - 1]), Out = Sub(P.Points[Index + 1], P.Points[Index]);
	const float L = std::sqrt(Dot(In, In) * Dot(Out, Out));
	if (L < 1e-3f) return 0.f;
	float C = Dot(In, Out) / L;
	C = C < -1.f ? -1.f : (C > 1.f ? 1.f : C);
	return std::acos(C) * 57.2957795f;
}

// Speed to hold now so the car can still brake down to the corner speed of the turn DistanceCm
// ahead (v^2 = v_corner^2 + 2 a d; 3.5 m/s^2 leaves margin for the AI's late, partial braking).
inline float ApproachSpeedKph(float DistanceCm, float TurnDeg, float CruiseKph, float BrakeMps2 = 3.5f)
{
	const float Corner = CornerSpeedKph(TurnDeg, CruiseKph) / 3.6f;
	const float D = DistanceCm > 0.f ? DistanceCm / 100.f : 0.f;
	const float V = std::sqrt(Corner * Corner + 2.f * BrakeMps2 * D) * 3.6f;
	return V < CruiseKph ? V : CruiseKph;
}

// ---- Weaving between obstacles in the road (the city battlefield's wrecks and barricades).

struct Obstacle
{
	Vec2 P;
	float Radius;
};

// Distance from point Q to segment A-B.
inline float DistToSegment(const Vec2& A, const Vec2& B, const Vec2& Q)
{
	const float T = ClosestT(A, B, Q);
	return Dist(Q, { A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T });
}

// Moves the steering point (the carrot) sideways so the straight run from the car to it passes
// every obstacle with Clearance to spare: tries sideways offsets nearest the centre first (0, +-Step,
// +-2 Step... up to MaxOffset), and keeps whichever clears. Returns the carrot unchanged if the way is
// already clear, and the least-bad offset if nothing clears (the obstacle avoidance probes take over).
inline Vec2 ChooseLane(const Vec2& Car, const Vec2& Carrot, const Obstacle* Obs, int Count, float Clearance, float MaxOffset, float Step = 100.f)
{
	const Vec2 D = Sub(Carrot, Car);
	const float L = std::sqrt(Dot(D, D));
	if (L < 1.f || Count <= 0) return Carrot;
	const Vec2 Side{ -D.Y / L, D.X / L };
	Vec2 Best = Carrot;
	float BestMargin = -1e30f;
	const int Steps = Step > 1.f ? static_cast<int>(MaxOffset / Step) : 0;
	for (int k = 0; k <= 2 * Steps; ++k)
	{
		// 0, +1, -1, +2, -2 ... steps.
		const int n = (k + 1) / 2;
		const float Offset = (k % 2 == 1 ? 1.f : -1.f) * n * Step;
		const Vec2 C{ Carrot.X + Side.X * Offset, Carrot.Y + Side.Y * Offset };
		float Margin = 1e30f;
		for (int i = 0; i < Count; ++i)
		{
			const float M = DistToSegment(Car, C, Obs[i].P) - Obs[i].Radius - Clearance;
			if (M < Margin) Margin = M;
		}
		if (Margin >= 0.f) return C;
		if (Margin > BestMargin) { BestMargin = Margin; Best = C; }
	}
	return Best;
}

// Speed limit from the route's corners: brake ahead of the next turn (ApproachSpeedKph), and keep
// the corner speed until the car is ExitCm clear of the turn it has just taken - switching to the
// next waypoint at the start of a turn must not let it accelerate halfway round it.
inline float RouteSpeedLimitKph(const Path& P, int Index, const Vec2& Car, float CruiseKph, float ExitCm = 1100.f)
{
	float Limit = CruiseKph;
	if (Index >= 0 && Index < P.Count - 1)
	{
		const float Ahead = ApproachSpeedKph(Dist(Car, P.Points[Index]), TurnAt(P, Index), CruiseKph);
		Limit = Ahead < Limit ? Ahead : Limit;
	}
	if (Index >= 1 && Index - 1 < P.Count && Dist(Car, P.Points[Index - 1]) < ExitCm)
	{
		const float Behind = CornerSpeedKph(TurnAt(P, Index - 1), CruiseKph);
		Limit = Behind < Limit ? Behind : Limit;
	}
	return Limit;
}
}
