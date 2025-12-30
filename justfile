set windows-shell := ["powershell", "-c"]

ext := if os_family() == "windows" { "ps1" } else { "sh" }

# Install dependencies
install:
    ./scripts/install.{{ext}}