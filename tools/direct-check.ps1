# Diagnostic fallback only. This does not replace the mandatory CMake P0 gate.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/dev-shell.ps1"
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed: $LASTEXITCODE" }
}
foreach ($mode in @('debug','release')) {
    $out = "work/direct-$mode"
    New-Item -ItemType Directory -Force $out | Out-Null
    $options = @('/nologo','/std:c++20','/EHsc','/W4','/WX','/permissive-','/utf-8','/Icore/include',"/Fo$out/")
    if ($mode -eq 'release') { $options += @('/O2','/DNDEBUG') } else { $options += @('/Od','/Zi') }
    Run cl ($options + @('core/src/world.cpp','tests/unit/world_tests.cpp',"/Fe$out/octo_core_tests.exe"))
    Run "./$out/octo_core_tests.exe" @()
    Run cl ($options + @('core/src/world.cpp','core/src/cli.cpp',"/Fe$out/octo_cli.exe"))
    Run "./$out/octo_cli.exe" @('--ticks','1200','--seed','42')
    & "./$out/octo_cli.exe" --ticks -1
    if ($LASTEXITCODE -ne 2) { throw 'negative tick was not rejected' }
}
Run python @('tests/integration/schema_tests.py')
Run python @('tools/check_boundary.py')
