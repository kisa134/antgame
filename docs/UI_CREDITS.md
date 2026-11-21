# UI credits and licences (code/ANT-ui)

## Art — generated with Higgsfield

All interface pictures in `SourceArt/UI/` (imported to `/Game/UI/`) were generated in **Higgsfield** with the model
**GPT Image 2.6** (`gpt_image_2_6`, quality high, 1k–2k) from prompts written for this project, on Yura's account, then cut and
sized by `Tools/ui_art_prep.py` (icon sheets → single icons with a flood-fill cut-out, crops, sizes). Procedural textures
(tape, stamp frames, dot, vignette, fades, grain, scratches, torn edge) are made by `Tools/ui_art_procedural.py`.

| Set | Count | Files |
|---|---|---|
| Backgrounds (21:9 ×3, 16:9 ×3) + newsreel plates ×2 | 8 | `Backgrounds/T_UI_Bg*`, `T_UI_News*` |
| Loading key art | 6 | `Loading/T_UI_Load*` |
| Portraits (Queen, Zhvalov, Gruzd, Usik, Tikhon, Kapelka, Vintik) | 7 | `Portraits/T_UI_Por*` |
| Card illustrations | 18 | `Cards/T_UI_Card*` |
| Icons (5 sheets of 16) | 80 | `Icons/T_UI_Icon_*` |
| Logo, emblem, studio card | 3 | `Logo/` |
| Paper and cardboard textures | 2 | `Materials/T_UI_Paper`, `T_UI_Cardboard` |

Generated: 52 images, ≈83 credits (2k high = 2, 1k high = 1.25 credits each). Rejected and regenerated: the first logo (sun-ray background reads as a national symbol),
the first recruit card (a hand salute). The style of portraits is "A" (stylized 3D); the user has not chosen between A and D yet.

## Fonts — SIL Open Font License 1.1 (licence texts next to the fonts in `Content/UI/Fonts/`)

| Font | Author | File | Licence |
|---|---|---|---|
| Russo One | Jovanny Lemonad | `RussoOne-Regular.ttf` | OFL 1.1 (`OFL-RussoOne.txt`) |
| PT Sans Narrow Regular/Bold | ParaType | `PTSansNarrow-*.ttf` | OFL 1.1 (`OFL-PTSansNarrow.txt`) |
| PT Mono | ParaType | `PTMono-Regular.ttf` | OFL 1.1 (`OFL-PTMono.txt`) |
| Neucha | Jovanny Lemonad | `Neucha-Regular.ttf` | OFL 1.1 (`OFL-Neucha.txt`) |

Source: github.com/google/fonts (`ofl/…`). OFL allows bundling in a commercial game; the fonts must not be sold alone and the
reserved names stay with the originals.

## Music / sound

Placeholders in the credits screen ("coming soon"). UI sounds call `AntUiSound()` (`AntUiSound.cpp`) — a stub that plays
existing effects until AntAudio supplies the real ones.
