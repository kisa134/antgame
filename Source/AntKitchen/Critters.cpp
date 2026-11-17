#include "Critters.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntKitchenGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "AntMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	FLinearColor Black_ish() { return FLinearColor(0.08f, 0.07f, 0.07f); }
	FLinearColor Steel_ish() { return FLinearColor(FColor(175, 180, 185)); }

	void HurtAntsNear(UWorld* W, AActor* By, const FVector& C, float R, float Dmg, const TCHAR* Cause, int32 ArmyKills)
	{
		if (!W)
		{
			return;
		}
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->IsInNest() || FVector::Dist(A->GetActorLocation(), C) > R + AAntCharacter::BodyRadius)
			{
				continue;
			}
			A->LastHazard = Cause;
			A->LastHazardTime = W->GetTimeSeconds();
			UGameplayStatics::ApplyDamage(A, Dmg, nullptr, By, UDamageType::StaticClass());
		}
		if (ArmyKills > 0)
		{
			AAntArmy::KillInSphere(W, C, R, ArmyKills);
		}
	}
}

// =============================================================================================
// ACritter: the dog and the cat
// =============================================================================================

ACritter::ACritter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACritter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACritter, Kind);
	DOREPLIFETIME(ACritter, State);
}

void ACritter::BeginPlay()
{
	Super::BeginPlay();
	LastPos = GetActorLocation();
	if (HasAuthority())
	{
		if (Kind == 0)
		{
			Spots = {
				{ FVector(600.f, -600.f, 0.f), 0, 50.f },          // asleep on the kitchen rug
				{ FVector(-3000.f, -3800.f, 0.f), 1, 14.f },       // the bowl
				{ FVector(5000.f, -1500.f, 0.f), 1, 8.f },         // sniffing the trash
				{ FVector(6300.f, -4000.f, 0.f), 1, 6.f },         // the shoe by the door
				{ FVector(14000.f, -5500.f, 0.f), 0, 45.f },       // asleep on the living-room rug
				{ FVector(12000.f, -1200.f, 0.f), 2, 12.f },       // sitting by the sofa, begging
				{ FVector(-5500.f, 1800.f, 0.f), 1, 7.f },         // licking the flour
			};
		}
		else
		{
			Spots = {
				{ FVector(13500.f, 400.f, 950.f), 0, 60.f },       // asleep on the sofa
				{ FVector(9250.f, -5800.f, 800.f), 0, 45.f },      // on grandma's lap... the armchair
				{ FVector(0.f, -4900.f, 2200.f), 2, 25.f },        // the window sill, watching the yard
				{ FVector(11000.f, -3000.f, 0.f), 2, 10.f },       // the living-room floor
				{ FVector(2000.f, -3000.f, 0.f), 2, 12.f },        // the kitchen rug
				{ FVector(19700.f, -3000.f, 960.f), 2, 15.f },     // a bookshelf shelf
			};
		}
		SpotIndex = 0;
		SetActorLocation(Spots[0].P);
		State = 0;
		StateTimer = Spots[0].Time * 0.5f;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildVisual();
	}
}

