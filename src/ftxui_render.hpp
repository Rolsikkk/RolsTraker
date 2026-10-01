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

using namespace ftxui;

extern AppView g_currentView;
extern std::string g_selectedPuuid;
extern PlayerInfo g_selectedPlayerInfo;
extern std::map<std::string, ftxui::Box> g_playerBoxes;
extern ftxui::Box g_myStatsBox;
extern int g_mouseX;
extern int g_mouseY;
extern std::string g_updateStatus;

// FTXUI colors for parties
const std::vector<Color> FTX_PARTY_COLORS = {
    Color::Green,
    Color::Cyan,
    Color::Yellow,
    Color::Magenta,
    Color::Red,
    Color::BlueLight
};

// Map Tier to FTXUI Color
inline Color getRankColorFTX(int tier) {
    if (tier >= 24) return Color::RedLight;      // Radiant
    if (tier >= 21) return Color::Red;           // Immortal
    if (tier >= 18) return Color::Magenta;       // Ascendant
    if (tier >= 15) return Color::CyanLight;     // Diamond
    if (tier >= 12) return Color::BlueLight;     // Platinum
    if (tier >= 9)  return Color::YellowLight;   // Gold
    if (tier >= 6)  return Color::GrayLight;     // Silver
    if (tier >= 3)  return Color::Orange1;       // Bronze
    return Color::GrayDark;                      // Iron / Unrated
}

struct PartyColorInfo {
    int groupIndex;
    std::string ansiColor;
    std::string name;
};

