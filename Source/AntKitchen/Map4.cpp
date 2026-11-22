#include "Map4.h"
#include "AntKitchenGameMode.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "HumanCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Engine/OverlapResult.h"

namespace
{
	struct FDistrict { const TCHAR* Id; TArray<FBox2D> Rects; };
	const TArray<FDistrict>& Districts()
	{
		static const TArray<FDistrict> D =
		{
			{ TEXT("palisad"), { FBox2D(FVector2D(-15300.f, -5200.f), FVector2D(-7200.f, 5600.f)) } },
			{ TEXT("reddvor"), { FBox2D(FVector2D(-9400.f, -8800.f), FVector2D(7000.f, -5200.f)), FBox2D(FVector2D(5200.f, -12200.f), FVector2D(20400.f, -9200.f)), FBox2D(FVector2D(5200.f, -9200.f), FVector2D(7000.f, -8800.f)) } },
			{ TEXT("plintus"), { FBox2D(FVector2D(-7000.f, -5000.f), FVector2D(-2700.f, 2200.f)), FBox2D(FVector2D(-2700.f, -5000.f), FVector2D(5000.f, -3400.f)) } },
			{ TEXT("kladovye"), { FBox2D(FVector2D(-7000.f, 2200.f), FVector2D(-4200.f, 5000.f)) } },
			{ TEXT("moika"), { FBox2D(FVector2D(-4200.f, 2200.f), FVector2D(-1500.f, 5000.f)) } },
			{ TEXT("plita"), { FBox2D(FVector2D(-1500.f, 2200.f), FVector2D(0.f, 5000.f)) } },
			{ TEXT("lednik"), { FBox2D(FVector2D(0.f, 2200.f), FVector2D(4750.f, 5000.f)) } },
			{ TEXT("plato"), { FBox2D(FVector2D(-2700.f, -3400.f), FVector2D(4300.f, 2200.f)) } },
			{ TEXT("svalka"), { FBox2D(FVector2D(4300.f, -3400.f), FVector2D(5000.f, -2500.f)), FBox2D(FVector2D(4300.f, -2500.f), FVector2D(7000.f, 2200.f)), FBox2D(FVector2D(4750.f, 2200.f), FVector2D(7000.f, 5000.f)) } },
			{ TEXT("zastenye"), { FBox2D(FVector2D(-7200.f, 4750.f), FVector2D(2700.f, 5000.f)), FBox2D(FVector2D(3500.f, 5000.f), FVector2D(4300.f, 5550.f)), FBox2D(FVector2D(7000.f, 880.f), FVector2D(7200.f, 1080.f)) } },
			{ TEXT("porog"), { FBox2D(FVector2D(5000.f, -5000.f), FVector2D(7000.f, -3400.f)), FBox2D(FVector2D(5000.f, -3400.f), FVector2D(7000.f, -2500.f)), FBox2D(FVector2D(7000.f, -4800.f), FVector2D(7200.f, -3200.f)), FBox2D(FVector2D(7200.f, -4700.f), FVector2D(10000.f, 1500.f)) } },
			{ TEXT("babushka"), { FBox2D(FVector2D(7200.f, -9000.f), FVector2D(10400.f, -4700.f)) } },
			{ TEXT("play"), { FBox2D(FVector2D(10000.f, -4700.f), FVector2D(18800.f, 1500.f)), FBox2D(FVector2D(10400.f, -7000.f), FVector2D(18800.f, -4700.f)) } },
			{ TEXT("stenka"), { FBox2D(FVector2D(10400.f, -9000.f), FVector2D(20200.f, -7000.f)), FBox2D(FVector2D(18800.f, -7000.f), FVector2D(20200.f, 1500.f)) } },
		};
		return D;
	}

	/** Catches the engine's "<actor> is stuck and failed to move! ... Location: X=.. Y=.. Z=.." lines. */
	class FStuckTap : public FOutputDevice
	{
	public:
		FCriticalSection Lock;
		TArray<FString> Lines;
		int32 Total = 0;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			if (V && FCString::Strstr(V, TEXT(" is stuck and failed to move")))
			{
				FScopeLock L(&Lock);
				++Total;
				if (Lines.Num() < 20000) { Lines.Add(V); }
			}
		}
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
	};
	FStuckTap* GTap = nullptr;

	FString SavedDir() { return FPaths::ProjectSavedDir() / TEXT("map4"); }

	bool IsPlaceholderMaterial(const UMaterialInterface* M)
	{
		if (!M) { return true; }
		const UMaterial* Base = M->GetMaterial();
		const FString P = Base ? Base->GetPathName() : FString();
		return P.StartsWith(TEXT("/Engine/BasicShapes/")) || P.StartsWith(TEXT("/Engine/EngineMaterials/WorldGridMaterial")) || P.StartsWith(TEXT("/Engine/EngineMaterials/DefaultMaterial"));
	}

	FString Stem(const FString& Name)
	{
		FString S = Name;
		while (S.Len() > 0 && (FChar::IsDigit(S[S.Len() - 1]) || S[S.Len() - 1] == TEXT('_'))) { S.LeftChopInline(1); }
		return S;
	}
}

