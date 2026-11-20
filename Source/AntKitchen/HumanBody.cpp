#include "HumanBody.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "AntFx.h"
#include "AntTypes.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntMovementComponent.h"
#include "KitchenLoot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "EngineUtils.h"

#include "HumanBodyData.inl"
#include "HumanGranData.inl"
#include "HumanKidData.inl"

// ---------------------------------------------------------------------------------------------
// Assets and looks
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Material slot -> the game's surface look (NpcRig::Material). Textured looks use /Game/Human/M_HumanSurface: M_Surface projected in the
	 *  segment's own space (a world-space weave would swim over a moving leg). Tex null: plain paint. */
	using FHumanLook = FNpcLook;
	const TCHAR* const ClothStep = TEXT("Surf_Fabric");
	const FHumanLook GHumanLooks[] =
	{
		{ TEXT("Skin"),         TEXT("T_Enamel_White"), 600.f, 0.90f, 0.58f, 0.44f, 0.55f, nullptr },     // clean paint, like the rest of the set
		{ TEXT("Shirt"),        TEXT("T_Fabric_Grey"),  260.f, 3.00f, 2.75f, 2.20f, 0.9f,  ClothStep },     // (the fabric texture is a dark grey: tints go well above 1)
		{ TEXT("ShirtTrim"),    TEXT("T_Fabric_Grey"),  260.f, 0.35f, 0.60f, 1.20f, 0.9f,  ClothStep },
		{ TEXT("Pyjama"),       TEXT("T_Fabric_Grey"),  260.f, 0.80f, 1.35f, 2.30f, 0.9f,  ClothStep },
		{ TEXT("PyjamaStripe"), TEXT("T_Fabric_Grey"),  260.f, 2.70f, 2.85f, 3.10f, 0.9f,  ClothStep },
		{ TEXT("Slipper"),      TEXT("T_Fabric_Grey"),  120.f, 3.10f, 1.20f, 1.80f, 0.95f, ClothStep },
		{ TEXT("Fluff"),        nullptr, 0.f, 0.98f, 0.96f, 0.92f, 0.95f, ClothStep },
		{ TEXT("SlipperSole"),  nullptr, 0.f, 0.74f, 0.62f, 0.46f, 0.8f,  nullptr },
		{ TEXT("Hair"),         nullptr, 0.f, 0.24f, 0.13f, 0.06f, 0.7f,  nullptr },
		{ TEXT("EyeWhite"),     nullptr, 0.f, 0.97f, 0.97f, 0.95f, 0.25f, nullptr },
		{ TEXT("Pupil"),        nullptr, 0.f, 0.03f, 0.03f, 0.04f, 0.2f,  nullptr },
		{ TEXT("Mouth"),        nullptr, 0.f, 0.42f, 0.10f, 0.10f, 0.5f,  nullptr },
	};
	const FHumanLook GGranLooks[] =
	{
		{ TEXT("Skin"),        TEXT("T_Enamel_White"), 600.f, 0.90f, 0.60f, 0.48f, 0.55f, nullptr },
		{ TEXT("Cardigan"),    TEXT("T_Fabric_Grey"),  200.f, 1.80f, 0.85f, 1.65f, 0.9f,  ClothStep },
		{ TEXT("Blouse"),      TEXT("T_Fabric_Grey"),  260.f, 3.00f, 2.85f, 2.45f, 0.9f,  ClothStep },
		{ TEXT("Skirt"),       TEXT("T_Fabric_Grey"),  260.f, 0.70f, 0.90f, 1.35f, 0.9f,  ClothStep },
		{ TEXT("SkirtDot"),    TEXT("T_Fabric_Grey"),  260.f, 2.60f, 2.70f, 3.00f, 0.9f,  ClothStep },
		{ TEXT("Stocking"),    nullptr, 0.f, 0.78f, 0.62f, 0.50f, 0.8f,  ClothStep },
		{ TEXT("Slipper"),     TEXT("T_Fabric_Grey"),  120.f, 1.30f, 2.00f, 1.20f, 0.95f, ClothStep },
		{ TEXT("Fluff"),       nullptr, 0.f, 0.98f, 0.96f, 0.92f, 0.95f, ClothStep },
		{ TEXT("SlipperSole"), nullptr, 0.f, 0.74f, 0.62f, 0.46f, 0.8f,  nullptr },
		{ TEXT("HairGrey"),    nullptr, 0.f, 0.86f, 0.86f, 0.88f, 0.7f,  nullptr },
		{ TEXT("Glasses"),     nullptr, 0.f, 0.16f, 0.12f, 0.10f, 0.3f,  nullptr },
		{ TEXT("EyeWhite"),    nullptr, 0.f, 0.97f, 0.97f, 0.95f, 0.25f, nullptr },
		{ TEXT("Pupil"),       nullptr, 0.f, 0.03f, 0.03f, 0.04f, 0.2f,  nullptr },
		{ TEXT("Mouth"),       nullptr, 0.f, 0.50f, 0.14f, 0.14f, 0.5f,  nullptr },
	};
	const FHumanLook GKidLooks[] =
	{
		{ TEXT("Skin"),        TEXT("T_Enamel_White"), 600.f, 0.95f, 0.66f, 0.52f, 0.55f, nullptr },
		{ TEXT("KidShirt"),    TEXT("T_Fabric_Grey"),  260.f, 3.20f, 2.40f, 0.45f, 0.9f,  ClothStep },
		{ TEXT("KidTrim"),     TEXT("T_Fabric_Grey"),  260.f, 2.90f, 0.90f, 0.60f, 0.9f,  ClothStep },
		{ TEXT("KidShorts"),   TEXT("T_Fabric_Grey"),  260.f, 0.60f, 1.30f, 2.40f, 0.9f,  ClothStep },
		{ TEXT("Sock"),        nullptr, 0.f, 0.96f, 0.96f, 0.94f, 0.9f,  ClothStep },
		{ TEXT("Sneaker"),     nullptr, 0.f, 0.88f, 0.20f, 0.20f, 0.7f,  nullptr },
		{ TEXT("SneakerSole"), nullptr, 0.f, 0.96f, 0.96f, 0.94f, 0.6f,  nullptr },
		{ TEXT("HairBrown"),   nullptr, 0.f, 0.45f, 0.26f, 0.10f, 0.7f,  nullptr },
		{ TEXT("Cheek"),       nullptr, 0.f, 0.96f, 0.52f, 0.48f, 0.6f,  nullptr },
		{ TEXT("EyeWhite"),    nullptr, 0.f, 0.97f, 0.97f, 0.95f, 0.25f, nullptr },
		{ TEXT("Pupil"),       nullptr, 0.f, 0.03f, 0.03f, 0.04f, 0.2f,  nullptr },
		{ TEXT("Mouth"),       nullptr, 0.f, 0.50f, 0.14f, 0.14f, 0.5f,  nullptr },
	};
	// Scale, capsule radius / half height, walk speed, tempo, hunch, arms out, swat: wind-up / radius / cooldown, sitting slowness.
	const FHumanStyle GHumanStyles[HumanFig::Count] =
	{
		{ 1.00f, 340.f, 1700.f, 720.f, 1.00f,  0.f,  0.f, HumanAct::SwatHit, 300.f, 2.6f, 1.0f },   // the Dad: exactly as he was
		{ 0.88f, 300.f, 1470.f, 380.f, 0.72f, 13.f,  0.f, 0.95f,             400.f, 4.0f, 1.7f },   // grandma: slow, stooped, a wide flat swatter
		{ 0.50f, 190.f,  790.f, 900.f, 1.55f,  0.f, 20.f, HumanAct::SwatHit, 200.f, 2.0f, 0.5f },   // the kid: quick, arms out for balance
	};
}

// The materials, the easing helpers, the bone rotation and the two-bone IK live in NpcBody.h now: every jointed creature uses them.
using namespace NpcRig;

namespace HumanBody
{

TConstArrayView<FHumanSegDef> Segs(uint8 Fig)
{
	switch (Fig)
	{
	case HumanFig::Grandma: return TConstArrayView<FHumanSegDef>(GGranSegs, UE_ARRAY_COUNT(GGranSegs));
	case HumanFig::Kid:     return TConstArrayView<FHumanSegDef>(GKidSegs, UE_ARRAY_COUNT(GKidSegs));
	default:                return TConstArrayView<FHumanSegDef>(GHumanSegs, UE_ARRAY_COUNT(GHumanSegs));
	}
}

const TCHAR* SegName(int32 Seg)
{
	return (Seg >= 0 && Seg < (int32)UE_ARRAY_COUNT(GHumanSegs)) ? GHumanSegs[Seg].Name + 5 : TEXT("None");   // without "SM_H_"
}

const FHumanStyle& Style(uint8 Fig)
{
	return GHumanStyles[Fig < HumanFig::Count ? Fig : 0];
}

UStaticMesh* Mesh(int32 Seg, uint8 Fig)
{
	const TConstArrayView<FHumanSegDef> Defs = Segs(Fig);
	if (Seg < 0 || Seg >= HumanSeg::Count || Seg >= Defs.Num())
	{
		return nullptr;
	}
	return NpcRig::LoadMesh(Fig == HumanFig::Dad ? TEXT("/Game/Human") : TEXT("/Game/Npc"), Defs[Seg].Name);
}

bool Enabled(uint8 Fig)
{
	static int32 States[HumanFig::Count] = { -1, -1, -1 };
	Fig = Fig < HumanFig::Count ? Fig : 0;
	int32& State = States[Fig];
	if (State < 0)
	{
		State = 1;
		if (Segs(Fig).Num() != HumanSeg::Count || (Fig == HumanFig::Dad ? FParse::Param(FCommandLine::Get(), TEXT("AntOldHuman")) : !NpcWorld::NewNpcs()))
		{
			State = 0;
		}
		for (int32 i = 0; i < HumanSeg::Count && State == 1; ++i)
		{
			if (!Mesh(i, Fig)) { State = 0; }
		}
		static const TCHAR* Who[HumanFig::Count] = { TEXT("the Human"), TEXT("grandma"), TEXT("the kid") };
		UE_LOG(LogTemp, Display, TEXT("ANTHUMAN body of %s: %s"), Who[Fig], State ? TEXT("jointed (19 segments)") : TEXT("the old one"));
	}
	return State == 1;
}

void ApplyLook(UStaticMeshComponent* C, uint8 Fig)
{
	const TConstArrayView<FHumanLook> Looks = Fig == HumanFig::Grandma ? TConstArrayView<FHumanLook>(GGranLooks, UE_ARRAY_COUNT(GGranLooks))
		: (Fig == HumanFig::Kid ? TConstArrayView<FHumanLook>(GKidLooks, UE_ARRAY_COUNT(GKidLooks)) : TConstArrayView<FHumanLook>(GHumanLooks, UE_ARRAY_COUNT(GHumanLooks)));
	UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
	if (!M)
	{
		return;
	}
	const TArray<FStaticMaterial>& Slots = M->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString Slot = Slots[i].MaterialSlotName.ToString();
		const FHumanLook* Look = nullptr;
		for (const FHumanLook& L : Looks)
		{
			if (Slot.Equals(L.Slot, ESearchCase::IgnoreCase)) { Look = &L; break; }
		}
		if (!Look)
		{
			UE_LOG(LogTemp, Warning, TEXT("HumanBody: no look for slot %s of %s"), *Slot, *M->GetName());
			continue;
		}
		if (UMaterialInstanceDynamic* Mat = NpcRig::Material(*Look))
		{
			C->SetMaterial(i, Mat);
		}
		if (i == 0 && Look->Step)
		{
			C->ComponentTags.AddUnique(FName(Look->Step));
		}
	}
}

} // namespace HumanBody

