#include "AntAdmin.h"
#include "AntTutorial.h"
#include "AntUiStyle.h"
#include "AntAudio.h"
#include "AntKitchenGameMode.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "AntGameState.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntVersus.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenFixture.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "AntRaid.h"
#include "AntSnowball.h"
#include "AntTrail.h"
#include "AntRts.h"
#include "AntTypes.h"
#include "AntFilm.h"
#include "AntMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "InputCoreTypes.h"
#include "CanvasItem.h"
#include "AntFighter.h"

// =============================================================================================
// The replicated state
// =============================================================================================

AAntAdminState::AAntAdminState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetNetUpdateFrequency(4.f);
}

void AAntAdminState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntAdminState, Tod);
	DOREPLIFETIME(AAntAdminState, bReveal);
	DOREPLIFETIME(AAntAdminState, bNpcFrozen);
	DOREPLIFETIME(AAntAdminState, Last);
}

void AAntAdminState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The lights of every machine follow the forced time of day (KitchenBuilder::UpdateTimeOfDay fades to it).
	if (AppliedTod != Tod && (Tod >= 0 || AppliedTod >= 0))   // (never touched: -AntTime keeps its say)
	{
		AppliedTod = Tod;
		LevelV3::SetTimeOverride(Tod);   // AKitchenBuilder::UpdateTimeOfDay reads it every frame (unless -AntTime pins the light)
	}
}

AAntAdminState* AAntAdminState::Get(const UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntAdminState> It(World); It; ++It)
	{
		return *It;
	}
	if (bCreate && World->GetNetMode() != NM_Client)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return const_cast<UWorld*>(World)->SpawnActor<AAntAdminState>(AAntAdminState::StaticClass(), FTransform::Identity, P);
	}
	return nullptr;
}

// =============================================================================================
// Shared helpers
// =============================================================================================

namespace
{
	bool GUsed = false;
	TArray<FString> GHistory;

	const FLinearColor AdmGold(0.8879f, 0.3968f, 0.0437f);   // the UI kit's amber (docs/UI_STYLE.md)
	const FLinearColor AdmRed(1.f, 0.4f, 0.35f);
	const FLinearColor AdmGreen(0.2384f, 0.5841f, 0.1022f);   // leaf

	/** Every food of the game, by name (AntKit::RollFood of each tier, sampled) and the four heavy prizes. Same on every machine. */
	const TArray<FAntFoodDef>& Foods()
	{
		static TArray<FAntFoodDef> All;
		if (All.Num() == 0)
		{
			TSet<FString> Seen;
			for (int32 Tier = 0; Tier < 4; ++Tier)
			{
				FRandomStream R(1234 + Tier);
				for (int32 k = 0; k < 400; ++k)
				{
					const FAntFoodDef D = AntKit::RollFood(Tier, R);
					if (!Seen.Contains(D.Name)) { Seen.Add(D.Name); All.Add(D); }
				}
			}
		}
		return All;
	}

	/** NPC kinds the panel names: 0 the Human, 1 cat, 2 dog, 3 grandma, 4 the kid. */
	const TCHAR* NpcName(int32 W)
	{
		static const TCHAR* N[] = { TEXT("Человек"), TEXT("Кошка"), TEXT("Собака"), TEXT("Бабушка"), TEXT("Ребёнок") };
		return N[FMath::Clamp(W, 0, 4)];
	}

	int32 NpcFromWord(const FString& S)
	{
		const FString L = S.ToLower();
		return L == TEXT("human") || L == TEXT("dad") ? 0 : L == TEXT("cat") ? 1 : L == TEXT("dog") ? 2 : L == TEXT("gran") || L == TEXT("grandma") ? 3 : L == TEXT("kid") ? 4 : -1;
	}

	/** The NPC actors of that kind (the Human: the dad only; grandma and the kid are AHouseFolk). */
	void NpcActors(UWorld* W, int32 Who, TArray<AActor*>& Out)
	{
		Out.Reset();
		if (Who == 1 || Who == 2)
		{
			for (TActorIterator<ACritter> It(W); It; ++It) { if (It->Kind == (Who == 1 ? 1 : 0)) { Out.Add(*It); } }
		}
		else
		{
			for (TActorIterator<AHumanCharacter> It(W); It; ++It)
			{
				const bool bGran = Cast<AGrandmaCharacter>(*It) != nullptr, bKid = Cast<AKidCharacter>(*It) != nullptr;
				if ((Who == 0 && !Cast<AHouseFolk>(*It)) || (Who == 3 && bGran) || (Who == 4 && bKid)) { Out.Add(*It); }
			}
		}
	}

	void SetNpcHidden(AActor* A, bool bHide)
	{
		A->SetActorHiddenInGame(bHide);
		A->SetActorEnableCollision(!bHide);
		A->SetActorTickEnabled(!bHide);
		A->CustomTimeDilation = bHide ? 0.f : 1.f;
		if (APawn* P = Cast<APawn>(A))
		{
			if (AController* C = P->GetController()) { C->SetActorTickEnabled(!bHide); }
		}
	}

	/** A point on whatever is under (X, Y), searched from high above. */
	FVector Ground(UWorld* W, float X, float Y, float FromZ = 7000.f)
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntAdminGround), false);
		if (W->LineTraceSingleByChannel(H, FVector(X, Y, FromZ), FVector(X, Y, -500.f), ECC_Visibility, Q))
		{
			return H.ImpactPoint;
		}
		return FVector(X, Y, 60.f);
	}
}

namespace AntAdmin
{
	bool Launched()
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("AntAdmin"));
		return bOn;
	}

	bool CanUse(const APlayerController* PC)
	{
		const UWorld* W = PC ? PC->GetWorld() : nullptr;
		// The Steam demo has no admin panel (docs/DEMO.md); the training stages its beats through StageCommand instead.
		return W && PC->IsLocalController() && W->GetNetMode() != NM_Client && !AntTut::IsDemo();
	}

	void MarkUsed()
	{
		GUsed = true;
	}
}

// =============================================================================================
// The server side: one text command
// =============================================================================================

void AAntKitchenGameMode::StageCommand(const FString& Cmd)
{
	AAntPlayerController* Host = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) { if (It->Get() && It->Get()->IsLocalController()) { Host = Cast<AAntPlayerController>(It->Get()); break; } }
	if (!Host) { DevCommand(Cmd); return; }
	TGuardValue<bool> G(bStaging, true);
	AdminCommand(Host, Cmd);
}

