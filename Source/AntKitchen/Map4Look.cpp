#include "Map4Look.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/CollisionProfile.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"

namespace Map4Look
{

namespace
{
	const FLook GLooks[] =
	{
		{ TEXT("SM_K_Rug/Red"), TEXT("Jute"), 260.000f, 1.050f, 0.780f, 0.620f, 1.000f, 0.050f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("SM_K_Rug/Paper"), TEXT("Jute"), 260.000f, 1.150f, 1.100f, 1.000f, 1.000f, 0.050f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("SM_K_Shaker#1/Enamel"), TEXT("Enamel"), 600.000f, 0.080f, 0.070f, 0.060f, 1.000f, 0.100f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("SM_K_PetBowl#1/Red"), TEXT("Enamel"), 600.000f, 0.250f, 0.420f, 0.750f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("SM_K_Skirting/Enamel"), TEXT("PaintWood"), 700.000f, 1.000f, 0.970f, 0.900f, 0.700f, 0.150f, 0.000f, 0.400f, 0.500f, 0.800f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("SM_S_Car_Wheel/Rubber"), TEXT("Rubber"), 200.000f, 0.200f, 0.200f, 0.200f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("SM_L_FloorLamp/LampShade"), TEXT("Linen"), 200.000f, 1.000f, 0.900f, 0.720f, 1.000f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 1.400f, nullptr },
		{ TEXT("SM_L_Carpet/Carpet"), TEXT("RugRose"), 1100.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Cabinet"), TEXT("PaintWood"), 900.000f, 0.520f, 0.720f, 0.680f, 0.600f, 0.120f, 0.000f, 0.350f, 0.150f, 0.350f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("CabinetInset"), TEXT("PaintWood"), 900.000f, 0.460f, 0.640f, 0.600f, 0.600f, 0.140f, 0.000f, 0.350f, 0.150f, 0.350f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("Butcher"), TEXT("Oak"), 1100.000f, 1.000f, 1.000f, 1.000f, 0.800f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("Walnut"), TEXT("Walnut"), 900.000f, 1.150f, 1.100f, 1.050f, 1.000f, 0.000f, 0.000f, 1.000f, 0.200f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("Plinth"), TEXT("Enamel"), 600.000f, 0.080f, 0.080f, 0.090f, 1.000f, 0.400f, 0.000f, 1.000f, 0.300f, 0.400f, 0.000f, nullptr },
		{ TEXT("Steel"), TEXT("Brushed"), 500.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("Chrome"), TEXT("Brushed2"), 500.000f, 1.100f, 1.100f, 1.120f, 0.400f, 0.000f, 1.000f, 0.500f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("Brass"), TEXT("Brushed2"), 400.000f, 1.300f, 0.930f, 0.400f, 0.700f, 0.000f, 1.000f, 0.600f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("Iron"), TEXT("MetalPlate"), 500.000f, 0.350f, 0.350f, 0.360f, 1.100f, 0.000f, 0.400f, 1.000f, 0.200f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("Enamel"), TEXT("Enamel"), 600.000f, 1.060f, 1.050f, 1.010f, 1.000f, 0.000f, 0.000f, 1.000f, 0.150f, 0.200f, 0.000f, nullptr },
		{ TEXT("FridgeRed"), TEXT("Enamel"), 600.000f, 0.800f, 0.120f, 0.090f, 0.800f, 0.000f, 0.000f, 1.000f, 0.100f, 0.200f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("FridgeLiner"), TEXT("Plastic"), 600.000f, 0.980f, 1.000f, 1.020f, 1.000f, 0.000f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Red"), TEXT("Enamel"), 600.000f, 0.780f, 0.100f, 0.080f, 1.000f, 0.050f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Blue"), TEXT("Enamel"), 600.000f, 0.100f, 0.250f, 0.780f, 1.000f, 0.050f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("MugBlue"), TEXT("Enamel"), 600.000f, 0.150f, 0.350f, 0.620f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Tile"), TEXT("Enamel"), 600.000f, 1.020f, 1.000f, 0.940f, 0.600f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Grout"), TEXT("Concrete"), 150.000f, 0.750f, 0.720f, 0.680f, 1.000f, 0.200f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Fabric"), TEXT("Terry"), 250.000f, 1.050f, 0.800f, 0.250f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("BlackGlass"), TEXT("Enamel"), 600.000f, 0.020f, 0.020f, 0.025f, 0.300f, 0.000f, 0.000f, 0.200f, 0.050f, 0.000f, 0.000f, nullptr },
		{ TEXT("ShelfGlass"), TEXT("Enamel"), 600.000f, 0.620f, 0.820f, 0.880f, 0.300f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("OvenLiner"), TEXT("MetalPlate"), 400.000f, 0.120f, 0.110f, 0.110f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("Terracotta"), TEXT("Concrete"), 220.000f, 1.000f, 0.520f, 0.330f, 1.000f, 0.100f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Leaf"), TEXT("Enamel"), 600.000f, 0.160f, 0.500f, 0.180f, 1.000f, 0.250f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Paper"), TEXT("Paper"), 300.000f, 1.050f, 1.040f, 0.980f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Honey"), TEXT("Enamel"), 600.000f, 0.900f, 0.550f, 0.050f, 0.300f, 0.000f, 0.000f, 0.300f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Jam"), TEXT("Enamel"), 600.000f, 0.550f, 0.050f, 0.120f, 0.300f, 0.000f, 0.000f, 0.300f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Sugar"), TEXT("Concrete"), 40.000f, 1.300f, 1.300f, 1.300f, 1.000f, 0.100f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("BurnerRing"), TEXT("MetalPlate"), 300.000f, 0.140f, 0.130f, 0.130f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("PlasticBlue"), TEXT("Plastic"), 600.000f, 0.300f, 0.500f, 1.100f, 1.000f, 0.000f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Sponge"), TEXT("Carpet"), 120.000f, 1.300f, 1.100f, 0.350f, 1.000f, 0.100f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Cheese"), TEXT("Enamel"), 600.000f, 0.980f, 0.780f, 0.220f, 1.000f, 0.300f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Toast"), TEXT("Cardboard"), 200.000f, 1.100f, 0.800f, 0.500f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("CakePink"), TEXT("Jersey"), 150.000f, 1.000f, 0.620f, 0.720f, 1.000f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Cardboard"), TEXT("Cardboard"), 400.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.200f, 0.000f, 0.000f, nullptr },
		{ TEXT("Kibble"), TEXT("Soil"), 100.000f, 0.650f, 0.420f, 0.250f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Rubber"), TEXT("Rubber"), 250.000f, 0.300f, 0.420f, 0.360f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("TennisBall"), TEXT("Boucle"), 60.000f, 0.950f, 1.050f, 0.300f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Leather"), TEXT("Leather"), 300.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Ice"), TEXT("Enamel"), 600.000f, 0.780f, 0.920f, 1.000f, 0.400f, 0.000f, 0.000f, 0.400f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("SofaFabric"), TEXT("SofaWool"), 260.000f, 0.400f, 0.850f, 0.880f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Cushion"), TEXT("Jersey"), 200.000f, 1.050f, 0.800f, 0.350f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Carpet"), TEXT("RugRose"), 1100.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("CarpetTrim"), TEXT("Jute"), 200.000f, 1.100f, 1.000f, 0.850f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("SockWool"), TEXT("Knit"), 120.000f, 0.900f, 0.950f, 1.300f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Yarn"), TEXT("Knit"), 90.000f, 1.500f, 0.350f, 0.600f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Teal"), TEXT("Enamel"), 600.000f, 0.050f, 0.450f, 0.480f, 1.000f, 0.100f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Navy"), TEXT("Enamel"), 600.000f, 0.080f, 0.140f, 0.360f, 1.000f, 0.150f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Crisps"), TEXT("Cardboard"), 100.000f, 1.250f, 1.050f, 0.450f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Biscuit"), TEXT("Cardboard"), 150.000f, 1.150f, 0.800f, 0.420f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Dust"), TEXT("Carpet"), 100.000f, 0.850f, 0.830f, 0.800f, 1.000f, 0.100f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Wicker"), TEXT("Jute"), 150.000f, 1.150f, 0.880f, 0.500f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("Sisal"), TEXT("Jute"), 120.000f, 1.100f, 0.950f, 0.700f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Screen"), TEXT("Enamel"), 600.000f, 0.130f, 0.400f, 1.000f, 0.300f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 1.000f, nullptr },
		{ TEXT("Bulb"), TEXT("Enamel"), 600.000f, 1.000f, 0.850f, 0.550f, 1.000f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 4.000f, nullptr },
		{ TEXT("Soil"), TEXT("Soil"), 700.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Soil") },
		{ TEXT("Bark"), TEXT("Bark"), 600.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("FencePaint"), TEXT("Picket"), 500.000f, 1.020f, 0.980f, 0.900f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.300f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("BinGreen"), TEXT("Plastic"), 500.000f, 0.150f, 0.400f, 0.220f, 1.000f, 0.050f, 0.000f, 0.600f, 0.100f, 0.200f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("TrashBag"), TEXT("Plastic"), 300.000f, 0.060f, 0.060f, 0.070f, 0.600f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Bone"), TEXT("Enamel"), 600.000f, 0.950f, 0.900f, 0.780f, 1.000f, 0.300f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Stone"), TEXT("Concrete"), 300.000f, 1.050f, 1.020f, 0.950f, 1.000f, 0.050f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Concrete"), TEXT("Pavement"), 900.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Kerb"), TEXT("Concrete"), 300.000f, 0.920f, 0.900f, 0.860f, 1.000f, 0.050f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("Asphalt"), TEXT("Asphalt"), 700.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("RoadPaint"), TEXT("Enamel"), 600.000f, 0.930f, 0.910f, 0.820f, 1.000f, 0.450f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("GrassDark"), TEXT("Enamel"), 300.000f, 0.170f, 0.400f, 0.110f, 1.000f, 0.300f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Grass") },
		{ TEXT("Grass"), TEXT("Enamel"), 300.000f, 0.300f, 0.580f, 0.170f, 1.000f, 0.300f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Grass") },
		{ TEXT("LeafLight"), TEXT("Enamel"), 600.000f, 0.360f, 0.620f, 0.180f, 1.000f, 0.250f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Siding"), TEXT("Siding"), 700.000f, 1.050f, 0.950f, 0.780f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.300f, 0.000f, nullptr },
		{ TEXT("IceCream"), TEXT("Jersey"), 150.000f, 1.000f, 0.660f, 0.760f, 0.600f, 0.000f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Waffle"), TEXT("Cardboard"), 120.000f, 1.150f, 0.820f, 0.420f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Gum"), TEXT("Enamel"), 600.000f, 0.950f, 0.420f, 0.660f, 1.000f, 0.100f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("CarPaint"), TEXT("Enamel"), 600.000f, 0.080f, 0.520f, 0.580f, 0.600f, 0.000f, 0.300f, 0.600f, 0.150f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("LampGlass"), TEXT("Enamel"), 600.000f, 1.000f, 0.900f, 0.620f, 1.000f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 3.000f, nullptr },
		{ TEXT("Headlight"), TEXT("Enamel"), 600.000f, 1.000f, 0.950f, 0.800f, 1.000f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 2.000f, nullptr },
		{ TEXT("HeatGlow"), TEXT("Enamel"), 600.000f, 1.000f, 0.300f, 0.080f, 1.000f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 4.000f, nullptr },
		{ TEXT("LampShade"), TEXT("Linen"), 200.000f, 1.000f, 0.980f, 0.940f, 1.000f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.250f, nullptr },
		{ TEXT("ClothCheck"), TEXT("Gingham"), 1500.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Curtain"), TEXT("Linen"), 400.000f, 1.050f, 0.970f, 0.820f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Cola"), TEXT("Enamel"), 600.000f, 0.160f, 0.050f, 0.020f, 0.200f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("PetClear"), TEXT("Enamel"), 600.000f, 0.820f, 0.900f, 0.880f, 0.200f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("GiftWrap"), TEXT("Jersey"), 160.000f, 0.250f, 0.380f, 0.900f, 0.500f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Photo"), TEXT("Enamel"), 600.000f, 0.550f, 0.620f, 0.700f, 0.500f, 0.000f, 0.000f, 0.300f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Brine"), TEXT("Enamel"), 600.000f, 0.700f, 0.780f, 0.450f, 0.300f, 0.000f, 0.000f, 0.300f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Latex"), TEXT("Enamel"), 600.000f, 0.850f, 0.120f, 0.150f, 0.600f, 0.000f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Plaster"), TEXT("Plaster"), 700.000f, 1.020f, 1.000f, 0.960f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.300f, 0.000f, nullptr },
		{ TEXT("Potato"), TEXT("Soil"), 80.000f, 1.250f, 1.000f, 0.620f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("Tint/Enamel"), TEXT("Linen"), 300.000f, 1.120f, 1.100f, 1.040f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("Tint/Teal"), TEXT("Linen"), 300.000f, 0.300f, 0.850f, 0.900f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("@FloorTile"), TEXT("FloorTile"), 2400.000f, 1.080f, 1.000f, 0.900f, 0.900f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@WallK"), TEXT("WallpaperK"), 1100.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.050f, 0.000f, 0.600f, 0.000f, 0.600f, 0.000f, nullptr },
		{ TEXT("@WallL"), TEXT("WallpaperL"), 1300.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.050f, 0.000f, 0.600f, 0.000f, 0.600f, 0.000f, nullptr },
		{ TEXT("@WallOut"), TEXT("Siding"), 800.000f, 1.050f, 0.970f, 0.850f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.500f, 0.000f, nullptr },
		{ TEXT("@Ceiling"), TEXT("Stucco"), 900.000f, 1.050f, 1.040f, 1.020f, 1.000f, 0.000f, 0.000f, 0.600f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Skirting"), TEXT("PaintWood"), 700.000f, 1.000f, 0.970f, 0.900f, 0.700f, 0.150f, 0.000f, 0.400f, 0.600f, 0.800f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Parquet"), TEXT("Parquet"), 1700.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Oak"), TEXT("Oak"), 1100.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Walnut"), TEXT("Walnut"), 900.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Table"), TEXT("TableWood"), 1300.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Brushed"), TEXT("Brushed"), 500.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("@DarkSteel"), TEXT("Brushed"), 500.000f, 0.400f, 0.400f, 0.420f, 1.000f, 0.050f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("@Enamel"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Paint"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.180f, 0.000f, 0.700f, 0.100f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Plastic"), TEXT("Plastic"), 400.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.050f, 0.000f, 0.500f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Soil"), TEXT("Soil"), 700.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Soil") },
		{ TEXT("@Grass"), TEXT("Grass"), 500.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Grass") },
		{ TEXT("@Pavement"), TEXT("Pavement"), 900.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Asphalt"), TEXT("Asphalt"), 700.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Carpet"), TEXT("Carpet"), 400.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("@SofaWool"), TEXT("SofaWool"), 260.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, TEXT("Surf_Fabric") },
		{ TEXT("@Fence"), TEXT("Siding"), 500.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f, 0.100f, 0.300f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Duct"), TEXT("MetalPlate"), 400.000f, 0.800f, 0.800f, 0.800f, 1.000f, 0.000f, 0.000f, 1.000f, 0.300f, 0.000f, 0.000f, TEXT("Surf_Metal") },
		{ TEXT("@Cabinet"), TEXT("PaintWood"), 900.000f, 0.520f, 0.720f, 0.680f, 0.600f, 0.120f, 0.000f, 0.350f, 0.150f, 0.350f, 0.000f, TEXT("Surf_Wood") },
		{ TEXT("@Marble"), TEXT("Marble"), 900.000f, 1.000f, 1.000f, 1.000f, 0.600f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@TileCream"), TEXT("TileCream"), 1500.000f, 1.000f, 1.000f, 1.000f, 0.800f, 0.000f, 0.000f, 1.000f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Silk"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 0.500f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Water"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 0.150f, -0.200f, 0.000f, 0.150f, 0.000f, 0.000f, 0.000f, nullptr },
		{ TEXT("@Coil"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.200f, 0.000f, 0.300f, 0.000f, 0.000f, 0.600f, TEXT("Surf_Metal") },
		{ TEXT("Sausage"), TEXT("Leather"), 120.000f, 1.15f, 0.55f, 0.45f, 0.6f, -0.1f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Grape"), TEXT("Enamel"), 200.000f, 0.48f, 0.72f, 0.26f, 0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("FlyWing"), TEXT("Enamel"), 200.000f, 0.78f, 0.84f, 0.9f, 0.25f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.05f, nullptr },
		{ TEXT("FlyLeg"), TEXT("Leather"), 60.000f, 0.12f, 0.11f, 0.11f, 0.7f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("SpiderLeg"), TEXT("Leather"), 60.000f, 0.75f, 0.5f, 0.32f, 0.9f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("SpiderLegDark"), TEXT("Leather"), 60.000f, 0.4f, 0.27f, 0.17f, 0.9f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Meatball"), TEXT("Soil"), 80.000f, 0.95f, 0.58f, 0.42f, 0.8f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Carrot"), TEXT("Enamel"), 200.000f, 1.0f, 0.45f, 0.08f, 1.0f, 0.1f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Soup"), TEXT("Enamel"), 400.000f, 0.95f, 0.6f, 0.22f, 0.2f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Pizza"), TEXT("Cardboard"), 300.000f, 1.2f, 0.72f, 0.32f, 1.0f, -0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Water"), TEXT("Enamel"), 600.000f, 0.35f, 0.55f, 0.8f, 0.1f, -0.2f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Oil"), TEXT("Enamel"), 600.000f, 0.9f, 0.55f, 0.05f, 0.2f, -0.15f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Burn"), TEXT("Soil"), 100.000f, 0.12f, 0.1f, 0.08f, 1.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("Spark"), TEXT("Enamel"), 600.000f, 1.0f, 0.95f, 0.5f, 1.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 6.0f, nullptr },
		{ TEXT("@Seed"), TEXT("Enamel"), 120.000f, 0.20f, 0.19f, 0.17f, 1.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Pea"), TEXT("Enamel"), 120.000f, 0.32f, 0.66f, 0.22f, 1.0f, 0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Rice"), TEXT("Concrete"), 60.000f, 1.30f, 1.28f, 1.20f, 1.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Kibble"), TEXT("Soil"), 60.000f, 0.80f, 0.50f, 0.28f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Raisin"), TEXT("Leather"), 60.000f, 0.45f, 0.18f, 0.28f, 0.6f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Cornflake"), TEXT("Cardboard"), 60.000f, 1.35f, 0.95f, 0.38f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@ChocoNut"), TEXT("Enamel"), 120.000f, 0.30f, 0.16f, 0.08f, 0.6f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, nullptr },
		{ TEXT("@Glow"), TEXT("Enamel"), 600.000f, 1.000f, 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 0.200f, 0.000f, 0.000f, 1.000f, nullptr },
	};

	UMaterialInterface* Base()
	{
		static UMaterialInterface* M = nullptr;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Map4/Materials/M_M4.M_M4"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (M) { M->AddToRoot(); }
		}
		return M;
	}

	UTexture* Tex(const TCHAR* Set, const TCHAR* Kind)
	{
		static TMap<FString, UTexture*> Cache;
		const FString N = FString::Printf(TEXT("T_M4_%s_%s"), Set, Kind);
		if (UTexture** F = Cache.Find(N)) { return *F; }
		UTexture* T = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Map4/Textures/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (T) { T->AddToRoot(); }
		else { UE_LOG(LogTemp, Warning, TEXT("Map4Look: texture %s is missing"), *N); }
		return Cache.Add(N, T);
	}

	void Fill(UMaterialInstanceDynamic* M, const FLook& L, const FLinearColor& Color)
	{
		if (UTexture* C = Tex(L.Set, TEXT("C"))) { M->SetTextureParameterValue(TEXT("C"), C); }
		if (UTexture* N = Tex(L.Set, TEXT("N"))) { M->SetTextureParameterValue(TEXT("N"), N); }
		if (UTexture* R = Tex(L.Set, TEXT("R"))) { M->SetTextureParameterValue(TEXT("R"), R); }
		if (UTexture* D = Tex(TEXT("Detail"), TEXT("N"))) { M->SetTextureParameterValue(TEXT("D"), D); }
		M->SetScalarParameterValue(TEXT("TileSize"), FMath::Max(10.f, L.Tile));
		M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(L.R, L.G, L.B, 1.f));
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("RoughMul"), L.RoughMul);
		M->SetScalarParameterValue(TEXT("RoughAdd"), L.RoughAdd);
		M->SetScalarParameterValue(TEXT("Metal"), L.Metal);
		M->SetScalarParameterValue(TEXT("NormalStrength"), L.NStr);
		M->SetScalarParameterValue(TEXT("Dust"), L.Dust);
		M->SetScalarParameterValue(TEXT("Grime"), L.Grime);
		M->SetVectorParameterValue(TEXT("Glow"), FLinearColor(L.Glow, L.Glow, L.Glow, 1.f));
		// micro detail: stronger on rough matter (wood, fabric, stone), faint on glossy paint and glass
		M->SetScalarParameterValue(TEXT("DetailStrength"), L.RoughAdd < -0.05f || L.RoughMul < 0.45f ? 0.04f : 0.14f);
	}
}

