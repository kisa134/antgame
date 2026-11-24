#include "AntLife.h"
#include "AntKitchenGameMode.h"
#include "AntCharacter.h"
#include "AntAudio.h"
#include "AntRaid.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "KitchenEvents.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "LevelV3.h"
#include "NpcRoutine.h"
#include "Map4Look.h"
#include "AntRun.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "MediaSoundComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "AntLifeChannels.inl"

// =============================================================================================
// The shared helpers
// =============================================================================================

namespace AntLife
{
TConstArrayView<FChannel> Channels() { return MakeArrayView(AntLifeData::Channels); }

bool Enabled()
{
	static int32 State = -1;
	if (State < 0)
	{
		State = (Map4Look::Enabled() && !FParse::Param(FCommandLine::Get(), TEXT("AntNoLife"))) ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE %s"), State ? TEXT("on (the living flat)") : TEXT("off"));
	}
	return State == 1;
}

float Now(const UWorld* W)
{
	if (!W) { return 0.f; }
	const AGameStateBase* GS = W->GetGameState();
	return GS ? (float)GS->GetServerWorldTimeSeconds() : W->GetTimeSeconds();
}

int32 Tod()
{
	static int32 Forced = -2;
	if (Forced == -2)
	{
		int32 P = -1;
		Forced = FParse::Value(FCommandLine::Get(), TEXT("AntTime="), P) ? FMath::Clamp(P, 0, 2) : -1;
	}
	if (Forced >= 0) { return Forced; }
	if (LevelV3::Enabled() && LevelV3::Current()) { return FMath::Clamp(LevelV3::TimeOfDay(), 0, 2); }
	return 0;
}

int32 Weather(const UWorld* W)
{
	if (const AAntLifeTV* Tv = AAntLifeTV::Get(W))
	{
		if (Tv->WeatherSet >= 0) { return Tv->WeatherSet; }
	}
	static int32 Cmd = -2;
	if (Cmd == -2)
	{
		int32 P = -1;
		Cmd = FParse::Value(FCommandLine::Get(), TEXT("AntWeather="), P) ? FMath::Clamp(P, 0, 2) : -1;
	}
	if (Cmd >= 0) { return Cmd; }
	if (LevelV3::Enabled() && LevelV3::IsHazardOn(TEXT("rain"))) { return 1; }
	const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
	if (!S) { return 0; }
	const uint32 H = (uint32)(S->Seed * 2654435761u) >> 13;
	const int32 K = H % 10;
	return (K == 2 || K == 6) ? 1 : (K == 8 && Tod() != 0) ? 2 : 0;   // a rainy seed in 5, a snowy evening / night in 10
}

void SetWeather(UWorld* W, int32 Value)
{
	if (AAntLifeTV* Tv = AAntLifeTV::Get(W))
	{
		Tv->WeatherSet = (int8)FMath::Clamp(Value, -1, 2);
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE weather set %d (now %d)"), Tv->WeatherSet, Weather(W));
	}
}

float ClockHours(const UWorld* W)
{
	static const float Base[3] = { 14.f, 19.5f, 23.f };
	const AAntRaid* R = W ? AAntRaid::Get(W) : nullptr;
	const float T = (R && R->InRaid()) ? R->RaidTime() : (W ? W->GetTimeSeconds() : 0.f);
	return FMath::Fmod(Base[Tod()] + T / 60.f, 24.f);
}

void Spawn(UWorld* W)
{
	if (!W || !Enabled())
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (W->GetNetMode() != NM_Client && !AAntLifeTV::Get(W))
	{
		W->SpawnActor<AAntLifeTV>(AAntLifeTV::StaticClass(), FTransform::Identity, P);
	}
	if (W->GetNetMode() != NM_DedicatedServer && !AAntLifeWorld::Get(W))
	{
		W->SpawnActor<AAntLifeWorld>(AAntLifeWorld::StaticClass(), FTransform::Identity, P);
	}
}

bool HumanChecked(AActor* Source)
{
	if (AAntLifeTV* Tv = Cast<AAntLifeTV>(Source))
	{
		Tv->HumanChecked();
		return true;
	}
	return false;
}

void StartVacuum(UWorld* W, float LifeSeconds)
{
	if (!W || W->GetNetMode() == NM_Client) { return; }
	for (TActorIterator<ARobotVacuum> It(W); It; ++It) { It->Life = FMath::Max(It->Life, LifeSeconds); return; }
	// out of the living room door, along the kid's crumb trail towards the high chair (it sweeps the trail first)
	FTransform TM(FRotator(0.f, 137.f, 0.f), FVector(6650.f, -3650.f, ARobotVacuum::Height * 0.5f + 2.f));
	if (ARobotVacuum* V = W->SpawnActorDeferred<ARobotVacuum>(ARobotVacuum::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		V->Life = LifeSeconds;
		V->FinishSpawning(TM);
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE vacuum out for %.0f s at %s"), LifeSeconds, *TM.GetLocation().ToCompactString());
		if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->Broadcast(TEXT("Робот-пылесос выехал на уборку: он засасывает крошки — и муравьёв!"), FLinearColor(0.7f, 0.9f, 1.f));
		}
	}
}

void StopVacuum(UWorld* W)
{
	if (!W || W->GetNetMode() == NM_Client) { return; }
	for (TActorIterator<ARobotVacuum> It(W); It; ++It) { It->Life = 0.01f; UE_LOG(LogTemp, Display, TEXT("ANTLIFE vacuum stopped")); }
}

void SetVacuumEvery(UWorld* W, float Seconds)
{
	if (AAntLifeTV* Tv = AAntLifeTV::Get(W))
	{
		Tv->VacuumEvery = FMath::Max(0.f, Seconds);
		Tv->NextVacuum = Seconds > 0.f ? Now(W) + Seconds : 1.e9f;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE vacuum rounds every %.0f s"), Seconds);
	}
}

void StartWash(UWorld* W)
{
	if (AAntLifeTV* Tv = AAntLifeTV::Get(W))
	{
		Tv->WashStart = Now(W);
		Tv->bWashForced = true;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE dishwasher cycle starts"));
	}
}

void StopWash(UWorld* W)
{
	if (AAntLifeTV* Tv = AAntLifeTV::Get(W))
	{
		Tv->WashStart = -1000.f;
		Tv->bWashForced = false;
		Tv->NextWash = Now(W) + 200.f;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE dishwasher cycle stopped"));
	}
}

bool PigeonAt(const UWorld* W, float T, FVector& OutPos, float& OutYaw, bool& bPeck, float& OutFly)
{
	// Every 150 s (shifted by the seed) a pigeon lands on the kitchen's outer sill for 22 s: flies in (2 s), walks and pecks, flies off (2 s).
	const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
	const float Period = 150.f;
	const float Off = S ? (float)((S->Seed * 37) % 150) : 40.f;
	const float L = FMath::Fmod(T + Off + 1000.f * Period, Period);
	bPeck = false;
	OutFly = 0.f;
	if (L > 22.f) { return false; }
	const float SillZ = 2232.f, SillY = -5330.f;
	const float WalkX = -700.f + 900.f * FMath::Sin(L * 0.31f) + 300.f * FMath::Sin(L * 0.77f);
	FVector P(WalkX, SillY, SillZ);
	OutYaw = FMath::Cos(L * 0.31f) >= 0.f ? 0.f : 180.f;
	if (L < 2.f)
	{
		const float K = 1.f - L / 2.f;
		OutFly = K;
		P += FVector(-1200.f * K, -5000.f * K, 2500.f * K * K);
		OutYaw = 70.f;
	}
	else if (L > 20.f)
	{
		const float K = (L - 20.f) / 2.f;
		OutFly = FMath::Max(0.01f, K);
		P += FVector(2000.f * K, -6000.f * K, 3000.f * K * K);
		OutYaw = -60.f;
	}
	else
	{
		bPeck = FMath::Frac(L * 0.5f) < 0.18f;
	}
	OutPos = P;
	return true;
}
}   // namespace AntLife

