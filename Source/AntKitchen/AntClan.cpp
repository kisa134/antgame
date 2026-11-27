#include "AntClan.h"
#include "AntRaid.h"
#include "AntRoutes.h"
#include "AntRun.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "NpcBody.h"
#include "KitchenLoot.h"
#include "KitchenMover.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "AntLab.h"
#include "LevelV2.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor ClanBad(1.f, 0.42f, 0.25f);
	const FLinearColor ClanInfo(0.9f, 0.9f, 0.9f);
	const FVector HillAt(12800.f, -10700.f, 0.f);

	const TCHAR* PlanName(uint8 P)
	{
		static const TCHAR* N[] = { TEXT("idle"), TEXT("scouting"), TEXT("fetch"), TEXT("haul"), TEXT("chase"), TEXT("guard"), TEXT("give up") };
		return N[FMath::Min<int32>(P, 6)];
	}
	const TCHAR* AmbushName(uint8 A)
	{
		static const TCHAR* N[] = { TEXT("home"), TEXT("staging"), TEXT("hidden"), TEXT("tell"), TEXT("sprung"), TEXT("retreat") };
		return N[FMath::Min<int32>(A, 5)];
	}
	bool HeavyContainer(int32 Index)
	{
		return Index == KitchenContainers::Fridge || Index == KitchenContainers::Freezer || Index == KitchenContainers::Oven
			|| Index == KitchenContainers::Dishwasher || Index == KitchenContainers::Microwave;
	}
	/** A place to stand around a point: the k-th of n on a ring. */
	FVector Ring(const FVector& C, int32 k, int32 n, float R)
	{
		const float A = (k + 0.35f) * 2.f * PI / FMath::Max(1, n);
		return C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R;
	}
}

// =============================================================================================
// One red ant
// =============================================================================================

AAntClanBot::AAntClanBot()
{
	bShy = true;   // the clan keeps away from the Human: no shooting at him, no climbing him
}

AAntCharacter* AAntClanBot::Ant() const
{
	AAntCharacter* A = Cast<AAntCharacter>(GetPawn());
	return (A && !A->bDead) ? A : nullptr;
}

void AAntClanBot::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AAntCharacter* A = Cast<AAntCharacter>(InPawn))
	{
		if (HealthMul > 1.01f)
		{
			A->MaxHealth *= HealthMul;
			A->Health = A->MaxHealth;
		}
		ProgressAt = A->GetActorLocation();
	}
	// Hatched again at the hill: whatever it was doing, it starts over (the clan gives it its place at the next think).
	Way.Reset();
	WayAt = 0;
	Prey = nullptr;
	Stance = ClanStance::Free;
	ProgressTime = GetWorld()->GetTimeSeconds();
	StuckCount = 0;
}

void AAntClanBot::GoAlong(const TArray<FVector>& Points, uint8 NewStance)
{
	Way = Points;
	WayAt = 0;
	Stance = NewStance;
	StuckCount = 0;
	BestToGo = 1.e9f;
	NearCakeSince = 0.f;
	ProgressTime = GetWorld()->GetTimeSeconds();
	if (AAntCharacter* A = Ant())
	{
		// On the mound (it hatches in the crater on top; a groove runs round the foot): it comes out of the hill's side, at the
		// foot, towards where it has to go.
		const FVector Hill = AKitchenBuilder::NestLocation(1);
		if (Way.Num() > 0 && FVector::Dist2D(A->GetActorLocation(), Hill) < 860.f && FVector::Dist2D(Way[0], Hill) > 1000.f)
		{
			const FVector Dir = (Way[0] - Hill).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
			A->ApplyTeleport(Hill + Dir * 930.f + Side * FMath::FRandRange(-160.f, 160.f) + FVector(0.f, 0.f, 20.f));
		}
		ProgressAt = A->GetActorLocation();
	}
}

void AAntClanBot::GoTo(const FVector& From, const FVector& To, uint8 NewStance, bool bSafe)
{
	TArray<FVector> Pts;
	AntRoutes::PathPoints(From, To, bSafe, Pts);
	GoAlong(Pts, NewStance);
}

bool AAntClanBot::FollowWay(AAntCharacter* Me)
{
	const FVector Here = Me->GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();
	while (WayAt < Way.Num() && FVector::Dist2D(Here, Way[WayAt]) < 170.f && FMath::Abs(Here.Z - Way[WayAt].Z) < 500.f)
	{
		++WayAt;
		StuckCount = 0;
		BestToGo = 1.e9f;
		ProgressTime = Now;
	}
	if (WayAt >= Way.Num())
	{
		return false;
	}
	const float ToGo = (float)FVector::Dist(Here, Way[WayAt]);
	if (ToGo < BestToGo - 100.f)
	{
		BestToGo = ToGo;
		ProgressAt = Here;
		ProgressTime = Now;
	}
	else if (Now - ProgressTime > 8.f)
	{
		// Not getting anywhere: a hop first; if that does not help, it is put on the next point of its way.
		ProgressTime = Now;
		if (++StuckCount >= 2)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN unstuck: %s put on its way at %s (was at %s)"), *Me->GetDisplayName(), *Way[WayAt].ToCompactString(), *Here.ToCompactString());
			Me->ApplyTeleport(Way[WayAt] + FVector(0.f, 0.f, 30.f));
			StuckCount = 0;
			BestToGo = 1.e9f;
		}
		else
		{
			Me->Jump();
		}
	}
	Goal = Way[WayAt] + FVector(0.f, 0.f, 30.f);
	bHasGoal = true;
	return true;
}

