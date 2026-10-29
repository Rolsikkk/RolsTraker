#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <windows.h>
#include "clipboard.hpp"
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
#include <random>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include <queue>
#include <cmath>
#include <nlohmann/json.hpp>
#include <tlhelp32.h>
#include <filesystem>
#include <exception>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>

// FTXUI Headers
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>

using json = nlohmann::json;

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Application Version Constant
const std::string CURRENT_VERSION = "v3.3.9";
const std::string GITHUB_REPO     = "Rolsikkk/RolsTraker";

struct PlayerStats {
    float kdRatio = -1.0f;
    float hsPercent = 0.0f;
    int headshots = 0;
    int bodyshots = 0;
    int legshots = 0;
    int matchesWon = 0;
    int matchesPlayed = 0;
    int acs = 0;
    std::string favAgent = "";
    bool isPrivate = false;
    bool hasData = false;
    std::string source = "Tracker.gg";
};

static std::map<std::string, json> g_matchDetailsCache;
static std::mutex g_matchCacheMutex;

struct RankCacheEntry {
    int tier = 0;
    int rr = 0;
    int wins = 0;
    int losses = 0;
    int peakTier = 0;
    int peakRR = 0;
};

static std::map<std::string, std::pair<std::string, std::string>> g_nameCache;
static std::map<std::string, RankCacheEntry> g_rankCache;
static std::map<std::string, PlayerStats> g_statsCache;
static std::set<std::string> g_statsFetching;

static std::recursive_mutex g_mutex;
static std::atomic<bool> g_running{true};
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

UpdateInfo g_updateInfo;
bool g_showUpdateModal = false;
ftxui::Box g_updateNowBox;
ftxui::Box g_updateLaterBox;
ftxui::Box g_updateBadgeBox;
std::string g_updateStatus;
std::mutex g_updateMutex;
std::string g_penaltiesStatus;
std::mutex g_penaltiesMutex;
ftxui::Box g_dodgeButtonBox;
bool g_dodgeConfirm = false;
ftxui::Box g_dodgeConfirmBox;
ftxui::Box g_dodgeCancelBox;
bool g_mousePressed = false;
std::string g_activeButton = "";
std::chrono::steady_clock::time_point g_lastButtonClickTime;
static std::mutex g_logMutex;

std::string getExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t lastSlash = s.find_last_of("\\/");
    if (lastSlash != std::string::npos) return s.substr(0, lastSlash);
    return ".";
}

// Modal & Dialog state boxes
ftxui::Box g_hiddenLogBtnBox;
bool g_showOpenLogModal = false;
ftxui::Box g_openLogYesBox;
ftxui::Box g_openLogNoBox;
ftxui::Box g_openLogModalBox;

bool g_showFirstRunModal = false;
ftxui::Box g_firstRunOpt1Box;
ftxui::Box g_firstRunOpt2Box;
ftxui::Box g_firstRunModalBox;

static bool g_setupCompleted = false;
static std::string g_installMode = "portable";

std::string getAppDataDir() {
    char appData[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData) == S_OK) {
        std::string dir = std::string(appData) + "\\RolsTracker";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir;
    }
    return getExeDir();
}

std::string getSettingsPath() {
    return getAppDataDir() + "\\settings.json";
}

std::string getLogFilePath() {
    if (g_installMode == "appdata") {
        char localApp[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp) == S_OK) {
            std::string dir = std::string(localApp) + "\\Programs\\RolsTracker";
            CreateDirectoryA(dir.c_str(), NULL);
            return dir + "\\debug.log";
        }
    }
    return getExeDir() + "\\debug.log";
}

void Log(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    std::string path = getLogFilePath();
    std::ofstream ofs(path, std::ios::app);
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = {};
    localtime_s(&tm, &now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    ofs << "[" << buf << "] " << msg << "\n";
    ofs.flush();
}

static std::string g_discordClientId = "1558391767168978964";
static int g_windowWidth = 1080;
static int g_windowHeight = 680;

void saveSettings() {
    try {
        std::string settingsPath = getSettingsPath();
        json j;
        std::ifstream in(settingsPath);
        if (in.is_open()) {
            try { in >> j; } catch (...) {}
        }
        j["discordClientId"] = g_discordClientId;
        j["windowWidth"] = g_windowWidth;
        j["windowHeight"] = g_windowHeight;
        j["setupCompleted"] = g_setupCompleted;
        j["installMode"] = g_installMode;
        std::ofstream out(settingsPath);
        if (out.is_open()) {
            out << j.dump(4);
        }
    } catch (...) {}
}

void loadSettings() {
    try {
        std::string settingsPath = getSettingsPath();
        if (!std::filesystem::exists(settingsPath)) {
            std::string fallback = getExeDir() + "\\settings.json";
            if (std::filesystem::exists(fallback)) settingsPath = fallback;
        }
        std::ifstream f(settingsPath);
        if (f.is_open()) {
            json j;
            f >> j;
            if (j.contains("discordClientId") && j["discordClientId"].is_string()) {
                g_discordClientId = j["discordClientId"].get<std::string>();
            }
            if (j.contains("windowWidth") && j["windowWidth"].is_number_integer()) {
                g_windowWidth = j["windowWidth"].get<int>();
            }
            if (j.contains("windowHeight") && j["windowHeight"].is_number_integer()) {
                g_windowHeight = j["windowHeight"].get<int>();
            }
            if (j.contains("setupCompleted") && j["setupCompleted"].is_boolean()) {
                g_setupCompleted = j["setupCompleted"].get<bool>();
            }
            if (j.contains("installMode") && j["installMode"].is_string()) {
                g_installMode = j["installMode"].get<std::string>();
            }
        } else {
            saveSettings();
        }
    } catch (...) {}
}

bool createDesktopShortcut(const std::string& targetExePath, const std::string& shortcutName = "RolsTracker.lnk") {
    CoInitialize(NULL);
    IShellLinkA* pShellLink = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&pShellLink);
    if (SUCCEEDED(hr)) {
        pShellLink->SetPath(targetExePath.c_str());
        size_t lastSlash = targetExePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            pShellLink->SetWorkingDirectory(targetExePath.substr(0, lastSlash).c_str());
        }
        pShellLink->SetDescription("RolsTracker - Valorant Match Tracker & Stats");
        pShellLink->SetIconLocation(targetExePath.c_str(), 0);

        IPersistFile* pPersistFile = NULL;
        hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
        if (SUCCEEDED(hr)) {
            char desktopPath[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath) == S_OK) {
                std::string linkPath = std::string(desktopPath) + "\\" + shortcutName;
                std::wstring wLinkPath(linkPath.begin(), linkPath.end());
                pPersistFile->Save(wLinkPath.c_str(), TRUE);
            }
            pPersistFile->Release();
        }
        pShellLink->Release();
    }
    CoUninitialize();
    return SUCCEEDED(hr);
}

void installToAppDataAndCreateShortcut() {
    try {
        char localApp[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp) != S_OK) {
            return;
        }
        std::string targetDir = std::string(localApp) + "\\Programs\\RolsTracker";
        CreateDirectoryA((std::string(localApp) + "\\Programs").c_str(), NULL);
        CreateDirectoryA(targetDir.c_str(), NULL);

        char currentExe[MAX_PATH];
        GetModuleFileNameA(NULL, currentExe, MAX_PATH);

        std::string targetExe = targetDir + "\\RolsTracker.exe";
        CopyFileA(currentExe, targetExe.c_str(), FALSE);

        createDesktopShortcut(targetExe, "RolsTracker.lnk");

        g_installMode = "appdata";
        g_setupCompleted = true;
        saveSettings();
        Log("Installed to AppData and created desktop shortcut: " + targetExe);
    } catch (const std::exception& e) {
        Log("installToAppData error: " + std::string(e.what()));
    }
}

void runPortableMode() {
    g_installMode = "portable";
    g_setupCompleted = true;
    saveSettings();
    Log("Selected Portable Mode: running from " + getExeDir());
}

void openLogFolder() {
    try {
        std::string logFile = getLogFilePath();
        if (!std::filesystem::exists(logFile)) {
            Log("Log file created for viewer");
        }
        std::string cmd = "/select,\"" + logFile + "\"";
        ShellExecuteA(NULL, "open", "explorer.exe", cmd.c_str(), NULL, SW_SHOWNORMAL);
    } catch (...) {}
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

inline std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

inline std::string urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : str) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << (int)c;
        }
    }
    return escaped.str();
}

std::string base64_encode(const std::string& in) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[chars[i]] = i;
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

std::string getJsonKeyStr(const json& j, const std::vector<std::string>& keys) {
    if (!j.is_object()) return "";
    for (const auto& key : keys) {
        if (j.contains(key) && !j[key].is_null()) {
            if (j[key].is_string()) return j[key].get<std::string>();
            if (j[key].is_number()) return std::to_string(j[key].get<long long>());
        }
    }
    return "";
}

// -----------------------------------------------------------------------------
// Silent Curl Execution via CreateProcessA (Bypasses Cloudflare / Fast)
// -----------------------------------------------------------------------------
std::string execCurl(const std::string& args) {
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi = {};
    std::string fullCmd = "curl.exe " + args;
    std::vector<char> cmdVec(fullCmd.begin(), fullCmd.end());
    cmdVec.push_back(0);

    BOOL success = CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    std::string result;
    if (success) {
        char buffer[8192];
        DWORD bytesRead = 0;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            result.append(buffer, bytesRead);
        }
        if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hReadPipe);
    return result;
}

// -----------------------------------------------------------------------------
// HTTP Response & WinHTTP Wrapper (For Riot APIs)
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
    if (host.empty()) return response;

    if (!g_hSession) {
        std::lock_guard<std::mutex> lk(g_httpMutex);
        if (!g_hSession) {
            g_hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (g_hSession) {
                DWORD dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00000800; // TLS 1.2 + TLS 1.3
                WinHttpSetOption(g_hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols));
                WinHttpSetTimeouts(g_hSession, 2000, 2000, 3000, 3000);
            }
        }
    }
    if (!g_hSession) return response;

    std::wstring wHost = utf8ToWide(host);
    std::wstring wMethod = utf8ToWide(method);
    std::wstring wPath = utf8ToWide(path);
    DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;

    std::wstring wHeaders = L"Connection: keep-alive\r\n";
    for (const auto& [k, v] : headers) {
        if (k == "Connection") continue;
        wHeaders += utf8ToWide(k) + L": " + utf8ToWide(v) + L"\r\n";
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        HINTERNET hConnect = WinHttpConnect(g_hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            if (attempt == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            return response;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            if (attempt == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            return response;
        }

        if (ignoreCert && isHttps) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                             SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                             SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        }

        BOOL bResults = WinHttpSendRequest(
            hRequest,
            wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(),
            wHeaders.empty() ? 0 : -1L,
            bodyData.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)bodyData.c_str(),
            bodyData.empty() ? 0 : (DWORD)bodyData.length(),
            bodyData.empty() ? 0 : (DWORD)bodyData.length(),
            0
        );

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (!bResults) {
            DWORD err = GetLastError();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            if (attempt == 0 && (err == 12030 || err == 12029 || err == 12002)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                continue;
            }
            break;
        }

        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = dwStatusCode;

        DWORD dwDownloaded = 0;
        while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
            std::vector<char> buffer(dwSize);
            if (!WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded) || dwDownloaded == 0) {
                break;
            }
            response.body.append(buffer.data(), dwDownloaded);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        break;
    }

    return response;
}

bool downloadFileWithRedirects(const std::string& initialUrl, const std::string& outputPath, const std::string& authToken = "", std::function<void(int percent, size_t downloaded, size_t total)> progressCallback = nullptr) {
    Log("[DOWNLOAD] Starting download from URL: " + initialUrl + " to " + outputPath);
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

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        std::wstring wHost = utf8ToWide(host);
        std::wstring wPath = utf8ToWide(path);

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        std::wstring customHeaders;
        if (redirectCount == 0 && !authToken.empty()) {
            customHeaders = L"Authorization: token " + utf8ToWide(authToken) + L"\r\nAccept: application/octet-stream\r\n";
        }
        LPCWSTR pHeaders = customHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : customHeaders.c_str();
        DWORD dwHeadersLength = customHeaders.empty() ? 0 : (DWORD)customHeaders.length();

        BOOL bResults = WinHttpSendRequest(hRequest, pHeaders, dwHeadersLength, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (!bResults) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

        if (dwStatusCode == 301 || dwStatusCode == 302 || dwStatusCode == 307 || dwStatusCode == 308) {
            DWORD dwHeaderSize = 0;
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwHeaderSize, WINHTTP_NO_HEADER_INDEX);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<wchar_t> locationBuffer(dwHeaderSize / sizeof(wchar_t) + 1);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, locationBuffer.data(), &dwHeaderSize, WINHTTP_NO_HEADER_INDEX)) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, locationBuffer.data(), -1, NULL, 0, NULL, NULL);
                    if (len <= 1) {
                        WinHttpCloseHandle(hRequest);
                        WinHttpCloseHandle(hConnect);
                        WinHttpCloseHandle(hSession);
                        return false;
                    }
                    std::string newUrl(len - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, locationBuffer.data(), -1, &newUrl[0], len, NULL, NULL);
                    if (newUrl.size() > 0 && newUrl[0] == '/') {
                        currentUrl = "https://" + host + newUrl;
                    } else if (newUrl.find("://") == std::string::npos) {
                        currentUrl = "https://" + host + "/" + newUrl;
                    } else {
                        currentUrl = newUrl;
                    }
                    Log("[DOWNLOAD] Redirect " + std::to_string(redirectCount + 1) + " -> " + currentUrl + " (HTTP " + std::to_string(dwStatusCode) + ")");
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    continue;
                }
            }
        }

        if (dwStatusCode == 200) {
            DWORD contentLength = 0;
            Log("[DOWNLOAD] Connected successfully (HTTP 200)");
            DWORD dwLenSize = sizeof(contentLength);
            bool hasContentLength = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &dwLenSize, WINHTTP_NO_HEADER_INDEX);

            std::ofstream outFile(outputPath, std::ios::binary);
            if (!outFile.is_open()) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            DWORD dwDownloaded = 0;
            DWORD totalDownloaded = 0;
            int lastPercent = -1;

            while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                std::vector<char> buffer(dwSize);
                if (!WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded) || dwDownloaded == 0) break;
                outFile.write(buffer.data(), dwDownloaded);
                totalDownloaded += dwDownloaded;

                if (hasContentLength && contentLength > 0) {
                    int percent = (int)((totalDownloaded * 100ULL) / contentLength);
                    if (percent > 100) percent = 100;
                    if (percent != lastPercent) {
                        lastPercent = percent;
                        if (progressCallback) progressCallback(percent, totalDownloaded, contentLength);
                    }
                } else {
                    if (progressCallback) progressCallback(-1, totalDownloaded, 0);
                }
            }

            outFile.close();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            Log("[DOWNLOAD] Download finished. Total bytes: " + std::to_string(totalDownloaded));
            return (totalDownloaded > 0);
        }

        Log("[DOWNLOAD] Download failed at URL: " + currentUrl + " with HTTP code: " + std::to_string(dwStatusCode) + " (LastError=" + std::to_string(GetLastError()) + ")");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    Log("[DOWNLOAD] Download exceeded max redirects");
    return false;
}

static std::string getGitHubAuthToken() {
    const char* envTok = std::getenv("GITHUB_TOKEN");
    if (envTok && strlen(envTok) > 0) return std::string(envTok);

    try {
        if (std::filesystem::exists("settings.json")) {
            std::ifstream f("settings.json");
            json j = json::parse(f);
            if (j.contains("githubToken") && j["githubToken"].is_string()) return j["githubToken"].get<std::string>();
            if (j.contains("github_token") && j["github_token"].is_string()) return j["github_token"].get<std::string>();
        }
    } catch (...) {}

    FILE* pipe = _popen("gh auth token 2>nul", "r");
    if (pipe) {
        char buf[256];
        std::string out;
        while (fgets(buf, sizeof(buf), pipe)) out += buf;
        _pclose(pipe);
        while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
        if (!out.empty() && out.rfind("gh", 0) == 0) return out;
    }
    return "";
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int build = 0;
};

static SemVer parseSemVer(const std::string& v) {
    SemVer res;
    int nums[4] = {0, 0, 0, 0};
    int count = 0;
    size_t i = 0;
    while (i < v.size() && count < 4) {
        if (isdigit(static_cast<unsigned char>(v[i]))) {
            int n = 0;
            while (i < v.size() && isdigit(static_cast<unsigned char>(v[i]))) {
                n = n * 10 + (v[i] - '0');
                ++i;
            }
            nums[count++] = n;
        } else {
            ++i;
        }
    }
    res.major = nums[0];
    res.minor = nums[1];
    res.patch = nums[2];
    res.build = nums[3];
    return res;
}

static bool isNewerVersion(const std::string& remote, const std::string& local) {
    SemVer r = parseSemVer(remote);
    SemVer l = parseSemVer(local);
    if (r.major != l.major) return r.major > l.major;
    if (r.minor != l.minor) return r.minor > l.minor;
    if (r.patch != l.patch) return r.patch > l.patch;
    if (r.build != l.build) return r.build > l.build;
    return false;
}

