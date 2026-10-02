#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
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
#include <mutex>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include <nlohmann/json.hpp>

// FTXUI Headers
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

using json = nlohmann::json;

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Application Version Constant
const std::string CURRENT_VERSION = "v2.0.8";
const std::string GITHUB_REPO     = "Rolsikkk/RolsTraker";

struct RecentMatch {
    std::string matchId;
    std::string characterId;
    std::string queueId;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int score = 0;
    int roundsWon = 0;
    int roundsLost = 0;
    bool won = false;
};

struct PlayerStats {
    float kdRatio = -1.0f;
    int headshots = 0;
    int bodyshots = 0;
    int legshots = 0;
    int totalScore = 0;
    int totalRounds = 0;
    int matchesWon = 0;
    int matchesPlayed = 0;
    std::map<std::string, int> agentPlays;
    std::vector<RecentMatch> recentMatches;
};

struct PlayerMatchStat {
    std::string puuid;
    std::string characterId;
    std::string teamId;
    int kills = 0, deaths = 0, assists = 0, score = 0;
};

struct MatchScoreboard {
    std::string matchId;
    std::string queueId;
    int roundsRed = 0;
    int roundsBlue = 0;
    std::vector<PlayerMatchStat> players;
};

struct MatchDetailsCacheEntry {
    std::string queueId;
    int kills;
    int deaths;
    int headshots;
    int bodyshots;
    int legshots;
    std::string characterId;
    int assists;
    int score;
    int roundsWon;
    int roundsLost;
    bool won;
};

static std::map<std::string, MatchScoreboard> g_matchScoreboards;
struct RankCacheEntry {
    int tier;
    int rr;
    int wins;
    int losses;
    int peakTier;
};
static std::map<std::string, std::pair<std::string, std::string>> g_nameCache;
static std::map<std::string, RankCacheEntry> g_rankCache;
static std::map<std::string, PlayerStats> g_statsCache;
static std::set<std::string> g_statsFetching;
static std::map<std::string, std::map<std::string, MatchDetailsCacheEntry>> g_matchDetailsCache;
static std::mutex g_mutex;
static std::atomic<bool> g_running{true};
static std::string g_updateStatus; // "" = idle, "checking" = checking, "downloading" = downloading, "done" = restarting
static std::mutex g_updateMutex;
static std::mutex g_logMutex;

std::string getExeDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) + "\\" : "";
}

void Log(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    std::string logPath = getExeDir() + "rolstraker_debug.log";
    std::ofstream out(logPath, std::ios_base::app);
    if (out.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        out << std::ctime(&now_time) << ": " << msg << "\n";
    }
}

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
// UI State & Bounding Boxes
// -----------------------------------------------------------------------------
static int g_statsMatchOffset = 0;
static std::vector<ftxui::Box> g_matchBoxes;
ftxui::Box g_webUrlBox;
bool g_copiedLink = false;

