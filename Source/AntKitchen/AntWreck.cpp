#include "AntWreck.h"
#include "AntWreckGeo.h"
#include "AntSwing.h"
#include "AntAudio.h"
#include "AntTypes.h"
#include "AntRun.h"
#include "AntCharacter.h"
#include "AntSwarm.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "KitchenArt.h"
#include "KitchenBuilder.h"
#include "KitchenFixture.h"
#include "KitchenLoot.h"
#include "AntMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

// =============================================================================================
// Classes and the catalog
// =============================================================================================

namespace
{
	using namespace AntWreck;

	const FClassDef GClasses[] =
	{
		//  name        kg/m2 wakeK hp/kg  minHp breaks noise
		{ TEXT("дерево"),      4.f, 0.55f, 6.f,  40.f, true,  0.7f },
		{ TEXT("ДСП"),         4.f, 0.55f, 4.f,  30.f, true,  0.7f },
		{ TEXT("металл"),      6.f, 0.6f, 15.f,  60.f, false, 0.9f },
		{ TEXT("стекло"),      3.f, 0.4f,  3.f,   8.f, true,  0.9f },
		{ TEXT("керамика"),    3.f, 0.4f,  3.f,   8.f, true,  0.8f },
		{ TEXT("пластик"),     2.f, 0.4f,  6.f,  15.f, true,  0.4f },
		{ TEXT("ткань"),      3.5f, 0.35f, 30.f, 60.f, false, 0.1f },
		{ TEXT("бумага"),      5.f, 0.4f, 10.f,  15.f, false, 0.3f },
		{ TEXT("техника"),    10.f, 2.2f,  8.f, 200.f, false, 1.0f },
		{ TEXT("еда"),         4.f, 0.3f,  4.f,   5.f, true,  0.3f },
	};

	bool Has(const FString& N, std::initializer_list<const TCHAR*> Keys)
	{
		for (const TCHAR* K : Keys) { if (N.Contains(K)) { return true; } }
		return false;
	}

	/** -1: not wreckable (architecture, rugs, strings). */
	int32 MatOf(const FString& Name, bool& bFixed, float& WakeMul)
	{
		const FString N = Name.ToLower();
		bFixed = false;
		WakeMul = 1.f;
		if (Has(N, { TEXT("skirting"), TEXT("splashback"), TEXT("window"), TEXT("worktop"), TEXT("_rug"), TEXT("carpet"), TEXT("cornice"), TEXT("facade"), TEXT("road"), TEXT("kerb"),
			TEXT("pavement"), TEXT("manhole"), TEXT("stormdrain"), TEXT("gutter"), TEXT("drainpipe"), TEXT("roofedge"), TEXT("doorframe"), TEXT("puddle"), TEXT("fence"), TEXT("flowerbed"),
			TEXT("_tree"), TEXT("steppingstone"), TEXT("anthill"), TEXT("ceilinglamp"), TEXT("pendantlamp"), TEXT("yarnbridge"), TEXT("yarnthread"), TEXT("garland"), TEXT("chilistring"),
			TEXT("flypaper"), TEXT("curtain"), TEXT("doormat"), TEXT("petmat"), TEXT("sinkhose"), TEXT("sinktrap"), TEXT("hood"), TEXT("faucet"), TEXT("track"), TEXT("s_car"),
			TEXT("streetlamp"), TEXT("hydrant"), TEXT("outlet"), TEXT("switch"), TEXT("vent"), TEXT("dustbunny"), TEXT("balloon"), TEXT("screw"), TEXT("nail"), TEXT("hinge"),
			TEXT("bracket"), TEXT("tapestrip"), TEXT("magnet"), TEXT("bookramp"), TEXT("gardenhose"), TEXT("sprinkler"), TEXT("outdoortap"), TEXT("lawnrobot"), TEXT("_y_bin"),
			TEXT("dogpoop"), TEXT("fridgenote"), TEXT("calendar"), TEXT("kiddrawing") }))
		{
			return -1;
		}
		if (Has(N, { TEXT("bookshelf"), TEXT("tvstand"), TEXT("shoerack"), TEXT("wallshelf"), TEXT("spiceshelf") })) { return Board; }
		if (Has(N, { TEXT("_base"), TEXT("dishwasher"), TEXT("_range"), TEXT("_sink"), TEXT("radiator") })) { bFixed = true; return N.Contains(TEXT("radiator")) ? Metal : Board; }
		if (Has(N, { TEXT("wallcab") })) { WakeMul = 3.f; return Board; }
		if (Has(N, { TEXT("fridge") })) { return Appliance; }
		if (Has(N, { TEXT("microwave"), TEXT("toaster"), TEXT("_tv"), TEXT("gameconsole") })) { return Appliance; }
		if (Has(N, { TEXT("glasses"), TEXT("jar"), TEXT("bottle"), TEXT("vase"), TEXT("spicejar") })) { return Glass; }
		if (Has(N, { TEXT("plate"), TEXT("mug"), TEXT("k_bowl"), TEXT("plantpot"), TEXT("pottedplant"), TEXT("cup"), TEXT("sugarbowl"), TEXT("snackbowl"), TEXT("shaker"), TEXT("fruitbowl"), TEXT("picture") })) { return China; }
		if (Has(N, { TEXT("k_bucket") })) { return Plastic; }   // KAN-22: the hero bucket is moulded plastic (cracks and splits, AntWreckGeo pre-cut pieces)
		if (Has(N, { TEXT("_pot"), TEXT("ladle"), TEXT("whisk"), TEXT("fork"), TEXT("knife"), TEXT("tincan"), TEXT("kettle"), TEXT("bucket"), TEXT("k_bin"), TEXT("cookietin"), TEXT("petbowl"),
			TEXT("utensilrail"), TEXT("sodacan"), TEXT("coin"), TEXT("dogbowl"), TEXT("wateringcan"), TEXT("colacap"), TEXT("wirerack") })) { return Metal; }
		if (Has(N, { TEXT("sofa"), TEXT("armchair"), TEXT("pillow"), TEXT("cushion"), TEXT("towel"), TEXT("sock"), TEXT("slipper"), TEXT("dogbed"), TEXT("throw"), TEXT("backpack"),
			TEXT("yarnball"), TEXT("shoe"), TEXT("scarf"), TEXT("dogrope"), TEXT("trashbag"), TEXT("bin_trash"), TEXT("ragpile") }))
		{
			if (Has(N, { TEXT("sofa"), TEXT("armchair") }) && !N.Contains(TEXT("cushion"))) { WakeMul = 2.5f; }
			return Cloth;
		}
		if (Has(N, { TEXT("book"), TEXT("magazine"), TEXT("newspaper"), TEXT("cardboard"), TEXT("delivery"), TEXT("cereal"), TEXT("pastabox"), TEXT("milkcarton"), TEXT("papertowel"),
			TEXT("floursack"), TEXT("grocerybag"), TEXT("giftbox"), TEXT("dogfoodbag"), TEXT("teabag"), TEXT("matchbox") })) { return Paper; }
		if (Has(N, { TEXT("remote"), TEXT("controller"), TEXT("charger"), TEXT("powerstrip"), TEXT("toy"), TEXT("detergent"), TEXT("icecream"), TEXT("butter"), TEXT("eggtray"),
			TEXT("icetray"), TEXT("dishrack"), TEXT("sponge"), TEXT("phone"), TEXT("dustpan"), TEXT("tennisball"), TEXT("dogball"), TEXT("veggiecrate"), TEXT("crisper"), TEXT("v3_pail"), TEXT("sprayer"), TEXT("dishsoap") })) { return Plastic; }
		if (Has(N, { TEXT("apple"), TEXT("_bun"), TEXT("toast"), TEXT("sandwich"), TEXT("cheese"), TEXT("bone"), TEXT("kibble") })) { return Food; }
		if (Has(N, { TEXT("bookshelf"), TEXT("tvstand"), TEXT("shoerack"), TEXT("wallshelf"), TEXT("spiceshelf") })) { return Board; }
		return Wood;
	}

