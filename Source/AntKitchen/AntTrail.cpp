#include "AntTrail.h"
#include "Map4Look.h"
#include "AntAudio.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AntSnowball.h"
#include "AntRaid.h"
#include "AntClan.h"
#include "AntRun.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntTypes.h"
#include "HumanCharacter.h"
#include "NpcBody.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace TrailLook
{
	const FLinearColor Body(0.055f, 0.022f, 0.014f);
	const FLinearColor Legs(0.03f, 0.012f, 0.008f);
	const FLinearColor Dot(0.55f, 1.6f, 0.75f);      // glowing pheromone green
	const FLinearColor LayDot(1.6f, 1.3f, 0.4f);     // a trail being laid: amber
	constexpr int32 NumData = 13;                    // = SwarmLook::NumData (M_SwarmAnt / M_SwarmAntArt)
	constexpr int32 MaxFerriesDrawn = 64;
	constexpr float DotStep = 34.f;
	const FLinearColor Msg(0.6f, 1.f, 0.7f);
	const FLinearColor Bad(1.f, 0.45f, 0.35f);
}

// v12: trail workers walk 1.5x faster (a crumb home in seconds, not a minute); -AntSnowOld: as before.
static float TrailPace()
{
	static const float K = FParse::Param(FCommandLine::Get(), TEXT("AntSnowOld")) ? 1.f : 1.5f;
	return K;
}

AAntTrailNet::AAntTrailNet()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntTrailNet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntTrailNet, Lines);
	DOREPLIFETIME(AAntTrailNet, Ferries);
}

AAntTrailNet* AAntTrailNet::Get(const UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntTrailNet> It(World); It; ++It)
	{
		return *It;
	}
	if (bCreate && World->GetNetMode() != NM_Client)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return const_cast<UWorld*>(World)->SpawnActor<AAntTrailNet>(AAntTrailNet::StaticClass(), FTransform::Identity, P);
	}
	return nullptr;
}

UInstancedStaticMeshComponent* AAntTrailNet::MakeWorkerPart(const TCHAR* Name, const FVector& Offset, const FVector& Size, float LegMode, const FLinearColor& Color, UStaticMesh* Mesh, const TCHAR* Mat)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
	C->SetupAttachment(RootComponent);
	C->SetMobility(EComponentMobility::Movable);
	UStaticMesh* LegMesh = Map4Look::Enabled() ? Map4Look::Prop(TEXT("SM_M4_LegPair")) : nullptr;   // map v4: bent leg pairs, not bars
	C->SetStaticMesh(Mesh ? Mesh : (LegMesh ? LegMesh : AntKit::ShapeMesh(EAntShape::Cube)));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	C->bAffectDistanceFieldLighting = false;
	C->SetNumCustomDataFloats(TrailLook::NumData);
	C->RegisterComponent();
	AddInstanceComponent(C);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Mat ? Mat : TEXT("/Game/Swarm/M_SwarmAnt.M_SwarmAnt"));
	bHasMaterial = Base != nullptr;
	if (Base)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, C);
		M->SetVectorParameterValue(TEXT("PartOffset"), FLinearColor(Offset.X, Offset.Y, Offset.Z, 0.f));
		M->SetVectorParameterValue(TEXT("PartScale"), FLinearColor(Size.X, Size.Y, Size.Z, 0.f));
		M->SetScalarParameterValue(TEXT("LegMode"), LegMode);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetVectorParameterValue(TEXT("Glow"), FLinearColor::Black);
		C->SetMaterial(0, M);
		MIDs.Add(M);
	}
	else
	{
		AntKit::Paint(C, Color);
	}
	return C;
}

void AAntTrailNet::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Workers: the swarm's look (the same GPU-walked ants, AntSwarm.cpp).
	constexpr float K = 0.7f * AAntCharacter::AntShrink;
	UStaticMesh* ArtMesh = AntArt::Get(TEXT("SM_Ant_Body")).Mesh;
	const bool bArtMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Swarm/M_SwarmAntArt.M_SwarmAntArt")) != nullptr;
	if (ArtMesh && bArtMat)
	{
		const FBox B = ArtMesh->GetBoundingBox();
		const float Len = 40.f * K;
		const float Sc = Len / FMath::Max(1.f, (float)B.GetSize().X);
		const FVector Off = FVector(0.f, 0.f, 6.f * K) - B.GetCenter() * Sc;
		WorkerParts.Add(MakeWorkerPart(TEXT("TrailBody"), Off, FVector(Sc * 100.f), 0.f, FLinearColor::White, ArtMesh, TEXT("/Game/Swarm/M_SwarmAntArt.M_SwarmAntArt")));
	}
	else
	{
		WorkerParts.Add(MakeWorkerPart(TEXT("TrailAbdomen"), FVector(-11.f, 0.f, 6.f) * K, FVector(17.f, 11.f, 10.f) * K, 0.f, TrailLook::Body, nullptr, nullptr));
		WorkerParts.Add(MakeWorkerPart(TEXT("TrailHead"), FVector(9.f, 0.f, 7.f) * K, FVector(8.f, 8.f, 7.f) * K, 0.f, TrailLook::Body, nullptr, nullptr));
	}
	WorkerParts.Add(MakeWorkerPart(TEXT("TrailLegsA"), FVector(0.f, 0.f, 3.f) * K, FVector(1.6f, 30.f, 1.4f) * K, 1.f, TrailLook::Legs, nullptr, nullptr));
	WorkerParts.Add(MakeWorkerPart(TEXT("TrailLegsB"), FVector(0.f, 0.f, 3.f) * K, FVector(1.6f, 30.f, 1.4f) * K, 2.f, TrailLook::Legs, nullptr, nullptr));

	// The trail itself: a faint glowing dotted line (unlit, so it reads from above and in the dark).
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	auto MakeDots = [this, Glow](const TCHAR* Name, const FLinearColor& Color)
	{
		if (bHasMaterial)
		{
			// The swarm's GPU material (it is made for instances): a flat glowing disc that does not move (speed 0).
			UStaticMesh* DotMesh = Map4Look::Enabled() && Map4Look::Shape(8) ? Map4Look::Shape(8) : AntKit::ShapeMesh(EAntShape::Sphere);   // map v4: a drop of pheromone
			UInstancedStaticMeshComponent* D = MakeWorkerPart(Name, FVector::ZeroVector, FVector(6.5f, 6.5f, Map4Look::Enabled() ? 30.f : 1.2f), 0.f, Color, DotMesh, nullptr);
			if (MIDs.Num() > 0) { MIDs.Last()->SetVectorParameterValue(TEXT("Glow"), Color); }
			return D;
		}
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Map4Look::Enabled() && Map4Look::Shape(8) ? Map4Look::Shape(8) : AntKit::ShapeMesh(EAntShape::Sphere));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->RegisterComponent();
		AddInstanceComponent(C);
		if (Glow)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Glow, C);
			M->SetVectorParameterValue(TEXT("Color"), Color);
			C->SetMaterial(0, M);
		}
		else
		{
			AntKit::Paint(C, Color);
		}
		return C;
	};
	Dots = MakeDots(TEXT("TrailDots"), TrailLook::Dot);
	LayDots = MakeDots(TEXT("TrailLayDots"), TrailLook::LayDot);
	// Crumbs on their way home, each in a worker's jaws.
	for (int32 i = 0; i < TrailLook::MaxFerriesDrawn; ++i)
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, RootComponent, EAntShape::Sphere, FVector::ZeroVector, FVector(0.1f), FLinearColor::White);
		if (Map4Look::Enabled() && Map4Look::Shape(7)) { C->SetStaticMesh(Map4Look::Shape(7)); }   // map v4: a crumb in the jaws
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetCastShadow(false);
		C->SetVisibility(false);
		FerryCrumbs.Add(C);
		FerryMIDs.Add(Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)));
		UStaticMeshComponent* A = AntKit::AddPart(this, RootComponent, EAntShape::Sphere, FVector::ZeroVector, FVector(0.1f), TrailLook::Body);
		if (ArtMesh) { A->SetStaticMesh(ArtMesh); if (UMaterialInterface* Mi = AntArt::Get(TEXT("SM_Ant_Body")).Material) { A->SetMaterial(0, Mi); } }
		A->SetUsingAbsoluteLocation(true);
		A->SetUsingAbsoluteRotation(true);
		A->SetCastShadow(false);
		A->SetVisibility(false);
		FerryAnts.Add(A);
	}
	Rebuild();
}

