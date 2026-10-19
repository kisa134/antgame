#include "KitchenInteractables.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "KitchenBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace InteractColors
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	const FLinearColor White = Hex(245, 245, 245);
	const FLinearColor Steel = Hex(175, 180, 185);
	const FLinearColor DarkSteel = Hex(70, 72, 78);
	const FLinearColor Black = Hex(20, 20, 22);
	const FLinearColor Cabinet = Hex(145, 205, 185);
	const FLinearColor Wood = Hex(160, 105, 60);
	const FLinearColor LightWood = Hex(205, 160, 105);
	const FLinearColor Copper = Hex(220, 120, 50);
	const FLinearColor Good(0.4f, 1.f, 0.4f);
	const FLinearColor Info(0.9f, 0.9f, 0.9f);
	const FLinearColor Warn(1.f, 0.7f, 0.2f);
}
using namespace InteractColors;

// =============================================================================================
// Base
// =============================================================================================

AKitchenInteractable::AKitchenInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

float AKitchenInteractable::DistanceTo(const FVector& P) const
{
	if (UsePointsLocal.Num() == 0)
	{
		return FVector::Dist(P, GetActorLocation());
	}
	float Best = TNumericLimits<float>::Max();
	const FTransform TM = GetActorTransform();
	for (const FVector& L : UsePointsLocal)
	{
		Best = FMath::Min(Best, (float)FVector::Dist(P, TM.TransformPosition(L)));
	}
	return Best;
}

UStaticMeshComponent* AKitchenInteractable::Part(USceneComponent* Parent, EAntShape Shape, const FVector& Center, const FVector& Size,
	const FLinearColor& Color, bool bCollide, const FRotator& Rot)
{
	UStaticMeshComponent* C = AntKit::AddPart(this, Parent ? Parent : Root, Shape, Center, Size / 100.f, Color, Rot, bCollide, false);
	AKitchenBuilder::Surface(C, Color);
	return C;
}

UStaticMeshComponent* AKitchenInteractable::BoxPart(USceneComponent* Parent, const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide)
{
	return Part(Parent, EAntShape::Cube, (Min + Max) * 0.5f, Max - Min, Color, bCollide);
}

void AKitchenInteractable::Broadcast(const FString& Text, const FLinearColor& Color) const
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->Broadcast(Text, Color);
	}
}

float AKitchenInteractable::Now() const
{
	return GetWorld()->GetTimeSeconds();
}

namespace
{
	UPointLightComponent* MakeLight(AActor* Owner, USceneComponent* Parent, const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(Parent);
		L->SetRelativeLocation(Loc);
		L->IntensityUnits = ELightUnits::Unitless;
		L->bUseInverseSquaredFalloff = false;
		L->LightFalloffExponent = 2.f;
		L->Intensity = Intensity;
		L->AttenuationRadius = Radius;
		L->CastShadows = false;
		L->SetLightColor(Color);
		L->RegisterComponent();
		Owner->AddInstanceComponent(L);
		return L;
	}

	bool InsideLocalBox(const AActor* A, const FVector& World, const FBox& LocalBox)
	{
		return LocalBox.IsInside(A->GetActorTransform().InverseTransformPosition(World));
	}
}

// =============================================================================================
// Doors: fridge / oven / drawer
// =============================================================================================

AKitchenDoor::AKitchenDoor()
{
	InteractRadius = 320.f;
}

void AKitchenDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenDoor, Kind);
	DOREPLIFETIME(AKitchenDoor, bOpen);
}

void AKitchenDoor::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Alpha = bOpen ? 1.f : 0.f;
	ApplyAlpha();
}

