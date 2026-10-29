#include "pgcpch.h"
#include "EditorFileDialog.h"
#include <array>
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <commdlg.h>
#if defined(_MSC_VER)
#pragma comment(lib, "Comdlg32.lib")
#endif
#endif

namespace PlagC::Editor {
namespace {

#if defined(_WIN32)
HWND OwnerWindow(NativeWindow owner)
{
    if (owner.kind != NativeWindow::Kind::Win32 || owner.handle == 0)
        return nullptr;
    return reinterpret_cast<HWND>(owner.handle);
}

std::optional<std::filesystem::path> ShowDialog(NativeWindow owner, bool save)
{
    std::array<wchar_t, 32768> buffer{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = OwnerWindow(owner);
    dialog.lpstrFilter =
        L"PlagC Scene (*.plscene)\0*.plscene\0"
        L"JSON Scene (*.json)\0*.json\0"
        L"All Files (*.*)\0*.*\0\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"plscene";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (accepted)
        return std::filesystem::path(buffer.data());

    const DWORD error = CommDlgExtendedError();
    if (error != 0)
        throw std::runtime_error("Windows scene file dialog failed with code " +
                                 std::to_string(static_cast<unsigned long>(error)));
    return std::nullopt;
}
#endif

} // namespace

bool NativeSceneFileDialogsAvailable(NativeWindow owner) noexcept
{
#if defined(_WIN32)
    return owner.kind == NativeWindow::Kind::Win32 && owner.handle != 0;
#else
    (void)owner;
    return false;
#endif
}

std::optional<std::filesystem::path> OpenSceneFileDialog(NativeWindow owner)
{
#if defined(_WIN32)
    return ShowDialog(owner, false);
#else
    (void)owner;
    return std::nullopt;
#endif
}

std::optional<std::filesystem::path> SaveSceneFileDialog(NativeWindow owner)
{
#if defined(_WIN32)
    return ShowDialog(owner, true);
#else
    (void)owner;
    return std::nullopt;
#endif
}

} // namespace PlagC::Editor
