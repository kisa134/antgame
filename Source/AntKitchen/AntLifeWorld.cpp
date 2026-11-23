#include "AntLife.h"
#include "AntKitchenGameMode.h"
#include "AntCharacter.h"
#include "AntAudio.h"
#include "AntFx.h"
#include "AntRaid.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "KitchenEvents.h"
#include "LevelV3.h"
#include "NpcRoutine.h"
#include "Map4Look.h"
#include "Map4.h"
#include "AntMovementComponent.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FName LifeTag(TEXT("M4Life"));

	UTexture* LifeTex(const TCHAR* Name)
	{
		static TMap<FString, UTexture*> Cache;
		if (UTexture** F = Cache.Find(Name)) { return *F; }
		UTexture* T = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Life/Textures/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (T) { T->AddToRoot(); } else { UE_LOG(LogTemp, Warning, TEXT("ANTLIFE texture %s missing"), Name); }
		return Cache.Add(Name, T);
	}

	UMaterialInterface* LifeMat(const TCHAR* Name)
	{
		static TMap<FString, UMaterialInterface*> Cache;
		if (UMaterialInterface** F = Cache.Find(Name)) { return *F; }
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Life/Materials/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (M) { M->AddToRoot(); } else { UE_LOG(LogTemp, Warning, TEXT("ANTLIFE material %s missing"), Name); }
		return Cache.Add(Name, M);
	}

	/** Printed things (quads, notes, pennants) face N with Up up: SM_Life_Quad's print reads right along +X, up along -Y, front +Z. */
	FQuat Facing(const FVector& N, const FVector& Up)
	{
		const FVector Nn = N.GetSafeNormal();
		FVector U = (Up - Nn * FVector::DotProduct(Up, Nn)).GetSafeNormal();
		if (U.IsNearlyZero()) { U = FVector::UpVector; }
		return FRotationMatrix::MakeFromXY(FVector::CrossProduct(Nn, U), -U).ToQuat();
	}

	float Hash01(int32 A, int32 B = 0)
	{
		uint32 H = (uint32)A * 2654435761u ^ ((uint32)B * 0x9e3779b9u + 0x7f4a7c15u);
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return (H & 0xffffff) / 16777216.f;
	}

	bool IsLivingThing(const AActor* A)
	{
		if (!A) { return true; }
		if (A->IsA<APawn>() || A->IsA<ACritter>()) { return true; }
		const FString C = A->GetClass()->GetName();
		return C.Contains(TEXT("Loot")) || C.Contains(TEXT("Human")) || C.Contains(TEXT("Folk")) || C.Contains(TEXT("Npc")) || C.Contains(TEXT("Vacuum")) || C.Contains(TEXT("Life"));
	}
}

// =============================================================================================
// The life of the flat (local)
// =============================================================================================

AAntLifeWorld::AAntLifeWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

AAntLifeWorld* AAntLifeWorld::Get(const UWorld* W)
{
	if (!W) { return nullptr; }
	for (TActorIterator<AAntLifeWorld> It(W); It; ++It) { return *It; }
	return nullptr;
}

void AAntLifeWorld::BeginPlay()
{
	Super::BeginPlay();
}

UStaticMeshComponent* AAntLifeWorld::FindArt(const TCHAR* MeshName, const FVector& Near, float Range) const
{
	UStaticMeshComponent* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == this || IsLivingThing(*It)) { continue; }
		TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
		for (UStaticMeshComponent* C : Cs)
		{
			if (!C || !C->GetStaticMesh() || !C->IsVisible() || C->IsA<UInstancedStaticMeshComponent>()) { continue; }
			if (C->GetStaticMesh()->GetName() != MeshName) { continue; }
			const float D = FVector::Dist(C->GetComponentLocation(), Near);
			if (D < BestD) { BestD = D; Best = C; }
		}
	}
	return Best;
}

UStaticMeshComponent* AAntLifeWorld::Mesh(const TCHAR* Prop, const FTransform& World, const TCHAR* Look, bool bSolid, USceneComponent* Parent)
{
	UStaticMesh* M = Map4Look::Prop(Prop);
	if (!M) { UE_LOG(LogTemp, Warning, TEXT("ANTLIFE prop %s missing"), Prop); return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(M);
	C->SetupAttachment(Parent ? Parent : RootComponent.Get());
	if (bSolid)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
	}
	C->SetCanEverAffectNavigation(false);
	if (Look) { if (const Map4Look::FLook* L = Map4Look::Named(Look)) { C->SetMaterial(0, Map4Look::Get(*L)); } }
	else { Map4Look::ApplySlots(C); }
	C->ComponentTags.Add(LifeTag);
	C->RegisterComponent();
	C->SetWorldTransform(World);
	AddInstanceComponent(C);
	if (bSolid) { UAntMovementComponent::MakeClimbable(C); }
	++Built;
	return C;
}

UMaterialInstanceDynamic* AAntLifeWorld::Printed(UTexture* Tex, bool bTwoSided)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bTwoSided ? TEXT("/Game/Map4/Materials/M_M4_UV2.M_M4_UV2") : TEXT("/Game/Map4/Materials/M_M4_UV.M_M4_UV"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base || !Tex) { return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetTextureParameterValue(TEXT("C"), Tex);
	PrintMids.Add(M);
	return M;
}

UStaticMeshComponent* AAntLifeWorld::PrintQuad(UTexture* Tex, const FTransform& World, bool bGlow, USceneComponent* Parent, bool bTwoSided)
{
	UStaticMeshComponent* C = Mesh(TEXT("SM_Life_Quad"), World, nullptr, false, Parent);
	if (!C) { return nullptr; }
	if (UMaterialInstanceDynamic* M = Printed(Tex, bTwoSided))
	{
		if (bGlow) { M->SetVectorParameterValue(TEXT("Glow"), FLinearColor(1.5f, 1.5f, 1.5f)); }
		C->SetMaterial(0, M);
	}
	C->ComponentTags.Add(TEXT("M4Skin"));
	C->SetCastShadow(false);
	return C;
}

UInstancedStaticMeshComponent* AAntLifeWorld::Ism(const TCHAR* Prop, const TCHAR* Look, bool bSolid)
{
	const FName Key(*FString::Printf(TEXT("Life_%s_%s"), Prop, Look ? Look : TEXT("slots")));
	for (UInstancedStaticMeshComponent* I : Isms) { if (I && I->GetFName() == Key) { return I; } }
	UStaticMesh* M = Map4Look::Prop(Prop);
	if (!M) { UE_LOG(LogTemp, Warning, TEXT("ANTLIFE prop %s missing"), Prop); return nullptr; }
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this, Key);
	I->SetMobility(EComponentMobility::Movable);
	I->SetStaticMesh(M);
	I->SetupAttachment(RootComponent);
	if (bSolid) { I->SetCollisionProfileName(TEXT("BlockAll")); }
	else { I->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	I->SetCanEverAffectNavigation(false);
	I->SetCullDistances(0, 7000);   // ant-scale details: nothing to see from further away
	if (Look) { if (const Map4Look::FLook* L = Map4Look::Named(Look)) { I->SetMaterial(0, Map4Look::Get(*L)); } }
	else { Map4Look::ApplySlots(I); }
	I->ComponentTags.Add(LifeTag);
	I->RegisterComponent();
	AddInstanceComponent(I);
	if (bSolid) { UAntMovementComponent::MakeClimbable(I); }
	Isms.Add(I);
	return I;
}

USoundBase* AAntLifeWorld::Sfx(const TCHAR* Name) const
{
	static TMap<FString, USoundBase*> Cache;
	if (USoundBase** F = Cache.Find(Name)) { return *F; }
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Life/Audio/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (S) { S->AddToRoot(); }
	return Cache.Add(Name, S);
}

UAudioComponent* AAntLifeWorld::Loop(const TCHAR* Name, const FVector& At, float Volume, float Radius)
{
	USoundBase* S = Sfx(Name);
	if (!S) { return nullptr; }
	UAudioComponent* A = NewObject<UAudioComponent>(this);
	A->SetupAttachment(RootComponent);
	A->SetSound(S);
	A->bAutoActivate = false;
	A->bAllowSpatialization = true;
	A->bOverrideAttenuation = true;
	A->AttenuationOverrides.bAttenuate = true;
	A->AttenuationOverrides.bSpatialize = true;
	A->AttenuationOverrides.FalloffDistance = Radius;
	A->AttenuationOverrides.AttenuationShapeExtents = FVector(Radius * 0.08f, 0.f, 0.f);
	A->SetVolumeMultiplier(Volume);
	A->RegisterComponent();
	A->SetWorldLocation(At);
	AddInstanceComponent(A);
	return A;
}

// ---------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------

