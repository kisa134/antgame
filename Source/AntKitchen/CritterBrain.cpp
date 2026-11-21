#include "Critters.h"
#include "AntFluidWorld.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "AntFx.h"
#include "AntTypes.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntMovementComponent.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "KitchenArt.h"
#include "KitchenBuilder.h"
#include "KitchenEvents.h"
#include "KitchenInteractables.h"
#include "KitchenLoot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "KitchenSlick.h"
#include "LevelV3.h"
#include "NpcRoutine.h"

// =============================================================================================
// The pets' jointed bodies and their brains. Every number a player can feel is in this block
// (and in docs/FEATURES.md, section 18).
// =============================================================================================

namespace
{
	// ---- the cat
	constexpr float CatWalk = 520.f, CatStalk = 400.f, CatTrot = 620.f, CatRun = 1500.f;
	constexpr float CatSee[3] = { 520.f, 2300.f, 3600.f };   // how far it notices an ant, by boldness (NpcWorld::Level)
	constexpr float CatNoticeMoving = 1.1f;      // seconds of watching a moving / loaded / very close ant before it locks on
	constexpr float CatNoticeStill = 4.0f;       // the same for an ant that stands still: freezing works
	constexpr float CatStare = 0.9f;             // locked on: the head and the ears point at you for this long before it moves
	constexpr float CatPounceMin = 380.f, CatPounceMax = 1250.f, CatPouncePerch = 1800.f;
	constexpr float CatPounceFix = 150.f;        // how far the landing may follow the prey during the wiggle
	constexpr float CatPounceRadius = 170.f;     // around the front paws
	constexpr float CatPounceDamage = 25.f;       // to whoever else is under the paws; the one it catches only takes the pin
	constexpr float CatPinTime = 1.4f, CatPinDamage = 15.f, CatKillDamage = 100.f;
	constexpr int32 CatPins = 3;                 // the third time it does not let go
	constexpr int32 CatGrip = 4;                 // presses of [Space] to wriggle free
	constexpr float CatWatch = 1.6f;             // the head start after a pin
	constexpr float CatSwipeRange = 430.f, CatSwipeDamage = 15.f, CatSwipeThrow = 1000.f, CatSwipeEvery = 1.6f;
	constexpr float CatPounceEvery = 3.0f;       // after a miss
	constexpr float CatGuard = 6.f;              // how long it waits at cover it cannot get under
	constexpr float CatCover = 330.f;            // anything solid this low over an ant keeps a paw out
	constexpr float CatPawStep = 30.f;           // damage of a walking paw landing on an ant (x1.8 at a run)
	constexpr float CatCalm = 12.f;              // seconds it stays up on its perch after a fright
	// ---- the dog
	constexpr float DogWalk = 430.f, DogSniff = 380.f, DogTrot = 650.f, DogRun = 1350.f;
	constexpr float DogSmell[3] = { 0.f, 1300.f, 2800.f };       // how far from his nose he picks up a scent trail, by boldness
	constexpr float DogSmellMin = 0.25f;                          // the faintest trail he follows (a bare ant leaves 0.13 per quarter second, a loaded one 0.5+)
	constexpr float DogSeeAnt = 650.f;                            // he is short-sighted: an ant this close to his nose he goes for directly
	constexpr float DogLickRange = 300.f, DogLickRadius = 95.f, DogLickFix = 70.f, DogLickHigh = 170.f;
	constexpr float DogLickDamage = 50.f, DogLickEvery = 2.2f, DogStagger = 1.2f;   // the second lick swallows
	constexpr float DogCover = 260.f;                             // anything solid this low over an ant keeps his muzzle out
	constexpr float DogFoodSmell[3] = { 0.f, 1500.f, 3200.f };   // food ants have dropped
	constexpr float DogFoodLeft = 4.f;                            // seconds a dropped piece lies before he counts it as left for him
	constexpr float DogEatTime = 2.5f;
	constexpr float DogBarkEvery = 12.f, DogBarkLoud = 0.85f, DogBarkAfter = 2.f;
	constexpr float DogBallSee = 4000.f;
	constexpr int32 DogBallNudges = 3;
	constexpr float DogPawStep = 35.f;

	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }

	FVector ArtAt(const TCHAR* Name, int32 Index, const FVector& Fallback, const FVector& Off = FVector::ZeroVector)
	{
		const FKitchenArtInst* I = KitchenArt::Enabled() ? KitchenArt::Find(Name, Index) : nullptr;
		return I ? FVector(I->Loc) + Off : Fallback;
	}

	bool TopOf(const UWorld* W, FVector& P, float Up, float Down)
	{
		FHitResult Hit;
		if (W->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, Up), P - FVector(0.f, 0.f, Down), FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(NpcSpot), false)))
		{
			P.Z = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------------------------

void ACritter::SetupJointedBody()
{
	const TConstArrayView<FNpcSegDef> Defs = QuadBody::Segs(Kind);
	for (int32 i = 0; i < QuadSeg::Count && i < Segs.Num(); ++i)
	{
		UStaticMeshComponent* C = Segs[i];
		if (!Defs[i].Name)
		{
			continue;   // this creature has no such segment: the component stays hidden
		}
		C->SetStaticMesh(QuadBody::Mesh(Kind, i));
		QuadBody::ApplyLook(Kind, C);
		C->SetRelativeTransform(FTransform(FQuat::Identity, Defs[i].Joint, FVector(QuadBody::Scale)));
		if (i != QuadSeg::Tongue)
		{
			// Every segment is a moving surface: ants climb it and ride it. (The tongue flicks out and back: nothing stands on it.)
			UAntMovementComponent::MakeClimbable(C);
		}
		C->SetCastShadow(true);
		C->SetVisibility(true);
	}
	PoseLast = GetActorLocation();
}

float ACritter::ActT() const
{
	const float T = NpcRig::ActTime(this, Act);
	return DevFreezeAt >= 0.f ? FMath::Min(T, DevFreezeAt) : T;
}

bool ACritter::ActBusy() const
{
	const float T = ActT();
	return Act.Kind != QuadAct::None && T >= 0.f && T < QuadAct::Duration(Act.Kind, Act.Arg);
}

void ACritter::StartAct(uint8 InKind, uint8 Arg, const FVector& Target)
{
	if (!HasAuthority())
	{
		return;
	}
	Act.Kind = InKind;
	Act.Arg = Arg;
	Act.Start = GetWorld()->GetTimeSeconds();
	Act.Target = Target;
	Act.From = GetActorLocation();
	bActHit = false;
	ForceNetUpdate();
}

void ACritter::PoseBody(float Dt)
{
	const FVector Loc = GetActorLocation();
	const float Yaw = (float)GetActorRotation().Yaw;
	FVector BLoc = Loc;
	float BYaw = Yaw;
	const float T = ActT();
	if (!HasAuthority())
	{
		// A client gets the actor's place 20 times a second: the body follows it smoothly, and flies a jump by the same formula the server uses.
		if (!bVisInit || FVector::DistSquared(VisLoc, Loc) > FMath::Square(2500.f))
		{
			VisLoc = Loc;
			VisYaw = Yaw;
			bVisInit = true;
		}
		const float T0 = Act.Kind == QuadAct::Jump ? QuadAct::JumpCrouch : 0.f;
		const float Fly = FMath::Max(0.05f, Act.Arg * 0.01f);
		if ((Act.Kind == QuadAct::Pounce || Act.Kind == QuadAct::Jump) && T >= T0 && T < T0 + Fly)
		{
			VisLoc = QuadBody::AirPoint(FVector(Act.From), FVector(Act.Target), (T - T0) / Fly);
		}
		else
		{
			VisLoc += (Loc - VisLoc) * (1.f - FMath::Exp(-14.f * Dt));
		}
		VisYaw += FMath::FindDeltaAngleDegrees(VisYaw, Yaw) * (1.f - FMath::Exp(-12.f * Dt));
		BLoc = VisLoc;
		BYaw = VisYaw;
		BodyPivot->SetWorldLocationAndRotation(BLoc, FRotator(0.f, BYaw, 0.f));
	}
	if (Dt > 1.e-4f)
	{
		const FVector V = (BLoc - PoseLast) / Dt;
		PoseLast = BLoc;
		if (V.SizeSquared() > FMath::Square(6000.f))
		{
			PoseVel = FVector::ZeroVector;   // put somewhere else
		}
		else
		{
			PoseVel += (V - PoseVel) * (1.f - FMath::Exp(-10.f * Dt));
		}
	}
	FQuadIn In;
	In.Kind = Kind;
	In.Mode = State;
	In.Mood = Mood;
	In.Act = Act.Kind;
	In.Arg = Act.Arg;
	In.ActT = T;
	In.ActTarget = FVector(Act.Target);
	In.LookAt = FVector(LookAt);
	In.Body = FTransform(FRotator(0.f, BYaw, 0.f), BLoc);
	In.Vel = PoseVel;
	In.Now = GetWorld()->GetTimeSeconds();
	In.Dt = Dt;
	QuadBody::Pose(In, Rig, Segs, [this](int32 Leg, float Strength, const FVector& At) { OnFootfall(Leg, Strength, At); });
}

void ACritter::OnFootfall(int32 Leg, float Strength, const FVector& At)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// Soft pads for the cat, a heavier thump for the bulldog; dust when they run.
		AntSfx::Play(this, Kind == 1 ? AntSfx::StepFabric : AntSfx::Step, At, (Kind == 1 ? 0.2f : 0.38f) + 0.35f * Strength, FMath::FRandRange(0.9f, 1.1f) * (Kind == 1 ? 1.25f : 0.8f));
		if (Strength > 0.6f)
		{
			AntFx::LandDust(this, At + FVector(0.f, 0.f, 5.f), FVector::UpVector, 40.f + 70.f * Strength);
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	// The paw is down NOW: what is under it gets stepped on (a shuffle in place is gentler, a run is worse).
	const float R = Kind == 1 ? 55.f : 70.f;
	const float Dmg = (Kind == 1 ? CatPawStep : DogPawStep) * (Strength > 0.8f ? 1.8f : (Strength < 0.3f ? 0.5f : 1.f));
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->IsAttached() || FVector::DistSquared(A->GetActorLocation(), At + FVector(0.f, 0.f, 20.f)) > FMath::Square(R))
		{
			continue;
		}
		const UAntMovementComponent* M = A->GetAntMovement();
		const UPrimitiveComponent* B = M ? M->GetRiddenComponent() : nullptr;
		if (B && B->GetOwner() == this)
		{
			continue;   // riding it, not under it
		}
		A->HurtBy(Dmg, Kind == 1 ? TEXT("попал под кошачью лапу") : TEXT("попал под лапу Бадрика"));
	}
	AAntArmy::KillInSphere(GetWorld(), At, R, 6);
}

FVector ACritter::PawPoint(int32 Paw) const
{
	if (!bNewBody)
	{
		return GetActorLocation();
	}
	const UStaticMeshComponent* C = Segs[Paw == 0 ? QuadSeg::FLP : QuadSeg::FRP];
	return C->GetComponentTransform().TransformPosition(FVector(60.f, 0.f, -QuadBody::PawDrop(Kind, false)) / QuadBody::Scale);
}

FVector ACritter::GetNose() const
{
	if (!bNewBody)
	{
		return GetActorTransform().TransformPosition(FVector(700.f, 0.f, 150.f));
	}
	return Segs[QuadSeg::Head]->GetComponentTransform().TransformPosition(FVector(Kind == 1 ? 235.f : 215.f, 0.f, Kind == 1 ? -10.f : 20.f) / QuadBody::Scale);
}

// ---------------------------------------------------------------------------------------------
// The brain: what both of them share
// ---------------------------------------------------------------------------------------------

void ACritter::SetBrain(EBrain B, float Seconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	Brain = B;
	BrainSince = Now;
	BrainUntil = Now + Seconds;
}

void ACritter::BrainBegin()
{
	State = QuadMode::Sleep;
	Mood = QuadMood::Calm;
	SetBrain(EBrain::Rest, FMath::FRandRange(18.f, 30.f));
}

