#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>

#include "ftxui_render.hpp"

struct AgentPickerItem {
    std::string uuid;
    std::string name;
    std::string role; // "Дуэлянт", "Инициатор", "Специалист", "Страж"
};

inline const std::vector<AgentPickerItem>& getStaticAgentList() {
    static const std::vector<AgentPickerItem> list = {
        // Дуэлянты
        {"add6443a-41bd-e414-f6ad-e58d267f4e95", "Jett", "Дуэлянт"},
        {"a3bfb853-43b2-7238-a4f1-ad90e9e46bcc", "Reyna", "Дуэлянт"},
        {"f94c3b30-42be-e959-889c-5aa313dba261", "Raze", "Дуэлянт"},
        {"eb93336a-449b-9c1b-0a54-a891f7921d69", "Phoenix", "Дуэлянт"},
        {"7f94d92c-4234-0a36-9646-3a87eb8b5c89", "Yoru", "Дуэлянт"},
        {"bb2a4828-46eb-8cd1-e765-15848195d751", "Neon", "Дуэлянт"},
        {"0e38b510-41a8-5780-5e8f-568b2a4f2d6c", "Iso", "Дуэлянт"},

        // Инициаторы
        {"320b2a48-4d9b-a075-30f1-1f93a9b638fa", "Sova", "Инициатор"},
        {"5f8d3a7f-467b-97f3-062c-13acf203c006", "Breach", "Инициатор"},
        {"6f2a04ca-43e0-be17-7f36-b3908627744d", "Skye", "Инициатор"},
        {"601dbbe7-43ce-be57-2a40-4abd24953621", "KAY/O", "Инициатор"},
        {"dade69b4-4f5a-8528-247b-219e5a1facd6", "Fade", "Инициатор"},
        {"e370fa57-4757-3604-3648-499e1f642d3f", "Gekko", "Инициатор"},
        {"b444168c-4e35-8076-db47-ef9bf368f384", "Tejo", "Инициатор"},

        // Специалисты (Смокеры)
        {"9f0d8ba9-4140-b941-57d3-a7ad57c6b417", "Brimstone", "Специалист"},
        {"8e253930-4c05-31dd-1b6c-968525494517", "Omen", "Специалист"},
        {"707eab51-4836-f488-046a-cda6bf494859", "Viper", "Специалист"},
        {"41fb69c1-4189-7b37-f117-bcaf1e96f1bf", "Astra", "Специалист"},
        {"95b78ed7-4637-86d9-7e41-71ba8c293152", "Harbor", "Специалист"},
        {"1dbf2edd-4729-0984-3115-daa5eed44993", "Clove", "Специалист"},

        // Стражи
        {"569fdd95-4d10-43ab-ca70-79becc718b46", "Sage", "Страж"},
        {"117ed9e3-49f3-6512-3ccf-0cada7e3823b", "Cypher", "Страж"},
        {"1e58de9c-4950-5125-93e9-a0aee9f98746", "Killjoy", "Страж"},
        {"22697a3d-45bf-8dd7-4fec-84a9e28c69d7", "Chamber", "Страж"},
        {"cc8b64c8-4b25-4ff9-6e7f-37b4da43d235", "Deadlock", "Страж"},
        {"efba5359-4016-a1e5-7626-b1ae76895940", "Vyse", "Страж"}
    };
    return list;
}

