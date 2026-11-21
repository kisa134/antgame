#include "AntWeapon.h"
#include "AntWreck.h"
#include "AntVersus.h"
#include "AntTypes.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "HumanBody.h"
#include "KitchenArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	using namespace AntWeapons;

	// name, mesh, length, kg, wind-up, every, radius, reach, ant damage, impulse, world damage, tier
	const FDef GPaper = { TEXT("газета"), nullptr, 1330.f, 0.2f, HumanAct::SwatHit, 2.6f, 300.f, 0.f, 150.f, 5.f, 4.f, 0 };
	const FDef GDefs[] =
	{
		{ TEXT("ничего"),        nullptr,              0.f,    0.f, 0.42f, 2.6f, 300.f,    0.f, 150.f,   5.f,   4.f, 0 },
		{ TEXT("тапок"),         TEXT("SM_K_Slipper"),  550.f, 0.3f, 0.42f, 2.2f, 280.f, -700.f, 150.f,   8.f,  12.f, 1 },
		{ TEXT("половник"),      TEXT("SM_K_Ladle"),    600.f, 0.3f, 0.45f, 2.4f, 240.f, -600.f, 150.f,  10.f,  18.f, 1 },
		{ TEXT("лопатка"),       TEXT("SM_K_Spatula"),  550.f, 0.1f, 0.42f, 2.0f, 220.f, -700.f,  60.f,   4.f,   5.f, 0 },
		{ TEXT("книга"),         TEXT("SM_L_Book_C"),   500.f, 0.8f, 0.50f, 2.8f, 380.f, -750.f, 150.f,  14.f,  25.f, 1 },
		{ TEXT("скалка"),        TEXT("SM_K_RollingPin"),800.f, 1.2f, 0.55f, 3.0f, 320.f, -450.f, 150.f,  25.f,  60.f, 2 },
		{ TEXT("сковородка"),    nullptr,              900.f, 1.5f, 0.60f, 3.2f, 420.f, -350.f, 150.f,  30.f,  80.f, 2 },
		{ TEXT("веник"),         TEXT("SM_V31_Broom"), 2300.f, 1.0f, 0.60f, 3.0f, 650.f,  900.f, 120.f,  20.f,  30.f, 2 },
		{ TEXT("бейсбольная бита"), nullptr,           1600.f, 1.0f, 0.75f, 3.4f, 450.f,  250.f, 200.f,  60.f, 160.f, 3 },
		{ TEXT("сосиска"),       nullptr,              500.f, 0.1f, 0.42f, 2.0f, 250.f, -800.f,   0.f,   1.f,   0.f, 0 },
	};

	struct FAiState
	{
		TWeakObjectPtr<AHumanWeapon> GoingFor;
		float GiveUpAt = 0.f;
		float NextThink = 0.f;
		float NextRampage = 0.f;
		int32 RampageInst = -1;
		int32 LastRampage = -1;
	};
	TMap<TWeakObjectPtr<AHumanCharacter>, FAiState> GAi;

	AHumanWeapon* HeldBy(const AHumanCharacter* H)
	{
		if (!H) { return nullptr; }
		for (TActorIterator<AHumanWeapon> It(H->GetWorld()); It; ++It)
		{
			if (It->Holder == H && !It->IsActorBeingDestroyed()) { return *It; }
		}
		return nullptr;
	}

	FVector Shoulder(const AHumanCharacter* H) { return H->GetActorLocation() + FVector(0.f, 0.f, 960.f); }

	float FloorAt(UWorld* W, const FVector& P, const AActor* Ignore)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WeaponFloor), true, Ignore);
		for (TActorIterator<APawn> It(W); It; ++It) { Q.AddIgnoredActor(*It); }
		FHitResult Hit;
		return W->LineTraceSingleByChannel(Hit, P, P - FVector(0.f, 0.f, 9000.f), ECC_Visibility, Q) ? (float)Hit.ImpactPoint.Z : (float)P.Z - 2000.f;
	}

	AAntKitchenGameMode* GMOf(UWorld* W) { return W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr; }
}

const AntWeapons::FDef& AntWeapons::Def(uint8 Kind) { return GDefs[FMath::Min<int32>(Kind, UE_ARRAY_COUNT(GDefs) - 1)]; }
const AntWeapons::FDef& AntWeapons::Paper() { return GPaper; }

