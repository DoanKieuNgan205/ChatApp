#pragma once
#include "Library.h"
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include <iostream>
#include <algorithm>
#include <winsock2.h>


extern std::vector<SOCKET> clients;        
extern std::map<std::string, SOCKET> userMap; 
extern std::mutex mtx;                     


void handleClient(SOCKET client);

void broadcast(const std::string& msg, SOCKET sender);

void sendToUser(const std::string& username, const std::string& msg);

std::string parseField(const std::string& json, const std::string& field);





