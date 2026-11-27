#include "KitchenInteractables.h"
#include "Map4Look.h"
#include "AntAudio.h"
#include "AntFx.h"
#include "AntFluidWorld.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "KitchenLoot.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntArmy.h"
#include "AntRaid.h"
#include "HumanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "LevelV3.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "AntReact.h"
#include "AntLab.h"
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

void AKitchenInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenInteractable, PullMissing);
	DOREPLIFETIME_CONDITION(AKitchenInteractable, LabId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKitchenInteractable, HomeScale, COND_InitialOnly);
}

void AKitchenInteractable::BeginPlay()
{
	if (!HasAuthority())
	{
		// KAN-40: a guest builds the gadget from the flat's tables, at the flat's scale (see LabId).
		const FAntLabVariant* Lab = AntLab::Get();
		if (LabId != 0 && (!Lab || Lab->Id.Len() == 0 || Lab->Id[0] != (TCHAR)LabId)) { AntLab::Activate((TCHAR)LabId); }
		if (!HomeScale.Equals(FVector::OneVector, 1.e-4)) { SetActorScale3D(HomeScale); }
	}
	Super::BeginPlay();
}

namespace
{
	/** KAN-40: the art group (KitchenArt::Home) a door of the kitchen belongs to. */
	const TCHAR* DoorGroup(EKitchenDoorKind Kind)
	{
		switch (Kind)
		{
		case EKitchenDoorKind::Fridge: return TEXT("fridge");
		case EKitchenDoorKind::Oven: return TEXT("range");
		case EKitchenDoorKind::Drawer: return TEXT("drawers");
		case EKitchenDoorKind::Dishwasher: return TEXT("dishwasher");
		default: return TEXT("microwave");
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Heavy doors: everyone holding [E] adds its strength
// ---------------------------------------------------------------------------------------------

bool AKitchenInteractable::IsHeldOpen() const
{
	return GetWorld() && HeldOpenUntil > GetWorld()->GetTimeSeconds();
}

float AKitchenInteractable::PullNeed() const
{
	int32 Class = DoorClass();
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	if (!GS || !GS->bHeistMode)
	{
		return AntDoor::Need(AntDoor::Light);   // heavy doors belong to the heist run; war and the classic raid keep a plain [E]
	}
	if (const AAntRaid* Raid = AAntRaid::Get(GetWorld()))
	{
		// A raid on a harder day: every real door (not the light ones) is a class heavier.
		if (Class > AntDoor::Light && Raid->InRaid()) { Class = FMath::Min<int32>(AntDoor::Heavy, Class + Raid->TierDef().DoorBias); }
	}
	if (Class > AntDoor::Light && GS->Cards.Contains(AntCard::Lever))
	{
		--Class;
	}
	return AntDoor::Need(Class);
}

float AKitchenInteractable::PullStrength(int32* OutHands, AAntCharacter** OutFirst) const
{
	float Have = 0.f;
	int32 Hands = 0;
	AAntCharacter* First = nullptr;
	const float Reach = InteractRadius * 1.5f;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (A && !A->bDead && A->Pulling == this && DistanceTo(A->GetActorLocation()) < Reach)
		{
			Have += A->GetStrength();
			++Hands;
			if (!First) { First = A; }
			// Its army leans on the door with it.
			if (const AAntArmy* Army = AAntArmy::FindFor(A))
			{
				if (Army->bSquad)
				{
					if (Army->HelpsCommander(A)) { Have += Army->Strength(); }   // a raid: the squad at its commander's side pulls with him
				}
				else if (FVector::Dist(Army->GetActorLocation(), A->GetActorLocation()) < 900.f)
				{
					Have += FMath::Min(30.f, Army->Count * 1.5f);
				}
			}
		}
	}
	if (First)
	{
		// Bots of the same colony standing at the door with nothing in their jaws help without being asked.
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			if (A && !A->bDead && A->Pulling != this && A->Creature == 0 && A->Team == First->Team && !A->Carrying && !A->IsPlayerControlled()
				&& DistanceTo(A->GetActorLocation()) < Reach && CanPull(A, false))
			{
				Have += A->GetStrength();
			}
		}
	}
	if (OutHands) { *OutHands = Hands; }
	if (OutFirst) { *OutFirst = First; }
	return Have;
}

bool AKitchenInteractable::TryOpenByPull()
{
	int32 Hands = 0;
	AAntCharacter* First = nullptr;
	const float Have = PullStrength(&Hands, &First);
	const float Need = PullNeed();
	if (Hands == 0)
	{
		PullMissing = 0;
		return false;
	}
	if (Have < Need)
	{
		PullMissing = (uint8)AntDoor::Missing(Need, Have);
		return false;
	}
	PullMissing = 0;
	const int32 Class = DoorClass();
	SetDoorOpen(true);
	UE_LOG(LogTemp, Display, TEXT("ANTDOOR %s opened by %s: strength %.0f of %.0f, hands %d, class %d"), *GetName(), First ? *First->GetDisplayName() : TEXT("-"), Have, Need, Hands, Class);
	AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation(), Class >= AntDoor::Heavy ? 0.85f : 1.3f);
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		if (It->Pulling == this)
		{
			It->Pulling = nullptr;
		}
	}
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->OnDoorOpened(this, First, Class);
	}
	return true;
}

