#include "fileclient.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

FileClient::FileClient(SOCKET sock, const std::string& user) {
    clientSock = sock;
    username = user; // ✅ gán username người gửi
}

// Gửi file từ client -> server (server sau đó sẽ chuyển tiếp đến người nhận)
bool FileClient::sendFileToServer(const std::string& toUser, const std::string& filePath) {
    namespace fs = std::filesystem;
    if (!fs::exists(filePath)) {
        std::cerr << "[ERROR] File khong ton tai: " << filePath << "\n";
        return false;
    }

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Khong the mo file.\n";
        return false;
    }

    long long fileSize = file.tellg();
    file.seekg(0);
    std::string filename = fs::path(filePath).filename().string();

    // Bước 1: Gửi metadata JSON
    std::string header = "{ \"action\": \"sendfile\", \"from\": \"" + username +
                   "\", \"to\": \"" + toUser +
                   "\", \"filename\": \"" + filename +
                   "\", \"size\": \"" + std::to_string(fileSize) + "\" }";
    std::cout << "[DEBUG] JSON gui server: " << header << std::endl;
    send(clientSock, header.c_str(), header.size(), 0);

    // Bước 2: Gửi dữ liệu file nhị phân
    std::cout << "[INFO] Dang gui file '" << filename 
              << "' den " << toUser << " (" << fileSize << " bytes)...\n";

    const int CHUNK = 4096;
    std::vector<char> buffer(CHUNK);
    long long sent = 0;

    while (!file.eof()) {
        file.read(buffer.data(), CHUNK);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        int s = send(clientSock, buffer.data(), bytesRead, 0);
        if (s <= 0) {
            std::cerr << "\n[ERROR] Loi khi gui du lieu file.\n";
            return false;
        }

        sent += s;
        int percent = static_cast<int>((sent * 100.0) / fileSize);
        std::cout << "\r[Uploading] " << percent << "%";
        std::cout.flush();
    }

    file.close();
    std::cout << "\n Da gui file '" << filename << "' thanh cong.\n";
    return true;
}
