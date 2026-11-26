// Matter (KAN-21, docs/MATERIALS.md "Вещество"): water, heat, fire, frost and electricity on the things of the flat, shown on
// the things themselves through the state layer (AntState.h). Server-authoritative; every machine draws from the records.
#include "AntMatter.h"
#include "AntState.h"
#include "AntReact.h"
#include "AntRun.h"
#include "AntFx.h"
#include "AntAudio.h"
#include "AntMess.h"
#include "AntWreck.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntFluidWorld.h"
#include "AntFluidSim.h"
#include "KitchenArt.h"
#include "KitchenLoot.h"
#include "LevelV3.h"
#include "Map4Look.h"
#include "HumanCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "GameFramework/HUD.h"
#include "AntCloth.h"

namespace
{
	constexpr float Room = 22.f;
	constexpr float SimStep = 0.125f;     // the simulation runs 8 times a second
	constexpr float NetStep = 0.25f;      // the records are written 4 times a second
	constexpr int32 MaxBodies = 220;
	constexpr int32 MaxBurning = 24;
	constexpr int32 MaxPools = 6;
	constexpr int32 MaxIce = 6;
	constexpr int32 MaxServerSpots = 8;
	constexpr float CellSize = 800.f;
	constexpr float CmToUu = 19.f;        // 1 m = 1900 uu
	constexpr float Litre = 1900.f * 1900.f * 1900.f * 0.001f;

	AAntKitchenGameMode* GMOf(const UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	float NetNowOf(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? (float)GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	}

	/** The liquid of the fluid world -> the AntState kind it leaves on a thing. */
	uint8 KindOfLiquid(uint8 Liquid)
	{
		switch (Liquid)
		{
		case AntFluid::Cola: return AntState::Cola;
		case AntFluid::Oil: return AntState::Oil;
		case AntFluid::Honey: return AntState::Honey;
		default: return AntState::Wet;
		}
	}
	/** Soak slot (0 oil, 1 jam, 2 cola, 3 honey) of a state kind, or -1. */
	int32 SoakSlot(uint8 Kind) { return Kind >= AntState::Oil && Kind <= AntState::Honey ? Kind - AntState::Oil : -1; }

	/** A mess stain's kind -> the AntState kind it leaves on what it lands on. */
	uint8 KindOfStain(uint8 Mess)
	{
		switch (Mess)
		{
		case MessKind::Jam: case MessKind::Juice: return AntState::Jam;
		case MessKind::Honey: return AntState::Honey;
		case MessKind::Cola: case MessKind::Chocolate: return AntState::Cola;
		case MessKind::Oil: return AntState::Oil;
		case MessKind::Water: return AntState::Wet;
		case MessKind::Scratch: return AntState::Damage;
		default: return AntState::Dirt;
		}
	}

	bool Burns(const FAntMatProps* M) { return M && M->IgniteAt > 0.f; }
	bool Is(const FAntMatProps* M, const TCHAR* Into) { return M && M->BurnsInto.Equals(Into, ESearchCase::IgnoreCase); }

	FVector CameraOf(const UWorld* W)
	{
		const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		return PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector(0.f, 0.f, 1.e7f);
	}
}

// =====================================================================================================================
// The test things
// =====================================================================================================================

AAntMatterThing::AAntMatterThing()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
}

void AAntMatterThing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntMatterThing, What);
	DOREPLIFETIME(AAntMatterThing, Size);
}

void AAntMatterThing::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AAntMatterThing::OnRep_What() { bBuilt = false; Build(); }

FString AAntMatterThing::MatId() const
{
	const FString W = What.ToString();
	if (W == TEXT("napkin")) { return TEXT("paper"); }
	if (W == TEXT("rag") || W == TEXT("towel")) { return TEXT("cloth"); }
	if (W == TEXT("pot") || W == TEXT("pan")) { return TEXT("metal"); }
	if (W == TEXT("box")) { return TEXT("cardboard"); }
	if (W == TEXT("bread")) { return TEXT("bread"); }
	if (W == TEXT("plastic") || W == TEXT("tub")) { return TEXT("plastic"); }
	if (W == TEXT("glass")) { return TEXT("glass"); }
	if (W == TEXT("board")) { return TEXT("wood"); }
	return W;
}

void AAntMatterThing::Build()
{
	if (bBuilt || What.IsNone() || !Body) { return; }
	bBuilt = true;
	const FString W = What.ToString();
	const bool bPot = W == TEXT("pot") || W == TEXT("pan");
	UStaticMesh* Mesh = Map4Look::Shape(bPot ? 2 : 0);   // a cylinder for the pot, a box for the rest
	if (!Mesh) { return; }
	Body->SetStaticMesh(Mesh);
	Body->SetRelativeScale3D(FVector(Size) / 100.f);
	const TCHAR* Look = W == TEXT("napkin") ? TEXT("Paper") : (W == TEXT("rag") || W == TEXT("towel")) ? TEXT("ClothFestive") : bPot ? TEXT("Steel")
		: W == TEXT("box") ? TEXT("Cardboard") : W == TEXT("board") ? TEXT("Oak") : W == TEXT("glass") ? TEXT("Ice") : TEXT("Paper");
	const Map4Look::FLook* L = Map4Look::Named(Look);
	if (!L) { L = Map4Look::Named(TEXT("Paper")); }
	if (W == TEXT("plastic") || W == TEXT("tub")) { Body->SetMaterial(0, Map4Look::Paint(Body, FLinearColor(0.15f, 0.45f, 0.85f))); }   // a blue plastic tub
	else if (L) { Body->SetMaterial(0, Map4Look::Get(*L)); }
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetGenerateOverlapEvents(false);
	Body->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("Surf_%s"), *MatId())));
}

// =====================================================================================================================
// AAntMatterNet: setup
// =====================================================================================================================

AAntMatterNet::AAntMatterNet()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5.f);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AAntMatterNet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntMatterNet, Recs);
	DOREPLIFETIME(AAntMatterNet, Pools);
	DOREPLIFETIME(AAntMatterNet, Ices);
}

AAntMatterNet* AAntMatterNet::Get(const UWorld* W)
{
	if (!W) { return nullptr; }
	static TWeakObjectPtr<AAntMatterNet> Cache;
	if (Cache.IsValid() && Cache->GetWorld() == W && !Cache->IsActorBeingDestroyed()) { return Cache.Get(); }
	for (TActorIterator<AAntMatterNet> It(const_cast<UWorld*>(W)); It; ++It) { Cache = *It; return *It; }
	return nullptr;
}

void AAntMatterNet::BeginPlay()
{
	Super::BeginPlay();
	AntMaterials::Load();
	if (GetNetMode() != NM_DedicatedServer)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(GetRootComponent());
			L->SetAbsolute(true, true, true);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(0.f);
			L->SetLightColor(FLinearColor(1.f, 0.55f, 0.22f));
			L->SetAttenuationRadius(600.f);
			L->SetCastShadows(false);
			L->SetVisibility(false);
			L->RegisterComponent();
			Lights.Add(L);
		}
		// near a fire: the highlights compressed by the local exposure, so a burning thing in a dark cupboard is not a white blot
		FireGrade = NewObject<UPostProcessComponent>(this);
		FireGrade->SetupAttachment(GetRootComponent());
		FireGrade->bUnbound = true;
		FireGrade->Priority = 5.f;
		FireGrade->BlendWeight = 0.f;
		FireGrade->Settings.bOverride_LocalExposureHighlightContrastScale = true;
		FireGrade->Settings.LocalExposureHighlightContrastScale = 0.55f;
		FireGrade->Settings.bOverride_LocalExposureShadowContrastScale = true;
		FireGrade->Settings.LocalExposureShadowContrastScale = 0.85f;
		// and the eye does not open up for the dark round a fire at night (that blew a lit cupboard out to white)
		FireGrade->Settings.bOverride_AutoExposureMinBrightness = true;
		FireGrade->Settings.AutoExposureMinBrightness = 0.f;   // EV100 (the project extends the luminance range); 1.5 blacked the night out
		FireGrade->RegisterComponent();
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER net up (%s)"), HasAuthority() ? TEXT("server") : TEXT("client"));
}

void AAntMatterNet::EndPlay(const EEndPlayReason::Type Reason)
{
	// put back what this machine hid or painted
	for (auto& It : Shown)
	{
		if (UPrimitiveComponent* C = It.Value.C.Get())
		{
			if (It.Value.bHidden) { C->SetVisibility(true); }
			if (It.Value.bSagged && C->Mobility == EComponentMobility::Movable) { C->SetWorldScale3D(It.Value.OrigScale); }
			AntState::ClearSource(C, AntState::Matter);
		}
	}
	Shown.Reset();
	Super::EndPlay(Reason);
}

void AAntMatterNet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!AntMatter::Enabled()) { return; }
	const double T0 = FPlatformTime::Seconds();
	if (HasAuthority())
	{
		SimClock += DeltaSeconds;
		int32 Steps = 0;
		while (SimClock >= SimStep && Steps < 2)
		{
			SimClock -= SimStep;
			ServerStep(SimStep);
			++Steps;
		}
		if (SimClock > SimStep) { SimClock = 0.f; }   // a hitch: drop the backlog instead of spiralling
		NetClock += DeltaSeconds;
		if (NetClock >= NetStep) { NetClock = 0.f; WriteRecords(); }
	}
	for (int32 i = 0; i < Script.Num();)
	{
		if (Script[i].Key > LocalClock) { ++i; continue; }
		TFunction<void()> Fn = MoveTemp(Script[i].Value);
		Script.RemoveAt(i);
		if (Fn) { Fn(); }
	}
	LastTickMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	AvgTickMs = AvgTickMs * 0.97 + LastTickMs * 0.03;
	const double T1 = FPlatformTime::Seconds();
	LocalTick(DeltaSeconds);
	LastLocalMs = (FPlatformTime::Seconds() - T1) * 1000.0;
	AvgLocalMs = AvgLocalMs * 0.97 + LastLocalMs * 0.03;
}

FString AAntMatterNet::Describe() const
{
	int32 CardsOn = 0;
	for (const UStaticMeshComponent* C : Cards) { if (C && C->IsVisible()) { ++CardsOn; } }
	const AAntFxManager* Fx = AAntFxManager::Get(this);
	return FString::Printf(TEXT("bodies %d (burning %d), records %d, pools %d, ice %d, registry %d (%d moving), sim %.3f ms (avg %.3f), local %.3f ms (avg %.3f), states shown %d, flame cards %d, fx bits %d/%d"),
		Bodies.Num(), NumBurning, Recs.Num(), Pools.Num(), Ices.Num(), Reg.Num(), Dyn.Num(), LastTickMs, AvgTickMs, LastLocalMs, AvgLocalMs, AntState::NumTracked(),
		CardsOn, Fx ? Fx->NumLive() : 0, AAntFxManager::MaxBits);
}

// =====================================================================================================================
// The registry (every machine)
// =====================================================================================================================

bool AAntMatterNet::IsCandidate(const UPrimitiveComponent* C)
{
	const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C);
	if (!S || !S->IsRegistered() || !S->GetStaticMesh() || S->IsA<UInstancedStaticMeshComponent>()) { return false; }
	const AActor* O = S->GetOwner();
	if (!O || O->IsA<APawn>() || O->IsA<AAntFxManager>() || O->IsA<AAntMatterNet>() || O->IsA<AAntMess>()) { return false; }
	const float R = S->Bounds.SphereRadius;
	return R >= 3.f && R <= 5000.f;
}

const FAntMatProps* AAntMatterNet::MatOf(const UPrimitiveComponent* C)
{
	if (!C) { return nullptr; }
	const AActor* O = C->GetOwner();
	if (const AAntMatterThing* T = Cast<AAntMatterThing>(O)) { return AntMaterials::Find(T->MatId()); }
	if (const AKitchenLoot* L = Cast<AKitchenLoot>(O))
	{
		if (C->GetFName() == TEXT("Beam") || C->GetFName() == TEXT("Glow")) { return nullptr; }   // the beam, the glow: not the food
		return AntMaterials::ForName(L->LootName);
	}
	return AntMaterials::ForComponent(C);
}

void AAntMatterNet::ScanRegistry(int32 Budget)
{
	UWorld* W = GetWorld();
	// every few seconds: what mesh components are there that the registry has not seen
	ScanClock -= 1.f;
	if (ScanClock <= 0.f || !bScannedOnce)
	{
		ScanClock = 240.f;   // ~4 s at 60 fps (counted in ticks)
		bScannedOnce = true;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* C = *It;
			if (!C || C->GetWorld() != W || Seen.Contains(C)) { continue; }
			Seen.Add(C);
			if (IsCandidate(C)) { Pending.Add(C); }
		}
		for (auto It = Seen.CreateIterator(); It; ++It) { if (!It->IsValid()) { It.RemoveCurrent(); } }
	}
	// classify the new ones a few at a time (the material lookup reads names)
	int32 N = 0;
	while (Pending.Num() > 0 && N < Budget)
	{
		UPrimitiveComponent* C = Pending.Pop(EAllowShrinking::No).Get();
		++N;
		if (!C || !IsCandidate(C) || RegIndex.Contains(C)) { continue; }
		const FAntMatProps* M = MatOf(C);
		if (!M) { continue; }
		const AActor* O = C->GetOwner();
		FReg R;
		R.C = C;
		R.M = M;
		R.bDyn = C->IsSimulatingPhysics() || O->IsA<AKitchenLoot>() || O->IsA<AAntWreckBody>() || O->IsA<AAntMatterThing>() || (O->GetIsReplicated() && C->Mobility == EComponentMobility::Movable);
		const int32 Idx = Reg.Add(R);
		RegIndex.Add(C, Idx);
		if (R.bDyn) { Dyn.Add(Idx); continue; }
		const FBox B = C->Bounds.GetBox().ExpandBy(200.f);
		const int32 X0 = FMath::FloorToInt32(B.Min.X / CellSize), X1 = FMath::FloorToInt32(B.Max.X / CellSize);
		const int32 Y0 = FMath::FloorToInt32(B.Min.Y / CellSize), Y1 = FMath::FloorToInt32(B.Max.Y / CellSize);
		for (int32 x = X0; x <= X1; ++x) { for (int32 y = Y0; y <= Y1; ++y) { Grid.FindOrAdd(FIntPoint(x, y)).Add(Idx); } }
	}
	if (N > 20 && Pending.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER registry: %d bodies (%d moving), %d grid cells"), Reg.Num(), Dyn.Num(), Grid.Num());
	}
}

void AAntMatterNet::RegisterNow(UPrimitiveComponent* C)
{
	if (!C || RegIndex.Contains(C)) { return; }
	Seen.Add(C);
	Pending.Add(C);
	const float SC = ScanClock;
	const bool bOnce = bScannedOnce;
	ScanClock = 1.e6f;
	bScannedOnce = true;
	ScanRegistry(1);   // (no rescan now; Pending pops from the end: this one)
	ScanClock = SC;
	bScannedOnce = bOnce;
}

void AAntMatterNet::Query(const FVector& P, float Radius, TArray<int32>& Out) const
{
	Out.Reset();
	const float R2 = Radius * Radius;
	auto Try = [&](int32 Idx)
	{
		const FReg& R = Reg[Idx];
		const UPrimitiveComponent* C = R.C.Get();
		if (!C || !C->IsRegistered()) { return; }
		if (C->Bounds.GetBox().ComputeSquaredDistanceToPoint(P) > R2) { return; }
		Out.AddUnique(Idx);
	};
	const int32 X0 = FMath::FloorToInt32((P.X - Radius) / CellSize), X1 = FMath::FloorToInt32((P.X + Radius) / CellSize);
	const int32 Y0 = FMath::FloorToInt32((P.Y - Radius) / CellSize), Y1 = FMath::FloorToInt32((P.Y + Radius) / CellSize);
	for (int32 x = X0; x <= X1; ++x)
	{
		for (int32 y = Y0; y <= Y1; ++y)
		{
			if (const TArray<int32>* L = Grid.Find(FIntPoint(x, y))) { for (int32 Idx : *L) { Try(Idx); } }
		}
	}
	for (int32 Idx : Dyn) { Try(Idx); }
}

UPrimitiveComponent* AAntMatterNet::TakeOver(UPrimitiveComponent* C) const
{
	if (!C || C->IsVisible()) { return C; }
	UWorld* W = GetWorld();
	const int32 Inst = AntWreck::PieceOfComponent(W, C);
	if (Inst < 0) { return C; }
	for (TActorIterator<AAntWreckBody> It(W); It; ++It)
	{
		if (It->Inst == Inst && It->Mesh) { return It->Mesh; }
	}
	return C;
}

// =====================================================================================================================
// Server: the bodies
// =====================================================================================================================

AAntMatterNet::FBody* AAntMatterNet::BodyOf(UPrimitiveComponent* C, bool bCreate)
{
	if (!C) { return nullptr; }
	if (const int32* I = BodyIndex.Find(C)) { return &Bodies[*I]; }
	if (!bCreate || Bodies.Num() >= MaxBodies) { return nullptr; }
	const int32* RI = RegIndex.Find(C);
	const FAntMatProps* M = RI ? Reg[*RI].M : MatOf(C);
	if (!M) { return nullptr; }
	FBody& B = Bodies.AddDefaulted_GetRef();
	B.C = C;
	B.M = M;
	B.Id = NextId++;
	if (NextId == 0) { NextId = 1; }
	BodyIndex.Add(C, Bodies.Num() - 1);
	return &B;
}

void AAntMatterNet::AddServerSpot(FBody& B, const FVector& World, float Radius, uint8 Kind, float Str)
{
	UPrimitiveComponent* C = B.C.Get();
	if (!C || Radius <= 0.f || Str <= 0.f) { return; }
	const FTransform& T = C->GetComponentTransform();
	const FVector L = T.InverseTransformPosition(World);
	const float Sc = FMath::Max(0.001f, (float)T.GetScale3D().GetAbs().GetMax());
	for (FSpotS& S : B.Spots)
	{
		if (S.Kind != Kind) { continue; }
		const float D = (float)FVector::Dist(S.Local, L) * Sc;
		if (D < FMath::Max(S.Radius, Radius) * 0.7f)
		{
			S.Radius = FMath::Max(S.Radius, FMath::Min(D + Radius, S.Radius + Radius * 0.5f));
			S.Str = FMath::Clamp(FMath::Max(S.Str, Str), 0.f, 1.f);
			return;
		}
	}
	if (B.Spots.Num() >= MaxServerSpots)
	{
		// drop the weakest that is not char or damage (those stay: burnt is burnt)
		int32 Weak = -1; float WeakV = 1.e9f;
		for (int32 i = 0; i < B.Spots.Num(); ++i)
		{
			const FSpotS& S = B.Spots[i];
			if (S.Kind == AntState::Char || S.Kind == AntState::Damage) { continue; }
			if (S.Str * S.Radius < WeakV) { WeakV = S.Str * S.Radius; Weak = i; }
		}
		if (Weak < 0) { return; }
		B.Spots.RemoveAtSwap(Weak);
	}
	FSpotS& N = B.Spots.AddDefaulted_GetRef();
	N.Local = L;
	N.Radius = Radius;
	N.Kind = Kind;
	N.Str = FMath::Clamp(Str, 0.f, 1.f);
}

FVector AAntMatterNet::FireWorld(const FBody& B) const
{
	const UPrimitiveComponent* C = B.C.Get();
	return C ? C->GetComponentTransform().TransformPosition(B.FireLocal) : FVector::ZeroVector;
}

bool AAntMatterNet::IsAtRest(const FBody& B) const
{
	if (B.bBurning || B.bMelting || B.Live > 0.f || B.Static > 0.01f || FMath::Abs(B.T - Room) > 3.f || B.Wet > 0.01f || B.Frost > 0.01f || B.HeatIn > Room + 5.f) { return false; }
	for (float S : B.Soak) { if (S > 0.01f) { return false; } }
	for (const FSpotS& S : B.Spots) { if (S.Kind != AntState::Char && S.Kind != AntState::Damage && S.Str > 0.02f) { return false; } }
	return true;
}

