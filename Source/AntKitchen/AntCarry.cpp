#include "AntCarry.h"
#include "KitchenLoot.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntArmy.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntBotController.h"
#include "HumanCharacter.h"
#include "AntTypes.h"
#include "AntFx.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "AntFighter.h"

// =============================================================================================
// Tuning (all in one place; -AntCarryK=, -AntCarryBrake= ... override for experiments)
// =============================================================================================
namespace
{
	float Opt(const TCHAR* Key, float Def) { float V = Def; FParse::Value(FCommandLine::Get(), Key, V); return V; }

	struct FTune
	{
		float K = Opt(TEXT("AntCarryK="), 40.f);              // grip spring, 1/s^2 (per unit of the carrier's mass share)
		float C = Opt(TEXT("AntCarryC="), 7.f);               // grip damper, 1/s
		float Reserve = Opt(TEXT("AntCarryReserve="), 1.35f);// a carrier may give 135 % of its strength while the team has slack
		float MainReserve = 1.75f;                            // the cake (it was carried with 1.75 before: over door rails)
		float Brake = Opt(TEXT("AntCarryBrake="), 0.15f);    // braking: the feet slip - friction 0.15 of what presses them down
		float SideMu = Opt(TEXT("AntCarrySide="), 0.25f);    // turning a running load: friction 0.25 (claws sideways)
		float AntW = 0.15f;                                   // an ant's own weight, in weight units (it brakes even under a crumb)
		float Upright = Opt(TEXT("AntCarryUpright="), 22.f); // how stiffly the jaws keep the load from tipping (1/s^2)
		float Beat = Opt(TEXT("AntCarryBeat="), 55.f);       // units of travel per beat of the carriers' stride
		float BeatAmp = Opt(TEXT("AntCarryBeatAmp="), 0.2f); // how much each carrier's support rises and falls with its stride
		float Entrain = 4.f;                                  // how fast strides lock when they move as one (1/s)
		float TetherK = Opt(TEXT("AntCarryTether="), 10.f);  // how hard the grip pulls the ant back (1/s)
		float TetherSlack = 2.5f;
		float TetherMax = 650.f;
		float YankV = Opt(TEXT("AntCarryYank="), 150.f);     // grip running away from the jaws this fast: a yank
		float TumbleV = Opt(TEXT("AntCarryTumble="), 340.f); // this fast: torn out of the grip, onto its back
		float MaxLoadSpeed = 1250.f;
		float MaxSpin = 600.f;                                // deg/s
		float WorkerSpeed = 480.f * AntKit::SpeedK;          // a worker's pace with a load factor of 1
	};
	const FTune& CT() { static FTune V; return V; }

	enum EBark : uint8 { B_None, B_Yank, B_Rhythm, B_Hold, B_Ouch, B_Heavy, B_Together, B_RunOver, B_Mine, B_Count };
	const TCHAR* BarkText(uint8 B)
	{
		static const TCHAR* Tx[B_Count] = { TEXT(""), TEXT("Не дёргай!"), TEXT("Раз-два, взяли!"), TEXT("Держите!"), TEXT("Ай!"),
			TEXT("Тяжело! Ещё лапы!"), TEXT("Тянем вместе!"), TEXT("Ой, переехали!"), TEXT("Отдай, это наше!") };
		return B < B_Count ? Tx[B] : TEXT("");
	}
	float Frac(float X) { return X - FMath::FloorToFloat(X); }
	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }
}

/** Per-load state that is not replicated (server: carriers; clients: snapshots). */
struct FAntCarryState
{
	struct FMate
	{
		TWeakObjectPtr<AAntCharacter> Ant;
		TWeakObjectPtr<AAntArmy> Army;
		int32 Worker = -1;
		TWeakObjectPtr<AAntCharacter> Leader;   // workers that help an ant follow it
		FVector WLocal = FVector::ZeroVector;    // workers: grip in the load's space (ants keep theirs in AKitchenLoot::GrabLocal)
		FVector Jaw = FVector::ZeroVector;       // workers: the virtual jaws (world)
		FVector JawV = FVector::ZeroVector;
		FVector Desired = FVector::ZeroVector;   // world velocity it wants (units/s)
		bool bExplicit = false;                  // Desired came from an order
		float Phase = 0.f;
		FVector Vrel = FVector::ZeroVector;
		FVector Pull = FVector::ZeroVector;
		float Strain = 0.f, Share = 0.f, Over = 0.f, NextYank = 0.f;
		FVector LastPos = FVector::ZeroVector;   // ants: where it was last tick (its real velocity: input is not motion when the load holds it)
		FVector RealV = FVector::ZeroVector;
		bool bDragged = false, bAtLimit = false;
		float JoinedAt = 0.f;                     // a fresh grip settles for a moment before any yank counts
		float TornT = 0.f;                        // workers: how long it has been pulled too far
		bool IsWorker() const { return Worker >= 0; }
	};
	TArray<FMate> Mates;
	float Sync = 1.f, RhythmT = 0.f, DesyncT = 0.f, HeavyT = 0.f;
	bool bRhythm = false;
	float NextBark = 0.f, NextRhythmBark = 0.f, NextYankToast = 0.f, SendAt = 0.f;
	int32 LastStrongEnough = -1;
	bool bWasActive = false;
	// clients
	FVector UpRef = FVector::UpVector;   // server: which way was up when they took hold (the jaws keep it so)
	struct FSnap { double T; FVector P; FQuat Q; FVector V; };
	TArray<FSnap> Snaps;
	float LastStamp = -1.f;
	double RenderT = 0.0;
	bool bKinematic = false;
	uint8 SeenBarkSeq = 0;
	float BarkShownAt = -100.f;
	uint8 ShownBark = 0, ShownBy = 0;

	FMate* FindAnt(const AAntCharacter* A) { for (FMate& M : Mates) { if (!M.IsWorker() && M.Ant.Get() == A) { return &M; } } return nullptr; }
	FMate* FindWorker(const AAntArmy* Ar, int32 W) { for (FMate& M : Mates) { if (M.IsWorker() && M.Army.Get() == Ar && M.Worker == W) { return &M; } } return nullptr; }
	int32 NumWorkers() const { int32 N = 0; for (const FMate& M : Mates) { N += M.IsWorker() ? 1 : 0; } return N; }
};

/** Reaches into AKitchenLoot (friend). */
struct FAntCarrySolver
{
	static FAntCarryState& St(AKitchenLoot* L) { if (!L->CarryState.IsValid()) { L->CarryState = MakeShared<FAntCarryState>(); } return *L->CarryState; }
	static const FAntCarryState* StC(const AKitchenLoot* L) { return L->CarryState.Get(); }
	static UBoxComponent* Body(const AKitchenLoot* L) { return L->Body; }
	static const TArray<FVector>& GrabLocal(const AKitchenLoot* L) { return L->GrabLocal; }
	static bool PhysicsReady(const AKitchenLoot* L) { return L->bPhysicsReady; }
	static void Tick(AKitchenLoot* L, float Dt, const float* Extra, int32 Win, const float* Str);
	static void Publish(AKitchenLoot* L, FAntCarryState& S, bool bTransform);
	static void SayBark(AKitchenLoot* L, FAntCarryState& S, uint8 Bark, int32 MateIndex, float Cooldown = 1.6f);
	static void Workers(AKitchenLoot* L, FAntCarryState& S, float Dt);
	static void Yanks(AKitchenLoot* L, FAntCarryState& S, float Dt, const FVector& LoadV);
	static void RunOver(AKitchenLoot* L, float Speed);
};

namespace
{
	/** Server: ants that lost their grip to a yank (when, and off which load). */
	TMap<TWeakObjectPtr<AAntCharacter>, TPair<float, TWeakObjectPtr<AKitchenLoot>>>& Tumbled() { static TMap<TWeakObjectPtr<AAntCharacter>, TPair<float, TWeakObjectPtr<AKitchenLoot>>> M; return M; }
	/** Server: last run-over per ant. */
	TMap<TWeakObjectPtr<AAntCharacter>, float>& RunOverAt() { static TMap<TWeakObjectPtr<AAntCharacter>, float> M; return M; }

	float ServerNow(const UObject* O)
	{
		const UWorld* W = O ? O->GetWorld() : nullptr;
		if (!W) { return 0.f; }
		const AGameStateBase* GS = W->GetGameState();
		return GS ? (float)GS->GetServerWorldTimeSeconds() : W->GetTimeSeconds();
	}

	/** Weight shares by the lever arms: the least-squares solution of sum s = 1, sum s * r = 0 (r: grip - centre of mass, flat). */
	void LeverShares(const TArray<FVector>& P, const FVector& C, TArray<float>& S)
	{
		const int32 N = P.Num();
		S.Init(N > 0 ? 1.f / N : 0.f, N);
		if (N <= 1)
		{
			return;
		}
		double Sx = 0, Sy = 0, Sxx = 0, Sxy = 0, Syy = 0;
		for (const FVector& Q : P)
		{
			const double X = Q.X - C.X, Y = Q.Y - C.Y;
			Sx += X; Sy += Y; Sxx += X * X; Sxy += X * Y; Syy += Y * Y;
		}
		const double Eps = 1.e-3 * (Sxx + Syy) + 1.0;   // grips in a line: the spread across it is ~0, the minimum-norm answer
		Sxx += Eps; Syy += Eps;
		// | N   Sx  Sy  | |a |   |1|
		// | Sx  Sxx Sxy | |bx| = |0|
		// | Sy  Sxy Syy | |by|   |0|
		const double Nn = N;
		const double Det = Nn * (Sxx * Syy - Sxy * Sxy) - Sx * (Sx * Syy - Sxy * Sy) + Sy * (Sx * Sxy - Sxx * Sy);
		if (FMath::Abs(Det) < 1.e-9)
		{
			return;
		}
		const double A = (Sxx * Syy - Sxy * Sxy) / Det;
		const double Bx = -(Sx * Syy - Sxy * Sy) / Det;
		const double By = (Sx * Sxy - Sxx * Sy) / Det;
		double Sum = 0;
		for (int32 i = 0; i < N; ++i)
		{
			const double V = FMath::Max(0.0, A + Bx * (P[i].X - C.X) + By * (P[i].Y - C.Y));
			S[i] = (float)V;
			Sum += V;
		}
		if (Sum < 1.e-6)
		{
			S.Init(1.f / N, N);
			return;
		}
		// On a slope the lower carriers bear more (the load's weight leans onto them): +1 % per unit lower than the top grip.
		float Top = -1.e9f;
		for (const FVector& Q : P) { Top = FMath::Max(Top, (float)Q.Z); }
		Sum = 0;
		for (int32 i = 0; i < N; ++i)
		{
			S[i] *= FMath::Clamp(1.f + 0.01f * (Top - (float)P[i].Z), 1.f, 1.6f);
			Sum += S[i];
		}
		for (float& V : S) { V = (float)(V / Sum); }
		// Statics with several grips has many answers; the least-norm one can pile most of the weight on one ant when the grips
		// are uneven. Ants also shift the weight among themselves: half of it is shared evenly, and with three or more nobody bears
		// more than one and a half times an even share (four ants: 37.5 %).
		if (N >= 2)
		{
			Sum = 0;
			for (float& V : S) { V = FMath::Min(0.5f * V + 0.5f / N, N >= 3 ? 1.5f / N : 1.f); Sum += V; }
			for (float& V : S) { V = (float)(V / Sum); }
		}
	}

	/** Speed of carriers for strength / weight R and N carriers: dragged below 1, a little faster with every bit of surplus;
	 *  a crowd with a lot of surplus runs (up to sprint pace with 11+ ants). */
	float LoadFactor(float R, int32 N)
	{
		if (R < 0.5f) { return 0.02f; }
		if (R < 1.f) { return FMath::Max(0.05f, 0.43f * (R - 0.5f) / 0.5f); }
		const float Cap = FMath::Clamp(0.9f + 0.06f * (N - 1), 0.9f, 1.5f);
		return FMath::Min(Cap, 0.5f + 0.45f * FMath::Log2(R));
	}

	/** The ant that is a carrier of L at index i: its grip in the world. */
	FVector AntGrip(const AKitchenLoot* L, int32 i)
	{
		const TArray<FVector>& G = FAntCarrySolver::GrabLocal(L);
		return G.IsValidIndex(i) ? L->GetActorTransform().TransformPosition(G[i]) : L->GetActorLocation();
	}

	const FAntCarryGrip* NetGrip(const AKitchenLoot* L, const AActor* Who, int32 Worker)
	{
		if (!L) { return nullptr; }
		const uint8 W = Worker < 0 ? 255 : (uint8)Worker;
		for (const FAntCarryGrip& G : L->CarryNet.Grips)
		{
			if (G.Who == Who && G.Worker == W) { return &G; }
		}
		return nullptr;
	}
}

// =============================================================================================
// API
// =============================================================================================

FAntCarryEventSignature& AntCarry::Events()
{
	static FAntCarryEventSignature E;
	return E;
}