const AntWeapons::FDef* AntWeapons::Held(const AHumanCharacter* H)
{
	const AHumanWeapon* W = HeldBy(H);
	return W ? &W->D() : nullptr;
}

float AntWeapons::SwingWindup(const AHumanCharacter* H, float PaperWindup)
{
	const AHumanWeapon* W = HeldBy(H);
	return W ? FMath::Max(0.4f, W->D().Windup) : PaperWindup;
}

void AntWeapons::OnSwingLanded(AHumanCharacter* H, const FVector& Point)
{
	if (!H || !H->HasAuthority())
	{
		return;
	}
	const AHumanWeapon* W = HeldBy(H);
	const FDef& D = W ? W->D() : Paper();
	const float R = 1.f + 0.15f * H->Rage;   // angrier: wider and harder
	FVector Dir = (Point - Shoulder(H)).GetSafeNormal();
	Dir = (Dir + FVector(0.f, 0.f, -0.6f)).GetSafeNormal();
	AntWreck::Hit(H->GetWorld(), Point, D.Radius * R, Dir * D.Impulse * (1.f + 0.2f * H->Rage), D.WorldDamage * (1.f + 0.25f * H->Rage), H, D.Name);
	if (W && D.Tier >= 2)
	{
		AntSfx::Everyone(H, AntSfx::Stomp, Point, FMath::FRandRange(0.6f, 0.75f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWEAPON swing %s landed at %s (rage %d, radius %.0f)"), D.Name, *Point.ToCompactString(), (int32)H->Rage, D.Radius * R);
}

void AntWeapons::Drop(AHumanCharacter* H, const TCHAR* Why)
{
	AHumanWeapon* W = HeldBy(H);
	if (!W || !W->HasAuthority())
	{
		return;
	}
	const FVector At = H->GetPalmLocation(1);
	W->LayDown(At + H->GetActorForwardVector() * 300.f, (float)H->GetActorRotation().Yaw + 70.f);
	UE_LOG(LogTemp, Display, TEXT("ANTWEAPON %s dropped %s (%s)"), *H->GetDisplayName(), W->D().Name, Why ? Why : TEXT("-"));
	if (AAntKitchenGameMode* GM = GMOf(H->GetWorld()))
	{
		GM->Broadcast(FString::Printf(TEXT("Человек выронил: %s (%s)"), W->D().Name, Why ? Why : TEXT("")), FLinearColor(1.f, 0.85f, 0.3f));
	}
}

bool AntWeapons::TryPickOrDrop(AHumanCharacter* H, const FVector& Point)
{
	if (!H || !H->HasAuthority())
	{
		return false;
	}
	AHumanWeapon* Best = nullptr;
	float BestD = 900.f;
	for (TActorIterator<AHumanWeapon> It(H->GetWorld()); It; ++It)
	{
		AHumanWeapon* W = *It;
		if (W->Holder || W->bStashed || FVector::Dist2D(W->GetActorLocation(), H->GetActorLocation()) > 1900.f)
		{
			continue;
		}
		const float D = W->DistanceTo(Point);
		if (D < BestD) { BestD = D; Best = W; }
	}
	if (Best)
	{
		Drop(H, TEXT("взял другое"));
		Best->PickUp(H);
		return true;
	}
	return false;
}

bool AntWeapons::AiTick(AHumanCharacter* H, AAntCharacter* Target, float Dt, FVector& Goal, FVector& Face)
{
	if (!H || !H->HasAuthority() || H->bDevPuppet || H->IsBodyBusy() || !AntWreck::Enabled())
	{
		return false;
	}
	UWorld* W = H->GetWorld();
	const float Now = W->GetTimeSeconds();
	FAiState& S = GAi.FindOrAdd(H);
	AHumanWeapon* Mine = HeldBy(H);
	const uint8 MyTier = Mine ? Mine->D().Tier : 0;
	const FVector Me = H->GetActorLocation();
	const float TargetDist = Target ? (float)FVector::Dist2D(Target->GetActorLocation(), Me) : 1.e9f;
	const bool bHot = Target && Target->IsCarrying() && TargetDist < 2600.f;
	// Rage 2: something heavier than the newspaper; rage 3: the bat.
	const uint8 Want = H->Rage >= 3 ? 3 : H->Rage >= 2 ? 2 : 0;
	if (Now >= S.NextThink && !bHot)
	{
		S.NextThink = Now + 1.f;
		if (Want > MyTier && !S.GoingFor.IsValid())
		{
			AHumanWeapon* Best = nullptr;
			float BestScore = -1.e9f;
			for (TActorIterator<AHumanWeapon> It(W); It; ++It)
			{
				AHumanWeapon* Wp = *It;
				if (Wp->Holder || Wp->bStashed || Wp->D().Tier <= MyTier || Wp->D().Tier > Want)
				{
					continue;
				}
				const float D = (float)FVector::Dist2D(Wp->GetActorLocation(), Me);
				if (D > 12000.f) { continue; }
				const float Score = Wp->D().Tier * 6000.f - D;
				if (Score > BestScore) { BestScore = Score; Best = Wp; }
			}
			if (Best)
			{
				S.GoingFor = Best;
				S.GiveUpAt = Now + 25.f;
				UE_LOG(LogTemp, Display, TEXT("ANTWEAPON AI rage %d goes for the %s at %s"), (int32)H->Rage, Best->D().Name, *Best->GetActorLocation().ToCompactString());
				if (AAntKitchenGameMode* GM = GMOf(W)) { GM->Broadcast(FString::Printf(TEXT("Человек идёт за: %s!"), Best->D().Name), FLinearColor(1.f, 0.35f, 0.2f)); }
			}
		}
	}
	if (AHumanWeapon* G = S.GoingFor.Get())
	{
		if (bHot || G->Holder || G->bStashed || Now > S.GiveUpAt)
		{
			S.GoingFor = nullptr;
		}
		else
		{
			const float D = (float)FVector::Dist2D(G->GetActorLocation(), Me);
			Face = G->GetActorLocation();
			if (D < 1500.f)
			{
				Drop(H, TEXT("взял другое"));
				G->PickUp(H);
				S.GoingFor = nullptr;
				Goal = FVector::ZeroVector;
				return true;
			}
			Goal = G->GetActorLocation();
			return true;
		}
	}
	if (!Mine || Mine->D().Tier == 0)
	{
		return false;
	}
	const FDef& D = Mine->D();
	const float Reach = 2500.f + D.Reach;
	// An ant in reach of the weapon: swing it (on the floor too: the bat comes down where the slipper would).
	if (Target && !Target->bDead && TargetDist < Reach + 400.f)
	{
		const FVector TL = Target->GetActorLocation();
		Face = TL;
		const float Yaw = FMath::Abs(FMath::FindDeltaAngleDegrees((float)H->GetActorRotation().Yaw, (float)(TL - Me).Rotation().Yaw));
		if (Now >= H->SwatReadyTime && Yaw < 35.f && FVector::Dist(TL, Shoulder(H)) < Reach)
		{
			H->TrySwat(TL);
		}
		Goal = TargetDist > Reach * 0.7f ? TL : FVector::ZeroVector;
		return true;
	}
	// Rage 3, nobody in reach: he takes it out on the furniture.
	if (H->Rage >= 3 && D.Tier >= 2 && TargetDist > 4000.f)
	{
		if (Now >= S.NextRampage || S.RampageInst < 0)
		{
			S.NextRampage = Now + FMath::FRandRange(3.f, 4.5f);
			S.RampageInst = -1;
			float Best = 1.e9f;
			const int32 Last = S.LastRampage;
			for (const AntWreck::FPiece& P : AntWreck::Pieces(W))
			{
				const float Dd = (float)FVector::Dist2D(P.Box.GetCenter(), Me);
				if (Dd > 4500.f || Dd < 600.f || P.Box.Max.Z > Me.Z + 1200.f || P.Inst == Last || !AntWreck::Class(P.Mat).bBreaks) { continue; }
				bool bBroken = false;
				for (TActorIterator<AAntWreckBody> It(W); It; ++It) { if (It->Inst == P.Inst && It->IsBroken()) { bBroken = true; break; } }
				if (bBroken) { continue; }
				const float Score = Dd + FMath::FRand() * 2500.f;
				if (Score < Best) { Best = Score; S.RampageInst = P.Inst; }
			}
			S.LastRampage = S.RampageInst;
		}
		const AntWreck::FPiece* P = AntWreck::PieceOf(W, S.RampageInst);
		if (!P)
		{
			return false;
		}
		FVector Aim = P->Box.GetCenter();
		for (TActorIterator<AAntWreckBody> It(W); It; ++It)
		{
			if (It->Inst == P->Inst) { if (It->IsBroken()) { S.RampageInst = -1; return false; } Aim = It->WorldBox().GetCenter(); }
		}
		Aim.Z = FMath::Min((float)Aim.Z + 100.f, (float)P->Box.Max.Z);
		Face = Aim;
		const float Yaw = FMath::Abs(FMath::FindDeltaAngleDegrees((float)H->GetActorRotation().Yaw, (float)(Aim - Me).Rotation().Yaw));
		if (FVector::Dist(Aim, Shoulder(H)) < Reach * 0.9f)
		{
			Goal = FVector::ZeroVector;
			if (Now >= H->SwatReadyTime && Yaw < 30.f)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTWEAPON rampage: %s at piece %d"), D.Name, P->Inst);
				H->TrySwat(Aim);
			}
		}
		else
		{
			Goal = Aim;
		}
		return true;
	}
	return false;
}

void AntWeapons::SpawnAll(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || !AntWreck::Enabled())
	{
		return;
	}
	for (TActorIterator<AHumanWeapon> It(World); It; ++It) { It->Destroy(); }
	GAi.Reset();
	// bFloor: on the floor beside the piece (the first free spot round it); else on top of what is there (the worktop).
	struct FPlace { uint8 Kind; const TCHAR* Near; float Dx, Dy; bool bLean; bool bFloor; };
	const FPlace Places[] =
	{
		{ Bat,        TEXT("SM_V3_ToyBox"),    0.f, 0.f, true, true },    // the kid's bat, leaning by the toy box
		{ RollingPin, TEXT("SM_K_RollingPin"), 0.f, -260.f, false, false },
		{ Pan,        TEXT("SM_K_Range"),      350.f, -400.f, false, false },
		{ Ladle,      TEXT("SM_K_Ladle"),      0.f, -300.f, false, false },
		{ Spatula,    TEXT("SM_K_Spatula"),    0.f, -250.f, false, false },
		{ Book,       TEXT("SM_L_Bookshelf"),  0.f, 0.f, false, true },
		{ Broom,      TEXT("SM_K_Mop"),        0.f, 0.f, true, true },
		{ Slipper,    TEXT("SM_K_Shoe"),       0.f, 0.f, false, true },
	};
	int32 N = 0;
	for (const FPlace& Pl : Places)
	{
		const int32 Inst = AntWreck::FindPiece(World, Pl.Near);
		const AntWreck::FPiece* P = AntWreck::PieceOf(World, Inst);
		const FKitchenArtInst* Row = KitchenArt::Instances().IsValidIndex(Inst) ? &KitchenArt::Instances()[Inst] : nullptr;
		FVector At = P ? P->Box.GetCenter() : Row ? FVector(Row->Loc) : FVector(0.f, 0.f, 200.f);
		const float TopZ = P ? (float)P->Box.Max.Z + 200.f : (float)At.Z + 400.f;
		At += FVector(Pl.Dx, Pl.Dy, 0.f);
		At.Z = TopZ;
		if (Pl.bFloor)
		{
			// Round the piece: the first direction where the floor is free (a trace down meets the floor, not furniture).
			const FBox B = P ? P->Box : FBox(At - FVector(200.f), At + FVector(200.f));
			const float Floor0 = (float)B.Min.Z;
			const float Out = (float)B.GetExtent().Size2D() + 260.f;
			for (int32 k = 0; k < 16; ++k)
			{
				const float Ang = k * (PI / 8.f) + 0.3f;
				const FVector Try = B.GetCenter() + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Out;
				const float F = FloorAt(World, FVector(Try.X, Try.Y, Floor0 + 900.f), nullptr);
				if (FMath::Abs(F - Floor0) < 60.f)
				{
					At = FVector(Try.X, Try.Y, Floor0 + 120.f);
					break;
				}
			}
		}
		AHumanWeapon* W = World->SpawnActorDeferred<AHumanWeapon>(AHumanWeapon::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!W) { continue; }
		W->Kind = Pl.Kind;
		W->FinishSpawning(FTransform(At));
		W->LayDown(At, Pl.Kind * 37.f, Pl.bLean);
		++N;
		UE_LOG(LogTemp, Display, TEXT("ANTWEAPON %s lies at %s"), W->D().Name, *W->GetActorLocation().ToCompactString());
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWEAPON spawned %d weapons"), N);
	if (AntVersus::Wanted() && !AntVersus::Active(World))
	{
		// The host asked for versus (menu or -AntVersus): the first client is the Human, the host if nobody has joined.
		TArray<FString> Args;
		Args.Add(TEXT("versus")); Args.Add(TEXT("start")); Args.Add(TEXT("client"));
		AntVersus::DevCommand(GMOf(World), Args);
	}
}

bool AntWeapons::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0)
	{
		return false;
	}
	const FString Op = A[0].ToLower();
	UWorld* W = GM->GetWorld();
	AHumanCharacter* H = nullptr;
	for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (It->GetClass() == AHumanCharacter::StaticClass()) { H = *It; break; } }
	auto KindOf = [](const FString& S) -> int32
	{
		for (int32 k = 0; k < Count; ++k)
		{
			const FString N = FString(TEXT("none,slipper,ladle,spatula,book,pin,pan,broom,bat,sausage")).Replace(TEXT(","), TEXT(" "));
			TArray<FString> Ns; N.ParseIntoArrayWS(Ns);
			if (Ns.IsValidIndex(k) && Ns[k].Equals(S, ESearchCase::IgnoreCase)) { return k; }
		}
		return S.IsNumeric() ? FCString::Atoi(*S) : -1;
	};
	if (Op == TEXT("rage") && H && A.Num() >= 2)
	{
		H->SetRage((uint8)FMath::Clamp(FCString::Atoi(*A[1]), 0, 3));
		UE_LOG(LogTemp, Display, TEXT("ANTWEAPON dev rage %d"), (int32)H->Rage);
		return true;
	}
	if (Op != TEXT("weapon"))
	{
		return false;
	}
	const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	if (Sub == TEXT("spawn"))
	{
		SpawnAll(W);
	}
	else if (Sub == TEXT("give") && H && A.IsValidIndex(2))
	{
		// weapon give <kind>: the lying one of that kind into his hand (one is made if there is none)
		const int32 K = KindOf(A[2]);
		AHumanWeapon* Found = nullptr;
		for (TActorIterator<AHumanWeapon> It(W); It; ++It) { if (It->Kind == K && !It->Holder) { Found = *It; break; } }
		if (!Found && K > 0)
		{
			Found = W->SpawnActorDeferred<AHumanWeapon>(AHumanWeapon::StaticClass(), H->GetActorTransform(), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Found) { Found->Kind = (uint8)K; Found->FinishSpawning(H->GetActorTransform()); }
		}
		Drop(H, TEXT("dev"));
		if (Found) { Found->PickUp(H); }
		if (K == 0) { Drop(H, TEXT("dev")); }
	}
	else if (Sub == TEXT("drop") && H)
	{
		Drop(H, TEXT("dev"));
	}
	else if (Sub == TEXT("swap"))
	{
		// weapon swap <kind>: as if the ants had finished their work on it
		const int32 K = A.IsValidIndex(2) ? KindOf(A[2]) : (int32)Bat;
		for (TActorIterator<AHumanWeapon> It(W); It; ++It) { if (It->Kind == K && !It->Holder && !It->bDone) { It->ForceDone(nullptr); break; } }
	}
	else if (Sub == TEXT("at") && H && A.Num() >= 5)
	{
		// weapon at x y z: swing at that point
		H->TrySwat(FVector(FCString::Atof(*A[2]), FCString::Atof(*A[3]), FCString::Atof(*A[4])));
	}
	else
	{
		for (TActorIterator<AHumanWeapon> It(W); It; ++It)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTWEAPON %s at %s holder %s hidden %d progress %.2f"), It->D().Name, *It->GetActorLocation().ToCompactString(), It->Holder ? *It->Holder->GetDisplayName() : TEXT("-"), It->bStashed ? 1 : 0, It->Progress);
		}
	}
	return true;
}

