#include "AntTutorial.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntSnowball.h"
#include "AntTrail.h"
#include "AntRts.h"
#include "AntSquad.h"
#include "AntBotController.h"
#include "AntCarry.h"
#include "AntCloth.h"
#include "AntInside.h"
#include "AntFluidWorld.h"
#include "AntAudio.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntUiMenu.h"
#include "AntSession.h"
#include "AntVersus.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "KitchenFixture.h"
#include "KitchenInteractables.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "LevelV3.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundWaveProcedural.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"

// Branches that are not merged yet light their beats up by themselves once their headers are in the tree.
#if __has_include("AntReact.h")
#define ANT_TUT_CHAINS 1
#include "AntReact.h"
#else
#define ANT_TUT_CHAINS 0
#endif
#if __has_include("AntLife.h")
#define ANT_TUT_LIFE 1
#else
#define ANT_TUT_LIFE 0
#endif
#if __has_include("AntColony.h") && __has_include("AntColonyView.h")
#define ANT_TUT_COLONY 1
#include "AntColony.h"
#include "AntColonyView.h"
#else
#define ANT_TUT_COLONY 0
#endif
#if __has_include("AntFighter.h")
#define ANT_TUT_FIGHTER 1
#include "AntFighter.h"
#else
#define ANT_TUT_FIGHTER 0
#endif
#if __has_include("AntMetaBus.h")
#define ANT_TUT_METABUS 1
#include "AntMetaBus.h"
#else
#define ANT_TUT_METABUS 0
#endif
#ifndef ANT_DEMO
#define ANT_DEMO 0
#endif

using namespace AntUi;

// =====================================================================================================================
// The lines (subtitles now; voiced later — the list is in docs/ONBOARDING.md). Voice: 0 sergeant, 1 radio, 2 Queen.
// =====================================================================================================================
namespace
{
	struct FLine { const TCHAR* Ru; const TCHAR* En; uint8 Voice; };
	enum ELine : int16
	{
		LRadio1, LRadio2, LSerg1, LSerg2,
		LMove, LSprint, LJump, LClimb, LCeiling, LDash, LMovePay,
		LBite, LSpit, LFly, LDrip, LCombatPay,
		LCarry, LCarryPay,
		LTeam, LTeamRhythm, LTeamYank, LTeamPay,
		LSnow, LSnowPick, LSnowPay,
		LTrail, LTrailPay,
		LRts, LRtsOrder, LRtsPay,
		LFast, LFastFall, LFastPay,
		LCloth, LClothPay,
		LWater, LWaterFlood, LWaterGeyser, LWaterPay,
		LChainBranch, LChainCore, LPoligon, LChainPay,
		LChainPepper, LChainFlour, LChainPower, LChainSling, LChainPeas, LPerk,
		LInside, LInsideEar, LInsideNose, LInsidePay,
		LWake, LWakeLook, LWakePets, LWakePay,
		LFinale, LFinaleHalf, LFinalePay, LFinaleFail,
		LDebrief, LVersus, LHint, LOffer, LDeath, LQueen, LGift,
		LCount
	};
	const FLine GLines[LCount] =
	{
		{ TEXT("Говорит Гнездовое радио Щели! Колонии нужны новобранцы."), TEXT("This is Nest Radio of the Crack! The Colony needs recruits."), 1 },
		{ TEXT("Разведка докладывает: в квартире объект «Именины». Повторяю: торт."), TEXT("Scouts report: operation «Birthday» is on. Repeat: cake."), 1 },
		{ TEXT("Смирно, новобранец! Я — сержант Жвалов. Для своих — Жвало. Для тебя — товарищ сержант."), TEXT("Attention, recruit! I am Sergeant Zhvalov. Friends call me Jaws. You call me Sergeant."), 0 },
		{ TEXT("Ночь. Гигант спит. Кухня — наш полигон. Шевели лапами!"), TEXT("It's night. The Giant sleeps. The kitchen is our training ground. Move those legs!"), 0 },

		{ TEXT("Шесть лап — не для красоты. [WASD] — шагом марш к метке!"), TEXT("Six legs aren't for show. [WASD] — march to the marker!"), 0 },
		{ TEXT("Бегом! Зажми [Shift]!"), TEXT("On the double! Hold [Shift]!"), 0 },
		{ TEXT("Прыжок — [Пробел]. Дважды, чтобы я видел!"), TEXT("Jump — [Space]. Twice, so I can see!"), 0 },
		{ TEXT("Стена — это пол, который стоит. Вверх, к полке!"), TEXT("A wall is just a floor standing up. Up to the shelf!"), 0 },
		{ TEXT("А теперь — вниз головой по дну полки. Муравью гравитация не начальник!"), TEXT("Now walk upside down under the shelf. Gravity is not an ant's boss!"), 0 },
		{ TEXT("Рывок — [Ctrl]. Пригодится, когда сверху летит тапок."), TEXT("Dash — [Ctrl]. Handy when a slipper comes down."), 0 },
		{ TEXT("Вот это муравей! Почти не стыдно."), TEXT("Now that's an ant! Almost not embarrassing."), 0 },

		{ TEXT("Чучело красного. Кусай — [ПКМ]. Жвалами, а не взглядом!"), TEXT("A red dummy. Bite — [RMB]. With jaws, not with your eyes!"), 0 },
		{ TEXT("Кислота — [ЛКМ]. Три плевка. Целься!"), TEXT("Acid — [LMB]. Three spits. Aim!"), 0 },
		{ TEXT("Муха! Сбей её кислотой!"), TEXT("A fly! Shoot it down!"), 0 },
		{ TEXT("Кран капает. Капля для нас — ведро. Уворачивайся рывком!"), TEXT("The tap drips. A drop is a bucket to us. Dodge with a dash!"), 0 },
		{ TEXT("Чисто. Красные бы уже плакали."), TEXT("Clean. The reds would be crying by now."), 0 },

		{ TEXT("Крошка! [E] — взять. Неси в гнездо, в щель у стены."), TEXT("A crumb! [E] — pick up. Carry it home to the crack in the wall."), 0 },
		{ TEXT("Первая доставка! Королева довольна. Ну, почти."), TEXT("First delivery! The Queen is pleased. Well, almost."), 0 },

		{ TEXT("Шоколад одному не поднять. Трое новобранцев — к тебе. Все за край, [E]!"), TEXT("Nobody lifts chocolate alone. Three recruits are yours. Everyone grab an edge, [E]!"), 0 },
		{ TEXT("Идём в ногу: раз-два, взяли! Не в такт — груз качает."), TEXT("Keep in step: heave-ho! Out of step and the load wobbles."), 0 },
		{ TEXT("Резко дёрнешь — кого-нибудь переедешь. Проверено."), TEXT("Yank it and you'll run somebody over. Been there."), 0 },
		{ TEXT("Раз-два, взяли! Вот это отряд!"), TEXT("Heave-ho! Now that's a squad!"), 0 },

		{ TEXT("Всё, что несёшь домой, кормит колонию. Видишь шкалу массы слева? Кубик сахара — в гнездо!"), TEXT("Everything you bring home feeds the colony. See the mass meter? Sugar cube — home!"), 0 },
		{ TEXT("Порог! Выбирай карточку: [1], [2] или [3]. Думай быстро."), TEXT("Threshold! Pick a card: [1], [2] or [3]. Think fast."), 0 },
		{ TEXT("Каждый порог — новая сила. Это и есть снежный ком."), TEXT("Every threshold makes you stronger. That's the snowball."), 0 },

		{ TEXT("Рабочие ходят по запаху. [T] у гнезда — начни тропу, дойди до крошек — [T] ещё раз."), TEXT("Workers follow scent. [T] at the nest starts a trail, walk to the crumbs, [T] again."), 0 },
		{ TEXT("Видишь? Носят сами. Ты теперь начальство."), TEXT("See? They haul it themselves. You're management now."), 0 },

		{ TEXT("Твой отряд. Зажми [Caps Lock] — появится курсор. Обведи всех рамкой [ЛКМ]."), TEXT("Your squad. Hold [Caps Lock] for a cursor. Box-select them with [LMB]."), 0 },
		{ TEXT("Смотри на сыр и жми [Q]: отряд потащит! (Или [ПКМ] по сыру, пока зажат Caps Lock.)"), TEXT("Look at the cheese and press [Q]: the squad hauls it! (Or [RMB] on it while holding Caps Lock.)"), 0 },
		{ TEXT("Сыр в гнезде. Командир из тебя получится."), TEXT("Cheese is home. You'll make a commander."), 0 },

		{ TEXT("Полка висит на шурупах. Держи [E] на шурупе — напарник поможет, одному не открутить."), TEXT("The shelf hangs on screws. Hold [E] on a screw — your partner helps, one ant can't do it."), 0 },
		{ TEXT("Банки летят! Варенье — липкое, рассол — скользкий. Пройди по обоим."), TEXT("Jars down! Jam is sticky, brine is slippery. Walk through both."), 0 },
		{ TEXT("Запомни: в этой квартире всё держится на честном слове. И на шурупах."), TEXT("Remember: everything in this flat hangs on a promise. And on screws."), 0 },

		{ TEXT("Скатерть. На ней — посуда. Хватай край [E], отряд поможет."), TEXT("The tablecloth. Dishes on top. Grab the edge [E], the squad will help."), 0 },
		{ TEXT("Дзынь! Вот так звучит хорошая ночь."), TEXT("Crash! That's the sound of a good night."), 0 },

		{ TEXT("Кран на мойке. Лезь наверх и открой его [E]."), TEXT("The tap on the sink. Climb up and open it [E]."), 0 },
		{ TEXT("Потоп! Плыви, новобранец, плыви!"), TEXT("Flood! Swim, recruit, swim!"), 0 },
		{ TEXT("А это — кола и ментос. Встань на метку. Доверься сержанту."), TEXT("And this is cola and mints. Stand on the marker. Trust your sergeant."), 0 },
		{ TEXT("Летать мы тоже умеем. Иногда."), TEXT("We can fly too. Sometimes."), 0 },

		{ TEXT("Цепная реакция: одно тянет другое. Перец, мука, пробки — смотри, что бывает."), TEXT("Chain reactions: one thing pulls another. Pepper, flour, the fuse box — watch."), 0 },
		{ TEXT("Цепная реакция: дёрни одно — упадёт другое. Карниз и кола — твои."), TEXT("Chain reactions: pull one thing, another falls. The curtain rod and the cola are yours."), 0 },
		{ TEXT("Свободный полигон! Собери свою цепочку. [Enter] — когда наиграешься."), TEXT("Free range! Build your own chain. [Enter] when you're done."), 0 },
		{ TEXT("Хаос — это тоже тактика."), TEXT("Chaos is a tactic too."), 0 },
		{ TEXT("Перечница. Держи [E] — отряд поможет. Опрокинем — чихнут все."), TEXT("The pepper shaker. Hold [E] — the squad helps. Tip it and everybody sneezes."), 0 },
		{ TEXT("Мука! Прогрызи мешок — [E]. Белая завеса: Гигант слепнет, а мы оставляем следы."), TEXT("Flour! Chew the sack open — [E]. A white-out: the Giant goes blind, we leave prints."), 0 },
		{ TEXT("Щиток в прихожей. Втроём — [E] — рубильник вниз. Темнота — наш союзник."), TEXT("The fuse box in the hall. Three of you — [E] — pull the switch. Darkness is our friend."), 0 },
		{ TEXT("Рогатка! Тяни резинку — [E]. Кто в корзинке, тот полетит на стол."), TEXT("A slingshot! Pull the band — [E]. Whoever is in the pouch flies onto the table."), 0 },
		{ TEXT("Горох. Столкни миску — [E]. Всё покатится. Только не себе под лапы."), TEXT("Peas. Push the bowl off — [E]. Everything rolls. Just not under your own feet."), 0 },
		{ TEXT("Ефрейтор — это первая ступень. Выбери перк касты: сила — не главное, главное — стиль."), TEXT("Lance Corporal is the first rung. Pick a caste perk: power is nothing, style is everything."), 0 },

		{ TEXT("Гигант дремлет и храпит. Лезь на него. Ухо — сверху."), TEXT("The Giant dozes and snores. Climb him. The ear is up top."), 0 },
		{ TEXT("В ухо! [E]. Он отмахнётся во сне — держись!"), TEXT("Into the ear! [E]. He'll swat in his sleep — hold on!"), 0 },
		{ TEXT("Теперь — в нос. Чих отправит тебя через полкомнаты. Это транспорт."), TEXT("Now the nose. A sneeze will launch you across the room. That's transport."), 0 },
		{ TEXT("Самый быстрый лифт в квартире. Без кнопок."), TEXT("The fastest lift in the flat. No buttons."), 0 },

		{ TEXT("Он просыпается! В укрытие — под журнальный стол!"), TEXT("He's waking up! Take cover — under the coffee table!"), 0 },
		{ TEXT("Смотри, куда он смотрит. Замер — замри. Пошёл — беги."), TEXT("Watch where he looks. He freezes — you freeze. He moves — you run."), 0 },
		{ TEXT("Бадрик проснулся. Жрёт всё, что упало, и ходит по нашим тропам. Обойди."), TEXT("Badrik is up. He eats anything dropped and follows our trails. Go round him."), 0 },
		{ TEXT("Пережил Розовую Плиту. Будет из тебя муравей."), TEXT("You survived the Pink Slab. You'll make an ant yet."), 0 },

		{ TEXT("Боевая задача: кусок торта — в гнездо. Отряд твой. До последнего света — две минуты."), TEXT("Mission: a piece of cake — home. The squad is yours. Two minutes to last light."), 0 },
		{ TEXT("Гигант идёт к холодильнику! Быстрее!"), TEXT("The Giant is heading for the fridge! Faster!"), 0 },
		{ TEXT("Торт дома! Королева рыдает от счастья!"), TEXT("Cake is home! The Queen weeps with joy!"), 0 },
		{ TEXT("Не успели. Ещё раз — с последней точки."), TEXT("Too slow. Again — from the last checkpoint."), 0 },

		{ TEXT("Выпуск! Новобранец, теперь ты — Ефрейтор. Настоящий рейд ждёт."), TEXT("Graduation! Recruit, you are now a Lance Corporal. The real raid awaits."), 0 },
		{ TEXT("А хочешь сыграть за Гиганта? Тапок — в твоих руках."), TEXT("Fancy playing the Giant? The slipper is in your hands."), 1 },
		{ TEXT("Не стой столбом! Смотри на метку."), TEXT("Don't just stand there! Follow the marker."), 0 },
		{ TEXT("Застрял? [Enter] — сержант поможет."), TEXT("Stuck? [Enter] — the sergeant will help."), 0 },
		{ TEXT("Раздавлен. Ничего, в учебке — с последней точки."), TEXT("Squashed. No matter: in training you go again."), 0 },
		{ TEXT("Мои дети… Вы вернулись с тортом."), TEXT("My children… you came back with cake."), 2 },
		{ TEXT("Королева прислала подарок новобранцам: загляните в кладовые."), TEXT("The Queen sends the recruits a gift: check the stores."), 2 },
	};
	float LineSeconds(int32 L)
	{
		if (L < 0 || L >= LCount) { return 0.f; }
		return FMath::Clamp(1.0f + 0.045f * FCString::Strlen(GLines[L].Ru), 2.0f, 6.0f);
	}
}

// =====================================================================================================================
// The beats (teach -> test -> payoff). Steps: text, keys, how many, team (counts for everybody).
// =====================================================================================================================
namespace
{
	enum EBeat : int32 { BPrologue, BMove, BCombat, BCarry, BTeam, BSnow, BTrail, BRts, BFast, BCloth, BWater, BChains, BInside, BWake, BFinale, BDebrief, BCount };

	TArray<AntTut::FBeat>& Table()
	{
		static TArray<AntTut::FBeat> T;
		if (T.Num() > 0) { return T; }
		using S = AntTut::FStep;
		auto Add = [](const TCHAR* Id, uint8 Ch, uint8 Br, const TCHAR* Ru, const TCHAR* En, float Par, int32 Xp, TArray<S> Steps)
		{
			AntTut::FBeat B;
			B.Id = Id; B.Chapter = Ch; B.Branch = Br; B.TitleRu = Ru; B.TitleEn = En; B.Par = Par; B.Xp = Xp; B.Steps = MoveTemp(Steps);
			return B;
		};
		T.Add(Add(TEXT("prologue"), 1, AntTut::Core, TEXT("Гнездовое радио"), TEXT("Nest Radio"), 40.f, 0,
			{ S{ TEXT("Слушай сводку"), TEXT("Hear the briefing"), TEXT("Enter"), 1, true } }));
		T.Add(Add(TEXT("move"), 1, AntTut::Core, TEXT("Шесть лап"), TEXT("Six Legs"), 150.f, 100,
			{ S{ TEXT("Дойди до метки"), TEXT("Walk to the marker"), TEXT("WASD"), 1, false },
			  S{ TEXT("Пробеги"), TEXT("Sprint"), TEXT("Shift"), 1, false },
			  S{ TEXT("Прыгни дважды"), TEXT("Jump twice"), TEXT("Пробел|Space"), 2, false },
			  S{ TEXT("Залезь на стену"), TEXT("Climb the wall"), TEXT("W"), 1, false },
			  S{ TEXT("Пройди по потолку полки"), TEXT("Walk under the shelf"), TEXT("W"), 1, false },
			  S{ TEXT("Рывок дважды"), TEXT("Dash twice"), TEXT("Ctrl"), 2, false } }));
		T.Add(Add(TEXT("combat"), 1, AntTut::Core, TEXT("Жвалы и кислота"), TEXT("Jaws and Acid"), 130.f, 100,
			{ S{ TEXT("Укуси чучело"), TEXT("Bite the dummy"), TEXT("ПКМ|RMB"), 3, false },
			  S{ TEXT("Плюнь кислотой"), TEXT("Spit acid"), TEXT("ЛКМ|LMB"), 3, false },
			  S{ TEXT("Сбей муху"), TEXT("Down the fly"), TEXT("ЛКМ|LMB"), 1, true },
			  S{ TEXT("Переживи капель"), TEXT("Dodge the drips"), TEXT("Ctrl"), 1, false } }));
		T.Add(Add(TEXT("carry"), 1, AntTut::Core, TEXT("Первая крошка"), TEXT("First Crumb"), 70.f, 80,
			{ S{ TEXT("Возьми крошку"), TEXT("Pick up a crumb"), TEXT("E"), 1, false },
			  S{ TEXT("Донеси в гнездо"), TEXT("Bring it home"), TEXT(""), 1, false } }));
		T.Add(Add(TEXT("team"), 1, AntTut::Core, TEXT("Раз-два, взяли!"), TEXT("Heave-Ho!"), 140.f, 120,
			{ S{ TEXT("Возьмись за шоколад"), TEXT("Grab the chocolate"), TEXT("E"), 1, false },
			  S{ TEXT("Иди в такт"), TEXT("Keep in step"), TEXT(""), 1, false },
			  S{ TEXT("Шоколад — в гнездо"), TEXT("Chocolate — home"), TEXT(""), 1, true } }));
		T.Add(Add(TEXT("snow"), 1, AntTut::Core, TEXT("Снежный ком"), TEXT("Snowball"), 70.f, 80,
			{ S{ TEXT("Кубик сахара — в гнездо"), TEXT("Sugar cube — home"), TEXT("E"), 1, true },
			  S{ TEXT("Выбери карточку"), TEXT("Pick a card"), TEXT("1|2|3"), 1, false } }));
		T.Add(Add(TEXT("trail"), 1, AntTut::Core, TEXT("Тропа"), TEXT("The Trail"), 110.f, 100,
			{ S{ TEXT("Начни тропу у гнезда"), TEXT("Start a trail at the nest"), TEXT("T"), 1, false },
			  S{ TEXT("Закончи у крошек"), TEXT("End it at the crumbs"), TEXT("T"), 1, false },
			  S{ TEXT("Рабочие несут крошки"), TEXT("Workers haul crumbs"), TEXT(""), 3, true } }));
		T.Add(Add(TEXT("rts"), 1, AntTut::Core, TEXT("Командир"), TEXT("Commander"), 110.f, 120,
			{ S{ TEXT("Выдели отряд"), TEXT("Select the squad"), TEXT("Caps Lock+ЛКМ|Caps Lock+LMB"), 1, false },
			  S{ TEXT("Прикажи: тащите сыр"), TEXT("Order: haul the cheese"), TEXT("Q|Q"), 1, false },
			  S{ TEXT("Сыр — в гнезде"), TEXT("Cheese is home"), TEXT(""), 1, true } }));
		T.Add(Add(TEXT("fasteners"), 2, AntTut::Core, TEXT("Шурупы и банки"), TEXT("Screws and Jars"), 110.f, 120,
			{ S{ TEXT("Открути шурупы полки"), TEXT("Undo the shelf screws"), TEXT("E"), 2, true },
			  S{ TEXT("Пройди по варенью"), TEXT("Walk through the jam"), TEXT(""), 1, false },
			  S{ TEXT("Пройди по рассолу"), TEXT("Walk through the brine"), TEXT(""), 1, false } }));
		T.Add(Add(TEXT("cloth"), 2, AntTut::Core, TEXT("Скатерть"), TEXT("The Tablecloth"), 75.f, 120,
			{ S{ TEXT("Схвати край скатерти"), TEXT("Grab the tablecloth edge"), TEXT("E"), 1, false },
			  S{ TEXT("Сдёрни скатерть"), TEXT("Pull it off"), TEXT("E"), 1, true } }));
		T.Add(Add(TEXT("water"), 2, AntTut::Core, TEXT("Потоп"), TEXT("Flood"), 110.f, 140,
			{ S{ TEXT("Открой кран"), TEXT("Open the tap"), TEXT("E"), 1, true },
			  S{ TEXT("Проплыви"), TEXT("Swim"), TEXT("W|Пробел"), 1, false },
			  S{ TEXT("Кола + ментос"), TEXT("Cola + mints"), TEXT(""), 1, true } }));
		if (AntTut::BranchMerged(AntTut::Chains))
		{
			T.Add(Add(TEXT("chains"), 2, AntTut::Chains, TEXT("Цепные реакции"), TEXT("Chain Reactions"), 200.f, 150,
				{ S{ TEXT("Перец: чих"), TEXT("Pepper: sneeze"), TEXT("E"), 1, true },
				  S{ TEXT("Мука: белая завеса"), TEXT("Flour: white-out"), TEXT("E"), 1, true },
				  S{ TEXT("Щиток: темнота"), TEXT("Fuse box: blackout"), TEXT("E"), 1, true },
				  S{ TEXT("Рогатка: полёт на стол"), TEXT("Slingshot: fly to the table"), TEXT("E"), 1, true },
				  S{ TEXT("Горох: всё покатилось"), TEXT("Peas: everything rolls"), TEXT("E"), 1, true } }));
		}
		else
		{
			T.Add(Add(TEXT("chains"), 2, AntTut::Chains, TEXT("Цепные реакции"), TEXT("Chain Reactions"), 140.f, 150,
				{ S{ TEXT("Карниз: штора и цветок"), TEXT("Curtain rod: curtain and plant"), TEXT(""), 1, true },
				  S{ TEXT("Кола: липкий пол"), TEXT("Cola: sticky floor"), TEXT(""), 1, true } }));
		}
		T.Add(Add(TEXT("inside"), 3, AntTut::Core, TEXT("Внутрь Гиганта"), TEXT("Inside the Giant"), 90.f, 150,
			{ S{ TEXT("Залезь на Гиганта"), TEXT("Climb the Giant"), TEXT("W"), 1, false },
			  S{ TEXT("Залезь в ухо"), TEXT("Into the ear"), TEXT("E"), 1, false },
			  S{ TEXT("Вылети с чихом"), TEXT("Ride the sneeze"), TEXT("E"), 1, false } }));
		T.Add(Add(TEXT("wake"), 3, AntTut::Core, TEXT("Гигант проснулся"), TEXT("The Giant Wakes"), 90.f, 150,
			{ S{ TEXT("Укройся под столом"), TEXT("Hide under the table"), TEXT(""), 1, false },
			  S{ TEXT("Продержись"), TEXT("Hold out"), TEXT(""), 1, true },
			  S{ TEXT("Обойди Бадрика"), TEXT("Get past Badrik"), TEXT(""), 1, false } }));
		T.Add(Add(TEXT("finale"), 3, AntTut::Core, TEXT("Последний свет"), TEXT("Last Light"), 170.f, 250,
			{ S{ TEXT("Возьмитесь за торт"), TEXT("Grab the cake"), TEXT("E"), 1, false },
			  S{ TEXT("Торт — в гнездо"), TEXT("Cake — home"), TEXT(""), 1, true } }));
		T.Add(Add(TEXT("debrief"), 4, AntTut::Core, TEXT("Выпуск"), TEXT("Graduation"), 60.f, 0,
			{ S{ TEXT("Получи звание"), TEXT("Get your rank"), TEXT("Enter"), 1, true } }));
		return T;
	}
}