void AntCarry::Notify(AKitchenLoot* L, uint8 Kind, uint8 Team)
{
	if (L && L->HasAuthority())
	{
		static const TCHAR* Names[] = { TEXT("delivered"), TEXT("dropped"), TEXT("broken"), TEXT("snatched"), TEXT("joined"), TEXT("left") };
		if (Kind <= AntCarryEvent::Snatched)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY event %s «%s» team %d"), Kind < 6 ? Names[Kind] : TEXT("?"), *L->LootName, (int32)Team);
		}
		Events().Broadcast(L, Kind, Team);
	}
}

int32 AntCarry::MaxCarriers(const AKitchenLoot* L)
{
	if (!L) { return 0; }
	// About one ant per 9 units of the outline (an ant is ~5 wide at the jaws, they stand shoulder to shoulder).
	return FMath::Clamp(FMath::RoundToInt(2.f * PI * L->GetRadius() / 9.f), 3, 24);
}

FVector AntCarry::PickGrip(const AKitchenLoot* L, const FVector& From, const FVector& Closest)
{
	// The jaws close on the side facing the ant, at the height of its jaws: the point of the surface nearest to a spot a little
	// above the ant's feet, approached from outside (from inside a box the "nearest point" is the spot itself: a grip in the
	// middle of the load, with no lever arm at all).
	const UBoxComponent* B = FAntCarrySolver::Body(L);
	if (!B)
	{
		return Closest;
	}
	const FVector C = L->GetActorLocation();
	const FVector Out = Flat(From - C).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const float Reach = (float)B->GetScaledBoxExtent().Size() + 20.f;
	FVector Q = FVector(From.X, From.Y, From.Z + 2.f);
	FVector P = Closest;
	for (int32 k = 0; k < 4; ++k)
	{
		FVector Hit;
		const float D = B->GetClosestPointOnCollision(Q, Hit);
		if (D > 0.01f)
		{
			P = Hit;
			break;
		}
		Q += Out * Reach * 0.5f;   // inside: step out along the ant's side and try again
	}
	return P;
}

bool AntCarry::HasWorkers(const AKitchenLoot* L)
{
	const FAntCarryState* S = L ? FAntCarrySolver::StC(L) : nullptr;
	return S && S->NumWorkers() > 0;
}

void AntCarry::AddWorkerStrength(const AKitchenLoot* L, float* TeamStr)
{
	const FAntCarryState* S = L ? FAntCarrySolver::StC(L) : nullptr;
	if (!S) { return; }
	for (const FAntCarryState::FMate& M : S->Mates)
	{
		if (M.IsWorker() && M.Army.IsValid())
		{
			TeamStr[FMath::Clamp<int32>(M.Army->Team, 0, 1)] += M.Army->WorkerStr;
		}
	}
}

void AntCarry::CreditAnts(const AKitchenLoot* L, uint8 Team, TArray<AAntCharacter*>& InOut)
{
	const FAntCarryState* S = L ? FAntCarrySolver::StC(L) : nullptr;
	if (!S) { return; }
	for (const FAntCarryState::FMate& M : S->Mates)
	{
		if (M.IsWorker() && M.Army.IsValid() && M.Army->Team == Team && IsValid(M.Army->Commander) && !M.Army->Commander->bDead)
		{
			InOut.AddUnique(M.Army->Commander);
		}
	}
}