void AAntTrailNet::OnRep_Lines()
{
	Rebuild();
}

void AAntTrailNet::OnRep_Ferries()
{
}

const FAntTrailLine* AAntTrailNet::LineById(uint16 Id) const
{
	for (const FAntTrailLine& L : Lines) { if (L.Id == Id) { return &L; } }
	return nullptr;
}

bool AAntTrailNet::PosAt(const FAntTrailLine& L, float D, FVector& Out, FVector* OutN) const
{
	const int32 N = L.Points.Num();
	if (N == 0)
	{
		return false;
	}
	float Acc = 0.f;
	for (int32 i = 0; i + 1 < N; ++i)
	{
		const FVector A = L.Points[i];
		const FVector B = L.Points[i + 1];
		const float Seg = (float)FVector::Dist(A, B);
		if (Acc + Seg >= D || i + 2 == N)
		{
			const float T = Seg > 0.f ? FMath::Clamp((D - Acc) / Seg, 0.f, 1.f) : 0.f;
			Out = FMath::Lerp(A, B, T);
			if (OutN) { *OutN = L.Normals.IsValidIndex(i) ? FVector(L.Normals[i]) : FVector::UpVector; }
			return true;
		}
		Acc += Seg;
	}
	Out = L.Points[0];
	if (OutN) { *OutN = FVector::UpVector; }
	return true;
}

float AAntTrailNet::NearestOnLine(const FAntTrailLine& L, const FVector& P, float& OutDistSq) const
{
	OutDistSq = 1.e18f;
	float Best = 0.f;
	float Acc = 0.f;
	for (int32 i = 0; i + 1 < L.Points.Num(); ++i)
	{
		const FVector A = L.Points[i];
		const FVector B = L.Points[i + 1];
		const FVector C = FMath::ClosestPointOnSegment(P, A, B);
		const float D2 = (float)FVector::DistSquared(C, P);
		const float Seg = (float)FVector::Dist(A, B);
		if (D2 < OutDistSq)
		{
			OutDistSq = D2;
			Best = Acc + (float)FVector::Dist(A, C);
		}
		Acc += Seg;
	}
	return Best;
}

void AAntTrailNet::Measure(FAntTrailLine& L) const
{
	L.Dist.Reset();
	float Acc = 0.f;
	for (int32 i = 0; i < L.Points.Num(); ++i)
	{
		if (i > 0) { Acc += (float)FVector::Dist(L.Points[i - 1], L.Points[i]); }
		L.Dist.Add(Acc);
	}
	L.Length = Acc;
}

bool AAntTrailNet::FerryPos(const FAntFerry& F, float Now, FVector& Out) const
{
	const FAntTrailLine* L = LineById(F.Trail);
	if (!L)
	{
		return false;
	}
	const float D = FMath::Max(0.f, F.From - (Now - F.Start) * F.Speed);
	return PosAt(*L, D, Out);
}

// ---------------------------------------------------------------------------------------------
// Drawing (every machine)
// ---------------------------------------------------------------------------------------------

