#pragma once
#include <windows.h>
#include <string>

inline void setClipboardTextUTF8(const std::string& utf8_text) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, NULL, 0);
    if (wlen == 0) return;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (!hg) return;
    wchar_t* wstr = (wchar_t*)GlobalLock(hg);
    MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, wstr, wlen);
    GlobalUnlock(hg);
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hg);
        CloseClipboard();
    }
}
