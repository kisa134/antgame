#include "AntMess.h"
#include "AntFx.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"

// =====================================================================================================================
// Dust motes and micro-particles. Each layer is ONE procedural mesh of quads (one draw call) whose motion is entirely in
// the material (/Game/Mess/M_MessDust): the CPU only moves the layer's bounds with the camera and sets a few parameters.
// The motes are lit translucency (per-vertex volumetric lighting): they show where light is strong - the sun shafts from
// the window, the fridge, the lamp cones - and nearly vanish in the shade, like real dust.
//   0  near motes   a 700 uu box round the camera: what an ant sees drifting past its face
//   1  far motes    a 5000 uu box: sparse and bigger, for the Human's and the top camera's scale
//   2  fibres, fridge     at floor level (2..70 uu up) round the fridge's front: fibres and crumbs in its draught
//   3  fibres, radiator   the same under the kitchen window, rising in the warm air
// =====================================================================================================================

namespace
{
	TAutoConsoleVariable<int32> CVarDustLit(TEXT("ant.MessDustLit"), 1, TEXT("Dust motes: 1 lit translucency (shows in the light), 0 unlit additive."));
	TAutoConsoleVariable<float> CVarDustScale(TEXT("ant.MessDustScale"), 1.f, TEXT("Dust motes: brightness multiplier."));

	struct FLayerDef
	{
		int32 Count;
		FVector Box;
		float Size;
		FVector4 Wind;       // xyz drift uu/s, w turbulence uu
		bool bBand;
		FVector2D Band;      // z from, to
		FVector4 Spot;       // xyz, radius (0: everywhere)
		float SpotBase;
		float NearFade;
		float Sparkle;
		float Bright;
		FLinearColor Color;
	};

	const FLayerDef GLayers[] =
	{
		{ 1800, FVector(700.f, 700.f, 700.f),     0.30f, FVector4(5.f, 2.f, 1.2f, 16.f),  false, FVector2D(0, 0),       FVector4(0, 0, 0, 0),           1.f,  3.f,  1.f, 1.0f, FLinearColor(0.95f, 0.92f, 0.86f) },
		{ 1600, FVector(5000.f, 5000.f, 4000.f),  2.0f,  FVector4(12.f, 5.f, 2.5f, 110.f), false, FVector2D(0, 0),      FVector4(0, 0, 0, 0),           1.f,  50.f, 1.f, 0.8f, FLinearColor(0.95f, 0.92f, 0.86f) },
		{ 700,  FVector(1500.f, 1500.f, 100.f),   0.75f, FVector4(-6.f, -14.f, 0.f, 9.f), true,  FVector2D(2.f, 70.f),  FVector4(3700.f, 2600.f, 0.f, 1500.f), 0.f, 4.f, 0.3f, 1.2f, FLinearColor(0.85f, 0.82f, 0.76f) },
		{ 700,  FVector(1500.f, 1500.f, 100.f),   0.75f, FVector4(3.f, 2.f, 14.f, 12.f), true,  FVector2D(2.f, 160.f), FVector4(0.f, -4600.f, 0.f, 1600.f), 0.f, 4.f, 0.3f, 1.2f, FLinearColor(0.88f, 0.84f, 0.78f) },
	};
}

