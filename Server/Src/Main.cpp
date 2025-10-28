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

// 🔹 Biến toàn cục
DatabaseHelper db;
map<string, SOCKET> userMap; 

// 🔹 Hàm chạy ChatServer (TCP)
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

// 🔹 Hàm chạy FileServer (TCP)
void startFileServer(FileServer& fileServer) {
    try {
        cout << "[INFO] FileServer dang chay tren port 9999...\n";
        fileServer.startFileServer(userMap);
    } catch (const exception& e) {
        cerr << "❌ Loi FileServer: " << e.what() << endl;
    }
}

// 🔹 Hàm chạy VoiceServer (UDP)
void startVoiceServer(VoiceServer& voiceServer) {
    try {
        voiceServer.start(6060); // Cổng UDP
    } catch (const exception& e) {
        cerr << "❌ Loi VoiceServer: " << e.what() << endl;
    }
}

// 🔹 Chương trình chính
int main() {

    system("chcp 65001 >nul");
    // ✅ Kết nối Database
    string connStr = "Driver={ODBC Driver 17 for SQL Server};Server=ADMIN-PC,1433;Database=DULIEU;Trusted_Connection=Yes;";
    if (!db.connect(connStr)) {
        cerr << "❌ Khong the ket noi SQL Server. Thoat...\n";
        return 1;
    }

    // ✅ Khởi tạo Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "❌ Loi khoi tao Winsock.\n";
        return 1;
    }

    FileServer fileServer;
    VoiceServer voiceServer;

    // ✅ Khởi chạy đa luồng cho 3 server
    thread chatThread(startChatServer);
    thread fileThread(startFileServer, ref(fileServer));
    thread voiceThread(startVoiceServer, ref(voiceServer));

    cout << "\n=============================\n";
    cout << "   He thong da san sang:\n";
    cout << "  • ChatServer: TCP 8888\n";
    cout << "  • FileServer: TCP 9999\n";
    cout << "  • VoiceServer: UDP 6060\n";
    cout << "=============================\n";

    // ✅ Chờ các luồng hoàn tất
    chatThread.join();
    fileThread.join();
    voiceThread.join();

    // ✅ Dọn dẹp
    WSACleanup();
    db.disconnect();
    cout << "[INFO] Server da dung va giai phong tai nguyen.\n";

    return 0;
}

/*#include <winsock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <mutex>
#include "ServerCore.h"
#include "FileServer.h"
#include "ChatServer.h"
#include "VoiceServer.h"
#include "DatabaseHelper.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")

// 🔹 Biến toàn cục
DatabaseHelper db;
map<string, SOCKET> userMap; 
mutex mtx;
vector<SOCKET> clients;

// ==========================
// 🔹 1️⃣ Login Server (TCP)
// ==========================
void startLoginServer() {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        cerr << "❌ Không thể tạo socket LoginServer: " << WSAGetLastError() << endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(7777);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "❌ Lỗi bind LoginServer: " << WSAGetLastError() << endl;
        return;
    }

    listen(listenSock, 5);
    cout << "[INFO] LoginServer đang chạy trên port 7777...\n";

    ServerCore core;

    while (true) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            cerr << "⚠️ Lỗi accept LoginServer: " << WSAGetLastError() << endl;
            continue;
        }

        thread([client, &core]() {
            char buffer[1024] = {0};
            int bytes = recv(client, buffer, sizeof(buffer), 0);
            if (bytes > 0) {
                string request(buffer, bytes);
                cout << "[LOGIN] Nhận yêu cầu: " << request << endl;

                string response = core.handleRequest(request);
                send(client, response.c_str(), (int)response.size(), 0);

                cout << "[LOGIN] Gửi phản hồi: " << response << endl;
            }
            closesocket(client);
        }).detach();
    }

    closesocket(listenSock);
}

// ==========================
// 🔹 2️⃣ Chat Server (TCP)
// ==========================
void startChatServer() {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        cerr << "❌ Không thể tạo socket ChatServer.\n";
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "❌ Lỗi bind ChatServer: " << WSAGetLastError() << endl;
        return;
    }

    listen(listenSock, 5);
    cout << "[INFO] ChatServer đang chạy trên port 8888...\n";

    ChatServer chatServer;

    while (true) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        thread([client, &chatServer]() {
            char buffer[4096] = {};
            int bytes = recv(client, buffer, sizeof(buffer), 0);
            if (bytes <= 0) { closesocket(client); return; }

            string request(buffer, bytes);
            cout << "[CHAT] Nhận: " << request << endl;

            string response = chatServer.handleChat(request);
            send(client, response.c_str(), (int)response.size(), 0);

            cout << "[CHAT] Gửi: " << response << endl;

            closesocket(client);
        }).detach();
    }

    closesocket(listenSock);
}

// ==========================
// 🔹 3️⃣ File Server (TCP)
// ==========================
void startFileServer(FileServer& fileServer) {
    try {
        cout << "[INFO] FileServer đang chạy trên port 9999...\n";
        fileServer.startFileServer(userMap);
    } catch (const exception& e) {
        cerr << "❌ Lỗi FileServer: " << e.what() << endl;
    }
}

// ==========================
// 🔹 4️⃣ Voice Server (UDP)
// ==========================
void startVoiceServer(VoiceServer& voiceServer) {
    try {
        voiceServer.start(6060);
    } catch (const exception& e) {
        cerr << "❌ Lỗi VoiceServer: " << e.what() << endl;
    }
}

// ==========================
// 🔹 5️⃣ Chương trình chính
// ==========================
int main() {

    system("chcp 65001 >nul");

    // ✅ Kết nối Database
    string connStr = "Driver={ODBC Driver 17 for SQL Server};Server=ADMIN-PC,1433;Database=DULIEU;Trusted_Connection=Yes;";
    if (!db.connect(connStr)) {
        cerr << "❌ Không thể kết nối SQL Server. Thoát...\n";
        return 1;
    }

    // ✅ Khởi tạo Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "❌ Lỗi khởi tạo Winsock.\n";
        return 1;
    }

    FileServer fileServer;
    VoiceServer voiceServer;

    // ✅ Khởi chạy đa luồng cho 4 server
    thread loginThread(startLoginServer);
    thread chatThread(startChatServer);
    thread fileThread(startFileServer, ref(fileServer));
    thread voiceThread(startVoiceServer, ref(voiceServer));

    cout << "\n=============================\n";
    cout << "   Hệ thống đã sẵn sàng:\n";
    cout << "  • LoginServer: TCP 7777\n";
    cout << "  • ChatServer: TCP 8888\n";
    cout << "  • FileServer: TCP 9999\n";
    cout << "  • VoiceServer: UDP 6060\n";
    cout << "================================\n";

    loginThread.join();
    chatThread.join();
    fileThread.join();
    voiceThread.join();

    // ✅ Dọn dẹp
    WSACleanup();
    db.disconnect();
    cout << "[INFO] Server đã dừng và giải phóng tài nguyên.\n";

    return 0;
}*/



