void ACritter::BuildVisual()
{
	Vis = NewObject<USceneComponent>(this, TEXT("Vis"));
	Vis->SetupAttachment(RootComponent);
	Vis->RegisterComponent();
	auto P = [this](EAntShape S, const FVector& L, const FVector& Size, const FLinearColor& C, const FRotator& R = FRotator::ZeroRotator, bool bCol = true)
	{
		UStaticMeshComponent* M = AntKit::AddPart(this, Vis, S, L, Size / 100.f, C, R, bCol, false);
		if (bCol)
		{
			UAntMovementComponent::MakeClimbable(M);   // ants crawl up the legs and ride on the back
		}
		return M;
	};
	if (Kind == 0)
	{
		// Бадрик: a French bulldog. Fawn with a white chest, big bat ears, flat face, stub tail.
		const FLinearColor Fawn = Hex(205, 170, 125), Dark = Hex(60, 45, 40), Pink = Hex(230, 150, 150);
		P(EAntShape::Cube, FVector(-40.f, 0.f, 360.f), FVector(760.f, 420.f, 320.f), Fawn);
		P(EAntShape::Sphere, FVector(300.f, 0.f, 330.f), FVector(420.f, 440.f, 360.f), Hex(240, 235, 225));   // white chest
		P(EAntShape::Sphere, FVector(-380.f, 0.f, 380.f), FVector(360.f, 400.f, 320.f), Fawn);                // rump
		Head = P(EAntShape::Cube, FVector(520.f, 0.f, 470.f), FVector(320.f, 340.f, 300.f), Fawn);
		P(EAntShape::Cube, FVector(680.f, 0.f, 400.f), FVector(120.f, 220.f, 160.f), Dark, FRotator::ZeroRotator, false);   // muzzle
		P(EAntShape::Sphere, FVector(740.f, 0.f, 440.f), FVector(70.f, 90.f, 60.f), Black_ish(), FRotator::ZeroRotator, false);
		for (float s : { -1.f, 1.f })
		{
			Ears.Add(P(EAntShape::Cone, FVector(480.f, 120.f * s, 660.f), FVector(150.f, 110.f, 260.f), Fawn, FRotator(0.f, 0.f, 15.f * s), false));
			Eyes.Add(P(EAntShape::Sphere, FVector(660.f, 95.f * s, 520.f), FVector(60.f, 60.f, 60.f), Hex(30, 25, 25), FRotator::ZeroRotator, false));
			P(EAntShape::Cube, FVector(680.f, 70.f * s, 330.f), FVector(90.f, 50.f, 70.f), Pink, FRotator::ZeroRotator, false);   // jowls
		}
		Tail = P(EAntShape::Cone, FVector(-560.f, 0.f, 470.f), FVector(90.f, 90.f, 140.f), Fawn, FRotator(-60.f, 0.f, 0.f), false);
		for (const FVector L : { FVector(280.f, 150.f, 0.f), FVector(280.f, -150.f, 0.f), FVector(-320.f, 150.f, 0.f), FVector(-320.f, -150.f, 0.f) })
		{
			Legs.Add(P(EAntShape::Cylinder, L + FVector(0.f, 0.f, 120.f), FVector(110.f, 110.f, 240.f), Fawn));
			P(EAntShape::Sphere, L + FVector(20.f, 0.f, 20.f), FVector(130.f, 110.f, 50.f), Dark);   // paw
		}
		P(EAntShape::Cylinder, FVector(440.f, 0.f, 420.f), FVector(360.f, 400.f, 40.f), Hex(200, 40, 40), FRotator(90.f, 0.f, 0.f), false);   // red collar
		P(EAntShape::Cylinder, FVector(610.f, 0.f, 300.f), FVector(60.f, 60.f, 6.f), Hex(220, 190, 60), FRotator(0.f, 0.f, 90.f), false);    // the tag
	}
	else
	{
		// A grey tabby, slim, long tail.
		const FLinearColor Grey = Hex(120, 118, 128), Dark = Hex(70, 68, 78), Pink = Hex(230, 170, 170);
		P(EAntShape::Sphere, FVector(0.f, 0.f, 300.f), FVector(760.f, 260.f, 280.f), Grey);
		P(EAntShape::Sphere, FVector(-250.f, 0.f, 320.f), FVector(360.f, 280.f, 260.f), Dark);
		for (int32 i = 0; i < 4; ++i)   // stripes
		{
			P(EAntShape::Cube, FVector(-200.f + i * 110.f, 0.f, 420.f), FVector(40.f, 250.f, 30.f), Dark, FRotator::ZeroRotator, false);
		}
		Head = P(EAntShape::Sphere, FVector(400.f, 0.f, 400.f), FVector(230.f, 220.f, 200.f), Grey);
		P(EAntShape::Sphere, FVector(500.f, 0.f, 370.f), FVector(90.f, 110.f, 70.f), Hex(230, 225, 220), FRotator::ZeroRotator, false);   // muzzle
		P(EAntShape::Sphere, FVector(540.f, 0.f, 385.f), FVector(30.f, 36.f, 24.f), Pink, FRotator::ZeroRotator, false);                   // nose
		for (float s : { -1.f, 1.f })
		{
			Ears.Add(P(EAntShape::Cone, FVector(380.f, 75.f * s, 520.f), FVector(80.f, 60.f, 140.f), Grey, FRotator(0.f, 0.f, 20.f * s), false));
			Eyes.Add(P(EAntShape::Sphere, FVector(480.f, 60.f * s, 430.f), FVector(44.f, 44.f, 40.f), Hex(120, 190, 60), FRotator::ZeroRotator, false));
			P(EAntShape::Cube, FVector(500.f, 150.f * s, 360.f), FVector(220.f, 4.f, 4.f), Hex(240, 240, 240), FRotator(0.f, 20.f * s, 0.f), false);   // whiskers
		}
		Tail = P(EAntShape::Cylinder, FVector(-480.f, 0.f, 420.f), FVector(50.f, 50.f, 560.f), Grey, FRotator(-70.f, 0.f, 0.f), false);
		for (const FVector L : { FVector(260.f, 90.f, 0.f), FVector(260.f, -90.f, 0.f), FVector(-230.f, 90.f, 0.f), FVector(-230.f, -90.f, 0.f) })
		{
			Legs.Add(P(EAntShape::Cylinder, L + FVector(0.f, 0.f, 110.f), FVector(70.f, 70.f, 220.f), Grey));
			P(EAntShape::Sphere, L + FVector(15.f, 0.f, 15.f), FVector(80.f, 70.f, 36.f), Hex(240, 240, 240));   // white socks
		}
	}
}

