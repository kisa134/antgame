#pragma once

#include "CoreMinimal.h"

/**
 * Layer A of the liquid: the shared, authoritative flow. A stack of shallow layers over a bed (a height field) per basin:
 * honey at the bottom, water (with a dye: cola) over it, oil on top. Each layer is the same solver (staggered shallow-water
 * equations: mass by upwind fluxes, momentum by the surface slope, semi-Lagrangian advection, implicit friction) with its
 * own viscosity, bed friction and contact thickness:
 *   - water: inertial, makes waves and wakes;
 *   - honey: its viscous term 3*nu/h^2 swamps inertia, so the same code solves the lubrication equation: a slow creeping blob;
 *   - oil: a very small contact thickness, so it spreads into a wide thin film, and it is dragged along by the water under it.
 * Surface tension at the scale of an 8 mm ant enters as the contact thickness: a front only advances onto a dry cell when the
 * liquid behind it is thicker than that (a puddle is a few millimetres thick and has a rounded rim, it never thins to nothing).
 *
 * Pure C++, no engine objects, fixed step, only + - * / and sqrt: the same inputs give the same state on the same build.
 * Units: Unreal units (1 m = 1900 uu in this game), seconds.
 */
namespace AntFluid
{
	enum ELayer : uint8 { LHoney = 0, LWater = 1, LOil = 2, LCount = 3 };
	enum ELiquid : uint8 { Water = 0, Cola = 1, Oil = 2, Honey = 3, LiquidCount = 4 };
	inline ELayer LayerOf(ELiquid L) { return L == Oil ? LOil : (L == Honey ? LHoney : LWater); }
	ANTKITCHEN_API const TCHAR* LiquidName(ELiquid L);
	ANTKITCHEN_API bool LiquidFromName(const FString& S, ELiquid& Out);

	/** Cell kinds of a basin. */
	enum ECell : uint8 { Open = 0, Wall = 1, Void = 2 };   // Void: nothing under it (past the edge of a worktop): what flows in falls out of the basin

	struct FLayerParams
	{
		float Viscosity = 3.6f;      // kinematic, uu^2/s (water 1e-6 m^2/s = 3.6)
		float Contact = 5.f;         // uu: the thickness a front needs before it wets the next dry cell (puddle thickness)
		float BedDrag = 0.02f;       // quadratic bed friction
		float Evaporation = 0.f;     // uu/s taken from films thinner than Film
		float Film = 0.6f;           // uu: thinner than this it is a wet film, not a body of liquid
		bool bAdvect = true;         // carry momentum with the flow (off for the overdamped layers)
		float FollowBelow = 0.f;     // 1/s: velocity pulled towards the layer below (oil riding on water)
	};

	struct FLayer
	{
		TArray<float> H;             // depth at cell centres
		TArray<float> U, V;          // face velocities: U at (i-1/2, j) size (Nx+1)*Ny, V at (i, j-1/2) size Nx*(Ny+1)
		TArray<float> Dye;           // water layer only: 0 water .. 1 cola (volume fraction)
		TArray<float> Foam;          // water layer only: 0..1
		int32 X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;   // box of cells that may hold liquid: [X0, X1) x [Y0, Y1). Empty when X1 <= X0
		double Volume = 0.;          // uu^3 (kept up to date by Step)
		bool Any() const { return X1 > X0 && Y1 > Y0; }
	};

	/** What Sample() tells gameplay about one point. */
	struct FSample
	{
		bool bInside = false;        // the point is over this basin
		uint8 Cell = Open;
		float Bed = 0.f;             // world Z of the bed
		float Surface = 0.f;         // world Z of the top of all liquid
		float Depth[LCount] = { 0.f, 0.f, 0.f };
		float Total = 0.f;           // all three
		FVector2f Vel = FVector2f::ZeroVector;   // of the water layer (of the honey where there is no water)
		float Dye = 0.f;
		float Foam = 0.f;
	};

	/** Liquid that left a basin over an edge during one step (a Void cell): the world hands it to the basin below. */
	struct FSpill
	{
		int32 Cell = 0;
		uint8 Layer = LWater;
		float Volume = 0.f;          // uu^3
		float Dye = 0.f;
		FVector2f Vel = FVector2f::ZeroVector;
	};

	class ANTKITCHEN_API FBasin
	{
	public:
		FString Name;
		int32 Nx = 0, Ny = 0;
		float Dx = 20.f;
		float OriginX = 0.f, OriginY = 0.f;    // world XY of the corner of cell (0, 0)
		float Z0 = 0.f;                         // nominal level of the bed (world Z); the bed is stored relative to it
		float Gravity = 980.f;
		float Dt = 1.f / 60.f;
		int32 SubSteps = 1;
		TArray<float> Bed;                      // relative to Z0
		TArray<uint8> Kind;
		FLayerParams Params[LCount];
		FLayer Layers[LCount];
		TArray<FSpill> Spills;                  // filled by Step, emptied by the caller
		uint32 StepCount = 0;
		bool bParallel = true;
		int32 SnapCoarse = 2;                   // a snapshot stores averages over SnapCoarse x SnapCoarse cells

		void Init(const FString& InName, float InOriginX, float InOriginY, int32 InNx, int32 InNy, float InDx, float InZ0);
		/** After Bed and Kind are filled. */
		void Finish();
		void Step();

		int32 Idx(int32 I, int32 J) const { return J * Nx + I; }
		bool CellAt(float X, float Y, int32& I, int32& J) const;
		FVector2f CellCentre(int32 I, int32 J) const { return FVector2f(OriginX + (I + 0.5f) * Dx, OriginY + (J + 0.5f) * Dx); }
		bool Contains(float X, float Y) const { return X >= OriginX && Y >= OriginY && X < OriginX + Nx * Dx && Y < OriginY + Ny * Dx; }
		void Sample(float X, float Y, FSample& Out) const;

		/** Adds Volume (uu^3) of a liquid over a disc; the liquid arrives with velocity Vel. Returns what did not fit (walls). */
		float AddVolume(ELiquid Liquid, float X, float Y, float Radius, float Volume, const FVector2f& Vel = FVector2f::ZeroVector);
		/** Takes liquid away over a disc, at most Rate uu of depth per call. Returns the volume removed. */
		float Drain(ELayer Layer, float X, float Y, float Radius, float MaxDepth);
		/** Pulls the water (and oil) in a disc towards a velocity: a foot, a floating thing, a falling jar. Blend 0..1. */
		void Push(float X, float Y, float Radius, const FVector2f& Vel, float Blend);
		/** A radial kick: something dropped in. Speed is at the rim of the disc. */
		void Kick(float X, float Y, float Radius, float Speed, float Gain = 1.f);
		void AddFoam(float X, float Y, float Radius, float Amount);

		double TotalVolume() const { return Layers[0].Volume + Layers[1].Volume + Layers[2].Volume; }
		uint32 Hash() const;
		int32 WetCells(ELayer L) const;

		// ---- replication: a coarse compressed picture of the state. A client that runs the same sources drifts little; this pulls it back.
		void WriteSnapshot(TArray<uint8>& Out) const;
		/** Blend 1 = take the server's state, less = ease towards it. False if the data does not fit this basin. */
		bool ReadSnapshot(const TArray<uint8>& In, float Blend);

	private:
		void StepLayer(int32 L);
		void GrowBox(FLayer& Ly, int32 I0, int32 J0, int32 I1, int32 J1);
		TArray<float> BedL;       // bed under the layer being solved (bed + the layers below)
		TArray<float> Tmp, Tmp2, Tmp3, FxS, FyS, Scale;
		TArray<float> Un, Vn;
	};
}