	struct FCatalog
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FPiece> Pieces;
		TMap<int32, int32> ByInst;
		TMap<const UPrimitiveComponent*, int32> ByComp;
	};
	FCatalog GCat;

	AKitchenBuilder* BuilderOf(UWorld* W)
	{
		for (TActorIterator<AKitchenBuilder> It(W); It; ++It) { return *It; }
		return nullptr;
	}

	void BuildCatalog(UWorld* W)
	{
		if (GCat.World.Get() == W && GCat.Pieces.Num() > 0)
		{
			return;
		}
		GCat = FCatalog();
		GCat.World = W;
		AKitchenBuilder* B = BuilderOf(W);
		if (!B)
		{
			return;
		}
		const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
		int32 Skipped = 0;
		for (int32 i = 0; i < All.Num(); ++i)
		{
			UStaticMeshComponent* C = B->ArtComponent(i);
			if (!C || !C->GetStaticMesh() || C->ComponentHasTag(TEXT("AntsOnly")))
			{
				continue;
			}
			bool bFixed = false;
			float WakeMul = 1.f;
			const int32 M = MatOf(All[i].Name, bFixed, WakeMul);
			if (M < 0)
			{
				++Skipped;
				continue;
			}
			FPiece P;
			P.Inst = i;
			P.Mat = (uint8)M;
			P.bFixed = bFixed;
			P.Box = C->Bounds.GetBox();
			const FVector Sz = P.Box.GetSize() / UuPerM;
			const float Area = 2.f * (float)(Sz.X * Sz.Y + Sz.Y * Sz.Z + Sz.X * Sz.Z);
			P.Mass = FMath::Clamp(Area * Class(M).Density * WakeMul, 0.05f, 300.f);
			P.Hp = FMath::Max(Class(M).MinHp, P.Mass * Class(M).HpPerKg);
			GCat.ByInst.Add(i, GCat.Pieces.Num());
			GCat.ByComp.Add(C, i);
			GCat.Pieces.Add(P);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK catalog: %d pieces can wreck, %d are architecture"), GCat.Pieces.Num(), Skipped);
	}

	AAntWreckBody* BodyFor(UWorld* W, int32 Inst)
	{
		for (TActorIterator<AAntWreckBody> It(W); It; ++It)
		{
			if (It->Inst == Inst && !It->IsActorBeingDestroyed()) { return *It; }
		}
		return nullptr;
	}

	AAntKitchenGameMode* GMOf(UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	float BoxDist(const FBox& B, const FVector& P) { return FMath::Sqrt((float)B.ComputeSquaredDistanceToPoint(P)); }

	/** Floor height under P (the first blocking thing below), ignoring the listed actors. */
	float FloorUnder(UWorld* W, const FVector& P, const TArray<const AActor*>& Ignore, float Fallback)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WreckFloor), true);
		for (const AActor* A : Ignore) { Q.AddIgnoredActor(A); }
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
		for (TActorIterator<AKitchenLoot> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
		for (TActorIterator<AAntWreckBody> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, P, P - FVector(0.f, 0.f, 8000.f), ECC_Visibility, Q))
		{
			return (float)H.ImpactPoint.Z;
		}
		return Fallback;
	}
}

const AntWreck::FClassDef& AntWreck::Class(uint8 Mat) { return GClasses[FMath::Min<int32>(Mat, UE_ARRAY_COUNT(GClasses) - 1)]; }

bool AntWreck::Enabled()
{
	static int32 S = -1;
	if (S < 0) { S = FParse::Param(FCommandLine::Get(), TEXT("AntNoWreck")) ? 0 : 1; }
	return S == 1;
}

const TArray<AntWreck::FPiece>& AntWreck::Pieces(UWorld* World)
{
	BuildCatalog(World);
	return GCat.Pieces;
}

const AntWreck::FPiece* AntWreck::PieceOf(UWorld* World, int32 Inst)
{
	BuildCatalog(World);
	const int32* I = GCat.ByInst.Find(Inst);
	return I ? &GCat.Pieces[*I] : nullptr;
}

int32 AntWreck::FindPiece(UWorld* World, const FString& NameOrIndex)
{
	BuildCatalog(World);
	if (NameOrIndex.IsNumeric())
	{
		return FCString::Atoi(*NameOrIndex);
	}
	FString Name = NameOrIndex;
	int32 Index = 0;
	FString L, R;
	if (NameOrIndex.Split(TEXT("#"), &L, &R)) { Name = L; Index = FCString::Atoi(*R); }
	const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
	for (const FPiece& P : GCat.Pieces)
	{
		if (FString(All[P.Inst].Name).Equals(Name, ESearchCase::IgnoreCase) && All[P.Inst].Index == Index) { return P.Inst; }
	}
	for (const FPiece& P : GCat.Pieces)
	{
		if (FString(All[P.Inst].Name).Contains(Name)) { return P.Inst; }
	}
	// Not a piece that wrecks (a fixture row, architecture): its row all the same (callers want its place).
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (FString(All[i].Name).Equals(Name, ESearchCase::IgnoreCase) && All[i].Index == Index) { return i; }
	}
	return -1;
}

int32 AntWreck::PieceOfComponent(UWorld* World, const UPrimitiveComponent* C)
{
	BuildCatalog(World);
	const int32* I = C ? GCat.ByComp.Find(C) : nullptr;
	return I ? *I : -1;
}

int32 AntWreck::ShakeRiders(AAntWreckBody* Carrier, const FVector& Vel, float Chance)
{
	if (!Carrier || !Carrier->HasAuthority())
	{
		return 0;
	}
	int32 N = 0;
	for (TActorIterator<AAntWreckBody> It(Carrier->GetWorld()); It; ++It)
	{
		AAntWreckBody* R = *It;
		if (R->RideOn != Carrier || FMath::FRand() >= Chance)
		{
			continue;
		}
		R->RideOn = nullptr;
		R->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		R->Root->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		for (UBoxComponent* Hb : R->Hull) { Hb->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); }
		const FVector V = Vel + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(0.f, 200.f));
		if (!R->Wake(V / UuPerM * R->Mass, R->GetActorLocation()))
		{
			R->Spill(V);
		}
		++N;
	}
	return N;
}

AntWreck::FOnWreckMess& AntWreck::OnMess()
{
	static FOnWreckMess D;
	return D;
}

AAntWreckBody* AntWreck::HitPiece(UWorld* World, int32 Inst, const FVector& Point, const FVector& Impulse, float Damage, AActor* Source)
{
	if (!World || World->GetNetMode() == NM_Client || !Enabled())
	{
		return nullptr;
	}
	const FPiece* P = PieceOf(World, Inst);
	if (!P)
	{
		return nullptr;
	}
	AAntWreckBody* B = BodyFor(World, Inst);
	if (!B)
	{
		AKitchenBuilder* Bld = BuilderOf(World);
		UStaticMeshComponent* C = Bld ? Bld->ArtComponent(Inst) : nullptr;
		if (!C)
		{
			return nullptr;
		}
		const FTransform T = C->GetComponentTransform();
		B = World->SpawnActorDeferred<AAntWreckBody>(AAntWreckBody::StaticClass(), FTransform(T.GetRotation(), T.GetLocation()), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!B)
		{
			return nullptr;
		}
		B->Inst = Inst;
		B->Seed = (int32)(GetTypeHash(Inst) * 2654435761u % 100000u) + 17;
		B->FinishSpawning(FTransform(T.GetRotation(), T.GetLocation()));
	}
	B->TakeHit(Point, Impulse, Damage, Source);
	return B;
}

int32 AntWreck::Hit(UWorld* World, const FVector& Point, float Radius, const FVector& Impulse, float Damage, AActor* Source, const TCHAR* Why)
{
	if (!World || World->GetNetMode() == NM_Client || !Enabled())
	{
		return 0;
	}
	BuildCatalog(World);
	int32 N = 0;
	const FVector Dir = Impulse.GetSafeNormal();
	const float I = (float)Impulse.Size();
	TSet<int32> Done;
	for (TActorIterator<AAntWreckBody> It(World); It; ++It)
	{
		AAntWreckBody* B = *It;
		if (B->IsBroken() || B->Inst < 0)
		{
			continue;
		}
		const float D = BoxDist(B->WorldBox(), Point);
		if (D < Radius)
		{
			const float F = FMath::Max(0.25f, 1.f - D / Radius);
			B->TakeHit(Point, Impulse * F, Damage * F, Source);
			Done.Add(B->Inst);
			++N;
		}
	}
	for (const FPiece& P : GCat.Pieces)
	{
		if (Done.Contains(P.Inst))
		{
			continue;
		}
		const float D = BoxDist(P.Box, Point);
		if (D < Radius)
		{
			const float F = FMath::Max(0.25f, 1.f - D / Radius);
			if (Damage * F >= 1.f || I * F > 0.3f * Class(P.Mat).WakeK * P.Mass)
			{
				HitPiece(World, P.Inst, Point, Impulse * F, Damage * F, Source);
				++N;
			}
		}
	}
	// Level v3 fixtures: a strike that loads them knocks them down (the shelf, the jars), a lying breakable one breaks.
	for (TActorIterator<AKitchenFixture> It(World); It; ++It)
	{
		AKitchenFixture* Fx = *It;
		const float D = Fx->DistanceTo(Point);
		if (D > Radius)
		{
			continue;
		}
		const float F = FMath::Max(0.25f, 1.f - D / Radius);
		if (Fx->State == AKitchenFixture::Fixed && I * F > 12.f)
		{
			Fx->PushDir = Dir.IsNearlyZero() ? FVector::ZeroVector : Dir.GetSafeNormal2D();
			Fx->Fall(Dir, nullptr, Why);
			++N;
		}
		else if (Fx->State == AKitchenFixture::Lying && Fx->IsBreakable() && Damage * F > 20.f)
		{
			Fx->BreakNow(nullptr);
			++N;
		}
	}
	// Food: knocked about; a heavy blow smashes it (the ants scavenge the pieces).
	for (TActorIterator<AKitchenLoot> It(World); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (L->bHeld || L->IsCarried())
		{
			continue;
		}
		const FVector To = L->GetActorLocation() - Point;
		if (To.Size() < Radius * 1.2f)
		{
			if (Damage > 90.f && L->CanBreak())
			{
				L->Shatter(Point);
			}
			else if (I > 2.f)
			{
				L->Kick((To.GetSafeNormal() + Dir * 0.5f).GetSafeNormal() * FMath::Min(1400.f, 120.f * I) + FVector(0.f, 0.f, 200.f), L->GetActorLocation());
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK hit %s at %s r=%.0f impulse %.1f damage %.0f: %d things (awake %d)"), Why ? Why : TEXT("-"), *Point.ToCompactString(), Radius, I, Damage, N, AAntWreckBody::NumAwake(World));
	return N;
}

void AntWreck::ResetAll(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	int32 N = 0;
	for (TActorIterator<AAntWreckBody> It(World); It; ++It) { It->Destroy(); ++N; }
	if (AAntWreckPiles* P = AAntWreckPiles::Get(World)) { P->Clear(); }
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK reset: %d bodies back in place"), N);
}

// =============================================================================================
// The body
// =============================================================================================

AAntWreckBody::AAntWreckBody()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(15.f);
	SetMinNetUpdateFrequency(1.f);
	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(10.f));
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Root->SetCanEverAffectNavigation(false);
	RootComponent = Root;
}

void AAntWreckBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntWreckBody, Inst);
	DOREPLIFETIME(AAntWreckBody, Seed);
	DOREPLIFETIME(AAntWreckBody, Stage);
	DOREPLIFETIME(AAntWreckBody, LostMask);
	DOREPLIFETIME(AAntWreckBody, Dents);
	DOREPLIFETIME(AAntWreckBody, Drops);
	DOREPLIFETIME(AAntWreckBody, Pose);
	DOREPLIFETIME(AAntWreckBody, bAwake);
	DOREPLIFETIME(AAntWreckBody, RideOn);
	DOREPLIFETIME(AAntWreckBody, RideRel);
	DOREPLIFETIME(AAntWreckBody, PropMesh);
	DOREPLIFETIME(AAntWreckBody, bClothGone);
}

