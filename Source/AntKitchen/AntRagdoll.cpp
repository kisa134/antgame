#include "AntRagdoll.h"
#include "AntCharacter.h"
#include "AntTypes.h"
#include "AntGibs.h"
#include "NpcBody.h"
#include "KitchenLoot.h"
#include "HumanCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

#include "AntBodyData.inl"

namespace
{
	const FLinearColor Hemolymph(0.62f, 0.75f, 0.12f);
	const float Gravity = -1400.f;

	TArray<TWeakObjectPtr<AAntCorpse>>& AllCorpses()
	{
		static TArray<TWeakObjectPtr<AAntCorpse>> All;   // weak: a map reload just leaves stale entries, pruned on the next use
		return All;
	}
	inline float Smooth01(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	inline int32 LegPt(int32 Leg, int32 Joint) { return 9 + Leg * 3 + Joint; }          // 0 knee, 1 ankle, 2 toe
	inline int32 FeelPt(int32 Side, int32 Joint) { return 9 + 18 + Side * 3 + Joint; }   // 0 elbow, 1 middle, 2 tip
}

AAntCorpse::AAntCorpse()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AAntCorpse::Register(AAntCorpse* C)
{
	TArray<TWeakObjectPtr<AAntCorpse>>& All = AllCorpses();
	All.RemoveAll([](const TWeakObjectPtr<AAntCorpse>& W) { return !W.IsValid(); });
	All.Add(C);
	// Only so many may lie about, only so many may simulate: the oldest go first.
	int32 Bodies = 0, Legs = 0, Awake = 0;
	for (const TWeakObjectPtr<AAntCorpse>& W : All)
	{
		if (W->GetWorld() != C->GetWorld()) { continue; }
		if (W->bBody) { ++Bodies; } else { ++Legs; }
		if (!W->bAsleep && !W->bFlat) { ++Awake; }
	}
	for (int32 i = 0; i < All.Num() && (Bodies > MaxCorpses || Legs > MaxLegs || Awake > MaxAwake); ++i)
	{
		AAntCorpse* O = All[i].Get();
		if (!O || O == C || O->GetWorld() != C->GetWorld() || O->Riding.IsValid())
		{
			continue;
		}
		if (O->bBody && Bodies > MaxCorpses) { --Bodies; if (!O->bAsleep && !O->bFlat) { --Awake; } O->Destroy(); }
		else if (!O->bBody && Legs > MaxLegs) { --Legs; if (!O->bAsleep) { --Awake; } O->Destroy(); }
		else if (!O->bAsleep && !O->bFlat && Awake > MaxAwake && O->Age > 1.f) { --Awake; O->Sleep(); }
	}
}

bool AAntCorpse::NewestAt(const UWorld* World, FVector& Out)
{
	const TArray<TWeakObjectPtr<AAntCorpse>>& All = AllCorpses();
	for (int32 i = All.Num() - 1; i >= 0; --i)
	{
		if (All[i].IsValid() && All[i]->GetWorld() == World && All[i]->bBody)
		{
			Out = All[i]->bFlat ? All[i]->GetActorLocation() : All[i]->ThoraxO;
			return true;
		}
	}
	return false;
}

FString AAntCorpse::Stats(const UWorld* World)
{
	int32 Bodies = 0, Legs = 0, Awake = 0, Riding = 0;
	for (const TWeakObjectPtr<AAntCorpse>& W : AllCorpses())
	{
		if (!W.IsValid() || W->GetWorld() != World) { continue; }
		if (W->bBody) { ++Bodies; } else { ++Legs; }
		if (!W->bAsleep && !W->bFlat) { ++Awake; }
		if (W->Riding.IsValid()) { ++Riding; }
	}
	return FString::Printf(TEXT("bodies %d legs %d awake %d riding %d"), Bodies, Legs, Awake, Riding);
}

AAntCorpse* AAntCorpse::Spawn(UWorld* World, const FAntBodySnapshot& Pose, uint8 InKind, const FVector& Dir, AAntCharacter* InOf)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	AAntCorpse* C = World->SpawnActor<AAntCorpse>(AAntCorpse::StaticClass(), FTransform(Pose.Thorax.GetLocation()));
	if (!C)
	{
		return nullptr;
	}
	C->Kind = InKind;
	C->Of = InOf;
	C->bHadOwner = InOf != nullptr;
	C->OfLastAt = InOf ? InOf->GetActorLocation() : Pose.Thorax.GetLocation();
	C->Seed = InOf ? InOf->GetUniqueID() : 1u;
	C->Build(Pose, -1);

	const FVector Up = Pose.Up;
	const FVector D = Dir.GetSafeNormal(UE_SMALL_NUMBER, Up);
	const float K = AntKit::SpeedK;
	const FVector Vel = Pose.Velocity;
	const float Rnd = (C->Seed % 1000) / 1000.f;
	switch (InKind)
	{
	case AntDeath::Squashed:
		C->Flatten();
		C->Goo(9, 190.f, C->GetActorLocation() + Up * 2.f);
		break;
	case AntDeath::Torn:
	{
		// the waist gives: the front half goes with one biter, the gaster with the other, goo between them
		C->bTorn = true;
		const FVector Pull = D * 330.f * K;
		C->AddVelocity(T0, P0, Vel * 0.3f + Pull + Up * 240.f * K);
		C->AddVelocity(LegPts, NumPts, Vel * 0.3f + Pull + Up * 240.f * K);
		C->AddVelocity(P0, LegPts, Vel * 0.3f - Pull + Up * 240.f * K);
		C->AddSpin(T0, P0, FVector::CrossProduct(Up, D).GetSafeNormal(UE_SMALL_NUMBER, Pose.Fwd), 9.f);
		C->AddSpin(P0, LegPts, FVector::CrossProduct(Up, D).GetSafeNormal(UE_SMALL_NUMBER, Pose.Fwd), -11.f);
		for (int32 i = 0; i < 3; ++i)
		{
			UStaticMeshComponent* S = AntKit::AddPart(C, C->Root, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.01f), Hemolymph);
			S->SetCastShadow(false);
			S->SetUsingAbsoluteLocation(true);
			S->SetUsingAbsoluteRotation(true);
			S->SetUsingAbsoluteScale(true);
			C->Strands.Add(S);
		}
		C->CurlMax = 0.6f;
		C->CurlFor = 1.5f;
		C->Goo(16, 400.f, C->Pts[T1].X);
		break;
	}
	case AntDeath::Flung:
	{
		// swatted, batted, swept away: it flies, tumbles, bounces, lies still
		// (already in the air when it died: it goes on as it went; hit while it stood or ran: thrown the way the blow went)
		FVector Fly = Vel;
		if (Fly.Size() < 600.f * K && FVector::DotProduct(Fly, Up) < 100.f * K)
		{
			Fly = (FVector::VectorPlaneProject(D, Up).GetSafeNormal(UE_SMALL_NUMBER, D) * 620.f + Up * 330.f) * K;
		}
		C->AddVelocity(0, NumPts, Fly);
		C->AddSpin(0, NumPts, (FVector::CrossProduct(Up, Fly).GetSafeNormal(UE_SMALL_NUMBER, Pose.Fwd) + Pose.Fwd * (Rnd - 0.5f)).GetSafeNormal(), 13.f + 8.f * Rnd);
		C->CurlMax = 0.75f;
		C->CurlFor = 1.6f;
		C->Goo(5, 260.f, C->Pts[T1].X);
		break;
	}
	case AntDeath::Fall:
		C->AddVelocity(0, NumPts, Vel);
		C->AddSpin(0, NumPts, Pose.Fwd, 3.f * (Rnd - 0.5f));
		C->CurlMax = 0.7f;
		C->CurlFor = 1.2f;
		break;
	case AntDeath::Eaten:
		// nothing is left to see (but if the body turns up as loot, this is what lies there)
		C->bHidden = true;
		C->CurlMax = 1.f;
		C->CurlFor = 0.1f;
		C->SetActorHiddenInGame(true);
		break;
	case AntDeath::Curl:
	case AntDeath::Burnt:
	case AntDeath::Drowned:
	case AntDeath::Frozen:
		// it sways on its feet, they give way, it rolls over and the legs fold in, as dead ants lie
		C->HoldFor = InKind == AntDeath::Burnt ? 0.45f : InKind == AntDeath::Frozen ? 1.1f : 0.75f;
		C->CurlFor = InKind == AntDeath::Frozen ? 2.6f : InKind == AntDeath::Burnt ? 0.7f : 1.3f;
		C->CurlMax = 1.f;
		C->AddVelocity(0, NumPts, Vel * 0.3f);
		if (InKind == AntDeath::Burnt)
		{
			for (UMaterialInterface* M : C->KeepMats)
			{
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.16f, 0.13f, 0.11f));   // charred
				}
			}
		}
		break;
	default:
		// shot or bitten to death: knocked off its feet, the legs give way
		C->HoldFor = 0.12f;
		C->CurlFor = 1.6f;
		C->CurlMax = 0.8f;
		C->AddVelocity(0, NumPts, Vel * 0.5f + (FVector::VectorPlaneProject(D, Up) * 150.f + Up * 60.f) * K);
		C->AddSpin(0, NumPts, FVector::CrossProduct(Up, D).GetSafeNormal(UE_SMALL_NUMBER, Pose.Fwd), 3.f);
		C->Goo(4, 240.f, C->Pts[T1].X);
		break;
	}
	C->Draw();
	Register(C);
	UE_LOG(LogTemp, Display, TEXT("ANTBODY corpse of %s: kind %d dir %s speed %.0f at %s (%s)"), InOf ? *InOf->GetDisplayName() : TEXT("?"), (int32)InKind, *D.ToCompactString(), Vel.Size(),
		*C->Pts[T1].X.ToCompactString(), *Stats(World));
	return C;
}

