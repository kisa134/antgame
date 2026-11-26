#include "AntLab.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntRun.h"
#include "AntRaid.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "HumanCharacter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

#include "AntLabData.inl"

namespace
{
	TCHAR GWanted = 0;
	bool GWantedInit = false;
	TUniquePtr<FAntLabVariant> GLab;
	TSet<FString> GOpenGates;

	FVector Vec(const TArray<TSharedPtr<FJsonValue>>* A)
	{
		if (!A || A->Num() < 2) { return FVector::ZeroVector; }
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0);
	}
	FVector VecField(const TSharedPtr<FJsonObject>& O, const TCHAR* Name)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O->TryGetArrayField(Name, A) ? Vec(A) : FVector::ZeroVector;
	}
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Name)
	{
		FString S;
		O->TryGetStringField(Name, S);
		return S;
	}
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Name, double Def = 0.0)
	{
		double V = Def;
		O->TryGetNumberField(Name, V);
		return V;
	}

	uint8 RouteKindOf(const FString& K)
	{
		// AntRoutes::EKind: Cover, Open, Outside, InWall
		if (K == TEXT("cover")) { return 0; }
		if (K == TEXT("loggia")) { return 2; }
		if (K == TEXT("wall")) { return 3; }
		if (K == TEXT("climb") || K == TEXT("high")) { return 0; }
		return 1;
	}

	TUniquePtr<FAntLabVariant> Parse(const FString& Json, FString& Err)
	{
		TSharedPtr<FJsonObject> R;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, R) || !R.IsValid())
		{
			Err = TEXT("not JSON");
			return nullptr;
		}
		TUniquePtr<FAntLabVariant> V = MakeUnique<FAntLabVariant>();
		V->Id = Str(R, TEXT("id"));
		V->Name = Str(R, TEXT("name"));
		V->Note = Str(R, TEXT("note"));
		V->M = (float)Num(R, TEXT("uu_per_m"), 1900.0);
		V->Origin = VecField(R, TEXT("origin_uu"));
		V->SprintMps = (float)Num(R, TEXT("sprint_mps"), 0.24);
		V->Ceiling = (float)Num(R, TEXT("ceiling"), 2.6) * V->M;
		const float M = V->M;
		auto P = [&V](const FVector& Metres) { return V->W(Metres); };
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		{
			const TArray<TSharedPtr<FJsonValue>>* BA = nullptr;
			if (R->TryGetArrayField(TEXT("bounds"), BA) && BA->Num() == 4)
			{
				const FVector A0 = P(FVector((*BA)[0]->AsNumber(), (*BA)[1]->AsNumber(), 0.0));
				const FVector A1 = P(FVector((*BA)[2]->AsNumber(), (*BA)[3]->AsNumber(), 0.0));
				V->Bounds = FBox2D(FVector2D(A0), FVector2D(A1));
			}
		}
		if (R->TryGetArrayField(TEXT("rooms"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabRoom Rm;
				Rm.Id = Str(O, TEXT("id")); Rm.Name = Str(O, TEXT("name")); Rm.Hero = Str(O, TEXT("hero")); Rm.Physics = Str(O, TEXT("physics"));
				Rm.Area = (float)Num(O, TEXT("area"));
				const TArray<TSharedPtr<FJsonValue>>* RA = nullptr;
				if (O->TryGetArrayField(TEXT("rect"), RA) && RA->Num() == 4)
				{
					Rm.Rect = FBox2D(FVector2D(P(FVector((*RA)[0]->AsNumber(), (*RA)[1]->AsNumber(), 0.0))), FVector2D(P(FVector((*RA)[2]->AsNumber(), (*RA)[3]->AsNumber(), 0.0))));
				}
				V->Rooms.Add(Rm);
			}
		}
		if (R->TryGetArrayField(TEXT("boxes"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabBox B;
				B.C = P(VecField(O, TEXT("c")));
				B.S = VecField(O, TEXT("s")) * M;
				const FVector Rot = VecField(O, TEXT("r"));
				B.R = FRotator(Rot.X, Rot.Y, Rot.Z);
				B.Col = FLinearColor(FColor::FromHex(Str(O, TEXT("col"))));
				B.Kind = Str(O, TEXT("kind"));
				B.Room = Str(O, TEXT("room"));
				B.Tag = Str(O, TEXT("tag"));
				B.bClimb = Num(O, TEXT("climb")) > 0.5;
				B.bCollide = Num(O, TEXT("nocol")) < 0.5;
				V->Boxes.Add(B);
			}
		}
		if (R->TryGetArrayField(TEXT("zones"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabZone Z;
				Z.Kind = Str(O, TEXT("kind"));
				Z.Box = FBox(P(VecField(O, TEXT("min"))), P(VecField(O, TEXT("max"))));
				Z.Temp = (float)Num(O, TEXT("temp"));
				Z.Rate = (float)Num(O, TEXT("rate"));
				Z.Note = Str(O, TEXT("note"));
				V->Zones.Add(Z);
			}
		}
		if (R->TryGetArrayField(TEXT("hatches"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabHatchDef H;
				H.Id = Str(O, TEXT("id")); H.Name = Str(O, TEXT("name")); H.Note = Str(O, TEXT("note"));
				H.C = P(VecField(O, TEXT("c")));
				H.S = VecField(O, TEXT("s")) * M;
				H.Class = (int32)Num(O, TEXT("cls"), 1.0);
				const TArray<TSharedPtr<FJsonValue>>* UA = nullptr;
				if (O->TryGetArrayField(TEXT("use"), UA))
				{
					for (const TSharedPtr<FJsonValue>& U : *UA) { H.Use.Add(P(Vec(&U->AsArray()))); }
				}
				V->Hatches.Add(H);
			}
		}
		if (R->TryGetArrayField(TEXT("cover"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				V->Cover.Add(FBox(P(VecField(O, TEXT("min"))), P(VecField(O, TEXT("max")))));
				V->CoverNames.Add(Str(O, TEXT("name")));
			}
		}
		const TSharedPtr<FJsonObject>* NestO = nullptr;
		if (R->TryGetObjectField(TEXT("nest"), NestO))
		{
			V->Nest = P(VecField(*NestO, TEXT("main")));
			V->Nest2 = P(VecField(*NestO, TEXT("second")));
			V->Spawn = P(VecField(*NestO, TEXT("spawn")));
			V->NestRadius = (float)Num(*NestO, TEXT("radius"), 0.3) * M;
			V->SpawnRadius = (float)Num(*NestO, TEXT("spawn_r"), 0.2) * M;
		}
		if (R->TryGetArrayField(TEXT("food"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabFood F;
				F.At = P(VecField(O, TEXT("at")));
				F.Tier = (int32)Num(O, TEXT("tier"));
				F.Node = Str(O, TEXT("node")); F.Room = Str(O, TEXT("room")); F.What = Str(O, TEXT("what"));
				F.bHeavy = Num(O, TEXT("heavy")) > 0.5;
				V->Food.Add(F);
			}
		}
		const TSharedPtr<FJsonObject>* CakeO = nullptr;
		if (R->TryGetObjectField(TEXT("cake"), CakeO))
		{
			V->Cake = P(VecField(*CakeO, TEXT("at")));
			V->CakeNode = Str(*CakeO, TEXT("node"));
			V->CakeWhere = Str(*CakeO, TEXT("where"));
		}
		const TSharedPtr<FJsonObject>* HumO = nullptr;
		if (R->TryGetObjectField(TEXT("human"), HumO))
		{
			V->HumanSpawn = FVector2D(P(VecField(*HumO, TEXT("spawn"))));
			const TArray<TSharedPtr<FJsonValue>>* PA = nullptr;
			if ((*HumO)->TryGetArrayField(TEXT("patrol"), PA))
			{
				for (const TSharedPtr<FJsonValue>& J : *PA) { V->Patrol.Add(FVector2D(P(Vec(&J->AsArray())))); }
			}
		}
		if (R->TryGetArrayField(TEXT("nodes"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabNode N;
				N.Id = Str(O, TEXT("id")); N.Name = Str(O, TEXT("name")); N.Room = Str(O, TEXT("room"));
				N.At = P(VecField(O, TEXT("at")));
				N.bHole = Num(O, TEXT("hole")) > 0.5;
				V->Nodes.Add(N);
			}
		}
		if (R->TryGetArrayField(TEXT("edges"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& J : *Arr)
			{
				const TSharedPtr<FJsonObject> O = J->AsObject();
				FAntLabEdge E;
				E.A = V->NodeIndex(Str(O, TEXT("a")));
				E.B = V->NodeIndex(Str(O, TEXT("b")));
				E.Kind = Str(O, TEXT("kind"));
				E.RouteKind = RouteKindOf(E.Kind);
				E.Run = (float)Num(O, TEXT("run"));
				E.Seen = (float)Num(O, TEXT("seen"));
				E.Gate = Str(O, TEXT("gate"));
				if (E.A < 0 || E.B < 0) { Err = FString::Printf(TEXT("edge to an unknown node %s-%s"), *Str(O, TEXT("a")), *Str(O, TEXT("b"))); return nullptr; }
				V->Edges.Add(E);
			}
		}
		if (V->Nodes.Num() == 0 || V->Boxes.Num() == 0)
		{
			Err = TEXT("no nodes or no boxes");
			return nullptr;
		}
		return V;
	}

	bool LoadJson(TCHAR Id, FString& Out)
	{
		FString File;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntLabFile="), File) && FFileHelper::LoadFileToString(Out, *File))   // dev: try a layout without a rebuild
		{
			UE_LOG(LogTemp, Display, TEXT("ANTLAB data from %s"), *File);
			return true;
		}
		for (const FAntLabEmbedded& E : GAntLabEmbedded)
		{
			if (E.Id[0] == Id)
			{
				const FUTF8ToTCHAR Conv((const ANSICHAR*)E.Data, E.Size);
				Out = FString(Conv.Length(), Conv.Get());
				return true;
			}
		}
		return false;
	}

	float Rand(FRandomStream* Rng, float A, float B) { return Rng ? Rng->FRandRange(A, B) : FMath::FRandRange(A, B); }
}

int32 FAntLabVariant::NodeIndex(const FString& NodeId) const
{
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].Id == NodeId) { return i; }
	}
	return INDEX_NONE;
}

namespace AntLab
{

TCHAR Wanted()
{
	if (!GWantedInit)
	{
		GWantedInit = true;
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntLab="), V) && V.Len() > 0)
		{
			GWanted = FChar::ToUpper(V[0]);
		}
	}
	return GWanted;
}

void SetWanted(TCHAR Id)
{
	Wanted();
	GWanted = Id;
	UE_LOG(LogTemp, Display, TEXT("ANTLAB wanted for the next world: %s"), Id ? *FString::Chr(Id) : TEXT("none (the kitchen)"));
}

FString Available()
{
	FString S;
	for (const FAntLabEmbedded& E : GAntLabEmbedded) { S += E.Id; }
	return S;
}

FString VariantName(TCHAR Id)
{
	if (GLab && GLab->Id.Len() > 0 && GLab->Id[0] == Id) { return GLab->Name; }
	// (only the names; the menu shows them before anything is loaded)
	if (Id == TEXT('A')) { return TEXT("Двушка"); }
	if (Id == TEXT('B')) { return TEXT("Студия"); }
	if (Id == TEXT('C')) { return TEXT("Проходные комнаты"); }
	if (Id == TEXT('D')) { return TEXT("Евродвушка"); }
	if (Id == TEXT('E')) { return TEXT("Дом в два этажа"); }
	return FString();
}

bool Activate(TCHAR Id)
{
	if (Id == 0)
	{
		if (GLab) { UE_LOG(LogTemp, Display, TEXT("ANTLAB off: the kitchen")); }
		GLab.Reset();
		GOpenGates.Reset();
		return true;
	}
	if (GLab && GLab->Id.Len() > 0 && GLab->Id[0] == Id)
	{
		GOpenGates.Reset();
		return true;   // already loaded (a new raid world of the same variant)
	}
	FString Json, Err;
	if (!LoadJson(Id, Json))
	{
		UE_LOG(LogTemp, Error, TEXT("ANTLAB no data for variant %c (there is: %s): the kitchen"), Id, *Available());
		GLab.Reset();
		return false;
	}
	GLab = Parse(Json, Err);
	GOpenGates.Reset();
	if (!GLab)
	{
		UE_LOG(LogTemp, Error, TEXT("ANTLAB variant %c does not parse: %s"), Id, *Err);
		return false;
	}
	const FAntLabVariant& V = *GLab;
	int32 Climb = 0;
	for (const FAntLabBox& B : V.Boxes) { Climb += B.bClimb ? 1 : 0; }
	UE_LOG(LogTemp, Display, TEXT("ANTLAB variant %s «%s» loaded: %d rooms, %d boxes (%d climbable), %d zones, %d hatches, %d food, %d nodes, %d edges, nest %s, cake %s, Human %s"),
		*V.Id, *V.Name, V.Rooms.Num(), V.Boxes.Num(), Climb, V.Zones.Num(), V.Hatches.Num(), V.Food.Num(), V.Nodes.Num(), V.Edges.Num(),
		*V.Nest.ToCompactString(), *V.Cake.ToCompactString(), *V.HumanSpawn.ToString());
	return true;
}

const FAntLabVariant* Get() { return GLab.Get(); }

int32 RoomIndexAt(const FVector& P, float Grow)
{
	const FAntLabVariant* V = Get();
	if (!V) { return INDEX_NONE; }
	for (int32 i = 0; i < V->Rooms.Num(); ++i)
	{
		const FBox2D& R = V->Rooms[i].Rect;
		if (P.X >= R.Min.X - Grow && P.X <= R.Max.X + Grow && P.Y >= R.Min.Y - Grow && P.Y <= R.Max.Y + Grow) { return i; }
	}
	return INDEX_NONE;
}

bool InHouse(const FVector& P)
{
	const FAntLabVariant* V = Get();
	return V && P.X >= V->Bounds.Min.X - 300.f && P.X <= V->Bounds.Max.X + 300.f && P.Y >= V->Bounds.Min.Y - 300.f && P.Y <= V->Bounds.Max.Y + 300.f
		&& P.Z > -500.f && P.Z < V->Ceiling + V->Origin.Z + 200.f;
}

FVector HomeFor(const FVector& From)
{
	const FAntLabVariant* V = Get();
	if (!V) { return FVector::ZeroVector; }
	return FVector::DistSquared2D(From, V->Nest2) < FVector::DistSquared2D(From, V->Nest) ? V->Nest2 : V->Nest;
}

FVector SpawnPoint(FRandomStream* Rng)
{
	const FAntLabVariant* V = Get();
	if (!V) { return FVector::ZeroVector; }
	const float A = Rand(Rng, 0.f, 2.f * PI), D = Rand(Rng, 0.f, V->SpawnRadius);
	return V->Spawn + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 60.f);
}

FBox PlayArea()
{
	const FAntLabVariant* V = Get();
	if (!V) { return FBox(ForceInit); }
	return FBox(FVector(V->Bounds.Min, 0.f), FVector(V->Bounds.Max, 0.f));
}

FVector RandomFloorPoint(FRandomStream* Rng)
{
	const FAntLabVariant* V = Get();
	if (!V || V->Nodes.Num() == 0) { return FVector::ZeroVector; }
	// a floor node of the graph, a little jittered: always somewhere walkable and inside a room
	for (int32 Guard = 0; Guard < 40; ++Guard)
	{
		const int32 i = Rng ? Rng->RandRange(0, V->Nodes.Num() - 1) : FMath::RandRange(0, V->Nodes.Num() - 1);
		const FAntLabNode& N = V->Nodes[i];
		if (N.bHole || N.At.Z - V->Origin.Z > 50.f) { continue; }
		return N.At + FVector(Rand(Rng, -150.f, 150.f), Rand(Rng, -150.f, 150.f), 0.f);
	}
	return V->Spawn;
}

bool GateOpen(const FString& HatchId) { return HatchId.IsEmpty() || GOpenGates.Contains(HatchId); }
void SetGate(const FString& HatchId, bool bOpen) { if (bOpen) { GOpenGates.Add(HatchId); } else { GOpenGates.Remove(HatchId); } }

namespace
{
	/** Dijkstra over the variant's graph. Walk: floor, cover and loggia edges only (what a walker without hands can follow). */
	void Shortest(const FAntLabVariant& V, const TArray<int32>& From, bool bWalkOnly, TArray<float>& Dist, TArray<int32>& Prev, const TSet<FString>* Gates = nullptr,
		const FString& BannedRoom = FString())
	{
		const int32 N = V.Nodes.Num();
		Dist.Init(1.e9f, N);
		Prev.Init(INDEX_NONE, N);
		TArray<bool> Done;
		Done.Init(false, N);
		for (int32 S : From) { if (V.Nodes.IsValidIndex(S)) { Dist[S] = 0.f; } }
		for (int32 Step = 0; Step < N; ++Step)
		{
			int32 U = INDEX_NONE;
			for (int32 i = 0; i < N; ++i) { if (!Done[i] && Dist[i] < 1.e8f && (U == INDEX_NONE || Dist[i] < Dist[U])) { U = i; } }
			if (U == INDEX_NONE) { break; }
			Done[U] = true;
			for (const FAntLabEdge& E : V.Edges)
			{
				const int32 W = E.A == U ? E.B : E.B == U ? E.A : INDEX_NONE;
				if (W == INDEX_NONE || Done[W]) { continue; }
				if (!E.Gate.IsEmpty() && !(Gates ? Gates->Contains(E.Gate) : GateOpen(E.Gate))) { continue; }
				if (bWalkOnly && !(E.Kind == TEXT("floor") || E.Kind == TEXT("cover") || E.Kind == TEXT("loggia"))) { continue; }
				if (!BannedRoom.IsEmpty() && (V.Nodes[W].Room == BannedRoom || V.Nodes[U].Room == BannedRoom)) { continue; }
				if (Dist[U] + E.Run < Dist[W]) { Dist[W] = Dist[U] + E.Run; Prev[W] = U; }
			}
		}
	}
	int32 NearestNode(const FAntLabVariant& V, const FVector& P, bool bFloorOnly)
	{
		const int32 Room = RoomIndexAt(P, 30.f);
		const FString RoomId = V.Rooms.IsValidIndex(Room) ? V.Rooms[Room].Id : FString();
		int32 Best = INDEX_NONE, BestAny = INDEX_NONE;
		float BestD = 1.e18f, BestAnyD = 1.e18f;
		for (int32 i = 0; i < V.Nodes.Num(); ++i)
		{
			const FAntLabNode& N = V.Nodes[i];
			if (bFloorOnly && N.bHole) { continue; }
			const float D = (float)FVector::DistSquared(P, N.At);
			if (D < BestAnyD) { BestAnyD = D; BestAny = i; }
			if (D < BestD && (RoomId.IsEmpty() || N.Room == RoomId)) { BestD = D; Best = i; }
		}
		return Best != INDEX_NONE ? Best : BestAny;
	}
}

FVector Route(const FVector& From, const FVector& To)
{
	const FAntLabVariant* V = Get();
	if (!V) { return To; }
	const int32 RF = RoomIndexAt(From, 0.f), RT = RoomIndexAt(To, 0.f);
	if (RF == RT || RF == INDEX_NONE || RT == INDEX_NONE)
	{
		return To;
	}
	const int32 A = NearestNode(*V, From, true), B = NearestNode(*V, To, true);
	TArray<float> Dist;
	TArray<int32> Prev;
	Shortest(*V, { A }, true, Dist, Prev);
	if (!V->Nodes.IsValidIndex(B) || Dist[B] >= 1.e8f)
	{
		return To;
	}
	// The path A .. B; the first point not already reached (within 160) that is not behind us.
	TArray<int32> Path;
	for (int32 At = B; At != INDEX_NONE; At = Prev[At]) { Path.Insert(At, 0); }
	for (int32 k = 0; k < Path.Num(); ++k)
	{
		const FVector Pt = V->Nodes[Path[k]].At;
		if (FVector::Dist2D(Pt, From) > 160.f)
		{
			// skip the first node if the second one is nearer than it (we are past it already)
			if (k == 0 && Path.Num() > 1 && FVector::Dist2D(V->Nodes[Path[1]].At, From) < FVector::Dist2D(V->Nodes[Path[0]].At, V->Nodes[Path[1]].At)) { continue; }
			return FVector(Pt.X, Pt.Y, From.Z);
		}
	}
	return To;
}

void EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env)
{
	const FAntLabVariant* V = Get();
	if (!V || !Ant) { return; }
	const FVector P = Ant->GetActorLocation();
	for (const FAntLabZone& Z : V->Zones)
	{
		if (!Z.Box.ExpandBy(10.f).IsInside(P)) { continue; }
		if (Z.Kind == TEXT("wet")) { Env.Flags |= AntStatus::Wet; }
		else if (Z.Kind == TEXT("sticky")) { Env.Flags |= AntStatus::Sticky; }
		else if (Z.Kind == TEXT("steam")) { Env.Flags |= AntStatus::Wet; Env.Temp(Z.Temp, Z.Rate); }
		else if (Z.Kind == TEXT("hot") || Z.Kind == TEXT("cold")) { Env.Temp(Z.Temp, Z.Rate); }
		// wind, soft: data only for now (no engine effect yet)
	}
}

void SpawnHatches(UWorld* World)
{
	const FAntLabVariant* V = Get();
	if (!V || !World || World->GetNetMode() == NM_Client) { return; }
	for (int32 i = 0; i < V->Hatches.Num(); ++i)
	{
		const FTransform TM(V->Hatches[i].C);
		if (AAntLabHatch* H = World->SpawnActorDeferred<AAntLabHatch>(AAntLabHatch::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			H->HatchIndex = (uint8)i;
			H->FinishSpawning(TM);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTLAB %d hatches in the walls, all shut"), V->Hatches.Num());
}

void LogCheck()
{
	const FAntLabVariant* V = Get();
	if (!V) { UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK no lab variant active")); return; }
	const TArray<int32> Src = { V->NodeIndex(TEXT("nest")), V->NodeIndex(TEXT("nest2")) };
	TSet<FString> All;
	for (const FAntLabHatchDef& H : V->Hatches) { All.Add(H.Id); }
	const float Sprint = V->SprintMps;
	auto FoodTime = [V, Sprint](const TArray<float>& Dist, const FAntLabFood& F) -> float
	{
		const int32 N = V->NodeIndex(F.Node);
		if (N == INDEX_NONE || Dist[N] >= 1.e8f) { return -1.f; }
		const FVector D = (V->Nodes[N].At - F.At) / V->M;
		return Dist[N] + ((float)FVector2D(D.X, D.Y).Size() + FMath::Abs((float)D.Z) * 1.3f) / Sprint;
	};
	int32 Problems = 0;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const TSet<FString> Gates = Pass == 0 ? TSet<FString>() : All;
		const TCHAR* Label = Pass == 0 ? TEXT("hatches shut") : TEXT("hatches open");
		TArray<float> Dist; TArray<int32> Prev;
		Shortest(*V, Src, false, Dist, Prev, &Gates);
		for (const FAntLabRoom& R : V->Rooms)
		{
			float Best = -1.f; FString What;
			for (const FAntLabFood& F : V->Food)
			{
				if (F.Room != R.Id) { continue; }
				const float T = FoodTime(Dist, F);
				if (T >= 0.f && (Best < 0.f || T < Best)) { Best = T; What = F.What; }
			}
			const bool bOk = Best >= 0.f && Best <= 25.f;
			if (!bOk && Pass == 0) { ++Problems; }
			UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK %s %s: nest -> first food %s (%s) %s"), *V->Id, Label, Best >= 0.f ? *FString::Printf(TEXT("%.1f s"), Best) : TEXT("none"), *R.Id, bOk ? TEXT("ok") : TEXT("FAIL"));
		}
		int32 Unreach = 0;
		for (const FAntLabFood& F : V->Food) { if (FoodTime(Dist, F) < 0.f) { ++Unreach; UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK %s %s: unreachable food «%s» (%s)"), *V->Id, Label, *F.What, *F.Room); } }
		if (Unreach > 0 && Pass == 1) { ++Problems; }
		const int32 Cake = V->NodeIndex(V->CakeNode);
		if (Cake == INDEX_NONE) { UE_LOG(LogTemp, Error, TEXT("ANTLABCHECK the cake's node «%s» is not in the graph"), *V->CakeNode); ++Problems; break; }
		// ways in: each entrance into the cake's room with the others taken away
		int32 Ways = 0;
		TArray<int32> Entries;
		for (int32 e = 0; e < V->Edges.Num(); ++e)
		{
			const FAntLabEdge& E = V->Edges[e];
			const bool bA = V->Nodes[E.A].Room == V->Nodes[Cake].Room, bB = V->Nodes[E.B].Room == V->Nodes[Cake].Room;
			if (bA != bB) { Entries.Add(e); }
		}
		for (int32 Keep : Entries)
		{
			FAntLabVariant Cut = *V;
			Cut.Edges.Reset();
			for (int32 e = 0; e < V->Edges.Num(); ++e) { if (e == Keep || !Entries.Contains(e)) { Cut.Edges.Add(V->Edges[e]); } }
			TArray<float> D2; TArray<int32> P2;
			Shortest(Cut, Src, false, D2, P2, &Gates);
			const FAntLabEdge& E = V->Edges[Keep];
			const bool bWay = D2[Cake] < 1.e8f;
			Ways += bWay ? 1 : 0;
			UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK %s %s: cake via %s <-> %s (%s): %s"), *V->Id, Label, *V->Nodes[E.A].Name, *V->Nodes[E.B].Name, *E.Kind,
				bWay ? *FString::Printf(TEXT("%.1f s"), D2[Cake]) : TEXT("no way"));
		}
		if (Ways < 3 && Pass == 1) { ++Problems; }
		TArray<float> DB; TArray<int32> PB;
		Shortest(*V, Src, false, DB, PB, &Gates, TEXT("corridor"));
		FString Rooms;
		for (const FAntLabRoom& R : V->Rooms)
		{
			if (R.Id == TEXT("corridor")) { continue; }
			bool bAny = false;
			for (const FAntLabFood& F : V->Food) { bAny |= F.Room == R.Id && FoodTime(DB, F) >= 0.f; }
			Rooms += FString::Printf(TEXT("%s %s, "), *R.Id, bAny ? TEXT("yes") : TEXT("NO"));
		}
		UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK %s %s: cake fastest %.1f s, %d ways in; the Human in the corridor: cake %s, rooms %s"), *V->Id, Label,
			Dist.IsValidIndex(Cake) ? Dist[Cake] : -1.f, Ways, DB[Cake] < 1.e8f ? *FString::Printf(TEXT("%.1f s"), DB[Cake]) : TEXT("NO WAY"), *Rooms);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTLABCHECK %s done: %d problems"), *V->Id, Problems);
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (A.Num() < 2 || A[0].ToLower() != TEXT("lab"))
	{
		return false;
	}
	const FAntLabVariant* V = Get();
	const FString Sub = A[1].ToLower();
	UWorld* W = GM ? GM->GetWorld() : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (!V)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTLAB no lab variant in this world (start with -AntLab=A or pick it in the menu)"));
		return true;
	}
	if (Sub == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTLAB info: variant %s «%s», me at %s in %s, gates open: %s"), *V->Id, *V->Name, Me ? *Me->GetActorLocation().ToCompactString() : TEXT("-"),
			Me && V->Rooms.IsValidIndex(RoomIndexAt(Me->GetActorLocation())) ? *V->Rooms[RoomIndexAt(Me->GetActorLocation())].Id : TEXT("-"), *FString::Join(GOpenGates.Array(), TEXT(",")));
		int32 Loot = 0;
		if (W) { for (TActorIterator<AKitchenLoot> It(W); It; ++It) { ++Loot; } }
		UE_LOG(LogTemp, Display, TEXT("ANTLAB info: %d loot actors in the world"), Loot);
	}
	else if (Sub == TEXT("check")) { LogCheck(); }
	else if ((Sub == TEXT("open") || Sub == TEXT("shut")) && W)
	{
		const FString Which = A.IsValidIndex(2) ? A[2] : TEXT("all");
		for (TActorIterator<AAntLabHatch> It(W); It; ++It)
		{
			if (Which == TEXT("all") || (V->Hatches.IsValidIndex(It->HatchIndex) && V->Hatches[It->HatchIndex].Id == Which)) { It->SetDoorOpen(Sub == TEXT("open")); }
		}
	}
	else if (Sub == TEXT("tp") && Me && A.Num() >= 3)
	{
		FVector To = FVector::ZeroVector;
		const FString What = A[2];
		if (What == TEXT("cake")) { To = V->Cake + FVector(120.f, 0.f, 40.f); if (GM->MainObject.IsValid()) { To = GM->MainObject->GetActorLocation() + FVector(90.f, 0.f, 30.f); } }
		else if (What == TEXT("nest")) { To = V->Nest + FVector(0.f, -200.f, 40.f); }
		else if (What == TEXT("human"))
		{
			// right in front of the Human, on the floor (to see him stomp)
			for (TActorIterator<AHumanCharacter> It(W); It; ++It)
			{
				const FVector H = It->GetActorLocation();
				To = FVector(H.X, H.Y, V->Origin.Z + 40.f) + It->GetActorForwardVector().GetSafeNormal2D() * 520.f;
				break;
			}
		}
		else if (What == TEXT("food") && A.Num() >= 4 && V->Food.IsValidIndex(FCString::Atoi(*A[3]))) { To = V->Food[FCString::Atoi(*A[3])].At + FVector(80.f, 0.f, 40.f); }
		else if (V->NodeIndex(What) != INDEX_NONE) { To = V->Nodes[V->NodeIndex(What)].At + FVector(0.f, 0.f, 40.f); }
		if (!To.IsZero())
		{
			Me->ApplyTeleport(To);
			UE_LOG(LogTemp, Display, TEXT("ANTLAB tp %s -> %s"), *What, *To.ToCompactString());
		}
	}
	else if (Sub == TEXT("near") && Me && W)
	{
		// the loot nearest to me (what "interact" would pick up)
		AKitchenLoot* Best = nullptr; float BestD = 1.e12f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			const float D = (float)FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTLAB near: %s at %s, %.0f away, carrying %s"), Best ? *Best->LootName : TEXT("-"), Best ? *Best->GetActorLocation().ToCompactString() : TEXT("-"), BestD,
			Me->Carrying ? *Me->Carrying->LootName : TEXT("nothing"));
	}
	else if (Sub == TEXT("grab") && Me && W)
	{
		// dev: step up to the nearest loose food and take it in the jaws (the same [E] a player presses)
		AKitchenLoot* Best = nullptr; float BestD = 1.e12f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->bIsGear || It->bMain || It->IsCarried()) { continue; }
			const float D = (float)FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		if (Best)
		{
			Me->ApplyTeleport(Best->GetActorLocation() + FVector(60.f, 0.f, 20.f));
			UE_LOG(LogTemp, Display, TEXT("ANTLAB grab: going for «%s» at %s"), *Best->LootName, *Best->GetActorLocation().ToCompactString());
		}
	}
	else if (Sub == TEXT("ceiling"))
	{
		const bool bShow = A.IsValidIndex(2) ? FCString::Atoi(*A[2]) != 0 : true;
		if (W)
		{
			for (TActorIterator<AKitchenBuilder> It(W); It; ++It)
			{
				TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
				for (UStaticMeshComponent* C : Parts) { if (C && C->ComponentHasTag(TEXT("LabCeiling"))) { C->SetVisibility(bShow); } }
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("ANTLAB commands: lab info | check | open <id|all> | shut <id|all> | tp <node|food N|cake|nest> | near | grab | ceiling 0|1"));
	}
	return true;
}

}   // namespace AntLab

// =============================================================================================
// The hatch
// =============================================================================================

AAntLabHatch::AAntLabHatch()
{
	InteractRadius = 260.f;
	SetReplicatingMovement(false);
}

void AAntLabHatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntLabHatch, bOpen);
	DOREPLIFETIME(AAntLabHatch, HatchIndex);
}

