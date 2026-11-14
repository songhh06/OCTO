param([switch]$Capture, [switch]$Check, [switch]$Wait, [switch]$Test)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:APPDATA = Join-Path $root 'work\appdata'
$env:LOCALAPPDATA = Join-Path $root 'work\localappdata'
New-Item -ItemType Directory -Force $env:APPDATA,$env:LOCALAPPDATA | Out-Null
$engine = 'C:\Tools\Godot\godot.exe'
if (-not (Test-Path $engine)) { $engine = (Get-Command godot -ErrorAction Stop).Source }
$argsList = @('--path',('"' + (Join-Path $root 'game') + '"'),'--single-threaded-scene','--render-thread','safe','--resolution','1280x800','res://scenes/octopus_preview.tscn')
if ($Check) { $argsList += @('--headless','--quit-after','12') }
if ($Test) { $argsList += @('--','--preview-test') }
elseif ($Capture) { $argsList += @('--','--preview-capture') }
$outLog = Join-Path $root 'work\preview-stdout.log'
$errLog = Join-Path $root 'work\preview-stderr.log'
if ($Check) {
    $process = Start-Process -FilePath $engine -ArgumentList $argsList -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
    Get-Content $outLog,$errLog
    if ($process.ExitCode -ne 0 -or (Get-Content $errLog -Raw)) { throw 'Preview validation failed; see work/preview-stderr.log' }
} elseif ($Wait -or $Test) {
    $process = Start-Process -FilePath $engine -ArgumentList $argsList -Wait -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
    $stdout = Get-Content $outLog -Raw
    $stderr = Get-Content $errLog -Raw
    $stdout
    if ($stderr) { $stderr }
    if ($process.ExitCode -ne 0 -or $stderr) { throw 'Preview failed; see work/preview-stderr.log' }
    if ($Test -and -not $stdout.Contains('PREVIEW_INPUT_CHECK_OK')) { throw 'Preview test marker missing' }
} else {
    # Visible interactive window explicitly requested by the user.
    Start-Process -FilePath $engine -ArgumentList $argsList -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog | Select-Object Id,ProcessName
}