void AAntKitchenGameMode::AdminCommand(AAntPlayerController* By, const FString& Cmd)
{
	UWorld* W = GetWorld();
	if (!By || (!bStaging && !AntAdmin::CanUse(By)))
	{
		// A client of an online game: never. (Its controller is not local on the server.)
		UE_LOG(LogTemp, Warning, TEXT("ANTADMIN REJECTED (not the host) from %s: %s"), By && By->PlayerState ? *By->PlayerState->GetPlayerName() : TEXT("?"), *Cmd);
		if (By) { By->ClientToast(TEXT("Админ-режим есть только у хоста")); }
		return;
	}
	TArray<FString> A;
	Cmd.ParseIntoArrayWS(A);
	if (A.Num() == 0)
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTADMIN %s: %s"), By->PlayerState ? *By->PlayerState->GetPlayerName() : TEXT("host"), *Cmd);
	AAntAdminState* St = AAntAdminState::Get(W, true);
	const FString Op = A[0].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	AAntCharacter* Me = Cast<AAntCharacter>(By->GetPawn());
	APlayerState* PS = By->PlayerState;
	AAntSnowball* Snow = AAntSnowball::Get(W);
	AAntGameState* GS = GetGameState<AAntGameState>();
	FString Done;
	auto Flag = [&](uint8 F, const TCHAR* Name)
	{
		if (!Me) { Done = TEXT("нет муравья"); return; }
		const bool bOn = !(Me->AdminFlags & F);
		Me->SetAdmin(bOn ? (Me->AdminFlags | F) : (Me->AdminFlags & ~F), Me->AdminSpeed);
		Done = FString::Printf(TEXT("%s: %s"), Name, bOn ? TEXT("ВКЛ") : TEXT("выкл"));
	};
	auto SpawnDef = [&](const FAntFoodDef& D, const FVector& At)
	{
		const FTransform TM(At + FVector(0.f, 0.f, 40.f));
		if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			L->InitFood(D);
			L->FinishSpawning(TM);
			return L;
		}
		return (AKitchenLoot*)nullptr;
	};

	// ---------------------------------------------------------------- me
	if (Op == TEXT("god")) { Flag(1, TEXT("Бессмертие")); }
	else if (Op == TEXT("inf")) { Flag(2, TEXT("Бесконечная кислота и рывок")); if (Me) { Me->Acid = AAntCharacter::MaxAcid; } }
	else if (Op == TEXT("fly")) { Flag(4, TEXT("Полёт сквозь стены")); }
	else if (Op == TEXT("strong")) { Flag(8, TEXT("Суперсила")); }
	else if (Op == TEXT("speed") && Me)
	{
		const float X = FMath::Clamp(Num(1, 1.f), 0.1f, 20.f);
		Me->SetAdmin(Me->AdminFlags, X);
		Done = FString::Printf(TEXT("Скорость x%g"), X);
	}
	else if (Op == TEXT("heal") && Me)
	{
		Me->Health = Me->MaxHealth;
		Me->Acid = AAntCharacter::MaxAcid;
		Done = TEXT("Здоровье и кислота полные");
	}
	else if (Op == TEXT("tpto") && Me && A.Num() >= 4)
	{
		Me->ApplyTeleport(Vec(1) + FVector(0.f, 0.f, 40.f));
		Done = FString::Printf(TEXT("Телепорт в %s"), *Vec(1).ToCompactString());
	}
	else if (Op == TEXT("tpd") && Me && LevelV3::Enabled())
	{
		const TConstArrayView<FLevelV2District> D = LevelV3::Districts();
		const int32 i = (int32)Num(1);
		if (D.IsValidIndex(i))
		{
			const FVector P = Ground(W, (D[i].X0 + D[i].X1) * 0.5f, (D[i].Y0 + D[i].Y1) * 0.5f, 2500.f);
			Me->ApplyTeleport(P + FVector(0.f, 0.f, 60.f));
			Done = FString::Printf(TEXT("Телепорт: %s"), D[i].Name);
		}
	}
	else if (Op == TEXT("tpc") && Me && LevelV3::Enabled())
	{
		const TConstArrayView<FLevelV2Callout> C = LevelV3::Callouts();
		const int32 i = (int32)Num(1);
		if (C.IsValidIndex(i))
		{
			Me->ApplyTeleport(Ground(W, C[i].X, C[i].Y, C[i].Z + 300.f) + FVector(0.f, 0.f, 60.f));
			Done = FString::Printf(TEXT("Телепорт: %s"), C[i].Name);
		}
	}
	else if (Op == TEXT("tphome") && Me)
	{
		Me->ApplyTeleport(AKitchenBuilder::NestLocation(0) + FVector(300.f, 0.f, 60.f));
		Done = TEXT("Телепорт: гнездо");
	}
	else if (Op == TEXT("tpcake") && Me)
	{
		if (MainObject.IsValid()) { Me->ApplyTeleport(MainObject->GetActorLocation() + FVector(70.f, 0.f, 40.f)); Done = TEXT("Телепорт: торт"); }
		else { Done = TEXT("Торта нет (заспавни: Спавн → Торт)"); }
	}
	// ---------------------------------------------------------------- spawn
	else if (Op == TEXT("food") && A.Num() >= 5)
	{
		// food x y z <name...>
		FString Name;
		for (int32 i = 4; i < A.Num(); ++i) { Name += (i > 4 ? TEXT(" ") : TEXT("")) + A[i]; }
		for (const FAntFoodDef& D : Foods())
		{
			if (D.Name.Equals(Name, ESearchCase::IgnoreCase)) { SpawnDef(D, Vec(1)); Done = FString::Printf(TEXT("Еда: %s (вес %d, цена %d)"), *D.Name, D.Weight, D.Value); break; }
		}
		if (Done.IsEmpty()) { Done = FString::Printf(TEXT("Нет такой еды: %s"), *Name); }
	}
	else if (Op == TEXT("heavy") && A.Num() >= 5)
	{
		SpawnDef(AntKit::HeavyFood((int32)Num(1)), Vec(2));
		Done = TEXT("Большой приз");
	}
	else if (Op == TEXT("crumbs") && A.Num() >= 5)
	{
		const int32 N = FMath::Clamp((int32)Num(1, 5.f), 1, 200);
		const FVector C = Vec(2);
		int32 Made = 0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D O = FMath::RandPointInCircle(60.f + 25.f * FMath::Sqrt((float)N));
			Made += SpawnFood(C + FVector(O.X, O.Y, 50.f), 0) ? 1 : 0;
		}
		Done = FString::Printf(TEXT("Крошек: %d"), Made);
	}
	else if (Op == TEXT("cake"))
	{
		SpawnMainObject();
		if (A.Num() >= 4 && MainObject.IsValid())
		{
			MainObject->ReleaseAll();
			MainObject->SetActorLocation(Vec(1) + FVector(0.f, 0.f, 80.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Done = MainObject.IsValid() ? FString::Printf(TEXT("Торт: %s"), *MainObject->GetActorLocation().ToCompactString()) : TEXT("Торт не появился");
	}
	else if (Op == TEXT("workers") && PS)
	{
		const int32 N = FMath::Clamp((int32)Num(1, 12.f), 1, 1000);
		if (Snow && Snow->bActive)
		{
			FAntSnowPlayer* P = Snow->For(PS);
			P->Workers += N;
			AntSnowball::StarterTrail(PS, false);
			if (AAntTrailNet* T = AAntTrailNet::Get(W, true)) { T->Rebalance(PS); }
			Done = FString::Printf(TEXT("Рабочих: %d (на тропах %d)"), P->Workers, AAntTrailNet::Get(W) ? AAntTrailNet::Get(W)->WorkersOnTrails(PS) : 0);
		}
		else { Done = TEXT("Рабочие — только в рейде (снежный ком)"); }
	}
	else if (Op == TEXT("squad") && A.Num() >= 2 && FCString::IsNumeric(*A[1]) && PS)
	{
		const int32 N = AntRts::Spawn(PS, FMath::Clamp((int32)Num(1, 1.f), 1, 60), (uint8)Num(2, 0.f));
		Done = FString::Printf(TEXT("Отряд +%d"), N);
	}
	else if (Op == TEXT("reds"))
	{
		AddBots(1, FMath::Clamp((int32)Num(1, 1.f), 1, 20));
		Done = TEXT("Рыжие муравьи добавлены (идут от своего холма)");
	}
	else if (Op == TEXT("allies"))
	{
		int32 Have = 0;
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { Have += (!It->bDead && It->Team == 0 && It->Creature == 0 && !Cast<AAntSquadBot>(It->GetController())) ? 1 : 0; }
		const int32 Want = FMath::Clamp((int32)Num(1, 1.f), 1, FMath::Max(0, 8 - Have));
		if (Want > 0 && Have < 8) { AddBots(0, Want); }
		Done = FString::Printf(TEXT("Муравьёв-игроков (с ботами): %d из 8"), FMath::Min(8, Have + Want));
	}
	else if (Op == TEXT("flyspawn")) { SpawnFly(); Done = TEXT("Муха"); }
	else if ((Op == TEXT("npcshow") || Op == TEXT("npchide")) && A.Num() >= 2)
	{
		const int32 Who = NpcFromWord(A[1]);
		TArray<AActor*> Npcs;
		NpcActors(W, Who, Npcs);
		for (AActor* N : Npcs)
		{
			SetNpcHidden(N, Op == TEXT("npchide"));
			if (Op == TEXT("npcshow") && A.Num() >= 5)
			{
				if (ACritter* C = Cast<ACritter>(N)) { C->DevPlace(Vec(2), Me ? (float)(Me->GetActorLocation() - Vec(2)).Rotation().Yaw : 0.f); }
				else if (ACharacter* H = Cast<ACharacter>(N)) { H->SetActorLocation(Vec(2) + FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight() + 10.f), false, nullptr, ETeleportType::TeleportPhysics); }
			}
		}
		Done = Npcs.Num() == 0 ? FString::Printf(TEXT("%s: нет в этом доме"), NpcName(Who)) : FString::Printf(TEXT("%s: %s"), NpcName(Who), Op == TEXT("npchide") ? TEXT("убран(а)") : TEXT("здесь"));
	}
	// ---------------------------------------------------------------- NPC
	else if (Op == TEXT("freeze"))
	{
		const bool bOn = A.Num() >= 2 ? Num(1) > 0.5f : !(St && St->bNpcFrozen);
		int32 N = 0;
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (!It->IsHidden()) { It->CustomTimeDilation = bOn ? 0.f : 1.f; ++N; } }
		for (TActorIterator<ACritter> It(W); It; ++It) { if (!It->IsHidden()) { It->CustomTimeDilation = bOn ? 0.f : 1.f; ++N; } }
		if (St) { St->bNpcFrozen = bOn; }
		Done = FString::Printf(TEXT("NPC %s (%d)"), bOn ? TEXT("заморожены") : TEXT("разморожены"), N);
	}
	else if (Op == TEXT("rage") && Human)
	{
		Human->SetRage((uint8)FMath::Clamp((int32)Num(1), 0, 3));
		if (GS) { GS->HumanRage = Human->Rage; }
		Done = FString::Printf(TEXT("Ярость Человека: %d"), (int32)Human->Rage);
	}
	else if ((Op == TEXT("npcgo") || Op == TEXT("npcatk") || Op == TEXT("npcsleep") || Op == TEXT("npcwake")) && A.Num() >= 2)
	{
		const int32 Who = NpcFromWord(A[1]);
		const FVector At = A.Num() >= 5 ? Vec(2) : (Me ? Me->GetActorLocation() : FVector::ZeroVector);
		const FVector MeAt = Me ? Me->GetActorLocation() : At;
		TArray<AActor*> Npcs;
		NpcActors(W, Who, Npcs);
		for (AActor* N : Npcs)
		{
			if (N->IsHidden()) { SetNpcHidden(N, false); }
			ACritter* C = Cast<ACritter>(N);
			AHumanCharacter* H = Cast<AHumanCharacter>(N);
			AHouseFolk* F = Cast<AHouseFolk>(N);
			if (Op == TEXT("npcgo"))
			{
				if (C) { C->bDevPuppet = false; C->DevPlace(At, (float)(MeAt - At).Rotation().Yaw); }
				else if (H) { H->SetActorLocation(At + FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight() + 10.f), false, nullptr, ETeleportType::TeleportPhysics); }
			}
			else if (Op == TEXT("npcatk"))
			{
				if (C)
				{
					C->bDevPuppet = false;
					if (C->Kind == 1) { C->Hunt(40.f); }
					if (Me) { C->DevPrey(Me); }
				}
				else if (F) { F->bDevPuppet = false; F->DirectorDoze(false); F->DevAct(Cast<AKidCharacter>(F) ? TEXT("pinch") : TEXT("sweep"), MeAt); }
				else if (H) { H->bDevPuppet = false; H->SetRage(3); if (GS) { GS->HumanRage = 3; } H->DevAct(TEXT("stomp0"), MeAt); }
			}
			else
			{
				const bool bSleep = Op == TEXT("npcsleep");
				if (C) { C->bDevPuppet = bSleep; C->DevWalk = FVector::ZeroVector; C->DevAct(bSleep ? TEXT("sleep") : TEXT("stand"), C->GetActorLocation()); }
				else if (F) { F->DirectorDoze(bSleep); }
				else if (H) { H->bDevPuppet = bSleep; H->DevWalk = FVector::ZeroVector; if (bSleep) { H->DevAct(TEXT("ko"), H->GetActorLocation()); } }
			}
		}
		const TCHAR* What = Op == TEXT("npcgo") ? TEXT("сюда") : Op == TEXT("npcatk") ? TEXT("в атаку") : Op == TEXT("npcsleep") ? TEXT("спит") : TEXT("проснулся");
		Done = Npcs.Num() == 0 ? FString::Printf(TEXT("%s: нет в этом доме"), NpcName(Who)) : FString::Printf(TEXT("%s: %s"), NpcName(Who), What);
	}
	else if ((Op == TEXT("killaim") || Op == TEXT("stunaim")) && A.Num() >= 4)
	{
		const FVector At = Vec(1);
		const bool bKill = Op == TEXT("killaim");
		AActor* Best = nullptr;
		float BestD = 900.f;
		auto Consider = [&](AActor* X, float Bonus)
		{
			if (!X || X == Me || X->IsHidden()) { return; }
			const float D = (float)FVector::Dist(X->GetActorLocation(), At) - Bonus;
			if (D < BestD) { BestD = D; Best = X; }
		};
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && !It->IsPlayerControlled()) { Consider(*It, 0.f); } }
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { Consider(*It, It->GetSimpleCollisionHalfHeight()); }
		for (TActorIterator<ACritter> It(W); It; ++It) { Consider(*It, It->BodyHeight() * 0.5f); }
		if (AAntCharacter* Ant = Cast<AAntCharacter>(Best))
		{
			if (bKill) { Ant->HurtBy(5000.f, TEXT("админ")); } else { Ant->Webbed(6.f); }
			Done = FString::Printf(TEXT("%s: %s"), *Ant->GetDisplayName(), bKill ? TEXT("убит") : TEXT("оглушён"));
		}
		else if (AHumanCharacter* H = Cast<AHumanCharacter>(Best))
		{
			H->DevAct(bKill ? TEXT("ko") : TEXT("stun1"), H->GetActorLocation());
			Done = FString::Printf(TEXT("%s: %s"), *H->GetName(), bKill ? TEXT("нокаут") : TEXT("оглушён"));
		}
		else if (ACritter* C = Cast<ACritter>(Best))
		{
			C->Scare(At, TEXT("админа"));
			Done = FString::Printf(TEXT("%s: напугана и убегает"), *C->DisplayName());
		}
		else { Done = TEXT("У прицела никого"); }
	}
	else if (Op == TEXT("routine") && A.Num() >= 2 && (A[1] == TEXT("day") || A[1] == TEXT("evening") || A[1] == TEXT("night")))
	{
		const int32 T = A[1] == TEXT("day") ? 0 : A[1] == TEXT("evening") ? 1 : 2;
		LevelV3::SetTimeOverride(T);
		if (St) { St->Tod = (int8)T; }
		DevCommand(TEXT("routine plan"));
		Done = FString::Printf(TEXT("Распорядок и свет: %s"), LevelV3::TimeName(T));
	}
	// ---------------------------------------------------------------- world
	else if (Op == TEXT("pause"))
	{
		const bool bOn = A.Num() >= 2 ? Num(1) > 0.5f : !UGameplayStatics::IsGamePaused(W);
		UGameplayStatics::SetGamePaused(W, bOn);
		Done = bOn ? TEXT("Пауза") : TEXT("Игра идёт");
	}
	else if (Op == TEXT("timescale"))
	{
		const float X = FMath::Clamp(Num(1, 1.f), 0.05f, 8.f);
		UGameplayStatics::SetGlobalTimeDilation(W, X);
		Done = FString::Printf(TEXT("Скорость времени x%g"), X);
	}
	else if (Op == TEXT("tod"))
	{
		const int32 T = FMath::Clamp((int32)Num(1, -1.f), -1, 2);
		if (St) { St->Tod = (int8)T; }
		LevelV3::SetTimeOverride(T);
		Done = T < 0 ? FString(TEXT("Время суток: как в сиде")) : FString::Printf(TEXT("Время суток: %s"), LevelV3::TimeName(T));
	}
	else if (Op == TEXT("restart") || Op == TEXT("newseed"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN restart of the level (%s)"), *Op);
		if (GS) { Broadcast(TEXT("Админ: перезапуск уровня"), AdmGold); }
		const FString Map = UGameplayStatics::GetCurrentLevelName(W, true);
		if (W->GetNetMode() == NM_ListenServer) { W->ServerTravel(Map + TEXT("?listen"), true); }   // the clients come along
		else { UGameplayStatics::OpenLevel(W, FName(*Map), true); }
		Done = TEXT("Перезапуск...");
	}
	else if ((Op == TEXT("breakaim") || Op == TEXT("unfastenaim")) && A.Num() >= 4)
	{
		const FVector At = Vec(1);
		AKitchenFixture* Best = nullptr;
		float BestD = 1600.f;
		for (TActorIterator<AKitchenFixture> It(W); It; ++It)
		{
			FVector O, E;
			It->GetActorBounds(false, O, E);
			const float D = FMath::Sqrt((float)FBox(O - E, O + E).ComputeSquaredDistanceToPoint(At));
			if (D < BestD) { BestD = D; Best = *It; }
		}
		if (Best && Op == TEXT("breakaim")) { Best->BreakNow(Me); Done = FString::Printf(TEXT("Сломано: %s"), *Best->DoorNoun()); }
		else if (Best) { DevCommand(FString::Printf(TEXT("fastener undo %s"), *Best->FixId())); Done = FString::Printf(TEXT("Откручено всё у: %s"), *Best->DoorNoun()); }
		else if (Op == TEXT("breakaim"))
		{
			// no fixture near: the nearest breakable food goes to pieces instead
			AKitchenLoot* L = nullptr;
			float LD = 900.f;
			for (TActorIterator<AKitchenLoot> It(W); It; ++It) { const float D = (float)FVector::Dist(It->GetActorLocation(), At); if (It->CanBreak() && D < LD) { LD = D; L = *It; } }
			if (L) { Done = FString::Printf(TEXT("Разбито: %s"), *L->LootName); L->Shatter(L->GetActorLocation()); }
			else { Done = TEXT("У прицела нечего ломать"); }
		}
		else { Done = TEXT("У прицела нет предмета с крепежом"); }
	}
	else if (Op == TEXT("stainaim") && A.Num() >= 5)
	{
		DevCommand(FString::Printf(TEXT("mess stain %s %.0f %.0f %.0f %.0f"), *A[1], Num(2), Num(3), Num(4), Num(5, 140.f)));
		Done = FString::Printf(TEXT("Пятно: %s"), *A[1]);
	}
	else if (Op == TEXT("cleanall"))
	{
		DevCommand(TEXT("mess clean 0 0 0 200000"));
		Done = TEXT("Вся грязь убрана");
	}
	else if (Op == TEXT("rampage") && Human)
	{
		// The wreck branch's rampage: rage 3 and a heavy weapon; with no ant near him he takes it out on the furniture (AntWeapon.cpp).
		Human->bDevPuppet = false;
		Human->SetRage(3);
		if (GS) { GS->HumanRage = 3; }
		DevCommand(TEXT("weapon give bat"));
		Done = TEXT("Погром: ярость 3, бита в руке — громит мебель, если муравьёв рядом нет");
	}
	else if (Op == TEXT("weapon") && A.Num() >= 3 && A[1] == TEXT("give"))
	{
		DevCommand(Cmd);
		Done = FString::Printf(TEXT("Человеку дали: %s"), *A[2]);
	}
	else if (Op == TEXT("noiseaim") && A.Num() >= 4)
	{
		MakeNoise(Vec(1), 1.f, AntNoise::Crash, nullptr, Me);
		Done = TEXT("Грохот у прицела: Человек идёт проверить");
	}
	// ---------------------------------------------------------------- raid
	else if (Op == TEXT("mass"))
	{
		if (Snow && Snow->bActive) { Snow->AddMass(PS, FMath::Max(1, (int32)Num(1, 50.f)), TEXT("админ")); Done = FString::Printf(TEXT("Масса %d / %d"), Snow->Mass, Snow->NextAt); }
		else { Done = TEXT("Масса — только в рейде"); }
	}
	else if (Op == TEXT("picknow"))
	{
		if (Snow && Snow->bActive && PS)
		{
			Snow->AddMass(PS, FMath::Max(1, Snow->NextAt - Snow->Mass), TEXT("админ: выбор"));
			if (FAntSnowPlayer* P = Snow->For(PS)) { if (P->Offer.Num() == 0 && P->PicksDone < Snow->Level) { Snow->MakeOffer(*P); } }
			Done = TEXT("Выбор карточки: [1] [2] [3]");
		}
		else { Done = TEXT("Выбор — только в рейде"); }
	}
	else if (Op == TEXT("allups"))
	{
		if (Snow && Snow->bActive && PS)
		{
			FAntSnowPlayer* P = Snow->For(PS);
			int32 N = 0;
			for (uint8 Id = 0; Id < AntPick::Count; ++Id)
			{
				const uint8 Pool = AntPick::Pool(Id);
				const int32 Times = (Pool == AntPick::PPersonal || Pool == AntPick::PSquadUp || Pool == AntPick::PTrail) ? (Id == AntPick::Grip ? 1 : 3) : (Pool == AntPick::PCards || Pool == AntPick::PTools) ? 1 : 0;
				for (int32 k = 0; k < Times; ++k) { P->Taken.Add(Id); Snow->Apply(*P, Id); ++N; }
			}
			Done = FString::Printf(TEXT("Все улучшения взяты (%d)"), N);
		}
		else if (GS)
		{
			for (uint8 c = 0; c < AntCard::Count; ++c) { GS->Cards.AddUnique(c); }
			Done = TEXT("Все карты колонии взяты");
		}
	}
	else if (Op == TEXT("addtime") && GS)
	{
		GS->TimeRemaining = FMath::Max(1.f, GS->TimeRemaining + Num(1, 300.f));
		Done = FString::Printf(TEXT("Осталось времени: %.0f с"), GS->TimeRemaining);
	}
	else if (Op == TEXT("settime") && GS)
	{
		GS->TimeRemaining = FMath::Max(1.f, Num(1, 30.f));
		Done = FString::Printf(TEXT("Осталось времени: %.0f с"), GS->TimeRemaining);
	}
	else if (Op == TEXT("cakehome"))
	{
		if (MainObject.IsValid())
		{
			MainObject->ReleaseAll();
			MainObject->SetActorLocation(AKitchenBuilder::NestLocation(0) + FVector(300.f, 0.f, 80.f), false, nullptr, ETeleportType::TeleportPhysics);
			Done = TEXT("Торт у входа в гнездо");
		}
		else { Done = TEXT("Торта нет"); }
	}
	else if (Op == TEXT("win") || Op == TEXT("lose") || Op == TEXT("evac"))
	{
		if (Raid && Raid->InRaid())
		{
			if (Op == TEXT("win") && MainObject.IsValid())
			{
				MainObject->ReleaseAll();
				MainObject->SetActorLocation(AKitchenBuilder::NestLocation(0) + FVector(150.f, 0.f, 60.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (Op != TEXT("lose") && Me) { Me->ApplyTeleport(AKitchenBuilder::NestLocation(0) + FVector(200.f, 120.f, 60.f)); }
			const FString End = Op == TEXT("lose") ? TEXT("raid end wipe") : TEXT("raid end");
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, End]() { DevCommand(End); }), Op == TEXT("win") ? 2.5f : 0.2f, false);
			Done = Op == TEXT("win") ? TEXT("Победа: торт дома, рейд кончается") : Op == TEXT("lose") ? TEXT("Поражение") : TEXT("Эвакуация: рейд кончается");
		}
		else
		{
			DevCommand(Op == TEXT("lose") ? TEXT("lose") : TEXT("win"));
			Done = Op;
		}
	}
	else if (Op == TEXT("reveal") && St)
	{
		St->bReveal = A.Num() >= 2 ? Num(1) > 0.5f : !St->bReveal;
		Done = St->bReveal ? TEXT("Видно всех сквозь стены") : TEXT("Метки скрыты");
	}
	// ---------------------------------------------------------------- everything else: the -AntDo set
	else
	{
		DevCommand(Cmd);
		Done = FString::Printf(TEXT("> %s"), *Cmd);
	}
	if (!Done.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN done: %s"), *Done);
		if (St) { St->Last = Done; St->ForceNetUpdate(); }
		if (!bStaging) { By->ClientToast(FString::Printf(TEXT("Админ: %s"), *Done)); }
	}
}

