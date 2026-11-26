#include "KitchenArt.h"
#include "AntMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AntSwing.h"

#include "KitchenArtData.inl"
#include "LevelV3.h"
#include "Map4Look.h"

// ---------------------------------------------------------------------------------------------
// The look table: material slot name -> one of the game's own surfaces.
// Textured looks use /Game/Textures/M_Surface (the triplanar material of the whole procedural kitchen) with the
// textures the game already has; "Flat" is the same material in its plain-paint mode with the colour, roughness and
// metallic from the art manifest. A key "Mesh/Slot" or "Mesh#Instance/Slot" overrides the plain slot name.
// ---------------------------------------------------------------------------------------------

namespace
{
	enum ELookKind { Textured = 0, Speckle = 3, Flat = 4, Glow = 10, Driven = 11 };   // Speckle: the material's stone mode (concrete, asphalt)

	struct FLook
	{
		const TCHAR* Key;
		int32 Kind;
		const TCHAR* Tex;      // /Game/Textures/<Tex>
		float Size;            // texture tile size, uu
		float R, G, B;         // tint (textured) or the colour itself (flat, glow)
		float Rough;
		float Metal;
		const TCHAR* Step;     // footstep tag (AntSfx::StepFor), or null: hard surface
	};

	const TCHAR* const Wood = TEXT("Surf_Wood");
	const TCHAR* const Metal = TEXT("Surf_Metal");
	const TCHAR* const Cloth = TEXT("Surf_Fabric");

