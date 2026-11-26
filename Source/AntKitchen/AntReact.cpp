// Materials and chain reactions 2.0 (docs/MATERIALS.md, docs/FEATURES.md section 37): the reactions of the flat and the
// reactive objects of the chains C7..C20. Server-authoritative; every machine draws from the replicated numbers.
#include "AntReact.h"
#include "AntMatter.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntRun.h"
#include "AntAudio.h"
#include "AntFx.h"
#include "AntMess.h"
#include "AntInside.h"
#include "AntCloth.h"
#include "AntWeapon.h"
#include "AntFluidWorld.h"
#include "AntFluidSim.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "CreatureCharacters.h"
#include "KitchenArt.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "KitchenGadgets.h"
#include "KitchenLoot.h"
#include "KitchenMover.h"
#include "KitchenSlick.h"
#include "KitchenEvents.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "Map4Look.h"
#include "NpcRoutine.h"
#include "NpcBody.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

namespace
{
	// ---------------------------------------------------------------------------------------------------------------
	// Where everything is (level v3 / map v4 coordinates; 1 m = 1900 uu). Surfaces are found by a trace down at spawn.
	// ---------------------------------------------------------------------------------------------------------------
	namespace Spot
	{
		const FVector Pepper(-1650.f, -1180.f, 1400.f);        // on the dining table, next to the salt
		const FVector Flour(-4420.f, 3150.f, 1712.f);          // on the worktop between the cutting board and the sink, at the front edge
		const FVector FlourFloor(-4420.f, 2620.f, 0.f);        // where the avalanche lands
		const FVector Ice(1700.f, 3110.f, 1712.f);             // on the worktop in front of the microwave, at the edge
		const FVector IceFloor(1700.f, 2700.f, 0.f);
		const FVector Strip(1950.f, 2470.f, 0.f);              // the power strip on the floor in front of the dishwasher / microwave
		const FVector StripCableTop(1950.f, 3000.f, 1712.f);   // its cable goes up to the worktop (kettle, microwave)
		const FVector Slingshot(-2450.f, -400.f, 0.f);         // on the floor west of the table, aimed at it
		const FVector SlingTarget(-1250.f, -380.f, 1420.f);    // where the team lands on the table
		const FVector Peas(2380.f, -1560.f, 1400.f);           // a bowl at the table's corner
		const FVector PeasFloor(2900.f, -2050.f, 0.f);
		const FVector Choc(260.f, -4850.f, 2600.f);            // on the kitchen windowsill (z found by a trace)
		const FVector ChocRad(260.f, -4870.f, 1400.f);         // the radiator's top under it
		const FVector ChocFloor(260.f, -4450.f, 0.f);
		const FVector ToasterLever(-5900.f, 4200.f, 1990.f);
		const FVector ToastFrom(-6250.f, 4085.f, 2120.f);
		const FVector SpiceJar(-2050.f, 4890.f, 2100.f);       // bottom spice shelf (z by trace)
		const FVector SpiceLand(-2000.f, 4560.f, 1712.f);
		const FVector PepperCloudK(-1800.f, 4300.f, 1950.f);
		const FVector Books(19460.f, -2350.f, 4700.f);         // on top of the bookcase (z by trace), the row runs towards -Y
		const FVector VaseFloor(18650.f, -3450.f, 0.f);
		const FVector MicroButton(2290.f, 3945.f, 2030.f);
		const FVector MicroBox0(1330.f, 4010.f, 1730.f), MicroBox1(2160.f, 4880.f, 2320.f);
		const FVector MentosPack(12660.f, -2640.f, 1000.f);    // the coffee table's north edge (z by trace)
		const FVector Bottle(12660.f, -2420.f, 0.f);           // the open cola bottle on the floor just below
		const FVector Tower(11850.f, -2760.f, 0.f);            // the kid's block tower (the skittles), clear of the level's own blocks
		const FVector Sponge(-2830.f, 3470.f, 1760.f);         // on the worktop between the sink and the dish rack
		const FVector Plate(-2470.f, 3170.f, 1900.f);          // a plate standing at the front of the rack
		const FVector PlateFloor(-2380.f, 2700.f, 0.f);
		const FVector SinkCenter(-3500.f, 4000.f, 1300.f);
		const FVector Balloon(15300.f, -1950.f, 0.f);          // on the living-room carpet
		const FVector Breaker(20180.f, 420.f, 2650.f);         // on the hall wall (east wall of the living room), eye level
		const FVector BreakerStand(19450.f, 420.f, 0.f);
		const FVector RadK0(-700.f, -5010.f, 280.f), RadK1(700.f, -4780.f, 1460.f);   // the kitchen radiator
		const FVector RadL0(17400.f, -9010.f, 280.f), RadL1(19160.f, -8780.f, 1460.f); // the living-room radiator
		const FVector Burner(-350.f, 3500.f, 1760.f);
		const FVector ToasterSlots0(-6600.f, 3900.f, 1900.f), ToasterSlots1(-5900.f, 4500.f, 2200.f);
		const FVector Freezer0(2700.f, 3100.f, 2520.f), Freezer1(4700.f, 4780.f, 3650.f);
		const FVector Fridge0(2700.f, 3100.f, 0.f), Fridge1(4700.f, 4780.f, 2520.f);
		const FVector DishLip(650.f, 2880.f, 400.f);
	}

	constexpr float Litre = 1900.f * 1900.f * 1900.f * 0.001f;   // uu^3
	const FLinearColor FlourCol(0.96f, 0.95f, 0.92f), PepperCol(0.42f, 0.31f, 0.21f), PepperSpeck(0.09f, 0.06f, 0.04f);

	AAntKitchenGameMode* GMOf(const UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	AHumanCharacter* Dad(const UWorld* W)
	{
		if (!W) { return nullptr; }
		for (TActorIterator<AHumanCharacter> It(const_cast<UWorld*>(W)); It; ++It)
		{
			if (It->Fig == HumanFig::Dad && !It->IsA<AHouseFolk>() && !It->IsHidden()) { return *It; }
		}
		return nullptr;
	}
	template <class T> T* First(const UWorld* W)
	{
		if (!W) { return nullptr; }
		for (TActorIterator<T> It(const_cast<UWorld*>(W)); It; ++It) { if (!It->IsHidden()) { return *It; } }
		return nullptr;
	}
	ACritter* Pet(const UWorld* W, uint8 Kind)
	{
		if (!W) { return nullptr; }
		for (TActorIterator<ACritter> It(const_cast<UWorld*>(W)); It; ++It) { if (It->Kind == Kind && !It->IsHidden()) { return *It; } }
		return nullptr;
	}
	void Say(UWorld* W, uint8 Who, const TCHAR* Line)
	{
		NpcRoutine::Say(W, Who, Line);
		AntAudio::Duck(W, 1.2f);
	}
	void Noise(UWorld* W, const FVector& At, float Loud, uint8 Kind, AActor* Src, AAntCharacter* By)
	{
		if (AAntKitchenGameMode* GM = GMOf(W)) { GM->MakeNoise(At, Loud, Kind, Src, By); }
	}
	void Tell(UWorld* W, const FString& Text, const FLinearColor& Col = FLinearColor(1.f, 0.85f, 0.4f))
	{
		if (AAntKitchenGameMode* GM = GMOf(W)) { GM->Broadcast(Text, Col); }
	}
	FVector Floor(const UWorld* W, const FVector& P, float Up = 300.f, float Down = 4000.f, const AActor* Ignore = nullptr)
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntReactFloor), true, Ignore);
		if (W && W->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, Up), P - FVector(0.f, 0.f, Down), ECC_Visibility, Q)) { return H.ImpactPoint; }
		return FVector(P.X, P.Y, 0.f);
	}
	FVector Head(const AActor* A)
	{
		if (const ACharacter* C = Cast<ACharacter>(A)) { return C->GetActorLocation() + FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.8f); }
		if (const ACritter* P = Cast<ACritter>(A)) { return P->GetActorLocation() + FVector(0.f, 0.f, P->BodyHeight() * 0.7f) + P->GetActorForwardVector() * P->BodyLength() * 0.45f; }
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}
	bool InBox(const FVector& P, const FVector& A, const FVector& B) { return FBox(A.ComponentMin(B), A.ComponentMax(B)).IsInside(P); }

	uint8 MessKindOf(const FString& S)
	{
		static const TCHAR* Names[] = { TEXT("jam"), TEXT("honey"), TEXT("cola"), TEXT("oil"), TEXT("cream"), TEXT("crumbs"), TEXT("mud"), TEXT("water"), TEXT("chocolate"), TEXT("scratch"), TEXT("print"), TEXT("drag"), TEXT("juice"), TEXT("sugar"), TEXT("flour"), TEXT("pepper") };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i) { if (S.Equals(Names[i], ESearchCase::IgnoreCase)) { return (uint8)i; } }
		return 255;
	}

	/** The chain table for the docs, the admin and the tests. */
	struct FChainDef { const TCHAR* Id; const TCHAR* Ru; };
	const FChainDef GChains[] =
	{
		{ TEXT("C7"),  TEXT("Перечница → облако перца → все чихают, папа роняет из рук") },
		{ TEXT("C8"),  TEXT("Мешок муки → лавина с края → белая мгла: Человек ослеп, муравьи белые") },
		{ TEXT("C9"),  TEXT("Лоток льда → лёд тает → лужа до удлинителя → КЗ → темнота") },
		{ TEXT("C10"), TEXT("Рогатка малыша → отряд летит на стол") },
		{ TEXT("C11"), TEXT("Миска гороха → горох катится → папа падает как на шариках") },
		{ TEXT("C12"), TEXT("Шоколадка → на батарею → тает → липкая река у окна") },
		{ TEXT("C13"), TEXT("Пружина тостера → тост летит → банка перца → папа у плиты чихает") },
		{ TEXT("C14"), TEXT("Книги-домино → ваза → вода и конфеты → Бадрик скользит") },
		{ TEXT("C15"), TEXT("Микроволновка → пищит → папа идёт выключать") },
		{ TEXT("C16"), TEXT("Посудомойка открыта → вода с дверцы → удлинитель → темнота") },
		{ TEXT("C17"), TEXT("Робот-пылесос → ест горох / развозит муку") },
		{ TEXT("C18"), TEXT("Ментос в колу → гейзер → бутылка-ракета → страйк по кубикам") },
		{ TEXT("C19"), TEXT("Губка в раковине → тяжелеет → падает → тарелка вдребезги") },
		{ TEXT("C20"), TEXT("Шарик о ковёр → статика → прилип к потолку с муравьями") },
		{ TEXT("C21"), TEXT("Сахар в воде → сладкая вода → муха") },
		{ TEXT("C22"), TEXT("Перегрызть провод → искра → КЗ → темнота") },
	};
}

namespace AntProp
{
	const TCHAR* Name(uint8 Kind)
	{
		static const TCHAR* N[] = { TEXT("pepper"), TEXT("flour"), TEXT("ice"), TEXT("peas"), TEXT("slingshot"), TEXT("choc"), TEXT("toaster"), TEXT("books"),
			TEXT("micro"), TEXT("mentos"), TEXT("sponge"), TEXT("balloon"), TEXT("strip"), TEXT("breaker") };
		return Kind < UE_ARRAY_COUNT(N) ? N[Kind] : TEXT("?");
	}
	const TCHAR* Ru(uint8 Kind)
	{
		static const TCHAR* N[] = { TEXT("перечницу"), TEXT("мешок муки"), TEXT("лоток со льдом"), TEXT("миску гороха"), TEXT("рогатку"), TEXT("шоколадку"),
			TEXT("пружину тостера"), TEXT("книгу"), TEXT("кнопку микроволновки"), TEXT("ментос"), TEXT("губку"), TEXT("шарик"), TEXT("провод удлинителя"), TEXT("рубильник") };
		return Kind < UE_ARRAY_COUNT(N) ? N[Kind] : TEXT("?");
	}
}

// =====================================================================================================================
// AAntReactNet: the world's reactions
// =====================================================================================================================

AAntReactNet::AAntReactNet()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	InteractRadius = 0.f;
}

void AAntReactNet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntReactNet, bPower);
	DOREPLIFETIME(AAntReactNet, Trips);
	DOREPLIFETIME(AAntReactNet, PowerAt);
	DOREPLIFETIME(AAntReactNet, bStripUnplugged);
	DOREPLIFETIME(AAntReactNet, Clouds);
	DOREPLIFETIME(AAntReactNet, Marks);
	DOREPLIFETIME(AAntReactNet, Melts);
	DOREPLIFETIME(AAntReactNet, HeatMode);
}

AAntReactNet* AAntReactNet::Get(const UWorld* W)
{
	if (!W) { return nullptr; }
	static TWeakObjectPtr<AAntReactNet> Cache;
	if (Cache.IsValid() && Cache->GetWorld() == W && !Cache->IsActorBeingDestroyed()) { return Cache.Get(); }
	for (TActorIterator<AAntReactNet> It(const_cast<UWorld*>(W)); It; ++It) { Cache = *It; return *It; }
	return nullptr;
}

float AAntReactNet::NetNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? (float)GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

void AAntReactNet::BeginPlay()
{
	Super::BeginPlay();
	AntMaterials::Load();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// the melting ice cubes: one instanced mesh (the cube model, or a clear gem if it is not in this build)
		UStaticMesh* Cube = Map4Look::Prop(TEXT("SM_R_IceCube"));
		if (!Cube) { Cube = Map4Look::Prop(TEXT("SM_M4_Gem")); }
		if (Cube)
		{
			UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this, TEXT("IceCubes"));
			I->SetStaticMesh(Cube);
			I->SetMobility(EComponentMobility::Movable);
			I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			I->SetCastShadow(true);
			I->SetupAttachment(GetRootComponent());
			I->SetAbsolute(true, true, true);
			if (const Map4Look::FLook* L = Map4Look::Named(TEXT("Ice"))) { I->SetMaterial(0, Map4Look::Get(*L)); }
			I->RegisterComponent();
			AddInstanceComponent(I);
			MeltMesh[0] = I;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTREACT net up (%s), %d materials"), HasAuthority() ? TEXT("server") : TEXT("client"), AntMaterials::All().Num());
}

void AAntReactNet::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bDarkApplied) { ApplyBlackout(false); }
	Super::EndPlay(Reason);
}

FString AAntReactNet::Describe() const
{
	return FString::Printf(TEXT("power %s (trips %d, strip %s), clouds %d, marks %d, melts %d, heat mode %d, rollers %d, tick %.3f ms (avg %.3f)"),
		bPower ? TEXT("on") : TEXT("OFF"), Trips, bStripUnplugged ? TEXT("unplugged") : TEXT("plugged"), Clouds.Num(), Marks.Num(), Melts.Num(), HeatMode, Rollers.Num(), LastTickMs, AvgTickMs);
}

void AAntReactNet::ChainLog(const TCHAR* Chain, const FString& What)
{
	const float Now = GetWorld()->GetTimeSeconds();
	float* T0 = ChainStart.Find(Chain);
	if (!T0) { T0 = &ChainStart.Add(Chain, Now); }
	UE_LOG(LogTemp, Display, TEXT("ANTCHAIN %s +%.1fs %s"), Chain, Now - *T0, *What);
}

void AAntReactNet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T0 = FPlatformTime::Seconds();
	if (HasAuthority()) { ServerTick(DeltaSeconds); }
	if (GetNetMode() != NM_DedicatedServer) { LocalTick(DeltaSeconds); }
	LastTickMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	AvgTickMs = AvgTickMs * 0.98 + LastTickMs * 0.02;
}

// ---------------------------------------------------------------------------------------------------------------------
// server
// ---------------------------------------------------------------------------------------------------------------------

void AAntReactNet::ServerTick(float Dt)
{
	const float Now = NetNow();
	// the marks wear off
	for (int32 i = Marks.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(Marks[i].Who) || Now > Marks[i].Until) { Marks.RemoveAt(i); }
	}
	TickClouds(Dt);
	TickPower(Dt);
	TickMicrowave(Dt);
	if (Now >= NextRuleTick)
	{
		// the slower rules: 8 times a second
		const float Step = 0.125f;
		NextRuleTick = Now + Step;
		TickHeat(Step);
		TickWet(Step);
		TickVacuums(Step);
	}
	TickRollers(Dt);
}

int32 AAntReactNet::AddCloud(uint8 Kind, const FVector& Center, float Radius, float Life, const FVector& Drift, const FVector& Source, float PourFor, const TCHAR* Chain)
{
	static uint8 NextId = 1;
	if (Clouds.Num() >= 8) { Clouds.RemoveAt(0); }
	FAntReactCloud& C = Clouds.AddDefaulted_GetRef();
	C.Kind = Kind;
	C.Id = NextId++;
	CloudChain.Add(C.Id, Chain ? Chain : (Kind == AntCloudKind::Flour ? TEXT("C8") : TEXT("C7")));
	C.Center = Center;
	C.Radius = Radius;
	C.Life = Life;
	C.Drift = Drift;
	C.Born = NetNow();
	C.Source = Source;
	C.PourFor = PourFor;
	UE_LOG(LogTemp, Display, TEXT("ANTREACT cloud %s r %.0f for %.0f s at %s"), Kind == AntCloudKind::Pepper ? TEXT("pepper") : Kind == AntCloudKind::Flour ? TEXT("flour") : Kind == AntCloudKind::Smoke ? TEXT("smoke") : TEXT("steam"), Radius, Life, *Center.ToCompactString());
	return C.Id;
}

void AAntReactNet::Mark(AActor* Who, uint8 Kind, float Seconds)
{
	if (!Who) { return; }
	const float Until = NetNow() + Seconds;
	for (FAntReactMark& M : Marks)
	{
		if (M.Who == Who && M.Kind == Kind) { M.Until = FMath::Max(M.Until, Until); return; }
	}
	if (Marks.Num() >= 64) { Marks.RemoveAt(0); }
	FAntReactMark& M = Marks.AddDefaulted_GetRef();
	M.Who = Who;
	M.Kind = Kind;
	M.Until = Until;
}

bool AAntReactNet::HasMark(const AActor* Who, uint8 Kind) const
{
	const float Now = NetNow();
	for (const FAntReactMark& M : Marks) { if (M.Who == Who && M.Kind == Kind && Now <= M.Until) { return true; } }
	return false;
}

void AAntReactNet::AddMelt(uint8 Kind, const FVector& Pos, float Life)
{
	if (Melts.Num() >= 40) { return; }
	FAntReactMelt& M = Melts.AddDefaulted_GetRef();
	M.Kind = Kind;
	M.Pos = Pos;
	M.Yaw = FMath::FRandRange(0.f, 360.f);
	M.Born = NetNow();
	M.Life = Life;
	M.Left = 1.f;
}

void AAntReactNet::Zap(AAntCharacter* Ant, float Damage, const TCHAR* Why)
{
	if (!Ant || Ant->bDead) { return; }
	Ant->HurtBy(Damage, Why);
	Ant->Stagger(1.4f, FVector(0.f, 0.f, 1.f));
	Ant->ApplyLaunch(FVector(FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(-160.f, 160.f), 380.f));
	Ant->Toast(TEXT("БЗЗЗТ! Ударило током"));
	Mark(Ant, AntMark::Burnt, 14.f);
	AntAudio::Everyone(this, EAntSfx::Zap, Ant->GetActorLocation(), 1.2f);
	MulticastSparks(Ant->GetActorLocation(), 6);
}

void AAntReactNet::Short(const FVector& Where, const TCHAR* Why, AAntCharacter* By)
{
	UWorld* W = GetWorld();
	ShortAt = Where;
	LiveWaterUntil = NetNow() + 2.5f;
	MulticastSparks(Where + FVector(0.f, 0.f, 30.f), 24);
	AntAudio::Everyone(this, EAntSfx::Zap, Where, 1.6f);
	AntAudio::Everyone(this, EAntSfx::Sizzle, Where, 1.2f);
	AntAudio::Everyone(this, EAntSfx::BreakerClick, Where, 1.2f);
	Noise(W, Where, 0.8f, AntNoise::Crash, this, By);
	// the live water: every wet ant near the short, and every ant in the liquid that reaches it
	int32 N = 0;
	const UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Creature != 0) { continue; }
		const FVector P = A->GetActorLocation();
		const float D = FVector::Dist(P, Where);
		const bool bInWater = Fluid && Fluid->DepthAt(P) > 0.5f && D < 4500.f;
		if (D < 260.f || (A->HasStatus(AntStatus::Wet) && D < 2600.f) || bInWater)
		{
			Zap(A, D < 260.f ? 35.f : 25.f, bInWater ? TEXT("поджарился током в луже") : TEXT("получил разряд током"));
			++N;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTREACT SHORT at %s (%s): %d ants zapped"), *Where.ToCompactString(), Why, N);
	ChainLog(TEXT("power"), FString::Printf(TEXT("short circuit (%s), %d ants zapped"), Why, N));
	AntMatter::OnShort(W, Where, Why);   // the metal and the wet things it touches go live; their sparks light what burns
	if (bPower) { SetPower(false, Why, Where); }
}

