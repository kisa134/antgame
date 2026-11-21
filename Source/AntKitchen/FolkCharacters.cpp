#include "FolkCharacters.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "AntRun.h"
#include "AntTypes.h"
#include "AntKitchenGameMode.h"
#include "AntMovementComponent.h"
#include "Critters.h"
#include "KitchenArt.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "KitchenLoot.h"
#include "AIController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// Every number a player can feel (also in docs/FEATURES.md, section 18).
namespace
{
	// ---- grandma
	constexpr float GranSee[3] = { 2600.f, 2600.f, 3600.f };   // how far she notices an ant on the floor (at 0 she only looks)
	constexpr float GranNotice = 1.2f;         // seconds of looking at it before she starts getting up
	constexpr float GranSwatRange = 1500.f;    // she swats what is this close to her feet
	constexpr float GranSweepNear = 700.f, GranSweepFar = 2100.f, GranSweepHalf = 65.f;   // the sector the broom clears
	constexpr float GranSweepThrow = 1300.f, GranSweepDamage = 10.f, GranSweepEvery = 5.f;
	constexpr float GranTidyRange = 2600.f;    // food on the floor this close gets put away
	constexpr int32 GranTidyTrip = 2;          // pieces per trip out of the armchair
	constexpr float GranLose = 4.f;            // seconds without seeing the ant before she gives up
	// ---- the kid
	constexpr float KidSee[3] = { 900.f, 1800.f, 2600.f };
	constexpr float KidReach = 430.f;          // how close he gets before he does something
	constexpr float KidPinchRadius = 130.f;
	constexpr float KidCarryMin = 3.5f, KidCarryMax = 5.f;
	constexpr int32 KidGrip = 5;               // presses of [Space] to wriggle out of his fingers
	constexpr float KidPokeDamage = 20.f;
	constexpr float KidStompEvery = 0.6f;
	constexpr float KidGrabEvery = 6.f;
	constexpr float KidCupTime = 25.f;
	constexpr int32 KidCupRocks = 6;
	constexpr float KidScreamEvery = 8.f;
	constexpr float KidReachUp = 1100.f;       // how high above the floor his fingers get: the sofa, the coffee table, low shelves

	FVector Flat2(const FVector& V) { return FVector(V.X, V.Y, 0.f); }

	FVector ArtPoint(const TCHAR* Name, int32 Index, const FVector& Fallback)
	{
		const FKitchenArtInst* I = KitchenArt::Enabled() ? KitchenArt::Find(Name, Index) : nullptr;
		return I ? FVector(I->Loc) : Fallback;
	}
}

// ---------------------------------------------------------------------------------------------
// What grandma and the kid share
// ---------------------------------------------------------------------------------------------

AHouseFolk::AHouseFolk()
{
	AIControllerClass = AAIController::StaticClass();   // a plain controller: the head is in the character (Think)
}

void AHouseFolk::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Rage();   // the figure's own walking speed
}

FVector AHouseFolk::FloorPoint(const FVector& P) const
{
	return FVector(P.X, P.Y, NpcWorld::SurfaceZ(GetWorld(), FVector(P.X, P.Y, 150.f), 100.f, 400.f) + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.f);
}

void AHouseFolk::SetMode(uint8 NewMode, float Seconds)
{
	Mode = NewMode;
	ModeSince = GetWorld()->GetTimeSeconds();
	ModeUntil = ModeSince + Seconds;
}

void AHouseFolk::TurnTo(const FVector& Point, float Dt, float Rate)
{
	const FVector To = Flat2(Point - GetActorLocation());
	if (To.SizeSquared() > 400.f)
	{
		// The body itself (a plain AI controller copies the pawn's facing every tick, it does not drive it).
		const FRotator Face = FMath::RInterpTo(FRotator(0.f, GetActorRotation().Yaw, 0.f), FRotator(0.f, (float)To.Rotation().Yaw, 0.f), Dt, Rate);
		SetActorRotation(Face);
		if (Controller) { Controller->SetControlRotation(Face); }
	}
}

bool AHouseFolk::WalkTowards(const FVector& Goal, float Scale, float Arrive)
{
	const FVector Me = GetActorLocation();
	if (FVector::Dist2D(Me, Goal) <= Arrive)
	{
		return true;
	}
	// Level v2: round the furniture on the house grid (the Human's FHumanNav), not straight into it.
	FVector Way = AKitchenBuilder::Route(Me, FVector(Goal.X, Goal.Y, Me.Z));
	if (AAntKitchenGameMode* NavGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		if (const FHumanNav* Nav = NavGM->HouseNav())
		{
			const FVector Next = NavFollow.Next(Nav, Me, FVector(Goal.X, Goal.Y, Me.Z), GetWorld()->GetTimeSeconds());
			if (NavFollow.Path.Num() > 0) { Way = Next; }
		}
	}
	const FVector D = (Way - Me).GetSafeNormal2D();
	if (!D.IsNearlyZero())
	{
		AddMovementInput(D, Scale);
		TurnTo(Me + D * 500.f, GetWorld()->GetDeltaSeconds(), Fig == HumanFig::Kid ? 7.f : 2.6f);
		// Walking into furniture: call it there after a while.
		StuckFor = GetVelocity().Size2D() < 30.f ? StuckFor + GetWorld()->GetDeltaSeconds() : 0.f;
		if (StuckFor > 2.5f)
		{
			StuckFor = 0.f;
			return true;
		}
	}
	return false;
}

void AHouseFolk::SitDown(const FVector& Seat, uint8 How)
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	StartAct(HumanAct::Sit, How, Seat);
}

void AHouseFolk::SetSitHow(uint8 How)
{
	if (Act.Kind == HumanAct::Sit && Act.Arg != How)
	{
		Act.Arg = How;   // the same sitting, something else in the hands: the action does not start over
		ForceNetUpdate();
	}
}

void AHouseFolk::GetUp()
{
	if (Act.Kind == HumanAct::Sit)
	{
		StartAct(HumanAct::Rise, Act.Arg, FVector(Act.Target));
	}
}

bool AHouseFolk::IsGettingUp() const
{
	return Act.Kind == HumanAct::Rise && ActTime() < HumanAct::SitTime * HumanBody::Style(Fig).SitSlow;
}

