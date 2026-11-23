#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Camera/CameraModifier.h"
#include "KitchenInteractables.h"
#include "AntInside.generated.h"

class AAntCharacter;
class AAntKitchenGameMode;
class AHumanCharacter;
class AHUD;
class UCanvas;
class UStaticMeshComponent;
class UPointLightComponent;
class ACameraActor;

/**
 * Inside people (docs/FEATURES.md, section 33; LEVEL_V3.md section 6 «Внутрь людей»). An ant at an entry point of a resident's body
 * (the Dad's ear, nose, trouser leg, pocket, slipper; grandma's ear, bun, cardigan pocket, slipper; the kid's pocket and collar; the dog's
 * collar) presses [E] and is inside: pinned to that segment (it rides every move of the body), hidden from the world and from the AI,
 * its own camera in a little dark diorama of that place (the ear canal with its wax, the hairy nostril, the pocket with lint and a coin,
 * the striped tunnel of the pyjama leg...). From inside it bites; the resident reacts (shakes the head, slaps the ear, sneezes it across
 * the room, hops, stumbles and falls when three are in the leg, kicks the slipper off, pats the pocket, drops the knitting, giggles and
 * runs). Time inside is short; the resident's reaction throws it out (a launch, a tumble). A player Human (versus) feels it: the screen
 * shakes, the eyes water, the ear is muffled, and [F] swats at himself (it costs patience).
 *
 * Server-authoritative: AAntInsideNet replicates who is inside whom and the residents' little body effects (a head shake, the sneeze);
 * the ant's place is the existing NpcHold (replicated); every machine poses the effects after the body is posed.
 */
namespace AntInsideSpot
{
	enum EId : uint8 { Ear, Nose, Leg, Pocket, Slipper, Bun, Collar, Count };
}

/** A resident's body effect every machine plays on top of the pose (the server starts them). */
namespace AntInsideFx
{
	enum EId : uint8 { HeadShake, Tickle, Sneeze, EarSlap, LegDance, BunFix, Giggle, EyesWater, Spin, SlipperFly, Count };
}

USTRUCT()
struct FAntInsideSlot
{
	GENERATED_BODY()
	UPROPERTY() AAntCharacter* Ant = nullptr;
	UPROPERTY() AActor* Host = nullptr;
	UPROPERTY() uint8 Def = 0;
	UPROPERTY() float Since = 0.f;
	/** Server time the resident's reaction lands (the slap, the sneeze, the kick); < 0: none coming. */
	UPROPERTY() float ReactAt = -1.f;
	UPROPERTY() uint8 React = 0;
	/** What it found inside (the pocket): 0 nothing, else a loot kind the ant takes out with it. */
	UPROPERTY() uint8 Loot = 0;
	UPROPERTY() uint8 Bites = 0;
};

USTRUCT()
struct FAntInsideFxState
{
	GENERATED_BODY()
	UPROPERTY() AActor* Host = nullptr;
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() float Start = 0.f;
	UPROPERTY() float Dur = 1.f;
};

UCLASS()
class ANTKITCHEN_API AAntInsideNet : public AInfo
{
	GENERATED_BODY()

public:
	AAntInsideNet();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AAntInsideNet* Get(const UWorld* World, bool bCreate = false);

	UPROPERTY(Replicated) TArray<FAntInsideSlot> Slots;
	UPROPERTY(Replicated) TArray<FAntInsideFxState> Fx;

	// ---- server
	bool Enter(AAntCharacter* Ant, AActor* Host, uint8 Def, bool bForce);
	/** Out: Mode 0 crawls out by itself, 1 thrown out with Vel (a launch, a tumble), 2 caught (kept by whoever caught it). */
	void Exit(int32 Slot, uint8 Mode, const FVector& Vel, float Damage, const TCHAR* Why);
	void StartFx(AActor* Host, uint8 Kind, float Dur);
	void OnBite(int32 Slot);
	/** The Human (a player in versus) swats at himself: [F]. True if somebody was inside him. */
	bool SelfSwat(class AHumanCharacter* H);
	int32 SlotOf(const AAntCharacter* Ant) const;
	/** Server: Host sneezes now (pepper in the air, AntReact.h): the sneeze of the nose, with whatever is in the nose shot out. */
	void ForceSneeze(AActor* Host);
	int32 CountIn(const AActor* Host, uint8 Spot) const;
	float NetNow() const;

