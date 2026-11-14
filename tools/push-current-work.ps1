param([string]$Branch='feature/P2-live-demo-wip')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
Push-Location $taskRoot
try {
 function Git-Checked([string[]]$GitArguments) {
   & git @GitArguments
   if ($LASTEXITCODE -ne 0) { throw "Git failed ($LASTEXITCODE). No force push or automatic reset is performed." }
 }
 $taskOrigin=(& git remote get-url origin)
 if ($LASTEXITCODE -ne 0 -or $taskOrigin.Trim() -notin @('https://github.com/Aggang-77/Octopus-simulator.git','git@github.com:Aggang-77/Octopus-simulator.git')) { throw 'Unexpected origin. Inspect remote configuration first.' }
 Git-Checked @('diff','--check')
 # Refuse to include pre-existing staged content implicitly.
 & git diff --cached --quiet
 if ($LASTEXITCODE -eq 1) { throw 'The index already contains staged changes. Review and commit them separately first.' }
 if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the Git index.' }
 $taskCurrent=(& git branch --show-current).Trim()
 if ($taskCurrent -ne $Branch) { Git-Checked @('switch','-c',$Branch) }
 $taskPaths=@('AGENTS.md','.clang-format','.gitattributes','.github','.gitignore','CMakeLists.txt','CMakePresets.json','LICENSE.draft.md','README.md','assets','core','docs','game','godot_bridge','profiles','replays','push_current_work.cmd','run_live_demo.cmd','run_p2_demo.cmd','run_physics_lab.cmd','run_preview.cmd','schemas','tests','toolchain.lock.json','tools')
 Git-Checked (@('add','--')+$taskPaths)
 Git-Checked @('diff','--cached','--stat')
 & git diff --cached --quiet
 if ($LASTEXITCODE -eq 1) { Git-Checked @('commit','-m','P2: checkpoint live demo and volumetric work (WIP; contact failure open)') }
 elseif ($LASTEXITCODE -ne 0) { throw 'Cannot inspect staged changes.' }
 Git-Checked @('push','--set-upstream','origin',"HEAD:refs/heads/$Branch")
 Write-Host "Uploaded WIP branch: $Branch"
 Git-Checked @('rev-parse','HEAD')
} finally { Pop-Location }
