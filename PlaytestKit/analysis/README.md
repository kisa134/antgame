# PlaytestKit analysis

```powershell
pip install --user pandas numpy matplotlib      # Python 3.11+

# 1) top-down map for the heatmaps (Ant Kitchen; one time, or after level changes)
python projects\make_topdown_ant_kitchen.py --level ..\..\docs\level\level_v3.json --out maps\ant_kitchen_topdown.png

# 2) report from local folders / zips from testers / Supabase
python ptk_report.py C:\ptk_data --config projects\ant-kitchen.json --out report.html
python ptk_report.py supabase --project ant-kitchen --url https://REF.supabase.co --key <service_role> --out report.html
python ptk_report.py C:\ptk_data --build 0.9.1          # one build only

# no data yet? synthetic sessions in the real format:
python make_sample_data.py C:\tmp\ptk_sample 6 && python ptk_report.py C:\tmp\ptk_sample --out sample.html
```

Output: `report.html` (self-contained, charts as base64) and `report_charts/*.png`.

A new project: copy `projects/ant-kitchen.json`, change `project`, `map` (image + world bounds; if `maps/<image>.json` exists next to the image it overrides the bounds), `hypotheses`, `feature_events`, `death_causes_ru`. Sections not covered by data print "нет данных" instead of failing; a section that throws shows the error in the report and the rest still renders.
