#pragma once

#include <string>

namespace PlagC::Editor {

// Spawns a fresh Editor process using the current command line minus renderer overrides.
// The caller stops the current Engine only after this succeeds.
bool RestartEditorProcess(std::string& error) noexcept;

} // namespace PlagC::Editor
