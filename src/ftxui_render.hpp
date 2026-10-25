#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <atomic>

using namespace ftxui;

enum class PlayerProfileTab { SKINS, STATS };
extern PlayerProfileTab g_playerProfileTab;
extern ftxui::Box g_tabSkinsBox;
extern ftxui::Box g_tabStatsBox;

extern AppView g_currentView;
extern std::string g_selectedPuuid;
extern PlayerInfo g_selectedPlayerInfo;
extern std::map<std::string, ftxui::Box> g_playerBoxes;
extern ftxui::Box g_myStatsBox;
extern int g_mouseX;
extern int g_mouseY;
extern std::string g_updateStatus;
extern std::mutex g_updateMutex;
extern std::string g_penaltiesStatus;
extern std::mutex g_penaltiesMutex;
extern ftxui::Box g_dodgeButtonBox;
extern bool g_dodgeConfirm;
extern ftxui::Box g_dodgeConfirmBox;
extern ftxui::Box g_dodgeCancelBox;
extern bool g_mousePressed;
extern std::string g_activeButton;
extern std::chrono::steady_clock::time_point g_lastButtonClickTime;
extern ftxui::Box g_backButtonBox;
extern ftxui::Box g_openTrackerBox;
void openTrackerInBrowser(std::string gName, std::string tLine);
extern ftxui::Box g_pickAgentBtnBox;
extern ftxui::Box g_agentPickerBackBox;
extern std::map<std::string, ftxui::Box> g_agentBoxes;
extern std::string g_pickStatusMsg;
extern bool g_isLockingAgent;
extern std::set<std::string> g_ownedAgents;
extern ftxui::Box g_reportOpenBox;
extern ftxui::Box g_chatOpenBox;

struct PostMatchInfo {
    std::string matchId = "";
    int tierAfter = 0;
    int tierBefore = 0;
    int rrAfter = 0;
    int rrBefore = 0;
    int rrEarned = 0;
    int performanceBonus = 0;
    std::string movement = "";
    int afkPenalty = 0;
};
extern bool g_showPostMatch;
extern PostMatchInfo g_postMatchInfo;
extern std::chrono::steady_clock::time_point g_postMatchStartTime;

// Animated button renderer with idle, hover, pressed, and click animation
inline ftxui::Element renderButton(const std::string& text, const ftxui::Box& box, ftxui::Color baseColor, const std::string& btnId, bool isPressed) {
    bool hovered = box.Contain(g_mouseX, g_mouseY);
    bool pressed = hovered && isPressed;
    auto now = std::chrono::steady_clock::now();
    bool justClicked = (g_activeButton == btnId) && (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastButtonClickTime).count() < 220);

    if (pressed || justClicked) {
        return ftxui::text(" ▶ " + text + " ◀ ") 
            | ftxui::bold 
            | ftxui::color(ftxui::Color::White) 
            | ftxui::bgcolor(baseColor) 
            | ftxui::borderHeavy;
    } else if (hovered) {
        return ftxui::text(" ► " + text + " ◄ ") 
            | ftxui::bold 
            | ftxui::color(baseColor) 
            | ftxui::inverted 
            | ftxui::border;
    } else {
        return ftxui::text(" [ " + text + " ] ") 
            | ftxui::bold 
            | ftxui::color(baseColor) 
            | ftxui::border;
    }
}

// FTXUI colors for parties
inline const std::vector<Color> FTX_PARTY_COLORS = {
    Color::Green,
    Color::Cyan,
    Color::Yellow,
    Color::Magenta,
    Color::Red,
    Color::BlueLight
};

