$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$QtBin = "D:\Qt\6.11.1\msvc2022_64\bin"
$AppName = "MotionController"
$ExePath = Join-Path $ProjectRoot "x64\Release\$AppName.exe"
$PackageDir = Join-Path $ProjectRoot "package\$AppName"
$PackageExe = Join-Path $PackageDir "$AppName.exe"
$Windeployqt = Join-Path $QtBin "windeployqt.exe"
$IssFile = Join-Path $ProjectRoot "installer\MotionController.iss"

if (-not (Test-Path $ExePath)) {
    throw "Release exe not found: $ExePath. Build Release x64 in Visual Studio first."
}

if (-not (Test-Path $Windeployqt)) {
    throw "windeployqt not found: $Windeployqt"
}

if (Test-Path $PackageDir) {
    Remove-Item $PackageDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item $ExePath $PackageExe -Force
Copy-Item (Join-Path $ProjectRoot "ThirdParty\Zmotion\x64\*.dll") $PackageDir -Force

& $Windeployqt --release --compiler-runtime $PackageExe

$Iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if (-not $Iscc) {
    $CandidatePaths = @(
        "D:\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
    )

    foreach ($CandidatePath in $CandidatePaths) {
        if ($CandidatePath -and (Test-Path $CandidatePath)) {
            $Iscc = [pscustomobject]@{ Source = $CandidatePath }
            break
        }
    }
}

if (-not $Iscc) {
    Write-Host "Package folder is ready: $PackageDir"
    Write-Host "Inno Setup compiler was not found. Install Inno Setup 6, then compile: $IssFile"
    exit 0
}

& $Iscc.Source $IssFile