// =============================================================================================
// The TV
// =============================================================================================

namespace
{
	FString MoviePath(const TCHAR* File)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/Life") / (FString(File) + TEXT(".mp4")));
	}

	UStaticMeshComponent* FindMeshNear(UWorld* W, const TCHAR* Name, const FVector& Near, float Range)
	{
		UStaticMeshComponent* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->IsA<APawn>()) { continue; }
			TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
			for (UStaticMeshComponent* C : Cs)
			{
				if (!C || !C->GetStaticMesh() || !C->IsVisible()) { continue; }
				if (C->GetStaticMesh()->GetName() != Name) { continue; }
				const float D = FVector::Dist(C->GetComponentLocation(), Near);
				if (D < BestD) { BestD = D; Best = C; }
			}
		}
		return Best;
	}

	AHumanCharacter* Dad(const UWorld* W)
	{
		for (TActorIterator<AHumanCharacter> It(W); It; ++It)
		{
			if (It->Fig == HumanFig::Dad && !It->IsA<AHouseFolk>()) { return *It; }
		}
		return nullptr;
	}
	template <class T> T* First(const UWorld* W) { for (TActorIterator<T> It(W); It; ++It) { return *It; } return nullptr; }
	ACritter* Pet(const UWorld* W, uint8 Kind)
	{
		for (TActorIterator<ACritter> It(W); It; ++It) { if (It->Kind == Kind) { return *It; } }
		return nullptr;
	}
	bool AtStep(const FNpcTask& T, const TCHAR* Key)
	{
		static TMap<FString, int32> Cache;
		int32* I = Cache.Find(Key);
		if (!I) { I = &Cache.Add(Key, NpcRoutine::FindStep(Key)); }
		return *I >= 0 && T.Step == *I && T.Phase == 2;
	}
	const TCHAR* ButtonName(int32 B)
	{
		static const TCHAR* N[] = { TEXT("power"), TEXT("channel"), TEXT("vol+"), TEXT("vol-") };
		return N[FMath::Clamp(B, 0, 3)];
	}
}

AAntLifeTV::AAntLifeTV()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

AAntLifeTV* AAntLifeTV::Get(const UWorld* W)
{
	if (!W) { return nullptr; }
	for (TActorIterator<AAntLifeTV> It(W); It; ++It) { return *It; }
	return nullptr;
}

