/*#include "ServerCore.h"
#include "Packet.h"
#include "LoginController.h"
#include "RegisterController.h"
#include "ChatServer.h"
#include "FileServer.h"
#include "VoiceServer.h"

std::string ServerCore::handleRequest(const std::string& jsonRequest) {
    Packet pkt = PacketUtil::parseJson(jsonRequest);

    switch (pkt.type) {
        case PacketType::LOGIN:
            return LoginController::handleLogin(jsonRequest);
        case PacketType::REGISTER:
            return RegisterController::handleRegister(jsonRequest);
        case PacketType::CHAT:
            return ChatServer::handleChat(jsonRequest);
        case PacketType::FILE:
            return FileServer::handleFile(jsonRequest);
        case PacketType::VOICE:
            return VoiceServer::handleVoice(jsonRequest);
        default:
            return R"({"action":"error","message":"Unknown action"})";
    }
}*/
