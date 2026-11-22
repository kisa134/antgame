#include "AntCloth.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntRun.h"
#include "AntAudio.h"
#include "AntWreck.h"
#include "AntFluidWorld.h"
#include "KitchenFixture.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV3.h"
#include "NpcRoutine.h"
#include "HumanCharacter.h"
#include "HumanBody.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "HAL/PlatformTime.h"

namespace
{
	constexpr float SimDt = 1.f / 40.f;
	constexpr float ClothGravity = 11000.f;
	constexpr float NoZ = -1.0e9f;

	struct FRule { FString Prefix; uint8 Kind; bool bExact; };
	TArray<FRule>& Rules()
	{
		static TArray<FRule> R = {
			{ TEXT("SM_V3_TableclothLong"), AntClothKind::LongCloth, true },
			{ TEXT("SM_V3_Tablecloth"), AntClothKind::Tablecloth, true },
			{ TEXT("SM_V3_CurtainPanel"), AntClothKind::Curtain, true },
			{ TEXT("SM_V31_Throw"), AntClothKind::Plaid, true },
			{ TEXT("SM_V3_BlanketFort"), AntClothKind::Fort, true },
		};
		return R;
	}

	uint8 KindFor(const UStaticMeshComponent* C)
	{
		if (!C || !C->GetStaticMesh())
		{
			return 255;
		}
		const FString N = C->GetStaticMesh()->GetName();
		for (const FRule& R : Rules())
		{
			if (R.bExact ? N == R.Prefix : N.StartsWith(R.Prefix))
			{
				if (R.Kind == AntClothKind::Curtain && C->GetComponentScale().X > 1.5f * FMath::Abs((float)C->GetComponentScale().Y))
				{
					return AntClothKind::Sheet;   // the sheets on the line in the yard are the curtain mesh, scaled
				}
				return R.Kind;
			}
		}
		return 255;
	}

	float NetNowW(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? (float)GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	}

	AAntKitchenGameMode* GMOf(UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	bool& CostLog() { static bool b = FParse::Param(FCommandLine::Get(), TEXT("AntClothLog")); return b; }

	/** Every machine, ~0.5 s after the raid's fixtures are respawned: what in the level is cloth becomes cloth (server spawns). */
	void Scan(UWorld* W)
	{
		if (!W || W->GetNetMode() == NM_Client || !AntCloth::Enabled())
		{
			return;
		}
		int32 N = 0;
		TArray<UStaticMeshComponent*> Found;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* C = *It;
			if (!C || C->GetWorld() != W || !C->IsRegistered() || !C->IsVisible() || !C->GetOwner() || C->GetOwner()->IsA<AAntCloth>() || C->GetOwner()->IsHidden())
			{
				continue;
			}
			if (KindFor(C) != 255) { Found.Add(C); }
		}
		for (UStaticMeshComponent* C : Found)
		{
			const uint8 K = KindFor(C);
			AActor* Owner = C->GetOwner();
			AKitchenFixture* Fx = Cast<AKitchenFixture>(Owner);
			int32 Row = -1;
			if (AKitchenBuilder* B = Cast<AKitchenBuilder>(Owner))
			{
				const int32 NumRows = LevelV3::BaseArt().Num();
				for (int32 r = 0; r < NumRows; ++r) { if (B->ArtComponent(r) == C) { Row = r; break; } }
			}
			const FTransform T(FRotator::ZeroRotator, C->Bounds.Origin);
			AAntCloth* A = W->SpawnActorDeferred<AAntCloth>(AAntCloth::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!A) { continue; }
			A->Kind = K;
			A->SourceMesh = C->GetStaticMesh()->GetFName();
			A->SourceAt = C->Bounds.Origin;
			A->SourceFix = Fx;
			A->SourceRow = Row;
			A->FinishSpawning(T);
			++N;
		}
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH scan: %d pieces of cloth adopted"), N);
	}
}

// =====================================================================================================================

bool AntCloth::Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoCloth"));
	return !bOff;
}

int32& AntCloth::WindMode()
{
	static int32 M = -1;
	return M;
}

void AntCloth::AddMeshRule(const TCHAR* MeshPrefix, uint8 Kind)
{
	Rules().Insert({ FString(MeshPrefix), Kind, false }, 0);
}

void AntCloth::ResetAll(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || !Enabled())
	{
		return;
	}
	for (TActorIterator<AAntCloth> It(World); It; ++It) { It->Destroy(); }
	FTimerHandle H;
	TWeakObjectPtr<UWorld> WW(World);
	World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WW]() { if (WW.IsValid()) { Scan(WW.Get()); } }), 0.6f, false);
}

AAntCloth* AntCloth::ForFixture(const AKitchenFixture* Fx)
{
	if (!Fx || !Fx->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AAntCloth> It(Fx->GetWorld()); It; ++It)
	{
		if (It->SourceFix == Fx) { return *It; }
	}
	return nullptr;
}

bool AntCloth::Yank(AKitchenFixture* Fx, const FVector& Dir, AAntCharacter* By)
{
	AAntCloth* C = ForFixture(Fx);
	if (!C)
	{
		return false;
	}
	C->Yank(Dir, 70.f, 3.5f, By);
	return true;
}

bool AntCloth::OnBite(AAntCharacter* Ant)
{
	if (!Ant || !Ant->GetWorld() || !Enabled())
	{
		return false;
	}
	FVector F = Ant->GetController() ? Ant->GetController()->GetControlRotation().Vector() : Ant->GetActorForwardVector();
	const FVector P = Ant->GetActorLocation() + F.GetSafeNormal() * 22.f;
	for (TActorIterator<AAntCloth> It(Ant->GetWorld()); It; ++It)
	{
		if (It->Bounds().ExpandBy(150.f).IsInside(P) && It->BiteAt(P, Ant))
		{
			return true;
		}
	}
	return false;
}

double AntCloth::FrameMs(const UWorld* World, int32* OutAwake, int32* OutParticles)
{
	double Ms = 0.0;
	int32 Awake = 0, Np = 0;
	if (World)
	{
		for (TActorIterator<AAntCloth> It(World); It; ++It)
		{
			Ms += It->MeanMs();
			Awake += It->IsAwake() ? 1 : 0;
			Np += It->NumParticles();
		}
	}
	if (OutAwake) { *OutAwake = Awake; }
	if (OutParticles) { *OutParticles = Np; }
	return Ms;
}

// =====================================================================================================================

AAntCloth::AAntCloth()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(2.f);
	InteractRadius = 130.f;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ClothMesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetAbsolute(true, true, true);
	Mesh->bUseAsyncCooking = true;
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->CastShadow = true;
}

void AAntCloth::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntCloth, Kind);
	DOREPLIFETIME(AAntCloth, SourceMesh);
	DOREPLIFETIME(AAntCloth, SourceAt);
	DOREPLIFETIME(AAntCloth, SourceFix);
	DOREPLIFETIME(AAntCloth, SourceRow);
	DOREPLIFETIME(AAntCloth, Grabs);
	DOREPLIFETIME(AAntCloth, PinOff);
	DOREPLIFETIME(AAntCloth, DeadQuads);
	DOREPLIFETIME(AAntCloth, Wet);
	DOREPLIFETIME(AAntCloth, Wind);
	DOREPLIFETIME(AAntCloth, Mode);
	DOREPLIFETIME(AAntCloth, Key);
	DOREPLIFETIME(AAntCloth, HeapAt);
	DOREPLIFETIME(AAntCloth, HeapRadius);
}

void AAntCloth::BeginPlay()
{
	Super::BeginPlay();
	if (FindSource())
	{
		BuildFromSource();
	}
}

void AAntCloth::EndPlay(const EEndPlayReason::Type Reason)
{
	HideSource(false);
	for (TWeakObjectPtr<UStaticMeshComponent>& C : CupsSrc)
	{
		if (C.IsValid()) { C->SetVisibility(true); }
	}
	Super::EndPlay(Reason);
}

FString AAntCloth::KindName() const
{
	switch (Kind)
	{
	case AntClothKind::Tablecloth: return TEXT("скатерть");
	case AntClothKind::LongCloth: return TEXT("длинная скатерть");
	case AntClothKind::Curtain: return TEXT("штора");
	case AntClothKind::Plaid: return TEXT("плед");
	case AntClothKind::Fort: return TEXT("шалаш");
	case AntClothKind::Sheet: return TEXT("простыня");
	default: return TEXT("ткань");
	}
}

FString AAntCloth::DoorNoun() const { return KindName(); }

bool AAntCloth::FindSource()
{
	if (Source.IsValid())
	{
		return true;
	}
	UStaticMeshComponent* Best = nullptr;
	double BestD = 160.0 * 160.0;
	auto Consider = [&](UStaticMeshComponent* C)
	{
		if (!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetFName() != SourceMesh)
		{
			return;
		}
		const double D = FVector::DistSquared(C->Bounds.Origin, FVector(SourceAt));
		if (D < BestD) { BestD = D; Best = C; }
	};
	if (SourceFix)
	{
		TInlineComponentArray<UStaticMeshComponent*> Comps;
		SourceFix->GetComponents(Comps);
		for (UStaticMeshComponent* C : Comps) { Consider(C); }
	}
	else
	{
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->GetOwner() && !It->GetOwner()->IsA<AAntCloth>()) { Consider(*It); }
		}
	}
	Source = Best;
	return Best != nullptr;
}

