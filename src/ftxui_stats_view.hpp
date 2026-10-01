#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "ftxui_render.hpp"

inline ftxui::Element renderMatchScoreboard(const std::map<std::string, std::string>& agentMap) {
    using namespace ftxui;
    
    if (g_selectedMatchId.empty() || g_matchScoreboards.find(g_selectedMatchId) == g_matchScoreboards.end()) {
        return vbox(text("Загрузка данных матча...") | center, text("[ ESC - Назад ]") | center) | border;
    }

    auto& sb = g_matchScoreboards[g_selectedMatchId];
    std::string scoreStr = std::to_string(sb.roundsBlue) + " : " + std::to_string(sb.roundsRed);
    
    std::vector<Element> rows;
    rows.push_back(hbox({
        text(" Агент ") | bold | size(WIDTH, EQUAL, 12), separator(),
        text(" Игрок ") | bold | size(WIDTH, EQUAL, 20), separator(),
        text(" Команда ") | bold | size(WIDTH, EQUAL, 10), separator(),
        text(" K/D/A ") | bold | size(WIDTH, EQUAL, 15), separator(),
        text(" Счёт ") | bold | size(WIDTH, EQUAL, 8)
    }) | color(Color::CyanLight));
    rows.push_back(separator());

    // Sort by score
    std::vector<PlayerMatchStat> pms = sb.players;
    std::sort(pms.begin(), pms.end(), [](const PlayerMatchStat& a, const PlayerMatchStat& b) {
        return a.score > b.score;
    });

    for (const auto& p : pms) {
        std::string agentName = p.characterId;
        std::string lowerId = p.characterId;
        for(auto& c : lowerId) c = tolower(c);
        for(const auto& [k, v] : agentMap) {
            std::string lk = k;
            for(auto& c : lk) c = tolower(c);
            if (lk == lowerId) { agentName = v; break; }
        }

        std::string playerName = "Player";
        if (g_nameCache.count(p.puuid)) {
            playerName = g_nameCache[p.puuid].first + "#" + g_nameCache[p.puuid].second;
        }

        Color teamColor = (p.teamId == "Blue") ? Color::BlueLight : (p.teamId == "Red" ? Color::RedLight : Color::White);
        
        std::string kda = std::to_string(p.kills) + "/" + std::to_string(p.deaths) + "/" + std::to_string(p.assists);

        rows.push_back(hbox({
            text(" " + agentName + " ") | size(WIDTH, EQUAL, 12), separator(),
            text(" " + playerName + " ") | size(WIDTH, EQUAL, 20), separator(),
            text(" " + p.teamId + " ") | color(teamColor) | size(WIDTH, EQUAL, 10), separator(),
            text(" " + kda + " ") | size(WIDTH, EQUAL, 15), separator(),
            text(" " + std::to_string(p.score) + " ") | size(WIDTH, EQUAL, 8)
        }));
    }

    return vbox(
        text(" Таблица Матча: " + scoreStr) | bold | center,
        separator(),
        vbox(rows) | border,
        filler(),
        text("[ ESC - Назад ]") | center
    ) | border;
}