void AAntLifeWorld::Build()
{
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	BuildOutside();
	BuildRoomLife();
	BuildDisplays();
	BuildDecor();
	BuildMicro();
	int32 Inst = 0;
	for (UInstancedStaticMeshComponent* I : Isms) { Inst += I ? I->GetInstanceCount() : 0; }
	UE_LOG(LogTemp, Display, TEXT("ANTLIFE built: %d parts, %d micro details in %d instanced sets, %.1f ms"), Built, Inst, Isms.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
	for (const TPair<FString, int32>& P : PropCounts) { UE_LOG(LogTemp, Display, TEXT("ANTLIFE details %s %d"), *P.Key, P.Value); }
}

void AAntLifeWorld::BuildOutside()
{
	// ---- the cloud layer: a huge sheet high over the house, drifting (seen through the windows and from the yard)
	if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Clouds")))
	{
		Clouds = Mesh(TEXT("SM_Life_Quad"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(3000.f, -20000.f, 70000.f), FVector(9000.f, 9000.f, 1.f)));
		if (Clouds)
		{
			CloudMid = UMaterialInstanceDynamic::Create(M, this);
			Clouds->SetMaterial(0, CloudMid);
			Clouds->SetCastShadow(false);
			Clouds->ComponentTags.Add(TEXT("M4Fx"));
			Clouds->SetTranslucentSortPriority(-10);
		}
	}
	// ---- the night dome: stars and the moon (additive; hidden by day)
	if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Stars")))
	{
		Stars = Mesh(TEXT("SM_M4_Ball"), FTransform(FRotator::ZeroRotator, FVector(4000.f, -4000.f, 0.f), FVector(5000.f)));
		if (Stars)
		{
			StarMid = UMaterialInstanceDynamic::Create(M, this);
			Stars->SetMaterial(0, StarMid);
			Stars->SetCastShadow(false);
			Stars->ComponentTags.Add(TEXT("M4Fx"));
			Stars->SetTranslucentSortPriority(-20);
			Stars->SetVisibility(false);
		}
	}
	// ---- the house across the yard: two blocks of flats and the city behind (procedural facades, lit windows by night)
	if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Facade")))
	{
		struct FBlock { FVector Min, Max; FLinearColor Wall; float Seed; };
		const FBlock Blocks[] = {
			{ FVector(-30000.f, -30000.f, -300.f), FVector(4000.f, -20000.f, 28200.f), FLinearColor(0.80f, 0.70f, 0.58f), 3.f },
			{ FVector(8500.f, -34000.f, -300.f), FVector(40000.f, -23000.f, 22600.f), FLinearColor(0.70f, 0.72f, 0.74f), 7.f },
			{ FVector(-60000.f, -90000.f, -300.f), FVector(-20000.f, -80000.f, 45000.f), FLinearColor(0.62f, 0.64f, 0.70f), 11.f },
			{ FVector(15000.f, -100000.f, -300.f), FVector(60000.f, -88000.f, 38000.f), FLinearColor(0.66f, 0.62f, 0.58f), 13.f },
			{ FVector(-14000.f, -110000.f, -300.f), FVector(12000.f, -100000.f, 60000.f), FLinearColor(0.58f, 0.62f, 0.68f), 17.f },
		};
		for (const FBlock& B : Blocks)
		{
			const FVector C = (B.Min + B.Max) * 0.5f, S = (B.Max - B.Min) / 100.f;
			if (UStaticMeshComponent* Bx = Mesh(TEXT("SM_M4_BoxSharp"), FTransform(FRotator::ZeroRotator, C, S)))
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(M, this);
				Mid->SetVectorParameterValue(TEXT("Wall"), B.Wall);
				Mid->SetScalarParameterValue(TEXT("Seed"), B.Seed);
				Mid->SetScalarParameterValue(TEXT("Top"), B.Max.Z - 1400.f);
				Mid->SetVectorParameterValue(TEXT("Cell"), FLinearColor(2200.f, 5600.f, 0.f));
				Bx->SetMaterial(0, Mid);
				Bx->SetCastShadow(false);   // the sun comes in through the windows over them
				Bx->ComponentTags.Add(TEXT("M4Arch"));
				FacadeMids.Add(Mid);
			}
		}
	}
	// ---- trees beyond the fence (copies of the yard's tree), swaying
	if (UStaticMeshComponent* Tree = FindArt(TEXT("SM_Y_Tree"), FVector(6400.f, -10700.f, 0.f), 8000.f))
	{
		const FVector Spots[] = { FVector(-9000.f, -19000.f, 0.f), FVector(-2500.f, -21000.f, 0.f), FVector(14000.f, -19500.f, 0.f), FVector(21000.f, -22000.f, 0.f) };
		int32 k = 0;
		for (const FVector& P : Spots)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Tree->GetStaticMesh());
			for (int32 m = 0; m < Tree->GetNumMaterials(); ++m) { C->SetMaterial(m, Tree->GetMaterial(m)); }
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetupAttachment(RootComponent);
			C->ComponentTags.Add(LifeTag);
			C->RegisterComponent();
			const FRotator R(0.f, 40.f + 77.f * k, 0.f);
			C->SetWorldTransform(FTransform(R, P, Tree->GetComponentScale() * (0.9f + 0.15f * k)));
			C->SetCastShadow(false);
			AddInstanceComponent(C);
			Sways.Add({ C, R, 0.7f, 0.35f + 0.07f * k, k * 1.7f, FVector(1.f, 0.3f, 0.f) });
			++Built; ++k;
		}
	}
	// ---- potted plants by the open windows sway a little in the draught
	for (const TPair<const TCHAR*, FVector>& P : { TPair<const TCHAR*, FVector>(TEXT("SM_M4R_K_PlantPot"), FVector(-1250.f, -4905.f, 2200.f)), TPair<const TCHAR*, FVector>(TEXT("SM_K_PlantPot"), FVector(-1250.f, -4905.f, 2200.f)),
		TPair<const TCHAR*, FVector>(TEXT("SM_M4R_L_PottedPlant"), FVector(18900.f, 900.f, 0.f)), TPair<const TCHAR*, FVector>(TEXT("SM_L_PottedPlant"), FVector(18900.f, 900.f, 0.f)) })
	{
		if (UStaticMeshComponent* C = FindArt(P.Key, P.Value, 1500.f))
		{
			if (C->Mobility != EComponentMobility::Movable) { continue; }   // static pieces stay put (lighting, collision cache)
			Sways.Add({ C, C->GetComponentRotation(), 0.6f, 0.9f, (float)P.Value.X * 0.001f, FVector(1.f, 0.6f, 0.f) });
		}
	}
	// ---- birds: three sparrows wheeling over the yard (seen through the windows), a pigeon that visits the kitchen sill
	for (int32 k = 0; k < 3; ++k)
	{
		const FVector C(-2000.f + k * 6000.f, -9500.f - k * 1500.f, 4500.f + k * 1200.f);
		UStaticMeshComponent* B = Mesh(TEXT("SM_Life_Bird"), FTransform(FRotator::ZeroRotator, C, FVector(2.9f)));
		if (!B) { break; }
		if (UMaterialInstanceDynamic* M = Printed(LifeTex(k == 1 ? TEXT("T_Life_Tit") : TEXT("T_Life_Sparrow")), false)) { B->SetMaterial(B->GetMaterialIndex(TEXT("M4Prop")), M); }
		FFlyer F;
		F.Body = B;
		for (int32 s = 0; s < 2; ++s)
		{
			UStaticMeshComponent* Wg = Mesh(TEXT("SM_Life_BirdWing"), FTransform::Identity, nullptr, false, B);
			if (Wg)
			{
				Wg->SetRelativeLocation(FVector(0.02f, s ? 6.f : -6.f, 8.f));
				Wg->SetRelativeScale3D(FVector(1.f, s ? -1.f : 1.f, 1.f));
				if (UMaterialInstanceDynamic* M = Printed(LifeTex(TEXT("T_Life_Sparrow")), true)) { Wg->SetMaterial(0, M); }
				Wg->SetCastShadow(false);
			}
			(s ? F.WingR : F.WingL) = Wg;
		}
		F.Center = C; F.Radii = FVector(3500.f + 800.f * k, 2200.f, 600.f); F.Speed = 0.32f + 0.07f * k; F.Phase = k * 2.1f; F.Kind = 0;
		Flyers.Add(F);
	}
	if (UStaticMeshComponent* Pg = Mesh(TEXT("SM_Life_Bird"), FTransform(FRotator::ZeroRotator, FVector(0.f, -5330.f, -9000.f), FVector(6.2f))))
	{
		if (UMaterialInstanceDynamic* M = Printed(LifeTex(TEXT("T_Life_Pigeon")), false)) { Pg->SetMaterial(Pg->GetMaterialIndex(TEXT("M4Prop")), M); }
		Pigeon = Pg;
		for (int32 s = 0; s < 2; ++s)
		{
			if (UStaticMeshComponent* Wg = Mesh(TEXT("SM_Life_BirdWing"), FTransform::Identity, nullptr, false, Pg))
			{
				Wg->SetRelativeLocation(FVector(0.02f, s ? 6.f : -6.f, 8.f));
				Wg->SetRelativeScale3D(FVector(1.f, s ? -1.f : 1.f, 1.f));
				if (UMaterialInstanceDynamic* M = Printed(LifeTex(TEXT("T_Life_Pigeon")), true)) { Wg->SetMaterial(0, M); }
				(s ? PigeonWingR : PigeonWingL) = Wg;
			}
		}
		Pg->SetVisibility(false, true);
	}
	// ---- cars along the street in front of the house, headlights by night
	if (UStaticMeshComponent* Road = FindArt(TEXT("SM_S_Road"), FVector(-13800.f, -4400.f, -260.f), 6000.f))
	{
		const FBox Rb = Road->Bounds.GetBox();
		const bool bAlongY = Rb.GetSize().Y >= Rb.GetSize().X;
		const float Lane = bAlongY ? Rb.Min.X + Rb.GetSize().X * 0.25f : Rb.Min.Y + Rb.GetSize().Y * 0.25f;
		RoadA = bAlongY ? FVector(Lane, Rb.Min.Y - 20000.f, Rb.Max.Z) : FVector(Rb.Min.X - 20000.f, Lane, Rb.Max.Z);
		RoadB = bAlongY ? FVector(Lane, Rb.Max.Y + 20000.f, Rb.Max.Z) : FVector(Rb.Max.X + 20000.f, Lane, Rb.Max.Z);
		if (UStaticMeshComponent* Car = FindArt(TEXT("SM_S_Car"), FVector(-13550.f, 50.f, -230.f), 8000.f))
		{
			// the parked car and its wheels: copied as one moving group
			TArray<UStaticMeshComponent*> Parts = { Car };
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
				for (UStaticMeshComponent* C : Cs)
				{
					if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_S_Car_Wheel")) && FVector::Dist(C->GetComponentLocation(), Car->GetComponentLocation()) < 7000.f) { Parts.Add(C); }
				}
			}
			const FTransform CarT = Car->GetComponentTransform();
			for (int32 k = 0; k < 2; ++k)
			{
				USceneComponent* Root = NewObject<USceneComponent>(this);
				Root->SetMobility(EComponentMobility::Movable);
				Root->SetupAttachment(RootComponent);
				Root->RegisterComponent();
				AddInstanceComponent(Root);
				Root->SetWorldTransform(CarT);
				for (UStaticMeshComponent* P : Parts)
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
					C->SetMobility(EComponentMobility::Movable);
					C->SetStaticMesh(P->GetStaticMesh());
					for (int32 m = 0; m < P->GetNumMaterials(); ++m) { C->SetMaterial(m, P->GetMaterial(m)); }
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->SetupAttachment(Root);
					C->ComponentTags.Add(LifeTag);
					C->RegisterComponent();
					C->SetRelativeTransform(P->GetComponentTransform().GetRelativeTransform(CarT));
					AddInstanceComponent(C);
					++Built;
				}
				FCar Cr;
				Cr.Root = Root; Cr.Offset = k * 0.5f; Cr.Dir = k ? -1.f : 1.f; Cr.Lane = k ? -1800.f : 0.f;
				const FBox Cb = Car->Bounds.GetBox();
				const float HalfL = FMath::Max(Cb.GetExtent().X, Cb.GetExtent().Y);
				for (int32 s = -1; s <= 1; s += 2)
				{
					USpotLightComponent* L = NewObject<USpotLightComponent>(this);
					L->SetupAttachment(Root);
					L->SetMobility(EComponentMobility::Movable);
					L->SetIntensityUnits(ELightUnits::Candelas);
					L->SetIntensity(0.f);
					L->SetLightColor(FLinearColor(1.f, 0.92f, 0.78f));
					L->SetAttenuationRadius(30000.f);
					L->SetOuterConeAngle(28.f);
					L->SetCastShadows(false);
					L->RegisterComponent();
					AddInstanceComponent(L);
					L->SetWorldLocation(Root->GetComponentLocation() + FVector(0.f, 0.f, Cb.GetExtent().Z * 0.6f) + Root->GetRightVector() * s * HalfL * 0.35f);
					Cr.Lights.Add(L);
				}
				(void)HalfL;
				Cars.Add(Cr);
			}
		}
	}
	// ---- the rain on the window openings (a translucent curtain of running drops) and its sound
	if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Rain")))
	{
		struct FWin { FVector C; float W, H; FVector N; };
		const FWin Wins[] = { { FVector(0.f, -5040.f, 3300.f), 3300.f, 2200.f, FVector(0.f, 1.f, 0.f) }, { FVector(18285.f, -9230.f, 3200.f), 3000.f, 2000.f, FVector(0.f, 1.f, 0.f) } };
		for (const FWin& Wn : Wins)
		{
			if (UStaticMeshComponent* P = Mesh(TEXT("SM_Life_Quad"), FTransform(Facing(Wn.N, FVector::UpVector), Wn.C, FVector(Wn.W / 100.f, Wn.H / 100.f, 1.f))))
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(M, this);
				Mid->SetScalarParameterValue(TEXT("Aspect"), Wn.H / Wn.W);
				Mid->SetScalarParameterValue(TEXT("Opacity"), 0.3f);
				Mid->SetScalarParameterValue(TEXT("Cols"), 95.f);
				P->SetMaterial(0, Mid);
				P->SetCastShadow(false);
				P->ComponentTags.Add(TEXT("M4Fx"));
				P->SetVisibility(false);
				RainMids.Add(Mid);
				RainPanes.Add(P);
			}
		}
		RainLoop = Loop(TEXT("A_Life_Rain"), FVector(9000.f, -6000.f, 2500.f), 0.9f, 16000.f);
	}
}

