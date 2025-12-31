# Constantes
$REPO_PATH = $(Split-Path -Parent $(Split-Path -Parent $MyInvocation.MyCommand.Definition))
$DEPS = @(
    "Git.Git",
    "Microsoft.VisualStudioCode",
    "Microsoft.VisualStudio.2022.BuildTools",
    "KhronosGroup.VulkanSDK"
    # "BaldurKarlsson.RenderDoc",
    # "Cppcheck.Cppcheck"
)

# Functions
function Build-CmakeFormat {
    docker build -f $REPO_PATH\docker\cmake-format.Dockerfile . -t cmake-format:latest
}

function Install-MicrosoftBuildTools {
    winget install Microsoft.VisualStudio.2022.BuildTools `
        --force --override "--wait --passive --config $REPO_PATH/configs/.vsconfig"
}

function Set-PathVar{
    param (
        [Parameter(Mandatory=$true)]
        [string]
        $Path
    )
    $CurrentUserPath = [Environment]::GetEnvironmentVariable('PATH', 'User')
    if ($CurrentUserPath -notlike "*$Path*") {
        [Environment]::SetEnvironmentVariable('PATH', "$CurrentUserPath;$Path", 'User')
    }
}

# Main
foreach ($Dep in $DEPS) {
    Write-Host "Installing $Dep..."
    if ($Dep -like "*BuildTools") {
        Install-MicrosoftBuildTools
    }
    elseif ($Dep -like "*Cppcheck*") {
        winget install $Dep
        # Set-PathVar -Path "$env:ProgramFiles\Cppcheck"
    }
    else {
        winget install $Dep
    }
}