// Map Tier to FTXUI Color
inline Color getRankColorFTX(int tier) {
    if (tier >= 27) return Color::RGB(255, 215, 0);       // Radiant (Yellow/Gold)
    if (tier >= 24) return Color::RGB(220, 20, 60);       // Immortal (Crimson Red)
    if (tier >= 21) return Color::RGB(34, 139, 34);       // Ascendant (Green)
    if (tier >= 18) return Color::RGB(147, 112, 219);     // Diamond (Purple)
    if (tier >= 15) return Color::RGB(0, 191, 255);       // Platinum (Cyan)
    if (tier >= 12) return Color::Yellow;                 // Gold (Yellow)
    if (tier >= 9)  return Color::RGB(192, 192, 192);     // Silver (Silver)
    if (tier >= 6)  return Color::RGB(156, 108, 77);      // Bronze (Brown)
    if (tier >= 3)  return Color::GrayDark;               // Iron (Dark Gray)
    return Color::GrayDark;                               // Unrated
}

struct PartyColorInfo {
    int groupIndex;
    std::string ansiColor;
    std::string name;
    int memberCount = 0;
};

inline std::string utf8_substr(const std::string& str, size_t max_len) {
    size_t len = 0;
    size_t i = 0;
    for (; i < str.length() && len < max_len; ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t char_bytes = 1;
        if (c < 0x80) char_bytes = 1;
        else if ((c & 0xE0) == 0xC0) char_bytes = 2;
        else if ((c & 0xF0) == 0xE0) char_bytes = 3;
        else if ((c & 0xF8) == 0xF0) char_bytes = 4;
        i = std::min(str.length(), i + char_bytes);
        len++;
    }
    return str.substr(0, i);
}

inline std::string getAgentName(const std::string& characterId, const std::map<std::string, std::string>& agentMap) {
    if (characterId.empty()) return "";
    std::string lowerId = characterId;
    for (auto& c : lowerId) c = tolower(c);
    auto it = agentMap.find(lowerId);
    if (it != agentMap.end()) return it->second;
    for (const auto& [k, v] : agentMap) {
        std::string lk = k;
        for (auto& c : lk) c = tolower(c);
        if (lk == lowerId) return v;
    }
    if (characterId.length() == 36) return "?";
    return characterId;
}

