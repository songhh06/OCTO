param([switch]$RequireCompleteP0)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/dev-shell.ps1"
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function Invoke-GodotChecked([string[]]$Arguments, [string]$Marker = '') {
    $out = Join-Path $env:TEMP 'godot-stdout.log'
    $err = Join-Path $env:TEMP 'godot-stderr.log'
    $process = Start-Process -FilePath (Get-Command godot).Source -ArgumentList $Arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $code = $process.ExitCode
    $log = (Get-Content -LiteralPath $out -Raw) + (Get-Content -LiteralPath $err -Raw)
    $log | Out-Host
    if ($code -ne 0 -or $log -match '(?m)^(ERROR:|SCRIPT ERROR:)') {
        throw "Godot failed: exit $code"
    }
    if ($Marker -and -not $log.Contains($Marker)) { throw "Godot marker missing: $Marker" }
}
Invoke-Checked python @('-c','from importlib.metadata import version; assert version("jsonschema") == "4.25.0"')
Invoke-Checked python @('tools/check_boundary.py')
foreach ($preset in @('dev','release','asan')) {
    Invoke-Checked cmake @('--preset',$preset)
    Invoke-Checked cmake @('--build','--preset',$preset)
    if ($preset -eq 'release') {
        Invoke-Checked ctest @('--preset',$preset,'--output-on-failure','--no-tests=error')
    } else {
        # Long dynamic/soak tests run completely in Release; debug + ASan run
        # the analytic, unit, rest-state, contact, fluid and rollback regressions.
        Invoke-Checked ctest @('--preset',$preset,'--output-on-failure','--no-tests=error','-LE','long')
    }
}
$llvm = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin'
$sources = @(Get-ChildItem core,tests/unit,godot_bridge,tools -Recurse -File | Where-Object { $_.Extension -in '.cpp','.hpp' } | ForEach-Object { $_.FullName })
Invoke-Checked (Join-Path $llvm 'clang-format.exe') (@('--dry-run','--Werror') + $sources)
Invoke-Checked (Join-Path $llvm 'clang-tidy.exe') (@(Get-ChildItem core/src -Filter '*.cpp' | ForEach-Object {$_.FullName}) + @('-p','build/dev','--checks=-*,clang-analyzer-*','--warnings-as-errors=*'))
Invoke-GodotChecked @('--headless','--single-threaded-scene','--render-thread','safe','--path','game','--editor','--quit')
Invoke-GodotChecked @('--headless','--single-threaded-scene','--render-thread','safe','--path','game','--quit-after','3') 'P0_SCENE_READY'
if ($RequireCompleteP0) {
    Invoke-Checked python @('tools/verify_lock.py','--require-verified')
    Invoke-GodotChecked @('--headless','--single-threaded-scene','--render-thread','safe','--path','game','--','--bridge-smoke') 'P0_BRIDGE_ROUNDTRIP_OK: tick=1'
}
Write-Host 'Available bootstrap checks passed. Full P0 requires -RequireCompleteP0.'