float AAntWreckBody::NetNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

FString AAntWreckBody::PieceName() const
{
	const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
	return All.IsValidIndex(Inst) ? FString::Printf(TEXT("%s#%d"), All[Inst].Name, All[Inst].Index) : FString(TEXT("?"));
}

FBox AAntWreckBody::WorldBox() const
{
	return Mesh ? Mesh->Bounds.GetBox() : FBox(GetActorLocation() - FVector(50.f), GetActorLocation() + FVector(50.f));
}

float AAntWreckBody::TiltDeg() const
{
	const FVector Up = LeanBase.GetRotation().GetUpVector();
	const FVector Now = GetActorQuat().GetUpVector();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)FVector::DotProduct(Up, Now), -1.f, 1.f)));
}

int32 AAntWreckBody::NumAwake(UWorld* World)
{
	int32 N = 0;
	for (TActorIterator<AAntWreckBody> It(World); It; ++It) { N += It->bAwake ? 1 : 0; }
	return N;
}

void AAntWreckBody::BeginPlay()
{
	Super::BeginPlay();
	Setup();
}

void AAntWreckBody::OnRep_Setup() { Setup(); }

void AAntWreckBody::Setup()
{
	if (bSetUp || Inst < 0)
	{
		return;
	}
	AKitchenBuilder* B = BuilderOf(GetWorld());
	UStaticMeshComponent* Orig = B ? B->ArtComponent(Inst) : nullptr;
	if (!Orig || !Orig->GetStaticMesh())
	{
		return;
	}
	bSetUp = true;
	const AntWreck::FPiece* P = AntWreck::PieceOf(GetWorld(), Inst);
	Mat = P ? P->Mat : 0;
	Mass = P ? P->Mass : 1.f;
	bFixed = P ? P->bFixed : true;
	MaxHealth = P ? P->Hp : 50.f;
	Health = MaxHealth;
	const FTransform OT = Orig->GetComponentTransform();
	const FVector Scale = OT.GetScale3D();
	const TArray<FBox>& HullBoxes = AntWreckGeo::Hull(Orig->GetStaticMesh(), Scale);
	const FVector C0 = HullBoxes.Num() > 0 ? HullBoxes[0].GetCenter() : FVector::ZeroVector;
	// The actor stands at the centre of the first box (the simulating root has to be a shape): the mesh hangs off it.
	MeshRel = FTransform(FQuat::Identity, -(C0 * Scale), Scale);
	if (HasAuthority())
	{
		SetActorTransform(FTransform(OT.GetRotation(), OT.TransformPosition(C0)));
	}
	LeanBase = FTransform(OT.GetRotation(), OT.TransformPosition(C0));
	bLeanBaseSet = true;
	Root->SetBoxExtent((HullBoxes.Num() > 0 ? HullBoxes[0].GetExtent() : FVector(50.f)) * Scale.GetAbs());
	auto Physical = [](UPrimitiveComponent* C)
	{
		C->SetCollisionObjectType(ECC_PhysicsBody);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		C->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		C->SetCanEverAffectNavigation(false);
	};
	Physical(Root);
	for (int32 i = 1; i < HullBoxes.Num(); ++i)
	{
		UBoxComponent* Hb = NewObject<UBoxComponent>(this);
		Hb->SetupAttachment(Root);
		Hb->SetRelativeLocation((HullBoxes[i].GetCenter() - C0) * Scale);
		Hb->SetBoxExtent(HullBoxes[i].GetExtent() * Scale.GetAbs());
		Physical(Hb);
		Hb->RegisterComponent();
		Hull.Add(Hb);
	}
	Mesh = NewObject<UStaticMeshComponent>(this, TEXT("WreckMesh"));
	Mesh->SetNetAddressable();   // ants stand on it: their base is replicated relative to it
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Orig->GetStaticMesh());
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeTransform(MeshRel);
	for (int32 i = 0; i < Orig->GetNumMaterials(); ++i) { Mesh->SetMaterial(i, Orig->GetMaterial(i)); }
	Mesh->ComponentTags = Orig->ComponentTags;
	{
		// its visible state goes with it (wet, charred, dirty, damaged: AntState.h)
		const TArray<float>& Cpd = Orig->GetCustomPrimitiveData().Data;
		for (int32 i = 0; i < Cpd.Num(); ++i) { Mesh->SetCustomPrimitiveDataFloat(i, Cpd[i]); }
	}
	KitchenArt::SetSolid(Mesh, KitchenArt::ESolid::Moving);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);   // ants, the Human and the camera; the boxes are its physics
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->RegisterComponent();
	{
		// parts on pins (the bucket's handle) ride the body now
		const TArray<TObjectPtr<USceneComponent>> Kids = Orig->GetAttachChildren();
		for (USceneComponent* K : Kids)
		{
			if (UAntSwingComponent* Sw = Cast<UAntSwingComponent>(K)) { Sw->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform); Sw->Kick(60.f); }
		}
	}
	Chunks = NewObject<UProceduralMeshComponent>(this, TEXT("WreckChunks"));
	Chunks->SetupAttachment(Root);
	Chunks->SetRelativeTransform(MeshRel);
	Chunks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Chunks->SetCastShadow(true);
	Chunks->RegisterComponent();
	Chunks->SetVisibility(false);
	// The builder's static copy goes on every machine.
	Orig->SetVisibility(false);
	Orig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HasAuthority())
	{
		for (UBoxComponent* Hb : Hull) { Hb->WeldTo(Root); }
		Root->OnComponentHit.AddDynamic(this, &AAntWreckBody::OnPhysHit);
		Root->SetNotifyRigidBodyCollision(true);
		Pose.Loc = GetActorLocation();
		Pose.Rot = GetActorRotation();
		Pose.T = NetNow();
	}
	RebuildLook();
	OnRep_Drops();
}

void AAntWreckBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Inst >= 0 && GetWorld())
	{
		if (AKitchenBuilder* B = BuilderOf(GetWorld()))
		{
			if (UStaticMeshComponent* Orig = B->ArtComponent(Inst))
			{
				if (Mesh)
				{
					const TArray<TObjectPtr<USceneComponent>> Kids = Mesh->GetAttachChildren();
					for (USceneComponent* K : Kids)
					{
						if (UAntSwingComponent* Sw = Cast<UAntSwingComponent>(K)) { Sw->AttachToComponent(Orig, FAttachmentTransformRules::KeepRelativeTransform); }
					}
				}
				Orig->SetVisibility(true);
				Orig->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
		if (AAntWreckPiles* P = AAntWreckPiles::Get(GetWorld()))
		{
			if (UProceduralMeshComponent** Pile = P->Piles.Find(Inst))
			{
				if (*Pile) { (*Pile)->DestroyComponent(); }
				P->Piles.Remove(Inst);
			}
		}
	}
	for (UProceduralMeshComponent* F : DropFx) { if (F) { F->DestroyComponent(); } }
	DropFx.Reset();
	Super::EndPlay(Reason);
}

// ---- damage

void AAntWreckBody::TakeHit(const FVector& Point, const FVector& Impulse, float Damage, AActor* Source)
{
	if (!HasAuthority() || !bSetUp || IsBroken())
	{
		return;
	}
	const AntWreck::FClassDef& C = AntWreck::Class(Mat);
	const float Now = GetWorld()->GetTimeSeconds();
	const float I = (float)Impulse.Size();
	const FVector Dir = Impulse.GetSafeNormal();
	const uint8 Before = Stage;
	Health = FMath::Max(0.f, Health - Damage);
	// Dents where it was struck (metal, appliances, plastic, cloth squash; wood shows them too, a little).
	if (Damage > 4.f && Mesh)
	{
		const FTransform MT = Mesh->GetComponentTransform();
		const float Scl = FMath::Max(0.01f, (float)MT.GetScale3D().GetAbsMax());
		const float Soft = (Mat == AntWreck::Metal || Mat == AntWreck::Appliance || Mat == AntWreck::Cloth || Mat == AntWreck::Plastic) ? 1.f : 0.45f;
		FWreckDent D;
		D.At = MT.InverseTransformPosition(Point);
		D.Dir = MT.InverseTransformVectorNoScale(Dir.IsNearlyZero() ? FVector::DownVector : Dir);
		D.Radius = FMath::Clamp(60.f + Damage * 2.f, 80.f, 400.f) / Scl;
		D.Depth = FMath::Clamp(Damage * 0.6f, 5.f, 90.f) * Soft / Scl;
		if (Dents.Num() >= 6) { Dents.RemoveAt(0); }
		Dents.Add(D);
	}
	const float Frac = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	uint8 NewStage = Frac > 0.8f ? 0 : Frac > 0.55f ? 1 : Frac > 0.3f ? 2 : Frac > 0.f ? 3 : 4;
	if (NewStage >= 4 && !C.bBreaks)
	{
		NewStage = 3;
		Health = 1.f;
	}
	if (NewStage > Stage)
	{
		Stage = FMath::Min<uint8>(NewStage, 3);
		if (Stage >= 2 && Before < 2 && !bAwake && !bFixed && !RideOn)
		{
			AntAudio::Everyone(this, EAntSfx::Wobble, Point, 1.f);   // it groans and leans
			ApplyLean(Dir);
		}
		if (Stage >= 3 && Before < 3)
		{
			// A loose part: the chunk nearest the blow comes off.
			const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
			const TSharedPtr<const AntWreckGeo::FGeo> B0 = AntWreckGeo::Base(Mesh->GetStaticMesh());
			if (Cut.Num() > 2 && B0.IsValid() && !B0->bProxy && Mat != AntWreck::Appliance && Mat != AntWreck::Cloth && Mat != AntWreck::Metal && Mat != AntWreck::Paper)
			{
				const FTransform MT = Mesh->GetComponentTransform();
				int32 Best = 0; float BestD = 1.e12f;
				for (int32 c = 0; c < Cut.Num(); ++c)
				{
					const float D = (float)FVector::DistSquared(MT.TransformPosition(Cut[c]->Box.GetCenter()), Point);
					if (D < BestD) { BestD = D; Best = c; }
				}
				DropChunk(Best, Dir);
			}
		}
		if (NewStage >= 4)
		{
			BreakApart(Dir);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s (%s, %.1f kg): stage %d -> %d, health %.0f/%.0f"), *PieceName(), AntWreck::Class(Mat).Name, Mass, Before, IsBroken() ? 4 : Stage, Health, MaxHealth);
	}
	if (!IsBroken())
	{
		OnRep_Look();
		// Loaded above its threshold: it wakes (tips, slides, topples).
		if (I > C.WakeK * Mass)
		{
			Wake(Impulse, Point);
		}
	}
	if (Now - LastHitAt > 0.25f && (Damage > 10.f || I > Mass))
	{
		LastHitAt = Now;
		AntAudio::Everyone(this, AntAudio::ForWreck(Mat, false), Point, FMath::Clamp(0.5f + Damage / 60.f, 0.5f, 1.5f));
		if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
		{
			GM->MakeNoise(Point, FMath::Clamp(C.Noise * (0.3f + Damage / 120.f), 0.1f, 1.f), AntNoise::Crash, this, Cast<AAntCharacter>(Source));
		}
	}
}

void AAntWreckBody::ApplyLean(const FVector& InDir)
{
	// It bends where it was hurt: leans away from the blow about its bottom edge on that side (5-9 degrees), like a chair with a cracked leg.
	FVector Dir = InDir.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		FRandomStream R(Seed);
		Dir = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	}
	const FBox B = WorldBox();
	const FVector Pivot(B.GetCenter().X + Dir.X * B.GetExtent().X, B.GetCenter().Y + Dir.Y * B.GetExtent().Y, B.Min.Z);
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	const float Deg = Mat == AntWreck::Cloth ? 3.f : FMath::Clamp(9.f - Mass * 0.05f, 4.f, 9.f);
	const FQuat Q(Axis, FMath::DegreesToRadians(Deg));
	const FVector L = Pivot + Q.RotateVector(GetActorLocation() - Pivot);
	SetActorLocationAndRotation(L, Q * GetActorQuat(), false, nullptr, ETeleportType::TeleportPhysics);
	PushPose();
	KnockFixtures(Dir, TEXT("lean"));
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s leans %.0f deg toward (%.2f, %.2f)"), *PieceName(), Deg, Dir.X, Dir.Y);
}

bool AAntWreckBody::Wake(const FVector& Impulse, const FVector& At, bool bForce)
{
	if (!HasAuthority() || !bSetUp || IsBroken())
	{
		return false;
	}
	if (bAwake)
	{
		Root->AddImpulseAtLocation(Impulse * AntWreck::UuPerM, At);
		return true;
	}
	if ((bFixed && !bForce) || (!bForce && NumAwake(GetWorld()) >= AntWreck::MaxAwake))
	{
		return false;
	}
	if (RideOn)
	{
		RideOn = nullptr;
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	bAwake = true;
	AwakeSince = GetWorld()->GetTimeSeconds();
	StillFor = 0.f;
	// What rests on it or stands inside it goes with it: it rides until this one tips, then falls on its own.
	const FBox Me = WorldBox();
	const FBox Xy = Me.ExpandBy(FVector(-0.05f * Me.GetExtent().X, -0.05f * Me.GetExtent().Y, 0.f));
	int32 Riders = 0;
	for (const AntWreck::FPiece& P : AntWreck::Pieces(GetWorld()))
	{
		if (P.Inst == Inst || P.bFixed || P.Mass >= Mass)
		{
			continue;
		}
		AAntWreckBody* Other = BodyFor(GetWorld(), P.Inst);
		const FBox Ob = Other ? Other->WorldBox() : P.Box;
		const FVector Oc = Ob.GetCenter();
		if (Other && (Other->bAwake || Other->RideOn || Other->IsBroken()))
		{
			continue;
		}
		if (Oc.X < Xy.Min.X || Oc.X > Xy.Max.X || Oc.Y < Xy.Min.Y || Oc.Y > Xy.Max.Y || Ob.Min.Z < Me.Min.Z + 15.f || Ob.Min.Z > Me.Max.Z + 40.f)
		{
			continue;
		}
		if (!Other)
		{
			Other = AntWreck::HitPiece(GetWorld(), P.Inst, Oc, FVector::ZeroVector, 0.f, nullptr);
		}
		if (Other && Other->bSetUp && !Other->bAwake)
		{
			Other->RideOn = this;
			Other->RideRel = Other->GetActorTransform().GetRelativeTransform(GetActorTransform());
			Other->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Other->Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // a kinematic rider would shove its carrier about
			for (UBoxComponent* Hb : Other->Hull) { Hb->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
			++Riders;
		}
	}
	// No box of it may start under the floor (a box a little fatter than a thin leg): lift it clear first.
	{
		float Low = 1.e9f;
		Low = FMath::Min(Low, (float)Root->Bounds.GetBox().Min.Z);
		for (UBoxComponent* Hb : Hull) { Low = FMath::Min(Low, (float)Hb->Bounds.GetBox().Min.Z); }
		TArray<const AActor*> Ign; Ign.Add(this);
		const FVector C = WorldBox().GetCenter();
		const float Floor = FloorUnder(GetWorld(), FVector(C.X, C.Y, WorldBox().Min.Z + 20.f), Ign, Low);
		if (Low < Floor + 1.f && Floor - Low < 200.f)
		{
			AddActorWorldOffset(FVector(0.f, 0.f, Floor - Low + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK wake %s: boxes from z %.0f, floor %.0f, %d boxes"), *PieceName(), Low, Floor, 1 + Hull.Num());
	}
	// Its boxes only approximate it: where one starts inside a neighbour it eases out instead of being fired off.
	Root->BodyInstance.SetMaxDepenetrationVelocity(60.f);
	KnockFixtures(Impulse, TEXT("wake"));
	Root->SetSimulatePhysics(true);
	Root->SetMassOverrideInKg(NAME_None, Mass, true);
	Root->SetLinearDamping(0.08f);
	Root->SetAngularDamping(0.7f);
	Root->SetUseCCD(Mass < 3.f);
	Root->WakeAllRigidBodies();
	// A shove, not a cannon: the change of speed is capped (2 m/s at the point struck).
	FVector J = Impulse;
	const float Dv = (float)J.Size() / FMath::Max(0.05f, Mass);
	if (Dv > 2.f) { J *= 2.f / Dv; }
	Root->AddImpulseAtLocation(J * AntWreck::UuPerM, At);
	SetNetUpdateFrequency(15.f);
	PushPose();
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK wake %s (%s %.1f kg), impulse %.1f, riders %d, awake now %d"), *PieceName(), AntWreck::Class(Mat).Name, Mass, (float)Impulse.Size(), Riders, NumAwake(GetWorld()));
	return true;
}

void AAntWreckBody::Sleep()
{
	if (!HasAuthority() || !bAwake)
	{
		return;
	}
	bAwake = false;
	Root->SetSimulatePhysics(false);
	for (UBoxComponent* Hb : Hull) { Hb->SetRelativeTransform(Hb->GetRelativeTransform()); }
	LeanBase = GetActorTransform();
	SpillRiders(FVector::ZeroVector);
	PushPose();
	SetNetUpdateFrequency(2.f);
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK sleep %s at %s, tilt %.0f, after %.1f s"), *PieceName(), *GetActorLocation().ToCompactString(), TiltDeg(), GetWorld()->GetTimeSeconds() - AwakeSince);
}

void AAntWreckBody::SpillRiders(const FVector& Vel)
{
	for (TActorIterator<AAntWreckBody> It(GetWorld()); It; ++It)
	{
		AAntWreckBody* R = *It;
		if (R->RideOn != this)
		{
			continue;
		}
		R->RideOn = nullptr;
		R->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		R->LeanBase = R->GetActorTransform();
		R->Root->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		for (UBoxComponent* Hb : R->Hull) { Hb->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); }
		R->PushPose();
		if (Vel.IsNearlyZero())
		{
			continue;   // it settled with them still on it
		}
		const FVector V = Vel + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f);
		if (!R->Wake(V / AntWreck::UuPerM * R->Mass, R->GetActorLocation()))
		{
			R->Spill(V);
		}
	}
}

void AAntWreckBody::OnPhysHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bAwake)
	{
		return;
	}
	const float I = (float)NormalImpulse.Size() / AntWreck::UuPerM;   // kg m/s
	const float Dv = I / FMath::Max(0.05f, Mass);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Dv < 1.0f || I < 1.f || Dv > 14.f || Now - AwakeSince < 0.15f)
	{
		return;   // (a solver kick out of an overlap is not a crash: nothing in a kitchen hits at 50 km/h)
	}
	TWeakObjectPtr<AActor> Key(Other);
	if (const float* Last = HitCool.Find(Key); Last && Now - *Last < 0.3f)
	{
		return;
	}
	HitCool.Add(Key, Now);
	const FVector At = Hit.ImpactPoint;
	const FVector Into = -NormalImpulse.GetSafeNormal();
	// It hurts itself landing (glass and china from a height break) and what it lands on.
	const float SelfDmg = (Mat == AntWreck::Glass || Mat == AntWreck::China) ? Dv * 6.f : FMath::Max(0.f, Dv - 4.f) * Mass * 0.08f;
	if (AAntWreckBody* B = Cast<AAntWreckBody>(Other))
	{
		if (Mass >= 0.25f * B->Mass) { B->TakeHit(At, Into * I * 0.7f, I * 0.9f, this); }   // (a book does not break the bookcase it fell out of)
	}
	else if (const int32* OtherInst = GCat.ByComp.Find(OtherComp))
	{
		AntWreck::HitPiece(GetWorld(), *OtherInst, At, Into * I * 0.7f, I * 0.9f, this);
	}
	else if (AKitchenFixture* Fx = Cast<AKitchenFixture>(Other))
	{
		if (Fx->State == AKitchenFixture::Fixed && I > 12.f) { Fx->Fall(Into, nullptr, TEXT("wreck")); }
	}
	if (Mass > 3.f && Dv > 1.5f)
	{
		Crush(At, FMath::Clamp((float)WorldBox().GetExtent().Size2D() * 0.6f, 150.f, 900.f), Mass);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK impact %s on %s: %.1f kg m/s (dv %.1f m/s)"), *PieceName(), Other ? *Other->GetName() : TEXT("-"), I, Dv);
	TakeHit(At, FVector::ZeroVector, SelfDmg, this);
}

void AAntWreckBody::KnockFixtures(const FVector& InDir, const TCHAR* Why)
{
	const FBox Me = WorldBox();
	const FVector Dir = InDir.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : InDir.GetSafeNormal2D();
	for (TActorIterator<AKitchenFixture> It(GetWorld()); It; ++It)
	{
		AKitchenFixture* Fx = *It;
		const FVector L = Fx->GetActorLocation();
		if (Fx->FixId() == TEXT("tablecloth") && FCString::Strcmp(Why, TEXT("break")) == 0 && !Fx->IsHidden() && Fx->DistanceTo(Me.GetCenter()) < (float)Me.GetExtent().Size2D())
		{
			Fx->SetActorHiddenInGame(true);   // the cloth goes with the broken table
			Fx->SetActorEnableCollision(false);
			UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s: the tablecloth goes with it"), *PieceName());
			continue;
		}
		if (Fx->State != AKitchenFixture::Fixed || L.X < Me.Min.X || L.X > Me.Max.X || L.Y < Me.Min.Y || L.Y > Me.Max.Y || L.Z < Me.Min.Z - 100.f || L.Z > Me.Max.Z + 600.f)
		{
			continue;
		}
		Fx->PushDir = Dir;
		if (Fx->FixId() == TEXT("tablecloth"))
		{
			// The cloth over the table: it stays while the table only leans or slides; when the table breaks it goes with it
			// (its own fall would drop the whole drape on the floor as a box).
			if (FCString::Strcmp(Why, TEXT("break")) != 0) { continue; }
			Fx->SetActorHiddenInGame(true);
			Fx->SetActorEnableCollision(false);
		}
		else { Fx->Fall(Dir, nullptr, Why); }
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s takes the fixture %s with it (%s)"), *PieceName(), *Fx->FixId(), Why);
	}
	// Static cloth over it (a variant's long tablecloth to the floor): it slides off with it (hidden on every machine via the cloth flag).
	if (!bClothGone)
	{
		bClothGone = true;
		ClothOff();
	}
}

void AAntWreckBody::OnRep_Cloth()
{
	if (bClothGone) { ClothOff(); }
}

void AAntWreckBody::ClothOff()
{
	const FBox Me = WorldBox().ExpandBy(60.f);
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* C = *It;
		if (!C || C->GetWorld() != GetWorld() || !C->IsVisible() || !C->GetStaticMesh() || !C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_V3_TableclothLong")))
		{
			continue;
		}
		const FVector L = C->Bounds.Origin;
		if (L.X > Me.Min.X && L.X < Me.Max.X && L.Y > Me.Min.Y && L.Y < Me.Max.Y)
		{
			C->SetVisibility(false);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s: the long tablecloth slides off"), *PieceName());
		}
	}
}

void AAntWreckBody::Crush(const FVector& At, float Radius, float Kg)
{
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->KillInSphere(At, Radius);
	}
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && FVector::Dist(Ant->GetActorLocation(), At) < Radius)
		{
			UGameplayStatics::ApplyDamage(Ant, FMath::Clamp(Kg * 25.f, 30.f, 400.f), nullptr, this, UDamageType::StaticClass());
			UE_LOG(LogTemp, Display, TEXT("ANTWRECK %s crushed %s"), *PieceName(), *Ant->GetDisplayName());
		}
	}
}