void AAntReactNet::SetPower(bool bOn, const TCHAR* Why, const FVector& Where)
{
	if (bPower == bOn) { return; }
	UWorld* W = GetWorld();
	bPower = bOn;
	PowerAt = NetNow();
	OnRep_Power();
	AAntReactProp* Box = AAntReactProp::Find(W, AntProp::Breaker);
	if (!bOn)
	{
		++Trips;
		UE_LOG(LogTemp, Display, TEXT("ANTREACT BLACKOUT (%s), trip %d"), Why, (int32)Trips);
		ChainLog(TEXT("power"), FString::Printf(TEXT("BLACKOUT: %s (trip %d)"), Why, (int32)Trips));
		Tell(W, TEXT("ЩЁЛК! Выбило пробки — в квартире темно"), FLinearColor(0.7f, 0.8f, 1.f));
		AntAudio::Everyone(this, EAntSfx::PowerDown, Where, 1.f);
		if (Box) { Box->SetState(3); }
		// the residents
		if (AHumanCharacter* D = Dad(W))
		{
			Say(W, NpcRoutine::Dad, Trips >= 2 ? TEXT("Да что ж такое! Опять выбило!") : TEXT("Опа. Пробки выбило... Где фонарик?"));
			if (NpcRoutine::Enabled() && Box)
			{
				NpcRoutine::AddMess(W, NpcRoutine::Power, Spot::BreakerStand, Box, TEXT("щиток в прихожей"));
			}
		}
		if (LevelV3::IsHome(LevelV3::Kid))
		{
			if (AKidCharacter* K = First<AKidCharacter>(W)) { K->Panic(5.f); }
			Say(W, NpcRoutine::Kid, TEXT("Ма-а-ам! Темно! Я боюсь!"));
		}
		if (LevelV3::IsHome(LevelV3::Grandma)) { Say(W, NpcRoutine::Gran, TEXT("Ой, свет погас... Где мои очки?")); }
		if (ACritter* Dog = Pet(W, 0)) { AntAudio::Everyone(this, EAntSfx::DogBark, Dog->GetActorLocation(), 1.f); Dog->Scare(Where, TEXT("темнота")); }
		StripResetAt = -100.f;
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("ANTREACT power back (%s)"), Why);
		ChainLog(TEXT("power"), FString::Printf(TEXT("power back: %s"), Why));
		AntAudio::Everyone(this, EAntSfx::BreakerClick, Spot::Breaker, 1.f);
		AntAudio::Everyone(this, EAntSfx::PowerUp, Where, 0.8f);
		if (Box) { Box->ResetProp(); }
		StripResetAt = NetNow();
		WetStripSince = -1.f;
	}
}

void AAntReactNet::TickPower(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	if (Now < NextPowerCheck) { return; }
	NextPowerCheck = Now + 0.25f;
	// nobody to reset it (routines off, the Dad gone): it comes back by itself after 45 s
	if (!bPower && Now - PowerAt > 45.f && (!NpcRoutine::Enabled() || !Dad(W)))
	{
		SetPower(true, TEXT("само (никого нет)"), Spot::Breaker);
	}
	if (!bPower || bStripUnplugged) { return; }
	AAntReactProp* Strip = AAntReactProp::Find(W, AntProp::PowerStrip);
	if (!Strip) { return; }
	const FVector P = Strip->GetActorLocation();
	float Depth = 0.f;
	if (const UAntFluidWorld* Fluid = UAntFluidWorld::Active(this)) { Depth = Fluid->DepthAt(P + FVector(0.f, 0.f, 20.f)); }
	const bool bSlick = AKitchenSlick::At(W, P, 60.f) != nullptr;
	if (Depth > 0.6f || bSlick)
	{
		if (WetStripSince < 0.f)
		{
			WetStripSince = Now;
			UE_LOG(LogTemp, Display, TEXT("ANTREACT water reaches the power strip (depth %.1f uu%s)"), Depth, bSlick ? TEXT(", a puddle") : TEXT(""));
		}
		if (Now - WetStripSince > 0.6f)
		{
			Short(P, TEXT("вода в удлинителе"), Strip->By.Get());
			WetStripSince = -1.f;
		}
	}
	else
	{
		WetStripSince = -1.f;
	}
}

void AAntReactNet::OnRep_Power()
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	ApplyBlackout(!bPower);
}

void AAntReactNet::ApplyBlackout(bool bDark)
{
	UWorld* W = GetWorld();
	if (!W || bDark == bDarkApplied) { return; }
	bDarkApplied = bDark;
	int32 N = 0;
	if (bDark)
	{
		SavedLights.Reset();
		for (TObjectIterator<ULocalLightComponent> It; It; ++It)
		{
			ULocalLightComponent* L = *It;
			if (!L || L->GetWorld() != W || !L->IsRegistered() || L->GetOwner() == this) { continue; }
			if (!L->IsVisible() || L->Intensity <= 0.f) { continue; }
			if (!LevelV2::InHouse(L->GetComponentLocation())) { continue; }   // the street lamp, the yard
			SavedLights.Add(L, L->Intensity);
			L->SetVisibility(false);
			++N;
		}
		for (TActorIterator<AKitchenBuilder> It(W); It; ++It) { It->SetMainLights(false); }
	}
	else
	{
		for (const TPair<TWeakObjectPtr<ULightComponent>, float>& P : SavedLights)
		{
			if (ULocalLightComponent* L = Cast<ULocalLightComponent>(P.Key.Get())) { L->SetVisibility(true); ++N; }
		}
		SavedLights.Reset();
		const bool bSwitch = AKitchenSwitch::AreLightsOn(W);
		for (TActorIterator<AKitchenBuilder> It(W); It; ++It) { It->SetMainLights(bSwitch); }
	}
	// the gadgets of other branches (TV, robot vacuum, washing machine) may say they want to know: UFUNCTION AntSetPowered(bool)
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (UFunction* F = It->FindFunction(TEXT("AntSetPowered")))
		{
			if (F->ParmsSize == sizeof(bool)) { bool bOn = !bDark; It->ProcessEvent(F, &bOn); }
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTREACT %s: %d lamps %s (%s)"), bDark ? TEXT("blackout") : TEXT("lights back"), N, bDark ? TEXT("off") : TEXT("on"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"));
}

void AAntReactNet::MulticastSparks_Implementation(FVector_NetQuantize At, uint8 Count)
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	AntFx::Sparks(this, At, Count, 14.f);
}

void AAntReactNet::MulticastPuff_Implementation(uint8 Kind, FVector_NetQuantize At, float Size)
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	AAntFxManager* Fx = AAntFxManager::Get(this);
	if (!Fx) { return; }
	const FLinearColor C = Kind == AntCloudKind::Pepper ? PepperCol : Kind == AntCloudKind::Flour ? FlourCol : FLinearColor(0.9f, 0.6f, 0.3f);
	for (int32 i = 0; i < 14; ++i)
	{
		FAntFxBit B;
		B.Pos = At + FMath::VRand() * Size * 0.2f;
		B.Vel = FMath::VRand() * Size * 1.2f + FVector(0.f, 0.f, Size * 0.6f);
		B.Scale0 = FVector(Size / 400.f);
		B.Scale1 = FVector(Size / 140.f);
		B.Color = FLinearColor(C.R, C.G, C.B, 0.45f);
		B.Life = 1.6f;
		B.Drag = 2.5f;
		B.MinAmbient = 0.5f;
		B.Look = EAntFxLook::Soft;
		Fx->Emit(B);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// powders: pepper (sneezing), flour (blind Human, white ants)
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	/** How far the cloud reaches now (it grows for the first 2 s, thins in its last quarter). */
	float CloudR(const FAntReactCloud& C, float Now)
	{
		const float Age = Now - C.Born;
		const float Grow = FMath::Clamp(Age / 2.f, 0.3f, 1.f);
		return C.Radius * Grow * (1.f + 0.25f * FMath::Clamp(Age / FMath::Max(1.f, C.Life), 0.f, 1.f));
	}
	FVector CloudC(const FAntReactCloud& C, float Now)
	{
		return FVector(C.Center) + FVector(C.Drift) * FMath::Max(0.f, Now - C.Born);
	}
	/** 0..1 how thick the cloud is at P (1 in the middle, 0 at its edge or when it is gone). */
	float CloudAt(const FAntReactCloud& C, const FVector& P, float Now)
	{
		const float Age = Now - C.Born;
		if (Age < 0.f || Age > C.Life) { return 0.f; }
		const float R = CloudR(C, Now);
		FVector D = P - CloudC(C, Now);
		D.Z *= 1.6f;   // flattened: a powder cloud hangs low and wide
		const float K = 1.f - D.Size() / R;
		const float Fade = 1.f - FMath::Clamp((Age - C.Life * 0.75f) / (C.Life * 0.25f), 0.f, 1.f);
		return FMath::Clamp(K * 1.6f, 0.f, 1.f) * Fade;
	}
}

float AAntReactNet::CloudThickness(uint8 Kind, const FVector& P, uint8* OutId, FVector* OutCenter, float* OutRadius) const
{
	const float Now = NetNow();
	float Best = 0.f;
	for (const FAntReactCloud& C : Clouds)
	{
		if (C.Kind != Kind) { continue; }
		const float K = CloudAt(C, P, Now);
		if (K > Best)
		{
			Best = K;
			if (OutId) { *OutId = C.Id; }
			if (OutCenter) { *OutCenter = CloudC(C, Now); }
			if (OutRadius) { *OutRadius = CloudR(C, Now); }
		}
	}
	return Best;
}

void AAntReactNet::EndCloud(uint8 Id)
{
	Clouds.RemoveAll([Id](const FAntReactCloud& C) { return C.Id == Id; });
}

void AAntReactNet::Sneeze(AActor* Who, const TCHAR* Why)
{
	UWorld* W = GetWorld();
	if (!Who) { return; }
	const float Now = NetNow();
	NextSneeze.Add(Who, Now + 7.f);
	Mark(Who, AntMark::Sneeze, 1.5f);
	if (ACritter* P = Cast<ACritter>(Who))
	{
		AntAudio::Everyone(this, P->Kind == 0 ? EAntSfx::DogSnort : EAntSfx::CatHiss, Head(P), 1.2f);
		P->Zoomies(4.f);
		Say(W, P->Kind == 0 ? NpcRoutine::Dog : NpcRoutine::Cat, P->Kind == 0 ? TEXT("Пфф! Апчхи! (Бадрик трёт нос лапой и носится кругами)") : TEXT("Пчхи! (кошка трясёт головой)"));
		ChainLog(FCString::Strstr(Why, TEXT("C13")) ? TEXT("C13") : TEXT("C7"), FString::Printf(TEXT("%s sneezes (%s)"), *P->DisplayName(), Why));
		return;
	}
	// a person: the nose (AntInside: the ants in it shoot out), the jolt, what was in the hand flies
	if (AAntInsideNet* In = AAntInsideNet::Get(W, true)) { In->ForceSneeze(Who); }
	else { AntAudio::Everyone(this, EAntSfx::Sneeze, Head(Who), 1.f); }
	if (AHumanCharacter* H = Cast<AHumanCharacter>(Who))
	{
		FString Dropped;
		if (H->Held && IsValid(H->Held))
		{
			AKitchenLoot* L = H->Held;
			H->Held = nullptr;
			Dropped = L->LootName;
			if (L->bHeld) { L->Throw(H->GetActorForwardVector() * 1100.f + FVector(0.f, 0.f, 900.f)); }
		}
		AntWeapons::Drop(H, TEXT("чихнул"));
		const bool bDad = H->Fig == HumanFig::Dad && !H->IsA<AHouseFolk>();
		UE_LOG(LogTemp, Display, TEXT("ANTREACT %s sneezes (%s)%s"), *H->GetName(), Why, Dropped.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", drops %s"), *Dropped));
		ChainLog(FCString::Strstr(Why, TEXT("C13")) ? TEXT("C13") : TEXT("C7"), FString::Printf(TEXT("%s sneezes%s"), bDad ? TEXT("the Dad") : *H->GetName(), Dropped.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" and drops %s"), *Dropped)));
		if (bDad)
		{
			static int32 Salt = 0;
			static const TCHAR* L[] = { TEXT("Апчхи! Кто перец рассыпал?!"), TEXT("А-а-апчхи! Да что ж такое..."), TEXT("Пчхи! Перец! Везде перец!") };
			Say(W, NpcRoutine::Dad, L[(Salt++) % 3]);
		}
	}
}

void AAntReactNet::TickClouds(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	for (int32 i = Clouds.Num() - 1; i >= 0; --i)
	{
		if (Now - Clouds[i].Born > Clouds[i].Life) { Clouds.RemoveAt(i); }
	}
	if (Clouds.Num() == 0) { SneezeAcc.Reset(); return; }
	CloudClock += Dt;
	if (CloudClock < 0.2f) { return; }
	static float DbgAt = 0.f;
	const bool bDbg = Now >= DbgAt;
	if (bDbg) { DbgAt = Now + 1.f; }
	const float Step = CloudClock;
	CloudClock = 0.f;
	// who breathes it: the residents and the pets
	TArray<AActor*> Noses;
	for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (!It->IsHidden()) { Noses.Add(*It); } }
	for (TActorIterator<ACritter> It(W); It; ++It) { if (!It->IsHidden()) { Noses.Add(*It); } }
	for (AActor* A : Noses)
	{
		const FVector Hd = Head(A);
		// a person walks through it with the whole body: the nearest point of the body's column (feet .. head) to the cloud
		const ACharacter* Ch = Cast<ACharacter>(A);
		const FVector Ft = Ch ? Ch->GetActorLocation() - FVector(0.f, 0.f, Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) : Hd;
		float Pepper = 0.f, Flour = 0.f;
		uint8 PepperId = 0;
		for (const FAntReactCloud& C : Clouds)
		{
			const FVector Q = FMath::ClosestPointOnSegment(CloudC(C, Now), Ft, Hd);
			const float K = FMath::Max(CloudAt(C, Hd, Now), CloudAt(C, Q, Now) * 0.8f);
			if ((C.Kind == AntCloudKind::Pepper || C.Kind == AntCloudKind::Smoke) && K > Pepper) { Pepper = K; PepperId = C.Id; }   // acrid smoke stings the nose like pepper
			else if (C.Kind == AntCloudKind::Flour) { Flour = FMath::Max(Flour, K); }
		}
		if (bDbg && Cast<AHumanCharacter>(A) && !A->IsA<AHouseFolk>()) { UE_LOG(LogTemp, Display, TEXT("ANTREACT clouds: the Dad at %s, pepper %.2f flour %.2f"), *A->GetActorLocation().ToCompactString(), Pepper, Flour); }
		if (Pepper > 0.05f)
		{
			float& Acc = SneezeAcc.FindOrAdd(A);
			Acc += Step * (0.5f + Pepper);
			const float* Next = NextSneeze.Find(A);
			if (Acc > 1.1f && (!Next || Now >= *Next))
			{
				Acc = 0.f;
				const FString* ChainOf = CloudChain.Find(PepperId);
				Sneeze(A, ChainOf && *ChainOf == TEXT("C13") ? TEXT("перец от тоста (C13)") : TEXT("перец в воздухе"));
			}
		}
		if (Flour > 0.15f)
		{
			AHumanCharacter* H = Cast<AHumanCharacter>(A);
			if (H && !HasMark(H, AntMark::Blind))
			{
				const bool bDad = H->Fig == HumanFig::Dad && !H->IsA<AHouseFolk>();
				UE_LOG(LogTemp, Display, TEXT("ANTREACT %s is blinded by flour (thickness %.2f)"), *H->GetName(), Flour);
				ChainLog(TEXT("C8"), FString::Printf(TEXT("%s blinded by flour for 5 s"), bDad ? TEXT("the Dad") : *H->GetName()));
				if (bDad)
				{
					Say(W, NpcRoutine::Dad, TEXT("Кха-кха! Мука! Ничего не вижу!"));
					H->Panic(2.5f);
					AntAudio::Everyone(this, EAntSfx::HumanPanic, Hd, 0.8f);
				}
				else if (H->IsA<AGrandmaCharacter>()) { Say(W, NpcRoutine::Gran, TEXT("Ох, батюшки, мука! Глаза!")); }
			}
			if (H) { Mark(H, AntMark::Blind, 5.f); }
		}
	}
	// ants in the clouds: pepper makes them sneeze (a stagger), flour turns them white for a while
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->Creature != 0) { continue; }
		const FVector P = Ant->GetActorLocation();
		for (const FAntReactCloud& C : Clouds)
		{
			const float K = CloudAt(C, P, Now);
			if (K <= 0.1f) { continue; }
			if (C.Kind == AntCloudKind::Flour)
			{
				if (!HasMark(Ant, AntMark::Floured)) { Ant->Toast(TEXT("Ты весь в муке: Человек видит тебя издалека, даже в темноте")); }
				Mark(Ant, AntMark::Floured, 25.f);
			}
			else if (C.Kind == AntCloudKind::Pepper && FMath::FRand() < 0.12f * K)
			{
				Ant->Stagger(0.5f, FVector::ZeroVector);
				Ant->Toast(TEXT("Апчхи! Перец!"));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// heat and cold: what melts
// ---------------------------------------------------------------------------------------------------------------------

const FAntMatProps* AAntReactNet::LootMat(const AKitchenLoot* L)
{
	if (!L) { return nullptr; }
	if (const FAntMatProps** Found = MatCache.Find(L)) { return *Found; }
	if (MatCache.Num() > 4000) { MatCache.Reset(); }
	const FAntMatProps* M = AntMaterials::ForName(L->LootName);
	MatCache.Add(L, M);
	return M;
}

void AAntReactNet::TickHeat(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	// the ice cubes lying about
	for (int32 i = Melts.Num() - 1; i >= 0; --i)
	{
		FAntReactMelt& M = Melts[i];
		const float T = AntReact::HeatAt(W, M.Pos);
		const float Rate = T <= 0.f ? 0.f : (T < 30.f ? 1.f / 40.f : 1.f / FMath::Lerp(14.f, 5.f, FMath::Clamp((T - 30.f) / 120.f, 0.f, 1.f)));
		const float Was = M.Left;
		M.Left = FMath::Max(0.f, M.Left - Rate * Dt);
		M.Life = Rate > 0.f ? M.Left / Rate : 999.f;
		if (Fluid && Was > M.Left)
		{
			// 60 ml a cube, given to the water as it melts
			Fluid->Spill(AntFluid::Water, FVector(M.Pos) + FVector(0.f, 0.f, 5.f), 0.06f * Litre * (Was - M.Left), 20.f);
		}
		if (M.Left <= 0.f)
		{
			AntMess::StainAt(this, MessKind::Water, M.Pos, FVector::UpVector, 90.f, i);
			Melts.RemoveAt(i);
			if (Melts.Num() == 0) { ChainLog(TEXT("C9"), TEXT("the last ice cube has melted")); }
		}
	}
	// food on something hot melts (not what the ants are carrying, not the big prizes)
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear || L->bTrophy || L->bMain || L->IsCarried() || L->bHeld || L->Weight >= 8) { continue; }
		const FAntMatProps* M = LootMat(L);
		if (!M || M->MeltAt < -999.f) { continue; }
		const FVector P = L->GetActorLocation();
		const float T = AntReact::HeatAt(W, P);
		if (T <= M->MeltAt + 3.f) { LootHeat.Remove(L); continue; }
		float& H = LootHeat.FindOrAdd(L);
		H += Dt * M->HeatTake * FMath::Clamp((T - M->MeltAt) / 30.f, 0.15f, 3.f);
		if (FMath::Fmod(H, 1.f) < Dt) { MulticastPuff(AntCloudKind::Steam, P + FVector(0.f, 0.f, 30.f), 60.f); }
		if (H < 4.f) { continue; }
		// melted: a puddle of what it was
		const uint8 Stain = MessKindOf(M->Stain);
		const FVector F = Floor(W, P, 60.f, 3000.f, L);
		if (Stain != 255) { AntMess::StainAt(this, Stain, F, FVector::UpVector, 260.f + 90.f * L->Weight, L->GetUniqueID() & 0xff); }
		if (Fluid)
		{
			AntFluid::ELiquid Liq = AntFluid::Water;
			if (AntFluid::LiquidFromName(M->MeltsInto, Liq)) { Fluid->Spill(Liq, F + FVector(0.f, 0.f, 8.f), 0.05f * Litre * L->Weight, 60.f); }
		}
		if (M->Sticky > 0.3f) { LevelV3::AddZone(0, F, 160.f + 60.f * L->Weight); }
		AntAudio::Everyone(this, EAntSfx::SplatJam, F, 0.7f);
		UE_LOG(LogTemp, Display, TEXT("ANTREACT %s melted at %.0f C (%s) at %s"), *L->LootName, T, *M->Id, *F.ToCompactString());
		ChainLog(TEXT("C12"), FString::Printf(TEXT("%s melted on something hot (%.0f C)"), *L->LootName, T));
		LootHeat.Remove(L);
		L->Destroy();
	}
}

