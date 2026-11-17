#include "KitchenShards.h"
#include "AntTypes.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"

namespace
{
	using AntShards::FChunk;

	TMap<FString, TSharedPtr<FChunk>> GBase;                  // whole models
	TMap<FString, TArray<TSharedPtr<FChunk>>> GCuts;          // one cut = its chunks

	void AddSection(FChunk& Out, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& UV, const TArray<FProcMeshTangent>& Tan)
	{
		if (V.Num() < 3 || T.Num() < 3)
		{
			return;
		}
		FProcMeshSection S;
		S.ProcVertexBuffer.Reserve(V.Num());
		for (int32 i = 0; i < V.Num(); ++i)
		{
			FProcMeshVertex PV;
			PV.Position = V[i];
			PV.Normal = N.IsValidIndex(i) ? N[i] : FVector::UpVector;
			PV.UV0 = UV.IsValidIndex(i) ? UV[i] : FVector2D::ZeroVector;
			PV.Tangent = Tan.IsValidIndex(i) ? Tan[i] : FProcMeshTangent();
			PV.Color = FColor::White;
			S.ProcVertexBuffer.Add(PV);
			S.SectionLocalBox += V[i];
		}
		S.ProcIndexBuffer.Reserve(T.Num());
		for (int32 Index : T)
		{
			S.ProcIndexBuffer.Add((uint32)Index);
		}
		S.bEnableCollision = false;
		Out.Box += S.SectionLocalBox;
		Out.Sections.Add(MoveTemp(S));
		Out.Cap.Add(false);
	}

	/** A ball of radius 50 (the size of the engine's basic shapes), facing outwards whatever the winding rules are. */
	void MakeBall(TArray<FVector>& V, TArray<int32>& T, TArray<FVector>& N, TArray<FVector2D>& UV)
	{
		const int32 Seg = 14, Rings = 9;
		for (int32 r = 0; r <= Rings; ++r)
		{
			const float Phi = PI * r / Rings;
			for (int32 s = 0; s <= Seg; ++s)
			{
				const float Th = 2.f * PI * s / Seg;
				const FVector Dir(FMath::Sin(Phi) * FMath::Cos(Th), FMath::Sin(Phi) * FMath::Sin(Th), FMath::Cos(Phi));
				V.Add(Dir * 50.f);
				N.Add(Dir);
				UV.Add(FVector2D((float)s / Seg, (float)r / Rings));
			}
		}
		auto Tri = [&](int32 A, int32 B, int32 C)
		{
			const FVector Face = FVector::CrossProduct(V[C] - V[A], V[B] - V[A]);
			if (Face.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				return;   // the poles
			}
			if (FVector::DotProduct(Face, V[A] + V[B] + V[C]) < 0.f)
			{
				Swap(B, C);
			}
			T.Add(A); T.Add(B); T.Add(C);
		};
		for (int32 r = 0; r < Rings; ++r)
		{
			for (int32 s = 0; s < Seg; ++s)
			{
				const int32 A = r * (Seg + 1) + s, B = A + 1, C = A + Seg + 1, D = C + 1;
				Tri(A, B, C);
				Tri(B, D, C);
			}
		}
	}