AAntCorpse* AAntCorpse::SpawnLeg(UWorld* World, const FAntBodySnapshot& Pose, int32 Leg, const FVector& Velocity)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || Leg < 0 || Leg > 5)
	{
		return nullptr;
	}
	AAntCorpse* C = World->SpawnActor<AAntCorpse>(AAntCorpse::StaticClass(), FTransform(Pose.Hip[Leg]));
	if (!C)
	{
		return nullptr;
	}
	C->Kind = AntDeath::Collapse;
	C->Seed = (uint32)(Leg * 7919) + (uint32)(Pose.Hip[Leg].X * 13.f);
	C->Life = 40.f;
	C->Build(Pose, Leg);
	C->AddVelocity(0, NumPts, Velocity);
	C->AddSpin(0, NumPts, FMath::VRand(), 16.f);
	C->Draw();
	Register(C);
	return C;
}

void AAntCorpse::Goo(int32 Count, float Speed, const FVector& At)
{
	AAntGibs* G = GetWorld()->SpawnActor<AAntGibs>(AAntGibs::StaticClass(), FTransform::Identity);
	if (!G)
	{
		return;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		G->AddGoo(At, (FMath::VRand() * 0.9f + Up0 * 0.7f).GetSafeNormal() * FMath::FRandRange(Speed * 0.3f, Speed), FMath::FRandRange(0.6f, 1.7f) * AntKit::AntShrink * 3.f * (VS / 0.158f), Hemolymph);
	}
}

void AAntCorpse::AddVelocity(int32 From, int32 To, const FVector& Vel)
{
	// (Verlet: the velocity is the step the point made last time)
	for (int32 i = From; i < To; ++i)
	{
		Pts[i].Prev -= Vel * (1.f / 90.f);
	}
}

void AAntCorpse::AddSpin(int32 From, int32 To, const FVector& Axis, float Rate)
{
	FVector Com = FVector::ZeroVector;
	int32 Num = 0;
	for (int32 i = From; i < To; ++i)
	{
		if (Pts[i].bOn) { Com += Pts[i].X; ++Num; }
	}
	if (Num == 0)
	{
		return;
	}
	Com /= Num;
	for (int32 i = From; i < To; ++i)
	{
		if (Pts[i].bOn) { Pts[i].Prev -= FVector::CrossProduct(Axis * Rate, Pts[i].X - Com) * (1.f / 90.f); }
	}
}

// ---------------------------------------------------------------------------------------------
// The points and the pieces
// ---------------------------------------------------------------------------------------------

