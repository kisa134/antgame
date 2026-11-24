#include "AntHUD.h"
#include "AntCarry.h"
#include "AntVersus.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "FolkCharacters.h"
#include "AntArmy.h"
#include "AntChain.h"
#include "WarMode.h"
#include "CreatureCharacters.h"
#include "AntTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "AntFx.h"
#include "AntRun.h"
#include "AntRaid.h"
#include "AntFilm.h"
#include "KitchenSlick.h"
#include "Misc/ScopeExit.h"
#include "AntRts.h"
#include "AntAdmin.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntInside.h"
#include "AntFighter.h"
#include "AntFighterUi.h"
#include "AntReact.h"
#include "AntTutorial.h"

namespace
{
	/** Dev: -AntHudTest plays the shift HUD with made-up numbers (drawn only; no game state is touched). */
	bool HudTest()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("AntHudTest"));
		return b;
	}
}

namespace HudColors
{
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Grey(0.7f, 0.7f, 0.7f);
	const FLinearColor Gold(1.f, 0.8f, 0.2f);
	const FLinearColor Red(1.f, 0.3f, 0.25f);
	const FLinearColor Green(0.4f, 1.f, 0.4f);
	const FLinearColor Panel(0.012f, 0.007f, 0.004f, 0.62f);   // warm ink (docs/UI_STYLE.md)
}
using namespace HudColors;

// ---------------------------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------------------------

void AAntHUD::Txt(const FString& Str, float X, float Y, const FLinearColor& C, UFont* F, float Scale, bool bCenter)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, F, Scale);
	const float DX = bCenter ? X - W * 0.5f : X;
	DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.8f * C.A), DX + 2.f, Y + 2.f, F, Scale);
	DrawText(Str, C, DX, Y, F, Scale);
}

void AAntHUD::TxtRight(const FString& Str, float RightX, float Y, const FLinearColor& C, UFont* F, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, F, Scale);
	Txt(Str, RightX - W, Y, C, F, Scale);
}

void AAntHUD::Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Fill)
{
	if (AntUi::Classic())
	{
		DrawRect(Panel, X, Y, W, H);
		DrawRect(Fill, X + 2.f, Y + 2.f, (W - 4.f) * FMath::Clamp(Frac, 0.f, 1.f), H - 4.f);
		return;
	}
	AntUiCanvas::Bar(Canvas, X, Y, W, H, Frac, Fill);   // ink track, fill, a light edge, ticks (AntUiCanvas.cpp)
}

void AAntHUD::Marker(const FVector& World, const FString& Label, const FLinearColor& C, bool bClampToEdges)
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	const FVector Local = CamRot.UnrotateVector(World - CamLoc);
	const int32 Meters = FMath::RoundToInt(FVector::Dist(World, CamLoc) / 100.f);
	const FString Full = FString::Printf(TEXT("%s %dм"), *Label, Meters);

	FVector2D P;
	if (Local.X > 1.f)
	{
		const FVector Proj = Project(World);
		P = FVector2D(Proj.X, Proj.Y);
	}
	else
	{
		if (!bClampToEdges) return;
		P = FVector2D(Local.Y >= 0.f ? W * 0.95f : W * 0.05f, H * 0.5f);
	}

	const bool bOff = P.X < 0.f || P.X > W || P.Y < 0.f || P.Y > H;
	if (bOff && !bClampToEdges)
	{
		return;
	}
	P.X = FMath::Clamp(P.X, 60.f * S, W - 60.f * S);
	P.Y = FMath::Clamp(P.Y, 60.f * S, H - 60.f * S);
	if (Label.IsEmpty())
	{
		// No words: just a small quiet mark where the thing is.
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), P.X - 4.f * S, P.Y - 4.f * S, 8.f * S, 8.f * S);
		DrawRect(FLinearColor(C.R, C.G, C.B, 0.8f), P.X - 3.f * S, P.Y - 3.f * S, 6.f * S, 6.f * S);
		return;
	}
	if (const TCHAR* IconName = AntUi::Classic() ? nullptr : AntUi::IconForLabel(Label))
	{
		// The front end's icon for the thing (cake, nest, the Giant, ...) on a small ink disc, the words under it.
		const float IS = 34.f * S;
		AntUiCanvas::Tile(Canvas, AntUi::Tex(TEXT("Materials/T_UI_Dot")), P.X - IS * 0.8f, P.Y - IS * 0.8f, IS * 1.6f, IS * 1.6f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		AntUiCanvas::Icon(Canvas, IconName, P.X - IS * 0.5f, P.Y - IS * 0.5f, IS);
		Txt(Full, P.X, P.Y + IS * 0.55f, C, GEngine->GetSmallFont(), 1.05f * S, true);
		return;
	}
	DrawRect(C, P.X - 5.f * S, P.Y - 5.f * S, 10.f * S, 10.f * S);
	Txt(Full, P.X, P.Y + 8.f * S, C, GEngine->GetSmallFont(), 1.1f * S, true);
}

