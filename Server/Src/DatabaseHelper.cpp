#include "DatabaseHelper.h"
#include <iostream>
using namespace std;

DatabaseHelper::DatabaseHelper() : hEnv(NULL), hDbc(NULL), connected(false) {
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
}

DatabaseHelper::~DatabaseHelper() {
    disconnect();
    if (hEnv) SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

void DatabaseHelper::printSQLError(SQLSMALLINT handleType, SQLHANDLE handle) {
    SQLWCHAR sqlState[6], message[256];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;
    SQLRETURN ret;

    int i = 1;
    while ((ret = SQLGetDiagRecW(handleType, handle, i,
                sqlState, &nativeError, message, sizeof(message)/sizeof(SQLWCHAR), &textLength)) 
                != SQL_NO_DATA) 
    {
        wcout << L"[ODBC ERROR] SQLSTATE: " << sqlState
              << L", Message: " << message << endl;
        i++;
    }
}

bool DatabaseHelper::connect(const std::string& connStr) {
    if (connected) return true;
    SQLCHAR outStr[1024];
    SQLSMALLINT outLen;
    SQLRETURN ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS, outStr, sizeof(outStr), &outLen, SQL_DRIVER_NOPROMPT);
    if (SQL_SUCCEEDED(ret)) {
        connected = true;
        return true;
    } else {
        std::cerr << "Ket noi SQL Server that bai.\n";
        printSQLError(SQL_HANDLE_DBC, hDbc);
        return false;
    }
}

void DatabaseHelper::disconnect() {
    if (connected) {
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        connected = false;
    }
}

bool DatabaseHelper::registerUser(const std::string& username, 
                                  const std::string& password,
                                  const std::string& email) {
    if (!connected) return false;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string checkSql = "SELECT 1 FROM [User] WHERE username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)checkSql.c_str(), SQL_NTS);
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLRETURN ret = SQLExecute(stmt);

    if (SQL_SUCCEEDED(ret) && SQLFetch(stmt) == SQL_SUCCESS) {
        std::cerr << "[DB] Username đã tồn tại.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string insertSql = "INSERT INTO [User] (username, password, email) VALUES (?, ?, ?)";
    ret = SQLPrepareA(stmt, (SQLCHAR*)insertSql.c_str(), SQL_NTS);

    SQLUINTEGER userColSize = 50;
    SQLUINTEGER passColSize = 50;
    SQLUINTEGER emailColSize = 50;

    SQLLEN lenPass = (SQLLEN)password.size();
    SQLLEN lenEmail = email.empty() ? SQL_NULL_DATA : (SQLLEN)email.size();
    lenUser = (SQLLEN)username.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        userColSize, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
        
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        passColSize, 0, (SQLPOINTER)password.c_str(), 0, &lenPass);
    
    if (email.empty()) {
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
            emailColSize, 0, NULL, 0, &lenEmail);
    } else {
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
            emailColSize, 0, (SQLPOINTER)email.c_str(), 0, &lenEmail);
    }

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] Thêm tài khoản thất bại.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    std::cout << "[DB] Đăng ký tài khoản thành công.\n";
    return true;
}


std::string DatabaseHelper::getUserEmail(const std::string& username) {
    if (!connected) return "";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    std::string sql = "SELECT email FROM [User] WHERE username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLRETURN ret = SQLExecute(stmt);
    SQLCHAR email[100] = {0};
    SQLLEN indicator = 0;
    if (SQL_SUCCEEDED(ret)) {
        if (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_CHAR, email, sizeof(email), &indicator);
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    if (indicator == SQL_NULL_DATA) return "";
    return std::string((char*)email);
}

bool DatabaseHelper::updateUserEmail(const std::string& username, const std::string& email) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    std::string sql = "UPDATE [User] SET email = ? WHERE username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    SQLLEN lenEmail = (SQLLEN)email.size();
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 100, 0, (SQLPOINTER)email.c_str(), 0, &lenEmail);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLRETURN ret = SQLExecute(stmt);
    bool success = SQL_SUCCEEDED(ret);
    if (!success) printSQLError(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return success;
}

