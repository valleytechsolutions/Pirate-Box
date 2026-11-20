param(
  [string]$CacheRoot = (Join-Path $env:LOCALAPPDATA 'PirateBoxSquare'),
  [string]$SeedPython = (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'),
  [string]$SeedToolDirectory = (Join-Path $env:USERPROFILE '.platformio\tools')
)
$ErrorActionPreference = 'Stop'
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
$core = Join-Path $CacheRoot 'platformio'
$toolRoot = Join-Path $core 'tools'
$privatePython = Join-Path $CacheRoot 'venv\Scripts\python.exe'
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
foreach ($tool in @('toolchain-riscv32-esp', 'tool-esptoolpy')) {
  $destination = Join-Path $toolRoot $tool
  $source = Join-Path $SeedToolDirectory $tool
  if (-not (Test-Path -LiteralPath $destination -PathType Container)) {
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing verified seed tool: $source" }
    Copy-Item -LiteralPath $source -Destination $destination -Recurse
  }
  $manifest = Get-Content -LiteralPath (Join-Path $destination 'package.json') -Raw | ConvertFrom-Json
  $expected = if ($tool -eq 'toolchain-riscv32-esp') { '14.2.0+20260121' } else { '5.4.0' }
  if ($manifest.version -ne $expected) { throw "Unexpected private $tool version $($manifest.version); expected $expected" }
}
$compiler = Join-Path $toolRoot 'toolchain-riscv32-esp\bin\riscv32-esp-elf-g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'Private RISC-V compiler copy is incomplete.' }
if (-not (Test-Path -LiteralPath $privatePython -PathType Leaf)) {
  if (-not (Test-Path -LiteralPath $SeedPython -PathType Leaf)) { throw "Missing Python bootstrap runtime: $SeedPython" }
  & $SeedPython -m venv (Join-Path $CacheRoot 'venv')
  if ($LASTEXITCODE -ne 0) { throw 'Private Python environment creation failed.' }
}
$version = & $privatePython -c "import platformio; print(platformio.__version__)" 2>$null
if ($LASTEXITCODE -ne 0 -or $version -ne '6.2.0') {
  & $privatePython -m pip install --disable-pip-version-check 'platformio==6.2.0' 'esptool==5.4.0'
  if ($LASTEXITCODE -ne 0) { throw 'Private PlatformIO/esptool installation failed.' }
}
& $privatePython (Join-Path $toolRoot 'tool-esptoolpy\esptool.py') version
if ($LASTEXITCODE -ne 0) { throw 'Private esptool dependencies are incomplete.' }
Write-Host "Private Pirate Box Python/toolchain ready: $CacheRoot"
