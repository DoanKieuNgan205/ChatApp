#include "DatabaseHelper.h"
#include <iostream>
using namespace std;

DatabaseHelper::DatabaseHelper() : hEnv(NULL), hDbc(NULL), connected(false) {
    // Cấp phát môi trường ODBC
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

    SQLRETURN ret = SQLDriverConnectA(
        hDbc,
        NULL,
        (SQLCHAR*)connStr.c_str(),
        SQL_NTS,
        outStr,
        sizeof(outStr),
        &outLen,
        SQL_DRIVER_NOPROMPT 
    );

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

bool DatabaseHelper::registerUser(const std::string& username, const std::string& password) {
    if (!connected) return false;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    // Kiểm tra trùng username
    std::string checkSql = "SELECT 1 FROM [User] WHERE username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)checkSql.c_str(), SQL_NTS);
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLRETURN ret = SQLExecute(stmt);

    if (SQL_SUCCEEDED(ret) && SQLFetch(stmt) == SQL_SUCCESS) {
        std::cerr << "[DB] Username đã tồn tại.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false; // Đã có username này
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    // Thêm tài khoản mới
    std::string insertSql = "INSERT INTO [User] (username, password) VALUES (?, ?)";
    ret = SQLPrepareA(stmt, (SQLCHAR*)insertSql.c_str(), SQL_NTS);

    SQLLEN lenPass = (SQLLEN)password.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, username.size(), 0,
                     (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, password.size(), 0,
                     (SQLPOINTER)password.c_str(), 0, &lenPass);

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

    // Bind tham số 1: Username
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, username.size(), 0,
        (SQLPOINTER)username.c_str(), 0, &lenUser);

    // Bind tham số 2: Password
    ret = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, password.size(), 0,
        (SQLPOINTER)password.c_str(), 0, &lenPass);

    // Thực thi
    ret = SQLExecute(stmt);
    bool success = false;

    if (SQL_SUCCEEDED(ret)) {
        if (SQLFetch(stmt) == SQL_SUCCESS) {
            success = true; // Có user hợp lệ
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

    // Nếu không có file hoặc tin nhắn thì lưu rỗng an toàn
    std::string safeContent = message.empty() ? "" : message;
    std::string safeFile = filename.empty() ? "" : filename;

    SQLLEN lenSender = (SQLLEN)sender.size();
    SQLLEN lenReceiver = (SQLLEN)receiver.size();
    SQLLEN lenMessage = (SQLLEN)message.size();
    SQLLEN lenFile = (SQLLEN)filename.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sender.size(), 0,
        (SQLPOINTER)sender.c_str(), 0, &lenSender);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, receiver.size(), 0,
        (SQLPOINTER)receiver.c_str(), 0, &lenReceiver);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, message.size(), 0,
        (SQLPOINTER)message.c_str(), 0, &lenMessage);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, filename.size(), 0,
        (SQLPOINTER)filename.c_str(), 0, &lenFile);

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

    // Các cột theo thứ tự SELECT: Sender, Message, FileName, SentTime
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


