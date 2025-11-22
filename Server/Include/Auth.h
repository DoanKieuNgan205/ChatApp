#pragma once
#include "Library.h"
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
using namespace std;

class Auth {
public:
    static bool checkCredentials(const string& request);
};













