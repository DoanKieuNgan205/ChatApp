@echo off
setlocal

echo ============================
echo 🚀 Build + Run ChatApp
echo ============================

REM --- Build Server ---
echo [1/2] Building Server...
g++ Server\Src\Main.cpp Server\Src\Auth.cpp Server\Src\LoginController.cpp Server\Src\ChatServer.cpp -IServer\Include -o server.exe -lws2_32
if errorlevel 1 (
    echo ❌ Build Server FAILED
    pause
    exit /b
)
echo ✅ Server built: server.exe

REM --- Build Client ---
echo [2/2] Building Client...
g++ Client\Src\Main.cpp Client\Src\Login.cpp Client\Src\Network.cpp Client\Src\ChatClient.cpp -IClient\Include -o client.exe -lws2_32
if errorlevel 1 (
    echo ❌ Build Client FAILED
    pause
    exit /b
)
echo ✅ Client built: client.exe

REM --- Run Server ---
echo ============================
echo 🚀 Starting Server...
echo ============================
start cmd /k "server.exe"

REM --- Delay 2 giây trước khi mở client ---
timeout /t 2 /nobreak >nul

REM --- Run Client ---
echo ============================
echo 🚀 Starting Client...
echo ============================
start cmd /k "client.exe"

echo ============================
echo 🎉 All done! Server + Client are running
echo ============================

pause