void AAntCloth::HideSource(bool bHide)
{
	UStaticMeshComponent* S = Source.Get();
	if (!S)
	{
		return;
	}
	S->SetVisibility(!bHide, false);
	if (Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth || Kind == AntClothKind::Plaid || Kind == AntClothKind::Fort)
	{
		// the cloth's own copy carries the ants (cooked when it rests); the piece of the level must not stay in the way
		S->SetCollisionEnabled(bHide ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}

void AAntCloth::BuildFromSource()
{
	UStaticMeshComponent* S = Source.Get();
	if (!S || bBuilt)
	{
		return;
	}
	bBuilt = true;
	const FBox B = S->Bounds.GetBox();
	const FVector C = B.GetCenter();
	const FVector E = B.GetExtent();
	// the piece's own look (the M_Surface tint is darkened when the cloth gets wet)
	if (UMaterialInterface* M = S->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* SM = Cast<UMaterialInstanceDynamic>(M))
		{
			Mid = UMaterialInstanceDynamic::Create(SM->Parent, this);   // (a dynamic instance cannot parent another: copy its values)
			Mid->CopyParameterOverrides(SM);
		}
		else
		{
			Mid = UMaterialInstanceDynamic::Create(M, this);
		}
		bHaveTint = Mid->GetVectorParameterValue(FName(TEXT("Tint")), BaseTint);
		Mesh->SetMaterial(0, Mid);
	}
	Mesh->SetCollisionObjectType(S->GetCollisionObjectType());
	Mesh->SetCollisionResponseToChannels(S->GetCollisionResponseToChannels());
	Mesh->ComponentTags = S->ComponentTags;   // the footstep surface (Surf_Fabric)

	TArray<FVector> Flat;   // rest-length geometry: the flat sheet (or the sampled shape)
	TArray<uint8> Missing;
	const bool bTable = Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth;
	const bool bHang = Kind == AntClothKind::Curtain || Kind == AntClothKind::Sheet;
	if (bTable)
	{
		const float Drop = FMath::Max(60.f, (float)(B.Max.Z - B.Min.Z) - 20.f);
		const float TW = (float)E.X - 6.f, TD = (float)E.Y - 6.f;
		const float SU = 2.f * TW + 2.f * Drop, SV = 2.f * TD + 2.f * Drop;
		NU = FMath::Clamp(FMath::RoundToInt(SU / 175.f) + 1, 12, 29);
		NV = FMath::Clamp(FMath::RoundToInt(SV / 175.f) + 1, 10, 22);
		const float Top = (float)B.Max.Z;
		X.SetNum(NU * NV);
		Flat.SetNum(NU * NV);
		for (int32 v = 0; v < NV; ++v)
		{
			for (int32 u = 0; u < NU; ++u)
			{
				const float fu = -SU * 0.5f + SU * u / (NU - 1), fv = -SV * 0.5f + SV * v / (NV - 1);
				const float du = FMath::Max(0.f, FMath::Abs(fu) - TW), dv = FMath::Max(0.f, FMath::Abs(fv) - TD);
				const float x = (float)C.X + FMath::Sign(fu) * (FMath::Min(FMath::Abs(fu), TW) + (du > 0.f ? 8.f + du * 0.1f : 0.f));
				const float y = (float)C.Y + FMath::Sign(fv) * (FMath::Min(FMath::Abs(fv), TD) + (dv > 0.f ? 8.f + dv * 0.1f : 0.f));
				const float z = Top + 3.f - FMath::Sqrt(du * du + dv * dv) * ((du > 0.f && dv > 0.f) ? 0.85f : 1.f);
				X[Idx(u, v)] = FVector(x, y, z);
				Flat[Idx(u, v)] = FVector(fu, fv, 0.f);
			}
		}
		CellUU = FMath::Min(SU / (NU - 1), SV / (NV - 1));
		Thickness = 5.f;
	}
	else if (bHang)
	{
		const bool bAlongX = E.X >= E.Y;
		const FVector UAx = bAlongX ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 1.f, 0.f);
		const FVector DAx = bAlongX ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
		const float W = (float)(bAlongX ? E.X : E.Y), Dp = (float)(bAlongX ? E.Y : E.X);
		const float H = (float)(B.Max.Z - B.Min.Z);
		const bool bSheet = Kind == AntClothKind::Sheet;
		const float Gather = bSheet ? 1.05f : 1.65f;
		const float SU = 2.f * W * Gather, SV = FMath::Max(100.f, H - (bSheet ? 10.f : 30.f));
		NU = bSheet ? FMath::Clamp(FMath::RoundToInt(SU / 90.f) + 1, 10, 18) : FMath::Clamp(FMath::RoundToInt(SU / 85.f) + 1, 8, 16);
		NV = bSheet ? FMath::Clamp(FMath::RoundToInt(SV / 90.f) + 1, 6, 14) : FMath::Clamp(FMath::RoundToInt(SV / 150.f) + 1, 12, 36);
		X.SetNum(NU * NV);
		Flat.SetNum(NU * NV);
		PinAt.SetNum(NU * NV);
		Pinned.SetNumZeroed(NU * NV);
		for (int32 v = 0; v < NV; ++v)
		{
			for (int32 u = 0; u < NU; ++u)
			{
				const float fu = -SU * 0.5f + SU * u / (NU - 1), fv = SV * v / (NV - 1);
				const float Zig = bSheet ? 0.f : ((u % 2) ? 1.f : -1.f) * FMath::Min(Dp * 0.7f, 28.f);
				const FVector Pin = FVector(C.X, C.Y, B.Max.Z - 12.f) + UAx * ((u / (float)(NU - 1) - 0.5f) * 2.f * W * (bSheet ? 1.f : 0.96f)) + DAx * Zig;
				X[Idx(u, v)] = Pin - FVector(0.f, 0.f, fv);
				Flat[Idx(u, v)] = FVector(fu, fv, 0.f);
				if (v == 0) { Pinned[Idx(u, v)] = 1; PinAt[Idx(u, v)] = Pin; }
			}
		}
		CellUU = FMath::Min(SU / (NU - 1), SV / (NV - 1));
		Thickness = 5.f;
		bPins = true;
		// the window's side: nothing goes behind the wall the curtain hangs at
		FCollisionQueryParams Q(TEXT("ClothWall"), false, this);
		Q.AddIgnoredComponent(S);
		if (SourceFix) { Q.AddIgnoredActor(SourceFix); }
		for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It)
		{
			for (int32 r = 0; r < LevelV3::BaseArt().Num(); ++r) { if (UStaticMeshComponent* Px = It->CurtainProxy(r)) { Q.AddIgnoredComponent(Px); } }
		}
		FHitResult H1, H2;
		const FVector Mid0 = FVector(C.X, C.Y, C.Z);
		const bool b1 = GetWorld()->LineTraceSingleByChannel(H1, Mid0, Mid0 + DAx * 700.f, ECC_WorldStatic, Q);
		const bool b2 = GetWorld()->LineTraceSingleByChannel(H2, Mid0, Mid0 - DAx * 700.f, ECC_WorldStatic, Q);
		if (b1 || b2)
		{
			const bool bBackPlus = b1 && (!b2 || H1.Distance < H2.Distance);
			WallN = bBackPlus ? -DAx : DAx;
			WallD = (float)FVector::DotProduct(WallN, Mid0) - Dp - 10.f;
		}
	}
	else
	{
		// The plaid, the fort: the piece's own surface sampled from above (it keeps its shape until it is pulled).
		NU = FMath::Clamp(FMath::RoundToInt(2.f * (float)E.X / 100.f) + 1, 10, 26);
		NV = FMath::Clamp(FMath::RoundToInt(2.f * (float)E.Y / 100.f) + 1, 10, 26);
		X.SetNum(NU * NV);
		Missing.SetNumZeroed(NU * NV);
		TArray<uint8> Hit;
		Hit.SetNumZeroed(NU * NV);
		for (int32 v = 0; v < NV; ++v)
		{
			for (int32 u = 0; u < NU; ++u)
			{
				const float x = (float)B.Min.X + 2.f * (float)E.X * u / (NU - 1), y = (float)B.Min.Y + 2.f * (float)E.Y * v / (NV - 1);
				FHitResult H;
				const bool b = S->LineTraceComponent(H, FVector(x, y, B.Max.Z + 60.f), FVector(x, y, B.Min.Z - 60.f), FCollisionQueryParams(TEXT("ClothSample"), true));
				Hit[Idx(u, v)] = b ? 1 : 0;
				X[Idx(u, v)] = FVector(x, y, b ? (float)H.ImpactPoint.Z + 2.f : (float)B.Min.Z + 3.f);
			}
		}
		for (int32 v = 0; v < NV; ++v)
		{
			for (int32 u = 0; u < NU; ++u)
			{
				if (Hit[Idx(u, v)]) { continue; }
				bool bNear = false;
				for (int32 dv = -1; dv <= 1; ++dv) { for (int32 du = -1; du <= 1; ++du)
				{
					const int32 uu = u + du, vv = v + dv;
					if (uu >= 0 && vv >= 0 && uu < NU && vv < NV && Hit[Idx(uu, vv)]) { bNear = true; }
				} }
				Missing[Idx(u, v)] = bNear ? 0 : 1;
			}
		}
		Flat = X;
		CellUU = FMath::Min(2.f * (float)E.X / (NU - 1), 2.f * (float)E.Y / (NV - 1));
		Thickness = 4.f;
	}
	if (Pinned.Num() != X.Num()) { Pinned.SetNumZeroed(X.Num()); PinAt.SetNum(X.Num()); }
	Prev = X;
	InvM.Init(1.f, X.Num());
	QuadAlive.Init(1, (NU - 1) * (NV - 1));
	if (Missing.Num())
	{
		for (int32 v = 0; v < NV - 1; ++v) { for (int32 u = 0; u < NU - 1; ++u)
		{
			if (Missing[Idx(u, v)] || Missing[Idx(u + 1, v)] || Missing[Idx(u, v + 1)] || Missing[Idx(u + 1, v + 1)]) { QuadAlive[v * (NU - 1) + u] = 2; }   // 2: never there
		} }
	}
	// edges from the flat geometry
	Edges.Reset();
	auto Add = [&](int32 a, int32 b, float K, int32 Q0, int32 Q1)
	{
		Edges.Add({ a, b, (float)FVector::Dist(Flat[a], Flat[b]), K, Q0, Q1 });
	};
	auto Q = [&](int32 u, int32 v) { return (u >= 0 && v >= 0 && u < NU - 1 && v < NV - 1) ? v * (NU - 1) + u : -1; };
	const float Bend = bHang ? 0.06f : 0.14f;
	for (int32 v = 0; v < NV; ++v)
	{
		for (int32 u = 0; u < NU; ++u)
		{
			if (u + 1 < NU) { Add(Idx(u, v), Idx(u + 1, v), 1.f, Q(u, v - 1), Q(u, v)); }
			if (v + 1 < NV) { Add(Idx(u, v), Idx(u, v + 1), 1.f, Q(u - 1, v), Q(u, v)); }
			if (u + 1 < NU && v + 1 < NV) { Add(Idx(u, v), Idx(u + 1, v + 1), 0.45f, Q(u, v), -2); Add(Idx(u + 1, v), Idx(u, v + 1), 0.45f, Q(u, v), -2); }
			if (u + 2 < NU) { Add(Idx(u, v), Idx(u + 2, v), -Bend, Q(u, FMath::Min(v, NV - 2)), Q(u + 1, FMath::Min(v, NV - 2))); }
			if (v + 2 < NV) { Add(Idx(u, v), Idx(u, v + 2), -Bend, Q(FMath::Min(u, NU - 2), v), Q(FMath::Min(u, NU - 2), v + 1)); }
		}
	}
	FlatPos = Flat;
	SourceTop = (float)B.Max.Z;
	HfInit();
	// the tablecloth's cups ride it (copies on this actor: the fixture may be hidden with a broken table)
	if (SourceFix && bTable)
	{
		SourceFix->bHeldWork = false;   // the [E] is the cloth's now (every machine: the prompt follows it)
		TInlineComponentArray<UStaticMeshComponent*> Comps;
		SourceFix->GetComponents(Comps);
		for (UStaticMeshComponent* P : Comps)
		{
			if (!P || P == S || !P->GetStaticMesh() || !P->IsVisible() || !P->GetName().StartsWith(TEXT("FixVarPart"))) { continue; }
			UStaticMeshComponent* Cp = NewObject<UStaticMeshComponent>(this);
			Cp->SetStaticMesh(P->GetStaticMesh());
			for (int32 m = 0; m < P->GetNumMaterials(); ++m) { Cp->SetMaterial(m, P->GetMaterial(m)); }
			Cp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Cp->SetupAttachment(Root);
			Cp->SetAbsolute(true, true, true);
			Cp->RegisterComponent();
			Cp->SetWorldTransform(P->GetComponentTransform());
			P->SetVisibility(false);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CupsSrc.Add(P);
			Cups.Add(Cp);
			const int32 Near = NearestParticle(P->Bounds.Origin);
			CupIdx.Add(Near);
			CupOff.Add(P->GetComponentLocation() - X[FMath::Max(0, Near)]);
			CupFall.Add(-1.f);
			CupVel.Add(FVector::ZeroVector);
			CupDone.Add(0);
		}
	}
	HideSource(Kind != AntClothKind::Plaid && Kind != AntClothKind::Fort);
	// Settle the drape once (every machine does the same).
	HfBudget = 100000;
	PreRelax(bTable ? 150 : bHang ? 90 : 0);
	Rest = X;
	Prev = X;
	if (SourceFix && bTable)
	{
		for (int32 c = 0; c < Cups.Num(); ++c)
		{
			const int32 Near = CupIdx[c];
			if (X.IsValidIndex(Near)) { CupOff[c] = CupsSrc[c].IsValid() ? CupsSrc[c]->GetComponentLocation() - X[Near] : FVector::ZeroVector; }
		}
	}
	if (HasAuthority()) { FindRiders(); }
	{
		// where it lies (the log tells a sunk drape from a good one)
		float T = NoZ, U = NoZ, G = NoZ;
		HfSample((float)C.X, (float)C.Y, T, U, G);
		float Lo = 1e9f, Hi = -1e9f;
		for (const FVector& P : X) { Lo = FMath::Min(Lo, (float)P.Z); Hi = FMath::Max(Hi, (float)P.Z); }
		FHitResult H;
		FString What = TEXT("-");
		if (GetWorld()->LineTraceSingleByObjectType(H, FVector(C.X, C.Y, HfStartZ), FVector(C.X, C.Y, HfStartZ - 9000.f), HfObjects, HfParams) && H.GetComponent())
		{
			What = FString::Printf(TEXT("%s/%s z %.0f"), *GetNameSafe(H.GetActor()), *H.GetComponent()->GetName(), H.ImpactPoint.Z);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s lies: source box z %.0f..%.0f, under its middle top %.0f under %.0f ground %.0f (%s), particles z %.0f..%.0f"),
			*KindName(), B.Min.Z, B.Max.Z, T, U, G, *What, Lo, Hi);
		if (bTable && FParse::Param(FCommandLine::Get(), TEXT("AntClothLog")))
		{
			for (int32 sy = 0; sy < 3; ++sy) { for (int32 sx = 0; sx < 3; ++sx)
			{
				const float px = (float)(B.Min.X + (B.Max.X - B.Min.X) * (0.15f + 0.35f * sx)), py = (float)(B.Min.Y + (B.Max.Y - B.Min.Y) * (0.15f + 0.35f * sy));
				float T1 = NoZ, U1 = NoZ, G1 = NoZ;
				HfSample(px, py, T1, U1, G1);
				FString W1 = TEXT("-");
				if (GetWorld()->LineTraceSingleByObjectType(H, FVector(px, py, HfStartZ), FVector(px, py, HfStartZ - 9000.f), HfObjects, HfParams) && H.GetComponent())
				{
					W1 = FString::Printf(TEXT("%s/%s %s"), *GetNameSafe(H.GetActor()), *H.GetComponent()->GetName(), H.GetComponent()->IsA<UStaticMeshComponent>() && Cast<UStaticMeshComponent>(H.GetComponent())->GetStaticMesh() ? *Cast<UStaticMeshComponent>(H.GetComponent())->GetStaticMesh()->GetName() : TEXT(""));
				}
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH   hf (%.0f,%.0f): top %.0f under %.0f ground %.0f first %s"), px, py, T1, U1, G1, *W1);
			} }
			int32 LoI = 0;
			for (int32 i = 0; i < X.Num(); ++i) { if (X[i].Z < X[LoI].Z) { LoI = i; } }
			float T1 = NoZ, U1 = NoZ, G1 = NoZ;
			HfSample((float)X[LoI].X, (float)X[LoI].Y, T1, U1, G1);
			UE_LOG(LogTemp, Display, TEXT("ANTCLOTH   lowest particle %d (u %d v %d) at %s: top %.0f under %.0f ground %.0f; hf start z %.0f"), LoI, LoI % NU, LoI / NU, *X[LoI].ToCompactString(), T1, U1, G1, HfStartZ);
		}
	}
	bTopoDirty = true;
	const bool bShowNow = Kind != AntClothKind::Plaid && Kind != AntClothKind::Fort;
	Mesh->SetVisibility(bShowNow);
	UpdateMesh(true, bShowNow && !bHang);
	StillFor = bShowNow ? 0.f : 99.f;   // the plaid and the fort keep the level's own mesh until somebody pulls them
	bRestMesh = !bShowNow;
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH built %s from %s (%s): %dx%d particles, %d edges, cell %.0f uu, cups %d, riders %d, wall %s"),
		*KindName(), *SourceMesh.ToString(), SourceFix ? *SourceFix->FixId() : TEXT("builder"), NU, NV, Edges.Num(), CellUU, Cups.Num(), Riders.Num(), WallN.IsNearlyZero() ? TEXT("-") : *WallN.ToCompactString());
}

