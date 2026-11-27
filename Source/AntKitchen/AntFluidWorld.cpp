#include "AntFluidWorld.h"
#include "Map4Look.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "KitchenLoot.h"
#include "KitchenInteractables.h"
#include "HumanCharacter.h"
#include "AntFx.h"
#include "AntMess.h"
#include "NpcRoutine.h"
#include "LevelV3.h"
#include "AntLab.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Async/ParallelFor.h"
#include "RHI.h"
#include "AntFighter.h"
#include "AntReact.h"
#include "AntMatter.h"

using namespace AntFluid;

namespace
{
	const float StepDt = 1.f / 60.f;
	const int32 Tile = 64;

	struct FBasinDef { const TCHAR* Name; float X0, Y0, X1, Y1; float Dx; float RefX, RefY, ZGuess; float Up, Down; int32 Sub; float Fill = -1.e9f; };
	// The bed of each basin is whatever the level's collision is under it: nothing here knows where a chair stands.
	const FBasinDef KitchenBasins[] =
	{
		// the worktop with the sink sunk into it: one height field, so the sink fills and slops over its own rim
		{ TEXT("worktop"), -7600.f, 1900.f, 2600.f, 5200.f, 20.f, -5200.f, 3700.f, 1710.f, 70.f, 640.f, 1 },   // v3: west run, the sink, the stove, the east run to the fridge
		// the kitchen floor, through the doorway and a little into the living room
		{ TEXT("floor"), -7050.f, -5150.f, 7700.f, 5000.f, 25.f, 2500.f, 1500.f, 0.f, 45.f, 40.f, 1 },
	};

	// KAN-40: a flat (AntLab) brings its own basins in its data (the worktop with the sink, the floor of the whole flat, the tub);
	// the order is the kitchen's: 0 the worktop with the sink, 1 the floor.
	void BasinDefsNow(TArray<FBasinDef>& Out)
	{
		if (const FAntLabVariant* Lab = AntLab::Get())
		{
			for (const FAntLabBasin& B : Lab->Basins)
			{
				FBasinDef D{ *B.Name, (float)B.Min.X, (float)B.Min.Y, (float)B.Max.X, (float)B.Max.Y, B.Dx, (float)B.Ref.X, (float)B.Ref.Y, B.Z, B.Up, B.Down, B.Sub, B.Fill };
				Out.Add(D);
			}
			return;
		}
		Out.Append(KitchenBasins, UE_ARRAY_COUNT(KitchenBasins));
	}

	bool LogOn()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("AntFluidLog"));
		return b;
	}
}

bool UAntFluidWorld::Wanted()
{
	// v12: the water is part of the game. -AntNoFluid turns it off (A/B, a weak machine).
	static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("AntNoFluid"));
	return b;
}

UAntFluidWorld* UAntFluidWorld::Active(const UObject* Ctx)
{
	UAntFluidWorld* F = Wanted() ? Get(Ctx) : nullptr;
	return F && F->bReady ? F : nullptr;
}

UAntFluidWorld* UAntFluidWorld::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UAntFluidWorld>() : nullptr;
}

bool UAntFluidWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return Wanted() && W && W->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UAntFluidWorld::Tick(float DeltaTime)
{
	UWorld* W = GetWorld();
	if (!W || !Wanted()) { return; }
	if (!bReady)
	{
		// The kitchen is built from code after the map starts: wait until there is a floor to stand on.
		if (W->GetTimeSeconds() < 2.f) { return; }
		FHitResult Hit;
		FVector Probe(2500.f, 1500.f, 0.f);
		{
			TArray<FBasinDef> Defs;
			BasinDefsNow(Defs);
			if (Defs.Num() == 0) { return; }
			if (AntLab::Get())
			{
				// (a flat: the floor is where its data says; a guest knows the variant a moment after the map starts)
				const FBasinDef& Fl = Defs[FMath::Min(1, Defs.Num() - 1)];
				Probe = FVector(Fl.RefX, Fl.RefY, Fl.ZGuess);
			}
		}
		if (!W->LineTraceSingleByObjectType(Hit, Probe + FVector(0.f, 0.f, 400.f), Probe - FVector(0.f, 0.f, 200.f), FCollisionObjectQueryParams(ECC_WorldStatic))) { return; }
		BuildBasins();
		if (W->GetNetMode() != NM_DedicatedServer) { BuildViews(); }
		FParse::Value(FCommandLine::Get(), TEXT("AntFluidDemo="), Demo);
		FParse::Value(FCommandLine::Get(), TEXT("AntFluidQuality="), Quality);
		bFlip = FParse::Param(FCommandLine::Get(), TEXT("AntFluidFlip"));
		FParse::Value(FCommandLine::Get(), TEXT("AntFluidCatchUp="), MaxSteps);   // dev: more steps per frame when a test runs at a low frame rate
		if (Demo == 0 && FCString::Stristr(FCommandLine::Get(), TEXT("-AntFluidScript=")) != nullptr) { Demo = 99; }   // a script with no built-in demo
		bReady = true;
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID ready: %d basins, demo %d, quality %d"), Basins.Num(), Demo, Quality);
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	FrameMs += (DeltaTime * 1000.f - FrameMs) * 0.05f;
	GpuMs += (FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)) - GpuMs) * 0.05f;
	if (!bFrozen) { TickGame(Dt); }
	double T0 = FPlatformTime::Seconds();
	if (!bFrozen) { StepSim(Dt); }
	double T1 = FPlatformTime::Seconds();
	if (!bFrozen)
	{
		const double C0 = FPlatformTime::Seconds();
		CoupleProps(Dt);
		const double C1 = FPlatformTime::Seconds();
		CoupleAnts(Dt);
		const double C2 = FPlatformTime::Seconds();
		CoupleFeet(Dt);
		PropsMs += ((float)(C1 - C0) * 1000.f - PropsMs) * 0.05f;
		AntsMs += ((float)(C2 - C1) * 1000.f - AntsMs) * 0.05f;
	}
	double T2 = FPlatformTime::Seconds();
	if (!bFrozen) { TickDrops(Dt); }
	double T3 = FPlatformTime::Seconds();
	if (W->GetNetMode() != NM_DedicatedServer)
	{
		UpdateViews();
		DrawDrops();
		DrawStreams();
		TickVolumes(Dt);
	}
	double T4 = FPlatformTime::Seconds();
	const float K = 0.05f;
	SimMs += ((float)(T1 - T0) * 1000.f - SimMs) * K;
	CoupleMs += ((float)(T2 - T1) * 1000.f - CoupleMs) * K;
	DropMs += ((float)(T3 - T2) * 1000.f - DropMs) * K;
	ViewMs += ((float)(T4 - T3) * 1000.f - ViewMs) * K;
	if (LogOn() && W->GetTimeSeconds() >= StatAt)
	{
		StatAt = W->GetTimeSeconds() + 5.f;
		LogStats();
	}
	if (Demo > 0) { RunDemo(Demo); }
}

// =============================================================================================
// Layer A: basins
// =============================================================================================

void UAntFluidWorld::BuildBasins()
{
	UWorld* W = GetWorld();
	const double T0 = FPlatformTime::Seconds();
	float Scale = 1.f;
	FParse::Value(FCommandLine::Get(), TEXT("AntFluidCell="), Scale);   // dev: -AntFluidCell=2 doubles every cell size (the resolution test)
	TArray<FBasinDef> BasinDefs;
	BasinDefsNow(BasinDefs);
	for (const FBasinDef& D : BasinDefs)
	{
		// the nominal level: what a trace finds at the reference point
		float Z0 = D.ZGuess;
		FHitResult Hit;
		FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		if (W->LineTraceSingleByObjectType(Hit, FVector(D.RefX, D.RefY, D.ZGuess + 300.f), FVector(D.RefX, D.RefY, D.ZGuess - 300.f), Obj)) { Z0 = (float)Hit.ImpactPoint.Z; }
		const float Dx = D.Dx * Scale;
		TUniquePtr<FBasin> B = MakeUnique<FBasin>();
		B->Init(D.Name, D.X0, D.Y0, FMath::CeilToInt((D.X1 - D.X0) / Dx), FMath::CeilToInt((D.Y1 - D.Y0) / Dx), Dx, Z0);
		B->Gravity = FMath::Abs(W->GetGravityZ());
		B->Dt = StepDt;
		B->SubSteps = D.Sub;
		// A film dries in about 20 s; a flood seeps away (the gaps under the skirting, the joints) over a few minutes.
		B->Params[LWater].Evaporation = 0.03f;
		B->Params[LWater].Soak = Basins.Num() == 0 ? 0.006f : 0.02f;
		B->Params[LOil].Evaporation = 0.0004f;
		const double R0 = FPlatformTime::Seconds();
		Rasterise(*B, D.Up, D.Down);
		B->Finish();
		if (D.Fill > -1.e8f)
		{
			// A basin that starts with water in it (the bath tub of a flat): level D.Fill in everything that lies well below the rim.
			FLayer& Ly = B->Layers[LWater];
			double Vol = 0.;
			for (int32 c = 0; c < B->Kind.Num(); ++c)
			{
				const float Depth = D.Fill - (B->Z0 + B->Bed[c]);
				if (B->Kind[c] == Open && B->Bed[c] < -50.f && Depth > 0.f) { Ly.H[c] = Depth; Vol += (double)Depth * Dx * Dx; }
			}
			Ly.X0 = 0; Ly.Y0 = 0; Ly.X1 = B->Nx; Ly.Y1 = B->Ny;
			Ly.Volume = Vol;
			B->WakeAll();
			B->Params[LWater].Evaporation = 0.f;   // it keeps its water
			B->Params[LWater].Soak = 0.f;
			Filled.Add(Basins.Num(), D.Fill);
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID basin %s starts with %.1f l of water (level %.0f)"), *B->Name, Vol / (1900. * 1900. * 1900. * 1.e-3), D.Fill);
		}
		int32 NWall = 0, NVoid = 0;
		for (uint8 K : B->Kind) { NWall += K == Wall; NVoid += K == Void; }
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID basin %s: %d x %d cells of %.0f uu (%.2f x %.2f m), level %.1f, walls %d, void %d, rasterised in %.0f ms"),
			*B->Name, B->Nx, B->Ny, Dx, B->Nx * Dx / 1900.f, B->Ny * Dx / 1900.f, Z0, NWall, NVoid, (FPlatformTime::Seconds() - R0) * 1000.);
		if (LogOn()) { AsciiMap(*B, -1); }
		Basins.Add(MoveTemp(B));
	}
	Sources.Reset();
	FSource Tap; Tap.Id = TEXT("tap"); Tap.Pos = AKitchenFaucet::Spout(); Tap.Liquid = Water; Tap.Rate = 0.f; Tap.Radius = 30.f;   // TickGame: the faucet fixture opens it
	Sources.Add(Tap);
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID basins built in %.0f ms"), (FPlatformTime::Seconds() - T0) * 1000.);
}

void UAntFluidWorld::Rasterise(FBasin& B, float ProbeUp, float ProbeDown)
{
	UWorld* W = GetWorld();
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_AntClimbable);   // doors and other things that move but are furniture
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AntFluidBed), false);
	for (TActorIterator<ACharacter> It(W); It; ++It) { Q.AddIgnoredActor(*It); }   // the Human, the pets: they are not the bed
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(B.Dx * 0.48f, B.Dx * 0.48f, 0.5f));
	for (int32 j = 0; j < B.Ny; ++j)
	{
		for (int32 i = 0; i < B.Nx; ++i)
		{
			const int32 c = B.Idx(i, j);
			if (i == 0 || j == 0 || i == B.Nx - 1 || j == B.Ny - 1) { B.Kind[c] = Wall; B.Bed[c] = ProbeUp; continue; }
			const FVector2f C = B.CellCentre(i, j);
			FHitResult Hit;
			// Swept from a little above the level: what the box starts inside is a wall (a cabinet, a chair leg), what it lands on is
			// the bed (the floor, a rug, a threshold, the bottom of the sink), and where it lands on nothing liquid falls out of the basin.
			if (W->SweepSingleByObjectType(Hit, FVector(C.X, C.Y, B.Z0 + ProbeUp), FVector(C.X, C.Y, B.Z0 - ProbeDown), FQuat::Identity, Obj, Shape, Q))
			{
				if (Hit.bStartPenetrating || Hit.Time <= 0.f) { B.Kind[c] = Wall; B.Bed[c] = ProbeUp; }
				else { B.Kind[c] = Open; B.Bed[c] = FMath::Min((float)Hit.ImpactPoint.Z - B.Z0, ProbeUp); }
			}
			else { B.Kind[c] = Void; B.Bed[c] = -ProbeDown; }
		}
	}
}

void UAntFluidWorld::AsciiMap(const FBasin& B, int32 Layer) const
{
	// One character per block of cells. Bed: '#' wall, ' ' void, '.' level, digits = how far below the level (tenths of the deepest), '+' above.
	const int32 S = FMath::Max(1, B.Nx / 140);
	float Deep = 1.f;
	for (int32 c = 0; c < B.Nx * B.Ny; ++c) { if (B.Kind[c] == Open) { Deep = FMath::Max(Deep, -B.Bed[c]); } }
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID map of %s (%s), 1 char = %d cells, north is up"), *B.Name, Layer < 0 ? TEXT("bed") : TEXT("liquid"), S);
	for (int32 j = B.Ny - 1; j >= 0; j -= S * 2)
	{
		FString Line;
		for (int32 i = 0; i < B.Nx; i += S)
		{
			const int32 c = B.Idx(i, j);
			TCHAR Ch = TEXT('.');
			if (B.Kind[c] == Wall) { Ch = TEXT('#'); }
			else if (B.Kind[c] == Void) { Ch = TEXT(' '); }
			else if (Layer >= 0)
			{
				const float H = B.Layers[Layer].H[c];
				Ch = H <= 0.f ? TEXT('.') : (H < 1.f ? TEXT(':') : (TCHAR)(TEXT('0') + FMath::Clamp((int32)(H / 5.f), 0, 9)));
			}
			else if (B.Bed[c] > 3.f) { Ch = TEXT('+'); }
			else if (B.Bed[c] < -3.f) { Ch = (TCHAR)(TEXT('0') + FMath::Clamp((int32)(-B.Bed[c] / Deep * 9.99f), 0, 9)); }
			Line.AppendChar(Ch);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID |%s|"), *Line);
	}
}

int32 UAntFluidWorld::BasinUnder(const FVector& P, int32 Skip) const
{
	// the basin whose bed right under the point is the highest one not above it (the sink's bottom lies far below the worktop's level)
	int32 Best = -1;
	float BestZ = -1.e9f;
	for (int32 b = 0; b < Basins.Num(); ++b)
	{
		const FBasin& B = *Basins[b];
		if (b == Skip || !B.Contains((float)P.X, (float)P.Y)) { continue; }
		int32 I, J;
		B.CellAt((float)P.X, (float)P.Y, I, J);
		if (B.Kind[B.Idx(I, J)] != Open) { continue; }
		const float BedZ = B.Z0 + B.Bed[B.Idx(I, J)];
		if (BedZ > P.Z + 5.f) { continue; }
		if (BedZ > BestZ) { Best = b; BestZ = BedZ; }
	}
	return Best;
}

bool UAntFluidWorld::SampleAt(const FVector& P, FSample& Out, int32* OutBasin) const
{
	Out = FSample();
	// the highest basin whose bed is not above the point
	const int32 b = BasinUnder(P + FVector(0.f, 0.f, 30.f));
	if (OutBasin) { *OutBasin = b; }
	if (b < 0) { return false; }
	Basins[b]->Sample((float)P.X, (float)P.Y, Out);
	return Out.bInside;
}

void UAntFluidWorld::Spill(ELiquid Liquid, const FVector& At, float Volume, float Radius, const FVector2f& Vel)
{
	FVector P = At;
	const int32 b = BasinUnderNear(P);
	if (b < 0) { LostVolume += Volume; return; }
	FBasin& B = *Basins[b];
	FSample S;
	B.Sample((float)P.X, (float)P.Y, S);
	const float Drop = FMath::Max(0.f, (float)At.Z - S.Surface);
	if (Drop > 60.f)
	{
		// it falls first
		FFall F; F.From = At; F.To = FVector(P.X, P.Y, S.Surface); F.Dur = FMath::Sqrt(2.f * Drop / B.Gravity); F.Volume = Volume; F.Liquid = Liquid; F.Vel = Vel; F.ToBasin = b;
		Falls.Add(F);
		return;
	}
	LostVolume += AddNear(B, Liquid, (float)P.X, (float)P.Y, Radius, Volume, Vel);
}

void UAntFluidWorld::NoteColumn(const FVector& Top, const FVector& Bottom, float Volume, uint8 Liquid, const FVector2f& Vel)
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (FColumn& C : Columns)
	{
		if (FVector::DistSquared2D(C.Top, Top) < FMath::Square(160.f) && FMath::Abs(C.Top.Z - Top.Z) < 200.f)
		{
			// a running mean of where it pours and how much (per second)
			const float Wt = FMath::Clamp(Volume / FMath::Max(C.Rate * StepDt * 8.f + Volume, 1.f), 0.02f, 1.f);
			C.Top = FMath::Lerp(C.Top, Top, Wt * 0.3f); C.Bottom = FMath::Lerp(C.Bottom, Bottom, Wt * 0.3f);
			C.Vel = FMath::Lerp(C.Vel, Vel, 0.1f);
			C.Rate += Volume / 0.5f;   // decays in TickVolumes with a 0.5 s time constant: Rate is uu^3/s
			C.Seen = Now; C.Liquid = Liquid;
			return;
		}
	}
	FColumn C; C.Top = Top; C.Bottom = Bottom; C.Rate = Volume / 0.5f; C.Seen = Now; C.Liquid = Liquid; C.Vel = Vel;
	Columns.Add(C);
}

int32 UAntFluidWorld::BasinUnderNear(FVector& P, int32 Skip) const
{
	// Straight down first; then a ring round the point (liquid off an edge lands on the floor in front of a cabinet, not inside it).
	const int32 b = BasinUnder(P, Skip);
	if (b >= 0) { return b; }
	for (int32 r = 1; r <= 4; ++r)
	{
		for (int32 k = 0; k < 8; ++k)
		{
			const float Ang = k * 0.785398f;
			const FVector Q = P + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (r * 30.f);
			const int32 bq = BasinUnder(Q, Skip);
			if (bq >= 0) { P = Q; return bq; }
		}
	}
	return -1;
}

