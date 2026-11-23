#include "AntInside.h"
#include "AntCharacter.h"
#include "AntCloth.h"
#include "AntKitchenGameMode.h"
#include "AntRun.h"
#include "AntAudio.h"
#include "AntVersus.h"
#include "AntTypes.h"
#include "HumanCharacter.h"
#include "HumanBody.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "QuadBody.h"
#include "KitchenFixture.h"
#include "KitchenLoot.h"
#include "NpcRoutine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "AntFighter.h"

float UAntInsideCamMod::Shake = 0.f;
float UAntInsideCamMod::Blur = 0.f;
float UAntInsideCamMod::Dark = 0.f;

namespace
{
	using namespace AntInsideSpot;
	using NpcRoutine::Dad;
	using NpcRoutine::Gran;
	using NpcRoutine::Kid;
	using NpcRoutine::Dog;

	// The entry points. Uvw: the segment mesh's own bounds, -1..1 (X forward, Y right, Z up in the bind pose).
	const AntInside::FSpotDef GDefs[] =
	{
		{ Ear,     Dad,  HumanSeg::Head,   FVector(-0.05f, -1.0f, 0.05f), 120.f, TEXT("в ухо"),          TEXT("ухо"),          25.f, 2 },
		{ Nose,    Dad,  HumanSeg::Head,   FVector(1.0f, 0.0f, -0.22f),   110.f, TEXT("в нос"),          TEXT("нос"),           8.f, 2 },
		{ Leg,     Dad,  HumanSeg::ShinL,  FVector(0.35f, 0.0f, -0.92f),  150.f, TEXT("в штанину"),      TEXT("штанина"),      30.f, 6 },
		{ Leg,     Dad,  HumanSeg::ShinR,  FVector(0.35f, 0.0f, -0.92f),  150.f, TEXT("в штанину"),      TEXT("штанина"),      30.f, 6 },
		{ Pocket,  Dad,  HumanSeg::ThighR, FVector(0.25f, 1.0f, 0.5f),    130.f, TEXT("в карман"),       TEXT("карман"),       40.f, 3 },
		{ Slipper, Dad,  HumanSeg::FootR,  FVector(-0.35f, 0.0f, 0.75f),  130.f, TEXT("в тапок"),        TEXT("тапок"),        30.f, 3 },
		{ Ear,     Gran, HumanSeg::Head,   FVector(-0.05f, -1.0f, 0.05f), 110.f, TEXT("в ухо"),          TEXT("ухо"),          25.f, 2 },
		{ Bun,     Gran, HumanSeg::Head,   FVector(-0.6f, 0.0f, 0.95f),   130.f, TEXT("в пучок"),        TEXT("пучок"),        40.f, 3 },
		{ Pocket,  Gran, HumanSeg::Belly,  FVector(0.95f, 0.55f, -0.4f),  120.f, TEXT("в карман кофты"), TEXT("карман кофты"), 40.f, 2 },
		{ Slipper, Gran, HumanSeg::FootR,  FVector(-0.35f, 0.0f, 0.75f),  120.f, TEXT("в тапок"),        TEXT("тапок"),        30.f, 2 },
		{ Pocket,  Kid,  HumanSeg::ThighR, FVector(0.25f, 1.0f, 0.5f),    110.f, TEXT("в карман шорт"),  TEXT("карман шорт"),  40.f, 2 },
		{ Collar,  Kid,  HumanSeg::Neck,   FVector(-1.0f, 0.0f, 0.6f),    110.f, TEXT("за шиворот"),     TEXT("ворот футболки"), 14.f, 2 },
		{ Collar,  Dog,  QuadSeg::Neck,    FVector(0.0f, 0.0f, 1.0f),     130.f, TEXT("под ошейник"),    TEXT("ошейник"),      30.f, 2 },
	};

	float NetNowW(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? (float)GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	}

	AAntKitchenGameMode* GMOf(UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	uint8 WhoOf(const AActor* Host)
	{
		if (const AHumanCharacter* H = Cast<AHumanCharacter>(Host))
		{
			return H->Fig == HumanFig::Grandma ? Gran : H->Fig == HumanFig::Kid ? Kid : Dad;
		}
		if (const ACritter* C = Cast<ACritter>(Host))
		{
			return C->Kind == 0 ? Dog : NpcRoutine::Cat;
		}
		return 255;
	}

	const TCHAR* WhoGen(uint8 Who)
	{
		return Who == Gran ? TEXT("бабушки") : Who == Kid ? TEXT("малыша") : Who == Dog ? TEXT("Бадрика") : TEXT("Человека");
	}

	const TCHAR* WhoDat(uint8 Who)
	{
		return Who == Gran ? TEXT("бабушке") : Who == Kid ? TEXT("малышу") : Who == Dog ? TEXT("Бадрику") : TEXT("Человеку");
	}

	UStaticMeshComponent* SegOf(const AActor* Host, int32 Seg)
	{
		if (const AHumanCharacter* H = Cast<AHumanCharacter>(Host)) { return H->HasJointedBody() ? H->GetSegment(Seg) : nullptr; }
		if (const ACritter* C = Cast<ACritter>(Host)) { return C->HasJointedBody() ? C->GetSegment(Seg) : nullptr; }
		return nullptr;
	}

	bool HostGone(const AActor* Host)
	{
		if (!IsValid(Host) || Host->IsHidden()) { return true; }
		if (const AHumanCharacter* H = Cast<AHumanCharacter>(Host)) { return H->IsAway() || H->IsKnockedOut(); }
		return false;
	}

	void SayFor(UWorld* W, const AActor* Host, const FString& Line)
	{
		const uint8 Who = WhoOf(Host);
		if (Who != 255 && Who != Dog) { NpcRoutine::Say(W, Who, Line); }
	}

	const TCHAR* LootName(uint8 Loot)
	{
		return Loot == 1 ? TEXT("Монетка") : Loot == 2 ? TEXT("Леденец") : Loot == 3 ? TEXT("Конфета") : TEXT("");
	}
}

// =====================================================================================================================

TConstArrayView<AntInside::FSpotDef> AntInside::Defs()
{
	return MakeArrayView(GDefs, UE_ARRAY_COUNT(GDefs));
}

bool AntInside::Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoInside"));
	return !bOff;
}

bool AntInside::SpotPoint(const AActor* Host, uint8 Def, FVector& Out, USceneComponent** OutComp, FVector* OutLocal)
{
	if (!Defs().IsValidIndex(Def))
	{
		return false;
	}
	UStaticMeshComponent* C = SegOf(Host, Defs()[Def].Seg);
	if (!C || !C->GetStaticMesh())
	{
		return false;
	}
	const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
	const FVector Local = B.Origin + Defs()[Def].Uvw * B.BoxExtent;
	Out = C->GetComponentTransform().TransformPosition(Local);
	if (OutComp) { *OutComp = C; }
	if (OutLocal) { *OutLocal = Local; }
	return true;
}

void AntInside::ResetAll(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || !Enabled())
	{
		return;
	}
	if (AAntInsideNet* N = AAntInsideNet::Get(World))
	{
		for (int32 i = N->Slots.Num() - 1; i >= 0; --i) { N->Exit(i, 0, FVector::ZeroVector, 0.f, TEXT("new raid")); }
		N->Fx.Reset();
	}
	for (TActorIterator<AAntInsideSpot> It(World); It; ++It) { It->Destroy(); }
	AAntInsideNet::Get(World, true);   // its tick puts the entry points on the residents
}

// ---- the entry point ---------------------------------------------------------------------------------------------------

AAntInsideSpot::AAntInsideSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);
	InteractRadius = 120.f;
}

void AAntInsideSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntInsideSpot, Host);
	DOREPLIFETIME(AAntInsideSpot, Def);
}

float AAntInsideSpot::DistanceTo(const FVector& P) const
{
	FVector S;
	if (HostGone(Host) || !AntInside::SpotPoint(Host, Def, S))
	{
		return TNumericLimits<float>::Max();
	}
	return (float)FVector::Dist(S, P);
}

bool AAntInsideSpot::CanInteract(const AAntCharacter* Ant) const
{
	if (!Ant || Ant->bDead || Ant->Creature != 0 || Ant->Team != 0 || HostGone(Host) || !AntInside::Defs().IsValidIndex(Def))
	{
		return false;
	}
	const AAntInsideNet* N = AAntInsideNet::Get(GetWorld());
	return !N || (N->SlotOf(Ant) < 0 && N->CountIn(Host, AntInside::Defs()[Def].Spot) < AntInside::Defs()[Def].Cap);
}

FString AAntInsideSpot::GetPrompt(const AAntCharacter* Ant) const
{
	const AntInside::FSpotDef& D = AntInside::Defs()[Def];
	return FString::Printf(TEXT("Залезть %s %s — [E]"), D.Ru, WhoDat(D.Who));
}

void AAntInsideSpot::Interact(AAntCharacter* Ant)
{
	if (AAntInsideNet* N = AAntInsideNet::Get(GetWorld(), true))
	{
		N->Enter(Ant, Host, Def, false);
	}
}

// ---- who is inside whom ------------------------------------------------------------------------------------------------

AAntInsideNet::AAntInsideNet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the bodies are posed: the effects go on top
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(20.f);
	SetHidden(false);   // (an info actor is hidden: its rooms, marks and flying slippers must show)
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("InsideRoot"));
}

void AAntInsideNet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntInsideNet, Slots);
	DOREPLIFETIME(AAntInsideNet, Fx);
}

AAntInsideNet* AAntInsideNet::Get(const UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntInsideNet> It(World); It; ++It) { return *It; }
	if (bCreate && World->GetNetMode() != NM_Client)
	{
		return const_cast<UWorld*>(World)->SpawnActor<AAntInsideNet>();
	}
	return nullptr;
}

void AAntInsideNet::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const TWeakObjectPtr<AAntCharacter>& A : Hidden) { if (A.IsValid() && !A->bCorpseGone) { A->SetActorHiddenInGame(false); } }
	Super::EndPlay(Reason);
}

float AAntInsideNet::NetNow() const { return NetNowW(GetWorld()); }

int32 AAntInsideNet::SlotOf(const AAntCharacter* Ant) const
{
	for (int32 i = 0; i < Slots.Num(); ++i) { if (Slots[i].Ant == Ant) { return i; } }
	return -1;
}

int32 AAntInsideNet::CountIn(const AActor* Host, uint8 Spot) const
{
	int32 N = 0;
	for (const FAntInsideSlot& S : Slots)
	{
		if (S.Host == Host && AntInside::Defs().IsValidIndex(S.Def) && (Spot == 255 || AntInside::Defs()[S.Def].Spot == Spot)) { ++N; }
	}
	return N;
}

float AAntInsideNet::FxWeight(const AActor* Host, uint8 Kind, float* OutT) const
{
	const float Now = NetNow();
	float W = 0.f;
	for (const FAntInsideFxState& F : Fx)
	{
		if (F.Host != Host || F.Kind != Kind || Now < F.Start || Now > F.Start + F.Dur) { continue; }
		const float T = (Now - F.Start) / FMath::Max(0.05f, F.Dur);
		if (OutT) { *OutT = T; }
		W = FMath::Max(W, FMath::Sin(PI * FMath::Clamp(T, 0.f, 1.f)));
	}
	return W;
}

