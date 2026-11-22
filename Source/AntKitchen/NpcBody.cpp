#include "NpcBody.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "AntKitchenGameMode.h"
#include "AntRaid.h"
#include "AntMovementComponent.h"
#include "KitchenLoot.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Map4Look.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------------------------
// The rig: maths and assets
// ---------------------------------------------------------------------------------------------

namespace NpcRig
{

FQuat AlongX(const FVector& Dir, const FVector& ZHint)
{
	const FVector X = Dir.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	FVector Z = FVector::VectorPlaneProject(ZHint, X);
	if (Z.SizeSquared() < 1.e-3f)
	{
		Z = FVector::VectorPlaneProject(FMath::Abs(X.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector, X);
	}
	return FRotationMatrix::MakeFromXZ(X, Z).ToQuat();
}

FVector TwoBone(const FVector& A, FVector& T, float L1, float L2, const FVector& PoleHint, const FVector& Fallback, FVector& OutPole)
{
	FVector D = T - A;
	float Len = (float)D.Size();
	const FVector Dir = Len > KINDA_SMALL_NUMBER ? D / Len : FVector(0.f, 0.f, -1.f);
	const float Clamped = FMath::Clamp(Len, FMath::Abs(L1 - L2) + 2.f, L1 + L2 - 1.f);
	if (Clamped != Len)
	{
		T = A + Dir * Clamped;
		Len = Clamped;
	}
	FVector Pole = FVector::VectorPlaneProject(PoleHint, Dir);
	if (Pole.SizeSquared() < 1.e-4f)
	{
		Pole = FVector::VectorPlaneProject(Fallback, Dir);
	}
	Pole = Pole.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutPole = Pole;
	const float Along = (L1 * L1 - L2 * L2 + Len * Len) / (2.f * Len);
	const float Out = FMath::Sqrt(FMath::Max(0.f, L1 * L1 - Along * Along));
	return A + Dir * Along + Pole * Out;
}

UMaterialInstanceDynamic* Material(const FNpcLook& L)
{
	static TMap<const FNpcLook*, UMaterialInstanceDynamic*> Cache;
	if (UMaterialInstanceDynamic** Found = Cache.Find(&L))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* M = nullptr;
	// M_HumanSurface: the kitchen's M_Surface projected in the segment's own space (a world-space weave would swim over a moving leg).
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Human/M_HumanSurface.M_HumanSurface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Textures/M_Surface.M_Surface"));
	}
	if (Base)
	{
		M = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
		UTexture* T = L.Tex ? LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Textures/%s.%s"), L.Tex, L.Tex)) : nullptr;
		FLinearColor Tint(L.R, L.G, L.B, 1.f);
		float Size = L.Size;
		if (Map4Look::Enabled())
		{
			// map v4 material pass: the residents' cloth gets the real weaves of the new set (grey copies, mean 0.30 like
			// T_Fabric_Grey, so the colours stay as designed); fur and hair a soft boucle; skin and eyes stay clean paint
			const FString Slot(L.Slot);
			const TCHAR* Set = nullptr;
			bool bWasPlain = false;
			if (L.Tex && FCString::Strcmp(L.Tex, TEXT("T_Fabric_Grey")) == 0)
			{
				Set = (Slot.Contains(TEXT("Cardigan")) || Slot.Contains(TEXT("Slipper")) || Slot.Contains(TEXT("Sock"))) ? TEXT("KnitG")
					: (Slot.Contains(TEXT("Skirt")) || Slot.Contains(TEXT("Shorts")) || Slot.Contains(TEXT("Blouse"))) ? TEXT("LinenG") : TEXT("JerseyG");
			}
			else if (!L.Tex && (Slot.StartsWith(TEXT("Fur")) || Slot.StartsWith(TEXT("Fawn")) || Slot.Equals(TEXT("DogWhite")) || Slot.StartsWith(TEXT("Hair"))))
			{
				Set = TEXT("BoucleG"); bWasPlain = true; Size = 140.f;
			}
			else if (!L.Tex && (Slot.Equals(TEXT("Stocking")) || Slot.Equals(TEXT("Sock")) || Slot.Equals(TEXT("Fluff"))))
			{
				Set = TEXT("KnitG"); bWasPlain = true; Size = 160.f;
			}
			if (Set)
			{
				const FString N = FString::Printf(TEXT("/Game/Map4/Textures/T_M4_%s_C.T_M4_%s_C"), Set, Set);
				if (UTexture* T4 = LoadObject<UTexture>(nullptr, *N, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					T = T4;
					if (bWasPlain) { Tint = FLinearColor(L.R / 0.30f, L.G / 0.30f, L.B / 0.30f, 1.f); }
				}
			}
		}
		M->SetScalarParameterValue(TEXT("Mode"), T ? 0.f : 4.f);
		M->SetScalarParameterValue(TEXT("TileSize"), FMath::Max(1.f, Size > 0.f ? Size : 200.f));
		M->SetScalarParameterValue(TEXT("Roughness"), L.Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		M->SetVectorParameterValue(TEXT("Tint"), Tint);
		if (T)
		{
			M->SetTextureParameterValue(TEXT("Tex"), T);
		}
		M->AddToRoot();
	}
	return Cache.Add(&L, M);
}

void ApplyLook(UStaticMeshComponent* C, TConstArrayView<FNpcLook> Looks)
{
	UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
	if (!M)
	{
		return;
	}
	const TArray<FStaticMaterial>& Slots = M->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString Slot = Slots[i].MaterialSlotName.ToString();
		const FNpcLook* Look = nullptr;
		for (const FNpcLook& L : Looks)
		{
			if (Slot.Equals(L.Slot, ESearchCase::IgnoreCase)) { Look = &L; break; }
		}
		if (!Look)
		{
			UE_LOG(LogTemp, Warning, TEXT("NpcBody: no look for slot %s of %s"), *Slot, *M->GetName());
			continue;
		}
		if (UMaterialInstanceDynamic* Mat = Material(*Look))
		{
			C->SetMaterial(i, Mat);
		}
		if (i == 0 && Look->Step)
		{
			C->ComponentTags.AddUnique(FName(Look->Step));
		}
	}
}

UStaticMesh* LoadMesh(const TCHAR* Dir, const TCHAR* Name)
{
	static TMap<FString, UStaticMesh*> Cache;   // rooted below: static, survives map travel
	if (!Dir || !Name)
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("%s/%s/%s.%s"), Dir, Name, Name, Name);
	if (UStaticMesh** Found = Cache.Find(Path))
	{
		return *Found;
	}
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (M)
	{
		M->AddToRoot();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NpcBody: mesh %s is missing"), *Path);
	}
	return Cache.Add(Path, M);
}

float ServerNow(const AActor* A)
{
	const UWorld* W = A ? A->GetWorld() : nullptr;
	if (!W)
	{
		return 0.f;
	}
	const AGameStateBase* GS = W->GetGameState();
	return (A->HasAuthority() || !GS) ? W->GetTimeSeconds() : (float)GS->GetServerWorldTimeSeconds();
}

float ActTime(const AActor* A, const FNpcActState& Act)
{
	return ServerNow(A) - Act.Start;
}

} // namespace NpcRig