void AAntCorpse::Build(const FAntBodySnapshot& S, int32 OnlyLeg)
{
	const FAntBodyArt& Art = AntRig::Art();
	VS = S.VS;
	LegThick = S.LegThick;
	Up0 = S.Up;
	Fwd0 = S.Fwd;
	JawOpen = S.JawOpen;
	bBody = OnlyLeg < 0;
	for (int32 i = 0; i < AntMat::Count; ++i) { KeepMats.Add(S.Mats[i]); }
	KeepMats.Add(S.GlandMat);

	const AntArt::FPart& ArtCoxa = AntRig::Art().Coxa;
	const AntArt::FPart& ArtFemur = AntRig::Art().Femur;
	const AntArt::FPart& ArtScape = AntRig::Art().Scape;
	auto On = [this](int32 i, const FVector& X, float RadAL)
	{
		Pts[i].X = Pts[i].Prev = X;
		Pts[i].Rad = RadAL * VS;
		Pts[i].bOn = true;
	};
	auto Link = [this](int32 A, int32 B, float WA, float Scale = 1.f, bool bMin = false)
	{
		FLink L;
		L.A = A; L.B = B; L.WA = WA; L.bMin = bMin;
		L.Len = (float)FVector::Dist(Pts[A].X, Pts[B].X) * Scale;
		Core.Add(L);
	};
	LegComps.SetNumZeroed(24);
	FeelComps.SetNumZeroed(6);
	auto MakeLeg = [&](int32 i)
	{
		UStaticMeshComponent* Coxa = AntArt::AddArt(this, Root, ArtCoxa);
		UStaticMeshComponent* Femur = AntArt::AddArt(this, Root, ArtFemur);
		if (S.Mats[AntMat::Coxa]) { Coxa->SetMaterial(0, S.Mats[AntMat::Coxa]); }
		if (S.Mats[AntMat::Femur]) { Femur->SetMaterial(0, S.Mats[AntMat::Femur]); }
		LegComps[i * 4 + 0] = Coxa;
		LegComps[i * 4 + 1] = Femur;
		LegComps[i * 4 + 2] = AntRig::AddPiece(this, Root, Art.Tibia, S.Mats[AntMat::Tibia], true);
		LegComps[i * 4 + 3] = AntRig::AddPiece(this, Root, Art.Tarsus, S.Mats[AntMat::Tibia], true);
		const float Len = AntRig::PairLen(i / 2) * VS;
		LenHK[i] = FMath::Clamp((float)FVector::Dist(S.Hip[i], S.Knee[i]), (AntRig::CoxaLen + AntRig::FemurLen) * Len * 0.75f, (AntRig::CoxaLen + AntRig::FemurLen) * Len);
		LenKA[i] = AntRig::TibiaLen * Len;
		LenAE[i] = AntRig::TarsusLen * Len;
		On(LegPt(i, 0), S.Knee[i], 1.6f);
		On(LegPt(i, 1), S.Ankle[i], 1.3f);
		On(LegPt(i, 2), S.Toe[i], 1.1f);
		ToePin[i] = S.Toe[i];
	};

	if (!bBody)
	{
		// a loose leg: its hip end is just another point
		const int32 i = OnlyLeg;
		MakeLeg(i);
		On(T0, S.Hip[i], 1.8f);
		HipW[i] = S.Hip[i];
		Link(T0, LegPt(i, 0), 0.5f);
		Link(LegPt(i, 0), LegPt(i, 1), 0.5f);
		Link(LegPt(i, 1), LegPt(i, 2), 0.5f);
		Core[0].Len = LenHK[i]; Core[1].Len = LenKA[i]; Core[2].Len = LenAE[i];
		Link(T0, LegPt(i, 1), 0.5f, 0.f, true);
		Core.Last().Len = (LenHK[i] + LenKA[i]) * 0.45f;
		Link(LegPt(i, 0), LegPt(i, 2), 0.5f, 0.f, true);
		Core.Last().Len = (LenKA[i] + LenAE[i]) * 0.5f;
		RestIdx[0] = T0; RestIdx[1] = LegPt(i, 0); RestIdx[2] = LegPt(i, 2);
		Sanitize();
		return;
	}

	// ---- the body's pieces
	Thorax = AntRig::AddPiece(this, Root, Art.Thorax, S.Mats[AntMat::Body], true);
	Head = AntRig::AddPiece(this, Root, Art.Head, S.Mats[AntMat::Body], true);
	Jaw = AntRig::AddPiece(this, Head, Art.Jaw, S.Mats[AntMat::Body], false);
	Jaw->SetRelativeLocationAndRotation(AntRig::Joint(2) - AntRig::Joint(1), FRotator(-FMath::Max(JawOpen, 16.f), 0.f, 0.f));
	Petiole = AntRig::AddPiece(this, Root, Art.Petiole, S.Mats[AntMat::Body], true);
	Gaster = AntRig::AddPiece(this, Root, Art.Gaster, S.Mats[AntMat::Body], true);
	const AntArt::FPart& ArtGland = AntArt::Get(TEXT("SM_Ant_AcidGland"));
	if (ArtGland.IsValid())
	{
		Gland = AntArt::AddArt(this, Gaster, ArtGland);
		Gland->SetRelativeTransform(AntArt::Fit(ArtGland, FVector(-57.f, 0.f, 6.f) - AntRig::Joint(4), 18.f, false));
		if (S.GlandMat) { Gland->SetMaterial(0, S.GlandMat); }
	}
	for (UStaticMeshComponent* C : { Thorax, Head, Gaster }) { C->SetCastShadow(true); }

	RestIdx[0] = T0; RestIdx[1] = HD; RestIdx[2] = G1;
	// ---- the points: a stiff thorax, a head, a waist, a heavy gaster
	const FQuat TQ = S.Thorax.GetRotation();
	const FVector TO = S.Thorax.GetLocation();
	On(T0, S.Head.GetLocation(), 4.f);
	On(T1, S.Petiole.GetLocation(), 3.5f);
	On(TL, S.Thorax.TransformPosition(FVector(0.f, -5.f, -4.f)), 5.5f);
	On(TR, S.Thorax.TransformPosition(FVector(0.f, 5.f, -4.f)), 5.5f);
	On(TU, S.Thorax.TransformPosition(FVector(0.f, 0.f, 6.5f)), 5.f);
	On(HD, S.Head.TransformPosition(FVector(10.9f, 0.f, -3.9f)), 7.5f);
	On(P0, S.Petiole.GetLocation(), 3.f);
	On(G0, S.Gaster.GetLocation(), 4.f);
	On(G1, S.Gaster.TransformPosition(FVector(-18.8f, 0.f, -2.4f)), 13.5f);
	for (int32 a = T0; a <= TU; ++a)
	{
		for (int32 b = a + 1; b <= TU; ++b) { Link(a, b, 0.5f); }
	}
	Link(T0, HD, 0.6f);
	Link(T1, P0, 0.5f);          // the waist (cut when it is torn in two)
	Core.Last().Len = 0.f;
	Link(P0, G0, 0.5f);
	Link(G0, G1, 0.65f);
	// joints do not fold flat: the head cannot go through the thorax, the gaster cannot fold right under or over
	Link(HD, T1, 0.5f, 0.86f, true);
	Link(HD, TL, 0.5f, 0.78f, true);
	Link(HD, TR, 0.5f, 0.78f, true);
	Link(HD, TU, 0.5f, 0.7f, true);
	Link(G0, T0, 0.5f, 0.93f, true);
	Link(G1, T1, 0.6f, 0.85f, true);
	Link(G1, TL, 0.6f, 0.85f, true);
	Link(G1, TR, 0.6f, 0.85f, true);
	Link(G1, TU, 0.6f, 0.8f, true);

	const FQuat Frame0 = FRotationMatrix::MakeFromXY(Pts[T0].X - Pts[T1].X, Pts[TR].X - Pts[TL].X).ToQuat();
	RelThorax = Frame0.Inverse() * TQ;
	const FVector TUp = TQ.GetAxisZ();
	RelHead = NpcRig::AlongX(Pts[HD].X - Pts[T0].X, TUp).Inverse() * S.Head.GetRotation();
	RelPetiole = NpcRig::AlongX(Pts[G0].X - Pts[P0].X, TUp).Inverse() * S.Petiole.GetRotation();
	RelGaster = NpcRig::AlongX(Pts[G1].X - Pts[G0].X, TUp).Inverse() * S.Gaster.GetRotation();
	ThoraxQ = TQ;
	ThoraxO = TO;
	HeadQ = S.Head.GetRotation();
	for (int32 a = T0; a <= TU; ++a) { HoldAt[a] = Pts[a].X; }

	// ---- legs and feelers hang on the thorax and the head
	static const float KX[3] = { 7.f, 0.f, -7.f }, AX[3] = { 4.f, 0.f, -4.f }, EX[3] = { -4.f, 0.f, 4.f };
	for (int32 i = 0; i < 6; ++i)
	{
		bLeg[i] = S.bLeg[i];
		HipLocal[i] = TQ.UnrotateVector(S.Hip[i] - TO);
		HipW[i] = S.Hip[i];
		if (!bLeg[i])
		{
			continue;
		}
		MakeLeg(i);
		const float Side = (i & 1) ? 1.f : -1.f;
		const int32 Pair = i / 2;
		const float Len = AntRig::PairLen(Pair);
		for (int32 j = 0; j < 3; ++j) { LimbRest[LegPt(i, j)] = TQ.UnrotateVector(Pts[LegPt(i, j)].X - TO); }
		// folded in under the body, the way a dead ant's legs are
		const FVector HL = (AntRig::HipLocal(i) - AntRig::Joint(0));
		const FVector K = HL + FVector(KX[Pair], 9.f * Side, -20.f) * Len;
		const FVector A = HL + FVector(KX[Pair] + AX[Pair], -3.f * Side, -8.5f) * Len;
		const FVector E = A + FVector(EX[Pair], -5.f * Side, -7.f) * Len;
		CurlAt[LegPt(i, 0)] = K * VS;
		CurlAt[LegPt(i, 1)] = A * VS;
		CurlAt[LegPt(i, 2)] = E * VS;
	}
	const FQuat HQ0 = S.Head.GetRotation();
	for (int32 s = 0; s < 2; ++s)
	{
		bFeel[s] = true;
		UStaticMeshComponent* Scape = AntArt::AddArt(this, Root, ArtScape);
		if (S.Mats[AntMat::Scape]) { Scape->SetMaterial(0, S.Mats[AntMat::Scape]); }
		FeelComps[s * 3 + 0] = Scape;
		FeelComps[s * 3 + 1] = AntRig::AddPiece(this, Root, Art.FlagA, S.Mats[AntMat::Flag], true);
		FeelComps[s * 3 + 2] = AntRig::AddPiece(this, Root, Art.FlagB, S.Mats[AntMat::Flag], true);
		FeelLocal[s] = HQ0.UnrotateVector(S.FeelerBase[s] - Pts[T0].X);
		FeelW[s] = S.FeelerBase[s];
		On(FeelPt(s, 0), S.FeelerElbow[s], 0.9f);
		On(FeelPt(s, 1), S.FeelerMid[s], 0.8f);
		On(FeelPt(s, 2), S.FeelerTip[s], 0.8f);
		const float Side = s ? 1.f : -1.f;
		const FVector FL = AntRig::FeelerLocal(s);
		const FVector El = FL + FVector(5.f, 7.f * Side, -14.f);
		const FVector Mi = El + FVector(-5.f, -2.f * Side, -8.f);
		CurlAt[FeelPt(s, 0)] = El * VS;
		CurlAt[FeelPt(s, 1)] = Mi * VS;
		CurlAt[FeelPt(s, 2)] = (Mi + FVector(-10.f, -3.f * Side, -7.f)) * VS;
	}
	Sanitize();
}

