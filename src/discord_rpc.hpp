#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static HANDLE hPipe = INVALID_HANDLE_VALUE;

inline void initDiscordRpc(std::string clientId) {
    if (hPipe != INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < 10; ++i) {
        std::string pipeName = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
        hPipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break;
    }
    if (hPipe == INVALID_HANDLE_VALUE) return;
    
    json handshake = { {"v", 1}, {"client_id", clientId} };
    std::string payload = handshake.dump();
    
    struct { uint32_t opcode; uint32_t length; } header = {0, (uint32_t)payload.length()};
    
    DWORD written;
    WriteFile(hPipe, &header, sizeof(header), &written, NULL);
    WriteFile(hPipe, payload.c_str(), (DWORD)payload.length(), &written, NULL);
}

inline void updateDiscordPresence(std::string state, std::string details) {
    if (hPipe == INVALID_HANDLE_VALUE) {
        extern std::string g_discordClientId;
        if (!g_discordClientId.empty()) {
            initDiscordRpc(g_discordClientId);
        }
        if (hPipe == INVALID_HANDLE_VALUE) return;
    }
    
    static time_t s_discordStartTime = 0;
    static std::string s_lastDiscordState = "";
    if (s_lastDiscordState != state || s_discordStartTime == 0) {
        s_discordStartTime = time(0);
        s_lastDiscordState = state;
    }

    json activity = {
        {"cmd", "SET_ACTIVITY"},
        {"args", {
            {"pid", GetCurrentProcessId()},
            {"activity", {
                {"state", state},
                {"details", details},
                {"timestamps", {{"start", s_discordStartTime}}},
                {"assets", {{"large_image", "valorant"}, {"large_text", "RolsTraker"}}}
            }}
        }},
        {"nonce", "1"}
    };
    std::string payload = activity.dump();
    struct { uint32_t opcode; uint32_t length; } header = {1, (uint32_t)payload.length()};
    DWORD written = 0;
    if (!WriteFile(hPipe, &header, sizeof(header), &written, NULL) ||
        !WriteFile(hPipe, payload.c_str(), (DWORD)payload.length(), &written, NULL)) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
}