float AntReact::HeatAt(const UWorld* W, const FVector& P)
{
	const AAntReactNet* N = AAntReactNet::Get(W);
	const int32 Mode = N ? N->HeatMode : -1;
	const bool bPower = !N || N->bPower;
	if (InBox(P, Spot::Freezer0, Spot::Freezer1)) { return -18.f; }
	if (InBox(P, Spot::Fridge0, Spot::Fridge1)) { return bPower ? 4.f : 12.f; }
	if (Mode == 0) { return 22.f; }
	const bool bAll = Mode == 1;
	if (InBox(P, Spot::RadK0, Spot::RadK1) || InBox(P, Spot::RadL0, Spot::RadL1)) { return 70.f; }
	if ((bAll || (bPower && AKitchenKnob::IsBurnerOn(const_cast<UWorld*>(W)))) && FVector::Dist2D(P, Spot::Burner) < 300.f && P.Z > Spot::Burner.Z - 40.f && P.Z < Spot::Burner.Z + 400.f) { return 180.f; }
	if (N && (bAll || (bPower && N->MicroUntil > N->NetNow())) && InBox(P, Spot::MicroBox0, Spot::MicroBox1)) { return 90.f; }
	if (bAll && InBox(P, Spot::ToasterSlots0, Spot::ToasterSlots1)) { return 85.f; }
	if (N)
	{
		for (const FAntReactMelt& M : N->Melts) { if (M.Kind == 0 && FVector::DistSquared(P, M.Pos) < 110.f * 110.f) { return 4.f; } }
	}
	return 22.f;
}

void AAntReactNet::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	// the kitchen radiator (the living-room one is the builder's) and the running microwave
	if (InBox(P, Spot::RadK0, Spot::RadK1)) { Env.Temp(64.f, 10.f); }
	if (MicroUntil > NetNow() && bPower && InBox(P, Spot::MicroBox0, Spot::MicroBox1)) { Env.Temp(75.f, 30.f); Env.Cause = TEXT("Микроволновка греет! Вылезай"); }
	for (const FAntReactMelt& M : Melts)
	{
		if (M.Kind == 0 && FVector::DistSquared(P, M.Pos) < 140.f * 140.f) { Env.Temp(3.f, 7.f); break; }
	}
	for (const FAntReactMark& M : Marks)
	{
		if (M.Who == Ant && M.Kind == AntMark::Burnt && M.Until - NetNow() > 12.5f) { Env.Flags |= AntStatus::Shocked; }
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// water: soaking, dissolving, the dishwasher's drip
// ---------------------------------------------------------------------------------------------------------------------

void AAntReactNet::TickWet(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	UAntFluidWorld* Fluid = UAntFluidWorld::Active(this);
	// the dishwasher: open, its wet racks drip off the door ramp for half a minute (C16)
	bool bDishOpen = false;
	for (TActorIterator<AKitchenDoor> It(W); It; ++It)
	{
		if (It->Kind == EKitchenDoorKind::Dishwasher && It->IsOpen()) { bDishOpen = true; }
	}
	if (bDishOpen && !bDishWasOpen)
	{
		DishDripUntil = Now + 30.f;
		ChainLog(TEXT("C16"), TEXT("the dishwasher door is open: the wet racks drip off the ramp"));
		if (Fluid) { Fluid->AddSource(AntFluid::Water, Spot::DishLip, 0.3f, 30.f, 50.f); }
	}
	bDishWasOpen = bDishOpen;
	if (!Fluid) { return; }
	// food in water: sugar and salt dissolve, bread goes soggy
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear || L->bTrophy || L->bMain || L->bHeld) { continue; }
		const FVector P = L->GetActorLocation();
		const FAntMatProps* M = LootMat(L);
		if (!M || (!M->bDissolves && M->Absorb <= 1.f)) { continue; }
		if (Fluid->DepthAt(P) < 2.f) { LootWet.Remove(L); continue; }
		if (!M) { continue; }
		float& T = LootWet.FindOrAdd(L);
		T += Dt;
		if (M->bDissolves && T > 4.f && !L->IsCarried())
		{
			UE_LOG(LogTemp, Display, TEXT("ANTREACT %s dissolved in water at %s"), *L->LootName, *P.ToCompactString());
			ChainLog(TEXT("C21"), FString::Printf(TEXT("%s dissolved: the water is sweet"), *L->LootName));
			SweetPos = P;
			SweetAt = Now;
			LootWet.Remove(L);
			L->Destroy();
			continue;
		}
		if (M->Absorb > 1.f && T > 2.f && L->Weight < 20 && !L->Tags.Contains(TEXT("AntSoggy")))
		{
			L->Tags.Add(TEXT("AntSoggy"));
			L->Weight = L->Weight * 2;
			L->Color = L->Color * 0.62f;
			L->OnRep_Visual();
			UE_LOG(LogTemp, Display, TEXT("ANTREACT %s soaked up water: weight %d"), *L->LootName, L->Weight);
		}
	}
	// sweet water draws a fly (once a minute at most)
	if (SweetAt > 0.f && Now - SweetAt > 6.f)
	{
		SweetAt = -1.f;
		if (AAntKitchenGameMode* GM = GMOf(W))
		{
			GM->SpawnFly();
			AFlyCharacter* Newest = nullptr;
			for (TActorIterator<AFlyCharacter> It(W); It; ++It) { if (!Newest || It->GetUniqueID() > Newest->GetUniqueID()) { Newest = *It; } }
			if (Newest) { Newest->SetActorLocation(SweetPos + FVector(0.f, 0.f, 700.f), false, nullptr, ETeleportType::TeleportPhysics); }
		}
		ChainLog(TEXT("C21"), TEXT("a fly comes for the sweet water"));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// round things under the Dad's feet
// ---------------------------------------------------------------------------------------------------------------------

void AAntReactNet::TickRollers(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	static float Next = 0.f, Scan = 0.f;
	if (Now < Next) { return; }
	Next = Now + 0.1f;
	if (Now >= Scan)
	{
		// every piece of round food lying on a floor is a roller (the peas of C11, grapes from the fridge...)
		Scan = Now + 2.f;
		for (int32 i = Rollers.Num() - 1; i >= 0; --i) { if (!Rollers[i].Loot.IsValid()) { Rollers.RemoveAt(i); } }
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->bIsGear || It->GetActorLocation().Z > 120.f) { continue; }
			const FAntMatProps* M = LootMat(*It);
			if (M && M->bRolls && !Rollers.ContainsByPredicate([&It](const FPeaWatch& R) { return R.Loot.Get() == *It; }))
			{
				FPeaWatch& R = Rollers.AddDefaulted_GetRef();
				R.Loot = *It;
				R.Born = Now;
			}
		}
	}
	if (Rollers.Num() == 0) { return; }
	AHumanCharacter* H = Dad(W);
	if (!H || H->IsDown() || H->GetVelocity().Size2D() < 60.f || !H->HasJointedBody()) { return; }
	for (int32 Leg = 0; Leg < 2; ++Leg)
	{
		const FVector Sole = H->GetSoleLocation(Leg);
		for (const FPeaWatch& R : Rollers)
		{
			const AKitchenLoot* L = R.Loot.Get();
			if (!L || L->IsCarried()) { continue; }
			const FVector P = L->GetActorLocation();
			if (FVector::DistSquared2D(P, Sole) < 75.f * 75.f && FMath::Abs(P.Z - Sole.Z) < 90.f)
			{
				const bool bRun = H->GetVelocity().Size2D() > 900.f;
				AAntReactProp* Bowl = AAntReactProp::Find(W, AntProp::PeaBowl);
				if (H->TrySlip(P, bRun ? 1.f : 0.7f, false, Bowl ? (AActor*)Bowl->By.Get() : nullptr))
				{
					UE_LOG(LogTemp, Display, TEXT("ANTREACT the Dad stepped on %s at %s and went down"), *L->LootName, *P.ToCompactString());
					ChainLog(TEXT("C11"), FString::Printf(TEXT("the Dad steps on %s and falls like on marbles"), *L->LootName));
					Say(W, NpcRoutine::Dad, TEXT("Уа-а-а! ...Горох?! Кто рассыпал горох?!"));
					if (NpcRoutine::Enabled()) { NpcRoutine::AddMess(W, NpcRoutine::Pickup, P, Bowl ? (AActor*)Bowl : (AActor*)this, TEXT("горох по полу")); }
				}
				return;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the microwave (C15)
// ---------------------------------------------------------------------------------------------------------------------

void AAntReactNet::TickMicrowave(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	if ((MicroUntil > 0.f || MicroBeepAt > 0.f) && !bPower)
	{
		MicroUntil = -1.f;
		MicroBeepAt = -1.f;
		ChainLog(TEXT("C15"), TEXT("the microwave dies with the power"));
	}
	if (MicroUntil > 0.f && Now >= MicroUntil)
	{
		MicroUntil = -1.f;
		MicroBeepAt = Now;
		MicroBeeps = 0;
		ChainLog(TEXT("C15"), TEXT("the microwave is done: it beeps"));
	}
	if (MicroBeepAt > 0.f && Now >= MicroBeepAt)
	{
		const FVector At = (Spot::MicroBox0 + Spot::MicroBox1) * 0.5f;
		AntAudio::Everyone(this, EAntSfx::MicroBeep, At, 1.f);
		Noise(W, At, 0.75f, AntNoise::Microwave, this, MicroBy.Get());
		++MicroBeeps;
		if (const AHumanCharacter* DH = Dad(W)) { if (MicroBeeps % 3 == 1) { UE_LOG(LogTemp, Display, TEXT("ANTREACT microwave beeps (%d): the Dad is %.0f away"), MicroBeeps, FVector::Dist2D(DH->GetActorLocation(), Spot::MicroButton)); } }
		MicroBeepAt = Now + (MicroBeeps % 3 == 0 ? 8.f : 0.6f);
		if (MicroBeeps == 3 && LevelV3::IsHome(LevelV3::Grandma)) { Say(W, NpcRoutine::Gran, TEXT("Что там пищит? Сынок, микроволновка!")); }
		if (MicroBeeps > 60) { MicroBeepAt = -1.f; }
	}
	if (MicroBeepAt > 0.f)
	{
		AHumanCharacter* H = Dad(W);
		if (H && FVector::Dist2D(H->GetActorLocation(), Spot::MicroButton) < 1650.f)   // (at the worktop: it is 1700 deep)
		{
			MicroBeepAt = -1.f;
			H->PlayReach(Spot::MicroButton);
			Say(W, NpcRoutine::Dad, TEXT("Кто включил микроволновку?! Пустую!"));
			ChainLog(TEXT("C15"), FString::Printf(TEXT("the Dad came to the microwave after %d beeps and opened it"), MicroBeeps));
			if (AAntReactProp* B = AAntReactProp::Find(W, AntProp::MicroButton)) { B->ResetProp(); }
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the robot vacuum (the life branch's gadget; the reaction rules are ours, C17)
// ---------------------------------------------------------------------------------------------------------------------

void AAntReactNet::TickVacuums(float Dt)
{
	UWorld* W = GetWorld();
	static TMap<TWeakObjectPtr<AActor>, FVector> LastTrail;
	static TMap<TWeakObjectPtr<AActor>, int32> Eaten;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* V = *It;
		if (!V || !V->GetClass()->GetName().Contains(TEXT("RobotVacuum"))) { continue; }
		const FVector P = V->GetActorLocation();
		// peas and grapes: sucked up (it chokes after a handful: stuck for a while, if the gadget lets us say so)
		for (int32 i = Rollers.Num() - 1; i >= 0; --i)
		{
			AKitchenLoot* L = Rollers[i].Loot.Get();
			if (L && !L->IsCarried() && FVector::DistSquared2D(L->GetActorLocation(), P) < 300.f * 300.f)
			{
				int32& N = Eaten.FindOrAdd(V);
				++N;
				AntAudio::Everyone(this, EAntSfx::Crunch, P, 0.8f);
				L->Destroy();
				Rollers.RemoveAt(i);
				ChainLog(TEXT("C17"), FString::Printf(TEXT("the robot vacuum sucks up a pea (%d)"), N));
				if (N == 6)
				{
					if (UFunction* F = V->FindFunction(TEXT("AntReactStuck"))) { if (F->ParmsSize == sizeof(float)) { float S = 12.f; V->ProcessEvent(F, &S); } }
					AntAudio::Everyone(this, EAntSfx::Wobble, P, 1.f);
					ChainLog(TEXT("C17"), TEXT("the robot vacuum chokes on the peas"));
				}
			}
		}
		// flour: it spreads it all over the floor
		bool bFlour = false;
		for (const FAntReactCloud& C : Clouds) { if (C.Kind == AntCloudKind::Flour && FVector::Dist2D(P, C.Center) < C.Radius) { bFlour = true; } }
		FVector* Last = LastTrail.Find(V);
		if (bFlour && (!Last || FVector::Dist2D(*Last, P) > 280.f))
		{
			LastTrail.Add(V, P);
			AntMess::StainAt(this, MessKind::Flour, FVector(P.X, P.Y, 2.f), FVector::UpVector, 220.f, (int32)P.X & 0xff);
			MulticastPuff(AntCloudKind::Flour, P + FVector(0.f, 0.f, 60.f), 120.f);
			ChainLog(TEXT("C17"), TEXT("the robot vacuum drives through the flour: white tracks"));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// every machine: what the clouds, the melts, the marks and the blackout look like
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	/** -AntReactDo="t:cam x y z tx ty tz [fov];t:shot name;t:camoff;t:info" on any machine (the client of a co-op test too). */
	TArray<TPair<float, FString>> GLocalScript;
	bool GLocalParsed = false;
}

void AAntReactNet::LocalTick(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = NetNow();
	LocalClock += Dt;
	if (!GLocalParsed)
	{
		GLocalParsed = true;
		FString Script;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntReactDo="), Script, false))
		{
			TArray<FString> Steps;
			Script.ParseIntoArray(Steps, TEXT(";"));
			for (const FString& S : Steps)
			{
				FString A, B;
				if (S.Split(TEXT(":"), &A, &B)) { GLocalScript.Add(TPair<float, FString>(FCString::Atof(*A), B.TrimStartAndEnd())); }
			}
		}
	}
	for (int32 k = GLocalScript.Num() - 1; k >= 0; --k)
	{
		if (W->GetTimeSeconds() < GLocalScript[k].Key) { continue; }
		TArray<FString> A;
		GLocalScript[k].Value.ParseIntoArrayWS(A);
		GLocalScript.RemoveAt(k);
		if (A.Num() == 0) { continue; }
		auto Nm = [&A](int32 i) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : 0.f; };
		if (A[0] == TEXT("shot")) { AntCloth::DevShot(W, TEXT("chains"), A.IsValidIndex(1) ? A[1] : FString(TEXT("local"))); }
		else if (A[0] == TEXT("cam")) { AntCloth::DevCam(W, FVector(Nm(1), Nm(2), Nm(3)), FVector(Nm(4), Nm(5), Nm(6)), A.IsValidIndex(7) ? Nm(7) : 70.f); }
		else if (A[0] == TEXT("camoff")) { AntCloth::DevCam(W, FVector::ZeroVector, FVector::ZeroVector, 0.f); }
		else if (A[0] == TEXT("info"))
		{
			int32 Visible = 0;
			for (TObjectIterator<ULocalLightComponent> It; It; ++It) { if (It->GetWorld() == W && It->IsVisible() && LevelV2::InHouse(It->GetComponentLocation())) { ++Visible; } }
			UE_LOG(LogTemp, Display, TEXT("ANTREACT local (%s): %s, %d house lamps lit, dark applied %d"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"), *Describe(), Visible, bDarkApplied ? 1 : 0);
		}
	}
	if (bDarkApplied == bPower) { ApplyBlackout(!bPower); }   // (a late joiner: the OnRep may have come before the lights existed)

	// clouds: soft puffs inside, and the powder pouring over the edge while it pours
	AAntFxManager* Fx = AAntFxManager::Get(this);
	FxAcc += Dt;
	if (Fx && FxAcc >= 0.05f)
	{
		const float Step = FxAcc;
		FxAcc = 0.f;
		for (const FAntReactCloud& C : Clouds)
		{
			const float Age = Now - C.Born;
			if (Age < 0.f || Age > C.Life) { continue; }
			const float R = CloudR(C, Now);
			const FVector Ctr = CloudC(C, Now);
			const FLinearColor Col = C.Kind == AntCloudKind::Pepper ? PepperCol : C.Kind == AntCloudKind::Flour ? FlourCol : C.Kind == AntCloudKind::Smoke ? FLinearColor(0.16f, 0.15f, 0.14f) : FLinearColor(0.9f, 0.9f, 0.9f);
			const float Fade = 1.f - FMath::Clamp((Age - C.Life * 0.6f) / (C.Life * 0.4f), 0.f, 1.f);
			const bool bFlourC = C.Kind == AntCloudKind::Flour;
			const int32 N = FMath::Max(1, FMath::RoundToInt(Step * (bFlourC ? 42.f : 70.f) * Fade));
			for (int32 i = 0; i < N; ++i)
			{
				FAntFxBit B;
				FVector O = FMath::VRand() * FMath::Sqrt(FMath::FRand()) * R * 0.85f;
				O.Z *= 0.55f;
				B.Pos = Ctr + O;
				B.Vel = FVector(C.Drift) + FMath::VRand() * 35.f + FVector(0.f, 0.f, -12.f);
				const float S = R * (bFlourC ? FMath::FRandRange(0.0016f, 0.0028f) : FMath::FRandRange(0.0009f, 0.0016f));
				B.Scale0 = FVector(S * 0.5f);
				B.Scale1 = FVector(S * 1.6f);
				B.Color = FLinearColor(Col.R, Col.G, Col.B, (bFlourC ? 0.2f : 0.5f) * Fade);
				B.Life = FMath::FRandRange(1.8f, 3.2f);
				B.FadeIn = 0.3f;
				B.Drag = 0.6f;
				B.MinAmbient = 0.7f;
				B.Look = EAntFxLook::Soft;
				Fx->Emit(B);
			}
			if (C.Kind == AntCloudKind::Pepper)
			{
				// the pepper itself: dark specks drifting in the haze (what reads as "pepper" up close)
				const int32 M = FMath::Max(1, FMath::RoundToInt(Step * 70.f * Fade));
				for (int32 i = 0; i < M; ++i)
				{
					FAntFxBit B;
					FVector O = FMath::VRand() * FMath::Sqrt(FMath::FRand()) * R * 0.7f;
					O.Z *= 0.5f;
					B.Pos = Ctr + O;
					B.Vel = FVector(C.Drift) + FMath::VRand() * 60.f + FVector(0.f, 0.f, -40.f);
					B.Scale0 = FVector(FMath::FRandRange(0.08f, 0.16f));
					B.Scale1 = B.Scale0 * 0.8f;
					B.Color = FLinearColor(PepperSpeck.R, PepperSpeck.G, PepperSpeck.B, 1.f);
					B.Life = FMath::FRandRange(1.5f, 3.f);
					B.Drag = 0.8f;
					B.Gravity = 60.f;
					B.Look = EAntFxLook::Solid;
					Fx->Emit(B);
				}
			}
			if (C.PourFor > 0.f && Age < C.PourFor)
			{
				// the avalanche: a falling curtain of powder from the edge to the floor
				const int32 M = FMath::Max(1, FMath::RoundToInt(Step * 40.f));
				for (int32 i = 0; i < M; ++i)
				{
					FAntFxBit B;
					B.Pos = FVector(C.Source) + FVector(FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-20.f, 10.f));
					B.Vel = FVector(FMath::FRandRange(-40.f, 40.f), -FMath::FRandRange(60.f, 200.f), -FMath::FRandRange(200.f, 600.f));
					B.Gravity = 3200.f;
					B.Scale0 = FVector(FMath::FRandRange(0.25f, 0.6f));
					B.Scale1 = B.Scale0 * 2.2f;
					B.Color = FLinearColor(Col.R, Col.G, Col.B, 0.55f);
					B.Life = 1.2f;
					B.FloorZ = 4.f;
					B.MinAmbient = 0.6f;
					B.Look = EAntFxLook::Soft;
					Fx->Emit(B);
				}
			}
		}
		// the melting ice drips and steams a little
		for (const FAntReactMelt& M : Melts)
		{
			if (FMath::FRand() < Step * 1.5f) { AntFx::Droplets(this, FVector(M.Pos) + FVector(0.f, 0.f, 12.f), 1, 3.f); }
		}
	}
	DrawMelts();
	UpdateCatEyes(Dt);

	// the marks: white with flour, sooty after a zap (every machine tints its own copy of the ant)
	static float TintAt = 0.f;
	if (LocalClock >= TintAt)
	{
		TintAt = LocalClock + 0.25f;
		static TSet<TWeakObjectPtr<AAntCharacter>> Tinted;
		TSet<TWeakObjectPtr<AAntCharacter>> Now2;
		for (const FAntReactMark& M : Marks)
		{
			AAntCharacter* A = Cast<AAntCharacter>(M.Who);
			if (!A || (M.Kind != AntMark::Floured && M.Kind != AntMark::Burnt) || Now > M.Until) { continue; }
			const float Left = FMath::Clamp((M.Until - Now) / 6.f, 0.f, 1.f);
			const FLinearColor C = M.Kind == AntMark::Floured ? FMath::Lerp(FLinearColor::White, FLinearColor(3.2f, 3.2f, 3.1f), Left) : FMath::Lerp(FLinearColor::White, FLinearColor(0.25f, 0.22f, 0.2f), Left);
			A->SetMessTint(C);
			Now2.Add(A);
			if (M.Kind == AntMark::Burnt && FMath::FRand() < 0.15f) { AntFx::Steam(this, A->GetActorLocation() + FVector(0.f, 0.f, 8.f), 6.f, 40.f); }
		}
		for (const TWeakObjectPtr<AAntCharacter>& T : Tinted) { if (T.IsValid() && !Now2.Contains(T)) { T->SetMessTint(FLinearColor::White); } }
		Tinted = MoveTemp(Now2);
	}
}

void AAntReactNet::DrawMelts()
{
	UInstancedStaticMeshComponent* I = MeltMesh[0];
	if (!I) { return; }
	int32 n = 0;
	for (const FAntReactMelt& M : Melts) { if (M.Kind == 0) { ++n; } }
	if (I->GetInstanceCount() != n) { I->ClearInstances(); for (int32 k = 0; k < n; ++k) { I->AddInstance(FTransform::Identity, true); } }
	int32 k = 0;
	const float Now = NetNow();
	for (const FAntReactMelt& M : Melts)
	{
		if (M.Kind != 0) { continue; }
		// the server's Left, run on locally at the rate it had (Life = seconds left at that rate)
		const float S = FMath::Max(0.05f, M.Left);
		const float K = FMath::Pow(S, 0.5f);
		I->UpdateInstanceTransform(k++, FTransform(FRotator(0.f, M.Yaw, 0.f), FVector(M.Pos) + FVector(0.f, 0.f, 24.f * K - 22.f), FVector(K, K, K * 0.92f)), true, false, true);
	}
	I->MarkRenderStateDirty();
}

void AAntReactNet::UpdateCatEyes(float Dt)
{
	UWorld* W = GetWorld();
	const bool bWant = !bPower;
	ACritter* Cat = bWant ? Pet(W, 1) : nullptr;
	if (!Cat)
	{
		for (UStaticMeshComponent* E : CatEyes) { if (E) { E->SetVisibility(false); } }
		if (CatGlow) { CatGlow->SetVisibility(false); }
		return;
	}
	if (CatEyes.Num() == 0)
	{
		UStaticMesh* Drop = Map4Look::Prop(TEXT("SM_M4_Droplet"));
		for (int32 i = 0; i < 2; ++i)
		{
			UStaticMeshComponent* E = NewObject<UStaticMeshComponent>(this);
			E->SetStaticMesh(Drop ? Drop : AntKit::ShapeMesh(EAntShape::Sphere));
			E->SetMobility(EComponentMobility::Movable);
			E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			E->SetCastShadow(false);
			E->SetAbsolute(true, true, true);
			E->ComponentTags.Add(TEXT("M4Fx"));
			E->RegisterComponent();
			AntFx::MakeGlow(E, FLinearColor(4.f, 6.5f, 0.6f));
			CatEyes.Add(E);
		}
		CatGlow = NewObject<UPointLightComponent>(this);
		CatGlow->SetIntensity(0.6f);
		CatGlow->SetAttenuationRadius(260.f);
		CatGlow->SetLightColor(FLinearColor(0.7f, 1.f, 0.3f));
		CatGlow->SetCastShadows(false);
		CatGlow->SetAbsolute(true, true, true);
		CatGlow->RegisterComponent();
	}
	// the head: the component of the cat's model named after it, else a guess from the body
	FVector HeadAt = Cat->GetActorLocation() + Cat->GetActorForwardVector() * Cat->BodyLength() * 0.48f + FVector(0.f, 0.f, Cat->BodyHeight() * 0.75f);
	FVector Fwd = Cat->GetActorForwardVector();
	TInlineComponentArray<UStaticMeshComponent*> Comps(Cat);
	for (UStaticMeshComponent* C : Comps)
	{
		if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName().Contains(TEXT("Cat_Head")))
		{
			HeadAt = C->Bounds.Origin;
			break;
		}
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	const float Blink = FMath::Fmod(LocalClock, 3.7f) < 0.12f ? 0.1f : 1.f;
	for (int32 i = 0; i < CatEyes.Num(); ++i)
	{
		CatEyes[i]->SetVisibility(true);
		CatEyes[i]->SetWorldLocation(HeadAt + Fwd * 62.f + Right * (i == 0 ? -24.f : 24.f) + FVector(0.f, 0.f, 18.f));
		CatEyes[i]->SetWorldScale3D(FVector(0.16f, 0.16f, 0.16f * Blink));
	}
	CatGlow->SetVisibility(true);
	CatGlow->SetWorldLocation(HeadAt + Fwd * 80.f + FVector(0.f, 0.f, 18.f));
}

// =====================================================================================================================
// AAntReactProp: the reactive objects
// =====================================================================================================================

namespace
{
	UStaticMeshComponent* NewPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Rel, bool bSolid)
	{
		if (!Mesh) { return nullptr; }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Parent);
		C->SetRelativeTransform(Rel);
		C->SetCanEverAffectNavigation(false);
		if (bSolid) { KitchenArt::SetSolid(C, KitchenArt::ESolid::Moving); }
		else { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		C->RegisterComponent();
		Owner->AddInstanceComponent(C);
		return C;
	}
	/** A piece of the modelled kitchen (SM_K_*, SM_L_*, SM_V3_*) at its own scale; Size > 0 refits its largest side to Size. */
	UStaticMeshComponent* KPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& Loc, const FRotator& Rot, bool bSolid, float Size = 0.f)
	{
		UStaticMesh* M = KitchenArt::Mesh(Name);
		if (!M) { return nullptr; }
		float S = KitchenArt::ImportScale;
		if (Size > 0.f) { const FVector E = M->GetBoundingBox().GetSize(); S = Size / FMath::Max(1.f, E.GetMax()); }
		UStaticMeshComponent* C = NewPart(Owner, Parent, M, FTransform(Rot, Loc, FVector(S)), bSolid);
		if (C) { KitchenArt::ApplyLook(C, Name, 0); }
		return C;
	}
	/** A map v4 prop (SM_M4_*, SM_R_*) fitted so its largest side is Size; Fallback when it is not in the build. */
	UStaticMeshComponent* MPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& Loc, const FRotator& Rot, float Size, bool bSolid, const TCHAR* Look = nullptr, const TCHAR* Fallback = nullptr)
	{
		UStaticMesh* M = Map4Look::Prop(Name);
		const TCHAR* Used = Name;
		if (!M && Fallback) { M = Map4Look::Prop(Fallback); Used = Fallback; if (!M) { M = KitchenArt::Mesh(Fallback); } }
		if (!M) { return nullptr; }
		const FVector E = M->GetBoundingBox().GetSize();
		UStaticMeshComponent* C = NewPart(Owner, Parent, M, FTransform(Rot, Loc, FVector(Size / FMath::Max(0.01f, E.GetMax()))), bSolid);
		if (!C) { return nullptr; }
		Map4Look::ApplySlots(C);
		if (Look) { if (const Map4Look::FLook* L = Map4Look::Named(Look)) { C->SetMaterial(0, Map4Look::Get(*L)); } }
		if (Used != Name) { UE_LOG(LogTemp, Display, TEXT("ANTREACT %s is not in this build: %s stands in"), Name, Used); }
		return C;
	}
	/** A thread / cable / rubber band between two world points (map v4's thread model along Z, a unit cylinder). */
	UStaticMeshComponent* Thread(AActor* Owner, const FVector& A, const FVector& B, float Thick, const TCHAR* Look)
	{
		UStaticMesh* M = Map4Look::Prop(TEXT("SM_M4_Thread"));
		if (!M) { M = AntKit::ShapeMesh(EAntShape::Cylinder); }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetAbsolute(true, true, true);
		C->ComponentTags.AddUnique(TEXT("M4Thread"));
		C->RegisterComponent();
		Owner->AddInstanceComponent(C);
		if (const Map4Look::FLook* L = Map4Look::Named(Look)) { C->SetMaterial(0, Map4Look::Get(*L)); }
		const FVector D = B - A;
		C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).ToQuat(), (A + B) * 0.5f, FVector(Thick / 100.f, Thick / 100.f, FMath::Max(1.f, (float)D.Size()) / 100.f)));
		return C;
	}
	void SetThread(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thick)
	{
		if (!C) { return; }
		const FVector D = B - A;
		C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).ToQuat(), (A + B) * 0.5f, FVector(Thick / 100.f, Thick / 100.f, FMath::Max(1.f, (float)D.Size()) / 100.f)));
	}
	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	FVector Arc(const FVector& A, const FVector& B, float T, float H)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return FMath::Lerp(A, B, T) + FVector(0.f, 0.f, H * 4.f * T * (1.f - T));
	}
	/** A falling object's arc: up first, then down faster (gravity-ish): T 0..1. */
	FVector Drop(const FVector& A, const FVector& B, float T, float H)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		const FVector Flat = FMath::Lerp(A, B, T);
		const float Z = FMath::Lerp((float)A.Z, (float)B.Z, T * T) + H * 4.f * T * (1.f - T);
		return FVector(Flat.X, Flat.Y, Z);
	}
	const int32 NumBooks = 6;
	const int32 NumBlocks = 6;
}