// ---------------------------------------------------------------------------------------------
// House rules and senses (server)
// ---------------------------------------------------------------------------------------------

namespace
{
	int32 GNpcForcedLevel = -2;   // -2: not read from the command line yet, -1: not forced

	/** The scent of ants on the floor: a coarse grid over the whole house. */
	struct FScentGrid
	{
		static constexpr float Cell = 250.f;
		static constexpr float X0 = -9500.f, Y0 = -10000.f;
		static constexpr int32 NX = 124, NY = 66;
		TWeakObjectPtr<const UWorld> World;
		TArray<float> V;
		float SampleClock = 0.f, DecayClock = 0.f;
		void For(const UWorld* W)
		{
			if (World.Get() != W || V.Num() != NX * NY)
			{
				World = W;
				V.Init(0.f, NX * NY);
				SampleClock = DecayClock = 0.f;
			}
		}
		static bool Index(const FVector& P, int32& X, int32& Y)
		{
			X = FMath::FloorToInt(((float)P.X - X0) / Cell);
			Y = FMath::FloorToInt(((float)P.Y - Y0) / Cell);
			return X >= 0 && Y >= 0 && X < NX && Y < NY;
		}
		static FVector Centre(int32 X, int32 Y) { return FVector(X0 + (X + 0.5f) * Cell, Y0 + (Y + 0.5f) * Cell, 0.f); }
	};
	FScentGrid GScent;
}

