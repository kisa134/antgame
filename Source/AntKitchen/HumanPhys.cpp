#include "HumanCharacter.h"
#include "HumanBody.h"
#include "AntWeapon.h"
#include "AntWreck.h"
#include "AntVersus.h"
#include "AntFx.h"
#include "AntFilm.h"
#include "AntTypes.h"
#include "AntCharacter.h"
#include "AntSwarm.h"
#include "AntKitchenGameMode.h"
#include "AntPlayerController.h"
#include "KitchenLoot.h"
#include "KitchenFixture.h"
#include "KitchenInteractables.h"
#include "Critters.h"
#include "KitchenArt.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

// =============================================================================================
// The heavy body (docs/FEATURES.md, section 28): the Human is a 90 kg giant with momentum.
//  - Movement with mass: slow to get going, a sprint that winds up, braking that takes a moment, a turn that lags (a player Human).
//  - What his body runs into is shoved by his momentum (AntWreck's own load path): a chair flies, a table slides and tips,
//    food is punted; something that does not give stops him and he reels back (at a run he can go down).
//  - The semi-ragdoll: springs on top of the posed body (HumanBody.cpp): the torso rocks with acceleration, the arms swing
//    with momentum, hits make him lurch, a bitten hand jerks.
//  - The player's keys: LMB a blow (hold: charged; a rhythm: a combo of three), Space a stomp (hold: a jump-stomp), Ctrl/C a kick,
//    Q a throw. Server-authoritative; every machine poses the same body from replicated state.
// =============================================================================================

namespace
{
	constexpr float UuPerM = AntWreck::UuPerM;
	float SpringStep(float& X, float& V, float Target, float K, float C, float Dt)
	{
		const float A = K * (Target - X) - C * V;
		V += A * Dt;
		X += V * Dt;
		return X;
	}
	AHumanWeapon* WeaponOf(const AHumanCharacter* H)
	{
		if (!H) { return nullptr; }
		for (TActorIterator<AHumanWeapon> It(H->GetWorld()); It; ++It)
		{
			if (It->Holder == H && !It->IsActorBeingDestroyed()) { return *It; }
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// The keys (the owning machine)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::OnFirePressed()
{
	FireDownAt = GetWorld()->GetRealTimeSeconds();
	ServerCharge(true);
}

void AHumanCharacter::OnFireReleased()
{
	if (FireDownAt < 0.f)
	{
		return;
	}
	const float HeldFor = GetWorld()->GetRealTimeSeconds() - FireDownAt;
	FireDownAt = -1.f;
	FVector P, D;
	if (!AimPoint(P, D))
	{
		ServerCharge(false);
		return;
	}
	// A click: a blow (a rhythm of them: the combo). Held past 0.3 s: charged, full at 0.9 s.
	ServerSwing(P, FMath::Clamp((HeldFor - 0.3f) / 0.6f, 0.f, 1.f));
}

void AHumanCharacter::OnJumpPressed()
{
	JumpDownAt = GetWorld()->GetRealTimeSeconds();
	bJumpFired = false;
}

void AHumanCharacter::OnJumpReleased()
{
	if (JumpDownAt < 0.f)
	{
		return;
	}
	JumpDownAt = -1.f;
	if (bJumpFired)
	{
		return;   // the hold already made it a jump-stomp
	}
	FVector P, D;
	if (AimPoint(P, D))
	{
		ServerStompAt(P);
	}
}

void AHumanCharacter::OnKickPressed() { ServerKick(); }

void AHumanCharacter::OnSprintPressed()
{
	bSprinting = true;   // at once here (the owner moves him); the server is told
	ServerSprint(true);
}

void AHumanCharacter::OnSprintReleased()
{
	bSprinting = false;
	ServerSprint(false);
}

void AHumanCharacter::OnThrowPressed()
{
	FVector P, D;
	if (AimPoint(P, D))
	{
		ServerThrow(D);
	}
}

void AHumanCharacter::ServerSwing_Implementation(FVector_NetQuantize Point, float Charge) { TrySwing(Point, FMath::Clamp(Charge, 0.f, 1.f)); }
void AHumanCharacter::ServerCharge_Implementation(bool bOn) { bCharging = bOn && !bKnockedOut && !IsDown(); }
void AHumanCharacter::ServerStompAt_Implementation(FVector_NetQuantize Point) { TryStompAt(Point); }
void AHumanCharacter::ServerJumpStomp_Implementation() { TryJumpStomp(); }
void AHumanCharacter::ServerKick_Implementation() { TryKick(); }
void AHumanCharacter::ServerThrow_Implementation(FVector_NetQuantizeNormal Dir) { ThrowHeld(Dir); }
void AHumanCharacter::ServerSprint_Implementation(bool bOn) { bSprinting = bOn; }

void AHumanCharacter::ClientStagger_Implementation(FVector_NetQuantize10 Vel, float Seconds)
{
	if (Seconds < 0.f)
	{
		// Down on the floor (a topple): face the way he fell and stop here too (this machine moves him).
		const FRotator Face(0.f, (float)FVector(Vel).Rotation().Yaw, 0.f);
		SetActorRotation(Face);
		if (Controller) { Controller->SetControlRotation(Face); }
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	LaunchCharacter(FVector(Vel), true, false);
}

// ---------------------------------------------------------------------------------------------
// The blows (server)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::TrySwing(const FVector& Point, float Charge)
{
	if (!HasAuthority())
	{
		return;
	}
	bCharging = false;
	const float Now = GetWorld()->GetTimeSeconds();
	if (bKnockedOut || bStunned || IsDown() || IsBodyBusy())
	{
		return;
	}
	const bool bCharged = Charge > 0.f;
	// A rhythm of clicks: the right-sided swing, the backhand, the overhead smash (heavier). A charged blow starts over.
	const bool bChain = !bCharged && Now < ComboUntil;
	const uint8 Step = bChain ? (uint8)((Combo + 1) % 3) : 0;
	if (bChain)
	{
		SwatReadyTime = FMath::Min(SwatReadyTime, Now);
	}
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D(), Right = GetActorRightVector().GetSafeNormal2D();
	SwingArg = bCharged ? 4 : Step;
	SwingPower = bCharged ? 1.f + Charge : (Step == 2 ? 1.35f : 1.f);
	SwingDir = (bCharged || Step == 2) ? (Fwd * 0.8f + FVector(0.f, 0.f, -0.6f)).GetSafeNormal()
		: Step == 0 ? (-Right * 0.85f + Fwd * 0.5f).GetSafeNormal() : (Right * 0.85f + Fwd * 0.5f).GetSafeNormal();
	const float Before = SwatReadyTime;
	TrySwat(Point, -1);
	const bool bStarted = SwatReadyTime != Before;
	SwingArg = 0;
	SwingPower = 1.f;
	SwingDir = FVector::ZeroVector;
	if (!bStarted)
	{
		return;
	}
	Combo = Step;
	const float Wind = AntWeapons::SwingWindup(this, HumanBody::Style(Fig).SwatHit);
	if (!bCharged && Step < 2)
	{
		// The next click may follow right after this blow lands.
		SwatReadyTime = Now + Wind + 0.12f;
		ComboUntil = SwatReadyTime + 0.75f;
	}
	else
	{
		// The combo's last blow (or a charged one): a breath before the next.
		SwatReadyTime = Now + Wind + 0.85f;
		ComboUntil = 0.f;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY swing %s at %s: %s, power %.2f, wind-up %.2f s"), AntWeapons::Held(this) ? AntWeapons::Held(this)->Name : TEXT("газета"),
		*Point.ToCompactString(), bCharged ? *FString::Printf(TEXT("charged %.2f"), Charge) : *FString::Printf(TEXT("combo %d"), (int32)Step), bCharged ? 1.f + Charge : (Step == 2 ? 1.35f : 1.f), Wind);
}

FVector AHumanCharacter::StompAimFrom(const FVector& Point, int32& OutLeg) const
{
	// Where a foot gets: in front of him, 250..1050 ahead and a stride to either side, on the floor he stands on.
	const FVector Loc = GetActorLocation();
	const FRotator Yaw(0.f, GetActorRotation().Yaw, 0.f);
	FVector Rel = Yaw.UnrotateVector(Point - Loc);
	Rel.X = FMath::Clamp((float)Rel.X, 250.f * FigScale, 1050.f * FigScale);
	Rel.Y = FMath::Clamp((float)Rel.Y, -600.f * FigScale, 600.f * FigScale);
	OutLeg = Rel.Y < 0.f ? 0 : 1;
	const FVector P = Loc + Yaw.RotateVector(FVector(Rel.X, Rel.Y, 0.f));
	return FVector(P.X, P.Y, FeetZ());
}

void AHumanCharacter::TryStompAt(const FVector& Point)
{
	if (!HasAuthority())
	{
		return;
	}
	int32 Leg = 0;
	PendingAim = StompAimFrom(Point, Leg);
	bPendingAim = true;
	TryStomp(Leg);
	bPendingAim = false;
}

void AHumanCharacter::TryJumpStomp()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || Fig != HumanFig::Dad || !bNewBody || bKnockedOut || bStunned || IsDown() || IsBodyBusy() || bStompPending || Now < StompReadyTime)
	{
		return;
	}
	if (!AntVersus::Spend(this, AntVersus::JumpStompCost))
	{
		return;
	}
	StompReadyTime = Now + 2.2f * (1.f - 0.15f * Rage);
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector At = FVector(GetActorLocation().X, GetActorLocation().Y, FeetZ()) + Fwd * 350.f * FigScale;
	StartAct(HumanAct::JumpStomp, 0, At);
	bSpraying = false;
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { DoJumpImpact(); }), HumanAct::JumpHit, false);
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY jump-stomp: up at %s"), *At.ToCompactString());
}

