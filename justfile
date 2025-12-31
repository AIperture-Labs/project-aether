set windows-shell := ["powershell", "-c"]

ext := if os_family() == "windows" { "ps1" } else { "sh" }
remove := if os_family() == "windows" { "rm -Force" } else { "rm -fr" }

# Install dependencies
install:
    ./tools/dev/bootstrap.{{ext}}

# Clean CMake project configuration
clean-out:
    {{remove}} out/