// =============================================================================================
// The weapon actor: lies about (ants can swap or hide it) or rides in the Human's right fist
// =============================================================================================

AHumanWeapon::AHumanWeapon()
{
	Need = 28.f;      // two foragers
	Seconds = 5.f;
	InteractRadius = 220.f;
	bAlwaysRelevant = true;
}

void AHumanWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHumanWeapon, Kind);
	DOREPLIFETIME(AHumanWeapon, Holder);
	DOREPLIFETIME(AHumanWeapon, LiePose);
	DOREPLIFETIME(AHumanWeapon, bStashed);
}

void AHumanWeapon::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Attach();
}

void AHumanWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Holder && GetAttachParentActor() != Holder)
	{
		Attach();   // the hand segment came up late on this machine
	}
}

void AHumanWeapon::OnRep_Kind() { Build(); }
void AHumanWeapon::OnRep_Holder() { Attach(); }

void AHumanWeapon::Build()
{
	if (BuiltKind == Kind)
	{
		return;
	}
	BuiltKind = Kind;
	for (UStaticMeshComponent* C : Parts) { if (C) { C->DestroyComponent(); } }
	Parts.Reset();
	const FDef& Df = D();
	auto Shape = [&](EAntShape S, float X0, float X1, float Dia, const FLinearColor& Col)
	{
		const bool bBall = S == EAntShape::Sphere;
		UStaticMeshComponent* C = AntKit::AddPart(this, Root, S, FVector((X0 + X1) * 0.5f, 0.f, 0.f), bBall ? FVector((X1 - X0) / 100.f, Dia / 100.f, Dia / 100.f) : FVector(Dia / 100.f, Dia / 100.f, (X1 - X0) / 100.f),
			Col, bBall ? FRotator::ZeroRotator : FRotator(90.f, 0.f, 0.f), false, false);
		C->SetCastShadow(true);
		Parts.Add(C);
		return C;
	};
	if (Kind == Bat)
	{
		const FLinearColor Ash(0.78f, 0.58f, 0.34f), Tape(0.05f, 0.05f, 0.06f);
		Shape(EAntShape::Sphere, -110.f, -20.f, 85.f, Ash);           // the knob
		Shape(EAntShape::Cylinder, -40.f, 420.f, 52.f, Tape);         // taped grip
		Shape(EAntShape::Cylinder, 400.f, 800.f, 62.f, Ash);
		Shape(EAntShape::Cylinder, 780.f, 1050.f, 92.f, Ash);         // the taper
		Shape(EAntShape::Cylinder, 1030.f, 1540.f, 125.f, Ash);       // the barrel
		Shape(EAntShape::Sphere, 1490.f, 1590.f, 125.f, Ash);
	}
	else if (Kind == Pan)
	{
		Shape(EAntShape::Cylinder, -60.f, 420.f, 48.f, FLinearColor(0.04f, 0.04f, 0.04f));
		UStaticMeshComponent* Disc = AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(720.f, 0.f, 0.f), FVector(6.2f, 6.2f, 0.55f), FLinearColor(0.12f, 0.12f, 0.13f), FRotator::ZeroRotator, false, false);
		Disc->SetCastShadow(true);
		Parts.Add(Disc);
	}
	else if (Kind == Sausage)
	{
		const FLinearColor Skin(0.62f, 0.27f, 0.2f);
		Shape(EAntShape::Cylinder, 0.f, 420.f, 115.f, Skin);
		Shape(EAntShape::Sphere, -55.f, 55.f, 115.f, Skin);
		Shape(EAntShape::Sphere, 365.f, 475.f, 115.f, Skin);
	}
	else if (Df.Mesh)
	{
		UStaticMesh* M = KitchenArt::Mesh(Df.Mesh);
		if (M)
		{
			// The long side of the model along X, the grip end at the origin.
			const FBox B = M->GetBoundingBox();
			const FVector S = B.GetSize();
			const FRotator R = (S.X >= S.Y && S.X >= S.Z) ? FRotator::ZeroRotator : (S.Y >= S.Z) ? FRotator(0.f, -90.f, 0.f) : FRotator(-90.f, 0.f, 0.f);
			const FTransform T0(R, FVector::ZeroVector, FVector(KitchenArt::ImportScale));
			const FBox RB = B.TransformBy(T0);
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(M);
			C->SetupAttachment(Root);
			C->SetRelativeTransform(FTransform(R, FVector(-RB.Min.X - 0.12f * (float)RB.GetSize().X, -RB.GetCenter().Y, -RB.GetCenter().Z), FVector(KitchenArt::ImportScale)));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			KitchenArt::ApplyLook(C, Df.Mesh, 0);
			C->RegisterComponent();
			Parts.Add(C);
		}
	}
}