namespace Map4
{

void MarkDebug(UActorComponent* C)
{
	if (C) { C->ComponentTags.AddUnique(TEXT("AntDebug")); }
}

FString DistrictAt(const FVector& P)
{
	for (const FDistrict& D : Districts())
	{
		for (const FBox2D& R : D.Rects)
		{
			if (R.IsInside(FVector2D(P.X, P.Y))) { return D.Id; }
		}
	}
	return TEXT("out");
}

int32 Audit(UWorld* W, bool bVerbose)
{
	if (!W) { return -1; }
	struct FRow { int32 Shapes = 0; int32 Flat = 0; int32 Proc = 0; int32 Generic = 0; int32 Arch = 0; FVector At = FVector::ZeroVector; FVector Size = FVector::ZeroVector; FString District; };
	TMap<FString, FRow> Rows;
	TArray<FString> Full;
	Full.Add(TEXT("owner_class,owner,component,mesh,kind,x,y,z,size_x,size_y,size_z,district"));
	int32 Shapes = 0, Flat = 0, Proc = 0, Comps = 0, Generic = 0, Arch = 0;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsHidden()) { continue; }
		TInlineComponentArray<UPrimitiveComponent*> Prims(A);
		for (UPrimitiveComponent* C : Prims)
		{
			if (!C || !C->IsRegistered() || !C->IsVisible() || C->bHiddenInGame || C->ComponentHasTag(TEXT("AntDebug"))) { continue; }
			if (C->GetComponentTransform().GetScale3D().GetAbsMax() < 0.002f) { continue; }
			++Comps;
			FString Mesh;
			int32 N = 1;
			bool bShape = false, bFlat = false, bProc = false, bGeneric = false;
			if (UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
			{
				const UStaticMesh* M = S->GetStaticMesh();
				if (!M) { continue; }
				if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(S))
				{
					N = 0;
					for (int32 k = 0; k < I->GetInstanceCount(); ++k)
					{
						FTransform T;
						if (I->GetInstanceTransform(k, T, false) && T.GetScale3D().GetAbsMax() > 0.01f) { ++N; }
					}
					if (N == 0) { continue; }
				}
				Mesh = M->GetName();
				bShape = M->GetPathName().StartsWith(TEXT("/Engine/"));
				// map v4's own base shapes: not engine placeholders, but still plain boxes / balls unless they are architecture
				static const TCHAR* GenericNames[] = { TEXT("SM_M4_Box"), TEXT("SM_M4_Ball"), TEXT("SM_M4_Cyl"), TEXT("SM_M4_Cone"), TEXT("SM_M4_Plane") };
				for (const TCHAR* G : GenericNames) { if (Mesh == G && !C->ComponentHasTag(TEXT("M4Skin"))) { bGeneric = true; } }
				for (int32 k = 0; k < S->GetNumMaterials(); ++k)
				{
					if (IsPlaceholderMaterial(S->GetMaterial(k))) { bFlat = true; }
				}
			}
			else if (C->IsA<UProceduralMeshComponent>())
			{
				Mesh = TEXT("(procedural)");
				bProc = true;
			}
			else
			{
				continue;   // skeletal meshes, decals, particles: not placeholders
			}
			if (!bShape && !bFlat && !bProc && !bGeneric) { continue; }
			const FVector Sz = C->Bounds.BoxExtent * 2.f;
			const bool bArch = bGeneric && Sz.GetMax() > 1200.f && Sz.GetMin() < 260.f;   // walls, floors, ceilings, shelves: slabs
			Full.Add(FString::Printf(TEXT("%s,%s,%s,%s,%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%s"), *A->GetClass()->GetName(), *A->GetName(), *C->GetName(), *Mesh,
				bShape ? TEXT("shape") : bFlat ? TEXT("flat") : bProc ? TEXT("proc") : bArch ? TEXT("generic_arch") : TEXT("generic"), C->Bounds.Origin.X, C->Bounds.Origin.Y, C->Bounds.Origin.Z,
				C->Bounds.BoxExtent.X * 2.f, C->Bounds.BoxExtent.Y * 2.f, C->Bounds.BoxExtent.Z * 2.f, *DistrictAt(C->Bounds.Origin)));
			const FString Key = FString::Printf(TEXT("%s|%s|%s"), *A->GetClass()->GetName(), *Stem(C->GetName()), *Mesh);
			FRow& R = Rows.FindOrAdd(Key);
			if (R.Shapes + R.Flat + R.Proc == 0)
			{
				R.At = C->Bounds.Origin;
				R.Size = C->Bounds.BoxExtent * 2.f;
				R.District = DistrictAt(R.At);
			}
			if (bShape) { R.Shapes += N; Shapes += N; }
			else if (bFlat) { R.Flat += N; Flat += N; }
			if (bProc) { R.Proc += N; Proc += N; }
			if (bGeneric) { if (bArch) { R.Arch += N; Arch += N; } else { R.Generic += N; Generic += N; } }
		}
	}
	Rows.ValueSort([](const FRow& L, const FRow& R) { return L.Shapes + L.Flat + L.Generic > R.Shapes + R.Flat + R.Generic; });
	TArray<FString> Csv;
	Csv.Add(TEXT("owner_class,component,mesh,shapes,flat_paint,procedural,generic,generic_arch,x,y,z,size_x,size_y,size_z,district"));
	for (const TPair<FString, FRow>& P : Rows)
	{
		TArray<FString> K;
		P.Key.ParseIntoArray(K, TEXT("|"));
		const FRow& R = P.Value;
		Csv.Add(FString::Printf(TEXT("%s,%s,%s,%d,%d,%d,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%s"), *K[0], K.IsValidIndex(1) ? *K[1] : TEXT(""), K.IsValidIndex(2) ? *K[2] : TEXT(""),
			R.Shapes, R.Flat, R.Proc, R.Generic, R.Arch, R.At.X, R.At.Y, R.At.Z, R.Size.X, R.Size.Y, R.Size.Z, *R.District));
		if (bVerbose && (R.Shapes > 0 || R.Flat > 0 || R.Generic > 0))
		{
			UE_LOG(LogTemp, Display, TEXT("ARTAUDIT %-70s shapes=%d flat=%d generic=%d at (%.0f,%.0f,%.0f) size (%.0f,%.0f,%.0f) %s"), *P.Key, R.Shapes, R.Flat, R.Generic, R.At.X, R.At.Y, R.At.Z, R.Size.X, R.Size.Y, R.Size.Z, *R.District);
		}
	}
	FFileHelper::SaveStringArrayToFile(Csv, *(SavedDir() / TEXT("art_audit.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	FFileHelper::SaveStringArrayToFile(Full, *(SavedDir() / TEXT("art_audit_full.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogTemp, Display, TEXT("ARTAUDIT RESULT primitives=%d flat_paint=%d generic_shapes=%d generic_architecture=%d procedural=%d (of %d visible components, %d kinds) -> %s"),
		Shapes, Flat, Generic, Arch, Proc, Comps, Rows.Num(), *(SavedDir() / TEXT("art_audit.csv")));
	return Shapes;
}

}   // namespace Map4

