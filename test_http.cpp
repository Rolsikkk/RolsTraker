#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#pragma comment(lib, "winhttp.lib")

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
    HINTERNET hSession = WinHttpOpen(L"RolsTraker/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return response;

    std::wstring wHost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return response; }

    std::wstring wMethod(method.begin(), method.end());
    std::wstring wPath(path.begin(), path.end());
    DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return response; }

    std::wstring wHeaders;
    for (const auto& kv : headers) wHeaders += std::wstring(kv.first.begin(), kv.first.end()) + L": " + std::wstring(kv.second.begin(), kv.second.end()) + L"\r\n";

    BOOL bResults = WinHttpSendRequest(hRequest, wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(), (DWORD)wHeaders.length(), bodyData.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)bodyData.c_str(), (DWORD)bodyData.length(), (DWORD)bodyData.length(), 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

    if (bResults) {
        DWORD dwStatusCode = 0; DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = dwStatusCode;

        DWORD dwDownloaded = 0;
        while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
            std::vector<char> buffer(dwSize);
            if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded) && dwDownloaded > 0) {
                response.body.append(buffer.data(), dwDownloaded);
            } else break;
        }
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return response;
}

int main() {
    auto res = httpRequest("GET", "valorant-api.com", 443, "/v1/agents", {}, "", true, false);
    std::cout << "Agents Status: " << res.statusCode << " Body: " << res.body.substr(0, 100) << "\n";
    if (res.statusCode == 200) {
        try {
            auto j = json::parse(res.body);
            std::cout << "Parsed json. Elements: " << j["data"].size() << "\n";
            for (const auto& item : j["data"]) {
                std::cout << item["uuid"].get<std::string>() << " -> " << item["displayName"].get<std::string>() << "\n";
                break;
            }
        } catch (const std::exception& e) {
            std::cout << "JSON parse error: " << e.what() << "\n";
        }
    }
    return 0;
}
