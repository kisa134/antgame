#include "KitchenFixture.h"
#include "KitchenArt.h"
#include "KitchenLoot.h"
#include "KitchenMover.h"
#include "KitchenSlick.h"
#include "KitchenBuilder.h"
#include "LevelV3.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntRun.h"
#include "AntTypes.h"
#include "AntMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

// =============================================================================================
// The held work: [E] together, strength sums (AKitchenInteractable's door rule), progress over time.
// =============================================================================================

AKitchenHeld::AKitchenHeld()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetUpdateFrequency(10.f);
	InteractRadius = 140.f;
}

void AKitchenHeld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenHeld, Progress);
	DOREPLIFETIME(AKitchenHeld, bDone);
	DOREPLIFETIME(AKitchenHeld, bLoose);
}

int32 AKitchenHeld::DoorClass() const
{
	return bDone ? 0 : AntDoor::Light;   // (non-zero: the door rule's pull tick runs; the strength it takes is Need, not the class)
}

void AKitchenHeld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bHeldWork && !bDone)
	{
		PullTick(DeltaSeconds);   // -> TryOpenByPull every 0.25 s while someone holds [E]
	}
}

bool AKitchenHeld::TryOpenByPull()
{
	const float T = GetWorld()->GetTimeSeconds();
	const float Dt = LastStep < 0.f ? 0.f : FMath::Min(0.5f, T - LastStep);
	LastStep = T;
	if (bDone || !bHeldWork)
	{
		PullMissing = 0;
		return false;
	}
	int32 Hands = 0;
	AAntCharacter* First = nullptr;
	const float Have = PullStrength(&Hands, &First);
	if (Hands == 0)
	{
		PullMissing = 0;
		LastStep = -1.f;
		return false;
	}
	if (Have < Need)
	{
		PullMissing = (uint8)AntDoor::Missing(Need, Have);
		return false;
	}
	PullMissing = 0;
	// Seconds at exactly Need; more strength, faster (at most three times); loose: twice as fast.
	const float Rate = FMath::Min(3.f, Have / FMath::Max(1.f, Need)) * (bLoose ? 2.f : 1.f) / FMath::Max(0.2f, Seconds);
	Progress = FMath::Min(1.f, Progress + Rate * Dt);
	if (Progress >= 1.f)
	{
		ForceDone(First);
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			if (It->Pulling == this) { It->Pulling = nullptr; }
		}
	}
	return false;
}

void AKitchenHeld::ForceDone(AAntCharacter* By)
{
	if (!HasAuthority() || bDone)
	{
		return;
	}
	bDone = true;
	Progress = 1.f;
	PullMissing = 0;
	DoneBy = By;
	OnDone();
}

// =============================================================================================
// A fastener
// =============================================================================================

namespace
{
	struct FKindRule { float Need; float Seconds; const TCHAR* Verb; const TCHAR* Noun; };
	// screw, hinge, nail, magnet, tape, ring, knot, cap, needle, friction, zip, code
	const FKindRule KindRules[] =
	{
		{ 14.f, 8.f, TEXT("Откручивать шуруп"), TEXT("шуруп") },
		{ 14.f, 8.f, TEXT("Выкручивать петлю"), TEXT("петлю") },
		{ 6.f, 6.f, TEXT("Перегрызать шнурок рамки"), TEXT("шнурок") },
		{ 24.f, 1.f, TEXT("Столкнуть магнит"), TEXT("магнит") },
		{ 6.f, 3.f, TEXT("Отклеивать скотч"), TEXT("скотч") },
		{ 24.f, 4.f, TEXT("Разжимать кольцо шторы"), TEXT("кольцо") },
		{ 6.f, 5.f, TEXT("Перегрызать ленточку"), TEXT("ленточку") },
		{ 40.f, 6.f, TEXT("Откручивать крышку"), TEXT("крышку") },
		{ 24.f, 4.f, TEXT("Вытаскивать спицу"), TEXT("спицу") },
		{ 14.f, 1.f, TEXT("Толкать"), TEXT("предмет") },
		{ 24.f, 4.f, TEXT("Открывать молнию"), TEXT("молнию") },
		{ 14.f, 5.f, TEXT("Возиться"), TEXT("предмет") },
	};
	const FKindRule& Rule(int32 Kind) { return KindRules[FMath::Clamp(Kind, 0, (int32)UE_ARRAY_COUNT(KindRules) - 1)]; }

	FTransform RowTransform(int32 Inst)
	{
		const TConstArrayView<FKitchenArtInst> All = LevelV3::BaseArt();
		return All.IsValidIndex(Inst) ? KitchenArt::WorldTransform(All[Inst]) : FTransform::Identity;
	}

	UStaticMeshComponent* AddRow(AActor* Owner, USceneComponent* Parent, const FKitchenArtInst& I, FName Stable)
	{
		const FTransform W = KitchenArt::WorldTransform(I);
		return KitchenArt::Add(Owner, Parent, I.Name, I.Index, W.GetRelativeTransform(Parent->GetComponentTransform()), KitchenArt::ESolid::Moving, false, Stable);
	}

	/** The floor (or whatever is under it) below P, ignoring Self. */
	float FloorBelow(const UWorld* W, const FVector& P, const AActor* Self, float Fallback = 0.f)
	{
		FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_AntClimbable);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FixtureFloor), true, Self);
		FHitResult H;
		if (W->LineTraceSingleByObjectType(H, P, P - FVector(0.f, 0.f, 9000.f), Obj, Q))
		{
			return (float)H.ImpactPoint.Z;
		}
		return Fallback;
	}

	AAntKitchenGameMode* GMOf(const UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }

	const TCHAR* const BreakableIds[] = { TEXT("jam_jar"), TEXT("pickles"), TEXT("plant_k"), TEXT("teapot"), TEXT("photo_l"), TEXT("glasses") };
	float BreakFrom(int32 Mat) { return Mat == LevelV3::Glass ? 450.f : (Mat == LevelV3::Ceramic || Mat == LevelV3::Terracotta) ? 700.f : 1.e9f; }
	float NoiseOf(int32 Mat) { return Mat == LevelV3::Glass ? 0.9f : Mat == LevelV3::Ceramic ? 0.8f : Mat == LevelV3::Terracotta ? 0.8f : Mat == LevelV3::Metal ? 0.9f : Mat == LevelV3::Wood ? 0.7f
		: Mat == LevelV3::Cardboard ? 0.3f : Mat == LevelV3::Cloth ? 0.3f : Mat == LevelV3::Paper ? 0.1f : 0.4f; }
}

