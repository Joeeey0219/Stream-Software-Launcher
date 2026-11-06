$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Build Tools not found' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'C++ Build Tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

# Import the VS compiler environment into PowerShell.
$environment = & cmd.exe /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Failed to initialize the VS compiler environment' }
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
    }
}

$output = [System.IO.Path]::GetFullPath((Join-Path $repo 'dist\StreamLauncher-V1.5.2'))
$repoRoot = [System.IO.Path]::GetFullPath($repo).TrimEnd('\') + '\'
if (-not $output.StartsWith($repoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build output is outside the repository'
}
$objects = Join-Path $repo 'build'
New-Item -ItemType Directory -Path $output, $objects -Force | Out-Null
Push-Location -LiteralPath $objects
try {
    & rc.exe /nologo "/foStreamLauncher.res" (Join-Path $repo 'src\resources.rc')
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & cl.exe /nologo /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE "/Fe:$output\StreamLauncher.exe" (Join-Path $repo 'src\main.cpp') (Join-Path $repo 'src\launcher.cpp') (Join-Path $repo 'src\autostart.cpp') (Join-Path $repo 'src\update_check.cpp') StreamLauncher.res /link /SUBSYSTEM:WINDOWS shell32.lib comdlg32.lib user32.lib gdi32.lib advapi32.lib winhttp.lib dwmapi.lib
    if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
    & cl.exe /nologo /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE "/Fe:$objects\launcher_tests.exe" (Join-Path $repo 'tests\test_launcher.cpp') (Join-Path $repo 'src\launcher.cpp') (Join-Path $repo 'src\autostart.cpp') (Join-Path $repo 'src\update_check.cpp') /link shell32.lib user32.lib advapi32.lib winhttp.lib
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & (Join-Path $objects 'launcher_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    $settingsTest = Join-Path $objects 'settings-test'
    New-Item -ItemType Directory -Path $settingsTest -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $objects 'launcher_tests.exe') -Destination $settingsTest -Force
    & (Join-Path $settingsTest 'launcher_tests.exe') --settings-roundtrip
    if ($LASTEXITCODE -ne 0) { throw 'Settings test failed' }
} finally {
    Pop-Location
}
Copy-Item -LiteralPath (Join-Path $repo 'settings.example.ini') -Destination $output -Force
Copy-Item -LiteralPath (Join-Path $repo 'twitchtest-2.0') -Destination $output -Recurse -Force
Write-Host "Build complete: $output"
