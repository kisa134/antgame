#include "AntMess.h"
#include "AntFx.h"
#include "AntTypes.h"
#include "AntCharacter.h"
#include "KitchenLoot.h"
#include "AntKitchenGameMode.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "DynamicRHI.h"

// =====================================================================================================================
// The look of every kind of mess. Thick / thin colour, roughness wet / dry / extra at the thin edge, relief, edge width,
// opacity thick / thin, how long it takes to dry (s; 0 never), and whether a dry one fades away (water).
// =====================================================================================================================
namespace
{
	struct FKindLook
	{
		FLinearColor Thick, Thin;
		float RWet, RDry, REdge;
		float Height, Edge;
		float OpThick, OpThin;
		float DryTime;
		bool bVanish;
	};

	const FKindLook GKinds[MessKind::Count] =
	{
		/* Jam       */ { FLinearColor(0.36f, 0.012f, 0.075f), FLinearColor(0.86f, 0.13f, 0.28f), 0.05f, 0.25f, 0.12f, 0.07f, 0.13f, 1.f, 0.8f, 240.f, false },
		/* Honey     */ { FLinearColor(0.50f, 0.20f, 0.01f), FLinearColor(0.95f, 0.60f, 0.10f), 0.04f, 0.18f, 0.08f, 0.08f, 0.16f, 0.96f, 0.6f, 400.f, false },
		/* Cola      */ { FLinearColor(0.05f, 0.016f, 0.004f), FLinearColor(0.32f, 0.14f, 0.05f), 0.03f, 0.40f, 0.15f, 0.03f, 0.06f, 0.94f, 0.45f, 120.f, false },
		/* Oil       */ { FLinearColor(0.30f, 0.24f, 0.05f), FLinearColor(0.55f, 0.48f, 0.18f), 0.02f, 0.06f, 0.04f, 0.01f, 0.05f, 0.5f, 0.22f, 0.f, false },
		/* Cream     */ { FLinearColor(0.96f, 0.92f, 0.84f), FLinearColor(0.93f, 0.88f, 0.80f), 0.32f, 0.55f, 0.08f, 0.14f, 0.20f, 1.f, 0.85f, 300.f, false },
		/* Crumbs    */ { FLinearColor(0.62f, 0.42f, 0.20f), FLinearColor(0.70f, 0.50f, 0.26f), 0.85f, 0.9f, 0.f, 0.16f, 0.025f, 1.f, 1.f, 0.f, false },
		/* Mud       */ { FLinearColor(0.16f, 0.11f, 0.06f), FLinearColor(0.34f, 0.26f, 0.17f), 0.30f, 0.85f, 0.1f, 0.05f, 0.08f, 0.92f, 0.5f, 150.f, false },
		/* Water     */ { FLinearColor(0.05f, 0.05f, 0.05f), FLinearColor(0.1f, 0.1f, 0.1f), 0.02f, 0.5f, 0.05f, 0.02f, 0.05f, 0.35f, 0.15f, 70.f, true },
		/* Chocolate */ { FLinearColor(0.10f, 0.04f, 0.015f), FLinearColor(0.26f, 0.13f, 0.05f), 0.14f, 0.45f, 0.1f, 0.06f, 0.11f, 1.f, 0.7f, 300.f, false },
		/* Scratch   */ { FLinearColor(0.88f, 0.88f, 0.86f), FLinearColor(0.80f, 0.80f, 0.78f), 0.65f, 0.65f, 0.f, -0.04f, 0.01f, 0.55f, 0.4f, 0.f, false },
		/* Print     */ { FLinearColor(0.3f, 0.02f, 0.05f), FLinearColor(0.5f, 0.06f, 0.08f), 0.10f, 0.35f, 0.1f, 0.04f, 0.06f, 0.85f, 0.45f, 200.f, false },
		/* Drag      */ { FLinearColor(0.5f, 0.4f, 0.3f), FLinearColor(0.55f, 0.45f, 0.35f), 0.70f, 0.8f, 0.f, 0.01f, 0.08f, 0.5f, 0.2f, 0.f, false },
		/* Juice     */ { FLinearColor(0.55f, 0.45f, 0.15f), FLinearColor(0.75f, 0.70f, 0.40f), 0.04f, 0.40f, 0.1f, 0.02f, 0.06f, 0.6f, 0.28f, 150.f, false },
		/* Sugar     */ { FLinearColor(0.97f, 0.97f, 0.97f), FLinearColor(0.95f, 0.95f, 0.95f), 0.30f, 0.30f, 0.f, 0.12f, 0.02f, 1.f, 1.f, 0.f, false },
	};

	const FKindLook& Look(uint8 K) { return GKinds[FMath::Min<int32>(K, MessKind::Count - 1)]; }

	// How runny a splat is: 1 a thin liquid (a spiky crown, drops thrown far), 0 a thick one (round lobes, a few fat drops).
	const float GSpike[MessKind::Count] = { 0.25f, 0.12f, 1.f, 0.8f, 0.08f, 1.f, 0.5f, 1.f, 0.3f, 1.f, 1.f, 1.f, 0.9f, 1.f };

	uint8 DefaultShape(uint8 Kind)
	{
		switch (Kind)
		{
		case MessKind::Crumbs: case MessKind::Sugar: return MessShape::Crumbs;
		case MessKind::Scratch: return MessShape::Scratch;
		case MessKind::Print: return MessShape::Print;
		case MessKind::Drag: return MessShape::Smear;
		case MessKind::Jam: case MessKind::Cream: case MessKind::Chocolate: return MessShape::Splat;
		default: return MessShape::Pool;
		}
	}

	TAutoConsoleVariable<int32> CVarMessDebug(TEXT("ant.MessDebug"), 0, TEXT("Logs every stain and burst (ANTMESS)."));

	const float DecalDepth = 7.f;          // half depth of the projection box: only what is within ~7 uu of the surface is painted
	const int32 MaxStains = 220;           // replicated
	const int32 MaxDecals = 300;           // drawn (replicated + local prints)
	const int32 MaxSpecks = 4000;          // the instanced droplets
	const int32 MaxSplats = 16;
	const int32 MaxAntBlobs = 48;

	UMaterialInterface* LoadMat(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path);
	}
}

// =====================================================================================================================
// Subsystem: the server makes the one replicated AAntMess of a game world.
// =====================================================================================================================

bool UAntMessSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UAntMessSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client || FParse::Param(FCommandLine::Get(), TEXT("AntNoMess")))
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<AAntMess>(AAntMess::StaticClass(), FTransform::Identity, P);
}

// =====================================================================================================================
// The actor
// =====================================================================================================================

AAntMess::AAntMess()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the cameras moved: the dust box follows this frame's view
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntMess::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntMess, Stains);
}

AAntMess* AAntMess::Get(const UObject* Ctx)
{
	UWorld* World = (GEngine && Ctx) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return nullptr;
	}
	static TWeakObjectPtr<AAntMess> Cached;
	if (AAntMess* M = Cached.Get())
	{
		if (M->GetWorld() == World && !M->IsActorBeingDestroyed())
		{
			return M;
		}
	}
	for (TActorIterator<AAntMess> It(World); It; ++It)
	{
		Cached = *It;
		return *It;
	}
	return nullptr;
}

float AAntMess::LocalNow() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AAntMess::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildLocal();
		SyncStains();
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMESS ready (%s): stains %d, stain material %d, blob material %d, dust layers %d"),
		GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), Stains.Num(), StainMat ? 1 : 0, BlobMat ? 1 : 0, Dust.Num());
}

void AAntMess::BuildLocal()
{
	if (bLocalBuilt)
	{
		return;
	}
	bLocalBuilt = true;
	StainMat = LoadMat(TEXT("/Game/Mess/M_MessStain.M_MessStain"));
	BlobMat = LoadMat(TEXT("/Game/Mess/M_MessBlob.M_MessBlob"));
	UMaterialInterface* DropMat = LoadMat(TEXT("/Game/Mess/M_MessDrop.M_MessDrop"));

	Specks = NewObject<UInstancedStaticMeshComponent>(this, TEXT("MessSpecks"));
	Specks->SetupAttachment(RootComponent);
	Specks->SetMobility(EComponentMobility::Movable);
	Specks->SetUsingAbsoluteLocation(true);
	Specks->SetUsingAbsoluteRotation(true);
	Specks->SetUsingAbsoluteScale(true);
	Specks->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (DropMat) { Specks->SetMaterial(0, DropMat); }
	Specks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Specks->SetCastShadow(false);
	Specks->bReceivesDecals = false;
	Specks->SetCanEverAffectNavigation(false);
	Specks->bAffectDistanceFieldLighting = false;
	Specks->NumCustomDataFloats = 5;
	Specks->RegisterComponent();
	AddInstanceComponent(Specks);

	BuildDust();
}

// =====================================================================================================================
// Stains: server
// =====================================================================================================================

FLinearColor AAntMess::KindColor(uint8 Kind) { return Look(Kind).Thick; }
FLinearColor AAntMess::KindColorThin(uint8 Kind) { return Look(Kind).Thin; }

int32 AAntMess::AddStain(uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg, float Aspect, float GrowSeconds, const FColor& Tint)
{
	if (!HasAuthority() || Kind >= MessKind::Count || Size < 2.f)
	{
		return 0;
	}
	const FVector N = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	// The same mess on the same spot again: the stain there grows instead of a second decal on top of it.
	for (FMessStain& S : Stains)
	{
		if (S.Kind == Kind && S.Shape == Shape && Shape != MessShape::Smear && Shape != MessShape::Print
			&& FVector::DistSquared(FVector(S.Pos), Pos) < FMath::Square(0.3f * FMath::Max<float>(S.Size, Size))
			&& FVector::DotProduct(FVector(S.Normal), N) > 0.9f)
		{
			if (Size > S.Size * 0.6f)
			{
				S.Size = (uint16)FMath::Clamp(FMath::Max<float>(S.Size, Size) * 1.12f, 2.f, 60000.f);
				S.Born = LocalNow();   // fresh again
				OnRep_Stains();
			}
			return S.Id;
		}
	}
	if (Stains.Num() >= MaxStains)
	{
		// Full: the oldest of the small half gives way (big pools and splats are what the eye remembers).
		int32 Victim = 0;
		float Best = 1.e9f;
		for (int32 i = 0; i < Stains.Num(); ++i)
		{
			const float Score = Stains[i].Born + Stains[i].Size * 0.2f;
			if (Score < Best) { Best = Score; Victim = i; }
		}
		Stains.RemoveAt(Victim);
	}
	FMessStain S;
	S.Id = NextId;
	NextId = NextId >= 0xFFF0 ? 1 : NextId + 1;
	S.Kind = Kind;
	S.Shape = Shape;
	S.Seed = (uint8)(Seed & 0xFF);
	S.Yaw = YawDeg < 0.f ? (uint8)((Seed >> 8) & 0xFF) : (uint8)FMath::RoundToInt(FMath::Fmod(YawDeg + 720.f, 360.f) / 360.f * 255.f);
	S.Asp = (uint8)FMath::Clamp(FMath::RoundToInt(Aspect * 16.f), 16, 255);
	S.Grow = (uint8)FMath::Clamp(FMath::RoundToInt(GrowSeconds), 0, 255);
	S.Size = (uint16)FMath::Clamp(Size, 2.f, 60000.f);
	S.Tint = Tint;
	S.Pos = Pos;
	S.Normal = N;
	S.Born = LocalNow();
	Stains.Add(S);
	if (CVarMessDebug.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMESS stain %d kind %d shape %d size %d at %s n %s"), S.Id, Kind, Shape, (int32)S.Size, *Pos.ToCompactString(), *N.ToCompactString());
	}
	OnRep_Stains();   // the server draws too (listen / standalone)
	ForceNetUpdate();
	return S.Id;
}

