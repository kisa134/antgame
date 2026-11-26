#include "AntFilm.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "KitchenLoot.h"
#include "KitchenInteractables.h"
#include "KitchenSlick.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

// =============================================================================================
// Shared switches
// =============================================================================================

namespace
{
	bool GNoHarm = false;
	const FLinearColor FilmGold(1.f, 0.82f, 0.3f), FilmGrey(0.78f, 0.8f, 0.84f), FilmRed(1.f, 0.42f, 0.32f), FilmGreen(0.55f, 1.f, 0.6f);

	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }
}

namespace AntFilm
{
	bool NoHarm() { return GNoHarm; }
	void SetNoHarm(bool bOn) { GNoHarm = bOn; }

	void Shake(const UObject* WorldContext, const FVector& At, float Amount, float Radius)
	{
		const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!W)
		{
			return;
		}
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			const AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
			if (PC && PC->IsLocalController() && PC->Film && PC->PlayerCameraManager)
			{
				const float D = (float)FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), At);
				PC->Film->ShakeAmount = FMath::Min(1.f, PC->Film->ShakeAmount + Amount * FMath::Clamp(1.f - D / FMath::Max(1.f, Radius), 0.f, 1.f));
			}
		}
	}
}

// =============================================================================================
// Setup
// =============================================================================================

UAntFilm::UAntFilm()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAntFilm::Init(AAntPlayerController* InPC)
{
	PC = InPC;
	if (!PC)
	{
		return;
	}
	Input = NewObject<UInputComponent>(PC, TEXT("AntFilmInput"));
	Input->Priority = 50;
	Input->bBlockInput = false;
	static const FKey Keys[] =
	{
		EKeys::F8, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7,
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::Divide, EKeys::Multiply, EKeys::Decimal,
		EKeys::PageUp, EKeys::PageDown,
		EKeys::U, EKeys::I, EKeys::O, EKeys::N, EKeys::M, EKeys::Comma, EKeys::Period, EKeys::Slash,
		EKeys::LeftBracket, EKeys::RightBracket, EKeys::End, EKeys::Home, EKeys::Backslash, EKeys::Insert, EKeys::Delete,
	};
	for (const FKey& K : Keys)
	{
		Input->BindKey(K, IE_Pressed, this, &UAntFilm::OnKey);
	}
	Input->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &UAntFilm::OnWheelUp);
	Input->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &UAntFilm::OnWheelDown);
	PC->PushInputComponent(Input);
	LoadSlots();

	// -AntFilmDo="5:key F8;6:key NumPadOne;9:shot k1": the same keys, pressed by a script (tests without a keyboard). Local: works on a guest too.
	FString Do;
	const bool bJoining = FString(FCommandLine::Get()).Contains(TEXT("-AntJoin=")) && GetWorld()->GetNetMode() == NM_Standalone;
	if (!bJoining && FParse::Value(FCommandLine::Get(), TEXT("AntFilmDo="), Do, false))
	{
		TArray<FString> Steps;
		Do.ParseIntoArray(Steps, TEXT(";"));
		for (const FString& S : Steps)
		{
			FString At, Cmd;
			if (S.Split(TEXT(":"), &At, &Cmd))
			{
				Script.Add(TPair<float, FString>(FCString::Atof(*At), Cmd.TrimStartAndEnd()));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFILM script with %d steps (%s)"), Script.Num(), IsHost() ? TEXT("host") : TEXT("guest"));
	}
}

void UAntFilm::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PC && Input)
	{
		PC->PopInputComponent(Input);
	}
	if (Cam)
	{
		Cam->Destroy();
		Cam = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UAntFilm::IsHost() const
{
	return PC && PC->GetNetMode() != NM_Client;
}

void UAntFilm::Toast(const FString& Text, bool bWarn)
{
	Note = Text;
	NoteTime = (float)RealNow;
	bNoteWarn = bWarn;
	UE_LOG(LogTemp, Display, TEXT("ANTFILM %s: %s"), IsHost() ? TEXT("host") : TEXT("guest"), *Text);
}

FVector UAntFilm::ViewLoc() const { return PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector; }
FRotator UAntFilm::ViewRot() const { return PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator; }
float UAntFilm::ViewFov() const { return PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 70.f; }

bool UAntFilm::AimPoint(FVector& Out) const
{
	const FVector From = ViewLoc();
	const FVector Dir = ViewRot().Vector();
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AntFilmAim), false, PC ? PC->GetPawn() : nullptr);
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
	for (TActorIterator<ACritter> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
	if (Cam) { Q.AddIgnoredActor(Cam); }
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * 40000.f, ECC_Visibility, Q))
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	return false;
}

// =============================================================================================
// Switching
// =============================================================================================

void UAntFilm::SetOn(bool bInOn)
{
	bOn = bInOn;
	if (!bOn)
	{
		BackToPlayer();
		bHideHud = false;
		bFrame = false;
		Pending = 0;
		if (IsHost() && UGameplayStatics::GetGlobalTimeDilation(GetWorld()) != 1.f)
		{
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
		}
	}
	UpdateBlocking();
	Toast(bOn ? TEXT("Режим съёмки включён. F6 — спрятать эту панель") : TEXT("Режим съёмки выключен"));
}

void UAntFilm::UpdateBlocking()
{
	if (Input)
	{
		// The free camera owns the keyboard: the ant gets nothing and stays where it is (safe for the operator).
		Input->bBlockInput = bOn && Mode == ECam::Free;
	}
}

void UAntFilm::UseViewCamera()
{
	UWorld* W = GetWorld();
	if (!Cam)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		Cam = W->SpawnActor<ACameraActor>(ViewLoc(), ViewRot(), P);
		if (Cam)
		{
			Cam->SetReplicates(false);
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	if (!Cam)
	{
		return;
	}
	if (PC->GetViewTarget() != Cam || !bCamInit)
	{
		CamLoc = ViewLoc();
		const FRotator R = ViewRot();
		CamRot = FRotator(R.Pitch, R.Yaw, 0.f);
		CamFov = ViewFov();
		WantYaw = (float)CamRot.Yaw;
		WantPitch = (float)CamRot.Pitch;
		FreeVel = FVector::ZeroVector;
		bCamInit = true;
		Cam->SetActorLocationAndRotation(CamLoc, CamRot);
		Cam->GetCameraComponent()->SetFieldOfView(CamFov);
		PC->SetViewTargetWithBlend(Cam, 0.f);
	}
}

void UAntFilm::BackToPlayer()
{
	Mode = ECam::Player;
	bCamInit = false;
	FollowTarget = nullptr;
	if (PC && PC->GetPawn() && PC->GetViewTarget() != PC->GetPawn())
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f);
	}
	UpdateBlocking();
}

void UAntFilm::SetCam(ECam NewCam)
{
	if (NewCam == ECam::Player || NewCam == ECam::K4)
	{
		BackToPlayer();
		if (AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn()))
		{
			Ant->SetBodycam(NewCam == ECam::K4);
		}
		Mode = NewCam;
		UpdateBlocking();
		return;
	}
	if (AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn()))
	{
		Ant->SetBodycam(false);
	}
	UseViewCamera();
	if (NewCam == ECam::Free)
	{
		WantYaw = (float)CamRot.Yaw;   // fly on from wherever the camera is looking now
		WantPitch = (float)CamRot.Pitch;
		FreeVel = FVector::ZeroVector;
	}
	Mode = NewCam;
	UpdateBlocking();
}

