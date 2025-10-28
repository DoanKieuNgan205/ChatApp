#pragma once
#include <winsock2.h>
#include <string>

class FileClient {
private:
    SOCKET clientSock;
    std::string username;

public:
    FileClient(SOCKET sock, const std::string& user);
    bool sendFileToServer(const std::string& toUser, const std::string& filePath);
};