bool AAntMatterNet::Ignite(UPrimitiveComponent* C, const FVector& At, const TCHAR* Why)
{
	C = TakeOver(C);
	FBody* B = BodyOf(C, true);
	if (!B || !Burns(B->M) || B->bBurning || B->bGone || B->Fuel <= 0.05f) { return false; }
	if (B->Wet > 0.3f || LocalClock < B->NoFireUntil) { return false; }   // soaked things do not catch; just put out: not at once again
	if (NumBurning >= MaxBurning)
	{
		static float LogAt = -10.f;
		if (LocalClock > LogAt) { LogAt = LocalClock + 2.f; UE_LOG(LogTemp, Display, TEXT("ANTMATTER fire cap (%d): %s does not catch"), MaxBurning, *GetNameSafe(C)); }
		return false;
	}
	const FBox Box = C->Bounds.GetBox();
	const FVector P = Box.GetClosestPointTo(At);
	B->bBurning = true;
	B->FireLocal = C->GetComponentTransform().InverseTransformPosition(P);
	B->FireR = FMath::Max(B->FireR, 10.f);
	B->Flame = 0.3f;
	B->BurnFor = 0.f;
	B->T = FMath::Max(B->T, B->M->IgniteAt + 30.f);
	++NumBurning;
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER FIRE %s/%s [%s] catches at %s (%s)"), *GetNameSafe(C->GetOwner()), *C->GetName(), *B->M->Id, *P.ToCompactString(), Why);
	AntAudio::Everyone(this, EAntSfx::Sizzle, P, 0.9f);
	if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->MakeNoise(P, 0.3f, AntNoise::Crash, this, nullptr); }
	return true;
}

void AAntMatterNet::HeatBody(UPrimitiveComponent* C, const FVector& At, float TempC, float Dt)
{
	C = TakeOver(C);
	FBody* B = BodyOf(C, TempC > Room + 15.f || TempC < Room - 15.f);
	if (!B) { return; }
	if (TempC > B->HeatIn) { B->HeatIn = TempC; B->HeatFrom = At; }
}

void AAntMatterNet::WetBody(UPrimitiveComponent* C, const FVector& At, float Radius, float Amount, uint8 Liquid)
{
	C = TakeOver(C);
	FBody* B = BodyOf(C, true);
	if (!B || B->bGone) { return; }
	const FAntMatProps& M = *B->M;
	const uint8 K = KindOfLiquid(Liquid);
	const float Take = FMath::Clamp(Amount * (0.35f + 0.65f * M.WetSpeed) * (M.Absorb > 0.5f ? 1.f : 0.7f), 0.f, 1.f);
	const bool bAll = Radius >= C->Bounds.SphereRadius * 0.9f;
	const float Was = B->Wet;
	if (K == AntState::Wet)
	{
		if (bAll) { B->Wet = FMath::Min(1.f, B->Wet + Take); }
		else { B->Wet = FMath::Min(1.f, B->Wet + Take * FMath::Clamp(Radius / FMath::Max(1.f, (float)C->Bounds.SphereRadius), 0.f, 1.f) * 0.6f); }
	}
	else if (SoakSlot(K) >= 0)
	{
		float& S = B->Soak[SoakSlot(K)];
		S = FMath::Min(1.f, S + (bAll ? Take : Take * 0.4f));
	}
	if (!bAll)
	{
		// a part of it is wet: the spot says where; the whole of it takes only a little (half a wet door does not put out the other half)
		AddServerSpot(*B, At, Radius, K, FMath::Min(1.f, 0.7f + Take));   // (a film of water on any surface shows, absorbent or not)
		if (K == AntState::Wet) { B->Wet = FMath::Min(1.f, Was + Take * FMath::Square(FMath::Clamp(Radius / FMath::Max(1.f, (float)C->Bounds.SphereRadius), 0.f, 1.f)) * 0.25f); }
	}
	// water on fire: the patch it lands on goes out (the whole fire if it reaches the front)
	if (B->bBurning && K == AntState::Wet && (B->Wet > 0.3f || FVector::Dist(FireWorld(*B), At) < Radius + B->FireR * 0.6f)) { Extinguish(*B, TEXT("залили водой")); }
	B->WetLastTick = FMath::Min(B->WetLastTick, Was);
}

void AAntMatterNet::Extinguish(FBody& B, const TCHAR* Why)
{
	if (!B.bBurning) { return; }
	UPrimitiveComponent* C = B.C.Get();
	const FVector At = FireWorld(B);
	B.bBurning = false;
	B.NoFireUntil = LocalClock + 4.f;
	NumBurning = FMath::Max(0, NumBurning - 1);
	// what burnt stays black: the patch becomes a char spot
	const float Cover = C ? FMath::Clamp(B.FireR / FMath::Max(20.f, (float)C->Bounds.SphereRadius), 0.f, 1.f) : 1.f;
	if (C && Cover < 0.95f) { AddServerSpot(B, At, B.FireR * 1.05f, AntState::Char, 1.f); }
	else { B.Char = FMath::Max(B.Char, 0.9f); }
	B.Flame = 0.f;
	B.T = FMath::Min(B.T, 95.f);
	const float Size = 60.f + B.FireR;
	MulticastSteam(At, Size);
	if (AAntReactNet* RN = AAntReactNet::Get(GetWorld())) { RN->AddCloud(AntCloudKind::Steam, At + FVector(0.f, 0.f, 80.f), 160.f + B.FireR, 5.f, FVector(0.f, 0.f, 40.f), FVector::ZeroVector, 0.f, TEXT("matter")); }
	AntAudio::Everyone(this, EAntSfx::Sizzle, At, 1.4f);
	AntAudio::Everyone(this, EAntSfx::Hiss, At, 0.8f);
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER OUT %s/%s [%s] at %s (%s): burnt patch %.0f uu, char %.2f, steam"), C ? *GetNameSafe(C->GetOwner()) : TEXT("?"), C ? *C->GetName() : TEXT("?"), B.M ? *B.M->Id : TEXT("?"), *At.ToCompactString(), Why, B.FireR, B.Char);
}

void AAntMatterNet::BurnAway(FBody& B)
{
	UPrimitiveComponent* C = B.C.Get();
	if (B.bBurning) { B.bBurning = false; NumBurning = FMath::Max(0, NumBurning - 1); }
	B.bGone = true;
	B.Char = 1.f;
	B.Flame = 0.f;
	if (!C) { return; }
	const FBox Box = C->Bounds.GetBox();
	const FVector Ctr = Box.GetCenter();
	// the ash it leaves where it lay (a dark smear: the mess has no grey ash kind, mud reads right at this size)
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterAsh), false, C->GetOwner());
	FVector Floor(Ctr.X, Ctr.Y, Box.Min.Z);
	if (GetWorld()->LineTraceSingleByChannel(H, Ctr, Ctr - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Q)) { Floor = H.ImpactPoint; }
	AntMess::StainAt(this, MessKind::Mud, Floor, FVector::UpVector, FMath::Clamp((float)C->Bounds.SphereRadius * 0.9f, 40.f, 600.f), B.Id & 0xff);
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s/%s [%s] burnt away to ash"), *GetNameSafe(C->GetOwner()), *C->GetName(), B.M ? *B.M->Id : TEXT("?"));
	if (AKitchenLoot* L = Cast<AKitchenLoot>(C->GetOwner())) { L->SetLifeSpan(0.5f); }
}

void AAntMatterNet::HurtAround(const FVector& At, float Radius, float Dps, float Dt, const TCHAR* Why)
{
	if (Dps <= 0.f) { return; }
	AAntReactNet* RN = AAntReactNet::Get(GetWorld());
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead) { continue; }
		const float D = FVector::Dist(A->GetActorLocation(), At);
		if (D > Radius) { continue; }
		A->HurtBy(Dps * Dt * (1.f - 0.5f * D / Radius), Why);
		if (RN && !RN->HasMark(A, AntMark::Burnt)) { A->Toast(TEXT("Горячо! Огонь обжигает")); }
		if (RN) { RN->Mark(A, AntMark::Burnt, 6.f); }
	}
}

void AAntMatterNet::ProbeWater(FBody& B, float Dt, bool bWasRest)
{
	UPrimitiveComponent* C = B.C.Get();
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	if (!C || !Fluid) { return; }
	const FBox Box = C->Bounds.GetBox();
	const FVector Bottom(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z + 1.f);
	AntFluid::FSample S;
	if (!Fluid->SampleAt(Bottom + FVector(0.f, 0.f, 20.f), S) || S.Total < 0.3f || S.Surface < Bottom.Z) { return; }
	const float Sub = FMath::Min(S.Surface - (float)Box.Min.Z, (float)Box.GetSize().Z);
	if (Sub <= 0.f) { return; }
	uint8 Liquid = AntFluid::Water;
	if (S.Depth[AntFluid::LOil] > S.Depth[AntFluid::LWater] && S.Depth[AntFluid::LOil] > S.Depth[AntFluid::LHoney]) { Liquid = AntFluid::Oil; }
	else if (S.Depth[AntFluid::LHoney] > S.Depth[AntFluid::LWater]) { Liquid = AntFluid::Honey; }
	else if (S.Dye > 0.5f) { Liquid = AntFluid::Cola; }
	const FAntMatProps& M = *B.M;
	const float Height = FMath::Max(1.f, (float)Box.GetSize().Z);
	const float Frac = FMath::Clamp(Sub / Height, 0.f, 1.f);
	const uint8 K = KindOfLiquid(Liquid);
	// what is under the surface is wet; absorbent things wick it up above the waterline
	const float Wick = M.Absorb > 0.5f ? FMath::Clamp(M.Absorb * 0.08f, 0.f, 0.8f) : 0.f;
	const float Reach = FMath::Clamp(Frac + Wick * FMath::Min(1.f, B.SoakedFor / 10.f), 0.f, 1.f);
	const float Rate = M.WetSpeed * Dt * (0.5f + Frac);
	if (K == AntState::Wet) { B.Wet = FMath::Min(1.f, FMath::Max(B.Wet, FMath::Min(Reach, B.Wet + Rate))); }
	else if (SoakSlot(K) >= 0) { float& So = B.Soak[SoakSlot(K)]; So = FMath::Min(1.f, FMath::Max(So, FMath::Min(Reach, So + Rate))); }
	if (Reach < 0.95f)
	{
		const FVector Contact(Bottom.X, Bottom.Y, FMath::Min(S.Surface, (float)Box.Max.Z));
		AddServerSpot(B, Contact - FVector(0.f, 0.f, Sub * 0.5f), FMath::Max(20.f, Reach * Height * 1.1f + Box.GetExtent().GetMax() * 0.35f), K, 1.f);
	}
	if (bWasRest) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s/%s [%s] in %s (%.0f uu deep)"), *GetNameSafe(C->GetOwner()), *C->GetName(), *M.Id, AntFluid::LiquidName((AntFluid::ELiquid)Liquid), Sub); }
	// a fire with its foot in water goes out
	if (B.bBurning && K == AntState::Wet && FireWorld(B).Z < S.Surface + 15.f) { Extinguish(B, TEXT("в воде")); }
}

// ---------------------------------------------------------------------------------------------------------------------
// one body, one step
// ---------------------------------------------------------------------------------------------------------------------

void AAntMatterNet::StepBody(FBody& B, float Dt)
{
	UWorld* W = GetWorld();
	UPrimitiveComponent* C = B.C.Get();
	if (!C || !B.M) { return; }
	const FAntMatProps& M = *B.M;
	const FBoxSphereBounds Bd = C->Bounds;
	const FVector Ctr = Bd.Origin;
	const bool bWasRest = IsAtRest(B);

	// ---- heat: the flat's hot things (AntReact), what burns next to it, the dev heat sources
	float Env = AntReact::HeatAt(W, Ctr);
	if (Env > Room + 1.f && B.HeatIn < Env) { B.HeatIn = Env; B.HeatFrom = Ctr - FVector(0.f, 0.f, Bd.BoxExtent.Z); }
	for (const FHeatSrc& H : HeatSrcs)
	{
		if (Bd.GetBox().ComputeSquaredDistanceToPoint(H.At) < H.Radius * H.Radius && H.Temp > B.HeatIn) { B.HeatIn = H.Temp; B.HeatFrom = Bd.GetBox().GetClosestPointTo(H.At); }
	}
	const float Target = B.bBurning ? FMath::Max(650.f, B.HeatIn) : (B.HeatIn > -999.f ? (Env < Room ? Env : FMath::Max(Env, B.HeatIn)) : Env);
	const float K = FMath::Clamp(M.HeatTake, 0.05f, 1.f) * (B.bBurning ? 1.5f : 0.6f);
	B.T += (Target - B.T) * (1.f - FMath::Exp(-K * Dt));
	// water boils off before it gets any hotter
	if (B.Wet > 0.02f && B.T > 100.f)
	{
		B.Wet = FMath::Max(0.f, B.Wet - Dt * (B.T - 100.f) / 500.f);
		B.T = 100.f + (B.T - 100.f) * (1.f - FMath::Min(1.f, B.Wet * 3.f));
	}
	// ---- drying in room air (faster when warm); spots dry with it; frost melts into water
	const float DryK = M.DrySpeed * (1.f + FMath::Max(0.f, B.T - Room) / 30.f) * Dt;
	B.Wet = FMath::Max(0.f, B.Wet - DryK);
	B.Soak[2] = FMath::Max(0.f, B.Soak[2] - DryK * 0.3f);   // cola dries to a sticky stain slowly
	for (int32 i = B.Spots.Num() - 1; i >= 0; --i)
	{
		FSpotS& S = B.Spots[i];
		if (S.Kind == AntState::Wet) { S.Str -= DryK * 1.2f; }
		else if (S.Kind == AntState::Cola) { S.Str -= DryK * 0.3f; }
		else if (S.Kind == AntState::Frost) { S.Str -= (B.T > 1.f ? Dt * 0.02f * B.T / Room : 0.f); }
		else if (S.Kind == AntState::Heat) { S.Str = FMath::Min(S.Str, FMath::Clamp((B.T - 100.f) / 700.f + 0.15f, 0.f, 1.f)) - Dt * 0.05f; }
		if (S.Str <= 0.02f) { B.Spots.RemoveAtSwap(i); }
	}
	if (Env < -2.f) { B.Frost = FMath::Min(1.f, B.Frost + Dt * 0.04f); }
	else if (B.Frost > 0.f && B.T > 1.f)
	{
		const float Melt = FMath::Min(B.Frost, Dt * 0.02f * B.T / Room);
		B.Frost -= Melt;
		B.Wet = FMath::Min(1.f, B.Wet + Melt * 0.8f);
	}
	// ---- a heated patch where the heat comes in (metal glows there first)
	if (B.HeatIn > B.T + 60.f && B.HeatIn > 250.f && M.Conduct > 0.5f)
	{
		AddServerSpot(B, B.HeatFrom, FMath::Clamp((float)Bd.SphereRadius * 0.45f, 30.f, 300.f), AntState::Heat, FMath::Clamp((B.HeatIn - 100.f) / 700.f, 0.f, 1.f));
	}

	// ---- wood, wool, leather scorch where strong heat reaches them, flame or not (pyrolysis): a char patch that spreads
	if (Is(B.M, TEXT("char")) && B.HeatIn > 250.f && !B.bBurning && !B.HeatFrom.IsZero())
	{
		const FTransform& Tr = C->GetComponentTransform();
		FSpotS* Near = nullptr;
		for (FSpotS& S : B.Spots) { if (S.Kind == AntState::Char && FVector::Dist(Tr.TransformPosition(S.Local), B.HeatFrom) < S.Radius + 40.f) { Near = &S; break; } }
		if (!Near) { AddServerSpot(B, B.HeatFrom, 30.f, AntState::Char, 0.5f); }
		else
		{
			Near->Radius = FMath::Min((float)Bd.SphereRadius * 1.2f, Near->Radius + Dt * (B.HeatIn - 250.f) / 25.f);
			Near->Str = FMath::Min(1.f, Near->Str + Dt * 0.06f);
		}
	}

	// ---- water: absorbent things grow heavy, wet cardboard tears, soaked bread falls apart, hot glass cracks
	if (M.Absorb > 0.5f && C->IsSimulatingPhysics())
	{
		if (B.BaseMass < 0.f) { B.BaseMass = C->GetMass(); }
		const float Want = B.BaseMass * (1.f + FMath::Min(M.Absorb, 6.f) * B.Wet * 0.5f);
		if (FMath::Abs(C->GetMass() - Want) > B.BaseMass * 0.05f) { C->SetMassOverrideInKg(NAME_None, Want, true); }
	}
	if (B.Wet > 0.6f) { B.SoakedFor += Dt; } else { B.SoakedFor = FMath::Max(0.f, B.SoakedFor - Dt); }
	if (!B.bTorn && M.Tear * (1.f - 0.85f * B.Wet) < 0.12f && B.SoakedFor > 8.f && M.Absorb > 0.5f)
	{
		B.bTorn = true;
		B.Dmg = FMath::Max(B.Dmg, 0.7f);
		AntWreck::Hit(W, Ctr, Bd.SphereRadius * 0.6f, FVector(0.f, 0.f, -0.5f), 80.f, this, TEXT("размок и порвался"));
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s/%s [%s] soaked through and tore (tear %.2f wet %.2f)"), *GetNameSafe(C->GetOwner()), *C->GetName(), *M.Id, M.Tear, B.Wet);
	}
	if (!B.bCrumbled && M.Crumbly > 0.5f && B.Wet > 0.7f && B.SoakedFor > 6.f)
	{
		B.bCrumbled = true;
		B.Dmg = FMath::Max(B.Dmg, 0.8f);
		AKitchenLoot* L = Cast<AKitchenLoot>(C->GetOwner());
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s [%s] soaked and fell apart"), *GetNameSafe(C->GetOwner()), *M.Id);
		if (L && L->CanBreak()) { L->Shatter(Ctr); return; }
	}
	if (!B.bCracked && (M.Breaks == TEXT("glass") || M.Breaks == TEXT("ceramic")) && B.T > 120.f && B.Wet - B.WetLastTick > 0.08f)
	{
		B.bCracked = true;
		B.Dmg = FMath::Max(B.Dmg, 0.65f);
		AddServerSpot(B, B.HeatFrom.IsZero() ? Ctr : B.HeatFrom, FMath::Max(40.f, (float)Bd.SphereRadius * 0.6f), AntState::Damage, 1.f);
		AntWreck::Hit(W, Ctr, FMath::Min(80.f, (float)Bd.SphereRadius), FVector::ZeroVector, 60.f, this, TEXT("термоудар"));
		AntAudio::Everyone(this, M.Breaks == TEXT("glass") ? EAntSfx::CrashGlass : EAntSfx::CrashCeramic, Ctr, 0.5f);
		MulticastSteam(Ctr, 80.f);
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s/%s [%s] cracked: hot (%.0f C) and suddenly wet (thermal shock)"), *GetNameSafe(C->GetOwner()), *C->GetName(), *M.Id, B.T);
	}
	B.WetLastTick = B.Wet;

	// ---- plastic softens, sags and drips with acrid smoke; food toasts before it burns
	if (Is(B.M, TEXT("melt")) && M.MeltAt > -999.f && B.T > M.MeltAt && !B.bGone)
	{
		if (!B.bMelting) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER %s/%s [%s] melts (%.0f C > %.0f)"), *GetNameSafe(C->GetOwner()), *C->GetName(), *M.Id, B.T, M.MeltAt); }
		B.bMelting = true;
		B.MeltFor += Dt;
		B.Dmg = FMath::Max(B.Dmg, FMath::Min(0.85f, B.MeltFor / 25.f));
		B.Fuel = FMath::Max(0.05f, B.Fuel - Dt * 0.01f);
		if (LocalClock >= B.NextSmoke)
		{
			B.NextSmoke = LocalClock + 6.f;
			if (AAntReactNet* RN = AAntReactNet::Get(W)) { RN->AddCloud(AntCloudKind::Smoke, Ctr + FVector(0.f, 0.f, 140.f), 260.f + Bd.SphereRadius, 8.f, FVector(0.f, 0.f, 25.f), FVector::ZeroVector, 0.f, TEXT("matter")); }
		}
	}
	else { B.bMelting = false; }
	if (Is(B.M, TEXT("toast")) && B.T > 120.f && !B.bBurning) { B.Char = FMath::Max(B.Char, FMath::Min(0.35f, B.Char + Dt * (B.T - 120.f) / 900.f)); }

	// ---- catching fire
	const float Ign = M.IgniteAt - 60.f * B.Soak[0];   // oil-soaked things light easier
	if (!B.bBurning && !B.bGone && Burns(B.M) && B.T >= Ign && B.Wet < 0.25f && B.Fuel > 0.05f)
	{
		Ignite(C, B.HeatFrom.IsZero() ? Ctr : B.HeatFrom, TEXT("нагрелось до воспламенения"));
	}
	StepFire(B, Dt);

	// ---- live: the current runs through it, sparks jump from its edges to what burns
	if (B.Live > 0.f)
	{
		B.Live -= Dt;
		if (LocalClock >= B.NextSpark)
		{
			B.NextSpark = LocalClock + FMath::FRandRange(0.3f, 0.7f);
			TArray<int32> Near;
			Query(Ctr, Bd.SphereRadius + 140.f, Near);
			bool bSparked = false;
			for (int32 Idx : Near)
			{
				UPrimitiveComponent* O = Reg[Idx].C.Get();
				const FAntMatProps* OM = Reg[Idx].M;
				if (!O || O == C || !Burns(OM) || OM->IgniteAt > 270.f) { continue; }
				// the spark jumps across the gap between the live edge and the paper
				FVector P = Bd.GetBox().GetClosestPointTo(O->Bounds.Origin);
				if (Bd.BoxExtent.GetMax() > 300.f)
				{
					// a big live thing (a cabinet run, a rail): its box takes in the shelves above it; the spark leaves its real surface
					FVector OnC;
					if (C->GetClosestPointOnCollision(O->Bounds.Origin, OnC) < 0.f) { continue; }
					P = OnC;
				}
				const FVector Q = O->Bounds.GetBox().GetClosestPointTo(P);
				if (FVector::Dist(P, Q) > 120.f) { continue; }
				Spark((P + Q) * 0.5f, (Q - P).GetSafeNormal(), TEXT("искра с металла под током"));
				bSparked = true;
				break;
			}
			if (!bSparked)
			{
				const FVector E = Ctr + FVector(FMath::FRandRange(-1.f, 1.f) * Bd.BoxExtent.X, FMath::FRandRange(-1.f, 1.f) * Bd.BoxExtent.Y, Bd.BoxExtent.Z);
				MulticastSpark(E, FMath::VRand());
			}
		}
		if (LocalClock >= B.NextZap)
		{
			B.NextZap = LocalClock + 1.2f;
			AAntReactNet* RN = AAntReactNet::Get(W);
			for (TActorIterator<AAntCharacter> It(W); It && RN; ++It)
			{
				if (!It->bDead && Bd.GetBox().ExpandBy(25.f).IsInside(It->GetActorLocation())) { RN->Zap(*It, 20.f, TEXT("тронул металл под током")); }
			}
		}
	}
	B.HeatIn = -1000.f;   // gathered again next step
	B.Quiet = IsAtRest(B) ? B.Quiet + Dt : 0.f;
	if (bWasRest && !IsAtRest(B)) { ProbeClock = FMath::Min(ProbeClock, 0.f); }
}

