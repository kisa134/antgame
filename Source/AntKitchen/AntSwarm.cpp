#include "AntSwarm.h"
#include "AntArmy.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "AntTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace SwarmLook
{
	const FLinearColor Body(0.055f, 0.022f, 0.014f);
	const FLinearColor Legs(0.03f, 0.012f, 0.008f);
	const FLinearColor Crumb(1.f, 0.72f, 0.25f);
	constexpr int32 NumData = 13;
}

AAntSwarm::AAntSwarm()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AAntSwarm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntSwarm, Streams);
	DOREPLIFETIME(AAntSwarm, Population);
	DOREPLIFETIME(AAntSwarm, Idle);
}

AAntSwarm* AAntSwarm::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntSwarm> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

UInstancedStaticMeshComponent* AAntSwarm::MakePart(const TCHAR* Name, const FVector& Offset, const FVector& Size, float LegMode,
	const FLinearColor& Color, const FLinearColor& Glow, bool bShadow, UStaticMesh* Mesh, const TCHAR* MaterialPath)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
	C->SetupAttachment(RootComponent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh ? Mesh : AntKit::ShapeMesh(EAntShape::Cube));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(bShadow);
	C->bAffectDistanceFieldLighting = false;
	C->SetNumCustomDataFloats(SwarmLook::NumData);
	C->RegisterComponent();
	AddInstanceComponent(C);

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MaterialPath ? MaterialPath : TEXT("/Game/Swarm/M_SwarmAnt.M_SwarmAnt"));
	bHasMaterial = Base != nullptr;
	if (Base)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, C);
		M->SetVectorParameterValue(TEXT("PartOffset"), FLinearColor(Offset.X, Offset.Y, Offset.Z, 0.f));
		M->SetVectorParameterValue(TEXT("PartScale"), FLinearColor(Size.X, Size.Y, Size.Z, 0.f));
		M->SetScalarParameterValue(TEXT("LegMode"), LegMode);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetVectorParameterValue(TEXT("Glow"), Glow);
		C->SetMaterial(0, M);
		MIDs.Add(M);
	}
	else
	{
		AntKit::Paint(C, Color);
	}
	// Remember the part layout for the no-material fallback.
	C->ComponentTags.Add(*FString::Printf(TEXT("%f,%f,%f,%f,%f,%f"), Offset.X, Offset.Y, Offset.Z, Size.X, Size.Y, Size.Z));
	return C;
}

void AAntSwarm::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	using namespace SwarmLook;
	// Workers: the same ant as the player (low-poly copy of the Meshy body), a bit smaller.
	constexpr float K = 0.7f * AAntCharacter::AntShrink;
	// The Nanite ant body: Nanite draws tens of thousands of tiny instances cheaply.
	UStaticMesh* ArtMesh = AntArt::Get(TEXT("SM_Ant_Body")).Mesh;
	const bool bArtMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Swarm/M_SwarmAntArt.M_SwarmAntArt")) != nullptr;
	if (ArtMesh && bArtMat)
	{
		const FBox B = ArtMesh->GetBoundingBox();
		const float Len = 40.f * K;
		const float S = Len / FMath::Max(1.f, (float)B.GetSize().X);
		const FVector Off = FVector(0.f, 0.f, 6.f * K) - B.GetCenter() * S;
		BodyParts.Add(MakePart(TEXT("Body"), Off, FVector(S * 100.f), 0.f, FLinearColor(1.f, 1.f, 1.f), FLinearColor::Black, true,
			ArtMesh, TEXT("/Game/Swarm/M_SwarmAntArt.M_SwarmAntArt")));
	}
	else
	{
		BodyParts.Add(MakePart(TEXT("Abdomen"), FVector(-11.f, 0.f, 6.f) * K, FVector(17.f, 11.f, 10.f) * K, 0.f, Body, FLinearColor::Black, true));
		BodyParts.Add(MakePart(TEXT("Thorax"), FVector(0.f, 0.f, 6.f) * K, FVector(10.f, 6.f, 6.f) * K, 0.f, Body, FLinearColor::Black, false));
		BodyParts.Add(MakePart(TEXT("Head"), FVector(9.f, 0.f, 7.f) * K, FVector(8.f, 8.f, 7.f) * K, 0.f, Body, FLinearColor::Black, false));
	}
	BodyParts.Add(MakePart(TEXT("LegsA"), FVector(0.f, 0.f, 3.f) * K, FVector(1.6f, 30.f, 1.4f) * K, 1.f, Legs, FLinearColor::Black, false));
	BodyParts.Add(MakePart(TEXT("LegsB"), FVector(0.f, 0.f, 3.f) * K, FVector(1.6f, 30.f, 1.4f) * K, 2.f, Legs, FLinearColor::Black, false));
	Crumbs = MakePart(TEXT("Crumbs"), FVector(9.f, 0.f, 13.f) * K, FVector(10.f, 10.f, 8.f) * K, 0.f, Crumb, Crumb * 0.25f, false);

	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* S = AntKit::AddPart(this, RootComponent, EAntShape::Cylinder, FVector::ZeroVector, FVector(1.f), FLinearColor(0.12f, 0.05f, 0.03f));
		S->SetVisibility(false);
		S->SetCastShadow(false);
		Splats.Add(S);
		SplatTimes.Add(0.f);
	}
	Rebuild();
}

