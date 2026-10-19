#include "WarMode.h"
#include "AntCharacter.h"
#include "CreatureCharacters.h"
#include "AntArmy.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor ColonyC(0.25f, 0.8f, 1.f);
	const FLinearColor FireC(1.f, 0.35f, 0.12f);
	const FLinearColor NeutralC(0.75f, 0.75f, 0.7f);

	FLinearColor TeamTint(uint8 Team)
	{
		return Team == 0 ? ColonyC : Team == 1 ? FireC : NeutralC;
	}

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, EAntShape S, const FVector& Loc, const FVector& Scale, const FLinearColor& C, bool bCollide = false)
	{
		UStaticMeshComponent* M = AntKit::AddPart(Owner, Parent, S, Loc, Scale, C, FRotator::ZeroRotator, bCollide);
		return M;
	}

	FVector GroundBelow(UWorld* W, const FVector& P, const AActor* Ignore)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WarGround), false, Ignore);
		if (W->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 200.f), P - FVector(0.f, 0.f, 3000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			return Hit.ImpactPoint;
		}
		return P;
	}
}

// =============================================================================================
// Territory
// =============================================================================================

AAntTerritory::AAntTerritory()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntTerritory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntTerritory, ZoneName);
	DOREPLIFETIME(AAntTerritory, Radius);
	DOREPLIFETIME(AAntTerritory, OwnerTeam);
	DOREPLIFETIME(AAntTerritory, Capture);
	DOREPLIFETIME(AAntTerritory, bContested);
	DOREPLIFETIME(AAntTerritory, bLineCut);
}

bool AAntTerritory::Contains(const FVector& P) const
{
	const FVector C = GetActorLocation();
	return FVector::Dist2D(P, C) < Radius && FMath::Abs(P.Z - C.Z) < 900.f;
}

void AAntTerritory::Presence(UWorld* World, const FVector& C, float R, float ZTol, float& Out0, float& Out1)
{
	Out0 = Out1 = 0.f;
	if (!World)
	{
		return;
	}
	for (TActorIterator<AAntCharacter> It(World); It; ++It)
	{
		const AAntCharacter* A = *It;
		if (!A || A->bDead || A->Team > 1)
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		if (FVector::Dist2D(P, C) < R && FMath::Abs(P.Z - C.Z) < ZTol)
		{
			(A->Team == 0 ? Out0 : Out1) += A->Creature == 0 ? 3.f : 1.f;
		}
	}
	for (TActorIterator<AAntArmy> It(World); It; ++It)
	{
		if (It->Team > 1)
		{
			continue;
		}
		const int32 N = It->CountNear(C, R, ZTol);
		(It->Team == 0 ? Out0 : Out1) += N * 0.25f;
	}
}

void AAntTerritory::ResetZone()
{
	OwnerTeam = 255;
	Capture = 0.f;
	bContested = false;
	bLineCut = false;
	IncomeTimer = 0.f;
}

void AAntTerritory::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority())
	{
		ServerTick(Dt);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateVisual(Dt);
	}
	else if (OwnerTeam != LineFor)
	{
		BuildLine();   // the server needs the line to know when it's cut
	}
}