AKitchenFastener::AKitchenFastener()
{
	InteractRadius = 120.f;
}

void AKitchenFastener::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenFastener, Def);
	DOREPLIFETIME(AKitchenFastener, OffAt);
}

FString AKitchenFastener::KindName(int32 Kind) { return Rule(Kind).Noun; }

void AKitchenFastener::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AKitchenFastener::Build()
{
	const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
	if (!All.IsValidIndex(Def))
	{
		return;
	}
	const FLevelV3FastenerDef& F = All[Def];
	Need = Rule(F.Kind).Need;
	Seconds = Rule(F.Kind).Seconds;
	const TConstArrayView<FKitchenArtInst> Rows = LevelV3::BaseArt();
	if (Rows.IsValidIndex(F.Inst))
	{
		Mesh = AddRow(this, Root, Rows[F.Inst], TEXT("FastenerMesh"));
		if (Mesh)
		{
			Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Home = Mesh->GetComponentTransform();
			FloorZ = FloorBelow(GetWorld(), Home.GetLocation() + FVector(Home.GetRotation().GetForwardVector().X * 0.f, 0.f, -40.f), this, 0.f);
		}
	}
}

float AKitchenFastener::DistanceTo(const FVector& P) const
{
	return Mesh ? (float)FVector::Dist(P, Mesh->Bounds.Origin) - (float)Mesh->Bounds.SphereRadius : (float)FVector::Dist(P, GetActorLocation());
}

FString AKitchenFastener::DoorNoun() const
{
	const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
	return All.IsValidIndex(Def) ? Rule(All[Def].Kind).Noun : FString(TEXT("крепёж"));
}

FString AKitchenFastener::GetPrompt(const AAntCharacter* Ant) const
{
	const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
	if (!All.IsValidIndex(Def) || bDone)
	{
		return FString();
	}
	const FKindRule& R = Rule(All[Def].Kind);
	FString Of;
	if (LevelV3::Fixtures().IsValidIndex(All[Def].Fixture)) { Of = LevelV3::Fixtures()[All[Def].Fixture].Name; }
	return FString::Printf(TEXT("%s (%s) — держать [E] вместе%s%s"), R.Verb, *Of, bLoose ? TEXT(", шатается") : TEXT(""),
		Progress > 0.f ? *FString::Printf(TEXT(" · %d%%"), FMath::RoundToInt(Progress * 100.f)) : TEXT(""));
}

void AKitchenFastener::OnDone()
{
	OffAt = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
	UE_LOG(LogTemp, Display, TEXT("ANTFIX fastener %s off (%s) by %s"), All.IsValidIndex(Def) ? All[Def].Id : TEXT("?"), *DoorNoun(), DoneBy.IsValid() ? *DoneBy->GetDisplayName() : TEXT("-"));
	if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
	{
		GM->MakeNoise(GetActorLocation(), 0.15f, AntNoise::Crash, this, DoneBy.Get());
	}
	if (All.IsValidIndex(Def))
	{
		for (TActorIterator<AKitchenFixture> It(GetWorld()); It; ++It)
		{
			if (It->Fix == All[Def].Fixture) { It->OnFastenerOff(Def, DoneBy.Get()); }
		}
	}
}

void AKitchenFastener::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (OffAt < 0.f || !Mesh)
	{
		return;
	}
	if (!bOffLogged && GetNetMode() == NM_Client)
	{
		bOffLogged = true;
		UE_LOG(LogTemp, Display, TEXT("ANTFIX client sees fastener %s off"), LevelV3::Fasteners().IsValidIndex(Def) ? LevelV3::Fasteners()[Def].Id : TEXT("?"));
	}
	// Every machine: it drops off the wall and lies on the floor (0.6 s, a spin).
	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float T = FMath::Max(0.f, (GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - OffAt);
	const float H = FMath::Max(0.f, (float)Home.GetLocation().Z - FloorZ - 6.f);
	const float Fall = FMath::Max(0.15f, FMath::Sqrt(2.f * H / 9000.f));
	const float U = FMath::Clamp(T / Fall, 0.f, 1.f);
	FVector P = Home.GetLocation();
	P.Z -= H * U * U;
	Mesh->SetWorldLocationAndRotation(P, Home.GetRotation() * FQuat(FVector(1.f, 0.f, 0.f), U * PI * 1.5f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (U >= 1.f)
	{
		SetActorTickEnabled(false);
	}
}

// =============================================================================================
// A fixture
// =============================================================================================

AKitchenFixture::AKitchenFixture()
{
	InteractRadius = 260.f;
	bAlwaysRelevant = true;
}

void AKitchenFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenFixture, Fix);
	DOREPLIFETIME(AKitchenFixture, Var);
	DOREPLIFETIME(AKitchenFixture, State);
	DOREPLIFETIME(AKitchenFixture, FallStart);
	DOREPLIFETIME(AKitchenFixture, FallFrom);
	DOREPLIFETIME(AKitchenFixture, FallTo);
	DOREPLIFETIME(AKitchenFixture, FallTime);
	DOREPLIFETIME(AKitchenFixture, HangPose);
	DOREPLIFETIME(AKitchenFixture, BreakAt);
	DOREPLIFETIME(AKitchenFixture, BreakYaw);
}

FString AKitchenFixture::FixId() const
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	return All.IsValidIndex(Fix) ? FString(All[Fix].Id) : FString();
}

int32 AKitchenFixture::Mat() const
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	return All.IsValidIndex(Fix) ? All[Fix].Mat : LevelV3::Wood;
}

bool AKitchenFixture::IsBreakable() const
{
	const int32 M = Mat();
	return M == LevelV3::Glass || M == LevelV3::Ceramic || M == LevelV3::Terracotta;
}

float AKitchenFixture::NetNow() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? (float)GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AKitchenFixture::BeginPlay()
{
	Super::BeginPlay();
	Build();
	OnRep_State();
}