bool AntCarry::MakeRoom(AKitchenLoot* L)
{
	FAntCarryState& S = FAntCarrySolver::St(L);
	if (L->Carriers.Num() + S.NumWorkers() < MaxCarriers(L))
	{
		return true;
	}
	// A worker steps aside for an ant (the last one that joined).
	for (int32 i = S.Mates.Num() - 1; i >= 0; --i)
	{
		if (S.Mates[i].IsWorker())
		{
			S.Mates.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool AntCarry::AddCarrier(AKitchenLoot* L, AActor* Who, int32 Worker, const FVector& Grip)
{
	if (!L || !Who || !L->HasAuthority() || L->bIsGear || L->bHeld)
	{
		return false;
	}
	if (AAntCharacter* A = Cast<AAntCharacter>(Who))
	{
		if (!A->GrabLoot(L))
		{
			return false;
		}
		if (!Grip.IsZero())
		{
			// an order chose the spot: move the grip there (it is the ant's: AKitchenLoot keeps it)
			const int32 i = L->Carriers.IndexOfByKey(A);
			TArray<FVector>& G = const_cast<TArray<FVector>&>(FAntCarrySolver::GrabLocal(L));
			FVector Q = Grip;
			if (UBoxComponent* B = FAntCarrySolver::Body(L)) { B->GetClosestPointOnCollision(Grip, Q); }
			if (G.IsValidIndex(i)) { G[i] = L->GetActorTransform().InverseTransformPosition(Q); }
		}
		return true;
	}
	AAntArmy* Army = Cast<AAntArmy>(Who);
	if (!Army || Worker < 0 || Worker >= Army->Count || Worker > 250)
	{
		return false;
	}
	FAntCarryState& S = FAntCarrySolver::St(L);
	if (S.FindWorker(Army, Worker))
	{
		return true;
	}
	if (L->Carriers.Num() + S.NumWorkers() >= MaxCarriers(L))
	{
		return false;
	}
	FVector At = Grip;
	if (At.IsZero())
	{
		// all round it, evenly, by worker index (a ring that fills from the front of the army's travel)
		const int32 K = S.NumWorkers();
		const float A = K * 2.39996f;
		const FVector C = L->GetActorLocation();
		At = C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (L->GetRadius() + 20.f);
		At.Z = C.Z;
	}
	FVector Closest = L->GetActorLocation();
	if (UBoxComponent* B = FAntCarrySolver::Body(L)) { B->GetClosestPointOnCollision(At, Closest); }
	// a worker's jaws are a few units above the floor the load stands on
	const float Bottom = (float)L->GetActorLocation().Z - (FAntCarrySolver::Body(L) ? (float)FAntCarrySolver::Body(L)->GetScaledBoxExtent().Z : L->GetRadius() * 0.5f);
	const FVector P = PickGrip(L, FVector(At.X, At.Y, Bottom + 5.f), Closest);
	FAntCarryState::FMate M;
	M.Army = Army;
	M.Worker = Worker;
	M.WLocal = L->GetActorTransform().InverseTransformPosition(P);
	M.Jaw = P;
	M.Phase = FMath::FRand();
	S.Mates.Add(M);
	ServerCarriersChanged(L);
	return true;
}

void AntCarry::RemoveCarrier(AKitchenLoot* L, AActor* Who, int32 Worker)
{
	if (!L || !Who || !L->HasAuthority())
	{
		return;
	}
	if (AAntCharacter* A = Cast<AAntCharacter>(Who))
	{
		if (A->Carrying == L) { A->LetGo(); }
		return;
	}
	FAntCarryState& S = FAntCarrySolver::St(L);
	const int32 Before = S.Mates.Num();
	S.Mates.RemoveAll([Who, Worker](const FAntCarryState::FMate& M) { return M.IsWorker() && M.Army.Get() == Who && (Worker < 0 || M.Worker == Worker); });
	if (S.Mates.Num() != Before)
	{
		ServerCarriersChanged(L);
	}
}

int32 AntCarry::AddArmy(AKitchenLoot* L, AAntArmy* Army, int32 N)
{
	int32 Got = 0;
	if (!L || !Army) { return 0; }
	FAntCarryState& S = FAntCarrySolver::St(L);
	for (int32 w = 0; w < Army->Count && Got < N; ++w)
	{
		if (S.FindWorker(Army, w)) { ++Got; continue; }
		if (!AddCarrier(L, Army, w)) { break; }
		++Got;
	}
	return Got;
}

void AntCarry::RemoveArmy(AKitchenLoot* L, AAntArmy* Army)
{
	RemoveCarrier(L, Army, -1);
}

int32 AntCarry::ArmyOn(const AKitchenLoot* L, const AAntArmy* Army)
{
	const FAntCarryState* S = (L && IsValid(L)) ? FAntCarrySolver::StC(L) : nullptr;
	int32 N = 0;
	if (S) { for (const FAntCarryState::FMate& M : S->Mates) { N += (M.IsWorker() && M.Army.Get() == Army) ? 1 : 0; } }
	return N;
}

void AntCarry::GetCarriers(const AKitchenLoot* L, TArray<FAntCarrierInfo>& Out)
{
	Out.Reset();
	if (!L) { return; }
	const FAntCarryState* S = FAntCarrySolver::StC(L);
	for (int32 i = 0; i < L->Carriers.Num(); ++i)
	{
		FAntCarrierInfo I;
		I.Who = L->Carriers[i];
		I.Grip = AntGrip(L, i);
		I.Strength = IsValid(L->Carriers[i]) ? L->Carriers[i]->GetStrength() : 0.f;
		if (S) { if (const FAntCarryState::FMate* M = const_cast<FAntCarryState*>(S)->FindAnt(L->Carriers[i])) { I.Desired = M->Desired; I.Strain = M->Strain; I.Share = M->Share; I.bDragged = M->bDragged; } }
		Out.Add(I);
	}
	if (S)
	{
		for (const FAntCarryState::FMate& M : S->Mates)
		{
			if (!M.IsWorker()) { continue; }
			FAntCarrierInfo I;
			I.Who = M.Army.Get();
			I.Worker = M.Worker;
			I.Grip = L->GetActorTransform().TransformPosition(M.WLocal);
			I.Desired = M.Desired;
			I.Strength = M.Army.IsValid() ? M.Army->WorkerStr : 0.f;
			I.Strain = M.Strain; I.Share = M.Share; I.bDragged = M.bDragged;
			Out.Add(I);
		}
	}
}

void AntCarry::SetDesired(AKitchenLoot* L, AActor* Who, int32 Worker, const FVector& Velocity)
{
	if (!L) { return; }
	FAntCarryState& S = FAntCarrySolver::St(L);
	FAntCarryState::FMate* M = Worker >= 0 ? S.FindWorker(Cast<AAntArmy>(Who), Worker) : S.FindAnt(Cast<AAntCharacter>(Who));
	if (!M && Worker < 0)
	{
		// an ant that is a carrier but has no entry yet (the first tick will make it): make it now
		if (AAntCharacter* A = Cast<AAntCharacter>(Who); A && L->Carriers.Contains(A))
		{
			FAntCarryState::FMate N;
			N.Ant = A;
			N.Phase = FMath::FRand();
			S.Mates.Add(N);
			M = &S.Mates.Last();
		}
	}
	if (M)
	{
		M->Desired = Flat(Velocity);
		M->bExplicit = !Velocity.IsNearlyZero();
	}
}

void AntCarry::SetArmyGoal(AKitchenLoot* L, AAntArmy* Army, const FVector& Goal, float SpeedMul)
{
	if (!L || !Army) { return; }
	FAntCarryState& S = FAntCarrySolver::St(L);
	const FVector To = Flat(Goal - L->GetActorLocation());
	const float D = (float)To.Size();
	// slow down for the last bit (a crowd still overshoots: their braking is weak)
	const FVector V = D < 25.f ? FVector::ZeroVector : To / D * CT().WorkerSpeed * SpeedMul * FMath::Clamp(D / 220.f, 0.25f, 1.f);
	for (FAntCarryState::FMate& M : S.Mates)
	{
		if (M.IsWorker() && M.Army.Get() == Army)
		{
			M.Desired = V;
			M.bExplicit = true;
			M.Leader = nullptr;
		}
	}
}

FVector AntCarry::GetDesired(const AKitchenLoot* L, const AActor* Who, int32 Worker)
{
	const FAntCarryState* S = L ? FAntCarrySolver::StC(L) : nullptr;
	if (!S) { return FVector::ZeroVector; }
	for (const FAntCarryState::FMate& M : S->Mates)
	{
		if ((Worker >= 0 && M.IsWorker() && M.Army.Get() == Who && M.Worker == Worker) || (Worker < 0 && !M.IsWorker() && M.Ant.Get() == Who))
		{
			return M.Desired;
		}
	}
	return FVector::ZeroVector;
}

// =============================================================================================
// Server: who holds it, the replicated state, shouts
// =============================================================================================

namespace
{
	/** The mates follow AKitchenLoot::Carriers (ants) and the armies' counts (workers). True if anything changed. */
	bool SyncMates(AKitchenLoot* L, FAntCarryState& S)
	{
		const int32 Before = S.Mates.Num();
		bool bChanged = false;
		S.Mates.RemoveAll([L, &bChanged](const FAntCarryState::FMate& M)
		{
			const bool bGone = M.IsWorker()
				? (!M.Army.IsValid() || M.Worker >= M.Army->Count || (M.Leader.IsValid() && M.Leader->Carrying != L) || (!M.bExplicit && M.Leader.IsStale()))
				: (!M.Ant.IsValid() || !L->Carriers.Contains(M.Ant.Get()));
			bChanged |= bGone;
			return bGone;
		});
		for (AAntCharacter* A : L->Carriers)
		{
			if (IsValid(A) && !S.FindAnt(A))
			{
				FAntCarryState::FMate M;
				M.Ant = A;
				M.Phase = FMath::FRand();
				M.JoinedAt = L->GetWorld()->GetTimeSeconds();
				S.Mates.Add(M);
				bChanged = true;
			}
		}
		return bChanged || Before != S.Mates.Num();
	}
}

void AntCarry::ReleaseAllWorkers(AKitchenLoot* L)
{
	if (L)
	{
		FAntCarrySolver::St(L).Mates.RemoveAll([](const FAntCarryState::FMate& M) { return M.IsWorker(); });
	}
}

void AntCarry::ServerCarriersChanged(AKitchenLoot* L)
{
	if (!L || !L->HasAuthority())
	{
		return;
	}
	FAntCarryState& S = FAntCarrySolver::St(L);
	SyncMates(L, S);
	if (L->bHeld)
	{
		S.Mates.Reset();
	}
	const bool bActive = S.Mates.Num() > 0 && !L->bHeld && !L->bIsGear;
	if (bActive != S.bWasActive)
	{
		// While ants carry it the server sends its own smoothed state (FAntCarryNet); the engine's movement replication would
		// make clients simulate it with gravity and snap it back 30 times a second.
		L->SetReplicatingMovement(!bActive);
		if (bActive)
		{
			S.UpRef = L->GetActorQuat().GetUpVector();
		}
		if (!bActive && !L->bHeld && !L->IsPendingKillPending())
		{
			AntCarry::Notify(L, AntCarryEvent::Dropped, L->CarrierTeam);
		}
		S.bWasActive = bActive;
	}
	// "Держите!": somebody let go and the rest cannot hold it up any more.
	const int32 Strong = L->Weight > 0 && L->CarryStrength >= (float)L->Weight ? 1 : 0;
	if (bActive && S.LastStrongEnough == 1 && Strong == 0 && S.Mates.Num() > 0)
	{
		FAntCarrySolver::SayBark(L, S, B_Hold, FMath::RandRange(0, S.Mates.Num() - 1), 1.f);
	}
	S.LastStrongEnough = bActive ? Strong : -1;
	FAntCarrySolver::Publish(L, S, true);
	L->ForceNetUpdate();
}

void FAntCarrySolver::SayBark(AKitchenLoot* L, FAntCarryState& S, uint8 Bark, int32 MateIndex, float Cooldown)
{
	const float Now = L->GetWorld()->GetTimeSeconds();
	if (Now < S.NextBark || !S.Mates.IsValidIndex(MateIndex))
	{
		return;
	}
	S.NextBark = Now + Cooldown;
	FAntCarryNet& Net = L->CarryNet;
	Net.Bark = Bark;
	Net.BarkSeq = (uint8)(Net.BarkSeq + 1);
	const FAntCarryState::FMate& M = S.Mates[MateIndex];
	Net.BarkBy = (uint8)MateIndex;   // Publish keeps the grips in the order of the mates
	UE_LOG(LogTemp, Display, TEXT("ANTCARRY bark «%s» by %s on «%s»"), BarkText(Bark),
		M.IsWorker() ? *FString::Printf(TEXT("worker %d"), M.Worker) : (M.Ant.IsValid() ? *M.Ant->GetDisplayName() : TEXT("?")), *L->LootName);
}

void FAntCarrySolver::Publish(AKitchenLoot* L, FAntCarryState& S, bool bTransform)
{
	FAntCarryNet& Net = L->CarryNet;
	Net.bActive = S.Mates.Num() > 0 && !L->bHeld;
	Net.Grips.SetNum(S.Mates.Num());
	for (int32 i = 0; i < S.Mates.Num(); ++i)
	{
		const FAntCarryState::FMate& M = S.Mates[i];
		FAntCarryGrip& G = Net.Grips[i];
		if (M.IsWorker())
		{
			G.Who = M.Army.Get();
			G.Worker = (uint8)FMath::Clamp(M.Worker, 0, 254);
			G.Local = M.WLocal;
		}
		else
		{
			G.Who = M.Ant.Get();
			G.Worker = 255;
			const int32 k = L->Carriers.IndexOfByKey(M.Ant.Get());
			G.Local = L->GrabLocal.IsValidIndex(k) ? L->GrabLocal[k] : FVector::ZeroVector;
		}
		G.Strain = (uint8)FMath::Clamp(FMath::RoundToInt(M.Strain * 100.f), 0, 250);
		G.Share = (uint8)FMath::Clamp(FMath::RoundToInt(M.Share * 255.f), 0, 255);
		G.PullX = (int8)FMath::Clamp(FMath::RoundToInt(M.Pull.X * 127.f), -127, 127);
		G.PullY = (int8)FMath::Clamp(FMath::RoundToInt(M.Pull.Y * 127.f), -127, 127);
		G.Flags = (M.bDragged ? 1 : 0) | (M.bAtLimit ? 2 : 0);
	}
	Net.Sync = (uint8)FMath::Clamp(FMath::RoundToInt(S.Sync * 255.f), 0, 255);
	Net.bRhythm = S.bRhythm;
	if (bTransform)
	{
		Net.Pos = L->GetActorLocation();
		Net.Rot = L->GetActorRotation();
		Net.Vel = L->Body ? L->Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		Net.Stamp = ServerNow(L);
	}
}

// =============================================================================================
// Server: the forces
// =============================================================================================

void AntCarry::ServerTick(AKitchenLoot* L, float Dt, const float* Extra, int32 WinTeam, const float* TeamStr)
{
	if (L && L->HasAuthority() && Dt > 0.f)
	{
		FAntCarrySolver::Tick(L, Dt, Extra, WinTeam, TeamStr);
	}
}

void FAntCarrySolver::Workers(AKitchenLoot* L, FAntCarryState& S, float Dt)
{
	Dt = FMath::Min(Dt, 0.05f);   // (a hitch - a screenshot, a load - must not throw the virtual jaws across the room)
	const FTune& K = CT();
	UWorld* W = L->GetWorld();
	const FTransform TM = L->GetActorTransform();
	const float Ratio = L->Weight > 0 ? L->CarryStrength / (float)L->Weight : 10.f;
	const float Pace = K.WorkerSpeed * LoadFactor(Ratio, S.Mates.Num()) * (S.bRhythm ? 1.1f : 1.f);
	const FVector Center = L->GetActorLocation();
	FCollisionQueryParams Q(TEXT("AntCarryWorker"), false, L);
	for (int32 i = S.Mates.Num() - 1; i >= 0; --i)
	{
		FAntCarryState::FMate& M = S.Mates[i];
		if (!M.IsWorker())
		{
			continue;
		}
		const FVector G = TM.TransformPosition(M.WLocal);
		FVector Want = M.Desired;
		if (!M.bExplicit)
		{
			Want = M.Leader.IsValid() ? Flat(M.Leader->GetVelocity()) : FVector::ZeroVector;
		}
		Want = Want.GetClampedToMaxSize(Pace);
		// the jaws go where the worker wants to go...
		FVector Jaw = M.Jaw + Want * Dt;
		// ...held back by the grip (the worker is tied to it like an ant)
		const FVector Out = Flat(G - Center).GetSafeNormal();
		const FVector Rest = G + Out * 1.5f;
		FVector Err = Flat(Rest - Jaw);
		const float D = (float)Err.Size() - K.TetherSlack;
		FVector Pull = FVector::ZeroVector;
		if (D > 0.f)
		{
			Pull = Err.GetSafeNormal() * FMath::Min(K.TetherMax, K.TetherK * D);
			Jaw += Pull * Dt;
		}
		// standing on the floor under its jaws (a trace each tick per worker: up to ~24 per load, on the server only)
		FHitResult Hit;
		float FloorZ = (float)G.Z - 30.f;
		if (W->LineTraceSingleByObjectType(Hit, FVector(Jaw.X, Jaw.Y, G.Z + 4.f), FVector(Jaw.X, Jaw.Y, G.Z - 60.f), UAntMovementComponent::SurfaceObjects(), Q))
		{
			FloorZ = (float)Hit.ImpactPoint.Z;
		}
		Jaw.Z = FloorZ + 7.f;
		M.JawV = Want + Pull;
		M.Jaw = Jaw;
		// dragged off its feet: the load runs away faster than it can walk, or it hangs from a load that is lifted high
		M.bDragged = Pull.Size() > Pace * 1.25f + 60.f || (float)G.Z - FloorZ > 26.f;
		M.TornT = (D > 70.f || (float)G.Z - FloorZ > 70.f) ? M.TornT + Dt : 0.f;
		if (M.TornT > 0.3f)
		{
			// torn off (a corner, an edge): it falls away; its squad may send it back
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY worker %d of %s torn off «%s»"), M.Worker, M.Army.IsValid() ? *M.Army->GetName() : TEXT("?"), *L->LootName);
			S.Mates.RemoveAt(i);
		}
	}
}

void FAntCarrySolver::RunOver(AKitchenLoot* L, float Speed)
{
	if (Speed < 110.f)
	{
		return;
	}
	const float Now = L->GetWorld()->GetTimeSeconds();
	const FVector C = L->GetActorLocation();
	const float R = L->GetRadius();
	for (auto It = Tumbled().CreateIterator(); It; ++It)
	{
		AAntCharacter* A = It->Key.Get();
		if (!A || Now - It->Value.Key > 4.f)
		{
			It.RemoveCurrent();
			continue;
		}
		if (It->Value.Value.Get() != L || A->bDead || A->Carrying)
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		float& Last = RunOverAt().FindOrAdd(A);
		if (Now - Last > 1.5f && FVector::Dist2D(P, C) < R * 0.85f && FMath::Abs(P.Z - C.Z) < R + 20.f)
		{
			Last = Now;
			A->NoteBodyAct(AntAct::Knock, -FVector::UpVector + Flat(L->Body->GetPhysicsLinearVelocity()).GetSafeNormal(), 0.8f);
			A->HurtBy(5.f, FString::Printf(TEXT("переехал свой же груз «%s»"), *L->LootName));
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY run over: «%s» rolled over %s at %.0f u/s"), *L->LootName, *A->GetDisplayName(), Speed);
			FAntCarryState& S = St(L);
			if (S.Mates.Num() > 0) { S.NextBark = 0.f; SayBark(L, S, B_RunOver, 0, 1.2f); }
			A->Toast(FString::Printf(TEXT("«%s» переехал тебя!"), *L->LootName));
			AntFighter::OnRunOver(A);   // «Переехан своими» (AntFighter.h)
		}
	}
}

void FAntCarrySolver::Yanks(AKitchenLoot* L, FAntCarryState& S, float Dt, const FVector& LoadV)
{
	const FTune& K = CT();
	const float Now = L->GetWorld()->GetTimeSeconds();
	const int32 N = S.Mates.Num();
	if (N < 2)
	{
		return;
	}
	// who differs most from the others: the one that jerks
	FVector Mean = FVector::ZeroVector;
	TArray<FVector, TInlineAllocator<24>> Vel;
	for (const FAntCarryState::FMate& M : S.Mates)
	{
		const FVector V = M.IsWorker() ? Flat(M.JawV) : Flat(M.RealV);
		Vel.Add(V);
		Mean += V;
	}
	Mean /= N;
	int32 Cause = -1;
	float CauseD = 80.f;
	for (int32 i = 0; i < N; ++i)
	{
		const float D = (float)(Vel[i] - Mean).Size();
		if (D > CauseD) { CauseD = D; Cause = i; }
	}
	TArray<AAntCharacter*, TInlineAllocator<4>> Tumble;
	TArray<FVector, TInlineAllocator<4>> TumbleDir;
	for (int32 i = 0; i < N; ++i)
	{
		FAntCarryState::FMate& M = S.Mates[i];
		AAntCharacter* A = M.Ant.Get();
		if (M.IsWorker() || !A)
		{
			continue;
		}
		const float Y = (float)M.Vrel.Size();
		M.Over = M.bAtLimit && M.Strain > 1.45f ? M.Over + Dt : FMath::Max(0.f, M.Over - Dt * 2.f);
		if (Now < M.NextYank || Now - M.JoinedAt < 1.2f)
		{
			continue;
		}
		const FVector Dir = M.Vrel.GetSafeNormal();
		if ((Y > K.TumbleV || (M.Over > 2.f && Y > K.YankV)) && AntFood::LeashMul(L) < 2.f)   // (sticky food glues the jaws: no tearing out)
		{
			M.NextYank = Now + 1.5f;
			Tumble.Add(A);
			TumbleDir.Add(Dir);
			S.NextBark = 0.f;
			SayBark(L, S, B_Ouch, i, 1.2f);
		}
		else if (Y > K.YankV && Cause >= 0 && i != Cause)
		{
			// pulled off balance: the body reels along the pull and the ant takes a stumbling step
			M.NextYank = Now + 0.9f;
			A->NoteBodyAct(AntAct::Flinch, Dir, FMath::Clamp(Y / 350.f, 0.25f, 1.f));
			A->Stagger(0.22f, FVector::ZeroVector);
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY yank: %s pulled at %.0f u/s on «%s» (by %s)"), *A->GetDisplayName(), Y, *L->LootName,
				S.Mates.IsValidIndex(Cause) ? (S.Mates[Cause].IsWorker() ? TEXT("a worker") : (S.Mates[Cause].Ant.IsValid() ? *S.Mates[Cause].Ant->GetDisplayName() : TEXT("?"))) : TEXT("the load"));
			SayBark(L, S, B_Yank, i, 2.2f);
			if (S.Mates.IsValidIndex(Cause) && !S.Mates[Cause].IsWorker() && S.Mates[Cause].Ant.IsValid() && S.Mates[Cause].Ant->IsPlayerControlled() && Now > S.NextYankToast)
			{
				S.NextYankToast = Now + 4.f;
				S.Mates[Cause].Ant->Toast(TEXT("Не дёргай: груз мотает, свои спотыкаются. Идите в ногу!"));
			}
		}
	}
	for (int32 k = 0; k < Tumble.Num(); ++k)
	{
		// torn out of the grip: onto its back, a few body lengths away (comic, and it can take hold again)
		AAntCharacter* A = Tumble[k];
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY tumble: %s lost its grip on «%s»"), *A->GetDisplayName(), *L->LootName);
		Tumbled().Add(A, TPair<float, TWeakObjectPtr<AKitchenLoot>>(Now, L));
		A->LetGo();
		A->ApplyLaunch((TumbleDir[k] * 230.f + FVector(0.f, 0.f, 210.f)) * AntKit::JumpK);
		A->NoteBodyAct(AntAct::Knock, TumbleDir[k], 0.65f);
		A->Toast(TEXT("Груз вырвался — ты кубарем! [E] — схватиться снова"));
	}
}

void FAntCarrySolver::Tick(AKitchenLoot* L, float Dt, const float* Extra, int32 Win, const float* TeamStr)
{
	FAntCarryState& S = St(L);
	UBoxComponent* B = L->Body;
	UWorld* W = L->GetWorld();
	const FTune& K = CT();
	const float Now = W->GetTimeSeconds();
	if (SyncMates(L, S))
	{
		AntCarry::ServerCarriersChanged(L);
	}

	// Squads help their commanders: the workers following an ant that carries take hold all round the load (up to 12).
	for (AAntCharacter* A : TArray<AAntCharacter*>(L->Carriers))
	{
		AAntArmy* Army = IsValid(A) ? AAntArmy::FindFor(A) : nullptr;
		if (!Army || !Army->bSquad || Army->Cargo == L)
		{
			continue;
		}
		const bool bHelp = Army->HelpsCommander(A) && Army->Team == A->Team;
		const int32 On = AntCarry::ArmyOn(L, Army);
		if (bHelp && On < FMath::Min(Army->Count, 12))
		{
			const int32 Want = FMath::Min(Army->Count, 12);
			for (int32 w = 0; w < Army->Count && AntCarry::ArmyOn(L, Army) < Want; ++w)
			{
				if (!S.FindWorker(Army, w) && AntCarry::AddCarrier(L, Army, w))
				{
					if (FAntCarryState::FMate* M = S.FindWorker(Army, w)) { M->Leader = A; M->bExplicit = false; }
				}
				else if (!S.FindWorker(Army, w))
				{
					break;   // no room
				}
			}
		}
		else if (!bHelp && On > 0)
		{
			AntCarry::RemoveArmy(L, Army);
		}
	}
	const int32 N = S.Mates.Num();
	if (N == 0 || !B || !B->IsSimulatingPhysics())
	{
		return;
	}
	Workers(L, S, Dt);
	if (S.Mates.Num() != N)
	{
		AntCarry::ServerCarriersChanged(L);
		return;
	}

	B->WakeAllRigidBodies();
	const FTransform TM = L->GetActorTransform();
	const FVector COM = B->GetCenterOfMass();
	const float Mass = B->GetMass();
	const float G = FMath::Abs(W->GetGravityZ());
	const float FPS = AKitchenLoot::KgPerWeight * G;
	const bool bLift = L->bMain && L->CarryStrength >= (float)L->Weight;
	const float Ratio0 = L->Weight > 0 ? L->CarryStrength / (float)L->Weight : 10.f;

	TArray<FVector, TInlineAllocator<24>> P, H, V;
	TArray<float, TInlineAllocator<24>> Str;
	TArray<int32, TInlineAllocator<24>> Team;
	TArray<FVector> Pts;
	for (FAntCarryState::FMate& M : S.Mates)
	{
		if (M.IsWorker())
		{
			P.Add(TM.TransformPosition(M.WLocal));
			H.Add(M.Jaw);
			V.Add(M.JawV);
			Str.Add(M.Army.IsValid() ? M.Army->WorkerStr : 0.f);
			Team.Add(M.Army.IsValid() ? FMath::Clamp<int32>(M.Army->Team, 0, 1) : 0);
		}
		else
		{
			AAntCharacter* A = M.Ant.Get();
			const int32 k = L->Carriers.IndexOfByKey(A);
			P.Add(AntGrip(L, k));
			H.Add(A->GetHoldPoint());
			const FVector Here = A->GetActorLocation();
			const FVector Went = M.LastPos.IsZero() ? FVector::ZeroVector : (Here - M.LastPos) / Dt;
			M.LastPos = Here;
			M.RealV += (Went.GetClampedToMaxSize(2000.f) - M.RealV) * (1.f - FMath::Exp(-Dt / 0.05f));
			V.Add(M.RealV);
			Str.Add(A->GetStrength() + (Extra && L->Carriers.IsValidIndex(k) ? Extra[k] : 0.f));
			Team.Add(FMath::Clamp<int32>(A->Team, 0, 1));
			if (!M.bExplicit) { M.Desired = Flat(A->GetVelocity()); }
		}
		Pts.Add(P.Last());
		if (bLift && Team.Last() == Win) { H.Last().Z += 28.f; }
		else if (Team.Last() == Win && Ratio0 >= 1.f) { H.Last().Z += FMath::Min(4.f + 6.f * (Ratio0 - 1.f), 10.f); }   // lifted clear of the floor
	}

	// ---- the weight, shared by the lever arms
	TArray<float> Share;
	LeverShares(Pts, COM, Share);

	// ---- moving as one? (the winning side)
	{
		FVector SumV = FVector::ZeroVector;
		float SumS = 0.f, SumS2 = 0.f;
		int32 Cnt = 0;
		for (int32 i = 0; i < N; ++i)
		{
			if (Team[i] != Win) { continue; }
			const FVector Fv = Flat(V[i]);
			const float Sp = (float)Fv.Size();
			SumV += Fv; SumS += Sp; SumS2 += Sp * Sp; ++Cnt;
		}
		float Raw = 1.f;
		const float Mean = Cnt > 0 ? SumS / Cnt : 0.f;
		if (Cnt >= 2 && Mean > 25.f)
		{
			const float Coh = (float)SumV.Size() / FMath::Max(1.f, SumS);
			const float Sd = FMath::Sqrt(FMath::Max(0.f, SumS2 / Cnt - Mean * Mean));
			Raw = Coh * FMath::Clamp(1.f - Sd / Mean, 0.f, 1.f);
		}
		S.Sync += (Raw - S.Sync) * (1.f - FMath::Exp(-Dt / 0.35f));
		const bool bMoving = Mean > 60.f && Cnt >= 2;
		S.RhythmT = (bMoving && S.Sync > 0.85f) ? S.RhythmT + Dt : FMath::Max(0.f, S.RhythmT - 2.f * Dt);
		const bool bWas = S.bRhythm;
		S.bRhythm = S.bRhythm ? (S.Sync > 0.7f && S.RhythmT > 0.f) : S.RhythmT > 1.f;
		if (S.bRhythm && !bWas && Now > S.NextRhythmBark)
		{
			S.NextRhythmBark = Now + 9.f;
			SayBark(L, S, B_Rhythm, FMath::RandRange(0, N - 1), 1.f);
		}
		S.DesyncT = (bMoving && S.Sync < 0.45f) ? S.DesyncT + Dt : 0.f;
		if (S.DesyncT > 2.f)
		{
			S.DesyncT = -6.f;
			SayBark(L, S, B_Together, FMath::RandRange(0, N - 1));
		}
		const float Ratio = L->Weight > 0 ? L->CarryStrength / (float)L->Weight : 10.f;
		S.HeavyT = (Ratio < 1.f && Mean > 20.f) ? S.HeavyT + Dt : 0.f;
		if (S.HeavyT > 2.5f)
		{
			S.HeavyT = -10.f;
			SayBark(L, S, B_Heavy, FMath::RandRange(0, N - 1));
		}
		// strides: each carrier's beat runs with its own speed; moving as one, the beats lock together
		float Cs = 0.f, Sn = 0.f;
		for (const FAntCarryState::FMate& M : S.Mates) { Cs += FMath::Cos(2.f * PI * M.Phase); Sn += FMath::Sin(2.f * PI * M.Phase); }
		const float MeanPh = FMath::Atan2(Sn, Cs) / (2.f * PI);
		for (int32 i = 0; i < N; ++i)
		{
			FAntCarryState::FMate& M = S.Mates[i];
			const float Pull = FMath::Sin(2.f * PI * (MeanPh - M.Phase));
			M.Phase = Frac(M.Phase + (float)Flat(V[i]).Size() / K.Beat * Dt + K.Entrain * S.Sync * Pull / (2.f * PI) * Dt);
		}
		// tug of war: shout at the thieves now and then
		bool bOther = false;
		for (int32 t : Team) { bOther |= t != Win; }
		if (bOther && FMath::FRand() < Dt * 0.3f)
		{
			for (int32 i = 0; i < N; ++i) { if (Team[i] == 0) { SayBark(L, S, B_Mine, i, 3.f); break; } }
		}
	}

	// ---- the forces
	const FVector LoadV = B->GetPhysicsLinearVelocity();
	const float LoadSp = (float)Flat(LoadV).Size();
	const FVector LoadDir = Flat(LoadV).GetSafeNormal();
	float MoveK = 0.f;
	for (const FVector& Vi : V) { MoveK += (float)Flat(Vi).Size(); }
	MoveK = FMath::Clamp(MoveK / N / 120.f, 0.f, 1.f);
	const float SyncEff = 0.9f + 0.2f * S.Sync + (S.bRhythm ? 0.05f : 0.f);
	const float Reserve = L->bMain ? K.MainReserve : K.Reserve;
	TArray<FVector, TInlineAllocator<24>> F;
	F.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const float Ms = Mass / N;
		const FVector Vg = B->GetPhysicsLinearVelocityAtPoint(P[i]);
		const FVector Vrel = Vg - V[i];
		FAntCarryState::FMate& M = S.Mates[i];
		M.Vrel += (Flat(Vrel) - M.Vrel) * (1.f - FMath::Exp(-Dt / 0.06f));
		const float Beat = 1.f + K.BeatAmp * MoveK * FMath::Sin(2.f * PI * M.Phase);
		F[i] = Ms * (K.K * (H[i] - P[i]) - K.C * Vrel) + FVector(0.f, 0.f, Share[i] * Mass * G * Beat);
		if (bLift && Team[i] == Win)
		{
			F[i].Z = FMath::Max((float)F[i].Z, 0.f);   // a carrier standing lower than the load does not drag it down
		}
	}
	// a side cannot hold up more than it has strength (the rest of the weight stays on the floor: it drags)
	for (int32 t = 0; t < 2; ++t)
	{
		float Up = 0.f;
		for (int32 i = 0; i < N; ++i) { if (Team[i] == t) { Up += FMath::Max(0.f, (float)F[i].Z); } }
		const float Cap = (bLift && t == Win ? FMath::Max(TeamStr[t], K.MainReserve * L->Weight) : TeamStr[t]) * FPS * SyncEff;
		if (Up > Cap && Up > 0.f)
		{
			for (int32 i = 0; i < N; ++i) { if (Team[i] == t && F[i].Z > 0.f) { F[i].Z *= Cap / Up; } }
		}
	}
	for (int32 i = 0; i < N; ++i)
	{
		FAntCarryState::FMate& M = S.Mates[i];
		const float Own = FMath::Max(0.5f, Str[i]) * FPS;
		// Pushing ahead the claws dig in (strength is the limit); braking and turning a running load the feet slip: friction of
		// what presses them down (their share of the load and their own weight). Easy to start, hard to stop or turn.
		if (LoadSp > 60.f)
		{
			const float Normal = (Share[i] * Mass + K.AntW * AKitchenLoot::KgPerWeight) * G;
			const FVector Fh = Flat(F[i]);
			float Along = (float)FVector::DotProduct(Fh, LoadDir);
			FVector Side = Fh - LoadDir * Along;
			const float MaxBrake = K.Brake * Normal, MaxSide = K.SideMu * Normal;
			if (Along < -MaxBrake) { Along = -MaxBrake; }
			if (Side.Size() > MaxSide) { Side = Side.GetSafeNormal() * MaxSide; }
			F[i] = LoadDir * Along + Side + FVector(0.f, 0.f, F[i].Z);
		}
		const float Max = Own * Reserve * SyncEff;
		const float Asked = (float)F[i].Size();
		M.bAtLimit = Asked > Max;
		if (M.bAtLimit)
		{
			F[i] *= Max / Asked;
		}
		M.Strain += (FMath::Min(Asked, Max) / Own - M.Strain) * (1.f - FMath::Exp(-Dt / 0.15f));
		M.Share = Share[i];
		M.Pull = Flat(F[i]) / Own;
		B->AddForceAtLocation(F[i], P[i]);
	}
	// The jaws keep the load the way it was taken (it does not topple off the grips): a tilt spring, limited by the winning
	// side's strength; turning about the vertical stays free (twisting is the carriers' business).
	{
		const FVector Up = L->GetActorQuat().GetUpVector();
		const FVector Axis = FVector::CrossProduct(Up, S.UpRef);
		const FVector Wang = B->GetPhysicsAngularVelocityInRadians();
		const FVector Wtilt = Wang - FVector::UpVector * Wang.Z;
		const float R = L->GetRadius();
		const float I = Mass * R * R * 0.4f;
		FVector Tq = I * (K.Upright * Axis - 5.f * Wtilt);
		const float MaxTq = TeamStr[Win] * FPS * R * 0.5f;
		if (Tq.Size() > MaxTq) { Tq = Tq.GetSafeNormal() * MaxTq; }
		B->AddTorqueInRadians(Tq);
	}
	// no explosions: a carried load never goes faster than a thrown one, nor spins like a top
	if (LoadV.Size() > K.MaxLoadSpeed)
	{
		B->SetPhysicsLinearVelocity(LoadV.GetSafeNormal() * K.MaxLoadSpeed);
	}
	const FVector Spin = B->GetPhysicsAngularVelocityInDegrees();
	if (Spin.Size() > K.MaxSpin)
	{
		B->SetPhysicsAngularVelocityInDegrees(Spin.GetSafeNormal() * K.MaxSpin);
	}

	Yanks(L, S, Dt, LoadV);
	RunOver(L, LoadSp);
	const bool bSend = Now >= S.SendAt;
	if (bSend) { S.SendAt = Now + 0.05f; }
	Publish(L, S, bSend);
	if (S.Mates.Num() != N || L->Carriers.Num() + S.NumWorkers() != N)
	{
		AntCarry::ServerCarriersChanged(L);
	}
}

// =============================================================================================
// Clients: the load follows the server
// =============================================================================================

void AntCarry::ClientTick(AKitchenLoot* L, float Dt)
{
	if (!L || L->HasAuthority() || L->bIsGear)
	{
		return;
	}
	FAntCarryState& S = FAntCarrySolver::St(L);
	UBoxComponent* B = FAntCarrySolver::Body(L);
	const FAntCarryNet& Net = L->CarryNet;
	if (!B)
	{
		return;
	}
	if (!Net.bActive || L->bHeld)
	{
		if (S.bKinematic)
		{
			S.bKinematic = false;
			S.Snaps.Reset();
			S.LastStamp = -1.f;
			if (!L->bHeld && FAntCarrySolver::PhysicsReady(L))
			{
				B->SetSimulatePhysics(true);
				B->SetPhysicsLinearVelocity(Net.Vel);
			}
		}
		return;
	}
	if (!S.bKinematic)
	{
		// the server drives it now: no local gravity, no local collisions pushing it about between updates
		B->SetSimulatePhysics(false);
		S.bKinematic = true;
		S.Snaps.Reset();
		S.RenderT = 0.0;
	}
	if (Net.Stamp != S.LastStamp)
	{
		S.LastStamp = Net.Stamp;
		FAntCarryState::FSnap Sn;
		Sn.T = Net.Stamp;
		Sn.P = Net.Pos;
		Sn.Q = Net.Rot.Quaternion();
		Sn.V = Net.Vel;
		if (S.Snaps.Num() > 0 && Sn.T <= S.Snaps.Last().T)
		{
			S.Snaps.Reset();
		}
		S.Snaps.Add(Sn);
		if (S.Snaps.Num() > 8) { S.Snaps.RemoveAt(0); }
	}
	if (S.Snaps.Num() == 0)
	{
		return;
	}
	// Shown 80 ms behind the server (a 20 Hz stream has two samples to interpolate between); the clock runs smoothly and is
	// nudged towards that target, never jumps (unless it is far off: a hitch, a late joiner).
	const double Target = ServerNow(L) - 0.08;
	if (S.RenderT <= 0.0 || FMath::Abs(S.RenderT - Target) > 0.5)
	{
		S.RenderT = Target;
	}
	else
	{
		S.RenderT += Dt;
		S.RenderT += (Target - S.RenderT) * 0.1;
	}
	const TArray<FAntCarryState::FSnap>& Sn = S.Snaps;
	FVector Pos;
	FQuat Rot;
	if (S.RenderT <= Sn[0].T)
	{
		Pos = Sn[0].P; Rot = Sn[0].Q;
	}
	else if (S.RenderT >= Sn.Last().T)
	{
		const float Ahead = FMath::Min((float)(S.RenderT - Sn.Last().T), 0.2f);   // a late packet: carry on for a moment
		Pos = Sn.Last().P + Sn.Last().V * Ahead;
		Rot = Sn.Last().Q;
	}
	else
	{
		int32 i = 0;
		while (i < Sn.Num() - 2 && Sn[i + 1].T < S.RenderT) { ++i; }
		const FAntCarryState::FSnap& A = Sn[i];
		const FAntCarryState::FSnap& Bn = Sn[i + 1];
		const float Span = FMath::Max(1.e-3f, (float)(Bn.T - A.T));
		const float U = FMath::Clamp((float)(S.RenderT - A.T) / Span, 0.f, 1.f);
		// Hermite on the position (the velocities make the curve between samples round, not a polyline)
		Pos = FMath::CubicInterp(A.P, A.V * Span, Bn.P, Bn.V * Span, U);
		Rot = FQuat::Slerp(A.Q, Bn.Q, U);
	}
	L->SetActorLocationAndRotation(Pos, Rot, false, nullptr, ETeleportType::TeleportPhysics);

	// Dev (-AntCarryClientShots=20,24,28): a client looks at the carried load from the side and takes a picture at these seconds
	// (Saved/Screenshots/carry/client_NN.png) - to see the replicated load on a client.
	static TArray<float> ShotAt = [] { TArray<float> Out; FString V; if (FParse::Value(FCommandLine::Get(), TEXT("AntCarryClientShots="), V, false)) { TArray<FString> P; V.ParseIntoArray(P, TEXT(",")); for (const FString& X : P) { Out.Add(FCString::Atof(*X)); } } return Out; }();
	static uint64 ShotFrame = 0;
	if (ShotAt.Num() > 0 && ShotFrame != GFrameCounter && Net.Grips.Num() > 1)   // (the first load with several carriers this frame)
	{
		ShotFrame = GFrameCounter;
		static TWeakObjectPtr<ACameraActor> Cam;
		static int32 Shot = 0;
		APlayerController* PC = L->GetWorld()->GetFirstPlayerController();
		if (PC && Net.Grips.Num() > 1)
		{
			if (!Cam.IsValid())
			{
				Cam = L->GetWorld()->SpawnActor<ACameraActor>();
				if (Cam.IsValid()) { Cam->GetCameraComponent()->SetFieldOfView(55.f); Cam->GetCameraComponent()->bConstrainAspectRatio = false; }
			}
			if (ACameraActor* C = Cam.Get())
			{
				const FVector Off(0.f, -(L->GetRadius() * 1.6f + 150.f), 90.f);
				C->SetActorLocationAndRotation(Pos + Off, (-Off).Rotation());
				if (PC->GetViewTarget() != C) { PC->SetViewTargetWithBlend(C, 0.f); }
			}
			const float Tw = L->GetWorld()->GetTimeSeconds();
			if (ShotAt.IsValidIndex(Shot) && Tw >= ShotAt[Shot])
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("carry") / FString::Printf(TEXT("client_%02d.png"), Shot), true, false);
				UE_LOG(LogTemp, Display, TEXT("ANTCARRY client shot %d at %.1f s of «%s» (%d carriers)"), Shot, Tw, *L->LootName, Net.Grips.Num());
				++Shot;
			}
		}
	}

	// Dev (-AntCarryClientLog): how smooth the load moves here - the mean and worst "kink" (second difference of the shown
	// position per frame) next to its speed, once a second.
	static const bool bClientLog = FParse::Param(FCommandLine::Get(), TEXT("AntCarryClientLog"));
	if (bClientLog)
	{
		static TMap<TWeakObjectPtr<AKitchenLoot>, FVector4> Acc;   // sum kink, worst kink, frames, time
		static TMap<TWeakObjectPtr<AKitchenLoot>, TPair<FVector, FVector>> Prev;
		TPair<FVector, FVector>& Pv = Prev.FindOrAdd(L, TPair<FVector, FVector>(Pos, Pos));
		const float Kink = (float)(Pos - 2.0 * Pv.Key + Pv.Value).Size();
		Pv.Value = Pv.Key;
		Pv.Key = Pos;
		FVector4& A = Acc.FindOrAdd(L, FVector4(0, 0, 0, 0));
		A.X += Kink; A.Y = FMath::Max((float)A.Y, Kink); A.Z += 1.f; A.W += Dt;
		if (A.W >= 1.f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY client «%s» at %s speed %.0f carriers %d snaps %d lag %.3f s kink mean %.2f worst %.2f (frames %d)"),
				*L->LootName, *Pos.ToCompactString(), Net.Vel.Size(), Net.Grips.Num(), S.Snaps.Num(), (float)(ServerNow(L) - S.Snaps.Last().T), A.X / FMath::Max(1.0, A.Z), A.Y, (int32)A.Z);
			A = FVector4(0, 0, 0, 0);
		}
	}
}