// ---------------------------------------------------------------------------------------------
// Setting the body up
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::SetupJointedBody()
{
	if (Segs.Num() != HumanSeg::Count || !HumanBody::Enabled(Fig))
	{
		return;
	}
	const TConstArrayView<FHumanSegDef> Defs = HumanBody::Segs(Fig);
	for (int32 i = 0; i < HumanSeg::Count; ++i)
	{
		UStaticMeshComponent* C = Segs[i];
		C->SetStaticMesh(HumanBody::Mesh(i, Fig));
		HumanBody::ApplyLook(C, Fig);
		C->SetRelativeTransform(FTransform(FQuat::Identity, Defs[i].Joint, FVector(HumanBody::Scale)));
		// Every segment is a moving surface: ants, the spider and the army climb it and ride it.
		UAntMovementComponent::MakeClimbable(C);
		C->SetCastShadow(true);
		C->SetVisibility(true);
		Rig.SegPos[i] = Defs[i].Joint;
		Rig.SegRot[i] = FQuat::Identity;
	}
	bNewBody = true;
	ModelPivot = nullptr;
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AHumanCharacter::BuildHeldThings()
{
	// What he holds: the rolled newspaper lives in his right fist, the spray can takes its place while he sprays.
	if (GetNetMode() == NM_DedicatedServer || !bNewBody)
	{
		return;
	}
	const float K = 1.f / HumanBody::Scale;   // the hand is scaled 19: lengths in its space are mesh centimetres
	const FVector Grip(10.f, -35.f, -150.f);  // the middle of the right fist, uu
	if (Fig == HumanFig::Kid)
	{
		return;   // empty hands
	}
	if (Fig == HumanFig::Grandma)
	{
		// Grandma's things, out of plain shapes: the fly swatter she carries, the broom she sweeps with, her knitting needles.
		auto Add = [&](USceneComponent* Parent, EAntShape Shape, const FVector& At, const FVector& Size, const FLinearColor& Color, const FRotator& Rot)
		{
			UStaticMeshComponent* C = AntKit::AddPart(this, Parent, Shape, At * K, Size * (K / 100.f), Color, Rot, false, false);
			C->SetCastShadow(true);
			return C;
		};
		const FLinearColor Wood(0.55f, 0.36f, 0.18f), Straw(0.85f, 0.70f, 0.30f), Steel(0.75f, 0.76f, 0.80f), Plastic(0.90f, 0.25f, 0.30f);
		SwatterPivot = AntKit::AddPivot(this, Segs[HumanSeg::HandR], Grip * K);
		Add(SwatterPivot, EAntShape::Cylinder, FVector(480.f, 0.f, 0.f), FVector(34.f, 34.f, 1000.f), Steel, FRotator(90.f, 0.f, 0.f));   // the wire handle, along the hand's X like the Dad's newspaper
		Add(SwatterPivot, EAntShape::Cube, FVector(1120.f, 0.f, 0.f), FVector(420.f, 30.f, 420.f), Plastic, FRotator::ZeroRotator);        // the flat head
		BroomPivot = AntKit::AddPivot(this, Segs[HumanSeg::HandR], Grip * K);
		Add(BroomPivot, EAntShape::Cylinder, FVector(820.f, 0.f, 0.f), FVector(60.f, 60.f, 2150.f), Wood, FRotator(90.f, 0.f, 0.f));
		Add(BroomPivot, EAntShape::Cube, FVector(1900.f, 0.f, 0.f), FVector(230.f, 170.f, 760.f), Straw, FRotator::ZeroRotator);            // the bristles
		BroomPivot->SetVisibility(false, true);
		for (int32 s = 0; s < 2; ++s)
		{
			UStaticMeshComponent* N = Add(Segs[s == 0 ? HumanSeg::HandL : HumanSeg::HandR], EAntShape::Cylinder, FVector(140.f, s == 0 ? 35.f : -35.f, -210.f), FVector(16.f, 16.f, 620.f), Steel, FRotator(55.f, s == 0 ? 40.f : -40.f, 0.f));
			N->SetVisibility(false);
			NeedleArt.Add(N);
		}
		return;
	}
	const AntArt::FPart& Paper = AntArt::Get(TEXT("SM_RolledNewspaper"));
	if (Paper.IsValid())
	{
		PaperArt = AntArt::AddArt(this, Segs[HumanSeg::HandR], Paper);
		PaperArt->SetRelativeTransform(AntArt::Fit(Paper, (Grip + FVector(600.f, 0.f, 0.f)) * K, 1500.f * K, false));
		PaperArt->SetCastShadow(true);
	}
	const AntArt::FPart& Can = AntArt::Get(TEXT("SM_BugSpray"));
	if (Can.IsValid())
	{
		SprayArt = AntArt::AddArt(this, Segs[HumanSeg::HandR], Can);
		SprayArt->SetRelativeTransform(AntArt::Fit(Can, (Grip + FVector(70.f, 0.f, 0.f)) * K, 400.f * K, false));
		SprayArt->SetVisibility(false);
		SprayArt->SetCastShadow(true);
	}
}

FTransform AHumanCharacter::BodyFrame() const
{
	const FVector Floor = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Floor, FVector(FigScale));   // the family are the same figure, smaller
}

float AHumanCharacter::ActTime() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	const float Now = (HasAuthority() || !GS) ? W->GetTimeSeconds() : (float)GS->GetServerWorldTimeSeconds();
	return Now - Act.Start;
}

void AHumanCharacter::StartAct(uint8 Kind, uint8 Arg, const FVector& Target)
{
	if (!HasAuthority())
	{
		return;
	}
	Act.Kind = Kind;
	Act.Arg = Arg;
	Act.Start = GetWorld()->GetTimeSeconds();
	Act.Target = Target;
	ForceNetUpdate();
}

void AHumanCharacter::PlayReach(const FVector& Point)
{
	if (bNewBody && !bKnockedOut && !bStunned && !bStompPending)
	{
		StartAct(HumanAct::Reach, 0, Point);
	}
}

void AHumanCharacter::PlayPeer(const FVector& Point, float Seconds)
{
	if (bNewBody && !bKnockedOut && !bStunned && !bStompPending)
	{
		StartAct(HumanAct::Peer, (uint8)FMath::Clamp(FMath::RoundToInt(Seconds * 10.f), 5, 250), Point);
	}
}

bool AHumanCharacter::IsPeering() const
{
	return Act.Kind == HumanAct::Peer && ActTime() < Act.Arg * 0.1f;
}

FVector AHumanCharacter::GetPalmLocation(int32 Side) const
{
	if (!bNewBody)
	{
		return GetActorTransform().TransformPosition(FVector(700.f, Side == 0 ? -420.f : 420.f, 500.f));
	}
	const UStaticMeshComponent* Hand = Segs[Side == 0 ? HumanSeg::HandL : HumanSeg::HandR];
	return Hand->GetComponentTransform().TransformPosition(FVector(0.f, Side == 0 ? 35.f : -35.f, -150.f) / HumanBody::Scale);
}

// ---------------------------------------------------------------------------------------------
// The pose
// ---------------------------------------------------------------------------------------------

namespace
{
	struct FSpine
	{
		FVector Pelvis, Belly, Chest, Neck;
		FQuat QPelvis, QBelly, QChest;
		FVector Shoulder[2];
	};

	/** What an action wants from one arm. The wrist goes to Shoulder + Rel (turned with the chest), or to Abs (body space), mixed by AbsW. */
	struct FArmCtl
	{
		float W = 0.f;            // how much the action owns the arm (0: it just hangs and swings)
		FVector Rel = FVector::ZeroVector;
		FVector Abs = FVector::ZeroVector;
		float AbsW = 0.f;
		FQuat RotA = FQuat::Identity;   // hand rotation at Rel
		FQuat RotB = FQuat::Identity;   // hand rotation at Abs
		float RotW = 1.f;
		bool bPaper = false;      // Abs is found from PaperP: the tip of the rolled newspaper lands there
		float PaperLen = 0.f;     // > 0: something longer than the newspaper (grandma's broom)
		FVector PaperP = FVector::ZeroVector;
		FVector PaperFlat = FVector::ForwardVector;
		bool bPole = false;
		FVector Pole = FVector::ZeroVector;
		float Curl = 0.f;
	};

