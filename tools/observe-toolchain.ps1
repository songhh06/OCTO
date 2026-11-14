# Inventory only: local digests do not establish official distribution provenance.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/dev-shell.ps1"
function Observed([string]$Command,[string]$Version,[string]$Source,[string]$License) {
    $path = (Get-Command $Command).Source
    return [ordered]@{version=$Version;source_url=$Source;url=$null;sha256=$null;
        local_path=$path;local_sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower();
        license=$License;status='installed_provenance_unverified'}
}
function Missing([string]$Version,[string]$Source,[string]$License) {
    return [ordered]@{version=$Version;commit=$null;source_url=$Source;url=$null;sha256=$null;
        license=$License;status='unavailable_unverified'}
}
$tools = [ordered]@{
    git=(Observed git '2.49.0.windows.1' 'https://github.com/git-for-windows/git/releases/tag/v2.49.0.windows.1' 'GPL-2.0');
    cmake=(Observed cmake '3.31.7' 'https://github.com/Kitware/CMake/releases/tag/v3.31.7' 'BSD-3-Clause');
    ninja=(Observed ninja '1.12.1' 'https://github.com/ninja-build/ninja/releases/tag/v1.12.1' 'Apache-2.0');
    compiler=(Observed cl '19.43.34809.0' 'https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options' 'Microsoft Visual Studio installed terms; not newly accepted');
    python=(Observed python '3.13.9' 'https://www.python.org/downloads/release/python-3139/' 'PSF; installed Anaconda distribution terms not independently verified');
    godot=(Observed godot '4.6.2.stable.official.71f334935' 'https://github.com/godotengine/godot/releases/tag/4.6.2-stable' 'MIT');
    godot_required=(Missing '4.7.2-stable' 'https://github.com/godotengine/godot/releases/tag/4.7.2-stable' 'MIT');
    godot_cpp=(Missing $null 'https://github.com/godotengine/godot-cpp' 'MIT');
    eigen=(Missing $null 'https://gitlab.com/libeigen/eigen' 'MPL-2.0');
    test_framework=(Missing $null 'https://github.com/catchorg/Catch2' 'BSL-1.0');
    blender=(Missing $null 'https://download.blender.org/release/' 'GPL-3.0-or-later');
    jsonschema=(Missing '4.25.0' 'https://pypi.org/project/jsonschema/4.25.0/' 'MIT')
}
$tools.jsonschema.status='installed_provenance_unverified'
$formatPath = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin'
foreach ($tool in @('clang-format','clang-tidy')) {
    $path = Join-Path $formatPath "$tool.exe"
    $tools[$tool] = [ordered]@{version='19.1.1';source_url='https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.1';
        url=$null;sha256=$null;local_path=$path;local_sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower();
        license='Apache-2.0 WITH LLVM-exception';status='installed_provenance_unverified'}
}
$lock=[ordered]@{schema=1;checked_at='2026-11-14';host='windows-x86_64';
    complete=$false;note='Observed binary hashes only. Official archive hashes and missing dependencies unresolved due DNS failure.';
    tools=$tools}
$lock | ConvertTo-Json -Depth 8 | Set-Content toolchain.lock.json -Encoding utf8
