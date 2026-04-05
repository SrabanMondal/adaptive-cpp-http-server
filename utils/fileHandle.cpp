#include "filehandle.hpp"
#include "logging.hpp"
using namespace std;

bool FileHandler::preloadFile(const string& fileName, bool binary) {
    string fullPath = basePath + fileName;

    if (binary) {
        ifstream file(fullPath, ios::binary);
        if (!file.is_open()) {
            logerr("Preload failed (binary): " + fileName);
            return false;
        }
        binaryCache[fileName] = vector<char>(
            istreambuf_iterator<char>(file),
            istreambuf_iterator<char>()
        );
    } else {
        ifstream file(fullPath);
        if (!file.is_open()) {
            logerr("Preload failed (text): " + fileName);
            return false;
        }
        stringstream buffer;
        buffer << file.rdbuf();
        textCache[fileName] = buffer.str();
    }
    return true;
}

void FileHandler::preloadFiles(const vector<string>& fileNames,
                                const vector<bool>& binaryFlags) {
    size_t loaded = 0;
    for (size_t i = 0; i < fileNames.size(); ++i) {
        bool binary = (i < binaryFlags.size()) ? binaryFlags[i] : false;
        if (preloadFile(fileNames[i], binary)) {
            ++loaded;
        }
    }
    log("Preloaded " + to_string(loaded) + "/" + to_string(fileNames.size()) + " static files into memory");
}

string FileHandler::readTextFile(const string& fileName) {
    auto it = textCache.find(fileName);
    if (it != textCache.end()) {
        return it->second;
    }

    logerr("Cache miss, reading from disk: " + fileName);
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
    auto it = binaryCache.find(fileName);
    if (it != binaryCache.end()) {
        return it->second;  // return cached copy
    }

    logerr("Cache miss, reading from disk: " + fileName);
    ifstream file(basePath + fileName, ios::binary);
    if (!file.is_open()) {
        logerr("Error opening file: " + fileName);
        return {};
    }
    return vector<char>(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}