#include "AntState.h"
#include "AntKitchenGameMode.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

namespace
{
	using namespace AntState;

	struct FView
	{
		float V[NumSources][Count];
		TArray<FSpot> Spots[NumSources];
		FView() { FMemory::Memzero(V, sizeof(V)); }
		bool Any() const
		{
			for (int32 s = 0; s < NumSources; ++s)
			{
				if (Spots[s].Num() > 0) { return true; }
				for (int32 k = 0; k < Count; ++k) { if (V[s][k] > 0.f) { return true; } }
			}
			return false;
		}
	};

	TMap<TWeakObjectPtr<UPrimitiveComponent>, FView> GViews;

	float ScaleOf(const UPrimitiveComponent* C)
	{
		const FVector S = C->GetComponentScale().GetAbs();
		return FMath::Max(0.001f, (float)(S.X + S.Y + S.Z) / 3.f);
	}

	float Cpd(const UPrimitiveComponent* C, int32 i)
	{
		const TArray<float>& D = C->GetCustomPrimitiveData().Data;
		return D.IsValidIndex(i) ? D[i] : 0.f;
	}

	void Put4(UPrimitiveComponent* C, int32 i, const FVector4& V)
	{
		if (FMath::IsNearlyEqual(Cpd(C, i), (float)V.X, 0.002f) && FMath::IsNearlyEqual(Cpd(C, i + 1), (float)V.Y, 0.002f)
			&& FMath::IsNearlyEqual(Cpd(C, i + 2), (float)V.Z, 0.002f) && FMath::IsNearlyEqual(Cpd(C, i + 3), (float)V.W, 0.002f)) { return; }
		C->SetCustomPrimitiveDataVector4(i, V);
	}

	/** Shown overall value of a kind. */
	float Shown(const FView& V, int32 k)
	{
		float M = 0.f;
		for (int32 s = 0; s < NumSources; ++s) { M = FMath::Max(M, V.V[s][k]); }
		return FMath::Clamp(M, 0.f, 1.f);
	}

	/** Writes the view into the component's custom primitive data (only what changed). */
	void Write(UPrimitiveComponent* C, const FView& V)
	{
		if (!C) { return; }
		Put4(C, 0, FVector4(Shown(V, 0), Shown(V, 1), Shown(V, 2), Shown(V, 3)));
		Put4(C, 4, FVector4(Shown(V, 4), Shown(V, 5), Shown(V, 6), Shown(V, 7)));
		Put4(C, 8, FVector4(Shown(V, 8), Shown(V, 9), Shown(V, 10), Shown(V, 11)));
		Put4(C, 12, FVector4(Shown(V, 12), 0.f, 0.f, 0.f));
		// the spots of every source, strongest (strength x size) first
		TArray<const FSpot*, TInlineAllocator<16>> All;
		for (int32 s = 0; s < NumSources; ++s) { for (const FSpot& Sp : V.Spots[s]) { if (Sp.Strength > 0.01f && Sp.Radius > 0.5f) { All.Add(&Sp); } } }
		All.Sort([](const FSpot& A, const FSpot& B) { return A.Strength * FMath::Sqrt(A.Radius) > B.Strength * FMath::Sqrt(B.Radius); });
		const float Sc = ScaleOf(C);
		for (int32 i = 0; i < MaxSpots; ++i)
		{
			FVector4 P(0.f, 0.f, 0.f, 0.f);
			if (All.IsValidIndex(i))
			{
				const FSpot& S = *All[i];
				const float RQ = FMath::Clamp(FMath::RoundToFloat(S.Radius / Sc * 4.f), 1.f, 16000.f);
				const float K = (float)FMath::Min<uint8>(S.Kind, Count - 1);
				const float St = FMath::Clamp(FMath::RoundToFloat(S.Strength * 63.f), 1.f, 63.f);
				P = FVector4(S.Local.X, S.Local.Y, S.Local.Z, RQ * 1024.f + K * 64.f + St);
			}
			Put4(C, SpotData + i * 4, P);
		}
	}