void AAntMatterNet::StepFire(FBody& B, float Dt)
{
	if (!B.bBurning) { return; }
	UWorld* W = GetWorld();
	UPrimitiveComponent* C = B.C.Get();
	if (!C) { return; }
	const FAntMatProps& M = *B.M;
	const FBoxSphereBounds Bd = C->Bounds;
	const FTransform& T = C->GetComponentTransform();
	B.BurnFor += Dt;
	const float Size = FMath::Max(20.f, (float)Bd.SphereRadius);
	// the front runs over it (upwards ~3x faster: the centre of the burnt patch climbs); wet slows it
	const float Rate = FMath::Max(M.BurnRate, 0.05f) * CmToUu * (1.f - FMath::Min(0.9f, B.Wet * 2.f)) * (0.4f + 0.6f * B.Flame);
	const float MaxR = Size * 1.15f;
	FVector Front = FireWorld(B);
	// a wet spot in its way stops it there
	bool bBlocked = false;
	for (const FSpotS& S : B.Spots)
	{
		if (S.Kind != AntState::Wet || S.Str < 0.35f) { continue; }
		const FVector SW = T.TransformPosition(S.Local);
		if (FVector::Dist(SW, Front) < S.Radius + B.FireR * 0.9f) { bBlocked = true; break; }
	}
	if (!bBlocked && B.FireR < MaxR)
	{
		const float dR = Rate * Dt;
		B.FireR = FMath::Min(MaxR, B.FireR + dR);
		const FVector UpL = T.InverseTransformVectorNoScale(FVector::UpVector) / FMath::Max(0.001f, (float)T.GetScale3D().GetAbs().GetMax());
		B.FireLocal += UpL * dR * 0.6f;
		// stay on the thing
		const FBox LB = C->CalcBounds(FTransform::Identity).GetBox();
		B.FireLocal = LB.GetClosestPointTo(B.FireLocal);
		Front = FireWorld(B);
	}
	// flame: grows while there is fuel, dies down as it runs out or on a wet patch
	const float Cover = FMath::Clamp(B.FireR / MaxR, 0.f, 1.f);
	if (bBlocked) { B.Flame = FMath::Max(0.f, B.Flame - Dt * 0.35f); }
	else { B.Flame = FMath::Min(1.f, B.Flame + Dt * 0.5f) * FMath::Clamp(B.Fuel * 4.f, 0.f, 1.f); }
	B.Fuel = FMath::Max(0.f, B.Fuel - Dt * Rate / FMath::Max(MaxR, 40.f) * (Cover >= 1.f ? 0.9f : 0.5f) * (Is(B.M, TEXT("char")) ? 0.25f : 1.f));
	B.Char = FMath::Max(B.Char, FMath::Pow(Cover, 1.5f) * 0.95f);
	if (Is(B.M, TEXT("char"))) { B.Dmg = FMath::Max(B.Dmg, B.Char * 0.5f); }
	if (B.Wet > 0.35f) { Extinguish(B, TEXT("промокло")); return; }
	if (B.Flame <= 0.05f && B.BurnFor > 1.f) { Extinguish(B, bBlocked ? TEXT("упёрлось в мокрое") : TEXT("догорело")); return; }
	if (B.Fuel <= 0.f)
	{
		if (Is(B.M, TEXT("ash"))) { BurnAway(B); }
		else { Extinguish(B, TEXT("выгорело")); B.Char = 1.f; }
		return;
	}
	// ---- it heats what is near it, more so what is above it
	const float Reach = 60.f + B.FireR * 0.6f + 140.f * B.Flame;
	const FVector Fc = Front + FVector(0.f, 0.f, B.FireR * 0.3f + 40.f * B.Flame);
	TArray<int32> Near;
	Query(Fc, Reach * 1.7f, Near);
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* O = Reg[Idx].C.Get();
		if (!O || O == C) { continue; }
		const FVector Q = O->Bounds.GetBox().GetClosestPointTo(Fc);
		float D = (float)FVector::Dist(Q, Fc);
		if (Q.Z > Fc.Z - 10.f) { D /= 1.7f; }   // flames go up
		if (D > Reach) { continue; }
		const float Kk = 1.f - D / Reach;
		HeatBody(O, Q, 220.f + 580.f * B.Flame * Kk, Dt);
	}
	// ---- burning oil under it, a flour cloud round it
	if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(this))
	{
		AntFluid::FSample S;
		if (Fluid->SampleAt(Front, S) && S.Depth[AntFluid::LOil] > 0.03f && Front.Z - S.Surface < 250.f && Front.Z - S.Surface > -30.f)
		{
			LightPool(FVector(Front.X, Front.Y, S.Surface), TEXT("огонь над масляной плёнкой"));
		}
	}
	if (AAntReactNet* RN = AAntReactNet::Get(W))
	{
		FVector CC; float CR = 0.f; uint8 Id = 0;
		if (RN->CloudThickness(AntCloudKind::Flour, Fc, &Id, &CC, &CR) > 0.12f) { Bang(CC, CR, TEXT("огонь в мучном облаке")); }
		// a big fire smokes the room
		if (LocalClock >= B.NextSmoke && B.FireR > 90.f && B.Flame > 0.5f)
		{
			B.NextSmoke = LocalClock + 7.f;
			RN->AddCloud(AntCloudKind::Smoke, Fc + FVector(0.f, 0.f, 220.f), 220.f + B.FireR, 9.f, FVector(0.f, 0.f, 30.f), FVector::ZeroVector, 0.f, TEXT("matter"));
			if (AAntKitchenGameMode* GM = GMOf(W)) { GM->MakeNoise(Fc, 0.45f, AntNoise::Crash, this, nullptr); }
		}
	}
	// ---- the ants in the flames
	HurtAround(Fc, Reach * 0.6f, 14.f * B.Flame, Dt, TEXT("обгорел в огне"));
	// the ice by it melts fast
	for (FAntMatterIce& I : Ices) { if (FVector::Dist2D(FVector(I.Center), Fc) < I.Cell * AntMatter::IceN * 0.7f + Reach) { I.Left = (uint8)FMath::Max(0, (int32)I.Left - 2); } }
}

// ---------------------------------------------------------------------------------------------------------------------
// the world step
// ---------------------------------------------------------------------------------------------------------------------

void AAntMatterNet::ServerStep(float Dt)
{
	if (Bodies.Max() < MaxBodies) { Bodies.Reserve(MaxBodies); }   // (bodies are made while the step walks them: never reallocate)
	const float Now = LocalClock;
	HeatSrcs.RemoveAll([Now](const FHeatSrc& H) { return Now > H.Until; });
	// ---- water reaching things that were at rest: a few registry entries a step, round robin
	if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(this))
	{
		const int32 N = FMath::Min(160, Reg.Num());
		for (int32 i = 0; i < N; ++i)
		{
			ProbeCursor = (ProbeCursor + 1) % FMath::Max(1, Reg.Num());
			const FReg& R = Reg[ProbeCursor];
			UPrimitiveComponent* C = R.C.Get();
			if (!C || BodyIndex.Contains(C)) { continue; }
			const FBox Box = C->Bounds.GetBox();
			if (Fluid->DepthIn(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z + 20.f), 15.f) < 0.3f) { continue; }   // (in it, not over it)
			if (FBody* B = BodyOf(C, true)) { ProbeWater(*B, Dt, true); }
		}
	}
	// ---- the dev heat sources reach the things at rest too
	for (const FHeatSrc& H : HeatSrcs)
	{
		TArray<int32> Near;
		Query(H.At, H.Radius, Near);
		for (int32 Idx : Near) { if (UPrimitiveComponent* C = Reg[Idx].C.Get()) { HeatBody(C, Reg[Idx].C->Bounds.GetBox().GetClosestPointTo(H.At), H.Temp, Dt); } }
	}
	// ---- the hot things of the flat reach the things on them (a pot on the burner, a towel on the radiator): slow round robin
	{
		const int32 N = FMath::Min(60, Reg.Num());
		for (int32 i = 0; i < N; ++i)
		{
			ScanCursor = (ScanCursor + 1) % FMath::Max(1, Reg.Num());
			UPrimitiveComponent* C = Reg[ScanCursor].C.Get();
			if (!C) { continue; }
			const float H = AntReact::HeatAt(GetWorld(), C->Bounds.Origin);
			if (H > Room + 25.f || H < -2.f)
			{
				if (FBody* B = BodyOf(C, true)) { if (H > B->HeatIn) { B->HeatIn = H; B->HeatFrom = C->Bounds.Origin; } B->Quiet = 0.f; }
			}
		}
	}
	// ---- the bodies
	NumBurning = 0;
	NumActive = 0;
	const int32 Count = Bodies.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		FBody& B = Bodies[i];
		UPrimitiveComponent* C = B.C.Get();
		if (!C || B.bGone) { continue; }
		// a builder piece a wreck body took over: the state moves onto the body's mesh
		if (!C->IsVisible())
		{
			UPrimitiveComponent* N = TakeOver(C);
			if (N != C && !BodyIndex.Contains(N)) { BodyIndex.Remove(C); BodyIndex.Add(N, i); B.C = N; C = N; }
		}
		ProbeWater(B, Dt, false);
		if (B.Quiet > 2.f && !B.bBurning && IsAtRest(B)) { B.HeatIn = -1000.f; continue; }   // at rest (maybe charred): nothing to step
		if (B.bBurning) { ++NumBurning; }
		++NumActive;
		StepBody(B, Dt);
	}
	StepPools(Dt);
	StepIce(Dt);
	StaticClock += Dt;
	if (StaticClock >= 0.25f) { StepStatic(StaticClock); StaticClock = 0.f; }
	// ---- forget the bodies that are back at rest and show nothing (char and damage stay as records: burnt stays burnt)
	for (int32 i = Bodies.Num() - 1; i >= 0; --i)
	{
		FBody& B = Bodies[i];
		const bool bMarked = B.Char > 0.01f || B.Dmg > 0.01f || B.bGone || B.Spots.Num() > 0;
		if (B.C.IsValid() && (bMarked || B.Quiet < 4.f)) { continue; }
		BodyIndex.Remove(B.C);
		Bodies.RemoveAtSwap(i);
		if (Bodies.IsValidIndex(i)) { BodyIndex.Add(Bodies[i].C, i); }
	}
	// ---- flour in the air near a fire (a pool, a fireball) goes up
	FlourClock += Dt;
	if (FlourClock > 0.25f)
	{
		FlourClock = 0.f;
		if (AAntReactNet* RN = AAntReactNet::Get(GetWorld()))
		{
			for (const FAntMatterPool& P : Pools)
			{
				FVector CC; float CR = 0.f; uint8 Id = 0;
				if (RN->CloudThickness(AntCloudKind::Flour, FVector(P.Center) + FVector(0.f, 0.f, 60.f), &Id, &CC, &CR) > 0.1f) { Bang(CC, CR, TEXT("горящее масло под мучным облаком")); break; }
			}
		}
	}
}

void AAntMatterNet::StepPools(float Dt)
{
	UWorld* W = GetWorld();
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	const float Now = NetNowOf(W);
	for (int32 i = Pools.Num() - 1; i >= 0; --i)
	{
		FAntMatterPool& P = Pools[i];
		const float Age = Now - P.Born;
		if (P.Kind == 1)
		{
			// the fireball of a flour cloud: a second and a half
			P.Power = FMath::Max(0.f, 1.f - Age / 1.5f);
			if (Age > 1.5f) { Pools.RemoveAt(i); }
			continue;
		}
		float Oil = 0.f, Water = 0.f;
		if (Fluid)
		{
			AntFluid::FSample S;
			if (Fluid->SampleAt(FVector(P.Center) + FVector(0.f, 0.f, 40.f), S)) { Oil = S.Depth[AntFluid::LOil]; Water = S.Depth[AntFluid::LWater]; }
			// it spreads where the film goes
			if (P.Radius < 900.f)
			{
				int32 Wet = 0;
				for (int32 d = 0; d < 8; ++d)
				{
					const float A = d * PI / 4.f;
					const FVector Q = FVector(P.Center) + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (P.Radius + 40.f) + FVector(0.f, 0.f, 40.f);
					AntFluid::FSample S2;
					if (Fluid->SampleAt(Q, S2) && S2.Depth[AntFluid::LOil] > 0.02f) { ++Wet; }
				}
				P.Radius += Dt * 70.f * Wet;
			}
			Fluid->DrainLayer(FVector(P.Center), P.Radius, AntFluid::LOil, 0.08f * Dt);   // it burns the oil up
		}
		// water thrown on burning oil: it flares up (it does not put it out)
		if (Water > 1.5f && !Flared.Contains(P.Id))
		{
			Flared.Add(P.Id);
			P.Radius *= 1.35f;
			P.Power = 1.f;
			MulticastFlash(P.Center, P.Radius * 1.6f, 2);
			HurtAround(FVector(P.Center), P.Radius * 1.5f, 120.f, 0.25f, TEXT("вспышка масла от воды"));
			if (AAntKitchenGameMode* GM = GMOf(W)) { GM->Broadcast(TEXT("Вода на горящее масло — вспышка!"), FLinearColor(1.f, 0.5f, 0.2f)); GM->MakeNoise(FVector(P.Center), 0.8f, AntNoise::Crash, this, nullptr); }
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER oil fire %d: water on it - it flares up to %.0f uu"), P.Id, P.Radius);
		}
		if (Oil < 0.015f && Age > 2.f) { P.Power -= Dt * 0.4f; }
		if (Age > 45.f) { P.Power -= Dt * 0.2f; }
		if (P.Power <= 0.f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER oil fire %d out after %.0f s (r %.0f)"), P.Id, Age, P.Radius);
			Pools.RemoveAt(i);
			continue;
		}
		// what is in it and over it gets the heat
		TArray<int32> Near;
		Query(FVector(P.Center) + FVector(0.f, 0.f, 80.f), P.Radius + 160.f, Near);
		for (int32 Idx : Near)
		{
			UPrimitiveComponent* O = Reg[Idx].C.Get();
			if (!O) { continue; }
			const FVector Q = O->Bounds.GetBox().GetClosestPointTo(FVector(P.Center));
			const float D = FMath::Max(0.f, (float)FVector::Dist2D(Q, FVector(P.Center)) - P.Radius);
			const float Kk = FMath::Clamp(1.f - D / 160.f, 0.f, 1.f) * FMath::Clamp(1.f - ((float)Q.Z - P.Center.Z) / 600.f, 0.f, 1.f);
			if (Kk > 0.f) { HeatBody(O, Q, 300.f + 500.f * P.Power * Kk, Dt); }
		}
		HurtAround(FVector(P.Center), P.Radius, 22.f * P.Power, Dt, TEXT("сгорел в горящем масле"));
		if (FMath::Fmod(Age, 6.f) < Dt)
		{
			if (AAntReactNet* RN = AAntReactNet::Get(W)) { RN->AddCloud(AntCloudKind::Smoke, FVector(P.Center) + FVector(0.f, 0.f, 260.f), 300.f + P.Radius, 9.f, FVector(0.f, 0.f, 30.f), FVector::ZeroVector, 0.f, TEXT("matter")); }
		}
	}
}

void AAntMatterNet::StepStatic(float Dt)
{
	UWorld* W = GetWorld();
	for (FBody& B : Bodies) { B.Static = FMath::Max(0.f, B.Static - Dt * 0.05f); }
	for (TActorIterator<AAntReactProp> It(W); It; ++It)
	{
		AAntReactProp* P = *It;
		if (!P || P->Kind != AntProp::Balloon || P->Level < 0.5f) { continue; }
		UStaticMeshComponent* Body = P->FindComponentByClass<UStaticMeshComponent>();
		if (!Body) { continue; }
		if (FBody* B = BodyOf(Body, true)) { B->Static = FMath::Max(B->Static, P->Level); B->Quiet = 0.f; }
		const FVector Ctr = Body->Bounds.Origin;
		const float R = (float)Body->Bounds.SphereRadius;
		// light things (crumbs, paper bits, popcorn) leap to the charged latex and stay on it
		for (TActorIterator<AKitchenLoot> L(W); L; ++L)
		{
			AKitchenLoot* Lt = *L;
			if (!Lt || Lt->bIsGear || Lt->bTrophy || Lt->bMain || Lt->IsCarried() || Lt->bHeld || Lt->Weight > 1 || Lt->Tags.Contains(TEXT("AntStaticStuck"))) { continue; }
			const FAntMatProps* M = AntMaterials::ForName(Lt->LootName);
			if (M && M->Density >= 1.f) { continue; }
			const FVector LP = Lt->GetActorLocation();
			const float D = (float)FVector::Dist(LP, Ctr) - R;
			if (D > 300.f * P->Level) { continue; }
			if (D < 25.f)
			{
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Lt->GetRootComponent())) { Root->SetSimulatePhysics(false); }
				Lt->AttachToActor(P, FAttachmentTransformRules::KeepWorldTransform);
				Lt->Tags.Add(TEXT("AntStaticStuck"));
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER static: %s sticks to the charged balloon"), *Lt->LootName);
				continue;
			}
			Lt->Kick((Ctr - LP).GetSafeNormal() * 120.f * P->Level * (1.f - D / 300.f), LP);
		}
	}
}