void AKitchenDoor::Build()
{
	Hinge = AntKit::AddPivot(this, Root, FVector::ZeroVector);

	switch (Kind)
	{
	case EKitchenDoorKind::Fridge:
	{
		// Hinge = east-front-bottom corner of the fridge; the door extends west (-X), outside is -Y.
		UStaticMeshComponent* Slab = BoxPart(Hinge, FVector(-2000.f, -100.f, 0.f), FVector(0.f, 0.f, 2380.f), White, true);
		UStaticMeshComponent* Handle = BoxPart(Hinge, FVector(-1910.f, -150.f, 1300.f), FVector(-1850.f, -100.f, 2200.f), Steel, true);   // handle
		UStaticMeshComponent* M1 = BoxPart(Hinge, FVector(-1200.f, -104.f, 1880.f), FVector(-1050.f, -100.f, 2030.f), Hex(240, 60, 60), false);  // magnets
		UStaticMeshComponent* M2 = BoxPart(Hinge, FVector(-800.f, -104.f, 1680.f), FVector(-700.f, -100.f, 1780.f), Hex(60, 160, 240), false);
		UStaticMeshComponent* Note = BoxPart(Hinge, FVector(-1400.f, -103.f, 1530.f), FVector(-1000.f, -100.f, 1830.f), White * 0.9f, false);       // a note
		if (AntArt::DressAuto(this, Hinge, TEXT("SM_Fridge_Door"), FVector(-2000.f, -160.f, 0.f), FVector(0.f, -2.f, 2380.f), 0.f, 4.f))
		{
			AntArt::HideAll({ Slab, Handle, M1, M2, Note });
		}
		// Door bins on the inside
		for (float Z : { 500.f, 1300.f })
		{
			BoxPart(Hinge, FVector(-1850.f, 0.f, Z), FVector(-150.f, 150.f, Z + 30.f), Hex(225, 230, 235), true);
			BoxPart(Hinge, FVector(-1850.f, 130.f, Z + 30.f), FVector(-150.f, 150.f, Z + 140.f), Hex(200, 225, 235), true);
		}
		Part(Hinge, EAntShape::Cylinder, FVector(-600.f, 75.f, 735.f), FVector(110.f, 110.f, 410.f), Hex(60, 140, 70), true);   // bottle
		Part(Hinge, EAntShape::Cylinder, FVector(-1300.f, 75.f, 1450.f), FVector(90.f, 90.f, 260.f), Hex(200, 60, 40), true);   // ketchup
		UsePointsLocal = { FVector(-1850.f, -150.f, -90.f), FVector(-1500.f, 300.f, 110.f), FVector(-1000.f, 300.f, 110.f) };
		InnerLight = MakeLight(this, Root, FVector(-1000.f, 800.f, 2000.f), 6.f, FLinearColor(0.85f, 0.95f, 1.f), 2600.f);
		break;
	}
	case EKitchenDoorKind::Oven:
	{
		// Hinge = bottom-front edge; the door folds down into a ramp.
		UStaticMeshComponent* Slab = BoxPart(Hinge, FVector(-700.f, -14.f, 0.f), FVector(700.f, 0.f, 1200.f), Hex(35, 35, 38), true);
		UStaticMeshComponent* Win = BoxPart(Hinge, FVector(-450.f, -17.f, 420.f), FVector(450.f, -14.f, 980.f), Hex(45, 30, 20), false);    // window
		UStaticMeshComponent* Handle = BoxPart(Hinge, FVector(-550.f, -70.f, 1080.f), FVector(550.f, -40.f, 1130.f), Steel, true);           // handle
		UStaticMeshComponent* H1 = BoxPart(Hinge, FVector(-540.f, -40.f, 1090.f), FVector(-500.f, -14.f, 1120.f), Steel, false);
		UStaticMeshComponent* H2 = BoxPart(Hinge, FVector(500.f, -40.f, 1090.f), FVector(540.f, -14.f, 1120.f), Steel, false);
		if (AntArt::DressAuto(this, Hinge, TEXT("SM_Oven_Door"), FVector(-700.f, -75.f, 0.f), FVector(700.f, 0.f, 1200.f), 0.f, 3.f))
		{
			AntArt::HideAll({ Slab, Win, Handle, H1, H2 });
		}
		UsePointsLocal = { FVector(0.f, -150.f, -274.f), FVector(0.f, 500.f, 26.f), FVector(0.f, -700.f, 26.f) };
		InnerLight = MakeLight(this, Root, FVector(0.f, 900.f, 1000.f), 3.f, FLinearColor(1.f, 0.45f, 0.1f), 1500.f);
		break;
	}
	case EKitchenDoorKind::Drawer:
	{
		UStaticMeshComponent* Front = BoxPart(Hinge, FVector(-650.f, -20.f, 0.f), FVector(650.f, 0.f, 270.f), Cabinet, true);      // front
		UStaticMeshComponent* Handle = BoxPart(Hinge, FVector(-120.f, -50.f, 150.f), FVector(120.f, -20.f, 185.f), Steel, true);    // handle
		{
			// The generated drawer: a flat front panel, or a whole box (then it covers the tray too).
			const AntArt::FPart& DA = AntArt::Get(TEXT("SM_Drawer"));
			if (DA.IsValid())
			{
				const FVector E = DA.Bounds.GetSize();
				const bool bFlat = FMath::Min(E.X, E.Y) < 0.3f * FMath::Max(E.X, E.Y);
				const FVector Max = bFlat ? FVector(650.f, 0.f, 270.f) : FVector(650.f, 1600.f, 270.f);
				if (AntArt::DressAuto(this, Hinge, TEXT("SM_Drawer"), FVector(-650.f, -55.f, 0.f), Max, 0.f, 3.f))
				{
					AntArt::HideAll({ Front, Handle });
				}
			}
		}
		BoxPart(Hinge, FVector(-630.f, 0.f, 0.f), FVector(630.f, 1600.f, 20.f), LightWood, true);    // bottom
		BoxPart(Hinge, FVector(-650.f, 0.f, 0.f), FVector(-630.f, 1600.f, 240.f), LightWood, true);
		BoxPart(Hinge, FVector(630.f, 0.f, 0.f), FVector(650.f, 1600.f, 240.f), LightWood, true);
		BoxPart(Hinge, FVector(-630.f, 1580.f, 0.f), FVector(630.f, 1600.f, 240.f), LightWood, true);
		for (int32 i = 0; i < 3; ++i)   // cutlery
		{
			const float X = -480.f + i * 70.f;
			BoxPart(Hinge, FVector(X, 200.f, 20.f), FVector(X + 30.f, 1000.f, 32.f), Steel, true);
			Part(Hinge, EAntShape::Sphere, FVector(X + 15.f, 1080.f, 30.f), FVector(70.f, 110.f, 20.f), Steel, false);
		}
		UsePointsLocal = { FVector(0.f, -30.f, 120.f), FVector(0.f, -30.f, -60.f) };
		break;
	}
	}
}