	FView* ViewOf(UPrimitiveComponent* C, bool bCreate)
	{
		if (!C) { return nullptr; }
		if (FView* V = GViews.Find(C)) { return V; }
		if (!bCreate) { return nullptr; }
		if (GViews.Num() > 1500)
		{
			for (auto It = GViews.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
		}
		return &GViews.Add(C);
	}

	void Commit(UPrimitiveComponent* C, FView* V)
	{
		if (!C || !V) { return; }
		Write(C, *V);
		if (!V->Any()) { GViews.Remove(C); }
	}

	const FView* ViewOfConst(const UPrimitiveComponent* C)
	{
		return C ? GViews.Find(TWeakObjectPtr<UPrimitiveComponent>(const_cast<UPrimitiveComponent*>(C))) : nullptr;
	}

	float SpotValue(const UPrimitiveComponent* C, const FSpot& S, const FVector& World)
	{
		const FVector L = C->GetComponentTransform().InverseTransformPosition(World);
		const float D = (float)FVector::Dist(L, S.Local) * ScaleOf(C) / FMath::Max(1.f, S.Radius);
		return FMath::Clamp((1.f - D) * 3.f, 0.f, 1.f) * S.Strength;
	}
}

namespace AntState
{

void Set(UPrimitiveComponent* C, EKind Kind, float Value)
{
	SetSource(C, Dev, Kind, Value);
}

float Get(const UPrimitiveComponent* C, EKind Kind)
{
	const FView* V = ViewOfConst(C);
	return V && Kind < Count ? Shown(*V, Kind) : 0.f;
}

void SetSource(UPrimitiveComponent* C, ESource Source, EKind Kind, float Value)
{
	if (!C || Kind >= Count || Source >= NumSources) { return; }
	const float Val = FMath::Clamp(Value, 0.f, 1.f);
	FView* V = ViewOf(C, Val > 0.f);
	if (!V || FMath::IsNearlyEqual(V->V[Source][Kind], Val, 0.003f)) { return; }
	V->V[Source][Kind] = Val;
	Commit(C, V);
}

float GetSource(const UPrimitiveComponent* C, ESource Source, EKind Kind)
{
	const FView* V = ViewOfConst(C);
	return V && Source < NumSources && Kind < Count ? V->V[Source][Kind] : 0.f;
}

void SetSpots(UPrimitiveComponent* C, ESource Source, TConstArrayView<FSpot> Spots)
{
	if (!C || Source >= NumSources) { return; }
	FView* V = ViewOf(C, Spots.Num() > 0);
	if (!V) { return; }
	TArray<FSpot>& Mine = V->Spots[Source];
	bool bSame = Mine.Num() == Spots.Num();
	for (int32 i = 0; bSame && i < Spots.Num(); ++i)
	{
		bSame = Mine[i].Kind == Spots[i].Kind && FMath::IsNearlyEqual(Mine[i].Strength, Spots[i].Strength, 0.01f)
			&& FMath::IsNearlyEqual(Mine[i].Radius, Spots[i].Radius, 0.5f) && Mine[i].Local.Equals(Spots[i].Local, 0.05f);
	}
	if (bSame) { return; }
	Mine.Reset(Spots.Num());
	Mine.Append(Spots.GetData(), Spots.Num());
	Commit(C, V);
}

void AddSpot(UPrimitiveComponent* C, ESource Source, const FVector& World, float Radius, EKind Kind, float Strength)
{
	if (!C || Source >= NumSources || Kind >= Count || Radius <= 0.f || Strength <= 0.f) { return; }
	FView* V = ViewOf(C, true);
	TArray<FSpot>& Mine = V->Spots[Source];
	const FVector L = C->GetComponentTransform().InverseTransformPosition(World);
	const float Sc = ScaleOf(C);
	for (FSpot& S : Mine)
	{
		if (S.Kind != Kind) { continue; }
		const float D = (float)FVector::Dist(S.Local, L) * Sc;
		if (D < FMath::Max(S.Radius, Radius) * 0.7f)
		{
			S.Radius = FMath::Max(S.Radius, FMath::Min(D + Radius, S.Radius + Radius));
			S.Strength = FMath::Clamp(FMath::Max(S.Strength, Strength), 0.f, 1.f);
			Commit(C, V);
			return;
		}
	}
	FSpot& N = Mine.AddDefaulted_GetRef();
	N.Local = L;
	N.Radius = Radius;
	N.Kind = Kind;
	N.Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (Mine.Num() > 8)
	{
		Mine.Sort([](const FSpot& A, const FSpot& B) { return A.Strength * A.Radius > B.Strength * B.Radius; });
		Mine.SetNum(8);
	}
	Commit(C, V);
}

void ClearSource(UPrimitiveComponent* C, ESource Source)
{
	FView* V = ViewOf(C, false);
	if (!V || Source >= NumSources) { return; }
	FMemory::Memzero(V->V[Source], sizeof(V->V[Source]));
	V->Spots[Source].Reset();
	Commit(C, V);
}

void ClearSourceEverywhere(ESource Source)
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Keys;
	GViews.GetKeys(Keys);
	for (const TWeakObjectPtr<UPrimitiveComponent>& K : Keys)
	{
		if (K.IsValid()) { ClearSource(K.Get(), Source); }
		else { GViews.Remove(K); }
	}
}

float ValueAt(const UPrimitiveComponent* C, const FVector& World, EKind Kind)
{
	const FView* V = ViewOfConst(C);
	if (!V || Kind >= Count) { return 0.f; }
	float M = Shown(*V, Kind);
	for (int32 s = 0; s < NumSources; ++s)
	{
		for (const FSpot& S : V->Spots[s]) { if (S.Kind == Kind) { M = FMath::Max(M, SpotValue(C, S, World)); } }
	}
	return M;
}

int32 SetAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Value)
{
	int32 N = 0;
	if (!World) { return 0; }
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* C = *It;
		if (!C || C->GetWorld() != World || !C->IsRegistered() || !C->GetStaticMesh()) { continue; }
		if (FVector::Dist(C->Bounds.Origin, At) > Radius) { continue; }
		Set(C, Kind, Value);
		++N;
	}
	return N;
}