void AAntTerritory::ServerTick(float Dt)
{
	Accum += Dt;
	if (Accum < 0.5f)
	{
		return;
	}
	const float Step = Accum;
	Accum = 0.f;
	AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (!GS || !GM || GS->Phase != 0)
	{
		return;
	}

	float P0, P1;
	Presence(GetWorld(), GetActorLocation(), Radius, 900.f, P0, P1);
	bContested = P0 > 0.f && P1 > 0.f;
	const float Push = FMath::Clamp(P0 - P1, -12.f, 12.f) * 0.04f * Step;
	Capture = FMath::Clamp(Capture + Push, -1.f, 1.f);

	const uint8 Old = OwnerTeam;
	if (Capture >= 1.f) { OwnerTeam = 0; }
	else if (Capture <= -1.f) { OwnerTeam = 1; }
	else if ((OwnerTeam == 0 && Capture < 0.f) || (OwnerTeam == 1 && Capture > 0.f)) { OwnerTeam = 255; }
	if (Old != OwnerTeam)
	{
		IncomeTimer = 0.f;
		if (OwnerTeam < 2)
		{
			GM->Broadcast(FString::Printf(TEXT("%s захватили «%s» — пошла цепочка снабжения"), OwnerTeam == 0 ? TEXT("ЧЁРНЫЕ") : TEXT("ОГНЕННЫЕ"), *ZoneName), TeamTint(OwnerTeam));
			GM->AddScore(OwnerTeam, 50);
		}
		else
		{
			GM->Broadcast(FString::Printf(TEXT("«%s» потеряна — метка стёрта"), *ZoneName), NeutralC);
		}
	}

	// Supply line: cut if an enemy stands anywhere on it.
	if (OwnerTeam != LineFor)
	{
		BuildLine();
	}
	CutCheck -= Step;
	if (CutCheck <= 0.f)
	{
		CutCheck = 1.f;
		bool bCut = false;
		if (OwnerTeam < 2 && Line.Num() > 1)
		{
			const uint8 Enemy = OwnerTeam == 0 ? 1 : 0;
			for (int32 i = 0; i < Line.Num() && !bCut; i += 2)
			{
				float E0, E1;
				Presence(GetWorld(), Line[i], 260.f, 400.f, E0, E1);
				bCut = (Enemy == 0 ? E0 : E1) >= 2.f;
			}
		}
		if (bCut && !bWasCut && OwnerTeam < 2)
		{
			GM->Broadcast(FString::Printf(TEXT("Цепочка снабжения «%s» ПЕРЕРЕЗАНА!"), *ZoneName), FLinearColor(1.f, 0.8f, 0.2f));
		}
		bWasCut = bCut;
		bLineCut = bCut;
	}

	if (OwnerTeam < 2 && !bLineCut)
	{
		IncomeTimer += Step;
		if (IncomeTimer >= IncomeEvery)
		{
			IncomeTimer = 0.f;
			GM->AddScore(OwnerTeam, 20);
			GM->AddPool(OwnerTeam, 3);
		}
	}
}

void AAntTerritory::BuildLine()
{
	LineFor = OwnerTeam;
	Line.Reset();
	LineDist.Reset();
	LineLen = 0.f;
	if (OwnerTeam > 1)
	{
		return;
	}
	// Waypoints nest -> zone (through the doorway / window), then densified and dropped on the ground.
	TArray<FVector> Way;
	FVector P = AKitchenBuilder::NestLocation(OwnerTeam);
	const FVector Goal = GetActorLocation();
	Way.Add(P);
	for (int32 i = 0; i < 16; ++i)
	{
		const FVector N = AKitchenBuilder::Route(P, Goal);
		Way.Add(N);
		if (N.Equals(Goal, 1.f))
		{
			break;
		}
		P = N;
	}
	for (int32 i = 0; i + 1 < Way.Num(); ++i)
	{
		const FVector A = Way[i], B = Way[i + 1];
		const bool bVertical = FVector::Dist2D(A, B) < 120.f;
		const int32 Steps = FMath::Max(1, FMath::CeilToInt((float)FVector::Dist(A, B) / 160.f));
		for (int32 s = 0; s < Steps; ++s)
		{
			FVector Q = FMath::Lerp(A, B, (float)s / Steps);
			if (!bVertical)
			{
				Q = GroundBelow(GetWorld(), Q, this);
			}
			Line.Add(Q + FVector(0.f, 0.f, 3.f));
		}
	}
	Line.Add(GroundBelow(GetWorld(), Goal, this) + FVector(0.f, 0.f, 3.f));
	LineDist.Add(0.f);
	for (int32 i = 1; i < Line.Num(); ++i)
	{
		LineLen += (float)FVector::Dist(Line[i - 1], Line[i]);
		LineDist.Add(LineLen);
	}
}