void ACritter::ResolveSpots()
{
	// Where they sleep, sit and eat: read from where the modelled furniture stands (KitchenArt), heights from the furniture itself.
	bSpotsReady = true;
	UWorld* W = GetWorld();
	auto Add = [&](FVector P, uint8 Do, float Time, bool bHigh, float FaceYaw = 1000.f)
	{
		if (bHigh)
		{
			if (!TopOf(W, P, 450.f, 450.f) || P.Z < 200.f)
			{
				UE_LOG(LogTemp, Warning, TEXT("ANTNPC %s: nothing to sit on at %s, spot dropped"), *DisplayName(), *P.ToCompactString());
				return;
			}
		}
		else
		{
			P.Z = 60.f;
			if (!TopOf(W, P, 200.f, 300.f)) { P.Z = 0.f; }
		}
		Rests.Add({ P, Do, Time, bHigh, FaceYaw });
	};
	if (Kind == 1)
	{
		// Level v2: she sleeps on the east cushion (over the blanket ramp), watches from the west one; the kid has the middle.
		Add(ArtAt(TEXT("SM_L_Sofa_SeatCushion"), 0, FVector(15500.f, 370.f, 950.f), FVector(0.f, 0.f, 250.f)), 0, 55.f, true, -90.f);    // asleep on the sofa: the cushion the kid does not sit on
		Add(ArtAt(TEXT("SM_L_Sofa_SeatCushion"), 2, FVector(11500.f, 370.f, 950.f), FVector(0.f, 0.f, 250.f)), 2, 25.f, true, -90.f);    // the other end of the sofa, watching the floor in front of it
		Add(ArtAt(TEXT("SM_L_CoffeeTable"), 0, FVector(13750.f, -3400.f, 950.f), FVector(-700.f, 0.f, 950.f)), 2, 16.f, true);            // on the coffee table, where it must not
		Add(FVector(0.f, -4900.f, 2200.f), 2, 25.f, true, 0.f);                                                                            // the kitchen window sill, lying along it
		Add(ArtAt(TEXT("SM_L_TVStand"), 0, FVector(12250.f, -8350.f, 900.f), FVector(-1500.f, 0.f, 900.f)), 2, 18.f, true, 90.f);          // on the TV stand, beside the TV
		Add(ArtAt(TEXT("SM_L_CatScratchPost"), 0, FVector(8000.f, -8300.f, 0.f), FVector(0.f, 0.f, 1400.f)), 2, 20.f, true);       // level v2: on top of its scratching post (beside it stands the side table now)
		Add(ArtAt(TEXT("SM_K_Rug"), 0, FVector(0.f, -150.f, 0.f), FVector(-520.f, 380.f, 0.f)), 2, 12.f, false);                    // the kitchen rug (not on the tennis ball)
		Add(FVector(12600.f, -1700.f, 0.f), 0, 30.f, false);                                                                        // level v2: a nap on the living-room floor by the toy blocks
	}
	else
	{
		Add(ArtAt(TEXT("SM_K_Rug"), 0, FVector(0.f, -150.f, 0.f), FVector(600.f, -450.f, 0.f)), 0, 50.f, false);                    // level v2: asleep on the rug under the dining table
		Add(ArtAt(TEXT("SM_K_PetBowl"), 0, FVector(-3800.f, -3800.f, 0.f), FVector(330.f, 150.f, 0.f)), 1, 12.f, false);            // the bowl
		Add(FVector(5000.f, -1500.f, 0.f), 1, 8.f, false);                                                                          // sniffing at the bin
		Add(FVector(8500.f, -500.f, 0.f), 1, 6.f, false);                                                                           // level v2: sniffing the shoes by the shoe rack in the living room
		Add(ArtAt(TEXT("SM_L_DogBed"), 0, FVector(18150.f, -7900.f, 0.f), FVector(0.f, 0.f, 0.f)), 0, 45.f, false);                 // asleep in his bed by the radiator
		Add(FVector(16700.f, -1500.f, 0.f), 2, 12.f, false);                                                                        // level v2: sitting by the east end of the sofa, begging
		Add(FVector(-5500.f, 1800.f, 0.f), 1, 7.f, false);                                                                          // licking up the flour
	}
	if (NpcRoutine::Enabled())
	{
		// Level v3: the spots of its evening (docs/level/LEVEL_V3.md, section 7). The ones that follow somebody move with them.
		const uint8 Who = Kind == 1 ? NpcRoutine::Cat : NpcRoutine::Dog;
		const TConstArrayView<NpcRoutine::FStep> Steps = NpcRoutine::Steps();
		for (int32 s = 0; s < Steps.Num(); ++s)
		{
			const NpcRoutine::FStep& St = Steps[s];
			if (St.Who != Who) { continue; }
			const uint8 Do = St.Pose == NpcRoutine::PRest ? St.Arg : 2;
			const int32 Before = Rests.Num();
			if (FMath::IsNearlyZero(St.X) && FMath::IsNearlyZero(St.Y)) { Rests.Add({ FVector::ZeroVector, Do, St.Secs, false, 1000.f }); }
			else { Add(FVector(St.X, St.Y, St.Z), Do, St.Secs, St.Z > 200.f, St.Pose == NpcRoutine::PDoor ? 0.f : 1000.f); }
			if (Rests.Num() > Before) { Rests.Last().Rt = s; }
		}
	}
	if (Rests.Num() == 0)
	{
		Rests.Add({ GetActorLocation(), 0, 40.f, false });
	}
	RestIndex = 0;
	LeapSpot = -1;
	SetActorLocation(Rests[0].P);
	UE_LOG(LogTemp, Display, TEXT("ANTNPC %s: %d rest spots, asleep at %s"), *DisplayName(), Rests.Num(), *Rests[0].P.ToCompactString());
}

void ACritter::Face(const FVector& Point, float Dt, float Rate)
{
	const FVector To = Flat(Point - GetActorLocation());
	if (To.SizeSquared() > 100.f)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, (float)To.Rotation().Yaw, 0.f), Dt, Rate));
	}
}

bool ACritter::WalkTo(const FVector& Dest, float Pace, float Dt, float Arrive)
{
	const FVector Me = GetActorLocation();
	if (FVector::Dist2D(Me, Dest) <= Arrive)
	{
		return true;
	}
	FVector Way = AKitchenBuilder::Route(Me, FVector(Dest.X, Dest.Y, Me.Z));
	if (NpcRoutine::Enabled() && FVector::Dist2D(Me, Dest) > 700.f && Me.Z < 250.f)
	{
		// Level v3: across the flat on the house grid (the Human's), round the furniture, not into it.
		if (AAntKitchenGameMode* NavGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			if (const FHumanNav* Nav = NavGM->HouseNav())
			{
				const FVector Next = NavFollow.Next(Nav, Me, FVector(Dest.X, Dest.Y, Me.Z), GetWorld()->GetTimeSeconds());
				if (NavFollow.Path.Num() > 0) { Way = FVector(Next.X, Next.Y, Me.Z); }
			}
		}
	}
	const float Radius = Kind == 1 ? 95.f : 125.f;
	FVector Dir = NpcWorld::Steer(GetWorld(), this, Me, Way, Radius, Radius + 75.f, SteerSide, SteerUntil);
	if (Dir.IsZero())
	{
		StuckTime += Dt;
		if (NpcRoutine::Enabled() && StuckTime > 1.f && FVector::Dist2D(Me, Dest) > 600.f && Me.Z < 250.f)
		{
			// Level v3: boxed in (under the slid-off blanket, between the sofa and the table) with somewhere to be: it squeezes out along its way.
			Dir = (Way - Me).GetSafeNormal2D();
		}
		if (Dir.IsZero())
		{
			return StuckTime > 1.6f;   // boxed in: call it there
		}
	}
	StuckTime = FMath::Max(0.f, StuckTime - Dt);
	// Spilled oil: the paws slide out sideways and it gets across slowly (it does not fall: it is a cat, or a bulldog).
	const AKitchenSlick* Oil = AKitchenSlick::At(GetWorld(), Me);
	const bool bOily = Oil && Oil->Kind == SlickKind::Oil;
	if (bOily && SlideVel.SizeSquared() < 400.f)
	{
		SlideVel = Dir.RotateAngleAxis(FMath::RandBool() ? 40.f : -40.f, FVector::UpVector) * Pace * 0.55f;
		UE_LOG(LogTemp, Display, TEXT("ANTSLICK %s slides on the oil at %s"), *DisplayName(), *Me.ToCompactString());
	}
	SlideVel *= FMath::Exp(-Dt * (bOily ? 1.2f : 4.f));
	// It turns as it goes, and does not get up to speed until it faces where it is going.
	const float Align = FMath::Clamp((float)FVector::DotProduct(GetActorForwardVector(), Dir) * 0.6f + 0.5f, 0.2f, 1.f);
	const float Step = FMath::Min((float)FVector::Dist2D(Me, Way), Pace * Align * Dt * (bOily ? 0.45f : 1.f));
	FVector Next = Me + Dir * Step + SlideVel * Dt;
	// The floor under the paws. It used to keep the height of the place it had rested on: the dog walked 15 uu in the air
	// off his mat, the cat at carpet height across the bare boards.
	const float Floor = NpcWorld::SurfaceZ(GetWorld(), Next, 150.f, 400.f);
	Next.Z = FMath::FInterpConstantTo((float)Me.Z, Floor, Dt, 900.f);
	SetActorLocation(Next);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, (float)Dir.Rotation().Yaw, 0.f), Dt, bOily ? 2.5f : Pace > 900.f ? 10.f : 6.f));
	return false;
}

int32 ACritter::CountRiders() const
{
	int32 N = 0;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		const UAntMovementComponent* M = It->GetAntMovement();
		const UPrimitiveComponent* B = (M && !It->bDead) ? M->GetRiddenComponent() : nullptr;
		if (B && B->GetOwner() == this) { ++N; }
	}
	return N;
}

void ACritter::ThrowRiders(float Force)
{
	const FVector Me = GetActorLocation();
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		const UAntMovementComponent* M = A ? A->GetAntMovement() : nullptr;
		const UPrimitiveComponent* B = (M && !A->bDead) ? M->GetRiddenComponent() : nullptr;
		if (B && B->GetOwner() == this)
		{
			A->ApplyLaunch(((A->GetActorLocation() - Me).GetSafeNormal2D() * Force + FVector(0.f, 0.f, Force * 0.8f)) * AntKit::SpeedK);
			A->Toast(Kind == 1 ? TEXT("Кошка стряхнула тебя!") : TEXT("Бадрик отряхнулся — ты слетел!"));
		}
	}
}

void ACritter::ReleaseHeld(bool bFling)
{
	if (AAntCharacter* A = HeldAnt.Get())
	{
		A->SetNpcHold(nullptr, FVector::ZeroVector);
		if (bFling && !A->bDead)
		{
			A->ApplyLaunch(((A->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 480.f + FVector(0.f, 0.f, 260.f)) * AntKit::SpeedK);
		}
	}
	HeldAnt = nullptr;
}

void ACritter::Struggle(AAntCharacter* Ant)
{
	if (!HasAuthority() || !Ant || HeldAnt.Get() != Ant)
	{
		return;
	}
	if (--Grip > 0)
	{
		Ant->Toast(FString::Printf(TEXT("Вырываешься... ещё %d"), Grip));
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	ReleaseHeld(true);
	Ant->Toast(TEXT("Вырвался! Беги в укрытие"));
	NextPounce = Now + 2.5f;
	NextSwipe = Now + 1.5f;
	NextLick = Now + 3.f;
	State = QuadMode::Move;
	SetBrain(EBrain::Watch, 1.0f);
	UE_LOG(LogTemp, Display, TEXT("ANTNPC %s: %s wriggled free"), *DisplayName(), *Ant->GetDisplayName());
}

void ACritter::BeginLeap(const FVector& To, bool bPounce)
{
	bAirPounce = bPounce;
	AirFly = QuadBody::AirTime(GetActorLocation(), To, bPounce);
	StartAct(bPounce ? QuadAct::Pounce : QuadAct::Jump, (uint8)FMath::Clamp(FMath::RoundToInt(AirFly * 100.f), 5, 250), To);
	SetBrain(bPounce ? EBrain::Pounce : EBrain::Leap, 0.f);
}

bool ACritter::TickAir(float Now)
{
	const float T = ActT();
	const float T0 = bAirPounce ? 0.f : QuadAct::JumpCrouch;
	const FVector From(Act.From), To(Act.Target);
	const FVector Dir = Flat(To - From);
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, (float)Dir.Rotation().Yaw, 0.f), GetWorld()->GetDeltaSeconds(), 12.f));
	}
	if (T < T0)
	{
		return true;   // gathering itself
	}
	const float U = (T - T0) / FMath::Max(0.05f, AirFly);
	if (U < 1.f)
	{
		State = QuadMode::Air;
		SetActorLocation(QuadBody::AirPoint(From, To, U));
		return true;
	}
	SetActorLocation(To);
	State = QuadMode::Move;
	Land(Now);
	return false;
}

void ACritter::ArriveAtSpot(int32 Index)
{
	if (!Rests.IsValidIndex(Index))
	{
		SetBrain(EBrain::Idle, 4.f);
		return;
	}
	RestIndex = Index;
	LeapSpot = -1;
	bSettling = true;
	const FRestSpot& S = Rests[Index];
	float Time = S.Time * FMath::FRandRange(0.7f, 1.3f);
	if (S.Rt >= 0 && NpcRoutine::Enabled())
	{
		// Level v3: there for as long as the plan says (it gets up earlier if the plan moves on).
		Time = S.Time;
		Task.Phase = 2;
		Task.Start = GetWorld()->GetTimeSeconds();
		NpcRoutine::Log(GetWorld(), Kind == 1 ? NpcRoutine::Cat : NpcRoutine::Dog, TEXT("arrived, at it"), S.Rt, GetActorLocation());
		const NpcRoutine::FStep& St = NpcRoutine::Steps()[S.Rt];
		if (St.Pose == NpcRoutine::PBeg && Kind == 0)
		{
			// Sitting up at the table, eyes on the Dad's plate.
			State = QuadMode::Sit;
			Mood = QuadMood::Happy;
			StartAct(QuadAct::Beg, (uint8)FMath::Clamp(FMath::RoundToInt(Time * 10.f), 20, 250), FVector(Task.Target) + FVector(0.f, 0.f, 900.f));
			SetBrain(EBrain::Idle, Time);
			return;
		}
	}
	if (S.Do == 0)
	{
		State = QuadMode::Sleep;
		SetBrain(EBrain::Rest, Time);
	}
	else
	{
		State = (Kind == 0 && S.Do == 1) ? QuadMode::NoseDown : QuadMode::Sit;
		SetBrain(EBrain::Idle, Time);
	}
}