void AKitchenFixture::Build()
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	if (!All.IsValidIndex(Fix))
	{
		return;
	}
	const FLevelV3Fixture& F = All[Fix];
	const FString Id = F.Id;
	const TConstArrayView<FKitchenArtInst> Rows = LevelV3::BaseArt();
	const TConstArrayView<FKitchenArtInst> VarRows = LevelV3::VarArt();
	if (Var >= 0 && VarRows.IsValidIndex(Var))
	{
		Body = AddRow(this, Root, VarRows[Var], TEXT("FixBody"));
		KitchenArt::ApplyTint(Body, LevelV3::VarTint(Var));
		// its own parts that came with the variant: the teapot's lid; the cups go with the tablecloth
		for (int32 v = 0; v < VarRows.Num(); ++v)
		{
			const FString VId = LevelV3::VarId(v);
			if (v != Var && LevelV3::IsVarActive(v) && (VId.StartsWith(FString(F.Obj) + TEXT("_")) || (Id == TEXT("tablecloth") && VId.StartsWith(TEXT("v3_cup")))))
			{
				if (UStaticMeshComponent* P = AddRow(this, Body ? (USceneComponent*)Body : Root, VarRows[v], FName(*FString::Printf(TEXT("FixVarPart%d"), v)))) { Parts.Add(P); }
			}
		}
	}
	else if (Rows.IsValidIndex(F.Inst))
	{
		Body = AddRow(this, Root, Rows[F.Inst], TEXT("FixBody"));
		KitchenArt::ApplyTint(Body, LevelV3::BaseTint(F.Inst));
	}
	TArray<int32> PartRows;
	LevelV3::FixtureParts(Fix, PartRows);
	for (const TCHAR* Extra : LevelV3::ExtraParts(*Id))
	{
		const int32 R = LevelV3::FindBase(Extra);
		if (R >= 0) { PartRows.AddUnique(R); }
	}
	for (int32 R : PartRows)
	{
		if (Rows.IsValidIndex(R) && Body && Rows[R].Role == KitchenArt::Static)   // (a mover's row - the toy box lid - is its mover's)
		{
			if (UStaticMeshComponent* P = AddRow(this, Body, Rows[R], FName(*FString::Printf(TEXT("FixPart%d"), R)))) { Parts.Add(P); KitchenArt::ApplyTint(P, LevelV3::BaseTint(R)); }
		}
	}
	if (Body && (LevelV3::BaseFlags(F.Inst) & 1 || Id.StartsWith(TEXT("curtain")) || Id == TEXT("tablecloth") || Id == TEXT("fridge_art") || Id == TEXT("balloons")))
	{
		// cloth, paper, ribbons: the ants climb them, the Human walks through
		Body->ComponentTags.AddUnique(TEXT("AntsOnly"));
		Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	Home = GetActorTransform();
	// The held work of the fixture itself: pushing it over its edge (friction), or a fastener the design keeps in the code.
	int32 Own = 0;
	for (int32 k = 0; k < F.NumFasteners; ++k)
	{
		const FLevelV3FastenerDef& D = LevelV3::Fasteners()[F.FirstFastener + k];
		if (D.Inst < 0) { Own = D.Kind == LevelV3::Friction ? 1 : 2; }
	}
	bHeldWork = Own != 0;
	Need = LevelV3::PushNeed(*Id);
	Seconds = Own == 1 ? 1.f : 5.f;
	if (Id == TEXT("tablecloth")) { Need = 40.f; Seconds = 3.f; }   // four ants at the edge, like a heavy door
	if (Id == TEXT("cola")) { bHeldWork = false; }                  // first the cap
	if (Id == TEXT("toaster") || Id == TEXT("upper_doors")) { bHeldWork = false; }   // the toaster is a gadget already; the hinges of the upper doors: not yet
}

float AKitchenFixture::DistanceTo(const FVector& P) const
{
	if (!Body)
	{
		return (float)FVector::Dist(P, GetActorLocation());
	}
	const FBox B = Body->Bounds.GetBox();
	return FMath::Sqrt((float)B.ComputeSquaredDistanceToPoint(P));
}

FString AKitchenFixture::DoorNoun() const
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	return All.IsValidIndex(Fix) ? FString(All[Fix].Name).ToLower() : FString(TEXT("предмет"));
}

FString AKitchenFixture::GetPrompt(const AAntCharacter* Ant) const
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	if (!All.IsValidIndex(Fix) || bDone || !bHeldWork || State != Fixed)
	{
		return FString();
	}
	const FString Id = All[Fix].Id;
	const TCHAR* Verb = Id == TEXT("tablecloth") ? TEXT("Тянуть скатерть") : Id == TEXT("gift") ? TEXT("Перегрызать ленту") : Id == TEXT("trolley") ? TEXT("Открывать молнию")
		: Id == TEXT("outlets") ? TEXT("Откручивать винт рамки") : TEXT("Толкать к краю");
	return FString::Printf(TEXT("%s: %s — держать [E] вместе%s"), Verb, All[Fix].Name, Progress > 0.f ? *FString::Printf(TEXT(" · %d%%"), FMath::RoundToInt(Progress * 100.f)) : TEXT(""));
}

void AKitchenFixture::OnDone()
{
	// The fixture's own work is done: pushed over the edge / the ribbon, the zip, the screw of the cover is undone.
	AAntCharacter* By = DoneBy.Get();
	const FString Id = FixId();
	FVector Dir = !PushDir.IsNearlyZero() ? PushDir.GetSafeNormal2D() : By ? (GetActorLocation() - By->GetActorLocation()).GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
	if (Dir.IsNearlyZero()) { Dir = FVector(1.f, 0.f, 0.f); }
	if (PushDir.IsNearlyZero() && Id != TEXT("tablecloth"))
	{
		Dir = NearestEdgeDir(Dir);   // the ants push it towards the nearest edge on their far side (not into the wall)
	}
	UE_LOG(LogTemp, Display, TEXT("ANTFIX %s: work done by %s"), *Id, By ? *By->GetDisplayName() : TEXT("-"));
	if (Id == TEXT("gift") || Id == TEXT("trolley") || Id == TEXT("toybox"))
	{
		OpenLid();
		State = Open;
		Spill(By);
		OnRep_State();
		return;
	}
	if (Id == TEXT("tablecloth"))
	{
		KnockRiders(-Dir, By);   // it comes towards the pullers, and what stood on it with it
	}
	Fall(Id == TEXT("tablecloth") ? -Dir : Dir, By, TEXT("pushed"));
}