FVector ACritter::PawWorld(int32 i) const
{
	const float X = Kind == 0 ? (i < 2 ? 280.f : -320.f) : (i < 2 ? 260.f : -230.f);
	const float Y = (Kind == 0 ? 150.f : 90.f) * ((i % 2) ? -1.f : 1.f);
	return GetActorTransform().TransformPosition(FVector(X, Y, 0.f));
}

void ACritter::Hunt(float Seconds)
{
	if (HasAuthority() && Kind == 1)
	{
		HuntUntil = GetWorld()->GetTimeSeconds() + Seconds;
		NextHuntPounce = 0.f;
		UE_LOG(LogTemp, Display, TEXT("ANTCAT the cat hunts for %.0f s"), Seconds);
	}
}

void ACritter::PickSpot()
{
	if (Spots.Num() < 2)
	{
		return;
	}
	int32 Next = SpotIndex;
	for (int32 Guard = 0; Guard < 8 && Next == SpotIndex; ++Guard)
	{
		Next = FMath::RandRange(0, Spots.Num() - 1);
	}
	SpotIndex = Next;
	Goal = Spots[Next].P;
	const FVector Me = GetActorLocation();
	if (Kind == 1 && FMath::Abs(Goal.Z - Me.Z) > 200.f)
	{
		// A cat gets up and down with a jump.
		PounceFrom = Me;
		PounceTo = Goal;
		PounceT = 0.f;
		State = 4;
		return;
	}
	State = 1;
}

