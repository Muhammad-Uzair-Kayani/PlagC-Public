#include "pgcpch.h"
#include <PlagC/RuntimePaths.h>
#include <stdexcept>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace PlagC {
std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const DWORD size=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if (!size || size>=buffer.size())
        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Resolve executable path");
    return std::filesystem::path(std::wstring(buffer.data(),size)).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size=0;
    _NSGetExecutablePath(nullptr,&size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(),&size)!=0)
        throw std::runtime_error("Unable to resolve executable path");
    return std::filesystem::canonical(buffer.data()).parent_path();
#elif defined(__linux__)
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#else
    throw std::runtime_error("Executable path resolution is unsupported on this platform");
#endif
}

void EnterRuntimePackage() {
    const auto root=ExecutableDirectory();
    for (const auto* relative : {"Assets", "Vendor/ogre-next/Samples/Media/Hlms"}) {
        const auto required=root/relative;
        if (!std::filesystem::is_directory(required))
            throw std::runtime_error("Incomplete runtime package: missing " + required.string() +
                ". Rebuild the application to stage its runtime assets.");
    }
    // One deliberate startup change before any worker, logger, or renderer exists.
    std::filesystem::current_path(root);
}
}