void AAntMatterNet::StepIce(float Dt)
{
	UWorld* W = GetWorld();
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	for (int32 i = Ices.Num() - 1; i >= 0; --i)
	{
		FAntMatterIce& I = Ices[i];
		const float T = AntReact::HeatAt(W, FVector(I.Center));
		if (T <= 0.f) { continue; }
		// a thin sheet of ice melts in a couple of minutes at room temperature, fast by anything hot
		static TMap<uint8, float> Acc;
		float& A = Acc.FindOrAdd(I.Id);
		A += Dt * 255.f / 150.f * FMath::Clamp(T / Room, 0.2f, 8.f);
		const int32 Drop = FMath::FloorToInt32(A);
		if (Drop <= 0) { continue; }
		A -= Drop;
		const int32 Was = I.Left;
		I.Left = (uint8)FMath::Max(0, Was - Drop);
		int32 Cells = 0;
		for (uint8 Byte : I.Mask) { for (int32 b = 0; b < 8; ++b) { Cells += (Byte >> b) & 1; } }
		if (Fluid)
		{
			const float Vol = Cells * I.Cell * I.Cell * I.Thick * (Was - I.Left) / 255.f;
			Fluid->Spill(AntFluid::Water, FVector(I.Center) + FVector(0.f, 0.f, 8.f), Vol, I.Cell * AntMatter::IceN * 0.35f);
		}
		bIceDirty = true;
		if (I.Left == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER ice %d melted away"), I.Id);
			Acc.Remove(I.Id);
			Ices.RemoveAt(i);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the server API
// ---------------------------------------------------------------------------------------------------------------------

void AAntMatterNet::AddHeat(const FVector& At, float Radius, float TempC, float Seconds)
{
	FHeatSrc& H = HeatSrcs.AddDefaulted_GetRef();
	H.At = At;
	H.Radius = Radius;
	H.Temp = TempC;
	H.Until = LocalClock + Seconds;
}

int32 AAntMatterNet::Douse(const FVector& At, float Radius, float Amount, uint8 Liquid)
{
	TArray<int32> Near;
	Query(At, Radius, Near);
	int32 N = 0;
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		if (!C) { continue; }
		const FVector Q = C->Bounds.GetBox().GetClosestPointTo(At);
		WetBody(C, Q, Radius, Amount, Liquid);
		++N;
	}
	// the water itself lands on whatever is under the splash
	if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(this)) { Fluid->Spill((AntFluid::ELiquid)Liquid, At, 0.25f * Litre * Amount, Radius * 0.5f); }
	AntAudio::Everyone(this, EAntSfx::Splash, At, 1.f);
	MulticastSteam(At, 1.f);   // (the steam call with a tiny size is just the splash droplets on every machine)
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER douse %s at %s r %.0f: %d things wet"), AntFluid::LiquidName((AntFluid::ELiquid)Liquid), *At.ToCompactString(), Radius, N);
	return N;
}

int32 AAntMatterNet::Freeze(const FVector& At, float Radius)
{
	int32 N = 0;
	TArray<int32> Near;
	Query(At, Radius, Near);
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		FBody* B = BodyOf(C, true);
		if (!B) { continue; }
		B->Frost = 1.f;
		B->T = FMath::Min(B->T, -12.f);
		if (B->bBurning) { Extinguish(*B, TEXT("заморожено")); }
		++N;
	}
	// the puddle in the sphere turns to a slab of ice (most of its water goes into the ice)
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	int32 Cells = 0;
	if (Fluid && Ices.Num() < MaxIce)
	{
		FAntMatterIce I;
		I.Id = NextIceId++;
		I.Cell = FMath::Max(8.f, Radius * 2.f / AntMatter::IceN);
		I.Mask.SetNumZeroed(AntMatter::IceN * AntMatter::IceN / 8);
		const FVector O = At - FVector(Radius, Radius, 0.f);
		double SumZ = 0.0, SumD = 0.0;
		for (int32 y = 0; y < AntMatter::IceN; ++y)
		{
			for (int32 x = 0; x < AntMatter::IceN; ++x)
			{
				const FVector Q = O + FVector((x + 0.5f) * I.Cell, (y + 0.5f) * I.Cell, 0.f);
				if (FVector::Dist2D(Q, At) > Radius) { continue; }
				AntFluid::FSample S;
				if (!Fluid->SampleAt(Q + FVector(0.f, 0.f, 120.f), S) || S.Depth[AntFluid::LWater] < 0.3f || FMath::Abs(S.Surface - (float)At.Z) > 250.f) { continue; }
				const int32 Bit = y * AntMatter::IceN + x;
				I.Mask[Bit / 8] |= (uint8)(1 << (Bit % 8));
				SumZ += S.Surface;
				SumD += S.Depth[AntFluid::LWater];
				++Cells;
				Fluid->DrainLayer(FVector(Q.X, Q.Y, S.Surface), I.Cell * 0.6f, AntFluid::LWater, S.Depth[AntFluid::LWater] * 0.7f);
			}
		}
		if (Cells > 0)
		{
			I.Center = FVector(At.X, At.Y, SumZ / Cells + 1.f);
			I.Thick = FMath::Clamp((float)(SumD / Cells) * 0.7f + 1.5f, 2.5f, 14.f);
			I.Born = NetNowOf(GetWorld());
			Ices.Add(I);
			bIceDirty = true;
			OnRep_Ice();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER freeze at %s r %.0f: %d things frost over, ice sheet %d cells"), *At.ToCompactString(), Radius, N, Cells);
	AntAudio::Everyone(this, EAntSfx::IceClink, At, 1.f);
	return N;
}

int32 AAntMatterNet::Electrify(const FVector& At, float Radius, float Seconds, const TCHAR* Why)
{
	auto Conducts = [this](int32 Idx)
	{
		const FAntMatProps* M = Reg[Idx].M;
		if (M && M->Conduct >= 0.5f) { return true; }
		const int32* BI = BodyIndex.Find(Reg[Idx].C);
		return BI && Bodies[*BI].Wet > 0.3f;   // wet things and wet trails carry it on
	};
	TArray<int32> Open, Near;
	TSet<int32> Done;
	Query(At, Radius, Near);
	for (int32 Idx : Near) { if (Conducts(Idx)) { Open.Add(Idx); Done.Add(Idx); } }
	// the water it is in: everything conducting that stands in the same water.
	// (water fix: it used to be "any water under it within 45 m": DepthAt reads the whole column, so a shelf at 2700 over the wet
	// worktop and a puddle across the room counted as the same water as the floor at the short, and the sparks of 27-40 live things
	// set the kitchen on fire in a handful of places at once. Now: the body of water joined to the short, and only what stands in it.)
	int32 InWater = 0;
	FAntWaterBody Body;
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	if (Fluid && Fluid->WaterBodyAt(At + FVector(0.f, 0.f, 10.f), 0.5f, Body))
	{
		Query(At, 4500.f, Near);
		for (int32 Idx : Near)
		{
			if (Done.Contains(Idx) || !Conducts(Idx)) { continue; }
			const UPrimitiveComponent* C = Reg[Idx].C.Get();
			if (C && Fluid->InWaterBody(Body, C->Bounds.GetBox())) { Open.Add(Idx); Done.Add(Idx); ++InWater; }
		}
	}
	// two things touch: their boxes meet, and for a big thing (a whole cabinet run, a rail along the wall: its box takes in half the
	// kitchen) the other one is really at its surface, not just inside its box
	auto Touch = [](const UPrimitiveComponent* A, const UPrimitiveComponent* O)
	{
		const FBox BA = A->Bounds.GetBox(), BO = O->Bounds.GetBox();
		if (!BA.ExpandBy(12.f).Intersect(BO)) { return false; }
		if (BA.GetExtent().GetMax() < 300.f && BO.GetExtent().GetMax() < 300.f) { return true; }
		const FVector P = BA.GetClosestPointTo(BO.GetCenter());
		FVector Q;
		const float DO = O->GetClosestPointOnCollision(P, Q);
		if (DO < 0.f) { return false; }
		FVector R;
		const float DA = A->GetClosestPointOnCollision(Q, R);
		return DA >= 0.f && DA < 12.f;
	};
	int32 N = 0;
	FString Names;
	while (Open.Num() > 0 && N < 40)
	{
		const int32 Idx = Open.Pop(EAllowShrinking::No);
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		FBody* B = BodyOf(C, true);
		if (!B) { continue; }
		B->Live = FMath::Max(B->Live, Seconds);
		B->NextSpark = LocalClock;
		++N;
		if (N <= 12) { Names += FString::Printf(TEXT(" %s[%s]@%.0f"), *GetNameSafe(C->GetOwner()), Reg[Idx].M ? *Reg[Idx].M->Id : TEXT("?"), C->Bounds.GetBox().Min.Z); }
		// what touches it and conducts goes live too
		Query(C->Bounds.Origin, C->Bounds.SphereRadius + 15.f, Near);
		for (int32 J : Near)
		{
			if (Done.Contains(J)) { continue; }
			const UPrimitiveComponent* O = Reg[J].C.Get();
			if (!O || !Conducts(J) || !Touch(C, O)) { continue; }
			Done.Add(J);
			Open.Add(J);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER live at %s (%s): %d conducting things carry the current for %.1f s (%d in the water at it, %d cells of water%s):%s"),
		*At.ToCompactString(), Why, N, Seconds, InWater, Body.Num, Body.bCapped ? TEXT(", capped") : TEXT(""), *Names);
	if (N > 0) { AntAudio::Everyone(this, EAntSfx::StaticCrackle, At, 1.2f); }
	return N;
}

void AAntMatterNet::Spark(const FVector& At, const FVector& Dir, const TCHAR* Why)
{
	MulticastSpark(At, Dir);
	AntAudio::Everyone(this, EAntSfx::Zap, At, 0.7f);
	TArray<int32> Near;
	Query(At, 70.f, Near);
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		const FAntMatProps* M = Reg[Idx].M;
		if (!C || !Burns(M)) { continue; }
		const FVector Q = C->Bounds.GetBox().GetClosestPointTo(At);
		if (M->IgniteAt <= 270.f && FVector::Dist(Q, At) < 55.f)   // a spark lights thin fuel (paper, cloth, cardboard), not a worktop
		{
			if (Ignite(C, Q, Why)) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER spark at %s lit %s [%s] (%s)"), *At.ToCompactString(), *GetNameSafe(C->GetOwner()), *M->Id, Why); }
		}
		else { HeatBody(C, Q, 420.f, SimStep); }
	}
	LightPool(At, Why);
	if (AAntReactNet* RN = AAntReactNet::Get(GetWorld()))
	{
		FVector CC; float CR = 0.f; uint8 Id = 0;
		if (RN->CloudThickness(AntCloudKind::Flour, At, &Id, &CC, &CR) > 0.1f) { Bang(CC, CR, Why); }
	}
}

bool AAntMatterNet::LightPool(const FVector& At, const TCHAR* Why)
{
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	if (!Fluid || Pools.Num() >= MaxPools) { return false; }
	AntFluid::FSample S;
	if (!Fluid->SampleAt(At + FVector(0.f, 0.f, 60.f), S) || S.Depth[AntFluid::LOil] < 0.03f || FMath::Abs(S.Surface - (float)At.Z) > 260.f) { return false; }
	for (const FAntMatterPool& P : Pools) { if (P.Kind == 0 && FVector::Dist2D(FVector(P.Center), At) < P.Radius + 80.f) { return false; } }
	FAntMatterPool& P = Pools.AddDefaulted_GetRef();
	P.Id = NextPoolId++;
	P.Kind = 0;
	P.Center = FVector(At.X, At.Y, S.Surface + 1.f);
	P.Radius = 70.f;
	P.Born = NetNowOf(GetWorld());
	P.Power = 1.f;
	MulticastFlash(P.Center, 260.f, 0);
	AntAudio::Everyone(this, EAntSfx::FlourWhoosh, At, 1.2f);
	if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
	{
		GM->MakeNoise(At, 0.6f, AntNoise::Crash, this, nullptr);
		GM->Broadcast(TEXT("Масло вспыхнуло!"), FLinearColor(1.f, 0.55f, 0.2f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER OIL FLASH %d at %s (oil %.2f uu deep, %s)"), P.Id, *FVector(P.Center).ToCompactString(), S.Depth[AntFluid::LOil], Why);
	return true;
}

void AAntMatterNet::Bang(const FVector& At, float Radius, const TCHAR* Why)
{
	UWorld* W = GetWorld();
	AAntReactNet* RN = AAntReactNet::Get(W);
	uint8 Id = 0;
	if (RN && RN->CloudThickness(AntCloudKind::Flour, At, &Id) > 0.f) { RN->EndCloud(Id); }
	const float R = FMath::Clamp(Radius, 300.f, 1600.f);
	if (Pools.Num() < MaxPools + 2)
	{
		FAntMatterPool& P = Pools.AddDefaulted_GetRef();
		P.Id = NextPoolId++;
		P.Kind = 1;
		P.Center = At;
		P.Radius = R;
		P.Born = NetNowOf(W);
		P.Power = 1.f;
	}
	MulticastFlash(At, R, 1);
	AntAudio::Everyone(this, EAntSfx::FlourWhoosh, At, 2.f);
	AntAudio::Everyone(this, EAntSfx::CrashWood, At, 1.6f);
	if (AAntKitchenGameMode* GM = GMOf(W))
	{
		GM->MakeNoise(At, 1.f, AntNoise::Crash, this, nullptr);   // the whole family comes to look
		GM->Broadcast(TEXT("БАХ! Мучная пыль вспыхнула"), FLinearColor(1.f, 0.6f, 0.25f));
	}
	// the blast: things in it shake, ants fly, what burns easily catches
	AntWreck::Hit(W, At, R, FVector(0.f, 0.f, 400.f), 20.f, this, TEXT("мучной взрыв"));
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead) { continue; }
		const FVector D = A->GetActorLocation() - At;
		const float Dd = (float)D.Size();
		if (Dd > R) { continue; }
		const float K = 1.f - Dd / R;
		A->HurtBy(35.f * K, TEXT("мучной взрыв"));
		A->Stagger(1.5f, D.GetSafeNormal());
		A->ApplyLaunch(D.GetSafeNormal() * 900.f * K + FVector(0.f, 0.f, 500.f * K));
	}
	TArray<int32> Near;
	Query(At, R * 0.8f, Near);
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		if (C && Burns(Reg[Idx].M) && Reg[Idx].M->IgniteAt <= 300.f && FMath::FRand() < 0.5f) { Ignite(C, C->Bounds.GetBox().GetClosestPointTo(At), TEXT("мучной взрыв")); }
		else if (C) { HeatBody(C, C->Bounds.GetBox().GetClosestPointTo(At), 300.f, SimStep); }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER BANG: flour cloud at %s r %.0f went up (%s): noise 1.0, %d things in the blast"), *At.ToCompactString(), R, Why, Near.Num());
}

// ---------------------------------------------------------------------------------------------------------------------
// the records (what every machine sees)
// ---------------------------------------------------------------------------------------------------------------------

void AAntMatterNet::WriteRecords()
{
	auto Q8 = [](float V) { return (uint8)FMath::Clamp(FMath::RoundToInt32(V * 255.f), 0, 255); };
	TMap<uint16, int32> Old;
	for (int32 i = 0; i < Recs.Num(); ++i) { Old.Add(Recs[i].Id, i); }
	TArray<FAntMatterRec> Out;
	Out.Reserve(Bodies.Num());
	for (const FBody& B : Bodies)
	{
		UPrimitiveComponent* C = B.C.Get();
		if (!C) { continue; }
		FAntMatterRec R;
		if (const int32* I = Old.Find(B.Id)) { R = Recs[*I]; }
		R.Id = B.Id;
		AActor* O = C->GetOwner();
		const bool bRep = O && O->GetIsReplicated();
		if (R.Actor != (bRep ? O : nullptr) || R.Comp != C->GetFName())
		{
			R.Actor = bRep ? O : nullptr;
			R.Comp = C->GetFName();
			const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C);
			R.Mesh = S && S->GetStaticMesh() ? S->GetStaticMesh()->GetFName() : NAME_None;
			R.Anchor = C->Bounds.Origin;
		}
		const float Size = FMath::Max(20.f, (float)C->Bounds.SphereRadius);
		const float Cover = B.bBurning ? FMath::Clamp(B.FireR / (Size * 1.15f), 0.f, 1.f) : 0.f;
		const float Vals[AntState::Count] =
		{
			// only metal (conductors) glows red/orange as a whole; wood, paper, cloth show their fire through the burn layer
			B.Wet, B.Char, 0.f, B.Dmg, FMath::Clamp((B.T - 100.f) / 700.f, 0.f, B.M && B.M->Conduct >= 0.5f ? 1.f : 0.45f), Cover >= 0.95f ? B.Flame : 0.f, B.Frost, B.Live > 0.f ? 1.f : B.Static,
			B.Soak[0], B.Soak[1], B.Soak[2], B.Soak[3], 0.f
		};
		for (int32 k = 0; k < AntState::Count; ++k) { R.S[k] = Q8(Vals[k]); }
		R.Flags = (B.bBurning ? AntMatterFlag::Burning : 0) | (B.Live > 0.f ? AntMatterFlag::Live : 0) | (B.bGone ? AntMatterFlag::Gone : 0)
			| (B.bMelting ? AntMatterFlag::Melting : 0) | (B.Wet > 0.05f && B.T >= 99.f ? AntMatterFlag::Steaming : 0) | (B.bBurning && B.Flame > 0.5f ? AntMatterFlag::Smoking : 0);
		R.Fire = B.bBurning ? (uint8)FMath::Clamp(FMath::RoundToInt32(B.FireR / 4.f), 1, 255) : 0;
		R.FireAt = B.FireLocal;
		// the spots: the burning patch first, then the strongest of the rest
		TArray<FAntMatterSpot> Sp;
		if (B.bBurning && Cover < 0.95f)
		{
			FAntMatterSpot& F = Sp.AddDefaulted_GetRef();
			F.Local = B.FireLocal; F.Radius = (uint16)FMath::Clamp(B.FireR, 1.f, 65000.f); F.Kind = AntState::Burn; F.Str = Q8(FMath::Max(0.3f, B.Flame));
		}
		TArray<const FSpotS*> Sorted;
		for (const FSpotS& S : B.Spots) { Sorted.Add(&S); }
		Sorted.Sort([](const FSpotS& A, const FSpotS& Bb) { return A.Str * A.Radius > Bb.Str * Bb.Radius; });
		for (const FSpotS* S : Sorted)
		{
			if (Sp.Num() >= AntState::MaxSpots) { break; }
			FAntMatterSpot& F = Sp.AddDefaulted_GetRef();
			F.Local = S->Local; F.Radius = (uint16)FMath::Clamp(S->Radius, 1.f, 65000.f); F.Kind = S->Kind; F.Str = Q8(S->Str);
		}
		bool bSame = Sp.Num() == R.Spots.Num();
		for (int32 i = 0; bSame && i < Sp.Num(); ++i)
		{
			bSame = Sp[i].Kind == R.Spots[i].Kind && FMath::Abs((int32)Sp[i].Str - (int32)R.Spots[i].Str) < 4 && FMath::Abs((int32)Sp[i].Radius - (int32)R.Spots[i].Radius) < 3
				&& FVector(Sp[i].Local).Equals(FVector(R.Spots[i].Local), 0.5f);
		}
		if (!bSame) { R.Spots = MoveTemp(Sp); }
		Out.Add(MoveTemp(R));
	}
	Recs = MoveTemp(Out);
	bRecsDirty = true;
}

void AAntMatterNet::OnRep_Recs() { bRecsDirty = true; }
void AAntMatterNet::OnRep_Ice() { bIceDirty = true; }

void AAntMatterNet::ResetMatter()
{
	for (FBody& B : Bodies)
	{
		UPrimitiveComponent* C = B.C.Get();
		if (C && B.BaseMass > 0.f && C->IsSimulatingPhysics()) { C->SetMassOverrideInKg(NAME_None, B.BaseMass, true); }
	}
	Bodies.Reset();
	Bodies.Reserve(MaxBodies);
	BodyIndex.Reset();
	Recs.Reset();
	Pools.Reset();
	Ices.Reset();
	HeatSrcs.Reset();
	Flared.Reset();
	NumBurning = 0;
	bRecsDirty = true;
	bIceDirty = true;
	for (TActorIterator<AAntMatterThing> It(GetWorld()); It; ++It) { It->Destroy(); }
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER reset: nothing wet, hot, burning or frozen"));
}

float AAntMatterNet::BodyTemp(const UPrimitiveComponent* C) const
{
	const int32* I = C ? BodyIndex.Find(TWeakObjectPtr<UPrimitiveComponent>(const_cast<UPrimitiveComponent*>(C))) : nullptr;
	return I && Bodies.IsValidIndex(*I) ? Bodies[*I].T : -1000.f;
}

void AAntMatterNet::LogAround(const FVector& At, float Radius) const
{
	TArray<int32> Near;
	Query(At, Radius, Near);
	int32 n = 0;
	for (int32 Idx : Near)
	{
		const UPrimitiveComponent* C = Reg[Idx].C.Get();
		if (!C) { continue; }
		const int32* BI = BodyIndex.Find(Reg[Idx].C);
		const FBody* B = BI ? &Bodies[*BI] : nullptr;
		const FBox Box = C->Bounds.GetBox();
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER info %s/%s [%s] box %s..%s%s"), *GetNameSafe(C->GetOwner()), *C->GetName(), Reg[Idx].M ? *Reg[Idx].M->Id : TEXT("?"),
			*Box.Min.ToCompactString(), *Box.Max.ToCompactString(),
			B ? *FString::Printf(TEXT(": T %.0f C, wet %.2f, char %.2f, frost %.2f, live %.1f, fuel %.2f%s%s, %d spots"), B->T, B->Wet, B->Char, B->Frost, B->Live, B->Fuel,
				B->bBurning ? *FString::Printf(TEXT(", BURNING r %.0f flame %.2f"), B->FireR, B->Flame) : TEXT(""), B->bGone ? TEXT(", GONE") : TEXT(""), B->Spots.Num()) : TEXT(" (at rest)"));
		if (++n >= 30) { break; }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER info at %s r %.0f: %d bodies. %s"), *At.ToCompactString(), Radius, Near.Num(), *Describe());
}

// ---------------------------------------------------------------------------------------------------------------------
// every machine: the effects of the server's events
// ---------------------------------------------------------------------------------------------------------------------

void AAntMatterNet::MulticastSteam_Implementation(FVector_NetQuantize At, float Size)
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	if (Size <= 2.f) { AntFx::Splash(this, At, 10.f); return; }
	for (int32 i = 0; i < 10; ++i) { AntFx::Steam(this, FVector(At) + FMath::VRand() * Size * 0.4f, Size * 0.6f, 160.f + Size); }
}

void AAntMatterNet::MulticastSpark_Implementation(FVector_NetQuantize At, FVector_NetQuantizeNormal Dir)
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	AntFx::Sparks(this, At, 10, 9.f);
	if (AAntFxManager* Fx = AAntFxManager::Get(this))
	{
		// the arc itself: a short bright blue-white streak
		FAntFxBit B;
		B.Pos = At;
		B.Vel = FVector(Dir) * 40.f;
		B.Scale0 = FVector(0.5f, 0.05f, 0.05f);
		B.Scale1 = FVector(0.2f, 0.02f, 0.02f);
		B.Color = FLinearColor(6.f, 7.f, 12.f);
		B.Life = 0.12f;
		B.Look = EAntFxLook::Glow;
		B.bStreak = true;
		Fx->Emit(B);
	}
}

