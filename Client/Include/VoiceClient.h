#pragma once

// Cần #include <winsock2.h> trước <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

// Include header của PortAudio và LAME
#include "portaudio.h"
#include "lame.h"
#include "mpg123.h"

// TODO: Include header của thư viện giải nén MP3 (ví dụ: mpg123.h)

class VoiceClient {
public:
    VoiceClient();
    ~VoiceClient();

    /**
     * @brief Khởi tạo WinSock, PortAudio, LAME và đăng ký với Server.
     * @param serverIp IP của VoiceServer C++ (UDP).
     * @param serverPort Cổng của VoiceServer C++ (UDP).
     * @param username Tên của người dùng này.
     * @return true nếu khởi tạo thành công.
     */
    bool init(const std::string& serverIp, int serverPort, const std::string& username);

    /**
     * @brief Bắt đầu cuộc gọi. Mở stream âm thanh và khởi động các luồng.
     * @param targetUser Tên của người muốn gọi.
     * @return true nếu bắt đầu thành công.
     */
    bool startCall(const std::string& targetUser);

    /**
     * @brief Dừng cuộc gọi. Đóng stream và dừng các luồng.
     */
    void stopCall();

    /**
     * @brief Dọn dẹp tài nguyên (PortAudio, WinSock).
     */
    void shutdown();

private:
    /**
     * @brief Vòng lặp chạy trên thread riêng để thu âm, nén, và gửi UDP.
     */
    void sendLoop();

    /**
     * @brief Vòng lặp chạy trên thread riêng để nhận UDP, giải nén, và phát.
     */
    void receiveLoop();

    // Hàm helper khởi tạo
    bool initWinSock();
    bool initPortAudio();
    bool initLameEncoder();
    bool initMp3Decoder(); // TODO: Cần hiện thực hàm này
    bool registerWithServer();

    // Trạng thái
    std::atomic<bool> m_isInitialized;
    std::atomic<bool> m_isCalling;

    // Thông tin mạng
    SOCKET m_udpSocket;
    sockaddr_in m_serverAddr;
    std::string m_currentUsername;
    std::string m_targetUsername;

    // Luồng
    std::thread m_sendThread;
    std::thread m_receiveThread;

    // Âm thanh (PortAudio)
    PaStream* m_recordStream;
    PaStream* m_playStream;

    // Nén/Giải nén (LAME)
    lame_t m_lameEncoder;
    // TODO: Khai báo đối tượng giải nén (ví dụ: mpg123_handle* m_mp3Decoder;)
    mpg123_handle* m_mp3Decoder; 
};