void AAntCorpse::Sanitize()
{
	// A point that starts inside the floor would fall through it (a line trace does not see a surface from behind):
	// every point must be reachable from the middle of the body.
	UWorld* W = GetWorld();
	const FVector Mid = bBody ? ThoraxO + Up0 * 3.f * VS : Pts[T0].X + Up0 * 4.f * VS;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseStart), false, this);
	const FCollisionObjectQueryParams Solid(ECC_WorldStatic);
	for (FPt& P : Pts)
	{
		if (!P.bOn)
		{
			continue;
		}
		const FVector D = P.X - Mid;
		const float Len = (float)D.Size();
		FHitResult Hit;
		if (Len > 0.01f && W->LineTraceSingleByObjectType(Hit, Mid, P.X + D / Len * P.Rad, Solid, Params))
		{
			P.X = Hit.ImpactPoint + Hit.ImpactNormal * (P.Rad + 0.05f);
		}
		P.Prev = P.X;
	}
	// ... and no limb may start further from its joint than it is long (a stale pose would be torn back with huge speed)
	if (bBody)
	{
		Frames();
		SolveLimbs(0.f, 0.f);
	}
	SolveCore();
	for (FPt& P : Pts) { P.Prev = P.X; }
}

// ---------------------------------------------------------------------------------------------
// The simulation
// ---------------------------------------------------------------------------------------------

void AAntCorpse::SolveCore()
{
	for (const FLink& L : Core)
	{
		FPt& A = Pts[L.A];
		FPt& B = Pts[L.B];
		if (bTorn && L.A == T1 && L.B == P0)
		{
			continue;
		}
		const FVector D = B.X - A.X;
		const float Len = (float)D.Size();
		if (L.bMin && Len >= L.Len)
		{
			continue;
		}
		if (Len < 1.e-4f)
		{
			if (L.Len <= 0.f) { continue; }
			B.X += Up0 * 0.01f;
			continue;
		}
		const FVector Fix = D * ((Len - L.Len) / Len);
		A.X += Fix * L.WA;
		B.X -= Fix * (1.f - L.WA);
	}
}

void AAntCorpse::Frames()
{
	if (!bBody)
	{
		return;
	}
	const FVector X = Pts[T0].X - Pts[T1].X;
	const FVector Y = Pts[TR].X - Pts[TL].X;
	if (X.SizeSquared() > 1.e-6f && Y.SizeSquared() > 1.e-6f)
	{
		ThoraxQ = (FRotationMatrix::MakeFromXY(X, Y).ToQuat() * RelThorax).GetNormalized();
	}
	ThoraxO = (Pts[T0].X + Pts[T1].X) * 0.5f;
	HeadQ = (NpcRig::AlongX(Pts[HD].X - Pts[T0].X, ThoraxQ.GetAxisZ()) * RelHead).GetNormalized();
	for (int32 i = 0; i < 6; ++i)
	{
		HipW[i] = ThoraxO + ThoraxQ.RotateVector(HipLocal[i]);
	}
	for (int32 s = 0; s < 2; ++s)
	{
		FeelW[s] = Pts[T0].X + HeadQ.RotateVector(FeelLocal[s]);
	}
}

namespace
{
	/** B is kept at Len from A; A does not move (a limb hangs on the body, the body does not feel it). */
	inline void Hang(const FVector& A, FVector& B, float Len)
	{
		const FVector D = B - A;
		const float L = (float)D.Size();
		B = L > 1.e-4f ? A + D * (Len / L) : A + FVector(0.f, 0.f, -Len);
	}
	inline void Rod(FVector& A, FVector& B, float Len, float WA)
	{
		const FVector D = B - A;
		const float L = (float)D.Size();
		if (L < 1.e-4f) { return; }
		const FVector Fix = D * ((L - Len) / L);
		A += Fix * WA;
		B -= Fix * (1.f - WA);
	}
	inline void Apart(const FVector& A, FVector& B, float MinLen)
	{
		const FVector D = B - A;
		const float L = (float)D.Size();
		if (L < MinLen && L > 1.e-4f) { B = A + D * (MinLen / L); }
	}
}

void AAntCorpse::SolveLimbs(float Curl, float Hold)
{
	if (!bBody)
	{
		return;
	}
	const FVector TUp = ThoraxQ.GetAxisZ(), TFwd = ThoraxQ.GetAxisX(), TRight = ThoraxQ.GetAxisY();
	for (int32 i = 0; i < 6; ++i)
	{
		if (!bLeg[i])
		{
			continue;
		}
		FVector& K = Pts[LegPt(i, 0)].X;
		FVector& A = Pts[LegPt(i, 1)].X;
		FVector& E = Pts[LegPt(i, 2)].X;
		const float Side = (i & 1) ? 1.f : -1.f;
		if (Hold > 0.3f)
		{
			E = ToePin[i];   // still on its feet: they stay where they stood
		}
		if (Hold > 0.02f)
		{
			K += (ThoraxO + ThoraxQ.RotateVector(LimbRest[LegPt(i, 0)]) - K) * (0.3f * Hold);
		}
		if (Curl > 0.01f)
		{
			K += (ThoraxO + ThoraxQ.RotateVector(CurlAt[LegPt(i, 0)]) - K) * (0.22f * Curl);
			A += (ThoraxO + ThoraxQ.RotateVector(CurlAt[LegPt(i, 1)]) - A) * (0.22f * Curl);
			E += (ThoraxO + ThoraxQ.RotateVector(CurlAt[LegPt(i, 2)]) - E) * (0.16f * Curl);
		}
		Hang(HipW[i], K, LenHK[i]);
		// the hip is a socket, not a ball: the femur stays on its own side of the body, more or less out and down
		const FVector OutW = (TRight * Side * 0.75f - TUp * 0.55f + TFwd * 0.3f * (1 - i / 2)).GetSafeNormal();
		const FVector KD = (K - HipW[i]) / FMath::Max(1.e-3f, LenHK[i]);
		const float Dot = (float)FVector::DotProduct(KD, OutW);
		if (Dot < 0.05f)
		{
			K = HipW[i] + (KD + OutW * (0.05f - Dot) * 1.4f).GetSafeNormal() * LenHK[i];
		}
		Rod(K, A, LenKA[i], 0.3f);
		Rod(A, E, LenAE[i], 0.4f);
		Apart(HipW[i], A, (LenHK[i] + LenKA[i]) * 0.32f);   // the knee does not close completely
		Apart(K, E, (LenKA[i] + LenAE[i]) * 0.45f);
		Hang(HipW[i], K, LenHK[i]);
	}
	const float ScapeLen = AntRig::ScapeLen * VS, ALen = AntRig::FlagALen * VS, BLen = AntRig::FlagBLen * VS;
	for (int32 s = 0; s < 2; ++s)
	{
		if (!bFeel[s])
		{
			continue;
		}
		FVector& El = Pts[FeelPt(s, 0)].X;
		FVector& Mi = Pts[FeelPt(s, 1)].X;
		FVector& Ti = Pts[FeelPt(s, 2)].X;
		if (Curl > 0.01f)
		{
			El += (Pts[T0].X + HeadQ.RotateVector(CurlAt[FeelPt(s, 0)]) - El) * (0.2f * Curl);
			Mi += (Pts[T0].X + HeadQ.RotateVector(CurlAt[FeelPt(s, 1)]) - Mi) * (0.2f * Curl);
			Ti += (Pts[T0].X + HeadQ.RotateVector(CurlAt[FeelPt(s, 2)]) - Ti) * (0.14f * Curl);
		}
		Hang(FeelW[s], El, ScapeLen);
		// the scape cannot fold back into the head
		const FVector Fwd = HeadQ.GetAxisX();
		const FVector SD = (El - FeelW[s]) / ScapeLen;
		const float Dot = (float)FVector::DotProduct(SD, Fwd);
		if (Dot < -0.2f)
		{
			El = FeelW[s] + (SD + Fwd * (-0.2f - Dot) * 1.4f).GetSafeNormal() * ScapeLen;
		}
		Rod(El, Mi, ALen, 0.3f);
		Rod(Mi, Ti, BLen, 0.4f);
		Apart(FeelW[s], Mi, (ScapeLen + ALen) * 0.45f);
		Apart(El, Ti, (ALen + BLen) * 0.7f);
		Hang(FeelW[s], El, ScapeLen);
	}
}

