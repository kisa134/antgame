#include "AntHaulAdapter.h"
#include "AntCharacter.h"
#include "KitchenLoot.h"

// MERGE NOTES (code/ANT-swarm + code/ANT-carry)
// ---------------------------------------------
// Every squad order that carries something goes through these functions (AntRts.cpp, AntTrail.cpp). After the merge:
//  * AddCarrier / RemoveCarrier / GetCarriers -> the new AKitchenLoot API of the same names (grip points, shared load);
//    today: AAntCharacter::GrabLoot / LetGo and AKitchenLoot::Carriers.
//  * MaxCarriers -> the new limit (more carriers above the weight give more speed); today 8 (AKitchenLoot::TryGrab).
//  * SetDesiredDirection -> the new desired direction input; LoadSteersItself() -> true. Then the squad's haul leader stops
//    walking the route itself (AntRts.cpp, HaulTick: "if (!AntHaul::LoadSteersItself())") and only feeds the direction.
//  * OnDelivered -> bind the new delivered event and call NotifyDelivered from it (or forward it); remove the call in
//    AAntKitchenGameMode::OnFoodDelivered (AntSnowball::OnDelivered calls NotifyDelivered).
//  * Ratio -> the new strength / weight of the load.

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
