#include <winsock2.h>
#include <iostream>
#include <thread>
#include "ChatServer.h"

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSock, 5);

    cout << "Server dang chay tren port 8888...\n";

    while (true) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        thread t(handleClient, client);
        t.detach();
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}




