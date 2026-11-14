$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
 . "$PSScriptRoot/dev-shell.ps1"
 cmake --preset release
 if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
 cmake --build --preset release --target octo_live
 if ($LASTEXITCODE -ne 0) { throw 'Live solver build failed' }
 Write-Host 'Ready: run_live_demo.cmd'
} finally { Pop-Location }
