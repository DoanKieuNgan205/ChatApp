#include "Network.h"
#include <iostream>

namespace Network {
    SOCKET connectToServer(const std::string& host, int port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return INVALID_SOCKET;
        }

        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Socket creation failed\n";
            WSACleanup();
            return INVALID_SOCKET;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = inet_addr(host.c_str());

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed, error: " << WSAGetLastError() << "\n";
            closesocket(sock);
            WSACleanup();
            return INVALID_SOCKET;
        }

        return sock;
    }

    void closeConnection(SOCKET sock) {
        closesocket(sock);
        WSACleanup();
    }
}