void AKitchenFixture::OnFastenerOff(int32 FastenerDef, AAntCharacter* By)
{
	const TConstArrayView<FLevelV3Fixture> All = LevelV3::Fixtures();
	if (!All.IsValidIndex(Fix) || State == Falling || State == Lying || State == Broken)
	{
		return;
	}
	LastBy = By;
	const FLevelV3Fixture& F = All[Fix];
	const int32 k = FastenerDef - F.FirstFastener;
	Undone.AddUnique(k);
	const FString Id = F.Id;
	auto Off = [this](int32 i) { return Undone.Contains(i); };
	auto Row = [&F](int32 i) { return RowTransform(LevelV3::Fasteners()[F.FirstFastener + i].Inst).GetLocation(); };
	if (Id == TEXT("shelf") || Id == TEXT("curtain_rod"))
	{
		// Two brackets, two screws each: both screws of one bracket out -> it hangs on the other; all four -> it falls.
		const bool A = Off(0) && Off(1), B = Off(2) && Off(3);
		if (A && B) { Fall(FVector(Id == TEXT("shelf") ? 1.f : 0.f, Id == TEXT("shelf") ? 0.f : 1.f, 0.f), By, TEXT("all screws out")); }
		else if ((A || B) && State == Fixed)
		{
			const FVector Pivot = A ? (Row(2) + Row(3)) * 0.5f : (Row(0) + Row(1)) * 0.5f;
			const FVector Loose = A ? (Row(0) + Row(1)) * 0.5f : (Row(2) + Row(3)) * 0.5f;
			const FVector Along = (Loose - Pivot).GetSafeNormal2D();
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
			Hang(Pivot, Axis, Id == TEXT("shelf") ? 35.f : 40.f, By);
		}
	}
	else if (Id == TEXT("curtain_k"))
	{
		if (Undone.Num() >= 3) { Fall(FVector(0.f, 1.f, 0.f), By, TEXT("three rings open")); }   // tears under its own weight
	}
	else if (Id == TEXT("balloons"))
	{
		Fall(FVector::UpVector, By, TEXT("ribbon bitten"));   // (up: a balloon rises to the ceiling)
	}
	else if (Id == TEXT("cola"))
	{
		bHeldWork = true;   // the cap is off: now push the bottle over
		Need = LevelV3::PushNeed(TEXT("cola"));
		Seconds = 1.f;
		UE_LOG(LogTemp, Display, TEXT("ANTFIX cola: cap off, the bottle can be pushed over"));
	}
	else if (Id == TEXT("fridge_art"))
	{
		if (Off(0) && Off(1)) { Fall(FVector(-1.f, 0.f, 0.f), By, TEXT("both strips of tape off")); }
	}
	else if (Id == TEXT("toybox"))
	{
		if (Off(0) && Off(1)) { OpenLid(); State = Open; Spill(By); OnRep_State(); }
	}
	else if (Id == TEXT("knitting"))
	{
		State = Open;   // a needle out: the stitches run, the scarf comes undone
		OnRep_State();
		if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->OnV3Fixture(this, TEXT("knitting"), By); }
	}
	else
	{
		// one fastener holds it (the photo's nail string, the outlet's screw...): it falls
		Fall(FVector(0.f, 0.f, 0.f), By, TEXT("fastener off"));
	}
}

void AKitchenFixture::Hang(const FVector& Pivot, const FVector& Axis, float Deg, AAntCharacter* By)
{
	const FTransform Cur = GetActorTransform();
	FQuat Best = FQuat::Identity;
	float BestZ = 1.e9f;
	for (float S : { 1.f, -1.f })
	{
		const FQuat Q(Axis, FMath::DegreesToRadians(Deg * S));
		const FVector C = Pivot + Q.RotateVector((Body ? Body->Bounds.Origin : Cur.GetLocation()) - Pivot);
		if (C.Z < BestZ) { BestZ = (float)C.Z; Best = Q; }
	}
	HangPose = FTransform(Best * Cur.GetRotation(), Pivot + Best.RotateVector(Cur.GetLocation() - Pivot), Cur.GetScale3D());
	State = Hanging;
	FallStart = NetNow();
	OnRep_State();
	const FVector Down = (BestZ < Cur.GetLocation().Z ? (Body ? Body->Bounds.Origin : Cur.GetLocation()) - Pivot : Pivot - Cur.GetLocation()).GetSafeNormal2D();
	UE_LOG(LogTemp, Display, TEXT("ANTFIX %s hangs on one side: %.0f degrees"), *FixId(), Deg);
	if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->MakeNoise(GetActorLocation(), 0.6f, AntNoise::Crash, this, By); GM->OnV3Fixture(this, TEXT("hang"), By); }
	KnockRiders(Down, By);   // what stood on it slides off the low end
}