AAntCharacter* AHouseFolk::SeeAnt(float Range, float* OutDist, float MaxUp) const
{
	const UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	const float Floor = FeetZ();
	const FVector Eyes = Me + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.75f);
	const int32 Room = AKitchenBuilder::Region(Me);
	AAntCharacter* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->Creature != 0 || A->IsAttached())
		{
			continue;
		}
		const FVector L = A->GetActorLocation();
		const float D = (float)FVector::Dist2D(L, Me);
		if (D >= BestD || AKitchenBuilder::Region(L) != Room || L.Z > Floor + MaxUp || L.Z < Floor - 200.f)
		{
			continue;
		}
		if (KitchenContainers::SealedAt(W, L) >= 0 || !NpcWorld::CanSee(W, Eyes, L + FVector(0.f, 0.f, 12.f)))
		{
			continue;
		}
		BestD = D;
		Best = A;
	}
	if (OutDist) { *OutDist = BestD; }
	return Best;
}

void AHouseFolk::Tick(float DeltaSeconds)
{
	if (HasAuthority() && HasJointedBody() && !bDevPuppet)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (!bSettled)
		{
			if (Now >= 1.5f)   // the house is built by then
			{
				bSettled = true;
				Settle();
			}
		}
		else if (!IsStunned())
		{
			Think(DeltaSeconds, Now);
		}
	}
	Super::Tick(DeltaSeconds);
}

void AHouseFolk::DevAct(const FString& What, const FVector& Target)
{
	if (!HasAuthority())
	{
		return;
	}
	if (What == TEXT("sit0") || What == TEXT("sit1") || What == TEXT("sit2"))
	{
		// Onto a seat behind the feet: the floor (sit2) or an armchair's height (sit0 knitting, sit1 dozing).
		const uint8 How = (uint8)(What[3] - TEXT('0'));
		const FVector Seat = GetActorLocation() - GetActorForwardVector() * (How == 2 ? 160.f : 660.f) * FigScale / 0.88f;
		SitDown(FVector(Seat.X, Seat.Y, FeetZ() + (How == 2 ? 0.f : 800.f)), How);
	}
	else if (What == TEXT("rise")) { GetUp(); GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
	else if (What == TEXT("look")) { FolkLook = Target; }
	else if (What == TEXT("nolook")) { FolkLook = FVector::ZeroVector; }
	else if (What == TEXT("carry1") || What == TEXT("carry0")) { bCarryPose = What.EndsWith(TEXT("1")); }
	else if (What == TEXT("panic")) { Panic(3.f); }
	else { Super::DevAct(What, Target); }
}

FString AHouseFolk::FolkDescribe() const
{
	return FString::Printf(TEXT("%s mode=%d yaw=%.0f since=%.1f level=%d quarry=%s seated=%d | %s"), Fig == HumanFig::Grandma ? TEXT("gran") : TEXT("kid"), (int32)Mode, GetActorRotation().Yaw,
		GetWorld()->GetTimeSeconds() - ModeSince, NpcWorld::Level(GetWorld(), Fig == HumanFig::Grandma ? NpcWho::Grandma : NpcWho::Kid),
		Quarry.IsValid() ? *Quarry->GetDisplayName() : TEXT("-"), IsSeated() ? 1 : 0, *DevDescribe());
}

// ---------------------------------------------------------------------------------------------
// Grandma
// ---------------------------------------------------------------------------------------------

AGrandmaCharacter::AGrandmaCharacter()
{
	InitFigure(HumanFig::Grandma);
}

void AGrandmaCharacter::Settle()
{
	// Her armchair, from where the furniture stands; the seat's height from the armchair itself.
	const FVector Chair = ArtPoint(TEXT("SM_L_Armchair"), 0, FVector(9150.f, -5800.f, 0.f));
	FVector Seat = Chair + FVector(100.f, 0.f, 800.f);
	Seat.Z = NpcWorld::SurfaceZ(GetWorld(), Seat, 500.f, 700.f);
	SeatPoint = Seat;
	StandYaw = 0.f;   // the armchair faces east
	StandPoint = FloorPoint(Seat + FVector(660.f, 0.f, 0.f));
	// Her feet are inside the armchair's footprint when she sits: near it her capsule does not collide with the chair (Think),
	// so she walks right up to the seat and sits down. (She used to stop at its front edge and shuffle the last 500 backwards.)
	ChairPart = AKitchenBuilder::FindArt(GetWorld(), TEXT("SM_L_Armchair"));
	SetActorLocationAndRotation(StandPoint, FRotator(0.f, StandYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller) { Controller->SetControlRotation(FRotator(0.f, StandYaw, 0.f)); }
	SitDown(SeatPoint, 0);
	SetMode(Seated);
	NextKnit = GetWorld()->GetTimeSeconds() + FMath::FRandRange(20.f, 35.f);
	UE_LOG(LogTemp, Display, TEXT("ANTGRAN in her armchair: seat %s, feet %s"), *SeatPoint.ToCompactString(), *StandPoint.ToCompactString());
}

void AGrandmaCharacter::OnFolkHurt(float Damage, AController* By)
{
	// Bitten on the ankle: "Ой!" - and she looks down at her feet for whoever did it.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextOuch)
	{
		return;
	}
	NextOuch = Now + 1.5f;
	if (AAntCharacter* A = By ? Cast<AAntCharacter>(By->GetPawn()) : nullptr)
	{
		Quarry = A;
		SeenSince = Now - GranNotice;
		LastSeen = Now;
		FolkLook = A->GetActorLocation();
	}
	if (IsSeated()) { SetSitHow(0); }
	if (AKitchenLoot* L = Held)
	{
		// ...and what she was carrying off falls out of her hand.
		Held = nullptr;
		if (IsValid(L) && L->bHeld) { L->Throw(FVector(0.f, 0.f, -50.f)); }
		UE_LOG(LogTemp, Display, TEXT("ANTGRAN bitten: dropped what she carried"));
	}
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 1.5f);
}

void AGrandmaCharacter::TrySweep(const FVector& Point)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || Now < NextSweep || IsBodyBusy() || IsSeated())
	{
		return;
	}
	NextSweep = Now + GranSweepEvery;
	StartAct(HumanAct::Sweep, 0, Point);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Point]() { DoSweep(Point); }), HumanAct::SweepHit + 0.15f, false);
	UE_LOG(LogTemp, Display, TEXT("ANTGRAN sweeps at %s"), *Point.ToCompactString());
}

