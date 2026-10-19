#include "AntChain.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "AntKitchenGameMode.h"
#include "AntTypes.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AAntChain::AAntChain()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);

	Walk = CreateDefaultSubobject<UBoxComponent>(TEXT("Walk"));
	RootComponent = Walk;
	Walk->SetUsingAbsoluteLocation(true);
	Walk->SetUsingAbsoluteRotation(true);
	Walk->SetBoxExtent(FVector(10.f));
	// Ants crawl only on WorldStatic: the chain is a surface like any shelf.
	Walk->SetCollisionObjectType(ECC_WorldStatic);
	Walk->SetCollisionResponseToAllChannels(ECR_Block);
	Walk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Walk->SetCanEverAffectNavigation(false);
}

void AAntChain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntChain, Team);
	DOREPLIFETIME(AAntChain, Anchor);
	DOREPLIFETIME(AAntChain, Dir);
	DOREPLIFETIME(AAntChain, EndPoint);
	DOREPLIFETIME(AAntChain, bBridge);
	DOREPLIFETIME(AAntChain, Links);
	DOREPLIFETIME(AAntChain, ArmyLinks);
	DOREPLIFETIME(AAntChain, Builder);
}

FVector AAntChain::PointAt(float D) const
{
	const FVector A = Anchor;
	if (!bBridge)
	{
		return A + FVector(Dir) * D;   // ladder: straight down
	}
	const float S = FMath::Max(1.f, Span());
	if (IsComplete())
	{
		// Spanned: a gentle sag in the middle.
		const float T = FMath::Clamp(D / S, 0.f, 1.f);
		return FMath::Lerp(A, FVector(EndPoint), T) - FVector(0.f, 0.f, 1.f) * (0.05f * S * FMath::Sin(T * PI));
	}
	// Still growing: the free end droops under its own weight.
	const float Droop = D * D / (S * 3.f);
	return A + FVector(Dir) * D - FVector(0.f, 0.f, Droop);
}

AAntChain* AAntChain::Start(UWorld* World, uint8 InTeam, AAntCharacter* First, const FVector& InAnchor, const FVector& InDir, bool bInBridge, const FVector& InEnd)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAntChain* C = World->SpawnActorDeferred<AAntChain>(AAntChain::StaticClass(), FTransform(InAnchor), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C)
	{
		return nullptr;
	}
	C->Team = InTeam;
	C->Anchor = InAnchor;
	C->Dir = InDir.GetSafeNormal();
	C->bBridge = bInBridge;
	C->EndPoint = bInBridge ? InEnd : InAnchor;
	C->FinishSpawning(FTransform(InAnchor));
	if (First)
	{
		C->Join(First);
	}
	return C;
}

bool AAntChain::Join(AAntCharacter* A)
{
	if (!HasAuthority() || !A || A->bDead || Links.Contains(A) || !IsGrowing())
	{
		return false;
	}
	Links.Add(A);
	A->SetChain(this);
	if (IsComplete() && !bAnnounced)
	{
		bAnnounced = true;
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->Broadcast(FString::Printf(TEXT("ЖИВОЙ МОСТ готов: %d муравьёв держат друг друга!"), NumLinks()), FLinearColor(0.5f, 1.f, 0.6f));
		}
	}
	return true;
}

void AAntChain::Leave(AAntCharacter* A)
{
	if (!HasAuthority())
	{
		return;
	}
	const int32 Idx = Links.IndexOfByKey(A);
	if (Idx == INDEX_NONE)
	{
		return;
	}
	// Everyone below lets go too (and the army part).
	TArray<AAntCharacter*> Fallen;
	for (int32 i = Idx; i < Links.Num(); ++i)
	{
		Fallen.Add(Links[i]);
	}
	Links.SetNum(Idx);
	const int32 Workers = ArmyLinks;
	if (Idx == 0 || Links.Num() == 0)
	{
		ArmyLinks = 0;
	}
	for (AAntCharacter* F : Fallen)
	{
		if (IsValid(F))
		{
			F->SetChain(nullptr);
		}
	}
	if (ArmyLinks == 0 && Workers > 0)
	{
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->ReturnWorkers(Team, Workers, Builder);
		}
	}
	bAnnounced = IsComplete();
	if (NumLinks() == 0)
	{
		Destroy();
	}
}

void AAntChain::Dissolve()
{
	if (!HasAuthority())
	{
		return;
	}
	const int32 Workers = ArmyLinks;
	ArmyLinks = 0;
	TArray<AAntCharacter*> All = Links;
	Links.Reset();
	for (AAntCharacter* A : All)
	{
		if (IsValid(A)) { A->SetChain(nullptr); }
	}
	if (Workers > 0)
	{
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->ReturnWorkers(Team, Workers, Builder);
		}
	}
	Destroy();
}

int32 AAntChain::KillArmyInSphere(const FVector& C, float R)
{
	if (!HasAuthority() || ArmyLinks <= 0)
	{
		return 0;
	}
	int32 Killed = 0;
	const int32 First = Links.Num();
	for (int32 i = 0; i < ArmyLinks; ++i)
	{
		if (FVector::Dist(LinkPoint(First + i), C) < R)
		{
			++Killed;
		}
	}
	ArmyLinks -= Killed;
	if (Killed > 0)
	{
		bAnnounced = IsComplete();
	}
	if (NumLinks() == 0)
	{
		Destroy();
	}
	return Killed;
}

