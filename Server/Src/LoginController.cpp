#include "LoginController.h"
#include "Auth.h"
#include <string>
using namespace std;

string LoginController::handleLogin(const string& request) {
    if (Auth::checkCredentials(request)) {
        return R"({ "action": "login_response", "status": "success", "message": "LOGIN_SUCCESS" })";
    } else {
        return R"({ "action": "login_response", "status": "fail", "message": "LOGIN_FAIL" })";
    }
}

/*#include "LoginController.h"
#include "Auth.h"
#include <iostream>
using namespace std;

string LoginController::handleLogin(const string& request) {
    cout << "[LoginController] Nhận yêu cầu login: " << request << endl;

    bool result = Auth::checkCredentials(request);

    if (result) {
        return R"({"action":"login_response","status":"success","message":"LOGIN_SUCCESS"})";
    } else {
        return R"({"action":"login_response","status":"fail","message":"LOGIN_FAIL"})";
    }
}

string LoginController::handleRegister(const string& request) {
    cout << "[LoginController] Nhận yêu cầu đăng ký: " << request << endl;

    bool result = Auth::registerAccount(request);

    if (result) {
        return R"({"action":"register_response","status":"success","message":"REGISTER_SUCCESS"})";
    } else {
        return R"({"action":"register_response","status":"fail","message":"REGISTER_FAIL"})";
    }
}*/

