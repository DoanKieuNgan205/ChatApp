#include "VoiceClient.h"
#include <iostream>
#include <stdexcept> 

// --- Thêm các include thư viện ---
#include "portaudio.h"
#include "lame.h"
#include "mpg123.h"

// --- Cấu hình âm thanh và mạng ---
#define SAMPLE_RATE         (22050)     // Tần số lấy mẫu (Hz)
#define NUM_CHANNELS        (1)         // 1 = Mono (tốt cho voice)
#define FRAMES_PER_BUFFER   (1024)      // Số lượng frame mỗi buffer
#define MP3_BITRATE         (32)        // Bitrate (kbps) - 32 là đủ cho voice mono
#define MP3_BUFFER_SIZE     (8192)      // Kích thước buffer MP3 (phải đủ lớn)
#define PCM_BUFFER_SIZE     (1152 * 4)  // Buffer cho PCM đã giải nén (1152 là size frame MP3 chuẩn)
// ------------------------------------


VoiceClient::VoiceClient()
    : m_isInitialized(false),
    m_isCalling(false),
    m_udpSocket(INVALID_SOCKET),
    m_recordStream(nullptr),
    m_playStream(nullptr),
    m_lameEncoder(nullptr),
    m_mp3Decoder(nullptr) // <-- ĐÃ THÊM
{
}

VoiceClient::~VoiceClient() {
    shutdown();
}

bool VoiceClient::init(const std::string& serverIp, int serverPort, const std::string& username) {
    m_currentUsername = username;

    if (!initWinSock()) return false;
    if (!initPortAudio()) return false;
    if (!initLameEncoder()) return false;
    if (!initMp3Decoder()) return false; // <-- ĐÃ HOÀN THIỆN HÀM NÀY

    // Thiết lập địa chỉ server
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIp.c_str(), &m_serverAddr.sin_addr);

    // Đăng ký với server
    if (!registerWithServer()) {
        std::cerr << "Khong the dang ky voi server UDP!\n";
        return false;
    }

    std::cout << "VoiceClient khoi tao thanh cong.\n";
    m_isInitialized = true;
    return true;
}

bool VoiceClient::startCall(const std::string& targetUser) {
    if (!m_isInitialized || m_isCalling) {
        return false;
    }

    m_targetUsername = targetUser;
    m_isCalling = true;
    PaError err;

    // 1. Mở stream thu âm
    err = Pa_OpenDefaultStream(
        &m_recordStream,
        NUM_CHANNELS,     // input channels
        0,                // output channels
        paInt16,          // 16-bit integer
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        nullptr,          // không dùng callback
        nullptr);
    if (err != paNoError) {
        std::cerr << "Loi mo stream thu am: " << Pa_GetErrorText(err) << "\n";
        return false;
    }

    // 2. Mở stream phát
    err = Pa_OpenDefaultStream(
        &m_playStream,
        0,                // input channels
        NUM_CHANNELS,     // output channels
        paInt16,          // 16-bit integer
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        nullptr,          // không dùng callback
        nullptr);
    if (err != paNoError) {
        std::cerr << "Loi mo stream phat: " << Pa_GetErrorText(err) << "\n";
        Pa_CloseStream(m_recordStream);
        return false;
    }

    // 3. Bắt đầu stream
    Pa_StartStream(m_recordStream);
    Pa_StartStream(m_playStream);

    // 4. Khởi động luồng
    m_sendThread = std::thread(&VoiceClient::sendLoop, this);
    m_receiveThread = std::thread(&VoiceClient::receiveLoop, this);

    std::cout << "Bat dau cuoc goi voi " << targetUser << "\n";
    return true;
}

void VoiceClient::stopCall() {
    if (!m_isCalling) {
        return;
    }

    m_isCalling = false;

    // Đợi các luồng kết thúc
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    // Dừng và đóng stream
    if (m_recordStream) {
        Pa_StopStream(m_recordStream);
        Pa_CloseStream(m_recordStream);
        m_recordStream = nullptr;
    }
    if (m_playStream) {
        Pa_StopStream(m_playStream);
        Pa_CloseStream(m_playStream);
        m_playStream = nullptr;
    }

    m_targetUsername = "";
    std::cout << "Ket thuc cuoc goi.\n";
}

