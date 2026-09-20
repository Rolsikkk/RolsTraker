@echo off
chcp 65001 > nul
title RolsTraker
if exist build\RolsTraker.exe copy /y build\RolsTraker.exe RolsTraker.exe > nul
cls
RolsTraker.exe
pause