void AAntTerritory::UpdateVisual(float Dt)
{
	if (!bVisualBuilt)
	{
		bVisualBuilt = true;
		const FVector C = GetActorLocation();
		Ring = MakePart(this, RootComponent, EAntShape::Cylinder, FVector::ZeroVector, FVector(Radius / 50.f, Radius / 50.f, 0.03f), NeutralC);
		Ring->SetUsingAbsoluteLocation(true);
		Ring->SetWorldLocation(GroundBelow(GetWorld(), C, this) + FVector(0.f, 0.f, 2.f));
		Ring->SetCastShadow(false);
		RingMID = Cast<UMaterialInstanceDynamic>(Ring->GetMaterial(0));
		// A pheromone banner: a stick with a flag in the team color.
		Pole = MakePart(this, RootComponent, EAntShape::Cylinder, FVector(0.f, 0.f, 260.f), FVector(0.12f, 0.12f, 5.2f), FLinearColor(0.45f, 0.32f, 0.2f), true);
		Flag = MakePart(this, RootComponent, EAntShape::Cube, FVector(70.f, 0.f, 440.f), FVector(1.4f, 0.04f, 0.9f), NeutralC);
		FlagMID = Cast<UMaterialInstanceDynamic>(Flag->GetMaterial(0));

		auto MakeISM = [this](const TCHAR* Name) -> UInstancedStaticMeshComponent*
		{
			UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this, Name);
			I->SetupAttachment(RootComponent);
			I->SetUsingAbsoluteLocation(true);
			I->SetUsingAbsoluteRotation(true);
			I->SetUsingAbsoluteScale(true);
			I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			I->SetCastShadow(false);
			I->RegisterComponent();
			return I;
		};
		Lane = MakeISM(TEXT("Lane"));
		const AntArt::FPart& Art = AntArt::Get(TEXT("SM_Ant_Body"));
		if (Art.IsValid())
		{
			Lane->SetStaticMesh(Art.Mesh);
			if (Art.Material) { Lane->SetMaterial(0, Art.Material); }
			LaneMID = Lane->CreateDynamicMaterialInstance(0);
			const float L = (float)Art.Mesh->GetBoundingBox().GetSize().GetMax();
			InstScale = L > 0.f ? 26.f * AAntCharacter::AntShrink / L : 0.1f;
		}
		else
		{
			Lane->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
			LaneMID = AntKit::Paint(Lane, FLinearColor(0.05f, 0.02f, 0.015f));
			InstScale = 0.12f * AAntCharacter::AntShrink;
		}
		Crumbs = MakeISM(TEXT("Crumbs"));
		Crumbs->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cube));
		AntKit::Paint(Crumbs, FLinearColor(1.f, 0.85f, 0.45f));
	}

	if (OwnerTeam != LineFor)
	{
		LineDelay -= Dt;
		if (LineDelay <= 0.f)
		{
			BuildLine();
		}
	}
	const FLinearColor Col = TeamTint(OwnerTeam);
	// Ring blends towards whoever is pushing.
	FLinearColor RingCol = FMath::Lerp(NeutralC, Capture >= 0.f ? ColonyC : FireC, FMath::Abs(Capture));
	if (bContested)
	{
		RingCol = FMath::Lerp(RingCol, FLinearColor(1.f, 1.f, 0.3f), 0.35f + 0.35f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f));
	}
	if (RingMID) { RingMID->SetVectorParameterValue(TEXT("Color"), RingCol); }
	if (FlagMID) { FlagMID->SetVectorParameterValue(TEXT("Color"), Col * 1.5f); }
	if (Flag)
	{
		const float T = GetWorld()->GetTimeSeconds();
		Flag->SetRelativeRotation(FRotator(0.f, FMath::Sin(T * 1.7f) * 12.f, FMath::Sin(T * 3.1f) * 4.f));
	}

	// The supply line: workers walk out to the zone and come back carrying crumbs.
	const int32 Want = (OwnerTeam < 2 && Line.Num() > 1) ? (bLineCut ? 16 : 56) : 0;
	if (Lane->GetInstanceCount() != Want)
	{
		Lane->ClearInstances();
		Crumbs->ClearInstances();
		TArray<FTransform> Xf;
		Xf.SetNum(Want);
		if (Want > 0)
		{
			Lane->AddInstances(Xf, false, true);
			TArray<FTransform> Xc;
			Xc.SetNum(Want / 2);
			Crumbs->AddInstances(Xc, false, true);
		}
		if (LaneMID)
		{
			LaneMID->SetVectorParameterValue(TEXT("Tint"), OwnerTeam == 1 ? FLinearColor(2.2f, 0.6f, 0.35f) : FLinearColor::White);
			LaneMID->SetVectorParameterValue(TEXT("Color"), OwnerTeam == 1 ? FLinearColor(0.5f, 0.08f, 0.03f) : FLinearColor(0.05f, 0.02f, 0.015f));
		}
	}
	if (Want == 0 || LineLen <= 1.f)
	{
		return;
	}
	const float T = GetWorld()->GetTimeSeconds();
	const float Speed = (bLineCut ? 60.f : 230.f) * AAntCharacter::SpeedK;
	TArray<FTransform> Xf, Xc;
	Xf.SetNum(Want);
	Xc.SetNum(Want / 2);
	auto At = [this](float D, FVector& OutP, FVector& OutDir)
	{
		D = FMath::Clamp(D, 0.f, LineLen);
		int32 i = 1;
		while (i < LineDist.Num() - 1 && LineDist[i] < D) { ++i; }
		const float A = LineDist[i - 1], B = LineDist[i];
		const float K = B > A ? (D - A) / (B - A) : 0.f;
		OutP = FMath::Lerp(Line[i - 1], Line[i], K);
		OutDir = (Line[i] - Line[i - 1]).GetSafeNormal();
	};
	for (int32 i = 0; i < Want; ++i)
	{
		const bool bBack = (i % 2) == 1;   // odd ones come home with crumbs
		const float Phase = (i / 2) * (LineLen / (Want / 2));
		float D = FMath::Fmod(T * Speed + Phase, LineLen);
		if (bBack) { D = LineLen - D; }
		FVector P, Dir;
		At(D, P, Dir);
		if (bBack) { Dir = -Dir; }
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal() * (bBack ? -9.f : 9.f);
		const FRotator R = FRotationMatrix::MakeFromXZ(Dir.IsNearlyZero() ? FVector::ForwardVector : Dir, FVector::UpVector).Rotator();
		Xf[i] = FTransform(R, P + Side * AAntCharacter::AntShrink + FVector(0.f, 0.f, 3.f * AAntCharacter::AntShrink), FVector(InstScale));
		if (bBack)
		{
			Xc[i / 2] = FTransform(R, P + Side + Dir * 14.f + FVector(0.f, 0.f, 10.f), FVector(0.07f));
		}
	}
	Lane->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
	Crumbs->BatchUpdateInstancesTransforms(0, Xc, true, true, true);
}