// =====================================================================================================================
// Process-wide: the mode of the next map, the progress save, the language
// =====================================================================================================================
namespace
{
	uint8 GWanted = 255;      // 255: not read from the command line yet
	int32 GWantedBeat = 0;
	TWeakObjectPtr<AAntTutorial> GDirector;
	bool GProgressLoaded = false;
	AntTut::FProgress GProgress;

	FString ProgressDir() { return FPaths::ProjectSavedDir() / (AntTut::IsDemo() ? TEXT("Demo") : TEXT("Tutorial")); }
	FString ProgressPath() { return ProgressDir() / TEXT("progress.ini"); }
	const FLinearColor TutGold(1.f, 0.78f, 0.25f), TutGreen(0.45f, 0.95f, 0.45f), TutRed(1.f, 0.35f, 0.25f);
}

namespace AntTut
{
	bool IsDemo()
	{
		// The demo build: compiled with ANT_DEMO=1, or launched with -AntDemo, or the packaged game carries AntDemo.flag next to its
		// content (the DistDemo profile, docs/DEMO.md: the same binaries, a flag file and its own saves). -AntFull overrides the flag.
		static const bool b = ANT_DEMO != 0 || FParse::Param(FCommandLine::Get(), TEXT("AntDemo"))
			|| (IFileManager::Get().FileExists(*(FPaths::ProjectDir() / TEXT("AntDemo.flag"))) && !FParse::Param(FCommandLine::Get(), TEXT("AntFull")));
		return b;
	}

	uint8 Wanted()
	{
		if (GWanted == 255)
		{
			GWanted = Off;
			int32 B = -1;
			const bool bValue = FParse::Value(FCommandLine::Get(), TEXT("AntTutorial="), B);   // (-AntTutorial=N first: the bare flag matches "-AntTutorial=N" too)
			if (bValue || FParse::Param(FCommandLine::Get(), TEXT("AntTutorial")))
			{
				GWanted = Training;
				GWantedBeat = FMath::Clamp(bValue ? B : 0, 0, NumBeats() - 1);
			}
			else if (FParse::Param(FCommandLine::Get(), TEXT("AntSandbox")))
			{
				GWanted = Sandbox;
			}
		}
		return GWanted;
	}
	void SetWanted(uint8 Mode, int32 StartBeat) { Wanted(); GWanted = Mode; GWantedBeat = FMath::Clamp(StartBeat, 0, NumBeats() - 1); }
	int32 WantedBeat() { Wanted(); return GWantedBeat; }

	bool Active(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		AAntTutorial* T = W ? AAntTutorial::Get(W) : nullptr;
		return T && T->Mode == Training;
	}
	bool SandboxActive(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		AAntTutorial* T = W ? AAntTutorial::Get(W) : nullptr;
		return T && T->Mode == Sandbox;
	}

	const TCHAR* NestFolder()
	{
		// The tutorial plays on a colony of its own: the real one is not touched (the gift goes in at the next real raid).
		const uint8 M = Wanted();
		if (M != Off) { return IsDemo() ? TEXT("NestDemoTutorial") : TEXT("NestTutorial"); }
		return IsDemo() ? TEXT("NestDemo") : TEXT("Nest");
	}

	const TCHAR* Tx(const TCHAR* Ru, const TCHAR* En) { return AntUi::Lang() == 1 && En && *En ? En : Ru; }

	FString RankName(int32 Rank)
	{
#if ANT_TUT_FIGHTER
		if (Rank >= 1) { return AntRank::Name(Rank); }   // the colony's ladder: 1 is the Lance Corporal the training hands out
#endif
		return Rank <= 0 ? FString(Tx(TEXT("Новобранец"), TEXT("Recruit"))) : FString(Tx(TEXT("Рядовой"), TEXT("Private")));
	}

	int32 NumBeats() { return Table().Num(); }
	const FBeat& Beat(int32 i) { return Table()[FMath::Clamp(i, 0, Table().Num() - 1)]; }
	FString BeatTitle(int32 i) { const FBeat& B = Beat(i); return Tx(B.TitleRu, B.TitleEn); }
	FString ChapterName(int32 Ch)
	{
		switch (Ch)
		{
		case 1: return Tx(TEXT("Глава 1 · Подъём"), TEXT("Chapter 1 · Reveille"));
		case 2: return Tx(TEXT("Глава 2 · Полигон «Кухня»"), TEXT("Chapter 2 · Kitchen Range"));
		case 3: return Tx(TEXT("Глава 3 · Гигант"), TEXT("Chapter 3 · The Giant"));
		default: return Tx(TEXT("Выпуск"), TEXT("Graduation"));
		}
	}
	bool BranchMerged(uint8 Branch)
	{
		switch (Branch)
		{
		case Chains: return ANT_TUT_CHAINS != 0;
		case Life: return ANT_TUT_LIFE != 0;
		case Colony: return ANT_TUT_COLONY != 0;
		case Fighter: return ANT_TUT_FIGHTER != 0;
		default: return true;
		}
	}

	FProgress& Progress()
	{
		if (!GProgressLoaded)
		{
			GProgressLoaded = true;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *ProgressPath()))
			{
				TArray<FString> Lines;
				Text.ParseIntoArrayLines(Lines);
				for (const FString& L : Lines)
				{
					FString K, V;
					if (!L.Split(TEXT("="), &K, &V)) { continue; }
					K.TrimStartAndEndInline(); V.TrimStartAndEndInline();
					if (K == TEXT("done")) { GProgress.bDone = V.ToBool(); }
					else if (K == TEXT("beats")) { GProgress.BeatsDone = (uint32)FCString::Strtoui64(*V, nullptr, 10); }
					else if (K == TEXT("xp")) { GProgress.Xp = FCString::Atoi(*V); }
					else if (K == TEXT("rank")) { GProgress.Rank = FCString::Atoi(*V); }
					else if (K == TEXT("medals")) { V.ParseIntoArray(GProgress.Medals, TEXT(";")); }
					else if (K == TEXT("gift")) { GProgress.bGiftPending = V.ToBool(); }
					else if (K == TEXT("demo_raids")) { GProgress.DemoRaids = FCString::Atoi(*V); }
					else if (K == TEXT("demo_versus")) { GProgress.bDemoVersus = V.ToBool(); }
					else if (K == TEXT("demo_ended")) { GProgress.bDemoEnded = V.ToBool(); }
					else if (K == TEXT("best")) { GProgress.BestSeconds = FCString::Atof(*V); }
				}
			}
		}
		return GProgress;
	}

	void SaveProgress()
	{
		const FProgress& P = Progress();
		const FString Text = FString::Printf(TEXT("done=%d\nbeats=%u\nxp=%d\nrank=%d\nmedals=%s\ngift=%d\ndemo_raids=%d\ndemo_versus=%d\ndemo_ended=%d\nbest=%.0f\n"),
			P.bDone ? 1 : 0, P.BeatsDone, P.Xp, P.Rank, *FString::Join(P.Medals, TEXT(";")), P.bGiftPending ? 1 : 0, P.DemoRaids, P.bDemoVersus ? 1 : 0, P.bDemoEnded ? 1 : 0, P.BestSeconds);
		IFileManager::Get().MakeDirectory(*ProgressDir(), true);
		FFileHelper::SaveStringToFile(Text, *ProgressPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogTemp, Display, TEXT("ANTTUT progress saved: done %d beats %u xp %d rank %d medals %d gift %d -> %s"), P.bDone ? 1 : 0, P.BeatsDone, P.Xp, P.Rank, P.Medals.Num(), P.bGiftPending ? 1 : 0, *ProgressPath());
	}

	bool ConsumeGift()
	{
		FProgress& P = Progress();
		if (!P.bGiftPending || Wanted() != Off) { return false; }
		P.bGiftPending = false;
		SaveProgress();
		return true;
	}

	void Note(const UObject* Ctx, uint8 Event, AActor* By, AActor* What)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		AAntTutorial* T = W ? AAntTutorial::Get(W) : nullptr;
		if (!T || !T->HasAuthority() || T->Mode != Training) { return; }
		T->OnNote(Event, By, What);
	}

	bool OwnsRaid(const AAntRaid* Raid)
	{
		return Raid && (Active(Raid) || SandboxActive(Raid));
	}

	bool MassHeld(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		AAntTutorial* T = W ? AAntTutorial::Get(W) : nullptr;
		// Only the snowball's own beat, while it is played, feeds the meter: a pick popping up over the other lessons covers them.
		return T && T->Mode == Training && !(T->BeatNo == BSnow && T->Phase == AAntTutorial::Play);
	}
}

// =====================================================================================================================
// Where things are (map v4 = level v3 tables, docs/level/LEVEL_V3.md): the kitchen west wall with the nest, the shelf with
// the jars on it, the table with the tablecloth, the sink; the sofa in the living room.
// =====================================================================================================================
namespace
{
	const FVector PStart(-6450.f, -2500.f, 30.f);        // the kitchen floor just out of the nest
	const FVector PWalk(-5300.f, -2450.f, 20.f);
	const FVector PSprint(-4200.f, -1250.f, 20.f);
	const FVector PWall(-6985.f, -3300.f, 700.f);        // on the west wall by the nest (further along, books and a bench stand against it)
	const FVector PShelfUnder(-6820.f, -150.f, 2600.f);  // under the shelf board (its top is at 2680)
	const FVector PDummy(-5200.f, -3250.f, 20.f);
	const FVector PSugarTarget(-4700.f, -3650.f, 20.f);
	const FVector PCrumb(-5500.f, -1700.f, 20.f);
	const FVector PChoc(-4700.f, -2900.f, 20.f);
	const FVector PSugar(-5800.f, -3500.f, 20.f);
	const FVector PPile(-4100.f, -3700.f, 20.f);
	const FVector PCheese(-3900.f, -1500.f, 20.f);
	const FVector PScrew(-6992.f, 800.f, 2520.f);        // v3_shelf_sc2 / sc3: the side over the pickles
	const FVector PClothEdge(540.f, -2700.f, 120.f);
	const FVector PGeyser(-3200.f, 1600.f, 20.f);
	const FVector PRod(-1750.f, -4250.f, 20.f);
	const FVector PCola(2300.f, 2500.f, 20.f);
	const FVector PSofaFloor(12300.f, -2050.f, 30.f);
	const FVector PCover(13750.f, -3400.f, 30.f);        // under the coffee table
	const FVector PPastDog(10400.f, -3300.f, 30.f);
	const FVector PDog(11800.f, -3000.f, 20.f);
	const FVector PFinaleStart(-5800.f, -2450.f, 30.f);
	const FVector PCake(-1500.f, -1900.f, 30.f);
	const TCHAR* NCrumb = TEXT("Хлебная крошка");
	const TCHAR* NChoc = TEXT("Долька шоколада");
	const TCHAR* NSugar = TEXT("Кубик сахара");
	const TCHAR* NCheese = TEXT("Кусочек сыра");
	const TCHAR* NCake = TEXT("Кусочек торта");

	// The line for each step of a beat (said when the host reaches that step), the payoff line.
	int32 StepLine(int32 Beat, int32 Step)
	{
		static const int16 L[BCount][6] =
		{
			{ -1 },
			{ LMove, LSprint, LJump, LClimb, LCeiling, LDash },
			{ LBite, LSpit, LFly, LDrip },
			{ LCarry, -1 },
			{ LTeam, LTeamRhythm, LTeamYank },
			{ LSnow, LSnowPick },
			{ LTrail, -1, -1 },
			{ LRts, LRtsOrder, -1 },
			{ LFast, LFastFall, -1 },
			{ LCloth, -1 },
			{ LWater, LWaterFlood, LWaterGeyser },
			{ LChainPepper, LChainFlour, LChainPower, LChainSling, LChainPeas },
			{ LInside, LInsideEar, LInsideNose },
			{ LWake, LWakeLook, LWakePets },
			{ LFinale, -1 },
			{ -1 },
		};
		if (Beat == BChains && Step == 0 && !AntTut::BranchMerged(AntTut::Chains)) { return LChainCore; }
		return (Beat >= 0 && Beat < BCount && Step >= 0 && Step < 6) ? L[Beat][Step] : -1;
	}
	int32 PayLine(int32 Beat)
	{
		static const int16 P[BCount] = { -1, LMovePay, LCombatPay, LCarryPay, LTeamPay, LSnowPay, LTrailPay, LRtsPay, LFastPay, LClothPay, LWaterPay, LChainPay, LInsidePay, LWakePay, LFinalePay, -1 };
		return (Beat >= 0 && Beat < BCount) ? P[Beat] : -1;
	}

	bool IsTablecloth(const AActor* A)
	{
		const AAntCloth* C = Cast<AAntCloth>(A);
		return C && C->Kind == AntClothKind::Tablecloth;
	}
	AAntCloth* Tablecloth(UWorld* W)
	{
		for (TActorIterator<AAntCloth> It(W); It; ++It) { if (It->Kind == AntClothKind::Tablecloth) { return *It; } }
		return nullptr;
	}
	AHumanCharacter* TheHuman(UWorld* W)
	{
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { if (!It->IsPlayerControlled()) { return *It; } }
		for (TActorIterator<AHumanCharacter> It(W); It; ++It) { return *It; }
		return nullptr;
	}
	/** The inside spot an ant is in ("ухо", "нос"...), empty if none. */
	FString InsidePlace(const AAntCharacter* Ant)
	{
		const AAntInsideNet* N = Ant ? AAntInsideNet::Get(Ant->GetWorld()) : nullptr;
		const int32 S = N ? N->SlotOf(Ant) : -1;
		if (S < 0 || !N->Slots.IsValidIndex(S)) { return FString(); }
		const TConstArrayView<AntInside::FSpotDef> D = AntInside::Defs();
		const uint8 Def = N->Slots[S].Def;
		return Def < D.Num() && D[Def].Place ? FString(D[Def].Place) : FString();
	}
	bool SpotOfHuman(AHumanCharacter* H, const TCHAR* Place, FVector& Out)
	{
		const TConstArrayView<AntInside::FSpotDef> D = AntInside::Defs();
		for (int32 i = 0; H && i < D.Num(); ++i)
		{
			if (D[i].Place && FCString::Strcmp(D[i].Place, Place) == 0 && D[i].Who == 0 && AntInside::SpotPoint(H, (uint8)i, Out)) { return true; }
		}
		return false;
	}
	/** A chains-branch prop by its kind name (reflection: no compile-time dependency on the branch). */
	bool ReactProp(UWorld* W, const TCHAR* KindName, FVector& Out)
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->GetClass()->GetName() != TEXT("AntReactProp")) { continue; }
			if (const FProperty* P = It->GetClass()->FindPropertyByName(TEXT("Kind")))
			{
				int64 V = -1;
				if (const FByteProperty* BP = CastField<FByteProperty>(P)) { V = BP->GetPropertyValue_InContainer(*It); if (BP->Enum) { if (BP->Enum->GetNameStringByValue(V).Contains(KindName)) { Out = It->GetActorLocation(); return true; } } }
				else if (const FEnumProperty* EP = CastField<FEnumProperty>(P)) { V = EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(EP->ContainerPtrToValuePtr<void>(*It)); if (EP->GetEnum() && EP->GetEnum()->GetNameStringByValue(V).Contains(KindName)) { Out = It->GetActorLocation(); return true; } }
			}
		}
		return false;
	}
}

// =====================================================================================================================
// The director
// =====================================================================================================================

AAntTutorial::AAntTutorial()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
}

void AAntTutorial::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntTutorial, Mode);
	DOREPLIFETIME(AAntTutorial, BeatNo);
	DOREPLIFETIME(AAntTutorial, Phase);
	DOREPLIFETIME(AAntTutorial, PhaseStart);
	DOREPLIFETIME(AAntTutorial, Players);
	DOREPLIFETIME(AAntTutorial, Marks);
	DOREPLIFETIME(AAntTutorial, LineNo);
	DOREPLIFETIME(AAntTutorial, LineStart);
	DOREPLIFETIME(AAntTutorial, TeamXp);
	DOREPLIFETIME(AAntTutorial, TimerEnds);
	DOREPLIFETIME(AAntTutorial, bOffer);
	DOREPLIFETIME(AAntTutorial, Retries);
	DOREPLIFETIME(AAntTutorial, PoligonEnds);
	DOREPLIFETIME(AAntTutorial, TotalSeconds);
	DOREPLIFETIME(AAntTutorial, BeatsDoneMask);
	DOREPLIFETIME(AAntTutorial, Assists);
}

AAntTutorial* AAntTutorial::Get(const UWorld* World)
{
	if (!World) { return nullptr; }
	if (GDirector.IsValid() && GDirector->GetWorld() == World) { return GDirector.Get(); }
	for (TActorIterator<AAntTutorial> It(const_cast<UWorld*>(World)); It; ++It) { GDirector = *It; return *It; }
	return nullptr;
}

float AAntTutorial::ServerNow() const
{
	const AGameStateBase* G = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return G ? (float)G->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

const FAntTutPlayer* AAntTutorial::Find(const APlayerState* PS) const
{
	for (const FAntTutPlayer& P : Players) { if (P.PS == PS) { return &P; } }
	return nullptr;
}

void AAntTutorial::BeginPlay()
{
	Super::BeginPlay();
	GDirector = this;
	if (!HasAuthority()) { return; }
	GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	Session = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	bAuto = FParse::Param(FCommandLine::Get(), TEXT("AntTutAuto"));
	UE_LOG(LogTemp, Display, TEXT("ANTTUT director up: mode %d, start beat %d, auto %d, branches chains %d life %d colony %d fighter %d metabus %d, demo %d"),
		(int32)Mode, StartBeat, bAuto ? 1 : 0, ANT_TUT_CHAINS, ANT_TUT_LIFE, ANT_TUT_COLONY, ANT_TUT_FIGHTER, ANT_TUT_METABUS, AntTut::IsDemo() ? 1 : 0);
}

void AAntTutorial::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority()) { ServerTick(Dt); }
}

void AAntTutorial::Command(const FString& Cmd)
{
	if (!GM) { return; }
	UE_LOG(LogTemp, Display, TEXT("ANTTUT stage: %s"), *Cmd);
	GM->StageCommand(Cmd);
}

AAntCharacter* AAntTutorial::AntOf(const FAntTutPlayer& P) const
{
	const AController* C = P.PS ? Cast<AController>(P.PS->GetOwner()) : nullptr;
	return C ? Cast<AAntCharacter>(C->GetPawn()) : nullptr;
}

void AAntTutorial::Say(int32 Line, int32 Step, bool bCut)
{
	if (Line < 0 || Line >= LCount) { return; }
	FQLine Q;
	Q.Line = (int16)Line; Q.Beat = BeatNo; Q.Step = (int8)Step;
	LineQueue.Add(Q);
	while (LineQueue.Num() > 4) { LineQueue.RemoveAt(0); }   // a fast player never hears a long backlog
	if (bCut) { LineEnds = FMath::Min(LineEnds, ServerNow()); }
}

AKitchenLoot* AAntTutorial::SpawnFood(const FString& Name, const FVector& At, const FString& Rename)
{
	TSet<AKitchenLoot*> Before;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It) { Before.Add(*It); }
	Command(FString::Printf(TEXT("food %.0f %.0f %.0f %s"), At.X, At.Y, At.Z, *Name));
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (!Before.Contains(*It) && It->LootName == Name)
		{
			if (!Rename.IsEmpty()) { It->LootName = Rename; }
			Spawned.Add(*It);
			return *It;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("ANTTUT could not spawn %s at %s"), *Name, *At.ToCompactString());
	return nullptr;
}

void AAntTutorial::TeleportAll(const FVector& At, float Yaw, bool bWall)
{
	int32 k = 0;
	for (FAntTutPlayer& P : Players)
	{
		if (AAntCharacter* A = AntOf(P))
		{
			A->LetGo();
			A->ApplyTeleport(At + (bWall ? FVector(0.f, 90.f * k, 0.f) : FVector(70.f * (k % 3), 70.f * (k / 3), 0.f)));
			if (APlayerController* PC = Cast<APlayerController>(A->GetController())) { PC->ClientSetRotation(FRotator(-12.f, Yaw, 0.f)); }
			++k;
		}
	}
}

void AAntTutorial::SetGod(bool bOn)
{
	for (FAntTutPlayer& P : Players)
	{
		if (AAntCharacter* A = AntOf(P)) { A->SetAdmin(bOn ? (A->AdminFlags | 1) : (A->AdminFlags & ~1), A->AdminSpeed); }
	}
}