void ACritter::GoToSpot(int32 Index)
{
	if (!Rests.IsValidIndex(Index))
	{
		return;
	}
	LeapSpot = Index;
	WalkGoal = Rests[Index].P;
	LeapAfter = FVector::ZeroVector;
	State = QuadMode::Move;
	SetBrain(EBrain::Go, 50.f);
}

bool ACritter::RoutineSpot()
{
	UWorld* W = GetWorld();
	const uint8 Who = Kind == 1 ? NpcRoutine::Cat : NpcRoutine::Dog;
	float Left = 0.f;
	const int32 S = NpcRoutine::Current(W, Who, &Left);
	RtStep = S;
	if (S < 0)
	{
		Task.Step = 255;
		Task.Phase = 0;
		return false;
	}
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < Rests.Num(); ++i) { if (Rests[i].Rt == S) { Idx = i; } }
	if (Idx == INDEX_NONE)
	{
		return false;
	}
	const NpcRoutine::FStep& St = NpcRoutine::Steps()[S];
	FVector Target = FVector(St.X, St.Y, 0.f);
	if (St.Pose == NpcRoutine::PBeg || St.Pose == NpcRoutine::PFollow)
	{
		// Whom it follows: the Dad (begging at the table, at the stove) or the kid.
		AActor* Who2 = nullptr;
		if (St.Pose == NpcRoutine::PFollow) { for (TActorIterator<AKidCharacter> It(W); It; ++It) { if (!It->IsHidden()) { Who2 = *It; } } }
		else { for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (!It->IsA<AHouseFolk>() && !It->IsHidden()) { Who2 = *It; } } }
		if (!Who2)
		{
			return false;
		}
		const FVector L = Who2->GetActorLocation();
		Target = FVector(L.X, L.Y, 0.f);
		const FVector Off = St.Pose == NpcRoutine::PFollow ? -Who2->GetActorForwardVector() * 650.f + Who2->GetActorRightVector() * 300.f : Who2->GetActorForwardVector() * 420.f - Who2->GetActorRightVector() * 520.f;
		Rests[Idx].P = FVector(L.X + Off.X, L.Y + Off.Y, 0.f);
	}
	Rests[Idx].Time = FMath::Clamp(Left, 6.f, 120.f);
	Task.Step = (uint8)S;
	Task.Phase = 1;
	Task.Start = W->GetTimeSeconds();
	Task.Target = Target;
	NpcRoutine::Log(W, Who, TEXT("goes to it"), S, GetActorLocation());
	GoToSpot(Idx);
	return true;
}

void ACritter::PickNextSpot()
{
	if (NpcRoutine::Enabled() && RoutineSpot())
	{
		return;
	}
	if (Rests.Num() < 2)
	{
		SetBrain(EBrain::Idle, 5.f);
		return;
	}
	int32 Next = RestIndex;
	for (int32 Guard = 0; Guard < 8 && Next == RestIndex; ++Guard)
	{
		Next = FMath::RandRange(0, Rests.Num() - 1);
	}
	GoToSpot(Next);
}

void ACritter::BrainTick(float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bSpotsReady)
	{
		if (Now < 1.2f)
		{
			return;   // the house is still being put together
		}
		ResolveSpots();
	}
	if (bDevPuppet)
	{
		if (Brain == EBrain::Pounce || Brain == EBrain::Leap)
		{
			TickAir(Now);
		}
		else if (!DevWalk.IsZero() && WalkTo(DevWalk, DevWalkSpeed, Dt, 60.f))
		{
			DevWalk = FVector::ZeroVector;
		}
		if (Brain == EBrain::Swipe && !bActHit && ActT() >= QuadAct::SwipeHit)
		{
			bActHit = true;
			DoSwipeHit();
		}
		return;
	}
	if (NpcRoutine::Enabled() && (Brain == EBrain::Rest || Brain == EBrain::Idle) && Now - BrainSince > 2.f && !ActBusy())
	{
		// Level v3: the plan has moved on (or the Dad sat down to eat, went out with the rubbish): up and off to the next spot.
		const uint8 Who = Kind == 1 ? NpcRoutine::Cat : NpcRoutine::Dog;
		const int32 S = NpcRoutine::Current(GetWorld(), Who);
		bool bMove = S != RtStep || RtNudge != NpcRoutine::Nudge(Who);
		if (!bMove && S >= 0 && NpcRoutine::Steps()[S].Pose == NpcRoutine::PFollow && Rests.IsValidIndex(RestIndex))
		{
			for (TActorIterator<AKidCharacter> It(GetWorld()); It; ++It) { bMove = bMove || FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > 1600.f; }
		}
		if (bMove)
		{
			RtNudge = NpcRoutine::Nudge(Who);
			if (S >= 0)
			{
				State = QuadMode::Move;
				PickNextSpot();
			}
			else
			{
				RtStep = -1;
				Task.Step = 255;
				Task.Phase = 0;
			}
		}
	}
	if (Kind == 1)
	{
		CatThink(Dt, Now);
	}
	else
	{
		DogThink(Dt, Now);
	}
}

// ---------------------------------------------------------------------------------------------
// The cat
// ---------------------------------------------------------------------------------------------

bool ACritter::PreyValid(const AAntCharacter* A) const
{
	return A && !A->bDead && !A->IsInNest() && (!A->IsAttached() || HeldAnt.Get() == A);
}

AAntCharacter* ACritter::FindPrey(float Range, bool bAnyRoom, float* OutDist) const
{
	const UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	const FVector EyeAt = Me + FVector(0.f, 0.f, 330.f);
	const int32 MyRoom = AKitchenBuilder::Region(Me);
	AAntCharacter* Best = nullptr;
	float BestScore = 1.e9f;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->Creature != 0 || A->IsAttached())
		{
			continue;
		}
		const FVector L = A->GetActorLocation();
		const float D = (float)FVector::Dist2D(L, Me);
		if (D > Range || (!bAnyRoom && AKitchenBuilder::Region(L) != MyRoom))
		{
			continue;
		}
		// Only what is on its own level or below it: not up a wall, not on a table above its head.
		if (L.Z > Me.Z + 240.f || Me.Z - L.Z > 1600.f)
		{
			continue;
		}
		const UAntMovementComponent* M = A->GetAntMovement();
		if (M && M->GetRiddenComponent() && M->GetRiddenComponent()->GetOwner() == this)
		{
			continue;   // on its own back: it cannot see that
		}
		if (KitchenContainers::SealedAt(W, L) >= 0 || !NpcWorld::CanSee(W, EyeAt, L + FVector(0.f, 0.f, 12.f)))
		{
			continue;
		}
		const float Score = D - (A->IsCarrying() ? 500.f : 0.f) - (A->GetVelocity().SizeSquared() > 3600.f ? 250.f : 0.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = A;
			if (OutDist) { *OutDist = D; }
		}
	}
	return Best;
}

void ACritter::Scare(const FVector& From, const TCHAR* Why)
{
	if (!HasAuthority() || !bNewBody || !bSpotsReady || bDevPuppet)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (Kind == 0)
	{
		// The dog does not run from a noise: he barks at it (and the Human hears the barking).
		if ((Brain == EBrain::Rest || Brain == EBrain::Idle || Brain == EBrain::Go || Brain == EBrain::Track) && Mood != QuadMood::Sulking && Now >= NextBark)
		{
			BeginBark(From);
		}
		return;
	}
	if (Brain == EBrain::Flee || Brain == EBrain::Cower || Brain == EBrain::Pounce || Brain == EBrain::Leap)
	{
		return;
	}
	ReleaseHeld(true);
	PreyAnt = nullptr;
	NoticeLevel = 0.f;
	Pins = 0;
	Mood = QuadMood::Scared;
	LookAt = From;
	ScareFrom = From;
	ScareWhy = Why;
	if (State == QuadMode::Sleep || State == QuadMode::Sit) { State = QuadMode::Move; }
	StartAct(QuadAct::Startle, 0, From);
	// Up onto the high place farthest from it (in this room if there is one).
	const FVector Me = GetActorLocation();
	const int32 Room = AKitchenBuilder::Region(Me);
	int32 Pick = -1;
	float BestD = -1.f;
	for (int32 Pass = 0; Pass < 2 && Pick < 0; ++Pass)
	{
		for (int32 i = 0; i < Rests.Num(); ++i)
		{
			if (!Rests[i].bHigh || (Pass == 0 && AKitchenBuilder::Region(Rests[i].P) != Room))
			{
				continue;
			}
			const float D = (float)FVector::Dist2D(Rests[i].P, From);
			if (D > BestD) { BestD = D; Pick = i; }
		}
	}
	RestIndex = Pick >= 0 ? Pick : RestIndex;
	SetBrain(EBrain::Flee, 20.f);
	AntSfx::Everyone(this, AntSfx::Hiss, Me + FVector(0.f, 0.f, 300.f), 1.3f);
	if (GM)
	{
		GM->Broadcast(FString::Printf(TEXT("Кошка испугалась (%s) и удирает наверх!"), Why), FLinearColor(1.f, 0.75f, 0.4f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCAT scared by %s at %s, flees to spot %d"), Why, *From.ToCompactString(), RestIndex);
}

bool ACritter::CheckFrights(float Now)
{
	if (Brain == EBrain::Flee || Brain == EBrain::Cower || Brain == EBrain::Pounce || Brain == EBrain::Leap)
	{
		return false;
	}
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	// The robot vacuum: its sworn enemy.
	for (TActorIterator<ARobotVacuum> It(W); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Me) < 2600.f && AKitchenBuilder::Region(It->GetActorLocation()) == AKitchenBuilder::Region(Me) && Me.Z < 250.f)
		{
			Scare(It->GetActorLocation(), TEXT("пылесоса"));
			return true;
		}
	}
	// Water on the floor.
	for (TActorIterator<AKitchenFlood> It(W); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Me) < It->PoolRadius + 700.f && Me.Z < 250.f)
		{
			Scare(It->GetActorLocation(), TEXT("воды"));
			return true;
		}
	}
	// The real water (AntFluidWorld): the cat will not set a paw in it and runs; Badrik laps at it (the puddle gets smaller).
	if (UAntFluidWorld* Fluid = UAntFluidWorld::Active(this))
	{
		if (Me.Z < 250.f && Now >= NextWaterLook)
		{
			NextWaterLook = Now + 0.5f;
			const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
			FVector WetAt = FVector::ZeroVector; bool bWet = false;
			for (const FVector& P : { Me, Me + Fwd * 350.f, Me + Fwd * 650.f })
			{
				if (Fluid->DepthAt(FVector(P.X, P.Y, Me.Z + 20.f)) > 1.5f) { WetAt = P; bWet = true; break; }
			}
			if (bWet && Kind == 1)
			{
				Scare(WetAt, TEXT("воды"));
				return true;
			}
			if (bWet && Kind != 1)
			{
				Fluid->Mop(Me + Fwd * 200.f, 140.f, 0.5f * 0.35f);   // a dog drinks about a third of what a towel takes
				if (Now >= NextLapLog) { NextLapLog = Now + 6.f; UE_LOG(LogTemp, Display, TEXT("ANTFLUID %s laps at the water at %s"), *DisplayName(), *WetAt.ToCompactString()); }
			}
		}
	}
	// A crash, the toaster going off, the trap snapping, the tap gushing.
	if (const AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
	{
		float Newest = LastNoiseTime;
		const FAntNoise* Loud = nullptr;
		for (const FAntNoise& N : GM->Run.Noises)
		{
			if (N.Time <= LastNoiseTime)
			{
				continue;
			}
			Newest = FMath::Max(Newest, N.Time);
			const bool bScary = N.Kind == AntNoise::Crash || N.Kind == AntNoise::Trap || N.Kind == AntNoise::Toaster || N.Kind == AntNoise::Microwave
				|| (N.Kind == AntNoise::Faucet && Kind == 1) || (N.Kind == AntNoise::Bark && Kind == 1 && FVector::Dist(N.Loc, Me) < 1500.f);
			if (bScary && N.Loud >= 0.5f && FVector::Dist(N.Loc, Me) < 3500.f)
			{
				Loud = &N;
			}
		}
		LastNoiseTime = Newest;
		if (Loud)
		{
			const FVector At = Loud->Loc;
			const TCHAR* Name = AntNoise::Name(Loud->Kind);
			Scare(At, Name);
			return Kind == 1;
		}
	}
	// Something heavy coming down right next to it (or on it).
	if (Now >= NextFallCheck)
	{
		NextFallCheck = Now + 0.12f;
		const AActor* Fell = nullptr;
		auto Check = [&](const AActor* A)
		{
			const FVector P = A->GetActorLocation();
			if (!Fell && A->GetVelocity().Z < -550.f && P.Z > Me.Z + 60.f && P.Z < Me.Z + 900.f && FVector::DistSquared2D(P, Me) < FMath::Square(520.f)) { Fell = A; }
		};
		for (TActorIterator<AKitchenProp> It(W); It && !Fell; ++It) { Check(*It); }
		for (TActorIterator<AKitchenLoot> It(W); It && !Fell; ++It) { if (!It->bHeld && !It->IsCarried() && It->Weight >= 3) { Check(*It); } }
		if (Fell)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTNPC %s: something fell on it (%s)"), Kind == 0 ? TEXT("dog") : TEXT("cat"), *Fell->GetName());
			Scare(Fell->GetActorLocation(), TEXT("того, что упало сверху"));
			return Kind == 1;
		}
	}
	// It keeps out of the dog's way.
	if (Kind == 1 && Me.Z < 250.f && Now >= NextDogFright && Brain != EBrain::Pin && Brain != EBrain::Wiggle)
	{
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == 0 && It->State != QuadMode::Sleep && It->State != QuadMode::Sit && FVector::Dist2D(It->GetActorLocation(), Me) < 850.f)
			{
				NextDogFright = Now + 20.f;
				Scare(It->GetActorLocation(), TEXT("Бадрика"));
				return true;
			}
		}
	}
	return false;
}