int32 AAntMess::Clean(const FVector& Center, float Radius)
{
	if (!HasAuthority())
	{
		return 0;
	}
	const int32 Before = Stains.Num();
	Stains.RemoveAll([&Center, Radius](const FMessStain& S) { return FVector::Dist(FVector(S.Pos), Center) < Radius + S.Size * 0.3f; });
	const int32 Gone = Before - Stains.Num();
	UE_LOG(LogTemp, Display, TEXT("ANTMESS clean %s r %.0f: %d stains wiped, %d left"), *Center.ToCompactString(), Radius, Gone, Stains.Num());
	OnRep_Stains();
	MulticastClean(Center, Radius);
	ForceNetUpdate();
	return Gone;
}

void AAntMess::MulticastClean_Implementation(FVector_NetQuantize At, float Radius)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		LocalClean(At, Radius);
	}
}

void AAntMess::MulticastDust_Implementation(bool bOn)
{
	SetDust(bOn);
}

void AAntMess::OnRep_Stains()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildLocal();
		SyncStains();
	}
}

void AAntMess::SyncStains()
{
	// Every replicated stain has a decal; a decal whose stain is gone (wiped, pushed out by the cap) fades away.
	TSet<int32> Have;
	for (const FDecal& D : Decals)
	{
		if (D.Id >= 0 && D.Id < 0x10000 && D.FadeOut < 0.f) { Have.Add(D.Id); }
	}
	TSet<int32> Want;
	for (const FMessStain& S : Stains)
	{
		Want.Add(S.Id);
		if (Have.Contains(S.Id))
		{
			// it may have grown
			for (FDecal& D : Decals)
			{
				if (D.Id == S.Id && D.FadeOut < 0.f && !FMath::IsNearlyEqual(D.Size, (float)S.Size, 1.f))
				{
					D.Size = S.Size;
					D.Born = S.Born;
					ApplyDecal(D, S.Kind, S.Shape, S.Pos, S.Normal, S.Size, S.Seed, S.Yaw * 360.f / 255.f, S.Asp / 16.f, FLinearColor(S.Tint));
				}
			}
			continue;
		}
		if (FDecal* D = AcquireDecal())
		{
			D->Id = S.Id;
			D->Kind = S.Kind;
			D->Born = S.Born;
			D->Grow = S.Grow;
			D->Life = 0.f;
			D->FadeOut = -1.f;
			D->Size = S.Size;
			D->Pos = S.Pos;
			ApplyDecal(*D, S.Kind, S.Shape, S.Pos, S.Normal, S.Size, S.Seed, S.Yaw * 360.f / 255.f, S.Asp / 16.f, FLinearColor(S.Tint));
		}
	}
	for (FDecal& D : Decals)
	{
		if (D.Id >= 0 && D.Id < 0x10000 && D.FadeOut < 0.f && !Want.Contains(D.Id))
		{
			D.FadeOut = 0.f;
		}
	}
}

// =====================================================================================================================
// Stains: decals (every machine that draws)
// =====================================================================================================================

AAntMess::FDecal* AAntMess::AcquireDecal()
{
	if (!StainMat)
	{
		return nullptr;
	}
	for (FDecal& D : Decals)
	{
		if (D.Id < 0) { return &D; }
	}
	if (Decals.Num() < MaxDecals)
	{
		FDecal& D = Decals.AddDefaulted_GetRef();
		D.C = NewObject<UDecalComponent>(this);
		D.C->SetupAttachment(RootComponent);
		D.C->SetUsingAbsoluteLocation(true);
		D.C->SetUsingAbsoluteRotation(true);
		D.C->SetUsingAbsoluteScale(true);
		D.M = UMaterialInstanceDynamic::Create(StainMat, this);
		D.C->SetDecalMaterial(D.M);
		D.C->SetFadeScreenSize(0.0004f);
		D.C->RegisterComponent();
		DecalComps.Add(D.C);
		DecalMids.Add(D.M);
		return &D;
	}
	// Full: the oldest local print goes first, then the oldest of all.
	FDecal* Oldest = nullptr;
	for (int32 Pass = 0; Pass < 2 && !Oldest; ++Pass)
	{
		for (FDecal& D : Decals)
		{
			if ((Pass == 1 || D.Id >= 0x10000) && (!Oldest || D.Born < Oldest->Born)) { Oldest = &D; }
		}
	}
	return Oldest;
}

void AAntMess::ApplyDecal(FDecal& D, uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg, float Aspect, const FLinearColor& Tint)
{
	if (!D.C || !D.M)
	{
		return;
	}
	const FKindLook& L = Look(Kind);
	const FVector N = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector T = FMath::Abs(N.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector;
	T = (T - N * FVector::DotProduct(T, N)).GetSafeNormal();
	T = T.RotateAngleAxis(YawDeg, N);
	const FRotator R = FRotationMatrix::MakeFromXY(-N, T).Rotator();
	D.C->SetWorldLocationAndRotation(Pos, R);
	const float G = D.Grow > 0.f ? FMath::Clamp((LocalNow() - D.Born) / D.Grow, 0.08f, 1.f) : 1.f;
	D.C->DecalSize = FVector(DecalDepth, Size * 0.5f * FMath::Max(1.f, Aspect) * G, Size * 0.5f * G);
	D.C->SortOrder = (D.Id & 0xFFFF) % 1000;
	D.C->MarkRenderStateDirty();
	D.C->SetVisibility(true);

	const bool bTinted = !(Tint.R > 0.98f && Tint.G > 0.98f && Tint.B > 0.98f);
	FLinearColor Thick = L.Thick, Thin = L.Thin;
	if (bTinted)
	{
		if (Kind == MessKind::Cream) { Thick = FMath::Lerp(Thick, Tint, 0.3f); Thin = FMath::Lerp(Thin, Tint, 0.4f); }
		else if (Kind == MessKind::Print || Kind == MessKind::Juice) { Thick = Tint * 0.7f; Thin = Tint; }
		else { Thick = Tint * 0.78f; Thin = Tint * 0.95f; }
	}
	D.M->SetScalarParameterValue(TEXT("Seed"), (Seed & 0xFF) / 255.f * 13.7f);
	D.M->SetScalarParameterValue(TEXT("Shape"), (float)Shape);
	D.M->SetScalarParameterValue(TEXT("Asp"), FMath::Max(1.f, Aspect));
	D.M->SetScalarParameterValue(TEXT("Edge"), L.Edge);
	D.M->SetScalarParameterValue(TEXT("Spike"), GSpike[FMath::Min<int32>(Kind, MessKind::Count - 1)]);
	D.M->SetVectorParameterValue(TEXT("Color"), Thick);
	D.M->SetVectorParameterValue(TEXT("Color2"), Thin);
	D.M->SetVectorParameterValue(TEXT("Par"), FLinearColor(L.Height, 0.f, 0.f, 1.f));
	D.M->SetVectorParameterValue(TEXT("Opac"), FLinearColor(L.OpThick, L.OpThin, 0.f, 0.f));
	D.M->SetVectorParameterValue(TEXT("Rough"), FLinearColor(L.RWet, L.RDry, L.REdge, 0.f));
}

void AAntMess::LocalStain(uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg, float Aspect, const FLinearColor& Tint, float Life)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	BuildLocal();
	if (FDecal* D = AcquireDecal())
	{
		D->Id = NextLocalId++;
		if (NextLocalId > 0x7FFFFFF0) { NextLocalId = 0x10000; }
		D->Kind = Kind;
		D->Born = LocalNow();
		D->Grow = 0.f;
		D->Life = Life;
		D->FadeOut = -1.f;
		D->Size = Size;
		D->Pos = Pos;
		ApplyDecal(*D, Kind, Shape, Pos, Normal, Size, Seed, YawDeg, Aspect, Tint);
	}
}

void AAntMess::TickDecals(float Dt)
{
	const float Now = LocalNow();
	static float DryTick = 0.f;
	DryTick += Dt;
	const bool bSlow = DryTick > 0.5f;
	if (bSlow) { DryTick = 0.f; }
	for (FDecal& D : Decals)
	{
		if (D.Id < 0 || !D.M)
		{
			continue;
		}
		const FKindLook& L = Look(D.Kind);
		if (D.FadeOut >= 0.f)
		{
			D.FadeOut += Dt;
			const float F = 1.f - D.FadeOut / 1.1f;
			if (F <= 0.f)
			{
				D.Id = -1;
				D.C->SetVisibility(false);
				continue;
			}
			D.M->SetVectorParameterValue(TEXT("Par"), FLinearColor(L.Height, L.DryTime > 0.f ? FMath::Clamp((Now - D.Born) / L.DryTime, 0.f, 0.7f) : 0.f, 0.f, F * F));
			continue;
		}
		if (D.Grow > 0.f && Now - D.Born < D.Grow + 0.5f)
		{
			const float G = FMath::Clamp((Now - D.Born) / D.Grow, 0.08f, 1.f);
			const FVector Sz = D.C->DecalSize;
			const float Asp = FMath::Max(1.f, Sz.Y / FMath::Max(1.f, Sz.Z));
			D.C->DecalSize = FVector(DecalDepth, D.Size * 0.5f * Asp * G, D.Size * 0.5f * G);
			D.C->MarkRenderStateDirty();
		}
		if (D.Life > 0.f && Now - D.Born > D.Life)
		{
			D.FadeOut = 0.f;
			continue;
		}
		if (bSlow)
		{
			const float Dry = L.DryTime > 0.f ? FMath::Clamp((Now - D.Born) / L.DryTime, 0.f, 0.7f) : 0.f;
			float Fade = 1.f;
			if (L.bVanish && L.DryTime > 0.f)
			{
				Fade = FMath::Clamp(1.f - ((Now - D.Born) / L.DryTime - 0.6f) * 2.f, 0.f, 1.f);   // water dries off completely
			}
			if (D.Life > 0.f)
			{
				Fade = FMath::Min(Fade, FMath::Clamp((D.Life - (Now - D.Born)) / 2.f, 0.f, 1.f));
			}
			D.M->SetVectorParameterValue(TEXT("Par"), FLinearColor(L.Height, Dry, 0.f, Fade));
		}
	}
}

