#include "AntArmy.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntSwarm.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
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
}

void AAntArmy::BeginPlay()
{
	Super::BeginPlay();
	LastPos = GetActorLocation();
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
	if (GetWorld()->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 150.f), P - FVector(0.f, 0.f, 6000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 4.f);
	}
	return P;
}

int32 AAntArmy::Lose(int32 N)
{
	const int32 Dead = FMath::Clamp(N, 0, Count);
	Count -= Dead;
	if (Team == 0 && Dead > 0)
	{
		if (AAntSwarm* S = AAntSwarm::Get(GetWorld())) { S->Population = FMath::Max(0, S->Population - Dead); }
	}
	if (Count <= 0)
	{
		Destroy();
	}
	return Dead;
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
		const float D = (float)FVector::Dist(A->GetActorLocation(), Center);
		const float AR = A->Radius();
		if (D < R + AR)
		{
			// The share of the blob under the foot dies.
			const float Overlap = FMath::Clamp((R + AR - D) / (2.f * AR), 0.f, 1.f);
			Total += A->Lose(FMath::Min(MaxKills, FMath::CeilToInt(A->Count * Overlap)));
		}
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
		const FVector To = It->GetActorLocation() - Origin;
		const float D = (float)To.Size();
		if (D < Range && D > 1.f && FVector::DotProduct(To / D, Dir) > CosHalfAngle)
		{
			It->StealAccum += (60.f + It->Count * 0.25f) * Dt;   // reuse accumulator: spray deaths
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
		if (!Commander && Mode == EArmyMode::Attack && Life > 75.f)
		{
			Disband();   // AI raid is over
		}
		// Where to go.
		FVector Goal = FVector(Target);
		if (Mode == EArmyMode::Follow)
		{
			if (!IsValid(Commander) || Commander->bDead)
			{
				Disband();
			}
			else
			{
				Goal = Commander->GetActorLocation() - Commander->GetActorForwardVector() * (Radius() + 60.f);
			}
		}
		Goal = GroundAt(Goal);
		const FVector Me = GetActorLocation();
		const float Speed = Mode == EArmyMode::Home ? 700.f : 560.f;
		const FVector New = FMath::VInterpConstantTo(Me, Goal, Dt, Speed);
		SetActorLocation(FVector(New.X, New.Y, GroundAt(New).Z));

		if (Mode == EArmyMode::Home && FVector::Dist2D(Me, Goal) < 300.f)
		{
			// Back in the nest: the workers return to the colony.
			if (Team == 0)
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
	TArray<FTransform> Xf;
	Xf.SetNum(Offsets.Num());
	const float R = Radius();
	const float Yaw = (float)MoveDir.Rotation().Yaw;
	for (int32 i = 0; i < Offsets.Num(); ++i)
	{
		const float Ph = Phases[i];
		// Each ant wanders in a small loop around its spot; while marching they all face forward.
		const FVector Loop(FMath::Cos(T * 2.f + Ph * 6.28f) * 14.f, FMath::Sin(T * 2.3f + Ph * 6.28f) * 14.f, 0.f);
		const FVector P = Offsets[i] * R + Loop;
		const float AntYaw = bMoving ? Yaw + FMath::Sin(T * 3.f + Ph * 10.f) * 15.f : Ph * 360.f + T * 40.f * (Ph > 0.5f ? 1.f : -1.f);
		Xf[i] = FTransform(FRotator(0.f, AntYaw, 0.f), FVector(P.X, P.Y, 6.f), FVector(InstScale));
	}
	Bodies->BatchUpdateInstancesTransforms(0, Xf, false, true, true);
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
		InstScale = Len > 0.f ? 28.f / Len : 0.3f;
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
		if (FVector::Dist(E->GetActorLocation(), Me) < R + 40.f)
		{
			E->LastHazard = TEXT("загрызен армией муравьёв");
			E->LastHazardTime = GetWorld()->GetTimeSeconds();
			UGameplayStatics::ApplyDamage(E, (6.f + Power * 1.6f) * Dt, Commander ? Commander->GetController() : nullptr, this, UDamageType::StaticClass());
			if (E->Carrying) { E->LetGo(); }
			// They fight back.
			StealAccum += (E->Creature == 2 ? 30.f : 8.f) * Dt;
		}
	}
	// The Human's feet.
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* H = *It;
		const FVector Feet = H->GetActorLocation() - FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight());
		if (!H->IsKnockedOut() && FVector::Dist2D(Feet, Me) < R + H->GetSimpleCollisionRadius() + 150.f && FMath::Abs(Feet.Z - Me.Z) < 300.f)
		{
			UGameplayStatics::ApplyDamage(H, (4.f + Power * 1.2f) * Dt, Commander ? Commander->GetController() : nullptr, this, UDamageType::StaticClass());
		}
	}
	// Other armies: both sides grind each other down.
	for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
	{
		AAntArmy* O = *It;
		if (O != this && O->Team != Team && FVector::Dist(O->GetActorLocation(), Me) < R + O->Radius())
		{
			StealAccum += FMath::Sqrt((float)O->Count) * 2.f * Dt;
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
			if (!L || L->bIsGear || L->bHeld || FVector::Dist(L->GetActorLocation(), Me) > R + L->GetRadius())
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
