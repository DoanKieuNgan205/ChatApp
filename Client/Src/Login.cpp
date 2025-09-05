#include "Login.h"
#include <iostream>
using namespace std;

string showLoginForm(SOCKET sock) {
    string username, password;
    cout << "Nhap username: ";
    cin >> username;
    cout << "Nhap password: ";
    cin >> password;

    // Chuẩn bị request
    string request = "{ \"username\": \"" + username + "\", \"password\": \"" + password + "\" }";
    send(sock, request.c_str(), request.size(), 0);

    char buf[256];
    int bytes = recv(sock, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) return "";
    buf[bytes] = '\0';

    string response(buf);
    if (response == "LOGIN_SUCCESS") {
        cout << "Dang nhap thanh cong!\n";
        return username;
    } else {
        cout << "Dang nhap that bai!\n";
        return "";
    }
}












