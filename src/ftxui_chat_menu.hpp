#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>

#include "chat_translator.hpp"

// ValorantChatMessage defined in main.cpp / chat_types

extern std::vector<ValorantChatMessage> g_chatMessages;
extern std::mutex g_chatMutex;
extern ftxui::Box g_chatOpenBox;
extern ftxui::Box g_chatCloseBox;
extern ftxui::Box g_chatClearBox;
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
        // Show up to the last 25 messages
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

            Color nameColor = m.isEnemy ? Color::RedLight : Color::GreenLight;
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

    auto backBtn = renderButton("НАЗАД (ESC / C)", g_chatCloseBox, Color::CyanLight, "chat_close", g_mousePressed)
                 | reflect(g_chatCloseBox);
    auto clearBtn = renderButton("ОЧИСТИТЬ ЧАТ", g_chatClearBox, Color::GrayLight, "chat_clear", g_mousePressed)
                  | reflect(g_chatClearBox);

    auto header = hbox({
        text("  ПЕРЕХВАТ ЧАТА МАТЧА + АВТОПЕРЕВОД НА РУССКИЙ ") | bold | color(Color::Cyan),
        filler() | flex,
        text("Автоперевод англ. фраз: ВКЛ ") | bold | color(Color::GreenLight),
        text(" "),
        clearBtn,
        text(" "),
        backBtn,
        text(" ")
    }) | bgcolor(Color::RGB(18, 25, 40));

    auto hint = hbox({
        text("  Подсказка: отображает сообщения союзников и общий чат с противниками. Английский сленг переводится сразу на русский.")
            | color(Color::GrayLight)
    });

    return vbox({
        header,
        separator(),
        listPane,
        separator(),
        hint
    }) | border | flex;
}