bool AKitchenInteractable::Pull(AAntCharacter* Ant)
{
	if (!HasAuthority() || !Ant || Ant->bDead || IsDoorOpen())
	{
		return false;
	}
	if (!CanPull(Ant, true))
	{
		return false;
	}
	Ant->Pulling = this;
	PullClock = 0.f;
	const bool bOpened = TryOpenByPull();
	if (!bOpened)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTDOOR %s strains: %s pulls, missing %d"), *GetName(), *Ant->GetDisplayName(), (int32)PullMissing);
	}
	return bOpened;
}

void AKitchenInteractable::PullTick(float Dt)
{
	if (!HasAuthority() || DoorClass() == 0)
	{
		return;
	}
	PullClock += Dt;
	if (PullClock < 0.25f)
	{
		return;
	}
	PullClock = 0.f;
	if (IsDoorOpen())
	{
		PullMissing = 0;
		return;
	}
	TryOpenByPull();
}

float AKitchenInteractable::StrainNow(float Dt)
{
	if (PullMissing == 0 || IsDoorOpen())
	{
		StrainT = 0.f;
		return 0.f;
	}
	StrainT += Dt;
	if (GetNetMode() != NM_DedicatedServer && StrainT >= StrainSfxAt)
	{
		StrainSfxAt = StrainT + FMath::FRandRange(0.7f, 1.1f);
		AntAudio::Play(this, FMath::RandBool() ? EAntSfx::HingeCreak : EAntSfx::CarryStrain, GetActorLocation(), 0.7f);   // a creak, the ants straining
	}
	// It gives a crack, shudders and falls back.
	const float Heave = FMath::Max(0.f, FMath::Sin(StrainT * 5.5f));
	return 0.012f + 0.05f * Heave * Heave + 0.006f * FMath::Sin(StrainT * 41.f);
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
	if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AntArtOpen")))   // dev: every door starts open and stays open (to look inside)
	{
		bOpen = true;
	}
	Build();
	Alpha = bOpen ? 1.f : 0.f;
	ApplyAlpha();
}

void AKitchenDoor::Build()
{
	Hinge = AntKit::AddPivot(this, Root, FVector::ZeroVector);
	// The modelled kitchen has a real door for every kind (hinge and motion as below); the boxes are the old kitchen.
	const bool bArt = KitchenArt::Enabled();
	auto ArtDoor = [this, bArt](const TCHAR* Name) { return bArt && KitchenArt::AddAt(this, Hinge, Name, 0, KitchenArt::ESolid::Moving, false, TEXT("ArtDoor")) != nullptr; };

	switch (Kind)
	{
	case EKitchenDoorKind::Fridge:
	{
		// Hinge = east-front-bottom corner of the fridge; the door extends west (-X), outside is -Y.
		if (!ArtDoor(TEXT("SM_K_Fridge_DoorMain")))   // the model has the handle, the magnets, the bins and the bottles
		{
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
		}
		UsePointsLocal = { FVector(-1850.f, -150.f, -90.f), FVector(-1500.f, 300.f, 110.f), FVector(-1000.f, 300.f, 110.f) };
		InnerLight = MakeLight(this, Root, FVector(-1000.f, 800.f, 2000.f), 6.f, FLinearColor(0.85f, 0.95f, 1.f), 2600.f);
		break;
	}
	case EKitchenDoorKind::Oven:
	{
		// Hinge = bottom-front edge; the door folds down into a ramp.
		if (!ArtDoor(TEXT("SM_K_Range_OvenDoor")))
		{
			UStaticMeshComponent* Slab = BoxPart(Hinge, FVector(-700.f, -14.f, 0.f), FVector(700.f, 0.f, 1200.f), Hex(35, 35, 38), true);
			UStaticMeshComponent* Win = BoxPart(Hinge, FVector(-450.f, -17.f, 420.f), FVector(450.f, -14.f, 980.f), Hex(45, 30, 20), false);    // window
			UStaticMeshComponent* Handle = BoxPart(Hinge, FVector(-550.f, -70.f, 1080.f), FVector(550.f, -40.f, 1130.f), Steel, true);           // handle
			UStaticMeshComponent* H1 = BoxPart(Hinge, FVector(-540.f, -40.f, 1090.f), FVector(-500.f, -14.f, 1120.f), Steel, false);
			UStaticMeshComponent* H2 = BoxPart(Hinge, FVector(500.f, -40.f, 1090.f), FVector(540.f, -14.f, 1120.f), Steel, false);
			if (AntArt::DressAuto(this, Hinge, TEXT("SM_Oven_Door"), FVector(-700.f, -75.f, 0.f), FVector(700.f, 0.f, 1200.f), 0.f, 3.f))
			{
				AntArt::HideAll({ Slab, Win, Handle, H1, H2 });
			}
		}
		UsePointsLocal = { FVector(0.f, -150.f, -274.f), FVector(0.f, 500.f, 26.f), FVector(0.f, -700.f, 26.f) };
		InnerLight = MakeLight(this, Root, FVector(0.f, 900.f, 1000.f), 3.f, FLinearColor(1.f, 0.45f, 0.1f), 1500.f);
		break;
	}
	case EKitchenDoorKind::Drawer:
	{
		if (!ArtDoor(TEXT("SM_K_BaseDrawers_Drawer1")))   // the model is the whole drawer: front, handle, tray
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
		}
		UsePointsLocal = { FVector(0.f, -30.f, 120.f), FVector(0.f, -30.f, -60.f) };
		break;
	}
	case EKitchenDoorKind::Dishwasher:
	{
		// Hinge = bottom-front edge of the dishwasher opening; the door drops into a ramp (grooved inside: easy to climb).
		if (!ArtDoor(TEXT("SM_K_Dishwasher_Door")))   // ribbed on the inside like the old one: an easy ramp
		{
			BoxPart(Hinge, FVector(-640.f, -16.f, 0.f), FVector(640.f, 0.f, 1480.f), Hex(200, 205, 210), true);
			BoxPart(Hinge, FVector(-620.f, -60.f, 1380.f), FVector(620.f, -16.f, 1440.f), Steel, true);            // handle
			BoxPart(Hinge, FVector(-400.f, -19.f, 1300.f), FVector(-200.f, -16.f, 1340.f), Hex(60, 200, 90), false); // ready light
			for (int32 i = 0; i < 6; ++i)                                                                          // grooves on the inside face
			{
				BoxPart(Hinge, FVector(-600.f, 0.f, 150.f + i * 220.f), FVector(600.f, 12.f, 170.f + i * 220.f), Hex(170, 175, 180), true);
			}
		}
		UsePointsLocal = { FVector(0.f, -150.f, -100.f), FVector(0.f, 600.f, 40.f), FVector(0.f, -900.f, 20.f) };
		InnerLight = MakeLight(this, Root, FVector(0.f, 900.f, 1100.f), 2.5f, FLinearColor(0.8f, 0.95f, 1.f), 1700.f);
		break;
	}
	case EKitchenDoorKind::Microwave:
	{
		// Hinge = the left (west) front edge; the door swings out towards -Y.
		if (!ArtDoor(TEXT("SM_K_Microwave_Door")))
		{
			UStaticMeshComponent* Slab = BoxPart(Hinge, FVector(0.f, -30.f, 0.f), FVector(850.f, 0.f, 560.f), Hex(60, 60, 64), true);
			BoxPart(Hinge, FVector(80.f, -33.f, 80.f), FVector(680.f, -30.f, 480.f), Hex(30, 25, 25), false);          // the dark window
			BoxPart(Hinge, FVector(760.f, -70.f, 120.f), FVector(800.f, -30.f, 440.f), Steel, true);                   // handle
			(void)Slab;
		}
		UsePointsLocal = { FVector(760.f, -120.f, -40.f), FVector(400.f, 300.f, 40.f) };
		InnerLight = MakeLight(this, Root, FVector(450.f, 450.f, 450.f), 2.f, FLinearColor(1.f, 0.9f, 0.6f), 1200.f);
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
	case EKitchenDoorKind::Dishwasher:
		Hinge->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(85.f * Alpha)));
		break;
	case EKitchenDoorKind::Microwave:
		Hinge->SetRelativeRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(-115.f * Alpha)));
		break;
	}
	if (InnerLight && (Kind == EKitchenDoorKind::Fridge || Kind == EKitchenDoorKind::Dishwasher || Kind == EKitchenDoorKind::Microwave))
	{
		InnerLight->SetVisibility(Alpha > 0.05f);
		if (Kind == EKitchenDoorKind::Fridge && LevelV3::Enabled())
		{
			// Level v3: at night the open fridge is the brightest thing in the kitchen - a cold wedge of light across the floor.
			int32 Pinned = -1;
			FParse::Value(FCommandLine::Get(), TEXT("AntTime="), Pinned);   // (-AntTime pins the light: the fridge follows it)
			const bool bNight = Pinned >= 0 ? Pinned == 2 : LevelV3::TimeOfDay() == LevelV3::Night;
			InnerLight->SetIntensity(bNight ? 22.f : 6.f);
			InnerLight->SetAttenuationRadius(bNight ? 5200.f : 2600.f);
		}
	}
}

void AKitchenDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PullTick(DeltaSeconds);
	const float Strain = StrainNow(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	if (!bOpen && Strain > 0.f)
	{
		// Not enough ants on it: it gives a crack and falls back.
		Alpha = Strain;
		ApplyAlpha();
	}
	else if (!FMath::IsNearlyEqual(Alpha, Target))
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
	if (!bInOpen && bOpen && IsHeldOpen())
	{
		return;   // a squad holds it open
	}
	const bool bWasOpen = bOpen;
	bOpen = bInOpen;
	if (bOpen && !bWasOpen)
	{
		OpenedAt = Now();
	}
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
			const KitchenArt::FHome H = KitchenArt::Home(DoorGroup(Kind));   // (KAN-40: the numbers are the kitchen's)
			const FVector Base = H.ToKitchen(GetActorLocation()) + FVector(0.f, -1100.f, 55.f);
			// Level v2, the gradient rule: the drawer is 18 s from the nest, so the stash is tiers 1, 2, 1 (it was 2, 3, 2).
			const int32 Down = KitchenArt::Enabled() ? 1 : 0;
			GM->SpawnFood(H.At(Base + FVector(-320.f, 500.f, 0.f)), 2 - Down);
			GM->SpawnFood(H.At(Base + FVector(250.f, 900.f, 0.f)), 3 - Down);
			GM->SpawnFood(H.At(Base + FVector(-60.f, 1350.f, 0.f)), 2 - Down);
			FRandomStream R(FMath::Rand());
			GM->SpawnGear(H.At(Base + FVector(380.f, 1250.f, 0.f)), AntKit::RollWeapon(R.RandRange(1, 3), R));
		}
	}
}

int32 AKitchenDoor::DoorClass() const
{
	switch (Kind)
	{
	case EKitchenDoorKind::Drawer: return AntDoor::Light;
	case EKitchenDoorKind::Microwave: return AntDoor::Medium;
	default: return AntDoor::Heavy;   // fridge, oven, dishwasher
	}
}

