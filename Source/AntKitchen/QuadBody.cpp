#include "QuadBody.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "QuadCatData.inl"
#include "QuadDogData.inl"

// ---------------------------------------------------------------------------------------------
// Assets, looks, proportions
// ---------------------------------------------------------------------------------------------

namespace
{
	const TCHAR* const FurStep = TEXT("Surf_Fabric");
	const FNpcLook GCatLooks[] =
	{
		{ TEXT("Fur"),      nullptr, 0.f, 0.47f, 0.46f, 0.52f, 0.85f, FurStep },
		{ TEXT("FurDark"),  nullptr, 0.f, 0.21f, 0.20f, 0.25f, 0.85f, FurStep },
		{ TEXT("FurWhite"), nullptr, 0.f, 0.95f, 0.94f, 0.91f, 0.85f, FurStep },
		{ TEXT("EarPink"),  nullptr, 0.f, 0.93f, 0.62f, 0.62f, 0.7f,  nullptr },
		{ TEXT("NosePink"), nullptr, 0.f, 0.90f, 0.48f, 0.50f, 0.45f, nullptr },
		{ TEXT("PawPad"),   nullptr, 0.f, 0.88f, 0.52f, 0.55f, 0.6f,  nullptr },
		{ TEXT("CatEye"),   nullptr, 0.f, 0.50f, 0.80f, 0.22f, 0.2f,  nullptr },
		{ TEXT("Pupil"),    nullptr, 0.f, 0.03f, 0.03f, 0.04f, 0.2f,  nullptr },
		{ TEXT("Whisker"),  nullptr, 0.f, 0.97f, 0.97f, 0.97f, 0.5f,  nullptr },
		{ TEXT("Mouth"),    nullptr, 0.f, 0.42f, 0.10f, 0.10f, 0.5f,  nullptr },
	};
	const FNpcLook GDogLooks[] =
	{
		{ TEXT("Fawn"),      nullptr, 0.f, 0.78f, 0.55f, 0.32f, 0.85f, FurStep },
		{ TEXT("FawnDark"),  nullptr, 0.f, 0.20f, 0.15f, 0.13f, 0.8f,  FurStep },
		{ TEXT("DogWhite"),  nullptr, 0.f, 0.95f, 0.93f, 0.88f, 0.85f, FurStep },
		{ TEXT("EarPink"),   nullptr, 0.f, 0.93f, 0.62f, 0.62f, 0.7f,  nullptr },
		{ TEXT("DogNose"),   nullptr, 0.f, 0.05f, 0.04f, 0.04f, 0.35f, nullptr },
		{ TEXT("PawPad"),    nullptr, 0.f, 0.30f, 0.22f, 0.22f, 0.6f,  nullptr },
		{ TEXT("DogEye"),    nullptr, 0.f, 0.20f, 0.11f, 0.05f, 0.2f,  nullptr },
		{ TEXT("EyeWhite"),  nullptr, 0.f, 0.97f, 0.97f, 0.95f, 0.25f, nullptr },
		{ TEXT("Pupil"),     nullptr, 0.f, 0.03f, 0.03f, 0.04f, 0.2f,  nullptr },
		{ TEXT("Mouth"),     nullptr, 0.f, 0.42f, 0.10f, 0.10f, 0.5f,  nullptr },
		{ TEXT("Tongue"),    nullptr, 0.f, 0.95f, 0.45f, 0.52f, 0.4f,  nullptr },
		{ TEXT("Collar"),    nullptr, 0.f, 0.80f, 0.10f, 0.10f, 0.6f,  nullptr },
		{ TEXT("CollarTag"), nullptr, 0.f, 0.90f, 0.72f, 0.20f, 0.3f,  nullptr },
		{ TEXT("Tooth"),     nullptr, 0.f, 0.97f, 0.96f, 0.90f, 0.3f,  nullptr },
	};

	const TCHAR* const GSegNames[QuadSeg::Count] =
	{
		TEXT("Hips"), TEXT("Belly"), TEXT("Chest"), TEXT("Neck"), TEXT("Head"), TEXT("Jaw"), TEXT("EarL"), TEXT("EarR"),
		TEXT("Tail1"), TEXT("Tail2"), TEXT("Tail3"), TEXT("Tongue"),
		TEXT("FLU"), TEXT("FLL"), TEXT("FLP"), TEXT("FRU"), TEXT("FRL"), TEXT("FRP"),
		TEXT("HLU"), TEXT("HLL"), TEXT("HLP"), TEXT("HRU"), TEXT("HRL"), TEXT("HRP"),
	};

	/** Proportions, straight from the model's joints (uu). */
	struct FQuadDims
	{
		bool bOk = false;
		float HS = 250.f;                 // half the spine: hips joint to shoulders joint is 2 * HS
		FVector Sh, Hip;                  // the joint of a RIGHT leg in the chest's / the hips' frame
		float L1[2], L2[2], Drop[2];      // front / hind: upper, lower, sole below the paw's joint
		FVector NeckAt; float NeckLen;    // chest frame
		FVector JawAt, EarAt, TongueAt;   // head frame (the right ear)
		float TongueLen = 100.f;
		FVector TailAt;                   // hips frame
		float TailLen[3];
		int32 TailN = 0;
		float StandSh, StandHip;          // height of the chest's / the hips' origin, standing
		float RestZ;                      // the same lying on the belly
	};

	const FQuadDims& Dims(uint8 Kind)
	{
		static FQuadDims All[2];
		static bool bDone[2] = { false, false };
		Kind = Kind > 1 ? 1 : Kind;
		FQuadDims& D = All[Kind];
		if (bDone[Kind])
		{
			return D;
		}
		bDone[Kind] = true;
		using namespace QuadSeg;
		const TConstArrayView<FNpcSegDef> J = QuadBody::Segs(Kind);
		if (J.Num() != Count || !J[Hips].Name || !J[Chest].Name || !J[FRU].Name || !J[HRU].Name || !J[Head].Name)
		{
			return D;
		}
		const bool bCat = Kind == 1;
		const float* PawDrop = bCat ? GCatPawDrop : GDogPawDrop;
		D.HS = (float)(J[Chest].Joint.X - J[Hips].Joint.X) * 0.5f;
		D.Sh = J[FRU].Joint - J[Chest].Joint;
		D.Hip = J[HRU].Joint - J[Hips].Joint;
		D.L1[0] = (float)(J[FRU].Joint.Z - J[FRL].Joint.Z);
		D.L2[0] = (float)(J[FRL].Joint.Z - J[FRP].Joint.Z);
		D.L1[1] = (float)(J[HRU].Joint.Z - J[HRL].Joint.Z);
		D.L2[1] = (float)(J[HRL].Joint.Z - J[HRP].Joint.Z);
		D.Drop[0] = PawDrop[0];
		D.Drop[1] = PawDrop[1];
		D.NeckAt = J[Neck].Joint - J[Chest].Joint;
		D.NeckLen = (float)(J[Head].Joint.X - J[Neck].Joint.X);
		D.JawAt = J[Jaw].Name ? J[Jaw].Joint - J[Head].Joint : FVector::ZeroVector;
		D.EarAt = J[EarR].Name ? J[EarR].Joint - J[Head].Joint : FVector::ZeroVector;
		D.TongueAt = J[Tongue].Name ? J[Tongue].Joint - J[Head].Joint : FVector::ZeroVector;
		D.TongueLen = bCat ? 100.f : GDogTongueLen;
		D.TailAt = J[Tail1].Name ? J[Tail1].Joint - J[Hips].Joint : FVector(-D.HS * 0.6f, 0.f, 40.f);
		D.TailN = 0;
		for (int32 k = 0; k < 3; ++k)
		{
			if (!J[Tail1 + k].Name) { break; }
			D.TailLen[k] = (k < 2 && J[Tail1 + k + 1].Name) ? (float)(J[Tail1 + k].Joint.X - J[Tail1 + k + 1].Joint.X) : (D.TailN > 0 ? D.TailLen[D.TailN - 1] : 110.f);
			++D.TailN;
		}
		// Standing: the front legs nearly straight, the hind legs bent the way a cat's (or a bulldog's) are.
		D.StandSh = D.Drop[0] + (bCat ? 0.955f : 0.95f) * (D.L1[0] + D.L2[0]) - (float)D.Sh.Z;
		D.StandHip = D.Drop[1] + (bCat ? 0.80f : 0.87f) * (D.L1[1] + D.L2[1]) - (float)D.Hip.Z;
		D.RestZ = bCat ? 150.f : 172.f;
		D.bOk = true;
		return D;
	}
}

