#include "AntFx.h"
#include "Map4Look.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const FName FxColorName(TEXT("Color"));

	float Rnd(float A, float B) { return FMath::FRandRange(A, B); }

	/** The manager, if the spot is close enough to the local camera to be worth drawing. */
	AAntFxManager* For(const UObject* Ctx, const FVector& At, float MaxDist)
	{
		AAntFxManager* M = AAntFxManager::Get(Ctx);
		if (!M)
		{
			return nullptr;
		}
		if (const APlayerController* PC = M->GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), At) > FMath::Square(MaxDist))
			{
				return nullptr;
			}
		}
		return M;
	}

	const FLinearColor DustColor(0.66f, 0.60f, 0.50f, 1.f);   // big clouds (the slipper)
	const FLinearColor FineDust(0.52f, 0.46f, 0.37f, 1.f);    // ant-sized puffs: a mid tone, so they show on pale tile and on dark wood
}

AAntFxManager::AAntFxManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AAntFxManager* AAntFxManager::Get(const UObject* Ctx)
{
	UWorld* World = (GEngine && Ctx) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer || World->bIsTearingDown)
	{
		return nullptr;
	}
	// A weak pointer, not a cached asset: after the menu reloads the map it is simply stale and a new manager is made.
	static TWeakObjectPtr<AAntFxManager> Cached;
	if (AAntFxManager* M = Cached.Get())
	{
		if (M->GetWorld() == World)
		{
			return M;
		}
	}
	for (TActorIterator<AAntFxManager> It(World); It; ++It)
	{
		Cached = *It;
		return *It;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAntFxManager* M = World->SpawnActor<AAntFxManager>(AAntFxManager::StaticClass(), FTransform::Identity, P);
	Cached = M;
	return M;
}

UMaterialInterface* AAntFxManager::BaseFor(EAntFxLook Look)
{
	// Held by this actor's UPROPERTYs and loaded again by the next manager: nothing static survives a map reload.
	if (!SolidBase)
	{
		SolidBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Map4/Materials/MI_M4_Paint.MI_M4_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet);   // map v4: painted PBR with a "Color"
		if (!SolidBase || FParse::Param(FCommandLine::Get(), TEXT("AntMap3"))) { SolidBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
		SoftBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
		GlowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		UE_LOG(LogTemp, Display, TEXT("AntFx: materials solid=%d soft=%d glow=%d"), SolidBase ? 1 : 0, SoftBase ? 1 : 0, GlowBase ? 1 : 0);
		if (!SoftBase) { SoftBase = SolidBase; }
		if (!GlowBase) { GlowBase = SolidBase; }
		FlameBase = AntFx::CardMaterial(EAntFxLook::Flame);
		SmokeBase = AntFx::CardMaterial(EAntFxLook::Smoke);
		UE_LOG(LogTemp, Display, TEXT("AntFx: cards flame=%d smoke=%d mesh=%d"), FlameBase ? 1 : 0, SmokeBase ? 1 : 0, AntFx::CardMesh() ? 1 : 0);
		if (!FlameBase) { FlameBase = GlowBase; }
		if (!SmokeBase) { SmokeBase = SoftBase; }
	}
	switch (Look)
	{
	case EAntFxLook::Soft: return SoftBase;
	case EAntFxLook::Glow: return GlowBase;
	case EAntFxLook::Flame: return FlameBase;
	case EAntFxLook::Smoke: return SmokeBase;
	default: return SolidBase;
	}
}