void ACritter::ServerThink(float Dt)
{
	const FVector Me = GetActorLocation();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();

	if (State == 4)
	{
		return;   // mid-air
	}
	if (Kind == 1 && HuntUntil > 0.f)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= HuntUntil)
		{
			HuntUntil = 0.f;
			PickSpot();
			return;
		}
		// The hunt: down to the floor, after the nearest ant, a pounce whenever one is in reach.
		Awake += Dt;
		if (Me.Z > 150.f)
		{
			const FVector Centre = Me.X > 7100.f ? FVector(13500.f, -4500.f, 0.f) : FVector(0.f, -1000.f, 0.f);
			PounceFrom = Me;
			PounceTo = Me + (Centre - Me).GetSafeNormal2D() * 1100.f;
			PounceTo.Z = 0.f;
			PounceT = 0.f;
			State = 4;
			return;
		}
		AAntCharacter* Prey = nullptr;
		float Best = 14000.f;
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->IsInNest() || A->Creature != 0 || A->GetActorLocation().Z > 350.f || A->GetActorLocation().Y > 3000.f)
			{
				continue;
			}
			const float D = (float)FVector::Dist2D(A->GetActorLocation(), Me);
			if (D < Best) { Best = D; Prey = A; }
		}
		if (Prey)
		{
			Goal = Prey->GetActorLocation();
			Goal.Z = 0.f;
			State = 1;
			if (Best < 800.f && Now >= NextHuntPounce)
			{
				NextHuntPounce = Now + 2.2f;
				PounceFrom = Me;
				PounceTo = Prey->GetActorLocation() + Prey->GetVelocity() * 0.4f;
				PounceTo.Z = Me.Z;
				PounceT = 0.f;
				State = 4;
				if (GM) { GM->Broadcast(TEXT("КОШКА ПРЫГНУЛА!"), FLinearColor(1.f, 0.6f, 0.3f)); }
			}
		}
		else if (State != 1 || FVector::Dist2D(Me, Goal) < 200.f)
		{
			Goal = AKitchenBuilder::RandomFloorPoint();
			Goal.Z = 0.f;
			State = 1;
		}
		return;
	}
	// The cat hunts: an ant (not a nestmate at home) close by while it's awake -> pounce. A fly -> swat.
	if (Kind == 1 && State != 0)
	{
		Awake += Dt;
		AAntCharacter* Prey = nullptr;
		float Best = 900.f;
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->IsInNest() || A->Creature == 2)
			{
				continue;
			}
			const float D = (float)FVector::Dist(A->GetActorLocation(), Me);
			if (A->Creature == 1)
			{
				if (D < 500.f && FMath::FRand() < Dt * 0.6f)
				{
					UGameplayStatics::ApplyDamage(A, 45.f, nullptr, this, UDamageType::StaticClass());
					A->Toast(TEXT("Кошка махнула лапой!"));
				}
				continue;
			}
			if (D < Best && FMath::Abs(A->GetActorLocation().Z - Me.Z) < 400.f)
			{
				Best = D;
				Prey = A;
			}
		}
		if (Prey && FMath::FRand() < Dt * 0.8f)
		{
			PounceFrom = Me;
			PounceTo = Prey->GetActorLocation() + Prey->GetVelocity() * 0.4f;
			PounceTo.Z = Me.Z;   // lands on the same level
			PounceT = 0.f;
			State = 4;
			if (GM) { GM->Broadcast(TEXT("КОШКА ПРЫГНУЛА!"), FLinearColor(1.f, 0.6f, 0.3f)); }
			return;
		}
	}
	// The dog eats what lies by its bowl (and licks the flour): loot near the mouth disappears.
	if (Kind == 0 && State == 2 && SpotIndex >= 0 && Spots[SpotIndex].Do == 1)
	{
		EatTimer += Dt;
		if (EatTimer > 1.6f)
		{
			EatTimer = 0.f;
			const FVector Mouth = GetActorTransform().TransformPosition(FVector(700.f, 0.f, 150.f));
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				AKitchenLoot* L = *It;
				if (L && !L->bIsGear && !L->bVault && !L->bHeld && FVector::Dist(L->GetActorLocation(), Mouth) < 450.f)
				{
					if (GM && L->Value >= 20) { GM->Broadcast(FString::Printf(TEXT("Бадрик сожрал: %s"), *L->LootName), FLinearColor(0.9f, 0.7f, 0.5f)); }
					for (AAntCharacter* C : L->Carriers) { if (C) { C->Toast(TEXT("Собака съела твою еду!")); } }
					L->ReleaseAll();
					L->Destroy();
					break;
				}
			}
		}
	}

	StateTimer -= Dt;
	if (State == 1)
	{
		if (FVector::Dist2D(Me, Goal) < 60.f)
		{
			const FSpot& S = Spots[SpotIndex];
			State = S.Do == 0 ? 0 : (S.Do == 1 ? 2 : 3);
			StateTimer = S.Time * FMath::FRandRange(0.7f, 1.3f);
			if (State == 0) { Awake = 0.f; }
		}
		return;
	}
	if (StateTimer <= 0.f)
	{
		PickSpot();
	}
}

void ACritter::ServerMove(float Dt)
{
	const FVector Me = GetActorLocation();
	if (State == 4)
	{
		// Jump: a parabola from PounceFrom to PounceTo.
		const float Dur = Kind == 1 ? 0.55f : 0.8f;
		PounceT = FMath::Min(1.f, PounceT + Dt / Dur);
		const float Arc = FMath::Max(250.f, (float)FVector::Dist(PounceFrom, PounceTo) * 0.35f);
		FVector P = FMath::Lerp(PounceFrom, PounceTo, PounceT);
		P.Z += FMath::Sin(PounceT * PI) * Arc;
		FVector Dir = PounceTo - PounceFrom; Dir.Z = 0.f;
		if (!Dir.IsNearlyZero()) { SetActorRotation(FRotator(0.f, (float)Dir.Rotation().Yaw, 0.f)); }
		SetActorLocation(P);
		if (PounceT >= 1.f)
		{
			HurtAntsNear(GetWorld(), this, PounceTo, 190.f, 220.f, TEXT("придавлен кошкой"), 60);
			State = 3;
			StateTimer = SpotIndex >= 0 && FVector::Dist(PounceTo, Spots[SpotIndex].P) < 100.f ? Spots[SpotIndex].Time : 2.5f;
			if (SpotIndex >= 0 && FVector::Dist(PounceTo, Spots[SpotIndex].P) < 100.f && Spots[SpotIndex].Do == 0) { State = 0; Awake = 0.f; }
		}
		return;
	}
	if (State != 1)
	{
		return;
	}
	const float Spd = (Kind == 0 ? 650.f : 820.f) * (Fleeing > 0.f ? 1.7f : 1.f);
	Fleeing = FMath::Max(0.f, Fleeing - Dt);
	const FVector Way = AKitchenBuilder::Route(Me, Goal);
	FVector To = Way - Me;
	To.Z = 0.f;
	const float D = (float)To.Size();
	if (D > 1.f)
	{
		const FVector Step = To / D * FMath::Min(D, Spd * Dt);
		SetActorLocation(FVector(Me.X + Step.X, Me.Y + Step.Y, Goal.Z));
		const FRotator Want(0.f, (float)To.Rotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, Dt, 6.f));
	}
}