// =============================================================================================
// Cashout
// =============================================================================================

AAntCashout::AAntCashout()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntCashout::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntCashout, ZoneName);
	DOREPLIFETIME(AAntCashout, CashTeam);
	DOREPLIFETIME(AAntCashout, Progress);
	DOREPLIFETIME(AAntCashout, bActive);
	DOREPLIFETIME(AAntCashout, bContested);
}

void AAntCashout::ResetStation()
{
	CashTeam = 255;
	Progress = 0.f;
	bActive = false;
	bContested = false;
}

void AAntCashout::BuildVisual()
{
	bBuilt = true;
	Base = MakePart(this, RootComponent, EAntShape::Cylinder, FVector(0.f, 0.f, 10.f), FVector(Radius / 50.f * 0.6f, Radius / 50.f * 0.6f, 0.2f), FLinearColor(0.85f, 0.7f, 0.25f), true);
	RingC = MakePart(this, RootComponent, EAntShape::Cylinder, FVector(0.f, 0.f, 2.f), FVector(Radius / 50.f, Radius / 50.f, 0.03f), NeutralC);
	RingC->SetCastShadow(false);
	RingMID = Cast<UMaterialInstanceDynamic>(RingC->GetMaterial(0));
	Bowl = MakePart(this, RootComponent, EAntShape::Cylinder, FVector(0.f, 0.f, 110.f), FVector(2.f, 2.f, 1.8f), FLinearColor(0.95f, 0.95f, 1.f));
	Bowl->SetVisibility(false);
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	Light->IntensityUnits = ELightUnits::Unitless;
	Light->bUseInverseSquaredFalloff = false;
	Light->LightFalloffExponent = 2.f;
	Light->AttenuationRadius = 900.f;
	Light->CastShadows = false;
	Light->SetIntensity(3.f);
	Light->SetLightColor(FLinearColor(1.f, 0.85f, 0.4f));
	Light->RegisterComponent();
}