void AAntFxManager::Emit(const FAntFxBit& Bit)
{
	if (Bit.Life <= 0.f)
	{
		return;
	}
	int32 Slot = INDEX_NONE;
	for (int32 n = 0; n < Live.Num(); ++n)
	{
		const int32 i = (Cursor + n) % Live.Num();
		if (!Live[i].bOn)
		{
			Slot = i;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		if (Live.Num() < MaxBits)
		{
			Slot = Live.AddDefaulted();
			Comps.Add(nullptr);
			Mids.AddZeroed(AntFxLooks);
		}
		else
		{
			Slot = Cursor % Live.Num();   // full: the bit that has been around longest gives way
			for (int32 n = 0; n < 64 && Live[Slot].B.bKeep; ++n) { Slot = (Slot + 1) % Live.Num(); }   // ...unless it is one to keep
		}
	}
	Cursor = (Slot + 1) % FMath::Max(1, Live.Num());

	UStaticMeshComponent* C = Comps[Slot];
	if (!C)
	{
		C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->bReceivesDecals = false;
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		Comps[Slot] = C;
	}
	UStaticMesh* Mesh = AntKit::ShapeMesh(Bit.Shape);
	const bool bCard = Bit.Look == EAntFxLook::Flame || Bit.Look == EAntFxLook::Smoke;
	if (bCard && AntFx::CardMesh())
	{
		Mesh = AntFx::CardMesh();
		C->ComponentTags.AddUnique(TEXT("M4Fx"));
	}
	else if (Map4Look::Enabled())
	{
		// map v4: puffs are clouds, solid bits are drops (bluish) or chips, flecks are flakes; rings and beams stay cylinders (glow)
		const bool bBlue = Bit.Color.B > Bit.Color.R + 0.08f;
		const int32 Want = Bit.Shape == EAntShape::Sphere ? (Bit.Look == EAntFxLook::Soft ? -1 : (bBlue || Bit.Look == EAntFxLook::Glow ? 5 : 7))
			: Bit.Shape == EAntShape::Cube ? 6 : -2;
		UStaticMesh* M4 = Want == -1 ? Map4Look::Prop(TEXT("SM_M4_Puff")) : Want >= 0 ? Map4Look::Shape(Want) : nullptr;
		if (M4) { Mesh = M4; }
		C->ComponentTags.AddUnique(TEXT("M4Fx"));
	}
	if (C->GetStaticMesh() != Mesh)
	{
		C->SetStaticMesh(Mesh);
	}
	const int32 MidIndex = Slot * AntFxLooks + (int32)Bit.Look;
	if (!Mids[MidIndex])
	{
		if (UMaterialInterface* Base = BaseFor(Bit.Look))
		{
			Mids[MidIndex] = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	if (Mids[MidIndex] && C->GetMaterial(0) != Mids[MidIndex])
	{
		C->SetMaterial(0, Mids[MidIndex]);
	}
	if (Bit.Look == EAntFxLook::Solid && Mids[MidIndex])
	{
		Mids[MidIndex]->SetVectorParameterValue(FxColorName, FLinearColor(Bit.Color.R, Bit.Color.G, Bit.Color.B, 1.f));
	}
	if (bCard && Mids[MidIndex])
	{
		Mids[MidIndex]->SetScalarParameterValue(TEXT("Seed"), Bit.Seed >= 0.f ? Bit.Seed : FMath::FRand() * 100.f);
	}

	FLive& L = Live[Slot];
	if (!L.bOn)
	{
		++NumOn;
	}
	L.B = Bit;
	L.Age = 0.f;
	L.bOn = true;
	Apply(Slot);
	C->SetVisibility(true);
}

void AAntFxManager::Apply(int32 Slot)
{
	const FLive& L = Live[Slot];
	const FAntFxBit& B = L.B;
	UStaticMeshComponent* C = Comps[Slot];
	if (!C)
	{
		return;
	}
	const float T = FMath::Clamp(L.Age / B.Life, 0.f, 1.f);
	const float Grow = 1.f - FMath::Square(1.f - T);   // fast at first, then it settles
	float A = FMath::Pow(1.f - T, B.FadePow);
	if (B.FadeIn > 0.f)
	{
		A *= FMath::Min(1.f, T / B.FadeIn);
	}
	FVector Scale = FMath::Lerp(B.Scale0, B.Scale1, Grow);
	UMaterialInstanceDynamic* M = Mids[Slot * AntFxLooks + (int32)B.Look];
	if (B.Look == EAntFxLook::Solid)
	{
		Scale *= FMath::Min(1.f, (1.f - T) * 5.f);   // opaque bits can't fade: they shrink away at the end
	}
	else if (M)
	{
		if (B.Look == EAntFxLook::Soft || B.Look == EAntFxLook::Smoke)
		{
			const float Amb = FMath::Max(Ambient, B.MinAmbient);
			M->SetVectorParameterValue(FxColorName, FLinearColor(B.Color.R * Amb * AmbientTint.R, B.Color.G * Amb * AmbientTint.G, B.Color.B * Amb * AmbientTint.B, B.Color.A * A));
		}
		else
		{
			M->SetVectorParameterValue(FxColorName, FLinearColor(B.Color.R * A, B.Color.G * A, B.Color.B * A, A));
		}
	}
	FQuat Rot = B.Rot;
	if (B.bStreak && !B.Vel.IsNearlyZero())
	{
		Rot = FRotationMatrix::MakeFromX(B.Vel).ToQuat();
	}
	else if (B.Look == EAntFxLook::Flame || B.Look == EAntFxLook::Smoke)
	{
		Rot = AntFx::CardRot(B.Pos, CamLoc, B.Look == EAntFxLook::Flame);
		Scale.Z = 1.f;
	}
	C->SetWorldTransform(FTransform(Rot, B.Pos, Scale.ComponentMax(FVector(0.0005f))));
}

void AAntFxManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (NumOn <= 0)
	{
		return;
	}
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) { CamLoc = PC->PlayerCameraManager->GetCameraLocation(); }
	}
	for (int32 i = 0; i < Live.Num(); ++i)
	{
		FLive& L = Live[i];
		if (!L.bOn)
		{
			continue;
		}
		L.Age += DeltaSeconds;
		if (L.Age >= L.B.Life)
		{
			L.bOn = false;
			--NumOn;
			if (Comps[i]) { Comps[i]->SetVisibility(false); }
			continue;
		}
		FAntFxBit& B = L.B;
		B.Vel.Z -= B.Gravity * DeltaSeconds;
		if (B.Drag > 0.f)
		{
			B.Vel *= FMath::Exp(-B.Drag * DeltaSeconds);
		}
		B.Pos += B.Vel * DeltaSeconds;
		if (B.Pos.Z < B.FloorZ)
		{
			B.Pos.Z = B.FloorZ;
			B.Vel = FVector::ZeroVector;
			B.Gravity = 0.f;
		}
		Apply(i);
	}
}

