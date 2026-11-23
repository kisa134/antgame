# One-off: a slimmer bottom bar (the chronicle's last line), tooltips by the mouse, the bottom row of chambers lifted above it.
import io, sys
ROOT = r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_colony\Source\AntKitchen"
P = ROOT + r"\AntColonyView.cpp"
t = io.open(P, encoding="utf-8-sig").read()
nl = "\r\n" if "\r\n" in t else "\n"
t = t.replace("\r\n", "\n")
a = t.index("	void DrawBottom(")
b = t.index("\n	}\n", a) + 4
NEW = '''	void DrawBottom(UCanvas* Cv, const FAntNestState& N, const AAntRaid* Raid, float W, float H, float Fs, bool bHost)
	{
		FState& S = St();
		// A strip of receipt paper: the chronicle's last line (the rest is in the radio's card and the save).
		const float BH = 46.f * Fs, Y = H - BH - 6.f * Fs, PW = W * 0.56f;
		AntUiCanvas::Paper(Cv, 14.f * Fs, Y, PW, BH, false);
		AntUiCanvas::Text(Cv, TEXT("ЛЕТОПИСЬ"), 28.f * Fs, Y + 13.f * Fs, EFont::Display, 13.f * Fs, C::JamDeep);
		const FString Last = N.Chronicle.Num() > 0 ? N.Chronicle.Last() : FString(TEXT("Пусто. Первая запись появится после первого рейда."));
		AntUiCanvas::TextFit(Cv, Last, 128.f * Fs, Y + 13.f * Fs, PW - 128.f * Fs, EFont::Mono, 13.f * Fs, C::Graphite);
		const FString Hint = !Raid->NestNote.IsEmpty() ? Raid->NestNote
			: bHost ? FString(TEXT("Клик по залу — карточка и стройка · колесо — зум · тяни — двигать · пунктир — ещё не прорыто"))
			: FString(TEXT("Колония хоста: строит он · клик по залу — карточка · колесо — зум · тяни — двигать"));
		AntUiCanvas::Rect(Cv, PW + 26.f * Fs, Y, W - PW - 40.f * Fs, BH, FLinearColor(0.012f, 0.006f, 0.003f, 0.72f));
		AntUiCanvas::Wrap(Cv, Hint, PW + 38.f * Fs, Y + 5.f * Fs, W - PW - 64.f * Fs, EFont::Body, 13.f * Fs, Raid->NestNote.IsEmpty() ? C::Muted : C::Jam, 2, 1.15f);
		// What the mouse is on: a slip of paper by the cursor.
		if (!S.Tip.IsEmpty() && S.Mouse.X >= 0.f)
		{
			const float TW = 430.f * Fs, Pt = 14.f * Fs;
			float X = S.Mouse.X + 22.f * Fs, TY = S.Mouse.Y + 20.f * Fs;
			if (X + TW > W - 10.f) { X = S.Mouse.X - TW - 16.f * Fs; }
			const float TH = AntUiCanvas::Wrap(Cv, S.Tip, -10000.f, -10000.f, TW - 24.f * Fs, EFont::Body, Pt, FLinearColor(0.f, 0.f, 0.f, 0.f), 8) + 18.f * Fs;
			if (TY + TH > H - 8.f) { TY = S.Mouse.Y - TH - 14.f * Fs; }
			AntUiCanvas::Rect(Cv, X + 4.f, TY + 5.f, TW, TH, FLinearColor(0.f, 0.f, 0.f, 0.35f));
			AntUiCanvas::Rect(Cv, X, TY, TW, TH, C::Paper);
			AntUiCanvas::Rect(Cv, X, TY, 4.f * Fs, TH, C::Amber);
			AntUiCanvas::Wrap(Cv, S.Tip, X + 14.f * Fs, TY + 9.f * Fs, TW - 24.f * Fs, EFont::Body, Pt, C::Graphite, 8);
		}
	}
'''
t = t[:a] + NEW + t[b:]
def rep(old, new):
    global t
    if t.count(old) != 1:
        sys.exit("anchor %r found %d" % (old[:60], t.count(old)))
    t = t.replace(old, new)
rep("Mouse.Y > H - 132.f * Fs", "Mouse.Y > H - 56.f * Fs")
rep("PH = H - PY - 140.f * Fs;", "PH = H - PY - 70.f * Fs;")
io.open(P, "w", encoding="utf-8-sig", newline="").write(t.replace("\n", nl))

P2 = ROOT + r"\AntColony.cpp"
t = io.open(P2, encoding="utf-8-sig").read()
nl2 = "\r\n" if "\r\n" in t else "\n"
t = t.replace("\r\n", "\n")
rep("1460.f, 968.f, 320.f, 206.f", "1460.f, 926.f, 312.f, 196.f")
rep("1090.f, 972.f, 300.f, 200.f", "1090.f, 930.f, 292.f, 190.f")
io.open(P2, "w", encoding="utf-8-sig", newline="").write(t.replace("\n", nl2))
print("ok")
