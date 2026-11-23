#include "AntCosmetics.h"
#include "AntFighter.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntBody.h"
#include "AntTypes.h"
#include "AntFx.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"

// Every part is an engine shape or a small procedural mesh hung on a piece of the jointed ant (AntBody.h): the head, the thorax,
// the gaster. Sizes come from the piece's own bounds, so the kit fits every caste. Art to replace with modelled pieces later:
// docs/FIGHTER.md, "Арт на замену".

namespace
{
	const FName KitTag(TEXT("AntKit"));
	const FName EmoteTag(TEXT("AntEmote"));

	// (lighter than the trailer's: a dark cap on a black ant did not read from the player's camera)
	const FLinearColor Acorn(0.34f, 0.19f, 0.07f);
	const FLinearColor AcornRim(0.2f, 0.11f, 0.04f);
	const FLinearColor Wood(0.78f, 0.56f, 0.30f);
	const FLinearColor MatchHead(0.42f, 0.05f, 0.03f);
	const FLinearColor Sugar(0.95f, 0.94f, 0.90f);
	const FLinearColor CapRed(0.62f, 0.03f, 0.03f);
	const FLinearColor Husk(0.035f, 0.035f, 0.04f);
	const FLinearColor HuskStripe(0.55f, 0.55f, 0.52f);
	const FLinearColor Bead(0.05f, 0.45f, 0.75f);
	const FLinearColor ThreadRed(0.70f, 0.06f, 0.05f);
	const FLinearColor Steel(0.55f, 0.56f, 0.58f);
	const FLinearColor MarkerBlue(0.04f, 0.16f, 0.62f);
	const FLinearColor Gold(0.95f, 0.66f, 0.12f);