void AAntLifeWorld::BuildRoomLife()
{
	// ---- steam: the kettle's spout, the soup pot, the dishwasher's door
	if (UStaticMeshComponent* K = FindArt(TEXT("SM_M4R_K_Kettle"), FVector(2250.f, 3600.f, 1710.f), 1500.f)) { Steams.Add({ K->Bounds.Origin + FVector(K->Bounds.BoxExtent.X * 0.8f, 0.f, K->Bounds.BoxExtent.Z * 0.6f), 30.f, 0.18f, 0.f, 1 }); }
	else if (UStaticMeshComponent* K2 = FindArt(TEXT("SM_K_Kettle"), FVector(2250.f, 3600.f, 1710.f), 1500.f)) { Steams.Add({ K2->GetComponentTransform().TransformPosition(FVector(17.f, 0.f, 18.f)), 30.f, 0.18f, 0.f, 1 }); }
	if (UStaticMeshComponent* P = FindArt(TEXT("SM_K_Pot"), FVector(-350.f, 3500.f, 1765.f), 1500.f)) { Steams.Add({ P->Bounds.Origin + FVector(0.f, 0.f, P->Bounds.BoxExtent.Z + 10.f), 55.f, 0.3f, 0.f, 2 }); }
	if (UStaticMeshComponent* D = FindArt(TEXT("SM_K_Dishwasher"), FVector(650.f, 3875.f, 0.f), 1500.f))
	{
		Dishwasher = D;
		DishwasherHome = D->GetComponentLocation();
		Steams.Add({ FVector(D->Bounds.Origin.X, D->Bounds.Origin.Y - D->Bounds.BoxExtent.Y - 10.f, D->Bounds.Origin.Z + D->Bounds.BoxExtent.Z - 40.f), 40.f, 0.7f, 0.f, 3 });
		WashLoop = Loop(TEXT("A_Life_Wash"), D->Bounds.Origin, 0.9f, 7000.f);
		// what the machine shakes on the worktop above it: a teaspoon's worth of loose bits, each its own little piece
		const FVector Top(650.f, 3400.f, 1712.f);
		const TCHAR* Bits[][2] = { { TEXT("SM_M4_BottleCap"), nullptr }, { TEXT("SM_M4_Coin"), nullptr }, { TEXT("SM_M4_Coin"), nullptr }, { TEXT("SM_M4_Button"), nullptr }, { TEXT("SM_Life_Brick"), TEXT("PlasticBlue") } };
		for (int32 k = 0; k < UE_ARRAY_COUNT(Bits); ++k)
		{
			FHitResult H;
			const FVector P = Top + FVector(-500.f + k * 230.f, (k % 2) * 160.f, 0.f);
			if (GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 300.f), P - FVector(0.f, 0.f, 200.f), ECC_Visibility) && H.ImpactNormal.Z > 0.8f)
			{
				UStaticMesh* Ms = Map4Look::Prop(Bits[k][0]);
				const float Sz = Ms ? Ms->GetBoundingBox().GetSize().GetMax() : 100.f;
				const float S = FString(Bits[k][0]).StartsWith(TEXT("SM_Life")) ? 1.f : (k == 0 ? 50.f : k == 3 ? 23.f : 38.f) / FMath::Max(1.f, Sz);
				if (UStaticMeshComponent* C = Mesh(Bits[k][0], FTransform(FRotator(0.f, k * 47.f, 0.f), H.ImpactPoint + FVector(0.f, 0.f, 0.5f), FVector(S)), Bits[k][1]))
				{
					Shakers.Add({ C, C->GetComponentLocation() });
					PropCounts.FindOrAdd(TEXT("plita"))++;
				}
			}
		}
	}
	KettleLoop = Steams.Num() ? Loop(TEXT("A_Life_KettleBoil"), Steams[0].At, 0.8f, 5000.f) : nullptr;
	// ---- the dripping tap
	if (UStaticMeshComponent* F = FindArt(TEXT("SM_K_Faucet"), FVector(-3500.f, 4636.f, 1710.f), 1500.f))
	{
		const FBox B = F->Bounds.GetBox();
		FaucetTip = FVector(B.GetCenter().X, B.Min.Y + 45.f, B.Max.Z - 130.f);
	}
	// ---- the wall clock's hands (hour, minute, second) on SM_K_Clock's face (+Y, the brass pin at y 1.6, centre z 15.8 cm)
	if (UStaticMeshComponent* Cl = FindArt(TEXT("SM_K_Clock"), FVector(6970.f, 1500.f, 3300.f), 2000.f))
	{
		const float Len[3] = { 0.075f, 0.115f, 0.13f };
		const float Wd[3] = { 1.4f, 1.f, 0.35f };
		for (int32 h = 0; h < 3; ++h)
		{
			if (UStaticMeshComponent* Hd = Mesh(TEXT("SM_Life_ClockHand"), FTransform::Identity, h == 2 ? TEXT("Red") : TEXT("BlackGlass"), false, Cl))
			{
				Hd->SetRelativeLocation(FVector(0.f, 1.75f + h * 0.18f, 15.8f));
				Hd->SetRelativeScale3D(FVector(Len[h] * Wd[h], Len[h], Len[h]));
				Hd->SetCastShadow(h < 2);
				Hd->ComponentTags.Add(TEXT("M4Thread"));
				ClockHands[h] = Hd;
			}
		}
	}
	// ---- flies over the fruit bowl and the bin, a moth at the floor lamp (night)
	auto AddBuzz = [this](const FVector& C, float R, float Speed, float Phase, bool bMoth)
	{
		UStaticMesh* Fm = Map4Look::Prop(TEXT("SM_M4_FlyBody"));
		const float Ext = Fm ? FMath::Max(0.01f, (float)Fm->GetBounds().BoxExtent.GetMax() * 2.f) : 1.f;
		const float S = (bMoth ? 70.f : 36.f) / Ext;   // a house fly ~2 mm long... at ant scale 36 uu; the moth twice that
		UStaticMeshComponent* B = Mesh(TEXT("SM_M4_FlyBody"), FTransform(FRotator::ZeroRotator, C, FVector(S)));
		if (!B) { return; }
		B->SetCastShadow(false);
		FBuzz Z;
		Z.Body = B;
		if (bMoth)
		{
			UStaticMesh* Wm = Map4Look::Prop(TEXT("SM_M4_Wing"));
			const float We = Wm ? FMath::Max(1.f, (float)Wm->GetBounds().BoxExtent.GetMax() * 2.f) : 90.f;
			for (int32 s = 0; s < 2; ++s)
			{
				UStaticMeshComponent* Wg = Mesh(TEXT("SM_M4_Wing"), FTransform::Identity, TEXT("Biscuit"), false, B);
				if (Wg)
				{
					Wg->SetRelativeLocation(FVector(0.f, (s ? 1.f : -1.f) * 0.1f, 0.3f));
					const float Ws = 55.f / We / S;
					Wg->SetRelativeScale3D(FVector(Ws, s ? -Ws : Ws, Ws));
					Wg->SetCastShadow(false);
				}
				(s ? Z.WingR : Z.WingL) = Wg;
			}
		}
		Z.Center = C; Z.R = R; Z.Speed = Speed; Z.Phase = Phase; Z.bMoth = bMoth;
		Buzzers.Add(Z);
	};
	if (UStaticMeshComponent* Bowl = FindArt(TEXT("SM_K_FruitBowl"), FVector(-1050.f, -1150.f, 1400.f), 2000.f))
	{
		for (int32 k = 0; k < 3; ++k) { AddBuzz(Bowl->Bounds.Origin + FVector(0.f, 0.f, Bowl->Bounds.BoxExtent.Z + 120.f + k * 40.f), 160.f + k * 50.f, 1.6f + k * 0.4f, k * 2.f, false); }
		FlyLoop = Loop(TEXT("A_Life_FlyBuzz"), Bowl->Bounds.Origin, 0.5f, 2500.f);
	}
	for (const TCHAR* Bin : { TEXT("SM_M4R_K_Bin"), TEXT("SM_K_Bin") })
	{
		if (UStaticMeshComponent* B = FindArt(Bin, FVector(6000.f, -1500.f, 0.f), 2000.f))
		{
			for (int32 k = 0; k < 2; ++k) { AddBuzz(B->Bounds.Origin + FVector(0.f, 0.f, B->Bounds.BoxExtent.Z + 160.f + k * 60.f), 220.f, 1.3f + k * 0.5f, 1.f + k, false); }
			break;
		}
	}
	if (UStaticMeshComponent* Lamp = FindArt(TEXT("SM_L_FloorLamp"), FVector(9900.f, -7450.f, 0.f), 2500.f))
	{
		AddBuzz(Lamp->Bounds.Origin + FVector(0.f, 0.f, Lamp->Bounds.BoxExtent.Z * 0.82f), 160.f, 2.4f, 0.f, true);
	}
	// ---- the phone on the sofa: it buzzes now and then (and creeps), its screen lights up
	if (UStaticMeshComponent* Ph = FindArt(TEXT("SM_V3_Phone"), FVector(11300.f, 200.f, 950.f), 2500.f))
	{
		Phone = Ph;
		PhoneHome = Ph->GetComponentTransform();
		if (UStaticMeshComponent* Scr = Mesh(TEXT("SM_Life_Quad"), FTransform::Identity, nullptr, false, Ph))
		{
			Scr->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 1.03f), FVector(0.076f, 0.152f, 1.f)));
			if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Display")))
			{
				PhoneMid = UMaterialInstanceDynamic::Create(M, this);
				PhoneMid->SetTextureParameterValue(TEXT("Tex"), LifeTex(TEXT("T_Life_PhoneLock")));
				PhoneMid->SetScalarParameterValue(TEXT("Power"), 0.f);
				Scr->SetMaterial(0, PhoneMid);
			}
			Scr->SetCastShadow(false);
			Scr->ComponentTags.Add(TEXT("M4Skin"));
		}
	}
	// ---- the kitchen radio on the window sill (it plays in the morning)
	{
		FHitResult H;
		const FVector P(1000.f, -4905.f, 2600.f);
		if (GetWorld()->LineTraceSingleByChannel(H, P, P - FVector(0.f, 0.f, 800.f), ECC_Visibility) && H.ImpactNormal.Z > 0.8f)
		{
			if (UStaticMeshComponent* R = Mesh(TEXT("SM_Life_Radio"), FTransform(FRotator(0.f, 180.f, 0.f), H.ImpactPoint, FVector(1.f)), nullptr, true))
			{
				RadioLoop = Loop(TEXT("A_Life_Radio"), R->Bounds.Origin, 0.8f, 9000.f);
				PropCounts.FindOrAdd(TEXT("plato"))++;
			}
		}
	}
	// ---- the music box on the floor by the kid's cushion, and the kid's tablet on the cushion
	{
		FHitResult H;
		FVector P(12950.f, -6950.f, 600.f);
		if (GetWorld()->LineTraceSingleByChannel(H, P, P - FVector(0.f, 0.f, 800.f), ECC_Visibility))
		{
			if (UStaticMeshComponent* Mb = Mesh(TEXT("SM_Life_MusicBox"), FTransform(FRotator(0.f, 200.f, 0.f), H.ImpactPoint, FVector(1.f)), nullptr, true))
			{
				if (UStaticMeshComponent* St = Mesh(TEXT("SM_Life_MusicStar"), FTransform::Identity, nullptr, false, Mb))
				{
					St->SetRelativeLocation(FVector(0.f, 0.f, 5.f * 0.19f * 100.f * 0.55f));
					MusicStar = St;
				}
				MusicBoxLoop = Loop(TEXT("A_Life_MusicBox"), Mb->Bounds.Origin, 0.7f, 5000.f);
				PropCounts.FindOrAdd(TEXT("play"))++;
			}
		}
		P = FVector(12450.f, -6350.f, 1200.f);
		if (GetWorld()->LineTraceSingleByChannel(H, P, P - FVector(0.f, 0.f, 1400.f), ECC_Visibility))
		{
			if (UStaticMeshComponent* Tb = Mesh(TEXT("SM_Life_Tablet"), FTransform(FRotator(4.f, 105.f, 0.f), H.ImpactPoint + FVector(0.f, 0.f, 1.f), FVector(1.f)), nullptr, true))
			{
				TabletAt = Tb->Bounds.Origin;
				if (UStaticMeshComponent* Scr = Mesh(TEXT("SM_Life_Quad"), FTransform::Identity, nullptr, false, Tb))
				{
					Scr->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 0.96f * 19.f + 0.3f), FVector(22.4f * 0.19f, 15.6f * 0.19f, 1.f)));
					if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Screen")))
					{
						TabletPlayer = NewObject<UMediaPlayer>(this);
						TabletPlayer->SetLooping(true);
						TabletPlayer->PlayOnOpen = true;
						TabletTex = NewObject<UMediaTexture>(this);
						TabletTex->AutoClear = true;
						TabletTex->SetMediaPlayer(TabletPlayer);
						TabletTex->UpdateResource();
						TabletMid = UMaterialInstanceDynamic::Create(M, this);
						TabletMid->SetTextureParameterValue(TEXT("Video"), TabletTex);
						TabletMid->SetScalarParameterValue(TEXT("Power"), 1.f);
						TabletMid->SetScalarParameterValue(TEXT("Bright"), 1.6f);
						Scr->SetMaterial(0, TabletMid);
						TabletSource = NewObject<UFileMediaSource>(this);
						TabletSource->SetFilePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/Life/ch6_kids.mp4")));
					}
					Scr->SetCastShadow(false);
					Scr->ComponentTags.Add(TEXT("M4Skin"));
				}
				PropCounts.FindOrAdd(TEXT("play"))++;
			}
		}
	}
}

