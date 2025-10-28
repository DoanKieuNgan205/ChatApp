/*#include "fileserver.h"
#include <iostream>
#include <fstream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")


bool FileServer::handleIncomingFile(
    SOCKET senderSock,
    const std::string& fromUser,
    const std::string& toUser,
    const std::string& filename,
    long long fileSize,
    std::map<std::string, SOCKET>& clientMap
) {
    std::cout << "[INFO] Dang nhan file tu " << fromUser
              << " gui toi: " << toUser
              << " | File: " << filename
              << " | Size: " << fileSize << " bytes\n";
              
    auto it = clientMap.find(toUser);
    if (it == clientMap.end()) {
        std::cerr << "[WARN] Nguoi dung '" << toUser << "' khong online. File se bi bo qua.\n";
        // Có thể lưu lại file tạm sau này gửi lại khi B online
        return false;
    }

    SOCKET receiverSock = it->second;

    // Bước 1: Gửi thông báo JSON cho client B để chuẩn bị nhận file
    std::string notify = "{ \"action\": \"sendfile\", \"from\": \"" + fromUser +
                     "\", \"filename\": \"" + filename +
                     "\", \"size\": \"" + std::to_string(fileSize) + "\" }";
    send(receiverSock, notify.c_str(), notify.size(), 0);

    // Bước 2: Đọc dữ liệu file từ A và gửi sang B
    const int CHUNK = 4096;
    std::vector<char> buffer(CHUNK);
    long long received = 0;

    while (received < fileSize) {
        int r = recv(senderSock, buffer.data(), CHUNK, 0);
        if (r <= 0) {
            std::cerr << "[ERROR] Mat ket noi khi nhan file.\n";
            return false;
        }

        send(receiverSock, buffer.data(), r, 0);
        received += r;
    }

    std::cout << "[SUCCESS] File '" << filename << "' da gui toi " << toUser << " thanh cong.\n";
    return true;
}*/

#include "FileServer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

bool FileServer::handleIncomingFile(
    SOCKET senderSock,
    const std::string& fromUser,
    const std::string& toUser,
    const std::string& filename,
    long long fileSize,
    std::map<std::string, SOCKET>& clientMap
) {
    std::cout << "[FILE] " << fromUser << " -> " << toUser
              << " | File: " << filename
              << " | Size: " << fileSize << " bytes\n";

    // Kiểm tra người nhận có online không
    auto it = clientMap.find(toUser);
    if (it == clientMap.end()) {
        std::cerr << "[WARN] '" << toUser << "' không online.\n";
        return false;
    }

    SOCKET receiverSock = it->second;

    // Gửi metadata sang người nhận
    std::string notify = "{ \"action\": \"sendfile\", \"from\": \"" + fromUser +
                         "\", \"filename\": \"" + filename +
                         "\", \"size\": \"" + std::to_string(fileSize) + "\" }";
    send(receiverSock, notify.c_str(), notify.size(), 0);

    // Ghi ra file tạm để test (optional)
    std::ofstream out("received_" + filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Không thể tạo file tạm.\n";
        return false;
    }

    const int CHUNK = 4096;
    std::vector<char> buffer(CHUNK);
    long long received = 0;

    while (received < fileSize) {
        int r = recv(senderSock, buffer.data(), CHUNK, 0);
        if (r <= 0) {
            std::cerr << "[ERROR] Mất kết nối khi nhận file.\n";
            out.close();
            return false;
        }
        out.write(buffer.data(), r);              // Lưu file tạm (test)
        send(receiverSock, buffer.data(), r, 0);
        received += r;
        
    }
    
    out.close();
    std::cout << "[SUCCESS] File '" << filename << "' đã gửi thành công.\n";
    return true;
}

void FileServer::startFileServer(std::map<std::string, SOCKET>& clientMap) {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Không bind được cổng 9999\n";
        return;
    }

    listen(listenSock, 5);
    std::cout << "[INFO] FileServer dang lang nghe tren cong 9999...\n";

    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;

        std::thread([clientSock, &clientMap]() {

            std::string header;
            char ch;
            // Đọc từng ký tự cho tới khi gặp '\n' => header kết thúc
            while (true) {
                int r = recv(clientSock, &ch, 1, 0);
                if (r <= 0) {
                    closesocket(clientSock);
                    return;
                }
                if (ch == '\n') break;
                header += ch;
            }


            // Tách thông tin JSON
            auto getField = [&](const std::string& field) {
                size_t pos = header.find("\"" + field + "\"");
                if (pos == std::string::npos) return std::string();
                pos = header.find(":", pos);
                pos = header.find("\"", pos);
                size_t end = header.find("\"", pos + 1);
                return header.substr(pos + 1, end - pos - 1);
            };

            std::string from = getField("from");
            std::string to = getField("to");
            std::string filename = getField("filename");
            std::string sizeStr = getField("size");

            if (from.empty() || to.empty() || filename.empty() || sizeStr.empty()) {
                std::cerr << "[WARN] Header file không hợp lệ.\n";
                closesocket(clientSock);
                return;
            }

            long long fileSize = std::stoll(sizeStr);
            FileServer fs;
            fs.handleIncomingFile(clientSock, from, to, filename, fileSize, clientMap);
            closesocket(clientSock);

        }).detach();
    }
}

/*#include "FileServer.h"
#include <iostream>

void FileServer::startFileServer(std::map<std::string, SOCKET>& clientMap) {
    std::cout << "[FileServer] Started file server stub.\n";
}

bool FileServer::handleIncomingFile(
    SOCKET senderSock,
    const std::string& fromUser,
    const std::string& toUser,
    const std::string& filename,
    long long fileSize,
    std::map<std::string, SOCKET>& clientMap
) {
    std::cout << "[FileServer] Simulate file transfer from " << fromUser
              << " to " << toUser << " (file: " << filename << ", size: " << fileSize << ")\n";
    return true;
}

std::string FileServer::handleFile(const std::string& request) {
    std::cout << "[FileServer] Received file request: " << equest << "\n";
    return R"({"action":"file_ok","status":"success"})";
}*/




