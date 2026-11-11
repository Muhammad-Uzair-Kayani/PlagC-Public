#include "transport.hpp"
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace survival::client {
Json encode(const Observation &o) {
    return {{"id", o.agent},
            {"features", o.features},
            {"valid_actions", o.valid},
            {"presence", o.presence}};
}
struct PythonTransport::Impl {
    int timeout;
    std::string buffer;
    bool failed{};
#ifdef _WIN32
    HANDLE input{}, output{}, process{};
#else
    int input{-1}, output{-1};
    pid_t pid{-1};
#endif
    Impl(const std::string &exe, const std::string &script, int ms) : timeout(ms) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE childIn, childOut;
        if (!CreatePipe(&childIn, &input, &sa, 0) || !CreatePipe(&output, &childOut, &sa, 0))
            throw std::runtime_error("CreatePipe failed");
        SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(output, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = childIn;
        si.hStdOutput = childOut;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION pi{};
        std::string cmd = "\"" + exe + "\" -u \"" + script + "\"";
        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            nullptr, &si, &pi))
            throw std::runtime_error("Python process launch failed");
        CloseHandle(childIn);
        CloseHandle(childOut);
        CloseHandle(pi.hThread);
        process = pi.hProcess;
#else
        signal(SIGPIPE, SIG_IGN);
        int to[2], from[2];
        if (pipe(to) || pipe(from))
            throw std::runtime_error("pipe failed");
        pid = fork();
        if (pid < 0)
            throw std::runtime_error("fork failed");
        if (pid == 0) {
            dup2(to[0], STDIN_FILENO);
            dup2(from[1], STDOUT_FILENO);
            close(to[0]);
            close(to[1]);
            close(from[0]);
            close(from[1]);
            execlp(exe.c_str(), exe.c_str(), "-u", script.c_str(), nullptr);
            _exit(127);
        }
        close(to[0]);
        close(from[1]);
        input = to[1];
        output = from[0];
#endif
    }
    ~Impl() {
#ifdef _WIN32
        CloseHandle(input);
        CloseHandle(output);
        if (WaitForSingleObject(process, 200) != WAIT_OBJECT_0)
            TerminateProcess(process, 1);
        CloseHandle(process);
#else
        if (input >= 0)
            close(input);
        if (output >= 0)
            close(output);
        if (pid > 0) {
            int status;
            if (waitpid(pid, &status, WNOHANG) == 0) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
            }
        }
#endif
    }
    void send(const std::string &s) {
        size_t offset = 0;
        while (offset < s.size()) {
#ifdef _WIN32
            DWORD count = 0;
            if (!WriteFile(input, s.data() + offset, DWORD(s.size() - offset), &count, nullptr) ||
                !count)
                throw std::runtime_error("policy write failed");
#else
            auto count = write(input, s.data() + offset, s.size() - offset);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                throw std::runtime_error("policy write failed");
#endif
            offset += count;
        }
    }
    std::string line() {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        for (;;) {
            auto pos = buffer.find('\n');
            if (pos != std::string::npos) {
                auto s = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);
                return s;
            }
            if (buffer.size() > 1024 * 1024)
                throw std::runtime_error("policy response too large");
            if (std::chrono::steady_clock::now() >= end)
                throw std::runtime_error("policy timeout");
            char bytes[4096];
#ifdef _WIN32
            DWORD available = 0;
            if (!PeekNamedPipe(output, nullptr, 0, nullptr, &available, nullptr))
                throw std::runtime_error("policy closed output");
            if (!available) {
                Sleep(1);
                continue;
            }
            DWORD count = 0;
            if (!ReadFile(output, bytes, std::min<DWORD>(available, sizeof(bytes)), &count,
                          nullptr) ||
                !count)
                throw std::runtime_error("policy read failed");
#else
            pollfd p{output, POLLIN, 0};
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 end - std::chrono::steady_clock::now())
                                 .count();
            int ready = poll(&p, 1, int(std::max<std::int64_t>(1, remaining)));
            if (ready < 0 && errno == EINTR)
                continue;
            if (ready <= 0)
                throw std::runtime_error("policy timeout");
            auto count = read(output, bytes, sizeof(bytes));
            if (count <= 0)
                throw std::runtime_error("policy closed output");
#endif
            buffer.append(bytes, count);
        }
    }
};
PythonTransport::PythonTransport(const std::string &exe, const std::string &service, int ms)
    : impl(std::make_unique<Impl>(exe, service, ms)) {
    request({{"v", 1}, {"type", "hello"}, {"request_id", 0}, {"episode", 0}});
}
PythonTransport::~PythonTransport() = default;
Json PythonTransport::request(Json q) {
    if (impl->failed)
        throw std::runtime_error("policy transport disabled after failure");
    try {
        impl->send(q.dump() + "\n");
        auto a = Json::parse(impl->line());
        if (a.at("v") != 1 || a.at("request_id") != q.at("request_id") ||
            a.at("episode") != q.at("episode") || a.value("type", "") == "error")
            throw std::runtime_error("policy response correlation/schema/error");
        return a;
    } catch (...) {
        impl->failed = true;
        throw;
    }
}
std::vector<AgentIntent> PythonTransport::infer(const std::vector<Observation> &obs,
                                                std::uint64_t req, int episode, int tick) {
    Json agents = Json::array();
    for (auto &o : obs)
        agents.push_back(encode(o));
    auto a = request({{"v", 1},
                      {"type", "infer_batch"},
                      {"request_id", req},
                      {"episode", episode},
                      {"tick", tick},
                      {"agents", agents}});
    if (a.at("type") != "intents" || a.at("actions").size() != obs.size())
        throw std::runtime_error("invalid policy action batch");
    std::vector<AgentIntent> result;
    std::set<Id> seen;
    for (auto &action : a.at("actions")) {
        auto readId = [](const Json &value) -> Id {
            if (!value.is_number_integer())
                throw std::runtime_error("non-integer ID");
            auto n = value.get<std::int64_t>();
            if (n < 0 || n > std::numeric_limits<Id>::max())
                throw std::runtime_error("ID out of range");
            return static_cast<Id>(n);
        };
        Id id = readId(action.at("id"));
        auto it = std::find_if(obs.begin(), obs.end(), [&](auto &o) { return o.agent == id; });
        if (it == obs.end() || !seen.insert(id).second)
            throw std::runtime_error("unknown/duplicate agent");
        Intent intent = parseIntent(action.at("intent"));
        if (!it->valid[static_cast<int>(intent)])
            throw std::runtime_error("masked action");
        Id target = readId(action.at("target_id"));
        if (target &&
            std::find(it->visibleActors.begin(), it->visibleActors.end(), target) ==
                it->visibleActors.end() &&
            std::find(it->visibleObjects.begin(), it->visibleObjects.end(), target) ==
                it->visibleObjects.end())
            throw std::runtime_error("unobserved target");
        result.push_back({id, intent, target});
    }
    return result;
}
} // namespace survival::client
