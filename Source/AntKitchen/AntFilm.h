#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "AntFilm.generated.h"

class AAntPlayerController;
class AHUD;
class ACameraActor;
class UInputComponent;
class UCanvas;

/**
 * Filming mode (docs/FILMING.md): the marketer's camera kit for vertical clips, on every machine (host, guests, solo).
 * F8 switches it on. Then: the HUD on a key, a free camera, five presets, camera slots kept in a file, a dolly between two
 * slots, a follow camera on anything alive, slow motion (host), a 9:16 safe frame, and the director's keys (host): the
 * Human, the pets and the family do things at the point under the crosshair. Nothing here is reachable without F8.
 * All of it is local to this player's machine except slow motion and the director, which run on the host only.
 */
namespace AntFilm
{
	/** Server: "no attacks" - the Human, the pets and the family play every motion but nothing they do harms an ant. */
	ANTKITCHEN_API bool NoHarm();
	ANTKITCHEN_API void SetNoHarm(bool bOn);
	/** Local: a jolt for the filming cameras (the Human hitting the floor). Amount 0..1. */
	ANTKITCHEN_API void Shake(const UObject* WorldContext, const FVector& At, float Amount, float Radius);
}

UCLASS()
class ANTKITCHEN_API UAntFilm : public UActorComponent
{
	GENERATED_BODY()

public:
	UAntFilm();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Made by the player controller for its local player. */
	void Init(AAntPlayerController* InPC);

	bool IsOn() const { return bOn; }
	/** The HUD is switched off (filming mode, F2): the game HUD draws nothing of its own. */
	bool HidesHud() const { return bOn && bHideHud; }
	/** Drawn by AAntHUD last, over everything: the 9:16 frame, the hints when the HUD is hidden, the panel. */
	void DrawOverlay(AHUD* Hud, UCanvas* Canvas);

	/** The keys of filming mode (also what -AntFilmDo "key X" simulates through the real input path). */
	void OnKey(FKey Key);
	void OnWheelUp();
	void OnWheelDown();

	float ShakeAmount = 0.f;

private:
	enum class ECam : uint8 { Player, Free, K1, K2, K3, K4, Follow, Dolly, Slot };
	struct FSlot { bool bSet = false; FVector Loc = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator; float Fov = 70.f; };

	void SetOn(bool bInOn);
	void SetCam(ECam NewCam);
	void UseViewCamera();
	void BackToPlayer();
	FVector ViewLoc() const;
	FRotator ViewRot() const;
	float ViewFov() const;
	/** The point under the middle of the current view (the director aims with it). */
	bool AimPoint(FVector& Out) const;
	void Toast(const FString& Text, bool bWarn = false);
	bool IsHost() const;

	void TickFree(float Dt);
	void TickPreset(float Dt);
	void TickFollow(float Dt);
	void TickDolly(float Dt);
	void ApplyCamera(const FVector& Loc, const FRotator& Rot, float Fov, float Dt, float Smooth);
	void UpdateBlocking();

	void StoreSlot(int32 Index);
	void RecallSlot(int32 Index);
	void StartDolly(int32 From, int32 To);
	void LoadSlots();
	void SaveSlots() const;

	void CycleFollow(int32 Step);
	void GatherTargets(TArray<AActor*>& Out) const;
	FString TargetName(const AActor* A) const;

	void SlowMo();
	void Director(FKey Key);
	void ScriptTick();
	void RunScript(const FString& Cmd);

	UPROPERTY() AAntPlayerController* PC = nullptr;
	UPROPERTY() UInputComponent* Input = nullptr;
	UPROPERTY() ACameraActor* Cam = nullptr;

	bool bOn = false;
	bool bHideHud = false;
	bool bHints = true;
	bool bFrame = false;
	bool bPanel = true;
	ECam Mode = ECam::Player;
	ECam BeforeSlot = ECam::Player;
	int32 Smoothing = 1;      // 0 off, 1 soft, 2 very soft
	float FreeSpeed = 400.f;  // uu per real second
	float DollySeconds = 6.f;
	int32 Pending = 0;        // 0 none, 1 store into, 2 recall, 3 dolly from, 4 dolly to
	int32 DollyFrom = -1;
	FSlot Slots[7];

	// the camera as shown (smoothed) and the free camera's wishes
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	float CamFov = 70.f;
	bool bCamInit = false;
	FVector FreeVel = FVector::ZeroVector;
	float WantYaw = 0.f, WantPitch = 0.f;
	FVector DevFly = FVector::ZeroVector;   // -AntFilmDo "fly": a held direction for a while (scripted tests cannot hold keys)
	float DevFlyUntil = 0.f;

	TWeakObjectPtr<AActor> FollowTarget;
	float FollowDist = 1.f;   // multiplies the framing distance (mouse wheel)
	float DollyT = 0.f;
	FSlot DollyA, DollyB;

	FString Note;             // the panel's last line (what just happened)
	float NoteTime = -100.f;
	bool bNoteWarn = false;

	double RealNow = 0.0;
	TArray<TPair<float, FString>> Script;   // -AntFilmDo="5:key F8;6:key NumPadOne;9:shot k1" (local, every machine)
	float ScriptClock = 0.f;
};
