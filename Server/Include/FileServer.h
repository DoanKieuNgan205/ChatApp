#pragma once
#include <winsock2.h>
#include <string>
#include <map>

class FileServer {
public:
    void startFileServer(std::map<std::string, SOCKET>& clientMap);
    static bool handleIncomingFile(
        SOCKET senderSock,
        const std::string& fromUser,
        const std::string& toUser,
        const std::string& filename,
        long long fileSize,
         std::map<std::string, SOCKET>& clientMap
    );
};


