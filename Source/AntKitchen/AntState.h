#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UWorld;
class AAntKitchenGameMode;

/**
 * The visible states of a thing (owner's rule: what gets wet, burns, gets dirty or damaged shows it on itself).
 *
 * ONE LAYER, MANY CAUSES. Every system that owns a cause (the matter system: water, heat, fire, frost, electricity; the mess;
 * the wreck; the food) writes ITS OWN values into its own source channel of a component; the layer shows the strongest of all
 * channels. So the water drying a towel never clears the char the fire left on it, and two systems never fight over a value.
 *
 * WHAT IS SHOWN. Per component: an overall value per kind (the whole thing is wet) and up to MaxSpots spots (a rag wet at one
 * end and smouldering at the other). The spots of all channels are merged (the strongest win) when there are more than fit.
 * Drawn by the common materials M_M4, M_M4_2S (triplanar) and M_M4_UV, M_M4_UV2 (generated props, prints), built by
 * Tools/map4/make_m4_materials.py, through custom primitive data - no material instance per object, all zero costs nothing:
 *   floats 0..12   the overall value of each EKind (index = kind)
 *   floats 16..35  five spots, a float4 each: xyz = position in the component's LOCAL space (it moves with the thing: a door
 *                  that opens keeps its wet corner), w = packed round(radius_local * 4) * 1024 + kind * 64 + round(strength * 63).
 * A component whose material is not one of these simply does not show it (the values are still kept and can be queried).
 *
 * WHERE IT RUNS. Local to the machine that sets it: the owning systems call this on every machine from their own replicated
 * state (AAntMatterNet's records, AAntMess's stains, AAntWreckBody's stage and dents). Gameplay on the server should read its
 * own authoritative numbers (AntMatter.h) - but ValueAt() is there for anything cosmetic or local.
 *
 * STABLE API (the food branch builds on it): EKind values and their order, ESource, Set/Get, SetSource/GetSource, SetSpots,
 * AddSpot, ClearSource, ValueAt, SetAround, SpotAround, KindName/KindFromName. New kinds or sources are only ever appended.
 */
namespace AntState
{
	/** What a thing can show. The index is also the custom primitive data slot of its overall value. */
	enum EKind : uint8
	{
		Wet = 0,      // dark and glossy
		Char = 1,     // 0..0.35 browns (toast), above that blackens through a noise threshold; irreversible within a raid
		Dirt = 2,     // brown tint, rough
		Damage = 3,   // cracks, dents, a sag (wreck stages; melted plastic)
		Heat = 4,     // 0 at 100 C .. 1 at 800 C: metal glows red, then orange (heat shimmer: AntMatter's FX)
		Burn = 5,     // flame on the surface: glowing embers inside, a bright rim at a spot's edge (the flames themselves: AntMatter's FX)
		Frost = 6,    // white crystals, rough
		Charge = 7,   // live / static: blue sparks crawling on it
		Oil = 8,      // soaked with oil: golden, glossy
		Jam = 9,      // soaked with jam: dark red, sticky gloss
		Cola = 10,    // soaked with cola: brown
		Honey = 11,   // soaked with honey: amber, glossy
		Spoil = 12,   // food going off: dull, desaturated, mould spots (the food system sets it)
		Count
	};

	/** Who set a value. Each source owns its channel; what is shown is the maximum over the channels. */
	enum ESource : uint8
	{
		Dev = 0,      // -AntDo "state ..." and the admin
		Matter = 1,   // AntMatter (water, heat, fire, frost, electricity), from AAntMatterNet's replicated records
		Mess = 2,     // AntMess stains that landed on the thing
		Wreck = 3,    // AAntWreckBody's stage and dents
		Food = 4,     // the food system (spoiling, juice)
		NumSources
	};

	constexpr int32 MaxSpots = 5;
	constexpr int32 SpotData = 16;   // custom primitive data index of the first spot

	struct FSpot
	{
		FVector Local = FVector::ZeroVector;   // component-local position
		float Radius = 0.f;                    // WORLD units (converted to local by the component's scale when drawn)
		uint8 Kind = 0;
		float Strength = 1.f;                  // 0..1
	};

	/** Dev source (the old API): the overall value of Kind. */
	ANTKITCHEN_API void Set(UPrimitiveComponent* C, EKind Kind, float Value);
	/** What is shown overall (the max over the sources). */
	ANTKITCHEN_API float Get(const UPrimitiveComponent* C, EKind Kind);

	/** One source's overall value of Kind. */
	ANTKITCHEN_API void SetSource(UPrimitiveComponent* C, ESource Source, EKind Kind, float Value);
	ANTKITCHEN_API float GetSource(const UPrimitiveComponent* C, ESource Source, EKind Kind);
	/** Replaces all spots of one source. */
	ANTKITCHEN_API void SetSpots(UPrimitiveComponent* C, ESource Source, TConstArrayView<FSpot> Spots);
	/** Adds a spot at a WORLD point (merges with a spot of the same kind it overlaps: the larger radius, the stronger strength). */
	ANTKITCHEN_API void AddSpot(UPrimitiveComponent* C, ESource Source, const FVector& World, float Radius, EKind Kind, float Strength);
	/** Everything of one source on C (values and spots). */
	ANTKITCHEN_API void ClearSource(UPrimitiveComponent* C, ESource Source);
	/** Everything of one source on every component (a new raid). */
	ANTKITCHEN_API void ClearSourceEverywhere(ESource Source);
	/** What C shows of Kind at a world point (the overall value or a spot covering the point). */
	ANTKITCHEN_API float ValueAt(const UPrimitiveComponent* C, const FVector& World, EKind Kind);

	/** Dev: every mesh component whose bounds centre lies within Radius of At gets the overall value. Returns how many. */
	ANTKITCHEN_API int32 SetAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Value);
	/** Dev: every mesh component whose bounds reach within Radius of At gets a spot there (a partial state). Returns how many. */
	ANTKITCHEN_API int32 SpotAround(UWorld* World, const FVector& At, float Radius, EKind Kind, float Strength, ESource Source = Dev);

	ANTKITCHEN_API const TCHAR* KindName(uint8 Kind);
	/** "wet", "char", "dirt", "damage", "heat", "burn", "frost", "charge", "oil", "jam", "cola", "honey", "spoil" (or a number); -1 if none. */
	ANTKITCHEN_API int32 KindFromName(const FString& Name);
	/** How many components carry any state now (stats). */
	ANTKITCHEN_API int32 NumTracked();

	/**
	 * Dev: -AntDo "state <kind>|clear <0..1> <x> <y> <z> [radius]"      the whole of every component near the point
	 *              "state spot <kind> <0..1> <x> <y> <z> [radius]"     a spot on every component the sphere touches
	 *              "state info <x> <y> <z> [radius]"                   what the components there show
	 */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
