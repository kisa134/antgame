#include "KitchenLoot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AKitchenLoot::AKitchenLoot()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Root);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetGenerateOverlapEvents(false);
	Beam->SetCanEverAffectNavigation(false);
	Beam->SetCastShadow(false);
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	Beam->SetRelativeScale3D(FVector(0.04f, 0.04f, 4.f));
	Beam->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
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
	if (Art && Art->IsValid())
	{
		// Generated food model, a bit bigger than the old shape so it reads well.
		bHasArt = true;
		const float Size = FMath::Max(40.f, (float)VisualScale.GetMax() * 100.f * 1.4f);
		const float S = Size / FMath::Max(1.f, (float)Art->Bounds.GetSize().GetMax());
		Mesh->SetStaticMesh(Art->Mesh);
		Mesh->SetMaterial(0, Art->Material);
		Mesh->SetRelativeScale3D(FVector(S));
		ArtOffset = -Art->Bounds.GetCenter() * S;
		MeshMID = nullptr;
	}
	else
	{
		bHasArt = false;
		ArtOffset = FVector::ZeroVector;
		Mesh->SetStaticMesh(AntKit::ShapeMesh((EAntShape)ShapeIndex));
		Mesh->SetRelativeScale3D(VisualScale);
		if (!MeshMID) { MeshMID = AntKit::Paint(Mesh, Col); }
		else { MeshMID->SetVectorParameterValue(TEXT("Color"), Col); }
	}

	const bool bShowBeam = bIsGear && Gear.Rarity >= 1;
	Beam->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cylinder));
	Beam->SetVisibility(bShowBeam);
	if (!BeamMID) { BeamMID = AntKit::Paint(Beam, Col); }
	else { BeamMID->SetVectorParameterValue(TEXT("Color"), Col); }

	const bool bGlow = bIsGear || Value >= 100;
	Glow->SetLightColor(Col);
	Glow->SetIntensity(bIsGear ? 2.f + Gear.Rarity * 1.5f : 2.5f);
	Glow->SetVisibility(bGlow);
}

void AKitchenLoot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Phase += DeltaSeconds;
	Mesh->SetRelativeLocation(ArtOffset + FVector(0.f, 0.f, FMath::Sin(Phase * 2.5f) * 6.f));
	Mesh->AddRelativeRotation(FRotator(0.f, 45.f * DeltaSeconds, 0.f));
}
