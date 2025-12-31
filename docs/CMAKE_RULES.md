# 🧠 CMake Cheat Sheet — Presets vs CMakeLists

---

## 🧱 Golden Rule

> **CMakeLists.txt = what the project IS**  
> **CMakePresets.json = how you BUILD it**

---

## 🟦 CMakeLists.txt  
### 👉 Describes the project (portable, versioned, stable)

**Use here for:**
- Project structure
  - `add_library()`, `add_executable()`
  - `target_link_libraries()`
  - `target_include_directories()`
- Compilation rules
  - Warnings (`-Wall`, `/W4`)
  - C++ standard (`cxx_std_20`)
  - Target-specific compile options
  - Compiler-specific conditions (`if(MSVC)`, `if(CLANG)`)
- Technical constraints
  - API choices (Vulkan, Physics, Rendering)
  - Library options
- Must be identical across all platforms and build types

**Do NOT put here:**
- Generator choice (Ninja, Visual Studio)
- Local paths
- Build type (Debug/Release)
- Compiler-specific flags
- Sanitizers enabled globally

---

## 🟨 CMakePresets.json  
### 👉 Describes the build environment and context

**Use here for:**
- Environment
  - Compiler (`clang`, `clang-cl`, `cl.exe`)
  - Generator (`Ninja`, `Visual Studio`)
  - Toolchain file
  - vcpkg triplet
  - Local cache paths
- Build variants
  - Debug / Release / RelWithDebInfo / MinSizeRel
  - ASan / UBSan / TSan
  - Profiling builds
- Machine/OS specific settings
  - Windows / Linux / macOS
  - MSVC vs clang
- Temporary or experimental flags

**Do NOT put here:**
- `add_library` / `add_executable`
- Target-specific options
- Project logic
- API choices or backend selection

---

## 🧩 Quick Decision Table

| Question                            | Preset | CMakeLists |
| ----------------------------------- | ------ | ---------- |
| Does this define project structure? | ❌      | ✅          |
| Which compiler to use?              | ✅      | ❌          |
| On which machine?                   | ✅      | ❌          |
| Does a specific lib need a flag?    | ❌      | ✅          |
| Debug or Release build?             | ✅      | ❌          |
| Sanitizer enabled?                  | ✅      | ❌          |
| Engine warnings?                    | ❌      | ✅          |
| Temporary flag for testing?         | ✅      | ❌          |
| Flag part of engine rules?          | ❌      | ✅          |

---

## 🧪 Practical Examples

**Sanitizer** — Preset  
```json
"CMAKE_CXX_FLAGS": "-fsanitize=address"
