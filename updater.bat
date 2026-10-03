@echo off
timeout /t 1 /nobreak > nul
:retry
move /y "RolsTraker.exe" "RolsTraker.exe_old" > nul 2>&1
copy /y "RolsTraker.exe_new" "RolsTraker.exe" > nul 2>&1
if not exist "RolsTraker.exe" (
    timeout /t 1 /nobreak > nul
    goto retry
)
del /f /q "RolsTraker.exe_new" > nul 2>&1
del /f /q "RolsTraker.exe_old" > nul 2>&1
start "" "C:\Users\rolsik\Desktop\ValorantPartyChecker\RolsTraker.exe"
del "%~f0"