bool DatabaseHelper::updatePassword(const std::string& username, const std::string& oldPass, const std::string& newPass) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    std::string checkSql = "SELECT 1 FROM [User] WHERE username = ? AND password = ?";
    SQLPrepareA(stmt, (SQLCHAR*)checkSql.c_str(), SQL_NTS);
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLLEN lenOld = (SQLLEN)oldPass.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)oldPass.c_str(), 0, &lenOld);
    SQLRETURN ret = SQLExecute(stmt);
    if (SQLFetch(stmt) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    std::string updateSql = "UPDATE [User] SET password = ? WHERE username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)updateSql.c_str(), SQL_NTS);
    SQLLEN lenNew = (SQLLEN)newPass.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)newPass.c_str(), 0, &lenNew);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    ret = SQLExecute(stmt);
    bool success = SQL_SUCCEEDED(ret);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return success;
}




bool DatabaseHelper::checkLogin(const std::string& username, const std::string& password) {
    if (!connected) return false;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string query = "SELECT 1 FROM [User] WHERE username=? AND password=?";
    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);

    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] Chuan bi cau lenh that bai.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLLEN lenPass = (SQLLEN)password.size();

    SQLUINTEGER userColSize = 50;
    SQLUINTEGER passColSize = 50;


    
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        userColSize, 
        0, (SQLPOINTER)username.c_str(), 0, &lenUser);

    
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        passColSize, 
        0, (SQLPOINTER)password.c_str(), 0, &lenPass);

   
    ret = SQLExecute(stmt);
    bool success = false;

    if (SQL_SUCCEEDED(ret)) {
        if (SQLFetch(stmt) == SQL_SUCCESS) {
            success = true; 
        }
    } else {
        std::cerr << "[DB] SQLExecute thất bại.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return success;
}

bool DatabaseHelper::saveMessage(const std::string& sender,
                                 const std::string& receiver,
                                 const std::string& message,
                                 const std::string& filename) {
    if (!connected) return false;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql = "INSERT INTO ChatHistory (Sender, Receiver, Message, FileName) VALUES (?, ?, ?, ?)";
    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] Loi chuan bi cau lenh luu tin nhan.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    std::string safeContent = message.empty() ? "" : message;
    std::string safeFile = filename.empty() ? "" : filename;
    
    SQLUINTEGER senderColSize = 50;   
    SQLUINTEGER receiverColSize = 50; 
    SQLUINTEGER msgColSize = 2000;    
    SQLUINTEGER fileColSize = 255;    
    SQLLEN lenSender = (SQLLEN)sender.size();
    SQLLEN lenReceiver = (SQLLEN)receiver.size();
    SQLLEN lenMessage = (SQLLEN)message.size(); 
    SQLLEN lenFile = (SQLLEN)filename.size();   


    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        senderColSize, 
        0, (SQLPOINTER)sender.c_str(), 0, &lenSender);

    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        receiverColSize, 
        0, (SQLPOINTER)receiver.c_str(), 0, &lenReceiver);

    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        msgColSize, 
        0, (SQLPOINTER)message.c_str(), 0, &lenMessage); 

    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
        fileColSize, 
        0, (SQLPOINTER)filename.c_str(), 0, &lenFile);  

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] Loi khi thuc thi luu tin nhan.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

