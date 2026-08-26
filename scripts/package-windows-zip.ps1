# Package a portable Windows ZIP for distribution.
# Usage: package-windows-zip.ps1 -Binary <path.exe> -DataDir <data> -OutDir <out> [-ArchLabel x64]
param(
    [Parameter(Mandatory = $true)][string]$Binary,
    [Parameter(Mandatory = $true)][string]$DataDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$ArchLabel = "x64"
)

$ErrorActionPreference = "Stop"

$Binary = (Resolve-Path $Binary).Path
$DataDir = (Resolve-Path $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

$AppName = "Stunt Car Racer for Lovers"
$ZipName = "StuntCarRacerForLovers-Windows-$ArchLabel.zip"
$ZipPath = Join-Path $OutDir $ZipName
$Stage = Join-Path $OutDir "stage-windows-$ArchLabel"
$GameDir = Join-Path $Stage $AppName

if (Test-Path $Stage) {
    Remove-Item -Recurse -Force $Stage
}
New-Item -ItemType Directory -Force -Path $GameDir | Out-Null

Copy-Item $Binary (Join-Path $GameDir "stuntcarracer.exe")
Copy-Item -Recurse $DataDir (Join-Path $GameDir "data")

@'
Stunt Car Racer for Lovers — Windows (64-bit)

1. Extract the ZIP anywhere (e.g. Downloads).
2. Open the folder "Stunt Car Racer for Lovers".
3. Double-click stuntcarracer.exe to play.

First launch — Windows SmartScreen (unsigned build):
  Click "More info", then "Run anyway".

Controls: U Amiga+ physics · I Speed feel · O Enhanced Look · P Pause · F11 fullscreen · N menu music

Play in browser: https://nmarchand73.github.io/StuntCarRacerForLovers/play/
'@ | Set-Content -Path (Join-Path $GameDir "HOW-TO-OPEN.txt") -Encoding utf8

if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $ZipPath

Write-Host "Packaged $ZipPath"
Get-Item $ZipPath | Format-List Name, Length, LastWriteTime
