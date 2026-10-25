#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>
#include <string>
#include <map>
#include <set>

#include "ftxui_render.hpp"

extern ftxui::Box g_reportOpenBox;
extern ftxui::Box g_reportCloseBox;
extern std::map<std::string, ftxui::Box> g_reportEnemyBoxes;
extern std::string g_reportStatusMsg;
extern std::set<std::string> g_reportedPuuids;

inline ftxui::Element renderReportEnemyMenu(const MatchState& state, const Session& session, const std::map<std::string, std::string>& agentMap) {
    using namespace ftxui;

    // 1. Identify local player team
    std::string localTeamId = "";
    for (const auto& p : state.players) {
        if (!session.puuid.empty() && p.puuid == session.puuid) {
            localTeamId = p.teamId;
            break;
        }
    }

    // 2. Collect enemies (strictly enemies, only their agents)
    std::vector<PlayerInfo> enemies;
    for (const auto& p : state.players) {
        if (!localTeamId.empty()) {
            if (p.teamId != localTeamId) {
                enemies.push_back(p);
            }
        } else {
            // fallback if local team unknown
            if (p.teamId == "Red" || p.teamId == "Defender") {
                enemies.push_back(p);
            }
        }
    }

    Elements enemyListElements;

    if (enemies.empty()) {
        enemyListElements.push_back(
            hbox({
                filler(),
                text(" ⏳ Противники ещё не обнаружены. Дождитесь загрузки в матч! ")
                    | bold | color(Color::YellowLight),
                filler()
            })
        );
    } else {
        for (size_t i = 0; i < enemies.size(); ++i) {
            const auto& en = enemies[i];

            // Resolve agent name (only their agents)
            std::string agentDisplayName = en.agentName;
            if (agentDisplayName.empty() && !en.characterId.empty()) {
                std::string cid = en.characterId;
                for (auto& c : cid) c = tolower(c);
                if (agentMap.count(cid)) agentDisplayName = agentMap.at(cid);
                else if (agentMap.count(en.characterId)) agentDisplayName = agentMap.at(en.characterId);
            }
            if (agentDisplayName.empty()) {
                agentDisplayName = "Агент #" + std::to_string(i + 1);
            }

            bool alreadyReported = g_reportedPuuids.count(en.puuid);

            std::string btnLabel;
            Color btnColor;
            if (alreadyReported) {
                btnLabel = "  [✓] РЕПОРТ ОТПРАВЛЕН: " + agentDisplayName + " (Читы, АФК, Абуз ранга)  ";
                btnColor = Color::GreenLight;
            } else {
                btnLabel = "  ⚠ КИНУТЬ РЕПОРТ: " + agentDisplayName + " (Читы + АФК + Абуз ранга)  ";
                btnColor = Color::RedLight;
            }

            std::string btnId = "report_" + en.puuid;
            auto btnElem = renderButton(btnLabel, g_reportEnemyBoxes[en.puuid], btnColor, btnId, g_mousePressed)
                         | reflect(g_reportEnemyBoxes[en.puuid])
                         | center;

            enemyListElements.push_back(btnElem);
            if (i + 1 < enemies.size()) {
                enemyListElements.push_back(separator());
            }
        }
    }

    // Status message display
    Element statusElem = text("");
    if (!g_reportStatusMsg.empty()) {
        Color col = Color::YellowLight;
        if (g_reportStatusMsg.find("✅") != std::string::npos || g_reportStatusMsg.find("отправлена") != std::string::npos) {
            col = Color::GreenLight;
        } else if (g_reportStatusMsg.find("❌") != std::string::npos || g_reportStatusMsg.find("Ошибка") != std::string::npos) {
            col = Color::RedLight;
        }
        statusElem = hbox({
            filler(),
            text("  " + g_reportStatusMsg + "  ") | bold | color(col) | border,
            filler()
        });
    }

    auto closeBtn = renderButton("⬅ НАЗАД В МАТЧ", g_reportCloseBox, Color::GrayLight, "report_close", g_mousePressed)
                  | reflect(g_reportCloseBox)
                  | center;

    auto modal = vbox({
        hbox({
            filler(),
            text(" ⚠ ПОЖАЛОВАТЬСЯ НА ПРОТИВНИКА (REPORT) ") | bold | color(Color::RedLight),
            filler(),
        }),
        separator(),
        text("Нажмите на агента противника для отправки репорта: Читы + АФК + Абуз ранга")
            | color(Color::GrayLight) | center,
        separator(),
        statusElem,
        vbox(std::move(enemyListElements)) | border,
        separator(),
        closeBtn
    }) | border | size(WIDTH, GREATER_THAN, 80);

    return vbox({
        filler(),
        hbox({
            filler(),
            modal,
            filler(),
        }),
        filler(),
    });
}