namespace NpcWorld
{

bool NewNpcs()
{
	static int32 State = -1;
	if (State < 0)
	{
		State = FParse::Param(FCommandLine::Get(), TEXT("AntOldNpc")) ? 0 : 1;
		UE_LOG(LogTemp, Display, TEXT("ANTNPC creatures: %s"), State ? TEXT("jointed bodies (/Game/Npc)") : TEXT("the old primitives (-AntOldNpc)"));
	}
	return State == 1;
}

void ForceLevel(int32 Level)
{
	GNpcForcedLevel = FMath::Clamp(Level, -1, 2);
	UE_LOG(LogTemp, Display, TEXT("ANTNPC level forced to %d"), GNpcForcedLevel);
}

int32 Level(const UWorld* World, uint8 Who)
{
	if (GNpcForcedLevel == -2)
	{
		int32 N = -1;
		GNpcForcedLevel = FParse::Value(FCommandLine::Get(), TEXT("AntNpcLevel="), N) ? FMath::Clamp(N, 0, 2) : -1;
	}
	if (GNpcForcedLevel >= 0)
	{
		return GNpcForcedLevel;
	}
	const AAntKitchenGameMode* GM = World ? World->GetAuthGameMode<AAntKitchenGameMode>() : nullptr;
	if (!GM || !GM->bHeist)
	{
		return 1;
	}
	if (GM->bRaid && GM->Raid)
	{
		return GM->Raid->NpcLevel(Who);   // a raid: by the threat tier and by how far the raid has got
	}
	// The heist run: one more thing to watch out for each shift, so that shift 1 can be won alone.
	const int32 S = GM->ShiftNumber;
	switch (Who)
	{
	case NpcWho::Dog:     return S >= 2 ? 2 : 1;
	case NpcWho::Cat:     return S >= 3 ? 2 : (S >= 2 ? 1 : 0);
	case NpcWho::Kid:     return S >= 4 ? 2 : (S >= 2 ? 1 : 0);
	case NpcWho::Grandma: return S >= 4 ? 2 : (S >= 3 ? 1 : 0);
	default:              return 1;
	}
}

void HurtAntsNear(UWorld* W, AActor* By, const FVector& C, float R, float Dmg, const TCHAR* Cause, int32 ArmyKills)
{
	if (!W)
	{
		return;
	}
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || FVector::Dist(A->GetActorLocation(), C) > R + AAntCharacter::BodyRadius)
		{
			continue;
		}
		A->LastHazard = Cause;
		A->LastHazardTime = W->GetTimeSeconds();
		UGameplayStatics::ApplyDamage(A, Dmg, nullptr, By, UDamageType::StaticClass());
	}
	if (ArmyKills > 0)
	{
		AAntArmy::KillInSphere(W, C, R, ArmyKills);
	}
}

bool UnderCover(const UWorld* W, const FVector& P, float Clearance)
{
	FHitResult Hit;
	return W && W->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 25.f), P + FVector(0.f, 0.f, Clearance), FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionQueryParams(SCENE_QUERY_STAT(NpcCover), false));
}

bool CanSee(const UWorld* W, const FVector& From, const FVector& To)
{
	FHitResult Hit;
	return W && !W->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(NpcSee), false));
}

float SurfaceZ(const UWorld* W, const FVector& P, float Up, float Down)
{
	FHitResult Hit;
	if (W && W->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, Up), P - FVector(0.f, 0.f, Down), FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionQueryParams(SCENE_QUERY_STAT(NpcFloor), false)))
	{
		return (float)Hit.ImpactPoint.Z;
	}
	return (float)P.Z;
}

FVector Steer(const UWorld* W, const AActor* Me, const FVector& From, const FVector& To, float Radius, float Height, float& Side, float& SideUntil)
{
	FVector Dir = To - From;
	Dir.Z = 0.f;
	const float Dist = (float)Dir.Size();
	if (!W || Dist < 1.f)
	{
		return FVector::ZeroVector;
	}
	Dir /= Dist;
	const float Now = W->GetTimeSeconds();
	const FVector Start = From + FVector(0.f, 0.f, Height);
	const float Probe = FMath::Min(Dist, Radius * 2.f + 160.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcSteer), false, Me);
	auto Free = [&](const FVector& D)
	{
		FHitResult Hit;
		return !W->SweepSingleByObjectType(Hit, Start, Start + D * Probe, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params);
	};
	if (Free(Dir))
	{
		if (Now >= SideUntil) { Side = 0.f; }
		return Dir;
	}
	// Something in the way: go round it, and keep going round the same side for a moment (no dithering in front of a chair leg).
	const float Pref = Side != 0.f && Now < SideUntil ? FMath::Sign(Side) : (FMath::RandBool() ? 1.f : -1.f);
	for (const float A : { 40.f, 75.f, 110.f })
	{
		for (const float S : { Pref, -Pref })
		{
			const FVector D = Dir.RotateAngleAxis(A * S, FVector::UpVector);
			if (Free(D))
			{
				Side = A * S;
				SideUntil = Now + 0.8f;
				return D;
			}
		}
	}
	return FVector::ZeroVector;
}

