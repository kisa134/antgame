#include "KitchenSlick.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntFx.h"
#include "AntTypes.h"
#include "KitchenArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

// =============================================================================================
// The puddle
// =============================================================================================

AKitchenSlick::AKitchenSlick()
{
	InteractRadius = 0.f;   // nothing to press: it is just there
}

void AKitchenSlick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenSlick, Kind);
	DOREPLIFETIME(AKitchenSlick, MaxRadius);
	DOREPLIFETIME(AKitchenSlick, Born);
	DOREPLIFETIME(AKitchenSlick, Life);
	DOREPLIFETIME(AKitchenSlick, WipeStart);
	DOREPLIFETIME(AKitchenSlick, WipeLen);
}

float AKitchenSlick::NetNow() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return (HasAuthority() || !GS) ? W->GetTimeSeconds() : (float)GS->GetServerWorldTimeSeconds();
}

float AKitchenSlick::Radius() const
{
	const float Age = NetNow() - Born;
	if (Age < 0.f)
	{
		return 0.f;
	}
	const float Grow = FMath::Sqrt(FMath::Clamp(Age / 1.1f, 0.f, 1.f));         // runs out of the bottle in about a second
	const float Dry = FMath::Clamp((Life - Age) / 8.f, 0.f, 1.f);                  // dries up in the last seconds
	const float Wipe = WipeStart >= 0.f ? 1.f - FMath::Clamp((NetNow() - WipeStart) / FMath::Max(0.5f, WipeLen), 0.f, 1.f) : 1.f;
	return MaxRadius * Grow * FMath::Sqrt(Dry) * Wipe;
}

void AKitchenSlick::StartWipe(float Seconds)
{
	if (HasAuthority() && WipeStart < 0.f)
	{
		WipeStart = GetWorld()->GetTimeSeconds();
		WipeLen = Seconds;
		ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("ANTSLICK wiping the %s puddle at %s over %.1f s"), Kind == SlickKind::Oil ? TEXT("oil") : TEXT("water"), *GetActorLocation().ToCompactString(), Seconds);
	}
}

void AKitchenSlick::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The level's own puddle model (SM_K_Puddle) where it exists, a flat disc otherwise; over it a thin glossy film in the liquid's colour.
	const bool bOilLook = Kind == SlickKind::Oil;   // oil: an amber pool of its own (the level's puddle model reads as water)
	if (UStaticMesh* M = (KitchenArt::Enabled() && !bOilLook) ? KitchenArt::Mesh(TEXT("SM_K_Puddle")) : nullptr)
	{
		Pool = NewObject<UStaticMeshComponent>(this);
		Pool->SetStaticMesh(M);
		Pool->SetupAttachment(Root);
		Pool->SetMobility(EComponentMobility::Movable);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCanEverAffectNavigation(false);
		Pool->SetCastShadow(false);
		Pool->RegisterComponent();
		AddInstanceComponent(Pool);
		KitchenArt::ApplyLook(Pool, TEXT("SM_K_Puddle"), 0);
		const FBoxSphereBounds B = M->GetBounds();
		MeshR = FMath::Max(1.f, (float)FMath::Max(B.BoxExtent.X, B.BoxExtent.Y));
		Pool->SetRelativeLocation(FVector(0.f, 0.f, 1.f) - FVector(B.Origin.X, B.Origin.Y, 0.f));
	}
	else
	{
		Pool = AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(0.f, 0.f, 1.f), FVector(1.f, 1.f, 0.02f), bOilLook ? FLinearColor(0.78f, 0.48f, 0.05f) : FLinearColor(0.35f, 0.55f, 0.8f));
		Pool->SetCastShadow(false);
		MeshR = 50.f;
	}
	const bool bOil = Kind == SlickKind::Oil;
	Sheen = AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(0.f, 0.f, 3.f), FVector(1.f, 1.f, 0.02f), FLinearColor::White);
	Sheen->SetCastShadow(false);
	if (UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent")))
	{
		UMaterialInstanceDynamic* Mi = UMaterialInstanceDynamic::Create(Soft, this);
		Mi->SetVectorParameterValue(TEXT("Color"), bOil ? FLinearColor(1.f, 0.62f, 0.04f, 0.78f) : FLinearColor(0.55f, 0.78f, 1.f, 0.30f));
		Sheen->SetMaterial(0, Mi);
	}
	Pool->SetRelativeScale3D(FVector(0.01f));
	Sheen->SetRelativeScale3D(FVector(0.01f));
}