void AAntLifeTV::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntLifeTV, bOn);
	DOREPLIFETIME(AAntLifeTV, Channel);
	DOREPLIFETIME(AAntLifeTV, Volume);
	DOREPLIFETIME(AAntLifeTV, ChannelStart);
	DOREPLIFETIME(AAntLifeTV, Hold);
	DOREPLIFETIME(AAntLifeTV, Presses);
	DOREPLIFETIME(AAntLifeTV, LastButton);
	DOREPLIFETIME(AAntLifeTV, WashStart);
	DOREPLIFETIME(AAntLifeTV, WeatherSet);
}

void AAntLifeTV::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
		const float Now = AntLife::Now(GetWorld());
		NextVacuum = Now + 90.f + (S ? (S->Seed % 7) * 10.f : 0.f);
		NextWash = Now + 50.f + (S ? (S->Seed % 5) * 12.f : 0.f);
		ZapAt = Now + 70.f;
		if (FParse::Param(FCommandLine::Get(), TEXT("AntNoVacuum"))) { VacuumEvery = 0.f; NextVacuum = 1.e9f; }
	}
}

void AAntLifeTV::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Player) { Player->Close(); }
	Super::EndPlay(Reason);
}

float AAntLifeTV::PlayPos() const
{
	const TConstArrayView<AntLife::FChannel> Ch = AntLife::Channels();
	if (!Ch.IsValidIndex(Channel)) { return 0.f; }
	const float Len = FMath::Max(1.f, Ch[Channel].Secs);
	return FMath::Fmod(FMath::Max(0.f, AntLife::Now(GetWorld()) - ChannelStart), Len);
}

FVector AAntLifeTV::ButtonAt(int32 Button) const
{
	const UStaticMeshComponent* R = RemoteComp.Get();
	if (!R) { return FVector(13000.f, -3900.f, 1018.f); }
	// SM_L_Remote (cm): the red power key at x -12.75, the teal one at x 8.25, the steel keys between (x -9..11.6), top z 1.9
	static const float X[4] = { -12.75f, 8.25f, 1.5f, -4.5f };
	return R->GetComponentTransform().TransformPosition(FVector(X[FMath::Clamp(Button, 0, 3)], 0.f, 2.1f));
}

void AAntLifeTV::FindTV()
{
	UWorld* W = GetWorld();
	if (!TvComp.IsValid())
	{
		if (UStaticMeshComponent* C = FindMeshNear(W, TEXT("SM_L_TV"), FVector(13150.f, -8700.f, 1100.f), 4000.f))
		{
			TvComp = C;
			const FTransform T = C->GetComponentTransform();
			// The Screen slot of SM_L_TV (cm): x -100.1..100.1, z 12..113.3, on the face at y -3; which world side is the room's decides the normal.
			FVector N = T.GetUnitAxis(EAxis::Y);
			const FVector ToRoom = (FVector(13700.f, -3750.f, 1100.f) - T.GetLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(N, ToRoom) < 0.f) { N = -N; }
			ScreenN = N;
			ScreenX = T.GetUnitAxis(EAxis::X);
			ScreenW = 200.2f * T.GetScale3D().X;
			ScreenH = 101.3f * T.GetScale3D().Z;
			ScreenC = T.TransformPosition(FVector(0.f, 0.f, 62.65f)) + N * (10.5f * T.GetScale3D().Y + 6.f);
			SetActorLocation(ScreenC);
			UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv found: screen %s normal %s %.0fx%.0f"), *ScreenC.ToCompactString(), *ScreenN.ToCompactString(), ScreenW, ScreenH);
		}
	}
	if (!RemoteComp.IsValid())
	{
		RemoteComp = FindMeshNear(W, TEXT("SM_L_Remote"), FVector(13000.f, -3900.f, 980.f), 6000.f);
		if (RemoteComp.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("ANTLIFE remote at %s: power %s channel %s volume %s"), *RemoteComp->GetComponentLocation().ToCompactString(),
				*ButtonAt(0).ToCompactString(), *ButtonAt(1).ToCompactString(), *ButtonAt(2).ToCompactString());
		}
	}
}

void AAntLifeTV::SetState(bool bNewOn, int32 NewChannel, int32 NewVolume, const TCHAR* Why)
{
	if (!HasAuthority()) { return; }
	const int32 NumCh = AntLife::Channels().Num();
	NewChannel = NumCh > 0 ? ((NewChannel % NumCh) + NumCh) % NumCh : 0;
	NewVolume = FMath::Clamp(NewVolume, 0, 10);
	if (NewChannel != Channel || (bNewOn && !bOn))
	{
		// a new channel joins its loop at a place of its own (not always the first frame)
		ChannelStart = AntLife::Now(GetWorld()) - FMath::FRandRange(0.f, 6.f);
	}
	const bool bChanged = bNewOn != bOn || NewChannel != Channel || NewVolume != Volume;
	bOn = bNewOn;
	Channel = (uint8)NewChannel;
	Volume = (uint8)NewVolume;
	if (bChanged)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv %s ch %d «%s» vol %d (%s) watchers %d"), bOn ? TEXT("ON") : TEXT("off"), Channel,
			AntLife::Channels().IsValidIndex(Channel) ? AntLife::Channels()[Channel].Ru : TEXT("?"), Volume, Why, NumWatchers);
	}
}

