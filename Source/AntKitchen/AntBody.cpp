#include "AntBody.h"
#include "AntRagdoll.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntTypes.h"
#include "AntGibs.h"
#include "AntFx.h"
#include "AntChain.h"
#include "AntKitchenGameMode.h"
#include "NpcBody.h"
#include "KitchenLoot.h"
#include "KitchenInteractables.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "AntCarry.h"

#include "AntBodyData.inl"

namespace
{
	enum : uint8 { M_Ground, M_Air, M_Tumble, M_Back, M_Flip, M_Held, M_Carried, M_Chain };

	const FLinearColor Hemolymph(0.62f, 0.75f, 0.12f);

	/**
	 * A damped spring towards Target. Freq in Hz; Damp 1 = no overshoot.
	 * Stable at any step: a far-away ant is updated every 4th frame, and at 60 fps that is a step of 0.07 s, where a plain
	 * explicit spring of 8 Hz blows up (it did: the body flew off to infinity and the renderer got NaNs). Small steps are
	 * integrated as before, long ones are cut in up to four, and what is still too long is solved implicitly.
	 */
	template <typename T>
	inline void SpringT(T& X, T& Vel, const T& Target, float Freq, float Damp, float Dt)
	{
		const float W = 2.f * PI * Freq;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt32(W * Dt / 0.7f), 1, 4);
		const float Hs = Dt / Steps;
		for (int32 i = 0; i < Steps; ++i)
		{
			if (W * Hs <= 0.7f)
			{
				Vel += ((Target - X) * (W * W) - Vel * (2.f * Damp * W)) * Hs;
			}
			else
			{
				Vel = (Vel + (Target - X) * (Hs * W * W)) / (1.f + 2.f * Damp * W * Hs + W * W * Hs * Hs);
			}
			X += Vel * Hs;
		}
	}
	inline void Spring(float& X, float& Vel, float Target, float Freq, float Damp, float Dt) { SpringT(X, Vel, Target, Freq, Damp, Dt); }
	inline void Spring(FVector& X, FVector& Vel, const FVector& Target, float Freq, float Damp, float Dt) { SpringT(X, Vel, Target, Freq, Damp, Dt); }
	inline float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	inline float Frac(float X) { return X - FMath::FloorToFloat(X); }
	inline float Bump(float T, float A, float B, float C, float D)
	{
		return Smooth((T - A) / FMath::Max(1.e-4f, B - A)) * (1.f - Smooth((T - C) / FMath::Max(1.e-4f, D - C)));
	}
}

// ---------------------------------------------------------------------------------------------
// The pieces
// ---------------------------------------------------------------------------------------------

namespace AntRig
{

bool Enabled()
{
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("AntOldAnt"));
	return !bOld;
}

const FAntBodyArt& Art()
{
	// NpcRig::LoadMesh roots what it loads: this cache outlives the map (the menu reloads it).
	static FAntBodyArt A;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const TCHAR* Dir = TEXT("/Game/Ant");
		A.Head = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Head"));
		A.Jaw = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Jaw"));
		A.Thorax = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Thorax"));
		A.Petiole = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Petiole"));
		A.Gaster = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Gaster"));
		A.Tibia = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Tibia"));
		A.Tarsus = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_Tarsus"));
		A.FlagA = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_FlagA"));
		A.FlagB = NpcRig::LoadMesh(Dir, TEXT("SM_AntB_FlagB"));
		A.Coxa = AntArt::Get(TEXT("SM_Ant_Coxa"));
		A.Femur = AntArt::Get(TEXT("SM_Ant_Femur"));
		A.Scape = AntArt::Get(TEXT("SM_Ant_Antenna_Scape"));
		A.bValid = A.Head && A.Jaw && A.Thorax && A.Petiole && A.Gaster && A.Tibia && A.Tarsus && A.FlagA && A.FlagB
			&& A.Femur.IsValid() && A.Coxa.IsValid() && A.Scape.IsValid() && AntArt::Get(TEXT("SM_Ant_Body")).IsValid();
		UE_LOG(LogTemp, Display, TEXT("ANTBODY art %s"), A.bValid ? TEXT("ready: the jointed ant") : TEXT("MISSING: the old ant is used"));
	}
	return A;
}

FVector HipLocal(int32 Leg)
{
	// the three sockets on the flank of the thorax (front to back), left then right: the order of AAntCharacter::LostLegs
	static const FVector Hips[3] = { FVector(8.3f, 6.6f, 3.4f), FVector(2.6f, 7.0f, 2.8f), FVector(-2.2f, 5.8f, 3.6f) };
	const int32 Pair = FMath::Clamp(Leg / 2, 0, 2);
	const float Side = (Leg & 1) ? 1.f : -1.f;
	return FVector(Hips[Pair].X, Hips[Pair].Y * Side, Hips[Pair].Z);
}

FVector HomeLocal(int32 Leg)
{
	static const FVector Homes[3] = { FVector(40.f, 30.f, 0.f), FVector(3.f, 44.f, 0.f), FVector(-47.f, 33.f, 0.f) };
	const int32 Pair = FMath::Clamp(Leg / 2, 0, 2);
	const float Side = (Leg & 1) ? 1.f : -1.f;
	return FVector(Homes[Pair].X, Homes[Pair].Y * Side, 0.f);
}

FVector FeelerLocal(int32 Side)
{
	return FVector(15.5f, 5.2f * (Side ? 1.f : -1.f), 2.2f);
}

FVector Joint(int32 Piece)
{
	switch (Piece)
	{
	case 1: return AntB_Head;
	case 2: return AntB_Jaw;
	case 3: return AntB_Petiole;
	case 4: return AntB_Gaster;
	default: return AntB_Thorax;
	}
}

void PlaceLimb(UStaticMeshComponent* C, const FVector& A, const FVector& B, const FVector& Up, float Across, float Natural)
{
	if (!C)
	{
		return;
	}
	const FVector D = B - A;
	const float Len = FMath::Max(0.01f, (float)D.Size());
	const float S = Across / FMath::Max(1.f, Natural);
	C->SetWorldTransform(FTransform(NpcRig::AlongX(D, Up), A, FVector(Len / 100.f, S, S)));
}

UStaticMeshComponent* AddPiece(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bAbsolute)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	if (Mat)
	{
		C->SetMaterial(0, Mat);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(Parent);
	if (bAbsolute)
	{
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
	}
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

} // namespace AntRig

UAntBodyComponent* UAntBodyComponent::Build(AAntCharacter* InAnt, USceneComponent* Parent)
{
	if (!InAnt || !Parent || !AntRig::Enabled() || !AntRig::Art().bValid)
	{
		return nullptr;
	}
	UAntBodyComponent* C = NewObject<UAntBodyComponent>(InAnt);
	C->Ant = InAnt;
	C->RegisterComponent();
	C->BuildPieces(Parent);
	return C;
}

void UAntBodyComponent::BuildPieces(USceneComponent* Parent)
{
	const FAntBodyArt& Art = AntRig::Art();
	static const TCHAR* MatFrom[AntMat::Count] = { TEXT("SM_Ant_Body"), TEXT("SM_Ant_Coxa"), TEXT("SM_Ant_Femur"), TEXT("SM_Ant_Tibia"), TEXT("SM_Ant_Antenna_Scape"), TEXT("SM_Ant_Antenna_Flagellum") };
	Mats.SetNum(AntMat::Count);
	for (int32 i = 0; i < AntMat::Count; ++i)
	{
		// One instance per ant and look (not per piece): the colony tint is its "Tint" parameter, set by AAntCharacter::OnRep_BodyColor.
		UMaterialInterface* Base = AntArt::Get(MatFrom[i]).Material;
		Mats[i] = Base ? UMaterialInstanceDynamic::Create(Base, GetTransientPackage()) : nullptr;
		if (Mats[i])
		{
			Ant->ArtMIDs.Add(Mats[i]);
		}
	}
	auto Shadow = [](UStaticMeshComponent* C) { C->SetCastShadow(true); return C; };

	Thorax = Shadow(AntRig::AddPiece(Ant, Parent, Art.Thorax, Mats[AntMat::Body], false));
	Thorax->SetRelativeLocation(AntB_Thorax);
	Head = Shadow(AntRig::AddPiece(Ant, Thorax, Art.Head, Mats[AntMat::Body], false));
	Head->SetRelativeLocation(AntB_Head - AntB_Thorax);
	Jaw = AntRig::AddPiece(Ant, Head, Art.Jaw, Mats[AntMat::Body], false);
	Jaw->SetRelativeLocation(AntB_Jaw - AntB_Head);
	Petiole = AntRig::AddPiece(Ant, Thorax, Art.Petiole, Mats[AntMat::Body], false);
	Petiole->SetRelativeLocation(AntB_Petiole - AntB_Thorax);
	Gaster = Shadow(AntRig::AddPiece(Ant, Petiole, Art.Gaster, Mats[AntMat::Body], false));
	Gaster->SetRelativeLocation(AntB_Gaster - AntB_Petiole);

	// The acid gland at the tip of the gaster glows in the weapon's rarity colour (AAntCharacter::OnRep_Weapon).
	const AntArt::FPart& ArtGland = AntArt::Get(TEXT("SM_Ant_AcidGland"));
	if (ArtGland.IsValid())
	{
		Gland = AntArt::AddArt(Ant, Gaster, ArtGland);
		Gland->SetRelativeTransform(AntArt::Fit(ArtGland, FVector(-57.f, 0.f, 6.f) - AntB_Gaster, 18.f, false));
		Ant->GlandArtMID = Gland->CreateDynamicMaterialInstance(0);
		GlandMat = Ant->GlandArtMID;
	}

	const AntArt::FPart& ArtCoxa = AntRig::Art().Coxa;
	const AntArt::FPart& ArtFemur = AntRig::Art().Femur;
	const AntArt::FPart& ArtScape = AntRig::Art().Scape;
	for (int32 i = 0; i < 6; ++i)
	{
		FAntRigLeg& L = Legs[i];
		L.Pair = i / 2;
		L.Side = (i & 1) ? 1 : -1;
		L.Group = (L.Pair + (L.Side > 0 ? 1 : 0)) % 2;
		L.Len = AntRig::PairLen(L.Pair);
		L.Off = L.Group * 0.5f;
		L.Coxa = AntArt::AddArt(Ant, Parent, ArtCoxa);
		L.Femur = AntArt::AddArt(Ant, Parent, ArtFemur);
		if (Mats[AntMat::Coxa]) { L.Coxa->SetMaterial(0, Mats[AntMat::Coxa]); }
		if (Mats[AntMat::Femur]) { L.Femur->SetMaterial(0, Mats[AntMat::Femur]); }
		L.Tibia = AntRig::AddPiece(Ant, Parent, Art.Tibia, Mats[AntMat::Tibia], true);
		L.Tarsus = AntRig::AddPiece(Ant, Parent, Art.Tarsus, Mats[AntMat::Tibia], true);
		Limbs.Append({ L.Coxa, L.Femur, L.Tibia, L.Tarsus });
	}
	for (int32 s = 0; s < 2; ++s)
	{
		FAntRigFeeler& F = Feelers[s];
		F.Scape = AntArt::AddArt(Ant, Parent, ArtScape);
		if (Mats[AntMat::Scape]) { F.Scape->SetMaterial(0, Mats[AntMat::Scape]); }
		F.FlagA = AntRig::AddPiece(Ant, Parent, Art.FlagA, Mats[AntMat::Flag], true);
		F.FlagB = AntRig::AddPiece(Ant, Parent, Art.FlagB, Mats[AntMat::Flag], true);
		F.TapAt = 0.4f + 0.5f * s;
		Limbs.Append({ F.Scape, F.FlagA, F.FlagB });
	}
	LimpLeg = (int32)(Hash01(3) * 5.99f);
	BrokenFeeler = Hash01(5) > 0.5f ? 1 : 0;
	NextIdle = 2.5f + 4.f * Hash01(7);
	Phase = Hash01(11);
}

float UAntBodyComponent::Hash01(int32 Salt) const
{
	const uint32 Id = Ant ? Ant->GetUniqueID() : 0u;
	uint32 X = Id * 2654435761u + (uint32)Salt * 40503u;
	X ^= X >> 15; X *= 2246822519u; X ^= X >> 13;
	return (X & 0xFFFF) / 65535.f;
}