void AAntMess::LocalClean(const FVector& At, float Radius)
{
	int32 Decs = 0, Sp = 0;
	for (FDecal& D : Decals)
	{
		if (D.Id >= 0 && D.FadeOut < 0.f && FVector::Dist(D.Pos, At) < Radius + D.Size * 0.3f)
		{
			D.FadeOut = 0.f;
			++Decs;
		}
	}
	if (Specks)
	{
		const float Now = LocalNow();
		for (int32 i = 0; i < SpeckInfo.Num(); ++i)
		{
			FSpeckInfo& S = SpeckInfo[i];
			if (S.Life >= 0.f && FVector::Dist(S.T.GetLocation(), At) < Radius)
			{
				// shrink away over the next second
				S.Life = FMath::Max(0.01f, Now - S.Born) + 0.9f;
				S.Born = S.Born;
				++Sp;
			}
		}
	}
	for (FDirt& D : Dirt)
	{
		if (D.Ant.IsValid() && FVector::Dist(D.Ant->GetActorLocation(), At) < Radius)
		{
			D.Feet = 0.f;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMESS local clean at %s r %.0f: %d decals, %d specks fade"), *At.ToCompactString(), Radius, Decs, Sp);
}

int32 AAntMess::NumDecals() const
{
	int32 N = 0;
	for (const FDecal& D : Decals) { N += D.Id >= 0 ? 1 : 0; }
	return N;
}

// =====================================================================================================================
// Specks: glossy droplets on one instanced mesh (a plane with a masked dome material: a bead of jam, a crumb pressed flat)
// =====================================================================================================================

void AAntMess::Speck(const FVector& At, const FVector& InN, float Size, float Flat, const FLinearColor& Col, float Rough, float Life, float YawDeg, float Stretch)
{
	if (!Specks || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector N = InN.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector T = FMath::Abs(N.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector;
	T = (T - N * FVector::DotProduct(T, N)).GetSafeNormal().RotateAngleAxis(YawDeg, N);
	const FQuat Q = FRotationMatrix::MakeFromXZ(T, N).ToQuat();
	const FTransform X(Q, At + N * 0.06f, FVector(Size * 0.01f * Stretch, Size * 0.01f, 1.f));
	int32 Slot = INDEX_NONE;
	const float Now = LocalNow();
	if (SpeckInfo.Num() < MaxSpecks)
	{
		Slot = Specks->AddInstance(X, true);
		SpeckInfo.SetNum(FMath::Max(SpeckInfo.Num(), Slot + 1));
	}
	else
	{
		// the ring: reuse a dead one near the cursor, else the next in turn
		for (int32 n = 0; n < 64; ++n)
		{
			const int32 i = (SpeckCursor + n) % SpeckInfo.Num();
			if (SpeckInfo[i].Life < 0.f) { Slot = i; break; }
		}
		if (Slot == INDEX_NONE) { Slot = SpeckCursor % SpeckInfo.Num(); }
		SpeckCursor = (Slot + 1) % SpeckInfo.Num();
		Specks->UpdateInstanceTransform(Slot, X, true, false, true);
	}
	if (Slot == INDEX_NONE)
	{
		return;
	}
	FSpeckInfo& I = SpeckInfo[Slot];
	I.Born = Now;
	I.Life = Life > 0.f ? Life : 1.e8f;
	I.T = X;
	Specks->SetCustomDataValue(Slot, 0, Col.R, false);
	Specks->SetCustomDataValue(Slot, 1, Col.G, false);
	Specks->SetCustomDataValue(Slot, 2, Col.B, false);
	Specks->SetCustomDataValue(Slot, 3, Rough, false);
	Specks->SetCustomDataValue(Slot, 4, 1.f - Flat, true);
}

void AAntMess::TickSpecks(float Dt)
{
	SpeckScan += Dt;
	if (!Specks || SpeckScan < 0.2f)
	{
		return;
	}
	SpeckScan = 0.f;
	const float Now = LocalNow();
	bool bDirty = false;
	for (int32 i = 0; i < SpeckInfo.Num(); ++i)
	{
		FSpeckInfo& S = SpeckInfo[i];
		if (S.Life < 0.f || S.Life > 1.e7f)
		{
			continue;
		}
		const float Age = Now - S.Born;
		const float Left = S.Life - Age;
		if (Left > 1.2f)
		{
			continue;
		}
		const float K = FMath::Clamp(Left / 1.2f, 0.f, 1.f);
		FTransform X = S.T;
		X.SetScale3D(S.T.GetScale3D() * FVector(K, K, 1.f) + FVector(0.0001f));
		Specks->UpdateInstanceTransform(i, X, true, false, true);
		bDirty = true;
		if (Left <= 0.f)
		{
			S.Life = -1.f;
		}
	}
	if (bDirty)
	{
		Specks->MarkRenderStateDirty();
	}
}

int32 AAntMess::NumSpecks() const
{
	int32 N = 0;
	for (const FSpeckInfo& S : SpeckInfo) { N += S.Life >= 0.f ? 1 : 0; }
	return N;
}

uint8 AAntMess::StainUnder(const FVector& P, float Margin, FLinearColor* OutColor) const
{
	// Newest first: what was spilled last is on top.
	for (int32 i = Decals.Num() - 1; i >= 0; --i)
	{
		const FDecal& D = Decals[i];
		if (D.Id < 0 || D.FadeOut >= 0.f || (!IsSticky(D.Kind) && D.Kind != MessKind::Mud && D.Kind != MessKind::Water && D.Kind != MessKind::Crumbs && D.Kind != MessKind::Sugar))
		{
			continue;
		}
		const float R = D.Size * (D.Kind == MessKind::Crumbs ? 0.35f : 0.42f) + Margin;
		if (FVector::DistSquared(D.Pos, P) < R * R && FMath::Abs(D.Pos.Z - P.Z) < 30.f + Margin)
		{
			if (OutColor)
			{
				FLinearColor C = Look(D.Kind).Thick;
				if (D.M) { D.M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), C); }
				*OutColor = C;
			}
			return D.Kind;
		}
	}
	return 255;
}

// =====================================================================================================================
// Bursts: what flies when food breaks. Appetising, never gory: crumbs, beads of jam, fizz, sugar glints, dollops of cream.
// =====================================================================================================================

namespace
{
	float R01(FRandomStream& R, float A, float B) { return R.FRandRange(A, B); }

	struct FPendingSpeck { float At; FVector P; FVector N; float Size; float Flat; FLinearColor C; float Rough; float Life; float Yaw; float Stretch; };
	TArray<FPendingSpeck> GPending;
	TWeakObjectPtr<UWorld> GPendingWorld;
}

void AAntMess::Burst(uint8 Kind, const FVector& At, const FVector& InN, float Size, const FLinearColor& Col, int32 Seed)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	BuildLocal();
	if (GPendingWorld.Get() != GetWorld()) { GPending.Reset(); GPendingWorld = GetWorld(); }
	FRandomStream R(Seed);
	const FVector N = InN.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const float Now = LocalNow();
	const float S = FMath::Clamp(Size, 20.f, 600.f);
	// the camera of a player close by takes a small kick (no time stop: other players and the server keep running)
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn()))
		{
			const float D = FVector::Dist(Me->GetActorLocation(), At);
			const float Reach = 600.f + S * 4.f;
			if (D < Reach) { Me->AddTrauma(FMath::Clamp(S / 300.f, 0.08f, 0.3f) * (1.f - D / Reach)); }
		}
	}
	Stir(At, FMath::Clamp(S * 0.5f, 20.f, 160.f), 300.f + S * 3.f);
	// ants in the blast get a coat of it
	if (IsSticky(Kind) || Kind == MessKind::Cream)
	{
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			if (A && !A->bDead && A->Creature == 0 && FVector::Dist(A->GetActorLocation(), At) < S * 0.9f + 20.f)
			{
				DirtyAnt(A, Kind, Col, 1.f, 1.f);
			}
		}
	}
	AAntFxManager* Fx = AAntFxManager::Get(this);
	const FVector Floor = At + N * 1.5f;
	auto Bit = [&](EAntShape Sh, EAntFxLook Lk, const FLinearColor& C, float Sc, const FVector& V, float Life, float Grav, bool bStreak)
	{
		if (!Fx) { return; }
		FAntFxBit B;
		B.Shape = Sh; B.Look = Lk; B.Color = C; B.Pos = Floor + FVector(R01(R, -0.15f, 0.15f) * S, R01(R, -0.15f, 0.15f) * S, R01(R, 2.f, 8.f));
		B.Vel = V; B.Scale0 = B.Scale1 = FVector(Sc); B.Life = Life; B.Gravity = Grav; B.FloorZ = At.Z + 0.6f; B.bStreak = bStreak;
		B.Rot = FQuat(FVector(R01(R, -1.f, 1.f), R01(R, -1.f, 1.f), R01(R, -1.f, 1.f)).GetSafeNormal(), R01(R, 0.f, 6.f));
		B.FadeIn = 0.f;
		if (bStreak) { B.Scale0 = B.Scale1 = FVector(Sc * 2.2f, Sc * 0.8f, Sc * 0.8f); }
		Fx->Emit(B);
	};
	auto Throw = [&](float Out, float Up) { const float A = R01(R, 0.f, 2.f * PI); return FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R01(R, 0.3f, 1.f) * Out + FVector(0.f, 0.f, R01(R, 0.4f, 1.f) * Up); };
	// a droplet lands where its arc meets the floor: its bead appears there when it gets there
	auto Land = [&](const FVector& V, float Size2, const FLinearColor& C, float Rough, float Flat, float Life)
	{
		const float G = 2600.f;
		const float T = (V.Z + FMath::Sqrt(V.Z * V.Z + 2.f * G * 6.f)) / G;
		const FVector P(At.X + V.X * T, At.Y + V.Y * T, At.Z);
		GPending.Add({ Now + T, P, N, Size2, Flat, C, Rough, Life, (float)FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)), 1.f + FMath::Min(1.5f, (float)V.Size2D() / 900.f) });
	};
	const int32 Count = FMath::Clamp(FMath::RoundToInt(S / 6.f), 10, 30);
	switch (Kind)
	{
	case MessKind::Crumbs:
	case MessKind::Sugar:
	{
		const bool bSugar = Kind == MessKind::Sugar;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Shade = R01(R, 0.75f, 1.15f);
			const FLinearColor C = bSugar ? FLinearColor(0.96f, 0.96f, 0.95f) : FLinearColor(Col.R * Shade, Col.G * Shade, Col.B * Shade);
			const FVector V = Throw(250.f + S * 2.5f, 500.f + S * 3.f);
			Bit(R.FRand() < 0.6f ? EAntShape::Cube : EAntShape::Sphere, EAntFxLook::Solid, C, R01(R, 0.012f, 0.03f) * FMath::Sqrt(S / 40.f), V, R01(R, 1.4f, 2.4f), 2600.f, false);
			if (i % 2 == 0) { Land(V, R01(R, 1.2f, 2.6f) * FMath::Sqrt(S / 40.f), C, bSugar ? 0.25f : 0.85f, 0.15f, 60.f); }
		}
		// a puff of flour-fine dust
		if (Fx)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				FAntFxBit P; P.Pos = Floor + FVector(0.f, 0.f, 3.f); P.Vel = Throw(180.f, 120.f); P.Drag = 3.f;
				P.Scale0 = FVector(S * 0.002f); P.Scale1 = FVector(S * 0.007f);
				P.Color = bSugar ? FLinearColor(0.95f, 0.95f, 0.95f, 0.10f) : FLinearColor(Col.R * 1.2f, Col.G * 1.2f, Col.B * 1.2f, 0.10f);
				P.Life = R01(R, 0.7f, 1.1f); P.FadeIn = 0.1f; Fx->Emit(P);
			}
			if (bSugar)
			{
				for (int32 i = 0; i < 10; ++i)
				{
					Bit(EAntShape::Sphere, EAntFxLook::Glow, FLinearColor(3.5f, 3.4f, 3.f), R01(R, 0.006f, 0.012f), Throw(400.f, 700.f), R01(R, 0.3f, 0.7f), 2600.f, false);
				}
			}
		}
		break;
	}
	case MessKind::Cola:
	case MessKind::Water:
	{
		const bool bCola = Kind == MessKind::Cola;
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector V = Throw(350.f + S * 2.f, 400.f + S * 2.5f);
			Bit(EAntShape::Sphere, EAntFxLook::Soft, bCola ? FLinearColor(0.35f, 0.16f, 0.05f, 0.85f) : FLinearColor(0.7f, 0.8f, 0.9f, 0.6f), R01(R, 0.012f, 0.03f), V, R01(R, 0.5f, 0.9f), 2600.f, true);
			if (i % 3 == 0) { Land(V, R01(R, 1.5f, 3.f), bCola ? FLinearColor(0.08f, 0.03f, 0.01f) : FLinearColor(0.2f, 0.2f, 0.22f), 0.03f, 0.5f, bCola ? 0.f : 30.f); }
		}
		if (bCola)
		{
			// fizz: tiny bright bubbles rising off the pool
			for (int32 i = 0; i < 14; ++i)
			{
				FAntFxBit B; B.Shape = EAntShape::Sphere; B.Look = EAntFxLook::Soft;
				B.Pos = At + FVector(R01(R, -0.3f, 0.3f) * S, R01(R, -0.3f, 0.3f) * S, 1.f);
				B.Vel = FVector(R01(R, -20.f, 20.f), R01(R, -20.f, 20.f), R01(R, 60.f, 180.f)); B.Drag = 1.5f;
				B.Scale0 = FVector(R01(R, 0.004f, 0.01f)); B.Scale1 = B.Scale0 * 0.4f;
				B.Color = FLinearColor(1.f, 0.95f, 0.85f, 0.55f); B.Life = R01(R, 0.4f, 1.2f); B.FadeIn = 0.05f; B.MinAmbient = 0.4f;
				if (Fx) { Fx->Emit(B); }
			}
		}
		break;
	}
	default:   // sticky and soft: jam, honey, juice, chocolate, cream, oil
	{
		const bool bCream = Kind == MessKind::Cream;
		const FKindLook& L = Look(Kind);
		const bool bWhite = Col.R > 0.98f && Col.G > 0.98f && Col.B > 0.98f;
		const FLinearColor Base = bWhite ? L.Thin : bCream ? FMath::Lerp(L.Thick, Col, 0.3f) : Col;
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector V = Throw(300.f + S * 2.2f, (bCream ? 300.f : 450.f) + S * 2.f);
			const float Sz = R01(R, 0.012f, bCream ? 0.05f : 0.035f) * FMath::Sqrt(S / 40.f);
			Bit(EAntShape::Sphere, EAntFxLook::Solid, Base * R01(R, 0.8f, 1.05f), Sz, V, R01(R, 0.9f, 1.4f), 2600.f, !bCream);
			Land(V, Sz * 100.f * R01(R, 0.9f, 1.6f), Base * R01(R, 0.7f, 1.f), L.RWet, bCream ? 0.0f : 0.25f, 0.f);
		}
		if (S > 30.f) { SplatBlob(At, N, S * (bCream ? 0.55f : 0.42f), Base, L.RWet, Seed); }
		break;
	}
	}
	if (CVarMessDebug.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMESS burst kind %d size %.0f at %s"), Kind, Size, *At.ToCompactString());
	}
}