void AAntHUD::Crosshair()
{
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const FLinearColor C(1.f, 1.f, 1.f, 0.9f);
	DrawRect(C, CX - 1.f, CY - 10.f * S, 2.f, 6.f * S);
	DrawRect(C, CX - 1.f, CY + 4.f * S, 2.f, 6.f * S);
	DrawRect(C, CX - 10.f * S, CY - 1.f, 6.f * S, 2.f);
	DrawRect(C, CX + 4.f * S, CY - 1.f, 6.f * S, 2.f);

	// Hit / kill markers.
	if (const AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const float HitAge = Now - PC->HitMarkerTime;
		const float KillAge = Now - PC->KillMarkerTime;
		auto Cross = [this, CX, CY](float R, float Th, const FLinearColor& Col)
		{
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				for (int32 sy = -1; sy <= 1; sy += 2)
				{
					for (float k = 0.45f; k <= 1.f; k += 0.08f)
					{
						DrawRect(Col, CX + sx * R * k - Th * 0.5f, CY + sy * R * k - Th * 0.5f, Th, Th);
					}
				}
			}
		};
		if (KillAge < 0.45f)
		{
			Cross(30.f * S * (1.f + KillAge), 4.f * S, FLinearColor(1.f, 0.15f, 0.1f, 1.f - KillAge / 0.45f));
		}
		else if (HitAge < 0.18f)
		{
			Cross(18.f * S, 3.f * S, PC->bHitCrit ? FLinearColor(1.f, 0.85f, 0.1f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 1.f - HitAge / 0.18f));
		}
		if (KillAge < 2.f)
		{
			Txt(PC->KillText, CX, CY + 60.f * S, FLinearColor(1.f, 0.3f, 0.2f, FMath::Clamp(2.f - KillAge, 0.f, 1.f)), GEngine->GetMediumFont(), 1.3f * S, true);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	S = FMath::Max(0.7f, Canvas->ClipY / 1080.f) * AntUi::HudScale();   // the HUD scale setting (AntUiSettings.h)

	// Dev: -AntFpsLog writes the frame rate to the log (to measure without a screenshot).
	static const bool bFpsLog = FParse::Param(FCommandLine::Get(), TEXT("AntFpsLog"));
	if (bFpsLog)
	{
		const float FrameDt = (float)FApp::GetDeltaTime();
		FpsTime += FrameDt;
		++FpsFrames;
		FpsWorst = FMath::Max(FpsWorst, FrameDt);
		if (FpsTime >= 5.0)
		{
			const AAntFxManager* Fx = AAntFxManager::Get(this);
			UE_LOG(LogTemp, Display, TEXT("ANTFPS avg=%.1f worst_frame_ms=%.1f fx_bits=%d"), FpsFrames / FpsTime, FpsWorst * 1000.f, Fx ? Fx->NumLive() : 0);
			FpsTime = 0.0;
			FpsFrames = 0;
			FpsWorst = 0.f;
		}
	}

	AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	APawn* Me = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;

	// Filming mode (F8): F2 hides everything of the HUD - the crosshair, the toasts, the panels; the film's own overlay stays.
	UAntFilm* Film = PC ? PC->Film : nullptr;
	if (Film && Film->HidesHud())
	{
		Film->DrawOverlay(this, Canvas);
		return;
	}
	ON_SCOPE_EXIT { if (Film && Canvas) { Film->DrawOverlay(this, Canvas); } };
	ON_SCOPE_EXIT { if (Canvas) { AntUiCanvas::DrawTests(this, Canvas); } };   // dev: -AntUiTest=cards|debrief|debrieflose

	// A raid: the nest screen and the results cover everything else.
	AAntRaid* Raid = (GS && GS->bHeistMode) ? AAntRaid::Get(GetWorld()) : nullptr;
	AntTut::DemoHud(this, Canvas, PC);   // the Steam demo: counts its raids, the end screen (AntTutorial.cpp)
	if (Raid && Raid->Phase != AntRaidPhase::Raid)
	{
		DrawRaid(Raid, GS, Me);
		AntFighterUi::DrawNest(this, Canvas, PC, Raid->Phase == AntRaidPhase::Results);   // the fighter's screens, the post-raid highlights (AntFighterUi.h)
		DrawToastsAndNumbers(PC);
		return;
	}
	const bool bVoteScreen = GS && GS->bHeistMode && GS->Interlude >= 2;
	if (!bVoteScreen)
	{
		DrawWorldTags(Me);   // names and health bars would sit on top of the cards
		AntCarry::DrawHud(this, Canvas, Me);   // strain marks on carried loads, the carriers' shouts
	}

	if (AAntCharacter* Ant = Cast<AAntCharacter>(Me))
	{
		DrawAnt(Ant);
	}
	else if (AHumanCharacter* H = Cast<AHumanCharacter>(Me))
	{
		DrawHuman(H, GS);
	}
	else
	{
		Txt(TEXT("Ожидание возрождения..."), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.45f, White, GEngine->GetLargeFont(), 1.4f * S, true);
	}

	DrawTop(GS);
	DrawAnnounce();
	DrawWar(GS, Me);
	DrawTrophies(GS, Me);
	DrawRun(GS, Me);
	if (Raid) { DrawRaid(Raid, GS, Me); }
	if (Raid) { AntRts::DrawHud(this, Canvas, PC); }   // the snowball: mass, picks, squad groups, selection (AntRts.cpp)
	AntAdmin::DrawHud(this, Canvas, PC);   // the АДМИН badge, "show everyone" (AntAdmin.h)
	AntFighterUi::DrawRaid(this, Canvas, PC);   // medal toasts; the profile's own tick (AntFighterUi.h)
	AntTut::DrawHud(this, Canvas, PlayerOwner);   // «Учебка»: the objective card, the sergeant's lines, markers, the newsreels (AntTutorial.cpp)
	AntInside::DrawHud(this, Canvas, PlayerOwner);   // inside a resident: the place, the keys, the warning; the Human's hints (AntInside.h)
	AntReact::DrawHud(this, Canvas, PlayerOwner);    // flour in the eyes, a powder cloud round the camera, the blackout line (AntReact.h)
	DrawFeed(GS);
	DrawToastsAndNumbers(PC);
	DrawHelp(Me);
	if (PC && PC->bShowScores)
	{
		DrawScoreboard(GS);
	}
}

void AAntHUD::DrawTop(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	const float W = Canvas->ClipX;
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();

	const FLinearColor Fire(1.f, 0.4f, 0.2f);
	AAntRaid* Raid = GS->bHeistMode ? AAntRaid::Get(GetWorld()) : nullptr;
	if (Raid && AntTut::OwnsRaid(Raid))
	{
		return;   // the training: its own card and timers (AntTutorial.cpp)
	}
	if (Raid)
	{
		DrawRaidTop(Raid, GS);
		return;   // (how a raid ended is the results screen's business)
	}
	if (GS->bHeistMode)
	{
		DrawShift(GS);
	}
	else
	{
		Txt(FString::Printf(TEXT("КОЛОНИЯ %d"), GS->TeamScore), W * 0.5f - 140.f * S, 12.f * S, Gold, Med, 1.3f * S, true);
		Txt(FString::Printf(TEXT("ОГНЕННЫЕ %d"), GS->RivalScore), W * 0.5f + 140.f * S, 12.f * S, Fire, Med, 1.3f * S, true);
		Txt(FString::Printf(TEXT("до %d"), GS->TargetScore), W * 0.5f, 16.f * S, Grey, GEngine->GetSmallFont(), 1.1f * S, true);
		const float Tg = FMath::Max(1, GS->TargetScore);
		Bar(W * 0.5f - 270.f * S, 48.f * S, 260.f * S, 16.f * S, GS->TeamScore / Tg, Gold);
		Bar(W * 0.5f + 10.f * S, 48.f * S, 260.f * S, 16.f * S, GS->RivalScore / Tg, Fire);
		if (GS->HumanRage > 0)
		{
			Txt(FString::Printf(TEXT("ЯРОСТЬ ЧЕЛОВЕКА: %d / 3"), GS->HumanRage), W * 0.5f, 128.f * S, Red, GEngine->GetSmallFont(), 1.15f * S, true);
		}
		const int32 T = FMath::Max(0, FMath::CeilToInt(GS->TimeRemaining));
		Txt(FString::Printf(TEXT("%d:%02d"), T / 60, T % 60), W * 0.5f, 70.f * S, T < 60 ? Red : White, Med, 1.3f * S, true);
		Txt(FString::Printf(TEXT("Раздавлено муравьёв: %d    Человек в панике: %d раз"), GS->AntsSquashed, GS->HumanKnockouts), W * 0.5f, 104.f * S, Grey, GEngine->GetSmallFont(), 1.1f * S, true);
	}

	if (GS->Phase != 0 && !(GS->bHeistMode && GS->Interlude >= 2))
	{
		// The shift: either the quota is in and everyone made it (Phase 1), or the colony starves and the run is over.
		const bool bWon = GS->Phase == 1;
		const FString Banner = GS->bHeistMode
			? (bWon ? FString::Printf(TEXT("КВОТА ВЫПОЛНЕНА! СМЕНА %d ПРОЙДЕНА"), GS->Shift) : FString::Printf(TEXT("КОЛОНИЯ ГОЛОДАЕТ. ЗАБЕГ ОКОНЧЕН НА СМЕНЕ %d"), GS->Shift))
			: FString(GS->Phase == 1 ? TEXT("КОЛОНИЯ ПОБЕДИЛА!") : GS->Phase == 3 ? TEXT("ОГНЕННЫЕ МУРАВЬИ ПОБЕДИЛИ...") : TEXT("ЧЕЛОВЕК ПОБЕДИЛ!"));
		const float BY = Canvas->ClipY * 0.3f;
		DrawRect(Panel, 0.f, BY, W, (GS->bHeistMode ? 200.f : 140.f) * S);
		if (GS->bHeistMode)
		{
			const FLinearColor Accent = bWon ? Gold : Red;
			DrawRect(Accent, 0.f, BY, W, 4.f * S);
			DrawRect(Accent, 0.f, BY + 196.f * S, W, 4.f * S);
			// Long line: shrink it to fit narrow windows.
			float TW = 0.f, TH = 0.f;
			GetTextSize(Banner, TW, TH, Big, 2.2f * S);
			const float Fit = FMath::Min(1.f, W * 0.94f / FMath::Max(1.f, TW));
			Txt(Banner, W * 0.5f, BY + 24.f * S, Accent, Big, 2.2f * S * Fit, true);
			Txt(FString::Printf(TEXT("Добыча %d из %d  ·  трофеев целыми %d  ·  разбито %d  ·  раздавлено муравьёв %d"), GS->TeamScore, GS->TargetScore, GS->TrophiesWhole, GS->TrophiesBroken, GS->AntsSquashed),
				W * 0.5f, BY + 104.f * S, White, Med, 1.1f * S, true);
			Txt(bWon ? TEXT("Дальше: выбор карты для колонии, потом новая смена") : FString::Printf(TEXT("Забег: пройдено смен %d, добыча %d. Новый забег через несколько секунд..."), GS->RunShifts, GS->RunHaul),
				W * 0.5f, BY + 150.f * S, Grey, Med, 1.f * S, true);
		}
		else
		{
			Txt(Banner, W * 0.5f, BY + 20.f * S, Gold, Big, 2.5f * S, true);
			Txt(TEXT("Новый раунд через несколько секунд..."), W * 0.5f, BY + 95.f * S, White, Med, 1.f * S, true);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// The shift: quota bar that jumps when food comes in, larvae as icons, big announcements
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawShift(AAntGameState* GS)
{
	const float W = Canvas->ClipX;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Dt = GetWorld()->GetDeltaSeconds();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	const FLinearColor Fire(1.f, 0.4f, 0.2f);

	int32 Shift = GS->Shift, Score = GS->TeamScore, Target = FMath::Max(1, GS->TargetScore), Larvae = GS->Larvae;
	int32 Stage = FMath::Min<int32>(GS->ShiftStage, 3);
	bool bQuota = GS->bQuotaMet;
	int32 TimeLeft = FMath::Max(0, FMath::CeilToInt(GS->TimeRemaining));
	if (HudTest())
	{
		// Made-up numbers that walk through every state of this panel.
		Shift = 2;
		Target = 300;
		Score = FMath::Min(360, FMath::FloorToInt(Now / 1.5f) * 30);
		Larvae = 5 - (FMath::FloorToInt(Now / 4.f) % 6);
		Stage = FMath::FloorToInt(Now / 3.8f) % 4;
		bQuota = Score >= Target;
		if (Stage == 0 && SeenStage == 3)
		{
			SeenShift = -1;   // the test loops: show the "new shift" line again
		}
	}

	// --- What changed since the last frame ---
	const bool bLive = GS->Phase == 0;
	if (SeenShift != Shift)
	{
		SeenShift = Shift;
		SeenStage = Stage;
		bSeenQuota = bQuota;
		SeenScore = Score;
		SeenLarvae = Larvae;
		if (bLive && Stage == 0)
		{
			Announce(FString::Printf(TEXT("СМЕНА %d"), Shift), FString::Printf(TEXT("Королеве нужно %d еды. Человек залип в телефон — лутай спокойно"), Target), Gold);
		}
	}
	if (Stage != SeenStage)
	{
		SeenStage = Stage;
		if (bLive)
		{
			if (Stage == 1) { Announce(TEXT("ЧЕЛОВЕК НА КУХНЕ"), TEXT("Теперь осторожно: он топчет всё, что шевелится"), FLinearColor(1.f, 0.55f, 0.25f)); }
			else if (Stage == 2) { Announce(TEXT("УБОРКА!"), TEXT("Он зол и взялся за дихлофос. Осталось 2 минуты"), Red); }
			else if (Stage == 3) { Announce(TEXT("ВСЕ В ГНЕЗДО!"), TEXT("Последние 30 секунд смены"), Gold); }
		}
	}
	if (bQuota && !bSeenQuota && bLive)
	{
		Announce(TEXT("КВОТА ВЫПОЛНЕНА!"), TEXT("Все в гнездо — и смена ваша. Остаться за добавкой — риск"), Gold);
	}
	bSeenQuota = bQuota;
	if (SeenScore >= 0 && Score > SeenScore)
	{
		ScoreGain = Score - SeenScore;
		GainFromFrac = FMath::Clamp(SeenScore / (float)Target, 0.f, 1.f);
		ScorePulse = 1.f;
	}
	SeenScore = Score;
	if (SeenLarvae >= 0 && Larvae < SeenLarvae)
	{
		LarvaPop = 1.f;
		LarvaPopCount = SeenLarvae - Larvae;
	}
	SeenLarvae = Larvae;
	ScorePulse = FMath::Max(0.f, ScorePulse - Dt * 1.3f);
	LarvaPop = FMath::Max(0.f, LarvaPop - Dt * 1.6f);

	// --- Row 1: shift, clock, the rivals ---
	const float CX = W * 0.5f;
	const bool bLastCall = TimeLeft <= 30 && bLive;
	const float ClockPulse = bLastCall ? 1.f + 0.12f * FMath::Abs(FMath::Sin(Now * 6.f)) : 1.f;
	Txt(FString::Printf(TEXT("%d:%02d"), TimeLeft / 60, TimeLeft % 60), CX, 6.f * S, TimeLeft < 60 ? Red : White, Med, 1.45f * S * ClockPulse, true);
	TxtRight(FString::Printf(TEXT("СМЕНА %d"), Shift), CX - 76.f * S, 13.f * S, Gold, Small, 1.25f * S);
	Txt(FString::Printf(TEXT("ОГНЕННЫЕ %d"), GS->RivalScore), CX + 76.f * S, 13.f * S, Fire, Small, 1.15f * S);

	// --- Row 2: the quota bar ---
	const float Frac = FMath::Clamp(Score / (float)Target, 0.f, 1.f);
	const float Kick = FMath::Square(ScorePulse);                 // sharp at first, then it settles
	const float Grow = Kick * 5.f * S;
	const float BW = 440.f * S + Grow * 2.f, BH = 22.f * S + Grow * 2.f;
	const float BX = CX - BW * 0.5f, BY = 42.f * S - Grow;
	const FLinearColor Done(0.5f, 1.f, 0.45f);
	const FLinearColor Base = bQuota ? Done : Gold;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), BX - 2.f, BY - 2.f, BW + 4.f, BH + 4.f);
	DrawRect(FMath::Lerp(Base, White, Kick * 0.6f), BX, BY, BW * Frac, BH);
	if (ScorePulse > 0.f && Frac > GainFromFrac)
	{
		// The piece that just came in flashes white.
		DrawRect(FLinearColor(1.f, 1.f, 1.f, ScorePulse), BX + BW * GainFromFrac, BY, BW * (Frac - GainFromFrac), BH);
	}
	for (float Mark : { 0.25f, 0.5f, 0.75f })
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), BX + BW * Mark - 1.f, BY, 2.f, BH);   // the Human gets angrier at each of these
	}
	if (bQuota)
	{
		const float Glint = 0.5f + 0.5f * FMath::Sin(Now * 4.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.10f + 0.12f * Glint), BX, BY, BW, BH);
	}
	{
		const FString QuotaText = bQuota ? FString::Printf(TEXT("КВОТА ВЫПОЛНЕНА  %d / %d"), Score, Target) : FString::Printf(TEXT("КВОТА  %d / %d"), Score, Target);
		if (Frac > 0.42f)
		{
			// The fill is bright under the text: dark letters, no shadow.
			float QW = 0.f, QH = 0.f;
			GetTextSize(QuotaText, QW, QH, Small, 1.15f * S);
			DrawText(QuotaText, FLinearColor(0.05f, 0.04f, 0.02f), CX - QW * 0.5f, BY + BH * 0.5f - 10.f * S, Small, 1.15f * S);
		}
		else
		{
			Txt(QuotaText, CX, BY + BH * 0.5f - 10.f * S, White, Small, 1.15f * S, true);
		}
	}
	if (ScorePulse > 0.f && ScoreGain > 0)
	{
		Txt(FString::Printf(TEXT("+%d"), ScoreGain), BX + BW + 14.f * S, BY - (1.f - ScorePulse) * 22.f * S, FLinearColor(1.f, 0.9f, 0.35f, FMath::Min(1.f, ScorePulse * 2.f)), Med, 1.25f * S);
	}

	// --- Row 3: larvae = respawns left, one icon each ---
	const float LY = 84.f * S;
	const int32 Shown = FMath::Clamp(Larvae, 0, 12);
	const float Gap = 17.f * S;
	auto Egg = [this](float X, float Y, float K, const FLinearColor& C)
	{
		DrawRect(C, X - 3.f * S * K, Y - 7.f * S * K, 6.f * S * K, 14.f * S * K);
		DrawRect(C, X - 5.f * S * K, Y - 4.5f * S * K, 10.f * S * K, 9.f * S * K);
	};
	if (Larvae <= 0)
	{
		Txt(TEXT("ЛИЧИНОК НЕТ — возрождений больше не будет"), CX, LY - 9.f * S, Red, Small, 1.1f * S, true);
	}
	else
	{
		float LW = 0.f, LH = 0.f;
		const FString Label = TEXT("ЛИЧИНКИ");
		GetTextSize(Label, LW, LH, Small, 1.05f * S);
		const float Total = LW + 10.f * S + Shown * Gap;
		float X = CX - Total * 0.5f;
		Txt(Label, X, LY - 9.f * S, Grey, Small, 1.05f * S);
		X += LW + 10.f * S + Gap * 0.5f;
		const FLinearColor Cream(1.f, 0.93f, 0.72f);
		for (int32 i = 0; i < Shown; ++i)
		{
			Egg(X + i * Gap, LY, 1.f, Larvae == 1 ? Red : Cream);
		}
		if (Larvae > Shown)
		{
			Txt(FString::Printf(TEXT("+%d"), Larvae - Shown), X + Shown * Gap, LY - 9.f * S, Grey, Small, 1.05f * S);
		}
		// The ones just spent pop: they swell and fade where they were.
		if (LarvaPop > 0.f)
		{
			for (int32 i = 0; i < FMath::Min(LarvaPopCount, 3) && Shown + i < 13; ++i)
			{
				Egg(X + (Shown + i) * Gap, LY - (1.f - LarvaPop) * 6.f * S, 1.f + (1.f - LarvaPop) * 1.6f, FLinearColor(1.f, 0.25f, 0.2f, LarvaPop));
			}
		}
	}
	if (Larvae <= 0 && LarvaPop > 0.f)
	{
		Egg(CX, LY + 16.f * S, 1.f + (1.f - LarvaPop) * 1.6f, FLinearColor(1.f, 0.25f, 0.2f, LarvaPop));
	}

	// --- Row 4: what the Human is doing (one quiet line; the stage change itself is announced big) ---
	static const TCHAR* StageName[] = { TEXT("Человек залип в телефон: лутай спокойно"), TEXT("Человек на кухне"), TEXT("УБОРКА: он зол и с дихлофосом"), TEXT("ВСЕ В ГНЕЗДО!") };
	FString Line = StageName[Stage];
	if (Stage >= 1 && Stage < 3 && GS->HumanGoal != AntHumanGoal::Idle && !GS->HumanGoalText.IsEmpty())
	{
		// What he is heading for right now: a noise, a door left open, somebody he saw.
		Line = FString::Printf(TEXT("%s: %s"), Stage == 2 ? TEXT("УБОРКА. Человек") : TEXT("Человек"), *GS->HumanGoalText);
	}
	if (GS->HumanRage > 0 && Stage < 2)
	{
		Line += FString::Printf(TEXT("  ·  ярость %d/3"), GS->HumanRage);
	}
	const FLinearColor StageColor = Stage == 3 ? Gold : Stage == 2 ? Red : Stage == 1 ? FLinearColor(1.f, 0.7f, 0.45f) : Grey;
	Txt(Line, CX, 104.f * S, StageColor, Small, 1.15f * S, true);
}