void AKitchenDoor::ApplyAlpha()
{
	if (!Hinge)
	{
		return;
	}
	switch (Kind)
	{
	case EKitchenDoorKind::Fridge:
		Hinge->SetRelativeRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(100.f * Alpha)));
		break;
	case EKitchenDoorKind::Oven:
		Hinge->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(88.f * Alpha)));
		break;
	case EKitchenDoorKind::Drawer:
		Hinge->SetRelativeLocation(FVector(0.f, -1100.f * Alpha, 0.f));
		break;
	}
	if (InnerLight && Kind == EKitchenDoorKind::Fridge)
	{
		InnerLight->SetVisibility(Alpha > 0.05f);
	}
}

void AKitchenDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		const float Speed = (Kind == EKitchenDoorKind::Drawer) ? 1.6f : 1.2f;
		Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, Speed);
		ApplyAlpha();
	}

	if (HasAuthority() && bOpen && AutoCloseAt > 0.f && Now() >= AutoCloseAt)
	{
		SetOpen(false);
		if (Kind == EKitchenDoorKind::Fridge)
		{
			Broadcast(TEXT("Холодильник захлопнулся! Кто внутри — открывай изнутри [E]"), Warn);
		}
		else if (Kind == EKitchenDoorKind::Oven)
		{
			Broadcast(TEXT("Духовка закрылась! Внутри становится очень жарко"), Warn);
		}
	}
}

void AKitchenDoor::SetOpen(bool bInOpen, float AutoClose)
{
	const bool bWasOpen = bOpen;
	bOpen = bInOpen;
	if (HasAuthority() && bOpen && !bWasOpen && Kind == EKitchenDoorKind::Fridge)
	{
		AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation(), 1.f);
	}
	AutoCloseAt = (bInOpen && AutoClose > 0.f) ? Now() + AutoClose : -1.f;

	if (bOpen && Kind == EKitchenDoorKind::Drawer && !bLootSpawned && HasAuthority())
	{
		bLootSpawned = true;
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			const FVector Base = GetActorLocation() + FVector(0.f, -1100.f, 55.f);
			GM->SpawnFood(Base + FVector(-320.f, 500.f, 0.f), 2);
			GM->SpawnFood(Base + FVector(250.f, 900.f, 0.f), 3);
			GM->SpawnFood(Base + FVector(-60.f, 1350.f, 0.f), 2);
			FRandomStream R(FMath::Rand());
			GM->SpawnGear(Base + FVector(380.f, 1250.f, 0.f), AntKit::RollWeapon(R.RandRange(1, 3), R));
		}
	}
}

FString AKitchenDoor::GetPrompt(const AAntCharacter* Ant) const
{
	switch (Kind)
	{
	case EKitchenDoorKind::Fridge: return bOpen ? TEXT("Захлопнуть холодильник") : TEXT("Подцепить дверь холодильника");
	case EKitchenDoorKind::Oven: return bOpen ? TEXT("Закрыть духовку") : TEXT("Открыть духовку (там печенье... и жар)");
	case EKitchenDoorKind::Drawer: return bOpen ? TEXT("Задвинуть ящик") : TEXT("Выдвинуть ящик");
	}
	return FString();
}

void AKitchenDoor::Interact(AAntCharacter* Ant)
{
	switch (Kind)
	{
	case EKitchenDoorKind::Fridge:
		SetOpen(!bOpen, 15.f);
		if (bOpen && Ant)
		{
			Ant->Toast(TEXT("Холодильник открыт на 15 секунд. Внутри холодно — не застрянь!"));
		}
		break;
	case EKitchenDoorKind::Oven:
		SetOpen(!bOpen, 12.f);
		if (bOpen && Ant)
		{
			Ant->Toast(TEXT("Духовка включена! Хватай печенье и беги"));
		}
		break;
	case EKitchenDoorKind::Drawer:
	{
		const bool bFirst = !bLootSpawned;
		SetOpen(!bOpen);
		if (bOpen && bFirst && Ant)
		{
			Broadcast(FString::Printf(TEXT("%s нашёл заначку в ящике со сладостями!"), *Ant->GetDisplayName()), Good);
		}
		break;
	}
	}
}

void AKitchenDoor::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	if (Kind == EKitchenDoorKind::Fridge)
	{
		if (FBox(FVector(2780.f, 3100.f, 200.f), FVector(4620.f, 4670.f, 2500.f)).IsInside(P))
		{
			Env.Temp(-7.f, bOpen ? 3.5f : 6.f);
		}
		else if (FBox(FVector(2780.f, 3100.f, 2560.f), FVector(4620.f, 4670.f, 3620.f)).IsInside(P))
		{
			Env.Temp(-20.f, 9.f);
		}
	}
	else if (Kind == EKitchenDoorKind::Oven)
	{
		if (FBox(FVector(-1450.f, 3000.f, 300.f), FVector(-50.f, 4700.f, 1500.f)).IsInside(P))
		{
			Env.Temp(bOpen ? 90.f : 170.f, bOpen ? 9.f : 35.f);
		}
	}
}