// =============================================================================================
// Help: every command of the command line
// =============================================================================================

const TArray<FString>& AntAdmin::Help()
{
	static TArray<FString> H;
	if (H.Num() == 0)
	{
		H = {
			TEXT("god — бессмертие (вкл/выкл)"),
			TEXT("inf — бесконечная кислота и рывок"),
			TEXT("fly — полёт сквозь стены (Пробел вверх, Ctrl вниз)"),
			TEXT("strong — суперсила: унести что угодно одному"),
			TEXT("speed X — скорость муравья x1..x20"),
			TEXT("heal — полное здоровье и кислота"),
			TEXT("caste N — каста 0 фуражир, 1 солдат, 2 разведчик, 3 бочка"),
			TEXT("tpto x y z — телепорт в точку"),
			TEXT("tphome / tpcake — телепорт в гнездо / к торту"),
			TEXT("tpd N / tpc N — телепорт в район / к месту карты v3"),
			TEXT("food x y z Имя — еда по имени (кнопки во вкладке Спавн)"),
			TEXT("heavy K x y z — приз: 0 торт, 1 мишка, 2 шоколад, 3 сыр"),
			TEXT("crumbs N x y z — N крошек"),
			TEXT("cake [x y z] — новый торт (туда)"),
			TEXT("cakehome — торт ко входу в гнездо"),
			TEXT("workers N — рабочие на тропы (+стартовая тропа)"),
			TEXT("squad N [каста] — отряд +N"),
			TEXT("squad+ N — отряд +N (команда ветки swarm)"),
			TEXT("reds N — рыжие муравьи +N"),
			TEXT("allies N — боты-игроки (до 8 муравьёв)"),
			TEXT("flyspawn — муха"),
			TEXT("npcshow who [x y z] / npchide who — NPC: human cat dog gran kid"),
			TEXT("npcgo who x y z — NPC сюда"),
			TEXT("npcatk who — NPC атакует тебя"),
			TEXT("npcsleep who / npcwake who — уснуть / проснуться"),
			TEXT("freeze [0|1] — заморозить всех NPC"),
			TEXT("rage N — ярость Человека 0..3"),
			TEXT("hplay — вселиться в Человека (ещё раз — вернуться)"),
			TEXT("hpuppet 1|0, hact stomp0|swat|grab|ko|... — Человек-кукла"),
			TEXT("heavy play|patience|strength K|mass K|topple — тяжёлое тело Человека (и move/aim/sprint/swing/stomp/jump/kick/throw для тестов)"),
			TEXT("killaim x y z / stunaim x y z — убить / оглушить у прицела"),
			TEXT("routine day|evening|night — распорядок NPC и свет"),
			TEXT("routine info|plan|warp S|skip who|off|on — распорядок v3"),
			TEXT("npclevel N — смелость питомцев и семьи 0..2"),
			TEXT("cat|dog act X / hunt / scare / zoom — питомцы"),
			TEXT("gran|kid act X — семья"),
			TEXT("pause [0|1] — пауза"),
			TEXT("timescale X — скорость времени 0.05..8"),
			TEXT("tod -1|0|1|2 — время суток (как в сиде / день / вечер / ночь)"),
			TEXT("seed N | seed random | seed info — сид уровня v3"),
			TEXT("restart — перезапуск уровня (новый сид)"),
			TEXT("breakaim x y z — сломать предмет у прицела"),
			TEXT("unfastenaim x y z — открутить все крепежи предмета"),
			TEXT("chain C1..C6 / state chaos — цепные реакции"),
			TEXT("fastener list | fastener undo id|all — крепёж"),
			TEXT("stainaim вид x y z [размер] — пятно (jam honey cola oil cream crumbs mud chocolate juice sugar water)"),
			TEXT("mess info|clean x y z r|dust 0|1|break вид x y z — грязь"),
			TEXT("cleanall — убрать всю грязь"),
			TEXT("wreck at x y z радиус импульс урон / wreck info|hit|tip|drop — разрушаемая мебель"),
			TEXT("weapon give none|slipper|ladle|spatula|book|pin|pan|broom|bat|sausage — оружие Человека"),
			TEXT("weapon swap sausage — подменить оружие колбасой"),
			TEXT("rampage — погром: ярость 3 и бита"),
			TEXT("versus start client|host|ai — режим «Против»"),
			TEXT("noiseaim x y z — грохот: Человек идёт проверить"),
			TEXT("mass N — масса снежного кома +N"),
			TEXT("picknow — сразу выбор карточки"),
			TEXT("allups — все улучшения"),
			TEXT("snow info|pick N|auto N|off|give id|offer — снежный ком"),
			TEXT("trail info|lay ...|crumbs N R|rich|fetch|toend — тропы"),
			TEXT("rts ... — RTS-управление отрядом (ветка swarm)"),
			TEXT("perf S W — большой отряд и рабочие для проверки fps"),
			TEXT("addtime S / settime S — время рейда"),
			TEXT("win / lose / evac — победа / поражение / эвакуация"),
			TEXT("reveal — видеть всех сквозь стены"),
			TEXT("raid info|start|end|time S|home|cake — рейд"),
			TEXT("nest food|jelly|larvae|workers|tier N — гнездо"),
			TEXT("carry spawn|crew N|go x y|info|... — перенос вместе"),
			TEXT("ant info|hp N|leg N|die kind — тело муравья"),
			TEXT("give W V — груз веса W в жвалы"),
			TEXT("bots N / enemy / helpers N — боты рядом"),
			TEXT("event N — событие смены (1 блэкаут, 2 кошка, ...)"),
			TEXT("cap ... — съёмка (ветка capture)"),
			TEXT("q команда — очередь команд"),
			TEXT("con команда — консоль движка на этой машине (stat fps, HighResShot 2)"),
			TEXT("stat fps / stat unit — статистика на экране"),
		};
	}
	return H;
}