float UAntFluidWorld::AddNear(FBasin& T, ELiquid Liquid, float X, float Y, float Radius, float Volume, const FVector2f& Vel)
{
	float Left = T.AddVolume(Liquid, X, Y, Radius, Volume, Vel);
	if (Left <= 0.f) { return 0.f; }
	// it landed on a wall cell (a cabinet's foot, a chair leg): it runs off to the nearest open cell
	int32 CI, CJ;
	if (!T.CellAt(X, Y, CI, CJ)) { return Left; }
	for (int32 r = 1; r <= 10 && Left > 0.f; ++r)
	{
		for (int32 dj = -r; dj <= r && Left > 0.f; ++dj)
		{
			for (int32 di = -r; di <= r && Left > 0.f; ++di)
			{
				if (FMath::Max(FMath::Abs(di), FMath::Abs(dj)) != r) { continue; }
				const int32 i = CI + di, j = CJ + dj;
				if (i < 1 || j < 1 || i >= T.Nx - 1 || j >= T.Ny - 1 || T.Kind[T.Idx(i, j)] != Open) { continue; }
				const FVector2f C = T.CellCentre(i, j);
				Left = T.AddVolume(Liquid, C.X, C.Y, T.Dx * 1.2f, Left, Vel);
			}
		}
	}
	return Left;
}

void UAntFluidWorld::HandleSpills(int32 b)
{
	FBasin& B = *Basins[b];
	for (const FSpill& S : B.Spills)
	{
		SpilledVol += S.Volume;
		const int32 I = S.Cell % B.Nx, J = S.Cell / B.Nx;
		const FVector2f C = B.CellCentre(I, J);
		const ELiquid Liquid = S.Layer == LOil ? Oil : (S.Layer == LHoney ? Honey : (S.Dye > 0.5f ? Cola : Water));
		const FVector From(C.X, C.Y, B.Z0);
		// where it lands: thrown forward by the speed it left the edge with
		FVector2f Vel = S.Vel;
		// where under this edge cell it comes down and how far it falls: looked up once and kept for half a second (an edge spills
		// from the same cells step after step, and the search below is most of what a pour costs)
		const int64 CK = ((int64)b << 40) | (int64)S.Cell;
		FLandCache* LC = LandCache.Find(CK);
		if (!LC || B.StepCount - LC->Step > 30)
		{
			FVector Below(C.X + Vel.X * 0.05f, C.Y + Vel.Y * 0.05f, B.Z0 - 20.f);
			FLandCache N; N.Step = B.StepCount;
			N.To = BasinUnderNear(Below, b);
			if (N.To >= 0)
			{
				FSample Smp;
				Basins[N.To]->Sample((float)Below.X, (float)Below.Y, Smp);
				N.X = (float)Below.X; N.Y = (float)Below.Y; N.Surface = Smp.Surface;
			}
			LC = &LandCache.Add(CK, N);
		}
		const int32 To = LC->To;
		if (To < 0) { LostVolume += S.Volume; continue; }
		FBasin& T = *Basins[To];
		const float Drop = FMath::Max(1.f, B.Z0 - LC->Surface);
		const float Dur = FMath::Sqrt(2.f * Drop / T.Gravity);
		FVector Land(LC->X + Vel.X * Dur, LC->Y + Vel.Y * Dur, LC->Surface);
		{
			int32 TI, TJ;
			if (!T.CellAt((float)Land.X, (float)Land.Y, TI, TJ) || T.Kind[T.Idx(TI, TJ)] != Open) { Land = FVector(LC->X, LC->Y, LC->Surface); }
		}
		FFall F; F.From = From; F.To = Land; F.Dur = Dur; F.Volume = S.Volume; F.Liquid = Liquid; F.Vel = Vel; F.ToBasin = To;
		int32 LI = 0, LJ = 0;
		T.CellAt((float)Land.X, (float)Land.Y, LI, LJ);
		const int64 Key = ((int64)To << 48) | ((int64)(LJ / 2 + 1) << 24) | (int64)(LI / 2 + 1) | ((int64)Liquid << 56);
		// what leaves an edge over a few steps towards the same spot falls as one lump (thousands of tiny falls cost more than they show)
		if (FFall* Ex = Pending.Find(Key)) { Ex->Volume += S.Volume; }
		else { Pending.Add(Key, F); }
	}
	B.Spills.Reset();
}

void UAntFluidWorld::FlushPending()
{
	for (TPair<int64, FFall>& P : Pending)
	{
		Falls.Add(P.Value);
		NoteColumn(P.Value.From, P.Value.To, P.Value.Volume, P.Value.Liquid, P.Value.Vel);
	}
	Pending.Reset();
}

void UAntFluidWorld::StepSim(float Dt)
{
	Accum += Dt;
	int32 Steps = 0;
	while (Accum >= StepDt && Steps < MaxSteps)
	{
		Accum -= StepDt; ++Steps;
		// sources
		for (FSource& S : Sources)
		{
			if (!S.bOn) { continue; }
			float Vol = S.Rate * StepDt;
			if (S.Left >= 0.f) { Vol = FMath::Min(Vol, S.Left); S.Left -= Vol; if (S.Left <= 0.f) { S.bOn = false; } }
			const int32 b = BasinUnder(S.Pos);
			if (b < 0) { LostVolume += Vol; continue; }
			FBasin& B = *Basins[b];
			FSample Smp;
			B.Sample((float)S.Pos.X, (float)S.Pos.Y, Smp);
			LostVolume += AddNear(B, (ELiquid)S.Liquid, (float)S.Pos.X, (float)S.Pos.Y, S.Radius, Vol);
			// A stream that falls on a surface leaves it sideways: the thin fast sheet round the impact (and the hydraulic jump round that) comes from this.
			const float Fall = FMath::Max(0.f, (float)S.Pos.Z - Smp.Surface);
			const float Impact = FMath::Sqrt(2.f * B.Gravity * Fall);
			if (S.Liquid != Honey) { B.Kick((float)S.Pos.X, (float)S.Pos.Y, S.Radius * 2.2f, FMath::Min(Impact * 0.45f, 0.4f * B.Dx / (B.Dt / B.SubSteps)), 0.5f); }
			if (S.Liquid == Water || S.Liquid == Cola) { B.AddFoam((float)S.Pos.X, (float)S.Pos.Y, S.Radius * 1.5f, FMath::Min(0.06f, Impact / 20000.f)); }
			NoteColumn(S.Pos, FVector(S.Pos.X, S.Pos.Y, Smp.Surface), Vol, S.Liquid, FVector2f::ZeroVector);
		}
		// what is in the air lands
		for (int32 i = Falls.Num() - 1; i >= 0; --i)
		{
			FFall& F = Falls[i];
			F.T += StepDt;
			if (F.T < F.Dur) { continue; }
			if (Basins.IsValidIndex(F.ToBasin))
			{
				FBasin& T = *Basins[F.ToBasin];
				LostVolume += AddNear(T, (ELiquid)F.Liquid, (float)F.To.X, (float)F.To.Y, T.Dx * 1.3f, F.Volume, F.Vel * 0.5f);
				if (F.Liquid == Water || F.Liquid == Cola) { T.AddFoam((float)F.To.X, (float)F.To.Y, T.Dx * 1.5f, FMath::Min(0.08f, F.Volume / (T.Dx * T.Dx * 40.f))); }
			}
			Falls.RemoveAtSwap(i, EAllowShrinking::No);
		}
		++SimStep;
		for (int32 b = 0; b < Basins.Num(); ++b)
		{
			// A basin where most of the water is on the move (a flood across the whole floor) steps at 30 Hz with twice the step:
			// still stable (waves a few cm deep cross a cell in several steps), half the cost. Decided by its own state: the same on every machine.
			FBasin& Bb = *Basins[b];
			const bool bHalf = bAllowHalf && Bb.RunTiles[LWater] > 260;
			if (bHalf && (SimStep & 1)) { continue; }
			Bb.Dt = bHalf ? 2.f * StepDt : StepDt;
			Bb.Step();
			HandleSpills(b);
		}
		if (++PendingSteps >= 4) { PendingSteps = 0; FlushPending(); }
	}
	if (Steps == MaxSteps) { Accum = FMath::Min(Accum, (double)StepDt); }   // the frame rate fell under 20: the liquid slows down rather than the game
}

// =============================================================================================
// Layer B (1): the surface. A flat grid per basin and layer; the material lifts every vertex to the simulated surface and
// clips what is dry, reading two float textures this code fills from the simulation.
// =============================================================================================

