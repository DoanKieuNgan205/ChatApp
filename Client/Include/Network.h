#pragma once
#include <winsock2.h>
#include <string>
using namespace std;


namespace Network {
    SOCKET connectToServer(const string& host, int port);
    void closeConnection(SOCKET sock);
}