	UMaterialInstanceDynamic* PaintAny(UMeshComponent* C, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base || !C) { return nullptr; }
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
		return MID;
	}

	UStaticMeshComponent* Shape(AAntCharacter* Ant, USceneComponent* Parent, EAntShape S, const FVector& Loc, const FVector& Size, const FLinearColor& Col, const FRotator& Rot = FRotator::ZeroRotator, FName Tag = KitTag)
	{
		UStaticMeshComponent* C = AntKit::AddPart(Ant, Parent, S, Loc, Size / 100.f, Col, Rot, false, false);
		if (C)
		{
			C->ComponentTags.Add(Tag);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
		}
		return C;
	}

	UProceduralMeshComponent* NewProc(AAntCharacter* Ant, USceneComponent* Parent, const FVector& Loc, const FRotator& Rot, const FLinearColor& Col)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Ant);
		P->SetupAttachment(Parent);
		P->SetRelativeLocationAndRotation(Loc, Rot);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->bUseAsyncCooking = false;
		P->ComponentTags.Add(KitTag);
		P->RegisterComponent();
		PaintAny(P, Col);
		return P;
	}

	/** A dome (an acorn cap): radius R, height H, the cupules as small bumps; open at the bottom, with an inner face. */
	void BuildDome(UProceduralMeshComponent* P, float R, float H, float Bumps)
	{
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<FProcMeshTangent> Tan;
		const int32 NA = 40, NB = 14;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const int32 Base = V.Num();
			const float Shrink = Side == 0 ? 1.f : 0.9f;
			for (int32 b = 0; b <= NB; ++b)
			{
				const float Phi = HALF_PI * b / NB;
				for (int32 a = 0; a <= NA; ++a)
				{
					const float Th = TWO_PI * a / NA;
					const float Bump = 1.f + Bumps * FMath::Sin(Th * 14.f + b * 1.7f) * FMath::Sin(Phi * 9.f) * (Side == 0 ? 1.f : 0.f);
					const FVector Dir(FMath::Cos(Phi) * FMath::Cos(Th), FMath::Cos(Phi) * FMath::Sin(Th), FMath::Sin(Phi));
					V.Add(FVector(Dir.X * R * Bump * Shrink, Dir.Y * R * Bump * Shrink, Dir.Z * H * Bump * Shrink));
					FVector Nrm(Dir.X / R, Dir.Y / R, Dir.Z / H);
					Nrm = Nrm.GetSafeNormal();
					N.Add(Side == 0 ? Nrm : -Nrm);
					UV.Add(FVector2D((float)a / NA, (float)b / NB));
				}
			}
			for (int32 b = 0; b < NB; ++b)
			{
				for (int32 a = 0; a < NA; ++a)
				{
					const int32 i0 = Base + b * (NA + 1) + a, i1 = i0 + 1, i2 = i0 + NA + 1, i3 = i2 + 1;
					if (Side == 0) { T.Append({ i0, i2, i1, i1, i2, i3 }); }
					else { T.Append({ i0, i1, i2, i1, i3, i2 }); }
				}
			}
		}
		P->CreateMeshSection_LinearColor(0, V, T, N, UV, Col, Tan, false);
	}

	/** A torus around the local Z axis: big radius R, tube radius r. */
	void BuildTorus(UProceduralMeshComponent* P, float R, float r)
	{
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<FProcMeshTangent> Tan;
		const int32 NA = 32, NB = 12;
		for (int32 a = 0; a <= NA; ++a)
		{
			const float Th = TWO_PI * a / NA;
			const FVector C(FMath::Cos(Th) * R, FMath::Sin(Th) * R, 0.f);
			const FVector Out(FMath::Cos(Th), FMath::Sin(Th), 0.f);
			for (int32 b = 0; b <= NB; ++b)
			{
				const float Ph = TWO_PI * b / NB;
				const FVector Nrm = Out * FMath::Cos(Ph) + FVector::UpVector * FMath::Sin(Ph);
				V.Add(C + Nrm * r);
				N.Add(Nrm);
				UV.Add(FVector2D((float)a / NA, (float)b / NB));
			}
		}
		for (int32 a = 0; a < NA; ++a)
		{
			for (int32 b = 0; b < NB; ++b)
			{
				const int32 i0 = a * (NB + 1) + b, i1 = i0 + 1, i2 = i0 + NB + 1, i3 = i2 + 1;
				T.Append({ i0, i1, i2, i1, i3, i2 });
			}
		}
		P->CreateMeshSection_LinearColor(0, V, T, N, UV, Col, Tan, false);
	}

	FBox LocalBox(UStaticMeshComponent* C)
	{
		return (C && C->GetStaticMesh()) ? C->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-5.f), FVector(5.f));
	}

	int32 Helmet(AAntCharacter* Ant, UStaticMeshComponent* Head, float Scale, float Pitch, float Fwd, bool bStem = true)
	{
		const FBox B = LocalBox(Head);
		const FVector C = B.GetCenter(), E = B.GetExtent();
		const float R = FMath::Max(E.Y * 1.08f, E.X * 0.62f) * Scale;
		const FVector At(C.X - E.X * 0.05f + Fwd * E.X, C.Y, C.Z + E.Z * 0.42f - (Scale - 1.f) * E.Z * 0.35f);
		const FRotator Rot(Pitch, 0.f, 0.f);
		UProceduralMeshComponent* Dome = NewProc(Ant, Head, At, Rot, Acorn);
		BuildDome(Dome, R, R * 0.78f, 0.035f);
		UProceduralMeshComponent* Rim = NewProc(Ant, Head, At, Rot, AcornRim);
		BuildTorus(Rim, R * 0.97f, R * 0.1f);
		int32 N = 2;
		if (bStem)
		{
			const FVector Up = Rot.RotateVector(FVector(0.f, 0.f, R * 0.78f));
			Shape(Ant, Head, EAntShape::Cylinder, At + Up * 1.04f, FVector(R * 0.16f, R * 0.16f, R * 0.32f), AcornRim, Rot + FRotator(-18.f, 0.f, 0.f));
			++N;
		}
		return N;
	}

	/** A match held at the corner of the mandibles, pointing ahead: the spear. */
	int32 Match(AAntCharacter* Ant, UStaticMeshComponent* Head, float LenK, const FLinearColor& Shaft, const FLinearColor& Tip, bool bCone)
	{
		const FBox HB = LocalBox(Head);
		const FVector HC = HB.GetCenter(), HE = HB.GetExtent();
		const float L = HE.X * 1.9f * LenK, D = HE.Y * 0.17f;
		const FRotator MR(6.f, 30.f, 0.f);
		const FVector Root(HC.X + HE.X * 0.85f, HC.Y + HE.Y * 0.25f, HC.Z - HE.Z * 0.62f);
		const FVector Mid = Root + MR.RotateVector(FVector(L * 0.45f, 0.f, 0.f));
		const FQuat Q = MR.Quaternion() * FRotator(-90.f, 0.f, 0.f).Quaternion();
		Shape(Ant, Head, EAntShape::Cylinder, Mid, FVector(D, D, L), Shaft, Q.Rotator());
		if (bCone) { Shape(Ant, Head, EAntShape::Cone, Root + MR.RotateVector(FVector(L * 0.98f, 0.f, 0.f)), FVector(D * 1.2f, D * 1.2f, D * 3.f), Tip, Q.Rotator()); }
		else { Shape(Ant, Head, EAntShape::Sphere, Root + MR.RotateVector(FVector(L * 0.95f, 0.f, 0.f)), FVector(D * 2.6f, D * 2.2f, D * 2.2f), Tip, MR); }
		return 2;
	}

	/** Rank insignia on the thorax: sugar grains, match bars, a bottle cap (never anything of a real army). */
	int32 Insignia(AAntCharacter* Ant, UStaticMeshComponent* Thorax, int32 Rank)
	{
		if (Rank <= 0) { return 0; }
		const FBox TB = LocalBox(Thorax);
		const FVector TC = TB.GetCenter(), TE = TB.GetExtent();
		const float Top = TC.Z + TE.Z * 0.95f;
		int32 N = 0;
		const int32 Grains = Rank == 1 ? 1 : Rank == 2 ? 2 : Rank == 4 ? 1 : Rank == 5 ? 1 : Rank == 7 ? 3 : 0;
		const int32 Bars = Rank == 3 ? 1 : Rank == 4 ? 2 : Rank == 6 ? 2 : 0;
		const bool bCap = Rank >= 5;
		if (bCap)
		{
			Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X - TE.X * 0.1f, TC.Y, Top + TE.Z * 0.05f), FVector(TE.Y * 1.1f, TE.Y * 1.1f, TE.Z * 0.12f), Rank >= 7 ? Gold : CapRed);
			++N;
		}
		for (int32 b = 0; b < Bars; ++b)
		{
			Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X + TE.X * (0.25f - 0.3f * b), TC.Y, Top + TE.Z * (bCap ? 0.2f : 0.08f)), FVector(TE.Y * 0.12f, TE.Y * 0.12f, TE.Y * 1.5f), Wood, FRotator(0.f, 0.f, 90.f));
			++N;
		}
		for (int32 g = 0; g < Grains; ++g)
		{
			const float t = Grains == 1 ? 0.f : (g / (float)(Grains - 1) - 0.5f);
			Shape(Ant, Thorax, EAntShape::Cube, FVector(TC.X - TE.X * 0.15f, TC.Y + t * TE.Y * 0.9f, Top + TE.Z * (bCap ? 0.28f : 0.12f)), FVector(TE.Y * 0.3f), Sugar, FRotator(0.f, 35.f + 20.f * g, 0.f));
			++N;
		}
		return N;
	}

	FLinearColor ShellTintOf(uint32 Hex)
	{
		if (Hex == 0) { return FLinearColor::White; }
		const FLinearColor C = AntCosmetics::Col(Hex);
		const float M = FMath::Max3(C.R, C.G, C.B);
		if (M < 0.01f) { return FLinearColor(0.35f, 0.35f, 0.38f); }   // coal: darker all over
		return FMath::Lerp(FLinearColor::White, C / M, 0.75f) * 1.35f;
	}
}