void AAntLifeTV::Press(int32 Button, AAntCharacter* By)
{
	if (!HasAuthority()) { return; }
	Button = FMath::Clamp(Button, 0, 3);
	++Presses;
	LastButton = (uint8)Button;
	const TCHAR* Who = By ? *By->GetDisplayName() : TEXT("admin");
	UE_LOG(LogTemp, Display, TEXT("ANTLIFE remote press %s by %s"), ButtonName(Button), Who);
	switch (Button)
	{
	case 0:
		if (bOn)
		{
			SetState(false, Channel, Volume, TEXT("remote: power off"));
			Hold = NumWatchers > 0 ? 2 : 0;
			ReholdAt = NumWatchers > 0 ? AntLife::Now(GetWorld()) + 6.f : -1.f;
		}
		else
		{
			Hold = 1;
			SetState(true, Channel, FMath::Max<int32>(Volume, 4), TEXT("remote: power on"));
		}
		break;
	case 1: SetState(true, Channel + 1, Volume, TEXT("remote: next channel")); if (NumWatchers == 0) { Hold = 1; } break;
	case 2: SetState(true, Channel, Volume + 2, TEXT("remote: louder")); if (NumWatchers == 0) { Hold = 1; } break;
	default: SetState(bOn, Channel, Volume - 2, TEXT("remote: quieter")); break;
	}
	if (By)
	{
		FString Msg;
		if (Button == 0) { Msg = bOn ? TEXT("Телевизор включён!") : TEXT("Телевизор выключен"); }
		else if (Button == 1) { Msg = FString::Printf(TEXT("Канал: %s"), AntLife::Channels().IsValidIndex(Channel) ? AntLife::Channels()[Channel].Ru : TEXT("?")); }
		else { Msg = FString::Printf(TEXT("Громкость: %d"), (int32)Volume); }
		By->Toast(Msg);
	}
	React(Button, By);
}

void AAntLifeTV::React(int32 Button, AAntCharacter* By)
{
	UWorld* W = GetWorld();
	const float Now = AntLife::Now(W);
	if (Now < ReactCool) { return; }
	ReactCool = Now + 2.5f;
	const uint8 Kind = AntLife::Channels().IsValidIndex(Channel) ? AntLife::Channels()[Channel].Kind : 0;
	static int32 Salt = 0;
	++Salt;
	AHumanCharacter* D = Dad(W);
	const bool bDadHere = D && (AtStep(D->Task, TEXT("h_tv")) || AtStep(D->Task, TEXT("h_nap")) || FVector::Dist2D(D->GetActorLocation(), ScreenC) < 6500.f);
	if (bDadHere)
	{
		static const TCHAR* Switch[] = { TEXT("Эй! Я же смотрел!"), TEXT("Кто переключил?!"), TEXT("Верните футбол!") };
		static const TCHAR* Loud[] = { TEXT("Сделайте потише!"), TEXT("Оглохнуть можно!"), TEXT("Кто включил на всю громкость?!") };
		static const TCHAR* Off[] = { TEXT("Кто выключил телевизор?!"), TEXT("Опять пульт глючит..."), TEXT("Ну вот, на самом интересном!") };
		const TCHAR* Line = Button == 0 ? (bOn ? TEXT("О, телевизор сам включился...") : Off[Salt % 3]) : Button == 2 ? Loud[Salt % 3] : Button == 1 ? Switch[Salt % 3] : nullptr;
		if (Line)
		{
			NpcRoutine::Say(W, NpcRoutine::Dad, Line);
			AntAudio::Everyone(this, Button == 2 ? EAntSfx::HumanRoar : EAntSfx::HumanGrumble, D->GetActorLocation(), 1.f);
		}
	}
	for (TActorIterator<AKidCharacter> It(W); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), ScreenC) < 9000.f && bOn)
		{
			const bool bFun = Kind == AntLife::KCartoon || Kind == AntLife::KKids;
			NpcRoutine::Say(W, NpcRoutine::Kid, bFun ? TEXT("Ура, мультики!") : Button == 2 ? TEXT("Ха-ха, громко!") : TEXT("Хи-хи!"));
			AntAudio::Everyone(this, EAntSfx::KidGiggle, It->GetActorLocation(), 1.f);
			break;
		}
	}
	if (bOn && (Kind == AntLife::KNature || Kind == AntLife::KBirds || Kind == AntLife::KSport))
	{
		if (ACritter* Dog = Pet(W, 0))
		{
			if (FVector::Dist2D(Dog->GetActorLocation(), ScreenC) < 12000.f)
			{
				Dog->DevAct(TEXT("look"), ScreenC);
				Dog->DevAct(TEXT("bark"), ScreenC);
				NpcRoutine::Say(W, NpcRoutine::Dog, Kind == AntLife::KBirds ? TEXT("Гав! Гав-гав! (на птиц в телевизоре)") : TEXT("Гав! Гав!"));
				AntAudio::Everyone(this, EAntSfx::DogBark, Dog->GetActorLocation(), 1.1f);
			}
		}
	}
	if (bOn && Kind == AntLife::KBirds)
	{
		if (ACritter* Cat = Pet(W, 1))
		{
			if (FVector::Dist2D(Cat->GetActorLocation(), ScreenC) < 14000.f)
			{
				CatMove = 0;
				CatStep = Now + 0.3f;
				NpcRoutine::Say(W, NpcRoutine::Cat, TEXT("Мрр?.. (кошка увидела птиц на экране)"));
			}
		}
	}
	if (Volume >= 8)
	{
		for (TActorIterator<AGrandmaCharacter> It(W); It; ++It)
		{
			NpcRoutine::Say(W, NpcRoutine::Gran, TEXT("Ох, батюшки, разбудили!"));
			break;
		}
	}
	if (By) { AntAudio::Excite(this, 0.3f, ScreenC); }
}