// =============================================================================================
// The panel (Slate)
// =============================================================================================

class SAntAdminPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAntAdminPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAntPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	void FocusLine();

private:
	TWeakObjectPtr<AAntPlayerController> PC;
	TSharedPtr<SVerticalBox> Body;
	TSharedPtr<SWrapBox> Row;
	TSharedPtr<SHorizontalBox> Tabs;
	TSharedPtr<SEditableTextBox> Line;
	TSharedPtr<SEditableTextBox> SeedBox;
	TSharedPtr<STextBlock> Hints;
	int32 Tab = 0;
	int32 HistPos = -1;
	FString Typed;

	void Rebuild();
	void Run(const FString& Cmd);
	/** "x y z" of the point under the crosshair. */
	FString Aim() const;
	void Section(const FString& Title);
	void Btn(const FString& Label, TFunction<void()> Fn, const FLinearColor& Color = FLinearColor(0.85f, 0.85f, 0.85f));
	void BtnL(TFunction<FString()> Label, TFunction<void()> Fn, const FLinearColor& Color = FLinearColor(0.85f, 0.85f, 0.85f));
	void UpdateHints(const FString& Text);
	AAntCharacter* MyAnt() const { return PC.IsValid() ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr; }
};

static const TCHAR* GTabNames[] = { TEXT("Я"), TEXT("Телепорт"), TEXT("Спавн"), TEXT("NPC"), TEXT("Мир"), TEXT("Рейд"), TEXT("Колония"), TEXT("Экран") };
static int32 GLastTab = 0;

void SAntAdminPanel::Construct(const FArguments& InArgs)
{
	PC = InArgs._PC;
	Tab = GLastTab;
	const FSlateFontInfo Small = AntUi::Font(AntUi::EFont::Body, 14);
	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Fill)
	.Padding(FMargin(12.f, 60.f, 0.f, 20.f))
	[
		SNew(SBox).WidthOverride(640.f)
		[
			SNew(SBorder)
			.BorderImage(AntUi::Rounded(12.f))
			.BorderBackgroundColor(FLinearColor(0.0110f, 0.0060f, 0.0033f, 0.9f))   // ink glass of the UI kit
			.Padding(10.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("АДМИН-ПАНЕЛЬ   ·   F10 / Esc — закрыть   ·   кнопки «у прицела» бьют в центр экрана")))
					.Font(AntUi::Font(AntUi::EFont::Bold, 15))
					.ColorAndOpacity(AdmGold)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SAssignNew(Tabs, SHorizontalBox)
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(Body, SVerticalBox)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const AAntAdminState* St = PC.IsValid() ? AAntAdminState::Get(PC->GetWorld()) : nullptr;
						return FText::FromString(St && !St->Last.IsEmpty() ? St->Last : FString(TEXT("Готов. Команда + Enter, ↑/↓ — история, Tab — дополнить")));
					})
					.Font(Small)
					.ColorAndOpacity(AdmGreen)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SAssignNew(Line, SEditableTextBox)
					.Font(AntUi::Font(AntUi::EFont::Mono, 14))
					.HintText(FText::FromString(TEXT("команда: god, fly, speed 5, mass 500, cat act sleep, seed 7 ...")))
					.ClearKeyboardFocusOnCommit(false)
					.SelectAllTextOnCommit(false)
					.OnTextChanged_Lambda([this](const FText& T) { UpdateHints(T.ToString()); })
					.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type How)
					{
						if (How != ETextCommit::OnEnter) { return; }
						const FString C = T.ToString().TrimStartAndEnd();
						if (C.IsEmpty()) { return; }
						GHistory.Remove(C);
						GHistory.Add(C);
						HistPos = -1;
						Run(C);
						Line->SetText(FText::GetEmpty());
						FocusLine();
					})
					.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& K) -> FReply
					{
						if (K.GetKey() == EKeys::Up || K.GetKey() == EKeys::Down)
						{
							if (GHistory.Num() == 0) { return FReply::Handled(); }
							if (HistPos < 0) { HistPos = GHistory.Num(); }
							HistPos = FMath::Clamp(HistPos + (K.GetKey() == EKeys::Up ? -1 : 1), 0, GHistory.Num());
							Line->SetText(FText::FromString(GHistory.IsValidIndex(HistPos) ? GHistory[HistPos] : FString()));
							return FReply::Handled();
						}
						if (K.GetKey() == EKeys::Tab)
						{
							const FString Cur = Line->GetText().ToString().TrimStart();
							for (const FString& H : AntAdmin::Help())
							{
								if (!Cur.IsEmpty() && H.StartsWith(Cur, ESearchCase::IgnoreCase))
								{
									FString Word;
									H.Split(TEXT(" "), &Word, nullptr);
									Line->SetText(FText::FromString(Word + TEXT(" ")));
									break;
								}
							}
							return FReply::Handled();
						}
						if (K.GetKey() == EKeys::Escape && PC.IsValid())
						{
							PC->ToggleAdmin();
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SAssignNew(Hints, STextBlock)
					.Font(Small)
					.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.7f))
				]
			]
		]
	];
	UpdateHints(FString());
	Rebuild();
}

void SAntAdminPanel::FocusLine()
{
	if (Line.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Line, EFocusCause::SetDirectly);
	}
}

FReply SAntAdminPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && PC.IsValid())
	{
		PC->ToggleAdmin();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SAntAdminPanel::UpdateHints(const FString& Text)
{
	if (!Hints.IsValid())
	{
		return;
	}
	const FString Cur = Text.TrimStart();
	TArray<FString> Out;
	for (const FString& H : AntAdmin::Help())
	{
		if (Cur.IsEmpty() || H.StartsWith(Cur.Left(FMath::Max(1, Cur.Find(TEXT(" ")) < 0 ? Cur.Len() : Cur.Find(TEXT(" ")))), ESearchCase::IgnoreCase))
		{
			Out.Add(H);
		}
		if (Out.Num() >= 6) { break; }
	}
	Hints->SetText(FText::FromString(Cur.IsEmpty() ? FString(TEXT("Пример: god · fly · speed 5 · mass 500 · seed 7 · cat act sleep · stat fps (полный список — docs/ADMIN.md)")) : FString::Join(Out, TEXT("\n"))));
}

FString SAntAdminPanel::Aim() const
{
	const FVector P = PC.IsValid() ? PC->CrosshairPoint() : FVector::ZeroVector;
	return FString::Printf(TEXT("%.0f %.0f %.0f"), P.X, P.Y, P.Z);
}

void SAntAdminPanel::Run(const FString& Cmd)
{
	if (PC.IsValid())
	{
		PC->Admin(Cmd);
	}
}

void SAntAdminPanel::Section(const FString& Title)
{
	Body->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Title))
		.Font(AntUi::Font(AntUi::EFont::Bold, 16))
		.ColorAndOpacity(AdmGold)
	];
	Body->AddSlot().AutoHeight()
	[
		SAssignNew(Row, SWrapBox).UseAllottedSize(true)
	];
}