void AAntTrailNet::Rebuild()
{
	if (GetNetMode() == NM_DedicatedServer || !Dots)
	{
		return;
	}
	TArray<FTransform> Xf;
	TArray<float> Data;
	TArray<FTransform> DotXf;
	TArray<FTransform> LayXf;
	TArray<float> DotData;
	TArray<float> LayData;
	const AAntSnowball* Snow = AAntSnowball::Get(GetWorld());
	for (const FAntTrailLine& L : Lines)
	{
		const int32 NP = L.Points.Num();
		if (NP < 2)
		{
			continue;
		}
		// The dotted line.
		TArray<FTransform>& Into = L.bLaying ? LayXf : DotXf;
		TArray<float>& IntoData = L.bLaying ? LayData : DotData;
		float Carry = 0.f;
		for (int32 i = 0; i + 1 < NP && Into.Num() < 6000; ++i)
		{
			const FVector A = L.Points[i];
			const FVector B = L.Points[i + 1];
			const FVector Nn = L.Normals.IsValidIndex(i) ? FVector(L.Normals[i]) : FVector::UpVector;
			const float Seg = (float)FVector::Dist(A, B);
			float T = Carry;
			while (T < Seg)
			{
				const FVector P = FMath::Lerp(A, B, Seg > 0.f ? T / Seg : 0.f) + Nn * 1.2f;
				const FVector Q = P + (B - A).GetSafeNormal() * 2.f;
				const float DD[TrailLook::NumData] = { (float)P.X, (float)P.Y, (float)P.Z, (float)Q.X, (float)Q.Y, (float)Q.Z, (float)Nn.X, (float)Nn.Y, (float)Nn.Z, 0.f, 0.f, 0.f, 0.f };
				Into.Add(bHasMaterial ? FTransform(FQuat::Identity, P, FVector(1.6f)) : FTransform(FQuat::Identity, P, FVector(0.035f, 0.035f, 0.012f)));
				IntoData.Append(DD, TrailLook::NumData);
				T += TrailLook::DotStep;
			}
			Carry = T - Seg;
		}
		if (L.bLaying || L.Workers <= 0)
		{
			continue;
		}
		// The workers, two-way traffic (as in AntSwarm.cpp).
		const int32 Lv = Snow ? Snow->LvOf(L.Owner, AntBoost::WorkerSpeed) : 0;
		const float Speed0 = BaseSpeed * TrailPace() * (1.f + 0.35f * Lv);
		TArray<float> SegLen;
		float Total = 0.f;
		for (int32 i = 0; i + 1 < NP; ++i)
		{
			const float Sl = FMath::Max(1.f, (float)FVector::Dist(L.Points[i], L.Points[i + 1]));
			SegLen.Add(Sl);
			Total += Sl;
		}
		FRandomStream R(L.Seed);
		float Acc = 0.f;
		for (int32 i = 0; i + 1 < NP; ++i)
		{
			const FVector NA = L.Normals.IsValidIndex(i) ? FVector(L.Normals[i]) : FVector::UpVector;
			const FVector NB = L.Normals.IsValidIndex(i + 1) ? FVector(L.Normals[i + 1]) : FVector::UpVector;
			FVector N = (NA + NB).GetSafeNormal();
			if (N.IsNearlyZero()) { N = NA; }
			const FVector A = FVector(L.Points[i]) + NA * 2.f;
			const FVector B = FVector(L.Points[i + 1]) + NB * 2.f;
			Acc += L.Workers * SegLen[i] / Total;
			const int32 Here = FMath::FloorToInt(Acc);
			Acc -= Here;
			const FVector Mid = (A + B) * 0.5f;
			const float Scale = (SegLen[i] + 160.f) / 100.f;
			const int32 Out = (Here + 1) / 2;
			const int32 Back = Here - Out;
			for (int32 k = 0; k < Here; ++k)
			{
				const bool bBack = k >= Out;
				const int32 Idx = bBack ? k - Out : k;
				const int32 InLane = FMath::Max(1, bBack ? Back : Out);
				const float Sp = Speed0 * R.FRandRange(0.97f, 1.03f) * (bBack ? -1.f : 1.f);
				const float Phase = FMath::Frac((Idx + R.FRandRange(-0.25f, 0.25f)) / InLane);
				const float Lat = (bBack ? 1.f : -1.f) * (11.f + R.FRandRange(-4.f, 4.f)) * AAntCharacter::AntShrink;
				const float D[TrailLook::NumData] = { (float)A.X, (float)A.Y, (float)A.Z, (float)B.X, (float)B.Y, (float)B.Z, (float)N.X, (float)N.Y, (float)N.Z, Sp, Phase, Lat, 0.f };
				FTransform T;
				if (bHasMaterial)
				{
					T = FTransform(FQuat::Identity, Mid, FVector(Scale));
				}
				else
				{
					const float t = bBack ? 1.f - Phase : Phase;
					const FVector F = (B - A).GetSafeNormal() * (bBack ? -1.f : 1.f);
					T = FTransform(FRotationMatrix::MakeFromXZ(F, N).ToQuat(), FMath::Lerp(A, B, t), FVector(0.2f, 0.12f, 0.1f));
				}
				Xf.Add(T);
				Data.Append(D, TrailLook::NumData);
			}
		}
	}
	for (UInstancedStaticMeshComponent* C : WorkerParts)
	{
		C->ClearInstances();
		if (Xf.Num() > 0)
		{
			C->AddInstances(Xf, false, true);
			for (int32 i = 0; i < Xf.Num(); ++i)
			{
				C->SetCustomData(i, TArrayView<const float>(Data.GetData() + i * TrailLook::NumData, TrailLook::NumData), false);
			}
		}
		C->MarkRenderStateDirty();
	}
	auto FillDots = [this](UInstancedStaticMeshComponent* C, const TArray<FTransform>& T, const TArray<float>& D)
	{
		C->ClearInstances();
		if (T.Num() == 0) { return; }
		C->AddInstances(T, false, true);
		if (bHasMaterial)
		{
			for (int32 i = 0; i < T.Num(); ++i) { C->SetCustomData(i, TArrayView<const float>(D.GetData() + i * TrailLook::NumData, TrailLook::NumData), false); }
		}
		C->MarkRenderStateDirty();
	};
	FillDots(Dots, DotXf, DotData);
	FillDots(LayDots, LayXf, LayData);
}