	// ---- every machine
	float FxWeight(const AActor* Host, uint8 Kind, float* OutT = nullptr) const;

private:
	void ServerTick(float Dt);
	void ServerSlot(int32 i, float Now);
	void React(int32 i, float Now);
	void PostPose(float Dt);
	void LocalView(float Dt);
	void BuildRoom(uint8 Spot, uint8 Who);
	void FlyingSlipper(const FVector& From, const FVector& Vel);
	float LocalClock = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> NextShake;
	TMap<TWeakObjectPtr<AActor>, float> NextBun;
	TMap<TWeakObjectPtr<AActor>, int32> LootTaken;
	TMap<TWeakObjectPtr<AActor>, float> LegBites;
	TSet<TWeakObjectPtr<AActor>> Spoke;
	// local: the diorama the local inside ant looks at
	UPROPERTY() USceneComponent* RoomRoot = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> RoomParts;
	UPROPERTY() TArray<UStaticMeshComponent*> Swaying;
	TArray<FRotator> SwayBase;
	TWeakObjectPtr<UCameraModifier> CamMod;
	int32 LastBites = 0;
	float BeatClock = 0.f;
	float DuckClock = 0.f;
	TSet<float> SlippersShown;
	UPROPERTY() UPointLightComponent* RoomLight = nullptr;
	UPROPERTY() UPointLightComponent* RoomLight2 = nullptr;
	UPROPERTY() UStaticMeshComponent* PuppetJaw = nullptr;
	UPROPERTY() TArray<USceneComponent*> PuppetAntennae;
	UPROPERTY() ACameraActor* RoomCam = nullptr;
	int32 RoomBuilt = -1;
	FVector RoomAt = FVector::ZeroVector;
	bool bViewing = false;
	TWeakObjectPtr<AActor> ViewHost;
	float ViewSince = 0.f;
	float BiteFlash = 0.f;
	TWeakObjectPtr<AAntCharacter> HiddenLocal;
	TSet<TWeakObjectPtr<AAntCharacter>> Hidden;
	struct FFlying { TWeakObjectPtr<UStaticMeshComponent> C; FVector V; float T; };
	TArray<FFlying> Flying;
	float NextHumanHint = 0.f;
};

/** An entry point on a resident: [E] here and the ant is inside (replicated; the place follows the body on every machine). */
UCLASS()
class ANTKITCHEN_API AAntInsideSpot : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AAntInsideSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual float DistanceTo(const FVector& P) const override;

	UPROPERTY(Replicated) AActor* Host = nullptr;
	UPROPERTY(Replicated) uint8 Def = 0;
};

/** The local player's view while something is inside (the ant in its diorama; the Human with an ant in him): shake, blur, vignette. */
UCLASS()
class ANTKITCHEN_API UAntInsideCamMod : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, struct FPostProcessSettings& PostProcessSettings) override;
	static float Shake;      // 0..1
	static float Blur;       // 0..1 (the eyes water)
	static float Dark;       // 0..1 (inside: the vignette)
	float Clock = 0.f;
};

namespace AntInside
{
	struct FSpotDef
	{
		uint8 Spot;
		uint8 Who;          // NpcRoutine::EWho: Dad, Gran, Kid, Dog
		int32 Seg;          // HumanSeg / QuadSeg
		FVector Uvw;        // in the segment mesh's local bounds, -1..1 on each axis
		float Radius;       // how close the ant must be
		const TCHAR* Ru;    // «в ухо»
		const TCHAR* Place; // «ухо»
		float MaxTime;
		uint8 Cap;
	};
	ANTKITCHEN_API TConstArrayView<FSpotDef> Defs();
	/** -AntNoInside: no entry points. */
	ANTKITCHEN_API bool Enabled();
	/** Server, AKitchenFixture::ResetAll: everybody out, the entry points on the residents again (a moment later). */
	ANTKITCHEN_API void ResetAll(UWorld* World);
	/** Where an entry point is now (world), or false. */
	ANTKITCHEN_API bool SpotPoint(const AActor* Host, uint8 Def, FVector& Out, USceneComponent** OutComp = nullptr, FVector* OutLocal = nullptr);
	/** Server, from the ant's [E] while inside: crawl out. True = handled. */
	ANTKITCHEN_API bool OnInteract(AAntCharacter* Ant);
	/** Server, from the ant's bite while inside: bite the resident from within. True = handled. */
	ANTKITCHEN_API bool OnBite(AAntCharacter* Ant);
	/** Server, [Space] while inside: wriggle out. True = handled. */
	ANTKITCHEN_API bool OnStruggle(AAntCharacter* Ant);
	ANTKITCHEN_API bool IsInside(const AAntCharacter* Ant);
	/** The AI does not see or hit an ant inside a body (HumanCharacter::InCover). */
	ANTKITCHEN_API bool HiddenAt(const UWorld* World, const FVector& P);
	/** An ant in this resident's ear: he does not hear (AAntKitchenGameMode::BestNoise from his place). */
	ANTKITCHEN_API bool DeafAt(const UWorld* World, const FVector& From);
	/** Server, AHumanCharacter::TryShakeOff: [F] with ants inside him swats at them. True = handled. */
	ANTKITCHEN_API bool HumanSelfSwat(AHumanCharacter* H);
	/** AAntHUD::DrawHUD: the inside ant's overlay, the Human player's hints. */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC);
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
