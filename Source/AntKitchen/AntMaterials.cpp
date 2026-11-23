// Materials and chain reactions 2.0: the material table (docs/MATERIALS.md). The defaults live here; Config/AntMaterials.json
// (written by «mat dump config») overrides them by id at startup, so a designer can tune a number without a build.
#include "AntReact.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TArray<FAntMatProps> GMats;
	bool GLoaded = false;

	FAntMatProps& Add(const TCHAR* Id, const TCHAR* Ru, const TCHAR* Keys)
	{
		FAntMatProps& M = GMats.AddDefaulted_GetRef();
		M.Id = Id;
		M.Ru = Ru;
		FString(Keys).ParseIntoArray(M.Match, TEXT(","), true);
		for (FString& K : M.Match) { K = AntMaterials::Lower(K.TrimStartAndEnd()); }
		return M;
	}

	void Defaults()
	{
		GMats.Reset();
		// Order matters: the first row whose piece is in the name wins (the specific ones before the general ones).
		// ---- food and powders (by the food's Russian name, lower case)
		{ FAntMatProps& M = Add(TEXT("pepper"), TEXT("перец молотый"), TEXT("перец,перечн,pepper,spicejar"));
		  M.bPowder = true; M.Density = 1.2f; M.Stain = TEXT("pepper"); M.Sound = TEXT("Crumbs"); M.Noise = 0.25f;
		  M.Does = TEXT("рассыпается облаком: все в облаке чихают (папа роняет то, что в руке, муравьи в носу вылетают)"); }
		{ FAntMatProps& M = Add(TEXT("flour"), TEXT("мука"), TEXT("мука,муки,flour"));
		  M.bPowder = true; M.Density = 0.6f; M.Absorb = 0.6f; M.Stain = TEXT("flour"); M.Sound = TEXT("Crumbs"); M.Noise = 0.35f;
		  M.Does = TEXT("лавиной с края: белое облако слепит Человека на 5 с, муравьи белеют и оставляют следы, видны даже в темноте"); }
		{ FAntMatProps& M = Add(TEXT("sugar"), TEXT("сахар"), TEXT("сахар,рафинад,sugar"));
		  M.bDissolves = true; M.Density = 1.6f; M.Stain = TEXT("sugar"); M.Sound = TEXT("Crumbs");
		  M.Does = TEXT("в воде растворяется за 4 с: вода сладкая, на сладкое прилетает муха"); }
		{ FAntMatProps& M = Add(TEXT("salt"), TEXT("соль"), TEXT("соль,солон,salt"));
		  M.bDissolves = true; M.bPowder = true; M.Density = 2.1f; M.Stain = TEXT("sugar");
		  M.Does = TEXT("растворяется в воде; рассыпанная — хрустит под лапами"); }
		{ FAntMatProps& M = Add(TEXT("chocolate"), TEXT("шоколад"), TEXT("шоколад,chocolate,choco"));
		  M.MeltAt = 34.f; M.MeltsInto = TEXT("honey"); M.Stain = TEXT("chocolate"); M.Density = 1.3f; M.Sticky = 0.7f; M.HeatTake = 0.6f;
		  M.Does = TEXT("на батарее, конфорке, в микроволновке тает за ~8 с: липкая шоколадная река, Человек вязнет (×0,4)"); }
		{ FAntMatProps& M = Add(TEXT("butter"), TEXT("масло сливочное"), TEXT("сливочн,масло,butter"));
		  M.MeltAt = 32.f; M.MeltsInto = TEXT("oil"); M.Stain = TEXT("oil"); M.Density = 0.9f; M.HeatTake = 0.7f; M.WetSlip = 1.f;
		  M.Does = TEXT("тает в масляную плёнку: скользко, как разлитое масло"); }
		{ FAntMatProps& M = Add(TEXT("icecream"), TEXT("мороженое"), TEXT("морожен,пломбир,icecream"));
		  M.MeltAt = 0.f; M.MeltsInto = TEXT("cola"); M.Stain = TEXT("cream"); M.Density = 0.6f; M.HeatTake = 0.5f; M.Sticky = 0.6f;
		  M.Does = TEXT("тает и при комнатной: сладкая липкая лужа, на неё идут муравьи и муха"); }
		{ FAntMatProps& M = Add(TEXT("ice"), TEXT("лёд"), TEXT("лёд,льда,icetray,icecube"));
		  M.MeltAt = 0.f; M.MeltsInto = TEXT("water"); M.Density = 0.92f; M.HeatTake = 0.4f; M.WetSlip = 0.9f; M.AntGrip = 0.3f; M.Sound = TEXT("CrashGlass"); M.Noise = 0.3f;
		  M.Does = TEXT("холодит муравьёв рядом (4 °C), тает за ~40 с (у тепла быстрее): вода течёт по полу — до удлинителя"); }
		{ FAntMatProps& M = Add(TEXT("pea"), TEXT("горох / виноград"), TEXT("горош,виноград,изюм,sm_m4_pea,grape,marble"));
		  M.bRolls = true; M.Density = 1.05f; M.Elastic = 0.3f; M.HumanFriction = 0.15f;
		  M.Does = TEXT("катится; Человек, наступивший на горошину, падает как на шариках (60 %, на бегу всегда)"); }
		{ FAntMatProps& M = Add(TEXT("bread"), TEXT("хлеб"), TEXT("хлеб,булк,тост,сухар,батон"));
		  M.Absorb = 2.5f; M.Density = 0.35f; M.Sound = TEXT("Crumbs");
		  M.Does = TEXT("плавает; в воде размокает: тяжелеет вдвое и темнеет"); }
		{ FAntMatProps& M = Add(TEXT("crumbs"), TEXT("крошки, хлопья, попкорн"), TEXT("крошк,хлопь,попкорн,чипс,cornflake,popcorn,crumb"));
		  M.Density = 0.4f; M.Absorb = 1.f; M.Sound = TEXT("Crumbs");
		  M.Does = TEXT("плавает и уплывает по течению (плот для муравья)"); }
		{ FAntMatProps& M = Add(TEXT("jam"), TEXT("мёд, варенье"), TEXT("мёд,мед,варень,джем"));
		  M.Sticky = 1.f; M.Density = 1.4f; M.Stain = TEXT("jam"); M.Sound = TEXT("SplatJam");
		  M.Does = TEXT("липко: муравьи вязнут, Человек шагает медленно и громко; в холоде густеет (задумано)"); }
		{ FAntMatProps& M = Add(TEXT("heavyfood"), TEXT("сыр, колбаса, конфеты"), TEXT("сыр,колбас,конфет,орех,мяс,котлет,монет"));
		  M.Density = 1.3f;
		  M.Does = TEXT("тонет"); }
		// ---- liquids (what lies on a floor)
		{ FAntMatProps& M = Add(TEXT("oil"), TEXT("масло растительное"), TEXT("oil,подсолн"));
		  M.WetSlip = 1.f; M.Density = 0.92f; M.AntGrip = 0.2f; M.Stain = TEXT("oil");
		  M.Does = TEXT("плёнка поверх воды; самый скользкий пол: падает и Человек, и кошка"); }
		{ FAntMatProps& M = Add(TEXT("cola"), TEXT("кола"), TEXT("кола,cola,soda"));
		  M.Sticky = 0.6f; M.Density = 1.05f; M.Stain = TEXT("cola"); M.Conduct = 0.9f;
		  M.Does = TEXT("лужа липнет, высохнув; с ментосом — гейзер"); }
		{ FAntMatProps& M = Add(TEXT("water"), TEXT("вода"), TEXT("water,вода,brine,рассол,soup"));
		  M.WetSlip = 0.8f; M.Conduct = 1.f; M.Density = 1.f;
		  M.Does = TEXT("течёт, несёт лёгкое, смывает грязь; до розетки или удлинителя — замыкание и темнота"); }
		// ---- the flat's own materials (mesh names, map v4 look keys, Surf_ tags)
		{ FAntMatProps& M = Add(TEXT("magnet"), TEXT("магнит"), TEXT("magnet,магнит"));
		  M.bMagnetic = true; M.Density = 5.f; M.Conduct = 0.6f; M.Sound = TEXT("CrashMetal"); M.Noise = 0.2f;
		  M.Does = TEXT("держит рисунки на холодильнике; упавший — тянет ложки и монетки, робот-пылесос на нём глохнет"); }
		{ FAntMatProps& M = Add(TEXT("cable"), TEXT("провод, удлинитель"), TEXT("wire,cable,charger,powerstrip,провод,удлинит"));
		  M.Conduct = 1.f; M.AntGrip = 1.1f; M.Density = 1.4f; M.Sound = TEXT("Zap");
		  M.Does = TEXT("под током: перегрызть — искра, удар и КЗ; вода у удлинителя — КЗ, щиток выбивает"); }
		{ FAntMatProps& M = Add(TEXT("sponge"), TEXT("губка"), TEXT("sponge,губк"));
		  M.Absorb = 10.f; M.Density = 0.3f; M.Elastic = 0.6f; M.AntGrip = 1.4f; M.Sound = TEXT("SoftThud");
		  M.Does = TEXT("впитывает воду в 10 раз больше себя: тяжелеет, темнеет, капает; тяжёлая — падает с края"); }
		{ FAntMatProps& M = Add(TEXT("papertowel"), TEXT("бумажное полотенце"), TEXT("papertowel,полотенц"));
		  M.Absorb = 6.f; M.Density = 0.5f; M.AntGrip = 1.3f;
		  M.Does = TEXT("впитывает лужу, размокает и рвётся"); }
		{ FAntMatProps& M = Add(TEXT("latex"), TEXT("латекс (шарик)"), TEXT("balloon,latex,шарик"));
		  M.Elastic = 0.9f; M.Static = 0.9f; M.Density = 0.1f; M.AntGrip = 0.8f; M.Sound = TEXT("Splat"); M.Noise = 1.f;
		  M.Does = TEXT("лопается от кислоты и укуса (шум 1,0 — все идут смотреть); натёртый о ковёр — липнет к потолку"); }
		{ FAntMatProps& M = Add(TEXT("rubber"), TEXT("резина"), TEXT("rubber,slingshot,band,резин,tennisball"));
		  M.Elastic = 1.f; M.HumanFriction = 1.3f; M.AntGrip = 1.6f; M.Density = 1.1f; M.Sound = TEXT("Jump");
		  M.Does = TEXT("пружинит: рогатка малыша — катапульта для отряда на стол"); }
		{ FAntMatProps& M = Add(TEXT("glass"), TEXT("стекло"), TEXT("glass,jar,банк,vase,window,lampglass,shelfglass,стакан,glasses"));
		  M.AntGrip = 0.35f; M.HumanFriction = 0.6f; M.WetSlip = 0.7f; M.Density = 2.5f; M.HeatTake = 0.3f; M.Breaks = TEXT("glass"); M.Sound = TEXT("CrashGlass"); M.Noise = 1.f;
		  M.Does = TEXT("бьётся: осколки режут муравьёв (10/с), содержимое вытекает; тонет"); }
		{ FAntMatProps& M = Add(TEXT("ceramic"), TEXT("фарфор, керамика"), TEXT("plate,mug,cup,teapot,bowl,ceramic,фарфор,тарелк,чашк"));
		  M.AntGrip = 0.5f; M.HumanFriction = 0.7f; M.WetSlip = 0.6f; M.Density = 2.3f; M.Breaks = TEXT("ceramic"); M.Sound = TEXT("CrashCeramic"); M.Noise = 0.9f;
		  M.Does = TEXT("бьётся на 3–5 черепков: укрытие и ступени"); }
		{ FAntMatProps& M = Add(TEXT("terracotta"), TEXT("терракота"), TEXT("plantpot,terracotta,горшок,pottedplant"));
		  M.AntGrip = 1.1f; M.Absorb = 0.15f; M.Density = 1.9f; M.Breaks = TEXT("terracotta"); M.Sound = TEXT("CrashCeramic"); M.Noise = 0.8f;
		  M.Does = TEXT("бьётся: горка земли — пандус и укрытие"); }
		{ FAntMatProps& M = Add(TEXT("tile"), TEXT("плитка"), TEXT("tile,grout,floor,плитк"));
		  M.AntGrip = 0.8f; M.HumanFriction = 0.8f; M.WetSlip = 0.85f; M.Density = 2.3f; M.HeatTake = 0.5f; M.Sound = TEXT("Step");
		  M.Does = TEXT("мокрая плитка — самое скользкое место для Человека после масла"); }
		{ FAntMatProps& M = Add(TEXT("metal"), TEXT("металл"), TEXT("brushed,chrome,steel,iron,metal,burner,sm_k_range,sink,faucet,spoon,fork,knife,sm_k_pot,sm_m4_pan,ladle,radiator,toaster,kettle,brass,surf_metal,ложк,нож,кастрюл,батаре"));
		  M.AntGrip = 0.6f; M.HumanFriction = 0.7f; M.WetSlip = 0.5f; M.Density = 7.8f; M.Conduct = 1.f; M.HeatTake = 0.9f; M.bMagnetic = true; M.Sound = TEXT("CrashMetal"); M.Noise = 0.9f;
		  M.Does = TEXT("громко падает (шум 0,9); проводит ток и тепло: ложка в горячем супе жжётся; тонет; магнит держит"); }
		{ FAntMatProps& M = Add(TEXT("enamel"), TEXT("эмаль, техника"), TEXT("enamel,fridge,microwave,dishwasher,oven,appliance,screen,холодильн"));
		  M.AntGrip = 0.5f; M.HumanFriction = 0.7f; M.WetSlip = 0.6f; M.Density = 3.f; M.Conduct = 0.2f; M.HeatTake = 0.5f; M.bMagnetic = true; M.Sound = TEXT("CrashMetal"); M.Noise = 0.7f;
		  M.Does = TEXT("гладкая: муравей держится хуже; магниты держатся; работает, пока есть свет"); }
		{ FAntMatProps& M = Add(TEXT("pet"), TEXT("пластиковая бутылка (ПЭТ)"), TEXT("petbottle,petclear,bottle,бутылк,colabottle"));
		  M.AntGrip = 0.5f; M.Density = 0.95f; M.Elastic = 0.5f; M.bRolls = true; M.Sound = TEXT("CrashPlastic"); M.Noise = 0.4f;
		  M.Does = TEXT("не бьётся; лёжа катится; открытая — льёт 20 с; кола + ментос — гейзер и ракета"); }
		{ FAntMatProps& M = Add(TEXT("plastic"), TEXT("пластик"), TEXT("plastic,sm_k_bin,bucket,toy,block,кубик,fridgeliner,plasticblue,bingreen,trashbag,outlet,switch,socket"));
		  M.AntGrip = 0.7f; M.Density = 0.95f; M.Elastic = 0.4f; M.Static = 0.3f; M.Sound = TEXT("CrashPlastic"); M.Noise = 0.5f;
		  M.Does = TEXT("отскакивает, не бьётся; кубики — кегли"); }
		{ FAntMatProps& M = Add(TEXT("cardboard"), TEXT("картон"), TEXT("cardboard,cereal,box,pasta,коробк,картон,pizza,waffle,giftwrap"));
		  M.AntGrip = 1.1f; M.Absorb = 0.8f; M.Density = 0.7f; M.Sound = TEXT("SoftThud"); M.Noise = 0.3f;
		  M.Does = TEXT("муравьи прогрызают дыру за 8 с; мнётся; мокрый — размокает"); }
		{ FAntMatProps& M = Add(TEXT("paper"), TEXT("бумага"), TEXT("paper,newspaper,note,drawing,sm_l_book_,calendar,бумаг,газет,книг,рисун"));
		  M.AntGrip = 1.f; M.Absorb = 1.2f; M.Density = 0.8f; M.Static = 0.2f; M.Sound = TEXT("SoftThud"); M.Noise = 0.3f;
		  M.Does = TEXT("рвётся по укусам, падая планирует; книги на полке — домино"); }
		{ FAntMatProps& M = Add(TEXT("wool"), TEXT("шерсть, ковёр"), TEXT("carpet,rug,wool,knit,yarn,sofawool,boucle,jersey,cushion,sock,сукн,ковр,плед,пряж,dust"));
		  M.AntGrip = 1.5f; M.HumanFriction = 1.2f; M.Absorb = 2.f; M.Static = 1.f; M.Density = 0.4f; M.Sound = TEXT("StepFabric"); M.Noise = 0.1f;
		  M.Does = TEXT("цепко; гасит падения; натереть о ковёр шарик — статика"); }
		{ FAntMatProps& M = Add(TEXT("cloth"), TEXT("ткань"), TEXT("fabric,terry,towel,curtain,linen,cloth,gingham,скатерт,штор,ткан,лён,throw"));
		  M.AntGrip = 1.4f; M.Absorb = 3.f; M.Static = 0.5f; M.Density = 0.5f; M.Sound = TEXT("StepFabric"); M.Noise = 0.1f;
		  M.Does = TEXT("лезут как по стене; впитывает пролитое (мокнет, тяжелеет); рвётся"); }
		{ FAntMatProps& M = Add(TEXT("leather"), TEXT("кожа"), TEXT("leather,slipper,shoe,тапок,ботин"));
		  M.AntGrip = 1.1f; M.Density = 0.9f; M.Sound = TEXT("SoftThud");
		  M.Does = TEXT("укрытие (тапок, ботинок)"); }
		{ FAntMatProps& M = Add(TEXT("wood"), TEXT("дерево"), TEXT("oak,walnut,butcher,paintwood,cabinet,wood,table,chair,bookshelf,shelf,door,frame,parquet,surf_wood,дерев,полк,стол,стул"));
		  M.AntGrip = 1.f; M.HumanFriction = 0.9f; M.WetSlip = 0.3f; M.Absorb = 0.05f; M.Density = 0.6f; M.HeatTake = 0.2f; M.Breaks = TEXT("wood"); M.Sound = TEXT("CrashWood"); M.Noise = 0.6f;
		  M.Does = TEXT("держится на крепеже; плавает; ломается стадиями"); }
		{ FAntMatProps& M = Add(TEXT("soil"), TEXT("земля"), TEXT("soil,potting,земл,kibble,mud"));
		  M.AntGrip = 1.2f; M.Absorb = 1.5f; M.Density = 1.5f; M.Stain = TEXT("mud"); M.Sound = TEXT("StepSoil");
		  M.Does = TEXT("мокрая — грязь: следы лапок"); }
		{ FAntMatProps& M = Add(TEXT("plant"), TEXT("растение, трава"), TEXT("grass,leaf,plant,bark,flowerbed,лист,трав"));
		  M.AntGrip = 1.2f; M.Absorb = 0.5f; M.Density = 0.7f; M.Sound = TEXT("StepGrass");
		  M.Does = TEXT("цепко; гасит падения"); }
		{ FAntMatProps& M = Add(TEXT("stone"), TEXT("камень, бетон"), TEXT("concrete,asphalt,pavement,stone,kerb,plaster,бетон,камен"));
		  M.AntGrip = 1.1f; M.HumanFriction = 1.f; M.WetSlip = 0.4f; M.Density = 2.4f; M.HeatTake = 0.4f; M.Sound = TEXT("Step");
		  M.Does = TEXT("ничего не делает — фон"); }
	}

	FString Join(const TArray<FString>& A) { return FString::Join(A, TEXT(",")); }
}

