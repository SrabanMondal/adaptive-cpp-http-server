#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "logging.hpp"

std::mutex ctx;

void log(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    std::string formatted = oss.str();

    std::lock_guard<std::mutex> lock(ctx);
    std::cout << "[" << formatted << "] " << msg << std::endl;
}

void logerr(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    std::string formatted = oss.str();

    std::lock_guard<std::mutex> lock(ctx);
    std::cerr << "[" << formatted << "] " << msg << std::endl;
}