void ScentTick(UWorld* W, float Dt)
{
	if (!W)
	{
		return;
	}
	GScent.For(W);
	GScent.SampleClock += Dt;
	GScent.DecayClock += Dt;
	if (GScent.DecayClock >= 1.f)
	{
		// Fades with a half-life of about 30 s.
		const float K = FMath::Exp(-GScent.DecayClock / 43.f);
		GScent.DecayClock = 0.f;
		for (float& V : GScent.V) { V = V < 0.02f ? 0.f : V * K; }
	}
	if (GScent.SampleClock < 0.25f)
	{
		return;
	}
	GScent.SampleClock = 0.f;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		const AAntCharacter* A = *It;
		if (!A || A->bDead || A->IsInNest() || A->Creature != 0 || A->IsAttached())
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		int32 X, Y;
		if (P.Z > 260.f || !FScentGrid::Index(P, X, Y))
		{
			continue;   // only what touches the floor leaves a trail on it
		}
		// A bare ant is a faint trace; an ant dragging food is a banquet.
		const float Add = A->IsCarrying() ? 0.5f + 0.04f * FMath::Min(10, A->CarriedWeight()) : 0.13f;
		float& V = GScent.V[Y * FScentGrid::NX + X];
		V = FMath::Min(2.f, V + Add);
	}
}

float ScentAt(const UWorld* W, const FVector& P)
{
	int32 X, Y;
	if (GScent.World.Get() != W || !FScentGrid::Index(P, X, Y) || GScent.V.Num() == 0)
	{
		return 0.f;
	}
	return GScent.V[Y * FScentGrid::NX + X];
}

bool BestScent(const UWorld* W, const FVector& From, float Radius, float MinStrength, FVector& OutPoint, float& OutStrength)
{
	int32 CX, CY;
	if (GScent.World.Get() != W || GScent.V.Num() == 0 || !FScentGrid::Index(From, CX, CY))
	{
		return false;
	}
	const int32 N = FMath::CeilToInt(Radius / FScentGrid::Cell);
	float Best = 0.f;
	bool bFound = false;
	for (int32 Y = FMath::Max(0, CY - N); Y <= FMath::Min(FScentGrid::NY - 1, CY + N); ++Y)
	{
		for (int32 X = FMath::Max(0, CX - N); X <= FMath::Min(FScentGrid::NX - 1, CX + N); ++X)
		{
			const float V = GScent.V[Y * FScentGrid::NX + X];
			if (V < MinStrength)
			{
				continue;
			}
			const FVector C = FScentGrid::Centre(X, Y);
			const float D = (float)FVector::Dist2D(C, From);
			if (D > Radius)
			{
				continue;
			}
			const float Score = V * (1.f - 0.55f * D / Radius);
			if (Score > Best)
			{
				Best = Score;
				OutPoint = C;
				OutStrength = V;
				bFound = true;
			}
		}
	}
	return bFound;
}

void ScentAdd(const UWorld* W, const FVector& P, float Amount)
{
	int32 X, Y;
	if (!W || !FScentGrid::Index(P, X, Y))
	{
		return;
	}
	GScent.For(W);
	float& V = GScent.V[Y * FScentGrid::NX + X];
	V = FMath::Min(2.f, V + Amount);
}

void ScentClear(const UWorld* W, const FVector& P, float Radius)
{
	int32 CX, CY;
	if (GScent.World.Get() != W || GScent.V.Num() == 0 || !FScentGrid::Index(P, CX, CY))
	{
		return;
	}
	const int32 N = FMath::CeilToInt(Radius / FScentGrid::Cell);
	for (int32 Y = FMath::Max(0, CY - N); Y <= FMath::Min(FScentGrid::NY - 1, CY + N); ++Y)
	{
		for (int32 X = FMath::Max(0, CX - N); X <= FMath::Min(FScentGrid::NX - 1, CX + N); ++X)
		{
			if (FVector::Dist2D(FScentGrid::Centre(X, Y), P) <= Radius + FScentGrid::Cell * 0.5f)
			{
				GScent.V[Y * FScentGrid::NX + X] = 0.f;
			}
		}
	}
}

} // namespace NpcWorld
