param([string]$BuildDir = 'build-windows')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo
# Requires Visual Studio 2022 Desktop development with C++ and CMake >= 3.22.
cmake -S . -B $BuildDir -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build $BuildDir --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& "$BuildDir/Release/acid_render.exe" "$BuildDir/renders"
if ($LASTEXITCODE -ne 0) { throw 'DSP checks failed' }
& "$BuildDir/acid_snapshot_artefacts/Release/acid_snapshot.exe" "$BuildDir/snapshots" | Tee-Object "$BuildDir/snapshot.log"
if ($LASTEXITCODE -ne 0) { throw 'GUI/processor checks failed' }
if (Select-String -Path "$BuildDir/snapshot.log" -Pattern 'FAILED|NON-FINITE' -Quiet) { throw 'A processor check reported failure' }
$package = Join-Path $BuildDir 'Punk-Fish-Windows-x64'
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item -Recurse -Force "$BuildDir/TB303_DevilAcid_artefacts/Release/VST3/Punk Fish.vst3" $package
Copy-Item -Force "$BuildDir/TB303_DevilAcid_artefacts/Release/Standalone/Punk Fish.exe" $package
Copy-Item docs/PUNK_FISH_BUILD.md $package
Compress-Archive -Force "$package/*" "$BuildDir/Punk-Fish-Windows-x64.zip"
Write-Host "Built and checked: $BuildDir/Punk-Fish-Windows-x64.zip"
