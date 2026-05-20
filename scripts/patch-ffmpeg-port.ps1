$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $PSCommandPath
$MuautilsRoot = (Resolve-Path -LiteralPath (Join-Path $ScriptDir '..')).Path
$OverlayDir = Join-Path $ScriptDir 'ffmpeg-port-overlay'
$SrcPort = 'ports/ffmpeg'

function Exit-WithError
{
    param([string] $Message)

    [Console]::Error.WriteLine($Message)
    exit 1
}

function Resolve-VcpkgDir
{
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_DIR))
    {
        return $env:VCPKG_DIR
    }

    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT))
    {
        return $env:VCPKG_ROOT
    }

    Exit-WithError 'error: set VCPKG_DIR or VCPKG_ROOT to your local microsoft/vcpkg clone'
}

function Show-Usage
{
    Write-Host "Usage: $($MyInvocation.ScriptName) [--help|-h]"
    Write-Host ''
    Write-Host "  Clean ports/ffmpeg, copy $SrcPort from the vcpkg checkout, then apply"
    Write-Host "  patches from $OverlayDir/ (see muautils-ffmpeg.patch)."
    Write-Host ''
    Write-Host '  Environment:'
    Write-Host '    VCPKG_DIR or VCPKG_ROOT  Path to the vcpkg repository root (required)'
    exit 0
}

function Require-UpstreamPort
{
    param([string] $VcpkgDir)

    if (-not (Test-Path -LiteralPath (Join-Path $VcpkgDir '.git') -PathType Container))
    {
        Exit-WithError "error: vcpkg repo not found at $VcpkgDir (set VCPKG_DIR or VCPKG_ROOT)"
    }

    $upstreamPort = Join-Path $VcpkgDir $SrcPort
    if (-not (Test-Path -LiteralPath $upstreamPort -PathType Container))
    {
        Exit-WithError "error: upstream port missing: $upstreamPort"
    }
}

function Invoke-Refresh
{
    $vcpkgDir = Resolve-VcpkgDir
    Require-UpstreamPort $vcpkgDir

    $patches = @(Get-ChildItem -LiteralPath $OverlayDir -Filter '*.patch' -File | Sort-Object -Property Name)
    if ($patches.Count -eq 0)
    {
        Exit-WithError "error: no *.patch files in $OverlayDir"
    }

    $targetPort = Join-Path $MuautilsRoot $SrcPort
    $upstreamPort = Join-Path $vcpkgDir $SrcPort

    Write-Host "Removing $targetPort"
    if (Test-Path -LiteralPath $targetPort)
    {
        Remove-Item -LiteralPath $targetPort -Recurse -Force
    }

    Write-Host "Copying from $upstreamPort"
    Copy-Item -LiteralPath $upstreamPort -Destination $targetPort -Recurse -Force

    Write-Host 'Applying overlay patch(es)'
    foreach ($patch in $patches)
    {
        Write-Host "  $($patch.Name)"
        & git -C $MuautilsRoot apply $patch.FullName
        if ($LASTEXITCODE -ne 0)
        {
            exit $LASTEXITCODE
        }
    }

    Write-Host "Done. Review changes under $SrcPort and commit when satisfied."
}

switch ($args.Count)
{
    0 { Invoke-Refresh }
    1 {
        switch ($args[0])
        {
            '--help' { Show-Usage }
            '-h' { Show-Usage }
            default { Show-Usage }
        }
    }
    default { Show-Usage }
}