void AHumanWeapon::Attach()
{
	if (Holder)
	{
		USceneComponent* Hand = Holder->GetSegment(HumanSeg::HandR);
		if (!Hand)
		{
			return;
		}
		const float K = 1.f / HumanBody::Scale;
		AttachToComponent(Hand, FAttachmentTransformRules::KeepRelativeTransform);
		SetActorRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(10.f, -35.f, -150.f) * K, FVector(K)));
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorTransform(LiePose);
	}
}

FVector AHumanWeapon::Tip() const
{
	return GetActorLocation() + GetActorForwardVector() * D().Length;
}

float AHumanWeapon::DistanceTo(const FVector& P) const
{
	return FMath::PointDistToSegment(P, GetActorLocation(), Tip());
}

FString AHumanWeapon::DoorNoun() const
{
	return D().Name;
}

bool AHumanWeapon::CanInteract(const AAntCharacter* Ant) const
{
	return !Holder && !bDone && !bStashed && Kind != Sausage && Kind != None;
}

FString AHumanWeapon::GetPrompt(const AAntCharacter* Ant) const
{
	if (!CanInteract(Ant))
	{
		return FString();
	}
	const FString Pct = Progress > 0.f ? FString::Printf(TEXT(" · %d%%"), FMath::RoundToInt(Progress * 100.f)) : FString();
	return D().Tier >= 2
		? FString::Printf(TEXT("Подменить: %s → сосиска — держать [E] вместе%s"), D().Name, *Pct)
		: FString::Printf(TEXT("Утащить и спрятать: %s — держать [E] вместе%s"), D().Name, *Pct);
}

