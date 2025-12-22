#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include "FileServer.h"
#include "ChatServer.h"
#include "VoiceServer.h"
#include "DatabaseHelper.h"

using namespace std;
#pragma comment(lib, "ws2_32.lib")

DatabaseHelper db;
map<string, SOCKET> userMap; 
extern vector<SOCKET> clients;
extern mutex mtx;


void sendMessageToUserCallback(const string& username, const string& message) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        string msg = message + "\n";
        int sent = ::send(it->second, msg.c_str(), (int)msg.size(), 0);  
        if (sent > 0) {
            cout << "[VOICE->CHAT] ✅ Sent to " << username << endl;
        } else {
            cerr << "[VOICE->CHAT] ❌ Failed, error: " << WSAGetLastError() << endl;
        }
    } else {
        cout << "[VOICE->CHAT] ⚠️ User " << username << " not found" << endl;
    }
}

void startChatServer() {
    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, 0);  
    if (listenSock == INVALID_SOCKET) {
        cerr << "❌ Khong the tao socket ChatServer: " << WSAGetLastError() << endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int bindResult = ::bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bindResult != 0) {
        cerr << "❌ Loi bind ChatServer: " << WSAGetLastError() << endl;
        closesocket(listenSock);
        return;
    }

    int listenResult = ::listen(listenSock, 5);
    if (listenResult != 0) {
        cerr << "❌ Loi listen ChatServer: " << WSAGetLastError() << endl;
        closesocket(listenSock);
        return;
    }
    
    cout << "[INFO] ✅ ChatServer dang chay tren port 8888...\n";

    while (true) {
        SOCKET client = ::accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            cerr << "⚠️ Loi accept: " << WSAGetLastError() << endl;
            continue;
        }
        
        cout << "[INFO] 🔗 New client connected: Socket " << client << endl;
        
        thread t(handleClient, client);
        t.detach();
    }

    closesocket(listenSock);
}

void startFileServer(FileServer& fileServer) {
    try {
        cout << "[INFO] ✅ FileServer dang chay tren port 9999...\n";
        fileServer.startFileServer(userMap);
    } catch (const exception& e) {
        cerr << "❌ Loi FileServer: " << e.what() << endl;
    }
}

void startVoiceServer(VoiceServer& voiceServer) {
    try {
        bool started = voiceServer.start(6060);
        if (started) {
            cout << "[INFO] ✅ VoiceServer started successfully on UDP 6060\n";
        } else {
            cerr << "❌ Failed to start VoiceServer\n";
        }
    } catch (const exception& e) {
        cerr << "❌ Loi VoiceServer: " << e.what() << endl;
    }
}

int main() {
    system("chcp 65001 >nul");

    string connStr = "Driver={ODBC Driver 17 for SQL Server};Server=ADMIN-PC,1433;Database=DULIEU;Trusted_Connection=Yes;MultipleActiveResultSets=True;";
    
    cout << "[INFO] 🔌 Connecting to database..." << endl;
    if (!db.connect(connStr)) {
        cerr << "❌ Khong the ket noi SQL Server. Thoat...\n";
        return 1;
    }
    cout << "[INFO] ✅ Database connected successfully\n";

    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        cerr << "❌ Loi khoi tao Winsock: " << wsaResult << "\n";
        return 1;
    }
    cout << "[INFO] ✅ Winsock initialized\n";

    FileServer fileServer;
    VoiceServer voiceServer;

    cout << "[INFO] 🔗 Setting up VoiceServer callback..." << endl;
    voiceServer.setNotifyCallback(sendMessageToUserCallback);
    cout << "[INFO] ✅ VoiceServer callback configured\n";
    cout << "\n[INFO] 🚀 Starting server threads...\n";
    
    thread chatThread(startChatServer);
    thread fileThread(startFileServer, ref(fileServer));
    thread voiceThread(startVoiceServer, ref(voiceServer));

    this_thread::sleep_for(chrono::milliseconds(500));

    cout << "\n╔═════════════════════════════════╗\n";
    cout << "║   HE THONG DA SAN SANG!         ║\n";
    cout << "╠═════════════════════════════════╣\n";
    cout << "║  • ChatServer:  TCP 8888        ║\n";
    cout << "║  • FileServer:  TCP 9999        ║\n";
    cout << "║  • VoiceServer: UDP 6060        ║\n";
    cout << "║  • Database:    Connected       ║\n";
    cout << "║  • Callback:    Configured      ║\n";
    cout << "╚═════════════════════════════════╝\n\n";

    cout << "[INFO] Press Ctrl+C to stop the server...\n\n";

    int counter = 0;
    while (true) {
        this_thread::sleep_for(chrono::seconds(30));
        
        {
            lock_guard<mutex> lock(mtx);
            cout << "\n[STATUS] 👥 Online users: " << userMap.size() << endl;
            if (!userMap.empty()) {
                cout << "[STATUS] Users: ";
                for (const auto& pair : userMap) {
                    cout << pair.first << " ";
                }
                cout << endl;
            }
        }
        
        counter++;
        if (counter % 10 == 0) {
            cout << "[STATUS] 🕐 Server uptime: " << (counter * 30 / 60) << " minutes" << endl;
        }
    }

    return 0;
}




















