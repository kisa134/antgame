#include "KitchenLoot.h"
#include "AntFx.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntKitchenGameMode.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AntGameState.h"
#include "KitchenInteractables.h"
#include "TimerManager.h"
#include "KitchenShards.h"
#include "AntRun.h"
#include "AntArmy.h"
#include "HumanCharacter.h"
#include "ProceduralMeshComponent.h"

AKitchenLoot::AKitchenLoot()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);   // physics: server simulates, clients follow
	SetNetUpdateFrequency(30.f);

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(20.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // food turns physics on in SetupPhysics
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);        // carriers hold it from inside
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCanEverAffectNavigation(false);
	Body->SetGenerateOverlapEvents(false);
	RootComponent = Body;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Body);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetGenerateOverlapEvents(false);
	Beam->SetCanEverAffectNavigation(false);
	Beam->SetCastShadow(false);
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	Beam->SetRelativeScale3D(FVector(0.04f, 0.04f, 4.f));
	Beam->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Body);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Glow->IntensityUnits = ELightUnits::Unitless;
	Glow->bUseInverseSquaredFalloff = false;
	Glow->LightFalloffExponent = 2.f;
	Glow->Intensity = 3.f;
	Glow->AttenuationRadius = 320.f;
	Glow->CastShadows = false;
	Glow->SetVisibility(false);
}

void AKitchenLoot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenLoot, LootName);
	DOREPLIFETIME(AKitchenLoot, Value);
	DOREPLIFETIME(AKitchenLoot, Weight);
	DOREPLIFETIME(AKitchenLoot, Color);
	DOREPLIFETIME(AKitchenLoot, ShapeIndex);
	DOREPLIFETIME(AKitchenLoot, VisualScale);
	DOREPLIFETIME(AKitchenLoot, bIsGear);
	DOREPLIFETIME(AKitchenLoot, Gear);
	DOREPLIFETIME(AKitchenLoot, Carriers);
	DOREPLIFETIME(AKitchenLoot, CarryStrength);
	DOREPLIFETIME(AKitchenLoot, CarrierTeam);
	DOREPLIFETIME(AKitchenLoot, bHeld);
	DOREPLIFETIME(AKitchenLoot, bVault);
	DOREPLIFETIME(AKitchenLoot, bTrophy);
	DOREPLIFETIME(AKitchenLoot, bBody);
	DOREPLIFETIME(AKitchenLoot, ShardPath);
	DOREPLIFETIME(AKitchenLoot, ShardSeed);
	DOREPLIFETIME(AKitchenLoot, ShardScale);
}

float AKitchenLoot::GetRadius() const
{
	if (bIsGear)
	{
		return 30.f;
	}
	return 0.5f * (32.f + Weight * 3.6f);   // ants got smaller: so did the crumbs
}

void AKitchenLoot::InitFood(const FAntFoodDef& Def)
{
	LootName = Def.Name;
	Value = Def.Value;
	Weight = Def.Weight;
	Color = Def.Color;
	ShapeIndex = (uint8)Def.Shape;
	VisualScale = Def.Scale;
	bIsGear = false;
}

void AKitchenLoot::InitGear(const FAntWeapon& InWeapon)
{
	bIsGear = true;
	Gear = InWeapon;
	LootName = InWeapon.Name;
	Value = 0;
	Weight = 0;
	Color = AntKit::RarityColor(InWeapon.Rarity);
	ShapeIndex = (uint8)EAntShape::Cube;
	VisualScale = FVector(0.35f, 0.35f, 0.25f);
}

void AKitchenLoot::BeginPlay()
{
	Super::BeginPlay();
	Phase = FMath::FRand() * 6.f;
	RefreshVisual();
	if (HasAuthority() && !bIsGear)
	{
		BreakableAfter = GetWorld()->GetTimeSeconds() + 2.f;
		if (!bOriginSet)
		{
			// Remember where it lay: in which container (for the Queen's orders) and in which room.
			bOriginSet = true;
			Origin = (uint8)(KitchenContainers::At(GetActorLocation()) + 1);
			bFromLiving = GetActorLocation().X > 7100.f;
		}
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &AKitchenLoot::OnBodyHit);
	}
}

// ---------------------------------------------------------------------------------------------
// Breaking (server)
// ---------------------------------------------------------------------------------------------

