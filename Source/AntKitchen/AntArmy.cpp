#include "AntArmy.h"
#include "AntCarry.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntSwarm.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntChain.h"
#include "AntSquad.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/DamageType.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AAntArmy::AAntArmy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(15.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntArmy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntArmy, Team);
	DOREPLIFETIME(AAntArmy, Count);
	DOREPLIFETIME(AAntArmy, Mode);
	DOREPLIFETIME(AAntArmy, Target);
	DOREPLIFETIME(AAntArmy, Commander);
	DOREPLIFETIME(AAntArmy, Trail);
	DOREPLIFETIME(AAntArmy, TrailHead);
	DOREPLIFETIME(AAntArmy, bSwarming);
	DOREPLIFETIME(AAntArmy, bSquad);
	DOREPLIFETIME(AAntArmy, Cargo);
	DOREPLIFETIME(AAntArmy, bCargoHeld);
	DOREPLIFETIME(AAntArmy, bCargoHome);
	DOREPLIFETIME(AAntArmy, bForaging);
	DOREPLIFETIME(AAntArmy, bOrphan);
	DOREPLIFETIME(AAntArmy, OwnerPS);
}

void AAntArmy::BeginPlay()
{
	Super::BeginPlay();
	LastPos = GetActorLocation();
	if (HasAuthority())
	{
		PushTrail(GetActorLocation());
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Bodies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Bodies"));
	Bodies->SetupAttachment(RootComponent);
	Bodies->SetUsingAbsoluteRotation(true);
	Bodies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bodies->SetCastShadow(false);
	Bodies->SetCanEverAffectNavigation(false);
	const AntArt::FPart& Art = AntArt::Get(TEXT("SM_Ant_Body"));
	if (Art.IsValid())
	{
		Bodies->SetStaticMesh(Art.Mesh);
		if (Art.Material) { Bodies->SetMaterial(0, Art.Material); }
		if (Team == 1)
		{
			if (UMaterialInstanceDynamic* M = Bodies->CreateDynamicMaterialInstance(0))
			{
				M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(2.2f, 0.6f, 0.35f));
			}
		}
	}
	else
	{
		Bodies->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
		AntKit::Paint(Bodies, Team == 1 ? FLinearColor(0.5f, 0.08f, 0.03f) : FLinearColor(0.05f, 0.02f, 0.015f));
	}
	Bodies->RegisterComponent();
	AddInstanceComponent(Bodies);
}

AAntArmy* AAntArmy::FindFor(const AAntCharacter* C)
{
	if (!C)
	{
		return nullptr;
	}
	for (TActorIterator<AAntArmy> It(C->GetWorld()); It; ++It)
	{
		if (It->Commander == C && It->Mode != EArmyMode::Home)
		{
			return *It;
		}
	}
	return nullptr;
}

FVector AAntArmy::GroundAt(const FVector& P) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArmyGround), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 150.f * AntKit::AntShrink), P - FVector(0.f, 0.f, 6000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 4.f);
	}
	return P;
}

int32 AAntArmy::Lose(int32 N)
{
	const int32 Dead = FMath::Clamp(N, 0, Count);
	Count -= Dead;
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	if (Team == 0 && Dead > 0 && !(GS && GS->bWarMode) && !bSquad)   // (a squad's workers are the nest's own stock, not the trail swarm)
	{
		if (AAntSwarm* S = AAntSwarm::Get(GetWorld())) { S->Population = FMath::Max(0, S->Population - Dead); }
	}
	if (Count <= 0)
	{
		if (bSquad && IsValid(Commander)) { Commander->Toast(TEXT("ОТРЯД ПОГИБ. Новых рабочих вырастят только в гнезде")); }
		Destroy();
	}
	return Dead;
}

bool AAntArmy::HelpsCommander(const AAntCharacter* Cmd) const
{
	return bSquad && Cmd && Commander == Cmd && Mode == EArmyMode::Follow && !bCargoHeld && FVector::Dist(GetActorLocation(), Cmd->GetActorLocation()) < 700.f;
}

void AAntArmy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && Cargo && bCargoHeld && IsValid(Cargo))
	{
		Cargo->SquadDrop();   // whatever it carried falls where it stood
	}
	if (HasAuthority())
	{
		// workers holding a load (AntCarry) let go: theirs, or their commander's
		if (IsValid(Cargo)) { AntCarry::RemoveArmy(Cargo, this); }
		if (IsValid(Commander) && IsValid(Commander->Carrying)) { AntCarry::RemoveArmy(Commander->Carrying, this); }
	}
	Super::EndPlay(EndPlayReason);
}