// ---------------------------------------------------------------------------------------------
// Rendering: each ant = 5 instances (+ crumb) whose motion lives in the material.
// ---------------------------------------------------------------------------------------------

void AAntSwarm::OnRep_Streams()
{
	Rebuild();
}

void AAntSwarm::Rebuild()
{
	if (GetNetMode() == NM_DedicatedServer || BodyParts.Num() == 0)
	{
		return;
	}

	TArray<FTransform> Xf;
	TArray<float> Data;
	TArray<FTransform> CrumbXf;
	TArray<float> CrumbData;

	for (const FSwarmStream& S : Streams)
	{
		const int32 NP = S.Points.Num();
		if (S.Count <= 0 || NP < 2)
		{
			continue;
		}
		TArray<float> SegLen;
		float Total = 0.f;
		for (int32 i = 0; i + 1 < NP; ++i)
		{
			const float L = FMath::Max(1.f, (float)FVector::Dist(S.Points[i], S.Points[i + 1]));
			SegLen.Add(L);
			Total += L;
		}

		FRandomStream R(S.Seed);
		float Carry = 0.f;
		for (int32 i = 0; i + 1 < NP; ++i)
		{
			const FVector NA = S.Normals.IsValidIndex(i) ? FVector(S.Normals[i]) : FVector::UpVector;
			const FVector NB = S.Normals.IsValidIndex(i + 1) ? FVector(S.Normals[i + 1]) : FVector::UpVector;
			FVector N = (NA + NB).GetSafeNormal();
			if (N.IsNearlyZero()) { N = NA; }
			const FVector A = FVector(S.Points[i]) + NA * 2.f;
			const FVector B = FVector(S.Points[i + 1]) + NB * 2.f;

			Carry += S.Count * SegLen[i] / Total;
			const int32 Here = FMath::FloorToInt(Carry);
			Carry -= Here;

			const FVector Mid = (A + B) * 0.5f;
			const float Scale = (SegLen[i] + 160.f) / 100.f;
			// Two-way traffic like real ants: going out on one side, coming back (with crumbs) on the other,
			// each lane a column of ants walking one after another.
			const int32 Out = (Here + 1) / 2;
			const int32 Back = Here - Out;
			for (int32 k = 0; k < Here; ++k)
			{
				const bool bBack = k >= Out;
				const int32 Idx = bBack ? k - Out : k;
				const int32 InLane = FMath::Max(1, bBack ? Back : Out);
				const float Speed = WorkerSpeed * R.FRandRange(0.97f, 1.03f) * (bBack ? -1.f : 1.f);
				const float Phase = FMath::Frac((Idx + R.FRandRange(-0.25f, 0.25f)) / InLane);
				const float Lat = (bBack ? 1.f : -1.f) * (13.f + R.FRandRange(-6.f, 6.f)) * AAntCharacter::AntShrink;
				const float D[SwarmLook::NumData] = { (float)A.X, (float)A.Y, (float)A.Z, (float)B.X, (float)B.Y, (float)B.Z, (float)N.X, (float)N.Y, (float)N.Z, Speed, Phase, Lat, 0.f };

				FTransform T;
				if (bHasMaterial)
				{
					T = FTransform(FQuat::Identity, Mid, FVector(Scale));
				}
				else
				{
					// No material: freeze each ant at its phase position.
					const float t = bBack ? 1.f - Phase : Phase;
					FVector F = (B - A).GetSafeNormal() * (bBack ? -1.f : 1.f);
					const FVector Rt = FVector::CrossProduct(N, F).GetSafeNormal();
					T = FTransform(FRotationMatrix::MakeFromXZ(F, N).ToQuat(), FMath::Lerp(A, B, t) + Rt * Lat, FVector(0.2f, 0.12f, 0.1f));
				}
				Xf.Add(T);
				Data.Append(D, SwarmLook::NumData);
				if (bBack && S.bHasFood)
				{
					CrumbXf.Add(T);
					CrumbData.Append(D, SwarmLook::NumData);
				}
			}
		}
	}

	auto Fill = [](UInstancedStaticMeshComponent* C, const TArray<FTransform>& T, const TArray<float>& D)
	{
		if (!C)
		{
			return;
		}
		C->ClearInstances();
		if (T.Num() == 0)
		{
			return;
		}
		C->AddInstances(T, false, true);
		for (int32 i = 0; i < T.Num(); ++i)
		{
			C->SetCustomData(i, TArrayView<const float>(D.GetData() + i * SwarmLook::NumData, SwarmLook::NumData), false);
		}
		C->MarkRenderStateDirty();
	};

	for (UInstancedStaticMeshComponent* C : BodyParts)
	{
		Fill(C, Xf, Data);
	}
	Fill(Crumbs, CrumbXf, CrumbData);
}