void UAntBodyComponent::SetLegsVisible(bool bVisible)
{
	if (bLegsShown == bVisible)
	{
		return;
	}
	bLegsShown = bVisible;
	for (int32 i = 0; i < 6; ++i)
	{
		const bool bShow = bVisible && !Legs[i].bLost;
		Legs[i].Femur->SetVisibility(bShow);
		Legs[i].Tibia->SetVisibility(bShow);
		Legs[i].Tarsus->SetVisibility(bShow);
		Legs[i].Coxa->SetVisibility(bVisible);
	}
	for (FAntRigFeeler& F : Feelers)
	{
		F.Scape->SetVisibility(bVisible);
		F.FlagA->SetVisibility(bVisible);
		F.FlagB->SetVisibility(bVisible);
	}
}

// ---------------------------------------------------------------------------------------------
// Every frame
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::UpdateLod(bool bNear)
{
	int32 NewLod = 0;
	// (only the player's own ant is always full: a bot is "locally controlled" on the host too, and it must not be)
	if (!(Ant->IsLocallyControlled() && Ant->IsPlayerControlled()))
	{
		float D = 1.e9f;
		if (const APlayerController* LPC = Ant->GetWorld()->GetFirstPlayerController(); LPC && LPC->PlayerCameraManager)
		{
			D = (float)FVector::Dist(LPC->PlayerCameraManager->GetCameraLocation(), Ant->GetActorLocation());
		}
		// (an ant is ~15 units long: at 900 it is a dozen pixels, at 3000 three or four)
		NewLod = !bNear ? 3 : D < 320.f ? 0 : D < 900.f ? 1 : D < 3000.f ? 2 : 3;
		SetLegsVisible(D < 6000.f);
	}
	else
	{
		SetLegsVisible(true);
	}
	if (DevLod >= 0)
	{
		NewLod = DevLod;
	}
	Lod = NewLod;
}

void UAntBodyComponent::Update(float Dt, bool bNear)
{
	if (!Ant || !Thorax || bDevFreeze)
	{
		return;
	}
	UpdateLod(bNear);
	++Frame;
	LodDt += Dt;
	const int32 Every = Lod >= 3 ? 4 : Lod == 2 ? 2 : 1;
	if (Every > 1 && ((Frame + (int32)(Ant->GetUniqueID() & 3)) % Every) != 0)
	{
		return;
	}
	Dt = FMath::Clamp(LodDt, 1.e-4f, 0.1f);
	LodDt = 0.f;
	Time += Dt;

	// ---- what the ant is doing
	VS = Ant->VisScale();
	P = Ant->GetActorLocation();
	N = Ant->VisNormal;
	H = Ant->VisHeading;
	R = FVector::CrossProduct(N, H).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	bHeld = Ant->IsAttached();
	bCrawl = Ant->IsCrawling() && !bHeld;
	bAir = !bCrawl && !bHeld;
	V = bHeld ? FVector::ZeroVector : Ant->GetVelocity();
	{
		// Pushing against something it cannot climb the capsule goes nowhere although the movement still "has" its speed:
		// the legs must not run on the spot. What counts is how far the body really went (on a moving thing: its own speed).
		const FVector Went = (P - PPrev) / Dt;
		PPrev = P;
		Moved += (Went.GetClampedToMaxSize(3000.f) - Moved) * (1.f - FMath::Exp(-22.f * Dt));
		const UAntMovementComponent* Move = Ant->GetAntMovement();
		if (bCrawl && !(Move && Move->GetRiddenComponent()))
		{
			const float Real = (float)FVector::VectorPlaneProject(Moved, N).Size() * 1.08f;
			const FVector Flat = FVector::VectorPlaneProject(V, N);
			if (Flat.SizeSquared() > FMath::Square(Real))
			{
				V = Flat.GetSafeNormal() * Real;
			}
		}
	}
	const FVector RawAcc = (V - VPrev) / Dt;
	Acc += (RawAcc.GetClampedToMaxSize(9000.f) - Acc) * (1.f - FMath::Exp(-14.f * Dt));
	VPrev = V;
	{
		const float Turn = FMath::Atan2((float)FVector::DotProduct(FVector::CrossProduct(HPrev, H), N), (float)FVector::DotProduct(HPrev, H));
		YawRate += (FMath::Clamp(Turn / Dt, -25.f, 25.f) - YawRate) * (1.f - FMath::Exp(-12.f * Dt));
		HPrev = H;
	}
	Ground = AAntCharacter::BodyRadius / VS + 0.5f;
	SpeedAL = (float)FVector::VectorPlaneProject(V, N).Size() / VS;
	StillTime = (SpeedAL < 60.f && bCrawl) ? StillTime + Dt : 0.f;

	const float Now = NpcRig::ServerNow(Ant);
	ActAge = Act.Kind != AntAct::None ? Now - Act.Start : 100.f;

	// ---- which body it is right now: on its feet, in the air, knocked over, held
	const uint8 OldMode = Mode;
	if (bHeld)
	{
		Mode = Ant->HeldBy ? M_Carried : Ant->NpcHold ? M_Held : M_Chain;
	}
	else if (bAir)
	{
		if (Mode != M_Tumble)
		{
			const bool bKnocked = Act.Kind == AntAct::Knock && ActAge < 0.6f && Act.Power > 60;
			if (bKnocked && Mode != M_Back && Mode != M_Flip)
			{
				Mode = M_Tumble;
				// over and over along the knock, with a twist that is the same on every machine
				const FVector Dir = FVector(Act.Dir).GetSafeNormal(UE_SMALL_NUMBER, H);
				const float Twist = Frac(Act.Start * 7.31f) - 0.5f;
				SpinAxis = (FVector::CrossProduct(FVector::UpVector, Dir) + Dir * Twist * 0.8f).GetSafeNormal(UE_SMALL_NUMBER, R);
				SpinRate = 9.f + 13.f * (Act.Power / 255.f);
				Spin = 0.f;
			}
			else if (Mode != M_Back && Mode != M_Flip)
			{
				Mode = M_Air;
			}
		}
		AirVel = V;
	}
	else
	{
		if (Mode == M_Tumble)
		{
			Mode = M_Back;
			Flip = 1.f;
		}
		else if (Mode == M_Back)
		{
			// lies on its back kicking: longer after a hard knock, shorter when the player is already running off
			const float Want = SpeedAL > 500.f ? 0.14f : 0.26f + 0.3f * (Act.Power / 255.f);
			if (ModeTime > Want) { Mode = M_Flip; }
		}
		else if (Mode == M_Flip)
		{
			if (ModeTime > 0.24f) { Mode = M_Ground; }
		}
		else
		{
			if (Mode == M_Air)
			{
				// the legs take the landing: the body dips, a hard one staggers it
				const float Impact = FMath::Max(0.f, -(float)FVector::DotProduct(AirVel, N));
				if (AirTime > 0.12f)
				{
					DipV -= FMath::Min(Impact, 900.f) * 0.4f;
					if (Impact > 520.f)
					{
						Stun = FMath::Max(Stun, FMath::Min(1.f, (Impact - 520.f) / 500.f + 0.35f));
						ShoveV += FVector::VectorPlaneProject(AirVel, N).GetSafeNormal() * 14.f + R * (Hash01(Frame) - 0.5f) * 18.f;
					}
				}
			}
			Mode = M_Ground;
		}
	}
	if (Mode != OldMode)
	{
		ModeTime = 0.f;
		if (Mode == M_Ground || OldMode == M_Ground)
		{
			for (FAntRigLeg& L : Legs) { L.bInit = false; }
		}
	}
	ModeTime += Dt;
	AirTime = bAir ? AirTime + Dt : 0.f;
	bWasAir = bAir;

	UpdateInjuries(Dt);
	UpdateGait(Dt);
	UpdateBodyFrame(Dt);
	UpdateSpine(Dt);
	UpdateLegs(Dt);
	UpdateFeelers(Dt);
	PlacePieces();
}

void UAntBodyComponent::OnAct(const FAntBodyAct& InAct)
{
	if (!Ant || InAct.Kind == AntAct::None)
	{
		return;
	}
	const float Now = NpcRig::ServerNow(Ant);
	// the owner predicted it already / an old record a late joiner received
	if ((InAct.Kind == Act.Kind && FMath::Abs(InAct.Start - LastActStart) < 0.3f) || Now - InAct.Start > 1.5f)
	{
		return;
	}
	Act = InAct;
	LastActStart = InAct.Start;
	ActAge = FMath::Max(0.f, Now - InAct.Start);
	const float K = InAct.Power / 255.f;
	const FVector Dir = FVector(InAct.Dir).GetSafeNormal();
	if (InAct.Kind == AntAct::Flinch || InAct.Kind == AntAct::Knock)
	{
		// the body is thrown off its feet along the blow and its top goes further than its feet
		ShoveV += Dir * (24.f + 62.f * K) * (VS / 0.158f);
		LeanV += Dir * (3.f + 8.f * K);
		DipV -= 20.f + 50.f * K;
		Hurt = FMath::Min(1.f, Hurt + 0.3f + 0.5f * K);
		JawV += 400.f;
		for (FAntRigFeeler& F : Feelers)
		{
			F.PitchV += 500.f + 700.f * K;
			F.YawV += 300.f + 500.f * K;
		}
	}
	else if (InAct.Kind == AntAct::Dash)
	{
		LeanV += Dir * 3.f;
	}
	else if (InAct.Kind == AntAct::Bite)
	{
		ShoveV += H * 26.f * (VS / 0.158f);
	}
}

// ---------------------------------------------------------------------------------------------
// Injuries: lost legs, wounds, hemolymph
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::Retime()
{
	NumAlive = 0;
	for (const FAntRigLeg& L : Legs) { NumAlive += L.bLost ? 0 : 1; }
	if (NumAlive == 6)
	{
		for (FAntRigLeg& L : Legs)
		{
			L.Group = (L.Pair + (L.Side > 0 ? 1 : 0)) % 2;
			L.Off = L.Group * 0.5f;
		}
		return;
	}
	// Legs missing: what is left takes turns, one after another, never two neighbours at once.
	static const int32 Order[6] = { 0, 3, 4, 1, 2, 5 };
	int32 k = 0;
	for (int32 o = 0; o < 6; ++o)
	{
		FAntRigLeg& L = Legs[Order[o]];
		if (!L.bLost)
		{
			L.Off = (float)k / FMath::Max(1, NumAlive);
			L.Group = k % 2;
			++k;
		}
	}
}

void UAntBodyComponent::ApplyLostLegs()
{
	const uint8 Bits = Ant->LostLegs;
	FVector Tilt = FVector::ZeroVector;
	for (int32 i = 0; i < 6; ++i)
	{
		FAntRigLeg& L = Legs[i];
		const bool bLost = (Bits & (1 << i)) != 0;
		if (bLost && !L.bLost && Lod <= 2 && Time > 0.5f)
		{
			// the leg comes off: a small thing that lies where it fell, and a spurt of hemolymph
			FAntBodySnapshot S;
			if (Snapshot(S))
			{
				const FVector Out = (L.Knee - P).GetSafeNormal(UE_SMALL_NUMBER, R * L.Side);
				AAntCorpse::SpawnLeg(Ant->GetWorld(), S, i, V * 0.4f + (FVector::VectorPlaneProject(Out, N).GetSafeNormal() * 170.f + N * 210.f) * AntKit::SpeedK * (0.8f + 0.4f * Hash01(i + Frame)));
			}
			if (AAntGibs* G = Ant->GetWorld()->SpawnActor<AAntGibs>(AAntGibs::StaticClass(), FTransform::Identity))
			{
				for (int32 g = 0; g < 4; ++g)
				{
					G->AddGoo(L.HipW, (FMath::VRand() * 0.7f + N + R * L.Side).GetSafeNormal() * FMath::FRandRange(60.f, 170.f), FMath::FRandRange(0.5f, 1.3f) * VS / 0.158f, Hemolymph);
				}
			}
			Hurt = 1.f;
			ShoveV -= R * L.Side * 20.f * (VS / 0.158f);
		}
		if (L.bLost && !bLost)
		{
			L.bInit = false;   // grown back in the nest
		}
		L.bLost = bLost;
		const bool bShow = bLegsShown && !bLost;
		L.Femur->SetVisibility(bShow);
		L.Tibia->SetVisibility(bShow);
		L.Tarsus->SetVisibility(bShow);
		if (bLost)
		{
			const FVector Home = AntRig::HomeLocal(i);
			Tilt += FVector(Home.X, Home.Y, 0.f).GetSafeNormal();
		}
	}
	ShownLost = Bits;
	Ant->ShownLostLegs = Bits;
	Retime();
	// the body leans to where it has nothing to stand on
	SagDir = Tilt;
}