// ---- the height field ----------------------------------------------------------------------------------------------

void AAntCloth::HfInit()
{
	FBox B(X);
	const bool bHang = Kind == AntClothKind::Curtain || Kind == AntClothKind::Sheet;
	const float Margin = bHang ? 1800.f : (Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth) ? 1500.f : 1200.f;
	B = B.ExpandBy(FVector(Margin, Margin, 0.f));
	HfCell = 40.f;
	HfMin = FVector2D(B.Min.X, B.Min.Y);
	HfW = FMath::Max(1, FMath::CeilToInt((float)(B.Max.X - B.Min.X) / HfCell));
	HfH = FMath::Max(1, FMath::CeilToInt((float)(B.Max.Y - B.Min.Y) / HfCell));
	const int32 N = HfW * HfH;
	HfTop.SetNumUninitialized(N);
	HfUnder.SetNumUninitialized(N);
	HfGround.SetNumUninitialized(N);
	HfKnown.Init(0, N);
	LayerCount.Init(0, ((HfW + 1) / 2) * ((HfH + 1) / 2));
	// curtains only see what is under their hem (the radiator, the floor): not the rod, the sill, the window
	HfStartZ = bHang ? (float)B.Min.Z + 400.f : (float)B.Max.Z + 400.f;
	HfObjects = FCollisionObjectQueryParams();
	HfObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	HfObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	HfObjects.AddObjectTypesToQuery(ECC_PhysicsBody);
	HfRefreshIgnores();
}

void AAntCloth::HfRefreshIgnores()
{
	HfParams = FCollisionQueryParams(TEXT("ClothHf"), false);
	TArray<AActor*> Ignore;
	for (TActorIterator<AAntCloth> It(GetWorld()); It; ++It) { Ignore.Add(*It); }
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It) { Ignore.Add(*It); }
	for (TActorIterator<AKitchenFixture> It(GetWorld()); It; ++It) { Ignore.Add(*It); }
	for (TActorIterator<AKitchenFastener> It(GetWorld()); It; ++It) { Ignore.Add(*It); }
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Ignore.Add(*It); }
	HfParams.AddIgnoredActors(Ignore);
	if (Source.IsValid()) { HfParams.AddIgnoredComponent(Source.Get()); }
	for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It)
	{
		// the curtains' invisible climbing boxes are not furniture
		for (int32 r = 0; r < LevelV3::BaseArt().Num(); ++r)
		{
			if (UStaticMeshComponent* Px = It->CurtainProxy(r)) { HfParams.AddIgnoredComponent(Px); }
			// what stands on a tablecloth (plates, the bowl, mugs) is not under it: every machine leaves the same pieces out
			UStaticMeshComponent* Pc = (Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth) && Source.IsValid() ? It->ArtComponent(r) : nullptr;
			if (Pc)
			{
				const FBox SB = Source->Bounds.GetBox();
				const FBox PB = Pc->Bounds.GetBox();
				const FVector PC = PB.GetCenter();
				if (PC.X > SB.Min.X && PC.X < SB.Max.X && PC.Y > SB.Min.Y && PC.Y < SB.Max.Y && PB.Min.Z > SB.Max.Z - 70.f && PB.Min.Z < SB.Max.Z + 90.f && PB.GetExtent().Size2D() < 700.f)
				{
					HfParams.AddIgnoredComponent(Pc);
				}
			}
		}
	}
	for (const FRider& R : Riders)
	{
		if (R.Piece >= 0)
		{
			for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It) { if (UStaticMeshComponent* C = It->ArtComponent(R.Piece)) { HfParams.AddIgnoredComponent(C); } }
		}
		if (R.Actor.IsValid()) { HfParams.AddIgnoredActor(R.Actor.Get()); }
	}
}

void AAntCloth::HfInvalidate(const FBox& Box)
{
	const int32 x0 = FMath::Clamp(FMath::FloorToInt(((float)Box.Min.X - HfMin.X) / HfCell), 0, HfW - 1);
	const int32 x1 = FMath::Clamp(FMath::FloorToInt(((float)Box.Max.X - HfMin.X) / HfCell), 0, HfW - 1);
	const int32 y0 = FMath::Clamp(FMath::FloorToInt(((float)Box.Min.Y - HfMin.Y) / HfCell), 0, HfH - 1);
	const int32 y1 = FMath::Clamp(FMath::FloorToInt(((float)Box.Max.Y - HfMin.Y) / HfCell), 0, HfH - 1);
	for (int32 y = y0; y <= y1; ++y) { for (int32 x = x0; x <= x1; ++x) { HfKnown[y * HfW + x] = 0; } }
}

bool AAntCloth::HfSample(float x, float y, float& Top, float& Under, float& Ground)
{
	const int32 ix = FMath::FloorToInt((x - (float)HfMin.X) / HfCell), iy = FMath::FloorToInt((y - (float)HfMin.Y) / HfCell);
	if (ix < 0 || iy < 0 || ix >= HfW || iy >= HfH)
	{
		return false;
	}
	const int32 i = iy * HfW + ix;
	if (!HfKnown[i])
	{
		if (HfBudget <= 0)
		{
			bHfStarved = true;
			return false;
		}
		--HfBudget;
		const float cx = (float)HfMin.X + (ix + 0.5f) * HfCell, cy = (float)HfMin.Y + (iy + 0.5f) * HfCell;
		UWorld* W = GetWorld();
		FHitResult H1;
		float T = NoZ, U = NoZ, G = NoZ;
		if (W->LineTraceSingleByObjectType(H1, FVector(cx, cy, HfStartZ), FVector(cx, cy, HfStartZ - 9000.f), HfObjects, HfParams))
		{
			if (H1.bStartPenetrating)
			{
				T = HfStartZ; U = NoZ; G = NoZ;   // solid all the way: a wall, the inside of a cabinet
			}
			else
			{
				T = (float)H1.ImpactPoint.Z;
				FCollisionQueryParams P2 = HfParams;
				P2.AddIgnoredComponent(H1.GetComponent());
				FHitResult H2;
				if (W->LineTraceSingleByObjectType(H2, FVector(cx, cy, T - 1.f), FVector(cx, cy, T - 9000.f), HfObjects, P2))
				{
					G = (float)H2.ImpactPoint.Z;
					FHitResult H3;
					if (T - G < 6.f)
					{
						U = NoZ;   // lying straight on what is below (a rug on the floor): one solid column
						G = NoZ;
					}
					else if (W->LineTraceSingleByObjectType(H3, FVector(cx, cy, G + 1.5f), FVector(cx, cy, T - 0.5f), HfObjects, HfParams) && !H3.bStartPenetrating)
					{
						U = (float)H3.ImpactPoint.Z;
					}
					else
					{
						U = G;   // solid from what is below up to the top (the sofa's base)
					}
				}
				else
				{
					U = NoZ;
				}
			}
		}
		HfTop[i] = T;
		HfUnder[i] = U;
		HfGround[i] = G;
		HfKnown[i] = 1;
	}
	Top = HfTop[i];
	Under = HfUnder[i];
	Ground = HfGround[i];
	return true;
}

// ---- the simulation ------------------------------------------------------------------------------------------------

namespace
{
	inline bool EdgeAliveRule(const TArray<uint8>& QA, int32 Q0, int32 Q1, float K)
	{
		auto Alive = [&QA](int32 q) { return q >= 0 && QA.IsValidIndex(q) && QA[q] == 1; };
		if (Q1 == -2) { return Alive(Q0); }       // shear: its own cell
		if (K < 0.f) { return Alive(Q0) && Alive(Q1); }   // bend: both cells it spans
		return Alive(Q0) || Alive(Q1);           // structural: either side
	}
}

void AAntCloth::SolveEdges(int32 Iter)
{
	static TArray<uint8> Live;
	Live.SetNumUninitialized(Edges.Num());
	for (int32 e = 0; e < Edges.Num(); ++e) { Live[e] = EdgeAliveRule(QuadAlive, Edges[e].Q0, Edges[e].Q1, Edges[e].K) ? 1 : 0; }
	for (int32 it = 0; it < Iter; ++it)
	{
		for (int32 e = 0; e < Edges.Num(); ++e)
		{
			if (!Live[e]) { continue; }
			const FEdge& Ed = Edges[e];
			const float wa = InvM[Ed.A], wb = InvM[Ed.B];
			const float w = wa + wb;
			if (w <= 0.f) { continue; }
			FVector D = X[Ed.B] - X[Ed.A];
			const float Len = (float)D.Size();
			if (Len < 1e-3f) { continue; }
			float K = FMath::Abs(Ed.K);
			if (Ed.K < 0.f && Len < Ed.L) { K *= 0.25f; }   // a fold bends easily
			const FVector Corr = D * (K * (Len - Ed.L) / (Len * w));
			X[Ed.A] += Corr * wa;
			X[Ed.B] -= Corr * wb;
		}
	}
}

void AAntCloth::Collide(int32 i)
{
	FVector& P = X[i];
	const FVector Q = Prev[i];
	const float R = Thickness;
	if (!WallN.IsNearlyZero())
	{
		const float D = (float)FVector::DotProduct(WallN, P) - WallD;
		if (D < 0.f) { P -= WallN * D; }
	}
	float Top, Under, Ground;
	bHfStarved = false;
	if (!HfSample((float)P.X, (float)P.Y, Top, Under, Ground) || Top <= NoZ * 0.5f)
	{
		if (bHfStarved)
		{
			// not traced yet (this frame's traces are spent): it waits instead of falling through what may be there
			P.Z = FMath::Max((float)P.Z, (float)Q.Z);
			bHfStarved = false;
		}
		return;
	}
	auto Layer = [&]() -> float
	{
		const int32 cx = FMath::Clamp(FMath::FloorToInt(((float)P.X - (float)HfMin.X) / (HfCell * 2.f)), 0, (HfW + 1) / 2 - 1);
		const int32 cy = FMath::Clamp(FMath::FloorToInt(((float)P.Y - (float)HfMin.Y) / (HfCell * 2.f)), 0, (HfH + 1) / 2 - 1);
		uint16& N = LayerCount[cy * ((HfW + 1) / 2) + cx];
		const float L = FMath::Min<float>(N, 12.f) * 6.f;
		++N;
		return L;
	};
	const float Fric = Mode == 2 ? 0.4f : 0.3f;
	// Static friction: what lies on something does not creep while nobody pulls (the hanging part alone never drags a
	// tablecloth off: real cloth holds by its weight on the top); a pulled cloth slides with a little rub.
	const float Stick = Grabs.Num() > 0 ? 0.5f : FVector(Wind).SizeSquared() > 1.f ? 0.f : 1e9f;   // the draught lifts a hem off the floor
	auto Rub = [&]()
	{
		const float Dx = (float)(P.X - Q.X), Dy = (float)(P.Y - Q.Y);
		const float L = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (L <= Stick) { P.X = Q.X; P.Y = Q.Y; return; }
		const float K = (L - Stick) / L * (1.f - Fric);
		P.X = Q.X + Dx * K;
		P.Y = Q.Y + Dy * K;
	};
	if (P.Z < Top + R && P.Z > Under - R)
	{
		if (Q.Z >= Top + R - 4.f || (Under <= NoZ * 0.5f && Q.Z >= Top - 40.f))
		{
			P.Z = Top + R + Layer();
			Rub();
		}
		else if (Under > NoZ * 0.5f && Q.Z <= Under - R + 4.f)
		{
			P.Z = Under - R;
		}
		else
		{
			P.X = Q.X;   // came at it from the side: stays where it was
			P.Y = Q.Y;
		}
	}
	else if (Ground > NoZ * 0.5f && P.Z < Ground + R)
	{
		if (Q.Z >= Ground + R - 4.f)
		{
			P.Z = Ground + R + Layer();
			Rub();
		}
		else
		{
			P.X = Q.X;
			P.Y = Q.Y;
			P.Z = FMath::Max((float)P.Z, Ground + R);
		}
	}
	else if (P.Z > Top + R + 2.f || P.Z < Under - R)
	{
		// Hanging free beside an edge (the table top): the sheet between this particle and the one on top must not cut
		// through the corner, so it keeps half a cell clear of a slab whose top is above it.
		const float H = FMath::Clamp(CellUU * 0.45f, 20.f, 60.f);
		const FVector2D Dirs[4] = { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) };
		for (const FVector2D& D : Dirs)
		{
			float T2, U2, G2;
			if (HfSample((float)P.X + D.X * H, (float)P.Y + D.Y * H, T2, U2, G2) && T2 > NoZ * 0.5f && T2 > P.Z + R && T2 < P.Z + R + CellUU * 1.5f)
			{
				// no closer to that edge (it is never pushed away: that would drag the whole cloth off the table)
				const float Toward = (float)((P.X - Q.X) * D.X + (P.Y - Q.Y) * D.Y);
				if (Toward > 0.f) { P.X -= D.X * Toward; P.Y -= D.Y * Toward; }
			}
		}
	}
}