void UAntFluidWorld::BuildViews()
{
	UWorld* W = GetWorld();
	LiquidMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fluid/M_AntLiquid.M_AntLiquid"));
	DropMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fluid/M_AntDrop.M_AntDrop"));
	if (!LiquidMat) { UE_LOG(LogTemp, Warning, TEXT("ANTFLUID no /Game/Fluid/M_AntLiquid: run Tools/make_fluid_assets.py. The liquid will not be drawn.")); }
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	Holder = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	Views.SetNum(Basins.Num() * LCount);
	for (int32 b = 0; b < Basins.Num(); ++b)
	{
		const FBasin& B = *Basins[b];
		for (int32 L = 0; L < LCount; ++L)
		{
			FAntFluidLayerView& V = Views[b * LCount + L];
			auto MakeTex = [&B]() -> UTexture2D*
			{
				UTexture2D* T = UTexture2D::CreateTransient(B.Nx, B.Ny, PF_A32B32G32R32F);
				T->SRGB = false; T->Filter = TF_Bilinear; T->AddressX = TA_Clamp; T->AddressY = TA_Clamp; T->NeverStream = true;
				if (FTexturePlatformData* PD = T->GetPlatformData())
				{
					void* Data = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
					FMemory::Memzero(Data, (SIZE_T)B.Nx * B.Ny * 16);
					PD->Mips[0].BulkData.Unlock();
				}
				T->UpdateResource();
				return T;
			};
			V.TexA = MakeTex();
			V.TexB = MakeTex();
			if (LiquidMat)
			{
				V.MID = UMaterialInstanceDynamic::Create(LiquidMat, Holder);
				V.MID->SetTextureParameterValue(TEXT("TexA"), V.TexA);
				V.MID->SetTextureParameterValue(TEXT("TexB"), V.TexB);
				V.MID->SetVectorParameterValue(TEXT("Org"), FLinearColor(B.OriginX, B.OriginY, 1.f / (B.Nx * B.Dx), 1.f / (B.Ny * B.Dx)));
				V.MID->SetVectorParameterValue(TEXT("Size"), FLinearColor((float)B.Nx, (float)B.Ny, B.Dx, 0.f));
				// the surface sits a hair above the bed so that a film does not fight the floor for the same pixels; upper layers a hair more
				V.MID->SetVectorParameterValue(TEXT("Par"), FLinearColor(B.Z0, B.Params[L].Contact, 0.25f + 0.2f * L, 0.f));
				if (L == LWater)
				{
					// Clear water. Real water takes ~0.45/m of red: 2.4e-4 per uu; a few times that, so a 3 cm flood reads a little
					// blue-green at a grazing look and is glass from above. Cola absorbs hard, blue most: dark brown, amber at the rim.
					V.MID->SetVectorParameterValue(TEXT("Absorb"), FLinearColor(0.0016f, 0.0006f, 0.00045f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("AbsorbDye"), FLinearColor(0.012f, 0.032f, 0.075f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Scatter"), FLinearColor(0.00012f, 0.00018f, 0.0002f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("ScatterDye"), FLinearColor(0.004f, 0.0018f, 0.0006f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Rip"), FLinearColor(0.006f, 0.16f, 6.f, 0.f));
					V.MID->SetScalarParameterValue(TEXT("Roughness"), 0.02f);
				}
				else if (L == LHoney)
				{
					V.MID->SetVectorParameterValue(TEXT("Absorb"), FLinearColor(0.012f, 0.05f, 0.33f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("AbsorbDye"), FLinearColor(0.012f, 0.05f, 0.33f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Scatter"), FLinearColor(0.016f, 0.009f, 0.0015f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("ScatterDye"), FLinearColor(0.016f, 0.009f, 0.0015f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Rip"), FLinearColor(0.f, 0.f, 9.f, 0.f));
					V.MID->SetScalarParameterValue(TEXT("Roughness"), 0.06f);
				}
				else if (L == LOil)
				{
					// (KAN-21 fire pass 4: without scattering a deep puddle had no colour of its own: where the floor under it was
					//  seen far through (the tiles) it went black, over the runner it was gold - one puddle in two halves with a
					//  seam at the runner's edge. Oil scatters amber: deep or thin, it is the same golden oil.)
					//  Dense enough that a few uu of it already shows its own colour, not the floor: over a raised board, the tiles
					//  or the runner it reads as one puddle; only the thin rim lets the floor through. A little rough: the fire's
					//  light is a soft glint in it, not a white disc.
					V.MID->SetVectorParameterValue(TEXT("Absorb"), FLinearColor(0.05f, 0.1f, 0.45f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("AbsorbDye"), FLinearColor(0.05f, 0.1f, 0.45f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Scatter"), FLinearColor(0.16f, 0.09f, 0.02f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("ScatterDye"), FLinearColor(0.16f, 0.09f, 0.02f, 0.f));
					V.MID->SetVectorParameterValue(TEXT("Rip"), FLinearColor(0.f, 0.02f, 9.f, 0.f));
					V.MID->SetScalarParameterValue(TEXT("Roughness"), 0.07f);
					V.MID->SetScalarParameterValue(TEXT("Irid"), 1.f);
					V.MID->SetScalarParameterValue(TEXT("MaskMin"), 0.01f);
				}
			}
			UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(Holder);
			PM->SetupAttachment(Root);
			PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PM->SetCastShadow(false);
			PM->bUseAsyncCooking = false;
			PM->SetRelativeLocation(FVector(0.f, 0.f, B.Z0));
			PM->RegisterComponent();
			V.Mesh = PM;
			V.TilesX = (B.Nx + Tile - 1) / Tile; V.TilesY = (B.Ny + Tile - 1) / Tile;
			V.TileShown.Init(0, V.TilesX * V.TilesY);
			float Deep = 10.f;
			for (int32 c = 0; c < B.Nx * B.Ny; ++c) { Deep = FMath::Max(Deep, -B.Bed[c]); }
			for (int32 ty = 0; ty < V.TilesY; ++ty)
			{
				for (int32 tx = 0; tx < V.TilesX; ++tx)
				{
					const int32 I0 = tx * Tile, J0 = ty * Tile, I1 = FMath::Min(I0 + Tile, B.Nx), J1 = FMath::Min(J0 + Tile, B.Ny);
					const int32 VW = I1 - I0 + 1, VH = J1 - J0 + 1;
					TArray<FVector> Vs; TArray<int32> Tris; TArray<FVector> Ns; TArray<FVector2D> UVs; TArray<FLinearColor> Cols; TArray<FProcMeshTangent> Tans;
					Vs.Reserve(VW * VH + 3); Tris.Reserve((VW - 1) * (VH - 1) * 6 + 3);
					for (int32 j = 0; j < VH; ++j)
					{
						for (int32 i = 0; i < VW; ++i)
						{
							Vs.Add(FVector(B.OriginX + (I0 + i) * B.Dx, B.OriginY + (J0 + j) * B.Dx, 0.f));
							UVs.Add(FVector2D((double)(I0 + i) / B.Nx, (double)(J0 + j) / B.Ny));
						}
					}
					for (int32 j = 0; j < VH - 1; ++j)
					{
						for (int32 i = 0; i < VW - 1; ++i)
						{
							const int32 a = j * VW + i;
							Tris.Add(a); Tris.Add(a + VW); Tris.Add(a + 1);
							Tris.Add(a + 1); Tris.Add(a + VW); Tris.Add(a + VW + 1);
						}
					}
					// The material moves the vertices; three more (a triangle without area) stretch the bounds over the whole range they can move in.
					const int32 X = Vs.Num();
					Vs.Add(FVector(B.OriginX + I0 * B.Dx, B.OriginY + J0 * B.Dx, -Deep - 20.f));
					Vs.Add(FVector(B.OriginX + I0 * B.Dx, B.OriginY + J0 * B.Dx, 400.f));
					Vs.Add(FVector(B.OriginX + I0 * B.Dx, B.OriginY + J0 * B.Dx, 400.f));
					UVs.Add(FVector2D::ZeroVector); UVs.Add(FVector2D::ZeroVector); UVs.Add(FVector2D::ZeroVector);
					Tris.Add(X); Tris.Add(X + 1); Tris.Add(X + 2);
					Ns.Init(FVector::UpVector, Vs.Num());
					Tans.Init(FProcMeshTangent(1.f, 0.f, 0.f), Vs.Num());
					const int32 Sec = ty * V.TilesX + tx;
					PM->CreateMeshSection_LinearColor(Sec, Vs, Tris, Ns, UVs, Cols, Tans, false);
					if (V.MID) { PM->SetMaterial(Sec, V.MID); }
					PM->SetMeshSectionVisible(Sec, false);
				}
			}
			PM->SetVisibility(false);
		}
	}
	// drops: instanced spheres
	DropMesh = NewObject<UInstancedStaticMeshComponent>(Holder);
	DropMesh->SetupAttachment(Root);
	DropMesh->SetStaticMesh(Map4Look::Enabled() && Map4Look::Shape(5) ? Map4Look::Shape(5) : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (DropMat) { DropMesh->SetMaterial(0, DropMat); }
	DropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropMesh->SetCastShadow(false);
	DropMesh->NumCustomDataFloats = 1;   // 0 water .. 1 cola (the drop material tints by it)
	DropMesh->RegisterComponent();
	StreamMesh = NewObject<UInstancedStaticMeshComponent>(Holder);
	StreamMesh->SetupAttachment(Root);
	StreamMesh->SetStaticMesh(Map4Look::Enabled() && Map4Look::Shape(2) ? Map4Look::Shape(2) : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	if (UMaterialInterface* SM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fluid/M_AntStream.M_AntStream"))) { StreamMesh->SetMaterial(0, SM); }
	else if (DropMat) { StreamMesh->SetMaterial(0, DropMat); }
	StreamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StreamMesh->SetCastShadow(false);
	StreamMesh->NumCustomDataFloats = 1;
	StreamMesh->RegisterComponent();
	// cosmetic droplets: the broken lower part of every stream and the crown where it lands (drawn, never simulated)
	SprayMesh = NewObject<UInstancedStaticMeshComponent>(Holder);
	SprayMesh->SetupAttachment(Root);
	SprayMesh->SetStaticMesh(Map4Look::Enabled() && Map4Look::Shape(5) ? Map4Look::Shape(5) : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (DropMat) { SprayMesh->SetMaterial(0, DropMat); }
	SprayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SprayMesh->SetCastShadow(false);
	SprayMesh->NumCustomDataFloats = 1;
	SprayMesh->RegisterComponent();

	FlipHose = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Fluid/NS_FlipHose.NS_FlipHose"));
	FlipPool = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Fluid/NS_FlipPool.NS_FlipPool"));
	FlipSplash = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Fluid/NS_FlipSplash.NS_FlipSplash"));
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID views built: material %s, drop material %s, FLIP hose %s pool %s splash %s"), LiquidMat ? TEXT("ok") : TEXT("MISSING"), DropMat ? TEXT("ok") : TEXT("MISSING"),
		FlipHose ? TEXT("ok") : TEXT("MISSING"), FlipPool ? TEXT("ok") : TEXT("MISSING"), FlipSplash ? TEXT("ok") : TEXT("MISSING"));
}

void UAntFluidWorld::UpdateViews()
{
	if (!Holder) { return; }
	UploadTiles = 0;
	// caustics follow the light: bright by day, faint under the evening lamps, a trace at night (only the water layer has them)
	const int32 Tod = LevelV3::Enabled() ? LevelV3::TimeOfDay() : 0;
	if (Tod != CausticTod)
	{
		CausticTod = Tod;
		const float K = Tod == 0 ? 1.f : (Tod == 1 ? 0.45f : 0.12f);
		for (int32 i = 0; i < Views.Num(); ++i)
		{
			if (Views[i].MID) { Views[i].MID->SetVectorParameterValue(TEXT("Caustic"), (i % LCount) == LWater ? FLinearColor(0.09f * K, 0.11f * K, 0.12f * K, 0.f) : FLinearColor::Black); }
		}
	}
	for (int32 b = 0; b < Basins.Num(); ++b)
	{
		// Only the tiles the simulation changed are written again (still water costs no upload); a layer's picture also depends on
		// the layers under and over it, so the union of all three.
		FBasin& B = *Basins[b];
		// a whole room of moving water: its picture is written every other frame (the flow is smooth enough at 30 Hz to the eye)
		int32 NDirty = 0;
		for (int32 L = 0; L < LCount; ++L) { for (uint8 D : B.Layers[L].Dirty) { NDirty += D; } }
		if (NDirty > 160 && ((GFrameCounter + b) & 1)) { continue; }
		ViewDirty.Init(0, B.TX * B.TY);
		for (int32 L = 0; L < LCount; ++L)
		{
			uint8* D = B.Layers[L].Dirty.GetData();
			for (int32 t = 0; t < B.TX * B.TY; ++t) { ViewDirty[t] |= D[t]; D[t] = 0; }
		}
		for (int32 L = 0; L < LCount; ++L) { UpdateLayerView(b, L); }
	}
}

void UAntFluidWorld::UpdateLayerView(int32 b, int32 L)
{
	const FBasin& B = *Basins[b];
	const FLayer& Ly = B.Layers[L];
	FAntFluidLayerView& V = Views[b * LCount + L];
	if (!V.Mesh || !V.TexA) { return; }
	const bool bAny = Ly.Any() && Quality >= 0;
	if (!bAny && !V.bShown) { return; }
	// the cells to write: where the liquid is (and a rim for the banks it leans on), and whatever was written last time
	FIntRect Now(0, 0, 0, 0);
	if (bAny) { Now = FIntRect(FMath::Max(Ly.X0 - 3, 0), FMath::Max(Ly.Y0 - 3, 0), FMath::Min(Ly.X1 + 3, B.Nx), FMath::Min(Ly.Y1 + 3, B.Ny)); }
	FIntRect R = Now;
	if (V.Uploaded.Area() > 0) { if (R.Area() > 0) { R.Union(V.Uploaded); } else { R = V.Uploaded; } }
	if (R.Area() <= 0) { V.bShown = false; V.Mesh->SetVisibility(false); return; }
	const int32 RW = R.Width(), RH = R.Height();
	float* A = (float*)FMemory::Malloc((SIZE_T)RW * RH * 16);
	float* Bx = (float*)FMemory::Malloc((SIZE_T)RW * RH * 16);
	const int32 UW = B.Nx + 1, Nx = B.Nx, Ny = B.Ny;
	const FLayer& Honey = B.Layers[LHoney]; const FLayer& Water = B.Layers[LWater]; const FLayer& Oil = B.Layers[LOil];
	// the bed this layer lies on and its own depth, as this layer is drawn
	auto BedOf = [&](int32 c) -> float { float Z = B.Bed[c]; if (L > LHoney) { Z += Honey.H[c]; } if (L > LWater) { Z += Water.H[c]; } return Z; };
	auto DepthOf = [&](int32 c) -> float
	{
		float H = Ly.H[c];
		if (L == LOil && Water.H[c] > 0.3f) { H = 0.f; }   // oil lying on water is drawn by the water's own surface (one refracting layer per pixel)
		return H;
	};
	// the regions to write: changed tiles (with a one-cell rim: a dry cell's picture depends on its neighbours), merged along a row
	TArray<FIntRect> Regs;
	{
		const int32 TS = 1 << FBasin::TileShift;
		const int32 T0 = R.Min.X / TS, T1 = (R.Max.X - 1) / TS, S0 = R.Min.Y / TS, S1 = (R.Max.Y - 1) / TS;
		for (int32 ty = S0; ty <= S1; ++ty)
		{
			int32 tx = T0;
			while (tx <= T1)
			{
				if (!ViewDirty.IsValidIndex(ty * B.TX + tx) || !ViewDirty[ty * B.TX + tx]) { ++tx; continue; }
				int32 e = tx;
				while (e + 1 <= T1 && ViewDirty[ty * B.TX + e + 1]) { ++e; }
				FIntRect Q(tx * TS - 1, ty * TS - 1, (e + 1) * TS + 1, (ty + 1) * TS + 1);
				Q.Clip(R);
				if (Q.Area() > 0) { Regs.Add(Q); UploadTiles += e - tx + 1; }
				tx = e + 1;
			}
		}
	}
	if (Regs.Num() == 0) { FMemory::Free(A); FMemory::Free(Bx); }
	else
	{
	ParallelFor(Regs.Num() * 2, [&](int32 RegHalf)
	{
		const FIntRect& Q = Regs[RegHalf >> 1];
		const int32 JMid = (Q.Min.Y + Q.Max.Y) / 2;
		const int32 JA = (RegHalf & 1) ? JMid : Q.Min.Y, JB = (RegHalf & 1) ? Q.Max.Y : JMid;
		for (int32 j = JA; j < JB; ++j)
		for (int32 i = Q.Min.X; i < Q.Max.X; ++i)
		{
			const int32 Row = j - R.Min.Y;
			const int32 c = j * Nx + i;
			float* Pa = A + ((SIZE_T)Row * RW + (i - R.Min.X)) * 4;
			float* Pb = Bx + ((SIZE_T)Row * RW + (i - R.Min.X)) * 4;
			const float Bed = BedOf(c);
			float H = DepthOf(c);
			float S = Bed + H, E = H;
			if (H <= 0.f)
			{
				// A dry cell that stands higher than the liquid next to it is a bank (a wall, the side of the sink): the surface runs on
				// level into it and is hidden inside it. A dry cell at the level of the liquid is a free edge: the rim of the puddle.
				float BestS = -1.e9f, BestE = 0.f; bool bLow = false; float LowS = 0.f;
				for (int32 dj = -1; dj <= 1; ++dj)
				{
					for (int32 di = -1; di <= 1; ++di)
					{
						const int32 ni = i + di, nj = j + dj;
						if ((di == 0 && dj == 0) || ni < 0 || nj < 0 || ni >= Nx || nj >= Ny) { continue; }
						const int32 n = nj * Nx + ni;
						const float Hn = DepthOf(n);
						if (Hn <= 0.f) { continue; }
						const float Sn = BedOf(n) + Hn;
						if (B.Kind[c] == Wall || Bed >= Sn - 0.05f) { if (Sn > BestS) { BestS = Sn; BestE = Hn; } }
						else if (B.Kind[c] == Void || Bed < BedOf(n) - 2.f * B.Dx) { bLow = true; LowS = Sn; }
					}
				}
				if (BestS > -1.e8f) { S = BestS; E = BestE; }
				else if (bLow) { S = LowS - 0.5f * B.Dx; E = 0.f; }   // past a brink the sheet just ends at the edge; the fall is drawn by the streams
				else if (B.Kind[c] == Wall || B.Kind[c] == Void) { S = 0.f; E = 0.f; }
			}
			Pa[0] = S; Pa[1] = E;
			Pa[2] = 0.5f * (Ly.U[j * UW + i] + Ly.U[j * UW + i + 1]);
			Pa[3] = 0.5f * (Ly.V[j * Nx + i] + Ly.V[(j + 1) * Nx + i]);
			Pb[0] = L == LWater ? Ly.Foam[c] : 0.f;
			Pb[1] = L == LWater ? Ly.Dye[c] : 0.f;
			Pb[2] = L == LWater ? Oil.H[c] : 0.f;
			Pb[3] = 0.f;
		}
	});
	auto Upload = [&R, &Regs, RW](UTexture2D* T, float* Data)
	{
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D[Regs.Num()];
		for (int32 r = 0; r < Regs.Num(); ++r)
		{
			const FIntRect& Q = Regs[r];
			Region[r] = FUpdateTextureRegion2D(Q.Min.X, Q.Min.Y, Q.Min.X - R.Min.X, Q.Min.Y - R.Min.Y, Q.Width(), Q.Height());
		}
		T->UpdateTextureRegions(0, (uint32)Regs.Num(), Region, RW * 16, 16, (uint8*)Data, [](uint8* Src, const FUpdateTextureRegion2D* Regions) { FMemory::Free(Src); delete[] Regions; });
	};
	Upload(V.TexA, A);
	Upload(V.TexB, Bx);
	}
	V.Uploaded = Now;
	// show the tiles the liquid is on
	for (int32 ty = 0; ty < V.TilesY; ++ty)
	{
		for (int32 tx = 0; tx < V.TilesX; ++tx)
		{
			const bool bOn = bAny && tx * Tile < Now.Max.X && (tx + 1) * Tile > Now.Min.X && ty * Tile < Now.Max.Y && (ty + 1) * Tile > Now.Min.Y;
			uint8& Was = V.TileShown[ty * V.TilesX + tx];
			if ((Was != 0) != bOn) { Was = bOn ? 1 : 0; V.Mesh->SetMeshSectionVisible(ty * V.TilesX + tx, bOn); }
		}
	}
	if (V.bShown != bAny) { V.bShown = bAny; V.Mesh->SetVisibility(bAny); }
}

// =============================================================================================
// Coupling: physics props, ants, the Human's feet
// =============================================================================================

namespace
{
	/** Water's density over the thing's: > 1 floats. */
	float FloatOf(const AActor* A)
	{
		if (A->ActorHasTag(TEXT("AntSponge"))) { return 3.2f; }
		if (A->ActorHasTag(TEXT("AntSinks"))) { return 0.45f; }
		if (A->ActorHasTag(TEXT("AntFloat"))) { return 1.6f; }
		if (const AKitchenLoot* L = Cast<AKitchenLoot>(A))
		{
			// by what it is: sugar, cheese, grapes, coins, jam go down; crumbs, bread, cereal, chips, crackers ride on top
			// the material table (AntMaterials.cpp, Config/AntMaterials.json): density decides
			return AntMaterials::FloatOf(L->LootName);
		}
		return 1.2f;
	}
}

void UAntFluidWorld::CoupleProps(float Dt)
{
	UWorld* W = GetWorld();
	if (W->GetNetMode() == NM_Client) { return; }
	static TArray<TWeakObjectPtr<AActor>> Bodies;
	static float NextScan = 0.f;
	static TWeakObjectPtr<UWorld> ScanWorld;
	if (ScanWorld.Get() != W || W->GetTimeSeconds() >= NextScan)
	{
		ScanWorld = W; NextScan = W->GetTimeSeconds() + 0.5f;
		Bodies.Reset();
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			const UPrimitiveComponent* P = Cast<UPrimitiveComponent>(It->GetRootComponent());
			if (P && P->IsSimulatingPhysics() && !It->IsA<APawn>()) { Bodies.Add(*It); }
		}
	}
	// the world rectangles that hold any liquid: most bodies are nowhere near it and are skipped at once
	TArray<FBox2f, TInlineAllocator<4>> Wet;
	for (const TUniquePtr<FBasin>& Bp : Basins)
	{
		for (int32 L = 0; L < LCount; ++L)
		{
			const FLayer& Ly = Bp->Layers[L];
			if (!Ly.Any()) { continue; }
			Wet.Add(FBox2f(FVector2f(Bp->OriginX + (Ly.X0 - 2) * Bp->Dx, Bp->OriginY + (Ly.Y0 - 2) * Bp->Dx), FVector2f(Bp->OriginX + (Ly.X1 + 2) * Bp->Dx, Bp->OriginY + (Ly.Y1 + 2) * Bp->Dx)));
		}
	}
	// Half the bodies a frame, each with twice the force: the same push on average at half the cost (it is a physics force
	// integrated over the frame, and 30 Hz is far faster than anything floating in a kitchen flood responds).
	const float Rate = 2.f;
	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		if (((uint32)BodyIdx + (uint32)GFrameCounter) & 1u) { continue; }
		const TWeakObjectPtr<AActor>& Wk = Bodies[BodyIdx];
		AActor* A = Wk.Get();
		UPrimitiveComponent* Body = A ? Cast<UPrimitiveComponent>(A->GetRootComponent()) : nullptr;
		if (!Body || !Body->IsSimulatingPhysics()) { continue; }
		{
			const FVector2f P2((float)Body->GetComponentLocation().X, (float)Body->GetComponentLocation().Y);
			bool bNear = false;
			for (const FBox2f& Bx : Wet) { if (Bx.IsInside(P2)) { bNear = true; break; } }
			if (!bNear && !Oiled.Contains(A)) { WasIn.Remove(A); continue; }
		}
		// A load the ants carry is not lifted (it would tear out of their mandibles) but the current pulls at it: a strong team holds
		// it, a weak one is dragged downstream with it (the carry solver answers this force like any other).
		const AKitchenLoot* Loot = Cast<AKitchenLoot>(A);
		const bool bCarried = Loot && Loot->IsCarried();
		const FVector P = Body->GetComponentLocation();
		const FBoxSphereBounds Bd = Body->Bounds;
		const float R = FMath::Max(3.f, (float)Bd.BoxExtent.Z);
		const float RXY = FMath::Max(3.f, 0.5f * (float)(Bd.BoxExtent.X + Bd.BoxExtent.Y));
		FSample S; int32 b = -1;
		const bool bAny = SampleAt(P, S, &b);
		{
			// an oil film under it: it slides (a physical material with almost no friction while it lies on the film)
			const bool bOil = bAny && S.Depth[LOil] > 0.03f && S.Depth[LWater] < 1.f && (float)Bd.Origin.Z - (float)Bd.BoxExtent.Z < S.Surface + 6.f;
			const bool bWas = Oiled.Contains(A);
			if (bOil != bWas)
			{
				if (bOil)
				{
					if (!OilMat) { OilMat = NewObject<UPhysicalMaterial>(this); OilMat->Friction = 0.02f; OilMat->StaticFriction = 0.02f; OilMat->FrictionCombineMode = EFrictionCombineMode::Min; OilMat->bOverrideFrictionCombineMode = true; }
					Oiled.Add(A, Body->BodyInstance.GetSimplePhysicalMaterial());
					Body->SetPhysMaterialOverride(OilMat);
				}
				else { Body->SetPhysMaterialOverride(nullptr); Oiled.Remove(A); }
				if (LogOn()) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID %s %s an oil film at %s"), *A->GetName(), bOil ? TEXT("slides onto") : TEXT("leaves"), *P.ToCompactString()); }
			}
		}
		if (!bAny || S.Total < 0.3f) { WasIn.Remove(A); continue; }
		const float Bottom = (float)Bd.Origin.Z - R;
		const float Sub = FMath::Clamp((S.Surface - Bottom) / (2.f * R), 0.f, 1.f);
		if (Sub <= 0.f) { WasIn.Remove(A); continue; }
		FBasin& B = *Basins[b];
		const float Mass = Body->GetMass();
		const FVector V = Body->GetPhysicsLinearVelocity();
		const float Thick = S.Depth[LHoney] > 1.f ? 9.f : 1.f;   // honey holds what it touches
		// buoyancy at three points round the body: where the surface is higher that side is lifted, so it tips and rights itself
		const float Lift = Mass * B.Gravity * FloatOf(A);
		for (int32 k = 0; k < (bCarried ? 0 : 3); ++k)
		{
			const float Ang = k * 2.0944f;
			const FVector Off(FMath::Cos(Ang) * RXY * 0.6f, FMath::Sin(Ang) * RXY * 0.6f, 0.f);
			const FVector Q = Body->GetComponentTransform().TransformPositionNoScale(Body->GetComponentTransform().InverseTransformVectorNoScale(Off));
			FSample Sq;
			B.Sample((float)Q.X, (float)Q.Y, Sq);
			const float SubQ = FMath::Clamp((Sq.Surface - ((float)Q.Z - R)) / (2.f * R), 0.f, 1.f);
			Body->AddForceAtLocation(FVector(0.f, 0.f, Lift * SubQ / 3.f) * Rate, Q);
		}
		// the current carries it; the liquid brakes what moves through it
		const FVector Rel = FVector(S.Vel.X, S.Vel.Y, 0.f) - V;
		Body->AddForce((FVector(Rel.X, Rel.Y, 0.f) * Mass * 3.5f * Sub * Thick + FVector(0.f, 0.f, bCarried ? 0.f : -V.Z * Mass * 4.f * Sub * Thick)) * Rate);
		Body->AddTorqueInRadians(-Body->GetPhysicsAngularVelocityInRadians() * Mass * R * R * 1.5f * Sub * Thick * Rate);
		Body->WakeAllRigidBodies();
		// and it pushes the liquid back
		B.Push((float)P.X, (float)P.Y, RXY, FVector2f((float)V.X, (float)V.Y), FMath::Min(0.5f, 3.f * Dt) * Sub);
		float* Was = WasIn.Find(A);
		if (!Was)
		{
			WasIn.Add(A, W->GetTimeSeconds());
			if (V.Z < -120.f)
			{
				// it fell in: a ring of waves, white water, a burst
				const float Hit = FMath::Min(FMath::Abs((float)V.Z) * 0.5f, 0.4f * B.Dx / (B.Dt / B.SubSteps));
				B.Kick((float)P.X, (float)P.Y, RXY * 2.5f, Hit, 0.8f);
				B.AddFoam((float)P.X, (float)P.Y, RXY * 2.f, 0.6f);
				if (W->GetNetMode() != NM_DedicatedServer) { AntFx::Splash(this, FVector(P.X, P.Y, S.Surface), FMath::Clamp(RXY * 0.5f, 6.f, 60.f)); }
				if (LogOn()) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID %s fell into %s at %s, speed %.0f, depth %.1f"), *A->GetName(), *B.Name, *P.ToCompactString(), V.Z, S.Total); }
				if (RXY > 60.f && Quality >= 2)
				{
					const FVector C(P.X, P.Y, S.Surface + 120.f);
					AddVolume(FName(*FString::Printf(TEXT("splash%d"), A->GetUniqueID())), 2, FBox(C - FVector(380.f, 380.f, 200.f), C + FVector(380.f, 380.f, 300.f)), Quality >= 3 ? 96 : 64, 3.5f);
				}
			}
		}
	}
}

void UAntFluidWorld::CoupleAnts(float Dt)
{
	UWorld* W = GetWorld();
	if (W->GetNetMode() == NM_Client) { return; }
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant->bDead) { continue; }
		UCharacterMovementComponent* CM = Ant->GetCharacterMovement();
		const float R = Ant->GetSimpleCollisionRadius();
		const FVector P = Ant->GetActorLocation();
		FSample S; int32 b = -1;
		const bool bIn = SampleAt(P, S, &b) && S.Total > 0.4f && P.Z - R < S.Surface + 2.f;
		const bool bSwim = bIn && (S.Depth[LWater] > R * 1.1f);
		const bool bOil = (bIn && S.Depth[LOil] > 0.03f && S.Depth[LWater] < 1.f) || AntMatter::IceAt(W, P - FVector(0.f, 0.f, R), 25.f);   // oil or ice: no grip
		if (bOil && !AntGrip.Contains(Ant)) { AntGrip.Add(Ant, FVector2f(CM->GroundFriction, CM->BrakingDecelerationWalking)); CM->GroundFriction = 0.15f; CM->BrakingDecelerationWalking = 20.f; }
		else if (!bOil && AntGrip.Contains(Ant)) { const FVector2f G0 = AntGrip.FindAndRemoveChecked(Ant); CM->GroundFriction = G0.X; CM->BrakingDecelerationWalking = G0.Y; }
		// honey holds: an ant in it barely moves
		if (bIn && S.Depth[LHoney] > 2.f && !bSwim) { CM->Velocity *= FMath::Max(0.f, 1.f - 12.f * Dt); }
		static TSet<TWeakObjectPtr<AAntCharacter>> Swimmers;
		if (bSwim)
		{
			// An ant is light and does not break the surface: it rides on it and goes where the water goes, paddling a little.
			if (!Swimmers.Contains(Ant)) { Swimmers.Add(Ant); CM->SetMovementMode(MOVE_Flying); }
			if (CM->MovementMode != MOVE_Flying) { CM->SetMovementMode(MOVE_Flying); }
			const float Want = S.Surface + R * 0.35f;
			FVector V = CM->Velocity;
			const FVector2f Cur = S.Vel;
			V.X += (Cur.X - V.X) * FMath::Min(1.f, 4.f * Dt); V.Y += (Cur.Y - V.Y) * FMath::Min(1.f, 4.f * Dt);
			V.Z = FMath::Clamp((Want - (float)P.Z) * 9.f, -400.f, 400.f);
			CM->Velocity = V;
			CM->MaxFlySpeed = 150.f;
			CM->BrakingDecelerationFlying = 0.f;
		}
		else
		{
			if (Swimmers.Contains(Ant)) { Swimmers.Remove(Ant); if (CM->MovementMode == MOVE_Flying) { CM->SetMovementMode(MOVE_Falling); } }
			if (bIn)
			{
				// wading: the current drags it along, the more the deeper it stands
				const float K = FMath::Clamp(S.Total / (R * 1.1f), 0.1f, 1.f);
				const FVector Move(S.Vel.X * Dt * K, S.Vel.Y * Dt * K, 0.f);
				if (!Move.IsNearlyZero(0.01f)) { Ant->AddActorWorldOffset(Move, true); }
			}
		}
	}
}

void UAntFluidWorld::CoupleFeet(float Dt)
{
	UWorld* W = GetWorld();
	if (W->GetNetMode() == NM_Client || Dt <= 0.f) { return; }
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* Hm = *It;
		const FVector Fwd = Hm->GetActorForwardVector();
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			const FVector Sole = Hm->GetSoleLocation(Leg);
			FVector* Last = LastFoot[Leg].Find(Hm);
			const FVector Vel = Last ? (Sole - *Last) / Dt : FVector::ZeroVector;
			const bool bWasAbove = Last ? true : false;
			const FVector Prev = Last ? *Last : Sole;
			LastFoot[Leg].Add(Hm, Sole);
			FSample S; int32 b = -1;
			if (!SampleAt(Sole, S, &b) || S.Total < 0.4f) { continue; }
			if (Sole.Z > S.Surface + 4.f) { continue; }
			FBasin& B = *Basins[b];
			// the slipper is long: two discs along it
			for (int32 k = -1; k <= 1; k += 2)
			{
				const FVector Q = Sole + Fwd * (150.f * k);
				B.Push((float)Q.X, (float)Q.Y, 190.f, FVector2f((float)Vel.X, (float)Vel.Y), FMath::Min(1.f, 12.f * Dt));
			}
			if (bWasAbove && Prev.Z > S.Surface + 4.f && Vel.Z < -150.f)
			{
				const float Hit = FMath::Min(FMath::Abs((float)Vel.Z) * 0.6f, 0.4f * B.Dx / (B.Dt / B.SubSteps));
				B.Kick((float)Sole.X, (float)Sole.Y, 420.f, Hit, 0.9f);
				B.AddFoam((float)Sole.X, (float)Sole.Y, 380.f, 0.5f);
				if (LogOn()) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID footfall of leg %d in %s at %s, speed %.0f, depth %.1f"), Leg, *B.Name, *Sole.ToCompactString(), Vel.Z, S.Total); }
				if (W->GetNetMode() != NM_DedicatedServer) { AntFx::Splash(this, FVector(Sole.X, Sole.Y, S.Surface), 40.f); }
				if (Quality >= 2)
				{
					const FVector C(Sole.X, Sole.Y, S.Surface + 150.f);
					AddVolume(FName(*FString::Printf(TEXT("step%d"), Leg)), 2, FBox(C - FVector(520.f, 520.f, 220.f), C + FVector(520.f, 520.f, 380.f)), Quality >= 3 ? 112 : 72, 3.f);
				}
				// drops fly off
				FRandomStream Rng(GFrameCounter);
				for (int32 d = 0; d < (Quality >= 2 ? 10 : 4); ++d)
				{
					const float Ang = Rng.FRandRange(0.f, 6.283f);
					const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
					AddDrop(FVector(Sole.X, Sole.Y, S.Surface + 10.f) + Dir * Rng.FRandRange(150.f, 260.f), Rng.FRandRange(5.f, 11.f), Water, Dir * Rng.FRandRange(250.f, 600.f) + FVector(0.f, 0.f, Rng.FRandRange(350.f, 800.f)));
				}
			}
		}
	}
}

void UAntFluidWorld::AffectAnt(const AAntCharacter* Ant, FAntEnv& Env) const
{
	if (!bReady || !Ant) { return; }
	const float R = Ant->GetSimpleCollisionRadius();
	const FVector P = Ant->GetActorLocation();
	FSample S;
	if (!SampleAt(P, S) || S.Total < 0.4f || P.Z - R > S.Surface + 3.f) { return; }
	if (S.Depth[LWater] > 0.4f) { Env.Flags |= AntStatus::Wet; }
	if (S.Depth[LHoney] > 1.f || (S.Dye > 0.6f && S.Depth[LWater] < 1.f && S.Depth[LWater] > 0.05f)) { Env.Flags |= AntStatus::Sticky; }   // honey; and cola that has dried to a film
}

// =============================================================================================
// Layer B (2): bounded 3D simulations (Niagara Fluids FLIP on the GPU). Cosmetic, local to this machine.
// =============================================================================================

void UAntFluidWorld::DumpNiagara(UNiagaraSystem* Sys) const
{
	if (!Sys) { return; }
	TArray<FNiagaraVariable> Vars;
	Sys->GetExposedParameters().GetUserParameters(Vars);
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID niagara %s: %d user parameters, %d emitters, fixed bounds %d, warmup %.2f"), *Sys->GetName(), Vars.Num(), Sys->GetEmitterHandles().Num(), Sys->bFixedBounds ? 1 : 0, Sys->GetWarmupTime());
	for (const FNiagaraVariable& V : Vars)
	{
		FString Val;
		const FNiagaraTypeDefinition T = V.GetType();
		const uint8* Data = Sys->GetExposedParameters().GetParameterData(FNiagaraVariable(T, FName(*(FString(TEXT("User.")) + V.GetName().ToString()))));
		if (Data)
		{
			if (T == FNiagaraTypeDefinition::GetFloatDef()) { Val = FString::Printf(TEXT("%g"), *(const float*)Data); }
			else if (T == FNiagaraTypeDefinition::GetIntDef()) { Val = FString::Printf(TEXT("%d"), *(const int32*)Data); }
			else if (T == FNiagaraTypeDefinition::GetBoolDef()) { Val = (*(const int32*)Data) != 0 ? TEXT("true") : TEXT("false"); }
			else if (T == FNiagaraTypeDefinition::GetVec3Def()) { const float* F = (const float*)Data; Val = FString::Printf(TEXT("(%g, %g, %g)"), F[0], F[1], F[2]); }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID   %s : %s = %s"), *V.GetName().ToString(), *T.GetName(), *Val);
	}
	for (const FNiagaraEmitterHandle& H : Sys->GetEmitterHandles())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID   emitter %s enabled %d"), *H.GetName().ToString(), H.GetIsEnabled() ? 1 : 0);
	}
}

UNiagaraComponent* UAntFluidWorld::AddVolume(FName Id, int32 Kind, const FBox& Box, int32 Cells, float Life, float WaterHeight)
{
	if (Quality <= 0 || !Holder || !bFlip) { return nullptr; }
	UNiagaraSystem* Sys = Kind == 0 ? FlipHose : (Kind == 1 ? FlipPool : FlipSplash);
	if (!Sys) { return nullptr; }
	RemoveVolume(Id);
	UNiagaraComponent* C = NewObject<UNiagaraComponent>(Holder);
	C->SetAsset(Sys);
	C->SetAutoActivate(false);
	C->SetupAttachment(Holder->GetRootComponent());
	C->SetWorldLocation(Box.GetCenter());
	C->RegisterComponent();
	C->SetVariableVec3(TEXT("World Grid Extents"), Box.GetSize());
	C->SetVariableInt(TEXT("Num Cells Max Axis"), Cells);
	C->SetVariableFloat(TEXT("Water Height"), WaterHeight);
	C->SetVariableBool(TEXT("Show Bounds"), false);
	C->Activate(true);
	FAntFluidVolume V; V.Comp = C; V.Id = Id; V.Box = Box; V.Kind = Kind; V.DieAt = Life > 0.f ? GetWorld()->GetTimeSeconds() + Life : -1.f;
	Volumes.Add(V);
	if (LogOn()) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID 3D volume %s kind %d at %s size %s, %d cells on the long axis, life %.1f"), *Id.ToString(), Kind, *Box.GetCenter().ToCompactString(), *Box.GetSize().ToCompactString(), Cells, Life); }
	return C;
}

void UAntFluidWorld::RemoveVolume(FName Id)
{
	for (int32 i = Volumes.Num() - 1; i >= 0; --i)
	{
		if (Volumes[i].Id == Id)
		{
			if (Volumes[i].Comp) { Volumes[i].Comp->DestroyComponent(); }
			Volumes.RemoveAt(i);
		}
	}
}

void UAntFluidWorld::TickVolumes(float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = Volumes.Num() - 1; i >= 0; --i)
	{
		if (Volumes[i].DieAt > 0.f && Now >= Volumes[i].DieAt)
		{
			if (Volumes[i].Comp) { Volumes[i].Comp->DestroyComponent(); }
			Volumes.RemoveAt(i);
		}
	}
	const float Keep = FMath::Exp(-Dt / 0.5f);
	for (int32 i = Columns.Num() - 1; i >= 0; --i)
	{
		Columns[i].Rate *= Keep;
		if (Now - Columns[i].Seen > 2.f) { Columns.RemoveAtSwap(i); }
	}
}

// =============================================================================================
// Layer C: drops
// =============================================================================================

int32 UAntFluidWorld::AddDrop(const FVector& At, float Radius, ELiquid Liquid, const FVector& Vel, uint8 State)
{
	if (Drops.Num() >= 600) { return 0; }
	FAntDrop D; D.Pos = At; D.Vel = Vel; D.Radius = Radius; D.Liquid = Liquid; D.State = State; D.Id = NextDropId++;
	Drops.Add(D);
	return D.Id;
}

void UAntFluidWorld::TickDrops(float Dt)
{
	UWorld* W = GetWorld();
	const bool bServer = W->GetNetMode() != NM_Client;   // a client flies its own drops (a geyser's) for the look; only the server traps ants
	if (!bServer && Drops.Num() == 0) { return; }
	const float G = FMath::Abs(W->GetGravityZ());
	const float Now = W->GetTimeSeconds();
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_AntClimbable);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AntFluidDrop), false);

	// the tap drips: a drop grows at the spout, necks and lets go
	if (bServer && bDrip && Sources.Num() > 0 && !Sources[0].bOn)
	{
		bool bHanging = false;
		for (const FAntDrop& D : Drops) { bHanging |= D.State == 2; }
		if (!bHanging && Now >= TapDripAt) { AddDrop(Sources[0].Pos - FVector(0.f, 0.f, 4.f), 3.f, Water, FVector::ZeroVector, 2); }
	}

	TArray<AAntCharacter*> Ants;
	if (bServer) { for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead) { Ants.Add(*It); } } }

	const int32 N0 = Drops.Num();
	for (int32 i = 0; i < N0; ++i)
	{
		FAntDrop& D = Drops[i];
		if (D.Radius <= 0.f) { continue; }
		D.Age += Dt;
		// the shape rings after a knock: a damped spring (the lowest mode of a drop: frequency falls with size)
		const float Freq = 60.f / FMath::Max(D.Radius, 2.f) * 4.f;
		D.WobbleVel += (-D.Wobble * Freq * Freq - D.WobbleVel * 5.f) * Dt;
		D.Wobble = FMath::Clamp(D.Wobble + D.WobbleVel * Dt, -0.45f, 0.45f);

		if (D.State == 2)
		{
			// hanging from the spout: it fills until its weight beats the surface tension holding it
			D.Radius += 5.5f * Dt;
			D.Pos = Sources[0].Pos - FVector(0.f, 0.f, D.Radius * 1.25f);
			if (D.Radius >= 17.f)
			{
				D.State = 0; D.Vel = FVector(0.f, 0.f, -40.f); D.Wobble = 0.35f;
				TapDripAt = Now + 1.6f;
				AddDrop(D.Pos + FVector(0.f, 0.f, D.Radius + 5.f), 3.5f, Water, FVector(0.f, 0.f, -10.f));   // the satellite droplet the neck leaves behind
			}
			continue;
		}

		AAntCharacter* Held = D.Trapped.Get();
		if (D.State == 0)
		{
			D.Vel.Z -= G * Dt;
			const FVector Next = D.Pos + D.Vel * Dt;
			// into liquid? it joins the pool
			FSample S; int32 b = -1;
			if (SampleAt(Next, S, &b) && S.Total > 0.8f && Next.Z - D.Radius < S.Surface)
			{
				FBasin& B = *Basins[b];
				const float Vol = 4.18879f * D.Radius * D.Radius * D.Radius;
				B.AddVolume((ELiquid)D.Liquid, (float)Next.X, (float)Next.Y, D.Radius, Vol, FVector2f((float)D.Vel.X, (float)D.Vel.Y));
				B.Kick((float)Next.X, (float)Next.Y, D.Radius * 3.f, FMath::Min(FMath::Abs((float)D.Vel.Z) * 0.35f, 0.4f * B.Dx / (B.Dt / B.SubSteps)), 0.7f);
				if (W->GetNetMode() != NM_DedicatedServer && D.Radius > 5.f) { AntFx::Droplets(this, FVector(Next.X, Next.Y, S.Surface), 3, D.Radius * 0.3f); }
				if (Held) { Held->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
				D.Radius = 0.f;
				continue;
			}
			// onto an ant? a drop bigger than the ant swallows it
			if (!Held && D.Radius > 12.f)
			{
				for (AAntCharacter* A : Ants)
				{
					if (FVector::DistSquared(A->GetActorLocation(), Next) < FMath::Square(D.Radius + A->GetSimpleCollisionRadius() * 0.5f))
					{
						D.Trapped = A; D.TrapTime = 0.f; Held = A; D.Wobble = 0.3f;
						UE_LOG(LogTemp, Display, TEXT("ANTFLUID drop %d (r %.1f) trapped %s"), D.Id, D.Radius, *A->GetName());
						break;
					}
				}
			}
			FHitResult Hit;
			if (W->LineTraceSingleByObjectType(Hit, D.Pos, Next - FVector(0.f, 0.f, D.Radius * 0.6f), Obj, Q))
			{
				const float Speed = (float)D.Vel.Size();
				const float Into = FMath::Abs((float)FVector::DotProduct(D.Vel, Hit.ImpactNormal));
				// Weber number ~ v^2 r: a fast or a big drop breaks up when it lands, a slow small one just sits down
				const float We = Into * Into * D.Radius / 2.2e6f;
				if (We > 1.f && D.Radius > 5.f && !Held)
				{
					const int32 Parts = FMath::Clamp(FMath::RoundToInt(1.5f + We), 2, 6);
					const float Vol = D.Radius * D.Radius * D.Radius;
					const float Rc = FMath::Pow(Vol * 0.85f / Parts, 1.f / 3.f);
					FRandomStream Rng(D.Id * 7919);
					FVector T1, T2; Hit.ImpactNormal.FindBestAxisVectors(T1, T2);
					for (int32 k = 0; k < Parts; ++k)
					{
						const float Ang = (k + Rng.FRandRange(0.f, 0.6f)) * 6.2832f / Parts;
						const FVector Dir = T1 * FMath::Cos(Ang) + T2 * FMath::Sin(Ang);
						AddDrop(Hit.ImpactPoint + Hit.ImpactNormal * (Rc + 1.f) + Dir * D.Radius * 0.6f, Rc * Rng.FRandRange(0.8f, 1.1f), (ELiquid)D.Liquid, Dir * Into * Rng.FRandRange(0.25f, 0.45f) + Hit.ImpactNormal * Into * Rng.FRandRange(0.12f, 0.25f));
					}
					Spill((ELiquid)D.Liquid, Hit.ImpactPoint + FVector(0.f, 0.f, 2.f), 4.18879f * Vol * 0.15f, D.Radius);   // the wet mark it leaves
					if (W->GetNetMode() != NM_DedicatedServer) { AntFx::Droplets(this, Hit.ImpactPoint, 4, D.Radius * 0.25f); }
					D.Radius = 0.f;
					continue;
				}
				if (Hit.ImpactNormal.Z > 0.5f)
				{
					D.State = 1; D.RestZ = (float)Hit.ImpactPoint.Z; D.Pos = Hit.ImpactPoint; D.Wobble = -FMath::Min(0.4f, Speed / 1500.f);
					D.Vel = FVector::VectorPlaneProject(D.Vel, Hit.ImpactNormal) * 0.25f;
				}
				else
				{
					// a wall: it slides off down it
					D.Pos = Hit.ImpactPoint + Hit.ImpactNormal * 1.f;
					D.Vel = FVector::VectorPlaneProject(D.Vel, Hit.ImpactNormal) * 0.5f;
				}
			}
			else { D.Pos = Next; }
		}
		else
		{
			// lying on a surface as a bead. It moves when the slope or a push beats the pinning of its contact line.
			FHitResult Hit;
			const FVector Probe = D.Pos + FVector(0.f, 0.f, 12.f);
			if (!W->LineTraceSingleByObjectType(Hit, Probe, Probe - FVector(0.f, 0.f, 30.f), Obj, Q))
			{
				D.State = 0; D.Pos.Z += D.Radius * 0.6f;   // rolled over an edge
				continue;
			}
			D.RestZ = (float)Hit.ImpactPoint.Z; D.Pos.Z = D.RestZ;
			const FVector Slope = FVector::VectorPlaneProject(FVector(0.f, 0.f, -G), Hit.ImpactNormal);
			const float Pin = G * 0.12f;   // about 7 degrees of slope
			if (Slope.Size() > Pin) { D.Vel += Slope.GetSafeNormal() * (Slope.Size() - Pin) * Dt; }
			// an ant leans on it: the bead is pushed along
			for (AAntCharacter* A : Ants)
			{
				if (A == Held) { continue; }
				const FVector To = D.Pos + FVector(0.f, 0.f, D.Radius * 0.6f) - A->GetActorLocation();
				const float Reach = D.Radius * 1.15f + A->GetSimpleCollisionRadius();
				if (To.SizeSquared() < Reach * Reach)
				{
					if (Held)
					{
						// a teammate reached the drop: it bursts, the one inside is free
						D.TrapTime = 100.f;
						break;
					}
					const FVector AV = A->GetVelocity();
					const FVector Dir = To.GetSafeNormal2D();
					const float Push = FMath::Max(0.f, (float)FVector::DotProduct(AV, Dir));
					D.Vel += Dir * (Push * 0.9f - FMath::Max(0.f, (float)FVector::DotProduct(D.Vel, Dir))) * FMath::Min(1.f, 10.f * Dt) * FMath::Clamp(8.f / D.Radius, 0.2f, 1.f);
					D.Wobble += 0.4f * Dt;
				}
			}
			D.Vel.Z = 0.f;
			const float Sp = (float)D.Vel.Size();
			if (Sp > 0.5f)
			{
				// rolling resistance; it also leaves a little of itself behind as a wet trail
				D.Vel *= FMath::Max(0.f, 1.f - (1.2f + 60.f / FMath::Max(Sp, 1.f)) * Dt);
				FHitResult Side;
				const FVector Step = D.Vel * Dt;
				if (W->LineTraceSingleByObjectType(Side, D.Pos + FVector(0.f, 0.f, D.Radius * 0.5f), D.Pos + FVector(0.f, 0.f, D.Radius * 0.5f) + Step.GetSafeNormal() * (Step.Size() + D.Radius * 0.7f), Obj, Q))
				{
					D.Vel = FVector::VectorPlaneProject(D.Vel, Side.ImpactNormal) * 0.6f;
				}
				else { D.Pos += Step; }
			}
			else { D.Vel = FVector::ZeroVector; }
			// touched the flow layer: it is part of the puddle now. And a bead too big to hold its shape is a puddle too.
			FSample S; int32 b = -1;
			const bool bLiquid = SampleAt(D.Pos + FVector(0.f, 0.f, 3.f), S, &b) && S.Total > 0.8f;
			if ((bLiquid || D.Radius > 30.f) && !Held)
			{
				Spill((ELiquid)D.Liquid, D.Pos + FVector(0.f, 0.f, 3.f), 4.18879f * D.Radius * D.Radius * D.Radius, D.Radius, FVector2f((float)D.Vel.X, (float)D.Vel.Y));
				D.Radius = 0.f;
				continue;
			}
		}

		// the ant inside: held in the middle, struggling. It gets out by itself after a while, at once if a teammate bursts the drop.
		if (Held)
		{
			D.TrapTime += Dt;
			UCharacterMovementComponent* CM = Held->GetCharacterMovement();
			if (CM->MovementMode != MOVE_Flying) { CM->SetMovementMode(MOVE_Flying); }
			CM->Velocity = FVector::ZeroVector;
			const FVector Centre = D.State == 1 ? D.Pos + FVector(0.f, 0.f, D.Radius * 0.7f) : D.Pos;
			Held->SetActorLocation(Centre + FVector(FMath::Sin(Now * 23.f), FMath::Cos(Now * 19.f), 0.f) * 1.5f, false, nullptr, ETeleportType::TeleportPhysics);
			D.WobbleVel += FMath::Sin(Now * 14.f) * 40.f * Dt;
			if (D.TrapTime > 7.f || Held->bDead)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTFLUID drop %d let %s go after %.1f s (%s)"), D.Id, *Held->GetName(), D.TrapTime, D.TrapTime >= 100.f ? TEXT("burst by a teammate") : TEXT("it struggled out"));
				CM->SetMovementMode(MOVE_Falling);
				D.Trapped = nullptr;
				// the drop breaks into small ones
				const float Vol = D.Radius * D.Radius * D.Radius;
				const float Rc = FMath::Pow(Vol * 0.7f / 5.f, 1.f / 3.f);
				for (int32 k = 0; k < 5; ++k)
				{
					const float Ang = k * 1.2566f;
					const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
					AddDrop(Centre + Dir * D.Radius * 0.7f + FVector(0.f, 0.f, 4.f), Rc, (ELiquid)D.Liquid, Dir * 160.f + FVector(0.f, 0.f, 140.f));
				}
				Spill((ELiquid)D.Liquid, Centre, 4.18879f * Vol * 0.3f, D.Radius);
				D.Radius = 0.f;
				continue;
			}
		}
	}

	// beads that touch run together into one (the bigger keeps its place, mostly)
	for (int32 i = 0; i < Drops.Num(); ++i)
	{
		FAntDrop& A = Drops[i];
		if (A.Radius <= 0.f || A.State == 2) { continue; }
		for (int32 j = i + 1; j < Drops.Num(); ++j)
		{
			FAntDrop& B = Drops[j];
			if (B.Radius <= 0.f || B.State == 2 || A.Liquid != B.Liquid) { continue; }
			if (A.Age < 0.25f && B.Age < 0.25f) { continue; }   // just split from the same drop
			const float Reach = (A.Radius + B.Radius) * (A.State == 1 && B.State == 1 ? 1.05f : 0.8f);
			if (FVector::DistSquared(A.Pos, B.Pos) > Reach * Reach) { continue; }
			const float Va = A.Radius * A.Radius * A.Radius, Vb = B.Radius * B.Radius * B.Radius;
			const float Wa = Va / (Va + Vb);
			A.Pos = A.Pos * Wa + B.Pos * (1.f - Wa);
			A.Vel = A.Vel * Wa + B.Vel * (1.f - Wa);
			A.Radius = FMath::Pow(Va + Vb, 1.f / 3.f);
			A.State = FMath::Min(A.State, B.State);
			A.Wobble = 0.35f * (1.f - FMath::Abs(2.f * Wa - 1.f)) + 0.1f;
			if (!A.Trapped.IsValid() && B.Trapped.IsValid()) { A.Trapped = B.Trapped; A.TrapTime = B.TrapTime; }
			B.Radius = 0.f;
		}
	}
	Drops.RemoveAllSwap([](const FAntDrop& D) { return D.Radius <= 0.f; }, EAllowShrinking::No);
}

