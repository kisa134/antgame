#include "AntSwing.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace
{
	/** The swinging parts of the hero props (SourceArt/Blender/hero/hero_manifest.json, "movers"): pivot in the parent mesh (cm, UE axes). */
	struct FSwingDef { const TCHAR* Parent; const TCHAR* Part; FVector Pivot; FVector Axis; float RestDeg; float LimitDeg; float LengthUu; };
	const FSwingDef GSwings[] =
	{
		{ TEXT("SM_K_Bucket"), TEXT("SM_K_Bucket_Handle"), FVector(0.f, 0.f, 30.7f), FVector(1.f, 0.f, 0.f), 112.f, 116.f, 285.f },
		{ TEXT("SM_V3_Pail"), TEXT("SM_V3_Pail_Handle"), FVector(0.f, 0.f, 23.3f), FVector(1.f, 0.f, 0.f), 112.f, 116.f, 210.f },   // KAN-23
	};
	constexpr float Gravity = 9000.f;   // uu/s^2, the world's (AntWreck::Gravity)
}

UAntSwingComponent::UAntSwingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	ComponentTags.Add(TEXT("AntSwing"));
}

UAntSwingComponent* UAntSwingComponent::AddTo(UStaticMeshComponent* Parent, const TCHAR* MeshName)
{
	if (!Parent || !Parent->GetOwner() || !Parent->GetStaticMesh())
	{
		return nullptr;
	}
	for (const FSwingDef& D : GSwings)
	{
		if (FCString::Strcmp(D.Parent, MeshName) != 0)
		{
			continue;
		}
		const FString Dir = FPackageName::GetLongPackagePath(Parent->GetStaticMesh()->GetOutermost()->GetName());
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Dir, D.Part, D.Part), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("AntSwing: %s has no %s"), MeshName, D.Part);
			return nullptr;
		}
		UAntSwingComponent* S = NewObject<UAntSwingComponent>(Parent->GetOwner());
		S->SetStaticMesh(M);
		S->SetMobility(EComponentMobility::Movable);
		S->Pivot = D.Pivot;
		S->Axis = D.Axis.GetSafeNormal();
		S->LimitDeg = D.LimitDeg;
		S->Length = D.LengthUu;
		// the rest side: the one where the part's top points to local -Y (the back of the piece, towards the wall)
		const FVector Up = FQuat(S->Axis, FMath::DegreesToRadians(10.f)).RotateVector(FVector::UpVector);
		S->AngleDeg = Up.Y < 0.f ? D.RestDeg : -D.RestDeg;
		S->SetupAttachment(Parent);
		S->Apply();
		S->RegisterComponent();
		Parent->GetOwner()->AddInstanceComponent(S);
		return S;
	}
	return nullptr;
}

void UAntSwingComponent::Apply()
{
	SetRelativeLocationAndRotation(Pivot, FQuat(Axis, FMath::DegreesToRadians(AngleDeg)));
}

void UAntSwingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	USceneComponent* P = GetAttachParent();
	if (!P || DeltaTime <= 0.f)
	{
		return;
	}
	if (IsVisible() != P->IsVisible()) { SetVisibility(P->IsVisible()); }
	const float Dt = FMath::Min(DeltaTime, 1.f / 30.f);
	const FVector Loc = P->GetComponentLocation();
	const FVector PVel = bHaveLast ? (Loc - LastParentLoc) / Dt : FVector::ZeroVector;
	const FVector PAcc = bHaveLast ? (PVel - LastParentVel) / Dt : FVector::ZeroVector;
	const bool bParentMoved = bHaveLast && (!Loc.Equals(LastParentLoc, 0.05f) || !P->GetComponentQuat().Equals(LastQuat, 1e-4f));
	LastParentVel = PVel; LastParentLoc = Loc; LastQuat = P->GetComponentQuat(); bHaveLast = true;
	if (!bAwake && !bParentMoved)
	{
		return;
	}
	bAwake = true;
	// pendulum: gravity and the parent's acceleration (felt backwards) pull the part's centre about the hinge axis
	const FQuat PQ = P->GetComponentQuat();
	const FVector A = PQ.RotateVector(Axis).GetSafeNormal();
	const FVector D = PQ.RotateVector(FQuat(Axis, FMath::DegreesToRadians(AngleDeg)).RotateVector(FVector::UpVector));
	const FVector Force = FVector(0.f, 0.f, -Gravity) - PAcc.GetClampedToMaxSize(60000.f);
	const float AccRad = FVector::DotProduct(FVector::CrossProduct(D, Force), A) / FMath::Max(1.f, Length);
	Vel += FMath::RadiansToDegrees(AccRad) * Dt;
	Vel -= Vel * FMath::Min(1.f, 2.2f * Dt);      // friction in the pins
	AngleDeg += Vel * Dt;
	if (FMath::Abs(AngleDeg) > LimitDeg)
	{
		AngleDeg = FMath::Sign(AngleDeg) * LimitDeg;
		Vel = -Vel * 0.25f;                         // it knocks against the bucket and drops back a little
	}
	Apply();
	Still = (FMath::Abs(Vel) < 2.f && !bParentMoved) ? Still + Dt : 0.f;
	if (Still > 0.6f)
	{
		bAwake = false;
		Vel = 0.f;
	}
}