// -----------------------------------------------------------------------------
// Clipboard Helper
// -----------------------------------------------------------------------------
void copyToClipboard(const std::string& text) {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
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

static HINTERNET g_hSession = nullptr;
static std::mutex g_httpMutex;

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

    if (!g_hSession) {
        std::lock_guard<std::mutex> lk(g_httpMutex);
        if (!g_hSession) {
            g_hSession = WinHttpOpen(L"RolsTraker/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        }
    }
    if (!g_hSession) return response;

    std::wstring wHost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(g_hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        return response;
    }

    std::wstring wMethod(method.begin(), method.end());
    std::wstring wPath(path.begin(), path.end());
    DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
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
// Auto-Updater Module (runs in background thread)
// -----------------------------------------------------------------------------
void checkAutoUpdate() {
    { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = "checking"; }

    std::map<std::string, std::string> headers = {
        {"User-Agent", "RolsTraker-App"},
        {"Accept", "application/vnd.github.v3+json"}
    };

    auto res = httpRequest("GET", "api.github.com", 443, "/repos/" + GITHUB_REPO + "/releases/latest", headers, "", true, false);
    if (res.statusCode != 200) {
        { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = ""; }
        return;
    }

    try {
        auto j = json::parse(res.body);
        std::string latestTag = getJsonKeyStr(j, {"tag_name"});
        if (latestTag.empty() || latestTag == CURRENT_VERSION) {
            { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = ""; }
            return;
        }

        // Found a new version
        std::string downloadUrl;
        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                if (getJsonKeyStr(asset, {"name"}) == "RolsTraker.exe") {
                    downloadUrl = getJsonKeyStr(asset, {"browser_download_url"});
                    break;
                }
            }
        }
        if (downloadUrl.empty()) {
            downloadUrl = "https://github.com/" + GITHUB_REPO + "/releases/download/" + latestTag + "/RolsTraker.exe";
        }

        { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = "downloading:" + latestTag; }

        if (downloadFileWithRedirects(downloadUrl, "RolsTraker_new.exe")) {
            { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = "restarting"; }

            std::ofstream updater("updater.bat");
            if (updater.is_open()) {
                updater << "@echo off\n";
                updater << "timeout /t 1 /nobreak > nul\n";
                updater << ":retry\n";
                updater << "move /y RolsTraker.exe RolsTraker_old.exe > nul 2>&1\n";
                updater << "copy /y RolsTraker_new.exe RolsTraker.exe > nul 2>&1\n";
                updater << "if not exist RolsTraker.exe (\n";
                updater << "    timeout /t 1 /nobreak > nul\n";
                updater << "    goto retry\n";
                updater << ")\n";
                updater << "del /f /q RolsTraker_new.exe > nul 2>&1\n";
                updater << "del /f /q RolsTraker_old.exe > nul 2>&1\n";
                updater << "start \"\" RolsTraker.exe\n";
                updater << "del \"%~f0\"\n";
                updater.close();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(800)); // Let user see "restarting"
            WinExec("cmd /c updater.bat", SW_HIDE);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            exit(0);
        } else {
            { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = ""; } // Download failed, continue normally
        }
    } catch (...) {
        { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = ""; }
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
    int peakRankTier = 0;
    float kdRatio = -1.0f;
    int wins = -1;
    int losses = -1;
    int headshots = 0;
    int bodyshots = 0;
    int legshots = 0;
    int totalScore = 0;
    int totalRounds = 0;
    int matchesWon = 0;
    int matchesPlayed = 0;
    bool isLoading = false;
    std::map<std::string, int> agentPlays;
    std::vector<RecentMatch> recentMatches;
};

// Global view state
enum class AppView { MAIN, PLAYER_STATS, MATCH_SCOREBOARD };
static AppView g_currentView = AppView::MAIN;
static std::string g_selectedMatchId = "";
static std::string g_selectedPuuid = "";
static PlayerInfo g_selectedPlayerInfo;
static std::map<std::string, ftxui::Box> g_playerBoxes;
static ftxui::Box g_myStatsBox;
static int g_mouseX = -1;
static int g_mouseY = -1;

struct MatchState {
    std::string phase; // "coregame", "pregame", "none"
    std::string matchId;
    std::string mapId;
    std::string mapName;
    std::vector<PlayerInfo> players;
};

static std::string g_publicWebUrl = "Инициализация сервера...";
static std::mutex g_webUrlMutex;
static MatchState g_liveMatchState;
static std::map<std::string, std::string> g_globalAgentMap;

struct RankDisplay {
    std::string name;
    std::string color;
};

const std::string COLOR_RED     = "\033[1;31m";
const std::string COLOR_BLUE    = "\033[1;34m";
const std::string COLOR_CYAN    = "\033[1;36m";
const std::string COLOR_GREEN   = "\033[1;32m";
const std::string COLOR_YELLOW  = "\033[1;33m";
const std::string COLOR_MAGENTA = "\033[1;35m";
const std::string COLOR_GRAY    = "\033[90m";
const std::string COLOR_WHITE   = "\033[1;97m";

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
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/agents", {}, "", true, false);
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

// -----------------------------------------------------------------------------
// Fetch K/D + W/L from match history + competitive updates (fire-and-forget)
// -----------------------------------------------------------------------------
void fetchPlayerStats(Session sess, std::string puuid, std::map<std::string, std::string> pdH) {
    if (!g_running) return;

    // === K/D + Advanced Stats: from match list + match details ===
    auto mlr = httpRequest("GET", sess.pdHost, 443, "/match-history/v1/history/" + puuid + "?queue=competitive&startIndex=0&endIndex=20", pdH, "", true, false);
    int kills = 0, deaths = 0, hs = 0, bs = 0, ls = 0;
    int totalScore = 0, totalRounds = 0, matchesWon = 0, matchesPlayed = 0;
    std::map<std::string, int> agentPlays;
    std::vector<RecentMatch> recentMatches;
    bool kdOk = false;
    
    if (mlr.statusCode == 200) {
        try {
            auto jm = json::parse(mlr.body);
            if (jm.contains("History") && jm["History"].is_array()) {
                int count = 0;
                for (auto& hm : jm["History"]) {
                    if (!g_running) return;
                    if (count >= 20) break;
                    std::string mid = getJsonKeyStr(hm, {"MatchID"});
                    bool foundInCache = false;
                    {
                        std::lock_guard<std::mutex> lk(g_mutex);
                        if (g_matchDetailsCache.count(mid)) {
                            if (g_matchDetailsCache[mid].count(puuid)) {
                                auto& ms = g_matchDetailsCache[mid][puuid];
                                bool isStandard = ms.queueId.empty() || ms.queueId == "competitive" || ms.queueId == "unrated" || ms.queueId == "premier" || ms.queueId == "swiftplay";
                                
                                if (isStandard) {
                                    kills += ms.kills; deaths += ms.deaths;
                                    hs += ms.headshots; bs += ms.bodyshots; ls += ms.legshots;
                                    totalScore += ms.score;
                                    totalRounds += ms.roundsWon + ms.roundsLost;
                                    if (ms.won) matchesWon++;
                                    matchesPlayed++;
                                    if (!ms.characterId.empty()) agentPlays[ms.characterId]++;
                                    kdOk = true; 
                                }
                                recentMatches.push_back({mid, ms.characterId, ms.queueId, ms.kills, ms.deaths, ms.assists, ms.score, ms.roundsWon, ms.roundsLost, ms.won});
                                count++;
                            }
                            foundInCache = true;
                        }
                    }
                    if (foundInCache) continue;

                    auto mr = httpRequest("GET", sess.pdHost, 443, "/match-details/v1/matches/" + mid, pdH, "", true, false);
                    if (mr.statusCode == 200) {
                        auto md = json::parse(mr.body);
                        std::map<std::string, MatchDetailsCacheEntry> matchStats;
                        
                        std::string queueId = "";
                        if (md.contains("matchInfo") && md["matchInfo"].is_object()) {
                            queueId = getJsonKeyStr(md["matchInfo"], {"queueID"});
                        }
                        
                        std::map<std::string, bool> teamWon;
                        std::map<std::string, int> teamRoundsWon;
                        std::map<std::string, int> teamRoundsLost;
                        if (md.contains("teams") && md["teams"].is_array()) {
                            for (auto& t : md["teams"]) {
                                std::string tId = getJsonKeyStr(t, {"teamId"});
                                bool won = (t.contains("won") && t["won"].is_boolean()) ? t["won"].get<bool>() : false;
                                int rWon = (t.contains("roundsWon") && t["roundsWon"].is_number()) ? t["roundsWon"].get<int>() : 0;
                                teamWon[tId] = won;
                                teamRoundsWon[tId] = rWon;
                            }
                            if (teamRoundsWon.count("Blue") && teamRoundsWon.count("Red")) {
                                teamRoundsLost["Blue"] = teamRoundsWon["Red"];
                                teamRoundsLost["Red"] = teamRoundsWon["Blue"];
                            }
                        }

                        MatchScoreboard sb;
                        sb.matchId = mid;
                        if (teamRoundsWon.count("Red")) sb.roundsRed = teamRoundsWon["Red"];
                        if (teamRoundsWon.count("Blue")) sb.roundsBlue = teamRoundsWon["Blue"];

                        if (md.contains("players") && md["players"].is_array()) {
                            for (auto& p : md["players"]) {
                                std::string subject = getJsonKeyStr(p, {"subject"});
                                std::string characterId = getJsonKeyStr(p, {"characterId"});
                                std::string teamId = getJsonKeyStr(p, {"teamId"});
                                int pk = 0, pd = 0, pa = 0, pscore = 0;
                                if (p.contains("stats") && p["stats"].is_object()) {
                                    if (p["stats"].contains("kills") && p["stats"]["kills"].is_number()) pk = p["stats"]["kills"].get<int>();
                                    if (p["stats"].contains("deaths") && p["stats"]["deaths"].is_number()) pd = p["stats"]["deaths"].get<int>();
                                    if (p["stats"].contains("assists") && p["stats"]["assists"].is_number()) pa = p["stats"]["assists"].get<int>();
                                    if (p["stats"].contains("score") && p["stats"]["score"].is_number()) pscore = p["stats"]["score"].get<int>();
                                }
                                bool won = teamWon.count(teamId) ? teamWon[teamId] : false;
                                int rWon = teamRoundsWon.count(teamId) ? teamRoundsWon[teamId] : 0;
                                int rLost = teamRoundsLost.count(teamId) ? teamRoundsLost[teamId] : 0;
                                matchStats[subject] = {queueId, pk, pd, 0, 0, 0, characterId, pa, pscore, rWon, rLost, won};
                                
                                sb.players.push_back({subject, characterId, teamId, pk, pd, pa, pscore});
                            }
                        }
                        
                        if (md.contains("roundResults") && md["roundResults"].is_array()) {
                            for (auto& r : md["roundResults"]) {
                                if (r.contains("playerStats") && r["playerStats"].is_array()) {
                                    for (auto& ps : r["playerStats"]) {
                                        std::string subject = getJsonKeyStr(ps, {"subject"});
                                        if (ps.contains("damage") && ps["damage"].is_array()) {
                                            for (auto& d : ps["damage"]) {
                                                if (d.contains("headshots") && d["headshots"].is_number()) matchStats[subject].headshots += d["headshots"].get<int>();
                                                if (d.contains("bodyshots") && d["bodyshots"].is_number()) matchStats[subject].bodyshots += d["bodyshots"].get<int>();
                                                if (d.contains("legshots") && d["legshots"].is_number()) matchStats[subject].legshots += d["legshots"].get<int>();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        {
                            std::lock_guard<std::mutex> lk(g_mutex);
                            g_matchDetailsCache[mid] = matchStats;
                            g_matchScoreboards[mid] = sb;
                        }
                        
                        if (matchStats.count(puuid)) {
                            auto& ms = matchStats[puuid];
                            bool isStandard = ms.queueId.empty() || ms.queueId == "competitive" || ms.queueId == "unrated" || ms.queueId == "premier" || ms.queueId == "swiftplay";

                            if (isStandard) {
                                kills += ms.kills; deaths += ms.deaths;
                                hs += ms.headshots; bs += ms.bodyshots; ls += ms.legshots;
                                totalScore += ms.score;
                                totalRounds += ms.roundsWon + ms.roundsLost;
                                if (ms.won) matchesWon++;
                                matchesPlayed++;
                                if (!ms.characterId.empty()) agentPlays[ms.characterId]++;
                                kdOk = true;
                            }
                            recentMatches.push_back({mid, ms.characterId, ms.queueId, ms.kills, ms.deaths, ms.assists, ms.score, ms.roundsWon, ms.roundsLost, ms.won});
                            count++;
                        }
                    }
                }
            }
        } catch (...) {}
    }

    float finalKd = kdOk ? (deaths > 0 ? (float)kills / (float)deaths : (float)kills) : -2.0f;

    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_statsFetching.count(puuid)) {
            g_statsCache[puuid] = {finalKd, hs, bs, ls, totalScore, totalRounds, matchesWon, matchesPlayed, agentPlays, recentMatches};
            g_statsFetching.erase(puuid);
        }
    }
}

void resolveDisplayNamesAndRanks(const Session& session, std::vector<PlayerInfo>& players) {
    if (players.empty()) return;

    // Apply cached names and ranks first
    std::vector<std::string> uncachedNamesPuuids;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& p : players) {
            if (g_nameCache.count(p.puuid)) {
                p.gameName = g_nameCache[p.puuid].first;
                p.tagLine = g_nameCache[p.puuid].second;
            } else {
                uncachedNamesPuuids.push_back(p.puuid);
            }
            if (g_rankCache.count(p.puuid)) {
                p.rankTier = g_rankCache[p.puuid].tier;
                p.rankRR = g_rankCache[p.puuid].rr;
                p.wins = g_rankCache[p.puuid].wins;
                p.losses = g_rankCache[p.puuid].losses;
            }
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
                std::lock_guard<std::mutex> lk(g_mutex);
                for (const auto& item : j) {
                    std::string subject = getJsonKeyStr(item, {"Subject", "subject"});
                    std::string gameName = getJsonKeyStr(item, {"GameName", "gameName"});
                    std::string tagLine = getJsonKeyStr(item, {"TagLine", "tagLine"});
                    if (gameName.empty()) gameName = "Player";
                    g_nameCache[subject] = {gameName, tagLine};
                }
            } catch (...) {}
        }
        // Update player names from newly cached data
        {
            std::lock_guard<std::mutex> lk(g_mutex);
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
    }

    // 2. Resolve uncached ranks (MMR) - check cache under lock first
    for (auto& p : players) {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_rankCache.count(p.puuid)) {
                p.rankTier = g_rankCache[p.puuid].tier;
                p.rankRR = g_rankCache[p.puuid].rr;
                p.wins = g_rankCache[p.puuid].wins;
                p.losses = g_rankCache[p.puuid].losses;
                p.peakRankTier = g_rankCache[p.puuid].peakTier;
                continue;
            }
        }

        auto mmrRes = httpRequest("GET", session.pdHost, 443, "/mmr/v1/players/" + p.puuid, pdHeaders, "", true, false);
        if (mmrRes.statusCode == 200) {
            int tier = 0;
            int rr = 0;
            int totalWins = 0;
            int totalGames = 0;
            int peakTier = 0;
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
                        
                        // Parse all-time W/L and peak rank
                        for (const auto& [sId, sData] : seasons.items()) {
                            if (sData.contains("NumberOfGames") && sData["NumberOfGames"].is_number()) {
                                totalGames += sData["NumberOfGames"].get<int>();
                            }
                            if (sData.contains("NumberOfWinsWithPlacements") && sData["NumberOfWinsWithPlacements"].is_number()) {
                                totalWins += sData["NumberOfWinsWithPlacements"].get<int>();
                            }
                            if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                int t = sData["CompetitiveTier"].get<int>();
                                if (t > peakTier) peakTier = t;
                            }
                        }
                    }
                }
            } catch (...) {}

            g_rankCache[p.puuid] = {tier, rr, totalWins, totalGames - totalWins, peakTier};
            p.rankTier = tier;
            p.rankRR = rr;
            p.wins = totalWins;
            p.losses = totalGames - totalWins;
            p.peakRankTier = peakTier;
        }
    }

    // 3. Resolve K/D asynchronously (single lock for the whole loop)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& p : players) {
            if (g_statsCache.count(p.puuid)) {
                p.kdRatio = g_statsCache[p.puuid].kdRatio;
                p.headshots = g_statsCache[p.puuid].headshots;
                p.bodyshots = g_statsCache[p.puuid].bodyshots;
                p.legshots = g_statsCache[p.puuid].legshots;
                p.totalScore = g_statsCache[p.puuid].totalScore;
                p.totalRounds = g_statsCache[p.puuid].totalRounds;
                p.matchesWon = g_statsCache[p.puuid].matchesWon;
                p.matchesPlayed = g_statsCache[p.puuid].matchesPlayed;
                p.agentPlays = g_statsCache[p.puuid].agentPlays;
                p.recentMatches = g_statsCache[p.puuid].recentMatches;
            } else if (!g_statsFetching.count(p.puuid)) {
                g_statsFetching.insert(p.puuid);
                std::thread(fetchPlayerStats, session, p.puuid, pdHeaders).detach();
            }
            p.isLoading = g_statsFetching.count(p.puuid) > 0;
        }
    }
}

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include "ftxui_stats_view.hpp"