void AKitchenSlick::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float R = Radius();
	const float Age = NetNow() - Born;
	if (HasAuthority() && (Age > Life || (WipeStart >= 0.f && GetWorld()->GetTimeSeconds() > WipeStart + WipeLen)))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTSLICK the puddle at %s is gone (%s, %.0f s)"), *GetActorLocation().ToCompactString(), WipeStart >= 0.f ? TEXT("wiped") : TEXT("dried"), Age);
		Destroy();
		return;
	}
	if (Pool)
	{
		const float K = FMath::Max(0.01f, R / MeshR);
		Pool->SetRelativeScale3D(FVector(K, K * 0.82f, MeshR == 50.f ? 0.02f : KitchenArt::ImportScale));
		Pool->SetVisibility(R > 4.f);
	}
	if (Sheen)
	{
		const float D = FMath::Max(0.01f, R * 2.f * 0.92f / 100.f);
		Sheen->SetRelativeScale3D(FVector(D, D * 0.8f, 0.02f));
		Sheen->SetVisibility(R > 4.f);
	}
	if (GetNetMode() != NM_DedicatedServer && R > 20.f)
	{
		// Glugs while it runs out, then a ripple now and then; splashes under the towel while it is wiped.
		FxClock += DeltaSeconds;
		const float Every = Age < 1.5f ? 0.12f : WipeStart >= 0.f ? 0.25f : 1.6f;
		if (FxClock > Every)
		{
			FxClock = 0.f;
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			AntFx::Ripple(this, GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(0.f, R * 0.8f) + FVector(0.f, 0.f, 4.f), FMath::Min(120.f, R * 0.25f));
		}
	}
}

void AKitchenSlick::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	const FVector C = GetActorLocation();
	if (P.Z < C.Z + 40.f && P.Z > C.Z - 30.f && FVector::Dist2D(P, C) < Radius())
	{
		Env.Flags |= AntStatus::Wet;   // wading through it: wet and slow
	}
}

AKitchenSlick* AKitchenSlick::At(const UWorld* World, const FVector& P, float Margin)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKitchenSlick> It(World); It; ++It)
	{
		const FVector C = It->GetActorLocation();
		if (FMath::Abs(P.Z - C.Z) < 220.f && It->IsSlippery() && FVector::Dist2D(P, C) < It->Radius() + Margin)
		{
			return *It;
		}
	}
	return nullptr;
}

AKitchenSlick* AKitchenSlick::Spill(UWorld* World, const FVector& P, uint8 InKind, AActor* InBy, float InRadius, float InLife)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	// Down to the floor (or the table) under the point.
	FVector At = P;
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 60.f), P - FVector(0.f, 0.f, 4000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		At = Hit.ImpactPoint;
	}
	const FTransform TM(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At);
	AKitchenSlick* S = World->SpawnActorDeferred<AKitchenSlick>(AKitchenSlick::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!S)
	{
		return nullptr;
	}
	S->Kind = InKind;
	S->MaxRadius = InRadius > 0.f ? InRadius : (InKind == SlickKind::Oil ? 520.f : 640.f);
	S->Life = InLife > 0.f ? InLife : FMath::FRandRange(60.f, 90.f);
	S->Born = World->GetTimeSeconds();
	S->By = InBy;
	S->FinishSpawning(TM);
	UE_LOG(LogTemp, Display, TEXT("ANTSLICK %s puddle at %s, radius %.0f, lies %.0f s"), InKind == SlickKind::Oil ? TEXT("oil") : TEXT("water"), *At.ToCompactString(), S->MaxRadius, S->Life);
	return S;
}

// =============================================================================================
// The bottle
// =============================================================================================

AKitchenSpillBottle::AKitchenSpillBottle()
{
	InteractRadius = 80.f;   // the ant stands at its foot or clings to its side
}

void AKitchenSpillBottle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenSpillBottle, Kind);
	DOREPLIFETIME(AKitchenSpillBottle, TipStart);
	DOREPLIFETIME(AKitchenSpillBottle, TipYaw);
}

float AKitchenSpillBottle::NetNow() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return (HasAuthority() || !GS) ? W->GetTimeSeconds() : (float)GS->GetServerWorldTimeSeconds();
}

int32 AKitchenSpillBottle::DoorClass() const
{
	return AntDoor::Heavy;   // about three workers
}

FString AKitchenSpillBottle::GetPrompt(const AAntCharacter* Ant) const
{
	return FString::Printf(TEXT("опрокинуть: %s"), *BottleName().ToLower());
}

float AKitchenSpillBottle::DistanceTo(const FVector& P) const
{
	const FVector B = GetActorLocation();
	const float Side = FMath::Max(0.f, (float)FVector::Dist2D(P, B) - Radius);
	const float Up = P.Z > B.Z + Height ? (float)(P.Z - B.Z - Height) : (P.Z < B.Z ? (float)(B.Z - P.Z) : 0.f);
	return FMath::Sqrt(Side * Side + Up * Up);
}