AAntReactProp::AAntReactProp()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	InteractRadius = 150.f;
}

void AAntReactProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntReactProp, Kind);
	DOREPLIFETIME(AAntReactProp, State);
	DOREPLIFETIME(AAntReactProp, StateAt);
	DOREPLIFETIME(AAntReactProp, FlyFrom);
	DOREPLIFETIME(AAntReactProp, FlyTo);
	DOREPLIFETIME(AAntReactProp, FlyTime);
	DOREPLIFETIME(AAntReactProp, FlyArc);
	DOREPLIFETIME(AAntReactProp, FlySpin);
	DOREPLIFETIME(AAntReactProp, RestRot);
	DOREPLIFETIME(AAntReactProp, Level);
	DOREPLIFETIME(AAntReactProp, Aux);
	DOREPLIFETIME(AAntReactProp, Index);
}

float AAntReactProp::NetNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? (float)GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

AAntReactProp* AAntReactProp::Find(const UWorld* W, uint8 InKind, int32 InIndex)
{
	if (!W) { return nullptr; }
	for (TActorIterator<AAntReactProp> It(const_cast<UWorld*>(W)); It; ++It)
	{
		if (It->Kind == InKind && (InIndex < 0 || It->Index == InIndex) && !It->IsActorBeingDestroyed()) { return *It; }
	}
	return nullptr;
}

void AAntReactProp::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorTransform();
	Build();
}

FVector AAntReactProp::UsePoint() const
{
	return GetActorTransform().TransformPosition(UseLocal);
}

float AAntReactProp::DistanceTo(const FVector& P) const
{
	if (Kind == AntProp::PowerStrip) { return FMath::Max(0.f, (float)FVector::Dist(P, FVector(Aux)) - UsePad); }
	return FMath::Max(0.f, (float)FVector::Dist(P, UsePoint()) - UsePad);
}

FString AAntReactProp::DoorNoun() const { return AntProp::Ru(Kind); }

bool AAntReactProp::CanInteract(const AAntCharacter* Ant) const
{
	if (bDone || !bHeldWork || State != 0) { return false; }
	return true;
}

bool AAntReactProp::CanPull(AAntCharacter* Ant, bool bTell) const
{
	if ((Kind == AntProp::ToasterSpring || Kind == AntProp::MicroButton) && !AntReact::HasPower(this))
	{
		if (bTell && Ant) { Ant->Toast(TEXT("Нет света — не работает. Сначала пусть папа включит щиток")); }
		return false;
	}
	if (Kind == AntProp::Breaker && !AntReact::HasPower(this))
	{
		if (bTell && Ant) { Ant->Toast(TEXT("Рубильник уже внизу: света нет")); }
		return false;
	}
	return true;
}

FString AAntReactProp::GetPrompt(const AAntCharacter* Ant) const
{
	const int32 Pct = FMath::RoundToInt(Progress * 100.f);
	const FString P = Pct > 0 ? FString::Printf(TEXT(" — %d%%"), Pct) : FString();
	switch (Kind)
	{
	case AntProp::Pepper:        return TEXT("Опрокинуть перечницу (держать [E], вдвоём)") + P;
	case AntProp::Flour:         return TEXT("Прогрызть мешок муки (держать [E])") + P;
	case AntProp::IceTray:       return TEXT("Столкнуть лоток со льдом с края (держать [E], вдвоём)") + P;
	case AntProp::PeaBowl:       return TEXT("Столкнуть миску с горохом (держать [E], вдвоём)") + P;
	case AntProp::Slingshot:     return TEXT("Натянуть рогатку (держать [E], втроём): кто стоит в кармашке — полетит на стол") + P;
	case AntProp::Chocolate:     return TEXT("Столкнуть шоколадку на горячую батарею (держать [E])") + P;
	case AntProp::ToasterSpring: return (AntReact::HasPower(this) ? TEXT("Взвести пружину тостера до упора (держать [E], втроём)") : TEXT("Тостер без света не работает")) + P;
	case AntProp::BookRow:       return TEXT("Толкнуть крайнюю книгу — домино! (держать [E], вдвоём)") + P;
	case AntProp::MicroButton:   return (AntReact::HasPower(this) ? TEXT("Нажать «Старт» микроволновки (держать [E])") : TEXT("Микроволновка без света не работает")) + P;
	case AntProp::Mentos:        return TEXT("Столкнуть ментос в бутылку колы (держать [E], вдвоём)") + P;
	case AntProp::Sponge:        return TEXT("Сдвинуть губку на самый край (держать [E]): намокнет — свалится") + P;
	case AntProp::Balloon:       return TEXT("Натереть шарик о ковёр (держать [E], втроём): статика") + P;
	case AntProp::PowerStrip:    return TEXT("Перегрызть провод удлинителя (держать [E]) — под током!") + P;
	case AntProp::Breaker:       return TEXT("Опустить рубильник (держать [E], втроём): свет погаснет") + P;
	default: return FString();
	}
}