void AAntLifeTV::HumanChecked()
{
	if (!HasAuthority()) { return; }
	if (NumWatchers > 0)
	{
		SetState(true, Channel, 4, TEXT("the Human turned it down"));
		NpcRoutine::Say(GetWorld(), NpcRoutine::Dad, TEXT("Так-то лучше."));
	}
	else
	{
		SetState(false, Channel, 4, TEXT("the Human switched it off"));
		NpcRoutine::Say(GetWorld(), NpcRoutine::Dad, TEXT("Кто опять включил телевизор?.."));
		Hold = 0;
	}
}

void AAntLifeTV::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!AntLife::Enabled()) { return; }
	FindRetry -= Dt;
	if ((!TvComp.IsValid() || !RemoteComp.IsValid()) && FindRetry <= 0.f)
	{
		FindRetry = 2.f;
		FindTV();
	}
	if (HasAuthority())
	{
		ServerTick(Dt);
		ServerLifeDuties(Dt);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		ClientTick(Dt);
	}
}

void AAntLifeTV::ServerTick(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = AntLife::Now(W);
	// ---- who watches
	int32 N = 0;
	bool bDadWatch = false, bKidWatch = false;
	if (AHumanCharacter* D = Dad(W)) { bDadWatch = AtStep(D->Task, TEXT("h_tv")) || AtStep(D->Task, TEXT("h_nap")); }
	for (TActorIterator<AKidCharacter> It(W); It; ++It) { bKidWatch |= AtStep(It->Task, TEXT("k_tv")); }
	N = (bDadWatch ? 1 : 0) + (bKidWatch ? 1 : 0);
	if (N != LoggedWatch)
	{
		LoggedWatch = N;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv watchers %d (dad %d kid %d)"), N, bDadWatch ? 1 : 0, bKidWatch ? 1 : 0);
	}
	const int32 Was = NumWatchers;
	NumWatchers = N;
	if (Hold == 2 && ReholdAt > 0.f && Now > ReholdAt && N > 0)
	{
		Hold = 0;
		ReholdAt = -1.f;
		NpcRoutine::Say(W, bDadWatch ? NpcRoutine::Dad : NpcRoutine::Kid, bDadWatch ? TEXT("Так, где пульт... Включаю обратно.") : TEXT("Включите мультики!"));
	}
	if (Hold == 0)
	{
		if (N > 0 && !bOn)
		{
			// the Dad: sport or news (by seed), the kid: cartoons
			const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
			const int32 DadCh = (S && S->Seed % 2) ? 2 : 3;
			SetState(true, bKidWatch && !bDadWatch ? ((S && S->Seed % 3 == 0) ? 6 : 0) : DadCh, 4, bDadWatch ? TEXT("the Dad sat down to watch") : TEXT("the kid wants cartoons"));
			ZapAt = Now + FMath::FRandRange(50.f, 90.f);
		}
		if (N == 0 && Was > 0) { NobodySince = Now; }
		if (N == 0 && bOn && Now - NobodySince > 25.f)
		{
			SetState(false, Channel, Volume, TEXT("nobody watches"));
		}
		if (bDadWatch && bOn && Now > ZapAt)
		{
			// the Dad flips through the channels now and then (adverts break in on their own)
			static const int32 Zap[] = { 3, 2, 1, 5, 4, 3, 2 };
			static int32 Zi = 0;
			SetState(true, Zap[Zi++ % UE_ARRAY_COUNT(Zap)], Volume, TEXT("the Dad flips channels"));
			ZapAt = Now + FMath::FRandRange(45.f, 80.f);
		}
	}
	// ---- a loud TV is a noise the Human goes to (a lure the ants can use)
	NoiseClock -= Dt;
	if (bOn && Volume >= 7 && NoiseClock <= 0.f)
	{
		NoiseClock = 3.f;
		if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->MakeNoise(ScreenC, 0.35f + 0.06f * Volume, AntNoise::TV, this);
		}
	}
	// ---- the remote: an ant jumping onto a key (or standing on it half a second) presses it
	if (RemoteComp.IsValid())
	{
		for (int32 b = 0; b < 3; ++b) { ButtonCool[b] -= Dt; }
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A->bDead || A->Creature != 0) { continue; }
			const FVector P = A->GetActorLocation();
			if (FVector::DistSquared2D(P, RemoteComp->GetComponentLocation()) > 500.f * 500.f) { OnButton.Remove(A); continue; }
			int32 Hit = -1;
			for (int32 b = 0; b < 3; ++b)
			{
				const FVector B = ButtonAt(b);
				if (FVector::Dist2D(P, B) < 30.f && P.Z > B.Z - 8.f && P.Z < B.Z + 45.f) { Hit = b; break; }
			}
			float* On = OnButton.Find(A);
			if (Hit < 0) { if (On) { OnButton.Remove(A); } continue; }
			const float Key = Hit * 100.f;
			if (!On)
			{
				OnButton.Add(A, Key + 0.001f);
				if (A->GetVelocity().Z < -40.f && ButtonCool[Hit] <= 0.f) { ButtonCool[Hit] = 1.2f; Press(Hit, A); OnButton.Add(A, Key + 99.f); }
				continue;
			}
			if (FMath::FloorToInt(*On / 100.f) != Hit) { *On = Key + 0.001f; continue; }
			const float Held = *On - Key;
			if (Held < 50.f)
			{
				*On += Dt;
				if (*On - Key >= 0.5f && ButtonCool[Hit] <= 0.f) { ButtonCool[Hit] = 1.2f; Press(Hit, A); *On = Key + 99.f; }
			}
		}
	}
	// ---- the cat at the bird channel: up onto the stand, paws at the screen
	if (CatStep > 0.f && Now > CatStep)
	{
		ACritter* Cat = Pet(W, 1);
		if (!Cat || !bOn) { CatStep = -1.f; }
		else
		{
			const FVector Stand = ScreenC + ScreenN * 450.f + FVector(0.f, 0.f, -ScreenC.Z + 1130.f) + ScreenX * 900.f;
			switch (CatMove++)
			{
			case 0: Cat->DevAct(TEXT("look"), ScreenC); Cat->DevAct(TEXT("stalk"), ScreenC); CatStep = Now + 1.5f; break;
			case 1: Cat->DevAct(TEXT("jump"), Stand); CatStep = Now + 2.f; UE_LOG(LogTemp, Display, TEXT("ANTLIFE cat jumps to the TV stand %s"), *Stand.ToCompactString()); break;
			case 2: case 3: case 4: Cat->DevAct(TEXT("swipe0"), ScreenC - FVector(0.f, 0.f, 300.f)); CatStep = Now + 1.1f; UE_LOG(LogTemp, Display, TEXT("ANTLIFE cat paws at the screen")); break;
			default: Cat->DevAct(TEXT("nolook"), ScreenC); CatStep = -1.f; break;
			}
		}
	}
}

