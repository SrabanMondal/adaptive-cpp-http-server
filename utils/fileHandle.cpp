#include "filehandle.hpp"
#include "logging.hpp"
using namespace std;

string FileHandler::readTextFile(const string& fileName) {
    ifstream file(basePath + fileName);
    if (!file.is_open()) {
        logerr("Error opening file: " + fileName);
        return "";
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

vector<char> FileHandler::readBinaryFile(const string& fileName) {
    ifstream file(basePath + fileName, ios::binary);
    if (!file.is_open()) {
        logerr("Error opening file: " + fileName);
        return {};
    }
    return vector<char>((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}