void UAntBodyComponent::UpdateInjuries(float Dt)
{
	if (ShownLost != Ant->LostLegs)
	{
		ApplyLostLegs();
	}
	const float Frac01 = Ant->MaxHealth > 0.f ? FMath::Clamp(Ant->Health / Ant->MaxHealth, 0.f, 1.f) : 1.f;
	// wounded below 45 % of its health, as bad as it gets at 10 %
	const float WantWound = FMath::Clamp((0.45f - Frac01) / 0.35f, 0.f, 1.f);
	Wound += (WantWound - Wound) * (1.f - FMath::Exp(-3.f * Dt));
	if (LastHealth >= 0.f && Ant->Health < LastHealth - 0.2f)
	{
		Hurt = FMath::Min(1.f, Hurt + (LastHealth - Ant->Health) / 22.f);
	}
	LastHealth = Ant->Health;
	Hurt = FMath::Max(0.f, Hurt - Dt * 2.6f);
	const bool bStag = Ant->HasStatus(AntStatus::Staggered) || Ant->HasStatus(AntStatus::Shocked);
	Stun += ((bStag ? 1.f : 0.f) - Stun) * (1.f - FMath::Exp(-(bStag ? 14.f : 3.5f) * Dt));

	// How hard it works: a load it can barely move, a door it pulls at.
	float WantEffort = 0.f;
	if (const AKitchenLoot* L = Ant->Carrying)
	{
		const float Ratio = L->CarryRatio();
		WantEffort = L->CarryTeam() != Ant->Team ? 1.f : FMath::Clamp((1.9f - Ratio) / 1.2f, 0.12f, 1.f);
	}
	{
		// carrying together (AntCarry): what its own grip asks of it; dragged off its feet it strains all out
		FAntCarryFeel CF;
		if (AntCarry::Feel(Ant, CF))
		{
			WantEffort = FMath::Max(WantEffort, CF.bDragged ? 1.f : FMath::Clamp(0.15f + 0.8f * CF.Strain, 0.f, 1.f));
		}
	}
	if (Ant->HoldingAnt) { WantEffort = FMath::Max(WantEffort, 0.45f); }
	if (Ant->Pulling) { WantEffort = 1.f; }
	Effort += (WantEffort - Effort) * (1.f - FMath::Exp(-5.f * Dt));
	Strain = Effort * (0.6f + 0.4f * FMath::Sin(Time * 2.3f));

	if (Lod > 1)
	{
		return;
	}
	// Drops of hemolymph (pale yellow-green, insects do not bleed red) on the wound...
	const int32 WantBlobs = Frac01 < 0.2f ? 4 : Frac01 < 0.35f ? 3 : Frac01 < 0.5f ? 2 : Frac01 < 0.62f ? 1 : 0;
	if (WantBlobs > 0 && Wounds.Num() == 0)
	{
		// where it was hit last, as far as that is known: on the side of the blow
		const FVector From = Act.Kind == AntAct::Flinch || Act.Kind == AntAct::Knock ? -FVector(Act.Dir) : FVector(R * (Hash01(21) > 0.5f ? 1.f : -1.f));
		const float Side = FVector::DotProduct(From, R) >= 0.f ? 1.f : -1.f;
		const FVector At[4] = { FVector(-14.f, 9.f * Side, 8.f), FVector(2.f, 6.5f * Side, 7.f), FVector(-24.f, 3.f * Side, 13.f), FVector(8.f, -4.f * Side, 10.f) };
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Par = (i == 0 || i == 2) ? Gaster : Thorax;
			UStaticMeshComponent* B = AntKit::AddPart(Ant, Par, EAntShape::Sphere, At[i], FVector(0.05f, 0.045f, 0.022f) * (0.8f + 0.5f * Hash01(30 + i)), Hemolymph * 1.15f,
				FRotator(-20.f * (i == 2 ? 0.f : 1.f), 0.f, 35.f * Side));
			B->SetCastShadow(false);
			B->SetVisibility(false);
			Wounds.Add(B);
		}
	}
	for (int32 i = 0; i < Wounds.Num(); ++i)
	{
		if (Wounds[i]) { Wounds[i]->SetVisibility(i < WantBlobs && Thorax->IsVisible()); }
	}
	// ... and left behind on the floor while it walks.
	if (Wound > 0.15f && bCrawl && SpeedAL > 150.f && Time > NextDrip)
	{
		NextDrip = Time + FMath::Lerp(1.5f, 0.45f, Wound) * (0.7f + 0.6f * FMath::FRand());
		if (!Drips.IsValid() || Time - DripsBorn > 9.f)
		{
			Drips = Ant->GetWorld()->SpawnActor<AAntGibs>(AAntGibs::StaticClass(), FTransform::Identity);
			DripsBorn = Time;
		}
		if (AAntGibs* G = Drips.Get())
		{
			G->AddGoo(P - H * 22.f * VS - N * (Ground - 9.f) * VS, V * 0.2f - N * 40.f, FMath::FRandRange(0.35f, 0.8f) * VS / 0.158f, Hemolymph);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// The gait: feet stand where they were put, legs step in two tripods
// ---------------------------------------------------------------------------------------------

FVector UAntBodyComponent::HomeWorld(const FAntRigLeg& L) const
{
	const int32 Index = L.Pair * 2 + (L.Side > 0 ? 1 : 0);
	const FVector Home = AntRig::HomeLocal(Index);
	// wider on a wall, under a load, when dizzy; pulled in under a web
	float Spread = 1.f + 0.12f * Wall + 0.10f * Effort + 0.16f * Stun;
	if (Ant->HasStatus(AntStatus::Webbed)) { Spread *= 0.62f; }
	const float Reach = 0.9f + 0.1f * L.Len;
	return P + (H * Home.X * Reach + R * Home.Y * Spread) * VS - N * Ground * VS;
}

FVector UAntBodyComponent::LegVelocity(const FAntRigLeg& L) const
{
	// the body's velocity where this leg stands: turning on the spot moves the feet round too
	const FVector Arm = HomeWorld(L) - P;
	return FVector::VectorPlaneProject(V, N) + FVector::CrossProduct(N * YawRate, Arm);
}

FVector UAntBodyComponent::FootWorld(const FAntRigLeg& L) const
{
	if (const UPrimitiveComponent* B = L.Base.Get())
	{
		return B->GetComponentTransform().TransformPosition(L.Foot);
	}
	return L.Foot;
}

void UAntBodyComponent::SetFoot(FAntRigLeg& L, const FVector& World, UPrimitiveComponent* Comp)
{
	// On something that moves (the Human's shin, a carried cake) the foot is kept in that thing's space: it rides along.
	if (Comp && Comp->Mobility == EComponentMobility::Movable && Comp->GetOwner() != Ant)
	{
		L.Base = Comp;
		L.Foot = Comp->GetComponentTransform().InverseTransformPosition(World);
	}
	else
	{
		L.Base = nullptr;
		L.Foot = World;
	}
	L.FootW = World;
}

bool UAntBodyComponent::FindFoothold(const FAntRigLeg& L, const FVector& Want, FVector& OutPoint, UPrimitiveComponent*& OutComp) const
{
	UWorld* W = Ant->GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntFoothold), false, Ant);
	if (Ant->Carrying) { Params.AddIgnoredActor(Ant->Carrying); }
	const FCollisionObjectQueryParams Obj = UAntMovementComponent::SurfaceObjects();
	const float G = Ground * VS;
	const int32 Index = L.Pair * 2 + (L.Side > 0 ? 1 : 0);
	const FVector Hip = AntRig::HipLocal(Index);
	const FVector Shoulder = P + (H * Hip.X + R * Hip.Y * 1.6f) * VS;
	const FVector Above = Want + N * G * 0.55f;
	FHitResult Hit;
	// 1. something stands between the body and where the foot wants to go (a wall ahead, a crumb): the foot goes onto it
	if (W->LineTraceSingleByObjectType(Hit, Shoulder, Above, Obj, Params) && FVector::DotProduct(Hit.ImpactNormal, Above - Shoulder) < 0.f)
	{
		OutPoint = Hit.ImpactPoint;
		OutComp = Hit.GetComponent();
		return true;
	}
	// 2. the surface under it
	if (W->LineTraceSingleByObjectType(Hit, Above, Want - N * G * 0.9f, Obj, Params))
	{
		OutPoint = Hit.ImpactPoint;
		OutComp = Hit.GetComponent();
		return true;
	}
	// 3. past an edge: the side of whatever the ant stands on (legs hold on round a table's edge)
	const FVector Below = Want - N * G * 0.55f;
	if (W->LineTraceSingleByObjectType(Hit, Below, P - N * G * 1.55f, Obj, Params))
	{
		OutPoint = Hit.ImpactPoint;
		OutComp = Hit.GetComponent();
		return true;
	}
	return false;
}

void UAntBodyComponent::StartStep(FAntRigLeg& L, float Dur, bool bFromPhase)
{
	L.From = L.FootW;
	L.Dur = FMath::Max(0.035f, Dur);
	L.T = 0.f;
	L.bRefined = false;
	// where the body will be when the foot comes down, plus half of what it will travel while the foot stands
	const float Cadence = FMath::Max(0.5f, SpeedAL / FMath::Max(20.f, Stride));
	const float StanceFor = bFromPhase ? Duty / Cadence : 0.f;
	const FVector Want = HomeWorld(L) + LegVelocity(L) * (L.Dur + StanceFor * 0.5f);
	FVector Pt;
	UPrimitiveComponent* Comp = nullptr;
	if (Lod <= 1 && FindFoothold(L, Want, Pt, Comp))
	{
		L.To = Pt;
		L.ToBase = Comp;
		L.bGround = true;
	}
	else
	{
		L.To = Want;
		L.ToBase = nullptr;
		L.bGround = Lod > 1;   // far away nobody checks: the feet walk on the plane the capsule is glued to
	}
}