void AHumanCharacter::DoJumpImpact()
{
	if (Act.Kind != HumanAct::JumpStomp || IsDown() || bKnockedOut)
	{
		return;
	}
	UWorld* W = GetWorld();
	const FVector P = FVector(Act.Target);
	const float SR = GetStompRadius() * 1.6f;
	const float Wave = GetStompRadius() * 7.f;
	const float S = StrengthK * FMath::Sqrt(FMath::Max(0.25f, BodyMassK));
	if (AAntSwarm* Swarm = AAntSwarm::Get(W))
	{
		Swarm->KillInSphere(P, SR);
	}
	int32 Killed = 0, Thrown = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest() || Ant->GetRiddenHuman() == this || InCover(W, Ant->GetActorLocation()))
		{
			continue;
		}
		const FVector A = Ant->GetActorLocation();
		const float D = (float)FVector::Dist2D(A, P);
		if (FMath::Abs(A.Z - P.Z) > 500.f)
		{
			continue;
		}
		if (D < SR)
		{
			UGameplayStatics::ApplyDamage(Ant, 300.f * S, GetController(), this, UDamageType::StaticClass());
			++Killed;
		}
		else if (D < Wave && !Ant->IsAttached())
		{
			const float K = 1.f - (D - SR) / (Wave - SR);
			Ant->ApplyLaunch(((A - P).GetSafeNormal2D() * (700.f + 1300.f * K) + FVector(0.f, 0.f, 600.f + 700.f * K)) * AntKit::SpeedK);
			if (Ant->Carrying && K > 0.25f)
			{
				Ant->LetGo();
				Ant->Toast(TEXT("Человек прыгнул — груз выбило!"));
			}
			++Thrown;
		}
	}
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		const FVector To = L->GetActorLocation() - P;
		if (!L->bHeld && To.Size2D() < Wave && FMath::Abs(To.Z) < 2000.f)
		{
			const float K = 1.f - (float)To.Size2D() / Wave;
			if (L->IsCarried() && K > 0.3f) { L->ReleaseAll(); }
			L->Kick(To.GetSafeNormal2D() * 700.f * K * S + FVector(0.f, 0.f, 900.f * (0.3f + K)), L->GetActorLocation() + FMath::VRand() * 10.f);
		}
	}
	Shockwave(P, Wave, 2.2f * S, TEXT("прыжок"));
	for (TActorIterator<ACritter> It(W); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), P) < 7000.f) { It->Scare(P, TEXT("грохота")); }
	}
	AntSfx::Everyone(this, AntSfx::Stomp, P, 0.62f);
	AntSfx::Everyone(this, AntSfx::Drop, P, 0.5f);
	MulticastJolt(FVector::DownVector, 1.2f, 5);
	if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->MakeNoise(P, 1.f, AntNoise::Crash, this, nullptr);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY jump-stomp lands at %s: %d ants squashed, %d thrown (wave %.0f)"), *P.ToCompactString(), Killed, Thrown, Wave);
}

FVector AHumanCharacter::KickPoint() const
{
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	return FVector(GetActorLocation().X, GetActorLocation().Y, FeetZ() + 420.f * FigScale) + Fwd * 950.f * FigScale;
}