void AAntInsideNet::StartFx(AActor* Host, uint8 Kind, float Dur)
{
	if (!HasAuthority() || !Host)
	{
		return;
	}
	const float Now = NetNow();
	Fx.RemoveAll([Now](const FAntInsideFxState& F) { return Now > F.Start + F.Dur + 1.f; });
	FAntInsideFxState F;
	F.Host = Host;
	F.Kind = Kind;
	F.Start = Now;
	F.Dur = Dur;
	Fx.Add(F);
}

bool AAntInsideNet::Enter(AAntCharacter* Ant, AActor* Host, uint8 Def, bool bForce)
{
	if (!HasAuthority() || !Ant || Ant->bDead || HostGone(Host) || !AntInside::Defs().IsValidIndex(Def) || SlotOf(Ant) >= 0)
	{
		return false;
	}
	const AntInside::FSpotDef& D = AntInside::Defs()[Def];
	if (!bForce && CountIn(Host, D.Spot) >= D.Cap)
	{
		Ant->Toast(TEXT("Там уже тесно"));
		return false;
	}
	FVector P, Local;
	USceneComponent* Comp = nullptr;
	if (!AntInside::SpotPoint(Host, Def, P, &Comp, &Local))
	{
		return false;
	}
	Ant->LetGo();
	Ant->SetNpcHold(Comp, Local);
	const float Now = NetNow();
	FAntInsideSlot S;
	S.Ant = Ant;
	S.Host = Host;
	S.Def = Def;
	S.Since = Now;
	if (D.Spot == Pocket)
	{
		int32& Taken = LootTaken.FindOrAdd(Host);
		const int32 Most = D.Who == Gran ? 3 : 1;
		if (Taken < Most) { S.Loot = D.Who == Dad ? 1 : D.Who == Gran ? 2 : 3; ++Taken; }
	}
	if (D.Spot == Nose)
	{
		S.ReactAt = Now + 2.4f;   // a tickle... and the sneeze
		S.React = 1;
		StartFx(Host, AntInsideFx::Tickle, 2.4f);
		SayFor(GetWorld(), Host, TEXT("А-а-а…"));
	}
	Slots.Add(S);
	AntAudio::Everyone(this, EAntSfx::CurtainRustle, P, 0.6f);
	const FString Hint = D.Spot == Pocket && S.Loot ? FString::Printf(TEXT(" Тут %s — вылезешь с ней!"), *FString(LootName(S.Loot)).ToLower()) : FString();
	Ant->Toast(FString::Printf(TEXT("Ты %s у %s! [ЛКМ/ПКМ] кусать · [E] вылезти.%s"), D.Ru, WhoGen(D.Who), *Hint));
	UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s enters the %s of %s (def %d, %d there now)%s"), *Ant->GetDisplayName(), D.Place, *Host->GetName(), Def, CountIn(Host, D.Spot), S.Loot ? *FString::Printf(TEXT(", finds loot %d"), S.Loot) : TEXT(""));
	AntFighter::OnInside(Ant, D.Spot == Ear ? 1 : 0, 0);   // XP, «Побывал в ухе» (AntFighter.h)
	return true;
}

void AAntInsideNet::Exit(int32 i, uint8 Mode, const FVector& Vel, float Damage, const TCHAR* Why)
{
	if (!HasAuthority() || !Slots.IsValidIndex(i))
	{
		return;
	}
	const FAntInsideSlot S = Slots[i];
	Slots.RemoveAt(i);
	AAntCharacter* A = S.Ant;
	if (!IsValid(A))
	{
		return;
	}
	FVector P = A->GetActorLocation();
	USceneComponent* Comp = nullptr;
	AntInside::SpotPoint(S.Host, S.Def, P, &Comp);
	if (A->NpcHold && (A->NpcHold == Comp || Comp == nullptr)) { A->SetNpcHold(nullptr, FVector::ZeroVector); }
	A->SetActorHiddenInGame(false);
	if (Mode == 2)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s is caught inside (%s)"), *A->GetDisplayName(), Why);
		return;
	}
	// out of the opening, a little away from the body
	FVector Out = P;
	if (Comp) { Out += (P - Comp->Bounds.Origin).GetSafeNormal() * 30.f; }
	A->ApplyTeleport(Out);
	if (A->bDead)
	{
		return;
	}
	if (S.Loot && !A->Carrying)
	{
		FAntFoodDef D;
		D.Name = LootName(S.Loot);
		D.Value = S.Loot == 1 ? 30 : S.Loot == 2 ? 12 : 22;
		D.Weight = S.Loot == 1 ? 3 : 2;
		D.Color = S.Loot == 1 ? FLinearColor(0.95f, 0.75f, 0.25f) : S.Loot == 2 ? FLinearColor(0.9f, 0.15f, 0.2f) : FLinearColor(0.85f, 0.25f, 0.6f);
		D.Shape = S.Loot == 1 ? EAntShape::Cylinder : EAntShape::Sphere;
		D.Scale = S.Loot == 1 ? FVector(0.22f, 0.22f, 0.04f) : FVector(0.15f);
		if (AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Out), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			L->InitFood(D);
			L->FinishSpawning(FTransform(Out));
			if (Mode == 0) { A->GrabLoot(L); }
			A->Toast(FString::Printf(TEXT("Украл из кармана: %s!"), *D.Name));
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s steals %s out of the pocket (%s)"), *A->GetDisplayName(), *D.Name, Mode == 0 ? TEXT("in the jaws") : TEXT("it falls out with him"));
			AntFighter::OnInside(A, 0, 1);   // «Украл из кармана Гиганта»
		}
	}
	if (Why && FCString::Strcmp(Why, TEXT("чих")) == 0)
	{
		AntFighter::OnInside(A, 0, 2);   // «Был чихнут» (AntFighter.h)
	}
	if (Mode == 1)
	{
		A->Stagger(1.0f, Vel);
		if (Damage > 0.f) { A->HurtBy(Damage, FString(Why)); }
		AntAudio::Everyone(this, EAntSfx::AntHurt, Out, 0.8f);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s out of the %s (%s): %s, launch %s, damage %.0f"), *A->GetDisplayName(), AntInside::Defs()[S.Def].Place, Why,
		Mode == 0 ? TEXT("crawled out") : TEXT("thrown out"), *Vel.ToCompactString(), Damage);
}

// ---- the residents react (server) ---------------------------------------------------------------------------------------

namespace
{
	/** Up off the seat first (the routine is interrupted: NpcRoutine picks it up again later). */
	void Unseat(AHumanCharacter* H)
	{
		if (H && H->Act.Kind == HumanAct::Sit) { H->EndBusy(); }
	}
	FVector HeadFwd(const AActor* Host)
	{
		return Host ? Host->GetActorForwardVector().GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
	}
}

void AAntInsideNet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	PostPose(DeltaSeconds);
	LocalView(DeltaSeconds);
}

void AAntInsideNet::ServerTick(float Dt)
{
	UWorld* W = GetWorld();
	// the entry points on every resident at home (new ones too: the admin's spawns)
	LocalClock += Dt;
	if (LocalClock >= 2.f && AntInside::Enabled())
	{
		LocalClock = 0.f;
		TSet<TPair<AActor*, uint8>> Have;
		for (TActorIterator<AAntInsideSpot> It(W); It; ++It) { if (IsValid(It->Host)) { Have.Add(TPair<AActor*, uint8>(It->Host, It->Def)); } }
		TArray<AActor*> Hosts;
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (It->HasJointedBody()) { Hosts.Add(*It); } }
		for (TActorIterator<ACritter> It(W); It; ++It) { if (It->Kind == 0 && It->HasJointedBody()) { Hosts.Add(*It); } }
		for (AActor* H : Hosts)
		{
			const uint8 Who = WhoOf(H);
			for (int32 d = 0; d < AntInside::Defs().Num(); ++d)
			{
				if (AntInside::Defs()[d].Who != Who || Have.Contains(TPair<AActor*, uint8>(H, (uint8)d))) { continue; }
				AAntInsideSpot* S = W->SpawnActorDeferred<AAntInsideSpot>(AAntInsideSpot::StaticClass(), FTransform(H->GetActorLocation()), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!S) { continue; }
				S->Host = H;
				S->Def = (uint8)d;
				S->InteractRadius = AntInside::Defs()[d].Radius;
				S->FinishSpawning(FTransform(H->GetActorLocation()));
			}
		}
	}
	const float Now = NetNow();
	for (int32 i = Slots.Num() - 1; i >= 0; --i)
	{
		if (Slots.IsValidIndex(i)) { ServerSlot(i, Now); }
	}
	Fx.RemoveAll([Now](const FAntInsideFxState& F) { return Now > F.Start + F.Dur + 2.f; });
}

