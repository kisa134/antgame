#pragma once
// The living flat (docs/level/LIFE.md, docs/FEATURES.md section 36): the TV with its channels and its remote, the screens and
// displays, the world outside the windows, the small life of the rooms and the micro details at ant scale.
//   AAntLifeTV     replicated (server-made): the TV's state (on, channel, volume, when the channel started), the remote the ants
//                  jump on, the residents' reactions; the server's own life duties (the robot vacuum's rounds, the dishwasher's
//                  shake, the pigeon's peck). Every drawing machine plays the channel itself (a media player per machine, in sync
//                  by the server clock) and lights the room with it.
//   AAntLifeWorld  local, cosmetic, on every machine that draws: sky, clouds, stars, the house across the yard, birds, cars,
//                  weather, clocks and displays, steam, flies, the moth, the phone, the radio, the music box, the notes, the banner,
//                  the micro details. Its moving parts follow the server clock and the seed, so all machines see the same.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntLife.generated.h"

class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class UFileMediaSource;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class URectLightComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureRenderTarget2D;
class UAudioComponent;
class USoundBase;
class UTexture;
class AAntCharacter;
class AAntKitchenGameMode;

namespace AntLife
{
	enum EKind : uint8 { KCartoon, KNature, KNews, KSport, KAd, KCooking, KKids, KBirds, KStatic, KNum };
	struct FChannel
	{
		const TCHAR* File;     // Content/Movies/Life/<File>.mp4 (staged as a loose file: the media player reads it from disk)
		const TCHAR* Ru;       // what the HUD / the admin calls it
		float Secs;            // the loop's length
		uint8 Kind;
		const uint8* Colors;   // the picture's mean colour, 4 times a second (rgb): the room's glow follows it
		int32 NumColors;
	};
	ANTKITCHEN_API TConstArrayView<FChannel> Channels();
	/** Map v4 look and not -AntNoLife. */
	ANTKITCHEN_API bool Enabled();
	/** Seconds on the server's clock (the same on every machine, within the net lag). */
	ANTKITCHEN_API float Now(const UWorld* W);
	/** 0 clear, 1 rain, 2 snow: the raid seed (or -AntWeather=N / the admin, replicated through the TV actor). */
	ANTKITCHEN_API int32 Weather(const UWorld* W);
	/** Server: 0 clear, 1 rain, 2 snow, -1 back to the seed. */
	ANTKITCHEN_API void SetWeather(UWorld* W, int32 Weather);
	/** Server: the robot vacuum sets off now (for LifeSeconds) / goes away / does its rounds every Seconds (0: none). */
	ANTKITCHEN_API void StartVacuum(UWorld* W, float LifeSeconds = 70.f);
	ANTKITCHEN_API void StopVacuum(UWorld* W);
	ANTKITCHEN_API void SetVacuumEvery(UWorld* W, float Seconds);
	/** Server: the dishwasher starts / stops its cycle (it hums, steams and shakes what stands on the worktop above). */
	ANTKITCHEN_API void StartWash(UWorld* W);
	ANTKITCHEN_API void StopWash(UWorld* W);
	/** Where the pigeon is on the kitchen's outer sill at this server time (false: not there); bPeck: its beak is down now. */
	ANTKITCHEN_API bool PigeonAt(const UWorld* W, float Now, FVector& OutPos, float& OutYaw, bool& bPeck, float& OutFly);
	/** The flat's clock, hours (14.0 by day, 19.5 in the evening, 23.0 at night + a game minute per real second of the raid). */
	ANTKITCHEN_API float ClockHours(const UWorld* W);
	/** The time of day the lights are heading for: 0 day, 1 evening, 2 night. */
	ANTKITCHEN_API int32 Tod();
	/** KitchenBuilder::BeginPlay, every machine: the life actors of this world. */
	ANTKITCHEN_API void Spawn(UWorld* W);
	/** AAntKitchenGameMode::NoiseChecked: the Human came to Source (the TV): he turns it down / off. */
	ANTKITCHEN_API bool HumanChecked(AActor* Source);
	/** Dev / admin (-AntDo, F10): tv on|off|ch N|next|vol N|press B|info, life info|vacuum|wash|pigeon|weather N|props. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}

UCLASS()
class ANTKITCHEN_API AAntLifeTV : public AActor
{
	GENERATED_BODY()

public:
	AAntLifeTV();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AAntLifeTV* Get(const UWorld* W);

	UPROPERTY(Replicated) bool bOn = false;
	UPROPERTY(Replicated) uint8 Channel = 0;
	/** 0..10. */
	UPROPERTY(Replicated) uint8 Volume = 4;
	/** AntLife::Now when the channel started (its loop position is Now - ChannelStart). */
	UPROPERTY(Replicated) float ChannelStart = 0.f;
	/** 0 the residents' routine decides, 1 kept on (admin, or the ants switched it on), 2 kept off (admin). */
	UPROPERTY(Replicated) uint8 Hold = 0;
	/** Counts presses of the remote's buttons (every machine flashes the screen / clicks). */
	UPROPERTY(Replicated) uint8 Presses = 0;
	UPROPERTY(Replicated) uint8 LastButton = 0;
	/** Server clock when the dishwasher started its cycle (< 0: idle) and when the robot vacuum last set off. */
	UPROPERTY(Replicated) float WashStart = -1000.f;
	/** -1 the seed's weather, else 0 clear, 1 rain, 2 snow (admin / tutorial). */
	UPROPERTY(Replicated) int8 WeatherSet = -1;
	/** Server: the robot vacuum's rounds, seconds between (0: none) and when the next one is due. */
	float VacuumEvery = 240.f;
	float NextVacuum = 90.f;
	float NextWash = 60.f;
	bool bWashForced = false;

