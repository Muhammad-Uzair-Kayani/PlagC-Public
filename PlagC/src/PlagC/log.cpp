#include "log.h"

std::shared_ptr<spdlog::logger> PlagC::log::s_CoreLogger;
std::shared_ptr<spdlog::logger> PlagC::log::s_ClientLogger;

PlagC::log::log()
{
	// Constructor implementation (if needed)
}

PlagC::log::~log()
{
	// Destructor implementation (if needed)
}

void PlagC::log::Init()
{	
	spdlog::set_pattern("%^[%T] %n: %v%$");

	s_CoreLogger = spdlog::stdout_color_mt("PLAGC");
	s_CoreLogger->set_level(spdlog::level::trace);

	s_ClientLogger = spdlog::stdout_color_mt("APP");
	s_ClientLogger->set_level(spdlog::level::trace);
}