void AAntSwarm::MulticastSplat_Implementation(FVector_NetQuantize Location, int32 Killed)
{
	if (Splats.Num() == 0)
	{
		return;
	}
	UStaticMeshComponent* S = Splats[NextSplat];
	SplatTimes[NextSplat] = 6.f;
	NextSplat = (NextSplat + 1) % Splats.Num();
	const float R = FMath::Clamp(150.f + Killed * 2.f, 150.f, 420.f);
	S->SetWorldLocation(FVector(Location) + FVector(0.f, 0.f, 2.f));
	S->SetWorldScale3D(FVector(R / 50.f, R / 50.f, 0.02f));
	S->SetVisibility(true);
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AAntSwarm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		ServerTimer += DeltaSeconds;
		if (ServerTimer >= 1.f)
		{
			ServerTimer -= 1.f;
			ServerTick();
		}
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Keep the GPU clock in sync with the server, so everyone sees the swarm in the same place.
	TimeSyncTimer -= DeltaSeconds;
	if (TimeSyncTimer <= 0.f)
	{
		TimeSyncTimer = 1.f;
		float Offset = 0.f;
		if (const AGameStateBase* GS = GetWorld()->GetGameState())
		{
			Offset = (float)(GS->GetServerWorldTimeSeconds() - GetWorld()->GetTimeSeconds());
		}
		for (UMaterialInstanceDynamic* M : MIDs)
		{
			M->SetScalarParameterValue(TEXT("TimeOffset"), Offset);
		}
	}

	for (int32 i = 0; i < Splats.Num(); ++i)
	{
		if (SplatTimes[i] > 0.f)
		{
			SplatTimes[i] -= DeltaSeconds;
			if (SplatTimes[i] <= 0.f)
			{
				Splats[i]->SetVisibility(false);
			}
		}
	}
}

