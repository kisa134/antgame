#include "AntRts.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntSnowball.h"
#include "AntTrail.h"
#include "AntHaulAdapter.h"
#include "AntRoutes.h"
#include "AntRaid.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntTypes.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AntFighter.h"

namespace
{
	const FLinearColor RtsGreen(0.45f, 1.f, 0.55f);
	const FLinearColor RtsGold(1.f, 0.82f, 0.3f);
	const FLinearColor RtsRed(1.f, 0.4f, 0.35f);
	const FLinearColor RtsGrey(0.8f, 0.8f, 0.8f);
	/** Where the pick cards were drawn last frame (screen pixels), for the mouse. */
	FBox2D GCardRect[3];
	bool GCardsShown = false;

	FAntSnowPlayer* EntryOf(const UWorld* W, const APlayerState* PS)
	{
		AAntSnowball* S = AAntSnowball::Get(W);
		return S ? S->Find(PS) : nullptr;
	}

	float ServerNow(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? (float)GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	}

	bool IsFoe(const AActor* A)
	{
		if (const AAntCharacter* Ant = Cast<AAntCharacter>(A)) { return !Ant->bDead && Ant->Team != 0; }
		return Cast<AHumanCharacter>(A) != nullptr;
	}
}

// =============================================================================================
// The squad ant's brain
// =============================================================================================

AAntSquadBot::AAntSquadBot()
{
	bWantsPlayerState = false;
}

AAntCharacter* AAntSquadBot::OwnerAnt() const
{
	AAntCharacter* A = OwnerPS.IsValid() ? Cast<AAntCharacter>(OwnerPS->GetPawn()) : nullptr;
	return (A && !A->bDead) ? A : nullptr;
}

void AAntSquadBot::Tick(float DeltaSeconds)
{
	AAIController::Tick(DeltaSeconds);   // (not AAntBotController::Tick: this brain replaces Think)
	AAntCharacter* Ant = Cast<AAntCharacter>(GetPawn());
	if (!Ant || Ant->bDead)
	{
		return;
	}
	if (Ant->IsAttached())
	{
		Shoot(Ant, DeltaSeconds);
		return;
	}
	AntVets::Tick(Ant, DeltaSeconds);   // a veteran's quirks of the moment: the coward and the cat (AntFighter.h)
	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		// Far from every player: think half as often (a cheap LOD for big squads).
		float Near = 1.e9f;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APawn* P = It->IsValid() ? (*It)->GetPawn() : nullptr) { Near = FMath::Min(Near, (float)FVector::DistSquared(P->GetActorLocation(), Ant->GetActorLocation())); }
		}
		ThinkEvery = Near < FMath::Square(3500.f) ? 0.4f : 0.9f;
		ThinkTimer = ThinkEvery;
		ThinkRts(Ant, ThinkEvery);
	}
	if (bHauling && HaulMove(Ant))
	{
	}
	else if (bCarryWithPlayer && CarryFollow(Ant))
	{
	}
	else if (bHasGoal)
	{
		Steer(Ant, Goal, DeltaSeconds);
	}
	Shoot(Ant, DeltaSeconds);
}

void AAntSquadBot::ThinkRts(AAntCharacter* Ant, float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();
	FAntSnowPlayer* P = EntryOf(GetWorld(), OwnerPS.Get());
	const int32 Me = P ? P->Squad.Find(Ant) : INDEX_NONE;
	if (!P || Me == INDEX_NONE)
	{
		Think(Ant);   // nobody's: the plain bot
		return;
	}
	const uint8 TaskIdx = P->SquadTask.IsValidIndex(Me) ? P->SquadTask[Me] : 255;
	const FAntRtsTask* Task = P->Tasks.IsValidIndex(TaskIdx) ? &P->Tasks[TaskIdx] : nullptr;
	const uint8 Order = Task ? Task->Order : AntTask::Follow;
	AAntCharacter* Boss = OwnerAnt();
	if (Now >= NextShootPick)
	{
		NextShootPick = Now + 0.9f;
		ShootTarget = PickShootTarget(Ant);
	}
	bCarryWithPlayer = false;
	bHauling = false;
	const int32 N = FMath::Max(1, P->Squad.Num());
	// The squad's doctrine (AntFighter.h): «Клин» close behind, «Рассыпной» spread wide, «Черепаха» huddled.
	const float Spread = Ant->VetDoctrine == AntDoctrine::Loose ? 2.2f : Ant->VetDoctrine == AntDoctrine::Turtle ? 0.6f : Ant->VetDoctrine == AntDoctrine::Wedge ? 0.75f : 1.f;
	auto Ring = [this, N, Spread](const FVector& C, float R0)
	{
		const float Ang = Slot * 2.39996f;
		const float R = (R0 + FMath::Sqrt((float)(Slot % N + 1)) * 26.f) * Spread;
		return C + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0.f);
	};
	auto Fight = [&](const FVector& Around, float Range) -> bool
	{
		AAntCharacter* E = Cast<AAntCharacter>(ShootTarget.Get());
		if (!E || E->bDead || FVector::Dist(E->GetActorLocation(), Around) > Range)
		{
			return false;
		}
		Goal = E->GetActorLocation();
		bHasGoal = true;
		if (FVector::Dist(E->GetActorLocation(), Ant->GetActorLocation()) < 130.f * AAntCharacter::AntShrink + AAntCharacter::BodyRadius * 2.5f) { Ant->BotBite(); }
		return true;
	};

	if (Order == AntTask::Free)
	{
		Think(Ant);
		return;
	}
	if (Order == AntTask::Haul)
	{
		AKitchenLoot* L = Cast<AKitchenLoot>(Task->Target);
		if (L && AntHaul::CanHaul(L) && HaulThink(Ant, L, TaskIdx))
		{
			return;
		}
	}
	// Whatever it held for an old order, it lets go (unless it helps its owner).
	if (Ant->Carrying && !(Boss && Boss->Carrying == Ant->Carrying) && Order != AntTask::Haul)
	{
		AntHaul::RemoveCarrier(Ant->Carrying, Ant);
	}
	if (Order == AntTask::Hold)
	{
		if (!Fight(Task->Point, 700.f))
		{
			Goal = Ring(FVector(Task->Point), 20.f);
			bHasGoal = FVector::Dist2D(Goal, Ant->GetActorLocation()) > 40.f;
		}
		return;
	}
	if (Order == AntTask::Attack)
	{
		AActor* T = Task->Target;
		if (T && IsFoe(T))
		{
			ShootTarget = T;
			Goal = T->GetActorLocation();
			if (const AHumanCharacter* H = Cast<AHumanCharacter>(T)) { Goal.Z = Ant->GetActorLocation().Z; }
			bHasGoal = true;
			if (FVector::Dist(Goal, Ant->GetActorLocation()) < 140.f) { Ant->BotBite(); }
			return;
		}
		if (!Fight(Task->Point, 1100.f))
		{
			Goal = Ring(FVector(Task->Point), 30.f);
			bHasGoal = true;
		}
		return;
	}
	// Follow: help him carry, fight what comes near him, walk with him.
	if (!Boss)
	{
		Goal = AKitchenBuilder::HomeFor(0, Ant->GetActorLocation());
		bHasGoal = true;
		return;
	}
	if (AKitchenLoot* His = Boss->Carrying)
	{
		if (Ant->Carrying == His)
		{
			bCarryWithPlayer = true;
			CarryLeader = Boss;
			return;
		}
		if (AntHaul::Ratio(His) < 1.5f && AntHaul::NumCarriers(His) < AntHaul::MaxCarriers(His) && FVector::Dist(His->GetActorLocation(), Ant->GetActorLocation()) < 1500.f)
		{
			const float Reach = AAntCharacter::InteractRadius + His->GetRadius() * 0.5f;
			if (FVector::Dist(His->GetActorLocation(), Ant->GetActorLocation()) < Reach && AntHaul::AddCarrier(His, Ant))
			{
				bCarryWithPlayer = true;
				CarryLeader = Boss;
				return;
			}
			Goal = His->GetActorLocation();
			bHasGoal = true;
			return;
		}
	}
	// Leash: a follower that is far from its owner and has not got closer for 5 s is stuck somewhere on the way (a ledge, a leg,
	// a gap the route graph does not know). It comes out of the cracks right behind him instead of being lost for the whole raid.
	{
		const float BossD = (float)FVector::Dist(Boss->GetActorLocation(), Ant->GetActorLocation());
		if (BossD < 1500.f || Ant->Carrying || Boss->IsInNest() || Boss->bDead) { LeashDeadline = 0.f; }
		else if (LeashDeadline <= 0.f || BossD < LeashBest - 150.f) { LeashBest = BossD; LeashDeadline = Now + 5.f; }
		else if (Now > LeashDeadline)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTRTS leash: squad ant %s stuck %.0f uu from its owner, regroups behind him"), *Ant->GetName(), BossD);
			Ant->ApplyTeleport(Boss->GetActorLocation() - Boss->GetActorForwardVector() * (40.f + 12.f * (Slot % 6)) + FVector(0.f, 0.f, 6.f));
			LeashDeadline = 0.f;
			bHasGoal = false;
			return;
		}
	}
	if (Fight(Boss->GetActorLocation(), 650.f))
	{
		return;
	}
	const FVector Back = -Boss->GetActorForwardVector().GetSafeNormal2D() * 50.f;
	Goal = Ring(Boss->GetActorLocation() + Back, 35.f);
	bHasGoal = FVector::Dist2D(Goal, Ant->GetActorLocation()) > 30.f || FVector::Dist(Boss->GetActorLocation(), Ant->GetActorLocation()) > 300.f;
}