void AAntMess::SplatBlob(const FVector& At, const FVector& N, float Size, const FLinearColor& Col, float Rough, int32 Seed)
{
	if (!BlobMat)
	{
		return;
	}
	FSplat* Use = nullptr;
	for (FSplat& S : Splats) { if (S.T > 9.f) { Use = &S; break; } }
	if (!Use && Splats.Num() < MaxSplats)
	{
		FSplat& S = Splats.AddDefaulted_GetRef();
		S.C = NewObject<UStaticMeshComponent>(this);
		S.C->SetupAttachment(RootComponent);
		S.C->SetUsingAbsoluteLocation(true); S.C->SetUsingAbsoluteRotation(true); S.C->SetUsingAbsoluteScale(true);
		S.C->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
		S.C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S.C->SetCastShadow(true);
		S.C->bReceivesDecals = false;
		S.M = UMaterialInstanceDynamic::Create(BlobMat, this);
		S.C->SetMaterial(0, S.M);
		S.C->RegisterComponent();
		SplatComps.Add(S.C);
		Use = &S;
	}
	if (!Use)
	{
		Use = &Splats[0];
		for (FSplat& S : Splats) { if (S.T > Use->T) { Use = &S; } }
	}
	FRandomStream R(Seed * 7 + 3);
	Use->T = 0.f;
	Use->Size = Size;
	Use->At = At;
	Use->N = N;
	Use->Yaw = R.FRandRange(0.f, 360.f);
	Use->Sx = R.FRandRange(0.85f, 1.25f);
	Use->Sy = R.FRandRange(0.85f, 1.15f);
	Use->M->SetVectorParameterValue(TEXT("Color"), Col);
	Use->M->SetScalarParameterValue(TEXT("Roughness"), Rough);
	Use->C->SetVisibility(true);
}

void AAntMess::TickSplats(float Dt)
{
	// The splat: hits as a ball, squashes flat (a short hold at the flattest: the "hit pause" of the blob itself),
	// spreads past its rest size with a wobble, settles, and after a few seconds sinks into its stain.
	for (FSplat& S : Splats)
	{
		if (S.T > 9.f || !S.C)
		{
			continue;
		}
		S.T += Dt;
		float W = 1.f, H = 1.f;
		const float T = S.T;
		if (T < 0.05f) { const float a = T / 0.05f; W = FMath::Lerp(0.55f, 1.15f, a); H = FMath::Lerp(0.7f, 0.32f, a); }
		else if (T < 0.11f) { W = 1.15f; H = 0.32f; }   // hold
		else if (T < 0.6f) { const float a = (T - 0.11f) / 0.49f; W = 1.15f + 0.45f * (1.f - FMath::Exp(-a * 5.f)) + 0.06f * FMath::Sin(a * 20.f) * (1.f - a); H = FMath::Lerp(0.32f, 0.16f, FMath::Min(1.f, a * 1.5f)); }
		else if (T < 5.5f) { W = 1.6f; H = 0.16f; }
		else if (T < 8.f) { const float a = (T - 5.5f) / 2.5f; W = 1.6f + 0.1f * a; H = FMath::Lerp(0.16f, 0.01f, a); }
		else { S.C->SetVisibility(false); S.T = 10.f; continue; }
		const float K = S.Size / 100.f;
		const FQuat Q = FRotationMatrix::MakeFromZ(S.N).ToQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(S.Yaw));
		S.C->SetWorldTransform(FTransform(Q, S.At + S.N * (S.Size * 0.5f * H * 0.55f), FVector(K * W * S.Sx, K * W * S.Sy, K * H)));
	}
	// droplets that reached the floor
	if (GPending.Num() > 0 && GPendingWorld.Get() == GetWorld())
	{
		const float Now = LocalNow();
		for (int32 i = GPending.Num() - 1; i >= 0; --i)
		{
			const FPendingSpeck& P = GPending[i];
			if (Now >= P.At)
			{
				Speck(P.P, P.N, P.Size, P.Flat, P.C, P.Rough, P.Life, P.Yaw, P.Stretch);
				GPending.RemoveAtSwap(i);
			}
		}
	}
}

// =====================================================================================================================
// Food pieces: squash when they land, smear where they slide, scuff where they are dragged (every machine, local)
// =====================================================================================================================

namespace
{
	bool IsSoftKind(uint8 K) { return K == MessKind::Jam || K == MessKind::Honey || K == MessKind::Cream || K == MessKind::Chocolate || K == MessKind::Juice; }