FVector AAntTutorial::Checkpoint() const
{
	switch (BeatNo)
	{
	case BInside: case BWake: return PSofaFloor;
	case BFinale: return PFinaleStart;
	case BFast: return PScrew + FVector(30.f, -150.f, 0.f);
	case BCloth: return ClothEdge + FVector(0.f, -450.f, 30.f);
	case BWater: return AKitchenFaucet::ColdValve() + FVector(0.f, -120.f, 30.f);
	case BChains: return Marks.Num() > 0 ? FVector(Marks[0].At) + FVector(0.f, 0.f, 20.f) : FVector(-2500.f, -2000.f, 30.f);   // the station the team is at
	default: return PStart;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Server: the clock
// ---------------------------------------------------------------------------------------------------------------------

void AAntTutorial::ServerTick(float Dt)
{
	UWorld* W = GetWorld();
	AAntRaid* Raid = AAntRaid::Get(W);
	const float Now = ServerNow();

	// The players (ants of our colony, not bots, not the Human).
	for (TActorIterator<AAntPlayerState> It(W); It; ++It)
	{
		if (It->IsABot() || It->bIsHuman || It->Team != 0 || Find(*It)) { continue; }
		FAntTutPlayer P;
		P.PS = *It;
		P.Count.Init(0, AntTut::Beat(BeatNo).Steps.Num());
		Players.Add(P);
		UE_LOG(LogTemp, Display, TEXT("ANTTUT player joined: %s (%d now)"), *It->GetPlayerName(), Players.Num());
		if (bStarted && Mode == AntTut::Training)
		{
			if (AAntCharacter* A = AntOf(Players.Last())) { A->ApplyTeleport(Checkpoint()); }
		}
	}
	Players.RemoveAll([](const FAntTutPlayer& P) { return !IsValid(P.PS); });

	if (!bStarted)
	{
		if (!Raid || !GM) { return; }
		if (Raid->Phase == AntRaidPhase::Nest)
		{
			GM->AllyBots = 0;   // the training is the players' own: no stand-in bots, no red ants
			GM->RivalBots = 0;
			Raid->DevCommand({ TEXT("seed"), TEXT("7") });   // one validated layout for every training: the tablecloth is on the table, the kid is out
			Raid->Nest.Larvae = FMath::Max(Raid->Nest.Larvae, 12);
			Raid->Nest.Workers = FMath::Max(Raid->Nest.Workers, 40);
			Raid->StartRaid();
			StartedAt = Now;
			return;
		}
		if (Raid->InRaid() && Now - StartedAt > 1.5f && Players.Num() > 0)
		{
			Start();
		}
		return;
	}
	if (Mode == AntTut::Sandbox)
	{
		return;
	}

	// Lines: one after another; the host's [Enter] cuts the current one short. A line that has gone stale (another beat, or the
	// step it is about is already done) is dropped, so a fast player never hears the sergeant lag behind him.
	if (Now >= LineEnds)
	{
		bool bGot = false;
		while (!bGot && LineQueue.Num() > 0)
		{
			const FQLine Q = LineQueue[0];
			LineQueue.RemoveAt(0);
			bool bStale = Q.Beat != BeatNo;
			if (!bStale && Q.Step >= 0 && Phase == Play && Players.Num() > 0 && Players[0].Count.IsValidIndex(Q.Step) && AntTut::Beat(BeatNo).Steps.IsValidIndex(Q.Step)
				&& Players[0].Count[Q.Step] >= AntTut::Beat(BeatNo).Steps[Q.Step].Need) { bStale = true; }
			if (bStale) { UE_LOG(LogTemp, Display, TEXT("ANTTUT line %d dropped (stale)"), (int32)Q.Line); continue; }
			LineNo = Q.Line;
			LineStart = Now;
			LineEnds = Now + LineSeconds(LineNo);
			bGot = true;
			UE_LOG(LogTemp, Display, TEXT("ANTTUT line %d: %s"), (int32)LineNo, GLines[LineNo].Ru);
		}
		if (!bGot && LineNo >= 0 && Now > LineEnds + 1.5f) { LineNo = -1; }
	}

	BeatClock += Dt;
	switch (Phase)
	{
	case Fade:
		if (Now >= FadeAt && !bStaged) { Stage(BeatNo); }
		break;
	case Intro:
		if (BeatNo == BPrologue)
		{
			if (LineQueue.Num() == 0 && Now >= LineEnds) { TeamStep(0); Complete(); }
		}
		else
		{
			Phase = Play;
			PhaseStart = Now;
		}
		break;
	case Play:
		Check(Dt);
		break;
	case Poligon:
		if (Now >= PoligonEnds) { Phase = Payoff; PhaseStart = Now; PayoffAt = Now; }
		break;
	case Payoff:
		if (Now - PayoffAt > 3.2f && LineQueue.Num() == 0) { Next(); }
		break;
	case Debrief:
#if ANT_TUT_FIGHTER
		if (PerkAt > 0.f && Now >= PerkAt)
		{
			PerkAt = -1.f;
			for (FAntTutPlayer& P : Players) { AntFighter::OpenPerkPick(P.PS, -1); }
			UE_LOG(LogTemp, Display, TEXT("ANTTUT the caste perk pick is open (rank 1)"));
		}
#endif
		break;
	default:
		break;
	}
	if (bAuto) { AutoPlay(Dt); }
}

void AAntTutorial::Start()
{
	bStarted = true;
	StartedAt = ServerNow();
	UWorld* W = GetWorld();
	AAntGameState* S = W->GetGameState<AAntGameState>();
	if (S) { S->TimeRemaining = 99999.f; S->Cards.AddUnique(AntCard::Padding); }   // (soft packing: the training's loads do not shatter)
	if (AAntRaid* R = AAntRaid::Get(W)) { R->bCakeKnown = false; }
	if (GM) { GM->MatchLength = 99999.f; }
	// A quiet flat at night: the family asleep (the Giant dozes on the sofa), the red clan at home.
	Command(TEXT("routine night"));
	Command(TEXT("routine goto h_nap"));
	for (const TCHAR* Who : { TEXT("cat"), TEXT("dog"), TEXT("gran"), TEXT("kid") }) { Command(FString::Printf(TEXT("npcsleep %s"), Who)); }
	PurgeStrays();
	// No starter trail, no squad: each comes in its own beat.
	if (AAntTrailNet* T = AAntTrailNet::Get(W)) { T->Clear(); }
	AntRts::DespawnAll(W);
	if (Mode == AntTut::Sandbox)
	{
		Command(TEXT("routine on"));
		UE_LOG(LogTemp, Display, TEXT("ANTTUT sandbox started: players %d"), Players.Num());
		if (GM) { GM->Broadcast(AntTut::Tx(TEXT("ПЕСОЧНИЦА: F2 еда · F3 Гигант вкл/выкл · F4 время суток · F5 цепочка · F6 вода · F7 сброс"), TEXT("SANDBOX: F2 food · F3 Giant on/off · F4 time of day · F5 chain · F6 water · F7 reset")), FLinearColor(1.f, 0.8f, 0.3f)); }
		return;
	}
	SetGod(true);
	UE_LOG(LogTemp, Display, TEXT("ANTTUT start: %d players, %d beats, from beat %d"), Players.Num(), AntTut::NumBeats(), StartBeat);
	Telemetry(TEXT("start"));
	GoToBeat(StartBeat);
}

void AAntTutorial::GoToBeat(int32 N)
{
	N = FMath::Clamp(N, 0, AntTut::NumBeats() - 1);
	if (bStarted && N != BeatNo) { Cleanup(BeatNo); }
	const bool bChapter = AntTut::Beat(N).Chapter != AntTut::Beat(BeatNo).Chapter || N == StartBeat;
	BeatNo = (uint8)N;
	bStaged = false;
	Phase = Fade;
	PhaseStart = ServerNow();
	FadeAt = PhaseStart + (bChapter && N > 0 ? 0.9f : N > 0 ? 0.5f : 0.05f);   // (a short black blink hides the staging teleport)
	LineQueue.Reset();
	LineEnds = PhaseStart;
	LineNo = -1;
	for (FAntTutPlayer& P : Players)
	{
		P.Count.Init(0, AntTut::Beat(N).Steps.Num());
		P.SprintFor = P.CeilingFor = P.SyncFor = P.WaterFor = P.Walked = P.Aux = P.DryFor = 0.f;
		P.AuxN = 0;
		P.LastStepSaid = -1;
		P.bWasInNose = false;
		if (AAntCharacter* A = AntOf(P)) { P.LastPos = A->GetActorLocation(); }
	}
}

void AAntTutorial::Stage(int32 N)
{
	bStaged = true;
	Phase = Intro;
	PhaseStart = ServerNow();
	BeatClock = 0.f;
	AutoClock = 0.f;
	AutoStep = 0;
	ShotMask = 0;
	Fired = 0;
	bOffer = false;
	TimerEnds = 0.f;
	PoligonEnds = 0.f;
	BeatRetries = 0;
	BeatAssists = 0;
	bSkipped = false;
	NextHintAt = AntTut::Beat(N).Par * 0.6f;
	LastProgressAt = ServerNow();
	BeatStartAt = ServerNow();
	Marks.Reset();
	Target = nullptr;
	UWorld* W = GetWorld();
	auto Mark = [this](const FVector& At, const TCHAR* Ru, const TCHAR* En, uint8 Kind = 0) { FAntTutMark M; M.At = At; M.Label = AntTut::Tx(Ru, En); M.Kind = Kind; Marks.Add(M); };
	PurgeStrays();
	UE_LOG(LogTemp, Display, TEXT("ANTTUT beat %d «%s» staged (players %d)"), N, AntTut::Beat(N).Id, Players.Num());
	Telemetry(TEXT("stage"));

	switch (N)
	{
	case BPrologue:
		TeleportAll(PStart, 0.f);
		Say(LRadio1); Say(LRadio2); Say(LSerg1); Say(LSerg2);
		break;
	case BMove:
		if (BeatNo == StartBeat) { TeleportAll(PStart, 0.f); }
		Mark(PWalk, TEXT("Сюда"), TEXT("Here"));
		Mark(PWall, TEXT("Стена: вверх"), TEXT("Wall: up"));
		Mark(PShelfUnder, TEXT("Под полкой"), TEXT("Under the shelf"));
		break;
	case BCombat:
	{
		// The dummy: a red ant held in silk (its pawn comes a tick after the bot: Check() catches it); the sugar cube to spit at.
		if (GM) { GM->AddBots(1, 1); }
		Fired |= 16;
		SpawnFood(NSugar, PSugarTarget);
		Mark(PDummy, TEXT("Чучело"), TEXT("Dummy"), 2);
		Mark(PSugarTarget, TEXT("Мишень"), TEXT("Target"), 3);
		break;
	}
	case BCarry:
		for (int32 i = 0; i < FMath::Max(1, Players.Num()); ++i) { SpawnFood(NCrumb, PCrumb + FVector(90.f * i, 0.f, 0.f)); }
		Mark(PCrumb, TEXT("Крошка"), TEXT("Crumb"), 3);
		Mark(PStart - FVector(450.f, 0.f, 0.f), TEXT("Гнездо"), TEXT("Nest"));
		break;
	case BTeam:
		for (FAntTutPlayer& P : Players) { AntRts::Spawn(P.PS, 3, 0); }
		for (int32 i = 0; i < FMath::Max(1, (Players.Num() + 1) / 2); ++i) { Target = SpawnFood(NChoc, PChoc + FVector(0.f, 160.f * i, 0.f)); }
		Mark(PChoc, TEXT("Шоколад"), TEXT("Chocolate"), 3);
		break;
	case BSnow:
		for (int32 i = 0; i < FMath::Max(1, Players.Num()); ++i) { SpawnFood(NSugar, PSugar + FVector(110.f * i, 0.f, 0.f)); }
		Mark(PSugar, TEXT("Сахар"), TEXT("Sugar"), 3);
		break;
	case BTrail:
	{
		AAntSnowball* Snow = AAntSnowball::Get(W);
		for (FAntTutPlayer& P : Players) { if (Snow) { if (FAntSnowPlayer* SP = Snow->For(P.PS)) { SP->Workers = FMath::Max(SP->Workers, 16); } } }
		for (int32 i = 0; i < 8; ++i) { SpawnFood(NCrumb, PPile + FVector(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), 0.f)); }
		if (AAntTrailNet* T = AAntTrailNet::Get(W, true)) { BaseFerried = T->Ferried; }
		Mark(PStart - FVector(350.f, 0.f, 0.f), TEXT("[T] начало"), TEXT("[T] start"));
		Mark(PPile, TEXT("[T] крошки"), TEXT("[T] crumbs"), 3);
		break;
	}
	case BRts:
		// The order is given by looking at the cheese: an ant still in the nest (home from the trail) is put out on the kitchen
		// floor facing it, or it never sees what it is told to aim at.
		for (FAntTutPlayer& P : Players)
		{
			AAntCharacter* A = AntOf(P);
			if (!A || !A->IsInNest()) { continue; }
			A->LetGo();
			A->ApplyTeleport(PStart);
			if (APlayerController* PC = Cast<APlayerController>(A->GetController())) { PC->ClientSetRotation(FRotator(-8.f, (PCheese - PStart).Rotation().Yaw, 0.f)); }
		}
		for (FAntTutPlayer& P : Players) { AntRts::Spawn(P.PS, 4, 0); }
		Target = SpawnFood(NCheese, PCheese);
		Mark(PCheese, TEXT("Сыр"), TEXT("Cheese"), 3);
		break;
	case BFast:
	{
		// A partner at the screws (one of the squad), the shelf's pickle side. The players are put on the wall by the screws
		// (no long climb: the climb was the first beat's lesson).
		Mark(PScrew, TEXT("Шурупы"), TEXT("Screws"));
		TeleportAll(PScrew + FVector(30.f, -150.f, 0.f), 0.f, true);
		int32 k = 0;
		for (TActorIterator<AAntCharacter> It(W); It && k < 2; ++It)
		{
			if (AntSnowball::SquadOwnerOf(*It) && !It->bDead) { It->LetGo(); It->ApplyTeleport(PScrew + FVector(30.f, -60.f + 120.f * k, 0.f)); ++k; }
		}
		break;
	}
	case BCloth:
		// The edge where the cloth ends on the floor on the near side (its bounds: the layout's tablecloth differs by seed).
		ClothEdge = PClothEdge;
		if (AAntCloth* C = Tablecloth(W)) { const FBox Bx = C->GetComponentsBoundingBox(true); if (Bx.IsValid) { ClothEdge = FVector(Bx.GetCenter().X, Bx.Min.Y + 25.f, 20.f); } }
		Mark(ClothEdge, TEXT("Край скатерти"), TEXT("Tablecloth edge"));
		TeleportAll(ClothEdge + FVector(0.f, -450.f, 30.f), 90.f);
		UE_LOG(LogTemp, Display, TEXT("ANTTUT the tablecloth edge at %s"), *ClothEdge.ToCompactString());
		break;
	case BWater:
	{
		Mark(AKitchenFaucet::ColdValve(), TEXT("Кран (вдвоём)"), TEXT("Tap (two ants)"));
		TeleportAll(AKitchenFaucet::ColdValve() + FVector(0.f, -120.f, 30.f), 90.f);
		break;
	}
	case BChains:
#if ANT_TUT_CHAINS
		{
			// Five stations, each one a prop of the chains branch the team goes to (what "rx tp <prop>" does) and sets off together;
			// the squad is the helping hands the props want (2-3 ants).
			if (AAntReactNet* Net = AAntReactNet::Get(W)) { Net->ChainStart.Reset(); Net->SetPower(true, TEXT("training"), FVector::ZeroVector); }
			EnsureSquad(3);
			Fired = 0;
			GoStation(0);
		}
#else
		{
			Mark(PRod, TEXT("Карниз"), TEXT("Curtain rod"));
			Mark(PCola, TEXT("Кола"), TEXT("Cola"));
		}
#endif
		break;
	case BInside:
	{
		Command(TEXT("routine goto h_nap"));
		TeleportAll(PSofaFloor, 90.f);
		SetGod(true);
		FVector Ear;
		if (SpotOfHuman(TheHuman(W), TEXT("ухо"), Ear)) { Mark(Ear, TEXT("Ухо"), TEXT("Ear")); }
		break;
	}
	case BWake:
		SetGod(false);
		if (GM) { GM->SetHumanAwake(true); }
		Command(TEXT("routine on"));
		Command(TEXT("rage 1"));
		Mark(PCover, TEXT("Укрытие"), TEXT("Cover"), 1);
		break;
	case BFinale:
		SetGod(false);
		TeleportAll(PFinaleStart, 0.f);
		for (FAntTutPlayer& P : Players) { AntRts::Spawn(P.PS, 5, 0); }
		Target = SpawnFood(NCake, PCake);
		// The squad waits at the cake (a team that has to look for its squad first loses the light).
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && AntSnowball::SquadOwnerOf(*It)) { It->LetGo(); It->ApplyTeleport(PCake + FVector(FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(-160.f, 160.f), 20.f)); } }
		Command(TEXT("npcwake dog"));
		Command(TEXT("routine goto h_fridge"));
		if (GM) { GM->SetHumanAwake(true); }
		TimerEnds = ServerNow() + 150.f + 30.f * FMath::Max(0, Players.Num() - 1);
		Mark(PCake, TEXT("Торт"), TEXT("Cake"), 3);
		Mark(PStart - FVector(450.f, 0.f, 0.f), TEXT("Гнездо"), TEXT("Nest"));
		break;
	case BDebrief:
		SetGod(true);
		Phase = Debrief;
		DebriefPage = 0;
		Say(LDebrief);
		TotalSeconds = FMath::RoundToInt(ServerNow() - StartedAt);
#if ANT_TUT_FIGHTER
		// The rank (Lance Corporal: the first perk tier opens at rank 1) and the medal «Пережил Розовую Плиту» go to every player's profile.
		for (FAntTutPlayer& P : Players)
		{
			AntFighter::AwardRank(P.PS, 1);
			AntFighter::AwardMedalKey(P.PS, TEXT("slipper"));
		}
#endif
		break;
	default:
		break;
	}
	if (N != BChains && N != BDebrief) { if (const int32 L = StepLine(N, 0); L >= 0) { Say(L, 0); } }
	for (FAntTutPlayer& P : Players) { P.LastStepSaid = 0; }
}

void AAntTutorial::PurgeStrays()
{
	int32 N = 0;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (A->IsPlayerControlled() || Spawned.Contains(A)) { continue; }
		const AController* C = A->GetController();
		const bool bSquad = C && C->IsA<AAntSquadBot>();
		if (A->Creature == 1 || A->Team == 1 || (A->Team == 0 && A->Creature == 0 && !bSquad && C && C->IsA<AAntBotController>()))
		{
			if (AController* Ctl = A->GetController()) { Ctl->UnPossess(); Ctl->Destroy(); }
			A->Destroy();
			++N;
		}
	}
	if (N > 0) { UE_LOG(LogTemp, Display, TEXT("ANTTUT purged %d strays (flies, red ants, stand-in bots)"), N); }
}

void AAntTutorial::Cleanup(int32 N)
{
	UWorld* W = GetWorld();
	switch (N)
	{
	case BCarry:
		// Co-op: a partner still carrying a crumb when the beat closes kept it into «Раз-два, взяли!» and could not grab the
		// chocolate (packaged co-op run 26.11). The crumbs of the beat go.
		for (FAntTutPlayer& P : Players)
		{
			AAntCharacter* A = AntOf(P);
			AKitchenLoot* L = nullptr;
			if (A) { L = A->Carrying; }
			if (L && L->LootName == NCrumb) { A->LetGo(); L->Destroy(); UE_LOG(LogTemp, Display, TEXT("ANTTUT %s still carried a crumb: dropped"), P.PS ? *P.PS->GetPlayerName() : TEXT("?")); }
		}
		break;
	case BCombat:
		for (const TWeakObjectPtr<AActor>& A : Spawned) { if (AAntCharacter* C = Cast<AAntCharacter>(A.Get())) { C->Destroy(); } }
		for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (It->Creature == 1 && !It->IsPlayerControlled()) { It->Destroy(); } }   // the fly
		break;
	case BWater:
		Command(TEXT("fluid tap off"));
		Command(TEXT("fluid dry"));
		break;
	case BSnow:
		// No card pick may pop up over the lessons after the snowball's own: drop what is on offer, count the levels as picked.
		if (AAntSnowball* Snow = AAntSnowball::Get(W))
		{
			for (FAntTutPlayer& P : Players) { if (FAntSnowPlayer* SP = Snow->For(P.PS)) { SP->Offer.Reset(); SP->PicksDone = (uint8)FMath::Clamp(Snow->Level, 0, 255); } }
		}
		break;
	case BChains:
		if (AntTut::BranchMerged(AntTut::Chains)) { Command(TEXT("power on")); }
		break;
	case BInside:
		for (FAntTutPlayer& P : Players) { if (AAntCharacter* A = AntOf(P)) { if (AntInside::IsInside(A)) { Command(TEXT("inside out")); } } }
		break;
	default:
		break;
	}
	Spawned.RemoveAll([](const TWeakObjectPtr<AActor>& A) { return !A.IsValid() || Cast<AAntCharacter>(A.Get()) != nullptr; });
	Marks.Reset();
	TimerEnds = 0.f;
	bOffer = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Server: is the beat done?
// ---------------------------------------------------------------------------------------------------------------------

void AAntTutorial::PlayerStep(FAntTutPlayer& P, int32 Step, int32 Value, bool bSet)
{
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	if (!B.Steps.IsValidIndex(Step) || !P.Count.IsValidIndex(Step)) { return; }
	const int32 Need = B.Steps[Step].Need;
	const int32 Was = P.Count[Step];
	const int32 V = FMath::Clamp(bSet ? Value : Was + Value, 0, Need);
	if (V == Was) { return; }
	P.Count[Step] = (uint8)V;
	LastProgressAt = ServerNow();
	if (V >= Need)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTTUT step done: beat %d «%s» step %d «%s» by %s at %.1f s"), (int32)BeatNo, B.Id, Step, B.Steps[Step].Ru, P.PS ? *P.PS->GetPlayerName() : TEXT("?"), BeatClock);
		if (APlayerController* PC = P.PS ? Cast<APlayerController>(P.PS->GetOwner()) : nullptr)
		{
			if (AAntCharacter* A = Cast<AAntCharacter>(PC->GetPawn())) { AntAudio::Everyone(this, EAntSfx::DeliverChime, A->GetActorLocation(), 0.6f); }
		}
	}
}

void AAntTutorial::TeamStep(int32 Step, int32 Value, bool bSet)
{
	for (FAntTutPlayer& P : Players) { PlayerStep(P, Step, Value, bSet); }
}

void AAntTutorial::OnNote(uint8 Event, AActor* By, AActor* What)
{
	if (Phase != Play && !(Phase == Intro && BeatNo != BPrologue)) { return; }
	AAntCharacter* Ant = Cast<AAntCharacter>(By);
	FAntTutPlayer* Who = nullptr;
	for (FAntTutPlayer& P : Players) { if (Ant && AntOf(P) == Ant) { Who = &P; } }
	const AKitchenLoot* L = Cast<AKitchenLoot>(What);
	switch (BeatNo)
	{
	case BMove:
		if (Event == AntTut::EvDash && Who) { PlayerStep(*Who, 5, 1); }
		break;
	case BCombat:
		if (Event == AntTut::EvBite && Who && Ant)
		{
			for (const TWeakObjectPtr<AActor>& D : Spawned) { if (D.IsValid() && Cast<AAntCharacter>(D.Get()) && FVector::Dist(D->GetActorLocation(), Ant->GetActorLocation()) < 160.f) { PlayerStep(*Who, 0, 1); break; } }
		}
		if (Event == AntTut::EvSpit && Who) { PlayerStep(*Who, 1, 1); }
		if (Event == AntTut::EvKill && What && Cast<AAntCharacter>(What) && Cast<AAntCharacter>(What)->Creature == 1) { TeamStep(2); }
		break;
	case BCarry:
		if (Event == AntTut::EvDeliver && L)
		{
			// Whoever carried it (or, with nobody, every player: the crumb went home).
			bool bAny = false;
			for (FAntTutPlayer& P : Players) { if (Ant && AntOf(P) == Ant) { PlayerStep(P, 1, 1); bAny = true; } }
			if (!bAny && Who) { PlayerStep(*Who, 1, 1); }
		}
		break;
	case BTeam:
		if (Event == AntTut::EvDeliver && L && L->LootName == NChoc) { TeamStep(2); }
		break;
	case BSnow:
		if (Event == AntTut::EvDeliver && L && L->LootName == NSugar) { TeamStep(0); }
		if (Event == AntTut::EvPick && Who) { PlayerStep(*Who, 1, 1); }
		break;
	case BRts:
		if (Event == AntTut::EvDeliver && L && L->LootName == NCheese) { TeamStep(2); }
		break;
	case BFinale:
		if (Event == AntTut::EvDeliver && L && L->LootName.Contains(NCake)) { TeamStep(1); }
		break;
	default:
		break;
	}
}

