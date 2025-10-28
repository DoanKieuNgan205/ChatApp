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

/*#pragma once
#include <winsock2.h>
#include <string>
#include <map>

#pragma comment(lib, "Ws2_32.lib")

class FileServer {
public:
    // 🔹 Hàm xử lý logic từ JSON (ServerCore gọi tới)
    static std::string handleFile(const std::string& request);

    // 🔹 Hàm khởi động server TCP thực tế (nếu cần truyền file binary)
    void startFileServer(std::map<std::string, SOCKET>& clientMap);

    // 🔹 Hàm xử lý khi có file đến (cấp thấp)
    static bool handleIncomingFile(
        SOCKET senderSock,
        const std::string& fromUser,
        const std::string& toUser,
        const std::string& filename,
        long long fileSize,
        std::map<std::string, SOCKET>& clientMap
    );
};*/