	bool FloorUnder(UWorld* W, const FVector& P, float Down, const AActor* Ignore, FVector& OutP, FVector& OutN)
	{
		FHitResult H;
		FCollisionQueryParams Q(TEXT("AntMessFloor"), false, Ignore);
		if (W->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 4.f), P - FVector(0.f, 0.f, Down), ECC_Visibility, Q))
		{
			OutP = H.ImpactPoint;
			OutN = H.ImpactNormal;
			return true;
		}
		return false;
	}

	int32 UpAxis(const USceneComponent* C)
	{
		const FVector L = C->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector);
		const FVector A = L.GetAbs();
		return A.X > A.Y && A.X > A.Z ? 0 : (A.Y > A.Z ? 1 : 2);
	}
}

void AAntMess::FoodTick(AKitchenLoot* Loot, float Dt, USceneComponent* Visual)
{
	if (!Loot || !Visual || Dt <= 0.f || Loot->bIsGear)
	{
		return;
	}
	const FVector P = Loot->GetActorLocation();
	FFood* F = Foods.Find(Loot);
	if (!F)
	{
		const FVector V0 = Loot->GetVelocity();
		if (V0.SizeSquared() < 40.f * 40.f && !Loot->IsCarried())
		{
			return;   // lying still: nothing to do, no bookkeeping
		}
		F = &Foods.Add(Loot);
		F->LastPos = P;
		F->Kind = AntMess::FoodKind(Loot->LootName);
		F->Visual = Visual;
		F->BaseScale = Visual->GetRelativeScale3D();
		F->SquashT = 10.f;
		F->bInit = true;
		return;
	}
	if (Loot->GetVelocity().ContainsNaN() || P.ContainsNaN())
	{
		return;
	}
	if (F->Visual.Get() != Visual)
	{
		F->Visual = Visual;
		F->BaseScale = Visual->GetRelativeScale3D();
		F->SquashT = 10.f;
	}
	// velocity from the motion itself (a client's copy of a physics body may not report one)
	const FVector Moved = P - F->LastPos;
	F->LastPos = P;
	FVector V = Moved / Dt;
	const FVector VR = Loot->GetVelocity();
	if (VR.SizeSquared() > V.SizeSquared()) { V = VR; }
	const float Rad = Loot->GetRadius();

	// ---- landing: squash and stretch
	// (one squash at a time, from the scale it has at rest: a bouncing piece must never compound its own squash)
	if (F->LastVz < -380.f && F->LastVz > -20000.f && V.Z > -90.f && !Loot->bHeld && F->SquashT >= 0.4f)
	{
		F->Squash = FMath::Clamp(-F->LastVz / 1700.f, 0.12f, 0.42f) * (IsSoftKind(F->Kind) ? 1.35f : 1.f);
		F->SquashT = 0.f;
		FVector FP, FN;
		// (food settling when the level is laid out is no mess: only a real fall, and not in the first seconds)
		if (F->LastVz < -700.f && GetWorld()->GetTimeSeconds() > 12.f && FloorUnder(GetWorld(), P, Rad * 2.5f + 20.f, Loot, FP, FN))
		{
			if (IsSoftKind(F->Kind))
			{
				LocalStain(F->Kind == MessKind::Juice ? MessKind::Juice : F->Kind, MessShape::Splat, FP, FN, Rad * 1.6f, (int32)(P.X * 7.f + P.Y), -1.f, 1.f, Loot->Color, 0.f);
			}
			else if (F->Kind == MessKind::Crumbs && F->Squash > 0.2f)
			{
				for (int32 i = 0; i < 3; ++i) { Speck(FP + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * Rad * 0.7f, FN, FMath::FRandRange(1.f, 2.2f), 0.5f, Loot->Color * 0.8f, 0.85f, 120.f); }
			}
			Stir(FP, FMath::Clamp(Rad * 0.6f, 10.f, 60.f), 200.f + Rad * 3.f);
		}
	}
	F->LastVz = V.Z;
	if (F->SquashT < 0.4f)
	{
		F->SquashT += Dt;
		const float T = F->SquashT;
		const float Sq = T >= 0.4f ? 0.f : F->Squash * FMath::Exp(-T * 11.f) * FMath::Cos(T * 36.f);
		FVector Sc(1.f + 0.5f * Sq);
		Sc[UpAxis(Visual)] = 1.f - Sq;
		Visual->SetRelativeScale3D(F->BaseScale * Sc);
	}
	else
	{
		const FVector Now3 = Visual->GetRelativeScale3D();
		if (!Now3.ContainsNaN() && Now3.GetMax() < 1.e4f) { F->BaseScale = Now3; }   // at rest: follow what RefreshVisual set (a bite makes it smaller)
	}

	// ---- sliding soft food smears; dragged food scuffs
	const float Speed2D = V.Size2D();
	const bool bDragged = Loot->IsCarried() && Loot->CarryRatio() < 1.f && !Loot->bHeld;
	const bool bSliding = !Loot->IsCarried() && !Loot->bHeld && Speed2D > 50.f && FMath::Abs(V.Z) < 120.f && IsSoftKind(F->Kind);
	if (bDragged || bSliding)
	{
		const float Step = bSliding ? FMath::Max(3.f, Rad * 0.25f) : FMath::Max(6.f, Rad * 0.45f);
		F->DragDist += FMath::Min(60.f, (float)Moved.Size2D());
		if (F->DragDist > Step)
		{
			F->DragDist = 0.f;
			FVector FP, FN;
			if (FloorUnder(GetWorld(), P, Rad * 2.f + 15.f, Loot, FP, FN))
			{
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
				if (IsSoftKind(F->Kind))
				{
					const FLinearColor C = F->Kind == MessKind::Cream ? FLinearColor(0.95f, 0.9f, 0.84f) : Look(F->Kind).Thin * 0.85f;
					Speck(FP, FN, Rad * 0.9f, 0.6f, C, Look(F->Kind).RWet + 0.05f, 0.f, Yaw, 1.8f);
				}
				else
				{
					const FLinearColor C = FMath::Lerp(FLinearColor(0.42f, 0.38f, 0.33f), Loot->Color, 0.45f);
					Speck(FP, FN, Rad * 0.55f, 1.f, C, 0.85f, 150.f, Yaw, 2.4f);
					if (F->Kind == MessKind::Crumbs && FMath::FRand() < 0.5f)
					{
						Speck(FP + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * Rad * 0.6f, FN, FMath::FRandRange(0.8f, 1.8f), 0.4f, Loot->Color * 0.85f, 0.85f, 150.f);
					}
				}
			}
		}
	}
}

// =====================================================================================================================
// The Human's steps: a step in something sticky leaves a few sticky prints after it
// =====================================================================================================================

void AAntMess::HumanStep(AActor* Who, const FVector& Foot, float YawDeg, float SoleLen)
{
	if (!Who || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Stir(Foot, 70.f, 1100.f);
	FVector FP = Foot, FN = FVector::UpVector;
	FloorUnder(GetWorld(), Foot + FVector(0.f, 0.f, 40.f), 160.f, Who, FP, FN);
	FLinearColor Col;
	const uint8 K = StainUnder(FP, SoleLen * 0.35f, &Col);
	FVector4& Sole = Soles.FindOrAdd(Who, FVector4(0, 0, 0, 0));
	if (K != 255 && K != MessKind::Water && K != MessKind::Crumbs && K != MessKind::Sugar)
	{
		Sole = FVector4(Col.R, Col.G, Col.B, 1.f);
		SoleKinds.Add(Who, K);
		return;   // stepping into it: the stain itself is under the foot
	}
	if (Sole.W > 0.06f)
	{
		const FLinearColor C((float)Sole.X, (float)Sole.Y, (float)Sole.Z);
		LocalStain(MessKind::Print, MessShape::Print, FP, FN, SoleLen * 0.40f, (int32)(FP.X * 3.f + FP.Y), YawDeg, 2.4f, C * FMath::Lerp(1.6f, 1.f, (float)Sole.W), 300.f);
		Sole.W *= 0.7f;
	}
}

// =====================================================================================================================
// Ants: sticky feet leave tracks, a coat of jam or cream tints them and sits on them as blobs for a while
// =====================================================================================================================

void AAntMess::DirtyAnt(AAntCharacter* Ant, uint8 Kind, const FLinearColor& Col, float Feet, float Body)
{
	FDirt* D = Dirt.FindByPredicate([Ant](const FDirt& X) { return X.Ant.Get() == Ant; });
	if (!D)
	{
		D = &Dirt.AddDefaulted_GetRef();
		D->Ant = Ant;
		D->LastStamp = Ant->GetGroundLocation();
	}
	if (Feet > D->Feet) { D->Feet = Feet; D->FeetKind = Kind; D->FeetColor = Col; }
	if (Body > D->Body + 0.05f)
	{
		D->Body = FMath::Max(D->Body, Body);
		D->BodyColor = Col;
		// blobs on the body (only for a real coat: a splat, not a walk through)
		if (Body > 0.6f && D->Blobs.Num() == 0 && BlobMat)
		{
			TArray<UStaticMeshComponent*> Pieces;
			Ant->GetComponents<UStaticMeshComponent>(Pieces);
			TArray<UStaticMeshComponent*> Targets;
			for (UStaticMeshComponent* C : Pieces)
			{
				const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
				const FString Nm = M ? M->GetName() : FString();
				if (C && C->IsVisible() && (Nm.Contains(TEXT("Gaster")) || Nm.Contains(TEXT("Thorax")) || Nm.Contains(TEXT("Head")) || Nm == TEXT("SM_Ant_Body"))) { Targets.Add(C); }
			}
			FRandomStream R(Ant->GetUniqueID() + (int32)(LocalNow() * 10.f));
			const float VS = Ant->VisScale();
			for (int32 i = 0; i < FMath::Min(3, Targets.Num() + 1) && Targets.Num() > 0; ++i)
			{
				UStaticMeshComponent* T = Targets[i % Targets.Num()];
				UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this);
				B->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
				B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				B->SetCastShadow(false);
				B->bReceivesDecals = false;
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BlobMat, B);
				M->SetVectorParameterValue(TEXT("Color"), Col);
				M->SetScalarParameterValue(TEXT("Roughness"), Kind == MessKind::Cream ? 0.35f : 0.06f);
				B->SetMaterial(0, M);
				B->RegisterComponent();
				const FBoxSphereBounds Bo = T->Bounds;
				const FVector Dir = (FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(0.3f, 1.f))).GetSafeNormal();
				const float Sz = VS * R.FRandRange(14.f, 26.f);
				B->SetWorldLocation(Bo.Origin + Dir * Bo.BoxExtent * 0.75f);
				B->SetWorldScale3D(FVector(Sz / 100.f, Sz / 100.f, Sz / 160.f));
				B->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
				B->AttachToComponent(T, FAttachmentTransformRules::KeepWorldTransform);
				D->Blobs.Add(B);
				AntBlobComps.Add(B);
			}
		}
	}
}

