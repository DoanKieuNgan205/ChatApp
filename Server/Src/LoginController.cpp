#include "LoginController.h"
#include "Auth.h"

string LoginController::handleLogin(const string& request) {
    if (Auth::checkCredentials(request)) {
        return "LOGIN_SUCCESS";
    } else {
        return "LOGIN_FAIL";
    }
}








