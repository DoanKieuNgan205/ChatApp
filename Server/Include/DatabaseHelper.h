#ifndef DATABASEHELPER_H
#define DATABASEHELPER_H
#include "Library.h"   
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>


class DatabaseHelper {
private:
    SQLHENV hEnv;
    SQLHDBC hDbc;
    bool connected;

public:
    DatabaseHelper();
    ~DatabaseHelper();
    void printSQLError(SQLSMALLINT handleType, SQLHANDLE handle);

    bool connect(const std::string& connStr);
    void disconnect();
    bool isConnected() const { return connected; }

    bool checkLogin(const std::string& username, const std::string& password);
    bool registerUser(const std::string& username, const std::string& password);

    bool saveMessage(const std::string& sender, const std::string& receiver,
                     const std::string& content, const std::string& filename = "");
    std::vector<std::string> getChatHistory(const std::string& username);
};

#endif 


/*#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <sqlext.h>

class DatabaseHelper {
private:
    SQLHENV hEnv;
    SQLHDBC hDbc;
    bool connected;

    void printSQLError(SQLSMALLINT handleType, SQLHANDLE handle);

public:
    DatabaseHelper();
    ~DatabaseHelper();

    bool connect(const std::string& connStr);
    void disconnect();
    bool isConnected() const { return connected; }

    // ===== USER FUNCTIONS =====
    bool checkLogin(const std::string& username, const std::string& password);
    bool checkUserExists(const std::string& username);
    bool addUser(const std::string& username, const std::string& password);

    // ===== CHAT FUNCTIONS =====
    bool saveMessage(const std::string& sender,
                     const std::string& receiver,
                     const std::string& message,
                     const std::string& fileName,
                     const std::string& messageType);

    std::vector<std::string> getHistory(const std::string& username);
};*/
