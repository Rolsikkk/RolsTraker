#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "ftxui_render.hpp"
#include "ftxui_skins_view.hpp"

inline ftxui::Element renderPlayerStats(const std::map<std::string, std::string>& agentMap, const std::map<std::string, std::string>& mapNameMap) {
    using namespace ftxui;

    auto backBtn = renderButton("ESC - Назад", g_backButtonBox, Color::CyanLight, "back", g_mousePressed) | reflect(g_backButtonBox);
    
    std::string fullName = g_selectedPlayerInfo.gameName + (g_selectedPlayerInfo.tagLine.empty() ? "" : "#" + g_selectedPlayerInfo.tagLine);
    ftxui::Element nameTitle = text(fullName) | bold | color(Color::White);
    if (g_selectedPlayerInfo.isMuted) {
        nameTitle = hbox(nameTitle, text(" [ В МУТЕ ]") | color(Color::RedLight) | bold);
    }
    
    std::string pStatus;
    { std::lock_guard<std::mutex> lk(g_penaltiesMutex); pStatus = g_penaltiesStatus; }
    if (g_selectedPuuid == g_localPuuid && !pStatus.empty()) {
        nameTitle = vbox(nameTitle, text(pStatus) | color(Color::RedLight) | bold);
    }

    std::string favAgent = g_selectedPlayerInfo.favAgent.empty() ? "Нет данных" : g_selectedPlayerInfo.favAgent;
    
    // KD & Accuracy
    std::string kdStr = "Загрузка...";
    if (!g_selectedPlayerInfo.isLoading) {
        if (g_selectedPlayerInfo.isPrivate) kdStr = "Профиль закрыт";
        else if (g_selectedPlayerInfo.kdRatio >= 0.0f) {
            std::ostringstream oss; oss << std::fixed << std::setprecision(2) << g_selectedPlayerInfo.kdRatio;
            kdStr = oss.str();
        } else kdStr = "N/A";
    }

    float hsPct = g_selectedPlayerInfo.hsPercent;
    int totalShots = g_selectedPlayerInfo.headshots + g_selectedPlayerInfo.bodyshots + g_selectedPlayerInfo.legshots;
    float bsPct = 0.0f, lsPct = 0.0f;
    if (totalShots > 0) {
        if (hsPct == 0.0f && g_selectedPlayerInfo.headshots > 0) {
            hsPct = (float)g_selectedPlayerInfo.headshots / totalShots * 100.0f;
        }
        bsPct = (float)g_selectedPlayerInfo.bodyshots / totalShots * 100.0f;
        lsPct = (float)g_selectedPlayerInfo.legshots / totalShots * 100.0f;
    }

    std::ostringstream hsStream, bsStream, lsStream;
    hsStream << std::fixed << std::setprecision(1) << hsPct << "%";
    bsStream << std::fixed << std::setprecision(1) << bsPct << "%";
    lsStream << std::fixed << std::setprecision(1) << lsPct << "%";

    auto bodyArt = g_selectedPlayerInfo.isPrivate
        ? vbox({
            text(""),
            text("  Профиль закрыт игроком  ") | color(Color::YellowLight) | bold | center,
            text("  на сайте Tracker.gg     ") | color(Color::GrayLight) | center,
            text("")
        })
        : vbox({
            hbox(text("  В голову (HS):  ") | color(Color::GrayLight), text(hsPct > 0 ? hsStream.str() : "N/A") | color(Color::RedLight) | bold, text(g_selectedPlayerInfo.headshots > 0 ? " (" + std::to_string(g_selectedPlayerInfo.headshots) + ")" : "") | color(Color::GrayDark)),
            hbox(text("  В тело (BS):    ") | color(Color::GrayLight), text(bsPct > 0 ? bsStream.str() : "N/A") | color(Color::YellowLight) | bold, text(g_selectedPlayerInfo.bodyshots > 0 ? " (" + std::to_string(g_selectedPlayerInfo.bodyshots) + ")" : "") | color(Color::GrayDark)),
            hbox(text("  В ноги (LS):    ") | color(Color::GrayLight), text(lsPct > 0 ? lsStream.str() : "N/A") | color(Color::BlueLight) | bold, text(g_selectedPlayerInfo.legshots > 0 ? " (" + std::to_string(g_selectedPlayerInfo.legshots) + ")" : "") | color(Color::GrayDark)),
        });

    float winPct = 0.0f;
    std::string winStr = "N/A";
    if (g_selectedPlayerInfo.isPrivate) {
        winStr = "Профиль закрыт";
    } else if (g_selectedPlayerInfo.matchesPlayed > 0) {
        winPct = ((float)g_selectedPlayerInfo.matchesWon / g_selectedPlayerInfo.matchesPlayed) * 100.0f;
        std::ostringstream winStream; winStream << std::fixed << std::setprecision(1) << winPct << "%";
        winStr = winStream.str() + " (" + std::to_string(g_selectedPlayerInfo.matchesWon) + "W / " + std::to_string(std::max(0, g_selectedPlayerInfo.matchesPlayed - g_selectedPlayerInfo.matchesWon)) + "L)";
    } else if (g_selectedPlayerInfo.wins >= 0) {
        winStr = std::to_string(g_selectedPlayerInfo.wins) + "W / " + std::to_string(g_selectedPlayerInfo.losses) + "L";
    }

    std::string acsStr = g_selectedPlayerInfo.isPrivate ? "Профиль закрыт" : (g_selectedPlayerInfo.acs > 0 ? std::to_string(g_selectedPlayerInfo.acs) : "N/A");

    Color kdColor = g_selectedPlayerInfo.isPrivate ? Color::YellowLight : (g_selectedPlayerInfo.kdRatio >= 1.0f ? Color::GreenLight : (g_selectedPlayerInfo.kdRatio >= 0.0f ? Color::RedLight : Color::GrayLight));

    std::string badgeText = " [ ДАННЫЕ: VALORANT TRACKER ] ";
    Color badgeColor = Color::CyanLight;
    if (g_selectedPlayerInfo.isPrivate) {
        badgeText = " [ ПРОФИЛЬ ЗАКРЫТ НА TRACKER.GG ] ";
        badgeColor = Color::YellowLight;
    } else if (g_selectedPlayerInfo.statsSource == "Riot Games") {
        badgeText = " [ ДАННЫЕ: RIOT MATCH HISTORY ] ";
        badgeColor = Color::GreenLight;
    }
    auto trackerBadge = text(badgeText) | bold | color(badgeColor) | border;

    RankDisplay curRank = formatRank(g_selectedPlayerInfo.rankTier, g_selectedPlayerInfo.rankRR, g_selectedPlayerInfo.peakRankTier, g_selectedPlayerInfo.peakRankRR);
    RankDisplay pkRank = formatRank(g_selectedPlayerInfo.peakRankTier, g_selectedPlayerInfo.peakRankRR);

    Color curRankCol = getRankColorFTX(g_selectedPlayerInfo.rankTier > 2 ? g_selectedPlayerInfo.rankTier : g_selectedPlayerInfo.peakRankTier);
    Color pkRankCol = getRankColorFTX(g_selectedPlayerInfo.peakRankTier);

    Elements headerItems = {
        hbox({
            text(" Игрок: ") | bold | color(Color::GrayLight),
            nameTitle,
            filler(),
            trackerBadge
        })
    };

    if (g_selectedPlayerInfo.isBooster) {
        std::string bText = " [ ⚡ ВНИМАНИЕ: ВОЗМОЖНЫЙ БУСТЕР — ";
        if (g_selectedPlayerInfo.customSkinCount >= 0) {
            bText += "ВСЕГО " + std::to_string(g_selectedPlayerInfo.customSkinCount) + " ИЗ 19 СКИНОВ В ИНВЕНТАРЕ";
        } else {
            bText += "МАЛО СКИНОВ В ИНВЕНТАРЕ";
        }
        bText += " ] ";
        headerItems.push_back(text(bText) | bold | color(Color::RedLight) | bgcolor(Color::RGB(70, 15, 15)) | center);
    }

    if (g_selectedPlayerInfo.detectedPartySize >= 2) {
        std::string pText = " [ 👥 В ПАЧКЕ: " + g_selectedPlayerInfo.detectedPartyLabel;
        if (!g_selectedPlayerInfo.detectedPartyPartners.empty()) {
            pText += " с " + g_selectedPlayerInfo.detectedPartyPartners;
        }
        if (g_selectedPlayerInfo.detectedSharedMatches > 0) {
            pText += " (" + std::to_string(g_selectedPlayerInfo.detectedSharedMatches) + " общих игр из 20)";
        }
        pText += " ] ";
        headerItems.push_back(text(pText) | bold | color(Color::CyanLight) | bgcolor(Color::RGB(15, 45, 60)) | center);
    }

    headerItems.push_back(separator());
    headerItems.push_back(hbox({
        renderProfileTabs(PlayerProfileTab::STATS),
        filler(),
        text(" [ 📊 БОЕВАЯ СТАТИСТИКА ] ") | bold | color(Color::GreenLight)
    }));
    headerItems.push_back(separator());

    auto contentBox = vbox({
        vbox(headerItems),
        hbox({
            // Left column: Combat Stats
            vbox({
                text(" БОЕВАЯ СТАТИСТИКА") | bold | color(Color::Cyan),
                separator(),
                hbox(text(" Любимый Агент: ") | color(Color::GrayLight), text(favAgent) | color(Color::YellowLight) | bold),
                hbox(text(" K/D Ratio:     ") | color(Color::GrayLight), text(kdStr) | color(kdColor) | bold),
                hbox(text(" Winrate:       ") | color(Color::GrayLight), text(winStr) | color(Color::GreenLight) | bold),
                hbox(text(" ACS (Счёт):    ") | color(Color::GrayLight), text(acsStr) | color(Color::MagentaLight) | bold),
                hbox(text(" Всего матчей:  ") | color(Color::GrayLight), text(g_selectedPlayerInfo.isPrivate ? "Профиль закрыт" : (g_selectedPlayerInfo.matchesPlayed > 0 ? std::to_string(g_selectedPlayerInfo.matchesPlayed) : "N/A")) | bold),
            }) | flex,
            separator(),
            // Middle column: Accuracy
            vbox({
                text(" ТОЧНОСТЬ СТРЕЛЬБЫ") | bold | color(Color::Yellow),
                separator(),
                bodyArt
            }) | flex,
            separator(),
            // Right column: Ranks
            vbox({
                text(" РАНГИ (RIOT COMPETITIVE)") | bold | color(Color::Green),
                separator(),
                hbox(text(" Текущий: ") | color(Color::GrayLight), text(curRank.name) | color(curRankCol) | bold),
                hbox(text(" Пиковый: ") | color(Color::GrayLight), text(pkRank.name) | color(pkRankCol) | bold)
            }) | flex
        }),
        separator(),
        backBtn | center
    }) | border;

    return vbox({
        filler(),
        hbox({ filler(), contentBox | size(WIDTH, GREATER_THAN, 66), filler() }),
        filler()
    });
}
