/*#pragma once
#include <string>
#include <unordered_map>

enum class PacketType {
    LOGIN,
    REGISTER,
    CHAT,
    FILE,
    VOICE,
    UNKNOWN
};

struct Packet {
    PacketType type;
    std::unordered_map<std::string, std::string> data;
};

// Hàm tiện ích parse JSON đơn giản
namespace PacketUtil {
    Packet parseJson(const std::string& json);
    std::string toJson(const Packet& packet);
}*/