void AAntSwarm::ServerTick()
{
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	bool bDirty = false;
	const FBox Burner = AKitchenBuilder::HotBurner().ExpandBy(40.f);

	for (int32 i = Streams.Num() - 1; i >= 0; --i)
	{
		FSwarmStream& S = Streams[i];

		// Trails over the hot burner lose ants every second.
		for (int32 p = 0; p + 1 < S.Points.Num(); ++p)
		{
			const FVector A = S.Points[p];
			const FVector B = S.Points[p + 1];
			if (Burner.IsInside(A) || Burner.IsInside(B) || FMath::LineBoxIntersection(Burner, A, B, B - A))
			{
				Kill(S, FMath::Max(1, S.Count / 20));
				bDirty = true;
				break;
			}
		}

		if (S.bHasFood)
		{
			AKitchenLoot* F = S.Food.Get();
			if (!F || F->bIsGear || F->IsCarried() || FVector::Dist(F->GetActorLocation(), FVector(S.Points.Last())) > 450.f)
			{
				S.bHasFood = false;
				bDirty = true;
			}
			else
			{
				const int32 Gain = FMath::Clamp(S.Count / 400, 1, F->Value);
				F->Value -= Gain;
				F->OnRep_Visual();
				if (GM)
				{
					GM->OnSwarmIncome(Gain);
				}
				if (F->Value <= 0)
				{
					if (GM)
					{
						GM->Broadcast(FString::Printf(TEXT("Рой %s доел «%s». Нужна новая тропа!"), *S.OwnerName, *F->LootName), FLinearColor(1.f, 0.8f, 0.3f));
					}
					F->Destroy();
					S.bHasFood = false;
					bDirty = true;
				}
			}
		}
		else
		{
			S.IdleTime += 1.f;
		}

		if (S.Count <= 0)
		{
			RemoveStream(i, false);
			bDirty = true;
		}
		else if (!S.bHasFood && S.IdleTime > 20.f)
		{
			RemoveStream(i, true);
			bDirty = true;
		}
	}

	if (bDirty && GetNetMode() != NM_DedicatedServer)
	{
		Rebuild();
	}
}

// ---------------------------------------------------------------------------------------------
// Server API
// ---------------------------------------------------------------------------------------------

void AAntSwarm::DebugSpawnTestStreams(int32 TotalAnts)
{
	Streams.Reset();
	const FVector Nest = AKitchenBuilder::NestLocation();
	auto Line = [](TArray<FVector>& P, TArray<FVector>& N, const FVector& From, const FVector& To, const FVector& Normal)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt((float)FVector::Dist(From, To) / 140.f));
		for (int32 i = (P.Num() > 0 ? 1 : 0); i <= Steps; ++i)
		{
			P.Add(FMath::Lerp(From, To, (float)i / Steps));
			N.Add(Normal);
		}
	};

	TArray<TArray<FVector>> AllP;
	TArray<TArray<FVector>> AllN;
	{   // floor: nest -> middle of the kitchen -> trash corner
		TArray<FVector> P, N;
		Line(P, N, Nest, FVector(-1800.f, -2600.f, 0.f), FVector::UpVector);
		Line(P, N, FVector(-1800.f, -2600.f, 0.f), FVector(4800.f, -1200.f, 0.f), FVector::UpVector);
		AllP.Add(P); AllN.Add(N);
	}
	{   // up the west wall to the shelf
		TArray<FVector> P, N;
		Line(P, N, Nest, FVector(-7000.f, -1800.f, 0.f), FVector::UpVector);
		Line(P, N, FVector(-7000.f, -1800.f, 0.f), FVector(-7000.f, -600.f, 2600.f), FVector::ForwardVector);
		Line(P, N, FVector(-7000.f, -600.f, 2600.f), FVector(-6600.f, -600.f, 2680.f), FVector::UpVector);
		AllP.Add(P); AllN.Add(N);
	}
	{   // floor to a table leg, up the leg, across the table
		TArray<FVector> P, N;
		Line(P, N, Nest, FVector(-1610.f, -1600.f, 0.f), FVector::UpVector);
		Line(P, N, FVector(-1610.f, -1600.f, 0.f), FVector(-1610.f, -1600.f, 1300.f), -FVector::ForwardVector);
		Line(P, N, FVector(-1610.f, -1600.f, 1400.f), FVector(1500.f, -500.f, 1400.f), FVector::UpVector);
		AllP.Add(P); AllN.Add(N);
	}

	float TotalLen = 0.f;
	TArray<float> Lens;
	for (const TArray<FVector>& P : AllP)
	{
		float L = 0.f;
		for (int32 i = 1; i < P.Num(); ++i) { L += FVector::Dist(P[i - 1], P[i]); }
		Lens.Add(L);
		TotalLen += L;
	}
	for (int32 k = 0; k < AllP.Num(); ++k)
	{
		FSwarmStream S;
		for (int32 i = 0; i < AllP[k].Num(); ++i)
		{
			S.Points.Add(AllP[k][i]);
			S.Normals.Add(AllN[k][i]);
		}
		S.Length = Lens[k];
		S.Count = FMath::Max(1, FMath::RoundToInt(TotalAnts * Lens[k] / TotalLen));
		S.Seed = 1000 + k;
		S.bHasFood = (k == 0);
		S.OwnerName = TEXT("[test]");
		S.IdleTime = -100000.f;
		Streams.Add(S);
	}
	Population = FMath::Max(Population, TotalAnts + Idle);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Rebuild();
	}
}