bool Enabled()
{
	static int32 State = -1;
	if (State < 0)
	{
		State = (!FParse::Param(FCommandLine::Get(), TEXT("AntMap3")) && Base() != nullptr) ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("Map4Look: %s"), State ? TEXT("map v4 look (M_M4)") : TEXT("v3 look"));
	}
	return State == 1;
}

const FLook* Named(const TCHAR* Key)
{
	// builder / named surfaces ("@Key") first, then the slot looks of the same name
	for (const FLook& L : GLooks)
	{
		if (L.Key[0] == TEXT('@') && (FCString::Stricmp(L.Key + 1, Key) == 0 || FCString::Stricmp(L.Key, Key) == 0)) { return &L; }
	}
	for (const FLook& L : GLooks)
	{
		if (FCString::Stricmp(L.Key, Key) == 0) { return &L; }
	}
	return nullptr;
}

UMaterialInterface* Get(const FLook& L, const FLinearColor& Color)
{
	static TMap<FString, UMaterialInterface*> Cache;
	const FString K = FString::Printf(TEXT("%s|%.3f,%.3f,%.3f"), L.Key, Color.R, Color.G, Color.B);
	if (UMaterialInterface** F = Cache.Find(K)) { return *F; }
	UMaterialInstanceDynamic* M = Base() ? UMaterialInstanceDynamic::Create(Base(), GetTransientPackage()) : nullptr;
	if (M)
	{
		Fill(M, L, Color);
		M->AddToRoot();
	}
	return Cache.Add(K, M);
}

