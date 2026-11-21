#include "AntSquad.h"
#include "AntArmy.h"
#include "AntChain.h"
#include "AntRaid.h"
#include "AntRoutes.h"
#include "AntRun.h"
#include "AntCharacter.h"
#include "AntBotController.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "AntCarry.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "KitchenInteractables.h"
#include "LevelV2.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "AntRts.h"

namespace
{
	bool HomePoint(const FVector& P)
	{
		if (FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) < AKitchenBuilder::NestRadius(0) - 120.f && P.Z < 400.f)
		{
			return true;
		}
		return KitchenArt::Enabled() && FVector::Dist2D(P, LevelV2::SouthEntrance()) < LevelV2::SouthEntranceRadius() - 80.f && P.Z < 500.f;
	}

	/** Loot lying near a point: what the crosshair is on. */
	AKitchenLoot* LootAt(UWorld* W, const FVector& P, float Radius)
	{
		AKitchenLoot* Best = nullptr;
		float BestD = Radius;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->bIsGear) { continue; }
			const float D = (float)FVector::Dist(It->GetActorLocation(), P) - It->GetRadius();
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}
}

namespace AntSquad
{

const TCHAR* OrderName(uint8 Order)
{
	static const TCHAR* Names[Count] = { TEXT("за мной"), TEXT("в атаку"), TEXT("вольно: собирают еду"), TEXT("несут"), TEXT("держат место") };
	return Order < Count ? Names[Order] : Names[0];
}

AAntArmy* Find(const AAntCharacter* Commander)
{
	AAntArmy* A = AAntArmy::FindFor(Commander);
	return (A && A->bSquad) ? A : nullptr;
}

int32 SpawnAll(AAntRaid* Raid)
{
	UWorld* W = Raid ? Raid->GetWorld() : nullptr;
	if (!W)
	{
		return 0;
	}
	FAntNestState& N = Raid->Nest;
	const int32 Cap = AntNest::SquadCap(N);
	int32 Out = 0;
	// The workers are shared out evenly: with 12 in the nest and four players the first must not take ten and leave the others two.
	int32 Eligible = 0;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		const AAntCharacter* Ant = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
		Eligible += (Ant && !Ant->bDead && Ant->Team == 0 && Ant->Creature == 0) ? 1 : 0;
	}
	const int32 Pool = N.Workers;
	int32 Seat = 0;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AAntCharacter* Ant = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
		AAntPlayerState* PS = PC ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
		if (!Ant || Ant->bDead || Ant->Team != 0 || Ant->Creature != 0 || !PS)
		{
			continue;
		}
		const int32 Share = Eligible > 1 ? Pool / Eligible + (Seat < Pool % Eligible ? 1 : 0) : Pool;
		++Seat;
		const int32 Want = PS->SquadWant == 255 ? Cap : FMath::Min<int32>(PS->SquadWant, Cap);
		const int32 Take = FMath::Clamp(FMath::Min(Want, Share), 0, N.Workers);
		if (Take <= 0)
		{
			continue;
		}
		N.Workers -= Take;
		const FVector At = Ant->GetActorLocation() - Ant->GetActorForwardVector() * 40.f;
		AAntArmy* A = W->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A)
		{
			N.Workers += Take;
			continue;
		}
		A->bSquad = true;
		A->Team = 0;
		A->Count = Take;
		A->Taken = Take;
		A->Commander = Ant;
		A->SquadOwner = PC;
		A->OwnerPS = PS;
		A->Mode = EArmyMode::Follow;
		A->WorkerStr = AntNest::WorkerStrength(N);
		A->BiteMul = N.Tier_(AntUp::Soldiers) >= 1 ? 2.f : 1.f;
		A->bArmor = N.Tier_(AntUp::Soldiers) >= 3;
		A->bHaulHome = N.Tier_(AntUp::Carriers) >= 1;
		A->HaulSpeedMul = N.Tier_(AntUp::Carriers) >= 3 ? 1.33f : 1.f;
		A->bHoldsDoors = N.Tier_(AntUp::Engineers) >= 1;
		A->BridgeMul = N.Tier_(AntUp::Engineers) >= 2 ? 12 : 4;
		A->bRamps = N.Tier_(AntUp::Engineers) >= 3;
		A->FinishSpawning(FTransform(At));
		Out += Take;
		Ant->Toast(FString::Printf(TEXT("ОТРЯД: %d рабочих идут с тобой. [Z] за мной · [X] фас · [C] вольно · [Q] нести · [B] держать"), Take));
		UE_LOG(LogTemp, Display, TEXT("ANTSQUAD %s takes %d workers (cap %d, asked %d), strength %.0f each, nest has %d left; bite x%.0f, armor %d, hauls home %d, haul speed x%.2f, holds doors %d, bridge links per worker %d, ramps %d"),
			*Ant->GetDisplayName(), Take, Cap, (int32)PS->SquadWant, A->WorkerStr, N.Workers, A->BiteMul, A->bArmor ? 1 : 0, A->bHaulHome ? 1 : 0, A->HaulSpeedMul, A->bHoldsDoors ? 1 : 0, A->BridgeMul, A->bRamps ? 1 : 0);
	}
	return Out;
}