void VoiceClient::shutdown() {
    stopCall(); // Đảm bảo mọi thứ đã dừng

    if (m_isInitialized) {
        // Dọn dẹp LAME
        if (m_lameEncoder) {
            lame_close(m_lameEncoder);
            m_lameEncoder = nullptr;
        }

        // --- ĐÃ THÊM: Dọn dẹp MP3 decoder ---
        if (m_mp3Decoder) {
            mpg123_close(m_mp3Decoder);
            mpg123_delete(m_mp3Decoder);
            m_mp3Decoder = nullptr;
        }
        mpg123_exit(); // Dọn dẹp thư viện
        // ------------------------------------

        // Dọn dẹp PortAudio
        Pa_Terminate();

        // Dọn dẹp WinSock
        if (m_udpSocket != INVALID_SOCKET) {
            closesocket(m_udpSocket);
            m_udpSocket = INVALID_SOCKET;
        }
        WSACleanup();

        m_isInitialized = false;
        std::cout << "VoiceClient shutdown.\n";
    }
}

// --- LUỒNG GỬI (Thu âm, Nén, Gửi) ---
void VoiceClient::sendLoop() {
    std::vector<short> pcmBuffer(FRAMES_PER_BUFFER * NUM_CHANNELS);
    std::vector<unsigned char> mp3Buffer(MP3_BUFFER_SIZE);

    while (m_isCalling) {
        // 1. Thu âm (đây là hàm blocking)
        PaError err = Pa_ReadStream(m_recordStream, pcmBuffer.data(), FRAMES_PER_BUFFER);
        if (err != paNoError) {
            std::cerr << "Loi doc stream: " << Pa_GetErrorText(err) << "\n";
            continue;
        }

        // 2. Nén (Encode)
        int mp3Bytes = lame_encode_buffer(
            m_lameEncoder,
            pcmBuffer.data(), // left channel
            pcmBuffer.data(), // right channel (giống left vì là mono)
            FRAMES_PER_BUFFER,
            mp3Buffer.data(),
            mp3Buffer.size()
        );

        if (mp3Bytes < 0) {
            std::cerr << "Loi LAME encode: " << mp3Bytes << "\n";
            continue;
        }

        if (mp3Bytes > 0) {
            // 3. Gửi UDP
            std::string header = "FROM:" + m_currentUsername + "|TO:" + m_targetUsername + "|";
            
            std::vector<char> packet(header.begin(), header.end());
            packet.insert(packet.end(), mp3Buffer.data(), mp3Buffer.data() + mp3Bytes);

            sendto(m_udpSocket, 
                   packet.data(), 
                   (int)packet.size(), 
                   0, 
                   (sockaddr*)&m_serverAddr, 
                   sizeof(m_serverAddr));
        }
    }
}

// --- LUỒNG NHẬN (Nhận, Giải nén, Phát) ---
void VoiceClient::receiveLoop() {
    std::vector<char> networkBuffer(MP3_BUFFER_SIZE + 1024); // Đủ cho cả header
    sockaddr_in fromAddr;
    int fromAddrLen = sizeof(fromAddr);

    // --- ĐÃ THÊM: Khởi tạo buffer PCM ---
    std::vector<short> decodedPcmBuffer(PCM_BUFFER_SIZE);
    int mpgErr = MPG123_OK;
    size_t pcmBytesDecoded = 0;
    // ------------------------------------

    while (m_isCalling) {
        // 1. Nhận UDP
        int bytesReceived = recvfrom(m_udpSocket,
            networkBuffer.data(),
            (int)networkBuffer.size(),
            0,
            (sockaddr*)&fromAddr,
            &fromAddrLen);

        if (bytesReceived <= 0) {
            continue; // Lỗi hoặc timeout
        }

        // 2. Phân tích gói tin
        std::string packetStr(networkBuffer.data(), bytesReceived);
        size_t dataPos = packetStr.find("|DATA|");

        if (dataPos == std::string::npos) {
            continue;
        }

        // Trỏ con trỏ vào đúng dữ liệu MP3 nhị phân
        const unsigned char* mp3Data = (unsigned char*)(networkBuffer.data() + dataPos + 6);
        int mp3Len = bytesReceived - (dataPos + 6);

        if (mp3Len > 0) {
            // --- ĐÃ THÊM: 3. Giải nén MP3 (Decode) ---
            
            // Đẩy (feed) dữ liệu MP3 thô vào bộ giải nén
            mpgErr = mpg123_feed(m_mp3Decoder, mp3Data, mp3Len);
            if (mpgErr != MPG123_OK) {
                std::cerr << "Loi mpg123_feed: " << mpg123_strerror(m_mp3Decoder) << "\n";
                continue;
            }

            // Lấy (read) dữ liệu PCM đã giải nén ra.
            // Phải dùng vòng lặp, vì 1 gói MP3 có thể tạo ra nhiều PCM.
            do {
                mpgErr = mpg123_read(
                    m_mp3Decoder,
                    (unsigned char*)decodedPcmBuffer.data(), // Buffer đầu ra
                    decodedPcmBuffer.size() * sizeof(short), // Kích thước (bytes)
                    &pcmBytesDecoded // Số bytes thực sự đã giải nén
                );

                if (pcmBytesDecoded > 0) {
                    // 4. Phát âm thanh (Playback)
                    // Chuyển đổi từ byte sang số lượng frame
                    int pcmFrames = pcmBytesDecoded / (sizeof(short) * NUM_CHANNELS);
                    
                    Pa_WriteStream(m_playStream, decodedPcmBuffer.data(), pcmFrames);
                }

            // Lặp lại cho đến khi bộ giải nén báo cần thêm dữ liệu
            } while (mpgErr == MPG123_OK || mpgErr == MPG123_NEED_MORE);
            // ------------------------------------
        }
    }
}

