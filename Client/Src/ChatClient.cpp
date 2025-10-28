#pragma comment(lib, "ws2_32.lib")
#include <thread>
#include <iostream>
#include <winsock2.h>
#include <string>
#include <fstream>
#include <vector>
#include "FileClient.h"


std::string parseField(const std::string &json, const std::string &field) {
    size_t pos = json.find("\"" + field + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}


/*void sendFile(SOCKET sock, const std::string &username, const std::string &to, const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Khong mo duoc file\n";
        return;
    }
    long size = file.tellg();
    file.seekg(0);

    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

    // Gửi header JSON
    std::string header = "{ \"action\": \"send_file\", \"from\": \"" + username +
                        "\", \"to\": \"" + to +
                        "\", \"filename\": \"" + filename +
                        "\", \"size\": \"" + std::to_string(size) + "\" }";
    send(sock, header.c_str(), header.size(), 0);

    // Gửi dữ liệu file
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    send(sock, buffer.data(), size, 0);

    std::cout << "[INFO] Da gui file: " << filename << " (" << size << " bytes)\n";
}*/



/*void receiveMessages(SOCKET sock) {
    char buf[512];
    while (true) {
        int bytes = recv(sock, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) break;

        buf[bytes] = '\0';
        std::string msg(buf);
        std::string action = parseField(msg, "action");

        if (action == "sendfile") {
            std::string from = parseField(msg, "from");
            std::string filename = parseField(msg, "filename");
            std::string sizeStr = parseField(msg, "size");
            long fileSize = stol(sizeStr);


            std::cout << "\n[INFO] Dang nhan file " << filename
                      << " (" << fileSize << " bytes) tu " << from << "...\n";

            
            // Nhận dữ liệu file theo từng khối để tránh treo
            std::vector<char> buffer;
            buffer.reserve(fileSize);

            long long received = 0;
            char chunk[4096]; // đọc mỗi lần 4KB

            while (received < fileSize) {
                int r = recv(sock, chunk, sizeof(chunk), 0);
                if (r <= 0) break;

                buffer.insert(buffer.end(), chunk, chunk + r);
                received += r;

                // Hiển thị tiến trình tải %
                int percent = static_cast<int>((received * 100.0) / fileSize);
                std::cout << "\r[Downloading] " << percent << "%";
                std::cout.flush();

            }

            std::cout << "\n";

            std::ofstream out(filename, std::ios::binary);
            if (out.is_open()) {
                out.write(buffer.data(), received);
                out.close();
                std::cout << "[INFO] Da luu file thanh cong: " << filename
                        << " (" << received << " bytes)\n> ";
            } else {
                std::cout << "[ERROR] Khong the luu file: " << filename << "\n> ";
            }
            
        }
        else if (action == "history") {
            std::string message = parseField(msg, "message");
            std::cout << "[Lich su] " << message << "\n> ";
        }
        else if (action == "private") {
            std::string from = parseField(msg, "from");
            std::string to = parseField(msg, "to");
            std::string message = parseField(msg, "message");
            std::cout << "[Private] " << from << " -> " << to << ": " << message << "\n> ";
        }
        else {
            std::string message = parseField(msg, "message");
            if (!message.empty())
                std::cout << message << "\n> ";
        }
    }
}*/

/*void receiveMessages(SOCKET sock) {
    constexpr int BUFSZ = 8192;
    std::vector<char> recvBuf(BUFSZ);

    while (true) {
        int bytes = recv(sock, recvBuf.data(), (int)recvBuf.size(), 0);
        if (bytes <= 0) break;

        // Chuyển thành string để parse JSON header (chỉ phần text)
        std::string msg(recvBuf.data(), bytes);

        // Tìm kết thúc JSON header (đơn giản: tìm dấu '}')
        size_t jsonEnd = msg.find('}');
        if (jsonEnd == std::string::npos) {
            // Nếu không thấy '}', coi đây là tin nhắn text bình thường
            // (hoặc JSON dài hơn kích thước buffer - trường hợp hiếm)
            std::string action = parseField(msg, "action");
            if (action.empty()) action = parseField(msg, "type");
            std::string message = parseField(msg, "message");
            if (!message.empty())
                std::cout << message << "\n> ";
            continue;
        }

        // Lấy jsonPart (từ đầu đến jsonEnd bao gồm '}')
        std::string jsonPart = msg.substr(0, jsonEnd + 1);

        // Hỗ trợ cả "action" hoặc "type"
        std::string action = parseField(jsonPart, "action");
        if (action.empty()) action = parseField(jsonPart, "type");

        // Nếu server dùng tên khác ("send_file" vs "sendfile"), chuẩn hóa
        if (action == "send_file") action = "sendfile";

        if (action == "sendfile") {
            std::string from = parseField(jsonPart, "from");
            std::string filename = parseField(jsonPart, "filename");
            std::string sizeStr = parseField(jsonPart, "size");
            long long fileSize = 0;
            try { fileSize = std::stoll(sizeStr); } catch (...) { fileSize = 0; }

            if (filename.empty() || fileSize <= 0) {
                std::cerr << "\n[ERROR] Metadata file không hợp lệ.\n> ";
                // Nếu có dữ liệu thừa, bỏ qua
                continue;
            }

            std::cout << "\n[INFO] Dang nhan file " << filename
                      << " (" << fileSize << " bytes) tu " << from << "...\n";

            // Mở file để ghi
            std::ofstream out(filename, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "[ERROR] Khong the tao file: " << filename << "\n> ";
                // Nếu có phần dữ liệu file được gửi kèm, cần vẫn phải đọc và bỏ qua nó
                long long toDiscard = fileSize;
                long long already = (long long)bytes - (long long)(jsonEnd + 1);
                if (already > 0) {
                    // bỏ phần đã nhận
                    toDiscard -= already;
                }
                char tmp[4096];
                while (toDiscard > 0) {
                    int r = recv(sock, tmp, (int)std::min<long long>(sizeof(tmp), toDiscard), 0);
                    if (r <= 0) break;
                    toDiscard -= r;
                }
                continue;
            }

            // Nếu buffer ban đầu có dữ liệu file thừa sau JSON, ghi nó trước
            long long received = 0;
            long long alreadyBytes = (long long)bytes - (long long)(jsonEnd + 1);
            if (alreadyBytes > 0) {
                out.write(recvBuf.data() + (jsonEnd + 1), (std::streamsize)alreadyBytes);
                received += alreadyBytes;
                // Hiển thị tiến trình
                int percent = static_cast<int>((received * 100.0) / fileSize);
                std::cout << "\r[Downloading] " << percent << "%";
                std::cout.flush();
            }

            // Tiếp tục nhận cho đến khi đủ fileSize
            char chunk[4096];
            while (received < fileSize) {
                int r = recv(sock, chunk, (int)std::min<long long>((long long)sizeof(chunk), fileSize - received), 0);
                if (r <= 0) {
                    std::cerr << "\n[ERROR] Mat ket noi khi nhan file.\n";
                    break;
                }
                out.write(chunk, r);
                received += r;

                int percent = static_cast<int>((received * 100.0) / fileSize);
                std::cout << "\r[Downloading] " << percent << "%";
                std::cout.flush();
            }

            out.close();
            if (received == fileSize) {
                std::cout << "\n[INFO] Da luu file thanh cong: " << filename
                          << " (" << received << " bytes)\n> ";
            } else {
                std::cout << "\n[ERROR] File nhan khong du: " << received << " / " << fileSize << "\n> ";
            }
        }
        else {
            // Không phải file — xử lý các action khác (private, history, chat...)
            std::string message = parseField(jsonPart, "message");
            if (!message.empty()) {
                std::cout << message << "\n> ";
            } else {
                // Fallback: in raw if cannot parse
                std::cout << jsonPart << "\n> ";
            }
            // Nếu còn dữ liệu thừa sau JSON (bytes - jsonEnd -1), có thể đó là phần của tin nhắn tiếp theo.
            // Cơ bản: ta sẽ tiếp vòng lặp và nhận tiếp dữ liệu.
        }
    }
}*/

void receiveMessages(SOCKET sock) {
    constexpr int BUFSZ = 8192;
    std::vector<char> recvBuf(BUFSZ);

    while (true) {
        int bytes = recv(sock, recvBuf.data(), (int)recvBuf.size(), 0);
        if (bytes <= 0) break;

        // Chuyển dữ liệu sang string để tách phần JSON đầu tiên
        std::string msg(recvBuf.data(), bytes);

        size_t start = 0; // thêm
        bool printedSomething = false; // 🔥 cờ kiểm soát in dấu ">"

        while(true) { // thêm
            // Xác định vị trí kết thúc JSON header
            //size_t jsonEnd = msg.find('}');
            size_t jsonEnd = msg.find('}', start); //thêm

            /*if (jsonEnd == std::string::npos) {
                // Nếu không có JSON hợp lệ, xử lý như tin nhắn thường
                std::string message = parseField(msg, "message");
                if (!message.empty()) std::cout << message << "\n> ";
                continue;
            }

            std::string jsonPart = msg.substr(0, jsonEnd + 1);
            std::string action = parseField(jsonPart, "action");
            if (action.empty()) action = parseField(jsonPart, "type");*/
            
            if (jsonEnd == std::string::npos) break;

            std::string jsonPart = msg.substr(start, jsonEnd - start + 1);
            start = jsonEnd + 1;

            std::string action = parseField(jsonPart, "action");
            if (action.empty()) action = parseField(jsonPart, "type");


            // Chuẩn hóa tên action
            if (action == "send_file") action = "sendfile";

            // ------------------ NHẬN FILE ------------------
            if (action == "sendfile") {
                std::string from = parseField(jsonPart, "from");
                std::string filename = parseField(jsonPart, "filename");
                std::string sizeStr = parseField(jsonPart, "size");
                long long fileSize = 0;
                try { fileSize = std::stoll(sizeStr); } catch (...) { fileSize = 0; }

                if (filename.empty() || fileSize <= 0) {
                    std::cerr << "\n[ERROR] Metadata file khong hop le.\n> ";
                    continue;
                }

                std::cout << "\n[INFO] Dang nhan file '" << filename
                        << "' (" << fileSize << " bytes) tu " << from << "...\n";

                std::ofstream out(filename, std::ios::binary);
                if (!out.is_open()) {
                    std::cerr << "[ERROR] Khong the tao file: " << filename << "\n> ";
                    continue;
                }

                // Nếu gói đầu tiên có chứa sẵn dữ liệu file
                long long received = 0;
                long long already = (long long)bytes - (long long)(jsonEnd + 1);
                if (already > 0) {
                    out.write(recvBuf.data() + (jsonEnd + 1), (std::streamsize)already);
                    received += already;
                    int percent = static_cast<int>((received * 100.0) / fileSize);
                    std::cout << "\r[Downloading] " << percent << "%";
                    std::cout.flush();
                }

                // Tiếp tục nhận cho đến đủ dung lượng file
                char chunk[4096];
                while (received < fileSize) {
                    int r = recv(sock, chunk, (int)std::min<long long>((long long)sizeof(chunk), fileSize - received), 0);
                    if (r <= 0) {
                        std::cerr << "\n[ERROR] Mat ket noi trong khi nhan file.\n";
                        break;
                    }
                    out.write(chunk, r);
                    received += r;

                    int percent = static_cast<int>((received * 100.0) / fileSize);
                    std::cout << "\r[Downloading] " << percent << "%";
                    std::cout.flush();
                }

                out.close();
                if (received == fileSize) {
                    std::cout << "\n[SUCCESS] Da luu file thanh cong: " << filename
                            << " (" << received << " bytes)\n> ";
                } else {
                    std::cout << "\n[ERROR] File nhan chua đay đu: " << received
                            << " / " << fileSize << " bytes\n> ";
                }
                printedSomething = true; // thêm
            }

            // ------------------ TIN NHẮN KHÁC ------------------
            /*(else if (action == "history") {
                std::string message = parseField(msg, "message");
                std::cout << "[Lich su] " << message << "\n> ";*/

            else if (action == "history") {
                std::string message = parseField(jsonPart, "message");
                if (!message.empty()) {
                    static bool first = true;
                    if (first) {
                        std::cout << "\n========= LiCH SU TIN NHAN =========\n";
                        first = false;
                    }
                    std::cout << message << "\n";
                    printedSomething = true; // thêm
                }
                continue;
            }
            
            else if (action == "private") {
                std::string from = parseField(msg, "from");
                std::string to = parseField(msg, "to");
                std::string message = parseField(msg, "message");
                std::cout << "[Private] " << from << " -> " << to << ": " << message << "\n> ";
                printedSomething = true; // thêm
            }
            else {
                std::string message = parseField(jsonPart, "message");
                if (!message.empty()) {
                    std::cout << message << "\n> ";
                    printedSomething = true;
                } else {
                    std::cout << jsonPart << "\n> ";
                    printedSomething = true;
                }
            }
        }
        // 🔧 Chỉ in dấu nhắc khi thực sự có dữ liệu hiển thị
        if (printedSomething)
            std::cout << "> ";
    }
}




void startChat(SOCKET sock, const std::string& username) {
    bool running = true;
    std::thread receiver(receiveMessages, sock);
    receiver.detach();

    FileClient fileClient(sock, username); // 👈 tạo 1 đối tượng xử lý file

    std::string msg;
    while (running) {
        std::cout << "> ";
        if (!std::getline(std::cin, msg)) break;

        if (msg == "/quit") {
            std::string quitMsg = "{ \"action\": \"quit\", \"username\": \"" + username + "\" }";
            send(sock, quitMsg.c_str(), quitMsg.size(), 0);
            running = false;
            break;
        }

        if (msg == "/list") {
            std::string request = "{ \"action\": \"list\" }";
            if (send(sock, request.c_str(), request.size(), 0) == SOCKET_ERROR) {
                std::cerr << "[DEBUG] Khong the gui du lieu.\n";
                break;
            }
            continue;
        }

        if (msg.rfind("/pm ", 0) == 0) {
            size_t space1 = msg.find(' ', 4);
            if (space1 != std::string::npos) {
                std::string recipient = msg.substr(4, space1 - 4);
                std::string content = msg.substr(space1 + 1);
                std::string sendMsg =
                    "{ \"action\": \"private\", \"from\": \"" + username +
                    "\", \"to\": \"" + recipient +
                    "\", \"message\": \"" + content + "\" }";
                if (send(sock, sendMsg.c_str(), sendMsg.size(), 0) == SOCKET_ERROR) {
                    std::cerr << "[DEBUG] Khong the gui du lieu.\n";
                    break;
                }
                continue;
            }
        }

        /*if (msg.rfind("/sendfile ", 0) == 0) {
            size_t space1 = msg.find(' ', 10);
            if (space1 != std::string::npos) {
                std::string recipient = msg.substr(10, space1 - 10);
                std::string filepath = msg.substr(space1 + 1);
                sendFile(sock, username, recipient, filepath);
                continue;
            }
        }*/

        // Gửi file: /sendfile <tên_người_nhận> <đường_dẫn_file>
        if (msg.rfind("/sendfile ", 0) == 0) {
            size_t space1 = msg.find(' ', 10);
            if (space1 != std::string::npos) {
                std::string recipient = msg.substr(10, space1 - 10);
                std::string filepath = msg.substr(space1 + 1);
                fileClient.sendFileToServer(recipient, filepath);
                continue;
            } else {
                std::cout << "Cu phap dung: /sendfile <ten_nguoi_nhan> <duong_dan_file>\n";
                continue;
            }
        }
        
        std::string sendMsg =
            "{ \"action\": \"chat\", \"username\": \"" + username +
            "\", \"message\": \"" + msg + "\" }";
        if (send(sock, sendMsg.c_str(), sendMsg.size(), 0) == SOCKET_ERROR) {
            std::cerr << "[DEBUG] Khong the gui du lieu.\n";
            break;
        }
    }

    running = false;
    if (receiver.joinable()) receiver.join();
}








