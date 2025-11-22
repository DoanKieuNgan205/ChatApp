#include <winsock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <map>
#include "FileServer.h"
#include "ChatServer.h"
#include "VoiceServer.h"
#include "DatabaseHelper.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")


DatabaseHelper db;
map<string, SOCKET> userMap; 

void startChatServer() {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        cerr << "❌ Khong the tao socket ChatServer: " << WSAGetLastError() << endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "❌ Loi bind ChatServer: " << WSAGetLastError() << endl;
        return;
    }

    listen(listenSock, 5);
    cout << "[INFO] ChatServer dang chay tren port 8888...\n";

    while (true) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            cerr << "⚠️ Loi accept: " << WSAGetLastError() << endl;
            continue;
        }
        thread t(handleClient, client);
        t.detach();
    }

    closesocket(listenSock);
}

void startFileServer(FileServer& fileServer) {
    try {
        cout << "[INFO] FileServer dang chay tren port 9999...\n";
        fileServer.startFileServer(userMap);
    } catch (const exception& e) {
        cerr << "❌ Loi FileServer: " << e.what() << endl;
    }
}

void startVoiceServer(VoiceServer& voiceServer) {
    try {
        voiceServer.start(6060); 
    } catch (const exception& e) {
        cerr << "❌ Loi VoiceServer: " << e.what() << endl;
    }
}

int main() {

    system("chcp 65001 >nul");
    string connStr = "Driver={ODBC Driver 17 for SQL Server};Server=ADMIN-PC,1433;Database=DULIEU;Trusted_Connection=Yes;";
    if (!db.connect(connStr)) {
        cerr << "❌ Khong the ket noi SQL Server. Thoat...\n";
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "❌ Loi khoi tao Winsock.\n";
        return 1;
    }

    FileServer fileServer;
    VoiceServer voiceServer;

    thread chatThread(startChatServer);
    thread fileThread(startFileServer, ref(fileServer));
    thread voiceThread(startVoiceServer, ref(voiceServer));

    cout << "\n=============================\n";
    cout << "   He thong da san sang:\n";
    cout << "  • ChatServer: TCP 8888\n";
    cout << "  • FileServer: TCP 9999\n";
    cout << "  • VoiceServer: UDP 6060\n";
    cout << "=============================\n";

    chatThread.join();
    fileThread.join();
    voiceThread.join();

    WSACleanup();
    db.disconnect();
    cout << "[INFO] Server da dung va giai phong tai nguyen.\n";

    return 0;
}





