void UAntBodyComponent::UpdateGait(float Dt)
{
	if (Mode != M_Ground)
	{
		return;
	}
	Wall += ((N.Z < 0.6f ? 1.f : 0.f) - Wall) * (1.f - FMath::Exp(-6.f * Dt));
	const bool bSprint = Ant->bSprinting && !Ant->Carrying;
	const bool bSticky = Ant->HasStatus(AntStatus::Sticky);
	const float Drag = NumAlive <= 3 ? 1.f : 0.f;

	// Stride and cadence follow the speed: short quick steps when slow, long ones at a run.
	// (an ant covers 18 body lengths a second: at the top of a run both tripods are in the air for a moment, like a real fast ant)
	float WantStride = FMath::Clamp(46.f + 0.0625f * SpeedAL, 46.f, bSprint ? 195.f : 160.f);
	WantStride *= 1.f - 0.38f * Effort - 0.18f * Wound - 0.2f * Wall;
	if (NumAlive < 6) { WantStride *= 0.5f + 0.08f * NumAlive; }
	float WantDuty = FMath::Lerp(0.70f, 0.45f, FMath::Clamp((SpeedAL - 200.f) / 1600.f, 0.f, 1.f));
	if (bSprint) { WantDuty -= 0.06f; }
	WantDuty += 0.14f * Wall + 0.14f * Effort + 0.06f * Wound;
	if (NumAlive < 6)
	{
		// fewer legs: more of them on the ground at any moment
		static const float MinDuty[6] = { 0.85f, 0.85f, 0.8f, 0.72f, 0.66f, 0.62f };
		WantDuty = FMath::Max(WantDuty, MinDuty[FMath::Clamp(NumAlive, 0, 5)]);
	}
	Stride += (WantStride - Stride) * (1.f - FMath::Exp(-10.f * Dt));
	Duty += (FMath::Clamp(WantDuty, 0.36f, 0.88f) - Duty) * (1.f - FMath::Exp(-10.f * Dt));

	// Ants hauling one load step together: everyone takes the cadence of the first carrier.
	const AAntCharacter* Leader = nullptr;
	if (const AKitchenLoot* Load = Ant->Carrying)
	{
		// (only while they move as one: out of step each walks its own beat - AntCarry's rhythm, visible)
		if (Load->Carriers.Num() > 1 && Load->Carriers[0] && Load->Carriers[0] != Ant && Load->Carriers[0]->Rig && Load->CarryNet.Sync > 150)
		{
			Leader = Load->Carriers[0];
		}
	}
	bPhaseGait = SpeedAL > 150.f;
	float Cadence = SpeedAL / FMath::Max(20.f, Stride);
	// wounded: an uneven rhythm, a hitch in every cycle
	Cadence *= 1.f + 0.4f * Wound * FMath::Sin(2.f * PI * Phase);
	const float OldPhase = Phase;
	Phase = Leader ? Leader->Rig->GetPhase() : Frac(Phase + Cadence * Dt);

	const float Swing = 1.f - Duty;
	const float StepH = (7.f + 0.0035f * SpeedAL + (bSticky ? 9.f : 0.f) + 3.f * Wall) * VS;
	bool Stepping[2] = { false, false };
	for (const FAntRigLeg& L : Legs)
	{
		if (!L.bLost && L.T < 1.f) { Stepping[L.Group] = true; }
	}
	const float Reach = (AntRig::FemurLen + AntRig::TibiaLen) * VS;
	StepClock += Dt;

	for (int32 i = 0; i < 6; ++i)
	{
		FAntRigLeg& L = Legs[i];
		if (L.bLost)
		{
			continue;
		}
		if (!L.bInit)
		{
			// just put down (spawn, a landing, getting up): reach for the ground from wherever the foot is
			L.bInit = true;
			if (L.FootW.IsZero() || FVector::DistSquared(L.FootW, P) > FMath::Square(160.f * VS))
			{
				FVector Pt;
				UPrimitiveComponent* Comp = nullptr;
				const FVector Home = HomeWorld(L);
				const bool bHit = Lod <= 1 && FindFoothold(L, Home, Pt, Comp);
				SetFoot(L, bHit ? Pt : Home, bHit ? Comp : nullptr);
				L.bGround = bHit || Lod > 1;
				L.T = 1.f;
			}
			else
			{
				L.Base = nullptr;
				L.Foot = L.FootW;
				StartStep(L, 0.07f + 0.03f * Hash01(i), false);
			}
			L.Phase = Frac(Phase + L.Off);
			continue;
		}
		const float Ph = Frac(Phase + L.Off);
		const float PrevPh = L.Phase;
		L.Phase = Ph;
		if (L.T < 1.f)
		{
			// ---- swinging
			float Dur = L.Dur;
			if (bSticky) { Dur *= 1.7f; }
			if (i == LimpLeg && Wound > 0.2f) { Dur *= 1.f - 0.35f * Wound; }
			L.T = FMath::Min(1.f, L.T + Dt / Dur);
			if (Lod == 0 && !L.bRefined && L.T >= 0.5f && L.T < 0.9f)
			{
				// half-way: look again where it will really come down (the body has turned, a ledge came up)
				L.bRefined = true;
				const float Cad = FMath::Max(0.5f, SpeedAL / FMath::Max(20.f, Stride));
				const FVector Want = HomeWorld(L) + LegVelocity(L) * (Dur * (1.f - L.T) + (bPhaseGait ? Duty / Cad * 0.5f : 0.f));
				FVector Pt;
				UPrimitiveComponent* Comp = nullptr;
				if (FindFoothold(L, Want, Pt, Comp))
				{
					L.To = Pt;
					L.ToBase = Comp;
					L.bGround = true;
				}
			}
			const float E = Smooth(L.T);
			L.Lift = FMath::Sin(L.T * PI);
			float Lift = StepH * (i == LimpLeg ? 1.f + 0.8f * Wound : 1.f);
			L.FootW = FMath::Lerp(L.From, L.To, E) + N * L.Lift * Lift;
			if (L.T >= 1.f)
			{
				// ---- touchdown
				SetFoot(L, L.To, L.ToBase.Get());
				L.Lift = 0.f;
				if (i == LimpLeg && Wound > 0.2f)
				{
					DipV -= 30.f * Wound;   // the bad leg gives under the weight
				}
				if (Effort > 0.3f || Drag > 0.f)
				{
					DipV -= 14.f * FMath::Max(Effort, Drag);
				}
				if (Ant->FootstepVolume > 0.f && Lod == 0 && StepClock > 0.045f)
				{
					// one tiny chitin tick per tripod (louder for your own ant, pitch by size, the sound of the surface)
					StepClock = 0.f;
					Ant->FootSurface = L.ToBase.Get();
					AntSfx::Play(Ant, AntSfx::StepFor(L.ToBase.Get()), L.To, Ant->FootstepVolume, FMath::Clamp(1.1f - VS * 2.f, 0.8f, 1.3f));
				}
			}
			continue;
		}
		// ---- standing
		L.Lift = 0.f;
		L.FootW = FootWorld(L);
		const FVector Home = HomeWorld(L);
		const float Err = (float)FVector::Dist(L.FootW, Home);
		if (Err > 150.f * VS)
		{
			SetFoot(L, Home, nullptr);   // teleported
			continue;
		}
		const bool bTooFar = Err > Reach * 0.82f;
		if (bPhaseGait)
		{
			// its turn in the cycle has come (the phase wrapped into the swing part)
			const bool bTurn = Ph < Swing && (PrevPh >= Swing || PrevPh > Ph);
			if (bTurn && Err > 5.f * VS)
			{
				StartStep(L, (Swing - Ph) / FMath::Max(0.5f, Cadence), true);
				Stepping[L.Group] = true;
			}
			else if (bTooFar && !Stepping[1 - L.Group])
			{
				StartStep(L, 0.06f, false);
				Stepping[L.Group] = true;
			}
		}
		else
		{
			// slow or turning on the spot: a foot steps when it is left behind, one tripod at a time
			const float Thr = (15.f + 6.f * Hash01(i * 3 + 1) + (Idle ? 6.f : 0.f)) * VS * (NumAlive < 6 ? 1.2f : 1.f);
			if ((Err > Thr || (Stepping[L.Group] && Err > Thr * 0.5f)) && !Stepping[1 - L.Group])
			{
				StartStep(L, bSticky ? 0.2f : 0.095f + 0.03f * Effort + 0.05f * Drag, false);
				Stepping[L.Group] = true;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// The body on its legs: height and tilt from where the feet stand, inertia, knocks
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::UpdateBodyFrame(float Dt)
{
	const float G = Ground * VS;
	const float K = VS / 0.158f;
	const bool bSprint = Ant->bSprinting && !Ant->Carrying && SpeedAL > 1500.f;
	const float Sp = FMath::Clamp(SpeedAL / 1800.f, 0.f, 1.3f);

	Spring(Shove, ShoveV, FVector::ZeroVector, 5.f, 0.45f, Dt);
	Shove = Shove.GetClampedToMaxSize(9.f * VS);
	Spring(Dip, DipV, 0.f, 5.5f, 0.42f, Dt);
	Dip = FMath::Clamp(Dip, -13.f, 7.f);

	// ---- the plane through the feet (in the frame of the surface the capsule is glued to)
	float A = 0.f, B = 0.f, C = -G;
	if (Mode == M_Ground)
	{
		double Sx = 0, Sy = 0, Sz = 0, Sxx = 0, Sxy = 0, Syy = 0, Sxz = 0, Syz = 0;
		int32 Num = 0;
		for (const FAntRigLeg& L : Legs)
		{
			if (L.bLost || !L.bInit || !L.bGround)
			{
				continue;
			}
			const FVector S = L.T < 1.f ? FMath::Lerp(L.From, L.To, Smooth(L.T)) : L.FootW;
			const FVector D = S - P;
			const double X = FVector::DotProduct(D, H), Y = FVector::DotProduct(D, R), Z = FVector::DotProduct(D, N);
			Sx += X; Sy += Y; Sz += Z; Sxx += X * X; Sxy += X * Y; Syy += Y * Y; Sxz += X * Z; Syz += Y * Z;
			++Num;
		}
		if (Num >= 3)
		{
			const double Nn = Num;
			const double Det = Sxx * (Syy * Nn - Sy * Sy) - Sxy * (Sxy * Nn - Sy * Sx) + Sx * (Sxy * Sy - Syy * Sx);
			if (FMath::Abs(Det) > 1.e-3)
			{
				A = (float)((Sxz * (Syy * Nn - Sy * Sy) - Sxy * (Syz * Nn - Sy * Sz) + Sx * (Syz * Sy - Syy * Sz)) / Det);
				B = (float)((Sxx * (Syz * Nn - Sz * Sy) - Sxz * (Sxy * Nn - Sy * Sx) + Sx * (Sxy * Sz - Syz * Sx)) / Det);
				C = (float)((Sxx * (Syy * Sz - Sy * Syz) - Sxy * (Sxy * Sz - Syz * Sx) + Sxz * (Sxy * Sy - Syy * Sx)) / Det);
			}
			else
			{
				C = (float)(Sz / Nn);
			}
		}
		else if (Num > 0)
		{
			C = (float)(Sz / Num);
		}
		A = FMath::Clamp(A, -0.8f, 0.8f);
		B = FMath::Clamp(B, -0.6f, 0.6f);
		C = FMath::Clamp(C, -1.7f * G, -0.35f * G);
	}
	Spring(FitA, FitAV, A, 6.5f, 0.85f, Dt);
	Spring(FitB, FitBV, B, 6.5f, 0.85f, Dt);
	Spring(FitC, FitCV, C, 8.f, 0.85f, Dt);
	FitA = FMath::Clamp(FitA, -1.f, 1.f);
	FitB = FMath::Clamp(FitB, -0.8f, 0.8f);
	FitC = FMath::Clamp(FitC, -2.2f * G, 0.3f * G);

	// ---- where the top of the body is pushed: into the acceleration, into the turn, towards a missing leg
	FVector WantLean = FVector::ZeroVector;
	FVector CarryOff = FVector::ZeroVector;
	if (Mode == M_Ground)
	{
		WantLean = (FVector::VectorPlaneProject(Acc, N) * 8.5e-5f).GetClampedToMaxSize(0.3f);
		if (bSprint) { WantLean += H * 0.10f; }
		WantLean += (H * SagDir.X + R * SagDir.Y) * 0.085f;
		if (Ant->Pulling) { WantLean -= H * 0.2f; }
		else if (Effort > 0.f) { WantLean -= H * 0.09f * Effort; }
		FAntCarryFeel CF;
		if (AntCarry::Feel(Ant, CF))
		{
			// the load pulls at the jaws: the body digs its heels in against it (leans away), and when it is dragged off its
			// feet it is thrown towards it; the body itself is drawn a little towards the grip (CarryOff below)
			const FVector PullN = FVector::VectorPlaneProject(CF.Pull, N);
			const float Pk = FMath::Clamp((float)PullN.Size() / (14.f * VS / 0.158f), 0.f, 1.f);
			WantLean += PullN.GetSafeNormal() * Pk * (CF.bDragged ? 0.3f : -0.18f);
			CarryOff = PullN.GetClampedToMaxSize(3.5f * VS / 0.158f) * 0.45f;
		}
		WantLean += (R * FMath::Sin(Time * 5.3f) * 0.13f + H * FMath::Sin(Time * 3.9f) * 0.07f) * Stun;
	}
	else if (Mode == M_Air)
	{
		const bool bDash = Act.Kind == AntAct::Dash && ActAge < 0.45f;
		WantLean = bDash ? H * 0.16f : -H * FMath::Clamp((float)FVector::DotProduct(V, N) / 750.f, -0.45f, 0.45f);
	}
	Spring(Lean, LeanV, WantLean, 4.5f, 0.4f, Dt);
	Lean = Lean.GetClampedToMaxSize(0.6f);

	FVector Up = (N - H * FitA - R * FitB + Lean).GetSafeNormal(UE_SMALL_NUMBER, N);
	if (FVector::DotProduct(Up, N) < 0.72f)
	{
		Up = (Up + N * 0.9f).GetSafeNormal(UE_SMALL_NUMBER, N);
	}
	const FQuat Base = FRotationMatrix::MakeFromZX(Up, H).ToQuat();

	// ---- standing height (ant-local, above the plane of the feet)
	Sag = 2.2f * (6 - NumAlive) + (NumAlive <= 3 ? 8.f : 0.f);
	// (20.11 merge: it stood about 7 units too low, the gaster near the floor; -AntStandLift= tunes it)
	static const float StandLift = [] { float V = 5.f; FParse::Value(FCommandLine::Get(), TEXT("AntStandLift="), V); return V; }();
	float Stand = Ground + StandLift - (bSprint ? 3.f : 0.f) - 4.5f * Wall - 6.5f * Effort - 3.f * Wound - Sag + Dip;
	Stand += 0.7f * Sp * FMath::Sin(4.f * PI * Phase + 0.6f) * (1.f + 1.5f * Effort);
	Stand += 0.35f * FMath::Sin(Time * 2.1f + Hash01(1) * 6.f);   // breathing
	if (Ant->HasStatus(AntStatus::Webbed)) { Stand -= 5.f; }
	Stand = FMath::Max(Stand, 11.5f);

	FQuat WantQ = Base;
	FVector WantO = P;
	float Blend = 45.f;
	switch (Mode)
	{
	case M_Ground:
	{
		float Yaw = 2.6f * Sp * FMath::Sin(2.f * PI * Phase);
		float Roll = 1.8f * Sp * FMath::Sin(2.f * PI * Phase + 1.2f);
		float Pitch = 1.f * Sp * FMath::Sin(4.f * PI * Phase);
		// in pain: a jerk; straining: a tremble
		Yaw += 5.f * Hurt * FMath::PerlinNoise1D(Time * 27.f + 3.f) + 0.6f * Effort * FMath::Sin(Time * 53.f);
		Roll += 6.f * Hurt * FMath::PerlinNoise1D(Time * 31.f + 11.f) + 0.8f * Effort * FMath::Sin(Time * 47.f);
		Pitch += 4.f * Hurt * FMath::PerlinNoise1D(Time * 23.f + 19.f);
		FVector Lurch = FVector::ZeroVector;
		if (NumAlive <= 3 && SpeedAL > 40.f)
		{
			// dragging itself: the body is hauled forward stroke by stroke
			Lurch = H * 5.f * VS * FMath::Sin(2.f * PI * Phase * FMath::Max(1, NumAlive));
			Pitch -= 5.f;
		}
		if (Idle == 2)
		{
			// shifting its weight from side to side
			const float T = IdleT / FMath::Max(0.1f, IdleDur);
			Lurch += R * 4.5f * VS * FMath::Sin(T * 2.f * PI) * Bump(T, 0.f, 0.2f, 0.8f, 1.f);
			Roll += 5.f * FMath::Sin(T * 2.f * PI) * Bump(T, 0.f, 0.2f, 0.8f, 1.f);
		}
		WantQ = Base * FRotator(Pitch, Yaw, Roll).Quaternion();
		WantO = P + N * FitC + Up * Stand * VS + Shove + Lurch + CarryOff;
		break;
	}
	case M_Air:
		WantO = P - N * 2.f * VS + Shove;
		Blend = 22.f;
		break;
	case M_Tumble:
		Spin += SpinRate * Dt;
		SpinRate *= FMath::Exp(-0.35f * Dt);
		WantQ = FQuat(SpinAxis, Spin) * FRotationMatrix::MakeFromZX(FVector::UpVector, H).ToQuat();
		WantO = P;
		Blend = 60.f;
		break;
	case M_Back:
	{
		// on its back, rocking, legs in the air
		const float Rock = 0.22f * FMath::Sin(Time * 15.f) + 0.1f * FMath::Sin(Time * 23.f);
		WantQ = FQuat(H, PI + Rock) * FRotationMatrix::MakeFromZX(N, H).ToQuat();
		WantO = P - N * G + N * 23.f * VS + Shove;
		Blend = 20.f;
		break;
	}
	case M_Flip:
	{
		// throws itself over: half a turn about its own length, with a hop
		const float T = FMath::Clamp(ModeTime / 0.24f, 0.f, 1.f);
		const float Side = Hash01(17) > 0.5f ? 1.f : -1.f;
		WantQ = FQuat(H, PI * (1.f - Smooth(T)) * Side) * FRotationMatrix::MakeFromZX(N, H).ToQuat();
		WantO = P - N * G + N * (FMath::Lerp(23.f, Stand, Smooth(T)) + 10.f * FMath::Sin(T * PI)) * VS + Shove;
		Blend = 60.f;
		break;
	}
	case M_Held:
	{
		// pinned, pinched, licked: it squirms
		const FRotator Squirm(9.f * FMath::Sin(Time * 7.3f), 14.f * FMath::Sin(Time * 11.f), 11.f * FMath::Sin(Time * 9.1f + 1.f));
		WantQ = FRotationMatrix::MakeFromXZ(Ant->AttachedAlong, Ant->AttachedUp).ToQuat() * Squirm.Quaternion();
		WantO = P;
		break;
	}
	default:
		WantQ = FRotationMatrix::MakeFromXZ(Ant->AttachedAlong, Ant->AttachedUp).ToQuat();
		WantO = P;
		break;
	}
	const float Alpha = Time < 0.05f ? 1.f : 1.f - FMath::Exp(-Blend * Dt);
	BodyQ = FQuat::Slerp(BodyQ, WantQ, Alpha).GetNormalized();
	BodyO = WantO;

	// ---- the last line of defence: a body that is not a number, or is nowhere near its ant, is stood back on the spot
	if (BodyO.ContainsNaN() || BodyQ.ContainsNaN() || FVector::DistSquared(BodyO, P) > FMath::Square(600.f * VS))
	{
		ResetPose();
	}
}

void UAntBodyComponent::ResetPose()
{
	static int32 Told = 0;
	if (Told < 8)
	{
		++Told;
		UE_LOG(LogTemp, Warning, TEXT("ANTBODY pose reset on %s (mode %d, lod %d)"), *GetNameSafe(Ant), (int32)Mode, Lod);
	}
	FitA = FitB = FitAV = FitBV = FitCV = 0.f;
	FitC = -Ground * VS;
	Lean = LeanV = Shove = ShoveV = FVector::ZeroVector;
	Dip = DipV = Spin = SpinRate = 0.f;
	HeadYaw = HeadPitch = HeadYawV = HeadPitchV = HeadRoll = 0.f;
	JawOpen = JawV = 0.f;
	Acc = Moved = FVector::ZeroVector;
	YawRate = 0.f;
	bGasterInit = false;
	for (FAntRigLeg& L : Legs)
	{
		L.bInit = false;
		L.FootW = FVector::ZeroVector;
	}
	for (FAntRigFeeler& F : Feelers)
	{
		F.Yaw = 25.f; F.Pitch = 40.f; F.Elbow = -70.f; F.Tip = -10.f;
		F.YawV = F.PitchV = F.ElbowV = F.TipV = 0.f;
	}
	const FVector SafeN = N.ContainsNaN() || N.IsNearlyZero() ? FVector::UpVector : N;
	const FVector SafeH = H.ContainsNaN() || H.IsNearlyZero() ? FVector::ForwardVector : H;
	BodyQ = FRotationMatrix::MakeFromZX(SafeN, SafeH).ToQuat();
	BodyO = P;
}

// ---------------------------------------------------------------------------------------------
// Head, jaw, petiole, gaster
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::UpdateSpine(float Dt)
{
	const float G = Ground * VS;
	const FVector ThoraxAt = BodyO + BodyQ.RotateVector(AntB_Thorax * VS);

	// ---- the head: looks where it is going, at what it carries, at food it can take
	if (Lod <= 1 && Time > NextLook)
	{
		NextLook = Time + 0.25f + 0.1f * Hash01(Frame);
		bLookAt = false;
		if (Ant->Carrying)
		{
			LookAt = Ant->Carrying->GetActorLocation();
			bLookAt = true;
		}
		else if (Mode == M_Ground && !Ant->HoldingAnt && (Ant->IsLocallyControlled() || Lod == 0))
		{
			if (const AKitchenLoot* Near = Ant->FindNearestLoot(AAntCharacter::InteractRadius * 2.4f))
			{
				LookAt = Near->GetActorLocation();
				bLookAt = true;
			}
		}
	}
	if (Ant->Carrying && Lod <= 1)
	{
		// the head is locked on its grip, every frame (the load moves under the jaws)
		FVector GripAt;
		if (AntCarry::GripWorld(Ant, GripAt)) { LookAt = GripAt; bLookAt = true; }
	}
	float WantYaw = 0.f, WantPitch = 0.f, WantRoll = 0.f;
	float WantJaw = 0.f;
	const FVector HeadAt = BodyO + BodyQ.RotateVector(AntB_Head * VS);
	if (Mode == M_Ground || Mode == M_Air)
	{
		WantYaw = FMath::Clamp(FMath::RadiansToDegrees(YawRate) * 0.1f, -28.f, 28.f);
		if (SpeedAL < 80.f)
		{
			// standing: the head follows where the player looks up and down
			WantPitch = FMath::Clamp((float)FRotator::NormalizeAxis(Ant->GetBaseAimRotation().Pitch) * 0.35f, -14.f, 22.f);
		}
		if (bLookAt && Lod <= 1)
		{
			const FVector D = BodyQ.UnrotateVector(LookAt - HeadAt);
			if (D.SizeSquared() > 1.f)
			{
				WantYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2((float)D.Y, (float)D.X)), -52.f, 52.f);
				WantPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2((float)D.Z, (float)FVector2D(D.X, D.Y).Size())), -30.f, 48.f);
			}
			WantJaw = Ant->Carrying ? -9.f : 20.f;   // clamped on the load / open for the bite
		}
	}
	// idle life: a look round, the jaws working
	if (Idle == 3)
	{
		const float T = IdleT / FMath::Max(0.1f, IdleDur);
		WantYaw += 38.f * FMath::Sin(T * 2.f * PI) * (IdleSide ? 1.f : -1.f);
		WantRoll += 9.f * FMath::Sin(T * 4.f * PI);
	}
	else if (Idle == 1)
	{
		WantYaw += (IdleSide ? 14.f : -14.f);
		WantRoll += (IdleSide ? 14.f : -14.f);
		WantPitch += 8.f;
	}
	if (Idle == 4 || (Mode == M_Ground && Frac(Time * 0.37f + Hash01(41)) < 0.06f))
	{
		WantJaw += 16.f * FMath::Abs(FMath::Sin(Time * 21.f));
	}
	if (Mode == M_Held || (Ant->GrippedBy && Mode != M_Carried))
	{
		WantJaw = 22.f + 12.f * FMath::Sin(Time * 16.f);   // snapping at whatever has it
		WantYaw += 24.f * FMath::Sin(Time * 6.1f);
		WantPitch += 14.f * FMath::Sin(Time * 8.3f);
	}
	if (Mode == M_Tumble || Mode == M_Back) { WantJaw = 18.f; WantPitch = -12.f; }
	if (Act.Kind == AntAct::Bite && ActAge < 0.32f)
	{
		// the bite: gape, lunge, snap shut
		WantJaw = ActAge < 0.07f ? 36.f : -12.f;
		WantPitch += 16.f * Bump(ActAge, 0.f, 0.06f, 0.12f, 0.32f);
	}
	if ((Act.Kind == AntAct::Flinch || Act.Kind == AntAct::Knock) && ActAge < 0.2f) { WantJaw = 26.f; }
	WantYaw += 15.f * Stun * FMath::Sin(Time * 21.f);
	WantPitch -= 12.f * Stun + 9.f * Wound;
	WantPitch += 7.f * Hurt * FMath::PerlinNoise1D(Time * 29.f);
	Spring(HeadYaw, HeadYawV, WantYaw, 8.f, 0.7f, Dt);
	Spring(HeadPitch, HeadPitchV, WantPitch, 8.f, 0.7f, Dt);
	HeadRoll += (WantRoll - HeadRoll) * (1.f - FMath::Exp(-10.f * Dt));
	Spring(JawOpen, JawV, WantJaw, 13.f, 0.55f, Dt);
	JawOpen = FMath::Clamp(JawOpen, -14.f, 44.f);

	// ---- the gaster: a weight on a thin waist. Its tip is a mass on a spring: it lags in a turn and bounces on a stop.
	const FVector Pivot = BodyO + BodyQ.RotateVector(AntB_Gaster * VS);
	float TipUp = 1.5f * FMath::Sin(Time * 2.1f) - 13.f * Wound - 9.f * Effort - 8.f * Stun;
	if (Ant->bSprinting && SpeedAL > 1500.f) { TipUp += 7.f; }
	if (Mode == M_Held) { TipUp = -34.f + 12.f * FMath::Sin(Time * 14.f); }
	else if (Mode == M_Carried || Mode == M_Tumble) { TipUp = -28.f; }
	else if (Mode == M_Back) { TipUp = -20.f + 10.f * FMath::Sin(Time * 15.f); }
	if (NumAlive <= 3) { TipUp -= 10.f; }
	const float GLen = 30.f * VS;
	const FVector Rest0(-1.f, 0.f, -0.10f);
	const FVector RestL = FVector(-1.f, 0.f, -0.10f + FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(TipUp, -45.f, 30.f)))).GetSafeNormal();
	const FVector RestW = BodyQ.RotateVector(RestL);
	const FVector RestTip = Pivot + RestW * GLen;
	if (!bGasterInit || FVector::DistSquared(GasterTip, RestTip) > FMath::Square(GLen * 2.5f) || Lod >= 2)
	{
		GasterTip = RestTip;
		GasterTipV = FVector::ZeroVector;
		bGasterInit = true;
	}
	else
	{
		const int32 Steps = Dt > 0.02f ? 2 : 1;
		for (int32 s = 0; s < Steps; ++s)
		{
			Spring(GasterTip, GasterTipV, RestTip, 7.f, 0.4f, Dt / Steps);
		}
		// it never goes through the floor: a low body drags its gaster along the ground
		if (Mode == M_Ground)
		{
			const float Floor = (float)FVector::DotProduct(GasterTip - (P + N * FitC), N) - 8.5f * VS;
			if (Floor < 0.f)
			{
				GasterTip -= N * Floor;
				GasterTipV = FVector::VectorPlaneProject(GasterTipV, N);
			}
		}
	}
	FVector DirW = (GasterTip - Pivot).GetSafeNormal(UE_SMALL_NUMBER, RestW);
	if (FVector::DotProduct(DirW, RestW) < 0.9f)
	{
		DirW = (DirW + RestW * 2.2f).GetSafeNormal(UE_SMALL_NUMBER, RestW);
	}
	GasterTip = Pivot + DirW * GLen;
	const FQuat Swing = FQuat::FindBetweenNormals(Rest0.GetSafeNormal(), BodyQ.UnrotateVector(DirW));
	const FQuat PetQ = FQuat::Slerp(FQuat::Identity, Swing, 0.35f).GetNormalized();

	// Mandibles locked on it: it is stretched like rubber between whoever pulls.
	float Stretch = 0.f;
	if (Ant->GrippedBy && IsValid(Ant->GrippedBy) && !Ant->GrippedBy->bDead)
	{
		Stretch = 2.2f + 2.2f * (0.5f + 0.5f * FMath::Sin(Time * 38.f));
	}

	// ---- place the body pieces (children follow their parents)
	Head->SetRelativeLocationAndRotation(AntB_Head - AntB_Thorax + FVector(Stretch * 0.6f, 0.f, 0.f), FRotator(HeadPitch, HeadYaw, HeadRoll));
	Jaw->SetRelativeRotation(FRotator(-JawOpen, 0.f, 0.f));
	Petiole->SetRelativeLocationAndRotation(AntB_Petiole - AntB_Thorax - FVector(Stretch, 0.f, 0.f), PetQ);
	Gaster->SetRelativeLocationAndRotation(AntB_Gaster - AntB_Petiole - FVector(Stretch * 1.4f, 0.f, 0.f), PetQ.Inverse() * Swing);
	Thorax->SetWorldLocationAndRotation(ThoraxAt, BodyQ);
	ThoraxW = Thorax->GetComponentTransform();
	HeadW = Head->GetComponentTransform();
	PetioleW = Petiole->GetComponentTransform();
	GasterW = Gaster->GetComponentTransform();

	// ---- idle life: after standing a while it cleans a feeler, shifts its weight, looks round
	if (Idle != 0)
	{
		IdleT += Dt;
		if (IdleT >= IdleDur || StillTime < 0.3f || Mode != M_Ground)
		{
			if (Idle == 1)
			{
				Legs[IdleSide].bInit = false;   // the foreleg goes back down
			}
			Idle = 0;
			NextIdle = Time + 2.f + 5.f * FMath::FRand();
		}
	}
	else if (DevIdle > 0 || (Lod == 0 && Mode == M_Ground && StillTime > 2.2f && Time > NextIdle && !Ant->Carrying && Stun < 0.2f && !Ant->bWantsFire && NumAlive == 6))
	{
		Idle = DevIdle > 0 ? DevIdle : 1 + (int32)(FMath::FRand() * 3.99f);
		DevIdle = 0;
		IdleT = 0.f;
		IdleDur = Idle == 1 ? 1.9f : Idle == 2 ? 1.6f : Idle == 3 ? 1.5f : 0.6f;
		IdleSide = FMath::RandBool() ? 1 : 0;
	}
}