	const FLook GLooks[] =
	{
		// ---- overrides
		{ TEXT("SM_K_Rug/Red"),           Textured, TEXT("T_Rug_Red"),       900.f,  1.f, 1.f, 1.f,          0.95f, 0.f, Cloth },
		{ TEXT("SM_K_Rug/Paper"),         Textured, TEXT("T_Fabric_Grey"),   300.f,  2.0f, 1.85f, 1.5f,      0.95f, 0.f, Cloth },
		{ TEXT("SM_K_Shaker#1/Enamel"),   Flat,     nullptr,                 0.f,    0.07f, 0.06f, 0.05f,    0.4f,  0.f, nullptr },   // the pepper
		{ TEXT("SM_K_PetBowl#1/Red"),     Textured, TEXT("T_Enamel_White"),  600.f,  0.22f, 0.36f, 0.62f,    0.3f,  0.f, nullptr },   // the water bowl
		{ TEXT("SM_K_Skirting/Enamel"),   Textured, TEXT("T_Enamel_White"),  600.f,  0.95f, 0.93f, 0.88f,    0.4f,  0.f, Wood },

		{ TEXT("SM_S_Car_Wheel/Rubber"),  Flat,     nullptr,                 0.f,    0.03f, 0.03f, 0.035f,   0.85f, 0.f, nullptr },   // tyres (the kitchen's Rubber is the dog's mat)
		{ TEXT("SM_L_FloorLamp/LampShade"), Glow,   nullptr,                 0.f,    1.6f, 1.3f, 0.8f,       0.8f,  0.f, nullptr },   // always lit, like its light
		{ TEXT("SM_L_Carpet/Carpet"),     Textured, TEXT("T_Rug_Red"),       900.f,  1.f, 1.f, 1.f,          0.95f, 0.f, Cloth },

		// ---- wood and paint
		{ TEXT("Cabinet"),      Textured, TEXT("T_Cabinet_Sage"),  900.f,  1.f, 1.f, 1.f,           0.5f,  0.f, Wood },
		{ TEXT("CabinetInset"), Textured, TEXT("T_Cabinet_Sage"),  900.f,  0.86f, 0.86f, 0.86f,     0.55f, 0.f, Wood },
		{ TEXT("Butcher"),      Textured, TEXT("T_Wood_Oak"),      1100.f, 1.f, 1.f, 1.f,           0.28f, 0.f, Wood },   // oiled and wiped: the worktop starts clean (the mess reads against it)
		{ TEXT("Walnut"),       Textured, TEXT("T_Wood_Walnut"),   900.f,  1.5f, 1.45f, 1.4f,       0.5f,  0.f, Wood },
		{ TEXT("Plinth"),       Flat,     nullptr,                 0.f,    0.08f, 0.08f, 0.09f,     0.7f,  0.f, nullptr },
		// ---- metal
		{ TEXT("Steel"),        Textured, TEXT("T_Steel_Brushed"), 500.f,  1.f, 1.f, 1.f,           0.32f, 0.9f, Metal },
		{ TEXT("Chrome"),       Textured, TEXT("T_Steel_Brushed"), 500.f,  1.2f, 1.2f, 1.22f,       0.14f, 1.f,  Metal },
		{ TEXT("Brass"),        Textured, TEXT("T_Steel_Brushed"), 400.f,  1.3f, 0.93f, 0.33f,      0.3f,  1.f,  Metal },
		{ TEXT("Iron"),         Textured, TEXT("T_Steel_Brushed"), 500.f,  0.22f, 0.22f, 0.23f,     0.6f,  0.6f, Metal },
		// ---- enamel: clean painted surfaces
		{ TEXT("Enamel"),       Textured, TEXT("T_Enamel_White"),  600.f,  1.f, 0.99f, 0.95f,       0.25f, 0.f, nullptr },
		{ TEXT("FridgeRed"),    Textured, TEXT("T_Enamel_White"),  600.f,  0.8f, 0.16f, 0.12f,      0.2f,  0.f, Metal },
		{ TEXT("FridgeLiner"),  Textured, TEXT("T_Enamel_White"),  600.f,  0.86f, 0.9f, 0.93f,      0.35f, 0.f, nullptr },
		{ TEXT("Red"),          Textured, TEXT("T_Enamel_White"),  600.f,  0.75f, 0.1f, 0.08f,      0.35f, 0.f, nullptr },
		{ TEXT("Blue"),         Textured, TEXT("T_Enamel_White"),  600.f,  0.1f, 0.25f, 0.75f,      0.35f, 0.f, nullptr },
		{ TEXT("MugBlue"),      Textured, TEXT("T_Enamel_White"),  600.f,  0.15f, 0.35f, 0.6f,      0.3f,  0.f, nullptr },
		// ---- the splashback is modelled tile by tile: glossy enamel tiles, plain grout
		{ TEXT("Tile"),         Textured, TEXT("T_Enamel_White"),  600.f,  0.96f, 0.95f, 0.9f,      0.12f, 0.f, nullptr },
		{ TEXT("Grout"),        Flat,     nullptr,                 0.f,    0.55f, 0.55f, 0.52f,     0.9f,  0.f, nullptr },
		// ---- cloth
		{ TEXT("Fabric"),       Textured, TEXT("T_Fabric_Grey"),   300.f,  2.1f, 1.58f, 0.35f,      0.9f,  0.f, Cloth },
		// ---- plain colours: nothing in /Game/Textures fits
		{ TEXT("BlackGlass"),   Flat, nullptr, 0.f, 0.02f, 0.02f, 0.025f,  0.08f, 0.f, nullptr },
		{ TEXT("ShelfGlass"),   Flat, nullptr, 0.f, 0.62f, 0.82f, 0.88f,   0.1f,  0.f, nullptr },
		{ TEXT("OvenLiner"),    Flat, nullptr, 0.f, 0.035f, 0.03f, 0.03f,  0.6f,  0.f, Metal },
		{ TEXT("Terracotta"),   Flat, nullptr, 0.f, 0.72f, 0.35f, 0.2f,    0.8f,  0.f, nullptr },
		{ TEXT("Leaf"),         Flat, nullptr, 0.f, 0.16f, 0.5f, 0.18f,    0.5f,  0.f, nullptr },
		{ TEXT("Paper"),        Flat, nullptr, 0.f, 0.97f, 0.96f, 0.88f,   0.9f,  0.f, nullptr },
		{ TEXT("Honey"),        Flat, nullptr, 0.f, 0.9f, 0.55f, 0.05f,    0.15f, 0.f, nullptr },
		{ TEXT("Jam"),          Flat, nullptr, 0.f, 0.55f, 0.05f, 0.12f,   0.15f, 0.f, nullptr },
		{ TEXT("Sugar"),        Flat, nullptr, 0.f, 0.95f, 0.95f, 0.95f,   0.6f,  0.f, nullptr },
		{ TEXT("BurnerRing"),   Flat, nullptr, 0.f, 0.65f, 0.12f, 0.05f,   0.4f,  0.f, Metal },
		{ TEXT("PlasticBlue"),  Flat, nullptr, 0.f, 0.12f, 0.3f, 0.75f,    0.35f, 0.f, nullptr },
		{ TEXT("Sponge"),       Flat, nullptr, 0.f, 0.92f, 0.8f, 0.2f,     0.95f, 0.f, Cloth },
		{ TEXT("Cheese"),       Flat, nullptr, 0.f, 0.96f, 0.78f, 0.22f,   0.5f,  0.f, nullptr },
		{ TEXT("Toast"),        Flat, nullptr, 0.f, 0.8f, 0.55f, 0.25f,    0.85f, 0.f, nullptr },
		{ TEXT("CakePink"),     Flat, nullptr, 0.f, 0.96f, 0.58f, 0.7f,    0.6f,  0.f, nullptr },
		{ TEXT("Cardboard"),    Flat, nullptr, 0.f, 0.7f, 0.52f, 0.32f,    0.9f,  0.f, nullptr },
		{ TEXT("Kibble"),       Flat, nullptr, 0.f, 0.42f, 0.24f, 0.1f,    0.9f,  0.f, nullptr },
		{ TEXT("Rubber"),       Flat, nullptr, 0.f, 0.2f, 0.28f, 0.25f,    0.9f,  0.f, Cloth },
		{ TEXT("TennisBall"),   Flat, nullptr, 0.f, 0.78f, 0.9f, 0.22f,    0.9f,  0.f, Cloth },
		{ TEXT("Leather"),      Flat, nullptr, 0.f, 0.24f, 0.17f, 0.13f,   0.6f,  0.f, Cloth },
		{ TEXT("Ice"),          Flat, nullptr, 0.f, 0.78f, 0.92f, 1.f,     0.1f,  0.f, nullptr },
		// ---- the living room
		{ TEXT("SofaFabric"),   Textured, TEXT("T_Fabric_Grey"),   300.f,  0.2f, 0.73f, 0.88f,      0.95f, 0.f, Cloth },
		{ TEXT("Cushion"),      Textured, TEXT("T_Fabric_Grey"),   300.f,  0.31f, 1.01f, 1.1f,      0.95f, 0.f, Cloth },
		{ TEXT("Carpet"),       Textured, TEXT("T_Rug_Red"),       900.f,  1.f, 1.f, 1.f,           0.95f, 0.f, Cloth },
		{ TEXT("CarpetTrim"),   Textured, TEXT("T_Fabric_Grey"),   300.f,  2.05f, 1.87f, 1.45f,     0.95f, 0.f, Cloth },
		{ TEXT("SockWool"),     Textured, TEXT("T_Fabric_Grey"),   200.f,  0.92f, 1.01f, 1.36f,     0.95f, 0.f, Cloth },
		{ TEXT("Yarn"),         Textured, TEXT("T_Fabric_Grey"),   150.f,  1.7f, 0.26f, 0.66f,      0.95f, 0.f, Cloth },
		{ TEXT("Teal"),         Textured, TEXT("T_Enamel_White"),  600.f,  0.05f, 0.45f, 0.48f,     0.4f,  0.f, nullptr },
		{ TEXT("Navy"),         Textured, TEXT("T_Enamel_White"),  600.f,  0.08f, 0.14f, 0.36f,     0.5f,  0.f, nullptr },
		{ TEXT("Crisps"),       Flat, nullptr, 0.f, 0.96f, 0.76f, 0.22f,   0.6f,  0.f, nullptr },
		{ TEXT("Biscuit"),      Flat, nullptr, 0.f, 0.74f, 0.48f, 0.2f,    0.8f,  0.f, nullptr },
		{ TEXT("Dust"),         Flat, nullptr, 0.f, 0.55f, 0.53f, 0.5f,    0.95f, 0.f, Cloth },
		{ TEXT("Wicker"),       Flat, nullptr, 0.f, 0.74f, 0.52f, 0.26f,   0.8f,  0.f, Wood },
		{ TEXT("Sisal"),        Flat, nullptr, 0.f, 0.8f, 0.66f, 0.42f,    0.95f, 0.f, Cloth },
		{ TEXT("Screen"),       Glow, nullptr, 0.f, 0.12f, 0.4f, 1.05f,    0.15f, 0.f, nullptr },
		{ TEXT("Bulb"),         Glow, nullptr, 0.f, 4.f, 3.4f, 2.2f,       0.3f,  0.f, nullptr },
		// ---- the yard and the street
		{ TEXT("Soil"),         Textured, TEXT("T_Soil"),          700.f,  0.6f, 0.6f, 0.6f,        0.95f, 0.f, TEXT("Surf_Soil") },
		{ TEXT("Bark"),         Textured, TEXT("T_Wood_Walnut"),   900.f,  0.95f, 0.8f, 0.7f,       0.9f,  0.f, Wood },
		{ TEXT("FencePaint"),   Flat, nullptr, 0.f, 0.94f, 0.91f, 0.82f,   0.6f,  0.f, Wood },
		{ TEXT("BinGreen"),     Textured, TEXT("T_Enamel_White"),  600.f,  0.1f, 0.34f, 0.18f,      0.45f, 0.f, Metal },
		{ TEXT("TrashBag"),     Flat, nullptr, 0.f, 0.03f, 0.03f, 0.035f,  0.35f, 0.f, nullptr },
		{ TEXT("Bone"),         Flat, nullptr, 0.f, 0.94f, 0.9f, 0.78f,    0.6f,  0.f, nullptr },
		// Stone outside: the granite texture, tinted, at a tile size that reads as fine grain from an ant's height.
		// (The material's "speckle" mode draws 3-7 unit square cells: at ant scale that was coarse square noise.)
		{ TEXT("Stone"),        Textured, TEXT("T_Granite"), 260.f, 1.05f, 1.02f, 0.95f, 0.85f, 0.f, nullptr },
		{ TEXT("Concrete"),     Textured, TEXT("T_Granite"), 300.f, 1.2f, 1.18f, 1.12f, 0.9f, 0.f, nullptr },
		{ TEXT("Kerb"),         Textured, TEXT("T_Granite"), 280.f, 0.92f, 0.9f, 0.86f, 0.9f,  0.f, nullptr },
		{ TEXT("Asphalt"),      Textured, TEXT("T_Granite"), 200.f, 0.2f, 0.2f, 0.22f, 0.9f,  0.f, nullptr },
		{ TEXT("RoadPaint"),    Flat, nullptr, 0.f, 0.93f, 0.91f, 0.82f,   0.7f,  0.f, nullptr },
		{ TEXT("GrassDark"),    Flat, nullptr, 0.f, 0.15f, 0.38f, 0.1f,    0.9f,  0.f, TEXT("Surf_Grass") },
		{ TEXT("Grass"),        Flat, nullptr, 0.f, 0.3f, 0.56f, 0.16f,    0.9f,  0.f, TEXT("Surf_Grass") },
		{ TEXT("LeafLight"),    Flat, nullptr, 0.f, 0.36f, 0.62f, 0.18f,   0.55f, 0.f, nullptr },
		{ TEXT("Siding"),       Flat, nullptr, 0.f, 0.95f, 0.82f, 0.56f,   0.8f,  0.f, nullptr },
		{ TEXT("IceCream"),     Flat, nullptr, 0.f, 0.97f, 0.62f, 0.74f,   0.4f,  0.f, nullptr },
		{ TEXT("Waffle"),       Flat, nullptr, 0.f, 0.82f, 0.57f, 0.26f,   0.75f, 0.f, nullptr },
		{ TEXT("Gum"),          Flat, nullptr, 0.f, 0.95f, 0.42f, 0.66f,   0.3f,  0.f, nullptr },
		{ TEXT("CarPaint"),     Textured, TEXT("T_Enamel_White"),  600.f,  0.08f, 0.52f, 0.58f,     0.22f, 0.f, Metal },
		{ TEXT("LampGlass"),    Glow, nullptr, 0.f, 3.f, 2.7f, 1.86f,      0.3f,  0.f, nullptr },
		{ TEXT("Headlight"),    Glow, nullptr, 0.f, 2.f, 1.88f, 1.5f,      0.2f,  0.f, nullptr },
		// ---- special
		{ TEXT("HeatGlow"),     Glow,   nullptr, 0.f, 4.f, 1.2f, 0.32f,    0.5f,  0.f, nullptr },   // the oven's elements
		{ TEXT("LampShade"),    Driven, nullptr, 0.f, 0.98f, 0.94f, 0.82f, 0.6f,  0.f, nullptr },   // lit by the light switch (its own material per lamp)
	};