void AHumanCharacter::TryKick()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || Fig != HumanFig::Dad || !bNewBody || bKnockedOut || bStunned || IsDown() || IsBodyBusy() || Now < KickReadyTime)
	{
		return;
	}
	if (!AntVersus::Spend(this, AntVersus::KickCost))
	{
		return;
	}
	KickReadyTime = Now + 0.95f;
	StartAct(HumanAct::Kick, 1, KickPoint());
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { DoKickImpact(); }), HumanAct::KickHit, false);
}

void AHumanCharacter::DoKickImpact()
{
	if (Act.Kind != HumanAct::Kick || IsDown() || bKnockedOut)
	{
		return;
	}
	UWorld* W = GetWorld();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector P = KickPoint();
	// A running kick carries his momentum.
	const float Run = 1.f + FMath::Clamp((float)FVector::DotProduct(GetVelocity(), Fwd) / 1400.f, 0.f, 0.6f);
	const float S = StrengthK * Run * FMath::Sqrt(FMath::Max(0.25f, BodyMassK));
	const int32 N = AntWreck::Hit(W, P, 700.f, (Fwd + FVector(0.f, 0.f, 0.45f)).GetSafeNormal() * 22.f * S, 25.f * S, this, TEXT("пинок"));
	if (AAntSwarm* Swarm = AAntSwarm::Get(W))
	{
		Swarm->KillInSphere(FVector(P.X, P.Y, FeetZ()), 300.f);
	}
	int32 Ants = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest() || Ant->IsAttached() || Ant->GetRiddenHuman() == this || InCover(W, Ant->GetActorLocation()))
		{
			continue;
		}
		const FVector A = Ant->GetActorLocation();
		if (FVector::Dist2D(A, P) < 650.f && A.Z - FeetZ() < 900.f)
		{
			UGameplayStatics::ApplyDamage(Ant, 60.f * S, GetController(), this, UDamageType::StaticClass());
			if (!Ant->bDead)
			{
				Ant->ApplyLaunch((Fwd * 1600.f * FMath::Min(S, 2.f) + FVector(0.f, 0.f, 700.f)) * AntKit::SpeedK);
				if (Ant->Carrying) { Ant->LetGo(); }
			}
			++Ants;
		}
	}
	// A punt into something that does not give: the foot stops dead and the body lurches back.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HumanKick), false, this);
	const FVector From = FVector(GetActorLocation().X, GetActorLocation().Y, P.Z);
	if (N == 0 && W->LineTraceSingleByChannel(Hit, From, P + Fwd * 150.f, ECC_Pawn, Q))
	{
		Jolt(-Fwd, 0.6f, 1);
	}
	else
	{
		Jolt(Fwd, 0.35f, 0);
	}
	AntSfx::Everyone(this, AntSfx::Swat, P, FMath::FRandRange(0.75f, 0.9f));
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY kick at %s: %d things, %d ants (x%.2f)"), *P.ToCompactString(), N, Ants, S);
}

void AHumanCharacter::ThrowHeld(const FVector& InDir)
{
	if (!HasAuthority() || bKnockedOut || IsDown() || bStunned)
	{
		return;
	}
	const FVector Dir = (InDir.GetSafeNormal() + FVector(0.f, 0.f, 0.12f)).GetSafeNormal();
	if (Held)
	{
		// Food: the left arm throws it (GrabOrThrow) and it is watched in flight: what it hits takes the blow.
		AKitchenLoot* L = Held;
		GrabOrThrow(GetActorLocation(), Dir);
		FThrownThing T;
		T.What = L;
		T.Until = GetWorld()->GetTimeSeconds() + 3.f;
		T.Kg = FMath::Max(1, L->Weight) * AKitchenLoot::KgPerWeight;
		ThrownThings.Add(T);
		UE_LOG(LogTemp, Display, TEXT("ANTHEAVY throw: food %s (%.0f kg)"), *L->LootName, T.Kg);
		return;
	}
	AHumanWeapon* Wp = WeaponOf(this);
	if (!Wp)
	{
		if (AAntPlayerController* PC = Cast<AAntPlayerController>(GetController())) { PC->ClientToast(TEXT("Нечего бросать: E — взять еду или оружие")); }
		return;
	}
	if (IsBodyBusy() || !AntVersus::Spend(this, AntVersus::SwingCost(this)))
	{
		return;
	}
	// The weapon: the right arm goes back over the shoulder and lets it fly (the swing's motion, released at its top).
	StartAct(HumanAct::Swat, 0, GetActorLocation() + FVector(0.f, 0.f, 900.f * FigScale) + Dir * 3000.f);
	const float Release = FMath::Max(0.4f, AntWeapons::SwingWindup(this, HumanBody::Style(Fig).SwatHit)) * 0.85f;
	TWeakObjectPtr<AHumanWeapon> WeakWp(Wp);
	const float S = FMath::Sqrt(FMath::Max(0.25f, StrengthK));
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, WeakWp, Dir, S]()
	{
		AHumanWeapon* Mine = WeakWp.Get();
		if (!Mine || Mine->Holder != this)
		{
			return;
		}
		const FVector From = GetPalmLocation(1) + Dir * 200.f;
		Mine->Launch(this, From, Dir * 3400.f * S + FVector(0.f, 0.f, 500.f));
	}), Release, false);
	SwatReadyTime = GetWorld()->GetTimeSeconds() + Release + 0.6f;
}

// ---------------------------------------------------------------------------------------------
// Going down, lurching
// ---------------------------------------------------------------------------------------------

bool AHumanCharacter::Topple(const FVector& Dir, const TCHAR* Why)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (!HasAuthority() || !bNewBody || Fig != HumanFig::Dad || bKnockedOut || bStunned || IsDown() || Act.Kind == HumanAct::Sit)
	{
		return false;
	}
	FVector Fwd = Dir.GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = GetActorForwardVector().GetSafeNormal2D();
	}
	// The slip's fall (HumanAct::Fall): he goes down backwards, the feet shoot forward; he lies, sits up and gets up.
	const FRotator Face(0.f, (float)Fwd.Rotation().Yaw, 0.f);
	SetActorRotation(Face);
	if (Controller) { Controller->SetControlRotation(Face); }
	GetCharacterMovement()->StopMovementImmediately();
	if (IsPlayerControlled() && !IsLocallyControlled())
	{
		ClientStagger(Fwd, -1.f);
	}
	const FVector Floor(GetActorLocation().X, GetActorLocation().Y, FeetZ());
	StartAct(HumanAct::Fall, 0, Floor - Fwd * 2300.f * FigScale);
	bSpraying = false;
	bStompPending = false;
	bCharging = false;
	bTowel = false;
	FallImpactAt = Now + HumanAct::FallHit;
	NextFallTime = Now + HumanAct::FallUp + 20.f;
	BackAcheUntil = Now + HumanAct::FallUp + 6.f;
	if (Held)
	{
		AKitchenLoot* L = Held;
		Held = nullptr;
		if (IsValid(L) && L->bHeld) { L->Throw(Fwd * 900.f + FVector(0.f, 0.f, 1500.f)); }
	}
	AntWeapons::Drop(this, TEXT("упал"));
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 0.85f);
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY topple: the Human goes down at %s (%s)"), *GetActorLocation().ToCompactString(), Why ? Why : TEXT("-"));
	return true;
}