namespace AntMaterials
{

FString Lower(const FString& S)
{
	FString O = S.ToLower();
	for (TCHAR& C : O)
	{
		if (C >= 0x0410 && C <= 0x042F) { C = (TCHAR)(C + 0x20); }   // А..Я -> а..я
		else if (C == 0x0401) { C = 0x0451; }                         // Ё -> ё
	}
	return O;
}

void Load()
{
	Defaults();
	GLoaded = true;
	const FString Path = FPaths::ProjectConfigDir() / TEXT("AntMaterials.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMAT %d materials (code defaults; no %s)"), GMats.Num(), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTMAT %s is not valid JSON: code defaults"), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetArrayField(TEXT("materials"), Rows))
	{
		return;
	}
	int32 Changed = 0, Added = 0;
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
		FString Id;
		if (!(*O)->TryGetStringField(TEXT("id"), Id)) { continue; }
		FAntMatProps* M = GMats.FindByPredicate([&Id](const FAntMatProps& X) { return X.Id == Id; });
		if (!M) { M = &GMats.AddDefaulted_GetRef(); M->Id = Id; ++Added; }
		else { ++Changed; }
		const TSharedPtr<FJsonObject>& J = *O;
		J->TryGetStringField(TEXT("ru"), M->Ru);
		FString Keys;
		if (J->TryGetStringField(TEXT("match"), Keys)) { M->Match.Reset(); Keys.ParseIntoArray(M->Match, TEXT(","), true); for (FString& K : M->Match) { K = Lower(K.TrimStartAndEnd()); } }
		auto F = [&J](const TCHAR* K, float& Out) { double D; if (J->TryGetNumberField(K, D)) { Out = (float)D; } };
		auto B = [&J](const TCHAR* K, bool& Out) { bool X; if (J->TryGetBoolField(K, X)) { Out = X; } };
		F(TEXT("ant_grip"), M->AntGrip); F(TEXT("human_friction"), M->HumanFriction); F(TEXT("wet_slip"), M->WetSlip); F(TEXT("sticky"), M->Sticky);
		F(TEXT("absorb"), M->Absorb); F(TEXT("density"), M->Density); F(TEXT("conduct"), M->Conduct); F(TEXT("heat_take"), M->HeatTake);
		F(TEXT("melt_at"), M->MeltAt); F(TEXT("static"), M->Static); F(TEXT("elastic"), M->Elastic); F(TEXT("noise"), M->Noise);
		B(TEXT("dissolves"), M->bDissolves); B(TEXT("powder"), M->bPowder); B(TEXT("rolls"), M->bRolls); B(TEXT("magnetic"), M->bMagnetic);
		J->TryGetStringField(TEXT("melts_into"), M->MeltsInto); J->TryGetStringField(TEXT("stain"), M->Stain); J->TryGetStringField(TEXT("breaks"), M->Breaks);
		J->TryGetStringField(TEXT("sound"), M->Sound); J->TryGetStringField(TEXT("does"), M->Does);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMAT %d materials (%s: %d tuned, %d added)"), GMats.Num(), *Path, Changed, Added);
}

TConstArrayView<FAntMatProps> All()
{
	if (!GLoaded) { Load(); }
	return GMats;
}

const FAntMatProps* Find(const FString& Id)
{
	for (const FAntMatProps& M : All()) { if (M.Id.Equals(Id, ESearchCase::IgnoreCase)) { return &M; } }
	return nullptr;
}

const FAntMatProps* ForName(const FString& Name)
{
	if (Name.IsEmpty()) { return nullptr; }
	const FString L = Lower(Name);
	for (const FAntMatProps& M : All())
	{
		for (const FString& K : M.Match)
		{
			if (!K.IsEmpty() && L.Contains(K)) { return &M; }
		}
	}
	return nullptr;
}

const FAntMatProps* ForComponent(const UPrimitiveComponent* C, FString* OutWhy)
{
	if (!C) { return nullptr; }
	// 1. the mesh's own name (most specific: SM_K_Sponge, SM_V3_GlassShards...)
	if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
	{
		if (const UStaticMesh* Mesh = S->GetStaticMesh())
		{
			if (const FAntMatProps* M = ForName(Mesh->GetName()))
			{
				if (OutWhy) { *OutWhy = FString::Printf(TEXT("меш %s"), *Mesh->GetName()); }
				return M;
			}
		}
		// 2. the look of its first material (map v4: MI_M4_<Look> / a slot named after the look)
		for (int32 i = 0; i < FMath::Min(3, S->GetNumMaterials()); ++i)
		{
			if (const UMaterialInterface* Mat = S->GetMaterial(i))
			{
				if (const FAntMatProps* M = ForName(Mat->GetName()))
				{
					if (OutWhy) { *OutWhy = FString::Printf(TEXT("материал %s"), *Mat->GetName()); }
					return M;
				}
			}
		}
	}
	// 3. its tags (Surf_Wood, Surf_Metal...)
	for (const FName& T : C->ComponentTags)
	{
		if (const FAntMatProps* M = ForName(T.ToString()))
		{
			if (OutWhy) { *OutWhy = FString::Printf(TEXT("тег %s"), *T.ToString()); }
			return M;
		}
	}
	// 4. the owner's name (a fixture, a loot "Шоколадный батончик")
	if (const AActor* A = C->GetOwner())
	{
		if (const FAntMatProps* M = ForName(A->GetName()))
		{
			if (OutWhy) { *OutWhy = FString::Printf(TEXT("предмет %s"), *A->GetName()); }
			return M;
		}
	}
	return nullptr;
}

float FloatOf(const FString& LootName)
{
	// lift per weight: > 1 floats, < 1 sinks (UAntFluidWorld's scale: 1.5 rides on top, 0.6 goes down)
	const FAntMatProps* M = ForName(LootName);
	if (!M) { return 1.5f; }
	return M->Density < 1.f ? 1.5f + (1.f - M->Density) * 0.6f : FMath::Clamp(0.75f - 0.1f * (M->Density - 1.f), 0.45f, 0.7f);
}

FString Describe(const FAntMatProps& M)
{
	TArray<FString> P;
	P.Add(FString::Printf(TEXT("сцепление муравья %.2f"), M.AntGrip));
	P.Add(FString::Printf(TEXT("трение тапка %.2f"), M.HumanFriction));
	if (M.WetSlip > 0.f) { P.Add(FString::Printf(TEXT("мокрое скользит %.0f%%"), M.WetSlip * 100.f)); }
	if (M.Sticky > 0.f) { P.Add(FString::Printf(TEXT("липкость %.1f"), M.Sticky)); }
	if (M.Absorb > 0.f) { P.Add(FString::Printf(TEXT("впитывает ×%.1f"), M.Absorb)); }
	P.Add(M.Density < 1.f ? FString::Printf(TEXT("плавает (%.2f)"), M.Density) : FString::Printf(TEXT("тонет (%.2f)"), M.Density));
	if (M.Conduct > 0.f) { P.Add(FString::Printf(TEXT("ток %.0f%%"), M.Conduct * 100.f)); }
	if (M.MeltAt > -999.f) { P.Add(FString::Printf(TEXT("тает при %.0f °C → %s"), M.MeltAt, *M.MeltsInto)); }
	if (M.bDissolves) { P.Add(TEXT("растворяется")); }
	if (M.bPowder) { P.Add(TEXT("порошок (облако)")); }
	if (M.bRolls) { P.Add(TEXT("катится")); }
	if (M.bMagnetic) { P.Add(TEXT("магнитится")); }
	if (M.Static > 0.f) { P.Add(FString::Printf(TEXT("статика %.1f"), M.Static)); }
	if (M.Elastic > 0.f) { P.Add(FString::Printf(TEXT("упругость %.1f"), M.Elastic)); }
	if (!M.Breaks.IsEmpty()) { P.Add(FString::Printf(TEXT("бьётся как «%s»"), *M.Breaks)); }
	if (M.Noise > 0.f) { P.Add(FString::Printf(TEXT("шум падения %.1f"), M.Noise)); }
	return FString::Printf(TEXT("%s [%s]: %s. %s"), *M.Ru, *M.Id, *FString::Join(P, TEXT(", ")), *M.Does);
}

bool DumpJson(const FString& Path)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("doc"), TEXT("Ant Kitchen material table (docs/MATERIALS.md). Read at startup over the code defaults (AntMaterials.cpp), by id."));
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FAntMatProps& M : All())
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), M.Id);
		J->SetStringField(TEXT("ru"), M.Ru);
		J->SetStringField(TEXT("match"), Join(M.Match));
		J->SetNumberField(TEXT("ant_grip"), M.AntGrip);
		J->SetNumberField(TEXT("human_friction"), M.HumanFriction);
		J->SetNumberField(TEXT("wet_slip"), M.WetSlip);
		J->SetNumberField(TEXT("sticky"), M.Sticky);
		J->SetNumberField(TEXT("absorb"), M.Absorb);
		J->SetNumberField(TEXT("density"), M.Density);
		J->SetNumberField(TEXT("conduct"), M.Conduct);
		J->SetNumberField(TEXT("heat_take"), M.HeatTake);
		J->SetNumberField(TEXT("melt_at"), M.MeltAt);
		J->SetStringField(TEXT("melts_into"), M.MeltsInto);
		J->SetStringField(TEXT("stain"), M.Stain);
		J->SetBoolField(TEXT("dissolves"), M.bDissolves);
		J->SetBoolField(TEXT("powder"), M.bPowder);
		J->SetBoolField(TEXT("rolls"), M.bRolls);
		J->SetBoolField(TEXT("magnetic"), M.bMagnetic);
		J->SetNumberField(TEXT("static"), M.Static);
		J->SetNumberField(TEXT("elastic"), M.Elastic);
		J->SetStringField(TEXT("breaks"), M.Breaks);
		J->SetStringField(TEXT("sound"), M.Sound);
		J->SetNumberField(TEXT("noise"), M.Noise);
		J->SetStringField(TEXT("does"), M.Does);
		Rows.Add(MakeShared<FJsonValueObject>(J));
	}
	Root->SetArrayField(TEXT("materials"), Rows);
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, W);
	return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

}