bool AAntSquadBot::HaulThink(AAntCharacter* Ant, AKitchenLoot* L, uint8 TaskIdx)
{
	if (Ant->Carrying && Ant->Carrying != L)
	{
		AntHaul::RemoveCarrier(Ant->Carrying, Ant);
	}
	const FVector LP = L->GetActorLocation();
	if (AntHaul::IsCarrying(Ant, L))
	{
		// If the owner holds it too, he leads (the old "carry with a player" rule).
		TArray<AAntCharacter*> Cs;
		AntHaul::GetCarriers(L, Cs);
		for (AAntCharacter* C : Cs)
		{
			if (C && C != Ant && !C->bDead && C->IsPlayerControlled() && C->Team == 0)
			{
				bCarryWithPlayer = true;
				CarryLeader = C;
				return true;
			}
		}
		// Every squad carrier walks the same way home (the route graph, on the floor), each keeping under its own grip: the
		// load goes where they all push it. (One leader with the others trailing it tore the grips apart.)
		if (WayFor.Get() != L || Way.Num() == 0)
		{
			Way.Reset();
			WayAt = 0;
			WayFor = L;
			const FVector Home = AKitchenBuilder::HomeFor(0, LP);
			if (!AntRoutes::PathPoints(LP, Home, true, Way, nullptr, AntRoutes::FloorOnly))
			{
				Way.Reset();
			}
			Way.Add(Home);
			StuckFor = 0.f;
			LastHaulPos = LP;
			if (Cs.Num() > 0 && Cs[0] == Ant)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTRTS haul «%s» (weight %d): %d waypoints home, carriers %d, ratio %.2f"), *L->LootName, L->Weight, Way.Num(), Cs.Num(), AntHaul::Ratio(L));
			}
		}
		while (WayAt < Way.Num() - 1 && FVector::Dist2D(Way[WayAt], LP) < 200.f) { ++WayAt; }
		HaulGoal = Way[FMath::Clamp(WayAt, 0, Way.Num() - 1)];
		bHauling = true;
		bHasGoal = false;
		AntHaul::SetDesiredDirection(L, (HaulGoal - LP).GetSafeNormal2D());
		StuckFor = FVector::Dist(LP, LastHaulPos) < 25.f ? StuckFor + ThinkEvery : 0.f;
		LastHaulPos = LP;
		if (StuckFor > 6.f && AntHaul::Ratio(L) >= 0.5f)
		{
			StuckFor = 0.f;
			WayAt = FMath::Min(WayAt + 1, Way.Num() - 1);   // a corner it cannot turn: aim for the next point
		}
		return true;
	}
	if (AntHaul::NumCarriers(L) >= AntHaul::MaxCarriers(L))
	{
		// No room on it: walk beside it as its guard.
		AAntCharacter* E = Cast<AAntCharacter>(ShootTarget.Get());
		if (E && !E->bDead && FVector::Dist(E->GetActorLocation(), LP) < 700.f)
		{
			Goal = E->GetActorLocation();
			if (FVector::Dist(Goal, Ant->GetActorLocation()) < 60.f) { Ant->BotBite(); }
		}
		else
		{
			const float Ang = Slot * 2.39996f;
			Goal = LP + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (L->GetRadius() + 70.f);
		}
		bHasGoal = true;
		return true;
	}
	const float Reach = AAntCharacter::InteractRadius + L->GetRadius() * 0.5f;
	if (FVector::Dist(LP, Ant->GetActorLocation()) < Reach && AntHaul::AddCarrier(L, Ant))
	{
		return true;
	}
	Goal = LP;
	bHasGoal = true;
	return true;
}

bool AAntSquadBot::HaulMove(AAntCharacter* Ant)
{
	AKitchenLoot* L = Ant->Carrying;
	FVector Bite;
	if (!L || !L->GrabPointOf(Ant, Bite))
	{
		return false;
	}
	// The way the load must go, plus staying under its own grip (so the grips are not torn apart).
	const FVector Dir = (HaulGoal - L->GetActorLocation()).GetSafeNormal2D();
	const FVector Want = Dir * 110.f + (Bite - Ant->GetHoldPoint()) * 1.5f;
	FVector To = FVector::VectorPlaneProject(Want, Ant->GetVisualUp());
	if (Ant->GetVisualUp().Z < 0.6f && (Want | Ant->GetVisualUp()) < -8.f)
	{
		const FVector UpOnWall = FVector::UpVector - Ant->GetVisualUp() * Ant->GetVisualUp().Z;
		To += UpOnWall.GetSafeNormal() * (float)Want.Size();
	}
	if (To.Size() > 8.f)
	{
		Ant->AddMovementInput(To.GetSafeNormal(), FMath::Clamp((float)To.Size() / 60.f, 0.3f, 1.f));
	}
	Desired = FRotator(0.f, (float)(Bite - Ant->GetActorLocation()).Rotation().Yaw, 0.f);
	return true;
}