inline ftxui::Element buildTeamTable(const std::string& title, ftxui::Color titleColor, const std::vector<PlayerInfo>& team, const std::map<std::string, PartyColorInfo>& partyGroupMap, const std::map<std::string, std::string>& agentMap) {
    std::vector<std::vector<ftxui::Element>> rows;
    
    // Header
    rows.push_back({
        ftxui::text(" Пати ") | ftxui::bold,
        ftxui::text(" Игрок (Ник#Тег) ") | ftxui::bold,
        ftxui::text(" Агент ") | ftxui::bold,
        ftxui::text(" Ранг ") | ftxui::bold,
        ftxui::text(" K/D ") | ftxui::bold,
        ftxui::text(" W/L ") | ftxui::bold
    });

    for (const auto& p : team) {
        std::string fullName = p.gameName + (p.tagLine.empty() ? "" : "#" + p.tagLine);
        if (utf8_length(fullName) > 20) fullName = fullName.substr(0, 17) + "...";

        std::string agent = agentMap.count(p.characterId) ? agentMap.at(p.characterId) : (p.characterId.empty() ? "Выбирает..." : "Агент");
        if (utf8_length(agent) > 12) agent = agent.substr(0, 9) + "...";

        RankDisplay rankInfo = formatRank(p.rankTier, p.rankRR);
        std::string rankStr = rankInfo.name;
        if (utf8_length(rankStr) > 20) rankStr = rankStr.substr(0, 17) + "...";

        std::string kdStr = "...";
        if (p.kdRatio == -2.0f) kdStr = "N/A";
        else if (p.kdRatio != -1.0f) {
            std::ostringstream oss; oss << std::fixed << std::setprecision(2) << p.kdRatio; kdStr = oss.str();
        }

        std::string wlStr = "...";
        if (p.wins == -2) wlStr = "N/A";
        else if (p.wins != -1) wlStr = std::to_string(p.wins) + "/" + std::to_string(p.losses);

        ftxui::Element partyIcon = ftxui::text(" [○] ") | ftxui::color(ftxui::Color::GrayDark);
        ftxui::Element partyText = ftxui::text(" Соло ") | ftxui::color(ftxui::Color::GrayDark);
        ftxui::Color rColor = getRankColorFTX(p.rankTier);

        if (!p.partyId.empty() && partyGroupMap.count(p.partyId)) {
            auto pInfo = partyGroupMap.at(p.partyId);
            ftxui::Color pColor = FTX_PARTY_COLORS[(pInfo.groupIndex - 1) % FTX_PARTY_COLORS.size()];
            partyIcon = ftxui::text(" [●] ") | ftxui::color(pColor) | ftxui::bold;
            partyText = ftxui::text(" Пати #" + std::to_string(pInfo.groupIndex) + " ") | ftxui::color(pColor) | ftxui::bold;
        }

        auto nameElement = ftxui::text(" " + fullName + " ") | ftxui::reflect(g_playerBoxes[p.puuid]);
        if (g_playerBoxes.count(p.puuid) && g_playerBoxes[p.puuid].Contain(g_mouseX, g_mouseY)) {
            nameElement = nameElement | ftxui::inverted;
        }

        rows.push_back({
            partyIcon,
            nameElement,
            ftxui::text(" " + agent + " "),
            ftxui::text(" " + rankStr + " ") | ftxui::color(rColor),
            ftxui::text(" " + kdStr + " "),
            ftxui::text(" " + wlStr + " ")
        });
    }

    auto table = ftxui::Table(rows);
    table.SelectAll().Border(ftxui::LIGHT);
    table.SelectRow(0).Decorate(ftxui::bold);
    table.SelectRow(0).BorderBottom(ftxui::DOUBLE);
    table.SelectColumn(0).BorderRight(ftxui::LIGHT);
    
    return ftxui::vbox({
        ftxui::text(title) | ftxui::color(titleColor) | ftxui::bold,
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

inline ftxui::Element renderFTXUI(const MatchState& state, const Session& session, const Lockfile& lock, const std::map<std::string, std::string>& agentMap, const std::map<std::string, std::string>& mapNameMap) {
    if (lock.port == 0) {
        static auto start_time_offline = std::chrono::steady_clock::now();
        int frame = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_offline).count() / 100;

        // Update status text
        std::string updateText = "";
        Color updateColor = Color::GrayDark;
        if (g_updateStatus == "checking") {
            updateText = " Проверка обновлений... ";
        } else if (g_updateStatus.substr(0, 11) == "downloading") {
            std::string ver = g_updateStatus.size() > 12 ? g_updateStatus.substr(12) : "";
            updateText = " Скачивание обновления " + ver + "... ";
            updateColor = Color::YellowLight;
        } else if (g_updateStatus == "restarting") {
            updateText = " Перезапуск... ";
            updateColor = Color::GreenLight;
        }
        
        auto updateElem = updateText.empty()
            ? ftxui::text("") 
            : (ftxui::text(updateText) | ftxui::color(updateColor) | ftxui::center);

        return ftxui::vbox({
            ftxui::filler(),
            circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
            ftxui::filler(),
            ftxui::text(" Riot Client не запущен! Ожидание запуска игры... ") | ftxui::color(ftxui::Color::RedLight) | ftxui::center,
            updateElem
        });
    } 
    
    if (state.phase == "none") {
        static auto start_time_idle = std::chrono::steady_clock::now();
        int frame = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_idle).count() / 100;
        
        // Update status text
        std::string updateText = "";
        Color updateColor = Color::GrayDark;
        if (g_updateStatus == "checking") {
            updateText = " Проверка обновлений... ";
        } else if (g_updateStatus.size() > 11 && g_updateStatus.substr(0, 11) == "downloading") {
            std::string ver = g_updateStatus.size() > 12 ? g_updateStatus.substr(12) : "";
            updateText = " Скачивание обновления " + ver + "... ";
            updateColor = Color::YellowLight;
        } else if (g_updateStatus == "restarting") {
            updateText = " Перезапуск... ";
            updateColor = Color::GreenLight;
        }
        
        auto updateElem = updateText.empty()
            ? ftxui::text("") 
            : (ftxui::text(updateText) | ftxui::color(updateColor) | ftxui::center);

        auto myStatsBtn = ftxui::text(" пока что вы можете увидеть свою статистику ") 
                        | ftxui::bold | ftxui::color(ftxui::Color::CyanLight)
                        | ftxui::reflect(g_myStatsBox);
                        
        if (g_myStatsBox.Contain(g_mouseX, g_mouseY)) {
            myStatsBtn = myStatsBtn | ftxui::inverted;
        }

        return ftxui::vbox({
            ftxui::filler(),
            circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
            ftxui::filler(),
            updateElem,
            ftxui::text(" зайдите в матч чтобы увидеть информацию ") | ftxui::color(ftxui::Color::GrayDark) | ftxui::center,
            myStatsBtn | ftxui::center
        });
    }

    // Party counts & Group Assignment
    std::map<std::string, int> partyCounts;
    for (const auto& p : state.players) {
        if (!p.partyId.empty()) partyCounts[p.partyId]++;
    }

    std::map<std::string, PartyColorInfo> partyGroupMap;
    int nextGroupIdx = 1;
    for (const auto& p : state.players) {
        if (!p.partyId.empty() && partyCounts[p.partyId] > 1 && partyGroupMap.find(p.partyId) == partyGroupMap.end()) {
            partyGroupMap[p.partyId] = {nextGroupIdx, "", ""}; // Name/ANSI not used here
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
            if (team1.size() < 5) team1.push_back(p);
            else team2.push_back(p);
        }
    }

    auto t1 = buildTeamTable(" [КОМАНДА 1 / ЗАЩИТНИКИ (RED)]", ftxui::Color::RedLight, team1, partyGroupMap, agentMap);
    auto t2 = buildTeamTable(" [КОМАНДА 2 / АТАКУЮЩИЕ (BLUE)]", ftxui::Color::BlueLight, team2, partyGroupMap, agentMap);

    return ftxui::vbox({
        ftxui::text("ROLSTRAKER (" + CURRENT_VERSION + ")") | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
        ftxui::separator(),
        header,
        ftxui::separator(),
        ftxui::hbox({
            t1 | ftxui::flex,
            ftxui::separator(),
            t2 | ftxui::flex
        }),
        ftxui::separator(),
        ftxui::hbox({
            ftxui::filler() | ftxui::flex,
            (g_myStatsBox.Contain(g_mouseX, g_mouseY) 
                ? ftxui::text(" [Моя статистика] ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight) | ftxui::border | ftxui::inverted | ftxui::reflect(g_myStatsBox)
                : ftxui::text(" [Моя статистика] ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight) | ftxui::border | ftxui::reflect(g_myStatsBox))
        })
    }) | ftxui::border;
}