void ACritter::BeginWiggle(AAntCharacter* Prey)
{
	const FVector PL = Prey->GetActorLocation();
	FVector Aim = PL + Flat(Prey->GetVelocity()) * 0.3f;
	Aim.Z = PL.Z;
	if (!TopOf(GetWorld(), Aim, 30.f, 250.f)) { Aim.Z = PL.Z - AAntCharacter::BodyRadius; }
	AirPaws = Aim;
	State = QuadMode::Stalk;
	StartAct(QuadAct::Wiggle, 0, Aim);
	SetBrain(EBrain::Wiggle, QuadAct::WiggleTime);
	Prey->Toast(TEXT("Кошка припала к полу — сейчас прыгнет! Рывок в сторону!"));
	UE_LOG(LogTemp, Display, TEXT("ANTCAT wiggle at %s (%.0f away), prey %s"), *Aim.ToCompactString(), FVector::Dist2D(GetActorLocation(), Aim), *Prey->GetDisplayName());
}

void ACritter::BeginSwipe(AAntCharacter* Prey)
{
	const FVector PL = Prey->GetActorLocation() - FVector(0.f, 0.f, AAntCharacter::BodyRadius);
	const float Side = (float)FVector::DotProduct(PL - GetActorLocation(), GetActorRightVector());
	if (State != QuadMode::Sit) { State = QuadMode::Stalk; }
	StartAct(QuadAct::Swipe, Side >= 0.f ? 1 : 0, PL);
	SetBrain(EBrain::Swipe, 1.0f);
	UE_LOG(LogTemp, Display, TEXT("ANTCAT swipe (paw %d) at %s"), Side >= 0.f ? 1 : 0, *PL.ToCompactString());
}

void ACritter::DoSwipeHit()
{
	UWorld* W = GetWorld();
	const FVector T(Act.Target);
	const float Sg = (Act.Arg & 1) ? 1.f : -1.f;
	const FVector Rt = GetActorRightVector();
	const FVector A1 = T + Rt * (Sg * 190.f), A2 = T - Rt * (Sg * 170.f);
	const FVector Sweep = -Rt * Sg;
	int32 Hit = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->IsAttached())
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		if (FMath::PointDistToSegment(P, A1, A2) > 115.f || FMath::Abs(P.Z - T.Z) > 170.f)
		{
			continue;
		}
		// Batted across the floor: it hurts a little and it throws you a long way (and what you carried stays behind).
		if (A->IsCarrying()) { A->LetGo(); }
		A->HurtBy(CatSwipeDamage, TEXT("получил кошачьей лапой"));
		if (!A->bDead)
		{
			A->ApplyLaunch((Sweep * CatSwipeThrow + GetActorForwardVector() * 250.f + FVector(0.f, 0.f, 330.f)) * AntKit::SpeedK);
			A->Toast(TEXT("Кошка наподдала лапой!"));
		}
		++Hit;
	}
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		if (!It->bHeld && FMath::PointDistToSegment(It->GetActorLocation(), A1, A2) < 150.f && FMath::Abs(It->GetActorLocation().Z - T.Z) < 200.f)
		{
			It->Kick(Sweep * 520.f + FVector(0.f, 0.f, 300.f), It->GetActorLocation() + FMath::VRand() * 8.f);
		}
	}
	AAntArmy::KillInSphere(W, T, 130.f, 15);
	AntSfx::Everyone(this, AntSfx::Swat, T, 1.5f);
	UE_LOG(LogTemp, Display, TEXT("ANTCAT swipe landed at %s: %d ants batted"), *T.ToCompactString(), Hit);
}

void ACritter::BeginPin(AAntCharacter* Ant)
{
	++Pins;
	Grip = CatGrip;
	HeldAnt = Ant;
	PreyAnt = Ant;
	const FVector AL = Ant->GetActorLocation();
	const int32 Side = FVector::DotProduct(AL - GetActorLocation(), GetActorRightVector()) >= 0.f ? 1 : 0;
	UStaticMeshComponent* PawC = Segs[Side ? QuadSeg::FRP : QuadSeg::FLP];
	// Under the toes of that paw: the player sees out from under it.
	Ant->SetNpcHold(PawC, FVector(84.f, 0.f, -QuadBody::PawDrop(Kind, false) + AAntCharacter::BodyRadius) / QuadBody::Scale);
	Ant->HurtBy(CatPinDamage, TEXT("придавлен кошачьей лапой"));
	State = QuadMode::Move;
	StartAct(QuadAct::Pin, (uint8)FMath::RoundToInt(CatPinTime * 10.f), AL - FVector(0.f, 0.f, AAntCharacter::BodyRadius));
	SetBrain(EBrain::Pin, CatPinTime);
	if (!Ant->bDead)
	{
		Ant->Toast(Pins == 1 ? TEXT("Кошка прижала тебя лапой! [Пробел] — вырываться. Свои могут её укусить")
			: (Pins >= CatPins ? TEXT("Кошке надоело играть... ВЫРЫВАЙСЯ!") : TEXT("Опять поймала! [Пробел] — вырываться")));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCAT pin %d on %s (paw %d)"), Pins, *Ant->GetDisplayName(), Side);
}

void ACritter::Land(float Now)
{
	const FVector Me = GetActorLocation();
	if (bAirPounce)
	{
		bAirPounce = false;
		UWorld* W = GetWorld();
		AntSfx::Everyone(this, AntSfx::Stomp, AirPaws, 1.6f);
		AAntCharacter* Caught = nullptr;
		float BestD = CatPounceRadius;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->IsInNest() || A->IsAttached() || FMath::Abs(A->GetActorLocation().Z - AirPaws.Z) > 220.f)
			{
				continue;
			}
			const float D = (float)FVector::Dist2D(A->GetActorLocation(), AirPaws);
			if (D >= CatPounceRadius)
			{
				continue;
			}
			if (A->Creature == 0 && D < BestD)
			{
				BestD = D;
				Caught = A;
			}
		}
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A == Caught || A->bDead || A->IsInNest() || A->IsAttached() || FMath::Abs(A->GetActorLocation().Z - AirPaws.Z) > 220.f
				|| FVector::Dist2D(A->GetActorLocation(), AirPaws) >= CatPounceRadius)
			{
				continue;
			}
			A->HurtBy(CatPounceDamage, TEXT("придавлен кошкой"));
			if (!A->bDead)
			{
				A->ApplyLaunch(((A->GetActorLocation() - AirPaws).GetSafeNormal2D() * 520.f + FVector(0.f, 0.f, 300.f)) * AntKit::SpeedK);
			}
		}
		AAntArmy::KillInSphere(W, AirPaws, CatPounceRadius, 40);
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			const FVector To = It->GetActorLocation() - AirPaws;
			if (!It->bHeld && To.Size2D() < 420.f && FMath::Abs(To.Z) < 250.f)
			{
				It->Kick(To.GetSafeNormal2D() * 380.f + FVector(0.f, 0.f, 420.f), It->GetActorLocation() + FMath::VRand() * 8.f);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTCAT landed at %s (paws %s): %s"), *Me.ToCompactString(), *AirPaws.ToCompactString(), Caught ? *Caught->GetDisplayName() : TEXT("missed"));
		if (Caught)
		{
			if (PreyAnt.Get() != Caught) { Pins = 0; }   // a new toy: the game starts over
			BeginPin(Caught);
			return;
		}
		NextPounce = Now + CatPounceEvery;
		SetBrain(EBrain::Watch, 0.9f);
		return;
	}
	const bool bScared = Mood == QuadMood::Scared;
	if (!LeapAfter.IsZero())
	{
		WalkGoal = LeapAfter;
		LeapAfter = FVector::ZeroVector;
		SetBrain(bScared ? EBrain::Flee : EBrain::Go, bScared ? 20.f : 50.f);
	}
	else if (bScared)
	{
		State = QuadMode::Fright;
		SetBrain(EBrain::Cower, CatCalm);
	}
	else if (PreyValid(PreyAnt.Get()) && Mood == QuadMood::Hunting)
	{
		SetBrain(EBrain::Stalk, 20.f);
	}
	else if (LeapSpot >= 0 && FVector::Dist2D(Me, Rests[LeapSpot].P) < 250.f)
	{
		ArriveAtSpot(LeapSpot);
	}
	else
	{
		State = QuadMode::Sit;
		SetBrain(EBrain::Idle, 3.f);
	}
}

void ACritter::BittenNew(AAntCharacter* By)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (Now - LastBite < 0.4f)
	{
		return;
	}
	if (Now - LastBite > 20.f) { Bites = 0; }
	++Bites;
	LastBite = Now;
	const FVector Me = GetActorLocation();
	const bool bFreed = HeldAnt.IsValid();
	ReleaseHeld(true);
	ThrowRiders(Kind == 1 ? 380.f : 500.f);
	if (Brain == EBrain::Pounce || Brain == EBrain::Leap)
	{
		return;
	}
	if (Kind == 0)
	{
		// Бадрик: a yelp and a shake; three bites and he runs off to sulk.
		AntSfx::Everyone(this, AntSfx::Hit, Me + FVector(0.f, 0.f, 300.f), 1.7f);
		Interest = nullptr;
		if (Bites >= 3)
		{
			Bites = 0;
			int32 Far = RestIndex;
			float BestD = -1.f;
			for (int32 i = 0; i < Rests.Num(); ++i)
			{
				const float D = (float)FVector::Dist(Rests[i].P, Me);
				if (Rests[i].Do == 0 && D > BestD) { BestD = D; Far = i; }
			}
			RestIndex = Far;
			Mood = QuadMood::Sulking;
			State = QuadMode::Move;
			StartAct(QuadAct::Yelp, 0, Me);
			SetBrain(EBrain::Sulk, 40.f);
			if (GM) { GM->Broadcast(TEXT("Бадрик, скуля, удрал от муравьёв!"), FLinearColor(0.9f, 0.8f, 0.5f)); }
		}
		else
		{
			if (State == QuadMode::Sleep) { State = QuadMode::Move; }
			StartAct(QuadAct::Yelp, 0, Me);
			NextLick = Now + 2.5f;
			bShook = false;
			SetBrain(EBrain::Watch, 1.8f);
			if (GM) { GM->Broadcast(TEXT("Бадрик взвизгнул!"), FLinearColor(0.9f, 0.8f, 0.5f)); }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTDOG bitten by %s (%d)"), By ? *By->GetDisplayName() : TEXT("-"), Bites);
		return;
	}
	// The cat: enough bites and it gives up; otherwise it hisses and goes for whoever did it.
	AntSfx::Everyone(this, AntSfx::Hiss, Me + FVector(0.f, 0.f, 300.f), 1.4f);
	UE_LOG(LogTemp, Display, TEXT("ANTCAT bitten by %s (%d in a row, freed a captive: %d)"), By ? *By->GetDisplayName() : TEXT("-"), Bites, bFreed ? 1 : 0);
	if (Bites >= 3)
	{
		Bites = 0;
		Scare(By ? By->GetActorLocation() : Me, TEXT("укусов"));
		return;
	}
	if (State == QuadMode::Sleep || State == QuadMode::Sit) { State = QuadMode::Move; }
	StartAct(QuadAct::Startle, 0, Me);
	Mood = QuadMood::Hunting;
	Pins = 0;
	PreyAnt = By;
	NoticeLevel = 1.f;
	if (By) { PreySeen = By->GetActorLocation(); LookAt = PreySeen; }
	PreySeenTime = Now;
	NextPounce = Now + 1.0f;
	NextSwipe = Now + 0.9f;
	SetBrain(EBrain::Notice, 0.7f);
	if (GM) { GM->Broadcast(bFreed ? TEXT("Кошку укусили — она выпустила добычу!") : TEXT("Кошка зашипела!"), FLinearColor(1.f, 0.6f, 0.3f)); }
}

