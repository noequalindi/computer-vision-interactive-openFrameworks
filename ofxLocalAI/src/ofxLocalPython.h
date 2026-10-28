#pragma once
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

// C++17 returns string; C++20+ returns u8string. Keep the public argv UTF-8.
inline std::string ofxLocalPathUtf8(const std::filesystem::path& path) {
    auto bytes=path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}

// Owns one Python child. No shell, no terminal, no global process-name killing.
// Call start/update/stop on the OF main thread. stop() is non-blocking.
class ofxLocalPython {
public:
    ofxLocalPython()=default;
    ~ofxLocalPython();
    ofxLocalPython(const ofxLocalPython&)=delete;
    ofxLocalPython& operator=(const ofxLocalPython&)=delete;
    bool start(const std::filesystem::path& workspace,const std::filesystem::path& python,
               const std::vector<std::string>& arguments);
    void update();
    void stop();
    void close(); // Graceful shutdown, then bounded forced termination of own child.
    bool running() const { return pid!=0; }
    bool stopping() const { return stopRequested; }
    int exitCode() const { return code; }
    unsigned long processId() const { return pid; }
    const std::string& error() const { return failure; }
    std::string logTail() const;
    std::filesystem::path logPath() const { return logFile; }
private:
    unsigned long pid=0;
    void* handle=nullptr; // Windows process handle; unused on POSIX.
    int code=0;
    bool stopRequested=false;
    std::filesystem::path logFile,stopFile;
    std::string failure;
    std::chrono::steady_clock::time_point stopAt;
    void forceStop();
};