// =============================================================================================
// The effects
// =============================================================================================

namespace AntFx
{

void StompDust(const UObject* Ctx, const FVector& At)
{
	AAntFxManager* M = For(Ctx, At, 30000.f);
	if (!M)
	{
		return;
	}
	const FVector Floor = At + FVector(0.f, 0.f, 4.f);
	// The shock ring racing out along the floor.
	FAntFxBit Ring;
	Ring.Shape = EAntShape::Cylinder;
	Ring.Pos = Floor;
	Ring.Scale0 = FVector(7.f, 7.f, 0.03f);
	Ring.Scale1 = FVector(22.f, 22.f, 0.03f);
	Ring.Color = FLinearColor(DustColor.R, DustColor.G, DustColor.B, 0.10f);
	Ring.Life = 0.75f;
	Ring.FadeIn = 0.05f;
	Ring.FadePow = 1.4f;
	M->Emit(Ring);
	// Dust rolling away from the slipper: small low puffs that never hide what is going on (an ant's camera sits right in them).
	const int32 N = 10;
	for (int32 i = 0; i < N; ++i)
	{
		const float Ang = (i + Rnd(-0.3f, 0.3f)) * 2.f * PI / N;
		const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		FAntFxBit P;
		P.Pos = Floor + Dir * Rnd(300.f, 430.f) + FVector(0.f, 0.f, Rnd(20.f, 80.f));
		P.Vel = Dir * Rnd(1300.f, 2100.f) + FVector(0.f, 0.f, Rnd(120.f, 420.f));
		P.Drag = 3.4f;
		const float S1 = Rnd(1.3f, 2.2f);
		P.Scale0 = FVector(Rnd(0.5f, 0.8f));
		P.Scale1 = FVector(S1, S1, S1 * 0.55f);
		const float Shade = Rnd(0.85f, 1.05f);
		P.Color = FLinearColor(DustColor.R * Shade, DustColor.G * Shade, DustColor.B * Shade, Rnd(0.09f, 0.15f));
		P.Life = Rnd(0.7f, 1.2f);
		P.FadeIn = 0.1f;
		P.FadePow = 1.3f;
		M->Emit(P);
	}
	// Grit knocked into the air.
	for (int32 i = 0; i < 10; ++i)
	{
		const float Ang = Rnd(0.f, 2.f * PI);
		const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		FAntFxBit G;
		G.Shape = EAntShape::Cube;
		G.Look = EAntFxLook::Solid;
		G.Pos = Floor + Dir * Rnd(100.f, 450.f);
		G.Vel = Dir * Rnd(300.f, 1000.f) + FVector(0.f, 0.f, Rnd(500.f, 1300.f));
		G.Gravity = 2600.f;
		G.Scale0 = G.Scale1 = FVector(Rnd(0.06f, 0.16f));
		G.Rot = FQuat(FMath::VRand(), Rnd(0.f, 6.f));
		const float Shade = Rnd(0.7f, 1.1f);
		G.Color = FLinearColor(0.45f * Shade, 0.40f * Shade, 0.33f * Shade, 1.f);
		G.Life = Rnd(1.f, 1.6f);
		G.FloorZ = Floor.Z + 3.f;
		M->Emit(G);
	}
}

void Droplets(const UObject* Ctx, const FVector& At, int32 Count, float Size)
{
	AAntFxManager* M = For(Ctx, At, 6000.f + Size * 500.f);
	if (!M)
	{
		return;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		FAntFxBit D;
		D.Pos = At + FVector(Rnd(-0.6f, 0.6f) * Size, Rnd(-0.6f, 0.6f) * Size, Size * 0.3f);
		D.Vel = FVector(Rnd(-14.f, 14.f) * Size, Rnd(-14.f, 14.f) * Size, Rnd(22.f, 48.f) * Size);
		D.Gravity = 1500.f;
		const float S = Size * Rnd(0.5f, 1.1f) / 100.f;
		D.Scale0 = FVector(S);
		D.Scale1 = FVector(S * 0.5f);
		D.Color = FLinearColor(0.78f, 0.90f, 1.f, 0.7f);
		D.MinAmbient = 0.6f;
		D.Life = Rnd(0.35f, 0.6f);
		D.FadeIn = 0.f;
		D.FadePow = 0.5f;
		D.FloorZ = At.Z;
		M->Emit(D);
	}
}

void Ripple(const UObject* Ctx, const FVector& At, float Radius)
{
	AAntFxManager* M = For(Ctx, At, 6000.f + Radius * 40.f);
	if (!M)
	{
		return;
	}
	FAntFxBit W;
	W.Shape = EAntShape::Cylinder;
	W.Pos = At + FVector(0.f, 0.f, 0.6f);
	const float D0 = Radius * 0.5f / 100.f, D1 = Radius * 2.f / 100.f;
	W.Scale0 = FVector(D0, D0, 0.004f);
	W.Scale1 = FVector(D1, D1, 0.004f);
	W.Color = FLinearColor(0.86f, 0.94f, 1.f, 0.30f);
	W.Life = FMath::Min(1.6f, 0.9f + Radius * 0.002f);
	W.FadeIn = 0.1f;
	W.FadePow = 1.3f;
	M->Emit(W);
}

void Splash(const UObject* Ctx, const FVector& At, float Size)
{
	Droplets(Ctx, At, 8, Size);
	Ripple(Ctx, At, Size * 5.f);
	Ripple(Ctx, At, Size * 2.5f);
}

void Sparks(const UObject* Ctx, const FVector& At, int32 Count, float Size)
{
	AAntFxManager* M = For(Ctx, At, 14000.f);
	if (!M)
	{
		return;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z) * 1.2f + 0.2f;
		Dir.Normalize();
		FAntFxBit S;
		S.Shape = EAntShape::Cube;
		S.Look = EAntFxLook::Glow;
		S.Pos = At;
		S.Vel = Dir * Rnd(260.f, 900.f) * (Size / 7.f);
		S.Gravity = 1400.f;
		S.Drag = 1.2f;
		const float W = Size * Rnd(0.5f, 1.1f) / 100.f;
		S.Scale0 = FVector(W * 2.6f, W * 0.28f, W * 0.28f);
		S.Scale1 = S.Scale0 * 0.25f;
		const float Heat = Rnd(0.6f, 1.3f);
		S.Color = FLinearColor(9.f * Heat, 5.5f * Heat, 1.4f * Heat, 1.f);
		S.Life = Rnd(0.25f, 0.65f);
		S.FadeIn = 0.f;
		S.FadePow = 0.6f;
		S.bStreak = true;
		S.FloorZ = At.Z - Size * 2.f;
		M->Emit(S);
	}
	// The arc itself: one short blue-white flash.
	FAntFxBit F;
	F.Look = EAntFxLook::Glow;
	F.Pos = At;
	F.Scale0 = FVector(Size * 2.2f / 100.f);
	F.Scale1 = FVector(Size * 5.f / 100.f);
	F.Color = FLinearColor(4.f, 5.f, 9.f, 1.f);
	F.Life = 0.12f;
	F.FadeIn = 0.f;
	M->Emit(F);
}