void AGrandmaCharacter::DoSweep(const FVector& Point)
{
	// Everything on the floor in front of her goes with the broom, right to left: ants, crumbs, whatever they were carrying.
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	const float Floor = FeetZ();
	const FVector Aim = Flat2(Point - Me).GetSafeNormal();
	const FVector Left = FVector::CrossProduct(Aim, FVector::UpVector);   // her left when she faces Aim
	const float Cos = FMath::Cos(FMath::DegreesToRadians(GranSweepHalf));
	auto InSector = [&](const FVector& P)
	{
		const FVector To = Flat2(P - Me);
		const float D = (float)To.Size();
		return D > GranSweepNear * 0.6f && D < GranSweepFar && P.Z < Floor + 260.f && P.Z > Floor - 120.f && FVector::DotProduct(To / FMath::Max(1.f, D), Aim) > Cos;
	};
	int32 Swept = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->IsAttached() || !InSector(A->GetActorLocation()))
		{
			continue;
		}
		if (A->IsCarrying()) { A->LetGo(); }
		A->HurtBy(GranSweepDamage, TEXT("выметен бабушкиной метлой"));
		if (!A->bDead)
		{
			A->ApplyLaunch((Left * GranSweepThrow + Aim * 250.f + FVector(0.f, 0.f, 300.f)) * AntKit::SpeedK);
			A->Toast(TEXT("Бабушка вымела тебя метлой!"));
		}
		++Swept;
	}
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		if (!It->bHeld && InSector(It->GetActorLocation()))
		{
			It->Kick(Left * 620.f + FVector(0.f, 0.f, 260.f), It->GetActorLocation() + FMath::VRand() * 8.f);
		}
	}
	AAntArmy::KillInSphere(W, Me + Aim * (GranSweepFar * 0.6f), GranSweepFar * 0.45f, 20);
	AntSfx::Everyone(this, AntSfx::Swat, Me + Aim * 1200.f - FVector(0.f, 0.f, Me.Z - Floor), 0.6f);
	UE_LOG(LogTemp, Display, TEXT("ANTGRAN broom stroke: %d ants swept"), Swept);
}

AKitchenLoot* AGrandmaCharacter::FindMess(float Range) const
{
	const UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	AKitchenLoot* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		const FVector P = L->GetActorLocation();
		const float Dd = (float)FVector::Dist2D(P, Me);
		// Only what was dragged about and left (an ant had it in its jaws) and what the kid dropped: not the house's own food.
		if (!L->bIsGear && !L->bVault && !L->bBody && !L->bTrophy && !L->bHeld && !L->IsCarried() && L->Weight <= 20 && Dd < BestD && P.Z < FeetZ() + 200.f
			&& (!L->LastCarrier.IsExplicitlyNull() || AKidCharacter::IsCrumb(W, L))
			&& AKitchenBuilder::Region(P) == AKitchenBuilder::Region(Me) && KitchenContainers::At(P) < 0 && !NpcWorld::UnderCover(W, P, 500.f))
		{
			BestD = Dd;
			Best = L;
		}
	}
	return Best;
}

void AGrandmaCharacter::DevAct(const FString& What, const FVector& Target)
{
	if (What == TEXT("sweep")) { NextSweep = 0.f; TrySweep(Target); }
	else { Super::DevAct(What, Target); }
}

