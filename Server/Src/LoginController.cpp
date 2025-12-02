#include "LoginController.h"
#include "Auth.h"
#include <string>
extern string parseField(const string& json, const string& field);
using namespace std;

/*string LoginController::handleLogin(const string& request) {
    if (Auth::checkCredentials(request)) {
        return R"({ "action": "login_response", "status": "success", "message": "LOGIN_SUCCESS" })";
    } else {
        return R"({ "action": "login_response", "status": "fail", "message": "LOGIN_FAIL" })";
    }
}*/

string LoginController::handleLogin(const string& request) {
    string username = parseField(request, "username"); 

    if (Auth::checkCredentials(request)) {
        string successResponse = R"({ "action": "login_response", "status": "success", "username": ")";
        successResponse += username;
        successResponse += R"(" , "message": "LOGIN_SUCCESS" })";
        
        return successResponse; 
    } else {
        return R"({ "action": "login_response", "status": "fail", "message": "LOGIN_FAIL" })";
    }
}