void UAntFluidWorld::DrawDrops()
{
	if (!DropMesh) { return; }
	const int32 Want = Quality <= 0 ? 0 : Drops.Num();
	TArray<FTransform> Xf;
	Xf.Reserve(Want);
	for (int32 i = 0; i < Want; ++i)
	{
		const FAntDrop& D = Drops[i];
		FVector Scale; FVector Pos = D.Pos;
		const float Wb = 1.f + D.Wobble;
		if (D.State == 1)
		{
			// a bead on a surface: a cap of a sphere meeting it at the contact angle (about 75 degrees), squashed by its weight when it is big
			const float Rs = D.Radius * 1.26f;                       // the sphere whose 75-degree cap holds the drop's volume
			float CapH = Rs * 0.741f;                                // 1 - cos(75 deg)
			const float MaxH = 11.f;                                 // twice the capillary length: no puddle stands taller
			const float Sq = CapH > MaxH ? MaxH / CapH : 1.f;
			const float Z = Sq * Wb, XY = 1.f / FMath::Sqrt(Z);
			Scale = FVector(Rs * XY, Rs * XY, Rs * Z) / 50.f;
			Pos.Z = D.RestZ + (CapH - Rs) * Z;
		}
		else if (D.State == 2)
		{
			Scale = FVector(D.Radius * 0.9f, D.Radius * 0.9f, D.Radius * 1.25f) / 50.f;   // a pendant drop: longer than wide
		}
		else
		{
			Scale = FVector(D.Radius / FMath::Sqrt(Wb), D.Radius / FMath::Sqrt(Wb), D.Radius * Wb) / 50.f;
		}
		Xf.Add(FTransform(FQuat::Identity, Pos, Scale));
	}
	if (DropMesh->GetInstanceCount() != Want)
	{
		DropMesh->ClearInstances();
		if (Want > 0) { DropMesh->AddInstances(Xf, false, true, false); }
	}
	else if (Want > 0)
	{
		DropMesh->BatchUpdateInstancesTransforms(0, Xf, true, true, false);
	}
	for (int32 i = 0; i < Want; ++i) { DropMesh->SetCustomDataValue(i, 0, Drops[i].Liquid == Cola ? 1.f : (Drops[i].Liquid == Oil ? 0.5f : 0.f), false); }
}