// =============================================================================================
// Keys
// =============================================================================================

void UAntFilm::OnWheelUp()
{
	if (!bOn) { return; }
	if (Mode == ECam::Follow) { FollowDist = FMath::Clamp(FollowDist / 1.15f, 0.25f, 6.f); }
	else { FreeSpeed = FMath::Clamp(FreeSpeed * 1.25f, 5.f, 20000.f); Toast(FString::Printf(TEXT("Скорость камеры: %.0f"), FreeSpeed)); }
}

void UAntFilm::OnWheelDown()
{
	if (!bOn) { return; }
	if (Mode == ECam::Follow) { FollowDist = FMath::Clamp(FollowDist * 1.15f, 0.25f, 6.f); }
	else { FreeSpeed = FMath::Clamp(FreeSpeed / 1.25f, 5.f, 20000.f); Toast(FString::Printf(TEXT("Скорость камеры: %.0f"), FreeSpeed)); }
}

void UAntFilm::OnKey(FKey Key)
{
	if (!PC)
	{
		return;
	}
	if (Key == EKeys::F8)
	{
		SetOn(!bOn);
		return;
	}
	if (!bOn)
	{
		return;   // nothing of filming mode without F8
	}
	UE_LOG(LogTemp, Display, TEXT("ANTFILM key %s (mode %d, pending %d)"), *Key.ToString(), (int32)Mode, Pending);

	// A digit after Num/ (store), Num* (recall) or Num. (dolly from, then to).
	static const FKey Digits[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix };
	int32 Digit = -1;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Digits); ++i)
	{
		if (Key == Digits[i]) { Digit = i; }
	}
	if (Pending != 0)
	{
		if (Digit >= 1 && Digit <= 6)
		{
			const int32 P = Pending;
			Pending = 0;
			if (P == 1) { StoreSlot(Digit); }
			else if (P == 2) { RecallSlot(Digit); }
			else if (P == 3) { DollyFrom = Digit; Pending = 4; Toast(FString::Printf(TEXT("Проезд из слота %d: теперь цифра слота, куда ехать"), Digit)); }
			else if (P == 4) { StartDolly(DollyFrom, Digit); }
			return;
		}
		Pending = 0;
		Toast(TEXT("Отменено"));
		if (Digit < 0) { /* fall through: the key does its own thing */ } else { return; }
	}

	if (Key == EKeys::F2) { bHideHud = !bHideHud; Toast(bHideHud ? TEXT("HUD скрыт (F2 — показать)") : TEXT("HUD виден")); }
	else if (Key == EKeys::F3) { bHints = !bHints; Toast(bHints ? TEXT("Подсказки в мире видны при скрытом HUD («Нужно ещё N»)") : TEXT("Подсказки в мире скрыты")); }
	else if (Key == EKeys::F4) { bFrame = !bFrame; Toast(bFrame ? TEXT("Рамка 9:16 включена") : TEXT("Рамка 9:16 выключена")); }
	else if (Key == EKeys::F6) { bPanel = !bPanel; }
	else if (Key == EKeys::F7) { Smoothing = (Smoothing + 1) % 3; Toast(FString::Printf(TEXT("Сглаживание: %s"), Smoothing == 0 ? TEXT("выкл") : Smoothing == 1 ? TEXT("мягкое") : TEXT("очень мягкое"))); }
	else if (Key == EKeys::F5)
	{
		if (Mode == ECam::Free) { SetCam(ECam::Player); Toast(TEXT("Свободная камера выключена")); }
		else { SetCam(ECam::Free); Toast(TEXT("Свободная камера: WASD, Q/E вниз/вверх, мышь, колесо — скорость. Муравей стоит на месте")); }
	}
	else if (Key == EKeys::NumPadOne) { SetCam(ECam::K1); Toast(TEXT("K1: у пола за муравьём")); }
	else if (Key == EKeys::NumPadTwo) { SetCam(ECam::K2); Toast(TEXT("K2: великан снизу")); }
	else if (Key == EKeys::NumPadThree)
	{
		SetCam(ECam::K3);
		bCamInit = true;
		TickPreset(0.f);   // K3 is placed once and stays
		Toast(TEXT("K3: общий план для масштаба"));
	}
	else if (Key == EKeys::NumPadFour) { SetCam(ECam::K4); Toast(TEXT("K4: бодикам на голове муравья")); }
	else if (Key == EKeys::NumPadFive) { SetCam(ECam::Player); Toast(TEXT("K5: обычная камера игрока")); }
	else if (Key == EKeys::NumPadSeven) { CycleFollow(-1); }
	else if (Key == EKeys::NumPadNine) { CycleFollow(1); }
	else if (Key == EKeys::NumPadEight) { SetCam(ECam::Player); Toast(TEXT("Слежение выключено")); }
	else if (Key == EKeys::NumPadZero) { SlowMo(); }
	else if (Key == EKeys::NumPadSix) { Toast(TEXT("Num6 не занят. Слоты: Num/ + цифра, Num* + цифра"), true); }
	else if (Key == EKeys::Divide) { Pending = 1; Toast(TEXT("Запомнить камеру: нажми цифру слота Num1–Num6")); }
	else if (Key == EKeys::Multiply) { Pending = 2; Toast(TEXT("Вызвать слот: нажми цифру Num1–Num6")); }
	else if (Key == EKeys::Decimal) { Pending = 3; Toast(FString::Printf(TEXT("Проезд за %.0f с: цифра слота, откуда ехать"), DollySeconds)); }
	else if (Key == EKeys::PageUp) { DollySeconds = FMath::Min(30.f, DollySeconds + 1.f); Toast(FString::Printf(TEXT("Проезд: %.0f с"), DollySeconds)); }
	else if (Key == EKeys::PageDown) { DollySeconds = FMath::Max(1.f, DollySeconds - 1.f); Toast(FString::Printf(TEXT("Проезд: %.0f с"), DollySeconds)); }
	else { Director(Key); }
}

// =============================================================================================
// Cameras
// =============================================================================================

void UAntFilm::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Dt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);   // real time: smooth in slow motion too
	RealNow += Dt;
	ScriptClock += Dt;
	ScriptTick();
	ShakeAmount = FMath::Max(0.f, ShakeAmount - Dt * 1.3f);
	if (!PC || !bOn || Mode == ECam::Player || Mode == ECam::K4)
	{
		return;
	}
	if (!Cam)
	{
		UseViewCamera();
	}
	if (Cam && PC->GetViewTarget() != Cam)
	{
		PC->SetViewTargetWithBlend(Cam, 0.f);   // a respawn handed the view back to the pawn
	}
	switch (Mode)
	{
	case ECam::Free: TickFree(Dt); break;
	case ECam::K1: case ECam::K2: TickPreset(Dt); break;
	case ECam::Follow: TickFollow(Dt); break;
	case ECam::Dolly: TickDolly(Dt); break;
	default: ApplyCamera(CamLoc, CamRot, CamFov, Dt, 0.f); break;
	}
}

