#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>

#include "ftxui_render.hpp"

struct WeaponSkinInfo {
    std::string weaponUuid;
    std::string weaponName;
    std::string category;
    int categoryOrder = 0;
    int weaponOrder = 0;
    std::string skinUuid;
    std::string skinName;
    std::string chromaUuid;
    std::string chromaName;
    std::string fullName;
    bool isCustom = false;
};

struct PlayerWeaponLoadout {
    std::string puuid;
    std::map<std::string, WeaponSkinInfo> weapons;
    bool loaded = false;
    bool loading = false;
};

extern std::map<std::string, PlayerWeaponLoadout> g_playerLoadouts;
extern std::mutex g_loadoutsMutex;
extern ftxui::Box g_tabSkinsBox;
extern ftxui::Box g_tabStatsBox;

inline ftxui::Element renderProfileTabs(PlayerProfileTab currentTab) {
    using namespace ftxui;
    
    Element skinsBtn;
    if (currentTab == PlayerProfileTab::SKINS) {
        skinsBtn = hbox({
            text(" [ * СКИНЫ ОРУЖИЯ (1/S) ] ") | bold | color(Color::CyanLight) | bgcolor(Color::RGB(15, 45, 60))
        }) | borderHeavy | reflect(g_tabSkinsBox);
    } else {
        skinsBtn = hbox({
            text(" [   СКИНЫ ОРУЖИЯ (1/S) ] ") | color(Color::GrayLight)
        }) | border | reflect(g_tabSkinsBox);
    }

    Element statsBtn;
    if (currentTab == PlayerProfileTab::STATS) {
        statsBtn = hbox({
            text(" [ * СТАТИСТИКА БОЯ (2/T) ] ") | bold | color(Color::CyanLight) | bgcolor(Color::RGB(15, 45, 60))
        }) | borderHeavy | reflect(g_tabStatsBox);
    } else {
        statsBtn = hbox({
            text(" [   СТАТИСТИКА БОЯ (2/T) ] ") | color(Color::GrayLight)
        }) | border | reflect(g_tabStatsBox);
    }

    return hbox({
        skinsBtn,
        text(" "),
        statsBtn
    });
}

inline ftxui::Element renderCategorySection(const std::string& title, ftxui::Color titleColor, const std::vector<WeaponSkinInfo>& weapons) {
    using namespace ftxui;
    Elements rows;

    rows.push_back(
        hbox({
            text("── " + title + " ") | color(titleColor) | bold,
            separator() | color(Color::RGB(55, 65, 75))
        })
    );

    for (const auto& w : weapons) {
        Color nameColor = w.isCustom ? Color::CyanLight : Color::GrayLight;
        Color skinColor = w.isCustom ? Color::YellowLight : Color::GrayDark;
        
        std::string dispSkin = w.fullName;
        if (dispSkin.empty()) {
            dispSkin = "Стандартный " + w.weaponName;
        }

        std::replace(dispSkin.begin(), dispSkin.end(), '\n', ' ');

        // Safe truncation to keep width compact
        if (dispSkin.length() > 32) {
            dispSkin = utf8_substr(dispSkin, 30) + "..";
        }

        Element starIcon = w.isCustom ? (text("★ ") | color(Color::Gold1) | bold) : (text("· ") | color(Color::GrayDark));
        Element skinText = text(dispSkin) | color(skinColor);
        if (w.isCustom) skinText = skinText | bold;

        Element row = hbox({
            text(" " + w.weaponName) | color(nameColor) | bold | size(WIDTH, EQUAL, 10),
            starIcon,
            skinText | flex
        });
        rows.push_back(row);
    }

    return vbox(std::move(rows));
}

