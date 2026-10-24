#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <algorithm>

#include "chat_translator.hpp"

// ValorantChatMessage defined in main.cpp / chat_types

extern std::vector<ValorantChatMessage> g_chatMessages;
extern std::mutex g_chatMutex;
extern ftxui::Box g_chatOpenBox;
extern ftxui::Box g_chatCloseBox;
extern ftxui::Box g_chatClearBox;
extern ftxui::Box g_chatChannelBox;
extern ftxui::Box g_chatTranslateToggleBox;
extern ftxui::Box g_chatSendBox;

extern std::string g_chatTargetChannel;
extern bool g_chatAutoTranslateRuToEn;
extern std::string g_chatInputText;
extern std::string g_chatStatusMessage;
extern std::chrono::steady_clock::time_point g_chatStatusTime;
extern bool g_mousePressed;

inline ftxui::Element renderChatMenu() {
    using namespace ftxui;

    std::vector<ValorantChatMessage> msgs;
    {
        std::lock_guard<std::mutex> lk(g_chatMutex);
        msgs = g_chatMessages;
    }

    Elements msgElements;

    if (msgs.empty()) {
        msgElements.push_back(
            hbox({
                filler(),
                text(" Сообщений в чате пока нет. Новые сообщения из матча появятся здесь автоматически! ")
                    | color(Color::GrayDark),
                filler()
            })
        );
    } else {
        int startIdx = std::max(0, (int)msgs.size() - 25);
        for (size_t i = startIdx; i < msgs.size(); ++i) {
            const auto& m = msgs[i];

            Color channelColor = Color::CyanLight;
            std::string channelTag = "[TEAM]";
            if (m.chatType == "all") {
                channelTag = "[ALL]";
                channelColor = Color::YellowLight;
            } else if (m.chatType == "party") {
                channelTag = "[PARTY]";
                channelColor = Color::GreenLight;
            } else if (m.chatType == "whisper") {
                channelTag = "[PM]";
                channelColor = Color::MagentaLight;
            }

            Color nameColor = m.isEnemy ? Color::RedLight : (m.senderName == "ВЫ" ? Color::YellowLight : Color::GreenLight);
            std::string enemyBadge = m.isEnemy ? " (Враг)" : "";
            std::string agentStr = m.senderAgent.empty() ? "" : ("[" + m.senderAgent + "] ");

            Elements rowParts;
            rowParts.push_back(text(channelTag + " ") | bold | color(channelColor));
            if (!agentStr.empty()) {
                rowParts.push_back(text(agentStr) | bold | color(Color::CyanLight));
            }
            rowParts.push_back(text(m.senderName + enemyBadge + ": ") | bold | color(nameColor));
            rowParts.push_back(text(m.text) | color(Color::White));

            if (!m.translated.empty()) {
                rowParts.push_back(text("  ->  " + m.translated) | italic | color(Color::YellowLight));
            }

            msgElements.push_back(
                hbox(std::move(rowParts)) | bgcolor(i % 2 == 0 ? Color::RGB(15, 20, 30) : Color::RGB(10, 14, 22))
            );
        }
    }

    auto listPane = vbox(std::move(msgElements)) | yframe | flex;

    auto backBtn = renderButton("НАЗАД (ESC)", g_chatCloseBox, Color::CyanLight, "chat_close", g_mousePressed)
                 | reflect(g_chatCloseBox);
    auto clearBtn = renderButton("ОЧИСТИТЬ ЧАТ", g_chatClearBox, Color::GrayLight, "chat_clear", g_mousePressed)
                  | reflect(g_chatClearBox);

    auto header = hbox({
        text("  ПЕРЕХВАТ И ОТПРАВКА В ЧАТ МАТЧА ") | bold | color(Color::Cyan),
        filler() | flex,
        clearBtn,
        text(" "),
        backBtn,
        text(" ")
    }) | bgcolor(Color::RGB(18, 25, 40));

    // Channel toggle button
    std::string chanLabel = (g_chatTargetChannel == "all") ? "[ Канал: ВСЕМ / ALL (Tab) ]" : "[ Канал: КОМАНДЕ / TEAM (Tab) ]";
    Color chanColor = (g_chatTargetChannel == "all") ? Color::YellowLight : Color::CyanLight;
    auto chanBtn = renderButton(chanLabel, g_chatChannelBox, chanColor, "chat_channel", g_mousePressed)
                 | reflect(g_chatChannelBox);

    // RU -> EN translation toggle button
    std::string trLabel = g_chatAutoTranslateRuToEn ? "[ RU->EN перевод: ВКЛ ]" : "[ RU->EN перевод: ВЫКЛ ]";
    Color trColor = g_chatAutoTranslateRuToEn ? Color::GreenLight : Color::GrayLight;
    auto trBtn = renderButton(trLabel, g_chatTranslateToggleBox, trColor, "chat_translate", g_mousePressed)
               | reflect(g_chatTranslateToggleBox);

    // Anti-ban filter badge
    auto antiBanBadge = text(" [Анти-бан фильтр: АКТИВЕН] ") | bold | color(Color::GreenLight) | bgcolor(Color::RGB(10, 40, 20));

    auto controlsRow = hbox({
        chanBtn,
        text("  "),
        trBtn,
        text("  "),
        antiBanBadge,
        filler() | flex
    });

    // Input line
    std::string displayInput = g_chatInputText.empty() ? "Введите текст сообщения..." : g_chatInputText;
    Color inputColor = g_chatInputText.empty() ? Color::GrayDark : Color::White;

    auto sendBtn = renderButton("ОТПРАВИТЬ (Enter)", g_chatSendBox, Color::GreenLight, "chat_send", g_mousePressed)
                 | reflect(g_chatSendBox);

    auto inputRow = hbox({
        text(" > ") | bold | color(Color::YellowLight),
        text(displayInput) | color(inputColor),
        text("_") | bold | color(Color::CyanLight),
        filler() | flex,
        sendBtn
    }) | bgcolor(Color::RGB(15, 20, 30)) | borderStyled(LIGHT, Color::GrayLight);

    // Status or hint
    Element statusRow;
    auto now = std::chrono::steady_clock::now();
    int statusAge = std::chrono::duration_cast<std::chrono::seconds>(now - g_chatStatusTime).count();
    if (!g_chatStatusMessage.empty() && statusAge < 6) {
        Color sCol = (g_chatStatusMessage.find("Ошибка") != std::string::npos || g_chatStatusMessage.find("Заблокировано") != std::string::npos)
                   ? Color::RedLight : Color::GreenLight;
        statusRow = text(" " + g_chatStatusMessage + " ") | bold | color(sCol);
    } else {
        statusRow = text(" Подсказка: Введите текст и нажмите Enter. Tab переключает канал [Команда / Все]. Маты цензурятся автоматически!")
                  | color(Color::GrayLight);
    }

    return vbox({
        header,
        separator(),
        listPane,
        separator(),
        controlsRow,
        inputRow,
        statusRow
    }) | border | flex;
}
