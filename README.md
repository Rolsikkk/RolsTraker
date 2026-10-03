# RolsTraker 🎯

![C++](https://img.shields.io/badge/C++-20-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Version](https://img.shields.io/github/v/release/Rolsikkk/RolsTraker)

[🇺🇸 English](#english) | [🇷🇺 Русский](#русский)

<a name="english"></a>
## 🇺🇸 English

A lightning-fast, terminal-based **Valorant stats tracker** built in C++. It automatically detects your live matches, shows ranks and stats for everyone in the lobby, and even gives you a live web link to share with your friends!

### ✨ Features
- 🚀 **Live Match Tracking**: Instantly detects when you enter an agent select or a live match.
- 📊 **Deep Stats**: View hidden MMR, current Ranks, K/D, Headshot %, Win/Loss ratios, and recent match history for everyone in your lobby.
- 🌐 **Live Web Sharing**: Automatically generates a unique Cloudflare link (e.g. olstraker...workers.dev) so your friends can watch your live lobby stats in their browser!
- 💻 **Terminal UI**: A beautiful, lightweight, and responsive console-based interface built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
- ⚡ **Asynchronous & Fast**: Fetches data in background threads to ensure the UI never freezes.
- 🔄 **Auto-Updater**: Keeps itself up-to-date with the latest GitHub releases.

### ⚙️ How it Works
1. **Local API Connection**: RolsTraker safely connects to your local Riot Client API by reading the local lockfile generated when Valorant is running.
2. **Session Authentication**: It extracts your current session tokens (AccessToken, EntitlementsToken).
3. **Live Polling**: It queries the glzHost endpoints to detect if you are currently in a Pre-Game (Agent Select) or Core-Game (Live Match).
4. **Data Aggregation**: Once a match is found, it pulls the PUUIDs of all players in the lobby and asynchronously queries the pdHost to fetch their match histories, competitive tiers, and accuracy stats.
5. **Web Broadcasting**: If enabled, it securely pushes the lobby data to a Cloudflare Worker, giving you a shareable link.

### 📥 Get Started
Ready to see your lobby stats in real-time?

[![Download Latest Release](https://img.shields.io/badge/-Download_Latest_Release-00e5ff?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Rolsikkk/RolsTraker/releases/latest)
[![View Source Code](https://img.shields.io/badge/-View_Source_Code-222222?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Rolsikkk/RolsTraker)

#### Installation
1. Download the latest RolsTraker.exe from the [Releases](https://github.com/Rolsikkk/RolsTraker/releases) page.
2. Run Valorant.
3. Run RolsTraker.exe. That's it! 

---

<a name="русский"></a>
## 🇷🇺 Русский

Молниеносный **Valorant трекер статистики**, работающий в терминале и написанный на C++. Он автоматически определяет, когда вы находитесь в матче, показывает ранги и статистику всех игроков в лобби, а также выдает уникальную веб-ссылку, чтобы ваши друзья могли следить за матчем!

### ✨ Возможности
- 🚀 **Отслеживание матчей в реальном времени**: Мгновенно распознает стадию выбора агентов и сам матч.
- 📊 **Глубокая статистика**: Скрытый MMR, текущие ранги, K/D, % попаданий в голову, винрейт и история последних матчей для каждого в лобби.
- 🌐 **Live Web Sharing**: Автоматически генерирует ссылку через Cloudflare (например, olstraker...workers.dev), чтобы друзья могли смотреть твою статистику через браузер!
- 💻 **Терминальный интерфейс**: Красивый, легкий и быстрый консольный интерфейс на базе [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
- ⚡ **Асинхронность и скорость**: Вся статистика подгружается в фоновых потоках, поэтому программа никогда не зависает.
- 🔄 **Авто-обновление**: Программа сама скачивает новые версии с GitHub.

### ⚙️ Как это работает
1. **Подключение к Local API**: RolsTraker безопасно подключается к локальному Riot Client API, считывая lockfile во время работы Valorant.
2. **Аутентификация**: Программа извлекает текущие токены сессии (AccessToken, EntitlementsToken).
3. **Live Polling**: Постоянно опрашивает glzHost, чтобы узнать, находитесь ли вы в меню, на стадии выбора (Pre-Game) или в игре (Core-Game).
4. **Сбор данных**: При нахождении матча собирает PUUID всех игроков и асинхронно запрашивает их историю и статистику через pdHost.
5. **Web Broadcasting**: Если включено, безопасно отправляет данные на Cloudflare Worker для отображения на сайте по ссылке.

### 📥 Как начать
Хочешь увидеть статистику своего лобби?

[![Скачать последнюю версию](https://img.shields.io/badge/-Скачать_последний_релиз-00e5ff?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Rolsikkk/RolsTraker/releases/latest)

#### Установка
1. Скачай последний RolsTraker.exe со страницы [Releases](https://github.com/Rolsikkk/RolsTraker/releases).
2. Запусти Valorant.
3. Запусти RolsTraker.exe. Готово!

---

## 📸 Support the Project / Поддержать проект

If you find this tool helpful, please consider leaving a star! ⭐ / Если программа оказалась полезной, поставьте звёздочку! ⭐

[![Star History Chart](https://api.star-history.com/svg?repos=Rolsikkk/RolsTraker&type=Date)](https://star-history.com/#Rolsikkk/RolsTraker&Date)