void AAntArmy::Disband()
{
	Mode = EArmyMode::Home;
	Target = AKitchenBuilder::NestLocation(FMath::Min<uint8>(Team, 1));
	Commander = nullptr;
}

int32 AAntArmy::KillInSphere(UWorld* World, const FVector& Center, float R, int32 MaxKills)
{
	int32 Total = 0;
	if (!World)
	{
		return 0;
	}
	for (TActorIterator<AAntArmy> It(World); It; ++It)
	{
		AAntArmy* A = *It;
		// The ants actually under the foot die (a column loses a chunk, a swarm loses its middle).
		int32 Under = A->CountNear(Center, R, FMath::Max(R, 250.f));
		if (A->bSquad)
		{
			// A raid squad is small and walks in a knot: one blow (a slipper, a paw, a lick) takes a quarter of it at most,
			// and the rest scatter for two seconds - or a dog walking over it would eat the whole stake in a blink.
			if (World->GetTimeSeconds() < A->BlowSafeUntil) { continue; }
			Under = FMath::Min(Under, FMath::Max(1, (A->Count + 3) / 4));
			if (A->bArmor) { Under = (Under + 1) / 2; }   // «Панцири»: half of those live
			if (Under > 0) { A->BlowSafeUntil = World->GetTimeSeconds() + 2.f; }
		}
		if (Under > 0)
		{
			Total += A->Lose(FMath::Min(MaxKills - Total, Under));
		}
		if (Total >= MaxKills)
		{
			return Total;
		}
	}
	for (TActorIterator<AAntChain> It(World); It; ++It)
	{
		Total += It->KillArmyInSphere(Center, R);
	}
	return Total;
}

void AAntArmy::KillInCone(UWorld* World, const FVector& Origin, const FVector& Dir, float Range, float CosHalfAngle, float Dt)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<AAntArmy> It(World); It; ++It)
	{
		// The spray hits the part of the army in the cone (sampled along the cone axis).
		int32 InCone = 0;
		const float Tan = FMath::Tan(FMath::Acos(FMath::Clamp(CosHalfAngle, 0.1f, 1.f)));
		for (float D = Range * 0.2f; D <= Range + 1.f; D += Range * 0.2f)
		{
			const float Rad = FMath::Max(60.f, D * Tan);
			InCone += It->CountNear(Origin + Dir * D, Rad, Rad + 100.f);
		}
		if (InCone > 0)
		{
			// (a squad is a dozen ants, not thousands: the spray takes them a few a second)
			It->StealAccum += (It->bSquad ? (It->bArmor ? 1.5f : 3.f) : 60.f + FMath::Min(InCone, It->Count) * 0.25f) * Dt;   // reuse accumulator: spray deaths
			const int32 N = FMath::FloorToInt(It->StealAccum);
			It->StealAccum -= N;
			It->Lose(N);
		}
	}
}

