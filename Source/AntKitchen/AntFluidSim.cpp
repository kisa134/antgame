#include "AntFluidSim.h"
#include "Async/ParallelFor.h"
#include "Misc/Compression.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace AntFluid
{
	static const float DryEps = 1.e-3f;

	const TCHAR* LiquidName(ELiquid L)
	{
		switch (L)
		{
		case Water: return TEXT("water");
		case Cola: return TEXT("cola");
		case Oil: return TEXT("oil");
		case Honey: return TEXT("honey");
		default: return TEXT("?");
		}
	}

	bool LiquidFromName(const FString& S, ELiquid& Out)
	{
		for (int32 i = 0; i < LiquidCount; ++i)
		{
			if (S.Equals(LiquidName((ELiquid)i), ESearchCase::IgnoreCase)) { Out = (ELiquid)i; return true; }
		}
		return false;
	}

	static FORCEINLINE float Bil(const float* A, int32 W, int32 Hh, float X, float Y)
	{
		X = FMath::Clamp(X, 0.f, (float)W - 1.001f);
		Y = FMath::Clamp(Y, 0.f, (float)Hh - 1.001f);
		const int32 I = (int32)X, J = (int32)Y;
		const float Fx = X - (float)I, Fy = Y - (float)J;
		const float* P = A + J * W + I;
		return (P[0] * (1.f - Fx) + P[1] * Fx) * (1.f - Fy) + (P[W] * (1.f - Fx) + P[W + 1] * Fx) * Fy;
	}

	void FBasin::Init(const FString& InName, float InOriginX, float InOriginY, int32 InNx, int32 InNy, float InDx, float InZ0)
	{
		Name = InName;
		OriginX = InOriginX; OriginY = InOriginY;
		Nx = FMath::Max(4, InNx); Ny = FMath::Max(4, InNy);
		Dx = InDx; Z0 = InZ0;
		const int32 N = Nx * Ny;
		Bed.Init(0.f, N);
		Kind.Init(Open, N);
		for (int32 L = 0; L < LCount; ++L)
		{
			FLayer& Ly = Layers[L];
			Ly.H.Init(0.f, N);
			Ly.U.Init(0.f, (Nx + 1) * Ny);
			Ly.V.Init(0.f, Nx * (Ny + 1));
			if (L == LWater) { Ly.Dye.Init(0.f, N); Ly.Foam.Init(0.f, N); }
			Ly.X0 = Ly.Y0 = Ly.X1 = Ly.Y1 = 0;
			Ly.Volume = 0.;
		}
		BedL.Init(0.f, N); Tmp.Init(0.f, N); Tmp2.Init(0.f, N); Tmp3.Init(0.f, N); Scale.Init(1.f, N);
		FxS.Init(0.f, (Nx + 1) * Ny); FyS.Init(0.f, Nx * (Ny + 1));
		Un.Init(0.f, (Nx + 1) * Ny); Vn.Init(0.f, Nx * (Ny + 1));

		// Honey: real honey is about 1e-2 m^2/s. Thick enough that the viscous term rules.
		Params[LHoney].Viscosity = 30000.f; Params[LHoney].Contact = 9.f; Params[LHoney].BedDrag = 0.f;
		Params[LHoney].bAdvect = false; Params[LHoney].Film = 0.8f;
		// Water: 1e-6 m^2/s. Contact thickness 2.6 mm: a puddle on tiles.
		Params[LWater].Viscosity = 3.6f; Params[LWater].Contact = 5.f; Params[LWater].BedDrag = 0.012f;
		Params[LWater].bAdvect = true; Params[LWater].Film = 0.6f; Params[LWater].Evaporation = 0.004f;
		// Oil: 8e-5 m^2/s, wets everything: a film a fraction of a millimetre thick.
		Params[LOil].Viscosity = 290.f; Params[LOil].Contact = 0.45f; Params[LOil].BedDrag = 0.f;
		Params[LOil].bAdvect = false; Params[LOil].Film = 0.05f; Params[LOil].FollowBelow = 30.f;
	}

	void FBasin::Finish()
	{
		// A wall never holds liquid; make sure nothing was left in one.
		for (int32 L = 0; L < LCount; ++L)
		{
			for (int32 c = 0; c < Nx * Ny; ++c) { if (Kind[c] == Wall) { Layers[L].H[c] = 0.f; } }
		}
	}

	bool FBasin::CellAt(float X, float Y, int32& I, int32& J) const
	{
		const float Fx = (X - OriginX) / Dx, Fy = (Y - OriginY) / Dx;
		I = (int32)FMath::FloorToFloat(Fx); J = (int32)FMath::FloorToFloat(Fy);
		return I >= 0 && J >= 0 && I < Nx && J < Ny;
	}

	void FBasin::GrowBox(FLayer& Ly, int32 I0, int32 J0, int32 I1, int32 J1)
	{
		I0 = FMath::Clamp(I0, 0, Nx); I1 = FMath::Clamp(I1, 0, Nx); J0 = FMath::Clamp(J0, 0, Ny); J1 = FMath::Clamp(J1, 0, Ny);
		if (I1 <= I0 || J1 <= J0) { return; }
		if (!Ly.Any()) { Ly.X0 = I0; Ly.X1 = I1; Ly.Y0 = J0; Ly.Y1 = J1; return; }
		Ly.X0 = FMath::Min(Ly.X0, I0); Ly.X1 = FMath::Max(Ly.X1, I1); Ly.Y0 = FMath::Min(Ly.Y0, J0); Ly.Y1 = FMath::Max(Ly.Y1, J1);
	}

	void FBasin::Step()
	{
		const int32 N = Nx * Ny;
		for (int32 s = 0; s < SubSteps; ++s)
		{
			FMemory::Memcpy(BedL.GetData(), Bed.GetData(), N * sizeof(float));
			for (int32 L = 0; L < LCount; ++L)
			{
				FLayer& Ly = Layers[L];
				StepLayer(L);
				if (Ly.Any() && L + 1 < LCount)
				{
					for (int32 j = Ly.Y0; j < Ly.Y1; ++j)
					{
						float* B = BedL.GetData() + j * Nx; const float* H = Ly.H.GetData() + j * Nx;
						for (int32 i = Ly.X0; i < Ly.X1; ++i) { B[i] += H[i]; }
					}
				}
			}
		}
		++StepCount;
	}

	void FBasin::StepLayer(int32 L)
	{
		FLayer& Ly = Layers[L];
		if (!Ly.Any()) { Ly.Volume = 0.; return; }
		const FLayerParams P = Params[L];
		const float dt = Dt / (float)SubSteps;
		const float k = dt / Dx;
		const float gk = Gravity * k;
		const float Umax = 0.45f * Dx / dt;
		const int32 CX0 = FMath::Max(Ly.X0 - 1, 0), CY0 = FMath::Max(Ly.Y0 - 1, 0), CX1 = FMath::Min(Ly.X1 + 1, Nx), CY1 = FMath::Min(Ly.Y1 + 1, Ny);
		const int32 UW = Nx + 1;
		float* H = Ly.H.GetData(); float* U = Ly.U.GetData(); float* V = Ly.V.GetData();
		const float* B = BedL.GetData(); const uint8* K = Kind.GetData();
		float* UnP = Un.GetData(); float* VnP = Vn.GetData(); float* Fx = FxS.GetData(); float* Fy = FyS.GetData();
		const FLayer* Below = (P.FollowBelow > 0.f && L > 0 && Layers[L - 1].Any()) ? &Layers[L - 1] : nullptr;
		const float Follow = FMath::Min(1.f, P.FollowBelow * dt);
		const float Visc3 = 3.f * P.Viscosity;
		const float StabK = 4.f * Gravity * dt / (Dx * Dx);
		const EParallelForFlags Flags = bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		const int32 LNx = Nx, LNy = Ny;

		// One face: momentum (advection, slope, friction), then the flux it carries. A = the cell behind the face, Bc = the cell ahead.
		auto Face = [&](float u, int32 A, int32 Bc, float uBelow, bool bWaterBelow) -> FVector2f
		{
			if (K[A] == Wall || K[Bc] == Wall) { return FVector2f(0.f, 0.f); }
			const float ha = H[A], hb = H[Bc];
			const bool aWet = ha > DryEps, bWet = hb > DryEps;
			if (!aWet && !bWet) { return FVector2f(0.f, 0.f); }
			const float ea = B[A] + ha, eb = B[Bc] + hb;
			// A front wets the next dry cell only when the liquid behind it is thicker than the contact thickness and stands above that cell's bed.
			if (!bWet && (ha <= P.Contact || ea <= B[Bc])) { return FVector2f(0.f, 0.f); }
			if (!aWet && (hb <= P.Contact || eb <= B[A])) { return FVector2f(0.f, 0.f); }
			// The head that drives the flow is never more than the depth of the cell it leaves (a weir, not a free fall inside the cell).
			const float d = FMath::Clamp(ea - eb, -hb, ha);
			u += gk * d;
			if (bWaterBelow) { u += (uBelow - u) * Follow; }
			const float hf = FMath::Max(u > 0.f ? ha : hb, 0.05f);
			float Kf = Visc3 / (hf * hf) + P.BedDrag * FMath::Abs(u) / hf;
			if (bWaterBelow) { Kf *= 0.03f; }
			if (Kf * dt > 1.f) { Kf = FMath::Max(Kf, StabK * hf); }   // overdamped: keep the explicit creep stable
			u /= (1.f + dt * Kf);
			u = FMath::Clamp(u, -Umax, Umax);
			return FVector2f(u, u * (u > 0.f ? ha : hb) * k);
		};

		// ---- U faces
		const int32 FI0 = FMath::Max(CX0, 1), FI1 = FMath::Min(CX1, Nx - 1);
		ParallelFor(CY1 - CY0, [&](int32 Row)
		{
			const int32 j = CY0 + Row;
			for (int32 i = FI0; i <= FI1; ++i)
			{
				const int32 A = j * LNx + i - 1, Bc = A + 1;
				float u = U[j * UW + i];
				if (P.bAdvect && (H[A] > DryEps || H[Bc] > DryEps))
				{
					const float v = 0.25f * (V[j * LNx + i - 1] + V[j * LNx + i] + V[(j + 1) * LNx + i - 1] + V[(j + 1) * LNx + i]);
					u = Bil(U, UW, LNy, (float)i - u * k, (float)j - v * k);
				}
				float ub = 0.f; bool bW = false;
				if (Below) { bW = Below->H[A] > 0.3f && Below->H[Bc] > 0.3f; ub = Below->U[j * UW + i]; }
				const FVector2f R = Face(u, A, Bc, ub, bW);
				UnP[j * UW + i] = R.X; Fx[j * UW + i] = R.Y;
			}
		}, Flags);
		// ---- V faces
		const int32 FJ0 = FMath::Max(CY0, 1), FJ1 = FMath::Min(CY1, Ny - 1);
		if (FJ1 >= FJ0)
		{
			ParallelFor(FJ1 - FJ0 + 1, [&](int32 Row)
			{
				const int32 j = FJ0 + Row;
				for (int32 i = CX0; i < CX1; ++i)
				{
					const int32 A = (j - 1) * LNx + i, Bc = j * LNx + i;
					float v = V[j * LNx + i];
					if (P.bAdvect && (H[A] > DryEps || H[Bc] > DryEps))
					{
						const float u = 0.25f * (U[(j - 1) * UW + i] + U[(j - 1) * UW + i + 1] + U[j * UW + i] + U[j * UW + i + 1]);
						v = Bil(V, LNx, LNy + 1, (float)i - u * k, (float)j - v * k);
					}
					float vb = 0.f; bool bW = false;
					if (Below) { bW = Below->H[A] > 0.3f && Below->H[Bc] > 0.3f; vb = Below->V[j * LNx + i]; }
					const FVector2f R = Face(v, A, Bc, vb, bW);
					VnP[j * LNx + i] = R.X; Fy[j * LNx + i] = R.Y;
				}
			}, Flags);
		}
		for (int32 j = CY0; j < CY1; ++j) { FMemory::Memcpy(U + j * UW + FI0, UnP + j * UW + FI0, (FI1 - FI0 + 1) * sizeof(float)); }
		for (int32 j = FJ0; j <= FJ1; ++j) { FMemory::Memcpy(V + j * LNx + CX0, VnP + j * LNx + CX0, (CX1 - CX0) * sizeof(float)); }

		// ---- a cell never gives more than it has
		float* Sc = Scale.GetData();
		ParallelFor(CY1 - CY0, [&](int32 Row)
		{
			const int32 j = CY0 + Row;
			for (int32 i = CX0; i < CX1; ++i)
			{
				const int32 c = j * LNx + i;
				const float Out = FMath::Max(0.f, Fx[j * UW + i + 1]) + FMath::Max(0.f, -Fx[j * UW + i]) + FMath::Max(0.f, Fy[(j + 1) * LNx + i]) + FMath::Max(0.f, -Fy[j * LNx + i]);
				Sc[c] = Out > H[c] ? H[c] / Out : 1.f;
			}
		}, Flags);

		// ---- mass (and the dye and the foam it carries)
		float* Hn = Tmp.GetData(); float* Dn = Tmp2.GetData(); float* Fn = Tmp3.GetData();
		float* Dye = Ly.Dye.Num() ? Ly.Dye.GetData() : nullptr;
		float* Foam = Ly.Foam.Num() ? Ly.Foam.GetData() : nullptr;
		const float FoamKeep = 1.f - dt * 1.1f;
		ParallelFor(CY1 - CY0, [&](int32 Row)
		{
			const int32 j = CY0 + Row;
			for (int32 i = CX0; i < CX1; ++i)
			{
				const int32 c = j * LNx + i;
				const float fl = Fx[j * UW + i], fr = Fx[j * UW + i + 1], fd = Fy[j * LNx + i], fu = Fy[(j + 1) * LNx + i];
				// flux through each face, scaled by the cell it leaves
				const float Fl = fl * (fl > 0.f ? (i > 0 ? Sc[c - 1] : 1.f) : Sc[c]);
				const float Fr = fr * (fr > 0.f ? Sc[c] : (i + 1 < LNx ? Sc[c + 1] : 1.f));
				const float Fd = fd * (fd > 0.f ? (j > 0 ? Sc[c - LNx] : 1.f) : Sc[c]);
				const float Fu = fu * (fu > 0.f ? Sc[c] : (j + 1 < LNy ? Sc[c + LNx] : 1.f));
				float h = H[c] + Fl - Fr + Fd - Fu;
				if (h < 0.f) { h = 0.f; }
				if (Dye)
				{
					float m = H[c] * Dye[c];
					m += Fl * ((Fl > 0.f && i > 0) ? Dye[c - 1] : Dye[c]);
					m -= Fr * ((Fr > 0.f || i + 1 >= LNx) ? Dye[c] : Dye[c + 1]);
					m += Fd * ((Fd > 0.f && j > 0) ? Dye[c - LNx] : Dye[c]);
					m -= Fu * ((Fu > 0.f || j + 1 >= LNy) ? Dye[c] : Dye[c + LNx]);
					Dn[c] = h > DryEps ? FMath::Clamp(m / h, 0.f, 1.f) : 0.f;
				}
				if (Foam)
				{
					const float uc = 0.5f * (U[j * UW + i] + U[j * UW + i + 1]), vc = 0.5f * (V[j * LNx + i] + V[(j + 1) * LNx + i]);
					float f = Bil(Foam, LNx, LNy, (float)i - uc * k, (float)j - vc * k) * FoamKeep;
					const float Sp2 = uc * uc + vc * vc;
					// white water where it runs fast and shallow (supercritical) and where the flow piles up
					const float Fr2 = Sp2 / (Gravity * FMath::Max(h, 0.5f));
					if (Fr2 > 3.f && h > 2.f) { f += dt * 0.5f * FMath::Min(Fr2 - 3.f, 2.f); }
					Fn[c] = h > DryEps ? FMath::Min(f, 1.f) : 0.f;
				}
				if (h < P.Film && P.Evaporation > 0.f) { h = FMath::Max(0.f, h - P.Evaporation * dt); }
				Hn[c] = h;
			}
		}, Flags);

		// ---- commit, collect what fell over an edge, find the new box
		int32 NX0 = Nx, NY0 = Ny, NX1 = 0, NY1 = 0;
		double Vol = 0.;
		const float Cell2 = Dx * Dx;
		for (int32 j = CY0; j < CY1; ++j)
		{
			for (int32 i = CX0; i < CX1; ++i)
			{
				const int32 c = j * Nx + i;
				float h = Hn[c];
				if (h > 0.f && K[c] == Void)
				{
					FSpill S; S.Cell = c; S.Layer = (uint8)L; S.Volume = h * Cell2; S.Dye = Dye ? Dn[c] : 0.f;
					S.Vel = FVector2f(0.5f * (U[j * UW + i] + U[j * UW + i + 1]), 0.5f * (V[j * Nx + i] + V[(j + 1) * Nx + i]));
					Spills.Add(S);
					h = 0.f;
				}
				if (h < 1.e-5f) { h = 0.f; }
				H[c] = h;
				if (Dye) { Dye[c] = Dn[c]; }
				if (Foam) { Foam[c] = Fn[c]; }
				if (h > 0.f)
				{
					Vol += h;
					NX0 = FMath::Min(NX0, i); NX1 = FMath::Max(NX1, i + 1); NY0 = FMath::Min(NY0, j); NY1 = FMath::Max(NY1, j + 1);
				}
			}
		}
		Ly.Volume = Vol * Cell2;
		if (NX1 > NX0) { Ly.X0 = NX0; Ly.X1 = NX1; Ly.Y0 = NY0; Ly.Y1 = NY1; }
		else { Ly.X0 = Ly.X1 = Ly.Y0 = Ly.Y1 = 0; }
	}

	// ---------------------------------------------------------------------------------------------
	// Sources, sinks, pushes
	// ---------------------------------------------------------------------------------------------

	template <typename F>
	static void ForDisc(const FBasin& Bn, float X, float Y, float Radius, F&& Fn)
	{
		int32 CI, CJ;
		Bn.CellAt(X, Y, CI, CJ);
		const int32 R = FMath::Max(0, FMath::CeilToInt(Radius / Bn.Dx));
		const float R2 = FMath::Square(FMath::Max(Radius, Bn.Dx * 0.5f));
		for (int32 j = FMath::Max(CJ - R, 0); j <= FMath::Min(CJ + R, Bn.Ny - 1); ++j)
		{
			for (int32 i = FMath::Max(CI - R, 0); i <= FMath::Min(CI + R, Bn.Nx - 1); ++i)
			{
				const FVector2f C = Bn.CellCentre(i, j);
				const float D2 = FMath::Square(C.X - X) + FMath::Square(C.Y - Y);
				if (D2 <= R2 || (i == CI && j == CJ)) { Fn(i, j, D2); }
			}
		}
	}

	float FBasin::AddVolume(ELiquid Liquid, float X, float Y, float Radius, float Volume, const FVector2f& Vel)
	{
		if (Volume <= 0.f || !Contains(X, Y)) { return Volume; }
		const ELayer L = LayerOf(Liquid);
		FLayer& Ly = Layers[L];
		int32 N = 0;
		ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float) { if (Kind[Idx(i, j)] != Wall) { ++N; } });
		if (N == 0) { return Volume; }
		const float Add = Volume / ((float)N * Dx * Dx);
		const float DyeIn = Liquid == Cola ? 1.f : 0.f;
		const int32 UW = Nx + 1;
		int32 I0 = Nx, J0 = Ny, I1 = 0, J1 = 0;
		ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float)
		{
			const int32 c = Idx(i, j);
			if (Kind[c] == Wall) { return; }
			const float H0 = Ly.H[c];
			const float W = 0.5f * Add / (H0 + Add);
			if (Ly.Dye.Num()) { Ly.Dye[c] = (H0 * Ly.Dye[c] + Add * DyeIn) / (H0 + Add); }
			Ly.H[c] = H0 + Add;
			Ly.U[j * UW + i] += (Vel.X - Ly.U[j * UW + i]) * W; Ly.U[j * UW + i + 1] += (Vel.X - Ly.U[j * UW + i + 1]) * W;
			Ly.V[j * Nx + i] += (Vel.Y - Ly.V[j * Nx + i]) * W; Ly.V[(j + 1) * Nx + i] += (Vel.Y - Ly.V[(j + 1) * Nx + i]) * W;
			I0 = FMath::Min(I0, i); I1 = FMath::Max(I1, i + 1); J0 = FMath::Min(J0, j); J1 = FMath::Max(J1, j + 1);
		});
		GrowBox(Ly, I0, J0, I1, J1);
		Ly.Volume += Volume;
		return 0.f;
	}

	float FBasin::Drain(ELayer Layer, float X, float Y, float Radius, float MaxDepth)
	{
		if (!Contains(X, Y)) { return 0.f; }
		FLayer& Ly = Layers[Layer];
		float Taken = 0.f;
		ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float)
		{
			const int32 c = Idx(i, j);
			const float T = FMath::Min(Ly.H[c], MaxDepth);
			Ly.H[c] -= T; Taken += T;
		});
		Taken *= Dx * Dx;
		Ly.Volume = FMath::Max(0., Ly.Volume - Taken);
		return Taken;
	}

	void FBasin::Push(float X, float Y, float Radius, const FVector2f& Vel, float Blend)
	{
		if (!Contains(X, Y)) { return; }
		const int32 UW = Nx + 1;
		for (int32 L = LWater; L <= LOil; ++L)
		{
			FLayer& Ly = Layers[L];
			if (!Ly.Any()) { continue; }
			ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float)
			{
				if (Ly.H[Idx(i, j)] <= DryEps) { return; }
				Ly.U[j * UW + i] += (Vel.X - Ly.U[j * UW + i]) * Blend; Ly.U[j * UW + i + 1] += (Vel.X - Ly.U[j * UW + i + 1]) * Blend;
				Ly.V[j * Nx + i] += (Vel.Y - Ly.V[j * Nx + i]) * Blend; Ly.V[(j + 1) * Nx + i] += (Vel.Y - Ly.V[(j + 1) * Nx + i]) * Blend;
			});
		}
	}

	void FBasin::Kick(float X, float Y, float Radius, float Speed, float Gain)
	{
		Speed *= Gain;
		if (!Contains(X, Y)) { return; }
		const int32 UW = Nx + 1;
		FLayer& Ly = Layers[LWater];
		if (!Ly.Any()) { return; }
		const float Inv = 1.f / FMath::Max(Radius, 1.f);
		ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float)
		{
			if (Ly.H[Idx(i, j)] <= DryEps) { return; }
			// each face gets the radial speed at its own position
			const float Xl = OriginX + i * Dx - X, Xr = Xl + Dx, Yc = OriginY + (j + 0.5f) * Dx - Y;
			const float Yd = OriginY + j * Dx - Y, Yu = Yd + Dx, Xc = OriginX + (i + 0.5f) * Dx - X;
			Ly.U[j * UW + i] += 0.5f * Speed * Xl * Inv; Ly.U[j * UW + i + 1] += 0.5f * Speed * Xr * Inv;
			Ly.V[j * Nx + i] += 0.5f * Speed * Yd * Inv; Ly.V[(j + 1) * Nx + i] += 0.5f * Speed * Yu * Inv;
			(void)Yc; (void)Xc;
		});
	}

	void FBasin::AddFoam(float X, float Y, float Radius, float Amount)
	{
		if (!Contains(X, Y)) { return; }
		FLayer& Ly = Layers[LWater];
		ForDisc(*this, X, Y, Radius, [&](int32 i, int32 j, float) { const int32 c = Idx(i, j); if (Ly.H[c] > DryEps) { Ly.Foam[c] = FMath::Min(1.f, Ly.Foam[c] + Amount); } });
	}

	void FBasin::Sample(float X, float Y, FSample& Out) const
	{
		Out = FSample();
		int32 I, J;
		if (!CellAt(X, Y, I, J)) { return; }
		const int32 c = Idx(I, J);
		Out.bInside = true;
		Out.Cell = Kind[c];
		Out.Bed = Z0 + Bed[c];
		float T = 0.f;
		for (int32 L = 0; L < LCount; ++L) { Out.Depth[L] = Layers[L].H[c]; T += Layers[L].H[c]; }
		Out.Total = T;
		Out.Surface = Out.Bed + T;
		const int32 UW = Nx + 1;
		const FLayer& W = Layers[Out.Depth[LWater] > DryEps ? LWater : (Out.Depth[LHoney] > DryEps ? LHoney : LOil)];
		Out.Vel = FVector2f(0.5f * (W.U[J * UW + I] + W.U[J * UW + I + 1]), 0.5f * (W.V[J * Nx + I] + W.V[(J + 1) * Nx + I]));
		Out.Dye = Layers[LWater].Dye[c];
		Out.Foam = Layers[LWater].Foam[c];
	}

	uint32 FBasin::Hash() const
	{
		uint32 Crc = 0;
		for (int32 L = 0; L < LCount; ++L)
		{
			Crc = FCrc::MemCrc32(Layers[L].H.GetData(), Layers[L].H.Num() * sizeof(float), Crc);
			Crc = FCrc::MemCrc32(Layers[L].U.GetData(), Layers[L].U.Num() * sizeof(float), Crc);
			Crc = FCrc::MemCrc32(Layers[L].V.GetData(), Layers[L].V.Num() * sizeof(float), Crc);
		}
		return Crc;
	}

	int32 FBasin::WetCells(ELayer L) const
	{
		const FLayer& Ly = Layers[L];
		int32 N = 0;
		for (int32 j = Ly.Y0; j < Ly.Y1; ++j) { for (int32 i = Ly.X0; i < Ly.X1; ++i) { if (Ly.H[Idx(i, j)] > Params[L].Film) { ++N; } } }
		return N;
	}

	// ---------------------------------------------------------------------------------------------
	// Snapshots: block averages (SnapCoarse x SnapCoarse cells), 16-bit depth, 8-bit velocity and dye, zlib over the lot.
	// ---------------------------------------------------------------------------------------------

	void FBasin::WriteSnapshot(TArray<uint8>& Out) const
	{
		const int32 C = FMath::Max(1, SnapCoarse);
		TArray<uint8> Raw;
		FMemoryWriter Ar(Raw);
		int32 Magic = 0x414E5446, VNx = Nx, VNy = Ny, VC = C; uint32 VStep = StepCount;
		Ar << Magic << VNx << VNy << VC << VStep;
		const int32 UW = Nx + 1;
		const float Umax = 0.45f * Dx / (Dt / (float)SubSteps);
		for (int32 L = 0; L < LCount; ++L)
		{
			const FLayer& Ly = Layers[L];
			int16 B0 = (int16)(Ly.X0 / C), B1 = (int16)((Ly.X1 + C - 1) / C), B2 = (int16)(Ly.Y0 / C), B3 = (int16)((Ly.Y1 + C - 1) / C);
			if (!Ly.Any()) { B0 = B1 = B2 = B3 = 0; }
			Ar << B0 << B1 << B2 << B3;
			// planes one after another: equal bytes sit together and pack well
			TArray<uint16> Dp; TArray<int8> Us, Vs; TArray<uint8> Dy;
			for (int32 bj = B2; bj < B3; ++bj)
			{
				for (int32 bi = B0; bi < B1; ++bi)
				{
					float SumH = 0.f, SumU = 0.f, SumV = 0.f, SumD = 0.f; int32 N = 0;
					for (int32 j = bj * C; j < FMath::Min((bj + 1) * C, Ny); ++j)
					{
						for (int32 i = bi * C; i < FMath::Min((bi + 1) * C, Nx); ++i)
						{
							const int32 c = Idx(i, j);
							if (Kind[c] != Open) { continue; }
							++N; SumH += Ly.H[c];
							SumU += 0.5f * (Ly.U[j * UW + i] + Ly.U[j * UW + i + 1]); SumV += 0.5f * (Ly.V[j * Nx + i] + Ly.V[(j + 1) * Nx + i]);
							if (L == LWater) { SumD += Ly.H[c] * Ly.Dye[c]; }
						}
					}
					const float Inv = N > 0 ? 1.f / (float)N : 0.f;
					Dp.Add((uint16)FMath::Clamp(FMath::RoundToInt(SumH * Inv * 32.f), 0, 65535));
					if (L != LHoney) { Us.Add((int8)FMath::Clamp(FMath::RoundToInt(SumU * Inv / Umax * 127.f), -127, 127)); Vs.Add((int8)FMath::Clamp(FMath::RoundToInt(SumV * Inv / Umax * 127.f), -127, 127)); }
					if (L == LWater) { Dy.Add((uint8)FMath::Clamp(FMath::RoundToInt(SumH > 0.f ? SumD / SumH * 255.f : 0.f), 0, 255)); }
				}
			}
			Ar.Serialize(Dp.GetData(), Dp.Num() * sizeof(uint16));
			Ar.Serialize(Us.GetData(), Us.Num()); Ar.Serialize(Vs.GetData(), Vs.Num()); Ar.Serialize(Dy.GetData(), Dy.Num());
		}
		int32 Bound = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
		Out.SetNumUninitialized(Bound + 4);
		int32 RawNum = Raw.Num();
		FMemory::Memcpy(Out.GetData(), &RawNum, 4);
		if (!FCompression::CompressMemory(NAME_Zlib, Out.GetData() + 4, Bound, Raw.GetData(), Raw.Num())) { Out.Reset(); return; }
		Out.SetNum(Bound + 4, EAllowShrinking::No);
	}

	bool FBasin::ReadSnapshot(const TArray<uint8>& In, float Blend)
	{
		if (In.Num() < 8) { return false; }
		int32 RawNum = 0;
		FMemory::Memcpy(&RawNum, In.GetData(), 4);
		if (RawNum < 20 || RawNum > 64 * 1024 * 1024) { return false; }
		TArray<uint8> Raw; Raw.SetNumUninitialized(RawNum);
		if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawNum, In.GetData() + 4, In.Num() - 4)) { return false; }
		FMemoryReader Ar(Raw);
		int32 Magic = 0, VNx = 0, VNy = 0, C = 0; uint32 VStep = 0;
		Ar << Magic << VNx << VNy << C << VStep;
		if (Magic != 0x414E5446 || VNx != Nx || VNy != Ny || C < 1) { return false; }
		const int32 UW = Nx + 1;
		const float Umax = 0.45f * Dx / (Dt / (float)SubSteps);
		for (int32 L = 0; L < LCount; ++L)
		{
			FLayer& Ly = Layers[L];
			int16 B0 = 0, B1 = 0, B2 = 0, B3 = 0;
			Ar << B0 << B1 << B2 << B3;
			const int32 BW = B1 - B0, BH = B3 - B2, BN = FMath::Max(0, BW * BH);
			TArray<uint16> Dp; Dp.SetNumUninitialized(BN); Ar.Serialize(Dp.GetData(), BN * sizeof(uint16));
			TArray<int8> Us, Vs; TArray<uint8> Dy;
			if (L != LHoney) { Us.SetNumUninitialized(BN); Vs.SetNumUninitialized(BN); Ar.Serialize(Us.GetData(), BN); Ar.Serialize(Vs.GetData(), BN); }
			if (L == LWater) { Dy.SetNumUninitialized(BN); Ar.Serialize(Dy.GetData(), BN); }
			if (Ar.IsError()) { return false; }
			// every block the server or this machine has liquid in
			int32 U0 = B0, U1 = B1, V0 = B2, V1 = B3;
			if (Ly.Any())
			{
				const int32 M0 = Ly.X0 / C, M1 = (Ly.X1 + C - 1) / C, M2 = Ly.Y0 / C, M3 = (Ly.Y1 + C - 1) / C;
				if (BN == 0) { U0 = M0; U1 = M1; V0 = M2; V1 = M3; }
				else { U0 = FMath::Min(U0, M0); U1 = FMath::Max(U1, M1); V0 = FMath::Min(V0, M2); V1 = FMath::Max(V1, M3); }
			}
			int32 NX0 = Nx, NY0 = Ny, NX1 = 0, NY1 = 0; double Vol = 0.;
			for (int32 bj = V0; bj < V1; ++bj)
			{
				for (int32 bi = U0; bi < U1; ++bi)
				{
					const bool bIn = bi >= B0 && bi < B1 && bj >= B2 && bj < B3;
					const int32 b = bIn ? (bj - B2) * BW + (bi - B0) : 0;
					const float SH = bIn ? (float)Dp[b] / 32.f : 0.f;
					float Sum = 0.f; int32 N = 0;
					for (int32 j = bj * C; j < FMath::Min((bj + 1) * C, Ny); ++j) { for (int32 i = bi * C; i < FMath::Min((bi + 1) * C, Nx); ++i) { if (Kind[Idx(i, j)] == Open) { ++N; Sum += Ly.H[Idx(i, j)]; } } }
					if (N == 0) { continue; }
					const float Diff = (SH - Sum / (float)N) * Blend;
					const float SU = (bIn && Us.Num()) ? (float)Us[b] / 127.f * Umax : 0.f, SV = (bIn && Vs.Num()) ? (float)Vs[b] / 127.f * Umax : 0.f;
					for (int32 j = bj * C; j < FMath::Min((bj + 1) * C, Ny); ++j)
					{
						for (int32 i = bi * C; i < FMath::Min((bi + 1) * C, Nx); ++i)
						{
							const int32 c = Idx(i, j);
							if (Kind[c] != Open) { continue; }
							// the block's mass is corrected, the fine shape this machine computed inside the block stays
							float h = FMath::Max(0.f, Ly.H[c] + Diff);
							if (h < 1.e-4f) { h = 0.f; }
							Ly.H[c] = h;
							if (L != LHoney && h > 0.f)
							{
								const float B5 = 0.25f * Blend;
								Ly.U[j * UW + i] += (SU - Ly.U[j * UW + i]) * B5; Ly.U[j * UW + i + 1] += (SU - Ly.U[j * UW + i + 1]) * B5;
								Ly.V[j * Nx + i] += (SV - Ly.V[j * Nx + i]) * B5; Ly.V[(j + 1) * Nx + i] += (SV - Ly.V[(j + 1) * Nx + i]) * B5;
							}
							if (L == LWater && bIn) { Ly.Dye[c] += ((float)Dy[b] / 255.f - Ly.Dye[c]) * Blend; }
							if (h > 0.f) { Vol += h; NX0 = FMath::Min(NX0, i); NX1 = FMath::Max(NX1, i + 1); NY0 = FMath::Min(NY0, j); NY1 = FMath::Max(NY1, j + 1); }
						}
					}
				}
			}
			Ly.Volume = Vol * Dx * Dx;
			if (NX1 > NX0) { Ly.X0 = NX0; Ly.X1 = NX1; Ly.Y0 = NY0; Ly.Y1 = NY1; } else { Ly.X0 = Ly.X1 = Ly.Y0 = Ly.Y1 = 0; }
		}
		return true;
	}
}
