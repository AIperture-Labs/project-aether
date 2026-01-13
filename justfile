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

# AI! make a comment as help message
cppcheck:
    cppcheck --project=cppcheck.cfg src/

# AI! make a comment as help message
clang-tidy:
    clang-tidy -p out/build/ src/*.cpp

# AI! make a comment as help message
static-analyzers: cppcheck clang-tidy

# AI! make a comment as help message
tracy:
    ./tools/tracy/tracy-profiler

# AI! make a comment as help message
aider-install:
    uv tool install --force --python python3.12 --with pip aider-chat[browser,help]

# AI! make a comment as help message
aider:
    aider --model ollama_chat/devstral-small-2:24b --watch-files --dark-mode