#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "AntFx.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** How a particle is drawn. All three use engine content that is always cooked (checked in a packaged build). */
enum class EAntFxLook : uint8
{
	Solid,   // lit opaque basic-shape material: grit, crumbs
	Soft,    // unlit translucent (/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent): dust, steam, spray, water
	Glow,    // additive emissive (/Engine/EngineMaterials/EmissiveMeshMaterial): sparks, beams
	// KAN-21: camera-facing cards of the project's own procedural materials (Tools/map4/make_m4_materials.py), on SM_M4_Plane
	Flame,   // /Game/Map4/Materials/M_M4_FxFlame: animated tongues of flame, additive; stays upright. Color RGB = brightness, A = fade
	Smoke,   // /Game/Map4/Materials/M_M4_FxSmoke: a soft billowing puff (smoke, flour dust); faces the camera. A = peak opacity
};
constexpr int32 AntFxLooks = 5;

/** One cosmetic particle: a basic shape that flies, grows and fades. Sizes are scales of a 100-unit shape. */
struct FAntFxBit
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	FVector Scale0 = FVector(0.1f);
	FVector Scale1 = FVector(0.1f);
	FLinearColor Color = FLinearColor::White;   // Soft: A is the peak opacity. Glow: RGB may go above 1.
	FQuat Rot = FQuat::Identity;
	float Life = 1.f;
	float Gravity = 0.f;       // units/s2, down
	float Drag = 0.f;          // 1/s
	float FadeIn = 0.08f;      // part of the life spent appearing
	float FadePow = 1.f;       // > 1 fades early, < 1 stays and then drops
	float FloorZ = -1.e9f;     // comes to rest here
	float MinAmbient = 0.f;    // Soft only: never dimmed below this by a dark room (hazards must stay readable at night)
	EAntShape Shape = EAntShape::Sphere;
	EAntFxLook Look = EAntFxLook::Soft;
	bool bStreak = false;      // keeps its X axis along the velocity (sparks)
	float Seed = -1.f;         // Flame / Smoke: the pattern of the card (< 0: a random one)
	bool bKeep = false;        // a one-off that must be seen (a bang's cloud): a full pool gives up other bits before it
};

/**
 * Cosmetic only, one per world, spawned locally on every machine that draws (never on a dedicated server, never replicated).
 * A pool of basic-shape components animated in Tick: the project has no Niagara assets, so dust, steam, sparks and
 * splashes are done the way AFoodCrumbs and AAntGibs are. The pool is capped; when it is full the oldest bit is reused.
 */
UCLASS()
class ANTKITCHEN_API AAntFxManager : public AActor
{
	GENERATED_BODY()

public:
	AAntFxManager();
	virtual void Tick(float DeltaSeconds) override;

	/** The manager of this world (made on first use). nullptr on a dedicated server. */
	static AAntFxManager* Get(const UObject* WorldContext);
	void Emit(const FAntFxBit& Bit);
	int32 NumLive() const { return NumOn; }

	/** How bright the room is (time of day, light switch): unlit dust and steam are dimmed and tinted by it. */
	float Ambient = 1.f;
	FLinearColor AmbientTint = FLinearColor::White;

	static constexpr int32 MaxBits = 600;   // (KAN-21 run 3: 420 -> 600: a room of fires and their smoke clouds kept it full)

private:
	struct FLive
	{
		FAntFxBit B;
		float Age = 0.f;
		bool bOn = false;
	};
	void Apply(int32 Slot);
	UMaterialInterface* BaseFor(EAntFxLook Look);

	UPROPERTY() TArray<UStaticMeshComponent*> Comps;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> Mids;   // AntFxLooks per component (one per look), made on demand
	UPROPERTY() UMaterialInterface* SolidBase = nullptr;
	UPROPERTY() UMaterialInterface* SoftBase = nullptr;
	UPROPERTY() UMaterialInterface* GlowBase = nullptr;
	UPROPERTY() UMaterialInterface* FlameBase = nullptr;
	UPROPERTY() UMaterialInterface* SmokeBase = nullptr;
	FVector CamLoc = FVector::ZeroVector;   // this frame's camera: the cards face it
	TArray<FLive> Live;
	int32 Cursor = 0;
	int32 NumOn = 0;
};

/** The effects themselves. Local and cosmetic: call them on every machine that should see them. */
namespace AntFx
{
	/** The Human's slipper hits the floor: a ring of dust rolling outwards and grit jumping up. */
	ANTKITCHEN_API void StompDust(const UObject* Ctx, const FVector& At);
	/** Droplets jumping out of water plus a ripple. Size = droplet size in units (an ant: ~5, the faucet: ~16). */
	ANTKITCHEN_API void Splash(const UObject* Ctx, const FVector& At, float Size);
	ANTKITCHEN_API void Droplets(const UObject* Ctx, const FVector& At, int32 Count, float Size);
	/** A ring spreading on a water surface. */
	ANTKITCHEN_API void Ripple(const UObject* Ctx, const FVector& At, float Radius);
	ANTKITCHEN_API void Sparks(const UObject* Ctx, const FVector& At, int32 Count, float Size);
	/** One puff of steam rising from At. */
	ANTKITCHEN_API void Steam(const UObject* Ctx, const FVector& At, float Size, float Rise);
	/** One puff of bug spray: shot along Dir, slows down and hangs in the air as a drifting cloud. */
	ANTKITCHEN_API void SprayPuff(const UObject* Ctx, const FVector& From, const FVector& Dir, float Range);
	/** Food reached the nest: a beam, a ring on the ground and sparkles. */
	ANTKITCHEN_API void DeliverPulse(const UObject* Ctx, const FVector& At, const FLinearColor& Color);
	/** An ant lands: a little ring of dust along the surface. Size = the ant's body length. */
	ANTKITCHEN_API void LandDust(const UObject* Ctx, const FVector& At, const FVector& Normal, float Size);
	/** Heavy food scraped along the floor: a few crumbs and a puff. Size = the food's radius. */
	ANTKITCHEN_API void DragCrumbs(const UObject* Ctx, const FVector& At, const FLinearColor& FoodColor, float Size);
	/** A lamp bulb / glowing bit: makes a basic-shape component additive-emissive. */
	ANTKITCHEN_API UMaterialInstanceDynamic* MakeGlow(UStaticMeshComponent* Comp, const FLinearColor& Color);
	ANTKITCHEN_API void SetAmbient(const UObject* Ctx, float Brightness, const FLinearColor& Tint);
	/** A card (SM_M4_Plane, its face +Z) at Pos turned to the camera: upright (flames: only around the vertical) or fully facing. */
	ANTKITCHEN_API FQuat CardRot(const FVector& Pos, const FVector& Cam, bool bUpright);
	/** The card mesh and the materials of the Flame / Smoke looks (nullptr if the project's assets are missing). */
	ANTKITCHEN_API UStaticMesh* CardMesh();
	ANTKITCHEN_API UMaterialInterface* CardMaterial(EAntFxLook Look);
	/** Dev (-AntFxTest): every effect once, laid out in front of the viewer. */
	ANTKITCHEN_API void TestAll(const UObject* Ctx, const FVector& ViewLoc, const FRotator& ViewRot);
}