// =============================================================================================
// Dev: "fluid ..." commands (from -AntDo through AAntKitchenGameMode::DevCommand), the demos (-AntFluidDemo=N), stats
// =============================================================================================

namespace
{
	float NumAt(const TArray<FString>& A, int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; }
	const float Litre = 1900.f * 1900.f * 1900.f * 1.e-3f;   // uu^3 in a litre (1 m = 1900 uu)
}

bool UAntFluidWorld::DevCommand(const TArray<FString>& A)
{
	if (A.Num() < 2 || !A[0].Equals(TEXT("fluid"), ESearchCase::IgnoreCase)) { return false; }
	UWorld* W = GetWorld();
	const FString Op = A[1].ToLower();
	APlayerController* PC = W->GetFirstPlayerController();
	if (Op == TEXT("cam") && PC)
	{
		// fluid cam x y z tx ty tz [fov] | fluid cam off
		if (A.Num() >= 3 && A[2] == TEXT("off")) { PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f); return true; }
		const FVector From(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4)), To(NumAt(A, 5), NumAt(A, 6), NumAt(A, 7));
		if (!DevCam) { DevCam = W->SpawnActor<ACameraActor>(); }
		if (DevCam)
		{
			DevCam->SetActorLocationAndRotation(From, (To - From).Rotation());
			DevCam->GetCameraComponent()->SetFieldOfView(NumAt(A, 8, 70.f));
			DevCam->GetCameraComponent()->bConstrainAspectRatio = false;
			PC->SetViewTargetWithBlend(DevCam, 0.f);
			if (PC->MyHUD) { PC->MyHUD->bShowHUD = false; }
		}
		return true;
	}
	if (Op == TEXT("shot") && PC)
	{
		static int32 ShotN = 0;
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID shot %d at %.1f s: %s"), ShotN++, W->GetTimeSeconds(), A.Num() > 2 ? *A[2] : TEXT("-"));
		PC->ConsoleCommand(TEXT("Shot"), false);
		return true;
	}
	if (Op == TEXT("human"))
	{
		// fluid human x y yaw [walkx walky scale]: the Human as a puppet, put there, optionally walking to a point
		for (TActorIterator<AHumanCharacter> It(W); It; ++It)
		{
			AHumanCharacter* H = *It;
			H->bDevPuppet = true;
			const FRotator Rot(0.f, NumAt(A, 4), 0.f);
			H->SetActorLocationAndRotation(FVector(NumAt(A, 2), NumAt(A, 3), H->GetActorLocation().Z), Rot, false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* HC = H->GetController()) { HC->SetControlRotation(Rot); }
			H->DevWalk = A.Num() >= 7 ? FVector(NumAt(A, 5), NumAt(A, 6), 0.f) : FVector::ZeroVector;
			H->DevWalkScale = NumAt(A, 7, 1.f);
			LastFoot[0].Remove(H); LastFoot[1].Remove(H);
			break;
		}
		return true;
	}
	if (!bReady) { UE_LOG(LogTemp, Warning, TEXT("ANTFLUID not ready yet: %s"), *FString::Join(A, TEXT(" "))); return true; }
	if (Op == TEXT("tap") && Sources.Num() > 0)
	{
		// fluid tap on [litres per second] [liquid] | fluid tap off | fluid tap toggle. The faucet fixture runs it (its sound, its valves, the Human shuts it).
		const bool bOn = A.Num() >= 3 && (A[2] == TEXT("on") || (A[2] == TEXT("toggle") && !Sources[0].bOn));
		if (bOn)
		{
			if (A.Num() >= 4) { TapRate = FMath::Clamp(NumAt(A, 3, 5.f), 0.05f, 80.f); }
			ELiquid L = Water;
			if (A.Num() >= 5) { LiquidFromName(A[4], L); }
			TapLiquid = L;
		}
		if (!Faucet.IsValid()) { for (TActorIterator<AKitchenFaucet> It(W); It; ++It) { Faucet = *It; break; } }
		if (AKitchenFaucet* F = Faucet.Get()) { F->SetWater(bOn, false); }
		else { Sources[0].bOn = bOn; }
		if (!bOn) { FloodLeft = 0.f; }
		bDrip = false;
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID tap %s, %.2f l/s"), bOn ? TEXT("on") : TEXT("off"), TapRate);
	}
	else if (Op == TEXT("flood")) { Flood(NumAt(A, 2, 15.f), NumAt(A, 3, 30.f)); }   // fluid flood [seconds] [litres per second]
	else if (Op == TEXT("bucket"))
	{
		// fluid bucket x y z [litres]: a bucket of water emptied from above the point
		const FVector At(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4) + 260.f);
		const float L = NumAt(A, 5, 8.f);
		Spill(Water, At, L * Litre, 160.f);
		if (Net.IsValid()) { Net->MulticastSpill((uint8)Water, At, L * Litre, 160.f); }
		FRandomStream Rng((int32)GFrameCounter);
		for (int32 d = 0; d < 18; ++d)
		{
			const float Ang = Rng.FRandRange(0.f, 6.283f);
			AddDrop(At + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Rng.FRandRange(10.f, 120.f), Rng.FRandRange(5.f, 13.f), Water,
				FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Rng.FRandRange(60.f, 260.f) + FVector(0.f, 0.f, Rng.FRandRange(-300.f, 100.f)));
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID bucket: %.1f l at %s"), L, *At.ToCompactString());
	}
	else if (Op == TEXT("geyser")) { ELiquid L = Cola; if (A.Num() >= 7) { LiquidFromName(A[6], L); } Geyser(FVector(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4)), NumAt(A, 5, 8.f), L); }   // fluid geyser x y z [seconds] [liquid]
	else if (Op == TEXT("pour"))
	{
		// fluid pour <liquid> x y z litres_per_second seconds [radius]
		ELiquid L = Water;
		LiquidFromName(A.IsValidIndex(2) ? A[2] : FString(), L);
		AddSource(L, FVector(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5)), NumAt(A, 6, 1.f), NumAt(A, 7, 5.f), NumAt(A, 8, 40.f));
	}
	else if (Op == TEXT("dry")) { Dry(); }
	else if (Op == TEXT("flip")) { bFlip = A.Num() < 3 || A[2] == TEXT("on"); UE_LOG(LogTemp, Display, TEXT("ANTFLUID 3D FLIP volumes %s"), bFlip ? TEXT("on") : TEXT("off")); }
	else if (Op == TEXT("half")) { bAllowHalf = A.Num() < 3 || A[2] == TEXT("on"); UE_LOG(LogTemp, Display, TEXT("ANTFLUID 30 Hz for busy basins %s"), bAllowHalf ? TEXT("on") : TEXT("off")); }
	else if (Op == TEXT("sleep")) { const bool b = A.Num() < 3 || A[2] == TEXT("on"); for (TUniquePtr<FBasin>& B : Basins) { B->bSleep = b; } UE_LOG(LogTemp, Display, TEXT("ANTFLUID sleeping tiles %s"), b ? TEXT("on") : TEXT("off")); }
	else if (Op == TEXT("drain")) { SinkDrain = FMath::Max(0.f, NumAt(A, 2, 1.2f)); UE_LOG(LogTemp, Display, TEXT("ANTFLUID the sink's plughole takes %.2f l/s"), SinkDrain); }
	else if (Op == TEXT("debug"))
	{
		bDebug = A.Num() < 3 ? !bDebug : A[2] == TEXT("on");
		if (bDebug) { for (int32 b = 0; b < Basins.Num(); ++b) { AsciiMap(*Basins[b], LWater); } }
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID debug %s"), bDebug ? TEXT("on") : TEXT("off"));
	}
	else if (Op == TEXT("info")) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID %s"), *Describe()); LogStats(); }
	else if (Op == TEXT("source"))
	{
		// fluid source <liquid> x y z litres_per_second seconds [radius]: a pour (a bottle, a spill that keeps coming)
		ELiquid L = Water;
		LiquidFromName(A.IsValidIndex(2) ? A[2] : FString(), L);
		FSource S; S.Id = FName(*FString::Printf(TEXT("src%d"), Sources.Num())); S.Pos = FVector(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5));
		S.Liquid = L; S.Rate = NumAt(A, 6, 1.f) * Litre; S.Left = S.Rate * NumAt(A, 7, 5.f); S.Radius = NumAt(A, 8, 40.f); S.bOn = true;
		Sources.Add(S);
	}
	else if (Op == TEXT("spill"))
	{
		// fluid spill <liquid> x y z litres [radius]
		ELiquid L = Water;
		if (!LiquidFromName(A.IsValidIndex(2) ? A[2] : FString(), L)) { UE_LOG(LogTemp, Warning, TEXT("ANTFLUID spill: water|cola|oil|honey")); return true; }
		const FVector At(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5));
		Spill(L, At, NumAt(A, 6, 1.f) * Litre, NumAt(A, 7, 120.f));
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID spill %.2f l of %s at %s into basin %d"), NumAt(A, 6, 1.f), LiquidName(L), *At.ToCompactString(), BasinUnder(At));
	}
	else if (Op == TEXT("drop")) { AddDrop(FVector(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4)), NumAt(A, 5, 12.f), Water, FVector(NumAt(A, 6), NumAt(A, 7), NumAt(A, 8))); }
	else if (Op == TEXT("drip")) { bDrip = A.Num() >= 3 && A[2] == TEXT("on"); if (Sources.Num() > 0 && bDrip) { Sources[0].bOn = false; } TapDripAt = 0.f; }
	else if (Op == TEXT("quality")) { Quality = FMath::Clamp((int32)NumAt(A, 2, 2.f), 0, 3); if (Quality == 0) { for (FAntFluidVolume& V : Volumes) { if (V.Comp) { V.Comp->DestroyComponent(); } } Volumes.Reset(); } UE_LOG(LogTemp, Display, TEXT("ANTFLUID quality %d"), Quality); }
	else if (Op == TEXT("freeze")) { bFrozen = A.Num() >= 3 && A[2] == TEXT("on"); }
	else if (Op == TEXT("clear")) { Dry(); }
	else if (Op == TEXT("stats")) { LogStats(); }
	else if (Op == TEXT("body"))
	{
		// fluid body x y z [seed radius]: the body of water a short there would run through (the KAN-21 water fix), and what the column says
		const FVector P(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4));
		FAntWaterBody Bd;
		const bool bOk = WaterBodyAt(P, 0.5f, Bd, NumAt(A, 5, 150.f));
		FSample S;
		const bool bS = SampleAt(P, S);
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID body at %s: %s, basin %d, %d cells%s | column: bed %.1f surface %.1f depth %.1f, DepthAt %.1f, DepthIn %.1f"),
			*P.ToCompactString(), bOk ? TEXT("water") : TEXT("none"), Bd.Basin, Bd.Num, Bd.bCapped ? TEXT(" (capped)") : TEXT(""),
			bS ? S.Bed : 0.f, bS ? S.Surface : 0.f, bS ? S.Total : 0.f, DepthAt(P), DepthIn(P));
	}
	else if (Op == TEXT("show") && Holder) { Holder->SetActorHiddenInGame(NumAt(A, 2, 1.f) < 0.5f); for (FAntFluidVolume& V : Volumes) { if (V.Comp) { V.Comp->SetVisibility(NumAt(A, 2, 1.f) >= 0.5f); } } }   // A/B: the liquid drawn or not (it still simulates)
	else if (Op == TEXT("res") && PC) { PC->ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%dw"), (int32)NumAt(A, 2, 1600.f), (int32)NumAt(A, 3, 900.f)), false); }
	else if (Op == TEXT("profile") && PC) { PC->ConsoleCommand(TEXT("ProfileGPU"), false); }   // one frame of GPU passes into the log
	else if (Op == TEXT("map"))
	{
		const int32 b = FMath::Clamp((int32)NumAt(A, 2), 0, Basins.Num() - 1);
		AsciiMap(*Basins[b], (int32)NumAt(A, 3, -1.f));
	}
	else if (Op == TEXT("vol"))
	{
		// fluid vol kind x y z sx sy sz cells life [waterheight]: a 3D FLIP volume by hand (0 hose, 1 pool, 2 splash)
		const FVector C(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5)), E(NumAt(A, 6, 200.f), NumAt(A, 7, 200.f), NumAt(A, 8, 200.f));
		UNiagaraComponent* NC = AddVolume(FName(*FString::Printf(TEXT("dev%d"), (int32)NumAt(A, 2))), (int32)NumAt(A, 2), FBox(C - E * 0.5f, C + E * 0.5f), (int32)NumAt(A, 9, 64.f), NumAt(A, 10, -1.f), NumAt(A, 11, 0.f));
		if (NC && A.Num() >= 13) { NC->SetWorldRotation(FRotator(NumAt(A, 13), NumAt(A, 12), 0.f)); }
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID vol: %s"), NC ? TEXT("spawned") : TEXT("FAILED (quality 0 or no asset)"));
	}
	else if (Op == TEXT("probe"))
	{
		// fluid probe x0 y0 x1 y1 n [ztop]: what falls straight down along a line lands on first (Z and the component)
		const FVector P0(NumAt(A, 2), NumAt(A, 3), 0.f), P1(NumAt(A, 4), NumAt(A, 5), 0.f);
		const int32 N = FMath::Clamp((int32)NumAt(A, 6, 20.f), 2, 400);
		const float ZTop = NumAt(A, 7, 2600.f);
		FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_AntClimbable);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P = FMath::Lerp(P0, P1, (float)i / (N - 1));
			FHitResult H;
			if (W->LineTraceSingleByObjectType(H, FVector(P.X, P.Y, ZTop), FVector(P.X, P.Y, -200.f), Obj))
			{
				UE_LOG(LogTemp, Display, TEXT("ANTFLUID probe (%.0f, %.0f): z %.0f %s"), P.X, P.Y, H.ImpactPoint.Z, H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"));
			}
			else { UE_LOG(LogTemp, Display, TEXT("ANTFLUID probe (%.0f, %.0f): nothing"), P.X, P.Y); }
		}
	}
	else if (Op == TEXT("prop"))
	{
		// fluid prop sponge|block|brick x y z [sx sy sz]: a physics box for the water to carry (sponge: very light, floats high)
		const FString Kind = A.IsValidIndex(2) ? A[2].ToLower() : FString(TEXT("block"));
		const FVector At(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5));
		FVector Size = Kind == TEXT("sponge") ? FVector(180.f, 120.f, 60.f) : (Kind == TEXT("brick") ? FVector(90.f, 60.f, 40.f) : FVector(70.f, 70.f, 70.f));
		if (A.Num() >= 9) { Size = FVector(NumAt(A, 6), NumAt(A, 7), NumAt(A, 8)); }
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Box = W->SpawnActor<AStaticMeshActor>(At, FRotator(0.f, NumAt(A, 9), 0.f), SP);
		if (Box)
		{
			UStaticMeshComponent* M = Box->GetStaticMeshComponent();
			M->SetMobility(EComponentMobility::Movable);
			M->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cube));
			Box->SetActorScale3D(Size / 100.f);
			AntKit::Paint(M, Kind == TEXT("sponge") ? FLinearColor(0.95f, 0.8f, 0.15f) : (Kind == TEXT("brick") ? FLinearColor(0.5f, 0.2f, 0.12f) : FLinearColor(0.75f, 0.55f, 0.3f)));
			M->SetCollisionProfileName(TEXT("PhysicsActor"));
			M->SetSimulatePhysics(true);
			M->SetMassOverrideInKg(NAME_None, Kind == TEXT("sponge") ? 0.08f : (Kind == TEXT("brick") ? 0.2f : 0.03f), true);
			if (Kind == TEXT("sponge")) { M->SetLinearDamping(0.5f); M->SetAngularDamping(2.f); }
			Box->Tags.Add(Kind == TEXT("sponge") ? FName(TEXT("AntSponge")) : (Kind == TEXT("brick") ? FName(TEXT("AntSinks")) : FName(TEXT("AntFloat"))));
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID prop %s at %s, size %s, mass %.4f kg"), *Kind, *At.ToCompactString(), *Size.ToCompactString(), M->GetMass());
			LastProp = Box;
		}
	}
	else if (Op == TEXT("shove"))
	{
		// fluid shove vx vy: the last prop is given a velocity (to compare a slide on oil with one on a dry floor)
		if (AActor* Pr = LastProp.Get()) { if (UPrimitiveComponent* Pc = Cast<UPrimitiveComponent>(Pr->GetRootComponent())) { Pc->SetPhysicsLinearVelocity(FVector(NumAt(A, 2), NumAt(A, 3), 0.f)); } }
	}
	else if (Op == TEXT("loot"))
	{
		// fluid loot n x y z: the n nearest loose pieces of food are dropped around a point
		const FVector At(NumAt(A, 3), NumAt(A, 4), NumAt(A, 5));
		TArray<AKitchenLoot*> All;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It) { if (!It->IsCarried() && !It->bTrophy && !It->bIsGear) { All.Add(*It); } }
		All.Sort([&At](const AKitchenLoot& L1, const AKitchenLoot& L2) { return FVector::DistSquared(L1.GetActorLocation(), At) < FVector::DistSquared(L2.GetActorLocation(), At); });
		const int32 N = FMath::Min((int32)NumAt(A, 2, 3.f), All.Num());
		for (int32 i = 0; i < N; ++i)
		{
			const float Ang = i * 2.4f;
			All[i]->SetActorLocation(At + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (60.f + 40.f * i) + FVector(0.f, 0.f, 30.f * i), false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID loot %s dropped at %s (floats: %s)"), *All[i]->LootName, *All[i]->GetActorLocation().ToCompactString(), FloatOf(All[i]) > 1.f ? TEXT("yes") : TEXT("no"));
		}
	}
	else if (Op == TEXT("ant") && PC)
	{
		// fluid ant x y z: the player's ant is put there (onto a sponge, into the water)
		AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn());
		FVector To(NumAt(A, 2), NumAt(A, 3), NumAt(A, 4));
		if (A.IsValidIndex(2) && A[2] == TEXT("sponge"))
		{
			// onto the sponge, wherever it has drifted
			for (TActorIterator<AActor> It(W); It; ++It) { if (It->ActorHasTag(TEXT("AntSponge"))) { To = It->GetActorLocation() + FVector(0.f, 0.f, 70.f); break; } }
		}
		if (Me && A.IsValidIndex(2) && A[2] == TEXT("sponge")) { Me->GetCharacterMovement()->bEnablePhysicsInteraction = false; }   // a rider does not kick its raft away
		if (Me) { Me->ApplyTeleport(To); UE_LOG(LogTemp, Display, TEXT("ANTFLUID ant put at %s"), *To.ToCompactString()); }
	}
	else if (Op == TEXT("track"))
	{
		// fluid track: where the props, the loot in liquid and the ants in liquid are, and what the liquid does there
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			const bool bProp = It->ActorHasTag(TEXT("AntSponge")) || It->ActorHasTag(TEXT("AntSinks")) || It->ActorHasTag(TEXT("AntFloat"));
			if (!bProp && !It->IsA<AKitchenLoot>() && !It->IsA<AAntCharacter>()) { continue; }
			FSample S;
			if (!SampleAt(It->GetActorLocation(), S) || S.Total < 0.3f) { continue; }
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID track %s at %s v %s | surface %.0f depth %.1f current (%.0f, %.0f)%s"), *It->GetName(), *It->GetActorLocation().ToCompactString(),
				*It->GetVelocity().ToCompactString(), S.Surface, S.Total, S.Vel.X, S.Vel.Y,
				It->IsA<AAntCharacter>() ? (Cast<AAntCharacter>(*It)->GetCharacterMovement()->MovementMode == MOVE_Flying ? TEXT(" SWIMMING") : TEXT(" walking")) : TEXT(""));
		}
	}
	else if (Op == TEXT("dump")) { DumpNiagara(FlipHose); DumpNiagara(FlipPool); DumpNiagara(FlipSplash); }
	else if (Op == TEXT("bench"))
	{
		// fluid bench: the cost of layer A off the game thread's frame: N steps of every basin, threaded and on one thread, the same result?
		const int32 N = (int32)NumAt(A, 2, 120.f);
		for (TUniquePtr<FBasin>& Bp : Basins)
		{
			FBasin C1 = *Bp, C2 = *Bp;
			C1.bParallel = true; C2.bParallel = false;
			const double T0 = FPlatformTime::Seconds();
			for (int32 i = 0; i < N; ++i) { C1.Step(); C1.Spills.Reset(); }
			const double T1 = FPlatformTime::Seconds();
			for (int32 i = 0; i < N; ++i) { C2.Step(); C2.Spills.Reset(); }
			const double T2 = FPlatformTime::Seconds();
			TArray<uint8> Snap; Bp->WriteSnapshot(Snap);
			const double T3 = FPlatformTime::Seconds();
			FBasin C3 = *Bp; const bool bRead = C3.ReadSnapshot(Snap, 1.f);
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID bench %s: %d steps, threaded %.3f ms/step, one thread %.3f ms/step, hashes %08x %08x %s; wet box water %dx%d; snapshot %d bytes (coarse %d) written in %.2f ms, read back %s, water after read %.1f l vs %.1f l"),
				*Bp->Name, N, (T1 - T0) * 1000. / N, (T2 - T1) * 1000. / N, C1.Hash(), C2.Hash(), C1.Hash() == C2.Hash() ? TEXT("SAME") : TEXT("DIFFERENT"),
				Bp->Layers[LWater].X1 - Bp->Layers[LWater].X0, Bp->Layers[LWater].Y1 - Bp->Layers[LWater].Y0, Snap.Num(), Bp->SnapCoarse, (T3 - T2) * 1000., bRead ? TEXT("ok") : TEXT("FAILED"),
				C3.Layers[LWater].Volume / Litre, Bp->Layers[LWater].Volume / Litre);
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("ANTFLUID unknown: %s"), *FString::Join(A, TEXT(" "))); }
	return true;
}