void AAntCorpse::Collide(FPt& P, float H)
{
	const FVector Move = P.X - P.Prev;
	if (Move.SizeSquared() < 1.e-10f)
	{
		return;
	}
	// A little ball swept from where the point was to where it wants to be (a line would let a point that slides along
	// the floor sink into it up to its radius).
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpse), false, this);
	const float Rad = FMath::Max(0.05f, P.Rad * 0.92f);
	if (!GetWorld()->SweepSingleByObjectType(Hit, P.Prev, P.X, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Rad), Params))
	{
		return;
	}
	const FVector Nrm = Hit.Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (Hit.bStartPenetrating)
	{
		// it was inside something already (the pose it died in, a constraint that pulled it in): out the short way
		const FVector Along = FVector::VectorPlaneProject(Move, Nrm) * 0.8f;
		P.X = P.Prev + Nrm * (Hit.PenetrationDepth + 0.03f) + Along;
		P.Prev = P.X - Along;
		return;
	}
	// stop at the surface; the velocity it keeps: a little bounce off a hard landing, most of the slide
	const FVector At = Hit.Location + Nrm * 0.02f;
	FVector Vel = Move / H;
	const float Into = (float)FVector::DotProduct(Vel, Nrm);
	if (Into < 0.f)
	{
		Vel = (Vel - Nrm * Into) * 0.86f - Nrm * Into * (Into < -160.f ? 0.3f : 0.f);
	}
	P.X = At + FVector::VectorPlaneProject(P.X - At, Nrm) * 0.86f;
	P.Prev = P.X - Vel * H;
}

void AAntCorpse::Step(float H)
{
	const float Hold = HoldFor > 0.f ? 1.f - Smooth01(Age / HoldFor) : 0.f;
	const float CurlT = Age - HoldFor;
	const float Curl = CurlT > 0.f ? CurlMax * Smooth01(CurlT / FMath::Max(0.05f, CurlFor)) : 0.f;
	// once it is down and folded it settles: what is left of the motion dies out quickly
	const bool bSettling = CurlT > CurlFor + 0.3f && !Riding.IsValid();
	const float Damp = FMath::Exp(-(bSettling ? 6.f : Kind == AntDeath::Drowned ? 3.f : 0.45f) * H);

	for (FPt& P : Pts)
	{
		if (!P.bOn)
		{
			continue;
		}
		const FVector Vel = (P.X - P.Prev) * Damp;
		P.Prev = P.X;
		P.X += Vel + FVector(0.f, 0.f, Gravity * H * H);
	}
	if (bBody && AntDeath::IsCurl(Kind) && !bRolled && CurlT > 0.f)
	{
		// the legs have given way: it tips over onto its side or its back
		bRolled = true;
		const float Way = (Seed & 1) ? 1.f : -1.f;
		AddSpin(0, NumPts, ThoraxQ.GetAxisX(), Way * (7.f + (Seed % 5)));
		AddVelocity(0, NumPts, FVector(0.f, 0.f, 85.f) * AntKit::SpeedK * 2.f);
	}

	const AKitchenLoot* Ride = Riding.Get();
	for (int32 It = 0; It < 5; ++It)
	{
		SolveCore();
		if (bBody && Hold > 0.02f)
		{
			// swaying on its feet before they give way
			const FVector Right = FVector::CrossProduct(Up0, Fwd0);
			const float Rate = Kind == AntDeath::Burnt ? 31.f : Kind == AntDeath::Frozen ? 6.f : 13.f;
			const FVector Sway = (Right * FMath::Sin(Age * Rate) + Fwd0 * 0.5f * FMath::Sin(Age * Rate * 0.63f)) * (5.f * VS * FMath::Min(1.f, Age / FMath::Max(0.1f, HoldFor) * 1.5f))
				- Up0 * (6.f * VS * Smooth01(Age / FMath::Max(0.1f, HoldFor)));
			for (int32 a = T0; a <= TU; ++a)
			{
				Pts[a].X += (HoldAt[a] + Sway - Pts[a].X) * (0.5f * Hold);
			}
		}
		if (bBody && Ride)
		{
			// "bring the body home": the thorax is where the body loot is, everything else dangles
			const FTransform LT = Ride->GetActorTransform();
			for (int32 a = T0; a <= TU; ++a)
			{
				const FVector Want = LT.TransformPositionNoScale(RideLocal[a].GetLocation()) - FVector(0.f, 0.f, RideDrop);
				Pts[a].X += (Want - Pts[a].X) * RideBlend;
			}
		}
		Frames();
		if (bBody)
		{
			// The waist and the neck are joints with a limit, not strings: the gaster stays behind the thorax, the head in front
			// of it. Dead, the gaster curls under and the head drops.
			const FVector Back = ThoraxQ.RotateVector(FVector(-1.f, 0.f, -0.1f - 0.55f * Curl).GetSafeNormal());
			const FVector Front = ThoraxQ.RotateVector(FVector(0.94f, 0.f, -0.34f - 0.35f * Curl).GetSafeNormal());
			auto Cone = [this](int32 Joint, int32 Tip, const FVector& Axis, float MinDot, float Pull)
			{
				const FVector D = Pts[Tip].X - Pts[Joint].X;
				const float Len = (float)D.Size();
				if (Len < 1.e-3f) { return; }
				FVector Dir = D / Len;
				const float Dot = (float)FVector::DotProduct(Dir, Axis);
				if (Dot < MinDot)
				{
					Dir = (Dir + Axis * (MinDot - Dot) * 1.6f).GetSafeNormal();
				}
				else if (Pull > 0.f)
				{
					Dir = (Dir + Axis * Pull).GetSafeNormal();
				}
				Pts[Tip].X = Pts[Joint].X + Dir * Len;
			};
			if (!bTorn) { Cone(G0, G1, Back, 0.72f, 0.06f * Curl); }
			Cone(T0, HD, Front, 0.74f, 0.06f * Curl);
		}
		SolveLimbs(Curl, Hold);
	}
	// every few steps: no limb point may be on the far side of a surface from its joint (pulled through a thin rug by its links)
	if (bBody && (++StepCount % 5) == 0)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseRescue), false, this);
		const FCollisionObjectQueryParams Solid(ECC_WorldStatic);
		FHitResult Hit;
		for (int32 i = 0; i < 6; ++i)
		{
			if (!bLeg[i]) { continue; }
			FVector From = HipW[i];
			for (int32 j = 0; j < 3; ++j)
			{
				FPt& P = Pts[LegPt(i, j)];
				if (GetWorld()->LineTraceSingleByObjectType(Hit, From, P.X, Solid, Params))
				{
					P.X = Hit.ImpactPoint + Hit.ImpactNormal * (P.Rad + 0.03f);
					P.Prev = P.X;
				}
				else if (P.X.Z < ThoraxO.Z && GetWorld()->LineTraceSingleByObjectType(Hit, FVector(P.X.X, P.X.Y, ThoraxO.Z + 2.f), P.X, Solid, Params) && Hit.ImpactNormal.Z > 0.5f)
				{
					// ... nor under the floor the body lies on
					P.X = Hit.ImpactPoint + Hit.ImpactNormal * (P.Rad + 0.03f);
					P.Prev = P.X;
				}
				From = P.X;
			}
		}
	}
	float Fastest = 0.f;
	for (int32 i = 0; i < NumPts; ++i)
	{
		FPt& P = Pts[i];
		if (!P.bOn)
		{
			continue;
		}
		Collide(P, H);
		if (i < LegPts || !bBody)
		{
			Fastest = FMath::Max(Fastest, (float)(P.X - P.Prev).SizeSquared());   // (the body counts: a twitching tarsus does not keep it awake)
		}
	}
	if (bBody)
	{
		Frames();
	}
	const float Speed = FMath::Sqrt(Fastest) / H;
	Still = (Speed < 14.f && Hold <= 0.f) ? Still + H : 0.f;
}