void Steam(const UObject* Ctx, const FVector& At, float Size, float Rise)
{
	AAntFxManager* M = For(Ctx, At, 14000.f + Size * 30.f);
	if (!M)
	{
		return;
	}
	FAntFxBit P;
	P.Pos = At + FVector(Rnd(-0.3f, 0.3f) * Size, Rnd(-0.3f, 0.3f) * Size, 0.f);
	P.Vel = FVector(Rnd(-0.15f, 0.15f) * Rise, Rnd(-0.15f, 0.15f) * Rise, Rise * Rnd(0.7f, 1.2f));
	P.Drag = 0.45f;
	P.Scale0 = FVector(Size * Rnd(0.3f, 0.45f) / 100.f);
	P.Scale1 = FVector(Size * Rnd(0.9f, 1.4f) / 100.f);
	P.Color = FLinearColor(0.95f, 0.96f, 0.98f, Rnd(0.06f, 0.11f));
	P.MinAmbient = 0.5f;
	P.Life = Rnd(2.f, 3.2f);
	P.FadeIn = 0.2f;
	P.FadePow = 1.2f;
	M->Emit(P);
}

void SprayPuff(const UObject* Ctx, const FVector& From, const FVector& Dir, float Range)
{
	AAntFxManager* M = For(Ctx, From, 40000.f);
	if (!M)
	{
		return;
	}
	const FVector D = FMath::VRandCone(Dir.GetSafeNormal(), FMath::DegreesToRadians(17.f));
	FAntFxBit P;
	P.Pos = From + D * 60.f;
	P.Drag = 2.2f;
	P.Vel = D * Range * P.Drag * Rnd(0.55f, 1.f);   // travels Range * 0.55..1 before it hangs
	P.Gravity = 60.f;
	P.Scale0 = FVector(Rnd(0.4f, 0.7f));
	P.Scale1 = FVector(Range * Rnd(0.08f, 0.14f) / 100.f);
	// A hazard first, a pretty cloud second: clearly green, readable day or night, but thin enough to see through.
	P.Color = FLinearColor(0.42f, 0.95f, 0.28f, Rnd(0.11f, 0.17f));
	P.MinAmbient = 0.75f;
	P.Life = Rnd(1.7f, 2.6f);
	P.FadeIn = 0.08f;
	P.FadePow = 1.1f;
	M->Emit(P);
}

