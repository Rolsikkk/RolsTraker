#include <windows.h>
#include <winhttp.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <chrono>
#include <thread>
#include <iomanip>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Application Version Constant
const std::string CURRENT_VERSION = "v1.0.1";
const std::string GITHUB_REPO     = "Rolsikkk/RolsTraker";

// Global Persistent Caches & Rendering Buffer
static std::map<std::string, std::pair<std::string, std::string>> g_nameCache;
static std::map<std::string, std::pair<int, int>> g_rankCache;
static std::string g_lastRenderedOutput;
static std::string g_lastPhase;
static std::string g_lastMatchId;

// -----------------------------------------------------------------------------
// UTF-8 Visual Character Length & Padding Helpers for Alignment
// -----------------------------------------------------------------------------
size_t utf8_length(const std::string& str) {
    size_t len = 0;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        len++;
    }
    return len;
}

std::string padRightUtf8(const std::string& str, size_t targetWidth) {
    size_t visLen = utf8_length(str);
    if (visLen >= targetWidth) return str;
    return str + std::string(targetWidth - visLen, ' ');
}

// -----------------------------------------------------------------------------
// Base64 Helpers
// -----------------------------------------------------------------------------
static const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[BASE64_CHARS[i]] = i;

    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// JSON Helper for Flexible Key Reading
// -----------------------------------------------------------------------------
std::string getJsonKeyStr(const json& j, const std::vector<std::string>& keys) {
    for (const auto& k : keys) {
        if (j.contains(k) && !j[k].is_null()) {
            if (j[k].is_string()) return j[k].get<std::string>();
        }
    }
    return "";
}

// -----------------------------------------------------------------------------
// HTTP Response & WinHTTP Wrapper
// -----------------------------------------------------------------------------
struct HttpResponse {
    int statusCode = 0;
    std::string body;
};

HttpResponse httpRequest(
    const std::string& method,
    const std::string& host,
    INTERNET_PORT port,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    const std::string& bodyData = "",
    bool isHttps = true,
    bool ignoreCert = false
) {
    HttpResponse response;
    HINTERNET hSession = WinHttpOpen(L"RolsTraker/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return response;

    std::wstring wHost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return response;
    }

    std::wstring wMethod(method.begin(), method.end());
    std::wstring wPath(path.begin(), path.end());
    DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    if (ignoreCert && isHttps) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::wstring wHeaders;
    for (const auto& [k, v] : headers) {
        std::wstring wk(k.begin(), k.end());
        std::wstring wv(v.begin(), v.end());
        wHeaders += wk + L": " + wv + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(),
        (DWORD)wHeaders.length(),
        bodyData.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)bodyData.c_str(),
        (DWORD)bodyData.length(),
        (DWORD)bodyData.length(),
        0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = dwStatusCode;

        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1);
            if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                response.body.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

