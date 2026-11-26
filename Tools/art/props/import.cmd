@echo off
rem import.cmd [names joined by +] : M_HeroGlass + break MIs, then the hero_props set into /Game/Hero
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\art\make_hero_glass.py" glass
set ANT_ART_SETS=hero_props
set ANT_ART_REPORT=hero_props_report.json
set ANT_HERO_NAMES=%~1
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\import_blender_art.py" import
