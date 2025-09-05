#pragma once
#include <string>
#include <winsock2.h>
using namespace std;

namespace Network {
    SOCKET connectToServer(const string& host, int port);
    void closeConnection(SOCKET sock);
}








