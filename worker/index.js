let memoryCache = {};

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    if (request.method === "POST" && url.pathname === "/api/update") {
      let t = url.searchParams.get("t");
      if(!t) return new Response("Missing token", {status: 400});
      const data = await request.text();
      memoryCache[t] = { data: data, time: Date.now() };
      await env.KV.put("matchState_" + t, data, { expirationTtl: 3600 });
      return new Response(JSON.stringify({success: true}), { headers: { "Content-Type": "application/json" }});
    }

    if (request.method === "GET" && url.pathname === "/api/game") {
      let t = url.searchParams.get("t");
      if(!t) return new Response("{\"phase\":\"none\",\"players\":[]}", { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" }});
      
      // Serve from memory if fresh (< 60 seconds) to bypass KV propagation delays
      if (memoryCache[t] && (Date.now() - memoryCache[t].time < 60000)) {
          return new Response(memoryCache[t].data, { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" }});
      }
      
      const data = await env.KV.get("matchState_" + t);
      if (data) {
          memoryCache[t] = { data: data, time: Date.now() }; // Backfill memory
      }
      return new Response(data || "{\"phase\":\"none\",\"players\":[]}", { headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" }});
    }

    if (request.method === "GET" && url.pathname === "/") {
      let t = url.searchParams.get("t") || "";
      if(!t) return new Response("Токен не указан. Откройте ссылку из программы RolsTraker.", { headers: { "Content-Type": "text/html;charset=UTF-8" }});

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
    if(p === "coregame") return "<span class=\\"green\\">В ИГРЕ (Core Game)</span>";
    if(p === "pregame") return "<span class=\\"cyan\\">ВЫБОР АГЕНТА (Agent Select)</span>";
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
    document.getElementById("m-name").innerText = "Подробная статистика — " + p.name;
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
            <div><span style="color:#aaa;">Любимый Агент:</span> \${escapeHtml(p.favAgent) || 'N/A'}</div>
            <div><span style="color:#aaa;">K/D:</span> \${p.kd >= 0 ? p.kd.toFixed(2) : 'N/A'}</div>
            <div><span style="color:#aaa;">Win %:</span> <span class="\${p.wins > p.losses ? 'won' : 'lost'}">\${winPct}</span></div>
            <div><span style="color:#aaa;">ACS:</span> \${acs}</div>
            <div><span style="color:#aaa;">Сыграно (акт):</span> \${totalMatchesRanked}</div>
        </div>
        <div class="stats-col" style="border-left: 1px solid #333; padding-left: 10px;">
            <div><span style="color:#aaa;">Точность стрельбы:</span></div>
            <div><span style="color:#ff4655;">В голову (HS):</span> \${hs}%</div>
            <div><span style="color:#d4d4d4;">В тело (BS):</span> \${bs}%</div>
            <div><span style="color:#888;">В ноги (LS):</span> \${ls}%</div>
        </div>
        <div class="stats-col" style="border-left: 1px solid #333; padding-left: 10px;">
            <div><span style="color:#aaa;">Ранги:</span></div>
            <div><span style="color:#aaa;">Текущий:</span> <span class="cyan">\${escapeHtml(p.rank)}</span></div>
            <div><span style="color:#aaa;">Макс:</span> \${peak}</div>
            <div style="margin-top: 10px;"><span style="color:#aaa;">W/L:</span> <span class="won">\${p.wins||0}W</span> / <span class="lost">\${p.losses||0}L</span></div>
        </div>
    \`;
    document.getElementById("m-stats").innerHTML = statsHtml;
    
    let histHtml = "<tr><th>Режим</th><th>Агент</th><th>K/D/A</th><th>Счет</th></tr>";
    if(p.recentMatches && p.recentMatches.length > 0) {
        for(let m of p.recentMatches) {
            let res = m.won ? "<span class='won'>ПОБЕДА</span>" : "<span class='lost'>ПОРАЖЕНИЕ</span>";
            let q = m.queue || "";
            if (q === "competitive") q = "РЕЙТИНГ";
            else if (q === "unrated") q = "БЕЗ РАНГА";
            else if (q === "deathmatch") q = "ДМ";
            else if (q === "ggteam") q = "ЭСКАЛАЦИЯ";
            else if (q === "swiftplay") q = "БЫСТРАЯ";
            histHtml += "<tr>";
            histHtml += "<td>" + escapeHtml(q) + "</td>";
            histHtml += "<td>" + escapeHtml(m.agent) + "</td>";
            histHtml += "<td>" + m.k + " / " + m.d + " / " + m.a + "</td>";
            histHtml += "<td>" + res + " (" + m.rw + " - " + m.rl + ")</td>";
            histHtml += "</tr>";
        }
    } else {
        histHtml += "<tr><td colspan='4'>Нет недавних матчей (или загружается)</td></tr>";
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
            content.innerHTML="<div class=\\"none-box\\">Режим: Вне матча (none) | Карта: N/A | Сервер: N/A</div>"; 
            return; 
        }
        
        let mapName = escapeHtml(data.map || "N/A");
        let server = escapeHtml(data.server || "N/A");
        
        let headerHtml = "<div class=\\"header\\"><h1>ROLSTRAKER (Live)</h1>";
        headerHtml += "<div class=\\"meta\\">Режим: " + parsePhase(data.phase) + " | Карта: <span class=\\"cyan\\">" + mapName + "</span> | Сервер: <span class=\\"cyan\\">" + server + "</span></div></div>";
        
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
            html += "<table><tr><th class=\\"party\\">Пати</th><th>Игрок (Ник#Тег)</th><th>Агент</th><th>Ранг</th><th>K/D</th><th>W/L</th></tr>";
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
            bodyHtml += renderTeam(redPlayers, "[КОМАНДА 1 / ЗАЩИТНИКИ (RED)]", true);
            bodyHtml += renderTeam(bluePlayers, "[КОМАНДА 2 / АТАКУЮЩИЕ (BLUE)]", false);
        } else {
            bodyHtml += renderTeam(bluePlayers, "[ИГРОКИ]", false);
        }
        bodyHtml += "</div>";
        
        content.innerHTML = headerHtml + bodyHtml;
    }catch(e){
        console.error(e);
    }
}
setInterval(update, 2000); window.onload=update;
</script></head><body>
<div class="container" id="content"><div class="none-box">Загрузка...</div></div>
<div class="modal-overlay" id="modal-overlay" onclick="if(event.target===this)closeModal()">
  <div class="modal">
    <div class="modal-close" onclick="closeModal()">X</div>
    <h2 id="m-name">Player Name</h2>
    <div class="stats-grid" id="m-stats"></div>
    <div style="font-size: 13px; color: #aaa; margin-bottom: 5px;">Последние матчи (Нажми на строку чтобы открыть):</div>
    <table id="m-hist"></table>
  </div>
</div>
</body></html>`;
      return new Response(html, { headers: { "Content-Type": "text/html;charset=UTF-8" }});
    }

    return new Response("Not found", { status: 404 });
  }
};