void AAntHUD::Announce(const FString& Title, const FString& Sub, const FLinearColor& Color)
{
	AnnTitle = Title;
	AnnSub = Sub;
	AnnColor = Color;
	AnnTime = GetWorld()->GetTimeSeconds();
}

void AAntHUD::DrawAnnounce()
{
	const float Hold = 3.4f;
	const float Age = GetWorld()->GetTimeSeconds() - AnnTime;
	if (Age < 0.f || Age > Hold || AnnTitle.IsEmpty())
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float In = FMath::Clamp(Age / 0.16f, 0.f, 1.f);
	const float Out = FMath::Clamp((Hold - Age) / 0.6f, 0.f, 1.f);
	const float A = In * Out;
	const float Pop = 1.f + 0.4f * FMath::Square(1.f - In);   // slams in big, settles
	const float Y = H * 0.185f;
	const float BandH = 112.f * S;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * A), 0.f, Y, W, BandH);
	FLinearColor C = AnnColor;
	C.A = A;
	const float LineW = W * (0.2f + 0.8f * In);
	DrawRect(C, (W - LineW) * 0.5f, Y, LineW, 3.f * S);
	DrawRect(C, (W - LineW) * 0.5f, Y + BandH - 3.f * S, LineW, 3.f * S);
	UFont* Big = GEngine->GetLargeFont();
	float TW = 0.f, TH = 0.f;
	GetTextSize(AnnTitle, TW, TH, Big, 2.3f * S);
	Txt(AnnTitle, W * 0.5f, Y + 12.f * S - TH * (Pop - 1.f) * 0.5f, C, Big, 2.3f * S * Pop, true);
	Txt(AnnSub, W * 0.5f, Y + 74.f * S, FLinearColor(1.f, 1.f, 1.f, A), GEngine->GetMediumFont(), 1.05f * S, true);
}

void AAntHUD::EdgeGlow(const FLinearColor& Color, float Strength, float Depth)
{
	if (Strength <= 0.01f)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const int32 N = 28;   // thin strips: the falloff reads as a gradient, not as bands
	const float Step = Depth / N;
	for (int32 i = 0; i < N; ++i)
	{
		FLinearColor C = Color;
		C.A = FMath::Clamp(Strength * FMath::Pow(1.f - i / (float)N, 2.4f), 0.f, 1.f);
		DrawRect(C, 0.f, i * Step, W, Step);
		DrawRect(C, 0.f, H - (i + 1) * Step, W, Step);
		DrawRect(C, i * Step, 0.f, Step, H);
		DrawRect(C, W - (i + 1) * Step, 0.f, Step, H);
	}
}
void AAntHUD::DrawFeed(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Y = 14.f * S;
	int32 Shown = 0;
	// Short and quick to fade: the newest four lines, six seconds each.
	for (int32 i = GS->Feed.Num() - 1; i >= 0 && Shown < 4; --i)
	{
		const FAntFeedLine& L = GS->Feed[i];
		const float Age = Now - L.Time;
		if (Age > 6.f)
		{
			continue;
		}
		FLinearColor C = L.Color;
		C.A = FMath::Clamp((6.f - Age) / 1.5f, 0.f, 1.f) * (Shown == 0 ? 1.f : 0.8f);
		TxtRight(L.Text, Canvas->ClipX - 20.f * S, Y, C, GEngine->GetSmallFont(), 1.15f * S);
		Y += 24.f * S;
		++Shown;
	}
}