	// Level v3 (-AntLevelV3): the new slots of its meshes and the calmer palette (PALETTE_V3 in Tools/level/level_v3_layout.py):
	// the kitchen rug gets its own terracotta instead of sharing Red with every red thing, the living-room carpet a dusty rose,
	// the sofa a darker teal. Looked up before the table above, only with v3 on.
	const FLook GLooksV3[] =
	{
		{ TEXT("SM_K_Rug/Red"),    Textured, TEXT("T_Fabric_Grey"),   300.f,  1.32f, 0.79f, 0.57f,     0.95f, 0.f, Cloth },
		{ TEXT("SM_K_Rug/Paper"),  Textured, TEXT("T_Fabric_Grey"),   300.f,  1.94f, 1.8f, 1.54f,      0.95f, 0.f, Cloth },
		{ TEXT("SM_L_Carpet/Carpet"), Textured, TEXT("T_Fabric_Grey"), 300.f, 1.23f, 0.73f, 0.66f,     0.95f, 0.f, Cloth },
		{ TEXT("Carpet"),          Textured, TEXT("T_Fabric_Grey"),   300.f,  1.23f, 0.73f, 0.66f,     0.95f, 0.f, Cloth },
		{ TEXT("CarpetTrim"),      Textured, TEXT("T_Fabric_Grey"),   300.f,  1.76f, 1.58f, 1.28f,     0.95f, 0.f, Cloth },
		{ TEXT("SofaFabric"),      Textured, TEXT("T_Fabric_Grey"),   300.f,  0.2f, 0.62f, 0.66f,      0.95f, 0.f, Cloth },
		{ TEXT("ClothCheck"),      Textured, TEXT("T_Fabric_Grey"),   200.f,  1.76f, 0.48f, 0.44f,     0.9f,  0.f, Cloth },
		{ TEXT("Curtain"),         Textured, TEXT("T_Fabric_Grey"),   300.f,  1.89f, 1.72f, 1.32f,     0.9f,  0.f, Cloth },
		{ TEXT("Cola"),            Flat, nullptr, 0.f, 0.16f, 0.05f, 0.02f,   0.08f, 0.f, nullptr },
		{ TEXT("PetClear"),        Flat, nullptr, 0.f, 0.82f, 0.9f, 0.88f,    0.05f, 0.f, nullptr },
		{ TEXT("GiftWrap"),        Flat, nullptr, 0.f, 0.2f, 0.32f, 0.7f,     0.5f,  0.f, Cloth },
		{ TEXT("Photo"),           Flat, nullptr, 0.f, 0.55f, 0.62f, 0.7f,    0.3f,  0.f, nullptr },
		{ TEXT("Brine"),           Flat, nullptr, 0.f, 0.7f, 0.78f, 0.45f,    0.1f,  0.f, nullptr },
		{ TEXT("Latex"),           Flat, nullptr, 0.f, 0.85f, 0.12f, 0.15f,   0.25f, 0.f, nullptr },
		{ TEXT("Plaster"),         Flat, nullptr, 0.f, 0.93f, 0.92f, 0.88f,   0.85f, 0.f, nullptr },
		{ TEXT("Potato"),          Flat, nullptr, 0.f, 0.7f, 0.55f, 0.32f,    0.9f,  0.f, nullptr },
		// the sheets on the line in the yard (tinted instances of the curtain mesh: KitchenArt::ApplyLook's Tint)
		{ TEXT("Tint/Enamel"),     Textured, TEXT("T_Fabric_Grey"),   300.f,  2.05f, 2.f, 1.9f,       0.9f,  0.f, Cloth },
		{ TEXT("Tint/Teal"),       Textured, TEXT("T_Fabric_Grey"),   300.f,  0.3f, 1.1f, 1.15f,      0.9f,  0.f, Cloth },
	};