// =============================================================================================
// Faucet
// =============================================================================================

AKitchenFaucet::AKitchenFaucet()
{
	InteractRadius = 230.f;
	UsePointsLocal = { HotValve(), ColdValve() };
	SetNetUpdateFrequency(10.f);
}

void AKitchenFaucet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenFaucet, bOn);
	DOREPLIFETIME(AKitchenFaucet, bHot);
	DOREPLIFETIME(AKitchenFaucet, WaterLevel);
}

void AKitchenFaucet::BeginPlay()
{
	Super::BeginPlay();
	ShownLevel = WaterLevel;
	if (GetNetMode() != NM_DedicatedServer)
	{
		Water = Part(Root, EAntShape::Cube, FVector::ZeroVector, FVector(100.f), FLinearColor(0.3f, 0.55f, 0.95f), false);
		Water->SetCastShadow(false);
		WaterMID = Cast<UMaterialInstanceDynamic>(Water->GetMaterial(0));
		StreamMesh = Part(Root, EAntShape::Cylinder, FVector::ZeroVector, FVector(100.f), FLinearColor(0.55f, 0.75f, 1.f), false);
		StreamMesh->SetCastShadow(false);
	}
}

void AKitchenFaucet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		WaterLevel = FMath::Clamp(WaterLevel + (bOn ? 32.f : -14.f) * DeltaSeconds, BasinFloor, BasinTop);
	}
	ShownLevel = FMath::FInterpTo(ShownLevel, WaterLevel, DeltaSeconds, 4.f);

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bOn && !FlowAudio)
		{
			if (USoundBase* S = AntSfx::Get(AntSfx::Faucet))
			{
				FlowAudio = UGameplayStatics::SpawnSoundAtLocation(this, S, GetActorTransform().TransformPosition(Spout()),
					FRotator::ZeroRotator, 0.8f, 1.f, 0.f, AntSfx::Attenuation(), nullptr, true);
			}
		}
		else if (!bOn && FlowAudio)
		{
			FlowAudio->FadeOut(0.5f, 0.f);
			FlowAudio = nullptr;
		}
	}

	if (Water)
	{
		const FBox B = Basin();
		const bool bVisible = ShownLevel > BasinFloor + 4.f;
		Water->SetVisibility(bVisible);
		if (bVisible)
		{
			const FVector C = B.GetCenter();
			Water->SetRelativeLocation(FVector(C.X, C.Y, ShownLevel - 3.f));
			Water->SetRelativeScale3D(FVector((B.Max.X - B.Min.X) / 100.f, (B.Max.Y - B.Min.Y) / 100.f, 0.06f));
		}
		if (WaterMID)
		{
			WaterMID->SetVectorParameterValue(TEXT("Color"), bHot ? FLinearColor(0.5f, 0.72f, 0.85f) : FLinearColor(0.25f, 0.5f, 0.95f));
		}
	}
	if (StreamMesh)
	{
		StreamMesh->SetVisibility(bOn);
		if (bOn)
		{
			const FVector S = Spout();
			const float Bottom = FMath::Max(BasinFloor, ShownLevel);
			const float Len = FMath::Max(10.f, (float)S.Z - Bottom);
			const float Wobble = 1.f + 0.08f * FMath::Sin(Now() * 30.f);
			StreamMesh->SetRelativeLocation(FVector(S.X, S.Y, Bottom + Len * 0.5f));
			StreamMesh->SetRelativeScale3D(FVector(0.6f * Wobble, 0.6f * Wobble, Len / 100.f));
		}
	}
}

FString AKitchenFaucet::GetPrompt(const AAntCharacter* Ant) const
{
	const bool bHotValve = FVector::Dist(Ant->GetActorLocation(), HotValve()) < FVector::Dist(Ant->GetActorLocation(), ColdValve());
	if (bOn && bHot == bHotValve)
	{
		return bHotValve ? TEXT("Закрутить горячую воду") : TEXT("Закрутить холодную воду");
	}
	return bHotValve ? TEXT("Открутить ГОРЯЧУЮ воду") : TEXT("Открутить холодную воду");
}

void AKitchenFaucet::Interact(AAntCharacter* Ant)
{
	const bool bHotValve = FVector::Dist(Ant->GetActorLocation(), HotValve()) < FVector::Dist(Ant->GetActorLocation(), ColdValve());
	if (bOn && bHot == bHotValve)
	{
		SetWater(false, bHot);
		Ant->Toast(TEXT("Вода выключена, раковина понемногу стекает"));
	}
	else
	{
		SetWater(true, bHotValve);
		Ant->Toast(bHotValve ? TEXT("Горячая вода! Кипяток в раковине") : TEXT("Холодная вода пошла. Раковина наполняется"));
	}
}

void AKitchenFaucet::SetWater(bool bInOn, bool bHotWater)
{
	bOn = bInOn;
	bHot = bHotWater;
}