	FQuat FromXZ(const FVector& X, const FVector& ZHint, const FVector& ZHint2)
	{
		const FVector XN = X.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		FVector Z = FVector::VectorPlaneProject(ZHint.GetSafeNormal(), XN);
		if (Z.SizeSquared() < 0.03f)
		{
			Z = FVector::VectorPlaneProject(ZHint2.GetSafeNormal(), XN);
		}
		return FRotationMatrix::MakeFromXZ(XN, Z).ToQuat();
	}
}

FVector AHumanCharacter::SprayNozzleBody() const
{
	const TConstArrayView<FHumanSegDef> Defs = HumanBody::Segs(Fig);
	const FVector DirB = BodyFrame().InverseTransformVectorNoScale(GetSprayDirection());
	return FVector(0.f, Defs[HumanSeg::UpperArmR].Joint.Y, Defs[HumanSeg::UpperArmR].Joint.Z - 35.f) + DirB.ToOrientationQuat().RotateVector(FVector(1150.f, 0.f, -60.f));
}

void AHumanCharacter::PoseBody(float Dt)
{
	using namespace HumanSeg;
	if (!bNewBody)
	{
		return;
	}
	FHumanRig& R = Rig;
	const TConstArrayView<FHumanSegDef> Defs = HumanBody::Segs(Fig);
	Dt = FMath::Clamp(Dt, 0.f, 0.1f);
	const float Now = GetWorld()->GetTimeSeconds();
	const FTransform Body = BodyFrame();
	const FVector Origin = Body.GetLocation();
	const FVector Fwd = Body.GetUnitAxis(EAxis::X);
	const FVector Right = Body.GetUnitAxis(EAxis::Y);
	const float Yaw = (float)Body.Rotator().Yaw;
	const float FloorZ = (float)Origin.Z;
	// The family: the same figure, smaller (FK: uu of the world per uu of the body) and carrying itself its own way.
	const FHumanStyle& St = HumanBody::Style(Fig);
	const float FK = FigScale;

	// Proportions, straight from the model's joints (uu).
	const float HipY = (float)Defs[ThighR].Joint.Y, HipZ = (float)Defs[ThighR].Joint.Z;
	const float LThigh = HipZ - (float)Defs[ShinR].Joint.Z, LShin = (float)(Defs[ShinR].Joint.Z - Defs[FootR].Joint.Z), AnkleZ = (float)Defs[FootR].Joint.Z;
	const float BellyUp = (float)Defs[Belly].Joint.Z - HipZ, ChestUp = (float)(Defs[Chest].Joint.Z - Defs[Belly].Joint.Z);
	const float NeckUp = (float)(Defs[Neck].Joint.Z - Defs[Chest].Joint.Z), HeadUp = (float)(Defs[Head].Joint.Z - Defs[Neck].Joint.Z);
	const float ShoulderY = (float)Defs[UpperArmR].Joint.Y, ShoulderUp = (float)(Defs[UpperArmR].Joint.Z - Defs[Chest].Joint.Z);
	const float LUpper = (float)(Defs[UpperArmR].Joint.Z - Defs[ForearmR].Joint.Z), LFore = (float)(Defs[ForearmR].Joint.Z - Defs[HandR].Joint.Z);
	const float LKnuckle = (float)(Defs[HandR].Joint.Z - Defs[FingersR].Joint.Z);
	const float ArmReach = LUpper + LFore;
	const float LegReach = (LThigh + LShin) * 0.985f;
	const float SoleFwd = 159.f;       // the middle of the sole is this far in front of the ankle
	const float PaperReach = 1330.f;   // from the fist to the tip of the rolled newspaper

	// ---- what is going on
	const FVector Vel = GetVelocity() * FVector(1.f, 1.f, 0.f);
	const float Speed = (float)Vel.Size() / FK;   // in the body's own units
	const FVector VelDir = Speed > 1.f ? Vel.GetSafeNormal() : Fwd;
	const float Ahead = (float)FVector::DotProduct(VelDir, Fwd);   // -1 when he backs away
	const bool bMoving = Speed > 50.f;
	const bool bKO = bKnockedOut;
	const bool bStun = bStunned && !bKO;
	uint8 A = Act.Kind;
	float T = ActTime();
	if (DevFreezeAt >= 0.f)
	{
		T = FMath::Min(T, DevFreezeAt);
	}
	float ActDur = 0.f;
	switch (A)
	{
	case HumanAct::Stomp: ActDur = 1.15f; break;
	case HumanAct::Swat: ActDur = St.SwatHit + 0.85f; break;
	case HumanAct::Sit: ActDur = 1.e9f; break;
	case HumanAct::Rise: ActDur = HumanAct::SitTime * St.SitSlow + 0.05f; break;
	case HumanAct::Sweep: ActDur = 2.3f; break;
	case HumanAct::Slap: ActDur = HumanAct::SlapHit + 1.0f; break;
	case HumanAct::Grab: ActDur = 0.95f; break;
	case HumanAct::Throw: ActDur = 0.95f; break;
	case HumanAct::Reach: ActDur = 1.0f; break;
	case HumanAct::Shake: ActDur = 0.95f; break;
	case HumanAct::Peer: ActDur = Act.Arg * 0.1f; break;
	default: break;
	}
	if (T < 0.f || T > ActDur || bStun || bKO)
	{
		A = HumanAct::None;
	}
	const FVector TargetB = Body.InverseTransformPosition(FVector(Act.Target));
	Approach(R.RageW, Rage / 3.f, 2.f, Dt);
	Approach(R.PanicW, bKO ? 1.f : 0.f, 5.f, Dt);
	Approach(R.StunW, bStun ? 1.f : 0.f, 8.f, Dt);
	Approach(R.SprayW, (bSpraying && !bKO && !bStun) ? 1.f : 0.f, 9.f, Dt);
	const bool bHolding = (Held != nullptr || bCarryPose) && !(A == HumanAct::Grab && T < 0.5f) && A != HumanAct::Throw;
	Approach(R.HoldW, bHolding ? 1.f : 0.f, 7.f, Dt);
	const float Tempo = (1.f + 0.35f * R.RageW) * St.Tempo;
	R.Clock += Dt;
	R.Breath += Dt * 2.f * PI * (0.22f + 0.2f * R.RageW + 0.5f * R.PanicW);
	const float C = R.Clock;

	// ---------------------------------------------------------------------------------- feet
	const int32 StompLeg = A == HumanAct::Stomp ? (Act.Arg & 1) : -1;
	const bool bFootStun = bStun && StunKind == 1;
	const bool bHop = A == HumanAct::Shake || bFootStun;
	const float Wide = 1.f + 0.16f * R.RageW + 0.35f * FMath::Min(1.f, R.Crouch / 900.f);
	auto Home = [&](int32 i) { return Origin + Right * ((i == 0 ? -HipY : HipY) * Wide * FK); };
	for (int32 i = 0; i < 2; ++i)
	{
		FHumanFoot& F = R.Feet[i];
		if (!F.bInit || FVector::DistSquared2D(F.Pos, Home(i)) > FMath::Square(1700.f * FK))
		{
			F = FHumanFoot();   // spawned or teleported: under the hips
			F.Pos = Home(i);
			F.Yaw = Yaw;
			F.bInit = true;
		}
	}
	bool bDrive[2] = { false, false };

	if (StompLeg >= 0)
	{
		// The stomp: the knee comes up (the warning), a short hang, the slam. Lands exactly on GetStompPoint and stays there.
		FHumanFoot& F = R.Feet[StompLeg];
		bDrive[StompLeg] = true;
		const float Sgn = StompLeg == 0 ? -1.f : 1.f;
		if (!R.bDriven[StompLeg])
		{
			F.From = F.Pos;
			F.FromYaw = F.Yaw;
		}
		const FVector Cock = Origin + (Fwd * 130.f + Right * (Sgn * HipY * 1.2f)) * FK;
		const FVector Hit = GetStompPoint(StompLeg);
		const float Top = 900.f;
		if (T < 0.40f)
		{
			const float K = EaseOut(T / 0.40f);
			F.Pos = FMath::Lerp(F.From, Cock, K);
			F.Lift = Top * K;
			F.Pitch = -30.f * K;
			F.T = 0.5f;
		}
		else if (T < 0.46f)
		{
			const float K = (T - 0.40f) / 0.06f;
			F.Pos = Cock;
			F.Lift = Top + 45.f * FMath::Sin(PI * K);
			F.Pitch = FMath::Lerp(-30.f, 4.f, K);
			F.T = 0.5f;
		}
		else if (T < HumanAct::StompHit)
		{
			const float K = EaseIn((T - 0.46f) / (HumanAct::StompHit - 0.46f));
			F.Pos = FMath::Lerp(Cock, Hit, K);
			F.Lift = Top * (1.f - K);
			F.Pitch = 4.f;
			F.T = 0.5f;
		}
		else if (F.T < 1.f)
		{
			F.Pos = Hit;
			F.T = 1.f;
			F.Lift = 0.f;
			F.Pitch = 0.f;
			R.DipVel -= 2600.f;
		}
		F.Pos.Z = FloorZ;
		F.Yaw += FMath::FindDeltaAngleDegrees(F.Yaw, Yaw) * FMath::Min(1.f, Dt * 10.f);
	}
	if (bHop)
	{
		// Hopping on the left leg: shaking the ants off, or holding the foot something fell on.
		const float Old = R.HopPhase;
		R.HopPhase += Dt * (bFootStun ? 2.1f : 2.7f);
		const float H = FMath::Abs(FMath::Sin(PI * R.HopPhase));
		bDrive[0] = bDrive[1] = true;
		FHumanFoot& S = R.Feet[0];
		if (S.T < 1.f)
		{
			S.T = 1.f;
		}
		S.Lift = (bFootStun ? 110.f : 150.f) * H;
		S.Pitch = -16.f * H;
		if (FMath::FloorToInt(Old) != FMath::FloorToInt(R.HopPhase))
		{
			R.DipVel -= 900.f;
			OnFootfall(0, 0.55f);
		}
		FHumanFoot& L = R.Feet[1];
		const FVector LB = bFootStun ? FVector(330.f, 150.f, 760.f + 40.f * FMath::Sin(C * 9.f))
			: FVector(140.f + 190.f * FMath::Sin(C * 21.f), 300.f + 60.f * FMath::Sin(C * 13.f), 560.f + 130.f * FMath::Sin(C * 17.f));
		L.Pos = Body.TransformPosition(FVector(LB.X, LB.Y, 0.f));
		L.Lift = (float)LB.Z + S.Lift;
		L.Pitch = bFootStun ? 10.f : -25.f + 20.f * FMath::Sin(C * 23.f);
		L.Yaw = Yaw + (bFootStun ? -30.f : 0.f);
		L.T = 0.5f;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		FHumanFoot& F = R.Feet[i];
		if (R.bDriven[i] && !bDrive[i] && F.T < 1.f)
		{
			F.From = F.Pos;   // the action let go of the foot in the air: it comes down as a step
			F.FromYaw = F.Yaw;
			F.T = 0.05f;
			F.Dur = 0.3f;
		}
		R.bDriven[i] = bDrive[i];
	}

	// The walk: a slow heavy cadence. Each foot stays where it landed and swings to its next place when its turn comes.
	float Freq = (0.45f + 0.8f * FMath::Min(Speed / 980.f, 1.2f)) * St.Tempo;   // steps per second
	if (bKO)
	{
		Freq = 1.8f;   // panic: pattering away
	}
	const float SwingDur = FMath::Clamp(0.8f / Freq, 0.3f, 0.62f);
	if (!bHop && StompLeg < 0)
	{
		auto StartSwing = [&](int32 i, float Dur)
		{
			FHumanFoot& F = R.Feet[i];
			F.From = F.Pos;
			F.FromYaw = F.Yaw;
			F.T = 0.f;
			F.Dur = Dur;
		};
		if (bMoving)
		{
			if (!R.bWasMoving)
			{
				// Setting off: the foot that is further behind goes first, at once.
				const float B0 = (float)FVector::DotProduct(R.Feet[0].Pos - Home(0), VelDir), B1 = (float)FVector::DotProduct(R.Feet[1].Pos - Home(1), VelDir);
				R.GaitPhase = FMath::FloorToFloat(R.GaitPhase) + (B0 <= B1 ? 0.985f : 0.485f);
			}
			const float Old = R.GaitPhase;
			R.GaitPhase += Dt * Freq * 0.5f;
			for (int32 i = 0; i < 2; ++i)
			{
				const float Off = i == 0 ? 0.f : 0.5f;
				const bool bTurn = FMath::FloorToInt(Old + Off) != FMath::FloorToInt(R.GaitPhase + Off);
				const bool bLeftBehind = FVector::DistSquared2D(R.Feet[i].Pos, Home(i)) > FMath::Square(950.f * FK);
				if ((bTurn || bLeftBehind) && R.Feet[i].T >= 1.f && R.Feet[1 - i].T >= 0.7f)
				{
					StartSwing(i, SwingDur);
				}
			}
			R.NextIdleStep = Now + 0.35f;
		}
		else if (R.Feet[0].T >= 1.f && R.Feet[1].T >= 1.f && Now >= R.NextIdleStep)
		{
			// Standing: shuffle a foot back under the hips when he has turned or stopped mid-stride.
			float Err[2];
			for (int32 i = 0; i < 2; ++i)
			{
				Err[i] = (float)FVector::Dist2D(R.Feet[i].Pos, Home(i)) / FK + FMath::Abs(FMath::FindDeltaAngleDegrees(R.Feet[i].Yaw, Yaw)) * 5.f;
			}
			const int32 Worse = Err[1] > Err[0] ? 1 : 0;
			if (Err[Worse] > 170.f)
			{
				StartSwing(Worse, 0.46f / Tempo);
				R.NextIdleStep = Now + 0.64f / Tempo;
			}
		}
	}
	R.bWasMoving = bMoving;
	for (int32 i = 0; i < 2; ++i)
	{
		FHumanFoot& F = R.Feet[i];
		if (bDrive[i] || F.T >= 1.f)
		{
			continue;
		}
		F.T = FMath::Min(1.f, F.T + Dt / FMath::Max(0.05f, F.Dur));
		// Lands ahead of the hip by half of what the body will travel while the foot stands.
		const float Lead = bMoving ? FMath::Min(520.f, Speed * FMath::Max(0.f, 2.f / Freq - SwingDur) * 0.5f) : 0.f;
		FVector To = Home(i) + Vel * ((1.f - F.T) * F.Dur) + VelDir * (Lead * FK);
		To.Z = FloorZ;
		const float E = Ease(F.T);
		F.Pos = FMath::Lerp(F.From, To, E);
		F.Yaw = F.FromYaw + FMath::FindDeltaAngleDegrees(F.FromYaw, Yaw + (i == 0 ? -7.f : 7.f)) * E;
		const float Height = bMoving ? 80.f + FMath::Min(Speed, 1000.f) * 0.17f : 110.f;
		F.Lift = FMath::Max(FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * F.T)), 0.8f) * Height, F.Lift - 5000.f * Dt);
		const float PK = (bMoving ? FMath::Clamp(Speed / 500.f, 0.4f, 1.f) : 0.4f) * Ahead;
		F.Pitch = (F.T < 0.45f ? -26.f * FMath::Sin(PI * F.T / 0.45f) : 15.f * FMath::Sin(PI * (F.T - 0.45f) / 0.55f)) * PK;   // heel up, then toes up for the heel strike
		if (F.T >= 1.f)
		{
			F.Pos = To;
			F.Lift = 0.f;
			F.Pitch = 0.f;
			const float Strength = FMath::Clamp(Speed / 700.f, 0.4f, 1.f);
			R.DipVel -= 1200.f * Strength;
			OnFootfall(i, Strength);
		}
	}
	FVector FootB[2];
	for (int32 i = 0; i < 2; ++i)
	{
		FootB[i] = Body.InverseTransformPosition(R.Feet[i].Pos);
	}

	// ---------------------------------------------------------------------------------- weight
	// The pelvis sinks under every footfall and comes back: a soft, heavy spring.
	R.DipVel += (-R.Dip * 90.f - R.DipVel * 11.f) * Dt;
	R.Dip = FMath::Clamp(R.Dip + R.DipVel * Dt, -260.f, 120.f);
	const bool bAir0 = R.Feet[0].T < 1.f, bAir1 = R.Feet[1].T < 1.f;
	if (!bMoving && Now > R.NextShift)
	{
		R.NextShift = Now + FMath::FRandRange(3.5f, 7.f) / Tempo;
		R.ShiftTarget = (float)FMath::RandRange(-1, 1) * 70.f;   // idle: the weight goes from one leg to the other
	}
	Approach(R.IdleShift, bMoving ? 0.f : R.ShiftTarget, 1.3f, Dt);
	float SwayT = R.IdleShift;
	if (bHop)
	{
		SwayT = (float)FootB[0].Y * 0.8f;
	}
	else if (StompLeg >= 0 && T < HumanAct::StompHit)
	{
		SwayT = (float)FootB[1 - StompLeg].Y * 0.7f;   // onto the standing leg
	}
	else if (bAir0 != bAir1)
	{
		SwayT = (float)FootB[bAir0 ? 1 : 0].Y * 0.36f;
	}
	Approach(R.Sway, FMath::Clamp(SwayT, -260.f, 260.f), 6.f, Dt);
	// Hips turn with the stride; the chest turns the other way.
	Approach(R.PelvisYaw, FMath::Clamp(-Deg(FMath::Atan2((float)(FootB[1].X - FootB[0].X), 2.f * HipY)) * 0.3f, -11.f, 11.f), 9.f, Dt);
	const float HopZ = bHop ? R.Feet[0].Lift : 0.f;

	auto BuildSpine = [&](float Crouch, float Lean, float Bend, float Twist, float Side, const FVector& Off) -> FSpine
	{
		FSpine S;
		// As he bends, the hips go back: he stays over his feet.
		const float Back = FMath::Sin(FMath::DegreesToRadians(FMath::Clamp(Bend + Lean, 0.f, 85.f))) * 230.f;
		S.Pelvis = FVector(Off.X - Back, R.Sway + Off.Y, HipZ + R.Dip + HopZ - Crouch + Off.Z);
		// Twist first, about the vertical: then he bends in the direction he faces. Most of the bend is at the hips.
		S.QPelvis = RZ(R.PelvisYaw + Twist * 0.3f) * RY(Lean + Bend * 0.45f) * RX(-Side * 0.4f);
		S.Belly = S.Pelvis + S.QPelvis.RotateVector(FVector(0.f, 0.f, BellyUp));
		S.QBelly = RZ(R.PelvisYaw + Twist * 0.65f) * RY(Lean + Bend * 0.75f) * RX(-Side * 0.7f);
		S.Chest = S.Belly + S.QBelly.RotateVector(FVector(0.f, 0.f, ChestUp));
		S.QChest = RZ(Twist - R.PelvisYaw * 0.7f) * RY(Lean + Bend) * RX(-Side);
		S.Neck = S.Chest + S.QChest.RotateVector(FVector(0.f, 0.f, NeckUp));
		for (int32 s = 0; s < 2; ++s)
		{
			S.Shoulder[s] = S.Chest + S.QChest.RotateVector(FVector(0.f, s == 0 ? -ShoulderY : ShoulderY, ShoulderUp));
		}
		return S;
	};

	// ---------------------------------------------------------------------------------- posture and actions
	float CrouchT = 30.f + 30.f * R.RageW;                                     // rage: lower, hunched, wound up
	float LeanT = 1.5f + FMath::Min(Speed * 0.005f, 5.f) * Ahead + 4.f * R.RageW;
	float BendT = 10.f * R.RageW + St.Hunch;
	float TwistT = 0.f, SideT = 0.f;
	FVector PelvisOff = FVector::ZeroVector;
	FArmCtl Arm[2];
	bool bLook = false;
	FVector LookB = FVector::ZeroVector;
	float HeadShake = 0.f, HeadUpT = 0.f;

	// How deep he has to crouch and bend (0..1) for the shoulder to get within Reach of a point.
	const float CrouchMax = 1080.f, BendMax = 76.f, LungeMax = 240.f;
	const FVector FlatDir = FVector(TargetB.X, TargetB.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	auto NeedReach = [&](int32 Side, const FVector& PointB, float Reach, float Twist) -> float
	{
		auto DistAt = [&](float K)
		{
			return (float)FVector::Dist(BuildSpine(CrouchT + K * CrouchMax, LeanT, BendT + K * BendMax, Twist, 0.f, FlatDir * (LungeMax * K)).Shoulder[Side], PointB);
		};
		if (DistAt(0.f) <= Reach)
		{
			return 0.f;
		}
		// The shallowest crouch that gets there; if nothing does (too far, too high), the one that gets closest.
		float BestK = 0.f, BestD = DistAt(0.f), Prev = 0.f;
		for (int32 i = 1; i <= 4; ++i)
		{
			const float K = i * 0.25f;
			const float D = DistAt(K);
			if (D <= Reach)
			{
				float Lo = Prev, Hi = K;
				for (int32 n = 0; n < 6; ++n)
				{
					const float Mid = (Lo + Hi) * 0.5f;
					if (DistAt(Mid) > Reach) { Lo = Mid; } else { Hi = Mid; }
				}
				return Hi;
			}
			if (D < BestD) { BestD = D; BestK = K; }
			Prev = K;
		}
		return BestK;
	};
	// Facing the target with the chest, the shoulder of the working arm leading (Side 0 left, 1 right).
	auto TwistTo = [](const FVector& PB, int32 Side) { return FMath::Clamp(Deg(FMath::Atan2((float)PB.Y, (float)PB.X)) * 0.85f + (Side == 0 ? 26.f : -26.f), -80.f, 80.f); };
	const FQuat PalmDown0 = FRotationMatrix::MakeFromYZ(FVector(0.f, 0.f, -1.f), -FlatDir).ToQuat();   // left palm on the floor, fingers towards the target
	const FQuat PalmFwdUp = FRotationMatrix::MakeFromYZ(FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, -1.f)).ToQuat();   // left hand raised, palm forward

	if (A == HumanAct::Stomp)
	{
		const float Sgn = StompLeg == 0 ? -1.f : 1.f;
		const float Up = Ease(Ramp(T, 0.f, 0.36f)) * (1.f - Ramp(T, 0.46f, 0.55f));        // wound up on one leg
		const float Down = Ramp(T, 0.50f, 0.58f) * (1.f - Ease(Ramp(T, 0.75f, 1.12f)));    // the weight thrown onto the slipper
		LeanT += -7.f * Up + 13.f * Down;
		BendT += 9.f * Down;
		CrouchT += 150.f * Down - 15.f * Up;
		SideT += -Sgn * 5.f * Up;
		TwistT += -Sgn * 8.f * Up;
		for (int32 s = 0; s < 2; ++s)
		{
			const float SS = s == 0 ? -1.f : 1.f;
			Arm[s].W = FMath::Max(Up, Down * 0.85f);
			Arm[s].Rel = FMath::Lerp(FVector(100.f, SS * 600.f, -330.f), FVector(280.f, SS * 340.f, -840.f), Ramp(T, 0.46f, 0.58f));   // out for balance, then down with the slam
			Arm[s].RotW = 0.f;
			Arm[s].Curl = 1.f;
		}
		bLook = true;
		LookB = Body.InverseTransformPosition(GetStompPoint(StompLeg));
	}
	else if (A == HumanAct::Swat || A == HumanAct::Slap)
	{
		const bool bSlap = A == HumanAct::Slap;
		const int32 s = bSlap ? 0 : 1;
		const float Sgn = s == 0 ? -1.f : 1.f;
		const float Ti = bSlap ? HumanAct::SlapHit : St.SwatHit;
		const float Tw = TwistTo(TargetB, s);
		const FVector WristHit = TargetB - FlatDir * 150.f + FVector(0.f, 0.f, 50.f);
		const float K = bSlap ? NeedReach(s, WristHit, ArmReach * 0.97f, Tw) : NeedReach(s, TargetB, ArmReach * 0.9f + PaperReach, Tw);
		const float Out = 1.f - Ease(Ramp(T, Ti + 0.22f, ActDur));
		const float In = Ease(Ramp(T, 0.f, Ti * 0.92f)) * Out;                 // the body going to the target
		const float Wind = Ease(Ramp(T, 0.f, Ti * 0.5f)) * (1.f - Ramp(T, Ti * 0.6f, Ti));
		const float Strike = EaseIn(Ramp(T, Ti * 0.6f, Ti));
		CrouchT += K * CrouchMax * In;
		BendT += K * BendMax * In;
		PelvisOff += FlatDir * (LungeMax * K * In);
		TwistT += Tw * In + Sgn * 16.f * Wind;
		LeanT += -6.f * Wind + 5.f * Strike * Out;
		FArmCtl& H = Arm[s];
		H.W = Ease(Ramp(T, 0.f, Ti * 0.35f)) * Out;
		H.AbsW = Strike;
		H.bPole = true;
		if (bSlap)
		{
			H.Rel = FVector(-60.f, Sgn * 220.f, 820.f);
			H.Abs = WristHit;
			H.RotA = PalmFwdUp;
			H.RotB = PalmDown0;
			H.Pole = FVector(-0.2f, Sgn * 0.9f, 0.1f);
			H.Curl = 0.f;
		}
		else
		{
			H.Rel = FVector(-150.f, Sgn * 250.f, 720.f);   // the fist above the shoulder, the paper pointing back
			H.RotA = FRotationMatrix::MakeFromXZ(FVector(-1.f, 0.f, 0.3f), FVector(0.f, 0.f, -1.f)).ToQuat();
			H.bPaper = true;
			H.PaperP = TargetB;
			H.PaperFlat = FlatDir;
			H.Pole = FVector(-0.3f, Sgn * 0.8f, -0.3f);
			H.Curl = 1.f;
		}
		bLook = true;
		LookB = TargetB;
	}
	else if (A == HumanAct::Grab || A == HumanAct::Reach)
	{
		const bool bGrab = A == HumanAct::Grab;
		const float Ti = bGrab ? HumanAct::GrabGrip : HumanAct::ReachTouch;
		const float Tw = TwistTo(TargetB, 0);
		const FVector From(0.f, -ShoulderY, (float)Defs[UpperArmL].Joint.Z);
		const FVector To3 = (TargetB - From).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector WristAt = bGrab ? TargetB - FlatDir * 150.f + FVector(0.f, 0.f, 110.f) : TargetB - To3 * 130.f;
		const float K = NeedReach(0, WristAt, ArmReach * 0.97f, Tw);
		const float Out = 1.f - Ease(Ramp(T, bGrab ? 0.5f : 0.6f, ActDur));
		const float In = Ease(Ramp(T, 0.f, Ti * 0.9f)) * Out;
		CrouchT += K * CrouchMax * In;
		BendT += K * BendMax * In;
		PelvisOff += FlatDir * (LungeMax * K * In);
		TwistT += Tw * In;
		FArmCtl& H = Arm[0];
		H.W = Ease(Ramp(T, 0.f, Ti * 0.3f)) * Out;
		H.Rel = FVector(260.f, -160.f, -420.f);
		H.Abs = WristAt;
		H.AbsW = Ease(Ramp(T, Ti * 0.15f, Ti));
		H.RotA = H.RotB = bGrab ? PalmDown0 : FRotationMatrix::MakeFromZY(-To3, FVector(0.f, 0.f, -1.f)).ToQuat();
		H.Curl = bGrab ? 0.7f * Ramp(T, Ti - 0.04f, Ti + 0.08f) : 0.15f;
		H.bPole = true;
		H.Pole = FVector(-0.2f, -0.9f, 0.1f);
		bLook = true;
		LookB = TargetB;
	}
	else if (A == HumanAct::Throw)
	{
		// The left arm goes back over the shoulder, whips forward, lets go, follows through.
		const FVector D = (TargetB - FVector(0.f, -ShoulderY, 2600.f)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const float Tw = TwistTo(D * 1000.f, 0) - 26.f;
		const float Back = Ease(Ramp(T, 0.f, 0.2f)) * (1.f - Ramp(T, 0.22f, 0.34f));
		const float Go = Ramp(T, 0.22f, 0.36f);
		const float Out = 1.f - Ease(Ramp(T, 0.5f, ActDur));
		TwistT += (Tw * 0.6f * Go - 22.f * Back + 24.f * Go) * Out;
		LeanT += (-7.f * Back + 11.f * Go) * Out;
		FArmCtl& H = Arm[0];
		H.W = Ease(Ramp(T, 0.f, 0.1f)) * Out;
		H.Rel = FMath::Lerp(FVector(-280.f, -140.f, 480.f), FVector(D.X * 900.f, D.Y * 900.f - 80.f, D.Z * 600.f - 80.f), EaseIn(Go));
		H.RotA = H.RotB = FRotationMatrix::MakeFromZY(-D, FVector(0.f, 0.f, -1.f)).ToQuat();
		H.RotW = 0.6f;
		H.Curl = 0.6f * (1.f - Ramp(T, HumanAct::ThrowRelease - 0.03f, HumanAct::ThrowRelease + 0.05f));
		bLook = true;
		LookB = TargetB;
	}
	else if (A == HumanAct::Shake)
	{
		// Flailing while he hops: ants on him!
		const float W = Ease(Ramp(T, 0.f, 0.12f)) * (1.f - Ease(Ramp(T, 0.75f, ActDur)));
		TwistT += 16.f * FMath::Sin(C * 15.f) * W;
		SideT += 6.f * FMath::Sin(C * 11.f) * W;
		LeanT += 5.f * W;
		for (int32 s = 0; s < 2; ++s)
		{
			const float SS = s == 0 ? -1.f : 1.f;
			Arm[s].W = W;
			Arm[s].Rel = FVector(150.f + 220.f * FMath::Sin(C * 17.f + s), SS * (430.f + 160.f * FMath::Sin(C * 13.f + 2.f * s)), 150.f + 480.f * FMath::Sin(C * 19.f + 3.f * s));
			Arm[s].RotW = 0.f;
		}
		HeadShake = 22.f * FMath::Sin(C * 14.f) * W;
	}
	else if (A == HumanAct::Peer)
	{
		// Down on his haunches, hands on the knees, the face right at the floor: where did it go?
		const float W = Ease(Ramp(T, 0.f, 0.6f)) * (1.f - Ease(Ramp(T, ActDur - 0.6f, ActDur)));
		Approach(R.PeerW, W, 20.f, Dt);
		CrouchT += 900.f * W;
		BendT += 56.f * W;
		TwistT += (TwistTo(TargetB, 0) - 26.f) * 0.6f * W;
		SideT += 5.f * FMath::Sin(C * 0.9f) * W;
		for (int32 s = 0; s < 2; ++s)
		{
			const float SS = s == 0 ? -1.f : 1.f;
			Arm[s].W = W;
			Arm[s].Abs = R.Knee[s] + FVector(60.f, SS * 30.f, 130.f);
			Arm[s].AbsW = 1.f;
			Arm[s].RotA = Arm[s].RotB = FRotationMatrix::MakeFromYZ(FVector(0.f, 0.f, -SS * -1.f), FVector(-1.f, 0.f, 0.4f)).ToQuat();
			Arm[s].RotW = 0.7f;
			Arm[s].bPole = true;
			Arm[s].Pole = FVector(-0.2f, SS * 0.9f, 0.3f);
		}
		bLook = true;
		LookB = TargetB;
	}
	else if (A == HumanAct::Sit || A == HumanAct::Rise)
	{
		// Onto a seat (grandma's armchair behind her, the floor under the kid): the pelvis goes back and down to it, the feet stay where they stand.
		const float Dur = HumanAct::SitTime * St.SitSlow;
		const float W = A == HumanAct::Sit ? Ease(Ramp(T, 0.f, Dur)) : 1.f - Ease(Ramp(T, 0.f, Dur));
		const float Moving = FMath::Sin(PI * W);   // on the way down / up: leaning forward over the knees
		const float SeatZ = FMath::Max(0.f, (float)TargetB.Z);
		R.SitW = W;
		CrouchT += FMath::Max(0.f, HipZ - (SeatZ + 190.f)) * W;
		PelvisOff = FVector(TargetB.X, TargetB.Y, 0.f) * W;
		BendT += 30.f * Moving - St.Hunch * 0.5f * W;
		LeanT += -9.f * W * (1.f - Moving);
		const float Settled = Ramp(W, 0.85f, 1.f);
		const uint8 How = Act.Arg;
		const FVector Lap(TargetB.X + 330.f, 0.f, SeatZ + 330.f);
		for (int32 s = 0; s < 2 && Settled > 0.f; ++s)
		{
			const float SS = s == 0 ? -1.f : 1.f;
			FArmCtl& H = Arm[s];
			H.W = Settled;
			H.AbsW = 1.f;
			H.RotW = 0.f;
			H.bPole = true;
			H.Pole = FVector(-0.2f, SS * 0.9f, -0.2f);
			if (How == 0)
			{
				// Knitting: the hands close together over the lap, the needles going.
				H.Abs = Lap + FVector(60.f + 26.f * FMath::Sin(C * 7.f + s * PI), SS * (95.f + 18.f * FMath::Cos(C * 7.f + s * PI)), 150.f + 22.f * FMath::Sin(C * 3.5f + s));
				H.Curl = 0.8f;
			}
			else if (How == 1)
			{
				H.Abs = Lap + FVector(40.f, SS * 150.f, 20.f);   // dozing: hands in the lap
				H.Curl = 0.35f;
			}
			else
			{
				// Playing on the floor: patting it in front of him, one hand after the other.
				H.Abs = FVector(TargetB.X + 620.f, SS * 210.f, 70.f + 230.f * FMath::Max(0.f, FMath::Sin(C * 5.f + s * PI)));
				H.Curl = 0.1f;
				H.Pole = FVector(-0.2f, SS * 0.9f, 0.4f);
			}
		}
		if (How == 1 && Settled > 0.f)
		{
			HeadUpT = (-34.f + 5.f * FMath::Sin(C * 0.7f)) * Settled;   // nodding off
		}
		else if (How == 0 && Settled > 0.f && !bLook && FVector(FolkLook).IsZero())
		{
			bLook = true;
			LookB = Lap + FVector(120.f, 0.f, 100.f);   // eyes on the knitting
		}
	}
	else if (A == HumanAct::Sweep)
	{
		// The broom: drawn back to her right (the warning), then one long stroke across the floor in front of her, right to left.
		const float Back = Ease(Ramp(T, 0.f, HumanAct::SweepHit * 0.6f));
		const float Go = Ease(Ramp(T, HumanAct::SweepHit - 0.1f, HumanAct::SweepHit + 0.4f));
		const float Out = 1.f - Ease(Ramp(T, 1.75f, ActDur));
		const float Base = Deg(FMath::Atan2((float)TargetB.Y, (float)TargetB.X));
		const float Ang = FMath::DegreesToRadians(Base + FMath::Lerp(0.f, FMath::Lerp(62.f, -62.f, Go), Back));
		const float Dist = FMath::Clamp((float)TargetB.Size2D(), 1100.f, 1900.f);
		const FVector HeadP(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 30.f + 260.f * Back * (1.f - Ramp(T, HumanAct::SweepHit * 0.6f, HumanAct::SweepHit)));
		BendT += 20.f * Out; CrouchT += 150.f * Out;
		TwistT += FMath::Clamp(Deg(Ang) * 0.5f, -45.f, 45.f) * Out;
		FArmCtl& Rh = Arm[1];
		Rh.W = Ease(Ramp(T, 0.f, 0.3f)) * Out;
		Rh.AbsW = 1.f;
		Rh.bPaper = true;
		Rh.PaperLen = 1950.f;
		Rh.PaperP = HeadP;
		Rh.PaperFlat = FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		Rh.RotA = FRotationMatrix::MakeFromXZ(FVector(1.f, 0.f, -0.6f), FVector(0.f, 0.f, 1.f)).ToQuat();
		Rh.bPole = true;
		Rh.Pole = FVector(-0.3f, 0.8f, -0.3f);
		Rh.Curl = 1.f;
		// The left hand lower down the handle.
		const FVector Comfy = R.Shoulder[1] + Rh.PaperFlat * 430.f + FVector(0.f, 0.f, 120.f);
		const FVector UpB = (Comfy - HeadP).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		FArmCtl& Lh = Arm[0];
		Lh.W = Rh.W;
		Lh.Abs = HeadP + UpB * 1250.f;
		Lh.AbsW = 1.f;
		Lh.RotW = 0.f;
		Lh.Curl = 0.9f;
		Lh.bPole = true;
		Lh.Pole = FVector(-0.2f, -0.9f, 0.1f);
		bLook = true;
		LookB = HeadP;
	}
	if (A != HumanAct::Sit && A != HumanAct::Rise)
	{
		R.SitW = 0.f;
	}
	if (R.SprayW > 0.01f)
	{
		// Bug spray: the right arm out along the aim, the can upright in the fist.
		const FVector DirB = Body.InverseTransformVectorNoScale(GetSprayDirection());
		const FQuat AimQ = DirB.ToOrientationQuat();
		FArmCtl& H = Arm[1];
		H = FArmCtl();
		H.W = R.SprayW;
		H.Abs = SprayNozzleBody() - AimQ.RotateVector(FVector(190.f, 0.f, 265.f));
		H.AbsW = 1.f;
		H.RotA = H.RotB = FRotationMatrix::MakeFromZX(-DirB, AimQ.GetUpVector()).ToQuat();
		H.bPole = true;
		H.Pole = FVector(-0.2f, 0.6f, -0.75f);
		H.Curl = 1.f;
		if (!bLook)
		{
			bLook = true;
			LookB = SprayNozzleBody() + DirB * 1500.f;
		}
	}
	if (R.HoldW > 0.01f && Arm[0].W < 0.02f)
	{
		// Food in the left hand, carried in front of him on the open palm.
		FArmCtl& H = Arm[0];
		H.W = R.HoldW;
		H.Abs = FVector(470.f, -230.f, 1900.f - R.Crouch * 0.6f);
		H.AbsW = 1.f;
		H.RotA = H.RotB = FRotationMatrix::MakeFromYZ(FVector(0.f, 0.f, 1.f), FVector(-1.f, 0.f, 0.f)).ToQuat();
		H.Curl = 0.3f;
		H.bPole = true;
		H.Pole = FVector(-0.4f, -0.7f, -0.6f);
	}
	if (R.StunW > 0.01f)
	{
		const float W = R.StunW;
		const FQuat PalmIn = FRotationMatrix::MakeFromYZ(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, -1.f)).ToQuat();
		if (StunKind == 1)
		{
			// It landed on his foot: he hops on the other one, holding it.
			BendT += 34.f * W;
			LeanT += 6.f * W;
			HeadUpT = 28.f * W;
			HeadShake = 14.f * FMath::Sin(C * 9.f) * W;
			for (int32 s = 0; s < 2; ++s)
			{
				Arm[s] = FArmCtl();
				Arm[s].W = W;
				Arm[s].Abs = R.SegPos[FootR] + FVector(70.f, s == 0 ? -110.f : 110.f, 60.f);
				Arm[s].AbsW = 1.f;
				Arm[s].RotW = 0.f;
				Arm[s].Curl = 0.6f;
			}
		}
		else
		{
			// It landed on his head: he reels, both hands on it.
			PelvisOff = FVector(70.f * FMath::Sin(C * 2.3f), 90.f * FMath::Sin(C * 3.1f), 0.f) * W;
			SideT += 8.f * FMath::Sin(C * 3.1f) * W;
			LeanT += (3.f + 6.f * FMath::Sin(C * 2.3f + 1.f)) * W;
			TwistT += 9.f * FMath::Sin(C * 1.7f) * W;
			CrouchT += 120.f * W;
			HeadShake = 12.f * FMath::Sin(C * 4.3f) * W;
			for (int32 s = 0; s < 2; ++s)
			{
				Arm[s] = FArmCtl();
				Arm[s].W = W;
				Arm[s].Abs = R.SegPos[Head] + R.SegRot[Head].RotateVector(FVector(110.f, s == 0 ? -250.f : 250.f, 300.f));
				Arm[s].AbsW = 1.f;
				Arm[s].RotA = Arm[s].RotB = PalmIn;
				Arm[s].RotW = 0.8f;
				Arm[s].bPole = true;
				Arm[s].Pole = FVector(0.3f, s == 0 ? -0.9f : 0.9f, 0.2f);
				Arm[s].Curl = 0.2f;
			}
		}
	}
	if (R.PanicW > 0.01f)
	{
		// Bitten into a panic: hands in the air, running away.
		const float W = R.PanicW;
		LeanT += -8.f * W;
		SideT += 5.f * FMath::Sin(C * 6.2f) * W;
		HeadShake = 26.f * FMath::Sin(C * 8.5f) * W;
		HeadUpT = 12.f * W;
		for (int32 s = 0; s < 2; ++s)
		{
			const float SS = s == 0 ? -1.f : 1.f;
			Arm[s] = FArmCtl();
			Arm[s].W = W;
			Arm[s].Rel = FVector(120.f + 110.f * FMath::Sin(C * 7.f + 2.f * s), SS * (300.f + 140.f * FMath::Sin(C * 5.3f + s)), 880.f + 130.f * FMath::Sin(C * 9.1f + 3.f * s));
			Arm[s].RotA = PalmFwdUp;
			Arm[s].RotW = s == 0 ? 0.8f : 0.f;
		}
	}
	if (A != HumanAct::Peer)
	{
		Approach(R.PeerW, 0.f, 6.f, Dt);
	}

	// Eased on top of the eased targets: nothing on a body this size starts or stops at once.
	Approach(R.Crouch, CrouchT, 13.f, Dt);
	Approach(R.Bend, BendT, 13.f, Dt);
	Approach(R.Lean, LeanT, 9.f, Dt);
	Approach(R.Twist, TwistT, 11.f, Dt);
	Approach(R.Side, SideT, 9.f, Dt);

	// ---------------------------------------------------------------------------------- spine and legs
	FSpine S = BuildSpine(R.Crouch, R.Lean, R.Bend, R.Twist, R.Side, PelvisOff);
	FVector Ankle[2];
	FQuat QFoot[2];
	float Drop = 0.f;
	for (int32 i = 0; i < 2; ++i)
	{
		const FHumanFoot& F = R.Feet[i];
		QFoot[i] = RZ(FMath::FindDeltaAngleDegrees(Yaw, F.Yaw)) * RY(-F.Pitch);
		const FVector Sole((float)FootB[i].X, (float)FootB[i].Y, (float)FootB[i].Z + F.Lift);
		Ankle[i] = Sole + QFoot[i].RotateVector(FVector(-SoleFwd, 0.f, AnkleZ));
		// The pelvis never goes higher than the legs can reach: a long stride pulls it down.
		const FVector Hip = S.Pelvis + S.QPelvis.RotateVector(FVector(0.f, i == 0 ? -HipY : HipY, 0.f));
		const float Flat = (float)FVector::Dist2D(Hip, Ankle[i]);
		const float MaxZ = (float)Ankle[i].Z + FMath::Sqrt(FMath::Max(0.f, LegReach * LegReach - Flat * Flat));
		Drop = FMath::Max(Drop, (float)Hip.Z - MaxZ);
	}
	Drop = FMath::Min(Drop, 700.f);
	if (Drop > 0.f)
	{
		const FVector Down(0.f, 0.f, -Drop);
		S.Pelvis += Down; S.Belly += Down; S.Chest += Down; S.Neck += Down; S.Shoulder[0] += Down; S.Shoulder[1] += Down;
	}
	auto Set = [&](int32 Seg, const FVector& P, const FQuat& Q)
	{
		if (P.ContainsNaN() || Q.ContainsNaN())
		{
			return;
		}
		R.SegPos[Seg] = P;
		R.SegRot[Seg] = Q.GetNormalized();
		Segs[Seg]->SetRelativeLocationAndRotation(P, R.SegRot[Seg]);
	};
	const float Breath = FMath::Sin(R.Breath);
	Set(Pelvis, S.Pelvis, S.QPelvis);
	Set(Belly, S.Belly + S.QBelly.RotateVector(FVector(5.f * Breath, 0.f, 0.f)), S.QBelly);
	Set(Chest, S.Chest + S.QChest.RotateVector(FVector(0.f, 0.f, 7.f * Breath)), S.QChest * RY(-0.8f * Breath));
	for (int32 i = 0; i < 2; ++i)
	{
		const float Sgn = i == 0 ? -1.f : 1.f;
		const FVector Hip = S.Pelvis + S.QPelvis.RotateVector(FVector(0.f, Sgn * HipY, 0.f));
		FVector Tgt = Ankle[i];
		FVector Pole;
		// The knee points where the hips and the foot point, a little outwards.
		const FVector KneeHint = S.QPelvis.RotateVector(FVector(1.f, Sgn * 0.14f, 0.f)) + QFoot[i].RotateVector(FVector(0.6f, 0.f, 0.f));
		const FVector Knee = TwoBone(Hip, Tgt, LThigh, LShin, KneeHint, FVector::ForwardVector, Pole);
		Set(i == 0 ? ThighL : ThighR, Hip, BoneRot((Knee - Hip).GetSafeNormal(), Pole));
		Set(i == 0 ? ShinL : ShinR, Knee, BoneRot((Tgt - Knee).GetSafeNormal(), Pole));
		Set(i == 0 ? FootL : FootR, Tgt, QFoot[i]);
		R.Knee[i] = Knee;
	}

	// ---------------------------------------------------------------------------------- head
	// Which way the shoulders face (from the shoulder line: an Euler yaw goes wild when he is bent double).
	const FVector Across = S.QChest.RotateVector(FVector::YAxisVector);
	const float ChestYaw = Deg(FMath::Atan2(-(float)Across.X, (float)Across.Y));
	float HeadYawT = 0.f, HeadPitchT = -8.f;
	if (!bLook && Fig != HumanFig::Dad && !FVector(FolkLook).IsZero())
	{
		// The family look at what has caught their attention: the head turns before the body does.
		bLook = true;
		LookB = Body.InverseTransformPosition(FVector(FolkLook));
	}
	if (bLook)
	{
		const FVector D = LookB - (S.Neck + FVector(0.f, 0.f, HeadUp + 150.f));
		// (Right under his nose the direction means little: the head only turns as far as the thing is out to the side.)
		HeadYawT = FMath::Clamp(FMath::FindDeltaAngleDegrees(ChestYaw, Deg(FMath::Atan2((float)D.Y, (float)D.X))), -75.f, 75.f) * FMath::Clamp((float)D.Size2D() / 700.f, 0.f, 1.f);
		HeadPitchT = Deg(FMath::Atan2((float)D.Z, (float)D.Size2D()));
	}
	else
	{
		const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
		const bool bPlayer = IsPlayerControlled();
		if (Fig == HumanFig::Dad && !bPlayer && GS && GS->HumanGoal != AntHumanGoal::Idle && GS->HumanGoal != AntHumanGoal::Stunned && !FVector(GS->HumanGoalLoc).IsNearlyZero())
		{
			// The AI's plan is replicated for the HUD: his eyes are on what he is after.
			const FVector D = Body.InverseTransformPosition(FVector(GS->HumanGoalLoc)) - (S.Neck + FVector(0.f, 0.f, HeadUp + 150.f));
			if (D.Size2D() > 200.f)
			{
				HeadYawT = FMath::Clamp(FMath::FindDeltaAngleDegrees(ChestYaw, Deg(FMath::Atan2((float)D.Y, (float)D.X))), -75.f, 75.f);
			}
			HeadPitchT = Deg(FMath::Atan2((float)D.Z, (float)D.Size2D()));
			if (GS->HumanGoal == AntHumanGoal::Look)
			{
				HeadYawT = 50.f * FMath::Sin(C * 1.3f);   // looking around for whoever made the noise
				HeadPitchT = -22.f + 10.f * FMath::Sin(C * 0.9f);
			}
		}
		else
		{
			HeadPitchT = FMath::Clamp(FRotator::NormalizeAxis((float)GetBaseAimRotation().Pitch), -60.f, 25.f);
			if (!bPlayer && !bKO)
			{
				// Nothing to do: his eyes wander over the floor.
				if (Now > R.NextGlance)
				{
					R.NextGlance = Now + FMath::FRandRange(1.8f, 4.5f) / Tempo;
					R.Glance = FMath::FRandRange(-45.f, 45.f);
					R.GlancePitch = FMath::FRandRange(-16.f, 0.f);
				}
				HeadYawT = bMoving ? R.Glance * 0.4f : R.Glance;
				HeadPitchT += R.GlancePitch;
			}
		}
	}
	HeadPitchT += HeadUpT - 6.f * R.RageW + St.Hunch * 0.7f;
	HeadPitchT = FMath::Clamp(HeadPitchT, -70.f, FMath::Min(30.f, 55.f - (R.Lean + R.Bend)));   // the neck only goes so far back
	Approach(R.HeadYaw, HeadYawT, 4.5f * Tempo, Dt);
	Approach(R.HeadPitch, HeadPitchT, 4.5f * Tempo, Dt);
	const FQuat QHead = RZ(ChestYaw + R.HeadYaw + HeadShake) * RY(-R.HeadPitch) * RX(-R.Side * 0.3f);
	const FQuat QNeck = FQuat::Slerp(S.QChest, QHead, 0.45f);
	Set(Neck, S.Neck + S.QChest.RotateVector(FVector(0.f, 0.f, 7.f * Breath)), QNeck);
	Set(Head, R.SegPos[Neck] + QNeck.RotateVector(FVector(0.f, 0.f, HeadUp)), QHead);

	// ---------------------------------------------------------------------------------- arms
	const FQuat G = RZ(ChestYaw);   // arms hang by gravity, turned with the chest
	const float Shrug = 9.f * FMath::Max(0.f, Breath) + 45.f * R.RageW + 60.f * R.StunW;
	const float Driven = (bDrive[0] || bDrive[1]) ? 0.3f : 1.f;
	for (int32 s = 0; s < 2; ++s)
	{
		const float Sgn = s == 0 ? -1.f : 1.f;
		const FVector Sh = S.Shoulder[s] + FVector(0.f, 0.f, Shrug);
		// At rest the arm hangs a little out from the body and swings against the leg of its side.
		const float Sw = FMath::Clamp((float)(FootB[1 - s].X - FootB[s].X) / 900.f, -1.f, 1.f) * Driven;
		const float SwingDeg = Sw * (10.f + 14.f * FMath::Min(1.f, Speed / 700.f));
		const float Abd = 9.f + 9.f * R.RageW + St.ArmsOut;
		const float Elbow = 14.f + FMath::Max(0.f, Sw) * 18.f + 30.f * R.RageW;
		const FVector UDir = G.RotateVector((RX(Sgn * Abd) * RY(-SwingDeg)).RotateVector(FVector(0.f, 0.f, -1.f)));
		const FVector FDir = G.RotateVector((RX(Sgn * Abd * 0.5f) * RY(-(SwingDeg + Elbow))).RotateVector(FVector(0.f, 0.f, -1.f)));
		FVector Wrist = Sh + UDir * LUpper + FDir * LFore;
		FVector PoleHint = UDir * LUpper - (Wrist - Sh) * 0.5f;
		if (Wrist.Z < 190.f)
		{
			// Crouched: the hanging hand stays off the floor, a little forward (the elbow bends).
			Wrist.X += (190.f - (float)Wrist.Z) * 0.5f;
			Wrist.Z = 190.f;
			PoleHint = G.RotateVector(FVector(-0.3f, Sgn, 0.3f)) * 300.f;
		}
		const FArmCtl& Ctl = Arm[s];
		FQuat ActRot = FQuat::Identity;
		if (Ctl.W > 0.001f)
		{
			FVector AbsPos = Ctl.Abs;
			FQuat RotB = Ctl.RotB;
			if (Ctl.bPaper)
			{
				// The fist stops a newspaper's length short of the target, on the side of a comfortable spot in front of the shoulder.
				const FVector Comfy = Sh + Ctl.PaperFlat * 430.f + FVector(0.f, 0.f, 120.f);
				const FVector Back = (Comfy - Ctl.PaperP).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
				AbsPos = Ctl.PaperP + Back * (Ctl.PaperLen > 0.f ? Ctl.PaperLen : PaperReach);
				RotB = FromXZ(-Back, Sh - AbsPos, FVector(0.f, 0.f, 1.f));
			}
			const FVector ActPos = FMath::Lerp(Sh + G.RotateVector(Ctl.Rel), AbsPos, Ctl.AbsW);
			Wrist = FMath::Lerp(Wrist, ActPos, Ctl.W);
			ActRot = FQuat::Slerp(Ctl.RotA, RotB, Ctl.AbsW);
			if (Ctl.bPole)
			{
				PoleHint = FMath::Lerp(PoleHint, G.RotateVector(Ctl.Pole) * 300.f, Ctl.W);
			}
		}
		FVector Pole;
		const FVector ElbowP = TwoBone(Sh, Wrist, LUpper, LFore, PoleHint, G.RotateVector(FVector(-1.f, Sgn * 0.5f, -0.3f)), Pole);
		const FVector ForeDir = (Wrist - ElbowP).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f));
		Set(s == 0 ? UpperArmL : UpperArmR, Sh, BoneRot((ElbowP - Sh).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f)), -Pole));
		Set(s == 0 ? ForearmL : ForearmR, ElbowP, BoneRot(ForeDir, -Pole));
		const FQuat RestRot = BoneRot(ForeDir, G.RotateVector(FVector(1.f, 0.f, 0.3f)) - Pole * 0.4f);   // thumb forward, palm to the thigh
		const FQuat HandQ = FQuat::Slerp(RestRot, ActRot, FMath::Clamp(Ctl.W * Ctl.RotW, 0.f, 1.f)).GetNormalized();
		Set(s == 0 ? HandL : HandR, Wrist, HandQ);
		// The right fist is always closed on the newspaper or the can; the left one closes with rage.
		const float Curl = (s == 1 && Fig != HumanFig::Kid) ? 1.f : FMath::Clamp(FMath::Lerp(0.12f + 0.75f * R.RageW, Ctl.Curl, Ctl.W), 0.f, 1.f);
		Set(s == 0 ? FingersL : FingersR, Wrist + HandQ.RotateVector(FVector(0.f, 0.f, -LKnuckle)), HandQ * RX(-Sgn * 82.f * Curl));
		R.Shoulder[s] = Sh;
	}
	R.bPosed = true;
}

