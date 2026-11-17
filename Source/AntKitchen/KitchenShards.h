#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

class UStaticMesh;

/**
 * Real fracture for food: the model is cut by random planes into chunks, so a chocolate bar that falls off the
 * fridge breaks into different pieces every time. The cut is seeded, so the server and every client get the
 * same chunks from (mesh, seed, path) without sending any geometry over the network.
 */
namespace AntShards
{
	struct FChunk
	{
		TArray<FProcMeshSection> Sections;
		/** Per section: true for the cut face (the inside of the food). */
		TArray<bool> Cap;
		FBox Box = FBox(ForceInit);
		bool IsValid() const { return Sections.Num() > 0 && Box.IsValid != 0; }
	};

	/**
	 * Path is (index, count) pairs: {2,5} is chunk 2 of 5; {2,5, 0,3} is chunk 0 of 3 cut out of that one.
	 * ArtMesh null = the basic shape. Null result = the mesh can't be read on the CPU (use the plain model then).
	 */
	ANTKITCHEN_API TSharedPtr<const FChunk> Get(AActor* Owner, UStaticMesh* ArtMesh, uint8 Shape, int32 Seed, const TArray<uint8>& Path);
}