void AHumanCharacter::Jolt(const FVector& Dir, float Strength, uint8 Kind)
{
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Kind >= 3 && Kind <= 4)
	{
		if (Now < NextHandJolt) { return; }   // a swarm on the hand: not a jerk per bite
		NextHandJolt = Now + 0.3f;
	}
	MulticastJolt(Dir.GetSafeNormal(), FMath::Clamp(Strength, 0.f, 3.f), Kind);
}

void AHumanCharacter::MulticastJolt_Implementation(FVector_NetQuantizeNormal Dir, float Strength, uint8 Kind)
{
	FHumanRig& R = Rig;
	const FVector DB = GetActorTransform().InverseTransformVectorNoScale(FVector(Dir));
	const float S = Strength;
	if (Kind == 3 || Kind == 4)
	{
		// A bitten hand jerks away (and the arm shakes a little).
		R.PHandV[Kind - 3] += 1400.f * S;
		R.PArmV[Kind - 3] += 120.f * S * (FMath::RandBool() ? 1.f : -1.f);
		return;
	}
	// The push goes into the springs of the body: the torso lurches the way it was pushed, the pelvis is shoved, it twists a little.
	R.PLeanV += (float)DB.X * 170.f * S;
	R.PSideV += (float)-DB.Y * 120.f * S;
	R.POffV += FVector(DB.X, DB.Y, 0.f) * 900.f * S;
	R.PTwistV += FMath::FRandRange(-70.f, 70.f) * S;
	for (int32 s = 0; s < 2; ++s) { R.PArmV[s] += (float)-DB.X * 140.f * S; }
	if (Kind == 2)
	{
		R.PFlail = FMath::Min(1.f, R.PFlail + 0.9f * S);   // the arms go out: he is trying not to go down
	}
	if (Kind == 5 && GetNetMode() != NM_DedicatedServer)
	{
		// The jump-stomp's landing, here (cosmetic): dust, the cameras jolt.
		const FVector P = FVector(Act.Target);
		AntFx::StompDust(this, P + FVector(0.f, 0.f, 6.f));
		AntFx::LandDust(this, P + FVector(0.f, 0.f, 6.f), FVector::UpVector, 1400.f * FigScale);
		AntFilm::Shake(this, P, 1.f, 9000.f);
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn()))
			{
				Me->AddTrauma(FMath::Clamp(1.f - (float)FVector::Dist(Me->GetActorLocation(), P) / 7000.f, 0.f, 1.f));
			}
		}
	}
	if (Kind == 2 && IsLocallyControlled())
	{
		AntFilm::Shake(this, GetActorLocation(), 0.5f * FMath::Min(S, 1.5f), 4000.f);
	}
}