void UAntFilm::ApplyCamera(const FVector& Loc, const FRotator& Rot, float Fov, float Dt, float Smooth)
{
	if (Smooth > 0.f && Dt > 0.f)
	{
		const float K = 1.f - FMath::Exp(-Dt / Smooth);
		CamLoc = FMath::Lerp(CamLoc, Loc, K);
		CamRot = FQuat::Slerp(CamRot.Quaternion(), FRotator(Rot.Pitch, Rot.Yaw, 0.f).Quaternion(), K).Rotator();
		CamFov = FMath::Lerp(CamFov, Fov, K);
	}
	else
	{
		CamLoc = Loc;
		CamRot = Rot;
		CamFov = Fov;
	}
	CamRot.Roll = 0.f;   // the horizon stays level
	FRotator Shown = CamRot;
	if (ShakeAmount > 0.f)
	{
		const float A = ShakeAmount * ShakeAmount;
		const float T = (float)RealNow;
		Shown.Pitch += A * 2.2f * FMath::Sin(T * 37.f);
		Shown.Yaw += A * 1.6f * FMath::Sin(T * 29.f + 1.3f);
	}
	if (Cam)
	{
		Cam->SetActorLocationAndRotation(CamLoc, Shown);
		Cam->GetCameraComponent()->SetFieldOfView(CamFov);
	}
}

void UAntFilm::TickFree(float Dt)
{
	// Mouse: yaw and pitch only (no roll ever). Keys are read straight from the player input: the stack is ours.
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	WantYaw += DX * 1.4f;
	WantPitch = FMath::Clamp(WantPitch + DY * 1.4f, -88.f, 88.f);
	FVector Move = FVector::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::W)) { Move.X += 1.f; }
	if (PC->IsInputKeyDown(EKeys::S)) { Move.X -= 1.f; }
	if (PC->IsInputKeyDown(EKeys::D)) { Move.Y += 1.f; }
	if (PC->IsInputKeyDown(EKeys::A)) { Move.Y -= 1.f; }
	if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::SpaceBar)) { Move.Z += 1.f; }
	if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::LeftControl)) { Move.Z -= 1.f; }
	if (RealNow < DevFlyUntil) { Move = DevFly; }
	float Speed = FreeSpeed;
	if (PC->IsInputKeyDown(EKeys::LeftShift)) { Speed *= 3.f; }
	if (PC->IsInputKeyDown(EKeys::LeftAlt)) { Speed *= 0.25f; }
	const FRotator YawOnly(0.f, WantYaw, 0.f);
	const FVector Want = (FRotator(WantPitch, WantYaw, 0.f).Vector() * Move.X + FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y) * Move.Y + FVector::UpVector * Move.Z).GetClampedToMaxSize(1.f) * Speed;
	// Velocity eases in and out (a dolly, not a jump), the look eases after the mouse.
	static const float VelEase[3] = { 0.03f, 0.25f, 0.6f };
	static const float LookEase[3] = { 0.f, 0.12f, 0.35f };
	FreeVel = FMath::Lerp(FreeVel, Want, 1.f - FMath::Exp(-Dt / VelEase[Smoothing]));
	const FVector Loc = CamLoc + FreeVel * Dt;
	const FRotator Rot(WantPitch, WantYaw, 0.f);
	if (LookEase[Smoothing] > 0.f)
	{
		const float K = 1.f - FMath::Exp(-Dt / LookEase[Smoothing]);
		const FRotator R = FQuat::Slerp(CamRot.Quaternion(), Rot.Quaternion(), K).Rotator();
		ApplyCamera(Loc, FRotator(R.Pitch, R.Yaw, 0.f), CamFov, Dt, 0.f);
	}
	else
	{
		ApplyCamera(Loc, Rot, CamFov, Dt, 0.f);
	}
}

namespace
{
	AHumanCharacter* FindDad(const UWorld* W, const FVector& Near)
	{
		AHumanCharacter* Best = nullptr;
		float BestD = 1.e12f;
		for (TActorIterator<AHumanCharacter> It(const_cast<UWorld*>(W)); It; ++It)
		{
			if (It->IsA<AHouseFolk>()) { continue; }
			const float D = (float)FVector::DistSquared(It->GetActorLocation(), Near);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** Keeps a camera point out of walls and furniture: pulled back towards Look if something stands in between. */
	FVector Unblock(const UWorld* W, const FVector& Look, const FVector& Want, const AActor* Ignore)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntFilmCam), false, Ignore);
		for (TActorIterator<AHumanCharacter> It(const_cast<UWorld*>(W)); It; ++It) { Q.AddIgnoredActor(*It); }
		for (TActorIterator<ACritter> It(const_cast<UWorld*>(W)); It; ++It) { Q.AddIgnoredActor(*It); }
		for (TActorIterator<AAntCharacter> It(const_cast<UWorld*>(W)); It; ++It) { Q.AddIgnoredActor(*It); }
		if (W->SweepSingleByChannel(Hit, Look, Want, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(6.f), Q))
		{
			return Hit.Location;
		}
		return Want;
	}
}

void UAntFilm::TickPreset(float Dt)
{
	const UWorld* W = GetWorld();
	AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
	static const float Ease[3] = { 0.f, 0.15f, 0.4f };
	if (Mode == ECam::K1)
	{
		// K1: on the floor 1-2 cm behind the ant, looking along its way and a little up: the load, and the Human's feet ahead.
		if (!Ant || Ant->bDead)
		{
			return;
		}
		const FVector Up = Ant->GetVisualUp().GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector Fwd = FVector::VectorPlaneProject(Ant->GetActorForwardVector(), Up).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector Foot = Ant->GetActorLocation() - Up * Ant->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Look = Foot + Fwd * 700.f + Up * 110.f;
		const FVector Want = Unblock(W, Foot + Up * 25.f, Foot + Up * 26.f - Fwd * 95.f, Ant);
		ApplyCamera(Want, FRotationMatrix::MakeFromXZ(Look - Want, Up).Rotator(), 78.f, Dt, Ease[Smoothing]);
		return;
	}
	const FVector Me = Ant ? Ant->GetActorLocation() : ViewLoc();
	AHumanCharacter* H = FindDad(W, Me);
	if (Mode == ECam::K2)
	{
		// K2: the giant from below. The camera lies on the floor in front of his feet, looking up at the knee and the slipper.
		if (!H)
		{
			return;
		}
		const FVector Floor(H->GetActorLocation().X, H->GetActorLocation().Y, H->FeetZ());
		const FVector Fwd = H->GetActorForwardVector().GetSafeNormal2D(), Right = H->GetActorRightVector().GetSafeNormal2D();
		const FVector Look = Floor + Fwd * 380.f + FVector(0.f, 0.f, 1050.f);
		const FVector Want = Unblock(W, Floor + Fwd * 600.f + FVector(0.f, 0.f, 30.f), Floor + Fwd * 1500.f + Right * 420.f + FVector(0.f, 0.f, 28.f), H);
		ApplyCamera(Want, (Look - Want).Rotation(), 84.f, Dt, FMath::Max(0.25f, Ease[Smoothing]));
		return;
	}
	if (Mode == ECam::K3 && Dt == 0.f)
	{
		// K3: one wide shot that shows the scale: the ant and the Human together, from the side and above. Placed once.
		FVector Look, Want;
		if (H && Ant && FVector::Dist2D(H->GetActorLocation(), Me) < 9000.f)
		{
			const FVector HF(H->GetActorLocation().X, H->GetActorLocation().Y, H->FeetZ());
			const FVector Mid = (HF + Me) * 0.5f;
			const float Span = FMath::Max(2800.f, (float)FVector::Dist2D(HF, Me) * 1.3f);
			const FVector Along = Flat(HF - Me).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
			Look = Mid + FVector(0.f, 0.f, 900.f);
			const FVector A = Unblock(W, Look, Mid + Side * Span + FVector(0.f, 0.f, Span * 0.6f), nullptr);
			const FVector B = Unblock(W, Look, Mid - Side * Span + FVector(0.f, 0.f, Span * 0.6f), nullptr);
			Want = FVector::DistSquared(A, Look) >= FVector::DistSquared(B, Look) ? A : B;
		}
		else
		{
			const FVector Fwd = Ant ? Ant->GetActorForwardVector().GetSafeNormal2D() : ViewRot().Vector().GetSafeNormal2D();
			Look = Me;
			Want = Unblock(W, Me + FVector(0.f, 0.f, 40.f), Me - Fwd * 1500.f + FVector(0.f, 0.f, 900.f), Ant);
		}
		CamLoc = Want;
		CamRot = (Look - Want).Rotation();
		CamFov = 62.f;
		ApplyCamera(CamLoc, CamRot, CamFov, 0.f, 0.f);
	}
}