int32 AAntSwarm::CountStreams(const FString& OwnerName) const
{
	int32 N = 0;
	for (const FSwarmStream& S : Streams)
	{
		if (S.OwnerName == OwnerName) { ++N; }
	}
	return N;
}

int32 AAntSwarm::AntsOnTrails() const
{
	int32 N = 0;
	for (const FSwarmStream& S : Streams)
	{
		N += S.Count;
	}
	return N;
}

int32 AAntSwarm::CreateStream(const TArray<FVector>& Points, const TArray<FVector>& Normals, AKitchenLoot* Food, AAntCharacter* By, FString& Why)
{
	if (Points.Num() < 2)
	{
		Why = TEXT("Тропа слишком короткая");
		return 0;
	}
	const FString TrailOwner = By ? By->GetDisplayName() : FString(TEXT("Колония"));
	if (CountStreams(TrailOwner) >= MaxStreamsPerPlayer)
	{
		// Replace this player's oldest trail.
		for (int32 i = 0; i < Streams.Num(); ++i)
		{
			if (Streams[i].OwnerName == TrailOwner)
			{
				RemoveStream(i, true);
				break;
			}
		}
	}
	if (Streams.Num() >= MaxStreams)
	{
		RemoveStream(0, true);
	}
	if (Idle < 30)
	{
		Why = TEXT("В муравейнике нет свободных рабочих. Носи еду — колония вырастет");
		return 0;
	}

	FSwarmStream S;
	float Len = 0.f;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		S.Points.Add(Points[i]);
		S.Normals.Add(Normals.IsValidIndex(i) ? Normals[i] : FVector::UpVector);
		if (i > 0)
		{
			Len += FVector::Dist(Points[i - 1], Points[i]);
		}
	}
	S.Length = FMath::Max(1.f, Len);
	S.Count = FMath::Clamp(FMath::RoundToInt(Len * AntsPerUnit), 30, Idle);
	S.Seed = FMath::Rand();
	S.Food = Food;
	S.bHasFood = Food != nullptr;
	S.OwnerName = TrailOwner;
	Idle -= S.Count;
	Streams.Add(S);

	if (GetNetMode() != NM_DedicatedServer)
	{
		Rebuild();
	}
	return S.Count;
}

void AAntSwarm::RemoveStream(int32 Index, bool bReturnToNest)
{
	if (!Streams.IsValidIndex(Index))
	{
		return;
	}
	if (bReturnToNest)
	{
		Idle += Streams[Index].Count;
	}
	Streams.RemoveAt(Index);
}

int32 AAntSwarm::RecallStreams(const FString& OwnerName)
{
	int32 N = 0;
	for (int32 i = Streams.Num() - 1; i >= 0; --i)
	{
		if (Streams[i].OwnerName == OwnerName)
		{
			N += Streams[i].Count;
			RemoveStream(i, true);
		}
	}
	if (N > 0 && GetNetMode() != NM_DedicatedServer)
	{
		Rebuild();
	}
	return N;
}

