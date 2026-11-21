#include "AntWreckGeo.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"

namespace
{
	using AntWreckGeo::FGeo;

	TMap<FString, TSharedPtr<const FGeo>> GBase;
	TMap<FString, TArray<TSharedPtr<const FGeo>>> GCuts;
	TMap<FString, TArray<FBox>> GHulls;

	struct FTri { FVector A, B, C; int32 Slot; };

	/** The cooked complex collision of the mesh as triangles (mesh space), outward winding. */
	bool ReadTris(UStaticMesh* M, TArray<FTri>& Out)
	{
		UBodySetup* BS = M ? M->GetBodySetup() : nullptr;
		if (BS)
		{
			for (const Chaos::FTriangleMeshImplicitObjectPtr& TM : BS->TriMeshGeometries)
			{
				if (!TM.IsValid())
				{
					continue;
				}
				const auto& P = TM->Particles();
				const Chaos::FTrimeshIndexBuffer& E = TM->Elements();
				const int32 N = E.GetNumTriangles();
				for (int32 t = 0; t < N; ++t)
				{
					int32 I0, I1, I2;
					if (E.RequiresLargeIndices()) { const auto& Tr = E.GetLargeIndexBuffer()[t]; I0 = Tr[0]; I1 = Tr[1]; I2 = Tr[2]; }
					else { const auto& Tr = E.GetSmallIndexBuffer()[t]; I0 = Tr[0]; I1 = Tr[1]; I2 = Tr[2]; }
					FTri T;
					T.A = FVector(P.GetX(I0));
					T.B = FVector(P.GetX(I1));
					T.C = FVector(P.GetX(I2));
					T.Slot = (int32)TM->GetMaterialIndex(t);
					Out.Add(T);
				}
			}
		}
		// The cooked collision keeps the render section of each triangle; the material slot is that section's (they are not always in slot order).
		if (Out.Num() > 0 && M->GetRenderData() && M->GetRenderData()->LODResources.Num() > 0)
		{
			const FStaticMeshSectionArray& Secs = M->GetRenderData()->LODResources[0].Sections;
			for (FTri& T : Out)
			{
				if (Secs.IsValidIndex(T.Slot)) { T.Slot = Secs[T.Slot].MaterialIndex; }
			}
		}
		if (Out.Num() == 0 && M && M->bAllowCPUAccess && M->GetRenderData())
		{
			const int32 Lod = FMath::Max(0, M->GetNumLODs() - 1);
			for (int32 s = 0; s < M->GetNumSections(Lod); ++s)
			{
				TArray<FVector> V; TArray<int32> Idx; TArray<FVector> Nn; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
				UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(M, Lod, s, V, Idx, Nn, UV, Tan);
				for (int32 i = 0; i + 2 < Idx.Num(); i += 3)
				{
					Out.Add({ V[Idx[i]], V[Idx[i + 1]], V[Idx[i + 2]], s });
				}
			}
		}
		if (Out.Num() == 0)
		{
			return false;
		}
		// Outward winding: the normal is (C-A)x(B-A) (the engine's front face). Collision cooking may have flipped them all.
		FVector Mid(0.f);
		for (const FTri& T : Out) { Mid += (T.A + T.B + T.C) / 3.f; }
		Mid /= Out.Num();
		double Score = 0.0;
		for (const FTri& T : Out)
		{
			const FVector Nrm = FVector::CrossProduct(T.C - T.A, T.B - T.A);
			Score += FVector::DotProduct(Nrm, (T.A + T.B + T.C) / 3.f - Mid);
		}
		if (Score < 0.0)
		{
			for (FTri& T : Out) { Swap(T.B, T.C); }
		}
		return true;
	}

	void AddTriSection(FGeo& G, const TArray<FTri>& Tris, int32 Slot)
	{
		FProcMeshSection S;
		for (const FTri& T : Tris)
		{
			if (T.Slot != Slot)
			{
				continue;
			}
			const FVector Nrm = FVector::CrossProduct(T.C - T.A, T.B - T.A).GetSafeNormal();
			if (Nrm.IsNearlyZero())
			{
				continue;
			}
			const FVector Tg = (T.B - T.A).GetSafeNormal();
			const int32 Base = S.ProcVertexBuffer.Num();
			for (const FVector& P : { T.A, T.B, T.C })
			{
				FProcMeshVertex V;
				V.Position = P;
				V.Normal = Nrm;
				V.Tangent = FProcMeshTangent(Tg, false);
				V.UV0 = FVector2D(P.X + P.Z, P.Y + P.Z) * 0.01f;
				V.Color = FColor::White;
				S.ProcVertexBuffer.Add(V);
				S.SectionLocalBox += P;
			}
			S.ProcIndexBuffer.Add(Base);
			S.ProcIndexBuffer.Add(Base + 1);
			S.ProcIndexBuffer.Add(Base + 2);
		}
		if (S.ProcIndexBuffer.Num() >= 3)
		{
			S.bEnableCollision = false;
			G.Box += S.SectionLocalBox;
			G.Sections.Add(MoveTemp(S));
			G.Slot.Add(Slot);
		}
	}