inline ftxui::Element buildTeamTable(const std::string& title, ftxui::Color titleColor, const std::vector<PlayerInfo>& team, const std::map<std::string, PartyColorInfo>& partyGroupMap, const std::map<std::string, std::string>& agentMap) {
    std::vector<std::vector<ftxui::Element>> rows;
    
    // Compact Header
    rows.push_back({
        ftxui::text(" # ") | ftxui::bold,
        ftxui::text(" Игрок ") | ftxui::bold,
        ftxui::text(" Агент ") | ftxui::bold,
        ftxui::text(" Ранг ") | ftxui::bold,
        ftxui::text(" K/D ") | ftxui::bold,
        ftxui::text(" W/L ") | ftxui::bold
    });

    for (const auto& p : team) {
        std::string fullName = p.gameName + (p.tagLine.empty() ? "" : "#" + p.tagLine);
        if (utf8_length(fullName) > 15) fullName = utf8_substr(fullName, 13) + "..";

        std::string agent = p.characterId.empty() ? "Пикает..." : getAgentName(p.characterId, agentMap);
        if (utf8_length(agent) > 9) agent = utf8_substr(agent, 7) + "..";

        RankDisplay rankInfo = formatRank(p.rankTier, p.rankRR, p.peakRankTier, p.peakRankRR);
        std::string rankStr = rankInfo.name;
        if (utf8_length(rankStr) > 16) rankStr = utf8_substr(rankStr, 14) + "..";

        std::string kdStr = "...";
        ftxui::Color kdColor = ftxui::Color::White;
        if (!p.isLoading) {
            if (p.isPrivate) {
                kdStr = "Закрыт";
                kdColor = ftxui::Color::YellowLight;
            } else if (p.kdRatio >= 0.0f) {
                std::ostringstream oss; oss << std::fixed << std::setprecision(2) << p.kdRatio; kdStr = oss.str();
                kdColor = (p.kdRatio >= 1.0f) ? ftxui::Color::GreenLight : ftxui::Color::RedLight;
            } else {
                kdStr = "N/A";
                kdColor = ftxui::Color::GrayLight;
            }
        }

        std::string wlStr = "...";
        if (!p.isLoading) {
            if (p.isPrivate) {
                wlStr = "Закрыт";
            } else if (p.matchesPlayed > 0) {
                int losses = std::max(0, p.matchesPlayed - p.matchesWon);
                wlStr = std::to_string(p.matchesWon) + "/" + std::to_string(losses);
            } else if (p.wins >= 0) {
                wlStr = std::to_string(p.wins) + "/" + std::to_string(p.losses);
            } else {
                wlStr = "N/A";
            }
        }

        ftxui::Element partyIcon = ftxui::text("-") | ftxui::color(ftxui::Color::GrayDark);
        ftxui::Color rColor = getRankColorFTX(p.rankTier > 2 ? p.rankTier : p.peakRankTier);

        if (!p.partyId.empty() && partyGroupMap.count(p.partyId)) {
            auto pInfo = partyGroupMap.at(p.partyId);
            ftxui::Color pColor = FTX_PARTY_COLORS[(pInfo.groupIndex - 1) % FTX_PARTY_COLORS.size()];
            std::string pLabel = "";
            if (p.detectedPartySize >= 2) {
                pLabel = p.detectedPartyLabel;
            } else if (pInfo.memberCount >= 2) {
                if (pInfo.memberCount == 2) pLabel = "ДУО";
                else if (pInfo.memberCount == 3) pLabel = "ТРИО";
                else if (pInfo.memberCount == 4) pLabel = "4-СТАК";
                else if (pInfo.memberCount >= 5) pLabel = "5-СТАК";
            }
            std::string fullPartyStr = "#" + std::to_string(pInfo.groupIndex);
            if (!pLabel.empty()) {
                fullPartyStr += " " + pLabel;
            }
            partyIcon = ftxui::text(" " + fullPartyStr + " ") | ftxui::color(pColor) | ftxui::bold;
        }

        ftxui::Element nameElement;
        std::vector<ftxui::Element> nameParts;
        if (!p.isConnected || !p.connectionStatus.empty()) {
            std::string statusStr = !p.connectionStatus.empty() ? p.connectionStatus : "[AFK]";
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
            nameParts.push_back(ftxui::text(statusStr) | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::Red));
        } else if ((p.hasKuronami ? 1 : 0) + (p.hasNocturnum ? 1 : 0) + (p.hasPrimordium ? 1 : 0) >= 2) {
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight) | ftxui::bgcolor(ftxui::Color::RGB(40, 20, 50)));
        } else if (p.hasKuronami) {
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight) | ftxui::bgcolor(ftxui::Color::RGB(15, 45, 65)));
        } else if (p.hasNocturnum) {
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::MagentaLight) | ftxui::bgcolor(ftxui::Color::RGB(55, 10, 45)));
        } else if (p.hasPrimordium) {
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::bgcolor(ftxui::Color::RGB(65, 20, 10)));
        } else {
            if (p.isMuted) {
                nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::color(ftxui::Color::GrayDark));
            } else {
                nameParts.push_back(ftxui::text(" " + fullName + " "));
            }
        }
        if (p.isMuted) {
            nameParts.push_back(ftxui::text("[MUTE] ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        }

        // Kuronami badge
        if (p.hasKuronami) {
            nameParts.push_back(ftxui::text(" [КУРОНАМИ]") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight) | ftxui::bgcolor(ftxui::Color::RGB(10, 30, 45)));
        }

        // Nocturnum / Остриё тьмы badge
        if (p.hasNocturnum) {
            nameParts.push_back(ftxui::text(" [НОКТЮРНУМ]") | ftxui::bold | ftxui::color(ftxui::Color::MagentaLight) | ftxui::bgcolor(ftxui::Color::RGB(45, 10, 35)));
        }

        // Primordium / Первоначало badge
        if (p.hasPrimordium) {
            nameParts.push_back(ftxui::text(" [ПРИМОРДИУМ]") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::bgcolor(ftxui::Color::RGB(55, 15, 10)));
        }

        // Smurf & Booster detection
        bool isSmurfKd = (p.kdRatio >= 1.4f && p.matchesPlayed >= 5 && !p.isPrivate && ((float)p.matchesWon / p.matchesPlayed >= 0.55f));
        if (isSmurfKd) {
            nameParts.push_back(ftxui::text(" [SMURF]") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        }
        if (p.isBooster) {
            nameParts.push_back(ftxui::text(" [БУСТЕР]") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        }

        nameElement = ftxui::hbox(nameParts) | ftxui::reflect(g_playerBoxes[p.puuid]);
        if (g_playerBoxes.count(p.puuid) && g_playerBoxes[p.puuid].Contain(g_mouseX, g_mouseY)) {
            nameElement = nameElement | ftxui::inverted;
        }

        rows.push_back({
            partyIcon,
            nameElement,
            ftxui::text(" " + agent + " "),
            ftxui::text(" " + rankStr + " ") | ftxui::color(rColor),
            ftxui::text(" " + kdStr + " ") | ftxui::color(kdColor),
            ftxui::text(" " + wlStr + " ")
        });
    }

    auto table = ftxui::Table(rows);
    table.SelectAll().Border(ftxui::LIGHT);
    table.SelectRow(0).Decorate(ftxui::bold);
    table.SelectRow(0).BorderBottom(ftxui::DOUBLE);
    table.SelectColumn(0).BorderRight(ftxui::LIGHT);
    
    table.SelectColumn(0).DecorateCells(ftxui::center);
    table.SelectColumn(2).DecorateCells(ftxui::center);
    table.SelectColumn(3).DecorateCells(ftxui::center);
    table.SelectColumn(4).DecorateCells(ftxui::center);
    table.SelectColumn(5).DecorateCells(ftxui::center);

    return ftxui::vbox({
        ftxui::text(title) | ftxui::color(titleColor) | ftxui::bold | ftxui::center,
        table.Render()
    });
}

// ASCII spinner that works on all Windows consoles: - \ | /
inline ftxui::Element circleSpinner(int frame) {
    static const std::vector<std::string> frames = {
        "   -   ",
        "  \\   ",
        "   |   ",
        "   /   "
    };
    int idx = (frame / 2) % (int)frames.size();
    return ftxui::text(frames[idx]);
}

inline ftxui::Element buildUpdateElement(const std::string& updateStatus) {
    std::string updateText = "";
    Color updateColor = Color::GrayDark;
    if (updateStatus == "checking") {
        updateText = " Проверка обновлений... ";
    } else if (updateStatus.size() >= 11 && updateStatus.substr(0, 11) == "downloading") {
        std::string ver = updateStatus.size() > 12 ? updateStatus.substr(12) : "";
        updateText = " Скачивание обновления " + ver + "... ";
        updateColor = Color::YellowLight;
    } else if (updateStatus == "restarting") {
        updateText = " Перезапуск... ";
        updateColor = Color::GreenLight;
    }
    
    return updateText.empty()
        ? ftxui::text("") 
        : (ftxui::text(updateText) | ftxui::color(updateColor) | ftxui::center);
}

inline ftxui::Element renderFTXUI(const MatchState& state, const Session& session, const Lockfile& lock, const std::map<std::string, std::string>& agentMap, const std::map<std::string, std::string>& mapNameMap) {
    std::string updateStatus;
    { std::lock_guard<std::mutex> lk(g_updateMutex); updateStatus = g_updateStatus; }

    if (lock.port == 0) {
        static auto start_time_offline = std::chrono::steady_clock::now();
        int frame = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_offline).count() / 100;

        auto updateElem = buildUpdateElement(updateStatus);

        return ftxui::vbox({
            ftxui::filler(),
            circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
            ftxui::filler(),
            ftxui::text(" Riot Client не запущен! Ожидание запуска игры... ") | ftxui::color(ftxui::Color::RedLight) | ftxui::center,
            updateElem,
            ftxui::filler(),
        });
    }
    
    if (state.phase == "none") {
        static auto start_time_idle = std::chrono::steady_clock::now();
        int frame = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_idle).count() / 100;
        
        auto updateElem = buildUpdateElement(updateStatus);

        auto myStatsBtn = renderButton("Моя статистика (Tracker.gg)", g_myStatsBox, ftxui::Color::CyanLight, "my_stats", g_mousePressed)
                        | ftxui::reflect(g_myStatsBox);

        auto reportBtn = renderButton("⚠ РЕПОРТ ВРАГОВ", g_reportOpenBox, ftxui::Color::RedLight, "report_open", g_mousePressed)
                       | ftxui::reflect(g_reportOpenBox);

        auto chatBtn = renderButton("ЧАТ МАТЧА", g_chatOpenBox, ftxui::Color::GreenLight, "chat_open", g_mousePressed)
                     | ftxui::reflect(g_chatOpenBox);

        return ftxui::vbox({
            ftxui::filler(),
            circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
            ftxui::filler(),
            updateElem,
            ftxui::text(" Ожидание начала матча... ") | ftxui::color(ftxui::Color::GrayDark) | ftxui::center,
            text(""),
            ftxui::hbox({
                myStatsBtn,
                ftxui::text("  "),
                reportBtn,
                ftxui::text("  "),
                chatBtn
            }) | ftxui::center,
            ftxui::text(""),
            ftxui::text(" [F11 / M] Развернуть на весь экран | [Q] Выход ") | ftxui::color(ftxui::Color::GrayDark) | ftxui::center,
            ftxui::filler(),
        });
    }

    // Party counts & Group Assignment
    std::map<std::string, int> partyCounts;
    for (const auto& p : state.players) {
        if (!p.partyId.empty() && p.partyId != "00000000-0000-0000-0000-000000000000") partyCounts[p.partyId]++;
    }

    std::map<std::string, PartyColorInfo> partyGroupMap;
    int nextGroupIdx = 1;
    for (const auto& p : state.players) {
        if (!p.partyId.empty() && p.partyId != "00000000-0000-0000-0000-000000000000" && partyCounts[p.partyId] > 1 && partyGroupMap.find(p.partyId) == partyGroupMap.end()) {
            partyGroupMap[p.partyId] = {nextGroupIdx, "", "", partyCounts[p.partyId]};
            nextGroupIdx++;
        }
    }

    // Header
    std::string displayMapName = mapNameMap.count(state.mapId) ? mapNameMap.at(state.mapId) : state.mapId;
    std::string phaseStr = (state.phase == "coregame") ? "В ИГРЕ (Core Game)" : "ВЫБОР АГЕНТА (Agent Select)";

    auto header = ftxui::hbox({
        ftxui::text(" Режим: ") | ftxui::bold, ftxui::text(phaseStr) | ftxui::color(ftxui::Color::Green) | ftxui::bold,
        ftxui::text(" | Карта: ") | ftxui::bold, ftxui::text(displayMapName.empty() ? "N/A" : displayMapName) | ftxui::color(ftxui::Color::YellowLight) | ftxui::bold,
        ftxui::text(" | Сервер: ") | ftxui::bold, ftxui::text(session.region) | ftxui::color(ftxui::Color::Cyan) | ftxui::bold,
    }) | ftxui::center;

    // Divide into Team 1 and Team 2
    std::vector<PlayerInfo> team1, team2;
    for (const auto& p : state.players) {
        if (p.teamId == "Red" || p.teamId == "Defender") {
            team1.push_back(p);
        } else if (p.teamId == "Blue" || p.teamId == "Attacker") {
            team2.push_back(p);
        } else {
            team1.push_back(p);
        }
    }

    auto termSize = ftxui::Terminal::Size();
    bool sideBySide = (termSize.dimx >= 180 && termSize.dimy < 50);

    ftxui::Element teamsElement;
    if (!team1.empty() && !team2.empty()) {
        auto t1 = buildTeamTable(" [Команда 1 / ВРАГИ (RED)]", ftxui::Color::RedLight, team1, partyGroupMap, agentMap);
        auto t2 = buildTeamTable(" [Команда 2 / ВАШИ (BLUE)]", ftxui::Color::BlueLight, team2, partyGroupMap, agentMap);
        if (sideBySide) {
            teamsElement = ftxui::hbox({
                t1 | ftxui::flex,
                ftxui::separator(),
                t2 | ftxui::flex
            });
        } else {
            teamsElement = ftxui::vbox({
                t1 | ftxui::center,
                ftxui::separator(),
                t2 | ftxui::center
            });
        }
    } else {
        auto singleTable = buildTeamTable((state.phase == "pregame") ? " [ВАША КОМАНДА / ALLIES]" : " [ИГРОКИ]", ftxui::Color::CyanLight, team1.empty() ? team2 : team1, partyGroupMap, agentMap);
        teamsElement = singleTable | ftxui::center;
    }

    int totalLobbyElo = 0;
    int rankedPlayers = 0;
    int localElo = 0;
    for (const auto& p : state.players) {
        if (p.rankTier >= 3) {
            int elo = p.rankTier * 100 + p.rankRR;
            totalLobbyElo += elo;
            rankedPlayers++;
            if (p.puuid == session.puuid) {
                localElo = elo;
            }
        }
    }
    
    ftxui::Element mmrElement = ftxui::text("");
    if (rankedPlayers > 0 && localElo > 0) {
        int avgElo = totalLobbyElo / rankedPlayers;
        std::string diffMsg;
        ftxui::Color diffColor;
        if (avgElo > localElo + 25) {
            diffMsg = "[MMR: Вы слабее лобби (Ожидается поражение, дадут больше RR при победе)]";
            diffColor = ftxui::Color::RedLight;
        } else if (avgElo < localElo - 25) {
            diffMsg = "[MMR: Вы сильнее лобби (Ожидается победа, снимут больше RR при поражении)]";
            diffColor = ftxui::Color::GreenLight;
        } else {
            diffMsg = "[MMR: Лобби сбалансировано]";
            diffColor = ftxui::Color::YellowLight;
        }
        mmrElement = ftxui::text(diffMsg) | ftxui::bold | ftxui::color(diffColor) | ftxui::center;
    }
    
    std::string penaltiesStatus;
    { std::lock_guard<std::mutex> lk(g_penaltiesMutex); penaltiesStatus = g_penaltiesStatus; }

    std::vector<ftxui::Element> uiElements;
    // Compact Top bar
    uiElements.push_back(ftxui::hbox({
        ftxui::text(" ROLSTRAKER ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
        ftxui::text("(" + CURRENT_VERSION + ") ") | ftxui::color(ftxui::Color::GrayDark),
        ftxui::separator(),
        ftxui::text(" "),
        header,
        ftxui::filler() | ftxui::flex,
        ftxui::text(" [F11/M] На весь экран ") | ftxui::color(ftxui::Color::YellowLight),
        ftxui::text(" [Q] Выход ") | ftxui::color(ftxui::Color::GrayLight)
    }));
    uiElements.push_back(ftxui::separator());

    if (state.phase == "pregame" && !displayMapName.empty()) {
        uiElements.push_back(ftxui::hbox({
            ftxui::filler() | ftxui::flex,
            ftxui::text(" ВЫБРАНА КАРТА: ") | ftxui::bold | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(ftxui::Color::Yellow),
            ftxui::text(" " + displayMapName + " ") | ftxui::bold | ftxui::color(ftxui::Color::White) | ftxui::bgcolor(ftxui::Color::RGB(50, 50, 75)),
            ftxui::text(" [ВЫБОР АГЕНТОВ] ") | ftxui::bold | ftxui::color(ftxui::Color::GreenLight),
            ftxui::filler() | ftxui::flex
        }));
        uiElements.push_back(ftxui::separator());
    }

    uiElements.push_back(teamsElement);

    if (rankedPlayers > 0 && localElo > 0) {
        uiElements.push_back(ftxui::separator());
        uiElements.push_back(mmrElement);
    }

    uiElements.push_back(ftxui::separator());
    uiElements.push_back(ftxui::hbox({
        (state.phase == "pregame" ? (
            !g_dodgeConfirm
            ? ftxui::hbox({
                renderButton("УКЛОНИТЬСЯ", g_dodgeButtonBox, ftxui::Color::RedLight, "dodge", g_mousePressed) | ftxui::reflect(g_dodgeButtonBox),
                ftxui::text(" "),
                renderButton("ВЫБРАТЬ АГЕНТА", g_pickAgentBtnBox, ftxui::Color::GreenLight, "pick_agent_open", g_mousePressed) | ftxui::reflect(g_pickAgentBtnBox)
            })
            : ftxui::hbox({
                ftxui::text(" Выйти? ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::center,
                renderButton("ДА", g_dodgeConfirmBox, ftxui::Color::RedLight, "dodge_confirm", g_mousePressed) | ftxui::reflect(g_dodgeConfirmBox),
                ftxui::text(" "),
                renderButton("ОТМЕНА", g_dodgeCancelBox, ftxui::Color::GrayLight, "dodge_cancel", g_mousePressed) | ftxui::reflect(g_dodgeCancelBox)
            })
        ) : ftxui::text("")),
        ftxui::filler() | ftxui::flex,
        renderButton("Мои скины и статистика", g_myStatsBox, ftxui::Color::YellowLight, "my_stats", g_mousePressed) | ftxui::reflect(g_myStatsBox),
        ftxui::text("  "),
        renderButton("ЧАТ МАТЧА", g_chatOpenBox, ftxui::Color::GreenLight, "chat_open", g_mousePressed) | ftxui::reflect(g_chatOpenBox),
        ftxui::text("  "),
        renderButton("⚠ РЕПОРТ ВРАГОВ", g_reportOpenBox, ftxui::Color::RedLight, "report_open", g_mousePressed) | ftxui::reflect(g_reportOpenBox),
        ftxui::filler() | ftxui::flex,
        (!penaltiesStatus.empty() ? ftxui::text(penaltiesStatus) | ftxui::bold | ftxui::color(ftxui::Color::RedLight) : ftxui::text(""))
    }));

    return ftxui::vbox(std::move(uiElements)) | ftxui::border;
}

inline ftxui::Element renderPostMatchOverlay(const PostMatchInfo& info) {
    RankDisplay rankInfo = formatRank(info.tierAfter, info.rrAfter);
    
    struct Rgb { int r, g, b; };
    auto getRankRgb = [](int tier) -> Rgb {
        if (tier >= 27) return {255, 215, 0};    // Radiant (Gold)
        if (tier >= 24) return {220, 20, 60};    // Immortal (Crimson)
        if (tier >= 21) return {34, 180, 75};    // Ascendant (Green)
        if (tier >= 18) return {150, 90, 225};   // Diamond (Purple)
        if (tier >= 15) return {0, 191, 255};    // Platinum (Cyan)
        if (tier >= 12) return {240, 190, 20};   // Gold (Yellow)
        if (tier >= 9)  return {192, 192, 192};  // Silver
        if (tier >= 6)  return {165, 110, 70};   // Bronze
        if (tier >= 3)  return {100, 100, 110};  // Iron
        return {120, 120, 120};                  // Unrated
    };

    Rgb base = getRankRgb(info.tierAfter);
    Color rankCol = Color::RGB(base.r, base.g, base.b);
    Color rankMid = Color::RGB(base.r * 6 / 10, base.g * 6 / 10, base.b * 6 / 10);
    Color rankDark = Color::RGB(base.r * 3 / 10, base.g * 3 / 10, base.b * 3 / 10);
    Color rankDeep = Color::RGB(base.r / 7, base.g / 7, base.b / 7);

    // RR Earned text & color
    std::string rrSign = (info.rrEarned > 0) ? "+" : "";
    std::string rrText = rrSign + std::to_string(info.rrEarned) + " RR";
    Color outcomeColor = (info.rrEarned > 0) ? Color::GreenLight : ((info.rrEarned < 0) ? Color::RedLight : Color::YellowLight);
    std::string outcomeTitle = (info.rrEarned > 0) ? "ПОБЕДА" : ((info.rrEarned < 0) ? "ПОРАЖЕНИЕ" : "НИЧЬЯ");

    // Promotion / Demotion banner
    Element moveElem = text("");
    if (info.tierAfter > info.tierBefore && info.tierBefore > 0) {
        moveElem = text(" ★ ПОВЫШЕНИЕ РАНГА! ★ ") | bold | color(Color::RGB(255, 230, 80)) | bgcolor(Color::RGB(100, 70, 0)) | center;
    } else if (info.tierAfter < info.tierBefore && info.tierAfter > 0) {
        moveElem = text(" ▼ ПОНИЖЕНИЕ РАНГА ▼ ") | bold | color(Color::White) | bgcolor(Color::RGB(120, 20, 20)) | center;
    }

    // RR Progress bar
    int currentRr = std::max(0, std::min(100, info.rrAfter));
    int barWidth = 24;
    int filled = (currentRr * barWidth) / 100;
    std::string barStr = "[";
    for (int i = 0; i < filled; ++i) barStr += "█";
    for (int i = filled; i < barWidth; ++i) barStr += "░";
    barStr += "] " + std::to_string(currentRr) + " / 100 RR";

    // Auto-close countdown
    auto now = std::chrono::steady_clock::now();
    int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_postMatchStartTime).count();
    int remaining = std::max(0, 15 - elapsed);

    // Core Content Card
    auto coreCard = vbox(Elements{
        text(""),
        text("★ " + outcomeTitle + " ★") | bold | color(outcomeColor) | center,
        text(""),
        hbox(Elements{
            filler(),
            text("  " + rrText + "  ") | bold | color(Color::White) | bgcolor(outcomeColor == Color::GreenLight ? Color::RGB(20, 100, 30) : (outcomeColor == Color::RedLight ? Color::RGB(130, 20, 20) : Color::RGB(60, 60, 60))) | borderHeavy,
            filler()
        }),
        moveElem,
        text(""),
        separator(),
        text(""),
        text("ВАШ РАНГ:") | bold | color(rankCol) | center,
        text(" " + rankInfo.name + " ") | bold | color(Color::White) | bgcolor(rankDark) | center,
        text(""),
        text(barStr) | bold | color(rankCol) | center,
        text(""),
        separator(),
        text(""),
        text(" Окно закроется через " + std::to_string(remaining) + " сек  (или нажмите [ESC]) ") | color(Color::GrayLight) | center,
        text("")
    }) 
    | bgcolor(rankDeep) 
    | color(Color::White) 
    | borderStyled(ftxui::DOUBLE, rankCol)
    | size(WIDTH, EQUAL, 64);

    auto glowLayer = vbox(Elements{
        filler(),
        hbox(Elements{ filler(), coreCard, filler() }),
        filler()
    }) | bgcolor(rankDeep) | borderStyled(ftxui::HEAVY, rankMid);

    auto outerTransition = vbox(Elements{
        filler(),
        glowLayer,
        filler()
    }) | bgcolor(Color::RGB(base.r / 15, base.g / 15, base.b / 15)) | borderStyled(ftxui::ROUNDED, rankDark);

    return vbox(Elements{
        filler(),
        hbox(Elements{
            filler(),
            outerTransition | size(WIDTH, GREATER_THAN, 72),
            filler()
        }),
        filler()
    }) | bgcolor(Color::Black);
}
