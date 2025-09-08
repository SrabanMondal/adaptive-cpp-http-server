#include<fstream>
#include<string>
#include<iostream>
#include <sstream>
#include<vector>
#pragma once
class FileHandler{
    std::string basePath;
    public:
        explicit FileHandler(const std::string& bp="./static/"): basePath(bp){}
        std::string readTextFile(const std::string& fileName);
        std::vector<char> readBinaryFile(const std::string& fileName);
};