void AGrandmaCharacter::Think(float Dt, float Now)
{
	// By her own armchair she walks through its front edge (see Settle); anywhere else the chair is furniture like the rest.
	if (UPrimitiveComponent* Chair = ChairPart.Get())
	{
		const bool bNear = FVector::Dist2D(GetActorLocation(), StandPoint) < 950.f;
		if (bNear != bChairThrough)
		{
			bChairThrough = bNear;
			GetCapsuleComponent()->IgnoreComponentWhenMoving(Chair, bNear);
		}
	}
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>();
	const int32 Level = NpcWorld::Level(W, NpcWho::Grandma);
	const bool bDozing = IsSeated() && Act.Arg == 1;

	// Her ears: a noise wakes her, and her head turns to it before anything else.
	bool bNoise = false;
	if (GM)
	{
		for (const FAntNoise& N : GM->Run.Noises)
		{
			if (N.Time > LastNoise && N.Loud >= 0.5f && FVector::Dist(N.Loc, Me) < 6000.f)
			{
				bNoise = true;
				FolkLook = N.Loc;
				ModeUntil = FMath::Max(ModeUntil, Now + 3.f);
			}
			LastNoise = FMath::Max(LastNoise, N.Time);
		}
	}
	if (bNoise && bDozing)
	{
		SetSitHow(0);
		NextKnit = Now + FMath::FRandRange(20.f, 35.f);
	}

	// Her eyes (not while she dozes): what she looks at, and how long she has been looking.
	float D = 0.f;
	AAntCharacter* Seen = bDozing ? nullptr : SeeAnt(GranSee[Level], &D);
	if (Seen)
	{
		if (Quarry.Get() != Seen && (!Quarry.IsValid() || Now - LastSeen > 1.f)) { Quarry = Seen; SeenSince = Now; }
		if (Quarry.Get() == Seen) { LastSeen = Now; }
		FolkLook = Seen->GetActorLocation();
	}
	else if (!bNoise && Now > ModeUntil && Now - LastSeen > 1.f)
	{
		FolkLook = FVector::ZeroVector;
	}
	AAntCharacter* Q = Quarry.Get();
	if (Q && (Q->bDead || Q->IsInNest() || Now - LastSeen > GranLose))
	{
		Q = nullptr;
		Quarry = nullptr;
	}

	// The cat on the coffee table: off!
	if (!bDozing && Now >= NextScold)
	{
		NextScold = Now + 6.f;
		const FVector Table = ArtPoint(TEXT("SM_L_CoffeeTable"), 0, FVector(13750.f, -3400.f, 0.f));
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == 1 && It->GetActorLocation().Z > 250.f && FVector::Dist2D(It->GetActorLocation(), Table) < 1300.f && FVector::Dist2D(Me, Table) < 6500.f && It->State != QuadMode::Fright)
			{
				NextScold = Now + 30.f;
				FolkLook = It->GetActorLocation();
				It->Scare(Me, TEXT("бабушки"));
				if (GM) { GM->Broadcast(TEXT("Бабушка согнала кошку со стола: «А ну брысь!»"), FLinearColor(0.9f, 0.8f, 0.9f)); }
			}
		}
	}

	switch (Mode)
	{
	case Seated:
		if (Now >= NextKnit && !Seen)
		{
			// Knitting, then nodding off, then knitting again.
			SetSitHow(Act.Arg == 0 ? 1 : 0);
			NextKnit = Now + (Act.Arg == 1 ? FMath::FRandRange(15.f, 25.f) : FMath::FRandRange(20.f, 35.f));
		}
	{
		bool bMess = false;
		if (Level >= 1 && !Q && !bDozing && !Seen && Now >= NextTidy && Now - ModeSince > 15.f)
		{
			bMess = FindMess(GranTidyRange) != nullptr;
			if (!bMess) { NextTidy = Now + 2.f; }
		}
		if (Level >= 1 && ((Q && Now - SeenSince >= GranNotice) || bMess))
		{
			// Up she gets, slowly: you see it coming.
			if (Q) { Q->Toast(TEXT("Бабушка тебя увидела и встаёт с кресла...")); }
			GetUp();
			TidyLeft = GranTidyTrip;
			if (!Q && GM) { GM->Broadcast(TEXT("Бабушка заметила на полу еду и встаёт её убрать"), FLinearColor(0.9f, 0.8f, 0.9f)); }
			SetMode(Rising, HumanAct::SitTime * HumanBody::Style(Fig).SitSlow + 0.1f);
			UE_LOG(LogTemp, Display, TEXT("ANTGRAN gets up (%s)"), Q ? *Q->GetDisplayName() : TEXT("to tidy"));
		}
		break;
	}

	case Rising:
		if (Now >= ModeUntil)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			SetMode(Q ? (uint8)Hunting : (uint8)Tidying, 40.f);
		}
		break;

	case Hunting:
	{
		if (!Q)
		{
			SetMode(Tidying, 30.f);
			break;
		}
		const FVector QL = Q->GetActorLocation();
		const float Dist = (float)FVector::Dist2D(Me, QL);
		const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, (float)Flat2(QL - Me).Rotation().Yaw));
		if (IsBodyBusy())
		{
			break;
		}
		if (NpcWorld::UnderCover(W, QL, 330.f))
		{
			TurnTo(QL, Dt);   // under the furniture: she stands and peers, and loses it after a while
			break;
		}
		// The broom for what is far or loaded (it shoves), the swatter for what is right at her feet (it kills).
		const bool bBroom = Dist > GranSweepNear + 250.f || Q->IsCarrying();
		if (bBroom && Dist < GranSweepFar - 250.f && YawErr < 25.f && Now >= NextSweep)
		{
			TrySweep(QL);
		}
		else if (!bBroom && Dist < GranSwatRange && YawErr < 30.f && Now >= SwatReadyTime)
		{
			Q->Toast(TEXT("Бабушка замахнулась мухобойкой!"));
			TrySwat(QL);
		}
		else if (Dist > (bBroom ? GranSweepFar - 500.f : GranSwatRange - 400.f))
		{
			WalkTowards(QL, 1.f, 300.f);
		}
		else
		{
			TurnTo(QL, Dt);
		}
		if (Now >= ModeUntil) { SetMode(Tidying, 30.f); }
		break;
	}

	case Tidying:
	{
		// Food lying about on the floor goes into the cabinet under the TV, and its door gets shut.
		if (Q && Now - SeenSince >= GranNotice && !Held)
		{
			SetMode(Hunting, 40.f);
			break;
		}
		if (!Held && !IsBodyBusy() && Now - TidyClock > 1.5f)
		{
			AKitchenLoot* Best = TidyLeft > 0 ? FindMess(GranTidyRange) : nullptr;
			if (!Best || Now >= ModeUntil || Level < 1)
			{
				NextTidy = Now + FMath::FRandRange(40.f, 70.f);
				SetMode(Returning, 40.f);
				break;
			}
			if (WalkTowards(Best->GetActorLocation(), 1.f, 1150.f))
			{
				TurnTo(Best->GetActorLocation(), Dt, 5.f);
				GrabOrThrow(Best->GetActorLocation(), GetActorForwardVector());
				TidyClock = Now;
			}
		}
		else if (Held && Now - TidyClock > 1.f)
		{
			SetMode(Stowing, 40.f);
		}
		break;
	}

	case Stowing:
	{
		const KitchenContainers::FDef& Box = KitchenContainers::Get(KitchenContainers::TVStand);
		const FVector Inside = (Box.Min + Box.Max) * 0.5f;
		AKitchenLoot* L = Held;
		if (!L || !IsValid(L))
		{
			Held = nullptr;
			SetMode(Tidying, 30.f);
			break;
		}
		if (WalkTowards(Inside, 1.f, 1150.f) || Now >= ModeUntil)
		{
			if (ReachAt <= 0.f)
			{
				PlayReach(Inside);
				ReachAt = Now + HumanAct::ReachTouch + 0.2f;
			}
			else if (Now >= ReachAt)
			{
				ReachAt = 0.f;
				Held = nullptr;
				L->Throw(FVector::ZeroVector);
				L->SetActorLocation(FVector(Inside.X + FMath::FRandRange(-300.f, 300.f), Inside.Y, Box.Min.Z + 80.f), false, nullptr, ETeleportType::TeleportPhysics);
				KitchenContainers::SetOpen(W, KitchenContainers::TVStand, false);
				if (GM) { GM->Broadcast(FString::Printf(TEXT("Бабушка убрала «%s» в тумбу под телевизором и закрыла дверцу"), *L->LootName), FLinearColor(0.9f, 0.8f, 0.9f)); }
				UE_LOG(LogTemp, Display, TEXT("ANTGRAN put «%s» away"), *L->LootName);
				--TidyLeft;
				SetMode(Tidying, 25.f);
			}
		}
		break;
	}

	case Returning:
		if (Q && Now - SeenSince >= GranNotice && Level >= 1)
		{
			bHome = false;
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			SetMode(Hunting, 40.f);
			break;
		}
		if (!bHome && (WalkTowards(StandPoint, 1.f, ChairPart.IsValid() ? 110.f : 520.f) || Now >= ModeUntil))
		{
			bHome = true;   // in front of her armchair, a step from where her feet go
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
		}
		if (bHome)
		{
			// Turns her back to it, shuffles the last steps backwards, and down.
			TurnTo(GetActorLocation() + FRotator(0.f, StandYaw, 0.f).Vector() * 500.f, Dt, 3.f);
			const FVector Step = FMath::VInterpConstantTo(GetActorLocation(), StandPoint, Dt, 260.f);
			SetActorLocation(Step, false, nullptr, ETeleportType::TeleportPhysics);
			if (FVector::Dist2D(Step, StandPoint) < 8.f && FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, StandYaw)) < 6.f)
			{
				bHome = false;
				SetActorLocation(StandPoint, false, nullptr, ETeleportType::TeleportPhysics);
				SitDown(SeatPoint, 0);
				NextKnit = Now + FMath::FRandRange(20.f, 35.f);
				SetMode(Seated);
			}
		}
		break;

	default:
		SetMode(Returning, 40.f);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The kid
// ---------------------------------------------------------------------------------------------

AKidCharacter::AKidCharacter()
{
	InitFigure(HumanFig::Kid);
}

