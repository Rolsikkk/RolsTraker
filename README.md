# RolsTraker 🎯

![C++](https://img.shields.io/badge/C++-20-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Version](https://img.shields.io/github/v/release/Rolsikkk/RolsTraker)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D6.svg)

[🇺🇸 English](#english) | [🇷🇺 Русский](#русский)

---

<a name="english"></a>
## 🇺🇸 English

A lightning-fast, portable, terminal-based **Valorant stats tracker and lobby companion** built in C++20 with [FTXUI](https://github.com/ArthurSonzogni/FTXUI). It automatically detects your live lobby, displays detailed player stats directly from Tracker.gg, shows the selected map, and provides an instant one-click agent auto-picker!

### ✨ Key Features
- 👥 **Background Party & Stack Detection (Recent 20 Games)**: Scans the last 20 matches of all lobby players in the background to detect who is queued together! Finds Duos, Trios, 4-stacks, and 5-stacks for both allies and enemies (where Riot completely hides party IDs), displaying vibrant color-coded badges (`#1 ДУО`, `#2 ТРИО`) and shared match counts.
- ⚡ **Booster Detection via Equipped Weapon Skins**: Automatically inspects player weapon loadouts in the background. If a player has $\le 2$ custom skins across their entire 19-weapon loadout, they are flagged as `[БУСТЕР ⚡]` with skin counts and warning banners.
- 🎨 **In-Game Weapon Skins Inspection**: Click on any player in the lobby to see their complete weapon loadout! All 19 weapons are neatly categorized into *Sidearms, SMGs, Shotguns, Rifles, Snipers, Heavies, Melee* exactly like in the in-game Valorant Arsenal, complete with chroma variants and custom skin highlights (`★`).
- ⚡ **Instant Agent Auto-Picker**: Pick and lock any unlocked agent in seconds! Browse agents by roles (*Duelists, Initiators, Controllers, Sentinels*) or press `[P]` / click the picker button during Agent Select.
- 🗺️ **Map Detection & Banner**: Clearly displays the current map prominently as soon as the pre-game lobby is generated.
- 📊 **Tracker.gg Stats Integration**: View accurate K/D, Headshot %, ACS, Winrate, and competitive ranks for all players in the lobby (with clean handling for private profiles).
- 📱 **Compact & Responsive UI**: Optimized layout designed to fit comfortably on any screen orientation, including vertical 1080×1920 displays and standard desktop resolutions without text clipping.
- 🎮 **Discord Rich Presence (RPC)**: Shows your live match status, map name, and persistent match timer directly on your Discord profile.
- 🚪 **Quick Match Dodge**: Dodge safely from agent select with penalty status tracking when needed.
- ⚡ **Zero Lag & Multithreaded**: Low-overhead asynchronous polling ensures smooth performance and zero in-game ping spikes.
- 🔄 **Integrated Auto-Updater**: Automatically checks and downloads the newest release from GitHub.

### ⚙️ How It Works
1. **Riot Local Client API**: Securely connects to your local Valorant Client via the Riot lockfile.
2. **Session Authentication**: Safely fetches local session tokens (`AccessToken`, `EntitlementsToken`).
3. **Live State Polling**: Non-blocking background worker detects Menu, Pre-Game (Agent Select), or Core-Game (Live Match).
4. **Data Aggregation**: Retrieves player PUUIDs, queries competitive tiers, and fetches Tracker.gg statistics asynchronously.

### 📥 Download & Usage
1. Download the latest `RolsTraker.zip` or `RolsTraker.exe` from the [Releases](https://github.com/Rolsikkk/RolsTraker/releases/latest) page.
2. Extract the files and ensure Valorant is running.
3. Launch `RolsTraker.exe` — enjoy real-time lobby intelligence!

---

<a name="русский"></a>
## 🇷🇺 Русский

Молниеносный, автономный консольный **трекер статистики и помощник для Valorant**, написанный на C++20 с использованием библиотеки [FTXUI](https://github.com/ArthurSonzogni/FTXUI). Автоматически определяет лобби, загружает актуальную статистику игроков с Tracker.gg, отображает выпавшую карту и позволяет мгновенно выбирать (авто-пикать) агентов в один клик.

### ✨ Основные возможности
- 👥 **Определение пачек по последним 20 играм (в фоне)**: Автоматически сканирует историю последних 20 матчей игроков прямо во время игры и находит, кто играет вместе! Безошибочно выявляет дуо, трио, 4-стаки и фулл-пати не только среди союзников, но и во вражеской команде (где Riot намеренно скрывает Party ID), с цветными бейджами (`#1 ДУО`, `#2 ТРИО`) и количеством общих матчей.
- ⚡ **Детект бустеров по скинам оружия**: Фоновый анализ арсенала скинов игроков. Если у игрока в активной экипировке нет скинов или всего 1-2 скина на все 19 видов оружия, программа маркирует его как `[БУСТЕР ⚡]` прямо в таблице лобби и в карточке игрока.
- 🎨 **Просмотр скинов оружия игроков в матче**: Нажмите на имя любого игрока в лобби, чтобы увидеть его полный экипированный арсенал оружия! Все 19 видов оружия аккуратно рассортированы по категориям (*Пистолеты, Пистолеты-пулемёты, Дробовики, Винтовки, Снайперские винтовки, Пулемёты, Холодное оружие*) точно так же, как в меню «Коллекция» в самом Валоранте, с отображением расцветок (хромов) и подсветкой особых скинов (`★`).
- ⚡ **Мгновенный авто-пик агентов**: Выбирайте и фиксируйте любого доступного агента за долю секунды! Удобная сортировка по ролям (*Дуэлянты, Зачинщики, Специалисты, Стражи*), открытие по кнопке или горячей клавише `[P]`.
- 🗺️ **Баннер карты в Pre-Game**: Крупное и четкое отображение выпавшей карты сразу после нахождения матча.
- 📊 **Статистика Tracker.gg**: Актуальные показатели K/D, % попаданий в голову (HS%), ACS, процент побед и текущий ранг для всех участников матча (с понятной плашкой «Закрыт» для приватных профилей).
- 📱 **Компактный и адаптивный интерфейс**: Полная оптимизация под любые разрешения и ориентации мониторов (включая вертикальные 1080×1920) — вся информация помещается без обрезания строк.
- 🎮 **Discord Rich Presence**: Отображение статуса матча, карты и непрерывного таймера игры в вашем профиле Discord.
- 🚪 **Уклонение от матча (Dodge)**: Быстрый выход из лобби выбора агентов с отслеживанием статуса штрафов.
- ⚡ **Высокая производительность**: Фоновый асинхронный опрос API без зависаний интерфейса и влияния на игровой пинг.
- 🔄 **Встроенный авто-апдейтер**: Автоматическая проверка и загрузка обновлений с GitHub.

### ⚙️ Принцип работы
1. **Подключение к Riot Client API**: Считывает локальный lockfile запущенного клиента игры.
2. **Аутентификация**: Получает актуальные локальные токены (`AccessToken`, `EntitlementsToken`).
3. **Отслеживание состояния**: Фоновый поток мгновенно распознает статус: Меню, Выбор агентов (Pre-Game) или Игра (Core-Game).
4. **Сбор статистики**: Получает PUUID игроков, опрашивает ранги и подгружает точные показатели с Tracker.gg.

### 📥 Установка и запуск
1. Скачайте свежий архив `RolsTraker.zip` или файл `RolsTraker.exe` со страницы [Релизов](https://github.com/Rolsikkk/RolsTraker/releases/latest).
2. Запустите Valorant.
3. Запустите `RolsTraker.exe`.

---

## 📸 Поддержать проект / Support

Если вам понравилась программа, не забудьте поставить звёздочку! ⭐ / If you like this project, please star the repository! ⭐

[![Star History Chart](https://api.star-history.com/svg?repos=Rolsikkk/RolsTraker&type=Date)](https://star-history.com/#Rolsikkk/RolsTraker&Date)