// ---------------------------------------------------------------------------------------------
// The floor jumps (stomps): pieces round the point are shoved out, light things hop, fixtures go over
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::Shockwave(const FVector& P, float Radius, float Power, const TCHAR* Why)
{
	UWorld* W = GetWorld();
	if (!HasAuthority() || !AntWreck::Enabled() || Power <= 0.f)
	{
		return;
	}
	struct FCand { int32 Inst; float D; FVector C; float Mass; uint8 Mat; };
	TArray<FCand> Cands;
	const float Floor = (float)P.Z;
	for (const AntWreck::FPiece& Pc : AntWreck::Pieces(W))
	{
		if (Pc.bFixed || Pc.Box.Min.Z > Floor + 1800.f || Pc.Mass > 40.f)
		{
			continue;   // built in, high on the wall, or too heavy to jump
		}
		const FVector C = Pc.Box.GetCenter();
		const float D = (float)FVector::Dist2D(C, P);
		if (D < Radius)
		{
			Cands.Add({ Pc.Inst, D, C, Pc.Mass, Pc.Mat });
		}
	}
	// Bodies already awake / moved: their own place.
	for (TActorIterator<AAntWreckBody> It(W); It; ++It)
	{
		for (FCand& Cd : Cands)
		{
			if (Cd.Inst == It->Inst) { Cd.C = It->WorldBox().GetCenter(); Cd.D = (float)FVector::Dist2D(Cd.C, P); }
		}
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.D < B.D; });
	int32 N = 0;
	for (const FCand& Cd : Cands)
	{
		if (N >= 14 || Cd.D >= Radius)
		{
			continue;
		}
		const float K = 1.f - Cd.D / Radius;
		const bool bLight = Cd.Mass < 4.f;
		// m/s: a light thing hops, a heavier one is shoved out along the floor.
		const FVector Out = (Cd.C - P).GetSafeNormal2D();
		const FVector V = (Out * (bLight ? 0.5f : 0.45f) + FVector(0.f, 0.f, bLight ? 1.1f : 0.25f)) * K * Power;
		const FVector J = V * Cd.Mass;
		const float Dmg = (Cd.Mat == AntWreck::Glass || Cd.Mat == AntWreck::China) ? 6.f * K * Power : 0.f;
		if ((float)J.Size() > AntWreck::Class(Cd.Mat).WakeK * Cd.Mass || Dmg >= 1.f)
		{
			AntWreck::HitPiece(W, Cd.Inst, Cd.C + FVector(0.f, 0.f, -30.f), J, Dmg, this);
			++N;
		}
	}
	int32 Props = 0, Fx = 0;
	for (TActorIterator<AKitchenProp> It(W); It; ++It)
	{
		const FVector Q = It->GetActorLocation();
		const float D = (float)FVector::Dist2D(Q, P);
		if (D < Radius && Q.Z - Floor < 2000.f)
		{
			const float K = 1.f - D / Radius;
			It->Jolt((FVector(Q - P).GetSafeNormal2D() * 160.f * K + FVector(0.f, 0.f, 240.f + 260.f * K)) * FMath::Min(Power, 2.5f) + FMath::VRand() * 50.f, this);
			++Props;
		}
	}
	for (TActorIterator<AKitchenFixture> It(W); It; ++It)
	{
		AKitchenFixture* F = *It;
		const float D = F->DistanceTo(P);
		if (D < Radius * 0.5f && F->State == AKitchenFixture::Fixed && (1.f - D / (Radius * 0.5f)) * Power > 0.9f && F->GetActorLocation().Z - Floor < 1900.f)
		{
			F->Fall((F->GetActorLocation() - P).GetSafeNormal2D(), nullptr, Why);
			++Fx;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY shockwave %s at %s r=%.0f x%.2f: %d pieces, %d props, %d fixtures (awake %d)"), Why, *P.ToCompactString(), Radius, Power, N, Props, Fx, AAntWreckBody::NumAwake(W));
}

// ---------------------------------------------------------------------------------------------
// What his body runs into (server)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::BodyContacts(float Dt)
{
	UWorld* W = GetWorld();
	const FVector V = GetVelocity() * FVector(1.f, 1.f, 0.f);
	const float Speed = (float)V.Size();
	// The momentum he brings: last tick's velocity when this tick's is lower (a blocking hit has already eaten this tick's).
	const FVector MV = PrevBodySpeed > Speed ? PrevBodyVel : V;
	const float MS = (float)MV.Size();
	const float DropK = PrevBodySpeed > 1.f ? Speed / PrevBodySpeed : 1.f;
	PrevBodyVel = V;
	PrevBodySpeed = Speed;
	if (!HasAuthority() || Fig != HumanFig::Dad || !bNewBody || bAway || IsDown() || Act.Kind == HumanAct::Sit || MS < 120.f)
	{
		return;
	}
	const float Now = W->GetTimeSeconds();
	// A player, an angry Human (rage 2+, his rampage) or a scripted puppet shoves the furniture about; the calm AI only nudges food.
	const bool bRough = IsPlayerControlled() || Rage >= 2 || bDevPuppet;
	const FVector Dir = MV / MS;
	const FVector Loc = GetActorLocation();
	const float Feet = FeetZ();
	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float CR = Cap->GetScaledCapsuleRadius(), CH = Cap->GetScaledCapsuleHalfHeight();
	const FVector Probe = Loc + Dir * FMath::Min(MS * 0.12f, 220.f);
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HumanBody), false, this);
	FCollisionObjectQueryParams OQ;
	OQ.AddObjectTypesToQuery(ECC_WorldStatic);
	OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
	OQ.AddObjectTypesToQuery(ECC_PhysicsBody);
	W->OverlapMultiByObjectType(Hits, Probe, FQuat::Identity, OQ, FCollisionShape::MakeCapsule(CR + 70.f, CH - 40.f), Q);
	const float Mh = BodyKg * FMath::Max(0.25f, BodyMassK);
	bool bBlocked = false;
	FVector BlockAt = FVector::ZeroVector;
	TSet<UObject*> Seen;
	for (const FOverlapResult& O : Hits)
	{
		UPrimitiveComponent* C = O.GetComponent();
		AActor* A = O.GetActor();
		if (!C || !A || A == this || Cast<APawn>(A) || C->ComponentHasTag(TEXT("AntsOnly")))
		{
			continue;
		}
		const FBox B = C->Bounds.GetBox();
		if (B.Min.Z > Feet + 2600.f || B.Max.Z < Feet + 25.f)
		{
			continue;   // over his head, or flat on the floor
		}
		// The contact: the nearest point of it at about his hips (or its top, if lower: a table is pushed at its edge and tips).
		const float Z = FMath::Clamp(Feet + 1500.f * FigScale, (float)B.Min.Z, (float)B.Max.Z);
		const FVector Pt = B.GetClosestPointTo(FVector(Loc.X, Loc.Y, Z));
		FVector To = (Pt - Loc) * FVector(1.f, 1.f, 0.f);
		To = To.IsNearlyZero() ? Dir : To.GetSafeNormal();
		const float Approach = (float)FVector::DotProduct(MV, To);
		if (Approach < 100.f)
		{
			continue;   // not going into it
		}
		AAntWreckBody* Body = Cast<AAntWreckBody>(A);
		const int32 Inst = Body ? Body->Inst : AntWreck::PieceOfComponent(W, C);
		UObject* Key = Body ? (UObject*)Body : Inst >= 0 ? (UObject*)C : (UObject*)A;
		if (Seen.Contains(Key))
		{
			continue;
		}
		Seen.Add(Key);
		const float* Last = ContactCool.Find(Key);
		const bool bCool = Last && Now - *Last < 0.3f;
		if (AKitchenLoot* L = Cast<AKitchenLoot>(A))
		{
			if (L->bHeld || L->bIsGear || bCool)
			{
				continue;
			}
			// Food is punted along: a little faster than he goes, a hop at a run.
			const float K = bRough ? 1.f : 0.5f;
			if (bRough && L->IsCarried() && Approach > 650.f) { L->ReleaseAll(); }
			L->Kick((To * Approach * 1.3f + FVector(0.f, 0.f, 150.f + 0.25f * Approach)) * K, Pt);
			ContactCool.Add(Key, Now);
			continue;
		}
		if (AKitchenProp* Pr = Cast<AKitchenProp>(A))
		{
			if (!bCool)
			{
				Pr->Jolt(To * Approach * (bRough ? 0.9f : 0.4f) + FVector(0.f, 0.f, 200.f), this);
				ContactCool.Add(Key, Now);
			}
			continue;
		}
		if (AKitchenFixture* Fx = Cast<AKitchenFixture>(A))
		{
			if (!bCool && bRough && Approach > 450.f)
			{
				if (Fx->State == AKitchenFixture::Fixed) { Fx->PushDir = To; Fx->Fall(To, nullptr, TEXT("человек задел")); }
				else if (Fx->State == AKitchenFixture::Lying && Fx->IsBreakable() && Approach > 900.f) { Fx->BreakNow(nullptr); }
				ContactCool.Add(Key, Now);
			}
			continue;
		}
		if (Inst < 0)
		{
			// The walls, the built-in architecture: it stops him (blocking stuff only).
			if (C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && Approach > 0.6f * MS)
			{
				bBlocked = true;
				BlockAt = Pt;
			}
			continue;
		}
		const AntWreck::FPiece* Pc = AntWreck::PieceOf(W, Inst);
		if (!Pc)
		{
			continue;
		}
		const float Mo = Body ? Body->Mass : Pc->Mass;
		const bool bFixed = Body ? Body->bFixed : Pc->bFixed;
		if (!bRough && Mo > 1.5f)
		{
			continue;   // the calm AI does not wreck his own kitchen
		}
		if (Body && Body->IsBroken())
		{
			continue;
		}
		if (bCool)
		{
			continue;
		}
		// A heavy body into a lighter one: it goes off at about his speed (x2 m/(M+m), a little lost in the crash); light ones fly.
		const float Ratio = 2.f * Mh / (Mh + Mo) * 0.75f * (Mo < 8.f ? 1.25f : 1.f);
		const float VT = Approach * Ratio;
		const float VObj = (Body && Body->bAwake && Body->Root) ? (float)FVector::DotProduct(Body->Root->GetPhysicsLinearVelocity(), To) : 0.f;
		const float Dv = FMath::Max(0.f, VT - VObj);
		const FVector JDir = (To + FVector(0.f, 0.f, (Mo < 8.f && Approach > 800.f) ? 0.35f : 0.05f)).GetSafeNormal();
		const FVector J = JDir * Mo * Dv / UuPerM;   // kg m/s
		const float Dmg = bRough ? 30.f * FMath::Max(0.25f, BodyMassK) * FMath::Max(0.f, (Approach - 750.f) / 700.f) : 0.f;
		const AntWreck::FClassDef& Cl = AntWreck::Class(Body ? Body->Mat : Pc->Mat);
		// Walking into a table pushes it along (lighter than him, not an appliance): it wakes even under the strike's threshold.
		const bool bPush = bRough && Approach > 300.f && !bFixed && Mo < Mh && Cl.WakeK < 1.f;
		const bool bGives = !bFixed && ((Body && Body->bAwake) || (float)J.Size() > Cl.WakeK * Mo || bPush);
		if (bGives || Dmg >= 1.f)
		{
			AAntWreckBody* Hit = Body;
			if (Body) { Body->TakeHit(Pt, J, Dmg, this); }
			else { Hit = AntWreck::HitPiece(W, Inst, Pt, J, Dmg, this); }
			if (bPush && Hit && !Hit->bAwake && !Hit->IsBroken())
			{
				Hit->Wake(J, Pt);
			}
			ContactCool.Add(Key, Now);
			++BumpCount;
			if (Now >= NextContactLog || Approach > 900.f)
			{
				NextContactLog = Now + 0.5f;
				const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
				UE_LOG(LogTemp, Display, TEXT("ANTHEAVY body hits %s (%s %.1f kg) at %.0f uu/s: %s, impulse %.1f, damage %.0f (awake %d)"), All.IsValidIndex(Inst) ? All[Inst].Name : TEXT("?"),
					Cl.Name, Mo, Approach, bGives ? TEXT("shoved") : TEXT("held"), (float)J.Size(), Dmg, AAntWreckBody::NumAwake(W));
			}
			if (bGives && Mo > 6.f)
			{
				Jolt(-To, FMath::Clamp(Approach / 1500.f, 0.15f, 0.6f) * FMath::Min(1.f, Mo / Mh * 2.f), 0);   // the shove goes back into him a little
			}
		}
		if (!bGives && Approach > 0.6f * MS)
		{
			bBlocked = true;
			BlockAt = Pt;
		}
	}
	// Ran into something that does not give: he reels back (and at a run he may go down).
	if (bBlocked && MS > 850.f && DropK < 0.6f && Now >= NextStaggerAt)
	{
		NextStaggerAt = Now + 1.2f;
		const float K = FMath::Clamp((MS - 850.f) / 700.f, 0.f, 1.f);
		Jolt(-Dir, 0.6f + 0.9f * K, 2);
		StaggerUntil = Now + 0.4f + 0.5f * K;
		const FVector Back = -Dir * (450.f + 650.f * K);
		if (IsPlayerControlled() && !IsLocallyControlled()) { ClientStagger(Back, 0.4f + 0.5f * K); }
		else { LaunchCharacter(Back, true, false); }
		AntSfx::Everyone(this, AntSfx::Stomp, BlockAt, 0.7f);
		UE_LOG(LogTemp, Display, TEXT("ANTHEAVY stagger: ran into something solid at %.0f uu/s (k %.2f) at %s"), MS, K, *BlockAt.ToCompactString());
		if (bRough && MS > 1150.f && FMath::FRand() < 0.3f + 0.5f * K)
		{
			FTimerHandle H;
			const FVector D = Dir;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, D]() { Topple(D, TEXT("врезался на бегу")); }), 0.25f, false);
		}
	}
	if (ContactCool.Num() > 64)
	{
		for (auto It = ContactCool.CreateIterator(); It; ++It) { if (!It.Key().IsValid() || Now - It.Value() > 2.f) { It.RemoveCurrent(); } }
	}
}