void AKidCharacter::Settle()
{
	// On the floor in front of the sofa, facing the TV.
	const FVector Sofa = ArtPoint(TEXT("SM_L_Sofa"), 0, FVector(13500.f, 550.f, 0.f));
	StandYaw = -90.f;
	StandPoint = FloorPoint(Sofa + FVector(300.f, -1750.f, 0.f));
	SeatPoint = FVector(StandPoint.X, StandPoint.Y + 190.f, StandPoint.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocationAndRotation(StandPoint, FRotator(0.f, StandYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller) { Controller->SetControlRotation(FRotator(0.f, StandYaw, 0.f)); }
	SitDown(SeatPoint, 2);
	SetMode(FloorPlay);
	const float Now = GetWorld()->GetTimeSeconds();
	NextUp = Now + FMath::FRandRange(14.f, 26.f);
	NextCrumb = Now + FMath::FRandRange(18.f, 30.f);
	UE_LOG(LogTemp, Display, TEXT("ANTKID on the floor at %s"), *StandPoint.ToCompactString());
}

void AKidCharacter::DropCrumb()
{
	// Something falls out of his hand: a crumb, now and then a sticky sweet.
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		const FVector At = GetPalmLocation(1) + GetActorForwardVector() * 120.f;
		Crumbs.Add(GM->SpawnFood(At, FMath::FRand() < 0.3f ? 2 : 1));
		GM->Broadcast(TEXT("Малыш уронил крошку!"), FLinearColor(0.9f, 0.9f, 0.6f));
	}
}

bool AKidCharacter::IsCrumb(const UWorld* W, const AKitchenLoot* L)
{
	for (TActorIterator<AKidCharacter> It(W); It; ++It)
	{
		It->Crumbs.RemoveAll([](const TWeakObjectPtr<AKitchenLoot>& C) { return !C.IsValid(); });
		if (It->Crumbs.Contains(L))
		{
			return true;
		}
	}
	return false;
}

void AKidCharacter::DropAnt(bool bCup)
{
	AAntCharacter* A = HeldAnt.Get();
	HeldAnt = nullptr;
	bCarryPose = false;
	if (!A)
	{
		return;
	}
	A->SetNpcHold(nullptr, FVector::ZeroVector);
	if (A->bDead)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (bCup)
	{
		// Under his cup, upside down on the floor: somebody has to knock it over (a dash into it), or it has to be rocked off from inside.
		const FVector At(A->GetActorLocation().X, A->GetActorLocation().Y, FeetZ() + 100.f);
		const FTransform TM(FRotator(180.f, 0.f, 0.f), At);
		if (AKitchenProp* P = W->SpawnActorDeferred<AKitchenProp>(AKitchenProp::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			P->ShapeIndex = (uint8)EAntShape::Cylinder;
			P->Size = FVector(170.f, 170.f, 190.f);
			P->Color = FLinearColor(0.45f, 0.8f, 0.95f);
			P->Mass = 1.2f;
			P->PropName = TEXT("Стаканчик малыша");
			P->FinishSpawning(TM);
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(P->GetRootComponent())) { Root->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); Root->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }   // the ant sits inside it, its camera stays outside
			A->SetNpcHold(P->GetRootComponent(), FVector::ZeroVector);
			FCup Cup;
			Cup.Cup = P;
			Cup.Ant = A;
			Cup.At = At;
			Cup.Until = (float)W->GetTimeSeconds() + KidCupTime;
			Cups.Add(Cup);
			Grip = KidCupRocks;
			A->Toast(TEXT("Малыш накрыл тебя стаканчиком! Свои могут опрокинуть его рывком; [Пробел] — раскачивать"));
			UE_LOG(LogTemp, Display, TEXT("ANTKID put %s under a cup at %s"), *A->GetDisplayName(), *At.ToCompactString());
			return;
		}
	}
	A->Toast(TEXT("Малыш уронил тебя"));
	UE_LOG(LogTemp, Display, TEXT("ANTKID dropped %s at %s"), *A->GetDisplayName(), *A->GetActorLocation().ToCompactString());
}

void AKidCharacter::WatchCups(float Now)
{
	for (int32 i = Cups.Num() - 1; i >= 0; --i)
	{
		FCup& C = Cups[i];
		AKitchenProp* P = C.Cup.Get();
		AAntCharacter* A = C.Ant.Get();
		const bool bTipped = !P || FVector::Dist(P->GetActorLocation(), C.At) > 70.f || FMath::Abs(P->GetActorUpVector().Z) < 0.75f;
		if (!A || A->bDead || bTipped || Now >= C.Until)
		{
			if (A && !A->bDead && A->NpcHold)
			{
				A->SetNpcHold(nullptr, FVector::ZeroVector);
				A->Toast(TEXT("Стаканчик опрокинут — свободен!"));
				UE_LOG(LogTemp, Display, TEXT("ANTKID the cup is off %s (%s)"), *A->GetDisplayName(), bTipped ? TEXT("tipped") : TEXT("time"));
			}
			if (P) { P->SetLifeSpan(20.f); }
			Cups.RemoveAt(i);
		}
	}
}

void AKidCharacter::Struggle(AAntCharacter* Ant)
{
	if (!HasAuthority() || !Ant)
	{
		return;
	}
	if (HeldAnt.Get() == Ant)
	{
		if (--Grip > 0)
		{
			Ant->Toast(FString::Printf(TEXT("Вырываешься из пальцев... ещё %d"), Grip));
			return;
		}
		DropAnt(false);
		Ant->Toast(TEXT("Вырвался! Малыш ойкнул"));
		Panic(1.2f);
		NextGrab = GetWorld()->GetTimeSeconds() + KidGrabEvery;
		SetMode(Scream, 1.2f);
		return;
	}
	for (FCup& C : Cups)
	{
		AKitchenProp* P = C.Cup.Get();
		if (C.Ant.Get() == Ant && P)
		{
			// Rocking the cup from inside: a few good shoves and it goes over.
			--Grip;
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(P->GetRootComponent()))
			{
				const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
				Root->AddImpulse(Dir * (Grip <= 0 ? 520.f : 60.f) + FVector(0.f, 0.f, Grip <= 0 ? 260.f : 40.f), NAME_None, true);
			}
			if (Grip > 0) { Ant->Toast(FString::Printf(TEXT("Раскачиваешь стаканчик... ещё %d"), Grip)); }
		}
	}
}