void AAntMess::ApplyAntTint(FDirt& D, float Amount)
{
	AAntCharacter* A = D.Ant.Get();
	if (!A)
	{
		return;
	}
	// The chitin's "Tint" times this: jam reddens it, cream and sugar pale it, chocolate darkens it.
	const FLinearColor C = D.BodyColor;
	const float L = C.GetLuminance();
	FLinearColor Mul = L > 0.6f ? FLinearColor(2.6f, 2.45f, 2.2f) : FMath::Lerp(FLinearColor::White, C / FMath::Max(0.05f, FMath::Max3(C.R, C.G, C.B)) * 1.25f, 0.85f);
	A->SetMessTint(FMath::Lerp(FLinearColor::White, Mul, FMath::Clamp(Amount, 0.f, 1.f)));
	D.TintApplied = Amount;
}

void AAntMess::TickAnts(float Dt)
{
	AntScan += Dt;
	if (AntScan < 0.05f)
	{
		return;
	}
	const float Step = AntScan;
	AntScan = 0.f;
	FVector Cam = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) { Cam = PC->PlayerCameraManager->GetCameraLocation(); }
	}
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Creature != 0 || FVector::DistSquared(A->GetActorLocation(), Cam) > FMath::Square(7000.f))
		{
			continue;
		}
		const FVector G = A->GetGroundLocation();
		FLinearColor Col;
		const uint8 K = StainUnder(G, 2.f, &Col);
		if (K != 255)
		{
			const bool bCoat = K == MessKind::Jam || K == MessKind::Honey || K == MessKind::Cream || K == MessKind::Chocolate;
			DirtyAnt(A, K, K == MessKind::Crumbs || K == MessKind::Sugar ? Col * 1.1f : Col, K == MessKind::Water ? 0.5f : 1.f, bCoat ? 0.35f : 0.f);
		}
	}
	for (int32 i = Dirt.Num() - 1; i >= 0; --i)
	{
		FDirt& D = Dirt[i];
		AAntCharacter* A = D.Ant.Get();
		if (!A || A->bDead)
		{
			for (auto& B : D.Blobs) { if (B.IsValid()) { B->DestroyComponent(); } }
			if (A) { A->SetMessTint(FLinearColor::White); }
			Dirt.RemoveAtSwap(i);
			continue;
		}
		// tracks: the tripod gait, alternate sides
		const FVector G = A->GetGroundLocation();
		const float VS = A->VisScale();
		const float Len = 98.f * VS;
		if (D.Feet > 0.03f && FVector::Dist(G, D.LastStamp) > Len * 0.3f)
		{
			const FVector N = A->GetVisualUp();
			FVector Fw = (G - D.LastStamp);
			Fw = (Fw - N * FVector::DotProduct(Fw, N)).GetSafeNormal();
			if (Fw.IsNearlyZero()) { Fw = A->GetActorForwardVector(); }
			const FVector Rt = FVector::CrossProduct(N, Fw);
			const float Sd = (D.Step++ % 2) ? 1.f : -1.f;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Fw.Y, Fw.X));
			const FKindLook& L = Look(D.FeetKind);
			const bool bDry = D.FeetKind == MessKind::Crumbs || D.FeetKind == MessKind::Sugar || D.FeetKind == MessKind::Mud;
			const float Dot = Len * (bDry ? 0.06f : 0.075f) * (0.6f + 0.4f * D.Feet);
			const FVector Feet[3] = { G + Fw * Len * 0.32f + Rt * Sd * Len * 0.34f, G - Rt * Sd * Len * 0.4f, G - Fw * Len * 0.34f + Rt * Sd * Len * 0.36f };
			for (const FVector& F : Feet)
			{
				Speck(F, N, Dot * FMath::FRandRange(0.8f, 1.2f), bDry ? 0.8f : 0.35f, D.FeetColor * FMath::Lerp(1.25f, 0.95f, D.Feet), L.RWet + (bDry ? 0.5f : 0.08f), 50.f, Yaw, 1.4f);
			}
			D.LastStamp = G;
			D.Feet *= D.FeetKind == MessKind::Water ? 0.85f : 0.95f;
		}
		else if (FVector::Dist(G, D.LastStamp) > Len * 2.f)
		{
			D.LastStamp = G;   // teleported / fell: start the track again
		}
		// the coat fades
		if (D.Body > 0.f)
		{
			D.Body = FMath::Max(0.f, D.Body - Step / 16.f);
			if (FMath::Abs(D.Body - D.TintApplied) > 0.03f || (D.Body <= 0.f && D.TintApplied > 0.f))
			{
				ApplyAntTint(D, D.Body * 0.9f);
			}
			for (int32 b = D.Blobs.Num() - 1; b >= 0; --b)
			{
				UStaticMeshComponent* B = D.Blobs[b].Get();
				if (!B) { D.Blobs.RemoveAt(b); continue; }
				if (D.Body < 0.05f) { B->DestroyComponent(); D.Blobs.RemoveAt(b); continue; }
				const float K = FMath::Clamp(D.Body / 0.7f, 0.15f, 1.f);
				const float Sz = VS * 20.f * K;
				B->SetWorldScale3D(FVector(Sz / 100.f, Sz / 100.f, Sz / 160.f));
			}
		}
		if (D.Feet <= 0.03f && D.Body <= 0.f && D.Blobs.Num() == 0)
		{
			Dirt.RemoveAtSwap(i);
		}
	}
	AntBlobComps.RemoveAll([](UStaticMeshComponent* C) { return !IsValid(C); });
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void AAntMess::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { DevTick(DeltaSeconds, (FPlatformTime::Seconds() - T0) * 1000.0); };
	TickDecals(DeltaSeconds);
	TickSpecks(DeltaSeconds);
	TickSplats(DeltaSeconds);
	TickAnts(DeltaSeconds);
	TickDust(DeltaSeconds);
	// forget food that went still (or was eaten, delivered, destroyed)
	static float Sweep = 0.f;
	Sweep += DeltaSeconds;
	if (Sweep > 1.f)
	{
		Sweep = 0.f;
		for (auto It = Foods.CreateIterator(); It; ++It)
		{
			AKitchenLoot* L = It.Key().Get();
			if (!L || (L->GetVelocity().SizeSquared() < 15.f * 15.f && !L->IsCarried() && It.Value().SquashT >= 0.4f))
			{
				if (L && It.Value().Visual.IsValid() && It.Value().SquashT < 0.4f) { It.Value().Visual->SetRelativeScale3D(It.Value().BaseScale); }
				It.RemoveCurrent();
			}
		}
		for (auto It = Soles.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
	}
}

void AAntMess::Stir(const FVector& At, float Strength, float Radius)
{
	// the newest strong stir wins the single slot of the dust shader
	const float Now = GetWorld()->GetTimeSeconds();
	const float Left = StirStrength * FMath::Exp(-(Now - StirTime) * 1.4f);
	if (Strength >= Left * 0.7f)
	{
		StirAt = At;
		StirTime = Now;
		StirStrength = Strength;
		StirRadius = Radius;
	}
}

FString AAntMess::Describe() const
{
	int32 Local = 0;
	for (const FDecal& D : Decals) { Local += D.Id >= 0x10000 ? 1 : 0; }
	return FString::Printf(TEXT("stains %d (replicated), decals drawn %d (local prints %d, pool %d), specks %d, splats live %d, dirty ants %d, food tracked %d, dust %s (%d layers)"),
		Stains.Num(), NumDecals(), Local, Decals.Num(), NumSpecks(), Splats.FilterByPredicate([](const FSplat& S) { return S.T < 9.f; }).Num(), Dirt.Num(), Foods.Num(), bDustOn ? TEXT("on") : TEXT("off"), Dust.Num());
}

// =====================================================================================================================
// The API
// =====================================================================================================================