void AKitchenSpillBottle::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AKitchenSpillBottle::BuildLook()
{
	// Base -> the yaw it falls towards -> the bottom edge it tips over -> the bottle itself. Plain shapes, solid: ants climb it.
	YawPivot = AntKit::AddPivot(this, Root, FVector::ZeroVector);
	Edge = AntKit::AddPivot(this, YawPivot, FVector(Radius, 0.f, 0.f));
	BodyRoot = AntKit::AddPivot(this, Edge, FVector(-Radius, 0.f, 0.f));
	const bool bOil = Kind == SlickKind::Oil;
	const FLinearColor Glass = bOil ? FLinearColor(0.95f, 0.68f, 0.10f) : FLinearColor(0.62f, 0.82f, 0.97f);
	const FLinearColor Cap = bOil ? FLinearColor(0.75f, 0.12f, 0.08f) : FLinearColor(0.12f, 0.35f, 0.85f);
	const FLinearColor Label = bOil ? FLinearColor(0.95f, 0.92f, 0.82f) : FLinearColor(0.95f, 0.97f, 1.f);
	const float D = Radius * 2.f;
	auto Cyl = [&](float Z0, float Z1, float Dia, const FLinearColor& C, bool bSolid)
	{
		UStaticMeshComponent* P = AntKit::AddPart(this, BodyRoot, EAntShape::Cylinder, FVector(0.f, 0.f, (Z0 + Z1) * 0.5f), FVector(Dia / 100.f, Dia / 100.f, (Z1 - Z0) / 100.f), C, FRotator::ZeroRotator, bSolid);
		P->SetCastShadow(true);
		return P;
	};
	Cyl(0.f, Height * 0.70f, D, Glass, true);
	Cyl(Height * 0.24f, Height * 0.50f, D * 1.03f, Label, false);
	Cyl(Height * 0.70f, Height * 0.80f, D * 0.78f, Glass, true);
	Cyl(Height * 0.80f, Height * 0.95f, D * 0.40f, Glass, true);
	Cyl(Height * 0.95f, Height, D * 0.46f, Cap, true);
}

float AKitchenSpillBottle::Lean() const
{
	if (TipStart < 0.f)
	{
		return 0.f;
	}
	const float T = NetNow() - TipStart;
	if (T < 0.f)
	{
		return 0.f;
	}
	if (T < FallTime)
	{
		const float U = T / FallTime;
		return 90.f * U * U;   // it goes over slowly, then all at once
	}
	if (T < FallTime + 0.28f)
	{
		return 90.f - 9.f * FMath::Sin(PI * (T - FallTime) / 0.28f);   // one small bounce on the floor
	}
	return 90.f;
}

void AKitchenSpillBottle::SetDoorOpen(bool bInOpen)
{
	if (!HasAuthority())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (!bInOpen)
	{
		TipStart = -1.f;
		bSpilled = false;
		bLandFx = false;
		StandAt = -1.f;
		Puddle = nullptr;
		PullMissing = 0;
		ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("ANTSPILL %s stands again at %s"), *BottleName(), *GetActorLocation().ToCompactString());
		return;
	}
	if (TipStart >= 0.f)
	{
		return;
	}
	// It falls away from the ants that push it; if a cabinet or a wall stands that way, the free way nearest to it,
	// and of two equally good ones the one into the room.
	const FVector B = GetActorLocation();
	FVector Push = FVector::ZeroVector;
	int32 N = 0;
	AAntCharacter* First = nullptr;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		if (It->Pulling == this && !It->bDead)
		{
			Push += It->GetActorLocation();
			++N;
			if (!First) { First = *It; }
		}
	}
	FVector Dir = N > 0 ? FVector(B - Push / N).GetSafeNormal2D() : FVector::ZeroVector;
	const FVector Room = FVector(0.f, -1.f, 0.f);
	if (Dir.IsNearlyZero()) { Dir = Room; }
	auto Free = [&](const FVector& V)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BottleTip), false, this);
		FHitResult Hit;
		for (float Z : { Radius, Height * 0.5f })
		{
			const FVector From = B + FVector(0.f, 0.f, Z) + V * (Radius + 5.f);
			if (W->LineTraceSingleByObjectType(Hit, From, From + V * (Height + 60.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
			{
				return false;
			}
		}
		return true;
	};
	FVector Best = Dir;
	float BestScore = -1.e9f;
	for (int32 i = 0; i < 16; ++i)
	{
		const FVector V = Dir.RotateAngleAxis(i * 22.5f, FVector::UpVector);
		const float Score = (Free(V) ? 100.f : 0.f) + 2.f * (float)FVector::DotProduct(V, Dir) + 0.35f * (float)FVector::DotProduct(V, Room);
		if (Score > BestScore) { BestScore = Score; Best = V; }
	}
	TipYaw = (float)Best.Rotation().Yaw;
	TipStart = W->GetTimeSeconds();
	LastTip = TipStart;
	TippedBy = First;
	bSpilled = false;
	bLandFx = false;
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("ANTSPILL %s tipped over by %s and %d more, falls to yaw %.0f (pushed towards %.0f), free %d"), *BottleName(), First ? *First->GetDisplayName() : TEXT("-"),
		FMath::Max(0, N - 1), TipYaw, (float)Dir.Rotation().Yaw, BestScore >= 100.f ? 1 : 0);
	if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->Broadcast(FString::Printf(TEXT("%s опрокинута! Скоро на полу будет скользко"), *BottleName()), FLinearColor(1.f, 0.8f, 0.3f));
	}
}