// ---- looks (every machine, from the replicated stage, lost chunks and dents)

void AAntWreckBody::OnRep_Look()
{
	RebuildLook();
}

void AAntWreckBody::RebuildLook()
{
	if (!bSetUp || !Mesh || !Chunks)
	{
		return;
	}
	if (LookStage == Stage && LookLost == LostMask && LookDents == Dents.Num())
	{
		return;
	}
	LookStage = Stage;
	LookLost = LostMask;
	LookDents = Dents.Num();
	if (IsBroken())
	{
		const TSharedPtr<const AntWreckGeo::FGeo> B0 = AntWreckGeo::Base(Mesh->GetStaticMesh());
		Mesh->SetVisibility(B0.IsValid() && B0->bProxy);   // a stand-in collapses whole (BreakApart) instead of flying apart
		Chunks->SetVisibility(false);
		return;
	}
	if (Stage == 0 && Dents.Num() == 0)
	{
		Mesh->SetVisibility(true);
		Chunks->SetVisibility(false);
		return;
	}
	{
		const TSharedPtr<const AntWreckGeo::FGeo> B0 = AntWreckGeo::Base(Mesh->GetStaticMesh());
		if (!B0.IsValid() || B0->bProxy)
		{
			Mesh->SetVisibility(true);   // (its collision is a stand-in: the drawn model stays, it only leans)
			Chunks->SetVisibility(false);
			return;
		}
	}
	// The assembled chunks of its seeded cut: they part along the cracks as it gets worse, the dents pressed into them.
	const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
	if (Cut.Num() == 0)
	{
		return;
	}
	const bool bCracks = Mat != AntWreck::Metal && Mat != AntWreck::Appliance && Mat != AntWreck::Cloth;
	FBox All(ForceInit);
	for (const auto& C : Cut) { All += C->Box; }
	const float Size = (float)All.GetSize().GetMax();
	const float Gap = !bCracks ? 0.f : Stage <= 1 ? 0.004f * Size : Stage == 2 ? 0.010f * Size : 0.016f * Size;
	Chunks->ClearAllMeshSections();
	int32 S = 0;
	UMaterialInterface* Inside = Mesh->GetMaterial(0);
	for (int32 c = 0; c < Cut.Num(); ++c)
	{
		if (LostMask & (1 << c))
		{
			continue;
		}
		FVector Off = Cut[c]->Box.GetCenter() - All.GetCenter();
		Off.Z = FMath::Min(0.f, (float)Off.Z);   // (sideways and down, never up through what lies on it)
		Off = Off.GetSafeNormal() * Gap;
		for (int32 k = 0; k < Cut[c]->Sections.Num(); ++k)
		{
			FProcMeshSection Sec = Cut[c]->Sections[k];
			for (const FWreckDent& D : Dents) { AntWreckGeo::Dent(Sec, D.At, D.Dir, D.Radius, D.Depth); }
			if (!Off.IsNearlyZero())
			{
				for (FProcMeshVertex& V : Sec.ProcVertexBuffer) { V.Position += Off; }
				Sec.SectionLocalBox = Sec.SectionLocalBox.ShiftBy(Off);
			}
			Chunks->SetProcMeshSection(S, Sec);
			const int32 Slot = Cut[c]->Slot[k];
			Chunks->SetMaterial(S, Slot >= 0 && Slot < Mesh->GetNumMaterials() ? Mesh->GetMaterial(Slot) : Cut[c]->BreakMat ? Cut[c]->BreakMat : Inside);
			++S;
		}
	}
	{
		const TArray<float>& Cpd = Mesh->GetCustomPrimitiveData().Data;   // the chunks keep its state (wet, charred: AntState.h)
		for (int32 i = 0; i < Cpd.Num() && i < 4; ++i) { Chunks->SetCustomPrimitiveDataFloat(i, Cpd[i]); }
	}
	Mesh->SetVisibility(false);
	Chunks->SetVisibility(true);
}