	const FLook* FindLook(const TCHAR* Name, int32 Index, const FString& Slot)
	{
		const FString Keys[3] = { FString::Printf(TEXT("%s#%d/%s"), Name, Index, *Slot), FString::Printf(TEXT("%s/%s"), Name, *Slot), Slot };
		if (LevelV3::Enabled())
		{
			for (const FString& K : Keys)
			{
				for (const FLook& L : GLooksV3)
				{
					if (K.Equals(L.Key, ESearchCase::IgnoreCase)) { return &L; }
				}
			}
		}
		for (const FString& K : Keys)
		{
			for (const FLook& L : GLooks)
			{
				if (K.Equals(L.Key, ESearchCase::IgnoreCase))   // slot names are FNames: their case is whatever was registered first
				{
					return &L;
				}
			}
		}
		return nullptr;
	}

	/** One material per look, shared by every piece. Rooted: the cache is static and must survive the map reload from the menu. */
	UMaterialInstanceDynamic* SharedMaterial(const FLook& L)
	{
		static TMap<const FLook*, UMaterialInstanceDynamic*> Cache;
		if (UMaterialInstanceDynamic** Found = Cache.Find(&L))
		{
			return *Found;
		}
		UMaterialInstanceDynamic* M = nullptr;
		if (L.Kind == Glow)
		{
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")))
			{
				M = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
				M->SetVectorParameterValue(TEXT("Color"), FLinearColor(L.R, L.G, L.B));
			}
		}
		else if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Textures/M_Surface.M_Surface")))
		{
			M = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
			UTexture* T = L.Tex ? LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Textures/%s.%s"), L.Tex, L.Tex)) : nullptr;
			const bool bTex = L.Kind == Textured && T != nullptr;
			M->SetScalarParameterValue(TEXT("Mode"), bTex ? 0.f : (L.Kind == Speckle ? 3.f : 4.f));
			M->SetScalarParameterValue(TEXT("TileSize"), FMath::Max(1.f, L.Size));
			M->SetScalarParameterValue(TEXT("Roughness"), L.Rough);
			M->SetScalarParameterValue(TEXT("Metallic"), L.Metal);
			M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(L.R, L.G, L.B, 1.f));
			if (bTex)
			{
				M->SetTextureParameterValue(TEXT("Tex"), T);   // the material instance keeps the texture alive
			}
		}
		if (M)
		{
			M->AddToRoot();
		}
		return Cache.Add(&L, M);
	}
}