void AAntLifeWorld::BuildDisplays()
{
	UMaterialInterface* M = LifeMat(TEXT("M_Life_Display"));
	if (!M) { return; }
	// ---- the microwave's clock (the BlackGlass panel, SM_K_Microwave cm: x -27.4..-17.9, the face at y 23.7, top part)
	if (UStaticMeshComponent* Mw = FindArt(TEXT("SM_K_Microwave"), FVector(1850.f, 4440.f, 1710.f), 2000.f))
	{
		MicrowaveRT = UKismetRenderingLibrary::CreateRenderTarget2D(this, 256, 96, RTF_RGBA8);
		if (UStaticMeshComponent* Q = Mesh(TEXT("SM_Life_Quad"), FTransform::Identity, nullptr, false, Mw))
		{
			const FQuat Rq = Facing(FVector(0.f, 1.f, 0.f), FVector::UpVector);
			Q->SetRelativeTransform(FTransform(Rq, FVector(-22.65f, 23.82f, 27.6f), FVector(0.085f, 0.032f, 1.f)));
			MicrowaveMid = UMaterialInstanceDynamic::Create(M, this);
			MicrowaveMid->SetTextureParameterValue(TEXT("Tex"), MicrowaveRT);
			MicrowaveMid->SetScalarParameterValue(TEXT("Bright"), 3.f);
			Q->SetMaterial(0, MicrowaveMid);
			Q->SetCastShadow(false);
			Q->ComponentTags.Add(TEXT("M4Skin"));
		}
	}
	// ---- the fridge's little panel on the freezer door (temperatures)
	if (UStaticMeshComponent* Fd = FindArt(TEXT("SM_K_Fridge_DoorFreezer"), FVector(4700.f, 3100.f, 2500.f), 3000.f))
	{
		FridgeRT = UKismetRenderingLibrary::CreateRenderTarget2D(this, 256, 128, RTF_RGBA8);
		const FBox Lb = Fd->GetStaticMesh()->GetBoundingBox();
		const FTransform T = Fd->GetComponentTransform();
		// the face that looks into the room (-Y in the world) in the door's own frame
		const FVector LocalOut = T.InverseTransformVectorNoScale(FVector(0.f, -1.f, 0.f));
		const int32 Ax = FMath::Abs(LocalOut.X) > FMath::Abs(LocalOut.Y) ? (FMath::Abs(LocalOut.X) > FMath::Abs(LocalOut.Z) ? 0 : 2) : (FMath::Abs(LocalOut.Y) > FMath::Abs(LocalOut.Z) ? 1 : 2);
		FVector Face = Lb.GetCenter();
		Face[Ax] = LocalOut[Ax] > 0.f ? Lb.Max[Ax] + 0.15f : Lb.Min[Ax] - 0.15f;
		Face.Z = Lb.Min.Z + Lb.GetSize().Z * 0.72f;
		FVector N = FVector::ZeroVector; N[Ax] = LocalOut[Ax] > 0.f ? 1.f : -1.f;
		if (UStaticMeshComponent* Q = Mesh(TEXT("SM_Life_Quad"), FTransform::Identity, nullptr, false, Fd))
		{
			Q->SetRelativeTransform(FTransform(Facing(N, FVector::UpVector), Face, FVector(0.11f, 0.055f, 1.f)));
			FridgeMid = UMaterialInstanceDynamic::Create(M, this);
			FridgeMid->SetTextureParameterValue(TEXT("Tex"), FridgeRT);
			FridgeMid->SetScalarParameterValue(TEXT("Bright"), 2.5f);
			Q->SetMaterial(0, FridgeMid);
			Q->SetCastShadow(false);
			Q->ComponentTags.Add(TEXT("M4Skin"));
		}
	}
}

