# TODO List

## 🚀 **Development & Configuration**

### 🔧 **Build System & Tools**
- [ ] Add support for **CLANG-TIDY**
- [ ] Add support for **CPPCHECK**
- [ ] Add **testing framework**
- [ ] Add **coverage reporting**
- [x] Enable **ASAN (Address Sanitizer)**
 - [ ] Introduce project-level export macro **AETHER_DECLSPEC** (replace `JPEG_API`) and update headers/CMake
- [ ] Add git submodule switch for **thirdparty** (CMake option `GIT_SUBMODULE_THIRDPARTY`)
- [ ] Add **IWYU (Include What You Use)** as a thirdparty tool

### 📝 **Code Formatting & Linting**
- [ ] Fix **clang-format** configuration for Slang language files (`.slang`)
- [ ] Configure **clang-tidy** for project-wide linting

### 🔄 **Build System Improvements**
- [ ] Add **static/shared build** option
- [ ] Configure **SDL3** with `BUILD_SHARED_LIBS` option

### 📜 **Language & Compiler Updates**
- [ ] Enable **C++20 modules**
- [ ] Update **C++ version to C++23**
- [ ] Update **C version to C23**
 - [ ] Unifier les `cxx_std_20` soit en `compile_features` soit en `properties` (CMake)

### 🛠 **IDE & Tooling**
- [ ] Switch to **clangd** for better IntelliSense
- [ ] Clean up `compile_commands.json` (remove old entries)
- [ ] Review placement of installation configs like `.vsconfig`

### 📁 **Project Structure**
- [ ] Move all settings to `configs/` folder
- [ ] Create subfolders (e.g., `dev/` for `.vscode`)

## 📚 **Documentation**

### 📝 **Content**
- [ ] Write **documentation** and blog post about dev environment stack
- [ ] Document **build system** (CMake, presets, tooling)
- [ ] Document **Slang shader** workflow

### 📖 **Resources**
- [ ] Organize **external resources** in `docs/RESSOURCES.md`

## 🧪 **Testing**

### 🔍 **Test Framework**
- [ ] Set up **unit testing** (Google Test, Catch2, etc.)
- [ ] Set up **integration testing**

### 📊 **Coverage & Analysis**
- [ ] Configure **code coverage** tools (gcov, llvm-cov)

## 🔄 **Refactoring**

### 🔄 **Codebase Improvements**
 - [ ] Add support for direct memory access from (re)BAR/SAM/UMA instead of staging buffers. Staging is now largely obsolete and direct access can be used in various cases.
 - [ ] Improve error handling throughout the codebase

### 📦 **Dependency Management**
- [ ] Optimize **CMake dependency handling**