void UAntFluidWorld::LogStats()
{
	UWorld* W = GetWorld();
	FVector2D VP(0.0, 0.0);
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(VP); }
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID stats t=%.1f (%.0fx%.0f): frame %.2f ms, gpu %.2f ms | sim %.3f ms, couple %.3f (props %.3f, ants %.3f), drops %.3f, view %.3f (game thread) | falls %d, columns %d, 3D volumes %d, drops %d, lost %.2f l, quality %d"),
		W->GetTimeSeconds(), VP.X, VP.Y, FrameMs, GpuMs, SimMs, CoupleMs, PropsMs, AntsMs, DropMs, ViewMs, Falls.Num(), Columns.Num(), Volumes.Num(), Drops.Num(), LostVolume / Litre, Quality);
	const float Since = FMath::Max(0.1f, W->GetTimeSeconds() - SpillStatAt);
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID   over the edges: %.2f l/s (%s)"), (float)(SpilledVol / Litre) / Since, bAllowHalf ? TEXT("30 Hz allowed") : TEXT("60 Hz"));
	SpilledVol = 0.; SpillStatAt = W->GetTimeSeconds();
	for (const TUniquePtr<FBasin>& B : Basins)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID   %s: honey %.2f l (%d wet), water %.2f l (%d wet, box %dx%d, running tiles %d of %d), oil %.3f l (%d wet), step %u, uploaded tiles %d"), *B->Name,
			B->Layers[LHoney].Volume / Litre, B->WetCells(LHoney), B->Layers[LWater].Volume / Litre, B->WetCells(LWater),
			B->Layers[LWater].X1 - B->Layers[LWater].X0, B->Layers[LWater].Y1 - B->Layers[LWater].Y0, B->RunTiles[LWater], B->TX * B->TY,
			B->Layers[LOil].Volume / Litre, B->WetCells(LOil), B->StepCount, UploadTiles);
	}
	for (const FColumn& C : Columns)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID   column %s -> %s, %.2f l/s"), *C.Top.ToCompactString(), *C.Bottom.ToCompactString(), C.Rate / Litre);
	}
}

void UAntFluidWorld::RunDemo(int32 N)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (!bDemoStarted)
	{
		bDemoStarted = true;
		DemoT0 = Now;
		DemoScript.Reset();
		auto At = [this](float T, const TCHAR* Cmd) { DemoScript.Add(TPair<float, FString>(T, Cmd)); };
		FString Extra;
		FParse::Value(FCommandLine::Get(), TEXT("AntFluidScript="), Extra, false);   // "t:cmd;t:cmd" added to the demo
		TArray<FString> Parts;
		Extra.ParseIntoArray(Parts, TEXT(";"));
		for (const FString& P : Parts) { FString L, R; if (P.Split(TEXT(":"), &L, &R)) { DemoScript.Add(TPair<float, FString>(FCString::Atof(*L), R)); } }
		if (N == 1)
		{
			// D1: tap -> sink -> over the rim -> along the worktop -> off the edge -> the floor, round the furniture, to the door
			At(0.f, TEXT("fluid map 0"));
			At(0.f, TEXT("fluid map 1"));
			At(0.5f, TEXT("fluid tap on 8"));
		}
		else if (N == 2)
		{
			// D2: things float and drift in the full sink; an ant rides the sponge, then swims and is carried
			At(0.5f, TEXT("fluid tap on 10"));
			At(11.f, TEXT("fluid tap on 1.5"));
			At(11.5f, TEXT("fluid prop sponge -3600 3750 1720"));
			At(11.5f, TEXT("fluid prop block -3250 3650 1720"));
			At(11.5f, TEXT("fluid prop brick -3800 4150 1720"));
			At(12.f, TEXT("fluid loot 3 -3350 4050 1720"));
			At(15.f, TEXT("fluid ant sponge"));
			At(20.f, TEXT("fluid track"));
			At(23.f, TEXT("fluid ant sponge"));
			At(16.f, TEXT("fluid track"));
			At(26.f, TEXT("fluid track"));
			At(36.f, TEXT("fluid ant -3250 4250 1650"));
			At(38.f, TEXT("fluid track"));
			At(46.f, TEXT("fluid track"));
			At(56.f, TEXT("fluid track"));
		}
		else if (N == 3)
		{
			// D3: the Human walks through a puddle on the floor; a jar falls into it
			At(0.5f, TEXT("fluid spill water -3000 1100 20 120 450"));
			At(6.f, TEXT("fluid human -3000 -2000 90 -3000 3200 0.45"));
			At(20.f, TEXT("fluid prop block -2700 1100 1300 140 140 170"));
			At(9.f, TEXT("fluid stats"));
			At(16.f, TEXT("fluid stats"));
			At(24.f, TEXT("fluid stats"));
		}
		else if (N == 4)
		{
			// D4: an oil film (things slide on it) and a honey blob (slow, it holds what it touches)
			At(0.5f, TEXT("fluid spill oil -1500 -1500 20 1.5 250"));
			At(0.5f, TEXT("fluid spill honey 900 -1500 20 5 150"));
			At(8.f, TEXT("fluid prop block -1900 -1500 60"));
			At(8.5f, TEXT("fluid shove 600 0"));
			At(8.f, TEXT("fluid prop block -1900 -2400 60"));
			At(8.5f, TEXT("fluid shove 600 0"));
			At(9.f, TEXT("fluid ant 900 -1500 60"));
			At(12.f, TEXT("fluid track"));
			At(20.f, TEXT("fluid track"));
			At(30.f, TEXT("fluid track"));
			At(31.f, TEXT("fluid stats"));
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID demo %d: %d scripted commands"), N, DemoScript.Num());
	}
	const float T = Now - DemoT0;
	for (int32 i = 0; i < DemoScript.Num(); )
	{
		if (DemoScript[i].Key <= T)
		{
			const FString Cmd = DemoScript[i].Value;
			DemoScript.RemoveAt(i);
			TArray<FString> A; Cmd.ParseIntoArrayWS(A);
			UE_LOG(LogTemp, Display, TEXT("ANTFLUID demo %.1f: %s"), T, *Cmd);
			DevCommand(A);
		}
		else { ++i; }
	}
}