void AAntLabHatch::BeginPlay()
{
	Super::BeginPlay();
	EnsurePlate();
}

void AAntLabHatch::EnsurePlate()
{
	const FAntLabVariant* V = AntLab::Get();
	if (Plate || !V || !V->Hatches.IsValidIndex(HatchIndex))
	{
		return;
	}
	const FAntLabHatchDef& D = V->Hatches[HatchIndex];
	Plate = BoxPart(nullptr, -D.S * 0.5f, D.S * 0.5f, FLinearColor(0.85f, 0.12f, 0.1f), true);
	if (Plate) { Plate->ComponentTags.Add(TEXT("AntNoClimb")); }
	UsePointsLocal.Reset();
	for (const FVector& U : D.Use) { UsePointsLocal.Add(U - GetActorLocation()); }
	UE_LOG(LogTemp, Display, TEXT("ANTLAB hatch «%s» in place on this machine (%s), %s"), *D.Id, GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), bOpen ? TEXT("open") : TEXT("shut"));
	Apply();
}

void AAntLabHatch::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Plate) { EnsurePlate(); }
	if (!bOpen) { PullTick(Dt); }
}

FString AAntLabHatch::GetPrompt(const AAntCharacter* Ant) const
{
	const FAntLabVariant* V = AntLab::Get();
	const FString Name = V && V->Hatches.IsValidIndex(HatchIndex) ? V->Hatches[HatchIndex].Name : FString(TEXT("Люк"));
	return bOpen ? FString::Printf(TEXT("%s: открыт"), *Name) : FString::Printf(TEXT("[E] %s: тянуть, чтобы открыть"), *Name);
}