void AAntCloth::Step(float Dt)
{
	const int32 N = X.Num();
	const float G = ClothGravity * (1.f + 0.5f * Wet);
	const float Damp = Mode == 2 ? 0.95f : (0.988f - 0.02f * Wet);
	const float Dt2 = Dt * Dt;
	const FVector Wnd = FVector(Wind) * (1.f - 0.7f * Wet);
	const bool bWind = Wnd.SizeSquared() > 1.f;
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < N; ++i)
	{
		const bool bPinOff = Pinned[i] == 1 && (PinOff & (1ull << FMath::Min(63, i % NU))) != 0;
		if (Pinned[i] == 1 && !bPinOff)
		{
			X[i] = PinAt[i];
			Prev[i] = X[i];
			InvM[i] = 0.f;
			continue;
		}
		InvM[i] = 1.f;
		FVector V = (X[i] - Prev[i]) * Damp;
		const float S = (float)V.Size();
		if (S > 60.f) { V *= 60.f / S; }
		Prev[i] = X[i];
		FVector Acc(0.f, 0.f, -G);
		if (bWind)
		{
			const int32 u = i % NU, v = i / NU;
			const float Flutter = 0.55f + 0.45f * FMath::PerlinNoise1D(T * 1.7f + u * 0.37f + v * 0.21f);
			Acc += Wnd * Flutter * (0.3f + 0.7f * (float)v / FMath::Max(1, NV - 1));
		}
		X[i] += V + Acc * Dt2;
	}
	// held by the ants (the server's grabs, replicated): those particles follow their hands
	for (const FAntClothGrab& Gr : Grabs)
	{
		if (X.IsValidIndex(Gr.Idx))
		{
			X[Gr.Idx] = FMath::Lerp(X[Gr.Idx], FVector(Gr.Target), 0.5f);
			InvM[Gr.Idx] = 0.f;
			// never through the floor (or a solid top): the jaws are on it, not under it
			float T1 = NoZ, U1 = NoZ, G1 = NoZ;
			if (HfSample((float)X[Gr.Idx].X, (float)X[Gr.Idx].Y, T1, U1, G1) && T1 > NoZ * 0.5f && T1 < HfStartZ - 1.f && U1 <= NoZ * 0.5f && X[Gr.Idx].Z < T1 + Thickness)
			{
				X[Gr.Idx].Z = T1 + Thickness;
			}
			else if (T1 > NoZ * 0.5f && G1 > NoZ * 0.5f && X[Gr.Idx].Z < G1 + Thickness)
			{
				X[Gr.Idx].Z = G1 + Thickness;   // under a table: its floor
			}
		}
	}
	SolveEdges(Grabs.Num() > 0 ? 4 : 2);
	FMemory::Memzero(LayerCount.GetData(), LayerCount.Num() * sizeof(uint16));
	if (Ghost.Num() != N) { Ghost.SetNumZeroed(N); }
	for (int32 i = 0; i < N; ++i)
	{
		if (Ghost[i] > 0) { --Ghost[i]; continue; }
		if (InvM[i] > 0.f) { Collide(i); }
	}
}

void AAntCloth::PreRelax(int32 Steps)
{
	const uint8 M0 = Mode;
	for (int32 s = 0; s < Steps; ++s)
	{
		Step(SimDt);
		// extra damping while it settles
		for (int32 i = 0; i < X.Num(); ++i) { Prev[i] = FMath::Lerp(Prev[i], X[i], 0.12f); }
		if ((s == 0 || s == 3 || s == 15 || s == 60 || s == Steps - 1) && (Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth) && FParse::Param(FCommandLine::Get(), TEXT("AntClothLog")))
		{
			float Lo = 1e9f, Mz = 0.f;
			int32 LoI = 0;
			for (int32 i = 0; i < X.Num(); ++i) { Mz += (float)X[i].Z; if (X[i].Z < Lo) { Lo = (float)X[i].Z; LoI = i; } }
			UE_LOG(LogTemp, Display, TEXT("ANTCLOTH   settle step %d: mean z %.0f, lowest %d (u %d v %d) z %.0f, budget left %d"), s, Mz / FMath::Max(1, X.Num()), LoI, LoI % NU, LoI / NU, Lo, HfBudget);
		}
	}
	Mode = M0;
}

// ---- the network: the coarse lattice ---------------------------------------------------------------------------------

namespace
{
	void LatticeAxis(int32 N, TArray<int32>& Out)
	{
		Out.Reset();
		for (int32 i = 0; i < N - 1; i += AAntCloth::KStep) { Out.Add(i); }
		Out.Add(N - 1);
	}
	/** Where particle i sits in the lattice axis: segment j and its blend t. */
	void LatticeAt(const TArray<int32>& L, int32 i, int32& J, float& T)
	{
		J = 0;
		while (J + 1 < L.Num() - 1 && L[J + 1] <= i) { ++J; }
		const int32 a = L[J], b = L[FMath::Min(J + 1, L.Num() - 1)];
		T = b > a ? FMath::Clamp((float)(i - a) / (float)(b - a), 0.f, 1.f) : 0.f;
	}
}

void AAntCloth::OnRep_Key()
{
	if (!bBuilt || X.Num() == 0)
	{
		return;
	}
	TArray<int32> LU, LV;
	LatticeAxis(NU, LU);
	LatticeAxis(NV, LV);
	if (Key.P.Num() != LU.Num() * LV.Num())
	{
		return;
	}
	KeyErr.SetNum(Key.P.Num());
	float MaxE = 0.f;
	for (int32 b = 0; b < LV.Num(); ++b)
	{
		for (int32 a = 0; a < LU.Num(); ++a)
		{
			const int32 k = b * LU.Num() + a;
			KeyErr[k] = FVector(Key.P[k]) - X[Idx(LU[a], LV[b])];
			MaxE = FMath::Max(MaxE, (float)KeyErr[k].Size());
		}
	}
	KeyAge = 0.f;
	bKeyFresh = true;
	StillFor = 0.f;   // something to settle: simulate here until it rests again
	if (MaxE > 350.f) { ApplyKey(-1.f); }
}

void AAntCloth::ApplyKey(float Dt)
{
	if (KeyErr.Num() == 0)
	{
		return;
	}
	TArray<int32> LU, LV;
	LatticeAxis(NU, LU);
	LatticeAxis(NV, LV);
	if (KeyErr.Num() != LU.Num() * LV.Num())
	{
		return;
	}
	const float F = Dt < 0.f ? 1.f : FMath::Min(1.f, 5.f * Dt);
	bool bSnapped = false;
	for (int32 v = 0; v < NV; ++v)
	{
		int32 jv; float tv;
		LatticeAt(LV, v, jv, tv);
		const int32 jv1 = FMath::Min(jv + 1, LV.Num() - 1);
		for (int32 u = 0; u < NU; ++u)
		{
			int32 ju; float tu;
			LatticeAt(LU, u, ju, tu);
			const int32 ju1 = FMath::Min(ju + 1, LU.Num() - 1);
			const FVector E0 = FMath::Lerp(KeyErr[jv * LU.Num() + ju], KeyErr[jv * LU.Num() + ju1], tu);
			const FVector E1 = FMath::Lerp(KeyErr[jv1 * LU.Num() + ju], KeyErr[jv1 * LU.Num() + ju1], tu);
			FVector E = FMath::Lerp(E0, E1, tv);
			const int32 i = Idx(u, v);
			if (E.SizeSquared() > 100.f * 100.f && Ghost.IsValidIndex(i))
			{
				Ghost[i] = 8;   // far from the server's: it goes there at once, through what is in the way (a few steps without collision)
				bSnapped = true;
			}
			else
			{
				E *= F;
			}
			X[i] += E;
			Prev[i] += E;
		}
	}
	for (FVector& E : KeyErr) { E *= bSnapped ? 0.f : (1.f - F); }
}

// ---- the render ----------------------------------------------------------------------------------------------------

void AAntCloth::UpdateMesh(bool bRebuild, bool bCollision)
{
	const int32 N = X.Num();
	if (N == 0 || !Mesh)
	{
		return;
	}
	static TArray<FVector> V, Nrm;
	static TArray<FVector2D> UV;
	static TArray<int32> Tri;
	V.SetNumUninitialized(N * 2);
	Nrm.SetNumUninitialized(N * 2);
	for (int32 v = 0; v < NV; ++v)
	{
		for (int32 u = 0; u < NU; ++u)
		{
			const int32 i = Idx(u, v);
			const FVector Du = X[Idx(FMath::Min(u + 1, NU - 1), v)] - X[Idx(FMath::Max(u - 1, 0), v)];
			const FVector Dv = X[Idx(u, FMath::Min(v + 1, NV - 1))] - X[Idx(u, FMath::Max(v - 1, 0))];
			FVector Nm = FVector::CrossProduct(Du, Dv).GetSafeNormal();
			if (Nm.IsNearlyZero()) { Nm = FVector::UpVector; }
			V[i] = X[i];
			V[N + i] = X[i];
			Nrm[i] = Nm;
			Nrm[N + i] = -Nm;
		}
	}
	if (bRebuild || bTopoDirty)
	{
		UV.SetNumUninitialized(N * 2);
		for (int32 v = 0; v < NV; ++v)
		{
			for (int32 u = 0; u < NU; ++u)
			{
				const FVector2D T(u * CellUU / 300.f, v * CellUU / 300.f);
				UV[Idx(u, v)] = T;
				UV[N + Idx(u, v)] = T;
			}
		}
		Tri.Reset();
		for (int32 v = 0; v < NV - 1; ++v)
		{
			for (int32 u = 0; u < NU - 1; ++u)
			{
				if (QuadAlive[v * (NU - 1) + u] != 1) { continue; }
				const int32 a = Idx(u, v), b = Idx(u + 1, v), c = Idx(u + 1, v + 1), d = Idx(u, v + 1);
				Tri.Append({ a, b, c, a, c, d });                          // front (normal along Du x Dv)
				Tri.Append({ N + a, N + c, N + b, N + a, N + d, N + c });  // back
			}
		}
		Mesh->CreateMeshSection_LinearColor(0, V, Tri, Nrm, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bCollision);
		Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		bCollisionCooked = bCollision;
		bTopoDirty = false;
		if (Mid) { Mesh->SetMaterial(0, Mid); }
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, V, Nrm, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	}
}

void AAntCloth::OnRep_Dead()
{
	if (!bBuilt)
	{
		return;
	}
	for (uint16 q : DeadQuads)
	{
		if (QuadAlive.IsValidIndex(q) && QuadAlive[q] == 1) { QuadAlive[q] = 0; }
	}
	bTopoDirty = true;
	StillFor = 0.f;
	if (Mode != 1 && !HasAuthority()) { UpdateMesh(true, bCollisionCooked); }
}

void AAntCloth::OnRep_Mode()
{
	if (!bBuilt)
	{
		return;
	}
	if (Mode == 1)
	{
		StillFor = 0.f;
		if (Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision) { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		if (!Mesh->IsVisible()) { Mesh->SetVisibility(true); HideSource(true); }
	}
	if (Mode == 2 && !bHeapCover && HeapRadius > 0.f)
	{
		bHeapCover = true;
		LevelV3::AddCover(FVector(HeapAt), HeapRadius, 260.f, *FString::Printf(TEXT("под %s"), Kind == AntClothKind::Curtain ? TEXT("сорванной шторой") : TEXT("тканью на полу")));
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: a heap at %s r %.0f is cover now"), *KindName(), *FVector(HeapAt).ToCompactString(), HeapRadius);
	}
}

FBox AAntCloth::Bounds() const
{
	return X.Num() ? FBox(X) : FBox(GetActorLocation() - FVector(100.f), GetActorLocation() + FVector(100.f));
}

float AAntCloth::Displaced() const
{
	if (Rest.Num() != X.Num() || X.Num() == 0)
	{
		return 0.f;
	}
	double S = 0.0;
	for (int32 i = 0; i < X.Num(); ++i) { S += FVector::Dist(X[i], Rest[i]); }
	return (float)(S / X.Num());
}

int32 AAntCloth::NearestParticle(const FVector& P, float* OutDist) const
{
	int32 Best = -1;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < X.Num(); ++i)
	{
		const float D = (float)FVector::DistSquared(X[i], P);
		if (D < BestD) { BestD = D; Best = i; }
	}
	if (OutDist) { *OutDist = Best >= 0 ? FMath::Sqrt(BestD) : BestD; }
	return Best;
}