// =============================================================================================
// Layer B (3): falling liquid as refracting streams. The tap's jet and every pour off an edge that layer A reports (Columns):
// a chain of cylinder segments along the ballistic path, as thick as the flow. Cheap (one instanced mesh), always on.
// =============================================================================================

void UAntFluidWorld::DrawStreams()
{
	if (!StreamMesh) { return; }
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	const float G = FMath::Abs(W->GetGravityZ());
	TArray<FTransform> Xf;
	TArray<float> XfKind;
	TArray<FTransform> Spray;
	TArray<float> SprayKind;
	auto Hash = [](float X) { return FMath::Frac(FMath::Sin(X * 12.9898f) * 43758.5453f); };
	// One falling stream or sheet: a chain of segments along the ballistic path, thinning as it speeds up (continuity), twisting a
	// little; its lower part breaks into drops (Plateau-Rayleigh), and where it lands a crown of droplets jumps up and falls back.
	// Width0 > 0: a sheet off an edge (flat, Width0 wide across EdgeDir), else a round jet of radius R0.
	auto Chain = [&](const FVector& Top, const FVector2f& Vel, float Bottom, float R0, float Width0, const FVector& EdgeDir, int32 Seg, float Phase, uint8 Liquid)
	{
		const float H = FMath::Max(1.f, (float)Top.Z - Bottom);
		const float Dur = FMath::Sqrt(2.f * H / G);
		const float Solid = FMath::Clamp(0.55f + R0 * 0.012f, 0.55f, 0.85f);   // the part of the fall that is still one body of water
		FVector Prev = Top;
		for (int32 s = 1; s <= Seg; ++s)
		{
			const float A = (float)s / Seg;
			if (A > Solid + 0.5f / Seg) { break; }
			const float T = Dur * A;
			const FVector Wob(FMath::Sin(Now * 7.f + A * 9.f + Phase) * R0 * 0.25f * A, FMath::Cos(Now * 6.1f + A * 7.f + Phase) * R0 * 0.25f * A, 0.f);
			const FVector P(Top.X + Vel.X * T + Wob.X, Top.Y + Vel.Y * T + Wob.Y, Top.Z - 0.5f * G * T * T);
			const FVector D = P - Prev;
			const float Len = (float)D.Size();
			if (Len > 0.5f)
			{
				const float Am = A - 0.5f / Seg;
				const float Thin = 1.f / FMath::Sqrt(FMath::Sqrt(1.f + 2.f * G * H * Am / FMath::Max(1.f, Vel.SizeSquared() + 4000.f)));   // v^-1/2
				const float R = R0 * Thin * (1.f + 0.1f * FMath::Sin(Now * 31.f + s * 1.7f + Phase));
				if (Width0 > 0.f)
				{
					// a sheet: wide across the edge, a few uu thick; it narrows as it falls (surface tension pulls it into strands)
					const float Wd = FMath::Max(R * 2.f, Width0 * FMath::Lerp(1.f, 0.35f, Am));
					const FQuat Q = FRotationMatrix::MakeFromZX(D / Len, EdgeDir).ToQuat();
					Xf.Add(FTransform(Q, (P + Prev) * 0.5, FVector(Wd * 0.5f / 50.f, FMath::Max(1.2f, R * 0.35f) / 50.f, Len * 1.06f / 100.f)));
				}
				else
				{
					const FQuat Q = FRotationMatrix::MakeFromZ(D / Len).ToQuat();
					Xf.Add(FTransform(Q, (P + Prev) * 0.5, FVector(R / 50.f, R / 50.f, Len * 1.06f / 100.f)));
				}
				XfKind.Add(Liquid == Cola ? 1.f : 0.f);
			}
			Prev = P;
		}
		if (Quality <= 0) { return; }
		// the broken lower part: drops strung along the path, each falling and replaced (a pattern that moves down with the flow)
		const int32 NDrops = Quality >= 2 ? (Width0 > 0.f ? 14 : 10) : 5;
		for (int32 k = 0; k < NDrops; ++k)
		{
			const float Seed = Phase * 3.1f + k * 17.13f;
			const float Ph = FMath::Frac(Now * 1.6f + Hash(Seed));
			const float A = FMath::Lerp(Solid - 0.08f, 1.f, Ph);
			const float T = Dur * A;
			const float Side = (Hash(Seed + 1.f) - 0.5f) * (Width0 > 0.f ? Width0 * 0.6f : R0 * 2.5f);
			const FVector Off = (Width0 > 0.f ? EdgeDir : FVector(FMath::Cos(Seed), FMath::Sin(Seed), 0.f)) * Side;
			const FVector P(Top.X + Vel.X * T + Off.X, Top.Y + Vel.Y * T + Off.Y, Top.Z - 0.5f * G * T * T);
			const float Rd = FMath::Max(1.2f, R0 * FMath::Lerp(0.35f, 0.75f, Hash(Seed + 2.f)) * (Width0 > 0.f ? 0.6f : 1.f));
			const float Vz = G * T;
			const float Stretch = FMath::Clamp(1.f + Vz / 2500.f, 1.f, 1.8f);   // a falling drop looks drawn out at this speed
			Spray.Add(FTransform(FQuat::Identity, P, FVector(Rd / Stretch, Rd / Stretch, Rd * Stretch) / 50.f));
			SprayKind.Add(Liquid == Cola ? 1.f : 0.f);
		}
		// the crown where it lands: droplets thrown up and out on short arcs, a new set every half second
		const FVector Land(Top.X + Vel.X * Dur, Top.Y + Vel.Y * Dur, Bottom);
		const int32 NCrown = Quality >= 2 ? 12 : 5;
		const float Spread = (Width0 > 0.f ? Width0 * 0.5f : R0 * 2.f) + 20.f;
		for (int32 k = 0; k < NCrown; ++k)
		{
			const float Seed = Phase * 5.7f + k * 3.77f;
			const float Ph = FMath::Frac(Now * 2.2f + Hash(Seed));
			const float Ang = Hash(Seed + 4.f) * 6.2832f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			const FVector Start = Land + (Width0 > 0.f ? EdgeDir * (Hash(Seed + 5.f) - 0.5f) * Width0 * 0.8f : FVector::ZeroVector);
			const float Out = Spread * (0.3f + 1.2f * Ph) * (0.6f + 0.8f * Hash(Seed + 6.f));
			const float Up = FMath::Sin(Ph * PI) * (25.f + R0 * 3.f) * (0.5f + Hash(Seed + 7.f));
			const float Rd = FMath::Max(0.9f, R0 * 0.3f * (1.f - Ph * 0.6f) * (0.6f + 0.8f * Hash(Seed + 8.f)));
			Spray.Add(FTransform(FQuat::Identity, Start + Dir * Out + FVector(0.f, 0.f, Up + Rd), FVector(Rd) / 50.f));
			SprayKind.Add(Liquid == Cola ? 1.f : 0.f);
		}
	};
	if (Quality > 0)
	{
		for (const FSource& S : Sources)
		{
			if (!S.bOn || S.Liquid == Honey) { continue; }
			FSample Smp;
			const float Bottom = SampleAt(S.Pos - FVector(0.f, 0.f, 50.f), Smp) ? Smp.Surface : (float)S.Pos.Z - 600.f;
			// a tap's jet: about as wide as the aerator (a few litres a second is 1-2 cm across at the spout)
			const float R0 = FMath::Clamp(FMath::Sqrt(S.Rate / Litre) * 5.f, 3.f, 22.f);
			Chain(S.Pos, FVector2f::ZeroVector, Bottom, R0, 0.f, FVector::ForwardVector, Quality >= 2 ? 14 : 7, S.Pos.X * 0.01f, S.Liquid);
		}
		for (const FColumn& C : Columns)
		{
			if (C.Rate < 0.01f * Litre || Now - C.Seen > 0.6f) { continue; }
			bool bTap = false;
			for (const FSource& S : Sources) { bTap |= FVector::DistSquared2D(S.Pos, C.Top) < 100.f * 100.f; }
			if (bTap) { continue; }
			// liquid over an edge: a sheet across the edge (perpendicular to how it flows off), as wide as the stretch it pours from
			const FVector Flow = FVector(C.Vel.X, C.Vel.Y, 0.f).GetSafeNormal();
			const FVector Edge = Flow.IsNearlyZero() ? FVector::RightVector : FVector(-Flow.Y, Flow.X, 0.f);
			const float L = C.Rate / Litre;
			const float R0 = FMath::Clamp(FMath::Sqrt(L) * 4.f, 1.5f, 14.f);
			const float Wd = FMath::Clamp(L * 60.f + 20.f, 24.f, 150.f);
			Chain(C.Top, C.Vel, (float)C.Bottom.Z, R0, Wd, Edge, Quality >= 2 ? 10 : 5, C.Top.X * 0.01f + C.Top.Y * 0.007f, C.Liquid);
		}
	}
	auto Push = [](UInstancedStaticMeshComponent* M, const TArray<FTransform>& T, const TArray<float>* Kind, TArray<float>& LastKind)
	{
		if (!M) { return; }
		bool bNew = false;
		// the instance buffer only grows (rebuilding it whenever a stream starts or stops cost more than all the rest):
		// spare instances sit at zero scale out of sight
		const int32 Have = M->GetInstanceCount();
		if (T.Num() > Have)
		{
			TArray<FTransform> More;
			const int32 Want = FMath::Max(T.Num(), Have + 64);
			More.Init(FTransform(FQuat::Identity, FVector(0.f, 0.f, -10000.f), FVector::ZeroVector), Want - Have);
			M->AddInstances(More, false, true, false);
			bNew = true;
		}
		const int32 N = M->GetInstanceCount();
		if (T.Num() == 0 && LastKind.Num() == 0 && !bNew) { return; }   // nothing falls now, and nothing fell last frame
		if (N > 0)
		{
			TArray<FTransform> All = T;
			if (All.Num() < N) { All.SetNum(N); for (int32 i = T.Num(); i < N; ++i) { All[i] = FTransform(FQuat::Identity, FVector(0.f, 0.f, -10000.f), FVector::ZeroVector); } }
			M->BatchUpdateInstancesTransforms(0, All, true, true, false);
		}
		// the tint per instance only when it changed (it almost never does: a whole stream is water or cola)
		if (Kind && M->NumCustomDataFloats > 0 && (bNew || LastKind != *Kind))
		{
			for (int32 i = 0; i < Kind->Num() && i < M->GetInstanceCount(); ++i) { M->SetCustomDataValue(i, 0, (*Kind)[i], false); }
			M->MarkRenderStateDirty();
			LastKind = *Kind;
		}
	};
	Push(StreamMesh, Xf, &XfKind, LastStreamKind);
	Push(SprayMesh, Spray, &SprayKind, LastSprayKind);
}

// =============================================================================================
// The game (server): the tap is the faucet fixture's, the sink's plughole, the stains water washes, the Human's reaction,
// geysers, the raid reset. docs/FEATURES.md, section 29.
// =============================================================================================

void UAntFluidWorld::TickGame(float Dt)
{
	UWorld* W = GetWorld();
	if (Sources.Num() == 0 || Basins.Num() == 0) { return; }
	if (W->GetNetMode() == NM_Client)
	{
		TickNetClient(Dt);
		TickGeysers(Dt);
		return;
	}
	GameClock += Dt;

	// the tap: the faucet fixture (the valves the ants turn, the Human shuts) says whether it runs
	if (!Faucet.IsValid()) { for (TActorIterator<AKitchenFaucet> It(W); It; ++It) { Faucet = *It; break; } }
	FSource& Tap = Sources[0];
	const bool bWas = Tap.bOn;
	if (AKitchenFaucet* F = Faucet.Get()) { Tap.bOn = F->IsOn(); }
	if (!Tap.bOn) { FloodLeft = 0.f; }
	Tap.Liquid = TapLiquid;
	Tap.Rate = (FloodLeft > 0.f ? FloodRate : TapRate) * Litre;
	if (FloodLeft > 0.f) { FloodLeft = FMath::Max(0.f, FloodLeft - Dt); }
	if (bWas != Tap.bOn) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID tap %s (%.1f l/s)"), Tap.bOn ? TEXT("runs") : TEXT("shut"), Tap.Rate / Litre); }
	// pours that ran out
	for (int32 i = Sources.Num() - 1; i >= 1; --i) { if (!Sources[i].bOn) { Sources.RemoveAt(i); } }

	// the sink's plughole: it takes a little (a plate lies half over it), so a running tap fills the sink and it slops over
	{
		FBasin& B = *Basins[0];
		const FVector Hole(AKitchenFaucet::Basin().GetCenter().X, AKitchenFaucet::Basin().GetCenter().Y, 0.f);
		FSample S;
		B.Sample((float)Hole.X, (float)Hole.Y, S);
		if (S.bInside && S.Depth[LWater] > 0.f && S.Bed < B.Z0 - 100.f)
		{
			const float R = 60.f;
			const float Cells = FMath::Max(1.f, PI * R * R / (B.Dx * B.Dx));
			B.Drain(LWater, (float)Hole.X, (float)Hole.Y, R, SinkDrain * Litre * Dt / (Cells * B.Dx * B.Dx));
		}
	}

	TickGeysers(Dt);
	TickNetServer(Dt);

	// water washes the stains it covers (a few seconds under running or standing water)
	WashClock += Dt;
	if (WashClock >= 0.5f)
	{
		const float Step = WashClock;
		WashClock = 0.f;
		WashStains(Step);
	}

	// the floor is flooding: the Dad notices it and goes to shut the tap
	if (Basins.Num() > 1 && Tap.bOn && GameClock >= NextYell && LitresIn(1) > 2.5f)
	{
		NextYell = GameClock + 90.f;
		static const TCHAR* Lines[] = { TEXT("Кто кран не закрыл?!"), TEXT("Это что за потоп?!"), TEXT("Опять вода на полу!") };
		NpcRoutine::Say(W, NpcRoutine::Dad, Lines[FMath::RandRange(0, 2)]);
		if (AKitchenFaucet* F = Faucet.Get()) { F->Alarm(); }
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID the floor floods (%.1f l): the Dad goes to shut the tap"), LitresIn(1));
		AntFighter::OnFlood();   // «Водопроводчик» (AntFighter.h)
	}

	if (bDebug && GameClock >= DebugAt && GEngine)
	{
		DebugAt = GameClock + 0.5f;
		GEngine->AddOnScreenDebugMessage(0x5A7E, 0.6f, FColor(120, 200, 255), Describe());
	}
}

void UAntFluidWorld::TickGeysers(float Dt)
{
	// cola + mentos: a fountain of drops, the pool under the bottle, the fallout all round (every machine: the drops are cosmetic,
	// the liquid they carry is the same on every machine to within the snapshot's correction)
	UWorld* W = GetWorld();
	const float G = FMath::Abs(W->GetGravityZ());
	for (int32 i = Geysers.Num() - 1; i >= 0; --i)
	{
		FGeyserJet& J = Geysers[i];
		J.Left -= Dt;
		const float Power = FMath::Clamp(J.Left / 4.f, 0.2f, 1.f);
		J.Acc += Dt * 45.f * Power;
		FRandomStream R(J.Seed);
		while (J.Acc >= 1.f)
		{
			J.Acc -= 1.f;
			J.Seed = R.RandHelper(0x7fffffff);
			const float H = R.FRandRange(1600.f, 2900.f) * Power;                 // how high this lump goes
			const float Up = FMath::Sqrt(2.f * G * H);
			const float Ang = R.FRandRange(0.f, 2.f * PI), Tilt = R.FRandRange(0.f, 0.22f);
			const FVector Dir = FVector(FMath::Cos(Ang) * Tilt, FMath::Sin(Ang) * Tilt, 1.f).GetSafeNormal();
			AddDrop(J.At + FVector(0.f, 0.f, 40.f), R.FRandRange(4.f, 10.f), (ELiquid)J.Liquid, Dir * Up);
			// what it carries lands where a lump like that comes down: a short fall to the floor, then the pool spreads
			const float Air = 2.f * Up * Dir.Z / G;
			const FVector Land = J.At + FVector(Dir.X, Dir.Y, 0.f) * Up * Air;
			Spill((ELiquid)J.Liquid, FVector(Land.X, Land.Y, J.At.Z + 300.f), 0.045f * Litre * Power, 60.f);
		}
		Spill((ELiquid)J.Liquid, J.At + FVector(0.f, 0.f, 30.f), 0.35f * Litre * Dt * Power, 50.f);   // it runs down the bottle
		if (J.Left <= 0.f) { Geysers.RemoveAtSwap(i); }
	}
}

void UAntFluidWorld::WashStains(float Step)
{
	// water washes the stains it covers (a few seconds under running or standing water)
	AAntMess* M = AAntMess::Get(this);
	if (!M) { return; }
	TArray<TPair<FVector, float>> Wash;
	TArray<FMessStain> Dough;
	TSet<uint16> Seen;
	for (const FMessStain& S : M->Stains)
	{
		if (S.Kind == MessKind::Water || S.Kind == MessKind::Scratch) { continue; }
		FSample Smp;
		const FVector P = S.Pos;
		if (!SampleAt(P, Smp) || Smp.Depth[LWater] < 1.2f || Smp.Dye > 0.5f || FMath::Abs(Smp.Bed - (float)P.Z) > 40.f) { continue; }
		Seen.Add(S.Id);
		float& T = StainWet.FindOrAdd(S.Id);
		T += Step * FMath::Clamp(Smp.Depth[LWater] / 5.f + Smp.Vel.Size() / 250.f, 0.4f, 3.f);
		// flour does not wash away: with water it turns into a sticky dough where it lies (KAN-21)
		if (S.Kind == MessKind::Flour) { if (T > 2.f) { Dough.Add(S); } continue; }
		if (T > 4.f) { Wash.Add(TPair<FVector, float>(P, FMath::Max(20.f, S.Size * 0.15f))); }
	}
	for (const FMessStain& D : Dough)
	{
		const FVector P = D.Pos;
		M->Clean(P, FMath::Max(30.f, D.Size * 0.45f));
		AntMess::StainAt(this, MessKind::Cream, P, D.Normal, FMath::Max(60.f, D.Size * 0.7f), D.Seed);
		LevelV3::AddZone(0, P, FMath::Max(60.f, D.Size * 0.4f));
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID flour + water at %s: a sticky dough (sticky zone r %.0f)"), *P.ToCompactString(), FMath::Max(60.f, D.Size * 0.4f));
	}
	for (auto It = StainWet.CreateIterator(); It; ++It) { if (!Seen.Contains(It.Key())) { It.RemoveCurrent(); } }
	for (const TPair<FVector, float>& Wp : Wash)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID water washed a stain away at %s"), *Wp.Key.ToCompactString());
		M->Clean(Wp.Key, Wp.Value);
	}
}

// ---------------------------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------------------------