void AAntTrailNet::DrawFerries()
{
	if (FerryCrumbs.Num() == 0)
	{
		return;
	}
	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float Now = GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < FerryCrumbs.Num(); ++i)
	{
		FVector P;
		const bool bShow = Ferries.IsValidIndex(i) && FerryPos(Ferries[i], Now, P);
		FerryCrumbs[i]->SetVisibility(bShow);
		FerryAnts[i]->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		const FAntFerry& F = Ferries[i];
		const FAntTrailLine* L = LineById(F.Trail);
		FVector Ahead;
		const float D = FMath::Max(0.f, F.From - (Now - F.Start) * F.Speed);
		PosAt(*L, FMath::Max(0.f, D - 20.f), Ahead);
		const FVector Dir = (Ahead - P).GetSafeNormal();
		const float R = F.Size;
		FerryCrumbs[i]->SetWorldLocation(P + Dir * 5.f + FVector(0.f, 0.f, 5.f + R * 0.5f));
		FerryCrumbs[i]->SetWorldScale3D(FVector(R / 50.f));
		if (FerryMIDs.IsValidIndex(i) && FerryMIDs[i]) { FerryMIDs[i]->SetVectorParameterValue(TEXT("Color"), FLinearColor(F.Color)); }
		FerryAnts[i]->SetWorldLocation(P + FVector(0.f, 0.f, 1.5f));
		if (!Dir.IsNearlyZero()) { FerryAnts[i]->SetWorldRotation(FRotationMatrix::MakeFromX(Dir).Rotator()); }
		const UStaticMesh* M = FerryAnts[i]->GetStaticMesh();
		const float Len = M ? FMath::Max(1.f, (float)M->GetBoundingBox().GetSize().X) : 100.f;
		FerryAnts[i]->SetWorldScale3D(FVector(28.f * 0.7f * AAntCharacter::AntShrink / Len));
	}
}

void AAntTrailNet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	DrawFerries();
	TimeSync -= DeltaSeconds;
	if (TimeSync <= 0.f)
	{
		TimeSync = 1.f;
		float Offset = 0.f;
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
		{
			Offset = (float)(GS->GetServerWorldTimeSeconds() - GetWorld()->GetTimeSeconds());
		}
		for (UMaterialInstanceDynamic* M : MIDs) { M->SetScalarParameterValue(TEXT("TimeOffset"), Offset); }
	}
}

// ---------------------------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------------------------

float AAntTrailNet::MaxLength(const APlayerState* PS) const
{
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	const AAntRaid* Raid = AAntRaid::Get(GetWorld());
	const int32 Lv = S ? S->LvOf(PS, AntBoost::TrailLen) : 0;
	return 6000.f * (1.f + 0.5f * Lv) + 1500.f * (Raid ? Raid->Nest.Tier_(AntUp::Carriers) : 0);
}

int32 AAntTrailNet::TrailsOf(const APlayerState* PS) const
{
	int32 N = 0;
	for (const FAntTrailLine& L : Lines) { N += (L.Owner == PS && !L.bLaying) ? 1 : 0; }
	return N;
}

int32 AAntTrailNet::WorkersOnTrails(const APlayerState* PS) const
{
	int32 N = 0;
	for (const FAntTrailLine& L : Lines) { N += L.Owner == PS ? L.Workers : 0; }
	return N;
}

bool AAntTrailNet::ToggleLay(AAntCharacter* Ant)
{
	if (!Ant || Ant->bDead || !Ant->GetPlayerState())
	{
		return false;
	}
	APlayerState* PS = Ant->GetPlayerState();
	for (int32 i = 0; i < Laying.Num(); ++i)
	{
		if (Laying[i].Ant.Get() != Ant)
		{
			continue;
		}
		const int32 Li = Laying[i].Line;
		const bool bFromNest = Laying[i].bFromNest;
		Laying.RemoveAt(i);
		if (!Lines.IsValidIndex(Li))
		{
			return true;
		}
		if (bFromNest)
		{
			Lines[Li].Points.Add(Ant->GetActorLocation() - Ant->GetVisualUp() * (AAntCharacter::BodyRadius - 2.f));
			Lines[Li].Normals.Add(Ant->GetVisualUp());
			Finish(Li);
		}
		else
		{
			RemoveLine(Li, true);
			Ant->Toast(TEXT("Тропа отменена: она должна доходить до гнезда"));
		}
		return true;
	}
	FAntTrailLine L;
	L.Owner = PS;
	L.bLaying = true;
	L.Id = NextId++;
	L.Points.Add(Ant->GetActorLocation() - Ant->GetVisualUp() * (AAntCharacter::BodyRadius - 2.f));
	L.Normals.Add(Ant->GetVisualUp());
	FLaying Lay;
	Lay.Ant = Ant;
	Lay.bFromNest = Ant->IsInNest() || FVector::Dist2D(Ant->GetActorLocation(), AKitchenBuilder::NestLocation(0)) < AKitchenBuilder::NestRadius(0) + 250.f;
	Lay.Line = Lines.Add(L);
	Laying.Add(Lay);
	Ant->Toast(Lay.bFromNest ? TEXT("Тропа из гнезда: ползи к еде, [T] ещё раз — конец тропы")
		: TEXT("Тропа от этого места: ползи в гнездо, у входа тропа готова ([T] — отменить)"));
	Rebuild();
	return true;
}

void AAntTrailNet::LayTick()
{
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	for (int32 i = Laying.Num() - 1; i >= 0; --i)
	{
		AAntCharacter* Ant = Laying[i].Ant.Get();
		const int32 Li = Laying[i].Line;
		if (!Lines.IsValidIndex(Li))
		{
			Laying.RemoveAt(i);
			continue;
		}
		FAntTrailLine& L = Lines[Li];
		if (!Ant || Ant->bDead)
		{
			Laying.RemoveAt(i);
			RemoveLine(Li, true);
			continue;
		}
		const FVector Up = Ant->GetVisualUp();
		const FVector P = Ant->GetActorLocation() - Up * (AAntCharacter::BodyRadius - 2.f);
		if (FVector::Dist(P, FVector(L.Points.Last())) >= PointStep && Ant->IsCrawling())
		{
			L.Points.Add(P);
			L.Normals.Add(Up);
			Measure(L);
			if (L.Length > MaxLength(L.Owner))
			{
				// Out of pheromone. Laid from the nest: the trail ends here. Laid towards the nest: it never got there, it is dropped.
				const bool bFromNest = Laying[i].bFromNest;
				Laying.RemoveAt(i);
				if (bFromNest)
				{
					Ant->Toast(TEXT("Феромона хватило только досюда: тропа готова"));
					Finish(Li);
				}
				else
				{
					Ant->Toast(TEXT("Феромон кончился раньше, чем ты дошёл до гнезда: тропы нет. Начни у гнезда"));
					RemoveLine(Li, true);
				}
				continue;
			}
		}
		if (!Laying[i].bFromNest && Ant->IsInNest())
		{
			Laying.RemoveAt(i);
			Finish(Li);
		}
	}
}

