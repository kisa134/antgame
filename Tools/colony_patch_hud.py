# One-off: small fixes in AntColonyView.cpp and the hooks of the colony screen into the HUD and the admin panel.
import sys, io, re
ROOT = r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_colony\Source\AntKitchen"

def load(name):
    t = io.open(ROOT + "\\" + name, encoding="utf-8-sig").read()
    return t, ("\r\n" if "\r\n" in t else "\n")

def save(name, t, nl):
    io.open(ROOT + "\\" + name, "w", encoding="utf-8-sig", newline="").write(t.replace("\r\n", "\n").replace("\n", nl))

def rep(t, old, new, count=1):
    n = t.count(old)
    if n != count:
        sys.exit("anchor %r found %d times" % (old[:80], n))
    return t.replace(old, new)

# --- AntColonyView.cpp: the Queen's clock was called C (clashes with AntUi::C), no tick glyph in the fonts
t, nl = load("AntColonyView.cpp")
t = t.replace("\r\n", "\n")
a = t.index("void DrawQueen(")
b = t.index("// ---------------------------------------------------------------------------------------------", t.index("What she says in the show"))
q = t[a:b]
q = q.replace("const float C = S.Clock;", "const float Tm = S.Clock;")
q = re.sub(r"\bC \* ", "Tm * ", q)
q = q.replace("C - S.FaceChangedAt", "Tm - S.FaceChangedAt").replace("S.FaceChangedAt = C;", "S.FaceChangedAt = Tm;")
t = t[:a] + q + t[b:]
t = t.replace('TEXT("✓ ")', 'TEXT("+ ")').replace('TEXT("✓")', 'TEXT("+")')
save("AntColonyView.cpp", t, nl)

# --- AntHUDRaid.cpp: the nest phase shows the colony (or the planning sheet)
t, nl = load("AntHUDRaid.cpp")
t = t.replace("\r\n", "\n")
t = rep(t, '#include "AntHUD.h"\n', '#include "AntHUD.h"\n#include "AntColonyView.h"\n')
t = rep(t, """	if (Raid->Phase == AntRaidPhase::Nest)
	{
		DrawNest(Raid, GS);
		return;
	}""", """	if (Raid->Phase == AntRaidPhase::Nest)
	{
		// The colony («Муравейник», AntColonyView) or the planning sheet (ШТАБ); [Tab] switches.
		if (AntColonyView::IsOn()) { AntColonyView::Draw(this, Canvas, Raid, GS, PC); }
		else { DrawNest(Raid, GS); }
		return;
	}""")
save("AntHUDRaid.cpp", t, nl)

# --- AntHUDNest.cpp: a way back to the colony
t, nl = load("AntHUDNest.cpp")
t = t.replace("\r\n", "\n")
t = rep(t, '#include "AntRaid.h"\n', '#include "AntRaid.h"\n#include "AntColonyView.h"\n')
t = rep(t, """	if (bHost)
	{
		const bool bArmed = Now - NewColonyArmedAt < 4.f;""", """	if (NestBtn(TEXT("МУРАВЕЙНИК  [Tab]"), PX(372.f), PY(18.f), 214.f * U, 44.f * U, true, TEXT("Колония в разрезе: залы, Королева, кладовые, трофеи, летопись"), false, 1.05f)
		|| (PC && PC->WasInputKeyJustPressed(EKeys::Tab)))
	{
		AntColonyView::SetOn(true);
	}
	if (bHost)
	{
		const bool bArmed = Now - NewColonyArmedAt < 4.f;""")
save("AntHUDNest.cpp", t, nl)

# --- AntAdmin.cpp: the tab "Колония"
t, nl = load("AntAdmin.cpp")
t = t.replace("\r\n", "\n")
t = rep(t, 'TEXT("Рейд"), TEXT("Экран") };', 'TEXT("Рейд"), TEXT("Колония"), TEXT("Экран") };')
t = rep(t, """	else   // ---------------------------------------------------------------- screen
	{""", """	else if (Tab == 6)   // ---------------------------------------------------------------- the colony (docs/COLONY.md)
	{
		Section(TEXT("Муравейник (экран гнезда; на этой машине)"));
		Btn(TEXT("Открыть муравейник"), R(TEXT("colony view")), On);
		Btn(TEXT("Штаб (старый экран)"), R(TEXT("colony sheet")));
		Btn(TEXT("Повторить шоу после рейда"), R(TEXT("colony show")), On);
		Btn(TEXT("Королева засыпает"), R(TEXT("colony sleep")));
		Btn(TEXT("Колония: инфо (в лог)"), R(TEXT("colony info")));
		Section(TEXT("Ресурсы"));
		Btn(TEXT("Еда +500"), R(TEXT("colony give food 500")));
		Btn(TEXT("Сахар +200"), R(TEXT("colony give sugar 200")));
		Btn(TEXT("Белок +200"), R(TEXT("colony give protein 200")));
		Btn(TEXT("Жир +200"), R(TEXT("colony give fat 200")));
		Btn(TEXT("Крошки +200"), R(TEXT("colony give crumbs 200")));
		Btn(TEXT("Молочко +10"), R(TEXT("colony give jelly 10")));
		Btn(TEXT("Личинки +3"), R(TEXT("colony give larvae 3")));
		Btn(TEXT("Рабочие +20"), R(TEXT("colony give workers 20")));
		Btn(TEXT("Трофей (случайный)"), R(TEXT("colony give trophy random")), On);
		Btn(TEXT("Все трофеи"), R(TEXT("colony give trophy all")));
		Btn(TEXT("Убрать трофеи"), R(TEXT("colony give trophy none")), Danger);
		Section(TEXT("Залы и ходы (бесплатно)"));
		Btn(TEXT("Прорыть все ходы"), R(TEXT("colony dig all")));
		Btn(TEXT("Все залы I"), R(TEXT("colony setall 1")));
		Btn(TEXT("Все залы II"), R(TEXT("colony setall 2")));
		Btn(TEXT("Все залы III"), R(TEXT("colony setall 3")), On);
		Btn(TEXT("Снести всё"), R(TEXT("colony setall 0")), Danger);
		Section(TEXT("Настроение Королевы"));
		Btn(TEXT("В ярости (10)"), R(TEXT("colony mood 10")), Danger);
		Btn(TEXT("Ворчит (40)"), R(TEXT("colony mood 40")));
		Btn(TEXT("Довольна (70)"), R(TEXT("colony mood 70")), On);
		Btn(TEXT("В восторге (95)"), R(TEXT("colony mood 95")), On);
		Section(TEXT("Рейды на бумаге (доход, Королева, трофеи, стройка; затем шоу)"));
		Btn(TEXT("+1 рейд"), R(TEXT("colony sim 1")), Blue);
		Btn(TEXT("+5 рейдов"), R(TEXT("colony sim 5")), Blue);
		Btn(TEXT("+15 рейдов"), R(TEXT("colony sim 15")), Blue);
		Btn(TEXT("+1 рейд без стройки"), R(TEXT("colony sim 1 nobuild")));
		Section(TEXT("Сброс"));
		Btn(TEXT("Сбросить колонию (старая — colony.old)"), R(TEXT("colony reset")), Danger);
		Btn(TEXT("Записать как старое сохранение v1 (тест миграции)"), R(TEXT("colony v1save")));
		Btn(TEXT("Перечитать сохранение"), R(TEXT("colony reload")));
	}
	else   // ---------------------------------------------------------------- screen
	{""")
save("AntAdmin.cpp", t, nl)
print("ok")