void UAntFilm::GatherTargets(TArray<AActor*>& Out) const
{
	UWorld* W = GetWorld();
	for (TActorIterator<AHumanCharacter> It(W); It; ++It) { Out.Add(*It); }
	for (TActorIterator<ACritter> It(W); It; ++It) { Out.Add(*It); }
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		if (It->Weight >= 50 && !It->bIsGear) { Out.Add(*It); }   // the cake (and any other big prize)
	}
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		if (!It->bDead && It->GetPlayerState() && *It != PC->GetPawn()) { Out.Add(*It); }   // the other players
	}
}

FString UAntFilm::TargetName(const AActor* A) const
{
	if (const AHumanCharacter* H = Cast<AHumanCharacter>(A)) { return H->GetDisplayName(); }
	if (const ACritter* C = Cast<ACritter>(A)) { return C->DisplayName(); }
	if (const AKitchenLoot* L = Cast<AKitchenLoot>(A)) { return L->LootName; }
	if (const AAntCharacter* Ant = Cast<AAntCharacter>(A)) { return Ant->GetDisplayName(); }
	return GetNameSafe(A);
}

void UAntFilm::CycleFollow(int32 Step)
{
	TArray<AActor*> All;
	GatherTargets(All);
	if (All.Num() == 0)
	{
		Toast(TEXT("Следить не за кем"), true);
		return;
	}
	int32 i = All.IndexOfByKey(FollowTarget.Get());
	i = i < 0 ? (Step > 0 ? 0 : All.Num() - 1) : (i + Step + All.Num()) % All.Num();
	FollowTarget = All[i];
	if (Mode != ECam::Follow)
	{
		SetCam(ECam::Follow);
	}
	Toast(FString::Printf(TEXT("Следим за: %s (%d из %d, колесо — ближе/дальше)"), *TargetName(All[i]), i + 1, All.Num()));
}

void UAntFilm::TickFollow(float Dt)
{
	AActor* T = FollowTarget.Get();
	if (!T)
	{
		return;
	}
	FVector Origin, Ext;
	T->GetActorBounds(true, Origin, Ext);
	const float Size = FMath::Clamp((float)Ext.Size(), 15.f, 4000.f);
	FVector Fwd = T->GetVelocity().GetSafeNormal2D();
	if (Fwd.IsNearlyZero()) { Fwd = T->GetActorForwardVector().GetSafeNormal2D(); }
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd);
	const float Dist = FMath::Max(60.f, Size * 2.4f) * FollowDist;
	const FVector Look = Origin + FVector(0.f, 0.f, Size * 0.15f);
	const FVector Want = Unblock(GetWorld(), Look, Look - Fwd * Dist * 0.8f + Side * Dist * 0.55f + FVector(0.f, 0.f, Dist * 0.35f), T);
	static const float Ease[3] = { 0.08f, 0.3f, 0.7f };
	ApplyCamera(Want, (Look - Want).Rotation(), 60.f, Dt, Ease[Smoothing]);
}

// =============================================================================================
// Slots and the dolly
// =============================================================================================

namespace
{
	FString SlotFile() { return FPaths::ProjectSavedDir() / TEXT("film_slots.txt"); }
}