int32 AAntCloth::NearestBorder(const FVector& P, float* OutDist) const
{
	int32 Best = -1;
	float BestD = TNumericLimits<float>::Max();
	auto Try = [&](int32 u, int32 v)
	{
		const int32 i = Idx(u, v);
		const float D = (float)FVector::DistSquared(X[i], P);
		if (D < BestD) { BestD = D; Best = i; }
	};
	if (X.Num() == 0)
	{
		if (OutDist) { *OutDist = BestD; }
		return -1;
	}
	for (int32 u = 0; u < NU; ++u) { Try(u, NV - 1); if (Kind != AntClothKind::Curtain && Kind != AntClothKind::Sheet) { Try(u, 0); } }
	for (int32 v = 0; v < NV; ++v) { Try(0, v); Try(NU - 1, v); }
	if (Mode == 2 || Kind == AntClothKind::Plaid || Kind == AntClothKind::Fort)
	{
		// a heap (or the plaid, the fort): any of it
		for (int32 i = 0; i < X.Num(); i += 2) { const float D = (float)FVector::DistSquared(X[i], P); if (D < BestD) { BestD = D; Best = i; } }
	}
	if (OutDist) { *OutDist = Best >= 0 ? FMath::Sqrt(BestD) : BestD; }
	return Best;
}

float AAntCloth::DistanceTo(const FVector& P) const
{
	float D = TNumericLimits<float>::Max();
	if (!Bounds().ExpandBy(InteractRadius + 50.f).IsInside(P))
	{
		return D;
	}
	NearestBorder(P, &D);
	return D;
}

bool AAntCloth::CanInteract(const AAntCharacter* Ant) const
{
	return bBuilt && X.Num() > 0 && Ant && Ant->Creature == 0;
}

float AAntCloth::NeedStrength() const
{
	if (Mode == 2) { return 14.f; }
	switch (Kind)
	{
	case AntClothKind::Tablecloth: return 40.f;
	case AntClothKind::LongCloth: return 48.f;
	case AntClothKind::Curtain: return 28.f;
	case AntClothKind::Plaid: return 24.f;
	default: return 14.f;
	}
}

FString AAntCloth::GetPrompt(const AAntCharacter* Ant) const
{
	const TCHAR* Verb = Kind == AntClothKind::Curtain ? TEXT("Тянуть штору (сорвать с колец)") : Mode == 2 ? TEXT("Тащить ткань") : TEXT("Тянуть за край");
	return FString::Printf(TEXT("%s: %s — держать [E] вместе (сила %.0f) · [ПКМ] прогрызть"), Verb, *KindName(), NeedStrength());
}

FString AAntCloth::Describe() const
{
	return FString::Printf(TEXT("%s %s mode %d %dx%d, dead %d, pins off %d, grabs %d, wet %.2f, wind %.0f, displaced %.0f uu, %.3f ms (avg %.3f)"), *KindName(), *GetName(), (int32)Mode, NU, NV,
		DeadQuads.Num(), (int32)(int32)FMath::CountBits64(PinOff), Grabs.Num(), Wet, FVector(Wind).Size(), Displaced(), LastMs, AvgMs);
}

// ---- the frame -----------------------------------------------------------------------------------------------------

void AAntCloth::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T0 = FPlatformTime::Seconds();
	if (!bBuilt)
	{
		FindTries += DeltaSeconds;
		if (FindTries > 0.5f)
		{
			FindTries = 0.f;
			if (FindSource()) { BuildFromSource(); }
		}
		return;
	}
	// the piece of the level stays hidden (a fixture put back by the Human shows its body again)
	ServerClock += DeltaSeconds;
	if (Source.IsValid() && Source->IsVisible() && Mesh->IsVisible()) { HideSource(true); }
	if (SourceFix && SourceFix->Tidied == 2 && SourceFix->State == AKitchenFixture::Fixed && Displaced() > 60.f && HasAuthority())
	{
		ResetCloth();   // the Human hung it back up
	}
	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	const bool bRun = Mode == 1 || StillFor < 1.2f || Grabs.Num() > 0 || FVector(Wind).SizeSquared() > 1.f || KeyAge < 0.6f;
	if (bRun)
	{
		HfBudget = Mode == 1 ? 160 : 60;
		Accum = FMath::Min(Accum + DeltaSeconds, SimDt * 2.f);
		float Motion = 0.f;
		while (Accum >= SimDt)
		{
			Accum -= SimDt;
			Step(SimDt);
		}
		if ((Kind == AntClothKind::Tablecloth || Kind == AntClothKind::LongCloth) && AwakeFor < 3.f && FMath::Fmod(AwakeFor, 0.25f) < DeltaSeconds && Source.IsValid() && FParse::Param(FCommandLine::Get(), TEXT("AntClothLog")))
		{
			const FBox SB = Source->Bounds.GetBox().ExpandBy(FVector(-200.f, -200.f, 0.f));
			int32 Sunk = 0, In = 0;
			float Mz = 0.f;
			int32 SunkI = -1;
			for (int32 i = 0; i < X.Num(); ++i)
			{
				Mz += (float)X[i].Z;
				if (X[i].X > SB.Min.X && X[i].X < SB.Max.X && X[i].Y > SB.Min.Y && X[i].Y < SB.Max.Y) { ++In; if (X[i].Z < SourceTop - 30.f) { ++Sunk; SunkI = i; } }
			}
			float T1 = NoZ, U1 = NoZ, G1 = NoZ;
			bool bK = SunkI >= 0 && HfSample((float)X[SunkI].X, (float)X[SunkI].Y, T1, U1, G1);
			UE_LOG(LogTemp, Display, TEXT("ANTCLOTH   t %.2f %s: mean z %.0f, over the top %d, sunk %d (e.g. %d at %s prev %s: known %d top %.0f under %.0f ground %.0f), budget left %d, mode %d"),
				AwakeFor, HasAuthority() ? TEXT("server") : TEXT("client"), Mz / FMath::Max(1, X.Num()), In, Sunk, SunkI, SunkI >= 0 ? *X[SunkI].ToCompactString() : TEXT("-"), SunkI >= 0 ? *Prev[SunkI].ToCompactString() : TEXT("-"), bK ? 1 : 0, T1, U1, G1, HfBudget, (int32)Mode);
		}
		int32 Cnt = 0;
		for (int32 i = 0; i < X.Num(); i += 3) { Motion += (float)FVector::DistSquared(X[i], Prev[i]); ++Cnt; }
		Motion /= FMath::Max(1, Cnt);
		if (AwakeFor > 8.f && Motion < 0.6f) { Motion = 0.f; }   // a heap that only shivers is at rest
		if (Mode == 0 && AwakeFor > 3.f && Grabs.Num() == 0) { Motion = 0.f; }   // never disturbed: the drape has settled
		if (!HasAuthority())
		{
			KeyAge += DeltaSeconds;
			if (KeyAge < 0.6f) { ApplyKey(DeltaSeconds); }
		}
		StillFor = Motion < 0.02f && Grabs.Num() == 0 && FVector(Wind).SizeSquared() < 1.f ? StillFor + DeltaSeconds : 0.f;
		AwakeFor += DeltaSeconds;
		TickCups(DeltaSeconds);
		if (!Mesh->IsVisible() && (Mode != 0 || Displaced() > 3.f)) { Mesh->SetVisibility(true); HideSource(true); }
		if (Mesh->IsVisible() && (Mesh->WasRecentlyRendered(0.25f) || AwakeFor < 0.3f || bTopoDirty))
		{
			UpdateMesh(bTopoDirty, false);
			bRestMesh = false;
		}
	}
	const bool bResting = Mode != 1 && StillFor >= 1.2f && Grabs.Num() == 0 && FVector(Wind).SizeSquared() < 1.f && KeyAge >= 0.6f;
	if (bResting && !bRestMesh && Mesh->IsVisible())
	{
		// at rest: one more mesh with a collision copy the ants climb (curtains and sheets keep their stand-in boxes)
		const bool bClimb = Kind != AntClothKind::Curtain && Kind != AntClothKind::Sheet;
		UpdateMesh(true, bClimb);
		bRestMesh = true;
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s settles on %s (collision %d, displaced %.0f)"), *KindName(), HasAuthority() ? TEXT("server") : TEXT("client"), bClimb ? 1 : 0, Displaced());
	}
	else if (!bRun && bTopoDirty && Mesh->IsVisible())
	{
		UpdateMesh(true, bCollisionCooked);
	}
	// the wet look: darker
	if (Mid && bHaveTint && !FMath::IsNearlyEqual(WetSeen, Wet, 0.01f))
	{
		WetSeen = Wet;
		const float K = 1.f - 0.45f * Wet;
		Mid->SetVectorParameterValue(FName(TEXT("Tint")), FLinearColor(BaseTint.R * K, BaseTint.G * K, BaseTint.B * K, BaseTint.A));
		Mid->SetScalarParameterValue(FName(TEXT("Roughness")), FMath::Lerp(0.9f, 0.45f, Wet));
	}
	// a builder curtain torn down: its climbing box goes
	if (SourceRow >= 0 && (Kind == AntClothKind::Curtain) && (int32)FMath::CountBits64(PinOff) * 2 > NU)
	{
		for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It)
		{
			if (UStaticMeshComponent* Px = It->CurtainProxy(SourceRow))
			{
				if (Px->GetCollisionEnabled() != ECollisionEnabled::NoCollision) { Px->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
			}
		}
	}
	LastMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	AvgMs = AvgMs * 0.95 + LastMs * 0.05;
}

void AAntCloth::ResetCloth()
{
	if (!bBuilt)
	{
		return;
	}
	X = Rest;
	Prev = Rest;
	PinOff = 0;
	DeadQuads.Reset();
	for (uint8& Q : QuadAlive) { if (Q == 0) { Q = 1; } }
	Grabs.Reset();
	Pullers.Reset();
	YankLeft = 0.f;
	Wet = 0.f;
	Mode = 0;
	bHeapCover = false;
	HeapRadius = 0.f;
	bTopoDirty = true;
	StillFor = 0.f;
	if (HasAuthority())
	{
		Key.P.Reset();
		TArray<int32> LU, LV;
		LatticeAxis(NU, LU);
		LatticeAxis(NV, LV);
		for (int32 b : LV) { for (int32 a : LU) { Key.P.Add(X[Idx(a, b)]); } }
		++Key.Seq;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s reset to its rest"), *KindName());
}

void AAntCloth::Wake(const TCHAR* Why)
{
	if (!HasAuthority() || !bBuilt)
	{
		return;
	}
	if (Mode != 1)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s wakes: %s"), *KindName(), Why);
	}
	Mode = 1;
	StillFor = 0.f;
	AwakeFor = 0.f;
	OnRep_Mode();
}

void AAntCloth::DropAll(const TCHAR* Why)
{
	if (!HasAuthority() || !bPins)
	{
		return;
	}
	PinOff = ~0ull;
	Wake(Why);
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: every pin lets go (%s)"), *KindName(), Why);
}

void AAntCloth::Yank(const FVector& Dir, float Strength, float Seconds, AAntCharacter* By)
{
	if (!HasAuthority() || !bBuilt)
	{
		return;
	}
	YankDir = Dir.GetSafeNormal2D();
	if (YankDir.IsNearlyZero()) { YankDir = FVector(1.f, 0.f, 0.f); }
	YankStrength = Strength;
	YankLeft = Seconds;
	YankBy = By;
	LastBy = By;
	// the hem on that side: the lowest particles of the edge facing Dir
	YankP.Reset();
	const FVector C = Bounds().GetCenter();
	float MaxD = -1e9f;
	for (int32 i = 0; i < X.Num(); ++i) { MaxD = FMath::Max(MaxD, (float)FVector::DotProduct(X[i] - C, YankDir)); }
	for (int32 i = 0; i < X.Num(); ++i)
	{
		if ((float)FVector::DotProduct(X[i] - C, YankDir) > MaxD - CellUU * 0.6f) { YankP.Add(i); }
	}
	Wake(TEXT("yank"));
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s yanked towards %s, strength %.0f for %.1f s (%d hem particles)"), *KindName(), *YankDir.ToCompactString(), Strength, Seconds, YankP.Num());
}

bool AAntCloth::CutAt(const FVector& P, float Reach)
{
	if (!HasAuthority() || !bBuilt)
	{
		return false;
	}
	int32 Best = -1;
	float BestD = Reach * Reach;
	for (int32 v = 0; v < NV - 1; ++v)
	{
		for (int32 u = 0; u < NU - 1; ++u)
		{
			const int32 q = v * (NU - 1) + u;
			if (QuadAlive[q] != 1) { continue; }
			const FVector M = (X[Idx(u, v)] + X[Idx(u + 1, v)] + X[Idx(u, v + 1)] + X[Idx(u + 1, v + 1)]) * 0.25f;
			const float D = (float)FVector::DistSquared(M, P);
			if (D < BestD) { BestD = D; Best = q; }
		}
	}
	if (Best < 0)
	{
		return false;
	}
	QuadAlive[Best] = 0;
	DeadQuads.Add((uint16)Best);
	bTopoDirty = true;
	Wake(TEXT("cut"));
	return true;
}