namespace KitchenArt
{

// Level v3 (-AntLevelV3) swaps in its own base table (LevelV3Art.inl): the same format, the movers keep v2's order (+ the toy box lid).
TConstArrayView<FKitchenArtInst> Instances() { return LevelV3::Enabled() ? LevelV3::BaseArt() : TConstArrayView<FKitchenArtInst>(GKitchenArt, UE_ARRAY_COUNT(GKitchenArt)); }
TConstArrayView<FKitchenArtMover> Movers() { return LevelV3::Enabled() ? LevelV3::BaseMovers() : TConstArrayView<FKitchenArtMover>(GKitchenMovers, UE_ARRAY_COUNT(GKitchenMovers)); }

UStaticMesh* Mesh(const TCHAR* Name)
{
	static TMap<FString, UStaticMesh*> Cache;   // rooted below: static, survives map travel
	const FString N(Name);
	if (UStaticMesh** Found = Cache.Find(N))
	{
		return *Found;
	}
	const TCHAR* Dir = N.StartsWith(TEXT("SM_V3")) ? TEXT("LevelV3") : N.StartsWith(TEXT("SM_K_")) ? TEXT("Kitchen") : TEXT("Living");
	UStaticMesh* M = nullptr;
	if (Map4Look::Enabled())
	{
		// map v4: a generated replacement fitted to the same frame and footprint (SM_M4R_<name>), or a map v4 prop by name
		// Owner's rule (26.11): the generated mop and broom read as foreign next to our own set. They keep their own Blender models.
		// KAN-22: the hero mop, bucket and broom (SourceArt/Blender/hero, approved by the owner 26.11) replace their own meshes in place.
		const bool bKeepOwn = N == TEXT("SM_K_Mop") || N == TEXT("SM_V31_Broom") || N == TEXT("SM_K_Bucket");
		if (!bKeepOwn) { M = Map4Look::Prop(*(TEXT("SM_M4R_") + N.Mid(3))); }
		if (!M && N.StartsWith(TEXT("SM_M4_"))) { M = Map4Look::Prop(*N); }
	}
	if (!M) { M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/%s/%s/%s.%s"), Dir, *N, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet); }
	if (M)
	{
		M->AddToRoot();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KitchenArt: mesh %s is missing"), *N);
	}
	return Cache.Add(N, M);
}

bool Enabled()
{
	static int32 State = -1;
	if (State < 0)
	{
		State = (!FParse::Param(FCommandLine::Get(), TEXT("AntOldKitchen")) && Mesh(TEXT("SM_K_Fridge")) != nullptr) ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("KitchenArt: %s"), State ? TEXT("the modelled kitchen") : TEXT("the box-built kitchen"));
	}
	return State == 1;
}

const FKitchenArtInst* Find(const TCHAR* Name, int32 Index)
{
	for (const FKitchenArtInst& I : Instances())
	{
		if (I.Index == Index && FCString::Strcmp(I.Name, Name) == 0)
		{
			return &I;
		}
	}
	return nullptr;
}

FTransform WorldTransform(const FKitchenArtInst& I)
{
	FQuat Q = FRotationMatrix::MakeFromXY(FVector(I.AX), FVector(I.AY)).ToQuat();   // orthonormal whatever the rounding in the table
	Q.Normalize();
	return FTransform(Q, FVector(I.Loc), FVector(I.Scale) * ImportScale);
}

void ApplyTint(UStaticMeshComponent* C, const TCHAR* Tint)
{
	if (!C || !Tint || !*Tint)
	{
		return;
	}
	if (Map4Look::Enabled())
	{
		if (const Map4Look::FLook* L4 = Map4Look::ForSlot(TEXT("Tint"), 0, FString(Tint)))
		{
			if (UMaterialInterface* Mat = Map4Look::Get(*L4)) { C->SetMaterial(0, Mat); return; }
		}
	}
	if (const FLook* L = FindLook(TEXT("Tint"), 0, FString(Tint)))
	{
		if (UMaterialInstanceDynamic* Mat = SharedMaterial(*L)) { C->SetMaterial(0, Mat); }
	}
}

void ApplyLook(UStaticMeshComponent* C, const TCHAR* Name, int32 Index)
{
	UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
	if (!M)
	{
		return;
	}
	const TArray<FStaticMaterial>& Slots = M->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString Slot = Slots[i].MaterialSlotName.ToString();
		if (Slot.StartsWith(TEXT("Hero_")) && Slots[i].MaterialInterface)
		{
			// Hero props (KAN-22): their own baked texture set and the state layer (M_Hero, Tools/art/make_hero_material.py)
			C->SetMaterial(i, Slots[i].MaterialInterface);
			if (i == 0) { C->ComponentTags.AddUnique(TEXT("HeroArt")); }
			continue;
		}
		if (Map4Look::Enabled())
		{
			// Map v4: real texture sets (M_M4). The driven lamp shades get their own instance (the light switch sets "Color").
			if (const Map4Look::FLook* L4 = Map4Look::ForSlot(Name, Index, Slot))
			{
				const bool bDriven = Slot.Equals(TEXT("LampShade"), ESearchCase::IgnoreCase) && FCString::Strcmp(Name, TEXT("SM_L_FloorLamp")) != 0;
				UMaterialInterface* Mat = bDriven ? (UMaterialInterface*)Map4Look::Own(*L4, C) : Map4Look::Get(*L4);
				if (Mat)
				{
					C->SetMaterial(i, Mat);
					if (i == 0 && L4->Step) { C->ComponentTags.AddUnique(FName(L4->Step)); }
					continue;
				}
			}
		}
		const FLook* L = FindLook(Name, Index, Slot);
		if (!L)
		{
			if (Slot != TEXT("M4Prop")) { UE_LOG(LogTemp, Warning, TEXT("KitchenArt: no look for slot %s of %s"), *Slot, Name); }
			continue;   // (map v4 replacements carry their own textured material)
		}
		if (L->Kind == Driven)
		{
			// Its own instance: the owner changes the colour at run time.
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* Own = UMaterialInstanceDynamic::Create(Base, C);
				Own->SetVectorParameterValue(TEXT("Color"), FLinearColor(L->R, L->G, L->B));
				C->SetMaterial(i, Own);
			}
		}
		else if (UMaterialInstanceDynamic* Mat = SharedMaterial(*L))
		{
			C->SetMaterial(i, Mat);
		}
		if (i == 0 && L->Step)
		{
			C->ComponentTags.AddUnique(FName(L->Step));
		}
	}
}