/*#include "DatabaseHelper.h"
#include <iostream>
using namespace std;

DatabaseHelper::DatabaseHelper() : hEnv(NULL), hDbc(NULL), connected(false) {
    // Cấp phát môi trường ODBC
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

    SQLRETURN ret = SQLDriverConnectA(
        hDbc,
        NULL,
        (SQLCHAR*)connStr.c_str(),
        SQL_NTS,
        outStr,
        sizeof(outStr),
        &outLen,
        SQL_DRIVER_NOPROMPT 
    );

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

// ===================================================
// 🔹 Kiểm tra login
// ===================================================
bool DatabaseHelper::checkLogin(const std::string& username, const std::string& password) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    string sql = "SELECT 1 FROM Users WHERE Username=? AND PasswordHash=?";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLLEN lenPass = (SQLLEN)password.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
        (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0,
        (SQLPOINTER)password.c_str(), 0, &lenPass);

    SQLRETURN ret = SQLExecute(stmt);
    bool success = false;
    if (SQL_SUCCEEDED(ret) && SQLFetch(stmt) == SQL_SUCCESS) {
        success = true;
    } else if (!SQL_SUCCEEDED(ret)) {
        printSQLError(SQL_HANDLE_STMT, stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return success;
}

// ===================================================
// 🔹 Kiểm tra user tồn tại
// ===================================================
bool DatabaseHelper::checkUserExists(const std::string& username) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql = "SELECT 1 FROM Users WHERE Username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
        SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);

    SQLRETURN ret = SQLExecute(stmt);
    bool exists = false;
    if (SQL_SUCCEEDED(ret) && SQLFetch(stmt) == SQL_SUCCESS)
        exists = true;

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return exists;
}

// ===================================================
// 🔹 Thêm user mới
// ===================================================
bool DatabaseHelper::addUser(const std::string& username, const std::string& password) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql = "INSERT INTO Users (Username, PasswordHash) VALUES (?, ?)";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    SQLLEN lenUser = (SQLLEN)username.size();
    SQLLEN lenPass = (SQLLEN)password.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        255, 0, (SQLPOINTER)password.c_str(), 0, &lenPass);

    SQLRETURN ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        cerr << "[DB] ❌ Them user that bai.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    cout << "[DB] ✅ Da them user moi: " << username << endl;
    return true;
}

// ===================================================
// 🔹 Lưu tin nhắn
// ===================================================
bool DatabaseHelper::saveMessage(const std::string& sender,
                                 const std::string& receiver,
                                 const std::string& message,
                                 const std::string& fileName,
                                 const std::string& messageType) {
    if (!connected) return false;
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql = "INSERT INTO ChatHistory (SenderID, ReceiverID, Message, FileName, MessageType) "
                      "SELECT S.UserID, R.UserID, ?, ?, ? "
                      "FROM Users S "
                      "LEFT JOIN Users R ON R.Username = ? "
                      "WHERE S.Username = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    SQLLEN lenMsg = (SQLLEN)message.size();
    SQLLEN lenFile = (SQLLEN)fileName.size();
    SQLLEN lenType = (SQLLEN)messageType.size();
    SQLLEN lenRecv = (SQLLEN)receiver.size();
    SQLLEN lenSend = (SQLLEN)sender.size();

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 2000, 0, (SQLPOINTER)message.c_str(), 0, &lenMsg);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, (SQLPOINTER)fileName.c_str(), 0, &lenFile);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLPOINTER)messageType.c_str(), 0, &lenType);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)receiver.c_str(), 0, &lenRecv);
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)sender.c_str(), 0, &lenSend);

    SQLRETURN ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[DB] Lưu tin nhắn thất bại.\n";
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

// ===================================================
// 🔹 Lấy lịch sử chat
// ===================================================
std::vector<std::string> DatabaseHelper::getHistory(const std::string& username) {
    std::vector<std::string> history;
    if (!connected) return history;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);

    std::string sql =
        "SELECT S.Username AS Sender, R.Username AS Receiver, C.Message, C.FileName, "
        "CONVERT(VARCHAR, C.SentAt, 120) AS SentTime, C.MessageType "
        "FROM ChatHistory C "
        "JOIN Users S ON C.SenderID = S.UserID "
        "LEFT JOIN Users R ON C.ReceiverID = R.UserID "
        "WHERE S.Username = ? OR R.Username = ? "
        "ORDER BY C.SentAt ASC";

    SQLPrepareA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    SQLLEN lenUser = (SQLLEN)username.size();
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)username.c_str(), 0, &lenUser);

    SQLRETURN ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        printSQLError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return history;
    }

    SQLCHAR sender[50], receiver[50], message[2000], fileName[255], sentTime[30], msgType[20];
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, sender, sizeof(sender), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, receiver, sizeof(receiver), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, message, sizeof(message), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, fileName, sizeof(fileName), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, sentTime, sizeof(sentTime), NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, msgType, sizeof(msgType), NULL);

        std::string log = "[" + std::string((char*)sentTime) + "] "
            + std::string((char*)sender) + " → " + std::string((char*)receiver)
            + " (" + std::string((char*)msgType) + "): ";

        if (strlen((char*)fileName) > 0)
            log += "[File: " + std::string((char*)fileName) + "]";
        else
            log += std::string((char*)message);

        history.push_back(log);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return history;
}*/