void AKidCharacter::OnFolkHurt(float Damage, AController* By)
{
	// Bitten: he drops what he holds, screams, and runs about with his hands in the air. The adults hear it.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextOuch)
	{
		return;
	}
	NextOuch = Now + KidScreamEvery;
	if (IsSeated())
	{
		GetUp();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	DropAnt(false);
	Panic(2.5f);
	NextGrab = Now + KidGrabEvery;
	GoalPoint = AKitchenBuilder::RandomFloorPoint();
	SetMode(Scream, 2.5f);
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 300.f), 1.9f);
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->MakeNoise(GetActorLocation(), 1.f, AntNoise::Scream, this, By ? Cast<AAntCharacter>(By->GetPawn()) : nullptr);
		GM->Broadcast(TEXT("Малыша укусили — он визжит на весь дом! Взрослые идут смотреть"), FLinearColor(1.f, 0.7f, 0.4f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTKID screams (bitten)"));
}

void AKidCharacter::DevAct(const FString& What, const FVector& Target)
{
	if (What == TEXT("crumb")) { DropCrumb(); }
	else if (What == TEXT("scream")) { OnFolkHurt(1.f, nullptr); }
	else if (What == TEXT("poke")) { PlayReach(Target); }
	else if (What == TEXT("pinch")) { StartAct(HumanAct::Grab, 0, Target); }
	else if (What == TEXT("cup")) { DropAnt(true); }
	else if (What == TEXT("swipe")) { GetCharacterMovement()->SetMovementMode(MOVE_Walking); StartAct(HumanAct::None, 0, FVector::ZeroVector); GoalPoint = FVector::ZeroVector; NextSwipe = 0.f; SetMode(Toddle, 3.f); }
	else if (What.StartsWith(TEXT("want"))) { DevWant = FCString::Atoi(*What.RightChop(4)) - 0; if (What.Len() == 4) { DevWant = -1; } }
	else { Super::DevAct(What, Target); }
}

void AKidCharacter::Think(float Dt, float Now)
{
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	const int32 Level = NpcWorld::Level(W, NpcWho::Kid);
	WatchCups(Now);

	// Crumbs and sticky sweets: wherever he is.
	if (Now >= NextCrumb && Mode <= Toddle && !IsBodyBusy() && !IsKnockedOut())
	{
		NextCrumb = Now + FMath::FRandRange(25.f, 45.f);
		if (!IsSeated()) { DropCrumb(); }
		else if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
		{
			Crumbs.Add(GM->SpawnFood(Me + GetActorForwardVector() * FMath::FRandRange(250.f, 500.f) + GetActorRightVector() * FMath::FRandRange(-300.f, 300.f) - FVector(0.f, 0.f, Me.Z - FeetZ() - 80.f), FMath::FRand() < 0.3f ? 2 : 1));
			GM->Broadcast(TEXT("Малыш уронил крошку!"), FLinearColor(0.9f, 0.9f, 0.6f));
		}
	}

	float D = 0.f;
	AAntCharacter* Seen = (Mode == FloorPlay || Mode == Toddle) ? SeeAnt(KidSee[Level], &D, KidReachUp) : nullptr;
	if (Seen)
	{
		FolkLook = Seen->GetActorLocation();   // he stares at it first
		if (Quarry.Get() != Seen) { Quarry = Seen; SeenSince = Now; }
		LastSeen = Now;
	}
	else if (Mode == FloorPlay || Mode == Toddle)
	{
		FolkLook = FVector::ZeroVector;
		if (Now - LastSeen > 1.f) { Quarry = nullptr; }
	}
	AAntCharacter* Q = Quarry.Get();
	if (Q && (Q->bDead || Q->IsInNest() || (Q->IsAttached() && HeldAnt.Get() != Q)))
	{
		Q = nullptr;
		Quarry = nullptr;
	}

	switch (Mode)
	{
	case FloorPlay:
		// Level 0: he only watches. Bolder: an ant he has stared at for a second is the most interesting thing in the room.
		if ((Level >= 1 && Q && Now - SeenSince > 1.f && Now >= NextGrab) || (Level >= 1 && Now >= NextUp))
		{
			GetUp();
			SetMode(Toddle, 0.6f);
			GoalPoint = FVector::ZeroVector;
		}
		break;

	case Toddle:
	{
		if (IsGettingUp())
		{
			break;
		}
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (Q && Now - SeenSince > 0.8f && Now >= NextGrab)
		{
			SetMode(Chase, 8.f);
			break;
		}
		if (Now >= NextSwipe && !IsBodyBusy())
		{
			// Something lying on a low surface within his reach: he pulls it off.
			NextSwipe = Now + 4.f;
			for (TActorIterator<AKitchenLoot> It(W); It; ++It)
			{
				const FVector P = It->GetActorLocation();
				if (!It->bIsGear && !It->bHeld && !It->IsCarried() && P.Z > FeetZ() + 300.f && P.Z < FeetZ() + 1250.f && FVector::Dist2D(P, Me) < 750.f && KitchenContainers::At(P) < 0)
				{
					Thing = *It;
					GoalPoint = P;
					bActed = false;
					PlayReach(P);
					SetMode(Poking, 1.f);
					NextSwipe = Now + 12.f;
					break;
				}
			}
			if (Mode == Poking) { break; }
		}
		if (GoalPoint.IsZero())
		{
			// Somewhere else, in bursts: the living room, and when he is bold the whole floor.
			FVector P = AKitchenBuilder::RandomFloorPoint();
			for (int32 i = 0; i < 12 && Level < 2 && AKitchenBuilder::Region(P) != 1; ++i) { P = AKitchenBuilder::RandomFloorPoint(); }
			GoalPoint = P;
			ModeUntil = Now + FMath::FRandRange(4.f, 9.f);
		}
		const bool bPause = FMath::Fmod(Now, 3.1f) > 2.3f;   // stop, look about, off again
		if ((!bPause && WalkTowards(GoalPoint, FMath::Fmod(Now, 3.1f) < 1.2f ? 1.f : 0.55f, 250.f)) || Now >= ModeUntil)
		{
			if (FMath::FRand() < 0.45f || Level < 1)
			{
				// Plops down where he is and plays.
				StandPoint = Me;
				SeatPoint = FVector(Me.X, Me.Y, FeetZ()) - GetActorForwardVector() * 190.f;
				SitDown(SeatPoint, 2);
				NextUp = Now + FMath::FRandRange(10.f, 20.f);
				SetMode(FloorPlay);
			}
			else
			{
				GoalPoint = FVector::ZeroVector;
			}
		}
		break;
	}

	case Chase:
	{
		if (!Q || Now >= ModeUntil || Now - LastSeen > 2.f)
		{
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 5.f);
			break;
		}
		const FVector QL = Q->GetActorLocation();
		if (QL.Z < FeetZ() + KidReachUp && NpcWorld::CanSee(W, Me + FVector(0.f, 0.f, 400.f), QL + FVector(0.f, 0.f, 12.f)) && !NpcWorld::UnderCover(W, QL, 300.f)) { LastSeen = Now; }
		FolkLook = QL;
		if (!WalkTowards(QL, 1.f, KidReach))
		{
			break;
		}
		TurnTo(QL, Dt, 8.f);
		if (FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, (float)Flat2(QL - Me).Rotation().Yaw)) > 30.f || IsBodyBusy())
		{
			break;
		}
		// What does he do with it? Mostly he picks it up; an ant with food gets stomped at; sometimes he just pokes.
		const float Roll = DevWant == 0 ? 0.f : DevWant == 1 ? 0.3f : DevWant == 2 ? 0.9f : FMath::FRand();
		const FVector Ground = QL - FVector(0.f, 0.f, AAntCharacter::BodyRadius);
		bActed = false;
		const bool bOnFloor = QL.Z < FeetZ() + 120.f;
		if (bOnFloor && (Q->IsCarrying() || Roll < 0.2f))
		{
			StompsLeft = 3;
			SetMode(Stomping, 3.f);
			Q->Toast(TEXT("Малыш затопал ножками!"));
		}
		else if (Roll < 0.45f)
		{
			PlayReach(Ground);
			GoalPoint = Ground;
			SetMode(Poking, 1.f);
			Q->Toast(TEXT("Малыш тянет к тебе палец..."));
		}
		else
		{
			StartAct(HumanAct::Grab, 0, Ground);
			GoalPoint = Ground;
			SetMode(Pinching, 1.f);
			Q->Toast(TEXT("Малыш тянется схватить тебя! В сторону!"));
		}
		break;
	}

	case Poking:
	case Pinching:
		if (!bActed && Now - ModeSince >= HumanAct::GrabGrip)
		{
			bActed = true;
			if (AKitchenLoot* T = Thing.Get())
			{
				Thing = nullptr;
				if (!T->bHeld)
				{
					if (T->IsCarried()) { T->ReleaseAll(); }
					T->Kick(Flat2(Me - T->GetActorLocation()).GetSafeNormal() * 420.f + FVector(0.f, 0.f, 220.f), T->GetActorLocation());
					UE_LOG(LogTemp, Display, TEXT("ANTKID pulled «%s» down"), *T->LootName);
				}
				break;
			}
			// The fingers are there: whoever is still under them...
			AAntCharacter* Got = nullptr;
			float Best = KidPinchRadius;
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				const float Dd = (float)FVector::Dist(It->GetActorLocation(), GoalPoint);
				if (!It->bDead && !It->IsInNest() && !It->IsAttached() && It->Creature == 0 && Dd < Best) { Best = Dd; Got = *It; }
			}
			if (Got && Mode == Poking)
			{
				Got->HurtBy(KidPokeDamage, TEXT("ткнул пальцем малыш"));
				if (!Got->bDead) { Got->ApplyLaunch((Flat2(Got->GetActorLocation() - Me).GetSafeNormal() * 520.f + FVector(0.f, 0.f, 260.f)) * AntKit::SpeedK); }
				UE_LOG(LogTemp, Display, TEXT("ANTKID poked %s"), *Got->GetDisplayName());
			}
			else if (Got)
			{
				// Between two fingers, up in front of his face, and off he goes with it.
				HeldAnt = Got;
				Grip = KidGrip;
				bCarryPose = true;
				Got->SetNpcHold(Segs[HumanSeg::FingersL], FVector(0.f, 40.f, -110.f) / HumanBody::Scale);
				Got->Toast(TEXT("Малыш схватил тебя двумя пальцами и куда-то несёт! [Пробел] — вырываться, ЛКМ — укусить за палец"));
				FVector To = AKitchenBuilder::RandomFloorPoint();
				for (int32 i = 0; i < 8 && FVector::Dist2D(To, Me) < 2500.f; ++i) { To = AKitchenBuilder::RandomFloorPoint(); }
				GoalPoint = To;
				SetMode(Carry, FMath::FRandRange(KidCarryMin, KidCarryMax));
				UE_LOG(LogTemp, Display, TEXT("ANTKID picked up %s, carries it towards %s"), *Got->GetDisplayName(), *To.ToCompactString());
				break;
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("ANTKID %s: missed"), Mode == Poking ? TEXT("poke") : TEXT("pinch"));
			}
		}
		if (Now >= ModeUntil)
		{
			NextGrab = Now + (Mode == Poking ? 2.f : KidGrabEvery * 0.5f);
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 4.f);
		}
		break;

	case Carry:
	{
		AAntCharacter* A = HeldAnt.Get();
		if (!A || A->bDead)
		{
			DropAnt(false);
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 4.f);
			break;
		}
		FolkLook = A->GetActorLocation();
		if (WalkTowards(GoalPoint, 1.f, 300.f) || Now >= ModeUntil)
		{
			// Bored of it: down it goes. When he is bold, sometimes under his cup.
			DropAnt(Level >= 2 && FMath::FRand() < 0.4f);
			NextGrab = Now + KidGrabEvery;
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 4.f);
		}
		break;
	}

	case Stomping:
		if (Q) { TurnTo(Q->GetActorLocation(), Dt, 8.f); }
		if (StompsLeft > 0 && !IsBodyBusy() && Now >= StompReadyTime)
		{
			// Small quick stomps where it was a moment ago.
			--StompsLeft;
			TryStomp(-1);
			StompReadyTime = Now + KidStompEvery;
		}
		if ((StompsLeft <= 0 && !IsBodyBusy()) || Now >= ModeUntil)
		{
			NextGrab = Now + 3.f;
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 4.f);
		}
		break;

	case Scream:
		// Running about, hands in the air.
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (WalkTowards(GoalPoint, 1.f, 300.f)) { GoalPoint = AKitchenBuilder::RandomFloorPoint(); }
		if (Now >= ModeUntil && !IsKnockedOut())
		{
			GoalPoint = FVector::ZeroVector;
			SetMode(Toddle, 4.f);
		}
		break;

	default:
		GoalPoint = FVector::ZeroVector;
		SetMode(Toddle, 4.f);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The test driver and the shared hooks