// ---------------------------------------------------------------------------------------------
// Legs: coxa, femur, tibia, tarsus
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::UpdateLegs(float Dt)
{
	const AntArt::FPart& ArtCoxa = AntRig::Art().Coxa;
	const AntArt::FPart& ArtFemur = AntRig::Art().Femur;
	const FVector Up = BodyQ.GetAxisZ();
	const FVector Fwd = BodyQ.GetAxisX();
	const FVector Right = BodyQ.GetAxisY();
	const float LK = Ant->LegLength;
	const float Thick = Ant->LegThickness * VS;
	const bool bDash = Act.Kind == AntAct::Dash && ActAge < 0.45f;

	for (int32 i = 0; i < 6; ++i)
	{
		FAntRigLeg& L = Legs[i];
		const FVector HipL = AntRig::HipLocal(i);
		L.HipW = BodyO + BodyQ.RotateVector(HipL * VS);
		const FVector Home = AntRig::HomeLocal(i);
		const float Side = (float)L.Side;

		if (L.bLost)
		{
			// a stump: the coxa stays
			const FVector Dir = (Right * Side * 0.8f - Up * 0.6f).GetSafeNormal();
			AntArt::PlaceAlong(L.Coxa, ArtCoxa, L.HipW - Dir * 2.f * VS, L.HipW + Dir * 5.f * VS, Up, 8.f * Thick, false, false);
			continue;
		}

		// ---- where the foot is when it does not stand on anything
		if (Mode != M_Ground)
		{
			FVector T(Home.X * 0.85f, Home.Y * 0.8f, -Ground * 0.8f);
			const float Ph = Time + i * 1.7f;
			float Rate = 30.f;
			switch (Mode)
			{
			case M_Air:
				if (bDash)
				{
					// the lunge: forelegs thrown forward, hind legs trailing
					T = L.Pair == 0 ? FVector(Home.X + 16.f, Home.Y * 0.6f, -Ground * 0.3f) : L.Pair == 1 ? FVector(Home.X - 4.f, Home.Y * 0.95f, -Ground * 0.5f) : FVector(Home.X - 24.f, Home.Y * 0.55f, -Ground * 0.25f);
				}
				else
				{
					const float Flail = AirTime > 0.5f ? 9.f : 3.f;
					T += FVector(FMath::Sin(Ph * 9.f) * Flail, FMath::Cos(Ph * 7.f) * Flail * 0.5f, FMath::Sin(Ph * 11.f) * Flail * 0.6f);
					T.Z = -Ground * (0.8f + 0.25f * FMath::Clamp(-(float)FVector::DotProduct(V, N) / 600.f, 0.f, 1.f));   // reaching for the ground as it comes
				}
				break;
			case M_Tumble:
				T = FVector(Home.X * 0.55f, Home.Y * 0.5f, -Ground * 0.45f) + FVector(FMath::Sin(Ph * 13.f), FMath::Cos(Ph * 11.f), FMath::Sin(Ph * 17.f)) * 8.f;
				Rate = 40.f;
				break;
			case M_Back:
				T = FVector(Home.X * 0.6f, Home.Y * 0.55f, -Ground * 0.75f) + FVector(FMath::Sin(Ph * 15.f + L.Group * PI) * 11.f, FMath::Cos(Ph * 12.f) * 5.f, FMath::Sin(Ph * 19.f) * 5.f);
				Rate = 45.f;
				break;
			case M_Flip:
				T = FVector(Home.X, Home.Y * 1.1f, -Ground * 0.9f);
				Rate = 40.f;
				break;
			case M_Held:
				T = FVector(Home.X * 0.8f, Home.Y * 0.75f, -Ground * 0.7f)
					+ FVector(FMath::Sin(Ph * (10.f + i)) * 13.f, FMath::Cos(Ph * (8.f + i * 0.7f)) * 8.f, FMath::Sin(Ph * (12.f + i * 0.5f) + 1.f) * 8.f);
				Rate = 45.f;
				break;
			case M_Carried:
				T = FVector(Home.X * 0.35f + 2.f, Home.Y * 0.3f, -Ground * 0.45f) + FVector(0.f, 0.f, FMath::Sin(Ph * 3.f) * 1.2f);
				Rate = 12.f;
				break;
			default:   // a link of a living chain: forelegs grip the next ant, hind legs are gripped
				T = L.Pair == 0 ? FVector(38.f, 9.f * Side, -5.f) : L.Pair == 1 ? FVector(6.f, 22.f * Side, -15.f) : FVector(-46.f, 9.f * Side, -3.f);
				Rate = 14.f;
				break;
			}
			const FVector Want = BodyO + BodyQ.RotateVector(T * VS);
			L.FootW = (L.FootW.IsZero() || FVector::DistSquared(L.FootW, Want) > FMath::Square(120.f * VS)) ? Want : FMath::VInterpTo(L.FootW, Want, Dt, Rate);
			L.Lift = 0.7f;
			L.T = 1.f;
			L.Base = nullptr;
			L.Foot = L.FootW;
			L.bGround = false;
		}
		else if (Idle == 1 && i == IdleSide)
		{
			// cleaning a feeler: the foreleg comes up and the feeler is drawn through it, again and again
			const FAntRigFeeler& F = Feelers[IdleSide];
			const float T = IdleT / FMath::Max(0.1f, IdleDur);
			const float In = Bump(T, 0.f, 0.14f, 0.86f, 1.f);
			const float Stroke = 0.5f + 0.5f * FMath::Sin(IdleT * 2.f * PI * 2.6f);
			const FVector Comb = FMath::Lerp(F.ElbowW, F.TipW, 0.1f + 0.75f * Stroke) + Up * 0.5f * VS;
			L.FootW = FMath::Lerp(FootWorld(L), Comb, In);
			L.Lift = In;
		}
		else if (!L.bGround && L.T >= 1.f)
		{
			// nothing under this foot (over an edge): it hangs a little below the body and feels about
			const FVector Hang = BodyO + BodyQ.RotateVector(FVector(Home.X * 0.9f, Home.Y * 0.85f, -Ground * 0.9f) * VS) + Up * FMath::Sin(Time * 9.f + i) * 2.f * VS;
			L.FootW = FMath::VInterpTo(L.FootW, Hang, Dt, 20.f);
			L.Lift = 0.5f;
		}

		// ---- three bones: the coxa swings out of its socket, femur and tibia fold at the knee, the tarsus lies on the ground
		const float Lc = AntRig::CoxaLen * VS;
		const float Lf = AntRig::FemurLen * L.Len * LK * VS;
		const float Lt = AntRig::TibiaLen * L.Len * LK * VS;
		const float Ls = AntRig::TarsusLen * L.Len * LK * VS;
		const FVector Foot = L.FootW;
		const FVector SurfN = L.bGround && Mode == M_Ground ? N : Up;
		FVector Out = FVector::VectorPlaneProject(Foot - L.HipW, Up).GetSafeNormal(UE_SMALL_NUMBER, Right * Side);
		const FVector CoxaDir = (Out * 0.78f - Up * 0.62f).GetSafeNormal();
		L.CoxaEnd = L.HipW + CoxaDir * Lc;
		// the tarsus: flat on the surface when the foot stands, hanging toe-down when it is lifted
		const float Tilt = FMath::DegreesToRadians(24.f + 46.f * L.Lift);
		const FVector OutS = FVector::VectorPlaneProject(Foot - L.CoxaEnd, SurfN).GetSafeNormal(UE_SMALL_NUMBER, Out);
		FVector Ankle = Foot - OutS * Ls * FMath::Cos(Tilt) + SurfN * Ls * FMath::Sin(Tilt);
		const FVector Ankle0 = Ankle;
		FVector Pole;
		L.Knee = NpcRig::TwoBone(L.CoxaEnd, Ankle, Lf, Lt, Up * 0.8f + Out * 0.6f, Up, Pole);
		L.Ankle = Ankle;
		// (out of reach: the whole foot comes off the ground rather than the leg stretching)
		const FVector Toe = Foot + (Ankle - Ankle0);

		if (Lod >= 3 && !bLegsShown)
		{
			continue;
		}
		const FVector KneeUp = FVector::VectorPlaneProject(Up, (L.Knee - L.CoxaEnd)).GetSafeNormal(UE_SMALL_NUMBER, Up);
		AntArt::PlaceAlong(L.Coxa, ArtCoxa, L.HipW - CoxaDir * 2.5f * VS, L.CoxaEnd + CoxaDir * 1.f * VS, Up, 8.f * Thick, false, false);
		// (the femur's ball sits a little inside the cup at the top of the tibia)
		AntArt::PlaceAlong(L.Femur, ArtFemur, L.CoxaEnd, L.Knee + (L.Knee - L.CoxaEnd).GetSafeNormal() * 1.3f * VS, KneeUp, 7.5f * Thick, AAntCharacter::ArtFlipFemur, false);
		AntRig::PlaceLimb(L.Tibia, L.Knee, Ankle, Up, 5.5f * Thick, AntB_TibiaThick);
		AntRig::PlaceLimb(L.Tarsus, Ankle, Toe + (Toe - Ankle).GetSafeNormal() * 0.4f * VS, SurfN, AntRig::TarsusAcross * Thick, AntB_TarsusThick);
	}
}

