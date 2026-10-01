#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

// (Copy httpRequest, readLockfile, getSession from main.cpp)
// Actually I can just write a quick powershell script to do the request, it's faster.