// =============================================================================================
// Server: squads and orders
// =============================================================================================

namespace AntRts
{
	int32 Spawn(APlayerState* PS, int32 Num, uint8 Class)
	{
		UWorld* W = PS ? PS->GetWorld() : nullptr;
		AAntSnowball* S = AAntSnowball::Get(W);
		FAntSnowPlayer* P = S ? S->For(PS) : nullptr;
		if (!P || Num <= 0 || W->GetNetMode() == NM_Client)
		{
			return 0;
		}
		const APawn* Owner = PS->GetPawn();
		const AAntCharacter* OA = Cast<AAntCharacter>(Owner);
		const FVector Base = (OA && !OA->bDead) ? OA->GetActorLocation() : AKitchenBuilder::NestLocation(0) + FVector(300.f, 0.f, 60.f);
		int32 Made = 0;
		for (int32 i = 0; i < Num; ++i)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const float Ang = FMath::FRand() * 2.f * PI;
			const FVector Loc = Base + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * FMath::FRandRange(40.f, 110.f) + FVector(0.f, 0.f, 12.f);
			AAntCharacter* A = W->SpawnActor<AAntCharacter>(AAntCharacter::StaticClass(), Loc, FRotator(0.f, FMath::FRand() * 360.f, 0.f), SP);
			if (!A)
			{
				continue;
			}
			AAntSquadBot* C = W->SpawnActor<AAntSquadBot>(AAntSquadBot::StaticClass(), FTransform::Identity, SP);
			if (!C)
			{
				A->Destroy();
				continue;
			}
			C->OwnerPS = PS;
			C->Slot = P->SquadEver;
			C->Possess(A);
			A->Team = 0;
			A->StrengthOverride = AntKit::ClassDef(Class == 1 ? EAntClass::Soldier : EAntClass::Worker).Strength + 4.f * P->Lv[AntBoost::SquadStr];
			A->HealthOverride = AntKit::ClassDef(Class == 1 ? EAntClass::Soldier : EAntClass::Worker).Health * (1.f + 0.4f * P->Lv[AntBoost::SquadHp]);
			A->SpeedScale = 1.f + 0.12f * P->Lv[AntBoost::SquadSpeed];
			A->SetClass(Class == 1 ? EAntClass::Soldier : EAntClass::Worker, true);
			A->Health = A->MaxHealth;
			P->Squad.Add(A);
			P->SquadTask.Add(255);
			P->SquadEver++;
			++Made;
		}
		S->ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("ANTRTS spawn %d squad ants (class %d) for %s: squad now %d"), Made, (int32)Class, *PS->GetPlayerName(), P->Squad.Num());
		return Made;
	}

	void DespawnAll(UWorld* W)
	{
		if (!W)
		{
			return;
		}
		for (TActorIterator<AAntSquadBot> It(W); It; ++It)
		{
			if (APawn* P = It->GetPawn())
			{
				if (AAntCharacter* A = Cast<AAntCharacter>(P)) { A->DropEverything(); }
				It->UnPossess();
				P->Destroy();
			}
			It->Destroy();
		}
		// Bodies of squad ants that died (their controllers are gone already).
		if (AAntSnowball* S = AAntSnowball::Get(W))
		{
			for (FAntSnowPlayer& P : S->Players)
			{
				P.Squad.Reset();
				P.SquadTask.Reset();
				P.Tasks.Reset();
				P.Selected.Reset();
			}
		}
	}

	static void Compact(FAntSnowPlayer& P)
	{
		// Members gone; tasks nobody does any more (indices renumbered).
		for (int32 i = P.Squad.Num() - 1; i >= 0; --i)
		{
			if (!IsValid(P.Squad[i]) || P.Squad[i]->bDead) { P.Squad.RemoveAt(i); if (P.SquadTask.IsValidIndex(i)) { P.SquadTask.RemoveAt(i); } }
		}
		P.SquadTask.SetNum(P.Squad.Num());
		TArray<int32> Remap;
		Remap.Init(-1, P.Tasks.Num());
		TArray<FAntRtsTask> Kept;
		for (int32 t = 0; t < P.Tasks.Num(); ++t)
		{
			int32 M = 0;
			for (uint8 X : P.SquadTask) { M += X == t ? 1 : 0; }
			if (M > 0 && P.Tasks[t].State != 4) { Remap[t] = Kept.Num(); P.Tasks[t].Members = (uint8)FMath::Min(255, M); Kept.Add(P.Tasks[t]); }
		}
		for (uint8& X : P.SquadTask) { X = (X != 255 && Remap.IsValidIndex(X) && Remap[X] >= 0) ? (uint8)Remap[X] : 255; }
		P.Tasks = Kept;
	}

	void Assign(FAntSnowPlayer& P, const TArray<AAntCharacter*>& Ants, uint8 Task, AActor* Target, const FVector& Point)
	{
		int32 Idx = 255;
		if (Task != AntTask::Follow)
		{
			for (int32 t = 0; t < P.Tasks.Num(); ++t)
			{
				if (P.Tasks[t].Order == Task && P.Tasks[t].Target == Target && Target) { Idx = t; break; }
			}
			if (Idx == 255 && P.Tasks.Num() < 250)
			{
				FAntRtsTask T;
				T.Order = Task;
				T.Target = Target;
				T.Point = Point;
				T.Id = P.NextTaskId++;
				Idx = P.Tasks.Add(T);
			}
		}
		int32 Given = 0;
		for (AAntCharacter* A : Ants)
		{
			const int32 i = P.Squad.Find(A);
			if (i == INDEX_NONE) { continue; }
			P.SquadTask[i] = (uint8)Idx;
			if (AAntSquadBot* C = Cast<AAntSquadBot>(A->GetController())) { C->Way.Reset(); C->WayFor = nullptr; C->Wake(); }
			if (A->Carrying && (Task != AntTask::Haul || A->Carrying != Target)) { AntHaul::RemoveCarrier(A->Carrying, A); }
			++Given;
		}
		Compact(P);
		const AKitchenLoot* L = Cast<AKitchenLoot>(Target);
		UE_LOG(LogTemp, Display, TEXT("ANTRTS order %s by %s: %d ants -> %s at %s (weight %d)"), AntTask::Name(Task), P.PS ? *P.PS->GetPlayerName() : TEXT("?"), Given,
			L ? *L->LootName : Target ? *Target->GetName() : TEXT("point"), *Point.ToCompactString(), L ? L->Weight : 0);
		if (AAntSnowball* S = P.PS ? AAntSnowball::Get(P.PS->GetWorld()) : nullptr) { S->ForceNetUpdate(); }
	}

	void TasksSecond(FAntSnowPlayer& P)
	{
		const UWorld* W = P.PS ? P.PS->GetWorld() : nullptr;
		for (FAntRtsTask& T : P.Tasks)
		{
			if (T.Order == AntTask::Haul)
			{
				AKitchenLoot* L = Cast<AKitchenLoot>(T.Target);
				if (!L || !AntHaul::CanHaul(L))
				{
					T.State = 4;   // delivered, eaten or gone: the group comes back
					UE_LOG(LogTemp, Display, TEXT("ANTRTS haul task %d done (load gone) for %s"), (int32)T.Id, P.PS ? *P.PS->GetPlayerName() : TEXT("?"));
					if (AAntPlayerController* PC = P.PS ? Cast<AAntPlayerController>(P.PS->GetOwner()) : nullptr) { PC->ClientToast(TEXT("Группа донесла груз (или его больше нет) и идёт к тебе")); }
					continue;
				}
				int32 Mine = 0;
				TArray<AAntCharacter*> Cs;
				AntHaul::GetCarriers(L, Cs);
				for (AAntCharacter* C : Cs) { Mine += P.Squad.Contains(C) ? 1 : 0; }
				const float R = AntHaul::Ratio(L);
				T.State = Mine == 0 ? 0 : (R >= 0.5f ? 1 : 2);
				T.Need = (uint8)FMath::Clamp(R >= 1.f || Mine == 0 ? 0 : L->AntsNeeded(14.f) - Cs.Num(), 0, 99);
				T.Point = L->GetActorLocation();
			}
			else if (T.Order == AntTask::Attack)
			{
				if (T.Target && !IsFoe(T.Target)) { T.Target = nullptr; }
				T.State = 3;
			}
		}
		const int32 Before = P.Tasks.Num();
		Compact(P);
		(void)Before;
		(void)W;
	}

	static TArray<AAntCharacter*> Pick(FAntSnowPlayer& P, const TArray<AAntCharacter*>& Asked)
	{
		TArray<AAntCharacter*> Out;
		for (AAntCharacter* A : Asked) { if (IsValid(A) && P.Squad.Contains(A)) { Out.Add(A); } }
		if (Out.Num() == 0)
		{
			for (const TWeakObjectPtr<AAntCharacter>& A : P.Selected) { if (A.IsValid() && P.Squad.Contains(A.Get())) { Out.Add(A.Get()); } }
		}
		if (Out.Num() == 0)
		{
			for (AAntCharacter* A : P.Squad) { if (IsValid(A) && !A->bDead) { Out.Add(A); } }
		}
		return Out;
	}

	static AKitchenLoot* LoadNear(UWorld* W, const FVector& At, float R)
	{
		// v12: measured from the load's edge, not its centre (the cake is wide: aimed at its side it was "nothing to haul"),
		// and the cake wins when the aim is anywhere on or near it.
		AKitchenLoot* Best = nullptr;
		float BestD = R;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (!AntHaul::CanHaul(*It) || It->bIsGear) { continue; }
			const float D = FMath::Max(0.f, (float)FVector::Dist(It->GetActorLocation(), At) - It->GetRadius());
			if (It->bMain && D < R * 2.f) { return *It; }
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	static AActor* FoeNear(UWorld* W, const FVector& At, float R)
	{
		AActor* Best = nullptr;
		float BestD = R * R;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			if (!IsFoe(*It)) { continue; }
			const float D = (float)FVector::DistSquared(It->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	void ServerCommand(AAntPlayerController* PC, uint8 Cmd, int32 A, const TArray<AAntCharacter*>& Ants, const FVector& Point, AActor* Target)
	{
		if (!PC)
		{
			return;
		}
		if (Cmd == CmdPick)
		{
			AntSnowball::ServerPick(PC, A);
			return;
		}
		FAntSnowPlayer* P = EntryOf(PC->GetWorld(), PC->PlayerState);
		if (!P)
		{
			return;
		}
		if (Cmd == CmdSelect)
		{
			P->Selected.Reset();
			for (AAntCharacter* X : Ants) { if (IsValid(X) && P->Squad.Contains(X)) { P->Selected.Add(X); } }
			return;
		}
		if (Cmd != CmdOrder || A < 0 || A >= AntTask::Count)
		{
			return;
		}
		UWorld* W = PC->GetWorld();
		AActor* T = Target;
		if (A == AntTask::Haul && !Cast<AKitchenLoot>(T))
		{
			T = LoadNear(W, Point, 260.f);
			if (!T)
			{
				PC->ClientToast(TEXT("Наведи на еду: тащить нечего"));
				return;
			}
		}
		if (A == AntTask::Attack && !T) { T = FoeNear(W, Point, 320.f); }
		Assign(*P, Pick(*P, Ants), (uint8)A, T, Point);
	}

	bool SquadKey(AAntCharacter* Ant, uint8 OldOrder, const FVector& Aim)
	{
		if (!Ant || !AntSnowball::Enabled(Ant))
		{
			return false;
		}
		AAntPlayerController* PC = Cast<AAntPlayerController>(Ant->GetController());
		FAntSnowPlayer* P = EntryOf(Ant->GetWorld(), Ant->GetPlayerState());
		if (!PC || !P)
		{
			return true;
		}
		// AntSquad::EOrder: Follow, Attack, Free, Haul, Hold
		static const uint8 Map[5] = { AntTask::Follow, AntTask::Attack, AntTask::Free, AntTask::Haul, AntTask::Hold };
		const uint8 Task = OldOrder < 5 ? Map[OldOrder] : AntTask::Follow;
		if (P->Squad.Num() == 0)
		{
			PC->ClientToast(TEXT("Отряда пока нет: набери массу и возьми «Новобранцев»"));
			return true;
		}
		ServerCommand(PC, CmdOrder, Task, TArray<AAntCharacter*>(), Aim, nullptr);
		return true;
	}

	bool RecallAll(AAntCharacter* Ant)
	{
		FAntSnowPlayer* P = (Ant && AntSnowball::Enabled(Ant)) ? EntryOf(Ant->GetWorld(), Ant->GetPlayerState()) : nullptr;
		if (!P)
		{
			return false;
		}
		TArray<AAntCharacter*> All;
		for (AAntCharacter* A : P->Squad) { if (IsValid(A)) { All.Add(A); } }
		Assign(*P, All, AntTask::Follow, nullptr, Ant->GetActorLocation());
		return All.Num() > 0;
	}

	bool OnSquadBotKilled(AAntCharacter* Ant)
	{
		AAntSquadBot* C = Ant ? Cast<AAntSquadBot>(Ant->GetController()) : nullptr;
		if (!C)
		{
			return false;
		}
		if (FAntSnowPlayer* P = EntryOf(Ant->GetWorld(), C->OwnerPS.Get()))
		{
			const int32 i = P->Squad.Find(Ant);
			if (i != INDEX_NONE)
			{
				P->Squad.RemoveAt(i);
				if (P->SquadTask.IsValidIndex(i)) { P->SquadTask.RemoveAt(i); }
				P->SquadLost++;
				Compact(*P);
			}
			UE_LOG(LogTemp, Display, TEXT("ANTRTS squad ant of %s died (%s): squad now %d"), P->PS ? *P->PS->GetPlayerName() : TEXT("?"), *Ant->LastHazard, P->Squad.Num());
		}
		C->UnPossess();
		C->Destroy();
		TWeakObjectPtr<AAntCharacter> Weak = Ant;
		FTimerHandle H;
		Ant->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Ant, [Weak]() { if (Weak.IsValid()) { Weak->Destroy(); } }), 8.f, false);
		return true;
	}
}