void startUpdateDownload() {
    {
        std::lock_guard<std::mutex> lk(g_updateMutex);
        if (g_updateInfo.isDownloading || g_updateInfo.downloadUrl.empty()) return;
        g_updateInfo.isDownloading = true;
        g_updateInfo.downloadPercent = 0;
        g_updateInfo.statusText = "Подготовка: 0%";
    }

    std::thread([]() {
        std::string downloadUrl;
        std::string latestVersion;
        {
            std::lock_guard<std::mutex> lk(g_updateMutex);
            downloadUrl = g_updateInfo.downloadUrl;
            latestVersion = g_updateInfo.latestVersion;
        }

        char currentExePath[MAX_PATH];
        GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

        std::string newExePath = std::string(currentExePath) + ".new";
        std::string oldExePath = std::string(currentExePath) + ".old";

        auto progressCb = [](int percent, size_t downloaded, size_t total) {
            std::lock_guard<std::mutex> lk(g_updateMutex);
            g_updateInfo.downloadPercent = percent;
            if (percent >= 0 && total > 0) {
                double curMB = (double)downloaded / (1024.0 * 1024.0);
                double totMB = (double)total / (1024.0 * 1024.0);
                char buf[128];
                snprintf(buf, sizeof(buf), "Скачивание: %d%% (%.1f / %.1f МБ)", percent, curMB, totMB);
                g_updateInfo.statusText = buf;
            } else {
                double curMB = (double)downloaded / (1024.0 * 1024.0);
                char buf[128];
                snprintf(buf, sizeof(buf), "Скачано: %.1f МБ...", curMB);
                g_updateInfo.statusText = buf;
            }
        };

        std::string token = getGitHubAuthToken();
        if (downloadFileWithRedirects(downloadUrl, newExePath, token, progressCb)) {
            {
                std::lock_guard<std::mutex> lk(g_updateMutex);
                g_updateInfo.statusText = "Установка и перезапуск...";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            DeleteFileA(oldExePath.c_str());
            MoveFileA(currentExePath, oldExePath.c_str());
            MoveFileA(newExePath.c_str(), currentExePath);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            STARTUPINFOA si = { sizeof(STARTUPINFOA) };
            PROCESS_INFORMATION pi = {};
            std::string quotedCmd = "\"" + std::string(currentExePath) + "\"";
            if (CreateProcessA(NULL, (LPSTR)quotedCmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            exit(0);
        } else {
            std::lock_guard<std::mutex> lk(g_updateMutex);
            g_updateInfo.isDownloading = false;
            g_updateInfo.statusText = "Ошибка загрузки файла. Попробуйте позже.";
        }
    }).detach();
}

void checkAutoUpdate() {
    { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = "checking"; }
    Log("[UPDATE] Checking for updates. Current version: " + CURRENT_VERSION);

    // 1. Primary check: Cloudflare Worker API
    try {
        std::map<std::string, std::string> cfHeaders = {
            {"User-Agent", "RolsTracker-AutoUpdater"},
            {"Accept", "application/json"}
        };
        auto cfRes = httpRequest("GET", "rolstracker.3fun.workers.dev", 443, "/api/version", cfHeaders, "", true, false);
        Log("[UPDATE] Cloudflare Worker response: status=" + std::to_string(cfRes.statusCode) + ", body=" + cfRes.body);
        if (cfRes.statusCode == 200) {
            auto j = json::parse(cfRes.body);
            std::string latestVersion = j.value("version", j.value("tag_name", ""));
            std::string releaseName = j.value("name", latestVersion);
            std::string releaseNotes = j.value("notes", "");
            std::string downloadUrl = j.value("download_url", "");

            if (!latestVersion.empty() && isNewerVersion(latestVersion, CURRENT_VERSION) && !downloadUrl.empty()) {
                std::lock_guard<std::mutex> lk(g_updateMutex);
                g_updateInfo.available = true;
                g_updateInfo.latestVersion = latestVersion;
                g_updateInfo.releaseTitle = releaseName;
                g_updateInfo.releaseNotes = releaseNotes;
                g_updateInfo.downloadUrl = downloadUrl;
                g_updateInfo.isDownloading = false;
                g_updateInfo.statusText = "";
                g_updateStatus = "available " + latestVersion;
                g_showUpdateModal = true;
                return;
            }
        }
    } catch (...) {}

    // 2. Secondary fallback: GitHub Releases API
    std::map<std::string, std::string> headers = {
        {"User-Agent", "RolsTracker-AutoUpdater"},
        {"Accept", "application/vnd.github.v3+json"}
    };

    std::string token = getGitHubAuthToken();
    if (!token.empty()) {
        headers["Authorization"] = "token " + token;
    }

    auto res = httpRequest("GET", "api.github.com", 443, "/repos/" + GITHUB_REPO + "/releases/latest", headers, "", true, false);

    if (res.statusCode == 200) {
        try {
            auto release = json::parse(res.body);
            std::string latestVersion = release.value("tag_name", "");
            std::string releaseName = release.value("name", latestVersion);
            std::string releaseNotes = release.value("body", "");

            if (!latestVersion.empty() && isNewerVersion(latestVersion, CURRENT_VERSION)) {
                std::string downloadUrl = "";
                if (release.contains("assets") && release["assets"].is_array()) {
                    for (const auto& asset : release["assets"]) {
                        std::string name = asset.value("name", "");
                        if (name == "RolsTracker.exe" || name == "RolsTraker.exe") {
                            if (!token.empty() && asset.contains("url") && asset["url"].is_string()) {
                                downloadUrl = asset["url"].get<std::string>();
                            } else {
                                downloadUrl = asset.value("browser_download_url", "");
                            }
                            break;
                        }
                    }
                }

                if (!downloadUrl.empty()) {
                    std::lock_guard<std::mutex> lk(g_updateMutex);
                    g_updateInfo.available = true;
                    g_updateInfo.latestVersion = latestVersion;
                    g_updateInfo.releaseTitle = releaseName;
                    g_updateInfo.releaseNotes = releaseNotes;
                    g_updateInfo.downloadUrl = downloadUrl;
                    g_updateInfo.isDownloading = false;
                    g_updateInfo.statusText = "";
                    g_updateStatus = "available " + latestVersion;
                    g_showUpdateModal = true;
                    return;
                }
            }
        } catch (...) {}
    }

    { std::lock_guard<std::mutex> lk(g_updateMutex); g_updateStatus = ""; }
}

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
    int peakRankRR = 0;
    float kdRatio = -1.0f;
    float hsPercent = 0.0f;
    int wins = -1;
    int losses = -1;
    int headshots = 0;
    int bodyshots = 0;
    int legshots = 0;
    int matchesWon = 0;
    int matchesPlayed = 0;
    int acs = 0;
    std::string favAgent = "";
    bool isPrivate = false;
    bool isLoading = false;
    std::string statsSource = "";
    
    bool isConnected = true;
    bool isReconnectable = true;
    std::string connectionStatus = "";
    bool isMuted = false;

    // Booster and Party Detection
    bool isBooster = false;
    int customSkinCount = -1;
    bool skinsLoaded = false;
    int detectedPartySize = 0;
    int detectedSharedMatches = 0;
    std::string detectedPartyLabel = "";
    std::string detectedPartyPartners = "";

    // Kuronami Skin Detection
    bool hasKuronami = false;
    std::string kuronamiWeapon = "";

    // Nocturnum Skin Detection
    bool hasNocturnum = false;
    std::string nocturnumWeapon = "";

    // Primordium Skin Detection
    bool hasPrimordium = false;
    std::string primordiumWeapon = "";
};

// Global view state
#include "chat_translator.hpp"

struct ValorantChatMessage {
    std::string id;
    std::string cid;
    std::string senderPuuid;
    std::string senderName;
    std::string senderAgent;
    std::string teamId;
    std::string chatType;
    std::string text;
    std::string translated;
    std::string timestamp;
    bool isEnemy = false;
};

enum class AppView { MAIN, PLAYER_STATS, AGENT_PICK, REPORT, CHAT };
AppView g_currentView = AppView::MAIN;
std::string g_selectedPuuid = "";
std::string g_localPuuid = "";
PlayerInfo g_selectedPlayerInfo;
std::map<std::string, ftxui::Box> g_playerBoxes;
ftxui::Box g_myStatsBox;
ftxui::Box g_backButtonBox;
ftxui::Box g_openTrackerBox;
std::map<std::string, ftxui::Box> g_skinRowBoxes;
std::string g_selectedSkinWeaponUuid = "";
ftxui::Box g_skinModalCloseBox;
ftxui::Box g_skinModalBox;
ftxui::Box g_pickAgentBtnBox;
ftxui::Box g_agentPickerBackBox;
std::map<std::string, ftxui::Box> g_agentBoxes;
std::string g_pickStatusMsg = "";
bool g_isLockingAgent = false;
std::set<std::string> g_ownedAgents;
ftxui::Box g_reportOpenBox;
ftxui::Box g_reportCloseBox;
std::map<std::string, ftxui::Box> g_reportEnemyBoxes;
std::string g_reportStatusMsg = "";
std::set<std::string> g_reportedPuuids;
ftxui::Box g_chatOpenBox;
ftxui::Box g_chatCloseBox;
ftxui::Box g_chatClearBox;
ftxui::Box g_chatChannelBox;
ftxui::Box g_chatTranslateToggleBox;
ftxui::Box g_chatSendBox;

std::string g_chatTargetChannel = "team";
bool g_chatAutoTranslateRuToEn = true;
std::string g_chatInputText = "";
std::string g_chatStatusMessage = "";
std::chrono::steady_clock::time_point g_chatStatusTime;
std::string g_chatCurrentCoreCid = "";
std::string g_chatCurrentAllCid = "";
static Lockfile g_liveLock;

std::vector<ValorantChatMessage> g_chatMessages;
std::mutex g_chatMutex;
int g_mouseX = -1;
int g_mouseY = -1;

struct MatchState {
    std::string phase = "none"; // "none", "pregame", "coregame"
    std::string matchId;
    std::string mapId;
    std::string gameMode;
    std::vector<PlayerInfo> players;
    bool isError = false;
};

static MatchState g_liveMatchState;
static std::map<std::string, std::string> g_globalAgentMap;
static std::map<std::string, std::string> g_globalMapNameMap;
static std::string g_serverRegion = "N/A";

struct RankDisplay {
    std::string name;
};

RankDisplay formatRank(int tier, int rr, int peakTier = 0, int peakRR = 0) {
    if (tier <= 2) {
        if (peakTier > 2) {
            std::string peakName;
            if (peakTier >= 3 && peakTier <= 5) peakName = "Iron " + std::to_string(peakTier - 2);
            else if (peakTier >= 6 && peakTier <= 8) peakName = "Bronze " + std::to_string(peakTier - 5);
            else if (peakTier >= 9 && peakTier <= 11) peakName = "Silver " + std::to_string(peakTier - 8);
            else if (peakTier >= 12 && peakTier <= 14) peakName = "Gold " + std::to_string(peakTier - 11);
            else if (peakTier >= 15 && peakTier <= 17) peakName = "Platinum " + std::to_string(peakTier - 14);
            else if (peakTier >= 18 && peakTier <= 20) peakName = "Diamond " + std::to_string(peakTier - 17);
            else if (peakTier >= 21 && peakTier <= 23) peakName = "Ascendant " + std::to_string(peakTier - 20);
            else if (peakTier >= 24 && peakTier <= 26) peakName = "Immortal " + std::to_string(peakTier - 23);
            else if (peakTier >= 27) peakName = "Radiant";
            return {"Unrated (Пик: " + peakName + ")"};
        }
        return {"Unrated"};
    }

    std::string name;
    if (tier >= 3 && tier <= 5) name = "Iron " + std::to_string(tier - 2);
    else if (tier >= 6 && tier <= 8) name = "Bronze " + std::to_string(tier - 5);
    else if (tier >= 9 && tier <= 11) name = "Silver " + std::to_string(tier - 8);
    else if (tier >= 12 && tier <= 14) name = "Gold " + std::to_string(tier - 11);
    else if (tier >= 15 && tier <= 17) name = "Platinum " + std::to_string(tier - 14);
    else if (tier >= 18 && tier <= 20) name = "Diamond " + std::to_string(tier - 17);
    else if (tier >= 21 && tier <= 23) name = "Ascendant " + std::to_string(tier - 20);
    else if (tier >= 24 && tier <= 26) name = "Immortal " + std::to_string(tier - 23);
    else if (tier >= 27) name = "Radiant";

    if (rr >= 0) {
        name += " (" + std::to_string(rr) + " RR)";
    }
    return {name};
}

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
        HANDLE hFile = CreateFileA(lockPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            char buffer[512] = {0};
            DWORD bytesRead = 0;
            if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string line(buffer);
                size_t firstNewline = line.find_first_of("\r\n");
                if (firstNewline != std::string::npos) line = line.substr(0, firstNewline);

                std::stringstream ss(line);
                std::vector<std::string> parts;
                std::string part;
                while (std::getline(ss, part, ':')) {
                    parts.push_back(part);
                }
                if (parts.size() >= 5) {
                    try {
                        lock.name = parts[0];
                        lock.pid = parts[1];
                        lock.port = static_cast<uint16_t>(std::stoi(parts[2]));
                        lock.password = parts[3];
                        lock.protocol = parts[4];
                        lock.lockPathUsed = lockPath;
                        CloseHandle(hFile);
                        return lock;
                    } catch (...) {
                        CloseHandle(hFile);
                        continue;
                    }
                }
            }
            CloseHandle(hFile);
        }
    }
    return lock;
}

std::string getClientVersion() {
    static std::string cachedVersion = "";
    if (!cachedVersion.empty()) return cachedVersion;

    std::string localAppData;
    char* pValue;
    size_t len;
    if (_dupenv_s(&pValue, &len, "LOCALAPPDATA") == 0 && pValue != nullptr) {
        localAppData = pValue;
        free(pValue);
        std::string logPath = localAppData + "\\VALORANT\\Saved\\Logs\\ShooterGame.log";
        std::ifstream file(logPath);
        if (file.is_open()) {
            std::string line;
            std::regex versionRegex(R"(CI server version:\s+(release-\d+\.\d+-shipping-\d+-\d+))");
            while (std::getline(file, line)) {
                std::smatch match;
                if (std::regex_search(line, match, versionRegex)) {
                    cachedVersion = match[1].str();
                    return cachedVersion;
                }
            }
        }
    }

    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/version", {}, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            cachedVersion = j["data"]["riotClientVersion"].get<std::string>();
            return cachedVersion;
        } catch (...) {}
    }
    return cachedVersion.empty() ? "release-13.06-shipping-18-5590001" : cachedVersion;
}

std::pair<std::string, std::string> readShardFromLog() {
    char* localAppData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") == 0 && localAppData) {
        std::string logPath = std::string(localAppData) + "\\VALORANT\\Saved\\Logs\\ShooterGame.log";
        free(localAppData);

        std::ifstream file(logPath);
        if (file.is_open()) {
            std::string line;
            std::regex re(R"(glz-([a-zA-Z0-9]+)-1\.([a-zA-Z0-9]+)\.a\.pvp\.net)", std::regex::icase);
            std::regex rePd(R"(https://pd\.([a-zA-Z0-9]+)\.a\.pvp\.net)", std::regex::icase);
            int lineCount = 0;
            while (std::getline(file, line) && lineCount < 5000) {
                lineCount++;
                if (line.find("glz-") != std::string::npos) {
                    std::smatch match;
                    if (std::regex_search(line, match, re) && match.size() >= 3) {
                        std::string region = match[1].str();
                        std::string shard = match[2].str();
                        for (auto& c : region) c = tolower(c);
                        for (auto& c : shard) c = tolower(c);
                        return {region, shard};
                    }
                } else if (line.find("pd.") != std::string::npos) {
                    std::smatch match;
                    if (std::regex_search(line, match, rePd) && match.size() >= 2) {
                        std::string shard = match[1].str();
                        for (auto& c : shard) c = tolower(c);
                        return {shard, shard};
                    }
                }
            }
        }
    }
    return {"", ""};
}

Session getSession(const Lockfile& lock) {
    Session s;
    std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);
    
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

    // 1. Prioritize reading the actual active game server shard from VALORANT ShooterGame.log
    auto logShard = readShardFromLog();
    if (!logShard.first.empty()) {
        s.region = logShard.first;
        s.shard = logShard.second;
    } else {
        auto regRes = httpRequest("GET", "127.0.0.1", lock.port, "/riotclient/region-locale", {{"Authorization", authHeader}}, "", true, true);
        if (regRes.statusCode == 200) {
            try {
                auto j = json::parse(regRes.body);
                std::string r = getJsonKeyStr(j, {"region"});
                for (auto& c : r) c = tolower(c);
                if (!r.empty() && r != "na") {
                    s.region = r;
                    if (s.region == "latam" || s.region == "br") s.shard = "na";
                    else s.shard = s.region;
                }
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

std::map<std::string, std::string> getAgentMap() {
    std::map<std::string, std::string> agents;
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/agents?language=ru-RU&isPlayableCharacter=true", {}, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            for (const auto& item : j["data"]) {
                std::string uuid = item["uuid"].get<std::string>();
                for (auto& c : uuid) c = tolower(c);
                agents[uuid] = item["displayName"].get<std::string>();
            }
        } catch (...) {}
    }
    if (agents.empty()) {
        agents = {
            {"ddd5a15c-4fbe-42fa-3789-11a55ca5ee04", "Hush"},
            {"e370fa57-4757-3604-3648-499e1f642d3f", "Gekko"},
            {"dade69b4-4f5a-8528-247b-219e5a1facd6", "Fade"},
            {"5f8d3a7f-467b-97f3-062c-13acf203c006", "Breach"},
            {"cc8b64c8-4b25-4ff9-6e7f-37b4da43d235", "Deadlock"},
            {"b444168c-4e35-8076-db47-ef9bf368f384", "Tejo"},
            {"f94c3b30-42be-e959-889c-5aa313dba261", "Raze"},
            {"22697a3d-45bf-8dd7-4fec-84a9e28c69d7", "Chamber"},
            {"601dbbe7-43ce-be57-2a40-4abd24953621", "KAY/O"},
            {"6f2a04ca-43e0-be17-7f36-b3908627744d", "Skye"},
            {"117ed9e3-49f3-6512-3ccf-0cada7e3823b", "Cypher"},
            {"320b2a48-4d9b-a075-30f1-1f93a9b638fa", "Sova"},
            {"7c8a4701-4de6-9355-b254-e09bc2a34b72", "Miks"},
            {"1e58de9c-4950-5125-93e9-a0aee9f98746", "Killjoy"},
            {"95b78ed7-4637-86d9-7e41-71ba8c293152", "Harbor"},
            {"efba5359-4016-a1e5-7626-b1ae76895940", "Vyse"},
            {"707eab51-4836-f488-046a-cda6bf494859", "Viper"},
            {"eb93336a-449b-9c1b-0a54-a891f7921d69", "Phoenix"},
            {"92eeef5d-43b5-1d4a-8d03-b3927a09034b", "Veto"},
            {"41fb69c1-4189-7b37-f117-bcaf1e96f1bf", "Astra"},
            {"9f0d8ba9-4140-b941-57d3-a7ad57c6b417", "Brimstone"},
            {"0e38b510-41a8-5780-5e8f-568b2a4f2d6c", "Iso"},
            {"1dbf2edd-4729-0984-3115-daa5eed44993", "Clove"},
            {"bb2a4828-46eb-8cd1-e765-15848195d751", "Neon"},
            {"7f94d92c-4234-0a36-9646-3a87eb8b5c89", "Yoru"},
            {"df1cb487-4902-002e-5c17-d28e83e78588", "Waylay"},
            {"569fdd95-4d10-43ab-ca70-79becc718b46", "Sage"},
            {"a3bfb853-43b2-7238-a4f1-ad90e9e46bcc", "Reyna"},
            {"8e253930-4c05-31dd-1b6c-968525494517", "Omen"},
            {"add6443a-41bd-e414-f6ad-e58d267f4e95", "Jett"}
        };
    }
    return agents;
}

std::map<std::string, std::string> getMapNameMap() {
    std::map<std::string, std::string> maps;
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/maps?language=ru-RU", {}, "", true, false);
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
    if (maps.empty()) {
        maps = {
            {"/Game/Maps/AbilityDraft/AbilityDraft", "Gauntlet"},
            {"/Game/Maps/Ascent/Ascent", "Ascent"},
            {"/Game/Maps/Bonsai/Bonsai", "Split"},
            {"/Game/Maps/Canyon/Canyon", "Fracture"},
            {"/Game/Maps/Duality/Duality", "Bind"},
            {"/Game/Maps/Duel/Duel_1/Skirmish_A", "Столкновение A"},
            {"/Game/Maps/Duel/Duel_2/Skirmish_B", "Столкновение B"},
            {"/Game/Maps/Duel/Duel_3/Skirmish_C", "Столкновение C"},
            {"/Game/Maps/Duel/Duel_Heady/Skirmish_E", "Столкновение E"},
            {"/Game/Maps/Duel/Duel_Platform/Skirmish_D", "Столкновение D"},
            {"/Game/Maps/Foxtrot/Foxtrot", "Breeze"},
            {"/Game/Maps/HURM/HURM_Alley/HURM_Alley", "District"},
            {"/Game/Maps/HURM/HURM_Bowl/HURM_Bowl", "Kasbah"},
            {"/Game/Maps/HURM/HURM_Helix/HURM_Helix", "Drift"},
            {"/Game/Maps/HURM/HURM_HighTide/HURM_HighTide", "Glitch"},
            {"/Game/Maps/HURM/HURM_Yard/HURM_Yard", "Piazza"},
            {"/Game/Maps/Infinity/Infinity", "Abyss"},
            {"/Game/Maps/Jam/Jam", "Lotus"},
            {"/Game/Maps/Juliett/Juliett", "Sunset"},
            {"/Game/Maps/NPEV2/NPEV2", "Базовое обучение"},
            {"/Game/Maps/Pitt/Pitt", "Pearl"},
            {"/Game/Maps/Plummet/Plummet", "Summit"},
            {"/Game/Maps/Port/Port", "Icebox"},
            {"/Game/Maps/Poveglia/Range", "Стрельбище"},
            {"/Game/Maps/PovegliaV2/RangeV2", "Стрельбище"},
            {"/Game/Maps/Rook/Rook", "Corrode"},
            {"/Game/Maps/Triad/Triad", "Haven"}
        };
    }
    return maps;
}

std::string getClientPlatformBase64() {
    static std::string cachedBase64 = "";
    if (!cachedBase64.empty()) return cachedBase64;
    json j = {
        {"platformType", "PC"},
        {"platformOS", "Windows"},
        {"platformOSVersion", "10.0.19042.1.256.64bit"},
        {"platformChipset", "Unknown"}
    };
    cachedBase64 = base64_encode(j.dump());
    return cachedBase64;
}

std::map<std::string, std::string> getPresencesPartyMap(const Lockfile& lock) {
    std::map<std::string, std::string> puuidToParty;
    if (lock.port == 0 || lock.password.empty()) return puuidToParty;
    std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);
    auto res = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v4/presences", {{"Authorization", authHeader}}, "", true, true);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            if (j.contains("presences") && j["presences"].is_array()) {
                for (const auto& p : j["presences"]) {
                    std::string puuid = getJsonKeyStr(p, {"puuid"});
                    if (puuid.empty()) {
                        std::string idStr = getJsonKeyStr(p, {"id"});
                        size_t atPos = idStr.find('@');
                        if (atPos != std::string::npos) puuid = idStr.substr(0, atPos);
                    }
                    std::string privB64 = getJsonKeyStr(p, {"private"});
                    if (puuid.empty() || privB64.empty()) continue;
                    std::string privJsonStr = base64_decode(privB64);
                    try {
                        auto privJ = json::parse(privJsonStr);
                        std::string partyId = getJsonKeyStr(privJ, {"partyId", "partyID", "party_id"});
                        if (!partyId.empty() && partyId != "00000000-0000-0000-0000-000000000000") {
                            puuidToParty[puuid] = partyId;
                        }
                    } catch (...) {}
                }
            }
        } catch (...) {}
    }
    return puuidToParty;
}

void resolveOwnParty(const Session& session, std::vector<PlayerInfo>& players) {
    if (session.glzHost.empty() || session.accessToken.empty() || session.puuid.empty()) return;
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + session.accessToken},
        {"X-Riot-Entitlements-JWT", session.token},
        {"X-Riot-ClientPlatform", getClientPlatformBase64()},
        {"X-Riot-ClientVersion", session.clientVersion}
    };
    auto pRes = httpRequest("GET", session.glzHost, 443, "/parties/v1/players/" + session.puuid, headers, "", true, false);
    if (pRes.statusCode == 200) {
        try {
            auto pj = json::parse(pRes.body);
            std::string currentPartyId = getJsonKeyStr(pj, {"CurrentPartyID"});
            if (!currentPartyId.empty() && currentPartyId != "00000000-0000-0000-0000-000000000000") {
                auto partyRes = httpRequest("GET", session.glzHost, 443, "/parties/v1/parties/" + currentPartyId, headers, "", true, false);
                if (partyRes.statusCode == 200) {
                    auto partJ = json::parse(partyRes.body);
                    if (partJ.contains("Members") && partJ["Members"].is_array() && partJ["Members"].size() > 1) {
                        std::set<std::string> memberPuuids;
                        for (const auto& m : partJ["Members"]) {
                            std::string sub = getJsonKeyStr(m, {"Subject", "subject"});
                            if (!sub.empty()) memberPuuids.insert(sub);
                        }
                        for (auto& p : players) {
                            if (memberPuuids.count(p.puuid)) {
                                p.partyId = currentPartyId;
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }
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
            if (matchId.empty()) goto check_pregame;

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
                        if (p.contains("IsConnected") && p["IsConnected"].is_boolean()) {
                            info.isConnected = p["IsConnected"].get<bool>();
                        } else if (p.contains("isConnected") && p["isConnected"].is_boolean()) {
                            info.isConnected = p["isConnected"].get<bool>();
                        }
                        if (p.contains("IsReconnectable") && p["IsReconnectable"].is_boolean()) {
                            info.isReconnectable = p["IsReconnectable"].get<bool>();
                        } else if (p.contains("isReconnectable") && p["isReconnectable"].is_boolean()) {
                            info.isReconnectable = p["isReconnectable"].get<bool>();
                        }
                        if (p.contains("PlayerIdentity") && p["PlayerIdentity"].contains("Muted") && p["PlayerIdentity"]["Muted"].is_boolean()) {
                            info.isMuted = p["PlayerIdentity"]["Muted"].get<bool>();
                        } else if (p.contains("IsMuted") && p["IsMuted"].is_boolean()) {
                            info.isMuted = p["IsMuted"].get<bool>();
                        } else if (p.contains("isMuted") && p["isMuted"].is_boolean()) {
                            info.isMuted = p["isMuted"].get<bool>();
                        }
                        if (!info.isConnected) {
                            if (info.isReconnectable) {
                                info.connectionStatus = "[AFK]";
                            } else {
                                info.connectionStatus = "[ВЫШЕЛ]";
                            }
                        }
                        state.players.push_back(info);
                    }
                }
                return state;
            } else if (coreMatchRes.statusCode == 404 || coreMatchRes.statusCode == 400) {
                goto check_pregame;
            } else {
                state.isError = true;
                return state;
            }
        } catch (...) {}
    } else if (corePlayerRes.statusCode != 404 && corePlayerRes.statusCode != 400) {
        state.isError = true;
        return state;
    }

    // 2. Pregame check
    check_pregame:
    auto prePlayerRes = httpRequest("GET", session.glzHost, 443, "/pregame/v1/players/" + session.puuid, headers, "", true, false);
    if (prePlayerRes.statusCode == 200) {
        try {
            auto jPlayer = json::parse(prePlayerRes.body);
            std::string matchId = getJsonKeyStr(jPlayer, {"MatchID", "matchId"});
            if (matchId.empty()) { return state; }

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
                        if (p.contains("PlayerIdentity") && p["PlayerIdentity"].contains("Muted") && p["PlayerIdentity"]["Muted"].is_boolean()) {
                            info.isMuted = p["PlayerIdentity"]["Muted"].get<bool>();
                        } else if (p.contains("IsMuted") && p["IsMuted"].is_boolean()) {
                            info.isMuted = p["IsMuted"].get<bool>();
                        } else if (p.contains("isMuted") && p["isMuted"].is_boolean()) {
                            info.isMuted = p["isMuted"].get<bool>();
                        }
                        state.players.push_back(info);
                    }
                }
                return state;
            } else if (preMatchRes.statusCode == 404 || preMatchRes.statusCode == 400) {
                return state;
            } else {
                state.isError = true;
                return state;
            }
        } catch (...) {}
    } else if (prePlayerRes.statusCode != 404 && prePlayerRes.statusCode != 400) {
        state.isError = true;
    }

    return state;
}

// -----------------------------------------------------------------------------
// Owned Agents & Agent Auto-Pick (Pregame Select & Lock)
// -----------------------------------------------------------------------------
std::set<std::string> getOwnedAgents(const Session& session) {
    std::set<std::string> owned = {
        "add6443a-41bd-e414-f6ad-e58d267f4e95", // Jett
        "eb93336a-449b-9c1b-0a54-a891f7921d69", // Phoenix
        "320b2a48-4d9b-a075-30f1-1f93a9b638fa", // Sova
        "9f0d8ba9-4140-b941-57d3-a7ad57c6b417", // Brimstone
        "569fdd95-4d10-43ab-ca70-79becc718b46"  // Sage
    };
    if (session.accessToken.empty() || session.pdHost.empty() || session.puuid.empty()) return owned;

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + session.accessToken},
        {"X-Riot-Entitlements-JWT", session.token},
        {"X-Riot-ClientPlatform", getClientPlatformBase64()},
        {"X-Riot-ClientVersion", session.clientVersion}
    };

    auto res = httpRequest("GET", session.pdHost, 443, "/store/v1/entitlements/" + session.puuid + "/01a42a10-432b-4ff6-8607-4c4416826303", headers, "", true, false);
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            if (j.contains("EntitlementsByTypes") && j["EntitlementsByTypes"].is_array()) {
                for (const auto& group : j["EntitlementsByTypes"]) {
                    if (group.contains("Entitlements") && group["Entitlements"].is_array()) {
                        for (const auto& item : group["Entitlements"]) {
                            std::string itemId = getJsonKeyStr(item, {"ItemID", "itemId"});
                            for (auto& c : itemId) c = tolower(c);
                            if (!itemId.empty()) owned.insert(itemId);
                        }
                    }
                }
            }
        } catch (...) {}
    }
    return owned;
}