// ---------------------------------------------------------------------------------------------
// Feelers: scape + two halves of the flagellum, on springs
// ---------------------------------------------------------------------------------------------

void UAntBodyComponent::UpdateFeelers(float Dt)
{
	if (!bLegsShown)
	{
		return;
	}
	const AntArt::FPart& ArtScape = AntRig::Art().Scape;
	const FQuat HQ = HeadW.GetRotation();
	const float Sp = FMath::Clamp(SpeedAL / 1800.f, 0.f, 1.5f);
	const bool bGroundMode = Mode == M_Ground;
	for (int32 s = 0; s < 2; ++s)
	{
		FAntRigFeeler& F = Feelers[s];
		const float Side = s ? 1.f : -1.f;
		const float Ph = Time * (3.f + 2.2f * Sp) + s * 1.7f + Hash01(50) * 6.f;
		// the resting sweep, faster when it runs
		float Yaw = 26.f + 11.f * FMath::Sin(Ph * 0.7f);
		float Pitch = 38.f + 9.f * FMath::Sin(Ph);
		float Elbow = -70.f + 7.f * FMath::Sin(Ph * 1.3f + 1.f);
		float Tip = -8.f;
		// at a run they are laid back and low, feeling the way
		Yaw += 14.f * Sp;
		Pitch -= 13.f * Sp;
		Elbow += 16.f * Sp;
		// tapping the ground ahead, one feeler then the other
		if (bGroundMode && Lod == 0)
		{
			if (Time > F.TapAt)
			{
				F.TapUntil = Time + 0.11f;
				F.TapAt = Time + (SpeedAL > 150.f ? 0.28f : 0.55f) * (0.7f + 0.8f * FMath::FRand());
			}
			if (Time < F.TapUntil)
			{
				Pitch = 6.f;
				Elbow = -46.f;
				Tip = -16.f;
				Yaw -= 8.f;
			}
		}
		if (!bGroundMode)
		{
			if (Mode == M_Held || Mode == M_Back || Mode == M_Tumble)
			{
				// whipping about
				Yaw += 26.f * FMath::Sin(Time * 17.f + s * 2.f);
				Pitch += 24.f * FMath::Sin(Time * 13.f + s);
			}
			else if (Mode == M_Air)
			{
				Yaw += 16.f; Pitch += 20.f; Elbow += 24.f;   // streaming back in the wind
			}
			else if (Mode == M_Carried)
			{
				Yaw = 40.f; Pitch = 70.f; Elbow = -120.f;     // folded, like a carried nestmate
			}
		}
		if (Idle == 1 && s == IdleSide)
		{
			// held out low in front, to be drawn through the foreleg's comb
			Yaw = 6.f; Pitch = 2.f; Elbow = -52.f; Tip = -8.f;
		}
		// hurt: they droop; dizzy: they hang and shake
		Pitch -= 26.f * Wound + 22.f * Stun;
		Elbow -= 26.f * Wound + 18.f * Stun;
		Yaw += 9.f * Stun * FMath::Sin(Time * 19.f + s);
		Pitch += 16.f * Hurt * FMath::PerlinNoise1D(Time * 37.f + s * 7.f);
		const bool bBroken = s == BrokenFeeler && Wound > 0.55f;
		if (bBroken)
		{
			// broken: the flagellum just hangs from the elbow and swings
			Pitch = 4.f; Yaw = 34.f; Elbow = -150.f + 14.f * FMath::Sin(Time * 6.f); Tip = -28.f;
		}
		Spring(F.Yaw, F.YawV, Yaw, 8.5f, 0.42f, Dt);
		Spring(F.Pitch, F.PitchV, Pitch, 8.5f, 0.42f, Dt);
		Spring(F.Elbow, F.ElbowV, Elbow, bBroken ? 3.f : 9.5f, bBroken ? 0.15f : 0.4f, Dt);
		Spring(F.Tip, F.TipV, Tip, 11.f, 0.35f, Dt);
		F.Yaw = FMath::Clamp(F.Yaw, -25.f, 95.f);
		F.Pitch = FMath::Clamp(F.Pitch, -45.f, 100.f);
		F.Elbow = FMath::Clamp(F.Elbow, -170.f, 10.f);

		F.BaseW = HeadW.TransformPosition(AntRig::FeelerLocal(s));
		const FQuat Q1 = HQ * FRotator(F.Pitch, F.Yaw * Side, 0.f).Quaternion();
		const FQuat Q2 = Q1 * FRotator(F.Elbow, 0.f, 0.f).Quaternion();
		const FQuat Q3 = Q2 * FRotator(F.Tip, 0.f, 0.f).Quaternion();
		F.ElbowW = F.BaseW + Q1.GetAxisX() * AntRig::ScapeLen * VS;
		F.MidW = F.ElbowW + Q2.GetAxisX() * AntRig::FlagALen * VS;
		F.TipW = F.MidW + Q3.GetAxisX() * AntRig::FlagBLen * VS;
		const FVector UpHint = Q1.GetAxisZ();
		AntArt::PlaceAlong(F.Scape, ArtScape, F.BaseW, F.ElbowW, UpHint, 3.4f * VS, false, false);
		AntRig::PlaceLimb(F.FlagA, F.ElbowW, F.MidW, UpHint, 2.0f * VS, AntB_FlagAThick);
		AntRig::PlaceLimb(F.FlagB, F.MidW, F.TipW, UpHint, 3.2f * VS, AntB_FlagBThick);
	}
}