int32 AAntLabHatch::DoorClass() const
{
	const FAntLabVariant* V = AntLab::Get();
	return V && V->Hatches.IsValidIndex(HatchIndex) ? FMath::Clamp(V->Hatches[HatchIndex].Class, 1, 3) : 1;
}

FString AAntLabHatch::DoorNoun() const
{
	const FAntLabVariant* V = AntLab::Get();
	return V && V->Hatches.IsValidIndex(HatchIndex) ? V->Hatches[HatchIndex].Name : FString(TEXT("люк"));
}

void AAntLabHatch::SetDoorOpen(bool bInOpen)
{
	if (!HasAuthority() || bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	if (bOpen) { OpenedAt = GetWorld()->GetTimeSeconds(); }
	const FAntLabVariant* V = AntLab::Get();
	UE_LOG(LogTemp, Display, TEXT("ANTLAB hatch «%s» %s"), V && V->Hatches.IsValidIndex(HatchIndex) ? *V->Hatches[HatchIndex].Id : TEXT("?"), bOpen ? TEXT("opened") : TEXT("shut"));
	Apply();
	ForceNetUpdate();
}

void AAntLabHatch::OnRep_Open()
{
	UE_LOG(LogTemp, Display, TEXT("ANTLAB hatch %d %s (replicated)"), (int32)HatchIndex, bOpen ? TEXT("opened") : TEXT("shut"));
	Apply();
}

void AAntLabHatch::Apply()
{
	const FAntLabVariant* V = AntLab::Get();
	if (V && V->Hatches.IsValidIndex(HatchIndex)) { AntLab::SetGate(V->Hatches[HatchIndex].Id, bOpen); }
	if (Plate)
	{
		Plate->SetVisibility(!bOpen);
		Plate->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}