void AKitchenLoot::OnBodyHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bIsGear || bShatterPending)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float ImpactSpeed = FMath::Max(0.f, -(float)FVector::DotProduct(LastVelocity, Hit.ImpactNormal));
	if (ImpactSpeed > 450.f && Now > NextThud)
	{
		NextThud = Now + 0.3f;
		AntSfx::Everyone(this, AntSfx::Drop, Hit.ImpactPoint, FMath::Clamp(1.6f - Weight * 0.02f, 0.6f, 1.6f));
	}
	// Something heavy coming down: a crash the Human hears, a blow if it lands on him, the end of an enemy ant under it.
	if (Weight >= 8 && !IsCarried() && !bHeld && Now > BreakableAfter)
	{
		AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
		AAntCharacter* By = LastCarrier.Get();
		const float Speed = (float)LastVelocity.Size();
		if (AHumanCharacter* H = Cast<AHumanCharacter>(Other))
		{
			// Only what ants let fall on him from above counts: not food his own stomp kicked up, not what he threw.
			if (By && Now - LastKickTime > 4.f && Speed > 600.f && LastVelocity.Z < -400.f)
			{
				H->Stun(2.5f, By, LootName, Hit.ImpactPoint.Z < H->GetActorLocation().Z - 900.f ? 1 : 0);   // low: it landed on his foot
			}
		}
		else if (ImpactSpeed > 600.f)
		{
			const uint8 Friends = By ? By->Team : 0;
			for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
			{
				AAntCharacter* A = *It;
				if (A && !A->bDead && A->Creature == 0 && A->Team != Friends && !A->IsInNest()
					&& FVector::Dist(A->GetActorLocation(), Hit.ImpactPoint) < GetRadius() + 14.f)
				{
					UE_LOG(LogTemp, Display, TEXT("ANTCRUSH %s crushed by falling %s"), *A->GetDisplayName(), *LootName);
					A->HurtBy(300.f, FString::Printf(TEXT("раздавлен: упал «%s»"), *LootName));
				}
			}
			if (GM && By && ImpactSpeed > 800.f && Now > NextCrash)   // somebody dropped it (food settling on its own is no event)
			{
				NextCrash = Now + 1.5f;
				GM->MakeNoise(Hit.ImpactPoint, FMath::Clamp(0.35f + Weight * 0.012f, 0.4f, 0.9f), AntNoise::Crash, nullptr, By);
			}
		}
	}
	// Dropped from a counter / the table / the fridge: it smashes. («Мягкая упаковка»: trophies take a harder fall.)
	const float BreakSpeed = (bTrophy && AntCard::Has(this, AntCard::Padding)) ? 1900.f : 1250.f;
	if (CanBreak() && !IsCarried() && Now > BreakableAfter && ImpactSpeed > BreakSpeed)
	{
		bShatterPending = true;
		ShatterAt = Hit.ImpactPoint;
	}
}

AKitchenLoot* AKitchenLoot::SpawnPiece(const FString& Name, int32 PieceValue, int32 PieceWeight, const FVector& Where, const FVector& Fling, int32 ShardIndex, int32 ShardCount, int32 Seed)
{
	FAntFoodDef D;
	D.Name = Name;
	D.Value = FMath::Max(1, PieceValue);
	D.Weight = FMath::Max(1, PieceWeight);
	D.Color = Color;
	D.Shape = (EAntShape)ShapeIndex;
	D.Scale = VisualScale;
	// Chunks keep the orientation of the thing they broke off.
	const FTransform SpawnAt(ShardCount > 0 ? GetActorQuat() : FQuat::Identity, Where);
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), SpawnAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(D);
		L->Origin = Origin;
		L->bFromLiving = bFromLiving;
		L->bOriginSet = true;
		L->LastCarrier = LastCarrier;
		L->ExpireAt = ExpireAt;
		if (ShardCount > 0 && ShardPath.Num() < 6)
		{
			// A chunk of a chunk continues the path; the first break picks the seed and remembers the model's size.
			L->ShardPath = ShardPath;
			L->ShardPath.Add((uint8)ShardIndex);
			L->ShardPath.Add((uint8)ShardCount);
			L->ShardSeed = ShardPath.Num() > 0 ? ShardSeed : Seed;
			L->ShardScale = ShardPath.Num() > 0 ? ShardScale : Mesh->GetRelativeScale3D();
		}
		L->FinishSpawning(SpawnAt);
		L->BreakableAfter = GetWorld()->GetTimeSeconds() + 1.5f;
		L->Kick(Fling, L->GetActorLocation() + FMath::VRand() * 5.f);
	}
	return L;
}

