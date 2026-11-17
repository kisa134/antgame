#include "AntGibs.h"
#include "AntTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AAntGibs::AAntGibs()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAntGibs::AddCopy(const UStaticMeshComponent* Src, const FVector& Velocity, float Spin, const FVector& ScaleMul, const FVector& Offset)
{
	if (!Src || !Src->GetStaticMesh())
	{
		return;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Src->GetStaticMesh());
	for (int32 i = 0; i < Src->GetNumMaterials(); ++i)
	{
		C->SetMaterial(i, Src->GetMaterial(i));
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	FTransform T = Src->GetComponentTransform();
	T.SetScale3D(T.GetScale3D() * ScaleMul);
	T.AddToTranslation(Offset);
	C->SetWorldTransform(T);
	Comps.Add(C);
	FPiece P;
	P.Velocity = Velocity;
	P.Axis = FMath::VRand();
	P.Spin = Spin;
	P.Radius = FMath::Max(0.4f, C->Bounds.SphereRadius * 0.35f);
	Pieces.Add(P);
}

void AAntGibs::AddGoo(const FVector& From, const FVector& Velocity, float Size, const FLinearColor& Color)
{
	UStaticMeshComponent* C = AntKit::AddPart(this, RootComponent, EAntShape::Sphere, FVector::ZeroVector, FVector(Size / 100.f), Color);
	if (!C)
	{
		return;
	}
	C->SetCastShadow(false);
	C->SetWorldLocation(From);
	Comps.Add(C);
	FPiece P;
	P.Velocity = Velocity;
	P.Radius = Size * 0.5f;
	P.GooSize = Size;
	Pieces.Add(P);
}

void AAntGibs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age > Life + 1.5f)
	{
		Destroy();
		return;
	}
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntGibs), false, this);
	const FCollisionObjectQueryParams Solid(ECC_WorldStatic);
	for (int32 i = 0; i < Comps.Num(); ++i)
	{
		UStaticMeshComponent* C = Comps[i];
		FPiece& P = Pieces[i];
		if (!C)
		{
			continue;
		}
		if (Age > Life)
		{
			C->SetWorldScale3D(C->GetComponentScale() * FMath::Clamp(1.f - DeltaSeconds / 1.5f, 0.f, 1.f));
		}
		if (P.bRest)
		{
			continue;
		}
		P.Velocity.Z -= 1500.f * DeltaSeconds;
		const FVector From = C->GetComponentLocation();
		const FVector Step = P.Velocity * DeltaSeconds;
		const FVector To = From + Step;
		FHitResult Hit;
		if (!Step.IsNearlyZero() && World->LineTraceSingleByObjectType(Hit, From, To + Step.GetSafeNormal() * P.Radius, Solid, Params))
		{
			const FVector N = Hit.ImpactNormal;
			if (P.GooSize > 0.f)
			{
				// Splat: flat on whatever it hit.
				const float S = P.GooSize * FMath::FRandRange(1.5f, 2.6f) / 100.f;
				C->SetWorldLocationAndRotation(Hit.ImpactPoint + N * 0.15f, FRotationMatrix::MakeFromZ(N).ToQuat());
				C->SetWorldScale3D(FVector(S, S * FMath::FRandRange(0.6f, 1.f), 0.004f * P.GooSize));
				P.bRest = true;
				continue;
			}
			const float Into = FVector::DotProduct(P.Velocity, N);
			FVector Along = P.Velocity - Into * N;
			P.Velocity = Along * 0.55f - N * Into * 0.32f;
			P.Spin *= 0.5f;
			C->SetWorldLocation(Hit.ImpactPoint + N * P.Radius);
			if (P.Velocity.SizeSquared() < 45.f * 45.f)
			{
				P.bRest = true;
			}
			continue;
		}
		C->SetWorldLocation(To);
		if (P.Spin != 0.f)
		{
			C->AddWorldRotation(FQuat(P.Axis, P.Spin * DeltaSeconds));
		}
		if (To.Z < -2000.f)
		{
			P.bRest = true;
			C->SetVisibility(false);
		}
	}
}