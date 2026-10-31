#include "pgcpch.h"

#if defined(_WIN32) && defined(PC_DIST)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

namespace
{
class FatalCaptureBuffer final : public std::streambuf
{
public:
    explicit FatalCaptureBuffer(std::streambuf* upstream) noexcept
        : upstream_(upstream)
    {
    }

    const std::string& Captured() const noexcept { return captured_; }

protected:
    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character, traits_type::eof()))
            return traits_type::not_eof(character);

        const char value = traits_type::to_char_type(character);
        captured_.push_back(value);
        if (upstream_)
            upstream_->sputc(value);
        return character;
    }

    std::streamsize xsputn(const char* text, std::streamsize count) override
    {
        if (count <= 0)
            return 0;

        captured_.append(text, static_cast<std::size_t>(count));
        if (upstream_)
            upstream_->sputn(text, count);
        return count;
    }

    int sync() override
    {
        return upstream_ ? upstream_->pubsync() : 0;
    }

private:
    std::streambuf* upstream_ = nullptr;
    std::string captured_;
};

class DistFatalDiagnostics final
{
public:
    DistFatalDiagnostics()
        : original_(std::cerr.rdbuf()), capture_(original_)
    {
        std::cerr.rdbuf(&capture_);
    }

    ~DistFatalDiagnostics()
    {
        std::cerr.rdbuf(original_);

        const std::string& message = capture_.Captured();
        if (message.find("PlagC fatal error:") == std::string::npos)
            return;

        try
        {
            std::filesystem::create_directories("Logs");
            std::ofstream output("Logs/PlagC-fatal.log", std::ios::app);
            if (output)
                output << message;
        }
        catch (...)
        {
        }

        MessageBoxA(nullptr,
                    message.c_str(),
                    "PlagC Editor - Fatal Error",
                    MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }

private:
    std::streambuf* original_ = nullptr;
    FatalCaptureBuffer capture_;
};

DistFatalDiagnostics g_distFatalDiagnostics;
} // namespace

#endif