bool AAntCloth::BiteAt(const FVector& P, AAntCharacter* By)
{
	float D = 0.f;
	const int32 Near = NearestParticle(P, &D);
	if (Near < 0 || D > CellUU * 0.75f + 45.f)
	{
		return false;
	}
	if (!CutAt(P, CellUU * 1.2f + 45.f))
	{
		return false;
	}
	LastBy = By;
	AntAudio::Everyone(this, EAntSfx::RipOff, P, 0.6f);
	if (By) { By->Toast(FString::Printf(TEXT("Прогрыз %s! Ряд укусов — и её порвёт, если тянуть"), *KindName())); }
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s bitten by %s at %s: %d cells cut"), *KindName(), By ? *By->GetDisplayName() : TEXT("-"), *P.ToCompactString(), DeadQuads.Num());
	return true;
}

// ---- the server's gameplay -------------------------------------------------------------------------------------------

void AAntCloth::ServerTick(float Dt)
{
	ServerPull(Dt);
	if (Mode == 1 || Grabs.Num() > 0)
	{
		ServerRiders(Dt);
	}
	ServerRings();
	TearClock += Dt;
	if (TearClock >= 0.1f)
	{
		TearClock = 0.f;
		if (Mode == 1 && DeadQuads.Num() > 0 && (Grabs.Num() > 0 || YankLeft > 0.f)) { ServerTear(); }
		ServerTrip();
	}
	ServerWetWind(Dt);
	// the furniture under it moves or breaks (AntWreck): what it lies on has to be traced again
	HfIgnoreClock += Dt;
	if (HfIgnoreClock >= 0.25f)
	{
		HfIgnoreClock = 0.f;
		const FBox Me = Bounds().ExpandBy(300.f);
		for (TActorIterator<AAntWreckBody> It(GetWorld()); It; ++It)
		{
			if ((It->bAwake || It->IsBroken()) && Me.Intersect(It->GetComponentsBoundingBox(true)))
			{
				HfInvalidate(It->GetComponentsBoundingBox(true).ExpandBy(150.f));
				if (It->bAwake || Mode == 0) { Wake(It->IsBroken() ? TEXT("the table under it broke") : TEXT("the furniture under it moves")); }
			}
		}
	}
	// the lattice for the clients while it moves
	KeyClock += Dt;
	if (Mode == 1 && KeyClock >= 0.2f)
	{
		KeyClock = 0.f;
		TArray<int32> LU, LV;
		LatticeAxis(NU, LU);
		LatticeAxis(NV, LV);
		Key.P.SetNum(LU.Num() * LV.Num());
		for (int32 b = 0; b < LV.Num(); ++b) { for (int32 a = 0; a < LU.Num(); ++a) { Key.P[b * LU.Num() + a] = X[Idx(LU[a], LV[b])]; } }
		++Key.Seq;
	}
	// asleep again
	if (Mode == 1 && StillFor >= 1.0f && AwakeFor > 0.8f && YankLeft <= 0.f && Pullers.Num() == 0)
	{
		CheckHeap();
		KeyClock = 1.f;   // one last lattice (late joiners get the final shape)
		TArray<int32> LU, LV;
		LatticeAxis(NU, LU);
		LatticeAxis(NV, LV);
		Key.P.SetNum(LU.Num() * LV.Num());
		for (int32 b = 0; b < LV.Num(); ++b) { for (int32 a = 0; a < LU.Num(); ++a) { Key.P[b * LU.Num() + a] = X[Idx(LU[a], LV[b])]; } }
		++Key.Seq;
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s rests (mode %d, displaced %.0f uu, %d cells cut)"), *KindName(), (int32)Mode, Displaced(), DeadQuads.Num());
	}
	if (CostLog() && Mode == 1 && FMath::Fmod(ServerClock, 2.f) < Dt)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH cost %s"), *Describe());
	}
}

void AAntCloth::ServerPull(float Dt)
{
	// the dev crew keeps holding on
	if (Crew.Num())
	{
		const bool bOn = GetWorld()->GetTimeSeconds() < CrewUntil;
		for (const TWeakObjectPtr<AAntCharacter>& C : Crew)
		{
			if (AAntCharacter* A = C.Get()) { if (!A->bDead) { A->Pulling = bOn ? this : (A->Pulling == this ? nullptr : A->Pulling); } }
		}
		if (!bOn) { Crew.Reset(); }
	}
	int32 Hands = 0;
	AAntCharacter* First = nullptr;
	float S = PullStrength(&Hands, &First);
	const float Need = NeedStrength();
	PullMissing = (Hands > 0 && S < Need) ? (uint8)AntDoor::Missing(Need, S) : 0;
	// who holds on (and which particle each holds)
	for (int32 k = Pullers.Num() - 1; k >= 0; --k)
	{
		AAntCharacter* A = Pullers[k].Ant.Get();
		if (!A || A->bDead || A->Pulling != this) { Pullers.RemoveAt(k); }
	}
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		if (It->Pulling != this || It->bDead) { continue; }
		bool bHave = false;
		for (const FPuller& P : Pullers) { if (P.Ant.Get() == *It) { bHave = true; } }
		if (bHave) { continue; }
		FPuller P;
		P.Ant = *It;
		P.P = NearestBorder(It->GetActorLocation());
		const FVector C = Bounds().GetCenter();
		P.Dir = (It->GetActorLocation() - C).GetSafeNormal2D();
		Pullers.Add(P);
		Wake(TEXT("pulled"));
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: %s grabs particle %d (strength now %.0f of %.0f)"), *KindName(), *It->GetDisplayName(), P.P, S, Need);
	}
	// the virtual crew of a yank
	if (YankLeft > 0.f)
	{
		YankLeft -= Dt;
		S = FMath::Max(S, YankStrength);
	}
	Grabs.Reset();
	const bool bStrong = S >= Need;
	const float Speed = bStrong ? 130.f * FMath::Clamp(S / Need, 1.f, 2.6f) : 0.f;
	auto Inward = [this](int32 i, const FVector& Dir) -> int32
	{
		// hand over hand: the next particle along the grid away from the pull
		const int32 u = i % NU, v = i / NU;
		int32 Best = -1;
		float BestDot = 0.2f;
		const int32 Cand[4][2] = { { u + 1, v }, { u - 1, v }, { u, v + 1 }, { u, v - 1 } };
		for (const auto& C : Cand)
		{
			if (C[0] < 0 || C[1] < 0 || C[0] >= NU || C[1] >= NV) { continue; }
			const float D = (float)FVector::DotProduct((X[Idx(C[0], C[1])] - X[i]).GetSafeNormal(), -Dir);
			if (D > BestDot) { BestDot = D; Best = Idx(C[0], C[1]); }
		}
		return Best;
	};
	for (FPuller& P : Pullers)
	{
		AAntCharacter* A = P.Ant.Get();
		if (!A || !X.IsValidIndex(P.P)) { continue; }
		const FVector Jaw = A->GetActorLocation();
		if (FVector::DistSquared(X[P.P], Jaw) > 450.f * 450.f && !Crew.Contains(P.Ant))
		{
			// it lost its hold (or grabbed before it got there): the hem nearest to its jaws again
			P.P = NearestBorder(Jaw);
			if (!X.IsValidIndex(P.P)) { continue; }
			P.Dir = (Jaw - Bounds().GetCenter()).GetSafeNormal2D();
		}
		if (bStrong)
		{
			FVector T = X[P.P] + P.Dir * Speed * Dt;
			if (!Crew.Contains(P.Ant)) { T.Z = FMath::Lerp((float)T.Z, (float)Jaw.Z, 0.15f); }   // (a dev crew is not really at the hem)
			if (FVector::DotProduct(X[P.P] - Jaw, P.Dir) > 30.f)
			{
				const int32 Nx = Inward(P.P, P.Dir);
				if (Nx >= 0) { P.P = Nx; }
			}
			Grabs.Add({ (uint16)P.P, FVector_NetQuantize10(T) });
			LastBy = A;
		}
		else
		{
			// not enough of us: the edge strains in the jaws
			const FVector T = X[P.P] + (Jaw - X[P.P]).GetClampedToMaxSize(4.f);
			Grabs.Add({ (uint16)P.P, FVector_NetQuantize10(T) });
		}
	}
	if (YankLeft > 0.f && bStrong)
	{
		for (int32& i : YankP)
		{
			if (!X.IsValidIndex(i)) { continue; }
			FVector T = X[i] + YankDir * Speed * Dt;
			if (FVector::DotProduct(X[i] - Rest[i], YankDir) > CellUU * 1.6f)
			{
				const int32 Nx = Inward(i, YankDir);
				if (Nx >= 0) { i = Nx; }
			}
			Grabs.Add({ (uint16)i, FVector_NetQuantize10(T) });
		}
	}
	if (Grabs.Num() > 0 && bStrong) { Wake(TEXT("pulled")); }
	// curtains: pulled hard, the rings give one by one
	if (bPins && Pullers.Num() > 0 && S >= Need * 1.5f && PinOff != ~0ull)
	{
		RipClock += Dt;
		if (RipClock >= 1.1f)
		{
			RipClock = 0.f;
			const FVector Pull = Pullers[0].Ant.IsValid() ? Pullers[0].Ant->GetActorLocation() : Bounds().GetCenter();
			int32 Best = -1;
			float BestD = TNumericLimits<float>::Max();
			for (int32 u = 0; u < NU; ++u)
			{
				if (PinOff & (1ull << FMath::Min(u, 63))) { continue; }
				const float D = (float)FVector::DistSquared2D(PinAt[Idx(u, 0)], Pull);
				if (D < BestD) { BestD = D; Best = u; }
			}
			if (Best >= 0)
			{
				PinOff |= 1ull << FMath::Min(Best, 63);
				AntAudio::Everyone(this, EAntSfx::RipOff, PinAt[Idx(Best, 0)], 0.8f);
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: a ring rips off (column %d, %d of %d off)"), *KindName(), Best, (int32)(int32)FMath::CountBits64(PinOff), NU);
				if ((int32)FMath::CountBits64(PinOff) * 2 > NU && SourceFix && SourceFix->State == AKitchenFixture::Fixed)
				{
					SourceFix->Fall(FVector(0.f, 1.f, 0.f), LastBy.Get(), TEXT("torn down by the cloth"));
				}
				if ((int32)FMath::CountBits64(PinOff) * 2 > NU) { PinOff = ~0ull; }
			}
		}
	}
	else
	{
		RipClock = 0.f;
	}
}

void AAntCloth::ServerRings()
{
	if (!bPins || PinOff == ~0ull)
	{
		return;
	}
	// the kitchen curtain hangs on the fixture's six ring fasteners: an undone ring lets the cloth near it go
	if (SourceFix)
	{
		if (SourceFix->State != AKitchenFixture::Fixed && SourceFix->State != AKitchenFixture::Hanging)
		{
			DropAll(TEXT("the curtain fixture came down"));
			return;
		}
		if (!bRingsBound)
		{
			// each pin column hangs on the nearest ring (bound while they are all still on)
			bRingsBound = true;
			RingOf.SetNum(NU);
			for (int32 u = 0; u < NU; ++u)
			{
				float BestD = 220.f;
				for (TActorIterator<AKitchenFastener> It(GetWorld()); It; ++It)
				{
					if (It->bDone) { continue; }
					const FVector R = It->GetActorLocation();
					const float D = (float)FVector::Dist2D(PinAt[Idx(u, 0)], R);
					if (D < BestD && FMath::Abs((float)(PinAt[Idx(u, 0)].Z - R.Z)) < 500.f) { BestD = D; RingOf[u] = *It; }
				}
			}
		}
		for (int32 u = 0; u < RingOf.Num(); ++u)
		{
			const AKitchenFastener* F = RingOf[u].Get();
			if (F && F->bDone && (PinOff & (1ull << FMath::Min(u, 63))) == 0)
			{
				PinOff |= 1ull << FMath::Min(u, 63);
				Wake(TEXT("a ring is off"));
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: ring fastener %d is off -> column %d sags"), *KindName(), F->Def, u);
			}
		}
	}
}

void AAntCloth::ServerTear()
{
	// A cut that is pulled tears on: a live cell next to a dead one, stretched hard, goes too (two per tick at most).
	int32 Torn = 0;
	const int32 QW = NU - 1;
	for (int32 k = 0; k < DeadQuads.Num() && Torn < 2; ++k)
	{
		const int32 q = DeadQuads[k];
		const int32 u = q % QW, v = q / QW;
		const int32 Nb[4][2] = { { u + 1, v }, { u - 1, v }, { u, v + 1 }, { u, v - 1 } };
		for (const auto& Nn : Nb)
		{
			if (Nn[0] < 0 || Nn[1] < 0 || Nn[0] >= QW || Nn[1] >= NV - 1) { continue; }
			const int32 n = Nn[1] * QW + Nn[0];
			if (QuadAlive[n] != 1) { continue; }
			const int32 a = Idx(Nn[0], Nn[1]), b = Idx(Nn[0] + 1, Nn[1]), c = Idx(Nn[0], Nn[1] + 1), d = Idx(Nn[0] + 1, Nn[1] + 1);
			const float SU = (float)FMath::Max(FVector::Dist(X[a], X[b]), FVector::Dist(X[c], X[d]));
			const float SV = (float)FMath::Max(FVector::Dist(X[a], X[c]), FVector::Dist(X[b], X[d]));
			const float RU = (float)FVector::Dist(FlatPos[a], FlatPos[b]) + 1.f, RV = (float)FVector::Dist(FlatPos[a], FlatPos[c]) + 1.f;
			if (SU / RU > 1.4f || SV / RV > 1.4f)
			{
				QuadAlive[n] = 0;
				DeadQuads.Add((uint16)n);
				bTopoDirty = true;
				++Torn;
				if (Torn == 1) { AntAudio::Everyone(this, EAntSfx::RipOff, X[a], 1.f); }
				break;
			}
		}
	}
	if (Torn > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s tears on: %d cells cut"), *KindName(), DeadQuads.Num());
	}
}