void AAntReactProp::Build()
{
	USceneComponent* R = Root;
	const bool bServer = HasAuthority();
	Need = 14.f;
	Seconds = 3.f;
	UsePad = 40.f;
	switch (Kind)
	{
	case AntProp::Pepper:
		Body = KPart(this, R, TEXT("SM_K_Shaker"), FVector::ZeroVector, FRotator::ZeroRotator, true);
		UseLocal = FVector(0.f, 0.f, 80.f);
		UsePad = 60.f;
		break;
	case AntProp::Flour:
		Body = KPart(this, R, TEXT("SM_K_FlourSack"), FVector::ZeroVector, FRotator(0.f, 180.f, 0.f), true);
		Need = 6.f; Seconds = 6.f;
		UseLocal = FVector(0.f, -240.f, 120.f);
		UsePad = 120.f;
		break;
	case AntProp::IceTray:
		Body = KPart(this, R, TEXT("SM_K_IceTray"), FVector::ZeroVector, FRotator::ZeroRotator, true);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector At((i % 3 - 1) * 82.f, (i / 3 - 0.5f) * 110.f, 70.f);
			if (UStaticMeshComponent* C = MPart(this, R, TEXT("SM_R_IceCube"), At, FRotator(0.f, i * 17.f, 0.f), 62.f, false, nullptr, TEXT("SM_M4_Gem"))) { Parts.Add(C); }
		}
		UseLocal = FVector(0.f, 0.f, 60.f);
		UsePad = 120.f;
		break;
	case AntProp::PeaBowl:
		Body = KPart(this, R, TEXT("SM_K_Bowl"), FVector::ZeroVector, FRotator::ZeroRotator, true);
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = i * 2.4f, D = 30.f + 12.f * (i % 4);
			if (UStaticMeshComponent* C = MPart(this, R, TEXT("SM_M4_Pea"), FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 70.f + 8.f * (i % 3)), FRotator(i * 31.f, i * 47.f, 0.f), 34.f, false, TEXT("Leaf"))) { Parts.Add(C); }
		}
		UseLocal = FVector(0.f, 0.f, 60.f);
		UsePad = 200.f;
		break;
	case AntProp::Slingshot:
	{
		// the fork stands on the floor, its band (two rubber strands) to the leather pouch; the pull drags the pouch back
		Body = MPart(this, R, TEXT("SM_R_Slingshot"), FVector::ZeroVector, FRotator::ZeroRotator, 300.f, true, nullptr, TEXT("SM_M4_WoodSpoon"));
		UStaticMeshComponent* Pouch = MPart(this, R, TEXT("SM_R_SlingPouch"), FVector(-110.f, 0.f, 150.f), FRotator::ZeroRotator, 70.f, false, TEXT("Leather"), TEXT("SM_M4_Thimble"));
		if (Pouch) { Parts.Add(Pouch); }
		Parts.Add(Thread(this, FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 9.f, TEXT("Rubber")));
		Parts.Add(Thread(this, FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 9.f, TEXT("Rubber")));
		Need = 21.f; Seconds = 3.f;
		UseLocal = FVector(-120.f, 0.f, 40.f);
		UsePad = 120.f;
		break;
	}
	case AntProp::Chocolate:
	{
		const AntArt::FPart& P = AntArt::Get(TEXT("SM_Food_Chocolate"));
		if (P.IsValid())
		{
			Body = AntArt::AddArt(this, R, P);
			const FVector E = P.Bounds.GetSize();
			Body->SetRelativeScale3D(FVector(260.f / FMath::Max(1.f, E.GetMax())));
			Body->SetRelativeLocation(FVector(0.f, 0.f, -P.Bounds.Min.Z * 260.f / FMath::Max(1.f, E.GetMax())));
			Body->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
			AntArt::MakeSolid(Body);
		}
		Need = 6.f; Seconds = 2.f;
		UseLocal = FVector(0.f, 0.f, 40.f);
		UsePad = 100.f;
		break;
	}
	case AntProp::ToasterSpring:
	{
		// no body of its own (the toaster's lever is the toaster's); the slice of toast and the pepper jar on the spice shelf
		UStaticMeshComponent* T = KPart(this, R, TEXT("SM_K_Toast"), GetActorTransform().InverseTransformPosition(Spot::ToastFrom), FRotator::ZeroRotator, false);
		UStaticMeshComponent* J = KPart(this, R, TEXT("SM_K_SpiceJar"), GetActorTransform().InverseTransformPosition(FVector(Aux)), FRotator::ZeroRotator, true);
		Parts.Add(T);
		Parts.Add(J);
		if (T) { T->SetVisibility(false); }
		Need = 21.f; Seconds = 4.f;
		UseLocal = FVector::ZeroVector;
		UsePad = 90.f;
		break;
	}
	case AntProp::BookRow:
	{
		static const TCHAR* Books[] = { TEXT("SM_L_Book_A"), TEXT("SM_L_Book_C"), TEXT("SM_L_Book_B"), TEXT("SM_L_Book_D"), TEXT("SM_L_Book_E"), TEXT("SM_L_Book_F") };
		float Y = 0.f;
		for (int32 i = 0; i < NumBooks; ++i)
		{
			UStaticMesh* M = KitchenArt::Mesh(Books[i]);
			if (!M) { Parts.Add(nullptr); continue; }
			// stand it up with its thinnest side along the row (-Y), its tallest upright
			const FVector E = M->GetBoundingBox().GetSize() * KitchenArt::ImportScale;
			FRotator Rot = FRotator::ZeroRotator;
			float Thick = E.Y;
			if (E.X < E.Y && E.X <= E.Z) { Rot = FRotator(0.f, 90.f, 0.f); Thick = E.X; }
			else if (E.Z < E.Y && E.Z <= E.X) { Rot = FRotator(0.f, 0.f, 90.f); Thick = E.Z; }
			Y -= Thick * 0.5f + 8.f;
			UStaticMeshComponent* C = NewPart(this, R, M, FTransform(Rot, FVector(0.f, Y, 0.f), FVector(KitchenArt::ImportScale)), false);
			if (C)
			{
				KitchenArt::ApplyLook(C, Books[i], 0);
				// sit it on the shelf: its lowest point at z 0
				const FBox B = C->CalcBounds(C->GetComponentTransform()).GetBox();
				C->AddWorldOffset(FVector(0.f, 0.f, GetActorLocation().Z - B.Min.Z));
			}
			Parts.Add(C);
			Y -= Thick * 0.5f + 34.f;
		}
		UStaticMeshComponent* V = KPart(this, R, TEXT("SM_L_Vase"), FVector(0.f, Y - 200.f, 0.f), FRotator::ZeroRotator, false);
		Parts.Add(V);
		UStaticMeshComponent* Sh = KPart(this, R, TEXT("SM_V3_GlassShards"), FVector::ZeroVector, FRotator::ZeroRotator, false);
		if (Sh) { Sh->SetAbsolute(true, true, true); Sh->SetVisibility(false); }
		Parts.Add(Sh);
		UseLocal = FVector(0.f, 40.f, 150.f);
		UsePad = 120.f;
		break;
	}
	case AntProp::MicroButton:
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(R);
		Light->SetAbsolute(true, true, true);
		Light->SetWorldLocation((Spot::MicroBox0 + Spot::MicroBox1) * 0.5f);
		Light->SetIntensity(6.f);
		Light->SetAttenuationRadius(700.f);
		Light->SetLightColor(FLinearColor(1.f, 0.85f, 0.55f));
		Light->SetVisibility(false);
		Light->RegisterComponent();
		AddInstanceComponent(Light);
		Need = 6.f; Seconds = 1.5f;
		UseLocal = FVector::ZeroVector;
		UsePad = 70.f;
		break;
	case AntProp::Mentos:
	{
		// the roll of mentos on the coffee table, one sweet, the open bottle on the floor below, the kid's block tower
		Body = MPart(this, R, TEXT("SM_R_MentosRoll"), FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 90.f), 170.f, true, nullptr, TEXT("SM_M4_BiscuitPack"));
		UStaticMeshComponent* Sweet = MPart(this, R, TEXT("SM_R_Mentos"), FVector(0.f, -60.f, 30.f), FRotator::ZeroRotator, 40.f, false, nullptr, TEXT("SM_M4_Zefir"));
		if (Sweet) { Sweet->SetAbsolute(true, true, true); Sweet->SetVisibility(false); }
		Parts.Add(Sweet);
		UStaticMeshComponent* Bot = MPart(this, R, TEXT("SM_M4_PetBottle"), FVector::ZeroVector, FRotator::ZeroRotator, 420.f, true);
		if (Bot) { Bot->SetAbsolute(true, true, true); Bot->SetWorldLocation(Floor(GetWorld(), Spot::Bottle, 300.f, 3000.f, this)); }
		Parts.Add(Bot);
		const FVector T0 = Floor(GetWorld(), Spot::Tower, 300.f, 3000.f, this);
		for (int32 i = 0; i < NumBlocks; ++i)
		{
			// a pyramid 3-2-1 across the bottle's path
			const int32 Row = i < 3 ? 0 : (i < 5 ? 1 : 2);
			const int32 Col = i < 3 ? i : (i < 5 ? i - 3 : 0);
			const float Off = (Col - (Row == 0 ? 1.f : (Row == 1 ? 0.5f : 0.f))) * 140.f;
			UStaticMeshComponent* B = KPart(this, R, TEXT("SM_L_ToyBlock"), FVector::ZeroVector, FRotator(0.f, i * 9.f, 0.f), true, 130.f);
			if (B) { B->SetAbsolute(true, true, true); B->SetWorldLocation(T0 + FVector(0.f, Off, Row * 132.f)); }
			Parts.Add(B);
		}
		UseLocal = FVector(0.f, 0.f, 40.f);
		UsePad = 110.f;
		break;
	}
	case AntProp::Sponge:
	{
		Body = KPart(this, R, TEXT("SM_K_Sponge"), FVector::ZeroVector, FRotator::ZeroRotator, true);
		UStaticMeshComponent* Pl = KPart(this, R, TEXT("SM_K_Plate"), FVector::ZeroVector, FRotator(0.f, 0.f, 80.f), false);
		if (Pl) { Pl->SetAbsolute(true, true, true); Pl->SetWorldLocationAndRotation(Spot::Plate, FRotator(0.f, 0.f, 80.f)); }
		Parts.Add(Pl);
		UStaticMeshComponent* Sh = KPart(this, R, TEXT("SM_V3_GlassShards"), FVector::ZeroVector, FRotator::ZeroRotator, false);
		if (Sh) { Sh->SetAbsolute(true, true, true); Sh->SetVisibility(false); }
		Parts.Add(Sh);
		Need = 6.f; Seconds = 3.f;
		UseLocal = FVector(0.f, 0.f, 50.f);
		UsePad = 90.f;
		break;
	}
	case AntProp::Balloon:
		Body = KPart(this, R, TEXT("SM_V3_Balloon"), FVector::ZeroVector, FRotator::ZeroRotator, false, 460.f);
		if (Body)
		{
			const FBox B = Body->CalcBounds(Body->GetComponentTransform()).GetBox();
			Body->AddWorldOffset(FVector(0.f, 0.f, GetActorLocation().Z - B.Min.Z));
			KitchenArt::SetSolid(Body, KitchenArt::ESolid::Moving);
		}
		Need = 21.f; Seconds = 5.f;
		UseLocal = FVector(0.f, 0.f, 60.f);
		UsePad = 230.f;
		break;
	case AntProp::PowerStrip:
	{
		Body = MPart(this, R, TEXT("SM_R_PowerStrip"), FVector::ZeroVector, FRotator::ZeroRotator, 520.f, true, nullptr, TEXT("SM_M4_Socket"));
		// two cables up to the worktop (the kettle, the microwave), one to the wall socket
		const FVector A = GetActorLocation() + FVector(0.f, 120.f, 18.f);
		Parts.Add(Thread(this, A, Spot::StripCableTop, 16.f, TEXT("Rubber")));
		Parts.Add(Thread(this, A + FVector(60.f, 0.f, 0.f), Spot::StripCableTop + FVector(260.f, 40.f, 0.f), 16.f, TEXT("Rubber")));
		Parts.Add(Thread(this, GetActorLocation() + FVector(-230.f, 0.f, 14.f), GetActorLocation() + FVector(-900.f, 520.f, 14.f), 18.f, TEXT("Rubber")));
		Need = 6.f; Seconds = 6.f;
		Aux = GetActorLocation() + FVector(-470.f, 0.f, 14.f);   // where the floor cable can be chewed
		UsePad = 60.f;
		break;
	}
	case AntProp::Breaker:
		Body = MPart(this, R, TEXT("SM_R_BreakerBox"), FVector::ZeroVector, FRotator::ZeroRotator, 620.f, true, nullptr, TEXT("SM_M4_Socket"));
		if (UStaticMeshComponent* L = MPart(this, R, TEXT("SM_R_BreakerLever"), FVector(-70.f, 0.f, 60.f), FRotator::ZeroRotator, 150.f, false, TEXT("Red"), TEXT("SM_M4_Button")))
		{
			Parts.Add(L);
		}
		Need = 21.f; Seconds = 3.f;
		UseLocal = FVector(-80.f, 0.f, 60.f);
		UsePad = 90.f;
		break;
	default: break;
	}
	if (Body) { BodyRel = Body->GetRelativeTransform(); BodyScale = Body->GetRelativeScale3D(); }
	// the people walk through them (they are small; the ants climb them): like the level's "ants only" threads
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* C : Prims)
	{
		if (!C || C->GetCollisionEnabled() == ECollisionEnabled::NoCollision) { continue; }
		C->ComponentTags.AddUnique(TEXT("AntsOnly"));
		for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It) { if (It->GetCapsuleComponent()) { It->GetCapsuleComponent()->IgnoreComponentWhenMoving(C, true); } }
	}
	PartRel.Reset();
	for (UStaticMeshComponent* P : Parts) { PartRel.Add(P ? P->GetRelativeTransform() : FTransform::Identity); }
	bHeldWork = true;
	UE_LOG(LogTemp, Display, TEXT("ANTREACT prop %s at %s (%s), body %s, %d parts"), AntProp::Name(Kind), *GetActorLocation().ToCompactString(), bServer ? TEXT("server") : TEXT("client"), Body ? *GetNameSafe(Body->GetStaticMesh()) : TEXT("-"), Parts.Num());
}

void AAntReactProp::SetState(uint8 S)
{
	State = S;
	StateAt = NetNow();
	Step = 0;
	OnRep_State();
}

void AAntReactProp::OnRep_State() {}

void AAntReactProp::Fly(const FVector& To, float Secs, float InArc, const FRotator& Spin)
{
	FlyFrom = GetActorLocation();
	FlyTo = To;
	FlyTime = FMath::Max(0.1f, Secs);
	FlyArc = InArc;
	FlySpin = Spin;
	RestRot = GetActorRotation();
	bLandedServer = false;
	if (Body) { Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	SetState(2);
}

FTransform AAntReactProp::PoseNow() const
{
	if (State == 2 || State == 3)
	{
		if (FlyTime > 0.f && !FVector(FlyFrom).IsZero() && (Kind == AntProp::IceTray || Kind == AntProp::PeaBowl || Kind == AntProp::Chocolate))
		{
			const float T = FMath::Clamp((NetNow() - StateAt) / FlyTime, 0.f, 1.f);
			const float K = State == 3 ? 1.f : T;
			return FTransform(RestRot + FlySpin * K, Drop(FlyFrom, FlyTo, K, FlyArc), GetActorScale3D());
		}
	}
	return GetActorTransform();
}

void AAntReactProp::ResetProp()
{
	bDone = false;
	Progress = 0.f;
	PullMissing = 0;
	Level = 0.f;
	Step = 0;
	Timer = -1.f;
	bSuper = false;
	bLandedServer = false;
	FlyFrom = FVector::ZeroVector;
	SetActorTransform(Home);
	if (Body) { Body->SetRelativeTransform(BodyRel); KitchenArt::SetSolid(Body, KitchenArt::ESolid::Moving); }
	SetState(0);
}

void AAntReactProp::OnDone()
{
	UWorld* W = GetWorld();
	AAntReactNet* Net = AAntReactNet::Get(W);
	By = DoneBy;
	AAntCharacter* Who = DoneBy.Get();
	const FString Name = Who ? Who->GetDisplayName() : FString(TEXT("кто-то"));
	switch (Kind)
	{
	case AntProp::Pepper:
	{
		// it tips over away from the ants: the cap comes off, a puff of pepper
		FVector Dir = FVector(1.f, 0.3f, 0.f);
		if (Who) { Dir = (GetActorLocation() - Who->GetActorLocation()).GetSafeNormal2D(); }
		if (Dir.IsNearlyZero()) { Dir = FVector(1.f, 0.f, 0.f); }
		Aux = Dir;
		SetState(2);
		if (Net) { Net->ChainLog(TEXT("C7"), FString::Printf(TEXT("%s tips the pepper shaker over"), *Name)); }
		break;
	}
	case AntProp::Flour:
		SetState(2);
		AntAudio::Everyone(this, EAntSfx::RipOff, GetActorLocation(), 1.f);
		if (Net) { Net->ChainLog(TEXT("C8"), FString::Printf(TEXT("%s chews the flour sack open"), *Name)); }
		break;
	case AntProp::IceTray:
	{
		const FVector To = Floor(W, Spot::IceFloor, 300.f, 3000.f, this);
		Fly(To + FVector(0.f, 0.f, 10.f), 0.62f, 160.f, FRotator(0.f, 40.f, 180.f));
		if (Net) { Net->ChainLog(TEXT("C9"), FString::Printf(TEXT("%s pushes the ice tray off the worktop"), *Name)); }
		break;
	}
	case AntProp::PeaBowl:
	{
		const FVector To = Floor(W, Spot::PeasFloor, 300.f, 3000.f, this);
		Fly(To + FVector(0.f, 0.f, 10.f), 0.6f, 180.f, FRotator(0.f, 70.f, 170.f));
		if (Net) { Net->ChainLog(TEXT("C11"), FString::Printf(TEXT("%s pushes the bowl of peas off the table"), *Name)); }
		break;
	}
	case AntProp::Slingshot:
	{
		// let go: everybody standing in the pouch flies onto the table
		const FVector Pouch = GetActorTransform().TransformPosition(FVector(-230.f, 0.f, 60.f));
		int32 N = 0;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->Creature != 0 || FVector::Dist(A->GetActorLocation(), Pouch) > 300.f) { continue; }
			const FVector P = A->GetActorLocation();
			const FVector T = FVector(Aux) + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 0.f);
			const float G = FMath::Abs(A->GetCharacterMovement()->GetGravityZ());
			const float Tf = 0.95f;
			const FVector D = T - P;
			const FVector V(D.X / Tf, D.Y / Tf, ((float)D.Z + 0.5f * G * Tf * Tf) / Tf);
			A->ApplyLaunch(V);
			A->Toast(TEXT("Рогатка! Летим на стол!"));
			++N;
		}
		AntAudio::Everyone(this, EAntSfx::SlingTwang, Pouch, 1.f);
		SetState(2);
		Timer = NetNow() + 1.6f;
		if (Net) { Net->ChainLog(TEXT("C10"), FString::Printf(TEXT("the slingshot fires: %d ants fly onto the table (%s let go)"), N, *Name)); }
		UE_LOG(LogTemp, Display, TEXT("ANTREACT slingshot launches %d ants to %s"), N, *FVector(Aux).ToCompactString());
		break;
	}
	case AntProp::Chocolate:
	{
		const FVector To = Floor(W, Spot::ChocRad + FVector(0.f, 0.f, 300.f), 50.f, 1000.f, this);
		Fly(To, 0.45f, 60.f, FRotator(0.f, 30.f, 0.f));
		if (Net) { Net->ChainLog(TEXT("C12"), FString::Printf(TEXT("%s pushes the chocolate bar off the sill"), *Name)); }
		break;
	}
	case AntProp::ToasterSpring:
	{
		bSuper = true;
		bool bArmed = false;
		for (TActorIterator<AKitchenToaster> It(W); It; ++It)
		{
			if (Who && It->CanInteract(Who)) { It->Interact(Who); bArmed = true; }
			else if (!It->bArmed) { It->bArmed = true; bArmed = true; It->ForceArm(); }
			break;
		}
		AntAudio::Everyone(this, EAntSfx::ScrewTurn, GetActorLocation(), 1.f);
		if (Net) { Net->ChainLog(TEXT("C13"), FString::Printf(TEXT("%s and the team wind the toaster spring right down (armed %d)"), *Name, bArmed ? 1 : 0)); }
		SetState(1);
		break;
	}
	case AntProp::BookRow:
		SetState(2);
		AntAudio::Everyone(this, EAntSfx::Wobble, GetActorLocation(), 1.f);
		if (Net) { Net->ChainLog(TEXT("C14"), FString::Printf(TEXT("%s pushes the first book: domino"), *Name)); }
		break;
	case AntProp::MicroButton:
		if (Net)
		{
			Net->MicroUntil = Net->NetNow() + 15.f;
			Net->MicroBy = Who;
			Net->MicroBeepAt = -1.f;
			Net->ChainLog(TEXT("C15"), FString::Printf(TEXT("%s starts the microwave (15 s, empty)"), *Name));
		}
		AntAudio::Everyone(this, EAntSfx::MicroBeep, GetActorLocation(), 0.6f);
		SetState(1);
		break;
	case AntProp::Mentos:
		SetState(2);
		if (Net) { Net->ChainLog(TEXT("C18"), FString::Printf(TEXT("%s pushes a mentos off the coffee table"), *Name)); }
		break;
	case AntProp::Sponge:
		// to the very edge: from now on half the water tips it
		Aux = FVector(1.f, 0.f, 0.f);
		AddActorWorldOffset(FVector(70.f, -20.f, 0.f));
		if (Net) { Net->ChainLog(TEXT("C19"), FString::Printf(TEXT("%s shoves the sponge to the edge of the dish rack"), *Name)); }
		break;
	case AntProp::Balloon:
	{
		// charged: it goes up and sticks to the ceiling, and the ants holding on to it go with it
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntReactCeiling), false, this);
		const FVector From = GetActorLocation() + FVector(0.f, 0.f, 600.f);
		FVector Ceil = GetActorLocation() + FVector(0.f, 0.f, 5200.f);
		if (W->LineTraceSingleByChannel(H, From, From + FVector(0.f, 0.f, 9000.f), ECC_Visibility, Q)) { Ceil = H.ImpactPoint; }
		Aux = Ceil - FVector(0.f, 0.f, 470.f);
		int32 N = 0;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->Creature != 0 || FVector::Dist(A->GetActorLocation(), UsePoint()) > 420.f) { continue; }
			const float G = FMath::Abs(A->GetCharacterMovement()->GetGravityZ());
			const float Up = (float)(Ceil.Z - A->GetActorLocation().Z) - 40.f;
			A->ApplyLaunch(FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::Sqrt(2.f * G * FMath::Max(100.f, Up)) * 1.04f));
			A->Toast(TEXT("Статика! Шарик тянет тебя к потолку"));
			++N;
		}
		AntAudio::Everyone(this, EAntSfx::StaticCrackle, GetActorLocation(), 1.f);
		if (Body) { Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		SetState(2);
		if (Net) { Net->ChainLog(TEXT("C20"), FString::Printf(TEXT("the balloon is charged (%s rubbed it): it rises to the ceiling at z %.0f with %d ants"), *Name, Ceil.Z, N)); }
		if (LevelV3::IsHome(LevelV3::Kid)) { Say(W, NpcRoutine::Kid, TEXT("Ой! Шарик прилип к потолку!")); AntAudio::Everyone(this, EAntSfx::KidGiggle, GetActorLocation(), 1.f); }
		break;
	}
	case AntProp::PowerStrip:
		if (Net)
		{
			if (Who) { Net->Zap(Who, 30.f, TEXT("перегрыз провод под током")); }
			Net->ChainLog(TEXT("C22"), FString::Printf(TEXT("%s chews through the live cable"), *Name));
			Net->Short(FVector(Aux), TEXT("перегрызли провод"), Who);
		}
		SetState(3);
		break;
	case AntProp::Breaker:
		if (Net)
		{
			Net->ChainLog(TEXT("power"), FString::Printf(TEXT("%s and the team pull the main switch down"), *Name));
			Net->SetPower(false, TEXT("муравьи опустили рубильник"), GetActorLocation());
		}
		SetState(3);
		break;
	default: break;
	}
}

void AAntReactProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) { ServerTick(DeltaSeconds); }
	if (GetNetMode() != NM_DedicatedServer) { LookTick(DeltaSeconds); }
	if (State == 2 && (Kind == AntProp::IceTray || Kind == AntProp::PeaBowl || Kind == AntProp::Chocolate) && !FVector(FlyFrom).IsZero())
	{
		const FTransform P = PoseNow();
		SetActorLocationAndRotation(P.GetLocation(), P.GetRotation());
	}
}

