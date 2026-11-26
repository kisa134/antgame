# QA v14.1 (branch code/ANT-qa)

Base: code/ANT-v14 (e2ff0e3). Tested on a packaged Win64 Development build, UE 5.8, offscreen, 720p/1080p.

## Fixed

| id | area | symptom | repro | cause | fix | status |
|----|------|---------|-------|-------|-----|--------|
| QA-01 | materials | Facade material fell back to the default shader in the cooked game | cook, look at any building facade | Tools/life/make_life_materials.py connected the "RG" pin (not a pin) so the custom node lost input 4 (Cell); SM5 compile failed | connect "RGB", use Cell.xy; M_Life_Facade.uasset regenerated | fixed 17775a7 |
| QA-02 | life world | about 760 warnings per 5 min session ("has to be Movable") | start any raid, read log | AAntLifeWorld::TickRoom moves the static SM_V3_Phone piece every frame | Ph->SetMobility(Movable) in phone setup | fixed e61c54d |
| QA-03 | colony UI | at 1280x720 the title runs under the emblem, subtitle under the fighter/barracks buttons, chronicle label overlaps its text | open colony screen at 1280x720 | fixed pixel positions in DrawTopBar/DrawBottom | TextFit with max widths, chronicle line moved to x=158 | fixed e7b24b8 |

## Open issues

| id | area | symptom | repro | note |
|----|------|---------|-------|------|
| QA-04 | art | 277 SkipPackage warnings for /Game/Map4/Props/SM_M4R_* (Fridge, WallShelf, Road, Kerb, Tree...); window tree is a flat dark disc | any raid, read log | art gap, code falls back to SM_K_*/SM_S_* |
| QA-05 | art | "Shard: no chunk for ..., plain model used" about 12 per session | raid seed 7 | missing chunk assets |
| QA-06 | materials | M_Life_Screen "applied to a non-external Texture2D sampler" x15 per session; cooking TV channel washed out | tv on, tv ch 3 | sampler type in generated material |
| QA-07 | net | FNetGUIDCache::SupportsObject ... NOT Supported (about 24 per co-op session), mesh SM_M4_BoxSharp of KitchenBuilder | host + client | replicated reference to unstable-named static component; owner property not identified; low impact |
| QA-08 | dev | -AntListenPort=7810 binds 7777 (option ignored) | -AntHost -AntListenPort=7810 | dev-only; client must join 127.0.0.1:7777 |
| QA-09 | nav | "patrol stop (11500,-2100) is not free for his capsule" | seed 7 / 240 | 1 of 13 patrol stops blocked |
| QA-10 | ai | Human stuck on KitchenLoot body at Z about 1744 (counter) | tutorial run | seen once |
| QA-11 | colltest | `colltest 12 all qa` (487 jobs, 12.6 min): stall=561 (130 unique routes), fell=20, embed=3, stuck=1, stuck_other=6. Same with 30 walkers (558), so not load dependent. Top: top_SM_M4_BoxSharp 46, top_SM_K_Slipper 18, top_SM_S_Car_Wheel 16, ceil_plintus 14, top_SM_V3_Balloon 12 | `-AntDo="5:colltest 12 all qa"` | stall definition (6 s without progress, StartColl in Map4.cpp) may count on-top routes differently from the v4 doc baseline; needs a comparison against DistV14b with the same parameters |
| QA-12 | art | 23_worktop camera sits inside a dark-blue surface on seed 7; dark navy speckled slabs in shots 03/10 | art shots day | seed layout, unidentified |
| QA-13 | HUD 720p | card-pick overlay overlaps HUD center lines; "Vybrano" toast over card images; admin badge over HUD top-left (dev); bottom hints crowded | raid at 1280x720 | not fixed |
| QA-14 | input | One/Two/Three bound in both AAntPlayerController (Vote1-3) and AAntCharacter (PickClass1-3) | read code | check no double handling in match start |
| QA-15 | UI | HQ screen header button "MURAVEYNIK [Tab]" (x 372..586) sits under the fighter/barracks buttons (x 300..590); dig card over the storage label | open HQ at 720p | same class as QA-03 |

## Not done in this pass
Perf measurements at 1440p, seeds 41/117 solo, versus, co-op feature sync checks, DistQA packaging and final smoke.