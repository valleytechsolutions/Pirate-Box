param(
  [string]$ArtifactDirectory = '',
  [string]$Port = 'COM79',
  [ValidateRange(115200, 2000000)][int]$Baud = 115200,
  [string]$CacheRoot = (Join-Path $env:LOCALAPPDATA 'PirateBoxSquare'),
  [string]$Python = '',
  [string]$Backup = '',
  [switch]$Execute
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
if ([string]::IsNullOrWhiteSpace($Python)) { $Python = Join-Path $CacheRoot 'venv\Scripts\python.exe' }
if ([string]::IsNullOrWhiteSpace($ArtifactDirectory)) { $ArtifactDirectory = Join-Path $projectRoot '.pio\build\waveshare_c5_273' }
if ([string]::IsNullOrWhiteSpace($Backup)) { $Backup = Join-Path (Split-Path $projectRoot -Parent) 'backups\piratebox-before-square-20261031-32mb.bin' }
if ($Port -notmatch '^COM\d+$') { throw "Expected a Windows serial port, got $Port" }
if (-not (Test-Path -LiteralPath $Python -PathType Leaf)) { throw "Missing Python runtime: $Python" }
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
$Backup = [IO.Path]::GetFullPath($Backup)
$oldUtf8 = $env:PYTHONUTF8
$oldEncoding = $env:PYTHONIOENCODING
$savedEnvironment = @{}
foreach ($name in @('PLATFORMIO_CORE_DIR','PLATFORMIO_PACKAGES_DIR','PIRATEBOX_ESPTOOL_ROOT')) {
  $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
try {
  $env:PLATFORMIO_CORE_DIR = Join-Path $CacheRoot 'platformio'
  $env:PLATFORMIO_PACKAGES_DIR = Join-Path $env:PLATFORMIO_CORE_DIR 'packages'
  $env:PIRATEBOX_ESPTOOL_ROOT = Join-Path $env:PLATFORMIO_CORE_DIR 'tools\tool-esptoolpy'
  $esptoolEntry = Join-Path $env:PIRATEBOX_ESPTOOL_ROOT 'esptool.py'
  if (-not (Test-Path -LiteralPath $esptoolEntry -PathType Leaf)) { throw 'Private esptool missing; run the build/provision wrapper.' }
  $env:PYTHONUTF8 = '1'
  $env:PYTHONIOENCODING = 'utf-8'
  & $Python (Join-Path $PSScriptRoot 'piratebox_square_artifacts.py') verify --artifact $ArtifactDirectory --backup $Backup
  if ($LASTEXITCODE -ne 0) { throw 'Artifact or backup validation failed; nothing was flashed.' }
  $segments = @(
    '0x2000', (Join-Path $ArtifactDirectory 'bootloader.bin'),
    '0x8000', (Join-Path $ArtifactDirectory 'partitions.bin'),
    '0xe000', (Join-Path $ArtifactDirectory 'boot_app0.bin'),
    '0x10000', (Join-Path $ArtifactDirectory 'firmware.bin')
  )
  Write-Host 'Verified write plan: bootloader 0x2000, partitions 0x8000, OTA selector 0xe000, firmware 0x10000.'
  Write-Host 'The flash transfer excludes NVS, SD and filesystem contents. On first boot, firmware initializes only a fully verified blank upper 16MiB piratefs partition; nonblank data is preserved.'
  if (-not $Execute) {
    Write-Host 'Artifact-only validation complete; no serial port was opened. Add -Execute to flash.'
    return
  }
  $probe = & $Python $esptoolEntry --chip esp32c5 --port $Port --baud 115200 --no-stub --after watchdog-reset flash-id 2>&1 | Out-String
  if ($LASTEXITCODE -ne 0) { throw "ESP32-C5 probe failed on $Port`n$probe" }
  if ($probe -notmatch '(?i)Detected flash size:\s*32\s*MB') { throw "Expected 32MB physical flash on $Port`n$probe" }
  Write-Host $probe
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $writeLog = Join-Path $ArtifactDirectory "flash-$stamp.log"
  & $Python $esptoolEntry --chip esp32c5 --port $Port --baud $Baud --no-stub --before default-reset --after no-reset `
    write-flash --flash-mode keep --flash-freq keep --flash-size keep @segments 2>&1 | Tee-Object -FilePath $writeLog
  if ($LASTEXITCODE -ne 0) { throw "Firmware write failed. Backup preserved at $Backup" }
  & $Python $esptoolEntry --chip esp32c5 --port $Port --baud $Baud --no-stub --before default-reset --after watchdog-reset `
    verify-flash @segments 2>&1 | Tee-Object -FilePath (Join-Path $ArtifactDirectory "verify-$stamp.log")
  if ($LASTEXITCODE -ne 0) { throw "Separate byte verification failed. Backup preserved at $Backup" }
  Write-Host 'All four images written and independently byte-verified; watchdog reset requested.'
  Write-Host 'Firmware transfer verification does not establish standalone boot, display or control behavior; validate those on the device.'
} finally {
  $env:PYTHONUTF8 = $oldUtf8
  $env:PYTHONIOENCODING = $oldEncoding
  foreach ($name in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process') }
}