using Map4::DistrictAt;

// ------------------------------------------------------------------------------------------ cameras

static TArray<AMap4Runner::FCam> AllCams()
{
	// From, To (world, uu), fov. Ant-eye shots sit 40-120 above the surface; two human-eye overviews.
	return {
		{ TEXT("01_palisad"),  FVector(-11800.f, -600.f, 90.f),   FVector(-7200.f, 300.f, 700.f),   70.f },
		{ TEXT("02_reddvor"),  FVector(-1500.f, -7600.f, 110.f),  FVector(5000.f, -6400.f, 900.f),  70.f },
		{ TEXT("03_plintus"),  FVector(-6300.f, -4300.f, 70.f),   FVector(-3500.f, 600.f, 500.f),   72.f },
		{ TEXT("04_kladovye"), FVector(-4300.f, 900.f, 140.f),    FVector(-5800.f, 4300.f, 1600.f), 72.f },
		{ TEXT("05_moika"),    FVector(-1700.f, 3000.f, 1790.f),  FVector(-3400.f, 4300.f, 1950.f), 72.f },
		{ TEXT("06_plita"),    FVector(700.f, 1000.f, 500.f),     FVector(-750.f, 4200.f, 2000.f),  72.f },
		{ TEXT("07_lednik"),   FVector(1300.f, -200.f, 90.f),     FVector(3500.f, 4200.f, 2600.f),  74.f },
		{ TEXT("08_plato"),    FVector(-2300.f, -2800.f, 90.f),   FVector(600.f, -400.f, 1300.f),   74.f },
		{ TEXT("09_svalka"),   FVector(3000.f, -2700.f, 70.f),    FVector(5800.f, 500.f, 500.f),    72.f },
		{ TEXT("10_zastenye"), FVector(-6600.f, 4875.f, 60.f),    FVector(1500.f, 4875.f, 120.f),   80.f },
		{ TEXT("11_porog"),    FVector(5200.f, -1800.f, 80.f),    FVector(7100.f, -4000.f, 1400.f), 74.f },
		{ TEXT("12_babushka"), FVector(11200.f, -6000.f, 90.f),   FVector(8600.f, -8000.f, 1400.f), 74.f },
		{ TEXT("13_play"),     FVector(11800.f, -1200.f, 80.f),   FVector(14800.f, -5300.f, 600.f), 74.f },
		{ TEXT("14_stenka"),   FVector(15200.f, -4800.f, 120.f),  FVector(19600.f, -8200.f, 2000.f),74.f },
		{ TEXT("20_kitchen_overview"), FVector(6400.f, -4600.f, 3600.f), FVector(-1500.f, 2400.f, 1000.f), 80.f },
		{ TEXT("21_living_overview"),  FVector(8000.f, 900.f, 3600.f),   FVector(16000.f, -6000.f, 900.f), 80.f },
		{ TEXT("22_table_top"),  FVector(-1200.f, -1400.f, 1460.f), FVector(1500.f, 300.f, 1600.f), 74.f },
		{ TEXT("23_worktop"),    FVector(-5800.f, 3400.f, 1770.f),  FVector(-200.f, 4300.f, 1900.f), 74.f },
	};
}

AMap4Runner::AMap4Runner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorHiddenInGame(true);
}

void AMap4Runner::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GTap && bColl)
	{
		GLog->RemoveOutputDevice(GTap);
	}
	Super::EndPlay(Reason);
}