// ---------------------------------------------------------------------------------------------
// Ant view
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawAnt(AAntCharacter* Ant)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const float Dt = GetWorld()->GetDeltaSeconds();

	if (LastHealth >= 0.f && Ant->Health < LastHealth - 0.5f)
	{
		HurtFlash = 1.f;
	}
	LastHealth = Ant->Health;
	if (HudTest() && FMath::Fmod(GetWorld()->GetTimeSeconds(), 3.f) < Dt)
	{
		HurtFlash = 1.f;
	}
	if (HurtFlash > 0.f)
	{
		// Hit: the edges of the screen flash red (the middle stays clear: you still have to see where to run).
		EdgeGlow(FLinearColor(0.95f, 0.04f, 0.02f), 0.6f * HurtFlash, 170.f * S);
		DrawRect(FLinearColor(0.8f, 0.f, 0.f, 0.06f * HurtFlash), 0.f, 0.f, W, H);
		HurtFlash -= Dt * 2.2f;
	}
	// Nearly dead: the edges beat like a heart, faster the less health is left.
	{
		const float Frac01 = HudTest() ? 0.18f : Ant->Health / FMath::Max(1.f, Ant->MaxHealth);
		if (!Ant->bDead && Frac01 < 0.35f)
		{
			const float Bad = 1.f - Frac01 / 0.35f;
			HeartPhase += Dt * (1.15f + 0.9f * Bad);
			const float Ph = FMath::Frac(HeartPhase);
			const float Lub = FMath::Max(0.f, 1.f - Ph / 0.16f);
			const float Dub = FMath::Max(0.f, 1.f - FMath::Abs(Ph - 0.30f) / 0.11f) * 0.6f;
			EdgeGlow(FLinearColor(0.6f, 0.f, 0.f), (0.10f + 0.4f * FMath::Max(Lub, Dub)) * (0.45f + 0.55f * Bad), 190.f * S);
		}
		else
		{
			HeartPhase = 0.f;
		}
	}

	if (Ant->bDead)
	{
		const bool bStuck = Ant->StuckTo != nullptr;
		// Who flattened you: the Human's slipper, grandma's swatter or the kid's sneaker (they are the same jointed figure in code).
		const bool bGran = bStuck && Ant->StuckTo->IsA<AGrandmaCharacter>();
		const bool bKid = bStuck && Ant->StuckTo->IsA<AKidCharacter>();
		Txt(bGran ? TEXT("БАБУШКА ПРИХЛОПНУЛА ТЕБЯ МУХОБОЙКОЙ") : bKid ? TEXT("МАЛЫШ ТЕБЯ РАСТОПТАЛ") : bStuck ? TEXT("ТЫ ПРИЛИП К ТАПКУ") : TEXT("ТЕБЯ РАЗДАВИЛИ"),
			W * 0.5f, H * 0.36f, Red, Big, (bGran ? 1.7f : 2.2f) * S, true);
		const AAntGameState* DeadGS = GetWorld()->GetGameState<AAntGameState>();
		const bool bOut = DeadGS && DeadGS->bHeistMode && DeadGS->Larvae <= 0 && DeadGS->Phase == 0;
		Txt(bOut ? TEXT("Личинок нет. Вернуть тебя могут, если свои принесут твоё тело в гнездо")
			: bGran ? TEXT("Висишь лепёшкой на мухобойке. Возрождение через пару секунд...")
			: bKid ? TEXT("Катаешься лепёшкой на подошве кеда. Возрождение через пару секунд...")
			: bStuck ? TEXT("Катаешься лепёшкой на подошве. Возрождение через пару секунд...") : TEXT("Возрождение в муравейнике через пару секунд..."),
			W * 0.5f, H * 0.36f + 80.f * S, White, Med, 1.1f * S, true);
		return;
	}

	Crosshair();
	DrawLinkState(Ant);

	// Level v2: where you are, by the name of the district (and the nearest callout): what you say to the others.
	if (KitchenArt::Enabled())
	{
		const FVector Here = Ant->GetActorLocation();
		const TCHAR* District = LevelV2::DistrictAt(Here);
		const TCHAR* Callout = LevelV2::CalloutNear(Here, 700.f);
		if (District || Callout)
		{
			const FString Place = (District && Callout && FCString::Strcmp(District, Callout) != 0) ? FString::Printf(TEXT("%s  ·  %s"), District, Callout) : FString(District ? District : Callout);
			Txt(Place, 30.f * S, 14.f * S, FLinearColor(0.85f, 0.85f, 0.8f, 0.8f), Small, 1.1f * S);   // (top left: the middle belongs to the clock)
		}
	}

	// Health + cargo (bottom left)
	const float X = 30.f * S, Y = H - 140.f * S;
	const float HFrac = Ant->Health / FMath::Max(1.f, Ant->MaxHealth);
	Txt(FString::Printf(TEXT("ЗДОРОВЬЕ %d"), FMath::CeilToInt(Ant->Health)), X, Y - 28.f * S, White, Small, 1.2f * S);
	Bar(X, Y, 340.f * S, 24.f * S, HFrac, FMath::Lerp(Red, Green, HFrac));
	if (Ant->Creature != 2)
	{
		// Acid: what you have to spit with.
		const float AFrac = Ant->Acid / AAntCharacter::MaxAcid;
		Bar(X, Y + 26.f * S, 340.f * S, 7.f * S, AFrac, AFrac < 0.2f ? Red : FLinearColor(0.55f, 1.f, 0.3f));
	}

	const FAntClassDef& Cls = Ant->GetClassDef();
	if (AKitchenLoot* C = Ant->Carrying)
	{
		const float R = C->CarryRatio();
		const bool bTug = C->CarryTeam() != Ant->Team;
		const FString State = bTug ? TEXT("ПЕРЕТЯГИВАНИЕ — враги сильнее!") : R >= 1.f ? TEXT("несём") : R >= 0.5f ? TEXT("волочём, тяжело") : TEXT("НЕ СДВИНУТЬ — нужна помощь");
		Txt(FString::Printf(TEXT("ТАЩИШЬ: %s (+%d)  вес %d, держат %d  — %s"), *C->LootName, C->Value, C->Weight, C->NumCarriers(), *State), X, Y + 34.f * S, bTug || R < 0.5f ? Red : Gold, Small, 1.2f * S);
		Bar(X, Y + 62.f * S, 340.f * S, 14.f * S, bTug ? 0.f : FMath::Min(1.f, R), R >= 1.f ? Gold : Red);
		Marker(AKitchenBuilder::NestLocation(Ant->Team) + FVector(0.f, 0.f, 200.f), TEXT("МУРАВЕЙНИК"), Gold, true);
		if (Ant->Team == 0 && KitchenArt::Enabled() && (AKitchenBuilder::Region(Ant->GetActorLocation()) == 2 || Ant->GetActorLocation().X > 7100.f))
		{
			// Level v2: from the living room and the yard the short way home is the south entrance (out through the cat flap).
			Marker(LevelV2::SouthEntrance() + FVector(0.f, 0.f, 200.f), TEXT("ЮЖНЫЙ ВХОД"), Gold, true);
		}
	}
	else
	{
		Txt(FString::Printf(TEXT("%s — сила %.0f"), Cls.Name, Cls.Strength), X, Y + 34.f * S, Gold, Small, 1.2f * S);
	}
	// Commander: army + squad. (A raid: the squad's own panel, AntHUDRaid.cpp.)
	if (Ant->Creature == 0 && !AAntRaid::Get(GetWorld()))
	{
		const AAntArmy* Army = nullptr;
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
		{
			if (It->Commander == Ant) { Army = *It; }
			else if (It->Team != Ant->Team)
			{
				Marker(It->GetActorLocation() + FVector(0.f, 0.f, 120.f), FString::Printf(TEXT("ВРАЖЕСКАЯ АРМИЯ %d"), It->Count), FLinearColor(1.f, 0.3f, 0.15f), true);
			}
		}
		const FString ArmyLine = Army
			? FString::Printf(TEXT("АРМИЯ: %d — %s   |  R — в атаку на прицел / ко мне   F — распустить"), Army->Count,
				Army->Mode == EArmyMode::Follow ? TEXT("за тобой") : Army->Mode == EArmyMode::Attack ? TEXT("АТАКУЕТ") : TEXT("домой"))
			: TEXT("F — вывести армию рабочих из муравейника   |   Z — отряд за мной   X — отряд в атаку на прицел   C — отряд свободен");
		// The hint about F / Z / X / C fades with the rest of the controls; a live army always shows.
		const float ArmyA = Army ? 1.f : HelpAlpha();
		if (ArmyA > 0.01f)
		{
			FLinearColor AC = Army ? FLinearColor(1.f, 0.8f, 0.3f) : Grey;
			AC.A = ArmyA;
			Txt(ArmyLine, X, Y - 112.f * S, AC, Small, 1.1f * S);
		}
		if (Army && Army->Mode == EArmyMode::Attack)
		{
			Marker(FVector(Army->Target) + FVector(0.f, 0.f, 80.f), TEXT("цель армии"), FLinearColor(1.f, 0.8f, 0.3f), true);
		}
	}

	if (Ant->IsInNest())
	{
		Txt(TEXT("В муравейнике безопасно.  Каста: 1 Фуражир (сила)  2 Солдат (броня, урон)  3 Разведчик (скорость, прыжок)  4 Бочка (лечит своих)"), X, Y + 84.f * S, Green, Small, 1.1f * S);
	}

	// Weapon card (bottom right)
	const FAntWeapon& Wp = Ant->Weapon;
	const FLinearColor RC = AntKit::RarityColor(Wp.Rarity);
	const float CW = 420.f * S, CH = 170.f * S;
	const float CX = W - CW - 30.f * S, CY = H - CH - 30.f * S;
	DrawRect(Panel, CX, CY, CW, CH);
	DrawRect(RC, CX, CY, 6.f * S, CH);
	Txt(Wp.Name, CX + 18.f * S, CY + 10.f * S, RC, Med, 1.f * S);
	Txt(FString::Printf(TEXT("Урон: %.0f%s"), Wp.Damage, Wp.Pellets > 1 ? *FString::Printf(TEXT(" x%d"), Wp.Pellets) : TEXT("")), CX + 18.f * S, CY + 50.f * S, White, Small, 1.15f * S);
	Txt(FString::Printf(TEXT("Скорострельность: %.1f /с"), Wp.FireRate), CX + 18.f * S, CY + 76.f * S, White, Small, 1.15f * S);
	Txt(FString::Printf(TEXT("DPS: %.0f"), AntKit::WeaponDps(Wp)), CX + 18.f * S, CY + 102.f * S, Gold, Small, 1.15f * S);
	FString Traits;
	if (Wp.bBounce) Traits += TEXT("Рикошет  ");
	if (Wp.Splash > 0.f) Traits += FString::Printf(TEXT("Взрыв %.0f  "), Wp.Splash);
	if (Wp.Gravity > 0.6f) Traits += TEXT("Навесом  ");
	if (!Traits.IsEmpty())
	{
		Txt(Traits, CX + 18.f * S, CY + 130.f * S, RC, Small, 1.1f * S);
	}

	// Status effects: temperature, honey, water
	{
		const float T = Ant->Temperature;
		if (T < 10.f)
		{
			const float A = FMath::Clamp((10.f - T) / 25.f, 0.f, 1.f);
			DrawRect(FLinearColor(0.5f, 0.8f, 1.f, 0.28f * A), 0.f, 0.f, W, H);
		}
		else if (T > 40.f)
		{
			const float A = FMath::Clamp((T - 40.f) / 60.f, 0.f, 1.f);
			DrawRect(FLinearColor(1.f, 0.35f, 0.05f, 0.25f * A), 0.f, 0.f, W, H);
		}
		FString St = FString::Printf(TEXT("%.0f°C"), T);
		FLinearColor SC = White;
		const bool bPlain = T >= 12.f && T <= 36.f && !(Ant->StatusFlags & (AntStatus::Sticky | AntStatus::Wet | AntStatus::Drowning)) && !Ant->IsBurning();
		if (bPlain) { St = FString(); }   // room temperature and nothing on you: nothing to say
		if (Ant->IsFrozen()) { St += TEXT("  ЗАМЁРЗ! медленно, не прыгнуть, не залезть на стену"); SC = FLinearColor(0.6f, 0.85f, 1.f); }
		else if (T < 8.f) { St += TEXT("  холодно: медленнее бегаешь и стреляешь"); SC = FLinearColor(0.6f, 0.85f, 1.f); }
		else if (T > 55.f) { St += TEXT("  ГОРИШЬ!"); SC = Red; }
		else if (T > 40.f) { St += TEXT("  жарко"); SC = Gold; }
		if (Ant->HasStatus(AntStatus::Sticky)) { St += TEXT("  | липкий: медленно, без прыжков"); SC = Gold; }
		if (Ant->HasStatus(AntStatus::Wet)) { St += TEXT("  | мокрый"); }
		if (Ant->HasStatus(AntStatus::Drowning)) { St += TEXT("  | ТОНЕШЬ!"); SC = Red; }
		if (Ant->IsBurning()) { St += TEXT("  | ГОРИШЬ! в воду / поджигай врагов"); SC = FLinearColor(1.f, 0.5f, 0.1f); }
		Txt(St, X, Y - 56.f * S, SC, Small, 1.15f * S);
	}

	// Colony swarm: only while it works for you (a trail is laid)
	if (const AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()); Swarm && Swarm->Streams.Num() > 0)
	{
		Txt(FString::Printf(TEXT("КОЛОНИЯ: %d рабочих  |  в гнезде %d  |  на тропах %d  (%d троп)"),
			Swarm->Population, Swarm->Idle, Swarm->AntsOnTrails(), Swarm->Streams.Num()),
			X, Y - 84.f * S, FLinearColor(0.55f, 1.f, 0.6f), Small, 1.1f * S);
	}
	if (Ant->bLayingTrail)
	{
		Txt(FString::Printf(TEXT("ФЕРОМОНОВАЯ ТРОПА: %d / %d — ползи в муравейник  (T — отменить)"), Ant->TrailPoints.Num(), AAntCharacter::TrailMaxPoints),
			W * 0.5f, H * 0.5f + 175.f * S, FLinearColor(0.4f, 1.f, 0.45f), Med, 1.f * S, true);
		Marker(AKitchenBuilder::NestLocation() + FVector(0.f, 0.f, 200.f), TEXT("МУРАВЕЙНИК"), FLinearColor(0.4f, 1.f, 0.45f), true);
	}

	// Interaction prompt: gadgets win over loot when closer.
	float GadgetDist = 0.f;
	AKitchenInteractable* Gadget = Ant->FindNearestInteractable(&GadgetDist);
	AKitchenLoot* NearLoot = Ant->FindNearestLoot(AAntCharacter::InteractRadius);
	if (Ant->Pulling && Ant->Pulling->PullMissing > 0)
	{
		// Straining at a door that will not give: say how many more it takes.
		const float Beat = 0.75f + 0.25f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f));
		Txt(FString::Printf(TEXT("%s  ·  Нужно ещё %d"), Ant->Pulling->IsA<AKitchenSpillBottle>() ? TEXT("БУТЫЛКА НЕ ПОДДАЁТСЯ") : TEXT("ДВЕРЬ НЕ ПОДДАЁТСЯ"), (int32)Ant->Pulling->PullMissing), W * 0.5f, H * 0.5f + 70.f * S, FLinearColor(1.f, 0.55f, 0.2f, Beat), Med, 1.2f * S, true);
		Txt(TEXT("Держи [E]. Зови своих, выведи армию (F) или переоденься в Солдата"), W * 0.5f, H * 0.5f + 106.f * S, Grey, Small, 1.1f * S, true);
	}
	else if (Gadget && (!NearLoot || GadgetDist < FVector::Dist(NearLoot->GetActorLocation(), Ant->GetActorLocation())))
	{
		const int32 DoorClass = Gadget->DoorClass();
		const int32 Missing = (DoorClass > 0 && !Gadget->IsDoorOpen()) ? AntDoor::Missing(Gadget->PullNeed(), Ant->GetStrength()) : 0;
		if (Missing > 0)
		{
			Txt(FString::Printf(TEXT("[E] держать — %s"), *Gadget->GetPrompt(Ant)), W * 0.5f, H * 0.5f + 70.f * S, Gold, Med, 1.05f * S, true);
			Txt(FString::Printf(TEXT("%s, нужно ещё %d"), Gadget->IsA<AKitchenSpillBottle>() ? TEXT("Тяжёлая бутылка: одному не опрокинуть") : DoorClass >= AntDoor::Heavy ? TEXT("Тяжёлая дверь: одному не открыть") : TEXT("Тугая дверца: одному не открыть"), Missing),
				W * 0.5f, H * 0.5f + 104.f * S, Gold, Small, 1.1f * S, true);
		}
		else
		{
			Txt(FString::Printf(TEXT("[E] %s"), *Gadget->GetPrompt(Ant)), W * 0.5f, H * 0.5f + 70.f * S, FLinearColor(0.6f, 1.f, 0.9f), Med, 1.05f * S, true);
		}
	}
	else if (AKitchenLoot* L = NearLoot)
	{
		const float PX = W * 0.5f, PY = H * 0.5f + 70.f * S;
		if (L->bIsGear)
		{
			const float NewDps = AntKit::WeaponDps(L->Gear);
			const float OldDps = AntKit::WeaponDps(Wp);
			Txt(FString::Printf(TEXT("[E] Взять: %s"), *L->Gear.Name), PX, PY, AntKit::RarityColor(L->Gear.Rarity), Med, 1.05f * S, true);
			Txt(FString::Printf(TEXT("DPS %.0f  (%s%.0f)"), NewDps, NewDps >= OldDps ? TEXT("+") : TEXT(""), NewDps - OldDps), PX, PY + 34.f * S, NewDps >= OldDps ? Green : Red, Small, 1.2f * S, true);
		}
		else
		{
			const int32 Need = L->AntsNeeded(Ant->GetStrength());
			const bool bMine = Ant->Carrying == L;
			const FString What = bMine ? FString::Printf(TEXT("[E] Отпустить: %s"), *L->LootName)
				: FString::Printf(TEXT("[E] Схватить: %s  +%d  (вес %d)"), *L->LootName, L->Value, L->Weight);
			Txt(What, PX, PY, Need <= 1 ? White : Gold, Med, 1.05f * S, true);
			if (!bMine)
			{
				const FString Hint = L->IsCarried()
					? (L->CarryTeam() == Ant->Team ? FString::Printf(TEXT("Свои тащат (%d) — помоги!"), L->NumCarriers()) : TEXT("Это тащат ЧУЖИЕ — хватай и тяни к себе!"))
					: Need <= 1 ? TEXT("Унесёшь один") : FString::Printf(TEXT("Тяжело: нужно %d таких муравья (один — только волочить или никак)"), Need);
				Txt(Hint, PX, PY + 34.f * S, Need <= 1 ? Green : Gold, Small, 1.1f * S, true);
			}
			if (!Ant->bLayingTrail)
			{
				Txt(TEXT("[T] проложить феромоновую тропу — рой будет таскать это сам"), PX, PY + 62.f * S, FLinearColor(0.5f, 1.f, 0.55f), Small, 1.05f * S, true);
			}
		}
	}

	if (Ant->IsClimbing())
	{
		Txt(Ant->GetVisualUp().Z < -0.5f ? TEXT("~ ползёшь по потолку ~  (Пробел — отцепиться)") : TEXT("~ ползёшь по стене ~  (Пробел — оттолкнуться)"), W * 0.5f, H * 0.5f + 140.f * S, Grey, Small, 1.1f * S, true);
	}

	// Rival nest marker
	Marker(AKitchenBuilder::NestLocation(1 - Ant->Team) + FVector(0.f, 0.f, 300.f), Ant->Team == 0 ? TEXT("гнездо огненных") : TEXT("гнездо чёрных"), FLinearColor(1.f, 0.4f, 0.2f), false);

	// Loot radar: gear and juicy food nearby
	const FVector Me = Ant->GetActorLocation();
	int32 Count = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It && Count < 12; ++It)
	{
		AKitchenLoot* L = *It;
		const float D = FVector::Dist(L->GetActorLocation(), Me);
		if (D > 5000.f || D < AAntCharacter::InteractRadius || L->bTrophy)
		{
			continue;
		}
		// Labels only for what is close or what you look at: the screen stays clean.
		const FVector LabelAt = L->GetActorLocation() + FVector(0.f, 0.f, 40.f + (L->bIsGear ? 0.f : L->GetRadius()));
		if (D > 650.f && !Aimed(LabelAt, 3.5f))
		{
			continue;
		}
		if (L->bIsGear)
		{
			Marker(LabelAt, AntKit::RarityName(L->Gear.Rarity), AntKit::RarityColor(L->Gear.Rarity), false);
			++Count;
		}
		else if (L->bBody)
		{
			Marker(LabelAt, FString::Printf(TEXT("%s — донеси в гнездо: вернёт личинку"), *L->LootName), FLinearColor(0.75f, 0.9f, 1.f), false);
			++Count;
		}
		else if (L->Value >= 40)
		{
			const int32 Sealed = KitchenContainers::SealedAt(GetWorld(), L->GetActorLocation());
			FString Label = L->IsCarried() ? FString::Printf(TEXT("+%d (тащат %d)"), L->Value, L->NumCarriers()) : FString::Printf(TEXT("+%d  x%d"), L->Value, L->AntsNeeded(Ant->GetStrength()));
			if (Sealed >= 0) { Label += FString::Printf(TEXT("  за дверью (%s)"), KitchenContainers::Get(Sealed).Where); }
			Marker(LabelAt, Label, L->IsCarried() && L->CarryTeam() != Ant->Team ? FLinearColor(1.f, 0.4f, 0.2f) : Gold, false);
			++Count;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Human view
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawHuman(AHumanCharacter* H, AAntGameState* GS)
{
	const float W = Canvas->ClipX, Hh = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Big = GEngine->GetLargeFont();

	Crosshair();

	const float X = 30.f * S, Y = Hh - 170.f * S;
	Txt(TEXT("ТЫ — ЧЕЛОВЕК"), X, Y - 40.f * S, Red, GEngine->GetMediumFont(), 1.2f * S);
	Txt(FString::Printf(TEXT("Нервы: %d / %d"), FMath::CeilToInt(H->Health), FMath::CeilToInt(H->MaxHealth)), X, Y, White, Small, 1.2f * S);
	Bar(X, Y + 26.f * S, 380.f * S, 22.f * S, H->Health / FMath::Max(1.f, H->MaxHealth), Red);
	Txt(TEXT("Дихлофос"), X, Y + 56.f * S, White, Small, 1.2f * S);
	Bar(X, Y + 82.f * S, 380.f * S, 14.f * S, H->SprayFuel / 100.f, FLinearColor(0.5f, 0.9f, 0.3f));

	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const bool bStompReady = ServerNow >= H->StompReadyTime;
	// The giant's keys (HumanPhys.cpp).
	const bool bSwatReady = ServerNow >= H->SwatReadyTime;
	if (!AntUi::Classic())
	{
		// The front end's style (docs/UI_STYLE.md): a glass card of keys on the right, teal for the Human's moves, greyed while it winds up.
		const bool bKickReady = ServerNow >= H->KickReadyTime;
		struct FRow { const TCHAR* Key; FString What; bool bReady; };
		const FRow Rows[] =
		{
			{ TEXT("ЛКМ"), H->bCharging ? FString(TEXT("ЗАМАХ... отпусти — сильный удар")) : FString(TEXT("удар · держать — сильный · в ритм — серия")), bSwatReady || H->bCharging },
			{ TEXT("ПРОБЕЛ"), FString(TEXT("топнуть · держать — прыжок двумя ногами")), bStompReady },
			{ TEXT("CTRL / C"), FString(TEXT("пинок")), bKickReady },
			{ TEXT("Q"), FString(H->Held ? TEXT("швырнуть еду") : TEXT("бросить оружие")), true },
			{ TEXT("E"), FString(TEXT("взять еду / оружие")), true },
			{ TEXT("SHIFT"), H->SprintRamp > 0.05f ? FString::Printf(TEXT("бег %d%%"), FMath::RoundToInt(H->SprintRamp * 100.f)) : FString(TEXT("бег")), true },
			{ TEXT("ПКМ"), FString(TEXT("дихлофос")), true },
			{ TEXT("F"), FString(TEXT("стряхнуть муравьёв")), true },
		};
		const int32 N = UE_ARRAY_COUNT(Rows);
		const float RowH = 26.f * S, PW = 500.f * S, PH = RowH * N + 52.f * S;
		const float PX = W - PW - 28.f * S, PY = Hh - PH - 120.f * S;
		AntUiCanvas::Glass(Canvas, PX, PY, PW, PH, 0.72f);
		AntUiCanvas::Text(Canvas, TEXT("ТЯЖЁЛОЕ ТЕЛО"), PX + 16.f * S, PY + 10.f * S, AntUi::EFont::Display, 17.f * S, AntUi::C::Amber);
		for (int32 i = 0; i < N; ++i)
		{
			const float RY = PY + 42.f * S + i * RowH;
			const FLinearColor KeyC = Rows[i].bReady ? AntUi::C::Teal : AntUi::C::Muted;
			AntUiCanvas::Text(Canvas, Rows[i].Key, PX + 16.f * S, RY, AntUi::EFont::Bold, 14.f * S, KeyC);
			AntUiCanvas::TextFit(Canvas, Rows[i].What, PX + 130.f * S, RY, PW - 146.f * S, AntUi::EFont::Body, 14.f * S, Rows[i].bReady ? AntUi::C::Sugar : AntUi::C::Muted);
		}
	}
	else
	{
	Txt(H->bCharging ? TEXT("ЛКМ — ЗАМАХ... отпусти: СИЛЬНЫЙ УДАР") : bSwatReady ? TEXT("ЛКМ — удар (держать — сильный, в ритм — серия из 3)") : TEXT("ЛКМ — удар: замах..."), X, Y + 106.f * S,
		H->bCharging ? Gold : bSwatReady ? Green : Grey, Small, 1.2f * S);
	Txt(bStompReady ? TEXT("ПРОБЕЛ — топнуть (держать — прыжок двумя ногами)  ·  CTRL/C — пинок") : TEXT("ПРОБЕЛ — топнуть: замах...  ·  CTRL/C — пинок"), X, Y + 130.f * S, bStompReady ? Green : Grey, Small, 1.2f * S);
	Txt(H->Held ? TEXT("Q / E — ШВЫРНУТЬ еду (разобьётся!)  ·  SHIFT — бег") : TEXT("E — взять еду / оружие  ·  Q — бросить оружие  ·  SHIFT — бег  ·  F — стряхнуть"), X, Y + 154.f * S, H->Held ? Gold : White, Small, 1.2f * S);
	}
	{
		// The weapon in hand, and in versus his patience and the x-ray (AntVersus.h).
		float Pat = -1.f;
		Txt(AntVersus::HudLine(GetWorld(), H, Pat), X, Y + 178.f * S, Gold, Small, 1.2f * S);
		if (Pat >= 0.f)
		{
			Txt(TEXT("Терпение"), X + 420.f * S, Y + 56.f * S, White, Small, 1.2f * S);
			Bar(X + 420.f * S, Y + 82.f * S, 260.f * S, 14.f * S, Pat, FLinearColor(1.f, 0.55f, 0.2f));
		}
	}
	// Where the slipper will land: the stomp goes where he aims (clamped to where a foot gets).
	if (PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
		const FVector Dir = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(HudStompAim), false, H);
		const FVector Aim = GetWorld()->LineTraceSingleByChannel(Hit, Cam, Cam + Dir * 9000.f, ECC_Visibility, Q) ? FVector(Hit.ImpactPoint) : Cam + Dir * 2000.f;
		int32 Leg = 0;
		Marker(H->StompAimFrom(Aim, Leg) + FVector(0.f, 0.f, 20.f), TEXT("тапок"), bStompReady ? FLinearColor(1.f, 0.5f, 0.2f) : Grey, false);
	}
	if (H->IsStaggered())
	{
		Txt(TEXT("ВРЕЗАЛСЯ!"), W * 0.5f, Hh * 0.42f, Gold, Big, 1.4f * S, true);
	}
	if (H->SprintRamp > 0.05f)
	{
		Txt(FString::Printf(TEXT("БЕГ %d%%"), FMath::RoundToInt(H->SprintRamp * 100.f)), X + 420.f * S, Y + 106.f * S, FLinearColor(1.f, 0.7f, 0.3f), Small, 1.2f * S);
	}

	if (H->IsKnockedOut())
	{
		Txt(TEXT("ТЫ В ПАНИКЕ! Муравьи тебя довели..."), W * 0.5f, Hh * 0.4f, Red, Big, 1.6f * S, true);
	}

	// Smell the sugar: ants carrying food are highlighted.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Ant->IsCarrying() && FVector::Dist(Ant->GetActorLocation(), H->GetActorLocation()) < 7000.f && AntVersus::XrayOn(GetWorld()))
		{
			Marker(Ant->GetActorLocation() + FVector(0.f, 0.f, 60.f), TEXT("сладкое!"), FLinearColor(1.f, 0.6f, 0.1f), false);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Shared
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawWorldTags(APawn* Me)
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	UFont* Small = GEngine->GetSmallFont();

	auto InFront = [&Cam, &CamRot](const FVector& P) { return CamRot.UnrotateVector(P - Cam).X > 1.f; };

	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant == Me || Ant->bDead)
		{
			continue;
		}
		const FVector P = Ant->GetActorLocation() + FVector(0.f, 0.f, 55.f);
		if (FVector::Dist(P, Cam) > 3000.f || !InFront(P))
		{
			continue;
		}
		const FVector S2 = Project(P);
		const AAntCharacter* MyAnt = Cast<AAntCharacter>(Me);
		const bool bEnemy = MyAnt ? MyAnt->IsEnemy(Ant) : Ant->Team != 0;
		if (!bEnemy && !Ant->IsPlayerControlled())
		{
			// A friendly bot / squad ant: its name only when the crosshair is on it or it is selected (a squad of 30 «Муравей» tags hid the kitchen).
			const UAntRtsInput* Rts = UAntRtsInput::Of(PlayerOwner);
			const bool bSelected = Rts && Rts->Selection.Contains(TWeakObjectPtr<AAntCharacter>(Ant));
			if (!bSelected && !Aimed(P, 2.5f))
			{
				continue;
			}
		}
		Txt(Ant->GetDisplayName(), S2.X, S2.Y - 18.f * S, bEnemy ? FLinearColor(1.f, 0.4f, 0.25f) : FLinearColor(0.8f, 0.9f, 1.f), Small, 1.f * S, true);
		if (bEnemy)
		{
			Bar(S2.X - 40.f * S, S2.Y + 2.f * S, 80.f * S, 6.f * S, Ant->Health / FMath::Max(1.f, Ant->MaxHealth), FLinearColor(1.f, 0.3f, 0.2f));
		}
	}

	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* Hu = *It;
		if (!Hu || Hu == Me)
		{
			continue;
		}
		const FVector P = Hu->GetActorLocation() + FVector(0.f, 0.f, 1900.f);
		if (!InFront(P))
		{
			continue;
		}
		const FVector S2 = Project(P);
		if (Hu->IsA<AHouseFolk>())
		{
			// Grandma and the kid have no "nerves" to wear down: a name, no bar (it used to hang there always full).
			Txt(Hu->GetDisplayName(), S2.X, S2.Y - 40.f * S, FLinearColor(1.f, 0.75f, 0.55f), Small, 1.1f * S, true);
			continue;
		}
		Txt(Hu->GetDisplayName(), S2.X, S2.Y - 40.f * S, Red, Small, 1.2f * S, true);
		Bar(S2.X - 120.f * S, S2.Y - 12.f * S, 240.f * S, 14.f * S, Hu->Health / FMath::Max(1.f, Hu->MaxHealth), Red);
		if (Hu->IsKnockedOut())
		{
			Txt(TEXT("В ПАНИКЕ!"), S2.X, S2.Y + 6.f * S, Gold, Small, 1.2f * S, true);
		}
	}
}