void AAntInsideNet::ServerSlot(int32 i, float Now)
{
	UWorld* W = GetWorld();
	FAntInsideSlot& S = Slots[i];
	AAntCharacter* A = S.Ant;
	AActor* Host = S.Host;
	if (!IsValid(A) || A->bDead || !A->NpcHold)
	{
		Slots.RemoveAt(i);   // it died in there, or something else took it
		return;
	}
	if (HostGone(Host))
	{
		Exit(i, 0, FVector::ZeroVector, 0.f, TEXT("the resident is gone"));
		return;
	}
	const AntInside::FSpotDef& D = AntInside::Defs()[S.Def];
	AHumanCharacter* H = Cast<AHumanCharacter>(Host);
	const float T = (Now - S.Since) / FMath::Max(0.2f, AntFighter::Mul(A, AntStat::InsideTime));   // «Ушной агент», «Ухогрыз»: they notice later (AntFighter.h)
	if (S.ReactAt > 0.f && Now >= S.ReactAt)
	{
		React(i, Now);
		return;
	}
	switch (D.Spot)
	{
	case Ear:
	{
		float& Next = NextShake.FindOrAdd(Host);
		if (Now >= Next)
		{
			Next = Now + 3.f;
			StartFx(Host, AntInsideFx::HeadShake, 0.8f);
			if (!Spoke.Contains(Host)) { Spoke.Add(Host); SayFor(W, Host, TEXT("Что-то в ухе…")); }
		}
		if (S.ReactAt < 0.f && (T > 9.f || S.Bites >= 2 || (CountIn(Host, Ear) >= 2 && T > 4.f)))
		{
			S.ReactAt = Now + 0.5f;   // the hand goes up: the slap lands 0.4 s after that
			S.React = 2;
			A->Toast(TEXT("Он тянется к уху! [E] — вылезай!"));
		}
		break;
	}
	case Leg:
	{
		float& Next = NextShake.FindOrAdd(Host);
		if (Now >= Next && T > 1.5f && H)
		{
			Next = Now + 4.f;
			const int32 InLegs = CountIn(Host, Leg);
			if (InLegs >= 3)
			{
				// three of us in his trousers: he dances, stumbles and goes down
				Unseat(H);
				StartFx(Host, AntInsideFx::LegDance, 1.2f);
				SayFor(W, Host, TEXT("Ай-ай-ай! Кто у меня в штанах?!"));
				if (H->Topple(HeadFwd(H), TEXT("муравьи в штанине")))
				{
					UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s falls: %d ants in his trouser legs"), *H->GetName(), InLegs);
				}
				for (int32 k = Slots.Num() - 1; k >= 0; --k)
				{
					if (Slots[k].Host == Host && AntInside::Defs()[Slots[k].Def].Spot == Leg) { Exit(k, 1, (FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 500.f + FVector(0.f, 0.f, 350.f)), 0.f, TEXT("he fell")); }
				}
				return;
			}
			if (FMath::FRand() < 0.5f)
			{
				// he shakes the leg: a 50 % chance for each of us to drop out
				Unseat(H);
				StartFx(Host, AntInsideFx::LegDance, 1.0f);
				H->Stun(1.2f, A, TEXT("муравей в штанине"), 1);
				for (int32 k = Slots.Num() - 1; k >= 0; --k)
				{
					if (Slots[k].Host == Host && AntInside::Defs()[Slots[k].Def].Spot == Leg && FMath::FRand() < 0.5f) { Exit(k, 1, FVector(0.f, 0.f, 250.f) + HeadFwd(H) * 300.f, 0.f, TEXT("shaken out of the leg")); }
				}
				return;
			}
		}
		break;
	}
	case Pocket:
		if (T > D.MaxTime && S.ReactAt < 0.f)
		{
			if (D.Who == Kid)
			{
				// the kid reaches into the pocket for his sweet: and takes the ant (carries it off as he does)
				Exit(i, 2, FVector::ZeroVector, 0.f, TEXT("the kid's hand"));
				if (AKidCharacter* K = Cast<AKidCharacter>(Host)) { K->DirectorGrab(A); }
				SayFor(W, Host, TEXT("Ой, жучок!"));
				return;
			}
			S.ReactAt = Now + 0.6f;
			S.React = 4;
			if (H && H->Act.Kind != HumanAct::Sit) { FVector P; if (AntInside::SpotPoint(Host, S.Def, P)) { H->PlayReach(P); } }
		}
		break;
	case Bun:
	{
		float& Next = NextBun.FindOrAdd(Host);
		if (Next <= 0.f) { Next = Now + 30.f; }
		if (Now >= Next && S.ReactAt < 0.f)
		{
			Next = Now + 30.f;
			StartFx(Host, AntInsideFx::BunFix, 1.2f);
			S.ReactAt = Now + 0.9f;
			S.React = 6;
		}
		break;
	}
	default:
		break;
	}
	if (T > D.MaxTime + 2.f && S.ReactAt < 0.f)
	{
		// too long in there: it gets shaken out one way or another
		if (H) { Unseat(H); H->Jolt(-HeadFwd(H), 0.6f, 2); }
		if (ACritter* C = Cast<ACritter>(Host)) { C->Bitten(nullptr); }
		Exit(i, 1, FVector(0.f, 0.f, 300.f) + HeadFwd(Host) * 250.f, 0.f, TEXT("shaken out"));
	}
}

void AAntInsideNet::React(int32 i, float Now)
{
	UWorld* W = GetWorld();
	FAntInsideSlot& S = Slots[i];
	AActor* Host = S.Host;
	AHumanCharacter* H = Cast<AHumanCharacter>(Host);
	const AntInside::FSpotDef& D = AntInside::Defs()[S.Def];
	const uint8 Code = S.React;
	S.ReactAt = -1.f;
	S.React = 0;
	FVector Spot;
	AntInside::SpotPoint(Host, S.Def, Spot);
	auto ExitAll = [&](uint8 SpotKind, float Chance, const FVector& Vel, float Dmg, const TCHAR* Why)
	{
		int32 N = 0;
		for (int32 k = Slots.Num() - 1; k >= 0; --k)
		{
			if (Slots[k].Host != Host || AntInside::Defs()[Slots[k].Def].Spot != SpotKind) { continue; }
			if (FMath::FRand() < Chance) { Exit(k, 1, Vel, Dmg, Why); ++N; }
			else if (Slots[k].Ant) { Slots[k].Ant->Toast(TEXT("Удержался!")); Slots[k].Bites = 0; }
		}
		return N;
	};
	switch (Code)
	{
	case 1:
	{
		// the sneeze: everything in the nose shoots out, far (a free ride across the room)
		const bool bAngry = S.Bites > 0;
		const FVector Fwd = HeadFwd(Host);
		StartFx(Host, AntInsideFx::Sneeze, 0.9f);
		StartFx(Host, AntInsideFx::EyesWater, 4.5f);
		SayFor(W, Host, TEXT("А-а-а… АПЧХИ!"));
		AntAudio::Everyone(this, EAntSfx::Sneeze, Spot, 1.f);
		if (AAntKitchenGameMode* GM = GMOf(W)) { GM->MakeNoise(Spot, 0.7f, AntNoise::Scream, Host, nullptr); }
		if (H)
		{
			const bool bDozing = H->Act.Kind == HumanAct::Sit && H->Act.Arg == 1;
			H->Jolt(-Fwd, 1.f, 1);
			if (bDozing) { Unseat(H); H->SetRage((uint8)FMath::Min<int32>(3, H->Rage + 1)); }
		}
		const FVector Vel = Fwd * (bAngry ? 2600.f : 2100.f) + FVector(0.f, 0.f, bAngry ? 1300.f : 1100.f);
		const int32 N = ExitAll(Nose, 1.f, Vel, 0.f, TEXT("чих"));
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s SNEEZES: %d ants shot out at %s"), *Host->GetName(), N, *Vel.ToCompactString());
		break;
	}
	case 2:
		// the ear: the hand goes up to it (the tell); the slap lands 0.4 s later
		if (H)
		{
			Unseat(H);
			H->PlayReach(Spot);
		}
		S.ReactAt = Now + 0.4f;
		S.React = 3;
		break;
	case 3:
	{
		StartFx(Host, AntInsideFx::EarSlap, 0.6f);
		AntAudio::Everyone(this, EAntSfx::EarSlap, Spot, 1.f);
		SayFor(W, Host, TEXT("Ай! Да что ж такое!"));
		const FVector Side = Host->GetActorRightVector() * -1.f;   // out of the left ear
		const int32 N = ExitAll(Ear, 0.75f, Side * 900.f + FVector(0.f, 0.f, 450.f), 35.f, TEXT("шлепок по уху"));
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s slaps his own ear: %d ants flicked out"), *Host->GetName(), N);
		break;
	}
	case 4:
	{
		AntAudio::Everyone(this, EAntSfx::PocketRustle, Spot, 1.f);
		const int32 N = ExitAll(Pocket, 0.8f, HeadFwd(Host) * 350.f + FVector(0.f, 0.f, 200.f), 0.f, TEXT("похлопал по карману"));
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s pats his pocket: %d ants out"), *Host->GetName(), N);
		break;
	}
	case 5:
		// the slipper: he kicks it off (the ant flies with it)
		if (H) { Unseat(H); H->TryKick(); }
		SayFor(W, Host, TEXT("Пошёл вон из тапка!"));
		S.ReactAt = Now + HumanAct::KickHit;
		S.React = 51;
		break;
	case 51:
	{
		const FVector Vel = HeadFwd(Host) * 2300.f + FVector(0.f, 0.f, 1150.f);
		StartFx(Host, AntInsideFx::SlipperFly, 1.5f);   // (the flying slipper every machine draws)
		AntAudio::Everyone(this, EAntSfx::SlipperSlap, Spot, 0.8f);
		const int32 N = ExitAll(Slipper, 1.f, Vel, 0.f, TEXT("пинок тапком"));
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s kicks his slipper off: %d ants fly with it"), *Host->GetName(), N);
		break;
	}
	case 6:
		ExitAll(Bun, 0.7f, FVector(0.f, 0.f, 120.f) + HeadFwd(Host) * 200.f, 0.f, TEXT("поправила пучок"));
		break;
	case 7:
		ExitAll(Collar, 1.f, FVector(0.f, 0.f, 500.f) - HeadFwd(Host) * 300.f, 0.f, TEXT("вытряс из-за шиворота"));
		break;
	case 8:
		ExitAll(Collar, 1.f, FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 700.f + FVector(0.f, 0.f, 500.f), 10.f, TEXT("Бадрик отряхнулся"));
		break;
	default:
		break;
	}
}

void AAntInsideNet::OnBite(int32 i)
{
	if (!HasAuthority() || !Slots.IsValidIndex(i))
	{
		return;
	}
	UWorld* W = GetWorld();
	FAntInsideSlot& S = Slots[i];
	AAntCharacter* A = S.Ant;
	AActor* Host = S.Host;
	AHumanCharacter* H = Cast<AHumanCharacter>(Host);
	const AntInside::FSpotDef& D = AntInside::Defs()[S.Def];
	const float Now = NetNow();
	S.Bites++;
	AntAudio::Everyone(this, EAntSfx::Bite, A->GetActorLocation(), 1.f);
	AntAudio::Excite(this, 0.25f, A->GetActorLocation());
	UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s bites the %s of %s from inside (bite %d)"), *A->GetDisplayName(), D.Place, *Host->GetName(), S.Bites);
	if (D.Spot == Bun) { AntFighter::OnInside(A, 0, 3); }   // «Довёл бабушку до вязания» (AntFighter.h)
	switch (D.Spot)
	{
	case Ear:
		StartFx(Host, AntInsideFx::HeadShake, 1.2f);
		if (H)
		{
			H->Stun(2.f, A, TEXT("укус в ухе"), 0);
			if (H->Held) { H->ThrowHeld(FVector(0.f, 0.f, -1.f) + H->GetActorRightVector() * 0.5f); }
		}
		SayFor(W, Host, D.Who == Gran ? TEXT("Ой-ой! В ухе кто-то!") : TEXT("А-а-а! В ухе кто-то кусается!"));
		if (AAntKitchenGameMode* GM = GMOf(W)) { GM->MakeNoise(Host->GetActorLocation(), 0.6f, AntNoise::Scream, Host, A); }
		if (S.ReactAt < 0.f) { S.ReactAt = Now + 1.1f; S.React = 2; A->Toast(TEXT("Он тянется к уху! [E] — вылезай!")); }
		break;
	case Nose:
		S.ReactAt = FMath::Min(S.ReactAt > 0.f ? S.ReactAt : Now + 1.f, Now + 0.5f);
		S.React = 1;
		StartFx(Host, AntInsideFx::Tickle, 0.5f);
		break;
	case Leg:
	{
		StartFx(Host, AntInsideFx::LegDance, 0.9f);
		if (H) { Unseat(H); H->Stun(1.5f, A, TEXT("укус под штаниной"), 1); }
		float& Last = LegBites.FindOrAdd(Host);
		SayFor(W, Host, TEXT("Ай! Нога!"));
		if (Now - Last < 2.5f || CountIn(Host, Leg) >= 3) { NextShake.FindOrAdd(Host) = Now; }   // bites in a row: the dance comes now
		Last = Now;
		break;
	}
	case Pocket:
		if (D.Who == Kid)
		{
			StartFx(Host, AntInsideFx::Giggle, 2.5f);
			if (H) { H->Panic(2.5f); }
			AntAudio::Everyone(this, EAntSfx::KidGiggle, Host->GetActorLocation(), 1.f);
			SayFor(W, Host, TEXT("Хи-хи! Щекотно!"));
		}
		else
		{
			if (H) { H->Jolt(-HeadFwd(H), 0.6f, 3); }
			SayFor(W, Host, D.Who == Gran ? TEXT("Ох! Мышь в кармане?!") : TEXT("Ай! Что там в кармане?!"));
			if (S.ReactAt < 0.f)
			{
				S.ReactAt = Now + 0.7f;
				S.React = 4;
				if (H && H->Act.Kind != HumanAct::Sit) { H->PlayReach(A->GetActorLocation()); }
			}
		}
		break;
	case Slipper:
		if (S.ReactAt < 0.f) { S.ReactAt = Now + 0.25f; S.React = 5; }
		break;
	case Bun:
		StartFx(Host, AntInsideFx::Spin, 1.6f);
		if (H) { H->Jolt(HeadFwd(H), 0.9f, 2); }
		SayFor(W, Host, TEXT("Ой! Кто там в волосах?!"));
		// the knitting drops: the needle comes out, the scarf runs (the chain C6)
		for (TActorIterator<AKitchenFixture> It(W); It; ++It)
		{
			if (It->FixId() != TEXT("knitting") || It->State != AKitchenFixture::Fixed) { continue; }
			for (TActorIterator<AKitchenFastener> F(W); F; ++F)
			{
				if (!F->bDone && FVector::Dist(F->GetActorLocation(), It->GetActorLocation()) < 900.f) { F->ForceDone(A); UE_LOG(LogTemp, Display, TEXT("ANTINSIDE grandma drops her knitting (needle %d out)"), F->Def); break; }
			}
		}
		if (S.ReactAt < 0.f) { S.ReactAt = Now + 1.6f; S.React = 6; }
		break;
	case Collar:
		if (D.Who == Dog)
		{
			if (ACritter* C = Cast<ACritter>(Host)) { C->Bitten(A); }
			StartFx(Host, AntInsideFx::HeadShake, 1.f);
			if (S.ReactAt < 0.f) { S.ReactAt = Now + 0.5f; S.React = 8; }
		}
		else
		{
			if (H) { H->DevAct(TEXT("scream"), H->GetActorLocation()); }
			SayFor(W, Host, TEXT("А-а-а! Жук за шиворотом!"));
			if (S.ReactAt < 0.f) { S.ReactAt = Now + 1.3f; S.React = 7; }
		}
		break;
	default:
		break;
	}
}