void AMap4Runner::StartTour(const FString& Tag, const FString& Only)
{
	Cams.Reset();
	TArray<FString> Want;
	Only.ParseIntoArray(Want, TEXT(","));
	for (const FCam& C : AllCams())
	{
		bool bOk = Want.Num() == 0;
		for (const FString& Wn : Want) { if (C.Name.Contains(Wn)) { bOk = true; } }
		if (bOk) { Cams.Add(C); }
	}
	ShotDir = SavedDir() / TEXT("shots") / Tag;
	CamIndex = -1;
	CamAt = GetWorld()->GetRealTimeSeconds() + 3.f;   // let the level settle (Lumen, streaming)
	bShotTaken = true;
	UE_LOG(LogTemp, Display, TEXT("M4SHOTS start %d cameras -> %s"), Cams.Num(), *ShotDir);
}

void AMap4Runner::Event(const FString& Kind, const FVector& At, const FString& Detail)
{
	const FString D = DistrictAt(At);
	Counts.FindOrAdd(Kind + TEXT("|") + D)++;
	Counts.FindOrAdd(Kind + TEXT("|ALL"))++;
	Events.Add(FString::Printf(TEXT("%.1f,%s,%s,%.0f,%.0f,%.0f,%s"), GetWorld()->GetTimeSeconds() - CollStart, *Kind, *D, At.X, At.Y, At.Z, *Detail.Replace(TEXT(","), TEXT(";"))));
	UE_LOG(LogTemp, Display, TEXT("M4COLL event %s %s (%.0f,%.0f,%.0f) %s"), *Kind, *D, At.X, At.Y, At.Z, *Detail);
}