void AHumanCharacter::TickThrown(float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = ThrownThings.Num() - 1; i >= 0; --i)
	{
		FThrownThing& T = ThrownThings[i];
		AKitchenLoot* L = Cast<AKitchenLoot>(T.What.Get());
		if (!L || Now > T.Until)
		{
			ThrownThings.RemoveAt(i);
			continue;
		}
		if (L->bHeld)
		{
			continue;   // still in the hand (the arm has not let go yet)
		}
		const FVector V = L->GetVelocity();
		const float Was = (float)T.LastVel.Size();
		if (Was > 900.f && (float)V.Size() < Was * 0.5f)
		{
			// It hit something: what is there takes the blow.
			const FVector At = L->GetActorLocation();
			const int32 N = AntWreck::Hit(GetWorld(), At, 380.f, T.LastVel.GetSafeNormal() * T.Kg * Was / UuPerM * 1.5f, T.Kg * Was / 1000.f * 3.f * StrengthK, this, TEXT("брошенная еда"));
			UE_LOG(LogTemp, Display, TEXT("ANTHEAVY thrown %s hit at %s at %.0f uu/s: %d things"), *L->LootName, *At.ToCompactString(), Was, N);
			ThrownThings.RemoveAt(i);
			continue;
		}
		T.LastVel = V;
	}
}