void DeliverPulse(const UObject* Ctx, const FVector& At, const FLinearColor& Color)
{
	AAntFxManager* M = For(Ctx, At, 30000.f);
	if (!M)
	{
		return;
	}
	const FLinearColor C(Color.R * 3.f, Color.G * 3.f, Color.B * 3.f, 1.f);
	FAntFxBit Beam;
	Beam.Shape = EAntShape::Cylinder;
	Beam.Look = EAntFxLook::Glow;
	Beam.Pos = At + FVector(0.f, 0.f, 500.f);
	Beam.Vel = FVector(0.f, 0.f, 250.f);
	Beam.Scale0 = FVector(0.5f, 0.5f, 10.f);
	Beam.Scale1 = FVector(2.2f, 2.2f, 13.f);
	Beam.Color = C * 0.6f;
	Beam.Life = 0.9f;
	Beam.FadePow = 1.5f;
	M->Emit(Beam);
	FAntFxBit Ring;
	Ring.Shape = EAntShape::Cylinder;
	Ring.Look = EAntFxLook::Glow;
	Ring.Pos = At + FVector(0.f, 0.f, 6.f);
	Ring.Scale0 = FVector(1.f, 1.f, 0.02f);
	Ring.Scale1 = FVector(11.f, 11.f, 0.02f);
	Ring.Color = C * 0.5f;
	Ring.Life = 0.7f;
	Ring.FadePow = 1.2f;
	M->Emit(Ring);
	for (int32 i = 0; i < 14; ++i)
	{
		FAntFxBit S;
		S.Shape = EAntShape::Cube;
		S.Look = EAntFxLook::Glow;
		S.Pos = At + FVector(Rnd(-120.f, 120.f), Rnd(-120.f, 120.f), Rnd(10.f, 120.f));
		S.Vel = FVector(Rnd(-90.f, 90.f), Rnd(-90.f, 90.f), Rnd(260.f, 760.f));
		S.Drag = 0.8f;
		const float W = Rnd(0.07f, 0.16f);
		S.Scale0 = FVector(W);
		S.Scale1 = FVector(W * 0.15f);
		S.Rot = FQuat(FMath::VRand(), Rnd(0.f, 6.f));
		S.Color = C * Rnd(0.7f, 1.5f);
		S.Life = Rnd(0.7f, 1.5f);
		S.FadeIn = 0.05f;
		S.FadePow = 0.7f;
		M->Emit(S);
	}
}