void AAntSwarm::Kill(FSwarmStream& S, int32 N)
{
	N = FMath::Clamp(N, 0, S.Count);
	S.Count -= N;
	Population = FMath::Max(0, Population - N);
}

void AAntSwarm::AddPopulation(int32 Amount)
{
	const int32 Room = FMath::Max(0, 60000 - Population);
	const int32 Add = FMath::Min(Amount, Room);
	Population += Add;
	Idle += Add;
}

int32 AAntSwarm::KillInSphere(const FVector& Center, float Radius)
{
	int32 Total = AAntArmy::KillInSphere(GetWorld(), Center, Radius, 100000);
	for (FSwarmStream& S : Streams)
	{
		float Killed = 0.f;
		for (int32 i = 0; i + 1 < S.Points.Num(); ++i)
		{
			const FVector A = S.Points[i];
			const FVector B = S.Points[i + 1];
			const FVector C = FMath::ClosestPointOnSegment(Center, A, B);
			if (FMath::Abs(C.Z - Center.Z) > 260.f)
			{
				continue;
			}
			const float D = (float)FVector::Dist2D(C, Center);
			if (D < Radius)
			{
				const float Overlap = FMath::Min(2.f * FMath::Sqrt(Radius * Radius - D * D), (float)FVector::Dist(A, B));
				Killed += S.Count * Overlap / FMath::Max(1.f, S.Length);
			}
		}
		const int32 K = FMath::RoundToInt(Killed);
		if (K > 0)
		{
			Kill(S, K);
			Total += K;
		}
	}
	if (Total > 0)
	{
		MulticastSplat(Center, Total);
		if (GetNetMode() != NM_DedicatedServer)
		{
			Rebuild();
		}
	}
	return Total;
}

int32 AAntSwarm::KillInCone(const FVector& Origin, const FVector& Dir, float Range, float CosHalfAngle, float Dt)
{
	AAntArmy::KillInCone(GetWorld(), Origin, Dir, Range, CosHalfAngle, Dt);
	int32 Total = 0;
	for (FSwarmStream& S : Streams)
	{
		float Killed = 0.f;
		for (int32 i = 0; i + 1 < S.Points.Num(); ++i)
		{
			const FVector A = S.Points[i];
			const FVector B = S.Points[i + 1];
			const FVector M = (A + B) * 0.5f;
			const FVector To = M - Origin;
			const float Dist = (float)To.Size();
			if (Dist > Range || Dist < 1.f || FVector::DotProduct(To / Dist, Dir) < CosHalfAngle)
			{
				continue;
			}
			Killed += S.Count * (float)FVector::Dist(A, B) / FMath::Max(1.f, S.Length) * 0.9f * Dt;
		}
		int32 K = FMath::FloorToInt(Killed);
		if (FMath::FRand() < Killed - K)
		{
			++K;
		}
		if (K > 0)
		{
			Kill(S, K);
			Total += K;
		}
	}
	return Total;
}

bool AAntSwarm::FindStompTarget(const FVector& From, float MaxDist, FVector& OutPoint) const
{
	float Best = 0.f;
	for (const FSwarmStream& S : Streams)
	{
		for (int32 i = 0; i + 1 < S.Points.Num(); ++i)
		{
			const FVector M = (FVector(S.Points[i]) + FVector(S.Points[i + 1])) * 0.5f;
			if (M.Z > 260.f || FVector::Dist2D(M, AKitchenBuilder::NestLocation()) < AKitchenBuilder::NestRadius() + 200.f)
			{
				continue;   // only trails on the floor, not inside the nest
			}
			const float D = (float)FVector::Dist2D(M, From);
			if (D > MaxDist)
			{
				continue;
			}
			const float Density = S.Count * (float)FVector::Dist(S.Points[i], S.Points[i + 1]) / FMath::Max(1.f, S.Length);
			const float Score = Density - D * 0.01f;
			if (Density >= 8.f && Score > Best)
			{
				Best = Score;
				OutPoint = M;
			}
		}
	}
	return Best > 0.f;
}