void AKitchenFixture::Fall(const FVector& InDir, AAntCharacter* By, const TCHAR* Why)
{
	if (!HasAuthority() || State == Falling || State == Lying || State == Broken)
	{
		return;
	}
	LastBy = By ? By : LastBy.Get();
	bDone = true;
	bHeldWork = false;
	const FString Id = FixId();
	const FTransform From = GetActorTransform();
	FVector Dir = InDir.GetSafeNormal2D();
	FTransform To = From;
	float T = 0.5f;
	if (Id == TEXT("balloons"))
	{
		To.SetLocation(From.GetLocation() + FVector(700.f, -450.f, FMath::Clamp(5150.f - (float)From.GetLocation().Z, 0.f, 3000.f)));   // free: up against the ceiling, drifting towards the window
		T = 2.5f;
	}
	else
	{
		// Over the edge: a little way along Dir, then down onto whatever is below (the floor, a chair, the sofa).
		const float Side = Body ? FMath::Max(80.f, (float)Body->Bounds.BoxExtent.Size2D()) : 150.f;
		const float Bottom = Body ? (float)(Body->Bounds.Origin.Z - Body->Bounds.BoxExtent.Z) : (float)From.GetLocation().Z;
		FVector Out = From.GetLocation() + Dir * (Dir.IsNearlyZero() ? 0.f : Side + 120.f);
		if (!Dir.IsNearlyZero())
		{
			// Pushed (or carried by the tablecloth): it slides along Dir until what it stands on ends, then goes over that edge.
			for (int32 k = 1; k <= 40; ++k)
			{
				const FVector P = From.GetLocation() + Dir * (100.f * k);
				if (FloorBelow(GetWorld(), FVector(P.X, P.Y, Bottom + 8.f), this, 0.f) < Bottom - 150.f)
				{
					Out = P + Dir * (Side * 0.6f + 60.f);
					break;
				}
			}
		}
		const float Floor = FloorBelow(GetWorld(), FVector(Out.X, Out.Y, Bottom - 5.f), this, 0.f);
		const float Drop = FMath::Max(0.f, Bottom - Floor);
		const float Lift = (float)From.GetLocation().Z - Bottom;   // origin above its own bottom
		// lying: tipped onto its side (jars, bottles, boxes); flat things (cloth, paper, frames, shelves) land flat
		const bool bTip = Id == TEXT("cola") || Id == TEXT("cereal") || Id == TEXT("pickles") || Id == TEXT("jam_jar") || Id == TEXT("teapot") || Id == TEXT("glasses");
		FQuat Rot = From.GetRotation();
		float Z = Floor + Lift;
		if (bTip && !Dir.IsNearlyZero())
		{
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
			Rot = FQuat(Axis, FMath::DegreesToRadians(90.f)) * Rot;
			Z = Floor + (Body ? FMath::Min((float)Body->Bounds.BoxExtent.X, (float)Body->Bounds.BoxExtent.Y) : 40.f) - (Body ? Lift * 0.f : 0.f);
		}
		else if (Id == TEXT("shelf") || Id == TEXT("curtain_rod") || Id == TEXT("photo_l") || Id == TEXT("fridge_art") || Id == TEXT("outlets"))
		{
			Rot = FQuat(FVector::CrossProduct(FVector::UpVector, Dir.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Dir).GetSafeNormal(), FMath::DegreesToRadians(Id == TEXT("shelf") ? 8.f : 70.f)) * Rot;
			Z = Floor + 15.f;
		}
		if (Id == TEXT("curtain_k") || Id == TEXT("tablecloth"))
		{
			Z = Floor;   // (they become a heap: the after-mesh)
		}
		To = FTransform(Rot, FVector(Out.X, Out.Y, Z), From.GetScale3D());
		T = FMath::Clamp(FMath::Sqrt(2.f * FMath::Max(Drop, 30.f) / 9000.f), 0.2f, 1.2f) * (Id == TEXT("fridge_art") ? 2.5f : 1.f);   // paper glides
		BreakAt = FVector(Out.X, Out.Y, Floor);
		BreakYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		UE_LOG(LogTemp, Display, TEXT("ANTFIX %s falls (%s): from %s, drop %.0f, lands %s in %.2f s"), *Id, Why, *From.GetLocation().ToCompactString(), Drop, *To.GetLocation().ToCompactString(), T);
		// What it drops from decides whether it breaks.
		FallTime = T;
		FallFrom = State == Hanging ? HangPose : From;
		FallTo = To;
		FallStart = NetNow();
		State = Falling;
		bLanded = false;
		if (Drop >= BreakFrom(Mat())) { BreakYaw += 0.001f; }   // (marker only)
		OnRep_State();
		KnockRiders(Dir.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Dir, By);
		if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->OnV3Fixture(this, TEXT("fall"), By); }
		return;
	}
	FallTime = T;
	FallFrom = From;
	FallTo = To;
	FallStart = NetNow();
	State = Falling;
	bLanded = false;
	BreakAt = To.GetLocation();
	OnRep_State();
	UE_LOG(LogTemp, Display, TEXT("ANTFIX %s %s: rises to %s"), *Id, Why, *To.GetLocation().ToCompactString());
}

FTransform AKitchenFixture::PoseAt(float T) const
{
	const float U = FMath::Clamp(T / FMath::Max(0.05f, FallTime), 0.f, 1.f);
	const FVector A = FallFrom.GetLocation(), B = FallTo.GetLocation();
	FVector P = FMath::Lerp(A, B, FMath::Min(1.f, U * 1.6f));   // over the edge first
	P.Z = FMath::Lerp(A.Z, B.Z, U * U);                         // then gravity
	if (FixId() == TEXT("balloons")) { P.Z = FMath::Lerp(A.Z, B.Z, 1.f - (1.f - U) * (1.f - U)); }
	return FTransform(FQuat::Slerp(FallFrom.GetRotation(), FallTo.GetRotation(), FMath::Min(1.f, U * 1.3f)), P, FallTo.GetScale3D());
}

void AKitchenFixture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == Hanging)
	{
		const float U = FMath::Clamp((NetNow() - FallStart) / 0.35f, 0.f, 1.f);
		SetActorTransform(FTransform(FQuat::Slerp(Home.GetRotation(), HangPose.GetRotation(), U), FMath::Lerp(Home.GetLocation(), HangPose.GetLocation(), U), Home.GetScale3D()));
	}
	else if (State == Falling)
	{
		const float T = NetNow() - FallStart;
		SetActorTransform(PoseAt(T));
		if (HasAuthority() && T >= FallTime && !bLanded)
		{
			bLanded = true;
			Land();
		}
	}
	if (PoolStart >= 0.f && Pool)
	{
		// The cola runs out of the open bottle for 20 s: the sticky pool grows.
		const float U = FMath::Clamp((NetNow() - PoolStart) / 20.f, 0.05f, 1.f);
		Pool->SetRelativeScale3D(FVector(U, U, 1.f) * KitchenArt::ImportScale);
	}
}

void AKitchenFixture::Land()
{
	const float Drop = (float)(FallFrom.GetLocation().Z - FallTo.GetLocation().Z);
	const FString Id = FixId();
	AAntCharacter* By = LastBy.Get();
	if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
	{
		GM->MakeNoise(FallTo.GetLocation(), NoiseOf(Mat()), AntNoise::Crash, this, By);
	}
	AntSfx::Everyone(this, AntSfx::Crunch, FallTo.GetLocation(), IsBreakable() ? 1.4f : 0.7f);
	if (IsBreakable() && Drop >= BreakFrom(Mat()) - 1.f)
	{
		Break();
		return;
	}
	State = Lying;
	UE_LOG(LogTemp, Display, TEXT("ANTFIX %s landed whole (drop %.0f, %s)"), *Id, Drop, IsBreakable() ? TEXT("too low to break") : TEXT("does not break"));
	Spill(By);
	OnRep_State();
	if (Id == TEXT("shelf") || Id == TEXT("curtain_rod"))
	{
		if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->OnV3Break(this, By); }   // torn off the wall: as bad as a break to him
	}
	if (Id == TEXT("tablecloth"))
	{
		for (UStaticMeshComponent* P : Parts)
		{
			if (P) { UE_LOG(LogTemp, Display, TEXT("ANTFIX a cup off the tablecloth BREAKS (china)")); if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->OnV3Break(this, By); } }
		}
	}
}

