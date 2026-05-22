$ErrorActionPreference = 'Stop'

Set-Location -LiteralPath $PSScriptRoot

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    [Console]::Error.WriteLine('error: clang-format not found in PATH')
    exit 1
}

$extensions = @('.cpp', '.hpp', '.h', '.c', '.cc', '.cxx')
$files = @(
    (Join-Path $PSScriptRoot 'src'),
    (Join-Path $PSScriptRoot 'tests')
) | ForEach-Object {
    Get-ChildItem -LiteralPath $_ -Recurse -File |
        Where-Object { $extensions -contains $_.Extension }
} | Sort-Object FullName

if ($files) {
    & clang-format -i -- @($files.FullName)
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host 'clang-format: done'
