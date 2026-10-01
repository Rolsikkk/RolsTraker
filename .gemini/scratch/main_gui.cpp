/*
 * RolsTraker v2.0.0 — GUI Edition
 * Native Win32 Dark-Theme Valorant Match Tracker
 * Shows: Live Players, Rank, K/D (last 5 games), W/L (last 5 games), Party Groups
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>

#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <cmath>

#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

using json = nlohmann::json;

// ─── Version ─────────────────────────────────────────────────────────────────
static const std::string APP_VERSION = "v2.0.0";
static const std::string GITHUB_REPO = "Rolsikkk/RolsTraker";

// ─── Window Layout ───────────────────────────────────────────────────────────
static const int WIN_W   = 900;
static const int WIN_H   = 800;
static const int TB_H    = 40;   // title bar
static const int SB_H    = 46;   // status bar
static const int FOOT_H  = 0;    // footer removed
static const int THDR_H  = 32;   // team section header
static const int CHDR_H  = 22;   // column header row
static const int ROW_H   = 42;   // player row
static const int PROW_H  = 28;   // party row
static const int PSEC_H  = 30;   // party section header
static const int PAD     = 14;   // horizontal padding

// column x-offsets from left edge (PAD already included in first)
static const int CX_MARK  = PAD;        // 14  — party circle
static const int CX_NAME  = PAD + 24;   // 38  — player name
static const int CX_AGENT = PAD + 238;  // 252 — agent
static const int CX_RANK  = PAD + 360;  // 374 — rank
static const int CX_KD    = PAD + 548;  // 562 — K/D
static const int CX_WL    = PAD + 620;  // 634 — W/L
static const int CX_PARTY = PAD + 758;  // 772 — party badge

// ─── Colors ──────────────────────────────────────────────────────────────────
#define C_BG       RGB(13,17,23)
#define C_PANEL    RGB(22,27,34)
#define C_PANEL2   RGB(28,36,48)
#define C_BORDER   RGB(48,54,61)
#define C_TBARBG   RGB(8,12,18)
#define C_TEXT     RGB(230,237,243)
#define C_DIM      RGB(139,148,158)
#define C_ACCENT   RGB(255,70,85)
#define C_BLUE     RGB(88,166,255)
#define C_GREEN    RGB(63,185,80)
#define C_YELLOW   RGB(210,153,34)
#define C_MAGENTA  RGB(188,140,233)
#define C_CYAN     RGB(56,200,200)
#define C_RED      RGB(255,80,80)
// Rank tier colors
#define C_IRON     RGB(170,170,170)
#define C_BRONZE   RGB(165,113,78)
#define C_SILVER   RGB(200,200,200)
#define C_GOLD     RGB(255,200,50)
#define C_PLAT     RGB(56,200,200)
#define C_DIAM     RGB(188,140,233)
#define C_ASCE     RGB(63,185,80)
#define C_IMMORT   RGB(255,90,90)
#define C_RADIANT  RGB(255,220,70)
#define C_UNRATED  RGB(139,148,158)

static const COLORREF PARTY_CLRS[6] = { C_GREEN, C_BLUE, C_YELLOW, C_MAGENTA, C_ACCENT, C_CYAN };

// ─── Data Structures ─────────────────────────────────────────────────────────
struct HttpResponse { int statusCode = 0; std::string body; };

struct Lockfile { uint16_t port = 0; std::string password; };

struct Session {
    std::string accessToken, token, puuid, region, shard, clientVersion, glzHost, pdHost;
};

struct PlayerInfo {
    std::string puuid, teamId, characterId, partyId, gameName, tagLine;
    int   rankTier = 0;
    int   rankRR   = 0;
    double kdRatio = -1.0;  // -1=loading, -2=unavailable
    int   wins     = -1;    // -1=loading, -2=unavailable
    int   losses   = -1;
};

struct MatchState {
    std::string phase;   // "coregame","pregame","none"
    std::string matchId, mapId;
    std::vector<PlayerInfo> players;
};

struct PlayerStats {
    double kdRatio = -1.0;
    int wins = -1, losses = -1;
    bool loaded = false;
};

// ─── Global State ─────────────────────────────────────────────────────────────
static HWND g_hWnd = nullptr;
static std::mutex g_mutex;
static std::atomic<bool> g_running{true};
static Lockfile  g_lock;
static Session   g_session;
static MatchState g_matchState;
static std::map<std::string,std::string> g_agentMap, g_mapNameMap;
static std::map<std::string,PlayerStats> g_statsCache;
static std::set<std::string> g_statsFetching;
static std::map<std::string,std::pair<std::string,std::string>> g_nameCache;
static std::map<std::string,std::pair<int,int>> g_rankCache;
static int  g_scrollY = 0;
static int  g_totalContentH = 600;
static bool g_updateAvailable = false;
static std::string g_latestVersion;
static bool g_hClose = false, g_hMin = false;

// GUI font handles
static HFONT g_fTitle  = nullptr;
static HFONT g_fBold   = nullptr;
static HFONT g_fNormal = nullptr;
static HFONT g_fSmall  = nullptr;
static HFONT g_fMono   = nullptr;

// ═════════════════════════════════════════════════════════════════════════════
// BASE64
// ═════════════════════════════════════════════════════════════════════════════
static const std::string B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64enc(const std::string& in) {
    std::string out; int v=0,vb=-6;
    for (uint8_t c:in){v=(v<<8)+c;vb+=8;while(vb>=0){out+=B64[(v>>vb)&63];vb-=6;}}
    if(vb>-6)out+=B64[((v<<8)>>(vb+8))&63];
    while(out.size()%4)out+='=';
    return out;
}
std::string b64dec(const std::string& in) {
    std::vector<int>T(256,-1);for(int i=0;i<64;i++)T[(uint8_t)B64[i]]=i;
    std::string out;int v=0,vb=-8;
    for(uint8_t c:in){if(T[c]==-1)break;v=(v<<6)+T[c];vb+=6;if(vb>=0){out+=(char)((v>>vb)&255);vb-=8;}}
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// UPDATE CHECK  (GitHub API — checks latest release tag vs APP_VERSION)
// ═════════════════════════════════════════════════════════════════════════════
void checkForUpdate(){
    HINTERNET hS=WinHttpOpen(L"RolsTraker/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!hS)return;
    HINTERNET hC=WinHttpConnect(hS,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!hC){WinHttpCloseHandle(hS);return;}
    std::wstring wPath=L"/repos/"+std::wstring(GITHUB_REPO.begin(),GITHUB_REPO.end())+L"/releases/latest";
    HINTERNET hR=WinHttpOpenRequest(hC,L"GET",wPath.c_str(),NULL,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!hR){WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return;}
    std::wstring wH=L"User-Agent: RolsTraker/2.0\r\nAccept: application/vnd.github+json\r\n";
    if(WinHttpSendRequest(hR,wH.c_str(),(DWORD)wH.length(),WINHTTP_NO_REQUEST_DATA,0,0,0)
        &&WinHttpReceiveResponse(hR,NULL)){
        DWORD sc=0,sz=sizeof(sc);
        WinHttpQueryHeaders(hR,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,&sc,&sz,WINHTTP_NO_HEADER_INDEX);
        if(sc==200){
            std::string body;DWORD dw=0;
            do{sz=0;if(!WinHttpQueryDataAvailable(hR,&sz)||sz==0)break;
               std::vector<char>buf(sz+1);
               if(WinHttpReadData(hR,(LPVOID)buf.data(),sz,&dw))body.append(buf.data(),dw);
            }while(sz>0);
            try{
                auto j=nlohmann::json::parse(body);
                if(j.contains("tag_name")&&j["tag_name"].is_string()){
                    std::string latest=j["tag_name"].get<std::string>();
                    
                    auto parseVer = [](const std::string& v) {
                        int mj=0, mn=0, pt=0; const char* p = v.c_str();
                        if(*p=='v'||*p=='V') p++;
                        sscanf_s(p, "%d.%d.%d", &mj, &mn, &pt);
                        return std::make_tuple(mj, mn, pt);
                    };
                    
                    if(!latest.empty() && parseVer(latest) > parseVer(APP_VERSION)){
                        g_latestVersion=latest;
                        g_updateAvailable=true;
                        if(g_hWnd)PostMessage(g_hWnd,WM_APP+1,0,0);
                    }
                }
            }catch(...){}
        }
    }
    WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);
}

// ═════════════════════════════════════════════════════════════════════════════
// HTTP (WinHTTP)
// ═════════════════════════════════════════════════════════════════════════════
HttpResponse httpReq(const std::string& method, const std::string& host, INTERNET_PORT port,
    const std::string& path, const std::map<std::string,std::string>& hdrs,
    const std::string& body="", bool https=true, bool ignoreCert=false)
{
    HttpResponse r;
    HINTERNET hS=WinHttpOpen(L"RolsTraker/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!hS)return r;
    std::wstring wHost(host.begin(),host.end());
    HINTERNET hC=WinHttpConnect(hS,wHost.c_str(),port,0);
    if(!hC){WinHttpCloseHandle(hS);return r;}
    std::wstring wM(method.begin(),method.end()),wP(path.begin(),path.end());
    HINTERNET hR=WinHttpOpenRequest(hC,wM.c_str(),wP.c_str(),NULL,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,https?WINHTTP_FLAG_SECURE:0);
    if(!hR){WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return r;}
    if(ignoreCert&&https){
        DWORD sf=SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE|
                 SECURITY_FLAG_IGNORE_CERT_CN_INVALID|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hR,WINHTTP_OPTION_SECURITY_FLAGS,&sf,sizeof(sf));
    }
    std::wstring wH;
    for(auto&[k,v]:hdrs)
        wH+=std::wstring(k.begin(),k.end())+L": "+std::wstring(v.begin(),v.end())+L"\r\n";
    BOOL ok=WinHttpSendRequest(hR,wH.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:wH.c_str(),(DWORD)wH.length(),
        body.empty()?WINHTTP_NO_REQUEST_DATA:(LPVOID)body.c_str(),(DWORD)body.length(),(DWORD)body.length(),0);
    if(ok)ok=WinHttpReceiveResponse(hR,NULL);
    if(ok){
        DWORD sc=0,sz=sizeof(sc);
        WinHttpQueryHeaders(hR,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,&sc,&sz,WINHTTP_NO_HEADER_INDEX);
        r.statusCode=(int)sc;
        DWORD dw=0;
        do{sz=0;if(!WinHttpQueryDataAvailable(hR,&sz)||sz==0)break;
           std::vector<char>buf(sz+1);
           if(WinHttpReadData(hR,(LPVOID)buf.data(),sz,&dw))r.body.append(buf.data(),dw);
        }while(sz>0);
    }
    WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);
    return r;
}

std::string jStr(const json& j,const std::vector<std::string>& keys){
    for(auto&k:keys)if(j.contains(k)&&!j[k].is_null()&&j[k].is_string())return j[k].get<std::string>();
    return "";
}

// ═════════════════════════════════════════════════════════════════════════════
// RIOT API LAYER
// ═════════════════════════════════════════════════════════════════════════════
Lockfile readLockfile(){
    Lockfile lf;
    char* la=nullptr;size_t len=0;
    if(_dupenv_s(&la,&len,"LOCALAPPDATA")!=0||!la)return lf;
    std::string base=la;free(la);
    std::vector<std::string> paths={
        base+"\\Riot Games\\Riot Games\\Config\\lockfile",
        base+"\\Riot Games\\Riot Client\\Config\\lockfile",
        base+"\\Riot Games\\Config\\lockfile"};
    for(auto&p:paths){
        std::ifstream f(p);if(!f.is_open())continue;
        std::string ln;getline(f,ln);
        std::stringstream ss(ln);std::vector<std::string>parts;std::string pt;
        while(getline(ss,pt,':'))parts.push_back(pt);
        if(parts.size()>=5){lf.port=(uint16_t)stoi(parts[2]);lf.password=parts[3];return lf;}
    }
    return lf;
}

std::string getClientVersion(){
    auto r=httpReq("GET","valorant-api.com",443,"/v1/version",{});
    if(r.statusCode==200){try{return json::parse(r.body)["data"]["riotClientVersion"].get<std::string>();}catch(...){}}
    return "release-09.05-shipping-15-2748173";
}

std::pair<std::string,std::string> readShardFromLog(){
    char* la=nullptr;size_t len=0;
    if(_dupenv_s(&la,&len,"LOCALAPPDATA")==0&&la){
        std::string lp=std::string(la)+"\\VALORANT\\Saved\\Logs\\ShooterGame.log";free(la);
        std::ifstream f(lp);
        if(f.is_open()){
            std::stringstream buf;buf<<f.rdbuf();std::string c=buf.str();
            std::regex re("glz-([a-zA-Z0-9]+)-1\\.([a-zA-Z0-9]+)\\.a\\.pvp\\.net",std::regex::icase);
            std::smatch m;
            if(std::regex_search(c,m,re)&&m.size()>=3){
                std::string reg=m[1].str(),sh=m[2].str();
                for(auto&x:reg)x=tolower(x);for(auto&x:sh)x=tolower(x);
                return{reg,sh};
            }
        }
    }
    return{"",""};
}

Session getSession(const Lockfile& lock){
    Session s;
    if(lock.port==0)return s;
    std::string auth="Basic "+b64enc("riot:"+lock.password);
    auto r=httpReq("GET","127.0.0.1",lock.port,"/entitlements/v1/token",{{"Authorization",auth}},"",true,true);
    if(r.statusCode!=200)return s;
    try{auto j=json::parse(r.body);s.accessToken=jStr(j,{"accessToken"});s.token=jStr(j,{"token"});s.puuid=jStr(j,{"subject","puuid"});}catch(...){return s;}
    auto ls=readShardFromLog();
    if(!ls.first.empty()){s.region=ls.first;s.shard=ls.second;}
    else{
        auto rr=httpReq("GET","127.0.0.1",lock.port,"/riotclient/region-locale",{{"Authorization",auth}},"",true,true);
        if(rr.statusCode==200){try{auto j=json::parse(rr.body);s.region=jStr(j,{"region"});for(auto&c:s.region)c=tolower(c);if(s.region=="latam"||s.region=="br")s.shard="na";else s.shard=s.region;}catch(...){}}
    }
    if(s.region.empty())s.region="eu";if(s.shard.empty())s.shard="eu";
    s.clientVersion=getClientVersion();
    s.glzHost="glz-"+s.region+"-1."+s.shard+".a.pvp.net";
    s.pdHost="pd."+s.shard+".a.pvp.net";
    return s;
}

void loadAgentsAndMaps(){
    auto r=httpReq("GET","valorant-api.com",443,"/v1/agents?isPlayableCharacter=true",{});
    if(r.statusCode==200){try{std::lock_guard<std::mutex>lk(g_mutex);for(auto&item:json::parse(r.body)["data"])g_agentMap[item["uuid"].get<std::string>()]=item["displayName"].get<std::string>();}catch(...){}}
    r=httpReq("GET","valorant-api.com",443,"/v1/maps",{});
    if(r.statusCode==200){try{std::lock_guard<std::mutex>lk(g_mutex);for(auto&item:json::parse(r.body)["data"]){if(item.contains("mapUrl")&&!item["mapUrl"].is_null())g_mapNameMap[item["mapUrl"].get<std::string>()]=item["displayName"].get<std::string>();}}catch(...){}}
}

std::string getPlatform(){
    json j={{"platformType","PC"},{"platformOS","Windows"},{"platformOSVersion","10.0.19042.1.256.64bit"},{"platformChipset","Unknown"}};
    return b64enc(j.dump());
}

std::map<std::string,std::string> getPresences(const Lockfile& lock){
    std::map<std::string,std::string>m;
    std::string auth="Basic "+b64enc("riot:"+lock.password);
    auto r=httpReq("GET","127.0.0.1",lock.port,"/chat/v4/presences",{{"Authorization",auth}},"",true,true);
    if(r.statusCode==200){try{auto j=json::parse(r.body);if(j.contains("presences"))for(auto&p:j["presences"]){std::string pu=jStr(p,{"puuid"});std::string priv=jStr(p,{"private"});if(pu.empty()||priv.empty())continue;try{auto pj=json::parse(b64dec(priv));std::string pid=jStr(pj,{"partyId","partyID","party_id"});if(!pid.empty())m[pu]=pid;}catch(...){}}}catch(...){}}
    return m;
}

MatchState getLiveMatch(const Session& sess,const Lockfile& lock){
    MatchState st;st.phase="none";
    std::string cp=getPlatform();
    std::map<std::string,std::string>h={{"Authorization","Bearer "+sess.accessToken},{"X-Riot-Entitlements-JWT",sess.token},{"X-Riot-ClientPlatform",cp},{"X-Riot-ClientVersion",sess.clientVersion}};
    // Core game
    auto cr=httpReq("GET",sess.glzHost,443,"/core-game/v1/players/"+sess.puuid,h);
    if(cr.statusCode==200){try{
        auto jp=json::parse(cr.body);std::string mid=jStr(jp,{"MatchID","matchId"});
        auto mr=httpReq("GET",sess.glzHost,443,"/core-game/v1/matches/"+mid,h);
        if(mr.statusCode==200){
            auto jm=json::parse(mr.body);st.phase="coregame";st.matchId=mid;st.mapId=jStr(jm,{"MapID","mapId"});
            if(jm.contains("Players")&&jm["Players"].is_array())
                for(auto&p:jm["Players"]){PlayerInfo pi;pi.puuid=jStr(p,{"Subject","subject","puuid"});pi.teamId=jStr(p,{"TeamID","teamId"});pi.characterId=jStr(p,{"CharacterID","characterId"});pi.partyId=jStr(p,{"PartyID","partyId"});st.players.push_back(pi);}
            return st;
        }
    }catch(...){}}
    // Pregame
    auto pr=httpReq("GET",sess.glzHost,443,"/pregame/v1/players/"+sess.puuid,h);
    if(pr.statusCode==200){try{
        auto jp=json::parse(pr.body);std::string mid=jStr(jp,{"MatchID","matchId"});
        auto mr=httpReq("GET",sess.glzHost,443,"/pregame/v1/matches/"+mid,h);
        if(mr.statusCode==200){
            auto jm=json::parse(mr.body);st.phase="pregame";st.matchId=mid;st.mapId=jStr(jm,{"MapID","mapId"});
            if(jm.contains("Teams")&&jm["Teams"].is_array()){
                for(auto&team:jm["Teams"]){std::string tid=jStr(team,{"TeamID","teamId"});if(tid.empty())tid="Ally";
                    if(team.contains("Players")&&team["Players"].is_array())
                        for(auto&p:team["Players"]){PlayerInfo pi;pi.puuid=jStr(p,{"Subject","subject","puuid"});pi.teamId=tid;pi.characterId=jStr(p,{"CharacterID","characterId"});pi.partyId=jStr(p,{"PartyID","partyId"});st.players.push_back(pi);}}}
            else if(jm.contains("AllyTeam")&&jm["AllyTeam"].contains("Players"))
                for(auto&p:jm["AllyTeam"]["Players"]){PlayerInfo pi;pi.puuid=jStr(p,{"Subject","subject","puuid"});pi.teamId="Ally";pi.characterId=jStr(p,{"CharacterID","characterId"});pi.partyId=jStr(p,{"PartyID","partyId"});st.players.push_back(pi);}
            return st;
        }
    }catch(...){}}
    return st;
}

void resolveNamesAndRanks(const Session& sess,std::vector<PlayerInfo>& players){
    if(players.empty())return;
    std::vector<std::string> needNames;
    std::vector<PlayerInfo*> needRanks;
    for(auto&p:players){
        if(g_nameCache.count(p.puuid)){p.gameName=g_nameCache[p.puuid].first;p.tagLine=g_nameCache[p.puuid].second;}
        else needNames.push_back(p.puuid);
        if(g_rankCache.count(p.puuid)){p.rankTier=g_rankCache[p.puuid].first;p.rankRR=g_rankCache[p.puuid].second;}
        else needRanks.push_back(&p);
    }
    std::string cp=getPlatform();
    std::map<std::string,std::string>pdH={{"Authorization","Bearer "+sess.accessToken},{"X-Riot-Entitlements-JWT",sess.token},{"X-Riot-ClientPlatform",cp},{"X-Riot-ClientVersion",sess.clientVersion}};
    if(!needNames.empty()){
        json arr=json::array();for(auto&id:needNames)arr.push_back(id);
        auto nh=pdH;nh["Content-Type"]="application/json";
        auto nr=httpReq("PUT",sess.pdHost,443,"/name-service/v2/players",nh,arr.dump());
        if(nr.statusCode==200){try{for(auto&item:json::parse(nr.body)){std::string subj=jStr(item,{"Subject","subject"});std::string gn=jStr(item,{"GameName","gameName"});std::string tl=jStr(item,{"TagLine","tagLine"});if(gn.empty())gn="Player";g_nameCache[subj]={gn,tl};}}catch(...){}}
        for(auto&p:players){if(g_nameCache.count(p.puuid)){p.gameName=g_nameCache[p.puuid].first;p.tagLine=g_nameCache[p.puuid].second;}else if(p.gameName.empty()){p.gameName="Player";p.tagLine="VAL";}}
    }
    if(!needRanks.empty()){
        std::vector<std::thread>threads;std::mutex mu;
        for(auto*pp:needRanks){
            threads.emplace_back([pp,sess,pdH,&mu](){
                auto r=httpReq("GET",sess.pdHost,443,"/mmr/v1/players/"+pp->puuid,pdH);
                if(r.statusCode==200){int tier=0,rr=0;try{
                    auto j=json::parse(r.body);
                    if(j.contains("LatestCompetitiveUpdate")&&!j["LatestCompetitiveUpdate"].is_null()){auto&lcu=j["LatestCompetitiveUpdate"];if(lcu.contains("TierAfterUpdate")&&!lcu["TierAfterUpdate"].is_null())tier=lcu["TierAfterUpdate"].get<int>();if(lcu.contains("RankedRatingAfterUpdate")&&!lcu["RankedRatingAfterUpdate"].is_null())rr=lcu["RankedRatingAfterUpdate"].get<int>();}
                    if(j.contains("QueueSkills")&&j["QueueSkills"].contains("competitive")){auto&comp=j["QueueSkills"]["competitive"];std::string lsid;if(j.contains("LatestCompetitiveUpdate")&&!j["LatestCompetitiveUpdate"].is_null())lsid=jStr(j["LatestCompetitiveUpdate"],{"SeasonID"});if(comp.contains("SeasonalInfoBySeasonID")&&comp["SeasonalInfoBySeasonID"].is_object()){auto&seasons=comp["SeasonalInfoBySeasonID"];if(!lsid.empty()&&seasons.contains(lsid)){auto&sd=seasons[lsid];if(sd.contains("CompetitiveTier")&&!sd["CompetitiveTier"].is_null()){int t=sd["CompetitiveTier"].get<int>();if(t>0){tier=t;if(sd.contains("RankedRating")&&!sd["RankedRating"].is_null())rr=sd["RankedRating"].get<int>();}}}if(tier==0)for(auto&[sid,sd]:seasons.items())if(sd.contains("CompetitiveTier")&&!sd["CompetitiveTier"].is_null()){int t=sd["CompetitiveTier"].get<int>();if(t>0){tier=t;if(sd.contains("RankedRating")&&!sd["RankedRating"].is_null())rr=sd["RankedRating"].get<int>();break;}}}}
                }catch(...){}
                std::lock_guard<std::mutex>lk(mu);g_rankCache[pp->puuid]={tier,rr};pp->rankTier=tier;pp->rankRR=rr;}
            });
        }
        for(auto&t:threads)if(t.joinable())t.join();
    }
}

// ─── Fetch K/D + W/L from match history + competitive updates (fire-and-forget) ─
void fetchPlayerStats(Session sess,std::string puuid){
    if(!g_running)return;
    std::string cp=getPlatform();
    std::map<std::string,std::string>pdH={{"Authorization","Bearer "+sess.accessToken},{"X-Riot-Entitlements-JWT",sess.token},{"X-Riot-ClientPlatform",cp},{"X-Riot-ClientVersion",sess.clientVersion}};

    // === W/L: from competitive updates endpoint (same auth as MMR, very reliable) ===
    int cuWins=0,cuLosses=0;bool wlOk=false;
    auto cur=httpReq("GET",sess.pdHost,443,"/mmr/v1/players/"+puuid+"/competitiveupdates?startIndex=0&endIndex=5&queue=competitive",pdH);
    if(cur.statusCode==200){try{
        auto j=json::parse(cur.body);
        if(j.contains("Matches")&&j["Matches"].is_array())
            for(auto&m:j["Matches"]){
                if(!g_running)return;
                int rr=0;
                if(m.contains("RankedRatingEarned")&&m["RankedRatingEarned"].is_number())rr=m["RankedRatingEarned"].get<int>();
                // Also check TierAfterUpdate vs TierBeforeUpdate for more accurate win detection
                int tierAfter=0,tierBefore=0;
                if(m.contains("TierAfterUpdate")&&m["TierAfterUpdate"].is_number())tierAfter=m["TierAfterUpdate"].get<int>();
                if(m.contains("TierBeforeUpdate")&&m["TierBeforeUpdate"].is_number())tierBefore=m["TierBeforeUpdate"].get<int>();
                // If tier went up, definitely win. If RR > 0, probably win.
                if(rr>0||tierAfter>tierBefore)cuWins++;else cuLosses++;
                wlOk=true;
            }
    }catch(...){}}

    // === K/D: from match list + match details ===
    auto mlr=httpReq("GET",sess.pdHost,443,"/match/v1/matchlist/"+puuid,pdH);
    if(!g_running)return;

    std::vector<std::string>matchIds;
    bool apiOk=(mlr.statusCode==200);
    if(apiOk){try{
        auto j=json::parse(mlr.body);
        if(j.contains("History")&&j["History"].is_array())
            for(auto&item:j["History"]){
                if(!g_running)return;
                std::string mid;
                if(item.contains("MatchID")&&item["MatchID"].is_string())mid=item["MatchID"].get<std::string>();
                if(!mid.empty()){matchIds.push_back(mid);if(matchIds.size()>=3)break;}
            }
    }catch(...){}}

    int totalK=0,totalD=0,totalW=0,totalL=0;
    for(auto&mid:matchIds){
        if(!g_running)return;
        auto mr=httpReq("GET",sess.pdHost,443,"/match/v1/matches/"+mid,pdH);
        if(mr.statusCode!=200)continue;
        try{
            auto j=json::parse(mr.body);
            int kills=0,deaths=0;std::string myTeam;
            if(j.contains("players")&&j["players"].is_array())
                for(auto&p:j["players"]){
                    std::string sub=jStr(p,{"subject","Subject"});if(sub!=puuid)continue;
                    myTeam=jStr(p,{"teamId","TeamID"});
                    if(p.contains("stats")&&p["stats"].is_object()){auto&st=p["stats"];if(st.contains("kills")&&st["kills"].is_number())kills=st["kills"].get<int>();if(st.contains("deaths")&&st["deaths"].is_number())deaths=st["deaths"].get<int>();}
                    break;
                }
            totalK+=kills;totalD+=deaths;
            if(!myTeam.empty()&&j.contains("teams")&&j["teams"].is_array())
                for(auto&team:j["teams"]){
                    std::string tid=jStr(team,{"teamId","TeamID"});if(tid!=myTeam)continue;
                    bool won=false;if(team.contains("won")&&team["won"].is_boolean())won=team["won"].get<bool>();
                    if(won)totalW++;else totalL++;break;
                }
        }catch(...){}
    }
    if(!g_running)return;

    PlayerStats ps;
    // W/L: prefer competitive updates (reliable), fall back to match history
    if(wlOk){
        ps.wins=cuWins;ps.losses=cuLosses;
    }else if(apiOk&&!matchIds.empty()){
        ps.wins=totalW;ps.losses=totalL;
    }else{
        ps.wins=-2;ps.losses=-2;
    }
    // K/D from match history
    if(apiOk&&!matchIds.empty()){
        ps.kdRatio=(totalD>0)?(double)totalK/totalD:(totalK>0?(double)totalK:0.0);
    }else{
        ps.kdRatio=-2.0;
    }
    ps.loaded=true;
    {std::lock_guard<std::mutex>lk(g_mutex);g_statsCache[puuid]=ps;g_statsFetching.erase(puuid);}
    if(g_hWnd)PostMessage(g_hWnd,WM_APP+1,0,0);
}

// ═════════════════════════════════════════════════════════════════════════════
// GUI HELPERS
// ═════════════════════════════════════════════════════════════════════════════
std::wstring W(const std::string& s){
    if(s.empty())return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
HFONT MkFont(const char* name,int pt,int wt=FW_NORMAL){
    HDC dc=GetDC(NULL);int h=-MulDiv(pt,GetDeviceCaps(dc,LOGPIXELSY),72);ReleaseDC(NULL,dc);
    return CreateFontA(h,0,0,0,wt,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,name);
}
void GFill(HDC hdc,int x,int y,int w,int h,COLORREF c){
    HBRUSH br=CreateSolidBrush(c);RECT rc={x,y,x+w,y+h};FillRect(hdc,&rc,br);DeleteObject(br);
}
void GPanel(HDC hdc,int x,int y,int w,int h,int r,COLORREF fill,COLORREF bord=(COLORREF)-1){
    HBRUSH br=CreateSolidBrush(fill);
    HPEN pen=(bord==(COLORREF)-1)?(HPEN)GetStockObject(NULL_PEN):CreatePen(PS_SOLID,1,bord);
    HBRUSH ob=(HBRUSH)SelectObject(hdc,br);HPEN op=(HPEN)SelectObject(hdc,pen);
    RoundRect(hdc,x,y,x+w,y+h,r,r);
    SelectObject(hdc,ob);SelectObject(hdc,op);DeleteObject(br);if(bord!=(COLORREF)-1)DeleteObject(pen);
}
void GT(HDC hdc,const std::string& text,int x,int y,COLORREF c,HFONT f){
    auto wt=W(text);if(wt.empty())return;
    SelectObject(hdc,f);SetTextColor(hdc,c);SetBkMode(hdc,TRANSPARENT);
    TextOutW(hdc,x,y,wt.c_str(),(int)wt.size());
}
int TW(HDC hdc,const std::string& text,HFONT f){
    auto wt=W(text);if(wt.empty())return 0;
    SelectObject(hdc,f);SIZE sz;GetTextExtentPoint32W(hdc,wt.c_str(),(int)wt.size(),&sz);return sz.cx;
}

struct RInfo{std::string name;COLORREF color;};
RInfo getRank(int tier,int rr){
    if(tier<=2)return{"Unrated",C_UNRATED};
    static const struct{int lo,hi;const char*n;COLORREF c;}tbl[]={
        {3,5,"Iron",C_IRON},{6,8,"Bronze",C_BRONZE},{9,11,"Silver",C_SILVER},
        {12,14,"Gold",C_GOLD},{15,17,"Platinum",C_PLAT},{18,20,"Diamond",C_DIAM},
        {21,23,"Ascendant",C_ASCE},{24,26,"Immortal",C_IMMORT},{27,99,"Radiant",C_RADIANT}};
    for(auto&t:tbl)if(tier>=t.lo&&tier<=t.hi){
        std::string n=t.n;
        if(tier<27)n+=" "+std::to_string(tier-t.lo+1);
        if(rr>0&&tier<27)n+=" ("+std::to_string(rr)+" RR)";
        return{n,t.c};
    }
    return{"Unknown",C_UNRATED};
}

struct PGroup{int idx;COLORREF color;};
std::map<std::string,PGroup> buildPartyGroups(const std::vector<PlayerInfo>& players){
    std::map<std::string,int>cnt;
    for(auto&p:players)if(!p.partyId.empty())cnt[p.partyId]++;
    std::map<std::string,PGroup>pgm;int next=0;
    for(auto&p:players)
        if(!p.partyId.empty()&&cnt[p.partyId]>1&&!pgm.count(p.partyId)){
            pgm[p.partyId]={next+1,PARTY_CLRS[next%6]};next++;
        }
    return pgm;
}

// Map path → display name (case-insensitive, fallback to last path segment)
std::string resolveMapName(const std::string& mapId,const std::map<std::string,std::string>& mMap){
    if(mapId.empty())return "";
    if(mMap.count(mapId))return mMap.at(mapId);
    // Case-insensitive search
    std::string low=mapId;for(auto&c:low)c=tolower(c);
    for(auto&[k,v]:mMap){std::string kl=k;for(auto&c:kl)c=tolower(c);if(kl==low)return v;}
    // Substring match (e.g., /Game/Maps/Jam/Jam → "Jam" in displayName)
    for(auto&[k,v]:mMap){std::string kl=k;for(auto&c:kl)c=tolower(c);if(kl.find(low)!=std::string::npos||low.find(kl)!=std::string::npos)return v;}
    // Extract last path segment
    auto pos=mapId.rfind('/');
    return(pos!=std::string::npos&&pos+1<mapId.size())?mapId.substr(pos+1):mapId;
}

// Case-insensitive agent lookup by UUID
std::string resolveAgent(const std::string& charId,const std::map<std::string,std::string>& aMap){
    if(charId.empty())return "";
    if(aMap.count(charId))return aMap.at(charId);
    std::string low=charId;for(auto&c:low)c=tolower(c);
    for(auto&[k,v]:aMap){std::string kl=k;for(auto&c:kl)c=tolower(c);if(kl==low)return v;}
    return "";
}

// ═════════════════════════════════════════════════════════════════════════════
// PAINT FUNCTIONS
// ═════════════════════════════════════════════════════════════════════════════
void PaintTitleBar(HDC hdc,int w){
    GFill(hdc,0,0,w,TB_H,C_TBARBG);
    GT(hdc,"◈ ROLSTRAKER",PAD,10,C_TEXT,g_fTitle);
    int vx=PAD+TW(hdc,"◈ ROLSTRAKER",g_fTitle)+10;
    GT(hdc,APP_VERSION,vx,12,C_DIM,g_fSmall);
    int gx=vx+TW(hdc,APP_VERSION,g_fSmall)+8;
    GT(hdc,"GUI",gx,12,C_ACCENT,g_fSmall);

    // Update notification banner (shown center-right if update available)
    if(g_updateAvailable&&!g_latestVersion.empty()){
        std::string upd="↑ Доступно обновление "+g_latestVersion;
        int btnH=22,btnW=32,bY=(TB_H-btnH)/2;
        int closeX=w-8-btnW,minX=closeX-6-btnW;
        int updW=TW(hdc,upd,g_fSmall)+16;
        int updX=minX-12-updW,updY=bY;
        GPanel(hdc,updX,updY,updW,btnH,5,RGB(50,40,10),C_YELLOW);
        GT(hdc,upd,updX+8,updY+4,C_YELLOW,g_fSmall);
    }

    int btnH=22,btnW=32,bY=(TB_H-btnH)/2;
    int closeX=w-8-btnW,minX=closeX-6-btnW;
    GPanel(hdc,minX,bY,btnW,btnH,5,g_hMin?C_PANEL2:C_PANEL,C_BORDER);
    GT(hdc,"-",minX+11,bY+3,C_DIM,g_fNormal);
    GPanel(hdc,closeX,bY,btnW,btnH,5,g_hClose?C_ACCENT:C_PANEL,C_BORDER);
    GT(hdc,"x",closeX+11,bY+3,g_hClose?C_TEXT:C_DIM,g_fNormal);
    GFill(hdc,0,TB_H-1,w,1,C_BORDER);
}

void PaintStatusBar(HDC hdc,int y,int w,const std::string& phase,const std::string& mapName,const std::string& region,uint16_t port){
    GFill(hdc,0,y,w,SB_H,C_PANEL);
    int x=PAD,ty=y+SB_H/2-9;
    if(phase=="coregame"||phase=="pregame"){
        std::string ph=(phase=="coregame")?"● В ИГРЕ":"● ВЫБОР АГЕНТА";
        COLORREF pc=(phase=="coregame")?C_GREEN:C_YELLOW;
        GT(hdc,ph,x,ty,pc,g_fBold);x+=TW(hdc,ph,g_fBold)+22;
        if(!mapName.empty()){GT(hdc,"КАРТА",x,ty,C_DIM,g_fSmall);x+=TW(hdc,"КАРТА",g_fSmall)+6;GT(hdc,mapName,x,ty,C_TEXT,g_fBold);x+=TW(hdc,mapName,g_fBold)+22;}
    }else if(port!=0){
        GT(hdc,"Ожидание матча...",x,ty,C_YELLOW,g_fNormal);
    }else{
        GT(hdc,"Riot Client не найден. Запустите Valorant...",x,ty,C_ACCENT,g_fNormal);
    }
    GFill(hdc,0,y+SB_H-1,w,1,C_BORDER);
}

int PaintTeam(HDC hdc,int y,int w,const std::string& title,COLORREF titleClr,
    const std::vector<PlayerInfo>& players,
    const std::map<std::string,std::string>& agentMap,
    const std::map<std::string,PGroup>& pgm)
{
    if(players.empty())return y;
    // Team header
    GFill(hdc,0,y,w,THDR_H,C_PANEL2);
    GFill(hdc,0,y,4,THDR_H,titleClr);
    GT(hdc,title,PAD+10,y+8,titleClr,g_fBold);
    std::string cnt="("+std::to_string(players.size())+" игроков)";
    GT(hdc,cnt,PAD+10+TW(hdc,title,g_fBold)+10,y+9,C_DIM,g_fSmall);
    GFill(hdc,0,y+THDR_H-1,w,1,C_BORDER);
    y+=THDR_H;
    // Column headers
    GFill(hdc,0,y,w,CHDR_H,C_PANEL);
    GT(hdc,"ИГРОК",     CX_NAME, y+4,C_DIM,g_fSmall);
    GT(hdc,"АГЕНТ",     CX_AGENT,y+4,C_DIM,g_fSmall);
    GT(hdc,"РАНГ",      CX_RANK, y+4,C_DIM,g_fSmall);
    GT(hdc,"K/D*",      CX_KD,   y+4,C_DIM,g_fSmall);
    GT(hdc,"W/L*",      CX_WL,   y+4,C_DIM,g_fSmall);
    GT(hdc,"ГРУППА",    CX_PARTY,y+4,C_DIM,g_fSmall);
    GFill(hdc,0,y+CHDR_H-1,w,1,C_BORDER);
    y+=CHDR_H;

    for(int i=0;i<(int)players.size();i++){
        auto&p=players[i];
        GFill(hdc,0,y,w,ROW_H,(i%2==0)?C_BG:C_PANEL);
        int ry=y+(ROW_H-16)/2;

        // Party circle indicator
        PGroup pg;bool hasGroup=false;
        if(!p.partyId.empty()&&pgm.count(p.partyId)){pg=pgm.at(p.partyId);hasGroup=true;}
        COLORREF mClr=hasGroup?pg.color:C_BORDER;
        GPanel(hdc,CX_MARK+2,ry+1,14,14,7,mClr,mClr);

        // Name
        std::string nm=p.gameName+(p.tagLine.empty()?"":" #"+p.tagLine);
        if(nm.size()>26)nm=nm.substr(0,23)+"...";
        GT(hdc,nm,CX_NAME,ry,C_TEXT,g_fBold);

        // Agent (case-insensitive UUID match)
        std::string ag="—";
        if(!p.characterId.empty()){
            std::string found=resolveAgent(p.characterId,agentMap);
            if(!found.empty())ag=found;
            else if(!agentMap.empty())ag="?";   // map loaded but no match
            else ag="...";                        // map still loading
        }
        if(ag.size()>14)ag=ag.substr(0,11)+"...";
        GT(hdc,ag,CX_AGENT,ry,C_DIM,g_fNormal);

        // Rank
        auto ri=getRank(p.rankTier,p.rankRR);
        if(ri.name.size()>22)ri.name=ri.name.substr(0,19)+"...";
        GT(hdc,ri.name,CX_RANK,ry,ri.color,g_fNormal);

        // K/D  (last 5 matches)
        std::string kdStr;COLORREF kdClr=C_DIM;
        if(p.kdRatio<=-2.0){kdStr="N/A";kdClr=C_DIM;}
        else if(p.kdRatio<0){kdStr="...";kdClr=C_DIM;}
        else{
            std::ostringstream os;os<<std::fixed<<std::setprecision(2)<<p.kdRatio;kdStr=os.str();
            if(p.kdRatio>=1.5)kdClr=C_GREEN;
            else if(p.kdRatio>=1.0)kdClr=RGB(150,210,100);
            else if(p.kdRatio>=0.8)kdClr=C_YELLOW;
            else kdClr=C_RED;
        }
        GT(hdc,kdStr,CX_KD,ry,kdClr,g_fMono);

        // W/L  (last 5 matches)
        std::string wlStr;COLORREF wlClr=C_DIM;
        if(p.wins<=-2){wlStr="N/A";wlClr=C_DIM;}
        else if(p.wins<0){wlStr="...";wlClr=C_DIM;}
        else{
            wlStr=std::to_string(p.wins)+"W / "+std::to_string(p.losses)+"L";
            int total=p.wins+p.losses;int pct=(total>0)?((p.wins*100)/total):50;
            if(pct>=60)wlClr=C_GREEN;else if(pct>=50)wlClr=C_YELLOW;else wlClr=C_RED;
        }
        GT(hdc,wlStr,CX_WL,ry,wlClr,g_fMono);

        // Party badge
        if(hasGroup){
            std::string pgStr="Пати #"+std::to_string(pg.idx);
            int bw=TW(hdc,pgStr,g_fSmall)+12;
            GPanel(hdc,CX_PARTY,ry,bw,16,4,C_PANEL2,pg.color);
            GT(hdc,pgStr,CX_PARTY+6,ry,pg.color,g_fSmall);
        }else{
            GT(hdc,"Соло",CX_PARTY,ry,C_BORDER,g_fSmall);
        }
        GFill(hdc,0,y+ROW_H-1,w,1,C_BORDER);
        y+=ROW_H;
    }
    return y;
}

int PaintParties(HDC hdc,int y,int w,const std::vector<PlayerInfo>& players,const std::map<std::string,PGroup>& pgm){
    if(pgm.empty())return y;
    GFill(hdc,0,y,w,PSEC_H,C_PANEL2);
    GFill(hdc,0,y,4,PSEC_H,C_MAGENTA);
    GT(hdc,"PREMADE PARTIES",PAD+10,y+8,C_MAGENTA,g_fBold);
    GFill(hdc,0,y+PSEC_H-1,w,1,C_BORDER);
    y+=PSEC_H;

    std::map<int,std::vector<std::string>>groups;
    std::map<int,COLORREF>groupClrs;
    for(auto&p:players)
        if(!p.partyId.empty()&&pgm.count(p.partyId)){
            auto&pg=pgm.at(p.partyId);
            groups[pg.idx].push_back(p.gameName+(p.tagLine.empty()?"":" #"+p.tagLine));
            groupClrs[pg.idx]=pg.color;
        }
    for(auto&[idx,members]:groups){
        GFill(hdc,0,y,w,PROW_H,C_PANEL);
        GFill(hdc,0,y,4,PROW_H,groupClrs[idx]);
        int x=PAD+10,ry=y+(PROW_H-16)/2;
        std::string lbl="Пати #"+std::to_string(idx)+"  ("+std::to_string(members.size())+" чел.)   ";
        GT(hdc,lbl,x,ry,groupClrs[idx],g_fBold);x+=TW(hdc,lbl,g_fBold);
        for(int i=0;i<(int)members.size();i++){
            if(i>0){GT(hdc,",  ",x,ry,C_DIM,g_fNormal);x+=TW(hdc,",  ",g_fNormal);}
            GT(hdc,members[i],x,ry,C_TEXT,g_fNormal);x+=TW(hdc,members[i],g_fNormal);
        }
        GFill(hdc,0,y+PROW_H-1,w,1,C_BORDER);
        y+=PROW_H;
    }
    return y;
}

void PaintWaiting(HDC hdc,int y,int w,int h,const std::string& line1,const std::string& line2,COLORREF c){
    int cx=w/2;
    int by=y+(h-y)/2 - 30; // Center vertically in the remaining space

    ULONGLONG ticks = GetTickCount64();
    float phase = (sin(ticks * 0.003f) + 1.0f) * 0.5f; // 0.0 to 1.0 smooth breathing
    int r = GetRValue(c); int g = GetGValue(c); int b = GetBValue(c);
    int bgR = GetRValue(C_BG); int bgG = GetGValue(C_BG); int bgB = GetBValue(C_BG);
    int br = bgR + (int)((r - bgR) * (0.3f + 0.7f * phase));
    int bg = bgG + (int)((g - bgG) * (0.3f + 0.7f * phase));
    int bb = bgB + (int)((b - bgB) * (0.3f + 0.7f * phase));
    COLORREF animColor = RGB(br, bg, bb);

    // Animated dots for waiting text
    std::string animL1 = line1;
    if(line1 == "Ожидание матча") {
        int dotCount = (ticks / 500) % 4;
        animL1 += std::string(dotCount, '.');
    }

    GT(hdc,animL1,cx-TW(hdc,animL1,g_fBold)/2,by,animColor,g_fBold);
    if(!line2.empty())GT(hdc,line2,cx-TW(hdc,line2,g_fNormal)/2,by+28,C_DIM,g_fNormal);
}

void PaintAll(HDC hdc,int w,int h){
    GFill(hdc,0,0,w,h,C_BG);

    // Snapshot state under lock
    Lockfile lock;Session sess;MatchState state;
    std::map<std::string,std::string> aMap,mMap;
    {
        std::lock_guard<std::mutex>lk(g_mutex);
        lock=g_lock;sess=g_session;state=g_matchState;aMap=g_agentMap;mMap=g_mapNameMap;
        for(auto&p:state.players)
            if(g_statsCache.count(p.puuid)){auto&ps=g_statsCache[p.puuid];p.kdRatio=ps.kdRatio;p.wins=ps.wins;p.losses=ps.losses;}
    }

    // Content area: TB_H → h-FOOT_H, scrollable
    HRGN hClip=CreateRectRgn(0,TB_H,w,h-FOOT_H);
    SelectClipRgn(hdc,hClip);DeleteObject(hClip);

    std::string mapName=resolveMapName(state.mapId,mMap);

    int y=TB_H-g_scrollY;
    PaintStatusBar(hdc,y,w,state.phase,mapName,sess.region,lock.port);
    y+=SB_H;

    if(lock.port==0){
        PaintWaiting(hdc,y,w,h,"Riot Client не запущен","Запустите Valorant или Riot Client...",C_ACCENT);
        g_totalContentH=SB_H+200;
    }else if(state.phase=="none"){
        PaintWaiting(hdc,y,w,h,"Ожидание матча","Зайдите в матч или лобби выбора агентов.",C_YELLOW);
        g_totalContentH=SB_H+200;
    }else{
        std::vector<PlayerInfo>t1,t2;
        for(auto&p:state.players){
            // Handle all known team ID variants
            std::string tid=p.teamId;
            if(tid=="Red"||tid=="Defender"||tid=="TeamOne"||tid=="Order")t1.push_back(p);
            else if(tid=="Blue"||tid=="Attacker"||tid=="TeamTwo"||tid=="Chaos")t2.push_back(p);
            else{if(t1.size()<=t2.size())t1.push_back(p);else t2.push_back(p);}
        }
        // If all players ended up in one team, split evenly
        if(t1.empty()&&!t2.empty()){
            size_t half=t2.size()/2;
            t1=std::vector<PlayerInfo>(t2.begin(),t2.begin()+half);
            t2=std::vector<PlayerInfo>(t2.begin()+half,t2.end());
        }else if(t2.empty()&&!t1.empty()){
            size_t half=t1.size()/2;
            t2=std::vector<PlayerInfo>(t1.begin()+half,t1.end());
            t1=std::vector<PlayerInfo>(t1.begin(),t1.begin()+half);
        }
        auto pgm=buildPartyGroups(state.players);
        y=PaintTeam(hdc,y,w,"ЗАЩИТНИКИ",C_ACCENT,t1,aMap,pgm);
        y+=8;
        y=PaintTeam(hdc,y,w,"НАПАДАЮЩИЕ",C_BLUE,t2,aMap,pgm);
        y+=8;
        y=PaintParties(hdc,y,w,state.players,pgm);
        g_totalContentH=(y+g_scrollY)-TB_H;
    }

    SelectClipRgn(hdc,NULL);
    // Fixed overlays
    PaintTitleBar(hdc,w);
}

// ═════════════════════════════════════════════════════════════════════════════
// WINDOW PROCEDURE
// ═════════════════════════════════════════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:
        g_fTitle  = MkFont("Segoe UI",13,FW_BOLD);
        g_fBold   = MkFont("Segoe UI",10,FW_BOLD);
        g_fNormal = MkFont("Segoe UI",10,FW_NORMAL);
        g_fSmall  = MkFont("Segoe UI", 9,FW_NORMAL);
        g_fMono   = MkFont("Consolas",10,FW_NORMAL);
        SetTimer(hwnd, 1, 33, NULL); // 30 FPS animation timer
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_running=false;
        for(auto f:{g_fTitle,g_fBold,g_fNormal,g_fSmall,g_fMono})if(f)DeleteObject(f);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:{
        PAINTSTRUCT ps;HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc;GetClientRect(hwnd,&rc);int cw=rc.right,ch=rc.bottom;
        HDC hMem=CreateCompatibleDC(hdc);
        HBITMAP hBmp=CreateCompatibleBitmap(hdc,cw,ch);
        HBITMAP hOld=(HBITMAP)SelectObject(hMem,hBmp);
        PaintAll(hMem,cw,ch);
        BitBlt(hdc,0,0,cw,ch,hMem,0,0,SRCCOPY);
        SelectObject(hMem,hOld);DeleteObject(hBmp);DeleteDC(hMem);
        EndPaint(hwnd,&ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;

    case WM_NCHITTEST:{
        POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&pt);
        RECT rc;GetClientRect(hwnd,&rc);
        int btnH=22,btnW=32,bY=(TB_H-btnH)/2;
        int closeX=rc.right-8-btnW,minX=closeX-6-btnW;
        if(pt.y>=bY&&pt.y<=bY+btnH&&(pt.x>=minX||pt.x>=closeX))return HTCLIENT;
        if(pt.y>=0&&pt.y<TB_H)return HTCAPTION;
        return HTCLIENT;
    }

    case WM_LBUTTONDOWN:{
        POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        RECT rc;GetClientRect(hwnd,&rc);
        int btnH=22,btnW=32,bY=(TB_H-btnH)/2;
        int closeX=rc.right-8-btnW,minX=closeX-6-btnW;
        if(pt.y>=bY&&pt.y<=bY+btnH&&pt.x>=closeX&&pt.x<=closeX+btnW){DestroyWindow(hwnd);return 0;}
        if(pt.y>=bY&&pt.y<=bY+btnH&&pt.x>=minX&&pt.x<=minX+btnW){ShowWindow(hwnd,SW_MINIMIZE);return 0;}
        return 0;
    }

    case WM_MOUSEMOVE:{
        POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        RECT rc;GetClientRect(hwnd,&rc);
        int btnH=22,btnW=32,bY=(TB_H-btnH)/2;
        int closeX=rc.right-8-btnW,minX=closeX-6-btnW;
        bool hc=(pt.y>=bY&&pt.y<=bY+btnH&&pt.x>=closeX&&pt.x<=closeX+btnW);
        bool hm=(pt.y>=bY&&pt.y<=bY+btnH&&pt.x>=minX&&pt.x<=minX+btnW);
        if(hc!=g_hClose||hm!=g_hMin){g_hClose=hc;g_hMin=hm;InvalidateRect(hwnd,NULL,FALSE);}
        return 0;
    }

    case WM_MOUSEWHEEL:{
        int delta=GET_WHEEL_DELTA_WPARAM(wp);
        g_scrollY-=delta/3;
        RECT rc;GetClientRect(hwnd,&rc);
        int viewH=rc.bottom-TB_H-FOOT_H;
        int maxS=std::max(0,g_totalContentH-viewH);
        g_scrollY=std::max(0,std::min(g_scrollY,maxS));
        InvalidateRect(hwnd,NULL,FALSE);
        return 0;
    }

    case WM_KEYDOWN:
        if(wp=='R'){
            {std::lock_guard<std::mutex>lk(g_mutex);g_rankCache.clear();g_statsCache.clear();g_statsFetching.clear();g_nameCache.clear();}
            return 0;
        }
        if(wp==VK_ESCAPE){DestroyWindow(hwnd);return 0;}
        return DefWindowProcW(hwnd,msg,wp,lp);

    case WM_TIMER:
        if(wp==1){
            bool shouldAnimate = false;
            {std::lock_guard<std::mutex>lk(g_mutex); if(g_lock.port==0 || g_matchState.phase=="none") shouldAnimate=true;}
            if(shouldAnimate) InvalidateRect(hwnd,NULL,FALSE);
            return 0;
        }
        break;

    case WM_APP+1: // data update from background thread
        InvalidateRect(hwnd,NULL,FALSE);
        return 0;

    default:
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// BACKGROUND UPDATE THREAD
// ═════════════════════════════════════════════════════════════════════════════
void UpdateThread(){
    // Check for new version from GitHub in background (non-blocking)
    std::thread(checkForUpdate).detach();
    loadAgentsAndMaps();
    auto lastCheck=std::chrono::steady_clock::now()-std::chrono::seconds(10);

    while(g_running){
        auto now=std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::seconds>(now-lastCheck).count()<3){
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        lastCheck=now;

        Lockfile lock=readLockfile();
        {std::lock_guard<std::mutex>lk(g_mutex);g_lock=lock;}

        if(lock.port!=0){
            Session sess=getSession(lock);
            if(!sess.accessToken.empty()){
                MatchState state=getLiveMatch(sess,lock);
                if(state.phase!="none"){
                    auto pmap=getPresences(lock);
                    for(auto&p:state.players)
                        if(p.partyId.empty()&&pmap.count(p.puuid))p.partyId=pmap[p.puuid];
                    resolveNamesAndRanks(sess,state.players);
                    // Launch per-player stats threads for new players
                    for(auto&p:state.players){
                        std::lock_guard<std::mutex>lk(g_mutex);
                        if(!g_statsCache.count(p.puuid)&&!g_statsFetching.count(p.puuid)){
                            g_statsFetching.insert(p.puuid);
                            std::thread(fetchPlayerStats,sess,p.puuid).detach();
                        }
                    }
                }
                {std::lock_guard<std::mutex>lk(g_mutex);g_session=sess;g_matchState=state;}
            }else{
                std::lock_guard<std::mutex>lk(g_mutex);
                g_matchState=MatchState();
            }
        }else{
            std::lock_guard<std::mutex>lk(g_mutex);
            g_matchState=MatchState();
        }

        if(g_hWnd)PostMessage(g_hWnd,WM_APP+1,0,0);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// WINMAIN
// ═════════════════════════════════════════════════════════════════════════════
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int nShow){
    WNDCLASSEXW wc={};
    wc.cbSize      = sizeof(wc);
    wc.style       = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInst;
    wc.hCursor     = LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"RolsTrakerGUI";
    wc.hIcon       = LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassExW(&wc);

    int sx=GetSystemMetrics(SM_CXSCREEN),sy=GetSystemMetrics(SM_CYSCREEN);
    int wx=(sx-WIN_W)/2,wy=(sy-WIN_H)/2;

    g_hWnd=CreateWindowExW(WS_EX_APPWINDOW,L"RolsTrakerGUI",L"RolsTraker v2.0",
        WS_POPUP|WS_VISIBLE,wx,wy,WIN_W,WIN_H,NULL,NULL,hInst,NULL);
    if(!g_hWnd)return 1;

    ShowWindow(g_hWnd,nShow);
    UpdateWindow(g_hWnd);

    std::thread(UpdateThread).detach();

    MSG msg={};
    while(GetMessage(&msg,NULL,0,0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_running=false;
    Sleep(500);
    return (int)msg.wParam;
}