// ---------------------------------------------------------------------------------------------
// What the body does to the room (cosmetic, local) and to the dev script
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::OnFootfall(int32 Leg, float Strength)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The foot is down NOW: the thud, a puff from under the slipper, the floor jumping under any ant close by.
	const FVector At = Rig.Feet[Leg & 1].Pos;
	AntSfx::Play(this, AntSfx::HumanStep, At, (0.5f + 0.45f * Strength) * FMath::Min(1.f, FigScale + 0.15f), FMath::FRandRange(0.9f, 1.05f) / FMath::Sqrt(FigScale));
	AntFx::LandDust(this, At + FVector(0.f, 0.f, 6.f), FVector::UpVector, (150.f + 130.f * Strength) * FigScale);
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn()))
		{
			const float D = (float)FVector::Dist(Me->GetActorLocation(), At);
			Me->AddTrauma(FMath::Clamp(1.f - D / (1500.f * FigScale), 0.f, 1.f) * 0.22f * Strength * FigScale);
		}
	}
}

void AHumanCharacter::BodyEffects(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || !bNewBody)
	{
		return;
	}
	if (Fig != HumanFig::Dad)
	{
		// Grandma: the swatter while she is up and about, the broom while she sweeps, the needles while she knits.
		const bool bSweep = Act.Kind == HumanAct::Sweep && ActTime() < 2.3f;
		const bool bSat = Rig.SitW > 0.5f;
		const bool bKnit = bSat && Act.Kind == HumanAct::Sit && Act.Arg == 0;
		if (SwatterPivot && SwatterPivot->IsVisible() != (!bSweep && !bSat)) { SwatterPivot->SetVisibility(!bSweep && !bSat, true); }
		if (BroomPivot && BroomPivot->IsVisible() != bSweep) { BroomPivot->SetVisibility(bSweep, true); }
		for (UStaticMeshComponent* N : NeedleArt)
		{
			if (N && N->IsVisible() != bKnit) { N->SetVisibility(bKnit); }
		}
		return;
	}
	const bool bCan = Rig.SprayW >= 0.35f;
	if (PaperArt && PaperArt->IsVisible() == bCan)
	{
		PaperArt->SetVisibility(!bCan);
	}
	if (SprayArt && SprayArt->IsVisible() != bCan)
	{
		SprayArt->SetVisibility(bCan);
	}
	// Hiss while spraying.
	if (bSpraying && !SprayAudio)
	{
		if (USoundBase* Hiss = AntSfx::Get(AntSfx::Spray))
		{
			SprayAudio = UGameplayStatics::SpawnSoundAttached(Hiss, Segs[HumanSeg::HandR], NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 1.2f);
		}
	}
	if (SprayAudio)
	{
		if (bSpraying && !SprayAudio->IsPlaying()) { SprayAudio->Play(); }
		if (!bSpraying && SprayAudio->IsPlaying()) { SprayAudio->Stop(); }
	}
	// The cloud comes out of the can once the arm is up: puffs shot down the cone that hurts.
	if (bSpraying && Rig.SprayW > 0.6f)
	{
		const FVector Origin = GetSprayOrigin();
		const FVector Dir = GetSprayDirection();
		Rig.SprayFx += DeltaSeconds * 26.f;
		int32 Burst = 0;
		while (Rig.SprayFx >= 1.f && Burst++ < 4)
		{
			Rig.SprayFx -= 1.f;
			AntFx::SprayPuff(this, Origin, Dir, GetSprayRange());
		}
		Rig.SprayFx = FMath::Min(Rig.SprayFx, 1.f);
	}
}

