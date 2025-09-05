#include "Auth.h"


static string trim(const string &s) {
    string result = s;
    result.erase(result.begin(), find_if(result.begin(), result.end(),
        [](unsigned char ch){ return !isspace(ch); }));
    result.erase(find_if(result.rbegin(), result.rend(),
        [](unsigned char ch){ return !isspace(ch); }).base(), result.end());
    return result;
}

bool Auth::checkCredentials(const string& request) {
    // parse JSON thủ công
    auto extractValue = [&](const string& key) {
        size_t pos = request.find(key);
        if (pos == string::npos) return string("");
        pos = request.find(":", pos);
        if (pos == string::npos) return string("");
        size_t start = request.find("\"", pos);
        size_t end   = request.find("\"", start+1);
        if (start == string::npos || end == string::npos) return string("");
        return request.substr(start+1, end-start-1);
    };

    string username = extractValue("\"username\"");
    string password = extractValue("\"password\"");

    cout << "[DEBUG] Username: " << username << ", Password: " << password << endl;

    // đọc file users.txt
    ifstream file("users.txt");
    if (!file.is_open()) {
        cerr << "Không mở được file users.txt\n";
        return false;
    }

    string user, pass;
    while (file >> user >> pass) {
        // xử lý \r nếu có
        if (!pass.empty() && pass.back() == '\r') {
            pass.pop_back();
        }
        if (user == username && pass == password) {
            return true;
        }
    }
    return false;
}