// ---------------------------------------------------------------------------------------------

void AHouseFolk::DevCommand(const TArray<FString>& A)
{
	UWorld* W = GetWorld();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	const FString Sub = A[1].ToLower();
	// The figure's own frame: dx ahead of it, dy to its right, dz above its floor.
	const FTransform B(FRotator(0.f, GetActorRotation().Yaw, 0.f), FVector(GetActorLocation().X, GetActorLocation().Y, FeetZ()));
	if (Sub == TEXT("puppet")) { bDevPuppet = Num(2, 1.f) > 0.5f; DevWalk = FVector::ZeroVector; }
	else if (Sub == TEXT("to"))
	{
		// gran to x y yaw: stands there (out of the armchair).
		StartAct(HumanAct::None, 0, FVector::ZeroVector);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		const FRotator R(0.f, Num(4), 0.f);
		SetActorLocationAndRotation(FloorPoint(FVector(Num(2), Num(3), 0.f)), R, false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) { Controller->SetControlRotation(R); }
	}
	else if (Sub == TEXT("walk")) { GetCharacterMovement()->SetMovementMode(MOVE_Walking); DevWalk = FVector(Num(2), Num(3), GetActorLocation().Z); DevWalkScale = Num(4, 1.f); }
	else if (Sub == TEXT("act") && A.Num() >= 3)
	{
		DevAct(A[2].ToLower(), A.Num() >= 6 ? B.TransformPosition(Vec(3)) : (Me ? Me->GetActorLocation() - FVector(0.f, 0.f, AAntCharacter::BodyRadius) : B.TransformPosition(FVector(600.f, 0.f, 0.f))));
	}
	else if (Sub == TEXT("freeze")) { DevFreezeAt = Num(2, -1.f); }
	else if (Sub == TEXT("info")) { UE_LOG(LogTemp, Display, TEXT("ANTNPC %s"), *FolkDescribe()); }
	else if (Sub == TEXT("bite") && Me) { UGameplayStatics::ApplyDamage(this, 10.f, Me->GetController(), Me, UDamageType::StaticClass()); }
	else if (Sub == TEXT("struggle") && Me) { FolkWorld::Struggle(Me); }
	else if (Sub == TEXT("me") && Me) { Me->ApplyTeleport(B.TransformPosition(FVector(Num(2, 600.f), Num(3), Num(4, 20.f)))); }
	else if (Sub == TEXT("cam") && PC)
	{
		// gran cam dx dy dz lookZ [fov] [lookX]
		static TWeakObjectPtr<ACameraActor> DevCam;
		if (!DevCam.IsValid()) { DevCam = W->SpawnActor<ACameraActor>(); }
		if (ACameraActor* Cam = DevCam.Get())
		{
			const FVector From = B.TransformPosition(Vec(2));
			const FVector To = B.TransformPosition(FVector(Num(7, 0.f), 0.f, Num(5, 700.f)));
			Cam->SetActorLocationAndRotation(From, (To - From).Rotation());
			Cam->GetCameraComponent()->SetFieldOfView(Num(6, 45.f));
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			PC->SetViewTargetWithBlend(Cam, 0.f);
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("ANTNPC unknown: %s %s"), *A[0], *Sub); }
}