void AAntArmy::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority())
	{
		Life += Dt;
		const FVector Me = GetActorLocation();
		FVector Goal = FVector(Target);
		if (bSquad)
		{
			// A raid squad walks, carries and waits by its own rules (AntSquad.cpp).
			if (!SquadTick(Dt, Goal))
			{
				return;
			}
		}
		else
		{
			if (!Commander && Mode == EArmyMode::Attack && Life > 75.f)
			{
				Disband();   // AI raid is over
			}
			// Where to go.
			if (Mode == EArmyMode::Follow)
			{
				if (!IsValid(Commander) || Commander->bDead)
				{
					Disband();
				}
				else
				{
					Goal = Commander->GetActorLocation() - Commander->GetActorForwardVector() * (Radius() + 60.f * AAntCharacter::AntShrink);
				}
			}
			Goal = GroundAt(Goal);
			ServerMove(Goal, Dt);
		}
		// Column while marching; a swarm when there, fighting or waiting.
		const bool bMovedNow = FVector::DistSquared(GetActorLocation(), Me) > FMath::Square(40.f * Dt);
		StillTime = bMovedNow ? 0.f : StillTime + Dt;
		FightHeat = FMath::Max(0.f, FightHeat - Dt);
		const bool bArrived = (Mode == EArmyMode::Attack || Mode == EArmyMode::Hold) && FVector::Dist(Me, Goal) < Radius() * 1.2f;
		bSwarming = bArrived || FightHeat > 0.f || StillTime > (bSquad ? 0.6f : 1.5f) || (bSquad && (bCargoHeld || bOrphan));

		if (Mode == EArmyMode::Home && FVector::Dist2D(Me, Goal) < 300.f && FMath::Abs(Me.Z - Goal.Z) < 150.f)
		{
			// Back in the nest: the workers return to the colony (in war: to the war pool).
			const AAntGameState* WGS = GetWorld()->GetGameState<AAntGameState>();
			AAntKitchenGameMode* WGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
			if (WGS && WGS->bWarMode && WGM)
			{
				WGM->AddPool(Team, Count);
			}
			else if (Team == 0)
			{
				if (AAntSwarm* S = AAntSwarm::Get(GetWorld())) { S->Idle += Count; }
			}
			else if (AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>())
			{
				GS->RivalWorkers += Count;
			}
			Destroy();
			return;
		}
		FightTimer += Dt;
		if (FightTimer >= 0.25f)
		{
			ServerFight(FightTimer);
			FightTimer = 0.f;
		}
	}

	if (GetNetMode() == NM_DedicatedServer || !Bodies)
	{
		return;
	}
	// Draw: a living carpet of ants milling around the army's center.
	const FVector Here = GetActorLocation();
	const FVector Vel = (Here - LastPos) / FMath::Max(Dt, 0.001f);
	LastPos = Here;
	if (Vel.SizeSquared2D() > 400.f)
	{
		MoveDir = FMath::VInterpNormalRotationTo(MoveDir, Vel.GetSafeNormal2D(), Dt, 360.f);
	}
	const int32 Want = FMath::Min(Count, 700);
	if (Want != DrawnFor)
	{
		RebuildVisual();
	}
	const float T = GetWorld()->GetTimeSeconds();
	const bool bMoving = Vel.SizeSquared2D() > 400.f;
	// The rustling carpet of thousands of legs.
	if (!March)
	{
		if (USoundBase* S = AntSfx::Get(AntSfx::ArmyMarch))
		{
			March = UGameplayStatics::SpawnSoundAttached(S, RootComponent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
				true, 0.5f, 1.f, 0.f, AntSfx::Attenuation(), nullptr, true);
		}
	}
	if (March)
	{
		March->SetVolumeMultiplier(FMath::Clamp(0.08f + Count / 1500.f, 0.08f, 0.5f) * (bMoving ? 1.f : 0.3f));
	}
	PrepareLayout();
	TArray<FTransform> Xf;
	Xf.SetNum(Offsets.Num());
	if (Cur.Num() != Offsets.Num())
	{
		Cur.Init(FVector(FLT_MAX), Offsets.Num());
	}
	const float Blend = 1.f - FMath::Exp(-5.f * Dt);
	const float Yaw = (float)MoveDir.Rotation().Yaw;
	for (int32 i = 0; i < Offsets.Num(); ++i)
	{
		const float Ph = Phases[i];
		FVector Dir = MoveDir;
		FVector P;
		float Lift = 0.f;
		// a worker holding a load (AntCarry) stands at its grip, facing it; dragged, it hangs nose-up, feet off the floor
		const bool bGrip = bSquad && AntCarry::WorkerPose(this, i, P, Dir, Lift);
		if (!bGrip) { P = SlotPos(i, Offsets.Num(), &Dir); }
		FRotator Rot;
		if (bGrip)
		{
			Rot = FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).Rotator();
			Rot.Pitch += Lift * 35.f + FMath::Sin(T * 9.f + Ph * 20.f) * 4.f;
		}
		else if (bSwarming)
		{
			// Milling around: each ant wanders in a small loop around its spot.
			P += FVector(FMath::Cos(T * 2.f + Ph * 6.28f) * 14.f, FMath::Sin(T * 2.3f + Ph * 6.28f) * 14.f, 0.f) * AAntCharacter::AntShrink;
			Rot = FRotator(0.f, bMoving ? Yaw + FMath::Sin(T * 3.f + Ph * 10.f) * 15.f : Ph * 360.f + T * 40.f * (Ph > 0.5f ? 1.f : -1.f), 0.f);
		}
		else
		{
			// In the column: a little weave, facing along the path (head up when climbing a wall).
			const bool bVertical = FMath::Abs(Dir.Z) > 0.8f;
			const FVector Side = FVector::CrossProduct(Dir, bVertical ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
			P += Side * FMath::Sin(T * 5.f + Ph * 20.f) * 5.f * AAntCharacter::AntShrink;
			Rot = FRotationMatrix::MakeFromXZ(Dir, bVertical ? FVector::CrossProduct(Side, Dir) : FVector::UpVector).Rotator();
		}
		if (FVector::DistSquared(Cur[i], P) > FMath::Square(1500.f))
		{
			Cur[i] = P;   // first frame / teleport
		}
		Cur[i] = FMath::Lerp(Cur[i], P, bGrip ? 1.f - FMath::Exp(-30.f * Dt) : Blend);
		Xf[i] = FTransform(Rot, Cur[i] + FVector(0.f, 0.f, 4.f * AAntCharacter::AntShrink), FVector(InstScale));
	}
	if (Xf.Num() > 0)
	{
		Bodies->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
	}
}