void AAntClanBot::ClanThink(AAntCharacter* Me)
{
	AAntClan* K = Clan.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector Here = Me->GetActorLocation();
	AKitchenLoot* C = K->Cake();
	ShootTarget = PickShootTarget(Me);
	if (Me->Carrying && Me->Carrying != C)
	{
		Me->LetGo();   // on duty: no crumbs
	}
	// Self-defence: whoever comes into its jaws gets bitten.
	if (const AAntCharacter* E = Cast<AAntCharacter>(ShootTarget.Get()))
	{
		if (FVector::Dist(E->GetActorLocation(), Here) < 55.f) { Me->BotBite(); }
	}
	FVector Away;
	if (Stance != ClanStance::Rush && Stance != ClanStance::Carry && K->Avoid(Me, Away))
	{
		// Something big is too close: back off first.
		Goal = Here + Away * 700.f;
		bHasGoal = true;
		ProgressTime = Now;
		return;
	}
	switch (Stance)
	{
	case ClanStance::Travel:
	case ClanStance::Flee:
		if (!FollowWay(Me)) { bHasGoal = false; }
		break;
	case ClanStance::Fetch:
	case ClanStance::Carry:
		if (!C)
		{
			bHasGoal = false;
		}
		else if (Me->Carrying == C)
		{
			Goal = Stance == ClanStance::Carry ? K->HaulGoal() + FVector(0.f, 0.f, 30.f) : Here;
			bHasGoal = Stance == ClanStance::Carry && !K->HaulGoal().IsZero();
			NearCakeSince = 0.f;
		}
		else if (FVector::Dist2D(Here, C->GetActorLocation()) > 700.f && FollowWay(Me))
		{
			// still on the way to it
		}
		else if (FVector::Dist2D(Here, C->GetActorLocation()) > 900.f)
		{
			// far from it and with no way laid (hatched again at the hill): along the house's routes, like anybody
			const uint8 Keep = Stance;
			GoTo(Here, FVector(C->GetActorLocation().X, C->GetActorLocation().Y, 0.f), Keep);
			NearCakeSince = 0.f;
			if (!FollowWay(Me)) { Goal = C->GetActorLocation(); bHasGoal = true; }
		}
		else
		{
			Goal = C->GetActorLocation();
			bHasGoal = true;
			if (FVector::Dist(C->GetActorLocation(), Here) < AAntCharacter::InteractRadius + C->GetRadius() * 0.5f + 12.f)
			{
				Me->GrabLoot(C);
			}
			// Close to it and still not holding it (it lies on a shelf above, in a cabinet whose floor is over the ant's head):
			// after a few seconds it is put next to it. (An ant would find the way up; this one does not always.)
			if (NearCakeSince <= 0.f) { NearCakeSince = Now; }
			else if (Now - NearCakeSince > 7.f && Me->Carrying != C)
			{
				NearCakeSince = Now;
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN unstuck: %s put next to the cake (was %.0f from it, %.0f below)"), *Me->GetDisplayName(), FVector::Dist2D(Here, C->GetActorLocation()), C->GetActorLocation().Z - Here.Z);
				Me->ApplyTeleport(C->GetActorLocation() + FVector(FMath::FRandRange(-55.f, 55.f), FMath::FRandRange(-55.f, 55.f), 22.f));
			}
		}
		break;
	case ClanStance::Escort:
	{
		const FVector Around = C ? C->GetActorLocation() : Here;
		const float Ang = (GetUniqueID() % 12) * 0.5236f;
		const FVector Want = Around + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 150.f;
		if (FVector::Dist2D(Here, Around) > 900.f && FollowWay(Me))
		{
			break;
		}
		Goal = Want;
		bHasGoal = FVector::Dist2D(Here, Want) > 45.f;
		break;
	}
	case ClanStance::Hide:
		if (FVector::Dist2D(Here, Spot) > 500.f && FollowWay(Me))
		{
			break;
		}
		Goal = Spot + FVector(0.f, 0.f, 20.f);
		bHasGoal = FVector::Dist2D(Here, Spot) > 60.f;
		break;
	case ClanStance::Rush:
	{
		const AAntCharacter* P = Cast<AAntCharacter>(Prey.Get());
		if (!P || P->bDead)
		{
			bHasGoal = false;
			break;
		}
		ShootTarget = Prey;
		const float D = (float)FVector::Dist(P->GetActorLocation(), Here);
		Goal = P->GetActorLocation();
		bHasGoal = true;
		if (D < 60.f)
		{
			Me->BotBite();
		}
		else if (D < 650.f && D > 150.f && Now > NextDash)
		{
			NextDash = Now + 2.6f;
			Me->Dash();   // a ram: knocks the load out of the jaws
		}
		break;
	}
	default:
		bHasGoal = false;
		break;
	}
}

void AAntClanBot::Tick(float DeltaSeconds)
{
	AAntCharacter* Me = Cast<AAntCharacter>(GetPawn());
	const float Now = GetWorld()->GetTimeSeconds();
	if (Stance == ClanStance::Free || Now < FreeUntil || !Clan.IsValid() || !Me || Me->bDead || Me->IsAttached())
	{
		bNoStrafe = false;
		Super::Tick(DeltaSeconds);   // a fire ant like any other: forage, steal, fight
		return;
	}
	AAIController::Tick(DeltaSeconds);
	bNoStrafe = true;
	ClanTimer -= DeltaSeconds;
	if (ClanTimer <= 0.f)
	{
		ClanTimer = 0.25f;
		ClanThink(Me);
	}
	AAntCharacter* Lead = (Stance == ClanStance::Carry && Me->Carrying && Me->Carrying == Clan->Cake()) ? Clan->HaulLeader() : nullptr;
	FVector Bite;
	if (Lead && Lead != Me)
	{
		// Somebody else leads the load: it walks under its own bite point, the way a bot carries with a player.
		CarryLeader = Lead;
		CarryFollow(Me);
	}
	else if (Lead == Me && Me->Carrying->NumCarriers() > 1 && Me->Carrying->GrabPointOf(Me, Bite) && FVector::Dist2D(Bite, Me->GetHoldPoint()) > 60.f
		&& FMath::Fmod(Now + (GetUniqueID() % 7), 3.f) < 1.2f)
	{
		// The leader lets the load catch up with its jaws (the others are bringing it) - a moment at a time: standing for good
		// it left the piece on a shelf with everybody under it.
	}
	else if (bHasGoal)
	{
		Steer(Me, Goal, DeltaSeconds);
	}
	Shoot(Me, DeltaSeconds);
}

// =============================================================================================
// The clan
// =============================================================================================

AAntClan::AAntClan()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AAntClan::Say(const FString& Text, bool bBad) const
{
	if (GM) { GM->Broadcast(Text, bBad ? ClanBad : ClanInfo); }
}

AKitchenLoot* AAntClan::Cake() const
{
	return GM ? GM->MainObject.Get() : nullptr;
}

void AAntClan::Alive(uint8 Job, TArray<AAntClanBot*>& Out) const
{
	for (AAntClanBot* B : Bots)
	{
		if (IsValid(B) && B->Job == Job && B->Ant()) { Out.Add(B); }
	}
}

void AAntClan::SetPlan(uint8 NewPlan, const TCHAR* Why)
{
	if (NewPlan != Plan)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLAN plan: %s -> %s (%s) at %s"), PlanName(Plan), PlanName(NewPlan), Why, Raid ? *Raid->Clock() : TEXT("-"));
		Plan = NewPlan;
		PlanSince = GetWorld()->GetTimeSeconds();
	}
}