void AKitchenFixture::BreakNow(AAntCharacter* By)
{
	if (!HasAuthority() || State == Broken)
	{
		return;
	}
	LastBy = By;
	FallTo = GetActorTransform();
	BreakAt = FVector(GetActorLocation().X, GetActorLocation().Y, FloorBelow(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 50.f), this, 0.f));
	if (AAntKitchenGameMode* GM = GMOf(GetWorld())) { GM->MakeNoise(GetActorLocation(), NoiseOf(Mat()), AntNoise::Crash, this, By); }
	Break();
}

void AKitchenFixture::Break()
{
	State = Broken;
	AAntCharacter* By = LastBy.Get();
	UE_LOG(LogTemp, Display, TEXT("ANTFIX %s BREAKS at %s (%s)"), *FixId(), *FVector(BreakAt).ToCompactString(), Mat() == LevelV3::Glass ? TEXT("glass") : Mat() == LevelV3::Ceramic ? TEXT("china") : TEXT("terracotta"));
	OnRep_State();
	Spill(By);
	if (AAntKitchenGameMode* GM = GMOf(GetWorld()))
	{
		GM->OnV3Break(this, By);
	}
}

void AKitchenFixture::Spill(AAntCharacter* By)
{
	// Server: what comes out of it.
	const FString Id = FixId();
	UWorld* W = GetWorld();
	const FVector At(BreakAt.X, BreakAt.Y, BreakAt.Z);
	auto Food = [W, this](const TCHAR* Name, int32 Value, int32 Weight, const FLinearColor& Color, EAntShape Shape, float Size, const FVector& P)
	{
		FAntFoodDef D;
		D.Name = Name; D.Value = Value; D.Weight = Weight; D.Color = Color; D.Shape = Shape; D.Scale = FVector(Size);
		if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			L->InitFood(D);
			L->FinishSpawning(FTransform(P));
		}
	};
	FRandomStream R(GetUniqueID() ^ 0x5EED);
	auto Around = [&R, &At](float Rad) { return At + FVector(R.FRandRange(-Rad, Rad), R.FRandRange(-Rad, Rad), 40.f); };
	if (Id == TEXT("jam_jar") && State == Broken)
	{
		for (int32 i = 0; i < 3; ++i) { Food(TEXT("Варенье"), 12, 2, FLinearColor(0.55f, 0.05f, 0.12f), EAntShape::Sphere, 0.16f, Around(250.f)); }
	}
	else if (Id == TEXT("pickles") && State == Broken)
	{
		AKitchenSlick::Spill(W, At + FVector(0.f, 0.f, 20.f), SlickKind::Water, By, 620.f, 80.f);   // brine: the Human slips in it
		for (int32 i = 0; i < 2; ++i) { Food(TEXT("Огурец"), 16, 8, FLinearColor(0.3f, 0.5f, 0.15f), EAntShape::Cylinder, 0.3f, Around(300.f)); }
	}
	else if (Id == TEXT("teapot") && State == Broken)
	{
		AKitchenSlick::Spill(W, At + FVector(0.f, 0.f, 20.f), SlickKind::Water, By, 480.f, 60.f);   // tea
	}
	else if (Id == TEXT("cereal"))
	{
		for (int32 i = 0; i < 20; ++i) { Food(TEXT("Хлопья"), 3, 1, FLinearColor(0.9f, 0.68f, 0.3f), EAntShape::Cube, 0.12f, Around(600.f)); }
	}
	else if (Id == TEXT("gift"))
	{
		for (int32 i = 0; i < 3; ++i) { Food(TEXT("Конфета"), 22, 2, FLinearColor(0.85f, 0.2f, 0.4f), EAntShape::Sphere, 0.16f, GetActorLocation() + FVector(R.FRandRange(-80.f, 80.f), R.FRandRange(-60.f, 60.f), 120.f)); }
	}
	else if (Id == TEXT("trolley"))
	{
		Food(TEXT("Картошка"), 18, 12, FLinearColor(0.7f, 0.55f, 0.32f), EAntShape::Sphere, 0.35f, GetActorLocation() + FVector(0.f, 200.f, 80.f));
		Food(TEXT("Лук"), 14, 8, FLinearColor(0.85f, 0.7f, 0.45f), EAntShape::Sphere, 0.3f, GetActorLocation() + FVector(150.f, 200.f, 80.f));
	}
	else if (Id == TEXT("toybox"))
	{
		Food(TEXT("Мармеладка"), 12, 1, FLinearColor(0.2f, 0.8f, 0.3f), EAntShape::Cube, 0.14f, GetActorLocation() + FVector(0.f, 0.f, 300.f));
		Food(TEXT("Печенье"), 8, 2, FLinearColor(0.74f, 0.48f, 0.2f), EAntShape::Cylinder, 0.2f, GetActorLocation() + FVector(60.f, 0.f, 300.f));
	}
}