void SetSolid(UStaticMeshComponent* C, ESolid Solid)
{
	if (!C)
	{
		return;
	}
	if (Solid == ESolid::None)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		return;
	}
	// The meshes collide with their own triangles (complex as simple, set at import): ants crawl on exactly what is drawn,
	// inside every cavity; food rests on it; the Human bumps into it.
	C->SetCollisionObjectType(Solid == ESolid::Moving ? ECC_AntClimbable : ECC_WorldStatic);
	C->SetCollisionResponseToAllChannels(ECR_Block);
	C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	C->SetGenerateOverlapEvents(false);
	if (Solid == ESolid::Moving)
	{
		C->ComponentTags.AddUnique(TEXT("AntClimbable"));   // a moving base: whoever stands on it travels with it
	}
}

UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, int32 Index, const FTransform& Relative, ESolid Solid, bool bStaticMobility, FName StableName)
{
	UStaticMesh* M = Mesh(Name);
	if (!M || !Owner)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = StableName.IsNone() ? NewObject<UStaticMeshComponent>(Owner) : NewObject<UStaticMeshComponent>(Owner, StableName);
	if (!StableName.IsNone())
	{
		C->SetNetAddressable();   // same name on every machine: an ant standing on it can be replicated relative to it
	}
	C->SetMobility(bStaticMobility ? EComponentMobility::Static : EComponentMobility::Movable);
	C->SetStaticMesh(M);
	C->SetupAttachment(Parent);
	C->SetRelativeTransform(Relative);
	SetSolid(C, Solid);
	C->SetCanEverAffectNavigation(false);
	ApplyLook(C, Name, Index);
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	UAntSwingComponent::AddTo(C, Name);   // a part on pins (the bucket's handle), if the piece has one
	return C;
}