	FBox BoxOf(UProceduralMeshComponent* P)
	{
		FBox B(ForceInit);
		for (int32 i = 0; i < P->GetNumSections(); ++i)
		{
			const FProcMeshSection* S = P->GetProcMeshSection(i);
			if (S && S->ProcVertexBuffer.Num() >= 3 && S->ProcIndexBuffer.Num() >= 3)
			{
				B += S->SectionLocalBox;
			}
		}
		return B;
	}
}

TSharedPtr<const FGeo> AntWreckGeo::Base(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}
	const FString Key = Mesh->GetPathName();
	if (const TSharedPtr<const FGeo>* F = GBase.Find(Key))
	{
		return *F;
	}
	TSharedPtr<FGeo> G = MakeShared<FGeo>();
	TArray<FTri> Tris;
	if (ReadTris(Mesh, Tris))
	{
		TSet<int32> Slots;
		for (const FTri& T : Tris) { Slots.Add(T.Slot); }
		TArray<int32> Sorted = Slots.Array();
		Sorted.Sort();
		for (int32 S : Sorted) { AddTriSection(*G, Tris, S); }
	}
	if (!G->IsValid())
	{
		// No triangles at all: its bounds as a box.
		const FBox B = Mesh->GetBoundingBox();
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
		UKismetProceduralMeshLibrary::GenerateBoxMesh(B.GetExtent(), V, T, N, UV, Tan);
		FProcMeshSection S;
		for (int32 i = 0; i < V.Num(); ++i)
		{
			FProcMeshVertex PV; PV.Position = V[i] + B.GetCenter(); PV.Normal = N[i]; PV.UV0 = UV[i]; PV.Color = FColor::White;
			S.ProcVertexBuffer.Add(PV); S.SectionLocalBox += PV.Position;
		}
		for (int32 I : T) { S.ProcIndexBuffer.Add(I); }
		G->Box += S.SectionLocalBox;
		G->Sections.Add(S);
		G->Slot.Add(0);
	}
	const int32 RenderTris = Mesh->GetNumLODs() > 0 ? Mesh->GetNumTriangles(0) : 0;
	G->bProxy = Tris.Num() == 0 || (RenderTris > 0 && Tris.Num() < RenderTris / 2);
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK geo %s: %d collision triangles (drawn %d)%s, %d slots"), *Mesh->GetName(), Tris.Num(), RenderTris, G->bProxy ? TEXT(" - a stand-in, its chunks are not shown in place") : TEXT(""), G->Sections.Num());
	GBase.Add(Key, G);
	return G;
}

