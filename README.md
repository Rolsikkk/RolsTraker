# RolsTraker ??

![C++](https://img.shields.io/badge/C++-20-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Version](https://img.shields.io/github/v/release/Rolsikkk/RolsTraker)

A lightning-fast, terminal-based **Valorant stats tracker** built in C++. It automatically detects your live matches, shows ranks and stats for everyone in the lobby, and even gives you a live web link to share with your friends!

---

## ? Features

- ?? **Live Match Tracking**: Instantly detects when you enter an agent select or a live match.
- ?? **Deep Stats**: View hidden MMR, current Ranks, K/D, Headshot %, Win/Loss ratios, and recent match history for everyone in your lobby.
- ?? **Live Web Sharing**: Automatically generates a unique Cloudflare link (e.g. olstraker...workers.dev) so your friends can watch your live lobby stats in their browser!
- ?? **Terminal UI**: A beautiful, lightweight, and responsive console-based interface built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
- ? **Asynchronous & Fast**: Fetches data in background threads to ensure the UI never freezes.
- ?? **Auto-Updater**: Keeps itself up-to-date with the latest GitHub releases.

## ?? How it Works

1. **Local API Connection**: RolsTraker safely connects to your local Riot Client API by reading the local lockfile generated when Valorant is running.
2. **Session Authentication**: It extracts your current session tokens (AccessToken, EntitlementsToken).
3. **Live Polling**: It queries the glzHost endpoints to detect if you are currently in a Pre-Game (Agent Select) or Core-Game (Live Match).
4. **Data Aggregation**: Once a match is found, it pulls the PUUIDs of all players in the lobby and asynchronously queries the pdHost to fetch their match histories, competitive tiers, and accuracy stats.
5. **Web Broadcasting**: If enabled, it securely pushes the lobby data to a Cloudflare Worker, giving you a shareable link.

---

## ?? Get Started

Ready to see your lobby stats in real-time?

[![Download Latest Release](https://img.shields.io/badge/-Download_Latest_Release-00e5ff?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Rolsikkk/RolsTraker/releases/latest)
[![View Source Code](https://img.shields.io/badge/-View_Source_Code-222222?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Rolsikkk/RolsTraker)

### Installation
1. Download the latest RolsTraker.exe from the [Releases](https://github.com/Rolsikkk/RolsTraker/releases) page.
2. Run Valorant.
3. Run RolsTraker.exe. That's it! 

---

## ?? Support the Project

If you find this tool helpful, please consider leaving a star! ?

[![GitHub stars](https://img.shields.io/github/stars/Rolsikkk/RolsTraker.svg?style=social&label=Star)](https://github.com/Rolsikkk/RolsTraker)