void ACritter::ServerHurtAnts(float Dt)
{
	HurtTimer += Dt;
	if (HurtTimer < 0.2f)
	{
		return;
	}
	const float T = HurtTimer;
	HurtTimer = 0.f;
	if (State != 1)
	{
		return;
	}
	// Paws on the move: whatever is under them is flattened.
	for (int32 i = 0; i < 4; ++i)
	{
		HurtAntsNear(GetWorld(), this, PawWorld(i), Kind == 0 ? 110.f : 80.f, 95.f * T, Kind == 0 ? TEXT("раздавлен лапой Бадрика") : TEXT("раздавлен кошачьей лапой"), 10);
	}
}

void ACritter::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority())
	{
		ThinkTimer += Dt;
		ServerThink(Dt);
		ServerMove(Dt);
		ServerHurtAnts(Dt);
	}
	if (GetNetMode() != NM_DedicatedServer && Vis)
	{
		AnimateVisual(Dt);
	}
}

void ACritter::AnimateVisual(float Dt)
{
	const FVector Here = GetActorLocation();
	const float V = (float)FVector::Dist2D(Here, LastPos) / FMath::Max(Dt, 0.001f);
	LastPos = Here;
	Speed = FMath::FInterpTo(Speed, V, Dt, 8.f);
	Anim += Dt * (1.f + Speed / 120.f);
	const bool bSleep = State == 0;
	const bool bAir = State == 4;
	// Body: lies flat when asleep, sits back on its haunches when sitting.
	FVector BodyLoc = FVector::ZeroVector;
	FRotator BodyRot = FRotator::ZeroRotator;
	if (bSleep) { BodyLoc.Z = -140.f; BodyRot.Roll = Kind == 1 ? 25.f : 0.f; }
	else if (State == 3) { BodyRot.Pitch = -18.f; BodyLoc.Z = -40.f; }
	else if (State == 2) { BodyRot.Pitch = 12.f; }
	else if (bAir) { BodyRot.Pitch = 15.f; }
	else { BodyLoc.Z = FMath::Abs(FMath::Sin(Anim * 6.f)) * 8.f * FMath::Min(1.f, Speed / 300.f); }
	Vis->SetRelativeLocationAndRotation(BodyLoc, BodyRot);
	// Legs trot; fold up in the air and when asleep.
	for (int32 i = 0; i < Legs.Num(); ++i)
	{
		float Pitch = 0.f;
		if (bAir) { Pitch = i < 2 ? -40.f : 35.f; }
		else if (bSleep) { Pitch = i < 2 ? -70.f : 60.f; }
		else if (Speed > 20.f) { Pitch = FMath::Sin(Anim * 6.f + (i == 0 || i == 3 ? 0.f : PI)) * FMath::Min(30.f, Speed * 0.08f); }
		Legs[i]->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	}
	if (Tail)
	{
		const float Wag = Kind == 0 ? (State == 2 || State == 3 ? FMath::Sin(Anim * 14.f) * 35.f : FMath::Sin(Anim * 4.f) * 10.f)
			: FMath::Sin(Anim * 2.5f) * 25.f;
		Tail->SetRelativeRotation(FRotator(Kind == 0 ? -60.f : -70.f, Wag, 0.f));
	}
	for (int32 i = 0; i < Ears.Num(); ++i)
	{
		const float s = i == 0 ? -1.f : 1.f;
		Ears[i]->SetRelativeRotation(FRotator(bSleep ? -35.f : FMath::Sin(Anim * 3.f + i) * 6.f, 0.f, (Kind == 0 ? 15.f : 20.f) * s));
	}
	for (UStaticMeshComponent* E : Eyes)
	{
		E->SetRelativeScale3D(FVector(0.6f, 0.6f, bSleep ? 0.08f : 0.6f) * (Kind == 0 ? 1.f : 0.73f));
	}
	if (Head)
	{
		Head->SetRelativeRotation(FRotator(bSleep ? -25.f : (State == 2 ? -35.f : FMath::Sin(Anim * 1.3f) * 6.f), FMath::Sin(Anim * 0.7f) * 12.f, 0.f));
	}
}

