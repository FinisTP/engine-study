[CmdletBinding()]
param(
    [switch]$SkipAddressSanitizer
)

$ErrorActionPreference = 'Stop'

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory)]
        [string]$Command,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $Command $($Arguments -join ' ')"
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake was not found. Run this script from an x64 Visual Studio Developer PowerShell.'
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    throw 'The MSVC compiler was not found. Run this script from an x64 Visual Studio Developer PowerShell.'
}

if (-not (Get-Command clang-cl -ErrorAction SilentlyContinue)) {
    throw 'clang-cl was not found. Install the Visual Studio C++ Clang tools component.'
}

if (-not $env:VCPKG_ROOT) {
    throw 'VCPKG_ROOT must point to the vcpkg installation.'
}

$configurations = @(
    @{ Configure = 'windows-msvc'; Build = 'windows-msvc-debug'; Test = 'windows-msvc-debug' },
    @{ Configure = 'windows-clang'; Build = 'windows-clang-debug'; Test = 'windows-clang-debug' }
)

if (-not $SkipAddressSanitizer) {
    $configurations += @{
        Configure = 'windows-msvc-asan'
        Build = 'windows-msvc-asan-debug'
        Test = 'windows-msvc-asan-debug'
    }
}

foreach ($configuration in $configurations) {
    Write-Host "Verifying $($configuration.Configure)..."
    Invoke-NativeCommand cmake @('--fresh', '--preset', $configuration.Configure)
    Invoke-NativeCommand cmake @('--build', '--preset', $configuration.Build)
    Invoke-NativeCommand ctest @('--preset', $configuration.Test)
}

Write-Host 'Phase 1 automated verification passed.'
