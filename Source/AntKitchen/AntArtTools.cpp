#include "AntArtTools.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshResources.h"
#endif

bool UAntArtTools::SetLodFromMesh(UStaticMesh* Dest, int32 Lod, UStaticMesh* Src, float ScreenSize)
{
#if WITH_EDITOR
	if (!Dest || !Src || Lod < 1 || Dest == Src)
	{
		return false;
	}
	const FMeshDescription* SrcMd = Src->GetMeshDescription(0);
	if (!SrcMd)
	{
		UE_LOG(LogTemp, Warning, TEXT("AntArtTools: %s has no mesh description"), *Src->GetName());
		return false;
	}
	Dest->Modify();
	if (Dest->GetNumSourceModels() <= Lod)
	{
		Dest->SetNumSourceModels(Lod + 1);
	}
	FStaticMeshSourceModel& Sm = Dest->GetSourceModel(Lod);
	Sm.BuildSettings = Dest->GetSourceModel(0).BuildSettings;
	Sm.ReductionSettings = FMeshReductionSettings();   // its own geometry, no reduction
	Sm.ScreenSize.Default = ScreenSize;
	FMeshDescription* Md = Dest->CreateMeshDescription(Lod, *SrcMd);
	if (!Md)
	{
		return false;
	}
	Dest->CommitMeshDescription(Lod);
	// every section of the new LOD draws with material slot 0 (the hero props have one slot: their own baked set)
	FMeshSectionInfoMap& Map = Dest->GetSectionInfoMap();
	for (int32 s = 0; s < 8; ++s)
	{
		FMeshSectionInfo Info = Map.Get(Lod, s);
		Info.MaterialIndex = 0;
		Map.Set(Lod, s, Info);
	}
	Dest->SetAutoComputeLODScreenSize(false);
	Dest->Build(false);
	Dest->PostEditChange();
	Dest->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("AntArtTools: %s LOD%d from %s (%d triangles), screen size %.2f"), *Dest->GetName(), Lod, *Src->GetName(), Dest->GetNumTriangles(Lod), ScreenSize);
	return true;
#else
	return false;
#endif
}

int32 UAntArtTools::LodCount(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	return Mesh ? Mesh->GetNumSourceModels() : 0;
#else
	return Mesh ? Mesh->GetNumLODs() : 0;
#endif
}

int32 UAntArtTools::ConvexCount(UStaticMesh* Mesh)
{
	UBodySetup* Bs = Mesh ? Mesh->GetBodySetup() : nullptr;
	return Bs ? Bs->AggGeom.ConvexElems.Num() : 0;
}
