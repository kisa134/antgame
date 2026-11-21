#include "AntHaulAdapter.h"
#include "AntCharacter.h"
#include "KitchenLoot.h"

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

namespace
{
	TMap<TWeakObjectPtr<AKitchenLoot>, FVector> GWanted;
}

namespace AntHaul
{
	bool AddCarrier(AKitchenLoot* Load, AAntCharacter* Ant)
	{
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
		return Ant->GrabLoot(Load);
	}

	void RemoveCarrier(AKitchenLoot* Load, AAntCharacter* Ant)
	{
		if (Ant && Ant->HasAuthority() && Ant->Carrying && (!Load || Ant->Carrying == Load))
		{
			Ant->LetGo();
		}
	}

	void GetCarriers(const AKitchenLoot* Load, TArray<AAntCharacter*>& Out)
	{
		Out.Reset();
		if (Load)
		{
			for (AAntCharacter* A : Load->Carriers)
			{
				if (IsValid(A)) { Out.Add(A); }
			}
		}
	}

	int32 NumCarriers(const AKitchenLoot* Load)
	{
		return Load ? Load->NumCarriers() : 0;
	}

	int32 MaxCarriers(const AKitchenLoot* Load)
	{
		return 8;   // (AKitchenLoot::TryGrab: "Вокруг уже нет места — 8 муравьёв держат это")
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
		if (Load)
		{
			GWanted.Add(Load, Dir);
		}
	}

	bool LoadSteersItself()
	{
		return false;
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