void AAntCorpse::Sleep()
{
	if (!bAsleep && bBody)
	{
		// (for the test runs: did it come to rest sanely? it must lie on the floor, in one piece)
		float Far = 0.f, Low = 1.e9f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseRest), false, this);
		for (const FPt& P : Pts)
		{
			if (!P.bOn) { continue; }
			Far = FMath::Max(Far, (float)FVector::Dist(P.X, bTorn && (&P - Pts) >= P0 && (&P - Pts) < LegPts ? Pts[G0].X : ThoraxO));
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, P.X + FVector(0.f, 0.f, 30.f), P.X - FVector(0.f, 0.f, 200.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				Low = FMath::Min(Low, (float)(P.X.Z - Hit.ImpactPoint.Z) - P.Rad);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTBODY corpse kind %d at rest after %.1f s (awake %.1f): thorax up.z %.2f, farthest point %.0f, lowest point %.1f above what is under it (ant units), at %s"),
			(int32)Kind, Age, Awake, ThoraxQ.GetAxisZ().Z, Far / VS, Low / VS, *ThoraxO.ToCompactString());
	}
	bAsleep = true;
	for (FPt& P : Pts) { P.Prev = P.X; }
	for (UStaticMeshComponent* S : Strands)
	{
		if (S) { S->SetVisibility(false); }
	}
}

// ---------------------------------------------------------------------------------------------
// Drawing the pieces between the points
// ---------------------------------------------------------------------------------------------

void AAntCorpse::Draw()
{
	if (bFlat)
	{
		return;
	}
	const AntArt::FPart& ArtCoxa = AntRig::Art().Coxa;
	const AntArt::FPart& ArtFemur = AntRig::Art().Femur;
	const AntArt::FPart& ArtScape = AntRig::Art().Scape;
	const FVector Scale(VS);
	const float Thick = LegThick * VS;
	FVector TUp = Up0;
	if (bBody)
	{
		TUp = ThoraxQ.GetAxisZ();
		Thorax->SetWorldTransform(FTransform(ThoraxQ, ThoraxO, Scale));
		Head->SetWorldTransform(FTransform(HeadQ, Pts[T0].X, Scale));
		Petiole->SetWorldTransform(FTransform((NpcRig::AlongX(Pts[G0].X - Pts[P0].X, TUp) * RelPetiole).GetNormalized(), Pts[P0].X, Scale));
		Gaster->SetWorldTransform(FTransform((NpcRig::AlongX(Pts[G1].X - Pts[G0].X, TUp) * RelGaster).GetNormalized(), Pts[G0].X, Scale));
	}
	for (int32 i = 0; i < 6; ++i)
	{
		if (!LegComps[i * 4])
		{
			continue;
		}
		const FVector Hip = bBody ? HipW[i] : Pts[T0].X;
		const FVector K = Pts[LegPt(i, 0)].X, A = Pts[LegPt(i, 1)].X, E = Pts[LegPt(i, 2)].X;
		const FVector Dir = (K - Hip).GetSafeNormal(UE_SMALL_NUMBER, -TUp);
		const FVector CoxaEnd = Hip + Dir * AntRig::CoxaLen * VS;
		const FVector KUp = FVector::VectorPlaneProject(TUp, Dir).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		AntArt::PlaceAlong(LegComps[i * 4 + 0], ArtCoxa, Hip - Dir * 2.5f * VS, CoxaEnd + Dir * VS, KUp, 8.f * Thick, false, false);
		AntArt::PlaceAlong(LegComps[i * 4 + 1], ArtFemur, CoxaEnd, K, KUp, 7.5f * Thick, AAntCharacter::ArtFlipFemur, false);
		AntRig::PlaceLimb(LegComps[i * 4 + 2], K, A, TUp, 5.5f * Thick, AntB_TibiaThick);
		AntRig::PlaceLimb(LegComps[i * 4 + 3], A, E, TUp, AntRig::TarsusAcross * Thick, AntB_TarsusThick);
	}
	for (int32 s = 0; s < 2; ++s)
	{
		if (!bFeel[s] || !FeelComps[s * 3])
		{
			continue;
		}
		const FVector El = Pts[FeelPt(s, 0)].X, Mi = Pts[FeelPt(s, 1)].X, Ti = Pts[FeelPt(s, 2)].X;
		const FVector HUp = HeadQ.GetAxisZ();
		AntArt::PlaceAlong(FeelComps[s * 3 + 0], ArtScape, FeelW[s], El, HUp, 3.4f * VS, false, false);
		AntRig::PlaceLimb(FeelComps[s * 3 + 1], El, Mi, HUp, 2.0f * VS, AntB_FlagAThick);
		AntRig::PlaceLimb(FeelComps[s * 3 + 2], Mi, Ti, HUp, 3.2f * VS, AntB_FlagBThick);
	}
	// strands of goo between the two halves, thinning as they stretch, then gone
	for (int32 i = 0; i < Strands.Num(); ++i)
	{
		UStaticMeshComponent* S = Strands[i];
		if (!S)
		{
			continue;
		}
		const FVector A = Pts[T1].X, B = Pts[P0].X;
		const float Len = (float)FVector::Dist(A, B);
		const float Snap = (70.f + 25.f * i) * VS;
		if (!bTorn || Len > Snap || Age > 0.8f + 0.15f * i || bAsleep)
		{
			S->SetVisibility(false);
			continue;
		}
		const FVector Side = FVector::CrossProduct((B - A).GetSafeNormal(), FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FVector Off = (Side * (i - 1) * 1.6f + FVector(0.f, 0.f, -0.25f * i)) * VS;
		const float Th = FMath::Max(0.25f, 2.6f * (1.f - Len / Snap)) * VS / 100.f;
		S->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(B - A).ToQuat(), (A + B) * 0.5f + Off, FVector(Th, Th, FMath::Max(0.001f, Len / 100.f))));
	}
}

