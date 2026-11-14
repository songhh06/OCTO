param([ValidateSet('arm','coupled','eight')][string]$Scenario='coupled', [ValidateRange(0,6)][int]$Channel=0, [double]$Command=0.04, [double]$Mass=0.5, [ValidateRange(2,72000)][int]$Ticks=600, [switch]$Capture, [switch]$BuildOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    . "$PSScriptRoot/dev-shell.ps1"
    cmake --preset release
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
    New-Item -ItemType Directory -Force game/data,docs/progress/physics | Out-Null
    & ./build/release/octo_sim.exe --scenario $Scenario --channel $Channel --command $Command --mass $Mass --ticks $Ticks --stride 4 --trace game/data/physics_trace.json --stats "docs/progress/physics/$Scenario-stats.json" --checkpoint "work/$Scenario.octostate"
    if ($LASTEXITCODE -ne 0) { throw 'Native solver failed. Check the failure checkpoint and stats; no successful playback is claimed.' }
    $manifest=[ordered]@{scenario=$Scenario;channel=$Channel;command=$Command;box_mass_kg=$Mass;ticks=$Ticks;build='MSVC 19.43 Release';fixed_dt_s=(1.0/120);unit_system='SI';coordinate_system='RH_Y_UP';source='native-cxx20-cosserat';playback_only=$true;bio_validated=$false}
    foreach($item in @(@('profile','profiles/dynamics-model.json'),@('trace','game/data/physics_trace.json'),@('executable','build/release/octo_sim.exe'))) {
        $manifest[$item[0]+'_sha256']=(Get-FileHash -LiteralPath $item[1] -Algorithm SHA256).Hash.ToLower()
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content docs/progress/physics/manifest.json -Encoding utf8
    if ($BuildOnly) { return }
    $engine=(Get-Command godot).Source
    $arguments=@('--path',('"'+(Join-Path $root 'game')+'"'),'--single-threaded-scene','--render-thread','safe','--resolution','1280x800','res://scenes/physics_lab.tscn')
    if ($Capture) { $arguments+=@('--quit-after','180','--','--lab-capture') }
    $process=Start-Process -FilePath $engine -ArgumentList $arguments -Wait -PassThru -RedirectStandardOutput (Join-Path $root 'work/lab-stdout.log') -RedirectStandardError (Join-Path $root 'work/lab-stderr.log')
    $stdout=Get-Content work/lab-stdout.log -Raw
    $stderr=Get-Content work/lab-stderr.log -Raw
    $stdout
    if ($process.ExitCode -ne 0 -or $stderr) { $stderr; throw 'Physics lab renderer failed' }
    if ($Capture -and -not $stdout.Contains('PHYSICS_LAB_CAPTURE_OK')) { throw 'Physics lab capture marker missing' }
} finally { Pop-Location }
