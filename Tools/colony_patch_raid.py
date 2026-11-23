# One-off: wires the colony (AntColony) into AntRaid.cpp. Fails loudly if an anchor is missing.
import sys, io
P = r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_colony\Source\AntKitchen\AntRaid.cpp"
t = io.open(P, encoding="utf-8-sig").read()
nl = "\r\n" if "\r\n" in t else "\n"
t = t.replace("\r\n", "\n")

def rep(old, new, count=1):
    global t
    n = t.count(old)
    if n != count:
        sys.exit("anchor %r found %d times" % (old[:70], n))
    t = t.replace(old, new)

rep('#include "AntSnowball.h"\n', '#include "AntSnowball.h"\n#include "AntColony.h"\n')

# A new raid: the colony's per-raid counters.
rep("\tVariety.Reset();\n\tDeaths = 0;\n", "\tVariety.Reset();\n\tDeaths = 0;\n\tFMemory::Memzero(RaidKinds);\n\tRaidLines.Reset();\n\tRaidTrophies.Reset();\n\tbCakeWasInHill = false;\n")

# The cake in the red hill (a trophy if it is taken back).
rep("\tCake = AntCake::InHill;\n\tCakeLoc = Top;\n", "\tCake = AntCake::InHill;\n\tbCakeWasInHill = true;\n\tCakeLoc = Top;\n")

# Deliveries: kinds, who carried, whole trophies, the trophy hall's bonus.
rep("""	Variety.Add(Food->LootName);
	FString Names;
	for (const AAntCharacter* A : Carriers)
	{
		if (A) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); }
	}
	if (Food->bMain)
	{""", """	Variety.Add(Food->LootName);
	FString Names;
	int32 NumCarriers = 0;
	for (const AAntCharacter* A : Carriers)
	{
		if (A) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); ++NumCarriers; }
	}
	// The colony: what kind of food it was and who brought it (the cake's value is counted at the end, by the tier).
	const uint8 Kind = AntColony::KindOf(Food->LootName);
	auto Credit = [&](int32 Amount, const TCHAR* Feat)
	{
		for (const AAntCharacter* A : Carriers)
		{
			if (!A) { continue; }
			FAntRaidPlayerLine& L = LineOf(A);
			L.Food[Kind] += Amount / FMath::Max(1, NumCarriers);
			if (Feat) { L.Feats.AddUnique(Feat); }
		}
	};
	if (Food->bMain)
	{
		Credit(0, TEXT("cake_home"));""")
rep("""		++TrophiesHome;
		RaidJelly += 1;""", """		++TrophiesHome;
		RaidJelly += 1;
		const FString TN = Food->LootName.ToLower();
		RaidTrophies.AddUnique(TN.Contains(TEXT("мишк")) ? 6 : TN.Contains(TEXT("шоколад")) ? 7 : TN.Contains(TEXT("сыр")) ? 8 : 255);""")
rep("	Value = FMath::RoundToInt(Value * TierDef().LootMul);\n", "	Value = FMath::RoundToInt(Value * TierDef().LootMul * AntColony::LootBonus(Nest));\n")
rep("""		UE_LOG(LogTemp, Display, TEXT("ANTRAID special find home: %d «%s» (%s) by %s"), (int32)Food->Special, *Food->LootName, *What, *Names);
	}
	return false;""", """		UE_LOG(LogTemp, Display, TEXT("ANTRAID special find home: %d «%s» (%s) by %s"), (int32)Food->Special, *Food->LootName, *What, *Names);
		RaidTrophies.AddUnique(9);
	}
	RaidKinds[Kind] += Value;
	Credit(Value, Food->bTrophy ? TEXT("trophy_home") : Food->Special != 0 ? TEXT("special_home") : nullptr);
	return false;""")