AAntChain* AAntChain::FindNearTail(UWorld* World, uint8 InTeam, const FVector& P, float Radius)
{
	AAntChain* Best = nullptr;
	float BestD = Radius;
	for (TActorIterator<AAntChain> It(World); It; ++It)
	{
		if (It->Team != InTeam || !It->IsGrowing())
		{
			continue;
		}
		const float D = (float)FVector::Dist(It->TailPoint(), P);
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

void AAntChain::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority())
	{
		// Dead or vanished ants break the chain at that point.
		for (int32 i = 0; i < Links.Num(); ++i)
		{
			AAntCharacter* A = Links[i];
			if (!IsValid(A) || A->bDead || A->Chain != this)
			{
				TArray<AAntCharacter*> Fallen;
				for (int32 k = i + 1; k < Links.Num(); ++k) { Fallen.Add(Links[k]); }
				Links.SetNum(i);
				for (AAntCharacter* F : Fallen) { if (IsValid(F)) { F->SetChain(nullptr); } }
				if (i == 0 && ArmyLinks > 0)
				{
					Dissolve();
					return;
				}
				break;
			}
		}
		Life += Dt;
		if ((MaxLife > 0.f && Life > MaxLife) || NumLinks() == 0)
		{
			Dissolve();
			return;
		}
	}
	UpdateCollision();
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateArmyVisual(Dt);
	}
}

void AAntChain::UpdateCollision()
{
	const int32 Key = NumLinks() * 4 + (bBridge ? 1 : 0) + (IsComplete() ? 2 : 0);
	if (Key == ShapeKey)
	{
		return;
	}
	ShapeKey = Key;
	// Walkable: a ladder of 2+ ants, or a finished bridge.
	const bool bWalkable = (!bBridge && NumLinks() >= 2) || IsComplete();
	if (!bWalkable)
	{
		Walk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	const FVector A = Anchor;
	const FVector B = bBridge ? FVector(EndPoint) : TailPoint();
	const FVector D = B - A;
	const float Len = FMath::Max(10.f, (float)D.Size());
	Walk->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromXZ(D, FVector::UpVector).ToQuat());
	Walk->SetBoxExtent(FVector(Len * 0.5f, 16.f, 12.f));
	Walk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AAntChain::UpdateArmyVisual(float Dt)
{
	if (ArmyLinks <= 0 && !ArmyBodies)
	{
		return;
	}
	if (!ArmyBodies)
	{
		ArmyBodies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("ArmyBodies"));
		ArmyBodies->SetupAttachment(RootComponent);
		ArmyBodies->SetUsingAbsoluteLocation(true);
		ArmyBodies->SetUsingAbsoluteRotation(true);
		ArmyBodies->SetUsingAbsoluteScale(true);
		ArmyBodies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArmyBodies->SetCastShadow(true);
		const AntArt::FPart& Art = AntArt::Get(TEXT("SM_Ant_Body"));
		if (Art.IsValid())
		{
			ArmyBodies->SetStaticMesh(Art.Mesh);
			if (Art.Material) { ArmyBodies->SetMaterial(0, Art.Material); }
			if (Team == 1)
			{
				if (UMaterialInstanceDynamic* M = ArmyBodies->CreateDynamicMaterialInstance(0))
				{
					M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(2.2f, 0.6f, 0.35f));
				}
			}
		}
		else
		{
			ArmyBodies->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
			AntKit::Paint(ArmyBodies, Team == 1 ? FLinearColor(0.5f, 0.08f, 0.03f) : FLinearColor(0.05f, 0.02f, 0.015f));
		}
		ArmyBodies->RegisterComponent();
		if (UStaticMesh* M = ArmyBodies->GetStaticMesh())
		{
			const float L = (float)M->GetBoundingBox().GetSize().GetMax();
			InstScale = L > 0.f ? (LinkLen * 1.1f) / L : 0.4f;
		}
	}
	if (DrawnArmy != ArmyLinks)
	{
		DrawnArmy = ArmyLinks;
		ArmyBodies->ClearInstances();
		TArray<FTransform> Xf;
		Xf.SetNum(ArmyLinks);
		ArmyBodies->AddInstances(Xf, false, true);
	}
	if (ArmyLinks <= 0)
	{
		return;
	}
	// Each worker grips the one in front; little wriggles so it looks alive.
	const float T = GetWorld()->GetTimeSeconds();
	const int32 First = Links.Num();
	TArray<FTransform> Xf;
	Xf.SetNum(ArmyLinks);
	for (int32 i = 0; i < ArmyLinks; ++i)
	{
		const int32 Idx = First + i;
		const FVector P = LinkPoint(Idx);
		const FVector Prev = PointAt(Idx * LinkLen);
		const FVector Along = (Prev - P).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
		const FVector Wiggle = Side * FMath::Sin(T * 7.f + i * 1.3f) * 2.5f;
		const FRotator R = FRotationMatrix::MakeFromXZ(Along.IsNearlyZero() ? FVector::ForwardVector : Along, FVector::UpVector).Rotator() + FRotator(0.f, 0.f, (i % 2) ? 180.f : 0.f);
		Xf[i] = FTransform(R, P + Wiggle, FVector(InstScale));
	}
	ArmyBodies->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
}