// =============================================================================================
// The carrier's side (every machine)
// =============================================================================================

bool AntCarry::GripWorld(const AAntCharacter* Ant, FVector& Out)
{
	const AKitchenLoot* L = Ant ? Ant->Carrying : nullptr;
	if (!L)
	{
		return false;
	}
	if (L->HasAuthority())
	{
		const int32 k = L->Carriers.IndexOfByKey(Ant);
		if (k == INDEX_NONE || !FAntCarrySolver::GrabLocal(L).IsValidIndex(k))
		{
			return false;
		}
		Out = AntGrip(L, k);
		return true;
	}
	const FAntCarryGrip* G = NetGrip(L, Ant, -1);
	if (!G)
	{
		return false;
	}
	Out = L->GetActorTransform().TransformPosition(G->Local);
	return true;
}

bool AntCarry::HoldPoint(const AAntCharacter* Ant, FVector& Out)
{
	if (!Ant || !Ant->Carrying || Ant->Creature != 0)
	{
		return false;
	}
	FVector G;
	if (!GripWorld(Ant, G))
	{
		return false;
	}
	// The jaws reach towards the grip: the load stays at the tip of the mandibles whichever way the ant walks (it backs up
	// dragging, walks sideways along a big load, pushes from behind).
	const FVector Up = Ant->GetVisualUp();
	const FVector P = Ant->GetActorLocation();
	FVector Dir = FVector::VectorPlaneProject(G - P, Up);
	if (Dir.SizeSquared() < 1.f)
	{
		Dir = FVector::VectorPlaneProject(Ant->GetActorForwardVector(), Up);
	}
	Dir = Dir.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	static const float HoldUp = [] { float V = AntRig::Enabled() ? 20.f : 30.f; FParse::Value(FCommandLine::Get(), TEXT("AntHoldUp="), V); return V; }();
	static const float Reach = Opt(TEXT("AntCarryReach="), 30.f);
	const float S = Ant->VisScale() / AAntCharacter::AntScale;
	Out = P + (Up * HoldUp + Dir * Reach) * AAntCharacter::AntScale * S * 1.3f;
	return true;
}

