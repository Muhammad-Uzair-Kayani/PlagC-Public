#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace PlagC {
enum class Severity { Trace, Debug, Info, Warning, Error, Critical };
struct LogRecord { std::uint64_t sequence; Severity severity; std::string timestamp, category, thread, threadID, message; };
class Log {
public:
    Log();
    ~Log();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    void Write(Severity, std::string_view category, std::string_view message);
    std::vector<LogRecord> Snapshot() const;
    void Clear();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