void AAntLifeWorld::BuildDecor()
{
	UWorld* W = GetWorld();
	const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
	const int32 Seed = S ? S->Seed : 7;
	// ---- notes on the fridge's side (three of eight, by seed), the face towards the bin (+X)
	if (UStaticMeshComponent* Fr = FindArt(TEXT("SM_K_Fridge"), FVector(3700.f, 3935.f, 0.f), 2000.f))
	{
		const FBox B = Fr->Bounds.GetBox();
		const FVector Spots[] = { FVector(B.Max.X + 1.5f, 3450.f, 1650.f), FVector(B.Max.X + 1.5f, 4250.f, 1900.f), FVector(B.Max.X + 1.5f, 3550.f, 2350.f) };
		for (int32 k = 0; k < 3; ++k)
		{
			const int32 Which = (Seed * 3 + k * 5) % 8;
			const FQuat Q = Facing(FVector(1.f, 0.f, 0.f), FVector::UpVector) * FQuat(FVector::UpVector, FMath::DegreesToRadians(0.f));
			const FQuat Tilt(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians((Hash01(Seed, k) - 0.5f) * 14.f));
			if (UStaticMeshComponent* N = Mesh(TEXT("SM_Life_Note"), FTransform(Tilt * Q, Spots[k], FVector(1.f))))
			{
				if (UMaterialInstanceDynamic* Mi = Printed(LifeTex(*FString::Printf(TEXT("T_Life_Note%d"), Which)), true)) { N->SetMaterial(0, Mi); }
				N->ComponentTags.Add(TEXT("M4Skin"));
				Mesh(TEXT("SM_M4_Button"), FTransform(FRotator(0.f, 0.f, 90.f), Spots[k] + FVector(4.f, 0.f, 0.36f * 19.f * 2.f), FVector(23.f / 100.f)), (k % 2) ? TEXT("Red") : TEXT("PlasticBlue"));
				PropCounts.FindOrAdd(TEXT("lednik"))++;
			}
		}
	}
	// ---- the calendar page: November, the 23rd circled for the birthday
	for (const TCHAR* Cal : { TEXT("SM_M4R_K_Calendar"), TEXT("SM_K_Calendar") })
	{
		if (UStaticMeshComponent* C = FindArt(Cal, FVector(-7000.f, -3600.f, 2300.f), 2000.f))
		{
			const FBox B = C->Bounds.GetBox();
			const FVector N(1.f, 0.f, 0.f);   // the west wall's calendar looks east into the kitchen
			const FVector At(B.Max.X + 1.f, B.GetCenter().Y, B.Min.Z + B.GetSize().Z * 0.3f);
			PrintQuad(LifeTex(TEXT("T_Life_Calendar")), FTransform(Facing(N, FVector::UpVector), At, FVector(B.GetSize().Y * 0.88f / 100.f, B.GetSize().Z * 0.5f / 100.f, 1.f)), false);
			PropCounts.FindOrAdd(TEXT("plintus"))++;
			break;
		}
	}
	// ---- the birthday banner «С ДНЁМ РОЖДЕНИЯ» over the sofa (pennants on a string, sagging)
	{
		const FVector A(10900.f, 1425.f, 4550.f), Bp(16300.f, 1425.f, 4550.f);
		const int32 Num = 13;
		const float Slots = 15.f;
		int32 Letter = 0;
		const FString Txt = TEXT("С ДНЁМ РОЖДЕНИЯ");
		for (int32 i = 0; i < Txt.Len(); ++i)
		{
			const float T = (i + 0.5f) / Slots;
			const FVector P = FMath::Lerp(A, Bp, T) - FVector(0.f, 0.f, 380.f * FMath::Sin(T * PI));
			if (Txt[i] == TEXT(' ')) { continue; }
			const float Slope = FMath::RadiansToDegrees(FMath::Atan2(-380.f * PI * FMath::Cos(T * PI), Bp.X - A.X));
			const FQuat Q = FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(-Slope)) * Facing(FVector(0.f, -1.f, 0.f), FVector::UpVector);
			if (UStaticMeshComponent* Pn = Mesh(TEXT("SM_Life_Pennant"), FTransform(Q, P, FVector(1.f))))
			{
				if (UMaterialInstanceDynamic* Mi = Printed(LifeTex(*FString::Printf(TEXT("T_Life_Letter%02d"), Letter)), true)) { Pn->SetMaterial(0, Mi); }
				Pn->ComponentTags.Add(TEXT("M4Skin"));
				Sways.Add({ Pn, Pn->GetComponentRotation(), 2.5f, 0.6f + 0.05f * Letter, Letter * 0.7f, FVector(0.f, 0.f, 1.f) });
			}
			++Letter;
		}
		(void)Num;
		// the string: segments of thread between the pennants
		const int32 Seg = 24;
		for (int32 k = 0; k < Seg; ++k)
		{
			const float T0 = (float)k / Seg, T1 = (float)(k + 1) / Seg;
			const FVector P0 = FMath::Lerp(A, Bp, T0) - FVector(0.f, 0.f, 380.f * FMath::Sin(T0 * PI)), P1 = FMath::Lerp(A, Bp, T1) - FVector(0.f, 0.f, 380.f * FMath::Sin(T1 * PI));
			UStaticMesh* Th = Map4Look::Prop(TEXT("SM_M4_Thread"));
			if (!Th) { break; }
			const FBox Tb = Th->GetBoundingBox();
			const float L = FMath::Max(1.f, Tb.GetSize().GetMax());
			const FVector D = P1 - P0;
			// the thread mesh runs along its longest axis: Z for a unit thread
			const bool bZ = Tb.GetSize().Z >= Tb.GetSize().X;
			const FRotator R = bZ ? FRotationMatrix::MakeFromZ(D).Rotator() : D.Rotation();
			if (UStaticMeshComponent* C = Mesh(TEXT("SM_M4_Thread"), FTransform(R, (P0 + P1) * 0.5f, bZ ? FVector(0.6f, 0.6f, D.Size() / L) : FVector(D.Size() / L, 0.6f, 0.6f)), TEXT("Yarn")))
			{
				C->ComponentTags.Add(TEXT("M4Thread"));
				C->SetCastShadow(false);
			}
		}
		PropCounts.FindOrAdd(TEXT("play")) += Letter;
	}
	// ---- the rival colony's graffiti on the skirting by the red crack (living room, south wall) and coffee rings, scratches
	if (UMaterialInterface* Dm = LifeMat(TEXT("M_Life_Decal")))
	{
		struct FDec { const TCHAR* Tex; FVector At; FVector N; float W, H, Rot; FLinearColor Tint; const TCHAR* District; };
		const FDec Decs[] = {
			{ TEXT("T_Life_Graffiti"), FVector(17950.f, -8968.f, 160.f), FVector(0.f, 1.f, 0.f), 420.f, 210.f, 0.f, FLinearColor::White, TEXT("stenka") },
			{ TEXT("T_Life_Graffiti"), FVector(-6985.f, 2600.f, 120.f), FVector(1.f, 0.f, 0.f), 300.f, 150.f, 0.f, FLinearColor::White, TEXT("plintus") },
			{ TEXT("T_Life_CoffeeRing"), FVector(-600.f, -1900.f, 1400.f), FVector(0.f, 0.f, 1.f), 180.f, 180.f, 20.f, FLinearColor::White, TEXT("plato") },
			{ TEXT("T_Life_CoffeeRing"), FVector(1500.f, -300.f, 1400.f), FVector(0.f, 0.f, 1.f), 170.f, 170.f, 80.f, FLinearColor(0.8f, 0.8f, 0.8f), TEXT("plato") },
			{ TEXT("T_Life_CoffeeRing"), FVector(13300.f, -2800.f, 980.f), FVector(0.f, 0.f, 1.f), 175.f, 175.f, 140.f, FLinearColor::White, TEXT("play") },
			{ TEXT("T_Life_Scratches"), FVector(9150.f, -5000.f, 300.f), FVector(1.f, 0.f, 0.f), 260.f, 260.f, 0.f, FLinearColor::White, TEXT("babushka") },
			{ TEXT("T_Life_Scratches"), FVector(10500.f, 400.f, 300.f), FVector(0.f, -1.f, 0.f), 240.f, 240.f, 0.f, FLinearColor::White, TEXT("play") },
		};
		for (const FDec& D : Decs)
		{
			// project onto what is really there: trace along -N from a little in front of the spot
			FHitResult H;
			FVector At = D.At;
			if (W->LineTraceSingleByChannel(H, D.At + D.N * 250.f, D.At - D.N * 400.f, ECC_Visibility)) { At = H.ImpactPoint; }
			UDecalComponent* Dc = NewObject<UDecalComponent>(this);
			Dc->SetupAttachment(RootComponent);
			UMaterialInstanceDynamic* Mi = UMaterialInstanceDynamic::Create(Dm, this);
			Mi->SetTextureParameterValue(TEXT("C"), LifeTex(D.Tex));
			Mi->SetVectorParameterValue(TEXT("Tint"), D.Tint);
			Dc->SetDecalMaterial(Mi);
			Dc->DecalSize = FVector(12.f, D.W * 0.5f, D.H * 0.5f);
			Dc->RegisterComponent();
			// a decal projects along its X: X into the surface, then the roll turns the print
			Dc->SetWorldLocationAndRotation(At, (FRotationMatrix::MakeFromX(-D.N).ToQuat() * FQuat(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians(D.Rot))));
			Dc->SetFadeScreenSize(0.002f);
			AddInstanceComponent(Dc);
			PropCounts.FindOrAdd(D.District)++;
			++Built;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Micro details at ant scale (instanced, seeded, on real surfaces)
// ---------------------------------------------------------------------------------------------

void AAntLifeWorld::BuildMicro()
{
	UWorld* W = GetWorld();
	struct FItem { const TCHAR* Key; const TCHAR* Prop; const TCHAR* Look; float Size; bool bSolid; const TCHAR* Print; };
	// Size: the longest side in uu (0: the model's own real size). 1 real cm = 19 uu.
	static const FItem Items[] = {
		{ TEXT("crumb"), TEXT("SM_M4_Crumb"), nullptr, 10.f, false, nullptr },
		{ TEXT("sugar"), TEXT("SM_M4_Speck"), TEXT("Sugar"), 5.f, false, nullptr },
		{ TEXT("rice"), TEXT("SM_M4_Rice"), nullptr, 12.f, false, nullptr },
		{ TEXT("pasta"), TEXT("SM_M4_Macaroni"), nullptr, 45.f, false, nullptr },
		{ TEXT("coin"), TEXT("SM_M4_Coin"), nullptr, 38.f, false, nullptr },
		{ TEXT("button"), TEXT("SM_M4_Button"), nullptr, 23.f, false, nullptr },
		{ TEXT("cap"), TEXT("SM_M4_BottleCap"), nullptr, 50.f, false, nullptr },
		{ TEXT("earring"), TEXT("SM_Life_Earring"), nullptr, 0.f, false, nullptr },
		{ TEXT("hairpin"), TEXT("SM_Life_HairPin"), nullptr, 0.f, false, nullptr },
		{ TEXT("brick_r"), TEXT("SM_Life_Brick"), TEXT("Red"), 0.f, true, nullptr },
		{ TEXT("brick_b"), TEXT("SM_Life_Brick"), TEXT("PlasticBlue"), 0.f, true, nullptr },
		{ TEXT("brick_y"), TEXT("SM_Life_Brick"), TEXT("Cheese"), 0.f, true, nullptr },
		{ TEXT("receipt"), TEXT("SM_Life_Receipt"), nullptr, 0.f, false, TEXT("T_Life_Receipt") },
		{ TEXT("price"), TEXT("SM_Life_Quad"), nullptr, 47.f, false, TEXT("T_Life_Price0") },
		{ TEXT("hair_dog"), TEXT("SM_Life_HairTuft"), TEXT("Biscuit"), 0.f, false, nullptr },
		{ TEXT("hair_cat"), TEXT("SM_Life_HairTuft"), TEXT("Dust"), 0.f, false, nullptr },
		{ TEXT("nail"), TEXT("SM_Life_NailClip"), nullptr, 0.f, false, nullptr },
		{ TEXT("spider"), TEXT("SM_Life_DeadSpider"), nullptr, 0.f, false, nullptr },
		{ TEXT("tie"), TEXT("SM_Life_CableTie"), nullptr, 0.f, false, nullptr },
		{ TEXT("gum"), TEXT("SM_Life_Gum"), nullptr, 0.f, false, nullptr },
		{ TEXT("shaving"), TEXT("SM_Life_Shaving"), nullptr, 0.f, false, nullptr },
		{ TEXT("star"), TEXT("SM_Life_Sticker"), nullptr, 0.f, false, nullptr },
		{ TEXT("dust"), TEXT("SM_M4_DustBunny"), nullptr, 70.f, false, nullptr },
		{ TEXT("popcorn"), TEXT("SM_M4_Popcorn"), nullptr, 38.f, false, nullptr },
		{ TEXT("pea"), TEXT("SM_M4_Pea"), nullptr, 15.f, false, nullptr },
		{ TEXT("crayon"), TEXT("SM_M4_Crayon"), nullptr, 160.f, false, nullptr },
		{ TEXT("thimble"), TEXT("SM_M4_Thimble"), nullptr, 34.f, false, nullptr },
		{ TEXT("pebble"), TEXT("SM_M4_Pebble"), nullptr, 30.f, false, nullptr },
	};
	auto Find = [](const TCHAR* Key) -> const FItem* { for (const FItem& I : Items) { if (FCString::Strcmp(I.Key, Key) == 0) { return &I; } } return nullptr; };
	// where: floor (under everything), tops (furniture, worktops); district rectangles (Map4.cpp) and what lies about in each
	struct FZone { const TCHAR* District; FBox2D R; bool bTops; const TCHAR* Items; int32 Count; };
	const FZone Zones[] = {
		{ TEXT("palisad"), FBox2D(FVector2D(-14800.f, -4800.f), FVector2D(-7600.f, 5200.f)), false, TEXT("cap coin tie pebble gum receipt crumb pebble"), 20 },
		{ TEXT("reddvor"), FBox2D(FVector2D(-9000.f, -8600.f), FVector2D(6800.f, -5400.f)), false, TEXT("pebble cap crumb tie coin pebble"), 18 },
		{ TEXT("plintus"), FBox2D(FVector2D(-6900.f, -4900.f), FVector2D(-2800.f, 2100.f)), false, TEXT("crumb sugar rice hair_dog button coin dust pea crumb"), 24 },
		{ TEXT("kladovye"), FBox2D(FVector2D(-6900.f, 2300.f), FVector2D(-4300.f, 4900.f)), true, TEXT("crumb sugar rice pasta price crumb"), 18 },
		{ TEXT("moika"), FBox2D(FVector2D(-4100.f, 2300.f), FVector2D(-1600.f, 4900.f)), true, TEXT("crumb rice tie coin hairpin sugar"), 16 },
		{ TEXT("plita"), FBox2D(FVector2D(-1400.f, 2300.f), FVector2D(-100.f, 4900.f)), true, TEXT("crumb sugar pasta rice"), 15 },
		{ TEXT("lednik"), FBox2D(FVector2D(100.f, 2300.f), FVector2D(4700.f, 4900.f)), false, TEXT("crumb pea sugar price coin hair_cat"), 18 },
		{ TEXT("plato"), FBox2D(FVector2D(-2600.f, -3300.f), FVector2D(4200.f, 2100.f)), true, TEXT("crumb sugar crumb pea button earring crumb shaving"), 26 },
		{ TEXT("plato"), FBox2D(FVector2D(-2600.f, -3300.f), FVector2D(4200.f, 2100.f)), false, TEXT("crumb pea crumb popcorn star shaving brick_r"), 22 },
		{ TEXT("svalka"), FBox2D(FVector2D(4400.f, -2400.f), FVector2D(6900.f, 2100.f)), false, TEXT("crumb cap tie receipt gum coin pea"), 20 },
		{ TEXT("porog"), FBox2D(FVector2D(5100.f, -4900.f), FVector2D(9900.f, 1400.f)), false, TEXT("pebble hair_dog dust coin earring button receipt"), 22 },
		{ TEXT("babushka"), FBox2D(FVector2D(7300.f, -8900.f), FVector2D(10300.f, -4800.f)), false, TEXT("thimble hairpin nail button hair_cat dust earring"), 20 },
		{ TEXT("play"), FBox2D(FVector2D(10100.f, -6900.f), FVector2D(18700.f, 1400.f)), false, TEXT("brick_r brick_b brick_y crayon shaving popcorn star coin dust hair_dog"), 34 },
		{ TEXT("stenka"), FBox2D(FVector2D(10500.f, -8900.f), FVector2D(20100.f, -7100.f)), false, TEXT("dust coin hair_cat button popcorn brick_b"), 18 },
	};
	FRandomStream Rng(LevelV3::Enabled() && LevelV3::Current() ? LevelV3::Current()->Seed * 131 + 7 : 7);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LifeMicro), true);
	int32 Placed = 0, Missed = 0;
	auto Place = [&](const FItem* It, const FVector& At, const FVector& N, float Yaw, const TCHAR* District, float ScaleK = 1.f)
	{
		if (!It) { return; }
		UInstancedStaticMeshComponent* I = Ism(It->Prop, It->Look, It->bSolid);
		if (!I) { return; }
		if (It->Print && I->GetNumMaterials() > 0 && !I->ComponentHasTag(TEXT("LifePrinted")))
		{
			if (UMaterialInstanceDynamic* M = Printed(LifeTex(It->Print), true)) { I->SetMaterial(0, M); }
			I->ComponentTags.Add(TEXT("LifePrinted"));
		}
		float S = 1.f;
		if (It->Size > 0.f)
		{
			const float L = I->GetStaticMesh()->GetBoundingBox().GetSize().GetMax();
			S = It->Size / FMath::Max(0.01f, L);
		}
		S *= ScaleK * Rng.FRandRange(0.85f, 1.15f);
		const FQuat Up = FQuat::FindBetweenNormals(FVector::UpVector, N);
		const FVector Sc = FCString::Strcmp(It->Prop, TEXT("SM_Life_Quad")) == 0 ? FVector(S, S * 0.6f, 1.f) : FVector(S);
		I->AddInstance(FTransform(Up * FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)), At + N * 0.3f, Sc), true);
		PropCounts.FindOrAdd(District)++;
		++Placed;
	};
	for (const FZone& Z : Zones)
	{
		TArray<FString> Keys;
		FString(Z.Items).ParseIntoArrayWS(Keys);
		for (int32 n = 0; n < Z.Count && Keys.Num(); ++n)
		{
			const FItem* It = Find(*Keys[n % Keys.Num()]);
			if (!It) { continue; }
			bool bOk = false;
			for (int32 Try = 0; Try < 6 && !bOk; ++Try)
			{
				FVector2D P(Rng.FRandRange(Z.R.Min.X, Z.R.Max.X), Rng.FRandRange(Z.R.Min.Y, Z.R.Max.Y));
				// things gather along the walls and under the furniture: half of them pulled towards the zone's nearest edge
				if (!Z.bTops && Rng.FRand() < 0.5f)
				{
					const float Dx0 = P.X - Z.R.Min.X, Dx1 = Z.R.Max.X - P.X, Dy0 = P.Y - Z.R.Min.Y, Dy1 = Z.R.Max.Y - P.Y;
					const float M = FMath::Min(FMath::Min(Dx0, Dx1), FMath::Min(Dy0, Dy1));
					const float Pull = Rng.FRandRange(40.f, 300.f);
					if (M == Dx0) { P.X = Z.R.Min.X + Pull; } else if (M == Dx1) { P.X = Z.R.Max.X - Pull; } else if (M == Dy0) { P.Y = Z.R.Min.Y + Pull; } else { P.Y = Z.R.Max.Y - Pull; }
				}
				const float FromZ = Z.bTops ? 4800.f : 260.f;
				FHitResult H;
				if (!W->LineTraceSingleByChannel(H, FVector(P.X, P.Y, FromZ), FVector(P.X, P.Y, -600.f), ECC_Visibility, Q)) { continue; }
				if (H.ImpactNormal.Z < 0.85f || IsLivingThing(H.GetActor())) { continue; }
				if (Z.bTops && H.ImpactPoint.Z < 300.f) { continue; }
				if (!Z.bTops && H.ImpactPoint.Z > 120.f) { continue; }
				Place(It, H.ImpactPoint, H.ImpactNormal, Rng.FRandRange(0.f, 360.f), Z.District);
				bOk = true;
			}
			if (!bOk) { ++Missed; }
		}
	}
	// ---- the story pieces, placed by hand
	auto Floor = [&](const FVector2D& P, float From, FVector& Out, FVector& N) -> bool
	{
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, FVector(P.X, P.Y, From), FVector(P.X, P.Y, -600.f), ECC_Visibility, Q) && H.ImpactNormal.Z > 0.7f && !IsLivingThing(H.GetActor()))
		{
			Out = H.ImpactPoint; N = H.ImpactNormal; return true;
		}
		return false;
	};
	FVector At, N;
	// the kid's snack trail: crumbs from the high chair towards the living room door
	for (int32 k = 0; k < 26; ++k)
	{
		const float T = k / 25.f;
		const FVector2D P = FMath::Lerp(FVector2D(3500.f, -700.f), FVector2D(6900.f, -3900.f), T) + FVector2D(FMath::Sin(T * 9.f) * 160.f, Rng.FRandRange(-60.f, 60.f));
		if (Floor(P, 200.f, At, N)) { Place(Find(k % 4 == 3 ? TEXT("popcorn") : TEXT("crumb")), At, N, Rng.FRandRange(0.f, 360.f), TEXT("plato"), k % 4 == 3 ? 0.6f : 1.f); }
	}
	// Бадрик's slobber by his bowl (a wet disc), kibble and a hair tuft
	if (Floor(FVector2D(-3500.f, -3600.f), 200.f, At, N))
	{
		if (UStaticMeshComponent* Pd = Mesh(TEXT("SM_M4_LiquidDisc"), FTransform(FRotator(0.f, 30.f, 0.f), At + FVector(0.f, 0.f, 0.4f), FVector(1.f)), TEXT("@Water")))
		{
			const float L = Pd->GetStaticMesh()->GetBoundingBox().GetSize().X;
			Pd->SetWorldScale3D(FVector(260.f / FMath::Max(1.f, L), 190.f / FMath::Max(1.f, L), 1.f));
			Pd->ComponentTags.Add(TEXT("M4Fx"));
			Pd->SetCastShadow(false);
			PropCounts.FindOrAdd(TEXT("plintus"))++;
		}
		Place(Find(TEXT("hair_dog")), At + FVector(180.f, -120.f, 0.f), N, 40.f, TEXT("plintus"));
	}
	// a dead spider in the corner under the kitchen window, with its web in the corner above it
	if (Floor(FVector2D(-6880.f, -4880.f), 200.f, At, N))
	{
		Place(Find(TEXT("spider")), At + FVector(60.f, 50.f, 0.f), N, 125.f, TEXT("plintus"));
		if (UStaticMeshComponent* Wb = Mesh(TEXT("SM_Life_Web"), FTransform(FRotator(0.f, 0.f, 0.f), FVector(-6995.f, -4995.f, 2.f), FVector(1.f)), TEXT("@Silk")))
		{
			Wb->ComponentTags.Add(TEXT("M4Thread"));
			Wb->SetCastShadow(false);
			PropCounts.FindOrAdd(TEXT("plintus"))++;
		}
	}
	// chewing gum stuck under the kitchen table (a trace up from the floor finds its underside)
	{
		FHitResult H;
		const FVector P(1300.f, -1200.f, 60.f);
		if (W->LineTraceSingleByChannel(H, P, P + FVector(0.f, 0.f, 1800.f), ECC_Visibility, Q) && H.ImpactNormal.Z < -0.7f && !IsLivingThing(H.GetActor()))
		{
			Place(Find(TEXT("gum")), H.ImpactPoint, H.ImpactNormal, 15.f, TEXT("plato"));
		}
	}
	// a receipt by the grocery bag, the kid's star stickers on the fridge's front
	if (Floor(FVector2D(5750.f, 2500.f), 200.f, At, N)) { Place(Find(TEXT("receipt")), At, N, 70.f, TEXT("svalka")); }
	for (int32 k = 0; k < 3; ++k)
	{
		const FVector P(2950.f + k * 230.f, 3070.f, 700.f + k * 150.f);
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, P + FVector(0.f, -300.f, 0.f), P + FVector(0.f, 300.f, 0.f), ECC_Visibility, Q) && !IsLivingThing(H.GetActor()))
		{
			Place(Find(TEXT("star")), H.ImpactPoint, H.ImpactNormal, k * 25.f, TEXT("lednik"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTLIFE micro details placed %d (missed %d)"), Placed, Missed);
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AAntLifeWorld::DrawSegments(UTextureRenderTarget2D* RT, const FString& Text, const FLinearColor& On, const FLinearColor& Bg)
{
	if (!RT) { return; }
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RT, Bg);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RT, Canvas, Size, Ctx);
	if (Canvas)
	{
		// seven segments: a top, b top-right, c bottom-right, d bottom, e bottom-left, f top-left, g middle
		static const uint8 Digits[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
		const int32 Lines = Text.Contains(TEXT("\n")) ? 2 : 1;
		TArray<FString> Rows;
		Text.ParseIntoArray(Rows, TEXT("\n"));
		const float RowH = Size.Y / FMath::Max(1, Rows.Num());
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const FString& Row = Rows[r];
			const float H = RowH * 0.72f, Wd = H * 0.5f, Th = H * 0.11f, Gap = Wd * 0.45f;
			float X = (Size.X - Row.Len() * (Wd + Gap)) * 0.5f;
			const float Y = r * RowH + RowH * 0.14f;
			for (TCHAR Ch : Row)
			{
				uint8 Mask = 0;
				if (Ch >= TEXT('0') && Ch <= TEXT('9')) { Mask = Digits[Ch - TEXT('0')]; }
				else if (Ch == TEXT('-')) { Mask = 0x40; }
				else if (Ch == TEXT('o')) { Mask = 0x63; }   // the degree sign: a small square up top
				const FVector2D Seg[7][2] = {
					{ FVector2D(X + Th, Y), FVector2D(Wd - 2 * Th, Th) }, { FVector2D(X + Wd - Th, Y + Th), FVector2D(Th, H / 2 - Th) }, { FVector2D(X + Wd - Th, Y + H / 2), FVector2D(Th, H / 2 - Th) },
					{ FVector2D(X + Th, Y + H - Th), FVector2D(Wd - 2 * Th, Th) }, { FVector2D(X, Y + H / 2), FVector2D(Th, H / 2 - Th) }, { FVector2D(X, Y + Th), FVector2D(Th, H / 2 - Th) },
					{ FVector2D(X + Th, Y + H / 2 - Th / 2), FVector2D(Wd - 2 * Th, Th) } };
				if (Ch == TEXT(':'))
				{
					Canvas->K2_DrawTexture(nullptr, FVector2D(X + Wd * 0.3f, Y + H * 0.28f), FVector2D(Th, Th), FVector2D::ZeroVector, FVector2D::UnitVector, On);
					Canvas->K2_DrawTexture(nullptr, FVector2D(X + Wd * 0.3f, Y + H * 0.66f), FVector2D(Th, Th), FVector2D::ZeroVector, FVector2D::UnitVector, On);
					X += Wd * 0.6f;
					continue;
				}
				const bool bDeg = Ch == TEXT('o');
				for (int32 s = 0; s < 7; ++s)
				{
					const bool bLit = (Mask >> s) & 1;
					FVector2D Pos = Seg[s][0], Sz = Seg[s][1];
					if (bDeg) { Pos = FVector2D(X, Y) + (Pos - FVector2D(X, Y)) * 0.45f; Sz *= 0.45f; }
					Canvas->K2_DrawTexture(nullptr, Pos, Sz, FVector2D::ZeroVector, FVector2D::UnitVector, bLit ? On : On * 0.06f);
				}
				X += (bDeg ? Wd * 0.5f : Wd) + Gap;
			}
		}
		(void)Lines;
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
}

void AAntLifeWorld::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!AntLife::Enabled()) { return; }
	UWorld* W = GetWorld();
	if (!bBuilt)
	{
		BuildRetry -= Dt;
		if (BuildRetry > 0.f) { return; }
		BuildRetry = 1.f;
		// the flat is built (its art is in) and, at level v3, the seed is known
		if (!FindArt(TEXT("SM_L_TV"), FVector(13150.f, -8700.f, 1100.f), 4000.f)) { return; }
		if (LevelV3::Enabled() && !LevelV3::Current() && W->GetTimeSeconds() < 20.f) { return; }
		Build();
	}
	const float Now = AntLife::Now(W);
	// the blend of day / evening / night follows the lights (6 s fades)
	const int32 T = AntLife::Tod();
	for (int32 k = 0; k < 3; ++k) { TodK[k] = FMath::FInterpConstantTo(TodK[k], k == T ? 1.f : 0.f, Dt, 1.f / 6.f); }
	TickOutside(Dt, Now, 0.f);
	TickRoom(Dt, Now);
	TickDisplays(Dt, Now);
}

