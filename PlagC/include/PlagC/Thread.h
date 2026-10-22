#pragma once
#include <string>
#include <utility>
#include <thread>
#include <stdexcept>
namespace PlagC::Thread {
inline thread_local std::string name = "External";
inline thread_local bool worker = false;
inline std::thread::id mainID;
inline void Register(std::string value, bool isWorker = false) { name = std::move(value); worker = isWorker; }
// Called once at bootstrap, before workers start.
inline void InitializeMain() { mainID = std::this_thread::get_id(); Register("Main"); }
inline bool IsMain() { return std::this_thread::get_id() == mainID; }
inline bool IsWorker() { return worker; }
inline auto CurrentID() { return std::this_thread::get_id(); }
inline const std::string& CurrentName() { return name; }
inline void RequireMain() { if (!IsMain()) throw std::logic_error("main-thread operation"); }
}