void AMap4Runner::BuildJobs(const FString& Kinds)
{
	UWorld* W = GetWorld();
	Jobs.Reset();
	const bool bAll = Kinds.IsEmpty() || Kinds == TEXT("all");
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_AntClimbable);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Map4Jobs), true);
	auto Ground = [&](float X, float Y, float FromZ, FVector& Out) -> bool
	{
		FHitResult H;
		if (W->LineTraceSingleByObjectType(H, FVector(X, Y, FromZ), FVector(X, Y, -800.f), Obj, Q) && H.ImpactNormal.Z > 0.6f)
		{
			Out = H.ImpactPoint + FVector(0.f, 0.f, 25.f);
			return true;
		}
		return false;
	};
	// Floors: rows across every district rectangle, 900 apart, low enough to stay under the furniture tops.
	if (bAll || Kinds.Contains(TEXT("floor")))
	{
		for (const FDistrict& D : Districts())
		{
			for (const FBox2D& R : D.Rects)
			{
				const FVector2D Sz = R.GetSize();
				if (Sz.X < 300.f || Sz.Y < 300.f) { continue; }
				const bool bAlongX = Sz.X >= Sz.Y;
				const float Across = bAlongX ? Sz.Y : Sz.X;
				const int32 Rows = FMath::Max(1, FMath::FloorToInt(Across / 900.f));
				for (int32 r = 0; r < Rows; ++r)
				{
					const float T = (r + 0.5f) / Rows;
					FVector A, B;
					const FVector2D P0 = bAlongX ? FVector2D(R.Min.X + 150.f, R.Min.Y + T * Sz.Y) : FVector2D(R.Min.X + T * Sz.X, R.Min.Y + 150.f);
					const FVector2D P1 = bAlongX ? FVector2D(R.Max.X - 150.f, R.Min.Y + T * Sz.Y) : FVector2D(R.Min.X + T * Sz.X, R.Max.Y - 150.f);
					if (!Ground(P0.X, P0.Y, 120.f, A) || !Ground(P1.X, P1.Y, 120.f, B)) { continue; }
					FJob J; J.Name = FString::Printf(TEXT("floor_%s_%d"), D.Id, Jobs.Num());
					J.Pts = { A, B };
					Jobs.Add(J);
				}
			}
		}
	}
	// Tops: every solid piece with a walkable top: drop on it, walk a loop around its middle (edges included).
	if (bAll || Kinds.Contains(TEXT("top")))
	{
		TSet<UPrimitiveComponent*> Seen;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->IsA<APawn>()) { continue; }
			TInlineComponentArray<UStaticMeshComponent*> Comps(*It);
			for (UStaticMeshComponent* C : Comps)
			{
				if (!C || !C->IsVisible() || C->GetCollisionEnabled() == ECollisionEnabled::NoCollision || C->ComponentHasTag(TEXT("AntDebug"))) { continue; }
				const FBoxSphereBounds B = C->Bounds;
				if (B.BoxExtent.X < 30.f || B.BoxExtent.Y < 30.f || B.BoxExtent.X > 3500.f || B.BoxExtent.Y > 3500.f) { continue; }
				if (B.Origin.Z + B.BoxExtent.Z > 6000.f) { continue; }
				FHitResult H;
				const FVector Top(B.Origin.X, B.Origin.Y, B.Origin.Z + B.BoxExtent.Z + 40.f);
				if (!W->LineTraceSingleByObjectType(H, Top, Top - FVector(0.f, 0.f, B.BoxExtent.Z * 2.f + 80.f), Obj, Q) || H.GetComponent() != C || H.ImpactNormal.Z < 0.7f) { continue; }
				if (Seen.Contains(C)) { continue; }
				Seen.Add(C);
				const float Ex = FMath::Min(B.BoxExtent.X * 0.6f, 600.f), Ey = FMath::Min(B.BoxExtent.Y * 0.6f, 600.f);
				const FVector Z0 = H.ImpactPoint + FVector(0.f, 0.f, 22.f);
				FJob J; J.Name = FString::Printf(TEXT("top_%s"), *(C->GetStaticMesh() ? C->GetStaticMesh()->GetName() : C->GetName()));
				J.Pts = { Z0, Z0 + FVector(Ex, Ey, 0.f), Z0 + FVector(-Ex, Ey, 0.f), Z0 + FVector(-Ex, -Ey, 0.f), Z0 + FVector(Ex, -Ey, 0.f), Z0 };
				Jobs.Add(J);
			}
		}
	}
	// Walls and ceilings: from the floor up the wall, along it, (onto the ceiling and back), down again.
	if (bAll || Kinds.Contains(TEXT("wall")) || Kinds.Contains(TEXT("ceil")))
	{
		struct FRoom { FVector2D Min, Max; };
		const FRoom Rooms[] = { { FVector2D(-7000.f, -5000.f), FVector2D(7000.f, 5000.f) }, { FVector2D(7200.f, -9000.f), FVector2D(20200.f, 1500.f) } };
		const bool bCeil = bAll || Kinds.Contains(TEXT("ceil"));
		for (const FRoom& Rm : Rooms)
		{
			const FVector2D C = (Rm.Min + Rm.Max) * 0.5f;
			FHitResult Up;
			const float CeilZ = W->LineTraceSingleByObjectType(Up, FVector(C.X, C.Y, 400.f), FVector(C.X, C.Y, 9000.f), Obj, Q) ? Up.ImpactPoint.Z : 5100.f;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const bool bX = Side < 2;   // walls at constant X (west/east) or constant Y
				const float Len = bX ? Rm.Max.Y - Rm.Min.Y : Rm.Max.X - Rm.Min.X;
				for (float s = 1200.f; s < Len - 600.f; s += 2600.f)
				{
					const float Along = (bX ? Rm.Min.Y : Rm.Min.X) + s;
					// find the wall: a horizontal trace from 900 inside the room towards it, at 2300 (above the furniture)
					const FVector2D Dir2 = Side == 0 ? FVector2D(-1, 0) : Side == 1 ? FVector2D(1, 0) : Side == 2 ? FVector2D(0, -1) : FVector2D(0, 1);
					const FVector2D Edge = Side == 0 ? FVector2D(Rm.Min.X, Along) : Side == 1 ? FVector2D(Rm.Max.X, Along) : Side == 2 ? FVector2D(Along, Rm.Min.Y) : FVector2D(Along, Rm.Max.Y);
					const FVector2D In = Edge - Dir2 * 900.f;
					FHitResult Hw;
					const FVector D3(Dir2.X, Dir2.Y, 0.f);
					if (!W->LineTraceSingleByObjectType(Hw, FVector(In.X, In.Y, 2300.f), FVector(In.X, In.Y, 2300.f) + D3 * 1400.f, Obj, Q)) { continue; }
					const FVector WallP = Hw.ImpactPoint - D3 * 14.f;
					FVector Floor;
					if (!Ground(In.X + Dir2.X * 500.f, In.Y + Dir2.Y * 500.f, 200.f, Floor)) { continue; }
					const FVector Side3 = bX ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
					FJob J; J.Name = FString::Printf(TEXT("wall_%s"), *DistrictAt(WallP));
					J.Pts.Add(Floor);
					J.Pts.Add(FVector(WallP.X, WallP.Y, 2300.f));
					J.Pts.Add(FVector(WallP.X, WallP.Y, 2300.f) + Side3 * 900.f);
					if (bCeil)
					{
						J.Name = FString::Printf(TEXT("ceil_%s"), *DistrictAt(WallP));
						J.Pts.Add(FVector(WallP.X, WallP.Y, CeilZ - 120.f) + Side3 * 900.f);
						J.Pts.Add(FVector(WallP.X, WallP.Y, CeilZ - 16.f) + Side3 * 900.f - D3 * 1200.f);
						J.Pts.Add(FVector(WallP.X, WallP.Y, CeilZ - 120.f) + Side3 * 900.f);
					}
					J.Pts.Add(FVector(WallP.X, WallP.Y, 2300.f));
					J.Pts.Add(Floor);
					Jobs.Add(J);
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("M4COLL %d jobs (%s)"), Jobs.Num(), *Kinds);
}

void AMap4Runner::StartColl(int32 NumWalkers, const FString& Kinds, const FString& Tag)
{
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (!GM) { return; }
	BuildJobs(Kinds);
	CollTag = Tag;
	Events.Reset();
	Counts.Reset();
	NextJob = 0;
	if (!GTap) { GTap = new FStuckTap(); }
	{
		FScopeLock L(&GTap->Lock);
		GTap->Lines.Reset();
		GTap->Total = 0;
	}
	GLog->AddOutputDevice(GTap);
	for (int32 i = 0; i < NumWalkers; ++i)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMap4Walker* C = GetWorld()->SpawnActor<AMap4Walker>(AMap4Walker::StaticClass(), FTransform::Identity, P);
		if (!C) { continue; }
		GM->RestartPlayer(C);
		FWalker Wk;
		Wk.Ctrl = C;
		Wk.Ant = Cast<AAntCharacter>(C->GetPawn());
		Walkers.Add(Wk);
	}
	CollStart = GetWorld()->GetTimeSeconds();
	bColl = true;
	UE_LOG(LogTemp, Display, TEXT("M4COLL start: %d walkers, %d jobs"), Walkers.Num(), Jobs.Num());
}