void AAntLifeTV::ServerLifeDuties(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = AntLife::Now(W);
	const AAntRaid* Raid = AAntRaid::Get(W);
	const bool bLive = !Raid || Raid->InRaid();
	// ---- the robot vacuum's rounds
	if (bLive && VacuumEvery > 0.f && Now > NextVacuum)
	{
		NextVacuum = Now + VacuumEvery;
		bool bOut = false;
		for (TActorIterator<ARobotVacuum> It(W); It; ++It) { bOut = true; }
		if (!bOut) { AntLife::StartVacuum(W, 70.f); }
	}
	// ---- the dishwasher: a 75 s cycle every ~3.5 min; it shakes what lies loose on the worktop above it
	if (bLive && !bWashForced && WashStart < 0.f && Now > NextWash)
	{
		WashStart = Now;
		NextWash = Now + 210.f;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE dishwasher cycle starts (rounds)"));
	}
	const float WashT = Now - WashStart;
	if (WashStart > 0.f && WashT > 75.f && !bWashForced)
	{
		WashStart = -1000.f;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE dishwasher cycle done"));
	}
	if (WashStart > 0.f && (WashT < 75.f || bWashForced))
	{
		WashImpulse -= Dt;
		if (WashImpulse <= 0.f)
		{
			WashImpulse = 0.3f;
			const FBox Top(FVector(-150.f, 3000.f, 1600.f), FVector(1450.f, 4800.f, 2200.f));
			int32 Moved = 0;
			for (TActorIterator<AKitchenLoot> It(W); It; ++It)
			{
				UPrimitiveComponent* P = Cast<UPrimitiveComponent>(It->GetRootComponent());
				if (!P || !Top.IsInside(It->GetActorLocation()) || It->bHeld || It->IsCarried()) { continue; }
				if (P->IsSimulatingPhysics())
				{
					const float M = P->GetMass();
					P->AddImpulse(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.6f) * M * 22.f);
				}
				else
				{
					It->AddActorWorldOffset(FVector(FMath::FRandRange(-2.f, 2.f), FMath::FRandRange(-2.f, 2.f), 0.f), true);
				}
				++Moved;
			}
			static float LogAt = 0.f;
			if (Moved > 0 && Now > LogAt) { LogAt = Now + 10.f; UE_LOG(LogTemp, Display, TEXT("ANTLIFE dishwasher shakes %d loose things on the worktop"), Moved); }
		}
	}
	// ---- the pigeon pecks at ants on the outer sill
	FVector Pp; float Yaw = 0.f, Fly = 0.f; bool bPeck = false;
	PeckClock -= Dt;
	if (AntLife::PigeonAt(W, Now, Pp, Yaw, bPeck, Fly) && bPeck && Fly <= 0.f && PeckClock <= 0.f)
	{
		PeckClock = 0.4f;
		const FVector Beak = Pp + FRotator(0.f, Yaw, 0.f).Vector() * 230.f;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			if (!It->bDead && It->Creature == 0 && FVector::Dist(It->GetActorLocation(), Beak) < 140.f)
			{
				It->HurtBy(45.f, TEXT("заклёван голубем"));
				UE_LOG(LogTemp, Display, TEXT("ANTLIFE pigeon pecked %s"), *It->GetDisplayName());
			}
		}
	}
}

