# Verification — 2026-10-21

Linux, GCC, Premake 5.0.0-beta2. GLFW 3.4, spdlog 1.14.1.
All code compiled through the Premake-generated projects; no CMake used.

| Check | Result |
|---|---|
| Debug, 4 workers, GLFW Null | 23 checks passed, exit 0 |
| Debug, 1 worker, headless | 20 checks passed, exit 0 |
| Release, 4 workers, GLFW Null | 23 checks passed, exit 0 |
| Release, 1 worker, headless | 20 checks passed, exit 0 |
| Dist, 4 workers, GLFW Null | 23 checks passed, exit 0 |
| Dist, 1 worker, headless | 20 checks passed, exit 0 |
| ThreadSanitizer, Debug, 4 workers, Null | 23 checks passed, exit 0, no race reports |
| AddressSanitizer + UBSan, Debug, 4 workers, Null | 23 checks passed, exit 0, no reports; leak detection disabled |
| LeakSanitizer | Environment cannot inspect process tasks; not verified |
| Fresh local clone bootstrap | Both pinned submodules downloaded and Premake projects generated successfully |
| VS2022 project generation | Passed; C++20, Windows vendor sources and system libraries inspected |
| macOS makefile generation | Passed; execution/build not verified |
| Native Windows/VS2022 build and input | Pending on Windows host |
| Native Linux X11 / macOS execution | Pending |

Logs from final successful runs are in Evidence/. Two intentionally throwing
jobs per run validate failure accounting, so failed=2 is expected. A single
worker naturally has no stealing check; headless mode omits two window checks.

Sanitizer configurations have separate output directories to prevent mixing
instrumented and ordinary libraries. Full native-input verification and Windows
acceptance are required before closing Phase 1; see Phase1.md.