void AAntLifeWorld::TickOutside(float Dt, float Now, float)
{
	UWorld* W = GetWorld();
	const int32 Wx = AntLife::Weather(W);
	const float Day = TodK[0], Eve = TodK[1], Night = TodK[2];
	if (CloudMid)
	{
		const FLinearColor Lit = FLinearColor(1.6f, 1.58f, 1.55f) * Day + FLinearColor(2.2f, 1.1f, 0.6f) * Eve + FLinearColor(0.06f, 0.07f, 0.12f) * Night;
		const FLinearColor Shade = FLinearColor(0.78f, 0.82f, 0.92f) * Day + FLinearColor(0.8f, 0.45f, 0.4f) * Eve + FLinearColor(0.02f, 0.025f, 0.05f) * Night;
		const float Dim = Wx == 1 ? 0.55f : Wx == 2 ? 0.8f : 1.f;
		CloudMid->SetVectorParameterValue(TEXT("Lit"), Lit * Dim);
		CloudMid->SetVectorParameterValue(TEXT("Shade"), Shade * Dim);
		CloudMid->SetScalarParameterValue(TEXT("Cover"), Wx == 1 ? 0.85f : Wx == 2 ? 0.75f : 0.42f);
		CloudMid->SetScalarParameterValue(TEXT("Opacity"), Wx ? 0.97f : 0.85f);
	}
	if (StarMid && Stars)
	{
		const float Op = Night * (Wx ? 0.15f : 1.f) + Eve * 0.12f;
		Stars->SetVisibility(Op > 0.01f);
		StarMid->SetScalarParameterValue(TEXT("Opacity"), Op);
	}
	for (UMaterialInstanceDynamic* M : FacadeMids)
	{
		M->SetScalarParameterValue(TEXT("Lit"), Day * 0.04f + Eve * 0.42f + Night * 0.5f);
		M->SetScalarParameterValue(TEXT("Glow"), Day * 1.f + Eve * 5.f + Night * 7.f);
	}
	// rain / snow
	static int32 LoggedWx = -9;
	if (Wx != LoggedWx)
	{
		LoggedWx = Wx;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE weather now %d on %s at server time %.1f"), Wx, W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), Now);
	}
	const bool bRain = Wx == 1, bSnow = Wx == 2;
	for (UStaticMeshComponent* P : RainPanes) { if (P) { P->SetVisibility(bRain); } }
	for (UMaterialInstanceDynamic* M : RainMids)
	{
		M->SetScalarParameterValue(TEXT("Rain"), bRain ? 1.f : 0.f);
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.82f, 0.9f) * (Day * 0.8f + Eve * 0.35f + Night * 0.06f));
	}
	if (RainLoop)
	{
		if (bRain && !RainLoop->IsPlaying()) { RainLoop->Play(); }
		if (!bRain && RainLoop->IsPlaying()) { RainLoop->Stop(); }
	}
	WeatherClock -= Dt;
	if ((bRain || bSnow) && WeatherClock <= 0.f)
	{
		WeatherClock = bRain ? 0.05f : 0.09f;
		if (AAntFxManager* Fx = AAntFxManager::Get(this))
		{
			const FVector Wins[] = { FVector(0.f, -5700.f, 0.f), FVector(18285.f, -9900.f, 0.f), FVector(-9000.f, 0.f, 0.f) };
			for (const FVector& Wn : Wins)
			{
				FAntFxBit B;
				B.Pos = Wn + FVector(FMath::FRandRange(-2600.f, 2600.f), FMath::FRandRange(-1600.f, 600.f), 6500.f);
				B.Look = EAntFxLook::Soft;
				B.Shape = EAntShape::Sphere;
				B.MinAmbient = 0.35f;
				if (bRain)
				{
					B.Vel = FVector(120.f, 0.f, -5200.f);
					B.Scale0 = B.Scale1 = FVector(0.03f, 0.03f, 1.3f);
					B.Color = FLinearColor(0.75f, 0.82f, 0.95f, 0.45f);
					B.Life = 1.3f;
					B.bStreak = true;
					B.FloorZ = 0.f;
				}
				else
				{
					B.Vel = FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-100.f, 100.f), -420.f);
					B.Scale0 = B.Scale1 = FVector(0.09f);
					B.Color = FLinearColor(1.f, 1.f, 1.f, 0.9f);
					B.Life = 14.f;
					B.Drag = 0.1f;
					B.FloorZ = 5.f;
				}
				Fx->Emit(B);
			}
		}
	}
	// swaying (trees, potted plants, pennants)
	const float Wind = bRain ? 1.8f : 1.f;
	for (const FSway& S : Sways)
	{
		if (USceneComponent* C = S.C.Get())
		{
			const float A = S.Amp * Wind * (FMath::Sin(Now * S.Speed + S.Phase) * 0.7f + FMath::Sin(Now * S.Speed * 2.3f + S.Phase * 1.7f) * 0.3f);
			C->SetWorldRotation(S.Base + FRotator(A * S.Axis.X, 0.f, A * S.Axis.Y) + FRotator(0.f, A * S.Axis.Z, 0.f));
		}
	}
	// birds
	for (FFlyer& F : Flyers)
	{
		UStaticMeshComponent* B = F.Body.Get();
		if (!B) { continue; }
		const float A = Now * F.Speed + F.Phase;
		const FVector P = F.Center + FVector(FMath::Cos(A) * F.Radii.X, FMath::Sin(A) * F.Radii.Y, FMath::Sin(A * 2.3f) * F.Radii.Z);
		const FVector V(-FMath::Sin(A) * F.Radii.X, FMath::Cos(A) * F.Radii.Y, 0.f);
		B->SetWorldLocationAndRotation(P, FRotator(0.f, V.Rotation().Yaw, -15.f));
		const float Flap = FMath::Sin(Now * 22.f + F.Phase) * 50.f;
		if (UStaticMeshComponent* L = F.WingL.Get()) { L->SetRelativeRotation(FRotator(0.f, 0.f, Flap)); }
		if (UStaticMeshComponent* R = F.WingR.Get()) { R->SetRelativeRotation(FRotator(0.f, 0.f, -Flap)); }
	}
	// the pigeon on the kitchen sill (the server pecks at what is under its beak; every machine sees the same bird)
	if (UStaticMeshComponent* Pg = Pigeon.Get())
	{
		FVector P; float Yaw = 0.f, Fly = 0.f; bool bPeck = false;
		const bool bHere = AntLife::PigeonAt(W, Now, P, Yaw, bPeck, Fly);
		if (bHere != Pg->IsVisible()) { Pg->SetVisibility(bHere, true); }
		if (bHere)
		{
			const float Bob = Fly > 0.f ? 0.f : FMath::Abs(FMath::Sin(Now * 6.f)) * 12.f;
			Pg->SetWorldLocationAndRotation(P + FVector(0.f, 0.f, 230.f + Bob), FRotator(bPeck ? -38.f : 0.f, Yaw, 0.f));
			const float Flap = Fly > 0.f ? FMath::Sin(Now * 18.f) * 60.f : 0.f;
			if (UStaticMeshComponent* L = PigeonWingL.Get()) { L->SetRelativeRotation(FRotator(0.f, 0.f, Flap)); }
			if (UStaticMeshComponent* R = PigeonWingR.Get()) { R->SetRelativeRotation(FRotator(0.f, 0.f, -Flap)); }
			if (PigeonPhase < 0.f)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTLIFE pigeon arrives on %s at server time %.1f"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), Now);
				if (USoundBase* S = Sfx(TEXT("A_Life_Flap"))) { UGameplayStatics::PlaySoundAtLocation(this, S, P, 0.8f); }
				bPigeonWarned = false;
			}
			if (Fly <= 0.f && !bPigeonWarned)
			{
				bPigeonWarned = true;
				if (USoundBase* S = Sfx(TEXT("A_Life_Pigeon"))) { UGameplayStatics::PlaySoundAtLocation(this, S, P, 1.f); }
				if (APlayerController* PC = W->GetFirstPlayerController())
				{
					AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn());
					if (Me && FVector::Dist(Me->GetActorLocation(), P) < 2500.f) { Me->Toast(TEXT("Голубь на подоконнике! Прячься — он клюёт муравьёв!")); }
				}
			}
			PigeonPhase = 1.f;
		}
		else { PigeonPhase = -1.f; }
	}
	// cars: a pass every ~40 s each way, headlights in the evening and at night
	for (FCar& C : Cars)
	{
		USceneComponent* R = C.Root.Get();
		if (!R || RoadA.IsZero()) { continue; }
		const float Period = 38.f;
		const float U = FMath::Frac(Now / Period + C.Offset);
		const float K = FMath::Clamp(U * 1.6f, 0.f, 1.f);   // drives for 60% of the period, then waits off-screen
		FVector From = C.Dir > 0.f ? RoadA : RoadB, To = C.Dir > 0.f ? RoadB : RoadA;
		const FVector Side = FVector::CrossProduct((To - From).GetSafeNormal(), FVector::UpVector) * C.Lane;
		const FVector P = FMath::Lerp(From, To, K) + Side;
		R->SetWorldLocationAndRotation(P, (To - From).Rotation());
		const float Lights = (Eve * 0.6f + Night) * 40.f;
		for (TWeakObjectPtr<USceneComponent>& L : C.Lights)
		{
			if (USpotLightComponent* S = Cast<USpotLightComponent>(L.Get()))
			{
				S->SetIntensity(Lights);
				S->SetVisibility(Lights > 0.5f && K < 1.f);
				S->SetWorldRotation(FRotator(-6.f, (To - From).Rotation().Yaw, 0.f));
			}
		}
	}
}