void AAntLifeTV::OpenChannel(int32 Ch)
{
	const TConstArrayView<AntLife::FChannel> All = AntLife::Channels();
	if (!Player || !All.IsValidIndex(Ch)) { return; }
	while (Sources.Num() < All.Num()) { Sources.Add(nullptr); }
	if (!Sources[Ch])
	{
		UFileMediaSource* S = NewObject<UFileMediaSource>(this);
		S->SetFilePath(MoviePath(All[Ch].File));
		Sources[Ch] = S;
	}
	const bool bOk = Player->OpenSource(Sources[Ch]);
	OpenCh = Ch;
	bPendingSeek = true;
	bPlaying = true;
	UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv player opens %s (%s) %s, exists=%d"), All[Ch].File, *MoviePath(All[Ch].File), bOk ? TEXT("ok") : TEXT("FAILED"),
		IFileManager::Get().FileExists(*MoviePath(All[Ch].File)) ? 1 : 0);
}

void AAntLifeTV::ClientTick(float Dt)
{
	UWorld* W = GetWorld();
	UStaticMeshComponent* Tv = TvComp.Get();
	if (!Tv) { return; }
	if (!Player)
	{
		Player = NewObject<UMediaPlayer>(this, TEXT("LifeTvPlayer"));
		Player->SetLooping(true);
		Player->PlayOnOpen = true;
		MediaTex = NewObject<UMediaTexture>(this, TEXT("LifeTvTexture"));
		MediaTex->AutoClear = true;
		MediaTex->ClearColor = FLinearColor::Black;
		MediaTex->SetMediaPlayer(Player);
		MediaTex->UpdateResource();
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Life/Materials/M_Life_Screen.M_Life_Screen")))
		{
			ScreenMid = UMaterialInstanceDynamic::Create(M, this);
			ScreenMid->SetTextureParameterValue(TEXT("Video"), MediaTex);
			// the Screen section's own UVs (u -1..1 across, v -0.13..0.97 up) onto the picture
			ScreenMid->SetScalarParameterValue(TEXT("TileU"), 0.5f);
			ScreenMid->SetScalarParameterValue(TEXT("OffU"), 0.5f);
			ScreenMid->SetScalarParameterValue(TEXT("TileV"), 1.f / 1.10f);
			ScreenMid->SetScalarParameterValue(TEXT("OffV"), 0.13f / 1.10f);
			const int32 Slot = Tv->GetMaterialIndex(TEXT("Screen"));
			Tv->SetMaterial(Slot >= 0 ? Slot : 1, ScreenMid);
		}
		Sound = NewObject<UMediaSoundComponent>(this, TEXT("LifeTvSound"));
		Sound->SetupAttachment(RootComponent);
		Sound->SetMediaPlayer(Player);
		Sound->bAllowSpatialization = true;
		Sound->bOverrideAttenuation = true;
		Sound->AttenuationOverrides.bAttenuate = true;
		Sound->AttenuationOverrides.bSpatialize = true;
		Sound->AttenuationOverrides.FalloffDistance = 14000.f;
		Sound->AttenuationOverrides.AttenuationShapeExtents = FVector(1500.f, 0.f, 0.f);
		if (USoundClass* Amb = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Mix/SC_Ambience.SC_Ambience"), nullptr, LOAD_NoWarn | LOAD_Quiet)) { Sound->SoundClass = Amb; }
		Sound->RegisterComponent();
		Sound->Activate(true);
		Glow = NewObject<URectLightComponent>(this, TEXT("LifeTvGlow"));
		Glow->SetupAttachment(RootComponent);
		Glow->SetMobility(EComponentMobility::Movable);
		Glow->SetIntensityUnits(ELightUnits::Candelas);
		Glow->SetSourceWidth(ScreenW * 0.95f);
		Glow->SetSourceHeight(ScreenH * 0.95f);
		Glow->SetBarnDoorAngle(88.f);
		Glow->SetAttenuationRadius(14000.f);
		Glow->SetCastShadows(true);
		Glow->SetIntensity(0.f);
		Glow->RegisterComponent();
		Glow->SetWorldLocationAndRotation(ScreenC + ScreenN * 40.f, ScreenN.Rotation());
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv screen ready (material %s)"), ScreenMid ? TEXT("ok") : TEXT("MISSING"));
	}
	// ---- what this machine shows (the co-op check compares host and client)
	const int32 Shown = (bOn ? 1000 : 0) + Channel * 20 + Volume;
	static int32 LoggedShown = -1;
	if (Shown != LoggedShown)
	{
		LoggedShown = Shown;
		UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv shows on %s: %s ch %d vol %d at server time %.1f (loop pos %.1f)"), W->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"),
			bOn ? TEXT("ON") : TEXT("off"), Channel, Volume, AntLife::Now(W), PlayPos());
	}
	// ---- awake only when somebody can see or hear it
	bool bNear = false;
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
			bNear = Map4Look::RoomAt(Eye, 200.f) == 2 || FVector::Dist(Eye, ScreenC) < 9000.f || Tv->WasRecentlyRendered(0.5f);
		}
	}
	const bool bWant = bOn && bNear && AntLife::Channels().IsValidIndex(Channel);
	if (bWant && OpenCh != Channel) { OpenChannel(Channel); }
	if (!bOn && OpenCh >= 0) { Player->Close(); OpenCh = -1; bPlaying = false; }
	if (OpenCh >= 0)
	{
		if (!bWant && bPlaying) { Player->Pause(); bPlaying = false; }
		else if (bWant && !bPlaying) { bPendingSeek = true; Player->Play(); bPlaying = true; }
		if (bPendingSeek && Player->IsReady())
		{
			bPendingSeek = false;
			Player->Seek(FTimespan::FromSeconds(PlayPos()));
			Player->Play();
		}
		SyncCheck -= Dt;
		if (bPlaying && SyncCheck <= 0.f && Player->IsPlaying())
		{
			SyncCheck = 4.f;
			const float Have = (float)Player->GetTime().GetTotalSeconds();
			const float Want = PlayPos();
			const float Len = AntLife::Channels()[Channel].Secs;
			float Off = FMath::Abs(Have - Want);
			Off = FMath::Min(Off, Len - Off);
			if (Off > 1.5f) { Player->Seek(FTimespan::FromSeconds(Want)); UE_LOG(LogTemp, Display, TEXT("ANTLIFE tv resync %.2f -> %.2f"), Have, Want); }
		}
	}
	// ---- the picture: power fade, static on a channel change, the glow that follows the picture's colour
	if (Presses != SeenPresses)
	{
		SeenPresses = Presses;
		StaticFlash = 0.35f;
		if (USoundBase* Click = LoadObject<USoundBase>(nullptr, TEXT("/Game/Life/Audio/A_Life_Click.A_Life_Click"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Click, ButtonAt(LastButton), 0.8f);
		}
	}
	static uint8 SeenCh = 255;
	if (SeenCh != Channel) { SeenCh = Channel; StaticFlash = FMath::Max(StaticFlash, 0.3f); }
	StaticFlash = FMath::Max(0.f, StaticFlash - Dt);
	PowerK = FMath::FInterpConstantTo(PowerK, bOn ? 1.f : 0.f, Dt, bOn ? 2.5f : 6.f);
	if (ScreenMid)
	{
		ScreenMid->SetScalarParameterValue(TEXT("Power"), PowerK);
		// the eye adapts to the room: a bright screen by day, a softer one in a dark room (the night exposure would blow it out)
		static const float TodBright[3] = { 1.7f, 1.0f, 0.5f };
		ScreenMid->SetScalarParameterValue(TEXT("Bright"), TodBright[AntLife::Tod()]);
		ScreenMid->SetScalarParameterValue(TEXT("Static"), (bPendingSeek && bOn) ? 1.f : FMath::Clamp(StaticFlash / 0.3f, 0.f, 1.f));
	}
	FLinearColor Want = FLinearColor::Black;
	if (bOn && AntLife::Channels().IsValidIndex(Channel))
	{
		const AntLife::FChannel& C = AntLife::Channels()[Channel];
		const float F = PlayPos() * 4.f;
		const int32 I0 = FMath::Clamp(FMath::FloorToInt(F), 0, C.NumColors - 1), I1 = (I0 + 1) % FMath::Max(1, C.NumColors);
		const float A = F - FMath::FloorToFloat(F);
		auto Col = [&C](int32 I) { return FLinearColor(FColor(C.Colors[I * 3], C.Colors[I * 3 + 1], C.Colors[I * 3 + 2])); };
		Want = C.NumColors > 0 ? FMath::Lerp(Col(I0), Col(I1), A) : FLinearColor::White;
	}
	GlowColor = FMath::CInterpTo(GlowColor, Want, Dt, 6.f);
	if (Glow)
	{
		const float Lum = GlowColor.GetLuminance();
		const float NightK = AntLife::Tod() == 2 ? 1.f : AntLife::Tod() == 1 ? 0.7f : 0.45f;
		Glow->SetLightColor(Lum > 0.001f ? GlowColor / FMath::Max(0.05f, GlowColor.GetMax()) : FLinearColor::White);
		Glow->SetIntensity(PowerK * (0.4f + 2.2f * Lum) * 22.f * NightK);
		Glow->SetVisibility(PowerK > 0.01f && bNear);
	}
	if (Sound)
	{
		Sound->SetVolumeMultiplier(FMath::Max(0.0001f, PowerK * Volume / 10.f * 1.8f));
	}
}