FVector AntCarry::Tether(const AAntCharacter* Ant, const FVector& N, float Dt)
{
	FVector G, J;
	if (!Ant || !Ant->Carrying || Ant->Creature != 0 || !GripWorld(Ant, G) || !HoldPoint(Ant, J))
	{
		return FVector::ZeroVector;
	}
	const FVector Err = FVector::VectorPlaneProject(G - J, N);
	const float D = (float)Err.Size() - CT().TetherSlack;
	if (D <= 0.f || D > 160.f)
	{
		return FVector::ZeroVector;   // (far off: the server is about to take the load from it)
	}
	return Err.GetSafeNormal() * FMath::Min(CT().TetherMax, CT().TetherK * D);
}

float AntCarry::SpeedMul(const AAntCharacter* Ant, float OldLoad)
{
	const AKitchenLoot* L = Ant ? Ant->Carrying : nullptr;
	if (!L || L->CarryTeam() != Ant->Team || Ant->Creature != 0)
	{
		return OldLoad;
	}
	const int32 N = FMath::Max(1, L->CarryNet.Grips.Num());
	float K = LoadFactor(L->CarryRatio(), N);
	if (const FAntCarryGrip* G = NetGrip(L, Ant, -1))
	{
		K *= 1.f - 0.3f * FMath::Clamp(G->Strain / 100.f - 0.8f, 0.f, 1.f);   // the one at the heavy end shuffles
	}
	if (N > 1)
	{
		K *= (0.85f + 0.15f * L->CarryNet.Sync / 255.f) * (L->CarryNet.bRhythm ? 1.1f : 1.f);
	}
	return K;
}