namespace AntMess
{

int32 StainAt(const UObject* Ctx, uint8 Type, const FVector& Location, const FVector& Normal, float Size, int32 Seed)
{
	AAntMess* M = AAntMess::Get(Ctx);
	if (!M || !M->HasAuthority())
	{
		return 0;
	}
	const uint8 Shape = DefaultShape(Type);
	const float Asp = Shape == MessShape::Smear ? 2.5f : Shape == MessShape::Print ? 2.4f : 1.f;
	return M->AddStain(Type, Shape, Location, Normal, Size, Seed, -1.f, Asp);
}

int32 CleanArea(const UObject* Ctx, const FVector& Center, float Radius)
{
	AAntMess* M = AAntMess::Get(Ctx);
	return (M && M->HasAuthority()) ? M->Clean(Center, Radius) : 0;
}

uint8 FoodKind(const FString& Name)
{
	const FString N = Name.ToLower();
	auto Has = [&N](const TCHAR* S) { return N.Contains(S); };
	if (Has(TEXT("торт")) || Has(TEXT("мороженого")) || Has(TEXT("зефир")) || Has(TEXT("крем")) || Has(TEXT("масл"))) { return MessKind::Cream; }
	if (Has(TEXT("мёд")) || Has(TEXT("мед"))) { return MessKind::Honey; }
	if (Has(TEXT("варень")) || Has(TEXT("джем")) || Has(TEXT("конфет"))) { return MessKind::Jam; }
	if (Has(TEXT("мармелад")) || Has(TEXT("мишка")) || Has(TEXT("яблок")) || Has(TEXT("огур")) || Has(TEXT("изюм"))) { return MessKind::Juice; }
	if (Has(TEXT("шоколад"))) { return MessKind::Chocolate; }
	if (Has(TEXT("сахар")) || Has(TEXT("карамел")) || Has(TEXT("леденец"))) { return MessKind::Sugar; }
	if (Has(TEXT("кола"))) { return MessKind::Cola; }
	return MessKind::Crumbs;
}

void OnFoodBreak(AKitchenLoot* Food, const FVector& At, int32 Seed)
{
	AAntMess* M = Food ? AAntMess::Get(Food) : nullptr;
	if (!M || !M->HasAuthority())
	{
		return;
	}
	const uint8 K = FoodKind(Food->LootName);
	const float Rad = Food->GetRadius();
	FVector P = At, N = FVector::UpVector;
	FloorUnder(Food->GetWorld(), At + FVector(0.f, 0.f, 30.f), 300.f, Food, P, N);
	const FColor Tint = (K == MessKind::Crumbs || K == MessKind::Juice || K == MessKind::Cream) ? Food->Color.ToFColor(true) : FColor::White;
	const float Size = Rad * (K == MessKind::Crumbs || K == MessKind::Sugar ? 3.2f : K == MessKind::Cream ? 3.6f : 3.f) + 30.f;
	M->AddStain(K, DefaultShape(K) == MessShape::Pool ? MessShape::Splat : DefaultShape(K), P, N, Size, Seed);
	if (K == MessKind::Cream)
	{
		M->AddStain(MessKind::Crumbs, MessShape::Crumbs, P + FVector(Rad * 0.6f, -Rad * 0.4f, 0.f), N, Size * 0.8f, Seed * 3 + 1, -1.f, 1.f, 0.f, FLinearColor(0.78f, 0.55f, 0.3f).ToFColor(true));
	}
	M->MulticastBurst(K, P, N, (uint16)FMath::Clamp(Rad * 2.f, 10.f, 2000.f), Food->Color.ToFColor(true), (uint8)(Seed & 0xFF));
	UE_LOG(LogTemp, Display, TEXT("ANTMESS break «%s» kind %d radius %.0f at %s"), *Food->LootName, K, Rad, *P.ToCompactString());
}

void OnFixtureSpill(AActor* Fixture, const FString& Id, const FVector& At, bool bBroken, int32 Mat)
{
	AAntMess* M = Fixture ? AAntMess::Get(Fixture) : nullptr;
	if (!M || !M->HasAuthority())
	{
		return;
	}
	const int32 Seed = (int32)(At.X * 13.f + At.Y * 7.f);
	FVector P = At, N = FVector::UpVector;
	FloorUnder(Fixture->GetWorld(), At + FVector(0.f, 0.f, 60.f), 400.f, Fixture, P, N);
	if (bBroken)
	{
		M->AddStain(MessKind::Scratch, MessShape::Scratch, P, N, 520.f, Seed);
		if (Id == TEXT("jam_jar")) { M->AddStain(MessKind::Jam, MessShape::Splat, P + FVector(60.f, 40.f, 0.f), N, 760.f, Seed + 1); M->MulticastBurst(MessKind::Jam, P, N, 240, FColor::White, (uint8)Seed); }
		else if (Id == TEXT("pickles")) { M->AddStain(MessKind::Juice, MessShape::Pool, P, N, 1100.f, Seed + 2, -1.f, 1.f, 3.f, FLinearColor(0.7f, 0.78f, 0.45f).ToFColor(true)); M->MulticastBurst(MessKind::Water, P, N, 220, FColor::White, (uint8)Seed); }
		else if (Id == TEXT("teapot")) { M->AddStain(MessKind::Juice, MessShape::Pool, P, N, 900.f, Seed + 3, -1.f, 1.f, 2.f, FLinearColor(0.55f, 0.32f, 0.12f).ToFColor(true)); M->MulticastBurst(MessKind::Water, P, N, 200, FColor::White, (uint8)Seed); }
		else if (Id == TEXT("plant_k")) { M->AddStain(MessKind::Mud, MessShape::Splat, P, N, 900.f, Seed + 4); M->MulticastBurst(MessKind::Crumbs, P, N, 240, FLinearColor(0.2f, 0.13f, 0.08f).ToFColor(true), (uint8)Seed); }
		else { M->MulticastBurst(MessKind::Sugar, P, N, 160, FColor::White, (uint8)Seed); }   // glass and china: glints
	}
	else if (Id == TEXT("cola"))
	{
		// the open bottle runs for 20 s (KitchenFixture): the pool grows with it
		M->AddStain(MessKind::Cola, MessShape::Pool, P + FVector(-200.f, 0.f, 0.f), N, 1500.f, Seed, -1.f, 1.f, 20.f);
		M->MulticastBurst(MessKind::Cola, P, N, 160, FColor::White, (uint8)Seed);
	}
	else if (Id == TEXT("cereal"))
	{
		M->AddStain(MessKind::Crumbs, MessShape::Crumbs, P + FVector(-250.f, 0.f, 0.f), N, 900.f, Seed, -1.f, 1.f, 0.f, FLinearColor(0.9f, 0.68f, 0.3f).ToFColor(true));
	}
}

void FoodTick(AKitchenLoot* Loot, float Dt, USceneComponent* Visual)
{
	if (AAntMess* M = AAntMess::Get(Loot))
	{
		M->FoodTick(Loot, Dt, Visual);
	}
}

void HumanStep(AActor* Who, const FVector& Foot, float YawDeg, float SoleLen)
{
	if (AAntMess* M = AAntMess::Get(Who))
	{
		M->HumanStep(Who, Foot, YawDeg, SoleLen);
	}
}

void Stir(const UObject* Ctx, const FVector& At, float Strength, float Radius)
{
	if (AAntMess* M = AAntMess::Get(Ctx))
	{
		M->Stir(At, Strength, Radius);
	}
}

}   // namespace AntMess

void AAntMess::MulticastBurst_Implementation(uint8 Kind, FVector_NetQuantize At, FVector_NetQuantizeNormal N, uint16 Size, FColor Col, uint8 Seed)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		Burst(Kind, At, N, Size, FLinearColor(Col), Seed * 131 + 7);
	}
}

// =====================================================================================================================
// Dev (-AntDo "mess ..."): stain, test, break, splat, jamwalk, dust, clean, fill, hide, cost, info
// =====================================================================================================================

namespace
{
	uint8 KindByName(const FString& S)
	{
		static const TCHAR* Names[MessKind::Count] = { TEXT("jam"), TEXT("honey"), TEXT("cola"), TEXT("oil"), TEXT("cream"), TEXT("crumbs"), TEXT("mud"), TEXT("water"), TEXT("choc"), TEXT("scratch"), TEXT("print"), TEXT("drag"), TEXT("juice"), TEXT("sugar") };
		for (int32 i = 0; i < MessKind::Count; ++i) { if (S.Equals(Names[i], ESearchCase::IgnoreCase)) { return (uint8)i; } }
		return S.IsNumeric() ? (uint8)FMath::Clamp(FCString::Atoi(*S), 0, MessKind::Count - 1) : (uint8)MessKind::Jam;
	}

	FAntFoodDef FoodByName(const FString& S)
	{
		FAntFoodDef D;
		D.Shape = EAntShape::Cube;
		D.Scale = FVector(0.5f);
		D.Weight = 24;
		D.Value = 40;
		if (S == TEXT("cake")) { D.Name = TEXT("Кусочек торта"); D.Color = FLinearColor(1.f, 0.55f, 0.70f); D.Weight = 36; }
		else if (S == TEXT("cookie")) { D.Name = TEXT("Печенье"); D.Color = FLinearColor(0.74f, 0.48f, 0.2f); D.Shape = EAntShape::Cylinder; D.Scale = FVector(0.5f, 0.5f, 0.15f); }
		else if (S == TEXT("bread")) { D.Name = TEXT("Хлебная корка"); D.Color = FLinearColor(0.75f, 0.55f, 0.30f); }
		else if (S == TEXT("jam")) { D.Name = TEXT("Варенье"); D.Color = FLinearColor(0.55f, 0.05f, 0.12f); D.Shape = EAntShape::Sphere; }
		else if (S == TEXT("honey")) { D.Name = TEXT("Капля мёда"); D.Color = FLinearColor(1.f, 0.55f, 0.05f); D.Shape = EAntShape::Sphere; }
		else if (S == TEXT("choc")) { D.Name = TEXT("Долька шоколада"); D.Color = FLinearColor(0.25f, 0.12f, 0.05f); D.Weight = 30; }
		else if (S == TEXT("sugar")) { D.Name = TEXT("Кубик сахара"); D.Color = FLinearColor::White; D.Weight = 18; }
		else if (S == TEXT("icecream")) { D.Name = TEXT("Кусок мороженого"); D.Color = FLinearColor(1.f, 0.85f, 0.9f); D.Weight = 40; }
		else if (S == TEXT("apple")) { D.Name = TEXT("Кусочек яблока"); D.Color = FLinearColor(0.85f, 0.95f, 0.6f); }
		else if (S == TEXT("cheese")) { D.Name = TEXT("Кусочек сыра"); D.Color = FLinearColor(1.f, 0.8f, 0.15f); }
		else { D.Name = TEXT("Мармеладка"); D.Color = FLinearColor(0.9f, 0.2f, 0.4f); }
		return D;
	}

	struct FCostProbe { bool bOn = false; double TickSum = 0.0; double GpuSum = 0.0; double FrameSum = 0.0; int32 N = 0; float Acc = 0.f; FString Tag; };
	FCostProbe GCost;
	TWeakObjectPtr<AAntCharacter> GWalker;
	FVector GWalkTo = FVector::ZeroVector;
	bool GHidden = false;
}