void SAntAdminPanel::Btn(const FString& Label, TFunction<void()> Fn, const FLinearColor& Color)
{
	BtnL([Label]() { return Label; }, MoveTemp(Fn), Color);
}

void SAntAdminPanel::BtnL(TFunction<FString()> Label, TFunction<void()> Fn, const FLinearColor& Color)
{
	if (!Row.IsValid())
	{
		Section(TEXT(""));
	}
	Row->AddSlot().Padding(3.f)
	[
		SNew(SButton)
			.ButtonStyle(&AntUi::ButtonStyle())   // paper tags of the UI kit
		.ContentPadding(FMargin(9.f, 5.f))
		.ButtonColorAndOpacity(Color)
		.OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text_Lambda([Label]() { return FText::FromString(Label()); })
			.Font(AntUi::Font(AntUi::EFont::Bold, 14))
			.ColorAndOpacity(FLinearColor::Black)
		]
	];
}

void SAntAdminPanel::Rebuild()
{
	GLastTab = Tab;
	Tabs->ClearChildren();
	for (int32 i = 0; i < UE_ARRAY_COUNT(GTabNames); ++i)
	{
		Tabs->AddSlot().AutoWidth().Padding(2.f)
		[
			SNew(SButton)
			.ButtonStyle(&AntUi::ButtonStyle())   // paper tags of the UI kit
			.ContentPadding(FMargin(10.f, 5.f))
			.ButtonColorAndOpacity(i == Tab ? FLinearColor(1.f, 0.75f, 0.35f) : FLinearColor(0.62f, 0.6f, 0.56f))
			.OnClicked_Lambda([this, i]() { Tab = i; Rebuild(); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(GTabNames[i]))
				.Font(AntUi::Font(AntUi::EFont::Bold, 15))
				.ColorAndOpacity(FLinearColor::Black)
			]
		];
	}
	Body->ClearChildren();
	Row.Reset();
	const FLinearColor On(0.55f, 1.f, 0.55f), Danger(1.f, 0.55f, 0.45f), Blue(0.6f, 0.8f, 1.f);
	auto R = [this](const FString& C) { return [this, C]() { Run(C); }; };
	auto RA = [this](const FString& C) { return [this, C]() { Run(C + TEXT(" ") + Aim()); }; };
	auto FlagLabel = [this](uint8 F, const FString& Name)
	{
		return [this, F, Name]() { const AAntCharacter* M = MyAnt(); return Name + ((M && (M->AdminFlags & F)) ? TEXT(": ВКЛ") : TEXT(": выкл")); };
	};
	auto FlagColor = [](bool) { return FLinearColor(0.85f, 0.85f, 0.85f); };
	(void)FlagColor;

	if (Tab == 0)   // ---------------------------------------------------------------- me
	{
		Section(TEXT("Режимы (кнопка — вкл/выкл)"));
		BtnL(FlagLabel(1, TEXT("Бессмертие")), R(TEXT("god")), On);
		BtnL(FlagLabel(2, TEXT("Бесконечная кислота и рывок")), R(TEXT("inf")), On);
		BtnL(FlagLabel(4, TEXT("Полёт сквозь стены")), R(TEXT("fly")), On);
		BtnL(FlagLabel(8, TEXT("Суперсила")), R(TEXT("strong")), On);
		Btn(TEXT("Вылечить"), R(TEXT("heal")));
		Section(TEXT("Полёт: Пробел — вверх, Ctrl — вниз, смотри куда летишь"));
		Section(TEXT("Скорость"));
		for (const TCHAR* S : { TEXT("1"), TEXT("2"), TEXT("5"), TEXT("10") })
		{
			const FString X(S);
			BtnL([this, X]() { const AAntCharacter* M = MyAnt(); const bool bCur = M && FMath::IsNearlyEqual(M->AdminSpeed, FCString::Atof(*X)); return FString::Printf(TEXT("%sx%s"), bCur ? TEXT("● ") : TEXT(""), *X); }, R(TEXT("speed ") + X));
		}
		Section(TEXT("Телепорт"));
		Btn(TEXT("В точку прицела"), RA(TEXT("tpto")), Blue);
		Btn(TEXT("В гнездо"), R(TEXT("tphome")), Blue);
		Btn(TEXT("К торту"), R(TEXT("tpcake")), Blue);
		Btn(TEXT("Все места карты →"), [this]() { Tab = 1; Rebuild(); }, Blue);
		Section(TEXT("Каста"));
		Btn(TEXT("Фуражир"), R(TEXT("caste 0")));
		Btn(TEXT("Солдат"), R(TEXT("caste 1")));
		Btn(TEXT("Разведчик"), R(TEXT("caste 2")));
		Btn(TEXT("Бочка"), R(TEXT("caste 3")));
		Section(TEXT("Груз"));
		Btn(TEXT("Взять ближайшее"), R(TEXT("grab")));
		Btn(TEXT("Груз 10 в жвалы"), R(TEXT("give 10 10")));
		Btn(TEXT("Груз 60 в жвалы"), R(TEXT("give 60 30")));
		Btn(TEXT("Отряд держит со мной (4)"), R(TEXT("helpers 4")));
	}
	else if (Tab == 1)   // ---------------------------------------------------------------- teleport
	{
		Section(TEXT("Главное"));
		Btn(TEXT("Гнездо"), R(TEXT("tphome")), Blue);
		Btn(TEXT("Торт"), R(TEXT("tpcake")), Blue);
		Btn(TEXT("В точку прицела"), RA(TEXT("tpto")), Blue);
		if (LevelV3::Enabled())
		{
			Section(TEXT("Районы квартиры (v3)"));
			const TConstArrayView<FLevelV2District> D = LevelV3::Districts();
			for (int32 i = 0; i < D.Num(); ++i) { Btn(D[i].Name, R(FString::Printf(TEXT("tpd %d"), i)), Blue); }
			Section(TEXT("Места (стол, холодильник, ...)"));
			const TConstArrayView<FLevelV2Callout> C = LevelV3::Callouts();
			for (int32 i = 0; i < C.Num(); ++i) { Btn(C[i].Name, R(FString::Printf(TEXT("tpc %d"), i)), Blue); }
		}
		else
		{
			Section(TEXT("Не уровень v3: районов нет, работает «в точку прицела»"));
		}
	}
	else if (Tab == 2)   // ---------------------------------------------------------------- spawn
	{
		Section(TEXT("Еда у прицела"));
		for (const FAntFoodDef& F : Foods())
		{
			Btn(FString::Printf(TEXT("%s (%d)"), *F.Name, F.Weight), [this, Name = F.Name]() { Run(FString::Printf(TEXT("food %s %s"), *Aim(), *Name)); });
		}
		Btn(TEXT("Крошки ×5"), [this]() { Run(TEXT("crumbs 5 ") + Aim()); });
		Btn(TEXT("Крошки ×20"), [this]() { Run(TEXT("crumbs 20 ") + Aim()); });
		Section(TEXT("Торт"));
		Btn(TEXT("Новый торт (на своё место)"), R(TEXT("cake")), On);
		Btn(TEXT("Новый торт у прицела"), RA(TEXT("cake")), On);
		Btn(TEXT("Торт домой (ко входу)"), R(TEXT("cakehome")), On);
		Section(TEXT("Муравьи"));
		Btn(TEXT("Рабочие +12"), R(TEXT("workers 12")));
		Btn(TEXT("Рабочие +50"), R(TEXT("workers 50")));
		Btn(TEXT("Отряд +1"), R(TEXT("squad 1")));
		Btn(TEXT("Отряд +5"), R(TEXT("squad 5")));
		Btn(TEXT("Отряд +10"), R(TEXT("squad 10")));
		Btn(TEXT("Солдаты +3"), R(TEXT("squad 3 1")));
		Btn(TEXT("Боты-игроки +1"), R(TEXT("allies 1")));
		Btn(TEXT("Боты-игроки до 8"), R(TEXT("allies 8")));
		Btn(TEXT("Рыжие +1"), R(TEXT("reds 1")), Danger);
		Btn(TEXT("Рыжие +5"), R(TEXT("reds 5")), Danger);
		Btn(TEXT("Рыжий передо мной"), R(TEXT("enemy")), Danger);
		Btn(TEXT("Муха"), R(TEXT("flyspawn")), Danger);
		Section(TEXT("NPC: появиться у прицела / убрать"));
		for (const TCHAR* Who : { TEXT("human"), TEXT("cat"), TEXT("dog"), TEXT("gran"), TEXT("kid") })
		{
			const FString W(Who);
			const FString Name = NpcName(NpcFromWord(W));
			Btn(Name + TEXT(" сюда"), [this, W]() { Run(TEXT("npcshow ") + W + TEXT(" ") + Aim()); }, On);
			Btn(Name + TEXT(" убрать"), R(TEXT("npchide ") + W), Danger);
		}
	}
	else if (Tab == 3)   // ---------------------------------------------------------------- NPC
	{
		Section(TEXT("Все NPC"));
		BtnL([this]() { const AAntAdminState* St = PC.IsValid() ? AAntAdminState::Get(PC->GetWorld()) : nullptr; return (St && St->bNpcFrozen) ? FString(TEXT("Разморозить всех")) : FString(TEXT("Заморозить всех")); }, R(TEXT("freeze")), Blue);
		Btn(TEXT("Все спят"), [this]() { for (const TCHAR* W : { TEXT("human"), TEXT("cat"), TEXT("dog"), TEXT("gran"), TEXT("kid") }) { Run(FString(TEXT("npcsleep ")) + W); } });
		Btn(TEXT("Все проснулись"), [this]() { for (const TCHAR* W : { TEXT("human"), TEXT("cat"), TEXT("dog"), TEXT("gran"), TEXT("kid") }) { Run(FString(TEXT("npcwake ")) + W); } });
		Btn(TEXT("Смелость 0"), R(TEXT("npclevel 0")));
		Btn(TEXT("Смелость 1"), R(TEXT("npclevel 1")));
		Btn(TEXT("Смелость 2"), R(TEXT("npclevel 2")));
		Section(TEXT("Ярость Человека"));
		for (int32 i = 0; i <= 3; ++i) { Btn(FString::Printf(TEXT("Ярость %d"), i), R(FString::Printf(TEXT("rage %d"), i)), i == 3 ? Danger : FLinearColor(0.85f, 0.85f, 0.85f)); }
		Btn(TEXT("Вселиться в Человека / выйти"), R(TEXT("hplay")), Blue);
		Btn(TEXT("Погром"), R(TEXT("rampage")), Danger);
		Section(TEXT("Человек: тяжёлое тело (ЛКМ удар, Пробел топот, Ctrl пинок, Q бросок, Shift бег)"));
		Btn(TEXT("Играть за Человека"), R(TEXT("heavy play")), Blue);
		BtnL([]() { return AntVersus::InfinitePatience() ? FString(TEXT("Бесконечное терпение: ВКЛ")) : FString(TEXT("Бесконечное терпение: выкл")); }, R(TEXT("heavy patience")), On);
		Btn(TEXT("Сила x1"), R(TEXT("heavy strength 1")));
		Btn(TEXT("Сила x2"), R(TEXT("heavy strength 2")), Danger);
		Btn(TEXT("Сила x5"), R(TEXT("heavy strength 5")), Danger);
		Btn(TEXT("Масса тела x1"), R(TEXT("heavy mass 1")));
		Btn(TEXT("Масса тела x2"), R(TEXT("heavy mass 2")), Danger);
		Btn(TEXT("Упасть (проверка)"), R(TEXT("heavy topple")));
		Section(TEXT("Оружие Человека"));
		static const TCHAR* Weapons[][2] = { { TEXT("none"), TEXT("Газета") }, { TEXT("slipper"), TEXT("Тапок") }, { TEXT("ladle"), TEXT("Половник") }, { TEXT("spatula"), TEXT("Лопатка") },
			{ TEXT("book"), TEXT("Книга") }, { TEXT("pin"), TEXT("Скалка") }, { TEXT("pan"), TEXT("Сковорода") }, { TEXT("broom"), TEXT("Веник") }, { TEXT("bat"), TEXT("Бита") }, { TEXT("sausage"), TEXT("Колбаса") } };
		for (const auto& Wp : Weapons) { Btn(Wp[1], R(FString(TEXT("weapon give ")) + Wp[0])); }
		Btn(TEXT("Выбить оружие (подменить колбасой)"), R(TEXT("weapon swap sausage")), On);
		Btn(TEXT("Грохот у прицела"), RA(TEXT("noiseaim")));
		for (const TCHAR* Who : { TEXT("human"), TEXT("cat"), TEXT("dog"), TEXT("gran"), TEXT("kid") })
		{
			const FString W(Who);
			Section(NpcName(NpcFromWord(W)));
			Btn(TEXT("Сюда (к прицелу)"), [this, W]() { Run(TEXT("npcgo ") + W + TEXT(" ") + Aim()); }, Blue);
			Btn(TEXT("Атаковать меня"), R(TEXT("npcatk ") + W), Danger);
			Btn(TEXT("Уснуть"), R(TEXT("npcsleep ") + W));
			Btn(TEXT("Проснуться"), R(TEXT("npcwake ") + W));
			Btn(TEXT("Пропустить шаг распорядка"), R(TEXT("routine skip ") + (W == TEXT("human") ? FString(TEXT("dad")) : W)));
		}
		Section(TEXT("У прицела"));
		Btn(TEXT("Убить / нокаут"), RA(TEXT("killaim")), Danger);
		Btn(TEXT("Оглушить"), RA(TEXT("stunaim")), Danger);
		Section(TEXT("Внутрь (муравей в людях: [E] у входа, ЛКМ/ПКМ кусать изнутри, [E] вылезти)"));
		Btn(TEXT("Меня — в ухо Человеку"), R(TEXT("inside put human ear")), Blue);
		Btn(TEXT("Меня — в нос Человеку"), R(TEXT("inside put human nose")), Blue);
		Btn(TEXT("Меня — в штанину"), R(TEXT("inside put human leg")), Blue);
		Btn(TEXT("Меня — в карман Человеку"), R(TEXT("inside put human pocket")), Blue);
		Btn(TEXT("Меня — в тапок Человеку"), R(TEXT("inside put human slipper")), Blue);
		Btn(TEXT("Меня — бабушке в пучок"), R(TEXT("inside put gran bun")), Blue);
		Btn(TEXT("Меня — бабушке в ухо"), R(TEXT("inside put gran ear")), Blue);
		Btn(TEXT("Меня — малышу в карман"), R(TEXT("inside put kid pocket")), Blue);
		Btn(TEXT("Меня — малышу за шиворот"), R(TEXT("inside put kid collar")), Blue);
		Btn(TEXT("Меня — Бадрику под ошейник"), R(TEXT("inside put dog collar")), Blue);
		Btn(TEXT("3 бота в штанины Человеку"), R(TEXT("inside bots human leg 3")), Danger);
		Btn(TEXT("2 бота в ухо Человеку"), R(TEXT("inside bots human ear 2")), Danger);
		Btn(TEXT("Укусить изнутри"), R(TEXT("inside bite")), Danger);
		Btn(TEXT("Все внутри кусают"), R(TEXT("inside bite all")), Danger);
		Btn(TEXT("Чихнуть (Человек)"), R(TEXT("inside sneeze")), Danger);
		Btn(TEXT("Хлопнуть себя по уху / [F] Человека"), R(TEXT("inside swat human")));
		Btn(TEXT("Вылезти"), R(TEXT("inside out")), On);
		Btn(TEXT("Внутрь: кто где (в лог)"), R(TEXT("inside info")));
		Section(TEXT("Распорядок дня (уровень v3)"));
		Btn(TEXT("День"), R(TEXT("routine day")));
		Btn(TEXT("Вечер"), R(TEXT("routine evening")));
		Btn(TEXT("Ночь"), R(TEXT("routine night")));
		Btn(TEXT("Вперёд на 1 мин"), R(TEXT("routine warp 60")));
		Btn(TEXT("Вперёд на 5 мин"), R(TEXT("routine warp 300")));
		Btn(TEXT("Распорядок стоп"), R(TEXT("routine off")));
		Btn(TEXT("Распорядок дальше"), R(TEXT("routine on")));
		Btn(TEXT("Кто что делает (в лог)"), R(TEXT("routine info")));
	}
	else if (Tab == 4)   // ---------------------------------------------------------------- world
	{
		Section(TEXT("Время"));
		BtnL([this]() { return (PC.IsValid() && UGameplayStatics::IsGamePaused(PC->GetWorld())) ? FString(TEXT("Продолжить")) : FString(TEXT("Пауза")); }, R(TEXT("pause")), Blue);
		for (const TCHAR* S : { TEXT("0.25"), TEXT("0.5"), TEXT("1"), TEXT("2"), TEXT("4") })
		{
			const FString X(S);
			BtnL([this, X]() { const bool bCur = PC.IsValid() && FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(PC->GetWorld()), FCString::Atof(*X)); return FString::Printf(TEXT("%sВремя x%s"), bCur ? TEXT("● ") : TEXT(""), *X); }, R(TEXT("timescale ") + X));
		}
		Section(TEXT("Время суток (свет и распорядок)"));
		Btn(TEXT("День"), R(TEXT("routine day")));
		Btn(TEXT("Вечер"), R(TEXT("routine evening")));
		Btn(TEXT("Ночь"), R(TEXT("routine night")));
		Btn(TEXT("Как в сиде"), R(TEXT("tod -1")));
		Section(TEXT("ТВ (гостиная; муравьи жмут кнопки пульта на журнальном столике)"));
		Btn(TEXT("ТВ вкл"), R(TEXT("tv on")), On);
		Btn(TEXT("ТВ выкл"), R(TEXT("tv off")));
		Btn(TEXT("ТВ: по распорядку"), R(TEXT("tv auto")));
		Btn(TEXT("Следующий канал"), R(TEXT("tv next")), Blue);
		{
			static const TCHAR* Ch[][2] = { { TEXT("0"), TEXT("Мультик") }, { TEXT("1"), TEXT("Природа") }, { TEXT("2"), TEXT("Новости") }, { TEXT("3"), TEXT("Футбол") }, { TEXT("4"), TEXT("Реклама") },
				{ TEXT("5"), TEXT("Кулинария") }, { TEXT("6"), TEXT("Детский") }, { TEXT("7"), TEXT("Птицы") }, { TEXT("8"), TEXT("Нет сигнала") } };
			for (const auto& C : Ch) { Btn(C[1], R(FString(TEXT("tv ch ")) + C[0])); }
		}
		for (const TCHAR* V : { TEXT("2"), TEXT("5"), TEXT("10") }) { Btn(FString(TEXT("Громкость ")) + V, R(FString(TEXT("tv vol ")) + V)); }
		Btn(TEXT("ТВ: инфо (в лог)"), R(TEXT("tv info")));
		Section(TEXT("Жизнь квартиры"));
		Btn(TEXT("Робот-пылесос"), R(TEXT("life vacuum")), Danger);
		Btn(TEXT("Пылесос стоп"), R(TEXT("life vacuum stop")));
		Btn(TEXT("Посудомойка"), R(TEXT("life wash")));
		Btn(TEXT("Дождь"), R(TEXT("life weather 1")), Blue);
		Btn(TEXT("Снег"), R(TEXT("life weather 2")), Blue);
		Btn(TEXT("Ясно"), R(TEXT("life weather 0")));
		Btn(TEXT("Погода по сиду"), R(TEXT("life weather auto")));
		Btn(TEXT("Голубь: когда (в лог)"), R(TEXT("life pigeon")));
		Btn(TEXT("Жизнь: инфо (в лог)"), R(TEXT("life info")));
		Section(TEXT("Уровень"));
		Btn(TEXT("Новый сид + перезапуск"), R(TEXT("restart")), Danger);
		Btn(TEXT("Перезапуск уровня"), R(TEXT("restart")), Danger);
		Btn(TEXT("Сид: что сейчас (в лог)"), R(TEXT("seed info")));
		Row->AddSlot().Padding(3.f)
		[
			SNew(SBox).WidthOverride(110.f)
			[
				SAssignNew(SeedBox, SEditableTextBox).HintText(FText::FromString(TEXT("сид №"))).Font(AntUi::Font(AntUi::EFont::Bold, 14))
			]
		];
		Btn(TEXT("Применить сид"), [this]() { if (SeedBox.IsValid() && !SeedBox->GetText().IsEmpty()) { Run(TEXT("seed ") + SeedBox->GetText().ToString().TrimStartAndEnd()); } });
		Section(TEXT("У прицела"));
		Btn(TEXT("Сломать предмет (крепёж v3)"), RA(TEXT("breakaim")), Danger);
		Btn(TEXT("Открутить все крепежи"), RA(TEXT("unfastenaim")), Danger);
		Btn(TEXT("Сломать мебель (удар, стадия+)"), [this]() { Run(TEXT("wreck at ") + Aim() + TEXT(" 220 40 400")); }, Danger);
		Btn(TEXT("Разбудить физику"), [this]() { Run(TEXT("wreck at ") + Aim() + TEXT(" 320 6 0")); });
		Btn(TEXT("Устроить погром (Человек)"), R(TEXT("rampage")), Danger);
		Btn(TEXT("Разруха: инфо (в лог)"), R(TEXT("wreck info")));
		Section(TEXT("Пятно у прицела"));
		static const TCHAR* Kinds[][2] = { { TEXT("jam"), TEXT("Варенье") }, { TEXT("honey"), TEXT("Мёд") }, { TEXT("cola"), TEXT("Кола") }, { TEXT("oil"), TEXT("Масло") },
			{ TEXT("cream"), TEXT("Крем") }, { TEXT("chocolate"), TEXT("Шоколад") }, { TEXT("juice"), TEXT("Сок") }, { TEXT("crumbs"), TEXT("Крошки") },
			{ TEXT("mud"), TEXT("Грязь") }, { TEXT("water"), TEXT("Вода") }, { TEXT("sugar"), TEXT("Сахар") } };
		for (const auto& K : Kinds)
		{
			const FString Id(K[0]);
			Btn(K[1], [this, Id]() { Run(TEXT("stainaim ") + Id + TEXT(" ") + Aim() + TEXT(" 160")); });
		}
		Btn(TEXT("Убрать всю грязь"), R(TEXT("cleanall")), On);
		Section(TEXT("Цепные реакции"));
		static const TCHAR* Chains[][2] = { { TEXT("C1"), TEXT("Полка → банки") }, { TEXT("C2"), TEXT("Скатерть → чайник") }, { TEXT("C3"), TEXT("Карниз → шторы") },
			{ TEXT("C4"), TEXT("Кола → лужа") }, { TEXT("C5"), TEXT("Шарик") }, { TEXT("C6"), TEXT("Спица → шарф") } };
		for (const auto& C : Chains) { Btn(C[1], R(FString(TEXT("chain ")) + C[0]), Danger); }
		Btn(TEXT("Всё сразу (хаос)"), R(TEXT("state chaos")), Danger);
		Section(TEXT("Реакции: материалы и цепочки 2.0 (docs/MATERIALS.md)"));
		static const TCHAR* Chains2[][2] = { { TEXT("C7"), TEXT("C7 Перечница → чих") }, { TEXT("C8"), TEXT("C8 Мука → белая мгла") }, { TEXT("C9"), TEXT("C9 Лёд → лужа → КЗ") },
			{ TEXT("C10"), TEXT("C10 Рогатка → на стол") }, { TEXT("C11"), TEXT("C11 Горох → папа падает") }, { TEXT("C12"), TEXT("C12 Шоколад на батарее") },
			{ TEXT("C13"), TEXT("C13 Тостер → тост → перец") }, { TEXT("C14"), TEXT("C14 Книги-домино → ваза") }, { TEXT("C15"), TEXT("C15 Микроволновка пищит") },
			{ TEXT("C16"), TEXT("C16 Посудомойка → вода → КЗ") }, { TEXT("C17"), TEXT("C17 Робот-пылесос") }, { TEXT("C18"), TEXT("C18 Ментос → страйк") },
			{ TEXT("C19"), TEXT("C19 Губка → тарелка") }, { TEXT("C20"), TEXT("C20 Шарик → потолок") }, { TEXT("C21"), TEXT("C21 Сахар → муха") }, { TEXT("C22"), TEXT("C22 Провод → КЗ") } };
		for (const auto& C : Chains2) { Btn(C[1], R(FString(TEXT("chain ")) + C[0]), Danger); }
		Btn(TEXT("Облако перца у прицела"), [this]() { Run(TEXT("rx cloud pepper ") + Aim() + TEXT(" 900")); }, Danger);
		Btn(TEXT("Облако муки у прицела"), [this]() { Run(TEXT("rx cloud flour ") + Aim() + TEXT(" 1100")); }, Danger);
		Btn(TEXT("Лёд (8 кубиков) у прицела"), [this]() { Run(TEXT("rx ice ") + Aim() + TEXT(" 8")); });
		Btn(TEXT("Горох (20) у прицела"), [this]() { Run(TEXT("rx peas ") + Aim() + TEXT(" 20")); });
		Btn(TEXT("Шоколадка у прицела"), [this]() { Run(TEXT("rx choc ") + Aim()); });
		Btn(TEXT("Сахар у прицела"), [this]() { Run(TEXT("rx sugar ") + Aim()); });
		Btn(TEXT("Короткое замыкание (удлинитель)"), R(TEXT("power short")), Danger);
		Btn(TEXT("Свет: выбить пробки"), R(TEXT("power off")), Danger);
		Btn(TEXT("Свет: включить"), R(TEXT("power on")), On);
		Btn(TEXT("Жар: все источники горячие"), R(TEXT("heat on")), Danger);
		Btn(TEXT("Жар: всё холодное"), R(TEXT("heat off")), Blue);
		Btn(TEXT("Жар: как в игре"), R(TEXT("heat auto")));
		Btn(TEXT("Папа ослеп (мука, 5 с)"), R(TEXT("rx blind")));
		Btn(TEXT("Папа чихает"), R(TEXT("rx sneeze dad")));
		Btn(TEXT("Материал у прицела (mat info)"), R(TEXT("mat info")));
		Btn(TEXT("Реакции: сбросить предметы"), R(TEXT("rx reset")), On);
		Btn(TEXT("Реакции: инфо и цена (в лог)"), [this]() { Run(TEXT("rx info")); Run(TEXT("rx cost")); });
		Section(TEXT("Вода (настоящая жидкость, docs/FEATURES.md §29)"));
		Btn(TEXT("Кран вкл"), R(TEXT("fluid tap on 5")), Blue);
		Btn(TEXT("Кран сильно"), R(TEXT("fluid tap on 14")), Blue);
		Btn(TEXT("Кран выкл"), R(TEXT("fluid tap off")));
		Btn(TEXT("ПОТОП (кран 30 л/с, 20 с)"), R(TEXT("fluid flood 20 30")), Danger);
		Btn(TEXT("Вылить ведро у прицела"), [this]() { Run(TEXT("fluid bucket ") + Aim() + TEXT(" 8")); }, Danger);
		Btn(TEXT("Кола + ментос у прицела"), [this]() { Run(TEXT("fluid geyser ") + Aim() + TEXT(" 8")); }, Danger);
		Btn(TEXT("Масло у прицела"), [this]() { Run(TEXT("fluid spill oil ") + Aim() + TEXT(" 0.5 200")); });
		Btn(TEXT("Мёд у прицела"), [this]() { Run(TEXT("fluid spill honey ") + Aim() + TEXT(" 2 120")); });
		Btn(TEXT("Осушить всё"), R(TEXT("fluid dry")), On);
		Btn(TEXT("Слив раковины открыт"), R(TEXT("fluid drain 6")));
		Btn(TEXT("Слив забит (переполнится)"), R(TEXT("fluid drain 0")));
		Btn(TEXT("Отладка воды (на экран)"), R(TEXT("fluid debug")));
		Btn(TEXT("Вода: инфо (в лог)"), R(TEXT("fluid info")));
		Section(TEXT("Ткань (скатерть, шторы, плед, шалаш: держать [E] у края, ПКМ — прогрызть)"));
		Btn(TEXT("Дёрнуть скатерть"), R(TEXT("cloth yank tablecloth 0 -1 60 3")), Danger);
		Btn(TEXT("6 муравьёв тянут скатерть"), R(TEXT("cloth crew tablecloth 6 0 -1 10")), Danger);
		Btn(TEXT("Порвать у прицела"), [this]() { Run(TEXT("cloth tear ") + Aim() + TEXT(" 6")); }, Danger);
		Btn(TEXT("Ветер вкл"), R(TEXT("cloth wind 1")), Blue);
		Btn(TEXT("Ветер выкл"), R(TEXT("cloth wind 0")));
		Btn(TEXT("Ветер: из окна (авто)"), R(TEXT("cloth wind -1")));
		Btn(TEXT("Намочить скатерть"), R(TEXT("cloth wet tablecloth 1")));
		Btn(TEXT("Сорвать штору"), R(TEXT("cloth drop curtain")), Danger);
		Btn(TEXT("Стянуть плед"), R(TEXT("cloth yank plaid 0 1 40 2")), Danger);
		Btn(TEXT("Сбросить ткань"), R(TEXT("cloth reset")), On);
		Btn(TEXT("Ткань: инфо и цена (в лог)"), R(TEXT("cloth info")));
	}
	else if (Tab == 5)   // ---------------------------------------------------------------- raid
	{
		Section(TEXT("Снежный ком"));
		BtnL([this]() { const AAntSnowball* S = PC.IsValid() ? AAntSnowball::Get(PC->GetWorld()) : nullptr; return S && S->bActive ? FString::Printf(TEXT("Масса %d / %d (выборов %d)"), S->Mass, S->NextAt, S->Level) : FString(TEXT("Снежный ком не идёт")); }, []() {}, FLinearColor(0.6f, 0.6f, 0.6f));
		Btn(TEXT("Масса +50"), R(TEXT("mass 50")), On);
		Btn(TEXT("Масса +500"), R(TEXT("mass 500")), On);
		Btn(TEXT("Выбор карточки сейчас"), R(TEXT("picknow")), On);
		Btn(TEXT("Все улучшения"), R(TEXT("allups")), On);
		Btn(TEXT("Автовыбор: 1-я карта"), R(TEXT("snow auto 1")));
		Btn(TEXT("Автовыбор выкл"), R(TEXT("snow auto off")));
		Section(TEXT("Таймер"));
		Btn(TEXT("Время +5 мин"), R(TEXT("addtime 300")));
		Btn(TEXT("Последний свет (30 с)"), R(TEXT("settime 30")), Danger);
		Btn(TEXT("Таймер 10 с"), R(TEXT("settime 10")), Danger);
		Section(TEXT("Исход"));
		Btn(TEXT("Мгновенная победа"), R(TEXT("win")), On);
		Btn(TEXT("Эвакуация (уйти)"), R(TEXT("evac")), Blue);
		Btn(TEXT("Поражение"), R(TEXT("lose")), Danger);
		Btn(TEXT("Начать рейд"), R(TEXT("raid start")));
		Section(TEXT("Режим «Против» (один играет Человеком)"));
		Btn(TEXT("Против: Человек — клиент"), R(TEXT("versus start client")), Blue);
		Btn(TEXT("Против: Человек — я (хост)"), R(TEXT("versus start host")), Blue);
		Btn(TEXT("Против: Человек — ИИ"), R(TEXT("versus start ai")), Blue);
		Section(TEXT("Разведка"));
		BtnL([this]() { const AAntAdminState* St = PC.IsValid() ? AAntAdminState::Get(PC->GetWorld()) : nullptr; return (St && St->bReveal) ? FString(TEXT("Показать всех: ВКЛ")) : FString(TEXT("Показать всех: выкл")); }, R(TEXT("reveal")), On);
		Btn(TEXT("Рейд: инфо (в лог)"), R(TEXT("raid info")));
		Section(TEXT("Гнездо"));
		Btn(TEXT("Еда +500"), R(TEXT("nest food 500")));
		Btn(TEXT("Желе +100"), R(TEXT("nest jelly 100")));
		Btn(TEXT("Личинки 10"), R(TEXT("nest larvae 10")));
		Section(TEXT("Боец (мой профиль; «all» в команде — всем)"));
		Btn(TEXT("Опыт +500"), R(TEXT("fighter xp 500")), On);
		Btn(TEXT("Звание: Сержант"), R(TEXT("fighter rank 3")));
		Btn(TEXT("Звание: Герой Щели"), R(TEXT("fighter rank 7")));
		Btn(TEXT("Все медали и облик"), R(TEXT("fighter unlockall")), On);
		Btn(TEXT("Медаль «Был чихнут»"), R(TEXT("fighter feat sneezed")));
		Btn(TEXT("Сбросить профиль"), R(TEXT("fighter reset")), Danger);
		Btn(TEXT("Профиль: инфо (в лог)"), R(TEXT("fighter info")));
		Section(TEXT("Ветераны отряда (казарма хоста)"));
		Btn(TEXT("+3 ветерана"), R(TEXT("vet add 3")), On);
		Btn(TEXT("+1 опытный (старшина)"), R(TEXT("vet add 1 900")), On);
		Btn(TEXT("Взять всех в мой отряд"), R(TEXT("vet autodraft")));
		Btn(TEXT("Убить первого ветерана"), R(TEXT("vet kill 0 приказ сверху")), Danger);
		Btn(TEXT("Вернуть последнего павшего"), R(TEXT("vet revive")), Blue);
		Btn(TEXT("Ветераны к прицелу"), R(TEXT("vet near")));
		Btn(TEXT("Список (в лог)"), R(TEXT("vet list")));
		Btn(TEXT("Очистить казарму и стену"), R(TEXT("vet clear")), Danger);
	}
	else if (Tab == 6)   // ---------------------------------------------------------------- the colony (docs/COLONY.md)
	{
		Section(TEXT("Муравейник (экран гнезда; на этой машине)"));
		Btn(TEXT("Открыть муравейник"), R(TEXT("colony view")), On);
		Btn(TEXT("Штаб (старый экран)"), R(TEXT("colony sheet")));
		Btn(TEXT("Повторить шоу после рейда"), R(TEXT("colony show")), On);
		Btn(TEXT("Королева засыпает"), R(TEXT("colony sleep")));
		Btn(TEXT("Колония: инфо (в лог)"), R(TEXT("colony info")));
		Section(TEXT("Ресурсы"));
		Btn(TEXT("Еда +500"), R(TEXT("colony give food 500")));
		Btn(TEXT("Сахар +200"), R(TEXT("colony give sugar 200")));
		Btn(TEXT("Белок +200"), R(TEXT("colony give protein 200")));
		Btn(TEXT("Жир +200"), R(TEXT("colony give fat 200")));
		Btn(TEXT("Крошки +200"), R(TEXT("colony give crumbs 200")));
		Btn(TEXT("Молочко +10"), R(TEXT("colony give jelly 10")));
		Btn(TEXT("Личинки +3"), R(TEXT("colony give larvae 3")));
		Btn(TEXT("Рабочие +20"), R(TEXT("colony give workers 20")));
		Btn(TEXT("Трофей (случайный)"), R(TEXT("colony give trophy random")), On);
		Btn(TEXT("Все трофеи"), R(TEXT("colony give trophy all")));
		Btn(TEXT("Убрать трофеи"), R(TEXT("colony give trophy none")), Danger);
		Section(TEXT("Залы и ходы (бесплатно)"));
		Btn(TEXT("Прорыть все ходы"), R(TEXT("colony dig all")));
		Btn(TEXT("Все залы I"), R(TEXT("colony setall 1")));
		Btn(TEXT("Все залы II"), R(TEXT("colony setall 2")));
		Btn(TEXT("Все залы III"), R(TEXT("colony setall 3")), On);
		Btn(TEXT("Снести всё"), R(TEXT("colony setall 0")), Danger);
		Section(TEXT("Настроение Королевы"));
		Btn(TEXT("В ярости (10)"), R(TEXT("colony mood 10")), Danger);
		Btn(TEXT("Ворчит (40)"), R(TEXT("colony mood 40")));
		Btn(TEXT("Довольна (70)"), R(TEXT("colony mood 70")), On);
		Btn(TEXT("В восторге (95)"), R(TEXT("colony mood 95")), On);
		Section(TEXT("Рейды на бумаге (доход, Королева, трофеи, стройка; затем шоу)"));
		Btn(TEXT("+1 рейд"), R(TEXT("colony sim 1")), Blue);
		Btn(TEXT("+5 рейдов"), R(TEXT("colony sim 5")), Blue);
		Btn(TEXT("+15 рейдов"), R(TEXT("colony sim 15")), Blue);
		Btn(TEXT("+1 рейд без стройки"), R(TEXT("colony sim 1 nobuild")));
		Section(TEXT("Сброс"));
		Btn(TEXT("Сбросить колонию (старая — colony.old)"), R(TEXT("colony reset")), Danger);
		Btn(TEXT("Записать как старое сохранение v1 (тест миграции)"), R(TEXT("colony v1save")));
		Btn(TEXT("Перечитать сохранение"), R(TEXT("colony reload")));
	}
	else   // ---------------------------------------------------------------- screen
	{
		Section(TEXT("Экран (только на этой машине)"));
		Btn(TEXT("FPS"), R(TEXT("con stat fps")));
		Btn(TEXT("Статистика кадра"), R(TEXT("con stat unit")));
		Btn(TEXT("Скрыть / показать HUD"), R(TEXT("hudtoggle")));
		Btn(TEXT("Режим съёмки (F8)"), R(TEXT("film")));
		Btn(TEXT("Свободная камера"), R(TEXT("freecam")));
		Btn(TEXT("Скриншот"), R(TEXT("con HighResShot 1")));
		Section(TEXT("Звук (на этой машине, сохраняется; M — музыка вкл/выкл)"));
		BtnL([]() { return AntAudio::MusicMuted() ? FString(TEXT("Музыка: выкл")) : FString(TEXT("Музыка: вкл")); }, []() { AntAudio::SetMusicMuted(!AntAudio::MusicMuted()); }, On);
		auto Vol = [this](const TCHAR* Name, uint8 Kls)
		{
			const FString N(Name);
			BtnL([N, Kls]() { return FString::Printf(TEXT("%s: %d%%"), *N, FMath::RoundToInt(AntAudio::GetVolume(Kls) * 100.f)); }, []() {}, FLinearColor(0.6f, 0.6f, 0.6f));
			Btn(TEXT("−"), [Kls]() { AntAudio::SetVolume(Kls, AntAudio::GetVolume(Kls) - 0.1f); });
			Btn(TEXT("+"), [Kls]() { AntAudio::SetVolume(Kls, AntAudio::GetVolume(Kls) + 0.1f); });
		};
		Vol(TEXT("Музыка"), AntMixClass::Music);
		Vol(TEXT("Звуки"), AntMixClass::Sfx);
		Vol(TEXT("Голоса"), AntMixClass::Voice);
		Vol(TEXT("Комната"), AntMixClass::Ambience);
		Vol(TEXT("Всё"), AntMixClass::Num);
		Section(TEXT("Клавиши: V — вид от первого лица, F8 — съёмка, в съёмке F5 — свободная камера, F2 — HUD"));
	}
}