void AKitchenLoot::Shatter(const FVector& At)
{
	if (!HasAuthority() || bIsGear || bVault || IsPendingKillPending())
	{
		return;
	}
	const int32 N = FMath::Clamp(Weight / 5, 2, 6);
	// Ordinary food: pieces are worth a bit more in total (smashing is a strategy). A trophy: the whole-delivery
	// bonus is gone, the pieces add up to the plain value.
	const int32 PieceValue = FMath::CeilToInt(Value * (bTrophy ? 1.f : 1.1f) / N);
	if (bTrophy)
	{
		if (AAntKitchenGameMode* TGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			TGM->TrophyLostAt = GetWorld()->GetTimeSeconds();
			TGM->OnTrophyBroken(this);
		}
		if (AAntGameState* TGS = GetWorld()->GetGameState<AAntGameState>())
		{
			TGS->TrophiesBroken++;
			TGS->MulticastMessage(FString::Printf(TEXT("ТРОФЕЙ РАЗБИТ: «%s» — куски стоят вдвое меньше целого"), *LootName), FLinearColor(1.f, 0.5f, 0.3f));
		}
	}
	const int32 PieceWeight = FMath::Max(2, FMath::RoundToInt((float)Weight / N));
	const FString Base = LootName.StartsWith(TEXT("Кусок")) ? LootName : FString::Printf(TEXT("Кусок: %s"), *LootName);
	const FVector C = GetActorLocation() + FVector(0.f, 0.f, 15.f);
	ReleaseAll();
	const int32 CutSeed = FMath::RandRange(1, 1000000);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = (2.f * PI * i) / N + FMath::FRand();
		const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		SpawnPiece(Base, PieceValue, PieceWeight, C + Out * GetRadius() * 0.5f, Out * FMath::FRandRange(250.f, 550.f) + FVector(0.f, 0.f, FMath::FRandRange(250.f, 500.f)), i, N, CutSeed);
	}
	if (AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>())
	{
		GS->MulticastCrumbs(At, Color, 18, FMath::Clamp(GetRadius() * 0.18f, 4.f, 14.f));
		AntSfx::Everyone(this, AntSfx::Crunch, At, FMath::Clamp(1.3f - Weight * 0.01f, 0.7f, 1.3f));
		if (Value >= 50)
		{
			GS->MulticastMessage(FString::Printf(TEXT("ХРЯСЬ! «%s» разлетелась на %d кусков"), *LootName, N), FLinearColor(1.f, 0.85f, 0.4f));
		}
	}
	Destroy();
}

AKitchenLoot* AKitchenLoot::BiteOff(AAntCharacter* By)
{
	if (!HasAuthority() || bIsGear || bVault || Weight < 9)
	{
		return nullptr;
	}
	const int32 W = 3;
	const int32 V = FMath::Max(1, FMath::RoundToInt((float)Value * W / Weight));
	Weight -= W;
	Value = FMath::Max(1, Value - V);
	OnRep_Visual();
	FVector Dir = By ? (By->GetActorLocation() - GetActorLocation()).GetSafeNormal() : FVector::UpVector;
	const FVector Where = GetActorLocation() + Dir * GetRadius() + FVector(0.f, 0.f, 20.f);
	FString Base = LootName.StartsWith(TEXT("Кусок")) ? LootName : FString::Printf(TEXT("Кусок: %s"), *LootName);
	AKitchenLoot* Piece = SpawnPiece(Base, V, W, Where, Dir * 200.f + FVector(0.f, 0.f, 250.f));
	if (AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>())
	{
		GS->MulticastCrumbs(Where, Color, 7, 4.f);
	}
	return Piece;
}

void AKitchenLoot::TickWater(float Dt)
{
	// Food floats in the sink when the water is up.
	static TWeakObjectPtr<AKitchenFaucet> Faucet;
	if (!Faucet.IsValid() || Faucet->GetWorld() != GetWorld())
	{
		Faucet = nullptr;
		for (TActorIterator<AKitchenFaucet> It(GetWorld()); It; ++It) { Faucet = *It; break; }
	}
	if (!Faucet.IsValid() || !Body->IsSimulatingPhysics())
	{
		return;
	}
	const FVector L = GetActorLocation();
	const FBox B = AKitchenFaucet::Basin();
	if (L.X > B.Min.X && L.X < B.Max.X && L.Y > B.Min.Y && L.Y < B.Max.Y && L.Z < Faucet->WaterLevel && L.Z > B.Min.Z - 50.f)
	{
		const float Depth = FMath::Clamp((float)(Faucet->WaterLevel - L.Z) / FMath::Max(10.f, GetRadius()), 0.f, 1.f);
		const float G = FMath::Abs(GetWorld()->GetGravityZ());
		Body->AddForce(FVector(0.f, 0.f, Body->GetMass() * G * 1.6f * Depth) - Body->GetPhysicsLinearVelocity() * Body->GetMass() * 1.5f);
		if (Faucet->bOn)
		{
			// The stream stirs the water.
			const FVector ToSpout = (AKitchenFaucet::Spout() - L).GetSafeNormal2D();
			Body->AddForce(FVector(-ToSpout.Y, ToSpout.X, 0.f) * Body->GetMass() * 300.f);
		}
	}
}