void AMap4Runner::NextFor(FWalker& Wk)
{
	AAntCharacter* Ant = Wk.Ant.Get();
	if (!Ant) { return; }
	if (NextJob >= Jobs.Num())
	{
		Wk.Job = -1;
		Ant->bDevGo = false;
		return;
	}
	Wk.Job = NextJob++;
	Wk.Pt = 1;
	const FJob& J = Jobs[Wk.Job];
	Ant->ApplyTeleport(J.Pts[0] + FVector(0.f, 0.f, 10.f));
	const float Now = GetWorld()->GetTimeSeconds();
	Wk.PtStart = Wk.LastCheck = Now;
	Wk.BestDist = 1.e9f;
	Wk.StallChecks = 0;
	Wk.FallingFor = 0.f;
	if (Wk.Pt < J.Pts.Num()) { Ant->DevGoTo = J.Pts[Wk.Pt]; Ant->bDevGo = true; }
}

void AMap4Runner::FinishColl()
{
	bColl = false;
	GLog->RemoveOutputDevice(GTap);
	for (FWalker& Wk : Walkers)
	{
		if (AAntCharacter* Ant = Wk.Ant.Get()) { Ant->bDevGo = false; Ant->Destroy(); }
		if (AController* C = Wk.Ctrl.Get()) { C->Destroy(); }
	}
	Walkers.Reset();
	const float Mins = (GetWorld()->GetTimeSeconds() - CollStart) / 60.f;
	TArray<FString> Csv;
	Csv.Add(TEXT("t,kind,district,x,y,z,detail"));
	Csv.Append(Events);
	FFileHelper::SaveStringArrayToFile(Csv, *(SavedDir() / FString::Printf(TEXT("coll_%s.csv"), *CollTag)), FFileHelper::EEncodingOptions::ForceUTF8);
	TArray<FString> Keys;
	Counts.GetKeys(Keys);
	Keys.Sort();
	FString Line;
	for (const FString& K : Keys) { Line += FString::Printf(TEXT(" %s=%d"), *K, Counts[K]); }
	UE_LOG(LogTemp, Display, TEXT("M4COLL SUMMARY tag=%s jobs=%d minutes=%.1f engine_stuck_lines=%d%s"), *CollTag, Jobs.Num(), Mins, GTap ? GTap->Total : 0, *Line);
	UE_LOG(LogTemp, Display, TEXT("M4COLL DONE"));
}

void AMap4Runner::StartHuman()
{
	// The Human's rounds, straight legs between free points (the ring around the table, the door, the sofa, back).
	HPts = { FVector(3800.f, -3200.f, 0.f), FVector(3600.f, 1500.f, 0.f), FVector(-2600.f, 1500.f, 0.f), FVector(-3800.f, -1500.f, 0.f), FVector(-2600.f, -3600.f, 0.f),
		FVector(3800.f, -3600.f, 0.f), FVector(6200.f, -3700.f, 0.f), FVector(8600.f, -3900.f, 0.f), FVector(11300.f, -1800.f, 0.f), FVector(16000.f, -2200.f, 0.f),
		FVector(16500.f, -6500.f, 0.f), FVector(11000.f, -6800.f, 0.f), FVector(8600.f, -3900.f, 0.f), FVector(6200.f, -3700.f, 0.f), FVector(3800.f, -3200.f, 0.f) };
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetClass() == AHumanCharacter::StaticClass())
		{
			It->bDevPuppet = true;
			It->SetActorLocation(FVector(HPts[0].X, HPts[0].Y, It->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			HPt = 1;
			It->DevWalk = FVector(HPts[1].X, HPts[1].Y, 0.f);
			HPtStart = HLastCheck = GetWorld()->GetTimeSeconds();
			HBest = 1.e9f;
			HStalls = HThrough = HReached = 0;
			HThroughWhat.Reset();
			bHuman = true;
			UE_LOG(LogTemp, Display, TEXT("M4HUMAN start %d points"), HPts.Num());
			return;
		}
	}
}

