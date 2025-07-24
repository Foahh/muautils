function Invoke-Checked
{
    param([string] $CommandLine)

    Write-Host "-> $CommandLine"
    cmd /c $CommandLine
    if ($LASTEXITCODE -ne 0)
    {
        throw "Command FAILED (exit $LASTEXITCODE): $CommandLine"
    }
}

function Import-VcVars64
{
    Write-Host "`n==> Importing vcvars64 environment ..."

    $vswhere = Join-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer" "vswhere.exe"
    if (-not (Test-Path $vswhere))
    {
        Write-Host "    vswhere.exe not found - install Visual Studio or add vswhere to PATH."
        return
    }

    $vsPath = & $vswhere -latest -products * -property installationPath -nologo
    if (-not $vsPath)
    {
        Write-Host "    Visual Studio installation not found."
        return
    }

    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars))
    {
        Write-Host "    vcvars64.bat not found in $vsPath."
        return
    }

    try
    {
        $output = cmd /c "`"$vcvars`" 2>&1 && set" | Out-String
        $output -split "`r`n" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)')
            {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
            }
        }
        Write-Host "VCVARS64 environment loaded successfully."
    }
    catch
    {
        Write-Host "Failed to load VCVARS64: $_" -ForegroundColor Red
    }
}

$ErrorActionPreference = 'Stop'

try
{
    Import-VcVars64

    $Target = 'Release'
    Write-Host "`n==> Configuring & Building $Target"
    Invoke-Checked "cmake --preset vcpkg"
    Invoke-Checked "cmake --build cmake-build-vcpkg --config $Target --target mua"

    Write-Host "`n==> Done."
}
catch
{
    throw
}