bool AntCarry::Heading(const AAntCharacter* Ant, const FVector& N, FVector& InOut)
{
	FVector G;
	if (!Ant || !Ant->Carrying || Ant->Creature != 0 || !GripWorld(Ant, G))
	{
		return false;
	}
	const FVector D = FVector::VectorPlaneProject(G - Ant->GetActorLocation(), N);
	if (D.SizeSquared() > 1.f)
	{
		InOut = D.GetSafeNormal();
	}
	return true;
}

bool AntCarry::Feel(const AAntCharacter* Ant, FAntCarryFeel& Out)
{
	const AKitchenLoot* L = Ant ? Ant->Carrying : nullptr;
	FVector J;
	if (!L || !GripWorld(Ant, Out.Grip) || !HoldPoint(Ant, J))
	{
		return false;
	}
	Out.Pull = FVector::VectorPlaneProject(Out.Grip - J, Ant->GetVisualUp());
	Out.Sync = L->CarryNet.Sync / 255.f;
	Out.bRhythm = L->CarryNet.bRhythm;
	Out.Carriers = L->CarryNet.Grips.Num();
	if (const FAntCarryGrip* G = NetGrip(L, Ant, -1))
	{
		Out.Strain = G->Strain / 100.f;
		Out.Share = G->Share / 255.f;
		Out.bDragged = (G->Flags & 1) != 0;
	}
	return true;
}

bool AntCarry::JustTumbled(const AAntCharacter* Ant)
{
	const TPair<float, TWeakObjectPtr<AKitchenLoot>>* P = Tumbled().Find(Ant);
	return P && Ant && Ant->GetWorld() && Ant->GetWorld()->GetTimeSeconds() - P->Key < 6.f && P->Value.IsValid();
}

bool AntCarry::WorkerPose(const AAntArmy* Army, int32 Worker, FVector& Pos, FVector& Dir, float& Lift)
{
	if (!Army || Worker < 0 || Worker > 254)
	{
		return false;
	}
	// Which loads this army's workers hold: one pass over the carried loads per frame, for every army at once.
	static uint64 Frame = 0;
	static TMap<const AAntArmy*, TArray<TPair<TWeakObjectPtr<AKitchenLoot>, int32>>> Held;
	if (Frame != GFrameCounter)
	{
		Frame = GFrameCounter;
		Held.Reset();
		for (TActorIterator<AKitchenLoot> It(Army->GetWorld()); It; ++It)
		{
			const FAntCarryNet& Net = It->CarryNet;
			if (!Net.bActive) { continue; }
			for (int32 g = 0; g < Net.Grips.Num(); ++g)
			{
				if (Net.Grips[g].Worker != 255 && Net.Grips[g].Who) { Held.FindOrAdd(Cast<AAntArmy>(Net.Grips[g].Who)).Add(TPair<TWeakObjectPtr<AKitchenLoot>, int32>(*It, g)); }
			}
		}
	}
	const TArray<TPair<TWeakObjectPtr<AKitchenLoot>, int32>>* List = Held.Find(Army);
	if (!List)
	{
		return false;
	}
	for (const TPair<TWeakObjectPtr<AKitchenLoot>, int32>& E : *List)
	{
		const AKitchenLoot* L = E.Key.Get();
		if (!L || !L->CarryNet.Grips.IsValidIndex(E.Value) || L->CarryNet.Grips[E.Value].Worker != Worker)
		{
			continue;
		}
		const FAntCarryGrip* G = &L->CarryNet.Grips[E.Value];
		const FVector Gw = L->GetActorTransform().TransformPosition(G->Local);
		const FVector Out = Flat(Gw - L->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		Dir = -Out;
		Pos = Gw + Out * 4.5f;
		// on the floor under its jaws; dragged: it hangs from the grip, feet off the ground
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("AntCarryWorkerPose"), false, L);
		float FloorZ = (float)Gw.Z - 7.f;
		if (Army->GetWorld()->LineTraceSingleByObjectType(Hit, Pos + FVector(0.f, 0.f, 3.f), Pos - FVector(0.f, 0.f, 60.f), UAntMovementComponent::SurfaceObjects(), Q))
		{
			FloorZ = (float)Hit.ImpactPoint.Z;
		}
		const bool bDragged = (G->Flags & 1) != 0;
		Lift = bDragged ? 1.f : 0.f;
		Pos.Z = bDragged ? FMath::Max(FloorZ + 2.f, (float)Gw.Z - 6.f) : FMath::Max(FloorZ, (float)Gw.Z - 9.f);
		return true;
	}
	return false;
}

float AntCarry::CakePlayerMul(int32 Players)
{
	const int32 P = FMath::Clamp(Players, 1, 8);
	// +20 % for each player up to four (as before); from five on +12 % each: the cake is already as big as a dinner plate for
	// an ant, and with eight the whole team (plus squads) has to hold it - 1.0 1.2 1.4 1.6 1.72 1.84 1.96 2.08.
	return 1.f + 0.2f * (FMath::Min(P, 4) - 1) + 0.12f * FMath::Max(0, P - 4);
}

// =============================================================================================
// HUD: strain marks and shouts
// =============================================================================================

void AntCarry::DrawHud(AHUD* Hud, UCanvas* Canvas, APawn* Me)
{
	if (!Hud || !Canvas || !Hud->PlayerOwner || !Hud->PlayerOwner->PlayerCameraManager || !GEngine)
	{
		return;
	}
	UWorld* W = Hud->GetWorld();
	const FVector Cam = Hud->PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = Hud->PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	const AAntCharacter* MyAnt = Cast<AAntCharacter>(Me);
	const float Now = W->GetTimeSeconds();
	UFont* Font = GEngine->GetMediumFont();
	const float Ui = FMath::Max(0.7f, Canvas->ClipY / 1080.f);
	auto OnScreen = [&](const FVector& P, FVector2D& Out)
	{
		if (FVector::DotProduct(P - Cam, CamFwd) < 1.f) { return false; }
		const FVector S = Canvas->Project(P);
		Out = FVector2D(S.X, S.Y);
		return S.X > -50 && S.Y > -50 && S.X < Canvas->ClipX + 50 && S.Y < Canvas->ClipY + 50;
	};
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		const FAntCarryNet& Net = L->CarryNet;
		if (!Net.bActive || Net.Grips.Num() == 0)
		{
			continue;
		}
		const bool bMine = MyAnt && MyAnt->Carrying == L;
		const float Dist = (float)FVector::Dist(Cam, L->GetActorLocation());
		if (!bMine && Dist > 1600.f)
		{
			continue;
		}
		const float Alpha = bMine ? 0.85f : FMath::Clamp((1600.f - Dist) / 600.f, 0.f, 0.7f);
		const FTransform TM = L->GetActorTransform();
		// one small arrow per carrier: where it pulls, how hard (green - fine, yellow - working, red - at its limit)
		if (Net.Grips.Num() > 1 || bMine)
		{
			for (const FAntCarryGrip& G : Net.Grips)
			{
				const FVector P = TM.TransformPosition(G.Local);
				const FVector Pull(G.PullX / 127.f, G.PullY / 127.f, 0.f);
				const float S = G.Strain / 100.f;
				const FLinearColor C = (G.Flags & 2) || S > 1.05f ? FLinearColor(1.f, 0.25f, 0.15f, Alpha) : S > 0.75f ? FLinearColor(1.f, 0.8f, 0.2f, Alpha) : FLinearColor(0.45f, 1.f, 0.45f, Alpha);
				FVector2D A, Bp;
				const FVector End = P + Pull.GetClampedToMaxSize(1.5f) * 14.f + FVector(0.f, 0.f, 2.f);
				if (OnScreen(P + FVector(0.f, 0.f, 2.f), A) && OnScreen(End, Bp))
				{
					Hud->DrawLine(A.X, A.Y, Bp.X, Bp.Y, C, 2.f * Ui);
					const FVector2D D = (Bp - A).GetSafeNormal();
					const FVector2D Nn(-D.Y, D.X);
					Hud->DrawLine(Bp.X, Bp.Y, Bp.X - (D.X - Nn.X * 0.6f) * 6.f * Ui, Bp.Y - (D.Y - Nn.Y * 0.6f) * 6.f * Ui, C, 2.f * Ui);
					Hud->DrawLine(Bp.X, Bp.Y, Bp.X - (D.X + Nn.X * 0.6f) * 6.f * Ui, Bp.Y - (D.Y + Nn.Y * 0.6f) * 6.f * Ui, C, 2.f * Ui);
				}
			}
		}
		// the rhythm, as a word over the load (only for the ones who carry it)
		FVector2D Top;
		if (bMine && Net.Grips.Num() > 1 && OnScreen(L->GetActorLocation() + FVector(0.f, 0.f, L->GetRadius() + 10.f), Top))
		{
			const float Sy = Net.Sync / 255.f;
			const FString Word = Net.bRhythm ? TEXT("раз-два · в ногу") : Sy < 0.5f ? TEXT("вразнобой") : TEXT("");
			if (!Word.IsEmpty())
			{
				float Tw = 0.f, Th = 0.f;
				Hud->GetTextSize(Word, Tw, Th, Font, 0.9f * Ui);
				Hud->DrawText(Word, Net.bRhythm ? FLinearColor(1.f, 0.85f, 0.3f, 0.9f) : FLinearColor(1.f, 0.55f, 0.3f, 0.8f), Top.X - Tw * 0.5f, Top.Y - Th, Font, 0.9f * Ui);
			}
		}
		// a shout: 1.8 s over the head of the one who shouted
		FAntCarryState& St = FAntCarrySolver::St(L);
		if (Net.BarkSeq != St.SeenBarkSeq)
		{
			St.SeenBarkSeq = Net.BarkSeq;
			St.BarkShownAt = Now;
			St.ShownBark = Net.Bark;
			St.ShownBy = Net.BarkBy;
		}
		if (Now - St.BarkShownAt < 1.8f && St.ShownBark > 0 && Net.Grips.IsValidIndex(St.ShownBy))
		{
			const FAntCarryGrip& G = Net.Grips[St.ShownBy];
			const FVector At = (G.Worker == 255 && G.Who ? G.Who->GetActorLocation() : TM.TransformPosition(G.Local)) + FVector(0.f, 0.f, 16.f);
			FVector2D Sp;
			if (OnScreen(At, Sp))
			{
				const FString Txt = BarkText(St.ShownBark);
				const float Rise = (Now - St.BarkShownAt) * 12.f * Ui;
				float Tw = 0.f, Th = 0.f;
				Hud->GetTextSize(Txt, Tw, Th, Font, 1.f * Ui);
				const float Fade = FMath::Clamp((1.8f - (Now - St.BarkShownAt)) / 0.4f, 0.f, 1.f);
				Hud->DrawText(Txt, FLinearColor(0.f, 0.f, 0.f, 0.7f * Fade), Sp.X - Tw * 0.5f + 1.5f, Sp.Y - Th - Rise + 1.5f, Font, 1.f * Ui);
				Hud->DrawText(Txt, FLinearColor(1.f, 1.f, 0.92f, Fade), Sp.X - Tw * 0.5f, Sp.Y - Th - Rise, Font, 1.f * Ui);
			}
		}
	}
}

// =============================================================================================
// Dev: scripted carrying tests (-AntDo "carry ...")
// =============================================================================================

namespace
{
	struct FDev
	{
		TWeakObjectPtr<AKitchenLoot> Load;
		TArray<TWeakObjectPtr<AAntCharacter>> Crew;
		TArray<TWeakObjectPtr<AAntCharacter>> Rivals;
		TMap<TWeakObjectPtr<AAntCharacter>, FVector4> Own;   // per bot: dir x y, scale, until
		FVector Goal = FVector::ZeroVector;
		bool bGo = false;
		float Speed = 1.f;
		TWeakObjectPtr<ACameraActor> Cam;
		FString CamMode;
		FVector CamOff = FVector::ZeroVector;
		float CamFov = 40.f;
		bool bLog = false;
		float NextLog = 0.f;
		float NextRegrab = 0.f;
		FTimerHandle Ticker;
		TWeakObjectPtr<AAntArmy> Army;
		TWeakObjectPtr<UWorld> World;
	};
	FDev& Dev() { static FDev D; return D; }

	FVector GroupDir()
	{
		FDev& D = Dev();
		AKitchenLoot* L = D.Load.Get();
		if (!L || !D.bGo) { return FVector::ZeroVector; }
		const FVector To = Flat(D.Goal - L->GetActorLocation());
		return To.Size() < 40.f ? FVector::ZeroVector : To.GetSafeNormal();
	}

