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