void AAntArmy::PushTrail(const FVector& P)
{
	if (Trail.IsValidIndex(TrailHead) && FVector::DistSquared(FVector(Trail[TrailHead]), P) < FMath::Square(TrailStep))
	{
		return;
	}
	TrailHead = (TrailHead + 1) % TrailMax;
	if (Trail.Num() <= TrailHead)
	{
		Trail.SetNum(TrailHead + 1);
	}
	Trail[TrailHead] = P;
}

void AAntArmy::PrepareLayout()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (LayoutTime == Now && Path.Num() > 0)
	{
		return;
	}
	LayoutTime = Now;
	Path.Reset();
	PathDist.Reset();
	Path.Add(GetActorLocation());
	PathDist.Add(0.f);
	const int32 N = Trail.Num();
	if (Trail.IsValidIndex(TrailHead))
	{
		for (int32 k = 0; k < N; ++k)
		{
			const FVector P = Trail[((TrailHead - k) % N + N) % N];
			const float Step = (float)FVector::Dist(P, Path.Last());
			if (Step < 1.f)
			{
				continue;
			}
			if (Step > TrailStep * 6.f)
			{
				break;   // a jump (respawn / ring wrap): the column ends here
			}
			Path.Add(P);
			PathDist.Add(PathDist.Last() + Step);
		}
	}
}

FVector AAntArmy::SlotPos(int32 i, int32 N, FVector* OutDir) const
{
	const FVector Head = GetActorLocation();
	N = FMath::Max(N, 1);
	if (bSquad && bSwarming && FightHeat <= 0.f)
	{
		if (OutDir) { *OutDir = MoveDir; }
		if (bCargoHeld && Cargo)
		{
			// Carrying: a ring of workers under the load.
			const float R = Cargo->GetRadius() * 0.75f + 4.f;
			const float A = i * 2.f * PI / N;
			return FVector(Head.X + FMath::Cos(A) * R, Head.Y + FMath::Sin(A) * R, Head.Z);
		}
		if (Mode == EArmyMode::Follow && IsValid(Commander))
		{
			// Standing with its commander: a crescent behind him, open in the middle (the camera looks along that line).
			const FVector Fwd = Commander->GetActorForwardVector().GetSafeNormal2D(), Right = FVector::CrossProduct(FVector::UpVector, Fwd);
			const int32 k = i / 2, Row = k / 4;
			const float Side = (i % 2 == 0) ? 1.f : -1.f;
			const float Ang = (0.50f + 0.27f * (k % 4)) * Side;
			const float R = 30.f + 11.f * Row;
			const FVector At = Commander->GetActorLocation() + (-Fwd * FMath::Cos(Ang) + Right * FMath::Sin(Ang)) * R;
			return FVector(At.X, At.Y, Head.Z);
		}
	}
	if (bSwarming || Path.Num() < 2)
	{
		// Sunflower spread: even, deterministic, no clumps.
		const float D = Radius() * FMath::Sqrt((i + 0.5f) / N);
		const float A = i * 2.39996f;
		if (OutDir) { *OutDir = MoveDir; }
		return FVector(Head.X + FMath::Cos(A) * D, Head.Y + FMath::Sin(A) * D, Head.Z);
	}
	const int32 Lanes = N <= 120 ? 2 : (N <= 400 ? 3 : 4);
	const int32 Row = i / Lanes;
	const int32 Lane = i % Lanes;
	const float Total = PathDist.Last();
	float D = (20.f + Row * 26.f) * AAntCharacter::AntShrink;
	float Spill = 0.f;
	if (D > Total)
	{
		Spill = D - Total;   // more ants than trail: they bunch up at the tail (still pouring out)
		D = Total;
	}
	int32 Seg = 1;
	while (Seg < PathDist.Num() - 1 && PathDist[Seg] < D)
	{
		++Seg;
	}
	const float L0 = PathDist[Seg - 1], L1 = PathDist[Seg];
	const float A = L1 > L0 ? (D - L0) / (L1 - L0) : 0.f;
	const FVector P = FMath::Lerp(Path[Seg - 1], Path[Seg], A);
	FVector Dir = (Path[Seg - 1] - Path[Seg]).GetSafeNormal();   // older -> newer = direction of travel
	if (Dir.IsNearlyZero()) { Dir = MoveDir; }
	if (OutDir) { *OutDir = Dir; }
	const FVector Side = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) > 0.8f ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
	FVector Out = P + Side * ((Lane - (Lanes - 1) * 0.5f) * 17.f * AAntCharacter::AntShrink);
	if (Spill > 0.f)
	{
		const float H = FMath::Frac(FMath::Abs(FMath::Sin(i * 12.9898f)) * 43758.5453f);
		const float Rad = FMath::Min(Spill * 0.15f, 250.f * AAntCharacter::AntShrink) * FMath::Sqrt(H);
		Out += FVector(FMath::Cos(i * 2.39996f), FMath::Sin(i * 2.39996f), 0.f) * Rad;
	}
	return Out;
}