	TSharedPtr<FChunk> BuildBase(UStaticMesh* Art, uint8 Shape)
	{
		TSharedPtr<FChunk> Out = MakeShared<FChunk>();
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
		if (Art)
		{
			if (!Art->bAllowCPUAccess || !Art->GetRenderData())
			{
				return nullptr;
			}
			const int32 Lod = FMath::Max(0, Art->GetNumLODs() - 1);
			const int32 NumSections = Art->GetNumSections(Lod);
			for (int32 i = 0; i < NumSections; ++i)
			{
				V.Reset(); T.Reset(); N.Reset(); UV.Reset(); Tan.Reset();
				UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Art, Lod, i, V, T, N, UV, Tan);
				AddSection(*Out, V, T, N, UV, Tan);
			}
		}
		else if ((EAntShape)Shape == EAntShape::Cube)
		{
			UKismetProceduralMeshLibrary::GenerateBoxMesh(FVector(50.f), V, T, N, UV, Tan);
			AddSection(*Out, V, T, N, UV, Tan);
		}
		else
		{
			MakeBall(V, T, N, UV);
			AddSection(*Out, V, T, N, UV, Tan);
		}
		return Out->IsValid() ? Out : nullptr;
	}

	FBox BoxOf(UProceduralMeshComponent* P)
	{
		FBox B(ForceInit);
		for (int32 i = 0; i < P->GetNumSections(); ++i)
		{
			const FProcMeshSection* S = P->GetProcMeshSection(i);
			if (S && S->ProcVertexBuffer.Num() >= 3)
			{
				B += S->SectionLocalBox;
			}
		}
		return B;
	}

	/** Cuts Src into (up to) Count chunks. The scratch components never reach the screen: they die before the frame ends. */
	TArray<TSharedPtr<FChunk>> Cut(AActor* Owner, const FChunk& Src, int32 Seed, int32 Count)
	{
		TArray<TSharedPtr<FChunk>> Out;
		UMaterialInterface* BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
		UMaterialInterface* CapMat = UMaterialInstanceDynamic::Create(BaseMat, Owner);   // only a marker: "this section is a cut face"
		UProceduralMeshComponent* Root = NewObject<UProceduralMeshComponent>(Owner);
		for (int32 i = 0; i < Src.Sections.Num(); ++i)
		{
			Root->SetProcMeshSection(i, Src.Sections[i]);
			Root->SetMaterial(i, Src.Cap[i] ? CapMat : BaseMat);
		}
		TArray<UProceduralMeshComponent*> Parts;
		Parts.Add(Root);
		FRandomStream Rng(Seed);
		int32 Fails = 0;
		while (Parts.Num() < Count && Fails < 6)
		{
			int32 Best = INDEX_NONE;
			double BestVolume = 0.0;
			for (int32 i = 0; i < Parts.Num(); ++i)
			{
				const FBox B = BoxOf(Parts[i]);
				if (B.IsValid && B.GetVolume() > BestVolume)
				{
					BestVolume = B.GetVolume();
					Best = i;
				}
			}
			if (Best == INDEX_NONE)
			{
				break;
			}
			const FBox B = BoxOf(Parts[Best]);
			const FVector E = B.GetExtent();
			// Mostly across the long side (that is how things snap), with a random tilt.
			const FVector Long = E.X >= E.Y && E.X >= E.Z ? FVector::ForwardVector : E.Y >= E.Z ? FVector::RightVector : FVector::UpVector;
			const FVector Normal = (Long + Rng.VRand() * 0.75f).GetSafeNormal();
			const FVector Pos = B.GetCenter() + FVector(Rng.FRandRange(-0.22f, 0.22f) * E.X, Rng.FRandRange(-0.22f, 0.22f) * E.Y, Rng.FRandRange(-0.22f, 0.22f) * E.Z);
			UProceduralMeshComponent* Other = nullptr;
			UKismetProceduralMeshLibrary::SliceProceduralMesh(Parts[Best], Pos, Normal, true, Other, EProcMeshSliceCapOption::CreateNewSectionForCap, CapMat);
			if (Other && BoxOf(Other).IsValid && BoxOf(Parts[Best]).IsValid)
			{
				Parts.Add(Other);
			}
			else
			{
				++Fails;
				if (Other)
				{
					Other->DestroyComponent();
				}
			}
		}
		for (UProceduralMeshComponent* P : Parts)
		{
			TSharedPtr<FChunk> C = MakeShared<FChunk>();
			for (int32 i = 0; i < P->GetNumSections(); ++i)
			{
				const FProcMeshSection* S = P->GetProcMeshSection(i);
				if (S && S->ProcVertexBuffer.Num() >= 3 && S->ProcIndexBuffer.Num() >= 3)
				{
					C->Sections.Add(*S);
					C->Sections.Last().bEnableCollision = false;
					C->Cap.Add(P->GetMaterial(i) == CapMat);
					C->Box += S->SectionLocalBox;
				}
			}
			Out.Add(C);
			P->DestroyComponent();
		}
		return Out;
	}
}

TSharedPtr<const AntShards::FChunk> AntShards::Get(AActor* Owner, UStaticMesh* ArtMesh, uint8 Shape, int32 Seed, const TArray<uint8>& Path)
{
	if (!Owner || Path.Num() < 2)
	{
		return nullptr;
	}
	FString Key = ArtMesh ? ArtMesh->GetPathName() : FString::Printf(TEXT("shape%d"), (int32)Shape);
	TSharedPtr<FChunk> Cur;
	if (TSharedPtr<FChunk>* Found = GBase.Find(Key))
	{
		Cur = *Found;
	}
	else
	{
		Cur = BuildBase(ArtMesh, Shape);
		GBase.Add(Key, Cur);
	}
	for (int32 Level = 0; Cur.IsValid() && Level * 2 + 1 < Path.Num(); ++Level)
	{
		const int32 Index = Path[Level * 2];
		const int32 Count = FMath::Clamp((int32)Path[Level * 2 + 1], 2, 8);
		Key += FString::Printf(TEXT("|%d:%d"), Seed + 7919 * Level, Count);
		if (!GCuts.Contains(Key))
		{
			if (GCuts.Num() > 48)
			{
				GCuts.Empty();   // chunks in use stay alive through their shared pointers
			}
			GCuts.Add(Key, Cut(Owner, *Cur, Seed + 7919 * Level, Count));
		}
		const TArray<TSharedPtr<FChunk>>& Chunks = GCuts[Key];
		Cur = Chunks.IsValidIndex(Index) && Chunks[Index].IsValid() && Chunks[Index]->IsValid() ? Chunks[Index] : nullptr;
		Key += FString::Printf(TEXT("/%d"), Index);
	}
	return Cur;
}