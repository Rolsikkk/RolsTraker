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
#include <cmath>

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
#ifndef UPDATE_INFO_DEFINED
#define UPDATE_INFO_DEFINED
struct UpdateInfo {
    bool available = false;
    std::string latestVersion;
    std::string releaseTitle;
    std::string releaseNotes;
    std::string downloadUrl;
    bool isDownloading = false;
    std::string statusText;
    int downloadPercent = -1;
};
#endif

extern UpdateInfo g_updateInfo;
extern std::string g_updateStatus;
extern std::mutex g_updateMutex;
extern bool g_showUpdateModal;
extern ftxui::Box g_updateNowBox;
extern ftxui::Box g_updateLaterBox;
extern ftxui::Box g_updateBadgeBox;
void startUpdateDownload();
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
extern std::map<std::string, ftxui::Box> g_skinRowBoxes;
extern std::string g_selectedSkinWeaponUuid;
extern ftxui::Box g_skinModalCloseBox;
extern ftxui::Box g_skinModalBox;
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
    bool isSkinModalActive = (g_currentView == AppView::PLAYER_STATS && !g_selectedSkinWeaponUuid.empty());
    bool hovered = box.Contain(g_mouseX, g_mouseY);
    if (isSkinModalActive && btnId != "skin_modal_close") {
        hovered = false;
    }
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