void LandDust(const UObject* Ctx, const FVector& At, const FVector& Normal, float Size)
{
	AAntFxManager* M = For(Ctx, At, 2500.f);
	if (!M)
	{
		return;
	}
	const FVector Up = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector T1, T2;
	Up.FindBestAxisVectors(T1, T2);
	const int32 N = 4;   // a few small puffs at the feet: the camera is a body length behind, big ones would fill the screen
	for (int32 i = 0; i < N; ++i)
	{
		const float Ang = (i + Rnd(-0.3f, 0.3f)) * 2.f * PI / N;
		const FVector Dir = T1 * FMath::Cos(Ang) + T2 * FMath::Sin(Ang);
		FAntFxBit P;
		P.Pos = At + Dir * Size * 0.35f + Up * Size * 0.05f;
		P.Vel = Dir * Size * Rnd(4.5f, 8.f) + Up * Size * Rnd(0.3f, 0.8f);
		P.Drag = 4.5f;
		P.Scale0 = FVector(Size * Rnd(0.12f, 0.2f) / 100.f);
		P.Scale1 = FVector(Size * Rnd(0.35f, 0.6f) / 100.f);
		P.Color = FLinearColor(FineDust.R, FineDust.G, FineDust.B, Rnd(0.12f, 0.2f));
		P.Life = Rnd(0.45f, 0.75f);
		P.FadeIn = 0.1f;
		P.FadePow = 1.2f;
		M->Emit(P);
	}
}

void DragCrumbs(const UObject* Ctx, const FVector& At, const FLinearColor& FoodColor, float Size)
{
	AAntFxManager* M = For(Ctx, At, 3500.f);
	if (!M)
	{
		return;
	}
	const int32 N = FMath::RandRange(2, 3);
	for (int32 i = 0; i < N; ++i)
	{
		FAntFxBit C;
		C.Shape = FMath::RandBool() ? EAntShape::Cube : EAntShape::Sphere;
		C.Look = EAntFxLook::Solid;
		C.Pos = At + FVector(Rnd(-0.5f, 0.5f) * Size, Rnd(-0.5f, 0.5f) * Size, Size * 0.1f);
		C.Vel = FVector(Rnd(-2.5f, 2.5f) * Size, Rnd(-2.5f, 2.5f) * Size, FMath::Min(260.f, Size * Rnd(3.f, 6.f)));
		C.Gravity = 900.f;
		C.Scale0 = C.Scale1 = FVector(FMath::Clamp(Size * Rnd(0.05f, 0.11f), 0.6f, 6.f) / 100.f);
		C.Rot = FQuat(FMath::VRand(), Rnd(0.f, 6.f));
		const float Shade = Rnd(0.7f, 1.1f);
		C.Color = FLinearColor(FoodColor.R * Shade, FoodColor.G * Shade, FoodColor.B * Shade, 1.f);
		C.Life = Rnd(0.7f, 1.1f);
		C.FloorZ = At.Z;
		M->Emit(C);
	}
	FAntFxBit P;
	P.Pos = At;
	P.Vel = FVector(Rnd(-0.6f, 0.6f) * Size, Rnd(-0.6f, 0.6f) * Size, Size * 0.8f);
	P.Drag = 3.f;
	P.Scale0 = FVector(Size * 0.2f / 100.f);
	P.Scale1 = FVector(Size * 0.5f / 100.f);
	P.Color = FLinearColor(FineDust.R, FineDust.G, FineDust.B, 0.13f);
	P.Life = 0.6f;
	M->Emit(P);
}