void AKitchenSpillBottle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PullTick(DeltaSeconds);
	const float Strain = StrainNow(DeltaSeconds);   // rocking while too few ants push
	const float L = Lean();
	if (YawPivot && Edge)
	{
		YawPivot->SetRelativeRotation(FRotator(0.f, TipYaw - (float)GetActorRotation().Yaw, 0.f));
		Edge->SetRelativeRotation(FRotator(-(L + Strain * 90.f), 0.f, 0.f));
	}
	if (TipStart < 0.f)
	{
		return;
	}
	const float T = NetNow() - TipStart;
	const FVector Dir = FRotator(0.f, TipYaw, 0.f).Vector();
	const FVector Mouth = GetActorLocation() + Dir * (Radius + Height + 30.f);
	if (!bLandFx && T >= FallTime && GetNetMode() != NM_DedicatedServer)
	{
		bLandFx = true;
		AntSfx::Play(this, AntSfx::Drop, GetActorLocation() + Dir * Height * 0.5f, 1.f, 0.8f);
		AntSfx::Play(this, AntSfx::Splash, Mouth, 0.9f, 0.9f);
		AntFx::LandDust(this, GetActorLocation() + Dir * Height * 0.5f, FVector::UpVector, 260.f);
		AntFx::Splash(this, Mouth, 60.f);
	}
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bSpilled && T >= FallTime * 0.9f)
	{
		bSpilled = true;
		Puddle = AKitchenSlick::Spill(GetWorld(), Mouth + FVector(0.f, 0.f, 40.f), Kind, TippedBy.Get());
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->MakeNoise(GetActorLocation(), 0.7f, AntNoise::Crash, this, Cast<AAntCharacter>(TippedBy.Get()));   // the clatter brings the Human over
		}
	}
	if (bSpilled && !Puddle.IsValid() && StandAt < 0.f)
	{
		StandAt = Now + 8.f;
	}
	if (StandAt > 0.f && Now >= StandAt)
	{
		SetDoorOpen(false);
	}
}

AKitchenSpillBottle* AKitchenSpillBottle::Nearest(const UWorld* World, const FVector& P, int32 OnlyKind)
{
	AKitchenSpillBottle* Best = nullptr;
	float BestD = 1.e12f;
	for (TActorIterator<AKitchenSpillBottle> It(World); It; ++It)
	{
		const float D = (float)FVector::DistSquared(It->GetActorLocation(), P);
		if ((OnlyKind < 0 || It->Kind == OnlyKind) && D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

void AKitchenSpillBottle::SpawnAll(UWorld* World)
{
	if (!World || !KitchenArt::Enabled())
	{
		return;   // the places are the level v2 kitchen's
	}
	struct FDef { uint8 Kind; FVector At; };
	const FDef Defs[] =
	{
		{ SlickKind::Oil,   FVector(-1650.f, 2860.f, 0.f) },   // on the floor by the stove, against the cabinet front
		{ SlickKind::Water, FVector(2350.f, 2850.f, 0.f) },    // by the fridge, in front of the corner cabinet
	};
	for (const FDef& D : Defs)
	{
		FVector At = D.At;
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0.f, 0.f, 500.f), At - FVector(0.f, 0.f, 500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			At.Z = Hit.ImpactPoint.Z;
		}
		const FTransform TM(At);
		if (AKitchenSpillBottle* B = World->SpawnActorDeferred<AKitchenSpillBottle>(AKitchenSpillBottle::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			B->Kind = D.Kind;
			B->FinishSpawning(TM);
			UE_LOG(LogTemp, Display, TEXT("ANTSPILL %s stands at %s"), *B->BottleName(), *At.ToCompactString());
		}
	}
}