void AAntCorpse::Flatten()
{
	// Pressed flat where it stood: the body pieces under one flattened root, the legs laid out flat around it.
	Draw();
	bFlat = true;
	bAsleep = true;
	UWorld* W = GetWorld();
	FVector Floor = ThoraxO - Up0 * 20.f * VS;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseFlat), false, this);
	if (W->LineTraceSingleByObjectType(Hit, ThoraxO + Up0 * 2.f, ThoraxO - Up0 * 60.f * VS, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Floor = Hit.ImpactPoint;
	}
	Floor += Up0 * 0.12f;
	const FQuat Q = FRotationMatrix::MakeFromZX(Up0, Fwd0).ToQuat();
	SetActorLocationAndRotation(Floor, Q);
	FlatRoot = NewObject<USceneComponent>(this);
	FlatRoot->SetupAttachment(Root);
	FlatRoot->RegisterComponent();
	const FTransform Plain(Q, Floor, FVector::OneVector);
	const float Wide = 1.3f, Thin = 0.13f;
	FlatRoot->SetRelativeScale3D(FVector(Wide, Wide, Thin));
	for (UStaticMeshComponent* C : { Thorax, Head, Petiole, Gaster })
	{
		const FTransform Rel = C->GetComponentTransform().GetRelativeTransform(Plain);
		C->SetUsingAbsoluteLocation(false);
		C->SetUsingAbsoluteRotation(false);
		C->SetUsingAbsoluteScale(false);
		C->AttachToComponent(FlatRoot, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeTransform(Rel);
	}
	Jaw->SetRelativeRotation(FRotator(-40.f, 0.f, 0.f));
	auto Flat = [&](const FVector& X, float Lift)
	{
		const FVector D = X - Floor;
		return Floor + FVector::VectorPlaneProject(D, Up0) * Wide + Up0 * Lift * VS;
	};
	const AntArt::FPart& ArtCoxa = AntRig::Art().Coxa;
	const AntArt::FPart& ArtFemur = AntRig::Art().Femur;
	const AntArt::FPart& ArtScape = AntRig::Art().Scape;
	auto Squeeze = [](UStaticMeshComponent* C)
	{
		FVector S = C->GetComponentScale();
		S.Z *= 0.35f;
		C->SetWorldScale3D(S);
	};
	auto Keep = [this](UStaticMeshComponent* C)
	{
		// from now on it moves with the actor (the pancake rides the slipper)
		const FTransform T = C->GetComponentTransform();
		C->SetUsingAbsoluteLocation(false);
		C->SetUsingAbsoluteRotation(false);
		C->SetUsingAbsoluteScale(false);
		C->SetWorldTransform(T);
	};
	for (int32 i = 0; i < 6; ++i)
	{
		if (!bLeg[i] || !LegComps[i * 4])
		{
			continue;
		}
		// splayed: straight out from the hip, the joints bent a little this way and that
		const FVector Hip = Flat(HipW[i], 1.2f);
		const float Side = (i & 1) ? 1.f : -1.f;
		FVector Out = FVector::VectorPlaneProject(Pts[LegPt(i, 2)].X - HipW[i], Up0).GetSafeNormal(UE_SMALL_NUMBER, Q.GetAxisY() * Side);
		const float Bend = ((Seed >> i) & 1) ? 0.45f : -0.35f;
		const FVector Out2 = FQuat(Up0, Bend).RotateVector(Out);
		const FVector Out3 = FQuat(Up0, -Bend * 0.6f).RotateVector(Out);
		const FVector CoxaEnd = Hip + Out * AntRig::CoxaLen * VS;
		const FVector K = Hip + Out * LenHK[i] * 1.05f;
		const FVector A = K + Out2 * LenKA[i];
		const FVector E = A + Out3 * LenAE[i];
		AntArt::PlaceAlong(LegComps[i * 4 + 0], ArtCoxa, Hip - Out * 2.f * VS, CoxaEnd, Up0, 5.f * LegThick * VS, false, false);
		AntArt::PlaceAlong(LegComps[i * 4 + 1], ArtFemur, CoxaEnd, K, Up0, 5.f * LegThick * VS, AAntCharacter::ArtFlipFemur, false);
		AntRig::PlaceLimb(LegComps[i * 4 + 2], K, A, Up0, 5.5f * LegThick * VS, AntB_TibiaThick);
		AntRig::PlaceLimb(LegComps[i * 4 + 3], A, E, Up0, AntRig::TarsusAcross * LegThick * VS, AntB_TarsusThick);
		Squeeze(LegComps[i * 4 + 2]);
		Squeeze(LegComps[i * 4 + 3]);
		for (int32 j = 0; j < 4; ++j) { Keep(LegComps[i * 4 + j]); }
	}
	for (int32 s = 0; s < 2; ++s)
	{
		if (!FeelComps[s * 3])
		{
			continue;
		}
		const float Side = s ? 1.f : -1.f;
		const FVector Base = Flat(FeelW[s], 1.f);
		const FVector D1 = (Q.GetAxisX() * 0.75f + Q.GetAxisY() * Side * 0.66f).GetSafeNormal();
		const FVector D2 = FQuat(Up0, Side * 0.9f).RotateVector(D1);
		const FVector El = Base + D1 * AntRig::ScapeLen * VS;
		const FVector Mi = El + D2 * AntRig::FlagALen * VS;
		const FVector Ti = Mi + FQuat(Up0, Side * 0.4f).RotateVector(D2) * AntRig::FlagBLen * VS;
		AntArt::PlaceAlong(FeelComps[s * 3 + 0], ArtScape, Base, El, Up0, 3.f * VS, false, false);
		AntRig::PlaceLimb(FeelComps[s * 3 + 1], El, Mi, Up0, 2.0f * VS, AntB_FlagAThick);
		AntRig::PlaceLimb(FeelComps[s * 3 + 2], Mi, Ti, Up0, 3.2f * VS, AntB_FlagBThick);
		Squeeze(FeelComps[s * 3 + 1]);
		Squeeze(FeelComps[s * 3 + 2]);
		for (int32 j = 0; j < 3; ++j) { Keep(FeelComps[s * 3 + j]); }
	}
	if (AAntCharacter* A = Of.Get())
	{
		FlatOffset = Floor - A->GetActorLocation();
	}
}

// ---------------------------------------------------------------------------------------------
// "Bring the body home"
// ---------------------------------------------------------------------------------------------

void AAntCorpse::RideLoot()
{
	bWantLoot = true;
	LootSearch = 2.f;
	NextLootTry = 0.f;
	TryRide();
}

void AAntCorpse::TryRide()
{
	if (Riding.IsValid() || !bBody || bTorn)
	{
		bWantLoot = false;
		return;
	}
	static const FName Taken(TEXT("AntCorpseOn"));
	AKitchenLoot* Best = nullptr;
	float BestD = 500.f;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (!It->bBody || It->Tags.Contains(Taken))
		{
			continue;
		}
		const float D = (float)FVector::Dist(It->GetActorLocation(), OfLastAt);
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best)
	{
		return;
	}
	Best->Tags.Add(Taken);
	Riding = Best;
	bWantLoot = false;
	bAsleep = false;
	Still = 0.f;
	Awake = 0.f;
	RideBlend = 0.f;
	if (bHidden)
	{
		bHidden = false;
		SetActorHiddenInGame(false);
	}
	// its own placeholder shape is not drawn: the ant itself lies there
	TArray<UStaticMeshComponent*> Meshes;
	Best->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M && M->GetFName() == TEXT("Mesh")) { M->SetVisibility(false); }
	}
	const FTransform LT = Best->GetActorTransform();
	if (bFlat)
	{
		// the pancake lies on the floor where the loot is (and goes with it when it is carried)
		FVector At = LT.GetLocation();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseFlatRide), false, this);
		Params.AddIgnoredActor(Best);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, At + FVector(0.f, 0.f, 1.f), At - FVector(0.f, 0.f, 60.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			At = Hit.ImpactPoint + FVector(0.f, 0.f, 0.12f);
		}
		SetActorLocation(At);
		AttachToActor(Best, FAttachmentTransformRules::KeepWorldTransform);
		return;
	}
	// the thorax keeps its attitude, centred on the loot
	const FVector Shift = LT.GetLocation() - ThoraxO;
	for (int32 a = T0; a <= TU; ++a)
	{
		RideLocal[a] = FTransform(LT.InverseTransformPositionNoScale(Pts[a].X + Shift));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTBODY corpse rides the body loot %s (%.0f away)"), *Best->GetName(), Shift.Size());
}

