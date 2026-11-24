# Карта проекта (Tools/projectmap)

- `build_map.py` собирает `project/map/map.json` и `project/map/SUMMARY.md` из репозитория (код, docs, Content, git, gh) и ручных YAML в `project/map/`.
- `apply_edits.py edits.json` вливает правки, сделанные на интерактивной странице, в `project/map/overrides.yaml`.
- `viewer/index.html` исходник страницы (публикуется как артефакт claude.ai, данные в `data/map.json`).
- Запуск: `pip install --user pyyaml`, затем `python Tools/projectmap/build_map.py [--no-gh]`.