# Bodies home, deaths.
rep("	AntSquad::OnBodyHome(BodyOwner);   // what is left of his squad is his again\n", """	AntSquad::OnBodyHome(BodyOwner);   // what is left of his squad is his again
	for (const AAntCharacter* A : Carriers) { if (A) { LineOf(A).Rescues++; } }
""")
rep("""	++Deaths;
	AntSquad::OnCommanderDied(Ant);""", """	++Deaths;
	LineOf(Ant).Deaths++;
	AntColony::AddFallen(Nest, Ant->GetDisplayName(), RaidNo, How);
	AntSquad::OnCommanderDied(Ant);""")

# Orders of the Queen: the radio pays double.
rep("	return FMath::RoundToInt(40.f * TierDef().LootMul);\n}", "	return AntColony::OrderFood(Nest, FMath::RoundToInt(40.f * TierDef().LootMul));\n}")

# The end of a raid.
rep("""	R.Kept = bWiped ? R.Banked / 2 : R.Banked;""", """	R.Kept = bWiped ? FMath::RoundToInt(R.Banked * AntColony::WipeKeep(Nest)) : R.Banked;
	AntColony::Sync(Nest);
	R.StockBefore = Nest.Stock;
	R.JellyBefore = Nest.Jelly;""")
rep("""	Mood -= FMath::Min(8, Deaths * 2);
	Mood -= bWiped ? 14 : 0;""", """	Mood -= AntColony::DeathMood(Nest, Deaths);
	Mood -= bWiped ? 14 : 0;
	Mood += AntColony::MoodPerRaid(Nest);""")
rep("""	// The stores.
	Nest.Food += R.Kept;""", """	// The stores, by kind: what the carriers brought, the cake (sugar), the rest (orders, refunds, untracked) as crumbs.
	{
		int32 K[AntFoodKind::Count];
		FMemory::Memcpy(K, RaidKinds, sizeof(K));
		if (Cake == AntCake::Home) { K[AntFoodKind::Sugar] += FMath::Max(0, R.Banked - K[0] - K[1] - K[2] - K[3]); }
		int32 Tracked = K[0] + K[1] + K[2] + K[3];
		if (Tracked < R.Banked) { K[AntFoodKind::Crumbs] += R.Banked - Tracked; Tracked = R.Banked; }
		R.Kinds.SetNumZeroed(AntFoodKind::Count);
		int32 Given = 0;
		for (int32 k = 0; k < AntFoodKind::Count; ++k) { R.Kinds[k] = Tracked > 0 ? (int32)((int64)K[k] * R.Kept / Tracked) : 0; Given += R.Kinds[k]; }
		R.Kinds[AntFoodKind::Crumbs] += R.Kept - Given;
		for (int32 k = 0; k < AntFoodKind::Count; ++k) { AntColony::AddFood(Nest, (uint8)k, R.Kinds[k]); }
	}""")
rep("""	if (Nest.Larvae + R.LarvaeLaid < 1) { R.LarvaeLaid = 1; }   // never a dead end""", """	R.LarvaeLaid += (Nest.Own_(AntColony::OwnMemorial) >= 3 && Deaths > 0) ? 1 : 0;   // the guard of honour
	if (Nest.Larvae + R.LarvaeLaid < 1) { R.LarvaeLaid = 1; }   // never a dead end""")
rep("""	while (R.Story.Num() > 9) { R.Story.RemoveAt(R.Story.Num() / 2); }   // keep the beginning and the end
	Result = R;""", """	while (R.Story.Num() > 9) { R.Story.RemoveAt(R.Story.Num() / 2); }   // keep the beginning and the end
	ColonyAfterRaid(R, How);
	Result = R;""")

# Saving keeps the stores by kind in step.
rep("""void AAntRaid::SaveNest()
{
	const bool bOk""", """void AAntRaid::SaveNest()
{
	AntColony::Sync(Nest);
	const bool bOk""")

# Commands: Buy and Mod had slipped under the seed's case; the colony's own commands.
rep("""	case AntRaidCmd::Buy:
	case AntRaidCmd::Mod:
	case AntRaidCmd::Seed:""", """	case AntRaidCmd::Seed:""")
