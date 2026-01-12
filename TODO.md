# TODO List

## 🚀 **Development & Configuration**

### 🔧 **Build System & Tools**
- [ ] Add support for **CLANG-TIDY**
- [ ] Add support for **CPPCHECK**
- [ ] Add **testing framework**
- [ ] Add **coverage reporting**
- [x] Enable **ASAN (Address Sanitizer)**

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

### 🛠 **IDE & Tooling**
- [ ] Switch to **clangd** for better IntelliSense
- [ ] Clean up `compile_commands.json` (remove old entries)

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
- [ ] Refactor **shader pipeline** for better modularity
- [ ] Optimize **Vulkan resource management**

### 📦 **Dependency Management**
- [ ] Update **vcpkg** configuration
- [ ] Optimize **CMake dependency handling**