bool downloadFileWithRedirects(const std::string& initialUrl, const std::string& outputPath) {
    std::string currentUrl = initialUrl;
    for (int redirectCount = 0; redirectCount < 5; redirectCount++) {
        std::string host, path;
        size_t protoPos = currentUrl.find("://");
        std::string urlWithoutProto = (protoPos != std::string::npos) ? currentUrl.substr(protoPos + 3) : currentUrl;
        size_t slashPos = urlWithoutProto.find('/');
        if (slashPos != std::string::npos) {
            host = urlWithoutProto.substr(0, slashPos);
            path = urlWithoutProto.substr(slashPos);
        } else {
            host = urlWithoutProto;
            path = "/";
        }

        HINTERNET hSession = WinHttpOpen(L"RolsTraker-Downloader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        std::wstring wHost(host.begin(), host.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), 443, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring wPath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring wHeaders = L"User-Agent: RolsTraker-App\r\nAccept: */*\r\n";
        BOOL bResults = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)wHeaders.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults) {
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

            if (dwStatusCode == 301 || dwStatusCode == 302 || dwStatusCode == 307 || dwStatusCode == 308) {
                wchar_t locationBuffer[2048] = {0};
                DWORD locationSize = sizeof(locationBuffer);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, locationBuffer, &locationSize, WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring wLoc(locationBuffer);
                    currentUrl = std::string(wLoc.begin(), wLoc.end());
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    continue;
                }
            }

            if (dwStatusCode == 200) {
                std::ofstream outFile(outputPath, std::ios::binary);
                if (!outFile.is_open()) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return false;
                }

                DWORD dwDownloaded = 0;
                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;
                    std::vector<char> buffer(dwSize);
                    if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                        outFile.write(buffer.data(), dwDownloaded);
                    }
                } while (dwSize > 0);

                outFile.close();
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return true;
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        break;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Auto-Updater Module
// -----------------------------------------------------------------------------
const std::string RESET         = "\033[0m";
const std::string BOLD          = "\033[1m";
const std::string COLOR_RED     = "\033[1;31m";
const std::string COLOR_BLUE    = "\033[1;34m";
const std::string COLOR_CYAN    = "\033[1;36m";
const std::string COLOR_GREEN   = "\033[1;32m";
const std::string COLOR_YELLOW  = "\033[1;33m";
const std::string COLOR_MAGENTA = "\033[1;35m";
const std::string COLOR_BR_BLUE = "\033[1;94m";
const std::string COLOR_GRAY    = "\033[90m";
const std::string COLOR_WHITE   = "\033[1;97m";

void checkAutoUpdate() {
    std::cout << COLOR_CYAN << "Проверка обновлений на GitHub (" << GITHUB_REPO << ")...\n" << RESET;

    std::map<std::string, std::string> headers = {
        {"User-Agent", "RolsTraker-App"},
        {"Accept", "application/vnd.github.v3+json"}
    };

    auto res = httpRequest("GET", "api.github.com", 443, "/repos/" + GITHUB_REPO + "/releases/latest", headers, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            std::string latestTag = getJsonKeyStr(j, {"tag_name"});
            if (!latestTag.empty() && latestTag != CURRENT_VERSION) {
                std::string downloadUrl;
                if (j.contains("assets") && j["assets"].is_array()) {
                    for (const auto& asset : j["assets"]) {
                        std::string name = getJsonKeyStr(asset, {"name"});
                        if (name == "RolsTraker.exe") {
                            downloadUrl = getJsonKeyStr(asset, {"browser_download_url"});
                            break;
                        }
                    }
                }

                if (downloadUrl.empty() && !latestTag.empty()) {
                    downloadUrl = "https://github.com/" + GITHUB_REPO + "/releases/download/" + latestTag + "/RolsTraker.exe";
                }

                std::cout << COLOR_GREEN << "\n [!] Найдено обновление! (Текущая: " << CURRENT_VERSION << ", Новая: " << latestTag << ")\n" << RESET;
                std::cout << " Скачивание обновленного файла RolsTraker.exe...\n";

                if (downloadFileWithRedirects(downloadUrl, "RolsTraker_new.exe")) {
                    std::cout << COLOR_CYAN << " Обновление успешно скачано! Перезапуск программы...\n" << RESET;

                    std::ofstream updater("updater.bat");
                    if (updater.is_open()) {
                        updater << "@echo off\n";
                        updater << "timeout /t 1 /nobreak > nul\n";
                        updater << "copy /y RolsTraker_new.exe RolsTraker.exe > nul\n";
                        updater << "del /f /q RolsTraker_new.exe > nul\n";
                        updater << "start RolsTraker.exe\n";
                        updater << "del \"%~f0\"\n";
                        updater.close();
                    }

                    WinExec("cmd /c updater.bat", SW_HIDE);
                    exit(0);
                } else {
                    std::cout << COLOR_RED << " Ошибка при скачивании файла обновления.\n" << RESET;
                }
            } else {
                std::cout << COLOR_GRAY << " У вас установлена актуальная версия (" << CURRENT_VERSION << ").\n" << RESET;
            }
        } catch (...) {}
    } else {
        std::cout << COLOR_GRAY << " Проверка обновлений завершена.\n" << RESET;
    }
}

// -----------------------------------------------------------------------------
// Data Structures & Models
// -----------------------------------------------------------------------------
struct Lockfile {
    std::string name;
    std::string pid;
    uint16_t port = 0;
    std::string password;
    std::string protocol;
    std::string lockPathUsed;
};

struct Session {
    std::string accessToken;
    std::string token;
    std::string puuid;
    std::string region;
    std::string shard;
    std::string clientVersion;
    std::string glzHost;
    std::string pdHost;
};

struct PlayerInfo {
    std::string puuid;
    std::string teamId;
    std::string characterId;
    std::string partyId;
    std::string gameName;
    std::string tagLine;
    std::string agentName;
    int rankTier = 0;
    int rankRR = 0;
};

struct MatchState {
    std::string phase; // "coregame", "pregame", "none"
    std::string matchId;
    std::string mapId;
    std::string mapName;
    std::vector<PlayerInfo> players;
};

struct RankDisplay {
    std::string name;
    std::string color;
};

RankDisplay formatRank(int tier, int rr) {
    if (tier <= 2) return {"Unrated", COLOR_GRAY};

    std::string name;
    std::string color = COLOR_WHITE;

    if (tier >= 3 && tier <= 5) {
        name = "Iron " + std::to_string(tier - 2);
        color = COLOR_GRAY;
    } else if (tier >= 6 && tier <= 8) {
        name = "Bronze " + std::to_string(tier - 5);
        color = "\033[38;2;165;113;78m";
    } else if (tier >= 9 && tier <= 11) {
        name = "Silver " + std::to_string(tier - 8);
        color = "\033[38;2;192;192;192m";
    } else if (tier >= 12 && tier <= 14) {
        name = "Gold " + std::to_string(tier - 11);
        color = COLOR_YELLOW;
    } else if (tier >= 15 && tier <= 17) {
        name = "Platinum " + std::to_string(tier - 14);
        color = COLOR_CYAN;
    } else if (tier >= 18 && tier <= 20) {
        name = "Diamond " + std::to_string(tier - 17);
        color = COLOR_MAGENTA;
    } else if (tier >= 21 && tier <= 23) {
        name = "Ascendant " + std::to_string(tier - 20);
        color = COLOR_GREEN;
    } else if (tier >= 24 && tier <= 26) {
        name = "Immortal " + std::to_string(tier - 23);
        color = COLOR_RED;
    } else if (tier >= 27) {
        name = "Radiant";
        color = "\033[1;93m";
    }

    if (rr > 0) {
        name += " (" + std::to_string(rr) + " RR)";
    }
    return {name, color};
}

// -----------------------------------------------------------------------------
// Lockfile & Client Helpers
// -----------------------------------------------------------------------------
Lockfile readLockfile() {
    Lockfile lock;
    char* localAppData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") != 0 || !localAppData) {
        return lock;
    }

    std::string baseDir = std::string(localAppData);
    free(localAppData);

    std::vector<std::string> candidatePaths = {
        baseDir + "\\Riot Games\\Riot Games\\Config\\lockfile",
        baseDir + "\\Riot Games\\Riot Client\\Config\\lockfile",
        baseDir + "\\Riot Games\\Config\\lockfile"
    };

    for (const auto& lockPath : candidatePaths) {
        std::ifstream file(lockPath);
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line)) {
                std::stringstream ss(line);
                std::vector<std::string> parts;
                std::string part;
                while (std::getline(ss, part, ':')) {
                    parts.push_back(part);
                }
                if (parts.size() >= 5) {
                    lock.name = parts[0];
                    lock.pid = parts[1];
                    lock.port = static_cast<uint16_t>(std::stoi(parts[2]));
                    lock.password = parts[3];
                    lock.protocol = parts[4];
                    lock.lockPathUsed = lockPath;
                    return lock;
                }
            }
        }
    }
    return lock;
}

std::string getClientVersion() {
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/version", {}, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            return j["data"]["riotClientVersion"].get<std::string>();
        } catch (...) {}
    }
    return "release-09.05-shipping-15-2748173";
}

std::pair<std::string, std::string> readShardFromLog() {
    char* localAppData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") == 0 && localAppData) {
        std::string logPath = std::string(localAppData) + "\\VALORANT\\Saved\\Logs\\ShooterGame.log";
        free(localAppData);

        std::ifstream file(logPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            std::regex re("glz-([a-zA-Z0-9]+)-1\\.([a-zA-Z0-9]+)\\.a\\.pvp\\.net", std::regex::icase);
            std::smatch match;
            if (std::regex_search(content, match, re) && match.size() >= 3) {
                std::string region = match[1].str();
                std::string shard = match[2].str();
                for (auto& c : region) c = tolower(c);
                for (auto& c : shard) c = tolower(c);
                return {region, shard};
            }
        }
    }
    return {"", ""};
}

Session getSession(const Lockfile& lock) {
    Session s;
    std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);
    
    // Entitlements
    auto entRes = httpRequest("GET", "127.0.0.1", lock.port, "/entitlements/v1/token", {{"Authorization", authHeader}}, "", true, true);
    if (entRes.statusCode != 200) return s;

    try {
        auto j = json::parse(entRes.body);
        s.accessToken = getJsonKeyStr(j, {"accessToken"});
        s.token = getJsonKeyStr(j, {"token"});
        s.puuid = getJsonKeyStr(j, {"subject", "puuid"});
    } catch (...) {
        return s;
    }

    // Region & Shard
    auto logShard = readShardFromLog();
    if (!logShard.first.empty()) {
        s.region = logShard.first;
        s.shard = logShard.second;
    } else {
        auto regRes = httpRequest("GET", "127.0.0.1", lock.port, "/riotclient/region-locale", {{"Authorization", authHeader}}, "", true, true);
        if (regRes.statusCode == 200) {
            try {
                auto j = json::parse(regRes.body);
                s.region = getJsonKeyStr(j, {"region"});
                for (auto& c : s.region) c = tolower(c);
                if (s.region == "latam" || s.region == "br") s.shard = "na";
                else s.shard = s.region;
            } catch (...) {}
        }
    }

    if (s.region.empty()) s.region = "eu";
    if (s.shard.empty()) s.shard = "eu";

    s.clientVersion = getClientVersion();
    s.glzHost = "glz-" + s.region + "-1." + s.shard + ".a.pvp.net";
    s.pdHost = "pd." + s.shard + ".a.pvp.net";

    return s;
}

// -----------------------------------------------------------------------------
// Valorant Metadata API (Agents & Maps)
// -----------------------------------------------------------------------------
std::map<std::string, std::string> getAgentMap() {
    std::map<std::string, std::string> agents;
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/agents?isPlayableCharacter=true", {}, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            for (const auto& item : j["data"]) {
                agents[item["uuid"].get<std::string>()] = item["displayName"].get<std::string>();
            }
        } catch (...) {}
    }
    return agents;
}

std::map<std::string, std::string> getMapNameMap() {
    std::map<std::string, std::string> maps;
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/maps", {}, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            for (const auto& item : j["data"]) {
                if (item.contains("mapUrl") && !item["mapUrl"].is_null()) {
                    maps[item["mapUrl"].get<std::string>()] = item["displayName"].get<std::string>();
                }
            }
        } catch (...) {}
    }
    return maps;
}

// -----------------------------------------------------------------------------
// Live Match & Party Logic
// -----------------------------------------------------------------------------
std::string getClientPlatformBase64() {
    json j = {
        {"platformType", "PC"},
        {"platformOS", "Windows"},
        {"platformOSVersion", "10.0.19042.1.256.64bit"},
        {"platformChipset", "Unknown"}
    };
    return base64_encode(j.dump());
}

std::map<std::string, std::string> getPresencesPartyMap(const Lockfile& lock) {
    std::map<std::string, std::string> puuidToParty;
    std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);
    auto res = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v4/presences", {{"Authorization", authHeader}}, "", true, true);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            if (j.contains("presences") && j["presences"].is_array()) {
                for (const auto& p : j["presences"]) {
                    std::string puuid = getJsonKeyStr(p, {"puuid"});
                    std::string privB64 = getJsonKeyStr(p, {"private"});
                    if (puuid.empty() || privB64.empty()) continue;
                    std::string privJsonStr = base64_decode(privB64);
                    try {
                        auto privJ = json::parse(privJsonStr);
                        std::string partyId = getJsonKeyStr(privJ, {"partyId", "partyID", "party_id"});
                        if (!partyId.empty()) {
                            puuidToParty[puuid] = partyId;
                        }
                    } catch (...) {}
                }
            }
        } catch (...) {}
    }
    return puuidToParty;
}

MatchState getLiveMatchState(const Session& session, const Lockfile& lock) {
    MatchState state;
    state.phase = "none";

    std::string clientPlatform = getClientPlatformBase64();
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + session.accessToken},
        {"X-Riot-Entitlements-JWT", session.token},
        {"X-Riot-ClientPlatform", clientPlatform},
        {"X-Riot-ClientVersion", session.clientVersion}
    };

    // 1. Core-Game check
    auto corePlayerRes = httpRequest("GET", session.glzHost, 443, "/core-game/v1/players/" + session.puuid, headers, "", true, false);
    if (corePlayerRes.statusCode == 200) {
        try {
            auto jPlayer = json::parse(corePlayerRes.body);
            std::string matchId = getJsonKeyStr(jPlayer, {"MatchID", "matchId"});

            auto coreMatchRes = httpRequest("GET", session.glzHost, 443, "/core-game/v1/matches/" + matchId, headers, "", true, false);
            if (coreMatchRes.statusCode == 200) {
                auto jMatch = json::parse(coreMatchRes.body);
                state.phase = "coregame";
                state.matchId = matchId;
                state.mapId = getJsonKeyStr(jMatch, {"MapID", "mapId"});

                if (jMatch.contains("Players") && jMatch["Players"].is_array()) {
                    for (const auto& p : jMatch["Players"]) {
                        PlayerInfo info;
                        info.puuid = getJsonKeyStr(p, {"Subject", "subject", "puuid"});
                        info.teamId = getJsonKeyStr(p, {"TeamID", "teamId"});
                        info.characterId = getJsonKeyStr(p, {"CharacterID", "characterId"});
                        info.partyId = getJsonKeyStr(p, {"PartyID", "partyId"});
                        state.players.push_back(info);
                    }
                }
                return state;
            }
        } catch (...) {}
    }

    // 2. Pregame check
    auto prePlayerRes = httpRequest("GET", session.glzHost, 443, "/pregame/v1/players/" + session.puuid, headers, "", true, false);
    if (prePlayerRes.statusCode == 200) {
        try {
            auto jPlayer = json::parse(prePlayerRes.body);
            std::string matchId = getJsonKeyStr(jPlayer, {"MatchID", "matchId"});

            auto preMatchRes = httpRequest("GET", session.glzHost, 443, "/pregame/v1/matches/" + matchId, headers, "", true, false);
            if (preMatchRes.statusCode == 200) {
                auto jMatch = json::parse(preMatchRes.body);
                state.phase = "pregame";
                state.matchId = matchId;
                state.mapId = getJsonKeyStr(jMatch, {"MapID", "mapId"});

                if (jMatch.contains("Teams") && jMatch["Teams"].is_array()) {
                    for (const auto& team : jMatch["Teams"]) {
                        std::string tId = getJsonKeyStr(team, {"TeamID", "teamId"});
                        if (tId.empty()) tId = "Ally";
                        if (team.contains("Players") && team["Players"].is_array()) {
                            for (const auto& p : team["Players"]) {
                                PlayerInfo info;
                                info.puuid = getJsonKeyStr(p, {"Subject", "subject", "puuid"});
                                info.teamId = tId;
                                info.characterId = getJsonKeyStr(p, {"CharacterID", "characterId"});
                                info.partyId = getJsonKeyStr(p, {"PartyID", "partyId"});
                                state.players.push_back(info);
                            }
                        }
                    }
                } else if (jMatch.contains("AllyTeam") && jMatch["AllyTeam"].contains("Players")) {
                    for (const auto& p : jMatch["AllyTeam"]["Players"]) {
                        PlayerInfo info;
                        info.puuid = getJsonKeyStr(p, {"Subject", "subject", "puuid"});
                        info.teamId = "Ally";
                        info.characterId = getJsonKeyStr(p, {"CharacterID", "characterId"});
                        info.partyId = getJsonKeyStr(p, {"PartyID", "partyId"});
                        state.players.push_back(info);
                    }
                }
                return state;
            }
        } catch (...) {}
    }

    return state;
}

void resolveDisplayNamesAndRanks(const Session& session, std::vector<PlayerInfo>& players) {
    if (players.empty()) return;

    // Apply cached names and ranks first
    std::vector<std::string> uncachedNamesPuuids;
    for (auto& p : players) {
        if (g_nameCache.count(p.puuid)) {
            p.gameName = g_nameCache[p.puuid].first;
            p.tagLine = g_nameCache[p.puuid].second;
        } else {
            uncachedNamesPuuids.push_back(p.puuid);
        }

        if (g_rankCache.count(p.puuid)) {
            p.rankTier = g_rankCache[p.puuid].first;
            p.rankRR = g_rankCache[p.puuid].second;
        }
    }

    std::string clientPlatform = getClientPlatformBase64();
    std::map<std::string, std::string> pdHeaders = {
        {"Authorization", "Bearer " + session.accessToken},
        {"X-Riot-Entitlements-JWT", session.token},
        {"X-Riot-ClientPlatform", clientPlatform},
        {"X-Riot-ClientVersion", session.clientVersion}
    };

    // 1. Resolve uncached display names
    if (!uncachedNamesPuuids.empty()) {
        json bodyArray = json::array();
        for (const auto& id : uncachedNamesPuuids) bodyArray.push_back(id);

        std::map<std::string, std::string> nameHeaders = pdHeaders;
        nameHeaders["Content-Type"] = "application/json";

        auto nameRes = httpRequest("PUT", session.pdHost, 443, "/name-service/v2/players", nameHeaders, bodyArray.dump(), true, false);
        if (nameRes.statusCode == 200) {
            try {
                auto j = json::parse(nameRes.body);
                for (const auto& item : j) {
                    std::string subject = getJsonKeyStr(item, {"Subject", "subject"});
                    std::string gameName = getJsonKeyStr(item, {"GameName", "gameName"});
                    std::string tagLine = getJsonKeyStr(item, {"TagLine", "tagLine"});
                    if (gameName.empty()) gameName = "Player";
                    g_nameCache[subject] = {gameName, tagLine};
                }
            } catch (...) {}
        }
        for (auto& p : players) {
            if (g_nameCache.count(p.puuid)) {
                p.gameName = g_nameCache[p.puuid].first;
                p.tagLine = g_nameCache[p.puuid].second;
            } else if (p.gameName.empty()) {
                p.gameName = "Player";
                p.tagLine = "VAL";
            }
        }
    }

    // 2. Resolve uncached ranks (MMR)
    for (auto& p : players) {
        if (g_rankCache.count(p.puuid)) {
            p.rankTier = g_rankCache[p.puuid].first;
            p.rankRR = g_rankCache[p.puuid].second;
            continue;
        }

        auto mmrRes = httpRequest("GET", session.pdHost, 443, "/mmr/v1/players/" + p.puuid, pdHeaders, "", true, false);
        if (mmrRes.statusCode == 200) {
            int tier = 0;
            int rr = 0;
            try {
                auto j = json::parse(mmrRes.body);

                // Path A: LatestCompetitiveUpdate
                if (j.contains("LatestCompetitiveUpdate") && !j["LatestCompetitiveUpdate"].is_null()) {
                    auto lcu = j["LatestCompetitiveUpdate"];
                    if (lcu.contains("TierAfterUpdate") && !lcu["TierAfterUpdate"].is_null()) {
                        tier = lcu["TierAfterUpdate"].get<int>();
                    }
                    if (lcu.contains("RankedRatingAfterUpdate") && !lcu["RankedRatingAfterUpdate"].is_null()) {
                        rr = lcu["RankedRatingAfterUpdate"].get<int>();
                    }
                }

                // Path B: QueueSkills.competitive.SeasonalInfoBySeasonID
                if (j.contains("QueueSkills") && j["QueueSkills"].contains("competitive")) {
                    auto comp = j["QueueSkills"]["competitive"];

                    std::string latestSeasonId;
                    if (j.contains("LatestCompetitiveUpdate") && !j["LatestCompetitiveUpdate"].is_null()) {
                        latestSeasonId = getJsonKeyStr(j["LatestCompetitiveUpdate"], {"SeasonID"});
                    }

                    if (comp.contains("SeasonalInfoBySeasonID") && comp["SeasonalInfoBySeasonID"].is_object()) {
                        auto seasons = comp["SeasonalInfoBySeasonID"];

                        if (!latestSeasonId.empty() && seasons.contains(latestSeasonId)) {
                            auto sData = seasons[latestSeasonId];
                            if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                int t = sData["CompetitiveTier"].get<int>();
                                if (t > 0) {
                                    tier = t;
                                    if (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) {
                                        rr = sData["RankedRating"].get<int>();
                                    }
                                }
                            }
                        }

                        if (tier == 0) {
                            for (const auto& [sId, sData] : seasons.items()) {
                                if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                    int t = sData["CompetitiveTier"].get<int>();
                                    int r = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                    if (t > 0) {
                                        tier = t;
                                        rr = r;
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (...) {}

            g_rankCache[p.puuid] = {tier, rr};
            p.rankTier = tier;
            p.rankRR = rr;
        }
    }
}

// -----------------------------------------------------------------------------
// UI Rendering Engine (ANSI Colors & Anti-Flicker Buffer)
// -----------------------------------------------------------------------------
void enableVTMode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
    SetConsoleOutputCP(CP_UTF8);
}

struct PartyColorInfo {
    int groupIndex;
    std::string ansiColor;
    std::string name;
};

// Available vibrant colors for premade parties
const std::vector<std::pair<std::string, std::string>> PARTY_COLORS = {
    {COLOR_GREEN,   "Зеленый"},
    {COLOR_CYAN,    "Голубой"},
    {COLOR_YELLOW,  "Желтый"},
    {COLOR_MAGENTA, "Пурпурный"},
    {COLOR_RED,     "Красный"},
    {COLOR_BR_BLUE, "Синий"}
};

void renderConsole(const MatchState& state, const Session& session, const Lockfile& lock, const std::map<std::string, std::string>& agentMap, const std::map<std::string, std::string>& mapNameMap) {
    std::ostringstream ss;

    ss << BOLD << COLOR_CYAN << "==================================================================================================\n";
    ss << "                                     ROLSTRAKER (" << CURRENT_VERSION << ")\n";
    ss << "==================================================================================================\n" << RESET;

    if (lock.port == 0) {
        ss << COLOR_RED << "\n [!] Riot Client не запущен! Ожидание запуска игры / Riot Client...\n" << RESET;
        ss << COLOR_GRAY << " Автоматическая проверка каждые 3 секунды. Нажмите [Q] для выхода.\n" << RESET;
    } else if (state.phase == "none") {
        ss << COLOR_YELLOW << "\n [i] Riot Client подключен (Порт: " << lock.port << "). Ожидание матча / выбора агента...\n" << RESET;
        ss << COLOR_GRAY << " Region: " << session.region << " | Shard: " << session.shard << RESET << "\n";
        ss << COLOR_GRAY << " Нажмите [R] для принудительного обновления | [Q] для выхода.\n" << RESET;
    } else {
        // Party counts & Group Assignment
        std::map<std::string, int> partyCounts;
        for (const auto& p : state.players) {
            if (!p.partyId.empty()) partyCounts[p.partyId]++;
        }

        std::map<std::string, PartyColorInfo> partyGroupMap;
        int nextGroupIdx = 1;

        for (const auto& p : state.players) {
            if (!p.partyId.empty() && partyCounts[p.partyId] > 1 && partyGroupMap.find(p.partyId) == partyGroupMap.end()) {
                int colorIdx = (nextGroupIdx - 1) % PARTY_COLORS.size();
                partyGroupMap[p.partyId] = {nextGroupIdx, PARTY_COLORS[colorIdx].first, PARTY_COLORS[colorIdx].second};
                nextGroupIdx++;
            }
        }

        // Match Header Info
        std::string displayMapName = mapNameMap.count(state.mapId) ? mapNameMap.at(state.mapId) : state.mapId;
        std::string phaseStr = (state.phase == "coregame") ? "В ИГРЕ (Core Game)" : "ВЫБОР АГЕНТА (Agent Select)";

        ss << BOLD << COLOR_WHITE << " Режим: " << RESET << COLOR_GREEN << phaseStr << RESET;
        if (!displayMapName.empty()) {
            ss << BOLD << COLOR_WHITE << " | Карта: " << RESET << COLOR_YELLOW << displayMapName << RESET;
        }
        ss << BOLD << COLOR_WHITE << " | Сервер: " << RESET << COLOR_CYAN << session.region << RESET << "\n";
        ss << COLOR_GRAY << "--------------------------------------------------------------------------------------------------\n" << RESET;

        // Divide into Team 1 and Team 2
        std::vector<PlayerInfo> team1, team2;
        for (const auto& p : state.players) {
            if (p.teamId == "Red" || p.teamId == "Defender") {
                team1.push_back(p);
            } else if (p.teamId == "Blue" || p.teamId == "Attacker") {
                team2.push_back(p);
            } else {
                if (team1.size() < 5) team1.push_back(p);
                else team2.push_back(p);
            }
        }

        auto printTeamTable = [&](const std::string& teamTitle, const std::string& titleColor, const std::vector<PlayerInfo>& team) {
            ss << BOLD << titleColor << "\n " << teamTitle << RESET << "\n";
            ss << COLOR_GRAY << " --------------------------------------------------------------------------------------------------\n" << RESET;
            ss << BOLD << "  Маркер  Игрок (Ник#Тег)            Агент          Ранг                    Пати\n" << RESET;
            ss << COLOR_GRAY << " --------------------------------------------------------------------------------------------------\n" << RESET;

            for (const auto& p : team) {
                std::string fullName = p.gameName + (p.tagLine.empty() ? "" : "#" + p.tagLine);
                if (utf8_length(fullName) > 24) {
                    std::string trimmed;
                    size_t cnt = 0;
                    for (size_t i = 0; i < fullName.length() && cnt < 21; ) {
                        unsigned char c = fullName[i];
                        size_t charLen = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
                        trimmed += fullName.substr(i, charLen);
                        i += charLen;
                        cnt++;
                    }
                    fullName = trimmed + "...";
                }

                std::string agent = agentMap.count(p.characterId) ? agentMap.at(p.characterId) : (p.characterId.empty() ? "Выбирает..." : "Агент");
                if (utf8_length(agent) > 14) agent = agent.substr(0, 11) + "...";

                RankDisplay rankInfo = formatRank(p.rankTier, p.rankRR);
                std::string rankStr = rankInfo.name;
                if (utf8_length(rankStr) > 22) rankStr = rankStr.substr(0, 19) + "...";

                std::string markStr;
                std::string partyStatus;

                if (!p.partyId.empty() && partyGroupMap.count(p.partyId)) {
                    auto pInfo = partyGroupMap[p.partyId];
                    markStr = pInfo.ansiColor + " [●] " + RESET;
                    partyStatus = pInfo.ansiColor + "Пати #" + std::to_string(pInfo.groupIndex) + " (" + pInfo.name + ")" + RESET;
                } else {
                    markStr = COLOR_GRAY + " [○] " + RESET;
                    partyStatus = COLOR_GRAY + "Соло" + RESET;
                }

                ss << markStr << " "
                   << padRightUtf8(fullName, 26) << " "
                   << padRightUtf8(agent, 14) << " "
                   << rankInfo.color << padRightUtf8(rankStr, 23) << RESET << " "
                   << partyStatus << "\n";
            }
        };

        printTeamTable("[КОМАНДА 1 / ЗАЩИТНИКИ (RED)]", COLOR_RED, team1);
        printTeamTable("[КОМАНДА 2 / АТАКУЮЩИЕ (BLUE)]", COLOR_BLUE, team2);

        // Party Summary Section
        ss << COLOR_GRAY << "\n--------------------------------------------------------------------------------------------------\n" << RESET;
        ss << BOLD << COLOR_WHITE << " СВОДКА ГРУПП (PARTY SUMMARY):\n" << RESET;

        if (partyGroupMap.empty()) {
            ss << COLOR_GRAY << "  • Все игроки играют СОЛО (премейд группы не обнаружены)\n" << RESET;
        } else {
            std::map<int, std::vector<std::string>> groupMembers;
            for (const auto& p : state.players) {
                if (!p.partyId.empty() && partyGroupMap.count(p.partyId)) {
                    int gIdx = partyGroupMap[p.partyId].groupIndex;
                    std::string fullName = p.gameName + (p.tagLine.empty() ? "" : "#" + p.tagLine);
                    groupMembers[gIdx].push_back(fullName);
                }
            }

            for (const auto& [gIdx, members] : groupMembers) {
                std::string colorCode;
                for (const auto& [pid, pInfo] : partyGroupMap) {
                    if (pInfo.groupIndex == gIdx) {
                        colorCode = pInfo.ansiColor;
                        break;
                    }
                }

                ss << colorCode << "  • Пати #" << gIdx << " (" << members.size() << " чел.): " << RESET;
                for (size_t i = 0; i < members.size(); i++) {
                    ss << members[i] << (i + 1 < members.size() ? ", " : "");
                }
                ss << "\n";
            }
        }

        ss << COLOR_GRAY << "--------------------------------------------------------------------------------------------------\n" << RESET;
        ss << COLOR_GRAY << " Нажмите [R] для обновления | [Q] для выхода\n" << RESET;
        ss << BOLD << COLOR_CYAN << "==================================================================================================\n" << RESET;
    }

    std::string currentOutput = ss.str();

    // IF NOTHING HAS CHANGED, DO NOT TOUCH CONSOLE AT ALL! ZERO FLICKER!
    if (currentOutput == g_lastRenderedOutput) {
        return;
    }

    // Handle Match ID / Phase change transitions cleanly
    if (state.matchId != g_lastMatchId || state.phase != g_lastPhase) {
        g_lastMatchId = state.matchId;
        g_lastPhase = state.phase;
        std::cout << "\033[2J\033[H";
    }

    g_lastRenderedOutput = currentOutput;

    // Reset cursor to top left
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = {0, 0};
    SetConsoleCursorPosition(hOut, pos);

    // Print current output followed by \033[J (erases any leftover lines from previous screen)
    std::cout << currentOutput << "\033[J" << std::flush;
}

// -----------------------------------------------------------------------------
// Main Loop
// -----------------------------------------------------------------------------
int main() {
    enableVTMode();

    std::cout << BOLD << COLOR_CYAN << "Запуск RolsTraker (" << CURRENT_VERSION << ")...\n" << RESET;
    checkAutoUpdate();

    std::cout << "Загрузка метаданных агентов и карт Valorant...\n";
    auto agentMap = getAgentMap();
    auto mapNameMap = getMapNameMap();

    std::cout << "Метаданные успешно загружены!\n";

    Lockfile lock;
    Session session;
    MatchState matchState;

    bool forceRefresh = true;
    auto lastCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        bool timeToRefresh = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 3;

        if (timeToRefresh || forceRefresh) {
            forceRefresh = false;
            lastCheck = now;

            lock = readLockfile();
            if (lock.port != 0) {
                session = getSession(lock);
                if (!session.accessToken.empty()) {
                    matchState = getLiveMatchState(session, lock);
                    if (matchState.phase != "none") {
                        // Merge presence party info if match party IDs are incomplete
                        auto presencePartyMap = getPresencesPartyMap(lock);
                        for (auto& p : matchState.players) {
                            if (p.partyId.empty() && presencePartyMap.count(p.puuid)) {
                                p.partyId = presencePartyMap[p.puuid];
                            }
                        }
                        resolveDisplayNamesAndRanks(session, matchState.players);
                    }
                }
            } else {
                matchState = MatchState();
            }

            renderConsole(matchState, session, lock, agentMap, mapNameMap);
        }

        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q' || ch == 27) { // Q or ESC
                std::cout << "\nЗавершение работы RolsTraker...\n";
                break;
            } else if (ch == 'r' || ch == 'R') {
                forceRefresh = true;
                g_lastRenderedOutput.clear();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}