void AAntCashout::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority())
	{
		ServerTick(Dt);
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!bBuilt)
	{
		BuildVisual();
	}
	const float T = GetWorld()->GetTimeSeconds();
	FLinearColor C = bActive ? TeamTint(CashTeam) : FLinearColor(1.f, 0.85f, 0.35f);
	if (bContested) { C = FMath::Lerp(C, FLinearColor(1.f, 1.f, 0.2f), 0.5f + 0.5f * FMath::Sin(T * 10.f)); }
	if (RingMID) { RingMID->SetVectorParameterValue(TEXT("Color"), C * (1.f + 0.5f * FMath::Sin(T * 3.f))); }
	if (Bowl) { Bowl->SetVisibility(bActive); }
	if (Light)
	{
		Light->SetLightColor(C);
		Light->SetIntensity(bActive ? 6.f + 3.f * FMath::Sin(T * 6.f) : 2.5f);
	}
}

void AAntCashout::ServerTick(float Dt)
{
	AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (!GS || !GM || GS->Phase != 0)
	{
		return;
	}
	const FVector C = GetActorLocation();
	if (!bActive)
	{
		// A safe brought onto the spot starts the cashout for whoever brought it.
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			AKitchenLoot* L = *It;
			if (!L || !L->bVault || L->bHeld || FVector::Dist2D(L->GetActorLocation(), C) > Radius || FMath::Abs(L->GetActorLocation().Z - C.Z) > 400.f)
			{
				continue;
			}
			uint8 Team = 255;
			if (L->IsCarried())
			{
				Team = L->CarryTeam();
			}
			else
			{
				float P0, P1;
				AAntTerritory::Presence(GetWorld(), C, Radius * 1.6f, 500.f, P0, P1);
				Team = P0 > P1 ? 0 : P1 > P0 ? 1 : 255;
			}
			if (Team > 1)
			{
				continue;
			}
			L->ReleaseAll();
			L->Destroy();
			bActive = true;
			CashTeam = Team;
			Progress = 0.f;
			AntSfx::Everyone(this, AntSfx::Legendary, C, 0.9f);
			GM->Broadcast(FString::Printf(TEXT("%s сдают СЕЙФ в «%s»! %d с — держите точку!"), Team == 0 ? TEXT("ЧЁРНЫЕ") : TEXT("ОГНЕННЫЕ"), *ZoneName, (int32)Duration), TeamTint(Team));
			break;
		}
		return;
	}

	float P0, P1;
	AAntTerritory::Presence(GetWorld(), C, Radius * 1.6f, 500.f, P0, P1);
	const float Mine = CashTeam == 0 ? P0 : P1;
	const float Theirs = CashTeam == 0 ? P1 : P0;
	bContested = Theirs > 0.f && Mine > 0.f;
	if (Theirs > Mine)
	{
		// Being stolen.
		Progress -= Dt / 15.f;
		if (Progress <= 0.f)
		{
			CashTeam = CashTeam == 0 ? 1 : 0;
			Progress = 0.1f;
			AntSfx::Everyone(this, AntSfx::ArmyCharge, C, 1.2f);
			GM->Broadcast(FString::Printf(TEXT("ПЕРЕХВАТ! «%s» теперь считает для %s"), *ZoneName, CashTeam == 0 ? TEXT("чёрных") : TEXT("огненных")), TeamTint(CashTeam));
		}
	}
	else
	{
		Progress += Dt / Duration * (bContested ? 0.5f : 1.f);
	}
	if (Progress >= 1.f)
	{
		const uint8 Winner = CashTeam;
		ResetStation();
		AntSfx::Everyone(this, AntSfx::Deliver, C, 0.8f);
		GM->Broadcast(FString::Printf(TEXT("КЭШАУТ! %s забрали сейф (+%d и подкрепление)"), Winner == 0 ? TEXT("ЧЁРНЫЕ") : TEXT("ОГНЕННЫЕ"), Reward), FLinearColor(1.f, 0.85f, 0.2f));
		GM->AddScore(Winner, Reward);
		GM->AddPool(Winner, 24);
	}
}