int32 AAntArmy::CountNear(const FVector& C, float R, float ZTol)
{
	if (Count <= 0)
	{
		return 0;
	}
	PrepareLayout();
	const float Reach = (bSwarming ? Radius() : PathDist.Last() + 100.f) + R;
	if (FVector::DistSquared2D(C, GetActorLocation()) > FMath::Square(Reach))
	{
		return 0;
	}
	const int32 Samples = FMath::Min(Count, 160);
	const float Per = (float)Count / Samples;
	int32 Hits = 0;
	for (int32 k = 0; k < Samples; ++k)
	{
		const FVector P = SlotPos(FMath::Min(Count - 1, FMath::FloorToInt(k * Per)), Count);
		if (FVector::DistSquared2D(P, C) < R * R && FMath::Abs(P.Z - C.Z) < ZTol)
		{
			++Hits;
		}
	}
	return FMath::Min(Count, FMath::RoundToInt(Hits * Per));
}

AAntChain* AAntArmy::BuildBridge(const FVector& From, const FVector& To)
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	const int32 Need = FMath::CeilToInt((float)FVector::Dist(From, To) / AAntChain::LinkLen) + 1;
	// A squad: every worker stands for several links (they stretch out: «Длинные мосты» triples that); a slope needs «Живой пандус».
	const int32 Workers = bSquad ? FMath::Max(1, FMath::CeilToInt((float)Need / FMath::Max(1, BridgeMul))) : Need;
	if (bSquad && !bRamps && FMath::Abs((To - From).GetSafeNormal().Z) > 0.34f)
	{
		if (Commander) { Commander->Toast(TEXT("Вверх и вниз мост не построить: это «Живой пандус» (Инженеры III). Целься через провал, на тот же уровень")); }
		return nullptr;
	}
	if (Need > AAntChain::MaxLinks || Count < Workers)
	{
		if (Commander)
		{
			Commander->Toast(FString::Printf(TEXT("Мало муравьёв для моста: нужно %d, в армии %d"), Workers, Count));
		}
		return nullptr;
	}
	AAntChain* C = AAntChain::Start(GetWorld(), Team, nullptr, From, To - From, true, To);
	if (!C)
	{
		return nullptr;
	}
	C->ArmyLinks = Need;
	C->Builder = Commander;
	C->MaxLife = 120.f;
	C->WorkersIn = bSquad ? Workers : 0;
	C->LinksAtStart = Need;
	Count -= Workers;
	if (bSquad && Commander) { Commander->Toast(FString::Printf(TEXT("Мост из %d рабочих: стоит 2 минуты, потом они вернутся в отряд"), Workers)); }
	AntSfx::Everyone(this, AntSfx::ArmyCharge, From, 1.2f);
	if (Count <= 0)
	{
		Destroy();
	}
	return C;
}