bool AAntInsideNet::SelfSwat(AHumanCharacter* H)
{
	if (!HasAuthority() || !H || CountIn(H, 255) == 0)
	{
		return false;
	}
	const bool bPlayer = H->GetController() && H->GetController()->IsA<APlayerController>();
	if (bPlayer && AntVersus::Active(GetWorld()) && !AntVersus::Spend(H, 10.f))
	{
		return true;   // no patience left: he only twitches
	}
	// the worst of it first: the ear, the nose, the leg, the pocket, the slipper
	const uint8 Order[] = { Ear, Nose, Leg, Pocket, Slipper, Bun, Collar };
	const float Now = NetNow();
	for (uint8 Sp : Order)
	{
		for (int32 k = 0; k < Slots.Num(); ++k)
		{
			if (Slots[k].Host != H || AntInside::Defs()[Slots[k].Def].Spot != Sp) { continue; }
			FAntInsideSlot& S = Slots[k];
			S.ReactAt = Now;
			S.React = Sp == Ear ? 2 : Sp == Nose ? 1 : Sp == Pocket ? 4 : Sp == Slipper ? 5 : Sp == Bun ? 6 : 7;
			if (Sp == Leg)
			{
				S.ReactAt = -1.f;
				NextShake.FindOrAdd(H) = Now;   // shakes the leg this tick
			}
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE %s swats at himself ([F]): the %s"), *H->GetName(), AntInside::Defs()[S.Def].Place);
			return true;
		}
	}
	return true;
}

// ---- every machine: the body effects on top of the pose ------------------------------------------------------------------

void AAntInsideNet::PostPose(float Dt)
{
	const float Now = NetNow();
	for (const FAntInsideFxState& F : Fx)
	{
		if (!IsValid(F.Host) || Now < F.Start || Now > F.Start + F.Dur)
		{
			continue;
		}
		const float T = Now - F.Start;
		const float U = FMath::Clamp(T / FMath::Max(0.05f, F.Dur), 0.f, 1.f);
		const float Env = FMath::Sin(PI * U);
		if (F.Kind == AntInsideFx::SlipperFly)
		{
			if (!SlippersShown.Contains(F.Start))
			{
				SlippersShown.Add(F.Start);
				if (const AHumanCharacter* H = Cast<AHumanCharacter>(F.Host))
				{
					if (UStaticMeshComponent* Foot = H->GetSegment(HumanSeg::FootR))
					{
						FlyingSlipper(Foot->Bounds.Origin, H->GetActorForwardVector().GetSafeNormal2D() * 2300.f + FVector(0.f, 0.f, 1150.f));
					}
				}
			}
			continue;
		}
		const bool bDog = F.Host->IsA<ACritter>();
		UStaticMeshComponent* Head = SegOf(F.Host, bDog ? (int32)QuadSeg::Head : (int32)HumanSeg::Head);
		if (!Head)
		{
			continue;
		}
		FRotator R = FRotator::ZeroRotator;
		switch (F.Kind)
		{
		case AntInsideFx::HeadShake:
			R.Yaw = 26.f * Env * FMath::Sin(T * 2.f * PI * 3.4f);
			R.Roll = (bDog ? 34.f : 9.f) * Env * FMath::Sin(T * 2.f * PI * (bDog ? 6.f : 3.4f) + 1.f);
			break;
		case AntInsideFx::Tickle:
			R.Pitch = 17.f * U + 1.5f * FMath::Sin(T * 41.f);
			break;
		case AntInsideFx::Sneeze:
			R.Pitch = U < 0.12f ? FMath::Lerp(17.f, -34.f, U / 0.12f) : -34.f * FMath::Square(1.f - (U - 0.12f) / 0.88f);
			break;
		case AntInsideFx::EarSlap:
			R.Roll = 20.f * FMath::Exp(-6.f * T) * FMath::Sin(T * 26.f + 1.2f);
			break;
		case AntInsideFx::BunFix:
			R.Roll = 13.f * Env + 3.f * FMath::Sin(T * 21.f);
			break;
		case AntInsideFx::Giggle:
			R.Pitch = 7.f * Env * FMath::Sin(T * 28.f);
			break;
		case AntInsideFx::Spin:
			R.Yaw = 48.f * Env * FMath::Sin(T * 2.f * PI * 1.3f);
			R.Roll = 8.f * Env * FMath::Sin(T * 17.f);
			break;
		case AntInsideFx::LegDance:
			R.Pitch = -8.f * Env;   // he looks down at his leg
			break;
		default:
			break;
		}
		if (!R.IsNearlyZero(0.01f))
		{
			Head->AddLocalRotation(R);
		}
	}
	// the slippers in flight (every machine draws its own)
	for (int32 k = Flying.Num() - 1; k >= 0; --k)
	{
		FFlying& S = Flying[k];
		UStaticMeshComponent* C = S.C.Get();
		if (!C) { Flying.RemoveAt(k); continue; }
		S.T += Dt;
		if (S.T > 7.f) { C->DestroyComponent(); Flying.RemoveAt(k); continue; }
		if (S.V.IsNearlyZero()) { continue; }
		S.V.Z -= 9000.f * Dt;
		FVector P = C->GetComponentLocation() + S.V * Dt;
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("Slipper"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, C->GetComponentLocation(), P - FVector(0.f, 0.f, 30.f), ECC_WorldStatic, Q))
		{
			P = Hit.ImpactPoint + FVector(0.f, 0.f, 30.f);
			S.V = FVector::ZeroVector;
			AntAudio::Play(this, EAntSfx::SoftThud, P, 0.8f);
		}
		C->SetWorldLocationAndRotation(P, C->GetComponentRotation() + (S.V.IsNearlyZero() ? FRotator::ZeroRotator : FRotator(520.f * Dt, 90.f * Dt, 260.f * Dt)));
	}
}

void AAntInsideNet::FlyingSlipper(const FVector& From, const FVector& Vel)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(HumanBody::Mesh(HumanSeg::FootR, HumanFig::Dad));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(GetRootComponent());
	C->SetAbsolute(true, true, true);
	C->RegisterComponent();
	HumanBody::ApplyLook(C, HumanFig::Dad);
	C->SetWorldLocation(From);
	Flying.Add({ C, Vel, 0.f });
}

// ---- the local view: the ants inside are hidden; the one we play looks at its little dark room -----------------------------

