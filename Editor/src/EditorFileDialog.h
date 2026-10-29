#pragma once

#include <PlagC/Window.h>
#include <filesystem>
#include <optional>

namespace PlagC::Editor {

std::optional<std::filesystem::path> OpenSceneFileDialog(NativeWindow owner);
std::optional<std::filesystem::path> SaveSceneFileDialog(NativeWindow owner);
bool NativeSceneFileDialogsAvailable(NativeWindow owner) noexcept;

} // namespace PlagC::Editor
