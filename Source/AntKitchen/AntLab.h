#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "AntLab.generated.h"

class AAntKitchenGameMode;
class AKitchenBuilder;
struct FAntEnv;
class AAntCharacter;

/**
 * KAN-37 «Лаборатория уровней»: grey-box floor plans as data (docs/level/lab/lab_<X>.json, made by Tools/levellab/build_lab.py,
 * embedded into the game as AntLabData.inl). When a lab variant is active in this world, the flat is built from its boxes
 * instead of the kitchen, and the layout questions the game asks (where the nest is, where the Human starts and walks, where
 * the food and the cake lie, which room a point is in, the route graph) are answered from the same data. Otherwise nothing
 * changes: every hook is "if (const FAntLabVariant* V = AntLab::Get()) ...".
 *
 * Which variant: -AntLab=A on the command line, or the menu (main menu / co-op screen: «Лаборатория уровней»). The host's
 * choice travels to the clients in AAntGameState::LabVariant. Process-wide: one world plays at a time.
 * Units: the JSON is in metres from the flat's inner south-west corner; everything below is already in uu (world).
 */
struct FAntLabBox { FVector C; FVector S; FRotator R; FLinearColor Col; FString Kind; FString Room; FString Tag; bool bClimb = false; bool bCollide = true; };
struct FAntLabZone { FString Kind; FBox Box; float Temp = 0.f; float Rate = 0.f; FString Note; };
struct FAntLabHatchDef { FString Id; FString Name; FVector C; FVector S; int32 Class = 1; TArray<FVector> Use; FString Note; };
struct FAntLabRoom { FString Id; FString Name; FBox2D Rect; FString Hero; FString Physics; float Area = 0.f; };
struct FAntLabFood { FVector At; int32 Tier = 0; FString Node; FString Room; bool bHeavy = false; FString What; };
struct FAntLabNode { FString Id; FVector At; FString Name; FString Room; bool bHole = false; };
struct FAntLabEdge { int32 A = 0, B = 0; FString Kind; uint8 RouteKind = 0; float Run = 0.f, Seen = 0.f; FString Gate; };

struct FAntLabVariant
{
	FString Id, Name, Note;
	float M = 1900.f;
	FVector Origin = FVector::ZeroVector;
	FBox2D Bounds;           // world, the rooms' union
	float Ceiling = 0.f;
	float SprintMps = 0.24f;
	TArray<FAntLabRoom> Rooms;
	TArray<FAntLabBox> Boxes;
	TArray<FAntLabZone> Zones;
	TArray<FAntLabHatchDef> Hatches;
	TArray<FBox> Cover;
	TArray<FString> CoverNames;
	FVector Nest, Nest2, Spawn;
	float NestRadius = 570.f, SpawnRadius = 400.f;
	TArray<FAntLabFood> Food;
	FVector Cake;
	FString CakeNode, CakeWhere;
	FVector2D HumanSpawn;
	TArray<FVector2D> Patrol;
	TArray<FAntLabNode> Nodes;
	TArray<FAntLabEdge> Edges;

	/** Metres (flat) -> world uu. */
	FVector W(const FVector& Metres) const { return Origin + Metres * M; }
	int32 NodeIndex(const FString& NodeId) const;
};

namespace AntLab
{
	/** The variant the next world should play ('\0' none). -AntLab=X sets it at start-up; the menu changes it. */
	ANTKITCHEN_API TCHAR Wanted();
	ANTKITCHEN_API void SetWanted(TCHAR Id);
	/** The letters there is data for (A..). */
	ANTKITCHEN_API FString Available();
	ANTKITCHEN_API FString VariantName(TCHAR Id);
	/** Make this variant the one of the world being started (every machine; '\0' switches the lab off). Logs what it loaded. */
	ANTKITCHEN_API bool Activate(TCHAR Id);
	/** The active variant, or null (the normal kitchen). */
	ANTKITCHEN_API const FAntLabVariant* Get();
	inline bool On() { return Get() != nullptr; }

	// ---- the layout questions (world uu)
	ANTKITCHEN_API int32 RoomIndexAt(const FVector& P, float Grow = 0.f);
	ANTKITCHEN_API bool InHouse(const FVector& P);
	ANTKITCHEN_API FVector HomeFor(const FVector& From);
	ANTKITCHEN_API FVector SpawnPoint(FRandomStream* Rng = nullptr);
	ANTKITCHEN_API FVector RandomFloorPoint(FRandomStream* Rng = nullptr);
	ANTKITCHEN_API FBox PlayArea();
	/** Next point on the way From -> To over the route graph (walkers: floor edges first). */
	ANTKITCHEN_API FVector Route(const FVector& From, const FVector& To);
	ANTKITCHEN_API void EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env);
	ANTKITCHEN_API bool GateOpen(const FString& HatchId);
	ANTKITCHEN_API void SetGate(const FString& HatchId, bool bOpen);

	/** Server, after the game mode's BeginPlay: the hatches (replicated). */
	ANTKITCHEN_API void SpawnHatches(UWorld* World);
	/** Dev (-AntDo): lab info | lab check | lab open <id|all> | lab shut <id|all> | lab tp <node|food N|cake|nest> | lab ceiling 0|1 | lab deliver */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** The automatic check (also "lab check"): nest -> first food per room, ways to the cake, unreachable food, corridor blocked. */
	ANTKITCHEN_API void LogCheck();
}

/** A shortcut in a wall that starts shut (the bathroom vent grille, the riser hatch, the skirting board): pull with [E]. */
UCLASS()
class ANTKITCHEN_API AAntLabHatch : public AKitchenInteractable
{
	GENERATED_BODY()
public:
	AAntLabHatch();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override {}
	virtual int32 DoorClass() const override;
	virtual bool IsDoorOpen() const override { return bOpen; }
	virtual void SetDoorOpen(bool bInOpen) override;
	virtual FString DoorNoun() const override;

	UPROPERTY(ReplicatedUsing = OnRep_Open) bool bOpen = false;
	UPROPERTY(Replicated) uint8 HatchIndex = 0;
	UFUNCTION() void OnRep_Open();

private:
	void Apply();
	/** The plate, once this machine knows the variant (a client may get the hatch before the game state). */
	void EnsurePlate();
	UPROPERTY() UStaticMeshComponent* Plate = nullptr;
};