const TArray<TSharedPtr<const FGeo>>& AntWreckGeo::Cut(AActor* Scratch, UStaticMesh* Mesh, int32 Seed, int32 Count)
{
	static const TArray<TSharedPtr<const FGeo>> None;
	if (!Mesh || !Scratch)
	{
		return None;
	}
	const FString Key = FString::Printf(TEXT("%s|%d|%d"), *Mesh->GetPathName(), Seed, Count);
	if (const TArray<TSharedPtr<const FGeo>>* F = GCuts.Find(Key))
	{
		return *F;
	}
	TArray<TSharedPtr<const FGeo>>& Out = GCuts.Add(Key);
	TSharedPtr<const FGeo> Src = Base(Mesh);
	if (!Src.IsValid())
	{
		return Out;
	}
	UMaterialInterface* BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
	TArray<UMaterialInterface*> Markers;
	for (int32 i = 0; i < Src->Sections.Num(); ++i) { Markers.Add(UMaterialInstanceDynamic::Create(BaseMat, Scratch)); }
	UMaterialInterface* CapMat = UMaterialInstanceDynamic::Create(BaseMat, Scratch);
	UProceduralMeshComponent* Root = NewObject<UProceduralMeshComponent>(Scratch);
	for (int32 i = 0; i < Src->Sections.Num(); ++i)
	{
		Root->SetProcMeshSection(i, Src->Sections[i]);
		Root->SetMaterial(i, Markers[i]);
	}
	TArray<UProceduralMeshComponent*> Parts;
	Parts.Add(Root);
	FRandomStream Rng(Seed);
	int32 Fails = 0;
	while (Parts.Num() < Count && Fails < 8)
	{
		int32 Best = INDEX_NONE;
		double BestVolume = 0.0;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const FBox B = BoxOf(Parts[i]);
			if (B.IsValid && B.GetVolume() > BestVolume) { BestVolume = B.GetVolume(); Best = i; }
		}
		if (Best == INDEX_NONE)
		{
			break;
		}
		const FBox B = BoxOf(Parts[Best]);
		const FVector E = B.GetExtent();
		const FVector Long = E.X >= E.Y && E.X >= E.Z ? FVector::ForwardVector : E.Y >= E.Z ? FVector::RightVector : FVector::UpVector;
		const FVector Normal = (Long + Rng.VRand() * 0.6f).GetSafeNormal();
		const FVector Pos = B.GetCenter() + FVector(Rng.FRandRange(-0.25f, 0.25f) * E.X, Rng.FRandRange(-0.25f, 0.25f) * E.Y, Rng.FRandRange(-0.25f, 0.25f) * E.Z);
		UProceduralMeshComponent* Other = nullptr;
		UKismetProceduralMeshLibrary::SliceProceduralMesh(Parts[Best], Pos, Normal, true, Other, EProcMeshSliceCapOption::CreateNewSectionForCap, CapMat);
		if (Other && BoxOf(Other).IsValid && BoxOf(Parts[Best]).IsValid)
		{
			Parts.Add(Other);
		}
		else
		{
			++Fails;
			if (Other) { Other->DestroyComponent(); }
		}
	}
	for (UProceduralMeshComponent* P : Parts)
	{
		TSharedPtr<FGeo> C = MakeShared<FGeo>();
		for (int32 i = 0; i < P->GetNumSections(); ++i)
		{
			const FProcMeshSection* S = P->GetProcMeshSection(i);
			if (S && S->ProcVertexBuffer.Num() >= 3 && S->ProcIndexBuffer.Num() >= 3)
			{
				C->Sections.Add(*S);
				C->Sections.Last().bEnableCollision = false;
				UMaterialInterface* Mi = P->GetMaterial(i);
				C->Slot.Add(Mi == CapMat ? -1 : Src->Slot.IsValidIndex(Markers.IndexOfByKey(Mi)) ? Src->Slot[Markers.IndexOfByKey(Mi)] : -1);
				C->Box += S->SectionLocalBox;
			}
		}
		if (C->IsValid())
		{
			Out.Add(C);
		}
		P->DestroyComponent();
	}
	if (Out.Num() == 0)
	{
		Out.Add(Src);
	}
	return Out;
}