void AKitchenFixture::OnRep_State()
{
	if (!Body)
	{
		return;
	}
	if (GetNetMode() == NM_Client && State != Fixed)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFIX client sees %s in state %d (break at %s)"), *FixId(), (int32)State, *FVector(BreakAt).ToCompactString());
	}
	if (State == Hanging)
	{
		SetActorTickEnabled(true);
	}
	if (State == Hanging || State == Falling)
	{
		DropStaticRiders();
	}
	if (State == Falling)
	{
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		for (UStaticMeshComponent* P : Parts) { if (P) { P->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	}
	if (State == Lying || State == Broken)
	{
		SetActorTransform(FallTo);
		const FString Id = FixId();
		const bool bFlat = Id == TEXT("curtain_k") || Id == TEXT("tablecloth");
		Body->SetCollisionEnabled(State == Broken || bFlat ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		if (State == Broken || bFlat)
		{
			Body->SetVisibility(false, false);
			for (UStaticMeshComponent* P : Parts) { if (P) { P->SetVisibility(false); P->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
		}
		ShowAfter();
	}
	if (State == Open)
	{
		const FString Id = FixId();
		for (UStaticMeshComponent* P : Parts)
		{
			// the lid comes off (the gift's, the toy box's): it lies beside
			if (P && (P->GetStaticMesh() && P->GetStaticMesh()->GetName().Contains(TEXT("Lid"))))
			{
				P->SetWorldLocationAndRotation(P->GetComponentLocation() + FVector(260.f, 0.f, -(float)P->GetComponentLocation().Z + FloorBelow(GetWorld(), P->GetComponentLocation() + FVector(260.f, 0.f, 0.f), this, 0.f)), P->GetComponentRotation() + FRotator(0.f, 35.f, 0.f));
			}
		}
		if (Id == TEXT("knitting"))
		{
			Body->SetRelativeScale3D(Body->GetRelativeScale3D() * FVector(1.f, 0.55f, 1.f));   // half of the scarf is gone: a long thread instead
		}
	}
}

void AKitchenFixture::ShowAfter()
{
	if (bAfterShown)
	{
		return;
	}
	bAfterShown = true;
	const FString Id = FixId();
	const FVector At(BreakAt.X, BreakAt.Y, BreakAt.Z);
	auto Put = [this, &At](const TCHAR* Mesh, float Scale, float Yaw, const FVector& Off = FVector::ZeroVector, bool bSolid = false) -> UStaticMeshComponent*
	{
		const FTransform T(FRotator(0.f, BreakYaw + Yaw, 0.f), At + Off, FVector(Scale * KitchenArt::ImportScale));
		UStaticMeshComponent* C = KitchenArt::Add(this, Root, Mesh, 0, T.GetRelativeTransform(Root->GetComponentTransform()), bSolid ? KitchenArt::ESolid::Moving : KitchenArt::ESolid::None, false);
		if (C)
		{
			C->SetAbsolute(true, true, true);
			C->SetWorldTransform(T);
			After.Add(C);
		}
		return C;
	};
	if (State == Broken)
	{
		const int32 M = Mat();
		if (Id == TEXT("plant_k")) { Put(TEXT("SM_V3_SoilHeap"), 1.f, 30.f, FVector::ZeroVector, true); }
		else { Put(TEXT("SM_V3_GlassShards"), Id == TEXT("teapot") ? 1.3f : Id == TEXT("glasses") ? 0.6f : 1.f, 20.f); }
		if (Id == TEXT("jam_jar")) { Put(TEXT("SM_V3_JamSplat"), 1.f, 0.f, FVector(60.f, 40.f, 0.f)); LevelV3::AddZone(0, At, 420.f); }
		if (M == LevelV3::Glass || M == LevelV3::Ceramic) { LevelV3::AddZone(2, At, Id == TEXT("glasses") ? 150.f : 300.f); }   // the shards cut
	}
	else if (Id == TEXT("curtain_k"))
	{
		Put(TEXT("SM_V3_CurtainTorn"), 1.f, 0.f, FVector::ZeroVector, true);
		LevelV3::AddCover(At, 750.f, 320.f, TEXT("в сорванной шторе"));
	}
	else if (Id == TEXT("tablecloth"))
	{
		Put(TEXT("SM_V3_TableclothPulled"), 1.f, 0.f, FVector::ZeroVector, true);
		// the cups rode it over the edge: shards where they hit
		int32 n = 0;
		for (UStaticMeshComponent* P : Parts)
		{
			if (!P) { continue; }
			Put(TEXT("SM_V3_GlassShards"), 0.55f, 40.f * n, FVector(-200.f + 260.f * n, 120.f * (n % 2 ? 1.f : -1.f), 0.f));
			LevelV3::AddZone(2, At + FVector(-200.f + 260.f * n, 0.f, 0.f), 220.f);
			++n;
		}
		LevelV3::AddCover(At, 700.f, 250.f, TEXT("под стянутой скатертью"));
	}
	else if (Id == TEXT("cereal"))
	{
		Put(TEXT("SM_V3_CerealSpill"), 1.f, 0.f, FVector(-250.f, 0.f, 0.f));
	}
	else if (Id == TEXT("cola") && PoolStart < 0.f)
	{
		// the open bottle lies there and runs: 20 s of a growing sticky pool
		Pool = Put(TEXT("SM_V3_ColaPuddle"), 0.05f, 0.f, FVector(-200.f, 0.f, 1.f));
		PoolStart = NetNow();
		LevelV3::AddZone(0, At, 700.f);
	}
}

void AKitchenFixture::DropStaticRiders()
{
	// Every machine: the plain pieces of the base that stood on it (the honey jar, the spice tins on the shelf) come down too.
	// The static piece is hidden and a movable copy lies on whatever is below it; on the server the loose props are knocked off.
	if (bRidersDropped || !Body)
	{
		return;
	}
	bRidersDropped = true;
	const FBox Top = FBox(Home.GetLocation() + Body->Bounds.Origin - GetActorLocation() - Body->Bounds.BoxExtent, Home.GetLocation() + Body->Bounds.Origin - GetActorLocation() + Body->Bounds.BoxExtent);
	const float TopZ = (float)Top.Max.Z;
	int32 N = 0;
	for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It)
	{
		const TConstArrayView<FKitchenArtInst> Rows = LevelV3::BaseArt();
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			UStaticMeshComponent* C = It->ArtComponent(r);
			if (!C || !C->IsVisible()) { continue; }
			const FBox B = C->Bounds.GetBox();
			const FVector O = B.GetCenter();
			if (O.X < Top.Min.X - 40.f || O.X > Top.Max.X + 40.f || O.Y < Top.Min.Y - 40.f || O.Y > Top.Max.Y + 40.f || B.Min.Z < TopZ - 40.f || B.Min.Z > TopZ + 120.f) { continue; }
			const float Floor = FloorBelow(GetWorld(), FVector(O.X + 200.f, O.Y, B.Min.Z - 30.f), this, 0.f);
			const FTransform T = C->GetComponentTransform();
			C->SetVisibility(false, true);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			const FTransform Lying(FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(80.f)) * T.GetRotation(), FVector(O.X + 200.f + 90.f * (N % 3), O.Y + 60.f * (N % 2), Floor + FMath::Min((float)B.GetExtent().X, (float)B.GetExtent().Y)), T.GetScale3D());
			if (UStaticMeshComponent* Copy = KitchenArt::Add(this, Root, Rows[r].Name, Rows[r].Index, Lying.GetRelativeTransform(Root->GetComponentTransform()), KitchenArt::ESolid::Moving, false))
			{
				Copy->SetAbsolute(true, true, true);
				Copy->SetWorldTransform(Lying);
				After.Add(Copy);
			}
			++N;
		}
	}
	if (HasAuthority())
	{
		for (TActorIterator<AKitchenProp> It(GetWorld()); It; ++It)
		{
			const FVector P = It->GetActorLocation();
			if (P.X > Top.Min.X - 60.f && P.X < Top.Max.X + 60.f && P.Y > Top.Min.Y - 60.f && P.Y < Top.Max.Y + 60.f && P.Z > TopZ - 40.f && P.Z < TopZ + 600.f)
			{
				It->Jolt(FVector(260.f, 0.f, 120.f), LastBy.Get());
				++N;
			}
		}
	}
	if (N > 0) { UE_LOG(LogTemp, Display, TEXT("ANTFIX %s: %d things that stood on it come down"), *FixId(), N); }
}

FVector AKitchenFixture::NearestEdgeDir(const FVector& Prefer) const
{
	if (!Body)
	{
		return Prefer;
	}
	const float Bottom = (float)(Body->Bounds.Origin.Z - Body->Bounds.BoxExtent.Z);
	FVector Best = Prefer;
	float BestD = 1.e9f;
	for (int32 a = 0; a < 16; ++a)
	{
		const FVector D(FMath::Cos(a * PI / 8.f), FMath::Sin(a * PI / 8.f), 0.f);
		if (FVector::DotProduct(D, Prefer) < -0.2f) { continue; }   // (not back through the ants)
		for (int32 k = 1; k <= 30; ++k)
		{
			const FVector P = GetActorLocation() + D * (100.f * k);
			if (FloorBelow(GetWorld(), FVector(P.X, P.Y, Bottom + 8.f), this, 0.f) < Bottom - 150.f)
			{
				const float Dist = 100.f * k - 30.f * FVector::DotProduct(D, Prefer);   // ties: the way they push
				if (Dist < BestD) { BestD = Dist; Best = D; }
				break;
			}
		}
	}
	return Best;
}

void AKitchenFixture::OpenLid()
{
	// the toy box's lid is a door of its own (AKitchenMover): both hinges out -> it stays open
	for (TActorIterator<AKitchenMover> It(GetWorld()); It; ++It)
	{
		const TConstArrayView<FKitchenArtMover> M = KitchenArt::Movers();
		if (M.IsValidIndex(It->PartIndex) && FString(KitchenArt::Instances()[M[It->PartIndex].Inst].Name) == TEXT("SM_V3_ToyBox_Lid") && FixId() == TEXT("toybox"))
		{
			It->SetOpen(true);
		}
	}
}

void AKitchenFixture::KnockRiders(const FVector& Dir, AAntCharacter* By)
{
	if (!HasAuthority() || !Body)
	{
		return;
	}
	// Fixtures standing on this one (the jars on the shelf, the plant on the sill under the rod, the teapot on the tablecloth) go over with it.
	const FString Id = FixId();
	const FBox Mine = Body->Bounds.GetBox().ExpandBy(FVector(60.f, 60.f, 200.f));
	for (TActorIterator<AKitchenFixture> It(GetWorld()); It; ++It)
	{
		AKitchenFixture* O = *It;
		if (O == this || !O->Body || O->State != Fixed) { continue; }
		const FString OId = O->FixId();
		bool bRides = Mine.IsInside(O->Body->Bounds.Origin) && O->Body->Bounds.Origin.Z >= Body->Bounds.Origin.Z - 60.f;
		if (Id == TEXT("curtain_rod") && (OId == TEXT("curtain_k") || OId == TEXT("plant_k"))) { bRides = true; }   // chain C3: the rod brings the curtains down, they sweep the plant off the sill
		if (Id == TEXT("tablecloth") && OId == TEXT("teapot")) { bRides = true; }                                  // chain C2
		if (bRides)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFIX chain: %s takes %s with it"), *Id, *OId);
			O->Fall(Dir, By, TEXT("knocked by what it stood on"));
		}
	}
}