UMaterialInstanceDynamic* Own(const FLook& L, UObject* Outer, const FLinearColor& Color)
{
	UMaterialInstanceDynamic* M = Base() ? UMaterialInstanceDynamic::Create(Base(), Outer ? Outer : GetTransientPackage()) : nullptr;
	if (M) { Fill(M, L, Color); }
	return M;
}

const FLook* ForSlot(const TCHAR* Mesh, int32 Index, const FString& Slot)
{
	const FString Keys[3] = { FString::Printf(TEXT("%s#%d/%s"), Mesh, Index, *Slot), FString::Printf(TEXT("%s/%s"), Mesh, *Slot), Slot };
	for (const FString& K : Keys)
	{
		for (const FLook& L : GLooks)
		{
			if (L.Key[0] != TEXT('@') && K.Equals(L.Key, ESearchCase::IgnoreCase)) { return &L; }
		}
	}
	return nullptr;
}

int32 RoomAt(const FVector& P, float Grow)
{
	if (P.Z < -Grow || P.Z > 5400.f + Grow) { return 0; }
	if (P.X > -7000.f - Grow && P.X < 7000.f + Grow && P.Y > -5000.f - Grow && P.Y < 5000.f + Grow) { return 1; }
	if (P.X > 7200.f - Grow && P.X < 20200.f + Grow && P.Y > -9000.f - Grow && P.Y < 1500.f + Grow) { return 2; }
	return 0;
}