void AKitchenLoot::OnRep_Visual()
{
	RefreshVisual();
}

void AKitchenLoot::RefreshVisual()
{
	if (!Mesh)
	{
		return;
	}
	const FLinearColor Col = bIsGear ? AntKit::RarityColor(Gear.Rarity) : Color;

	const TCHAR* ArtName = bIsGear ? nullptr : AntArt::FoodArtName(LootName);
	const AntArt::FPart* Art = ArtName ? &AntArt::Get(ArtName) : nullptr;
	if (BuildShard(Art))
	{
		// A chunk cut out of the model: BuildShard set the mesh and the physics body.
	}
	else if (Art && Art->IsValid())
	{
		// Generated food model, a bit bigger than the old shape so it reads well.
		bHasArt = true;
		// Size follows the weight, so heavy things look heavy.
		const float Size = GetRadius() * 2.f;
		const float S = Size / FMath::Max(1.f, (float)Art->Bounds.GetSize().GetMax());
		Mesh->SetStaticMesh(Art->Mesh);
		Mesh->SetMaterial(0, Art->Material);
		Mesh->SetRelativeScale3D(FVector(S));
		ArtOffset = -Art->Bounds.GetCenter() * S;
		MeshMID = nullptr;
		SetupPhysics(Art->Bounds.GetExtent() * S);
	}
	else
	{
		bHasArt = false;
		ArtOffset = FVector::ZeroVector;
		Mesh->SetStaticMesh(AntKit::ShapeMesh((EAntShape)ShapeIndex));
		Mesh->SetRelativeScale3D(bIsGear ? VisualScale : VisualScale * (GetRadius() * 2.f / FMath::Max(10.f, (float)VisualScale.GetMax() * 100.f)));
		if (!MeshMID) { MeshMID = AntKit::Paint(Mesh, Col); }
		else { MeshMID->SetVectorParameterValue(TEXT("Color"), Col); }
		SetupPhysics(Mesh->GetRelativeScale3D() * 50.f);
	}

	const bool bShowBeam = (bIsGear && Gear.Rarity >= 1) || bTrophy;
	const FLinearColor BeamCol = bTrophy ? FLinearColor(1.f, 0.8f, 0.2f) : Col;
	Beam->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cylinder));
	Beam->SetVisibility(bShowBeam);
	if (!BeamMID) { BeamMID = AntKit::Paint(Beam, BeamCol); }
	else { BeamMID->SetVectorParameterValue(TEXT("Color"), BeamCol); }

	const bool bGlow = bIsGear || Value >= 100 || bTrophy;
	Glow->SetLightColor(bTrophy ? FLinearColor(1.f, 0.8f, 0.2f) : Col);
	Glow->SetIntensity(bIsGear ? 2.f + Gear.Rarity * 1.5f : 2.5f);
	Glow->SetVisibility(bGlow);
}

void AKitchenLoot::SetupPhysics(const FVector& HalfExtent)
{
	if (bIsGear)
	{
		return;
	}
	const FVector E = HalfExtent.ComponentMax(FVector(6.f));
	if (bPhysicsReady)
	{
		// Got smaller (bitten): resize the body.
		Body->SetBoxExtent(E, true);
		Body->SetMassOverrideInKg(NAME_None, FMath::Max(1, Weight) * AKitchenLoot::KgPerWeight, true);
		return;
	}
	bPhysicsReady = true;
	Body->SetBoxExtent(E, false);
	// Lift out of the surface it was placed on, then let it fall and settle.
	if (HasAuthority())
	{
		AddActorWorldOffset(FVector(0.f, 0.f, E.Z + 4.f));
	}
	static TWeakObjectPtr<UPhysicalMaterial> FoodMat;
	if (!FoodMat.IsValid())
	{
		UPhysicalMaterial* M = NewObject<UPhysicalMaterial>(GetTransientPackage(), TEXT("PM_Food"));
		M->Friction = 0.5f;           // dragging needs strength > half the weight
		M->StaticFriction = 0.55f;
		M->Restitution = 0.15f;
		M->AddToRoot();
		FoodMat = M;
	}
	Body->SetPhysMaterialOverride(FoodMat.Get());
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, FMath::Max(1, Weight) * AKitchenLoot::KgPerWeight, true);
	Body->SetLinearDamping(0.3f);
	Body->SetAngularDamping(1.5f);
	Body->SetUseCCD(true);
}

