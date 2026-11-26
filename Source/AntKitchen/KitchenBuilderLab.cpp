#include "KitchenBuilder.h"
#include "AntLab.h"
#include "AntTypes.h"
#include "Map4.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// KAN-37 «Лаборатория уровней»: the grey-box flat of the active lab variant (AntLab.h). Every box of the data becomes an engine
// cube on this machine (server and clients alike, like the kitchen). Climbing is decided per box: what the data does not mark as
// climbable (walls, furniture bodies, glass) carries the tag AntNoClimb, which the ant's harness respects
// (UAntMovementComponent::IsStickable); -AntLabClimbAll lifts that for a comparison. No art, plain lights.

void AKitchenBuilder::BuildLab()
{
	const FAntLabVariant* V = AntLab::Get();
	if (!V)
	{
		return;
	}
	const bool bClimbAll = FParse::Param(FCommandLine::Get(), TEXT("AntLabClimbAll"));
	const bool bNoCeiling = FParse::Param(FCommandLine::Get(), TEXT("AntLabNoCeiling"));
	int32 Solid = 0, NoClimb = 0;
	for (const FAntLabBox& B : V->Boxes)
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, Root, EAntShape::Cube, B.C - GetActorLocation(), B.S / 100.f, B.Col, B.R, B.bCollide, true);
		if (!C)
		{
			continue;
		}
		C->ComponentTags.Add(TEXT("AntLab"));
		Map4::MarkDebug(C);   // (the art audit has nothing to say about a grey box)
		if (B.bCollide)
		{
			++Solid;
			if (!B.bClimb && !bClimbAll && B.Kind != TEXT("floor") && B.Kind != TEXT("ceiling"))
			{
				C->ComponentTags.Add(TEXT("AntNoClimb"));
				++NoClimb;
			}
		}
		if (B.Kind == TEXT("decal") || B.Kind == TEXT("glass"))
		{
			C->SetCastShadow(false);
		}
		if (B.Tag == TEXT("ceiling"))
		{
			C->ComponentTags.Add(TEXT("LabCeiling"));
			C->SetCastShadow(false);   // (lit from inside: the ceiling must not shade the rooms from the sky light)
			if (bNoCeiling) { C->SetVisibility(false); }
		}
	}

	// Plain light: a soft sun from above, the sky, one shadowless lamp under the ceiling of every room.
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetupAttachment(Root);
	Sun->SetRelativeRotation(FRotator(-55.f, 60.f, 0.f));
	Sun->Intensity = 3.f;
	Sun->CastShadows = true;
	Sun->bAtmosphereSunLight = true;
	Sun->RegisterComponent();
	AddInstanceComponent(Sun);
	Fill = Sun;
	USkyAtmosphereComponent* Sky = NewObject<USkyAtmosphereComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->RegisterComponent();
	AddInstanceComponent(Sky);
	USkyLightComponent* SkyL = NewObject<USkyLightComponent>(this);
	SkyL->SetMobility(EComponentMobility::Movable);
	SkyL->SetupAttachment(Root);
	SkyL->bRealTimeCapture = true;
	SkyL->Intensity = 1.f;
	SkyL->RegisterComponent();
	AddInstanceComponent(SkyL);
	SkyFill = SkyL;
	for (const FAntLabRoom& R : V->Rooms)
	{
		const FVector2D C = R.Rect.GetCenter();
		const float Radius = (float)R.Rect.GetSize().Size() * 0.9f + 1500.f;
		MainLamps.Add(Lamp(FVector(C.X, C.Y, V->Origin.Z + V->Ceiling - 300.f) - GetActorLocation(), 18.f, FLinearColor(1.f, 0.95f, 0.88f), Radius, false));
		MainLampBase.Add(18.f);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UPostProcessComponent* Pp = NewObject<UPostProcessComponent>(this);
		Pp->SetupAttachment(Root);
		Pp->bUnbound = true;
		Pp->Settings.bOverride_AutoExposureMinBrightness = true;
		Pp->Settings.AutoExposureMinBrightness = 0.6f;
		Pp->Settings.bOverride_AutoExposureMaxBrightness = true;
		Pp->Settings.AutoExposureMaxBrightness = 2.f;
		Pp->RegisterComponent();
		AddInstanceComponent(Pp);
	}
	bBuilt = true;
	UE_LOG(LogTemp, Display, TEXT("ANTLAB built variant %s «%s» on this machine (%s): %d boxes, %d solid, %d without climbing%s, %d room lamps"),
		*V->Id, *V->Name, GetNetMode() == NM_Client ? TEXT("client") : TEXT("server/standalone"), V->Boxes.Num(), Solid, NoClimb, bClimbAll ? TEXT(" (-AntLabClimbAll)") : TEXT(""), V->Rooms.Num());
}
