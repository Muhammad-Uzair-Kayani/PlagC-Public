#pragma once
//Headers for vendor library
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//Headers for PlagC Cores
#include "PlagC/Core.h"

namespace PlagC
{

	class PC_API log
	{
	public:
		log();
		~log();
		static void Init();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}



#define PC_CORE_TRACE(...)		::PlagC::log::GetCoreLogger()->trace(__VA_ARGS__)
#define PC_CORE_INFO(...)		::PlagC::log::GetCoreLogger()->info(__VA_ARGS__)
#define PC_CORE_WARN(...)		::PlagC::log::GetCoreLogger()->warn(__VA_ARGS__)
#define PC_CORE_ERROR(...)		::PlagC::log::GetCoreLogger()->error(__VA_ARGS__)
#define PC_CORE_FATAL(...)      ::PlagC::log::GetCoreLogger()->critical(__VA_ARGS__)

#define PC_TRACE(...)			::PlagC::log::GetClientLogger()->trace(__VA_ARGS__)
#define PC_INFO(...)			::PlagC::log::GetClientLogger()->info(__VA_ARGS__)
#define PC_WARN(...)			::PlagC::log::GetClientLogger()->warn(__VA_ARGS__)
#define PC_ERROR(...)			::PlagC::log::GetClientLogger()->error(__VA_ARGS__)
#define PC_FATAL(...)      ::PlagC::log::GetCoreLogger()->critical(__VA_ARGS__)