# Collect third-party copyright files from a vcpkg install into legal/licenses/.
# Usage:
#   .\scripts\collect-licenses.ps1
#   .\scripts\collect-licenses.ps1 -BuildDir cmake-build-vcpkg -Triplet x64-windows-static

param(
    [string]$BuildDir = "cmake-build-vcpkg",
    [string]$Triplet = "x64-windows-static"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
$legalDir = Join-Path $root "legal"
$licensesDir = Join-Path $legalDir "licenses"
$vcpkgShare = Join-Path $root "$BuildDir/vcpkg_installed/$Triplet/share"

if (-not (Test-Path $vcpkgShare)) {
    Write-Error "vcpkg share dir not found: $vcpkgShare. Configure and build with vcpkg first."
}

$packages = @(
    "ffmpeg",
    "openimageio",
    "opencolorio",
    "openexr",
    "imath",
    "libpng",
    "libjpeg-turbo",
    "tiff",
    "libwebp",
    "zlib",
    "libdeflate",
    "openjph",
    "zstd",
    "bzip2",
    "liblzma",
    "robin-map",
    "expat",
    "yaml-cpp",
    "pystring",
    "minizip-ng",
    "fmt",
    "spdlog",
    "cli11"
)

New-Item -ItemType Directory -Force -Path $licensesDir | Out-Null

Copy-Item -Force (Join-Path $root "LICENSE") (Join-Path $licensesDir "LGPL-2.1.txt")

$bc7 = Join-Path $root "third_party/bc7enc_rdo/LICENSE"
if (Test-Path $bc7) {
    Copy-Item -Force $bc7 (Join-Path $licensesDir "bc7enc_rdo.txt")
}

$missing = @()
foreach ($pkg in $packages) {
    $src = Join-Path $vcpkgShare "$pkg/copyright"
    $dst = Join-Path $licensesDir "$pkg.txt"
    if (Test-Path $src) {
        Copy-Item -Force $src $dst
    } else {
        $missing += $pkg
    }
}

if ($missing.Count -gt 0) {
    Write-Warning "Missing copyright files: $($missing -join ', ')"
}

Write-Host "Wrote licenses to $licensesDir"
