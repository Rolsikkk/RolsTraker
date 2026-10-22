import os, re

log_path = os.path.expandvars(r'%LOCALAPPDATA%\VALORANT\Saved\Logs\ShooterGame.log')
if os.path.exists(log_path):
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()
    urls = set(re.findall(r'https://[^\s"\']+\.pvp\.net[^\s"\']*', text))
    for u in list(urls)[:15]:
        print(u)
else:
    print('No ShooterGame.log')
