#pragma once

#include "CoreMinimal.h"
#include "LevelV2.h"

struct FKitchenArtInst;
struct FKitchenArtMover;
class UWorld;

/**
 * Level v3 (docs/level/LEVEL_V3.md): the same flat, directed — one family, one evening — with 14 districts of 3-4 variants each,
 * a raid seed, fasteners and things that break. Behind -AntLevelV3 (v2 stays the default until v3 is accepted).
 * The tables are data, generated from docs/level/level_v3.json, the Blender manifest and level_v3_layout.compose() by
 * Tools/level/gen_level_v3_code.py into LevelV3Art.inl (the base placement) and LevelV3Data.inl (gameplay, variants, the seed table).
 *
 * THE SEED: the game never composes a layout itself. The generator ran compose(seed, tier) for seeds 0..N-1 of every tier and kept only
 * those that pass the seed checks (check_level_v3: cake routes, cover <= 4 s, the Human's reach, nothing floating or overlapping, door
 * sweeps; dropped scatter re-drawn). The host picks one of them, replicates (seed, tier) through the game state, and every machine applies
 * the same rows: the layout is identical everywhere by construction.
 */
struct FLevelV3Op { int32 Kind; int32 A; int32 B; float F[6]; const TCHAR* Text; const TCHAR* Ru; };
struct FLevelV3Variant { int32 District; TCHAR Letter; const TCHAR* Name; const TCHAR* Plays; int32 FirstOp; int32 NumOps; int32 When; };
struct FLevelV3Fixture { const TCHAR* Id; const TCHAR* Obj; int32 Inst; const TCHAR* Name; int32 Mat; int32 FirstFastener; int32 NumFasteners; const TCHAR* District; const TCHAR* Undo; const TCHAR* Result; };
struct FLevelV3FastenerDef { const TCHAR* Id; int32 Inst; int32 Kind; int32 Fixture; };
struct FLevelV3Seed { int32 Seed; int32 Tier; int32 Time; int32 Home; const TCHAR* Variants; int32 Cake; int32 Doors[3]; int32 Loose[3]; int32 ScatterFirst; int32 ScatterNum; };
struct FLevelV3Scatter { int32 Zone; int32 Item; int32 HasPos; float X, Y, Z, Yaw; };

namespace LevelV3
{
	enum EOp { Add = 0, Remove = 1, Move = 2, Mover = 3, Hide = 4, HideOff = 5, Hazard = 6, HazardOff = 7, DogSpot = 8, LootZ = 9, ConnOff = 10, Edge = 11, Cake = 12, Gadget = 13 };
	enum EMat { Glass = 0, Ceramic, Terracotta, PlasticBottle, Plastic, Cardboard, Paper, Cloth, Latex, Wood, Metal, Yarn, Food, Tin };
	enum EFastener { Screw = 0, Hinge, Nail, Magnet, Tape, Ring, Knot, Cap, Needle, Friction, Zip, Code };
	enum EHome { Grandma = 1, Kid = 2, Dog = 4, Cat = 8 };
	enum ETime { Day = 0, Evening = 1, Night = 2 };

	/** The interactive objects are fixture actors (not static pieces of the base). */
	ANTKITCHEN_API bool FixturesOn();
	/** -AntLevelV3 on the command line (and the meshes are in the build). -AntLevelV2 always wins. */
	ANTKITCHEN_API bool Enabled();

	// ---- the static tables
	ANTKITCHEN_API TConstArrayView<FKitchenArtInst> BaseArt();
	ANTKITCHEN_API TConstArrayView<FKitchenArtMover> BaseMovers();
	ANTKITCHEN_API const TCHAR* BaseId(int32 Inst);
	ANTKITCHEN_API uint8 BaseFlags(int32 Inst);   // 1 = ants only
	ANTKITCHEN_API const TCHAR* BaseTint(int32 Inst);
	ANTKITCHEN_API int32 FindBase(const TCHAR* Id);
	ANTKITCHEN_API TConstArrayView<FKitchenArtInst> VarArt();
	ANTKITCHEN_API uint8 VarFlags(int32 i);
	ANTKITCHEN_API const TCHAR* VarTint(int32 i);
	ANTKITCHEN_API TConstArrayView<FLevelV3Op> Ops();
	ANTKITCHEN_API TConstArrayView<FLevelV3Variant> Variants();
	ANTKITCHEN_API int32 NumDistricts();
	ANTKITCHEN_API const TCHAR* DistrictId(int32 D);
	ANTKITCHEN_API const TCHAR* DistrictName(int32 D);
	ANTKITCHEN_API TConstArrayView<FLevelV3Fixture> Fixtures();
	ANTKITCHEN_API TConstArrayView<FLevelV3FastenerDef> Fasteners();
	/** Base rows the fixture actors build themselves (objects, parts and fasteners of the interactive objects): KitchenBuilder skips them. */
	ANTKITCHEN_API void FixtureRows(TSet<int32>& Out);
	/** Is this base row removed or moved by some district variant? (Those are built movable.) */
	ANTKITCHEN_API bool IsVariantRow(int32 Inst);
	/** Base rows that belong to fixture F (brackets, lids...). */
	ANTKITCHEN_API void FixtureParts(int32 F, TArray<int32>& Out);
	ANTKITCHEN_API TConstArrayView<FLevelV3Seed> Seeds();
	ANTKITCHEN_API TConstArrayView<FLevelV3Scatter> Scatter();
	ANTKITCHEN_API const TCHAR* ScatterName(int32 i);