void AAntInsideNet::LocalView(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	// Dev, any machine (a client too): -AntLocalDo="t:shot name;t:cam x y z tx ty tz fov;t:camoff;t:clothlog" at seconds of this world.
	{
		static TArray<TPair<float, FString>> Script;
		static bool bParsed = false;
		if (!bParsed)
		{
			bParsed = true;
			FString S;
			if (FParse::Value(FCommandLine::Get(), TEXT("AntLocalDo="), S, false))
			{
				TArray<FString> Items;
				S.ParseIntoArray(Items, TEXT(";"));
				for (const FString& It : Items)
				{
					FString L, R;
					if (It.Split(TEXT(":"), &L, &R)) { Script.Add(TPair<float, FString>(FCString::Atof(*L), R)); }
				}
			}
		}
		const float T = W->GetTimeSeconds();
		for (int32 k = Script.Num() - 1; k >= 0; --k)
		{
			if (T < Script[k].Key) { continue; }
			TArray<FString> A;
			Script[k].Value.ParseIntoArrayWS(A);
			Script.RemoveAt(k);
			if (A.Num() == 0) { continue; }
			auto Nm = [&A](int32 i) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : 0.f; };
			if (A[0] == TEXT("shot"))
			{
				AntCloth::DevShot(W, TEXT("inside"), A.IsValidIndex(1) ? A[1] : FString(TEXT("local")));
				UE_LOG(LogTemp, Display, TEXT("ANTINSIDE local shot %s at server t %.1f: shake %.2f blur %.2f dark %.2f"), A.IsValidIndex(1) ? *A[1] : TEXT("-"), Now, UAntInsideCamMod::Shake, UAntInsideCamMod::Blur, UAntInsideCamMod::Dark);
			}
			else if (A[0] == TEXT("cam")) { AntCloth::DevCam(W, FVector(Nm(1), Nm(2), Nm(3)), FVector(Nm(4), Nm(5), Nm(6)), A.IsValidIndex(7) ? Nm(7) : 70.f); }
			else if (A[0] == TEXT("camoff")) { AntCloth::DevCam(W, FVector::ZeroVector, FVector::ZeroVector, 0.f); }
			else if (A[0] == TEXT("clothlog"))
			{
				for (TActorIterator<AAntCloth> It(W); It; ++It) { UE_LOG(LogTemp, Display, TEXT("ANTCLOTH local (%s) %s at %s"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"), *It->Describe(), *It->Bounds().GetCenter().ToCompactString()); }
				UE_LOG(LogTemp, Display, TEXT("ANTINSIDE local (%s): %d inside, shake %.2f blur %.2f dark %.2f"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"), Slots.Num(), UAntInsideCamMod::Shake, UAntInsideCamMod::Blur, UAntInsideCamMod::Dark);
			}
		}
	}
	TSet<TWeakObjectPtr<AAntCharacter>> In;
	for (const FAntInsideSlot& S : Slots)
	{
		if (!IsValid(S.Ant)) { continue; }
		In.Add(S.Ant);
		if (!S.Ant->IsHidden()) { S.Ant->SetActorHiddenInGame(true); }
		Hidden.Add(S.Ant);
	}
	for (auto It = Hidden.CreateIterator(); It; ++It)
	{
		if (!In.Contains(*It))
		{
			if (It->IsValid() && !(*It)->bCorpseGone && !(*It)->IsInNest()) { (*It)->SetActorHiddenInGame(false); }
			It.RemoveCurrent();
		}
	}
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (!CamMod.IsValid() && PC->PlayerCameraManager)
	{
		UCameraModifier* M = PC->PlayerCameraManager->FindCameraModifierByClass(UAntInsideCamMod::StaticClass());
		CamMod = M ? M : PC->PlayerCameraManager->AddNewCameraModifier(UAntInsideCamMod::StaticClass());
	}
	APawn* Pawn = PC->GetPawn();
	float Shake = 0.f, Blur = 0.f, Dark = 0.f;
	const AAntCharacter* MeAnt = Cast<AAntCharacter>(Pawn);
	const int32 Mine = MeAnt ? SlotOf(MeAnt) : -1;
	if (Mine >= 0)
	{
		const FAntInsideSlot& S = Slots[Mine];
		const AntInside::FSpotDef& D = AntInside::Defs()[S.Def];
		const int32 Key = D.Spot * 8 + D.Who;
		if (RoomBuilt != Key) { BuildRoom(D.Spot, D.Who); LastBites = S.Bites; }
		if (!bViewing && RoomCam)
		{
			PC->SetViewTargetWithBlend(RoomCam, 0.35f);
			bViewing = true;
			ViewSince = Now;
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE view: inside the %s (local camera in its room)"), D.Place);
		}
		ViewHost = S.Host;
		float HostFx = 0.f;
		for (uint8 K : { (uint8)AntInsideFx::HeadShake, (uint8)AntInsideFx::Sneeze, (uint8)AntInsideFx::EarSlap, (uint8)AntInsideFx::LegDance, (uint8)AntInsideFx::Spin, (uint8)AntInsideFx::BunFix, (uint8)AntInsideFx::Giggle })
		{
			HostFx = FMath::Max(HostFx, FxWeight(S.Host, K));
		}
		const float Speed = IsValid(S.Host) ? (float)S.Host->GetVelocity().Size() / 800.f : 0.f;
		Shake = FMath::Clamp(0.12f * Speed + HostFx, 0.f, 1.f);
		Dark = 1.f;
		if (S.Bites != LastBites) { LastBites = S.Bites; BiteFlash = 0.3f; }
		BiteFlash = FMath::Max(0.f, BiteFlash - Dt);
		// the room breathes: the camera sways, the hairs move, the heart beats (louder when he is upset)
		const float T = Now - ViewSince;
		if (RoomCam)
		{
			const FVector Base = RoomAt + FVector(-125.f, 0.f, 48.f);
			const FVector Sway(2.f * FMath::Sin(T * 1.3f), 1.5f * FMath::Sin(T * 1.7f), 1.2f * FMath::Sin(T * 2.1f));
			const FVector Jolt = FVector(FMath::PerlinNoise1D(T * 9.f), FMath::PerlinNoise1D(T * 11.f + 3.f), FMath::PerlinNoise1D(T * 13.f + 7.f)) * 14.f * Shake;
			RoomCam->SetActorLocationAndRotation(Base + Sway + Jolt, (RoomAt + FVector(90.f, 0.f, 22.f) - (Base + Sway)).Rotation() + FRotator(0.f, 0.f, 4.f * Shake * FMath::Sin(T * 23.f)));
		}
		for (int32 k = 0; k < Swaying.Num(); ++k)
		{
			if (Swaying[k] && SwayBase.IsValidIndex(k)) { Swaying[k]->SetRelativeRotation(SwayBase[k] + FRotator(6.f * FMath::Sin(T * 2.2f + k), 0.f, 6.f * FMath::Sin(T * 1.7f + k * 0.7f)) * (1.f + 3.f * Shake)); }
		}
		const float Beat = FMath::Fmod(T * (1.1f + 0.8f * Shake), 1.f);
		const float Pulse = FMath::Exp(-30.f * FMath::Square(Beat - 0.1f)) + 0.6f * FMath::Exp(-30.f * FMath::Square(Beat - 0.32f));
		if (RoomLight2) { RoomLight2->SetIntensity(6.f + 30.f * Pulse); }
		if (RoomLight) { RoomLight->SetIntensity(22.f + 25.f * BiteFlash); }
		if (PuppetJaw) { PuppetJaw->SetRelativeScale3D(FVector(0.05f, 0.09f + 0.06f * BiteFlash / 0.3f, 0.025f)); }
		BeatClock -= Dt;
		if (BeatClock <= 0.f)
		{
			BeatClock = 1.f / (1.1f + 0.8f * Shake);
			AntAudio::Play2D(this, EAntSfx::Heartbeat, 0.7f + 0.5f * Shake);
		}
	}
	else if (bViewing)
	{
		bViewing = false;
		if (Pawn) { PC->SetViewTargetWithBlend(Pawn, 0.25f); }
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE view: out again"));
	}
	// the Human player with ants inside him (versus): the screen shakes, the eyes water, the ear is muffled
	if (AHumanCharacter* HP = Cast<AHumanCharacter>(Pawn))
	{
		Shake = FMath::Max3(FxWeight(HP, AntInsideFx::HeadShake), FxWeight(HP, AntInsideFx::Sneeze), 0.8f * FxWeight(HP, AntInsideFx::EarSlap));
		Shake = FMath::Max3(Shake, 0.6f * FxWeight(HP, AntInsideFx::LegDance), 0.3f * FxWeight(HP, AntInsideFx::Tickle));
		Blur = FMath::Max(FxWeight(HP, AntInsideFx::EyesWater), 0.4f * FxWeight(HP, AntInsideFx::Tickle));
		const bool bEar = CountIn(HP, Ear) > 0;
		Dark = bEar ? 0.35f : 0.f;
		DuckClock -= Dt;
		if (bEar && DuckClock <= 0.f)
		{
			DuckClock = 0.5f;
			AntAudio::Duck(this, 0.7f);   // muffled: the music sinks
			AntAudio::Play2D(this, EAntSfx::Heartbeat, 0.5f);
		}
	}
	auto Ease = [Dt](float& V, float To) { V += (To - V) * FMath::Min(1.f, 6.f * Dt); };
	Ease(UAntInsideCamMod::Shake, Shake);
	Ease(UAntInsideCamMod::Blur, Blur);
	Ease(UAntInsideCamMod::Dark, Dark);
}

// ---- the little rooms (local, cosmetic): built from the kit's shapes far under the flat --------------------------------------

void AAntInsideNet::BuildRoom(uint8 Spot, uint8 Who)
{
	UWorld* W = GetWorld();
	for (UStaticMeshComponent* C : RoomParts) { if (C) { C->DestroyComponent(); } }
	RoomParts.Reset();
	Swaying.Reset();
	SwayBase.Reset();
	if (RoomLight) { RoomLight->DestroyComponent(); RoomLight = nullptr; }
	if (RoomLight2) { RoomLight2->DestroyComponent(); RoomLight2 = nullptr; }
	// high above and far from the flat (under it the height fog is a wall), shut in a dark shell (no sun inside a pocket)
	RoomAt = FVector(90000.f + Spot * 6000.f, 90000.f + Who * 6000.f, 30000.f);
	if (!RoomRoot)
	{
		RoomRoot = NewObject<USceneComponent>(this);
		RoomRoot->SetupAttachment(GetRootComponent());
		RoomRoot->SetAbsolute(true, true, true);
		RoomRoot->RegisterComponent();
	}
	RoomRoot->SetWorldLocation(RoomAt);
	FRandomStream R(Spot * 31 + Who * 7 + 5);
	auto Part = [&](EAntShape Shape, const FVector& Loc, const FVector& Size, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator, bool bSway = false) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, RoomRoot, Shape, Loc, Size / 100.f, Color, Rot, false, false);
		if (C)
		{
			C->SetCastShadow(false);
			RoomParts.Add(C);
			if (bSway) { Swaying.Add(C); SwayBase.Add(Rot); }
		}
		return C;
	};
	auto Light = [&](const FVector& Loc, float Cd, const FLinearColor& Color, float Radius) -> UPointLightComponent*
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RoomRoot);
		L->SetRelativeLocation(Loc);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Cd);
		L->SetLightColor(Color);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	};
	// A tube along +X around the ant (the canal, the nostril, the trouser leg): slabs around a ring, the ant on its floor.
	auto Tube = [&](float Rad, float Len, int32 Slabs, TFunctionRef<FLinearColor(int32)> Color)
	{
		for (int32 k = 0; k < Slabs; ++k)
		{
			const float A = 2.f * PI * k / Slabs;
			const FVector C(Len * 0.5f - 60.f, FMath::Cos(A) * Rad, Rad + FMath::Sin(A) * Rad);
			const float Wd = 2.f * PI * Rad / Slabs * 1.3f;
			Part(EAntShape::Cube, C, FVector(Len, Wd, 4.f), Color(k), FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) + 90.f));
		}
	};
	auto Hairs = [&](float Rad, float Len, int32 N, float HairLen, const FLinearColor& Color, bool bInward)
	{
		for (int32 k = 0; k < N; ++k)
		{
			const float A = R.FRandRange(0.f, 2.f * PI);
			const float X = R.FRandRange(0.f, Len - 120.f);
			const FVector At(X, FMath::Cos(A) * Rad * 0.96f, Rad + FMath::Sin(A) * Rad * 0.96f);
			const FVector Dir = bInward ? (FVector(X + R.FRandRange(-60.f, 60.f), 0.f, Rad) - At).GetSafeNormal() : FVector(0.f, FMath::Cos(A), FMath::Sin(A));
			Part(EAntShape::Cylinder, At + Dir * HairLen * 0.5f, FVector(2.2f, 2.2f, HairLen), Color, FRotationMatrix::MakeFromZ(Dir).Rotator(), true);
		}
	};
	const FLinearColor Skin(0.86f, 0.62f, 0.52f);
	{
		const FLinearColor Shell(0.015f, 0.01f, 0.01f);
		const FVector C(300.f, 0.f, 200.f), E(1100.f, 800.f, 700.f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			if (UStaticMeshComponent* A = Part(EAntShape::Cube, C + FVector(s * E.X, 0.f, 0.f), FVector(20.f, 2.f * E.Y, 2.f * E.Z), Shell)) { A->SetCastShadow(true); }
			if (UStaticMeshComponent* A = Part(EAntShape::Cube, C + FVector(0.f, s * E.Y, 0.f), FVector(2.f * E.X, 20.f, 2.f * E.Z), Shell)) { A->SetCastShadow(true); }
			if (UStaticMeshComponent* A = Part(EAntShape::Cube, C + FVector(0.f, 0.f, s * E.Z), FVector(2.f * E.X, 2.f * E.Y, 20.f), Shell)) { A->SetCastShadow(true); }
		}
	}
	switch (Spot)
	{
	case Ear:
		Tube(70.f, 620.f, 18, [&](int32 k) { return FLinearColor(0.80f + 0.05f * (k % 3), 0.42f, 0.38f); });
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = R.FRandRange(PI * 0.9f, PI * 2.1f);
			Part(EAntShape::Sphere, FVector(R.FRandRange(60.f, 480.f), FMath::Cos(A) * 62.f, 70.f + FMath::Sin(A) * 62.f), FVector(R.FRandRange(16.f, 34.f)), FLinearColor(0.88f, 0.62f, 0.14f));   // wax
		}
		Hairs(70.f, 560.f, 22, 38.f, FLinearColor(0.35f, 0.22f, 0.15f), true);
		Part(EAntShape::Cylinder, FVector(570.f, 0.f, 70.f), FVector(140.f, 140.f, 3.f), FLinearColor(0.95f, 0.75f, 0.70f), FRotator(90.f, 0.f, 0.f));   // the eardrum
		RoomLight = Light(FVector(10.f, 0.f, 60.f), 22.f, FLinearColor(1.f, 0.62f, 0.5f), 500.f);
		RoomLight2 = Light(FVector(520.f, 0.f, 70.f), 10.f, FLinearColor(1.f, 0.3f, 0.25f), 420.f);
		break;
	case Nose:
		Tube(64.f, 560.f, 16, [&](int32 k) { return FLinearColor(0.66f, 0.27f + 0.03f * (k % 2), 0.28f); });
		Hairs(64.f, 520.f, 55, 58.f, FLinearColor(0.12f, 0.08f, 0.06f), true);
		for (int32 k = 0; k < 4; ++k) { Part(EAntShape::Sphere, FVector(R.FRandRange(120.f, 420.f), R.FRandRange(-40.f, 40.f), 8.f), FVector(R.FRandRange(14.f, 26.f), R.FRandRange(14.f, 26.f), 8.f), FLinearColor(0.62f, 0.72f, 0.3f)); }
		RoomLight = Light(FVector(-150.f, 0.f, 70.f), 30.f, FLinearColor(0.95f, 0.95f, 1.f), 700.f);   // the nostril's opening, behind
		RoomLight2 = Light(FVector(420.f, 0.f, 60.f), 8.f, FLinearColor(0.9f, 0.25f, 0.2f), 400.f);
		break;
	case Leg:
		// the pyjama trouser leg: striped fabric all round, the giant hairy shin up the middle
		Tube(230.f, 900.f, 26, [&](int32 k) { return (k % 2) ? FLinearColor(0.30f, 0.42f, 0.72f) : FLinearColor(0.86f, 0.87f, 0.93f); });
		Part(EAntShape::Cylinder, FVector(380.f, 0.f, 250.f), FVector(210.f, 210.f, 900.f), Skin, FRotator(90.f, 0.f, 0.f));
		for (int32 k = 0; k < 45; ++k)
		{
			const float A = R.FRandRange(0.f, 2.f * PI);
			const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
			const FVector At = FVector(R.FRandRange(-40.f, 800.f), 0.f, 250.f) + Dir * 105.f;
			Part(EAntShape::Cylinder, At + Dir * 22.f, FVector(2.5f, 2.5f, 48.f), FLinearColor(0.16f, 0.1f, 0.07f), FRotationMatrix::MakeFromZ(Dir + FVector(0.3f, 0.f, 0.f)).Rotator(), true);
		}
		RoomLight = Light(FVector(-160.f, 0.f, 40.f), 40.f, FLinearColor(1.f, 0.95f, 0.85f), 900.f);   // the hem: light from below
		RoomLight2 = Light(FVector(600.f, 0.f, 200.f), 5.f, FLinearColor(1.f, 0.5f, 0.4f), 500.f);
		break;
	case Pocket:
	{
		const FLinearColor Cloth = Who == Gran ? FLinearColor(0.45f, 0.22f, 0.40f) : Who == Kid ? FLinearColor(0.16f, 0.30f, 0.62f) : FLinearColor(0.30f, 0.34f, 0.52f);
		Part(EAntShape::Cube, FVector(120.f, 0.f, -2.f), FVector(460.f, 360.f, 4.f), Cloth * 0.9f);
		Part(EAntShape::Cube, FVector(120.f, -180.f, 100.f), FVector(460.f, 4.f, 220.f), Cloth, FRotator(0.f, 0.f, -12.f));
		Part(EAntShape::Cube, FVector(120.f, 180.f, 100.f), FVector(460.f, 4.f, 220.f), Cloth, FRotator(0.f, 0.f, 12.f));
		Part(EAntShape::Cube, FVector(350.f, 0.f, 100.f), FVector(4.f, 360.f, 220.f), Cloth * 0.8f);
		Part(EAntShape::Cube, FVector(60.f, 60.f, 215.f), FVector(380.f, 200.f, 4.f), Cloth * 0.7f, FRotator(0.f, 0.f, 8.f));   // the top: a slit of light beside it
		for (int32 k = 0; k < 9; ++k) { Part(EAntShape::Sphere, FVector(R.FRandRange(20.f, 320.f), R.FRandRange(-150.f, 150.f), 10.f), FVector(R.FRandRange(14.f, 32.f)), FLinearColor(0.62f, 0.62f, 0.64f)); }   // lint
		if (Who == Dad)
		{
			Part(EAntShape::Cylinder, FVector(110.f, 35.f, 6.f), FVector(60.f, 60.f, 6.f), FLinearColor(0.95f, 0.75f, 0.25f), FRotator(0.f, 0.f, 8.f));   // the coin
			Part(EAntShape::Cube, FVector(230.f, -60.f, 10.f), FVector(110.f, 30.f, 6.f), FLinearColor(0.72f, 0.73f, 0.76f), FRotator(0.f, 25.f, 0.f));   // keys
			Part(EAntShape::Cylinder, FVector(285.f, -90.f, 6.f), FVector(50.f, 50.f, 5.f), FLinearColor(0.72f, 0.73f, 0.76f));
		}
		else if (Who == Gran)
		{
			for (int32 k = 0; k < 3; ++k) { Part(EAntShape::Sphere, FVector(100.f + 70.f * k, -40.f + 50.f * k, 22.f), FVector(40.f), k == 1 ? FLinearColor(0.2f, 0.7f, 0.25f) : FLinearColor(0.9f, 0.15f, 0.2f)); }
			Part(EAntShape::Cube, FVector(260.f, 70.f, 6.f), FVector(150.f, 90.f, 4.f), FLinearColor(0.92f, 0.9f, 0.85f), FRotator(0.f, 20.f, 0.f));   // a handkerchief
		}
		else
		{
			Part(EAntShape::Sphere, FVector(130.f, 30.f, 22.f), FVector(44.f), FLinearColor(0.85f, 0.25f, 0.6f));
			Part(EAntShape::Cone, FVector(85.f, 30.f, 22.f), FVector(24.f, 24.f, 30.f), FLinearColor(0.95f, 0.85f, 0.3f), FRotator(90.f, 0.f, 0.f));
			Part(EAntShape::Cone, FVector(175.f, 30.f, 22.f), FVector(24.f, 24.f, 30.f), FLinearColor(0.95f, 0.85f, 0.3f), FRotator(-90.f, 0.f, 0.f));
			Part(EAntShape::Cylinder, FVector(240.f, -70.f, 8.f), FVector(14.f, 14.f, 160.f), FLinearColor(0.95f, 0.8f, 0.1f), FRotator(90.f, 30.f, 0.f));   // a crayon
		}
		RoomLight = Light(FVector(60.f, -110.f, 260.f), 26.f, FLinearColor(1.f, 0.95f, 0.85f), 700.f);
		RoomLight2 = Light(FVector(200.f, 0.f, 60.f), 3.f, FLinearColor(0.8f, 0.6f, 0.5f), 400.f);
		break;
	}
	case Slipper:
	{
		const FLinearColor Pink = Who == Gran ? FLinearColor(0.35f, 0.65f, 0.35f) : FLinearColor(0.95f, 0.52f, 0.68f);
		Part(EAntShape::Cube, FVector(150.f, 0.f, -2.f), FVector(520.f, 320.f, 4.f), Pink * 0.8f);
		Part(EAntShape::Cube, FVector(200.f, 0.f, 180.f), FVector(420.f, 320.f, 4.f), Pink, FRotator(-10.f, 0.f, 0.f));
		Part(EAntShape::Cube, FVector(150.f, -160.f, 90.f), FVector(520.f, 4.f, 190.f), Pink);
		Part(EAntShape::Cube, FVector(150.f, 160.f, 90.f), FVector(520.f, 4.f, 190.f), Pink);
		for (int32 k = 0; k < 5; ++k) { Part(EAntShape::Sphere, FVector(330.f + 10.f * (k % 2), -120.f + 60.f * k, 55.f - 6.f * FMath::Abs(k - 1)), FVector(k == 0 ? 130.f : 90.f - 6.f * k), Skin); }   // toes
		for (int32 k = 0; k < 14; ++k) { Part(EAntShape::Sphere, FVector(R.FRandRange(0.f, 300.f), R.FRandRange(-150.f, 150.f), R.FRandRange(150.f, 175.f)), FVector(R.FRandRange(20.f, 40.f)), FLinearColor(0.97f, 0.95f, 0.93f), FRotator::ZeroRotator, true); }   // fluff
		RoomLight = Light(FVector(-90.f, 0.f, 150.f), 30.f, FLinearColor(1.f, 0.9f, 0.85f), 700.f);
		RoomLight2 = Light(FVector(280.f, 0.f, 80.f), 4.f, FLinearColor(1.f, 0.6f, 0.5f), 300.f);
		break;
	}
	case Bun:
		Part(EAntShape::Cube, FVector(150.f, 0.f, -2.f), FVector(600.f, 600.f, 4.f), Skin * 1.05f);
		for (int32 k = 0; k < 80; ++k)
		{
			const FRotator Rt(R.FRandRange(-80.f, 80.f), R.FRandRange(0.f, 360.f), 0.f);
			Part(EAntShape::Cylinder, FVector(R.FRandRange(-60.f, 380.f), R.FRandRange(-260.f, 260.f), R.FRandRange(30.f, 260.f)), FVector(4.f, 4.f, R.FRandRange(220.f, 420.f)), FLinearColor(0.74f, 0.74f, 0.77f) * R.FRandRange(0.8f, 1.05f), Rt, k % 4 == 0);
		}
		Part(EAntShape::Cube, FVector(150.f, 40.f, 140.f), FVector(380.f, 8.f, 8.f), FLinearColor(0.45f, 0.45f, 0.5f), FRotator(12.f, 20.f, 0.f));   // a hairpin
		RoomLight = Light(FVector(-60.f, 0.f, 180.f), 20.f, FLinearColor(1.f, 0.92f, 0.82f), 700.f);
		RoomLight2 = Light(FVector(250.f, 0.f, 80.f), 4.f, FLinearColor(1.f, 0.7f, 0.6f), 300.f);
		break;
	case Collar:
	default:
	{
		const bool bDog = Who == Dog;
		Part(EAntShape::Cube, FVector(150.f, 0.f, -2.f), FVector(600.f, 500.f, 4.f), bDog ? FLinearColor(0.82f, 0.62f, 0.38f) : Skin);
		if (bDog)
		{
			for (int32 k = 0; k < 70; ++k)
			{
				Part(EAntShape::Cylinder, FVector(R.FRandRange(-40.f, 420.f), R.FRandRange(-220.f, 220.f), 50.f), FVector(4.f, 4.f, R.FRandRange(80.f, 130.f)), FLinearColor(0.84f, 0.64f, 0.38f) * R.FRandRange(0.85f, 1.1f), FRotator(R.FRandRange(-35.f, 35.f), 0.f, R.FRandRange(-35.f, 35.f)), k % 3 == 0);
			}
			Part(EAntShape::Cube, FVector(150.f, 0.f, 170.f), FVector(700.f, 520.f, 30.f), FLinearColor(0.72f, 0.07f, 0.06f));   // the red collar overhead
			Part(EAntShape::Cylinder, FVector(300.f, 0.f, 120.f), FVector(110.f, 110.f, 8.f), FLinearColor(0.95f, 0.78f, 0.3f), FRotator(90.f, 0.f, 0.f));   // the tag
		}
		else
		{
			for (int32 k = 0; k < 6; ++k) { Part(EAntShape::Cube, FVector(150.f, -200.f + 80.f * k, 150.f + 25.f * FMath::Sin(k * 1.3f)), FVector(620.f, 90.f, 6.f), FLinearColor(0.95f, 0.82f, 0.25f) * (0.85f + 0.05f * k), FRotator(0.f, 0.f, -20.f + 8.f * k)); }
		}
		RoomLight = Light(FVector(-60.f, 0.f, 120.f), 24.f, FLinearColor(1.f, 0.92f, 0.8f), 700.f);
		RoomLight2 = Light(FVector(250.f, 0.f, 80.f), 4.f, FLinearColor(1.f, 0.6f, 0.5f), 300.f);
		break;
	}
	}
	// the ant itself (a puppet of the player's ant: the camera looks over its back)
	const FLinearColor Body(0.14f, 0.05f, 0.03f);
	Part(EAntShape::Sphere, FVector(-16.f, 0.f, 9.f), FVector(20.f, 15.f, 13.f), Body, FRotator(8.f, 0.f, 0.f));   // gaster
	Part(EAntShape::Sphere, FVector(-3.f, 0.f, 9.f), FVector(5.f, 4.f, 4.f), Body);                                // petiole
	Part(EAntShape::Sphere, FVector(5.f, 0.f, 10.f), FVector(14.f, 8.f, 8.f), Body, FRotator(-10.f, 0.f, 0.f));   // thorax
	Part(EAntShape::Sphere, FVector(17.f, 0.f, 12.f), FVector(11.f, 11.f, 9.f), Body);                             // head
	PuppetJaw = Part(EAntShape::Cube, FVector(24.f, 0.f, 9.f), FVector(5.f, 9.f, 2.5f), FLinearColor(0.08f, 0.03f, 0.02f));
	for (int32 s = -1; s <= 1; s += 2)
	{
		Part(EAntShape::Cylinder, FVector(24.f, 5.f * s, 18.f), FVector(1.2f, 1.2f, 16.f), Body, FRotator(-55.f, 25.f * s, 0.f), true);   // antennae
		for (int32 k = 0; k < 3; ++k)
		{
			Part(EAntShape::Cylinder, FVector(1.f + 6.f * k, 9.f * s, 5.f), FVector(1.3f, 1.3f, 18.f), Body, FRotator(0.f, -30.f + 30.f * k, 70.f * s));   // legs
		}
	}
	if (!RoomCam)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		RoomCam = W->SpawnActor<ACameraActor>(RoomAt + FVector(-62.f, 0.f, 24.f), FRotator(-8.f, 0.f, 0.f), P);
		if (RoomCam)
		{
			RoomCam->SetReplicates(false);
			RoomCam->GetCameraComponent()->SetFieldOfView(74.f);
			RoomCam->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	RoomBuilt = Spot * 8 + Who;
	UE_LOG(LogTemp, Display, TEXT("ANTINSIDE room %d for %d built: %d parts"), (int32)Spot, (int32)Who, RoomParts.Num());
}

