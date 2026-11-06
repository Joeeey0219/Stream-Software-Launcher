$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = 'V1.5.2'
$folder = "StreamLauncher-$version"
$build = Join-Path $repo "dist\$folder"
$exe = Join-Path $build 'StreamLauncher.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Build the public version first.' }

$files = @(
    @{ Source = $exe; Entry = 'StreamLauncher.exe' },
    @{ Source = (Join-Path $repo 'README.md'); Entry = 'README.md' },
    @{ Source = (Join-Path $repo 'LICENSE'); Entry = 'LICENSE' },
    @{ Source = (Join-Path $repo 'settings.example.ini'); Entry = 'settings.example.ini' }
)
$twitchRoot = Join-Path $build 'twitchtest-2.0'
foreach ($file in Get-ChildItem -LiteralPath $twitchRoot -File -Recurse) {
    $relative = $file.FullName.Substring($build.Length + 1).Replace('\', '/')
    $files += @{ Source = $file.FullName; Entry = $relative }
}
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) { throw "Missing package file: $($file.Source)" }
    if ($file.Entry -match '(^|/)(settings\.ini|AGENTS\.md)$' -or $file.Entry -match '(^|/)\.codex-private/') {
        throw "Private file in package list: $($file.Entry)"
    }
}

$release = Join-Path $repo 'release'
New-Item -ItemType Directory -Path $release -Force | Out-Null
$archive = Join-Path $release "$folder.zip"
if (Test-Path -LiteralPath $archive) { throw "Package already exists: $archive" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($archive, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $files) {
        $entry = "$folder/$($file.Entry)"
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip, $file.Source, $entry, [System.IO.Compression.CompressionLevel]::Optimal
        ) | Out-Null
    }
} finally {
    $zip.Dispose()
}
$verify = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    $names = @($verify.Entries | ForEach-Object FullName)
    if ($names | Where-Object { $_ -match '(^|/)(settings\.ini|AGENTS\.md)$' -or $_ -match '(^|/)\.codex-private/' }) {
        throw 'Private file found in package.'
    }
} finally {
    $verify.Dispose()
}
Write-Host "Public package: $archive"