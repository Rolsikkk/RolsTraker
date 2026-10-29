@echo off
chcp 65001 > nul
title RolsTracker
if exist build\RolsTracker.exe copy /y build\RolsTracker.exe RolsTracker.exe > nul
if exist build\RolsTracker.exe copy /y build\RolsTracker.exe RolsTraker.exe > nul
cls
if exist RolsTracker.exe (
    RolsTracker.exe
) else (
    RolsTraker.exe
)
pause