void ACritter::CatThink(float Dt, float Now)
{
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	if (Brain == EBrain::Pounce || Brain == EBrain::Leap)
	{
		TickAir(Now);
		return;
	}
	if (CheckFrights(Now))
	{
		return;
	}
	const bool bHuntEvent = HuntUntil > Now;
	const int32 Level = bHuntEvent ? 2 : NpcWorld::Level(W, NpcWho::Cat);
	if (bSettling && (Brain == EBrain::Rest || Brain == EBrain::Idle) && Rests.IsValidIndex(RestIndex) && Rests[RestIndex].Yaw < 999.f && NoticeLevel <= 0.f)
	{
		// Settling on its spot: it turns round to lie the way the spot wants (along the window sill, facing out from the sofa).
		const FRotator Want(0.f, Rests[RestIndex].Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, Dt, 3.f));
		bSettling = FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, Want.Yaw)) > 2.f && Now - BrainSince < 4.f;
	}
	else if (Brain != EBrain::Rest && Brain != EBrain::Idle)
	{
		bSettling = false;
	}
	const bool bPerched = Me.Z > 250.f;
	const FVector EyeAt = Me + FVector(0.f, 0.f, 330.f);

	// Ants walking about on it: it puts up with that for a moment, asleep a little longer.
	RiderTime = CountRiders() > 0 ? RiderTime + Dt : 0.f;
	if (RiderTime > (State == QuadMode::Sleep ? 4.f : 2.2f) && Now >= NextShake && Brain != EBrain::Pin && Brain != EBrain::Wiggle)
	{
		NextShake = Now + 5.f;
		RiderTime = 0.f;
		if (State == QuadMode::Sleep)
		{
			State = QuadMode::Sit;
			SetBrain(EBrain::Idle, 5.f);
		}
		StartAct(QuadAct::Startle, 0, Me);
		ThrowRiders(420.f);
	}

	AAntCharacter* Prey = PreyAnt.Get();
	if (Prey && !PreyValid(Prey))
	{
		Prey = nullptr;
		PreyAnt = nullptr;
	}

	// Noticing: the head turns to an ant long before the cat does anything about it. A still ant takes four times as long to notice.
	SenseClock += Dt;
	if (State != QuadMode::Sleep && (Brain == EBrain::Idle || Brain == EBrain::Go) && SenseClock >= 0.2f)
	{
		const float SDt = SenseClock;
		SenseClock = 0.f;
		float D = 0.f;
		AAntCharacter* Seen = Now >= NextNotice ? FindPrey(CatSee[Level], Level >= 2, &D) : nullptr;
		if (Seen)
		{
			const bool bObvious = Seen->GetVelocity().SizeSquared() > 3600.f || Seen->IsCarrying() || D < 500.f;
			NoticeLevel += SDt / ((bObvious ? CatNoticeMoving : CatNoticeStill) * (Level >= 2 ? 0.6f : 1.f));
			LookAt = Seen->GetActorLocation();
			if (NoticeLevel >= 1.f)
			{
				PreyAnt = Seen;
				Prey = Seen;
				PreySeen = Seen->GetActorLocation();
				PreySeenTime = Now;
				Mood = QuadMood::Hunting;
				Pins = 0;
				SetBrain(EBrain::Notice, CatStare);
				Seen->Toast(TEXT("Кошка тебя заметила! Замри в укрытии или уходи с глаз"));
				UE_LOG(LogTemp, Display, TEXT("ANTCAT noticed %s at %.0f (level %d)"), *Seen->GetDisplayName(), D, Level);
			}
		}
		else
		{
			NoticeLevel = FMath::Max(0.f, NoticeLevel - SDt * 0.5f);
			if (NoticeLevel <= 0.f) { LookAt = FVector::ZeroVector; }
		}
	}

	auto CalmDown = [&](float Idle, float NoNotice)
	{
		PreyAnt = nullptr;
		NoticeLevel = 0.f;
		Pins = 0;
		NextNotice = Now + NoNotice;
		Mood = QuadMood::Calm;
		LookAt = FVector::ZeroVector;
		State = QuadMode::Sit;
		SetBrain(EBrain::Idle, Idle);
	};
	auto JumpDownTowards = [&](const FVector& Towards, float Far)
	{
		FVector P = Me + Flat(Towards - Me).GetSafeNormal() * Far;
		P.Z = 60.f;
		if (!TopOf(W, P, 200.f, 300.f)) { P.Z = 0.f; }
		BeginLeap(P, false);
	};

	switch (Brain)
	{
	case EBrain::Rest:
		State = QuadMode::Sleep;
		Mood = QuadMood::Calm;
		LookAt = FVector::ZeroVector;
		if (Now >= BrainUntil)
		{
			// Waking up: the long stretch and a yawn, then a wash.
			State = QuadMode::Move;
			StartAct(QuadAct::Stretch, 0, Me);
			SetBrain(EBrain::Idle, FMath::FRandRange(6.f, 11.f));
			NextWash = Now + 2.4f;
		}
		break;

	case EBrain::Idle:
		if (Act.Kind == QuadAct::Stretch && ActBusy())
		{
			break;
		}
		State = QuadMode::Sit;
		if (Now >= NextWash && !ActBusy() && NoticeLevel <= 0.f)
		{
			NextWash = Now + FMath::FRandRange(7.f, 14.f);
			StartAct(QuadAct::Wash, 34, Me);
		}
		if (Now >= BrainUntil && !ActBusy())
		{
			if (bHuntEvent && FMath::FRand() < 0.7f)
			{
				// On the prowl: down on the floor, from room to room.
				LeapSpot = -1;
				WalkGoal = AKitchenBuilder::RandomFloorPoint();
				LeapAfter = FVector::ZeroVector;
				State = QuadMode::Move;
				SetBrain(EBrain::Go, 30.f);
			}
			else
			{
				PickNextSpot();
			}
		}
		break;

	case EBrain::Go:
	{
		State = QuadMode::Move;
		const bool bToSpot = Rests.IsValidIndex(LeapSpot);
		if (bPerched)
		{
			LeapAfter = WalkGoal;
			JumpDownTowards(WalkGoal, 850.f);
			break;
		}
		if (bToSpot && Rests[LeapSpot].bHigh && FVector::Dist2D(Me, Rests[LeapSpot].P) < 1000.f)
		{
			LeapAfter = FVector::ZeroVector;
			BeginLeap(Rests[LeapSpot].P, false);
			break;
		}
		if (WalkTo(WalkGoal, bHuntEvent ? CatTrot : CatWalk, Dt, 90.f) || Now >= BrainUntil)
		{
			if (bToSpot && !Rests[LeapSpot].bHigh)
			{
				ArriveAtSpot(LeapSpot);
			}
			else
			{
				State = QuadMode::Sit;
				SetBrain(EBrain::Idle, FMath::FRandRange(2.f, 5.f));
			}
		}
		break;
	}

	case EBrain::Notice:
		// Locked on. The head and the ears are already on you; now the body turns.
		if (!Prey)
		{
			CalmDown(3.f, 1.f);
			break;
		}
		LookAt = Prey->GetActorLocation();
		Face(LookAt, Dt, 3.f);
		if (Now >= BrainUntil && !ActBusy())
		{
			const float D = (float)FVector::Dist2D(Me, LookAt);
			if (Level == 0)
			{
				// Minding its own business: it only bats at what walks right up to it.
				if (D < CatSwipeRange && Now >= NextSwipe) { BeginSwipe(Prey); }
				else { SetBrain(EBrain::Watch, 2.5f); }
			}
			else
			{
				SetBrain(EBrain::Stalk, 25.f);
			}
		}
		break;

	case EBrain::Stalk:
	{
		if (!Prey)
		{
			CalmDown(3.f, 2.f);
			break;
		}
		const FVector PL = Prey->GetActorLocation();
		const float D = (float)FVector::Dist2D(Me, PL);
		const bool bCover = NpcWorld::UnderCover(W, PL, CatCover);
		const bool bSee = NpcWorld::CanSee(W, EyeAt, PL + FVector(0.f, 0.f, 12.f));
		if (bSee && !bCover)
		{
			PreySeen = PL;
			PreySeenTime = Now;
		}
		LookAt = PreySeen;
		Mood = QuadMood::Hunting;
		if (bCover || Now - PreySeenTime > 2.5f || PL.Z > Me.Z + 260.f)
		{
			SetBrain(EBrain::Guard, CatGuard);
			UE_LOG(LogTemp, Display, TEXT("ANTCAT lost %s (%s): waits"), *Prey->GetDisplayName(), bCover ? TEXT("under cover") : TEXT("out of sight"));
			break;
		}
		if (Now >= BrainUntil || D > (Level >= 2 ? 5200.f : 3200.f))
		{
			CalmDown(3.f, 4.f);
			break;
		}
		const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, (float)Flat(PL - Me).Rotation().Yaw));
		if (bPerched)
		{
			// Up on the furniture: it does not come down to stalk. It waits for you to walk into range underneath it.
			State = QuadMode::Stalk;
			Face(PL, Dt, 5.f);
			if (D < CatPouncePerch && D > 250.f && Now >= NextPounce && YawErr < 25.f)
			{
				BeginWiggle(Prey);
			}
			else if (Now - BrainSince > 6.f && Level >= 2)
			{
				LeapAfter = FVector::ZeroVector;
				LeapSpot = -1;
				JumpDownTowards(PL, FMath::Clamp(D * 0.6f, 600.f, 1000.f));
			}
			break;
		}
		State = QuadMode::Stalk;
		if (D < CatSwipeRange && Now >= NextSwipe)
		{
			BeginSwipe(Prey);
			break;
		}
		if (D < CatPounceMax && D > CatPounceMin && Now >= NextPounce && YawErr < 25.f)
		{
			BeginWiggle(Prey);
			break;
		}
		if (D > 330.f)
		{
			WalkTo(PL, D > 1600.f ? CatTrot : CatStalk, Dt, 300.f);
		}
		else
		{
			Face(PL, Dt, 6.f);
		}
		break;
	}

	case EBrain::Wiggle:
		Face(AirPaws, Dt, 9.f);
		LookAt = AirPaws;
		if (Now >= BrainUntil)
		{
			// Committed. The landing follows the prey only a little: a dash to the side gets you out from under it.
			FVector Final = AirPaws;
			if (Prey)
			{
				const FVector Want = Flat(Prey->GetActorLocation() + Prey->GetVelocity() * 0.15f - AirPaws);
				Final += Want.GetClampedToMaxSize(CatPounceFix);
			}
			AirPaws = Final;
			const FVector Dir = Flat(Final - Me).GetSafeNormal();
			FVector BodyTo = Final - Dir * 290.f;   // the front paws land this far ahead of the middle of the body
			BodyTo.Z = Final.Z;
			AntSfx::Everyone(this, AntSfx::Jump, Me, 0.7f);
			BeginLeap(BodyTo, true);
		}
		break;

	case EBrain::Pin:
	{
		AAntCharacter* A = HeldAnt.Get();
		if (!A || A->bDead)
		{
			HeldAnt = nullptr;
			SetBrain(EBrain::Watch, 0.8f);
			break;
		}
		LookAt = A->GetActorLocation();
		if (Now >= BrainUntil)
		{
			if (Pins >= CatPins)
			{
				// Bored of the game.
				A->SetNpcHold(nullptr, FVector::ZeroVector);
				HeldAnt = nullptr;
				A->HurtBy(CatKillDamage, TEXT("кошка наигралась"));
				UE_LOG(LogTemp, Display, TEXT("ANTCAT finished %s after %d pins"), *A->GetDisplayName(), Pins);
				CalmDown(6.f, 5.f);
				NextWash = Now + 0.6f;
				break;
			}
			ReleaseHeld(false);
			A->Toast(TEXT("Кошка убрала лапу... БЕГИ в укрытие!"));
			State = QuadMode::Sit;
			SetBrain(EBrain::Watch, CatWatch);
		}
		break;
	}

	case EBrain::Watch:
		// Sits back with its eyes on you and its tail lashing: your head start.
		if (!Prey)
		{
			CalmDown(3.f, 2.f);
			break;
		}
		LookAt = Prey->GetActorLocation();
		Face(LookAt, Dt, 2.5f);
		if (Now >= BrainUntil)
		{
			if (Level == 0 && Pins == 0)
			{
				CalmDown(4.f, 3.f);
			}
			else
			{
				if (Pins > 0) { NextPounce = FMath::Min(NextPounce, Now + 0.3f); }
				PreySeenTime = Now;
				SetBrain(EBrain::Stalk, 20.f);
			}
		}
		break;

	case EBrain::Swipe:
		Face(FVector(Act.Target), Dt, 8.f);
		LookAt = FVector(Act.Target);
		if (!bActHit && Now - BrainSince >= QuadAct::SwipeHit)
		{
			bActHit = true;
			DoSwipeHit();
		}
		if (Now >= BrainUntil)
		{
			NextSwipe = Now + CatSwipeEvery;
			SetBrain(EBrain::Watch, 0.5f);
		}
		break;

	case EBrain::Guard:
	{
		// You are somewhere it cannot get a paw: it sits and stares at the spot, then loses interest.
		Mood = QuadMood::Hunting;
		LookAt = PreySeen;
		if (!bPerched && FVector::Dist2D(Me, PreySeen) > 650.f)
		{
			State = QuadMode::Stalk;
			WalkTo(PreySeen, CatStalk, Dt, 500.f);
		}
		else
		{
			State = QuadMode::Sit;
			Face(PreySeen, Dt, 4.f);
		}
		if (Prey)
		{
			const FVector PL = Prey->GetActorLocation();
			if (!NpcWorld::UnderCover(W, PL, CatCover) && PL.Z <= Me.Z + 260.f && NpcWorld::CanSee(W, EyeAt, PL + FVector(0.f, 0.f, 12.f)))
			{
				PreySeen = PL;
				PreySeenTime = Now;
				SetBrain(EBrain::Stalk, 20.f);
				break;
			}
		}
		if (Now >= BrainUntil || !Prey)
		{
			if (Prey) { Prey->Toast(TEXT("Кошка потеряла к тебе интерес")); }
			CalmDown(3.f, 4.f);
		}
		break;
	}

	case EBrain::Flee:
	{
		if (Now - BrainSince < 0.5f)
		{
			break;   // the jump of fright itself
		}
		Mood = QuadMood::Scared;
		LookAt = FVector::ZeroVector;
		State = QuadMode::Move;
		const FRestSpot& S = Rests[RestIndex];
		if (!S.bHigh)
		{
			State = QuadMode::Fright;
			SetBrain(EBrain::Cower, CatCalm);
			break;
		}
		const float D = (float)FVector::Dist2D(Me, S.P);
		if (bPerched)
		{
			if (D < 300.f)
			{
				State = QuadMode::Fright;
				SetBrain(EBrain::Cower, CatCalm);
			}
			else
			{
				LeapAfter = S.P;
				JumpDownTowards(S.P, 850.f);
			}
			break;
		}
		if (D < 1000.f || WalkTo(S.P, CatRun, Dt, 950.f) || Now >= BrainUntil)
		{
			LeapSpot = RestIndex;
			LeapAfter = FVector::ZeroVector;
			BeginLeap(S.P, false);
		}
		break;
	}

	case EBrain::Cower:
		State = Now - BrainSince < 2.5f ? QuadMode::Fright : QuadMode::Sit;
		Mood = QuadMood::Scared;
		LookAt = ScareFrom;
		Face(ScareFrom, Dt, 3.f);
		if (Now >= BrainUntil)
		{
			CalmDown(FMath::FRandRange(4.f, 9.f), 2.f);
		}
		break;

	default:
		CalmDown(3.f, 1.f);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The dog