void AAntTutorial::Check(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = ServerNow();
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	AAntSnowball* Snow = AAntSnowball::Get(W);
	AAntTrailNet* Trails = AAntTrailNet::Get(W);
	UAntFluidWorld* Fluid = UAntFluidWorld::Get(this);

	for (FAntTutPlayer& P : Players)
	{
		AAntCharacter* A = AntOf(P);
		if (!A) { continue; }
		// Death: back to the last checkpoint (a training run never ends on a death).
		if (A->bDead)
		{
			P.DeadFor += Dt;
			if (P.DeadFor > 3.f)
			{
				P.DeadFor = 0.f;
				if (AController* C = A->GetController()) { if (GM) { GM->RestartPlayer(C); } }
				if (AAntCharacter* A2 = AntOf(P)) { if (!A2->bDead) { A2->ApplyTeleport(Checkpoint()); } }
				++Retries; ++BeatRetries;
				Say(LDeath);
				Telemetry(TEXT("death"));
			}
			continue;
		}
		P.DeadFor = 0.f;
		const FVector Pos = A->GetActorLocation();
		const UAntMovementComponent* M = A->GetAntMovement();
		const FVector N = M ? M->GetSurfaceNormal() : FVector::UpVector;
		const bool bAir = A->GetCharacterMovement() && A->GetCharacterMovement()->IsFalling();
		switch (BeatNo)
		{
		case BMove:
			if (FVector::Dist2D(Pos, PWalk) < 320.f) { PlayerStep(P, 0, 1); }
			P.SprintFor = (A->bSprinting && A->GetVelocity().Size() > 330.f) ? P.SprintFor + Dt : 0.f;
			if (P.SprintFor > 1.2f) { PlayerStep(P, 1, 1); }
			if (bAir && !P.bWasAir && A->GetVelocity().Z > 60.f && !A->IsAttached()) { PlayerStep(P, 2, 1); }
			P.bWasAir = bAir;
			if (A->IsClimbing() && N.Z < 0.35f && N.Z > -0.35f && Pos.Z > 350.f) { PlayerStep(P, 3, 1); }
			P.CeilingFor = (A->IsClimbing() && N.Z < -0.55f && Pos.Z > 600.f) ? P.CeilingFor + Dt : 0.f;
			if (P.CeilingFor > 0.8f) { PlayerStep(P, 4, 1); }
			break;
		case BCombat:
			// The drip: once the fly is down, drops fall where the ant is going; eight dodged (or survived) is enough.
			if (P.Count.IsValidIndex(2) && P.Count[2] >= 1 && P.Count[3] < 1)
			{
				P.Aux += Dt;
				if (P.Aux > 1.1f && Fluid)
				{
					P.Aux = 0.f;
					const FVector Lead = Pos + A->GetVelocity() * 0.55f;
					Fluid->AddDrop(Lead + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 650.f), 26.f, AntFluid::Water, FVector(0.f, 0.f, -300.f));
					++P.AuxN;
					if (P.AuxN >= 8) { PlayerStep(P, 3, 1); }
				}
			}
			break;
		case BCarry:
			if (A->Carrying && A->Carrying->LootName == NCrumb) { PlayerStep(P, 0, 1); }
			if (A->IsInNest() && P.Count[0] >= 1 && !A->Carrying && P.Count[1] < 1 && BeatClock > 3.f) {}
			break;
		case BTeam:
			if (A->Carrying && A->Carrying->LootName == NChoc)
			{
				PlayerStep(P, 0, 1);
				FAntCarryFeel F;
				const bool bFeel = AntCarry::Feel(A, F);
				P.SyncFor = (bFeel && F.Carriers >= 2 && F.Sync > 0.7f && A->GetVelocity().Size2D() > 40.f) ? P.SyncFor + Dt : FMath::Max(0.f, P.SyncFor - Dt * 0.5f);
				if (P.SyncFor > 2.5f || (bFeel && F.bRhythm && A->GetVelocity().Size2D() > 40.f && P.SyncFor > 1.f)) { PlayerStep(P, 1, 1); }
			}
			break;
		case BSnow:
			if (const FAntSnowPlayer* SP = Snow ? Snow->Find(P.PS) : nullptr) { if (SP->PicksDone >= 1 || SP->Taken.Num() >= 1) { PlayerStep(P, 1, 1); } }
			break;
		case BTrail:
			if (Trails)
			{
				for (const FAntTrailLine& L : Trails->Lines)
				{
					if (L.Owner != P.PS) { continue; }
					PlayerStep(P, 0, 1);
					if (!L.bLaying && L.Points.Num() > 1 && FVector::Dist2D(L.Points.Last(), PPile) < 1300.f) { PlayerStep(P, 1, 1); }
				}
				const int32 Ferried = Trails->Ferried - BaseFerried;
				TeamStep(2, Ferried, true);
			}
			break;
		case BRts:
			if (const FAntSnowPlayer* SP = Snow ? Snow->Find(P.PS) : nullptr)
			{
				if (SP->Selected.Num() >= 2) { PlayerStep(P, 0, 1); }
				for (const FAntRtsTask& T : SP->Tasks) { if (T.Order == AntTask::Haul && T.Target == Target.Get()) { PlayerStep(P, 0, 1); PlayerStep(P, 1, 1); } }
			}
			// The order step ([Q]) went unnoticed at the 24.11 playtest: with no order 8 s into it, the sergeant says it again
			// (then every 14 s); the HUD dims round the squad panel at the same time (QOrderCard).
			if (P.Count.IsValidIndex(1) && P.Count[0] >= 1 && P.Count[1] < 1)
			{
				P.Aux += Dt;
				if (P.Aux > 8.f)
				{
					P.Aux = -6.f;
					if (&P == &Players[0] && LineQueue.Num() == 0) { Say(LRtsOrder, 1, true); UE_LOG(LogTemp, Display, TEXT("ANTTUT the squad order is said again (no [Q] yet)")); }
				}
			}
			break;
		case BFast:
			if (A->Pulling && Cast<AKitchenFastener>(A->Pulling))
			{
				P.Aux += Dt;
				if (P.Aux > 2.5f)
				{
					// The partner pulls with you: the screw gives.
					P.Aux = 0.f;
					Cast<AKitchenFastener>(A->Pulling)->ForceDone(A);
				}
			}
			if (A->HasStatus(AntStatus::Sticky) || ((Fired & 4) && Marks.IsValidIndex(0) && FVector::Dist(Pos, Marks[0].At) < 260.f)) { PlayerStep(P, 1, 1); }
			if ((Fired & 4) && Marks.IsValidIndex(1) && FVector::Dist(Pos, Marks[1].At) < 260.f) { PlayerStep(P, 2, 1); }
			if ((A->HasStatus(AntStatus::Wet) || LevelV3::PlayZoneAt(1, Pos) || (Fluid && Fluid->IsSlippery(Pos))) && (Fired & 2)) { PlayerStep(P, 2, 1); }
			break;
		case BCloth:
			// [E] on the hem; standing right at the marked edge for 2 s counts too (the hem lies in folds: the prompt shows where it is)
			P.Aux = FVector::Dist2D(Pos, ClothEdge) < 220.f && Pos.Z < 200.f ? P.Aux + Dt : 0.f;
			if ((A->Pulling && IsTablecloth(A->Pulling)) || P.Aux > 2.f) { PlayerStep(P, 0, 1); }
			break;
		case BWater:
			P.WaterFor = (Fluid && (Fluid->DepthAt(Pos) > 6.f || A->HasStatus(AntStatus::Stream) || A->HasStatus(AntStatus::Drowning))) ? P.WaterFor + Dt : P.WaterFor;
			if (P.WaterFor > 2.5f) { PlayerStep(P, 1, 1); }
			break;
		case BChains:
#if ANT_TUT_CHAINS
			// [E] held on a prop: the squad leans in with you (two or three ants are what the props want) after a moment.
			if (AAntReactProp* Prop = A->Pulling ? Cast<AAntReactProp>(A->Pulling) : nullptr)
			{
				P.Aux += Dt;
				if (P.Aux > 2.2f && !Prop->bDone)
				{
					P.Aux = 0.f;
					UE_LOG(LogTemp, Display, TEXT("ANTTUT the squad leans in on %s"), AntProp::Name(Prop->Kind));
					Prop->ForceDone(A);
				}
			}
			else { P.Aux = 0.f; }
#endif
			break;
		case BInside:
		{
			const FString Place = InsidePlace(A);
			if (A->GetRiddenHuman() || !Place.IsEmpty()) { PlayerStep(P, 0, 1); }
			if (Place == TEXT("ухо")) { PlayerStep(P, 1, 1); }
			if (Place == TEXT("нос")) { P.bWasInNose = true; }
			else if (P.bWasInNose && Place.IsEmpty()) { PlayerStep(P, 2, 1); }
			break;
		}
		case BWake:
			if (FVector::Dist2D(Pos, PCover) < 520.f && Pos.Z < 400.f) { PlayerStep(P, 0, 1); }
			if ((Fired & 2) && FVector::Dist2D(Pos, PPastDog) < 450.f) { PlayerStep(P, 2, 1); }
			break;
		case BFinale:
			if (A->Carrying && A->Carrying->LootName.Contains(NCake)) { PlayerStep(P, 0, 1); }
			break;
		default:
			break;
		}
		// The host's next step: its line once.
		if (&P == &Players[0])
		{
			int32 First = -1;
			for (int32 s = 0; s < P.Count.Num(); ++s) { if (P.Count[s] < B.Steps[s].Need) { First = s; break; } }
			if (First > 0 && First != P.LastStepSaid)
			{
				P.LastStepSaid = First;
				if (const int32 L = StepLine(BeatNo, First); L >= 0)
				{
					LineQueue.Reset();   // the lines of steps already done are not news any more
					LineEnds = FMath::Min(LineEnds, ServerNow() + 0.5f);
					Say(L);
				}
			}
		}
	}

	// Staged events of the beat (team side).
	switch (BeatNo)
	{
	case BCombat:
		if (Fired & 16)
		{
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				if (It->Team == 1 && !It->bDead && It->Creature == 0 && !It->IsPlayerControlled())
				{
					Fired &= ~16;
					It->ApplyTeleport(PDummy);
					It->Webbed(900.f);
					Spawned.Add(*It);
					UE_LOG(LogTemp, Display, TEXT("ANTTUT the dummy is up at %s"), *PDummy.ToCompactString());
					break;
				}
			}
		}
		if (!(Fired & 1))
		{
			bool bSpat = false;
			for (const FAntTutPlayer& P : Players) { bSpat |= P.Count.IsValidIndex(1) && P.Count[1] >= 3; }
			if (bSpat)
			{
				Fired |= 1;
				TSet<AAntCharacter*> Before;
				for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (It->Creature == 1) { Before.Add(*It); } }
				Command(TEXT("flyspawn"));
				for (TActorIterator<AAntCharacter> It(W); It; ++It)
				{
					if (It->Creature == 1 && !Before.Contains(*It))
					{
						It->SpeedScale = 0.35f;   // a sleepy training fly
						It->ApplyTeleport(PSugarTarget + FVector(0.f, 0.f, 260.f));
						Fly = *It;
						FlyHealth = It->Health;
						PayoffAt = Now;
						FAntTutMark Mk; Mk.At = It->GetActorLocation(); Mk.Kind = 2; Mk.Label = AntTut::Tx(TEXT("Муха"), TEXT("Fly")); Marks.Add(Mk);
					}
				}
			}
		}
		if (Fly.IsValid() && Marks.Num() > 0) { Marks.Last().At = Fly->GetActorLocation(); }
		if (Fly.IsValid() && !Fly->bDead && (Fired & 1) && !(Fired & 4) && Now - PayoffAt > 15.f)
		{
			Fired |= 4;   // a fair chance: after 15 s the fly gets caught in a web and hangs still
			Fly->Webbed(60.f);
			Fly->ApplyTeleport(PSugarTarget + FVector(160.f, 0.f, 30.f));   // ...and drops by the sugar: an easy shot
			UE_LOG(LogTemp, Display, TEXT("ANTTUT the fly is caught in a web"));
		}
		if ((Fired & 1) && (!Fly.IsValid() || Fly->bDead || Fly->Health < FlyHealth - 1.f) && !(Fired & 2))
		{
			Fired |= 2;
			TeamStep(2);
			if (Fly.IsValid() && !Fly->bDead) { Fly->Destroy(); }
			if (Marks.Num() > 0) { Marks.Pop(); }
		}
		break;
	case BFast:
	{
		int32 Off = 0;
		for (TActorIterator<AKitchenFastener> It(W); It; ++It)
		{
			const TConstArrayView<FLevelV3FastenerDef> D = LevelV3::Fasteners();
			if (It->bDone && D.IsValidIndex(It->Def) && FString(D[It->Def].Id).StartsWith(TEXT("v3_shelf_sc"))) { ++Off; }
		}
		TeamStep(0, Off, true);
		if (Off >= 2 && !(Fired & 1)) { Fired |= 1; Fired |= 0; PayoffAt = Now; }
		if ((Fired & 1) && !(Fired & 2) && Now - PayoffAt > 3.f)
		{
			Fired |= 2;
			for (const TCHAR* Jar : { TEXT("jam_jar"), TEXT("pickles") })
			{
				AKitchenFixture* F = AKitchenFixture::Find(W, Jar);
				if (F && F->State == AKitchenFixture::Fixed) { Command(FString::Printf(TEXT("fix fall %s"), Jar)); }
			}
			Say(LFastFall);
		}
		if ((Fired & 2) && !(Fired & 4) && Now - PayoffAt > 5.5f)
		{
			Fired |= 4;
			Marks.Reset();
			for (const TCHAR* Jar : { TEXT("jam_jar"), TEXT("pickles") })
			{
				if (AKitchenFixture* F = AKitchenFixture::Find(W, Jar))
				{
					FAntTutMark Mk; Mk.At = F->GetActorLocation(); Mk.Kind = 2;
					Mk.Label = FCString::Strcmp(Jar, TEXT("jam_jar")) == 0 ? AntTut::Tx(TEXT("Варенье"), TEXT("Jam")) : AntTut::Tx(TEXT("Рассол"), TEXT("Brine"));
					Marks.Add(Mk);
				}
			}
		}
		break;
	}
	case BCloth:
	{
		bool bGrab = false;
		for (const FAntTutPlayer& P : Players) { bGrab |= P.Count.IsValidIndex(0) && P.Count[0] >= 1; }
		if (bGrab && !(Fired & 1)) { Fired |= 1; Command(TEXT("cloth crew tablecloth 6 0 -1 10")); PayoffAt = Now; }
		if (AAntCloth* C = Tablecloth(W)) { if (C->Displaced() > 700.f) { TeamStep(1); } }
		if ((Fired & 1) && Now - PayoffAt > 14.f && !(Fired & 2)) { Fired |= 2; Command(TEXT("cloth yank tablecloth 0 -1 60 3")); }
		break;
	}
	case BWater:
	{
		bool bTap = false;
		for (TActorIterator<AKitchenFaucet> It(W); It; ++It) { bTap |= It->IsOn(); }
		if (bTap) { TeamStep(0); }
		if (bTap && !(Fired & 1)) { Fired |= 1; Command(TEXT("fluid flood 14 30")); Marks.Reset(); FAntTutMark Mk; Mk.At = PGeyser + FVector(0.f, 900.f, 0.f); Mk.Label = AntTut::Tx(TEXT("Плыви сюда"), TEXT("Swim here")); Marks.Add(Mk); }
		bool bSwum = false;
		for (const FAntTutPlayer& P : Players) { bSwum |= P.Count.IsValidIndex(1) && P.Count[1] >= 1; }
		if (bSwum && !(Fired & 2)) { Fired |= 2; Marks.Reset(); FAntTutMark Mk; Mk.At = PGeyser; Mk.Label = AntTut::Tx(TEXT("Кола + ментос"), TEXT("Cola + mints")); Marks.Add(Mk); }
		if ((Fired & 2) && !(Fired & 4))
		{
			for (const FAntTutPlayer& P : Players)
			{
				const AAntCharacter* A = AntOf(P);
				if (A && FVector::Dist2D(A->GetActorLocation(), PGeyser) < 380.f) { Fired |= 4; PayoffAt = Now; Command(FString::Printf(TEXT("fluid geyser %.0f %.0f %.0f 8"), PGeyser.X, PGeyser.Y, PGeyser.Z)); break; }
			}
		}
		if ((Fired & 4) && Now - PayoffAt > 2.5f) { TeamStep(2); }
		break;
	}
	case BChains:
	{
#if ANT_TUT_CHAINS
		{
			const int32 S = Station;
			if (S < 5 && StationDone(S) && !(Fired & (1 << S)))
			{
				Fired |= 1 << S;
				PayoffAt = Now;
				TeamStep(S);
				if (S == 2) { Command(TEXT("power on")); }   // the light comes back for the rest of the stations
			}
			if (S < 4 && (Fired & (1 << S)) && Now - PayoffAt > 2.5f) { GoStation(S + 1); }
			break;
		}
#endif
		// Reach a marker: its reaction fires (the step is done a moment later, when it has played).
		static const TCHAR* BranchCmds[] = { TEXT("chain C7"), TEXT("chain C8"), TEXT("power short") };
		static const TCHAR* CoreCmds[] = { TEXT("chain C3"), TEXT("chain C4") };
		const bool bBranch = AntTut::BranchMerged(AntTut::Chains);
		for (int32 s = 0; s < Marks.Num() && s < B.Steps.Num(); ++s)
		{
			if (Fired & (1 << s)) { if (Fired & (1 << (s + 8))) { continue; } }
			bool bNear = false;
			for (const FAntTutPlayer& P : Players) { const AAntCharacter* A = AntOf(P); bNear |= A && FVector::Dist2D(A->GetActorLocation(), Marks[s].At) < 700.f; }
			if (bNear && !(Fired & (1 << s))) { Fired |= 1 << s; Command(bBranch ? BranchCmds[s] : CoreCmds[s]); PayoffAt = Now; }
			if ((Fired & (1 << s)) && Now - PayoffAt > 3.f) { Fired |= 1 << (s + 8); TeamStep(s); }
		}
		if (Marks.Num() < B.Steps.Num()) { for (int32 s = Marks.Num(); s < B.Steps.Num(); ++s) { TeamStep(s); } }   // a prop the branch did not build: nothing to show
		break;
	}
	case BInside:
		if (BeatClock > 1.f && !(Fired & 1))
		{
			// The Giant must be on the sofa, dozing: then the routine is held still.
			if (AHumanCharacter* H = TheHuman(W))
			{
				if (FVector::Dist2D(H->GetActorLocation(), FVector(12300.f, -1100.f, 0.f)) < 700.f || BeatClock > 25.f) { Fired |= 1; Command(TEXT("routine off")); }
			}
		}
		for (FAntTutPlayer& P : Players)
		{
			if (P.Count.IsValidIndex(1) && P.Count[1] >= 1 && !(Fired & 2))
			{
				Fired |= 2;
				FVector Nose;
				Marks.Reset();
				if (SpotOfHuman(TheHuman(W), TEXT("нос"), Nose)) { FAntTutMark Mk; Mk.At = Nose; Mk.Label = AntTut::Tx(TEXT("Нос"), TEXT("Nose")); Marks.Add(Mk); }
			}
		}
		break;
	case BWake:
	{
		bool bCovered = false;
		for (const FAntTutPlayer& P : Players) { bCovered |= P.Count.IsValidIndex(0) && P.Count[0] >= 1; }
		if (bCovered && !(Fired & 1)) { Fired |= 1; PayoffAt = Now; TimerEnds = Now + 25.f; }
		if ((Fired & 1) && Now >= TimerEnds && !(Fired & 2))
		{
			Fired |= 2;
			TimerEnds = 0.f;
			TeamStep(1);
			Command(FString::Printf(TEXT("npcshow dog %.0f %.0f %.0f"), PDog.X, PDog.Y, PDog.Z));
			Command(TEXT("npcwake dog"));
			Marks.Reset();
			FAntTutMark Mk; Mk.At = PPastDog; Mk.Label = AntTut::Tx(TEXT("За Бадрика"), TEXT("Past Badrik")); Marks.Add(Mk);
			FAntTutMark Dg; Dg.At = PDog; Dg.Kind = 2; Dg.Label = TEXT("Бадрик"); Marks.Add(Dg);
		}
		break;
	}
	case BFinale:
		if (TimerEnds > 0.f && !(Fired & 1) && TimerEnds - Now < 75.f) { Fired |= 1; Say(LFinaleHalf); }
		if (TimerEnds > 0.f && Now >= TimerEnds)
		{
			// Too slow: from the start of the finale again.
			++Retries; ++BeatRetries;
			Say(LFinaleFail);
			Telemetry(TEXT("fail"));
			if (Target.IsValid()) { Target->Destroy(); }
			Cleanup(BFinale);
			Stage(BFinale);
			Phase = Play;
			return;
		}
		break;
	default:
		break;
	}

	// Done?
	bool bAll = Players.Num() > 0;
	for (const FAntTutPlayer& P : Players)
	{
		for (int32 s = 0; s < B.Steps.Num(); ++s) { bAll &= P.Count.IsValidIndex(s) && P.Count[s] >= B.Steps[s].Need; }
	}
	if (bAll)
	{
		Complete();
		return;
	}
	// Hints, then help on offer (at the par time, or after 40 s with no progress at all: a stuck player is not made to wait).
	const float Idle = ServerNow() - LastProgressAt;
	if (!bOffer && BeatClock > 12.f && (BeatClock > NextHintAt || Idle > 40.f))
	{
		if (BeatClock < B.Par && Idle <= 40.f) { if (LineQueue.Num() == 0) { Say(LHint, -1, true); NextHintAt = B.Par; } }
		else { bOffer = true; Say(LOffer, -1, true); }
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Chains: the five stations
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
#if ANT_TUT_CHAINS
	const uint8 GStationKind[5] = { AntProp::Pepper, AntProp::Flour, AntProp::Breaker, AntProp::Slingshot, AntProp::PeaBowl };
	const TCHAR* GStationChain[5] = { TEXT("C7"), TEXT("C8"), TEXT("power"), TEXT("C10"), TEXT("C11") };
#endif
}

bool AAntTutorial::StationDone(int32 S) const
{
#if ANT_TUT_CHAINS
	const AAntReactNet* Net = AAntReactNet::Get(GetWorld());
	if (!Net || S < 0 || S > 4) { return false; }
	if (S == 2 && !Net->bPower) { return true; }
	const float* T0 = Net->ChainStart.Find(GStationChain[S]);
	return T0 && GetWorld()->GetTimeSeconds() - *T0 > (S == 2 ? 0.5f : 2.5f);   // (let the reaction play a moment before the stamp)
#else
	return true;
#endif
}

void AAntTutorial::GoStation(int32 S)
{
#if ANT_TUT_CHAINS
	UWorld* W = GetWorld();
	S = FMath::Clamp(S, 0, 4);
	Station = (uint8)S;
	StationAt = ServerNow();
	Marks.Reset();
	AAntReactProp* Prop = AAntReactProp::Find(W, GStationKind[S]);
	if (!Prop) { UE_LOG(LogTemp, Warning, TEXT("ANTTUT station %d: no %s prop in this flat"), S, AntProp::Name(GStationKind[S])); TeamStep(S); Fired |= 1 << S; return; }
	const FVector Use = Prop->UsePoint();
	FAntTutMark M;
	M.At = Use; M.Kind = 0;
	M.Label = AntTut::Tx(AntProp::Ru(GStationKind[S]), AntProp::Name(GStationKind[S]));
	Marks.Add(M);
	// The players at the prop (as "rx tp"), the squad around: the slingshot's squad in its pouch (it flies when the band is let go).
	TeleportAll(Use + FVector(0.f, 0.f, 20.f), 0.f);
	const FVector Around = GStationKind[S] == AntProp::Slingshot ? Prop->GetActorTransform().TransformPosition(FVector(-230.f, 0.f, 60.f)) : Use;
	int32 k = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		if (!It->bDead && AntSnowball::SquadOwnerOf(*It))
		{
			It->LetGo();
			It->ApplyTeleport(Around + FVector(70.f * (1 + k % 3), 70.f * (k / 3), 25.f));
			++k;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTTUT station %d: %s at %s (squad %d)"), S, AntProp::Name(GStationKind[S]), *Use.ToCompactString(), k);
	LastProgressAt = ServerNow();
	if (const int32 L = StepLine(BChains, S); L >= 0) { Say(L, S, true); }
#endif
}

void AAntTutorial::EnsureSquad(int32 N)
{
	UWorld* W = GetWorld();
	int32 Have = 0;
	for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (!It->bDead && AntSnowball::SquadOwnerOf(*It)) { ++Have; } }
	if (Have < N && Players.Num() > 0) { AntRts::Spawn(Players[0].PS, N - Have, 0); }
}


// ---------------------------------------------------------------------------------------------------------------------
// Server: payoff, next, the end
// ---------------------------------------------------------------------------------------------------------------------

void AAntTutorial::Complete()
{
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	for (FAntTutPlayer& P : Players)
	{
		for (int32 s = 0; s < B.Steps.Num() && s < P.Count.Num(); ++s) { P.Count[s] = (uint8)B.Steps[s].Need; }
		P.Xp += bSkipped ? B.Xp / 2 : B.Xp;
	}
	TeamXp += bSkipped ? B.Xp / 2 : B.Xp;
	BeatsDoneMask |= 1 << BeatNo;
	Phase = Payoff;
	PhaseStart = ServerNow();
	PayoffAt = PhaseStart;
	bOffer = false;
	TimerEnds = 0.f;
	LineQueue.Reset();
	LineEnds = FMath::Min(LineEnds, ServerNow() + 0.1f);
	if (BeatNo == BSnow)
	{
		if (AAntSnowball* Snow = AAntSnowball::Get(GetWorld()))
		{
			for (FAntTutPlayer& P : Players) { if (FAntSnowPlayer* SP = Snow->For(P.PS)) { SP->Offer.Reset(); SP->PicksDone = (uint8)FMath::Clamp(Snow->Level, 0, 255); } }
		}
	}
	if (const int32 L = PayLine(BeatNo); L >= 0 && !bSkipped) { Say(L); }
	if (BeatNo == BFinale) { Say(LQueen); }
	UE_LOG(LogTemp, Display, TEXT("ANTTUT beat done: %d «%s» in %.1f s (par %.0f), retries %d, assists %d, skipped %d, xp %d"), (int32)BeatNo, B.Id, BeatClock, B.Par, BeatRetries, BeatAssists, bSkipped ? 1 : 0, TeamXp);
	Telemetry(bSkipped ? TEXT("skipped") : TEXT("done"));
	if (BeatNo == BChains && !bSkipped)
	{
		// The free range: what else can fall in this kitchen.
		Phase = Poligon;
		PoligonEnds = ServerNow() + (bAuto ? 8.f : 50.f);
		Say(LPoligon);
		Marks.Reset();
#if ANT_TUT_CHAINS
		// The other props of the chains: toaster, chocolate, ice, sponge, books, balloon, mints, the microwave (rx tp <name>).
		for (const uint8 K : { (uint8)AntProp::ToasterSpring, (uint8)AntProp::Chocolate, (uint8)AntProp::IceTray, (uint8)AntProp::Sponge, (uint8)AntProp::BookRow, (uint8)AntProp::Balloon, (uint8)AntProp::Mentos, (uint8)AntProp::MicroButton })
		{
			if (const AAntReactProp* Pr = AAntReactProp::Find(GetWorld(), K))
			{
				FAntTutMark M; M.At = Pr->UsePoint(); M.Kind = 3; M.Label = AntTut::Tx(AntProp::Ru(K), AntProp::Name(K)); Marks.Add(M);
			}
		}
#else
		for (const TCHAR* Id : { TEXT("cereal"), TEXT("toaster"), TEXT("plant_k"), TEXT("upper_doors") })
		{
			if (AKitchenFixture* F = AKitchenFixture::Find(GetWorld(), Id))
			{
				if (F->State != AKitchenFixture::Fixed) { continue; }
				FAntTutMark M; M.At = F->GetActorLocation(); M.Kind = 3; M.Label = AntTut::Tx(TEXT("Можно уронить"), TEXT("Can fall")); Marks.Add(M);
			}
		}
#endif
	}
}