void AAntReactProp::ServerTick(float Dt)
{
	UWorld* W = GetWorld();
	AAntReactNet* Net = AAntReactNet::Get(W);
	const float Now = NetNow();
	const float T = Now - StateAt;
	// flights land
	if (State == 2 && !bLandedServer && !FVector(FlyFrom).IsZero() && T >= FlyTime && (Kind == AntProp::IceTray || Kind == AntProp::PeaBowl || Kind == AntProp::Chocolate))
	{
		bLandedServer = true;
		SetActorLocationAndRotation(FlyTo, (RestRot + FlySpin).Quaternion());
		const uint8 Keep = Kind == AntProp::Chocolate ? 5 : 3;
		SetState(Keep);
		OnLanded();
		return;
	}
	switch (Kind)
	{
	case AntProp::Pepper:
		if (State == 2 && T >= 0.35f)
		{
			const FVector At = GetActorLocation() + FVector(Aux) * 160.f + FVector(0.f, 0.f, 90.f);
			AntAudio::Everyone(this, EAntSfx::CrashPlastic, GetActorLocation(), 0.5f);
			Noise(W, At, 0.55f, AntNoise::Crash, this, By.Get());
			AntMess::StainAt(this, MessKind::Pepper, Floor(W, At, 100.f, 400.f, this), FVector::UpVector, 260.f, 17);
			if (Net)
			{
				Net->AddCloud(AntCloudKind::Pepper, At + FVector(0.f, 0.f, 120.f), 1300.f, 14.f, FVector(Aux) * 40.f + FVector(0.f, 0.f, -10.f));
				Net->MulticastPuff(AntCloudKind::Pepper, At, 200.f);
				Net->ChainLog(TEXT("C7"), TEXT("the shaker lands on its side: a cloud of pepper over the table"));
			}
			SetState(3);
		}
		break;
	case AntProp::Flour:
		if (State == 2 && T >= 0.5f)
		{
			// the avalanche: over the front edge onto the floor
			const FVector Edge(GetActorLocation().X, 2990.f, GetActorLocation().Z + 40.f);
			const FVector F = Floor(W, Spot::FlourFloor, 300.f, 3000.f, this);
			AntAudio::Everyone(this, EAntSfx::FlourWhoosh, F + FVector(0.f, 0.f, 400.f), 1.2f);
			Noise(W, F, 0.6f, AntNoise::Crash, this, By.Get());
			for (int32 i = 0; i < 4; ++i) { AntMess::StainAt(this, MessKind::Flour, F + FVector(FMath::FRandRange(-260.f, 260.f), FMath::FRandRange(-200.f, 200.f), 0.f), FVector::UpVector, 520.f - i * 70.f, 31 + i); }
			AntMess::StainAt(this, MessKind::Flour, Edge - FVector(0.f, -60.f, 40.f), FVector::UpVector, 300.f, 7);
			if (Net)
			{
				Net->AddCloud(AntCloudKind::Flour, F + FVector(0.f, 0.f, 650.f), 1250.f, 16.f, FVector(0.f, -30.f, -15.f), Edge, 4.f);
				Net->MulticastPuff(AntCloudKind::Flour, F + FVector(0.f, 0.f, 200.f), 600.f);
				Net->ChainLog(TEXT("C8"), TEXT("a flour avalanche over the edge: a white cloud over the kitchen floor"));
			}
			if (NpcRoutine::Enabled()) { NpcRoutine::AddMess(W, NpcRoutine::Spill, F, this, TEXT("мука по всему полу")); }
			SetState(3);
		}
		break;
	case AntProp::Slingshot:
		if (State == 2 && Timer > 0.f && Now >= Timer) { ResetProp(); }
		break;
	case AntProp::Chocolate:
		if (State == 5)
		{
			const float Heat = AntReact::HeatAt(W, GetActorLocation() + FVector(0.f, 0.f, 10.f));
			if (Heat > 34.f) { Level = FMath::Min(1.f, Level + Dt / 8.f); }
			if (FMath::FRand() < Dt * 2.f && Net) { Net->MulticastPuff(AntCloudKind::Steam, GetActorLocation() + FVector(0.f, 0.f, 40.f), 50.f); }
			if (Level >= 1.f)
			{
				// the river: down the radiator, a pool on the floor in front of the window
				const FVector F = Floor(W, Spot::ChocFloor, 300.f, 3000.f, this);
				AntMess::StainAt(this, MessKind::Chocolate, F, FVector::UpVector, 820.f, 5);
				AntMess::StainAt(this, MessKind::Chocolate, F + FVector(0.f, -260.f, 0.f), FVector::UpVector, 520.f, 9);
				AntMess::StainAt(this, MessKind::Chocolate, GetActorLocation() + FVector(0.f, 110.f, -200.f), FVector(0.f, 1.f, 0.f), 300.f, 3);
				if (UAntFluidWorld* Fl = UAntFluidWorld::Active(this)) { Fl->Spill(AntFluid::Honey, F + FVector(0.f, 0.f, 10.f), 0.8f * Litre, 120.f); }
				LevelV3::AddZone(0, F, 520.f);
				AntAudio::Everyone(this, EAntSfx::SplatJam, F, 1.f);
				if (NpcRoutine::Enabled()) { NpcRoutine::AddMess(W, NpcRoutine::Spill, F, this, TEXT("шоколад на батарее")); }
				if (Net) { Net->ChainLog(TEXT("C12"), TEXT("the chocolate has melted: a sticky chocolate river at the window")); }
				SetState(3);
			}
		}
		else if (State == 3 && Step == 0)
		{
			// the Dad walks into it: one line
			AHumanCharacter* H = Dad(W);
			if (H && FVector::Dist2D(H->GetActorLocation(), Spot::ChocFloor) < 650.f)
			{
				Step = 1;
				Say(W, NpcRoutine::Dad, TEXT("Фу! Шоколад?! Кто положил шоколадку на батарею?!"));
				if (Net) { Net->ChainLog(TEXT("C12"), FString::Printf(TEXT("the Dad steps into the chocolate (slowed x%.1f)"), H->StickyK)); }
			}
		}
		break;
	case AntProp::ToasterSpring:
		if (State == 2)
		{
			if (Step == 0 && T >= 1.1f)
			{
				Step = 1;
				AntAudio::Everyone(this, EAntSfx::CrashGlass, FVector(Aux), 0.5f);
				if (Net) { Net->ChainLog(TEXT("C13"), TEXT("the toast hits the pepper jar on the spice shelf")); }
			}
			if (Step == 1 && T >= 1.6f)
			{
				Step = 2;
				const FVector L = Floor(W, Spot::SpiceLand, 200.f, 600.f, this);
				AntAudio::Everyone(this, EAntSfx::CrashPlastic, L, 0.7f);
				Noise(W, L, 0.5f, AntNoise::Crash, this, By.Get());
				AntMess::StainAt(this, MessKind::Pepper, L, FVector::UpVector, 300.f, 23);
				if (Net)
				{
					Net->AddCloud(AntCloudKind::Pepper, Spot::PepperCloudK, 1250.f, 18.f, FVector(60.f, -90.f, -10.f), FVector::ZeroVector, 0.f, TEXT("C13"));
					Net->MulticastPuff(AntCloudKind::Pepper, L + FVector(0.f, 0.f, 60.f), 260.f);
					Net->ChainLog(TEXT("C13"), TEXT("the jar lands on the worktop: pepper drifts towards the stove"));
				}
				SetState(3);
			}
		}
		break;
	case AntProp::BookRow:
		if (State == 2)
		{
			const float VaseHit = (NumBooks - 1) * 0.22f + 0.3f, VaseLand = VaseHit + 0.75f;
			if (Step == 0 && T >= VaseHit) { Step = 1; AntAudio::Everyone(this, EAntSfx::SoftThud, GetActorLocation(), 0.9f); }
			if (Step == 1 && T >= VaseLand)
			{
				Step = 2;
				const FVector F = Floor(W, Spot::VaseFloor, 300.f, 6000.f, this);
				AntAudio::Everyone(this, EAntSfx::CrashGlass, F, 1.3f);
				Noise(W, F, 1.f, AntNoise::Crash, this, By.Get());
				LevelV3::AddZone(2, F, 260.f);
				AKitchenSlick::Spill(W, F + FVector(-150.f, 0.f, 0.f), SlickKind::Water, By.Get(), 700.f, 70.f);
				AntMess::StainAt(this, MessKind::Water, F, FVector::UpVector, 900.f, 3);
				// the sweets that were in the vase
				TArray<AKitchenLoot*> Sweets;
				for (int32 i = 0; i < 5; ++i)
				{
					FAntFoodDef D;
					D.Name = TEXT("Карамелька");
					D.Value = 6; D.Weight = 1; D.Color = FLinearColor(0.9f, 0.4f, 0.1f); D.Shape = EAntShape::Sphere; D.Scale = FVector(0.16f);
					const FTransform TM(F + FVector(FMath::FRandRange(-350.f, 350.f), FMath::FRandRange(-350.f, 350.f), 40.f));
					if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
					{
						L->InitFood(D);
						L->FinishSpawning(TM);
						Sweets.Add(L);
					}
				}
				if (NpcRoutine::Enabled()) { NpcRoutine::AddMess(W, NpcRoutine::Shards, F, this, TEXT("разбитая ваза и лужа")); }
				if (ACritter* Dog = Pet(W, 0)) { if (Sweets.Num() > 0) { Dog->BaitWith(Sweets[0]); } }
				if (Net) { Net->ChainLog(TEXT("C14"), FString::Printf(TEXT("the vase hits the floor: glass, a puddle (slippery 70 s), %d sweets"), Sweets.Num())); }
				Timer = Now + 60.f;
				SetState(3);
			}
		}
		else if (State == 3 && Step == 0 && Timer > Now)
		{
			// Бадрик comes for the sweets and goes skating on the puddle
			ACritter* Dog = Pet(W, 0);
			const FVector F = Floor(W, Spot::VaseFloor, 300.f, 6000.f, this);
			if (Dog && FVector::Dist2D(Dog->GetActorLocation(), F) < 750.f)
			{
				Step = 1;
				AntAudio::Everyone(this, EAntSfx::DogBark, Dog->GetActorLocation(), 1.f);
				Dog->Zoomies(3.f);
				Say(W, NpcRoutine::Dog, TEXT("Ав?! (Бадрик едет по луже на всех четырёх лапах)"));
				if (Net) { Net->ChainLog(TEXT("C14"), TEXT("Бадрик runs into the puddle and slips")); }
			}
		}
		break;
	case AntProp::MicroButton:
		if (State == 1 && Net && Net->MicroUntil < 0.f) { SetState(3); }
		break;
	case AntProp::Mentos:
		if (State == 2)
		{
			if (Step == 0 && T >= 0.5f)
			{
				Step = 1;
				const FVector B = Floor(W, Spot::Bottle, 300.f, 3000.f, this);
				AntAudio::Everyone(this, EAntSfx::ColaFizz, B, 1.5f);
				Noise(W, B, 0.6f, AntNoise::Crash, this, By.Get());
				AntMess::StainAt(this, MessKind::Cola, B, FVector::UpVector, 900.f, 11);
				LevelV3::AddZone(0, B, 420.f);
				if (UAntFluidWorld* Fl = UAntFluidWorld::Active(this)) { Fl->Geyser(B + FVector(0.f, 0.f, 380.f), 3.f, AntFluid::Cola); }
				if (Net) { Net->ChainLog(TEXT("C18"), TEXT("the mentos drops into the cola: GEYSER")); }
			}
			if (Step == 1 && T >= 1.4f) { Step = 2; AntAudio::Everyone(this, EAntSfx::CrashPlastic, Spot::Bottle, 0.8f); }
			if (Step == 2 && T >= 2.5f)
			{
				Step = 3;
				const FVector Tw = Floor(W, Spot::Tower, 300.f, 3000.f, this);
				for (int32 k = 0; k < 3; ++k) { AntAudio::Everyone(this, EAntSfx::CrashPlastic, Tw + FVector(0.f, k * 120.f, 0.f), 1.f); }
				Noise(W, Tw, 0.85f, AntNoise::Crash, this, By.Get());
				if (LevelV3::IsHome(LevelV3::Kid)) { Say(W, NpcRoutine::Kid, TEXT("СТРАЙК!!! Ещё! Ещё!")); AntAudio::Everyone(this, EAntSfx::KidGiggle, Tw, 1.f); }
				else { Say(W, NpcRoutine::Dad, TEXT("Это что там грохочет в комнате?!")); }
				if (Net) { Net->ChainLog(TEXT("C18"), TEXT("the bottle rockets into the block tower: STRIKE")); }
				SetState(3);
			}
		}
		break;
	case AntProp::Sponge:
		if (State == 0)
		{
			// it soaks up whatever water is in the sink (or a pour on it)
			// it soaks while the tap runs into a sink with water in it, or when water reaches it on the worktop
			float Sink = 0.f, Here = 0.f;
			if (const UAntFluidWorld* Fl = UAntFluidWorld::Active(this)) { Sink = Fl->DepthAt(Spot::SinkCenter); Here = Fl->DepthAt(GetActorLocation() + FVector(0.f, 0.f, 10.f)); }
			bool bTap = false;
			for (TActorIterator<AKitchenFaucet> It(W); It; ++It) { if (It->IsOn()) { bTap = true; } }
			if ((bTap && Sink > 2.f) || Here > 1.f || (bDone && Sink > 2.f)) { Level = FMath::Min(1.f, Level + Dt / 14.f); }
			const float Tip = bDone ? 0.5f : 1.f;
			if (Level >= Tip)
			{
				SetState(2);
				AntAudio::Everyone(this, EAntSfx::SoftThud, GetActorLocation(), 0.9f);
				if (Net) { Net->ChainLog(TEXT("C19"), FString::Printf(TEXT("the sponge is heavy with water (%.0f%%): it tips over onto the dish rack"), Level * 100.f)); }
			}
		}
		else if (State == 2 && T >= 1.f)
		{
			const FVector F = Floor(W, Spot::PlateFloor, 300.f, 3000.f, this);
			AntAudio::Everyone(this, EAntSfx::CrashCeramic, F, 1.3f);
			Noise(W, F, 0.9f, AntNoise::Crash, this, By.Get());
			LevelV3::AddZone(2, F, 240.f);
			AntMess::StainAt(this, MessKind::Scratch, F, FVector::UpVector, 360.f, 8);
			if (NpcRoutine::Enabled()) { NpcRoutine::AddMess(W, NpcRoutine::Shards, F, this, TEXT("разбитая тарелка")); }
			if (Net) { Net->ChainLog(TEXT("C19"), TEXT("the plate falls off the rack and breaks on the floor")); }
			SetState(3);
		}
		break;
	case AntProp::Balloon:
		if (State == 2 && T >= 3.f) { SetState(5); }
		else if (State == 5)
		{
			// static: crumbs and fluff below jump up to it now and then
			if (FMath::FRand() < Dt * 0.8f)
			{
				for (TActorIterator<AKitchenLoot> It(W); It; ++It)
				{
					if (!It->bIsGear && !It->IsCarried() && It->Weight <= 1 && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 900.f && It->GetActorLocation().Z < 300.f)
					{
						It->Kick(FVector(0.f, 0.f, 2600.f) + (FVector(Aux) - It->GetActorLocation()).GetSafeNormal2D() * 300.f, It->GetActorLocation());
						AntAudio::Everyone(this, EAntSfx::StaticCrackle, It->GetActorLocation(), 0.5f);
						break;
					}
				}
			}
			if (T >= 40.f) { SetState(6); }
		}
		else if (State == 6 && T >= 4.f) { ResetProp(); }
		break;
	default: break;
	}
}

void AAntReactProp::OnLanded()
{
	UWorld* W = GetWorld();
	AAntReactNet* Net = AAntReactNet::Get(W);
	const FVector At = GetActorLocation();
	switch (Kind)
	{
	case AntProp::IceTray:
		AntAudio::Everyone(this, EAntSfx::IceClink, At, 1.f);
		AntAudio::Everyone(this, EAntSfx::CrashPlastic, At, 0.6f);
		Noise(W, At, 0.45f, AntNoise::Crash, this, By.Get());
		if (Net)
		{
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = i * 0.63f + FMath::FRandRange(-0.2f, 0.2f), D = FMath::FRandRange(80.f, 330.f);
				Net->AddMelt(0, Floor(W, At + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.8f - 60.f, 0.f), 200.f, 600.f, this) + FVector(0.f, 0.f, 2.f), 40.f);
			}
			Net->ChainLog(TEXT("C9"), TEXT("the tray hits the floor: 10 ice cubes scatter and start to melt (~40 s, 0.6 l)"));
		}
		break;
	case AntProp::PeaBowl:
	{
		AntAudio::Everyone(this, EAntSfx::CrashPlastic, At, 1.f);
		AntAudio::Everyone(this, EAntSfx::Crumbs, At, 1.2f);
		Noise(W, At, 0.6f, AntNoise::Crash, this, By.Get());
		int32 N = 0;
		for (int32 i = 0; i < 26; ++i)
		{
			FAntFoodDef D;
			D.Name = TEXT("горошина");
			D.Value = 1; D.Weight = 1; D.Color = FLinearColor(0.35f, 0.6f, 0.15f); D.Shape = EAntShape::Sphere; D.Scale = FVector(0.13f);
			const FVector P = At + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 30.f + i);
			const FTransform TM(P);
			if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->InitFood(D);
				L->FinishSpawning(TM);
				const float A = FMath::FRandRange(0.f, 2.f * PI);
				L->Kick(FVector(FMath::Cos(A), FMath::Sin(A), 0.2f) * FMath::FRandRange(250.f, 900.f), P);
				if (Net) { AAntReactNet::FPeaWatch& R = Net->Rollers.AddDefaulted_GetRef(); R.Loot = L; R.Born = Net->NetNow(); }
				++N;
			}
		}
		if (Net) { Net->ChainLog(TEXT("C11"), FString::Printf(TEXT("the bowl lands: %d peas roll over the floor"), N)); }
		break;
	}
	case AntProp::Chocolate:
		AntAudio::Everyone(this, EAntSfx::SoftThud, At, 0.8f);
		Level = 0.f;
		if (Net) { Net->ChainLog(TEXT("C12"), FString::Printf(TEXT("the bar lands on the radiator (%.0f C): it starts to melt"), AntReact::HeatAt(W, At + FVector(0.f, 0.f, 10.f)))); }
		break;
	default: break;
	}
}