void AAntMatterNet::MulticastFlash_Implementation(FVector_NetQuantize At, float Radius, uint8 Kind)
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	AAntFxManager* Fx = AAntFxManager::Get(this);
	if (!Fx) { return; }
	// KAN-21 run 3: real flame (cards of M_M4_FxFlame) bursting out, not glowing cones; the flour bang adds its own white cloud
	// and the dust that settles after it
	const bool bFlour = Kind == 1;
	const int32 N = bFlour ? 18 : 12;
	for (int32 i = 0; i < N; ++i)
	{
		FAntFxBit B;
		FVector D = FMath::VRand();
		if (!bFlour) { D.Z = FMath::Abs(D.Z) * 0.6f + 0.3f; }   // a flash over a puddle goes up and out
		B.Look = EAntFxLook::Flame;
		B.Pos = FVector(At) + D * Radius * 0.12f;
		B.Vel = D * Radius * (bFlour ? 1.6f : 1.1f) + FVector(0.f, 0.f, Radius * 0.3f);
		B.Drag = 3.f;
		const float S0 = Radius / (bFlour ? 420.f : 380.f);
		B.Scale0 = FVector(S0 * 0.7f, S0, 1.f);
		B.Scale1 = FVector(S0 * 1.6f, S0 * 2.4f, 1.f);
		B.Color = FLinearColor(1.2f, 1.2f, 1.2f, 1.f);
		B.Life = FMath::FRandRange(0.45f, 0.9f);
		B.FadeIn = 0.05f;
		B.FadePow = 1.3f;
		B.bKeep = true;
		Fx->Emit(B);
	}
	// the flash itself: a short hot core
	{
		FAntFxBit B;
		B.Look = EAntFxLook::Glow;
		B.Pos = FVector(At);
		B.Scale0 = FVector(Radius / 700.f);
		B.Scale1 = FVector(Radius / 260.f);
		B.Color = FLinearColor(3.f, 2.f, 0.8f);
		B.Life = 0.18f;
		B.FadeIn = 0.f;
		B.FadePow = 1.5f;
		Fx->Emit(B);
	}
	if (bFlour)
	{
		// the flour that did not burn: a white cloud thrown out by the blast, rolling and hanging
		for (int32 i = 0; i < 26; ++i)
		{
			FAntFxBit B;
			const FVector D = FMath::VRand();
			B.Look = EAntFxLook::Smoke;
			B.Pos = FVector(At) + D * Radius * FMath::FRandRange(0.1f, 0.45f);
			B.Vel = D * Radius * FMath::FRandRange(0.8f, 1.5f) + FVector(0.f, 0.f, Radius * 0.15f);
			B.Drag = 2.4f;
			B.Scale0 = FVector(Radius / 300.f);
			B.Scale1 = FVector(Radius / FMath::FRandRange(100.f, 140.f));
			const float Sh = FMath::FRandRange(0.88f, 1.f);
			B.Color = FLinearColor(0.93f * Sh, 0.91f * Sh, 0.86f * Sh, FMath::FRandRange(0.55f, 0.8f));
			B.Life = FMath::FRandRange(4.5f, 6.5f);
			B.FadeIn = 0.12f;
			B.FadePow = 1.6f;
			B.MinAmbient = 0.65f;
			B.bKeep = true;
			Fx->Emit(B);
		}
		// and then it settles: a thin haze sinking to the floor, and white flecks drifting down onto everything
		FHitResult H;
		float FloorZ = (float)At.Z - Radius;
		if (GetWorld()->LineTraceSingleByChannel(H, FVector(At), FVector(At) - FVector(0.f, 0.f, 4000.f), ECC_Visibility)) { FloorZ = (float)H.ImpactPoint.Z; }
		for (int32 i = 0; i < 12; ++i)
		{
			FAntFxBit B;
			B.Look = EAntFxLook::Smoke;
			B.Pos = FVector(At) + FVector(FMath::FRandRange(-1.f, 1.f) * Radius * 0.8f, FMath::FRandRange(-1.f, 1.f) * Radius * 0.8f, FMath::FRandRange(-0.2f, 0.4f) * Radius);
			B.Vel = FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), -FMath::FRandRange(15.f, 35.f));
			B.Scale0 = FVector(Radius / 200.f);
			B.Scale1 = FVector(Radius / 110.f);
			B.Color = FLinearColor(0.9f, 0.88f, 0.83f, 0.22f);
			B.Life = FMath::FRandRange(7.f, 9.f);
			B.FadeIn = 0.35f;
			B.FadePow = 0.9f;
			B.MinAmbient = 0.65f;
			B.FloorZ = FloorZ + 30.f;
			B.bKeep = true;
			Fx->Emit(B);
		}
		for (int32 i = 0; i < 45; ++i)
		{
			FAntFxBit B;
			B.Look = EAntFxLook::Solid;
			B.Shape = EAntShape::Cube;
			B.Pos = FVector(At) + FMath::VRand() * Radius * FMath::FRandRange(0.1f, 0.9f);
			B.Vel = FMath::VRand() * Radius * 0.35f;
			B.Gravity = 70.f;
			B.Drag = 1.6f;
			B.Rot = FQuat(FMath::VRand(), FMath::FRandRange(0.f, 6.f));
			B.Scale0 = B.Scale1 = FVector(FMath::FRandRange(0.025f, 0.05f), FMath::FRandRange(0.025f, 0.05f), 0.01f);
			B.Color = FLinearColor(0.92f, 0.9f, 0.85f, 1.f);
			B.Life = FMath::FRandRange(6.f, 9.f);
			B.FloorZ = FloorZ + 0.5f;
			B.bKeep = true;
			Fx->Emit(B);
		}
	}
	// the smoke after it
	for (int32 i = 0; i < (bFlour ? 8 : 6); ++i)
	{
		FAntFxBit B;
		B.Look = EAntFxLook::Smoke;
		B.Pos = FVector(At) + FMath::VRand() * Radius * 0.3f + FVector(0.f, 0.f, Radius * 0.25f);
		B.Vel = FMath::VRand() * Radius * 0.25f + FVector(0.f, 0.f, Radius * 0.4f);
		B.Scale0 = FVector(Radius / 450.f);
		B.Scale1 = FVector(Radius / 130.f);
		B.Color = FLinearColor(0.09f, 0.085f, 0.08f, 0.45f);
		B.Life = FMath::FRandRange(2.2f, 3.4f);
		B.FadeIn = 0.25f;
		B.Drag = 1.2f;
		B.MinAmbient = 0.35f;
		Fx->Emit(B);
	}
	// a flash of light for a moment
	if (Lights.Num() > 0 && Lights.Last())
	{
		UPointLightComponent* L = Lights.Last();
		L->SetWorldLocation(FVector(At) + FVector(0.f, 0.f, 60.f));
		L->SetAttenuationRadius(Radius * 3.f);
		L->SetIntensity(Kind == 1 ? 1200.f : 500.f);
		L->SetVisibility(true);
		FlashUntil = LocalClock + (Kind == 1 ? 0.5f : 0.3f);
	}
}

// =====================================================================================================================
// Every machine: the records onto the things, the flames, the ice
// =====================================================================================================================

void AAntMatterNet::LocalTick(float Dt)
{
	LocalClock += Dt;
	ScanRegistry(HasAuthority() ? 400 : 250);
	if (bRecsDirty) { bRecsDirty = false; ApplyRecords(); }
	MessClock += Dt;
	if (MessClock >= 0.5f) { MessClock = 0.f; WatchMess(); }
	if (GetNetMode() == NM_DedicatedServer) { return; }
	if (bIceDirty) { bIceDirty = false; DrawIce(); }
	DrawFires(Dt);
	static const bool bWatch = FParse::Param(FCommandLine::Get(), TEXT("AntMatterWatch"));
	if (bWatch && !HasAuthority()) { ClientWatch(Dt); }
}

void AAntMatterNet::ClientWatch(float Dt)
{
	WatchClock += Dt;
	if (WatchClock < 1.f) { return; }
	WatchClock = 0.f;
	int32 Burn = 0, Live = 0, Melt = 0, Wet = 0, Charred = 0, Shown2 = 0, CardsOn = 0;
	const FAntMatterRec* Pick = nullptr;
	for (const FAntMatterRec& R : Recs)
	{
		if (R.Flags & AntMatterFlag::Burning) { ++Burn; }
		if (R.Flags & AntMatterFlag::Live) { ++Live; }
		if (R.Flags & AntMatterFlag::Melting) { ++Melt; }
		if (R.S[AntState::Wet] > 25) { ++Wet; }
		if (R.S[AntState::Char] > 25) { ++Charred; }
		const FShown* S = Shown.Find(R.Id);
		if (S && S->C.IsValid()) { ++Shown2; }
		if (!Pick && S && S->C.IsValid() && (R.Flags & (AntMatterFlag::Burning | AntMatterFlag::Live)) && !WatchShotIds.Contains(R.Id)) { Pick = &R; }
	}
	for (const UStaticMeshComponent* C : Cards) { if (C && C->IsVisible()) { ++CardsOn; } }
	UE_LOG(LogTemp, Display, TEXT("ANTMATTER watch (client): records %d (on things here %d), burning %d, live %d, melting %d, wet %d, charred %d, pools %d, ice %d, flame cards drawn %d"),
		Recs.Num(), Shown2, Burn, Live, Melt, Wet, Charred, Pools.Num(), Ices.Num(), CardsOn);
	// this machine's own picture of a burning / live thing: its camera on it, a shot now and one a few seconds later
	if (Pick && LocalClock >= WatchShotAt && WatchShots < 10)
	{
		const FShown* S = Shown.Find(Pick->Id);
		UPrimitiveComponent* C = S ? S->C.Get() : nullptr;
		if (C)
		{
			WatchShotIds.Add(Pick->Id);
			const FVector To = C->Bounds.Origin;
			const float Dd = FMath::Clamp((float)C->Bounds.SphereRadius * 3.f + 500.f, 700.f, 2400.f);
			FVector From = To + FVector(0.f, -Dd, Dd * 0.45f);
			const FVector Offs[] = { FVector(0.f, -Dd, Dd * 0.45f), FVector(-Dd * 0.8f, -Dd * 0.6f, Dd * 0.5f), FVector(Dd * 0.8f, -Dd * 0.6f, Dd * 0.5f), FVector(0.f, Dd, Dd * 0.45f), FVector(Dd, 0.f, Dd * 0.45f), FVector(-Dd, 0.f, Dd * 0.45f) };
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterWatch), false);
			for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
			for (const FVector& O : Offs)
			{
				FHitResult H;
				if (!GetWorld()->SweepSingleByChannel(H, To + O.GetSafeNormal() * (float)C->Bounds.SphereRadius * 1.1f, To + O, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(20.f), Q)) { From = To + O; break; }
			}
			AntCloth::DevCam(GetWorld(), From, To, 55.f);
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { if (PC->MyHUD) { PC->MyHUD->bShowHUD = false; } }
			const FString Kind = (Pick->Flags & AntMatterFlag::Burning) ? TEXT("burning") : TEXT("live");
			const FString Name = FString::Printf(TEXT("client_%02d_%s_%s"), WatchShots, *Kind, *GetNameSafe(C->GetOwner()).Left(24));
			++WatchShots;
			WatchShotAt = LocalClock + 6.f;
			TWeakObjectPtr<UWorld> WW = GetWorld();
			const float At = LocalClock;
			Later(0.6f, [WW, Name]() { if (UWorld* W2 = WW.Get()) { AntCloth::DevShot(W2, TEXT("matter"), Name); } });
			Later(4.5f, [WW, Name]() { if (UWorld* W2 = WW.Get()) { AntCloth::DevShot(W2, TEXT("matter"), Name + TEXT("_later")); } });
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER watch (client): shooting %s (%s) at %.1f"), *Name, *Kind, At);
		}
	}
}

UPrimitiveComponent* AAntMatterNet::Resolve(const FAntMatterRec& R)
{
	if (HasAuthority())
	{
		for (const FBody& B : Bodies) { if (B.Id == R.Id) { return B.C.Get(); } }
		return nullptr;
	}
	if (R.Actor)
	{
		TInlineComponentArray<UPrimitiveComponent*> Comps(R.Actor);
		for (UPrimitiveComponent* C : Comps) { if (C && C->GetFName() == R.Comp) { return C; } }
		return nullptr;
	}
	// a piece of the builder (the same on every machine): by its mesh and a point inside it
	TArray<int32> Near;
	Query(FVector(R.Anchor), 40.f, Near);
	UPrimitiveComponent* Best = nullptr;
	float BestD = 1.e12f;
	for (int32 Idx : Near)
	{
		UPrimitiveComponent* C = Reg[Idx].C.Get();
		const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C);
		if (!S || !S->GetStaticMesh() || S->GetStaticMesh()->GetFName() != R.Mesh) { continue; }
		const float D = (float)FVector::DistSquared(C->Bounds.Origin, FVector(R.Anchor));
		if (D < BestD) { BestD = D; Best = C; }
	}
	return TakeOver(Best);
}

void AAntMatterNet::ApplyRecords()
{
	TSet<uint16> Live;
	for (const FAntMatterRec& R : Recs)
	{
		Live.Add(R.Id);
		FShown& S = Shown.FindOrAdd(R.Id);
		UPrimitiveComponent* C = S.C.Get();
		if (!C || (!HasAuthority() && R.Actor && C->GetOwner() != R.Actor)) { C = Resolve(R); }
		else if (HasAuthority()) { C = Resolve(R); }   // (the server's body may have moved onto a wreck body's mesh)
		if (!C) { continue; }
		if (S.C.Get() != C)
		{
			if (UPrimitiveComponent* OldC = S.C.Get()) { AntState::ClearSource(OldC, AntState::Matter); }
			S.C = C;
		}
		for (int32 k = 0; k < AntState::Count; ++k) { AntState::SetSource(C, AntState::Matter, (AntState::EKind)k, R.S[k] / 255.f); }
		TArray<AntState::FSpot, TInlineAllocator<AntState::MaxSpots>> Sp;
		for (const FAntMatterSpot& F : R.Spots)
		{
			AntState::FSpot& X = Sp.AddDefaulted_GetRef();
			X.Local = FVector(F.Local); X.Radius = F.Radius; X.Kind = F.Kind; X.Strength = F.Str / 255.f;
		}
		AntState::SetSpots(C, AntState::Matter, Sp);
		// molten plastic slumps: lower and wider, its foot where it was (only a thing that moves: never a fixed piece of the flat)
		if ((R.Flags & AntMatterFlag::Melting) || S.bSagged)
		{
			const float Want = FMath::Clamp((R.S[AntState::Damage] / 255.f) / 0.85f, 0.f, 1.f);
			if (C->Mobility == EComponentMobility::Movable && Want > S.Sag + 0.02f)
			{
				if (!S.bSagged) { S.bSagged = true; S.OrigScale = C->GetComponentScale(); }
				S.Sag = Want;
				const float FootZ = (float)C->Bounds.GetBox().Min.Z;
				C->SetWorldScale3D(S.OrigScale * FVector(1.f + 0.16f * Want, 1.f + 0.16f * Want, 1.f - 0.5f * Want));
				C->UpdateBounds();
				C->AddWorldOffset(FVector(0.f, 0.f, FootZ - (float)C->Bounds.GetBox().Min.Z));
			}
		}
		// burnt away: gone on every machine
		const bool bGone = (R.Flags & AntMatterFlag::Gone) != 0;
		if (bGone && !S.bHidden)
		{
			S.bHidden = true;
			C->SetVisibility(false, true);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	// records that went (a new raid): the things show nothing of the matter any more, what burnt away is back
	for (auto It = Shown.CreateIterator(); It; ++It)
	{
		if (Live.Contains(It.Key())) { continue; }
		if (UPrimitiveComponent* C = It.Value().C.Get())
		{
			AntState::ClearSource(C, AntState::Matter);
			if (It.Value().bSagged && C->Mobility == EComponentMobility::Movable) { C->SetWorldScale3D(It.Value().OrigScale); }
			if (It.Value().bHidden)
			{
				C->SetVisibility(true, true);
				C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
		It.RemoveCurrent();
	}
}

namespace
{
	/** A stable 0..1 from two integers: the same every frame and on every machine (where a flame stands on its patch). */
	float Hash01(uint32 A, uint32 B)
	{
		uint32 H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA77u;
		H ^= H >> 15; H *= 0x2C1B3C6Du; H ^= H >> 12; H *= 0x297A2D39u; H ^= H >> 15;
		return (float)(H & 0xFFFFFFu) / 16777216.f;
	}

	/** The colour of a thing (its material's Color / Tint), for what drips off it when it melts. */
	FLinearColor ColorOf(const UPrimitiveComponent* C)
	{
		FLinearColor Out(0.35f, 0.35f, 0.36f);
		if (const UMaterialInterface* M = C ? C->GetMaterial(0) : nullptr)
		{
			FLinearColor V;
			if (M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), V) && !V.Equals(FLinearColor::White, 0.02f)) { Out = V; }
			else if (M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), V) && !V.Equals(FLinearColor::White, 0.02f)) { Out = V; }
		}
		return Out;
	}
}