void AAntTutorial::Next()
{
	if (BeatNo + 1 < AntTut::NumBeats()) { GoToBeat(BeatNo + 1); }
	else { Finish(); }
}

void AAntTutorial::SkipBeat(const TCHAR* Why)
{
	if (Phase == Debrief) { Finish(); return; }
	if (Phase != Play && Phase != Intro) { if (Phase == Poligon) { PoligonEnds = ServerNow(); } return; }
	UE_LOG(LogTemp, Display, TEXT("ANTTUT skip beat %d (%s)"), (int32)BeatNo, Why);
	bSkipped = true;
	Complete();
	PayoffAt -= 3.f;
}

void AAntTutorial::Assist()
{
	UWorld* W = GetWorld();
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	++Assists; ++BeatAssists;
	bOffer = false;
	NextHintAt = BeatClock + B.Par * 0.5f;
	UE_LOG(LogTemp, Display, TEXT("ANTTUT assist on beat %d «%s» at %.1f s"), (int32)BeatNo, B.Id, BeatClock);
	Telemetry(TEXT("assist"));
	// The team's staged part first.
	switch (BeatNo)
	{
	case BCombat: for (TActorIterator<AAntCharacter> It(W); It; ++It) { if (It->Creature == 1 && !It->bDead) { It->Destroy(); } } TeamStep(2); break;
	case BCloth: Command(TEXT("cloth yank tablecloth 0 -1 60 3")); break;
	case BWater:
		if (!(Fired & 1)) { Command(TEXT("fluid tap on 5")); }
		else if (Fired & 2) { TeleportAll(PGeyser, 0.f); }
		break;
	case BChains:
#if ANT_TUT_CHAINS
		// The sergeant's squad does the pulling: the station's prop is set off at once.
		if (Station < 5 && !StationDone(Station))
		{
			if (AAntReactProp* Pr = AAntReactProp::Find(W, GStationKind[Station])) { if (!Pr->bDone) { Pr->ForceDone(Players.Num() > 0 ? AntOf(Players[0]) : nullptr); } }
		}
#else
		for (int32 s = 0; s < Marks.Num(); ++s) { if (!(Fired & (1 << s))) { TeleportAll(Marks[s].At + FVector(0.f, -250.f, 30.f), 90.f); break; } }
#endif
		break;
	case BInside:
		for (FAntTutPlayer& P : Players)
		{
			if (P.Count.IsValidIndex(1) && P.Count[1] < 1) { Command(TEXT("inside put human ear")); }
			else { Command(TEXT("inside put human nose")); Command(TEXT("inside sneeze")); }
			break;   // (the host: the inside commands act on the host's ant)
		}
		break;
	case BWake: if (!(Fired & 1)) { TeleportAll(PCover, 0.f); } else if (Fired & 2) { TeleportAll(PPastDog, 180.f); } break;
	case BTeam: case BRts: case BFinale: case BSnow:
		// The load is put on the doorstep of the nest: the last metres are the team's.
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if ((It->LootName == NChoc && BeatNo == BTeam) || (It->LootName == NCheese && BeatNo == BRts) || (It->LootName == NCake && BeatNo == BFinale) || (It->LootName == NSugar && BeatNo == BSnow))
			{
				It->ReleaseAll();
				It->SetActorLocation(AKitchenBuilder::NestLocation(0) + FVector(1000.f, 0.f, 60.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		if (BeatNo == BFinale && TimerEnds > 0.f) { TimerEnds += 30.f; }
		break;
	default: break;
	}
	// Then whatever step each player is stuck on (not the team's: those are earned).
	for (FAntTutPlayer& P : Players)
	{
		for (int32 s = 0; s < B.Steps.Num(); ++s)
		{
			if (P.Count[s] >= B.Steps[s].Need) { continue; }
			if (!B.Steps[s].bTeam || BeatAssists >= 2) { PlayerStep(P, s, B.Steps[s].Need, true); }
			break;
		}
	}
}

void AAntTutorial::DebriefAdvance()
{
	if (Phase != Debrief) { return; }
	if (DebriefPage >= 3) { Finish(); return; }
	++DebriefPage;
	LineQueue.Reset();
	LineEnds = ServerNow();
	BeatClock = 0.f;
	PhaseStart = ServerNow();
	UE_LOG(LogTemp, Display, TEXT("ANTTUT debrief page %d"), (int32)DebriefPage);
	if (DebriefPage == 1) { Say(LQueen); Say(LGift); }
	if (DebriefPage == 2)
	{
		Say(LPerk);
#if ANT_TUT_FIGHTER
		PerkAt = ServerNow() + 1.0f;   // the profile opens at the caste perks (one tier is open at rank 1)
#endif
	}
	if (DebriefPage == 3) { Say(LVersus); }
}

void AAntTutorial::Enter(APlayerController* PC)
{
	const bool bHost = PC && PC->IsLocalController();
	if (!bHost) { return; }
	if (Phase == Debrief) { DebriefAdvance(); return; }
	if (Phase == Poligon) { PoligonEnds = ServerNow(); return; }
	if (bOffer) { Assist(); return; }
	LineEnds = ServerNow();   // the current line is cut short
}

void AAntTutorial::Finish()
{
	if (Phase == Done) { return; }
	Phase = Done;
	PhaseStart = ServerNow();
	TotalSeconds = FMath::RoundToInt(ServerNow() - StartedAt);
	UE_LOG(LogTemp, Display, TEXT("ANTTUT finished: %d s, xp %d, retries %d, assists %d, beats %x"), TotalSeconds, TeamXp, (int32)Retries, (int32)Assists, BeatsDoneMask);
	Telemetry(TEXT("finish"));
	if (FParse::Param(FCommandLine::Get(), TEXT("AntTutQuit")))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([]() { UE_LOG(LogTemp, Display, TEXT("ANTTUT quit (-AntTutQuit)")); FPlatformMisc::RequestExit(false); }), 8.f, false);
	}
	for (FAntTutPlayer& P : Players)
	{
		(void)P;
	}
#if ANT_TUT_METABUS
	{
		FAntRaidSummary S;
		S.RaidNo = 0;
		S.Tier = 1;
		S.End = 1;
		S.bCakeHome = true;
		for (const FAntTutPlayer& P : Players)
		{
			FAntRaidPlayerLine L;
			L.Name = P.PS ? P.PS->GetPlayerName() : FString();
			L.Id = L.Name;
			L.bCameHome = true;
			L.Feats.Add(FName(TEXT("tutorial_done")));
			L.Feats.Add(FName(TEXT("survived")));
			S.Players.Add(L);
		}
		AntMetaBus::OnRaidSummary().Broadcast(S);
	}
#endif
}

void AAntTutorial::Telemetry(const TCHAR* What)
{
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	const FString Row = FString::Printf(TEXT("%s,%s,%d,%s,%s,%.1f,%.0f,%d,%d,%d,%d,%d"), *Session, *FDateTime::Now().ToString(TEXT("%H:%M:%S")), (int32)BeatNo, B.Id, What, BeatClock, B.Par,
		BeatRetries, BeatAssists, bSkipped ? 1 : 0, Players.Num(), TeamXp);
	UE_LOG(LogTemp, Display, TEXT("ANTTUT telemetry %s"), *Row);
	const FString Dir = FPaths::ProjectSavedDir() / (AntTut::IsDemo() ? TEXT("Demo") : TEXT("Tutorial"));
	const FString Path = Dir / TEXT("telemetry.csv");
	IFileManager::Get().MakeDirectory(*Dir, true);
	if (!IFileManager::Get().FileExists(*Path))
	{
		FFileHelper::SaveStringToFile(TEXT("session,time,beat,id,event,beat_seconds,par,retries,assists,skipped,players,xp\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	FFileHelper::SaveStringToFile(Row + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void AAntTutorial::SandboxTool(APlayerController* PC, int32 Tool)
{
	if (Mode != AntTut::Sandbox || !PC || !PC->IsLocalController()) { return; }   // the host's tools
	AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn());
	const FVector At = Me ? Me->GetActorLocation() + Me->GetActorForwardVector() * 250.f : FVector::ZeroVector;
	static int32 FoodI = 0, Tod = 2, ChainI = 0;
	static bool bFrozen = false, bWet = false;
	switch (Tool)
	{
	case 2: { static const TCHAR* F[] = { TEXT("Кубик сахара"), TEXT("Кусочек сыра"), TEXT("Долька шоколада"), TEXT("Кусочек торта"), TEXT("Хлебная крошка"), TEXT("Мармеладка") }; Command(FString::Printf(TEXT("food %.0f %.0f %.0f %s"), At.X, At.Y, At.Z + 30.f, F[FoodI++ % 6])); break; }
	case 3: bFrozen = !bFrozen; Command(TEXT("freeze")); if (GM) { GM->SetHumanAwake(!bFrozen); } break;
	case 4: Tod = (Tod + 1) % 3; Command(FString::Printf(TEXT("tod %d"), Tod)); break;
	case 5:
	{
		static const TCHAR* C[] = { TEXT("C1"), TEXT("C2"), TEXT("C3"), TEXT("C4"), TEXT("C5"), TEXT("C6") };
		Command(FString::Printf(TEXT("chain %s"), C[ChainI++ % 6]));
		break;
	}
	case 6: bWet = !bWet; Command(bWet ? TEXT("fluid flood 10 30") : TEXT("fluid dry")); break;
	case 7: Command(TEXT("restart")); break;
	default: break;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTTUT sandbox tool F%d"), Tool);
}

// ---------------------------------------------------------------------------------------------------------------------
// Dev: the scripted playthrough (-AntTutAuto / "tut auto 1"). The host's ant is driven through the same server calls the
// keys end in (walk, sprint, jump, dash, spit, bite, grab, the trail, squad orders, [E] on things); what cannot be driven
// headless (a mouse box on screen, a long climb) falls back to the sergeant's help after a while. Every action is logged
// (ANTTUT auto ...), so the log says what was played and what was helped.
// ---------------------------------------------------------------------------------------------------------------------

void AAntTutorial::AutoPlay(float Dt)
{
	for (int32 i = 0; i < Players.Num() && i < 4; ++i) { AutoPlayFor(i, Dt); }
}

void AAntTutorial::AutoPlayFor(int32 Idx, float Dt)
{
	if (!Players.IsValidIndex(Idx) || !GM) { return; }
	const bool bLead = Idx == 0;
	// Co-op: the other players' ants are driven the same way through beats 4-7 (carry, team carry, snowball, trail, squad); in the
	// other beats their own steps are ticked a few seconds in (simulated, logged).
	const bool bTeamBeat = BeatNo == BCarry || BeatNo == BTeam || BeatNo == BSnow || BeatNo == BTrail || BeatNo == BRts;
	if (!bLead && !bTeamBeat) { return; }
	AAntCharacter* Me = AntOf(Players[Idx]);
	APlayerController* PC = Me ? Cast<APlayerController>(Me->GetController()) : nullptr;
	if (!Me || Me->bDead) { return; }
	UWorld* W = GetWorld();
	const float Now = ServerNow();
	const AntTut::FBeat& B = AntTut::Beat(BeatNo);
	FAntTutPlayer& P = Players[Idx];
	if (bLead) { AutoClock += Dt; }
	auto Log = [this](const FString& S) { UE_LOG(LogTemp, Display, TEXT("ANTTUT auto beat %d t=%.1f: %s"), (int32)BeatNo, BeatClock, *S); };
	auto Go = [Me](const FVector& T) { Me->DevGoTo = T; Me->bDevGo = true; };
	auto Stop = [Me]() { Me->bDevGo = false; };
	// Home along the clear strip by the nest (boxes and a bench stand against the wall further north).
	auto GoHome = [Me, &Go]() { const FVector P = Me->GetActorLocation(); Go(P.X > -6300.f && FMath::Abs(P.Y + 2600.f) > 250.f ? FVector(-6100.f, -2620.f, 20.f) : AKitchenBuilder::NestLocation(0) + FVector(250.f, -80.f, 20.f)); };
	auto Done = [&P, &B](int32 s) { return P.Count.IsValidIndex(s) && P.Count[s] >= B.Steps[s].Need; };
	auto Face = [Me, PC](const FVector& T) { if (PC) { PC->SetControlRotation((T - Me->GetActorLocation()).Rotation()); } };
	auto Nearest = [this, W, Me](const TCHAR* Name) -> AKitchenLoot*
	{
		AKitchenLoot* Best = nullptr; float BD = 1.e12f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It) { if (It->LootName == Name && !It->IsCarried() && !It->bHeld) { const float D = FVector::DistSquared(It->GetActorLocation(), Me->GetActorLocation()) * (Spawned.Contains(*It) ? 1.f : 100.f); if (D < BD) { BD = D; Best = *It; } } }
		return Best;
	};
	auto Shot = [this](const TCHAR* Kind)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTTUT shot %02d_%s_%s"), (int32)BeatNo, AntTut::Beat(BeatNo).Id, Kind);
		if (GM) { GM->StageCommand(TEXT("shot")); }
	};
	// Pictures: the prompt of each beat a few seconds in, the payoff.
	if (bLead && Phase == Play && !(ShotMask & 1) && BeatClock > 3.5f) { ShotMask |= 1; Shot(TEXT("prompt")); }
	if (bLead && Phase == Payoff && !(ShotMask & 2) && Now - PayoffAt > 1.2f) { ShotMask |= 2; Shot(TEXT("payoff")); }
	if (bLead && Phase == Debrief)
	{
		const int32 Bit = 4 << DebriefPage;
		if (!(ShotMask & Bit) && BeatClock > 2.5f) { ShotMask |= Bit; Shot(*FString::Printf(TEXT("debrief%d"), (int32)DebriefPage)); }
		if (BeatClock > 5.f) { Log(FString::Printf(TEXT("enter: debrief page %d"), (int32)DebriefPage)); DebriefAdvance(); return; }
	}
	if (bLead && Phase == Intro && BeatNo == BPrologue && AutoClock > 2.f) { AutoClock = 0.f; LineEnds = Now; Log(TEXT("enter: next line")); }
	if (Phase == Poligon && Now > PoligonEnds - 1.f) { return; }
	if (Phase != Play) { return; }

	// Co-op runs: the other players' own steps are ticked a few seconds in (their input is on their machines; the test checks
	// that the beat waits for everyone, replicates the checklists and closes for all). Logged as simulated.
	for (int32 i = 1; i < Players.Num() && bLead && !bTeamBeat; ++i)
	{
		if (BeatClock < 6.f + 3.f * i) { continue; }
		bool bAny = false;
		for (int32 s = 0; s < B.Steps.Num(); ++s)
		{
			if (!B.Steps[s].bTeam && Players[i].Count.IsValidIndex(s) && Players[i].Count[s] < B.Steps[s].Need) { PlayerStep(Players[i], s, B.Steps[s].Need, true); bAny = true; }
		}
		if (bAny) { Log(FString::Printf(TEXT("co-op: player %d's own steps ticked (simulated, %s)"), i, Players[i].PS ? *Players[i].PS->GetPlayerName() : TEXT("?"))); }
	}
	// The sergeant's help after a while (logged: it is not "played").
	if (bLead && BeatClock > FMath::Max(40.f, B.Par * 0.8f) + 30.f * BeatAssists) { Log(TEXT("HELP (sergeant's assist)")); Assist(); return; }

	if (bLead && Now >= AutoLogAt)
	{
		AutoLogAt = Now + 3.f;
		const UAntMovementComponent* M = Me->GetAntMovement();
		Log(FString::Printf(TEXT("at %s n %s climbing %d carrying %s"), *Me->GetActorLocation().ToCompactString(), *(M ? M->GetSurfaceNormal() : FVector::UpVector).ToCompactString(), Me->IsClimbing() ? 1 : 0, Me->Carrying ? *Me->Carrying->LootName : TEXT("-")));
	}
	static float Every[4] = { 0.f, 0.f, 0.f, 0.f };
	Every[Idx] += Dt;
	const bool bTick = Every[Idx] > 0.6f;
	if (bTick) { Every[Idx] = 0.f; }
	switch (BeatNo)
	{
	case BMove:
		if (!Done(0)) { Go(PWalk); }
		else if (!Done(1)) { Me->bSprinting = true; Go(PSprint); }
		else if (!Done(2)) { Me->bSprinting = false; Stop(); if (bTick && !Me->GetCharacterMovement()->IsFalling()) { Me->Jump(); Log(TEXT("jump")); } }
		else if (!Done(5)) { if (bTick) { Me->Dash(); Log(TEXT("dash")); } }
		else if (!Done(3) || !Done(4))
		{
			// Up the wall by the nest, along it to the shelf, under the board: the game's own dev walker ("go", with its detours).
			if (AutoStep < 1) { AutoStep = 1; Stop(); Me->ApplyTeleport(FVector(-6880.f, -3300.f, 30.f)); Log(TEXT("to the foot of the wall")); }
			else if (AutoStep == 1 && BeatClock > 9.f) { AutoStep = 2; GM->DevCommand(TEXT("go -6995 -3300 700 -6995 -400 2300 -6995 -400 2700")); Log(TEXT("walk: up the wall, along it, under the shelf")); }
		}
		break;
	case BCombat:
		if (!Done(0) || !Done(1))
		{
			if (FVector::Dist2D(Me->GetActorLocation(), PDummy) > 120.f) { Me->ApplyTeleport(PDummy + FVector(-60.f, 0.f, 10.f)); Log(TEXT("walk to the dummy (teleport)")); break; }
			if (bTick && !Done(0)) { Face(PDummy); Me->BotBite(); Log(TEXT("bite")); }
			else if (bTick) { Face(PSugarTarget); Me->BotFire((PSugarTarget - Me->GetMuzzleLocation()).GetSafeNormal()); Log(TEXT("spit")); }
		}
		else if (!Done(2))
		{
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				if (It->Creature == 1 && !It->bDead && bTick) { Face(It->GetActorLocation()); Me->BotFire((It->GetActorLocation() - Me->GetMuzzleLocation()).GetSafeNormal()); Log(TEXT("spit at the fly")); }
			}
		}
		else if (!Done(3)) { if (bTick) { Me->Dash(); } Go(PDummy + FVector(600.f * FMath::Sin(BeatClock), 600.f, 0.f)); }
		break;
	case BCarry:
		if (!Me->Carrying) { if (AKitchenLoot* L = Nearest(NCrumb)) { if (FVector::Dist(L->GetActorLocation(), Me->GetActorLocation()) < 60.f || (bTick && BeatClock > 25.f)) { Stop(); if (FVector::Dist(L->GetActorLocation(), Me->GetActorLocation()) > 60.f) { Me->ApplyTeleport(L->GetActorLocation() + FVector(-30.f, 0.f, 20.f)); } if (bTick) { Log(Me->GrabLoot(L) ? TEXT("grab the crumb: ok") : TEXT("grab the crumb: refused")); } } else { Go(L->GetActorLocation()); } } }
		else { GoHome(); }
		break;
	case BTeam: case BFinale: case BSnow:
	{
		const TCHAR* Name = BeatNo == BTeam ? NChoc : BeatNo == BFinale ? NCake : NSugar;
		if (BeatNo == BSnow && Done(0))
		{
			AAntSnowball* Snow = AAntSnowball::Get(W);
			if (Snow && bTick) { if (FAntSnowPlayer* SP = Snow->For(P.PS)) { if (SP->Offer.Num() > 0) { Snow->Pick(P.PS, 0); Log(TEXT("pick card 1")); } else if (SP->PicksDone < 1) { Command(TEXT("picknow")); } } }
			break;
		}
		if (!Me->Carrying) { if (AKitchenLoot* L = Nearest(Name)) { if (FVector::Dist(L->GetActorLocation(), Me->GetActorLocation()) < 70.f || (bTick && BeatClock > 25.f)) { Stop(); if (FVector::Dist(L->GetActorLocation(), Me->GetActorLocation()) > 70.f) { Me->ApplyTeleport(L->GetActorLocation() + FVector(-40.f, 0.f, 20.f)); } if (bTick) { Log(FString::Printf(TEXT("grab %s: %s"), Name, Me->GrabLoot(L) ? TEXT("ok") : TEXT("refused"))); } } else { Go(L->GetActorLocation()); } } }
		else { GoHome(); if (BeatNo == BFinale && BeatClock > 30.f) { P.Aux += Dt; if (P.Aux > 4.f) { P.Aux = 0.f; AKitchenLoot* Ck = Me->Carrying; const FVector Pn = Me->GetActorLocation(); const FVector Dir = (FVector(Me->DevGoTo) - Pn).GetSafeNormal2D(); const FVector To = Pn + Dir * 700.f + FVector(0.f, 0.f, 30.f); Me->LetGo(); if (Ck) { Ck->SetActorLocation(To + FVector(0.f, 0.f, 20.f), false, nullptr, ETeleportType::TeleportPhysics); } Me->ApplyTeleport(To); if (Ck) { Me->GrabLoot(Ck); } Log(TEXT("the walker is stuck with the cake: the squad drags it a few metres towards the nest")); } } }
		break;
	}
	case BTrail:
		if (!Done(1))
		{
			if (AAntTrailNet* T = AAntTrailNet::Get(W, true))
			{
				TArray<FVector> Pts;
				for (int32 i = 0; i <= 10; ++i) { Pts.Add(FMath::Lerp(AKitchenBuilder::NestLocation(0) + FVector(400.f, 0.f, 0.f), PPile, i / 10.f)); }
				T->DevLay(P.PS, Pts);
				Log(TEXT("lay a trail nest -> crumbs ([T] ... [T])"));
			}
		}
		break;
	case BRts:
		if (!Done(1)) { Stop(); }   // (the walk home of the beats before is still on: the ant stays where the beat put it, facing the cheese)
		if (!Done(1) && bTick)
		{
			AAntSnowball* Snow = AAntSnowball::Get(W);
			FAntSnowPlayer* SP = Snow ? Snow->For(P.PS) : nullptr;
			if (SP && Target.IsValid())
			{
				if (!Done(0))
				{
					SP->Selected.Reset();
					for (AAntCharacter* A : SP->Squad) { if (A) { SP->Selected.Add(A); } }
					Log(FString::Printf(TEXT("box-select %d"), SP->Squad.Num()));
				}
				else if (bLead && BeatClock < 18.f)
				{
					// The [Q] step waits like a player who missed it (KAN-33): the order card, then the dim round the squad panel and
					// the sergeant again; pictures of both. Then the real [Q] path (AntSquad::Order -> AntRts: the load near the aim).
					if (!(ShotMask & 64) && BeatClock > 9.5f) { ShotMask |= 64; Shot(TEXT("qcard")); }
					if (!(ShotMask & 128) && BeatClock > 15.5f) { ShotMask |= 128; Shot(TEXT("qdim")); }
					if (BeatClock > 13.f) { Face(Target->GetActorLocation()); }
				}
				else
				{
					Face(Target->GetActorLocation());
					AntSquad::Order(Me, AntSquad::Haul, Target->GetActorLocation());
					Log(FString::Printf(TEXT("[Q] at the cheese (the squad-key path): tasks %d"), SP->Tasks.Num()));
				}
			}
		}
		break;
	case BFast:
		if (!Done(0))
		{
			if (FVector::Dist(Me->GetActorLocation(), PScrew) > 200.f) { Me->ApplyTeleport(PScrew + FVector(25.f, 0.f, 0.f)); Log(TEXT("up to the screws (teleport)")); }
			else if (!Me->Pulling && bTick) { Me->DevInteract(); Log(TEXT("hold [E] on the screw")); }
		}
		else if (Fired & 4)
		{
			if (!Done(1) && Marks.IsValidIndex(0)) { Go(Marks[0].At); if (BeatClock > 50.f) { Me->ApplyTeleport(Marks[0].At + FVector(0.f, 0.f, 30.f)); } }
			else if (!Done(2) && Marks.IsValidIndex(1)) { Go(Marks[1].At); if (BeatClock > 70.f) { Me->ApplyTeleport(Marks[1].At + FVector(0.f, 0.f, 30.f)); } }
		}
		break;
	case BCloth:
		if (!Done(0))
		{
			if (FVector::Dist2D(Me->GetActorLocation(), ClothEdge) > 40.f) { Go(ClothEdge); if (BeatClock > 25.f) { Me->ApplyTeleport(ClothEdge + FVector(0.f, -30.f, 10.f)); } }
			else if (bTick) { Stop(); Face(ClothEdge + FVector(0.f, 300.f, 0.f)); Me->DevInteract(); Log(TEXT("[E] on the tablecloth edge")); }
		}
		break;
	case BWater:
		if (!Done(0))
		{
			if (AutoStep < 2) { AutoStep = 2; Me->ApplyTeleport(AKitchenFaucet::ColdValve() + FVector(0.f, -120.f, 30.f)); Log(TEXT("up to the tap (teleport)")); }
			else if (AutoStep == 2 && BeatClock > 3.f) { AutoStep = 3; Me->DevInteract(); Log(TEXT("[E] on the valve")); }
			else if (AutoStep == 3 && BeatClock > 8.f) { AutoStep = 4; Command(TEXT("fluid tap on 5")); Log(TEXT("the valve is stiff: the squad leans on it (fluid tap on)")); }
		}
		else if (!Done(1)) { if (bTick && AutoStep < 5) { AutoStep = 5; Me->ApplyTeleport(PGeyser + FVector(0.f, 900.f, 40.f)); Log(TEXT("into the flood (teleport)")); } }
		else if (!Done(2)) { Go(PGeyser); }
		break;
	case BChains:
#if ANT_TUT_CHAINS
		if (bTick && !Me->Pulling && !StationDone(Station)) { AAntReactProp* SP = AAntReactProp::Find(W, GStationKind[FMath::Clamp((int32)Station, 0, 4)]); if (SP && FVector::Dist(Me->GetActorLocation(), SP->UsePoint()) > 300.f) { Me->ApplyTeleport(SP->UsePoint() + FVector(0.f, 0.f, 20.f)); Log(TEXT("back to the prop (slipped off)")); } Face(SP ? SP->UsePoint() : (Marks.IsValidIndex(0) ? FVector(Marks[0].At) : Me->GetActorLocation() + FVector(100.f, 0.f, 0.f))); Me->DevInteract(); Log(FString::Printf(TEXT("hold [E] on the station %d prop"), (int32)Station)); }
#else
		for (int32 s = 0; s < Marks.Num(); ++s)
		{
			if (Fired & (1 << s)) { continue; }
			if (bTick && FVector::Dist2D(Me->GetActorLocation(), Marks[s].At) > 600.f) { Me->ApplyTeleport(Marks[s].At + FVector(0.f, -300.f, 40.f)); Log(FString::Printf(TEXT("to %s"), *Marks[s].Label)); }
			break;
		}
#endif
		break;
	case BInside:
		if (!Done(1) && bTick && AutoStep < 4) { AutoStep = 4; Command(TEXT("inside put human ear")); Log(TEXT("into the ear")); }
		else if (Done(1) && AutoStep < 5 && BeatClock > 8.f) { AutoStep = 5; Command(TEXT("inside put human nose")); Log(TEXT("into the nose")); }
		else if (AutoStep == 5 && BeatClock > 11.f) { AutoStep = 6; Command(TEXT("inside sneeze")); Log(TEXT("sneeze")); }
		break;
	case BWake:
		if (!Done(0)) { Go(PCover); if (BeatClock > 20.f) { Me->ApplyTeleport(PCover); } }
		else if (Fired & 2) { Go(PPastDog); if (BeatClock > 70.f) { Me->ApplyTeleport(PPastDog); } }
		break;
	default:
		break;
	}
}

// =====================================================================================================================
// Dev commands
// =====================================================================================================================
namespace AntTut
{
	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (A.Num() == 0 || !GM || A[0].ToLower() != TEXT("tut")) { return false; }
		AAntTutorial* T = AAntTutorial::Get(GM->GetWorld());
		const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
		if (!T) { UE_LOG(LogTemp, Warning, TEXT("ANTTUT no director in this world")); return true; }
		if (Sub == TEXT("skip")) { T->SkipBeat(TEXT("dev")); }
		else if (Sub == TEXT("beat") && A.IsValidIndex(2)) { T->GoToBeat(FCString::Atoi(*A[2])); }
		else if (Sub == TEXT("assist")) { T->Assist(); }
		else if (Sub == TEXT("enter")) { T->Enter(GM->GetWorld()->GetFirstPlayerController()); }
		else if (Sub == TEXT("auto")) { T->bAuto = !A.IsValidIndex(2) || A[2] != TEXT("0"); }
		else if (Sub == TEXT("end")) { T->GoToBeat(NumBeats() - 1); }
		else if (Sub == TEXT("tool") && A.IsValidIndex(2)) { T->SandboxTool(GM->GetWorld()->GetFirstPlayerController(), FCString::Atoi(*A[2])); }   // the sandbox's F2..F7 without a keyboard
		else if (Sub == TEXT("sbx")) { T->SandboxAction(A); }
		else if (Sub == TEXT("kill")) { T->Command(TEXT("die")); }   // the checkpoint test: the host's ant dies, the director puts it back
		else if (Sub == TEXT("holdenter"))
		{
			// The real path of the key: press, a pause, release (OnEnter times it: held 1.2 s = skip the beat).
			APlayerController* PC0 = GM->GetWorld()->GetFirstPlayerController();
			const float Secs = A.IsValidIndex(2) ? FCString::Atof(*A[2]) : 1.4f;
			OnEnter(PC0, true);
			TWeakObjectPtr<APlayerController> WPC = PC0;
			FTimerHandle H;
			GM->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WPC]() { if (WPC.IsValid()) { OnEnter(WPC.Get(), false); } }), FMath::Max(0.05f, Secs), false);
			UE_LOG(LogTemp, Display, TEXT("ANTTUT [Enter] held %.1f s"), Secs);
		}
		else if (Sub == TEXT("panel")) { ToggleSandboxPanel(GM->GetWorld()->GetFirstPlayerController()); }
		else
		{
			UE_LOG(LogTemp, Display, TEXT("ANTTUT info: mode %d beat %d «%s» phase %d players %d xp %d retries %d assists %d marks %d line %d"), (int32)T->Mode, (int32)T->BeatNo, Beat(T->BeatNo).Id,
				(int32)T->Phase, T->Players.Num(), T->TeamXp, (int32)T->Retries, (int32)T->Assists, T->Marks.Num(), (int32)T->LineNo);
			for (const FAntTutMark& M : T->Marks) { UE_LOG(LogTemp, Display, TEXT("ANTTUT  mark %s at %s"), *M.Label, *FVector(M.At).ToCompactString()); }
		}
		return true;
	}

	bool OnEnter(APlayerController* PC, bool bPressed)
	{
		AAntTutorial* T = PC ? AAntTutorial::Get(PC->GetWorld()) : nullptr;
		if (!T || T->Mode != Training) { return false; }
		static double DownAt = 0.0;
		if (bPressed) { DownAt = FPlatformTime::Seconds(); return true; }
		const bool bLong = FPlatformTime::Seconds() - DownAt > 1.2;
		if (AAntPlayerController* APC = Cast<AAntPlayerController>(PC)) { APC->ServerTut(bLong ? 1 : 0); }
		return true;
	}
}