void ACritter::Bitten(AAntCharacter* By)
{
	if (!HasAuthority())
	{
		return;
	}
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	const FVector Me = GetActorLocation();
	if (Kind == 0)
	{
		// Yelp and shake: whoever rides him flies off.
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			const UAntMovementComponent* M = A ? A->GetAntMovement() : nullptr;
			const UPrimitiveComponent* B = M ? M->GetRiddenComponent() : nullptr;
			if (A && !A->bDead && B && B->GetOwner() == this)
			{
				A->ApplyLaunch((A->GetActorLocation() - Me).GetSafeNormal2D() * 500.f * AntKit::SpeedK + FVector(0.f, 0.f, 420.f * AntKit::SpeedK));
				A->Toast(TEXT("Бадрик отряхнулся — ты слетел!"));
			}
		}
		if (--Patience > 0)
		{
			if (GM) { GM->Broadcast(TEXT("Бадрик взвизгнул!"), FLinearColor(0.9f, 0.8f, 0.5f)); }
			if (State == 0) { State = 3; StateTimer = 3.f; }
			return;
		}
		// Enough: runs off to the farthest sleeping spot and sulks there.
		Patience = 3;
		int32 Far = -1;
		float BestD = -1.f;
		for (int32 i = 0; i < Spots.Num(); ++i)
		{
			const float D = (float)FVector::Dist(Spots[i].P, Me);
			if (Spots[i].Do == 0 && D > BestD) { BestD = D; Far = i; }
		}
		if (Far >= 0)
		{
			SpotIndex = Far;
			Goal = Spots[Far].P;
			State = 1;
			Fleeing = 8.f;
		}
		if (GM) { GM->Broadcast(TEXT("Бадрик, скуля, удрал от муравьёв!"), FLinearColor(0.9f, 0.8f, 0.5f)); }
		return;
	}
	// The cat: hisses, and either pounces straight back at the biter or leaps away.
	Awake = 1.f;
	if (State == 4)
	{
		return;
	}
	PounceFrom = Me;
	if (By && FMath::FRand() < 0.6f)
	{
		PounceTo = By->GetActorLocation();
		PounceTo.Z = Me.Z;
		if (GM) { GM->Broadcast(TEXT("Кошка зашипела и бросилась на обидчика!"), FLinearColor(1.f, 0.6f, 0.3f)); }
	}
	else
	{
		const FVector Away = By ? (Me - By->GetActorLocation()).GetSafeNormal2D() : FVector::ForwardVector;
		PounceTo = Me + Away * 900.f;
		if (GM) { GM->Broadcast(TEXT("Кошка зашипела и отскочила!"), FLinearColor(1.f, 0.6f, 0.3f)); }
	}
	PounceT = 0.f;
	State = 4;
}

// =============================================================================================
// AHouseholdNpc: the kid on the sofa and grandma in the armchair
// =============================================================================================

AHouseholdNpc::AHouseholdNpc()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AHouseholdNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHouseholdNpc, Kind);
	DOREPLIFETIME(AHouseholdNpc, Pose);
	DOREPLIFETIME(AHouseholdNpc, bWarning);
}

void AHouseholdNpc::BeginPlay()
{
	Super::BeginPlay();
	Habit = FMath::FRandRange(14.f, 26.f);
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildVisual();
	}
}