std::vector<std::string> DatabaseHelper::getChatHistory(const std::string& username) {
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql =
        "SELECT Sender, Receiver, Message, FileName, "
        "CONVERT(VARCHAR, SentAt, 120) AS SentTime "
        "FROM ChatHistory "
        "WHERE Sender = ? OR Receiver = ? OR Receiver = 'all' "
        "ORDER BY SentAt ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    if (!SQL_SUCCEEDED(ret)) { 
        std::cerr << "[DB] Loi prepare cau lenh lay lich su.\n"; 
        printSQLError(SQL_HANDLE_STMT, stmt); 
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); 
        return history; 
    }

    SQLLEN lenUser = (SQLLEN)username.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser); 
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) { 
        std::cerr << "[DB] Loi chuan bi cau lenh lay lich su.\n"; 
        printSQLError(SQL_HANDLE_STMT, stmt); 
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); 
        return history; 
    }

    SQLCHAR sender[50], receiver[50], message[2000], filename[255], sentTime[30];
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        memset(sender, 0, sizeof(sender)); 
        memset(receiver, 0, sizeof(receiver)); 
        memset(message, 0, sizeof(message)); 
        memset(filename, 0, sizeof(filename)); 
        memset(sentTime, 0, sizeof(sentTime));

        SQLGetData(stmt, 1, SQL_C_CHAR, sender, sizeof(sender), NULL); 
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL); 
        SQLGetData(stmt, 3, SQL_C_CHAR, message, sizeof(message), NULL); 
        SQLGetData(stmt, 4, SQL_C_CHAR, filename, sizeof(filename), NULL); 
        SQLGetData(stmt, 5, SQL_C_CHAR, sentTime, sizeof(sentTime), NULL);

        std::string direction = (username == (char*)sender) ? "->" : "<-"; 
        std::string msg = "[Lich su] " + std::string((char*)sender) + " " + direction + " " + std::string((char*)receiver) + ": "; 
        if (strlen((char*)filename) > 0) 
            msg += "(File: " + std::string((char*)filename) + ")"; 
        else if (strlen((char*)message) > 0) 
            msg += std::string((char*)message); 
        else 
            msg += "(Tin nhan trong)"; 
        msg += " [" + std::string((char*)sentTime) + "]"; 
        history.push_back(msg);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}


bool DatabaseHelper::saveFileHistory(const std::string& sender, const std::string& receiver, const std::string& filename) {
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    std::string query = "INSERT INTO FileHistory (Sender, Receiver, FileName, Timestamp) VALUES (?, ?, ?, GETDATE())";
    SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLCHAR*)sender.c_str(), 0, NULL);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLCHAR*)receiver.c_str(), 0, NULL);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, (SQLCHAR*)filename.c_str(), 0, NULL);
    SQLRETURN ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool DatabaseHelper::saveCallHistory(
    const std::string& caller,
    const std::string& receiver,
    int duration,
    const std::string& status
) {
    if (!connected) return false;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string query =
        "INSERT INTO CallHistory (Caller, Receiver, Duration, Status, Timestamp) "
        "VALUES (?, ?, ?, ?, GETDATE())";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare saveCallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN lenCaller = (SQLLEN)caller.size();
    SQLLEN lenReceiver = (SQLLEN)receiver.size();
    SQLLEN lenStatus = (SQLLEN)status.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     50, 0, (SQLPOINTER)caller.c_str(), 0, &lenCaller);

    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     50, 0, (SQLPOINTER)receiver.c_str(), 0, &lenReceiver);

    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                     0, 0, (SQLPOINTER)&duration, 0, NULL);

    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     20, 0, (SQLPOINTER)status.c_str(), 0, &lenStatus);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi execute saveCallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    
    std::cout << "[DB] ✅ Saved call history: " << caller << " -> " << receiver 
              << ", status=" << status << ", duration=" << duration << "s\n";

    return true;
}


std::vector<std::string> DatabaseHelper::getFileHistory(const std::string& username) {
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string query =
        "SELECT Sender, Receiver, FileName, CONVERT(VARCHAR, Timestamp, 120) AS SentTime "
        "FROM FileHistory "
        "WHERE Sender = ? OR Receiver = ? "
        "ORDER BY Timestamp ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare lấy FileHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi thực thi lấy FileHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR sender[50], receiver[50], filename[255], sentTime[30];
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        memset(sender, 0, sizeof(sender));
        memset(receiver, 0, sizeof(receiver));
        memset(filename, 0, sizeof(filename));
        memset(sentTime, 0, sizeof(sentTime));

        SQLGetData(stmt, 1, SQL_C_CHAR, sender, sizeof(sender), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, filename, sizeof(filename), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, sentTime, sizeof(sentTime), NULL);

        std::string record = "[File] " + std::string((char*)sender) + " -> "
            + std::string((char*)receiver) + ": "
            + std::string((char*)filename)
            + " [" + std::string((char*)sentTime) + "]";
        history.push_back(record);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}