void AKitchenLoot::Kick(const FVector& Impulse, const FVector& At)
{
	if (Body && Body->IsSimulatingPhysics())
	{
		Body->AddImpulseAtLocation(Impulse * Body->GetMass(), At);
		if (Impulse.SizeSquared() > 200.f * 200.f)
		{
			LastKickTime = GetWorld()->GetTimeSeconds();
		}
	}
}

FVector AKitchenLoot::HoldPoint(const AAntCharacter* Ant) const
{
	return Ant->GetHoldPoint();
}

bool AKitchenLoot::GrabPointOf(const AAntCharacter* Ant, FVector& Out) const
{
	const int32 i = Carriers.IndexOfByKey(Ant);
	if (!GrabLocal.IsValidIndex(i))
	{
		return false;
	}
	Out = GetActorTransform().TransformPosition(GrabLocal[i]);
	return true;
}

void AKitchenLoot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		if (bShatterPending)
		{
			bShatterPending = false;
			Shatter(ShatterAt);
			return;
		}
		if (ExpireAt > 0.f && GetWorld()->GetTimeSeconds() > ExpireAt)
		{
			ReleaseAll();
			Destroy();
			return;
		}
		ServerCarryTick(DeltaSeconds);
		if (IsPendingKillPending())
		{
			return;   // delivered
		}
		if (!bIsGear && Body->IsSimulatingPhysics())
		{
			LastVelocity = Body->GetPhysicsLinearVelocity();
			TickWater(DeltaSeconds);
		}
	}
	// Ants and the Human bump into food lying around (and can shove it);
	// carriers hold it from inside, so they don't collide with what they carry.
	if (!bIsGear && Body)
	{
		const ECollisionResponse Want = IsCarried() ? ECR_Ignore : ECR_Block;
		if (Body->GetCollisionResponseToChannel(ECC_Pawn) != Want)
		{
			Body->SetCollisionResponseToChannel(ECC_Pawn, Want);
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Phase += DeltaSeconds;
	if (!bIsGear)
	{
		Mesh->SetRelativeLocation(ArtOffset);   // food is a physics body: it tumbles for real
		// Too heavy to lift: it scrapes along the floor, leaving crumbs and a little dust behind.
		const FVector Here = GetActorLocation();
		if (IsCarried() && CarryRatio() < 1.f && !bHeld)
		{
			const float Rad = GetRadius();
			DragFxDist += FMath::Min(60.f, (float)FVector::Dist(Here, DragFxLast));
			if (DragFxDist > Rad * 0.7f + 5.f)
			{
				DragFxDist = 0.f;
				AntFx::DragCrumbs(this, Here - FVector(0.f, 0.f, Rad * 0.8f), Color, Rad);
			}
		}
		else
		{
			DragFxDist = 0.f;
		}
		DragFxLast = Here;
		return;
	}
	Mesh->SetRelativeLocation(ArtOffset + FVector(0.f, 0.f, FMath::Sin(Phase * 2.5f) * 6.f));
	Mesh->AddRelativeRotation(FRotator(0.f, 45.f * DeltaSeconds, 0.f));
}

// ---------------------------------------------------------------------------------------------
// Dragging (server)
// ---------------------------------------------------------------------------------------------

void AKitchenLoot::PickUpBy(USceneComponent* Hand, const FVector& RelativeLocation)
{
	if (!HasAuthority() || bIsGear || !Hand)
	{
		return;
	}
	ReleaseAll();
	bHeld = true;
	OnRep_Held();
	AttachToComponent(Hand, FAttachmentTransformRules::KeepWorldTransform);
	SetActorRelativeLocation(RelativeLocation);
}

void AKitchenLoot::Throw(const FVector& Velocity)
{
	if (!HasAuthority() || !bHeld)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bHeld = false;
	OnRep_Held();
	Body->SetPhysicsLinearVelocity(Velocity);
	Body->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 360.f);
	BreakableAfter = GetWorld()->GetTimeSeconds() + 0.1f;
	LastKickTime = GetWorld()->GetTimeSeconds();
}

