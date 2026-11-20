param(
  [string]$CacheRoot = (Join-Path $env:LOCALAPPDATA 'PirateBoxSquare'),
  [string]$Python = '',
  [string]$BuildRoot = '',
  [string]$ArtifactDirectory = '',
  [ValidateRange(1, 16)][int]$Jobs = 4,
  [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$helper = Join-Path $PSScriptRoot 'piratebox_square_artifacts.py'
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
& (Join-Path $PSScriptRoot 'provision_piratebox_square.ps1') -CacheRoot $CacheRoot
if ([string]::IsNullOrWhiteSpace($Python)) { $Python = Join-Path $CacheRoot 'venv\Scripts\python.exe' }
if (-not (Test-Path -LiteralPath $Python -PathType Leaf)) { throw "Missing Python runtime: $Python" }
if ([string]::IsNullOrWhiteSpace($BuildRoot)) { $BuildRoot = Join-Path $projectRoot '.pio\build' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$buildDirectory = Join-Path $BuildRoot 'waveshare_c5_273'
if ([string]::IsNullOrWhiteSpace($ArtifactDirectory)) { $ArtifactDirectory = $buildDirectory }
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$workDirectory = Join-Path $projectRoot ".work\square-build-$stamp-$PID"
New-Item -ItemType Directory -Force -Path $workDirectory, $ArtifactDirectory | Out-Null
$snapshot = Join-Path $workDirectory 'source-before.json'
$logPath = Join-Path $workDirectory 'build.log'
$oldPath = $env:PATH
$oldUtf8 = $env:PYTHONUTF8
$oldEncoding = $env:PYTHONIOENCODING
$oldBuild = $env:PLATFORMIO_BUILD_DIR
$savedEnvironment = @{}
foreach ($name in @('PLATFORMIO_CORE_DIR','PLATFORMIO_PACKAGES_DIR','PLATFORMIO_PLATFORMS_DIR','PLATFORMIO_CACHE_DIR','PLATFORMIO_GLOBALLIB_DIR','PIRATEBOX_TOOLCHAIN_DIR','PIRATEBOX_ESPTOOL_ROOT','IDF_TOOLS_PATH')) {
  $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
try {
  $env:PLATFORMIO_CORE_DIR = Join-Path $CacheRoot 'platformio'
  $env:PLATFORMIO_PACKAGES_DIR = Join-Path $env:PLATFORMIO_CORE_DIR 'packages'
  $env:PLATFORMIO_PLATFORMS_DIR = Join-Path $env:PLATFORMIO_CORE_DIR 'platforms'
  $env:PLATFORMIO_CACHE_DIR = Join-Path $env:PLATFORMIO_CORE_DIR '.cache'
  $env:PLATFORMIO_GLOBALLIB_DIR = Join-Path $env:PLATFORMIO_CORE_DIR 'lib'
  $env:IDF_TOOLS_PATH = $env:PLATFORMIO_CORE_DIR
  $env:PIRATEBOX_TOOLCHAIN_DIR = Join-Path $env:PLATFORMIO_CORE_DIR 'tools\toolchain-riscv32-esp'
  $env:PIRATEBOX_ESPTOOL_ROOT = Join-Path $env:PLATFORMIO_CORE_DIR 'tools\tool-esptoolpy'
  $compilerBin = Join-Path $env:PIRATEBOX_TOOLCHAIN_DIR 'bin'
  $env:PATH = "$compilerBin;$(Split-Path $Python -Parent);$oldPath"
  $env:PYTHONUTF8 = '1'
  $env:PYTHONIOENCODING = 'utf-8'
  $env:PLATFORMIO_BUILD_DIR = $BuildRoot
  Push-Location $projectRoot
  try {
    # Finish all downloads/installations before the stable source/dependency
    # snapshot. This private core cannot replace another chat's framework.
    & $Python -m platformio pkg install -e waveshare_c5_273
    if ($LASTEXITCODE -ne 0) { throw 'Private PlatformIO package provisioning failed.' }
    if ($Clean) {
      & $Python -m platformio run -e waveshare_c5_273 -t clean
      if ($LASTEXITCODE -ne 0) { throw 'PlatformIO clean failed.' }
    }
    & $Python $helper snapshot --snapshot $snapshot
    if ($LASTEXITCODE -ne 0) { throw 'Could not capture pre-build source hashes.' }
    & $Python -m platformio run -e waveshare_c5_273 -j $Jobs 2>&1 | Tee-Object -FilePath $logPath
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO build failed. See $logPath" }
    $pioCore = if ($env:PLATFORMIO_CORE_DIR) { $env:PLATFORMIO_CORE_DIR } else { Join-Path $env:USERPROFILE '.platformio' }
    $packages = if ($env:PLATFORMIO_PACKAGES_DIR) { $env:PLATFORMIO_PACKAGES_DIR } else { Join-Path $pioCore 'packages' }
    $bootApp = Join-Path $packages 'framework-arduinoespressif32\tools\partitions\boot_app0.bin'
    if (-not (Test-Path -LiteralPath $bootApp -PathType Leaf)) { throw "Missing OTA selector: $bootApp" }
    Copy-Item -LiteralPath $bootApp -Destination (Join-Path $ArtifactDirectory 'boot_app0.bin') -Force
    $bootloader = Join-Path $projectRoot 'hardware\waveshare-c5-273\bootloader.bin'
    if (-not (Test-Path -LiteralPath $bootloader -PathType Leaf)) { throw "Missing reviewed C5 bootloader: $bootloader" }
    Copy-Item -LiteralPath $bootloader -Destination (Join-Path $ArtifactDirectory 'bootloader.bin') -Force
    # PlatformIO merges a factory image before we substitute the reviewed DIO
    # ROM bootloader. Do not leave that unreviewed merged image beside the
    # verified segments: it can carry the incompatible default QIO header and
    # its filler would also erase saved NVS if flashed as one continuous range.
    $unreviewedFactory = Join-Path $buildDirectory 'firmware.factory.bin'
    if (Test-Path -LiteralPath $unreviewedFactory -PathType Leaf) {
      Remove-Item -LiteralPath $unreviewedFactory
    }
    if ($ArtifactDirectory -ne $buildDirectory) {
      foreach ($name in @('partitions.bin', 'firmware.bin')) {
        Copy-Item -LiteralPath (Join-Path $buildDirectory $name) -Destination (Join-Path $ArtifactDirectory $name) -Force
      }
    }
    & $Python $helper record --snapshot $snapshot --artifact $ArtifactDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Build receipt validation failed; these images must not be flashed.' }
    Copy-Item -LiteralPath $logPath -Destination (Join-Path $ArtifactDirectory 'build.log') -Force
    Write-Host "Pirate Box build verified: $ArtifactDirectory"
  } finally { Pop-Location }
} finally {
  $env:PATH = $oldPath
  $env:PYTHONUTF8 = $oldUtf8
  $env:PYTHONIOENCODING = $oldEncoding
  $env:PLATFORMIO_BUILD_DIR = $oldBuild
  foreach ($name in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process') }
}
