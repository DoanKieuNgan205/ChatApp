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


    bool saveFileHistory(const std::string& sender, const std::string& receiver, const std::string& filename);
    bool saveCallHistory(const std::string& caller, const std::string& receiver, const std::string& status);
    std::vector<std::string> getFileHistory(const std::string& username);
    std::vector<std::string> getCallHistory(const std::string& username);
};

#endif

