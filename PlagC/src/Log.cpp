#include "pgcpch.h"
#include <PlagC/Log.h>
#include <PlagC/Thread.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
namespace PlagC {
struct Log::Impl {
    mutable std::mutex mutex;
    std::deque<LogRecord> records;
    std::uint64_t sequence = 0;
    std::shared_ptr<spdlog::logger> backend;
};
Log::Log() : impl_(std::make_unique<Impl>()) {
    std::filesystem::create_directories("Logs");
    std::vector<spdlog::sink_ptr> sinks;
#if !defined(_WIN32) || !defined(PC_DIST)
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>("Logs/PlagC.log", 4*1024*1024, 3));
    impl_->backend = std::make_shared<spdlog::logger>("PlagC", sinks.begin(), sinks.end());
    impl_->backend->set_level(spdlog::level::trace);
    impl_->backend->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    impl_->backend->flush_on(spdlog::level::err);
}
Log::~Log() { impl_->backend->flush(); }
void Log::Write(Severity severity, std::string_view category, std::string_view message) {
#ifdef PC_DIST
    if (severity < Severity::Info) return;
#elif defined(NDEBUG)
    if (severity < Severity::Debug) return;
#endif
    static constexpr spdlog::level::level_enum levels[] = {spdlog::level::trace,spdlog::level::debug,spdlog::level::info,spdlog::level::warn,spdlog::level::err,spdlog::level::critical};
    std::ostringstream id; id << Thread::CurrentID();
    auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    LogRecord record{0,severity,std::to_string(stamp),std::string(category),Thread::CurrentName(),id.str(),std::string(message)};
    std::lock_guard lock(impl_->mutex);
    record.sequence = ++impl_->sequence;
    impl_->backend->log(levels[static_cast<unsigned>(severity)], "[{}] [{}:{}] {}",record.category,record.thread,record.threadID,record.message);
    if (impl_->records.size() == 4096) impl_->records.pop_front();
    impl_->records.push_back(std::move(record));
}
std::vector<LogRecord> Log::Snapshot() const { std::lock_guard lock(impl_->mutex); return {impl_->records.begin(),impl_->records.end()}; }
void Log::Clear() { std::lock_guard lock(impl_->mutex); impl_->records.clear(); }
}