void AAntHUD::DrawToastsAndNumbers(AAntPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Y = Canvas->ClipY * 0.66f;
	for (const FAntToast& T : PC->Toasts)
	{
		const float Age = Now - T.Time;
		if (Age > 3.f)
		{
			continue;
		}
		Txt(T.Text, Canvas->ClipX * 0.5f, Y, FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(3.f - Age, 0.f, 1.f)), GEngine->GetMediumFont(), 1.f * S, true);
		Y += 32.f * S;
	}

	if (!PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	for (const FAntDamageNumber& N : PC->DamageNumbers)
	{
		const float Age = Now - N.Time;
		if (Age > 1.2f)
		{
			continue;
		}
		const FVector World = N.Location + FVector(0.f, 0.f, 40.f + Age * 120.f);
		if (CamRot.UnrotateVector(World - Cam).X <= 1.f)
		{
			continue;
		}
		const FVector P = Project(World);
		const FLinearColor C = N.bCrit ? FLinearColor(1.f, 0.9f, 0.1f, 1.2f - Age) : FLinearColor(1.f, 1.f, 1.f, 1.2f - Age);
		const FString Str = N.bCrit ? FString::Printf(TEXT("%d КРИТ!"), N.Amount) : FString::FromInt(N.Amount);
		Txt(Str, P.X, P.Y, C, GEngine->GetMediumFont(), (N.bCrit ? 1.4f : 1.0f) * S, true);
	}
}