	/** Server. */
	void SetState(bool bNewOn, int32 NewChannel, int32 NewVolume, const TCHAR* Why);
	/** Server: an ant (or the admin) pressed a button of the remote: 0 power, 1 next channel, 2 volume up, 3 volume down. */
	void Press(int32 Button, AAntCharacter* By);
	/** Server: the Human walked over to it (the noise of a loud TV): turns it down, or off when nobody watches. */
	void HumanChecked();
	float PlayPos() const;
	/** Every machine: the remote's buttons (world) and the screen's centre / normal (for the glow and the cat). */
	FVector ButtonAt(int32 Button) const;
	FVector ScreenCenter() const { return ScreenC; }
	FVector ScreenNormal() const { return ScreenN; }
	bool HasTV() const { return TvComp.IsValid(); }
	int32 Watchers() const { return NumWatchers; }

private:
	void FindTV();
	void ServerTick(float Dt);
	void ClientTick(float Dt);
	void React(int32 Button, AAntCharacter* By);
	void ServerLifeDuties(float Dt);
	void OpenChannel(int32 Ch);

	TWeakObjectPtr<UStaticMeshComponent> TvComp;
	TWeakObjectPtr<UStaticMeshComponent> RemoteComp;
	FVector ScreenC = FVector::ZeroVector, ScreenN = FVector::ForwardVector;
	FVector ScreenX = FVector::RightVector;
	float ScreenW = 0.f, ScreenH = 0.f;
	float FindRetry = 0.f;

	UPROPERTY() UMediaPlayer* Player = nullptr;
	UPROPERTY() UMediaTexture* MediaTex = nullptr;
	UPROPERTY() UMediaSoundComponent* Sound = nullptr;
	UPROPERTY() TArray<UFileMediaSource*> Sources;
	UPROPERTY() UMaterialInstanceDynamic* ScreenMid = nullptr;
	UPROPERTY() URectLightComponent* Glow = nullptr;
	UPROPERTY() UPointLightComponent* Spill = nullptr;
	int32 OpenCh = -1;          // the channel the player has open (-1 none)
	bool bPendingSeek = false;
	bool bPlaying = false;
	float StaticFlash = 0.f;
	uint8 SeenPresses = 0;
	float PowerK = 0.f;          // screen power 0..1 (warm up / off fade)
	FLinearColor GlowColor = FLinearColor::Black;
	float SyncCheck = 0.f;

	// server
	int32 NumWatchers = 0;
	float NobodySince = 0.f;
	float ButtonCool[4] = { 0.f, 0.f, 0.f, 0.f };
	TMap<TWeakObjectPtr<AAntCharacter>, float> OnButton;
	float NoiseClock = 0.f;
	float WashImpulse = 0.f;
	float PeckClock = 0.f;
	float CatStep = -1.f;      // the cat's paw at the screen: when the next move of the sequence is due
	int32 CatMove = 0;
	float ZapAt = 0.f;         // the Dad flips channels himself now and then
	float ReholdAt = -1.f;     // the ants switched it off under the Dad's nose: he turns it back on at this time
	float ReactCool = 0.f;
	int32 LoggedWatch = -1;
};

UCLASS()
class ANTKITCHEN_API AAntLifeWorld : public AActor
{
	GENERATED_BODY()

public:
	AAntLifeWorld();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AAntLifeWorld* Get(const UWorld* W);
	/** Dev: what was built, how much of it is awake. */
	FString Info() const;
	/** The micro details per district (for docs/level/LIFE.md and the log). */
	TMap<FString, int32> PropCounts;
	int32 Built = 0;

	// ---- moving parts (filled by Build*)
	struct FSway { TWeakObjectPtr<USceneComponent> C; FRotator Base; float Amp; float Speed; float Phase; FVector Axis; };
	struct FFlyer { TWeakObjectPtr<UStaticMeshComponent> Body; TWeakObjectPtr<UStaticMeshComponent> WingL, WingR; FVector Center; FVector Radii; float Speed; float Phase; uint8 Kind; };
	struct FCar { TWeakObjectPtr<USceneComponent> Root; TArray<TWeakObjectPtr<USceneComponent>> Lights; float Offset; float Dir; float Lane; };

private:
	void Build();
	void BuildOutside();
	void BuildRoomLife();
	void BuildDisplays();
	void BuildDecor();
	void BuildMicro();
	void ApplySeed();
	void TickOutside(float Dt, float Now, float Tod01);
	void TickRoom(float Dt, float Now);
	void TickDisplays(float Dt, float Now);