void AAntClan::SpawnRoster()
{
	const AntTier::FDef& T = Raid->TierDef();
	// More of us, more of them: up to four players an escort for every player after the first and an ambusher for every second
	// one; from five to eight an escort for every second player more and an ambusher for every third (8 players: +5, +3).
	const int32 Players = FMath::Clamp(GM ? GM->GetNumPlayers() : 1, 1, 8);
	const int32 Over4 = FMath::Max(0, Players - 4);
	const int32 Want[ClanRole::Count] = { T.Scouts, T.Haulers, T.Escorts + (FMath::Min(Players, 4) - 1) + (Over4 + 1) / 2, T.Ambushers + FMath::Min(Players, 4) / 2 + Over4 / 3 };
	static const TCHAR* RoleNames[ClanRole::Count] = { TEXT("Разведчик"), TEXT("Носильщик"), TEXT("Охранник"), TEXT("Засадник") };
	int32 Have[ClanRole::Count] = { 0, 0, 0, 0 };
	// Those of the raid before stay; a smaller tier sends the extra ones home for good.
	for (int32 i = Bots.Num() - 1; i >= 0; --i)
	{
		AAntClanBot* B = Bots[i];
		if (!IsValid(B)) { Bots.RemoveAt(i); continue; }
		if (Have[B->Job] < Want[B->Job]) { ++Have[B->Job]; continue; }
		if (APawn* P = B->GetPawn()) { B->UnPossess(); P->Destroy(); }
		B->Destroy();
		Bots.RemoveAt(i);
	}
	int32 Serial = Bots.Num();
	for (uint8 Job = 0; Job < ClanRole::Count; ++Job)
	{
		while (Have[Job] < Want[Job])
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AAntClanBot* B = GetWorld()->SpawnActor<AAntClanBot>(AAntClanBot::StaticClass(), FTransform::Identity, P);
			if (!B) { break; }
			const int32 k = Have[Job]++;
			B->Job = Job;
			B->Clan = this;
			const EAntClass Cls = Job == ClanRole::Scout ? EAntClass::Scout : Job == ClanRole::Escort ? EAntClass::Soldier : (k % 2 == 0 ? EAntClass::Soldier : EAntClass::Worker);
			if (AAntPlayerState* PS = B->GetPlayerState<AAntPlayerState>())
			{
				PS->Team = 1;
				PS->AntClass = Cls;
				PS->AntColor = AntKit::TeamColor(1, Serial++);
				PS->SetPlayerName(FString::Printf(TEXT("[красный] %s %d"), RoleNames[Job], k + 1));
			}
			GM->RestartPlayer(B);
			Bots.Add(B);
		}
	}
	for (AAntClanBot* B : Bots)
	{
		B->Clan = this;
		if (!FMath::IsNearlyEqual(B->HealthMul, T.ClanHealth))
		{
			B->HealthMul = T.ClanHealth;
			if (AAntCharacter* A = B->Ant())
			{
				A->MaxHealth = A->GetClassDef().Health * T.ClanHealth;
				A->Health = A->MaxHealth;
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCLAN roster for tier %d, %d players: scouts %d, haulers %d, escorts %d, ambushers %d (bots %d), first out at %.0f s, respawn %.0f s, health x%.2f"),
		(int32)Raid->Tier, Players, Want[0], Want[1], Want[2], Want[3], Bots.Num(), T.ClanFirst, T.ClanRespawn, T.ClanHealth);
}

void AAntClan::Begin(AAntKitchenGameMode* InGM, AAntRaid* InRaid)
{
	GM = InGM;
	Raid = InRaid;
	if (!GM || !Raid)
	{
		return;
	}
	SpawnRoster();
	Plan = Idle;
	PlanSince = GetWorld()->GetTimeSeconds();
	bKnowsCake = false;
	bSaidSealed = false;
	HaulWay.Reset();
	HaulAt = 0;
	Nudges = 0;
	FirstGripAt = 0.f;
	SeenTime = -1000.f;
	bSeenWithCake = false;
	Ambush = AHome;
	AmbushNode = -1;
	AmbushSince = PlanSince;
	AmbushCount = 0;
	DecoyUntil = 0.f;
	for (AAntClanBot* B : Bots)
	{
		B->Stance = ClanStance::Free;
		B->FreeUntil = 0.f;
		B->Way.Reset();
		B->ScoutSpot = 0;
	}
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AAntClan::Think, 0.5f, true);
}

void AAntClan::Stop()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	for (AAntClanBot* B : Bots)
	{
		if (IsValid(B)) { B->Stance = ClanStance::Free; }
	}
	if (Raid) { Raid->AmbushState = 0; Raid->ClanAlert = 0; }
	UE_LOG(LogTemp, Display, TEXT("ANTCLAN stop: plan %s, ambushes sprung %d, nudges %d"), PlanName(Plan), AmbushCount, Nudges);
}

bool AAntClan::Avoid(const AAntCharacter* A, FVector& OutAway) const
{
	const FVector P = A->GetActorLocation();
	if (AHumanCharacter::InCover(GetWorld(), P))
	{
		return false;   // where the big ones cannot reach
	}
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		const AHumanCharacter* H = *It;
		const bool bDad = H->Fig == HumanFig::Dad;
		if (H->IsKnockedOut() || (bDad && GS && GS->ShiftStage == 0))
		{
			continue;   // on his phone
		}
		const float R = bDad ? 1500.f : 850.f;
		if (FVector::Dist2D(H->GetActorLocation(), P) < R && P.Z < 500.f)
		{
			OutAway = (P - H->GetActorLocation()).GetSafeNormal2D();
			return true;
		}
	}
	for (TActorIterator<ACritter> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), P) < 750.f && FMath::Abs(It->GetActorLocation().Z - P.Z) < 700.f)
		{
			OutAway = (P - It->GetActorLocation()).GetSafeNormal2D();
			return true;
		}
	}
	return false;
}

AAntCharacter* AAntClan::PickPrey(const FVector& Near, float Range, AKitchenLoot* C) const
{
	AAntCharacter* Best = nullptr;
	float BestScore = 1.e9f;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Team != 0 || A->Creature != 0 || AAntRaid::IsHome(A))
		{
			continue;
		}
		const float D = (float)FVector::Dist(A->GetActorLocation(), Near);
		if (D > Range)
		{
			continue;
		}
		float Score = D;
		if (C && A->Carrying == C) { Score -= 3000.f; }
		else if (A->Carrying) { Score -= 900.f; }
		if (A->IsPlayerControlled()) { Score -= 400.f; }
		if (Score < BestScore) { BestScore = Score; Best = A; }
	}
	return Best;
}

void AAntClan::Sightings(AKitchenLoot* C, float Now)
{
	// What the clan has seen of the players: any of its ants with a clear line to one of ours in the open.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		const AAntCharacter* A = *It;
		if (!A || A->bDead || A->Team != 0 || A->Creature != 0 || !A->IsPlayerControlled() || AAntRaid::IsHome(A))
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		for (const AAntClanBot* B : Bots)
		{
			const AAntCharacter* R = IsValid(B) ? B->Ant() : nullptr;
			if (R && FVector::DistSquared(R->GetActorLocation(), P) < FMath::Square(2200.f) && NpcWorld::CanSee(GetWorld(), R->GetActorLocation() + FVector(0.f, 0.f, 8.f), P + FVector(0.f, 0.f, 8.f)))
			{
				SeenAt = P;
				SeenTime = Now;
				bSeenWithCake = C && A->Carrying == C;
				break;
			}
		}
	}
}

void AAntClan::PlaceCake(AKitchenLoot* C, const FVector& At)
{
	// (Moved with ants holding it, their bodies ended up inside the piece and the physics threw it across the room.)
	TArray<AAntCharacter*> Holders = C->Carriers;
	C->ReleaseAll();
	if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(C->GetRootComponent()))
	{
		P->SetPhysicsLinearVelocity(FVector::ZeroVector);
		P->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	C->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	const float R = C->GetRadius() * 1.5f + 80.f;
	for (int32 i = 0; i < Holders.Num(); ++i)
	{
		if (IsValid(Holders[i]) && !Holders[i]->bDead && Holders[i]->Team == 1)
		{
			const FVector Spot = Ring(At, i, Holders.Num(), R);
			Holders[i]->ApplyTeleport(FVector(Spot.X, Spot.Y, At.Z - C->GetRadius() + 20.f));
		}
	}
}

void AAntClan::OpenFor(int32 Box, const FVector& At)
{
	// A cabinet with two sliding doors (the TV stand): only the one in front of the cake. Slid open both, they change places
	// and the front is shut again.
	const KitchenContainers::FDef& D = KitchenContainers::Get(Box);
	AKitchenMover* Near = nullptr;
	AKitchenMover* Other = nullptr;
	if (D.Movers[0] >= 0 && D.Movers[1] >= 0)
	{
		for (TActorIterator<AKitchenMover> It(GetWorld()); It; ++It)
		{
			if (It->PartIndex != D.Movers[0] && It->PartIndex != D.Movers[1]) { continue; }
			if (!Near || FVector::Dist2D(It->GetActorLocation(), At) < FVector::Dist2D(Near->GetActorLocation(), At)) { Other = Near; Near = *It; }
			else { Other = *It; }
		}
	}
	if (Near)
	{
		if (!Near->IsOpen()) { Near->SetDoorOpen(true); }
		if (Other && Other->IsOpen()) { Other->SetDoorOpen(false); }
	}
	else
	{
		KitchenContainers::SetOpen(GetWorld(), Box, true);
	}
}