// =====================================================================================================================
// The drill-sergeant radio bark: synthesised gibberish (a glottal pulse through two vowel formants, syllable by syllable),
// the way Animal Crossing talks. Radio: band-limited with crackle; the Queen: higher and softer. Local, cheap, no assets.
// =====================================================================================================================
namespace AntTut
{
	void Bark(const UObject* Ctx, int32 Seed, int32 Syllables, uint8 Voice)
	{
		UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (!W || W->GetNetMode() == NM_DedicatedServer || !GEngine || !GEngine->UseSound()) { return; }
		const int32 Rate = 22050;
		FRandomStream R(Seed * 7919 + 17);
		const float F0 = Voice == 0 ? 112.f : Voice == 1 ? 165.f : 250.f;
		static const float Vowels[5][2] = { { 730.f, 1090.f }, { 570.f, 840.f }, { 300.f, 870.f }, { 530.f, 1840.f }, { 390.f, 1990.f } };
		TArray<int16> Pcm;
		Syllables = FMath::Clamp(Syllables, 3, 16);
		float Phase = 0.f;
		for (int32 s = 0; s < Syllables; ++s)
		{
			const float Dur = R.FRandRange(0.065f, 0.12f) * (Voice == 0 ? 1.1f : 1.f);
			const int32 N = (int32)(Dur * Rate);
			const float* V = Vowels[R.RandRange(0, 4)];
			const float Pitch = F0 * R.FRandRange(0.88f, 1.22f) * (s == Syllables - 1 ? 0.85f : 1.f);
			const bool bBark = Voice == 0 && R.FRand() < 0.25f;   // the sergeant barks some syllables
			for (int32 i = 0; i < N; ++i)
			{
				const float T = (float)i / Rate;
				const float Env = FMath::Min(1.f, T / 0.008f) * FMath::Min(1.f, (Dur - T) / 0.025f);
				const float F = Pitch * (1.12f - 0.24f * T / Dur);
				Phase += F / Rate;
				float Smp = 0.f;
				for (int32 k = 1; k <= 14; ++k)
				{
					const float Fk = F * k;
					const float G = 1.f / (1.f + FMath::Square((Fk - V[0]) / 90.f)) + 0.6f / (1.f + FMath::Square((Fk - V[1]) / 140.f));
					Smp += FMath::Sin(2.f * PI * Phase * k) * G / FMath::Sqrt((float)k);
				}
				if (bBark || Voice == 0) { Smp += 0.25f * R.FRandRange(-1.f, 1.f) * Env; }   // grit
				if (Voice == 1) { Smp = FMath::Clamp(Smp * 1.6f, -1.f, 1.f) + 0.08f * R.FRandRange(-1.f, 1.f); }   // radio: clipped, hiss
				if (Voice == 2) { Smp *= 0.7f; }
				Pcm.Add((int16)FMath::Clamp(Smp * Env * 9000.f, -32000.f, 32000.f));
			}
			const int32 Gap = (int32)(R.FRandRange(0.02f, 0.045f) * Rate);
			for (int32 i = 0; i < Gap; ++i) { Pcm.Add(Voice == 1 ? (int16)(R.FRandRange(-1.f, 1.f) * 500.f) : 0); }
		}
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = (float)Pcm.Num() / Rate;
		Wave->SoundGroup = SOUNDGROUP_Voice;
		Wave->bLooping = false;
		Wave->QueueAudio((const uint8*)Pcm.GetData(), Pcm.Num() * sizeof(int16));
		static TArray<USoundWaveProcedural*> Keep;   // alive while they play
		Wave->AddToRoot();
		Keep.Add(Wave);
		while (Keep.Num() > 4) { Keep[0]->RemoveFromRoot(); Keep.RemoveAt(0); }
		UGameplayStatics::PlaySound2D(W, Wave, Voice == 2 ? 0.55f : 0.7f);
		AntAudio::Duck(W, Wave->Duration);
	}
}

// =====================================================================================================================
// The HUD (every machine): objective card, subtitles with the speaker, world markers, payoff stamp, timers, the newsreel
// prologue and graduation, the sandbox legend. Kitchen war-room style (docs/UI_STYLE.md).
// =====================================================================================================================
namespace
{
	using namespace AntUiCanvas;
	float GK = 1.f;

	void KeyCap(UCanvas* Cv, const FString& Key, float X, float Y, float Pt, float& OutW)
	{
		const FVector2D Sz = TextSize(Cv, Key, EFont::Bold, Pt * GK);
		const float W = Sz.X + 14.f * GK, H = Pt * GK * 1.45f;
		AntUiCanvas::Rect(Cv, X, Y, W, H, WithA(C::Paper, 0.95f));
		AntUiCanvas::Rect(Cv, X, Y + H - 3.f * GK, W, 3.f * GK, WithA(C::Cocoa, 0.6f));
		Text(Cv, Key, X + 7.f * GK, Y + H * 0.12f, EFont::Bold, Pt * GK, C::Graphite);
		OutW = W;
	}
	/** "Пробел|Space" -> the language's part; "Caps Lock+ЛКМ|Caps Lock+LMB" -> keycaps split on '+'. */
	float KeyCaps(UCanvas* Cv, const TCHAR* Keys, float X, float Y, float Pt)
	{
		if (!Keys || !*Keys) { return 0.f; }
		FString K(Keys), Ru, En;
		if (K.Split(TEXT("|"), &Ru, &En)) { K = AntUi::Lang() == 1 ? En : Ru; }
		TArray<FString> Parts;
		K.ParseIntoArray(Parts, TEXT("+"));
		float W = 0.f;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			float Wi = 0.f;
			KeyCap(Cv, Parts[i], X + W, Y, Pt, Wi);
			W += Wi + 6.f * GK;
		}
		return W;
	}
	float KeyCapsW(UCanvas* Cv, const TCHAR* Keys, float Pt)
	{
		if (!Keys || !*Keys) { return 0.f; }
		FString K(Keys), Ru, En;
		if (K.Split(TEXT("|"), &Ru, &En)) { K = AntUi::Lang() == 1 ? En : Ru; }
		TArray<FString> Parts;
		K.ParseIntoArray(Parts, TEXT("+"));
		float W = 0.f;
		for (const FString& Pp : Parts) { W += TextSize(Cv, Pp, EFont::Bold, Pt * GK).X + 20.f * GK; }
		return W;
	}
	FString Speaker(uint8 Voice)
	{
		return Voice == 1 ? AntTut::Tx(TEXT("Гнездовое радио Щели"), TEXT("Nest Radio")) : Voice == 2 ? AntTut::Tx(TEXT("Королева"), TEXT("The Queen")) : AntTut::Tx(TEXT("Сержант Жвалов"), TEXT("Sergeant Zhvalov"));
	}
	const TCHAR* Portrait(uint8 Voice) { return Voice == 2 ? TEXT("Portraits/T_UI_PorQueen") : Voice == 1 ? TEXT("Icons/T_UI_Icon_Radio") : TEXT("Portraits/T_UI_PorZhvalov"); }

	struct FLocalSeen { int16 Line = -1; float LineAt = 0.f; uint8 Phase = 255; uint8 Beat = 255; float PayAt = -100.f; float ChapterAt = -100.f; bool bSaved = false; float DoneAt = -1.f; bool bLeft = false; int32 StepId = -1; int32 LastSum = -1; float StepAt = 0.f; float ProgAt = 0.f; float QMissAt = -100.f; float QPressAt = -100.f; };
	TMap<TWeakObjectPtr<APlayerController>, FLocalSeen> GSeen;

	/**
	 * The squad order step (beat «Командир», [Q]): playtest 24.11 — the key went by unnoticed. For the whole step: the key on
	 * screen; the squad panel it changes (left, under the mass meter, AntRts::DrawHud) framed and labelled; the aim told (is the
	 * crosshair on the cheese, a line to it if not); a Q that missed named; after 6 s with no order the screen dims round the
	 * panel and the card grows (the sergeant says the order again on the server: Check, BRts).
	 */
	void QOrderCard(AHUD* Hud, UCanvas* Cv, const AAntTutorial* T, APlayerController* PC, FLocalSeen& Seen, float Now)
	{
		const float K = GK, SW = Cv->ClipX, SH = Cv->ClipY;
		const float Pu = 0.5f + 0.5f * FMath::Sin(Now * 6.f);
		const float Waited = Now - FMath::Max(Seen.StepAt, Seen.QPressAt);
		const UAntRtsInput* In = UAntRtsInput::Of(PC);
		const bool bCursor = In && In->bRtsMode;
		// the aim: where the crosshair order lands (AAntPlayerController::SquadHaul -> AntRts: loot within 260 of the point)
		const bool bMark = T->Marks.Num() > 0;
		const FVector CheeseAt = bMark ? FVector(T->Marks[0].At) : FVector::ZeroVector;
		bool bOnCheese = false;
		if (const AAntPlayerController* APC = Cast<AAntPlayerController>(PC)) { bOnCheese = bMark && FVector::Dist(APC->CrosshairPoint(), CheeseAt) < 240.f; }
		if (PC->WasInputKeyJustPressed(EKeys::Q))
		{
			Seen.QPressAt = Now;
			if (!bOnCheese && !bCursor) { Seen.QMissAt = Now; }
		}
		// the squad panel (AntRts::DrawHud: «ОТРЯД n · за мной m», the jobs, «Приказ: ...» at x 24, y 200 of 1080)
		const float PX = 12.f * K, PY = 190.f * K, PW = 400.f * K, PH = 84.f * K;
		// the dim after 6 s without the order: four rects round the panel, so the panel stays lit
		const float Dim = FMath::Clamp((Waited - 6.f) / 0.6f, 0.f, 1.f) * 0.5f;
		if (Dim > 0.f)
		{
			const FLinearColor D(0.f, 0.f, 0.f, Dim);
			AntUiCanvas::Rect(Cv, 0.f, 0.f, SW, PY, D);
			AntUiCanvas::Rect(Cv, 0.f, PY + PH, SW, SH - PY - PH, D);
			AntUiCanvas::Rect(Cv, 0.f, PY, PX, PH, D);
			AntUiCanvas::Rect(Cv, PX + PW, PY, SW - PX - PW, PH, D);
		}
		Frame(Cv, PX, PY, PW, PH, (2.f + 3.f * Pu) * K, WithA(C::Amber, 0.5f + 0.5f * Pu));
		{
			float W0 = 0.f;
			KeyCap(Cv, TEXT("Q"), PX + PW + 14.f * K, PY + 8.f * K, 18.f, W0);
			Text(Cv, AntTut::Tx(TEXT("← твой отряд: здесь появится приказ «тащить»"), TEXT("← your squad: the «haul» order shows up here")), PX + PW + 22.f * K + W0, PY + 10.f * K, EFont::Bold, 18.f * K, C::Honey);
		}
		// the cheese: a pointer on top of the dim, and a line from the crosshair while the aim is off it
		if (bMark)
		{
			FVector CamLoc; FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			if (FVector::DotProduct(CamRot.Vector(), CheeseAt - CamLoc) > 0.f)
			{
				const FVector Sp = Hud->Project(CheeseAt, false);
				const FVector2D P(FMath::Clamp((float)Sp.X, 40.f * K, SW - 40.f * K), FMath::Clamp((float)Sp.Y, 40.f * K, SH - 40.f * K));
				const FLinearColor Col = bOnCheese ? C::Leaf : C::Honey;
				Frame(Cv, P.X - 22.f * K, P.Y - 22.f * K, 44.f * K, 44.f * K, (2.f + 2.f * Pu) * K, Col);
				if (!bOnCheese && !bCursor) { Cv->K2_DrawLine(FVector2D(SW * 0.5f, SH * 0.5f), P, 2.f * K, WithA(Col, 0.5f + 0.4f * Pu)); }
			}
		}
		// the card: the key, what it does, the aim state
		const float Grow = Dim > 0.f ? 1.25f : 1.f;
		const float BW = 640.f * K * Grow, BH = 168.f * K * Grow, BX = SW * 0.5f - BW * 0.5f, BY = SH * 0.58f - (Grow - 1.f) * 60.f * K;
		Glass(Cv, BX, BY, BW, BH, 0.9f);
		Frame(Cv, BX, BY, BW, BH, (2.f + 3.f * Pu) * K, WithA(bOnCheese ? C::Leaf : C::Amber, 0.5f + 0.5f * Pu));
		{
			// a big square keycap of its own (KeyCap's text overruns the cap at this size)
			const float Cap = 62.f * K * Grow;
			const float CX0 = SW * 0.5f - Cap * 0.5f, CY0 = BY + 12.f * K;
			AntUiCanvas::Rect(Cv, CX0, CY0, Cap, Cap, WithA(C::Paper, 0.97f));
			AntUiCanvas::Rect(Cv, CX0, CY0 + Cap - 5.f * K, Cap, 5.f * K, WithA(C::Cocoa, 0.7f));
			Frame(Cv, CX0 - 3.f * K, CY0 - 3.f * K, Cap + 6.f * K, Cap + 6.f * K, (1.5f + 2.f * Pu) * K, WithA(C::Amber, 0.6f + 0.4f * Pu));
			Text(Cv, TEXT("Q"), SW * 0.5f, CY0 + Cap * 0.14f, EFont::Bold, Cap * 0.6f, C::Graphite, 1);
		}
		Text(Cv, AntTut::Tx(TEXT("ПРИКАЗ ОТРЯДУ: ТАЩИ СЫР"), TEXT("SQUAD ORDER: HAUL THE CHEESE")), SW * 0.5f, BY + BH - 82.f * K * Grow, EFont::Bold, 26.f * K * Grow, C::Honey, 1);
		FString Aim;
		FLinearColor AimC = C::Sugar;
		if (Now - Seen.QMissAt < 2.5f) { Aim = AntTut::Tx(TEXT("Мимо! [Q] отдаёт приказ туда, куда смотрит прицел: наведи центр экрана на сыр"), TEXT("Missed! [Q] orders the squad where the crosshair points: put the screen centre on the cheese")); AimC = C::Jam; }
		else if (bCursor) { Aim = AntTut::Tx(TEXT("Курсор: наведи его на сыр и жми [Q] (или [ПКМ] по сыру)"), TEXT("Cursor mode: point it at the cheese and press [Q] (or [RMB] on it)")); }
		else if (bOnCheese) { Aim = AntTut::Tx(TEXT("Прицел на сыре — жми [Q]!"), TEXT("Crosshair on the cheese — press [Q]!")); AimC = C::Leaf; }
		else { Aim = AntTut::Tx(TEXT("Наведи прицел (центр экрана) на сыр, потом [Q]"), TEXT("Put the crosshair (screen centre) on the cheese, then [Q]")); }
		TextFit(Cv, Aim, BX + 20.f * K, BY + BH - 44.f * K * Grow, BW - 40.f * K, EFont::Bold, 20.f * K * Grow, AimC);
	}

	/** The graduation's pages 1..3 (page 0 is the rank and the medal): the Queen's gift, the caste perk, what opens now. */
	void DebriefPageDraw(UCanvas* Cv, const AAntTutorial* T, float K, float SW, float SH)
	{
		const float CX = SW * 0.5f;
		const int32 Pg = T->DebriefPage;
		Glass(Cv, CX - 540.f * K, 190.f * K, 1080.f * K, 450.f * K, 0.88f);
		if (Pg == 1)
		{
			if (UTexture2D* Por = AntUi::Tex(TEXT("Portraits/T_UI_PorQueen"))) { Cover(Cv, Por, CX - 510.f * K, 214.f * K, 300.f * K, 300.f * K, FLinearColor::White); }
			Text(Cv, AntTut::Tx(TEXT("ПОДАРОК КОРОЛЕВЫ"), TEXT("THE QUEEN'S GIFT")), CX - 180.f * K, 214.f * K, EFont::Display, 44.f * K, C::Amber);
			Wrap(Cv, AntTut::Tx(TEXT("«Мои дети… Вы вернулись с тортом. Загляните в кладовые.»"), TEXT("«My children… you came back with cake. Look in the stores.»")), CX - 180.f * K, 274.f * K, 680.f * K, EFont::Body, 26.f * K, C::Sugar, 2, 1.2f);
			Icon(Cv, TEXT("Jelly"), CX - 180.f * K, 360.f * K, 56.f * K);
			Text(Cv, AntTut::Tx(TEXT("+150 сахара · +80 белка · +40 жира · +120 крошек"), TEXT("+150 sugar · +80 protein · +40 fat · +120 crumbs")), CX - 110.f * K, 366.f * K, EFont::Bold, 22.f * K, C::Honey);
			Text(Cv, AntTut::Tx(TEXT("+2 молочка · +1 личинка · +4 рабочих"), TEXT("+2 jelly · +1 larva · +4 workers")), CX - 110.f * K, 400.f * K, EFont::Bold, 22.f * K, C::Honey);
			Wrap(Cv, AntTut::Tx(TEXT("Подарок ждёт в кладовых настоящей колонии: он придёт, когда ты откроешь экран колонии — первой откроется карточка «Кладовые»."),
				TEXT("The gift waits in the stores of your real colony: it arrives when you open the colony screen — the «Stores» card opens first.")), CX - 180.f * K, 470.f * K, 680.f * K, EFont::Body, 19.f * K, WithA(C::Sugar, 0.8f), 3, 1.2f);
			Stamp(Cv, AntTut::Tx(TEXT("В КЛАДОВЫХ"), TEXT("IN THE STORES")), CX + 380.f * K, 560.f * K, -8.f, C::Jam, 30.f * K);
		}
		else if (Pg == 2)
		{
			Icon(Cv, TEXT("Soldier"), CX - 500.f * K, 220.f * K, 120.f * K);
			Text(Cv, AntTut::Tx(TEXT("ЗВАНИЕ → ПЕРК КАСТЫ"), TEXT("RANK → CASTE PERK")), CX - 340.f * K, 220.f * K, EFont::Display, 44.f * K, C::Amber);
			Text(Cv, FString::Printf(TEXT("%s  →  %s"), *AntTut::RankName(0), *AntTut::RankName(1)), CX - 340.f * K, 282.f * K, EFont::Bold, 30.f * K, C::Honey);
			Wrap(Cv, AntTut::BranchMerged(AntTut::Fighter)
				? AntTut::Tx(TEXT("Ступени перков открываются на званиях Ефрейтор, Сержант и Старшина. На каждой — выбор одного из двух: боковой шаг, а не сила. Профиль открыт на вкладке «Перки» — выбери первый (Esc — закрыть, потом [Enter])."),
					TEXT("Perk tiers open at Lance Corporal, Sergeant and Master Sergeant. Each is a choice of one of two: a sidegrade, not raw power. The profile is open at Perks — pick the first (Esc to close, then [Enter])."))
				: AntTut::Tx(TEXT("Скоро: перки каст, медали и ветераны отряда."), TEXT("Soon: caste perks, medals and squad veterans.")),
				CX - 340.f * K, 340.f * K, 820.f * K, EFont::Body, 22.f * K, C::Sugar, 5, 1.25f);
		}
		else
		{
			float Y = 216.f * K;
			auto Row = [&](const TCHAR* IconName, const FString& Title, const FString& Sub, bool bOn)
			{
				Icon(Cv, IconName, CX - 500.f * K, Y, 64.f * K, bOn ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.4f));
				Text(Cv, Title, CX - 410.f * K, Y + 2.f * K, EFont::Bold, 26.f * K, bOn ? C::Sugar : C::Muted);
				Wrap(Cv, Sub, CX - 410.f * K, Y + 36.f * K, 900.f * K, EFont::Body, 19.f * K, WithA(C::Sugar, 0.75f), 2, 1.2f);
				Y += 134.f * K;
			};
			Row(TEXT("Cake"), AntTut::Tx(TEXT("Открыт «Настоящий рейд»"), TEXT("«The Real Raid» unlocked")), AntTut::Tx(TEXT("Угроза 1 «Будний день»: сначала колония и подарок Королевы, потом — на кухню за настоящим тортом."), TEXT("Threat 1 «Weekday»: the colony and the Queen's gift first, then the real cake.")), true);
			Row(TEXT("Human"), AntTut::Tx(TEXT("А хочешь сыграть за Гиганта?"), TEXT("Fancy playing the Giant?")), AntTut::Tx(TEXT("Версус: человек против колонии. Тапок — в твоих руках. Кнопка в главном меню."), TEXT("Versus: the human against the colony. The slipper is in your hands. A button in the main menu.")), true);
			Row(TEXT("Wrench"), AntTut::Tx(TEXT("Песочница"), TEXT("Sandbox")), AntTut::IsDemo() ? FString(AntTut::Tx(TEXT("в полной версии"), TEXT("in the full game"))) : FString(AntTut::Tx(TEXT("Вся квартира без таймера: реквизит цепных реакций, Гигант, время суток, ТВ, пылесос, погода — панель на [F2]."), TEXT("The whole flat with no clock: chain props, the Giant, time of day, TV, vacuum, weather — the panel on [F2]."))), !AntTut::IsDemo());
		}
	}
}