TSharedRef<SWidget> AntAdmin::MakePanel(AAntPlayerController* PC)
{
	GUsed = true;
	return SNew(SAntAdminPanel).PC(PC);
}

void AntAdmin::FocusPanel(const TSharedPtr<SWidget>& Panel)
{
	if (Panel.IsValid())
	{
		StaticCastSharedPtr<SAntAdminPanel>(Panel)->FocusLine();
	}
}

// =============================================================================================
// HUD: the badge and the "show everyone" markers
// =============================================================================================

void AntAdmin::DrawHud(AHUD* Hud, UCanvas* Canvas, AAntPlayerController* PC)
{
	if (!Hud || !Canvas || !PC || !CanUse(PC) || !(Launched() || GUsed))
	{
		return;
	}
	UFont* F = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Sc = FMath::Max(0.8f, Canvas->ClipY / 1080.f);
	// The badge, top left.
	{
		const AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn());
		FString Line = TEXT("АДМИН");
		if (Me && Me->AdminFlags)
		{
			Line += TEXT(" ·");
			if (Me->AdminFlags & 1) { Line += TEXT(" бессмертие"); }
			if (Me->AdminFlags & 2) { Line += TEXT(" кислота"); }
			if (Me->AdminFlags & 4) { Line += TEXT(" полёт"); }
			if (Me->AdminFlags & 8) { Line += TEXT(" сила"); }
		}
		if (Me && !FMath::IsNearlyEqual(Me->AdminSpeed, 1.f)) { Line += FString::Printf(TEXT(" · x%g"), Me->AdminSpeed); }
		Line += TEXT("   [F10]");
		float W = 0.f, H = 0.f;
		Canvas->StrLen(F, Line, W, H);
		W *= Sc; H *= Sc;
		Canvas->K2_DrawBox(FVector2D(8.f, 8.f), FVector2D(W + 16.f, H + 8.f), 1.f, FLinearColor(1.f, 0.3f, 0.2f, 0.9f));
		FCanvasTileItem Bg(FVector2D(8.f, 8.f), FVector2D(W + 16.f, H + 8.f), FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Bg.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bg);
		FCanvasTextItem T(FVector2D(16.f, 12.f), FText::FromString(Line), F, FLinearColor(1.f, 0.45f, 0.3f));
		T.Scale = FVector2D(Sc, Sc);
		T.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(T);
	}
	const AAntAdminState* St = AAntAdminState::Get(PC->GetWorld());
	if (!St || !St->bReveal)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	auto Mark = [&](const AActor* A, const FString& Label, const FLinearColor& C)
	{
		if (!A || A->IsHidden()) { return; }
		const FVector P = A->GetActorLocation();
		FVector S = Canvas->Project(P);
		const bool bBehind = ((P - CamLoc) | CamRot.Vector()) < 0.f;
		if (bBehind) { S.X = Canvas->ClipX - S.X; S.Y = Canvas->ClipY - 20.f; }
		S.X = FMath::Clamp(S.X, 20.f, Canvas->ClipX - 140.f);
		S.Y = FMath::Clamp(S.Y, 40.f, Canvas->ClipY - 30.f);
		const FString Txt = FString::Printf(TEXT("» %s %.0fм"), *Label, FVector::Dist(P, CamLoc) / 1000.f);
		FCanvasTextItem T(FVector2D(S.X, S.Y), FText::FromString(Txt), F, C);
		T.Scale = FVector2D(Sc * 0.85f, Sc * 0.85f);
		T.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(T);
	};
	UWorld* W = PC->GetWorld();
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		Mark(*It, Cast<AGrandmaCharacter>(*It) ? TEXT("Бабушка") : Cast<AKidCharacter>(*It) ? TEXT("Ребёнок") : TEXT("Человек"), AdmRed);
	}
	for (TActorIterator<ACritter> It(W); It; ++It) { Mark(*It, It->DisplayName(), AdmRed); }
	int32 Reds = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		if (!It->bDead && It->Team == 1 && Reds++ < 24) { Mark(*It, TEXT("рыжий"), FLinearColor(1.f, 0.6f, 0.2f)); }
	}
	int32 Big = 0;
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		if (It->bMain) { Mark(*It, TEXT("ТОРТ"), AdmGold); }
		else if (!It->bIsGear && It->Weight >= 20 && !It->IsCarried() && Big++ < 20) { Mark(*It, It->LootName, AdmGreen); }
	}
}
