#include "KitchenMover.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "AntTypes.h"
#include "AntRun.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AKitchenMover::AKitchenMover()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractRadius = 300.f;
}

void AKitchenMover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenMover, PartIndex);
	DOREPLIFETIME(AKitchenMover, bOpen);
	DOREPLIFETIME(AKitchenMover, Ajar);
}

void AKitchenMover::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AntArtOpen")))   // dev: everything starts open (to look inside)
	{
		bOpen = true;
	}
	Build();
	Alpha = bOpen ? 1.f : Ajar;
	ApplyAlpha();
}

void AKitchenMover::Build()
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return;
	}
	const FKitchenArtMover& M = All[PartIndex];
	const FKitchenArtInst& I = KitchenArt::Instances()[M.Inst];

	// The actor stands at the part's origin (hinge line / back of the drawer), unrotated: the motion is in game space.
	Hinge = AntKit::AddPivot(this, Root, FVector::ZeroVector);
	Body = KitchenArt::AddAt(this, Hinge, I.Name, I.Index, KitchenArt::ESolid::Moving, false, TEXT("ArtBody"));
	// Whatever stands on this part travels with it (plates in the dishwasher rack...).
	int32 N = 0;
	for (const FKitchenArtInst& R : KitchenArt::Instances())
	{
		if (R.Role == KitchenArt::Rider && R.Parent == PartIndex)
		{
			KitchenArt::AddAt(this, Hinge, R.Name, R.Index, KitchenArt::ESolid::Moving, false, FName(*FString::Printf(TEXT("ArtRider%d"), N++)));
		}
	}

	// Where it can be used from: around the part, shut and open (also from behind it: an ant shut inside gets out).
	UsePointsLocal.Reset();
	if (Body && Body->GetStaticMesh())
	{
		const FBox B = Body->GetStaticMesh()->GetBoundingBox().TransformBy(Body->GetRelativeTransform());
		const FVector C = B.GetCenter(), E = B.GetExtent();
		TArray<FVector> Pts;
		for (int32 sx = -1; sx <= 1; ++sx)
		{
			for (int32 sy = -1; sy <= 1; ++sy)
			{
				Pts.Add(C + FVector(sx * E.X, sy * E.Y, -E.Z));
			}
		}
		Pts.Add(C);
		Pts.Add(C + FVector(0.f, 0.f, E.Z));
		const FTransform Open = M.Type == 0
			? FTransform(FQuat(FVector(M.Axis).GetSafeNormal(), FMath::DegreesToRadians(M.Angle)))
			: FTransform(FVector(M.Slide));
		for (const FVector& P : Pts)
		{
			UsePointsLocal.Add(P);
			UsePointsLocal.Add(Open.TransformPosition(P));
		}
	}
}

void AKitchenMover::ApplyAlpha()
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!Hinge || !All.IsValidIndex(PartIndex))
	{
		return;
	}
	const FKitchenArtMover& M = All[PartIndex];
	const float K = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
	if (M.Type == 0)
	{
		Hinge->SetRelativeRotation(FQuat(FVector(M.Axis).GetSafeNormal(), FMath::DegreesToRadians(M.Angle * K)));
	}
	else
	{
		Hinge->SetRelativeLocation(FVector(M.Slide) * K);
	}
}

bool AKitchenMover::IsBlocked() const
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return false;
	}
	const int32 Flags = All[PartIndex].Flags;
	if (!(Flags & (KitchenArt::NeedsFridge | KitchenArt::NeedsDishwasher)))
	{
		return false;
	}
	const EKitchenDoorKind Need = (Flags & KitchenArt::NeedsFridge) ? EKitchenDoorKind::Fridge : EKitchenDoorKind::Dishwasher;
	for (TActorIterator<AKitchenDoor> It(GetWorld()); It; ++It)
	{
		if (It->Kind == Need)
		{
			return !It->IsOpen();
		}
	}
	return false;
}

void AKitchenMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PullTick(DeltaSeconds);
	const float Strain = StrainNow(DeltaSeconds);
	const float Target = bOpen ? 1.f : Ajar;
	if (!bOpen && Strain > 0.f)
	{
		// Not enough ants on it: it gives a crack and falls back (the travel is eased, so undo the ease).
		Alpha = FMath::Max(Ajar, FMath::Sqrt(Strain * 0.5f));
		ApplyAlpha();
	}
	else if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, 1.5f);
		ApplyAlpha();
	}

	if (!HasAuthority())
	{
		return;
	}
	if (!bOpen)
	{
		// The door between the rooms: the Human and the pets never find it shut in their way, they push it open.
		const TConstArrayView<FKitchenArtMover> Parts = KitchenArt::Movers();
		ServerCheck -= DeltaSeconds;
		if (ServerCheck <= 0.f && Parts.IsValidIndex(PartIndex) && (Parts[PartIndex].Flags & KitchenArt::BigOpens))
		{
			ServerCheck = 0.25f;
			bool bNear = false;
			for (TActorIterator<AHumanCharacter> It(GetWorld()); It && !bNear; ++It)
			{
				bNear = FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 1900.f;
			}
			for (TActorIterator<ACritter> It(GetWorld()); It && !bNear; ++It)
			{
				bNear = FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 1900.f;
			}
			if (bNear)
			{
				SetOpen(true);
				AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation(), 0.9f);
			}
		}
		return;
	}
	if (AutoCloseAt > 0.f && Now() >= AutoCloseAt)
	{
		SetOpen(false);
		return;
	}
	ServerCheck -= DeltaSeconds;
	if (ServerCheck > 0.f)
	{
		return;
	}
	ServerCheck = 0.25f;
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return;
	}
	// The fridge door swings shut on the crisper, the dishwasher door lifts the racks back in.
	if (IsBlocked())
	{
		SetOpen(false);
		return;
	}
	// The Human walks into an open cabinet door or drawer: he bumps it shut and walks on (it never traps him).
	if ((All[PartIndex].Flags & KitchenArt::HumanShuts) && Body && Alpha > 0.9f)
	{
		const FBox B = Body->Bounds.GetBox();
		for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
		{
			const UCapsuleComponent* Cap = It->GetCapsuleComponent();
			const float R = Cap ? Cap->GetScaledCapsuleRadius() + 60.f : 400.f;
			const float Feet = It->GetActorLocation().Z - (Cap ? Cap->GetScaledCapsuleHalfHeight() : 1700.f);
			const FVector P = It->GetActorLocation();
			if (P.X > B.Min.X - R && P.X < B.Max.X + R && P.Y > B.Min.Y - R && P.Y < B.Max.Y + R && B.Max.Z > Feet + 250.f)
			{
				SetOpen(false);
				AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation(), 1.5f);
				break;
			}
		}
	}
}

int32 AKitchenMover::DoorClass() const
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return 0;
	}
	const FKitchenArtMover& M = All[PartIndex];
	if ((M.Flags & (KitchenArt::NeedsFridge | KitchenArt::NeedsDishwasher)) && IsBlocked())
	{
		return 0;   // behind another door: a plain [E] says which one to open first
	}
	const FString Name = KitchenArt::Instances()[M.Inst].Name;
	if (Name.Contains(TEXT("DoorFreezer")) || Name.Contains(TEXT("CatFlap")))
	{
		return AntDoor::Heavy;   // (the cat flap: about three foragers, and only from the living room: CanPull)
	}
	if (M.Type == 1 || Name.Contains(TEXT("Lid")) || Name.Contains(TEXT("Gate")) || Name.Contains(TEXT("Door_Leaf")))
	{
		return AntDoor::Light;   // drawers, racks, the crisper, sliding doors, lids, the gate, the door between the rooms
	}
	return AntDoor::Medium;      // cabinet doors
}

FString AKitchenMover::DoorNoun() const
{
	// "Открыть дверцу шкафа" -> "дверцу шкафа"; "Выдвинуть ящик" -> "ящик".
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	FString Verb, Noun;
	if (All.IsValidIndex(PartIndex) && FString(All[PartIndex].OpenText).Split(TEXT(" "), &Verb, &Noun))
	{
		int32 Paren = INDEX_NONE;
		if (Noun.FindChar(TEXT('('), Paren)) { Noun = Noun.Left(Paren).TrimEnd(); }
		return Noun;
	}
	return TEXT("дверцу");
}