void AAntTrailNet::Finish(int32 Index)
{
	if (!Lines.IsValidIndex(Index))
	{
		return;
	}
	FAntTrailLine& L = Lines[Index];
	const FVector Nest = AKitchenBuilder::NestLocation(0) + FVector(0.f, 0.f, 2.f);
	// Nest first: a trail laid towards the nest is turned round.
	if (L.Points.Num() > 0 && FVector::Dist2D(FVector(L.Points.Last()), Nest) < FVector::Dist2D(FVector(L.Points[0]), Nest))
	{
		Algo::Reverse(L.Points);
		Algo::Reverse(L.Normals);
	}
	if (L.Points.Num() == 0 || FVector::Dist2D(FVector(L.Points[0]), Nest) > 120.f)
	{
		L.Points.Insert(Nest, 0);
		L.Normals.Insert(FVector::UpVector, 0);
	}
	L.bLaying = false;
	L.Seed = FMath::Rand();
	Measure(L);
	APlayerState* TrailOwner = L.Owner;
	const uint16 Id = L.Id;
	if (L.Points.Num() < 3 || L.Length < 400.f)
	{
		RemoveLine(Index, true);
		if (AAntPlayerController* PC = TrailOwner ? Cast<AAntPlayerController>(TrailOwner->GetOwner()) : nullptr) { PC->ClientToast(TEXT("Тропа слишком короткая")); }
		return;
	}
	// Too many trails: the oldest of this player's goes.
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	const int32 Max = 1 + (S ? FMath::Min(2, S->LvOf(TrailOwner, AntBoost::TrailCount)) : 0);
	while (TrailsOf(TrailOwner) > Max)
	{
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Lines[i].Owner == TrailOwner && !Lines[i].bLaying && Lines[i].Id != Id) { RemoveLine(i, true); break; }
		}
	}
	Rebalance(TrailOwner);
	const FAntTrailLine* Done = LineById(Id);
	const int32 W = Done ? Done->Workers : 0;
	UE_LOG(LogTemp, Display, TEXT("ANTTRAIL laid %d by %s: %d points, length %.0f, workers %d"), (int32)Id, TrailOwner ? *TrailOwner->GetPlayerName() : TEXT("?"), Done ? Done->Points.Num() : 0, Done ? Done->Length : 0.f, W);
	if (AAntPlayerController* PC = TrailOwner ? Cast<AAntPlayerController>(TrailOwner->GetOwner()) : nullptr)
	{
		PC->ClientToast(W > 0 ? FString::Printf(TEXT("Тропа готова: %d рабочих понесут крошки с неё домой"), W)
			: FString(TEXT("Тропа готова, но рабочих нет: возьми «Рабочих» при следующем выборе")));
	}
	Rebuild();
	ForceNetUpdate();
}

int32 AAntTrailNet::DevLay(APlayerState* PS, const TArray<FVector>& Pts)
{
	if (!PS || Pts.Num() < 2)
	{
		return 0;
	}
	FAntTrailLine L;
	L.Owner = PS;
	L.Id = NextId++;
	// Densify the given corners and drop every point onto the floor / furniture under it.
	for (int32 i = 0; i + 1 < Pts.Num(); ++i)
	{
		const float Seg = (float)FVector::Dist2D(Pts[i], Pts[i + 1]);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Seg / PointStep));
		for (int32 k = 0; k < Steps || (i + 2 == Pts.Num() && k == Steps); ++k)
		{
			FVector P = FMath::Lerp(Pts[i], Pts[i + 1], (float)k / Steps);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(TrailDrop), false);
			if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0.f, 0.f, 120.f), P - FVector(0.f, 0.f, 600.f), ECC_Visibility, Q))
			{
				P = Hit.ImpactPoint;
			}
			L.Points.Add(P);
			L.Normals.Add(FVector::UpVector);
		}
	}
	L.bLaying = true;
	const int32 Idx = Lines.Add(L);
	Finish(Idx);
	return LineById(L.Id) ? L.Id : 0;
}

void AAntTrailNet::RemoveLine(int32 Index, bool bWorkersHome)
{
	if (!Lines.IsValidIndex(Index))
	{
		return;
	}
	const uint16 Id = Lines[Index].Id;
	APlayerState* TrailOwner = Lines[Index].Owner;
	for (int32 f = Ferries.Num() - 1; f >= 0; --f)
	{
		FVector P;
		if (Ferries[f].Trail == Id)
		{
			const AGameStateBase* GS = GetWorld()->GetGameState();
			FerryPos(Ferries[f], GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds(), P);
			DropFerry(f, P);
		}
	}
	for (int32 p = Pickups.Num() - 1; p >= 0; --p)
	{
		if (Pickups[p].Trail == Id) { Claimed.Remove(Pickups[p].Load); Pickups.RemoveAt(p); }
	}
	Lines.RemoveAt(Index);
	for (FLaying& L : Laying)
	{
		if (L.Line > Index) { --L.Line; } else if (L.Line == Index) { L.Line = -1; }
	}
	Rebalance(TrailOwner);
	Rebuild();
}

int32 AAntTrailNet::Recall(APlayerState* PS)
{
	int32 N = 0;
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		if (Lines[i].Owner == PS && !Lines[i].bLaying) { RemoveLine(i, true); ++N; }
	}
	return N;
}

void AAntTrailNet::Clear()
{
	for (int32 f = Ferries.Num() - 1; f >= 0; --f)
	{
		if (AKitchenLoot* L = Ferries[f].Load.Get()) { L->Destroy(); }
	}
	Ferries.Reset();
	Pickups.Reset();
	Claimed.Reset();
	Laying.Reset();
	Lines.Reset();
	Ferried = 0;
	CutsDone = 0;
	AmbushAt = 0.f;
	Rebuild();
	ForceNetUpdate();
}

