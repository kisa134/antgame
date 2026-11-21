# Audio credits and licences

Ant Kitchen is a paid Steam game: every sound in it must be ours or under a licence that allows commercial use without attribution
conditions we cannot meet. Nothing here is "free for non-commercial use". Add a row for every new file that comes from outside.

## 1. Music — generated in-house (owned)

All music is synthesized by our own code: `Tools/audio/synth.py` (instruments, drums, reverb) and `Tools/audio/make_music.py`
(the compositions: chords, bass lines, melodies, arrangement, mix). No samples, no loops, no third-party recordings are used.
Composition and code: Ant Kitchen team (generated 2026-11-21). Licence: ours.

| File (SourceArt/Audio/Music → /Game/Audio/Music) | What | Length |
|---|---|---|
| M_Funk_Base / _Tension / _Action / _Urgency | Raid set 1, D-dorian funk, 98 BPM, 48 bars, 4 synchronized stems | 117.6 s loop |
| M_Lounge_Base / _Tension / _Action / _Urgency | Raid set 2, A-minor lounge / lo-fi bossa, 88 BPM, 40 bars, 4 stems | 109.1 s loop |
| M_Nest | «Марш Великой Колонии Щели» — the colony march (nest / menu), Bb, 100 BPM, 32 bars | 76.8 s loop |
| M_Sting_RaidStart, _Win, _Fail, _Card, _LastLight | Stings | 3–5 s |
| Sfx: A_DeliverChime_1, A_CardPick_1, A_TrailCut_1, A_AmbClock_1 | UI chimes, the trail snip, the wall clock (synthesized) | < 8 s |

## 2. Sound effects — generated with Higgsfield (mirelo_text_to_audio), owned output

Generated 2026-11-21 through the Higgsfield account of the project (paid credits; generated output is ours to use commercially under
the Higgsfield terms of the paying account). Raw takes are listed with their source URLs in `Tools/audio/raw_manifest.txt`
(downloaded to `Saved/AudioRaw/`, not committed). `Tools/audio/make_sfx.py` cuts the takes into variations, trims, levels,
adds the sub-bass "macro" layer to the big impacts and crossfades the loop seams.

| Game files (SourceArt/Audio/Sfx → /Game/Audio/Sfx) | Raw take (raw_manifest.txt name) |
|---|---|
| A_StepTile_1-5, A_StepWoodN_1-5, A_StepRug_1-5, A_StepMetalN_1-5, A_StepSticky_1-5 | step_tile, step_wood, step_rug, step_metal, step_sticky |
| A_BiteN_1-5, A_SpitN_1-4, A_AcidHit_1-3, A_GrabN_1-3, A_Release_1-3, A_AntHurt_1-3 | bite, spit, acid_sizzle, grab, release, ant_hurt |
| A_CarryStrain_1-3, A_CarryThump_1-4, A_RipOff_1, A_DeliverCheer_1, A_TrailLay_1-3, A_WorkerPatter_1 | carry_strain, carry_thump, rip_tumble, crowd_cheer, trail_lay, worker_patter |
| A_ScrewTurn_1-3, A_ScrewDrop_1-3, A_HingeCreak_1-3, A_Wobble_1, A_CurtainRustle_1 | screw_unscrew, screw_drop, hinge_creak, wobble_creak, curtain_rustle |
| A_CrashWood_1, A_CrashGlass_1, A_CrashCeramic_1, A_CrashMetal_1, A_CrashPlastic_1 | tip_wood, crash_glass, crash_ceramic, crash_metal, crash_plastic |
| A_SplatJam_1-3, A_SplatCream_1-3, A_ColaFizz_1, A_Crumbs_1 | splat_jam, splat_cream, cola_fizz, crumbs |
| A_HumanStepN_1-4, A_SlipperSlap_1-2, A_HumanGrumble_1-2, A_HumanRoar_1-2, A_HumanSlip_1 | human_step, slipper_slap, human_grumble, human_roar, human_slipfall |
| A_BatWhoosh_1-2, A_BatImpact_1-2, A_HumanPickup_1-2, A_HumanSniff_1 | bat_whoosh, bat_impact, human_pickup, human_sniff |
| A_CatPurr_1, A_CatHiss_1-2, A_CatPounce_1, A_DogSnort_1-2, A_DogBark_1-3, A_DogSniff_1-2, A_DogEat_1-2 | cat_purr, cat_hiss, cat_pounce, dog_snort, dog_bark, dog_sniff, dog_eat |
| A_Broom_1-2, A_SwatterN_1, A_KidGiggle_1-2, A_KidShout_1, A_SpiderSkitter_1, A_CardFlip_1-2 | broom, swatter, kid_giggle, kid_shout, spider_skitter, card_flip |
| A_AmbFridge_1, A_AmbDrip_1, A_AmbDay_1, A_AmbEvening_1, A_AmbNight_1 | amb_fridge, amb_drip, amb_day, amb_evening, amb_night |

Credits spent: 90.25 (361 s of audio at 0.25 credit/s, 59 takes, no retries needed).

## 3. Older sounds still in use

`Content/SoundsReal/S_*` — 38 ElevenLabs sound effects generated earlier for the project (docs/FEATURES.md section on sound),
generated output of the project's paid ElevenLabs account.

## 4. Downloaded material

None. (Allowed sources if ever needed: CC0 only — FreePD.com, public-domain recordings on Wikimedia Commons / Musopen, Kenney
and the Sonniss GDC bundles after reading their licence text. Record URL + licence here.)