void AAntPlayerController::ServerSwarm_Implementation(uint8 Cmd, int32 A, const TArray<AAntCharacter*>& Ants, FVector_NetQuantize Point, AActor* Target)
{
	AntRts::ServerCommand(this, Cmd, A, Ants, Point, Target);
}

// =============================================================================================
// The local controls
// =============================================================================================

UAntRtsInput::UAntRtsInput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UAntRtsInput* UAntRtsInput::Attach(AAntPlayerController* InPC)
{
	if (!InPC || !InPC->IsLocalController())
	{
		return nullptr;
	}
	UAntRtsInput* C = NewObject<UAntRtsInput>(InPC, TEXT("AntRtsInput"));
	C->RegisterComponent();
	FString Do;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntRtsDo="), Do, false))
	{
		TArray<FString> Steps;
		Do.ParseIntoArray(Steps, TEXT(";"));
		for (const FString& S : Steps)
		{
			FString At, Cmd;
			if (S.Split(TEXT(":"), &At, &Cmd)) { C->Script.Add(TPair<float, FString>(FCString::Atof(*At), Cmd.TrimStartAndEnd())); }
		}
	}
	return C;
}

UAntRtsInput* UAntRtsInput::Of(const APlayerController* InPC)
{
	return InPC ? InPC->FindComponentByClass<UAntRtsInput>() : nullptr;
}