// ---- the camera: shake, tears, the dark -------------------------------------------------------------------------------------

bool UAntInsideCamMod::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Clock += DeltaTime;
	if (Shake > 0.01f)
	{
		InOutPOV.Rotation += FRotator(FMath::PerlinNoise1D(Clock * 17.f) * 3.5f, FMath::PerlinNoise1D(Clock * 13.f + 5.f) * 4.5f, FMath::PerlinNoise1D(Clock * 11.f + 9.f) * 2.5f) * Shake;
	}
	return false;
}

void UAntInsideCamMod::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PP)
{
	const float W = FMath::Max3(Blur, Dark, Shake * 0.3f);
	if (W < 0.01f)
	{
		PostProcessBlendWeight = 0.f;
		return;
	}
	PostProcessBlendWeight = 1.f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = FMath::Clamp(0.4f + 0.9f * Dark + 0.4f * Blur, 0.f, 1.6f);
	if (Blur > 0.02f)
	{
		// the eyes water: everything swims out of focus, a little blue
		PP.bOverride_DepthOfFieldFocalDistance = true;
		PP.DepthOfFieldFocalDistance = FMath::Lerp(1000.f, 3.f, Blur);
		PP.bOverride_DepthOfFieldFstop = true;
		PP.DepthOfFieldFstop = FMath::Lerp(4.f, 0.7f, Blur);
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(FMath::Lerp(1.f, 0.85f, Blur), FMath::Lerp(1.f, 0.92f, Blur), 1.f);
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.7f + 2.5f * Blur;
	}
	if (Dark > 0.5f)
	{
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.35f * Dark;
	}
}

