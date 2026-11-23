#pragma once
// Map v4 look (docs/level/V4_ART.md): every surface of the flat through one triplanar PBR material (/Game/Map4/Materials/M_M4)
// with real texture sets (/Game/Map4/Textures/T_M4_<Set>_{C,N,R}); the game's own base shapes (/Game/Map4/Props/SM_M4_*)
// instead of /Engine/BasicShapes; generated props (/Game/Map4/Props/<Name>). Off with -AntMap3 (the v3 look).
#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMeshComponent;

namespace Map4Look
{
	struct FLook
	{
		const TCHAR* Key;
		const TCHAR* Set;        // T_M4_<Set>_{C,N,R}
		float Tile;              // uu per texture repeat
		float R, G, B;           // tint
		float RoughMul, RoughAdd, Metal;
		float NStr;              // normal strength
		float Dust, Grime;       // dust on tops, grime at the foot of walls
		float Glow;              // emissive = colour * Glow
		const TCHAR* Step;       // footstep tag or null
	};

	ANTKITCHEN_API bool Enabled();
	/** Shared material of a look (rooted, cached). Color multiplies the tint. */
	ANTKITCHEN_API UMaterialInterface* Get(const FLook& L, const FLinearColor& Color = FLinearColor::White);
	/** A component's own instance of a look (the caller drives "Color" / "Glow"). */
	ANTKITCHEN_API UMaterialInstanceDynamic* Own(const FLook& L, UObject* Outer, const FLinearColor& Color = FLinearColor::White);
	/** The look of a material slot of a modelled piece ("Mesh#Index/Slot", "Mesh/Slot", "Slot"), or null. */
	ANTKITCHEN_API const FLook* ForSlot(const TCHAR* Mesh, int32 Index, const FString& Slot);
	/** The look of a builder surface by its palette colour and where it is (rooms differ). */
	/** Which room a wall box belongs to by its centre: 1 kitchen, 2 living room, 0 outside (its faces elsewhere get skins). */
	ANTKITCHEN_API int32 WallRoom(const FVector& Center);
	/** Room of a point (0 outside); Grow widens the rooms. */
	ANTKITCHEN_API int32 RoomAt(const FVector& P, float Grow);
	ANTKITCHEN_API const FLook* ForColor(const FLinearColor& Color, const FVector& Where, FLinearColor& OutTint);
	/** Painted (plastic / enamel) look with a "Color" parameter: AntKit::Paint's replacement. */
	ANTKITCHEN_API UMaterialInstanceDynamic* Paint(UObject* Outer, const FLinearColor& Color);
	/** Named look (e.g. "Oak", "Brushed"), or null. */
	ANTKITCHEN_API const FLook* Named(const TCHAR* Key);
	/** The base shapes: 0 box, 1 ball, 2 cylinder, 3 cone, 4 plane, 5 droplet, 6 speck, 7 crumb, 8 pheromone, 9 pebble. */
	ANTKITCHEN_API UStaticMesh* Shape(int32 Index);
	/** A prop under /Game/Map4/Props/<Name>/<Name> (rooted, cached), or null. */
	ANTKITCHEN_API UStaticMesh* Prop(const TCHAR* Name);
	/** Gives each material slot of the component's mesh its look (slot names = look keys). */
	ANTKITCHEN_API void ApplySlots(UStaticMeshComponent* C);
	/** Puts prop Name on C (keeps its transform); Look (optional) paints slot 0; Tag marks it for the art audit. False if the prop is missing. */
	/** Plain query boxes round the thin things on walls (frames, calendar, drawing, outlets): ants hold on to them. */
	ANTKITCHEN_API int32 BoxWallFlats(class UWorld* W);
	ANTKITCHEN_API bool Swap(UStaticMeshComponent* C, const TCHAR* Name, const TCHAR* Look = nullptr, const TCHAR* Tag = nullptr);
	/** A new component with prop Name under Parent at a relative transform (solid: blocks like the level, ants climb it). */
	ANTKITCHEN_API UStaticMeshComponent* AddProp(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FTransform& Relative, bool bSolid, const TCHAR* Look = nullptr);
}