AAntPlayerController* UAntRtsInput::PC() const
{
	return Cast<AAntPlayerController>(GetOwner());
}

TArray<AAntCharacter*> UAntRtsInput::MySquad() const
{
	TArray<AAntCharacter*> Out;
	const AAntPlayerController* C = PC();
	const AAntSnowball* S = C ? AAntSnowball::Get(C->GetWorld()) : nullptr;
	const FAntSnowPlayer* P = S ? S->Find(C->PlayerState) : nullptr;
	if (P)
	{
		for (AAntCharacter* A : P->Squad) { if (IsValid(A) && !A->bDead) { Out.Add(A); } }
	}
	return Out;
}

void UAntRtsInput::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AAntPlayerController* C = PC();
	if (!C)
	{
		return;
	}
	if (!Always && C->InputComponent)
	{
		Always = NewObject<UInputComponent>(C, TEXT("AntRtsAlways"));
		Always->Priority = 40;
		Always->BindKey(EKeys::CapsLock, IE_Pressed, this, &UAntRtsInput::OnCapsDown).bConsumeInput = false;
		Always->BindKey(EKeys::CapsLock, IE_Released, this, &UAntRtsInput::OnCapsUp).bConsumeInput = false;
		Always->BindKey(EKeys::Seven, IE_Pressed, this, &UAntRtsInput::Key7);
		Always->BindKey(EKeys::Eight, IE_Pressed, this, &UAntRtsInput::Key8);
		Always->BindKey(EKeys::Nine, IE_Pressed, this, &UAntRtsInput::Key9);
		Always->BindKey(EKeys::Zero, IE_Pressed, this, &UAntRtsInput::Key0);
		C->PushInputComponent(Always);
		Modal = NewObject<UInputComponent>(C, TEXT("AntRtsModal"));
		Modal->Priority = 60;
		Modal->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UAntRtsInput::OnLmbDown);
		Modal->BindKey(EKeys::LeftMouseButton, IE_Released, this, &UAntRtsInput::OnLmbUp);
		Modal->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &UAntRtsInput::OnRmb);
		Modal->BindKey(EKeys::Z, IE_Pressed, this, &UAntRtsInput::KeyZ);
		Modal->BindKey(EKeys::X, IE_Pressed, this, &UAntRtsInput::KeyX);
		Modal->BindKey(EKeys::Q, IE_Pressed, this, &UAntRtsInput::KeyQ);
		Modal->BindKey(EKeys::B, IE_Pressed, this, &UAntRtsInput::KeyB);
		Modal->BindKey(EKeys::Seven, IE_Pressed, this, &UAntRtsInput::Key7);
		Modal->BindKey(EKeys::Eight, IE_Pressed, this, &UAntRtsInput::Key8);
		Modal->BindKey(EKeys::Nine, IE_Pressed, this, &UAntRtsInput::Key9);
		Modal->BindKey(EKeys::Zero, IE_Pressed, this, &UAntRtsInput::Key0);
	}
	if (bRtsMode)
	{
		float X, Y;
		if (C->GetMousePosition(X, Y)) { Mouse = FVector2D(X, Y); }
		if (!C->IsInputKeyDown(EKeys::CapsLock) && !bDevHold) { LeaveRts(); }
	}
	// Squad ants that died drop out of the selection.
	Selection.RemoveAll([](const TWeakObjectPtr<AAntCharacter>& A) { return !A.IsValid() || A->bDead; });
	ScriptTick();
}

void UAntRtsInput::ScriptTick()
{
	if (Script.Num() == 0)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Script.Num();)
	{
		if (Script[i].Key <= Now)
		{
			const FString Cmd = Script[i].Value;
			Script.RemoveAt(i);
			TArray<FString> A;
			Cmd.ParseIntoArrayWS(A);
			if (A.Num() > 0 && A[0] != TEXT("rts")) { A.Insert(TEXT("rts"), 0); }
			UE_LOG(LogTemp, Display, TEXT("ANTRTS script %.1f: %s"), Now, *Cmd);
			Dev(A);
		}
		else { ++i; }
	}
}

void UAntRtsInput::EnterRts()
{
	AAntPlayerController* C = PC();
	if (!C || bRtsMode || !Modal)
	{
		return;
	}
	bRtsMode = true;
	C->bShowMouseCursor = true;
	C->SetIgnoreLookInput(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	C->SetInputMode(Mode);
	C->PushInputComponent(Modal);
}

void UAntRtsInput::LeaveRts()
{
	AAntPlayerController* C = PC();
	if (!C || !bRtsMode)
	{
		return;
	}
	bRtsMode = false;
	bDragging = false;
	C->bShowMouseCursor = false;
	C->ResetIgnoreLookInput();
	C->SetInputMode(FInputModeGameOnly());
	C->PopInputComponent(Modal);
}

void UAntRtsInput::OnCapsDown() { EnterRts(); }
void UAntRtsInput::OnCapsUp() { LeaveRts(); }

void UAntRtsInput::OnLmbDown()
{
	// A pick card under the mouse?
	if (GCardsShown)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (GCardRect[i].bIsValid && GCardRect[i].IsInside(Mouse)) { AntSnowball::TryPick(PC(), i); return; }
		}
	}
	bDragging = true;
	DragFrom = Mouse;
}