// ---- the hooks -------------------------------------------------------------------------------------------------------------

bool AntInside::IsInside(const AAntCharacter* Ant)
{
	const AAntInsideNet* N = Ant ? AAntInsideNet::Get(Ant->GetWorld()) : nullptr;
	return N && N->SlotOf(Ant) >= 0;
}

bool AntInside::OnInteract(AAntCharacter* Ant)
{
	AAntInsideNet* N = Ant ? AAntInsideNet::Get(Ant->GetWorld()) : nullptr;
	const int32 i = N ? N->SlotOf(Ant) : -1;
	if (i < 0)
	{
		return false;
	}
	N->Exit(i, 0, FVector::ZeroVector, 0.f, TEXT("crawled out"));
	return true;
}

bool AntInside::OnStruggle(AAntCharacter* Ant)
{
	return OnInteract(Ant);
}

bool AntInside::OnBite(AAntCharacter* Ant)
{
	AAntInsideNet* N = Ant ? AAntInsideNet::Get(Ant->GetWorld()) : nullptr;
	const int32 i = N ? N->SlotOf(Ant) : -1;
	if (i < 0)
	{
		return false;
	}
	N->OnBite(i);
	return true;
}

bool AntInside::HiddenAt(const UWorld* World, const FVector& P)
{
	const AAntInsideNet* N = AAntInsideNet::Get(World);
	if (!N || N->Slots.Num() == 0)
	{
		return false;
	}
	for (const FAntInsideSlot& S : N->Slots)
	{
		if (IsValid(S.Ant) && FVector::DistSquared(S.Ant->GetActorLocation(), P) < 60.f * 60.f) { return true; }
	}
	return false;
}

bool AntInside::DeafAt(const UWorld* World, const FVector& From)
{
	const AAntInsideNet* N = AAntInsideNet::Get(World);
	if (!N)
	{
		return false;
	}
	for (const FAntInsideSlot& S : N->Slots)
	{
		if (IsValid(S.Host) && Defs()[S.Def].Spot == Ear && FVector::Dist2D(S.Host->GetActorLocation(), From) < 600.f) { return true; }
	}
	return false;
}

bool AntInside::HumanSelfSwat(AHumanCharacter* H)
{
	AAntInsideNet* N = H ? AAntInsideNet::Get(H->GetWorld()) : nullptr;
	return N && N->SelfSwat(H);
}

// ---- the HUD -------------------------------------------------------------------------------------------------------------

void AntInside::DrawHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC)
{
	if (!Hud || !Canvas || !PC)
	{
		return;
	}
	const AAntInsideNet* N = AAntInsideNet::Get(Hud->GetWorld());
	if (!N)
	{
		return;
	}
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float SX = Canvas->ClipX, SY = Canvas->ClipY;
	const float K = SY / 1080.f;
	auto Text = [&](const FString& S, float Y, UFont* F, const FLinearColor& C, float Scale)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(F, S, W, H, Scale * K, Scale * K);
		FCanvasTextItem T(FVector2D((SX - W) * 0.5f, Y * K), FText::FromString(S), F, C);
		T.Scale = FVector2D(Scale * K, Scale * K);
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Canvas->DrawItem(T);
	};
	const float Now = N->NetNow();
	if (const AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn()))
	{
		const int32 i = N->SlotOf(Me);
		if (i < 0)
		{
			return;
		}
		const FAntInsideSlot& S = N->Slots[i];
		const FSpotDef& D = Defs()[S.Def];
		// the dark edges of the place
		for (int32 k = 0; k < 6; ++k)
		{
			const float B = (0.10f - 0.016f * k) * SY;
			FCanvasTileItem Top(FVector2D(0.f, 0.f), FVector2D(SX, B), FLinearColor(0.02f, 0.f, 0.f, 0.18f));
			Top.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Top);
			FCanvasTileItem Bot(FVector2D(0.f, SY - B), FVector2D(SX, B), FLinearColor(0.02f, 0.f, 0.f, 0.18f));
			Bot.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Bot);
		}
		Text(FString::Printf(TEXT("ВНУТРИ: %s %s"), D.Place, WhoGen(D.Who)), 70.f, Big, FLinearColor(1.f, 0.82f, 0.55f), 1.6f);
		const float Left = FMath::Max(0.f, D.MaxTime - (Now - S.Since));
		Text(FString::Printf(TEXT("[ЛКМ / ПКМ] кусать изнутри   ·   [E] или [Пробел] — вылезти   ·   ещё ~%d с"), FMath::CeilToInt(Left)), 935.f, Mid, FLinearColor(0.95f, 0.92f, 0.88f), 1.25f);
		const TCHAR* Hint =
			D.Spot == Ear ? TEXT("Укус — он трясёт головой и роняет то, что держит. Потом хлопнет по уху: вылезай вовремя!") :
			D.Spot == Nose ? TEXT("Щекотно… сейчас чихнёт — и тебя выстрелит через всю комнату!") :
			D.Spot == Leg ? TEXT("Укус — он прыгает на одной ноге. Трое в штанинах — и он падает!") :
			D.Spot == Pocket ? (S.Loot ? TEXT("Здесь добыча: вылезай [E] — унесёшь её с собой. Можно и прокатиться в кармане") : TEXT("Укрытие: в кармане тебя не достать. Можно прокатиться в другую комнату")) :
			D.Spot == Slipper ? TEXT("Укус — он скинет тапок пинком, и ты полетишь вместе с ним") :
			D.Spot == Bun ? TEXT("Укус — бабушка уронит вязание и закрутится. Отсюда видно всю гостиную") :
			D.Who == Dog ? TEXT("Укус — Бадрик взвизгнет и отряхнётся") : TEXT("Укус — малыш завизжит, и на визг придёт Человек");
		Text(Hint, 985.f, Mid, FLinearColor(0.85f, 0.85f, 0.8f), 1.05f);
		if (S.ReactAt > 0.f && S.ReactAt - Now < 1.6f)
		{
			const bool bBlink = FMath::Fmod(Now * 6.f, 1.f) < 0.6f;
			const TCHAR* Warn = D.Spot == Ear ? TEXT("ЛАДОНЬ ЛЕТИТ К УХУ! [E] — вылезай!") : D.Spot == Nose ? TEXT("А-А-А… СЕЙЧАС ЧИХНЁТ!") : D.Spot == Slipper ? TEXT("ОН ЗАМАХНУЛСЯ НОГОЙ!") : D.Spot == Pocket ? TEXT("РУКА ЛЕЗЕТ В КАРМАН!") : TEXT("ТЕБЯ СЕЙЧАС ВЫТРЯХНУТ!");
			if (bBlink) { Text(Warn, 140.f, Big, FLinearColor(1.f, 0.25f, 0.15f), 1.5f); }
		}
		return;
	}
	if (const AHumanCharacter* H = Cast<AHumanCharacter>(PC->GetPawn()))
	{
		const int32 Ears = N->CountIn(H, Ear), Noses = N->CountIn(H, Nose), Legs = N->CountIn(H, Leg), Pockets = N->CountIn(H, Pocket), Feet = N->CountIn(H, Slipper);
		FString S;
		if (Ears) { S = TEXT("Муравей в ухе! Ты почти ничего не слышишь. [F] — хлопнуть себя по уху"); }
		else if (Noses) { S = TEXT("В носу щекотно… [F] — высморкаться"); }
		else if (Legs) { S = FString::Printf(TEXT("Муравьи в штанине (%d)! Трое — и ты упадёшь. [F] — трясти ногой"), Legs); }
		else if (Feet) { S = TEXT("Кто-то в тапке! [F] — скинуть тапок"); }
		else if (Pockets) { S = TEXT("Что-то шуршит в кармане… [F] — похлопать по карману"); }
		if (!S.IsEmpty())
		{
			if (AntVersus::Active(Hud->GetWorld())) { S += TEXT("  (−10 терпения)"); }
			Text(S, 860.f, Big, FLinearColor(1.f, 0.6f, 0.35f), 1.15f);
		}
	}
}