void AAntMess::BuildDust()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("AntNoDust")))
	{
		bDustOn = false;
		return;
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, CVarDustLit.GetValueOnGameThread() ? TEXT("/Game/Mess/M_MessDust.M_MessDust") : TEXT("/Game/Mess/M_MessDustUnlit.M_MessDustUnlit"));
	if (!Base)
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTMESS dust: material missing"));
		return;
	}
	FRandomStream R(4242);
	for (const FLayerDef& L : GLayers)
	{
		TArray<FVector> V;
		TArray<int32> Tri;
		TArray<FVector> Nrm;
		TArray<FVector2D> UV;
		TArray<FLinearColor> VC;
		TArray<FProcMeshTangent> Tan;
		V.Reserve(L.Count * 4); Tri.Reserve(L.Count * 6); Nrm.Reserve(L.Count * 4); UV.Reserve(L.Count * 4); VC.Reserve(L.Count * 4); Tan.Reserve(L.Count * 4);
		static const FVector2D Corner[4] = { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1) };
		for (int32 i = 0; i < L.Count; ++i)
		{
			const FVector P(R.FRandRange(0.f, L.Box.X), R.FRandRange(0.f, L.Box.Y), R.FRandRange(0.f, L.Box.Z));
			const FLinearColor C(R.FRand(), R.FRand(), R.FRand(), R.FRand());
			const int32 B = V.Num();
			for (int32 k = 0; k < 4; ++k)
			{
				V.Add(P); Nrm.Add(FVector::UpVector); UV.Add(Corner[k]); VC.Add(C); Tan.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}
			Tri.Append({ B, B + 1, B + 2, B, B + 2, B + 3 });
		}
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->bUseAsyncCooking = false;
		C->CreateMeshSection_LinearColor(0, V, Tri, Nrm, UV, VC, Tan, false);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bReceivesDecals = false;
		C->SetCanEverAffectNavigation(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetBoundsScale(1.2f);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Box"), FLinearColor(L.Box.X, L.Box.Y, L.Box.Z, L.Size));
		M->SetVectorParameterValue(TEXT("Wind"), FLinearColor(L.Wind.X, L.Wind.Y, L.Wind.Z, L.Wind.W));
		M->SetVectorParameterValue(TEXT("Band"), FLinearColor(L.bBand ? 1.f : 0.f, L.Band.X, L.Band.Y, 0.f));
		M->SetVectorParameterValue(TEXT("Spot"), FLinearColor(L.Spot.X, L.Spot.Y, L.Spot.Z, L.Spot.W));
		M->SetVectorParameterValue(TEXT("Spot2"), FLinearColor(L.SpotBase, 0.f, 0.f, 0.f));
		M->SetVectorParameterValue(TEXT("Color"), L.Color);
		C->SetMaterial(0, M);
		C->RegisterComponent();
		DustComps.Add(C);
		FDustLayer& D = Dust.AddDefaulted_GetRef();
		D.C = C;
		D.M = M;
		D.Box = L.Box;
		D.bBand = L.bBand;
	}
}

void AAntMess::SetDust(bool bOn)
{
	bDustOn = bOn;
	for (FDustLayer& D : Dust)
	{
		if (D.C) { D.C->SetVisibility(bOn); }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMESS dust %s"), bOn ? TEXT("on") : TEXT("off"));
}

void AAntMess::TickDust(float Dt)
{
	if (!bDustOn || Dust.Num() == 0)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const float Fov = FMath::Clamp(PC->PlayerCameraManager->GetFOVAngle(), 5.f, 170.f);
	float ViewH = 900.f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Sz;
		GEngine->GameViewport->GetViewportSize(Sz);
		if (Sz.Y > 10.f) { ViewH = (float)Sz.Y; }
	}
	// world size of one pixel per uu of distance (vertical fov from the horizontal one at 16:9 is close enough)
	const float PixPerUU = 2.f * FMath::Tan(FMath::DegreesToRadians(Fov) * 0.5f) / ViewH * 0.6f;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Age = Now - StirTime;
	const AAntFxManager* Fx = AAntFxManager::Get(this);
	const float Amb = Fx ? Fx->Ambient : 1.f;
	const bool bLit = CVarDustLit.GetValueOnGameThread() != 0;
	const float Inten = CVarDustScale.GetValueOnGameThread() * (bLit ? 1.f : FMath::Clamp(Amb, 0.2f, 1.f));
	for (int32 i = 0; i < Dust.Num(); ++i)
	{
		FDustLayer& D = Dust[i];
		const FLayerDef& L = GLayers[FMath::Min(i, (int32)UE_ARRAY_COUNT(GLayers) - 1)];
		if (!D.C || !D.M)
		{
			continue;
		}
		// the layer's bounds ride with the camera (the motes themselves stay put in the world: the shader wraps them)
		const FVector Loc = D.bBand ? FVector(Cam.X - D.Box.X * 0.5f, Cam.Y - D.Box.Y * 0.5f, 0.f) : Cam - D.Box * 0.5f;
		D.C->SetWorldLocation(Loc);
		D.M->SetVectorParameterValue(TEXT("Pix"), FLinearColor(PixPerUU, 1.4f, L.NearFade, L.Sparkle));
		D.M->SetVectorParameterValue(TEXT("Stir"), FLinearColor(StirAt.X, StirAt.Y, StirAt.Z, FMath::Max(0.f, Age)));
		D.M->SetVectorParameterValue(TEXT("Stir2"), FLinearColor(StirRadius, StirStrength, 0.f, 0.f));
		D.M->SetScalarParameterValue(TEXT("Intensity"), Inten * L.Bright);
	}
}