FString AKitchenDoor::DoorNoun() const
{
	switch (Kind)
	{
	case EKitchenDoorKind::Fridge: return TEXT("холодильник");
	case EKitchenDoorKind::Oven: return TEXT("духовку");
	case EKitchenDoorKind::Drawer: return TEXT("ящик");
	case EKitchenDoorKind::Dishwasher: return TEXT("посудомойку");
	default: return TEXT("микроволновку");
	}
}

FString AKitchenDoor::GetPrompt(const AAntCharacter* Ant) const
{
	switch (Kind)
	{
	case EKitchenDoorKind::Fridge: return bOpen ? TEXT("Захлопнуть холодильник") : TEXT("Подцепить дверь холодильника");
	case EKitchenDoorKind::Oven: return bOpen ? TEXT("Закрыть духовку") : TEXT("Открыть духовку (там печенье... и жар)");
	case EKitchenDoorKind::Drawer: return bOpen ? TEXT("Задвинуть ящик") : TEXT("Выдвинуть ящик");
	case EKitchenDoorKind::Dishwasher: return bOpen ? TEXT("Закрыть посудомойку") : TEXT("Открыть посудомойку (грязные тарелки!)");
	case EKitchenDoorKind::Microwave: return bOpen ? TEXT("Закрыть микроволновку") : TEXT("Открыть микроволновку");
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
	case EKitchenDoorKind::Dishwasher:
		SetOpen(!bOpen, 25.f);
		if (bOpen && Ant) { Ant->Toast(TEXT("Посудомойка открыта на 25 с: внутри мокро, но тарелки грязные — объедение")); }
		break;
	case EKitchenDoorKind::Microwave:
		SetOpen(!bOpen, 20.f);
		if (bOpen && Ant) { Ant->Toast(TEXT("Микроволновка открыта на 20 с")); }
		break;
	}
}

void AKitchenDoor::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = KitchenArt::Home(DoorGroup(Kind)).ToKitchen(Ant->GetActorLocation());   // (KAN-40: the boxes below are the kitchen's)
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
	else if (Kind == EKitchenDoorKind::Dishwasher)
	{
		// Rinse water on the floor of the machine, warm and humid inside when shut.
		if (FBox(FVector(60.f, 3040.f, 160.f), FVector(1240.f, 4700.f, 230.f)).IsInside(P))
		{
			Env.Flags |= AntStatus::Wet;
			Env.Temp(34.f, 6.f);
		}
		else if (!bOpen && FBox(FVector(60.f, 3040.f, 230.f), FVector(1240.f, 4700.f, 1600.f)).IsInside(P))
		{
			Env.Flags |= AntStatus::Wet;
			Env.Temp(44.f, 5.f);
		}
	}
}

// =============================================================================================
// Faucet
// =============================================================================================

bool AKitchenFaucet::RealWater()
{
	return UAntFluidWorld::Wanted();
}

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
	// Dev/test: -AntGadgetsOn starts with the hot tap running and the burner on (to look at the water and the steam).
	if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AntGadgetsOn")))
	{
		bOn = true;
		bHot = true;
	}
	ShownLevel = WaterLevel;
	if (KitchenArt::Enabled())
	{
		// The modelled taps (hot, cold): each turns a quarter while its water runs. Ants stand on them to use them.
		for (const TCHAR* Name : { TEXT("SM_K_Faucet_TapHot"), TEXT("SM_K_Faucet_TapCold") })
		{
			UStaticMeshComponent* T = KitchenArt::AddAt(this, Root, Name, 0, KitchenArt::ESolid::Moving);
			Taps.Add(T);
			TapBase.Add(T ? T->GetRelativeRotation().Quaternion() : FQuat::Identity);
		}
	}
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
	if (HasAuthority() && bOn && bNoisy && Now() >= NextNoise)
	{
		NextNoise = Now() + 3.f;
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->MakeNoise(Spout(), 0.65f, AntNoise::Faucet, this);
		}
	}

	if (HasAuthority())
	{
		// With the real water the sink fills by itself (AntFluidWorld); the stand-in level stays at the bottom.
		WaterLevel = RealWater() ? BasinFloor : FMath::Clamp(WaterLevel + (bOn ? 32.f : -14.f) * DeltaSeconds, BasinFloor, BasinTop);
	}
	ShownLevel = FMath::FInterpTo(ShownLevel, WaterLevel, DeltaSeconds, 4.f);
	for (int32 i = 0; i < Taps.Num() && i < 2; ++i)
	{
		const float Want = (bOn && bHot == (i == 0)) ? 1.f : 0.f;
		if (Taps[i] && !FMath::IsNearlyEqual(TapTurn[i], Want))
		{
			TapTurn[i] = FMath::FInterpConstantTo(TapTurn[i], Want, DeltaSeconds, 3.f);
			Taps[i]->SetRelativeRotation(TapBase[i] * FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.f * TapTurn[i])));
		}
	}

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
		StreamMesh->SetVisibility(bOn && !RealWater());
		if (bOn && !RealWater())
		{
			const FVector S = Spout();
			const float Bottom = FMath::Max(BasinFloor, ShownLevel);
			const float Len = FMath::Max(10.f, (float)S.Z - Bottom);
			const float Wobble = 1.f + 0.08f * FMath::Sin(Now() * 30.f);
			StreamMesh->SetRelativeLocation(FVector(S.X, S.Y, Bottom + Len * 0.5f));
			StreamMesh->SetRelativeScale3D(FVector(0.6f * Wobble, 0.6f * Wobble, Len / 100.f));

			// Where the stream lands: droplets jump, rings spread over the water, hot water steams.
			const FVector Land = GetActorTransform().TransformPosition(FVector(S.X, S.Y, Bottom + 2.f));
			SplashAccum += DeltaSeconds;
			RippleAccum += DeltaSeconds;
			if (SplashAccum >= 0.06f)
			{
				SplashAccum = 0.f;
				AntFx::Droplets(this, Land, 2, 14.f);
				if (bHot && FMath::FRand() < 0.3f)
				{
					AntFx::Steam(this, Land + FVector(0.f, 0.f, 20.f), 140.f, 160.f);
				}
			}
			if (RippleAccum >= 0.4f)
			{
				RippleAccum = 0.f;
				AntFx::Ripple(this, Land, ShownLevel > BasinFloor + 4.f ? 420.f : 150.f);
			}
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
		if (RealWater() && !bOn)
		{
			// The valve is stiff for an 8 mm ant: two of them have to lean on the lever together (bots of the squad count).
			const FVector V = bHotValve ? HotValve() : ColdValve();
			int32 Near = 0;
			for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
			{
				if (!It->bDead && FVector::Dist(It->GetActorLocation(), V) < 330.f) { ++Near; }
			}
			if (Near < 2)
			{
				Ant->Toast(TEXT("Вентиль тугой: нужен ещё один муравей рядом — вдвоём на рычаг!"));
				return;
			}
		}
		SetWater(true, bHotValve);
		bNoisy = true;   // an ant left the tap running: the Human hears it and comes to turn it off
		NextNoise = 0.f;
		Ant->Toast(bHotValve ? TEXT("Горячая вода! Кипяток в раковине. Человек услышит") : TEXT("Холодная вода пошла. Раковина наполняется, Человек услышит"));
	}
}

