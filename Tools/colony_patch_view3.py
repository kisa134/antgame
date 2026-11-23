# One-off: ants in vertical shafts walk behind the chamber shells (never over the Queen), small and dark, no load drawn; dig tags stay on screen.
import io, sys
P = r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_colony\Source\AntKitchen\AntColonyView.cpp"
t = io.open(P, encoding="utf-8-sig").read()
nl = "\r\n" if "\r\n" in t else "\n"
t = t.replace("\r\n", "\n")
def rep(old, new):
    global t
    if t.count(old) != 1:
        sys.exit("anchor %r found %d" % (old[:70], t.count(old)))
    t = t.replace(old, new)

rep("""	void DrawAnts(UCanvas* Cv, const FView& V)
	{
		for (const FWalker& A : St().Ants)
		{
			if (A.Delay > 0.f) { continue; }""", """	/** A tunnel that climbs (more up than across): its ants walk behind the chamber pictures, so they never cross the Queen or the stores. */
	bool IsShaft(int32 E)
	{
		const FVector2D A = NodePos(Edges[E].A), B = NodePos(Edges[E].B);
		return FMath::Abs(B.Y - A.Y) > FMath::Abs(B.X - A.X);
	}

	/** bShafts: the ants of the climbing tunnels (drawn before the chambers, small, dark, no load); otherwise the rest. */
	void DrawAnts(UCanvas* Cv, const FView& V, bool bShafts)
	{
		for (const FWalker& A : St().Ants)
		{
			if (A.Delay > 0.f || IsShaft(A.Edge) != bShafts) { continue; }
			if (bShafts)
			{
				FVector2D P[4];
				EdgePath(A.Edge, P);
				FVector2D Dir;
				const FVector2D At = PathAt(P, A.bBack ? 1.f - A.T : A.T, &Dir);
				DrawAnt(Cv, V, At, A.bBack ? -Dir : Dir, A.Size * 0.78f, A.Phase, -1, FLinearColor(0.03f, 0.014f, 0.008f, 1.f));
				continue;
			}""")
rep("""	DrawTunnels(Cv, V, N, -1);
	for (uint8 c = 0; c < AntColony::Count; ++c)""", """	DrawTunnels(Cv, V, N, -1);
	DrawAnts(Cv, V, true);   // the shafts: behind the chambers
	for (uint8 c = 0; c < AntColony::Count; ++c)""")
rep("	DrawAnts(Cv, V);\n", "	DrawAnts(Cv, V, false);\n")
# the shine of an ant is drawn only when it is big enough to read as one
rep("		Dot(Cv, Abd - Side * 0.08f * L - Dir * 0.04f * L, 0.09f * L, FLinearColor(0.55f, 0.32f, 0.16f, 0.55f));   // the shine",
    "		if (L > 14.f) { Dot(Cv, Abd - Side * 0.08f * L - Dir * 0.04f * L, 0.09f * L, FLinearColor(0.4f, 0.22f, 0.1f, 0.45f)); }   // the shine")
# dig tags: kept inside the screen
rep("""			const float TW = M.X + 20.f, TH = M.Y * 2.3f + 10.f;
""", """			const float TW = M.X + 20.f, TH = M.Y * 2.3f + 10.f;
			Mid.X = FMath::Clamp(Mid.X, TW * 0.5f + 8.f, V.W - TW * 0.5f - 8.f);
""")
rep("			const FVector2D Mid = V.S((P[1] + P[2]) * 0.5f);\n", "			FVector2D Mid = V.S((P[1] + P[2]) * 0.5f);\n")
io.open(P, "w", encoding="utf-8-sig", newline="").write(t.replace("\n", nl))
print("ok")
