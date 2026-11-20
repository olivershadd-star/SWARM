@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ===============================================
echo   ANKS LABS SWARM / 01 - Windows VST3 build
 echo ===============================================
where cmake >nul 2>&1
if errorlevel 1 (
  echo ERREUR : CMake introuvable. Installer CMake et Visual Studio 2022 C++.
  pause
  exit /b 1
)

rem An optional local JUCE checkout may be specified as ANKS_JUCE_PATH.
rem Otherwise the official JUCE git repo is downloaded by CMake.
if defined ANKS_JUCE_PATH (
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DANKS_JUCE_PATH="%ANKS_JUCE_PATH%"
) else (
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
)
if errorlevel 1 goto error

cmake --build build --config Release --parallel
if errorlevel 1 goto error

ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 goto error

cmake --build build --config Release --target swarm_windows_package
if errorlevel 1 goto error

echo.
echo COMPILATION REUSSIE.
echo VST3 et Standalone dans build\ANKS_Swarm_01_artefacts\Release\
echo Archives ZIP dans build\packages\Release\.
pause
exit /b 0
:error
echo.
echo ECHEC DE COMPILATION : consulter les messages ci-dessus.
pause
exit /b 1
