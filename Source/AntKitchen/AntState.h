#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UWorld;
class AAntKitchenGameMode;

/**
 * The visible states of a thing (owner's rule: what gets wet, burns, gets dirty or damaged shows it on itself).
 * One layer in the common material M_M4 (Tools/map4/make_m4_materials.py), driven per component through custom primitive
 * data, so no material instance per object is needed and the default (all zero) costs nothing.
 * Values 0..1. Local to the machine that sets them: the systems that own the cause (water, fire, mess, wreck) call this on
 * every machine from their own replicated state.
 */
namespace AntState
{
	enum EKind : uint8 { Wet = 0, Char = 1, Dirt = 2, Damage = 3, Count };

	ANTKITCHEN_API void Set(UPrimitiveComponent* C, EKind Kind, float Value);
	ANTKITCHEN_API float Get(const UPrimitiveComponent* C, EKind Kind);
	/** Every mesh component whose bounds centre lies within Radius of At. Returns how many. */
	ANTKITCHEN_API int32 SetAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Value);
	/** Dev: -AntDo "state wet|char|dirt|damage|clear <0..1> <x> <y> <z> [radius]". */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}