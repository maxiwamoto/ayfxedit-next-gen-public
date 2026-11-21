#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#endif
namespace ayfx {
inline std::string temporarySaveSuffix() {
    static std::atomic<unsigned long long> serial{0};
#ifdef _WIN32
    auto process = GetCurrentProcessId();
#else
    auto process = getpid();
#endif
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return ".tmp." + std::to_string(process) + "." + std::to_string(stamp) + "." + std::to_string(serial++);
}
inline bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(from, to, error);
    return !error;
#endif
}
}