UStaticMeshComponent* AddAt(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, int32 Index, ESolid Solid, bool bStaticMobility, FName StableName)
{
	const FKitchenArtInst* I = Find(Name, Index);
	if (!I || !Parent)
	{
		return nullptr;
	}
	return Add(Owner, Parent, Name, Index, WorldTransform(*I).GetRelativeTransform(Parent->GetComponentTransform()), Solid, bStaticMobility, StableName);
}

void ProbeSpots(UWorld* World, const TArray<FVector>& Spots)
{
	if (!World)
	{
		return;
	}
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_AntClimbable);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArtProbe), true);
	for (int32 i = 0; i < Spots.Num(); ++i)
	{
		const FVector P = Spots[i];   // 35 above the surface the spot was designed for

		auto What = [](bool bHit, const FHitResult& H) -> FString
		{
			const UStaticMeshComponent* SM = bHit ? Cast<UStaticMeshComponent>(H.GetComponent()) : nullptr;
			return SM && SM->GetStaticMesh() ? SM->GetStaticMesh()->GetName() : FString(bHit ? TEXT("?") : TEXT("-"));
		};
		FHitResult Down, Near, Up;
		const bool bDown = World->LineTraceSingleByObjectType(Down, P + FVector(0.f, 0.f, 400.f), P - FVector(0.f, 0.f, 300.f), Obj, Params);
		const bool bNear = World->LineTraceSingleByObjectType(Near, P, P - FVector(0.f, 0.f, 300.f), Obj, Params);
		const bool bUp = World->LineTraceSingleByObjectType(Up, P, P + FVector(0.f, 0.f, 400.f), Obj, Params);
		UE_LOG(LogTemp, Display, TEXT("ARTPROBE %d (%.0f,%.0f,%.0f) surface %.0f | from above: %s %.0f | from the spot: %s %.0f | ceiling: %s %.0f"),
			i, P.X, P.Y, P.Z, P.Z - 35.f,
			*What(bDown, Down), bDown ? Down.ImpactPoint.Z : 0.f, *What(bNear, Near), bNear ? Near.ImpactPoint.Z : 0.f, *What(bUp, Up), bUp ? Up.ImpactPoint.Z : 0.f);
	}
}

void ProbeAnt(const ACharacter* Ant, int32 Step, const FVector& Target)
{
	if (!Ant)
	{
		return;
	}
	const UCharacterMovementComponent* CM = Ant->GetCharacterMovement();
	const FVector P = Ant->GetActorLocation();
	FHitResult H;
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_AntClimbable);
	const bool bHit = Ant->GetWorld()->LineTraceSingleByObjectType(H, P, P - FVector(0.f, 0.f, 80.f), Obj, FCollisionQueryParams(SCENE_QUERY_STAT(ArtProbeAnt), true, Ant));
	const UStaticMeshComponent* SM = bHit ? Cast<UStaticMeshComponent>(H.GetComponent()) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("ARTPOS %d target (%.0f,%.0f,%.0f) at (%.0f,%.0f,%.0f) moved %.0f falling=%d speed=%.0f on %s"),
		Step, Target.X, Target.Y, Target.Z, P.X, P.Y, P.Z, FVector::Dist(P, Target), CM && CM->IsFalling() ? 1 : 0, Ant->GetVelocity().Size(),
		SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : (bHit ? TEXT("?") : TEXT("nothing within 80")));
}

} // namespace KitchenArt