void AAntCloth::ServerTrip()
{
	// The Human trips over cloth on the floor (pulled under his feet: every time).
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextTrip || (Mode == 0 && Displaced() < 80.f))
	{
		return;
	}
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* H = *It;
		if (FParse::Param(FCommandLine::Get(), TEXT("AntClothLog")) && H->Fig == HumanFig::Dad && Now >= TripLogAt)
		{
			TripLogAt = Now + 1.f;
			int32 Near = 0;
			const FVector F0(H->GetActorLocation().X, H->GetActorLocation().Y, H->FeetZ());
			for (int32 i = 0; i < X.Num(); i += 2) { if (FVector::Dist2D(X[i], F0) < 260.f && X[i].Z < F0.Z + 70.f && X[i].Z > F0.Z - 60.f) { ++Near; } }
			UE_LOG(LogTemp, Display, TEXT("ANTCLOTH trip check: Dad at %s speed %.0f down %d, %d particles at his feet"), *F0.ToCompactString(), H->GetVelocity().Size2D(), H->IsDown() ? 1 : 0, Near);
		}
		if (H->Fig != HumanFig::Dad || H->IsDown() || H->IsAway() || H->IsKnockedOut() || H->GetVelocity().Size2D() < 260.f)
		{
			continue;
		}
		const FVector Feet(H->GetActorLocation().X, H->GetActorLocation().Y, H->FeetZ());
		int32 Under = 0;
		for (int32 i = 0; i < X.Num(); ++i)
		{
			if (FVector::Dist2D(X[i], Feet) < 280.f && X[i].Z < Feet.Z + 70.f && X[i].Z > Feet.Z - 60.f) { ++Under; }
		}
		if (Under < 3)
		{
			continue;
		}
		const float Chance = Pullers.Num() > 0 || YankLeft > 0.f ? 1.f : 0.12f;
		if (FMath::FRand() < Chance && H->Topple(H->GetVelocity().GetSafeNormal2D(), TEXT("запнулся о ткань")))
		{
			NextTrip = Now + 18.f;
			UE_LOG(LogTemp, Display, TEXT("ANTCLOTH the Human trips over the %s (%d particles under his feet, pulled %d)"), *KindName(), Under, Pullers.Num() > 0 ? 1 : 0);
			if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->Broadcast(FString::Printf(TEXT("Человек запнулся о %s!"), Kind == AntClothKind::Curtain ? TEXT("штору") : TEXT("ткань")), FLinearColor(1.f, 0.85f, 0.3f)); }
		}
		else
		{
			NextTrip = Now + 1.5f;
		}
	}
}

void AAntCloth::ServerWetWind(float Dt)
{
	PullClockC += Dt;
	if (PullClockC < 0.25f)
	{
		return;
	}
	PullClockC = 0.f;
	// water (the real liquid): the cloth in it soaks up and gets heavy; it dries slowly
	if (UAntFluidWorld* F = UAntFluidWorld::Active(this))
	{
		int32 InWater = 0;
		for (int32 i = 0; i < X.Num(); i += 5) { if (F->DepthAt(X[i]) > 1.5f) { ++InWater; } }
		const float Before = Wet;
		Wet = FMath::Clamp(Wet + (InWater > 0 ? 0.08f * FMath::Min(InWater, 6) : -0.004f), 0.f, 1.f);
		if (Before < 0.3f && Wet >= 0.3f) { Wake(TEXT("wet")); UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s gets wet"), *KindName()); }
	}
	else if (Wet > 0.f)
	{
		Wet = FMath::Max(0.f, Wet - 0.004f);
	}
	// the draught: the open kitchen window (the Dad airing the kitchen), the yard's breeze, or the admin's wind
	FVector Want = FVector::ZeroVector;
	const int32 WM = AntCloth::WindMode();
	const FVector C = Bounds().GetCenter();
	if (Kind == AntClothKind::Curtain || Kind == AntClothKind::Sheet)
	{
		bool bOpen = WM == 1;
		if (WM == -1)
		{
			const int32 Cur = NpcRoutine::Current(GetWorld(), NpcRoutine::Dad);
			const TConstArrayView<NpcRoutine::FStep> Steps = NpcRoutine::Steps();
			bOpen = Steps.IsValidIndex(Cur) && FCString::Strcmp(Steps[Cur].Key, TEXT("h_window")) == 0 && C.Y < -4000.f && FMath::Abs((float)C.X) < 4000.f;
			bOpen |= Kind == AntClothKind::Sheet;   // the yard always has a breeze
		}
		if (bOpen && WM != 0)
		{
			const float T = GetWorld()->GetTimeSeconds();
			const float Gust = 0.7f + 0.3f * FMath::Sin(T * 0.9f + C.X * 0.001f) * FMath::Sin(T * 2.3f);
			const FVector Into = WallN.IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : WallN;
			Want = (Into * 8500.f + FVector(1.f, 0.f, 0.f) * 1500.f * FMath::Sin(T * 0.4f)) * Gust * (Kind == AntClothKind::Sheet ? 0.6f : 1.f);
		}
	}
	const bool bWas = FVector(Wind).SizeSquared() > 1.f;
	Wind = Want;
	if (!bWas && Want.SizeSquared() > 1.f) { Wake(TEXT("wind")); }
}

void AAntCloth::CheckHeap()
{
	// Mostly on the floor: a heap (cover); otherwise at rest where it is.
	int32 Low = 0;
	FVector Sum = FVector::ZeroVector;
	float Ground0 = NoZ;
	for (int32 i = 0; i < X.Num(); ++i)
	{
		float T, U, G;
		if (!HfSample((float)X[i].X, (float)X[i].Y, T, U, G)) { continue; }
		const float Floor = G > NoZ * 0.5f ? FMath::Min(T, G) : T;
		if (X[i].Z < Floor + 160.f || (X[i].Z < T + 160.f && T < (float)Rest[0].Z - 400.f)) { ++Low; Sum += X[i]; Ground0 = FMath::Max(Ground0, Floor); }
	}
	const bool bHeap = Low * 2 > X.Num() && Displaced() > 150.f;
	Mode = bHeap ? 2 : 0;
	if (bHeap)
	{
		const FVector M = Sum / FMath::Max(1, Low);
		float R2 = 0.f;
		for (int32 i = 0; i < X.Num(); ++i) { R2 += (float)FVector::DistSquared2D(X[i], M); }
		HeapAt = M;
		HeapRadius = FMath::Clamp(FMath::Sqrt(R2 / X.Num()) * 1.2f, 300.f, 1100.f);
	}
	OnRep_Mode();
}

void AAntCloth::FindRiders()
{
	Riders.Reset();
	if (Kind != AntClothKind::Tablecloth && Kind != AntClothKind::LongCloth)
	{
		return;
	}
	const float TopZ = SourceTop;
	const FBox B = Bounds();
	auto OnTop = [&](const FVector& P, float Up) { return P.X > B.Min.X && P.X < B.Max.X && P.Y > B.Min.Y && P.Y < B.Max.Y && P.Z > TopZ - 60.f && P.Z < TopZ + Up; };
	for (TActorIterator<AKitchenFixture> It(GetWorld()); It; ++It)
	{
		if (*It == SourceFix || It->State != AKitchenFixture::Fixed || It->FixId() == TEXT("tablecloth")) { continue; }
		if (OnTop(It->GetActorLocation(), 500.f)) { FRider R; R.Actor = *It; R.P = NearestParticle(It->GetActorLocation()); Riders.Add(R); }
	}
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (OnTop(It->GetActorLocation(), 500.f)) { FRider R; R.Actor = *It; R.P = NearestParticle(It->GetActorLocation()); Riders.Add(R); }
	}
	if (AntWreck::Enabled())
	{
		for (const AntWreck::FPiece& P : AntWreck::Pieces(GetWorld()))
		{
			if (P.bFixed || P.Mass > 8.f) { continue; }
			const FVector Cn = P.Box.GetCenter();
			if (OnTop(FVector(Cn.X, Cn.Y, P.Box.Min.Z), 90.f)) { FRider R; R.Piece = P.Inst; R.P = NearestParticle(FVector(Cn.X, Cn.Y, P.Box.Min.Z)); Riders.Add(R); }
		}
	}
	HfRefreshIgnores();
	for (const FRider& R : Riders)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s rider: %s"), *KindName(), R.Piece >= 0 ? *FString::Printf(TEXT("piece %d"), R.Piece) : R.Actor.IsValid() ? *R.Actor->GetName() : TEXT("-"));
	}
}

void AAntCloth::ServerRiders(float Dt)
{
	UWorld* W = GetWorld();
	for (FRider& R : Riders)
	{
		if (R.bOff || !X.IsValidIndex(R.P)) { continue; }
		const FVector Q = X[R.P];
		const FVector Moved = Q - Rest[R.P];
		const FVector Vel = (X[R.P] - Prev[R.P]) / SimDt;
		const bool bOverEdge = Q.Z < Rest[R.P].Z - 90.f;
		const FVector Dir = Moved.GetSafeNormal2D().IsNearlyZero() ? YankDir : Moved.GetSafeNormal2D();
		if (R.Piece >= 0 && !R.bWoken)
		{
			if (Moved.Size2D() > 25.f || bOverEdge)
			{
				const AntWreck::FPiece* P = AntWreck::PieceOf(W, R.Piece);
				AAntWreckBody* Bd = P ? AntWreck::HitPiece(W, R.Piece, P->Box.GetCenter(), Dir * FMath::Max(1.f, P->Mass * 2.5f), 0.f, this) : nullptr;
				R.Actor = Bd;
				R.bWoken = true;
				if (!Bd) { R.bOff = true; }
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: piece %d on it wakes (%s)"), *KindName(), R.Piece, Bd ? TEXT("a body now") : TEXT("no body"));
			}
			continue;
		}
		AActor* A = R.Actor.Get();
		if (!A) { R.bOff = true; continue; }
		if (AKitchenFixture* Fx = Cast<AKitchenFixture>(A))
		{
			if (Fx->State != AKitchenFixture::Fixed) { R.bOff = true; continue; }
			if (Moved.Size2D() > 110.f || bOverEdge)
			{
				Fx->PushDir = Dir;
				Fx->Fall(Dir, LastBy.Get(), TEXT("the tablecloth went"));
				R.bOff = true;
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s takes the fixture %s over the edge"), *KindName(), *Fx->FixId());
			}
			continue;
		}
		// a body (food, a woken plate) rides on the cloth's friction until it is off the table
		UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(A->GetRootComponent());
		const float BodyZ = (float)A->GetActorLocation().Z;
		if (BodyZ < (float)Rest[R.P].Z - 120.f || bOverEdge)
		{
			R.bOff = true;
			continue;
		}
		const FVector Carry(Vel.X, Vel.Y, 0.f);
		if (Carry.SizeSquared() < 25.f) { continue; }
		if (Prim && Prim->IsSimulatingPhysics())
		{
			const FVector Cur = Prim->GetPhysicsLinearVelocity();
			Prim->SetPhysicsLinearVelocity(FVector(Carry.X, Carry.Y, Cur.Z));
		}
		else
		{
			A->SetActorLocation(A->GetActorLocation() + Carry * Dt, false, nullptr, ETeleportType::TeleportPhysics);
		}
		// it keeps to the particle under it
		R.P = NearestParticle(A->GetActorLocation() - FVector(0.f, 0.f, 30.f));
	}
}

void AAntCloth::TickCups(float Dt)
{
	for (int32 c = 0; c < Cups.Num(); ++c)
	{
		UStaticMeshComponent* Cp = Cups[c];
		if (!Cp || CupDone[c] || !X.IsValidIndex(CupIdx[c])) { continue; }
		const int32 i = CupIdx[c];
		if (CupFall[c] < 0.f)
		{
			Cp->SetWorldLocation(X[i] + CupOff[c]);
			if (X[i].Z < Rest[i].Z - 90.f)
			{
				CupFall[c] = 0.f;
				CupVel[c] = (X[i] - Prev[i]) / SimDt + FVector(0.f, 0.f, 150.f);
			}
			continue;
		}
		CupFall[c] += Dt;
		CupVel[c].Z -= ClothGravity * Dt;
		FVector P = Cp->GetComponentLocation() + CupVel[c] * Dt;
		Cp->SetWorldLocationAndRotation(P, Cp->GetComponentRotation() + FRotator(400.f * Dt, 0.f, 250.f * Dt));
		float T, U, G;
		const bool bHf = HfSample((float)P.X, (float)P.Y, T, U, G);
		const float Floor = bHf ? (P.Z > T ? T : (G > NoZ * 0.5f ? G : T)) : (float)P.Z - 1.f;
		if (P.Z <= Floor + 10.f || CupFall[c] > 2.f)
		{
			CupDone[c] = 1;
			Cp->SetVisibility(false);
			const FVector At(P.X, P.Y, Floor);
			// shards where it hit (every machine draws them)
			if (UStaticMeshComponent* Sh = KitchenArt::Add(this, Root, TEXT("SM_V3_GlassShards"), 0, FTransform::Identity, KitchenArt::ESolid::None, false))
			{
				Sh->SetAbsolute(true, true, true);
				Sh->SetWorldTransform(FTransform(FRotator(0.f, 70.f * c, 0.f), At, FVector(0.55f * KitchenArt::ImportScale)));
			}
			AntAudio::Play(this, EAntSfx::CrashCeramic, At, 1.f);
			if (HasAuthority())
			{
				LevelV3::AddZone(2, At, 220.f);   // the shards cut
				if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
				{
					GM->MakeNoise(At, 0.8f, AntNoise::Crash, this, LastBy.Get());
					if (SourceFix) { GM->OnV3Break(SourceFix, LastBy.Get()); }
				}
				UE_LOG(LogTemp, Display, TEXT("ANTCLOTH a cup off the %s BREAKS at %s"), *KindName(), *At.ToCompactString());
			}
		}
	}
}