bool AntMess::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (A.Num() < 2 || !A[0].Equals(TEXT("mess"), ESearchCase::IgnoreCase) || !GM)
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	AAntMess* M = AAntMess::Get(GM);
	if (!M)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev: no mess actor in this world"));
		return true;
	}
	const FString Op = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	auto Ground = [W, M](const FVector& P, FVector& OutN) { FVector G = P; OutN = FVector::UpVector; FloorUnder(W, P + FVector(0.f, 0.f, 60.f), 2000.f, M, G, OutN); return G; };

	if (Op == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMESS info: %s"), *M->Describe());
	}
	else if (Op == TEXT("stain"))
	{
		// mess stain <kind> x y z size [seed]   (z: drops onto what is below)
		FVector N; const FVector P = Ground(Vec(3), N);
		const int32 Id = StainAt(M, KindByName(A.IsValidIndex(2) ? A[2] : TEXT("jam")), P, N, Num(6, 400.f), (int32)Num(7, 17.f));
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev stain %s at %s size %.0f -> id %d"), *A[2], *P.ToCompactString(), Num(6, 400.f), Id);
	}
	else if (Op == TEXT("smear"))
	{
		// mess smear <kind> x y z size yaw aspect
		FVector N; const FVector P = Ground(Vec(3), N);
		M->AddStain(KindByName(A[2]), MessShape::Smear, P, N, Num(6, 200.f), (int32)Num(9, 5.f), Num(7, 0.f), Num(8, 3.f));
	}
	else if (Op == TEXT("test"))
	{
		// mess test x y z [spacing]: every kind in a row (the stain test)
		const float Sp = Num(5, 700.f);
		for (int32 k = 0; k < MessKind::Count; ++k)
		{
			FVector N; const FVector P = Ground(Vec(2) + FVector((k % 5) * Sp, (k / 5) * Sp, 0.f), N);
			const uint8 Shape = DefaultShape((uint8)k);
			M->AddStain((uint8)k, Shape, P, N, Sp * 0.75f, 11 + k * 37, Shape == MessShape::Smear || Shape == MessShape::Print ? 0.f : -1.f, Shape == MessShape::Smear ? 2.5f : Shape == MessShape::Print ? 2.4f : 1.f,
				0.f, k == MessKind::Crumbs ? FLinearColor(0.74f, 0.48f, 0.2f).ToFColor(true) : k == MessKind::Drag ? FLinearColor(0.6f, 0.45f, 0.3f).ToFColor(true) : FColor::White);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev test: %d kinds laid out from %s"), (int32)MessKind::Count, *Vec(2).ToCompactString());
	}
	else if (Op == TEXT("break") || Op == TEXT("splat"))
	{
		// mess break <food> x y z [height]: a piece of that food appears there and breaks at once (as if it fell)
		const FAntFoodDef D = FoodByName(A.IsValidIndex(2) ? A[2].ToLower() : TEXT("cake"));
		FVector N; const FVector G = Ground(Vec(3), N);
		const FVector P = G + FVector(0.f, 0.f, Num(6, 60.f));
		AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (L)
		{
			L->InitFood(D);
			L->FinishSpawning(FTransform(P));
			L->Shatter(G);
			UE_LOG(LogTemp, Display, TEXT("ANTMESS dev break «%s» at %s"), *D.Name, *G.ToCompactString());
		}
	}
	else if (Op == TEXT("burst"))
	{
		FVector N; const FVector G = Ground(Vec(3), N);
		M->MulticastBurst(KindByName(A[2]), G, N, (uint16)Num(6, 80.f), FColor::White, (uint8)Num(7, 9.f));
	}
	else if (Op == TEXT("jamwalk") && Me)
	{
		// mess jamwalk [kind] [len]: a pool in front of the player's ant, and the ant walks through it and on
		const FVector Fw = Me->GetActorForwardVector().GetSafeNormal2D();
		FVector N; const FVector G = Ground(Me->GetActorLocation() + Fw * 90.f, N);
		M->AddStain(KindByName(A.IsValidIndex(2) ? A[2] : TEXT("jam")), MessShape::Pool, G, N, 110.f, 23);
		GWalker = Me;
		GWalkTo = Me->GetActorLocation() + Fw * Num(3, 600.f);
		Me->DevGoTo = GWalkTo;
		Me->bDevGo = true;
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev jamwalk: pool at %s, walking to %s"), *G.ToCompactString(), *GWalkTo.ToCompactString());
	}
	else if (Op == TEXT("walk") && Me)
	{
		GWalker = Me; GWalkTo = Vec(2); Me->DevGoTo = GWalkTo; Me->bDevGo = true;
	}
	else if (Op == TEXT("dust"))
	{
		M->MulticastDust(Num(2, 1.f) > 0.5f);
	}
	else if (Op == TEXT("stir"))
	{
		FVector N; const FVector G = Ground(Vec(2), N);
		M->Stir(G, Num(5, 80.f), Num(6, 900.f));
		AntFx::StompDust(M, G);
	}
	else if (Op == TEXT("clean"))
	{
		CleanArea(M, Vec(2), Num(5, 800.f));
	}
	else if (Op == TEXT("fill"))
	{
		// mess fill N: N random stains over the kitchen and the living room, plus specks up to the cap (the cost test)
		FRandomStream R(77);
		const int32 Count = (int32)Num(2, 150.f);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector P = R.FRand() < 0.6f ? FVector(R.FRandRange(-6500.f, 6500.f), R.FRandRange(-4600.f, 2700.f), 200.f) : FVector(R.FRandRange(7600.f, 19800.f), R.FRandRange(-8600.f, 1200.f), 200.f);
			FVector N; const FVector G = Ground(P, N);
			const uint8 K = (uint8)R.RandRange(0, MessKind::Juice);
			M->AddStain(K, DefaultShape(K), G, N, R.FRandRange(150.f, 900.f), R.RandRange(0, 9999));
		}
		for (int32 i = 0; i < (int32)Num(3, 3000.f); ++i)
		{
			FVector N; const FVector G = Ground(FVector(R.FRandRange(-6500.f, 6500.f), R.FRandRange(-4600.f, 2700.f), 200.f), N);
			M->Speck(G, N, R.FRandRange(0.8f, 3.f), 0.4f, FLinearColor(0.4f, 0.05f, 0.08f), 0.08f, 0.f, R.FRandRange(0.f, 360.f), 1.3f);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev fill: %s"), *M->Describe());
	}
	else if (Op == TEXT("hide"))
	{
		GHidden = Num(2, 1.f) > 0.5f;
		M->SetMessHidden(GHidden);
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev hide %d"), GHidden ? 1 : 0);
	}
	else if (Op == TEXT("cost"))
	{
		// mess cost <tag> [1|0]: averages of the frame, the GPU frame and the mess tick, logged every 4 s while on
		GCost.bOn = Num(3, 1.f) > 0.5f; GCost.Tag = A.IsValidIndex(2) ? A[2] : TEXT("-"); GCost.N = 0; GCost.TickSum = GCost.GpuSum = GCost.FrameSum = 0.0; GCost.Acc = 0.f;
	}
	else if (Op == TEXT("decaltest"))
	{
		// mess decaltest x y z [size]: a plain engine decal there (is the decal path alive at all?)
		FVector N; const FVector G = Ground(Vec(2), N);
		UDecalComponent* C = NewObject<UDecalComponent>(M);
		C->SetupAttachment(M->GetRootComponent());
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true);
		C->SetDecalMaterial(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial")));
		C->DecalSize = FVector(20.f, Num(5, 200.f), Num(5, 200.f));
		C->SetWorldLocationAndRotation(G, FRotator(-90.f, 0.f, 0.f));
		C->RegisterComponent();
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev decaltest at %s, material %s"), *G.ToCompactString(), *GetNameSafe(C->GetDecalMaterial()));
	}
	else if (Op == TEXT("profilegpu") && PC)
	{
		PC->ConsoleCommand(TEXT("ProfileGPU"), false);
	}
	else if (Op == TEXT("debug"))
	{
		CVarMessDebug->Set((int32)Num(2, 1.f));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMESS dev: unknown «%s»"), *Op);
	}
	return true;
}

void AAntMess::SetMessHidden(bool bHide)
{
	for (UDecalComponent* C : DecalComps) { if (C) { C->SetHiddenInGame(bHide); } }
	if (Specks) { Specks->SetHiddenInGame(bHide); }
	for (UStaticMeshComponent* C : SplatComps) { if (C) { C->SetHiddenInGame(bHide); } }
}

void AAntMess::DevTick(float Dt, double TickMs)
{
	// -AntMessDo="t:cmd;..." works on ANY machine (a client has no game mode, so -AntDo does not run there):
	// cam x y z tx ty tz [fov] | shot | info | hud 0|1 - to look at what a client sees (replication checks).
	static TArray<TPair<float, FString>> Script;
	static bool bParsed = false;
	if (!bParsed)
	{
		bParsed = true;
		FString S;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntMessDo="), S, false))
		{
			TArray<FString> Steps;
			S.ParseIntoArray(Steps, TEXT(";"));
			for (const FString& St : Steps)
			{
				FString At, Cmd;
				if (St.Split(TEXT(":"), &At, &Cmd)) { Script.Add(TPair<float, FString>(FCString::Atof(*At), Cmd.TrimStartAndEnd())); }
			}
		}
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Script.Num(); )
	{
		if (Script[i].Key > Now) { ++i; continue; }
		TArray<FString> A;
		Script[i].Value.ParseIntoArrayWS(A);
		Script.RemoveAt(i);
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (A.Num() == 0 || !PC) { continue; }
		auto Num = [&A](int32 k, float Def = 0.f) { return A.IsValidIndex(k) ? FCString::Atof(*A[k]) : Def; };
		UE_LOG(LogTemp, Display, TEXT("ANTMESS local do %.1f: %s"), Now, *FString::Join(A, TEXT(" ")));
		if (A[0] == TEXT("cam"))
		{
			static TWeakObjectPtr<ACameraActor> Cam;
			if (!Cam.IsValid()) { Cam = GetWorld()->SpawnActor<ACameraActor>(); }
			const FVector From(Num(1), Num(2), Num(3)), To(Num(4), Num(5), Num(6));
			Cam->SetActorLocationAndRotation(From, (To - From).Rotation());
			Cam->GetCameraComponent()->SetFieldOfView(Num(7, 60.f));
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			PC->SetViewTargetWithBlend(Cam.Get(), 0.f);
		}
		else if (A[0] == TEXT("shot")) { PC->ConsoleCommand(TEXT("Shot"), false); }
		else if (A[0] == TEXT("hud") && PC->GetHUD()) { PC->GetHUD()->bShowHUD = Num(1) > 0.5f; }
		else if (A[0] == TEXT("info")) { UE_LOG(LogTemp, Display, TEXT("ANTMESS info (%s): %s"), GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), *Describe()); }
	}
	if (AAntCharacter* Wk = GWalker.Get())
	{
		if (FVector::Dist2D(Wk->GetActorLocation(), GWalkTo) < 30.f || Wk->bDead)
		{
			Wk->bDevGo = false;
			GWalker = nullptr;
			UE_LOG(LogTemp, Display, TEXT("ANTMESS dev walk: arrived at %s"), *Wk->GetActorLocation().ToCompactString());
		}
	}
	if (GCost.bOn)
	{
		GCost.TickSum += TickMs;
		GCost.FrameSum += Dt * 1000.0;
		GCost.GpuSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		++GCost.N;
		GCost.Acc += Dt;
		if (GCost.Acc >= 4.f && GCost.N > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTMESS cost [%s] frame_ms=%.3f gpu_ms=%.3f mess_tick_ms=%.4f frames=%d | %s"), *GCost.Tag, GCost.FrameSum / GCost.N, GCost.GpuSum / GCost.N, GCost.TickSum / GCost.N, GCost.N, *Describe());
			GCost.N = 0; GCost.TickSum = GCost.GpuSum = GCost.FrameSum = 0.0; GCost.Acc = 0.f;
		}
	}
}