// =============================================================================================
// Fly feeder
// =============================================================================================

AFlyFeeder::AFlyFeeder()
{
	InteractRadius = 520.f;
}

void AFlyFeeder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFlyFeeder, Kind);
}

void AFlyFeeder::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || Kind != 0)
	{
		return;
	}
	// A proper dog pile on the lawn (with a few circling specks).
	const FLinearColor Poo(0.36f, 0.22f, 0.1f), PooDark(0.25f, 0.15f, 0.07f);
	Part(Root, EAntShape::Sphere, FVector(0.f, 0.f, 70.f), FVector(520.f, 420.f, 170.f), Poo, false);
	Part(Root, EAntShape::Sphere, FVector(40.f, -20.f, 190.f), FVector(380.f, 320.f, 150.f), PooDark, false);
	Part(Root, EAntShape::Sphere, FVector(10.f, 10.f, 290.f), FVector(250.f, 210.f, 130.f), Poo, false);
	Part(Root, EAntShape::Cone, FVector(10.f, 10.f, 380.f), FVector(90.f, 90.f, 120.f), PooDark, false);
	Part(Root, EAntShape::Sphere, FVector(380.f, 160.f, 40.f), FVector(160.f, 130.f, 80.f), Poo, false);
}

bool AFlyFeeder::CanInteract(const AAntCharacter* Ant) const
{
	return Ant && Ant->Creature == 1;
}

FString AFlyFeeder::GetPrompt(const AAntCharacter* Ant) const
{
	return Kind == 0 ? TEXT("Жрать собачье говно (лечит + полный форсаж)") : TEXT("Жрать помои из мусорки (лечит + полный форсаж)");
}

void AFlyFeeder::Interact(AAntCharacter* Ant)
{
	AFlyCharacter* Fly = Cast<AFlyCharacter>(Ant);
	if (!Fly)
	{
		return;
	}
	Fly->Health = Fly->MaxHealth;
	Fly->Fuel = 100.f;
	AntSfx::Everyone(this, AntSfx::Squish, Fly->GetActorLocation(), 0.7f);
	Fly->Toast(Kind == 0 ? TEXT("Ммм, свежак! Здоровье и форсаж на максимум") : TEXT("Помои — топливо чемпионов! Здоровье и форсаж на максимум"));
}