void AKitchenFaucet::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	const FBox B = Basin();
	const bool bInBasinXY = P.X > B.Min.X && P.X < B.Max.X && P.Y > B.Min.Y && P.Y < B.Max.Y;

	if (bInBasinXY && WaterLevel > BasinFloor + 15.f && P.Z < WaterLevel + 8.f && P.Z > BasinFloor - 60.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(bHot ? 62.f : 12.f, 10.f);
		if (P.Z < WaterLevel - 60.f)
		{
			Env.Flags |= AntStatus::Drowning;
			Env.Hurt(7.f, TEXT("Тонешь! Выбирайся из воды"));
		}
	}

	if (bOn)
	{
		const FVector S = Spout();
		if (FVector::Dist2D(P, S) < 75.f && P.Z < S.Z && P.Z > WaterLevel - 40.f)
		{
			Env.Flags |= AntStatus::Wet | AntStatus::Stream;
			Env.Hurt(6.f, TEXT("Струя воды сбивает с ног!"));
			float& Last = LastKnock.FindOrAdd(Ant);
			if (Now() - Last > 1.2f)
			{
				Last = Now();
				const FVector Side = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
				Ant->ApplyLaunch(Side * 520.f + FVector(0.f, 0.f, -150.f));
			}
		}
	}
}

// =============================================================================================
// Light switch
// =============================================================================================

AKitchenSwitch::AKitchenSwitch()
{
	InteractRadius = 230.f;
	UsePointsLocal = { FVector(26.f, 0.f, -40.f), FVector(26.f, 0.f, 60.f) };
}

void AKitchenSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenSwitch, bLightsOn);
}

void AKitchenSwitch::BeginPlay()
{
	Super::BeginPlay();
	// Local +X points out of the wall.
	UStaticMeshComponent* Plate = BoxPart(Root, FVector(0.f, -70.f, -100.f), FVector(10.f, 70.f, 100.f), Hex(235, 230, 215), true);
	UStaticMeshComponent* Frame = BoxPart(Root, FVector(8.f, -25.f, -45.f), FVector(14.f, 25.f, 45.f), Hex(190, 185, 170), false);
	if (AntArt::Dress(this, Root, TEXT("SM_LightSwitch"), FVector(0.f, -80.f, -110.f), FVector(18.f, 80.f, 110.f), -90.f))
	{
		AntArt::HideAll({ Plate, Frame });
	}
	Toggle = AntKit::AddPivot(this, Root, FVector(12.f, 0.f, 0.f));
	BoxPart(Toggle, FVector(0.f, -18.f, -12.f), FVector(34.f, 18.f, 12.f), Hex(250, 248, 240), true);
	OnRep_Lights();
}

FString AKitchenSwitch::GetPrompt(const AAntCharacter* Ant) const
{
	return bLightsOn ? TEXT("Выключить свет (Человек ослепнет)") : TEXT("Включить свет");
}

void AKitchenSwitch::Interact(AAntCharacter* Ant)
{
	SetLights(!bLightsOn);
	Broadcast(bLightsOn
		? FString::Printf(TEXT("%s включил свет"), *Ant->GetDisplayName())
		: FString::Printf(TEXT("%s ВЫКЛЮЧИЛ СВЕТ! Человек почти ничего не видит"), *Ant->GetDisplayName()),
		bLightsOn ? Info : Good);
}

void AKitchenSwitch::SetLights(bool bOn)
{
	bLightsOn = bOn;
	OnRep_Lights();
}

void AKitchenSwitch::OnRep_Lights()
{
	if (Toggle)
	{
		Toggle->SetRelativeRotation(FRotator(bLightsOn ? 25.f : -25.f, 0.f, 0.f));
	}
	for (TActorIterator<AKitchenBuilder> It(GetWorld()); It; ++It)
	{
		It->SetMainLights(bLightsOn);
	}
}

bool AKitchenSwitch::AreLightsOn(const UWorld* World)
{
	for (TActorIterator<AKitchenSwitch> It(World); It; ++It)
	{
		return It->bLightsOn;
	}
	return true;
}

// =============================================================================================
// Passages: vents, drains, pipes
// =============================================================================================

AKitchenPassage::AKitchenPassage()
{
	InteractRadius = 320.f;
}

void AKitchenPassage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenPassage, Exit);
	DOREPLIFETIME(AKitchenPassage, RouteName);
	DOREPLIFETIME(AKitchenPassage, Look);
}