// ---------------------------------------------------------------------------------------------

void AKitchenFixture::ResetAll(UWorld* World)
{
	if (!World || !LevelV3::Enabled() || !LevelV3::FixturesOn())
	{
		return;
	}
	for (TActorIterator<AKitchenFixture> It(World); It; ++It) { It->Destroy(); }
	for (TActorIterator<AKitchenFastener> It(World); It; ++It) { It->Destroy(); }
	LevelV3::ClearPlayZones();
	const TConstArrayView<FLevelV3Fixture> Fix = LevelV3::Fixtures();
	const TConstArrayView<FKitchenArtInst> Rows = LevelV3::BaseArt();
	int32 NFix = 0, NFast = 0;
	for (int32 f = 0; f < Fix.Num(); ++f)
	{
		const FLevelV3Fixture& F = Fix[f];
		int32 VarRow = -1;
		FVector Loc;
		if (Rows.IsValidIndex(F.Inst))
		{
			Loc = FVector(Rows[F.Inst].Loc);
		}
		else
		{
			VarRow = LevelV3::ActiveVarRow(F.Obj);   // the tablecloth, the teapot, the trolley: only when their variant is in play
			if (VarRow < 0) { continue; }
			Loc = FVector(LevelV3::VarArt()[VarRow].Loc);
		}
		if (FCString::Strcmp(F.Id, TEXT("toaster")) == 0 || FCString::Strcmp(F.Id, TEXT("upper_doors")) == 0)
		{
			continue;   // the toaster is its own gadget; the upper doors' hinges have no fastener rows yet
		}
		AKitchenFixture* A = World->SpawnActorDeferred<AKitchenFixture>(AKitchenFixture::StaticClass(), FTransform(Loc), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) { continue; }
		A->Fix = f;
		A->Var = VarRow;
		A->FinishSpawning(FTransform(Loc));
		++NFix;
		for (int32 k = 0; k < F.NumFasteners; ++k)
		{
			const int32 D = F.FirstFastener + k;
			const FLevelV3FastenerDef& FD = LevelV3::Fasteners()[D];
			if (!Rows.IsValidIndex(FD.Inst)) { continue; }   // (friction and code: the fixture's own work)
			AKitchenFastener* S = World->SpawnActorDeferred<AKitchenFastener>(AKitchenFastener::StaticClass(), FTransform(FVector(Rows[FD.Inst].Loc)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!S) { continue; }
			S->Def = D;
			S->bLoose = LevelV3::IsLoose(FD.Inst);
			S->FinishSpawning(FTransform(FVector(Rows[FD.Inst].Loc)));
			++NFast;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTFIX reset: %d fixtures, %d fasteners"), NFix, NFast);
}

AKitchenFixture* AKitchenFixture::Find(UWorld* World, const FString& Id)
{
	for (TActorIterator<AKitchenFixture> It(World); It; ++It)
	{
		if (It->FixId().Equals(Id, ESearchCase::IgnoreCase)) { return *It; }
	}
	return nullptr;
}