void UAntRtsInput::OnLmbUp()
{
	if (!bDragging)
	{
		return;
	}
	bDragging = false;
	const bool bAdd = PC() && PC()->IsInputKeyDown(EKeys::LeftShift);
	if (FVector2D::Distance(DragFrom, Mouse) < 8.f)
	{
		// A click: the nearest squad ant within 30 px.
		AAntCharacter* Best = nullptr;
		float BestD = 30.f;
		for (AAntCharacter* A : MySquad())
		{
			FVector2D Sp;
			if (PC()->ProjectWorldLocationToScreen(A->GetActorLocation(), Sp))
			{
				const float D = FVector2D::Distance(Sp, Mouse);
				if (D < BestD) { BestD = D; Best = A; }
			}
		}
		if (!bAdd) { Selection.Reset(); }
		if (Best) { Selection.AddUnique(Best); }
		SendSelection();
		return;
	}
	SelectBox(DragFrom, Mouse, bAdd);
}

void UAntRtsInput::OnRmb()
{
	AActor* T = nullptr;
	FVector P;
	Probe(true, T, P);
	const uint8 Task = Cast<AKitchenLoot>(T) ? AntTask::Haul : (T && IsFoe(T)) ? AntTask::Attack : AntTask::Hold;
	OrderAt(Task, T, P);
}

void UAntRtsInput::SelectBox(const FVector2D& A, const FVector2D& B, bool bAdd)
{
	const FBox2D Box(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	if (!bAdd) { Selection.Reset(); }
	for (AAntCharacter* Ant : MySquad())
	{
		FVector2D Sp;
		if (PC()->ProjectWorldLocationToScreen(Ant->GetActorLocation(), Sp) && Box.IsInside(Sp)) { Selection.AddUnique(Ant); }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRTS select box (%.0f,%.0f)-(%.0f,%.0f): %d of %d squad ants"), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, Selection.Num(), MySquad().Num());
	SendSelection();
}

void UAntRtsInput::SelectAll()
{
	Selection.Reset();
	for (AAntCharacter* A : MySquad()) { Selection.Add(A); }
	SendSelection();
}

void UAntRtsInput::ClearSelection()
{
	Selection.Reset();
	SendSelection();
}

void UAntRtsInput::SendSelection()
{
	TArray<AAntCharacter*> Ants;
	for (const TWeakObjectPtr<AAntCharacter>& A : Selection) { if (A.IsValid()) { Ants.Add(A.Get()); } }
	if (AAntPlayerController* C = PC()) { C->ServerSwarm(AntRts::CmdSelect, 0, Ants, FVector::ZeroVector, nullptr); }
}

void UAntRtsInput::Probe(bool bAtCursor, AActor*& OutActor, FVector& OutPoint) const
{
	OutActor = nullptr;
	AAntPlayerController* C = PC();
	if (!C)
	{
		return;
	}
	FVector From, Dir;
	if (bAtCursor && bRtsMode && C->DeprojectScreenPositionToWorld(Mouse.X, Mouse.Y, From, Dir))
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntRtsProbe), false, C->GetPawn());
		OutPoint = GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * 30000.f, ECC_Visibility, Q) ? Hit.ImpactPoint : From + Dir * 2000.f;
	}
	else
	{
		OutPoint = C->CrosshairPoint();
	}
	float Best = 220.f * 220.f;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (!AntHaul::CanHaul(*It) || It->bIsGear) { continue; }
		const float D = (float)FVector::DistSquared(It->GetActorLocation(), OutPoint) - FMath::Square(It->GetRadius());
		if (D < Best) { Best = D; OutActor = *It; }
	}
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		if (!IsFoe(*It)) { continue; }
		const float D = (float)FVector::DistSquared(It->GetActorLocation(), OutPoint);
		if (D < Best) { Best = D; OutActor = *It; }
	}
}

void UAntRtsInput::Order(uint8 Task, bool bAtCursor)
{
	AActor* T = nullptr;
	FVector P;
	Probe(bAtCursor, T, P);
	if (Task == AntTask::Follow) { T = nullptr; }
	if (Task == AntTask::Hold || (Task == AntTask::Attack && T && !IsFoe(T))) { T = nullptr; }
	OrderAt(Task, T, P);
}

void UAntRtsInput::OrderAt(uint8 Task, AActor* Target, const FVector& Point)
{
	TArray<AAntCharacter*> Ants;
	for (const TWeakObjectPtr<AAntCharacter>& A : Selection) { if (A.IsValid()) { Ants.Add(A.Get()); } }
	if (AAntPlayerController* C = PC()) { C->ServerSwarm(AntRts::CmdOrder, Task, Ants, Point, Target); }
	const AKitchenLoot* L = Cast<AKitchenLoot>(Target);
	LastOrderAt = GetWorld()->GetTimeSeconds();
	LastOrderText = FString::Printf(TEXT("%s: %s%s"), Ants.Num() > 0 ? *FString::Printf(TEXT("%d муравьёв"), Ants.Num()) : TEXT("весь отряд"), AntTask::Name(Task), L ? *FString::Printf(TEXT(" «%s»"), *L->LootName) : TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("ANTRTS local order %s: %d selected -> %s"), AntTask::Name(Task), Ants.Num(), L ? *L->LootName : *Point.ToCompactString());
}

void UAntRtsInput::NumKey(int32 G)
{
	if (bRtsMode) { SaveGroup(G); } else { RecallGroup(G); }
}

void UAntRtsInput::SaveGroup(int32 G)
{
	if (G < 0 || G > 3) { return; }
	Groups[G] = Selection;
	UE_LOG(LogTemp, Display, TEXT("ANTRTS group %d saved: %d ants"), G + 7, Groups[G].Num());
}

void UAntRtsInput::RecallGroup(int32 G)
{
	if (G < 0 || G > 3) { return; }
	Selection.Reset();
	for (const TWeakObjectPtr<AAntCharacter>& A : Groups[G]) { if (A.IsValid() && !A->bDead) { Selection.Add(A); } }
	UE_LOG(LogTemp, Display, TEXT("ANTRTS group %d selected: %d ants"), (G + 7) % 10, Selection.Num());
	SendSelection();
}

