import os, base64, urllib.request, ssl, json

lockfile_path = os.path.expandvars(r'%LOCALAPPDATA%\Riot Games\Riot Games\Config\lockfile')
with open(lockfile_path, 'r') as f:
    parts = f.read().strip().split(':')
port, pw = parts[2], parts[3]
auth = base64.b64encode(f'riot:{pw}'.encode()).decode()
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

req = urllib.request.Request(f'https://127.0.0.1:{port}/entitlements/v1/token', headers={'Authorization': f'Basic {auth}'})
with urllib.request.urlopen(req, context=ctx) as resp:
    tdata = json.loads(resp.read())
    puuid = tdata.get('subject')
    access_token = tdata.get('accessToken')
    token = tdata.get('token')

plat = 'ewogICJwbGF0Zm9ybVR5cGUiOiAiUEMiLAogICJwbGF0Zm9ybU9TIjogIldpbmRvd3MiLAogICJwbGF0Zm9ybU9TVmVyc2lvbiI6ICIxMC4wLjE5MDQyLjEuMjU2LjY0Yml0IiwKICAicGxhdGZvcm1DaGlwc2V0IjogIlVua25vd24iCn0='

headers = {
    'Authorization': f'Bearer {access_token}',
    'X-Riot-Entitlements-JWT': token,
    'X-Riot-ClientPlatform': plat,
    'X-Riot-ClientVersion': 'release-13.08-shipping-20-5715960'
}

# 1. Check pregame player
try:
    req = urllib.request.Request(f'https://glz-eu-1.eu.a.pvp.net/pregame/v1/players/{puuid}', headers=headers)
    with urllib.request.urlopen(req) as pre_p:
        match_id = json.loads(pre_p.read()).get('MatchID')
        print('Current Pregame MatchID:', match_id)
except Exception as e:
    print('Pregame player status:', e)

# 2. Check coregame player
try:
    req = urllib.request.Request(f'https://glz-eu-1.eu.a.pvp.net/core-game/v1/players/{puuid}', headers=headers)
    with urllib.request.urlopen(req) as core_p:
        core_match_id = json.loads(core_p.read()).get('MatchID')
        print('Current Coregame MatchID:', core_match_id)
        if core_match_id:
            req_loadouts = urllib.request.Request(f'https://glz-eu-1.eu.a.pvp.net/core-game/v1/matches/{core_match_id}/loadouts', headers=headers)
            with urllib.request.urlopen(req_loadouts) as res_l:
                l_json = json.loads(res_l.read())
                print('Coregame Loadouts entries count:', len(l_json.get('Loadouts', [])))
                for entry in l_json.get('Loadouts', []):
                    subj = entry.get('Subject')
                    print('Entry Subject:', subj)
except Exception as e:
    print('Coregame player status:', e)