namespace QuadAct
{
float Duration(uint8 Kind, uint8 Arg)
{
	switch (Kind)
	{
	case Stretch: return 2.0f;
	case Wash:    return Arg * 0.1f;
	case Wiggle:  return WiggleTime + 0.05f;
	case Pounce:  return Arg * 0.01f + 0.4f;
	case Swipe:   return 1.0f;
	case Pin:     return Arg * 0.1f;
	case Jump:    return JumpCrouch + Arg * 0.01f + 0.3f;
	case Startle: return 0.55f;
	case Twitch:  return TwitchTime + 0.05f;
	case Lick:    return 0.5f;
	case Eat:     return Arg * 0.1f;
	case Bark:    return Arg * BarkEvery + 0.1f;
	case Shake:   return 1.1f;
	case Beg:     return Arg * 0.1f;
	case Yelp:    return 0.5f;
	default:      return 0.f;
	}
}
}

namespace QuadBody
{

TConstArrayView<FNpcSegDef> Segs(uint8 Kind)
{
	return Kind == 0 ? TConstArrayView<FNpcSegDef>(GDogSegs, UE_ARRAY_COUNT(GDogSegs)) : TConstArrayView<FNpcSegDef>(GCatSegs, UE_ARRAY_COUNT(GCatSegs));
}

const TCHAR* SegName(int32 Seg)
{
	return (Seg >= 0 && Seg < QuadSeg::Count) ? GSegNames[Seg] : TEXT("None");
}

UStaticMesh* Mesh(uint8 Kind, int32 Seg)
{
	const TConstArrayView<FNpcSegDef> J = Segs(Kind);
	return (J.IsValidIndex(Seg) && J[Seg].Name) ? NpcRig::LoadMesh(TEXT("/Game/Npc"), J[Seg].Name) : nullptr;
}

bool Enabled(uint8 Kind)
{
	static int32 State[2] = { -1, -1 };
	Kind = Kind > 1 ? 1 : Kind;
	if (State[Kind] < 0)
	{
		State[Kind] = NpcWorld::NewNpcs() && Dims(Kind).bOk ? 1 : 0;
		const TConstArrayView<FNpcSegDef> J = Segs(Kind);
		for (int32 i = 0; i < J.Num() && State[Kind] == 1; ++i)
		{
			if (J[i].Name && !Mesh(Kind, i)) { State[Kind] = 0; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTNPC %s body: %s"), Kind == 0 ? TEXT("dog") : TEXT("cat"), State[Kind] ? TEXT("jointed (/Game/Npc)") : TEXT("the old primitives"));
	}
	return State[Kind] == 1;
}

void ApplyLook(uint8 Kind, UStaticMeshComponent* C)
{
	if (Kind == 0)
	{
		NpcRig::ApplyLook(C, TConstArrayView<FNpcLook>(GDogLooks, UE_ARRAY_COUNT(GDogLooks)));
	}
	else
	{
		NpcRig::ApplyLook(C, TConstArrayView<FNpcLook>(GCatLooks, UE_ARRAY_COUNT(GCatLooks)));
	}
}

FVector BindJoint(uint8 Kind, int32 Seg)
{
	const TConstArrayView<FNpcSegDef> J = Segs(Kind);
	return J.IsValidIndex(Seg) ? J[Seg].Joint : FVector::ZeroVector;
}

FVector AirPoint(const FVector& From, const FVector& To, float T)
{
	T = FMath::Clamp(T, 0.f, 1.f);
	FVector P = FMath::Lerp(From, To, T);
	const float Arc = FMath::Max(140.f, (float)FVector::Dist2D(From, To) * 0.26f) + FMath::Max(0.f, (float)(To.Z - From.Z)) * 0.3f;
	P.Z += FMath::Sin(T * PI) * Arc;
	return P;
}

float AirTime(const FVector& From, const FVector& To, bool bPounce)
{
	const float D = (float)FVector::Dist2D(From, To), H = FMath::Abs((float)(To.Z - From.Z));
	return bPounce ? FMath::Clamp(D / 2700.f + H / 4000.f, 0.34f, 0.62f) : FMath::Clamp(D / 2100.f + H / 2600.f, 0.42f, 0.9f);
}

float PawDrop(uint8 Kind, bool bHind)
{
	return Dims(Kind).Drop[bHind ? 1 : 0];
}

float TongueLength(uint8 Kind)
{
	return Dims(Kind).TongueLen;
}

// ---------------------------------------------------------------------------------------------
// The pose
// ---------------------------------------------------------------------------------------------

namespace
{
	/** What the pose wants from one paw instead of standing on it: the sole goes to a point (W: how much). */
	struct FPawCtl
	{
		float W = 0.f;
		bool bAbs = false;                    // P is a body-space point; otherwise an offset from under the leg's own joint, turned with its girdle
		FVector P = FVector::ZeroVector;
		float Pitch = 0.f;                    // toes up > 0
	};
}

void Pose(const FQuadIn& In, FQuadRig& R, TArrayView<UStaticMeshComponent*> Segs, TFunctionRef<void(int32, float, const FVector&)> Footfall)
{
	using namespace QuadSeg;
	using namespace NpcRig;
	const uint8 Kind = In.Kind > 1 ? 1 : In.Kind;
	const FQuadDims& D = Dims(Kind);
	if (!D.bOk || Segs.Num() < Count)
	{
		return;
	}
	const bool bCat = Kind == 1;
	const TConstArrayView<FNpcSegDef> Defs = QuadBody::Segs(Kind);
	const float Dt = FMath::Clamp(In.Dt, 0.f, 0.1f);
	const FTransform& Body = In.Body;
	const FVector Origin = Body.GetLocation();
	const float Yaw = (float)Body.Rotator().Yaw;
	const float FloorZ = (float)Origin.Z;
	const FVector Vel(In.Vel.X, In.Vel.Y, 0.f);
	const float Speed = (float)Vel.Size();
	const FVector VelDir = Speed > 1.f ? Vel / Speed : Body.GetUnitAxis(EAxis::X);
	const bool bMoving = Speed > 40.f;
	const uint8 Mode = In.Mode;
	uint8 A = In.Act;
	const float T = In.ActT;
	const float ActDur = QuadAct::Duration(A, In.Arg);
	if (T < 0.f || T > ActDur)
	{
		A = QuadAct::None;
	}
	const FVector TargetB = Body.InverseTransformPosition(In.ActTarget);
	R.Clock += Dt;
	const float C = R.Clock;
	if (!R.bInit)
	{
		R.bInit = true;
		R.HipZ = D.StandHip; R.ShZ = D.StandSh; R.HipX = -D.HS; R.ShX = D.HS;
		R.LastYaw = Yaw;
		R.Neck = bCat ? 40.f : 34.f;
		for (int32 i = 0; i < Count; ++i)
		{
			R.SegPos[i] = Defs[i].Joint;
			R.SegRot[i] = FQuat::Identity;
		}
	}
	{
		const float DYaw = FMath::FindDeltaAngleDegrees(R.LastYaw, Yaw);
		R.LastYaw = Yaw;
		if (Dt > 1.e-4f)
		{
			Approach(R.YawRate, FMath::Clamp(DYaw / Dt, -500.f, 500.f), 8.f, Dt);
		}
	}

	// ---------------------------------------------------------------------------------- posture: what the mode wants
	float HipZT = D.StandHip, ShZT = D.StandSh, HipXT = -D.HS, ShXT = D.HS, BodyYT = 0.f, CurlT = 0.f, RollT = 0.f, ArchT = 0.f;
	float HipPitchT = 0.f, ChestPitchT = 0.f, HipSwayT = 0.f, HipRoll = 0.f;
	float HeadYawT = 0.f, HeadPitchT = bCat ? -4.f : 2.f, HeadRollT = 0.f, NeckT = bCat ? 40.f : 34.f, JawT = 0.f, EarBackT = 0.f, EarOutT = 0.f;
	float TailLiftT = bCat ? 30.f : 48.f, TailCurlT = bCat ? 24.f : 0.f, TailSideT = 0.f, TailWagT = bCat ? 8.f : 12.f, TailHz = bCat ? 0.4f : 2.0f, TailPuffT = 1.f;
	float TongueT = 0.f;
	float BreathHz = 0.5f, BreathAmp = 3.f;
	float Rate = 9.f, HeadRate = 7.f;
	bool bLook = !In.LookAt.IsZero();
	FVector LookB = Body.InverseTransformPosition(In.LookAt);
	bool bWalkFeet = true;        // the walk owns the paws nothing else has taken
	bool bFlying = false;         // off the floor: the legs are not asked to reach it
	bool bAwake = true;
	FPawCtl Paw[4];
	const float SgnOf[4] = { -1.f, 1.f, -1.f, 1.f };

	switch (In.Mood)
	{
	case QuadMood::Hunting:
		if (bCat) { TailLiftT = -4.f; TailCurlT = 6.f; TailWagT = 26.f; TailHz = 2.3f; EarOutT = -8.f; }
		else { TailWagT = 4.f; EarBackT = -8.f; }
		break;
	case QuadMood::Scared:
		TailPuffT = bCat ? 1.75f : 1.f; EarBackT = 62.f; TailLiftT = bCat ? 66.f : -35.f; TailWagT = 0.f;
		break;
	case QuadMood::Happy:
		TailWagT = bCat ? 12.f : 42.f; TailHz = bCat ? 0.8f : 5.5f; TailLiftT += 12.f;
		break;
	case QuadMood::Sulking:
		TailLiftT = -30.f; TailWagT = 0.f; EarBackT = 40.f; HeadPitchT -= 14.f;
		break;
	default:
		break;
	}

	if (!bCat)
	{
		if (bMoving && Mode == QuadMode::Move)
		{
			// The bulldog waddle: the barrel rolls from side to side over those short legs.
			const float Ph = 2.f * PI * R.Gait;
			RollT += 6.5f * FMath::Sin(Ph) * FMath::Min(1.f, Speed / 400.f);
			BodyYT += 16.f * FMath::Sin(Ph) * FMath::Min(1.f, Speed / 400.f);
		}
		if (In.Mood == QuadMood::Happy && Mode != QuadMode::Sleep && Mode != QuadMode::NoseDown)
		{
			// Panting, tongue out.
			TongueT = 62.f + 10.f * FMath::Sin(C * 9.f);
			JawT = 13.f + 4.f * FMath::Sin(C * 9.f);
			BreathHz = 2.6f; BreathAmp = 5.f;
		}
	}

	if (Mode == QuadMode::Sleep)
	{
		bAwake = false;
		bWalkFeet = false;
		bLook = false;
		HipZT = ShZT = D.RestZ;
		TailWagT = 0.f;
		if (bCat)
		{
			// Curled up: nose to tail.
			CurlT = 118.f; RollT = 9.f;
			NeckT = -16.f; HeadYawT = 76.f; HeadPitchT = -18.f; HeadRollT = 14.f; EarBackT = 16.f;
			TailLiftT = -10.f; TailCurlT = 0.f; TailSideT = 58.f;
			BreathHz = 0.27f; BreathAmp = 9.f;
			for (int32 i = 0; i < 4; ++i)
			{
				Paw[i].W = 1.f;
				Paw[i].P = i < 2 ? FVector(40.f, SgnOf[i] * -25.f, 0.f) : FVector(120.f, SgnOf[i] * -10.f, 0.f);
			}
		}
		else
		{
			// Flat out on his belly, hind legs splayed behind him: the bulldog sploot.
			NeckT = -26.f; HeadPitchT = -4.f; EarBackT = 12.f; TailLiftT = 12.f;
			BreathHz = 0.3f; BreathAmp = 15.f;
			JawT = 3.f + 6.f * FMath::Max(0.f, FMath::Sin(R.Breath + 0.6f));   // snoring: the jowls puff on the way out
			for (int32 i = 0; i < 4; ++i)
			{
				Paw[i].W = 1.f;
				Paw[i].P = i < 2 ? FVector(230.f, SgnOf[i] * 25.f, 0.f) : FVector(-260.f, SgnOf[i] * 95.f, 0.f);
				Paw[i].Pitch = i < 2 ? 0.f : -62.f;
			}
		}
	}
	else if (Mode == QuadMode::Sit)
	{
		HipZT = D.RestZ * (bCat ? 1.0f : 0.98f);
		ShZT = D.StandSh + (bCat ? 24.f : 12.f);
		ShXT = D.HS * 0.78f;
		HipXT = ShXT - FMath::Sqrt(FMath::Max(FMath::Square(D.HS * 0.9f), FMath::Square(D.HS * 1.86f) - FMath::Square(ShZT - HipZT)));
		HipPitchT = bCat ? 30.f : 20.f; ChestPitchT = -6.f;
		NeckT = bCat ? 50.f : 42.f;
		if (bCat) { TailLiftT = -14.f; TailCurlT = 0.f; TailSideT = 46.f; TailWagT = 3.f; }
		for (int32 i = 2; i < 4; ++i)
		{
			Paw[i].W = 1.f;
			Paw[i].P = FVector(bCat ? 165.f : 150.f, SgnOf[i] * (bCat ? 45.f : 70.f), 0.f);
		}
	}
	else if (Mode == QuadMode::NoseDown)
	{
		ShZT = D.StandSh * 0.9f;
		NeckT = -40.f; HeadPitchT = -46.f;
		HeadYawT = 24.f * FMath::Sin(C * 2.9f);   // casting about
		EarBackT = -6.f;
		BreathHz = 2.2f; BreathAmp = 2.f;
	}
	else if (Mode == QuadMode::Stalk)
	{
		HipZT = D.StandHip * 0.66f; ShZT = D.StandSh * 0.56f;
		NeckT = 4.f; HeadPitchT = 3.f; EarOutT = -8.f;
		TailLiftT = -10.f; TailCurlT = 5.f;
	}
	else if (Mode == QuadMode::Fright)
	{
		ArchT = 78.f; HipZT = D.StandHip * 1.05f; ShZT = D.StandSh * 1.03f; ShXT = D.HS * 0.84f; HipXT = -D.HS * 0.84f;
		TailLiftT = 80.f; TailCurlT = 4.f; TailPuffT = 1.75f; TailWagT = 0.f;
		EarBackT = 70.f; JawT = 20.f; NeckT = 18.f; HeadPitchT = -4.f;
		BodyYT = 7.f * FMath::Sin(C * 26.f);
	}

	// ---------------------------------------------------------------------------------- actions
	auto Flight = [&](float U)
	{
		// In the air, U 0..1: launched off the hind legs, stretched out, the front paws reaching for the landing.
		bFlying = true;
		bWalkFeet = false;
		const float Pitch = FMath::Lerp(24.f, -30.f, Ease(U));
		const float Base = D.StandHip * 0.92f;
		const float S2 = FMath::Sin(FMath::DegreesToRadians(Pitch)) * D.HS;
		ShZT = Base + S2; HipZT = Base - S2; ShXT = D.HS * 1.04f; HipXT = -D.HS * 1.04f;
		Rate = 26.f;
		const float Reach0 = (D.L1[0] + D.L2[0]) * 0.97f, Reach1 = (D.L1[1] + D.L2[1]) * 0.97f;
		for (int32 i = 0; i < 4; ++i)
		{
			Paw[i].W = 1.f;
			Paw[i].bAbs = true;
			if (i < 2)
			{
				const float Down = EaseIn(Ramp(U, 0.5f, 1.f));
				Paw[i].P = FVector(D.HS + 150.f + 110.f * FMath::Sin(PI * U), SgnOf[i] * (D.Sh.Y * 0.8f), FMath::Lerp(R.ShZ - Reach0 * 0.45f, R.ShZ + (float)D.Sh.Z - Reach0 - D.Drop[0] + 6.f, Down));
				Paw[i].Pitch = FMath::Lerp(-35.f, 12.f, Down);
			}
			else
			{
				const float Tuck = FMath::Sin(PI * FMath::Clamp(U * 1.1f, 0.f, 1.f));
				Paw[i].P = FVector(-D.HS - 60.f - 200.f * (1.f - Ramp(U, 0.55f, 1.f)), SgnOf[i] * (float)D.Hip.Y, FMath::Lerp(R.HipZ + (float)D.Hip.Z - Reach1 - D.Drop[1] + 6.f, R.HipZ - Reach1 * 0.4f, Tuck * 0.8f));
				Paw[i].Pitch = -50.f * (1.f - Ramp(U, 0.6f, 1.f));
			}
		}
		// (the body's origin is on the arc of the jump: the floor it will land on is where the target is)
		for (int32 i = 0; i < 4; ++i)
		{
			Paw[i].P.Z = FMath::Max(Paw[i].P.Z, TargetB.Z);
		}
		TailLiftT = 10.f; TailCurlT = -4.f; TailWagT = 0.f;
		EarBackT = 25.f;
		NeckT = 14.f; HeadPitchT = -14.f * U;
	};

	if (A == QuadAct::Stretch)
	{
		// The bow: front paws walked out, chest to the floor, rump in the air, a yawn.
		const float K = Bump(T, 0.f, 0.55f, 1.25f, 1.9f);
		ShZT = FMath::Lerp(ShZT, D.StandSh * 0.44f, K);
		HipZT = FMath::Lerp(HipZT, D.StandHip * 1.07f, K);
		ShXT += 70.f * K;
		for (int32 i = 0; i < 2; ++i)
		{
			Paw[i].W = Ease(Ramp(T, 0.f, 0.3f)) * (1.f - Ease(Ramp(T, 1.5f, 1.9f)));
			Paw[i].P = FVector(250.f * K, SgnOf[i] * 15.f, 0.f);
		}
		TailLiftT = FMath::Lerp(TailLiftT, 62.f, K); TailCurlT = 14.f;
		JawT = 36.f * Bump(T, 0.45f, 0.75f, 1.05f, 1.35f);
		HeadPitchT = 22.f * K; NeckT = FMath::Lerp(NeckT, 24.f, K); EarBackT = 34.f * K;
		bLook = false;
	}
	else if (A == QuadAct::Wash)
	{
		// Licks the left front paw, then wipes it over the ear and down the face.
		const float W = Bump(T, 0.f, 0.4f, ActDur - 0.45f, ActDur);
		const float Ph = FMath::Fmod(T, 1.7f) / 1.7f;
		const FVector Muzzle = R.SegPos[Head] + R.SegRot[Head].RotateVector(FVector(150.f, -20.f, -70.f));
		const FVector Ear = R.SegPos[Head] + R.SegRot[Head].RotateVector(FVector(40.f, -120.f, 110.f));
		const float Wipe = Ease(Ramp(Ph, 0.55f, 0.95f));
		Paw[0].W = W;
		Paw[0].bAbs = true;
		Paw[0].P = Ph < 0.5f ? Muzzle : FMath::Lerp(Ear, Muzzle, Wipe) + FVector(20.f * FMath::Sin(PI * Wipe), 0.f, 0.f);
		Paw[0].Pitch = 50.f;
		HeadPitchT = (Ph < 0.5f ? -34.f + 6.f * FMath::Sin(C * 15.f) : -18.f) * W;
		HeadYawT = (Ph < 0.5f ? -14.f : -26.f * (1.f - Wipe)) * W;
		HeadRollT = (Ph < 0.5f ? 0.f : -16.f * FMath::Sin(PI * Wipe)) * W;
		NeckT = FMath::Lerp(NeckT, 30.f, W);
		JawT = Ph < 0.5f ? 9.f * FMath::Max(0.f, FMath::Sin(C * 15.f)) * W : 0.f;
		bLook = false;
	}
	else if (A == QuadAct::Wiggle)
	{
		// The player's warning: chest on the floor, eyes locked, the rump wiggling, the hind paws treading.
		const float W = Ease(Ramp(T, 0.f, 0.14f));
		const float Osc = FMath::Sin(2.f * PI * 5.2f * T);
		ShZT = FMath::Lerp(D.StandSh * 0.56f, D.StandSh * 0.40f, W);
		HipZT = FMath::Lerp(D.StandHip * 0.66f, D.StandHip * 0.78f, W);
		HipSwayT = 48.f * Osc * W; HipRoll = 9.f * Osc * W;
		Rate = 18.f;
		for (int32 i = 2; i < 4; ++i)
		{
			Paw[i].W = W;
			Paw[i].P = FVector(40.f, 0.f, 26.f * FMath::Max(0.f, Osc * SgnOf[i]));
		}
		NeckT = 2.f; EarOutT = -10.f;
		TailLiftT = 6.f; TailCurlT = 4.f; TailWagT = 34.f; TailHz = 5.2f;
		bLook = true;
		LookB = TargetB;
	}
	else if (A == QuadAct::Pounce)
	{
		const float Fly = FMath::Max(0.05f, In.Arg * 0.01f);
		if (T < Fly)
		{
			Flight(T / Fly);
		}
		else
		{
			// Down on the front paws, the rump still coming.
			const float K = 1.f - Ease(Ramp(T, Fly, Fly + 0.35f));
			ShZT = FMath::Lerp(ShZT, D.StandSh * 0.6f, K); HipZT = FMath::Lerp(HipZT, D.StandHip * 0.9f, K);
			Rate = 16.f;
		}
		JawT = 16.f * Ramp(T, Fly * 0.6f, Fly);
		bLook = true;
		LookB = TargetB;
	}
	else if (A == QuadAct::Jump)
	{
		const float Fly = FMath::Max(0.05f, In.Arg * 0.01f);
		if (T < QuadAct::JumpCrouch)
		{
			// Gathering itself: looks where it is going, sinks on its haunches.
			const float K = Ease(Ramp(T, 0.f, QuadAct::JumpCrouch * 0.8f));
			HipZT = FMath::Lerp(HipZT, D.StandHip * 0.56f, K); ShZT = FMath::Lerp(ShZT, D.StandSh * 0.8f, K);
			Rate = 16.f;
			TailWagT = 18.f; TailHz = 3.f;
		}
		else if (T < QuadAct::JumpCrouch + Fly)
		{
			Flight((T - QuadAct::JumpCrouch) / Fly);
		}
		else
		{
			const float K = 1.f - Ease(Ramp(T, QuadAct::JumpCrouch + Fly, ActDur));
			ShZT = FMath::Lerp(ShZT, D.StandSh * 0.62f, K); HipZT = FMath::Lerp(HipZT, D.StandHip * 0.8f, K);
			Rate = 16.f;
		}
		bLook = true;
		LookB = TargetB + FVector(0.f, 0.f, 150.f);
	}
	else if (A == QuadAct::Startle || A == QuadAct::Yelp)
	{
		// Straight up in the air, all four paws off the floor.
		const float K = FMath::Sin(PI * Ramp(T, 0.f, 0.42f));
		const float H = A == QuadAct::Startle ? 150.f : 90.f;
		HipZT += H * K; ShZT += H * 0.9f * K;
		Rate = 30.f;
		for (int32 i = 0; i < 4; ++i)
		{
			Paw[i].W = Ramp(T, 0.f, 0.05f) * (1.f - Ramp(T, 0.36f, 0.44f));
			Paw[i].P = FVector(0.f, SgnOf[i] * 20.f, H * 0.9f * K);
			Paw[i].Pitch = -30.f * K;
		}
		if (A == QuadAct::Startle) { ArchT = 70.f; TailPuffT = 1.75f; TailLiftT = 80.f; EarBackT = 70.f; JawT = 24.f; }
		else { TailLiftT = -40.f; EarBackT = 55.f; JawT = 26.f * Bump(T, 0.f, 0.08f, 0.2f, 0.4f); HeadPitchT = 20.f; }
		TailWagT = 0.f;
	}
	else if (A == QuadAct::Swipe)
	{
		// A front paw goes up and hangs there (the warning), then bats across the floor through the target.
		const int32 s = In.Arg & 1;
		const float Sg = SgnOf[s];
		const float Hit = QuadAct::SwipeHit;
		const FVector Up(D.HS + 110.f, Sg * 230.f, D.StandSh * 0.95f);
		const FVector A1 = TargetB + FVector(-20.f, Sg * 190.f, 30.f), A2 = TargetB + FVector(30.f, -Sg * 170.f, 20.f);
		Paw[s].W = Ease(Ramp(T, 0.f, 0.16f)) * (1.f - Ease(Ramp(T, 0.72f, 1.f)));
		Paw[s].bAbs = true;
		Paw[s].P = T < Hit - 0.07f ? Up + FVector(0.f, 0.f, 14.f * FMath::Sin(C * 30.f))
			: FMath::Lerp(FMath::Lerp(Up, A1, Ramp(T, Hit - 0.07f, Hit - 0.02f)), A2, EaseOut(Ramp(T, Hit - 0.02f, Hit + 0.09f)));
		Paw[s].Pitch = T < Hit - 0.07f ? 35.f : 0.f;
		const float Lean = Bump(T, Hit - 0.1f, Hit, Hit + 0.1f, 0.8f);
		ShZT = FMath::Lerp(ShZT, D.StandSh * 0.72f, Lean);
		BodyYT = -Sg * 30.f * Ease(Ramp(T, 0.f, 0.2f)) * (1.f - Ramp(T, 0.7f, 1.f));   // the weight goes onto the other front leg
		HeadRollT = Sg * 10.f;
		TailWagT = 30.f; TailHz = 4.f;
		Rate = 14.f;
		bLook = true;
		LookB = TargetB;
	}
	else if (A == QuadAct::Pin)
	{
		// A paw on it. Got you.
		const int32 s = TargetB.Y >= 0.f ? 1 : 0;
		const float Down = EaseIn(Ramp(T, 0.f, QuadAct::PinHit));
		Paw[s].W = Ease(Ramp(T, 0.f, 0.08f)) * (1.f - Ease(Ramp(T, ActDur - 0.28f, ActDur)));
		Paw[s].bAbs = true;
		Paw[s].P = FMath::Lerp(TargetB + FVector(-50.f, 0.f, 200.f), TargetB, Down);
		Paw[s].Pitch = 25.f * (1.f - Down);
		ShZT = FMath::Lerp(ShZT, D.StandSh * 0.74f, Down);
		HeadRollT = 14.f * FMath::Sin(C * 1.7f);
		TailWagT = 22.f; TailHz = 2.5f;
		bLook = true;
		LookB = TargetB;
	}
	else if (A == QuadAct::Twitch)
	{
		// The dog's warning: the nose stops dead over the ant and twitches, the ears come up, the tail freezes.
		const float W = Ease(Ramp(T, 0.f, 0.12f));
		ShZT = FMath::Lerp(ShZT, D.StandSh * 0.86f, W); ShXT += 26.f * W;
		EarBackT = -16.f * W; EarOutT = -10.f * W;
		TailWagT = 0.f; TailLiftT += 14.f;
		HeadRate = 16.f;
		bLook = true;
		LookB = TargetB + FVector(0.f, 0.f, 60.f + 16.f * FMath::Sin(2.f * PI * 12.f * T));
		JawT = 5.f * Ramp(T, 0.35f, QuadAct::TwitchTime);
	}
	else if (A == QuadAct::Lick)
	{
		const float Ext = Ease(Ramp(T, 0.f, QuadAct::LickHit)) * (1.f - Ease(Ramp(T, QuadAct::LickHit + 0.07f, 0.4f)));
		const FVector Mouth = R.SegPos[Head] + R.SegRot[Head].RotateVector(D.TongueAt);
		TongueT = (float)FVector::Dist(Mouth, TargetB) * Ext;
		JawT = 26.f * Bump(T, 0.f, 0.06f, 0.3f, 0.48f);
		ShZT *= 0.88f;
		HeadRate = 18.f;
		bLook = true;
		LookB = TargetB + FVector(0.f, 0.f, 50.f);
	}
	else if (A == QuadAct::Eat)
	{
		const float W = Bump(T, 0.f, 0.3f, ActDur - 0.3f, ActDur);
		ShZT = FMath::Lerp(ShZT, D.StandSh * 0.86f, W);
		JawT = (12.f + 11.f * FMath::Sin(2.f * PI * 3.4f * T)) * W;
		HeadRollT = 5.f * FMath::Sin(2.f * PI * 1.7f * T) * W;
		TailWagT = 34.f; TailHz = 4.5f;
		bLook = true;
		LookB = TargetB + FVector(0.f, 0.f, 40.f + 14.f * FMath::Sin(2.f * PI * 3.4f * T));
	}
	else if (A == QuadAct::Bark)
	{
		const float B = FMath::Fmod(T, QuadAct::BarkEvery) / QuadAct::BarkEvery;
		const float K = T < In.Arg * QuadAct::BarkEvery ? Bump(B, 0.f, 0.12f, 0.3f, 0.6f) : 0.f;
		JawT = 32.f * K; HeadPitchT += 16.f * K; NeckT += 8.f * K;
		ShZT += 26.f * K; ShXT -= 20.f * K;
		Rate = 22.f; HeadRate = 20.f;
		for (int32 i = 0; i < 2; ++i)
		{
			Paw[i].W = K;
			Paw[i].P = FVector(0.f, 0.f, 22.f);
		}
		EarBackT = 14.f * K;
		TailWagT = 0.f; TailLiftT += 10.f;
		if (!In.ActTarget.IsZero()) { bLook = true; LookB = TargetB; }
	}
	else if (A == QuadAct::Shake)
	{
		const float W = Bump(T, 0.f, 0.15f, 0.85f, 1.1f);
		const float Ph = 2.f * PI * 8.5f * T;
		RollT = 19.f * FMath::Sin(Ph) * W; HipRoll = 16.f * FMath::Sin(Ph - 1.4f) * W;
		HeadRollT = 30.f * FMath::Sin(Ph + 1.1f) * W;
		EarOutT = 46.f * FMath::Abs(FMath::Sin(Ph + 1.1f)) * W;
		JawT = 8.f * W;
		Rate = 40.f; HeadRate = 40.f;
		TailWagT = 30.f; TailHz = 8.5f;
		bLook = false;
	}
	else if (A == QuadAct::Beg)
	{
		const float W = Bump(T, 0.f, 0.4f, ActDur - 0.4f, ActDur);
		NeckT = FMath::Lerp(NeckT, 58.f, W);
		TailWagT = 44.f; TailHz = 6.f;
		Paw[0].W = W * FMath::Max(0.f, FMath::Sin(C * 2.4f));
		Paw[0].P = FVector(40.f, 0.f, 90.f);
		Paw[0].Pitch = 30.f;
		JawT = 10.f * W;   // panting
		TongueT = 0.f;
		bLook = true;
		LookB = TargetB;
	}
	else if (Mode == QuadMode::Air)
	{
		Flight(0.5f);
	}

	Approach(R.HipZ, HipZT, Rate, Dt); Approach(R.ShZ, ShZT, Rate, Dt);
	Approach(R.HipX, HipXT, Rate, Dt); Approach(R.ShX, ShXT, Rate, Dt);
	Approach(R.BodyY, BodyYT, Rate * 1.5f, Dt); Approach(R.HipSway, HipSwayT, 40.f, Dt);
	Approach(R.Curl, CurlT, 4.5f, Dt); Approach(R.Roll, RollT, Rate, Dt); Approach(R.Arch, ArchT, 12.f, Dt);
	Approach(R.HipPitch, HipPitchT, Rate, Dt); Approach(R.ChestPitch, ChestPitchT, Rate, Dt);
	// Leaning into a turn at speed.
	Approach(R.Lean, FMath::Clamp(R.YawRate * Speed * 0.00006f, -14.f, 14.f), 6.f, Dt);

	// ---------------------------------------------------------------------------------- the spine's two ends
	const float CurlRad = FMath::DegreesToRadians(R.Curl);
	const bool bCurled = FMath::Abs(CurlRad) > 0.03f;
	const float Chord = bCurled ? FMath::Sin(CurlRad * 0.5f) / (CurlRad * 0.5f) : 1.f;
	const float Sag = bCurled ? (2.f * D.HS / CurlRad) * (1.f - FMath::Cos(CurlRad * 0.5f)) : 0.f;
	const float HipYaw = -R.Curl * 0.5f, ChestYaw = R.Curl * 0.5f;
	R.DipVel[0] += (-R.Dip[0] * 120.f - R.DipVel[0] * 14.f) * Dt;
	R.DipVel[1] += (-R.Dip[1] * 120.f - R.DipVel[1] * 14.f) * Dt;
	R.Dip[0] = FMath::Clamp(R.Dip[0] + R.DipVel[0] * Dt, -70.f, 30.f);
	R.Dip[1] = FMath::Clamp(R.Dip[1] + R.DipVel[1] * Dt, -70.f, 30.f);
	FVector H(R.HipX * Chord, R.BodyY + R.HipSway, R.HipZ + R.Dip[1]);
	FVector S(R.ShX * Chord, R.BodyY, R.ShZ + R.Dip[0]);
	const FQuat YawH = RZ(HipYaw), YawC = RZ(ChestYaw);
	auto LegOff = [&](int32 i) { const FVector& O = i < 2 ? D.Sh : D.Hip; return FVector(O.X, SgnOf[i] * O.Y, O.Z); };
	auto HomeB = [&](int32 i)
	{
		const FVector J = (i < 2 ? S : H) + (i < 2 ? YawC : YawH).RotateVector(LegOff(i) * FVector(1.f, bCat ? 1.f : 1.12f, 0.f));
		return FVector(J.X + (i < 2 ? 12.f : (bCat ? 40.f : 10.f)), J.Y, 0.f);
	};
	auto HomeW = [&](int32 i) { FVector P = Body.TransformPosition(HomeB(i)); P.Z = FloorZ; return P; };

	// ---------------------------------------------------------------------------------- paws
	for (int32 i = 0; i < 4; ++i)
	{
		FNpcFoot& F = R.Feet[i];
		if (!F.bInit || FVector::DistSquared2D(F.Pos, HomeW(i)) > FMath::Square(900.f) || FMath::Abs((float)F.Pos.Z - FloorZ) > 400.f)
		{
			F = FNpcFoot();   // spawned, teleported or landed on another level: under the body
			F.Pos = HomeW(i);
			F.Yaw = Yaw;
			F.bInit = true;
		}
	}
	bool bDrive[4] = { false, false, false, false };
	FVector SoleB[4];
	float PawPitch[4] = { 0.f, 0.f, 0.f, 0.f };
	for (int32 i = 0; i < 4; ++i)
	{
		FNpcFoot& F = R.Feet[i];
		const FPawCtl& P = Paw[i];
		if (P.W > 0.001f)
		{
			bDrive[i] = true;
			const FVector Planted = Body.InverseTransformPosition(F.Pos) + FVector(0.f, 0.f, F.Lift);
			const FVector Want = P.bAbs ? P.P : HomeB(i) + (i < 2 ? YawC : YawH).RotateVector(FVector(P.P.X, P.P.Y, 0.f)) + FVector(0.f, 0.f, P.P.Z);
			if (P.W > 0.98f)
			{
				// The pose owns the paw: it is where the pose puts it.
				SoleB[i] = Want;
				F.Pos = Body.TransformPosition(FVector(Want.X, Want.Y, 0.f));
				F.Pos.Z = FloorZ;
				F.Lift = (float)Want.Z;
				F.T = FMath::Abs(F.Lift) > 4.f ? 0.5f : 1.f;
				F.Yaw = Yaw;
			}
			else
			{
				SoleB[i] = FMath::Lerp(Planted, Want, P.W);
			}
			PawPitch[i] = P.Pitch * P.W;
		}
	}
	for (int32 i = 0; i < 4; ++i)
	{
		FNpcFoot& F = R.Feet[i];
		if (R.bDriven[i] && !bDrive[i] && F.T < 1.f)
		{
			F.From = F.Pos;   // let go of in the air: it comes down as a step
			F.From.Z = FloorZ + F.Lift;
			F.FromYaw = F.Yaw;
			F.T = 0.05f;
			F.Dur = 0.2f;
		}
		R.bDriven[i] = bDrive[i];
	}

	// The gait. Each paw stays where it landed and swings to its next place when its turn comes.
	const float Stride = bCat ? 430.f : 340.f;
	const float Cyc = 0.75f + Speed / Stride * 0.62f;   // cycles per second
	const bool bGallop = Speed > (bCat ? 1000.f : 1050.f);
	const bool bTrot = !bGallop && Speed > (bCat ? 520.f : 230.f);
	static const float OffWalk[4] = { 0.25f, 0.75f, 0.f, 0.5f }, OffTrot[4] = { 0.f, 0.5f, 0.5f, 0.f }, OffGallop[4] = { 0.5f, 0.62f, 0.f, 0.1f };
	const float* Off = bGallop ? OffGallop : (bTrot ? OffTrot : OffWalk);
	const float SwingDur = FMath::Clamp((bGallop ? 0.5f : (bTrot ? 0.42f : 0.3f)) / Cyc, 0.09f, 0.34f);
	auto StartSwing = [&](int32 i, float Dur)
	{
		FNpcFoot& F = R.Feet[i];
		F.From = F.Pos;
		F.From.Z = FloorZ;
		F.FromYaw = F.Yaw;
		F.T = 0.f;
		F.Dur = Dur;
	};
	if (bWalkFeet)
	{
		if (bMoving)
		{
			if (!R.bWasMoving)
			{
				R.Gait = FMath::FloorToFloat(R.Gait) + 0.99f;   // setting off: a hind paw goes at once
			}
			const float Old = R.Gait;
			R.Gait += Dt * Cyc;
			for (int32 i = 0; i < 4; ++i)
			{
				const bool bTurn = FMath::FloorToInt(Old + Off[i]) != FMath::FloorToInt(R.Gait + Off[i]);
				const bool bBehind = FVector::DistSquared2D(R.Feet[i].Pos, HomeW(i)) > FMath::Square(bCat ? 300.f : 230.f);
				if ((bTurn || bBehind) && R.Feet[i].T >= 1.f && !bDrive[i])
				{
					StartSwing(i, SwingDur);
				}
			}
			R.NextIdleStep = In.Now + 0.3f;
		}
		else if (In.Now >= R.NextIdleStep)
		{
			// Standing: shuffle a paw back under the body when it has turned or stopped mid-stride.
			int32 Worst = -1;
			float WorstErr = 55.f;
			bool bAllDown = true;
			for (int32 i = 0; i < 4; ++i)
			{
				if (bDrive[i]) { continue; }
				if (R.Feet[i].T < 1.f) { bAllDown = false; break; }
				const float Err = (float)FVector::Dist2D(R.Feet[i].Pos, HomeW(i)) + FMath::Abs(FMath::FindDeltaAngleDegrees(R.Feet[i].Yaw, Yaw)) * 1.2f;
				if (Err > WorstErr) { WorstErr = Err; Worst = i; }
			}
			if (bAllDown && Worst >= 0)
			{
				StartSwing(Worst, 0.2f);
				R.NextIdleStep = In.Now + 0.24f;
			}
		}
	}
	R.bWasMoving = bMoving;
	for (int32 i = 0; i < 4; ++i)
	{
		FNpcFoot& F = R.Feet[i];
		if (bDrive[i])
		{
			continue;
		}
		if (F.T < 1.f)
		{
			F.T = FMath::Min(1.f, F.T + Dt / FMath::Max(0.05f, F.Dur));
			const float Lead = bMoving ? FMath::Min(bCat ? 260.f : 190.f, Speed * FMath::Max(0.f, 1.f / Cyc - SwingDur) * 0.5f) : 0.f;
			FVector To = HomeW(i) + Vel * ((1.f - F.T) * F.Dur) + VelDir * Lead;
			To.Z = FloorZ;
			const float E = Ease(F.T);
			const float FromLift = (float)F.From.Z - FloorZ;
			F.Pos = FMath::Lerp(FVector(F.From.X, F.From.Y, FloorZ), To, E);
			F.Yaw = F.FromYaw + FMath::FindDeltaAngleDegrees(F.FromYaw, Yaw) * E;
			const float Height = bMoving ? (bCat ? 42.f : 34.f) + FMath::Min(Speed, 1300.f) * 0.075f : 44.f;
			F.Lift = FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * F.T)), 0.8f) * Height + FromLift * (1.f - E);
			F.Pitch = -24.f * FMath::Sin(PI * F.T);
			if (F.T >= 1.f)
			{
				F.Pos = To;
				F.Lift = 0.f;
				F.Pitch = 0.f;
				const float Strength = FMath::Clamp(Speed / 900.f, 0.25f, 1.f);
				R.DipVel[i < 2 ? 0 : 1] -= (bCat ? 240.f : 420.f) * Strength;
				Footfall(i, Strength, To);
			}
		}
		SoleB[i] = Body.InverseTransformPosition(F.Pos) + FVector(0.f, 0.f, F.Lift);
		PawPitch[i] = F.Pitch;
	}