bool UAntRtsInput::Dev(const TArray<FString>& A)
{
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	AAntPlayerController* C = PC();
	if (!C)
	{
		return false;
	}
	if (Op == TEXT("all")) { SelectAll(); }
	else if (Op == TEXT("clear")) { ClearSelection(); }
	else if (Op == TEXT("mode")) { if (Num(2, 1) > 0.5f) { EnterRts(); } else { LeaveRts(); } }
	else if (Op == TEXT("mouse")) { Mouse = FVector2D(Num(2) * Screen.X, Num(3) * Screen.Y); }
	else if (Op == TEXT("box"))
	{
		// rts box x0 y0 x1 y1 (fractions of the screen): drawn as a drag for a moment, then selected.
		const FVector2D P0(Num(2) * Screen.X, Num(3) * Screen.Y), P1(Num(4) * Screen.X, Num(5) * Screen.Y);
		bRtsMode = true;
		bDevHold = true;
		bDragging = true;
		DragFrom = P0;
		Mouse = P1;
		SelectBox(P0, P1, false);
	}
	else if (Op == TEXT("drop")) { bDragging = false; bDevHold = false; bRtsMode = false; }
	else if (Op == TEXT("near"))
	{
		TArray<AAntCharacter*> All = MySquad();
		const FVector Me = C->GetPawn() ? C->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		All.Sort([&Me](const AAntCharacter& X, const AAntCharacter& Y) { return FVector::DistSquared(X.GetActorLocation(), Me) < FVector::DistSquared(Y.GetActorLocation(), Me); });
		Selection.Reset();
		for (int32 i = 0; i < FMath::Min((int32)Num(2, 5), All.Num()); ++i) { Selection.Add(All[i]); }
		SendSelection();
	}
	else if (Op == TEXT("save")) { SaveGroup((int32)Num(2, 7) - 7); }
	else if (Op == TEXT("recall")) { RecallGroup((int32)Num(2, 7) - 7); }
	else if (Op == TEXT("order"))
	{
		const FString What = A.IsValidIndex(2) ? A[2].ToLower() : TEXT("follow");
		const uint8 Task = What == TEXT("haul") ? AntTask::Haul : What == TEXT("hold") ? AntTask::Hold : What == TEXT("attack") ? AntTask::Attack : What == TEXT("free") ? AntTask::Free : AntTask::Follow;
		AActor* T = nullptr;
		FVector P = C->GetPawn() ? C->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		const FString Tgt = A.IsValidIndex(3) ? A[3].ToLower() : FString();
		if (Tgt == TEXT("cake") || Tgt == TEXT("heavy") || Tgt == TEXT("cheese") || Tgt == TEXT("food"))
		{
			// the nearest load of that kind to my ant
			float Best = 1.e12f;
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				if (!AntHaul::CanHaul(*It) || It->bIsGear || It->IsHidden()) { continue; }
				const bool bOk = Tgt == TEXT("cake") ? It->Weight >= 40 && It->LootName.Contains(TEXT("торт"))
					: Tgt == TEXT("cheese") ? It->LootName.Contains(TEXT("сыр"))
					: Tgt == TEXT("heavy") ? It->Weight >= 15 : true;
				if (!bOk) { continue; }
				const float D = (float)FVector::DistSquared(It->GetActorLocation(), P);
				if (D < Best) { Best = D; T = *It; }
			}
			if (T) { P = T->GetActorLocation(); }
			UE_LOG(LogTemp, Display, TEXT("ANTRTS dev target %s: %s"), *Tgt, T ? *Cast<AKitchenLoot>(T)->LootName : TEXT("none"));
		}
		else if (A.IsValidIndex(5))
		{
			P = FVector(Num(3), Num(4), Num(5));
			AActor* Dummy = nullptr;
			if (Task == AntTask::Attack) { (void)Dummy; }
		}
		else if (Tgt == TEXT("aim"))
		{
			Probe(false, T, P);
		}
		OrderAt(Task, T, P);
	}
	else if (Op == TEXT("pick")) { AntSnowball::TryPick(C, (int32)Num(2, 1) - 1); }
	else if (Op == TEXT("shot"))
	{
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("swarm") / (A.IsValidIndex(2) ? A[2] : FString(TEXT("shot"))) + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogTemp, Display, TEXT("ANTRTS shot %s"), *File);
	}
	else if (Op == TEXT("look"))
	{
		// rts look x y z: turn my view to that point
		if (const APawn* Pw = C->GetPawn())
		{
			C->SetControlRotation((FVector(Num(2), Num(3), Num(4)) - Pw->GetActorLocation()).Rotation());
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("ANTRTS info: squad %d, selected %d, mode %d"), MySquad().Num(), Selection.Num(), bRtsMode ? 1 : 0);
	}
	return true;
}

// =============================================================================================
// HUD
// =============================================================================================