	UStaticMeshComponent* Mesh(const TCHAR* Prop, const FTransform& World, const TCHAR* Look = nullptr, bool bSolid = false, USceneComponent* Parent = nullptr);
	UStaticMeshComponent* PrintQuad(UTexture* Tex, const FTransform& World, bool bGlow, USceneComponent* Parent = nullptr, bool bTwoSided = false);
	UInstancedStaticMeshComponent* Ism(const TCHAR* Prop, const TCHAR* Look, bool bSolid);
	UStaticMeshComponent* FindArt(const TCHAR* MeshName, const FVector& Near, float Range) const;
	UMaterialInstanceDynamic* Printed(UTexture* Tex, bool bTwoSided);
	void DrawSegments(UTextureRenderTarget2D* RT, const FString& Text, const FLinearColor& On, const FLinearColor& Bg);
	USoundBase* Sfx(const TCHAR* Name) const;
	UAudioComponent* Loop(const TCHAR* Name, const FVector& At, float Volume, float Radius);

	bool bBuilt = false;
	float BuildRetry = 1.5f;
	int32 SeedSeen = -2;
	float TodK[3] = { 1.f, 0.f, 0.f };   // the blend of day / evening / night (6 s fades like the lights)

	// outside
	UPROPERTY() UStaticMeshComponent* Clouds = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* CloudMid = nullptr;
	UPROPERTY() UStaticMeshComponent* Stars = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* StarMid = nullptr;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> FacadeMids;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> RainMids;
	UPROPERTY() TArray<UStaticMeshComponent*> RainPanes;
	UPROPERTY() UAudioComponent* RainLoop = nullptr;
	TArray<FSway> Sways;
	TArray<FFlyer> Flyers;
	TArray<FCar> Cars;
	FVector RoadA = FVector::ZeroVector, RoadB = FVector::ZeroVector;
	float WeatherClock = 0.f;
	TWeakObjectPtr<UStaticMeshComponent> Pigeon, PigeonWingL, PigeonWingR;
	float PigeonPhase = -1.f;
	bool bPigeonWarned = false;

	// the rooms
	struct FSteam { FVector At; float Size; float Every; float Clock; uint8 When; };   // When: 0 always, 1 kettle boils, 2 the soup is on, 3 the dishwasher runs
	TArray<FSteam> Steams;
	struct FBuzz { TWeakObjectPtr<UStaticMeshComponent> Body; TWeakObjectPtr<UStaticMeshComponent> WingL, WingR; FVector Center; float R; float Speed; float Phase; bool bMoth; };
	TArray<FBuzz> Buzzers;
	UPROPERTY() UAudioComponent* FlyLoop = nullptr;
	UPROPERTY() UAudioComponent* RadioLoop = nullptr;
	UPROPERTY() UAudioComponent* MusicBoxLoop = nullptr;
	UPROPERTY() UAudioComponent* WashLoop = nullptr;
	UPROPERTY() UAudioComponent* KettleLoop = nullptr;
	TWeakObjectPtr<UStaticMeshComponent> Dishwasher;
	FVector DishwasherHome = FVector::ZeroVector;
	TArray<TPair<TWeakObjectPtr<USceneComponent>, FVector>> Shakers;   // small things on top that the machine shakes (home locations)
	TWeakObjectPtr<UStaticMeshComponent> Phone;
	FTransform PhoneHome;
	float PhoneBuzzAt = 0.f;
	UPROPERTY() UMaterialInstanceDynamic* PhoneMid = nullptr;
	TWeakObjectPtr<UStaticMeshComponent> MusicStar;
	TWeakObjectPtr<UStaticMeshComponent> ClockHands[3];
	FVector ClockAxis = FVector::ForwardVector;
	FQuat ClockBase = FQuat::Identity;
	FVector FaucetTip = FVector::ZeroVector;
	float DripClock = 0.f;
	float SnoreClock = 0.f;
	TWeakObjectPtr<UStaticMeshComponent> DadPhoneScreen;
	UPROPERTY() UPointLightComponent* DadPhoneLight = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* DadPhoneMid = nullptr;
	TWeakObjectPtr<UStaticMeshComponent> DadPhoneHost;

	// displays
	UPROPERTY() UTextureRenderTarget2D* MicrowaveRT = nullptr;
	UPROPERTY() UTextureRenderTarget2D* FridgeRT = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* MicrowaveMid = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* FridgeMid = nullptr;
	FString MicrowaveShown, FridgeShown;
	float DisplayClock = 0.f;
	// the kid's tablet: its own little player (the kids' channel), awake only near it
	UPROPERTY() UMediaPlayer* TabletPlayer = nullptr;
	UPROPERTY() UMediaTexture* TabletTex = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* TabletMid = nullptr;
	UPROPERTY() UFileMediaSource* TabletSource = nullptr;
	FVector TabletAt = FVector::ZeroVector;
	bool bTabletOpen = false;

	UPROPERTY() TArray<UInstancedStaticMeshComponent*> Isms;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> PrintMids;
	float AwakeRange = 9000.f;
};