inline ftxui::Element renderAgentPicker(const MatchState& matchState, const std::map<std::string, std::string>& mapNameMap, const std::set<std::string>& ownedAgents) {
    using namespace ftxui;

    std::string currentMapName = mapNameMap.count(matchState.mapId) ? mapNameMap.at(matchState.mapId) : matchState.mapId;
    if (currentMapName.empty()) currentMapName = "Неизвестно";

    // Set of agents already picked by allies in pregame
    std::set<std::string> allyPickedAgents;
    for (const auto& p : matchState.players) {
        if (!p.characterId.empty()) {
            std::string cid = p.characterId;
            for (auto& c : cid) c = tolower(c);
            allyPickedAgents.insert(cid);
        }
    }

    // Role definitions
    struct RoleGroup {
        std::string title;
        Color color;
        std::vector<AgentPickerItem> agents;
    };

    std::vector<RoleGroup> groups = {
        {"ДУЭЛЯНТЫ", Color::RedLight, {}},
        {"ИНИЦИАТОРЫ", Color::YellowLight, {}},
        {"СПЕЦИАЛИСТЫ", Color::MagentaLight, {}},
        {"СТРАЖИ", Color::CyanLight, {}}
    };

    const auto& allAgents = getStaticAgentList();
    for (const auto& ag : allAgents) {
        if (ag.role == "Дуэлянт") groups[0].agents.push_back(ag);
        else if (ag.role == "Инициатор") groups[1].agents.push_back(ag);
        else if (ag.role == "Специалист") groups[2].agents.push_back(ag);
        else if (ag.role == "Страж") groups[3].agents.push_back(ag);
    }

    std::vector<Element> columnElements;
    for (size_t gIdx = 0; gIdx < groups.size(); ++gIdx) {
        const auto& grp = groups[gIdx];
        std::vector<Element> items;
        items.push_back(text(" " + grp.title + " ") | bold | color(grp.color) | center);
        items.push_back(separator());

        for (const auto& ag : grp.agents) {
            std::string aid = ag.uuid;
            for (auto& c : aid) c = tolower(c);

            bool isPicked = allyPickedAgents.count(aid);
            bool isOwned = (ownedAgents.empty() || ownedAgents.count(aid));

            if (isPicked) {
                items.push_back(
                    text(" " + ag.name + " (Занят) ") 
                    | color(Color::GrayDark) 
                    | center 
                    | border
                );
            } else if (!isOwned && ownedAgents.size() > 5) {
                items.push_back(
                    text(" " + ag.name + " (Закрыт) ") 
                    | color(Color::RGB(120, 120, 120)) 
                    | center 
                    | border
                );
            } else {
                std::string btnId = "agent_" + aid;
                auto btn = renderButton(ag.name, g_agentBoxes[aid], grp.color, btnId, g_mousePressed)
                         | reflect(g_agentBoxes[aid])
                         | center;
                items.push_back(btn);
            }
        }

        columnElements.push_back(vbox(std::move(items)) | flex);
        if (gIdx + 1 < groups.size()) {
            columnElements.push_back(separator());
        }
    }

    Element statusElement = text("");
    if (!g_pickStatusMsg.empty()) {
        Color sColor = Color::YellowLight;
        if (g_pickStatusMsg.find("[OK]") != std::string::npos || g_pickStatusMsg.find("УСПЕШНО") != std::string::npos) sColor = Color::GreenLight;
        else if (g_pickStatusMsg.find("[ОШИБКА]") != std::string::npos || g_pickStatusMsg.find("ERROR") != std::string::npos) sColor = Color::RedLight;

        statusElement = hbox({
            text("  " + g_pickStatusMsg + "  ") | bold | color(sColor) | border
        }) | center;
    }

    auto backBtn = renderButton("Назад к матчу", g_agentPickerBackBox, Color::GrayLight, "agent_picker_back", g_mousePressed) 
                 | reflect(g_agentPickerBackBox) 
                 | center;

    auto modal = vbox({
        // Top Banner
        hbox({
            text(" ВЫБОР АГЕНТА (АВТО-ПИК) ") | bold | color(Color::YellowLight),
            filler(),
            text(" КАРТА: ") | bold | color(Color::Black) | bgcolor(Color::Yellow),
            text(" " + currentMapName + " ") | bold | color(Color::White) | bgcolor(Color::RGB(50, 50, 75)),
        }),
        separator(),
        text("Нажмите на любого доступного агента для мгновенного пика в игре") | color(Color::GrayLight) | center,
        statusElement,
        separator(),
        hbox(std::move(columnElements)),
        separator(),
        backBtn
    }) | border | size(WIDTH, GREATER_THAN, 85);

    return vbox({
        filler(),
        hbox({ filler(), modal, filler() }),
        filler()
    });
}