inline ftxui::Element renderPlayerStats(const std::map<std::string, std::string>& agentMap) {
    using namespace ftxui;

    auto backBtn = text("[ ESC - Назад ]") | bold | color(Color::GrayDark);
    
    std::string fullName = g_selectedPlayerInfo.gameName + "#" + g_selectedPlayerInfo.tagLine;
    
    std::string favAgent = "Нет данных";
    int maxPlays = -1;
    for (const auto& [agentId, plays] : g_selectedPlayerInfo.agentPlays) {
        if (plays > maxPlays) {
            maxPlays = plays;
            std::string lowerId = agentId;
            for(auto& c : lowerId) c = tolower(c);
            
            favAgent = agentId;
            for(const auto& [k, v] : agentMap) {
                std::string lk = k;
                for(auto& c : lk) c = tolower(c);
                if (lk == lowerId) {
                    favAgent = v;
                    break;
                }
            }
        }
    }
    
    int totalShots = g_selectedPlayerInfo.headshots + g_selectedPlayerInfo.bodyshots + g_selectedPlayerInfo.legshots;
    float hsPct = totalShots > 0 ? (float)g_selectedPlayerInfo.headshots / totalShots * 100.0f : 0.0f;
    float bsPct = totalShots > 0 ? (float)g_selectedPlayerInfo.bodyshots / totalShots * 100.0f : 0.0f;
    float lsPct = totalShots > 0 ? (float)g_selectedPlayerInfo.legshots / totalShots * 100.0f : 0.0f;

    std::ostringstream hsStream, bsStream, lsStream, kdStream;
    hsStream << std::fixed << std::setprecision(1) << hsPct << "%";
    bsStream << std::fixed << std::setprecision(1) << bsPct << "%";
    lsStream << std::fixed << std::setprecision(1) << lsPct << "%";
    kdStream << std::fixed << std::setprecision(2) << g_selectedPlayerInfo.kdRatio;

    auto bodyArt = vbox({
        text("В голову (HS): " + (totalShots > 0 ? hsStream.str() : "N/A")) | color(Color::RedLight) | bold,
        text("В тело (BS):   " + (totalShots > 0 ? bsStream.str() : "N/A")) | color(Color::YellowLight) | bold,
        text("В ноги (LS):   " + (totalShots > 0 ? lsStream.str() : "N/A")) | color(Color::GrayLight) | bold,
    });

    std::vector<ftxui::Element> matchElems;
    if (g_selectedPlayerInfo.recentMatches.empty()) {
        matchElems.push_back(text(" Нет данных о матчах") | color(Color::GrayDark));
    } else {
        int maxVisible = 6;
        int totalMatches = g_selectedPlayerInfo.recentMatches.size();
        
        // Ensure offset is valid
        if (g_statsMatchOffset > std::max(0, totalMatches - maxVisible)) {
            g_statsMatchOffset = std::max(0, totalMatches - maxVisible);
        }

        g_matchBoxes.clear();
        g_matchBoxes.resize(std::min(maxVisible, totalMatches - g_statsMatchOffset));

        for (int i = 0; i < g_matchBoxes.size(); ++i) {
            int matchIdx = g_statsMatchOffset + i;
            const auto& rm = g_selectedPlayerInfo.recentMatches[matchIdx];

            std::string agent = rm.characterId;
            std::string lowerId = rm.characterId;
            for(auto& c : lowerId) c = tolower(c);
            for(const auto& [k, v] : agentMap) {
                std::string lk = k;
                for(auto& c : lk) c = tolower(c);
                if (lk == lowerId) { agent = v; break; }
            }
            if (agent.empty()) agent = "Unknown";
            
            ftxui::Color resultColor = rm.won ? Color::GreenLight : Color::RedLight;
            std::string resultStr = rm.won ? "ПОБЕДА" : "ПОРАЖЕНИЕ";
            std::string scoreStr = std::to_string(rm.roundsWon) + " - " + std::to_string(rm.roundsLost);
            
            std::string kda = std::to_string(rm.kills) + " / " + std::to_string(rm.deaths) + " / " + std::to_string(rm.assists);
            std::string combatScore = std::to_string(rm.score);

            std::string q = rm.queueId;
            if (q == "competitive") q = "РЕЙТИНГ";
            else if (q == "unrated") q = "БЕЗ РАНГА";
            else if (q == "deathmatch") q = "ДМ";
            else if (q == "ggteam") q = "ЭСКАЛАЦИЯ";
            else if (q == "swiftplay") q = "БЫСТРАЯ";
            else if (q == "spikerush") q = "SPIKE RUSH";
            else if (q == "snowball") q = "СНЕЖКИ";
            else if (q == "custom") q = "СВОЯ ИГРА";
            else if (q.empty()) q = "НЕИЗВЕСТНО";
            else {
                for (auto& c : q) c = toupper(c);
            }

            auto row = hbox({
                text(" " + agent + " ") | bold | color(Color::CyanLight) | size(WIDTH, EQUAL, 12),
                separator(),
                text(" " + q + " ") | bold | color(Color::YellowLight) | size(WIDTH, EQUAL, 12),
                separator(),
                text(" " + resultStr + " (" + scoreStr + ") ") | bold | color(resultColor) | size(WIDTH, EQUAL, 20),
                separator(),
                text(" K/D/A: " + kda + " ") | color(Color::White) | size(WIDTH, EQUAL, 18),
                separator(),
                text(" СЧЁТ: " + combatScore + " ") | color(Color::GrayLight)
            }) | border | reflect(g_matchBoxes[i]);

            if (g_matchBoxes[i].Contain(g_mouseX, g_mouseY)) {
                row = row | inverted;
            }

            matchElems.push_back(row);
        }

        if (totalMatches > maxVisible) {
            std::string scrollInfo = " Показаны матчи " + std::to_string(g_statsMatchOffset + 1) + 
                                     "-" + std::to_string(g_statsMatchOffset + g_matchBoxes.size()) + 
                                     " из " + std::to_string(totalMatches) + " (Крути колесико или стрелки)";
            matchElems.push_back(text(scrollInfo) | color(Color::GrayDark) | center);
        }
    }
    
    std::string loadingStr = "Загрузка...";
    std::string kdStr = g_selectedPlayerInfo.isLoading ? loadingStr : (g_selectedPlayerInfo.kdRatio < 0 ? "N/A" : kdStream.str());
    
    float winPct = g_selectedPlayerInfo.matchesPlayed > 0 ? ((float)g_selectedPlayerInfo.matchesWon / g_selectedPlayerInfo.matchesPlayed) * 100.0f : 0.0f;
    std::ostringstream winStream; winStream << std::fixed << std::setprecision(1) << winPct << "%";
    std::string winStr = g_selectedPlayerInfo.isLoading ? loadingStr : (g_selectedPlayerInfo.matchesPlayed > 0 ? winStream.str() : "N/A");

    int acs = g_selectedPlayerInfo.totalRounds > 0 ? g_selectedPlayerInfo.totalScore / g_selectedPlayerInfo.totalRounds : 0;
    std::string acsStr = g_selectedPlayerInfo.isLoading ? loadingStr : (g_selectedPlayerInfo.totalRounds > 0 ? std::to_string(acs) : "N/A");

    auto matchHistoryBox = window(text(" Последние рейтинговые матчи (Нажми для таблицы) ") | bold | color(Color::White), vbox(matchElems));

    auto statsBox = window(text(" Подробная статистика (Только Рейтинг) ") | bold | color(Color::Cyan),
        vbox(
            hbox(text(" Игрок: ") | bold, text(fullName) | color(Color::White)),
            separator(),
            hbox(
                vbox(
                    hbox(text(" Любимый Агент: ") | bold, text(favAgent) | color(Color::YellowLight)),
                    hbox(text(" K/D: ") | bold, text(kdStr) | color(Color::GreenLight)),
                    hbox(text(" Win %: ") | bold, text(winStr) | color(Color::MagentaLight)),
                    hbox(text(" ACS: ") | bold, text(acsStr) | color(Color::RedLight))
                ),
                separator(),
                vbox(text(" Точность стрельбы:") | bold, bodyArt),
                separator(),
                vbox(
                    text(" Ранги:") | bold,
                    hbox(text("Текущий: ") | color(Color::GrayLight), text(formatRank(g_selectedPlayerInfo.rankTier, g_selectedPlayerInfo.rankRR).name) | color(getRankColorFTX(g_selectedPlayerInfo.rankTier))),
                    hbox(text("Макс:    ") | color(Color::GrayLight), text(formatRank(g_selectedPlayerInfo.peakRankTier, 0).name) | color(getRankColorFTX(g_selectedPlayerInfo.peakRankTier)))
                )
            ),
            separator(),
            matchHistoryBox
        )
    );

    return vbox(
        statsBox,
        filler(),
        backBtn | center
    ) | border;
}