	FString Describe(AKitchenLoot* L)
	{
		UBoxComponent* B = FAntCarrySolver::Body(L);
		const FVector C = L->GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("AntCarryDev"), false, L);
		float Above = -1.f;
		const FVector Ext = B ? B->GetScaledBoxExtent() : FVector(10.f);
		if (L->GetWorld()->LineTraceSingleByObjectType(Hit, C, C - FVector(0.f, 0.f, 600.f), UAntMovementComponent::SurfaceObjects(), Q))
		{
			// lowest corner of the box above the floor under the centre
			float Low = 1.e9f;
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector Corner((i & 1) ? Ext.X : -Ext.X, (i & 2) ? Ext.Y : -Ext.Y, (i & 4) ? Ext.Z : -Ext.Z);
				Low = FMath::Min(Low, (float)L->GetActorTransform().TransformPosition(Corner).Z);
			}
			Above = Low - (float)Hit.ImpactPoint.Z;
		}
		const FVector Up = L->GetActorQuat().GetUpVector();
		const float Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)Up.Z, -1.f, 1.f)));
		const FVector V = B ? B->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		const float Spin = B ? (float)B->GetPhysicsAngularVelocityInDegrees().Size() : 0.f;
		const FAntCarryNet& Net = L->CarryNet;
		FString Who;
		for (const FAntCarryGrip& G : Net.Grips)
		{
			const AAntCharacter* A = G.Worker == 255 ? Cast<AAntCharacter>(G.Who) : nullptr;
			Who += FString::Printf(TEXT(" [%s s%.2f sh%.2f%s%s]"), A ? *A->GetDisplayName().Right(10) : *FString::Printf(TEXT("w%d"), G.Worker),
				G.Strain / 100.f, G.Share / 255.f, (G.Flags & 1) ? TEXT(" DRAG") : TEXT(""), (G.Flags & 2) ? TEXT(" LIM") : TEXT(""));
		}
		return FString::Printf(TEXT("«%s» w%d at %s v=%.0f (h %.0f) above_floor=%.1f tilt=%.0f spin=%.0f ratio=%.2f carriers=%d sync=%.2f rhythm=%d team=%d%s"),
			*L->LootName, L->Weight, *C.ToCompactString(), V.Size(), Flat(V).Size(), Above, Tilt, Spin, L->CarryRatio(), Net.Grips.Num(), Net.Sync / 255.f, Net.bRhythm ? 1 : 0, (int32)L->CarryTeam(), *Who);
	}

	void DevTick()
	{
		FDev& D = Dev();
		UWorld* W = D.World.Get();
		if (!W) { return; }
		AKitchenLoot* L = D.Load.Get();
		APlayerController* PC = W->GetFirstPlayerController();
		AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
		const float Now = W->GetTimeSeconds();
		// the player walks with the group (the dev walker of AAntCharacter, pointed half a step ahead)
		if (Me && D.Crew.Contains(Me) && Me->Carrying)
		{
			const FVector4* O = D.Own.Find(Me);
			FVector Dir = (O && O->W > Now) ? FVector(O->X, O->Y, 0.f) : GroupDir();
			const float Sc = (O && O->W > Now) ? O->Z : D.Speed;
			Me->bDevGo = !Dir.IsNearlyZero() && Sc > 0.f;
			Me->DevGoTo = Me->GetActorLocation() + Dir * 400.f;
		}
		// the scripted crew takes hold again after a tumble (a player presses [E]; bots do it in their own AI)
		if (L && Now >= D.NextRegrab)
		{
			D.NextRegrab = Now + 0.5f;
			for (const TWeakObjectPtr<AAntCharacter>& C : D.Crew)
			{
				AAntCharacter* A = C.Get();
				if (A && !A->bDead && !A->Carrying && FVector::Dist(A->GetActorLocation(), L->GetActorLocation()) < L->GetRadius() + 25.f) { A->GrabLoot(L); }
			}
			if (Me && D.Crew.Contains(Me) && !Me->Carrying && !Me->bDead)
			{
				Me->bDevGo = true;   // back to the load
				Me->DevGoTo = L->GetActorLocation();
			}
		}
		if (L && D.Cam.IsValid() && !D.CamMode.IsEmpty())
		{
			// framed on the carriers (the middle of their grips), not on the middle of a load 15 ants tall
			FVector C = L->GetActorLocation();
			if (L->CarryNet.Grips.Num() > 0)
			{
				FVector Sum = FVector::ZeroVector;
				for (const FAntCarryGrip& G : L->CarryNet.Grips) { Sum += L->GetActorTransform().TransformPosition(G.Local); }
				C = Sum / L->CarryNet.Grips.Num();
			}
			D.Cam->SetActorLocationAndRotation(C + D.CamOff, (-D.CamOff).Rotation());
		}
		if (L && D.bLog && Now >= D.NextLog)
		{
			D.NextLog = Now + 0.5f;
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY state t=%.1f %s"), Now, *Describe(L));
		}
	}

	AKitchenLoot* SpawnTestLoad(UWorld* W, const FString& Kind, const FVector& At, int32 Players, int32 ForceWeight)
	{
		FAntFoodDef Def;
		bool bCake = false;
		if (Kind == TEXT("bear")) { Def.Name = TEXT("Золотой мармеладный мишка"); Def.Value = 260; Def.Weight = 48; Def.Color = FLinearColor(1.f, 0.75f, 0.1f); Def.Shape = EAntShape::Sphere; Def.Scale = FVector(0.55f); }
		else if (Kind == TEXT("gummy")) { Def.Name = TEXT("Мармеладка"); Def.Value = 60; Def.Weight = 12; Def.Color = FLinearColor(0.9f, 0.2f, 0.4f); Def.Shape = EAntShape::Sphere; Def.Scale = FVector(0.35f); }
		else if (Kind == TEXT("box")) { Def.Name = TEXT("Коробочка хлопьев"); Def.Value = 90; Def.Weight = 24; Def.Color = FLinearColor(0.95f, 0.45f, 0.1f); Def.Shape = EAntShape::Cube; Def.Scale = FVector(1.f, 0.3f, 0.42f); }
		else if (Kind == TEXT("cheese")) { Def.Name = TEXT("Кусочек сыра"); Def.Value = 45; Def.Weight = 22; Def.Color = FLinearColor(1.f, 0.8f, 0.15f); Def.Shape = EAntShape::Cube; Def.Scale = FVector(0.4f, 0.3f, 0.2f); }
		else if (Kind == TEXT("cake")) { bCake = true; Def.Name = TEXT("Кусок именинного торта"); Def.Value = 600; Def.Weight = FMath::RoundToInt(48.f * AntCarry::CakePlayerMul(Players)); Def.Color = FLinearColor(1.f, 0.55f, 0.7f); Def.Shape = EAntShape::Cube; Def.Scale = FVector(0.8f, 0.55f, 0.45f); }
		else if (AntFood::TestDef(Kind, Def)) {}   // any food of AntFood's table by id (sponge, apple, grape...) or by kind (crumbly, elastic...)
		else { Def.Name = TEXT("Хлебная крошка"); Def.Value = 5; Def.Weight = 2; Def.Color = FLinearColor(0.85f, 0.65f, 0.35f); Def.Shape = EAntShape::Cube; Def.Scale = FVector(0.2f); }
		if (ForceWeight > 0) { Def.Weight = ForceWeight; }
		const FTransform TM(At);
		AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!L) { return nullptr; }
		L->InitFood(Def);
		L->bMain = bCake;
		L->bTrophy = bCake;
		L->FinishSpawning(TM);
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev spawn «%s» weight %d radius %.0f at %s (fits %d carriers)"), *L->LootName, L->Weight, L->GetRadius(), *At.ToCompactString(), AntCarry::MaxCarriers(L));
		return L;
	}

	AStaticMeshActor* Slab(UWorld* W, const FVector& At, const FRotator& Rot, const FVector& Size, const FLinearColor& Col)
	{
		AStaticMeshActor* S = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rot, At));
		if (!S) { return nullptr; }
		UStaticMeshComponent* M = S->GetStaticMeshComponent();
		M->SetMobility(EComponentMobility::Movable);
		M->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cube));
		M->SetWorldScale3D(Size / 100.f);
		M->SetCollisionObjectType(ECC_WorldStatic);
		M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		M->SetCollisionResponseToAllChannels(ECR_Block);
		AntKit::Paint(M, Col);
		return S;
	}
}

bool AntCarry::DevSteer(const AAntCharacter* Ant, FVector& OutDir, float& OutScale)
{
	FDev& D = Dev();
	const float Now = Ant && Ant->GetWorld() ? Ant->GetWorld()->GetTimeSeconds() : 0.f;
	TWeakObjectPtr<AAntCharacter> Key(const_cast<AAntCharacter*>(Ant));
	if (D.Rivals.Contains(Key))
	{
		// the thieves pull the other way
		const FVector G = GroupDir();
		OutDir = G.IsNearlyZero() ? FVector(-1.f, 0.f, 0.f) : -G;
		OutScale = 1.f;
		return true;
	}
	if (D.Crew.Contains(Key))
	{
		if (const FVector4* O = D.Own.Find(Key); O && O->W > Now)
		{
			OutDir = FVector(O->X, O->Y, 0.f);
			OutScale = O->Z;
			return true;
		}
		OutDir = GroupDir();
		OutScale = OutDir.IsNearlyZero() ? 0.f : D.Speed;
		if (Ant->Carrying == nullptr && D.Load.IsValid() && JustTumbled(Ant))
		{
			OutDir = Flat(D.Load->GetActorLocation() - Ant->GetActorLocation()).GetSafeNormal();   // back to the load
			OutScale = 1.f;
		}
		return true;
	}
	// An order (AntCarry::SetDesired) on a carrying bot
	if (Ant && Ant->Carrying)
	{
		const FVector V = GetDesired(Ant->Carrying, Ant, -1);
		const FAntCarryState* S = FAntCarrySolver::StC(Ant->Carrying);
		bool bExplicit = false;
		if (S) { for (const FAntCarryState::FMate& M : S->Mates) { if (!M.IsWorker() && M.Ant.Get() == Ant) { bExplicit = M.bExplicit; } } }
		if (bExplicit)
		{
			OutDir = V.GetSafeNormal();
			OutScale = FMath::Clamp((float)V.Size() / (480.f * AntKit::SpeedK), 0.f, 1.f);
			return true;
		}
	}
	return false;
}

