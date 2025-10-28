/*#include "Packet.h"
#include <sstream>

Packet PacketUtil::parseJson(const std::string& json) {
    Packet pkt;
    pkt.type = PacketType::UNKNOWN;
    auto findValue = [&](const std::string& key) {
        size_t pos = json.find(key);
        if (pos == std::string::npos) return std::string("");
        pos = json.find(":", pos);
        size_t start = json.find("\"", pos);
        size_t end = json.find("\"", start + 1);
        return json.substr(start + 1, end - start - 1);
    };
    std::string action = findValue("\"action\"");
    if (action == "login") pkt.type = PacketType::LOGIN;
    else if (action == "register") pkt.type = PacketType::REGISTER;
    else if (action == "chat") pkt.type = PacketType::CHAT;
    else if (action == "file") pkt.type = PacketType::FILE;
    else if (action == "voice") pkt.type = PacketType::VOICE;

    pkt.data["username"] = findValue("\"username\"");
    pkt.data["password"] = findValue("\"password\"");
    pkt.data["message"] = findValue("\"message\"");
    pkt.data["filename"] = findValue("\"filename\"");
    return pkt;
}

std::string PacketUtil::toJson(const Packet& pkt) {
    std::ostringstream os;
    os << "{";
    switch (pkt.type) {
        case PacketType::LOGIN: os << "\"action\":\"login_response\","; break;
        case PacketType::REGISTER: os << "\"action\":\"register_response\","; break;
        case PacketType::CHAT: os << "\"action\":\"chat_response\","; break;
        default: os << "\"action\":\"unknown_response\","; break;
    }
    for (auto it = pkt.data.begin(); it != pkt.data.end(); ++it) {
        os << "\"" << it->first << "\":\"" << it->second << "\"";
        if (std::next(it) != pkt.data.end()) os << ",";
    }
    os << "}";
    return os.str();
}*/