void AHouseholdNpc::BuildVisual()
{
	Vis = NewObject<USceneComponent>(this, TEXT("Vis"));
	Vis->SetupAttachment(RootComponent);
	Vis->RegisterComponent();
	auto P = [this](EAntShape S, const FVector& L, const FVector& Size, const FLinearColor& C, const FRotator& R = FRotator::ZeroRotator, bool bCol = true)
	{
		return AntKit::AddPart(this, Vis, S, L, Size / 100.f, C, R, bCol, false);
	};
	const FLinearColor Skin = Hex(235, 195, 160);
	if (Kind == 0)
	{
		// The kid, sitting on the sofa (actor at the seat, facing -Y towards the TV), legs dangling in front.
		const FLinearColor Shirt = Hex(60, 140, 200), Shorts = Hex(50, 60, 90), Hair = Hex(120, 70, 30);
		P(EAntShape::Cube, FVector(0.f, 0.f, 150.f), FVector(520.f, 420.f, 300.f), Shorts);                  // pelvis
		Chest = P(EAntShape::Cube, FVector(0.f, 60.f, 650.f), FVector(500.f, 330.f, 700.f), Shirt);          // torso
		P(EAntShape::Sphere, FVector(0.f, 40.f, 1230.f), FVector(240.f, 240.f, 260.f), Skin);                // head
		P(EAntShape::Sphere, FVector(0.f, 60.f, 1330.f), FVector(250.f, 250.f, 170.f), Hair, FRotator::ZeroRotator, false);
		for (float s : { -1.f, 1.f })
		{
			P(EAntShape::Cube, FVector(150.f * s, -380.f, 140.f), FVector(200.f, 760.f, 220.f), Shorts);           // thighs forward
			P(EAntShape::Cylinder, FVector(320.f * s, 0.f, 700.f), FVector(130.f, 130.f, 600.f), Shirt, FRotator(0.f, 0.f, 10.f * s));   // arms
			P(EAntShape::Sphere, FVector(360.f * s, -80.f, 380.f), FVector(110.f, 110.f, 110.f), Skin);            // hands on the knees
			// Shin hangs from the knee (pivot at the knee: the mesh is offset down).
			USceneComponent* Knee = AntKit::AddPivot(this, Vis, FVector(150.f * s, -760.f, 60.f));
			Shins.Add(AntKit::AddPart(this, Knee, EAntShape::Cylinder, FVector(0.f, 0.f, -480.f), FVector(1.5f, 1.5f, 9.6f), Skin, FRotator::ZeroRotator, true, false));
			Feet.Add(AntKit::AddPart(this, Knee, EAntShape::Cube, FVector(0.f, -80.f, -960.f), FVector(1.8f, 3.2f, 1.1f), Hex(240, 240, 240), FRotator::ZeroRotator, true, false));
		}
		P(EAntShape::Cube, FVector(0.f, -120.f, 820.f), FVector(220.f, 60.f, 120.f), Hex(230, 230, 230), FRotator::ZeroRotator, false);   // the game controller
	}
	else
	{
		// Grandma in the armchair (actor at the seat, facing +X), feet on the floor in front, knitting.
		const FLinearColor Cardigan = Hex(150, 90, 130), Skirt = Hex(70, 60, 90), HairC = Hex(225, 225, 225), Slipper = Hex(230, 120, 150);
		P(EAntShape::Cube, FVector(0.f, 0.f, 180.f), FVector(760.f, 900.f, 360.f), Skirt);                         // lap
		Chest = P(EAntShape::Cube, FVector(-250.f, 0.f, 800.f), FVector(560.f, 820.f, 900.f), Cardigan);           // torso, leaning back
		P(EAntShape::Sphere, FVector(-200.f, 0.f, 1500.f), FVector(280.f, 260.f, 300.f), Skin);                    // head
		P(EAntShape::Sphere, FVector(-300.f, 0.f, 1640.f), FVector(230.f, 230.f, 140.f), HairC, FRotator::ZeroRotator, false);   // bun
		P(EAntShape::Cube, FVector(-60.f, 0.f, 1480.f), FVector(60.f, 300.f, 40.f), Hex(80, 80, 90), FRotator::ZeroRotator, false);  // glasses
		for (float s : { -1.f, 1.f })
		{
			P(EAntShape::Cube, FVector(600.f, 280.f * s, 150.f), FVector(760.f, 300.f, 260.f), Skirt);                  // thighs forward
			P(EAntShape::Cylinder, FVector(100.f, 520.f * s, 700.f), FVector(150.f, 150.f, 700.f), Cardigan, FRotator(-60.f, 0.f, 0.f));   // arms down to the lap
			P(EAntShape::Sphere, FVector(420.f, 300.f * s, 420.f), FVector(120.f, 120.f, 100.f), Skin);                 // hands knitting
			USceneComponent* Knee = AntKit::AddPivot(this, Vis, FVector(980.f, 280.f * s, 100.f));
			Shins.Add(AntKit::AddPart(this, Knee, EAntShape::Cylinder, FVector(60.f, 0.f, -420.f), FVector(1.6f, 1.6f, 8.6f), Hex(200, 180, 170), FRotator(-8.f, 0.f, 0.f), true, false));
			Feet.Add(AntKit::AddPart(this, Knee, EAntShape::Cube, FVector(230.f, 0.f, -800.f), FVector(4.2f, 2.2f, 1.6f), Slipper, FRotator::ZeroRotator, true, false));
		}
		P(EAntShape::Cube, FVector(380.f, 0.f, 450.f), FVector(300.f, 420.f, 60.f), Hex(200, 60, 90));              // the knitting on her lap
		P(EAntShape::Cylinder, FVector(380.f, 120.f, 480.f), FVector(16.f, 16.f, 500.f), Steel_ish(), FRotator(0.f, 0.f, 70.f), false);   // needles
		P(EAntShape::Cylinder, FVector(380.f, -120.f, 480.f), FVector(16.f, 16.f, 500.f), Steel_ish(), FRotator(0.f, 0.f, -70.f), false);
	}
}