// ---------------------------------------------------------------------------------------------

void ACritter::Zoomies(float Seconds)
{
	if (HasAuthority() && Kind == 0 && bNewBody)
	{
		ZoomUntil = GetWorld()->GetTimeSeconds() + Seconds;
		UE_LOG(LogTemp, Display, TEXT("ANTDOG zoomies for %.0f s"), Seconds);
	}
}

void ACritter::BeginBark(const FVector& At)
{
	const float Now = GetWorld()->GetTimeSeconds();
	NextBark = Now + DogBarkEvery;
	ScareFrom = At;
	BarksLeft = 3;
	if (State == QuadMode::Sleep || State == QuadMode::NoseDown) { State = QuadMode::Move; }
	StartAct(QuadAct::Bark, 3, At);
	SetBrain(EBrain::Bark, 3.f * QuadAct::BarkEvery + 0.25f);
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		// The barking is a noise like any other: the Human comes to see what the dog has found.
		GM->MakeNoise(GetActorLocation(), DogBarkLoud, AntNoise::Bark, this);
		GM->Broadcast(TEXT("Бадрик лает! Человек услышит"), FLinearColor(0.95f, 0.75f, 0.4f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTDOG barks at %s"), *At.ToCompactString());
}

AAntCharacter* ACritter::FindLick(const FVector& Nose, float Range, bool bBlocked) const
{
	const UWorld* W = GetWorld();
	const float MyZ = (float)GetActorLocation().Z;
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
		const float D = (float)FVector::Dist2D(L, Nose);
		if (D >= BestD || L.Z < MyZ - 200.f)
		{
			continue;
		}
		const UAntMovementComponent* M = A->GetAntMovement();
		if (M && M->GetRiddenComponent() && M->GetRiddenComponent()->GetOwner() == this)
		{
			continue;
		}
		// What his tongue can reach: on the floor, with nothing low over it. What it cannot: under furniture, or up a wall or a table leg.
		const bool bReach = L.Z < MyZ + DogLickHigh && !NpcWorld::UnderCover(W, L, DogCover);
		if (bBlocked ? (!bReach && L.Z < MyZ + 1100.f) : bReach)
		{
			BestD = D;
			Best = A;
		}
	}
	return Best;
}

void ACritter::DoLick()
{
	UWorld* W = GetWorld();
	const FVector P(Act.Target);
	AAntCharacter* Got = nullptr;
	float BestD = DogLickRadius;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->IsAttached() || FMath::Abs(A->GetActorLocation().Z - P.Z) > 170.f)
		{
			continue;
		}
		const float D = (float)FVector::Dist2D(A->GetActorLocation(), P);
		if (D < BestD) { BestD = D; Got = A; }
	}
	AAntArmy::KillInSphere(W, P, DogLickRadius + 20.f, 25);
	AntSfx::Everyone(this, AntSfx::Splash, P, 1.7f);
	if (!Got)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTDOG lick at %s: missed"), *P.ToCompactString());
		return;
	}
	if (Got->IsCarrying()) { Got->LetGo(); }
	if (Got->Health <= DogLickDamage)
	{
		// Licked up: it rides the tongue back into the mouth.
		HeldAnt = Got;
		Grip = 3;
		Got->SetNpcHold(Segs[QuadSeg::Tongue], FVector(QuadBody::TongueLength(Kind) * 0.9f, 0.f, 8.f) / QuadBody::Scale);
		Got->Toast(TEXT("Бадрик слизнул тебя! [Пробел] — вырываться!"));
	}
	else
	{
		Got->HurtBy(DogLickDamage, TEXT("облизан Бадриком"));
		if (!Got->bDead)
		{
			Got->Stagger(DogStagger, (Flat(Got->GetActorLocation() - GetActorLocation()).GetSafeNormal() * 260.f + FVector(0.f, 0.f, 120.f)) * AntKit::SpeedK);
			Got->Toast(TEXT("Бадрик лизнул тебя — весь в слюнях! Ещё раз, и проглотит. Под мебель или на стену!"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTDOG lick at %s: %s (%s)"), *P.ToCompactString(), *Got->GetDisplayName(), HeldAnt.IsValid() ? TEXT("on the tongue") : TEXT("slobbered"));
}

void ACritter::BaitWith(AKitchenLoot* Food)
{
	if (Food) { LeftFood.Add(Food, GetWorld()->GetTimeSeconds() - 10.f); }
}

AKitchenLoot* ACritter::FindLeftFood(const FVector& Nose, float Range, float Now) const
{
	const UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	const int32 Room = AKitchenBuilder::Region(Me);
	AKitchenLoot* Best = nullptr;
	float BestD = Range;
	const bool bAtBowl = Brain == EBrain::Idle && Rests.IsValidIndex(RestIndex) && Rests[RestIndex].Do == 1;
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear || L->bVault || L->bTrophy || L->bBody || L->bHeld || L->IsCarried())
		{
			continue;
		}
		const FVector P = L->GetActorLocation();
		const float D = (float)FVector::Dist2D(P, Nose);
		if (D >= BestD || P.Z > Me.Z + 180.f || P.Z < Me.Z - 100.f || NpcWorld::UnderCover(W, P, DogCover))
		{
			continue;
		}
		// Food the ants were hauling and let go of (and anything at all lying right by his bowl, the bin or the flour).
		const float* Left = LeftFood.Find(L);
		const bool bLeft = Left && Now - *Left > DogFoodLeft && (AKitchenBuilder::Region(P) == Room || Range > 3000.f);
		if (bLeft || (bAtBowl && D < 450.f))
		{
			BestD = D;
			Best = L;
		}
	}
	return Best;
}

void ACritter::DogThink(float Dt, float Now)
{
	UWorld* W = GetWorld();
	const FVector Me = GetActorLocation();
	AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>();
	const int32 RawLevel = NpcWorld::Level(W, NpcWho::Dog);
	const int32 Level = FMath::Clamp(RawLevel, 1, 2);
	// Level 0 (a weekday raid while the Human is on his phone): he dozes. Dropped food and his ball still get him up, ant trails do not.
	const bool bNoNose = RawLevel <= 0;
	const FVector Nose = GetNose();
	NpcWorld::ScentTick(W, Dt);
	if (!Ball.IsValid() && !bBallMade)
	{
		// His ball: push it (a dash into it) and he is off after it.
		bBallMade = true;
		const FTransform TM(Rests[0].P + FVector(0.f, 750.f, 90.f));   // on the open floor north of his mat (not in a bowl)
		if (AKitchenProp* P = W->SpawnActorDeferred<AKitchenProp>(AKitchenProp::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			P->ShapeIndex = (uint8)EAntShape::Sphere;
			P->Size = FVector(130.f);
			P->Color = FLinearColor(0.75f, 0.9f, 0.2f);
			P->Mass = 1.f;
			P->PropName = TEXT("Мячик Бадрика");
			P->FinishSpawning(TM);
			Ball = P;
		}
	}
	const bool bFree = Brain == EBrain::Rest || Brain == EBrain::Idle || Brain == EBrain::Go || Brain == EBrain::Track;
	if (bFree)
	{
		CheckFrights(Now);   // a crash, the toaster, the vacuum, something falling next to him: he barks at it
	}

	// Food the ants have been hauling: he remembers what was in somebody's jaws.
	FoodClock += Dt;
	if (FoodClock > 0.5f)
	{
		FoodClock = 0.f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->IsCarried() && !It->bIsGear) { LeftFood.Add(*It, Now); }
		}
		for (auto It = LeftFood.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) { It.RemoveCurrent(); }
		}
	}

	// Ants on his back: a good shake.
	RiderTime = CountRiders() > 0 ? RiderTime + Dt : 0.f;
	if (RiderTime > 2.f && Now >= NextShake && !ActBusy() && Brain != EBrain::Lick && Brain != EBrain::Tell)
	{
		NextShake = Now + 6.f;
		RiderTime = 0.f;
		if (State == QuadMode::Sleep) { State = QuadMode::Move; SetBrain(EBrain::Idle, 4.f); }
		StartAct(QuadAct::Shake, 0, Me);
		bShook = false;
	}
	if (Act.Kind == QuadAct::Shake && !bShook && ActT() > 0.4f)
	{
		bShook = true;
		ThrowRiders(520.f);
	}

	SenseClock += Dt;
	const bool bSense = SenseClock >= 0.3f;
	if (bSense) { SenseClock = 0.f; }
	const bool bAsleep = State == QuadMode::Sleep;
	auto WakeUp = [&]() { if (State == QuadMode::Sleep) { State = QuadMode::Move; } };

	if (ZoomUntil > Now && bFree)
	{
		// It has come over him.
		WakeUp();
		Mood = QuadMood::Happy;
		LookAt = FVector::ZeroVector;
		StartAct(QuadAct::Bark, 2, FVector::ZeroVector);
		AntSfx::Everyone(this, AntSfx::Swat, Me + FVector(0.f, 0.f, 300.f), 0.7f);
		WalkGoal = AKitchenBuilder::RandomFloorPoint();
		SetBrain(EBrain::Zoom, ZoomUntil - Now);
	}
	else if (bSense && bFree && Mood != QuadMood::Sulking)
	{
		AKitchenProp* B = Ball.Get();
		if (AKitchenLoot* Food = FindLeftFood(Nose, DogFoodSmell[Level] * (bAsleep ? 0.6f : 1.f), Now))
		{
			// Food before everything: a dropped crumb buys you time.
			WakeUp();
			Interest = Food;
			Mood = QuadMood::Happy;
			bEating = false;
			SetBrain(EBrain::Eat, 30.f);
			UE_LOG(LogTemp, Display, TEXT("ANTDOG smells food left lying: %s at %s"), *Food->LootName, *Food->GetActorLocation().ToCompactString());
		}
		else if (B && B->GetVelocity().Size2D() > 140.f && FVector::Dist2D(B->GetActorLocation(), Me) < DogBallSee && AKitchenBuilder::Region(B->GetActorLocation()) == AKitchenBuilder::Region(Me))
		{
			WakeUp();
			BallNudges = 0;
			Mood = QuadMood::Happy;
			SetBrain(EBrain::Ball, 12.f);
			UE_LOG(LogTemp, Display, TEXT("ANTDOG after the ball"));
		}
		else if (Brain != EBrain::Track && !bNoNose)
		{
			FVector P;
			float S = 0.f;
			if (NpcWorld::BestScent(W, Nose, DogSmell[Level] * (bAsleep ? 0.5f : 1.f), bAsleep ? 0.7f : DogSmellMin, P, S))
			{
				// His nose has found where ants have been walking.
				WakeUp();
				Mood = QuadMood::Hunting;
				WalkGoal = P;
				NoScentSince = Now;
				CantReachSince = Now;
				SetBrain(EBrain::Track, 45.f);
				UE_LOG(LogTemp, Display, TEXT("ANTDOG picked up a scent (%.2f) at %s, %.0f from his nose"), S, *P.ToCompactString(), FVector::Dist2D(P, Nose));
			}
		}
	}

	switch (Brain)
	{
	case EBrain::Rest:
		State = QuadMode::Sleep;
		LookAt = FVector::ZeroVector;
		if (Now >= BrainUntil)
		{
			// Up, a shake, and now and then a mad minute.
			Mood = QuadMood::Calm;
			State = QuadMode::Move;
			StartAct(QuadAct::Shake, 0, Me);
			bShook = true;
			SetBrain(EBrain::Idle, FMath::FRandRange(3.f, 6.f));
			if (FMath::FRand() < 0.12f) { ZoomUntil = Now + 8.f; }
		}
		break;

	case EBrain::Idle:
	{
		if (ActBusy() && (Act.Kind == QuadAct::Shake || Act.Kind == QuadAct::Yelp || Act.Kind == QuadAct::Bark))
		{
			State = QuadMode::Move;
			break;
		}
		const uint8 Do = Rests.IsValidIndex(RestIndex) && FVector::Dist2D(Me, Rests[RestIndex].P) < 300.f ? Rests[RestIndex].Do : 2;
		if (Do == 1)
		{
			State = QuadMode::NoseDown;
			if (Mood != QuadMood::Sulking) { Mood = QuadMood::Calm; }
		}
		else
		{
			State = QuadMode::Sit;
			if (Do == 2 && Rests.IsValidIndex(RestIndex) && Rests[RestIndex].Rt < 0 && FVector::Dist2D(Me, Rests[RestIndex].P) < 300.f)
			{
				// By the sofa: begging from whoever sits on it with a snack.
				const FVector Kid = ArtAt(TEXT("SM_L_Sofa_SeatCushion"), 1, FVector(13500.f, 370.f, 700.f)) + FVector(0.f, -200.f, 700.f);
				Mood = QuadMood::Happy;
				LookAt = Kid;
				Face(Kid, Dt, 3.f);
				if (!ActBusy()) { StartAct(QuadAct::Beg, 40, Kid); }
			}
		}
		if (Now >= BrainUntil && !(ActBusy() && Act.Kind != QuadAct::Beg))
		{
			if (Mood != QuadMood::Sulking) { Mood = QuadMood::Calm; }
			LookAt = FVector::ZeroVector;
			PickNextSpot();
		}
		break;
	}

	case EBrain::Go:
		State = QuadMode::Move;
		LookAt = FVector::ZeroVector;
		if (WalkTo(WalkGoal, DogWalk, Dt, 90.f) || Now >= BrainUntil)
		{
			if (Rests.IsValidIndex(LeapSpot) && Rests[LeapSpot].Rt >= 0 && FVector::Dist2D(Me, WalkGoal) > 1200.f)
			{
				// Level v3: did not get to its routine spot: sits where it is a moment and tries again (not "there" in the wrong place).
				State = QuadMode::Sit;
				SetBrain(EBrain::Idle, 4.f);
				break;
			}
			ArriveAtSpot(Rests.IsValidIndex(LeapSpot) ? LeapSpot : RestIndex);
		}
		break;

	case EBrain::Track:
	{
		State = QuadMode::NoseDown;
		Mood = QuadMood::Hunting;
		// An ant right under his nose?
		if (AAntCharacter* A = FindLick(Nose, DogSeeAnt, false))
		{
			const FVector AL = A->GetActorLocation();
			const float D = (float)FVector::Dist2D(Nose, AL);
			const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees((float)GetActorRotation().Yaw, (float)Flat(AL - Me).Rotation().Yaw));
			PreyAnt = A;
			LookAt = AL;
			NoScentSince = Now;
			CantReachSince = Now;
			if (D < DogLickRange && YawErr < 40.f && Now >= NextLick)
			{
				// The tell: the nose stops dead over you and twitches.
				StartAct(QuadAct::Twitch, 0, AL - FVector(0.f, 0.f, AAntCharacter::BodyRadius));
				SetBrain(EBrain::Tell, QuadAct::TwitchTime);
				A->Toast(TEXT("Бадрик принюхался... сейчас слизнёт! В сторону!"));
				UE_LOG(LogTemp, Display, TEXT("ANTDOG nose-twitch over %s (%.0f from the nose)"), *A->GetDisplayName(), D);
			}
			else if (D > DogLickRange * 0.7f || YawErr > 30.f)
			{
				if (WalkTo(AL, DogSniff + 70.f, Dt, 520.f)) { Face(AL, Dt, 5.f); }
			}
			break;
		}
		// One he can smell but cannot get at (under something, up a wall): he barks at it, and the Human comes to look.
		if (AAntCharacter* Up = FindLick(Nose, 800.f, true))
		{
			LookAt = Up->GetActorLocation();
			Face(LookAt, Dt, 4.f);
			State = QuadMode::Move;
			NoScentSince = Now;
			if (Now - CantReachSince > DogBarkAfter && Now >= NextBark)
			{
				Up->Toast(TEXT("Бадрик учуял тебя и лает — сейчас придёт Человек!"));
				BeginBark(LookAt);
			}
			break;
		}
		CantReachSince = Now;
		LookAt = FVector::ZeroVector;
		// Following the trail: to the strongest smell within reach of his nose; what he has sniffed over is sniffed out.
		if (FVector::Dist2D(Nose, WalkGoal) < 190.f || FVector::Dist2D(Me, WalkGoal) < 540.f)
		{
			NpcWorld::ScentClear(W, WalkGoal, 230.f);
			FVector P;
			float S = 0.f;
			if (NpcWorld::BestScent(W, Nose, DogSmell[Level], DogSmellMin, P, S))
			{
				WalkGoal = P;
				NoScentSince = Now;
			}
		}
		if (Now - NoScentSince > 3.5f || Now >= BrainUntil || bNoNose)
		{
			// Lost it (or dozing off again: the house is quiet).
			Mood = QuadMood::Calm;
			State = QuadMode::Sit;
			PreyAnt = nullptr;
			SetBrain(EBrain::Idle, 3.f);
			UE_LOG(LogTemp, Display, TEXT("ANTDOG lost the scent"));
			break;
		}
		if (WalkTo(WalkGoal, DogSniff, Dt, 520.f)) { Face(WalkGoal, Dt, 4.f); }
		break;
	}

	case EBrain::Tell:
		Face(FVector(Act.Target), Dt, 6.f);
		LookAt = FVector(Act.Target);
		if (Now >= BrainUntil)
		{
			// The tongue goes where the ant was when the nose stopped, give or take a little: a step aside is enough.
			FVector P(Act.Target);
			if (const AAntCharacter* A = PreyAnt.Get())
			{
				P += Flat(A->GetActorLocation() - P).GetClampedToMaxSize(DogLickFix);
			}
			StartAct(QuadAct::Lick, 0, P);
			SetBrain(EBrain::Lick, 0.5f);
		}
		break;

	case EBrain::Lick:
		LookAt = FVector(Act.Target);
		if (!bActHit && ActT() >= QuadAct::LickHit)
		{
			bActHit = true;
			DoLick();
		}
		if (Now >= BrainUntil)
		{
			if (AAntCharacter* A = HeldAnt.Get())
			{
				A->SetNpcHold(nullptr, FVector::ZeroVector);
				A->HurtBy(100.f, TEXT("слизан Бадриком"));
				UE_LOG(LogTemp, Display, TEXT("ANTDOG swallowed %s"), *A->GetDisplayName());
			}
			HeldAnt = nullptr;
			NextLick = Now + DogLickEvery;
			NoScentSince = Now;
			SetBrain(EBrain::Track, 25.f);
		}
		break;

	case EBrain::Eat:
	{
		AKitchenLoot* Food = Cast<AKitchenLoot>(Interest.Get());
		if (!Food || Food->IsCarried() || Food->bHeld)
		{
			// Gone, or snatched back from under his nose.
			if (Act.Kind == QuadAct::Eat) { Act.Start = -100.f; }
			Interest = nullptr;
			Mood = QuadMood::Calm;
			State = QuadMode::Sit;
			SetBrain(EBrain::Idle, 2.f);
			UE_LOG(LogTemp, Display, TEXT("ANTDOG: the food is gone"));
			break;
		}
		const FVector FL = Food->GetActorLocation();
		LookAt = FL;
		State = QuadMode::Move;
		if (bEating)
		{
			Face(FL, Dt, 4.f);
			if (!ActBusy())
			{
				if (FVector::Dist2D(Nose, FL) < 500.f)
				{
					if (GM && Food->Value >= 20) { GM->Broadcast(FString::Printf(TEXT("Бадрик сожрал: %s"), *Food->LootName), FLinearColor(0.9f, 0.7f, 0.5f)); }
					UE_LOG(LogTemp, Display, TEXT("ANTDOG ate %s"), *Food->LootName);
					LeftFood.Remove(Food);
					Food->ReleaseAll();
					Food->Destroy();
					AntSfx::Everyone(this, AntSfx::Crunch, Nose, 0.8f);
				}
				Interest = nullptr;
				bEating = false;
				State = QuadMode::Sit;
				SetBrain(EBrain::Idle, 3.f);
			}
		}
		else if (FVector::Dist2D(Nose, FL) < 250.f)
		{
			// Chewing takes a while: time enough to grab it back, or to bite him.
			StartAct(QuadAct::Eat, (uint8)FMath::RoundToInt(DogEatTime * 10.f), FL);
			bEating = true;
			AntSfx::Everyone(this, AntSfx::Crunch, Nose, 0.7f);
		}
		else if (WalkTo(FL, DogTrot, Dt, 540.f) || Now >= BrainUntil)
		{
			Face(FL, Dt, 5.f);
			if (Now >= BrainUntil) { Interest = nullptr; }
		}
		break;
	}

	case EBrain::Bark:
		Face(ScareFrom, Dt, 5.f);
		LookAt = ScareFrom;
		if (BarksLeft > 0 && ActT() >= (3 - BarksLeft) * QuadAct::BarkEvery + 0.05f)
		{
			--BarksLeft;
			AntSfx::Everyone(this, AntSfx::Swat, Nose, 0.62f);
		}
		if (Now >= BrainUntil)
		{
			LookAt = FVector::ZeroVector;
			NoScentSince = Now;
			CantReachSince = Now;
			if (Mood == QuadMood::Hunting) { SetBrain(EBrain::Track, 20.f); }
			else { State = QuadMode::Sit; SetBrain(EBrain::Idle, 2.5f); }
		}
		break;

	case EBrain::Ball:
	{
		AKitchenProp* B = Ball.Get();
		if (!B || Now >= BrainUntil || BallNudges >= DogBallNudges)
		{
			State = QuadMode::Sit;
			Mood = QuadMood::Happy;
			LookAt = B ? B->GetActorLocation() : FVector::ZeroVector;
			SetBrain(EBrain::Idle, 4.f);
			break;
		}
		const FVector BL = B->GetActorLocation();
		LookAt = BL;
		State = QuadMode::Move;
		if (FVector::Dist2D(Nose, BL) < 260.f && Now >= NextNudge)
		{
			// Got it... and off it goes again.
			NextNudge = Now + 0.9f;
			++BallNudges;
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(B->GetRootComponent()))
			{
				Root->AddImpulse(Flat(BL - Me).GetSafeNormal() * 950.f + FVector(0.f, 0.f, 260.f), NAME_None, true);
			}
		}
		else if (WalkTo(BL, DogRun, Dt, 500.f))
		{
			Face(BL, Dt, 6.f);
		}
		break;
	}

	case EBrain::Zoom:
		State = QuadMode::Move;
		Mood = QuadMood::Happy;
		if (Now >= ZoomUntil)
		{
			ZoomUntil = 0.f;
			State = QuadMode::Sit;
			StartAct(QuadAct::Shake, 0, Me);
			bShook = true;
			SetBrain(EBrain::Idle, 6.f);
			break;
		}
		if (ActBusy() && Act.Kind == QuadAct::Bark)
		{
			break;   // the two barks that start it: the warning
		}
		if (WalkTo(WalkGoal, DogRun, Dt, 350.f))
		{
			WalkGoal = AKitchenBuilder::RandomFloorPoint();
		}
		break;

	case EBrain::Watch:
		// After a bite: looks round for what did that, shakes himself.
		State = QuadMode::Move;
		if (!ActBusy() && !bShook)
		{
			StartAct(QuadAct::Shake, 0, Me);
			bShook = true;
		}
		if (Now >= BrainUntil)
		{
			State = QuadMode::Sit;
			SetBrain(EBrain::Idle, 2.f);
		}
		break;

	case EBrain::Sulk:
		Mood = QuadMood::Sulking;
		State = QuadMode::Move;
		LookAt = FVector::ZeroVector;
		if (ActBusy() && Act.Kind == QuadAct::Yelp)
		{
			break;
		}
		if (WalkTo(Rests[RestIndex].P, DogTrot * 1.4f, Dt, 120.f) || Now >= BrainUntil)
		{
			State = QuadMode::Sleep;
			SetBrain(EBrain::Rest, 40.f);
		}
		break;

	default:
		State = QuadMode::Sit;
		SetBrain(EBrain::Idle, 3.f);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// Dev: puppets for screenshots (-AntDo)
