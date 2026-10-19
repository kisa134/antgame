# Звуки Ant Kitchen

Все звуки сейчас — **временные заглушки, синтезированные кодом** (`SourceArt/Sounds/*.wav`).
Замени любую: сгенерь файл с **ровно таким именем**, положи `.wav` в «Загрузки» и скажи — импортирую.
Где генерить: **ElevenLabs → Sound Effects** (лучше всего для SFX), либо Stable Audio / freesound.org.
Формат: WAV или MP3, моно, короткие (длительность указана), без тишины в начале.
«Петля» = звук должен бесшовно зацикливаться (в ElevenLabs включи *Loop*).

Масштаб: мы — муравей размером с крошку. Всё огромное (Человек, тапок) звучит **низко и гулко**,
всё наше (лапки, плевки, укусы) — **тонко, сухо, щёлкающе**, как макро-запись.

## Муравей и бой (самое частое — делай первыми)

| Файл | Длит. | Промпт (EN) |
|---|---|---|
| `S_Step` | 0.05 с | `tiny insect leg tapping on a hard kitchen tile, single dry chitin click, macro foley, very short` (сделай 4 варианта: `S_Step_1..4`) |
| `S_Spit` | 0.3 с | `small wet acid spit shot, juicy pfft with a tiny sizzle tail, cartoony but crisp` |
| `S_Splat` | 0.4 с | `small acid blob splatting and fizzing on a surface, wet splat plus short hiss` |
| `S_Bite` | 0.25 с | `insect mandibles snapping shut, sharp double crunchy click, aggressive` |
| `S_Hit` | 0.1 с | `satisfying hitmarker tick, short bright click for a video game hit confirmation` |
| `S_Crit` | 0.5 с | `critical hit ding, bright metallic ping with sparkle, rewarding, game UI` |
| `S_Kill` | 0.6 с | `kill confirm sound, punchy two-tone chime with a subtle bass thump, very rewarding, game UI` |
| `S_Squish` | 0.5 с | `small bug getting squished, crunchy wet squelch with a tiny squeak` |
| `S_Jump` | 0.15 с | `tiny insect hop, quick light whoosh blip` |

## Еда и лут

| Файл | Длит. | Промпт |
|---|---|---|
| `S_Grab` | 0.2 с | `ant grabbing a crumb with mandibles, light crunchy grab with a soft pop` |
| `S_Drop` | 0.3 с | `small piece of food dropping on a kitchen counter, soft thud with a little bounce` |
| `S_Crunch` | 0.6 с | `chocolate bar shattering into pieces, crispy crunchy break with crumbs scattering` |
| `S_Deliver` | 1.2 с | `rewarding treasure delivered jingle, warm ascending marimba arpeggio with a sparkle, coin-like, dopamine` |
| `S_Pickup_Gear` | 0.6 с | `weapon pickup, mechanical clack plus magical shimmer, loot in a looter shooter` (новый) |
| `S_Legendary` | 1.5 с | `legendary loot drop, glorious shimmering choir hit with sparkles` (новый) |

## Человек

| Файл | Длит. | Промпт |
|---|---|---|
| `S_Stomp` | 1.2 с | `giant slipper stomping a kitchen floor heard from an insect's point of view, huge deep boom, floor shakes, low rumble tail` |
| `S_Spray` | 2 с, **петля** | `aerosol bug spray can spraying continuously, harsh hiss, loopable` |
| `S_Swat` | 1 с | `rolled newspaper slamming down on a table, massive whack from tiny perspective, deep thwack with paper rustle` (новый) |
| `S_HumanStep` | 0.8 с | `heavy human footstep in slippers on tiles heard from the floor, deep muffled thump` (новый, 3 варианта) |
| `S_Rage` | 1.5 с | `angry man grunt and growl, frustrated, short, from far above` |
| `S_HumanPanic` | 1.5 с | `man yelping and running away in panic, comedic` (новый) |

## Существа

| Файл | Длит. | Промпт |
|---|---|---|
| `S_Buzz` | 2 с, **петля** | `housefly buzzing close to the microphone, steady annoying drone, loopable` |
| `S_Web` | 0.4 с | `spider shooting silk, quick stretchy thwip with a sticky snap` |
| `S_Hiss` | 0.8 с | `large spider hiss and clicking chelicerae, creepy, close` |
| `S_SpiderStep` | 0.1 с | `heavy spider leg tapping, hairy soft tick` (новый) |

## Кухня (атмосфера)

| Файл | Длит. | Промпт |
|---|---|---|
| `S_Ambience` | 30 с, **петля** | `quiet kitchen room tone, refrigerator hum, distant clock ticking, soft air, heard from the floor, loopable` |
| `S_Zap` | 0.7 с | `electric spark from a frayed wire, crackling zap with a buzz` |
| `S_Sizzle` | 1 с | `small insect catching fire, crackle and sizzle` |
| `S_Splash` | 0.6 с | `water drop splash at tiny scale, fire extinguished hiss` |
| `S_Faucet` | 3 с, **петля** | `kitchen faucet running into a sink, loopable` (новый) |
| `S_FridgeDoor` | 1.5 с | `refrigerator door opening with suction pop and hum swelling` (новый) |
| `S_Toaster` | 1 с | `toaster lever clunk then loud spring ding pop` (новый) |
| `S_ArmyMarch` | 3 с, **петля** | `thousands of tiny insect legs marching on tiles, dense rustling clicking carpet of sound, loopable` (новый) |
| `S_ArmyCharge` | 1.5 с | `swarm of ants surging forward, rising rustle crescendo with a war horn feel, tiny scale` (новый) |

## Музыка (по желанию)

| Файл | Длит. | Промпт |
|---|---|---|
| `M_Raid` | 2–3 мин, петля | `playful tense heist music, pizzicato strings and light percussion, sneaky but energetic, loopable` |
| `M_War` | 2 мин, петля | `epic tiny-scale battle music, tribal drums and staccato strings, comedic heroic, loopable` |
