#pragma once

#include "CoreMinimal.h"

/**
 * The route graph of the level v2 flat (docs/level/LEVEL_V2.md, "Граф маршрутов"): 32 named nodes, 51 edges with the time to
 * run them and the seconds of that spent where the Human's slipper reaches. The rival clan walks it, the squads haul home
 * along it, ambushes sit on its junctions. Static data: the same on every machine.
 */
namespace AntRoutes
{
	enum EKind : uint8 { Cover, Open, Outside, InWall };
	struct FNode { float X, Y, Z; const TCHAR* Name; };
	struct FEdge { int32 A, B; uint8 Kind; float Run; float Seen; };

	ANTKITCHEN_API int32 NumNodes();
	ANTKITCHEN_API const FNode& Node(int32 Index);
	ANTKITCHEN_API FVector Point(int32 Index);
	ANTKITCHEN_API TConstArrayView<FEdge> Edges();
	ANTKITCHEN_API int32 Find(const TCHAR* Name);
	/** The node to start from when standing at P (the nearest in the same part of the house; any, if that part has none). */
	ANTKITCHEN_API int32 Nearest(const FVector& P);
	/** The four junctions where routes meet (J1 under the table, J2 the fridge gap, J3 the threshold, J4 the house corner). */
	ANTKITCHEN_API TConstArrayView<int32> Junctions();
	ANTKITCHEN_API bool IsJunction(int32 Index);
	/** UseFlap: the cat flap may be walked through (it only opens from the living room: for those who can open it).
	 *  FloorOnly: no passages inside the walls, no vents, no holes (for a squad that walks as a column). */
	enum EFlags : uint32 { UseFlap = 1, FloorOnly = 2 };
	/** Shortest way From -> To as node indices (both included). bSafe: a second in the Human's view costs five. */
	ANTKITCHEN_API bool Path(int32 From, int32 To, bool bSafe, TArray<int32>& Out, float* OutSeconds = nullptr, uint32 Flags = 0);
	/** The same as world points, from where you stand to the goal. */
	ANTKITCHEN_API bool PathPoints(const FVector& From, const FVector& To, bool bSafe, TArray<FVector>& Out, float* OutSeconds = nullptr, uint32 Flags = 0);
	/** Our two ways in: the crack to the nest, and the south entrance at the house corner. */
	ANTKITCHEN_API int32 NestNode();
	ANTKITCHEN_API int32 SouthNode();
	ANTKITCHEN_API int32 HillNode();
}