int32 SpotAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Strength, ESource Source)
{
	int32 N = 0;
	if (!World) { return 0; }
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* C = *It;
		if (!C || C->GetWorld() != World || !C->IsRegistered() || !C->GetStaticMesh() || !C->IsVisible()) { continue; }
		if (C->Bounds.SphereRadius > 6000.f) { continue; }   // floors and walls: the decals of the mess draw those
		if (C->Bounds.GetBox().ComputeSquaredDistanceToPoint(At) > Radius * Radius) { continue; }
		AddSpot(C, Source, At, Radius, Kind, Strength);
		++N;
	}
	return N;
}

const TCHAR* KindName(uint8 Kind)
{
	static const TCHAR* N[] = { TEXT("wet"), TEXT("char"), TEXT("dirt"), TEXT("damage"), TEXT("heat"), TEXT("burn"), TEXT("frost"), TEXT("charge"),
		TEXT("oil"), TEXT("jam"), TEXT("cola"), TEXT("honey"), TEXT("spoil") };
	return Kind < UE_ARRAY_COUNT(N) ? N[Kind] : TEXT("?");
}

int32 KindFromName(const FString& Name)
{
	const FString L = Name.ToLower();
	for (int32 k = 0; k < Count; ++k) { if (L == KindName(k)) { return k; } }
	if (L == TEXT("dmg")) { return Damage; }
	if (L == TEXT("dirty")) { return Dirt; }
	if (L == TEXT("burning") || L == TEXT("fire")) { return Burn; }
	if (L == TEXT("charred")) { return Char; }
	if (L.IsNumeric()) { const int32 K = FCString::Atoi(*L); return K >= 0 && K < Count ? K : -1; }
	return -1;
}

int32 NumTracked()
{
	int32 N = 0;
	for (const auto& It : GViews) { if (It.Key.IsValid()) { ++N; } }
	return N;
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || A[0].ToLower() != TEXT("state")) { return false; }
	const FString Sub = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	UWorld* W = GM->GetWorld();
	if (Sub == TEXT("spot"))
	{
		// state spot <kind> <v> <x> <y> <z> [r]
		const int32 Kind = A.IsValidIndex(2) ? KindFromName(A[2]) : -1;
		if (Kind < 0) { return false; }
		const FVector At(Num(4), Num(5), Num(6));
		const int32 N = SpotAround(W, At, Num(7, 200.f), (EKind)Kind, Num(3, 1.f), Dev);
		UE_LOG(LogTemp, Display, TEXT("ANTSTATE spot %s %.2f at %s r %.0f: %d components"), KindName(Kind), Num(3, 1.f), *At.ToCompactString(), Num(7, 200.f), N);
		return true;
	}
	if (Sub == TEXT("info"))
	{
		const FVector At(Num(2), Num(3), Num(4));
		const float R = Num(5, 300.f);
		int32 N = 0;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* C = *It;
			if (!C || C->GetWorld() != W || !C->IsRegistered() || C->Bounds.GetBox().ComputeSquaredDistanceToPoint(At) > R * R) { continue; }
			const FView* V = ViewOfConst(C);
			if (!V) { continue; }
			FString S;
			for (int32 k = 0; k < Count; ++k) { const float X = Shown(*V, k); if (X > 0.f) { S += FString::Printf(TEXT(" %s %.2f"), KindName(k), X); } }
			int32 Sp = 0;
			for (int32 s = 0; s < NumSources; ++s) { Sp += V->Spots[s].Num(); }
			UE_LOG(LogTemp, Display, TEXT("ANTSTATE info %s/%s:%s, %d spots"), *GetNameSafe(C->GetOwner()), *C->GetName(), *S, Sp);
			if (++N >= 40) { break; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTSTATE info at %s r %.0f: %d components with states (%d tracked)"), *At.ToCompactString(), R, N, NumTracked());
		return true;
	}
	const FVector At(Num(3), Num(4), Num(5));
	const float R = Num(6, 300.f);
	int32 N = 0;
	if (Sub == TEXT("clear"))
	{
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* C = *It;
			if (!C || C->GetWorld() != W || FVector::Dist(C->Bounds.Origin, At) > R) { continue; }
			ClearSource(C, Dev);
			++N;
		}
	}
	else
	{
		const int32 Kind = KindFromName(Sub);
		if (Kind < 0) { return false; }
		N = SetAround(W, At, R, (EKind)Kind, Num(2, 1.f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSTATE %s %.2f at %s r %.0f: %d components"), *Sub, Num(2, 1.f), *At.ToCompactString(), R, N);
	return true;
}

}
