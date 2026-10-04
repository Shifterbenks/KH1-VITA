$ErrorActionPreference = "Stop"
if (-not $env:VITASDK) { throw "Definis la variable VITASDK avant le build." }
if (Test-Path build) { Remove-Item build -Recurse -Force }
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
Write-Host "VPK: build/KH1VITA.vpk"