// ---------------------------------------------------------------------------------------------

void ACritter::DevPlace(const FVector& P, float Yaw)
{
	SetActorLocationAndRotation(P, FRotator(0.f, Yaw, 0.f));
	DevWalk = FVector::ZeroVector;
	for (FNpcFoot& F : Rig.Feet) { F.bInit = false; }
}

void ACritter::DevPrey(AAntCharacter* Ant)
{
	if (!Ant || !bNewBody)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (State == QuadMode::Sleep) { State = QuadMode::Sit; }
	PreyAnt = Ant;
	NoticeLevel = 1.f;
	PreySeen = Ant->GetActorLocation();
	PreySeenTime = Now;
	Mood = QuadMood::Hunting;
	Pins = 0;
	NextPounce = NextSwipe = NextLick = 0.f;
	WalkGoal = PreySeen;
	NoScentSince = CantReachSince = Now;
	SetBrain(Kind == 1 ? EBrain::Notice : EBrain::Track, Kind == 1 ? CatStare : 20.f);
}

void ACritter::DevAct(const FString& What, const FVector& Target)
{
	if (!HasAuthority() || !bNewBody)
	{
		return;
	}
	const FVector Me = GetActorLocation();
	static const TCHAR* Modes[] = { TEXT("sleep"), TEXT("stand"), TEXT("nose"), TEXT("sit"), TEXT("air"), TEXT("stalk"), TEXT("fright") };
	static const TCHAR* Moods[] = { TEXT("calm"), TEXT("hunt"), TEXT("scared"), TEXT("happy"), TEXT("sulk") };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Modes); ++i)
	{
		if (What == Modes[i]) { State = (uint8)i; Act = FNpcActState(); return; }
	}
	for (int32 i = 0; i < UE_ARRAY_COUNT(Moods); ++i)
	{
		if (What == Moods[i]) { Mood = (uint8)i; return; }
	}
	if (What == TEXT("look")) { LookAt = Target; }
	else if (What == TEXT("nolook")) { LookAt = FVector::ZeroVector; }
	else if (What == TEXT("stretch")) { StartAct(QuadAct::Stretch, 0, Target); }
	else if (What == TEXT("wash")) { State = QuadMode::Sit; StartAct(QuadAct::Wash, 60, Target); }
	else if (What == TEXT("wiggle")) { State = QuadMode::Stalk; StartAct(QuadAct::Wiggle, 0, Target); }
	else if (What == TEXT("pounce"))
	{
		AirPaws = Target;
		FVector To = Target - Flat(Target - Me).GetSafeNormal() * 290.f;
		To.Z = Target.Z;
		BeginLeap(To, true);
	}
	else if (What == TEXT("jump")) { LeapSpot = -1; LeapAfter = FVector::ZeroVector; BeginLeap(Target, false); }
	else if (What == TEXT("swipe0") || What == TEXT("swipe1")) { StartAct(QuadAct::Swipe, What.EndsWith(TEXT("1")) ? 1 : 0, Target); Brain = EBrain::Swipe; }
	else if (What == TEXT("pin")) { StartAct(QuadAct::Pin, 30, Target); }
	else if (What == TEXT("startle")) { StartAct(QuadAct::Startle, 0, Target); }
	else if (What == TEXT("twitch")) { StartAct(QuadAct::Twitch, 0, Target); }
	else if (What == TEXT("lick")) { StartAct(QuadAct::Lick, 0, Target); }
	else if (What == TEXT("eat")) { StartAct(QuadAct::Eat, 40, Target); }
	else if (What == TEXT("bark")) { StartAct(QuadAct::Bark, 3, Target); }
	else if (What == TEXT("shake")) { StartAct(QuadAct::Shake, 0, Target); }
	else if (What == TEXT("beg")) { State = QuadMode::Sit; StartAct(QuadAct::Beg, 50, Target); }
	else if (What == TEXT("yelp")) { StartAct(QuadAct::Yelp, 0, Target); }
	else { UE_LOG(LogTemp, Warning, TEXT("ANTNPC unknown dev act: %s"), *What); }
}