void AAntTrailNet::Rebalance(APlayerState* PS)
{
	if (!PS || !HasAuthority())
	{
		return;
	}
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	const FAntSnowPlayer* P = S ? S->Find(PS) : nullptr;
	const int32 Total = P ? P->Workers : 0;
	float Len = 0.f;
	for (const FAntTrailLine& L : Lines) { if (L.Owner == PS && !L.bLaying) { Len += FMath::Max(1.f, L.Length); } }
	int32 Left = Total;
	int32 Last = -1;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		FAntTrailLine& L = Lines[i];
		if (L.Owner != PS || L.bLaying) { continue; }
		L.Workers = Len > 0.f ? FMath::FloorToInt(Total * FMath::Max(1.f, L.Length) / Len) : 0;
		Left -= L.Workers;
		Last = i;
	}
	if (Last >= 0) { Lines[Last].Workers += FMath::Max(0, Left); }
	Rebuild();
	ForceNetUpdate();
}

void AAntTrailNet::ServerTick(float Dt)
{
	if (Laying.Num() > 0)
	{
		LayTick();
	}
	SlowClock += Dt;
	if (SlowClock < 0.25f)
	{
		return;
	}
	const float Step = SlowClock;
	SlowClock = 0.f;
	for (FAntTrailLine& L : Lines)
	{
		if (!L.bLaying && L.Workers > 0) { Forage(L, Step); }
	}
	ArriveFerries();
	static float DangerClock = 0.f;
	DangerClock += Step;
	if (DangerClock >= 0.5f)
	{
		DangerClock = 0.f;
		Dangers();
	}
}

void AAntTrailNet::Forage(FAntTrailLine& L, float Dt)
{
	if (L.Dist.Num() != L.Points.Num()) { Measure(L); }
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	const AAntRaid* Raid = AAntRaid::Get(GetWorld());
	const int32 Carry = 6 + 4 * (S ? S->LvOf(L.Owner, AntBoost::WorkerCarry) : 0) + (Raid ? Raid->Nest.Tier_(AntUp::Carriers) : 0);
	const float Reach = 300.f + 500.f * (S ? S->LvOf(L.Owner, AntBoost::Forage) : 0);
	const float Speed = BaseSpeed * TrailPace() * (1.f + 0.35f * (S ? S->LvOf(L.Owner, AntBoost::WorkerSpeed) : 0));
	int32 Busy = 0;
	for (const FAntFerry& F : Ferries) { Busy += F.Trail == L.Id ? 1 : 0; }
	for (const FPickup& P : Pickups) { Busy += P.Trail == L.Id ? 1 : 0; }
	const int32 Room = (TrailPace() > 1.f ? FMath::Max(2, L.Workers / 2) : FMath::Max(1, L.Workers / 3)) - Busy;   // v12: more of them out at once
	if (Room <= 0)
	{
		return;
	}
	// Bounding box of the trail, grown by the reach.
	FBox Box(ForceInit);
	for (const FVector_NetQuantize& P : L.Points) { Box += FVector(P); }
	Box = Box.ExpandBy(FMath::Max(Reach, 700.f) + 60.f);
	TArray<TPair<float, AKitchenLoot*>> Found;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* F = *It;
		if (!F || F->bIsGear || F->bMain || F->bBody || F->bVault || F->bHeld || F->IsCarried() || F->Weight > Carry || F->IsHidden() || Claimed.Contains(F))
		{
			continue;
		}
		const FVector P = F->GetActorLocation();
		if (!Box.IsInside(P) || FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) < AKitchenBuilder::NestRadius(0))
		{
			continue;
		}
		float D2;
		const float S0 = NearestOnLine(L, P, D2);
		// The far end of a trail is where the food is: there the workers search wider (700, or the forage reach if larger).
		const float R = S0 > L.Length - 150.f ? FMath::Max(Reach, 700.f) : Reach;
		if (D2 > R * R || KitchenContainers::SealedAt(GetWorld(), P) >= 0)
		{
			continue;
		}
		Found.Add(TPair<float, AKitchenLoot*>(FMath::Sqrt(D2) + S0 * 0.05f, F));
	}
	Found.Sort([](const TPair<float, AKitchenLoot*>& A, const TPair<float, AKitchenLoot*>& B) { return A.Key < B.Key; });
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 k = 0; k < FMath::Min(Room, Found.Num()); ++k)
	{
		AKitchenLoot* F = Found[k].Value;
		float D2;
		const float S0 = NearestOnLine(L, F->GetActorLocation(), D2);
		const float Off = FMath::Sqrt(D2);
		// A worker already out on the trail turns aside: the nearer the busier the trail.
		const float Delay = FMath::FRandRange(0.4f, 1.f) * FMath::Min(S0, 2200.f) / Speed * 0.5f + 2.f * Off / Speed + 0.5f;
		FPickup Pk;
		Pk.Load = F;
		Pk.Trail = L.Id;
		Pk.At = Now + Delay;
		Pk.From = S0;
		Pickups.Add(Pk);
		Claimed.Add(F);
	}
}

