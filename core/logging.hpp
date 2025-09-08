#pragma once
#include <mutex>
#include <string>

extern std::mutex ctx;

void log(const std::string& msg);
void logerr(const std::string& msg);
