set windows-shell := ["powershell", "-c"]

ext := if os_family() == "windows" { "ps1" } else { "sh" }
remove := if os_family() == "windows" { "rm -Force" } else { "rm -fr" }

# Install dependencies
install:
    ./tools/dev/bootstrap.{{ext}}

# Clean CMake project configuration
clean-out:
    {{remove}} out/

# Clean Clangd cache
clean-clangd:
    {{remove}} .cache/
    {{remove}} compile_commands.json

# Clean all build, cache and artifact files
clean-all: clean-out clean-clangd

cppcheck:
    cppcheck --project=cppcheck.cfg src/

clang-tidy:
    clang-tidy -p out/build/ src/*.cpp

static-analyzers: cppcheck clang-tidy

tracy:
    ./tools/tracy/tracy-profiler

aider:
    aider --model ollama_chat/devstral-small-2:24b --watch-files --dark-mode