void OnCommanderDied(AAntCharacter* Ant)
{
	AAntArmy* A = Find(Ant);
	if (!A)
	{
		return;
	}
	const int32 Was = A->Count;
	A->DropCargo();
	A->bOrphan = true;
	A->OrphanUntil = A->GetWorld()->GetTimeSeconds() + 90.f;
	A->Commander = nullptr;
	A->Mode = EArmyMode::Hold;
	A->Target = A->GetActorLocation();
	A->Lead.Reset();
	const int32 Gone = Was / 2;
	UE_LOG(LogTemp, Display, TEXT("ANTSQUAD %s died: %d of his %d workers scatter, %d wait by the body for 90 s"), *Ant->GetDisplayName(), Gone, Was, Was - Gone);
	Ant->Toast(FString::Printf(TEXT("Отряд без тебя: %d разбежались, %d ждут у тела 90 с. Тело в гнездо — и они снова твои"), Gone, Was - Gone));
	if (Gone > 0) { A->Lose(Gone); }
}

void OnBodyHome(AController* Owner)
{
	if (!Owner)
	{
		return;
	}
	for (TActorIterator<AAntArmy> It(Owner->GetWorld()); It; ++It)
	{
		if (It->bSquad && It->bOrphan && It->SquadOwner == Owner)
		{
			It->OrphanUntil = -1.f;   // rescued: it joins its owner as soon as he is on his legs again (SquadTick)
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD rescued: %d workers go back to their commander"), It->Count);
		}
	}
}

int32 CountBack(AAntRaid* Raid, bool bWiped, bool bOnlyHome)
{
	int32 Back = 0;
	for (TActorIterator<AAntArmy> It(Raid->GetWorld()); It; ++It)
	{
		AAntArmy* A = *It;
		if (!A->bSquad || A->Team != 0 || bWiped || A->bOrphan || !IsValid(A->Commander) || A->Commander->bDead)
		{
			continue;   // a squad whose commander did not come home is lost
		}
		if (bOnlyHome && !AAntRaid::IsHome(A->Commander))
		{
			continue;
		}
		Back += A->Count;
	}
	// Workers standing in a bridge come home with their commander too (the bridge is taken apart when the raid ends).
	for (TActorIterator<AAntChain> It(Raid->GetWorld()); It; ++It)
	{
		const AAntCharacter* B = It->Builder;
		if (It->WorkersIn <= 0 || bWiped || !IsValid(B) || B->bDead || B->Team != 0 || (bOnlyHome && !AAntRaid::IsHome(B)))
		{
			continue;
		}
		Back += It->LinksAtStart > 0 ? FMath::RoundToInt((float)It->WorkersIn * It->ArmyLinks / It->LinksAtStart) : It->WorkersIn;
	}
	return Back;
}