void AAntTrailNet::ArriveFerries()
{
	const float Now = GetWorld()->GetTimeSeconds();
	const AAntSnowball* S = AAntSnowball::Get(GetWorld());
	// Pickups whose worker got there.
	for (int32 p = Pickups.Num() - 1; p >= 0; --p)
	{
		FPickup& Pk = Pickups[p];
		AKitchenLoot* F = Pk.Load.Get();
		const FAntTrailLine* L = LineById(Pk.Trail);
		if (!F || !L || F->bHeld || F->IsCarried())
		{
			Claimed.Remove(Pk.Load);
			Pickups.RemoveAt(p);
			continue;
		}
		if (Now < Pk.At)
		{
			continue;
		}
		FAntFerry Fy;
		Fy.Trail = Pk.Trail;
		Fy.Start = Now;
		Fy.From = Pk.From;
		Fy.Speed = BaseSpeed * TrailPace() * (1.f + 0.35f * (S ? S->LvOf(L->Owner, AntBoost::WorkerSpeed) : 0));
		Fy.Color = F->Color.ToFColor(true);
		Fy.Size = (uint8)FMath::Clamp(FMath::RoundToInt(4.f + F->Weight * 1.2f), 4, 18);
		Fy.Load = F;
		F->SquadPick(this, RootComponent, F->GetActorLocation());
		F->SetActorHiddenInGame(true);
		Ferries.Add(Fy);
		Pickups.RemoveAt(p);
	}
	// Ferries home; the hidden load rides along (so a red ant that snatches it drops it where the worker was).
	for (int32 f = Ferries.Num() - 1; f >= 0; --f)
	{
		FAntFerry& Fy = Ferries[f];
		AKitchenLoot* Load = Fy.Load.Get();
		const FAntTrailLine* L = LineById(Fy.Trail);
		if (!Load || !L || !Load->bHeld || Load->SquadHolder.Get() != this)
		{
			if (Load) { Load->SetActorHiddenInGame(false); }
			Claimed.Remove(Fy.Load);
			Ferries.RemoveAt(f);
			continue;
		}
		FVector P;
		PosAt(*L, FMath::Max(0.f, Fy.From - (Now - Fy.Start) * Fy.Speed), P);
		Load->SetActorRelativeLocation(P);
		if (Fy.From - (Now - Fy.Start) * Fy.Speed > 0.f)
		{
			continue;
		}
		// Home: delivered like any food (the snowball credits the trail's owner).
		APlayerState* TrailOwner = L->Owner;
		Ferries.RemoveAt(f);
		Claimed.Remove(Load);
		Load->SquadDrop();
		Load->SetActorHiddenInGame(false);
		Load->SetActorLocation(AKitchenBuilder::NestLocation(0) + FVector(0.f, 0.f, 30.f));
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			AntSnowball::SetCredit(TrailOwner);
			TArray<AAntCharacter*> None;
			GM->OnFoodDelivered(0, Load, None);
			AntSnowball::SetCredit(nullptr);
		}
		++Ferried;
		UE_LOG(LogTemp, Display, TEXT("ANTTRAIL ferried «%s» (%d) home on trail %d"), *Load->LootName, Load->Value, (int32)L->Id);
		Load->Destroy();
	}
}

