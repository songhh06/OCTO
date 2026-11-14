$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    . "$PSScriptRoot/dev-shell.ps1"
    cmake --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
    cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    New-Item -ItemType Directory -Force game/data/demo,work | Out-Null
    function Record-Case([string]$Name,[string]$Scenario,[int]$Channel,[double]$Activation,[double]$Mass) {
        & ./build/release/octo_sim.exe --scenario $Scenario --channel $Channel --command $Activation --mass $Mass --ticks 600 --stride 4 --trace "game/data/demo/$Name.json" --stats "game/data/demo/$Name-stats.json"
        if ($LASTEXITCODE -ne 0) { throw "Recording failed: $Name" }
    }
    Record-Case bend arm 0 0.12 0.5
    Record-Case extend arm 4 0.12 0.5
    Record-Case light coupled 0 0.04 0.5
    Record-Case heavy coupled 0 0.04 8
    & ./build/release/octo_replay.exe --scene tests/scenes/P2_eight_cups.json --inputs tests/scenes/P2_eight_inputs.json --record work/demo-init.json --ticks 120 --checkpoint work/demo-init.octostate
    if ($LASTEXITCODE -ne 0) { throw 'Eight-arm initial state failed' }
    & ./build/release/octo_sim.exe --scenario eight --restore work/demo-init.octostate --ticks 240 --stride 4 --trace game/data/demo/eight.json --stats game/data/demo/eight-stats.json
    if ($LASTEXITCODE -ne 0) { throw 'Eight-arm recording failed' }
    $manifest=[ordered]@{source='native-cxx20-cosserat';playback_only=$true;bio_validated=$false;fixed_dt_s=(1.0/120);executable_sha256=(Get-FileHash build/release/octo_sim.exe).Hash;cases=@()}
    foreach($name in @('bend','extend','light','heavy','eight')) {
        $record=Get-Content "game/data/demo/$name.json" -Raw | ConvertFrom-Json
        if (-not $record.success) { throw "Failed native trace: $name" }
        $manifest.cases+=@{name=$name;trace_sha256=(Get-FileHash "game/data/demo/$name.json").Hash;stats=(Get-Content "game/data/demo/$name-stats.json" -Raw | ConvertFrom-Json)}
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content game/data/demo/manifest.json -Encoding utf8
    Write-Host 'Demo recordings ready. Double-click run_p2_demo.cmd.'
} finally { Pop-Location }
