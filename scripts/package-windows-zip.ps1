# Package a portable Windows ZIP for distribution.
# Usage: package-windows-zip.ps1 -Binary <path.exe> -DataDir <data> -OutDir <out> [-ArchLabel x64]
param(
    [Parameter(Mandatory = $true)][string]$Binary,
    [Parameter(Mandatory = $true)][string]$DataDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$ArchLabel = "x64"
)

$ErrorActionPreference = "Stop"

function Copy-VcRuntimeDlls {
    param(
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $required = @('VCRUNTIME140.dll', 'VCRUNTIME140_1.dll', 'MSVCP140.dll')
    $copied = @{}

    $searchRoots = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022"
    )

    $redistDirs = foreach ($root in $searchRoots) {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $crtRoot = Join-Path $_.FullName "VC\Redist\MSVC"
            if (-not (Test-Path $crtRoot)) { return }
            Get-ChildItem -Path $crtRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $archDir = Join-Path $_.FullName $ArchLabel
                if (-not (Test-Path $archDir)) { return }
                Get-ChildItem -Path $archDir -Filter 'Microsoft.VC*.CRT' -Directory -ErrorAction SilentlyContinue
            }
        }
    }

    foreach ($dir in $redistDirs) {
        foreach ($name in $required) {
            if ($copied.ContainsKey($name)) { continue }
            $src = Join-Path $dir.FullName $name
            if (Test-Path $src) {
                Copy-Item $src (Join-Path $Destination $name)
                $copied[$name] = $true
                Write-Host "Bundled $name"
            }
        }
    }

    $missing = $required | Where-Object { -not $copied.ContainsKey($_) }
    if ($missing.Count -gt 0) {
        Write-Warning "Could not locate VC runtime DLL(s): $($missing -join ', ')"
    }
}

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
Copy-VcRuntimeDlls -Destination $GameDir

@'
Stunt Car Racer for Lovers — Windows (64-bit)

1. Extract the ZIP anywhere (e.g. Downloads).
2. Open the folder "Stunt Car Racer for Lovers".
3. Double-click stuntcarracer.exe to play.

First launch — Windows SmartScreen (unsigned build):
  Click "More info", then "Run anyway".

If you see a missing VCRUNTIME140_1.dll error:
  Re-download the latest ZIP from the project page, or install the
  Microsoft Visual C++ 2015–2022 Redistributable (x64):
  https://aka.ms/vcredist/x64

Controls: U Amiga+ physics · I Speed feel · O Enhanced Look · P Pause · F11 fullscreen · N menu music

Play in browser: https://nmarchand73.github.io/StuntCarRacerForLovers/play/
'@ | Set-Content -Path (Join-Path $GameDir "HOW-TO-OPEN.txt") -Encoding utf8

if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $ZipPath

Write-Host "Packaged $ZipPath"
Get-Item $ZipPath | Format-List Name, Length, LastWriteTime