void AAntMatterNet::DrawFires(float Dt)
{
	UWorld* W = GetWorld();
	AAntFxManager* Fx = AAntFxManager::Get(this);
	const FVector Cam = CameraOf(W);
	// the flames (cards) every frame; the bits (embers, smoke, drips) 20 times a second
	FxAcc += Dt;
	const bool bEmit = FxAcc >= 0.05f;
	const float Step = bEmit ? FxAcc : 0.f;
	if (bEmit) { FxAcc = 0.f; }
	// the bit pool is shared with the whole game: fires never take the last of it (bangs, splashes, the slipper's dust)
	const bool bRoom = Fx && Fx->NumLive() < AAntFxManager::MaxBits - 120;
	struct FLit { FVector At; float Size; float D; };
	TArray<FLit> Lit;
	TArray<FCard> Want;
	float Nearest = 1.e9f;
	const float Now = NetNowOf(W);
	for (const FAntMatterRec& R : Recs)
	{
		if (!(R.Flags & (AntMatterFlag::Burning | AntMatterFlag::Live | AntMatterFlag::Melting | AntMatterFlag::Steaming))) { continue; }
		FShown* S = Shown.Find(R.Id);
		UPrimitiveComponent* C = S ? S->C.Get() : nullptr;
		if (!C || !C->IsVisible()) { continue; }
		const FBox Box = C->Bounds.GetBox();
		const float CamD = (float)FVector::Dist(Box.GetCenter(), Cam);
		if (CamD > 9000.f) { continue; }
		const float Lod = CamD < 2500.f ? 1.f : 0.45f;
		if (R.Flags & AntMatterFlag::Burning)
		{
			const FVector Front = C->GetComponentTransform().TransformPosition(FVector(R.FireAt));
			const float Size = FMath::Max(8.f, R.Fire * 4.f);
			const float Big = R.S[AntState::Burn] > 0 ? (float)C->Bounds.SphereRadius : Size;
			Lit.Add({ Front, Big, CamD });
			Nearest = FMath::Min(Nearest, CamD);
			// the flames: a few tongues standing on the burning patch, the same ones every frame (the material animates them);
			// on a thin thing (a towel, a napkin) they stand in front of it, on a thick one on its face
			FVector ToCam = Cam - Front;
			ToCam.Z = 0.f;
			ToCam = ToCam.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.f, 0.f, 0.f));
			const FVector Ext = Box.GetExtent();
			const float Thick = (float)(FMath::Abs(ToCam.X) * Ext.X + FMath::Abs(ToCam.Y) * Ext.Y);
			const int32 K = CamD < 2500.f ? FMath::Clamp(1 + FMath::RoundToInt32(Big / 45.f), 2, 6) : FMath::Clamp(1 + FMath::RoundToInt32(Big / 90.f), 1, 3);
			for (int32 k = 0; k < K; ++k)
			{
				const float U = Hash01(R.Id, k * 4) * 2.f - 1.f, V = Hash01(R.Id, k * 4 + 1) * 2.f - 1.f, Sd = Hash01(R.Id, k * 4 + 2);
				const float Spread = k == 0 ? 0.f : Big * 0.7f;
				FVector P = Box.ExpandBy(-1.f).GetClosestPointTo(Front + FVector(U * Spread, V * Spread, -FMath::Abs(V) * Spread * 0.3f));
				if (Thick < 60.f) { P += ToCam * ((float)FVector::DotProduct(Box.GetCenter() - P, ToCam) + Thick + 3.f); }
				else { P += ToCam * 6.f; }
				FCard Cd;
				const float Breathe = 0.86f + 0.14f * FMath::Sin(LocalClock * (2.1f + Sd * 2.3f) + Sd * 40.f);
				Cd.H = FMath::Clamp(Big * (0.9f + 0.7f * Sd), 30.f, 320.f) * Breathe * (k == 0 ? 1.15f : 1.f);
				Cd.Wd = Cd.H * (0.75f + 0.3f * Hash01(R.Id, k * 4 + 3));
				Cd.Base = P;
				Cd.Seed = Sd * 100.f;
				Cd.Bright = 1.f;
				Cd.D = CamD;
				Want.Add(Cd);
			}
			if (bEmit && (bRoom || CamD < 2000.f))
			{
				// soft dark smoke rising off the flames
				if (FMath::FRand() < Step * (1.2f + Big / 90.f) * Lod)
				{
					FAntFxBit B;
					B.Look = EAntFxLook::Smoke;
					B.Pos = Front + FVector(FMath::FRandRange(-0.3f, 0.3f) * Big, FMath::FRandRange(-0.3f, 0.3f) * Big, FMath::Clamp(Big * 0.9f, 25.f, 260.f));
					B.Vel = FVector(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f), 70.f + Big * 0.5f);
					const float Sz = FMath::Clamp(Big / 110.f, 0.25f, 2.6f);
					B.Scale0 = FVector(Sz);
					B.Scale1 = FVector(Sz * 3.2f);
					B.Color = FLinearColor(0.07f, 0.065f, 0.06f, 0.38f);
					B.Life = FMath::FRandRange(2.6f, 4.f);
					B.FadeIn = 0.25f;
					B.FadePow = 1.4f;
					B.Drag = 0.5f;
					B.MinAmbient = 0.3f;
					B.bKeep = CamD < 2000.f;   // the near fire's smoke is not pushed out of a full pool
					Fx->Emit(B);
				}
				// embers flying up out of it
				if (FMath::FRand() < Step * (2.f + Big / 120.f) * Lod)
				{
					FAntFxBit B;
					B.Pos = Front + FMath::VRand() * Big * 0.4f + FVector(0.f, 0.f, Big * 0.3f);
					B.Vel = FVector(FMath::FRandRange(-50.f, 50.f), FMath::FRandRange(-50.f, 50.f), FMath::FRandRange(140.f, 330.f));
					B.Gravity = -40.f;
					B.Drag = 0.9f;
					B.Scale0 = FVector(0.035f, 0.012f, 0.012f);
					B.Scale1 = B.Scale0 * 0.4f;
					B.Color = FLinearColor(3.2f, 0.9f, 0.12f);
					B.Life = FMath::FRandRange(0.7f, 1.4f);
					B.FadeIn = 0.f;
					B.FadePow = 0.7f;
					B.Shape = EAntShape::Cube;
					B.Look = EAntFxLook::Glow;
					B.bStreak = true;
					Fx->Emit(B);
				}
			}
			if (S && LocalClock >= S->SoundAt && CamD < 4000.f)
			{
				S->SoundAt = LocalClock + FMath::FRandRange(0.9f, 1.6f);
				AntAudio::Play(this, EAntSfx::Sizzle, Front, 0.35f + 0.5f * FMath::Clamp(Big / 300.f, 0.f, 1.f));
			}
		}
		if (!bEmit) { continue; }
		if ((R.Flags & AntMatterFlag::Live) && FMath::FRand() < Step * 6.f * Lod)
		{
			const FVector P(FMath::FRandRange(Box.Min.X, Box.Max.X), FMath::FRandRange(Box.Min.Y, Box.Max.Y), FMath::FRandRange(Box.Min.Z, Box.Max.Z));
			AntFx::Sparks(this, Box.GetClosestPointTo(P), 3, 5.f);
			if (FMath::FRand() < 0.2f) { AntAudio::Play(this, EAntSfx::StaticCrackle, P, 0.5f); }
		}
		if ((R.Flags & AntMatterFlag::Steaming) && FMath::FRand() < Step * 4.f * Lod)
		{
			AntFx::Steam(this, FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z), FMath::Clamp((float)C->Bounds.SphereRadius * 0.3f, 10.f, 120.f), 160.f);
		}
		if ((R.Flags & AntMatterFlag::Melting) && Fx)   // (a few bits: not held back by a full pool, or a melt next to fires shows nothing)
		{
			const FVector Ext = Box.GetExtent();
			const float Dim = (float)FMath::Max(Ext.X, Ext.Y);
			// molten drops running down its sides and dropping to the floor, where they spread into little puddles
			if (FMath::FRand() < Step * 4.f * Lod)
			{
				if (S && S->MeltColor.R < 0.f)
				{
					S->MeltColor = ColorOf(C);
					UE_LOG(LogTemp, Display, TEXT("ANTMATTER melt fx: drips (%s) and smoke on %s, fx bits %d"), *S->MeltColor.ToString(), *GetNameSafe(C->GetOwner()), Fx->NumLive());
				}
				const float A = FMath::FRand() * 2.f * PI;
				const FVector Side = Box.GetClosestPointTo(Box.GetCenter() + FVector(FMath::Cos(A) * Ext.X * 2.f, FMath::Sin(A) * Ext.Y * 2.f, Ext.Z * FMath::FRandRange(-0.3f, 0.6f)));
				const FVector Out = FVector(Side.X - Box.GetCenter().X, Side.Y - Box.GetCenter().Y, 0.f).GetSafeNormal();
				const float Dr = FMath::Clamp(Dim * 0.12f, 4.f, 22.f) / 100.f;   // (run 3: 4 uu drops did not read from a step away)
				FAntFxBit B;
				B.Look = EAntFxLook::Solid;
				B.Shape = EAntShape::Sphere;
				B.Pos = Side + Out * Dr * 40.f;
				B.Vel = FVector(0.f, 0.f, -20.f);
				B.Gravity = 260.f;
				B.Drag = 0.4f;
				B.Scale0 = FVector(Dr * 0.7f, Dr * 0.7f, Dr * 1.5f);
				B.Scale1 = FVector(Dr * 2.2f, Dr * 2.2f, Dr * 0.3f);   // lands and spreads
				const FLinearColor Mc = S ? S->MeltColor : FLinearColor(0.35f, 0.35f, 0.36f);
				B.Color = FLinearColor(Mc.R * 0.8f, Mc.G * 0.8f, Mc.B * 0.8f, 1.f);
				B.Life = FMath::FRandRange(3.f, 4.5f);
				B.FloorZ = Box.Min.Z + 0.5f;
				B.bKeep = CamD < 2500.f;
				Fx->Emit(B);
			}
			// thick dark acrid smoke off its top
			if (FMath::FRand() < Step * 3.5f * Lod)
			{
				FAntFxBit B;
				B.Look = EAntFxLook::Smoke;
				B.Pos = FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z) + FVector(FMath::FRandRange(-0.5f, 0.5f) * Ext.X, FMath::FRandRange(-0.5f, 0.5f) * Ext.Y, 10.f);
				B.Vel = FVector(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(45.f, 80.f));
				const float Sz = FMath::Clamp(Dim / 90.f, 0.4f, 2.2f);
				B.Scale0 = FVector(Sz * 0.7f);
				B.Scale1 = FVector(Sz * 3.2f);
				B.Color = FLinearColor(0.16f, 0.15f, 0.14f, 0.6f);   // sooty grey: black smoke vanished against a dark room at night
				B.Life = FMath::FRandRange(3.f, 4.5f);
				B.FadeIn = 0.2f;
				B.FadePow = 1.3f;
				B.Drag = 0.35f;
				B.MinAmbient = 0.3f;
				B.bKeep = CamD < 2500.f;
				Fx->Emit(B);
			}
		}
	}
	// burning oil and the fireballs: a bed of flames over the puddle, a ball of fire
	for (const FAntMatterPool& P : Pools)
	{
		const float CamD = (float)FVector::Dist(FVector(P.Center), Cam);
		if (CamD > 9000.f) { continue; }
		Lit.Add({ FVector(P.Center), P.Radius, CamD });
		Nearest = FMath::Min(Nearest, CamD);
		const bool bBall = P.Kind == 1;
		const int32 K = bBall ? 9 : FMath::Clamp(2 + FMath::RoundToInt32(P.Radius / 30.f), 3, 9);
		for (int32 k = 0; k < K; ++k)
		{
			const float U = Hash01(P.Id + 1000u, k * 4), V = Hash01(P.Id + 1000u, k * 4 + 1), Sd = Hash01(P.Id + 1000u, k * 4 + 2);
			const float A = U * 2.f * PI, Rr = FMath::Sqrt(V) * P.Radius * (bBall ? 0.55f : 0.8f);
			FCard Cd;
			Cd.Base = FVector(P.Center) + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, bBall ? (Sd - 0.6f) * P.Radius * 0.6f : 0.f);
			const float Breathe = 0.85f + 0.15f * FMath::Sin(LocalClock * (2.4f + Sd * 2.f) + Sd * 30.f);
			Cd.H = (bBall ? P.Radius * (0.55f + 0.5f * Sd) : FMath::Lerp(70.f, 160.f, Sd) * (0.5f + 0.5f * P.Power)) * Breathe;
			Cd.Wd = Cd.H * (bBall ? 0.95f : 0.85f);
			Cd.Seed = Sd * 100.f;
			Cd.Bright = bBall ? 1.1f + 0.4f * P.Power : 1.f;
			Cd.D = CamD;
			Want.Add(Cd);
		}
		if (bEmit && bRoom && !bBall && FMath::FRand() < Step * (1.5f + P.Radius / 80.f))
		{
			FAntFxBit B;
			B.Look = EAntFxLook::Smoke;
			B.Pos = FVector(P.Center) + FVector(FMath::FRandRange(-0.5f, 0.5f) * P.Radius, FMath::FRandRange(-0.5f, 0.5f) * P.Radius, 90.f);
			B.Vel = FVector(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f), 90.f);
			B.Scale0 = FVector(FMath::Clamp(P.Radius / 140.f, 0.4f, 2.f));
			B.Scale1 = B.Scale0 * 3.f;
			B.Color = FLinearColor(0.06f, 0.055f, 0.05f, 0.42f);
			B.Life = FMath::FRandRange(2.6f, 4.f);
			B.FadeIn = 0.25f;
			B.FadePow = 1.4f;
			B.Drag = 0.5f;
			B.MinAmbient = 0.3f;
			Fx->Emit(B);
		}
	}
	(void)Now;
	DrawCards(Want, Cam);
	// the light of the nearest fires, flickering: a small kitchen fire, not a floodlight (KAN-21 run 3: lower again, the
	// cupboards are white inside and Lumen bounces it all round them)
	Lit.Sort([](const FLit& A, const FLit& B) { return A.D < B.D; });
	const int32 NL = Lights.Num() - 1;   // the last one is the flash
	for (int32 i = 0; i < NL; ++i)
	{
		UPointLightComponent* L = Lights[i];
		if (!L) { continue; }
		if (!Lit.IsValidIndex(i)) { if (L->IsVisible()) { L->SetVisibility(false); } continue; }
		const FLit& F = Lit[i];
		const float Fl = 0.75f + 0.25f * FMath::Sin(LocalClock * 23.f + i * 1.7f) * FMath::Sin(LocalClock * 7.3f + i);
		L->SetWorldLocation(F.At + FVector(0.f, 0.f, 30.f + F.Size * 0.4f));
		L->SetAttenuationRadius(FMath::Min(250.f + F.Size * 1.6f, 1000.f));
		L->SetIntensity(FMath::Min(10.f + F.Size * 0.25f, 80.f) * Fl);   // candelas
		if (!L->IsVisible()) { L->SetVisibility(true); }
	}
	if (Lights.Num() > 0 && Lights.Last() && Lights.Last()->IsVisible() && LocalClock > FlashUntil) { Lights.Last()->SetVisibility(false); }
	if (FireGrade)
	{
		const float Wt = Nearest < 1.e8f ? FMath::Clamp(1.f - (Nearest - 2000.f) / 1500.f, 0.f, 1.f) : 0.f;
		FireGrade->BlendWeight = FMath::FInterpTo(FireGrade->BlendWeight, Wt, Dt, 2.f);
	}
}

void AAntMatterNet::DrawCards(const TArray<FCard>& WantIn, const FVector& Cam)
{
	constexpr int32 MaxCards = 72;
	TArray<FCard> Want = WantIn;
	if (Want.Num() > MaxCards)
	{
		Want.Sort([](const FCard& A, const FCard& B) { return A.D < B.D; });
		Want.SetNum(MaxCards);
	}
	UStaticMesh* Mesh = AntFx::CardMesh();
	UMaterialInterface* Mat = AntFx::CardMaterial(EAntFxLook::Flame);
	if (!Mesh || !Mat) { return; }
	while (Cards.Num() < Want.Num())
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->bReceivesDecals = false;
		C->SetAbsolute(true, true, true);
		C->SetStaticMesh(Mesh);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, this);
		C->SetMaterial(0, M);
		C->ComponentTags.Add(TEXT("M4Fx"));
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		C->SetVisibility(false);
		Cards.Add(C);
		CardMids.Add(M);
		CardParams.Add(FVector2f(-1.f, -1.f));
	}
	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		UStaticMeshComponent* C = Cards[i];
		if (!C) { continue; }
		if (!Want.IsValidIndex(i))
		{
			if (C->IsVisible()) { C->SetVisibility(false); }
			continue;
		}
		const FCard& Cd = Want[i];
		const FVector Ctr = Cd.Base + FVector(0.f, 0.f, Cd.H * 0.38f);   // the card's soft foot just under the burning surface
		C->SetWorldTransform(FTransform(AntFx::CardRot(Ctr, Cam, true), Ctr, FVector(Cd.Wd / 100.f, Cd.H / 100.f, 1.f)));
		if (UMaterialInstanceDynamic* M = CardMids[i])
		{
			FVector2f& Pm = CardParams[i];
			if (FMath::Abs(Pm.X - Cd.Seed) > 0.01f) { M->SetScalarParameterValue(TEXT("Seed"), Cd.Seed); Pm.X = Cd.Seed; }
			if (FMath::Abs(Pm.Y - Cd.Bright) > 0.02f) { M->SetVectorParameterValue(TEXT("Color"), FLinearColor(Cd.Bright, Cd.Bright, Cd.Bright, 1.f)); Pm.Y = Cd.Bright; }
		}
		if (!C->IsVisible()) { C->SetVisibility(true); }
	}
}

void AAntMatterNet::DrawIce()
{
	if (!IceMesh)
	{
		IceMesh = NewObject<UInstancedStaticMeshComponent>(this, TEXT("AntIceSheet"));
		IceMesh->SetStaticMesh(Map4Look::Shape(0));
		IceMesh->SetMobility(EComponentMobility::Movable);
		IceMesh->SetupAttachment(GetRootComponent());
		IceMesh->SetAbsolute(true, true, true);
		if (const Map4Look::FLook* L = Map4Look::Named(TEXT("Ice"))) { IceMesh->SetMaterial(0, Map4Look::Get(*L)); }
		IceMesh->ComponentTags.Add(TEXT("Surf_Ice"));
		IceMesh->RegisterComponent();
		AddInstanceComponent(IceMesh);
		KitchenArt::SetSolid(IceMesh, KitchenArt::ESolid::Static);   // ants and feet stand on it: a bridge over the puddle
	}
	// the rim (KAN-21 run 3): the cells at the edge of the sheet are drawn as low frosted domes that run out to nothing, so the
	// sheet ends in a rounded, rimed lip of frozen water instead of a step of boxes (the boxes under them stay as the bridge)
	if (!IceRim)
	{
		IceRim = NewObject<UInstancedStaticMeshComponent>(this, TEXT("AntIceRim"));
		IceRim->SetStaticMesh(Map4Look::Shape(1));
		IceRim->SetMobility(EComponentMobility::Movable);
		IceRim->SetupAttachment(GetRootComponent());
		IceRim->SetAbsolute(true, true, true);
		if (const Map4Look::FLook* L = Map4Look::Named(TEXT("Ice"))) { IceRim->SetMaterial(0, Map4Look::Get(*L)); }
		IceRim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		IceRim->SetCastShadow(false);
		IceRim->SetCustomPrimitiveDataFloat(AntState::Frost, 0.7f);   // the state layer's frost: white rime crystals
		IceRim->RegisterComponent();
		AddInstanceComponent(IceRim);
	}
	IceMesh->ClearInstances();
	IceRim->ClearInstances();
	int32 N = 0;
	auto On = [](const FAntMatterIce& I, int32 x, int32 y)
	{
		if (x < 0 || y < 0 || x >= AntMatter::IceN || y >= AntMatter::IceN) { return false; }
		const int32 Bit = y * AntMatter::IceN + x;
		return I.Mask.IsValidIndex(Bit / 8) && (I.Mask[Bit / 8] & (1 << (Bit % 8))) != 0;
	};
	for (const FAntMatterIce& I : Ices)
	{
		const float Left = I.Left / 255.f;
		const float Th = FMath::Max(1.f, I.Thick * (0.35f + 0.65f * Left));
		const FVector O = FVector(I.Center) - FVector(I.Cell * AntMatter::IceN * 0.5f, I.Cell * AntMatter::IceN * 0.5f, 0.f);
		for (int32 y = 0; y < AntMatter::IceN; ++y)
		{
			for (int32 x = 0; x < AntMatter::IceN; ++x)
			{
				const int32 Bit = y * AntMatter::IceN + x;
				if (!On(I, x, y)) { continue; }
				// a melting sheet thins and its cells shrink unevenly
				const float Rn = FMath::Frac(FMath::Sin((float)Bit * 12.9898f) * 43758.5453f);
				const float Shrink = FMath::Clamp(Left * 1.4f - 0.2f * Rn, 0.25f, 1.f);
				const bool bEdge = !On(I, x - 1, y) || !On(I, x + 1, y) || !On(I, x, y - 1) || !On(I, x, y + 1);
				const float Tb = bEdge ? Th * 0.3f : Th;
				const FVector P = O + FVector((x + 0.5f) * I.Cell, (y + 0.5f) * I.Cell, -Th + Tb * 0.5f + 0.5f);
				IceMesh->AddInstance(FTransform(FRotator(0.f, (Bit * 37) % 11 - 5.f, 0.f), P, FVector(I.Cell * (bEdge ? 0.9f : 1.04f) * Shrink / 100.f, I.Cell * (bEdge ? 0.9f : 1.04f) * Shrink / 100.f, Tb / 100.f)), true);
				if (bEdge)
				{
					// a dome over the edge cell (SM_M4_Ball, 1 m): as high as the ice in its middle, out to nothing past the cell
					const float R = I.Cell * (0.95f + 0.35f * Rn) * Shrink;
					const FVector Dc = O + FVector((x + 0.5f + (Rn - 0.5f) * 0.4f) * I.Cell, (y + 0.5f + (FMath::Frac(Rn * 7.31f) - 0.5f) * 0.4f) * I.Cell, -Th + 0.3f);
					IceRim->AddInstance(FTransform(FRotator(0.f, Rn * 360.f, 0.f), Dc, FVector(R * 2.f / 100.f, R * 1.6f / 100.f, Th * 1.7f / 100.f)), true);
				}
				++N;
			}
		}
	}
	IceMesh->SetVisibility(N > 0);
	IceRim->SetVisibility(N > 0);
}

