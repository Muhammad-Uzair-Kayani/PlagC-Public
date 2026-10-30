#include "pgcpch.h"
#include "EditorRestart.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <string_view>
#endif

namespace PlagC::Editor {

#ifdef _WIN32
namespace {

std::wstring QuoteArgument(std::wstring_view argument)
{
    if (argument.empty())
        return L"\"\"";
    if (argument.find_first_of(L" \t\"") == std::wstring_view::npos)
        return std::wstring(argument);

    std::wstring result;
    result.push_back(L'\"');
    std::size_t backslashes = 0;
    for (const wchar_t ch : argument)
    {
        if (ch == L'\\')
        {
            ++backslashes;
            continue;
        }
        if (ch == L'\"')
        {
            result.append(backslashes * 2u + 1u, L'\\');
            result.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2u, L'\\');
    result.push_back(L'\"');
    return result;
}

bool IsRendererOverride(std::wstring_view argument)
{
    return argument == L"--renderer" || argument.starts_with(L"--renderer=");
}

} // namespace
#endif

bool RestartEditorProcess(std::string& error) noexcept
{
#ifdef _WIN32
    try
    {
        std::wstring executable(32768u, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                                static_cast<DWORD>(executable.size()));
        if (length == 0u || length >= executable.size())
        {
            error = "Unable to resolve the Editor executable path (Win32 error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
        executable.resize(length);

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv)
        {
            error = "Unable to parse the Editor command line (Win32 error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }

        std::vector<std::wstring> arguments;
        arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 1u);
        arguments.push_back(executable);
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring_view argument(argv[i]);
            if (argument == L"--renderer")
            {
                if (i + 1 < argc)
                    ++i;
                continue;
            }
            if (IsRendererOverride(argument))
                continue;
            arguments.emplace_back(argument);
        }
        LocalFree(argv);

        std::wstring commandLine;
        for (const auto& argument : arguments)
        {
            if (!commandLine.empty())
                commandLine.push_back(L' ');
            commandLine += QuoteArgument(argument);
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                            0u, nullptr, nullptr, &startup, &process))
        {
            error = "Failed to restart the Editor (Win32 error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
    catch (...)
    {
        error = "Unknown error while restarting the Editor";
        return false;
    }
#else
    error = "Restart Now is currently implemented for Windows only";
    return false;
#endif
}

} // namespace PlagC::Editor