void AAntArmy::ServerMove(const FVector& Goal, float Dt, float SpeedMul)
{
	const FVector Me = GetActorLocation();
	const FVector Step = AKitchenBuilder::Route(Me, Goal);
	const float Speed = (Mode == EArmyMode::Home ? 700.f : 560.f) * AAntCharacter::SpeedK * SpeedMul;
	const FVector To = Step - Me;
	const float Flat = (float)To.Size2D();
	FVector New = Me;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArmyMove), false, this);
	if (Flat < 120.f * AAntCharacter::AntShrink && To.Z > 60.f * AAntCharacter::AntShrink)
	{
		// Right under the target (a table top, a sill): straight up the leg / wall.
		New.Z = FMath::Min(Step.Z, Me.Z + Speed * 0.8f * Dt);
		New.X += To.X * FMath::Min(1.f, Dt * 2.f);
		New.Y += To.Y * FMath::Min(1.f, Dt * 2.f);
	}
	else if (Flat > 1.f)
	{
		const FVector Dir2 = FVector(To.X, To.Y, 0.f) / Flat;
		FHitResult Wall;
		const FVector Eye = Me + FVector(0.f, 0.f, 25.f * AAntCharacter::AntShrink);
		const bool bWall = GetWorld()->LineTraceSingleByObjectType(Wall, Eye, Eye + Dir2 * 70.f * AAntCharacter::AntShrink, FCollisionObjectQueryParams(ECC_WorldStatic), Q)
			&& FMath::Abs(Wall.ImpactNormal.Z) < 0.5f;
		if (bWall && Step.Z > Me.Z - 100.f * AAntCharacter::AntShrink && Me.Z < 6000.f)
		{
			// An obstacle in the way: climb it like ants do.
			New.Z += Speed * 0.8f * Dt;
		}
		else
		{
			New += Dir2 * FMath::Min(Flat, Speed * Dt);
			const float G = GroundAt(FVector(New.X, New.Y, Me.Z)).Z;
			// Small steps snap; drops are walked down (down the wall / the leg).
			New.Z = G > Me.Z - 30.f * AAntCharacter::AntShrink ? G : FMath::Max(G, Me.Z - Speed * 1.4f * Dt);
		}
	}
	else
	{
		New.Z = FMath::FInterpConstantTo(Me.Z, Step.Z, Dt, Speed);
	}
	SetActorLocation(New);
	PushTrail(New);
}

void AAntArmy::RebuildVisual()
{
	const int32 Want = FMath::Min(Count, 700);
	DrawnFor = Want;
	Bodies->ClearInstances();
	Offsets.Reset();
	Phases.Reset();
	FRandomStream Rng(GetUniqueID());
	TArray<FTransform> Xf;
	for (int32 i = 0; i < Want; ++i)
	{
		const float A = Rng.FRand() * 2.f * PI;
		const float D = FMath::Sqrt(Rng.FRand());
		Offsets.Add(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f));
		Phases.Add(Rng.FRand());
		Xf.Add(FTransform(FVector::ZeroVector));
	}
	if (Xf.Num() > 0)
	{
		Bodies->AddInstances(Xf, false, false);
	}
	// Scale the body to a worker (~28 long; the player ant is ~60).
	if (UStaticMesh* M = Bodies->GetStaticMesh())
	{
		const float Len = (float)M->GetBoundingBox().GetSize().GetMax();
		InstScale = Len > 0.f ? 28.f * AAntCharacter::AntShrink / Len : 0.1f;
	}
}