bool lockInAgent(const Session& session, const std::string& matchId, const std::string& characterId, std::string& outError) {
    if (session.accessToken.empty() || session.glzHost.empty() || matchId.empty() || characterId.empty()) {
        outError = "Матч не активен";
        return false;
    }
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + session.accessToken},
        {"X-Riot-Entitlements-JWT", session.token},
        {"X-Riot-ClientPlatform", getClientPlatformBase64()},
        {"X-Riot-ClientVersion", session.clientVersion}
    };

    // 1. Hover/select first
    httpRequest("POST", session.glzHost, 443, "/pregame/v1/matches/" + matchId + "/select/" + characterId, headers, "", true, false);

    // 2. Lock in
    auto lockRes = httpRequest("POST", session.glzHost, 443, "/pregame/v1/matches/" + matchId + "/lock/" + characterId, headers, "", true, false);
    if (lockRes.statusCode == 200) {
        return true;
    }
    if (lockRes.statusCode == 400) {
        outError = "Агент уже взят или недоступен";
    } else {
        outError = "Код ошибки: " + std::to_string(lockRes.statusCode);
    }
    return false;
}

bool sendPlayerReport(const Session& session, const Lockfile& lock, const std::string& matchId, const std::string& offenderPuuid, const std::string& agentName, std::string& outMsg) {
    if (offenderPuuid.empty()) {
        outMsg = "Игрок не найден";
        return false;
    }

    std::string reportToken = "";

    // 1. Try to fetch signed player report token from Riot Remote (Restrictions Service)
    if (!session.pdHost.empty() && !session.accessToken.empty() && !matchId.empty()) {
        std::map<std::string, std::string> pdHeaders = {
            {"Authorization", "Bearer " + session.accessToken},
            {"X-Riot-Entitlements-JWT", session.token},
            {"X-Riot-ClientPlatform", getClientPlatformBase64()},
            {"X-Riot-ClientVersion", session.clientVersion}
        };
        auto tokenRes = httpRequest("GET", session.pdHost, 443, "/restrictions/v1/playerReportToken/" + matchId + "/" + offenderPuuid, pdHeaders, "", true, false);
        if (tokenRes.statusCode == 200) {
            try {
                auto j = json::parse(tokenRes.body);
                if (j.contains("token") && j["token"].is_string()) {
                    reportToken = j["token"].get<std::string>();
                }
            } catch (...) {}
        }
    }

    bool reportedSuccessfully = false;

    // 2. Submit report to local Riot Client
    if (lock.port > 0 && !lock.password.empty()) {
        std::string localAuth = "Basic " + base64_encode("riot:" + lock.password);
        std::map<std::string, std::string> localHeaders = {
            {"Authorization", localAuth},
            {"Content-Type", "application/json"}
        };

        if (!reportToken.empty()) {
            json reportBody = {
                {"tokenType", "MATCH_TOKEN"},
                {"token", reportToken},
                {"offenderId", offenderPuuid},
                {"categories", {"cheating", "afk", "rank_abuse", "queue_manipulation", "sabotage"}},
                {"location", "in-game"},
                {"comment", "Cheating, AFK, Rank Abuse"}
            };
            auto repRes = httpRequest("POST", "127.0.0.1", lock.port, "/player-reporting/v2/report", localHeaders, reportBody.dump(), true, true);
            if (repRes.statusCode >= 200 && repRes.statusCode < 300) {
                reportedSuccessfully = true;
            }
        }

        if (!reportedSuccessfully) {
            // Unsigned report fallback on local Riot Client
            json unsignedBody = {
                {"offenderId", offenderPuuid},
                {"categories", {"cheating", "afk", "rank_abuse", "queue_manipulation", "sabotage"}},
                {"comment", "Cheating, AFK, Rank Abuse"}
            };
            auto fallbackRes = httpRequest("POST", "127.0.0.1", lock.port, "/player-reporting/v1/unsigned-report", localHeaders, unsignedBody.dump(), true, true);
            if (fallbackRes.statusCode >= 200 && fallbackRes.statusCode < 300) {
                reportedSuccessfully = true;
            }
        }
    }

    // 3. Fallback to remote restrictions endpoint if available
    if (!reportedSuccessfully && !session.pdHost.empty() && !session.accessToken.empty()) {
        std::map<std::string, std::string> pdHeaders = {
            {"Authorization", "Bearer " + session.accessToken},
            {"X-Riot-Entitlements-JWT", session.token},
            {"X-Riot-ClientPlatform", getClientPlatformBase64()},
            {"X-Riot-ClientVersion", session.clientVersion},
            {"Content-Type", "application/json"}
        };
        json remoteBody = {
            {"offenderPuuid", offenderPuuid},
            {"matchId", matchId},
            {"categories", {"cheating", "afk", "rank_abuse", "queue_manipulation", "sabotage"}},
            {"comment", "Cheating, AFK, Rank Abuse"}
        };
        auto remoteRes = httpRequest("POST", session.pdHost, 443, "/restrictions/v2/reportPlayer", pdHeaders, remoteBody.dump(), true, false);
        if (remoteRes.statusCode >= 200 && remoteRes.statusCode < 300) {
            reportedSuccessfully = true;
        }
    }

    if (reportedSuccessfully || !reportToken.empty()) {
        outMsg = "[✓] УСПЕШНО: Репорт на " + agentName + " (Читы, АФК, Абуз ранга) отправлен!";
        return true;
    } else {
        outMsg = "[ОШИБКА] Не удалось отправить репорт (код или клиент Riot недоступен)";
        return false;
    }
}

// -----------------------------------------------------------------------------
// Fetch Stats (First Tracker.gg, with automatic instant fallback to Riot Match API)
// -----------------------------------------------------------------------------
inline std::string resolveAgentName(const std::string& characterId) {
    if (characterId.empty()) return "";
    std::string lowerId = characterId;
    for (auto& c : lowerId) c = tolower(c);
    for (const auto& [k, v] : g_globalAgentMap) {
        std::string lk = k;
        for (auto& c : lk) c = tolower(c);
        if (lk == lowerId) return v;
    }
    return characterId;
}

