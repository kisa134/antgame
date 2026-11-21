#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

class UStaticMesh;

/**
 * Wreck geometry: a piece's own collision triangles (the cooked complex collision, readable in a packaged game without CPU access
 * to the render mesh) as procedural sections per material slot, the seeded cut of them into chunks, and the few boxes that make
 * its physics body. Everything is cached per mesh and deterministic: every machine gets the same chunks from (mesh, seed, count).
 */
namespace AntWreckGeo
{
	struct FGeo
	{
		TArray<FProcMeshSection> Sections;
		/** Per section: the mesh's material slot, -1 for a cut face. */
		TArray<int32> Slot;
		FBox Box = FBox(ForceInit);
		/** The collision is a simplified stand-in, not the drawn model (a table whose cloth is a skirt to the floor for the ants): its chunks must not be shown in place. */
		bool bProxy = false;
		bool IsValid() const { return Sections.Num() > 0 && Box.IsValid != 0; }
	};

	/** The whole piece (mesh space). Null when the mesh has neither collision triangles nor CPU data (then: its bounds box). */
	ANTKITCHEN_API TSharedPtr<const FGeo> Base(UStaticMesh* Mesh);
	/** The seeded cut into (up to) Count chunks, mesh space. */
	ANTKITCHEN_API const TArray<TSharedPtr<const FGeo>>& Cut(AActor* Scratch, UStaticMesh* Mesh, int32 Seed, int32 Count);
	/** The boxes of its physics body, mesh space, for that scale (cell size ~300 uu in the world). */
	ANTKITCHEN_API const TArray<FBox>& Hull(UStaticMesh* Mesh, const FVector& Scale);
	/** Chunks for a piece of that world size. */
	ANTKITCHEN_API int32 ChunkCount(float WorldSize);
	/** A dent: vertices within Radius of At (mesh space) pushed along Dir by up to Depth. */
	ANTKITCHEN_API void Dent(FProcMeshSection& S, const FVector& At, const FVector& Dir, float Radius, float Depth);
}