void AMap4Runner::Tick(float Dt)
{
	Super::Tick(Dt);
	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	const float RealNow = W->GetRealTimeSeconds();
	const float Now = W->GetTimeSeconds();

	// ---- camera tour
	if (Cams.Num() > 0 && PC)
	{
		if (CamIndex >= 0 && CamIndex < Cams.Num() && CamActor.IsValid() && PC->GetViewTarget() != CamActor.Get()) { PC->SetViewTarget(CamActor.Get()); }
		if (PC->GetHUD()) { PC->GetHUD()->bShowHUD = false; }
		if (RealNow >= CamAt)
		{
			if (!bShotTaken && CamIndex >= 0)
			{
				const FString File = ShotDir / Cams[CamIndex].Name + TEXT(".png");
				FScreenshotRequest::RequestScreenshot(File, false, false);
				UE_LOG(LogTemp, Display, TEXT("M4SHOTS %s"), *File);
				bShotTaken = true;
				CamAt = RealNow + 0.6f;
			}
			else if (CamIndex + 1 < Cams.Num())
			{
				++CamIndex;
				const FCam& C = Cams[CamIndex];
				if (!CamActor.IsValid()) { CamActor = W->SpawnActor<ACameraActor>(); }
				if (ACameraActor* Cam = Cast<ACameraActor>(CamActor.Get()))
				{
					Cam->SetActorLocationAndRotation(C.From, (C.To - C.From).Rotation());
					Cam->GetCameraComponent()->SetFieldOfView(C.Fov);
					Cam->GetCameraComponent()->bConstrainAspectRatio = false;
					PC->SetViewTarget(Cam);
				}
				bShotTaken = false;
				CamAt = RealNow + 2.2f;
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("M4SHOTS DONE %d"), Cams.Num());
				Cams.Reset();
				if (PC->GetPawn()) { PC->SetViewTarget(PC->GetPawn()); }
				if (FParse::Param(FCommandLine::Get(), TEXT("AntM4Quit"))) { FPlatformMisc::RequestExit(false); }
			}
		}
	}

	// ---- collision test
	if (bColl)
	{
		// engine "stuck" lines
		TArray<FString> Lines;
		{
			FScopeLock L(&GTap->Lock);
			Lines = MoveTemp(GTap->Lines);
			GTap->Lines.Reset();
		}
		for (const FString& S : Lines)
		{
			FVector P = FVector::ZeroVector;
			const int32 At = S.Find(TEXT("Location:"));
			if (At != INDEX_NONE)
			{
				const FString R = S.Mid(At + 9);
				FParse::Value(*R, TEXT("X="), P.X);
				FParse::Value(*R, TEXT("Y="), P.Y);
				FParse::Value(*R, TEXT("Z="), P.Z);
			}
			const FString Who = S.Left(S.Find(TEXT(" is stuck")));
			bool bMine = false;
			for (const FWalker& Wk : Walkers) { if (Wk.Ant.IsValid() && Who.EndsWith(Wk.Ant->GetName())) { bMine = true; } }
			Event(bMine ? TEXT("stuck") : TEXT("stuck_other"), P, Who.Right(60));
		}

		FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		bool bAnyBusy = false;
		for (FWalker& Wk : Walkers)
		{
			AAntCharacter* Ant = Wk.Ant.Get();
			if (!Ant || Ant->bDead)
			{
				if (Ant) { Event(TEXT("died"), Ant->GetActorLocation(), Wk.Job >= 0 ? Jobs[Wk.Job].Name : FString()); }
				if (AController* C = Wk.Ctrl.Get())
				{
					if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>()) { GM->RestartPlayer(C); }
					Wk.Ant = Cast<AAntCharacter>(C->GetPawn());
					Wk.Job = -1;
					if (Wk.Ant.IsValid()) { NextFor(Wk); }
				}
				bAnyBusy = true;
				continue;
			}
			if (Wk.Job < 0)
			{
				if (NextJob < Jobs.Num()) { NextFor(Wk); bAnyBusy = true; }
				continue;
			}
			bAnyBusy = true;
			const FJob& J = Jobs[Wk.Job];
			const FVector L = Ant->GetActorLocation();
			const FVector T = J.Pts[Wk.Pt];
			const float Dist = FVector::Dist(L, T);
			auto Advance = [&](bool bTeleport)
			{
				++Wk.Pt;
				Wk.PtStart = Wk.LastCheck = Now;
				Wk.BestDist = 1.e9f;
				Wk.StallChecks = 0;
				if (Wk.Pt >= J.Pts.Num()) { Wk.Job = -1; Ant->bDevGo = false; return; }
				if (bTeleport) { Ant->ApplyTeleport(T + FVector(0.f, 0.f, 10.f)); }
				Ant->DevGoTo = J.Pts[Wk.Pt];
				Ant->bDevGo = true;
			};
			if (Dist < 55.f) { Advance(false); continue; }
			UCharacterMovementComponent* CM = Ant->GetCharacterMovement();
			Wk.FallingFor = (CM && CM->IsFalling()) ? Wk.FallingFor + Dt : 0.f;
			if (Wk.FallingFor > 4.f) { Event(TEXT("fall_long"), L, J.Name); Wk.FallingFor = 0.f; Advance(true); continue; }
			if (L.Z < -400.f) { Event(TEXT("under"), L, J.Name); Advance(true); continue; }
			if (Now - Wk.LastCheck >= 1.f)
			{
				Wk.LastCheck = Now;
				if (Dist < Wk.BestDist - 20.f) { Wk.BestDist = Dist; Wk.StallChecks = 0; }
				else if (++Wk.StallChecks >= 7) { Event(TEXT("stall"), L, FString::Printf(TEXT("%s pt%d to (%.0f,%.0f,%.0f)"), *J.Name, Wk.Pt, T.X, T.Y, T.Z)); Advance(true); continue; }
				// embedded: the ant's centre inside something solid
				FCollisionQueryParams Qp(SCENE_QUERY_STAT(Map4Embed), true, Ant);
				TArray<FOverlapResult> Ov;
				if (W->OverlapMultiByObjectType(Ov, L, FQuat::Identity, Obj, FCollisionShape::MakeSphere(1.5f), Qp))
				{
					for (const FOverlapResult& O : Ov)
					{
						if (O.GetComponent() && O.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
						{
							Event(TEXT("embed"), L, FString::Printf(TEXT("%s in %s"), *J.Name, *O.GetComponent()->GetName()));
							Advance(true);
							break;
						}
					}
					if (Wk.Job < 0 || Wk.Pt >= J.Pts.Num()) { continue; }
				}
			}
			if (Now - Wk.PtStart > 30.f + FVector::Dist(J.Pts[FMath::Max(0, Wk.Pt - 1)], T) / 60.f) { Event(TEXT("slow"), L, J.Name); Advance(true); }
		}
		if (!bAnyBusy && NextJob >= Jobs.Num()) { FinishColl(); }
	}

	// ---- Human rounds
	if (bHuman)
	{
		AHumanCharacter* H = nullptr;
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (It->GetClass() == AHumanCharacter::StaticClass()) { H = *It; } }
		if (!H) { bHuman = false; return; }
		const FVector L = H->GetActorLocation();
		const FVector T = HPts[HPt];
		const float Dist = FVector::Dist2D(L, T);
		auto Next = [&](bool bTp)
		{
			if (bTp) { H->SetActorLocation(FVector(T.X, T.Y, L.Z + 20.f), false, nullptr, ETeleportType::TeleportPhysics); }
			if (++HPt >= HPts.Num())
			{
				bHuman = false;
				H->bDevPuppet = false;
				FString What;
				for (const FString& S : HThroughWhat) { What += S + TEXT(" "); }
				UE_LOG(LogTemp, Display, TEXT("M4HUMAN DONE reached=%d stalls=%d walkthrough=%d (%s)"), HReached, HStalls, HThrough, *What);
				return;
			}
			HPtStart = HLastCheck = Now;
			HBest = 1.e9f;
			H->DevWalk = FVector(HPts[HPt].X, HPts[HPt].Y, 0.f);
		};
		if (Dist < 260.f) { ++HReached; Next(false); return; }
		if (H->DevWalk.IsZero()) { H->DevWalk = FVector(T.X, T.Y, 0.f); }
		if (Now - HLastCheck > 1.f)
		{
			HLastCheck = Now;
			// walking through furniture: his capsule (shrunk 15%) overlapping a solid static piece
			if (UCapsuleComponent* Cap = H->GetCapsuleComponent())
			{
				TArray<FOverlapResult> Ov;
				FCollisionQueryParams Qp(SCENE_QUERY_STAT(Map4Human), false, H);
				const FCollisionShape S = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() * 0.85f, Cap->GetScaledCapsuleHalfHeight() * 0.8f);
				if (W->OverlapMultiByObjectType(Ov, L + FVector(0.f, 0.f, Cap->GetScaledCapsuleHalfHeight() * 0.15f), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), S, Qp))
				{
					for (const FOverlapResult& O : Ov)
					{
						UPrimitiveComponent* C = O.GetComponent();
						if (C && C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
						{
							++HThrough;
							const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
							HThroughWhat.Add(SM && SM->GetStaticMesh() ? SM->GetStaticMesh()->GetName() : C->GetName());
							UE_LOG(LogTemp, Display, TEXT("M4HUMAN through %s at (%.0f,%.0f)"), SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : *C->GetName(), L.X, L.Y);
						}
					}
				}
			}
			if (Dist < HBest - 60.f) { HBest = Dist; }
			else if (Now - HPtStart > 6.f)
			{
				++HStalls;
				UE_LOG(LogTemp, Display, TEXT("M4HUMAN stall at (%.0f,%.0f) to point %d (%.0f,%.0f)"), L.X, L.Y, HPt, T.X, T.Y);
				Next(true);
				return;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------ commands

namespace Map4
{

AMap4Runner* Runner(UWorld* W)
{
	for (TActorIterator<AMap4Runner> It(W); It; ++It) { return *It; }
	return W->SpawnActor<AMap4Runner>();
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0) { return false; }
	UWorld* W = GM->GetWorld();
	const FString Op = A[0].ToLower();
	if (Op == TEXT("art") && A.Num() >= 2)
	{
		const FString Sub = A[1].ToLower();
		if (Sub == TEXT("audit")) { Audit(W, true); return true; }
		if (Sub == TEXT("shots")) { Runner(W)->StartTour(A.IsValidIndex(2) ? A[2] : FString(TEXT("now")), A.IsValidIndex(3) ? A[3] : FString()); return true; }
		return false;
	}
	if (Op == TEXT("colltest"))
	{
		Runner(W)->StartColl(A.IsValidIndex(1) ? FCString::Atoi(*A[1]) : 8, A.IsValidIndex(2) ? A[2] : FString(TEXT("all")), A.IsValidIndex(3) ? A[3] : FString(TEXT("run")));
		return true;
	}
	if (Op == TEXT("humantest")) { Runner(W)->StartHuman(); return true; }
	return false;
}

}