const TArray<FBox>& AntWreckGeo::Hull(UStaticMesh* Mesh, const FVector& Scale)
{
	static const TArray<FBox> None;
	if (!Mesh)
	{
		return None;
	}
	const FString Key = FString::Printf(TEXT("%s|%.2f|%.2f|%.2f"), *Mesh->GetPathName(), Scale.X, Scale.Y, Scale.Z);
	if (const TArray<FBox>* F = GHulls.Find(Key))
	{
		return *F;
	}
	TArray<FBox>& Out = GHulls.Add(Key);
	TArray<FTri> Tris;
	const FBox B = Mesh->GetBoundingBox();
	if (!ReadTris(Mesh, Tris) || !B.IsValid)
	{
		Out.Add(B);
		return Out;
	}
	const FVector Size = B.GetSize() * Scale.GetAbs();
	int32 Nx = FMath::Clamp(FMath::RoundToInt(Size.X / 300.f), 1, 5);
	int32 Ny = FMath::Clamp(FMath::RoundToInt(Size.Y / 300.f), 1, 5);
	int32 Nz = FMath::Clamp(FMath::RoundToInt(Size.Z / 300.f), 1, 5);
	for (int32 Try = 0; Try < 14; ++Try)
	{
		Out.Reset();
		const FVector Cell = B.GetSize() / FVector(Nx, Ny, Nz);
		TArray<FBox> Tight;
		Tight.Init(FBox(ForceInit), Nx * Ny * Nz);
		auto Idx = [&](int32 x, int32 y, int32 z) { return (z * Ny + y) * Nx + x; };
		for (const FTri& T : Tris)
		{
			FBox TB(ForceInit);
			TB += T.A; TB += T.B; TB += T.C;
			const FIntVector Lo(FMath::Clamp((int32)((TB.Min.X - B.Min.X) / Cell.X), 0, Nx - 1), FMath::Clamp((int32)((TB.Min.Y - B.Min.Y) / Cell.Y), 0, Ny - 1), FMath::Clamp((int32)((TB.Min.Z - B.Min.Z) / Cell.Z), 0, Nz - 1));
			const FIntVector Hi(FMath::Clamp((int32)((TB.Max.X - B.Min.X) / Cell.X), 0, Nx - 1), FMath::Clamp((int32)((TB.Max.Y - B.Min.Y) / Cell.Y), 0, Ny - 1), FMath::Clamp((int32)((TB.Max.Z - B.Min.Z) / Cell.Z), 0, Nz - 1));
			for (int32 z = Lo.Z; z <= Hi.Z; ++z) for (int32 y = Lo.Y; y <= Hi.Y; ++y) for (int32 x = Lo.X; x <= Hi.X; ++x)
			{
				const FBox C(B.Min + Cell * FVector(x, y, z), B.Min + Cell * FVector(x + 1, y + 1, z + 1));
				const FBox Clip = C.Overlap(TB);
				if (Clip.IsValid) { Tight[Idx(x, y, z)] += Clip; }
			}
		}
		// Greedy: grow each free cell along x, then y, then z while the whole slab is occupied.
		TArray<bool> Used;
		Used.Init(false, Tight.Num());
		auto Occ = [&](int32 x, int32 y, int32 z) { return Tight[Idx(x, y, z)].IsValid && !Used[Idx(x, y, z)]; };
		for (int32 z = 0; z < Nz; ++z) for (int32 y = 0; y < Ny; ++y) for (int32 x = 0; x < Nx; ++x)
		{
			if (!Occ(x, y, z)) { continue; }
			int32 X1 = x, Y1 = y, Z1 = z;
			while (X1 + 1 < Nx && Occ(X1 + 1, y, z)) { ++X1; }
			auto RowOk = [&](int32 yy, int32 zz) { for (int32 xx = x; xx <= X1; ++xx) { if (!Occ(xx, yy, zz)) { return false; } } return true; };
			while (Y1 + 1 < Ny && RowOk(Y1 + 1, z)) { ++Y1; }
			auto SlabOk = [&](int32 zz) { for (int32 yy = y; yy <= Y1; ++yy) { if (!RowOk(yy, zz)) { return false; } } return true; };
			while (Z1 + 1 < Nz && SlabOk(Z1 + 1)) { ++Z1; }
			FBox U(ForceInit);
			for (int32 zz = z; zz <= Z1; ++zz) for (int32 yy = y; yy <= Y1; ++yy) for (int32 xx = x; xx <= X1; ++xx) { U += Tight[Idx(xx, yy, zz)]; Used[Idx(xx, yy, zz)] = true; }
			// Thin walls get some thickness (a board is not a sheet of paper for the solver).
			const FVector Min3 = FVector(25.f) / Scale.GetAbs().ComponentMax(FVector(0.01f));
			FVector Ext = U.GetExtent().ComponentMax(Min3);
			Out.Add(FBox(U.GetCenter() - Ext * 0.97f, U.GetCenter() + Ext * 0.97f));
		}
		if (Out.Num() <= 24)
		{
			break;
		}
		// Too many boxes: coarser along the longest axis.
		if (Nx >= Ny && Nx >= Nz && Nx > 1) { --Nx; } else if (Ny >= Nz && Ny > 1) { --Ny; } else if (Nz > 1) { --Nz; } else { break; }
	}
	if (Out.Num() == 0 || Out.Num() > 24)
	{
		Out.Reset();
		Out.Add(B);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK hull %s: %d boxes"), *Mesh->GetName(), Out.Num());
	return Out;
}

int32 AntWreckGeo::ChunkCount(float WorldSize)
{
	return WorldSize < 250.f ? 3 : WorldSize < 700.f ? 4 : WorldSize < 1500.f ? 5 : 6;
}

void AntWreckGeo::Dent(FProcMeshSection& S, const FVector& At, const FVector& Dir, float Radius, float Depth)
{
	if (Radius <= 0.f)
	{
		return;
	}
	for (FProcMeshVertex& V : S.ProcVertexBuffer)
	{
		const float D = (float)FVector::Dist(V.Position, At);
		if (D < Radius)
		{
			const float K = FMath::Square(1.f - D / Radius);
			V.Position += Dir * (Depth * K);
		}
	}
}
