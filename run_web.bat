@echo off
title Limit Order Book Pro - Real-Time Exchange & Web UI
echo =======================================================================
echo     Limit Order Book & Quantitative Strategy Trading Engine
echo =======================================================================
echo.

REM 1. Check if web_bridge.exe exists, if not compile it
if not exist "web_bridge.exe" (
    echo [*] Compiling C++ Web Bridge engine...
    g++ -std=c++14 -O3 -Wall -Wextra -Wpedantic -Iinclude src\OrderBook.cpp src\MatchingEngine.cpp src\MarketSimulator.cpp src\MarketMaker.cpp src\AvellanedaStoikovMarketMaker.cpp src\web_bridge.cpp -o web_bridge.exe
    if errorlevel 1 (
        echo [!] C++ Compilation Failed!
        pause
        exit /b 1
    )
    echo [*] C++ compilation succeeded!
)

echo [*] Starting FastAPI + WebSocket Server on http://127.0.0.1:8000
echo [*] Open your browser at: http://127.0.0.1:8000
echo.

python -m uvicorn backend.server:app --host 127.0.0.1 --port 8000
pause