void AAntArmy::ServerFight(float Dt)
{
	const FVector Me = GetActorLocation();
	const float R = Radius();
	const float Power = FMath::Sqrt((float)Count);   // big armies hit hard but not linearly

	// Enemy ants (other colony, flies, the spider) inside the swarm get bitten to pieces.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* E = *It;
		if (!E || E->bDead || E->Team == Team || E->IsInNest())
		{
			continue;
		}
		const int32 Biters = CountNear(E->GetActorLocation(), 90.f * AAntCharacter::AntShrink, 160.f * AAntCharacter::AntShrink);
		if (Biters > 0)
		{
			FightHeat = 3.f;
			E->LastHazard = TEXT("загрызен армией муравьёв");
			E->LastHazardTime = GetWorld()->GetTimeSeconds();
			UGameplayStatics::ApplyDamage(E, (6.f + FMath::Sqrt((float)Biters) * 2.2f) * BiteMul * Dt, Commander ? Commander->GetController() : nullptr, this, UDamageType::StaticClass());
			if (E->Carrying) { E->LetGo(); }
			// They fight back. (A squad: one enemy ant kills a worker every three seconds or so, not eight a second.)
			StealAccum += (bSquad ? (E->Creature == 2 ? 1.5f : 0.35f) : (E->Creature == 2 ? 30.f : 8.f)) * Dt;
		}
	}
	// The Human's feet.
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* H = *It;
		const FVector Feet = H->GetActorLocation() - FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight());
		const int32 OnFeet = H->IsKnockedOut() ? 0 : CountNear(Feet, H->GetSimpleCollisionRadius() + 150.f, 120.f);
		if (OnFeet > 0)
		{
			FightHeat = 3.f;
			UGameplayStatics::ApplyDamage(H, (4.f + Power * 1.2f) * Dt, Commander ? Commander->GetController() : nullptr, this, UDamageType::StaticClass());
		}
	}
	// Other armies: both sides grind each other down.
	for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
	{
		AAntArmy* O = *It;
		if (O == this || O->Team == Team)
		{
			continue;
		}
		// Where the two armies touch.
		const int32 Clash = FMath::Max(O->CountNear(Me, R + 60.f * AAntCharacter::AntShrink, 100.f), CountNear(O->GetActorLocation(), O->Radius() + 60.f * AAntCharacter::AntShrink, 100.f));
		if (Clash > 0)
		{
			FightHeat = 3.f;
			StealAccum += FMath::Sqrt((float)FMath::Min(Clash, O->Count)) * 2.f * Dt;
		}
	}
	const int32 Lost = FMath::FloorToInt(StealAccum);
	StealAccum -= Lost;
	if (Lost > 0 && Lose(Lost) > 0 && Count <= 0)
	{
		return;
	}

	AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	// Raid: inside the other colony's nest, carry their sweetness home.
	const uint8 Other = Team == 0 ? 1 : 0;
	if (bSquad)
	{
		return;   // a raid squad neither robs a nest's score nor eats food on the spot: loot counts when it is carried home
	}
	if (GS && GM && Team < 2 && FVector::Dist2D(Me, AKitchenBuilder::NestLocation(Other)) < AKitchenBuilder::NestRadius(Other) + R)
	{
		int32& TheirScore = Other == 0 ? GS->TeamScore : GS->RivalScore;
		RaidAccum += (2.f + Power * 0.35f) * Dt;
		const int32 Take = FMath::Min(TheirScore, FMath::FloorToInt(RaidAccum));
		if (Take > 0)
		{
			RaidAccum -= Take;
			TheirScore -= Take;
			GM->AddScore(Team, Take);
			if (GetWorld()->GetTimeSeconds() > NextRaidMsg)
			{
				NextRaidMsg = GetWorld()->GetTimeSeconds() + 6.f;
				GM->Broadcast(FString::Printf(TEXT("%s ГРАБИТ муравейник %s! (%d муравьёв)"), Team == 0 ? TEXT("Армия колонии") : TEXT("Армия огненных"),
					Other == 0 ? TEXT("колонии") : TEXT("огненных"), Count), FLinearColor(1.f, 0.6f, 0.2f));
			}
		}
	}
	// Food under the army is eaten on the spot (and counts for the colony).
	if (GM && Team < 2)
	{
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			AKitchenLoot* L = *It;
			if (!L || L->bIsGear || L->bVault || L->bHeld || FVector::Dist(L->GetActorLocation(), Me) > R + L->GetRadius())
			{
				continue;
			}
			EatAccum += (1.f + Power * 0.25f) * Dt;
			const int32 Bite = FMath::Min(L->Value, FMath::FloorToInt(EatAccum));
			if (Bite > 0)
			{
				EatAccum -= Bite;
				L->Value -= Bite;
				GM->AddScore(Team, Bite);
				if (L->Value <= 0) { L->ReleaseAll(); L->Destroy(); }
				else { L->OnRep_Visual(); }
			}
			break;
		}
	}
}