void AAntTrailNet::DropFerry(int32 FerryIndex, const FVector& At)
{
	if (!Ferries.IsValidIndex(FerryIndex))
	{
		return;
	}
	AKitchenLoot* Load = Ferries[FerryIndex].Load.Get();
	Claimed.Remove(Ferries[FerryIndex].Load);
	Ferries.RemoveAt(FerryIndex);
	if (Load)
	{
		Load->SetActorRelativeLocation(At + FVector(0.f, 0.f, 15.f));
		Load->SquadDrop();
		Load->SetActorHiddenInGame(false);
		Load->SetActorLocation(At + FVector(0.f, 0.f, 15.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

int32 AAntTrailNet::Cut(const FVector& Center, float Radius, const TCHAR* Cause)
{
	if (!HasAuthority())
	{
		return 0;
	}
	int32 Killed = 0;
	AAntSnowball* S = AAntSnowball::Get(GetWorld());
	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float Now = GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	for (int32 li = Lines.Num() - 1; li >= 0; --li)
	{
		FAntTrailLine& L = Lines[li];
		if (L.bLaying || L.Points.Num() < 2)
		{
			continue;
		}
		if (L.Dist.Num() != L.Points.Num()) { Measure(L); }
		int32 Hit = -1;
		for (int32 i = 1; i < L.Points.Num(); ++i)
		{
			if (FVector::DistSquared(FVector(L.Points[i]), Center) < FMath::Square(Radius + 25.f)) { Hit = i; break; }
		}
		if (Hit < 0)
		{
			continue;
		}
		const float CutAt = L.Dist[Hit - 1];
		const float Lost = FMath::Max(0.f, L.Length - CutAt);
		// The ones under the blow die; the rest of the cut-off stretch run home.
		const int32 Under = FMath::Min(L.Workers, FMath::CeilToInt(L.Workers * FMath::Min(1.f, 2.f * Radius / FMath::Max(1.f, L.Length))) + 1);
		Killed += Under;
		APlayerState* TrailOwner = L.Owner;
		const uint16 Id = L.Id;
		for (int32 f = Ferries.Num() - 1; f >= 0; --f)
		{
			if (Ferries[f].Trail != Id) { continue; }
			const float D = Ferries[f].From - (Now - Ferries[f].Start) * Ferries[f].Speed;
			if (D > CutAt - 30.f)
			{
				FVector P;
				FerryPos(Ferries[f], Now, P);
				DropFerry(f, P);
			}
		}
		if (FAntSnowPlayer* P = S ? S->Find(TrailOwner) : nullptr) { P->Workers = FMath::Max(0, P->Workers - Under); }
		++CutsDone;
		AntAudio::Everyone(this, EAntSfx::TrailCut, Center, 1.f);   // the snip of a cut trail (+ the workers' squeak)
		UE_LOG(LogTemp, Display, TEXT("ANTTRAIL cut %d (%s) at %s r %.0f: kept %.0f of %.0f, %d workers killed"), (int32)Id, Cause, *Center.ToCompactString(), Radius, CutAt, L.Length, Under);
		if (CutAt < 400.f || Hit <= 2)
		{
			RemoveLine(li, true);
		}
		else
		{
			L.Points.SetNum(Hit);
			L.Normals.SetNum(FMath::Min(L.Normals.Num(), Hit));
			Measure(L);
			for (int32 p = Pickups.Num() - 1; p >= 0; --p)
			{
				if (Pickups[p].Trail == Id && Pickups[p].From > CutAt) { Claimed.Remove(Pickups[p].Load); Pickups.RemoveAt(p); }
			}
			Rebalance(TrailOwner);
		}
		if (AAntPlayerController* PC = TrailOwner ? Cast<AAntPlayerController>(TrailOwner->GetOwner()) : nullptr)
		{
			if (Now - LastCutMsg > 3.f)
			{
				LastCutMsg = Now;
				PC->ClientToast(FString::Printf(TEXT("Тропу перерезали (%s): −%d рабочих. Проложи заново [T]"), Cause, Under));
			}
		}
		(void)Lost;
	}
	if (Killed > 0)
	{
		Rebuild();
		ForceNetUpdate();
	}
	return Killed;
}

void AAntTrailNet::Dangers()
{
	UWorld* W = GetWorld();
	AAntSnowball* S = AAntSnowball::Get(W);
	const float Now = W->GetTimeSeconds();
	// The Human, grandma and the kid: a foot coming down on a trail cuts it.
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* H = *It;
		if (!H || H->IsHidden())
		{
			continue;
		}
		// Moving? (by where he was half a second ago: puppets and the AI do not always move through the velocity)
		const TWeakObjectPtr<AActor> Key(H);
		if (!HumanWas.Contains(Key)) { HumanWas.Add(Key, H->GetActorLocation()); continue; }
		FVector& Was = HumanWas[Key];
		const bool bMoving = FVector::Dist2D(Was, H->GetActorLocation()) > 25.f || H->GetVelocity().Size2D() > 60.f;
		Was = H->GetActorLocation();
		if (!bMoving)
		{
			continue;
		}
		const float HalfH = H->GetCapsuleComponent() ? H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 900.f;
		const FVector Feet = H->GetActorLocation() - FVector(0.f, 0.f, HalfH);
		for (int32 li = Lines.Num() - 1; li >= 0; --li)
		{
			if (!Lines.IsValidIndex(li) || Lines[li].bLaying) { continue; }
			const FAntTrailLine& L = Lines[li];
			for (const FVector_NetQuantize& P : L.Points)
			{
				if (P.Z < Feet.Z + 200.f && P.Z > Feet.Z - 200.f && FVector::DistSquared2D(FVector(P), Feet) < FMath::Square(230.f))
				{
					Cut(FVector(P), 90.f, H->Fig == HumanFig::Grandma ? TEXT("шаг бабушки") : H->Fig == HumanFig::Kid ? TEXT("шаг малыша") : TEXT("шаг Человека"));
					break;
				}
			}
		}
	}
	// Red ants on a trail: they bite the workers and knock the crumbs out of their jaws.
	const AGameStateBase* GS = W->GetGameState();
	const float SNow = GS ? (float)GS->GetServerWorldTimeSeconds() : Now;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		const AAntCharacter* R = *It;
		if (!R || R->bDead || R->Team != 1 || R->Creature != 0)
		{
			continue;
		}
		const FVector RP = R->GetActorLocation();
		for (FAntTrailLine& L : Lines)
		{
			if (L.bLaying || L.Workers <= 0) { continue; }
			float D2;
			NearestOnLine(L, RP, D2);
			if (D2 > FMath::Square(170.f)) { continue; }
			--L.Workers;
			if (FAntSnowPlayer* P = S ? S->Find(L.Owner) : nullptr) { P->Workers = FMath::Max(0, P->Workers - 1); }
			for (int32 f = Ferries.Num() - 1; f >= 0; --f)
			{
				FVector FP;
				if (Ferries[f].Trail == L.Id && FerryPos(Ferries[f], SNow, FP) && FVector::DistSquared(FP, RP) < FMath::Square(260.f)) { DropFerry(f, FP); }
			}
			UE_LOG(LogTemp, Display, TEXT("ANTTRAIL red ant %s on trail %d: a worker killed (%d left)"), *R->GetDisplayName(), (int32)L.Id, L.Workers);
			Rebuild();
			break;
		}
	}
	// The dog's nose: workers leave scent on the floor, so Badrik walks the trail (his paws and tongue then cut it, AAntArmy::KillInSphere).
	for (const FAntTrailLine& L : Lines)
	{
		if (L.bLaying || L.Workers <= 0) { continue; }
		const float Add = FMath::Min(0.35f, 0.04f + 0.004f * L.Workers);
		for (int32 i = 0; i < L.Points.Num(); i += 6)
		{
			if (L.Points[i].Z < 260.f) { NpcWorld::ScentAdd(W, FVector(L.Points[i]), Add); }
		}
	}
	// A busy trail draws the red clan's ambush once in a while (their decoy rule: they go and sit on that spot).
	AAntRaid* Raid = AAntRaid::Get(W);
	if (Raid && Raid->Clan && Raid->ClanAlert > 0 && Now >= AmbushAt)
	{
		for (const FAntTrailLine& L : Lines)
		{
			if (L.bLaying || L.Workers < 15 || L.Length < 2500.f) { continue; }
			if (FMath::FRand() < 0.5f / 60.f)
			{
				FVector At;
				PosAt(L, L.Length * 0.6f, At);
				Raid->Clan->Decoy(At, 30.f);
				AmbushAt = Now + 90.f;
				if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
				{
					GM->Broadcast(FString::Printf(TEXT("Красные учуяли тропу %s: засада на тропе!"), L.Owner ? *L.Owner->GetPlayerName() : TEXT("")), TrailLook::Bad);
				}
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL red ambush on trail %d at %s"), (int32)L.Id, *At.ToCompactString());
				break;
			}
		}
	}
}

// =============================================================================================
// Hooks
// =============================================================================================

namespace AntTrail
{
	bool OnToggleKey(AAntCharacter* Ant)
	{
		if (!Ant || !AntSnowball::Enabled(Ant))
		{
			return false;
		}
		const AAntRaid* Raid = AAntRaid::Get(Ant->GetWorld());
		if (!Raid || !Raid->InRaid())
		{
			return true;
		}
		if (AAntTrailNet* T = AAntTrailNet::Get(Ant->GetWorld(), true))
		{
			T->ToggleLay(Ant);
		}
		return true;
	}

	bool OnRecallKey(AAntCharacter* Ant)
	{
		if (!Ant || !AntSnowball::Enabled(Ant))
		{
			return false;
		}
		if (AAntTrailNet* T = AAntTrailNet::Get(Ant->GetWorld()))
		{
			const int32 N = T->Recall(Ant->GetPlayerState());
			Ant->Toast(N > 0 ? FString::Printf(TEXT("Твои тропы стёрты (%d), рабочие ждут в гнезде"), N) : FString(TEXT("У тебя нет троп")));
		}
		return true;
	}

	void OnBlow(UWorld* World, const FVector& Center, float Radius)
	{
		if (AAntTrailNet* T = World ? AAntTrailNet::Get(World) : nullptr)
		{
			if (T->Lines.Num() > 0) { T->Cut(Center, Radius, TEXT("удар")); }
		}
	}
}