void AAntLifeWorld::TickRoom(float Dt, float Now)
{
	UWorld* W = GetWorld();
	const AAntLifeTV* Tv = AAntLifeTV::Get(W);
	// who is where (replicated tasks)
	bool bKettle = false, bCook = false, bPhone = false;
	AHumanCharacter* DadC = nullptr;
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		const int32 S = It->Task.Step == 255 ? -1 : (int32)It->Task.Step;
		if (S < 0 || It->Task.Phase != 2 || !NpcRoutine::Steps().IsValidIndex(S)) { if (It->Fig == HumanFig::Dad && !It->IsA<AHouseFolk>()) { DadC = *It; } continue; }
		const FString Key = NpcRoutine::Steps()[S].Key;
		bKettle |= Key == TEXT("g_kettle");
		bCook |= Key == TEXT("h_cook");
		if (It->Fig == HumanFig::Dad && !It->IsA<AHouseFolk>()) { DadC = *It; bPhone |= Key == TEXT("h_phone"); }
	}
	const bool bWash = Tv && Tv->WashStart > 0.f && (Now - Tv->WashStart < 75.f || Now - Tv->WashStart < 100000.f && Tv->bWashForced);
	const int32 Tod = AntLife::Tod();
	// steam
	for (FSteam& S : Steams)
	{
		const bool bOn = S.When == 0 || (S.When == 1 && bKettle) || (S.When == 2 && (bCook || Tod == 1)) || (S.When == 3 && bWash && FMath::Frac(Now / 9.f) < 0.3f);
		if (!bOn) { continue; }
		S.Clock -= Dt;
		if (S.Clock <= 0.f)
		{
			S.Clock = S.Every * FMath::FRandRange(0.7f, 1.3f);
			AntFx::Steam(this, S.At + FVector(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-8.f, 8.f), 0.f), S.Size, 180.f);
		}
	}
	if (KettleLoop) { if (bKettle != KettleLoop->IsPlaying()) { bKettle ? KettleLoop->Play() : KettleLoop->Stop(); } }
	// the dishwasher: hum, the small things on top of it creep
	if (WashLoop) { if (bWash != WashLoop->IsPlaying()) { bWash ? WashLoop->Play() : WashLoop->Stop(); } }
	for (TPair<TWeakObjectPtr<USceneComponent>, FVector>& Sh : Shakers)
	{
		if (USceneComponent* C = Sh.Key.Get())
		{
			if (bWash)
			{
				FVector P = C->GetComponentLocation();
				const FVector Jit(FMath::FRandRange(-1.6f, 1.6f), FMath::FRandRange(-1.6f, 1.6f), FMath::FRandRange(0.f, 0.8f));
				FVector Drift = (P - Sh.Value) + FVector(FMath::FRandRange(-0.25f, 0.35f), FMath::FRandRange(-0.25f, 0.3f), 0.f) * Dt * 20.f;
				Drift = Drift.GetClampedToMaxSize2D(140.f);
				Drift.Z = 0.f;
				C->SetWorldLocation(Sh.Value + Drift + Jit);
				C->AddWorldRotation(FRotator(0.f, FMath::FRandRange(-3.f, 3.f), 0.f));
			}
		}
	}
	// the dripping tap
	if (!FaucetTip.IsZero())
	{
		DripClock -= Dt;
		if (DripClock <= 0.f)
		{
			DripClock = 2.4f;
			AntFx::Droplets(this, FaucetTip, 1, 4.f);
			if (USoundBase* S = Sfx(TEXT("A_Life_Drip"))) { UGameplayStatics::PlaySoundAtLocation(this, S, FaucetTip - FVector(0.f, 0.f, 500.f), 0.35f, FMath::FRandRange(0.9f, 1.1f)); }
		}
	}
	// the clock: the flat's time (a game minute per second of the raid), the second hand ticks
	const float Hours = AntLife::ClockHours(W);
	const float Secs = FMath::Fmod(Now, 60.f);
	const float Ang[3] = { FMath::Fmod(Hours, 12.f) / 12.f * 360.f, FMath::Frac(Hours) * 360.f, FMath::FloorToFloat(Secs) * 6.f };
	for (int32 h = 0; h < 3; ++h)
	{
		if (UStaticMeshComponent* C = ClockHands[h].Get())
		{
			C->SetRelativeRotation(FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(Ang[h])) * FQuat(FVector::UpVector, PI));
		}
	}
	// flies and the moth
	const bool bNight = Tod == 2 || TodK[2] > 0.5f;
	for (FBuzz& Z : Buzzers)
	{
		UStaticMeshComponent* B = Z.Body.Get();
		if (!B) { continue; }
		if (Z.bMoth && B->IsVisible() != bNight) { B->SetVisibility(bNight, true); }
		if (Z.bMoth && !bNight) { continue; }
		const float A = Now * Z.Speed + Z.Phase;
		const FVector P = Z.Center + FVector(FMath::Sin(A * 1.3f) * Z.R, FMath::Cos(A * 0.9f) * Z.R, FMath::Sin(A * 2.1f) * Z.R * 0.35f)
			+ FVector(FMath::PerlinNoise1D(A * 3.f), FMath::PerlinNoise1D(A * 3.f + 17.f), FMath::PerlinNoise1D(A * 3.f + 41.f)) * (Z.bMoth ? 90.f : 40.f);
		const FVector V = P - B->GetComponentLocation();
		B->SetWorldLocationAndRotation(P, V.IsNearlyZero() ? B->GetComponentRotation() : FRotator(0.f, V.Rotation().Yaw, 0.f));
		const float Flap = FMath::Sin(Now * (Z.bMoth ? 30.f : 90.f) + Z.Phase) * (Z.bMoth ? 50.f : 35.f);
		if (UStaticMeshComponent* L = Z.WingL.Get()) { L->SetRelativeRotation(FRotator(0.f, 0.f, Flap)); }
		if (UStaticMeshComponent* R = Z.WingR.Get()) { R->SetRelativeRotation(FRotator(0.f, 0.f, -Flap)); }
		if (Z.bMoth && FMath::Frac(A * 0.21f) < Dt * Z.Speed * 0.21f)
		{
			if (USoundBase* S = Sfx(TEXT("A_Life_MothTap"))) { UGameplayStatics::PlaySoundAtLocation(this, S, P, 0.5f); }
		}
	}
	if (FlyLoop && !FlyLoop->IsPlaying()) { FlyLoop->Play(); }
	// Бадрик asleep: snore bubbles at his nose
	SnoreClock -= Dt;
	if (SnoreClock <= 0.f)
	{
		SnoreClock = 2.6f;
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == 0 && It->State == 0 && It->HasJointedBody())
			{
				const FVector Nose = It->GetNose();
				if (AAntFxManager* Fx = AAntFxManager::Get(this))
				{
					FAntFxBit B;
					B.Pos = Nose + FVector(0.f, 0.f, 10.f);
					B.Vel = FVector(0.f, 0.f, 14.f);
					B.Scale0 = FVector(0.12f); B.Scale1 = FVector(0.75f);
					B.Color = FLinearColor(0.85f, 0.92f, 1.f, 0.35f);
					B.Look = EAntFxLook::Soft;
					B.Life = 2.2f;
					B.FadePow = 0.4f;
					B.MinAmbient = 0.5f;
					Fx->Emit(B);
				}
				AntAudio::Play(this, EAntSfx::DogSnort, Nose, 0.45f);
			}
		}
	}
	// the phone on the sofa: a buzz every ~35 s (seeded clock), it creeps, the screen lights up
	if (UStaticMeshComponent* Ph = Phone.Get())
	{
		const float Cyc = FMath::Fmod(Now + 11.f, 35.f);
		if (Cyc < 1.5f)
		{
			if (PhoneBuzzAt < 0.f || Now - PhoneBuzzAt > 5.f)
			{
				PhoneBuzzAt = Now;
				if (USoundBase* S = Sfx(TEXT("A_Life_PhoneBuzz"))) { UGameplayStatics::PlaySoundAtLocation(this, S, Ph->GetComponentLocation(), 0.9f); }
			}
			FVector Off = Ph->GetComponentLocation() - PhoneHome.GetLocation();
			Off += FVector(FMath::FRandRange(-0.6f, 1.f), FMath::FRandRange(-0.6f, 1.f), 0.f) * Dt * 30.f;
			Off = Off.GetClampedToMaxSize2D(60.f); Off.Z = 0.f;
			Ph->SetWorldLocation(PhoneHome.GetLocation() + Off + FVector(FMath::FRandRange(-1.2f, 1.2f), FMath::FRandRange(-1.2f, 1.2f), FMath::FRandRange(0.f, 0.6f)));
		}
		if (PhoneMid) { PhoneMid->SetScalarParameterValue(TEXT("Power"), Cyc < 6.f ? 1.f : 0.f); }
	}
	// the Dad's own phone in his hand: its screen glows on his face, the feed scrolls
	if (DadC)
	{
		if (!DadPhoneHost.IsValid())
		{
			TInlineComponentArray<UStaticMeshComponent*> Cs(DadC);
			for (UStaticMeshComponent* C : Cs)
			{
				if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName() == TEXT("SM_M4_Phone")) { DadPhoneHost = C; break; }
			}
			if (UStaticMeshComponent* Host = DadPhoneHost.Get())
			{
				if (UStaticMeshComponent* Scr = Mesh(TEXT("SM_Life_Quad"), FTransform::Identity, nullptr, false, Host))
				{
					Scr->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 52.4f), FVector(0.86f, 0.84f, 1.f)));
					if (UMaterialInterface* M = LifeMat(TEXT("M_Life_Display")))
					{
						DadPhoneMid = UMaterialInstanceDynamic::Create(M, this);
						DadPhoneMid->SetTextureParameterValue(TEXT("Tex"), LifeTex(TEXT("T_Life_PhoneFeed")));
						DadPhoneMid->SetScalarParameterValue(TEXT("TileV"), 0.3f);
						DadPhoneMid->SetScalarParameterValue(TEXT("Scroll"), 0.05f);
						Scr->SetMaterial(0, DadPhoneMid);
					}
					Scr->SetCastShadow(false);
					Scr->ComponentTags.Add(TEXT("M4Skin"));
					DadPhoneScreen = Scr;
				}
				DadPhoneLight = NewObject<UPointLightComponent>(this);
				DadPhoneLight->SetupAttachment(Host);
				DadPhoneLight->SetMobility(EComponentMobility::Movable);
				DadPhoneLight->SetIntensityUnits(ELightUnits::Candelas);
				DadPhoneLight->SetIntensity(0.f);
				DadPhoneLight->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
				DadPhoneLight->SetAttenuationRadius(1100.f);
				DadPhoneLight->SetCastShadows(false);
				DadPhoneLight->RegisterComponent();
				DadPhoneLight->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
				AddInstanceComponent(DadPhoneLight);
			}
		}
		if (UStaticMeshComponent* Host = DadPhoneHost.Get())
		{
			const bool bShow = bPhone && Host->IsVisible();
			if (UStaticMeshComponent* Scr = DadPhoneScreen.Get()) { Scr->SetVisibility(bShow); }
			if (DadPhoneLight) { DadPhoneLight->SetIntensity(bShow ? 1.6f : 0.f); DadPhoneLight->SetVisibility(bShow); }
		}
	}
	// the radio (by day), the music box (evening and night), the music box's star turns
	const bool bListen = [&]() { APlayerController* PC = W->GetFirstPlayerController(); return PC && PC->PlayerCameraManager; }();
	if (RadioLoop) { const bool bOn = Tod == 0 && bListen; if (bOn != RadioLoop->IsPlaying()) { bOn ? RadioLoop->Play() : RadioLoop->Stop(); } }
	if (MusicBoxLoop) { const bool bOn = Tod != 0 && bListen; if (bOn != MusicBoxLoop->IsPlaying()) { bOn ? MusicBoxLoop->Play() : MusicBoxLoop->Stop(); } }
	if (UStaticMeshComponent* St = MusicStar.Get()) { if (Tod != 0) { St->AddRelativeRotation(FRotator(0.f, 40.f * Dt, 0.f)); } }
	// the kid's tablet: a little player of its own, awake only near it
	if (TabletPlayer && TabletSource)
	{
		bool bNear = false;
		if (APlayerController* PC = W->GetFirstPlayerController()) { if (PC->PlayerCameraManager) { bNear = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), TabletAt) < 4500.f; } }
		if (bNear && !bTabletOpen) { TabletPlayer->OpenSource(TabletSource); bTabletOpen = true; UE_LOG(LogTemp, Display, TEXT("ANTLIFE tablet wakes")); }
		if (!bNear && bTabletOpen) { TabletPlayer->Close(); bTabletOpen = false; }
	}
}