// ---- things falling along a parabola (chunks that came off, whole pieces spilled), the same on every machine

void AAntWreckBody::DropChunk(int32 Chunk, const FVector& Dir)
{
	if (!HasAuthority() || Chunk < 0 || Chunk >= 16 || (LostMask & (1 << Chunk)))
	{
		return;
	}
	const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
	if (!Cut.IsValidIndex(Chunk))
	{
		return;
	}
	LostMask |= (1 << Chunk);
	FRandomStream R(Seed + 31 * Chunk + 7 * Drops.Num());
	const FTransform MT = Mesh->GetComponentTransform();
	const FVector Ctr = MT.TransformPosition(Cut[Chunk]->Box.GetCenter());
	FVector Out = (Ctr - WorldBox().GetCenter()).GetSafeNormal2D() + Dir.GetSafeNormal2D() * 0.8f;
	if (Out.IsNearlyZero()) { Out = R.VRand().GetSafeNormal2D(); }
	const FVector Land2 = Ctr + Out.GetSafeNormal2D() * R.FRandRange(120.f, 420.f);
	TArray<const AActor*> Ign; Ign.Add(this);
	for (TActorIterator<AAntWreckBody> It(GetWorld()); It; ++It) { if (It->RideOn == this || It->bAwake) { Ign.Add(*It); } }
	const float FloorZ = FloorUnder(GetWorld(), FVector(Land2.X, Land2.Y, Ctr.Z + 50.f), Ign, (float)WorldBox().Min.Z);
	// It lies on its broadest side (its thinnest axis up), a random yaw, a few degrees of tilt.
	const FVector Ext = Cut[Chunk]->Box.GetExtent() * MT.GetScale3D().GetAbs();
	const int32 Thin = Ext.X <= Ext.Y && Ext.X <= Ext.Z ? 0 : Ext.Y <= Ext.Z ? 1 : 2;
	const FQuat Flat = Thin == 0 ? FQuat(FVector::RightVector, -HALF_PI) : Thin == 1 ? FQuat(FVector::ForwardVector, HALF_PI) : FQuat::Identity;
	const FQuat Rot = FQuat(FVector::UpVector, R.FRandRange(-PI, PI)) * FQuat(R.VRand().GetSafeNormal2D(), FMath::DegreesToRadians(R.FRandRange(0.f, 8.f))) * Flat;
	const float Half = (float)Ext.GetMin();
	const FVector LandCtr(Land2.X, Land2.Y, FloorZ + Half + 2.f);
	FWreckDrop D;
	D.Chunk = (uint8)Chunk;
	D.Start = NetNow();
	D.From = MT;
	D.To = FTransform(Rot, LandCtr - Rot.RotateVector(Cut[Chunk]->Box.GetCenter() * MT.GetScale3D()), MT.GetScale3D());
	D.Time = FMath::Clamp(FMath::Sqrt(2.f * FMath::Max(50.f, (float)(Ctr.Z - FloorZ)) / AntWreck::Gravity) + 0.1f, 0.25f, 1.4f);
	Drops.Add(D);
	OnRep_Look();
	OnRep_Drops();
}

void AAntWreckBody::Spill(const FVector& Velocity)
{
	if (!HasAuthority() || IsBroken())
	{
		return;
	}
	if (bAwake) { bAwake = false; Root->SetSimulatePhysics(false); }
	FRandomStream R(Seed + 991 + Drops.Num());
	const FVector From = GetActorLocation();
	const FVector Flat = FVector(Velocity.X, Velocity.Y, 0.f);
	const FVector Land2 = From + (Flat.IsNearlyZero() ? R.VRand().GetSafeNormal2D() * 300.f : Flat * 0.35f) + R.VRand().GetSafeNormal2D() * 80.f;
	TArray<const AActor*> Ign; Ign.Add(this);
	const float FloorZ = FloorUnder(GetWorld(), FVector(Land2.X, Land2.Y, WorldBox().Min.Z + 30.f), Ign, (float)WorldBox().Min.Z);
	const FBox B = WorldBox();
	const FQuat Rot = FQuat(FVector::UpVector, R.FRandRange(-PI, PI)) * FQuat(FVector::ForwardVector, B.GetSize().Z > B.GetSize().GetMax() * 0.8f ? HALF_PI : 0.f) * GetActorQuat();
	FWreckDrop D;
	D.Chunk = 255;
	D.Start = NetNow();
	D.From = GetActorTransform();
	D.To = FTransform(Rot, FVector(Land2.X, Land2.Y, FloorZ + (float)B.GetExtent().GetMin() * 0.9f));
	D.Time = FMath::Clamp(FMath::Sqrt(2.f * FMath::Max(50.f, (float)(B.Min.Z - FloorZ)) / AntWreck::Gravity) + 0.1f, 0.25f, 1.4f);
	Drops.Add(D);
	OnRep_Drops();
}