void AAntMatterNet::WatchMess()
{
	const AAntMess* M = AAntMess::Get(this);
	if (!M) { return; }
	TSet<uint16> Now;
	for (const FMessStain& S : M->Stains)
	{
		Now.Add(S.Id);
		if (MessOn.Contains(S.Id)) { continue; }
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& On = MessOn.Add(S.Id);
		TArray<int32> Near;
		Query(FVector(S.Pos), S.Size * 0.5f, Near);
		const uint8 K = KindOfStain(S.Kind);
		for (int32 Idx : Near)
		{
			UPrimitiveComponent* C = Reg[Idx].C.Get();
			if (!C || C->Bounds.SphereRadius > 4000.f) { continue; }
			AntState::AddSpot(C, AntState::Mess, FVector(S.Pos), S.Size * 0.5f, (AntState::EKind)K, K == AntState::Dirt ? 0.75f : 0.85f);
			On.Add(C);
		}
	}
	// stains wiped away: what they were on is clean of them (the others on it are put back)
	TArray<uint16> Gone;
	for (const auto& It : MessOn) { if (!Now.Contains(It.Key)) { Gone.Add(It.Key); } }
	if (Gone.Num() == 0) { return; }
	TSet<UPrimitiveComponent*> Redo;
	for (uint16 Id : Gone)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& C : MessOn[Id]) { if (C.IsValid()) { Redo.Add(C.Get()); } }
		MessOn.Remove(Id);
	}
	for (UPrimitiveComponent* C : Redo) { AntState::ClearSource(C, AntState::Mess); }
	for (const FMessStain& S : M->Stains)
	{
		const TArray<TWeakObjectPtr<UPrimitiveComponent>>* On = MessOn.Find(S.Id);
		if (!On) { continue; }
		for (const TWeakObjectPtr<UPrimitiveComponent>& C : *On)
		{
			if (C.IsValid() && Redo.Contains(C.Get())) { AntState::AddSpot(C.Get(), AntState::Mess, FVector(S.Pos), S.Size * 0.5f, (AntState::EKind)KindOfStain(S.Kind), 0.8f); }
		}
	}
}

// =====================================================================================================================
// AntMatter
// =====================================================================================================================

namespace AntMatter
{

bool Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoMatter"));
	return !bOff && AntReact::Enabled();
}

void ResetAll(UWorld* W)
{
	if (!W || W->GetNetMode() == NM_Client || !Enabled()) { return; }
	AAntMatterNet* N = AAntMatterNet::Get(W);
	if (!N) { N = W->SpawnActor<AAntMatterNet>(AAntMatterNet::StaticClass(), FTransform::Identity); }
	if (N) { N->ResetMatter(); }
}

void OnShort(UWorld* W, const FVector& Where, const TCHAR* Why)
{
	AAntMatterNet* N = AAntMatterNet::Get(W);
	if (!N || !N->HasAuthority()) { return; }
	N->Electrify(Where, 260.f, 2.5f, Why);
	N->Spark(Where + FVector(0.f, 0.f, 20.f), FVector::UpVector, Why);
}

bool IceAt(const UWorld* W, const FVector& P, float Tolerance)
{
	const AAntMatterNet* N = AAntMatterNet::Get(W);
	if (!N) { return false; }
	for (const FAntMatterIce& I : N->Ices)
	{
		if (FMath::Abs((float)P.Z - I.Center.Z) > Tolerance + I.Thick) { continue; }
		const float Half = I.Cell * IceN * 0.5f;
		const int32 X = FMath::FloorToInt32(((float)P.X - (I.Center.X - Half)) / I.Cell);
		const int32 Y = FMath::FloorToInt32(((float)P.Y - (I.Center.Y - Half)) / I.Cell);
		if (X < 0 || Y < 0 || X >= IceN || Y >= IceN) { continue; }
		const int32 Bit = Y * IceN + X;
		if (I.Mask.IsValidIndex(Bit / 8) && (I.Mask[Bit / 8] & (1 << (Bit % 8)))) { return true; }
	}
	return false;
}