void AAntReactProp::LookTick(float Dt)
{
	const float Now = NetNow();
	const float T = Now - StateAt;
	AnimT += Dt;
	auto PartAt = [this](int32 i) { return Parts.IsValidIndex(i) ? Parts[i] : nullptr; };
	switch (Kind)
	{
	case AntProp::Pepper:
		if (Body)
		{
			const float K = State >= 2 ? Ease(T / 0.35f) : 0.f;
			const FVector Ax = FVector::CrossProduct(FVector::UpVector, FVector(Aux).IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : FVector(Aux)).GetSafeNormal();
			const FQuat Q(GetActorTransform().InverseTransformVectorNoScale(Ax), FMath::DegreesToRadians(88.f * K));
			Body->SetRelativeRotation(Q * BodyRel.GetRotation());
			Body->SetRelativeLocation(BodyRel.GetLocation() + FVector(0.f, 0.f, 35.f * K));
			if (State >= 2 && T < 0.6f && FMath::FRand() < Dt * 25.f) { AntFx::Droplets(this, GetActorLocation() + FVector(Aux) * 120.f + FVector(0.f, 0.f, 60.f), 1, 4.f); }
		}
		break;
	case AntProp::Flour:
		if (Body)
		{
			const float K = State >= 2 ? Ease(T / 1.2f) : (Progress > 0.f ? 0.02f * FMath::Sin(AnimT * 40.f) : 0.f);
			Body->SetRelativeScale3D(BodyScale * FVector(1.f + 0.18f * K, 1.f + 0.1f * K, 1.f - 0.35f * K));
			Body->SetRelativeRotation(BodyRel.GetRotation() * FQuat(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians(-14.f * K)));
		}
		break;
	case AntProp::IceTray:
	case AntProp::PeaBowl:
		for (UStaticMeshComponent* P : Parts) { if (P && P->IsVisible() == (State >= 2)) { P->SetVisibility(State < 2); } }
		if (State == 3 && !FVector(FlyFrom).IsZero()) { const FTransform P = PoseNow(); SetActorLocationAndRotation(P.GetLocation(), P.GetRotation()); }
		break;
	case AntProp::Slingshot:
	{
		// the pouch is pulled back with the work (or snaps forward right after the shot)
		const float Pull = State == 2 ? FMath::Max(0.f, 1.f - T / 0.12f) : Progress;
		const FTransform X = GetActorTransform();
		const FVector Pouch = X.TransformPosition(FVector(-110.f - 260.f * Pull, 0.f, 150.f - 90.f * Pull));
		if (UStaticMeshComponent* P = PartAt(0)) { P->SetWorldLocation(Pouch); }
		SetThread(PartAt(1), X.TransformPosition(FVector(0.f, -92.f, 280.f)), Pouch + X.TransformVectorNoScale(FVector(0.f, -22.f, 0.f)), 9.f);
		SetThread(PartAt(2), X.TransformPosition(FVector(0.f, 92.f, 280.f)), Pouch + X.TransformVectorNoScale(FVector(0.f, 22.f, 0.f)), 9.f);
		break;
	}
	case AntProp::Chocolate:
		if (Body)
		{
			const float M = State == 5 ? Level : 0.f;
			Body->SetVisibility(State != 3 || FVector(FlyFrom).IsZero());
			Body->SetRelativeScale3D(BodyScale * FVector(1.f + 0.35f * M, 1.f + 0.2f * M, FMath::Max(0.08f, 1.f - 0.85f * M)));
			if (State == 5 && FMath::FRand() < Dt * (2.f + 8.f * M)) { AntFx::Droplets(this, GetActorLocation() + FVector(0.f, 120.f, -10.f), 1, 5.f); }
		}
		break;
	case AntProp::ToasterSpring:
	{
		UStaticMeshComponent* Toast = PartAt(0);
		UStaticMeshComponent* Jar = PartAt(1);
		const FVector JarHome = FVector(Aux);
		const FVector L = Spot::SpiceLand + FVector(0.f, 0.f, 0.f);
		if (State == 2 || State == 3)
		{
			const float TT = State == 3 ? 99.f : T;
			if (Toast)
			{
				Toast->SetVisibility(true);
				Toast->SetAbsolute(true, true, true);
				if (TT < 1.1f) { Toast->SetWorldLocationAndRotation(Arc(Spot::ToastFrom, JarHome + FVector(0.f, -40.f, 40.f), TT / 1.1f, 1300.f), FRotator(TT * 420.f, 0.f, TT * 260.f)); }
				else { Toast->SetWorldLocationAndRotation(Drop(JarHome, L + FVector(-280.f, 60.f, 12.f), FMath::Clamp((TT - 1.1f) / 0.5f, 0.f, 1.f), 120.f), FRotator(90.f, 20.f, 0.f)); }
			}
			if (Jar)
			{
				Jar->SetAbsolute(true, true, true);
				const float J = FMath::Clamp((TT - 1.1f) / 0.5f, 0.f, 1.f);
				Jar->SetWorldLocationAndRotation(Drop(JarHome, L, J, 140.f), FRotator(0.f, 0.f, 90.f * J + 360.f * J));
			}
		}
		else
		{
			if (Toast && Toast->IsVisible()) { Toast->SetVisibility(false); }
			if (Jar && Jar->IsUsingAbsoluteLocation()) { Jar->SetAbsolute(false, false, false); Jar->SetRelativeTransform(PartRel[1]); }
		}
		break;
	}
	case AntProp::BookRow:
	{
		const float TT = State == 3 ? 99.f : (State == 2 ? T : -1.f);
		const FTransform X = GetActorTransform();
		for (int32 i = 0; i < NumBooks; ++i)
		{
			UStaticMeshComponent* B = PartAt(i);
			if (!B || !PartRel.IsValidIndex(i)) { continue; }
			const float K = TT < 0.f ? 0.f : Ease((TT - i * 0.22f) / 0.3f);
			// tips towards -Y about its bottom -Y edge (a domino)
			const FBox Bx = B->GetStaticMesh() ? B->GetStaticMesh()->GetBoundingBox() : FBox(FVector::ZeroVector, FVector::ZeroVector);
			const FTransform Rest = PartRel[i];
			const FQuat Q(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians(-62.f * K));
			FVector Piv = Rest.GetLocation() + FVector(0.f, -0.5f * Rest.GetScale3D().Y * (float)Bx.GetSize().Y, 0.f);
			const FVector Rel = Rest.GetLocation() - Piv;
			B->SetRelativeLocationAndRotation(Piv + Q.RotateVector(Rel), Q * Rest.GetRotation());
		}
		UStaticMeshComponent* V = PartAt(NumBooks);
		UStaticMeshComponent* Sh = PartAt(NumBooks + 1);
		const float VaseHit = (NumBooks - 1) * 0.22f + 0.3f;
		if (V && PartRel.IsValidIndex(NumBooks))
		{
			if (TT < VaseHit) { V->SetVisibility(true); V->SetRelativeTransform(PartRel[NumBooks]); }
			else
			{
				const float J = (TT - VaseHit) / 0.75f;
				const FVector From = X.TransformPosition(PartRel[NumBooks].GetLocation());
				const FVector To = Floor(GetWorld(), Spot::VaseFloor, 300.f, 6000.f, this);
				V->SetVisibility(J < 1.f);
				if (J < 1.f) { V->SetWorldLocationAndRotation(Drop(From, To, J, 250.f), FRotator(J * -200.f, 0.f, J * 90.f)); }
				if (Sh && J >= 1.f && !Sh->IsVisible()) { Sh->SetVisibility(true); Sh->SetWorldLocation(To + FVector(0.f, 0.f, 2.f)); AntFx::Splash(this, To, 40.f); }
			}
		}
		if (Sh && TT < VaseHit && Sh->IsVisible()) { Sh->SetVisibility(false); }
		break;
	}
	case AntProp::MicroButton:
		if (Light) { Light->SetVisibility(State == 1 && AntReact::HasPower(this)); }
		break;
	case AntProp::Mentos:
	{
		UStaticMeshComponent* Sweet = PartAt(0);
		UStaticMeshComponent* Bot = PartAt(1);
		const float TT = State == 3 ? 99.f : (State == 2 ? T : -1.f);
		const FVector BotHome = Floor(GetWorld(), Spot::Bottle, 300.f, 3000.f, this);
		const FVector Tw = Floor(GetWorld(), Spot::Tower, 300.f, 3000.f, this);
		if (Sweet)
		{
			const bool bShow = TT >= 0.f && TT < 0.5f;
			Sweet->SetVisibility(bShow);
			if (bShow) { Sweet->SetWorldLocation(Drop(GetActorLocation() + FVector(0.f, 60.f, 20.f), BotHome + FVector(0.f, 0.f, 400.f), TT / 0.5f, 40.f)); }
		}
		if (Bot)
		{
			if (TT < 1.2f) { Bot->SetWorldLocationAndRotation(BotHome, FRotator::ZeroRotator); }
			else
			{
				// it falls over and rockets along the floor (the cola still fizzing out of it) into the tower
				const float J = FMath::Clamp((TT - 1.2f) / 1.3f, 0.f, 1.f);
				const FVector Dir = (Tw - BotHome).GetSafeNormal2D();
				const FVector To = Tw - Dir * 160.f;
				const FRotator Lay = FRotationMatrix::MakeFromZ(-Dir).Rotator();
				Bot->SetWorldLocationAndRotation(FMath::Lerp(BotHome, To, J * J) + FVector(0.f, 0.f, 60.f * FMath::Min(1.f, J * 5.f)), FQuat::Slerp(FQuat::Identity, Lay.Quaternion(), FMath::Min(1.f, J * 4.f)) * FQuat(FVector(0.f, 0.f, 1.f), J * 18.f));
				if (TT < 2.5f && FMath::FRand() < Dt * 30.f) { AntFx::Droplets(this, Bot->GetComponentLocation() - Dir * 180.f, 2, 6.f); }
			}
		}
		for (int32 i = 0; i < NumBlocks; ++i)
		{
			UStaticMeshComponent* B = PartAt(2 + i);
			if (!B || !PartRel.IsValidIndex(2 + i)) { continue; }
			const int32 Row = i < 3 ? 0 : (i < 5 ? 1 : 2);
			const int32 Col = i < 3 ? i : (i < 5 ? i - 3 : 0);
			const float Off = (Col - (Row == 0 ? 1.f : (Row == 1 ? 0.5f : 0.f))) * 140.f;
			const FVector Home2 = Tw + FVector(0.f, Off, Row * 132.f);
			if (TT < 2.5f) { B->SetWorldLocationAndRotation(Home2, FRotator(0.f, i * 9.f, 0.f)); continue; }
			FRandomStream R(1234 + i * 77);
			const float Dur = R.FRandRange(0.55f, 0.95f);
			const float J = FMath::Clamp((TT - 2.5f) / Dur, 0.f, 1.f);
			const FVector Dir = (Tw - BotHome).GetSafeNormal2D();
			const FVector To = Tw + Dir * R.FRandRange(200.f, 900.f) + FVector(0.f, R.FRandRange(-600.f, 600.f), 0.f);
			B->SetWorldLocationAndRotation(Drop(Home2, FVector(To.X, To.Y, Tw.Z), J, R.FRandRange(200.f, 600.f)), FRotator(R.FRandRange(-400.f, 400.f) * J, i * 9.f + R.FRandRange(-200.f, 200.f) * J, R.FRandRange(-300.f, 300.f) * J));
		}
		break;
	}
	case AntProp::Sponge:
	{
		if (Body)
		{
			// soaked: it swells a little and drips; then it tips over onto the rack
			Body->SetRelativeScale3D(BodyScale * (1.f + 0.12f * Level));
			if (Level > 0.25f && FMath::FRand() < Dt * 3.f * Level) { AntFx::Droplets(this, GetActorLocation() + FVector(0.f, -60.f, 10.f), 1, 4.f); }
			const float K = State >= 2 ? Ease(T / 0.45f) : 0.f;
			Body->SetRelativeRotation(BodyRel.GetRotation() * FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(75.f * K)));
			Body->SetRelativeLocation(BodyRel.GetLocation() + FVector(150.f * K, -80.f * K, 40.f * K));
		}
		UStaticMeshComponent* Pl = PartAt(0);
		UStaticMeshComponent* Sh = PartAt(1);
		const float TT = State == 3 ? 99.f : (State == 2 ? T : -1.f);
		const FVector F = Floor(GetWorld(), Spot::PlateFloor, 300.f, 3000.f, this);
		if (Pl)
		{
			if (TT < 0.35f) { Pl->SetVisibility(true); Pl->SetWorldLocationAndRotation(Spot::Plate, FRotator(0.f, 0.f, 80.f)); }
			else
			{
				const float J = (TT - 0.35f) / 0.6f;
				Pl->SetVisibility(J < 1.f);
				if (J < 1.f) { Pl->SetWorldLocationAndRotation(Drop(Spot::Plate, F + FVector(0.f, 0.f, 20.f), J, 150.f), FRotator(J * 140.f, 0.f, 80.f + J * 200.f)); }
			}
		}
		if (Sh)
		{
			const bool bShow = TT >= 0.95f;
			if (Sh->IsVisible() != bShow) { Sh->SetVisibility(bShow); if (bShow) { Sh->SetWorldLocation(F + FVector(0.f, 0.f, 2.f)); } }
		}
		break;
	}
	case AntProp::Balloon:
		if (Body)
		{
			FVector Off = FVector::ZeroVector;
			const float Rise = (float)(FVector(Aux).Z - GetActorLocation().Z);
			if (State == 1 || (State == 0 && Progress > 0.f)) { Off = FVector(FMath::Sin(AnimT * 37.f), FMath::Cos(AnimT * 29.f), 0.f) * 6.f; if (FMath::FRand() < Dt * 6.f) { AntFx::Sparks(this, GetActorLocation() + FVector(0.f, 0.f, 30.f) + FMath::VRand() * 120.f, 1, 4.f); } }
			else if (State == 2) { Off = FVector(0.f, 0.f, Rise * Ease(T / 3.f)); }
			else if (State == 5) { Off = FVector(0.f, 0.f, Rise + 8.f * FMath::Sin(AnimT * 1.7f)); }
			else if (State == 6) { Off = FVector(0.f, 0.f, Rise * (1.f - Ease(T / 4.f))); }
			else if (State == 0 && Progress > 0.f) { Off = FVector(FMath::Sin(AnimT * 37.f), 0.f, 0.f) * 5.f; }
			Body->SetRelativeLocation(BodyRel.GetLocation() + Off);
		}
		break;
	case AntProp::Breaker:
		if (UStaticMeshComponent* L = PartAt(0))
		{
			const bool bUp = AntReact::HasPower(this);
			const float Want = bUp ? 0.f : 1.f;
			LeverK = FMath::FInterpTo(LeverK, Want, Dt, 14.f);
			if (PartRel.IsValidIndex(0)) { L->SetRelativeRotation(PartRel[0].GetRotation() * FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(-70.f * LeverK + 35.f))); }
		}
		break;
	default: break;
	}
}

// =====================================================================================================================
// AntReact: the API for the rest of the game, the raid reset, the dev commands
// =====================================================================================================================

namespace AntReact
{

bool Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoReact"));
	return !bOff && LevelV3::Enabled() && KitchenArt::Enabled();
}

bool HasPower(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	const AAntReactNet* N = AAntReactNet::Get(W);
	return !N || N->bPower;
}

bool IsBlind(const AActor* Who)
{
	const AAntReactNet* N = Who ? AAntReactNet::Get(Who->GetWorld()) : nullptr;
	return N && N->HasMark(Who, AntMark::Blind);
}

bool IsFloured(const AActor* Who)
{
	const AAntReactNet* N = Who ? AAntReactNet::Get(Who->GetWorld()) : nullptr;
	return N && N->HasMark(Who, AntMark::Floured);
}

namespace
{
	AAntReactProp* SpawnProp(UWorld* W, uint8 Kind, const FVector& At, float Yaw, const FVector& Aux = FVector::ZeroVector)
	{
		const FTransform TM(FRotator(0.f, Yaw, 0.f), At);
		AAntReactProp* P = W->SpawnActorDeferred<AAntReactProp>(AAntReactProp::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!P) { return nullptr; }
		P->Kind = Kind;
		P->Aux = Aux;
		P->FinishSpawning(TM);
		return P;
	}
	FVector OnTop(UWorld* W, const FVector& P, float Up, float Down)
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntReactPlace), true);
		for (TActorIterator<AAntReactProp> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
		if (W->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, Up), P - FVector(0.f, 0.f, Down), ECC_Visibility, Q)) { return H.ImpactPoint; }
		return P;
	}
}

void ResetAll(UWorld* W)
{
	if (!W || W->GetNetMode() == NM_Client || !Enabled())
	{
		return;
	}
	AntMaterials::Load();
	AAntReactNet* Net = AAntReactNet::Get(W);
	if (!Net)
	{
		Net = W->SpawnActor<AAntReactNet>(AAntReactNet::StaticClass(), FTransform::Identity);
	}
	if (!Net) { return; }
	Net->SetPower(true, TEXT("новый рейд"), Spot::Breaker);
	Net->Trips = 0;
	Net->bStripUnplugged = false;
	Net->Clouds.Reset();
	Net->Marks.Reset();
	Net->Melts.Reset();
	Net->Rollers.Reset();
	Net->LootHeat.Reset();
	Net->LootWet.Reset();
	Net->SneezeAcc.Reset();
	Net->NextSneeze.Reset();
	Net->MicroUntil = -1.f;
	Net->MicroBeepAt = -1.f;
	Net->ChainStart.Reset();
	Net->HeatMode = -1;
	for (TActorIterator<AAntReactProp> It(W); It; ++It) { It->Destroy(); }

	SpawnProp(W, AntProp::Pepper, OnTop(W, Spot::Pepper, 500.f, 900.f), 0.f);
	SpawnProp(W, AntProp::Flour, OnTop(W, Spot::Flour, 500.f, 900.f), 0.f);
	SpawnProp(W, AntProp::IceTray, OnTop(W, Spot::Ice, 500.f, 900.f), 0.f);
	SpawnProp(W, AntProp::PeaBowl, OnTop(W, Spot::Peas, 500.f, 900.f), 0.f);
	SpawnProp(W, AntProp::Slingshot, OnTop(W, Spot::Slingshot, 300.f, 900.f), 0.f, OnTop(W, Spot::SlingTarget, 400.f, 900.f) + FVector(0.f, 0.f, 20.f));
	SpawnProp(W, AntProp::Chocolate, OnTop(W, Spot::Choc, 300.f, 1500.f), 15.f);
	SpawnProp(W, AntProp::ToasterSpring, Spot::ToasterLever, 0.f, OnTop(W, Spot::SpiceJar, 60.f, 400.f));
	SpawnProp(W, AntProp::BookRow, OnTop(W, Spot::Books, 900.f, 1500.f), 0.f);
	SpawnProp(W, AntProp::MicroButton, Spot::MicroButton, 0.f);
	SpawnProp(W, AntProp::Mentos, OnTop(W, Spot::MentosPack, 600.f, 1500.f), 0.f);
	SpawnProp(W, AntProp::Sponge, OnTop(W, Spot::Sponge, 300.f, 800.f), 0.f);
	SpawnProp(W, AntProp::Balloon, OnTop(W, Spot::Balloon, 300.f, 900.f), 0.f);
	SpawnProp(W, AntProp::PowerStrip, OnTop(W, Spot::Strip, 300.f, 800.f), 0.f);
	{
		// the breaker box on the hall wall: wherever the wall really is
		FHitResult H;
		FVector At = Spot::Breaker;
		const FVector From(19300.f, Spot::Breaker.Y, Spot::Breaker.Z);
		if (W->LineTraceSingleByChannel(H, From, From + FVector(3000.f, 0.f, 0.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(AntReactWall), false))) { At = H.ImpactPoint - FVector(18.f, 0.f, 0.f); }
		SpawnProp(W, AntProp::Breaker, At, 0.f);
	}
	int32 N = 0;
	for (TActorIterator<AAntReactProp> It(W); It; ++It) { ++N; }
	UE_LOG(LogTemp, Display, TEXT("ANTREACT reset: power on, %d reactive objects in place"), N);
	AntMatter::ResetAll(W);   // nothing wet, hot, burning, frozen or charred any more; the ice and the burnt-away things back
}

void OnToasterPop(AKitchenToaster* Toaster)
{
	UWorld* W = Toaster ? Toaster->GetWorld() : nullptr;
	AAntReactProp* P = AAntReactProp::Find(W, AntProp::ToasterSpring);
	if (!P || !P->bSuper) { return; }
	P->bSuper = false;
	P->SetState(2);
	AntAudio::Everyone(Toaster, EAntSfx::SlingTwang, Spot::ToastFrom, 1.2f);
	if (AAntReactNet* Net = AAntReactNet::Get(W)) { Net->ChainLog(TEXT("C13"), TEXT("SUPER POP: the toast flies across the kitchen")); }
}

bool OnTidied(UWorld* W, AActor* Src, uint8 MessKind, uint8 Who)
{
	AAntReactNet* Net = AAntReactNet::Get(W);
	AAntReactProp* P = Cast<AAntReactProp>(Src);
	if (!Net || (!P && Src != Net)) { return false; }
	const uint8 PropKind = P ? P->Kind : 255;
	if (MessKind == NpcRoutine::Power)
	{
		Net->SetPower(true, TEXT("папа поднял автомат в щитке"), Spot::Breaker);
		Say(W, NpcRoutine::Dad, TEXT("Так. Есть свет."));
		if (Net->Trips >= 2 && !Net->bStripUnplugged)
		{
			Net->bStripUnplugged = true;
			Say(W, NpcRoutine::Dad, TEXT("И удлинитель из розетки — от греха подальше."));
			Net->ChainLog(TEXT("power"), TEXT("the Dad unplugs the power strip (it tripped twice)"));
		}
		return true;
	}
	if (MessKind == NpcRoutine::Pickup)
	{
		int32 N = 0;
		for (int32 i = Net->Rollers.Num() - 1; i >= 0; --i)
		{
			if (AKitchenLoot* L = Net->Rollers[i].Loot.Get()) { if (!L->IsCarried()) { L->Destroy(); ++N; } }
			Net->Rollers.RemoveAt(i);
		}
		Net->ChainLog(TEXT("C11"), FString::Printf(TEXT("%s picks up %d peas"), NpcRoutine::WhoName(Who), N));
		return true;
	}
	FVector Where = Src->GetActorLocation();
	if (PropKind == AntProp::Flour) { Where = Spot::FlourFloor; for (int32 i = Net->Clouds.Num() - 1; i >= 0; --i) { if (Net->Clouds[i].Kind == AntCloudKind::Flour) { Net->Clouds.RemoveAt(i); } } }
	else if (PropKind == AntProp::Chocolate) { Where = Spot::ChocFloor; }
	else if (PropKind == AntProp::BookRow) { Where = Spot::VaseFloor; if (AKitchenSlick* S = AKitchenSlick::At(W, Spot::VaseFloor, 600.f)) { S->StartWipe(1.f); } }
	else if (PropKind == AntProp::Sponge) { Where = Spot::PlateFloor; }
	const int32 Stains = AntMess::CleanArea(Net, Where, 1200.f);
	LevelV3::RemovePlayZones(Where, 1200.f);
	if (P && (PropKind == AntProp::BookRow || PropKind == AntProp::Sponge)) { P->SetState(4); }
	Net->ChainLog(TEXT("tidy"), FString::Printf(TEXT("%s cleaned up after %s (%d stains)"), NpcRoutine::WhoName(Who), P ? AntProp::Name(PropKind) : TEXT("?"), Stains));
	return true;
}

void DrawHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC)
{
	if (!Canvas || !PC || !Enabled()) { return; }
	const UWorld* W = PC->GetWorld();
	const AAntReactNet* Net = AAntReactNet::Get(W);
	if (!Net) { return; }
	const float Now = Net->NetNow();
	float White = 0.f;
	FLinearColor Tint(1.f, 1.f, 1.f);
	// the blinded Human (a player in versus): flour in the eyes
	if (const AHumanCharacter* H = Cast<AHumanCharacter>(PC->GetPawn()))
	{
		for (const FAntReactMark& M : Net->Marks) { if (M.Who == H && M.Kind == AntMark::Blind) { White = FMath::Max(White, FMath::Clamp((M.Until - Now) / 1.5f, 0.f, 1.f) * 0.92f); } }
	}
	// any camera inside a cloud: a haze of its colour
	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	for (const FAntReactCloud& C : Net->Clouds)
	{
		const float K = CloudAt(C, CamLoc, Now);
		if (K > 0.f)
		{
			const float A = K * (C.Kind == AntCloudKind::Flour ? 0.55f : 0.3f);
			if (A > White) { White = A; Tint = C.Kind == AntCloudKind::Flour ? FlourCol : PepperCol; }
		}
	}
	if (White > 0.01f)
	{
		FCanvasTileItem Tile(FVector2D(0.f, 0.f), GWhiteTexture, FVector2D(Canvas->SizeX, Canvas->SizeY), FLinearColor(Tint.R, Tint.G, Tint.B, White));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
	// the blackout: one line for a few seconds
	if (!Net->bPower && Now - Net->PowerAt < 7.f)
	{
		FCanvasTextItem Txt(FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.16f), FText::FromString(TEXT("ВЫБИЛО ПРОБКИ — в квартире темно")), GEngine->GetLargeFont(), FLinearColor(0.75f, 0.85f, 1.f));
		Txt.bCentreX = true;
		Txt.EnableShadow(FLinearColor::Black);
		Txt.Scale = FVector2D(1.4f, 1.4f);
		Canvas->DrawItem(Txt);
	}
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (A.Num() == 0 || !GM) { return false; }
	const FString Op = A[0].ToLower();
	if (Op != TEXT("chain") && Op != TEXT("mat") && Op != TEXT("rx") && Op != TEXT("power") && Op != TEXT("heat")) { return false; }   // ("react" is NpcRoutine's)
	UWorld* W = GM->GetWorld();
	const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	if (Op == TEXT("chain"))
	{
		const FString C = Sub.ToUpper();
		if (C == TEXT("C1") || C == TEXT("C2") || C == TEXT("C3") || C == TEXT("C4") || C == TEXT("C5") || C == TEXT("C6")) { return false; }   // the v3 chains (AntRaid.cpp)
		if (!Enabled()) { UE_LOG(LogTemp, Warning, TEXT("ANTREACT off (needs level v3)")); return true; }
		AAntReactNet* Net = AAntReactNet::Get(W);
		if (!Net) { ResetAll(W); Net = AAntReactNet::Get(W); }
		if (C == TEXT("LIST") || C.IsEmpty())
		{
			for (const FChainDef& D : GChains) { UE_LOG(LogTemp, Display, TEXT("ANTCHAIN %s — %s"), D.Id, D.Ru); }
			return true;
		}
		auto Force = [W](uint8 Kind) { if (AAntReactProp* P = AAntReactProp::Find(W, Kind)) { if (P->bDone) { P->ResetProp(); } P->ForceDone(nullptr); return true; } return false; };
		bool bOk = true;
		if (C == TEXT("C7")) { bOk = Force(AntProp::Pepper); }
		else if (C == TEXT("C8")) { bOk = Force(AntProp::Flour); }
		else if (C == TEXT("C9")) { bOk = Force(AntProp::IceTray); }
		else if (C == TEXT("C10")) { bOk = Force(AntProp::Slingshot); }
		else if (C == TEXT("C11")) { bOk = Force(AntProp::PeaBowl); }
		else if (C == TEXT("C12")) { bOk = Force(AntProp::Chocolate); }
		else if (C == TEXT("C13")) { bOk = Force(AntProp::ToasterSpring); }
		else if (C == TEXT("C14")) { bOk = Force(AntProp::BookRow); }
		else if (C == TEXT("C15")) { bOk = Force(AntProp::MicroButton); }
		else if (C == TEXT("C16"))
		{
			bOk = false;
			for (TActorIterator<AKitchenDoor> It(W); It; ++It)
			{
				if (It->Kind == EKitchenDoorKind::Dishwasher) { It->SetOpen(true); bOk = true; }
			}
		}
		else if (C == TEXT("C17"))
		{
			FVector At = Spot::PeasFloor + FVector(1400.f, 0.f, 0.f);
			if (A.Num() >= 5) { At = Vec(2); }
			bOk = W->SpawnActor<ARobotVacuum>(ARobotVacuum::StaticClass(), FTransform(At)) != nullptr;
		}
		else if (C == TEXT("C18")) { bOk = Force(AntProp::Mentos); }
		else if (C == TEXT("C19"))
		{
			if (A.IsValidIndex(2) && A[2] == TEXT("now")) { if (AAntReactProp* P = AAntReactProp::Find(W, AntProp::Sponge)) { P->Level = 1.f; } }
			else if (UAntFluidWorld* Fl = UAntFluidWorld::Active(W)) { Fl->DevCommand({ TEXT("fluid"), TEXT("tap"), TEXT("on"), TEXT("14") }); }
		}
		else if (C == TEXT("C20")) { bOk = Force(AntProp::Balloon); }
		else if (C == TEXT("C21"))
		{
			FAntFoodDef D; D.Name = TEXT("Кусочек сахара"); D.Value = 4; D.Weight = 1; D.Color = FLinearColor::White; D.Shape = EAntShape::Cube; D.Scale = FVector(0.15f);
			const FVector At = A.Num() >= 5 ? Vec(2) : Spot::IceFloor + FVector(0.f, -200.f, 60.f);
			const FTransform TM(At);
			if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)) { L->InitFood(D); L->FinishSpawning(TM); }
		}
		else if (C == TEXT("C22")) { bOk = Force(AntProp::PowerStrip); }
		else { UE_LOG(LogTemp, Warning, TEXT("ANTCHAIN unknown chain %s (chain list)"), *C); return true; }
		Net->ChainStart.Add(C, W->GetTimeSeconds());
		UE_LOG(LogTemp, Display, TEXT("ANTCHAIN %s started by dev command (%s)"), *C, bOk ? TEXT("ok") : TEXT("FAILED: its object is missing"));
		return true;
	}
	if (Op == TEXT("mat"))
	{
		if (Sub == TEXT("list"))
		{
			for (const FAntMatProps& M : AntMaterials::All()) { UE_LOG(LogTemp, Display, TEXT("ANTMAT %s"), *AntMaterials::Describe(M)); }
		}
		else if (Sub == TEXT("dump"))
		{
			const FString Path = A.IsValidIndex(2) && A[2] == TEXT("config") ? FPaths::ProjectConfigDir() / TEXT("AntMaterials.json") : FPaths::ProjectSavedDir() / TEXT("AntMaterials.json");
			UE_LOG(LogTemp, Display, TEXT("ANTMAT dump %s: %s"), *Path, AntMaterials::DumpJson(Path) ? TEXT("ok") : TEXT("FAILED"));
		}
		else if (Sub == TEXT("reload")) { AntMaterials::Load(); }
		else if (Sub == TEXT("info") || Sub == TEXT("at"))
		{
			// at the crosshair of the first player, or along a ray (mat at x y z dx dy dz)
			APlayerController* PC = W->GetFirstPlayerController();
			FVector From, Dir;
			if (Sub == TEXT("at") && A.Num() >= 8) { From = Vec(2); Dir = Vec(5).GetSafeNormal(); }
			else if (PC) { FRotator R; PC->GetPlayerViewPoint(From, R); Dir = R.Vector(); }
			else { return true; }
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AntMatInfo), true, PC ? PC->GetPawn() : nullptr);
			FString Line = TEXT("ничего");
			if (W->LineTraceSingleByChannel(H, From, From + Dir * 30000.f, ECC_Visibility, Q))
			{
				FString Why;
				const FAntMatProps* M = AntMaterials::ForComponent(H.GetComponent(), &Why);
				const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(H.GetComponent());
				Line = FString::Printf(TEXT("%s / %s (%s) в %s: %s [%s]"), *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent()), S && S->GetStaticMesh() ? *S->GetStaticMesh()->GetName() : TEXT("-"),
					*H.ImpactPoint.ToCompactString(), M ? *AntMaterials::Describe(*M) : TEXT("материал не известен"), *Why);
				Line += FString::Printf(TEXT(" | тепло %.0f °C"), HeatAt(W, H.ImpactPoint));
			}
			UE_LOG(LogTemp, Display, TEXT("ANTMAT info: %s"), *Line);
			if (PC) { if (AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn())) { Me->Toast(Line.Left(300)); } }
		}
		else if (Sub == TEXT("get") && A.IsValidIndex(2))
		{
			const FAntMatProps* M = AntMaterials::ForName(A[2]);
			UE_LOG(LogTemp, Display, TEXT("ANTMAT %s -> %s"), *A[2], M ? *AntMaterials::Describe(*M) : TEXT("?"));
		}
		return true;
	}
	if (!Enabled()) { UE_LOG(LogTemp, Warning, TEXT("ANTREACT off (needs level v3)")); return true; }
	AAntReactNet* Net = AAntReactNet::Get(W);
	if (!Net) { ResetAll(W); Net = AAntReactNet::Get(W); }
	if (!Net) { return true; }
	if (Op == TEXT("power"))
	{
		if (Sub == TEXT("off")) { Net->SetPower(false, TEXT("админ"), Spot::Breaker); }
		else if (Sub == TEXT("on")) { Net->SetPower(true, TEXT("админ"), Spot::Breaker); }
		else if (Sub == TEXT("short")) { Net->Short(A.Num() >= 5 ? Vec(2) : Spot::Strip, TEXT("админ: короткое замыкание"), nullptr); }
		else if (Sub == TEXT("unplug")) { Net->bStripUnplugged = !Net->bStripUnplugged; }
		if (const AAntReactProp* S = AAntReactProp::Find(W, AntProp::PowerStrip))
		{
			const UAntFluidWorld* Fl = UAntFluidWorld::Active(W);
			float Mx = 0.f;
			for (int32 i = 0; i < 9 && Fl; ++i) { Mx = FMath::Max(Mx, Fl->DepthAt(S->GetActorLocation() + FVector((i % 3 - 1) * 200.f, (i / 3 - 1) * 200.f, 20.f))); }
			UE_LOG(LogTemp, Display, TEXT("ANTREACT strip at %s: water depth there %.2f, near (400) %.2f, floor litres %.1f"), *S->GetActorLocation().ToCompactString(), Fl ? Fl->DepthAt(S->GetActorLocation() + FVector(0.f, 0.f, 20.f)) : -1.f, Mx, Fl ? Fl->LitresIn(1) : -1.f);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTREACT %s"), *Net->Describe());
		return true;
	}
	if (Op == TEXT("heat"))
	{
		Net->HeatMode = Sub == TEXT("on") ? 1 : (Sub == TEXT("off") ? 0 : -1);
		UE_LOG(LogTemp, Display, TEXT("ANTREACT heat mode %d"), (int32)Net->HeatMode);
		return true;
	}
	// react ...
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (Sub == TEXT("info") || Sub.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTREACT %s"), *Net->Describe());
		for (TActorIterator<AAntReactProp> It(W); It; ++It)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTREACT prop %s state %d done %d progress %.2f level %.2f at %s use %s"), AntProp::Name(It->Kind), It->State, It->bDone ? 1 : 0, It->Progress, It->Level, *It->GetActorLocation().ToCompactString(), *It->UsePoint().ToCompactString());
		}
	}
	else if (Sub == TEXT("reset")) { ResetAll(W); }
	else if (Sub == TEXT("cloud"))
	{
		const bool bFlour = A.IsValidIndex(2) && A[2].ToLower() == TEXT("flour");
		Net->AddCloud(bFlour ? AntCloudKind::Flour : AntCloudKind::Pepper, Vec(3), Num(6, 900.f), 14.f, FVector(0.f, 0.f, -10.f));
	}
	else if (Sub == TEXT("ice"))
	{
		const int32 N = FMath::Clamp((int32)Num(5, 8.f), 1, 30);
		for (int32 i = 0; i < N; ++i) { Net->AddMelt(0, Vec(2) + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 2.f), 40.f); }
	}
	else if (Sub == TEXT("peas") || Sub == TEXT("choc") || Sub == TEXT("sugar") || Sub == TEXT("butter") || Sub == TEXT("bread"))
	{
		const int32 N = Sub == TEXT("peas") ? FMath::Clamp((int32)Num(5, 20.f), 1, 60) : 1;
		for (int32 i = 0; i < N; ++i)
		{
			FAntFoodDef D;
			D.Name = Sub == TEXT("peas") ? TEXT("горошина") : Sub == TEXT("choc") ? TEXT("Плитка шоколада") : Sub == TEXT("sugar") ? TEXT("Кусочек сахара") : Sub == TEXT("butter") ? TEXT("Кусочек масла") : TEXT("Хлебная крошка");
			D.Value = 3; D.Weight = Sub == TEXT("choc") ? 3 : 1; D.Color = FLinearColor(0.4f, 0.6f, 0.2f); D.Shape = EAntShape::Sphere; D.Scale = FVector(Sub == TEXT("choc") ? 0.4f : 0.13f);
			const FTransform TM(Vec(2) + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 40.f + i * 3.f));
			if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->InitFood(D);
				L->FinishSpawning(TM);
				if (Sub == TEXT("peas")) { const float An = FMath::FRandRange(0.f, 6.28f); L->Kick(FVector(FMath::Cos(An), FMath::Sin(An), 0.1f) * FMath::FRandRange(200.f, 700.f), TM.GetLocation()); }
			}
		}
	}
	else if (Sub == TEXT("blind")) { if (AHumanCharacter* H = Dad(W)) { Net->Mark(H, AntMark::Blind, 5.f); } }
	else if (Sub == TEXT("sneeze"))
	{
		const FString Who = A.IsValidIndex(2) ? A[2].ToLower() : FString(TEXT("dad"));
		AActor* T = Who == TEXT("dog") ? (AActor*)Pet(W, 0) : Who == TEXT("cat") ? (AActor*)Pet(W, 1) : Who == TEXT("gran") ? (AActor*)First<AGrandmaCharacter>(W) : Who == TEXT("kid") ? (AActor*)First<AKidCharacter>(W) : (AActor*)Dad(W);
		Net->Sneeze(T, TEXT("админ"));
	}
	else if (Sub == TEXT("dad") && A.Num() >= 5)
	{
		if (AHumanCharacter* H = Dad(W)) { H->SetActorLocation(Vec(2) + FVector(0.f, 0.f, H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20.f), false, nullptr, ETeleportType::TeleportPhysics); if (A.IsValidIndex(5)) { H->SetActorRotation(FRotator(0.f, Num(5), 0.f)); } }
	}
	else if (Sub == TEXT("bots") && A.Num() >= 6)
	{
		int32 N = (int32)Num(2, 1.f);
		for (TActorIterator<AAntCharacter> It(W); It && N > 0; ++It)
		{
			AAntCharacter* B = *It;
			if (B->bDead || B->Creature != 0 || B->Team != 0 || B->IsPlayerControlled()) { continue; }
			B->LetGo();
			B->ApplyTeleport(Vec(3) + FVector(FMath::FRandRange(-50.f, 50.f), FMath::FRandRange(-50.f, 50.f), 15.f));
			--N;
		}
	}
	else if (Sub == TEXT("tp") && Me && A.IsValidIndex(2))
	{
		for (uint8 k = 0; k < AntProp::Count; ++k)
		{
			if (A[2].ToLower() == AntProp::Name(k)) { if (AAntReactProp* P = AAntReactProp::Find(W, k)) { Me->ApplyTeleport((k == AntProp::PowerStrip ? FVector(P->Aux) : P->UsePoint()) + FVector(0.f, 0.f, 20.f)); } }
		}
	}
	else if (Sub == TEXT("prop") && A.IsValidIndex(3))
	{
		for (uint8 k = 0; k < AntProp::Count; ++k)
		{
			if (A[2].ToLower() != AntProp::Name(k)) { continue; }
			if (AAntReactProp* P = AAntReactProp::Find(W, k))
			{
				if (A[3] == TEXT("force")) { P->ForceDone(Me); }
				else if (A[3] == TEXT("reset")) { P->ResetProp(); }
				else if (A[3] == TEXT("level")) { P->Level = Num(4, 1.f); }
			}
		}
	}
	else if (Sub == TEXT("walk") && A.Num() >= 5)
	{
		// the Dad as a puppet walks straight to x y z (the AI lets go of him): over the peas, through a cloud...
		if (AHumanCharacter* H = Dad(W)) { H->bDevPuppet = true; H->DevWalk = Vec(2); H->DevWalkScale = Num(5, 1.f); }
	}
	else if (Sub == TEXT("dogto") && A.Num() >= 5) { if (ACritter* D = Pet(W, A.IsValidIndex(5) && A[5] == TEXT("cat") ? 1 : 0)) { D->DevPlace(Vec(2), 0.f); } }
	else if (Sub == TEXT("free")) { if (AHumanCharacter* H = Dad(W)) { H->bDevPuppet = false; H->DevWalk = FVector::ZeroVector; H->DevWalkScale = 1.f; } }
	else if (Sub == TEXT("noise") && A.Num() >= 5) { GM->MakeNoise(Vec(2), Num(5, 0.9f), AntNoise::Crash, nullptr, Me); }
	else if (Sub == TEXT("quit")) { UE_LOG(LogTemp, Display, TEXT("ANTREACT quit")); FPlatformMisc::RequestExit(false); }
	else if (Sub == TEXT("vacuum") && A.Num() >= 5) { W->SpawnActor<ARobotVacuum>(ARobotVacuum::StaticClass(), FTransform(Vec(2))); }
	else if (Sub == TEXT("cam")) { AntCloth::DevCam(W, Vec(2), Vec(5), A.IsValidIndex(8) ? Num(8) : 70.f); }
	else if (Sub == TEXT("camcat") || Sub == TEXT("camdog") || Sub == TEXT("camdad"))
	{
		AActor* T = Sub == TEXT("camdad") ? (AActor*)Dad(W) : (AActor*)Pet(W, Sub == TEXT("camcat") ? 1 : 0);
		if (T)
		{
			const FVector H = Head(T);
			AntCloth::DevCam(W, H + T->GetActorForwardVector() * Num(2, 900.f) + FVector(0.f, 0.f, Num(3, 150.f)), H, 60.f);
			UE_LOG(LogTemp, Display, TEXT("ANTREACT camera on %s at %s"), *T->GetName(), *H.ToCompactString());
		}
	}
	else if (Sub == TEXT("camoff")) { AntCloth::DevCam(W, FVector::ZeroVector, FVector::ZeroVector, 0.f); }
	else if (Sub == TEXT("shot")) { AntCloth::DevShot(W, TEXT("chains"), A.IsValidIndex(2) ? A[2] : FString(TEXT("chain"))); }
	else if (Sub == TEXT("cost"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTREACT cost: net tick %.3f ms (avg %.3f), %d props, %d clouds, %d melts, %d rollers, fx live %d"), Net->LastTickMs, Net->AvgTickMs, [W]() { int32 n = 0; for (TActorIterator<AAntReactProp> It(W); It; ++It) { ++n; } return n; }(),
			Net->Clouds.Num(), Net->Melts.Num(), Net->Rollers.Num(), AAntFxManager::Get(W) ? AAntFxManager::Get(W)->NumLive() : -1);
	}
	else { UE_LOG(LogTemp, Warning, TEXT("ANTREACT unknown: react %s"), *Sub); }
	return true;
}

}
