# Dot-source this script before running the standard CMake commands.
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC C++ build tools were not found' }
$batch = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
# Read the environment from the vendor batch file, without constructing file operations.
$variables = & $env:ComSpec /d /s /c "`"$batch`" >nul && set"
foreach ($line in $variables) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
}
$taskRoot = Split-Path $PSScriptRoot -Parent
$env:APPDATA = Join-Path $taskRoot 'work\appdata'
$env:LOCALAPPDATA = Join-Path $taskRoot 'work\localappdata'
New-Item -ItemType Directory -Force $env:APPDATA,$env:LOCALAPPDATA | Out-Null
$taskTemp = Join-Path $taskRoot 'work\temp'
New-Item -ItemType Directory -Force $taskTemp | Out-Null
$env:TEMP = $taskTemp
$env:TMP = $taskTemp
$env:VSLANG = '1033'
if (Test-Path 'C:\Anaconda\python.exe') { $env:PATH = "C:\Anaconda;$env:PATH" }