AAntFluidNet::AAntFluidNet()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(60.f);   // the snapshot's pieces leave a couple per update
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AAntFluidNet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntFluidNet, TapRate);
	DOREPLIFETIME(AAntFluidNet, TapLiquid);
	DOREPLIFETIME(AAntFluidNet, Pours);
}

void AAntFluidNet::MulticastSnap_Implementation(uint8 Basin, uint16 Seq, uint8 Part, uint8 Parts, const TArray<uint8>& Data)
{
	if (HasAuthority()) { return; }
	if (UAntFluidWorld* F = UAntFluidWorld::Get(this)) { F->ClientSnap(Basin, Seq, Part, Parts, Data); }
}

void AAntFluidNet::MulticastGeyser_Implementation(FVector_NetQuantize At, float Seconds, uint8 Liquid)
{
	if (HasAuthority()) { return; }
	if (UAntFluidWorld* F = UAntFluidWorld::Active(this)) { F->Geyser(At, Seconds, (ELiquid)Liquid); }
}

void AAntFluidNet::MulticastSpill_Implementation(uint8 Liquid, FVector_NetQuantize At, float Volume, float Radius)
{
	if (HasAuthority()) { return; }
	if (UAntFluidWorld* F = UAntFluidWorld::Active(this)) { F->Spill((ELiquid)Liquid, At, Volume, Radius); }
}

void AAntFluidNet::MulticastDry_Implementation()
{
	if (HasAuthority()) { return; }
	if (UAntFluidWorld* F = UAntFluidWorld::Active(this)) { F->Dry(); }
}

void UAntFluidWorld::TickNetServer(float Dt)
{
	UWorld* W = GetWorld();
	if (W->GetNetMode() != NM_ListenServer && W->GetNetMode() != NM_DedicatedServer) { return; }
	AAntFluidNet* N = Net.Get();
	if (!N)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		N = W->SpawnActor<AAntFluidNet>(AAntFluidNet::StaticClass(), FTransform::Identity, SP);
		Net = N;
		if (!N) { return; }
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID net: the water is sent to clients"));
	}
	// what every client runs itself: the tap's flow and the pours
	const FSource& Tap = Sources[0];
	if (N->TapRate != Tap.Rate) { N->TapRate = Tap.Rate; }
	if (N->TapLiquid != Tap.Liquid) { N->TapLiquid = Tap.Liquid; }
	TArray<FAntFluidPour> P;
	for (int32 i = 1; i < Sources.Num(); ++i)
	{
		if (!Sources[i].bOn) { continue; }
		FAntFluidPour Q; Q.Pos = Sources[i].Pos; Q.Rate = Sources[i].Rate; Q.Radius = Sources[i].Radius; Q.Liquid = Sources[i].Liquid;
		P.Add(Q);
	}
	if (P != N->Pours) { N->Pours = P; }
	// The pieces go out reliably, one every other frame (~30 kB/s at most, a few kB/s usually): sent all at once, or unreliably,
	// a flooded floor's 14 pieces never all arrived (checked: the 6-piece worktop did, the floor never).
	if ((GFrameCounter & 1) == 0 && SendQueue.Num() > 0) { return; }
	for (int32 n = 0; n < 1 && SendQueue.Num() > 0; ++n)
	{
		const FSnapOut& P0 = SendQueue[0];
		N->MulticastSnap(P0.Basin, P0.Seq, P0.Part, P0.Parts, P0.Data);
		SendQueue.RemoveAt(0);
	}
	if (SendQueue.Num() > 0) { N->ForceNetUpdate(); return; }
	// the server's picture, one basin a second, coarse (4 x 4 cells), cut into pieces an unreliable message carries
	SnapClock += Dt;
	if (SnapClock < 1.f || Basins.Num() == 0) { return; }
	SnapClock = 0.f;
	const int32 b = SnapTurn++ % Basins.Num();
	FBasin& B = *Basins[b];
	if (B.TotalVolume() <= 0. && (SnapTurn / Basins.Num()) % 5 != 0) { return; }   // a dry basin: now and then, so a client's stray water dries too
	const int32 C0 = B.SnapCoarse;
	B.SnapCoarse = B.Nx * B.Ny > 150000 ? 6 : 4;   // the floor in blocks of 15 cm, the worktop in 8 cm: the mass is corrected, the fine shape is the client's own
	TArray<uint8> Data;
	B.WriteSnapshot(Data);
	B.SnapCoarse = C0;
	if (Data.Num() == 0) { return; }
	const int32 Piece = 1000;
	const int32 Parts = (Data.Num() + Piece - 1) / Piece;
	if (Parts > 250) { UE_LOG(LogTemp, Warning, TEXT("ANTFLUID net: the %s picture is too big (%d bytes)"), *B.Name, Data.Num()); return; }
	++SnapSeq;
	for (int32 p = 0; p < Parts; ++p)
	{
		FSnapOut& O = SendQueue.AddDefaulted_GetRef();
		O.Basin = (uint8)b; O.Seq = SnapSeq; O.Part = (uint8)p; O.Parts = (uint8)Parts;
		O.Data.Append(Data.GetData() + p * Piece, FMath::Min(Piece, Data.Num() - p * Piece));
	}
	if (LogOn()) { UE_LOG(LogTemp, Display, TEXT("ANTFLUID net: sent %s, %d bytes in %d pieces (seq %d)"), *B.Name, Data.Num(), Parts, SnapSeq); }
}

void UAntFluidWorld::TickNetClient(float Dt)
{
	UWorld* W = GetWorld();
	if (!Net.IsValid()) { for (TActorIterator<AAntFluidNet> It(W); It; ++It) { Net = *It; break; } }
	if (!Faucet.IsValid()) { for (TActorIterator<AKitchenFaucet> It(W); It; ++It) { Faucet = *It; break; } }
	AAntFluidNet* N = Net.Get();
	// the same inputs as the server: the faucet (replicated), its flow and the pours
	FSource& Tap = Sources[0];
	Tap.bOn = Faucet.IsValid() && Faucet->IsOn();
	if (N) { Tap.Rate = N->TapRate; Tap.Liquid = N->TapLiquid; }
	Sources.SetNum(1);
	if (N)
	{
		for (const FAntFluidPour& Q : N->Pours)
		{
			FSource S; S.Id = TEXT("pour"); S.Pos = Q.Pos; S.Rate = Q.Rate; S.Radius = Q.Radius; S.Liquid = Q.Liquid; S.bOn = true; S.Left = -1.f;
			Sources.Add(S);
		}
	}
}

void UAntFluidWorld::ClientSnap(uint8 Basin, uint16 Seq, uint8 Part, uint8 Parts, const TArray<uint8>& Data)
{
	if (!bReady || !Basins.IsValidIndex(Basin) || Parts == 0 || Part >= Parts) { return; }
	FSnapIn& In = SnapIn.FindOrAdd(Basin);
	if (In.Seq != Seq || In.Parts != Parts) { In.Seq = Seq; In.Parts = Parts; In.Got = 0; In.Chunks.Reset(); In.Chunks.SetNum(Parts); }
	if (In.Chunks[Part].Num() > 0) { return; }
	In.Chunks[Part] = Data;
	if (++In.Got < In.Parts) { return; }
	TArray<uint8> All;
	for (const TArray<uint8>& C : In.Chunks) { All.Append(C); }
	// the first picture is taken whole (a late joiner sees the flood at once); after that it eases the local flow towards it
	const bool bOk = Basins[Basin]->ReadSnapshot(All, In.bHadFirst ? 0.6f : 1.f);
	In.bHadFirst |= bOk;
	In.Chunks.Reset(); In.Got = 0;
	++SnapsRead;
	if (LogOn() || SnapsRead <= 2)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFLUID net: read %s (%d bytes) %s, it holds %.1f l"), *Basins[Basin]->Name, All.Num(), bOk ? TEXT("ok") : TEXT("FAILED"), LitresIn(Basin));
	}
}

void UAntFluidWorld::AddSource(ELiquid Liquid, const FVector& At, float LitresPerSecond, float Seconds, float Radius)
{
	if (!bReady) { return; }
	FSource S; S.Id = FName(*FString::Printf(TEXT("src%d"), Sources.Num())); S.Pos = At;
	S.Liquid = Liquid; S.Rate = LitresPerSecond * Litre; S.Left = S.Rate * Seconds; S.Radius = Radius; S.bOn = true;
	Sources.Add(S);
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID pour of %s at %s: %.2f l/s for %.0f s"), LiquidName(Liquid), *At.ToCompactString(), LitresPerSecond, Seconds);
}

void UAntFluidWorld::Geyser(const FVector& At, float Seconds, ELiquid Liquid)
{
	if (!bReady) { return; }
	FGeyserJet J; J.At = At; J.Left = Seconds; J.Liquid = Liquid; J.Seed = (int32)(At.X * 13.f + At.Y * 7.f) ^ (int32)GFrameCounter;
	Geysers.Add(J);
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) { AntFx::Splash(this, At + FVector(0.f, 0.f, 60.f), 60.f); }
	if (GetWorld()->GetNetMode() != NM_Client && Net.IsValid()) { Net->MulticastGeyser(At, Seconds, (uint8)Liquid); }
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID geyser of %s at %s for %.0f s"), LiquidName(Liquid), *At.ToCompactString(), Seconds);
}

void UAntFluidWorld::Flood(float Seconds, float LitresPerSecond)
{
	FloodLeft = Seconds;
	FloodRate = LitresPerSecond;
	if (AKitchenFaucet* F = Faucet.Get()) { F->SetWater(true, false); }
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID flood: the tap gushes %.0f l/s for %.0f s"), LitresPerSecond, Seconds);
}

float UAntFluidWorld::DepthAt(const FVector& P) const
{
	FSample S;
	return bReady && SampleAt(P, S) ? S.Total : 0.f;
}

float UAntFluidWorld::DepthIn(const FVector& P, float Above) const
{
	FSample S;
	if (!bReady || !SampleAt(P, S) || S.Total <= 0.f) { return 0.f; }
	return (float)P.Z <= S.Surface + Above ? S.Total : 0.f;
}

bool UAntFluidWorld::WaterBodyAt(const FVector& At, float MinDepth, FAntWaterBody& Out, float SeedRadius) const
{
	// (KAN-21 water fix) the liquid joined to the point: a flood fill over the cells of its basin that hold more than MinDepth.
	// A short in a puddle on the floor reaches what stands in that puddle, not the wet worktop over it or a puddle across the room.
	Out = FAntWaterBody();
	if (!bReady) { return false; }
	const int32 b = BasinUnder(At + FVector(0.f, 0.f, 30.f));
	if (b < 0) { return false; }
	const FBasin& B = *Basins[b];
	auto Depth = [&B](int32 c) { return B.Layers[LHoney].H[c] + B.Layers[LWater].H[c] + B.Layers[LOil].H[c]; };
	auto Wet = [&B, &Depth, MinDepth](int32 c) { return B.Kind[c] == Open && Depth(c) > MinDepth; };
	Out.Basin = b;
	Out.Cells.Init(false, B.Nx * B.Ny);
	TArray<int32> Open_;
	Open_.Reserve(4096);
	// The seeds: every wet cell within SeedRadius whose water is level with the point (the thing at At stands on its own footprint,
	// which the bed takes as a wall: the water touching it lies round it, maybe split by its cable into pockets)
	int32 Ic, Jc;
	if (!B.CellAt((float)At.X, (float)At.Y, Ic, Jc)) { return false; }
	const int32 R = FMath::Max(0, FMath::CeilToInt(SeedRadius / B.Dx));
	for (int32 j = FMath::Max(0, Jc - R); j <= FMath::Min(B.Ny - 1, Jc + R); ++j)
	{
		for (int32 i = FMath::Max(0, Ic - R); i <= FMath::Min(B.Nx - 1, Ic + R); ++i)
		{
			if (FMath::Square(i - Ic) + FMath::Square(j - Jc) > R * R + 1) { continue; }
			const int32 c = B.Idx(i, j);
			if (!Wet(c)) { continue; }
			const float Bed = B.Z0 + B.Bed[c];
			if (Bed > (float)At.Z + 30.f || Bed + Depth(c) < (float)At.Z - 30.f) { continue; }   // a shelf of water above it, or a pit far below
			Out.Cells[c] = true; Open_.Add(c); ++Out.Num;
		}
	}
	if (Out.Num == 0) { Out.Basin = -1; return false; }
	const int32 MaxCells = 400000;
	while (Open_.Num() > 0)
	{
		const int32 c = Open_.Pop(EAllowShrinking::No);
		const int32 i = c % B.Nx, j = c / B.Nx;
		const int32 Nb[4] = { i > 0 ? c - 1 : -1, i < B.Nx - 1 ? c + 1 : -1, j > 0 ? c - B.Nx : -1, j < B.Ny - 1 ? c + B.Nx : -1 };
		for (int32 n : Nb)
		{
			if (n < 0 || Out.Cells[n] || !Wet(n)) { continue; }
			Out.Cells[n] = true;
			Open_.Add(n);
			if (++Out.Num >= MaxCells) { Out.bCapped = true; Open_.Reset(); break; }
		}
	}
	return true;
}

bool UAntFluidWorld::InWaterBody(const FAntWaterBody& Body, const FBox& Box, float Tol) const
{
	if (!bReady || !Basins.IsValidIndex(Body.Basin) || Body.Num == 0) { return false; }
	const FBasin& B = *Basins[Body.Basin];
	// the cells under the thing and one ring round it (a static thing's own footprint is a wall cell of the bed: the water is beside it);
	// at most 24 x 24 samples over a big thing
	const float X0 = (float)Box.Min.X - B.Dx, Y0 = (float)Box.Min.Y - B.Dx, X1 = (float)Box.Max.X + B.Dx, Y1 = (float)Box.Max.Y + B.Dx;
	const float StepX = FMath::Max(B.Dx, (X1 - X0) / 24.f), StepY = FMath::Max(B.Dx, (Y1 - Y0) / 24.f);
	for (float Y = Y0; Y <= Y1 + 0.5f; Y += StepY)
	{
		for (float X = X0; X <= X1 + 0.5f; X += StepX)
		{
			int32 I, J;
			if (!B.CellAt(X, Y, I, J)) { continue; }
			const int32 c = B.Idx(I, J);
			if (!Body.Cells[c]) { continue; }
			const float Bed = B.Z0 + B.Bed[c];
			const float Surface = Bed + B.Layers[LHoney].H[c] + B.Layers[LWater].H[c] + B.Layers[LOil].H[c];
			// its bottom is under the surface of that water, and it is not hanging over it from above
			if ((float)Box.Min.Z <= Surface + Tol && (float)Box.Max.Z >= Bed - Tol) { return true; }
		}
	}
	return false;
}

bool UAntFluidWorld::IsSlippery(const FVector& P) const
{
	if (AntMatter::IceAt(GetWorld(), P, 60.f)) { return true; }   // a frozen puddle (AntMatter): slipperier than the water was
	FSample S;
	if (!bReady || !SampleAt(P, S) || FMath::Abs((float)P.Z - S.Bed) > 60.f) { return false; }
	return S.Depth[LWater] > 0.8f || S.Depth[LOil] > 0.03f;
}

void UAntFluidWorld::Mop(const FVector& At, float Radius, float Dt)
{
	const int32 b = bReady ? BasinUnder(At + FVector(0.f, 0.f, 30.f)) : -1;
	if (b < 0) { return; }
	for (int32 L = 0; L < LCount; ++L) { Basins[b]->Drain((ELayer)L, (float)At.X, (float)At.Y, Radius, 6.f * Dt); }
}

void UAntFluidWorld::DrainLayer(const FVector& At, float Radius, uint8 Layer, float Depth)
{
	const int32 b = bReady && Layer < LCount ? BasinUnder(At + FVector(0.f, 0.f, 30.f)) : -1;
	if (b < 0 || Depth <= 0.f) { return; }
	Basins[b]->Drain((ELayer)Layer, (float)At.X, (float)At.Y, Radius, Depth);
}

float UAntFluidWorld::LitresIn(int32 b) const
{
	return Basins.IsValidIndex(b) ? (float)(Basins[b]->TotalVolume() / Litre) : 0.f;
}

void UAntFluidWorld::Dry()
{
	for (TUniquePtr<FBasin>& B : Basins) { B->Clear(); }
	for (const TPair<int32, float>& F : Filled)
	{
		// (a tub that starts full is full again)
		if (!Basins.IsValidIndex(F.Key)) { continue; }
		FBasin& B = *Basins[F.Key];
		FLayer& Ly = B.Layers[LWater];
		double Vol = 0.;
		for (int32 c = 0; c < B.Kind.Num(); ++c)
		{
			const float Depth = F.Value - (B.Z0 + B.Bed[c]);
			if (B.Kind[c] == Open && B.Bed[c] < -50.f && Depth > 0.f) { Ly.H[c] = Depth; Vol += (double)Depth * B.Dx * B.Dx; }
		}
		Ly.X0 = 0; Ly.Y0 = 0; Ly.X1 = B.Nx; Ly.Y1 = B.Ny;
		Ly.Volume = Vol;
		B.WakeAll();
	}
	Falls.Reset(); Pending.Reset(); LandCache.Reset(); Drops.Reset(); Columns.Reset(); Geysers.Reset(); StainWet.Reset();
	if (Sources.Num() > 1) { Sources.SetNum(1); }
	FloodLeft = 0.f;
	LostVolume = 0.;
	if (GetWorld()->GetNetMode() != NM_Client && Net.IsValid()) { Net->MulticastDry(); }
	UE_LOG(LogTemp, Display, TEXT("ANTFLUID everything dried"));
}

void UAntFluidWorld::ResetAll(UWorld* World)
{
	if (UAntFluidWorld* F = Active(World)) { F->Dry(); F->TapRate = 5.f; F->TapLiquid = Water; }
}

FString UAntFluidWorld::Describe() const
{
	const bool bTap = Sources.Num() > 0 && Sources[0].bOn;
	FString S = FString::Printf(TEXT("ВОДА: кран %s (%.1f л/с)%s | столешница+раковина %.1f л, пол %.1f л | капель %d, струй %d, гейзеров %d | sim %.2f ms, связь %.2f, капли %.2f, вид %.2f"),
		bTap ? TEXT("ОТКРЫТ") : TEXT("закрыт"), Sources.Num() > 0 ? Sources[0].Rate / Litre : 0.f, FloodLeft > 0.f ? TEXT(" ПОТОП") : TEXT(""),
		LitresIn(0), LitresIn(1), Drops.Num(), Columns.Num(), Geysers.Num(), SimMs, CoupleMs, DropMs, ViewMs);
	for (const TUniquePtr<FBasin>& B : Basins)
	{
		const FLayer& Ly = B->Layers[LWater];
		S += FString::Printf(TEXT("\n  %s: мокрых клеток %d, рамка %dx%d"), *B->Name, B->WetCells(LWater), Ly.X1 - Ly.X0, Ly.Y1 - Ly.Y0);
	}
	return S;
}