inline ftxui::Element renderPlayerSkins(const PlayerWeaponLoadout& loadout) {
    using namespace ftxui;

    auto backBtn = renderButton("ESC - Назад в лобби", g_backButtonBox, Color::CyanLight, "back", g_mousePressed) | reflect(g_backButtonBox);

    std::string fullName = g_selectedPlayerInfo.gameName + (g_selectedPlayerInfo.tagLine.empty() ? "" : "#" + g_selectedPlayerInfo.tagLine);
    ftxui::Element nameTitle = text(fullName) | bold | color(Color::White);
    if (g_selectedPlayerInfo.isMuted) {
        nameTitle = hbox(nameTitle, text(" [В МУТЕ]") | color(Color::RedLight) | bold);
    }

    std::string pStatus;
    { std::lock_guard<std::mutex> lk(g_penaltiesMutex); pStatus = g_penaltiesStatus; }
    if (g_selectedPuuid == g_localPuuid && !pStatus.empty()) {
        nameTitle = vbox(nameTitle, text(pStatus) | color(Color::RedLight) | bold);
    }

    RankDisplay curRank = formatRank(g_selectedPlayerInfo.rankTier, g_selectedPlayerInfo.rankRR, g_selectedPlayerInfo.peakRankTier, g_selectedPlayerInfo.peakRankRR);
    Color curRankCol = getRankColorFTX(g_selectedPlayerInfo.rankTier > 2 ? g_selectedPlayerInfo.rankTier : g_selectedPlayerInfo.peakRankTier);

    // Group weapons into categories
    std::vector<WeaponSkinInfo> sidearms;
    std::vector<WeaponSkinInfo> smgs;
    std::vector<WeaponSkinInfo> shotguns;
    std::vector<WeaponSkinInfo> rifles;
    std::vector<WeaponSkinInfo> snipers;
    std::vector<WeaponSkinInfo> heavies;
    std::vector<WeaponSkinInfo> melee;

    int customCount = 0;
    int totalWeapons = 0;

    for (const auto& [_, w] : loadout.weapons) {
        totalWeapons++;
        if (w.isCustom) customCount++;

        if (w.categoryOrder == 1) sidearms.push_back(w);
        else if (w.categoryOrder == 2) smgs.push_back(w);
        else if (w.categoryOrder == 3) shotguns.push_back(w);
        else if (w.categoryOrder == 4) rifles.push_back(w);
        else if (w.categoryOrder == 5) snipers.push_back(w);
        else if (w.categoryOrder == 6) heavies.push_back(w);
        else if (w.categoryOrder == 7) melee.push_back(w);
        else rifles.push_back(w);
    }

    auto sortFn = [](const WeaponSkinInfo& a, const WeaponSkinInfo& b) {
        return a.weaponOrder < b.weaponOrder;
    };
    std::sort(sidearms.begin(), sidearms.end(), sortFn);
    std::sort(smgs.begin(), smgs.end(), sortFn);
    std::sort(shotguns.begin(), shotguns.end(), sortFn);
    std::sort(rifles.begin(), rifles.end(), sortFn);
    std::sort(snipers.begin(), snipers.end(), sortFn);
    std::sort(heavies.begin(), heavies.end(), sortFn);
    std::sort(melee.begin(), melee.end(), sortFn);

    Element skinsBody;
    if (!loadout.loaded && loadout.weapons.empty()) {
        skinsBody = vbox({
            text(""),
            text(" Получение данных об экипированных скинах из матча... ") | color(Color::CyanLight) | bold | center,
            text(" (Скины считываются из активного лобби Riot Client API) ") | color(Color::GrayLight) | center,
            text("")
        }) | border;
    } else {
        auto leftCol = vbox({
            renderCategorySection("ПИСТОЛЕТЫ", Color::Cyan, sidearms),
            text(""),
            renderCategorySection("ПИСТОЛЕТЫ-ПУЛЕМЁТЫ", Color::MagentaLight, smgs),
            text(""),
            renderCategorySection("ДРОБОВИКИ", Color::RedLight, shotguns),
        }) | border | flex;

        auto rightCol = vbox({
            renderCategorySection("ВИНТОВКИ", Color::GreenLight, rifles),
            text(""),
            renderCategorySection("СНАЙПЕРСКИЕ ВИНТОВКИ", Color::BlueLight, snipers),
            text(""),
            renderCategorySection("ПУЛЕМЕТЫ", Color::Yellow, heavies),
            text(""),
            renderCategorySection("ХОЛОДНОЕ ОРУЖИЕ", Color::Gold1, melee),
        }) | border | flex;

        skinsBody = hbox({
            leftCol,
            separator(),
            rightCol
        });
    }

    auto headerBox = vbox({
        hbox({
            text(" Игрок: ") | bold | color(Color::GrayLight),
            nameTitle,
            text("  [ " + curRank.name + " ]") | bold | color(curRankCol),
            filler(),
            text(" [ КОЛЛЕКЦИЯ СКИНОВ В ИГРЕ ] ") | bold | color(Color::CyanLight) | border
        }),
        separator(),
        hbox({
            renderProfileTabs(PlayerProfileTab::SKINS),
            filler(),
            text(" ★ Особых скинов: " + std::to_string(customCount) + " из " + std::to_string(std::min(19, std::max(19, totalWeapons))) + " ") | color(Color::YellowLight) | bold
        })
    });

    auto contentBox = vbox({
        headerBox,
        separator(),
        skinsBody,
        separator(),
        backBtn | center
    }) | border;

    return vbox({
        contentBox | size(WIDTH, GREATER_THAN, 80) | size(WIDTH, LESS_THAN, 110) | center
    });
}