void AAntLifeWorld::TickDisplays(float Dt, float Now)
{
	DisplayClock -= Dt;
	if (DisplayClock > 0.f) { return; }
	DisplayClock = 0.5f;
	const float H = AntLife::ClockHours(GetWorld());
	const int32 Hh = FMath::FloorToInt(H), Mm = FMath::FloorToInt(FMath::Frac(H) * 60.f);
	const bool bColon = FMath::Frac(Now) < 0.5f;
	const FString Mw = FString::Printf(TEXT("%02d%s%02d"), Hh, bColon ? TEXT(":") : TEXT(" "), Mm);
	if (MicrowaveRT && Mw != MicrowaveShown)
	{
		MicrowaveShown = Mw;
		DrawSegments(MicrowaveRT, Mw, FLinearColor(0.25f, 1.f, 0.45f), FLinearColor(0.01f, 0.015f, 0.012f));
	}
	const FString Fr = TEXT("4o\n-18o");
	if (FridgeRT && Fr != FridgeShown)
	{
		FridgeShown = Fr;
		DrawSegments(FridgeRT, Fr, FLinearColor(0.35f, 0.75f, 1.f), FLinearColor(0.01f, 0.012f, 0.02f));
	}
}

FString AAntLifeWorld::Info() const
{
	int32 Inst = 0;
	for (const UInstancedStaticMeshComponent* I : Isms) { Inst += I ? I->GetInstanceCount() : 0; }
	return FString::Printf(TEXT("built %d parts, %d micro in %d sets, %d flyers, %d buzzers, %d cars, %d steams, weather %d, tod %d"),
		Built, Inst, Isms.Num(), Flyers.Num(), Buzzers.Num(), Cars.Num(), Steams.Num(), AntLife::Weather(GetWorld()), AntLife::Tod());
}

// =============================================================================================
// Dev commands
// =============================================================================================

bool AntLife::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0) { return false; }
	UWorld* W = GM->GetWorld();
	const FString Op = A[0].ToLower();
	const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	if (Op == TEXT("tv"))
	{
		AAntLifeTV* Tv = AAntLifeTV::Get(W);
		if (!Tv) { UE_LOG(LogTemp, Warning, TEXT("ANTLIFE no TV (map v4 look off or -AntNoLife)")); return true; }
		if (Sub == TEXT("on")) { Tv->Hold = 1; Tv->SetState(true, A.IsValidIndex(2) ? FCString::Atoi(*A[2]) : Tv->Channel, FMath::Max<int32>(Tv->Volume, 4), TEXT("admin")); }
		else if (Sub == TEXT("off")) { Tv->Hold = 2; Tv->SetState(false, Tv->Channel, Tv->Volume, TEXT("admin")); }
		else if (Sub == TEXT("auto")) { Tv->Hold = 0; UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv back to the residents' routine")); }
		else if ((Sub == TEXT("ch") || Sub == TEXT("channel")) && A.IsValidIndex(2)) { Tv->Hold = 1; Tv->SetState(true, FCString::Atoi(*A[2]), Tv->Volume, TEXT("admin")); }
		else if (Sub == TEXT("next")) { Tv->Hold = 1; Tv->SetState(true, Tv->Channel + 1, Tv->Volume, TEXT("admin")); }
		else if ((Sub == TEXT("vol") || Sub == TEXT("volume")) && A.IsValidIndex(2)) { Tv->SetState(Tv->bOn, Tv->Channel, FCString::Atoi(*A[2]), TEXT("admin")); }
		else if (Sub == TEXT("press") && A.IsValidIndex(2)) { Tv->Press(FCString::Atoi(*A[2]), nullptr); }
		else
		{
			UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv info: %s ch %d «%s» vol %d hold %d pos %.1f watchers %d screen %s remote keys %s %s %s"), Tv->bOn ? TEXT("ON") : TEXT("off"), Tv->Channel,
				AntLife::Channels().IsValidIndex(Tv->Channel) ? AntLife::Channels()[Tv->Channel].Ru : TEXT("?"), Tv->Volume, Tv->Hold, Tv->PlayPos(), Tv->Watchers(),
				*Tv->ScreenCenter().ToCompactString(), *Tv->ButtonAt(0).ToCompactString(), *Tv->ButtonAt(1).ToCompactString(), *Tv->ButtonAt(2).ToCompactString());
		}
		return true;
	}
	if (Op == TEXT("life"))
	{
		if (Sub == TEXT("vacuum"))
		{
			const FString V = A.IsValidIndex(2) ? A[2].ToLower() : FString();
			if (V == TEXT("stop")) { StopVacuum(W); }
			else if (V == TEXT("every") && A.IsValidIndex(3)) { SetVacuumEvery(W, FCString::Atof(*A[3])); }
			else { StartVacuum(W, V.IsNumeric() ? FCString::Atof(*V) : 70.f); }
		}
		else if (Sub == TEXT("wash")) { (A.IsValidIndex(2) && A[2].ToLower() == TEXT("stop")) ? StopWash(W) : StartWash(W); }
		else if (Sub == TEXT("weather")) { SetWeather(W, A.IsValidIndex(2) && A[2].ToLower() != TEXT("auto") ? FCString::Atoi(*A[2]) : -1); }
		else if (Sub == TEXT("pigeon"))
		{
			FVector P; float Y, F; bool bP;
			for (int32 s = 0; s < 150; ++s) { if (PigeonAt(W, Now(W) + s, P, Y, bP, F)) { UE_LOG(LogTemp, Display, TEXT("ANTLIFE pigeon lands in %d s at %s"), s, *P.ToCompactString()); break; } }
		}
		else
		{
			const AAntLifeWorld* L = AAntLifeWorld::Get(W);
			UE_LOG(LogTemp, Display, TEXT("ANTLIFE info: %s; clock %.2f h"), L ? *L->Info() : TEXT("(no life actor here)"), ClockHours(W));
			if (L) { for (const TPair<FString, int32>& P : L->PropCounts) { UE_LOG(LogTemp, Display, TEXT("ANTLIFE details %s %d"), *P.Key, P.Value); } }
		}
		return true;
	}
	return false;
}
