#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "winhttp.lib")

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
            path = "/?";
        }

        HINTERNET hSession = WinHttpOpen(L"RolsTraker-Downloader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        std::wstring wHost(host.begin(), host.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), 443, 0);
        std::wstring wPath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        
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
                    continue;
                }
            }

            if (dwStatusCode == 200) {
                std::ofstream outFile(outputPath, std::ios::binary);
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
                return true;
            }
        }
        break;
    }
    return false;
}

int main() {
    downloadFileWithRedirects("https://github.com/Rolsikkk/RolsTraker/releases/download/v1.1.8/RolsTraker.exe", "test.exe");
    return 0;
}