namespace AntRts
{
	static void Text(UCanvas* Cv, const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale, bool bCenter = false)
	{
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(S), F, C);
		T.Scale = FVector2D(Scale, Scale);
		T.bCentreX = bCenter;
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Cv->DrawItem(T);
	}

	static void Rect(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
		T.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(T);
	}

	static void Frame(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& C, float Th = 1.5f)
	{
		FCanvasBoxItem B(FVector2D(X, Y), FVector2D(W, H));
		B.SetColor(C);
		B.LineThickness = Th;
		Cv->DrawItem(B);
	}

	void DrawHud(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC)
	{
		if (!Hud || !Cv || !PC || !GEngine)
		{
			return;
		}
		GCardsShown = false;
		const UWorld* W = PC->GetWorld();
		const AAntSnowball* S = AAntSnowball::Get(W);
		if (!S || !S->bActive)
		{
			return;
		}
		const FAntSnowPlayer* P = S->Find(PC->PlayerState);
		UAntRtsInput* In = UAntRtsInput::Of(PC);
		const float K = FMath::Max(0.7f, Cv->ClipY / 1080.f);
		if (In) { In->Screen = FVector2D(Cv->ClipX, Cv->ClipY); }
		UFont* Small = GEngine->GetSmallFont();
		UFont* Med = GEngine->GetMediumFont();
		const float Now = ServerNow(W);

		// ---- The mass meter (left).
		{
			const float X = 24.f * K, Y = 150.f * K, BW = 300.f * K, BH = 14.f * K;
			const float Frac = FMath::Clamp((float)(S->Mass - S->PrevAt) / FMath::Max(1, S->NextAt - S->PrevAt), 0.f, 1.f);
			Text(Cv, FString::Printf(TEXT("МАССА %d · до выбора %d"), S->Mass, FMath::Max(0, S->NextAt - S->Mass)), X, Y - 22.f * K, RtsGold, Med, 1.f * K);
			Rect(Cv, X, Y, BW, BH, FLinearColor(0.f, 0.f, 0.f, 0.5f));
			Rect(Cv, X, Y, BW * Frac, BH, FLinearColor(1.f, 0.75f, 0.2f, 0.9f));
			if (P)
			{
				int32 OnTrails = 0;
				if (const AAntTrailNet* T = AAntTrailNet::Get(W)) { OnTrails = T->WorkersOnTrails(P->PS); }
				Text(Cv, FString::Printf(TEXT("твой вклад %d · выборов %d · отряд %d · рабочие %d (на тропах %d)"), P->Mass, P->Taken.Num(), P->Squad.Num(), P->Workers, OnTrails), X, Y + 18.f * K, RtsGrey, Small, 1.05f * K);
			}
		}
		if (!P)
		{
			return;
		}

		// ---- Groups and their jobs (left, under the meter).
		{
			float Y = 200.f * K;
			const float X = 24.f * K;
			int32 Followers = 0;
			for (uint8 T : P->SquadTask) { Followers += T == 255 ? 1 : 0; }
			if (P->Squad.Num() > 0)
			{
				Text(Cv, FString::Printf(TEXT("ОТРЯД %d · за мной %d"), P->Squad.Num(), Followers), X, Y, RtsGreen, Small, 1.1f * K);
				Y += 18.f * K;
			}
			for (const FAntRtsTask& T : P->Tasks)
			{
				FString Line = FString::Printf(TEXT("  #%d %d %s"), (int32)T.Id, (int32)T.Members, AntTask::Name(T.Order));
				FLinearColor C = RtsGrey;
				if (const AKitchenLoot* L = Cast<AKitchenLoot>(T.Target))
				{
					Line += FString::Printf(TEXT(" «%s»"), *L->LootName);
					Line += T.State == 1 ? FString(TEXT(" — несут домой")) : T.State == 2 ? FString::Printf(TEXT(" — тяжело, нужно ещё %d"), (int32)T.Need) : FString(TEXT(" — идут"));
					C = T.State == 2 ? RtsRed : T.State == 1 ? RtsGreen : RtsGrey;
				}
				Text(Cv, Line, X, Y, C, Small, 1.f * K);
				Y += 16.f * K;
				if (Y > 390.f * K) { break; }
			}
			if (In)
			{
				FString G;
				for (int32 i = 0; i < 4; ++i)
				{
					int32 Alive = 0;
					for (const TWeakObjectPtr<AAntCharacter>& A : In->Groups[i]) { Alive += (A.IsValid() && !A->bDead) ? 1 : 0; }
					if (Alive > 0) { G += FString::Printf(TEXT("[%d] %d  "), (i + 7) % 10, Alive); }
				}
				if (!G.IsEmpty()) { Text(Cv, TEXT("Группы: ") + G, X, Y, RtsGrey, Small, 1.f * K); Y += 16.f * K; }
				if (In->Selection.Num() > 0) { Text(Cv, FString::Printf(TEXT("Выделено: %d"), In->Selection.Num()), X, Y, RtsGreen, Small, 1.f * K); Y += 16.f * K; }
				if (W->GetTimeSeconds() - In->LastOrderAt < 3.f) { Text(Cv, TEXT("Приказ: ") + In->LastOrderText, X, Y, RtsGold, Small, 1.f * K); }
			}
		}

		// ---- Loads in progress: an icon over each.
		for (const FAntRtsTask& T : P->Tasks)
		{
			const AKitchenLoot* L = Cast<AKitchenLoot>(T.Target);
			if (!L) { continue; }
			FVector2D Sp;
			if (!PC->ProjectWorldLocationToScreen(L->GetActorLocation() + FVector(0.f, 0.f, 40.f), Sp)) { continue; }
			const FLinearColor C = T.State == 2 ? RtsRed : T.State == 1 ? RtsGreen : RtsGold;
			Frame(Cv, Sp.X - 12.f * K, Sp.Y - 12.f * K, 24.f * K, 24.f * K, C, 2.f);
			Text(Cv, FString::Printf(TEXT("#%d ▲%d %s"), (int32)T.Id, (int32)T.Members, T.State == 2 ? *FString::Printf(TEXT("+%d?"), (int32)T.Need) : T.State == 1 ? TEXT("тащат") : TEXT("")), Sp.X, Sp.Y - 34.f * K, C, Small, 1.f * K, true);
		}

		// ---- Selected ants: little brackets.
		if (In)
		{
			for (const TWeakObjectPtr<AAntCharacter>& A : In->Selection)
			{
				FVector2D Sp;
				if (A.IsValid() && PC->ProjectWorldLocationToScreen(A->GetActorLocation(), Sp))
				{
					Frame(Cv, Sp.X - 7.f * K, Sp.Y - 7.f * K, 14.f * K, 14.f * K, RtsGreen, 1.5f);
				}
			}
			if (In->bRtsMode)
			{
				Text(Cv, TEXT("ОТРЯД: рамка ЛКМ · клик — один · Shift — добавить · ПКМ — приказ (еда: тащи, враг: атакуй, пол: держи) · Z за мной · X атакуй · Q тащи · B держи · 7–0 — запомнить группу"),
					Cv->ClipX * 0.5f, Cv->ClipY - 150.f * K, RtsGold, Small, 1.05f * K, true);
				if (In->bDragging)
				{
					const FVector2D A0(FMath::Min(In->DragFrom.X, In->Mouse.X), FMath::Min(In->DragFrom.Y, In->Mouse.Y));
					const FVector2D A1(FMath::Max(In->DragFrom.X, In->Mouse.X), FMath::Max(In->DragFrom.Y, In->Mouse.Y));
					Rect(Cv, A0.X, A0.Y, A1.X - A0.X, A1.Y - A0.Y, FLinearColor(0.3f, 1.f, 0.4f, 0.12f));
					Frame(Cv, A0.X, A0.Y, A1.X - A0.X, A1.Y - A0.Y, RtsGreen, 2.f);
				}
			}
		}

		// ---- The pick (bottom centre): three cards, 1/2/3 or a click (with Caps Lock), 10 s.
		if (P->Offer.Num() > 0)
		{
			GCardsShown = true;
			if (!AntUi::Classic())
			{
				// Illustrated cards with rarity frames (AntUiCanvas.cpp); the click rectangles stay this file's.
				AntUiCanvas::DrawCardPick(Cv, P->Offer, P->OfferEnds, Now, P->PicksDone + 1, In ? In->Mouse : FVector2D::ZeroVector, In && In->bRtsMode, GCardRect);
				return;
			}
			const float CW = 330.f * K, CH = 118.f * K, Gap = 18.f * K;
			const float X0 = Cv->ClipX * 0.5f - (CW * 3.f + Gap * 2.f) * 0.5f;
			const float Y0 = Cv->ClipY - CH - 200.f * K;
			const float Left = FMath::Clamp((P->OfferEnds - Now) / 10.f, 0.f, 1.f);
			Text(Cv, FString::Printf(TEXT("ВЫБОР #%d — [1] [2] [3]   (сам выберется через %.0f с)"), P->PicksDone + 1, FMath::Max(0.f, P->OfferEnds - Now)), Cv->ClipX * 0.5f, Y0 - 30.f * K, RtsGold, Med, 1.f * K, true);
			Rect(Cv, X0, Y0 - 6.f * K, (CW * 3.f + Gap * 2.f) * Left, 4.f * K, FLinearColor(1.f, 0.75f, 0.2f, 0.9f));
			for (int32 i = 0; i < 3; ++i)
			{
				GCardRect[i] = FBox2D(ForceInit);
				if (!P->Offer.IsValidIndex(i)) { continue; }
				const uint8 Id = P->Offer[i];
				const float X = X0 + i * (CW + Gap);
				const bool bHover = In && In->bRtsMode && FBox2D(FVector2D(X, Y0), FVector2D(X + CW, Y0 + CH)).IsInside(In->Mouse);
				Rect(Cv, X, Y0, CW, CH, bHover ? FLinearColor(0.12f, 0.1f, 0.04f, 0.9f) : FLinearColor(0.03f, 0.03f, 0.03f, 0.82f));
				Frame(Cv, X, Y0, CW, CH, RtsGold, bHover ? 3.f : 1.5f);
				Text(Cv, FString::Printf(TEXT("[%d]  %s"), i + 1, AntPick::PoolName(AntPick::Pool(Id))), X + 12.f * K, Y0 + 8.f * K, RtsGrey, Small, 1.f * K);
				Text(Cv, AntPick::Name(Id), X + 12.f * K, Y0 + 30.f * K, FLinearColor::White, Med, 1.1f * K);
				Text(Cv, AntPick::Text(Id), X + 12.f * K, Y0 + 70.f * K, RtsGrey, Small, 0.95f * K);
				GCardRect[i] = FBox2D(FVector2D(X, Y0), FVector2D(X + CW, Y0 + CH));
			}
		}
	}
}