// ---------------------------------------------------------------------------------------------
// Every tick: the movement's weight, the sprint, the contacts, the camera
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::TickHeavyBody(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	UCharacterMovementComponent* CM = GetCharacterMovement();
	const bool bDad = Fig == HumanFig::Dad && bNewBody;
	const bool bDevDrive = DevInputUntil > Now;
	const bool bPlayer = IsPlayerControlled() || bDevDrive;
	const float Speed = (float)GetVelocity().Size2D();
	// The sprint winds up: it takes a second and a half of running to reach its top speed (every machine, from the replicated flag).
	const bool bRun = bSprinting && Speed > 200.f && !IsDown() && !bKnockedOut && !bStunned && !IsStaggered() && !IsRooted();
	SprintRamp = FMath::FInterpConstantTo(SprintRamp, bRun ? 1.f : 0.f, Dt, bRun ? 0.7f : 1.8f);
	if (bDad)
	{
		if (bPlayer)
		{
			// A 90 kg giant: slow to get going, slow to stop (no stopping on a dime), a change of direction carries the old one for a moment.
			const float MK = FMath::Sqrt(FMath::Max(0.25f, BodyMassK));
			CM->MaxAcceleration = 1150.f / MK;
			CM->BrakingDecelerationWalking = 820.f / MK;
			CM->GroundFriction = 2.6f / MK;
			CM->bUseSeparateBrakingFriction = true;
			CM->BrakingFriction = 0.4f;
			// The body turns after the camera, not with it: the heavier and the faster, the slower.
			bUseControllerRotationYaw = false;
			CM->bOrientRotationToMovement = false;
			CM->bUseControllerDesiredRotation = !IsDown() && !IsStaggered() && Act.Kind != HumanAct::Sit;
			CM->RotationRate = FRotator(0.f, FMath::Lerp(320.f, 150.f, SprintRamp) / MK, 0.f);
		}
		else if (!bUseControllerRotationYaw)
		{
			// Back to the AI: his movement as it always was.
			CM->MaxAcceleration = 2048.f;
			CM->BrakingDecelerationWalking = 2048.f;
			CM->GroundFriction = 8.f;
			CM->bUseSeparateBrakingFriction = false;
			CM->bUseControllerDesiredRotation = false;
			bUseControllerRotationYaw = true;
		}
	}
	if (bDevDrive && Controller && !IsRooted() && !IsStaggered() && !IsDown())
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), DevInput.X);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), DevInput.Y);
	}
	// Space held: the stomp becomes a jump-stomp (the owner's machine decides).
	if (JumpDownAt >= 0.f && !bJumpFired && W->GetRealTimeSeconds() - JumpDownAt > 0.38f)
	{
		bJumpFired = true;
		ServerJumpStomp();
	}
	BodyContacts(Dt);
	if (HasAuthority())
	{
		TickThrown(Dt);
	}
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		TickCamera(Dt);
	}
}

void AHumanCharacter::TickCamera(float Dt)
{
	if (!Camera || !SpringArm)
	{
		return;
	}
	if (!bCamInit)
	{
		BaseFov = Camera->FieldOfView;
		BaseSocket = SpringArm->SocketOffset;
		bCamInit = true;
	}
	const float Speed = (float)GetVelocity().Size2D();
	const float WalkK = FMath::Clamp(Speed / 700.f, 0.f, 1.6f);
	// A heavy stride: the view sways side to side with the steps and dips on each footfall; it lurches with the body.
	CamBob += Dt * (1.1f + 0.7f * SprintRamp) * 2.f * PI * FMath::Min(1.f, Speed / 250.f);
	const FVector Bob(0.f, FMath::Sin(CamBob * 0.5f) * 30.f * WalkK, -FMath::Abs(FMath::Sin(CamBob)) * 40.f * WalkK);
	const FVector Lurch(-Rig.POff.X * 0.5f, -Rig.POff.Y * 0.5f, -FMath::Abs(Rig.PLean) * 4.f);
	SpringArm->SocketOffset = FMath::VInterpTo(SpringArm->SocketOffset, BaseSocket + Bob + Lurch, Dt, 12.f);
	const float FovT = BaseFov + 10.f * SprintRamp * FMath::Min(1.f, Speed / 1000.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, FovT, Dt, 3.f));
	Camera->SetRelativeRotation(FRotator(0.f, 0.f, Rig.PSide * 0.35f + FMath::Sin(CamBob * 0.5f) * 0.7f * WalkK));
}

// ---------------------------------------------------------------------------------------------
// The semi-ragdoll springs (every machine, from PoseBody)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::TickPhysLayer(float Dt, const FTransform& Body)
{
	FHumanRig& R = Rig;
	if (Dt <= 1.e-4f)
	{
		return;
	}
	const FVector V = GetVelocity() * FVector(1.f, 1.f, 0.f);
	if (!R.bPrevVel)
	{
		R.PrevVel = V;
		R.bPrevVel = true;
	}
	// (a teleport or a launch is not an acceleration of the body: clamped)
	const FVector Acc = ((V - R.PrevVel) / Dt).GetClampedToMaxSize(5000.f);
	R.PrevVel = V;
	const FVector AB = Body.InverseTransformVectorNoScale(Acc);
	const float Speed = (float)V.Size();
	const bool bDown = Act.Kind == HumanAct::Fall && IsDown();
	const float Free = bDown ? 0.f : 1.f;
	// Passive: braking pitches the torso forward, speeding up leaves it behind; a turn rolls it; the pelvis lags the feet.
	// Active: a run leans into it.
	const float LeanT = (FMath::Clamp(-(float)AB.X * 0.0065f, -9.f, 12.f) + 6.f * SprintRamp * FMath::Min(1.f, Speed / 900.f)) * Free;
	const float SideT = FMath::Clamp((float)AB.Y * 0.005f, -8.f, 8.f) * Free;
	SpringStep(R.PLean, R.PLeanV, LeanT, 55.f, 8.f, Dt);
	SpringStep(R.PSide, R.PSideV, SideT, 55.f, 8.f, Dt);
	SpringStep(R.PTwist, R.PTwistV, 0.f, 45.f, 8.f, Dt);
	float OX = (float)R.POff.X, OXV = (float)R.POffV.X, OY = (float)R.POff.Y, OYV = (float)R.POffV.Y;
	SpringStep(OX, OXV, FMath::Clamp(-(float)AB.X * 0.012f, -80.f, 80.f) * Free, 40.f, 7.f, Dt);
	SpringStep(OY, OYV, FMath::Clamp(-(float)AB.Y * 0.012f, -80.f, 80.f) * Free, 40.f, 7.f, Dt);
	R.POff = FVector(FMath::Clamp(OX, -400.f, 400.f), FMath::Clamp(OY, -400.f, 400.f), 0.f);
	R.POffV = FVector(OXV, OYV, 0.f);
	for (int32 s = 0; s < 2; ++s)
	{
		// The arms keep going when he brakes, and trail when he sets off.
		SpringStep(R.PArm[s], R.PArmV[s], FMath::Clamp(-(float)AB.X * 0.009f, -25.f, 25.f) * Free, 28.f, 4.5f, Dt);
		R.PArm[s] = FMath::Clamp(R.PArm[s], -60.f, 60.f);
		SpringStep(R.PHand[s], R.PHandV[s], 0.f, 110.f, 11.f, Dt);
		R.PHand[s] = FMath::Clamp(R.PHand[s], -300.f, 350.f);
	}
	R.PLean = FMath::Clamp(R.PLean, -32.f, 32.f);
	R.PSide = FMath::Clamp(R.PSide, -24.f, 24.f);
	R.PTwist = FMath::Clamp(R.PTwist, -30.f, 30.f);
	R.PFlail = FMath::FInterpTo(R.PFlail, IsStaggered() ? FMath::Max(R.PFlail, 0.6f) : 0.f, Dt, 2.5f);
	if (bDown)
	{
		R.PLean *= 0.9f; R.PSide *= 0.9f; R.PTwist *= 0.9f;
	}
	R.ChargeW = FMath::FInterpConstantTo(R.ChargeW, (bCharging && !IsBodyBusy()) ? 1.f : 0.f, Dt, bCharging ? 2.2f : 6.f);
}