// --- CÁC HÀM KHỞI TẠO HELPER ---

bool VoiceClient::initWinSock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup that bai.\n";
        return false;
    }
    
    m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udpSocket == INVALID_SOCKET) {
        std::cerr << "Tao socket UDP that bai: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    DWORD timeout = 1000; // 1 giây
    setsockopt(m_udpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    return true;
}

bool VoiceClient::initPortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init that bai: " << Pa_GetErrorText(err) << "\n";
        return false;
    }
    return true;
}

bool VoiceClient::initLameEncoder() {
    m_lameEncoder = lame_init();
    if (!m_lameEncoder) {
        std::cerr << "LAME init that bai.\n";
        return false;
    }
    
    lame_set_in_samplerate(m_lameEncoder, SAMPLE_RATE);
    lame_set_num_channels(m_lameEncoder, NUM_CHANNELS);
    lame_set_mode(m_lameEncoder, (NUM_CHANNELS == 1) ? MONO : STEREO);
    lame_set_brate(m_lameEncoder, MP3_BITRATE);
    lame_set_quality(m_lameEncoder, 5); 
    
    if (lame_init_params(m_lameEncoder) < 0) {
        std::cerr << "LAME init_params that bai.\n";
        return false;
    }
    return true;
}

// --- ĐÃ HOÀN THIỆN HÀM NÀY ---
bool VoiceClient::initMp3Decoder() {
    // 1. Khởi tạo thư viện
    int err = mpg123_init();
    if (err != MPG123_OK) {
        std::cerr << "Khong the khoi tao libmpg123: " << mpg123_plain_strerror(err) << "\n";
        return false;
    }

    // 2. Tạo một handle
    m_mp3Decoder = mpg123_new(nullptr, &err);
    if (!m_mp3Decoder) {
        std::cerr << "Khong the tao handle mpg123: " << mpg123_plain_strerror(err) << "\n";
        return false;
    }

    // 3. Thiết lập chế độ "feed-based" (nhận dữ liệu liên tục)
    if (mpg123_open_feed(m_mp3Decoder) != MPG123_OK) {
        std::cerr << "Khong the mo feed cho mpg123.\n";
        return false;
    }
    
    // 4. Bắt buộc format đầu ra (quan trọng!)
    // Chúng ta muốn output khớp với PortAudio (SAMPLE_RATE, 1 kênh, 16-bit)
    mpg123_format_none(m_mp3Decoder); // Xóa hết format
    mpg123_format(m_mp3Decoder, SAMPLE_RATE, (NUM_CHANNELS == 1) ? MPG123_MONO : MPG123_STEREO, MPG123_ENC_SIGNED_16); // 16-bit
    
    std::cout << "libmpg123 khoi tao thanh cong.\n";
    return true; 
}

bool VoiceClient::registerWithServer() {
    std::string msg = "REGISTER:" + m_currentUsername;
    int sentBytes = sendto(m_udpSocket, 
                           msg.c_str(), 
                           (int)msg.length(), 
                           0, 
                           (sockaddr*)&m_serverAddr, 
                           sizeof(m_serverAddr));
                           
    return sentBytes == (int)msg.length();
}