	// ---------------------------------------------------------------------------------- the body never rides higher than its legs reach
	if (!bFlying)
	{
		for (int32 g = 0; g < 2; ++g)
		{
			FVector& Anchor = g == 0 ? S : H;
			const float Reach = (D.L1[g] + D.L2[g]) * 0.985f;
			float Drop = 0.f;
			for (int32 i = g * 2; i < g * 2 + 2; ++i)
			{
				if (Paw[i].bAbs && Paw[i].W > 0.5f)
				{
					continue;   // a paw in the air on purpose does not pull the body down
				}
				const FVector J = Anchor + (g == 0 ? YawC : YawH).RotateVector(LegOff(i));
				const FVector Wrist = SoleB[i] + FVector(0.f, 0.f, D.Drop[g]);
				const float Flat = (float)FVector::Dist2D(J, Wrist);
				const float MaxZ = (float)Wrist.Z + FMath::Sqrt(FMath::Max(0.f, Reach * Reach - Flat * Flat));
				Drop = FMath::Max(Drop, (float)J.Z - MaxZ);
			}
			Anchor.Z -= FMath::Min(Drop, 260.f);
		}
	}
	const FVector Line = S - H;
	const float Phi = Deg(FMath::Atan2((float)Line.Z, FMath::Max(1.f, (float)Line.Size2D())));   // how far the spine points up
	R.Breath += Dt * 2.f * PI * BreathHz;
	const float Br = FMath::Sin(R.Breath);
	const FQuat QH = RZ(HipYaw) * RY(-(Phi + R.HipPitch)) * RX(R.Roll + HipRoll - R.Lean);
	const FQuat QC = RZ(ChestYaw) * RY(-(Phi + R.ChestPitch)) * RX(R.Roll - R.Lean);
	const FQuat QB = RY(-Phi) * RX(R.Roll + HipRoll * 0.5f - R.Lean);
	auto Set = [&](int32 Seg, const FVector& P, const FQuat& Q)
	{
		if (!Segs[Seg] || !Defs[Seg].Name || P.ContainsNaN() || Q.ContainsNaN())
		{
			return;
		}
		R.SegPos[Seg] = P;
		R.SegRot[Seg] = Q.GetNormalized();
		Segs[Seg]->SetRelativeLocationAndRotation(P, R.SegRot[Seg]);
	};
	Set(Hips, H, QH);
	Set(Chest, S + FVector(0.f, 0.f, BreathAmp * 0.5f * Br), QC);
	Set(Belly, (H + S) * 0.5f + FVector(0.f, -Sag, R.Arch + BreathAmp * Br), QB);

