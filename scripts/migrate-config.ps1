param(
    [Parameter(Mandatory = $true)][string]$Source,
    [string]$Destination = (Join-Path $PSScriptRoot '..\settings.ini')
)

$ErrorActionPreference = 'Stop'
$target = if ([System.IO.Path]::IsPathRooted($Destination)) { $Destination } else { Join-Path (Get-Location).Path $Destination }
if (Test-Path -LiteralPath $target) { throw 'Destination already exists. Choose a new path before migrating.' }
$old = Get-Content -LiteralPath $Source -Raw -Encoding UTF8 | ConvertFrom-Json
$values = @{
    obs_path = $old.obs_path
    vts_path = $old.vts_path
    onecomme_path = $old.onecomme_path
    mix_it_path = $old.mix_it_path
    rtmp_path = if ($null -ne $old.rtmp_path) { $old.rtmp_path } else { $old.RTMP_path }
    admin_obs = [bool]$old.admin_obs
    vts_steam = ($old.vts_launch_mode -ne 'direct')
    sync_onecomme = [bool]$old.sync_onecomme
    sync_mix_it = [bool]$old.sync_mix_it
    sync_rtmp = if ($null -ne $old.sync_rtmp) { [bool]$old.sync_rtmp } else { [bool]$old.sync_RTMP }
    sync_twitch = [bool]$old.sync_twitch
    theme_mode = 'system'
    english = $false
    accent_color = '#2C5DCC'
}
$lines = @('# Stream Launcher V1.5.2')
foreach ($key in @('obs_path', 'vts_path', 'onecomme_path', 'mix_it_path', 'rtmp_path', 'admin_obs', 'vts_steam', 'sync_onecomme', 'sync_mix_it', 'sync_rtmp', 'sync_twitch', 'theme_mode', 'english', 'accent_color')) {
    $value = $values[$key]
    if ($value -is [bool]) { $value = $value.ToString().ToLowerInvariant() }
    $lines += "${key}=$value"
}
[System.IO.File]::WriteAllLines($target, $lines, [System.Text.UTF8Encoding]::new($false))
Write-Host "Settings migrated: $target"