// ---- dev -----------------------------------------------------------------------------------------------------------

void AAntCloth::DevCrew(int32 N, const FVector& InDir, float Seconds)
{
	AAntKitchenGameMode* GM = GMOf(GetWorld());
	if (!HasAuthority() || !GM || !bBuilt)
	{
		return;
	}
	const FVector Dir = InDir.GetSafeNormal2D().IsNearlyZero() ? FVector(0.f, -1.f, 0.f) : InDir.GetSafeNormal2D();
	TArray<AAntCharacter*> Pool;
	auto Gather = [&]()
	{
		Pool.Reset();
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->bDead && It->Creature == 0 && It->Team == 0 && !It->IsPlayerControlled() && !It->IsAttached()) { Pool.Add(*It); }
		}
	};
	Gather();
	if (Pool.Num() < N) { GM->AddBots(0, N - Pool.Num()); Gather(); }
	// the hem on that side, spread along it
	const FVector C = Bounds().GetCenter();
	TArray<int32> Edge;
	float MaxD = -1e9f;
	for (int32 i = 0; i < X.Num(); ++i) { MaxD = FMath::Max(MaxD, (float)FVector::DotProduct(X[i] - C, Dir)); }
	for (int32 i = 0; i < X.Num(); ++i) { if ((float)FVector::DotProduct(X[i] - C, Dir) > MaxD - CellUU * 0.6f) { Edge.Add(i); } }
	Edge.Sort([this, Dir](int32 a, int32 b) { const FVector S = FVector::CrossProduct(FVector::UpVector, Dir); return FVector::DotProduct(X[a], S) < FVector::DotProduct(X[b], S); });
	Crew.Reset();
	for (int32 k = 0; k < N && k < Pool.Num() && Edge.Num(); ++k)
	{
		AAntCharacter* A = Pool[k];
		const int32 i = Edge[FMath::Clamp(FMath::RoundToInt((k + 0.5f) * Edge.Num() / (float)N - 0.5f), 0, Edge.Num() - 1)];
		float T, U, G;
		FVector At = X[i] + Dir * 45.f;
		if (HfSample((float)At.X, (float)At.Y, T, U, G)) { At.Z = (At.Z > T ? T : (G > NoZ * 0.5f ? G : T)) + 20.f; }
		A->LetGo();
		A->ApplyTeleport(At);
		A->SetActorRotation((-Dir).Rotation());
		A->Pulling = this;
		Crew.Add(A);
		FPuller Pl;
		Pl.Ant = A;
		Pl.P = i;
		Pl.Dir = Dir;
		Pullers.Add(Pl);
	}
	Wake(TEXT("a crew at the hem"));
	CrewUntil = GetWorld()->GetTimeSeconds() + Seconds;
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: a crew of %d holds the hem on the %s side for %.0f s"), *KindName(), Crew.Num(), *Dir.ToCompactString(), Seconds);
}

int32 AAntCloth::CutLine(const FVector& P, int32 Cells)
{
	// along the cloth's own u direction near P (a row of bites across the sheet)
	const int32 Near = NearestParticle(P);
	if (Near < 0)
	{
		return 0;
	}
	const int32 u0 = Near % NU, v0 = FMath::Min(Near / NU, NV - 2);
	int32 Cut = 0;
	for (int32 k = 0; k < Cells; ++k)
	{
		const int32 u = u0 - Cells / 2 + k;
		if (u < 0 || u >= NU - 1) { continue; }
		const int32 q = v0 * (NU - 1) + u;
		if (QuadAlive[q] == 1) { QuadAlive[q] = 0; DeadQuads.Add((uint16)q); ++Cut; }
	}
	bTopoDirty = true;
	Wake(TEXT("a row of bites"));
	return Cut;
}

void AntCloth::DevCam(UWorld* World, const FVector& From, const FVector& At, float Fov)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	static TWeakObjectPtr<ACameraActor> Cam;
	if (Fov <= 0.f)
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f);
		return;
	}
	if (!Cam.IsValid() || Cam->GetWorld() != World)
	{
		Cam = World->SpawnActor<ACameraActor>(From, (At - From).Rotation());
	}
	Cam->SetActorLocationAndRotation(From, (At - From).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(Fov);
	Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	PC->SetViewTargetWithBlend(Cam.Get(), 0.f);
}

void AntCloth::DevShot(UWorld* World, const FString& Folder, const FString& Name)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") / Folder / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	UE_LOG(LogTemp, Display, TEXT("ANTSHOT %s"), *Path);
}

AAntCloth* AntCloth::Find(UWorld* World, const FString& Word)
{
	if (!World)
	{
		return nullptr;
	}
	TArray<AAntCloth*> All;
	for (TActorIterator<AAntCloth> It(World); It; ++It) { All.Add(*It); }
	All.Sort([](const AAntCloth& A, const AAntCloth& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });
	const FString W = Word.ToLower();
	if (W.IsNumeric())
	{
		const int32 i = FCString::Atoi(*W);
		return All.IsValidIndex(i) ? All[i] : nullptr;
	}
	uint8 K = AntClothKind::Tablecloth;
	int32 Nth = 0;
	FString Base = W;
	while (Base.Len() && FChar::IsDigit(Base[Base.Len() - 1])) { Base.LeftChopInline(1); }
	if (Base.Len() < W.Len()) { Nth = FCString::Atoi(*W.Mid(Base.Len())); }
	if (Base.StartsWith(TEXT("long"))) { K = AntClothKind::LongCloth; }
	else if (Base.StartsWith(TEXT("curt"))) { K = AntClothKind::Curtain; }
	else if (Base.StartsWith(TEXT("plaid")) || Base.StartsWith(TEXT("throw"))) { K = AntClothKind::Plaid; }
	else if (Base.StartsWith(TEXT("fort")) || Base.StartsWith(TEXT("shalash"))) { K = AntClothKind::Fort; }
	else if (Base.StartsWith(TEXT("sheet"))) { K = AntClothKind::Sheet; }
	int32 n = 0;
	for (AAntCloth* C : All)
	{
		if (C->Kind == K || (K == AntClothKind::Tablecloth && C->Kind == AntClothKind::LongCloth && Base.StartsWith(TEXT("table"))))
		{
			if (n++ == Nth) { return C; }
		}
	}
	return nullptr;
}

bool AntCloth::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0 || !A[0].Equals(TEXT("cloth"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	auto Num = [&A](int32 i, float Def) { return A.IsValidIndex(i) && (A[i].IsNumeric() || A[i].StartsWith(TEXT("-"))) ? FCString::Atof(*A[i]) : Def; };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	AAntCloth* C = A.IsValidIndex(2) && !A[2].IsNumeric() && !A[2].StartsWith(TEXT("-")) ? Find(W, A[2]) : Find(W, TEXT("tablecloth"));
	const bool bNoWord = !A.IsValidIndex(2) || A[2].IsNumeric() || A[2].StartsWith(TEXT("-"));
	if (!C && bNoWord) { C = Find(W, TEXT("long")); }
	if (!C && bNoWord) { C = Find(W, TEXT("0")); }
	if (Op == TEXT("info") || Op == TEXT("list") || Op == TEXT("cost"))
	{
		if (Op == TEXT("cost")) { CostLog() = !CostLog(); }
		int32 Awake = 0, Np = 0;
		const double Ms = FrameMs(W, &Awake, &Np);
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH info: %d particles, %d awake, %.3f ms this frame, wind mode %d"), Np, Awake, Ms, WindMode());
		int32 i = 0;
		TArray<AAntCloth*> All;
		for (TActorIterator<AAntCloth> It(W); It; ++It) { All.Add(*It); }
		All.Sort([](const AAntCloth& X1, const AAntCloth& X2) { return X1.GetActorLocation().X < X2.GetActorLocation().X; });
		for (AAntCloth* X1 : All) { UE_LOG(LogTemp, Display, TEXT("ANTCLOTH  #%d %s at %s"), i++, *X1->Describe(), *X1->Bounds().GetCenter().ToCompactString()); }
		return true;
	}
	if (Op == TEXT("wind")) { WindMode() = (int32)Num(2, 1.f); UE_LOG(LogTemp, Display, TEXT("ANTCLOTH wind mode %d"), WindMode()); return true; }
	if (Op == TEXT("shot")) { DevShot(W, TEXT("cloth"), A.IsValidIndex(2) ? A[A.Num() - 1] : FString(TEXT("cloth"))); return true; }
	if (Op == TEXT("reset")) { ResetAll(W); return true; }
	if (!C)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH no such cloth"));
		return true;
	}
	if (Op == TEXT("yank") || Op == TEXT("pull"))
	{
		const int32 o = A.IsValidIndex(2) && !A[2].IsNumeric() && !A[2].StartsWith(TEXT("-")) ? 3 : 2;
		FVector Dir(Num(o, 0.f), Num(o + 1, -1.f), 0.f);
		C->Yank(Dir, Num(o + 2, 60.f), Num(o + 3, 3.f), Me);
	}
	else if (Op == TEXT("crew"))
	{
		// cloth crew <kind> N [dx dy] [sec]
		const int32 o = A.IsValidIndex(2) && !A[2].IsNumeric() ? 3 : 2;
		C->DevCrew((int32)Num(o, 6.f), FVector(Num(o + 1, 0.f), Num(o + 2, -1.f), 0.f), Num(o + 3, 10.f));
	}
	else if (Op == TEXT("tear") || Op == TEXT("bite"))
	{
		// cloth tear <kind> [x y z] [cells]: a row of bites through the point (default: the middle of the cloth)
		const int32 o = A.IsValidIndex(2) && !A[2].IsNumeric() && !A[2].StartsWith(TEXT("-")) ? 3 : 2;
		const FVector P = A.IsValidIndex(o + 2) ? FVector(Num(o, 0.f), Num(o + 1, 0.f), Num(o + 2, 0.f)) : C->Bounds().GetCenter() + FVector(0.f, 0.f, C->Bounds().GetExtent().Z);
		if (o == 2)
		{
			// at a point: the cloth nearest to it
			double Best = TNumericLimits<double>::Max();
			for (TActorIterator<AAntCloth> It(W); It; ++It)
			{
				const double D = It->Bounds().ComputeSquaredDistanceToPoint(P);
				if (D < Best) { Best = D; C = *It; }
			}
		}
		const int32 Cut = Op == TEXT("bite") ? (C->CutAt(P, 400.f) ? 1 : 0) : C->CutLine(P, (int32)Num(o + 3, 6.f));
		UE_LOG(LogTemp, Display, TEXT("ANTCLOTH %s: %d cells cut near %s"), *C->KindName(), Cut, *P.ToCompactString());
	}
	else if (Op == TEXT("wet")) { C->Wet = FMath::Clamp(Num(3, 1.f), 0.f, 1.f); C->Wake(TEXT("dev wet")); }
	else if (Op == TEXT("drop")) { C->DropAll(TEXT("dev")); }
	else if (Op == TEXT("wake")) { C->Wake(TEXT("dev")); }
	else if (Op == TEXT("rest")) { C->ResetCloth(); }
	else if (Op == TEXT("cam"))
	{
		// cloth cam <kind> yaw dist height [fov] [lookdz]: a fixed camera at the cloth (off: cloth cam off)
		if (A.IsValidIndex(2) && A[2] == TEXT("off")) { DevCam(W, FVector::ZeroVector, FVector::ZeroVector, 0.f); return true; }
		const FVector Ctr = C->Bounds().GetCenter() + FVector(0.f, 0.f, Num(8, 0.f));
		const float Yaw = FMath::DegreesToRadians(Num(3, 200.f)), Dist = Num(4, 4000.f), H = Num(5, 1500.f);
		DevCam(W, Ctr + FVector(FMath::Cos(Yaw) * Dist, FMath::Sin(Yaw) * Dist, H), Ctr, Num(6, 70.f));
	}
	else if (Op == TEXT("shot"))
	{
		DevShot(W, TEXT("cloth"), A.IsValidIndex(2) ? A[A.Num() - 1] : FString(TEXT("cloth")));
	}
	else
	{
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCLOTH dev %s: %s"), *Op, *C->Describe());
	return true;
}