void AKitchenPassage::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Grille = Hex(150, 150, 145);
	switch (Look)
	{
	case EPassageLook::Vent:
		// Local +X points out of the wall; the bottom of the grille is at the actor.
		BoxPart(Root, FVector(0.f, -170.f, 10.f), FVector(6.f, 170.f, 230.f), Hex(15, 15, 15), false);
		if (AntArt::Dress(this, Root, TEXT("SM_WallVent"), FVector(0.f, -190.f, -5.f), FVector(20.f, 190.f, 245.f), -90.f))
		{
			break;   // generated grille
		}
		BoxPart(Root, FVector(6.f, -180.f, 0.f), FVector(16.f, 180.f, 16.f), Grille, false);
		BoxPart(Root, FVector(6.f, -180.f, 224.f), FVector(16.f, 180.f, 240.f), Grille, false);
		BoxPart(Root, FVector(6.f, -180.f, 0.f), FVector(16.f, -164.f, 240.f), Grille, false);
		BoxPart(Root, FVector(6.f, 164.f, 0.f), FVector(16.f, 180.f, 240.f), Grille, false);
		for (int32 i = 1; i < 6; ++i)
		{
			const float Z = i * 40.f;
			BoxPart(Root, FVector(6.f, -164.f, Z - 5.f), FVector(14.f, 164.f, Z + 5.f), Grille, false);
		}
		break;
	case EPassageLook::Drain:
		Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 1.f), FVector(150.f, 150.f, 4.f), Steel, false);
		Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 3.f), FVector(100.f, 100.f, 2.f), Black, false);
		break;
	case EPassageLook::Hole:
		BoxPart(Root, FVector(0.f, -80.f, 0.f), FVector(4.f, 80.f, 120.f), Black, false);
		Part(Root, EAntShape::Sphere, FVector(2.f, 0.f, 120.f), FVector(4.f, 160.f, 80.f), Black, false);
		break;
	case EPassageLook::Pipe:
		Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 60.f), FVector(110.f, 110.f, 120.f), Hex(235, 235, 230), true);
		Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 121.f), FVector(80.f, 80.f, 2.f), Black, false);
		break;
	}
}

FString AKitchenPassage::GetPrompt(const AAntCharacter* Ant) const
{
	if (Ant && Ant->IsCarrying())
	{
		return FString::Printf(TEXT("%s (с таким грузом не пролезешь!)"), *RouteName);
	}
	return FString::Printf(TEXT("Пролезть: %s"), *RouteName);
}

void AKitchenPassage::Interact(AAntCharacter* Ant)
{
	if (!Ant)
	{
		return;
	}
	if (Ant->IsCarrying())
	{
		Ant->Toast(TEXT("Слишком толстый с таким грузом. Сбрось часть или тащи в обход"));
		return;
	}
	Ant->ApplyTeleport(Exit);
	Ant->Toast(FString::Printf(TEXT("Ты пролез: %s"), *RouteName));
}

// =============================================================================================
// Toaster
// =============================================================================================

AKitchenToaster::AKitchenToaster()
{
	InteractRadius = 300.f;
	UsePointsLocal = { FVector(-5870.f, 4200.f, 2000.f), FVector(-5860.f, 4200.f, 1740.f) };
}

void AKitchenToaster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenToaster, bArmed);
}

void AKitchenToaster::BeginPlay()
{
	Super::BeginPlay();
	BoxPart(Root, FVector(-5902.f, 4185.f, 1780.f), FVector(-5898.f, 4215.f, 2090.f), Black, false);   // lever slot
	Lever = AntKit::AddPivot(this, Root, FVector(-5900.f, 4200.f, 2060.f));
	UStaticMeshComponent* Knob = BoxPart(Lever, FVector(0.f, -45.f, -22.f), FVector(55.f, 45.f, 22.f), Black, true);
	if (AntArt::DressAuto(this, Lever, TEXT("SM_Toaster_Lever"), FVector(0.f, -55.f, -30.f), FVector(80.f, 55.f, 30.f)))
	{
		AntArt::HideAll({ Knob });
	}
	for (float Y : { 4085.f, 4315.f })
	{
		Toasts.Add(BoxPart(Root, FVector(-6420.f, Y - 25.f, 1950.f), FVector(-6080.f, Y + 25.f, 2140.f), Hex(215, 150, 70), false));
	}
}

bool AKitchenToaster::CanInteract(const AAntCharacter* Ant) const
{
	return !bArmed && (!HasAuthority() || Now() >= ReadyAt);
}

FString AKitchenToaster::GetPrompt(const AAntCharacter* Ant) const
{
	return TEXT("Нажать рычаг тостера (катапульта!)");
}

void AKitchenToaster::Interact(AAntCharacter* Ant)
{
	if (!CanInteract(Ant))
	{
		return;
	}
	bArmed = true;
	PopAt = Now() + 1.8f;
	Ant->Toast(TEXT("Тостер греется... запрыгивай в щель и жди ДЗЫНЬ!"));
}

void AKitchenToaster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Lever)
	{
		const FVector Target(-5900.f, 4200.f, bArmed ? 1820.f : 2060.f);
		Lever->SetRelativeLocation(FMath::VInterpTo(Lever->GetRelativeLocation(), Target, DeltaSeconds, 12.f));
	}
	ToastAnim = FMath::Max(0.f, ToastAnim - DeltaSeconds * 0.6f);
	for (int32 i = 0; i < Toasts.Num(); ++i)
	{
		const float Up = FMath::Sin(FMath::Clamp(ToastAnim, 0.f, 1.f) * PI) * 320.f;
		Toasts[i]->SetRelativeLocation(FVector(-6250.f, i == 0 ? 4085.f : 4315.f, 2045.f + Up));
	}

	if (HasAuthority() && bArmed && Now() >= PopAt)
	{
		Pop();
	}
}