// ---- dev ---------------------------------------------------------------------------------------------------------------

namespace
{
	AActor* FindHost(UWorld* W, const FString& Word)
	{
		const FString Wd = Word.ToLower();
		if (Wd == TEXT("dog") || Wd == TEXT("badrik"))
		{
			for (TActorIterator<ACritter> It(W); It; ++It) { if (It->Kind == 0) { return *It; } }
			return nullptr;
		}
		const uint8 Fig = Wd == TEXT("gran") || Wd == TEXT("grandma") ? HumanFig::Grandma : Wd == TEXT("kid") ? HumanFig::Kid : HumanFig::Dad;
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (It->Fig == Fig) { return *It; } }
		return nullptr;
	}

	int32 FindDef(const AActor* Host, const FString& Word)
	{
		const FString Wd = Word.ToLower();
		const uint8 Who = WhoOf(Host);
		uint8 Spot = Ear;
		int32 Nth = 0;
		if (Wd.StartsWith(TEXT("nose"))) { Spot = Nose; }
		else if (Wd.StartsWith(TEXT("leg"))) { Spot = Leg; Nth = Wd == TEXT("legr") ? 1 : 0; }
		else if (Wd.StartsWith(TEXT("pocket"))) { Spot = Pocket; }
		else if (Wd.StartsWith(TEXT("slipper"))) { Spot = Slipper; }
		else if (Wd.StartsWith(TEXT("bun"))) { Spot = Bun; }
		else if (Wd.StartsWith(TEXT("collar")) || Wd.StartsWith(TEXT("hood"))) { Spot = Collar; }
		int32 n = 0;
		for (int32 d = 0; d < AntInside::Defs().Num(); ++d)
		{
			if (AntInside::Defs()[d].Who == Who && AntInside::Defs()[d].Spot == Spot && n++ == Nth) { return d; }
		}
		return -1;
	}
}

bool AntInside::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0 || !A[0].Equals(TEXT("inside"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	AAntInsideNet* N = AAntInsideNet::Get(W, true);
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	auto Num = [&A](int32 i, float Def) { return A.IsValidIndex(i) && (A[i].IsNumeric() || A[i].StartsWith(TEXT("-"))) ? FCString::Atof(*A[i]) : Def; };
	if (Op == TEXT("put") || Op == TEXT("bots"))
	{
		// inside put <who> <spot>: the local player's ant goes in; inside bots <who> <spot> N: N worker bots go in
		AActor* Host = FindHost(W, A.IsValidIndex(2) ? A[2] : FString(TEXT("human")));
		const int32 Def = Host ? FindDef(Host, A.IsValidIndex(3) ? A[3] : FString(TEXT("ear"))) : -1;
		if (!Host || Def < 0)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE dev: no such resident or spot"));
			return true;
		}
		if (Op == TEXT("put"))
		{
			if (Me && N->SlotOf(Me) >= 0) { N->Exit(N->SlotOf(Me), 0, FVector::ZeroVector, 0.f, TEXT("dev: somewhere else")); }
			if (Me) { N->Enter(Me, Host, (uint8)Def, true); }
		}
		else
		{
			const int32 Want = (int32)Num(4, 3.f);
			TArray<AAntCharacter*> Pool;
			auto Gather = [&]() { Pool.Reset(); for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && It->Creature == 0 && It->Team == 0 && !It->IsPlayerControlled() && !It->IsAttached()) { Pool.Add(*It); } } };
			Gather();
			if (Pool.Num() < Want) { GM->AddBots(0, Want - Pool.Num()); Gather(); }
			int32 In = 0;
			for (int32 k = 0; k < Pool.Num() && In < Want; ++k)
			{
				// spread them over both legs (and over every spot of that kind)
				const int32 D = (Defs()[Def].Spot == Leg) ? FindDef(Host, k % 2 ? TEXT("legr") : TEXT("legl")) : Def;
				if (N->Enter(Pool[k], Host, (uint8)D, true)) { ++In; }
			}
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE dev: %d bots into the %s of %s"), In, Defs()[Def].Place, *Host->GetName());
		}
	}
	else if (Op == TEXT("bite"))
	{
		// inside bite [all]: the local ant (or every ant inside) bites once
		for (int32 i = N->Slots.Num() - 1; i >= 0; --i)
		{
			if (N->Slots.IsValidIndex(i) && (N->Slots[i].Ant == Me || (A.IsValidIndex(2) && A[2] == TEXT("all")))) { N->OnBite(i); }
		}
	}
	else if (Op == TEXT("out"))
	{
		const int32 i = N->SlotOf(Me);
		if (i >= 0) { N->Exit(i, 0, FVector::ZeroVector, 0.f, TEXT("dev")); }
	}
	else if (Op == TEXT("sneeze") || Op == TEXT("slap") || Op == TEXT("react"))
	{
		// the reaction coming to everyone inside that spot now
		const uint8 Sp = Op == TEXT("sneeze") ? Nose : Ear;
		for (FAntInsideSlot& S : N->Slots)
		{
			if (Op == TEXT("react") || Defs()[S.Def].Spot == Sp) { S.ReactAt = N->NetNow(); if (!S.React) { S.React = Defs()[S.Def].Spot == Nose ? 1 : Defs()[S.Def].Spot == Ear ? 2 : Defs()[S.Def].Spot == Pocket ? 4 : Defs()[S.Def].Spot == Slipper ? 5 : 7; } }
		}
	}
	else if (Op == TEXT("swat"))
	{
		if (AHumanCharacter* H = Cast<AHumanCharacter>(FindHost(W, A.IsValidIndex(2) ? A[2] : FString(TEXT("human"))))) { N->SelfSwat(H); }
	}
	else if (Op == TEXT("cam"))
	{
		// inside cam <who> <spot> [dist] [yaw] [height] [fov]: a fixed camera at that entry point (follows it on every call)
		AActor* Host = FindHost(W, A.IsValidIndex(2) ? A[2] : FString(TEXT("human")));
		const int32 Def = Host ? FindDef(Host, A.IsValidIndex(3) ? A[3] : FString(TEXT("ear"))) : -1;
		FVector P;
		if (Def >= 0 && SpotPoint(Host, (uint8)Def, P))
		{
			const float Yaw = FMath::DegreesToRadians(Num(5, 30.f)) + FMath::Atan2((float)Host->GetActorForwardVector().Y, (float)Host->GetActorForwardVector().X);
			const float Dist = Num(4, 900.f);
			AntCloth::DevCam(W, P + FVector(FMath::Cos(Yaw) * Dist, FMath::Sin(Yaw) * Dist, Num(6, 100.f)), P, Num(7, 60.f));
		}
	}
	else if (Op == TEXT("camoff"))
	{
		AntCloth::DevCam(W, FVector::ZeroVector, FVector::ZeroVector, 0.f);
	}
	else if (Op == TEXT("shot"))
	{
		AntCloth::DevShot(W, TEXT("inside"), A.IsValidIndex(2) ? A[2] : FString(TEXT("inside")));
	}
	else if (Op == TEXT("mark"))
	{
		// little bright balls on every entry point for 20 s (where they are on the bodies)
		for (TActorIterator<AAntInsideSpot> It(W); It; ++It)
		{
			FVector P;
			if (!SpotPoint(It->Host, It->Def, P)) { continue; }
			if (UStaticMeshComponent* C = AntKit::AddPart(N, N->GetRootComponent(), EAntShape::Sphere, FVector::ZeroVector, FVector(0.4f), FLinearColor(0.1f, 1.f, 0.3f), FRotator::ZeroRotator, false, false))
			{
				C->SetAbsolute(true, true, true);
				C->SetWorldLocation(P);
				TWeakObjectPtr<UStaticMeshComponent> WC(C);
				FTimerHandle T;
				W->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WC]() { if (WC.IsValid()) { WC->DestroyComponent(); } }), 20.f, false);
			}
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE spot %s of %s at %s"), Defs()[It->Def].Place, *It->Host->GetName(), *P.ToCompactString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE info: %d inside"), N->Slots.Num());
		for (const FAntInsideSlot& S : N->Slots)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTINSIDE  %s in the %s of %s for %.1f s, bites %d, reaction %d at %+.1f s, loot %d"), IsValid(S.Ant) ? *S.Ant->GetDisplayName() : TEXT("-"), Defs()[S.Def].Place,
				IsValid(S.Host) ? *S.Host->GetName() : TEXT("-"), N->NetNow() - S.Since, S.Bites, S.React, S.ReactAt > 0.f ? S.ReactAt - N->NetNow() : 0.f, S.Loot);
		}
		int32 Spots = 0;
		for (TActorIterator<AAntInsideSpot> It(W); It; ++It) { ++Spots; }
		UE_LOG(LogTemp, Display, TEXT("ANTINSIDE  %d entry points on the residents"), Spots);
	}
	return true;
}