void OnDoorSlammed(AKitchenInteractable* Door)
{
	if (!Door)
	{
		return;
	}
	for (TActorIterator<AAntArmy> It(Door->GetWorld()); It; ++It)
	{
		if (It->bSquad && It->HeldDoor == Door && It->Count > 0)
		{
			if (IsValid(It->Commander)) { It->Commander->Toast(TEXT("Человек хлопнул дверью по отряду: двое рабочих раздавлены")); }
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD the Human slams the door the squad holds: -2 of %d"), It->Count);
			It->Lose(2);
			return;
		}
	}
}

bool Recall(AAntCharacter* Ant)
{
	if (AntRts::RecallAll(Ant))
	{
		return true;
	}
	AAntArmy* A = Find(Ant);
	if (!A)
	{
		return false;
	}
	A->DropCargo();
	A->Mode = EArmyMode::Follow;
	A->bForaging = false;
	A->Lead.Reset();
	A->bCatchUp = false;
	A->SetActorLocation(Ant->GetActorLocation() - Ant->GetActorForwardVector() * 30.f);
	return true;
}

void Order(AAntCharacter* Ant, uint8 Order, const FVector& Aim)
{
	if (!Ant || Ant->bDead)
	{
		return;
	}
	if (AntRts::SquadKey(Ant, Order, Aim))
	{
		return;
	}
	AAntArmy* A = Find(Ant);
	if (!A)
	{
		Ant->Toast(TEXT("Отряда нет: он остался в гнезде или погиб"));
		return;
	}
	UWorld* W = Ant->GetWorld();
	switch (Order)
	{
	case Follow:
		A->bForaging = false;
		A->AttackWho = nullptr;
		if (A->bCargoHeld)
		{
			A->bCargoHome = false;
			Ant->Toast(TEXT("Отряд: за мной, с грузом"));
		}
		else
		{
			A->Cargo = nullptr;
			A->Mode = EArmyMode::Follow;
			Ant->Toast(FString::Printf(TEXT("Отряд (%d): за мной!"), A->Count));
		}
		break;
	case Attack:
	{
		A->DropCargo();
		A->bForaging = false;
		A->Mode = EArmyMode::Attack;
		A->Target = Aim;
		A->IdleFor = 0.f;
		AAntCharacter* Who = nullptr;
		float Best = 320.f;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			const float D = (float)FVector::Dist(It->GetActorLocation(), Aim);
			if (!It->bDead && It->Team != Ant->Team && D < Best) { Best = D; Who = *It; }
		}
		A->AttackWho = Who;
		Ant->Toast(Who ? FString::Printf(TEXT("Отряд: фас! %s"), *Who->GetDisplayName()) : FString(TEXT("Отряд: в атаку на точку!")));
		AntSfx::Everyone(Ant, AntSfx::ArmyCharge, Ant->GetActorLocation(), 1.3f);
		break;
	}
	case Free:
		A->bForaging = true;
		A->AttackWho = nullptr;
		if (!A->bCargoHeld)
		{
			A->Cargo = nullptr;
			A->Mode = EArmyMode::Follow;
		}
		Ant->Toast(A->bHaulHome ? TEXT("Отряд: вольно. Собирают еду вокруг тебя и носят домой сами") : TEXT("Отряд: вольно. Поднимут еду рядом и понесут с тобой"));
		break;
	case Haul:
	{
		if (A->bCargoHeld && A->Cargo)
		{
			// Pressed again while they carry: home on their own, or back to carrying beside me.
			if (!A->bCargoHome)
			{
				if (A->bHaulHome)
				{
					A->bCargoHome = true;
					A->BuildWayHome();
					Ant->Toast(FString::Printf(TEXT("Отряд понёс «%s» домой сам. [Z] — вернуть к себе"), *A->Cargo->LootName));
				}
				else
				{
					Ant->Toast(TEXT("Сами домой не донесут: это «Носильщики I» в гнезде. Пока несут рядом с тобой"));
				}
			}
			else
			{
				A->bCargoHome = false;
				Ant->Toast(TEXT("Отряд несёт груз рядом с тобой"));
			}
			break;
		}
		AKitchenLoot* L = LootAt(W, Aim, 260.f);
		if (!L && Ant->Carrying) { L = Ant->Carrying; }
		if (!L)
		{
			Ant->Toast(TEXT("Наведи прицел на еду и нажми [Q]: отряд её возьмёт"));
			break;
		}
		const float Need = L->bMain ? (float)L->Weight : L->Weight * 0.5f;
		if (A->Strength() < Need)
		{
			Ant->Toast(FString::Printf(TEXT("«%s» отряду не поднять: сила %.0f из %.0f. Берись сам [E] — отряд поможет"), *L->LootName, A->Strength(), Need));
			break;
		}
		if (L == Ant->Carrying) { Ant->LetGo(); }   // handed over
		A->bForaging = false;
		A->AttackWho = nullptr;
		A->Cargo = L;
		A->bCargoHeld = false;
		A->bCargoHome = false;
		A->Mode = EArmyMode::Haul;
		Ant->Toast(FString::Printf(TEXT("Отряд: взять «%s». Ещё раз [Q] — нести домой самим"), *L->LootName));
		break;
	}
	case Hold:
		A->DropCargo();
		A->bForaging = false;
		A->AttackWho = nullptr;
		A->HeldDoor = nullptr;
		A->Mode = EArmyMode::Hold;
		A->Target = Aim;
		Ant->Toast(A->bHoldsDoors ? TEXT("Отряд: держать место. У двери — откроют и удержат её") : TEXT("Отряд: держать место"));
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSQUAD order %d (%s) by %s at %s: count %d, mode %d, cargo %s held %d home %d"), (int32)Order, OrderName(Order), *Ant->GetDisplayName(), *Aim.ToCompactString(), A->Count, (int32)A->Mode,
		A->Cargo ? *A->Cargo->LootName : TEXT("-"), A->bCargoHeld ? 1 : 0, A->bCargoHome ? 1 : 0);
}