// -----------------------------------------------------------------------------
// Ctrl+C Handler
// -----------------------------------------------------------------------------
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        return TRUE; // Ignore Ctrl+C
    }
    return FALSE;
}

void runDataPusher() {
    Log("runDataPusher started");
    std::string configPath = getExeDir() + "config.json";
    std::string pushUrl = "https://status.example.ru/api/update";

    // Create or read config
    std::ifstream cfgIn(configPath);
    if (cfgIn.is_open()) {
        try {
            json c = json::parse(cfgIn);
            if (c.contains("pushUrl")) pushUrl = c["pushUrl"];
        } catch(...) {}
        cfgIn.close();
    } else {
        std::ofstream cfgOut(configPath);
        if (cfgOut.is_open()) {
            json c = {{"pushUrl", pushUrl}};
            cfgOut << c.dump(4);
            cfgOut.close();
        }
    }

    { std::lock_guard<std::mutex> lk(g_webUrlMutex); g_publicWebUrl = "Загрузка..."; }
    
    // Extract domain, port, and path from URL
    std::regex urlReg(R"(^(https?)://([^/:]+)(?::(\d+))?(/.*)?$)");
    std::smatch m;
    if (!std::regex_match(pushUrl, m, urlReg)) {
        Log("Invalid pushUrl format");
        return;
    }
    bool isHttps = (m[1] == "https");
    std::string host = m[2];
    int port = isHttps ? 443 : 80;
    if (m[3].matched) port = std::stoi(m[3]);
    std::string path = m[4].matched ? m[4].str() : "/api/update";

    // Set the web UI URL to just the base URL
    std::string baseUrl = m[1].str() + "://" + host;
    if (m[3].matched) baseUrl += ":" + m[3].str();
    { std::lock_guard<std::mutex> lk(g_webUrlMutex); g_publicWebUrl = baseUrl; }

    Log("Data pusher configured for host: " + host + " path: " + path);

    while (true) {
        json j = {{"phase", "none"}, {"players", json::array()}};
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            j["phase"] = g_liveMatchState.phase;
            for (const auto& p : g_liveMatchState.players) {
                json pj; pj["team"] = p.teamId;
                std::string fullName = p.gameName;
                if (g_nameCache.count(p.puuid)) fullName = g_nameCache[p.puuid].first + "#" + g_nameCache[p.puuid].second;
                pj["name"] = fullName;
                pj["agent"] = g_globalAgentMap.count(p.characterId) ? g_globalAgentMap[p.characterId] : "Выбирает...";
                std::string rankName = "Unrated";
                if (g_rankCache.count(p.puuid)) rankName = formatRank(g_rankCache[p.puuid].tier, g_rankCache[p.puuid].rr).name;
                pj["rank"] = rankName;
                float kd = -1.0f; if (g_statsCache.count(p.puuid)) kd = g_statsCache[p.puuid].kdRatio;
                pj["kd"] = kd;
                j["players"].push_back(pj);
            }
        }
        
        std::map<std::string, std::string> headers = { {"Content-Type", "application/json"} };
        auto res = httpRequest("POST", host, port, path, headers, j.dump(), isHttps, false);
        
        if (res.statusCode != 200) {
            Log("Failed to push data: HTTP " + std::to_string(res.statusCode));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// -----------------------------------------------------------------------------
// Main Loop
// -----------------------------------------------------------------------------
int main() {
    Log("Starting main()");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    signal(SIGINT, SIG_IGN);

    // Disable Quick Edit Mode (prevents stopping program by clicking)
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prev_mode;
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);

    // Automatically set optimal size
    HWND console = GetConsoleWindow();
    if (console) {
        RECT r;
        GetWindowRect(console, &r);
        MoveWindow(console, r.left, r.top, 1000, 600, TRUE);
    }

    // Run auto-updater in background so UI starts immediately
    std::thread(checkAutoUpdate).detach();

    auto agentMap = getAgentMap();
    auto mapNameMap = getMapNameMap();
    
    g_globalAgentMap = agentMap;
    std::thread(runDataPusher).detach();

    // std::cout << "Метаданные успешно загружены!\n";

    Lockfile lock;
    Session session;
    MatchState matchState;

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    std::atomic<bool> refresh_ui = true;
    
    // Background polling thread
    Log("Starting polling thread");
    std::thread polling_thread([&]() {
        Log("Polling thread running");
        auto lastCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        while (g_running) {
            auto now = std::chrono::steady_clock::now();
            bool timeToRefresh = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 3;
            
            if (timeToRefresh) {
                lastCheck = now;
                lock = readLockfile();
                if (lock.port != 0) {
                    session = getSession(lock);
                    if (!session.accessToken.empty()) {
                        matchState = getLiveMatchState(session, lock);
                        if (matchState.phase != "none") {
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
                
                { std::lock_guard<std::mutex> lk(g_mutex); g_liveMatchState = matchState; }
                
                // Keep selectedPlayerInfo updated if we are viewing it
                if (g_currentView == AppView::PLAYER_STATS && !g_selectedPuuid.empty()) {
                    for (const auto& p : matchState.players) {
                        if (p.puuid == g_selectedPuuid) {
                            g_selectedPlayerInfo = p;
                            break;
                        }
                    }
                }
            }
            
            // Post event for UI animations
            screen.PostEvent(ftxui::Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // FTXUI Component
    auto renderer = ftxui::Renderer([&] {
        if (g_currentView == AppView::PLAYER_STATS) {
            return renderPlayerStats(agentMap);
        } else if (g_currentView == AppView::MATCH_SCOREBOARD) {
            return renderMatchScoreboard(agentMap);
        }
        return renderFTXUI(matchState, session, lock, agentMap, mapNameMap);
    });

    // Catch events for clicks and quitting
    renderer = ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q')) {
            g_running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        
        if (event == ftxui::Event::Special("\x03") || (event.is_character() && event.character() == "\x03")) { // Ctrl+C
            return true; 
        }
        
        if (event == ftxui::Event::Escape) {
            if (g_currentView == AppView::MATCH_SCOREBOARD) {
                g_currentView = AppView::PLAYER_STATS;
                return true;
            } else if (g_currentView == AppView::PLAYER_STATS) {
                g_currentView = AppView::MAIN;
                return true;
            } else {
                g_running = false;
                screen.ExitLoopClosure()();
                return true;
            }
        }
        
        if (event.is_mouse()) {
            g_mouseX = event.mouse().x;
            g_mouseY = event.mouse().y;
        }

        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left && event.mouse().motion == ftxui::Mouse::Released) {
            if (g_webUrlBox.Contain(event.mouse().x, event.mouse().y)) {
                std::string webUrl;
                { std::lock_guard<std::mutex> lk(g_webUrlMutex); webUrl = g_publicWebUrl; }
                if (webUrl.find("http") == 0) {
                    copyToClipboard(webUrl);
                    g_copiedLink = true;
                    // Start a thread to reset copied status after 2 seconds
                    std::thread([&screen]() {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        g_copiedLink = false;
                        screen.PostEvent(ftxui::Event::Custom);
                    }).detach();
                }
                return true;
            }

            if (g_currentView == AppView::MAIN) {
                // Check "My Stats" button
                if (g_myStatsBox.Contain(event.mouse().x, event.mouse().y) && !session.puuid.empty()) {
                    g_selectedPuuid = session.puuid;
                    bool found = false;
                    for (const auto& p : matchState.players) {
                        if (p.puuid == g_selectedPuuid) {
                            g_selectedPlayerInfo = p;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        g_selectedPlayerInfo = PlayerInfo();
                        g_selectedPlayerInfo.puuid = session.puuid;
                        g_selectedPlayerInfo.gameName = "Вы";
                        if (g_nameCache.count(session.puuid)) {
                            g_selectedPlayerInfo.gameName = g_nameCache[session.puuid].first;
                            g_selectedPlayerInfo.tagLine = g_nameCache[session.puuid].second;
                        }
                        if (g_rankCache.count(session.puuid)) {
                            g_selectedPlayerInfo.rankTier = g_rankCache[session.puuid].tier;
                            g_selectedPlayerInfo.rankRR = g_rankCache[session.puuid].rr;
                            g_selectedPlayerInfo.peakRankTier = g_rankCache[session.puuid].peakTier;
                        }
                        
                        // Manually trigger stats fetch if missing
                        if (!g_statsCache.count(session.puuid) && !g_statsFetching.count(session.puuid)) {
                            g_selectedPlayerInfo.isLoading = true;
                            g_statsFetching.insert(session.puuid);
                            std::map<std::string, std::string> pdHeaders = {
                                {"Authorization", "Bearer " + session.accessToken},
                                {"X-Riot-Entitlements-JWT", session.token},
                                {"X-Riot-ClientPlatform", getClientPlatformBase64()},
                                {"X-Riot-ClientVersion", session.clientVersion}
                            };
                            std::thread(fetchPlayerStats, session, session.puuid, pdHeaders).detach();
                        } else if (g_statsCache.count(session.puuid)) {
                            g_selectedPlayerInfo.kdRatio = g_statsCache[session.puuid].kdRatio;
                            g_selectedPlayerInfo.headshots = g_statsCache[session.puuid].headshots;
                            g_selectedPlayerInfo.bodyshots = g_statsCache[session.puuid].bodyshots;
                            g_selectedPlayerInfo.legshots = g_statsCache[session.puuid].legshots;
                            g_selectedPlayerInfo.totalScore = g_statsCache[session.puuid].totalScore;
                            g_selectedPlayerInfo.totalRounds = g_statsCache[session.puuid].totalRounds;
                            g_selectedPlayerInfo.matchesWon = g_statsCache[session.puuid].matchesWon;
                            g_selectedPlayerInfo.matchesPlayed = g_statsCache[session.puuid].matchesPlayed;
                            g_selectedPlayerInfo.agentPlays = g_statsCache[session.puuid].agentPlays;
                            g_selectedPlayerInfo.recentMatches = g_statsCache[session.puuid].recentMatches;
                        }
                    }
                    g_currentView = AppView::PLAYER_STATS;
                    return true;
                }
                
                // Check Player Names
                for (const auto& [puuid, box] : g_playerBoxes) {
                    if (box.Contain(event.mouse().x, event.mouse().y)) {
                        g_selectedPuuid = puuid;
                        for (const auto& p : matchState.players) {
                            if (p.puuid == puuid) {
                                g_selectedPlayerInfo = p;
                                break;
                            }
                        }
                        g_currentView = AppView::PLAYER_STATS;
                        return true;
                    }
                }
            } else if (g_currentView == AppView::PLAYER_STATS) {
                // Check recent match clicks
                for (size_t i = 0; i < g_matchBoxes.size(); ++i) {
                    if (g_matchBoxes[i].Contain(event.mouse().x, event.mouse().y)) {
                        int matchIdx = g_statsMatchOffset + i;
                        if (matchIdx < g_selectedPlayerInfo.recentMatches.size()) {
                            std::string matchId = g_selectedPlayerInfo.recentMatches[matchIdx].matchId;
                            if (!matchId.empty()) {
                                g_selectedMatchId = matchId;
                                g_currentView = AppView::MATCH_SCOREBOARD;
                            }
                        }
                        return true;
                    }
                }
            }
        }
        
        // Handle Scrolling in Stats View
        if (g_currentView == AppView::PLAYER_STATS) {
            if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelUp) {
                if (g_statsMatchOffset > 0) g_statsMatchOffset--;
                return true;
            }
            if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelDown) {
                int maxOffset = std::max(0, (int)g_selectedPlayerInfo.recentMatches.size() - 8);
                if (g_statsMatchOffset < maxOffset) g_statsMatchOffset++;
                return true;
            }
            if (event == ftxui::Event::ArrowUp) {
                if (g_statsMatchOffset > 0) g_statsMatchOffset--;
                return true;
            }
            if (event == ftxui::Event::ArrowDown) {
                int maxOffset = std::max(0, (int)g_selectedPlayerInfo.recentMatches.size() - 8);
                if (g_statsMatchOffset < maxOffset) g_statsMatchOffset++;
                return true;
            }
        }
        return false;
    });

    screen.Loop(renderer);
    
    g_running = false;
    if (polling_thread.joinable()) {
        polling_thread.join();
    }
    
    if (g_hSession) { WinHttpCloseHandle(g_hSession); g_hSession = nullptr; }
    return 0;
}