void AAntHUD::DrawScoreboard(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	UFont* Small = GEngine->GetSmallFont();
	const float W = 760.f * S;
	const float X = Canvas->ClipX * 0.5f - W * 0.5f;
	float Y = Canvas->ClipY * 0.22f;
	const float RowH = 30.f * S;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X, Y, W, RowH * (GS->PlayerArray.Num() + 2) + 20.f * S);

	Y += 10.f * S;
	Txt(TEXT("Игрок"), X + 20.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Роль"), X + 300.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Принёс"), X + 430.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Смерти"), X + 550.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Давил"), X + 660.f * S, Y, Gold, Small, 1.2f * S);
	Y += RowH;

	for (APlayerState* Base : GS->PlayerArray)
	{
		AAntPlayerState* PS = Cast<AAntPlayerState>(Base);
		if (!PS)
		{
			continue;
		}
		DrawRect(PS->AntColor, X + 6.f * S, Y + 6.f * S, 8.f * S, 16.f * S);
		Txt(PS->GetPlayerName(), X + 20.f * S, Y, White, Small, 1.15f * S);
		Txt(PS->bIsHuman ? TEXT("Человек") : PS->Team == 0 ? AntKit::ClassDef(PS->AntClass).Name : *FString::Printf(TEXT("огн. %s"), AntKit::ClassDef(PS->AntClass).Name),
			X + 300.f * S, Y, PS->bIsHuman ? Red : PS->Team == 0 ? Green : FLinearColor(1.f, 0.4f, 0.2f), Small, 1.15f * S);
		Txt(FString::FromInt(PS->Delivered), X + 430.f * S, Y, White, Small, 1.15f * S);
		Txt(FString::FromInt(PS->Deaths), X + 550.f * S, Y, White, Small, 1.15f * S);
		Txt(FString::FromInt(PS->Kills), X + 660.f * S, Y, White, Small, 1.15f * S);
		Y += RowH;
	}
}

