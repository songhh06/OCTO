$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
 . "$PSScriptRoot/dev-shell.ps1"
 # Build directories were once committed with absolute local CMake paths.
 # Discard the cache only when it belongs to a different checkout root.
 $cache = Join-Path (Get-Location).Path 'build/release/CMakeCache.txt'
 if (Test-Path -LiteralPath $cache) {
   $home = Get-Content -LiteralPath $cache | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } | Select-Object -First 1
   $actual = if ($home) { ($home -split '=',2)[1].Replace('\','/').TrimEnd('/') } else { '' }
   $expected = (Get-Location).Path.Replace('\','/').TrimEnd('/')
   if (-not [string]::Equals($actual, $expected, [StringComparison]::OrdinalIgnoreCase)) {
     Write-Host 'Ignoring CMake build cache from a different checkout.'
     Remove-Item -LiteralPath 'build/release' -Recurse -Force
   }
 }
 cmake --preset release
 if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
 cmake --build --preset release --target octo_live
 if ($LASTEXITCODE -ne 0) { throw 'Live solver build failed' }
 Write-Host 'Ready: run_live_demo.cmd'
} finally { Pop-Location }