// ---------------------------------------------------------------------------------------------
// Dev: -AntDo "heavy ..." (a scripted Human: his keys, his movement, the admin's knobs)
// ---------------------------------------------------------------------------------------------

bool AHumanCharacter::HeavyDevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0 || !A[0].Equals(TEXT("heavy"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	AHumanCharacter* H = nullptr;
	for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (It->GetClass() == AHumanCharacter::StaticClass()) { H = *It; break; } }
	if (!H)
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTHEAVY dev: no Human"));
		return true;
	}
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	const float Now = W->GetTimeSeconds();
	const FVector Fwd = H->GetActorForwardVector().GetSafeNormal2D();
	auto Ahead = [&](float Dist, float Up) { return H->GetActorLocation() + Fwd * Dist + FVector(0.f, 0.f, Up - H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); };
	if (Op == TEXT("play"))
	{
		// The first local player takes the Human now (nothing if he already plays him).
		APlayerController* PC = W->GetFirstPlayerController();
		if (AAntPlayerController* APC = Cast<AAntPlayerController>(PC); APC && H->GetController() != APC)
		{
			GM->ToggleHuman(APC);
		}
	}
	else if (Op == TEXT("move"))
	{
		// heavy move fwd right seconds: as if the keys were held
		H->DevInput = FVector2D(Num(2, 1.f), Num(3, 0.f));
		H->DevInputUntil = Now + Num(4, 2.f);
	}
	else if (Op == TEXT("stop")) { H->DevInputUntil = -1.f; H->bSprinting = false; }
	else if (Op == TEXT("aim") || Op == TEXT("face"))
	{
		// heavy aim yaw [pitch] | heavy face x y: the controller (and the body, at once with "face")
		FRotator R(Num(3, -10.f), Num(2), 0.f);
		if (Op == TEXT("face")) { R = FRotator(-10.f, (float)(FVector(Num(2), Num(3), 0.f) - H->GetActorLocation()).Rotation().Yaw, 0.f); H->SetActorRotation(FRotator(0.f, R.Yaw, 0.f)); }
		if (H->Controller) { H->Controller->SetControlRotation(R); }
	}
	else if (Op == TEXT("sprint")) { H->bSprinting = Num(2, 1.f) > 0.5f; }
	else if (Op == TEXT("swing"))
	{
		// heavy swing [charge] [x y z]: the LMB blow at a point (default: chest high, 1800 ahead)
		H->SwatReadyTime = FMath::Min(H->SwatReadyTime, Now + 0.f);
		H->TrySwing(A.Num() >= 6 ? Vec(3) : Ahead(1800.f, 1500.f), FMath::Clamp(Num(2, 0.f), 0.f, 1.f));
	}
	else if (Op == TEXT("stomp")) { H->StompReadyTime = 0.f; H->TryStompAt(A.Num() >= 5 ? Vec(2) : Ahead(700.f, 0.f)); }
	else if (Op == TEXT("jump")) { H->StompReadyTime = 0.f; H->TryJumpStomp(); }
	else if (Op == TEXT("kick")) { H->KickReadyTime = 0.f; H->TryKick(); }
	else if (Op == TEXT("throw"))
	{
		// heavy throw [yaw pitch]: what is in his hands, that way (default: where he faces)
		const FVector D = A.Num() >= 4 ? FRotator(Num(3), Num(2), 0.f).Vector() : Fwd;
		H->SwatReadyTime = 0.f;
		H->ThrowHeld(D);
	}
	else if (Op == TEXT("topple")) { H->Topple(Fwd, TEXT("dev")); }
	else if (Op == TEXT("mass")) { H->BodyMassK = FMath::Clamp(Num(2, 1.f), 0.25f, 5.f); }
	else if (Op == TEXT("strength")) { H->StrengthK = FMath::Clamp(Num(2, 1.f), 0.25f, 10.f); }
	else if (Op == TEXT("patience")) { AntVersus::InfinitePatience() = A.IsValidIndex(2) ? Num(2) > 0.5f : !AntVersus::InfinitePatience(); }
	else if (Op == TEXT("jolt")) { H->Jolt(-Fwd, Num(2, 1.f), (uint8)Num(3, 2.f)); }
	UE_LOG(LogTemp, Display, TEXT("ANTHEAVY dev %s: at %s yaw %.0f speed %.0f sprint %d ramp %.2f act %d t %.2f stagger %d down %d mass x%.1f strength x%.1f patience-inf %d bumps %d | lean %.1f side %.1f twist %.1f off (%.0f,%.0f) flail %.2f | player %d"),
		*Op, *H->GetActorLocation().ToCompactString(), (float)H->GetActorRotation().Yaw, (float)H->GetVelocity().Size2D(), H->bSprinting ? 1 : 0, H->SprintRamp, (int32)H->Act.Kind, H->ActTime(),
		H->IsStaggered() ? 1 : 0, H->IsDown() ? 1 : 0, H->BodyMassK, H->StrengthK, AntVersus::InfinitePatience() ? 1 : 0, H->BumpCount,
		H->Rig.PLean, H->Rig.PSide, H->Rig.PTwist, H->Rig.POff.X, H->Rig.POff.Y, H->Rig.PFlail, H->IsPlayerControlled() ? 1 : 0);
	return true;
}