namespace FolkWorld
{

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	const FString Op = A.Num() > 0 ? A[0].ToLower() : FString();
	if (!GM || (Op != TEXT("gran") && Op != TEXT("kid")))
	{
		return false;
	}
	AHouseFolk* F = nullptr;
	for (TActorIterator<AHouseFolk> It(GM->GetWorld()); It; ++It)
	{
		if (It->Fig == (Op == TEXT("gran") ? HumanFig::Grandma : HumanFig::Kid)) { F = *It; }
	}
	if (!F || A.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTNPC no %s / no command"), *Op);
		return true;
	}
	F->DevCommand(A);
	return true;
}

bool Struggle(AAntCharacter* Ant)
{
	if (!Ant || !Ant->NpcHold)
	{
		return false;
	}
	if (AHouseFolk* F = Cast<AHouseFolk>(Ant->NpcHold->GetOwner()))
	{
		F->Struggle(Ant);
		return true;
	}
	if (Cast<AKitchenProp>(Ant->NpcHold->GetOwner()))
	{
		for (TActorIterator<AKidCharacter> It(Ant->GetWorld()); It; ++It) { It->Struggle(Ant); }   // under the kid's cup
		return true;
	}
	return false;
}

bool BiteHolder(AAntCharacter* Ant)
{
	AHouseFolk* F = (Ant && Ant->NpcHold) ? Cast<AHouseFolk>(Ant->NpcHold->GetOwner()) : nullptr;
	if (F)
	{
		UGameplayStatics::ApplyDamage(F, 10.f, Ant->GetController(), Ant, UDamageType::StaticClass());
		Ant->Toast(FString::Printf(TEXT("Укусил: %s"), *F->GetDisplayName()));
	}
	return F != nullptr;
}

} // namespace FolkWorld

// ---------------------------------------------------------------------------------------------
// Filming mode (AntFilm.h): the director's keys
// ---------------------------------------------------------------------------------------------

void AHouseFolk::DirectorDoze(bool bSleep)
{
	if (!HasAuthority())
	{
		return;
	}
	bDevPuppet = bSleep;
	DevWalk = FVector::ZeroVector;
	FolkLook = FVector::ZeroVector;
	if (IsSeated())
	{
		SetSitHow(bSleep ? 1 : (Fig == HumanFig::Grandma ? 0 : 2));
	}
	else if (bSleep)
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void AGrandmaCharacter::DirectorSweep(const FVector& Point)
{
	if (!HasAuthority())
	{
		return;
	}
	const bool bWasPuppet = bDevPuppet;
	bDevPuppet = true;   // her own head stays out of it for the stroke
	auto Go = [this, Point]()
	{
		const FVector Me = GetActorLocation();
		const FVector To = Flat2(Point - Me);
		const float D = (float)To.Size();
		const FVector Dir = D > 1.f ? To / D : GetActorForwardVector();
		if (D > GranSweepFar - 300.f || D < GranSweepNear)
		{
			const FVector At = Point - Dir * 1300.f;
			SetActorLocation(FloorPoint(FVector(At.X, At.Y, 0.f)), false, nullptr, ETeleportType::TeleportPhysics);
		}
		const FRotator R(0.f, (float)Dir.Rotation().Yaw, 0.f);
		SetActorRotation(R);
		if (Controller) { Controller->SetControlRotation(R); }
		NextSweep = 0.f;
		TrySweep(Point);
	};
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	float Wait = 0.f;
	if (IsSeated())
	{
		GetUp();
		Wait = HumanAct::SitTime * HumanBody::Style(Fig).SitSlow + 0.2f;
	}
	FTimerHandle T1, T2;
	if (Wait > 0.f) { GetWorldTimerManager().SetTimer(T1, FTimerDelegate::CreateWeakLambda(this, Go), Wait, false); }
	else { Go(); }
	GetWorldTimerManager().SetTimer(T2, FTimerDelegate::CreateWeakLambda(this, [this, bWasPuppet]() { bDevPuppet = bWasPuppet; }), Wait + 3.f, false);
}

void AKidCharacter::DirectorGrab(AAntCharacter* Ant)
{
	if (!HasAuthority() || !Ant)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	bDevPuppet = false;
	if (IsSeated())
	{
		GetUp();
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Quarry = Ant;
	SeenSince = Now - 2.f;
	LastSeen = Now;
	NextGrab = 0.f;
	DevWant = 2;   // he picks it up (no stomping, no poking)
	SetMode(Chase, 10.f);
	FTimerHandle T;
	GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateWeakLambda(this, [this]() { DevWant = -1; }), 12.f, false);
	UE_LOG(LogTemp, Display, TEXT("ANTKID director: goes to pick up %s"), *Ant->GetDisplayName());
}