void AKitchenLoot::OnRep_Held()
{
	if (!Body || bIsGear || !bPhysicsReady)
	{
		return;
	}
	Body->SetSimulatePhysics(!bHeld);
	Body->SetCollisionEnabled(bHeld ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
}

bool AKitchenLoot::TryGrab(AAntCharacter* Ant, FString& Why)
{
	if (!Ant || bIsGear || Ant->bDead)
	{
		return false;
	}
	if (bHeld)
	{
		Why = TEXT("Это в руке у Человека!");
		return false;
	}
	if (Carriers.Contains(Ant))
	{
		return true;
	}
	if (Carriers.Num() >= 8)
	{
		Why = TEXT("Вокруг уже нет места — 8 муравьёв держат это");
		return false;
	}
	// Bite into the side of the food closest to the ant.
	FVector Closest;
	if (Body->GetClosestPointOnCollision(Ant->GetActorLocation(), Closest) < 0.f)
	{
		Closest = GetActorLocation();
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCARRY grab %s by %s w=%d"), *LootName, *Ant->GetDisplayName(), Weight);
	AntSfx::Everyone(this, AntSfx::Grab, GetActorLocation(), FMath::Clamp(1.4f - Weight * 0.015f, 0.6f, 1.4f));
	Carriers.Add(Ant);
	GrabLocal.Add(GetActorTransform().InverseTransformPosition(Closest));
	Ant->SetCarrying(this);
	ServerCarryTick(0.f);
	return true;
}

void AKitchenLoot::Release(AAntCharacter* Ant)
{
	const int32 Idx = Carriers.Find(Ant);
	if (Idx != INDEX_NONE)
	{
		LastCarrier = Ant;
		Carriers.RemoveAt(Idx);
		GrabLocal.RemoveAt(Idx);
		if (Ant && Ant->Carrying == this)
		{
			Ant->SetCarrying(nullptr);
		}
	}
	if (Carriers.Num() == 0)
	{
		CarryStrength = 0.f;
	}
}

void AKitchenLoot::ReleaseAll()
{
	TArray<AAntCharacter*> Old = Carriers;
	if (Old.Num() > 0 && IsValid(Old[0]))
	{
		LastCarrier = Old[0];
	}
	Carriers.Reset();
	GrabLocal.Reset();
	for (AAntCharacter* A : Old)
	{
		if (IsValid(A) && A->Carrying == this)
		{
			A->SetCarrying(nullptr);
		}
	}
	CarryStrength = 0.f;
}

void AKitchenLoot::ServerCarryTick(float Dt)
{
	if (!bIsGear && GetActorLocation().Z < -900.f)
	{
		ReleaseAll();
		Destroy();   // fell out of the world; refill spawns new food
		return;
	}
	if (Carriers.Num() == 0)
	{
		return;
	}
	const FTransform TM = GetActorTransform();
	const float Leash = 260.f * AAntCharacter::AntShrink + GetRadius();

	// Drop ants that died, let go or got pulled too far away.
	for (int32 i = Carriers.Num() - 1; i >= 0; --i)
	{
		AAntCharacter* A = Carriers[i];
		const bool bValid = IsValid(A) && !A->bDead && A->Carrying == this;
		if (!bValid || FVector::Dist(HoldPoint(A), TM.TransformPosition(GrabLocal[i])) > Leash)
		{
			Carriers.RemoveAt(i);
			GrabLocal.RemoveAt(i);
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY lost %s valid=%d"), *LootName, bValid ? 1 : 0);
			if (IsValid(A) && A->Carrying == this)
			{
				A->SetCarrying(nullptr);
				A->Toast(TEXT("Груз вырвался из жвал!"));
			}
		}
	}
	if (Carriers.Num() == 0)
	{
		CarryStrength = 0.f;
		return;
	}

	// Who is winning (for HUD / movement speed): the team strengths cancel out.
	float Str[2] = { 0.f, 0.f };
	TArray<float, TInlineAllocator<16>> Extra;   // what the army of each carrier adds (the main object only)
	Extra.Init(0.f, Carriers.Num());
	for (int32 i = 0; i < Carriers.Num(); ++i)
	{
		AAntCharacter* A = Carriers[i];
		Str[FMath::Clamp<int32>(A->Team, 0, 1)] += A->GetStrength();   // flies (team 2) count as the second side
		if (bMain)
		{
			const AAntArmy* Army = AAntArmy::FindFor(A);
			if (Army && FVector::Dist(Army->GetActorLocation(), A->GetActorLocation()) < 900.f)
			{
				Extra[i] = FMath::Min(30.f, Army->Count * 1.5f);
				Str[FMath::Clamp<int32>(A->Team, 0, 1)] += Extra[i];
			}
		}
	}
	const int32 Win = Str[1] > Str[0] ? 1 : 0;
	CarrierTeam = (uint8)Win;
	CarryStrength = FMath::Max(0.f, Str[Win] - Str[1 - Win]);
	if (Dt <= 0.f || !Body->IsSimulatingPhysics())
	{
		return;
	}

	// Real physics: every ant is a spring pulling its bite point to its mandibles,
	// limited by its own strength. Strength 1 lifts 1 weight unit against gravity.
	Body->WakeAllRigidBodies();
	const float Mass = Body->GetMass();
	const float Gravity = FMath::Abs(GetWorld()->GetGravityZ());
	const float ForcePerStrength = KgPerWeight * Gravity;
	const int32 N = Carriers.Num();
	// The main object (the birthday cake) is carried by a crowd that is only just strong enough (ratio 1.0 - 1.1). With every
	// ant pulling for itself at the edge of its strength the weak side sagged, the piece ploughed the floor and a door rail
	// 12 uu high stopped it. So: its carriers share the weight evenly (the pooled strength of the side that is winning); once
	// they can carry it at all (ratio >= 1) they grip it as firmly as a lone ant grips a crumb (the springs get the force of
	// ratio 1.75, the walking speed stays that of the real ratio) and hold it 28 uu clear of the ground: over rails, the
	// carpet edge, the garden stones. Dragged (ratio < 1) it behaves like any other load.
	int32 OnWin = 0;
	for (const AAntCharacter* A : Carriers) { OnWin += FMath::Clamp<int32>(A->Team, 0, 1) == Win ? 1 : 0; }
	const bool bLift = bMain && CarryStrength >= (float)Weight;
	const float Pool = (bMain && OnWin > 0) ? (bLift ? FMath::Max(Str[Win], 1.75f * Weight) : Str[Win]) / OnWin * ForcePerStrength : 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		AAntCharacter* A = Carriers[i];
		const bool bPooled = Pool > 0.f && FMath::Clamp<int32>(A->Team, 0, 1) == Win;
		const FVector P = TM.TransformPosition(GrabLocal[i]);
		const FVector H = HoldPoint(A) + FVector(0.f, 0.f, (bLift && bPooled) ? 28.f : 0.f);
		const FVector V = Body->GetPhysicsLinearVelocityAtPoint(P) - A->GetVelocity();
		const float Share = Mass / N;
		FVector F = Share * (40.f * (H - P) - 7.f * V) + FVector(0.f, 0.f, Share * Gravity);   // spring + hold up its share
		const float MaxF = bPooled ? Pool : (A->GetStrength() + Extra[i]) * ForcePerStrength;
		if (bPooled && bLift)
		{
			// A carrier that stands lower than the load (it has stepped down off a shelf, the others are still up there) does
			// not drag it down: gravity does that. (It did: the leader below pulled the front of the cake onto the edge with all
			// its share while the three above could not hold both the weight and him.)
			F.Z = FMath::Max((float)F.Z, 0.f);
		}
		if (F.SizeSquared() > MaxF * MaxF)
		{
			F = F.GetSafeNormal() * MaxF;
		}
		Body->AddForceAtLocation(F, P);
	}

	// Home?
	const FVector L = GetActorLocation();
	// Level v2: our colony has a second way in, the south entrance of the nest at the house corner (through the cat flap and the yard).
	const bool bSouth = CarrierTeam == 0 && KitchenArt::Enabled() && FVector::Dist2D(L, LevelV2::SouthEntrance()) < LevelV2::SouthEntranceRadius() && L.Z < 500.f;
	if (!bVault && CarrierTeam < 2 && (bSouth || (FVector::Dist2D(L, AKitchenBuilder::NestLocation(CarrierTeam)) < AKitchenBuilder::NestRadius(CarrierTeam)
		&& L.Z < ((CarrierTeam == 1 && KitchenArt::Enabled()) ? 800.f : 500.f))))
	{
		if (bSouth)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTV2 delivered at the south entrance: «%s» (%d)"), *LootName, Value);
		}
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			TArray<AAntCharacter*> Team;
			for (AAntCharacter* A : Carriers)
			{
				if (A->Team == CarrierTeam) { Team.Add(A); }
			}
			GM->OnFoodDelivered(CarrierTeam, this, Team);
			AntSfx::Everyone(this, AntSfx::Deliver, L, CarrierTeam == 0 ? 1.f : 0.75f);
		}
		ReleaseAll();
		Destroy();
	}
}

