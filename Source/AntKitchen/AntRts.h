#pragma once

#include "CoreMinimal.h"
#include "AntBotController.h"
#include "Components/ActorComponent.h"
#include "AntRts.generated.h"

class AAntCharacter;
class AAntPlayerController;
class AKitchenLoot;
class APlayerState;
class AHUD;
class UCanvas;
class UInputComponent;
struct FAntSnowPlayer;

/**
 * A smart squad ant of the snowball raid: a real ant (AAntCharacter) driven by the bot brain, owned by one player.
 * It follows him, helps him carry, fights, and carries out its group's order (AntTask): haul a load home along the route
 * graph, hold a spot, attack, or forage freely. Far from every player it thinks less often.
 */
UCLASS()
class ANTKITCHEN_API AAntSquadBot : public AAntBotController
{
	GENERATED_BODY()

public:
	AAntSquadBot();
	virtual void Tick(float DeltaSeconds) override;

	TWeakObjectPtr<APlayerState> OwnerPS;
	/** The haul: route waypoints home and where it is on them (only the leader of a load walks them). */
	TArray<FVector> Way;
	int32 WayAt = 0;
	TWeakObjectPtr<AActor> WayFor;
	float StuckFor = 0.f;
	float LeashDeadline = 0.f;   // follow watchdog: far from the owner and not getting closer by this time -> come out right behind him
	float LeashBest = 0.f;
	FVector LastHaulPos = FVector::ZeroVector;
	float NextShootPick = 0.f;
	int32 Slot = 0;
	/** Think at once (a new order). */
	void Wake() { ThinkTimer = 0.f; }

private:
	void ThinkRts(AAntCharacter* Ant, float Dt);
	bool HaulThink(AAntCharacter* Ant, AKitchenLoot* L, uint8 TaskIdx);
	/** Every frame while it carries for its group: push towards HaulGoal, keep under its grip. */
	bool HaulMove(AAntCharacter* Ant);
	FVector HaulGoal = FVector::ZeroVector;
	bool bHauling = false;
	AAntCharacter* OwnerAnt() const;
	float ThinkEvery = 0.5f;
};

/**
 * The local RTS controls (made for the local player controller only). Hold [Caps Lock]: the mouse shows, drag a box over
 * your squad ants (or click one), [Shift] adds; right click: "тащи это" on food, "атакуй" on an enemy, "держи здесь" on
 * the floor. While Caps Lock is held [Z] за мной, [X] атакуй, [Q] тащи, [B] держи use the cursor; without it they use the
 * crosshair and go to the selection (to the whole squad when nothing is selected). [Caps]+[7..0] saves a group, [7..0]
 * selects it again. The selection is local; orders and the selection itself go to the server (ServerSwarm).
 */
UCLASS()
class ANTKITCHEN_API UAntRtsInput : public UActorComponent
{
	GENERATED_BODY()

public:
	UAntRtsInput();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	static UAntRtsInput* Attach(AAntPlayerController* PC);
	static UAntRtsInput* Of(const APlayerController* PC);

	bool bRtsMode = false;
	bool bDragging = false;
	/** Dev: the RTS view stays on without Caps Lock (scripted shots). */
	bool bDevHold = false;
	FVector2D DragFrom = FVector2D::ZeroVector;
	FVector2D Mouse = FVector2D::ZeroVector;
	TArray<TWeakObjectPtr<AAntCharacter>> Selection;
	TArray<TWeakObjectPtr<AAntCharacter>> Groups[4];
	/** Screen size the HUD last drew at (for the dev box in screen fractions). */
	FVector2D Screen = FVector2D(1600.f, 900.f);
	float LastOrderAt = -100.f;
	FString LastOrderText;

	// ---- actions (keys and dev commands)
	void SelectBox(const FVector2D& A, const FVector2D& B, bool bAdd);
	void SelectAll();
	void ClearSelection();
	void Order(uint8 Task, bool bAtCursor);
	void OrderAt(uint8 Task, AActor* Target, const FVector& Point);
	void SaveGroup(int32 G);
	void RecallGroup(int32 G);
	/** Dev: "rts ..." commands; returns true if handled. */
	bool Dev(const TArray<FString>& A);
	void SendSelection();
	/** What is under the cursor / the crosshair: a load, an enemy or a point. */
	void Probe(bool bAtCursor, AActor*& OutActor, FVector& OutPoint) const;
	TArray<AAntCharacter*> MySquad() const;

private:
	AAntPlayerController* PC() const;
	void EnterRts();
	void LeaveRts();
	void OnCapsDown();
	void OnCapsUp();
	void OnLmbDown();
	void OnLmbUp();
	void OnRmb();
	void KeyZ() { Order(0, true); }
	void KeyX() { Order(3, true); }
	void KeyQ() { Order(1, true); }
	void KeyB() { Order(2, true); }
	void Key7() { NumKey(0); }
	void Key8() { NumKey(1); }
	void Key9() { NumKey(2); }
	void Key0() { NumKey(3); }
	void NumKey(int32 G);
	void ScriptTick();

	UPROPERTY() UInputComponent* Always = nullptr;
	UPROPERTY() UInputComponent* Modal = nullptr;
	TArray<TPair<float, FString>> Script;
	float ScriptStart = -1.f;
};

namespace AntRts
{
	/** Server: spawn N squad ants for this player at his ant (or the nest). Class: 0 forager, 1 soldier. */
	ANTKITCHEN_API int32 Spawn(APlayerState* PS, int32 N, uint8 Class);
	/** Server: every squad ant of the raid goes (the raid ends / a new one starts). */
	ANTKITCHEN_API void DespawnAll(UWorld* World);
	/** Server: the old squad keys (Z, X, C, Q, B) in the snowball raid: to the selection or the whole squad. True: handled. */
	ANTKITCHEN_API bool SquadKey(AAntCharacter* Ant, uint8 OldOrder, const FVector& Aim);
	/** Server: the alarm scent: the whole squad to its owner. */
	ANTKITCHEN_API bool RecallAll(AAntCharacter* Ant);
	/** Server: a squad ant died. True: it was one (no respawn, no kill feed line). */
	ANTKITCHEN_API bool OnSquadBotKilled(AAntCharacter* Ant);
	/** Server: the RPC body (AAntPlayerController::ServerSwarm). */
	ANTKITCHEN_API void ServerCommand(AAntPlayerController* PC, uint8 Cmd, int32 A, const TArray<AAntCharacter*>& Ants, const FVector& Point, AActor* Target);
	/** Server: give these ants (of this player) a task. */
	ANTKITCHEN_API void Assign(FAntSnowPlayer& P, const TArray<AAntCharacter*>& Ants, uint8 Task, AActor* Target, const FVector& Point);
	/** Server: once a second: tasks done, members gone, the HUD state of each task. */
	ANTKITCHEN_API void TasksSecond(FAntSnowPlayer& P);
	/** The HUD of the local player: selection box, highlight, group list, loads in progress, the pick cards and the mass meter. */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, AAntPlayerController* PC);
	/** Commands of the RPC. */
	enum ECmd : uint8 { CmdOrder, CmdSelect, CmdPick, CmdCount };
}