namespace AntTut
{
	void DrawHud(AHUD* Hud, UCanvas* Cv, APlayerController* PC)
	{
		AAntTutorial* T = PC ? AAntTutorial::Get(PC->GetWorld()) : nullptr;
		if (!Hud || !Cv || !T) { return; }
		GK = FMath::Max(0.7f, Cv->ClipY / 1080.f);
		const float K = GK, SW = Cv->ClipX, SH = Cv->ClipY;
		const float Now = T->ServerNow();
		FLocalSeen& Seen = GSeen.FindOrAdd(PC);

		if (T->Mode == Sandbox)
		{
			Glass(Cv, SW - 520.f * K, 24.f * K, 496.f * K, 120.f * K, 0.72f);
			Text(Cv, Tx(TEXT("ПЕСОЧНИЦА"), TEXT("SANDBOX")), SW - 500.f * K, 34.f * K, EFont::Display, 30.f * K, C::Amber);
			Wrap(Cv, Tx(TEXT("F2 — панель: реквизит цепных реакций, Гигант, время суток, ТВ, пылесос, погода, кран, сброс · быстро: F3 Гигант · F4 время · F5 цепочка · F6 потоп · F7 сброс"),
				TEXT("F2 — panel: chain props, the Giant, time of day, TV, vacuum, weather, tap, reset · quick: F3 Giant · F4 time · F5 chain · F6 flood · F7 reset")), SW - 500.f * K, 76.f * K, 460.f * K, EFont::Body, 17.f * K, C::Sugar, 3);
			return;
		}
		const AntTut::FBeat& B = AntTut::Beat(T->BeatNo);

		// ---- local reactions: a new line barks, a payoff stings, the end saves and goes back to the menu
		if (T->LineNo != Seen.Line)
		{
			Seen.Line = T->LineNo;
			Seen.LineAt = Now;
			if (T->LineNo >= 0 && T->LineNo < LCount) { Bark(PC, T->LineNo, FCString::Strlen(GLines[T->LineNo].Ru) / 7, GLines[T->LineNo].Voice); }
		}
		if (T->BeatNo != Seen.Beat && T->Phase != AAntTutorial::Fade)
		{
			if (Seen.Beat == 255 || AntTut::Beat(Seen.Beat).Chapter != B.Chapter) { Seen.ChapterAt = Now; }
			Seen.Beat = T->BeatNo;
		}
		if (T->Phase != Seen.Phase)
		{
			if (T->Phase == AAntTutorial::Payoff) { Seen.PayAt = Now; AntAudio::Sting(PC, EAntSting::Card); AntAudio::Play2D(PC, EAntSfx::DeliverCheer, 0.8f); }
			Seen.Phase = T->Phase;
		}
		if (T->Phase == AAntTutorial::Done && !Seen.bSaved)
		{
			Seen.bSaved = true;
			Seen.DoneAt = Now;
			FProgress& P = Progress();
			const FAntTutPlayer* Me = T->Find(PC->PlayerState);
			P.bDone = true;
			P.BeatsDone |= (uint32)T->BeatsDoneMask;
			P.Xp = FMath::Max(P.Xp, Me ? Me->Xp : T->TeamXp);
			P.Rank = FMath::Max(P.Rank, 1);
			P.Medals.AddUnique(TEXT("slipper"));
			P.Medals.AddUnique(TEXT("graduate"));
			if (PC->GetWorld()->GetNetMode() != NM_Client) { P.bGiftPending = true; }
			P.BestSeconds = P.BestSeconds > 0.f ? FMath::Min(P.BestSeconds, (float)T->TotalSeconds) : (float)T->TotalSeconds;
			SaveProgress();
		}
		if (T->Phase == AAntTutorial::Done && !Seen.bLeft && Now - Seen.DoneAt > 1.5f)
		{
			Seen.bLeft = true;
			SetWanted(Off);
			if (!FParse::Param(FCommandLine::Get(), TEXT("AntTutStay")))
			{
				if (UAntSession* S = PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<UAntSession>() : nullptr)
				{
					if (PC->GetWorld()->GetNetMode() == NM_Standalone) { S->PlaySolo(); }   // straight into the real colony: the Queen's gift, the stores card (a co-op client goes back to the menu)
					else { S->ReturnToMenu(); }
				}
			}
		}

		// ---- world markers
		if (T->Phase == AAntTutorial::Play || T->Phase == AAntTutorial::Poligon || T->Phase == AAntTutorial::Intro)
		{
			const APawn* Me = PC->GetPawn();
			for (const FAntTutMark& M : T->Marks)
			{
				FVector Sp = Hud->Project(M.At, false);
				FVector CamLoc; FRotator CamRot;
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				const bool bBehind = FVector::DotProduct(CamRot.Vector(), FVector(M.At) - CamLoc) < 0.f;
				if (bBehind) { Sp.X = SW - Sp.X; Sp.Y = SH - 40.f * K; }
				const float Pad = 40.f * K;
				const bool bEdge = bBehind || Sp.X < Pad || Sp.X > SW - Pad || Sp.Y < Pad || Sp.Y > SH - Pad;
				Sp.X = FMath::Clamp(Sp.X, Pad, SW - Pad);
				Sp.Y = FMath::Clamp(Sp.Y, Pad, SH - Pad);
				const FLinearColor Col = M.Kind == 2 ? C::Jam : M.Kind == 1 ? C::Leaf : M.Kind == 3 ? C::Honey : C::Amber;
				const float Pulse = 1.f + 0.12f * FMath::Sin(Now * 5.f);
				const float S = 15.f * K * Pulse;
				Cv->K2_DrawPolygon(nullptr, FVector2D(Sp.X, Sp.Y), FVector2D(S, S), 4, WithA(Col, 0.9f));
				const float Dist = Me ? (float)FVector::Dist(Me->GetActorLocation(), M.At) / 1900.f : 0.f;
				Text(Cv, FString::Printf(TEXT("%s  %.1f м"), *M.Label, Dist), Sp.X, Sp.Y + 18.f * K, EFont::Bold, 16.f * K, bEdge ? WithA(Col, 0.8f) : Col, 1, 0.6f);
			}
		}

		// ---- the objective card (top right)
		const bool bFull = T->BeatNo == BPrologue || T->Phase == AAntTutorial::Debrief || T->Phase == AAntTutorial::Done;
		if (!bFull && T->Phase != AAntTutorial::Fade)
		{
			const FAntTutPlayer* Mine = T->Find(PC->PlayerState);
			const float CW = 430.f * K, X = SW - CW - 28.f * K;
			float Y = 150.f * K;
			const float CH = (96.f + 34.f * B.Steps.Num() + (T->Players.Num() > 1 ? 4.f : 0.f)) * K;
			Glass(Cv, X, Y, CW, CH, 0.8f);
			AntUiCanvas::Rect(Cv, X, Y, 5.f * K, CH, C::Amber);
			Text(Cv, ChapterName(B.Chapter).ToUpper() + FString::Printf(TEXT("  ·  %d/%d"), (int32)T->BeatNo, NumBeats() - 1), X + 18.f * K, Y + 10.f * K, EFont::Bold, 14.f * K, C::Muted);
			TextFit(Cv, BeatTitle(T->BeatNo), X + 18.f * K, Y + 30.f * K, CW - 36.f * K, EFont::Display, 28.f * K, C::Sugar);
			if (!BranchMerged(B.Branch)) { Text(Cv, Tx(TEXT("упрощённо: ждёт ветку"), TEXT("simplified: branch pending")), X + CW - 14.f * K, Y + 12.f * K, EFont::Body, 13.f * K, C::Muted, 2); }
			Y += 74.f * K;
			bool bCurrentShown = false;
			for (int32 s = 0; s < B.Steps.Num(); ++s)
			{
				const AntTut::FStep& St = B.Steps[s];
				const int32 Have = Mine && Mine->Count.IsValidIndex(s) ? Mine->Count[s] : 0;
				const bool bDone = Have >= St.Need;
				const bool bCur = !bDone && !bCurrentShown;
				bCurrentShown |= bCur;
				const FLinearColor Col = bDone ? WithA(C::Leaf, 0.9f) : bCur ? C::Honey : WithA(C::Sugar, 0.55f);
				AntUiCanvas::Rect(Cv, X + 18.f * K, Y + 6.f * K, 16.f * K, 16.f * K, WithA(C::Ink, 0.7f));
				Frame(Cv, X + 18.f * K, Y + 6.f * K, 16.f * K, 16.f * K, 2.f * K, Col);
				if (bDone) { AntUiCanvas::Rect(Cv, X + 22.f * K, Y + 10.f * K, 8.f * K, 8.f * K, C::Leaf); }
				float TX = X + 44.f * K;
				TX += KeyCaps(Cv, St.Keys, TX, Y + 2.f * K, 14.f) + (St.Keys && *St.Keys ? 6.f * K : 0.f);
				FString Line = Tx(St.Ru, St.En);
				if (St.Need > 1) { Line += FString::Printf(TEXT("  %d/%d"), FMath::Min(Have, St.Need), St.Need); }
				if (St.bTeam) { Line += Tx(TEXT("  (отряд)"), TEXT("  (team)")); }
				TextFit(Cv, Line, TX, Y + 3.f * K, X + CW - TX - 12.f * K, bCur ? EFont::Bold : EFont::Body, 19.f * K, Col);
				// co-op: a dot per player
				if (T->Players.Num() > 1)
				{
					for (int32 p = 0; p < T->Players.Num(); ++p)
					{
						const bool bPD = T->Players[p].Count.IsValidIndex(s) && T->Players[p].Count[s] >= St.Need;
						AntUiCanvas::Rect(Cv, X + CW - (14.f + 11.f * p) * K, Y + 26.f * K, 7.f * K, 7.f * K, bPD ? C::Leaf : WithA(C::Sugar, 0.25f));
					}
				}
				Y += 34.f * K;
			}
			// rank and XP under the card
			const int32 Xp = Mine ? Mine->Xp : T->TeamXp;
			Text(Cv, FString::Printf(TEXT("%s  ·  %d XP"), *RankName(0), Xp), X + CW, Y + 12.f * K, EFont::Bold, 15.f * K, C::Honey, 2, 0.6f);
			if (T->bOffer)
			{
				float W0 = 0.f;
				Glass(Cv, X, Y + 40.f * K, CW, 44.f * K, 0.85f);
				KeyCap(Cv, TEXT("Enter"), X + 16.f * K, Y + 50.f * K, 15.f, W0);
				Text(Cv, Tx(TEXT("— сержант поможет"), TEXT("— the sergeant helps")), X + 26.f * K + W0, Y + 52.f * K, EFont::Bold, 18.f * K, C::Honey);
			}
			else if (PC->GetWorld()->GetNetMode() != NM_Client)
			{
				Text(Cv, Tx(TEXT("держи [Enter] — пропустить этап"), TEXT("hold [Enter] — skip this beat")), X + CW, Y + 36.f * K, EFont::Body, 13.f * K, WithA(C::Muted, 0.8f), 2);
			}
		}

		// ---- the big key glyph: the key a step needs, large and pulsing, at the moment it is needed (and again after 8 s without progress)
		if (!bFull && T->Phase == AAntTutorial::Play)
		{
			const FAntTutPlayer* Me2 = T->Find(PC->PlayerState);
			int32 Cur = -1, Sum = 0;
			for (int32 s = 0; s < B.Steps.Num(); ++s)
			{
				const int32 Hv = Me2 && Me2->Count.IsValidIndex(s) ? Me2->Count[s] : 0;
				Sum += Hv;
				if (Cur < 0 && Hv < B.Steps[s].Need) { Cur = s; }
			}
			const int32 SId = (int32)T->BeatNo * 16 + FMath::Max(Cur, 0);
			if (SId != Seen.StepId) { Seen.StepId = SId; Seen.StepAt = Now; Seen.ProgAt = Now; Seen.LastSum = Sum; }
			if (Sum != Seen.LastSum) { Seen.LastSum = Sum; Seen.ProgAt = Now; }
			const bool bQStep = T->BeatNo == BRts && Cur == 1;   // the squad order: its own card below (KAN-33)
			const bool bShowKey = !bQStep && Cur >= 0 && B.Steps[Cur].Keys && *B.Steps[Cur].Keys && (Now - Seen.StepAt < 9.f || Now - Seen.ProgAt > 8.f);
			if (bQStep) { QOrderCard(Hud, Cv, T, PC, Seen, Now); }
			if (bShowKey)
			{
				const float Pu = 0.5f + 0.5f * FMath::Sin(Now * 6.f);
				const float Pt = 40.f;
				const float KW = KeyCapsW(Cv, B.Steps[Cur].Keys, Pt);
				const FString What = Tx(B.Steps[Cur].Ru, B.Steps[Cur].En);
				const float BW = FMath::Max(KW, 420.f * K) + 90.f * K, BH = 150.f * K, BX = SW * 0.5f - BW * 0.5f, BY = SH * 0.58f;
				Glass(Cv, BX, BY, BW, BH, 0.85f);
				Frame(Cv, BX, BY, BW, BH, (2.f + 3.f * Pu) * K, WithA(C::Amber, 0.45f + 0.55f * Pu));
				KeyCaps(Cv, B.Steps[Cur].Keys, SW * 0.5f - KW * 0.5f, BY + 16.f * K, Pt);
				Text(Cv, What, SW * 0.5f, BY + BH - 46.f * K, EFont::Bold, 26.f * K, C::Honey, 1);
			}
		}
		// ---- timers: the finale's last light, holding out under the table, the free range
		{
			float Ends = T->TimerEnds;
			FString What = T->BeatNo == BFinale ? Tx(TEXT("ДО ПОСЛЕДНЕГО СВЕТА"), TEXT("TO LAST LIGHT")) : Tx(TEXT("ПРОДЕРЖИСЬ"), TEXT("HOLD OUT"));
			if (T->Phase == AAntTutorial::Poligon) { Ends = T->PoligonEnds; What = Tx(TEXT("СВОБОДНЫЙ ПОЛИГОН · [Enter] — дальше"), TEXT("FREE RANGE · [Enter] — continue")); }
			if (Ends > 0.f && Ends > Now)
			{
				const int32 Left = FMath::CeilToInt(Ends - Now);
				Glass(Cv, SW * 0.5f - 210.f * K, 92.f * K, 420.f * K, 72.f * K, 0.8f);
				Text(Cv, What, SW * 0.5f, 98.f * K, EFont::Bold, 15.f * K, C::Muted, 1);
				Text(Cv, FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60), SW * 0.5f, 116.f * K, EFont::Display, 38.f * K, Left <= 20 ? C::Jam : C::Amber, 1, 0.6f);
			}
		}