void UAntFilm::LoadSlots()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SlotFile()))
	{
		return;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	int32 N = 0;
	for (const FString& L : Lines)
	{
		TArray<FString> F;
		L.ParseIntoArrayWS(F);
		if (F.Num() >= 7)
		{
			const int32 i = FCString::Atoi(*F[0]);
			if (i >= 1 && i <= 6)
			{
				FSlot& S = Slots[i];
				S.bSet = true;
				S.Loc = FVector(FCString::Atof(*F[1]), FCString::Atof(*F[2]), FCString::Atof(*F[3]));
				S.Rot = FRotator(FCString::Atof(*F[4]), FCString::Atof(*F[5]), 0.f);
				S.Fov = FMath::Clamp(FCString::Atof(*F[6]), 10.f, 140.f);
				++N;
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTFILM loaded %d camera slots from %s"), N, *SlotFile());
}

void UAntFilm::SaveSlots() const
{
	FString Text;
	for (int32 i = 1; i <= 6; ++i)
	{
		const FSlot& S = Slots[i];
		if (S.bSet)
		{
			Text += FString::Printf(TEXT("%d %.1f %.1f %.1f %.2f %.2f %.1f\n"), i, S.Loc.X, S.Loc.Y, S.Loc.Z, S.Rot.Pitch, S.Rot.Yaw, S.Fov);
		}
	}
	FFileHelper::SaveStringToFile(Text, *SlotFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UAntFilm::StoreSlot(int32 Index)
{
	FSlot& S = Slots[Index];
	S.bSet = true;
	S.Loc = ViewLoc();
	S.Rot = FRotator(ViewRot().Pitch, ViewRot().Yaw, 0.f);
	S.Fov = ViewFov();
	SaveSlots();
	Toast(FString::Printf(TEXT("Камера запомнена в слот %d (%s)"), Index, *S.Loc.ToCompactString()));
}

void UAntFilm::RecallSlot(int32 Index)
{
	const FSlot& S = Slots[Index];
	if (!S.bSet)
	{
		Toast(FString::Printf(TEXT("Слот %d пуст: Num/ и цифра — запомнить"), Index), true);
		return;
	}
	SetCam(ECam::Slot);
	CamLoc = S.Loc;
	CamRot = S.Rot;
	CamFov = S.Fov;
	WantYaw = (float)S.Rot.Yaw;
	WantPitch = (float)S.Rot.Pitch;
	ApplyCamera(CamLoc, CamRot, CamFov, 0.f, 0.f);
	Toast(FString::Printf(TEXT("Слот %d. F5 — лететь отсюда свободно"), Index));
}

void UAntFilm::StartDolly(int32 From, int32 To)
{
	if (!Slots[From].bSet || !Slots[To].bSet)
	{
		Toast(FString::Printf(TEXT("Для проезда нужны оба слота: %d и %d"), From, To), true);
		return;
	}
	DollyA = Slots[From];
	DollyB = Slots[To];
	DollyT = 0.f;
	SetCam(ECam::Dolly);
	CamLoc = DollyA.Loc;
	CamRot = DollyA.Rot;
	CamFov = DollyA.Fov;
	ApplyCamera(CamLoc, CamRot, CamFov, 0.f, 0.f);
	Toast(FString::Printf(TEXT("Проезд: слот %d -> слот %d за %.0f с"), From, To, DollySeconds));
}

void UAntFilm::TickDolly(float Dt)
{
	DollyT = FMath::Min(1.f, DollyT + Dt / FMath::Max(0.5f, DollySeconds));
	const float K = FMath::InterpEaseInOut(0.f, 1.f, DollyT, 2.2f);
	const FVector L = FMath::Lerp(DollyA.Loc, DollyB.Loc, K);
	const FRotator R = FQuat::Slerp(DollyA.Rot.Quaternion(), DollyB.Rot.Quaternion(), K).Rotator();
	ApplyCamera(L, FRotator(R.Pitch, R.Yaw, 0.f), FMath::Lerp(DollyA.Fov, DollyB.Fov, K), Dt, 0.f);
	if (DollyT >= 1.f)
	{
		Mode = ECam::Slot;
		WantYaw = (float)CamRot.Yaw;
		WantPitch = (float)CamRot.Pitch;
		UE_LOG(LogTemp, Display, TEXT("ANTFILM dolly done at %s"), *CamLoc.ToCompactString());
	}
}

// =============================================================================================
// Slow motion and the director (host)
// =============================================================================================

void UAntFilm::SlowMo()
{
	if (!IsHost())
	{
		Toast(TEXT("Замедление — только у хоста (оно для всех сразу)"), true);
		return;
	}
	const float Now = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	const float Next = Now > 0.75f ? 0.5f : Now > 0.375f ? 0.25f : 1.f;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Next);
	Toast(FString::Printf(TEXT("Скорость игры: %.2f"), Next));
}

namespace
{
	/** Puts the Human DistFromPoint away from P on his side of it, facing it (if he is not already in a good place for it). */
	void PlaceHuman(AHumanCharacter* H, const FVector& P, float Near, float Far, float Dist)
	{
		const FVector To = Flat(P - H->GetActorLocation());
		const float D = (float)To.Size();
		const FVector Dir = To.GetSafeNormal(UE_SMALL_NUMBER, H->GetActorForwardVector());
		const FRotator Face(0.f, (float)Dir.Rotation().Yaw, 0.f);
		if (D < Near || D > Far)
		{
			const FVector At = P - Dir * Dist;
			H->SetActorLocation(FVector(At.X, At.Y, H->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
		H->SetActorRotation(Face);
		if (AController* C = H->GetController()) { C->SetControlRotation(Face); }
	}

	/** The AI keeps its hands off him for a moment (the action plays out), then gets him back unless he was frozen already. */
	void HoldHuman(AHumanCharacter* H, float Seconds)
	{
		if (H->bDevPuppet)
		{
			return;
		}
		H->bDevPuppet = true;
		H->DevWalk = FVector::ZeroVector;
		FTimerHandle T;
		H->GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateWeakLambda(H, [H]() { H->bDevPuppet = false; }), Seconds, false);
	}

	template<class T> T* FindOne(UWorld* W, TFunctionRef<bool(T*)> Pred)
	{
		for (TActorIterator<T> It(W); It; ++It)
		{
			if (Pred(*It)) { return *It; }
		}
		return nullptr;
	}
}

void UAntFilm::Director(FKey Key)
{
	static const FKey Keys[] = { EKeys::U, EKeys::I, EKeys::O, EKeys::N, EKeys::M, EKeys::Comma, EKeys::Period, EKeys::Slash, EKeys::LeftBracket,
		EKeys::RightBracket, EKeys::End, EKeys::Home, EKeys::Backslash, EKeys::Insert, EKeys::Delete };
	bool bDirector = false;
	for (const FKey& K : Keys) { bDirector |= K == Key; }
	if (!bDirector)
	{
		return;
	}
	if (!IsHost())
	{
		Toast(TEXT("Режиссёрские клавиши — только у хоста"), true);
		return;
	}
	UWorld* W = GetWorld();
	AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>();
	FVector P;
	const bool bAim = AimPoint(P);
	AHumanCharacter* H = FindDad(W, bAim ? P : ViewLoc());
	AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn());
	auto NeedAim = [&]() { if (!bAim) { Toast(TEXT("Под прицелом ничего нет: наведи центр экрана на пол"), true); } return bAim; };
	auto NeedHuman = [&]() { if (!H) { Toast(TEXT("Человека нет на уровне"), true); } return H != nullptr; };

	if (Key == EKeys::U && NeedAim() && NeedHuman())
	{
		// The stomp lands right on the point.
		PlaceHuman(H, P, 0.f, 0.f, 600.f);
		const int32 Leg = FVector::DotProduct(P - H->GetActorLocation(), H->GetActorRightVector()) < 0.f ? 0 : 1;
		const FVector Off = H->GetActorLocation() - H->GetStompPoint(Leg);
		H->SetActorLocation(FVector(P.X + Off.X, P.Y + Off.Y, H->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		HoldHuman(H, 2.f);
		H->DevAct(Leg ? TEXT("stomp1") : TEXT("stomp0"), P);
		Toast(TEXT("Человек: топнуть сюда"));
	}
	else if (Key == EKeys::I && NeedAim() && NeedHuman())
	{
		PlaceHuman(H, P, 800.f, 1300.f, 1050.f);
		HoldHuman(H, 2.5f);
		H->SwatReadyTime = 0.f;
		H->TrySwat(FVector(P.X, P.Y, H->FeetZ() + 20.f), 0);
		Toast(TEXT("Человек: шлёпнуть ладонью по полу"));
	}
	else if (Key == EKeys::O && NeedAim() && NeedHuman())
	{
		PlaceHuman(H, P, 1100.f, 2300.f, 1600.f);
		HoldHuman(H, 2.5f);
		H->SwatReadyTime = 0.f;
		H->TrySwat(P, 1);
		Toast(TEXT("Человек: газетой сюда"));
	}
	else if (Key == EKeys::N && NeedAim() && NeedHuman())
	{
		PlaceHuman(H, P, 600.f, 1600.f, 1150.f);
		HoldHuman(H, 6.5f);
		H->DevAct(TEXT("peer"), P);
		Toast(TEXT("Человек: заглянуть под мебель"));
	}
	else if (Key == EKeys::M && NeedAim() && NeedHuman())
	{
		PlaceHuman(H, P, 500.f, 1500.f, 900.f);
		HoldHuman(H, 2.5f);
		H->DevAct(TEXT("grab"), P);
		Toast(H->Held ? FString::Printf(TEXT("Человек схватил «%s»"), *H->Held->LootName) : FString(TEXT("Человек тянется за едой (рядом с точкой еды нет — рука пустая)")));
	}
	else if (Key == EKeys::Comma && NeedAim() && NeedHuman())
	{
		H->bDevPuppet = true;
		const FVector Dir = Flat(P - H->GetActorLocation()).GetSafeNormal();
		H->DevWalk = FVector(P.X, P.Y, 0.f) - Dir * 650.f;
		H->DevWalkScale = 0.8f;
		Toast(TEXT("Человек: иди сюда (потом стоит; / — отпустить)"));
	}
	else if (Key == EKeys::Period && NeedHuman())
	{
		H->bDevPuppet = true;
		const FVector From = bAim ? P : ViewLoc();
		const FVector Away = Flat(H->GetActorLocation() - From).GetSafeNormal(UE_SMALL_NUMBER, -H->GetActorForwardVector());
		H->DevWalk = H->GetActorLocation() + Away * 4500.f;
		H->DevWalkScale = 0.7f;
		Toast(TEXT("Человек: уходит (/ — отпустить)"));
	}
	else if (Key == EKeys::Slash && NeedHuman())
	{
		H->bDevPuppet = !H->bDevPuppet;
		H->DevWalk = FVector::ZeroVector;
		H->SetSpraying(false);
		Toast(H->bDevPuppet ? TEXT("Человек замер (/ — отпустить)") : TEXT("Человек снова сам по себе"));
	}
	else if (Key == EKeys::LeftBracket && NeedAim())
	{
		ACritter* Cat = FindOne<ACritter>(W, [](ACritter* C) { return C->Kind == 1; });
		if (!Cat) { Toast(TEXT("Кошки нет"), true); return; }
		const FVector Dir = Flat(P - Cat->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, Cat->GetActorForwardVector());
		if (FVector::Dist2D(Cat->GetActorLocation(), P) > 2600.f || FVector::Dist2D(Cat->GetActorLocation(), P) < 500.f)
		{
			Cat->DevPlace(FVector(P.X, P.Y, P.Z) - Dir * 1100.f, (float)Dir.Rotation().Yaw);
		}
		const bool bWas = Cat->bDevPuppet;
		Cat->bDevPuppet = true;
		Cat->DevAct(TEXT("wiggle"), P);
		TWeakObjectPtr<ACritter> WeakCat = Cat;
		FTimerHandle T1, T2;
		W->GetTimerManager().SetTimer(T1, FTimerDelegate::CreateWeakLambda(Cat, [WeakCat, P]() { if (WeakCat.IsValid()) { WeakCat->DevAct(TEXT("pounce"), P); } }), 1.1f, false);
		W->GetTimerManager().SetTimer(T2, FTimerDelegate::CreateWeakLambda(Cat, [WeakCat, bWas]() { if (WeakCat.IsValid()) { WeakCat->bDevPuppet = bWas; } }), 4.f, false);
		Toast(TEXT("Кошка: виляет задом и прыгает сюда"));
	}
	else if (Key == EKeys::RightBracket && NeedAim())
	{
		AKidCharacter* Kid = FindOne<AKidCharacter>(W, [](AKidCharacter*) { return true; });
		AAntCharacter* Prey = nullptr;
		float Best = 700.f;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			const float D = (float)FVector::Dist(It->GetActorLocation(), P);
			if (!It->bDead && It->Creature == 0 && D < Best) { Best = D; Prey = *It; }
		}
		if (!Prey) { Prey = Me; }
		if (!Kid || !Prey) { Toast(TEXT("Нет малыша или муравья у точки"), true); return; }
		if (FVector::Dist2D(Kid->GetActorLocation(), Prey->GetActorLocation()) > 3200.f)
		{
			const FVector Dir = Flat(Prey->GetActorLocation() - Kid->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector At = Prey->GetActorLocation() - Dir * 1500.f;
			TArray<FString> Args;
			Args.Add(TEXT("kid")); Args.Add(TEXT("to")); Args.Add(FString::SanitizeFloat(At.X)); Args.Add(FString::SanitizeFloat(At.Y)); Args.Add(FString::SanitizeFloat(Dir.Rotation().Yaw));
			Kid->DevCommand(Args);
		}
		Kid->DirectorGrab(Prey);
		Toast(FString::Printf(TEXT("Малыш идёт хватать: %s"), *Prey->GetDisplayName()));
	}
	else if (Key == EKeys::End && NeedAim())
	{
		AGrandmaCharacter* Gran = FindOne<AGrandmaCharacter>(W, [](AGrandmaCharacter*) { return true; });
		if (!Gran) { Toast(TEXT("Бабушки нет"), true); return; }
		Gran->DirectorSweep(P);
		Toast(TEXT("Бабушка: метлой сюда"));
	}
	else if (Key == EKeys::Home)
	{
		// Everyone else in the house to sleep, or awake again.
		static bool bAsleep = false;
		bAsleep = !bAsleep;
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			It->bDevPuppet = bAsleep;
			It->DevWalk = FVector::ZeroVector;
			It->DevAct(bAsleep ? TEXT("sleep") : TEXT("stand"), It->GetActorLocation());
		}
		for (TActorIterator<AHouseFolk> It(W); It; ++It)
		{
			It->DirectorDoze(bAsleep);
		}
		Toast(bAsleep ? TEXT("Питомцы и семья спят (Home — разбудить)") : TEXT("Питомцы и семья проснулись"));
	}
	else if (Key == EKeys::Backslash)
	{
		AntFilm::SetNoHarm(!AntFilm::NoHarm());
		Toast(AntFilm::NoHarm() ? TEXT("Без атак: все замахиваются, но никого не ранят") : TEXT("Атаки снова настоящие"));
	}
	else if (Key == EKeys::Insert && NeedAim())
	{
		AKitchenSlick::Spill(W, P, SlickKind::Oil, Me);
		Toast(TEXT("Лужа масла под прицелом"));
	}
	else if (Key == EKeys::Delete && NeedHuman())
	{
		// He slips right where he stands (a puddle appears under his feet if there is none).
		const FVector Feet(H->GetActorLocation().X, H->GetActorLocation().Y, H->FeetZ());
		if (!AKitchenSlick::At(W, Feet, 300.f))
		{
			AKitchenSlick::Spill(W, Feet + H->GetActorForwardVector() * 250.f, SlickKind::Water, Me, 650.f);
		}
		const bool bOk = H->TrySlip(Feet, 1.f, true, Me);
		Toast(bOk ? TEXT("Человек поскользнулся!") : TEXT("Сейчас он упасть не может (уже лежит или оглушён)"), !bOk);
	}
	if (GM && Key != EKeys::Home && Key != EKeys::Backslash)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFILM director %s at %s"), *Key.ToString(), bAim ? *P.ToCompactString() : TEXT("-"));
	}
}

// =============================================================================================
// The panel and the 9:16 frame
// =============================================================================================

void UAntFilm::DrawOverlay(AHUD* Hud, UCanvas* Canvas)
{
	if (!bOn || !Hud || !Canvas || !GEngine)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float S = FMath::Max(0.7f, H / 1080.f);
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	auto Txt = [&](const FString& Str, float X, float Y, const FLinearColor& C, UFont* F, float Sc, bool bCenter = false)
	{
		float TW = 0.f, TH = 0.f;
		if (bCenter) { Hud->GetTextSize(Str, TW, TH, F, Sc); }
		Hud->DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.8f * C.A), X - TW * 0.5f + 1.5f, Y + 1.5f, F, Sc);
		Hud->DrawText(Str, C, X - TW * 0.5f, Y, F, Sc);
	};

	// Hints that live in the world, when the HUD is hidden (F3): "Нужно ещё N" over a door or bottle that will not give.
	if (HidesHud() && bHints)
	{
		const FVector CamL = ViewLoc();
		const FRotator CamR = ViewRot();
		for (TActorIterator<AKitchenInteractable> It(GetWorld()); It; ++It)
		{
			if (It->PullMissing > 0 && !It->IsDoorOpen() && CamR.UnrotateVector(It->GetActorLocation() - CamL).X > 1.f)
			{
				const FVector P = Hud->Project(It->GetActorLocation() + FVector(0.f, 0.f, 120.f));
				Txt(FString::Printf(TEXT("Нужно ещё %d"), (int32)It->PullMissing), P.X, P.Y, FLinearColor(1.f, 0.6f, 0.2f), Med, 1.3f * S, true);
			}
		}
		if (const AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn()))
		{
			if (Ant->Pulling && Ant->Pulling->PullMissing > 0)
			{
				Txt(FString::Printf(TEXT("Нужно ещё %d"), (int32)Ant->Pulling->PullMissing), W * 0.5f, H * 0.5f + 70.f * S, FLinearColor(1.f, 0.6f, 0.2f), Med, 1.3f * S, true);
			}
		}
	}

	if (bFrame)
	{
		// The 9:16 crop: what stays in a vertical clip, the platform's buttons on the right, the caption zones top and bottom.
		const float BW = FMath::Min(W, H * 9.f / 16.f), BH = BW * 16.f / 9.f;
		const float X0 = (W - BW) * 0.5f, Y0 = (H - BH) * 0.5f;
		const FLinearColor Mask(0.f, 0.f, 0.f, 0.55f), Edge(1.f, 1.f, 1.f, 0.9f), Zone(1.f, 0.35f, 0.3f, 0.18f), ZoneEdge(1.f, 0.45f, 0.4f, 0.8f);
		Hud->DrawRect(Mask, 0.f, 0.f, X0, H);
		Hud->DrawRect(Mask, X0 + BW, 0.f, W - X0 - BW, H);
		auto Box = [&](float X, float Y, float BoxW, float BoxH, const FLinearColor& Fill, const FLinearColor& Line, float T)
		{
			if (Fill.A > 0.f) { Hud->DrawRect(Fill, X, Y, BoxW, BoxH); }
			Hud->DrawRect(Line, X, Y, BoxW, T);
			Hud->DrawRect(Line, X, Y + BoxH - T, BoxW, T);
			Hud->DrawRect(Line, X, Y, T, BoxH);
			Hud->DrawRect(Line, X + BoxW - T, Y, T, BoxH);
		};
		Box(X0, Y0, BW, BH, FLinearColor::Transparent, Edge, 2.f);
		// right-side buttons (like, comment, share, profile): 14% of the width, 38%..86% of the height
		Box(X0 + BW * 0.86f, Y0 + BH * 0.38f, BW * 0.14f - 2.f, BH * 0.48f, Zone, ZoneEdge, 1.f);
		Txt(TEXT("кнопки"), X0 + BW * 0.93f, Y0 + BH * 0.62f, ZoneEdge, Small, 0.9f * S, true);
		// bottom: nickname and caption (20%), top: the app's header (11%)
		Box(X0 + 2.f, Y0 + BH * 0.80f, BW * 0.84f, BH * 0.20f - 2.f, Zone, ZoneEdge, 1.f);
		Txt(TEXT("ник и подпись"), X0 + BW * 0.42f, Y0 + BH * 0.88f, ZoneEdge, Small, 0.9f * S, true);
		Box(X0 + 2.f, Y0 + 2.f, BW - 4.f, BH * 0.11f, Zone, ZoneEdge, 1.f);
		Txt(TEXT("шапка приложения"), X0 + BW * 0.5f, Y0 + BH * 0.045f, ZoneEdge, Small, 0.9f * S, true);
		// thirds and the middle
		const FLinearColor Third(1.f, 1.f, 1.f, 0.18f);
		Hud->DrawRect(Third, X0 + BW / 3.f, Y0, 1.f, BH);
		Hud->DrawRect(Third, X0 + BW * 2.f / 3.f, Y0, 1.f, BH);
		Hud->DrawRect(Third, X0, Y0 + BH / 3.f, BW, 1.f);
		Hud->DrawRect(Third, X0, Y0 + BH * 2.f / 3.f, BW, 1.f);
		Hud->DrawRect(Edge, X0 + BW * 0.5f - 8.f, Y0 + BH * 0.5f, 16.f, 1.f);
		Hud->DrawRect(Edge, X0 + BW * 0.5f, Y0 + BH * 0.5f - 8.f, 1.f, 16.f);
	}

	if (bPanel)
	{
		const bool bHost = IsHost();
		const float Slomo = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		static const TCHAR* ModeNames[] = { TEXT("камера игрока"), TEXT("свободная"), TEXT("K1 у пола"), TEXT("K2 великан снизу"), TEXT("K3 масштаб"), TEXT("K4 бодикам"), TEXT("слежение"), TEXT("проезд"), TEXT("слот") };
		FString SlotLine = TEXT("Слоты:");
		for (int32 i = 1; i <= 6; ++i) { SlotLine += FString::Printf(TEXT("  %d%s"), i, Slots[i].bSet ? TEXT("+") : TEXT("-")); }
		const AHumanCharacter* Dad = FindDad(GetWorld(), ViewLoc());
		TArray<FString> L;
		L.Add(FString::Printf(TEXT("РЕЖИМ СЪЁМКИ — %s   (F8 выйти, F6 спрятать панель)"), bHost ? TEXT("хост") : TEXT("гость")));
		L.Add(FString::Printf(TEXT("Камера: %s%s   скорость %.0f   сглаживание %s"), ModeNames[(int32)Mode],
			Mode == ECam::Follow && FollowTarget.IsValid() ? *(TEXT(": ") + TargetName(FollowTarget.Get())) : TEXT(""), FreeSpeed,
			Smoothing == 0 ? TEXT("выкл") : Smoothing == 1 ? TEXT("мягкое") : TEXT("очень мягкое")));
		L.Add(FString::Printf(TEXT("F2 HUD: %s   F3 подсказки: %s   F4 рамка 9:16: %s   F7 сглаживание"), bHideHud ? TEXT("скрыт") : TEXT("виден"), bHints ? TEXT("да") : TEXT("нет"), bFrame ? TEXT("да") : TEXT("нет")));
		L.Add(TEXT("F5 свободная камера: WASD, E/Пробел вверх, Q/Ctrl вниз, Shift быстрее, Alt медленнее, колесо — скорость"));
		L.Add(TEXT("Num1 K1 у пола · Num2 K2 снизу на Человека · Num3 K3 масштаб · Num4 K4 бодикам · Num5 K5 обычная"));
		L.Add(TEXT("Num7 / Num9 — следить за другим (Человек, питомцы, семья, торт, игроки) · Num8 — хватит"));
		L.Add(FString::Printf(TEXT("Num/ + цифра — запомнить · Num* + цифра — вызвать · Num. + A + B — проезд за %.0f с (PgUp/PgDn)"), DollySeconds));
		L.Add(SlotLine);
		L.Add(FString::Printf(TEXT("Num0 замедление: %.2f%s"), Slomo, bHost ? TEXT("") : TEXT(" (только у хоста)")));
		if (bHost)
		{
			L.Add(TEXT("РЕЖИССЁР (в точку прицела): U топнуть · I ладонью · O газетой · N заглянуть под мебель · M взять еду"));
			L.Add(FString::Printf(TEXT(", иди сюда · . уходи · / замри [%s] · [ кошка прыгает · ] малыш хватает · End бабушка метлой"), Dad && Dad->bDevPuppet ? TEXT("замер") : TEXT("сам")));
			L.Add(FString::Printf(TEXT("Home все спят/проснулись · \\ без атак [%s] · Ins лужа масла · Del Человек поскользнулся"), AntFilm::NoHarm() ? TEXT("вкл") : TEXT("выкл")));
		}
		else
		{
			L.Add(TEXT("Режиссёрские клавиши — у хоста"));
		}
		if (Pending == 1) { L.Add(TEXT(">> Нажми цифру слота Num1–Num6, чтобы запомнить")); }
		else if (Pending == 2) { L.Add(TEXT(">> Нажми цифру слота Num1–Num6, чтобы вызвать")); }
		else if (Pending == 3) { L.Add(TEXT(">> Проезд: цифра слота, ОТКУДА")); }
		else if (Pending == 4) { L.Add(FString::Printf(TEXT(">> Проезд из %d: цифра слота, КУДА"), DollyFrom)); }
		const float Line = 21.f * S, X = 18.f * S;
		float Y = 18.f * S;
		Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 8.f * S, Y - 6.f * S, 940.f * S, Line * (L.Num() + 1) + 12.f * S);
		for (int32 i = 0; i < L.Num(); ++i)
		{
			Txt(L[i], X, Y, i == 0 ? FilmGold : (L[i].StartsWith(TEXT(">>")) ? FilmGreen : FilmGrey), Small, (i == 0 ? 1.15f : 1.f) * S);
			Y += Line;
		}
		if (RealNow - NoteTime < 6.f)
		{
			Txt(Note, X, Y, bNoteWarn ? FilmRed : FilmGreen, Small, 1.05f * S);
		}
	}
}

// =============================================================================================
// The test script (-AntFilmDo)
// =============================================================================================

void UAntFilm::ScriptTick()
{
	for (int32 i = 0; i < Script.Num();)
	{
		if (Script[i].Key <= ScriptClock)
		{
			const FString Cmd = Script[i].Value;
			Script.RemoveAt(i);
			UE_LOG(LogTemp, Display, TEXT("ANTFILM %.1f: %s"), ScriptClock, *Cmd);
			RunScript(Cmd);
		}
		else
		{
			++i;
		}
	}
}

void UAntFilm::RunScript(const FString& Cmd)
{
	TArray<FString> A;
	Cmd.ParseIntoArrayWS(A);
	if (A.Num() == 0 || !PC)
	{
		return;
	}
	const FString Op = A[0].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	if (Op == TEXT("key") && A.Num() >= 2)
	{
		// Through the real input path: the key goes into the player input and the input stack hands it to whoever binds it.
		const FKey K = FKey(FName(*A[1]));
		if (!K.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTFILM no such key: %s"), *A[1]);
			return;
		}
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, IE_Pressed, 1.f));
		TWeakObjectPtr<AAntPlayerController> WeakPC = PC;
		FTimerHandle T;
		GetWorld()->GetTimerManager().SetTimer(T, FTimerDelegate::CreateWeakLambda(PC, [WeakPC, K]()
		{
			if (WeakPC.IsValid()) { WeakPC->InputKey(FInputKeyEventArgs::CreateSimulated(K, IE_Released, 0.f)); }
		}), 0.05f, false);
	}
	else if (Op == TEXT("fly"))
	{
		// fly fwd right up seconds: as if those keys were held (camera frame, -1..1)
		DevFly = FVector(Num(1), Num(2), Num(3));
		DevFlyUntil = (float)RealNow + Num(4, 1.f);
	}
	else if (Op == TEXT("look"))
	{
		WantYaw = Num(1);
		WantPitch = Num(2);
		if (Mode == ECam::Free) { CamRot = FRotator(WantPitch, WantYaw, 0.f); }
	}
	else if (Op == TEXT("at"))
	{
		// at x y z yaw pitch [fov]: the free camera jumps there
		CamLoc = FVector(Num(1), Num(2), Num(3));
		WantYaw = Num(4);
		WantPitch = Num(5);
		CamRot = FRotator(WantPitch, WantYaw, 0.f);
		CamFov = Num(6, CamFov);
		FreeVel = FVector::ZeroVector;
		ApplyCamera(CamLoc, CamRot, CamFov, 0.f, 0.f);
	}
	else if (Op == TEXT("shot"))
	{
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("film") / (A.IsValidIndex(1) ? A[1] : FString(TEXT("shot"))) + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(File, true, false);
	}
	else if (Op == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFILM info on=%d hud=%d hints=%d frame=%d mode=%d cam=%s rot=%s fov=%.0f viewtarget=%s slomo=%.2f noharm=%d speed=%.0f slots=%d%d%d%d%d%d block=%d pawn=%s"),
			bOn ? 1 : 0, bHideHud ? 0 : 1, bHints ? 1 : 0, bFrame ? 1 : 0, (int32)Mode, *ViewLoc().ToCompactString(), *ViewRot().ToCompactString(), ViewFov(),
			*GetNameSafe(PC->GetViewTarget()), UGameplayStatics::GetGlobalTimeDilation(GetWorld()), AntFilm::NoHarm() ? 1 : 0, FreeSpeed,
			Slots[1].bSet, Slots[2].bSet, Slots[3].bSet, Slots[4].bSet, Slots[5].bSet, Slots[6].bSet, Input && Input->bBlockInput ? 1 : 0,
			PC->GetPawn() ? *PC->GetPawn()->GetActorLocation().ToCompactString() : TEXT("-"));
	}
	else if (Op == TEXT("raid") && A.Num() >= 2)
	{
		// raid <cmd> [a] [b]: what a nest button sends (AAntPlayerController::ServerRaid), from this machine - a guest's test of the
		// colony vote (KAN-31: "raid 17 0" proposes the first chamber, "raid 16 0" the first tunnel; 202 1 / 202 0 votes yes / no).
		PC->ServerRaid((uint8)FMath::Clamp((int32)Num(1), 0, 255), (int32)Num(2), (int32)Num(3));
	}
	else if (Op == TEXT("menu") && A.Num() >= 2)
	{
		// menu close | menu <screen>: the Slate front end (the co-op muster opens by itself; a test closes it to see the nest HUD)
		if (A[1] == TEXT("close")) { PC->CloseMenu(); }
		else { PC->OpenUiScreen((uint8)Num(1)); }
	}
	else if (Op == TEXT("wheel"))
	{
		for (int32 i = 0; i < FMath::Abs((int32)Num(1, 1.f)); ++i) { Num(1, 1.f) > 0.f ? OnWheelUp() : OnWheelDown(); }
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTFILM unknown script command: %s"), *Cmd);
	}
}
