#pragma once

#include "CoreMinimal.h"
#include "AntFood.generated.h"

class AKitchenLoot;
class AAntCharacter;
class AAntKitchenGameMode;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;
struct FAntFoodDef;
struct FAntEnv;

/**
 * What food is made of (KAN-36). Carrying a piece of cake, a gummy, a slice of cheese and a sugar cube are four different jobs:
 *
 *  Crumbly  cake, cookie, bread, sponge: bumps and shaking knock crumbs off (mass and value go), a trail of crumbs behind it;
 *  Elastic  gummies, marshmallow, gum: stretches between carriers, springs back, bounces when dropped (a catapult), squeezes
 *           through gaps it would not fit;
 *  Soft     cheese slice, sausage, noodle, pancake: sags between the grips and drags, drapes, folds - goes under a door, snags;
 *  Hard     sugar cube, nut, hard candy, bone: no give at all, inertia, slides down slopes, a ram; dropped, it breaks the fragile
 *           things it lands on;
 *  Rolling  grape, pea, olive: rolls by itself, a ball bearing under the Human's foot;
 *  Sticky   honey, jam, condensed milk: never dropped (a yank does not tear it out of the jaws), slow, leaves a sticky trail that
 *           holds rivals;
 *  Melting  ice cream, butter, chocolate, ice: shrinks with the heat (AntReact::HeatAt), leaves a puddle; shade and cold save it;
 *  Brittle  crisp, cracker, wafer, eggshell: snaps in two on a hit or under one carrier too many; halves are worth less;
 *  Juicy    apple slice, watermelon, tomato: squirts when squeezed or hit, gets lighter; the juice is a sticky puddle and bait.
 *
 * States of a piece of food (FAntFoodNet, replicated with the load): soaked, toasted, burnt, frozen, dirty, spoiled, bitten, melted.
 * The visible part goes into the shared state layer (AntState.h, source Food: Wet, Char, Dirt, Frost, Spoil) plus the packed
 * deformation in the 3 floats AntState leaves free (CpdDeform0, below).
 *
 * Deformation without soft bodies: the food materials move vertices by a few per-component numbers (sag, tip droop, squash,
 * stretch, wobble, melt, bite), worked out on every machine from what is already replicated (the grips of the carriers, the food
 * state). THE OLD MODELS KEEP THEIR LOOK: an old food's material (an instance of M_M4 / M_M4_UV / M_MeshyPBR) is swapped on the
 * visible component for a shared dynamic instance of the same master + the deformation layer (M_M4_Food, M_M4_UV_Food,
 * M_MeshyPBR_Food, Tools/food/make_food_materials.py) with the old instance's parameters copied: one per old instance, not per piece.
 *
 * New models (Blender, Tools/food/food_v2.py) only for foods that were not in the game before, and only with -AntFoodV2Models
 * (the owner compares first): by default there is no new food and every old food keeps its old model. The behaviour of the
 * kinds is on by default; -AntFoodKinds=0 turns it off.
 */

UENUM()
enum class EAntFoodKind : uint8
{
	None,
	Crumbly,
	Elastic,
	Soft,
	Hard,
	Rolling,
	Sticky,
	Melting,
	Brittle,
	Juicy,
	Count UMETA(Hidden)
};

/** Replicated with the load (KitchenLoot::FoodNet). Bytes 0..255 = 0..1. */
USTRUCT()
struct FAntFoodNet
{
	GENERATED_BODY()

	UPROPERTY() uint8 Kind = 0;        // EAntFoodKind
	UPROPERTY() uint8 Food = 255;      // index in the food table (AntFood::Table), 255: not in it
	UPROPERTY() uint8 Soak = 0;        // soaked through (water): heavier, falls apart
	UPROPERTY() uint8 Toast = 0;       // toasted: briefly worth more
	UPROPERTY() uint8 Burn = 0;        // burnt
	UPROPERTY() uint8 Frozen = 0;      // frozen: hard, does not spoil or crumble
	UPROPERTY() uint8 Dirt = 0;        // dirty
	UPROPERTY() uint8 Spoil = 0;       // gone off
	UPROPERTY() uint8 Melt = 0;        // melted away (melting kind)
	UPROPERTY() uint8 Bitten = 0;      // how much of it has been bitten off
	UPROPERTY() uint8 Squeeze = 0;     // elastic squeezing through a gap
	UPROPERTY() uint16 BaseWeight = 0; // the weight it had whole (sizes the model, scales the value)
	UPROPERTY() uint16 BaseValue = 0;
};

namespace AntFood
{
	/**
	 * The food's own custom primitive data: the 3 floats AntState leaves free (0..12 its kinds, 16..35 its spots; 36 is the
	 * engine's limit). The states go into AntState's kinds (source Food): soaked = Wet, toasted = Char 0..0.35, burnt = Char
	 * 0.35..1, frozen = Frost, dirty = Dirt, spoiled = Spoil. The deformation is packed, exact integers in a float (< 2^24):
	 *   13  round(Sag * 1023) * 1024 + round(Tip * 1023)
	 *   14  round(Stretch * 1023) * 1024 + round(Squash * 1023)
	 *   15  round(Wobble * 255) * 65536 + round(Melt * 255) * 256 + round(Bite * 63) * 4 + Axis * 2 + (TipSign < 0)
	 * Read by FOOD_DEFORM in Tools/food/make_food_materials.py (M_FoodV2, M_M4_Food, M_M4_UV_Food, M_MeshyPBR_Food).
	 */
	constexpr int32 CpdDeform0 = 13;
	ANTKITCHEN_API void PackDeform(float Sag, float Tip, float TipSign, float Stretch, float Squash, int32 Axis, float Wobble, float Melt, float Bite, float Out[3]);