void UAntBodyComponent::PlacePieces()
{
	// (every piece is placed where it is computed: the spine in UpdateSpine, the limbs in UpdateLegs and UpdateFeelers)
}

// ---------------------------------------------------------------------------------------------
// Death: the pose goes to the corpse
// ---------------------------------------------------------------------------------------------

bool UAntBodyComponent::Snapshot(FAntBodySnapshot& S) const
{
	if (!Thorax || !Ant)
	{
		return false;
	}
	S.VS = VS;
	S.Thorax = Thorax->GetComponentTransform();
	S.Head = Head->GetComponentTransform();
	S.Petiole = Petiole->GetComponentTransform();
	S.Gaster = Gaster->GetComponentTransform();
	S.JawOpen = JawOpen;
	for (int32 i = 0; i < 6; ++i)
	{
		const FAntRigLeg& L = Legs[i];
		S.bLeg[i] = !L.bLost;
		S.Hip[i] = L.HipW;
		S.Knee[i] = L.Knee;
		S.Ankle[i] = L.Ankle;
		S.Toe[i] = L.FootW;
	}
	for (int32 s = 0; s < 2; ++s)
	{
		S.FeelerBase[s] = Feelers[s].BaseW;
		S.FeelerElbow[s] = Feelers[s].ElbowW;
		S.FeelerMid[s] = Feelers[s].MidW;
		S.FeelerTip[s] = Feelers[s].TipW;
	}
	S.Velocity = V;
	S.Up = N;
	S.Fwd = H;
	for (int32 i = 0; i < AntMat::Count; ++i)
	{
		S.Mats[i] = Mats.IsValidIndex(i) ? Mats[i] : nullptr;
	}
	S.GlandMat = GlandMat;
	S.LegThick = Ant->LegThickness;
	return true;
}

void UAntBodyComponent::Die()
{
	if (!Ant || !Thorax)
	{
		return;
	}
	if (Time <= 0.f || FVector::DistSquared(Thorax->GetComponentLocation(), Ant->GetActorLocation()) > FMath::Square(90.f * VS))
	{
		// never drawn on this machine, or not since it was moved somewhere else: stand it where the capsule is now
		for (FAntRigLeg& L : Legs)
		{
			L.bInit = false;
			L.FootW = FVector::ZeroVector;
		}
		bGasterInit = false;
		Time = 0.f;
		LodDt = 0.016f;
		Frame = -1 - (int32)(Ant->GetUniqueID() & 3);   // (so that this update is not one the far-away schedule skips)
		Update(0.016f, true);
	}
	FAntBodySnapshot S;
	Snapshot(S);
	S.Velocity = Ant->GetVelocity();
	Corpse = AAntCorpse::Spawn(Ant->GetWorld(), S, Ant->DeathKind, FVector(Ant->DeathDir), Ant);
	Thorax->SetVisibility(false, true);
	for (UStaticMeshComponent* C : Limbs)
	{
		if (C) { C->SetVisibility(false); }
	}
	for (UStaticMeshComponent* C : Wounds)
	{
		if (C) { C->SetVisibility(false); }
	}
	bLegsShown = false;
}

void UAntBodyComponent::CorpseToLoot()
{
	if (AAntCorpse* C = Corpse.Get())
	{
		C->RideLoot();
	}
}

FVector UAntBodyComponent::HeadLocation() const
{
	return HeadW.GetLocation();
}

FString UAntBodyComponent::Describe() const
{
	int32 Stance = 0, OnGround = 0;
	for (const FAntRigLeg& L : Legs)
	{
		if (!L.bLost && L.T >= 1.f) { ++Stance; }
		if (!L.bLost && L.bGround) { ++OnGround; }
	}
	return FString::Printf(TEXT("mode %d lod %d speed %.0f phase %.2f stride %.0f duty %.2f legs %d (standing %d, on ground %d) fit a %.2f b %.2f c %.1f stand-dip %.1f wound %.2f effort %.2f stun %.2f hurt %.2f head %.0f/%.0f jaw %.0f idle %d wall %.2f"),
		(int32)Mode, Lod, SpeedAL, Phase, Stride, Duty, NumAlive, Stance, OnGround, FitA, FitB, FitC / VS, Dip, Wound, Effort, Stun, Hurt, HeadYaw, HeadPitch, JawOpen, Idle, Wall);
}

// ---------------------------------------------------------------------------------------------
// Rules next to the body: what kind of death, dev commands
// ---------------------------------------------------------------------------------------------