void AKitchenToaster::Pop()
{
	bArmed = false;
	ReadyAt = Now() + 6.f;
	MulticastPop();
	AntSfx::Everyone(this, AntSfx::Toaster, FVector(-6000.f, 4200.f, 1900.f), 1.f);

	int32 Launched = 0;
	FString Who;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Slots().IsInside(Ant->GetActorLocation()))
		{
			Ant->ApplyLaunch(FVector(FMath::FRandRange(-120.f, 120.f), 320.f, 2750.f));
			Who = Ant->GetDisplayName();
			++Launched;
		}
	}
	if (Launched > 0)
	{
		Broadcast(FString::Printf(TEXT("ДЗЫНЬ! %s выстрелил собой из тостера!"), *Who), Good);
	}
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->SpawnFood(FVector(-5700.f, 3820.f, 1745.f), 0);
		GM->SpawnFood(FVector(-5740.f, 4560.f, 1745.f), 1);
	}
}

void AKitchenToaster::MulticastPop_Implementation()
{
	ToastAnim = 1.f;
}

void AKitchenToaster::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	if (bArmed && Slots().IsInside(Ant->GetActorLocation()))
	{
		Env.Temp(85.f, 40.f);
	}
}

// =============================================================================================
// Exposed wire
// =============================================================================================

namespace WireSpot
{
	const FVector Outlet(3200.f, 4990.f, 700.f);
	const FVector Floor(3200.f, 4955.f, 18.f);
	const FVector End(3520.f, 4860.f, 16.f);
}

AKitchenWire::AKitchenWire()
{
	InteractRadius = 0.f;
}

void AKitchenWire::BeginPlay()
{
	Super::BeginPlay();
	using namespace WireSpot;

	BoxPart(Root, Outlet + FVector(-70.f, -12.f, -80.f), Outlet + FVector(70.f, 10.f, 80.f), Hex(230, 225, 210), true);
	Part(Root, EAntShape::Sphere, Outlet + FVector(0.f, -12.f, -10.f), FVector(160.f, 4.f, 120.f), Hex(40, 30, 25), false);   // burn mark

	auto Cable = [this](const FVector& A, const FVector& B, float Thick, const FLinearColor& C, bool bCollide)
	{
		const FVector D = B - A;
		UStaticMeshComponent* M = Part(Root, EAntShape::Cylinder, (A + B) * 0.5f, FVector(Thick, Thick, (float)D.Size()), C, bCollide);
		M->SetWorldRotation(FRotationMatrix::MakeFromZ(D).ToQuat());
		return M;
	};
	Cable(Outlet + FVector(0.f, -15.f, -40.f), Floor, 26.f, Hex(30, 30, 30), true);
	UStaticMeshComponent* FloorCable = Cable(Floor, End, 26.f, Hex(30, 30, 30), true);
	if (AntArt::Dress(this, Root, TEXT("SM_Wire_Exposed"), FVector(3150.f, 4790.f, 0.f), FVector(3620.f, 4975.f, 70.f), 0.f))
	{
		AntArt::HideAll({ FloorCable });
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Tip = End + FVector(60.f, FMath::Sin(i * 1.6f) * 40.f, 8.f + i * 6.f);
		Cable(End, Tip, 6.f, Copper, false);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		Flash = MakeLight(this, Root, End + FVector(40.f, 0.f, 60.f), 14.f, FLinearColor(0.6f, 0.8f, 1.f), 1100.f);
		Flash->SetVisibility(false);
		for (int32 i = 0; i < 6; ++i)
		{
			UStaticMeshComponent* S = Part(Root, EAntShape::Sphere, End, FVector(14.f), FLinearColor(1.f, 0.95f, 0.5f), false);
			S->SetVisibility(false);
			S->SetCastShadow(false);
			Sparks.Add(S);
		}
	}
	NextSpark = Now() + 4.f;
}

void AKitchenWire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashT > 0.f)
	{
		FlashT -= DeltaSeconds;
		const bool bOn = FlashT > 0.f && FMath::Fmod(FlashT, 0.08f) > 0.03f;
		if (Flash) { Flash->SetVisibility(bOn); }
		for (UStaticMeshComponent* S : Sparks)
		{
			S->SetVisibility(bOn);
			if (bOn)
			{
				S->SetWorldLocation(WireSpot::End + FVector(60.f, 0.f, 10.f) + FMath::VRand() * FMath::FRandRange(10.f, 90.f));
			}
		}
	}

	if (HasAuthority() && Now() >= NextSpark)
	{
		NextSpark = Now() + FMath::FRandRange(3.5f, 6.5f);
		Spark();
	}
}