// =============================================================================================
// Cosmetic crumbs
// =============================================================================================

AFoodCrumbs::AFoodCrumbs()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	InitialLifeSpan = 4.f;
}

void AFoodCrumbs::Burst(const FLinearColor& InColor, int32 Count, float Size)
{
	BaseSize = Size;
	for (int32 i = 0; i < Count; ++i)
	{
		const FLinearColor C = InColor * FMath::FRandRange(0.7f, 1.1f);
		UStaticMeshComponent* M = AntKit::AddPart(this, RootComponent, FMath::RandBool() ? EAntShape::Cube : EAntShape::Sphere,
			FMath::VRand() * Size, FVector(Size * FMath::FRandRange(0.5f, 1.3f) / 100.f), C, FRotator(FMath::FRand() * 360.f, FMath::FRand() * 360.f, 0.f));
		M->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		M->SetCollisionObjectType(ECC_PhysicsBody);
		M->SetCollisionResponseToAllChannels(ECR_Ignore);
		M->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		M->SetSimulatePhysics(true);
		M->SetMassOverrideInKg(NAME_None, 0.05f, true);
		M->SetLinearDamping(0.6f);
		M->SetPhysicsLinearVelocity(FMath::VRand() * FMath::FRandRange(150.f, 500.f) + FVector(0.f, 0.f, FMath::FRandRange(200.f, 600.f)));
		M->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 720.f);
		M->SetCastShadow(false);
		Bits.Add(M);
	}
}

