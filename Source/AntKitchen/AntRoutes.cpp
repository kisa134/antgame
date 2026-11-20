#include "AntRoutes.h"
#include "KitchenBuilder.h"
#include "LevelV2.h"

namespace AntRoutes
{

namespace
{
	// Same order and names as the callouts of LevelV2Data.inl.
	enum : int32
	{
		N_Nest, N_Crack, N_NorthHole, N_Corner, N_WindowCrack, N_Vent1, N_Broom, N_Pantry, N_Sink, N_Stove, N_UnderTable, N_FridgeGap,
		N_CorrW, N_CorrSink, N_CorrE, N_Dump, N_Vent2, N_TrashHole, N_Threshold, N_BehindDoor, N_Granny, N_Carpet, N_UnderSofa, N_TV,
		N_LivingWindow, N_Library, N_CatFlap, N_RedCrack, N_Hill, N_TreePass, N_YardFlap, N_Bed, N_Count
	};
	static const FNode Nodes[N_Count] =
	{
		{ -7900.f, -2500.f, 0.f, TEXT("Гнездо") },
		{ -6950.f, -2500.f, 0.f, TEXT("Щель") },
		{ -7250.f, 4875.f, 0.f, TEXT("Северный лаз") },
		{ -8300.f, -5300.f, 0.f, TEXT("Угол дома") },
		{ -4500.f, -5100.f, 0.f, TEXT("Щель под окном") },
		{ -3100.f, -4950.f, 0.f, TEXT("Вентиляция-1") },
		{ -6700.f, 650.f, 0.f, TEXT("Метла") },
		{ -5300.f, 2800.f, 0.f, TEXT("Кладовые") },
		{ -3000.f, 2850.f, 0.f, TEXT("Мойка") },
		{ -750.f, 2700.f, 0.f, TEXT("Плита") },
		{ 500.f, -500.f, 0.f, TEXT("Подстолье") },
		{ 2600.f, 2950.f, 0.f, TEXT("Щель у холодильника") },
		{ -6800.f, 4875.f, 0.f, TEXT("Коридор, запад") },
		{ -3500.f, 4875.f, 0.f, TEXT("Коридор, мойка") },
		{ 2600.f, 4875.f, 0.f, TEXT("Коридор, восток") },
		{ 5450.f, 250.f, 0.f, TEXT("Свалка") },
		{ 6930.f, 400.f, 0.f, TEXT("Вентиляция-2") },
		{ 7100.f, 980.f, 0.f, TEXT("Мусорный лаз") },
		{ 7100.f, -4000.f, 0.f, TEXT("Порог") },
		{ 8100.f, -1100.f, 0.f, TEXT("За дверью") },
		{ 10200.f, -4450.f, 0.f, TEXT("Бабушкин угол") },
		{ 13750.f, -4600.f, 0.f, TEXT("Ковёр") },
		{ 13500.f, 900.f, 0.f, TEXT("Под диваном") },
		{ 12700.f, -7600.f, 0.f, TEXT("Тумба ТВ") },
		{ 18300.f, -8600.f, 0.f, TEXT("Окно гостиной") },
		{ 19100.f, -4500.f, 0.f, TEXT("Библиотека") },
		{ 7250.f, -7350.f, 30.f, TEXT("Кошачья дверца") },
		{ 19500.f, -9100.f, 0.f, TEXT("Красная щель") },
		{ 12800.f, -10700.f, 0.f, TEXT("Красный холм") },
		{ 6100.f, -9000.f, 0.f, TEXT("Проход у дерева") },
		{ 6800.f, -7350.f, 0.f, TEXT("Двор у дверцы") },
		{ 0.f, -5600.f, 0.f, TEXT("Грядка") },
	};
	// from, to, kind, seconds at a run, seconds of those in the Human's view
	static const FEdge EdgeTable[] =
	{
		{ N_Nest, N_Crack, Cover, 2.1f, 0.1f },
		{ N_Nest, N_NorthHole, Outside, 16.7f, 0.f },
		{ N_Nest, N_Corner, Outside, 6.2f, 0.f },
		{ N_NorthHole, N_CorrW, InWall, 1.0f, 0.f },
		{ N_CorrW, N_CorrSink, InWall, 7.2f, 0.f },
		{ N_CorrSink, N_CorrE, InWall, 13.4f, 0.f },
		{ N_CorrSink, N_Sink, Cover, 4.6f, 0.3f },
		{ N_CorrE, N_FridgeGap, Cover, 4.2f, 1.2f },
		{ N_Crack, N_UnderTable, Open, 16.9f, 11.8f },
		{ N_Crack, N_Broom, Open, 7.0f, 7.0f },
		{ N_Crack, N_Pantry, Open, 12.2f, 9.8f },
		{ N_Crack, N_Sink, Open, 14.6f, 12.0f },
		{ N_Crack, N_Stove, Open, 17.7f, 16.5f },
		{ N_Crack, N_WindowCrack, Open, 7.8f, 7.5f },
		{ N_Broom, N_Pantry, Open, 5.6f, 5.3f },
		{ N_Pantry, N_Sink, Open, 5.0f, 5.0f },
		{ N_Sink, N_Stove, Open, 4.9f, 3.8f },
		{ N_Stove, N_FridgeGap, Open, 7.4f, 7.4f },
		{ N_Stove, N_UnderTable, Open, 7.5f, 5.9f },
		{ N_Pantry, N_UnderTable, Open, 14.6f, 11.5f },
		{ N_FridgeGap, N_UnderTable, Open, 8.9f, 5.7f },
		{ N_WindowCrack, N_Vent1, Open, 3.1f, 1.0f },
		{ N_WindowCrack, N_UnderTable, Open, 14.9f, 11.9f },
		{ N_WindowCrack, N_Corner, Outside, 8.8f, 0.f },
		{ N_UnderTable, N_Threshold, Open, 16.6f, 13.6f },
		{ N_UnderTable, N_Dump, Open, 11.3f, 5.9f },
		{ N_FridgeGap, N_Dump, Cover, 12.8f, 6.6f },
		{ N_Dump, N_Vent2, Cover, 3.6f, 0.8f },
		{ N_Vent2, N_TrashHole, Cover, 1.3f, 0.6f },
		{ N_TrashHole, N_BehindDoor, Cover, 5.7f, 4.7f },
		{ N_Threshold, N_BehindDoor, Cover, 6.9f, 3.3f },
		{ N_Threshold, N_Granny, Open, 6.9f, 6.2f },
		{ N_Threshold, N_Carpet, Open, 14.7f, 13.2f },
		{ N_BehindDoor, N_UnderSofa, Cover, 13.1f, 6.2f },
		{ N_UnderSofa, N_Library, Cover, 24.1f, 13.0f },
		{ N_UnderSofa, N_Carpet, Open, 14.0f, 10.8f },
		{ N_Granny, N_Carpet, Open, 7.8f, 6.9f },
		{ N_Carpet, N_TV, Open, 7.0f, 7.0f },
		{ N_Carpet, N_Library, Open, 11.8f, 10.5f },
		{ N_Granny, N_CatFlap, Cover, 10.3f, 0.f },
		{ N_TV, N_CatFlap, Cover, 13.7f, 5.7f },
		{ N_TV, N_LivingWindow, Cover, 13.7f, 12.7f },
		{ N_LivingWindow, N_Library, Open, 9.8f, 7.1f },
		{ N_LivingWindow, N_RedCrack, Cover, 3.2f, 2.7f },
		{ N_RedCrack, N_Hill, Outside, 15.5f, 0.f },
		{ N_Hill, N_TreePass, Outside, 15.3f, 0.f },
		{ N_TreePass, N_YardFlap, Outside, 3.9f, 0.f },
		{ N_CatFlap, N_YardFlap, Outside, 1.0f, 0.f },
		{ N_YardFlap, N_Bed, Outside, 15.8f, 0.f },
		{ N_Bed, N_WindowCrack, Outside, 10.6f, 0.f },
		{ N_Corner, N_Bed, Outside, 23.4f, 0.f },
	};
	static const int32 JunctionList[] = { N_UnderTable, N_FridgeGap, N_Threshold, N_Corner };
}

int32 NumNodes() { return N_Count; }
const FNode& Node(int32 Index) { return Nodes[FMath::Clamp(Index, 0, N_Count - 1)]; }
FVector Point(int32 Index) { const FNode& N = Node(Index); return FVector(N.X, N.Y, N.Z); }
TConstArrayView<FEdge> Edges() { return TConstArrayView<FEdge>(EdgeTable, UE_ARRAY_COUNT(EdgeTable)); }
TConstArrayView<int32> Junctions() { return TConstArrayView<int32>(JunctionList, UE_ARRAY_COUNT(JunctionList)); }
bool IsJunction(int32 Index) { return Junctions().Contains(Index); }
int32 NestNode() { return N_Crack; }
int32 SouthNode() { return N_Corner; }
int32 HillNode() { return N_Hill; }

int32 Find(const TCHAR* Name)
{
	for (int32 i = 0; i < N_Count; ++i)
	{
		if (FCString::Strcmp(Nodes[i].Name, Name) == 0) { return i; }
	}
	return INDEX_NONE;
}

int32 Nearest(const FVector& P)
{
	// The passages (cracks, the corridor in the wall, the flap) sit in walls where "which room" is anybody's guess: a node counts
	// as being in the same part of the house if it is, or if it is one of those and close.
	const int32 Region = AKitchenBuilder::Region(P);
	int32 Best = INDEX_NONE, BestAny = 0;
	float BestD = 1.e12f, BestAnyD = 1.e12f;
	for (int32 i = 0; i < N_Count; ++i)
	{
		const float D = (float)FVector::DistSquared2D(P, Point(i));
		if (D < BestAnyD) { BestAnyD = D; BestAny = i; }
		if (D < BestD && (AKitchenBuilder::Region(Point(i)) == Region || D < FMath::Square(500.f))) { BestD = D; Best = i; }
	}
	return Best != INDEX_NONE ? Best : BestAny;
}

namespace
{
	bool Hole(int32 N)
	{
		return N == N_NorthHole || N == N_CorrW || N == N_CorrSink || N == N_CorrE || N == N_Vent1 || N == N_Vent2 || N == N_TrashHole;
	}
}

bool Path(int32 From, int32 To, bool bSafe, TArray<int32>& Out, float* OutSeconds, uint32 Flags)
{
	Out.Reset();
	if (From < 0 || From >= N_Count || To < 0 || To >= N_Count)
	{
		return false;
	}
	float Cost[N_Count];
	float Time[N_Count];
	int32 Prev[N_Count];
	bool Done[N_Count];
	for (int32 i = 0; i < N_Count; ++i) { Cost[i] = 1.e9f; Time[i] = 0.f; Prev[i] = INDEX_NONE; Done[i] = false; }
	Cost[From] = 0.f;
	for (int32 Step = 0; Step < N_Count; ++Step)
	{
		int32 U = INDEX_NONE;
		for (int32 i = 0; i < N_Count; ++i)
		{
			if (!Done[i] && Cost[i] < 1.e8f && (U == INDEX_NONE || Cost[i] < Cost[U])) { U = i; }
		}
		if (U == INDEX_NONE || U == To)
		{
			break;
		}
		Done[U] = true;
		for (const FEdge& E : EdgeTable)
		{
			const int32 V = E.A == U ? E.B : E.B == U ? E.A : INDEX_NONE;
			if (V == INDEX_NONE || Done[V])
			{
				continue;
			}
			if (!(Flags & UseFlap) && (E.A == N_CatFlap || E.B == N_CatFlap))
			{
				continue;   // latched from the inside
			}
			if ((Flags & FloorOnly) && (E.Kind == InWall || Hole(E.A) || Hole(E.B)))
			{
				continue;
			}
			const float C = Cost[U] + E.Run + (bSafe ? E.Seen * 4.f : 0.f);
			if (C < Cost[V]) { Cost[V] = C; Time[V] = Time[U] + E.Run; Prev[V] = U; }
		}
	}
	if (Cost[To] >= 1.e8f)
	{
		return false;
	}
	for (int32 At = To; At != INDEX_NONE; At = Prev[At]) { Out.Insert(At, 0); }
	if (OutSeconds) { *OutSeconds = Time[To]; }
	return true;
}

bool PathPoints(const FVector& From, const FVector& To, bool bSafe, TArray<FVector>& Out, float* OutSeconds, uint32 Flags)
{
	Out.Reset();
	TArray<int32> Ids;
	const int32 A = Nearest(From), B = Nearest(To);
	if (!Path(A, B, bSafe, Ids, OutSeconds, Flags))
	{
		Out.Add(To);
		return false;
	}
	// Standing at the first node or between the first two already: no walking back to the first one.
	// (And never INTO the red hill: its node is the middle of a solid mound.)
	int32 First = 0;
	if (Ids.Num() >= 2 && (FVector::Dist2D(From, Point(Ids[0])) < 900.f || FVector::Dist2D(From, Point(Ids[1])) < FVector::Dist2D(Point(Ids[0]), Point(Ids[1]))))
	{
		First = 1;
	}
	for (int32 i = First; i < Ids.Num(); ++i) { Out.Add(Point(Ids[i])); }
	if (Out.Num() == 0 || FVector::Dist2D(Out.Last(), To) > 150.f) { Out.Add(To); }
	return true;
}

}   // namespace AntRoutes