void AHumanWeapon::PickUp(AHumanCharacter* H)
{
	if (!HasAuthority() || !H)
	{
		return;
	}
	Holder = H;
	bStashed = false;
	Progress = 0.f;
	Attach();
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("ANTWEAPON %s picked up the %s"), *H->GetDisplayName(), D().Name);
}

void AHumanWeapon::LayDown(const FVector& At, float Yaw, bool bLean)
{
	if (!HasAuthority())
	{
		return;
	}
	Holder = nullptr;
	const float Floor = FloorAt(GetWorld(), At + FVector(0.f, 0.f, 50.f), this);
	// Leaning (the bat against the wall): the grip end up, the barrel on the floor.
	LiePose = bLean
		? FTransform(FRotator(-70.f, Yaw, 0.f), FVector(At.X, At.Y, Floor + D().Length * 0.94f), FVector::OneVector)
		: FTransform(FRotator(0.f, Yaw, 90.f), FVector(At.X, At.Y, Floor + 45.f), FVector::OneVector);
	Attach();
	ForceNetUpdate();
}

void AHumanWeapon::OnDone()
{
	AAntKitchenGameMode* GM = GMOf(GetWorld());
	const FString Was = D().Name;
	if (D().Tier >= 2)
	{
		Kind = Sausage;
		Build();
		if (GM) { GM->Broadcast(FString::Printf(TEXT("Муравьи подменили: %s → сосиска!"), *Was), FLinearColor(0.5f, 1.f, 0.5f)); }
	}
	else
	{
		// Dragged under the sofa.
		const AntWreck::FPiece* Sofa = AntWreck::PieceOf(GetWorld(), AntWreck::FindPiece(GetWorld(), TEXT("SM_L_Sofa")));
		const FVector At = Sofa ? FVector(Sofa->Box.GetCenter().X, Sofa->Box.GetCenter().Y, Sofa->Box.Min.Z + 5.f) : GetActorLocation();
		LayDown(At, 0.f);
		bStashed = true;
		if (GM) { GM->Broadcast(FString::Printf(TEXT("Муравьи утащили под диван: %s"), *Was), FLinearColor(0.5f, 1.f, 0.5f)); }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTWEAPON the ants did the %s: now %s, hidden %d"), *Was, D().Name, bStashed ? 1 : 0);
}