void AAntHUD::DrawHelp(APawn* Me)
{
	FString Help;
	if (Cast<AHumanCharacter>(Me))
	{
		Help = TEXT("WASD — ходить | Shift — бег | ЛКМ — удар (держать — сильный, в ритм — серия) | Пробел — топнуть (держать — прыжок) | Ctrl/C — пинок | ПКМ — дихлофос | E — взять | Q — бросить | F — стряхнуть | H — снова муравей");
	}
	else if (const AAntCharacter* C = Cast<AAntCharacter>(Me); C && C->Creature == 1)
	{
		Help = TEXT("МУХА: WASD — лететь | Shift — ФОРСАЖ | Пробел — вверх | E — съесть (какашка/мусор), схватить крошку или поднять муравья | ЛКМ — плевок | ПКМ — укус | J — снова муравей");
	}
	else if (const AAntCharacter* C2 = Cast<AAntCharacter>(Me); C2 && C2->Creature == 2)
	{
		Help = TEXT("ПАУК: по стенам и потолку | ПКМ (держать) — полёт на паутине, отпустить — бросок | Пробел — подтянуться | ЛКМ — паутина | Q — укус | K — снова муравей");
	}
	else if (AAntRaid::Get(GetWorld()))
	{
		Help = TEXT("ЛКМ — плевок | ПКМ — укус | Ctrl — РЫВОК | E — взять/отпустить, держать — тянуть дверь | ОТРЯД: Z за мной · X фас · C вольно · Q нести · B держать · G мост | L у входа (держать) — уйти домой | СКМ — метка | Tab счёт");
	}
	else
	{
		Help = TEXT("ЛКМ — плевок (кислота кончается — жди) | ПКМ — укус | Ctrl — РЫВОК | в спину — крит | E — взять/отпустить | G — сцепка | F/R — армия | Y — рой | T — тропа | P — сторона | J муха | K паук | H Человек | Tab счёт");
	}
	// The long line is for the first minute; after that F1 brings it back for a while.
	const float A = HelpAlpha();
	if (A > 0.01f)
	{
		Txt(Help, Canvas->ClipX * 0.5f, Canvas->ClipY - 26.f * S, FLinearColor(1.f, 1.f, 1.f, 0.6f * A), GEngine->GetSmallFont(), 1.f * S, true);
	}
	else
	{
		Txt(TEXT("F1 — управление"), Canvas->ClipX * 0.5f, Canvas->ClipY - 26.f * S, FLinearColor(1.f, 1.f, 1.f, 0.28f), GEngine->GetSmallFont(), 0.95f * S, true);
	}
}

float AAntHUD::HelpAlpha() const
{
	const float Now = GetWorld()->GetTimeSeconds();
	const AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	const float SinceAsked = PC ? Now - PC->HelpAskedAt : 1.e6f;
	const float SinceStart = FirstDrawTime >= 0.f ? Now - FirstDrawTime : 0.f;
	const float A1 = FMath::Clamp((65.f - SinceStart) / 5.f, 0.f, 1.f);
	const float A2 = FMath::Clamp((20.f - SinceAsked) / 3.f, 0.f, 1.f);
	return FMath::Max(A1, A2);
}