void AAntCorpse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	AAntCharacter* Ant = Of.Get();
	if (Ant)
	{
		OfLastAt = Ant->GetActorLocation();
	}
	else if (bHadOwner)
	{
		// the dead ant is gone (its player hatched again): if its body was left as loot, that is where this corpse belongs
		bHadOwner = false;
		if (!Riding.IsValid() && bBody && !bTorn)
		{
			bWantLoot = true;
			LootSearch = 1.5f;
		}
	}
	if (bWantLoot)
	{
		LootSearch -= DeltaSeconds;
		if (Age > NextLootTry)
		{
			NextLootTry = Age + 0.2f;
			TryRide();
		}
		if (LootSearch <= 0.f) { bWantLoot = false; }
	}
	if (Riding.IsStale())
	{
		Destroy();   // delivered to the nest, or the minute is up
		return;
	}
	if (!Riding.IsValid() && Age > Life)
	{
		Destroy();
		return;
	}
	if (AKitchenLoot* L = Riding.Get())
	{
		// (the loot redraws itself when its state replicates)
		TArray<UStaticMeshComponent*> Meshes;
		L->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M && M->GetFName() == TEXT("Mesh") && M->IsVisible()) { M->SetVisibility(false); }
		}
	}
	if (bFlat)
	{
		if (Riding.IsValid())
		{
			return;
		}
		if (Ant && Ant->StuckTo)
		{
			// the pancake is stuck to the sole (the palm, the newspaper) that made it
			SetActorLocation(Ant->GetActorLocation() + FlatOffset);
			bFlatFalling = true;
			FlatVel = FVector::ZeroVector;
		}
		else if (bFlatFalling)
		{
			// peeled off: it drops to whatever is below
			FlatVel.Z += Gravity * DeltaSeconds;
			const FVector From = GetActorLocation();
			const FVector To = From + FlatVel * DeltaSeconds;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseDrop), false, this);
			if (GetWorld()->LineTraceSingleByObjectType(Hit, From + FVector(0.f, 0.f, 1.f), To - FVector(0.f, 0.f, 0.3f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				SetActorLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal * 0.12f, FRotationMatrix::MakeFromZX(Hit.ImpactNormal, GetActorForwardVector()).ToQuat());
				bFlatFalling = false;
			}
			else
			{
				SetActorLocation(To);
			}
		}
		return;
	}
	if (bHidden && !Riding.IsValid())
	{
		return;
	}
	const bool bCarried = Riding.IsValid() && (Riding->IsCarried() || Riding->GetVelocity().SizeSquared() > 25.f);
	if (bAsleep)
	{
		if (!bCarried)
		{
			return;
		}
		bAsleep = false;   // somebody picked the body up
		Still = 0.f;
		Awake = 0.f;
	}
	RideBlend = Riding.IsValid() ? FMath::Min(1.f, RideBlend + DeltaSeconds * 3.f) : 0.f;

	// fixed steps: the pose a corpse comes to rest in should not depend on the frame rate
	const float H = 1.f / 90.f;
	Awake += DeltaSeconds;
	StepAcc += FMath::Min(DeltaSeconds, 0.05f);
	int32 Steps = 0;
	while (StepAcc >= H && Steps < 4)
	{
		StepAcc -= H;
		++Steps;
		Step(H);
	}
	if (Steps > 0)
	{
		Draw();
		// The body lies where it came to rest, for everyone: the dead ant itself is moved there (the loot "body" appears
		// at the dead ant's place, and the dead player's camera follows its body).
		if (Ant && bBody && !bTorn && Ant->bDead && !Ant->StuckTo && !Ant->bCorpseGone && !Riding.IsValid() && (Ant->HasAuthority() || Ant->IsLocallyControlled()))
		{
			Ant->SetActorLocation(ThoraxO + FVector(0.f, 0.f, AAntCharacter::BodyRadius * 0.6f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	// At rest when nothing of it has gone anywhere for half a second (the points always tremble a little where the links and
	// the floor disagree: their speed says nothing).
	RestClock += DeltaSeconds;
	if (RestClock >= 0.25f)
	{
		RestClock = 0.f;
		float Went = 0.f;
		for (int32 k = 0; k < 3; ++k)
		{
			Went = FMath::Max(Went, (float)FVector::Dist(Pts[RestIdx[k]].X, RestRef[k]));
			RestRef[k] = Pts[RestIdx[k]].X;
		}
		RestHits = (Went < 1.2f * VS && Age > HoldFor + CurlFor * 0.7f) ? RestHits + 1 : 0;
	}
	if (AKitchenLoot* Loot = Riding.Get())
	{
		// lying on the floor as loot, the corpse lies on the floor too (the loot's own box is bigger than an ant)
		if (Age > NextDropCheck)
		{
			NextDropCheck = Age + 0.15f;
			RideDropWant = 0.f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCorpseDrop), false, this);
			Params.AddIgnoredActor(Loot);
			const FVector At = Loot->GetActorLocation();
			if (!Loot->IsCarried() && GetWorld()->LineTraceSingleByObjectType(Hit, At + FVector(0.f, 0.f, 1.f), At - FVector(0.f, 0.f, 60.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				RideDropWant = FMath::Max(0.f, (float)(At.Z - Hit.ImpactPoint.Z) - 9.5f * VS);
			}
		}
		RideDrop = FMath::FInterpTo(RideDrop, RideDropWant, DeltaSeconds, 10.f);
	}
	if (!bCarried && (!Riding.IsValid() || (RideBlend >= 1.f && FMath::Abs(RideDrop - RideDropWant) < 0.05f)) && (RestHits >= 2 || Awake > 7.f))
	{
		Sleep();   // at rest: no more simulation (a body lying as loot wakes when somebody picks it up)
	}
}

// ---------------------------------------------------------------------------------------------
// The instanced ants: a flip onto the back, nothing more
// ---------------------------------------------------------------------------------------------

AAntDeadPile::AAntDeadPile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntDeadPile::Scatter(const UObject* WorldContext, const FVector& At, int32 Killed)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Killed <= 0 || !AntRig::Enabled())
	{
		return;
	}
	AAntDeadPile* Pile = nullptr;
	for (TActorIterator<AAntDeadPile> It(World); It; ++It)
	{
		Pile = *It;
		break;
	}
	if (!Pile)
	{
		Pile = World->SpawnActor<AAntDeadPile>(AAntDeadPile::StaticClass(), FTransform::Identity);
		if (!Pile)
		{
			return;
		}
		const AntArt::FPart& Art = AntArt::Get(TEXT("SM_Ant_Body"));
		UInstancedStaticMeshComponent* M = NewObject<UInstancedStaticMeshComponent>(Pile, TEXT("Dead"));
		M->SetupAttachment(Pile->RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->SetCanEverAffectNavigation(false);
		if (Art.IsValid())
		{
			M->SetStaticMesh(Art.Mesh);
			if (Art.Material) { M->SetMaterial(0, Art.Material); }
			// a worker is ~28 long (the same size AAntArmy draws them)
			const FVector Size = Art.Mesh->GetBoundingBox().GetSize();
			Pile->Scale = 28.f * AAntCharacter::AntShrink / FMath::Max(1.f, (float)Size.GetMax());
			Pile->HalfHeight = (float)Size.Z * 0.5f * Pile->Scale;
		}
		else
		{
			M->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
			Pile->Scale = 0.03f;
			Pile->HalfHeight = 1.5f;
		}
		M->RegisterComponent();
		Pile->AddInstanceComponent(M);
		Pile->Mesh = M;
	}
	const int32 Num = FMath::Clamp(Killed, 1, 14);
	const float Radius = FMath::Clamp(110.f + Killed * 2.f, 110.f, 340.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntDeadPile), false);
	for (int32 i = 0; i < Num; ++i)
	{
		const float A = FMath::FRand() * 2.f * PI, D = FMath::Sqrt(FMath::FRand()) * Radius;
		const FVector P = At + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 60.f), P - FVector(0.f, 0.f, 120.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;
		}
		if (Pile->Dead.Num() >= MaxDead)
		{
			Pile->Dead.RemoveAt(0);
			Pile->Mesh->RemoveInstance(0);
		}
		FDead Dd;
		Dd.At = Hit.ImpactPoint;
		Dd.Up = Hit.ImpactNormal;
		Dd.Yaw = FMath::FRand() * 360.f;
		Dd.Side = FMath::RandBool() ? 1.f : -1.f;
		Dd.Age = -0.12f * FMath::FRand();
		Pile->Dead.Add(Dd);
		Pile->Mesh->AddInstance(FTransform(Dd.At), true);
		Pile->Place(Pile->Dead.Num() - 1, i == Num - 1);
	}
}

void AAntDeadPile::Place(int32 Index, bool bDirty)
{
	const FDead& D = Dead[Index];
	// tossed up and over onto its back in a third of a second; shrinks away at the end
	const float T = FMath::Clamp(D.Age / 0.32f, 0.f, 1.f);
	const float Turn = PI * Smooth01(T) * D.Side;
	const float Hop = FMath::Sin(T * PI) * 5.f * AAntCharacter::AntShrink;
	const float Fade = FMath::Clamp((Life - D.Age) / 1.2f, 0.f, 1.f);
	const FQuat Base = FRotationMatrix::MakeFromZX(D.Up, FRotator(0.f, D.Yaw, 0.f).Vector()).ToQuat();
	const FQuat Q = Base * FQuat(FVector::ForwardVector, Turn);
	Mesh->UpdateInstanceTransform(Index, FTransform(Q, D.At + D.Up * (HalfHeight * 0.8f + Hop), FVector(Scale * FMath::Max(0.01f, Fade))), true, bDirty, true);
}

void AAntDeadPile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Mesh || Dead.Num() == 0)
	{
		return;
	}
	bool bAny = false;
	for (int32 i = Dead.Num() - 1; i >= 0; --i)
	{
		FDead& D = Dead[i];
		const bool bMoved = D.Age < 0.34f || D.Age > Life - 1.3f;
		D.Age += DeltaSeconds;
		if (D.Age > Life)
		{
			Dead.RemoveAt(i);
			Mesh->RemoveInstance(i);
			continue;
		}
		if (bMoved)
		{
			Place(i, false);
			bAny = true;
		}
	}
	if (bAny)
	{
		Mesh->MarkRenderStateDirty();
	}
}