FString AHumanCharacter::DevDescribe() const
{
	const FHumanRig& R = Rig;
	const FTransform B = BodyFrame();
	const FVector PL = B.InverseTransformPosition(GetPalmLocation(0)), PR = B.InverseTransformPosition(GetPalmLocation(1));
	const FVector F0 = B.InverseTransformPosition(R.Feet[0].Pos), F1 = B.InverseTransformPosition(R.Feet[1].Pos);
	return FString::Printf(TEXT("at %s jointed=%d act=%d t=%.2f speed=%.0f rage=%d stun=%d ko=%d spray=%d | footL (%.0f,%.0f) lift %.0f T %.2f | footR (%.0f,%.0f) lift %.0f T %.2f | pelvisZ %.0f crouch %.0f bend %.0f | palmL (%.0f,%.0f,%.0f) palmR (%.0f,%.0f,%.0f) | head yaw %.0f pitch %.0f"),
		*GetActorLocation().ToCompactString(), bNewBody ? 1 : 0, (int32)Act.Kind, ActTime(), GetVelocity().Size2D(), (int32)Rage, bStunned ? 1 : 0, bKnockedOut ? 1 : 0, bSpraying ? 1 : 0,
		F0.X, F0.Y, R.Feet[0].Lift, R.Feet[0].T, F1.X, F1.Y, R.Feet[1].Lift, R.Feet[1].T,
		R.SegPos[HumanSeg::Pelvis].Z, R.Crouch, R.Bend, PL.X, PL.Y, PL.Z, PR.X, PR.Y, PR.Z, R.HeadYaw, R.HeadPitch);
}