bool HandleCommand(AAntCharacter* Ant, uint8 Cmd, const FVector& Point)
{
	const AAntRaid* Raid = Ant ? AAntRaid::Get(Ant->GetWorld()) : nullptr;
	if (!Raid || Ant->Team != 0)
	{
		return false;   // not a raid: the old army and squad keys
	}
	if (!Raid->InRaid())
	{
		return true;
	}
	// F / Z: follow.  R / X: attack what I aim at (aimed at my own feet: follow).  C: free.
	const uint8 What = (Cmd == 0 || Cmd == 2) ? Follow : (Cmd == 1 || Cmd == 3) ? Attack : Free;
	if (AntRts::SquadKey(Ant, What, Point))
	{
		return true;   // the snowball raid: the keys go to the smart squad (AntRts.cpp)
	}
	if (What == Attack && FVector::Dist(Point, Ant->GetActorLocation()) < 120.f)
	{
		Order(Ant, Follow, Point);
	}
	else if (Find(Ant) || Cmd <= 1)
	{
		Order(Ant, What, Point);
	}
	// The ally bots take the same order (Z, X, C).
	if (Cmd >= 2)
	{
		for (TActorIterator<AAntBotController> It(Ant->GetWorld()); It; ++It)
		{
			AAntCharacter* B = Cast<AAntCharacter>(It->GetPawn());
			if (B && B->Team == Ant->Team && B->Creature == 0)
			{
				It->SetOrder(Cmd == 2 ? 1 : Cmd == 3 ? 2 : 0, Ant, Point);
			}
		}
	}
	return true;
}

}   // namespace AntSquad

// =============================================================================================
// The squad itself: methods of AAntArmy (bSquad)
// =============================================================================================

void AAntArmy::DropCargo()
{
	if (Cargo && bCargoHeld && IsValid(Cargo))
	{
		Cargo->SquadDrop();
		AntCarry::RemoveArmy(Cargo, this);   // the workers let go of their grips (AntCarry)
	}
	Cargo = nullptr;
	bCargoHeld = false;
	bCargoHome = false;
	Way.Reset();
	WayAt = 0;
}