void AAntWreckBody::BreakApart(const FVector& Dir)
{
	if (!HasAuthority() || IsBroken())
	{
		return;
	}
	KnockFixtures(Dir, TEXT("break"));
	const TSharedPtr<const AntWreckGeo::FGeo> B0 = AntWreckGeo::Base(Mesh->GetStaticMesh());
	const bool bCollapse = B0.IsValid() && B0->bProxy;
	const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
	if (bCollapse)
	{
		// Its legs give: it goes down by half its height and tips 10 degrees toward the blow (the legs end up under the floor).
		FVector D2 = Dir.GetSafeNormal2D();
		if (D2.IsNearlyZero()) { D2 = FVector::ForwardVector; }
		const FBox Bx = WorldBox();
		const FQuat Tilt(FVector::CrossProduct(FVector::UpVector, D2).GetSafeNormal(), FMath::DegreesToRadians(10.f));
		FWreckDrop D;
		D.Chunk = 255;
		D.Start = NetNow();
		D.From = GetActorTransform();
		D.To = FTransform(Tilt * GetActorQuat(), GetActorLocation() - FVector(0.f, 0.f, Bx.GetSize().Z * 0.5f) + D2 * 120.f);
		D.Time = 0.35f;
		Drops.Add(D);
		OnRep_Drops();
	}
	for (int32 c = 0; !bCollapse && c < Cut.Num() && c < 16; ++c)
	{
		DropChunk(c, Dir);   // (before the riders let go: the chunks land on the floor, not on a book still up in the shelf)
	}
	SpillRiders(Dir * 300.f + FVector(0.f, 0.f, 100.f));
	if (bAwake) { bAwake = false; Root->SetSimulatePhysics(false); }
	Stage = 4;
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (UBoxComponent* Hb : Hull) { Hb->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	Mesh->SetCollisionEnabled(bCollapse ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	OnRep_Look();
	const FVector At = WorldBox().GetCenter();
	AntAudio::Everyone(this, AntAudio::ForWreck(Mat, true), At, 1.4f);
	AntWreck::OnMess().Broadcast(GetWorld(), At, (float)WorldBox().GetExtent().Size2D(), Mat);
	if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
	{
		GM->MakeNoise(At, AntWreck::Class(Mat).Noise, AntNoise::Crash, this, nullptr);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWRECK broke %s (%s) into %d chunks"), *PieceName(), AntWreck::Class(Mat).Name, Cut.Num());
}

void AAntWreckBody::OnRep_Drops()
{
	if (!bSetUp)
	{
		return;
	}
	while (DropLanded.Num() < Drops.Num()) { DropLanded.Add(false); DropFx.Add(nullptr); }
	const float Now = NetNow();
	for (int32 i = 0; i < Drops.Num(); ++i)
	{
		const FWreckDrop& D = Drops[i];
		if (DropLanded[i] || D.Chunk == 255 || DropFx[i])
		{
			continue;
		}
		if (Now >= D.Start + D.Time)
		{
			LandDrop(i);
			continue;
		}
		const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
		if (!Cut.IsValidIndex(D.Chunk))
		{
			DropLanded[i] = true;
			continue;
		}
		AAntWreckPiles* Pl = AAntWreckPiles::Get(GetWorld());
		UProceduralMeshComponent* F = NewObject<UProceduralMeshComponent>(Pl ? (AActor*)Pl : (AActor*)this);
		F->SetupAttachment(Pl ? Pl->Origin : Root);
		F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		F->RegisterComponent();
		const TSharedPtr<const AntWreckGeo::FGeo>& G = Cut[D.Chunk];
		for (int32 k = 0; k < G->Sections.Num(); ++k)
		{
			F->SetProcMeshSection(k, G->Sections[k]);
			const int32 Slot = G->Slot[k];
			F->SetMaterial(k, Slot >= 0 && Slot < Mesh->GetNumMaterials() ? Mesh->GetMaterial(Slot) : G->BreakMat ? G->BreakMat : Mesh->GetMaterial(0));
		}
		F->SetWorldTransform(D.From);
		DropFx[i] = F;
	}
}

void AAntWreckBody::TickDrops(float Dt)
{
	const float Now = NetNow();
	for (int32 i = 0; i < Drops.Num() && i < DropLanded.Num(); ++i)
	{
		if (DropLanded[i])
		{
			continue;
		}
		const FWreckDrop& D = Drops[i];
		const float A = FMath::Clamp((Now - D.Start) / FMath::Max(0.05f, D.Time), 0.f, 1.f);
		// Parabola: up a little, then down; the turn eases in.
		const FVector P0 = D.From.GetLocation(), P1 = D.To.GetLocation();
		FVector P = FMath::Lerp(P0, P1, A);
		P.Z += FMath::Max(60.f, (float)FMath::Abs(P0.Z - P1.Z) * 0.15f) * 4.f * A * (1.f - A);
		const FQuat Q = FQuat::Slerp(D.From.GetRotation(), D.To.GetRotation(), FMath::Min(1.f, A * 1.2f));
		if (D.Chunk == 255)
		{
			SetActorLocationAndRotation(P, Q, false, nullptr, ETeleportType::TeleportPhysics);
		}
		else if (DropFx.IsValidIndex(i) && DropFx[i])
		{
			DropFx[i]->SetWorldTransform(FTransform(Q, P, D.From.GetScale3D()));
		}
		if (A >= 1.f)
		{
			LandDrop(i);
		}
	}
}

void AAntWreckBody::LandDrop(int32 i)
{
	if (!Drops.IsValidIndex(i) || DropLanded[i])
	{
		return;
	}
	DropLanded[i] = true;
	const FWreckDrop& D = Drops[i];
	if (D.Chunk == 255)
	{
		SetActorTransform(D.To, false, nullptr, ETeleportType::TeleportPhysics);
		LeanBase = D.To;
		if (HasAuthority())
		{
			PushPose();
			const float H = (float)(D.From.GetLocation().Z - D.To.GetLocation().Z);
			const float V = FMath::Sqrt(2.f * AntWreck::Gravity * FMath::Max(0.f, H)) / AntWreck::UuPerM;
			if ((Mat == AntWreck::Glass || Mat == AntWreck::China) && H > 450.f)
			{
				Health = 0.f;
				BreakApart(FVector::DownVector);
			}
			else if (Mass > 3.f && Mass * V > 3.f)
			{
				AntWreck::Hit(GetWorld(), D.To.GetLocation(), FMath::Clamp((float)WorldBox().GetExtent().Size2D(), 120.f, 600.f), FVector(0.f, 0.f, -Mass * V * 0.5f), Mass * V * 0.5f, this, TEXT("fall"));
			}
			if (Mass > 3.f && V > 2.f) { Crush(D.To.GetLocation(), FMath::Clamp((float)WorldBox().GetExtent().Size2D() * 0.6f, 150.f, 900.f), Mass); }
		}
		return;
	}
	// A chunk: it joins the merged debris pile of this piece.
	const TArray<TSharedPtr<const AntWreckGeo::FGeo>>& Cut = AntWreckGeo::Cut(this, Mesh->GetStaticMesh(), Seed, AntWreckGeo::ChunkCount((float)WorldBox().GetSize().GetMax()));
	if (Cut.IsValidIndex(D.Chunk))
	{
		TArray<UMaterialInterface*> Mats;
		for (int32 Slot : Cut[D.Chunk]->Slot) { Mats.Add(Slot >= 0 && Slot < Mesh->GetNumMaterials() ? Mesh->GetMaterial(Slot) : Mesh->GetMaterial(0)); }
		if (AAntWreckPiles* Pl = AAntWreckPiles::Get(GetWorld()))
		{
			Pl->AddToPile(Inst, Cut[D.Chunk]->Sections, Mats, D.To);
		}
	}
	if (DropFx.IsValidIndex(i) && DropFx[i])
	{
		DropFx[i]->DestroyComponent();
		DropFx[i] = nullptr;
	}
}

// ---- the pose: simulated on the server, sent at 15 Hz while it moves, smoothed on clients

void AAntWreckBody::PushPose()
{
	if (!HasAuthority())
	{
		return;
	}
	Pose.Loc = GetActorLocation();
	Pose.Rot = GetActorRotation();
	Pose.T = NetNow();
	++Pose.Seq;
	ForceNetUpdate();
}

void AAntWreckBody::OnRep_Pose()
{
	if (HasAuthority())
	{
		return;
	}
	if (RideOn && !bWasRiding)
	{
		AttachToActor(RideOn, FAttachmentTransformRules::KeepWorldTransform);
		SetActorRelativeTransform(RideRel);
		bWasRiding = true;
		return;
	}
	if (!RideOn && bWasRiding)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bWasRiding = false;
	}
	if (!bHavePrev)
	{
		CurPose.Loc = GetActorLocation();
		CurPose.Rot = GetActorRotation();
		CurPose.T = Pose.T - 0.066f;
		bHavePrev = true;
	}
	PrevPose = CurPose;
	CurPose = Pose;
}

void AAntWreckBody::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bSetUp)
	{
		Setup();
		return;
	}
	TickDrops(Dt);
	if (Mesh && Dt > 0.f && Mesh->ComponentHasTag(TEXT("HeroArt")))
	{
		// hero props: what hangs loose (the mop's yarn, the broom's bristles: vertex colour R) sways while it moves (M_Hero, custom data 4)
		const FVector L = Mesh->GetComponentLocation();
		const float Speed = bSwayLast ? (float)FVector::Dist(L, SwayLast) / Dt : 0.f;
		SwayLast = L; bSwayLast = true;
		const float Target = FMath::Clamp(Speed / 1500.f, 0.f, 1.f);
		Sway = Target > Sway ? FMath::FInterpTo(Sway, Target, Dt, 12.f) : FMath::FInterpTo(Sway, 0.f, Dt, 0.8f);
		Mesh->SetCustomPrimitiveDataFloat(4, Sway < 0.01f ? 0.f : Sway);
	}
	if (HasAuthority())
	{
		ServerPhysicsTick(Dt);
		return;
	}
	// Client: interpolate 100 ms behind the server between the last two poses it sent.
	if (RideOn || !bHavePrev)
	{
		return;
	}
	for (int32 i = 0; i < Drops.Num() && i < DropLanded.Num(); ++i)
	{
		if (Drops[i].Chunk == 255 && !DropLanded[i]) { return; }   // the parabola owns the pose
	}
	const float RenderT = NetNow() - 0.1f;
	if (RenderT >= CurPose.T)
	{
		const FVector L = FMath::VInterpTo(GetActorLocation(), FVector(CurPose.Loc), Dt, 18.f);
		const FQuat Q = FQuat::Slerp(GetActorQuat(), CurPose.Rot.Quaternion(), FMath::Min(1.f, Dt * 18.f));
		SetActorLocationAndRotation(L, Q);
		return;
	}
	const float A = FMath::Clamp((RenderT - PrevPose.T) / FMath::Max(0.01f, CurPose.T - PrevPose.T), 0.f, 1.f);
	SetActorLocationAndRotation(FMath::Lerp(FVector(PrevPose.Loc), FVector(CurPose.Loc), A), FQuat::Slerp(PrevPose.Rot.Quaternion(), CurPose.Rot.Quaternion(), A));
}

void AAntWreckBody::ServerPhysicsTick(float Dt)
{
	if (!bAwake)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// The engine's gravity is far too weak at this scale: the rest of real gravity (scaled down a little for weight on screen).
	Root->AddForce(FVector(0.f, 0.f, -(AntWreck::Gravity + GetWorld()->GetGravityZ())), NAME_None, true);
	FVector V = Root->GetPhysicsLinearVelocity();
	FVector W = Root->GetPhysicsAngularVelocityInRadians();
	// Capped: a falling bookcase, not a rocket (9 m/s, 3 turns a second).
	if (V.Size() > 17000.f) { V = V.GetSafeNormal() * 17000.f; Root->SetPhysicsLinearVelocity(V); }
	if (W.Size() > 18.f) { W = W.GetSafeNormal() * 18.f; Root->SetPhysicsAngularVelocityInRadians(W); }
	if (TiltDeg() > 28.f || V.Size() > 900.f)
	{
		SpillRiders(V);
	}
	if (V.Size() < 60.f && W.Size() < 0.5f) { StillFor += Dt; } else { StillFor = 0.f; }
	const float Age = Now - AwakeSince;
	if ((StillFor > 0.6f && Age > 0.5f) || Age > 12.f || GetActorLocation().Z < -3000.f)
	{
		Sleep();
		return;
	}
	if (Now >= NextPoseAt)
	{
		NextPoseAt = Now + 1.f / 15.f;
		PushPose();
	}
}

// =============================================================================================
// Debris piles: every fallen chunk of one piece merged into one procedural mesh (local, deterministic on every machine)
// =============================================================================================

AAntWreckPiles::AAntWreckPiles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	RootComponent = Origin;
}

AAntWreckPiles* AAntWreckPiles::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntWreckPiles> It(World); It; ++It) { return *It; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAntWreckPiles>(AAntWreckPiles::StaticClass(), FTransform::Identity, SP);
}