void ACritter::DevPoke(const FString& What)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	const FVector Me = GetActorLocation();
	if (What == TEXT("food"))
	{
		// The nearest loose food is put on the floor ahead of him, as if an ant had just dropped it there.
		AKitchenLoot* Best = nullptr;
		float BestD = 1.e9f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			const float D = (float)FVector::Dist(It->GetActorLocation(), Me);
			if (!It->bIsGear && !It->bVault && !It->bTrophy && !It->bBody && !It->bHeld && !It->IsCarried() && It->Weight <= 8 && D < BestD) { BestD = D; Best = *It; }
		}
		if (Best)
		{
			const FVector At = Me + GetActorForwardVector() * 1500.f + FVector(0.f, 0.f, 60.f);
			Best->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			LeftFood.Add(Best, Now - 10.f);
			UE_LOG(LogTemp, Display, TEXT("ANTDOG dev: «%s» left lying at %s"), *Best->LootName, *At.ToCompactString());
		}
	}
	else if (What == TEXT("ball"))
	{
		if (AKitchenProp* B = Ball.Get())
		{
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(B->GetRootComponent()))
			{
				Root->AddImpulse(GetActorRightVector() * 900.f + FVector(0.f, 0.f, 200.f), NAME_None, true);
				UE_LOG(LogTemp, Display, TEXT("ANTDOG dev: the ball is pushed from %s"), *B->GetActorLocation().ToCompactString());
			}
		}
	}
}

FString ACritter::DevDescribe() const
{
	const FQuadRig& R = Rig;
	const FTransform B(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorLocation());
	FString Paws;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector P = B.InverseTransformPosition(R.Feet[i].Pos);
		Paws += FString::Printf(TEXT(" paw%d (%.0f,%.0f) lift %.0f"), i, P.X, P.Y, R.Feet[i].Lift);
	}
	return FString::Printf(TEXT("%s at %s yaw %.0f jointed=%d mode=%d mood=%d act=%d t=%.2f brain=%d level=%d speed=%.0f | hipZ %.0f shZ %.0f curl %.0f | head yaw %.0f pitch %.0f jaw %.0f | tail lift %.0f puff %.2f |%s | riders %d prey %s held %s notice %.2f"),
		Kind == 0 ? TEXT("dog") : TEXT("cat"), *GetActorLocation().ToCompactString(), GetActorRotation().Yaw, bNewBody ? 1 : 0, (int32)State, (int32)Mood, (int32)Act.Kind, ActT(), (int32)Brain,
		NpcWorld::Level(GetWorld(), Kind == 0 ? NpcWho::Dog : NpcWho::Cat), PoseVel.Size2D(),
		R.HipZ, R.ShZ, R.Curl, R.HeadYaw, R.HeadPitch, R.Jaw, R.TailLift, R.TailPuff, *Paws, CountRiders(),
		PreyAnt.IsValid() ? *PreyAnt->GetDisplayName() : TEXT("-"), HeldAnt.IsValid() ? *HeldAnt->GetDisplayName() : TEXT("-"), NoticeLevel);
}

// ---------------------------------------------------------------------------------------------
// Hooks from the ant and from the dev script
// ---------------------------------------------------------------------------------------------

namespace NpcWorld
{

bool AntBites(AAntCharacter* Ant, float Reach)
{
	if (!Ant || !Ant->HasAuthority())
	{
		return false;
	}
	if (FolkWorld::BiteHolder(Ant))
	{
		return true;   // in the kid's fingers: the finger is right there
	}
	bool bHit = false;
	const FVector Me = Ant->GetActorLocation();
	const UAntMovementComponent* MM = Ant->GetAntMovement();
	const UPrimitiveComponent* Ridden = MM ? MM->GetRiddenComponent() : nullptr;
	const AActor* Holder = Ant->NpcHold ? Ant->NpcHold->GetOwner() : nullptr;
	for (TActorIterator<ACritter> It(Ant->GetWorld()); It; ++It)
	{
		ACritter* C = *It;
		bool bTouch = (Ridden && Ridden->GetOwner() == C) || Holder == C;
		if (!bTouch && C->HasJointedBody())
		{
			// The real body: whichever segment is within reach of the jaws.
			for (int32 i = 0; i < QuadSeg::Count && !bTouch; ++i)
			{
				const UStaticMeshComponent* S = C->GetSegment(i);
				// (a triangle-mesh body cannot be asked for its closest point: the segment's box is close enough for a pair of jaws)
				bTouch = S && S->GetStaticMesh() && S->GetCollisionEnabled() != ECollisionEnabled::NoCollision
					&& S->Bounds.GetBox().ComputeSquaredDistanceToPoint(Me) < FMath::Square(Reach * 0.6f);
			}
		}
		else if (!bTouch)
		{
			bTouch = FVector::Dist(C->GetActorLocation(), Me) < C->BodyLength() * 0.7f + Reach;
		}
		if (bTouch)
		{
			C->Bitten(Ant);
			Ant->Toast(FString::Printf(TEXT("Укусил: %s"), *C->DisplayName()));
			bHit = true;
		}
	}
	return bHit;
}

void Struggle(AAntCharacter* Ant)
{
	if (FolkWorld::Struggle(Ant))
	{
		return;
	}
	AActor* Holder = (Ant && Ant->NpcHold) ? Ant->NpcHold->GetOwner() : nullptr;
	if (ACritter* C = Cast<ACritter>(Holder))
	{
		C->Struggle(Ant);
	}
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0)
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	const FString Op = A[0].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (Op == TEXT("npclevel"))
	{
		ForceLevel((int32)Num(1, -1.f));
		return true;
	}
	if (FolkWorld::DevCommand(GM, A))
	{
		return true;
	}
	if (Op == TEXT("cat") || Op == TEXT("dog"))
	{
		ACritter* C = nullptr;
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == (Op == TEXT("cat") ? 1 : 0)) { C = *It; }
		}
		if (!C || A.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTNPC no %s / no command"), *Op);
			return true;
		}
		const FString Sub = A[1].ToLower();
		const FTransform B(FRotator(0.f, C->GetActorRotation().Yaw, 0.f), C->GetActorLocation());
		if (Sub == TEXT("puppet")) { C->bDevPuppet = Num(2, 1.f) > 0.5f; C->DevWalk = FVector::ZeroVector; }
		else if (Sub == TEXT("to")) { C->DevPlace(Vec(2), Num(5)); }                                           // cat to x y z yaw
		else if (Sub == TEXT("walk")) { C->DevWalk = FVector(Num(2), Num(3), C->GetActorLocation().Z); C->DevWalkSpeed = Num(4, 500.f); C->State = QuadMode::Move; }   // cat walk x y [speed]
		else if (Sub == TEXT("act") && A.Num() >= 3)
		{
			// cat act <what> [dx dy dz]: the point is in the creature's own frame (dx ahead of it, dy to its right); without one: the player's ant.
			C->DevAct(A[2].ToLower(), A.Num() >= 6 ? B.TransformPosition(Vec(3)) : (Me ? Me->GetActorLocation() - FVector(0.f, 0.f, AAntCharacter::BodyRadius) : B.TransformPosition(FVector(600.f, 0.f, 0.f))));
		}
		else if (Sub == TEXT("freeze")) { C->DevFreezeAt = Num(2, -1.f); }
		else if (Sub == TEXT("info")) { UE_LOG(LogTemp, Display, TEXT("ANTNPC %s"), *C->DevDescribe()); }
		else if (Sub == TEXT("hunt")) { C->Hunt(Num(2, 40.f)); }
		else if (Sub == TEXT("scare")) { C->Scare(A.Num() >= 5 ? Vec(2) : B.TransformPosition(FVector(800.f, 0.f, 0.f)), TEXT("испытаний")); }
		else if (Sub == TEXT("zoom")) { C->Zoomies(Num(2, 12.f)); }
		else if (Sub == TEXT("prey") && Me) { C->DevPrey(Me); }
		else if (Sub == TEXT("food") || Sub == TEXT("ball")) { C->DevPoke(Sub); }
		else if (Sub == TEXT("bite") && Me) { C->Bitten(Me); }
		else if (Sub == TEXT("me") && Me)
		{
			// cat me dx dy [dz]: puts the player's ant at that point of the creature's frame.
			Me->ApplyTeleport(B.TransformPosition(FVector(Num(2, 600.f), Num(3), Num(4, 20.f))));
		}
		else if (Sub == TEXT("ride") && Me)
		{
			// cat ride seg: drops the player's ant onto a segment from above.
			if (const UStaticMeshComponent* S = C->GetSegment((int32)Num(2, (float)QuadSeg::Belly)))
			{
				Me->ApplyTeleport(S->Bounds.Origin + FVector(0.f, 0.f, S->Bounds.BoxExtent.Z + 30.f));
			}
		}
		else if (Sub == TEXT("cam") && PC)
		{
			// cat cam dx dy dz lookZ [fov] [lookX]: a camera at a point of the creature's frame, looking at it lookZ above its floor.
			static TWeakObjectPtr<ACameraActor> DevCam;
			if (!DevCam.IsValid())
			{
				DevCam = W->SpawnActor<ACameraActor>();
			}
			if (ACameraActor* Cam = DevCam.Get())
			{
				const FVector From = B.TransformPosition(Vec(2));
				const FVector To = B.TransformPosition(FVector(Num(7, 0.f), 0.f, Num(5, 250.f)));
				Cam->SetActorLocationAndRotation(From, (To - From).Rotation());
				Cam->GetCameraComponent()->SetFieldOfView(Num(6, 60.f));
				Cam->GetCameraComponent()->bConstrainAspectRatio = false;
				PC->SetViewTargetWithBlend(Cam, 0.f);
			}
		}
		else { UE_LOG(LogTemp, Warning, TEXT("ANTNPC unknown: %s %s"), *Op, *Sub); }
		return true;
	}
	return false;
}

} // namespace NpcWorld
