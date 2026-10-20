# Онбординг в Ant Kitchen

1. **Аккаунты на одну почту:** Claude (Pro или Max), GitHub, Atlassian (приглашение в Jira придёт на почту), Epic Games.
2. **Доступы:** скинь Юре GitHub-ник → прими приглашение в репо. Прими приглашение в Jira https://antgame.atlassian.net
3. **Установка (Windows):**
   - Unreal Engine **5.8** (ровно эта версия) через Epic Games Launcher;
   - Visual Studio 2022 с компонентами «Разработка игр на C++» (галочка *Unreal Engine installer*) и «Разработка классических приложений на C++»;
   - Git + Git LFS, потом в терминале `git lfs install`. Для ручной работы удобен GitHub Desktop.
4. **Проект:** `git clone https://github.com/kisa134/antgame`, ветка `main`. Двойной клик по `AntKitchen.uproject` → на *Rebuild?* жми **Yes** (компиляция долгая) → **Play**.
5. **Claude:**
   - Settings → Connectors → **Atlassian** (на экране согласия выбери сайт **antgame**) и **GitHub**;
   - в Claude Desktop дай доступ к папке с репо;
   - первое сообщение: «Прочитай CLAUDE.md, README.md, docs/ и покажи мои задачи в Jira ANT».

Правила работы — в `CLAUDE.md`. Коротко: задача в Jira → ветка → `git lfs lock` перед правкой `.uasset` → PR с ключом задачи → отчёт в Jira.