void AAntWreckPiles::AddToPile(int32 Key, const TArray<FProcMeshSection>& Sections, const TArray<UMaterialInterface*>& Mats, const FTransform& Where)
{
	UProceduralMeshComponent*& P = Piles.FindOrAdd(Key);
	if (!P)
	{
		P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(Origin);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->RegisterComponent();
	}
	// Merge by material: one section per material in the pile, the chunk's triangles appended in world space.
	for (int32 k = 0; k < Sections.Num(); ++k)
	{
		UMaterialInterface* M = Mats.IsValidIndex(k) ? Mats[k] : nullptr;
		int32 Target = -1;
		for (int32 s = 0; s < P->GetNumSections(); ++s)
		{
			if (P->GetMaterial(s) == M) { Target = s; break; }
		}
		FProcMeshSection Merged;
		if (Target >= 0 && P->GetProcMeshSection(Target)) { Merged = *P->GetProcMeshSection(Target); }
		else { Target = P->GetNumSections(); }
		const int32 Base = Merged.ProcVertexBuffer.Num();
		for (const FProcMeshVertex& V0 : Sections[k].ProcVertexBuffer)
		{
			FProcMeshVertex V = V0;
			V.Position = Where.TransformPosition(V0.Position);
			V.Normal = Where.TransformVectorNoScale(V0.Normal);
			Merged.ProcVertexBuffer.Add(V);
			Merged.SectionLocalBox += V.Position;
		}
		for (uint32 I : Sections[k].ProcIndexBuffer) { Merged.ProcIndexBuffer.Add(Base + I); }
		Merged.bEnableCollision = false;
		P->SetProcMeshSection(Target, Merged);
		P->SetMaterial(Target, M);
	}
	++NumChunks;
}

void AAntWreckPiles::Clear()
{
	for (auto& It : Piles) { if (It.Value) { It.Value->DestroyComponent(); } }
	Piles.Reset();
	NumChunks = 0;
}

// =============================================================================================
// Dev: -AntDo "wreck ..."
// =============================================================================================

bool AntWreck::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0 || !A[0].Equals(TEXT("wreck"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
	BuildCatalog(W);
	auto PieceArg = [&](int32 i) { return A.IsValidIndex(i) ? FindPiece(W, A[i]) : -1; };
	auto Top = [&](int32 Inst)
	{
		AAntWreckBody* B = BodyFor(W, Inst);
		const FPiece* P = PieceOf(W, Inst);
		const FBox Bx = B ? B->WorldBox() : P ? P->Box : FBox(FVector::ZeroVector, FVector::ZeroVector);
		return FVector(Bx.GetCenter().X, Bx.GetCenter().Y, Bx.Max.Z - Bx.GetExtent().Z * 0.2f);
	};
	if (Op == TEXT("list"))
	{
		const FString Sub = A.IsValidIndex(2) ? A[2] : FString();
		int32 N = 0;
		for (const FPiece& P : GCat.Pieces)
		{
			if (!Sub.IsEmpty() && !FString(All[P.Inst].Name).Contains(Sub)) { continue; }
			UE_LOG(LogTemp, Display, TEXT("ANTWRECK piece %d %s#%d %s %.1f kg hp %.0f%s at %s size %s"), P.Inst, All[P.Inst].Name, All[P.Inst].Index, Class(P.Mat).Name, P.Mass, P.Hp,
				P.bFixed ? TEXT(" fixed") : TEXT(""), *P.Box.GetCenter().ToCompactString(), *P.Box.GetSize().ToCompactString());
			if (++N > 200) { break; }
		}
	}
	else if (Op == TEXT("hit") || Op == TEXT("tip"))
	{
		// wreck hit <piece> <impulse kg m/s> [damage] [dx dy dz]; wreck tip <piece> [dx dy]: just enough to tip it over
		const int32 Inst = PieceArg(2);
		const FPiece* P = PieceOf(W, Inst);
		if (!P) { UE_LOG(LogTemp, Display, TEXT("ANTWRECK no piece %s"), A.IsValidIndex(2) ? *A[2] : TEXT("-")); return true; }
		FVector Dir = Op == TEXT("tip") ? FVector(Num(3, 1.f), Num(4, 0.f), 0.f) : FVector(Num(5, 1.f), Num(6, 0.f), Num(7, -0.3f));
		Dir = Dir.GetSafeNormal();
		const float I = Op == TEXT("tip") ? Class(P->Mat).WakeK * P->Mass * 2.2f : Num(3, 20.f);
		const float D = Op == TEXT("tip") ? 0.f : Num(4, 30.f);
		const FVector At = Top(Inst) - Dir * 50.f;
		HitPiece(W, Inst, At, Dir * I, D, nullptr);
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK dev %s %s#%d impulse %.1f damage %.0f"), *Op, All[Inst].Name, All[Inst].Index, I, D);
	}
	else if (Op == TEXT("at"))
	{
		// wreck at x y z radius impulse damage [dx dy dz]
		const FVector Dir = FVector(Num(8, 0.f), Num(9, 0.f), Num(10, -1.f)).GetSafeNormal();
		Hit(W, FVector(Num(2), Num(3), Num(4)), Num(5, 300.f), Dir * Num(6, 20.f), Num(7, 40.f), nullptr, TEXT("dev"));
	}
	else if (Op == TEXT("stage"))
	{
		const int32 Inst = PieceArg(2);
		const FPiece* P = PieceOf(W, Inst);
		if (!P) { return true; }
		const int32 Want = (int32)Num(3, 1.f);
		const float Frac[] = { 1.f, 0.7f, 0.45f, 0.2f, -1.f };
		AAntWreckBody* B = BodyFor(W, Inst);
		const float Cur = B ? B->Health : P->Hp;
		const float Dmg = FMath::Max(1.f, Cur - P->Hp * Frac[FMath::Clamp(Want, 0, 4)]);
		const FVector Dir = FVector(Num(4, 1.f), Num(5, 0.f), Num(6, -0.4f)).GetSafeNormal();
		HitPiece(W, Inst, Top(Inst) - Dir * 40.f, FVector::ZeroVector, Dmg, nullptr);
	}
	else if (Op == TEXT("drop"))
	{
		// wreck drop <piece> x y z: lift it there and let go (the cabinet onto the table)
		const int32 Inst = PieceArg(2);
		AAntWreckBody* B = HitPiece(W, Inst, Top(Inst), FVector::ZeroVector, 0.f, nullptr);
		if (B)
		{
			B->SetActorLocation(FVector(Num(3), Num(4), Num(5)), false, nullptr, ETeleportType::TeleportPhysics);
			B->Wake(FVector(0.f, 0.f, -0.01f), B->GetActorLocation(), true);
		}
	}
	else if (Op == TEXT("reset"))
	{
		ResetAll(W);
	}
	else if (Op == TEXT("track"))
	{
		// wreck track <piece>: where its actor, mesh and boxes are right now
		AAntWreckBody* B = BodyFor(W, PieceArg(2));
		if (B)
		{
			FString Boxes;
			Boxes += FString::Printf(TEXT(" root[%s..%s]"), *B->Root->Bounds.GetBox().Min.ToCompactString(), *B->Root->Bounds.GetBox().Max.ToCompactString());
			for (UBoxComponent* Hb : B->Hull) { Boxes += FString::Printf(TEXT(" [%s..%s]"), *Hb->Bounds.GetBox().Min.ToCompactString(), *Hb->Bounds.GetBox().Max.ToCompactString()); }
			UE_LOG(LogTemp, Display, TEXT("ANTWRECK track %s awake %d stage %d actor %s rot %s mesh box %s..%s v %.0f boxes%s"), *B->PieceName(), B->bAwake ? 1 : 0, B->Stage, *B->GetActorLocation().ToCompactString(),
				*B->GetActorRotation().ToCompactString(), *B->WorldBox().Min.ToCompactString(), *B->WorldBox().Max.ToCompactString(), (float)B->GetVelocity().Size(), *Boxes);
		}
	}
	else if (Op == TEXT("shot"))
	{
		// wreck shot <name>: Saved/Screenshots/wreck/<name>.png, no UI
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("wreck") / (A.IsValidIndex(2) ? A[2] : FString(TEXT("shot"))) + TEXT(".png"), false, false);
	}
	else
	{
		int32 Bodies = 0, Awake = 0, Broken = 0, Staged[5] = { 0, 0, 0, 0, 0 };
		for (TActorIterator<AAntWreckBody> It(W); It; ++It)
		{
			++Bodies; Awake += It->bAwake ? 1 : 0; Broken += It->IsBroken() ? 1 : 0; Staged[FMath::Min<int32>(It->Stage, 4)]++;
		}
		AAntWreckPiles* Pl = AAntWreckPiles::Get(W);
		// Frame rate since the last "wreck info" (the HUD's own -AntFpsLog stops with "hud 0").
		static double LastT = 0.0;
		static uint64 LastFrame = 0;
		const double NowT = FPlatformTime::Seconds();
		const float Fps = LastT > 0.0 ? (float)((GFrameCounter - LastFrame) / FMath::Max(0.001, NowT - LastT)) : 0.f;
		LastT = NowT;
		LastFrame = GFrameCounter;
		UE_LOG(LogTemp, Display, TEXT("ANTWRECK info: %d pieces, %d bodies (%d awake, %d broken; stages %d/%d/%d/%d), debris %d chunks in %d piles, fps %.1f since the last info"), GCat.Pieces.Num(), Bodies, Awake, Broken,
			Staged[0], Staged[1], Staged[2], Staged[3], Pl ? Pl->NumChunks : 0, Pl ? Pl->Piles.Num() : 0, Fps);
	}
	return true;
}
