#include<fstream>
#include<string>
#include<iostream>
#include<sstream>
#include<vector>
#include<unordered_map>
#pragma once

class FileHandler{
    std::string basePath;

    std::unordered_map<std::string, std::string> textCache;
    std::unordered_map<std::string, std::vector<char>> binaryCache;

public:
    explicit FileHandler(const std::string& bp="./static/"): basePath(bp){}

    bool preloadFile(const std::string& fileName, bool binary = false);

    void preloadFiles(const std::vector<std::string>& fileNames,
                      const std::vector<bool>& binaryFlags = {});

    std::string readTextFile(const std::string& fileName);
    std::vector<char> readBinaryFile(const std::string& fileName);
    
    void clear() {
        textCache.clear();
        binaryCache.clear();
    }
};
