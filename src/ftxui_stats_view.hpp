#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>

#include "ftxui_render.hpp"

inline std::string padCenter(const std::string& str, size_t width) {
    if (str.length() >= width) return str;
    size_t left = (width - str.length()) / 2;
    size_t right = width - str.length() - left;
    return std::string(left, ' ') + str + std::string(right, ' ');
}

inline ftxui::Element renderPlayerStats(const std::map<std::string, std::string>& agentMap) {
    using namespace ftxui;

    auto backBtn = text("[ ESC - Назад ]") | bold | color(Color::GrayDark);
    
    std::string fullName = g_selectedPlayerInfo.gameName + "#" + g_selectedPlayerInfo.tagLine;
    
    // Find favorite agent securely with case-insensitivity
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
        hbox(text("         _.-'^^'-._         ") | color(Color::GrayDark)),
        hbox(text("        /          \\        ") | color(Color::GrayDark)),
        hbox(text("       |   "), text(padCenter(totalShots > 0 ? hsStream.str() : "N/A", 6)) | color(Color::RedLight) | bold, text("   |        ") | color(Color::GrayDark)),
        hbox(text("        \\          /        ") | color(Color::GrayDark)),
        hbox(text("         '-.,__,.-'         ") | color(Color::GrayDark)),
        hbox(text("           |    |           ") | color(Color::GrayDark)),
        hbox(text("         .-'    '-.         ") | color(Color::GrayDark)),
        hbox(text("        /          \\        ") | color(Color::GrayDark)),
        hbox(text("       |   "), text(padCenter(totalShots > 0 ? bsStream.str() : "N/A", 6)) | color(Color::YellowLight) | bold, text("   |        ") | color(Color::GrayDark)),
        hbox(text("       |            |       ") | color(Color::GrayDark)),
        hbox(text("       |            |       ") | color(Color::GrayDark)),
        hbox(text("        \\          /        ") | color(Color::GrayDark)),
        hbox(text("         '-.,,,.-'          ") | color(Color::GrayDark)),
        hbox(text("           |    |           ") | color(Color::GrayDark)),
        hbox(text("           |    |           ") | color(Color::GrayDark)),
        hbox(text("          /      \\          ") | color(Color::GrayDark)),
        hbox(text("         |  "), text(padCenter(totalShots > 0 ? lsStream.str() : "N/A", 6)) | color(Color::GrayLight) | bold, text("  |         ") | color(Color::GrayDark)),
        hbox(text("         '--'  '--'         ") | color(Color::GrayDark))
    }) | center;

    std::vector<ftxui::Element> matchElems;
    if (g_selectedPlayerInfo.recentMatches.empty()) {
        matchElems.push_back(text(" Нет данных о последних матчах (сыграйте еще или подождите)") | color(Color::GrayDark) | center);
    } else {
        for (const auto& rm : g_selectedPlayerInfo.recentMatches) {
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

            auto row = hbox({
                text(" " + agent + " ") | bold | color(Color::CyanLight) | size(WIDTH, EQUAL, 12),
                separator(),
                text(" " + resultStr + " (" + scoreStr + ") ") | bold | color(resultColor) | size(WIDTH, EQUAL, 20),
                separator(),
                text(" K/D/A: " + kda + " ") | color(Color::White) | size(WIDTH, EQUAL, 20),
                separator(),
                text(" СЧЁТ: " + combatScore + " ") | color(Color::GrayLight)
            }) | border;
            matchElems.push_back(row);
        }
    }
    
    auto matchHistoryBox = window(text(" Последние матчи ") | bold | color(Color::White), vbox(matchElems));

    auto statsBox = window(text(" Подробная статистика игрока ") | bold | color(Color::Cyan),
        vbox(
            hbox(text(" Игрок: ") | bold, text(fullName) | color(Color::White)),
            separator(),
            hbox(text(" Любимый Агент (за последние матчи): ") | bold, text(favAgent) | color(Color::YellowLight)),
            hbox(text(" K/D (за последние матчи): ") | bold, text(g_selectedPlayerInfo.kdRatio < 0 ? "N/A" : kdStream.str()) | color(Color::GreenLight)),
            separator(),
            hbox(
                vbox(text(" Точность стрельбы:") | bold, bodyArt) | flex,
                separator(),
                matchHistoryBox | flex
            )
        )
    );

    return vbox(
        statsBox,
        filler(),
        backBtn | center
    ) | border;
}