void AFoodCrumbs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age > 3.f)
	{
		const float K = FMath::Clamp(4.f - Age, 0.01f, 1.f);
		for (UStaticMeshComponent* M : Bits)
		{
			if (M) { M->SetWorldScale3D(FVector(BaseSize / 100.f * K)); }
		}
	}
}

bool AKitchenLoot::BuildShard(const AntArt::FPart* Art)
{
	const bool bArt = Art && Art->IsValid();
	TSharedPtr<const AntShards::FChunk> Chunk;
	if (!bIsGear && ShardPath.Num() >= 2)
	{
		Chunk = AntShards::Get(this, bArt ? Art->Mesh : nullptr, ShapeIndex, ShardSeed, ShardPath);
	}
	if (!Chunk.IsValid())
	{
		if (ShardPath.Num() >= 2 && !bIsGear) { UE_LOG(LogTemp, Warning, TEXT("Shard: no chunk for %s, plain model used"), *LootName); }
		if (Shard) { Shard->SetVisibility(false); }
		Mesh->SetVisibility(true);
		return false;
	}
	if (!Shard)
	{
		Shard = NewObject<UProceduralMeshComponent>(this);
		Shard->SetupAttachment(Body);
		Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shard->SetGenerateOverlapEvents(false);
		Shard->SetCanEverAffectNavigation(false);
		Shard->RegisterComponent();
	}
	if (ShardBuilt != Chunk)
	{
		ShardBuilt = Chunk;
		UE_LOG(LogTemp, Display, TEXT("Shard: %s chunk %d/%d of level %d, %d sections"), *LootName, (int32)ShardPath[ShardPath.Num() - 2] + 1, (int32)ShardPath.Last(), ShardPath.Num() / 2, Chunk->Sections.Num());
		Shard->ClearAllMeshSections();
		for (int32 i = 0; i < Chunk->Sections.Num(); ++i)
		{
			Shard->SetProcMeshSection(i, Chunk->Sections[i]);
		}
	}
	// Outside: the food's own look. Cut faces: plain, in the food's colour.
	UMaterialInterface* Outside = nullptr;
	if (bArt)
	{
		Outside = Art->Material;
	}
	else
	{
		Mesh->SetStaticMesh(AntKit::ShapeMesh((EAntShape)ShapeIndex));
		if (!MeshMID) { MeshMID = AntKit::Paint(Mesh, Color); }
		else { MeshMID->SetVectorParameterValue(TEXT("Color"), Color); }
		Outside = MeshMID;
	}
	if (!CapMID)
	{
		if (!BeamMID)
		{
			Beam->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cylinder));
			BeamMID = AntKit::Paint(Beam, Color);
		}
		if (BeamMID && BeamMID->Parent)
		{
			CapMID = UMaterialInstanceDynamic::Create(BeamMID->Parent, this);
		}
	}
	if (CapMID)
	{
		CapMID->SetVectorParameterValue(TEXT("Color"), Color * 0.85f);
	}
	for (int32 i = 0; i < Chunk->Sections.Num(); ++i)
	{
		UMaterialInterface* M = Chunk->Cap[i] && CapMID ? (UMaterialInterface*)CapMID : Outside;
		if (M)
		{
			Shard->SetMaterial(i, M);
		}
	}
	Shard->SetRelativeScale3D(ShardScale);
	Shard->SetRelativeLocation(-Chunk->Box.GetCenter() * ShardScale);
	Shard->SetVisibility(true);
	Mesh->SetVisibility(false);
	bHasArt = false;
	ArtOffset = FVector::ZeroVector;
	SetupPhysics(Chunk->Box.GetExtent() * ShardScale);
	return true;
}