namespace AntCosmetics
{
	FLinearColor Col(uint32 Hex)
	{
		return FLinearColor(FColor((Hex >> 16) & 0xFF, (Hex >> 8) & 0xFF, Hex & 0xFF));
	}

	void Clear(AAntCharacter* Ant)
	{
		if (!Ant) { return; }
		TArray<UActorComponent*> Old = Ant->GetComponentsByTag(UActorComponent::StaticClass(), KitTag);
		for (UActorComponent* C : Old) { C->DestroyComponent(); }
	}

	void Attach(AAntCharacter* Ant)
	{
		if (!Ant || Ant->Creature != 0 || Ant->FindComponentByClass<UAntDress>()) { return; }
		UAntDress* D = NewObject<UAntDress>(Ant, TEXT("AntDress"));
		D->RegisterComponent();
		D->AddTickPrerequisiteActor(Ant);
	}

	int32 Dress(AAntCharacter* Ant, const FAntLoadout* L)
	{
		if (!Ant) { return 0; }
		Clear(Ant);
		const uint8 Rank = L ? L->Rank : Ant->VetRank;
		const bool bInsignia = L ? L->bInsignia : true;
		// the shell colour and the trail colour work without the jointed body too
		Ant->ShellTint = L ? ShellTintOf(AntCosm::Item(AntCosm::Shell, L->CosmAt(AntCosm::Shell)).Color) : FLinearColor::White;
		Ant->RefreshBodyColor();
		if (L && L->CosmAt(AntCosm::Trail) > 0) { Ant->SetTrailTint(Col(AntCosm::Item(AntCosm::Trail, L->CosmAt(AntCosm::Trail)).Color)); }
		else { Ant->SetTrailTint(FLinearColor(0.3f, 1.f, 0.35f)); }
		if (!Ant->Rig) { return 0; }
		UStaticMeshComponent* Thorax = Ant->Rig->GetPiece(0);
		UStaticMeshComponent* Head = Ant->Rig->GetPiece(1);
		UStaticMeshComponent* Gaster = Ant->Rig->GetPiece(4);
		if (!Head || !Thorax) { return 0; }
		const FBox HB = LocalBox(Head), TB = LocalBox(Thorax);
		const FVector HC = HB.GetCenter(), HE = HB.GetExtent(), TC = TB.GetCenter(), TE = TB.GetExtent();
		int32 N = 0;
		if (bInsignia) { N += Insignia(Ant, Thorax, Rank); }
		if (!L)
		{
			if (!Ant->VetName.IsEmpty()) { N += Helmet(Ant, Head, 1.f, 4.f, 0.f); }   // a squad veteran has earned his acorn helmet
			return N;
		}
		auto Key = [L](uint8 Slot) { return FString(AntCosm::Item(Slot, L->CosmAt(Slot)).Key); };
		auto ItemCol = [L](uint8 Slot) { return Col(AntCosm::Item(Slot, L->CosmAt(Slot)).Color); };

		// ---- head
		const FString H = Key(AntCosm::Head);
		if (H == TEXT("acorn")) { N += Helmet(Ant, Head, 1.f, 4.f, 0.f); }
		else if (H == TEXT("halfacorn")) { N += Helmet(Ant, Head, 0.8f, 8.f, -0.18f, false); }
		else if (H == TEXT("thimble"))
		{
			const float R = HE.Y * 1.05f;
			const FVector At(HC.X - HE.X * 0.05f, HC.Y, HC.Z + HE.Z * 0.85f);
			Shape(Ant, Head, EAntShape::Cylinder, At, FVector(R * 2.f, R * 2.f, HE.Z * 1.1f), Steel, FRotator(6.f, 0.f, 0.f));
			Shape(Ant, Head, EAntShape::Sphere, At + FVector(0.f, 0.f, HE.Z * 0.5f), FVector(R * 2.f, R * 2.f, HE.Z * 0.5f), Steel * 0.9f, FRotator(6.f, 0.f, 0.f));
			UProceduralMeshComponent* Rim = NewProc(Ant, Head, At - FVector(0.f, 0.f, HE.Z * 0.52f), FRotator(6.f, 0.f, 0.f), Steel * 0.75f);
			BuildTorus(Rim, R * 1.02f, R * 0.08f);
			N += 3;
		}
		else if (H == TEXT("bottlecap"))
		{
			const float R = HE.Y * 1.25f;
			const FVector At(HC.X - HE.X * 0.05f, HC.Y, HC.Z + HE.Z * 0.62f);
			Shape(Ant, Head, EAntShape::Cylinder, At, FVector(R * 2.f, R * 2.f, HE.Z * 0.5f), CapRed, FRotator(6.f, 0.f, 0.f));
			Shape(Ant, Head, EAntShape::Cylinder, At + FVector(0.f, 0.f, HE.Z * 0.26f), FVector(R * 1.7f, R * 1.7f, HE.Z * 0.04f), Sugar, FRotator(6.f, 0.f, 0.f));
			N += 2;
		}
		else if (H == TEXT("marker"))
		{
			const float R = HE.Y * 0.95f;
			Shape(Ant, Head, EAntShape::Cylinder, FVector(HC.X - HE.X * 0.05f, HC.Y, HC.Z + HE.Z * 1.0f), FVector(R * 2.f, R * 2.f, HE.Z * 1.2f), MarkerBlue, FRotator(8.f, 0.f, 0.f));
			Shape(Ant, Head, EAntShape::Cylinder, FVector(HC.X - HE.X * 0.05f, HC.Y, HC.Z + HE.Z * 1.62f), FVector(R * 1.3f, R * 1.3f, HE.Z * 0.2f), MarkerBlue * 0.6f, FRotator(8.f, 0.f, 0.f));
			N += 2;
		}
		// ---- face
		const FString F = Key(AntCosm::Face);
		if (F == TEXT("beads") || F == TEXT("dew"))
		{
			const float K = F == TEXT("dew") ? 0.8f : 0.62f;
			for (int32 s = -1; s <= 1; s += 2)
			{
				UStaticMeshComponent* E = Shape(Ant, Head, EAntShape::Sphere, FVector(HC.X + HE.X * 0.38f, HC.Y + s * HE.Y * 0.52f, HC.Z + HE.Z * 0.42f), FVector(HE.Y * K), ItemCol(AntCosm::Face));
				if (E && F == TEXT("dew")) { AntFx::MakeGlow(E, ItemCol(AntCosm::Face) * 0.6f); }
				++N;
			}
			Shape(Ant, Head, EAntShape::Cylinder, FVector(HC.X + HE.X * 0.38f, HC.Y, HC.Z + HE.Z * 0.42f), FVector(HE.Y * 0.12f, HE.Y * 0.12f, HE.Y * 1.5f), AcornRim, FRotator(0.f, 0.f, 90.f));
			++N;
		}
		else if (F == TEXT("monocle"))
		{
			UProceduralMeshComponent* M = NewProc(Ant, Head, FVector(HC.X + HE.X * 0.42f, HC.Y + HE.Y * 0.52f, HC.Z + HE.Z * 0.42f), FRotator(90.f, 0.f, 0.f), Gold);
			BuildTorus(M, HE.Y * 0.34f, HE.Y * 0.06f);
			Shape(Ant, Head, EAntShape::Cylinder, FVector(HC.X + HE.X * 0.42f, HC.Y + HE.Y * 0.52f, HC.Z + HE.Z * 0.42f), FVector(HE.Y * 0.62f, HE.Y * 0.62f, HE.Y * 0.03f), ItemCol(AntCosm::Face), FRotator(90.f, 0.f, 0.f));
			N += 2;
		}
		// ---- neck
		const FString K = Key(AntCosm::Neck);
		if (K == TEXT("scarf") || K == TEXT("yarn"))
		{
			const FLinearColor C = ItemCol(AntCosm::Neck);
			UProceduralMeshComponent* Ring = NewProc(Ant, Thorax, FVector(TC.X + TE.X * 0.75f, TC.Y, TC.Z + TE.Z * 0.15f), FRotator(90.f, 0.f, 0.f), C);
			BuildTorus(Ring, TE.Y * 0.95f, TE.Y * (K == TEXT("yarn") ? 0.3f : 0.22f));
			if (K == TEXT("scarf"))
			{
				Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X - TE.X * 0.2f, TC.Y + TE.Y * 0.5f, TC.Z + TE.Z * 1.05f), FVector(TE.Y * 0.35f, TE.Y * 0.12f, TE.X * 2.2f), C, FRotator(-80.f, 10.f, 0.f));
				Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X - TE.X * 0.5f, TC.Y + TE.Y * 0.9f, TC.Z + TE.Z * 1.2f), FVector(TE.Y * 0.32f, TE.Y * 0.1f, TE.X * 1.6f), C, FRotator(-70.f, 25.f, 0.f));
			}
			else
			{
				for (int32 s = -1; s <= 1; s += 2) { Shape(Ant, Thorax, EAntShape::Sphere, FVector(TC.X + TE.X * 0.8f, TC.Y + s * TE.Y * 0.35f, TC.Z + TE.Z * 1.0f), FVector(TE.Y * 0.55f, TE.Y * 0.4f, TE.Y * 0.3f), C, FRotator(0.f, s * 30.f, 0.f)); }
			}
			N += 3;
		}
		else if (K == TEXT("washer"))
		{
			UProceduralMeshComponent* W = NewProc(Ant, Thorax, FVector(TC.X + TE.X * 0.85f, TC.Y, TC.Z + TE.Z * 0.2f), FRotator(90.f, 0.f, 0.f), Steel);
			BuildTorus(W, TE.Y * 1.05f, TE.Y * 0.32f);
			++N;
		}
		// ---- weapon
		const FString Wp = Key(AntCosm::Weapon);
		if (Wp == TEXT("match")) { N += Match(Ant, Head, 1.f, Wood, MatchHead, false); }
		else if (Wp == TEXT("toothpick")) { N += Match(Ant, Head, 1.5f, ItemCol(AntCosm::Weapon), ItemCol(AntCosm::Weapon), true); }
		else if (Wp == TEXT("burnt")) { N += Match(Ant, Head, 0.9f, ItemCol(AntCosm::Weapon), FLinearColor(0.02f, 0.02f, 0.02f), false); }
		// ---- back
		const FString Bk = Key(AntCosm::Back);
		if (Bk == TEXT("husk"))
		{
			const FVector Bp(TC.X - TE.X * 0.35f, TC.Y, TC.Z + TE.Z * 1.25f);
			const FRotator BR(14.f, 0.f, 0.f);
			Shape(Ant, Thorax, EAntShape::Sphere, Bp, FVector(TE.X * 2.6f, TE.Y * 1.7f, TE.Z * 1.15f), Husk, BR);
			for (int32 i = -1; i <= 1; ++i)
			{
				Shape(Ant, Thorax, EAntShape::Cube, Bp + BR.RotateVector(FVector(0.f, i * TE.Y * 0.42f, TE.Z * 0.52f - FMath::Abs(i) * TE.Z * 0.12f)), FVector(TE.X * 2.2f, TE.Y * 0.12f, TE.Z * 0.1f), HuskStripe, BR + FRotator(0.f, 0.f, i * 22.f));
			}
			N += 4;
		}
		else if (Bk == TEXT("sugarbag"))
		{
			Shape(Ant, Thorax, EAntShape::Cube, FVector(TC.X - TE.X * 0.3f, TC.Y, TC.Z + TE.Z * 1.3f), FVector(TE.X * 1.4f, TE.Y * 1.3f, TE.Z * 0.9f), Sugar, FRotator(10.f, 8.f, 0.f));
			Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X + TE.X * 0.1f, TC.Y, TC.Z + TE.Z * 0.6f), FVector(TE.Y * 0.1f, TE.Y * 0.1f, TE.Z * 2.4f), ThreadRed, FRotator(0.f, 0.f, 60.f));
			N += 2;
		}
		else if (Bk == TEXT("leaf"))
		{
			Shape(Ant, Thorax, EAntShape::Sphere, FVector(TC.X - TE.X * 0.9f, TC.Y, TC.Z + TE.Z * 0.9f), FVector(TE.X * 3.2f, TE.Y * 2.4f, TE.Z * 0.15f), ItemCol(AntCosm::Back), FRotator(-12.f, 0.f, 0.f));
			Shape(Ant, Thorax, EAntShape::Cylinder, FVector(TC.X - TE.X * 0.9f, TC.Y, TC.Z + TE.Z * 1.0f), FVector(TE.Y * 0.06f, TE.Y * 0.06f, TE.X * 3.f), ItemCol(AntCosm::Back) * 0.6f, FRotator(-102.f, 0.f, 0.f));
			N += 2;
		}
		// ---- war paint (two stripes on each cheek)
		const FString P = Key(AntCosm::Paint);
		if (P != TEXT("none"))
		{
			for (int32 s = -1; s <= 1; s += 2)
			{
				for (int32 k = 0; k < 2; ++k)
				{
					Shape(Ant, Head, EAntShape::Cube, FVector(HC.X + HE.X * (0.15f - 0.25f * k), HC.Y + s * HE.Y * 0.98f, HC.Z + HE.Z * 0.05f), FVector(HE.X * 0.42f, HE.Y * 0.06f, HE.Z * 0.12f), ItemCol(AntCosm::Paint), FRotator(0.f, 0.f, s * 8.f));
					++N;
				}
			}
		}
		// ---- pattern on the gaster
		const FString Pt = Key(AntCosm::Pattern);
		if (Gaster && Pt != TEXT("none"))
		{
			const FBox GB = LocalBox(Gaster);
			const FVector GC = GB.GetCenter(), GE = GB.GetExtent();
			if (Pt == TEXT("stripes"))
			{
				for (int32 i = 0; i < 3; ++i)
				{
					UProceduralMeshComponent* R = NewProc(Ant, Gaster, FVector(GC.X + GE.X * (-0.45f + 0.4f * i), GC.Y, GC.Z), FRotator(90.f, 0.f, 0.f), ItemCol(AntCosm::Pattern));
					BuildTorus(R, FMath::Min(GE.Y, GE.Z) * (1.0f - 0.12f * FMath::Abs(i - 1)), GE.Y * 0.05f);
					++N;
				}
			}
			else
			{
				FRandomStream Rng(77);
				for (int32 i = 0; i < 9; ++i)
				{
					const FVector Dir = FVector(Rng.FRandRange(-0.8f, 0.8f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(0.2f, 1.f)).GetSafeNormal();
					Shape(Ant, Gaster, EAntShape::Sphere, GC + FVector(Dir.X * GE.X, Dir.Y * GE.Y, Dir.Z * GE.Z) * 0.97f, FVector(GE.Y * 0.16f), ItemCol(AntCosm::Pattern));
					++N;
				}
			}
		}
		return N;
	}

	int32 Apply(AAntCharacter* Ant, const FString& RoleIn)
	{
		// The trailer's squad as loadouts (docs/CAPTURE.md "cap kit").
		FAntLoadout L;
		L.Cosm.SetNumZeroed(AntCosm::SlotCount);
		L.bInsignia = false;
		const FString Role = RoleIn.ToLower();
		auto Set = [&L](uint8 Slot, const TCHAR* Key)
		{
			for (int32 i = 0; i < AntCosm::NumItems(Slot); ++i) { if (FString(AntCosm::Item(Slot, i).Key) == Key) { L.Cosm[Slot] = (uint8)i; } }
		};
		if (Role == TEXT("helmet") || Role == TEXT("medic") || Role == TEXT("recruit")) { Set(AntCosm::Head, TEXT("acorn")); }
		else if (Role == TEXT("sgt")) { Set(AntCosm::Head, TEXT("acorn")); Set(AntCosm::Weapon, TEXT("match")); L.Rank = 3; L.bInsignia = true; }
		else if (Role == TEXT("hauler")) { Set(AntCosm::Head, TEXT("bottlecap")); Set(AntCosm::Back, TEXT("husk")); }
		else if (Role == TEXT("scout")) { Set(AntCosm::Head, TEXT("halfacorn")); Set(AntCosm::Face, TEXT("beads")); Set(AntCosm::Neck, TEXT("scarf")); }
		else if (Role == TEXT("sapper")) { Set(AntCosm::Head, TEXT("marker")); Set(AntCosm::Neck, TEXT("washer")); }
		const int32 N = Role == TEXT("none") ? (Clear(Ant), 0) : Dress(Ant, &L);
		UE_LOG(LogTemp, Display, TEXT("ANTKIT %s on %s: %d parts"), *Role, Ant ? *Ant->GetName() : TEXT("-"), N);
		return N;
	}
}