void AKitchenFaucet::SetWater(bool bInOn, bool bHotWater)
{
	bOn = bInOn;
	bHot = bHotWater;
	if (!bOn)
	{
		bNoisy = false;
	}
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
	const bool bArt = KitchenArt::Enabled();   // the plate is part of the modelled kitchen (SM_K_Switch): only the rocker is ours
	if (!bArt)
	{
		UStaticMeshComponent* Plate = BoxPart(Root, FVector(0.f, -70.f, -100.f), FVector(10.f, 70.f, 100.f), Hex(235, 230, 215), true);
		UStaticMeshComponent* Frame = BoxPart(Root, FVector(8.f, -25.f, -45.f), FVector(14.f, 25.f, 45.f), Hex(190, 185, 170), false);
		if (AntArt::Dress(this, Root, TEXT("SM_LightSwitch"), FVector(0.f, -80.f, -110.f), FVector(18.f, 80.f, 110.f), -90.f))
		{
			AntArt::HideAll({ Plate, Frame });
		}
	}
	Toggle = AntKit::AddPivot(this, Root, FVector(12.f, 0.f, 0.f));
	if (!(bArt && KitchenArt::AddAt(this, Toggle, TEXT("SM_K_Switch_Toggle"), 0, KitchenArt::ESolid::Moving)))
	{
		BoxPart(Toggle, FVector(0.f, -18.f, -12.f), FVector(34.f, 18.f, 12.f), Hex(250, 248, 240), true);
	}
	OnRep_Lights();
}

FString AKitchenSwitch::GetPrompt(const AAntCharacter* Ant) const
{
	return bLightsOn ? TEXT("Выключить свет (Человек ослепнет)") : TEXT("Включить свет");
}

void AKitchenSwitch::Interact(AAntCharacter* Ant)
{
	if (const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>(); GS && GS->EventId == AntEvent::Blackout)
	{
		if (Ant) { Ant->Toast(TEXT("Пробки выбило: выключатель не работает")); }
		return;
	}
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
	if (!AntReact::HasPower(World))
	{
		return false;   // the breaker tripped (AntReact.h): every lamp is out whatever the switch says
	}
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
		if (!Map4Look::Swap(Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 1.f), FVector(150.f, 150.f, 4.f), Steel, false), TEXT("SM_M4_Drain")))
		{
			Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 3.f), FVector(100.f, 100.f, 2.f), Black, false);
		}
		break;
	case EPassageLook::Hole:
		BoxPart(Root, FVector(0.f, -80.f, 0.f), FVector(4.f, 80.f, 120.f), Black, false)->ComponentTags.AddUnique(TEXT("M4Arch"));   // the dark of the hole
		Part(Root, EAntShape::Sphere, FVector(2.f, 0.f, 120.f), FVector(4.f, 160.f, 80.f), Black, false)->ComponentTags.AddUnique(TEXT("M4Arch"));
		break;
	case EPassageLook::Pipe:
		if (!Map4Look::Swap(Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 60.f), FVector(110.f, 110.f, 120.f), Hex(235, 235, 230), true), TEXT("SM_M4_PipeEnd")))
		{
			Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 121.f), FVector(80.f, 80.f, 2.f), Black, false);
		}
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
	const bool bArt = KitchenArt::Enabled();   // the toaster itself (real slots, lever slot) is part of the modelled kitchen
	if (!bArt)
	{
		BoxPart(Root, FVector(-5902.f, 4185.f, 1780.f), FVector(-5898.f, 4215.f, 2090.f), Black, false);   // lever slot
	}
	Lever = AntKit::AddPivot(this, Root, FVector(-5900.f, 4200.f, 2060.f));
	if (!(bArt && KitchenArt::AddAt(this, Lever, TEXT("SM_K_Toaster_Lever"), 0, KitchenArt::ESolid::Moving)))
	{
		UStaticMeshComponent* Knob = BoxPart(Lever, FVector(0.f, -45.f, -22.f), FVector(55.f, 45.f, 22.f), Black, true);
		if (AntArt::DressAuto(this, Lever, TEXT("SM_Toaster_Lever"), FVector(0.f, -55.f, -30.f), FVector(80.f, 55.f, 30.f)))
		{
			AntArt::HideAll({ Knob });
		}
	}
	int32 Slice = 0;
	for (float Y : { 4085.f, 4315.f })
	{
		// Modelled slices stand in the real slots (origin at their bottom, z 1950); ants ride them when they pop.
		UStaticMeshComponent* T = bArt ? KitchenArt::AddAt(this, Root, TEXT("SM_K_Toast"), Slice, KitchenArt::ESolid::Moving) : nullptr;
		bToastArt = bToastArt || T != nullptr;
		Toasts.Add(T ? T : BoxPart(Root, FVector(-6420.f, Y - 25.f, 1950.f), FVector(-6080.f, Y + 25.f, 2140.f), Hex(215, 150, 70), false));
		++Slice;
	}
}

