#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <string>

#include "ftxui_render.hpp"

inline ftxui::Element renderPlayerStats(const std::map<std::string, std::string>& agentMap) {
    using namespace ftxui;

    auto backBtn = text("[ ESC - Назад ]") | bold | color(Color::GrayDark);
    
    std::string fullName = g_selectedPlayerInfo.gameName + "#" + g_selectedPlayerInfo.tagLine;
    
    // Find favorite agent
    std::string favAgent = "Нет данных";
    int maxPlays = -1;
    for (const auto& [agentId, plays] : g_selectedPlayerInfo.agentPlays) {
        if (plays > maxPlays) {
            maxPlays = plays;
            favAgent = agentMap.count(agentId) ? agentMap.at(agentId) : agentId;
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

    auto statsBox = window(text(" Подробная статистика игрока ") | bold | color(Color::Cyan),
        vbox(
            hbox(text(" Игрок: ") | bold, text(fullName) | color(Color::White)),
            separator(),
            hbox(text(" Любимый Агент (за последние матчи): ") | bold, text(favAgent) | color(Color::YellowLight)),
            hbox(text(" K/D (за последние матчи): ") | bold, text(g_selectedPlayerInfo.kdRatio < 0 ? "N/A" : kdStream.str()) | color(Color::GreenLight)),
            separator(),
            text(" Точность стрельбы:") | bold,
            hbox(
                vbox(
                    text("В голову (HS)") | center,
                    text(totalShots > 0 ? hsStream.str() : "N/A") | bold | color(Color::RedLight) | center
                ) | flex | border,
                vbox(
                    text("В тело (BS)") | center,
                    text(totalShots > 0 ? bsStream.str() : "N/A") | bold | color(Color::YellowLight) | center
                ) | flex | border,
                vbox(
                    text("В ноги (LS)") | center,
                    text(totalShots > 0 ? lsStream.str() : "N/A") | bold | color(Color::GrayLight) | center
                ) | flex | border
            )
        )
    );

    return vbox(
        statsBox,
        filler(),
        backBtn | center
    ) | border;
}