std::vector<std::string> DatabaseHelper::getCallHistory(const std::string& username) {
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string query =
        "SELECT Caller, Receiver, Duration, Status, CONVERT(VARCHAR, Timestamp, 120) AS CallTime "
        "FROM CallHistory "
        "WHERE Caller = ? OR Receiver = ? "
        "ORDER BY Timestamp ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare lấy CallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi thực thi lấy CallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR caller[50], receiver[50], status[50], callTime[30];
    SQLINTEGER duration = 0;
    SQLLEN durationIndicator = 0;

    while (SQLFetch(stmt) == SQL_SUCCESS) {
        memset(caller, 0, sizeof(caller));
        memset(receiver, 0, sizeof(receiver));
        memset(status, 0, sizeof(status));
        memset(callTime, 0, sizeof(callTime));
        duration = 0;

        SQLGetData(stmt, 1, SQL_C_CHAR, caller, sizeof(caller), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);
        SQLGetData(stmt, 3, SQL_C_SLONG, &duration, 0, &durationIndicator);
        SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, callTime, sizeof(callTime), NULL);

        std::string record = "[Call] " + std::string((char*)caller) + " -> "
            + std::string((char*)receiver) + ": "
            + std::to_string(duration) + "s ("
            + std::string((char*)status) + ") ["
            + std::string((char*)callTime) + "]";
        
        history.push_back(record);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}

std::vector<std::string> DatabaseHelper::getPrivateChatHistory(
    const std::string& user1, const std::string& user2)
{
   
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string query =
        "SELECT Sender, Receiver, Message, FileName, "
        "CONVERT(VARCHAR, SentAt, 120) AS SentTime "
        "FROM ChatHistory "
        "WHERE (Sender = ? AND Receiver = ?) OR (Sender = ? AND Receiver = ?) "
        "ORDER BY SentAt ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare lấy PrivateChatHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLLEN lenUser1 = (SQLLEN)user1.size();
    SQLLEN lenUser2 = (SQLLEN)user2.size();
    SQLUINTEGER userColSize = 50;

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     userColSize, 0, (SQLPOINTER)user1.c_str(), 0, &lenUser1);

    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     userColSize, 0, (SQLPOINTER)user2.c_str(), 0, &lenUser2);

    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     userColSize, 0, (SQLPOINTER)user2.c_str(), 0, &lenUser2);

    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     userColSize, 0, (SQLPOINTER)user1.c_str(), 0, &lenUser1);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi thực thi lấy PrivateChatHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR sender[50], receiver[50];
    SQLCHAR message[2000], filename[255], sentTime[30];
    SQLLEN indicator = 0;

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        memset(sender, 0, sizeof(sender));
        memset(receiver, 0, sizeof(receiver));
        memset(message, 0, sizeof(message));
        memset(filename, 0, sizeof(filename));
        memset(sentTime, 0, sizeof(sentTime));

        SQLGetData(stmt, 1, SQL_C_CHAR, sender, sizeof(sender), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);

        SQLGetData(stmt, 3, SQL_C_CHAR, message, sizeof(message), &indicator);
        if (indicator == SQL_NULL_DATA) message[0] = '\0';

        SQLGetData(stmt, 4, SQL_C_CHAR, filename, sizeof(filename), &indicator);
        if (indicator == SQL_NULL_DATA) filename[0] = '\0';

        SQLGetData(stmt, 5, SQL_C_CHAR, sentTime, sizeof(sentTime), NULL);

        std::string record = "[Chat] " +
            std::string((char*)sender) + " -> " + 
            std::string((char*)receiver) + ": ";

        if (strlen((char*)filename) > 0)
            record += "(File: " + std::string((char*)filename) + ")";
        else if (strlen((char*)message) > 0)
            record += std::string((char*)message);
        else
            record += "(Tin nhắn trống)";

        record += " [" + std::string((char*)sentTime) + "]";

        history.push_back(record);
    }

    SQLFreeStmt(stmt, SQL_CLOSE);  
    SQLFreeStmt(stmt, SQL_UNBIND);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}

