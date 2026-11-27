#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UWorld;

/**
 * The modelled kitchen (Blender set in /Game/Kitchen, later /Game/Living): where every piece stands, what it looks like,
 * how it collides. The numbers come from the art manifest through Tools/gen_kitchen_art.py (KitchenArtData.inl).
 * -AntOldKitchen on the command line keeps the old box-built kitchen instead.
 */
struct FKitchenArtInst
{
	const TCHAR* Name;       // mesh: /Game/Kitchen/<Name>/<Name>
	int32 Index;             // which placement of that mesh
	int32 Role;              // KitchenArt::ERole
	int32 Parent;            // riders: index into KitchenArt::Movers()
	FVector3f Loc;           // game location of the mesh origin
	FVector3f AX, AY, AZ;    // where the mesh-local axes point in the game
	FVector3f Scale;         // on top of the import scale
};

struct FKitchenArtMover
{
	int32 Inst;              // index into KitchenArt::Instances()
	int32 Type;              // 0 hinge, 1 slide
	FVector3f Axis;          // hinge axis, game space
	float Angle;             // open angle, degrees
	FVector3f Slide;         // open offset, game space
	int32 Flags;             // KitchenArt::EMoverFlags
	float AutoClose;         // seconds, 0 = stays open
	const TCHAR* OpenText;
	const TCHAR* CloseText;
};

namespace KitchenArt
{
	enum ERole { Static = 0, Mover = 1, Gadget = 2, Rider = 3 };
	enum EMoverFlags { StartsOpen = 1, NeedsFridge = 2, NeedsDishwasher = 4, HumanShuts = 8, HumanOpens = 16, BigOpens = 32 };
	/** None: decoration. Static: part of the fixed kitchen. Moving: a door, a drawer, a lid - ants ride it, food rests on it. */
	enum class ESolid : uint8 { None, Static, Moving };

	/** FBX metres -> game units (1 m = 1900 uu, the importer gives 100). */
	constexpr float ImportScale = 19.f;

	/** Is the modelled kitchen in use? False with -AntOldKitchen, or when the meshes are missing from the build. */
	ANTKITCHEN_API bool Enabled();

	ANTKITCHEN_API TConstArrayView<FKitchenArtInst> Instances();
	ANTKITCHEN_API TConstArrayView<FKitchenArtMover> Movers();
	ANTKITCHEN_API const FKitchenArtInst* Find(const TCHAR* Name, int32 Index = 0);
	/** Where the manifest puts this instance (rotation, location, full scale). */
	ANTKITCHEN_API FTransform WorldTransform(const FKitchenArtInst& I);

	/** Hero props in use (default)? -AntHeroArt=0 brings back the old models: Mesh() then skips /Game/Hero/<Name>/<Name>. */
	ANTKITCHEN_API bool HeroArt();

	ANTKITCHEN_API UStaticMesh* Mesh(const TCHAR* Name);

	/** The game's own materials on the mesh, one shared material per slot name (see the look table in KitchenArt.cpp). */
	ANTKITCHEN_API void ApplyLook(UStaticMeshComponent* C, const TCHAR* Name, int32 Index = 0);
	/** Level v3: an instance tint (the sheets on the line): the look "Tint/<Tint>" on the first slot. */
	ANTKITCHEN_API void ApplyTint(UStaticMeshComponent* C, const TCHAR* Tint);

	/** Adds a piece under Parent with a relative transform. StableName: set for parts ants ride over the network. */
	ANTKITCHEN_API UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, int32 Index, const FTransform& Relative,
		ESolid Solid, bool bStaticMobility = false, FName StableName = NAME_None);

	/** Adds instance Index of the piece exactly where the manifest puts it, attached to Parent (wherever Parent is right now). */
	ANTKITCHEN_API UStaticMeshComponent* AddAt(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, int32 Index, ESolid Solid,
		bool bStaticMobility = false, FName StableName = NAME_None);

	/** Dev (-AntArtProbe): logs what lies above and below every loot spot, to catch spots buried inside a model. */
	ANTKITCHEN_API void ProbeSpots(UWorld* World, const TArray<FVector>& Spots);

	/** Dev (-AntArtTour): logs where an ant ended up after being put at Target (did it stand, fall through, get pushed out?). */
	ANTKITCHEN_API void ProbeAnt(const class ACharacter* Ant, int32 Step, const FVector& Target);

	/** Collision of a piece: Static = fixed kitchen, Moving = rideable. */
	ANTKITCHEN_API void SetSolid(UStaticMeshComponent* C, ESolid Solid);

	/**
	 * KAN-40. The gadget code is written in the kitchen's coordinates. In a flat (AntLab) every art group of the kitchen stands
	 * somewhere else, turned and scaled per axis: a home takes a kitchen point of that group to where it is in this world, and
	 * back. In the kitchen it changes nothing.
	 */
	struct FHome
	{
		bool bMoved = false;
		FTransform K, L;   // the group's box in the kitchen (a plain offset) and in this world (offset, yaw, scale)

		FVector At(const FVector& KitchenPoint) const { return bMoved ? L.TransformPosition(K.InverseTransformPosition(KitchenPoint)) : KitchenPoint; }
		FVector ToKitchen(const FVector& WorldPoint) const { return bMoved ? K.TransformPosition(L.InverseTransformPosition(WorldPoint)) : WorldPoint; }
		FVector Dir(const FVector& KitchenDir) const { return bMoved ? L.GetRotation().RotateVector(KitchenDir) : KitchenDir; }
		FRotator Rot(const FRotator& KitchenRot = FRotator::ZeroRotator) const { return bMoved ? (L.GetRotation() * KitchenRot.Quaternion()).Rotator() : KitchenRot; }
		/** The axis-aligned box round a kitchen box. */
		ANTKITCHEN_API FBox Box(const FBox& KitchenBox) const;
		/** The place of an actor that stands at a kitchen point: turned and SCALED with the group, so whatever the actor builds at
		 *  its own offsets (pivots, use points, lights) lands where the home takes those kitchen points. Tm(0) is the whole mapping. */
		ANTKITCHEN_API FTransform Tm(const FVector& KitchenLoc, const FRotator& KitchenRot = FRotator::ZeroRotator) const;
		/** One number for radii and reaches. */
		float Size() const { return bMoved ? (float)((L.GetScale3D().X + L.GetScale3D().Y + L.GetScale3D().Z) / 3.) : 1.f; }
	};
	ANTKITCHEN_API FHome Home(const TCHAR* Group);
	/** Does this world have that group of the kitchen? The kitchen: always. A flat: what was placed there and has its models. */
	ANTKITCHEN_API bool Has(const TCHAR* Group);
}