bool AntCarry::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || !A[0].Equals(TEXT("carry"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	FDev& D = Dev();
	if (D.World.Get() != W)
	{
		D = FDev();
		D.World = W;
	}
	if (!W->GetTimerManager().IsTimerActive(D.Ticker))
	{
		W->GetTimerManager().SetTimer(D.Ticker, FTimerDelegate::CreateStatic(&DevTick), 0.016f, true);
	}
	const FString Op = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	AKitchenLoot* L = D.Load.Get();
	const float Now = W->GetTimeSeconds();

	if (Op == TEXT("spawn"))
	{
		// carry spawn bear|gummy|box|cheese|cake|crumb [x y z] [players] [weight]
		const FVector At = A.Num() >= 5 ? Vec(3) : (Me ? Me->GetActorLocation() + Me->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 30.f) : FVector::ZeroVector);
		if (L) { L->ReleaseAll(); L->Destroy(); }
		D.Load = SpawnTestLoad(W, A.IsValidIndex(2) ? A[2].ToLower() : FString(TEXT("bear")), At, (int32)Num(6, 1.f), (int32)Num(7, 0.f));
		D.Crew.Reset();
		D.Rivals.Reset();
		D.Own.Reset();
		D.bGo = false;
		return true;
	}
	if (Op == TEXT("log")) { D.bLog = Num(2, 1.f) > 0.5f; return true; }
	if (Op == TEXT("shot"))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("carry") / (A.IsValidIndex(2) ? A[2] : FString(TEXT("shot"))) + TEXT(".png"), true, false);
		return true;
	}
	if (Op == TEXT("ramp"))
	{
		// carry ramp x y z yaw length angle: a slope (a board leaning on something) to carry up
		const float Len = Num(6, 600.f), Ang = Num(7, 15.f);
		const FRotator Rot(Ang, Num(5), 0.f);
		const FVector Dir = Rot.Vector();
		Slab(W, Vec(2) + Dir * Len * 0.5f + FVector(0.f, 0.f, -4.f), Rot, FVector(Len, 260.f, 8.f), FLinearColor(0.55f, 0.4f, 0.25f));
		const FVector Base = Vec(2);
		const FVector TopAt = Base + Dir * Len;
		const float Hgt = FMath::Max(10.f, (float)(TopAt.Z - Base.Z));
		Slab(W, FVector(TopAt.X, TopAt.Y, Base.Z) + Flat(Dir).GetSafeNormal() * 130.f + FVector(0.f, 0.f, Hgt * 0.5f - 4.f), FRotator(0.f, Num(5), 0.f), FVector(260.f, 260.f, Hgt), FLinearColor(0.55f, 0.4f, 0.25f));
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev ramp from %s to %s"), *Vec(2).ToCompactString(), *TopAt.ToCompactString());
		return true;
	}
	if (Op == TEXT("corner"))
	{
		// carry corner x y z: an L of two walls 40 high: the corridor goes +X then turns to +Y at (x, y)
		const FVector C = Vec(2);
		const float Wd = Num(5, 170.f), Lg = 600.f, H = Num(6, 40.f);
		const FLinearColor Grey(0.3f, 0.3f, 0.35f);
		Slab(W, FVector(C.X - Lg + (Lg + Wd * 0.5f) * 0.5f, C.Y - Wd * 0.5f - 6.f, C.Z + H * 0.5f), FRotator::ZeroRotator, FVector(Lg + Wd * 0.5f + 12.f, 12.f, H), Grey);
		Slab(W, FVector(C.X - Lg + (Lg - Wd * 0.5f) * 0.5f, C.Y + Wd * 0.5f + 6.f, C.Z + H * 0.5f), FRotator::ZeroRotator, FVector(Lg - Wd * 0.5f, 12.f, H), Grey);
		Slab(W, FVector(C.X + Wd * 0.5f + 6.f, C.Y - Wd * 0.5f + (Lg + Wd * 0.5f) * 0.5f, C.Z + H * 0.5f), FRotator::ZeroRotator, FVector(12.f, Lg + Wd * 0.5f, H), Grey);
		Slab(W, FVector(C.X - Wd * 0.5f - 6.f, C.Y + Wd * 0.5f + (Lg - Wd * 0.5f) * 0.5f, C.Z + H * 0.5f), FRotator::ZeroRotator, FVector(12.f, Lg - Wd * 0.5f, H), Grey);
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev corner at %s, corridor %.0f wide"), *C.ToCompactString(), Wd);
		return true;
	}
	if (Op == TEXT("cam") && PC)
	{
		const AKitchenLoot* L0 = D.Load.Get();
		// carry cam side|front|back|top dist height [fov] | carry cam player | carry cam off
		const FString Mode = A.IsValidIndex(2) ? A[2].ToLower() : FString(TEXT("side"));
		if (Mode == TEXT("player") || Mode == TEXT("off"))
		{
			D.CamMode.Empty();
			PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f);
			return true;
		}
		const float Dist = FMath::Max(Num(3, 900.f), L0 ? L0->GetRadius() * 1.6f + 60.f : 0.f), Hgt = Num(4, 220.f);
		D.CamFov = Num(5, 40.f);
		FVector G = GroupDir();
		if (G.IsNearlyZero()) { G = FVector::ForwardVector; }
		const FVector R = FVector::CrossProduct(FVector::UpVector, G);
		D.CamOff = Mode == TEXT("front") ? G * Dist : Mode == TEXT("back") ? -G * Dist : Mode == TEXT("top") ? FVector(-G * 0.01f) : -R * Dist;
		D.CamOff.Z = Mode == TEXT("top") ? Dist : Hgt;
		D.CamMode = Mode;
		if (!D.Cam.IsValid()) { D.Cam = W->SpawnActor<ACameraActor>(); }
		if (ACameraActor* C = D.Cam.Get())
		{
			C->GetCameraComponent()->SetFieldOfView(D.CamFov);
			C->GetCameraComponent()->bConstrainAspectRatio = false;
			PC->SetViewTargetWithBlend(C, 0.f);
		}
		DevTick();
		return true;
	}
	if (!L)
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTCARRY dev: no test load (carry spawn first): %s"), *FString::Join(A, TEXT(" ")));
		return true;
	}
	if (Op == TEXT("crew"))
	{
		// carry crew N [me] [caste 0..3]: N carriers round the load (the player first, at its back, when "me")
		const int32 Want = FMath::Clamp((int32)Num(2, 2.f), 1, 24);
		const bool bMe = A.IsValidIndex(3) && A[3].ToLower() == TEXT("me");
		const int32 Caste = (int32)Num(bMe ? 4 : 3, 0.f);
		TArray<AAntCharacter*> Pool;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			if (!It->bDead && It->Creature == 0 && It->Team == 0 && !It->IsPlayerControlled()) { Pool.Add(*It); }
		}
		const int32 NeedBots = Want - (bMe ? 1 : 0);
		if (Pool.Num() < NeedBots)
		{
			for (int32 Left = NeedBots - Pool.Num(); Left > 0; Left -= 12) { GM->AddBots(0, FMath::Min(12, Left)); }
			Pool.Reset();
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				if (!It->bDead && It->Creature == 0 && It->Team == 0 && !It->IsPlayerControlled()) { Pool.Add(*It); }
			}
		}
		for (const TWeakObjectPtr<AAntCharacter>& C : D.Crew) { if (C.IsValid() && C->Carrying == L) { C->LetGo(); } }
		D.Crew.Reset();
		D.Own.Reset();
		FVector Back = -GroupDir();
		if (Back.IsNearlyZero()) { Back = FVector(-1.f, 0.f, 0.f); }
		const float Base = FMath::Atan2((float)Back.Y, (float)Back.X);
		UBoxComponent* B = FAntCarrySolver::Body(L);
		const FVector C = L->GetActorLocation();
		const float Bottom = (float)C.Z - (B ? (float)B->GetScaledBoxExtent().Z : 10.f);
		for (int32 k = 0; k < Want; ++k)
		{
			AAntCharacter* Ant = (bMe && k == 0) ? Me : (Pool.IsValidIndex(k - (bMe ? 1 : 0)) ? Pool[k - (bMe ? 1 : 0)] : nullptr);
			if (!Ant) { continue; }
			Ant->SetClass((EAntClass)FMath::Clamp(Caste, 0, 3), true);
			Ant->LetGo();
			const float Ang = Base + 2.f * PI * k / Want;
			const FVector Far = C + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (L->GetRadius() + 80.f);
			FVector Edge = C;
			if (B) { B->GetClosestPointOnCollision(FVector(Far.X, Far.Y, C.Z), Edge); }
			const FVector Out = Flat(Far - C).GetSafeNormal();
			Ant->ApplyTeleport(FVector(Edge.X, Edge.Y, Bottom + 8.f) + Out * 7.f);
			D.Crew.Add(Ant);
		}
		// take hold once everybody stands where it was put (next frame: the teleport of a client ant is not instant)
		TArray<TWeakObjectPtr<AAntCharacter>> Grab = D.Crew;
		TWeakObjectPtr<AKitchenLoot> WL = L;
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Grab, WL]()
		{
			int32 Got = 0;
			for (const TWeakObjectPtr<AAntCharacter>& G : Grab) { if (G.IsValid() && WL.IsValid() && G->GrabLoot(WL.Get())) { ++Got; } }
			UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev crew: %d of %d took hold of «%s»"), Got, Grab.Num(), WL.IsValid() ? *WL->LootName : TEXT("-"));
		}), 0.3f, false);
		return true;
	}
	if (Op == TEXT("go"))
	{
		// carry go x y [speed 0..1]: everybody (crew, player, workers) heads the same way: towards (x, y)
		D.Goal = FVector(Num(2), Num(3), L->GetActorLocation().Z);
		D.Speed = Num(4, 1.f);
		D.bGo = true;
		D.Own.Reset();
		if (D.Army.IsValid()) { SetArmyGoal(L, D.Army.Get(), D.Goal, D.Speed); }
		return true;
	}
	if (Op == TEXT("stop"))
	{
		D.bGo = false;
		if (D.Army.IsValid()) { SetArmyGoal(L, D.Army.Get(), L->GetActorLocation(), 0.f); }
		return true;
	}
	if (Op == TEXT("desync"))
	{
		// carry desync [secs]: each bot its own way: half veer left, half right, every third stops, speeds differ
		const FVector G = GroupDir().IsNearlyZero() ? FVector::ForwardVector : GroupDir();
		const float Until = Now + Num(2, 4.f);
		for (int32 k = 0; k < D.Crew.Num(); ++k)
		{
			const float Ang = (k % 2 == 0 ? 70.f : -70.f) + 15.f * (k % 3);
			const FVector Dir = G.RotateAngleAxis(Ang, FVector::UpVector);
			D.Own.Add(D.Crew[k], FVector4(Dir.X, Dir.Y, k % 3 == 2 ? 0.f : (k % 2 ? 0.55f : 1.f), Until));
		}
		return true;
	}
	if (Op == TEXT("yank"))
	{
		// carry yank i dx dy [secs] [scale]: one carrier jerks that way (0 = the player when he is in the crew)
		const int32 i = (int32)Num(2, 1.f);
		if (D.Crew.IsValidIndex(i))
		{
			const FVector Dir = FVector(Num(3), Num(4), 0.f).GetSafeNormal();
			D.Own.Add(D.Crew[i], FVector4(Dir.X, Dir.Y, Num(6, 1.f), Now + Num(5, 0.6f)));
		}
		return true;
	}
	if (Op == TEXT("kill"))
	{
		const int32 i = (int32)Num(2, 1.f);
		if (D.Crew.IsValidIndex(i) && D.Crew[i].IsValid()) { D.Crew[i]->HurtBy(100000.f, TEXT("погиб на испытаниях")); }
		return true;
	}
	if (Op == TEXT("human"))
	{
		// carry human: the Human steps up and snatches the load out of the carriers' jaws
		for (TActorIterator<AHumanCharacter> It(W); It; ++It)
		{
			AHumanCharacter* H = *It;
			if (H->Fig == HumanFig::Dad)
			{
				H->bDevPuppet = true;
				H->DevWalk = FVector::ZeroVector;
				const FVector Lp = L->GetActorLocation();
				const FVector Side = FVector(0.f, -1.f, 0.f);
				const FVector At = Lp - Side * 900.f;
				H->SetActorLocationAndRotation(FVector(At.X - 250.f, At.Y, H->GetActorLocation().Z), Side.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
				H->DevAct(TEXT("grab"), Lp);
				UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev: the Human reaches for «%s»"), *L->LootName);
				break;
			}
		}
		return true;
	}
	if (Op == TEXT("rivals"))
	{
		// carry rivals N: N red ants take hold on the front side and pull the other way (a tug of war)
		const int32 Want = FMath::Clamp((int32)Num(2, 2.f), 1, 12);
		TArray<AAntCharacter*> Pool;
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && It->Creature == 0 && It->Team == 1 && !It->IsPlayerControlled()) { Pool.Add(*It); } }
		if (Pool.Num() < Want) { GM->AddBots(1, Want - Pool.Num()); Pool.Reset(); for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && It->Creature == 0 && It->Team == 1 && !It->IsPlayerControlled()) { Pool.Add(*It); } } }
		FVector G = GroupDir();
		if (G.IsNearlyZero()) { G = FVector::ForwardVector; }
		UBoxComponent* B = FAntCarrySolver::Body(L);
		const FVector C = L->GetActorLocation();
		D.Rivals.Reset();
		for (int32 k = 0; k < Want && k < Pool.Num(); ++k)
		{
			const float Ang = FMath::Atan2((float)G.Y, (float)G.X) + (k - (Want - 1) * 0.5f) * 0.5f;
			const FVector Far = C + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * (L->GetRadius() + 80.f);
			FVector Edge = C;
			if (B) { B->GetClosestPointOnCollision(FVector(Far.X, Far.Y, C.Z), Edge); }
			Pool[k]->ApplyTeleport(FVector(Edge.X, Edge.Y, C.Z - (B ? B->GetScaledBoxExtent().Z : 10.f) + 8.f) + Flat(Far - C).GetSafeNormal() * 7.f);
			D.Rivals.Add(Pool[k]);
		}
		TArray<TWeakObjectPtr<AAntCharacter>> Grab = D.Rivals;
		TWeakObjectPtr<AKitchenLoot> WL = L;
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Grab, WL]()
		{
			for (const TWeakObjectPtr<AAntCharacter>& R : Grab) { if (R.IsValid() && WL.IsValid()) { R->GrabLoot(WL.Get()); } }
		}), 0.3f, false);
		return true;
	}
	if (Op == TEXT("army") && Me)
	{
		// carry army N: a squad of N workers for the player (strength 4 each), standing by him
		AAntArmy* Ar = AAntArmy::FindFor(Me);
		if (!Ar)
		{
			const FVector At = Me->GetActorLocation() - Me->GetActorForwardVector() * 40.f;
			Ar = W->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Ar)
			{
				Ar->bSquad = true;
				Ar->Team = 0;
				Ar->Count = FMath::Clamp((int32)Num(2, 12.f), 1, 100);
				Ar->Taken = Ar->Count;
				Ar->Commander = Me;
				Ar->SquadOwner = PC;
				Ar->OwnerPS = Me->GetPlayerState();
				Ar->Mode = EArmyMode::Follow;
				Ar->FinishSpawning(FTransform(At));
			}
		}
		D.Army = Ar;
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev army: %s with %d workers"), Ar ? *Ar->GetName() : TEXT("none"), Ar ? Ar->Count : 0);
		return true;
	}
	if (Op == TEXT("workers"))
	{
		// carry workers N [follow]: N workers of the dev squad take hold (follow: they go where the player goes; else "carry go")
		AAntArmy* Ar = D.Army.Get();
		const int32 Got = Ar ? AddArmy(L, Ar, (int32)Num(2, 6.f)) : 0;
		if (Ar && A.IsValidIndex(3) && A[3].ToLower() == TEXT("follow") && Me)
		{
			FAntCarryState& S = FAntCarrySolver::St(L);
			for (FAntCarryState::FMate& M : S.Mates) { if (M.IsWorker() && M.Army.Get() == Ar) { M.Leader = Me; M.bExplicit = false; } }
		}
		else if (Ar && D.bGo)
		{
			SetArmyGoal(L, Ar, D.Goal, D.Speed);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY dev workers: %d on «%s»"), Got, *L->LootName);
		return true;
	}
	if (Op == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCARRY state t=%.1f %s"), Now, *Describe(L));
		return true;
	}
	if (Op == TEXT("drop"))
	{
		L->ReleaseAll();
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("ANTCARRY dev unknown: %s"), *FString::Join(A, TEXT(" ")));
	return true;
}