void AAntClan::BuildHaulWay(const FVector& From)
{
	HaulWay.Reset();
	HaulAt = 0;
	LowerIdx = -1;
	LowerSince = 0.f;
	LastFloorAt = FVector::ZeroVector;
	FVector Floor = From;
	if (From.Z > 500.f && From.X > 12400.f && From.X < 15200.f && From.Y > -4300.f && From.Y < -1800.f)
	{
		// On the coffee table: down the stair of toy blocks at its west end, a step at a time (a fall from the edge would smash it).
		HaulWay.Add(FVector(12620.f, -3000.f, From.Z));
		HaulWay.Add(FVector(12370.f, -3000.f, 975.f));
		HaulWay.Add(FVector(12120.f, -3000.f, 735.f));
		HaulWay.Add(FVector(11870.f, -3000.f, 495.f));
		HaulWay.Add(FVector(11620.f, -3000.f, 255.f));
		HaulWay.Add(FVector(11330.f, -3000.f, 10.f));
		Floor = HaulWay.Last();
		LowerIdx = HaulWay.Num() - 1;
	}
	const int32 Box = KitchenContainers::At(From);
	if (Box >= 0)
	{
		// Inside a cabinet (the TV stand, the fridge): out through its front first. The front is the side its nearest
		// junction of the house lies on (the TV stand opens into the room, to the north).
		const KitchenContainers::FDef& D = KitchenContainers::Get(Box);
		const FVector Mid = (D.Min + D.Max) * 0.5f;
		FVector Dir(0.f, 1.f, 0.f);
		if (Box != KitchenContainers::TVStand)
		{
			const FVector To = AntRoutes::Point(AntRoutes::Nearest(Mid)) - Mid;
			Dir = FMath::Abs(To.X) > FMath::Abs(To.Y) ? FVector(FMath::Sign(To.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(To.Y), 0.f);
		}
		const float Out = (Dir.X != 0.f ? (D.Max.X - D.Min.X) : (D.Max.Y - D.Min.Y)) * 0.5f + 320.f;
		const FVector Door(Dir.X != 0.f ? Mid.X + Dir.X * Out : From.X, Dir.Y != 0.f ? Mid.Y + Dir.Y * Out : From.Y, From.Z);
		HaulWay.Add(Door);
		HaulWay.Add(FVector(Door.X + Dir.X * 150.f, Door.Y + Dir.Y * 150.f, 10.f));
		Floor = HaulWay.Last();
		LowerIdx = HaulWay.Num() - 1;
	}
	TArray<FVector> Pts;
	AntRoutes::PathPoints(Floor, HillAt, true, Pts);
	if (Floor.X < 13900.f && Floor.Y < -6800.f && Floor.Y > -7750.f && Pts.Num() > 0 && Pts[0].X > 15000.f)
	{
		// From the front of the TV stand to the living-room window: round the stand's corner, not into it (an ant steering at
		// a point takes a wall for a way and crawls up it, cake and all).
		HaulWay.Add(FVector(14250.f, -7200.f, 10.f));
	}
	HaulWay.Append(Pts);
	NudgeFails = 0;
	HaulProgressAt = From;
	HaulProgressTime = GetWorld()->GetTimeSeconds();
	FString Line;
	for (const FVector& P : HaulWay) { Line += FString::Printf(TEXT(" (%.0f,%.0f,%.0f)"), P.X, P.Y, P.Z); }
	UE_LOG(LogTemp, Display, TEXT("ANTCLAN haul way, %d points:%s"), HaulWay.Num(), *Line);
}

int32 AAntClan::ChooseAmbushNode(AKitchenLoot* C, float Now) const
{
	// Where will they come home from? What the clan saw last (if it is fresh), else where the cake is, else the middle of the living room.
	const bool bFresh = Now - SeenTime < 100.f;
	FVector Ref = bFresh ? SeenAt : AntLab::Get() ? AntLab::Get()->Cake : FVector(13750.f, -4600.f, 0.f);   // (KAN-40, a flat: where its cake lies)
	if (!bFresh && bKnowsCake && C) { Ref = C->GetActorLocation(); }
	if (Raid->Cake == AntCake::Ours && C && bFresh) { Ref = C->GetActorLocation(); }
	// Both ways home from there; the players take the quicker one (so the clan reckons).
	const int32 From = AntRoutes::Nearest(Ref);
	TArray<int32> P1, P2;
	float C1 = 1.e9f, C2 = 1.e9f;
	AntRoutes::Path(From, AntRoutes::NestNode(), false, P1, &C1, AntRoutes::UseFlap);
	AntRoutes::Path(From, AntRoutes::SouthNode(), false, P2, &C2, AntRoutes::UseFlap);
	const TArray<int32>& P = C2 < C1 ? P2 : P1;
	for (int32 i = 1; i < P.Num(); ++i)
	{
		if (AntRoutes::IsJunction(P[i])) { return P[i]; }
	}
	for (int32 i = 0; i < P.Num(); ++i)
	{
		if (AntRoutes::IsJunction(P[i])) { return P[i]; }
	}
	return AntRoutes::Find(TEXT("Порог"));
}

void AAntClan::CakeThink(AKitchenLoot* C, float Now)
{
	const AntTier::FDef& T = Raid->TierDef();
	const float RT = Raid->RaidTime();
	TArray<AAntClanBot*> Scouts, Haulers, Escorts;
	Alive(ClanRole::Scout, Scouts);
	Alive(ClanRole::Hauler, Haulers);
	Alive(ClanRole::Escort, Escorts);
	const uint8 CS = Raid->Cake;
	const FVector CP = C ? C->GetActorLocation() : FVector::ZeroVector;
	if ((CS == AntCake::Home || CS == AntCake::Eaten || CS == AntCake::Broken || !C) && Plan != GiveUp)
	{
		SetPlan(GiveUp, TEXT("the cake is out of the game"));
	}
	auto FreeAll = [&]()
	{
		for (AAntClanBot* B : Scouts) { B->Stance = ClanStance::Free; }
		for (AAntClanBot* B : Haulers) { B->Stance = ClanStance::Free; }
		for (AAntClanBot* B : Escorts) { B->Stance = ClanStance::Free; }
	};
	auto EscortAll = [&]()
	{
		for (AAntClanBot* B : Scouts) { if (B->Stance != ClanStance::Escort) { B->GoTo(B->Ant()->GetActorLocation(), CP, ClanStance::Escort); } }
		for (AAntClanBot* B : Escorts) { if (B->Stance != ClanStance::Escort) { B->GoTo(B->Ant()->GetActorLocation(), CP, ClanStance::Escort); } }
	};

	// --- Do they know where it is? Any of theirs that comes close to it does. ---
	if (C && !bKnowsCake && Plan <= Scouting && CS != AntCake::InHill)
	{
		for (const AAntClanBot* B : Bots)
		{
			const AAntCharacter* A = IsValid(B) ? B->Ant() : nullptr;
			if (!A) { continue; }
			const float D = (float)FVector::Dist2D(A->GetActorLocation(), CP);
			if (C->IsCarried() && C->CarryTeam() == 0)
			{
				if (D < 2200.f && NpcWorld::CanSee(GetWorld(), A->GetActorLocation() + FVector(0.f, 0.f, 8.f), CP)) { bKnowsCake = true; }
			}
			else if (D < 900.f && FMath::Abs(A->GetActorLocation().Z - CP.Z) < 1300.f)
			{
				const int32 Sealed = KitchenContainers::SealedAt(GetWorld(), CP);
				if (Sealed >= 0 && !HeavyContainer(Sealed))
				{
					// A light door (a sliding door, a drawer): one ant opens it, same as for the players.
					OpenFor(Sealed, CP);
					UE_LOG(LogTemp, Display, TEXT("ANTCLAN %s opened the door %s"), *A->GetDisplayName(), KitchenContainers::Get(Sealed).Where);
				}
				bKnowsCake = true;
			}
			if (bKnowsCake)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN found the cake: %s at %s, %.0f from it, raid time %s"), *A->GetDisplayName(), *CP.ToCompactString(), D, *Raid->Clock());
				break;
			}
		}
	}

	switch (Plan)
	{
	case Idle:
		FreeAll();
		if (RT >= T.ClanFirst && C)
		{
			SetPlan(Scouting, TEXT("time"));
			Raid->ClanAlert = FMath::Max<uint8>(Raid->ClanAlert, 1);
			Say(TEXT("Красные разведчики вышли из холма: тоже ищут торт"));
		}
		break;

	case Scouting:
	{
		for (AAntClanBot* B : Haulers) { B->Stance = ClanStance::Free; }
		for (AAntClanBot* B : Escorts) { B->Stance = ClanStance::Free; }
		// Each scout checks the three places in its own order (the nearest to their crack first, the others after it).
		const TConstArrayView<FLevelV2Main> Spots = LevelV2::MainSpots();
		static const int32 Order[3] = { 1, 0, 2 };   // the TV stand, the coffee table, the fridge
		for (int32 i = 0; i < Scouts.Num(); ++i)
		{
			AAntClanBot* B = Scouts[i];
			if (B->Stance != ClanStance::Travel || B->Arrived())
			{
				const int32 Which = Order[(B->ScoutSpot + i) % 3] % FMath::Max(1, Spots.Num());
				B->ScoutSpot++;
				const FLevelV2Main& S = Spots[Which];
				B->GoTo(B->Ant()->GetActorLocation(), FVector(S.X, S.Y, 0.f), ClanStance::Travel);
			}
		}
		if (bKnowsCake)
		{
			const int32 Sealed = KitchenContainers::SealedAt(GetWorld(), CP);
			if (Sealed >= 0 && HeavyContainer(Sealed) && Raid->Tier <= 1)
			{
				if (!bSaidSealed)
				{
					bSaidSealed = true;
					Say(FString::Printf(TEXT("Красные учуяли торт %s, но дверь им не по зубам: будут ждать вас на выходе"), KitchenContainers::Get(Sealed).Where));
				}
				SetPlan(GiveUp, TEXT("the cake is behind a heavy door"));
				break;
			}
			Raid->ClanAlert = 2;
			Say(TEXT("КРАСНЫЕ НАШЛИ ТОРТ: их отряд идёт за ним!"));
			Raid->Note(FString::Printf(TEXT("%s — красные нашли торт"), *Raid->Clock()));
			FirstGripAt = 0.f;
			SetPlan(CS == AntCake::Ours ? Chase : Fetch, TEXT("found"));
			LostSightAt = Now;
		}
		break;
	}

	case Fetch:
	{
		Raid->ClanAlert = FMath::Max<uint8>(Raid->ClanAlert, 2);
		if (CS == AntCake::Ours) { SetPlan(Chase, TEXT("the players have it")); LostSightAt = Now; break; }
		if (CS == AntCake::InHill) { SetPlan(Guard, TEXT("it is in the hill")); break; }
		const int32 Sealed = KitchenContainers::SealedAt(GetWorld(), CP);
		if (Sealed >= 0)
		{
			// A heavy door: as many of them at it as it takes (strength 40, the same as for the players).
			float Have = 0.f;
			for (const AAntClanBot* B : Bots)
			{
				const AAntCharacter* A = IsValid(B) ? B->Ant() : nullptr;
				if (A && FVector::Dist2D(A->GetActorLocation(), CP) < 1000.f) { Have += A->GetStrength(); }
			}
			if (Have >= AntDoor::Need(HeavyContainer(Sealed) ? AntDoor::Heavy : AntDoor::Light))
			{
				OpenFor(Sealed, CP);
				Say(FString::Printf(TEXT("Красные открыли дверь (%s)!"), KitchenContainers::Get(Sealed).Where));
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN opened a door %s with strength %.0f"), KitchenContainers::Get(Sealed).Where, Have);
			}
		}
		for (AAntClanBot* B : Haulers)
		{
			if (B->Stance != ClanStance::Fetch) { B->GoTo(B->Ant()->GetActorLocation(), FVector(CP.X, CP.Y, 0.f), ClanStance::Fetch); }
		}
		EscortAll();
		if (C->IsCarried() && C->CarryTeam() == 1)
		{
			if (FirstGripAt <= 0.f) { FirstGripAt = Now; }
			if (C->CarryRatio() >= 1.f || Now - FirstGripAt > 8.f)
			{
				BuildHaulWay(CP);
				SetPlan(Haul, TEXT("they have hold of it"));
				Raid->Note(FString::Printf(TEXT("%s — красные взяли торт (%s)"), *Raid->Clock(), *Raid->PlaceName(CP)));
			}
		}
		else
		{
			FirstGripAt = 0.f;
		}
		break;
	}

	case Haul:
	{
		Raid->ClanAlert = 3;
		Raid->ClanPartyLoc = CP;
		if (CS == AntCake::Ours) { SetPlan(Chase, TEXT("the players took it")); LostSightAt = Now; break; }
		if (CS == AntCake::InHill) { SetPlan(Guard, TEXT("home")); break; }
		const bool bCarried = C->IsCarried() && C->CarryTeam() == 1;
		if (const int32 In = KitchenContainers::At(CP); In >= 0 && !HeavyContainer(In)) { OpenFor(In, CP); }
		if (LowerIdx >= 0)
		{
			// Still up where it lay. With a full grip held for six seconds it is on the floor in front (anybody can break the
			// grip meanwhile: a ram, a bite, a tug).
			if (CP.Z < 130.f) { LowerIdx = -1; }   // it is down already (the players dropped it there)
			else if (bCarried && C->CarryRatio() >= 1.f)
			{
				if (LowerSince <= 0.f) { LowerSince = Now; }
				else if (Now - LowerSince > 6.f && HaulWay.IsValidIndex(LowerIdx))
				{
					const FVector To = HaulWay[LowerIdx];
					const int32 Held = C->NumCarriers();
					PlaceCake(C, FVector(To.X, To.Y, To.Z + C->GetRadius() + 6.f));
					UE_LOG(LogTemp, Display, TEXT("ANTCLAN the cake is lowered to the floor at %s (from %s), %d carriers"), *To.ToCompactString(), *CP.ToCompactString(), Held);
					Say(TEXT("Красные спустили торт на пол и тащат к своей щели"), true);
					HaulAt = LowerIdx + 1;
					LowerIdx = -1;
					HaulProgressAt = To;
					HaulProgressTime = Now;
				}
			}
			else { LowerSince = 0.f; }
			for (AAntClanBot* B : Haulers)
			{
				const uint8 Want = B->Ant()->Carrying == C ? ClanStance::Carry : ClanStance::Fetch;
				if (B->Stance != Want) { B->Stance = Want; B->Way.Reset(); B->WayAt = 0; }
			}
			EscortAll();
			HaulProgressTime = Now;
			break;
		}
		// The leader has crawled up something with it (steering at a point, an ant takes a wall for a way): back to the floor.
		if (CP.Z < 135.f) { LastFloorAt = CP; }
		else if (CP.Z > 480.f && !LastFloorAt.IsZero() && bCarried && FVector::Dist2D(CP, LastFloorAt) < 900.f)
		{
			// (only while they hold it and near where it left the floor: a cake thrown by a stomp is no business of this)
			PlaceCake(C, LastFloorAt + FVector(0.f, 0.f, 6.f));
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN the cake went up a wall to %s: put back on the floor at %s"), *CP.ToCompactString(), *LastFloorAt.ToCompactString());
			++Nudges;
			break;
		}
		// One of the carriers leads (the slowest class; of those the one nearest the goal), the others walk with it.
		if (!Leader.IsValid() || Leader->bDead || Leader->Carrying != C)
		{
			Leader = nullptr;
			float Best = 1.e12f;
			for (AAntCharacter* A : C->Carriers)
			{
				if (!IsValid(A) || A->bDead || A->Team != 1) { continue; }
				const float Score = A->GetClassDef().Speed * 100000.f + (float)FVector::Dist2D(A->GetActorLocation(), HaulGoal());
				if (Score < Best) { Best = Score; Leader = A; }
			}
		}
		for (AAntClanBot* B : Haulers)
		{
			const uint8 Want = B->Ant()->Carrying == C ? ClanStance::Carry : ClanStance::Fetch;
			if (B->Stance != Want) { B->Stance = Want; B->Way.Reset(); B->WayAt = 0; }
		}
		// With too few haulers alive the escorts take hold as well.
		if (Haulers.Num() < 3)
		{
			for (AAntClanBot* B : Escorts)
			{
				const uint8 Want = B->Ant()->Carrying == C ? ClanStance::Carry : ClanStance::Fetch;
				if (B->Stance != Want) { B->Stance = Want; B->Way.Reset(); B->WayAt = 0; }
			}
			for (AAntClanBot* B : Scouts) { if (B->Stance != ClanStance::Escort) { B->GoTo(B->Ant()->GetActorLocation(), CP, ClanStance::Escort); } }
		}
		else
		{
			EscortAll();
		}
		while (HaulAt < HaulWay.Num() - 1 && FVector::Dist2D(CP, HaulWay[HaulAt]) < 230.f && FMath::Abs(CP.Z - HaulWay[HaulAt].Z) < 380.f)
		{
			++HaulAt;
		}
		if (FVector::Dist(CP, HaulProgressAt) > 90.f || !bCarried)
		{
			HaulProgressAt = CP;
			HaulProgressTime = Now;
		}
		else if (Now - HaulProgressTime > 9.f && HaulWay.IsValidIndex(HaulAt))
		{
			// The load is stuck on something (a rail, a rug's edge, a corner): it is put a step further along the way, carriers and all.
			// (Swept: never through a wall or the floor. First up a little, then along.)
			TArray<AAntCharacter*> Holders = C->Carriers;
			FHitResult Hit;
			C->SetActorLocation(CP + FVector(0.f, 0.f, 30.f), true, &Hit, ETeleportType::TeleportPhysics);
			const FVector Along = (HaulWay[HaulAt] - CP).GetSafeNormal2D() * FMath::Min(320.f, (float)FVector::Dist2D(HaulWay[HaulAt], CP));
			C->SetActorLocation(C->GetActorLocation() + Along, true, &Hit, ETeleportType::TeleportPhysics);
			FVector Delta = C->GetActorLocation() - CP;
			NudgeFails = Delta.Size2D() < 100.f ? NudgeFails + 1 : 0;
			if (NudgeFails >= 3)
			{
				// Walled in (a rail, a lip of the crack in the wall): it is lifted over, the way the crowd of them would in the end.
				C->SetActorLocation(C->GetActorLocation() + Along + FVector(0.f, 0.f, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
				Delta = C->GetActorLocation() - CP;
				NudgeFails = 0;
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN the cake is lifted over what held it at %s"), *CP.ToCompactString());
			}
			for (AAntCharacter* A : Holders)
			{
				if (IsValid(A) && Delta.Size2D() > 20.f) { A->ApplyTeleport(A->GetActorLocation() + FVector(Delta.X, Delta.Y, FMath::Max(0.f, (float)Delta.Z))); }
			}
			++Nudges;
			HaulProgressTime = Now;
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN nudge %d: the cake was stuck at %s, moved towards point %d of %d"), Nudges, *CP.ToCompactString(), HaulAt, HaulWay.Num());
		}
		break;
	}

	case Chase:
	{
		Raid->ClanAlert = 3;
		Raid->ClanPartyLoc = CP;
		if (CS == AntCake::Lying) { FirstGripAt = 0.f; SetPlan(Fetch, TEXT("it was dropped")); break; }
		if (CS == AntCake::Clan) { BuildHaulWay(CP); SetPlan(Haul, TEXT("won the tug")); break; }
		bool bNear = false;
		for (const AAntClanBot* B : Bots)
		{
			const AAntCharacter* A = IsValid(B) ? B->Ant() : nullptr;
			if (A && FVector::Dist(A->GetActorLocation(), CP) < 3000.f) { bNear = true; break; }
		}
		if (bNear) { LostSightAt = Now; }
		else if (Now - LostSightAt > 8.f)
		{
			Say(TEXT("Красные потеряли торт из виду"), false);
			SetPlan(GiveUp, TEXT("lost sight of it"));
			break;
		}
		AAntCharacter* P = PickPrey(CP, 700.f, C);
		for (AAntClanBot* B : Haulers) { B->Stance = ClanStance::Rush; B->Prey = P; }
		for (AAntClanBot* B : Escorts) { B->Stance = ClanStance::Rush; B->Prey = P; }
		for (AAntClanBot* B : Scouts) { B->Stance = ClanStance::Rush; B->Prey = P; }
		break;
	}

	case Guard:
	{
		Raid->ClanAlert = 4;
		if (CS == AntCake::Ours) { SetPlan(Chase, TEXT("the players raided the hill")); LostSightAt = Now; break; }
		if (CS != AntCake::InHill) { SetPlan(GiveUp, TEXT("nothing to guard")); break; }
		TArray<AAntClanBot*> All;
		for (AAntClanBot* B : Bots) { if (IsValid(B) && B->Ant()) { All.Add(B); } }
		AAntCharacter* P = PickPrey(HillAt, 1500.f, C);
		for (int32 i = 0; i < All.Num(); ++i)
		{
			AAntClanBot* B = All[i];
			if (P)
			{
				B->Stance = ClanStance::Rush;
				B->Prey = P;
			}
			else
			{
				B->Spot = Ring(HillAt, i, All.Num(), 720.f);
				if (B->Stance != ClanStance::Hide) { B->GoTo(B->Ant()->GetActorLocation(), B->Spot, ClanStance::Hide); }
			}
		}
		break;
	}

	default:   // GiveUp: forage, steal what lies about
		FreeAll();
		if (C && bKnowsCake && CS == AntCake::Lying && Now - PlanSince > 15.f && KitchenContainers::SealedAt(GetWorld(), CP) < 0)
		{
			FirstGripAt = 0.f;
			SetPlan(Fetch, TEXT("it lies in the open again"));
		}
		else if (C && CS == AntCake::Ours && bSeenWithCake && Now - SeenTime < 2.f)
		{
			bKnowsCake = true;
			LostSightAt = Now;
			SetPlan(Chase, TEXT("they see the players with it"));
		}
		break;
	}
}

void AAntClan::AmbushThink(AKitchenLoot* C, float Now)
{
	const AntTier::FDef& T = Raid->TierDef();
	TArray<AAntClanBot*> Party;
	Alive(ClanRole::Ambusher, Party);
	if (Plan == Guard)
	{
		// The cake is in the hill: everybody guards it (CakeThink places them).
		if (Ambush != AHome) { Ambush = AHome; Raid->AmbushState = 0; }
		return;
	}
	if (Now < DecoyUntil)
	{
		// A pheromone decoy: they take it for a trail of ours and sit on it.
		for (int32 i = 0; i < Party.Num(); ++i)
		{
			Party[i]->Spot = Ring(DecoyAt, i, Party.Num(), 180.f);
			if (Party[i]->Stance != ClanStance::Hide || FVector::Dist2D(Party[i]->Spot, DecoyAt) > 400.f) { Party[i]->GoTo(Party[i]->Ant()->GetActorLocation(), Party[i]->Spot, ClanStance::Hide); }
		}
		if (Ambush != AHome) { Ambush = AHome; Raid->AmbushState = 0; }
		AmbushSince = Now;
		return;
	}
	const FVector NodeAt = AmbushNode >= 0 ? AntRoutes::Point(AmbushNode) : HillAt;
	switch (Ambush)
	{
	case AHome:
		for (AAntClanBot* B : Party) { if (Now >= B->FreeUntil) { B->Stance = ClanStance::Free; } }
		if (Raid->RaidTime() >= T.ClanFirst * 0.6f && Now - AmbushSince > (AmbushCount > 0 ? 25.f : 0.f) && Party.Num() > 0)
		{
			AmbushNode = ChooseAmbushNode(C, Now);
			Ambush = AStaging;
			AmbushSince = Now;
			NextRechoose = Now + 30.f;
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush: the party of %d goes to «%s» (seen %.0f s ago at %s, with the cake %d) at %s"), Party.Num(), AntRoutes::Node(AmbushNode).Name,
				Now - SeenTime, *SeenAt.ToCompactString(), bSeenWithCake ? 1 : 0, *Raid->Clock());
		}
		break;

	case AStaging:
	case AHidden:
	{
		int32 There = 0;
		for (int32 i = 0; i < Party.Num(); ++i)
		{
			AAntClanBot* B = Party[i];
			B->Spot = Ring(NodeAt, i, Party.Num(), 300.f);
			if (B->Stance != ClanStance::Hide) { B->GoTo(B->Ant()->GetActorLocation(), B->Spot, ClanStance::Hide); }
			There += FVector::Dist2D(B->Ant()->GetActorLocation(), B->Spot) < 220.f ? 1 : 0;
		}
		if (Ambush == AHidden && There > 0 && Raid->AmbushState == 0 && Raid->Nest.Tier_(AntUp::Scouts) >= 2)
		{
			// Scouts II: the players' lookouts have found them where they sit.
			Raid->AmbushLoc = NodeAt + FVector(0.f, 0.f, 60.f);
			Raid->AmbushState = 3;
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush at «%s» is seen by the lookouts"), AntRoutes::Node(AmbushNode).Name);
		}
		if (Ambush == AStaging && (There >= FMath::Max(1, Party.Num() - 1) || Now - AmbushSince > 70.f) && There > 0)
		{
			Ambush = AHidden;
			AmbushSince = Now;
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush: hidden at «%s», %d of %d in place, at %s"), AntRoutes::Node(AmbushNode).Name, There, Party.Num(), *Raid->Clock());
		}
		if (Party.Num() == 0)
		{
			Ambush = AHome;
			AmbushSince = Now;
			break;
		}
		// A better place, by what they have seen since? (Not while somebody is close: no walking away in plain sight.)
		const AAntCharacter* Close = PickPrey(NodeAt, 2600.f, C);
		if (Now > NextRechoose && !Close)
		{
			NextRechoose = Now + 25.f;
			const int32 Better = ChooseAmbushNode(C, Now);
			if (Better != AmbushNode)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush: moving from «%s» to «%s» (seen %.0f s ago at %s, with the cake %d)"), AntRoutes::Node(AmbushNode).Name, AntRoutes::Node(Better).Name,
					Now - SeenTime, *SeenAt.ToCompactString(), bSeenWithCake ? 1 : 0);
				AmbushNode = Better;
				Ambush = AStaging;
				AmbushSince = Now;
				Raid->AmbushState = 0;
				for (AAntClanBot* B : Party) { B->Stance = ClanStance::Travel; }   // (Hide is set again above with the new spots)
				break;
			}
		}
		// The tell: somebody of ours walks into it. They rattle and hop where they sit, then they spring.
		if (Ambush == AHidden && There > 0)
		{
			const AAntCharacter* Victim = PickPrey(NodeAt, 1500.f, C);
			if (Victim && FMath::Abs(Victim->GetActorLocation().Z - NodeAt.Z) < 900.f)
			{
				Ambush = ATell;
				AmbushUntil = Now + (Raid->Tier >= 3 ? 2.2f : 2.8f);
				Raid->AmbushLoc = NodeAt + FVector(0.f, 0.f, 60.f);
				Raid->AmbushState = 1;
				AntSfx::Everyone(this, AntSfx::Hiss, NodeAt, 0.75f);
				for (AAntClanBot* B : Party) { if (AAntCharacter* A = B->Ant()) { A->Jump(); } }
				UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush TELL at «%s»: %s is %.0f away, %d waiting, at %s"), AntRoutes::Node(AmbushNode).Name, *Victim->GetDisplayName(),
					FVector::Dist(Victim->GetActorLocation(), NodeAt), There, *Raid->Clock());
			}
		}
		break;
	}

	case ATell:
		if (FMath::Fmod(Now, 1.f) < 0.5f)
		{
			AntSfx::Everyone(this, AntSfx::Hiss, NodeAt, 0.9f);
			for (AAntClanBot* B : Party) { if (AAntCharacter* A = B->Ant()) { A->Jump(); } }
		}
		if (Now >= AmbushUntil)
		{
			Ambush = ASprung;
			AmbushUntil = Now + 30.f;
			SprungWith = Party.Num();
			++AmbushCount;
			Raid->AmbushState = 2;
			Say(FString::Printf(TEXT("ЗАСАДА КРАСНЫХ у «%s»! Их %d"), AntRoutes::Node(AmbushNode).Name, Party.Num()));
			Raid->Note(FString::Printf(TEXT("%s — засада красных: %s"), *Raid->Clock(), AntRoutes::Node(AmbushNode).Name));
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush SPRUNG at «%s» with %d at %s"), AntRoutes::Node(AmbushNode).Name, Party.Num(), *Raid->Clock());
		}
		break;

	case ASprung:
	{
		// Who is winning here? Ours: a player 3, a bot 2, a squad worker a quarter. Theirs: 2 each.
		float Ours = 0.f, Theirs = 0.f;
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			const AAntCharacter* A = *It;
			if (!A || A->bDead || A->Creature != 0 || FVector::Dist(A->GetActorLocation(), NodeAt) > 1700.f) { continue; }
			if (A->Team == 0) { Ours += A->IsPlayerControlled() ? 3.f : 2.f; }
			else if (A->Team == 1) { Theirs += 2.f; }
		}
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
		{
			if (It->Team == 0 && FVector::Dist(It->GetActorLocation(), NodeAt) < 1700.f) { Ours += It->Count * 0.25f; }
		}
		const bool bHalfGone = Party.Num() * 2 <= SprungWith && SprungWith >= 2;
		const bool bOutnumbered = Ours > Theirs * 2.2f;
		if (Party.Num() == 0 || bHalfGone || bOutnumbered || Now >= AmbushUntil)
		{
			Ambush = ARetreat;
			AmbushSince = Now;
			Raid->AmbushState = 0;
			for (AAntClanBot* B : Party) { if (Now >= B->FreeUntil) { B->GoTo(B->Ant()->GetActorLocation(), HillAt, ClanStance::Flee); } }
			if (bHalfGone || bOutnumbered || Party.Num() == 0)
			{
				Say(TEXT("Красные отступают к холму и зовут подмогу"), false);
				Raid->Note(FString::Printf(TEXT("%s — засаду отбили"), *Raid->Clock()));
			}
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush over at «%s»: alive %d of %d, ours %.1f theirs %.1f, half gone %d outnumbered %d, at %s"), AntRoutes::Node(AmbushNode).Name, Party.Num(), SprungWith,
				Ours, Theirs, bHalfGone ? 1 : 0, bOutnumbered ? 1 : 0, *Raid->Clock());
			break;
		}
		int32 Thieves = 0;
		for (AAntClanBot* B : Party) { Thieves += Now < B->FreeUntil ? 1 : 0; }
		for (AAntClanBot* B : Party)
		{
			if (Now < B->FreeUntil) { continue; }
			AAntCharacter* Me = B->Ant();
			// Something dropped right here: one or two of them carry it off (as any fire ant would), the rest keep fighting.
			if (Thieves < 2)
			{
				bool bLoot = false;
				for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
				{
					if (!It->bIsGear && !It->bBody && !It->IsCarried() && !It->bHeld && *It != C && It->Value >= 8 && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < 500.f) { bLoot = true; break; }
				}
				if (bLoot)
				{
					B->FreeUntil = Now + 25.f;
					++Thieves;
					UE_LOG(LogTemp, Display, TEXT("ANTCLAN ambush: %s goes for the dropped loot"), *Me->GetDisplayName());
					continue;
				}
			}
			B->Stance = ClanStance::Rush;
			B->Prey = PickPrey(Me->GetActorLocation(), 2200.f, C);
		}
		break;
	}

	default:   // ARetreat
		if (Now - AmbushSince > 20.f)
		{
			Ambush = AHome;
			AmbushSince = Now;
		}
		break;
	}
}