bool AKitchenMover::HumanShuts() const
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return false;
	}
	const FKitchenArtMover& M = All[PartIndex];
	// Not what stands open by design (the sink door, the gate, the yard bin), not the door between the rooms, not the cat flap
	// (it falls shut by itself).
	return !(M.Flags & (KitchenArt::StartsOpen | KitchenArt::BigOpens)) && !FString(KitchenArt::Instances()[M.Inst].Name).StartsWith(TEXT("SM_Y_")) && !IsCatFlap();
}

bool AKitchenMover::IsCatFlap() const
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	return All.IsValidIndex(PartIndex) && FCString::Strcmp(KitchenArt::Instances()[All[PartIndex].Inst].Name, TEXT("SM_L_CatFlap_Leaf")) == 0;
}

bool AKitchenMover::CanPullFrom(const FVector& P) const
{
	return !IsCatFlap() || LevelV2::OnLatchSideOfCatFlap(P);
}

bool AKitchenMover::CanPull(AAntCharacter* Ant, bool bTell) const
{
	if (!Ant || !IsCatFlap() || LevelV2::OnLatchSideOfCatFlap(Ant->GetActorLocation()))
	{
		return true;
	}
	if (bTell)
	{
		Ant->Toast(TEXT("Засов изнутри: кошачью дверцу открывают только из гостиной"));
		UE_LOG(LogTemp, Display, TEXT("ANTV2 catflap: %s pulls from the yard side, latched"), *Ant->GetDisplayName());
	}
	return false;
}

void AKitchenMover::SetDoorOpen(bool bInOpen)
{
	// Parts with a spring (the freezer door, the cat flap) fall shut again however they were opened.
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	SetOpen(bInOpen, (bInOpen && All.IsValidIndex(PartIndex)) ? All[PartIndex].AutoClose : -1.f);
	if (IsCatFlap())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTV2 catflap: %s"), bInOpen ? TEXT("open, falls shut in 15 s") : TEXT("shut"));
	}
}

void AKitchenMover::SetOpen(bool bInOpen, float AutoClose)
{
	if (!bInOpen && bOpen && IsHeldOpen())
	{
		AutoCloseAt = AutoCloseAt > 0.f ? Now() + 1.f : AutoCloseAt;   // a squad holds it open: it falls shut only when they let go
		return;
	}
	if (bInOpen && !bOpen)
	{
		OpenedAt = Now();
	}
	bOpen = bInOpen;
	if (!bInOpen) { Ajar = 0.f; }   // (shut by someone: shut for good, not back to the seed's crack)
	AutoCloseAt = (bInOpen && AutoClose > 0.f) ? Now() + AutoClose : -1.f;
	ServerCheck = 0.6f;
}

FString AKitchenMover::GetPrompt(const AAntCharacter* Ant) const
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return FString();
	}
	if (!bOpen && Ant && IsCatFlap() && !LevelV2::OnLatchSideOfCatFlap(Ant->GetActorLocation()))
	{
		return TEXT("Кошачья дверца (заперта изнутри: открывают из гостиной)");
	}
	return bOpen ? All[PartIndex].CloseText : All[PartIndex].OpenText;
}

void AKitchenMover::Interact(AAntCharacter* Ant)
{
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	if (!All.IsValidIndex(PartIndex))
	{
		return;
	}
	const FKitchenArtMover& M = All[PartIndex];
	if (!bOpen && IsBlocked())
	{
		if (Ant)
		{
			Ant->Toast((M.Flags & KitchenArt::NeedsFridge) ? TEXT("Сначала открой дверь холодильника") : TEXT("Сначала открой посудомойку"));
		}
		return;
	}
	SetOpen(!bOpen, M.AutoClose);
	AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation(), bOpen ? 1.35f : 1.6f);
}

bool AKitchenMover::HumanOpensOne(UWorld* World, FRandomStream& Rng, float Seconds)
{
	TArray<AKitchenMover*> Free;
	const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
	for (TActorIterator<AKitchenMover> It(World); It; ++It)
	{
		if (!It->bOpen && All.IsValidIndex(It->PartIndex) && (All[It->PartIndex].Flags & KitchenArt::HumanOpens))
		{
			Free.Add(*It);
		}
	}
	if (Free.Num() == 0)
	{
		return false;
	}
	AKitchenMover* M = Free[Rng.RandRange(0, Free.Num() - 1)];
	M->SetOpen(true, Seconds);
	AntSfx::Everyone(M, AntSfx::FridgeDoor, M->GetActorLocation(), 1.35f);
	return true;
}