float FireNear(const UWorld* W, const FVector& P, float Radius, FVector* OutAt)
{
	const AAntMatterNet* N = AAntMatterNet::Get(W);
	if (!N) { return 0.f; }
	float Best = 0.f;
	for (const FAntMatterPool& Pl : N->Pools)
	{
		const float D = (float)FVector::Dist(FVector(Pl.Center), P) - Pl.Radius;
		if (D < Radius) { const float K = Pl.Power * (1.f - FMath::Max(0.f, D) / Radius); if (K > Best) { Best = K; if (OutAt) { *OutAt = Pl.Center; } } }
	}
	for (const FAntMatterRec& R : N->Recs)
	{
		if (!(R.Flags & AntMatterFlag::Burning)) { continue; }
		// the anchor is close enough for "keep away" (the exact flame is drawn from the component)
		const float D = (float)FVector::Dist(FVector(R.Anchor), P) - R.Fire * 4.f;
		if (D < Radius) { const float K = FMath::Clamp(R.Fire / 60.f, 0.2f, 1.f) * (1.f - FMath::Max(0.f, D) / Radius); if (K > Best) { Best = K; if (OutAt) { *OutAt = R.Anchor; } } }
	}
	return Best;
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || A[0].ToLower() != TEXT("matter")) { return false; }
	UWorld* W = GM->GetWorld();
	AAntMatterNet* N = AAntMatterNet::Get(W);
	if (!N && Enabled()) { ResetAll(W); N = AAntMatterNet::Get(W); }
	if (!N) { UE_LOG(LogTemp, Warning, TEXT("ANTMATTER off (level v3 only, not -AntNoMatter)")); return true; }
	const FString Op = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) && A[i].IsNumeric() ? FCString::Atof(*A[i]) : Def; };
	const FVector At(Num(2), Num(3), Num(4));
	auto Nearest = [N](const FVector& P, float R, bool bBurnable) -> UPrimitiveComponent*
	{
		TArray<int32> Near;
		N->Query(P, R, Near);
		UPrimitiveComponent* Best = nullptr;
		float BestD = 1.e12f;
		for (int32 Idx : Near)
		{
			UPrimitiveComponent* C = N->Reg[Idx].C.Get();
			if (!C || (bBurnable && (!N->Reg[Idx].M || N->Reg[Idx].M->IgniteAt <= 0.f))) { continue; }
			const float D = (float)C->Bounds.GetBox().ComputeSquaredDistanceToPoint(P) + 0.001f * (float)C->Bounds.SphereRadius;
			if (D < BestD) { BestD = D; Best = C; }
		}
		return Best;
	};
	if (Op == TEXT("fire") || Op == TEXT("ignite"))
	{
		UPrimitiveComponent* C = Nearest(At, Num(5, 150.f), true);
		const bool bOk = C && N->Ignite(C, At, TEXT("админ: поджечь"));
		if (!bOk && !N->LightPool(At, TEXT("админ: поджечь"))) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER fire at %s: nothing that burns there (%s)"), *At.ToCompactString(), *GetNameSafe(C)); }
	}
	else if (Op == TEXT("heat")) { N->AddHeat(At, Num(6, 120.f), Num(5, 600.f), Num(7, 6.f)); UE_LOG(LogTemp, Display, TEXT("ANTMATTER heat %.0f C at %s r %.0f for %.0f s"), Num(5, 600.f), *At.ToCompactString(), Num(6, 120.f), Num(7, 6.f)); }
	else if (Op == TEXT("wet"))
	{
		TArray<int32> Near;
		N->Query(At, Num(5, 120.f), Near);
		for (int32 Idx : Near) { if (UPrimitiveComponent* C = N->Reg[Idx].C.Get()) { N->WetBody(C, C->Bounds.GetBox().GetClosestPointTo(At), Num(5, 120.f), Num(6, 1.f), AntFluid::Water); } }
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER wet at %s r %.0f: %d things"), *At.ToCompactString(), Num(5, 120.f), Near.Num());
	}
	else if (Op == TEXT("soak") && A.Num() >= 6)
	{
		// matter soak oil|jam|cola|honey x y z [r]
		const FString L = A[2].ToLower();
		const FVector P(Num(3), Num(4), Num(5));
		const float R = Num(6, 120.f);
		const uint8 Liq = L == TEXT("oil") ? AntFluid::Oil : L == TEXT("cola") ? AntFluid::Cola : L == TEXT("honey") || L == TEXT("jam") ? AntFluid::Honey : AntFluid::Water;
		TArray<int32> Near;
		N->Query(P, R, Near);
		for (int32 Idx : Near)
		{
			UPrimitiveComponent* C = N->Reg[Idx].C.Get();
			if (!C) { continue; }
			if (L == TEXT("jam")) { AntState::AddSpot(C, AntState::Mess, P, R, AntState::Jam, 0.9f); }   // jam is not a liquid of the water: a smear
			else { N->WetBody(C, C->Bounds.GetBox().GetClosestPointTo(P), R, 1.f, Liq); }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER soak %s at %s r %.0f: %d things"), *L, *P.ToCompactString(), R, Near.Num());
	}
	else if (Op == TEXT("douse") || Op == TEXT("water")) { N->Douse(At, Num(5, 160.f), Num(6, 1.f), AntFluid::Water); }
	else if (Op == TEXT("freeze") || Op == TEXT("cold")) { N->Freeze(At, Num(5, 250.f)); }
	else if (Op == TEXT("short"))
	{
		if (AAntReactNet* RN = AAntReactNet::Get(W)) { RN->Short(At, TEXT("админ: короткое замыкание"), nullptr); }
		else { OnShort(W, At, TEXT("админ: короткое замыкание")); }
	}
	else if (Op == TEXT("live")) { N->Electrify(At, Num(5, 200.f), Num(6, 4.f), TEXT("админ: под током")); }
	else if (Op == TEXT("spark")) { N->Spark(At, FVector::UpVector, TEXT("админ: искра")); }
	else if (Op == TEXT("oil"))
	{
		if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(N)) { Fluid->Spill(AntFluid::Oil, At + FVector(0.f, 0.f, 30.f), Num(5, 0.4f) * Litre, 80.f); }
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER oil %.2f l spilt at %s"), Num(5, 0.4f), *At.ToCompactString());
	}
	else if (Op == TEXT("flour"))
	{
		if (AAntReactNet* RN = AAntReactNet::Get(W)) { RN->AddCloud(AntCloudKind::Flour, At, Num(5, 700.f), 14.f, FVector::ZeroVector, FVector::ZeroVector, 0.f, TEXT("matter")); }
	}
	else if (Op == TEXT("thing") && A.Num() >= 6)
	{
		// matter thing napkin|rag|pot|box|bread|plastic|board x y z [yaw] [sx sy sz]
		const FName What(*A[2].ToLower());
		const FVector P(Num(3), Num(4), Num(5));
		FVector Size(160.f, 160.f, 2.f);
		if (What == TEXT("rag") || What == TEXT("towel")) { Size = FVector(420.f, 260.f, 6.f); }
		else if (What == TEXT("pot") || What == TEXT("pan")) { Size = FVector(240.f, 240.f, 150.f); }
		else if (What == TEXT("box")) { Size = FVector(260.f, 200.f, 180.f); }
		else if (What == TEXT("bread")) { Size = FVector(120.f, 90.f, 50.f); }
		else if (What == TEXT("plastic") || What == TEXT("tub")) { Size = FVector(150.f, 150.f, 90.f); }
		else if (What == TEXT("board")) { Size = FVector(400.f, 260.f, 30.f); }
		if (A.Num() >= 10) { Size = FVector(Num(7, Size.X), Num(8, Size.Y), Num(9, Size.Z)); }
		const FTransform T(FRotator(0.f, Num(6, 0.f), 0.f), P + FVector(0.f, 0.f, Size.Z * 0.5f));
		AAntMatterThing* Th = W->SpawnActorDeferred<AAntMatterThing>(AAntMatterThing::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Th)
		{
			Th->What = What;
			Th->Size = Size;
			Th->FinishSpawning(T);
			N->RegisterNow(Th->Body);
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER thing %s [%s] at %s size %s"), *What.ToString(), *Th->MatId(), *P.ToCompactString(), *Size.ToCompactString());
		}
	}
	else if (Op == TEXT("info")) { N->LogAround(At, Num(5, 200.f)); }
	else if (Op == TEXT("find") && A.Num() >= 3)
	{
		// matter find <piece of a mesh name>: where those things are (for scripts)
		int32 n = 0;
		for (const AAntMatterNet::FReg& R : N->Reg)
		{
			const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(R.C.Get());
			if (!S || !S->GetStaticMesh() || !S->GetStaticMesh()->GetName().Contains(A[2])) { continue; }
			const FBox B = S->Bounds.GetBox();
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER find %s/%s mesh %s [%s] centre %s size %s"), *GetNameSafe(S->GetOwner()), *S->GetName(), *S->GetStaticMesh()->GetName(), R.M ? *R.M->Id : TEXT("?"), *B.GetCenter().ToCompactString(), *B.GetSize().ToCompactString());
			if (++n >= 20) { break; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER find '%s': %d"), *A[2], n);
	}
	else if (Op == TEXT("perf"))
	{
		// matter perf N x y z: N fires round the point (the cap applies), then watch "matter stats"
		const int32 Want = FMath::Clamp((int32)Num(2, 6.f), 1, 40);
		const FVector P(Num(3), Num(4), Num(5));
		TArray<int32> Near;
		N->Query(P, 2500.f, Near);
		int32 Lit = 0;
		for (int32 Idx : Near)
		{
			UPrimitiveComponent* C = N->Reg[Idx].C.Get();
			if (C && N->Reg[Idx].M && N->Reg[Idx].M->IgniteAt > 0.f && N->Ignite(C, C->Bounds.Origin, TEXT("тест производительности")) && ++Lit >= Want) { break; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER perf: %d fires lit of %d wanted"), Lit, Want);
	}
	else if (Op == TEXT("stats") || Op == TEXT("list"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER stats: %s, frame %.2f ms"), *N->Describe(), FApp::GetDeltaTime() * 1000.0);
	}
	else if (Op == TEXT("reset")) { N->ResetMatter(); }
	else if (Op == TEXT("test") && A.Num() >= 3)
	{
		// matter test door|towel|oil|ice|spark|perf|all: the verification scenes (KAN-21), each with its own camera and shots
		// (Saved/Screenshots/matter/<test>_<n>.png); every step is logged with ANTMATTER
		const FString T = A[2].ToLower();
		TWeakObjectPtr<AAntMatterNet> WN = N;
		auto FindMesh = [N](const FString& Piece, bool bExact) -> UPrimitiveComponent*
		{
			for (const AAntMatterNet::FReg& R : N->Reg)
			{
				UStaticMeshComponent* S = Cast<UStaticMeshComponent>(R.C.Get());
				if (!S || !S->IsVisible() || !S->GetStaticMesh()) { continue; }
				const FString Nm = S->GetStaticMesh()->GetName();
				if (bExact ? Nm == Piece : Nm.Contains(Piece)) { return S; }
			}
			return nullptr;
		};
		auto Cam = [W](const FVector& FromIn, const FVector& To, float Fov)
		{
			// pull the camera in front of whatever stands between it and the scene (a table, a curtain)
			FVector From = FromIn;
			const FVector Dir = (FromIn - To).GetSafeNormal();
			const float Want = (float)FVector::Dist(FromIn, To);
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterCam), false);
			for (TActorIterator<APawn> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
			if (W->LineTraceSingleByChannel(H, To + Dir * 160.f, FromIn, ECC_Visibility, Q)) { From = To + Dir * FMath::Max(260.f, (float)H.Distance + 160.f - 50.f); }
			if (Want - (float)FVector::Dist(From, To) > 1.f) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER test cam pulled in to %.0f uu (blocked by %s)"), (float)FVector::Dist(From, To), *GetNameSafe(H.GetActor())); }
			AntCloth::DevCam(W, From, To, Fov);
			if (APlayerController* PC = W->GetFirstPlayerController()) { if (PC->MyHUD) { PC->MyHUD->bShowHUD = false; } }
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER test cam %s -> %s fov %.0f"), *From.ToCompactString(), *To.ToCompactString(), Fov);
		};
		auto Shot = [W](const FString& Name) { AntCloth::DevShot(W, TEXT("matter"), Name); UE_LOG(LogTemp, Display, TEXT("ANTMATTER test shot %s"), *Name); };
		auto Floor = [W](const FVector& P) -> FVector
		{
			FHitResult H;
			if (W->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 400.f), P - FVector(0.f, 0.f, 3000.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(AntMatterTest), false))) { return H.ImpactPoint; }
			return P;
		};
		auto ToRoom = [](const FVector& Ctr, bool bX) { return bX ? FVector(0.f, Ctr.Y > 0.f ? -1.f : 1.f, 0.f) : FVector(Ctr.X > 0.f ? -1.f : 1.f, 0.f, 0.f); };
		// the first of the views (offsets from To) with nothing between it and To (a fat ray: a chair leg counts)
		auto FreeView = [W](const FVector& To, TArray<FVector> Offs) -> FVector
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterCam), false);
			for (TActorIterator<APawn> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
			for (const FVector& O : Offs)
			{
				FHitResult H;
				if (!W->SweepSingleByChannel(H, To + O.GetSafeNormal() * 120.f, To + O, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.f), Q)) { return To + O; }
			}
			return To + Offs[0];
		};
		// the side of a flat thing (a door) that faces the open room: the side with more free air in front of it (behind a
		// cupboard's door there is the cupboard and the wall)
		auto OpenSide = [W](UPrimitiveComponent* C, FVector Axis) -> FVector
		{
			const FBox B = C->Bounds.GetBox();
			const FVector Ctr = B.GetCenter(), Sz = B.GetSize();
			if (Axis.IsNearlyZero()) { Axis = Sz.X < Sz.Y ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 1.f, 0.f); }
			const float Half = (float)FVector::DotProduct(Sz, Axis.GetAbs()) * 0.5f;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterCam), false);
			for (TActorIterator<APawn> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
			Q.AddIgnoredComponent(C);
			float Free[2] = { 0.f, 0.f };
			for (int32 s = 0; s < 2; ++s)
			{
				const float Sg = s == 0 ? 1.f : -1.f;
				for (const float Hz : { -0.25f, 0.f, 0.25f })
				{
					FHitResult H;
					const FVector From = Ctr + Axis * Sg * (Half + 8.f) + FVector(0.f, 0.f, Sz.Z * Hz);
					Free[s] += W->LineTraceSingleByChannel(H, From, From + Axis * Sg * 3000.f, ECC_Visibility, Q) ? (float)H.Distance : 3000.f;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("ANTMATTER test: free air in front of %s: +%.0f / -%.0f"), *C->GetName(), Free[0] / 3.f, Free[1] / 3.f);
			return Axis * (Free[0] >= Free[1] ? 1.f : -1.f);
		};
		float T0 = 0.f;
		const bool bAll = T == TEXT("all");
		if (T == TEXT("door") || bAll)
		{
			N->Later(T0, [WN, FindMesh, Cam, Shot, Floor, OpenSide, FreeView]()
			{
				AAntMatterNet* N = WN.Get();
				UPrimitiveComponent* C = N ? FindMesh(TEXT("SM_K_Base1400_DoorL"), false) : nullptr;
				if (!C) { UE_LOG(LogTemp, Warning, TEXT("ANTMATTER test door: no door")); return; }
				const FBox B = C->Bounds.GetBox();
				const FVector Ctr = B.GetCenter(), Sz = B.GetSize();
				// the door's own axes (it may stand open at an angle: its world box says nothing of its face): the thin one of its
				// mesh is the face normal, which way of it faces the room is where there is free air
				FVector Face = OpenSide(C, FVector::ZeroVector), Along = FVector::CrossProduct(FVector::UpVector, Face).GetSafeNormal();
				float Wd = Sz.X >= Sz.Y ? (float)Sz.X : (float)Sz.Y;
				const float H = (float)Sz.Z;
				if (const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C); SM && SM->GetStaticMesh())
				{
					const FVector Ext = SM->GetStaticMesh()->GetBoundingBox().GetExtent() * SM->GetComponentScale().GetAbs();
					const FTransform& Tf = SM->GetComponentTransform();
					const FVector Ax[2] = { Tf.GetUnitAxis(EAxis::X), Tf.GetUnitAxis(EAxis::Y) };
					const int32 Thin = Ext.X <= Ext.Y ? 0 : 1;
					FVector Nn = Ax[Thin];
					Nn.Z = 0.f;
					if (Nn.Normalize())
					{
						Face = OpenSide(C, Nn);
						Along = FVector::CrossProduct(FVector::UpVector, Face).GetSafeNormal();
						Wd = (float)(Thin == 0 ? Ext.Y : Ext.X) * 2.f;
					}
				}
				const FVector L = Ctr - Along * Wd * 0.25f, R = Ctr + Along * Wd * 0.25f;
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test door: %s box %s size %s, face %s, width %.0f"), *C->GetName(), *Ctr.ToCompactString(), *Sz.ToCompactString(), *Face.ToCompactString(), Wd);
				// the left half wet (two splashes), a rag burning on the floor at the right half's foot
				N->WetBody(C, L + FVector(0.f, 0.f, H * 0.25f), Wd * 0.42f, 1.f, AntFluid::Water);
				N->WetBody(C, L - FVector(0.f, 0.f, H * 0.25f), Wd * 0.42f, 1.f, AntFluid::Water);
				const FVector RagAt = Floor(R + Face * 90.f);
				TArray<FString> Cmd = { TEXT("matter"), TEXT("thing"), TEXT("rag"), FString::SanitizeFloat(RagAt.X), FString::SanitizeFloat(RagAt.Y), FString::SanitizeFloat(RagAt.Z), FString::SanitizeFloat(Along.Rotation().Yaw), FString::SanitizeFloat(Wd * 0.45f), TEXT("150"), TEXT("6") };
				DevCommand(N->GetWorld()->GetAuthGameMode<AAntKitchenGameMode>(), Cmd);
				N->AddHeat(R - FVector(0.f, 0.f, H * 0.3f) + Face * 30.f, Wd * 0.16f, 520.f, 26.f);   // the rag's flames lick the door's foot
				{
					// square on to the door's face, the whole door and the rag at its foot in the frame
					const float Dist = FMath::Max(Wd, H) * 1.25f + 500.f;
					const FVector To = Ctr - FVector(0.f, 0.f, H * 0.1f);
					const FVector From = FreeView(To, { Face * Dist + FVector(0.f, 0.f, H * 0.15f), Face * Dist * 0.8f + Along * Wd * 0.3f + FVector(0.f, 0.f, H * 0.25f),
						Face * Dist * 0.8f - Along * Wd * 0.3f + FVector(0.f, 0.f, H * 0.25f), Face * Dist * 0.6f + FVector(0.f, 0.f, H * 0.1f) });
					UE_LOG(LogTemp, Display, TEXT("ANTMATTER test door: face %s"), *Face.ToCompactString());
					Cam(From, To, 55.f);
				}
				N->Later(0.6f, [WN, RagAt]() { if (AAntMatterNet* N2 = WN.Get()) { TArray<int32> Q; N2->Query(RagAt + FVector(0.f, 0.f, 4.f), 30.f, Q); for (int32 I : Q) { if (UPrimitiveComponent* X = N2->Reg[I].C.Get()) { if (Cast<AAntMatterThing>(X->GetOwner())) { N2->Ignite(X, RagAt, TEXT("тест: горящая тряпка")); } } } } });
				N->Later(1.f, [Shot]() { Shot(TEXT("door_0_wet_left")); });
				N->Later(12.f, [Shot]() { Shot(TEXT("door_1_burning_right")); });
				N->Later(24.f, [Shot]() { Shot(TEXT("door_2_burning_right")); });
				N->Later(28.f, [WN, R, Wd, Face]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Douse(R + Face * 40.f, Wd * 0.45f, 1.f); } });
				N->Later(28.6f, [Shot]() { Shot(TEXT("door_3_douse_steam")); });
				N->Later(36.f, [Shot, WN, Ctr, Wd]() { Shot(TEXT("door_4_half_wet_half_charred")); if (AAntMatterNet* N2 = WN.Get()) { N2->LogAround(Ctr, Wd * 0.6f); } });
			});
			T0 += 40.f;
		}
		if (T == TEXT("towel") || bAll)
		{
			N->Later(T0, [WN, FindMesh, Cam, Shot, ToRoom]()
			{
				AAntMatterNet* N = WN.Get();
				// the oven's towel (the one of run 2: the same scene to compare), not the first towel of the registry
				UPrimitiveComponent* C = nullptr;
				if (N)
				{
					float BestD = 1.e12f;
					for (const AAntMatterNet::FReg& Rg : N->Reg)
					{
						UStaticMeshComponent* S = Cast<UStaticMeshComponent>(Rg.C.Get());
						if (!S || !S->IsVisible() || !S->GetStaticMesh() || !S->GetStaticMesh()->GetName().Contains(TEXT("Towel"))) { continue; }
						const float D = (float)FVector::DistSquared(S->Bounds.Origin, FVector(-750.f, 2853.f, 718.f));
						if (D < BestD) { BestD = D; C = S; }
					}
				}
				if (!C) { UE_LOG(LogTemp, Warning, TEXT("ANTMATTER test towel: no towel")); return; }
				const FBox B = C->Bounds.GetBox();
				const FVector Ctr = B.GetCenter(), Sz = B.GetSize();
				const bool bX = Sz.X >= Sz.Y;
				const FVector Face = ToRoom(Ctr, bX);
				const float Big = (float)FMath::Max(Sz.X, FMath::Max(Sz.Y, Sz.Z));
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test towel: %s box %s size %s"), *C->GetName(), *Ctr.ToCompactString(), *Sz.ToCompactString());
				const FVector Corner(bX ? B.Min.X + Sz.X * 0.15f : Ctr.X, bX ? Ctr.Y : B.Min.Y + Sz.Y * 0.15f, B.Min.Z + 8.f);
				N->Ignite(C, Corner, TEXT("тест: огонь у края полотенца"));
				Cam(Ctr + Face * 2300.f + FVector(0.f, 0.f, 250.f), Ctr, 50.f);
				N->Later(1.5f, [Shot]() { Shot(TEXT("towel_0_catches")); });
				N->Later(7.f, [Shot]() { Shot(TEXT("towel_1_spreads")); });
				N->Later(12.f, [Shot]() { Shot(TEXT("towel_2_spreads")); });
				N->Later(13.f, [WN, Ctr, Big, Face]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Douse(Ctr + Face * 30.f - FVector(0.f, 0.f, Big * 0.2f), Big * 0.42f, 1.f); } });
				N->Later(13.5f, [Shot]() { Shot(TEXT("towel_3_steam")); });
				N->Later(20.f, [Shot, WN, Ctr, Big]() { Shot(TEXT("towel_4_out_charred_wet")); if (AAntMatterNet* N2 = WN.Get()) { N2->LogAround(Ctr, Big); } });
			});
			T0 += 24.f;
		}
		if (T == TEXT("oil") || bAll)
		{
			const FVector P0 = A.Num() >= 6 ? FVector(Num(3), Num(4), Num(5)) : FVector(-1500.f, 1700.f, 0.f);
			N->Later(T0, [WN, Cam, Shot, Floor, P0]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				const FVector P = Floor(P0);
				if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(N)) { Fluid->Spill(AntFluid::Oil, P + FVector(0.f, 0.f, 30.f), 0.6f * Litre, 80.f); }
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test oil: 0.6 l of oil at %s"), *P.ToCompactString());
				Cam(P + FVector(0.f, -1100.f, 650.f), P, 60.f);
				N->Later(4.f, [Shot]() { Shot(TEXT("oil_0_puddle")); });
				N->Later(5.f, [WN, P]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Spark(P + FVector(0.f, 0.f, 8.f), FVector::UpVector, TEXT("тест: искра над маслом")); } });
				N->Later(5.25f, [Shot]() { Shot(TEXT("oil_1_flash")); });
				N->Later(8.f, [Shot]() { Shot(TEXT("oil_2_burning_puddle")); });
				N->Later(11.f, [WN, P]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Douse(P + FVector(0.f, 0.f, 60.f), 160.f, 1.f); } });
				N->Later(11.35f, [Shot]() { Shot(TEXT("oil_3_water_flare")); });
				N->Later(16.f, [Shot, WN, P]() { Shot(TEXT("oil_4_after")); if (AAntMatterNet* N2 = WN.Get()) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER test oil: %s"), *N2->Describe()); } });
			});
			T0 += 20.f;
		}
		if (T == TEXT("ice") || bAll)
		{
			const FVector P0 = A.Num() >= 6 && !bAll ? FVector(Num(3), Num(4), Num(5)) : FVector(1500.f, 1500.f, 0.f);
			N->Later(T0, [WN, Cam, Shot, Floor, P0]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				const FVector P = Floor(P0);
				if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(N)) { Fluid->Spill(AntFluid::Water, P + FVector(0.f, 0.f, 30.f), 2.f * Litre, 120.f); }
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test ice: 2 l of water at %s"), *P.ToCompactString());
				Cam(P + FVector(0.f, -900.f, 520.f), P, 60.f);
				N->Later(5.f, [Shot]() { Shot(TEXT("ice_0_puddle")); });
				N->Later(6.f, [WN, P]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Freeze(P + FVector(0.f, 0.f, 5.f), 340.f); } });
				N->Later(7.f, [Shot, WN, P]()
				{
					Shot(TEXT("ice_1_frozen"));
					if (AAntMatterNet* N2 = WN.Get())
					{
						UE_LOG(LogTemp, Display, TEXT("ANTMATTER test ice: ice at the centre %d, slippery %d, ice sheets %d"), AntMatter::IceAt(N2->GetWorld(), P + FVector(0.f, 0.f, 3.f)) ? 1 : 0,
							UAntFluidWorld::Active(N2) && UAntFluidWorld::Active(N2)->IsSlippery(P + FVector(0.f, 0.f, 3.f)) ? 1 : 0, N2->Ices.Num());
					}
				});
			});
			T0 += 12.f;
		}
		if (T == TEXT("spark") || bAll)
		{
			N->Later(T0, [WN, FindMesh, Cam, Shot]()
			{
				AAntMatterNet* N = WN.Get();
				UPrimitiveComponent* Pot = N ? FindMesh(TEXT("SM_K_Pot"), true) : nullptr;
				if (!Pot) { UE_LOG(LogTemp, Warning, TEXT("ANTMATTER test spark: no pot")); return; }
				const FBox B = Pot->Bounds.GetBox();
				const FVector Ctr = B.GetCenter();
				const FVector Nap(B.Max.X + 18.f + 80.f, Ctr.Y - 40.f, B.Min.Z);
				TArray<FString> Cmd = { TEXT("matter"), TEXT("thing"), TEXT("napkin"), FString::SanitizeFloat(Nap.X), FString::SanitizeFloat(Nap.Y), FString::SanitizeFloat(Nap.Z), TEXT("12") };
				DevCommand(N->GetWorld()->GetAuthGameMode<AAntKitchenGameMode>(), Cmd);
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test spark: pot %s box %s, napkin at %s"), *Pot->GetName(), *Ctr.ToCompactString(), *Nap.ToCompactString());
				{
					// the first free view of pot + napkin (the counter and the hood block some of them)
					const FVector To = Ctr + FVector(200.f, 0.f, -120.f);
					const FVector Offs[] = { FVector(250.f, -1700.f, 700.f), FVector(300.f, -1100.f, 900.f), FVector(900.f, -900.f, 700.f),
						FVector(-700.f, -900.f, 700.f), FVector(200.f, -800.f, 1300.f), FVector(1100.f, -400.f, 500.f), FVector(200.f, -600.f, 400.f) };
					FVector Pick = To + Offs[0];
					FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatterCam), false);
					for (TActorIterator<APawn> It(N->GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
					for (const FVector& O : Offs)
					{
						FHitResult H;
						const FVector From = To + O;
						if (!N->GetWorld()->LineTraceSingleByChannel(H, To + O.GetSafeNormal() * 160.f, From, ECC_Visibility, Q)) { Pick = From; break; }
					}
					Cam(Pick, To, 45.f);
				}
				N->Later(1.f, [Shot]() { Shot(TEXT("spark_0_pot_napkin")); });
				N->Later(1.5f, [WN, Ctr]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Electrify(Ctr, 40.f, 5.f, TEXT("тест: кастрюля под током")); } });
				N->Later(2.3f, [Shot]() { Shot(TEXT("spark_1_live_pot")); });
				N->Later(5.f, [Shot]() { Shot(TEXT("spark_2_napkin_burning")); });
				N->Later(9.f, [Shot, WN, Ctr]() { Shot(TEXT("spark_3_after")); if (AAntMatterNet* N2 = WN.Get()) { N2->LogAround(Ctr, 300.f); } });
			});
			T0 += 12.f;
		}
		if (T == TEXT("flour") || bAll)
		{
			N->Later(T0, [WN, Cam, Shot, Floor, FreeView]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				const FVector P = Floor(FVector(2600.f, 1700.f, 0.f)) + FVector(0.f, 0.f, 420.f);
				if (AAntReactNet* RN = AAntReactNet::Get(N->GetWorld())) { RN->AddCloud(AntCloudKind::Flour, P, 650.f, 14.f, FVector::ZeroVector, FVector::ZeroVector, 0.f, TEXT("matter test")); }
				// (run 2: a chair leg right in front of the lens) the first view with nothing in the way
				// (run 3: from the table's side the bang threw its tablecloth over the lens: from the open side first)
				Cam(FreeView(P, { FVector(2400.f, -700.f, 600.f), FVector(-2400.f, -700.f, 600.f), FVector(1800.f, 1800.f, 600.f), FVector(0.f, 2400.f, 600.f),
					FVector(0.f, -2600.f, 500.f), FVector(0.f, -2000.f, 1300.f) }), P, 60.f);
				N->Later(2.5f, [Shot]() { Shot(TEXT("flour_0_cloud")); });
				N->Later(3.f, [WN, P]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Spark(P, FVector::UpVector, TEXT("тест: искра в мучном облаке")); } });
				N->Later(3.15f, [Shot]() { Shot(TEXT("flour_1_bang")); });
				N->Later(3.6f, [Shot]() { Shot(TEXT("flour_2_fireball")); });
				N->Later(8.f, [Shot]() { Shot(TEXT("flour_3_dust_settles")); });
			});
			T0 += 11.f;
		}
		if (T == TEXT("plastic") || bAll)
		{
			N->Later(T0, [WN, Cam, Shot, Floor]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				// (run 2: the tub stood in the ice test's puddle, cooled by it, and nothing read) a dry spot of floor
				FVector P = Floor(FVector(2200.f, 900.f, 0.f));
				if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(N))
				{
					const FVector Try[] = { FVector(2200.f, 900.f, 0.f), FVector(2200.f, -300.f, 0.f), FVector(3200.f, 300.f, 0.f), FVector(1200.f, -600.f, 0.f), FVector(-300.f, -900.f, 0.f), FVector(3400.f, -900.f, 0.f) };
					for (const FVector& T2 : Try)
					{
						const FVector Q = Floor(T2);
						AntFluid::FSample S;
						const bool bWet = Fluid->SampleAt(Q + FVector(0.f, 0.f, 30.f), S) && S.Total > 0.05f;
						if (!bWet && !AntMatter::IceAt(N->GetWorld(), Q, 200.f)) { P = Q; break; }
						UE_LOG(LogTemp, Display, TEXT("ANTMATTER test plastic: %s is wet, next"), *Q.ToCompactString());
					}
				}
				TArray<FString> Cmd = { TEXT("matter"), TEXT("thing"), TEXT("plastic"), FString::SanitizeFloat(P.X), FString::SanitizeFloat(P.Y), FString::SanitizeFloat(P.Z) };
				DevCommand(N->GetWorld()->GetAuthGameMode<AAntKitchenGameMode>(), Cmd);
				N->AddHeat(P + FVector(0.f, 0.f, 40.f), 160.f, 420.f, 24.f);   // a hot pan put down on it (run 2: 300 C reached the melting point only at the end)
				Cam(P + FVector(0.f, -700.f, 300.f), P + FVector(0.f, 0.f, 45.f), 50.f);
				N->Later(1.f, [Shot]() { Shot(TEXT("plastic_0_cold")); });
				N->Later(12.f, [Shot]() { Shot(TEXT("plastic_1_melting_smoke")); });
				N->Later(22.f, [Shot, WN, P]() { Shot(TEXT("plastic_2_sagged")); if (AAntMatterNet* N2 = WN.Get()) { N2->LogAround(P, 200.f); } });
			});
			T0 += 24.f;
		}
		if (T == TEXT("coop"))
		{
			// the co-op check (KAN-21 run 3): current through a puddle into a pot standing in it, a rag on fire, then put out (steam,
			// wet, char). All on the host; a client started with -AntMatterWatch logs and shoots what it draws of it.
			N->Later(T0, [WN, Cam, Shot, Floor, FreeView]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				AAntKitchenGameMode* GM = N->GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
				const FVector P = Floor(FVector(1500.f, -600.f, 0.f));
				if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(N)) { Fluid->Spill(AntFluid::Water, P + FVector(0.f, 0.f, 30.f), 1.f * Litre, 120.f); }
				const FVector Pot = P + FVector(60.f, 0.f, 0.f), Rag = Floor(P + FVector(-1300.f, 150.f, 0.f));   // (run 3a: at 650 the puddle reached it and soaked it)
				TArray<FString> C1 = { TEXT("matter"), TEXT("thing"), TEXT("pot"), FString::SanitizeFloat(Pot.X), FString::SanitizeFloat(Pot.Y), FString::SanitizeFloat(Pot.Z) };
				DevCommand(GM, C1);
				TArray<FString> C2 = { TEXT("matter"), TEXT("thing"), TEXT("rag"), FString::SanitizeFloat(Rag.X), FString::SanitizeFloat(Rag.Y), FString::SanitizeFloat(Rag.Z) };
				DevCommand(GM, C2);
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test coop: puddle at %s, pot in it, rag at %s"), *P.ToCompactString(), *Rag.ToCompactString());
				const FVector Mid = (P + Rag) * 0.5f;
				Cam(FreeView(Mid, { FVector(0.f, -1500.f, 700.f), FVector(-900.f, -1300.f, 800.f), FVector(900.f, -1300.f, 800.f), FVector(0.f, 1500.f, 700.f) }), Mid, 60.f);
				N->Later(4.f, [WN, P]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Electrify(P + FVector(0.f, 0.f, 3.f), 80.f, 9.f, TEXT("тест: ток по луже")); } });
				N->Later(5.f, [Shot]() { Shot(TEXT("coop_0_live_water")); });
				N->Later(6.f, [WN, Rag]() { if (AAntMatterNet* N2 = WN.Get()) { TArray<int32> Q; N2->Query(Rag + FVector(0.f, 0.f, 4.f), 40.f, Q); for (int32 I : Q) { if (UPrimitiveComponent* X = N2->Reg[I].C.Get()) { if (Cast<AAntMatterThing>(X->GetOwner())) { N2->Ignite(X, Rag, TEXT("тест: тряпка")); } } } } });
				N->Later(13.f, [Shot]() { Shot(TEXT("coop_1_fire")); });
				N->Later(15.f, [WN, Rag]() { if (AAntMatterNet* N2 = WN.Get()) { N2->Douse(Rag + FVector(0.f, 0.f, 40.f), 260.f, 1.f); } });
				N->Later(15.4f, [Shot]() { Shot(TEXT("coop_2_douse_steam")); });
				N->Later(19.f, [Shot, WN]() { Shot(TEXT("coop_3_after")); if (AAntMatterNet* N2 = WN.Get()) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER test coop: %s"), *N2->Describe()); } });
			});
			T0 += 21.f;
		}
		if (T == TEXT("perf") || bAll)
		{
			N->Later(T0, [WN, Cam, Shot]()
			{
				AAntMatterNet* N = WN.Get();
				if (!N) { return; }
				UWorld* W2 = N->GetWorld();
				if (APlayerController* PC = W2->GetFirstPlayerController()) { PC->ConsoleCommand(TEXT("stat unit"), false); }
				// (run 2 lit none: the earlier scenes already held the fire cap) a clean flat, then the cap's worth of fires in view
				N->ResetMatter();
				const FVector P(-2500.f, 3400.f, 1700.f);
				TArray<int32> Near;
				N->Query(P, 3500.f, Near);
				int32 Lit = 0;
				for (int32 Idx : Near)
				{
					UPrimitiveComponent* C = N->Reg[Idx].C.Get();
					const FAntMatProps* M = N->Reg[Idx].M;
					if (C && M && M->IgniteAt > 0.f && M->IgniteAt < 320.f && C->Bounds.SphereRadius < 900.f && N->Ignite(C, C->Bounds.Origin, TEXT("тест производительности")) && ++Lit >= MaxBurning) { break; }
				}
				UE_LOG(LogTemp, Display, TEXT("ANTMATTER test perf: %d fires"), Lit);
				Cam(P + FVector(0.f, -3800.f, 900.f), P, 70.f);
				for (int32 k = 1; k <= 8; ++k)
				{
					N->Later(k * 2.f, [WN, k]() { if (AAntMatterNet* N2 = WN.Get()) { UE_LOG(LogTemp, Display, TEXT("ANTMATTER perf %d: frame %.2f ms, %s"), k, FApp::GetDeltaTime() * 1000.0, *N2->Describe()); } });
				}
				// with the UI: "stat unit" is drawn over the picture (the other shots leave it out)
				auto ShotUI = [](const FString& Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("matter") / (Name + TEXT(".png")), true, false); UE_LOG(LogTemp, Display, TEXT("ANTMATTER test shot %s (with stat unit)"), *Name); };
				N->Later(6.f, [ShotUI]() { ShotUI(TEXT("perf_0_fires_stat_unit")); });
				N->Later(12.f, [ShotUI]() { ShotUI(TEXT("perf_1_fires_stat_unit")); });
			});
			T0 += 18.f;
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMATTER test %s scheduled (%.0f s)"), *T, T0);
	}
	else { UE_LOG(LogTemp, Warning, TEXT("ANTMATTER: fire|heat|wet|soak|douse|freeze|short|live|spark|oil|flour|thing|info|find|perf|stats|reset")); }
	return true;
}

}