void AHouseholdNpc::Tick(float Dt)
{
	Super::Tick(Dt);
	Anim += Dt;
	if (HasAuthority())
	{
		Timer += Dt;
		// A second before the habit: the warning (the kid fidgets, grandma sighs).
		bWarning = Timer > Habit - 1.f && Timer <= Habit;
		if (Timer > Habit)
		{
			Timer = 0.f;
			Habit = FMath::FRandRange(Kind == 0 ? 12.f : 25.f, Kind == 0 ? 24.f : 45.f);
			AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
			if (Kind == 0)
			{
				if (Pose == 0)
				{
					Pose = 1;   // swings his legs for a while
					Habit = FMath::FRandRange(4.f, 7.f);
				}
				else
				{
					Pose = 0;
					// Something falls out of the snack bag now and then.
					if (GM && FMath::FRand() < 0.6f)
					{
						const FVector At = GetActorTransform().TransformPosition(FVector(FMath::FRandRange(-500.f, 500.f), -900.f, -900.f)) + FVector(0.f, 0.f, 80.f);
						GM->SpawnFood(At, FMath::FRand() < 0.3f ? 2 : 1);
						GM->Broadcast(TEXT("Малыш уронил крошку у дивана!"), FLinearColor(0.9f, 0.9f, 0.6f));
					}
				}
			}
			else
			{
				Pose = Pose == 0 ? 1 : 0;   // shifts her feet: whatever is under the one that moves...
				const FVector Foot = GetActorTransform().TransformPosition(FVector(1210.f, Pose == 0 ? 280.f : -280.f, -870.f));
				HurtAntsNear(GetWorld(), this, FVector(Foot.X, Foot.Y, GetActorLocation().Z - 800.f), 260.f, 300.f, TEXT("раздавлен бабушкиным тапком"), 40);
			}
		}
		// The kid's swinging feet scrape the floor in front of the sofa.
		if (Kind == 0 && Pose == 1)
		{
			HurtTimer += Dt;
			if (HurtTimer > 0.2f)
			{
				const float T = HurtTimer;
				HurtTimer = 0.f;
				const FVector Z = GetActorTransform().TransformPosition(FVector(0.f, -900.f, -950.f));
				for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
				{
					AAntCharacter* A = *It;
					if (A && !A->bDead && FMath::Abs(A->GetActorLocation().X - Z.X) < 560.f && FMath::Abs(A->GetActorLocation().Y - Z.Y) < 320.f && A->GetActorLocation().Z < Z.Z + 320.f)
					{
						A->LastHazard = TEXT("получил пинок от малыша");
						A->LastHazardTime = GetWorld()->GetTimeSeconds();
						UGameplayStatics::ApplyDamage(A, 140.f * T, nullptr, this, UDamageType::StaticClass());
					}
				}
			}
		}
	}
	if (GetNetMode() == NM_DedicatedServer || !Vis)
	{
		return;
	}
	// Animation: legs swing / feet shift; breathing; the warning twitch.
	Swing = FMath::FInterpTo(Swing, Kind == 0 && Pose == 1 ? 1.f : 0.f, Dt, 4.f);
	for (int32 i = 0; i < Shins.Num(); ++i)
	{
		USceneComponent* Knee = Shins[i]->GetAttachParent();
		if (!Knee) { continue; }
		if (Kind == 0)
		{
			const float A = FMath::Sin(Anim * 5.f + (i == 0 ? 0.f : PI)) * 32.f * Swing + (bWarning ? FMath::Sin(Anim * 30.f) * 4.f : 0.f);
			Knee->SetRelativeRotation(FRotator(A, 0.f, 0.f));
		}
		else
		{
			// Grandma: the pose says which foot is forward.
			const bool bFwd = (Pose == 0) == (i == 0);
			const FRotator Want(bFwd ? -8.f : 10.f, 0.f, 0.f);
			Knee->SetRelativeRotation(FMath::RInterpTo(Knee->GetRelativeRotation(), Want + FRotator(bWarning ? FMath::Sin(Anim * 25.f) * 3.f : 0.f, 0.f, 0.f), Dt, 5.f));
		}
	}
	if (Chest)
	{
		const float Breath = 1.f + FMath::Sin(Anim * (Kind == 0 ? 2.2f : 1.1f)) * 0.012f;
		const FVector Base = Kind == 0 ? FVector(5.f, 3.3f, 7.f) : FVector(5.6f, 8.2f, 9.f);
		Chest->SetRelativeScale3D(Base * FVector(Breath, Breath, 1.f));
	}
}