FBox AKitchenToaster::Slots()
{
	// The modelled toaster has real slots 350 deep: whoever sits down in one is launched too.
	return KitchenArt::Home(TEXT("toaster")).Box(FBox(FVector(-6500.f, 4000.f, KitchenArt::Enabled() ? 1780.f : 2100.f), FVector(-6000.f, 4400.f, 2400.f)));
}

bool AKitchenToaster::CanInteract(const AAntCharacter* Ant) const
{
	return !bArmed && (!HasAuthority() || Now() >= ReadyAt);
}

void AKitchenToaster::ForceArm()
{
	bArmed = true;
	PopAt = Now() + 1.8f;
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
	if (!AntReact::HasPower(this))
	{
		Ant->Toast(TEXT("Тостер без света не работает: выбило пробки"));
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
		const FVector Want(-6250.f, i == 0 ? 4085.f : 4315.f, (bToastArt ? 1950.f : 2045.f) + Up);
		if (!Toasts[i]->GetRelativeLocation().Equals(Want, 0.05f))
		{
			Toasts[i]->SetRelativeLocation(Want);
		}
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
	AntReact::OnToasterPop(this);   // C13: wound right down by three ants, the toast flies across the kitchen (AntReact.h)
	const KitchenArt::FHome Home = KitchenArt::Home(TEXT("toaster"));   // (KAN-40: the numbers below are the kitchen's)
	AntSfx::Everyone(this, AntSfx::Toaster, Home.At(FVector(-6000.f, 4200.f, 1900.f)), 1.f);
	if (AAntKitchenGameMode* NoiseGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		NoiseGM->MakeNoise(Home.At(FVector(-6000.f, 4200.f, 1900.f)), 0.9f, AntNoise::Toaster, this);
	}

	int32 Launched = 0;
	FString Who;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Slots().IsInside(Ant->GetActorLocation()))
		{
			Ant->ApplyLaunch(Home.Dir(FVector(FMath::FRandRange(-120.f, 120.f), 320.f, 2750.f)));
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
		// Level v2: the toaster is not an endless bakery. Toast drops only while fewer than two pieces of food lie on the worktop
		// round it, and after it has dropped some it bakes nothing for 40 s (it still launches ants and still makes its noise).
		bool bBake = true;
		if (KitchenArt::Enabled())
		{
			int32 OnTop = 0;
			const FBox Worktop(FVector(-7000.f, 2950.f, 1690.f), FVector(-4150.f, 5000.f, 2100.f));
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				if (!It->bIsGear && !It->IsCarried() && Worktop.IsInside(Home.ToKitchen(It->GetActorLocation()))) { ++OnTop; }
			}
			bBake = OnTop < 2 && Now() >= NextFoodAt;
			UE_LOG(LogTemp, Display, TEXT("ANTV2 toaster: %d on the worktop, %s"), OnTop, bBake ? TEXT("two pieces of toast pop out") : (Now() < NextFoodAt ? TEXT("no toast: still cooling (40 s)") : TEXT("no toast: two lie there already")));
		}
		if (bBake)
		{
			NextFoodAt = Now() + 40.f;
			GM->SpawnFood(Home.At(FVector(-5700.f, 3820.f, 1745.f)), 0);
			GM->SpawnFood(Home.At(FVector(-5740.f, 4560.f, 1745.f)), 1);
		}
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

	Map4Look::Swap(BoxPart(Root, Outlet + FVector(-70.f, -12.f, -80.f), Outlet + FVector(70.f, 10.f, 80.f), Hex(230, 225, 210), true), TEXT("SM_M4_Socket"));
	Map4Look::Swap(Part(Root, EAntShape::Sphere, Outlet + FVector(0.f, -12.f, -10.f), FVector(160.f, 4.f, 120.f), Hex(40, 30, 25), false), TEXT("SM_M4_Pheromone"), TEXT("Burn"), TEXT("M4Fx"));   // burn mark

	auto Cable = [this](const FVector& A, const FVector& B, float Thick, const FLinearColor& C, bool bCollide)
	{
		const FVector D = B - A;
		UStaticMeshComponent* M = Part(Root, EAntShape::Cylinder, (A + B) * 0.5f, FVector(Thick, Thick, (float)D.Size()), C, bCollide);
		Map4Look::Swap(M, TEXT("SM_M4_Thread"), Thick > 10.f ? TEXT("Rubber") : TEXT("Brass"), TEXT("M4Thread"));   // map v4: a cable
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
			Map4Look::Swap(S, TEXT("SM_M4_Droplet"), TEXT("Spark"), TEXT("M4Fx"));
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
				S->SetWorldLocation(GetActorTransform().TransformPosition(WireSpot::End + FVector(60.f, 0.f, 10.f)) + FMath::VRand() * FMath::FRandRange(10.f, 90.f));
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
	const FVector WireEnd = GetActorTransform().TransformPosition(WireSpot::End);   // (KAN-40: the wire stands where its group does)
	AntSfx::Everyone(this, AntSfx::Zap, WireEnd, 1.f);
	TArray<AAntCharacter*> Shocked;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead)
		{
			continue;
		}
		const FVector P = Ant->GetActorLocation();
		const float D = FVector::Dist(P, WireEnd);
		if (D < 280.f)
		{
			Shocked.Add(Ant);
			Ant->HurtBy(35.f, TEXT("получил разряд током"));
			Ant->ApplyLaunch((P - WireEnd).GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 350.f));
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
	const bool bArt = KitchenArt::Enabled();   // the base with the bait and the spring is part of the modelled kitchen (SM_K_MouseTrap)
	if (!bArt)
	{
		UStaticMeshComponent* Base = BoxPart(Root, FVector(-190.f, -90.f, 0.f), FVector(190.f, 90.f, 16.f), LightWood, true);             // base
		UStaticMeshComponent* Bait = BoxPart(Root, FVector(40.f, -35.f, 16.f), FVector(110.f, 35.f, 22.f), Hex(200, 190, 60), false);      // bait plate
		if (AntArt::Dress(this, Root, TEXT("SM_MouseTrap"), FVector(-195.f, -95.f, 0.f), FVector(195.f, 95.f, 75.f), 0.f))
		{
			AntArt::HideAll({ Base, Bait });
		}
		Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 22.f), FVector(16.f, 150.f, 16.f), Steel, false, FRotator(0.f, 0.f, 90.f));   // spring axis
	}
	Bar = AntKit::AddPivot(this, Root, FVector(0.f, 0.f, 22.f));
	if (!(bArt && KitchenArt::AddAt(this, Bar, TEXT("SM_K_MouseTrap_Bar"), 0, KitchenArt::ESolid::None)))
	{
		BoxPart(Bar, FVector(-175.f, -88.f, 0.f), FVector(0.f, -78.f, 8.f), Steel, false);
		BoxPart(Bar, FVector(-175.f, 78.f, 0.f), FVector(0.f, 88.f, 8.f), Steel, false);
		BoxPart(Bar, FVector(-175.f, -88.f, 0.f), FVector(-165.f, 88.f, 8.f), Steel, false);
	}
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
	if (AAntKitchenGameMode* NoiseGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		NoiseGM->MakeNoise(GetActorLocation(), 0.8f, AntNoise::Trap, this, Caught[0]);
	}
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
	PrimaryActorTick.bCanEverTick = true;
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
	DOREPLIFETIME(AKitchenProp, PropName);
}

void AKitchenProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !Mesh->IsSimulatingPhysics())
	{
		return;
	}
	LastVelocity = Mesh->GetPhysicsLinearVelocity();
	if (Mass < 3.f || LastVelocity.SizeSquared() > 90.f * 90.f)
	{
		return;   // light junk is shoved by the ants' bodies alone; this one is already moving
	}
	// Heavy things (a mug, a jar, a pot lid): an ant leaning on one pushes it with its strength - off the edge, if it likes.
	const FVector C = Mesh->Bounds.Origin;
	const float R = Mesh->Bounds.SphereRadius;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Creature != 0 || A->Carrying)
		{
			continue;
		}
		const FVector To = C - A->GetActorLocation();
		if (To.SizeSquared() > FMath::Square(R + 16.f))
		{
			continue;
		}
		const FVector Want = A->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
		if (Want.IsNearlyZero() || FVector::DotProduct(Want, To.GetSafeNormal2D()) < 0.45f)
		{
			continue;
		}
		Mesh->WakeAllRigidBodies();
		Mesh->AddForce(Want * A->GetStrength() * 900.f);
		LastPusher = A;
	}
}

bool AKitchenProp::TryShove(AAntCharacter* By, const FVector& From, const FVector& Dir, float Reach)
{
	if (!HasAuthority() || !By || !Mesh->IsSimulatingPhysics())
	{
		return false;
	}
	FVector Closest;
	if (Mesh->GetClosestPointOnCollision(From, Closest) < 0.f)
	{
		Closest = Mesh->Bounds.Origin;
	}
	const FVector ToCentre = (Mesh->Bounds.Origin - From).GetSafeNormal2D();
	if (FVector::Dist(Closest, From) > Reach || FVector::DotProduct(ToCentre, Dir.GetSafeNormal2D()) < 0.3f)
	{
		return false;
	}
	// A shove, not a launch: a worker slides a mug a couple of body lengths, a soldier further; heavier things go less far.
	const float Speed = By->GetStrength() * 38.f * FMath::Clamp(5.f / FMath::Max(1.f, Mass), 0.5f, 2.f);
	Mesh->WakeAllRigidBodies();
	Mesh->AddImpulse(Dir.GetSafeNormal2D() * Speed + FVector(0.f, 0.f, 30.f), NAME_None, true);
	LastPusher = By;
	AntSfx::Everyone(this, AntSfx::Hit, Closest, 0.7f);
	UE_LOG(LogTemp, Display, TEXT("ANTRAM %s shoved %s (%.0f uu/s)"), *By->GetDisplayName(), PropName.IsEmpty() ? TEXT("a prop") : *PropName, Speed);
	return true;
}

