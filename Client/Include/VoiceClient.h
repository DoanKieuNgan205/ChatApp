#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

#include "portaudio.h"
#include "lame.h"
#include "mpg123.h"

class VoiceClient {
public:
    VoiceClient();
    ~VoiceClient();

    bool init(const std::string& serverIp, int serverPort, const std::string& username);
    bool startCall(const std::string& targetUser);
    void stopCall();
    void shutdown();

private:
    void sendLoop();
    void receiveLoop();

    bool initWinSock();
    bool initPortAudio();
    bool initLameEncoder();
    bool initMp3Decoder(); 
    bool registerWithServer();

    std::atomic<bool> m_isInitialized;
    std::atomic<bool> m_isCalling;

    SOCKET m_udpSocket;
    sockaddr_in m_serverAddr;
    std::string m_currentUsername;
    std::string m_targetUsername;
    
    std::thread m_sendThread;
    std::thread m_receiveThread;

    
    PaStream* m_recordStream;
    PaStream* m_playStream;

    
    lame_t m_lameEncoder;
    
    mpg123_handle* m_mp3Decoder; 
};