void fetchPlayerStats(const Session& session, const std::string& puuid, const std::string& gameName, const std::string& tagLine) {
    if (!g_running || puuid.empty()) {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        g_statsFetching.erase(puuid);
        return;
    }

    PlayerStats stats;
    stats.kdRatio = -2.0f; // Default N/A
    bool trackerSuccess = false;

    std::string actualName = gameName;
    std::string actualTag = tagLine;
    if (actualName.empty() || actualName == "Player" || actualTag.empty()) {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (g_nameCache.count(puuid)) {
            actualName = g_nameCache[puuid].first;
            actualTag = g_nameCache[puuid].second;
        }
    }

    // 1. Try Tracker.gg if gameName and tagLine are present
    if (!actualName.empty() && actualName != "Player" && !actualTag.empty()) {
        std::string url = "https://api.tracker.gg/api/v2/valorant/standard/profile/riot/" + urlEncode(actualName) + "%23" + urlEncode(actualTag);
        std::string cmd = "-s --compressed -m 3 "
            "-H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36\" "
            "-H \"Accept: application/json, text/plain, */*\" "
            "-H \"Accept-Language: en-US,en;q=0.9\" "
            "-H \"Referer: https://tracker.gg/\" "
            "-H \"Origin: https://tracker.gg\" \"" + url + "\"";

        std::string response = execCurl(cmd);
        if (!response.empty() && response[0] == '{') {
            try {
                auto j = json::parse(response);
                if (j.contains("errors") && j["errors"].is_array() && !j["errors"].empty()) {
                    std::string errCode = getJsonKeyStr(j["errors"][0], {"code"});
                    std::string errMsg = getJsonKeyStr(j["errors"][0], {"message"});
                    std::string errCodeLower = errCode;
                    std::string errMsgLower = errMsg;
                    for (auto& c : errCodeLower) c = tolower(c);
                    for (auto& c : errMsgLower) c = tolower(c);
                    if (errCode == "CollectorResultStatus::Private" || 
                        errCodeLower.find("private") != std::string::npos || 
                        errMsgLower.find("private") != std::string::npos) {
                        stats.isPrivate = true;
                        stats.hasData = true;
                        stats.source = "Tracker.gg";
                        trackerSuccess = true;
                    }
                } else if (j.contains("data") && j["data"].contains("segments") && j["data"]["segments"].is_array()) {
                    for (const auto& seg : j["data"]["segments"]) {
                        std::string type = getJsonKeyStr(seg, {"type"});
                        if (seg.contains("stats") && seg["stats"].is_object()) {
                            const auto& s = seg["stats"];
                            if (s.contains("kDRatio") && stats.kdRatio < 0.0f) {
                                if (s["kDRatio"].contains("value") && s["kDRatio"]["value"].is_number()) {
                                    stats.kdRatio = s["kDRatio"]["value"].get<float>();
                                }
                                if (s.contains("headshotsPercentage") && s["headshotsPercentage"].contains("value") && s["headshotsPercentage"]["value"].is_number()) {
                                    stats.hsPercent = s["headshotsPercentage"]["value"].get<float>();
                                }
                                if (s.contains("dealtHeadshots") && s["dealtHeadshots"].contains("value") && s["dealtHeadshots"]["value"].is_number()) {
                                    stats.headshots = s["dealtHeadshots"]["value"].get<int>();
                                }
                                if (s.contains("dealtBodyshots") && s["dealtBodyshots"].contains("value") && s["dealtBodyshots"]["value"].is_number()) {
                                    stats.bodyshots = s["dealtBodyshots"]["value"].get<int>();
                                }
                                if (s.contains("dealtLegshots") && s["dealtLegshots"].contains("value") && s["dealtLegshots"]["value"].is_number()) {
                                    stats.legshots = s["dealtLegshots"]["value"].get<int>();
                                }
                                if (s.contains("matchesWon") && s["matchesWon"].contains("value") && s["matchesWon"]["value"].is_number()) {
                                    stats.matchesWon = s["matchesWon"]["value"].get<int>();
                                }
                                if (s.contains("matchesPlayed") && s["matchesPlayed"].contains("value") && s["matchesPlayed"]["value"].is_number()) {
                                    stats.matchesPlayed = s["matchesPlayed"]["value"].get<int>();
                                }
                                if (s.contains("scorePerRound") && s["scorePerRound"].contains("value") && s["scorePerRound"]["value"].is_number()) {
                                    stats.acs = (int)std::round(s["scorePerRound"]["value"].get<float>());
                                }
                                stats.hasData = true;
                                stats.source = "Tracker.gg";
                                trackerSuccess = true;
                            }
                        }
                        if (type == "agent" && seg.contains("metadata") && seg["metadata"].contains("name")) {
                            std::string agentName = getJsonKeyStr(seg["metadata"], {"name"});
                            if (stats.favAgent.empty() && !agentName.empty()) {
                                stats.favAgent = agentName;
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // 2. Fallback to Riot Match History API (works directly, no Cloudflare block, works even for private profiles)
    if (!trackerSuccess && !session.accessToken.empty() && !session.pdHost.empty()) {
        std::string clientPlatform = getClientPlatformBase64();
        std::map<std::string, std::string> pdHeaders = {
            {"Authorization", "Bearer " + session.accessToken},
            {"X-Riot-Entitlements-JWT", session.token},
            {"X-Riot-ClientPlatform", clientPlatform},
            {"X-Riot-ClientVersion", session.clientVersion}
        };

        std::string histPath = "/match-history/v1/history/" + puuid + "?startIndex=0&endIndex=20&queue=competitive";
        auto histRes = httpRequest("GET", session.pdHost, 443, histPath, pdHeaders, "", true, false);
        if (histRes.statusCode != 200 || histRes.body.find("\"History\":[]") != std::string::npos) {
            histPath = "/match-history/v1/history/" + puuid + "?startIndex=0&endIndex=20";
            histRes = httpRequest("GET", session.pdHost, 443, histPath, pdHeaders, "", true, false);
        }
        if (histRes.statusCode == 200) {
            try {
                auto histJ = json::parse(histRes.body);
                if (histJ.contains("History") && histJ["History"].is_array() && !histJ["History"].empty()) {
                    int totKills = 0;
                    int totDeaths = 0;
                    int totHs = 0;
                    int totBs = 0;
                    int totLs = 0;
                    int totWon = 0;
                    int totPlayed = 0;
                    int totScore = 0;
                    int totRounds = 0;
                    std::map<std::string, int> agentCounts;

                    for (const auto& matchItem : histJ["History"]) {
                        std::string matchId = getJsonKeyStr(matchItem, {"MatchID", "matchId"});
                        if (matchId.empty()) continue;

                        json matchJ;
                        bool hasCachedMatch = false;
                        {
                            std::lock_guard<std::mutex> lk(g_matchCacheMutex);
                            if (g_matchDetailsCache.count(matchId)) {
                                matchJ = g_matchDetailsCache[matchId];
                                hasCachedMatch = true;
                            }
                        }

                        if (!hasCachedMatch) {
                            auto mRes = httpRequest("GET", session.pdHost, 443, "/match-details/v1/matches/" + matchId, pdHeaders, "", true, false);
                            if (mRes.statusCode == 200) {
                                try {
                                    matchJ = json::parse(mRes.body);
                                    std::lock_guard<std::mutex> lk(g_matchCacheMutex);
                                    if (g_matchDetailsCache.size() > 200) {
                                        g_matchDetailsCache.erase(g_matchDetailsCache.begin());
                                    }
                                    g_matchDetailsCache[matchId] = matchJ;
                                    hasCachedMatch = true;
                                } catch (...) {}
                            }
                        }

                        if (hasCachedMatch) {
                            std::string playerTeamId = "";
                            if (matchJ.contains("players") && matchJ["players"].is_array()) {
                                for (const auto& pl : matchJ["players"]) {
                                    std::string s = getJsonKeyStr(pl, {"subject", "Subject"});
                                    if (s == puuid) {
                                        totPlayed++;
                                        playerTeamId = getJsonKeyStr(pl, {"teamId", "TeamID"});
                                        std::string charId = getJsonKeyStr(pl, {"characterId", "CharacterID"});
                                        if (!charId.empty()) agentCounts[charId]++;

                                        if (pl.contains("stats") && pl["stats"].is_object()) {
                                            const auto& st = pl["stats"];
                                            if (st.contains("kills") && st["kills"].is_number()) totKills += st["kills"].get<int>();
                                            if (st.contains("deaths") && st["deaths"].is_number()) totDeaths += st["deaths"].get<int>();
                                            if (st.contains("score") && st["score"].is_number()) totScore += st["score"].get<int>();
                                            if (st.contains("roundsPlayed") && st["roundsPlayed"].is_number()) totRounds += st["roundsPlayed"].get<int>();
                                        }
                                        break;
                                    }
                                }
                            }

                            if (!playerTeamId.empty() && matchJ.contains("teams") && matchJ["teams"].is_array()) {
                                for (const auto& tm : matchJ["teams"]) {
                                    std::string tId = getJsonKeyStr(tm, {"teamId", "TeamID"});
                                    if (tId == playerTeamId) {
                                        if (tm.contains("won") && tm["won"].is_boolean() && tm["won"].get<bool>()) {
                                            totWon++;
                                        }
                                        break;
                                    }
                                }
                            }

                            if (matchJ.contains("roundResults") && matchJ["roundResults"].is_array()) {
                                for (const auto& rr : matchJ["roundResults"]) {
                                    if (rr.contains("playerStats") && rr["playerStats"].is_array()) {
                                        for (const auto& rps : rr["playerStats"]) {
                                            std::string sub = getJsonKeyStr(rps, {"subject", "Subject"});
                                            if (sub == puuid && rps.contains("damage") && rps["damage"].is_array()) {
                                                for (const auto& dmg : rps["damage"]) {
                                                    if (dmg.contains("headshots") && dmg["headshots"].is_number()) totHs += dmg["headshots"].get<int>();
                                                    if (dmg.contains("bodyshots") && dmg["bodyshots"].is_number()) totBs += dmg["bodyshots"].get<int>();
                                                    if (dmg.contains("legshots") && dmg["legshots"].is_number()) totLs += dmg["legshots"].get<int>();
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (totPlayed > 0) {
                        stats.kdRatio = (totDeaths > 0) ? ((float)totKills / (float)totDeaths) : (float)totKills;
                        stats.headshots = totHs;
                        stats.bodyshots = totBs;
                        stats.legshots = totLs;
                        int totalShots = totHs + totBs + totLs;
                        stats.hsPercent = (totalShots > 0) ? ((float)totHs * 100.0f / (float)totalShots) : 0.0f;
                        int seasonGames = 0;
                        int seasonWins = 0;
                        {
                            std::lock_guard<std::recursive_mutex> lk(g_mutex);
                            if (g_rankCache.count(puuid)) {
                                seasonWins = g_rankCache[puuid].wins;
                                seasonGames = g_rankCache[puuid].wins + g_rankCache[puuid].losses;
                            }
                        }
                        if (seasonGames > 0) {
                            stats.matchesPlayed = seasonGames;
                            stats.matchesWon = seasonWins;
                        } else {
                            stats.matchesWon = totWon;
                            stats.matchesPlayed = totPlayed;
                        }
                        stats.acs = (totRounds > 0) ? (totScore / totRounds) : 0;
                        stats.hasData = true;
                        stats.source = "Riot Games";
                        stats.isPrivate = false;

                        std::string bestCharId = "";
                        int maxAgentCount = 0;
                        for (const auto& [cId, cnt] : agentCounts) {
                            if (cnt > maxAgentCount) {
                                maxAgentCount = cnt;
                                bestCharId = cId;
                            }
                        }
                        if (!bestCharId.empty()) {
                            stats.favAgent = resolveAgentName(bestCharId);
                        }
                    }
                }
            } catch (...) {}
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (g_statsCache.size() > 500) g_statsCache.erase(g_statsCache.begin());
        g_statsCache[puuid] = stats;
        g_statsFetching.erase(puuid);

        for (auto& p : g_liveMatchState.players) {
            if (p.puuid == puuid) {
                p.kdRatio = stats.kdRatio;
                p.hsPercent = stats.hsPercent;
                p.headshots = stats.headshots;
                p.bodyshots = stats.bodyshots;
                p.legshots = stats.legshots;
                p.matchesWon = stats.matchesWon;
                p.matchesPlayed = stats.matchesPlayed;
                p.acs = stats.acs;
                p.favAgent = stats.favAgent;
                p.isPrivate = stats.isPrivate;
                p.statsSource = stats.source;
                p.isLoading = false;
            }
        }
        if (g_selectedPuuid == puuid) {
            g_selectedPlayerInfo.kdRatio = stats.kdRatio;
            g_selectedPlayerInfo.hsPercent = stats.hsPercent;
            g_selectedPlayerInfo.headshots = stats.headshots;
            g_selectedPlayerInfo.bodyshots = stats.bodyshots;
            g_selectedPlayerInfo.legshots = stats.legshots;
            g_selectedPlayerInfo.matchesWon = stats.matchesWon;
            g_selectedPlayerInfo.matchesPlayed = stats.matchesPlayed;
            g_selectedPlayerInfo.acs = stats.acs;
            g_selectedPlayerInfo.favAgent = stats.favAgent;
            g_selectedPlayerInfo.isPrivate = stats.isPrivate;
            g_selectedPlayerInfo.statsSource = stats.source;
            g_selectedPlayerInfo.isLoading = false;
        }
    }
}

void resolveDisplayNamesAndRanks(const Session& session, std::vector<PlayerInfo>& players) {
    if (players.empty()) return;

    std::vector<std::string> uncachedNamesPuuids;
    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
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
                p.peakRankTier = g_rankCache[p.puuid].peakTier;
                p.peakRankRR = g_rankCache[p.puuid].peakRR;
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
                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                for (const auto& item : j) {
                    std::string subject = getJsonKeyStr(item, {"Subject", "subject"});
                    std::string gameName = getJsonKeyStr(item, {"GameName", "gameName"});
                    std::string tagLine = getJsonKeyStr(item, {"TagLine", "tagLine"});
                    if (gameName.empty()) gameName = "Player";
                    if (g_nameCache.size() > 500) g_nameCache.erase(g_nameCache.begin());
                    g_nameCache[subject] = {gameName, tagLine};
                }
            } catch (...) {}
        }
        {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
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

    // 2. Resolve uncached ranks (MMR)
    int mmrDelayMs = 0;
    for (auto& p : players) {
        bool shouldFetch = false;
        {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
            if (g_rankCache.count(p.puuid)) {
                p.rankTier = g_rankCache[p.puuid].tier;
                p.rankRR = g_rankCache[p.puuid].rr;
                p.wins = g_rankCache[p.puuid].wins;
                p.losses = g_rankCache[p.puuid].losses;
                p.peakRankTier = g_rankCache[p.puuid].peakTier;
                p.peakRankRR = g_rankCache[p.puuid].peakRR;
                continue;
            }
            if (!g_statsFetching.count(p.puuid + "_mmr")) {
                g_statsFetching.insert(p.puuid + "_mmr");
                shouldFetch = true;
            }
        }
        
        if (shouldFetch) {
            std::thread([=]() {
                if (mmrDelayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(mmrDelayMs));
                auto mmrRes = httpRequest("GET", session.pdHost, 443, "/mmr/v1/players/" + p.puuid, pdHeaders, "", true, false);
                int tier = 0;
                int rr = 0;
                int totalWins = 0;
                int totalGames = 0;
                int peakTier = 0;
                int peakRR = 0;

                if (mmrRes.statusCode == 200) {
                    try {
                        auto j = json::parse(mmrRes.body);

                        if (j.contains("LatestCompetitiveUpdate") && !j["LatestCompetitiveUpdate"].is_null()) {
                            auto lcu = j["LatestCompetitiveUpdate"];
                            if (lcu.contains("TierAfterUpdate") && !lcu["TierAfterUpdate"].is_null()) {
                                int t = lcu["TierAfterUpdate"].get<int>();
                                if (t > 0) tier = t;
                            }
                            if (lcu.contains("RankedRatingAfterUpdate") && !lcu["RankedRatingAfterUpdate"].is_null()) {
                                rr = lcu["RankedRatingAfterUpdate"].get<int>();
                            }
                        }

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
                                    if (sData.contains("NumberOfGames") && sData["NumberOfGames"].is_number()) {
                                        totalGames = sData["NumberOfGames"].get<int>();
                                    }
                                    if (sData.contains("NumberOfWinsWithPlacements") && sData["NumberOfWinsWithPlacements"].is_number()) {
                                        totalWins = sData["NumberOfWinsWithPlacements"].get<int>();
                                    }
                                }

                                if (tier == 0) {
                                    for (const auto& [sId, sData] : seasons.items()) {
                                        if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                            int t = sData["CompetitiveTier"].get<int>();
                                            int r = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                            if (t > tier) {
                                                tier = t;
                                                rr = r;
                                            }
                                        }
                                    }
                                }
                                
                                for (const auto& [sId, sData] : seasons.items()) {
                                    if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                        int t = sData["CompetitiveTier"].get<int>();
                                        if (t > peakTier) {
                                            peakTier = t;
                                            peakRR = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                        } else if (t == peakTier) {
                                            int r = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                            if (r > peakRR) peakRR = r;
                                        }
                                    }
                                    if (sData.contains("Rank") && sData["Rank"].is_number()) {
                                        int rk = sData["Rank"].get<int>();
                                        if (rk > peakTier) {
                                            peakTier = rk;
                                            peakRR = 0;
                                        }
                                    }
                                    if (sData.contains("WinsByTier") && sData["WinsByTier"].is_object()) {
                                        for (const auto& [tStr, wCount] : sData["WinsByTier"].items()) {
                                            try {
                                                int t = std::stoi(tStr);
                                                if (t > peakTier) {
                                                    peakTier = t;
                                                    peakRR = 0;
                                                }
                                            } catch (...) {}
                                        }
                                    }
                                }
                                if (tier > peakTier) {
                                    peakTier = tier;
                                    peakRR = rr;
                                }
                            }
                        }
                    } catch (...) {}
                }

                if (mmrRes.statusCode == 200) {
                    std::lock_guard<std::recursive_mutex> lk(g_mutex);
                    if (g_rankCache.size() > 500) g_rankCache.erase(g_rankCache.begin());
                    g_rankCache[p.puuid] = {tier, rr, totalWins, std::max(0, totalGames - totalWins), peakTier, peakRR};
                    for (auto& lp : g_liveMatchState.players) {
                        if (lp.puuid == p.puuid) {
                            lp.rankTier = tier;
                            lp.rankRR = rr;
                            lp.wins = totalWins;
                            lp.losses = std::max(0, totalGames - totalWins);
                            lp.peakRankTier = peakTier;
                            lp.peakRankRR = peakRR;
                        }
                    }
                }
                {
                    std::lock_guard<std::recursive_mutex> lk(g_mutex);
                    g_statsFetching.erase(p.puuid + "_mmr");
                }
            }).detach();
            mmrDelayMs += 100;
        }
    }

    // 3. Resolve stats (Tracker.gg with instant Riot Match API fallback)
    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        int fetchDelayMs = 0;
        for (auto& p : players) {
            if (g_statsCache.count(p.puuid)) {
                const auto& sc = g_statsCache[p.puuid];
                p.kdRatio = sc.kdRatio;
                p.hsPercent = sc.hsPercent;
                p.headshots = sc.headshots;
                p.bodyshots = sc.bodyshots;
                p.legshots = sc.legshots;
                p.matchesWon = sc.matchesWon;
                p.matchesPlayed = sc.matchesPlayed;
                p.acs = sc.acs;
                p.favAgent = sc.favAgent;
                p.isPrivate = sc.isPrivate;
                p.statsSource = sc.source;
                p.isLoading = false;
            } else if (!g_statsFetching.count(p.puuid)) {
                if (!p.puuid.empty()) {
                    g_statsFetching.insert(p.puuid);
                    p.isLoading = true;
                    std::string pGameName = p.gameName;
                    std::string pTagLine = p.tagLine;
                    std::string pPuuid = p.puuid;
                    Session pSession = session;
                    std::thread([pSession, pPuuid, pGameName, pTagLine, fetchDelayMs]() {
                        if (fetchDelayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(fetchDelayMs));
                        fetchPlayerStats(pSession, pPuuid, pGameName, pTagLine);
                    }).detach();
                    fetchDelayMs += 60;
                }
            } else {
                p.isLoading = true;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Safe URL launcher
// -----------------------------------------------------------------------------
// ShellExecute(A) resolves the default browser through shell32/windows.storage.
// On this machine (Windows 11 22000) that path crashes with an access violation
// inside windows.storage.dll (0xc0000005 at windows.storage.dll+0x203730) - the
// same crash that is visible in explorer.exe crash dumps. Opening a link used to
// take the whole application down with it, so the default browser command is
// now resolved from the registry and started directly with CreateProcessW.
// -----------------------------------------------------------------------------

static std::wstring regReadString(HKEY root, const std::wstring& subkey, const wchar_t* valueName) {
    DWORD size = 0;
    DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
    if (RegGetValueW(root, subkey.c_str(), valueName, flags, nullptr, nullptr, &size) != ERROR_SUCCESS || size < sizeof(wchar_t)) {
        return L"";
    }
    std::wstring buf(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, subkey.c_str(), valueName, flags, nullptr, buf.data(), &size) != ERROR_SUCCESS) {
        return L"";
    }
    while (!buf.empty() && buf.back() == L'\0') buf.pop_back();
    return buf;
}

static std::wstring expandEnv(const std::wstring& s) {
    if (s.empty() || s.find(L'%') == std::wstring::npos) return s;
    DWORD len = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
    if (len == 0) return s;
    std::wstring out(len, L'\0');
    if (ExpandEnvironmentStringsW(s.c_str(), out.data(), len) == 0) return s;
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

static std::wstring urlProgId(const wchar_t* scheme) {
    std::wstring key = L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\";
    key += scheme;
    key += L"\\UserChoice";
    return regReadString(HKEY_CURRENT_USER, key, L"ProgId");
}

static std::wstring urlOpenCommand(const std::wstring& progId) {
    if (progId.empty()) return L"";
    std::wstring sub = progId + L"\\shell\\open\\command";
    std::wstring cmd = regReadString(HKEY_CLASSES_ROOT, sub, nullptr);
    if (cmd.empty()) cmd = regReadString(HKEY_CURRENT_USER, L"Software\\Classes\\" + sub, nullptr);
    if (cmd.empty()) cmd = regReadString(HKEY_LOCAL_MACHINE, L"Software\\Classes\\" + sub, nullptr);
    return cmd;
}

static std::wstring substituteUrl(const std::wstring& cmdTemplate, const std::wstring& rawUrl) {
    std::wstring out;
    out.reserve(cmdTemplate.size() + rawUrl.size() + 8);
    bool replaced = false;
    for (size_t i = 0; i < cmdTemplate.size(); ++i) {
        if (cmdTemplate[i] == L'%' && i + 1 < cmdTemplate.size() &&
            (cmdTemplate[i + 1] == L'1' || cmdTemplate[i + 1] == L'l' || cmdTemplate[i + 1] == L'L')) {
            out += rawUrl;
            ++i;
            replaced = true;
        } else {
            out += cmdTemplate[i];
        }
    }
    if (!replaced) {
        if (cmdTemplate.find(L"--single-argument") != std::wstring::npos) {
            out += L" " + rawUrl;
        } else {
            out += L" \"" + rawUrl + L"\"";
        }
    }
    return out;
}

static bool startDetachedProcess(const std::wstring& cmdline) {
    if (cmdline.empty()) return false;
    std::wstring cmd = cmdline;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

static std::wstring envVar(const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(name, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return L"";
    return std::wstring(buf, n);
}

static bool launchUrlInBrowser(const std::string& url) {
    if (url.empty()) return false;
    std::wstring wideUrl = utf8ToWide(url);
    std::wstring quotedUrl = L"\"" + wideUrl + L"\"";

    // 1) Use the registered default handler (Chrome/Edge/Firefox/... command line)
    for (const wchar_t* scheme : {L"https", L"http"}) {
        std::wstring progId = urlProgId(scheme);
        if (progId.empty()) continue;
        std::wstring cmd = urlOpenCommand(progId);
        if (cmd.empty()) continue;
        cmd = expandEnv(cmd);
        if (startDetachedProcess(substituteUrl(cmd, wideUrl))) {
            Log("Opened URL via registered handler (" + std::string(progId.begin(), progId.end()) + ")");
            return true;
        }
    }

    // 2) Well-known browser locations as a fallback
    const std::wstring pf = envVar(L"ProgramFiles");
    const std::wstring pf86 = envVar(L"ProgramFiles(x86)");
    const std::wstring local = envVar(L"LOCALAPPDATA");
    std::vector<std::wstring> exeCandidates;
    auto addBrowser = [&exeCandidates](const std::wstring& dir, const std::wstring& rel) {
        if (dir.empty()) return;
        std::wstring path = dir + L"\\" + rel;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) exeCandidates.push_back(path);
    };
    addBrowser(pf, L"Google\\Chrome\\Application\\chrome.exe");
    addBrowser(pf86, L"Google\\Chrome\\Application\\chrome.exe");
    addBrowser(local, L"Google\\Chrome\\Application\\chrome.exe");
    addBrowser(pf, L"Microsoft\\Edge\\Application\\msedge.exe");
    addBrowser(pf86, L"Microsoft\\Edge\\Application\\msedge.exe");
    addBrowser(pf, L"Mozilla Firefox\\firefox.exe");
    addBrowser(pf86, L"Mozilla Firefox\\firefox.exe");
    addBrowser(pf, L"BraveSoftware\\Brave-Browser\\Application\\brave.exe");
    addBrowser(local, L"BraveSoftware\\Brave-Browser\\Application\\brave.exe");
    addBrowser(pf, L"Yandex\\YandexBrowser\\Application\\browser.exe");
    addBrowser(local, L"Yandex\\YandexBrowser\\Application\\browser.exe");
    addBrowser(local, L"Programs\\Opera\\opera.exe");
    addBrowser(pf, L"Vivaldi\\Application\\vivaldi.exe");
    for (const auto& exe : exeCandidates) {
        if (startDetachedProcess(L"\"" + exe + L"\" \"" + wideUrl + L"\"")) {
            Log("Opened URL via browser fallback: " + std::string(exe.begin(), exe.end()));
            return true;
        }
    }

    // 3) Last resort: let cmd.exe do it in a separate process, so a broken shell
    //    can no longer crash our application.
    std::wstring comspec = envVar(L"COMSPEC");
    if (comspec.empty()) comspec = L"cmd.exe";
    if (startDetachedProcess(L"\"" + comspec + L"\" /c start \"\" " + quotedUrl)) {
        Log("Opened URL via cmd.exe fallback");
        return true;
    }

    Log("Failed to open URL in browser: " + url);
    return false;
}

void openTrackerInBrowser(std::string gName, std::string tLine) {
    if (gName.empty() && !g_selectedPuuid.empty()) {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (g_nameCache.count(g_selectedPuuid)) {
            gName = g_nameCache[g_selectedPuuid].first;
            if (tLine.empty()) tLine = g_nameCache[g_selectedPuuid].second;
        }
    }
    if (gName.empty()) return;
    size_t hashPos = gName.find('#');
    if (hashPos != std::string::npos) {
        if (tLine.empty()) tLine = gName.substr(hashPos + 1);
        gName = gName.substr(0, hashPos);
    }
    if (!tLine.empty() && tLine[0] == '#') {
        tLine = tLine.substr(1);
    }
    std::string url = "https://tracker.gg/valorant/profile/riot/" + urlEncode(gName) + "%23" + urlEncode(tLine) + "/overview";
    std::thread([url]() {
        launchUrlInBrowser(url);
    }).detach();
}

#include "discord_rpc.hpp"
#include "ftxui_render.hpp"
#include "ftxui_skins_view.hpp"
#include "ftxui_stats_view.hpp"
#include "ftxui_agent_picker.hpp"
#include "ftxui_report_menu.hpp"
#include "ftxui_chat_menu.hpp"

PlayerProfileTab g_playerProfileTab = PlayerProfileTab::STATS;
ftxui::Box g_tabSkinsBox;
ftxui::Box g_tabStatsBox;
std::map<std::string, PlayerWeaponLoadout> g_playerLoadouts;
std::recursive_mutex g_loadoutsMutex;

struct DefaultWeaponMeta {
    std::string uuid;
    std::string name;
    std::string category;
    int catOrder;
    int weaponOrder;
};

static const std::vector<DefaultWeaponMeta> DEFAULT_WEAPONS = {
    {"29a0cfab-485b-f5d5-779a-b59f85e204a8", "Classic", "ПИСТОЛЕТЫ", 1, 1},
    {"42da8ccc-40d5-affc-beec-15aa47b42eda", "Shorty", "ПИСТОЛЕТЫ", 1, 2},
    {"44d4e95c-4157-0037-81b2-17841bf2e8e3", "Frenzy", "ПИСТОЛЕТЫ", 1, 3},
    {"1baa85b4-4c70-1284-64bb-6481dfc3bb4e", "Ghost", "ПИСТОЛЕТЫ", 1, 4},
    {"e336c6b8-418d-9340-d77f-7a9e4cfe0702", "Sheriff", "ПИСТОЛЕТЫ", 1, 5},
    {"f7e1b454-4ad4-1063-ec0a-159e56b58941", "Stinger", "ПИСТОЛЕТЫ-ПУЛЕМЁТЫ", 2, 6},
    {"462080d1-4035-2937-7c09-27aa2a5c27a7", "Spectre", "ПИСТОЛЕТЫ-ПУЛЕМЁТЫ", 2, 7},
    {"910be174-449b-c412-ab22-d0873436b21b", "Bucky", "ДРОБОВИКИ", 3, 8},
    {"ec845bf4-4f79-ddda-a3da-0db3774b2794", "Judge", "ДРОБОВИКИ", 3, 9},
    {"ae3de142-4d85-2547-dd26-4e90bed35cf7", "Bulldog", "ВИНТОВКИ", 4, 10},
    {"4ade7faa-4cf1-8376-95ef-39884480959b", "Guardian", "ВИНТОВКИ", 4, 11},
    {"ee8e8d15-496b-07ac-e5f6-8fae5d4c7b1a", "Phantom", "ВИНТОВКИ", 4, 12},
    {"9c82e19d-4575-0200-1a81-3eacf00cf872", "Vandal", "ВИНТОВКИ", 4, 13},
    {"c4883e50-4494-202c-3ec3-6b8a9284f00b", "Marshal", "СНАЙПЕРСКИЕ ВИНТОВКИ", 5, 14},
    {"5f0aaf7a-4289-3998-d5ff-eb9a5cf7ef5c", "Outlaw", "СНАЙПЕРСКИЕ ВИНТОВКИ", 5, 15},
    {"a03b24d3-4319-996d-0f8c-94bbfba1dfc7", "Operator", "СНАЙПЕРСКИЕ ВИНТОВКИ", 5, 16},
    {"55d8a0f4-4274-ca67-fe2c-06ab45efdf58", "Ares", "ПУЛЕМЕТЫ", 6, 17},
    {"63e6c2b6-4a8e-869c-3d4c-e38355226584", "Odin", "ПУЛЕМЕТЫ", 6, 18},
    {"2f59173c-4bed-b6c3-2191-dea9b58be9c7", "Нож", "ХОЛОДНОЕ ОРУЖИЕ", 7, 19}
};

static bool isPlayableWeapon(const std::string& weaponUuid) {
    std::string wLower = weaponUuid;
    for (auto& c : wLower) c = tolower(c);
    for (const auto& dw : DEFAULT_WEAPONS) {
        std::string dLower = dw.uuid;
        for (auto& c : dLower) c = tolower(c);
        if (dLower == wLower) return true;
    }
    return false;
}

struct SkinMetadata {
    std::string skinUuid;
    std::string skinName;
    std::string weaponUuid;
    bool isStandard = false;
};

struct ChromaMetadata {
    std::string chromaUuid;
    std::string skinUuid;
    std::string variantName;
    int chromaIndex = 0;
    int totalChromas = 1;
};

static std::map<std::string, SkinMetadata> g_skinMetaMap;
static std::map<std::string, ChromaMetadata> g_chromaMetaMap;
static std::map<std::string, std::string> g_weaponNameMap;
static std::map<std::string, std::string> g_weaponCategoryMap;
static std::mutex g_weaponDbMutex;
static std::atomic<bool> g_weaponDbLoaded{false};

static std::string extractChromaVariant(const std::string& chromaDisplayName, const std::string& skinDisplayName) {
    if (chromaDisplayName.empty()) return "";

    std::string clean = chromaDisplayName;
    for (auto& c : clean) {
        if (c == '\r' || c == '\n') c = ' ';
    }
    
    // Look for (расцветка ... "VARIANT") or ("VARIANT")
    size_t lastQuote = clean.rfind('"');
    if (lastQuote != std::string::npos && lastQuote > 0) {
        size_t prevQuote = clean.rfind('"', lastQuote - 1);
        if (prevQuote != std::string::npos && prevQuote < lastQuote) {
            std::string variant = clean.substr(prevQuote + 1, lastQuote - prevQuote - 1);
            if (variant != skinDisplayName && !variant.empty() && variant.length() < 35) {
                return variant;
            }
        }
    }
    
    size_t pOpen = clean.rfind('(');
    size_t pClose = clean.rfind(')');
    if (pOpen != std::string::npos && pClose != std::string::npos && pClose > pOpen) {
        std::string inner = clean.substr(pOpen + 1, pClose - pOpen - 1);
        size_t q1 = inner.find('"');
        size_t q2 = inner.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
            return inner.substr(q1 + 1, q2 - q1 - 1);
        }
        return inner;
    }
    
    return "";
}

static void parseWeaponsJson(const std::string& jsonStr) {
    try {
        auto root = json::parse(jsonStr);
        if (!root.contains("data") || !root["data"].is_array()) return;
        
        std::lock_guard<std::mutex> lk(g_weaponDbMutex);
        for (const auto& w : root["data"]) {
            std::string wUuid = getJsonKeyStr(w, {"uuid"});
            std::string wName = getJsonKeyStr(w, {"displayName"});
            std::string wCat = getJsonKeyStr(w, {"category"});
            if (wUuid.empty()) continue;
            if (!isPlayableWeapon(wUuid)) continue;
            
            std::string catClean = "ДРУГОЕ";
            if (wCat.find("Sidearm") != std::string::npos) catClean = "ПИСТОЛЕТЫ";
            else if (wCat.find("SMG") != std::string::npos) catClean = "ПИСТОЛЕТЫ-ПУЛЕМЁТЫ";
            else if (wCat.find("Shotgun") != std::string::npos) catClean = "ДРОБОВИКИ";
            else if (wCat.find("Rifle") != std::string::npos) catClean = "ВИНТОВКИ";
            else if (wCat.find("Sniper") != std::string::npos) catClean = "СНАЙПЕРСКИЕ ВИНТОВКИ";
            else if (wCat.find("Heavy") != std::string::npos) catClean = "ПУЛЕМЕТЫ";
            else if (wCat.find("Melee") != std::string::npos) catClean = "ХОЛОДНОЕ ОРУЖИЕ";

            if (wCat.find("Melee") != std::string::npos && (wName == "Холодное оружие" || wName.empty())) {
                wName = "Нож";
            }

            g_weaponNameMap[wUuid] = wName;
            g_weaponCategoryMap[wUuid] = catClean;
            
            if (w.contains("skins") && w["skins"].is_array()) {
                for (const auto& s : w["skins"]) {
                    std::string sUuid = getJsonKeyStr(s, {"uuid"});
                    std::string sName = getJsonKeyStr(s, {"displayName"});
                    if (sUuid.empty()) continue;
                    
                    bool isStd = (sName.find("Стандарт") != std::string::npos ||
                                  sName.find("Standard") != std::string::npos ||
                                  sName == wName ||
                                  sName == "Melee");
                    
                    SkinMetadata sm;
                    sm.skinUuid = sUuid;
                    sm.skinName = sName;
                    sm.weaponUuid = wUuid;
                    sm.isStandard = isStd;
                    
                    std::string sUuidLower = sUuid;
                    for (auto& ch : sUuidLower) ch = tolower(ch);
                    g_skinMetaMap[sUuid] = sm;
                    g_skinMetaMap[sUuidLower] = sm;
                    
                    if (s.contains("levels") && s["levels"].is_array()) {
                        for (const auto& l : s["levels"]) {
                            std::string lUuid = getJsonKeyStr(l, {"uuid"});
                            if (!lUuid.empty()) {
                                std::string lUuidLower = lUuid;
                                for (auto& ch : lUuidLower) ch = tolower(ch);
                                g_skinMetaMap[lUuid] = sm;
                                g_skinMetaMap[lUuidLower] = sm;
                            }
                        }
                    }

                    if (s.contains("chromas") && s["chromas"].is_array()) {
                        size_t totalChromas = s["chromas"].size();
                        for (size_t cIdx = 0; cIdx < totalChromas; ++cIdx) {
                            const auto& c = s["chromas"][cIdx];
                            std::string cUuid = getJsonKeyStr(c, {"uuid"});
                            std::string cName = getJsonKeyStr(c, {"displayName"});
                            if (cUuid.empty()) continue;
                            
                            std::string cUuidLower = cUuid;
                            for (auto& ch : cUuidLower) ch = tolower(ch);

                            ChromaMetadata cm;
                            cm.chromaUuid = cUuid;
                            cm.skinUuid = sUuid;
                            cm.chromaIndex = (int)(cIdx + 1);
                            cm.totalChromas = (int)totalChromas;

                            if (totalChromas > 1) {
                                if (cIdx == 0) {
                                    cm.variantName = "Базовая";
                                } else {
                                    std::string v = extractChromaVariant(cName, sName);
                                    if (v.empty()) {
                                        v = "Расцветка " + std::to_string(cIdx + 1);
                                    }
                                    cm.variantName = v;
                                }
                            } else {
                                cm.variantName = "";
                            }

                            g_chromaMetaMap[cUuid] = cm;
                            g_chromaMetaMap[cUuidLower] = cm;

                            // Also index chroma in skinMetaMap as fallback
                            if (!g_skinMetaMap.count(cUuid)) {
                                g_skinMetaMap[cUuid] = sm;
                                g_skinMetaMap[cUuidLower] = sm;
                            }
                        }
                    }
                }
            }
        }
        g_weaponDbLoaded = true;
    } catch (...) {}
}

static void initWeaponsDatabase() {
    std::string cachePath = getAppDataDir() + "\\weapons_cache.json";
    bool loadedFromDisk = false;
    
    std::ifstream f(cachePath);
    if (!f.is_open()) {
        std::string fallback = getExeDir() + "\\weapons_cache.json";
        f.open(fallback);
    }
    if (f.is_open()) {
        std::stringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        if (content.length() > 1000) {
            parseWeaponsJson(content);
            loadedFromDisk = true;
        }
    }
    
    std::thread([cachePath, loadedFromDisk]() {
        if (loadedFromDisk) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/weapons?language=ru-RU", {}, "", true, false);
        if (res.statusCode == 200 && res.body.length() > 1000) {
            parseWeaponsJson(res.body);
            std::ofstream out(cachePath, std::ios::binary);
            if (out.is_open()) {
                out << res.body;
            }
        }
    }).detach();
}

static WeaponSkinInfo resolveWeaponSkin(const std::string& weaponUuid, const std::string& skinUuid, const std::string& chromaUuid) {
    WeaponSkinInfo wsi;
    wsi.weaponUuid = weaponUuid;
    wsi.skinUuid = skinUuid;
    wsi.chromaUuid = chromaUuid;

    std::string wName = "Оружие";
    std::string wCat = "ВИНТОВКИ";
    int catOrd = 4;
    int wOrd = 10;

    std::string sUuidLower = skinUuid;
    for (auto& c : sUuidLower) c = tolower(c);
    std::string cUuidLower = chromaUuid;
    for (auto& c : cUuidLower) c = tolower(c);
    std::string wUuidLower = weaponUuid;
    for (auto& c : wUuidLower) c = tolower(c);

    for (const auto& dw : DEFAULT_WEAPONS) {
        if (_stricmp(dw.uuid.c_str(), weaponUuid.c_str()) == 0) {
            wName = dw.name;
            wCat = dw.category;
            catOrd = dw.catOrder;
            wOrd = dw.weaponOrder;
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lk(g_weaponDbMutex);
        if (g_weaponNameMap.count(weaponUuid)) {
            wName = g_weaponNameMap[weaponUuid];
        } else if (g_weaponNameMap.count(wUuidLower)) {
            wName = g_weaponNameMap[wUuidLower];
        }
        if (g_weaponCategoryMap.count(weaponUuid)) {
            wCat = g_weaponCategoryMap[weaponUuid];
        } else if (g_weaponCategoryMap.count(wUuidLower)) {
            wCat = g_weaponCategoryMap[wUuidLower];
        }
    }

    wsi.weaponName = wName;
    wsi.category = wCat;
    wsi.categoryOrder = catOrd;
    wsi.weaponOrder = wOrd;

    std::string sName = "";
    bool isStd = true;
    {
        std::lock_guard<std::mutex> lk(g_weaponDbMutex);
        if (g_skinMetaMap.count(skinUuid)) {
            sName = g_skinMetaMap[skinUuid].skinName;
            isStd = g_skinMetaMap[skinUuid].isStandard;
        } else if (g_skinMetaMap.count(sUuidLower)) {
            sName = g_skinMetaMap[sUuidLower].skinName;
            isStd = g_skinMetaMap[sUuidLower].isStandard;
        }
    }

    if (sName.empty() || isStd) {
        wsi.skinName = "Стандартный " + wName;
        wsi.isCustom = false;
        wsi.fullName = wsi.skinName;
    } else {
        wsi.skinName = sName;
        wsi.isCustom = true;
        
        std::string varName = "";
        bool hasChromas = false;
        {
            std::lock_guard<std::mutex> lk(g_weaponDbMutex);
            if (g_chromaMetaMap.count(chromaUuid)) {
                varName = g_chromaMetaMap[chromaUuid].variantName;
                hasChromas = (g_chromaMetaMap[chromaUuid].totalChromas > 1);
            } else if (g_chromaMetaMap.count(cUuidLower)) {
                varName = g_chromaMetaMap[cUuidLower].variantName;
                hasChromas = (g_chromaMetaMap[cUuidLower].totalChromas > 1);
            }
        }
        wsi.chromaName = varName;
        wsi.hasChromas = hasChromas;
        if (!varName.empty() && varName != "Базовая") {
            wsi.fullName = sName + " (" + varName + ")";
        } else {
            wsi.fullName = sName;
        }
    }

    return wsi;
}

static bool getPlayerLoadout(const std::string& puuid, PlayerWeaponLoadout& out) {
    if (puuid.empty()) return false;
    std::string low = puuid;
    for (auto& c : low) c = tolower(c);
    std::string up = puuid;
    for (auto& c : up) c = toupper(c);

    std::lock_guard<std::recursive_mutex> lk(g_loadoutsMutex);
    if (g_playerLoadouts.count(puuid) && g_playerLoadouts[puuid].loaded) {
        out = g_playerLoadouts[puuid];
        return true;
    }
    if (g_playerLoadouts.count(low) && g_playerLoadouts[low].loaded) {
        out = g_playerLoadouts[low];
        return true;
    }
    if (g_playerLoadouts.count(up) && g_playerLoadouts[up].loaded) {
        out = g_playerLoadouts[up];
        return true;
    }
    return false;
}

static void parseMatchLoadoutsResponse(const std::string& jsonBody, const Session& session) {
    try {
        auto j = json::parse(jsonBody);
        
        if (j.contains("Loadouts") && j["Loadouts"].is_array()) {
            for (const auto& entry : j["Loadouts"]) {
                std::string subject = "";
                json jLoadout;
                if (entry.contains("Subject") && entry["Subject"].is_string()) {
                    subject = entry["Subject"].get<std::string>();
                }
                if (entry.contains("Loadout") && entry["Loadout"].is_object()) {
                    jLoadout = entry["Loadout"];
                    if (subject.empty() && jLoadout.contains("Subject") && jLoadout["Subject"].is_string()) {
                        subject = jLoadout["Subject"].get<std::string>();
                    }
                } else {
                    jLoadout = entry;
                }
                if (subject.empty()) continue;

                PlayerWeaponLoadout pLoadout;
                pLoadout.puuid = subject;
                pLoadout.loaded = true;

                for (const auto& dw : DEFAULT_WEAPONS) {
                    WeaponSkinInfo wsi;
                    wsi.weaponUuid = dw.uuid;
                    wsi.weaponName = dw.name;
                    wsi.category = dw.category;
                    wsi.categoryOrder = dw.catOrder;
                    wsi.weaponOrder = dw.weaponOrder;
                    wsi.skinName = "Стандартный " + dw.name;
                    wsi.fullName = wsi.skinName;
                    wsi.isCustom = false;
                    pLoadout.weapons[dw.uuid] = wsi;
                }

                if (jLoadout.contains("Items") && jLoadout["Items"].is_object()) {
                    for (const auto& [weaponId, itemObj] : jLoadout["Items"].items()) {
                        if (!isPlayableWeapon(weaponId)) continue;
                        std::string skinId = "";
                        std::string chromaId = "";
                        if (itemObj.contains("Sockets")) {
                            if (itemObj["Sockets"].is_object()) {
                                for (const auto& [sockKey, sockVal] : itemObj["Sockets"].items()) {
                                    std::string sId = sockKey;
                                    if (sockVal.contains("ID") && sockVal["ID"].is_string()) {
                                        sId = sockVal["ID"].get<std::string>();
                                    }
                                    std::string itemId = "";
                                    if (sockVal.contains("Item") && sockVal["Item"].is_object() && sockVal["Item"].contains("ID") && sockVal["Item"]["ID"].is_string()) {
                                        itemId = sockVal["Item"]["ID"].get<std::string>();
                                    }
                                    if (_stricmp(sId.c_str(), "bcef87d6-209b-46c6-8b19-fbe40bd95abc") == 0) {
                                        skinId = itemId;
                                    } else if (_stricmp(sId.c_str(), "3ad1b2b2-acdb-4524-852f-954a76ddae0a") == 0) {
                                        chromaId = itemId;
                                    }
                                }
                            } else if (itemObj["Sockets"].is_array()) {
                                for (const auto& sockVal : itemObj["Sockets"]) {
                                    std::string sId = getJsonKeyStr(sockVal, {"ID", "id"});
                                    std::string itemId = "";
                                    if (sockVal.contains("Item") && sockVal["Item"].is_object()) {
                                        itemId = getJsonKeyStr(sockVal["Item"], {"ID", "id"});
                                    }
                                    if (_stricmp(sId.c_str(), "bcef87d6-209b-46c6-8b19-fbe40bd95abc") == 0) {
                                        skinId = itemId;
                                    } else if (_stricmp(sId.c_str(), "3ad1b2b2-acdb-4524-852f-954a76ddae0a") == 0) {
                                        chromaId = itemId;
                                    }
                                }
                            }
                        }
                        if (skinId.empty()) skinId = getJsonKeyStr(itemObj, {"SkinID", "skinId"});
                        if (chromaId.empty()) chromaId = getJsonKeyStr(itemObj, {"ChromaID", "chromaId"});

                        std::string canonWeaponUuid = weaponId;
                        for (const auto& dw : DEFAULT_WEAPONS) {
                            if (_stricmp(dw.uuid.c_str(), weaponId.c_str()) == 0) {
                                canonWeaponUuid = dw.uuid;
                                break;
                            }
                        }
                        WeaponSkinInfo wsi = resolveWeaponSkin(canonWeaponUuid, skinId, chromaId);
                        pLoadout.weapons[canonWeaponUuid] = wsi;
                    }
                }
                
                {
                    std::string sLower = subject;
                    for (auto& c : sLower) c = tolower(c);
                    std::string sUpper = subject;
                    for (auto& c : sUpper) c = toupper(c);

                    std::lock_guard<std::recursive_mutex> lk(g_loadoutsMutex);
                    g_playerLoadouts[subject] = pLoadout;
                    g_playerLoadouts[sLower] = pLoadout;
                    g_playerLoadouts[sUpper] = pLoadout;
                }
            }
        }
        
        if (j.contains("Guns") && j["Guns"].is_array()) {
            std::string subject = getJsonKeyStr(j, {"Subject", "subject"});
            if (subject.empty()) subject = session.puuid;

            PlayerWeaponLoadout pLoadout;
            pLoadout.puuid = subject;
            pLoadout.loaded = true;

            for (const auto& dw : DEFAULT_WEAPONS) {
                WeaponSkinInfo wsi;
                wsi.weaponUuid = dw.uuid;
                wsi.weaponName = dw.name;
                wsi.category = dw.category;
                wsi.categoryOrder = dw.catOrder;
                wsi.weaponOrder = dw.weaponOrder;
                wsi.skinName = "Стандартный " + dw.name;
                wsi.fullName = wsi.skinName;
                wsi.isCustom = false;
                pLoadout.weapons[dw.uuid] = wsi;
            }

            for (const auto& gunObj : j["Guns"]) {
                std::string weaponId = getJsonKeyStr(gunObj, {"ID", "id"});
                if (!isPlayableWeapon(weaponId)) continue;
                std::string skinId = getJsonKeyStr(gunObj, {"SkinID", "skinId"});
                std::string chromaId = getJsonKeyStr(gunObj, {"ChromaID", "chromaId"});
                WeaponSkinInfo wsi = resolveWeaponSkin(weaponId, skinId, chromaId);
                pLoadout.weapons[weaponId] = wsi;
            }

            {
                std::string sLower = subject;
                for (auto& c : sLower) c = tolower(c);
                std::string sUpper = subject;
                for (auto& c : sUpper) c = toupper(c);

                std::lock_guard<std::recursive_mutex> lk(g_loadoutsMutex);
                g_playerLoadouts[subject] = pLoadout;
                g_playerLoadouts[sLower] = pLoadout;
                g_playerLoadouts[sUpper] = pLoadout;
            }
        }
    } catch (...) {}
}

static void fetchPlayerLoadoutDirect(const Session& session, const std::string& puuid) {
    if (session.accessToken.empty() || puuid.empty()) return;
    
    bool needsFetch = false;
    {
        std::lock_guard<std::recursive_mutex> lk(g_loadoutsMutex);
        if (!g_playerLoadouts.count(puuid) || !g_playerLoadouts[puuid].loaded) {
            needsFetch = true;
        }
    }
    if (!needsFetch) return;

    std::thread([session, puuid]() {
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + session.accessToken},
            {"X-Riot-Entitlements-JWT", session.token},
            {"X-Riot-ClientPlatform", getClientPlatformBase64()},
            {"X-Riot-ClientVersion", session.clientVersion}
        };

        if (puuid == session.puuid && !session.pdHost.empty()) {
            auto res = httpRequest("GET", session.pdHost, 443, "/personalization/v2/players/" + puuid + "/playerloadout", headers, "", true, false);
            if (res.statusCode == 200) {
                parseMatchLoadoutsResponse(res.body, session);
                return;
            }
        }

        std::string matchId;
        std::string phase;
        {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
            matchId = g_liveMatchState.matchId;
            phase = g_liveMatchState.phase;
        }

        if (!matchId.empty() && !session.glzHost.empty()) {
            if (phase == "coregame") {
                auto res = httpRequest("GET", session.glzHost, 443, "/core-game/v1/matches/" + matchId + "/loadouts", headers, "", true, false);
                if (res.statusCode == 200) {
                    parseMatchLoadoutsResponse(res.body, session);
                    return;
                }
            } else if (phase == "pregame") {
                auto res = httpRequest("GET", session.glzHost, 443, "/pregame/v1/matches/" + matchId + "/loadouts", headers, "", true, false);
                if (res.statusCode == 200) {
                    parseMatchLoadoutsResponse(res.body, session);
                    return;
                }
            }
        }
    }).detach();
}

static std::string g_lastLoadedMatchId = "";
static std::chrono::steady_clock::time_point g_lastLoadoutFetchTime;

static void updateMatchLoadouts(const Session& session, const MatchState& state) {
    if (session.accessToken.empty() || state.matchId.empty()) return;
    if (state.phase != "coregame" && state.phase != "pregame") return;

    auto now = std::chrono::steady_clock::now();
    bool matchChanged = (g_lastLoadedMatchId != state.matchId);
    if (matchChanged) {
        std::lock_guard<std::recursive_mutex> lk(g_loadoutsMutex);
        g_playerLoadouts.clear();
        g_lastLoadedMatchId = state.matchId;
    }

    bool allLoaded = true;
    int playersWithPuuid = 0;
    for (const auto& p : state.players) {
        if (p.puuid.empty()) continue;
        playersWithPuuid++;
        PlayerWeaponLoadout dummy;
        if (!getPlayerLoadout(p.puuid, dummy)) {
            allLoaded = false;
        }
    }

    if (playersWithPuuid > 0 && allLoaded && !matchChanged) {
        return;
    }

    if (!matchChanged && std::chrono::duration_cast<std::chrono::seconds>(now - g_lastLoadoutFetchTime).count() < 2) {
        return;
    }

    g_lastLoadoutFetchTime = now;
    std::string matchId = state.matchId;
    std::string phase = state.phase;

    std::thread([session, matchId, phase]() {
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + session.accessToken},
            {"X-Riot-Entitlements-JWT", session.token},
            {"X-Riot-ClientPlatform", getClientPlatformBase64()},
            {"X-Riot-ClientVersion", session.clientVersion}
        };

        if (phase == "coregame" && !session.glzHost.empty()) {
            auto res = httpRequest("GET", session.glzHost, 443, "/core-game/v1/matches/" + matchId + "/loadouts", headers, "", true, false);
            if (res.statusCode == 200) {
                parseMatchLoadoutsResponse(res.body, session);
            }
        } else if (phase == "pregame" && !session.glzHost.empty()) {
            auto res = httpRequest("GET", session.glzHost, 443, "/pregame/v1/matches/" + matchId + "/loadouts", headers, "", true, false);
            if (res.statusCode == 200) {
                parseMatchLoadoutsResponse(res.body, session);
            }
        }

        if (!session.puuid.empty() && !session.pdHost.empty()) {
            auto res = httpRequest("GET", session.pdHost, 443, "/personalization/v2/players/" + session.puuid + "/playerloadout", headers, "", true, false);
            if (res.statusCode == 200) {
                parseMatchLoadoutsResponse(res.body, session);
            }
        }
    }).detach();
}

struct DetectedPartyInfo {
    int partyNumber = 0;
    int partySize = 0;
    int sharedMatches = 0;
    std::string partyLabel = "";
    std::vector<std::string> partnerNames;
    std::string partyId = "";
};

static std::map<std::string, std::vector<std::string>> g_playerMatchHistoryCache;
static std::map<std::string, DetectedPartyInfo> g_detectedParties;
static std::recursive_mutex g_partyDetectionMutex;
static std::string g_lastPartyCheckMatchId = "";
static bool g_partyCheckRunning = false;
static std::chrono::steady_clock::time_point g_lastPartyCheckTime;

static void detectPartiesFromMatchHistory(const Session& session, const MatchState& state) {
    if (session.accessToken.empty() || session.pdHost.empty()) return;
    if (state.matchId.empty() || (state.phase != "coregame" && state.phase != "pregame")) return;

    auto now = std::chrono::steady_clock::now();
    bool matchChanged = (g_lastPartyCheckMatchId != state.matchId);
    
    bool allCached = true;
    {
        std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
        if (g_partyCheckRunning) return;
        for (const auto& p : state.players) {
            if (!p.puuid.empty() && !g_playerMatchHistoryCache.count(p.puuid)) {
                allCached = false;
                break;
            }
        }
    }

    if (!matchChanged && allCached) return;
    if (!matchChanged && std::chrono::duration_cast<std::chrono::seconds>(now - g_lastPartyCheckTime).count() < 4) {
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
        g_partyCheckRunning = true;
        g_lastPartyCheckMatchId = state.matchId;
        g_lastPartyCheckTime = now;
    }

    MatchState stateCopy = state;
    Session sessionCopy = session;

    std::thread([sessionCopy, stateCopy]() {
        try {
            std::string clientPlatform = getClientPlatformBase64();
            std::map<std::string, std::string> pdHeaders = {
                {"Authorization", "Bearer " + sessionCopy.accessToken},
                {"X-Riot-Entitlements-JWT", sessionCopy.token},
                {"X-Riot-ClientPlatform", clientPlatform},
                {"X-Riot-ClientVersion", sessionCopy.clientVersion}
            };

            // 1. Fetch recent 20 match history for players who are not cached yet
            for (const auto& p : stateCopy.players) {
                if (p.puuid.empty()) continue;
                bool needsFetch = false;
                {
                    std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
                    if (!g_playerMatchHistoryCache.count(p.puuid)) {
                        needsFetch = true;
                    }
                }
                if (needsFetch) {
                    auto histRes = httpRequest("GET", sessionCopy.pdHost, 443, "/match-history/v1/history/" + p.puuid + "?startIndex=0&endIndex=20", pdHeaders, "", true, false);
                    if (histRes.statusCode == 200) {
                        try {
                            auto histJ = json::parse(histRes.body);
                            std::vector<std::string> mIds;
                            if (histJ.contains("History") && histJ["History"].is_array()) {
                                for (const auto& item : histJ["History"]) {
                                    std::string mid = getJsonKeyStr(item, {"MatchID", "matchId"});
                                    if (!mid.empty()) {
                                        mIds.push_back(mid);
                                    }
                                }
                            }
                            {
                                std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
                                if (g_playerMatchHistoryCache.size() > 500) {
                                    g_playerMatchHistoryCache.erase(g_playerMatchHistoryCache.begin());
                                }
                                g_playerMatchHistoryCache[p.puuid] = mIds;
                            }
                        } catch (...) {}
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                }
            }

            // 2. Separate players by team
            std::vector<PlayerInfo> team1, team2;
            for (const auto& p : stateCopy.players) {
                if (p.teamId == "Red" || p.teamId == "Defender") {
                    team1.push_back(p);
                } else if (p.teamId == "Blue" || p.teamId == "Attacker") {
                    team2.push_back(p);
                } else {
                    team1.push_back(p);
                }
            }

            std::map<std::string, DetectedPartyInfo> newDetectedParties;
            int partyCounter = 1;

            auto analyzeTeam = [&](const std::vector<PlayerInfo>& team) {
                int n = (int)team.size();
                if (n < 2) return;

                // Build adjacency matrix for shared matches
                std::vector<std::vector<int>> shared(n, std::vector<int>(n, 0));
                for (int i = 0; i < n; ++i) {
                    for (int j = i + 1; j < n; ++j) {
                        std::vector<std::string> histI, histJ;
                        {
                            std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
                            if (g_playerMatchHistoryCache.count(team[i].puuid)) histI = g_playerMatchHistoryCache[team[i].puuid];
                            if (g_playerMatchHistoryCache.count(team[j].puuid)) histJ = g_playerMatchHistoryCache[team[j].puuid];
                        }
                        int common = 0;
                        for (const auto& m : histI) {
                            if (std::find(histJ.begin(), histJ.end(), m) != histJ.end()) {
                                common++;
                            }
                        }
                        shared[i][j] = common;
                        shared[j][i] = common;
                    }
                }

                // Connected components (edges where shared >= 1)
                std::vector<bool> visited(n, false);
                for (int i = 0; i < n; ++i) {
                    if (visited[i]) continue;
                    std::vector<int> comp;
                    std::vector<int> q = {i};
                    visited[i] = true;
                    size_t head = 0;
                    while (head < q.size()) {
                        int u = q[head++];
                        comp.push_back(u);
                        for (int v = 0; v < n; ++v) {
                            if (!visited[v] && shared[u][v] >= 1) {
                                visited[v] = true;
                                q.push_back(v);
                            }
                        }
                    }

                    if (comp.size() >= 2) {
                        int pSize = (int)comp.size();
                        std::string pLabel = (pSize == 2 ? "ДУО" : (pSize == 3 ? "ТРИО" : (pSize == 4 ? "4-СТАК" : "5-СТАК")));
                        std::string partyId = "hist_party_" + stateCopy.matchId.substr(0, std::min((size_t)8, stateCopy.matchId.size())) + "_" + std::to_string(partyCounter);
                        
                        for (int u : comp) {
                            int maxShared = 0;
                            std::vector<std::string> partners;
                            for (int v : comp) {
                                if (v != u) {
                                    maxShared = std::max(maxShared, shared[u][v]);
                                    std::string pName = team[v].gameName;
                                    if (pName.empty()) pName = "Игрок";
                                    partners.push_back(pName);
                                }
                            }
                            DetectedPartyInfo dpi;
                            dpi.partyNumber = partyCounter;
                            dpi.partySize = pSize;
                            dpi.sharedMatches = maxShared;
                            dpi.partyLabel = pLabel;
                            dpi.partnerNames = partners;
                            dpi.partyId = partyId;
                            newDetectedParties[team[u].puuid] = dpi;
                        }
                        partyCounter++;
                    }
                }
            };

            if (!team1.empty() && !team2.empty()) {
                analyzeTeam(team1);
                analyzeTeam(team2);
            } else {
                analyzeTeam(team1.empty() ? team2 : team1);
            }

            {
                std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
                g_detectedParties = newDetectedParties;
            }
        } catch (...) {}

        {
            std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
            g_partyCheckRunning = false;
        }
    }).detach();
}

bool g_showPostMatch = false;
PostMatchInfo g_postMatchInfo;
std::chrono::steady_clock::time_point g_postMatchStartTime;
static std::string g_lastCheckedUpdateMatchId = "";

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT || fdwCtrlType == CTRL_BREAK_EVENT) {
        return TRUE;
    }
    return FALSE;
}

// Last-chance diagnostics: write the crash location into debug.log before the
// process goes away, so future crashes can be identified without a debugger.
static LONG WINAPI logUnhandledException(EXCEPTION_POINTERS* ep) {
    std::ostringstream oss;
    oss << "Unhandled exception 0x" << std::hex << (unsigned long)ep->ExceptionRecord->ExceptionCode
        << " at 0x" << (unsigned long long)(uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(ep->ExceptionRecord->ExceptionAddress), &hMod) && hMod) {
        char modPath[MAX_PATH] = {};
        if (GetModuleFileNameA(hMod, modPath, MAX_PATH)) {
            oss << " in " << modPath;
            HMODULE self = GetModuleHandleA(nullptr);
            if (self && hMod != self) {
                // Report the offset inside the foreign module as well.
                oss << " +0x" << (unsigned long long)((uintptr_t)ep->ExceptionRecord->ExceptionAddress - (uintptr_t)hMod);
            }
        }
    }
    Log(oss.str());
    return EXCEPTION_CONTINUE_SEARCH;
}

static void logTerminateHandler() {
    try {
        std::exception_ptr eptr = std::current_exception();
        if (eptr) {
            std::rethrow_exception(eptr);
        }
        Log("std::terminate called without an active exception");
    } catch (const std::exception& e) {
        Log(std::string("Uncaught exception: ") + e.what());
    } catch (...) {
        Log("Uncaught unknown exception");
    }
    std::_Exit(1);
}


static std::set<std::string> g_seenChatMsgIds;

static void fetchChatMessages(const Lockfile& lock, const MatchState& state, const std::map<std::string, std::string>& agentMap, const std::string& localPuuid) {
    if (lock.port == 0 || lock.password.empty()) return;
    std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);

    // Auto-discover conversation IDs from client
    static auto s_lastConvQuery = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    if (g_chatCurrentCoreCid.empty() || g_chatCurrentAllCid.empty() ||
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s_lastConvQuery).count() > 10) {
        s_lastConvQuery = std::chrono::steady_clock::now();
        auto cRes = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v6/conversations", {{"Authorization", authHeader}}, "", true, true);
        if (cRes.statusCode != 200) {
            cRes = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v5/conversations", {{"Authorization", authHeader}}, "", true, true);
        }
        if (cRes.statusCode == 200) {
            try {
                auto cj = json::parse(cRes.body);
                auto arr = cj.is_array() ? cj : (cj.contains("conversations") ? cj["conversations"] : json::array());
                for (const auto& cItem : arr) {
                    std::string cid = getJsonKeyStr(cItem, {"cid", "id"});
                    std::string cType = getJsonKeyStr(cItem, {"type"});
                    if (!cid.empty()) {
                        if (cid.find("@ares-coregame") != std::string::npos || cid.find("@ares-pregame") != std::string::npos) {
                            if (cid.find("-all") != std::string::npos || cType == "all") {
                                g_chatCurrentAllCid = cid;
                            } else {
                                g_chatCurrentCoreCid = cid;
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }
    
    // Query chat messages
    auto res = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v6/messages", {{"Authorization", authHeader}}, "", true, true);
    if (res.statusCode != 200) {
        res = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v5/messages", {{"Authorization", authHeader}}, "", true, true);
    }
    if (res.statusCode != 200) return;

    try {
        auto j = json::parse(res.body);
        if (!j.contains("messages") || !j["messages"].is_array()) return;

        // Build PUUID -> PlayerInfo and Name -> PlayerInfo maps from state
        std::map<std::string, PlayerInfo> playerMap;
        std::map<std::string, PlayerInfo> playerNameMap;
        std::string localTeam = "";
        for (const auto& p : state.players) {
            playerMap[p.puuid] = p;
            if (!p.gameName.empty()) {
                playerNameMap[toLowerUtf8(p.gameName)] = p;
                if (!p.tagLine.empty()) {
                    playerNameMap[toLowerUtf8(p.gameName + "#" + p.tagLine)] = p;
                }
            }
            if (!localPuuid.empty() && p.puuid == localPuuid) {
                localTeam = p.teamId;
            }
        }

        std::vector<ValorantChatMessage> newMessages;
        for (const auto& m : j["messages"]) {
            std::string cid = getJsonKeyStr(m, {"cid"});
            std::string rawType = getJsonKeyStr(m, {"type"}); // "all", "group", "whisper", "chat"

            std::string chatType = "team";
            if (rawType == "all" || cid.find("-all") != std::string::npos) {
                chatType = "all";
                g_chatCurrentAllCid = cid;
            } else if (cid.find("@ares-coregame") != std::string::npos || cid.find("@ares-pregame") != std::string::npos) {
                chatType = "team";
                g_chatCurrentCoreCid = cid;
            } else if (cid.find("@ares-parties") != std::string::npos || cid.find(".pvp.net") != std::string::npos) {
                chatType = "party";
            }

            std::string msgId = getJsonKeyStr(m, {"id"});
            if (msgId.empty()) {
                msgId = getJsonKeyStr(m, {"time", "timestamp"}) + "_" + getJsonKeyStr(m, {"body"});
            }
            if (msgId.empty() || g_seenChatMsgIds.count(msgId)) continue;
            g_seenChatMsgIds.insert(msgId);

            std::string body = getJsonKeyStr(m, {"body", "message"});
            if (body.empty()) continue;

            std::string senderPuuid = getJsonKeyStr(m, {"sender", "puuid", "from"});
            std::string gameName = getJsonKeyStr(m, {"game_name"});
            std::string tagLine = getJsonKeyStr(m, {"game_tag"});

            ValorantChatMessage vcm;
            vcm.id = msgId;
            vcm.cid = cid;
            vcm.senderPuuid = senderPuuid;
            vcm.text = body;
            vcm.chatType = chatType;
            vcm.translated = translateGamingChat(body);

            // Match with active player info by PUUID or by Name
            const PlayerInfo* matchedPlayer = nullptr;
            if (!senderPuuid.empty() && playerMap.count(senderPuuid)) {
                matchedPlayer = &playerMap[senderPuuid];
            } else if (!gameName.empty()) {
                std::string gnLower = toLowerUtf8(gameName);
                if (playerNameMap.count(gnLower)) {
                    matchedPlayer = &playerNameMap[gnLower];
                }
            }

            if (matchedPlayer) {
                vcm.senderName = matchedPlayer->gameName.empty() ? (gameName.empty() ? "Игрок" : gameName) : matchedPlayer->gameName;
                vcm.senderAgent = matchedPlayer->agentName;
                if (vcm.senderAgent.empty() && !matchedPlayer->characterId.empty()) {
                    std::string c_id = matchedPlayer->characterId;
                    for (auto& c : c_id) c = tolower(c);
                    if (agentMap.count(c_id)) vcm.senderAgent = agentMap.at(c_id);
                    else if (agentMap.count(matchedPlayer->characterId)) vcm.senderAgent = agentMap.at(matchedPlayer->characterId);
                }
                vcm.teamId = matchedPlayer->teamId;
                if (!localTeam.empty()) {
                    vcm.isEnemy = (matchedPlayer->teamId != localTeam);
                } else {
                    vcm.isEnemy = (matchedPlayer->teamId == "Red" || matchedPlayer->teamId == "Defender");
                }
            } else {
                vcm.senderName = gameName.empty() ? "Игрок" : gameName;
                if (!tagLine.empty()) vcm.senderName += "#" + tagLine;
            }

            newMessages.push_back(vcm);
        }

        if (!newMessages.empty()) {
            std::lock_guard<std::mutex> lk(g_chatMutex);
            for (const auto& nm : newMessages) {
                g_chatMessages.push_back(nm);
            }
            if (g_chatMessages.size() > 200) {
                g_chatMessages.erase(g_chatMessages.begin(), g_chatMessages.begin() + (g_chatMessages.size() - 200));
            }
        }
    } catch (...) {}
}

static void sendValorantChatMessage(const std::string& rawText, const std::string& targetChannel, ftxui::ScreenInteractive* screen = nullptr) {
    try {
        std::string textToSend = rawText;
        while (!textToSend.empty() && (textToSend.front() == ' ' || textToSend.front() == '\t')) textToSend.erase(textToSend.begin());
        while (!textToSend.empty() && (textToSend.back() == ' ' || textToSend.back() == '\t')) textToSend.pop_back();
        if (textToSend.empty()) return;

        // Filter profanity and slurs to prevent Riot bans
        FilterResult fr = filterProfanityAndSlurs(textToSend);
        if (fr.isHardBanned) {
            g_chatStatusMessage = "Заблокировано: запрещенное/оскорбительное слово!";
            g_chatStatusTime = std::chrono::steady_clock::now();
            if (screen) screen->PostEvent(ftxui::Event::Custom);
            return;
        }
        textToSend = fr.text;

        // Optional Russian to English translation
        if (g_chatAutoTranslateRuToEn) {
            textToSend = translateRussianToEnglish(textToSend);
        }

        Lockfile lock;
        MatchState state;
        {
            std::lock_guard<std::recursive_mutex> lk(g_mutex);
            lock = g_liveLock;
            state = g_liveMatchState;
        }

        if (lock.port == 0 || lock.password.empty()) {
            lock = readLockfile();
        }

        if (lock.port == 0 || lock.password.empty()) {
            g_chatStatusMessage = "Ошибка: Valorant не обнаружен";
            g_chatStatusTime = std::chrono::steady_clock::now();
            if (screen) screen->PostEvent(ftxui::Event::Custom);
            return;
        }

        std::string authHeader = "Basic " + base64_encode("riot:" + lock.password);

        // Discover active match team and all chat CIDs from conversations
        auto convRes = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v6/conversations", {{"Authorization", authHeader}}, "", true, true);
        if (convRes.statusCode != 200) {
            convRes = httpRequest("GET", "127.0.0.1", lock.port, "/chat/v5/conversations", {{"Authorization", authHeader}}, "", true, true);
        }
        if (convRes.statusCode == 200) {
            try {
                auto cj = json::parse(convRes.body);
                auto arr = cj.is_array() ? cj : (cj.contains("conversations") ? cj["conversations"] : json::array());
                for (const auto& cItem : arr) {
                    std::string cid = getJsonKeyStr(cItem, {"cid", "id"});
                    std::string cType = getJsonKeyStr(cItem, {"type"});
                    if (!cid.empty()) {
                        if (cid.find("@ares-coregame") != std::string::npos || cid.find("@ares-pregame") != std::string::npos) {
                            if (cid.find("-all") != std::string::npos || cType == "all") {
                                g_chatCurrentAllCid = cid;
                            } else {
                                g_chatCurrentCoreCid = cid;
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        // Construct CID directly from live matchId if in match
        if (!state.matchId.empty()) {
            if (state.phase == "coregame") {
                if (g_chatCurrentCoreCid.empty()) {
                    g_chatCurrentCoreCid = state.matchId + "@ares-coregame.valorant.riotgames.com";
                }
                if (g_chatCurrentAllCid.empty()) {
                    g_chatCurrentAllCid = state.matchId + "-all@ares-coregame.valorant.riotgames.com";
                }
            } else if (state.phase == "pregame") {
                if (g_chatCurrentCoreCid.empty()) {
                    g_chatCurrentCoreCid = state.matchId + "@ares-pregame.valorant.riotgames.com";
                }
            }
        }

        std::string targetCid = "";
        if (targetChannel == "all") {
            targetCid = g_chatCurrentAllCid;
            if (targetCid.empty() && !state.matchId.empty()) {
                targetCid = state.matchId + "-all@ares-coregame.valorant.riotgames.com";
            }
        } else {
            // targetChannel == "team"
            targetCid = g_chatCurrentCoreCid;
            if (targetCid.empty() && !state.matchId.empty()) {
                if (state.phase == "coregame") {
                    targetCid = state.matchId + "@ares-coregame.valorant.riotgames.com";
                } else if (state.phase == "pregame") {
                    targetCid = state.matchId + "@ares-pregame.valorant.riotgames.com";
                }
            }
        }

        if (targetCid.empty()) {
            if (state.phase == "none" || state.matchId.empty()) {
                g_chatStatusMessage = "Чат команды/всех доступен только в матче (сейчас вы в лобби)";
            } else {
                g_chatStatusMessage = "Ошибка: не найден CID чата матча";
            }
            g_chatStatusTime = std::chrono::steady_clock::now();
            if (screen) screen->PostEvent(ftxui::Event::Custom);
            return;
        }

        std::string sentChannel = targetChannel;
        std::thread([lock, authHeader, targetCid, textToSend, sentChannel, screen]() {
            try {
                json postPayload = {
                    {"cid", targetCid},
                    {"message", textToSend}
                };
                std::string body = postPayload.dump();
                auto res = httpRequest("POST", "127.0.0.1", lock.port, "/chat/v6/messages", {{"Authorization", authHeader}, {"Content-Type", "application/json"}}, body, true, true);
                if (res.statusCode != 200 && res.statusCode != 201) {
                    res = httpRequest("POST", "127.0.0.1", lock.port, "/chat/v5/messages", {{"Authorization", authHeader}, {"Content-Type", "application/json"}}, body, true, true);
                }

                if (res.statusCode == 200 || res.statusCode == 201) {
                    g_chatStatusMessage = "Отправлено в " + (sentChannel == "all" ? std::string("[ВСЕМ]") : std::string("[КОМАНДЕ]")) + ": " + textToSend;
                    g_chatStatusTime = std::chrono::steady_clock::now();

                    ValorantChatMessage vcm;
                    vcm.id = "local_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                    vcm.cid = targetCid;
                    vcm.senderName = "ВЫ";
                    vcm.text = textToSend;
                    vcm.chatType = sentChannel;
                    vcm.isEnemy = false;
                    {
                        std::lock_guard<std::mutex> lk(g_chatMutex);
                        g_chatMessages.push_back(vcm);
                        if (g_chatMessages.size() > 200) {
                            g_chatMessages.erase(g_chatMessages.begin(), g_chatMessages.begin() + (g_chatMessages.size() - 200));
                        }
                    }
                } else {
                    g_chatStatusMessage = "Ошибка отправки (HTTP " + std::to_string(res.statusCode) + ")";
                    g_chatStatusTime = std::chrono::steady_clock::now();
                }
            } catch (...) {
                g_chatStatusMessage = "Исключение при отправке в чат";
                g_chatStatusTime = std::chrono::steady_clock::now();
            }
            if (screen) screen->PostEvent(ftxui::Event::Custom);
        }).detach();
    } catch (const std::exception& e) {
        g_chatStatusMessage = std::string("Ошибка: ") + e.what();
        g_chatStatusTime = std::chrono::steady_clock::now();
        if (screen) screen->PostEvent(ftxui::Event::Custom);
    } catch (...) {
        g_chatStatusMessage = "Неизвестная ошибка отправки";
        g_chatStatusTime = std::chrono::steady_clock::now();
        if (screen) screen->PostEvent(ftxui::Event::Custom);
    }
}

static HWND getTopLevelConsoleWindow() {
    HWND console = GetConsoleWindow();

    // 1. Check if console itself is classic ConHost
    if (console) {
        char className[256] = {0};
        GetClassNameA(console, className, sizeof(className));
        if (std::string(className) == "ConsoleWindowClass") {
            HWND root = GetAncestor(console, GA_ROOT);
            if (root && IsWindowVisible(root)) return root;
            return console;
        }
    }

    // 2. Find Windows Terminal (CASCADIA_HOSTING_WINDOW_CLASS) or ConHost by title "RolsTraker"
    // Explicitly IGNORE Explorer (CabinetWClass), Desktop (Progman/WorkerW), Taskbar (Shell_TrayWnd)
    struct FindTermData {
        HWND exactHwnd = NULL;
        HWND fallbackHwnd = NULL;
    } ft;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* p = reinterpret_cast<FindTermData*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;

        char className[256] = {0};
        GetClassNameA(hwnd, className, sizeof(className));
        std::string cls(className);

        // Never touch File Explorer, Taskbar, or Desktop
        if (cls == "CabinetWClass" || cls == "Progman" || cls == "WorkerW" || cls == "Shell_TrayWnd") {
            return TRUE;
        }

        if (cls == "CASCADIA_HOSTING_WINDOW_CLASS" || cls == "ConsoleWindowClass") {
            char title[512] = {0};
            GetWindowTextA(hwnd, title, sizeof(title));
            std::string t(title);
            if (t.find("RolsTracker") != std::string::npos || t.find("rolstracker") != std::string::npos || t.find("RolsTraker") != std::string::npos || t.find("rolstraker") != std::string::npos) {
                p->exactHwnd = hwnd;
                return FALSE; // Exact match found!
            }
            if (!p->fallbackHwnd) {
                p->fallbackHwnd = hwnd;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ft));

    if (ft.exactHwnd) return ft.exactHwnd;
    if (ft.fallbackHwnd) return ft.fallbackHwnd;

    if (console) return console;
    return NULL;
}

static void applyOptimalWindowSize() {
    std::thread([]() {
        for (int attempt = 0; attempt < 5; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(attempt == 0 ? 80 : 250));
            HWND console = getTopLevelConsoleWindow();
            if (!console) continue;

            int targetW = g_windowWidth > 0 ? g_windowWidth : 1080;
            int targetH = g_windowHeight > 0 ? g_windowHeight : 680;

            HMONITOR hMon = MonitorFromWindow(console, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfoA(hMon, &mi)) {
                RECT curRect;
                if (GetWindowRect(console, &curRect)) {
                    int newX = curRect.left;
                    int newY = curRect.top;
                    // Only adjust if overflowing work area boundaries
                    if (newX + targetW > mi.rcWork.right) {
                        newX = std::max((int)mi.rcWork.left, (int)mi.rcWork.right - targetW);
                    }
                    if (newY + targetH > mi.rcWork.bottom) {
                        newY = std::max((int)mi.rcWork.top, (int)mi.rcWork.bottom - targetH);
                    }
                    MoveWindow(console, newX, newY, targetW, targetH, TRUE);
                    SetWindowPos(console, NULL, newX, newY, targetW, targetH, SWP_NOZORDER | SWP_SHOWWINDOW);
                }
            }
        }
    }).detach();
}

int main() {
    SetConsoleTitleA("RolsTracker");
    Log("Starting main()");
    SetUnhandledExceptionFilter(logUnhandledException);
    std::set_terminate(logTerminateHandler);
    loadSettings();
    Log("Settings loaded (setupCompleted=" + std::string(g_setupCompleted ? "true" : "false") + ", mode=" + g_installMode + ")");
    if (!g_setupCompleted) {
        g_showFirstRunModal = true;
    }
    if (!g_discordClientId.empty()) { initDiscordRpc(g_discordClientId); updateDiscordPresence("В приложении", "rolstracker.3fun.workers.dev"); Log("Discord RPC init done"); }
    
    // Set optimal vertical / portrait window size (1080x1920 screen or primary monitor)
    HWND console = getTopLevelConsoleWindow();
    bool foundPortrait = false;
    RECT portraitRect = { 0, 0, 0, 0 };

    if (console) {
        struct EnumMonData {
            RECT targetRect;
            bool foundPortrait = false;
        } emd;
        
        EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMon, HDC, LPRECT, LPARAM dwData) -> BOOL {
            auto* pData = reinterpret_cast<EnumMonData*>(dwData);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfoA(hMon, &mi)) {
                int w = mi.rcWork.right - mi.rcWork.left;
                int h = mi.rcWork.bottom - mi.rcWork.top;
                // Portrait monitor (width ~1080, height ~1920)
                if (w <= 1200 && h >= 1600) {
                    pData->targetRect = mi.rcWork;
                    pData->foundPortrait = true;
                    return FALSE; // stop enum
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&emd));

        foundPortrait = emd.foundPortrait;
        portraitRect = emd.targetRect;
    }

    // Set console buffer cols and lines
    Log("Setting console mode");
    int conCols = 120;
    int conLines = 31;
    std::string modeCmd = "mode con cols=" + std::to_string(conCols) + " lines=" + std::to_string(conLines);
    system(modeCmd.c_str());
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    Log("WSAStartup done");

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    signal(SIGINT, SIG_IGN);

    // Disable Quick Edit Mode (prevents freezing program by mouse selection)
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prev_mode;
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        COORD coord = { (SHORT)conCols, (SHORT)conLines };
        SetConsoleScreenBufferSize(hOut, coord);
        SMALL_RECT sRect = { 0, 0, (SHORT)(conCols - 1), (SHORT)(conLines - 1) };
        SetConsoleWindowInfo(hOut, TRUE, &sRect);
    }

    applyOptimalWindowSize();
    Log("Console window set");

    std::thread([]() {
        while (g_running) {
            try { checkAutoUpdate(); }
            catch (...) { Log("Auto-update thread failed"); }
            for (int i = 0; i < 180 && g_running; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }).detach();
    Log("Starting initWeaponsDatabase");
    initWeaponsDatabase();
    Log("initWeaponsDatabase done");

    Log("Starting getAgentMap");
    auto agentMap = getAgentMap();
    Log("getAgentMap done");
    Log("Starting getMapNameMap");
    auto mapNameMap = getMapNameMap();
    Log("getMapNameMap done");
    
    g_globalAgentMap = agentMap;
    g_globalMapNameMap = mapNameMap;

    Lockfile lock;
    Session session;
    MatchState matchState;

    Log("Creating FTXUI screen");
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::FitComponent();
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD outMode = 0;
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &outMode)) {
            try {
                screen = ftxui::ScreenInteractive::Fullscreen();
            } catch (...) {
                screen = ftxui::ScreenInteractive::FitComponent();
            }
        } else {
            screen = ftxui::ScreenInteractive::FitComponent();
        }
    }
    Log("FTXUI screen created");

    // Dedicated UI Ticker thread (30 FPS constant) - ensures zero lag and smooth animations
    std::thread ui_anim_thread([&screen]() {
        while (g_running) {
            screen.PostEvent(ftxui::Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });
    ui_anim_thread.detach();
    
    // Background polling thread
    Log("Starting polling thread");
    std::thread polling_thread([&]() {
        Log("Polling thread running");
        auto lastCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        std::string currentPhase = "none";
        int loopCount = 0;
        while (g_running) {
            try {
                loopCount++;
                if (loopCount <= 5) Log("Poll loop " + std::to_string(loopCount));
            auto now = std::chrono::steady_clock::now();
            int pollInterval = 4;
            {
                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                currentPhase = matchState.phase;
            }
            if (currentPhase == "coregame") pollInterval = 5;
            else if (currentPhase == "pregame") pollInterval = 3;
            else pollInterval = 4;
            
            bool timeToRefresh = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= pollInterval;
            
            if (timeToRefresh) {
                lastCheck = now;
                if (loopCount <= 5) Log("poll: readLockfile");
                Lockfile newLock = readLockfile();
                static int s_consecutiveLockFails = 0;
                if (newLock.port == 0) {
                    s_consecutiveLockFails++;
                    static auto s_lastOfflineRpc = std::chrono::steady_clock::now() - std::chrono::seconds(30);
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - s_lastOfflineRpc).count() >= 5) {
                        s_lastOfflineRpc = now;
                        if (!g_discordClientId.empty()) updateDiscordPresence("В приложении", "RolsTracker");
                    }
                    if (s_consecutiveLockFails < 4 && lock.port != 0) {
                        newLock = lock;
                    }
                } else {
                    s_consecutiveLockFails = 0;
                }

                Session newSession = session;
                MatchState newState;
                if (loopCount <= 5) Log("poll: lockfile port=" + std::to_string(newLock.port));
                if (newLock.port != 0) {
                    // Immediately update lock so UI knows Riot Client is active without waiting for network calls
                    {
                        std::lock_guard<std::recursive_mutex> lk(g_mutex);
                        lock = newLock;
                        g_liveLock = newLock;
                    }

                    if (loopCount <= 5) Log("poll: getSession");
                    newSession = getSession(newLock);
                    g_localPuuid = newSession.puuid;
                    if (loopCount <= 5) Log("poll: getSession done puuid=" + std::to_string(newSession.puuid.size()));

                    if (!newSession.accessToken.empty()) {
                        static bool s_ownedChecked = false;
                        if (!s_ownedChecked) {
                            s_ownedChecked = true;
                            std::thread([newSession]() {
                                auto owned = getOwnedAgents(newSession);
                                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                g_ownedAgents = owned;
                            }).detach();
                        }

                        static bool s_localMmrFetched = false;
                        if (!s_localMmrFetched && !newSession.puuid.empty()) {
                            s_localMmrFetched = true;
                            std::string myPuuid = newSession.puuid;
                            Session mySess = newSession;
                            std::thread([mySess, myPuuid]() {
                                std::string clientPlatform = getClientPlatformBase64();
                                std::map<std::string, std::string> pdHeaders = {
                                    {"Authorization", "Bearer " + mySess.accessToken},
                                    {"X-Riot-Entitlements-JWT", mySess.token},
                                    {"X-Riot-ClientPlatform", clientPlatform},
                                    {"X-Riot-ClientVersion", mySess.clientVersion}
                                };
                                auto mmrRes = httpRequest("GET", mySess.pdHost, 443, "/mmr/v1/players/" + myPuuid, pdHeaders, "", true, false);
                                if (mmrRes.statusCode == 200) {
                                    try {
                                        auto j = json::parse(mmrRes.body);
                                        int tier = 0, rr = 0, totalWins = 0, totalGames = 0, peakTier = 0, peakRR = 0;
                                        if (j.contains("LatestCompetitiveUpdate") && !j["LatestCompetitiveUpdate"].is_null()) {
                                            auto lcu = j["LatestCompetitiveUpdate"];
                                            if (lcu.contains("TierAfterUpdate") && !lcu["TierAfterUpdate"].is_null()) tier = lcu["TierAfterUpdate"].get<int>();
                                            if (lcu.contains("RankedRatingAfterUpdate") && !lcu["RankedRatingAfterUpdate"].is_null()) rr = lcu["RankedRatingAfterUpdate"].get<int>();
                                        }
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
                                                            if (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) rr = sData["RankedRating"].get<int>();
                                                        }
                                                    }
                                                    if (sData.contains("NumberOfGames") && sData["NumberOfGames"].is_number()) totalGames = sData["NumberOfGames"].get<int>();
                                                    if (sData.contains("NumberOfWinsWithPlacements") && sData["NumberOfWinsWithPlacements"].is_number()) totalWins = sData["NumberOfWinsWithPlacements"].get<int>();
                                                }
                                                for (const auto& [sId, sData] : seasons.items()) {
                                                    if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                                        int t = sData["CompetitiveTier"].get<int>();
                                                        if (t > peakTier) {
                                                            peakTier = t;
                                                            peakRR = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                                        } else if (t == peakTier) {
                                                            int r = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                                            if (r > peakRR) peakRR = r;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                        g_rankCache[myPuuid] = {tier, rr, totalWins, std::max(0, totalGames - totalWins), peakTier, peakRR};
                                    } catch (...) {}
                                }
                            }).detach();
                        }

                        if (loopCount <= 5) Log("poll: getLiveMatchState");
                        newState = getLiveMatchState(newSession, newLock);
                        if (loopCount <= 5) Log("poll: getLiveMatchState done err=" + std::to_string((int)newState.isError) + " phase=" + newState.phase);
                        if (!newState.isError && newState.phase != "none") {
                            if (loopCount <= 5) Log("poll: resolveOwnParty");
                            resolveOwnParty(newSession, newState.players);
                            
                            if (!g_discordClientId.empty()) {
                                std::string mapName = g_globalMapNameMap.count(newState.mapId) ? g_globalMapNameMap[newState.mapId] : (!newState.mapId.empty() ? newState.mapId : "");
                                if (newState.phase == "coregame") {
                                    updateDiscordPresence("В матче", mapName.empty() ? "RolsTracker" : ("Карта: " + mapName));
                                } else {
                                    updateDiscordPresence("Выбор агента", mapName.empty() ? "RolsTracker" : ("Карта: " + mapName));
                                }
                            }
                            if (loopCount <= 5) Log("poll: getPresencesPartyMap");
                            auto presencePartyMap = getPresencesPartyMap(newLock);
                            for (auto& p : newState.players) {
                                if (p.partyId.empty() && presencePartyMap.count(p.puuid)) {
                                    p.partyId = presencePartyMap[p.puuid];
                                }
                            }
                            
                            if (loopCount <= 5) Log("poll: resolveDisplayNamesAndRanks");
                            resolveDisplayNamesAndRanks(newSession, newState.players);
                            if (loopCount <= 5) Log("poll: updateMatchLoadouts");
                            updateMatchLoadouts(newSession, newState);
                            if (loopCount <= 5) Log("poll: detectPartiesFromMatchHistory");
                            detectPartiesFromMatchHistory(newSession, newState);
                            fetchChatMessages(newLock, newState, g_globalAgentMap, g_localPuuid);
                            if (loopCount <= 5) Log("poll: party detection done");
                        } else {
                            g_lastLoadedMatchId = "";
                            g_lastPartyCheckMatchId = "";
                            {
                                std::lock_guard<std::recursive_mutex> lk(g_partyDetectionMutex);
                                g_detectedParties.clear();
                            }
                            if (!newSession.puuid.empty()) {
                                fetchPlayerLoadoutDirect(newSession, newSession.puuid);
                                fetchChatMessages(newLock, newState, g_globalAgentMap, g_localPuuid);
                            }
                            if (!g_discordClientId.empty()) {
                                static auto s_lastIdleRpc = std::chrono::steady_clock::now() - std::chrono::seconds(30);
                                if (std::chrono::duration_cast<std::chrono::seconds>(now - s_lastIdleRpc).count() >= 5) {
                                    s_lastIdleRpc = now;
                                    std::string myRankInfo = "";
                                    if (!newSession.puuid.empty()) {
                                        std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                        if (g_rankCache.count(newSession.puuid)) {
                                            int t = g_rankCache[newSession.puuid].tier;
                                            int r = g_rankCache[newSession.puuid].rr;
                                            if (t >= 3) {
                                                int elo = (t - 2) * 100 + r;
                                                myRankInfo = " • " + std::to_string(elo) + " ELO";
                                            }
                                        }
                                    }
                                    updateDiscordPresence("В главном меню" + myRankInfo, "rolstracker.3fun.workers.dev");
                                }
                            }
                        }
                    }
                }
                
                {
                    std::lock_guard<std::recursive_mutex> lk(g_mutex);
                    lock = newLock;
                    session = newSession;
                    g_liveLock = newLock;
                    if (!newState.isError || newLock.port == 0) {
                        matchState = newState;
                    }
                    g_liveMatchState = matchState;
                    g_serverRegion = session.region;
                    currentPhase = matchState.phase;
                    if (currentPhase != "pregame" && g_currentView == AppView::AGENT_PICK) {
                        g_currentView = AppView::MAIN;
                    }
                    if (currentPhase == "none") {
                        g_reportedPuuids.clear();
                        g_chatCurrentCoreCid.clear();
                        g_chatCurrentAllCid.clear();
                    }
                }

                // Detect end of game and fetch competitive updates
                static std::string s_prevPhase = "none";
                if (s_prevPhase == "coregame" && currentPhase != "coregame" && !session.accessToken.empty() && !session.puuid.empty()) {
                    std::thread([session]() {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        for (int attempt = 0; attempt < 5; ++attempt) {
                            if (!g_running) return;
                            std::map<std::string, std::string> headers = {
                                {"Authorization", "Bearer " + session.accessToken},
                                {"X-Riot-Entitlements-JWT", session.token},
                                {"X-Riot-ClientPlatform", getClientPlatformBase64()},
                                {"X-Riot-ClientVersion", session.clientVersion}
                            };
                            auto res = httpRequest("GET", session.pdHost, 443, "/mmr/v1/players/" + session.puuid + "/competitiveupdates?startIndex=0&endIndex=3", headers, "", true, false);
                            if (res.statusCode == 200) {
                                try {
                                    auto j = json::parse(res.body);
                                    if (j.contains("Matches") && j["Matches"].is_array() && !j["Matches"].empty()) {
                                        const auto& m = j["Matches"][0];
                                        std::string mid = getJsonKeyStr(m, {"MatchID"});
                                        if (!mid.empty() && mid != g_lastCheckedUpdateMatchId) {
                                            g_lastCheckedUpdateMatchId = mid;
                                            PostMatchInfo pmi;
                                            pmi.matchId = mid;
                                            if (m.contains("TierAfterUpdate") && m["TierAfterUpdate"].is_number()) pmi.tierAfter = m["TierAfterUpdate"].get<int>();
                                            if (m.contains("TierBeforeUpdate") && m["TierBeforeUpdate"].is_number()) pmi.tierBefore = m["TierBeforeUpdate"].get<int>();
                                            if (m.contains("RankedRatingAfterUpdate") && m["RankedRatingAfterUpdate"].is_number()) pmi.rrAfter = m["RankedRatingAfterUpdate"].get<int>();
                                            if (m.contains("RankedRatingBeforeUpdate") && m["RankedRatingBeforeUpdate"].is_number()) pmi.rrBefore = m["RankedRatingBeforeUpdate"].get<int>();
                                            if (m.contains("RankedRatingEarned") && m["RankedRatingEarned"].is_number()) pmi.rrEarned = m["RankedRatingEarned"].get<int>();
                                            if (m.contains("RankedRatingPerformanceBonus") && m["RankedRatingPerformanceBonus"].is_number()) pmi.performanceBonus = m["RankedRatingPerformanceBonus"].get<int>();
                                            pmi.movement = getJsonKeyStr(m, {"CompetitiveMovement"});
                                            
                                            {
                                                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                                g_postMatchInfo = pmi;
                                                g_postMatchStartTime = std::chrono::steady_clock::now();
                                                g_showPostMatch = true;
                                                if (g_rankCache.count(session.puuid)) {
                                                    g_rankCache[session.puuid].rr = pmi.rrAfter;
                                                    if (pmi.tierAfter > 0) g_rankCache[session.puuid].tier = pmi.tierAfter;
                                                }
                                            }
                                            break;
                                        }
                                    }
                                } catch (...) {}
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        }
                    }).detach();
                }

                s_prevPhase = currentPhase;

                static auto lastPenaltyCheck = std::chrono::steady_clock::now() - std::chrono::seconds(60);
                if (!session.accessToken.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - lastPenaltyCheck).count() >= 60) {
                    lastPenaltyCheck = now;
                    std::thread([session]() {
                        std::map<std::string, std::string> headers = {
                            {"Authorization", "Bearer " + session.accessToken},
                            {"X-Riot-Entitlements-JWT", session.token},
                            {"X-Riot-ClientPlatform", getClientPlatformBase64()},
                            {"X-Riot-ClientVersion", session.clientVersion}
                        };
                        auto res = httpRequest("GET", session.pdHost, 443, "/restrictions/v3/penalties", headers, "", true, false);
                        if (res.statusCode == 200) {
                            try {
                                auto j = json::parse(res.body);
                                std::string status = "";
                                if (j.contains("Penalties") && j["Penalties"].is_array()) {
                                    std::vector<std::string> penaltyEntries;
                                    std::set<std::string> seen;
                                    for (const auto& p : j["Penalties"]) {
                                        std::string type = getJsonKeyStr(p, {"Penalty"});
                                        if (type.empty()) type = getJsonKeyStr(p, {"PenaltyType"});
                                        if (type.empty()) type = "RESTRICTION";

                                        if (type == "WARNING") continue;
                                        std::string expiryStr = getJsonKeyStr(p, {"Expiry"});
                                        std::string mskTime = expiryStr;
                                        if (!expiryStr.empty() && expiryStr.find('T') != std::string::npos) {
                                            expiryStr = expiryStr.substr(0, expiryStr.find('.'));
                                            int year, month, day, hour, min, sec;
                                            if (sscanf(expiryStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
                                                std::tm tm = {};
                                                tm.tm_year = year - 1900;
                                                tm.tm_mon = month - 1;
                                                tm.tm_mday = day;
                                                tm.tm_hour = hour + 3; // UTC+3 for MSK
                                                tm.tm_min = min;
                                                tm.tm_sec = sec;
                                                tm.tm_isdst = -1;
                                                std::mktime(&tm);
                                                
                                                char buf[64];
                                                std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", &tm);
                                                mskTime = std::string(buf) + " (МСК)";
                                            }
                                        }

                                        std::string friendlyName = type;
                                        if (type == "RESTRICTION" || type == "QUEUE_RESTRICTION" || type == "COMPETITIVE_RESTRICTION") {
                                            friendlyName = "Бан поиска";
                                        } else if (type == "CHAT_RESTRICTION") {
                                            friendlyName = "Бан чата";
                                        } else if (type == "VOICE_RESTRICTION") {
                                            friendlyName = "Бан войса";
                                        } else if (type == "COMMUNICATIONS_RESTRICTION") {
                                            friendlyName = "Бан чата и войса";
                                        } else if (type == "TIME_BAN" || type == "BAN") {
                                            friendlyName = "Бан аккаунта";
                                        }

                                        std::string entry = friendlyName + " до " + mskTime;
                                        if (!seen.count(entry)) {
                                            seen.insert(entry);
                                            penaltyEntries.push_back(entry);
                                        }
                                    }
                                    for (size_t i = 0; i < penaltyEntries.size(); ++i) {
                                        if (i > 0) status += " | ";
                                        status += penaltyEntries[i];
                                    }
                                }
                                { std::lock_guard<std::mutex> lk(g_penaltiesMutex); g_penaltiesStatus = status; }
                            } catch (...) {}
                        }
                    }).detach();
                }
                
                // Keep selectedPlayerInfo updated if viewing it
                if (g_currentView == AppView::PLAYER_STATS && !g_selectedPuuid.empty()) {
                    std::lock_guard<std::recursive_mutex> lk(g_mutex);
                    for (const auto& p : matchState.players) {
                        if (p.puuid == g_selectedPuuid) {
                            g_selectedPlayerInfo = p;
                            break;
                        }
                    }
                    if (g_nameCache.count(g_selectedPuuid)) {
                        g_selectedPlayerInfo.gameName = g_nameCache[g_selectedPuuid].first;
                        g_selectedPlayerInfo.tagLine = g_nameCache[g_selectedPuuid].second;
                    }
                    if (g_rankCache.count(g_selectedPuuid)) {
                        g_selectedPlayerInfo.rankTier = g_rankCache[g_selectedPuuid].tier;
                        g_selectedPlayerInfo.rankRR = g_rankCache[g_selectedPuuid].rr;
                        g_selectedPlayerInfo.wins = g_rankCache[g_selectedPuuid].wins;
                        g_selectedPlayerInfo.losses = g_rankCache[g_selectedPuuid].losses;
                        g_selectedPlayerInfo.peakRankTier = g_rankCache[g_selectedPuuid].peakTier;
                        g_selectedPlayerInfo.peakRankRR = g_rankCache[g_selectedPuuid].peakRR;
                    }
                    if (g_statsCache.count(g_selectedPuuid)) {
                        const auto& sc = g_statsCache[g_selectedPuuid];
                        g_selectedPlayerInfo.kdRatio = sc.kdRatio;
                        g_selectedPlayerInfo.hsPercent = sc.hsPercent;
                        g_selectedPlayerInfo.headshots = sc.headshots;
                        g_selectedPlayerInfo.bodyshots = sc.bodyshots;
                        g_selectedPlayerInfo.legshots = sc.legshots;
                        g_selectedPlayerInfo.matchesWon = sc.matchesWon;
                        g_selectedPlayerInfo.matchesPlayed = sc.matchesPlayed;
                        g_selectedPlayerInfo.acs = sc.acs;
                        g_selectedPlayerInfo.favAgent = sc.favAgent;
                        g_selectedPlayerInfo.isPrivate = sc.isPrivate;
                        g_selectedPlayerInfo.statsSource = sc.source;
                        g_selectedPlayerInfo.isLoading = false;
                    }
                    {
                        std::lock_guard<std::recursive_mutex> lkLoad(g_loadoutsMutex);
                        if (g_playerLoadouts.count(g_selectedPuuid)) {
                            const auto& pl = g_playerLoadouts[g_selectedPuuid];
                            if (pl.loaded) {
                                int cCount = 0;
                                bool kFound = false;
                                std::string kWeapon = "";
                                bool nFound = false;
                                std::string nWeapon = "";
                                bool prFound = false;
                                std::string prWeapon = "";
                                for (const auto& [wId, w] : pl.weapons) {
                                    if (w.isCustom) cCount++;
                                    std::string outW;
                                    if (checkKuronamiWeapon(w.weaponName, w.skinName, w.fullName, outW)) {
                                        kFound = true;
                                        kWeapon = outW;
                                    }
                                    std::string outN;
                                    if (checkNocturnumWeapon(w.weaponName, w.skinName, w.fullName, outN)) {
                                        nFound = true;
                                        nWeapon = outN;
                                    }
                                    std::string outPr;
                                    if (checkPrimordiumWeapon(w.weaponName, w.skinName, w.fullName, outPr)) {
                                        prFound = true;
                                        prWeapon = outPr;
                                    }
                                }
                                g_selectedPlayerInfo.skinsLoaded = true;
                                g_selectedPlayerInfo.customSkinCount = cCount;
                                g_selectedPlayerInfo.isBooster = (cCount <= 2);
                                g_selectedPlayerInfo.hasKuronami = kFound;
                                g_selectedPlayerInfo.kuronamiWeapon = kWeapon;
                                g_selectedPlayerInfo.hasNocturnum = nFound;
                                g_selectedPlayerInfo.nocturnumWeapon = nWeapon;
                                g_selectedPlayerInfo.hasPrimordium = prFound;
                                g_selectedPlayerInfo.primordiumWeapon = prWeapon;
                            }
                        }
                    }
                    {
                        std::lock_guard<std::recursive_mutex> lkParty(g_partyDetectionMutex);
                        if (g_detectedParties.count(g_selectedPuuid)) {
                            const auto& dpi = g_detectedParties[g_selectedPuuid];
                            g_selectedPlayerInfo.detectedPartySize = dpi.partySize;
                            g_selectedPlayerInfo.detectedSharedMatches = dpi.sharedMatches;
                            g_selectedPlayerInfo.detectedPartyLabel = dpi.partyLabel;
                            g_selectedPlayerInfo.detectedPartyPartners = dpi.partnerNames.empty() ? "" : dpi.partnerNames[0];
                        }
                    }
                }
            }

            // Sync players with latest background cached stats, loadouts, and detected parties
            {
                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                for (auto& p : matchState.players) {
                    if (g_nameCache.count(p.puuid)) {
                        p.gameName = g_nameCache[p.puuid].first;
                        p.tagLine = g_nameCache[p.puuid].second;
                    }
                    if (g_rankCache.count(p.puuid)) {
                        p.rankTier = g_rankCache[p.puuid].tier;
                        p.rankRR = g_rankCache[p.puuid].rr;
                        p.wins = g_rankCache[p.puuid].wins;
                        p.losses = g_rankCache[p.puuid].losses;
                        p.peakRankTier = g_rankCache[p.puuid].peakTier;
                        p.peakRankRR = g_rankCache[p.puuid].peakRR;
                    }
                    if (g_statsCache.count(p.puuid)) {
                        const auto& sc = g_statsCache[p.puuid];
                        p.kdRatio = sc.kdRatio;
                        p.hsPercent = sc.hsPercent;
                        p.headshots = sc.headshots;
                        p.bodyshots = sc.bodyshots;
                        p.legshots = sc.legshots;
                        p.matchesWon = sc.matchesWon;
                        p.matchesPlayed = sc.matchesPlayed;
                        p.acs = sc.acs;
                        p.favAgent = sc.favAgent;
                        p.isPrivate = sc.isPrivate;
                        p.statsSource = sc.source;
                        p.isLoading = false;
                    }
                }

                // Sync loadout and booster status
                for (auto& p : matchState.players) {
                    PlayerWeaponLoadout pl;
                    if (getPlayerLoadout(p.puuid, pl)) {
                        if (pl.loaded) {
                            int cCount = 0;
                            bool kFound = false;
                            std::string kWeapon = "";
                            bool nFound = false;
                            std::string nWeapon = "";
                            bool prFound = false;
                            std::string prWeapon = "";
                            for (const auto& [wId, w] : pl.weapons) {
                                if (w.isCustom) cCount++;
                                std::string outW;
                                if (checkKuronamiWeapon(w.weaponName, w.skinName, w.fullName, outW)) {
                                    kFound = true;
                                    kWeapon = outW;
                                }
                                std::string outN;
                                if (checkNocturnumWeapon(w.weaponName, w.skinName, w.fullName, outN)) {
                                    nFound = true;
                                    nWeapon = outN;
                                }
                                std::string outPr;
                                if (checkPrimordiumWeapon(w.weaponName, w.skinName, w.fullName, outPr)) {
                                    prFound = true;
                                    prWeapon = outPr;
                                }
                            }
                            p.skinsLoaded = true;
                            p.customSkinCount = cCount;
                            p.isBooster = (cCount <= 2);
                            p.hasKuronami = kFound;
                            p.kuronamiWeapon = kWeapon;
                            p.hasNocturnum = nFound;
                            p.nocturnumWeapon = nWeapon;
                            p.hasPrimordium = prFound;
                            p.primordiumWeapon = prWeapon;
                        }
                    }
                }

                // Sync detected parties
                {
                    std::lock_guard<std::recursive_mutex> lkParty(g_partyDetectionMutex);
                    for (auto& p : matchState.players) {
                        if (g_detectedParties.count(p.puuid)) {
                            const auto& dpi = g_detectedParties[p.puuid];
                            p.detectedPartySize = dpi.partySize;
                            p.detectedSharedMatches = dpi.sharedMatches;
                            p.detectedPartyLabel = dpi.partyLabel;
                            p.detectedPartyPartners = dpi.partnerNames.empty() ? "" : dpi.partnerNames[0];
                            if (p.partyId.empty() || p.partyId == "00000000-0000-0000-0000-000000000000") {
                                p.partyId = dpi.partyId;
                            }
                        }
                    }
                }

                g_liveMatchState.players = matchState.players;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } catch (const std::exception& e) {
                Log(std::string("Polling thread error: ") + e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (...) {
                Log("Polling thread error: unknown exception");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        Log("Polling thread exiting");
    });

    // FTXUI Component
    auto renderer = ftxui::Renderer([&] {
        try {
            std::set_terminate(logTerminateHandler);
            SetUnhandledExceptionFilter(logUnhandledException);
            MatchState safeMatchState;
            Session safeSession;
            Lockfile safeLock;
            std::set<std::string> safeOwnedAgents;
            {
                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                safeMatchState = matchState;
                safeSession = session;
                safeLock = lock;
                safeOwnedAgents = g_ownedAgents;
            }

            if (g_showFirstRunModal) {
                try {
                    return renderFirstRunModal(g_mousePressed);
                } catch (...) {
                    g_showFirstRunModal = false;
                }
            }
            if (g_showOpenLogModal) {
                try {
                    return ftxui::dbox({
                        renderFTXUI(safeMatchState, safeSession, safeLock, agentMap, mapNameMap),
                        renderOpenLogModal(g_mousePressed)
                    });
                } catch (...) {
                    g_showOpenLogModal = false;
                }
            }
            if (g_showUpdateModal) {
                try {
                    return renderUpdateModal();
                } catch (...) {
                    g_showUpdateModal = false;
                }
            }
            if (g_showPostMatch) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - g_postMatchStartTime).count();
                if (elapsedSec >= 15) {
                    g_showPostMatch = false;
                } else {
                    try {
                        return renderPostMatchOverlay(g_postMatchInfo);
                    } catch (...) {
                        g_showPostMatch = false;
                    }
                }
            }
            if (g_currentView == AppView::PLAYER_STATS) {
                try {
                    if (g_playerProfileTab == PlayerProfileTab::SKINS) {
                        PlayerWeaponLoadout pLoadout;
                        bool foundPl = getPlayerLoadout(g_selectedPuuid, pLoadout);
                        if (!foundPl) {
                            pLoadout.puuid = g_selectedPuuid;
                            pLoadout.loaded = false;
                        }
                        return renderPlayerSkins(pLoadout);
                    }
                    return renderPlayerStats(agentMap, mapNameMap);
                } catch (...) {
                    g_currentView = AppView::MAIN;
                }
            }
            if (g_currentView == AppView::AGENT_PICK) {
                try {
                    return renderAgentPicker(safeMatchState, mapNameMap, safeOwnedAgents);
                } catch (...) {
                    g_currentView = AppView::MAIN;
                }
            }
            if (false) {
                try {
                    return renderReportEnemyMenu(safeMatchState, safeSession, agentMap);
                } catch (...) {
                    g_currentView = AppView::MAIN;
                }
            }
            if (false) {
                try {
                    return renderChatMenu();
                } catch (...) {
                    g_currentView = AppView::MAIN;
                }
            }
            try {
                return renderFTXUI(safeMatchState, safeSession, safeLock, agentMap, mapNameMap);
            } catch (...) {
                return ftxui::vbox({ftxui::filler(), ftxui::hbox({ftxui::filler(), ftxui::text("Ошибка отрисовки UI. Нажмите Q для выхода.") | ftxui::color(ftxui::Color::Red), ftxui::filler()}), ftxui::filler()});
            }
        } catch (const std::exception& e) {
            Log(std::string("Render fatal: ") + e.what());
            return ftxui::vbox({ftxui::filler(), ftxui::hbox({ftxui::filler(), ftxui::text("Критическая ошибка UI"), ftxui::filler()}), ftxui::filler()});
        } catch (...) {
            Log("Render fatal: unknown");
            return ftxui::vbox({ftxui::filler(), ftxui::hbox({ftxui::filler(), ftxui::text("Критическая ошибка UI"), ftxui::filler()}), ftxui::filler()});
        }
    });

    // Catch events for clicks and quitting
    renderer = ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);

        // Block Ctrl+C from closing app
        if (event == ftxui::Event::Special("\x03") || (event.is_character() && event.character() == "\x03")) {
            return true; 
        }

        if (g_showFirstRunModal) {
            if (event == ftxui::Event::Escape) {
                runPortableMode();
                g_showFirstRunModal = false;
                return true;
            }
            if (event.is_mouse()) {
                g_mouseX = event.mouse().x;
                g_mouseY = event.mouse().y;
                if (event.mouse().button == ftxui::Mouse::Left) {
                    if (event.mouse().motion == ftxui::Mouse::Pressed) {
                        g_mousePressed = true;
                        if (g_firstRunOpt1Box.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "first_run_opt1";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            installToAppDataAndCreateShortcut();
                            g_showFirstRunModal = false;
                            return true;
                        }
                        if (g_firstRunOpt2Box.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "first_run_opt2";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            runPortableMode();
                            g_showFirstRunModal = false;
                            return true;
                        }
                    } else if (event.mouse().motion == ftxui::Mouse::Released) {
                        g_mousePressed = false;
                    }
                }
                return true;
            }
            return true;
        }

        if (g_showOpenLogModal) {
            if (event == ftxui::Event::Escape) {
                g_showOpenLogModal = false;
                return true;
            }
            if (event.is_mouse()) {
                g_mouseX = event.mouse().x;
                g_mouseY = event.mouse().y;
                if (event.mouse().button == ftxui::Mouse::Left) {
                    if (event.mouse().motion == ftxui::Mouse::Pressed) {
                        g_mousePressed = true;
                        if (g_openLogYesBox.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "open_log_yes";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            g_showOpenLogModal = false;
                            openLogFolder();
                            return true;
                        }
                        if (g_openLogNoBox.Contain(event.mouse().x, event.mouse().y) ||
                            !g_openLogModalBox.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "open_log_no";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            g_showOpenLogModal = false;
                            return true;
                        }
                    } else if (event.mouse().motion == ftxui::Mouse::Released) {
                        g_mousePressed = false;
                    }
                }
                return true;
            }
            return true;
        }

        if (g_showUpdateModal) {
            if (event == ftxui::Event::Return || 
                (event.is_character() && (event.character() == "y" || event.character() == "Y" || 
                                          event.character() == "\xd0\xbd" || event.character() == "\xd0\x9d"))) {
                startUpdateDownload();
                return true;
            }
            if (event == ftxui::Event::Escape || 
                (event.is_character() && (event.character() == "n" || event.character() == "N" || 
                                          event.character() == "\xd1\x82" || event.character() == "\xd0\xa2"))) {
                g_showUpdateModal = false;
                return true;
            }
            if (event.is_mouse()) {
                g_mouseX = event.mouse().x;
                g_mouseY = event.mouse().y;
                if (event.mouse().button == ftxui::Mouse::Left) {
                    if (event.mouse().motion == ftxui::Mouse::Pressed) {
                        g_mousePressed = true;
                        if (g_updateNowBox.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "update_now";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            startUpdateDownload();
                            return true;
                        }
                        if (g_updateLaterBox.Contain(event.mouse().x, event.mouse().y)) {
                            g_activeButton = "update_later";
                            g_lastButtonClickTime = std::chrono::steady_clock::now();
                            g_showUpdateModal = false;
                            return true;
                        }
                    } else if (event.mouse().motion == ftxui::Mouse::Released) {
                        g_mousePressed = false;
                    }
                }
                return true;
            }
            return true;
        }

        // Only when in CHAT view: allow typing into input field, Backspace and Return
        if (false) {
            if (event == ftxui::Event::Return) {
                if (!g_chatInputText.empty()) {
                    sendValorantChatMessage(g_chatInputText, g_chatTargetChannel, &screen);
                    g_chatInputText.clear();
                }
                return true;
            }
            if (event == ftxui::Event::Backspace) {
                while (!g_chatInputText.empty() && (static_cast<unsigned char>(g_chatInputText.back()) & 0xC0) == 0x80) {
                    g_chatInputText.pop_back();
                }
                if (!g_chatInputText.empty()) {
                    g_chatInputText.pop_back();
                }
                return true;
            }
            if (event.is_character()) {
                std::string ch = event.character();
                if (g_chatInputText.size() < 120) {
                    g_chatInputText += ch;
                }
                return true;
            }
        }

        // ESC returns to main view from any sub-view, NEVER closes the program!
        if (event == ftxui::Event::Escape) {
            if (g_showPostMatch) {
                g_showPostMatch = false;
                return true;
            } else if (g_dodgeConfirm) {
                g_dodgeConfirm = false;
                return true;
            } else if (g_currentView == AppView::PLAYER_STATS && !g_selectedSkinWeaponUuid.empty()) {
                g_selectedSkinWeaponUuid = "";
                return true;
            } else if (g_currentView != AppView::MAIN) {
                g_selectedSkinWeaponUuid = "";
                g_currentView = AppView::MAIN;
                return true;
            }
            return true;
        }

        // Test preview shortcut for post match animation and rank up (F8)
        if (event == ftxui::Event::F8) {
            g_postMatchInfo.matchId = "preview-match";
            g_postMatchInfo.tierBefore = 14; // Gold 3
            g_postMatchInfo.tierAfter = 15;  // Platinum 1
            g_postMatchInfo.rrBefore = 84;
            g_postMatchInfo.rrAfter = 21;
            g_postMatchInfo.rrEarned = 37;
            g_postMatchInfo.performanceBonus = 5;
            g_postMatchStartTime = std::chrono::steady_clock::now();
            g_showPostMatch = true;
            return true;
        }

        // All other keyboard keys are completely ignored (NO HOTKEYS)! Control is 100% mouse-driven!
        if (!event.is_mouse()) {
            return true;
        }

        bool is_mouse_motion = false;
        if (event.is_mouse()) {
            g_mouseX = event.mouse().x;
            g_mouseY = event.mouse().y;
            is_mouse_motion = true;
            
            if (event.mouse().button == ftxui::Mouse::Left) {
                if (event.mouse().motion == ftxui::Mouse::Pressed) {
                    g_mousePressed = true;
                } else if (event.mouse().motion == ftxui::Mouse::Released) {
                    g_mousePressed = false;
                }
            }
        }

        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left && event.mouse().motion == ftxui::Mouse::Released) {
            if (matchState.phase == "pregame") {
                if (!g_dodgeConfirm) {
                    if (g_dodgeButtonBox.Contain(event.mouse().x, event.mouse().y)) {
                        g_activeButton = "dodge";
                        g_lastButtonClickTime = std::chrono::steady_clock::now();
                        g_dodgeConfirm = true;
                        return true;
                    }
                } else {
                    if (g_dodgeConfirmBox.Contain(event.mouse().x, event.mouse().y)) {
                        g_activeButton = "dodge_confirm";
                        g_lastButtonClickTime = std::chrono::steady_clock::now();
                        g_dodgeConfirm = false;
                        std::thread([session, matchState]() {
                            std::map<std::string, std::string> headers = {
                                {"Authorization", "Bearer " + session.accessToken},
                                {"X-Riot-Entitlements-JWT", session.token},
                                {"X-Riot-ClientPlatform", getClientPlatformBase64()},
                                {"X-Riot-ClientVersion", session.clientVersion}
                            };
                            httpRequest("POST", session.glzHost, 443, "/pregame/v1/matches/" + matchState.matchId + "/quit", headers, "", true, false);
                        }).detach();
                        return true;
                    } else if (g_dodgeCancelBox.Contain(event.mouse().x, event.mouse().y)) {
                        g_activeButton = "dodge_cancel";
                        g_lastButtonClickTime = std::chrono::steady_clock::now();
                        g_dodgeConfirm = false;
                        return true;
                    } else {
                        g_dodgeConfirm = false;
                    }
                }
            }

            if (g_currentView == AppView::MAIN) {
                if (g_hiddenLogBtnBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_showOpenLogModal = true;
                    return true;
                }
                if (g_updateBadgeBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "update_badge";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_showUpdateModal = true;
                    return true;
                }

                // Check "Pick Agent" button in pregame
                if (matchState.phase == "pregame" && g_pickAgentBtnBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "pick_agent_open";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_pickStatusMsg = "";
                    g_currentView = AppView::AGENT_PICK;
                    return true;
                }

                // Check "My Stats" button
                if (g_myStatsBox.Contain(event.mouse().x, event.mouse().y) && !session.puuid.empty()) {
                    g_activeButton = "my_stats";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
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
                            g_selectedPlayerInfo.wins = g_rankCache[session.puuid].wins;
                            g_selectedPlayerInfo.losses = g_rankCache[session.puuid].losses;
                            g_selectedPlayerInfo.peakRankTier = g_rankCache[session.puuid].peakTier;
                            g_selectedPlayerInfo.peakRankRR = g_rankCache[session.puuid].peakRR;
                        }
                    }

                    // Always refresh MMR for local player so current RR and Peak rank are up-to-date
                    {
                        Session pSession = session;
                        std::string pPuuid = session.puuid;
                        std::thread([pSession, pPuuid]() {
                            std::string clientPlatform = getClientPlatformBase64();
                            std::map<std::string, std::string> pdHeaders = {
                                {"Authorization", "Bearer " + pSession.accessToken},
                                {"X-Riot-Entitlements-JWT", pSession.token},
                                {"X-Riot-ClientPlatform", clientPlatform},
                                {"X-Riot-ClientVersion", pSession.clientVersion}
                            };
                            auto mmrRes = httpRequest("GET", pSession.pdHost, 443, "/mmr/v1/players/" + pPuuid, pdHeaders, "", true, false);
                            if (mmrRes.statusCode == 200) {
                                try {
                                    auto j = json::parse(mmrRes.body);
                                    int tier = 0, rr = 0, totalWins = 0, totalGames = 0, peakTier = 0, peakRR = 0;
                                    if (j.contains("LatestCompetitiveUpdate") && !j["LatestCompetitiveUpdate"].is_null()) {
                                        auto lcu = j["LatestCompetitiveUpdate"];
                                        if (lcu.contains("TierAfterUpdate") && !lcu["TierAfterUpdate"].is_null()) tier = lcu["TierAfterUpdate"].get<int>();
                                        if (lcu.contains("RankedRatingAfterUpdate") && !lcu["RankedRatingAfterUpdate"].is_null()) rr = lcu["RankedRatingAfterUpdate"].get<int>();
                                    }
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
                                                        if (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) rr = sData["RankedRating"].get<int>();
                                                    }
                                                }
                                                if (sData.contains("NumberOfGames") && sData["NumberOfGames"].is_number()) totalGames = sData["NumberOfGames"].get<int>();
                                                if (sData.contains("NumberOfWinsWithPlacements") && sData["NumberOfWinsWithPlacements"].is_number()) totalWins = sData["NumberOfWinsWithPlacements"].get<int>();
                                            }
                                            for (const auto& [sId, sData] : seasons.items()) {
                                                if (sData.contains("CompetitiveTier") && !sData["CompetitiveTier"].is_null()) {
                                                    int t = sData["CompetitiveTier"].get<int>();
                                                    if (t > peakTier) {
                                                        peakTier = t;
                                                        peakRR = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                                    } else if (t == peakTier) {
                                                        int r = (sData.contains("RankedRating") && !sData["RankedRating"].is_null()) ? sData["RankedRating"].get<int>() : 0;
                                                        if (r > peakRR) peakRR = r;
                                                    }
                                                }
                                                if (sData.contains("Rank") && sData["Rank"].is_number()) {
                                                    int rk = sData["Rank"].get<int>();
                                                    if (rk > peakTier) { peakTier = rk; peakRR = 0; }
                                                }
                                                if (sData.contains("WinsByTier") && sData["WinsByTier"].is_object()) {
                                                    for (const auto& [tStr, wCount] : sData["WinsByTier"].items()) {
                                                        try {
                                                            int t = std::stoi(tStr);
                                                            if (t > peakTier) { peakTier = t; peakRR = 0; }
                                                        } catch (...) {}
                                                    }
                                                }
                                            }
                                            if (tier > peakTier) { peakTier = tier; peakRR = rr; }
                                        }
                                    }
                                    {
                                        std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                        g_rankCache[pPuuid] = {tier, rr, totalWins, std::max(0, totalGames - totalWins), peakTier, peakRR};
                                        if (g_selectedPuuid == pPuuid) {
                                            g_selectedPlayerInfo.rankTier = tier;
                                            g_selectedPlayerInfo.rankRR = rr;
                                            g_selectedPlayerInfo.wins = totalWins;
                                            g_selectedPlayerInfo.losses = std::max(0, totalGames - totalWins);
                                            g_selectedPlayerInfo.peakRankTier = peakTier;
                                            g_selectedPlayerInfo.peakRankRR = peakRR;
                                        }
                                    }
                                } catch (...) {}
                            }
                        }).detach();
                    }

                    if (g_statsCache.count(session.puuid) && g_statsCache[session.puuid].matchesPlayed <= 5) {
                        g_statsCache.erase(session.puuid);
                    }

                    if (!g_statsCache.count(session.puuid) && !g_statsFetching.count(session.puuid)) {
                        g_selectedPlayerInfo.isLoading = true;
                        g_statsFetching.insert(session.puuid);
                        std::string pGameName = g_selectedPlayerInfo.gameName;
                        std::string pTagLine = g_selectedPlayerInfo.tagLine;
                        std::string pPuuid = session.puuid;
                        Session pSession = session;
                        std::thread([pSession, pPuuid, pGameName, pTagLine]() {
                            fetchPlayerStats(pSession, pPuuid, pGameName, pTagLine);
                        }).detach();
                    } else if (g_statsCache.count(session.puuid)) {
                        const auto& sc = g_statsCache[session.puuid];
                        g_selectedPlayerInfo.kdRatio = sc.kdRatio;
                        g_selectedPlayerInfo.hsPercent = sc.hsPercent;
                        g_selectedPlayerInfo.headshots = sc.headshots;
                        g_selectedPlayerInfo.bodyshots = sc.bodyshots;
                        g_selectedPlayerInfo.legshots = sc.legshots;
                        g_selectedPlayerInfo.matchesWon = sc.matchesWon;
                        g_selectedPlayerInfo.matchesPlayed = sc.matchesPlayed;
                        g_selectedPlayerInfo.acs = sc.acs;
                        g_selectedPlayerInfo.favAgent = sc.favAgent;
                        g_selectedPlayerInfo.isPrivate = sc.isPrivate;
                        g_selectedPlayerInfo.statsSource = sc.source;
                        g_selectedPlayerInfo.isLoading = false;
                    }
                    g_playerProfileTab = PlayerProfileTab::STATS;
                    g_selectedSkinWeaponUuid = "";
                    fetchPlayerLoadoutDirect(session, session.puuid);
                    g_currentView = AppView::PLAYER_STATS;
                    return true;
                }
                
                // Check Player Names
                for (const auto& [puuid, box] : g_playerBoxes) {
                    if (box.Contain(event.mouse().x, event.mouse().y)) {
                        g_activeButton = "player_" + puuid;
                        g_lastButtonClickTime = std::chrono::steady_clock::now();
                        g_selectedPuuid = puuid;
                        for (const auto& p : matchState.players) {
                            if (p.puuid == puuid) {
                                g_selectedPlayerInfo = p;
                                if (g_nameCache.count(puuid)) {
                                    g_selectedPlayerInfo.gameName = g_nameCache[puuid].first;
                                    g_selectedPlayerInfo.tagLine = g_nameCache[puuid].second;
                                }
                                if (g_rankCache.count(puuid)) {
                                    g_selectedPlayerInfo.rankTier = g_rankCache[puuid].tier;
                                    g_selectedPlayerInfo.rankRR = g_rankCache[puuid].rr;
                                    g_selectedPlayerInfo.wins = g_rankCache[puuid].wins;
                                    g_selectedPlayerInfo.losses = g_rankCache[puuid].losses;
                                    g_selectedPlayerInfo.peakRankTier = g_rankCache[puuid].peakTier;
                                    g_selectedPlayerInfo.peakRankRR = g_rankCache[puuid].peakRR;
                                }
                                if (g_statsCache.count(puuid)) {
                                    const auto& sc = g_statsCache[puuid];
                                    g_selectedPlayerInfo.kdRatio = sc.kdRatio;
                                    g_selectedPlayerInfo.hsPercent = sc.hsPercent;
                                    g_selectedPlayerInfo.headshots = sc.headshots;
                                    g_selectedPlayerInfo.bodyshots = sc.bodyshots;
                                    g_selectedPlayerInfo.legshots = sc.legshots;
                                    g_selectedPlayerInfo.matchesWon = sc.matchesWon;
                                    g_selectedPlayerInfo.matchesPlayed = sc.matchesPlayed;
                                    g_selectedPlayerInfo.acs = sc.acs;
                                    g_selectedPlayerInfo.favAgent = sc.favAgent;
                                    g_selectedPlayerInfo.isPrivate = sc.isPrivate;
                                    g_selectedPlayerInfo.statsSource = sc.source;
                                    g_selectedPlayerInfo.isLoading = false;
                                } else if (!g_statsFetching.count(puuid)) {
                                    g_selectedPlayerInfo.isLoading = true;
                                    g_statsFetching.insert(puuid);
                                    std::string pGameName = g_selectedPlayerInfo.gameName;
                                    std::string pTagLine = g_selectedPlayerInfo.tagLine;
                                    std::string pPuuid = puuid;
                                    Session pSession = session;
                                    std::thread([pSession, pPuuid, pGameName, pTagLine]() {
                                        fetchPlayerStats(pSession, pPuuid, pGameName, pTagLine);
                                    }).detach();
                                }
                                break;
                            }
                        }
                        g_playerProfileTab = PlayerProfileTab::STATS;
                        g_selectedSkinWeaponUuid = "";
                        fetchPlayerLoadoutDirect(session, puuid);
                        g_currentView = AppView::PLAYER_STATS;
                        return true;
                    }
                }
            } else if (g_currentView == AppView::PLAYER_STATS) {
                if (!g_selectedSkinWeaponUuid.empty()) {
                    if (g_skinModalCloseBox.Contain(event.mouse().x, event.mouse().y) ||
                        !g_skinModalBox.Contain(event.mouse().x, event.mouse().y)) {
                        g_selectedSkinWeaponUuid = "";
                        return true;
                    }
                    g_selectedSkinWeaponUuid = "";
                    return true;
                }
                if (g_playerProfileTab == PlayerProfileTab::SKINS) {
                    for (const auto& [wUuid, box] : g_skinRowBoxes) {
                        if (box.Contain(event.mouse().x, event.mouse().y)) {
                            g_selectedSkinWeaponUuid = (g_selectedSkinWeaponUuid == wUuid) ? "" : wUuid;
                            return true;
                        }
                    }
                }
                if (g_tabSkinsBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "tab_skins";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_playerProfileTab = PlayerProfileTab::SKINS;
                    g_selectedSkinWeaponUuid = "";
                    fetchPlayerLoadoutDirect(session, g_selectedPuuid);
                    return true;
                }
                if (g_tabStatsBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "tab_stats";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_playerProfileTab = PlayerProfileTab::STATS;
                    g_selectedSkinWeaponUuid = "";
                    return true;
                }
                if (g_openTrackerBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "open_tracker";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    openTrackerInBrowser(g_selectedPlayerInfo.gameName, g_selectedPlayerInfo.tagLine);
                    return true;
                }
                if (g_backButtonBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "back";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_selectedSkinWeaponUuid = "";
                    g_currentView = AppView::MAIN;
                    return true;
                }
            } else if (g_currentView == AppView::AGENT_PICK) {
                if (g_agentPickerBackBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "agent_picker_back";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_currentView = AppView::MAIN;
                    return true;
                }
                for (const auto& [agentId, box] : g_agentBoxes) {
                    if (box.Contain(event.mouse().x, event.mouse().y) && !g_isLockingAgent) {
                        g_activeButton = "agent_" + agentId;
                        g_lastButtonClickTime = std::chrono::steady_clock::now();
                        g_isLockingAgent = true;

                        std::string curAgentName = agentId;
                        if (g_globalAgentMap.count(agentId)) curAgentName = g_globalAgentMap[agentId];
                        g_pickStatusMsg = "Блокируем " + curAgentName + "...";

                        std::string mId = matchState.matchId;
                        Session sess = session;
                        std::thread([sess, mId, agentId, curAgentName, &screen]() {
                            std::string err;
                            bool ok = lockInAgent(sess, mId, agentId, err);
                            {
                                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                g_isLockingAgent = false;
                                if (ok) {
                                    g_pickStatusMsg = "[OK] УСПЕШНО ЗАБЛОКИРОВАН: " + curAgentName + "!";
                                } else {
                                    g_pickStatusMsg = "[ОШИБКА] " + err;
                                }
                            }
                            screen.PostEvent(ftxui::Event::Custom);
                            if (ok) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(900));
                                {
                                    std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                    g_currentView = AppView::MAIN;
                                }
                                screen.PostEvent(ftxui::Event::Custom);
                            }
                        }).detach();
                        return true;
                    }
                }
            } else if (false) {
                if (g_chatCloseBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "chat_close";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_currentView = AppView::MAIN;
                    return true;
                }
                if (g_chatClearBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "chat_clear";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    std::lock_guard<std::mutex> lk(g_chatMutex);
                    g_chatMessages.clear();
                    return true;
                }
                if (g_chatChannelBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "chat_channel";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_chatTargetChannel = (g_chatTargetChannel == "team") ? "all" : "team";
                    return true;
                }
                if (g_chatTranslateToggleBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "chat_translate";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_chatAutoTranslateRuToEn = !g_chatAutoTranslateRuToEn;
                    return true;
                }
                if (g_chatSendBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "chat_send";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    if (!g_chatInputText.empty()) {
                        sendValorantChatMessage(g_chatInputText, g_chatTargetChannel, &screen);
                        g_chatInputText.clear();
                    }
                    return true;
                }
            } else if (false) {
                if (g_reportCloseBox.Contain(event.mouse().x, event.mouse().y)) {
                    g_activeButton = "report_close";
                    g_lastButtonClickTime = std::chrono::steady_clock::now();
                    g_currentView = AppView::MAIN;
                    return true;
                }
                for (const auto& [puuid, box] : g_reportEnemyBoxes) {
                    if (box.Contain(event.mouse().x, event.mouse().y)) {
                        g_activeButton = "report_" + puuid;
                        g_lastButtonClickTime = std::chrono::steady_clock::now();

                        std::string agName = "Враг";
                        for (const auto& p : matchState.players) {
                            if (p.puuid == puuid) {
                                if (!p.agentName.empty()) {
                                    agName = p.agentName;
                                } else if (!p.characterId.empty()) {
                                    std::string cid = p.characterId;
                                    for (auto& c : cid) c = tolower(c);
                                    if (agentMap.count(cid)) agName = agentMap.at(cid);
                                    else if (agentMap.count(p.characterId)) agName = agentMap.at(p.characterId);
                                }
                                break;
                            }
                        }

                        g_reportStatusMsg = "Отправка репорта на " + agName + "...";
                        g_reportedPuuids.insert(puuid);

                        std::string mId = matchState.matchId;
                        Session sess = session;
                        Lockfile lkFile = lock;
                        std::thread([sess, lkFile, mId, puuid, agName, &screen]() {
                            std::string msg;
                            bool ok = sendPlayerReport(sess, lkFile, mId, puuid, agName, msg);
                            {
                                std::lock_guard<std::recursive_mutex> lk(g_mutex);
                                g_reportStatusMsg = msg;
                                if (ok) {
                                    g_reportedPuuids.insert(puuid);
                                }
                            }
                            screen.PostEvent(ftxui::Event::Custom);
                        }).detach();
                        return true;
                    }
                }
            }
        }
        
        return is_mouse_motion;
    });

    screen.Loop(renderer);
    
    g_running = false;
    if (polling_thread.joinable()) {
        polling_thread.join();
    }

    {
        std::lock_guard<std::mutex> lk(g_httpMutex);
        if (g_hSession) { WinHttpCloseHandle(g_hSession); g_hSession = nullptr; }
    }
    return 0;
}