void AAntArmy::BuildWayHome()
{
	// The safer of the two ways in: the route graph, a second under the Human's slipper counts five, floor passages only.
	const FVector Me = GetActorLocation();
	const FVector NestIn = AKitchenBuilder::NestLocation(0) + FVector(250.f, 0.f, 0.f);
	TArray<FVector> W1, W2;
	float C1 = 1.e9f, C2 = 1.e9f;
	AntRoutes::PathPoints(Me, NestIn, true, W1, &C1, AntRoutes::FloorOnly);
	AntRoutes::PathPoints(Me, LevelV2::SouthEntrance(), true, W2, &C2, AntRoutes::FloorOnly);
	// (seconds only; the points beyond the last node are the entrances themselves)
	Way = (KitchenArt::Enabled() && C2 < C1) ? W2 : W1;
	WayAt = 0;
	WaitFor = 0.f;
	FString Line;
	for (const FVector& P : Way) { Line += FString::Printf(TEXT(" (%.0f,%.0f)"), P.X, P.Y); }
	UE_LOG(LogTemp, Display, TEXT("ANTSQUAD way home, %d points (nest %.0f s, south %.0f s):%s"), Way.Num(), C1, C2, *Line);
}

bool AAntArmy::SquadTick(float Dt, FVector& OutGoal)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	FVector Me = GetActorLocation();
	OutGoal = Me;
	AAntCharacter* Cmd = (IsValid(Commander) && !Commander->bDead) ? Commander : nullptr;

	// --- Nest guards: at their post, and at the throat of any red ant within 700 of it ---
	if (bGuard)
	{
		AAntCharacter* Best = nullptr;
		float BestD = 700.f;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* E = *It;
			if (!E || E->bDead || E->Team != 1) { continue; }
			const float D = (float)FVector::Dist(E->GetActorLocation(), GuardPost);
			if (D < BestD) { BestD = D; Best = E; }
		}
		OutGoal = Best ? Best->GetActorLocation() : GuardPost;
		if (FVector::Dist2D(Me, OutGoal) > 12.f) { ServerMove(GroundAt(OutGoal), Dt, Best ? 1.5f : 1.f); }
		if (Best && AttackWho.Get() != Best)
		{
			AttackWho = Best;
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD guards at %s go for %s (%.0f from the post), %d guards"), *GuardPost.ToCompactString(), *Best->GetDisplayName(), BestD, Count);
		}
		return true;
	}

	// --- Its commander is dead: what is left of it waits by the body ---
	if (bOrphan)
	{
		if (OrphanUntil < 0.f)
		{
			// The body is home: back to the owner as soon as he is on his legs.
			AAntCharacter* A = SquadOwner.IsValid() ? Cast<AAntCharacter>(SquadOwner->GetPawn()) : nullptr;
			if (A && !A->bDead && A->Creature == 0)
			{
				Commander = A;
				bOrphan = false;
				OrphanUntil = 0.f;
				Mode = EArmyMode::Follow;
				Lead.Reset();
				bCatchUp = true;
				A->Toast(FString::Printf(TEXT("Отряд снова твой: %d рабочих бегут к тебе"), Count));
			}
		}
		else if (Now > OrphanUntil)
		{
			if (AAntPlayerController* PC = Cast<AAntPlayerController>(SquadOwner.Get())) { PC->ClientToast(TEXT("Отряд разбежался: тело не донесли")); }
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD an orphan squad of %d is lost"), Count);
			Destroy();
			return false;
		}
		return true;
	}
	if (!Cmd)
	{
		// (the pawn is gone without the death hook: its player left, or took the Human)
		DropCargo();
		bOrphan = true;
		OrphanUntil = Now + 90.f;
		Commander = nullptr;
		Mode = EArmyMode::Hold;
		Target = Me;
		return true;
	}
	const FVector CP = Cmd->GetActorLocation();

	// --- The commander's crumbs: the squad walks exactly where he walked (through cracks, up walls) ---
	if (FVector::DistSquared(CP, LastCmdAt) > FMath::Square(700.f))
	{
		Lead.Reset();   // he was moved (a vent, a respawn): catch up by the house's own routes
		bCatchUp = FVector::Dist(Me, CP) > 300.f;
	}
	LastCmdAt = CP;
	if (Lead.Num() == 0 || FVector::DistSquared(Lead.Last(), CP) > FMath::Square(22.f))
	{
		Lead.Add(CP);
		if (Lead.Num() > 600) { Lead.Reset(); bCatchUp = true; }
	}
	auto FollowCrumbs = [&](float SpeedMul)
	{
		if (bCatchUp)
		{
			ServerMove(GroundAt(CP), Dt, 1.5f * SpeedMul);
			if (FVector::Dist(GetActorLocation(), CP) < 220.f) { bCatchUp = false; Lead.Reset(); }
			return;
		}
		const float Speed = (Lead.Num() > 9 ? 640.f : Lead.Num() > 4 ? 480.f : 300.f) * SpeedMul;
		float Budget = Speed * Dt;
		FVector P = Me;
		while (Budget > 0.f && Lead.Num() > 2)
		{
			const FVector To = Lead[0];
			const float D = (float)FVector::Dist(P, To);
			if (D <= Budget) { P = To; Budget -= D; Lead.RemoveAt(0); }
			else { P += (To - P) / D * Budget; Budget = 0.f; }
		}
		if (!P.Equals(Me, 0.01f))
		{
			SetActorLocation(P);
			PushTrail(P);
		}
	};
	auto GoHomeMode = [&]()
	{
		Mode = EArmyMode::Follow;
		Lead.Reset();
		bCatchUp = FVector::Dist(GetActorLocation(), CP) > 300.f;
	};

	// --- Free: something to pick up near the commander? ---
	if (bForaging && !Cargo && Mode == EArmyMode::Follow && Now >= NextForage)
	{
		NextForage = Now + 1.5f;
		AKitchenLoot* Best = nullptr;
		float BestD = 1500.f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			AKitchenLoot* L = *It;
			if (L->bIsGear || L->bBody || L->bMain || L->bHeld || L->IsCarried() || L->bVault || (float)L->Weight > Strength() || HomePoint(L->GetActorLocation()))
			{
				continue;
			}
			const FVector P = L->GetActorLocation();
			if (FMath::Abs(P.Z - CP.Z) > 350.f || KitchenContainers::SealedAt(W, P) >= 0)
			{
				continue;
			}
			const float D = (float)FVector::Dist2D(P, CP) - L->Value * 2.f;   // the dearer piece first
			if (D < BestD) { BestD = D; Best = L; }
		}
		if (Best)
		{
			Cargo = Best;
			bCargoHeld = false;
			bCargoHome = bHaulHome;
			Mode = EArmyMode::Haul;
		}
	}

	switch (Mode)
	{
	case EArmyMode::Attack:
	{
		AAntCharacter* E = AttackWho.Get();
		if (E && (E->bDead || E->Team == Team)) { E = nullptr; }
		if (!E)
		{
			// the next enemy at hand
			float Best = 700.f;
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				const float D = (float)FVector::Dist(It->GetActorLocation(), Me);
				if (!It->bDead && It->Team != Team && !It->IsInNest() && D < Best) { Best = D; E = *It; }
			}
			AttackWho = E;
		}
		OutGoal = E ? E->GetActorLocation() : FVector(Target);
		ServerMove(GroundAt(OutGoal), Dt, 1.4f);
		if (!E && FVector::Dist2D(GetActorLocation(), FVector(Target)) < Radius() * 1.5f + 30.f)
		{
			IdleFor += Dt;
			if (IdleFor > 6.f)
			{
				Cmd->Toast(TEXT("Отряд: врагов нет, возвращаемся"));
				GoHomeMode();
			}
		}
		else
		{
			IdleFor = 0.f;
		}
		break;
	}

	case EArmyMode::Hold:
	{
		OutGoal = FVector(Target);
		if (FVector::Dist(Me, OutGoal) > 25.f)
		{
			ServerMove(GroundAt(OutGoal), Dt, 1.3f);
		}
		// At a door: open it if they are strong enough, and keep it open (Engineers).
		if (bHoldsDoors && Now >= NextDoorCheck && FVector::Dist2D(Me, OutGoal) < 160.f)
		{
			NextDoorCheck = Now + 0.4f;
			AKitchenInteractable* Door = HeldDoor.Get();
			if (!Door)
			{
				float Best = 520.f;
				for (TActorIterator<AKitchenInteractable> It(W); It; ++It)
				{
					const float D = It->DoorClass() > 0 ? It->DistanceTo(OutGoal) : 1.e9f;
					if (D < Best) { Best = D; Door = *It; }
				}
				HeldDoor = Door;
				if (Door && !Door->IsDoorOpen())
				{
					if (!Door->CanPullFrom(Me))
					{
						Cmd->Toast(TEXT("Отряд: с этой стороны дверцу не открыть (засов изнутри)"));
						HeldDoor = nullptr;
						Door = nullptr;
					}
					else if (Strength() < Door->PullNeed())
					{
						Cmd->Toast(FString::Printf(TEXT("Отряд: дверь не по силам (%.0f из %.0f). Тяни сам [E] — помогут"), Strength(), Door->PullNeed()));
					}
				}
			}
			if (Door && Strength() >= Door->PullNeed() && (Door->IsDoorOpen() || Door->CanPullFrom(Me)))
			{
				if (!Door->IsDoorOpen())
				{
					Door->SetDoorOpen(true);
					AntSfx::Everyone(this, AntSfx::FridgeDoor, Door->GetActorLocation(), 1.2f);
					Cmd->Toast(FString::Printf(TEXT("Отряд открыл и держит: %s"), *Door->DoorNoun()));
					UE_LOG(LogTemp, Display, TEXT("ANTSQUAD holds open %s (%s): strength %.0f of %.0f"), *Door->GetName(), *Door->DoorNoun(), Strength(), Door->PullNeed());
				}
				Door->HeldOpenUntil = Now + 1.2f;
			}
		}
		break;
	}

	case EArmyMode::Haul:
	{
		// The squad carries like ants do (AntCarry.h): each worker that fits round the load holds its own grip point and walks
		// towards the goal; the rest of the squad walks alongside. The load is a physics body: it sways, dips at a weak end,
		// overshoots when the crowd is strong, and anybody (the commander, other ants, the red thieves) can take hold too.
		AKitchenLoot* L = Cargo;
		if (!IsValid(L))
		{
			Cargo = nullptr;
			bCargoHeld = false;
			GoHomeMode();
			break;
		}
		const float Need = L->bMain ? (float)L->Weight : L->Weight * 0.5f;
		if (!bCargoHeld)
		{
			if (Cmd->Carrying == L)
			{
				Cargo = nullptr;   // the commander has it himself: following him, the workers take hold as his helpers
				GoHomeMode();
				break;
			}
			if (L->bHeld || (L->IsCarried() && L->CarryTeam() != Team) || Strength() < Need)
			{
				if (!bForaging) { Cmd->Toast(FString::Printf(TEXT("Отряд: «%s» не взять"), *L->LootName)); }
				Cargo = nullptr;
				GoHomeMode();
				break;
			}
			OutGoal = L->GetActorLocation();
			ServerMove(GroundAt(OutGoal), Dt, 1.4f);
			if (FVector::Dist(GetActorLocation(), OutGoal) < Radius() + L->GetRadius() + 30.f)
			{
				const int32 Got = AntCarry::AddArmy(L, this, FMath::Min(Count, AntCarry::MaxCarriers(L)));
				if (Got <= 0)
				{
					if (!bForaging) { Cmd->Toast(FString::Printf(TEXT("Отряд: вокруг «%s» нет места"), *L->LootName)); }
					Cargo = nullptr;
					GoHomeMode();
					break;
				}
				bCargoHeld = true;
				if (bCargoHome) { BuildWayHome(); }
				if (!bForaging) { Cmd->Toast(FString::Printf(TEXT("Отряд несёт «%s»%s"), *L->LootName, bCargoHome ? TEXT(" домой") : TEXT(" рядом с тобой"))); }
				UE_LOG(LogTemp, Display, TEXT("ANTSQUAD picked up «%s» (weight %d, value %d) with strength %.0f, %d workers holding, home %d"), *L->LootName, L->Weight, L->Value, Strength(), Got, bCargoHome ? 1 : 0);
			}
			break;
		}
		// Carrying it.
		if (L->bHeld || AntCarry::ArmyOn(L, this) == 0)
		{
			Cmd->Toast(FString::Printf(TEXT("У отряда отняли «%s»!"), *L->LootName));
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD lost its cargo «%s»"), *L->LootName);
			Cargo = nullptr;
			bCargoHeld = false;
			GoHomeMode();
			break;
		}
		if (Strength() < Need)
		{
			Cmd->Toast(FString::Printf(TEXT("Отряд поредел и уронил «%s»"), *L->LootName));
			DropCargo();
			GoHomeMode();
			break;
		}
		if (AntCarry::ArmyOn(L, this) < FMath::Min(Count, AntCarry::MaxCarriers(L)) && FMath::FRand() < Dt * 2.f)
		{
			AntCarry::AddArmy(L, this, Count);   // workers torn off at a corner run back and take hold again
		}
		const FVector LoadAt = L->GetActorLocation();
		FVector Goal = LoadAt;
		if (bCargoHome && Way.IsValidIndex(WayAt))
		{
			// Home on its own: point by point; it waits when the Human stands close to where it has to go.
			bool bWait = false;
			if (WaitFor < 9.f && !AHumanCharacter::InCover(W, LoadAt))
			{
				const AAntGameState* GS = W->GetGameState<AAntGameState>();
				for (TActorIterator<AHumanCharacter> It(W); It; ++It)
				{
					if (It->Fig == HumanFig::Dad && GS && GS->ShiftStage >= 1 && !It->IsKnockedOut() && FVector::Dist2D(It->GetActorLocation(), LoadAt) < 1300.f) { bWait = true; }
				}
			}
			if (bWait)
			{
				WaitFor += Dt;
			}
			else
			{
				WaitFor = FMath::Max(0.f, WaitFor - Dt * 0.5f);
				Goal = Way[WayAt];
				if (FVector::Dist2D(LoadAt, Goal) < 90.f + L->GetRadius() * 0.5f) { ++WayAt; }
			}
		}
		else if (bCargoHome)
		{
			bCargoHome = false;   // the end of the way and still not home: back to the commander with it
		}
		else
		{
			// beside its commander: to him, stopping a step short (a strong squad still overshoots: ants brake badly)
			Goal = Cmd->GetActorLocation();
			if (FVector::Dist2D(LoadAt, Goal) < L->GetRadius() + 60.f) { Goal = LoadAt; }
		}
		OutGoal = Goal;
		AntCarry::SetArmyGoal(L, this, Goal, HaulSpeedMul);
		ServerMove(GroundAt(LoadAt), Dt, 2.f);   // the squad (and the workers that do not hold) walks with its load
		if (HomePoint(LoadAt))
		{
			// Delivered: the same as an ant carrying it in.
			AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>();
			const FString Name = L->LootName;
			AntCarry::Notify(L, AntCarryEvent::Delivered, 0);
			if (GM)
			{
				TArray<AAntCharacter*> Who;
				Who.Add(Cmd);
				GM->OnFoodDelivered(0, L, Who);
				AntSfx::Everyone(this, AntSfx::Deliver, LoadAt, 1.1f);
			}
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD delivered «%s» at %s"), *Name, *LoadAt.ToCompactString());
			L->ReleaseAll();
			L->Destroy();
			Cargo = nullptr;
			bCargoHeld = false;
			bCargoHome = false;
			GoHomeMode();
		}
		break;
	}

	default:   // Follow
		FollowCrumbs(1.f);
		OutGoal = GetActorLocation();
		break;
	}
	return true;
}