std::vector<std::string> DatabaseHelper::getPrivateFileHistory(const std::string& user1, const std::string& user2) {

    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt))) return history;

    std::string query =
        "SELECT Sender, Receiver, FileName, CONVERT(VARCHAR, Timestamp, 120) AS SentTime "
        "FROM FileHistory "
        "WHERE (Sender = ? AND Receiver = ?) OR (Sender = ? AND Receiver = ?) "
        "ORDER BY Timestamp ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare lấy PrivateFileHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLLEN lenU1 = user1.size();
    SQLLEN lenU2 = user2.size();
    const SQLUINTEGER COL_SIZE = 50;

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user1.c_str(), 0, &lenU1);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user2.c_str(), 0, &lenU2);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user2.c_str(), 0, &lenU2);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user1.c_str(), 0, &lenU1);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi thực thi lấy PrivateFileHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR sender[50], receiver[50], filename[255], sentTime[30];

    while (SQLFetch(stmt) == SQL_SUCCESS) {

        memset(sender, 0, sizeof(sender));
        memset(receiver, 0, sizeof(receiver));
        memset(filename, 0, sizeof(filename));
        memset(sentTime, 0, sizeof(sentTime));

        SQLGetData(stmt, 1, SQL_C_CHAR, sender, sizeof(sender), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, filename, sizeof(filename), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, sentTime, sizeof(sentTime), NULL);

        history.push_back(
            "[File] " + std::string((char*)sender) + " -> " +
            std::string((char*)receiver) + ": " +
            std::string((char*)filename) + " [" +
            std::string((char*)sentTime) + "]"
        );
    }

    //SQLCloseCursor(stmt);
    SQLFreeStmt(stmt, SQL_CLOSE);  
    SQLFreeStmt(stmt, SQL_UNBIND);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}

std::vector<std::string> DatabaseHelper::getPrivateCallHistory(const std::string& user1, const std::string& user2) {
     
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt))) return history;

    std::string query =
        "SELECT Caller, Receiver, Duration, Status, CONVERT(VARCHAR, Timestamp, 120) AS CallTime "
        "FROM CallHistory "
        "WHERE (Caller = ? AND Receiver = ?) OR (Caller = ? AND Receiver = ?) "
        "ORDER BY Timestamp ASC";

    SQLRETURN ret = SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi prepare lấy PrivateCallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLLEN lenU1 = user1.size();
    SQLLEN lenU2 = user2.size();
    const SQLUINTEGER COL_SIZE = 50;

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user1.c_str(), 0, &lenU1);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user2.c_str(), 0, &lenU2);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user2.c_str(), 0, &lenU2);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, COL_SIZE, 0, (SQLPOINTER)user1.c_str(), 0, &lenU1);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] ❌ Lỗi thực thi lấy PrivateCallHistory\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR caller[50], receiver[50], status[50], callTime[30];
    SQLINTEGER duration = 0;  
    SQLLEN durationIndicator = 0;

    while (SQLFetch(stmt) == SQL_SUCCESS) {
        memset(caller, 0, sizeof(caller));
        memset(receiver, 0, sizeof(receiver));
        memset(status, 0, sizeof(status));
        memset(callTime, 0, sizeof(callTime));
        duration = 0;

        SQLGetData(stmt, 1, SQL_C_CHAR, caller, sizeof(caller), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);
        SQLGetData(stmt, 3, SQL_C_SLONG, &duration, 0, &durationIndicator);  
        SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, callTime, sizeof(callTime), NULL);

        std::string record = "[Call] " + std::string((char*)caller) + " -> " +
            std::string((char*)receiver) + ": " +
            std::to_string(duration) + "s (" +
            std::string((char*)status) + ") [" +
            std::string((char*)callTime) + "]";

        history.push_back(record);
        
        std::cout << "[DB] 📞 " << record << std::endl;  
    }

    std::cout << "[DB] Total call history loaded: " << history.size() << std::endl;


    SQLFreeStmt(stmt, SQL_CLOSE);  
    SQLFreeStmt(stmt, SQL_UNBIND);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}