namespace AntBodyRules
{

static int32 GForceKind = -1;
static FVector GForceDir = FVector::ZeroVector;

void ForceDeath(int32 Kind, const FVector& Dir)
{
	GForceKind = Kind;
	GForceDir = Dir;
}

void ClassifyDeath(AAntCharacter* Ant, const AController* Killer, const AActor* Causer)
{
	if (!Ant || Ant->Creature != 0)
	{
		return;
	}
	if (GForceKind >= 0)
	{
		Ant->DeathKind = (uint8)GForceKind;
		if (!GForceDir.IsNearlyZero()) { Ant->DeathDir = GForceDir.GetSafeNormal(); }
		GForceKind = -1;
		return;
	}
	if (Ant->DeathKind == AntDeath::Torn)
	{
		return;
	}
	const UWorld* W = Ant->GetWorld();
	const bool bFresh = W && W->GetTimeSeconds() - Ant->LastHazardTime < 3.f && !Ant->LastHazard.IsEmpty();
	const FString& Hz = Ant->LastHazard;
	const AHumanCharacter* Human = Cast<AHumanCharacter>(Causer);
	uint8 Kind = Ant->DeathKind;   // 0, or 1 when a foot, a paw or a newspaper came down on it
	if (Human && Human->bSpraying)
	{
		Kind = AntDeath::Curl;
	}
	else if (Kind == AntDeath::Squashed)
	{
	}
	else if (bFresh)
	{
		// the kitchen says what happened in the words of the kill feed
		if (Hz.Contains(TEXT("лизан")) || Hz.Contains(TEXT("засосан"))) { Kind = AntDeath::Eaten; }
		else if (Hz.Contains(TEXT("раздавлен")) || Hz.Contains(TEXT("придавлен")) || Hz.Contains(TEXT("попал под")) || Hz.Contains(TEXT("наигралась")) || Hz.Contains(TEXT("мышелов"))) { Kind = AntDeath::Squashed; }
		else if (Hz.Contains(TEXT("лапой")) || Hz.Contains(TEXT("метлой")) || Hz.Contains(TEXT("пинок")) || Hz.Contains(TEXT("ткнул"))) { Kind = AntDeath::Flung; }
		else if (Hz.Contains(TEXT("сгорел")) || Hz.Contains(TEXT("поджарился"))) { Kind = AntDeath::Burnt; }
		else if (Hz.Contains(TEXT("утонул"))) { Kind = AntDeath::Drowned; }
		else if (Hz.Contains(TEXT("замёрз"))) { Kind = AntDeath::Frozen; }
		else if (!Killer && !Causer && !Hz.Contains(TEXT("армией")) && !Hz.Contains(TEXT("испытаниях"))) { Kind = AntDeath::Curl; }
	}
	const FVector Vel = Ant->GetVelocity();
	if (Kind == AntDeath::Collapse && !Ant->IsCrawling() && !Ant->IsAttached())
	{
		// it was in the air when it died: thrown, or falling
		Kind = Vel.Size() > 420.f * AntKit::SpeedK ? AntDeath::Flung : Vel.Z < -320.f ? AntDeath::Fall : AntDeath::Collapse;
	}
	if ((Kind == AntDeath::Flung || Kind == AntDeath::Fall) && Vel.Size() > 60.f)
	{
		Ant->DeathDir = Vel.GetSafeNormal();
	}
	else if (Kind == AntDeath::Flung)
	{
		// who did it is not passed down here: away from the nearest big body (the paw, the broom, the finger are part of it)
		const FVector Me = Ant->GetActorLocation();
		float Best = 4000.f;
		FVector From = Me - Ant->GetActorForwardVector();
		for (TActorIterator<ACritter> It(Ant->GetWorld()); It; ++It)
		{
			const float D = (float)FVector::Dist2D(It->GetActorLocation(), Me);
			if (D < Best) { Best = D; From = It->GetActorLocation(); }
		}
		for (TActorIterator<AHumanCharacter> It(Ant->GetWorld()); It; ++It)
		{
			const float D = (float)FVector::Dist2D(It->GetActorLocation(), Me);
			if (D < Best) { Best = D; From = It->GetActorLocation(); }
		}
		Ant->DeathDir = (Me - From).GetSafeNormal2D(UE_SMALL_NUMBER, Ant->GetActorForwardVector());
	}
	Ant->DeathKind = Kind;
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || !A[0].Equals(TEXT("ant"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	const FString Op = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	static TWeakObjectPtr<AAntCharacter> Picked;
	static TWeakObjectPtr<ACameraActor> Cam;
	static TWeakObjectPtr<AActor> Holder;
	AAntCharacter* S = Picked.IsValid() ? Picked.Get() : Me;
	auto MakeLamp = [](ACameraActor* C, float Intensity)
	{
		// a lamp on the camera: a black ant on a pale floor is a silhouette without it
		UPointLightComponent* Lamp = C->FindComponentByClass<UPointLightComponent>();
		if (!Lamp)
		{
			Lamp = NewObject<UPointLightComponent>(C);
			Lamp->SetupAttachment(C->GetRootComponent());
			Lamp->SetRelativeLocation(FVector(-4.f, 6.f, 8.f));
			Lamp->IntensityUnits = ELightUnits::Unitless;
			Lamp->bUseInverseSquaredFalloff = false;
			Lamp->LightFalloffExponent = 1.5f;
			Lamp->AttenuationRadius = 400.f;
			Lamp->CastShadows = false;
			Lamp->RegisterComponent();
		}
		Lamp->SetIntensity(Intensity);
	};

	if (Op == TEXT("on"))
	{
		// ant on me | ant on bot N | ant on enemy N: who the next "ant ..." commands are for
		Picked = nullptr;
		const FString Who = A.IsValidIndex(2) ? A[2].ToLower() : TEXT("me");
		if (Who != TEXT("me"))
		{
			const uint8 Team = Who == TEXT("enemy") ? 1 : 0;
			int32 Skip = (int32)Num(3, 0.f);
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				if (!It->bDead && It->Creature == 0 && It->Team == Team && !It->IsPlayerControlled() && Skip-- <= 0)
				{
					Picked = *It;
					break;
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTBODY on %s"), Picked.IsValid() ? *Picked->GetName() : TEXT("me"));
		return true;
	}
	if (Op == TEXT("slomo"))
	{
		UGameplayStatics::SetGlobalTimeDilation(W, FMath::Clamp(Num(2, 1.f), 0.01f, 4.f));
		return true;
	}
	if (Op == TEXT("shot"))
	{
		// ant shot name: a screenshot (no UI) into Saved/Screenshots/antbody/name.png, taken at the end of this very frame
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("antbody") / (A.IsValidIndex(2) ? A[2] : FString(TEXT("shot"))) + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(File, false, false);
		return true;
	}
	if (Op == TEXT("burst"))
	{
		// ant burst N dt [prefix]: N screenshots, one every dt game seconds, starting now (the script itself only ticks 4 times a second)
		static FTimerHandle Handle;
		static int32 Left = 0, Index = 0;
		static FString Prefix;
		Left = FMath::Clamp((int32)Num(2, 8.f), 1, 64);
		Index = 0;
		Prefix = A.IsValidIndex(4) ? A[4] : FString(TEXT("f"));
		TWeakObjectPtr<UWorld> WeakW = W;
		auto Take = [WeakW]()
		{
			if (Left <= 0 || !WeakW.IsValid())
			{
				return;
			}
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("antbody") / FString::Printf(TEXT("%s%02d.png"), *Prefix, Index), false, false);
			++Index;
			if (--Left <= 0) { WeakW->GetTimerManager().ClearTimer(Handle); }
		};
		Take();
		if (Left > 0)
		{
			W->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Take), FMath::Max(0.0005f, Num(3, 0.01f)), true);
		}
		return true;
	}
	if (Op == TEXT("exec") && PC)
	{
		// ant exec <console command>
		TArray<FString> Rest(A);
		Rest.RemoveAt(0, 2);
		PC->ConsoleCommand(FString::Join(Rest, TEXT(" ")), false);
		return true;
	}
	if (Op == TEXT("camc") && PC)
	{
		// ant camc dx dy dz [fov] [lamp]: look at the newest dead body from that far away (world axes)
		FVector At;
		if (AAntCorpse::NewestAt(W, At))
		{
			if (!Cam.IsValid()) { Cam = W->SpawnActor<ACameraActor>(); }
			if (ACameraActor* C = Cam.Get())
			{
				C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				C->SetActorLocationAndRotation(At + Vec(2), (-Vec(2)).Rotation());
				C->GetCameraComponent()->SetFieldOfView(Num(5, 30.f));
				C->GetCameraComponent()->bConstrainAspectRatio = false;
				MakeLamp(C, Num(6, 5.f));
				PC->SetViewTargetWithBlend(C, 0.f);
			}
		}
		return true;
	}
	if (Op == TEXT("corpses"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTBODY corpses %s"), *AAntCorpse::Stats(W));
		return true;
	}
	if (!S)
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTBODY no ant for: %s"), *FString::Join(A, TEXT(" ")));
		return true;
	}
	const FVector P = S->GetActorLocation();
	const FVector Up = S->GetVisualUp();
	const FVector Fwd = FVector::VectorPlaneProject(S->GetActorForwardVector(), Up).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(Up, Fwd);
	auto Local = [&](const FVector& L) { return Fwd * L.X + Right * L.Y + Up * L.Z; };

	if (Op == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTBODY info %s at %s hp %.0f/%.0f legs lost %d dead %d kind %d crawling %d: %s"), *S->GetName(), *P.ToCompactString(), S->Health, S->MaxHealth,
			S->NumLostLegs(), S->bDead ? 1 : 0, (int32)S->DeathKind, S->IsCrawling() ? 1 : 0, S->Rig ? *S->Rig->Describe() : TEXT("the old ant (no rig)"));
	}
	else if (Op == TEXT("load"))
	{
		const AKitchenLoot* L = S->Carrying;
		const FVector D = L ? L->GetActorLocation() - P : FVector::ZeroVector;
		UE_LOG(LogTemp, Display, TEXT("ANTBODY load %s: ahead %.1f right %.1f up %.1f (units), ratio %.2f, carriers %d"), L ? *L->LootName : TEXT("none"),
			FVector::DotProduct(D, Fwd), FVector::DotProduct(D, Right), FVector::DotProduct(D, Up), L ? L->CarryRatio() : 0.f, L ? L->NumCarriers() : 0);
	}
	else if (Op == TEXT("hp")) { S->Health = FMath::Clamp(Num(2, 100.f), 1.f, S->MaxHealth); }
	else if (Op == TEXT("leg"))
	{
		// ant leg N (0..5: front left, front right, middle left, ...) | ant leg -1: all back
		const int32 N = (int32)Num(2, 0.f);
		S->LostLegs = N < 0 ? 0 : (uint8)(S->LostLegs | (1 << FMath::Clamp(N, 0, 5)));
	}
	else if (Op == TEXT("hit")) { S->NoteBodyAct(AntAct::Flinch, Local(Vec(2)), Num(5, 0.6f)); }             // ant hit dx dy dz power: a blow pushing that way (the ant's own axes)
	else if (Op == TEXT("knock")) { S->ApplyLaunch(Local(Vec(2))); }                                          // ant knock vx vy vz: thrown (the ant's own axes, units/s)
	else if (Op == TEXT("stun")) { S->Stagger(Num(2, 1.f), FVector::ZeroVector); }
	else if (Op == TEXT("bite")) { S->BotBite(); }
	else if (Op == TEXT("sprint")) { S->bSprinting = Num(2, 1.f) > 0.5f; }
	else if (Op == TEXT("die"))
	{
		// ant die kind [dx dy dz]: 0 collapse 1 squashed 2 torn 3 flung 4 poisoned 5 fall 6 eaten 7 burnt 8 drowned 9 frozen
		const int32 Kind = (int32)Num(2, 0.f);
		ForceDeath(Kind, A.Num() >= 6 ? Local(Vec(3)) : FVector::ZeroVector);
		if (Kind == AntDeath::Torn) { S->DeathKind = AntDeath::Torn; S->DeathDir = (A.Num() >= 6 ? Local(Vec(3)) : Right).GetSafeNormal(); }
		S->HurtBy(100000.f, TEXT("погиб на испытаниях"));
	}
	else if (Op == TEXT("freeze") && S->Rig) { S->Rig->bDevFreeze = Num(2, 1.f) > 0.5f; }
	else if (Op == TEXT("idle") && S->Rig) { S->Rig->DevIdle = (int32)Num(2, 1.f); }
	else if (Op == TEXT("lod") && S->Rig) { S->Rig->DevLod = (int32)Num(2, -1.f); }                             // ant lod N: force the level of detail 0..3 (-1: by distance again)
	else if (Op == TEXT("hold"))
	{
		// ant hold 1 [up]: pinched and lifted (what the kid's fingers do) | ant hold 0
		if (Num(2, 1.f) > 0.5f)
		{
			if (!Holder.IsValid())
			{
				AActor* H = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
				USceneComponent* Root = NewObject<USceneComponent>(H);
				H->SetRootComponent(Root);
				Root->RegisterComponent();
				Holder = H;
			}
			Holder->SetActorLocationAndRotation(P + Up * Num(3, 30.f), Fwd.Rotation());
			S->SetNpcHold(Holder->GetRootComponent(), FVector::ZeroVector);
		}
		else
		{
			S->SetNpcHold(nullptr, FVector::ZeroVector);
		}
	}
	else if (Op == TEXT("cam") && PC)
	{
		// ant cam off | ant cam dx dy dz lx ly lz [fov] [fix]: from a point of the ant's frame (ahead, right, up) at another one.
		// The camera travels with the ant unless "fix" is given.
		if (A.IsValidIndex(2) && A[2] == TEXT("off"))
		{
			PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f);
		}
		else
		{
			if (!Cam.IsValid()) { Cam = W->SpawnActor<ACameraActor>(); }
			if (ACameraActor* C = Cam.Get())
			{
				const FVector From = P + Local(Vec(2)), To = P + Local(Vec(5));
				C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				C->SetActorLocationAndRotation(From, FRotationMatrix::MakeFromXZ(To - From, Up).Rotator());
				C->GetCameraComponent()->SetFieldOfView(Num(8, 40.f));
				C->GetCameraComponent()->bConstrainAspectRatio = false;
				MakeLamp(C, Num(10, 6.f));
				if (!(A.IsValidIndex(9) && A[9] == TEXT("fix")))
				{
					C->AttachToActor(S, FAttachmentTransformRules::KeepWorldTransform);
				}
				PC->SetViewTargetWithBlend(C, 0.f);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTBODY unknown: %s"), *FString::Join(A, TEXT(" ")));
	}
	return true;
}

} // namespace AntBodyRules