int32 WallRoom(const FVector& Center)
{
	const int32 K = RoomAt(Center, 330.f);
	return K;
}

const FLook* ForColor(const FLinearColor& C, const FVector& Where, FLinearColor& OutTint)
{
	auto Hex = [](uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); };
	auto Is = [&C](const FLinearColor& K) { return C.Equals(K, 0.002f); };
	OutTint = FLinearColor::White;
		if (Is(Hex(225, 215, 195)) || Is(Hex(150, 60, 55))) { return Named(TEXT("FloorTile")); }
	if (Is(Hex(240, 225, 185)))
	{
		const int32 R = WallRoom(Where);
		return Named(R == 1 ? TEXT("WallK") : R == 2 ? TEXT("WallL") : TEXT("WallOut"));
	}
	if (Is(Hex(250, 250, 245))) { return Named(TEXT("Ceiling")); }
	if (Is(Hex(120, 80, 50))) { return Named(TEXT("Skirting")); }
	if (Is(Hex(120, 170, 200))) { return Named(TEXT("TileCream")); }
	if (Is(Hex(130, 190, 170)) || Is(Hex(145, 205, 185))) { return Named(TEXT("Cabinet")); }
	if (Is(Hex(60, 60, 68))) { return Named(TEXT("Marble")); }
	if (Is(Hex(175, 180, 185))) { return Named(TEXT("Brushed")); }
	if (Is(Hex(70, 72, 78))) { return Named(TEXT("DarkSteel")); }
	if (Is(Hex(160, 105, 60))) { return Named(TEXT("Oak")); }
	if (Is(Hex(110, 70, 40))) { return Named(TEXT("Walnut")); }
	if (Is(Hex(95, 65, 40)) || Is(Hex(110, 80, 50))) { return Named(TEXT("Soil")); }
	if (Is(Hex(60, 40, 25))) { OutTint = FLinearColor(0.6f, 0.6f, 0.6f); return Named(TEXT("Soil")); }
	if (Is(Hex(86, 142, 60)) || Is(Hex(52, 104, 38)) || Is(Hex(70, 120, 50)) || Is(Hex(60, 120, 50))) { return Named(TEXT("Grass")); }
	if (Is(Hex(168, 165, 158))) { return Named(TEXT("Pavement")); }
	if (Is(Hex(58, 58, 62))) { return Named(TEXT("Asphalt")); }
	if (Is(Hex(170, 40, 50))) { OutTint = FLinearColor(1.1f, 0.35f, 0.35f); return Named(TEXT("Carpet")); }
	if (Is(Hex(70, 92, 125)) || Is(Hex(52, 70, 98))) { OutTint = C * 2.4f; return Named(TEXT("SofaWool")); }
	if (Is(Hex(196, 152, 104))) { return Named(TEXT("Parquet")); }
	if (Is(Hex(205, 160, 105))) { return Named(TEXT("Oak")); }
	if (Is(Hex(160, 120, 78))) { return Named(TEXT("Fence")); }
	if (Is(Hex(150, 152, 150))) { return Named(TEXT("Duct")); }
	if (Is(Hex(245, 245, 245))) { return Named(TEXT("Enamel")); }
	// anything else: painted, in its own colour (browns read as wood)
	const FLinearColor H = C.LinearRGBToHSV();
	if (H.R > 15.f && H.R < 45.f && H.G > 0.35f && H.B < 0.75f) { OutTint = C * 1.6f; return Named(TEXT("Oak")); }
	OutTint = C;
	return Named(TEXT("Paint"));
}

