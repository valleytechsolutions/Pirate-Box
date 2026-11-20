param([string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent))
# Compatibility entry point; the current source is the friendly sixteen-frame set.
& (Join-Path $PSScriptRoot 'pack_piratebox_live.ps1') -ProjectRoot $ProjectRoot