void AAntClan::Think()
{
	if (!Raid || !GM || !Raid->InRaid())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	AKitchenLoot* C = Cake();
	int32 AliveNow = 0;
	for (AAntClanBot* B : Bots) { AliveNow += (IsValid(B) && B->Ant()) ? 1 : 0; }
	Raid->ClanAlive = (uint8)FMath::Clamp(AliveNow, 0, 255);
	Sightings(C, Now);
	CakeThink(C, Now);
	AmbushThink(C, Now);
}

void AAntClan::OnCakeInHill()
{
	SetPlan(Guard, TEXT("the cake is in the hill"));
}

void AAntClan::Decoy(const FVector& At, float Seconds)
{
	DecoyAt = At;
	DecoyUntil = GetWorld()->GetTimeSeconds() + Seconds;
	UE_LOG(LogTemp, Display, TEXT("ANTCLAN decoy at %s for %.0f s"), *At.ToCompactString(), Seconds);
}

bool AAntClan::DevCommand(const TArray<FString>& A)
{
	if (A.Num() < 2 || A[0].ToLower() != TEXT("clan"))
	{
		return false;
	}
	const FString Sub = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	const float Now = GetWorld()->GetTimeSeconds();
	AKitchenLoot* C = Cake();
	if (Sub == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCLAN info: plan %s (%.0f s), knows %d, ambush %s at «%s», sprung %d times, nudges %d, haul %d of %d, seen %.0f s ago at %s cake %d, cake state %d at %s"),
			PlanName(Plan), Now - PlanSince, bKnowsCake ? 1 : 0, AmbushName(Ambush), AmbushNode >= 0 ? AntRoutes::Node(AmbushNode).Name : TEXT("-"), AmbushCount, Nudges, HaulAt, HaulWay.Num(),
			Now - SeenTime, *SeenAt.ToCompactString(), bSeenWithCake ? 1 : 0, (int32)Raid->Cake, C ? *C->GetActorLocation().ToCompactString() : TEXT("-"));
		for (const AAntClanBot* B : Bots)
		{
			const AAntCharacter* R = IsValid(B) ? B->Ant() : nullptr;
			UE_LOG(LogTemp, Display, TEXT("ANTCLAN info:   %s role %d stance %d %s hp %.0f way %d/%d carrying %s"), R ? *R->GetDisplayName() : TEXT("(dead)"), (int32)B->Job, (int32)B->Stance,
				R ? *R->GetActorLocation().ToCompactString() : TEXT("-"), R ? R->Health : 0.f, B->WayAt, B->Way.Num(), (R && R->Carrying) ? *R->Carrying->LootName : TEXT("-"));
		}
	}
	else if (Sub == TEXT("find")) { bKnowsCake = true; if (Plan == Idle) { SetPlan(Scouting, TEXT("dev")); } }
	else if (Sub == TEXT("out")) { SetPlan(Scouting, TEXT("dev")); Raid->ClanAlert = 1; }
	else if (Sub == TEXT("here") && C)
	{
		// the hauling party is put next to the cake (to see the haul without waiting for the walk)
		bKnowsCake = true;
		for (AAntClanBot* B : Bots)
		{
			AAntCharacter* R = IsValid(B) ? B->Ant() : nullptr;
			if (R && (B->Job == ClanRole::Hauler || B->Job == ClanRole::Escort)) { R->ApplyTeleport(C->GetActorLocation() + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 25.f)); }
		}
		FirstGripAt = 0.f;
		SetPlan(Fetch, TEXT("dev"));
	}
	else if (Sub == TEXT("ambush"))
	{
		// clan ambush <node name or index>: the party goes there now; "clan ambush" alone: they choose
		int32 Node = A.Num() > 2 ? (A[2].IsNumeric() ? (int32)Num(2) : AntRoutes::Find(*A[2])) : ChooseAmbushNode(C, Now);
		if (Node < 0) { Node = AntRoutes::Find(TEXT("Порог")); }
		AmbushNode = Node;
		Ambush = AStaging;
		AmbushSince = Now;
		NextRechoose = Now + 600.f;
		TArray<AAntClanBot*> Party;
		Alive(ClanRole::Ambusher, Party);
		for (AAntClanBot* B : Party) { B->Stance = ClanStance::Travel; }
		if (A.Num() > 3 && Num(3) > 0.5f)   // ... 1: put them there at once
		{
			for (int32 i = 0; i < Party.Num(); ++i) { Party[i]->Ant()->ApplyTeleport(Ring(AntRoutes::Point(Node), i, Party.Num(), 300.f) + FVector(0.f, 0.f, 30.f)); }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTCLAN dev: ambush at «%s» (%d in the party)"), AntRoutes::Node(Node).Name, Party.Num());
	}
	else if (Sub == TEXT("choose"))
	{
		const int32 Node = ChooseAmbushNode(C, Now);
		UE_LOG(LogTemp, Display, TEXT("ANTCLAN dev: would ambush at «%s» (seen %.0f s ago at %s, with the cake %d)"), AntRoutes::Node(Node).Name, Now - SeenTime, *SeenAt.ToCompactString(), bSeenWithCake ? 1 : 0);
	}
	else if (Sub == TEXT("hill") && C) { Raid->OnClanCakeHome(C); }
	else if (Sub == TEXT("decoy")) { Decoy(FVector(Num(2), Num(3), Num(4)), Num(5, 20.f)); }
	else { return false; }
	return true;
}