bool AAntHUD::Aimed(const FVector& World, float Degrees) const
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return false;
	}
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	return FVector::DotProduct((World - Cam).GetSafeNormal(), Fwd) > FMath::Cos(FMath::DegreesToRadians(Degrees));
}

void AAntHUD::DrawLinkState(AAntCharacter* Ant)
{
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	FString Line;
	FLinearColor C = Gold;
	if (Ant->HeldBy)
	{
		Line = Ant->HeldBy->Creature == 1 ? TEXT("ТЕБЯ НЕСЁТ МУХА — G: вырваться") : TEXT("Тебя держат — G: вырваться");
	}
	else if (AAntChain* Ch = Ant->Chain)
	{
		const int32 I = Ch->IndexOf(Ant);
		Line = Ch->bBridge
			? FString::Printf(TEXT("ТЫ ЗВЕНО МОСТА (%d из %d)%s — G/Пробел: отпустить"), I + 1, Ch->NumLinks(), Ch->IsComplete() ? TEXT(", мост готов") : TEXT(", нужны ещё муравьи"))
			: FString::Printf(TEXT("ТЫ ЗВЕНО ЛЕСТНИЦЫ (%d из %d) — G/Пробел: отпустить"), I + 1, Ch->NumLinks());
	}
	else if (Ant->HoldingAnt)
	{
		Line = FString::Printf(TEXT("ДЕРЖИШЬ: %s — G: отпустить, E: бросить"), *Ant->HoldingAnt->GetHumanReadableName());
	}
	else if (Ant->Creature == 0)
	{
		// What G would do right now (only when it is something to notice).
		if (AAntChain* Tail = AAntChain::FindNearTail(GetWorld(), Ant->Team, Ant->GetActorLocation(), 190.f * AAntCharacter::AntShrink))
		{
			Line = TEXT("G — прицепиться к живой цепи");
		}
		else if (Ant->FindGrabbableAlly(AAntCharacter::InteractRadius))
		{
			Line = TEXT("G — взять муравья");
			C = White;
		}
	}
	else if (Ant->Creature == 1 && Ant->FindGrabbableAlly(AAntCharacter::InteractRadius * 1.5f))
	{
		Line = TEXT("E — поднять муравья");
		C = White;
	}
	if (!Line.IsEmpty())
	{
		Txt(Line, W * 0.5f, H * 0.62f, C, Small, 1.25f * S, true);
	}
	// Fly afterburner.
	if (const AFlyCharacter* Fly = Cast<AFlyCharacter>(Ant))
	{
		const float X = 30.f * S, Y = H - 196.f * S;
		Txt(Fly->IsBoosting() ? TEXT("ФОРСАЖ!") : TEXT("ФОРСАЖ (Shift)"), X, Y - 24.f * S, Fly->Fuel < 20.f ? Red : White, Small, 1.1f * S);
		Bar(X, Y, 240.f * S, 12.f * S, Fly->Fuel / 100.f, FLinearColor(0.3f, 0.8f, 1.f));
		if (Fly->Fuel < 25.f || Fly->Health < Fly->MaxHealth * 0.4f)
		{
			Txt(TEXT("Голодно: лети к какашке во дворе или к мусорке (E)"), X, Y + 18.f * S, Gold, Small, 1.f * S);
		}
	}
}

void AAntHUD::DrawWar(AAntGameState* GS, APawn* Me)
{
	if (!GS || !GS->bWarMode)
	{
		return;
	}
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->ClipX;
	const FLinearColor Ours(0.35f, 0.75f, 1.f), Fire(1.f, 0.4f, 0.2f), Neutral(0.75f, 0.75f, 0.75f);
	const AAntCharacter* Ant = Cast<AAntCharacter>(Me);
	const uint8 MyTeam = Ant ? Ant->Team : 0;
	auto TeamC = [&](uint8 T) { return T == 0 ? Ours : T == 1 ? Fire : Neutral; };

	// Territory strip under the timer.
	TArray<AAntTerritory*> Zones;
	for (TActorIterator<AAntTerritory> It(GetWorld()); It; ++It)
	{
		Zones.Add(*It);
	}
	Zones.Sort([](const AAntTerritory& A, const AAntTerritory& B) { return A.GetActorLocation().X + A.GetActorLocation().Y * 0.1 < B.GetActorLocation().X + B.GetActorLocation().Y * 0.1; });
	const float BoxW = 118.f * S, Gap = 6.f * S;
	float X = W * 0.5f - (Zones.Num() * (BoxW + Gap) - Gap) * 0.5f;
	const float Y = 150.f * S;
	for (AAntTerritory* Z : Zones)
	{
		const FLinearColor C = TeamC(Z->OwnerTeam);
		DrawRect(FLinearColor(C.R * 0.3f, C.G * 0.3f, C.B * 0.3f, 0.75f), X, Y, BoxW, 38.f * S);
		// Capture bar: blue fills from the left, red from the right.
		const float Cap = Z->Capture;
		if (Cap > 0.f) { DrawRect(Ours, X, Y + 32.f * S, BoxW * 0.5f * Cap, 6.f * S); }
		if (Cap < 0.f) { DrawRect(Fire, X + BoxW - BoxW * 0.5f * -Cap, Y + 32.f * S, BoxW * 0.5f * -Cap, 6.f * S); }
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.5f), X + BoxW * 0.5f - 1.f, Y + 30.f * S, 2.f, 10.f * S);
		Txt(Z->ZoneName, X + BoxW * 0.5f, Y + 4.f * S, C, Small, 1.05f * S, true);
		const FString Sub = Z->bContested ? TEXT("БОЙ") : (Z->OwnerTeam < 2 && Z->bLineCut) ? TEXT("снабжение перерезано") : TEXT("");
		if (!Sub.IsEmpty())
		{
			Txt(Sub, X + BoxW * 0.5f, Y + 18.f * S, Gold, Small, 0.85f * S, true);
		}
		X += BoxW + Gap;
	}
	int32 Held[2] = { 0, 0 };
	for (AAntTerritory* Z : Zones)
	{
		if (Z->OwnerTeam < 2) { ++Held[Z->OwnerTeam]; }
	}
	Txt(FString::Printf(TEXT("Резерв армии: наши %d  |  огненные %d     Территории %d : %d"),
		MyTeam == 0 ? GS->WarPool0 : GS->WarPool1, MyTeam == 0 ? GS->WarPool1 : GS->WarPool0, Held[MyTeam], Held[1 - MyTeam]),
		W * 0.5f, Y + 44.f * S, White, Small, 1.05f * S, true);

	if (!Ant || Ant->bDead)
	{
		return;
	}
	// World markers: zones (only when far), the safe, counting cashouts.
	for (AAntTerritory* Z : Zones)
	{
		if (FVector::Dist(Z->GetActorLocation(), Ant->GetActorLocation()) > Z->Radius)
		{
			Marker(Z->GetActorLocation() + FVector(0.f, 0.f, 300.f), Z->ZoneName, TeamC(Z->OwnerTeam), false);
		}
	}
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (It->bVault)
		{
			const FString L = It->IsCarried() ? (It->CarryTeam() == Ant->Team ? TEXT("СЕЙФ — наши несут на кассу") : TEXT("СЕЙФ — ОТБЕЙ!")) : TEXT("СЕЙФ-САХАРНИЦА");
			Marker(It->GetActorLocation() + FVector(0.f, 0.f, 150.f), L, Gold, true);
		}
	}
	const bool bCarryVault = Ant->Carrying && Ant->Carrying->bVault;
	for (TActorIterator<AAntCashout> It(GetWorld()); It; ++It)
	{
		if (It->bActive)
		{
			Marker(It->GetActorLocation() + FVector(0.f, 0.f, 250.f), FString::Printf(TEXT("%s %d%%"), It->CashTeam == Ant->Team ? TEXT("КАССА (держим)") : TEXT("КАССА ВРАГА — укради!"),
				FMath::RoundToInt(It->Progress * 100.f)), TeamC(It->CashTeam), true);
		}
		else if (bCarryVault)
		{
			Marker(It->GetActorLocation() + FVector(0.f, 0.f, 250.f), It->ZoneName, Gold, true);
		}
	}
}

void AAntHUD::DrawTrophies(AAntGameState* GS, APawn* Me)
{
	const AAntCharacter* Ant = Cast<AAntCharacter>(Me);
	if (!GS || !GS->bHeistMode || !Ant || Ant->bDead)
	{
		return;
	}
	const AAntRaid* RaidOfHud = AAntRaid::Get(GetWorld());
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L->bTrophy || L->bHeld)
		{
			continue;
		}
		FString Label;
		const FVector At = L->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		const bool bClose = FVector::Dist(L->GetActorLocation(), Ant->GetActorLocation()) < 1600.f || Aimed(At, 5.f);
		if (RaidOfHud && !bClose && !L->IsCarried() && L->LootName == AAntRaid::CakeName())
		{
			continue;   // a raid: the cake has to be found (once it is, the raid HUD marks it)
		}
		if (L->IsCarried())
		{
			Label = L->CarryTeam() == Ant->Team ? FString::Printf(TEXT("%s — несём (%d)"), *L->LootName, L->NumCarriers()) : FString::Printf(TEXT("%s — ТАЩАТ ВРАГИ!"), *L->LootName);
		}
		else if (bClose)
		{
			const int32 Sealed = KitchenContainers::SealedAt(GetWorld(), L->GetActorLocation());
			Label = FString::Printf(TEXT("ТРОФЕЙ: %s  +%d ×2  нужно %d"), *L->LootName, L->Value, L->AntsNeeded(Ant->GetStrength()));
			if (Sealed >= 0) { Label += FString::Printf(TEXT("  ·  за дверью (%s)"), KitchenContainers::Get(Sealed).Where); }
		}
		// Far away and not looked at: only the small gold mark, no text.
		Marker(At, Label, L->IsCarried() && L->CarryTeam() != Ant->Team ? FLinearColor(1.f, 0.4f, 0.2f) : Gold, L->IsCarried() || bClose);
	}
}