	// ---------------------------------------------------------------------------------- legs
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 g = i < 2 ? 0 : 1;
		const FQuat& Q = g == 0 ? QC : QH;
		const FVector J = (g == 0 ? S : H) + Q.RotateVector(LegOff(i));
		const FQuat QPaw = RZ(FMath::FindDeltaAngleDegrees(Yaw, R.Feet[i].Yaw) + (g == 0 ? ChestYaw : HipYaw)) * RY(-PawPitch[i]);
		FVector Wrist = SoleB[i] + QPaw.RotateVector(FVector(0.f, 0.f, D.Drop[g]));
		// Elbows point back, knees point forward; both a little out.
		const FVector PoleHint = Q.RotateVector(g == 0 ? FVector(-1.f, SgnOf[i] * 0.22f, 0.25f) : FVector(1.f, SgnOf[i] * 0.3f, 0.1f));
		FVector Pole;
		const FVector Mid = TwoBone(J, Wrist, D.L1[g], D.L2[g], PoleHint, Q.RotateVector(FVector(g == 0 ? -1.f : 1.f, 0.f, 0.f)), Pole);
		const FVector XHint = Q.RotateVector(FVector::ForwardVector);
		const int32 U = FLU + i * 3;
		Set(U, J, BoneRot((Mid - J).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f)), XHint));
		Set(U + 1, Mid, BoneRot((Wrist - Mid).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f)), XHint));
		Set(U + 2, Wrist, QPaw);
	}

	// ---------------------------------------------------------------------------------- neck and head
	const FVector NeckBase = S + QC.RotateVector(D.NeckAt);
	if (bLook)
	{
		const FVector D3 = LookB - (NeckBase + FVector(0.f, 0.f, D.NeckLen * 0.6f));
		const float Flat = (float)D3.Size2D();
		HeadYawT = FMath::Clamp(FMath::FindDeltaAngleDegrees(ChestYaw, Deg(FMath::Atan2((float)D3.Y, (float)D3.X))), bCat ? -115.f : -80.f, bCat ? 115.f : 80.f) * FMath::Clamp(Flat / 220.f, 0.f, 1.f);
		const float Elev = FMath::Clamp(Deg(FMath::Atan2((float)D3.Z, FMath::Max(1.f, Flat))), -75.f, 60.f);
		HeadPitchT = Elev * 0.75f;
		NeckT = FMath::Clamp(NeckT + Elev * 0.6f, -50.f, 75.f);
	}
	else if (bAwake && A == QuadAct::None)
	{
		// Nothing in particular: the eyes wander, the head follows.
		if (In.Now > R.NextGlance)
		{
			R.NextGlance = In.Now + FMath::FRandRange(1.4f, 4.2f);
			R.Glance = FMath::FRandRange(-55.f, 55.f);
			R.GlancePitch = FMath::FRandRange(-14.f, 8.f);
		}
		if (Mode != QuadMode::NoseDown)
		{
			HeadYawT += bMoving ? R.Glance * 0.3f : R.Glance;
			HeadPitchT += R.GlancePitch;
		}
	}
	if (In.Now > R.NextEarFlick && Mode != QuadMode::Fright)
	{
		R.NextEarFlick = In.Now + FMath::FRandRange(1.2f, bAwake ? 5.f : 8.f);
		R.EarFlick[FMath::RandRange(0, 1)] = 1.f;
	}
	R.EarFlick[0] = FMath::Max(0.f, R.EarFlick[0] - Dt * 5.f);
	R.EarFlick[1] = FMath::Max(0.f, R.EarFlick[1] - Dt * 5.f);
	Approach(R.HeadYaw, HeadYawT - R.Lean * 1.5f, HeadRate, Dt);
	Approach(R.HeadPitch, HeadPitchT, HeadRate, Dt);
	Approach(R.HeadRoll, HeadRollT, HeadRate * 1.4f, Dt);
	Approach(R.Neck, NeckT, HeadRate, Dt);
	Approach(R.Jaw, JawT, 26.f, Dt);
	Approach(R.EarBack, EarBackT, 14.f, Dt);
	Approach(R.EarOut, EarOutT, 20.f, Dt);
	const float NeckYaw = FMath::DegreesToRadians(ChestYaw + R.HeadYaw * 0.5f);
	const float NeckElev = FMath::DegreesToRadians(FMath::Clamp(R.Neck + (Phi + R.ChestPitch) * 0.6f, -70.f, 85.f));
	const FVector NeckDir(FMath::Cos(NeckElev) * FMath::Cos(NeckYaw), FMath::Cos(NeckElev) * FMath::Sin(NeckYaw), FMath::Sin(NeckElev));
	const FVector HeadJ = NeckBase + NeckDir * D.NeckLen;
	const FQuat QHead = RZ(ChestYaw + R.HeadYaw) * RY(-R.HeadPitch) * RX(R.HeadRoll + (R.Roll - R.Lean) * 0.5f);
	Set(Neck, NeckBase, AlongX(NeckDir, QC.RotateVector(FVector::UpVector) + FVector(0.f, 0.f, 0.5f)));
	Set(Head, HeadJ, QHead);
	Set(Jaw, HeadJ + QHead.RotateVector(D.JawAt), QHead * RY(R.Jaw));
	for (int32 e = 0; e < 2; ++e)
	{
		const float Sg = e == 0 ? -1.f : 1.f;
		const float Flick = FMath::Sin(R.EarFlick[e] * PI) * 26.f;
		Set(EarL + e, HeadJ + QHead.RotateVector(FVector(D.EarAt.X, Sg * D.EarAt.Y, D.EarAt.Z)), QHead * RX(-Sg * R.EarOut) * RY(-(R.EarBack + Flick)));
	}
	if (Defs[Tongue].Name && Segs[Tongue])
	{
		Approach(R.Tongue, TongueT, 60.f, Dt);
		const FVector Mouth = HeadJ + QHead.RotateVector(D.TongueAt);
		const FVector Dir = (A == QuadAct::Lick) ? (TargetB - Mouth).GetSafeNormal(UE_SMALL_NUMBER, QHead.GetForwardVector())
			: QHead.RotateVector(FVector(0.86f, 0.f, -0.5f));   // lolling out over the lower teeth
		Set(Tongue, Mouth, AlongX(Dir, QHead.GetUpVector()));
		const float Len = FMath::Max(0.04f, R.Tongue / FMath::Max(1.f, D.TongueLen));
		if (FMath::Abs(Len - R.TongueSet) > 0.01f)
		{
			R.TongueSet = Len;
			Segs[Tongue]->SetRelativeScale3D(FVector(QuadBody::Scale * Len, QuadBody::Scale, QuadBody::Scale));
			Segs[Tongue]->SetVisibility(Len > 0.08f);
		}
	}

	// ---------------------------------------------------------------------------------- tail
	if (D.TailN > 0)
	{
		Approach(R.TailLift, TailLiftT, 6.f, Dt); Approach(R.TailCurl, TailCurlT, 6.f, Dt); Approach(R.TailSide, TailSideT, 5.f, Dt);
		Approach(R.TailWag, TailWagT, 6.f, Dt); Approach(R.TailPuff, TailPuffT, 10.f, Dt);
		R.WagPhase += Dt * 2.f * PI * TailHz;
		FVector P = H + QH.RotateVector(D.TailAt);
		for (int32 k = 0; k < D.TailN; ++k)
		{
			const float Wag = R.TailWag * FMath::Sin(R.WagPhase - k * 0.9f) * (bCat ? (k + 1) / 3.f : 1.f);
			const float E = FMath::DegreesToRadians(FMath::Clamp(R.TailLift + R.TailCurl * k + (bCat && k == D.TailN - 1 ? 8.f * FMath::Sin(C * 3.1f) : 0.f), -80.f, 100.f));
			const float Yw = FMath::DegreesToRadians(HipYaw + 180.f - (R.TailSide * (k + 1) + Wag - FMath::Clamp(R.YawRate * 0.05f, -25.f, 25.f) * (k + 1)));
			FVector Dir(FMath::Cos(E) * FMath::Cos(Yw), FMath::Cos(E) * FMath::Sin(Yw), FMath::Sin(E));
			const float Len = D.TailLen[k];
			const float MinZ = 42.f;   // it lies on the floor, never in it
			if (P.Z + Dir.Z * Len < MinZ)
			{
				const float Z = FMath::Clamp((MinZ - (float)P.Z) / Len, -1.f, 1.f);
				const float Flat = FMath::Sqrt(FMath::Max(0.f, 1.f - Z * Z));
				Dir = FVector(FMath::Cos(Yw) * Flat, FMath::Sin(Yw) * Flat, Z);
			}
			Set(Tail1 + k, P, AlongX(-Dir, FVector::UpVector));
			P += Dir * Len;
		}
		if (FMath::Abs(R.TailPuff - R.TailPuffSet) > 0.02f)
		{
			R.TailPuffSet = R.TailPuff;
			for (int32 k = 0; k < D.TailN; ++k)
			{
				if (Segs[Tail1 + k]) { Segs[Tail1 + k]->SetRelativeScale3D(FVector(QuadBody::Scale, QuadBody::Scale * R.TailPuff, QuadBody::Scale * R.TailPuff)); }
			}
		}
	}
}

} // namespace QuadBody