	// ---- the gameplay tables of v3 (LevelV2 hands these out when v3 is on)
	ANTKITCHEN_API TConstArrayView<FLevelV2Spot> LootSpots();      // with the current seed's loot_z
	ANTKITCHEN_API TConstArrayView<FVector3f> HeavySpots();
	ANTKITCHEN_API TConstArrayView<FLevelV2Main> MainSpots();
	ANTKITCHEN_API TConstArrayView<FVector2f> PatrolStops();
	ANTKITCHEN_API TConstArrayView<FLevelV2Box> CoverBoxes();      // base + the seed's hide - hide_off
	ANTKITCHEN_API TConstArrayView<FLevelV2Box> TargetBounds();
	ANTKITCHEN_API TConstArrayView<FLevelV2Zone> Zones();          // base + the seed's wet / sticky hazards (+ broken jars, spills: AddZone)
	ANTKITCHEN_API TConstArrayView<FLevelV2District> Districts();
	ANTKITCHEN_API TConstArrayView<FLevelV2Callout> Callouts();
	ANTKITCHEN_API FBox2D WanderBox(int32 Room);

	// ---- the seed
	/** The validated entry for (Seed, Tier), or the nearest validated seed of that tier above it (wrapping), or null if the tier has none. */
	ANTKITCHEN_API const FLevelV3Seed* FindSeed(int32 Seed, int32 Tier);
	/** Server: a random validated seed of the tier. */
	ANTKITCHEN_API const FLevelV3Seed* PickSeed(int32 Tier, FRandomStream& Rng);
	/** Every machine: this is the layout now (recomputes cover, zones, loot heights). Null: the default (all variants A, nothing loose). */
	ANTKITCHEN_API void SetCurrent(const FLevelV3Seed* S);
	ANTKITCHEN_API const FLevelV3Seed* Current();
	/** The variant (index into Variants()) the current seed uses in district D. */
	ANTKITCHEN_API int32 CurrentVariant(int32 D);
	/** The variant letter of district D in the current seed ('A' without a seed). */
	ANTKITCHEN_API TCHAR CurrentLetter(int32 D);
	ANTKITCHEN_API bool IsHome(int32 Who);
	ANTKITCHEN_API int32 TimeOfDay();
	ANTKITCHEN_API const TCHAR* TimeName(int32 T);
	/** Is this base row loose in the current seed (unscrews twice as fast)? */
	ANTKITCHEN_API bool IsLoose(int32 Inst);
	/** Is a conn (car_track, drain, pipe, towel, hose...) switched off by the current variants? */
	ANTKITCHEN_API bool IsConnOff(const TCHAR* Id);
	/** Is a hazard (sprinkler, ...) switched off, or one added (burner_on, tv_on, leak, rain)? */
	ANTKITCHEN_API bool IsHazardOff(const TCHAR* Id);
	ANTKITCHEN_API bool IsHazardOn(const TCHAR* Id);
	/** The dog's bowl spot when a variant moved it, else false. */
	ANTKITCHEN_API bool DogBowl(FVector2D& Out);
	/** "ANTSEED seed=7 tier=1 time=day home=gran,dog,cat variants=PLKM... cake=M2 doors=4,9 loose=v3_rod_sc1" */
	ANTKITCHEN_API FString Describe();
	/** For the HUD: "Сид 7 · день · дома: бабушка, Бадрик, кошка". */
	ANTKITCHEN_API FString ShortText();
	/** -AntVariant=plato:D,porog:B (dev): forces variants on top of the seed (an unvalidated layout: dev only, logged as such). */
	ANTKITCHEN_API void ApplyDevVariants();

	/** Every machine: a puddle, a smear or shards made in play become a zone (0 sticky, 1 wet, 2 shards). */
	ANTKITCHEN_API void AddZone(int32 Kind, const FVector& Center, float Radius);
	ANTKITCHEN_API void ClearPlayZones();
	/** Shards under P (the cutting zone of broken glass), else false. */
	ANTKITCHEN_API bool ShardsAt(const FVector& P);
	/** What the shards do to an ant standing in them, hp a second (the design: 10). */
	inline float ShardDps() { return 10.f; }
}
