#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AntArtTools.generated.h"

class UStaticMesh;

/**
 * Editor helpers for the art import scripts (Tools/import_blender_art.py, the hero props of KAN-22), callable from Python in a
 * commandlet, where the editor subsystems (StaticMeshEditorSubsystem) do not exist. Editor builds only: in the game they do nothing.
 */
UCLASS()
class ANTKITCHEN_API UAntArtTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Dest's LOD <Lod> becomes a copy of Src's LOD 0 (its own geometry: a hand-made or Blender-decimated LOD), drawn below ScreenSize. */
	UFUNCTION(BlueprintCallable, Category = "AntArt")
	static bool SetLodFromMesh(UStaticMesh* Dest, int32 Lod, UStaticMesh* Src, float ScreenSize);

	/** How many LODs the mesh has (its source models). */
	UFUNCTION(BlueprintCallable, Category = "AntArt")
	static int32 LodCount(UStaticMesh* Mesh);

	/** Its convex hulls (the UCX_ of the FBX) and whether they are ready for physics. */
	UFUNCTION(BlueprintCallable, Category = "AntArt")
	static int32 ConvexCount(UStaticMesh* Mesh);
};
