#include "AntHaulAdapter.h"
#include "AntCharacter.h"
#include "KitchenLoot.h"
#include "AntCarry.h"
#include "AntRts.h"
#include "AntTypes.h"

// MERGE NOTES (code/ANT-swarm + code/ANT-carry, API as of code/ANT-carry 8119858 "AntCarry.h")
// ----------------------------------------------------------------------------------------------
// Every squad order that carries something goes through these functions (AntRts.cpp). After the merge, only this file changes:
//  * AddCarrier(L, Ant)      -> AntCarry::AddCarrier(L, Ant)   (ants still enter through GrabLoot inside it; keep the MaxCarriers check)
//  * RemoveCarrier(L, Ant)   -> AntCarry::RemoveCarrier(L, Ant)
//  * GetCarriers(L, Out)     -> AntCarry::GetCarriers(L, Infos), keep the entries with Worker == -1 and Cast<AAntCharacter>(Who)
//  * NumCarriers(L)          -> the size of that list (workers of an army count too: Infos.Num())
//  * MaxCarriers(L)          -> AntCarry::MaxCarriers(L)       (today 8, AKitchenLoot::TryGrab)
//  * SetDesiredDirection(L, Dir) -> for every carrier of the squad on it: AntCarry::SetDesired(L, Ant, -1, Dir * <pace>);
//    LoadSteersItself() -> true: then AAntSquadBot::HaulThink lets the leader feed the route direction and lets the carry code
//    walk the load (the other squad carriers already only keep their grip through CarryFollow / the new tether).
//  * OnDelivered             -> bind AntCarry::Events() and forward AntCarryEvent::Delivered to NotifyDelivered; then delete the
//    AntHaul::NotifyDelivered call in AntSnowball::OnDelivered (it would fire twice).
//  * Ratio(L)                -> unchanged (AKitchenLoot::CarryRatio still exists on the carry branch).
// Conflicts to expect when merging the branches (both touched them): AntRaid.cpp, AntRun.cpp, AntSquad.cpp, AntHUD.cpp,
// AntArmy.cpp, AntCharacter.cpp, docs/FEATURES.md (their section 23 is "carrying", ours is 24). All our edits there are one-liners.

// v12: merged with code/ANT-carry - the bodies below speak to AntCarry (AntCarry.h).

namespace
{
	TMap<TWeakObjectPtr<AKitchenLoot>, FVector> GWanted;
	bool GBound = false;
	/** AntCarry's own Delivered event is the one delivery signal (AntSnowball::OnDelivered no longer fires it). */
	void Bind()
	{
		if (GBound)
		{
			return;
		}
		GBound = true;
		AntCarry::Events().AddLambda([](AKitchenLoot* L, uint8 Kind, uint8 Team)
		{
			if (Kind == AntCarryEvent::Delivered)
			{
				AntHaul::NotifyDelivered(L, Team);
			}
		});
	}
}

namespace AntHaul
{
	bool AddCarrier(AKitchenLoot* Load, AAntCharacter* Ant)
	{
		Bind();
		if (!Load || !Ant || !Ant->HasAuthority())
		{
			return false;
		}
		if (Ant->Carrying == Load)
		{
			return true;
		}
		if (NumCarriers(Load) >= MaxCarriers(Load))
		{
			return false;
		}
		return AntCarry::AddCarrier(Load, Ant);
	}

	void RemoveCarrier(AKitchenLoot* Load, AAntCharacter* Ant)
	{
		if (Ant && Ant->HasAuthority() && Ant->Carrying && (!Load || Ant->Carrying == Load))
		{
			AntCarry::RemoveCarrier(Ant->Carrying, Ant);
		}
	}

	void GetCarriers(const AKitchenLoot* Load, TArray<AAntCharacter*>& Out)
	{
		Out.Reset();
		TArray<FAntCarrierInfo> Infos;
		AntCarry::GetCarriers(Load, Infos);
		for (const FAntCarrierInfo& I : Infos)
		{
			if (I.Worker < 0)
			{
				if (AAntCharacter* A = Cast<AAntCharacter>(I.Who); IsValid(A)) { Out.Add(A); }
			}
		}
	}

	int32 NumCarriers(const AKitchenLoot* Load)
	{
		TArray<FAntCarrierInfo> Infos;
		AntCarry::GetCarriers(Load, Infos);
		return Infos.Num();   // workers of an army count too
	}

	int32 MaxCarriers(const AKitchenLoot* Load)
	{
		return Load ? AntCarry::MaxCarriers(Load) : 0;
	}

	bool IsCarrying(const AAntCharacter* Ant, const AKitchenLoot* Load)
	{
		return Ant && Load && Ant->Carrying == Load;
	}

	float Ratio(const AKitchenLoot* Load)
	{
		return Load ? Load->CarryRatio() : 0.f;
	}

	void SetDesiredDirection(AKitchenLoot* Load, const FVector& Dir)
	{
		Bind();
		if (!Load)
		{
			return;
		}
		GWanted.Add(Load, Dir);
		// Every squad ant on it pushes that way at a brisk pace (AntCarry drives the load from its carriers' wishes).
		const FVector V = Dir.GetSafeNormal2D() * 480.f * AntKit::SpeedK * 0.85f;
		TArray<AAntCharacter*> Cs;
		GetCarriers(Load, Cs);
		for (AAntCharacter* A : Cs)
		{
			if (A && !A->IsPlayerControlled() && Cast<AAntSquadBot>(A->GetController()))
			{
				AntCarry::SetDesired(Load, A, -1, V);
			}
		}
	}

	bool LoadSteersItself()
	{
		return true;
	}

	bool CanHaul(const AKitchenLoot* Load)
	{
		return Load && !Load->IsActorBeingDestroyed() && !Load->bIsGear && !Load->bVault && !(Load->bHeld && !Load->SquadHolder.IsValid());
	}

	FOnDelivered& OnDelivered()
	{
		static FOnDelivered Event;
		return Event;
	}

	void NotifyDelivered(AKitchenLoot* Load, uint8 Team)
	{
		GWanted.Remove(Load);
		OnDelivered().Broadcast(Load, Team);
	}
}