UMaterialInstanceDynamic* Paint(UObject* Outer, const FLinearColor& Color)
{
	static const FLook* P = Named(TEXT("Paint"));
	return P ? Own(*P, Outer, Color) : nullptr;
}

UStaticMesh* Prop(const TCHAR* Name)
{
	static TMap<FString, UStaticMesh*> Cache;
	if (UStaticMesh** F = Cache.Find(Name)) { return *F; }
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Map4/Props/%s/%s.%s"), Name, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (M) { M->AddToRoot(); }
	return Cache.Add(Name, M);
}

void ApplySlots(UStaticMeshComponent* C)
{
	const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
	if (!M) { return; }
	const TArray<FStaticMaterial>& Slots = M->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString S = Slots[i].MaterialSlotName.ToString();
		const FLook* L = ForSlot(TEXT(""), 0, S);
		if (!L) { L = Named(*S); }
		if (L)
		{
			C->SetMaterial(i, Get(*L));
			if (i == 0 && L->Step) { C->ComponentTags.AddUnique(FName(L->Step)); }
		}
	}
}

bool Swap(UStaticMeshComponent* C, const TCHAR* Name, const TCHAR* Look, const TCHAR* Tag)
{
	UStaticMesh* M = (C && Enabled()) ? Prop(Name) : nullptr;
	if (!M) { return false; }
	C->SetStaticMesh(M);
	C->EmptyOverrideMaterials();
	if (Look)
	{
		if (const FLook* L = Named(Look)) { C->SetMaterial(0, Get(*L)); }
	}
	else
	{
		ApplySlots(C);
	}
	if (Tag) { C->ComponentTags.AddUnique(FName(Tag)); }
	return true;
}

UStaticMeshComponent* AddProp(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FTransform& Relative, bool bSolid, const TCHAR* Look)
{
	UStaticMesh* M = (Owner && Enabled()) ? Prop(Name) : nullptr;
	if (!M) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(M);
	C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	C->SetRelativeTransform(Relative);
	if (bSolid) { C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); }
	else { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetGenerateOverlapEvents(false); }
	C->SetCanEverAffectNavigation(false);
	if (Look) { if (const FLook* L = Named(Look)) { C->SetMaterial(0, Get(*L)); } }
	else { ApplySlots(C); }
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

UStaticMesh* Shape(int32 Index)
{
	static const TCHAR* Names[] = { TEXT("SM_M4_Box"), TEXT("SM_M4_Ball"), TEXT("SM_M4_Cyl"), TEXT("SM_M4_Cone"), TEXT("SM_M4_Plane"), TEXT("SM_M4_Droplet"),
		TEXT("SM_M4_Speck"), TEXT("SM_M4_Crumb"), TEXT("SM_M4_Pheromone"), TEXT("SM_M4_Pebble") };
	return Prop(Names[FMath::Clamp(Index, 0, (int32)UE_ARRAY_COUNT(Names) - 1)]);
}

}
