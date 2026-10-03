// index.js
var memoryCache = {};
var MAX_CACHE_SIZE = 100;
var index_default = {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    if (request.method === "POST" && url.pathname === "/api/update") {
      let t = url.searchParams.get("t");
      if (!t) return new Response("Missing token", { status: 400 });
      const data = await request.text();
      memoryCache[t] = { data, time: Date.now() };
      const keys = Object.keys(memoryCache);
      if (keys.length > MAX_CACHE_SIZE) {
        keys.sort((a, b) => memoryCache[a].time - memoryCache[b].time);
        for (let i = 0; i < keys.length - MAX_CACHE_SIZE; i++) {
          delete memoryCache[keys[i]];
        }
      }
      await env.KV.put("matchState_" + t, data, { expirationTtl: 3600 });
      return new Response(JSON.stringify({ success: true }), { headers: { "Content-Type": "application/json" } });
    }
    if (request.method === "GET" && url.pathname === "/api/game") {
      let t = url.searchParams.get("t");
      if (!t) return new Response('{"phase":"none","players":[]}', { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" } });
      if (memoryCache[t] && Date.now() - memoryCache[t].time < 6e4) {
        return new Response(memoryCache[t].data, { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" } });
      }
      const data = await env.KV.get("matchState_" + t);
      if (data) {
        memoryCache[t] = { data, time: Date.now() };
      }
      return new Response(data || '{"phase":"none","players":[]}', { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" } });
    }
    if (request.method === "GET" && url.pathname === "/") {
      let t = url.searchParams.get("t") || "";
      if (!t) return new Response("\u0422\u043E\u043A\u0435\u043D \u043D\u0435 \u0443\u043A\u0430\u0437\u0430\u043D. \u041E\u0442\u043A\u0440\u043E\u0439\u0442\u0435 \u0441\u0441\u044B\u043B\u043A\u0443 \u0438\u0437 \u043F\u0440\u043E\u0433\u0440\u0430\u043C\u043C\u044B RolsTraker.", { headers: { "Content-Type": "text/html;charset=UTF-8" } });
      const html = `<!DOCTYPE html><html><head><meta charset="UTF-8"><title>RolsTraker Live</title>
<style>
  body { background: #0c0c0c; color: #d4d4d4; font-family: "Consolas", "Courier New", monospace; text-align: center; margin: 0; padding: 20px; }
  .container { max-width: 1100px; margin: 0 auto; border: 1px solid #d4d4d4; padding: 10px; border-radius: 4px; box-shadow: 0 0 10px rgba(0,0,0,0.5); }
  .header { border-bottom: 1px solid #d4d4d4; padding-bottom: 10px; margin-bottom: 10px; }
  .header h1 { margin: 0 0 10px 0; font-size: 18px; color: #00e5ff; font-weight: normal; }
  .meta { font-size: 14px; margin-bottom: 10px; }
  .meta span.green { color: #00ff00; }
  .meta span.cyan { color: #00e5ff; }
  .teams-container { display: flex; justify-content: space-between; text-align: left; }
  .team { width: 49%; }
  .team-red { border-right: 1px solid #d4d4d4; padding-right: 10px; }
  .team-blue { padding-left: 10px; }
  .team-title { font-size: 14px; margin-bottom: 5px; }
  .team-title.red { color: #ff4655; }
  .team-title.blue { color: #00e5ff; }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th { text-align: left; border-bottom: 1px double #d4d4d4; padding: 4px 2px; font-weight: normal; }
  td { padding: 4px 2px; border-bottom: 1px dotted #333; cursor: pointer; }
  tr:hover td { background-color: #222; }
  .party { width: 40px; }
  .party-0 { color: #00ff00; } .party-1 { color: #00e5ff; } .party-2 { color: #ffff00; }
  .party-3 { color: #ff00ff; } .party-4 { color: #ff0000; } .party-5 { color: #87cefa; }
  .none-box { padding: 50px; font-size: 18px; color: #888; }
  
  /* Modal */
  .modal-overlay { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.8); z-index: 1000; }
  .modal { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); background: #1a1a1a; border: 1px solid #d4d4d4; padding: 20px; width: 700px; max-width: 90%; text-align: left; box-shadow: 0 0 20px #000; border-radius: 5px; }
  .modal h2 { margin: 0 0 10px 0; color: #00e5ff; font-size: 18px; border-bottom: 1px solid #333; padding-bottom: 5px; }
  .modal-close { position: absolute; top: 10px; right: 10px; cursor: pointer; color: #ff4655; font-weight: bold; font-size: 18px; }
  .stats-grid { display: flex; justify-content: space-between; margin-bottom: 15px; font-size: 14px; border-bottom: 1px solid #333; padding-bottom: 10px; }
  .stats-col { flex: 1; }
  .stats-col div { margin-bottom: 4px; }
  .modal table { margin-top: 10px; }
  .modal table th { border-bottom: 1px solid #666; }
  .won { color: #00ff00; } .lost { color: #ff4655; }
</style>
<script>
const symbols = ["[o]", "[*]", "[x]", "[+]", "[-]"];
const partyColors = ["party-0", "party-1", "party-2", "party-3", "party-4", "party-5"];
const token = "${t}";
let lastData = null;

function escapeHtml(str) {
    if (str === null || str === undefined) return "";
    return String(str)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

function parsePhase(p) {
    if(p === "coregame") return "<span class=\\"green\\">\u0412 \u0418\u0413\u0420\u0415 (Core Game)</span>";
    if(p === "pregame") return "<span class=\\"cyan\\">\u0412\u042B\u0411\u041E\u0420 \u0410\u0413\u0415\u041D\u0422\u0410 (Agent Select)</span>";
    return p;
}

function formatRank(tier) {
    const ranks = ["Unrated","Unused1","Unused2","Iron 1","Iron 2","Iron 3","Bronze 1","Bronze 2","Bronze 3","Silver 1","Silver 2","Silver 3","Gold 1","Gold 2","Gold 3","Platinum 1","Platinum 2","Platinum 3","Diamond 1","Diamond 2","Diamond 3","Ascendant 1","Ascendant 2","Ascendant 3","Immortal 1","Immortal 2","Immortal 3","Radiant"];
    if(tier >= 0 && tier < ranks.length) return ranks[tier];
    return "Unknown";
}

function openModal(playerIndex) {
    if(!lastData || !lastData.players || !lastData.players[playerIndex]) return;
    let p = lastData.players[playerIndex];
    document.getElementById("m-name").innerText = "\u041F\u043E\u0434\u0440\u043E\u0431\u043D\u0430\u044F \u0441\u0442\u0430\u0442\u0438\u0441\u0442\u0438\u043A\u0430 \u2014 " + p.name;
    let hs = 0, bs = 0, ls = 0, acs = 0;
    let totalShots = (p.hs||0) + (p.bs||0) + (p.ls||0);
    if(totalShots > 0) {
        hs = ((p.hs * 100) / totalShots).toFixed(1);
        bs = ((p.bs * 100) / totalShots).toFixed(1);
        ls = ((p.ls * 100) / totalShots).toFixed(1);
    }
    if(p.totalScore !== undefined && p.totalRounds !== undefined && p.totalRounds > 0) {
        acs = Math.round(p.totalScore / p.totalRounds);
    }
    
    let winPct = "0.0%";
    let totalMatchesRanked = (p.wins || 0) + (p.losses || 0);
    if (totalMatchesRanked > 0) winPct = ((p.wins * 100) / totalMatchesRanked).toFixed(1) + "%";
    
    let peak = p.peakRank ? formatRank(p.peakRank) : "Unknown";

    let statsHtml = \`
        <div class="stats-col">
            <div><span style="color:#aaa;">\u041B\u044E\u0431\u0438\u043C\u044B\u0439 \u0410\u0433\u0435\u043D\u0442:</span> \${escapeHtml(p.favAgent) || 'N/A'}</div>
            <div><span style="color:#aaa;">K/D:</span> \${p.kd >= 0 ? p.kd.toFixed(2) : 'N/A'}</div>
            <div><span style="color:#aaa;">Win %:</span> <span class="\${p.wins > p.losses ? 'won' : 'lost'}">\${winPct}</span></div>
            <div><span style="color:#aaa;">ACS:</span> \${acs}</div>
            <div><span style="color:#aaa;">\u0421\u044B\u0433\u0440\u0430\u043D\u043E (\u0430\u043A\u0442):</span> \${totalMatchesRanked}</div>
        </div>
        <div class="stats-col" style="border-left: 1px solid #333; padding-left: 10px;">
            <div><span style="color:#aaa;">\u0422\u043E\u0447\u043D\u043E\u0441\u0442\u044C \u0441\u0442\u0440\u0435\u043B\u044C\u0431\u044B:</span></div>
            <div><span style="color:#ff4655;">\u0412 \u0433\u043E\u043B\u043E\u0432\u0443 (HS):</span> \${hs}%</div>
            <div><span style="color:#d4d4d4;">\u0412 \u0442\u0435\u043B\u043E (BS):</span> \${bs}%</div>
            <div><span style="color:#888;">\u0412 \u043D\u043E\u0433\u0438 (LS):</span> \${ls}%</div>
        </div>
        <div class="stats-col" style="border-left: 1px solid #333; padding-left: 10px;">
            <div><span style="color:#aaa;">\u0420\u0430\u043D\u0433\u0438:</span></div>
            <div><span style="color:#aaa;">\u0422\u0435\u043A\u0443\u0449\u0438\u0439:</span> <span class="cyan">\${escapeHtml(p.rank)}</span></div>
            <div><span style="color:#aaa;">\u041C\u0430\u043A\u0441:</span> \${peak}</div>
            <div style="margin-top: 10px;"><span style="color:#aaa;">W/L:</span> <span class="won">\${p.wins||0}W</span> / <span class="lost">\${p.losses||0}L</span></div>
        </div>
    \`;
    document.getElementById("m-stats").innerHTML = statsHtml;
    
    let histHtml = "<tr><th>\u0420\u0435\u0436\u0438\u043C</th><th>\u0410\u0433\u0435\u043D\u0442</th><th>K/D/A</th><th>\u0421\u0447\u0435\u0442</th></tr>";
    if(p.recentMatches && p.recentMatches.length > 0) {
        for(let m of p.recentMatches) {
            let res = m.won ? "<span class='won'>\u041F\u041E\u0411\u0415\u0414\u0410</span>" : "<span class='lost'>\u041F\u041E\u0420\u0410\u0416\u0415\u041D\u0418\u0415</span>";
            let q = m.queue || "";
            if (q === "competitive") q = "\u0420\u0415\u0419\u0422\u0418\u041D\u0413";
            else if (q === "unrated") q = "\u0411\u0415\u0417 \u0420\u0410\u041D\u0413\u0410";
            else if (q === "deathmatch") q = "\u0414\u041C";
            else if (q === "ggteam") q = "\u042D\u0421\u041A\u0410\u041B\u0410\u0426\u0418\u042F";
            else if (q === "swiftplay") q = "\u0411\u042B\u0421\u0422\u0420\u0410\u042F";
            histHtml += "<tr>";
            histHtml += "<td>" + escapeHtml(q) + "</td>";
            histHtml += "<td>" + escapeHtml(m.agent) + "</td>";
            histHtml += "<td>" + m.k + " / " + m.d + " / " + m.a + "</td>";
            histHtml += "<td>" + res + " (" + m.rw + " - " + m.rl + ")</td>";
            histHtml += "</tr>";
        }
    } else {
        histHtml += "<tr><td colspan='4'>\u041D\u0435\u0442 \u043D\u0435\u0434\u0430\u0432\u043D\u0438\u0445 \u043C\u0430\u0442\u0447\u0435\u0439 (\u0438\u043B\u0438 \u0437\u0430\u0433\u0440\u0443\u0436\u0430\u0435\u0442\u0441\u044F)</td></tr>";
    }
    document.getElementById("m-hist").innerHTML = histHtml;
    document.getElementById("modal-overlay").style.display = "block";
}
window.closeModal = function() { document.getElementById("modal-overlay").style.display = "none"; }

async function update(){
    if (document.getElementById("modal-overlay").style.display === "block") return; // do not refresh while modal open
    try{
        let res = await fetch("/api/game?t=" + token); let data = await res.json();
        lastData = data;
        let content = document.getElementById("content");
        if(!data || data.phase === "none" || !data.players || data.players.length === 0){ 
            content.innerHTML="<div class=\\"none-box\\">\u0420\u0435\u0436\u0438\u043C: \u0412\u043D\u0435 \u043C\u0430\u0442\u0447\u0430 (none) | \u041A\u0430\u0440\u0442\u0430: N/A | \u0421\u0435\u0440\u0432\u0435\u0440: N/A</div>"; 
            return; 
        }
        
        let mapName = escapeHtml(data.map || "N/A");
        let server = escapeHtml(data.server || "N/A");
        
        let headerHtml = "<div class=\\"header\\"><h1>ROLSTRAKER (Live)</h1>";
        headerHtml += "<div class=\\"meta\\">\u0420\u0435\u0436\u0438\u043C: " + parsePhase(data.phase) + " | \u041A\u0430\u0440\u0442\u0430: <span class=\\"cyan\\">" + mapName + "</span> | \u0421\u0435\u0440\u0432\u0435\u0440: <span class=\\"cyan\\">" + server + "</span></div></div>";
        
        // Calculate party indices
        let partyMap = {};
        let nextPartyIdx = 0;
        data.players.forEach(p => {
            if(p.partyId && p.partyId !== "00000000-0000-0000-0000-000000000000" && p.partyId !== "") {
                if(partyMap[p.partyId] === undefined) { partyMap[p.partyId] = { count: 1, idx: -1 }; }
                else {
                    if(partyMap[p.partyId].idx === -1) { partyMap[p.partyId].idx = nextPartyIdx++; }
                    partyMap[p.partyId].count++;
                }
            }
        });

        function renderTeam(players, teamName, isRed) {
            let html = "<div class=\\"team " + (isRed ? "team-red" : "team-blue") + "\\">";
            if(teamName) html += "<div class=\\"team-title " + (isRed ? "red" : "blue") + "\\">" + teamName + "</div>";
            html += "<table><tr><th class=\\"party\\">\u041F\u0430\u0442\u0438</th><th>\u0418\u0433\u0440\u043E\u043A (\u041D\u0438\u043A#\u0422\u0435\u0433)</th><th>\u0410\u0433\u0435\u043D\u0442</th><th>\u0420\u0430\u043D\u0433</th><th>K/D</th><th>W/L</th></tr>";
            for(let p of players) {
                let partyStr = "";
                if(p.partyId && partyMap[p.partyId] && partyMap[p.partyId].idx !== -1) {
                    let pIdx = partyMap[p.partyId].idx;
                    let sym = symbols[pIdx % symbols.length];
                    let col = partyColors[pIdx % partyColors.length];
                    partyStr = "<span class=\\"" + col + "\\">" + sym + "</span>";
                }
                
                let kd = p.kd >= 0 ? p.kd.toFixed(2) : "N/A";
                let wl = (p.wins !== undefined && p.losses !== undefined) ? (p.wins + "/" + p.losses) : "N/A";
                
                html += "<tr onclick='openModal(" + data.players.indexOf(p) + ")'><td class=\\"party\\">" + partyStr + "</td>";
                html += "<td>" + escapeHtml(p.name) + "</td>";
                html += "<td>" + escapeHtml(p.agent) + "</td>";
                html += "<td>" + escapeHtml(p.rank) + "</td>";
                html += "<td>" + kd + "</td>";
                html += "<td>" + wl + "</td></tr>";
            }
            html += "</table></div>";
            return html;
        }

        let redPlayers = data.players.filter(p => p.team === "Red" || p.team === "Defender");
        let bluePlayers = data.players.filter(p => p.team === "Blue" || p.team === "Attacker");
        if(redPlayers.length === 0 && bluePlayers.length === 0) {
            bluePlayers = data.players;
        }

        let bodyHtml = "<div class=\\"teams-container\\">";
        if (redPlayers.length > 0) {
            bodyHtml += renderTeam(redPlayers, "[\u041A\u041E\u041C\u0410\u041D\u0414\u0410 1 / \u0417\u0410\u0429\u0418\u0422\u041D\u0418\u041A\u0418 (RED)]", true);
            bodyHtml += renderTeam(bluePlayers, "[\u041A\u041E\u041C\u0410\u041D\u0414\u0410 2 / \u0410\u0422\u0410\u041A\u0423\u042E\u0429\u0418\u0415 (BLUE)]", false);
        } else {
            bodyHtml += renderTeam(bluePlayers, "[\u0418\u0413\u0420\u041E\u041A\u0418]", false);
        }
        bodyHtml += "</div>";
        
        content.innerHTML = headerHtml + bodyHtml;
    }catch(e){
        console.error(e);
    }
}
setInterval(update, 2000); window.onload=update;
<\/script></head><body>
<div class="container" id="content"><div class="none-box">\u0417\u0430\u0433\u0440\u0443\u0437\u043A\u0430...</div></div>
<div class="modal-overlay" id="modal-overlay" onclick="if(event.target===this)closeModal()">
  <div class="modal">
    <div class="modal-close" onclick="closeModal()">X</div>
    <h2 id="m-name">Player Name</h2>
    <div class="stats-grid" id="m-stats"></div>
    <div style="font-size: 13px; color: #aaa; margin-bottom: 5px;">\u041F\u043E\u0441\u043B\u0435\u0434\u043D\u0438\u0435 \u043C\u0430\u0442\u0447\u0438 (\u041D\u0430\u0436\u043C\u0438 \u043D\u0430 \u0441\u0442\u0440\u043E\u043A\u0443 \u0447\u0442\u043E\u0431\u044B \u043E\u0442\u043A\u0440\u044B\u0442\u044C):</div>
    <table id="m-hist"></table>
  </div>
</div>
</body></html>`;
      return new Response(html, { headers: { "Content-Type": "text/html;charset=UTF-8" } });
    }
    return new Response("Not found", { status: 404 });
  }
};
export {
  index_default as default
};
//# sourceMappingURL=index.js.map