	/** One food the game knows (the Blender model SM_FoodV2_<Id>, its pieces, its behaviour). */
	struct FFood
	{
		const TCHAR* Id;          // SM_FoodV2_<Id>
		EAntFoodKind Kind;
		const TCHAR* Keys;        // '|'-separated parts of loot names that mean this food (first match wins, table order)
		const TCHAR* Ru;          // its own name (new foods)
		int32 Value;
		int32 Weight;
		float SizeCm;             // the longest side of the real thing
		float SpoilMin;           // minutes to go fully off at room temperature (0: never)
		FLinearColor Color;
		int32 Pieces;             // pre-cut break pieces of the model
		bool bNew;                // not in the old game: rolled only with -AntFoodV2
	};

	ANTKITCHEN_API TConstArrayView<FFood> Table();
	ANTKITCHEN_API const FFood* Find(const FString& LootName);
	ANTKITCHEN_API const FFood* ById(const FString& Id);
	ANTKITCHEN_API EAntFoodKind KindOf(const FString& LootName);
	ANTKITCHEN_API const TCHAR* KindName(EAntFoodKind K);

	/** -AntFoodV2: the new models. */
	ANTKITCHEN_API bool V2();
	/** Behaviour of the kinds (default on; -AntFoodKinds=0 off). */
	ANTKITCHEN_API bool Kinds();
	/** -AntFoodV2: the new model's mesh (null: none, use the old art). */
	ANTKITCHEN_API UStaticMesh* ModelV2(const AKitchenLoot* L, int32 Piece = -1, UMaterialInterface** OutMat = nullptr, UMaterialInterface** OutInner = nullptr);
	/** Pieces a break makes (the pre-cut ones of the V2 model), 0: the old rule. */
	ANTKITCHEN_API int32 BreakPieces(const AKitchenLoot* L);
	/** A new food of the V2 set for the loot tables (-AntFoodV2 only). False: keep the old roll. */
	ANTKITCHEN_API bool RollNew(int32 Tier, FRandomStream& R, FAntFoodDef& Out);
	/** A test food by table id or kind name (dev, AntCarry "carry spawn"). */
	ANTKITCHEN_API bool TestDef(const FString& What, FAntFoodDef& Out);

	// ------------------------------------------------------------------ hooks (KitchenLoot)
	/** Server, BeginPlay / InitFood: kind, table entry, base weight. */
	ANTKITCHEN_API void Init(AKitchenLoot* L);
	/** Server, every tick: the kinds' behaviour and the states. */
	ANTKITCHEN_API void ServerTick(AKitchenLoot* L, float Dt);
	/** Every machine but a dedicated server: the deformation numbers on the visible component. */
	ANTKITCHEN_API void VisualTick(AKitchenLoot* L, UPrimitiveComponent* Visual, float Dt);
	/** Server: it hit something at this speed (KitchenLoot::OnBodyHit). */
	ANTKITCHEN_API void OnImpact(AKitchenLoot* L, float Speed, const FVector& At, AActor* Other);
	/** Server: an ant let go of it (elastic: the spring fires). */
	ANTKITCHEN_API void OnRelease(AKitchenLoot* L, const FVector& GripWorld);
	/** Server: one more carrier took hold (brittle: one too many snaps it). */
	ANTKITCHEN_API void OnGrab(AKitchenLoot* L);
	/** Server: bitten (KitchenLoot::BiteOff). */
	ANTKITCHEN_API void OnBitten(AKitchenLoot* L);
	/** Server: how far a carrier may be pulled from its grip before it loses it (sticky: much further). */
	ANTKITCHEN_API float LeashMul(const AKitchenLoot* L);
	/** Physics of the kind: friction, bounce, damping (KitchenLoot::SetupPhysics). Null: the plain food material. */
	ANTKITCHEN_API class UPhysicalMaterial* PhysMat(const AKitchenLoot* L, float& LinDamp, float& AngDamp);
	/** -AntFoodV2: the scale of the new model (real size, smaller as it loses weight). */
	ANTKITCHEN_API float ScaleV2(const AKitchenLoot* L);
	/** Value of the piece now, after its states. */
	ANTKITCHEN_API int32 ValueNow(const AKitchenLoot* L);
	/** Value multiplier for the pieces of a break (brittle halves are worth less). */
	ANTKITCHEN_API float BreakValueMul(const AKitchenLoot* L);
	/** Short text of the states (HUD, logs). */
	ANTKITCHEN_API FString Describe(const AKitchenLoot* L);
	/** Server, KitchenBuilder's hazard pass: honey, jam and juice on the floor (and sticky food itself) hold the ants of the other side. */
	ANTKITCHEN_API void AffectAnt(AAntCharacter* Ant, FAntEnv& Env);
	/** Listens to AntCarry's events (delivered, broken, snatched) for the carry log. */
	ANTKITCHEN_API void RegisterEvents();

	/** Dev (-AntDo): food list | food spawn <id|kind> [x y z] | food state <soak|toast|burn|frozen|dirt|spoil|melt|bite> <0..1> | food log 0|1 | food report. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