UMaterialInstanceDynamic* MakeGlow(UStaticMeshComponent* Comp, const FLinearColor& Color)
{
	if (!Comp)
	{
		return nullptr;
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Comp);
	MID->SetVectorParameterValue(FxColorName, Color);
	Comp->SetMaterial(0, MID);
	Comp->SetCastShadow(false);
	return MID;
}

void SetAmbient(const UObject* Ctx, float Brightness, const FLinearColor& Tint)
{
	if (AAntFxManager* M = AAntFxManager::Get(Ctx))
	{
		M->Ambient = Brightness;
		M->AmbientTint = Tint;
	}
}

FQuat CardRot(const FVector& Pos, const FVector& Cam, bool bUpright)
{
	FVector To = Cam - Pos;
	if (bUpright) { To.Z = 0.f; }
	if (!To.Normalize()) { To = FVector(1.f, 0.f, 0.f); }
	// the card's face (+Z) to the camera, its Y (the flame's height) as close to world up as it can be
	const FVector Up = FMath::Abs(To.Z) > 0.98f ? FVector(1.f, 0.f, 0.f) : FVector::UpVector;
	return FRotationMatrix::MakeFromZY(To, Up).ToQuat();
}

UStaticMesh* CardMesh()
{
	return Map4Look::Shape(4);   // SM_M4_Plane: the project's own 1 m plane (Tools/map4/blender_shapes.py), always cooked with /Game/Map4
}

UMaterialInterface* CardMaterial(EAntFxLook Look)
{
	const TCHAR* Path = Look == EAntFxLook::Flame ? TEXT("/Game/Map4/Materials/M_M4_FxFlame.M_M4_FxFlame")
		: Look == EAntFxLook::Smoke ? TEXT("/Game/Map4/Materials/M_M4_FxSmoke.M_M4_FxSmoke") : nullptr;
	return Path ? LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
}

void TestAll(const UObject* Ctx, const FVector& ViewLoc, const FRotator& ViewRot)
{
	AAntFxManager* M = AAntFxManager::Get(Ctx);
	if (!M)
	{
		return;
	}
	static int32 Step = 0;
	++Step;
	const FVector Fwd = FRotator(0.f, ViewRot.Yaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	UWorld* World = M->GetWorld();
	auto Ground = [World, &ViewLoc, &Fwd, &Right](float Ahead, float Side) -> FVector
	{
		const FVector Top = ViewLoc + Fwd * Ahead + Right * Side + FVector(0.f, 0.f, 60.f);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 4000.f), ECC_Visibility))
		{
			return Hit.ImpactPoint;
		}
		return Top - FVector(0.f, 0.f, 70.f);
	};
	// Steady emitters: every call.
	Steam(Ctx, Ground(150.f, 110.f) + FVector(0.f, 0.f, 2.f), 18.f, 30.f);
	Droplets(Ctx, Ground(150.f, 55.f), 2, 4.f);
	for (int32 i = 0; i < 6; ++i)
	{
		SprayPuff(Ctx, Ground(900.f, 1500.f) + FVector(0.f, 0.f, 1700.f), (Fwd * 0.6f - Right * 0.5f - FVector(0.f, 0.f, 0.6f)).GetSafeNormal(), 2300.f);
	}
	// Bursts: about once a second.
	if (Step % 4 == 0)
	{
		LandDust(Ctx, Ground(150.f, -110.f), FVector::UpVector, 15.f);
		Splash(Ctx, Ground(150.f, -55.f), 5.f);
		Sparks(Ctx, Ground(150.f, 0.f) + FVector(0.f, 0.f, 6.f), 16, 7.f);
		DragCrumbs(Ctx, Ground(110.f, 30.f), FLinearColor(0.95f, 0.8f, 0.4f), 22.f);
		DeliverPulse(Ctx, Ground(700.f, -300.f), FLinearColor(1.f, 0.8f, 0.2f));
		StompDust(Ctx, Ground(2400.f, -900.f));
	}
}

}   // namespace AntFx