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

# Run static analysis with cppcheck
cppcheck:
    cppcheck --project=cppcheck.cfg src/

# Run static analysis with clang-tidy
clang-tidy:
    clang-tidy -p out/build/ src/*.cpp

# Run all static analyzers
static-analyzers: cppcheck clang-tidy

# Launch Tracy profiler
tracy:
    ./tools/tracy/tracy-profiler

# Install aider tool
aider-install:
    uv tool install --force --python python3.12 --with pip aider-chat[browser,help,playwright]

# Launch aider chat
aider:
    aider --model ollama_chat/devstral-small-2:24b --watch-files --dark-mode
