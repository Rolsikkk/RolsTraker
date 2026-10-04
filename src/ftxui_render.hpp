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

extern AppView g_currentView;
extern std::string g_selectedPuuid;
extern PlayerInfo g_selectedPlayerInfo;
extern std::map<std::string, ftxui::Box> g_playerBoxes;
extern ftxui::Box g_myStatsBox;
extern ftxui::Box g_scrollbarBox;
extern int g_mouseX;
extern int g_mouseY;
extern std::string g_updateStatus;
extern std::mutex g_updateMutex;
extern std::string g_penaltiesStatus;
extern std::mutex g_penaltiesMutex;
extern ftxui::Box g_dodgeButtonBox;

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
};

inline std::string utf8_substr(const std::string& str, size_t max_len) {
    size_t len = 0;
    size_t i = 0;
    for (; i < str.length() && len < max_len; ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        len++;
    }
    return str.substr(0, i);
}

inline std::string getAgentName(const std::string& characterId, const std::map<std::string, std::string>& agentMap) {
    std::string agent = characterId;
    std::string lowerId = characterId;
    for (auto& c : lowerId) c = tolower(c);
    for (const auto& [k, v] : agentMap) {
        std::string lk = k;
        for (auto& c : lk) c = tolower(c);
        if (lk == lowerId) { agent = v; break; }
    }
    return agent;
}

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
        if (utf8_length(fullName) > 20) fullName = utf8_substr(fullName, 17) + "...";

        std::string agent = p.characterId.empty() ? "Выбирает..." : getAgentName(p.characterId, agentMap);
        if (utf8_length(agent) > 12) agent = utf8_substr(agent, 9) + "...";

        RankDisplay rankInfo = formatRank(p.rankTier, p.rankRR);
        std::string rankStr = rankInfo.name;
        if (utf8_length(rankStr) > 20) rankStr = utf8_substr(rankStr, 17) + "...";

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
    g_playerBoxes.clear();

    // Capture update status once under the mutex to avoid races
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
            myStatsBtn | ftxui::center,
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
            team1.push_back(p);
        }
    }

    ftxui::Element teamsElement;
    if (!team1.empty() && !team2.empty()) {
        auto t1 = buildTeamTable(" [КОМАНДА 1 / ЗАЩИТНИКИ (RED)]", ftxui::Color::RedLight, team1, partyGroupMap, agentMap);
        auto t2 = buildTeamTable(" [КОМАНДА 2 / АТАКУЮЩИЕ (BLUE)]", ftxui::Color::BlueLight, team2, partyGroupMap, agentMap);
        teamsElement = ftxui::hbox({
            t1 | ftxui::flex,
            ftxui::separator(),
            t2 | ftxui::flex
        });
    } else {
        auto singleTable = buildTeamTable((state.phase == "pregame") ? " [ВАША КОМАНДА / ALLIES]" : " [ИГРОКИ]", ftxui::Color::CyanLight, team1.empty() ? team2 : team1, partyGroupMap, agentMap);
        teamsElement = ftxui::hbox({
            ftxui::filler() | ftxui::flex,
            singleTable,
            ftxui::filler() | ftxui::flex
        });
    }
    std::string penaltiesStatus;
    { std::lock_guard<std::mutex> lk(g_penaltiesMutex); penaltiesStatus = g_penaltiesStatus; }

    return ftxui::vbox({
        ftxui::text("ROLSTRAKER (" + CURRENT_VERSION + ")") | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
        ftxui::separator(),
        header,
        ftxui::separator(),
        teamsElement,
        ftxui::separator(),
        ftxui::hbox({
            (state.phase == "pregame" ? (
                g_dodgeButtonBox.Contain(g_mouseX, g_mouseY)
                ? ftxui::text(" [УКЛОНИТЬСЯ (DODGE)] ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::border | ftxui::inverted | ftxui::reflect(g_dodgeButtonBox)
                : ftxui::text(" [УКЛОНИТЬСЯ (DODGE)] ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::border | ftxui::reflect(g_dodgeButtonBox)
            ) : ftxui::text("")),
            ftxui::filler() | ftxui::flex,
            (g_myStatsBox.Contain(g_mouseX, g_mouseY) 
                ? ftxui::text(" [Моя статистика] ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight) | ftxui::border | ftxui::inverted | ftxui::reflect(g_myStatsBox)
                : ftxui::text(" [Моя статистика] ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight) | ftxui::border | ftxui::reflect(g_myStatsBox)),
            ftxui::filler() | ftxui::flex
        }),
        ftxui::filler() | ftxui::flex,
        (!penaltiesStatus.empty() ? ftxui::text(penaltiesStatus) | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::center : ftxui::text(""))
    }) | ftxui::border;
}