inline ftxui::Element renderMyMMRBadge(int tier, int rr, int peakTier = 0, int peakRR = 0, int avgLobbyElo = 0) {
    if (tier < 3) {
        if (peakTier >= 3) {
            std::string peakName = "";
            if (peakTier >= 3 && peakTier <= 5) peakName = "Железо " + std::to_string(peakTier - 2);
            else if (peakTier >= 6 && peakTier <= 8) peakName = "Бронза " + std::to_string(peakTier - 5);
            else if (peakTier >= 9 && peakTier <= 11) peakName = "Серебро " + std::to_string(peakTier - 8);
            else if (peakTier >= 12 && peakTier <= 14) peakName = "Золото " + std::to_string(peakTier - 11);
            else if (peakTier >= 15 && peakTier <= 17) peakName = "Платина " + std::to_string(peakTier - 14);
            else if (peakTier >= 18 && peakTier <= 20) peakName = "Алмаз " + std::to_string(peakTier - 17);
            else if (peakTier >= 21 && peakTier <= 23) peakName = "Расцвет " + std::to_string(peakTier - 20);
            else if (peakTier >= 24 && peakTier <= 26) peakName = "Бессмертный " + std::to_string(peakTier - 23);
            else if (peakTier >= 27) peakName = "Радиант";
            int peakElo = (peakTier - 2) * 100 + peakRR;
            return ftxui::hbox({
                ftxui::text(" ⚡ Скрытый MMR: ") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight),
                ftxui::text("Unrated ") | ftxui::bold | ftxui::color(ftxui::Color::GrayLight),
                ftxui::text("[Пик: " + peakName + " • " + std::to_string(peakElo) + " ELO]") | ftxui::bold | ftxui::color(getRankColorFTX(peakTier))
            }) | ftxui::center;
        }
        return ftxui::hbox({
            ftxui::text(" ⚡ Скрытый MMR: ") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight),
            ftxui::text("Unrated (Калибровка)") | ftxui::bold | ftxui::color(ftxui::Color::GrayLight)
        }) | ftxui::center;
    }

    int myElo = (tier - 2) * 100 + rr;
    std::string rankName = "";
    if (tier >= 3 && tier <= 5) rankName = "Железо " + std::to_string(tier - 2);
    else if (tier >= 6 && tier <= 8) rankName = "Бронза " + std::to_string(tier - 5);
    else if (tier >= 9 && tier <= 11) rankName = "Серебро " + std::to_string(tier - 8);
    else if (tier >= 12 && tier <= 14) rankName = "Золото " + std::to_string(tier - 11);
    else if (tier >= 15 && tier <= 17) rankName = "Платина " + std::to_string(tier - 14);
    else if (tier >= 18 && tier <= 20) rankName = "Алмаз " + std::to_string(tier - 17);
    else if (tier >= 21 && tier <= 23) rankName = "Расцвет " + std::to_string(tier - 20);
    else if (tier >= 24 && tier <= 26) rankName = "Бессмертный " + std::to_string(tier - 23);
    else if (tier >= 27) rankName = "Радиант";

    ftxui::Color rankColor = getRankColorFTX(tier);

    std::vector<ftxui::Element> parts;
    parts.push_back(ftxui::text(" ⚡ Скрытый MMR: ") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight));
    parts.push_back(ftxui::text(std::to_string(myElo) + " ELO ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight));
    parts.push_back(ftxui::text("[" + rankName + " • " + std::to_string(rr) + " RR] ") | ftxui::bold | ftxui::color(rankColor));

    if (avgLobbyElo > 0) {
        int diff = myElo - avgLobbyElo;
        parts.push_back(ftxui::separator());
        parts.push_back(ftxui::text(" Лобби: " + std::to_string(avgLobbyElo) + " ELO "));
        if (diff > 25) {
            parts.push_back(ftxui::text("(+" + std::to_string(diff) + " выше лобби 🚀)") | ftxui::bold | ftxui::color(ftxui::Color::GreenLight));
        } else if (diff < -25) {
            parts.push_back(ftxui::text("(" + std::to_string(diff) + " ниже лобби ⚠️)") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
        } else {
            parts.push_back(ftxui::text("(сбалансировано ⚖️)") | ftxui::color(ftxui::Color::YellowLight));
        }
    }

    return ftxui::hbox(std::move(parts)) | ftxui::center;
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

        bool hasNoSkins = (p.skinsLoaded && p.customSkinCount == 0);

        if (!p.isConnected || !p.connectionStatus.empty()) {
            std::string statusStr = !p.connectionStatus.empty() ? p.connectionStatus : "[AFK]";
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
            nameParts.push_back(ftxui::text(statusStr) | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::Red));
        } else if (hasNoSkins) {
            nameParts.push_back(ftxui::text(" " + fullName + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
            nameParts.push_back(ftxui::text("[СМУРФ] ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight));
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

        // Smurf & Booster detection (disabled)

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

inline ftxui::Element renderUpdateModal() {
    UpdateInfo info;
    {
        std::lock_guard<std::mutex> lk(g_updateMutex);
        info = g_updateInfo;
    }

    std::vector<ftxui::Element> notesElements;
    std::string notes = info.releaseNotes;
    if (notes.empty()) {
        notes = "Описание изменений в этой версии не указано.";
    }

    std::stringstream ss(notes);
    std::string line;
    int lineCount = 0;
    while (std::getline(ss, line) && lineCount < 12) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;

        if (line.rfind("###", 0) == 0 || line.rfind("##", 0) == 0 || line.rfind("#", 0) == 0) {
            std::string headerText = line;
            size_t p = headerText.find_first_not_of("# ");
            if (p != std::string::npos) headerText = headerText.substr(p);
            notesElements.push_back(ftxui::text(" ▶ " + headerText) | ftxui::bold | ftxui::color(ftxui::Color::CyanLight));
            lineCount++;
        } else if (line.rfind("-", 0) == 0 || line.rfind("*", 0) == 0) {
            std::string itemText = line.substr(1);
            size_t p = itemText.find_first_not_of(" ");
            if (p != std::string::npos) itemText = itemText.substr(p);
            notesElements.push_back(ftxui::hbox({
                ftxui::text("  • ") | ftxui::bold | ftxui::color(ftxui::Color::GreenLight),
                ftxui::paragraph(itemText) | ftxui::color(ftxui::Color::White)
            }));
            lineCount++;
        } else {
            notesElements.push_back(ftxui::paragraph("    " + line) | ftxui::color(ftxui::Color::GrayLight));
            lineCount++;
        }
    }
    if (notesElements.empty()) {
        notesElements.push_back(ftxui::text("    Нет дополнительной информации.") | ftxui::color(ftxui::Color::GrayDark));
    }

    ftxui::Element actionArea;
    if (info.isDownloading) {
        static auto start_time_spin = std::chrono::steady_clock::now();
        int frame = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_spin).count() / 100;
        std::string label = info.statusText.empty() ? "Скачивание обновления..." : info.statusText;
        float frac = (info.downloadPercent >= 0 && info.downloadPercent <= 100) ? ((float)info.downloadPercent / 100.0f) : 0.0f;
        actionArea = ftxui::vbox({
            ftxui::text(""),
            ftxui::hbox({
                ftxui::filler(),
                circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
                ftxui::text("  " + label) | ftxui::bold | ftxui::color(ftxui::Color::YellowLight),
                ftxui::filler()
            }),
            ftxui::text(""),
            (info.downloadPercent >= 0 ? ftxui::hbox({
                ftxui::filler(),
                ftxui::gauge(frac) | ftxui::color(ftxui::Color::Cyan) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 34),
                ftxui::text(" " + std::to_string(info.downloadPercent) + "%") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight),
                ftxui::filler()
            }) : ftxui::text("")),
            ftxui::text("")
        });
    } else {
        auto updateBtn = renderButton("ОБНОВИТЬ СЕЙЧАС", g_updateNowBox, ftxui::Color::GreenLight, "update_now", g_mousePressed)
                       | ftxui::reflect(g_updateNowBox);
        auto laterBtn = renderButton("НАПОМНИТЬ ПОЗЖЕ", g_updateLaterBox, ftxui::Color::GrayLight, "update_later", g_mousePressed)
                      | ftxui::reflect(g_updateLaterBox);

        actionArea = ftxui::vbox({
            ftxui::text(""),
            ftxui::hbox({
                ftxui::filler(),
                updateBtn,
                ftxui::text("    "),
                laterBtn,
                ftxui::filler()
            }),
            ftxui::text(""),
            ftxui::text(" [ENTER / Y] Обновить  •  [ESC / N] Напомнить позже ") | ftxui::color(ftxui::Color::GrayDark) | ftxui::center,
            ftxui::text("")
        });
    }

    auto card = ftxui::vbox({
        ftxui::text(""),
        ftxui::text(" 🚀 ДОСТУПНО НОВОЕ ОБНОВЛЕНИЕ ") | ftxui::bold | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(ftxui::Color::GreenLight) | ftxui::center,
        ftxui::text(""),
        ftxui::hbox({
            ftxui::filler(),
            ftxui::text("Текущая версия: ") | ftxui::color(ftxui::Color::GrayLight),
            ftxui::text(CURRENT_VERSION) | ftxui::bold | ftxui::color(ftxui::Color::White),
            ftxui::text("  ➜  Новая версия: ") | ftxui::color(ftxui::Color::GrayLight),
            ftxui::text(info.latestVersion) | ftxui::bold | ftxui::color(ftxui::Color::YellowLight),
            ftxui::filler()
        }),
        (!info.releaseTitle.empty() ? (ftxui::text("« " + info.releaseTitle + " »") | ftxui::bold | ftxui::color(ftxui::Color::CyanLight) | ftxui::center) : ftxui::text("")),
        ftxui::text(""),
        ftxui::separator(),
        ftxui::text(" ЧТО ДОБАВИЛИ / ИЗМЕНИЛИ: ") | ftxui::bold | ftxui::color(ftxui::Color::YellowLight),
        ftxui::text(""),
        ftxui::vbox(std::move(notesElements)) | ftxui::borderStyled(ftxui::ROUNDED, ftxui::Color::GrayDark) | ftxui::bgcolor(ftxui::Color::RGB(15, 18, 26)),
        actionArea
    })
    | ftxui::bgcolor(ftxui::Color::RGB(10, 14, 20))
    | ftxui::borderStyled(ftxui::DOUBLE, ftxui::Color::Cyan)
    | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 78);

    return ftxui::vbox({
        ftxui::filler(),
        ftxui::hbox({
            ftxui::filler(),
            card,
            ftxui::filler()
        }),
        ftxui::filler()
    });
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

        auto myStatsBtn = renderButton("Моя статистика", g_myStatsBox, ftxui::Color::CyanLight, "my_stats", g_mousePressed)
                        | ftxui::reflect(g_myStatsBox);

        auto updateBanner = g_updateInfo.available ? ftxui::vbox({
            renderButton("🚀 Доступно обновление " + g_updateInfo.latestVersion + " (посмотреть изменения)", g_updateBadgeBox, ftxui::Color::GreenLight, "update_badge", g_mousePressed) | ftxui::reflect(g_updateBadgeBox) | ftxui::center,
            ftxui::text("")
        }) : ftxui::text("");

        int myTier = 0, myRR = 0, myPeakTier = 0, myPeakRR = 0;
        if (!session.puuid.empty()) {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
            if (g_rankCache.count(session.puuid)) {
                const auto& rk = g_rankCache[session.puuid];
                myTier = rk.tier;
                myRR = rk.rr;
                myPeakTier = rk.peakTier;
                myPeakRR = rk.peakRR;
            }
        }
        std::string penaltiesStatus;
        { std::lock_guard<std::mutex> lk(g_penaltiesMutex); penaltiesStatus = g_penaltiesStatus; }

        auto myMmrBadge = renderMyMMRBadge(myTier, myRR, myPeakTier, myPeakRR, 0);

        return ftxui::vbox({
            ftxui::filler(),
            circleSpinner(frame) | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
            ftxui::filler(),
            updateElem,
            updateBanner,
            ftxui::text(" Ожидание начала матча... ") | ftxui::color(ftxui::Color::GrayDark) | ftxui::center,
            text(""),
            ftxui::hbox({
                myStatsBtn
            }) | ftxui::center,
            ftxui::text(""),
            myMmrBadge,
            (!penaltiesStatus.empty() ? ftxui::vbox({
                ftxui::text(""),
                ftxui::text(" ⛔ " + penaltiesStatus + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::center
            }) : ftxui::text("")),
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
            int elo = (p.rankTier - 2) * 100 + p.rankRR;
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
        ftxui::text(" ROLSTRACKER ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
        ftxui::text("(" + CURRENT_VERSION + ") ") | ftxui::color(ftxui::Color::GrayDark),
        ftxui::separator(),
        ftxui::text(" "),
        header,
        ftxui::filler() | ftxui::flex,
        (g_updateInfo.available ? (
            renderButton("🚀 Доступно обновление " + g_updateInfo.latestVersion, g_updateBadgeBox, ftxui::Color::GreenLight, "update_badge", g_mousePressed)
            | ftxui::reflect(g_updateBadgeBox)
        ) : ftxui::text(""))
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

    {
        int myTier = 0, myRR = 0, myPeakTier = 0, myPeakRR = 0;
        for (const auto& p : state.players) {
            if (!session.puuid.empty() && p.puuid == session.puuid) {
                myTier = p.rankTier;
                myRR = p.rankRR;
                break;
            }
        }
        if (!session.puuid.empty()) {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
            if (g_rankCache.count(session.puuid)) {
                const auto& rk = g_rankCache[session.puuid];
                if (myTier == 0) myTier = rk.tier;
                if (myRR == 0) myRR = rk.rr;
                myPeakTier = rk.peakTier;
                myPeakRR = rk.peakRR;
            }
        }
        int avgElo = (rankedPlayers > 1) ? (totalLobbyElo / rankedPlayers) : 0;
        auto myMmrBadge = renderMyMMRBadge(myTier, myRR, myPeakTier, myPeakRR, avgElo);
        uiElements.push_back(ftxui::separator());
        uiElements.push_back(myMmrBadge);
    }

    if (!penaltiesStatus.empty()) {
        uiElements.push_back(ftxui::separator());
        uiElements.push_back(ftxui::text(" ⛔ " + penaltiesStatus + " ") | ftxui::bold | ftxui::color(ftxui::Color::RedLight) | ftxui::center);
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
        ftxui::filler() | ftxui::flex
    }));

    return ftxui::vbox(std::move(uiElements)) | ftxui::border;
}

inline ftxui::Element renderPostMatchOverlay(const PostMatchInfo& info) {
    using namespace ftxui;

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

    auto getRankTierName = [](int tier) -> std::string {
        if (tier >= 27) return "Radiant";
        if (tier >= 24 && tier <= 26) return "Immortal " + std::to_string(tier - 23);
        if (tier >= 21 && tier <= 23) return "Ascendant " + std::to_string(tier - 20);
        if (tier >= 18 && tier <= 20) return "Diamond " + std::to_string(tier - 17);
        if (tier >= 15 && tier <= 17) return "Platinum " + std::to_string(tier - 14);
        if (tier >= 12 && tier <= 14) return "Gold " + std::to_string(tier - 11);
        if (tier >= 9 && tier <= 11) return "Silver " + std::to_string(tier - 8);
        if (tier >= 6 && tier <= 8) return "Bronze " + std::to_string(tier - 5);
        if (tier >= 3 && tier <= 5) return "Iron " + std::to_string(tier - 2);
        return "Unrated";
    };

    auto getDivisionName = [](int tier) -> std::string {
        if (tier >= 27) return "RADIANT";
        if (tier >= 24) return "IMMORTAL";
        if (tier >= 21) return "ASCENDANT";
        if (tier >= 18) return "DIAMOND";
        if (tier >= 15) return "PLATINUM";
        if (tier >= 12) return "GOLD";
        if (tier >= 9)  return "SILVER";
        if (tier >= 6)  return "BRONZE";
        if (tier >= 3)  return "IRON";
        return "UNRATED";
    };

    int tierBefore = (info.tierBefore > 0) ? info.tierBefore : info.tierAfter;
    int tierAfter = (info.tierAfter > 0) ? info.tierAfter : tierBefore;
    int targetRrBefore = std::max(0, info.rrBefore);
    int targetRrAfter = std::max(0, info.rrAfter);
    int rrEarned = info.rrEarned;
    if (info.tierBefore <= 0 && targetRrBefore == 0 && rrEarned != 0) {
        targetRrBefore = std::max(0, targetRrAfter - rrEarned);
    }

    bool isPromo = (tierAfter > tierBefore && tierBefore > 0);
    bool isDemo = (tierAfter < tierBefore && tierAfter > 0);
    bool isMajorPromo = isPromo && ((tierBefore - 3) / 3 != (tierAfter - 3) / 3);

    auto now = std::chrono::steady_clock::now();
    int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - g_postMatchStartTime).count();
    int elapsedSec = elapsedMs / 1000;
    int remaining = std::max(0, 15 - elapsedSec);

    // Animation progress (0.0 to 1.0) between 300ms and 2400ms
    float animProgress = 0.0f;
    if (elapsedMs > 300) {
        animProgress = std::min(1.0f, (float)(elapsedMs - 300) / 2100.0f);
    }
    // Smooth ease-out
    float ease = (animProgress >= 1.0f) ? 1.0f : (1.0f - std::pow(1.0f - animProgress, 2.2f));

    // Rolling RR earned count
    int displayEarned = static_cast<int>(std::round(ease * rrEarned));
    std::string rrSign = (rrEarned > 0) ? "+" : "";
    std::string rrText = rrSign + std::to_string(displayEarned) + " RR";

    // Outcome title & colors
    Color outcomeColor = (rrEarned > 0) ? Color::GreenLight : ((rrEarned < 0) ? Color::RedLight : Color::YellowLight);
    std::string outcomeTitle = (rrEarned > 0) ? "ПОБЕДА" : ((rrEarned < 0) ? "ПОРАЖЕНИЕ" : "НИЧЬЯ");

    // Dynamic Tier & Progress Bar calculation
    int currentDisplayTier = tierAfter;
    int currentDisplayRr = targetRrAfter;

    if (isPromo) {
        if (animProgress < 0.45f) {
            currentDisplayTier = tierBefore;
            float p1 = animProgress / 0.45f;
            currentDisplayRr = static_cast<int>(targetRrBefore + (100 - targetRrBefore) * (1.0f - std::pow(1.0f - p1, 1.8f)));
        } else if (animProgress < 0.55f) {
            // Flash point / moment of rank-up!
            currentDisplayTier = tierAfter;
            currentDisplayRr = 0;
        } else if (animProgress < 1.0f) {
            currentDisplayTier = tierAfter;
            float p2 = (animProgress - 0.55f) / 0.45f;
            currentDisplayRr = static_cast<int>(targetRrAfter * (1.0f - std::pow(1.0f - p2, 2.0f)));
        } else {
            currentDisplayTier = tierAfter;
            currentDisplayRr = targetRrAfter;
        }
    } else if (isDemo) {
        if (animProgress < 0.45f) {
            currentDisplayTier = tierBefore;
            float p1 = animProgress / 0.45f;
            currentDisplayRr = static_cast<int>(targetRrBefore * (1.0f - p1));
        } else if (animProgress < 0.55f) {
            currentDisplayTier = tierAfter;
            currentDisplayRr = 100;
        } else if (animProgress < 1.0f) {
            currentDisplayTier = tierAfter;
            float p2 = (animProgress - 0.55f) / 0.45f;
            currentDisplayRr = static_cast<int>(100 - (100 - targetRrAfter) * (1.0f - std::pow(1.0f - p2, 2.0f)));
        } else {
            currentDisplayTier = tierAfter;
            currentDisplayRr = targetRrAfter;
        }
    } else {
        currentDisplayTier = tierAfter;
        currentDisplayRr = static_cast<int>(std::round(targetRrBefore + ease * (targetRrAfter - targetRrBefore)));
    }
    currentDisplayRr = std::max(0, std::min(100, currentDisplayRr));

    Rgb base = getRankRgb(currentDisplayTier);
    Color rankCol = Color::RGB(base.r, base.g, base.b);
    Color rankMid = Color::RGB(base.r * 6 / 10, base.g * 6 / 10, base.b * 6 / 10);
    Color rankDark = Color::RGB(base.r * 3 / 10, base.g * 3 / 10, base.b * 3 / 10);
    Color rankDeep = Color::RGB(base.r / 7, base.g / 7, base.b / 7);

    // Rank-Up special effects & pulsing
    Element promoCard = text("");
    if (isPromo) {
        if (animProgress >= 0.45f) {
            // Animated sparkling stars
            int sparklePhase = (elapsedMs / 120) % 4;
            std::string starBanner;
            if (sparklePhase == 0) starBanner = "✨ ✦ ★  ПОВЫШЕНИЕ РАНГА!  ★ ✦ ✨";
            else if (sparklePhase == 1) starBanner = "✧ ★ ✦  ПОВЫШЕНИЕ РАНГА!  ✦ ★ ✧";
            else if (sparklePhase == 2) starBanner = "✦ ✧ ★  ПОВЫШЕНИЕ РАНГА!  ★ ✧ ✦";
            else starBanner = "★ ✨ ✧  ПОВЫШЕНИЕ РАНГА!  ✧ ✨ ★";

            // Pulsing glow border
            float pulse = 0.5f + 0.5f * std::sin((float)(elapsedMs - 1200) * 0.008f);
            int pr = std::min(255, (int)(base.r * (0.85f + 0.35f * pulse)));
            int pg = std::min(255, (int)(base.g * (0.85f + 0.35f * pulse)));
            int pb = std::min(255, (int)(base.b * (0.85f + 0.35f * pulse)));
            rankCol = Color::RGB(pr, pg, pb);
            rankMid = Color::RGB(std::min(255, (int)(base.r * (0.6f + 0.25f * pulse))), std::min(255, (int)(base.g * (0.6f + 0.25f * pulse))), std::min(255, (int)(base.b * (0.6f + 0.25f * pulse))));

            std::string beforeName = getRankTierName(tierBefore);
            std::string afterName = getRankTierName(tierAfter);

            Elements promoInner;
            promoInner.push_back(text(starBanner) | bold | color(Color::RGB(255, 235, 90)) | center);
            promoInner.push_back(text(""));
            promoInner.push_back(
                hbox(Elements{
                    filler(),
                    text(" " + beforeName + " ") | bold | color(Color::GrayLight),
                    text("  ══════►  ") | bold | color(Color::YellowLight),
                    text(" " + afterName + " ") | bold | color(Color::White) | bgcolor(rankDark),
                    filler()
                })
            );

            if (isMajorPromo) {
                promoInner.push_back(text(""));
                promoInner.push_back(
                    text(" ⚡ НОВЫЙ ДИВИЗИОН: " + getDivisionName(tierAfter) + "! ⚡ ") 
                        | bold | color(Color::CyanLight) | center
                );
            }

            promoCard = vbox(std::move(promoInner)) 
                | bgcolor(Color::RGB(base.r / 6, base.g / 6, base.b / 6)) 
                | borderStyled(ftxui::DOUBLE, rankCol);
        } else {
            // Before rank up trigger: show charging up
            promoCard = text(" ★ ЗАВЕРШЕНИЕ ДИВИЗИОНА... ★ ") | bold | color(Color::RGB(255, 215, 0)) | center;
        }
    } else if (isDemo) {
        if (animProgress >= 0.45f) {
            std::string beforeName = getRankTierName(tierBefore);
            std::string afterName = getRankTierName(tierAfter);
            promoCard = vbox(Elements{
                text(" ▼ ПОНИЖЕНИЕ РАНГА ▼ ") | bold | color(Color::White) | bgcolor(Color::RGB(130, 20, 20)) | center,
                text(""),
                hbox(Elements{
                    filler(),
                    text(" " + beforeName + "  ══════►  " + afterName + " ") | bold | color(Color::RedLight),
                    filler()
                })
            }) | bgcolor(Color::RGB(30, 10, 10)) | borderLight;
        } else {
            promoCard = text(" ▼ ПОНИЖЕНИЕ РАНГА ▼ ") | bold | color(Color::White) | bgcolor(Color::RGB(120, 20, 20)) | center;
        }
    }

    // Performance bonus line
    Element perfElem = text("");
    if (info.performanceBonus > 0 && animProgress >= 0.8f) {
        perfElem = hbox(Elements{
            filler(),
            text(" ★ ЗВЕЗДНЫЙ БОНУС: +" + std::to_string(info.performanceBonus) + " RR за отличную игру! ★ ") 
                | bold | color(Color::YellowLight) | bgcolor(Color::RGB(60, 45, 10)) | borderLight,
            filler()
        });
    }

    // Progress bar string
    int barWidth = 26;
    int filled = std::max(0, std::min(barWidth, (currentDisplayRr * barWidth) / 100));
    std::string barStr = "[";
    for (int i = 0; i < filled; ++i) barStr += "█";
    for (int i = filled; i < barWidth; ++i) barStr += "░";
    barStr += "] " + std::to_string(currentDisplayRr) + " / 100 RR";

    // Animated Badge style
    Color earnedBg = (outcomeColor == Color::GreenLight) ? Color::RGB(20, 100, 30) : ((outcomeColor == Color::RedLight) ? Color::RGB(130, 20, 20) : Color::RGB(60, 60, 60));
    if (animProgress < 1.0f && rrEarned != 0) {
        // Subtle pulse while rolling
        if ((elapsedMs / 100) % 2 == 0) {
            earnedBg = (outcomeColor == Color::GreenLight) ? Color::RGB(30, 130, 40) : Color::RGB(160, 30, 30);
        }
    }

    RankDisplay rankDisplayInfo = formatRank(currentDisplayTier, currentDisplayRr);

    // Build the Card
    Elements cardElements;
    cardElements.push_back(text(""));
    cardElements.push_back(text("★ " + outcomeTitle + " ★") | bold | color(outcomeColor) | center);
    cardElements.push_back(text(""));
    cardElements.push_back(
        hbox(Elements{
            filler(),
            text("  " + rrText + "  ") | bold | color(Color::White) | bgcolor(earnedBg) | borderHeavy,
            filler()
        })
    );

    if (info.performanceBonus > 0 && animProgress >= 0.8f) {
        cardElements.push_back(text(""));
        cardElements.push_back(perfElem);
    }

    if (isPromo || isDemo) {
        cardElements.push_back(text(""));
        cardElements.push_back(promoCard);
    }

    cardElements.push_back(text(""));
    cardElements.push_back(separator());
    cardElements.push_back(text(""));
    cardElements.push_back(text("ВАШ РАНГ:") | bold | color(rankCol) | center);
    cardElements.push_back(text(" " + rankDisplayInfo.name + " ") | bold | color(Color::White) | bgcolor(rankDark) | center);
    cardElements.push_back(text(""));
    cardElements.push_back(text(barStr) | bold | color(rankCol) | center);
    cardElements.push_back(text(""));
    cardElements.push_back(separator());
    cardElements.push_back(text(""));
    cardElements.push_back(text(" Окно закроется через " + std::to_string(remaining) + " сек  (или нажмите [ESC]) ") | color(Color::GrayLight) | center);
    cardElements.push_back(text(""));

    auto coreCard = vbox(std::move(cardElements))
        | bgcolor(rankDeep)
        | color(Color::White)
        | borderStyled(ftxui::DOUBLE, rankCol)
        | size(WIDTH, EQUAL, 66);

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
