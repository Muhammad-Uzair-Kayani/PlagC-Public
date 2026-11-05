#pragma once
#include <filesystem>

namespace PlagC {
// OS-resolved executable location; independent of argv[0] and the caller's cwd.
std::filesystem::path ExecutableDirectory();
// Graphical executable bootstrap only, before constructing Engine or starting jobs.
// Anchors legacy relative renderer/editor paths to the verified staged package.
// Does not affect library clients or headless test harnesses.
void EnterRuntimePackage();
}