void AKitchenWire::Spark()
{
	MulticastSpark();
	AntSfx::Everyone(this, AntSfx::Zap, WireSpot::End, 1.f);
	TArray<AAntCharacter*> Shocked;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead)
		{
			continue;
		}
		const FVector P = Ant->GetActorLocation();
		const float D = FVector::Dist(P, WireSpot::End);
		if (D < 280.f)
		{
			Shocked.Add(Ant);
			Ant->HurtBy(35.f, TEXT("получил разряд током"));
			Ant->ApplyLaunch((P - WireSpot::End).GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 350.f));
			Ant->Toast(TEXT("БЗЗЗТ! Оголённый провод!"));
		}
		else if (Puddle().IsInside(P))
		{
			Shocked.Add(Ant);
			Ant->HurtBy(40.f, TEXT("поджарился током в луже"));
			Ant->ApplyLaunch(FVector(0.f, 0.f, 300.f));
			Ant->Toast(TEXT("Лужа под током! Не стой в воде у провода"));
		}
	}
	// Electricity jumps between wet ants that touch each other (a chain!).
	for (int32 i = 0; i < Shocked.Num() && i < 24; ++i)
	{
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* O = *It;
			if (O && !O->bDead && !Shocked.Contains(O) && O->HasStatus(AntStatus::Wet)
				&& FVector::Dist(O->GetActorLocation(), Shocked[i]->GetActorLocation()) < 260.f)
			{
				Shocked.Add(O);
				O->HurtBy(30.f, TEXT("ударило током по цепочке мокрых муравьёв"));
				O->ApplyLaunch(FVector(0.f, 0.f, 280.f));
				O->Toast(TEXT("ЦЕПНАЯ МОЛНИЯ! Мокрые муравьи проводят ток"));
				AntSfx::Everyone(this, AntSfx::Zap, O->GetActorLocation(), 1.3f);
			}
		}
	}
	// Burning ants near the sparks... and sparks set dry ants on fire.
	for (AAntCharacter* A : Shocked)
	{
		if (A && !A->HasStatus(AntStatus::Wet) && FMath::FRand() < 0.5f)
		{
			A->Ignite(4.f);
		}
	}
}

void AKitchenWire::MulticastSpark_Implementation()
{
	FlashT = 0.4f;
}

void AKitchenWire::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	if (Puddle().IsInside(Ant->GetActorLocation()))
	{
		Env.Flags |= AntStatus::Wet;
	}
}

// =============================================================================================
// Mouse trap
// =============================================================================================

AKitchenTrap::AKitchenTrap()
{
	InteractRadius = 0.f;
}

void AKitchenTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenTrap, bArmed);
}

void AKitchenTrap::BeginPlay()
{
	Super::BeginPlay();
	UStaticMeshComponent* Base = BoxPart(Root, FVector(-190.f, -90.f, 0.f), FVector(190.f, 90.f, 16.f), LightWood, true);             // base
	UStaticMeshComponent* Bait = BoxPart(Root, FVector(40.f, -35.f, 16.f), FVector(110.f, 35.f, 22.f), Hex(200, 190, 60), false);      // bait plate
	if (AntArt::Dress(this, Root, TEXT("SM_MouseTrap"), FVector(-195.f, -95.f, 0.f), FVector(195.f, 95.f, 75.f), 0.f))
	{
		AntArt::HideAll({ Base, Bait });
	}
	Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 22.f), FVector(16.f, 150.f, 16.f), Steel, false, FRotator(0.f, 0.f, 90.f));   // spring axis
	Bar = AntKit::AddPivot(this, Root, FVector(0.f, 0.f, 22.f));
	BoxPart(Bar, FVector(-175.f, -88.f, 0.f), FVector(0.f, -78.f, 8.f), Steel, false);
	BoxPart(Bar, FVector(-175.f, 78.f, 0.f), FVector(0.f, 88.f, 8.f), Steel, false);
	BoxPart(Bar, FVector(-175.f, -88.f, 0.f), FVector(-165.f, 88.f, 8.f), Steel, false);
	BarAlpha = bArmed ? 0.f : 1.f;
}

void AKitchenTrap::OnRep_Armed()
{
}

void AKitchenTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	BarAlpha = FMath::FInterpConstantTo(BarAlpha, bArmed ? 0.f : 1.f, DeltaSeconds, bArmed ? 1.5f : 14.f);
	if (Bar)
	{
		Bar->SetRelativeRotation(FQuat(FVector::RightVector, PI * BarAlpha));
	}

	if (!HasAuthority())
	{
		return;
	}
	if (!bArmed)
	{
		if (Now() >= RearmAt)
		{
			bArmed = true;
		}
		return;
	}

	const FBox Trigger(FVector(-190.f, -95.f, -10.f), FVector(190.f, 95.f, 90.f));
	TArray<AAntCharacter*> Caught;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		if (*It && !It->bDead && InsideLocalBox(this, It->GetActorLocation(), Trigger))
		{
			Caught.Add(*It);
		}
	}
	if (Caught.Num() == 0)
	{
		return;
	}
	bArmed = false;
	RearmAt = Now() + 15.f;
	for (AAntCharacter* Ant : Caught)
	{
		Ant->HurtBy(80.f, TEXT("попал в мышеловку"));
		Ant->ApplyLaunch(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 900.f));
	}
	Broadcast(FString::Printf(TEXT("ЩЁЛК! %s попался в мышеловку"), *Caught[0]->GetDisplayName()), Warn);
}

// =============================================================================================
// Physics props
// =============================================================================================

AKitchenProp::AKitchenProp()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void AKitchenProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenProp, ShapeIndex);
	DOREPLIFETIME(AKitchenProp, Size);
	DOREPLIFETIME(AKitchenProp, Color);
	DOREPLIFETIME(AKitchenProp, Mass);
}

void AKitchenProp::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetStaticMesh(AntKit::ShapeMesh((EAntShape)ShapeIndex));
	Mesh->SetWorldScale3D(Size / 100.f);
	AntKit::Paint(Mesh, Color);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetMassOverrideInKg(NAME_None, Mass, true);
	Mesh->SetLinearDamping(0.6f);
	Mesh->SetAngularDamping(0.8f);
}