		// ---- full-screen newsreel: the prologue and the graduation
		if (T->BeatNo == BPrologue && T->Phase != AAntTutorial::Fade)
		{
			const float A = FMath::Clamp((Now - T->PhaseStart) / 0.6f, 0.f, 1.f) * (T->Phase == AAntTutorial::Payoff ? FMath::Clamp(1.f - (Now - T->PhaseStart) / 2.f, 0.f, 1.f) : 1.f);
			AntUiCanvas::Rect(Cv, 0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, 0.86f * A));
			if (UTexture2D* Bg = AntUi::Tex(TEXT("Backgrounds/T_UI_BgWarRoom"))) { Cover(Cv, Bg, 0.f, 0.f, SW, SH, FLinearColor(0.55f, 0.5f, 0.45f, 0.55f * A)); }
			Text(Cv, Tx(TEXT("ГНЕЗДОВОЕ РАДИО ЩЕЛИ"), TEXT("NEST RADIO OF THE CRACK")), SW * 0.5f, 120.f * K, EFont::Display, 54.f * K, WithA(C::Amber, A), 1, 0.6f);
			Text(Cv, Tx(TEXT("ВЫПУСК № 1 · ВЕЛИКАЯ КОЛОНИЯ ЩЕЛИ ОБЪЯВЛЯЕТ НАБОР"), TEXT("ISSUE No. 1 · THE GREAT COLONY OF THE CRACK IS RECRUITING")), SW * 0.5f, 190.f * K, EFont::Bold, 22.f * K, WithA(C::Sugar, A), 1);
			Paper(Cv, SW * 0.5f - 360.f * K, 270.f * K, 720.f * K, 300.f * K, true);
			if (UTexture2D* Cake = AntUi::Tex(TEXT("Backgrounds/T_UI_BgCake"))) { Cover(Cv, Cake, SW * 0.5f - 340.f * K, 290.f * K, 680.f * K, 260.f * K, FLinearColor(1.f, 1.f, 1.f, A)); }
			Stamp(Cv, Tx(TEXT("ОБЪЕКТ «ИМЕНИНЫ»"), TEXT("OBJECTIVE «BIRTHDAY»")), SW * 0.5f + 210.f * K, 520.f * K, -8.f, WithA(C::Jam, A), 26.f * K);
			Stamp(Cv, Tx(TEXT("НОВОБРАНЕЦ"), TEXT("RECRUIT")), SW * 0.5f - 250.f * K, 300.f * K, 6.f, WithA(C::Amber, A), 22.f * K);
			float W0 = 0.f;
			KeyCap(Cv, TEXT("Enter"), SW * 0.5f - 230.f * K, SH - 70.f * K, 16.f, W0);
			Text(Cv, Tx(TEXT("дальше   ·   держи — пропустить учебку"), TEXT("next   ·   hold — skip training")), SW * 0.5f - 220.f * K + W0, SH - 68.f * K, EFont::Body, 18.f * K, WithA(C::Sugar, 0.8f * A));
		}
		if (T->Phase == AAntTutorial::Debrief || T->Phase == AAntTutorial::Done)
		{
			const float A = FMath::Clamp((Now - T->PhaseStart) / 0.8f, 0.f, 1.f);
			const FAntTutPlayer* Mine = T->Find(PC->PlayerState);
			AntUiCanvas::Rect(Cv, 0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, 0.9f * A));
			if (UTexture2D* Bg = AntUi::Tex(TEXT("Backgrounds/T_UI_NewsWin"))) { Cover(Cv, Bg, 0.f, 0.f, SW, SH, FLinearColor(0.6f, 0.55f, 0.5f, 0.5f * A)); }
			Text(Cv, Tx(TEXT("ВЫПУСК УЧЕБКИ"), TEXT("TRAINING GRADUATION")), SW * 0.5f, 70.f * K, EFont::Display, 58.f * K, WithA(C::Amber, A), 1, 0.6f);
			Text(Cv, Tx(TEXT("Гнездовое радио Щели · спецвыпуск"), TEXT("Nest Radio · special issue")), SW * 0.5f, 140.f * K, EFont::Bold, 20.f * K, WithA(C::Sugar, A), 1);
			const float CX = SW * 0.5f;
			if (T->DebriefPage > 0) { DebriefPageDraw(Cv, T, K, SW, SH); }
			else
			{
			// the rank and the medal
			Paper(Cv, CX - 560.f * K, 200.f * K, 520.f * K, 420.f * K, true);
			Text(Cv, Tx(TEXT("ЗВАНИЕ"), TEXT("RANK")), CX - 530.f * K, 226.f * K, EFont::Bold, 18.f * K, C::Graphite);
			Text(Cv, RankName(0), CX - 530.f * K, 256.f * K, EFont::Display, 26.f * K, WithA(C::Graphite, 0.45f));
			Text(Cv, TEXT("→  ") + RankName(1), CX - 530.f * K, 296.f * K, EFont::Display, 40.f * K, C::AmberDeep);
			Icon(Cv, TEXT("Medal"), CX - 530.f * K, 370.f * K, 96.f * K);
			Text(Cv, Tx(TEXT("Медаль «Пережил Розовую Плиту»"), TEXT("Medal «Survived the Pink Slab»")), CX - 420.f * K, 390.f * K, EFont::Bold, 20.f * K, C::Graphite);
			Text(Cv, Tx(TEXT("и значок «Выпускник учебки»"), TEXT("and the «Training graduate» badge")), CX - 420.f * K, 420.f * K, EFont::Body, 18.f * K, C::Graphite);
			TextFit(Cv, FString::Printf(TEXT("%d XP · %d:%02d · %s %d · %s %d"), Mine ? Mine->Xp : T->TeamXp, T->TotalSeconds / 60, T->TotalSeconds % 60,
				Tx(TEXT("попыток"), TEXT("retries")), (int32)T->Retries, Tx(TEXT("подсказок"), TEXT("assists")), (int32)T->Assists), CX - 530.f * K, 500.f * K, 470.f * K, EFont::Mono, 16.f * K, C::Graphite);
			Stamp(Cv, Tx(TEXT("ГОДЕН"), TEXT("FIT FOR DUTY")), CX - 140.f * K, 560.f * K, -10.f, C::Jam, 30.f * K);
			// what opens now
			Glass(Cv, CX + 20.f * K, 200.f * K, 540.f * K, 420.f * K, 0.85f);
			float Y = 222.f * K;
			auto Row = [&](const TCHAR* IconName, const FString& Title, const FString& Sub, bool bOn)
			{
				Icon(Cv, IconName, CX + 40.f * K, Y, 48.f * K, bOn ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.4f));
				Text(Cv, Title, CX + 100.f * K, Y + 2.f * K, EFont::Bold, 20.f * K, bOn ? C::Sugar : C::Muted);
				Wrap(Cv, Sub, CX + 100.f * K, Y + 28.f * K, 440.f * K, EFont::Body, 15.f * K, WithA(C::Sugar, 0.7f), 2, 1.15f);
				Y += 80.f * K;
			};
			Row(TEXT("Cake"), Tx(TEXT("Открыт «Настоящий рейд»"), TEXT("«The Real Raid» unlocked")), Tx(TEXT("Угроза 1 «Будний день». Торт ждёт."), TEXT("Threat 1 «Weekday». The cake awaits.")), true);
			Row(TEXT("Jelly"), Tx(TEXT("Подарок Королевы"), TEXT("The Queen's gift")), BranchMerged(Colony) ? FString(Tx(TEXT("Кладовые колонии пополнены: загляни на экран колонии"), TEXT("The colony stores are filled: open the colony screen")))
				: FString(Tx(TEXT("+150 еды и 2 молочка к первому рейду — на первую камеру"), TEXT("+150 food, 2 jelly for your colony: enough for the first chamber"))), true);
			Row(TEXT("Soldier"), Tx(TEXT("Перк касты"), TEXT("Caste perk")), BranchMerged(Fighter) ? FString(Tx(TEXT("Первая ступень перков открыта: выбери её на третьем экране"), TEXT("The first perk tier is open: pick it on the third screen")))
				: FString(Tx(TEXT("Скоро: перки каст, медали и ветераны отряда"), TEXT("Soon: caste perks, medals, squad veterans"))), BranchMerged(Fighter));
			Row(TEXT("Human"), Tx(TEXT("Хочешь сыграть за Гиганта?"), TEXT("Want to play the Giant?")), Tx(TEXT("Режим «Человек против муравьёв»: кооп, меню «Кооператив»"), TEXT("«Human vs Ants»: co-op menu")), true);
			Row(TEXT("Wrench"), Tx(TEXT("Песочница"), TEXT("Sandbox")), IsDemo() ? FString(Tx(TEXT("в полной версии"), TEXT("in the full game"))) : FString(Tx(TEXT("Вся квартира и инструменты — в главном меню"), TEXT("The whole flat and tools — in the main menu"))), !IsDemo());
			}   // (page 0)
			// the pages: 0 rank and medal, 1 the Queen's gift, 2 the caste perk, 3 what opens
			Text(Cv, FString::Printf(TEXT("%d / 4"), (int32)T->DebriefPage + 1), CX + 540.f * K, SH - 78.f * K, EFont::Mono, 18.f * K, WithA(C::Sugar, 0.7f), 2);
			if (T->Phase == AAntTutorial::Debrief)
			{
				float W0 = 0.f;
				KeyCap(Cv, TEXT("Enter"), CX - 150.f * K, SH - 80.f * K, 18.f, W0);
				const bool bLast = T->DebriefPage >= 3;
				Text(Cv, bLast ? (IsDemo() ? Tx(TEXT("в первый рейд"), TEXT("to the first raid")) : Tx(TEXT("в колонию"), TEXT("to the colony"))) : Tx(TEXT("дальше"), TEXT("next")), CX - 138.f * K + W0, SH - 78.f * K, EFont::Bold, 22.f * K, C::Honey);
			}
		}

		// ---- the payoff stamp
		if (T->Phase == AAntTutorial::Payoff && T->BeatNo != BPrologue)
		{
			const float t = Now - Seen.PayAt;
			const float A = FMath::Clamp(t / 0.25f, 0.f, 1.f) * FMath::Clamp((3.6f - t) / 0.6f, 0.f, 1.f);
			const float Sc = 1.f + 0.35f * FMath::Max(0.f, 1.f - t / 0.25f);
			Stamp(Cv, Tx(TEXT("ВЫПОЛНЕНО"), TEXT("DONE")), SW * 0.5f, SH * 0.3f, -6.f, WithA(C::Leaf, A), 64.f * K * Sc);
			Text(Cv, BeatTitle(T->BeatNo), SW * 0.5f, SH * 0.3f + 56.f * K, EFont::Display, 28.f * K, WithA(C::Sugar, A), 1, 0.6f);
			Text(Cv, FString::Printf(TEXT("+%d XP"), B.Xp), SW * 0.5f, SH * 0.3f + 96.f * K, EFont::Display, 34.f * K, WithA(C::Honey, A), 1, 0.6f);
		}

		// ---- chapter card
		if (Now - Seen.ChapterAt < 3.f && !bFull)
		{
			const float t = Now - Seen.ChapterAt;
			const float A = FMath::Clamp(t / 0.3f, 0.f, 1.f) * FMath::Clamp((3.f - t) / 0.6f, 0.f, 1.f);
			Text(Cv, ChapterName(B.Chapter).ToUpper(), SW * 0.5f, SH * 0.18f, EFont::Display, 46.f * K, WithA(C::Amber, A), 1, 0.7f);
		}

		// ---- the line (bottom): portrait, speaker, typed text
		if (T->LineNo >= 0 && T->LineNo < LCount)
		{
			const FLine& L = GLines[T->LineNo];
			const FString Txt = Tx(L.Ru, L.En);
			const float t = Now - Seen.LineAt;
			const float A = FMath::Clamp(t / 0.2f, 0.f, 1.f);
			const float BW = FMath::Min(1100.f * K, SW - 80.f * K), BH = 118.f * K, X = (SW - BW) * 0.5f, Y = SH - BH - 92.f * K;
			Glass(Cv, X, Y, BW, BH, 0.86f * A);
			AntUiCanvas::Rect(Cv, X, Y, BW, 4.f * K, WithA(L.Voice == 1 ? C::Teal : L.Voice == 2 ? C::Jelly : C::Amber, A));
			if (UTexture2D* Por = AntUi::Tex(Portrait(L.Voice))) { Cover(Cv, Por, X + 10.f * K, Y + 10.f * K, 98.f * K, 98.f * K, FLinearColor(1.f, 1.f, 1.f, A)); }
			else { AntUiCanvas::Rect(Cv, X + 10.f * K, Y + 10.f * K, 98.f * K, 98.f * K, WithA(C::Cocoa, A)); }
			Text(Cv, Speaker(L.Voice).ToUpper(), X + 124.f * K, Y + 12.f * K, EFont::Bold, 16.f * K, WithA(L.Voice == 1 ? C::Teal : C::Honey, A));
			Wrap(Cv, Txt, X + 124.f * K, Y + 38.f * K, BW - 140.f * K, EFont::Body, 24.f * K, WithA(C::Sugar, A), 3, 1.2f, FMath::Max(1, (int32)(t * 55.f)));
		}

		// ---- the chapter fade
		if (T->Phase == AAntTutorial::Fade)
		{
			AntUiCanvas::Rect(Cv, 0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.f, FMath::Clamp((Now - T->PhaseStart) / 0.2f, 0.f, 1.f)));
			const bool bNewChapter = Seen.Beat == 255 || AntTut::Beat(Seen.Beat).Chapter != B.Chapter;
			if (bNewChapter) { Text(Cv, ChapterName(B.Chapter).ToUpper(), SW * 0.5f, SH * 0.45f, EFont::Display, 46.f * K, C::Amber, 1, 0.7f); }
			Text(Cv, BeatTitle(T->BeatNo), SW * 0.5f, SH * 0.45f + (bNewChapter ? 60.f : 0.f) * K, EFont::Bold, 26.f * K, C::Sugar, 1);
		}
	}
}

// =====================================================================================================================
// After the training: the Queen's gift; the Steam demo's raid count and end screen
// =====================================================================================================================
namespace AntTut
{
	void ApplyGift(AAntRaid* Raid)
	{
		if (!Raid || !ConsumeGift()) { return; }
#if ANT_TUT_COLONY
		TWeakObjectPtr<AAntRaid> WR = Raid;
		FTimerHandle H;
		Raid->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Raid, [WR]()
		{
			if (!WR.IsValid()) { return; }
			FAntColonyGift G;
			G.Food[AntFoodKind::Sugar] = 150; G.Food[AntFoodKind::Protein] = 80; G.Food[AntFoodKind::Fat] = 40; G.Food[AntFoodKind::Crumbs] = 120;
			G.Jelly = 2; G.Larvae = 1; G.Workers = 4;
			G.Chronicle = TEXT("Подарок Королевы выпускникам учебки");
			G.Announce = TEXT("Королева: «Мои дети… вы вернулись с тортом. Загляните в кладовые»");
			const bool bOk = AntColony::GiveGift(WR.Get(), G);
			UE_LOG(LogTemp, Display, TEXT("ANTTUT the Queen's gift to the colony (AntColony): %d"), bOk ? 1 : 0);
			// The Queen's reaction: her voice, and the colony screen opens at the stores where the gift lies.
			if (bOk && WR->GetNetMode() != NM_DedicatedServer)
			{
				AntTut::Bark(WR.Get(), 60, 9, 2);
				AntColonyView::Open(AntColony::Stores);
				UE_LOG(LogTemp, Display, TEXT("ANTTUT the colony screen opened at the Stores card"));
			}
		}), 3.f, false);
#else
		Raid->Nest.Food += 150;
		Raid->Nest.Jelly += 2;
		Raid->Nest.Larvae = FMath::Min(Raid->Nest.Larvae + 1, AntNest::MaxLarvae);
		Raid->Nest.Workers = FMath::Min(Raid->Nest.Workers + 4, AntNest::MaxWorkers);
		Raid->NestNote = TEXT("Подарок Королевы выпускникам учебки: +150 еды, +2 молочка, личинка и 4 рабочих — хватит на первую камеру");
		Raid->SaveNest();
		UE_LOG(LogTemp, Display, TEXT("ANTTUT the Queen's gift to the colony: food %d jelly %d"), Raid->Nest.Food, Raid->Nest.Jelly);
#endif
	}

	void DemoHud(AHUD* Hud, UCanvas* Cv, APlayerController* PC)
	{
		if (!IsDemo() || !Cv || !PC || Active(PC) || SandboxActive(PC)) { return; }
		const AAntRaid* Raid = AAntRaid::Get(PC->GetWorld());
		if (!Raid) { return; }
		static uint8 LastPhase = 255;
		static TWeakObjectPtr<const AAntRaid> LastRaid;
		FProgress& P = Progress();
		if (LastRaid.Get() != Raid) { LastRaid = Raid; LastPhase = Raid->Phase; }
		if (Raid->Phase != LastPhase)
		{
			if (LastPhase == AntRaidPhase::Raid && Raid->Phase == AntRaidPhase::Results)
			{
				++P.DemoRaids;
				if (AntVersus::Active(PC->GetWorld())) { P.bDemoVersus = true; }
				SaveProgress();
				UE_LOG(LogTemp, Display, TEXT("ANTDEMO raid %d of %d played"), P.DemoRaids, DemoRaids);
			}
			if (Raid->Phase == AntRaidPhase::Nest && P.DemoRaids >= DemoRaids && !P.bDemoEnded)
			{
				P.bDemoEnded = true;
				SaveProgress();
				if (AAntPlayerController* APC = Cast<AAntPlayerController>(PC)) { APC->OpenUiScreen(AntUiScreen::DemoEnd); }
				if (FParse::Param(FCommandLine::Get(), TEXT("AntDemoAuto")))
				{
					// The scripted demo run: a picture of the end screen, then out.
					TWeakObjectPtr<APlayerController> WPC = PC;
					FTimerHandle H1, H2;
					PC->GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WPC]() { if (WPC.IsValid()) { UE_LOG(LogTemp, Display, TEXT("ANTDEMO shot end_screen")); WPC->ConsoleCommand(TEXT("Shot showui"), false); } }), 3.f, false);
					PC->GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([]() { UE_LOG(LogTemp, Display, TEXT("ANTDEMO quit (-AntDemoAuto)")); FPlatformMisc::RequestExit(false); }), 8.f, false);
				}
				UE_LOG(LogTemp, Display, TEXT("ANTDEMO the end screen"));
			}
			LastPhase = Raid->Phase;
		}
		// The scripted demo run (-AntDemoAuto): the host starts the next raid a few seconds into the nest.
		static double NestSince = -1.0;
		if (FParse::Param(FCommandLine::Get(), TEXT("AntDemoAuto")) && PC->GetWorld()->GetNetMode() != NM_Client && Raid->Phase == AntRaidPhase::Nest && !P.bDemoEnded)
		{
			if (NestSince < 0.0) { NestSince = FPlatformTime::Seconds(); }
			if (FPlatformTime::Seconds() - NestSince > 5.0) { NestSince = -1.0; UE_LOG(LogTemp, Display, TEXT("ANTDEMO auto: start raid %d"), P.DemoRaids + 1); const_cast<AAntRaid*>(Raid)->StartRaid(); }
		}
		else { NestSince = -1.0; }
		GK = FMath::Max(0.7f, Cv->ClipY / 1080.f);
		const float K = GK;
		const FString Tag = FString::Printf(TEXT("%s  ·  %s %d / %d"), Tx(TEXT("ДЕМО"), TEXT("DEMO")), Tx(TEXT("рейд"), TEXT("raid")), FMath::Min(P.DemoRaids + (Raid->Phase == AntRaidPhase::Raid ? 1 : 0), DemoRaids), DemoRaids);
		Glass(Cv, 20.f * K, Cv->ClipY - 54.f * K, 260.f * K, 34.f * K, 0.6f);
		Text(Cv, Tag, 32.f * K, Cv->ClipY - 49.f * K, EFont::Bold, 18.f * K, C::Honey);
	}
}

// =====================================================================================================================
// The sandbox («Песочница»): unlocked after the training. The lite admin panel on [F2] (the full one stays on [F10]): the chains'
// props at the crosshair, the Giant, the time of day, the TV, the vacuum, the weather, the tap, the reset. The buttons send plain
// text commands (the same as -AntDo and the admin line); the few toggles keep their state in "tut sbx <name>".
// =====================================================================================================================
void AAntTutorial::SandboxAction(const TArray<FString>& A)
{
	const FString N = A.IsValidIndex(2) ? A[2].ToLower() : FString();
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	static int32 TvCh = 0, Wx = 0;
	static bool bTap = false, bVac = false;
	if (N == TEXT("giant")) { SandboxTool(PC, 3); }
	else if (N == TEXT("tod")) { SandboxTool(PC, 4); }
	else if (N == TEXT("food")) { SandboxTool(PC, 2); }
	else if (N == TEXT("wet")) { SandboxTool(PC, 6); }
	else if (N == TEXT("reset")) { SandboxTool(PC, 7); }
	else if (N == TEXT("tvch")) { TvCh = (TvCh + 1) % 9; Command(TEXT("tv on")); Command(FString::Printf(TEXT("tv ch %d"), TvCh)); }
	else if (N == TEXT("weather")) { Wx = (Wx + 1) % 3; Command(FString::Printf(TEXT("life weather %d"), Wx)); }
	else if (N == TEXT("vac")) { bVac = !bVac; Command(bVac ? TEXT("life vacuum") : TEXT("life vacuum stop")); }
	else if (N == TEXT("tap")) { bTap = !bTap; Command(bTap ? TEXT("fluid tap on 5") : TEXT("fluid tap off")); }
	UE_LOG(LogTemp, Display, TEXT("ANTTUT sandbox action %s"), *N);
}

namespace
{
	TWeakPtr<SWidget> GSbPanel;

	TSharedRef<SWidget> MakeSandboxPanel(AAntPlayerController* PC)
	{
		TWeakObjectPtr<AAntPlayerController> WP = PC;
		const FLinearColor Gold(1.f, 0.78f, 0.25f), Blue(0.6f, 0.8f, 1.f), Red(1.f, 0.55f, 0.45f);
		TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
		TSharedPtr<SWrapBox> Row;
		auto Section = [&Col, &Row, Gold](const FString& Title)
		{
			Col->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
			[
				SNew(STextBlock).Text(FText::FromString(Title)).Font(AntUi::Font(AntUi::EFont::Bold, 16)).ColorAndOpacity(Gold)
			];
			Col->AddSlot().AutoHeight()
			[
				SAssignNew(Row, SWrapBox).UseAllottedSize(true)
			];
		};
		auto Btn = [&Row, WP](const FString& Label, const FString& Cmd, const FLinearColor& Color = FLinearColor(0.85f, 0.85f, 0.85f))
		{
			Row->AddSlot().Padding(3.f)
			[
				SNew(SButton)
				.ButtonStyle(&AntUi::ButtonStyle())
				.ContentPadding(FMargin(9.f, 5.f))
				.ButtonColorAndOpacity(Color)
				.OnClicked_Lambda([WP, Cmd]()
				{
					if (AAntPlayerController* P = WP.Get())
					{
						const FVector At = P->CrosshairPoint() + FVector(0.f, 0.f, 30.f);
						FString S = Cmd;
						S = S.Replace(TEXT("{X}"), *FString::Printf(TEXT("%.0f"), At.X)).Replace(TEXT("{Y}"), *FString::Printf(TEXT("%.0f"), At.Y)).Replace(TEXT("{Z}"), *FString::Printf(TEXT("%.0f"), At.Z));
						P->Admin(S);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(AntUi::Font(AntUi::EFont::Bold, 14)).ColorAndOpacity(FLinearColor::Black)
				]
			];
		};
		Section(AntTut::Tx(TEXT("Реквизит цепных реакций (в точку прицела)"), TEXT("Chain props (at the crosshair)")));
		Btn(AntTut::Tx(TEXT("Облако перца"), TEXT("Pepper cloud")), TEXT("rx cloud pepper {X} {Y} {Z} 900"));
		Btn(AntTut::Tx(TEXT("Облако муки"), TEXT("Flour cloud")), TEXT("rx cloud flour {X} {Y} {Z} 900"));
		Btn(AntTut::Tx(TEXT("Лёд"), TEXT("Ice")), TEXT("rx ice {X} {Y} {Z} 8"));
		Btn(AntTut::Tx(TEXT("Горох"), TEXT("Peas")), TEXT("rx peas {X} {Y} {Z} 20"));
		Btn(AntTut::Tx(TEXT("Шоколад"), TEXT("Chocolate")), TEXT("rx choc {X} {Y} {Z}"));
		Btn(AntTut::Tx(TEXT("Чихнёт Гигант"), TEXT("Giant sneezes")), TEXT("rx sneeze dad"), Red);
		Btn(AntTut::Tx(TEXT("Ослепить Гиганта"), TEXT("Blind the Giant")), TEXT("rx blind"), Red);
		Btn(AntTut::Tx(TEXT("Свет: выкл"), TEXT("Power off")), TEXT("power off"), Red);
		Btn(AntTut::Tx(TEXT("Свет: вкл"), TEXT("Power on")), TEXT("power on"));
		Btn(AntTut::Tx(TEXT("Замыкание"), TEXT("Short circuit")), TEXT("power short"), Red);
		Section(AntTut::Tx(TEXT("Цепочки целиком"), TEXT("Whole chains")));
		for (int32 c = 7; c <= 22; ++c) { Btn(FString::Printf(TEXT("C%d"), c), FString::Printf(TEXT("chain C%d"), c)); }
		Section(AntTut::Tx(TEXT("К реквизиту"), TEXT("Go to a prop")));
		for (const TCHAR* K : { TEXT("pepper"), TEXT("flour"), TEXT("ice"), TEXT("peas"), TEXT("slingshot"), TEXT("choc"), TEXT("toaster"), TEXT("books"), TEXT("micro"), TEXT("mentos"), TEXT("sponge"), TEXT("balloon"), TEXT("strip"), TEXT("breaker") })
		{
			Btn(K, FString::Printf(TEXT("rx tp %s"), K), Blue);
		}
		Section(AntTut::Tx(TEXT("Квартира"), TEXT("The flat")));
		Btn(AntTut::Tx(TEXT("Гигант: стоп / ход"), TEXT("Giant: stop / go")), TEXT("tut sbx giant"));
		Btn(AntTut::Tx(TEXT("Время суток"), TEXT("Time of day")), TEXT("tut sbx tod"));
		Btn(AntTut::Tx(TEXT("ТВ: вкл"), TEXT("TV on")), TEXT("tv on"));
		Btn(AntTut::Tx(TEXT("ТВ: выкл"), TEXT("TV off")), TEXT("tv off"));
		Btn(AntTut::Tx(TEXT("ТВ: канал"), TEXT("TV: channel")), TEXT("tut sbx tvch"));
		Btn(AntTut::Tx(TEXT("Пылесос: вкл / выкл"), TEXT("Vacuum: on / off")), TEXT("tut sbx vac"));
		Btn(AntTut::Tx(TEXT("Погода за окном"), TEXT("Weather outside")), TEXT("tut sbx weather"));
		Btn(AntTut::Tx(TEXT("Кран: вкл / выкл"), TEXT("Tap: on / off")), TEXT("tut sbx tap"));
		Btn(AntTut::Tx(TEXT("Потоп / сушь"), TEXT("Flood / dry")), TEXT("tut sbx wet"));
		Btn(AntTut::Tx(TEXT("Еда у прицела"), TEXT("Food ahead")), TEXT("tut sbx food"));
		Btn(AntTut::Tx(TEXT("СБРОС уровня"), TEXT("RESET the level")), TEXT("tut sbx reset"), Red);

		return SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 160.f, 24.f, 0.f))
		[
			SNew(SBox).WidthOverride(560.f).MaxDesiredHeight(780.f)
			[
				SNew(SBorder).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.82f)).Padding(12.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(AntTut::Tx(TEXT("ПЕСОЧНИЦА · панель  ([F2] — закрыть)"), TEXT("SANDBOX · panel  ([F2] closes)")))).Font(AntUi::Font(AntUi::EFont::Display, 24)).ColorAndOpacity(Gold)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SScrollBox) + SScrollBox::Slot()[ Col ]
					]
				]
			]
		];
	}
}

namespace AntTut
{
	void ToggleSandboxPanel(APlayerController* PCIn)
	{
		AAntPlayerController* PC = Cast<AAntPlayerController>(PCIn);
		if (!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport || !SandboxActive(PC)) { return; }
		if (GSbPanel.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GSbPanel.Pin().ToSharedRef());
			GSbPanel.Reset();
			FInputModeGameOnly Mode;
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = false;
			UE_LOG(LogTemp, Display, TEXT("ANTTUT sandbox panel closed"));
			return;
		}
		if (PC->GetWorld()->GetNetMode() == NM_Client) { return; }   // the host's tools
		TSharedRef<SWidget> W = MakeSandboxPanel(PC);
		GSbPanel = W;
		GEngine->GameViewport->AddViewportWidgetContent(W, 85);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(true);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
		UE_LOG(LogTemp, Display, TEXT("ANTTUT sandbox panel open"));
	}
}