void AKitchenProp::Jolt(const FVector& Velocity, AActor* By)
{
	if (!HasAuthority() || !Mesh->IsSimulatingPhysics())
	{
		return;
	}
	Mesh->WakeAllRigidBodies();
	Mesh->AddImpulse(Velocity, NAME_None, true);
	Mesh->AddAngularImpulseInDegrees(FMath::VRand() * 120.f, NAME_None, true);
}

void AKitchenProp::OnPropHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || Mass < 3.f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = (float)LastVelocity.Size();
	const float ImpactSpeed = FMath::Max(0.f, -(float)FVector::DotProduct(LastVelocity, Hit.ImpactNormal));
	AAntCharacter* By = LastPusher.Get();
	const FString What = PropName.IsEmpty() ? FString(TEXT("Железка")) : PropName;
	if (ImpactSpeed > 600.f && Now > NextCrash)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTPROPHIT %s hit %s: speed %.0f, down %.0f"), *What, Other ? *Other->GetName() : TEXT("-"), Speed, -LastVelocity.Z);
	}
	if (AHumanCharacter* H = Cast<AHumanCharacter>(Other))
	{
		// Only what an ant pushed over counts (he kicks things around himself all the time).
		if ((By || bDevPushed) && Speed > 600.f && LastVelocity.Z < -400.f)
		{
			H->Stun(2.5f, By, What, Hit.ImpactPoint.Z < H->GetActorLocation().Z - 900.f ? 1 : 0);   // low: it landed on his foot
		}
		return;
	}
	if (ImpactSpeed < 600.f)
	{
		return;
	}
	const uint8 Friends = By ? By->Team : 0;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (A && !A->bDead && A->Creature == 0 && A->Team != Friends && !A->IsInNest()
			&& FVector::Dist(A->GetActorLocation(), Hit.ImpactPoint) < Mesh->Bounds.SphereRadius + 14.f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTCRUSH %s crushed by falling %s"), *A->GetDisplayName(), *What);
			A->HurtBy(300.f, FString::Printf(TEXT("раздавлен: упала %s"), *What.ToLower()));
		}
	}
	if (Now > NextCrash)
	{
		NextCrash = Now + 1.5f;
		AntSfx::Everyone(this, AntSfx::Drop, Hit.ImpactPoint, 0.7f);
		if (AAntGameState* CrashGS = GetWorld()->GetGameState<AAntGameState>())
		{
			CrashGS->MulticastCrumbs(Hit.ImpactPoint, Color, 8, 5.f);   // a few chips fly
		}
		if (AAntKitchenGameMode* GM = By ? GetWorld()->GetAuthGameMode<AAntKitchenGameMode>() : nullptr)   // an ant pushed it over
		{
			GM->MakeNoise(Hit.ImpactPoint, FMath::Clamp(0.4f + Mass * 0.05f, 0.4f, 0.9f), AntNoise::Crash, nullptr, By);
		}
	}
}

void AKitchenProp::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetStaticMesh(AntKit::ShapeMesh((EAntShape)ShapeIndex));
	Mesh->SetWorldScale3D(Size / 100.f);
	AntKit::Paint(Mesh, Color);
	if (Map4Look::Enabled())
	{
		// map v4: what the junk is (by its old shape and colour): a bottle cap, a grape, a pea, a thimble, a sugar cube, a coin, a meatball
		const EAntShape Sh = (EAntShape)ShapeIndex;
		const TCHAR* Name = nullptr;
		if (Sh == EAntShape::Cylinder) { Name = Color.R > 0.7f && Color.G < 0.3f ? TEXT("SM_M4_BottleCap") : (Size.Z < 20.f ? TEXT("SM_M4_Coin") : TEXT("SM_M4_Thimble")); }
		else if (Sh == EAntShape::Sphere) { Name = Color.R > 0.8f ? TEXT("SM_M4_Pebble@Meatball") : (Size.X >= 80.f ? TEXT("SM_M4_Grape") : TEXT("SM_M4_Pea@Pea")); }
		else if (Sh == EAntShape::Cube) { Name = TEXT("SM_Food_SugarCube"); }
		const AntArt::FPart& P = Name ? AntArt::Get(Name) : AntArt::Get(TEXT(""));
		if (P.IsValid())
		{
			Mesh->SetStaticMesh(P.Mesh);
			const FVector E = P.Bounds.GetSize().ComponentMax(FVector(0.01f));
			Mesh->SetWorldScale3D(FVector(Size.GetMax() / E.GetMax()));
			if (FString(Name).Contains(TEXT("@")) || FString(Name).StartsWith(TEXT("SM_Food"))) { if (P.Material) { Mesh->SetMaterial(0, P.Material); } }
			else { KitchenArt::ApplyLook(Mesh, Name, 0); }
		}
	}
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetMassOverrideInKg(NAME_None, Mass, true);
	Mesh->SetLinearDamping(0.6f);
	Mesh->SetAngularDamping(0.8f);
	if (HasAuthority() && Mass >= 3.f)
	{
		Mesh->SetNotifyRigidBodyCollision(true);
		Mesh->OnComponentHit.AddDynamic(this, &AKitchenProp::OnPropHit);
		Mesh->SetUseCCD(true);
	}
}