rep("""	case AntRaidCmd::Tier:
	case AntRaidCmd::Contract:""", """	case AntRaidCmd::Buy:
	case AntRaidCmd::Mod:
	case AntRaidCmd::Dig:
	case AntRaidCmd::Chamber:
	case AntRaidCmd::Tier:
	case AntRaidCmd::Contract:""")
rep("""		else if (Cmd == AntRaidCmd::Mod)
		{""", """		else if (Cmd == AntRaidCmd::Dig)
		{
			FString Why;
			if (AntColony::CanDig(Nest, (uint8)FMath::Clamp(A, 0, 255), &Why))
			{
				AntColony::DoDig(Nest, (uint8)A);
				const AntColony::FDigDef& D = AntColony::Dig((uint8)A);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: прорыт %s — %s. %s"), Nest.Raids, D.Name, D.Toward, D.Text));
				Say(FString::Printf(TEXT("ПРОРЫТ %s (%s): %s"), D.Name, D.Toward, D.Text), RaidGold);
				UE_LOG(LogTemp, Display, TEXT("ANTCOLONY dug %d %s: food %d jelly %d left"), A, D.Name, Nest.Food, Nest.Jelly);
				SaveNest();
			}
			else
			{
				PC->ClientToast(FString::Printf(TEXT("Не прорыть: %s"), *Why));
			}
		}
		else if (Cmd == AntRaidCmd::Chamber)
		{
			FString Why;
			const uint8 C = (uint8)FMath::Clamp(A, 0, 255);
			if (C < AntColony::Count && AntColony::CanUpgrade(Nest, C, &Why))
			{
				AntColony::Upgrade(Nest, C);
				const int32 T = AntColony::TierOf(Nest, C);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: построено — %s %d «%s»"), Nest.Raids, AntColony::Def(C).Name, T, *AntColony::TierName(C, T)));
				Say(FString::Printf(TEXT("ПОСТРОЕНО: %s %d — «%s». %s"), AntColony::Def(C).Name, T, *AntColony::TierName(C, T), *AntColony::TierText(C, T)), RaidGold);
				UE_LOG(LogTemp, Display, TEXT("ANTCOLONY built %s tier %d: food %d jelly %d left, level %d"), AntColony::Def(C).Id, T, Nest.Food, Nest.Jelly, AntColony::Level(Nest));
				SaveNest();
			}
			else
			{
				PC->ClientToast(FString::Printf(TEXT("Не построить: %s"), *Why));
			}
		}
		else if (Cmd == AntRaidCmd::Mod)
		{""")
# Buying a branch from the old sheet also goes into the chronicle.
rep("""				UE_LOG(LogTemp, Display, TEXT("ANTNEST built branch %d tier %d «%s»: food %d jelly %d left"), A, (int32)Nest.Tier_((uint8)A), D.Name, Nest.Food, Nest.Jelly);""",
    """				UE_LOG(LogTemp, Display, TEXT("ANTNEST built branch %d tier %d «%s»: food %d jelly %d left"), A, (int32)Nest.Tier_((uint8)A), D.Name, Nest.Food, Nest.Jelly);
				const uint8 Room = AntColony::ChamberOfBranch((uint8)A);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: построено — %s %d «%s»"), Nest.Raids, AntColony::Def(Room).Name, (int32)Nest.Tier_((uint8)A), D.Name));""")
rep("""			AntNest::Fresh(Nest);
			Result = FAntRaidResult();""", """			AntNest::Fresh(Nest);
			AntColony::Chronicle(Nest, TEXT("Основана Великая Колония Щели. Королева-Мать заняла земляной трон"));
			Result = FAntRaidResult();""")

# Dev: "colony ..."
rep("""	if (Op == TEXT("nest"))
	{
		const int32 N = (int32)Num(2);""", """	if (Op == TEXT("colony"))
	{
		return ColonyCommand(A);
	}
	if (Op == TEXT("nest"))
	{
		const int32 N = (int32)Num(2);""")

io.open(P, "w", encoding="utf-8-sig", newline="").write(t.replace("\n", nl))
print("ok")