// =============================================================================================
// The dress component
// =============================================================================================

UAntDress::UAntDress()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UAntDress::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AAntCharacter* Ant = Cast<AAntCharacter>(GetOwner());
	if (!Ant)
	{
		return;
	}
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (Now >= NextCheck)
	{
		NextCheck = Now + 0.25f;
		const FAntLoadout* L = AntFighter::LoadoutOf(Ant);
		uint32 S = HashCombine(GetTypeHash(Ant->Rig != nullptr), GetTypeHash((int32)Ant->VetRank));
		S = HashCombine(S, GetTypeHash(Ant->VetName));
		S = HashCombine(S, GetTypeHash((int32)Ant->bDead));
		S = HashCombine(S, GetTypeHash((int32)Ant->AntClass));
		if (L)
		{
			S = HashCombine(S, GetTypeHash((int32)L->Rank));
			S = HashCombine(S, GetTypeHash((int32)L->bInsignia));
			for (uint8 C : L->Cosm) { S = HashCombine(S, GetTypeHash((int32)C)); }
		}
		if (S != Sig)
		{
			Sig = S;
			if (Ant->bDead) { AntCosmetics::Clear(Ant); Parts = 0; }
			else
			{
				Parts = AntCosmetics::Dress(Ant, L);
				if (Parts > 0)
				{
					UE_LOG(LogTemp, Display, TEXT("ANTKIT dressed %s on %s: %d parts (rank %d, cosm %s)"), *Ant->GetDisplayName(), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), Parts,
						L ? (int32)L->Rank : (int32)Ant->VetRank, L ? *FString::JoinBy(L->Cosm, TEXT(","), [](uint8 C) { return FString::FromInt(C); }) : TEXT("-"));
				}
			}
		}
	}
	// ---- emotes: the server plays the steps (body acts and hops replicate); every machine shows the crumbs
	const float SNow = W->GetGameState() ? (float)W->GetGameState()->GetServerWorldTimeSeconds() : Now;
	const float T = SNow - Ant->EmoteAt;
	const bool bOn = Ant->Emote != 0 && T >= 0.f && T < 2.4f && !Ant->bDead;
	if (!bOn)
	{
		if (EmoteBits.Num() > 0) { for (const TWeakObjectPtr<UStaticMeshComponent>& B : EmoteBits) { if (B.IsValid()) { B->DestroyComponent(); } } EmoteBits.Reset(); }
		EmoteStep = -1;
		if (Ant->HasAuthority() && Ant->Emote != 0 && T >= 2.4f) { Ant->Emote = 0; }
		return;
	}
	const int32 Step = FMath::FloorToInt(T / 0.4f);
	const FString Key = AntCosm::Item(AntCosm::Emote, Ant->Emote).Key;
	if (Step != EmoteStep)
	{
		EmoteStep = Step;
		if (Ant->HasAuthority())
		{
			const FVector Fwd = Ant->GetActorForwardVector(), Right = Ant->GetActorRightVector();
			if (Key == TEXT("stomp")) { if (Step % 2 == 0) { Ant->ApplyLaunch(Ant->GetVisualUp() * 230.f * AntKit::SpeedK); } else { Ant->NoteBodyAct(AntAct::Bite, Fwd, 0.6f); } }
			else if (Key == TEXT("spin")) { Ant->NoteBodyAct(AntAct::Knock, FRotator(0.f, 90.f * Step, 0.f).RotateVector(Fwd), 0.45f); }
			else if (Key == TEXT("bow")) { if (Step == 0 || Step == 3) { Ant->NoteBodyAct(AntAct::Flinch, -Fwd, 0.95f); } }
			else if (Key == TEXT("dance")) { Ant->NoteBodyAct(AntAct::Knock, (Step % 2 ? Right : -Right), 0.55f); }
		}
		if (EmoteBits.Num() == 0 && Ant->Rig && (Key == TEXT("spin") || Key == TEXT("dance")))
		{
			if (UStaticMeshComponent* Head = Ant->Rig->GetPiece(1))
			{
				const FBox HB = LocalBox(Head);
				for (int32 i = 0; i < 4; ++i)
				{
					UStaticMeshComponent* C = Shape(Ant, Head, EAntShape::Cube, HB.GetCenter(), FVector(HB.GetExtent().Y * 0.35f), Key == TEXT("dance") ? Wood : Sugar, FRotator::ZeroRotator, EmoteTag);
					if (C) { EmoteBits.Add(C); }
				}
			}
		}
	}
	// the crumbs circle over the head
	for (int32 i = 0; i < EmoteBits.Num(); ++i)
	{
		UStaticMeshComponent* C = EmoteBits[i].Get();
		if (!C || !Ant->Rig || !Ant->Rig->GetPiece(1)) { continue; }
		const FBox HB = LocalBox(Ant->Rig->GetPiece(1));
		const float A = T * 7.f + i * HALF_PI;
		const float R = HB.GetExtent().Y * 1.8f;
		C->SetRelativeLocation(HB.GetCenter() + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, HB.GetExtent().Z * (2.f + 0.3f * FMath::Sin(A * 2.f))));
		C->SetRelativeRotation(FRotator(A * 40.f, A * 60.f, 0.f));
	}
}
