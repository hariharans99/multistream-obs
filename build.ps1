# build.ps1 - OBS Multi-Stream Plugin Build Script (Enhanced VS2022 Support)
# Handles the full build process for the plugin on Windows using Visual Studio 2022.

param(
    [switch]$Clean,
    [string]$InstallPath = "C:\Program Files\obs-studio",
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"

# ── Resolve paths ─────────────────────────────────────────────────────────────
$SCRIPT_DIR  = $PSScriptRoot
$DEPS_DIR    = "$SCRIPT_DIR\.deps"
$BUILD_DIR   = "$SCRIPT_DIR\build_vs"
$OBS_SRC_DIR = "$DEPS_DIR\obs-studio-31.1.1"
$OBS_BLD_DIR = "$OBS_SRC_DIR\build_vs"

# ── Step 0: Detect Visual Studio 2022 Environment ────────────────────────────
Write-Host "Detecting Visual Studio 2022 environment..." -ForegroundColor Cyan

$VS_PATH = ""
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vswhere) {
    $VS_PATH = & $vswhere -version "[17.0,18.0)" -latest -property installationPath
}

if (-not $VS_PATH) {
    # Fallback to common paths
    $commonPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
    )
    foreach ($path in $commonPaths) {
        if (Test-Path "$path\VC\Auxiliary\Build\vcvars64.bat") {
            $VS_PATH = $path
            break
        }
    }
}

if (-not $VS_PATH) {
    Write-Error "Visual Studio 2022 not detected! Please install 'Desktop development with C++' workload."
    exit 1
}

$VCVARS = "$VS_PATH\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $VCVARS)) {
    Write-Error "vcvars64.bat not found at: $VCVARS. Please ensure MSVC v143 build tools are installed."
    exit 1
}

Write-Host "Found Visual Studio 2022 at: $VS_PATH" -ForegroundColor Green

# ── Helper: run commands inside vcvars64 environment ──────────────────────────
function Invoke-WithVCVars([string]$cmd) {
    $full = "`"$VCVARS`" && $cmd"
    cmd /c $full
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}"
    }
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  OBS Multi-Stream Plugin — Professional Build System" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# ── Step 1: Download deps if not present ─────────────────────────────────────
Write-Host "[1/4] Downloading OBS dependencies (if needed)..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $DEPS_DIR | Out-Null

function Download-Dep([string]$url, [string]$dest, [string]$label) {
    if (-not (Test-Path $dest)) {
        Write-Host "  Downloading $label..."
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
        Write-Host "  Downloaded $label ✓" -ForegroundColor Green
    } else {
        Write-Host "  $label already downloaded ✓" -ForegroundColor Green
    }
}

$BASE_URL = "https://github.com/obsproject/obs-deps/releases/download/2025-07-11"
$OBS_TAG  = "31.1.1"

Download-Dep "$BASE_URL/windows-deps-2025-07-11-x64.zip"      "$DEPS_DIR\windows-deps-2025-07-11-x64.zip"      "obs-deps"
Download-Dep "$BASE_URL/windows-deps-qt6-2025-07-11-x64.zip"  "$DEPS_DIR\windows-deps-qt6-2025-07-11-x64.zip"  "Qt6"
Download-Dep "https://github.com/obsproject/obs-studio/archive/refs/tags/$OBS_TAG.zip" "$DEPS_DIR\$OBS_TAG.zip" "OBS source"

# ── Step 2: Extract deps ───────────────────────────────────────────────────────
Write-Host "[2/4] Extracting dependencies..." -ForegroundColor Yellow

function Extract-Dep([string]$archive, [string]$destDir, [string]$label) {
    if (-not (Test-Path $destDir)) {
        Write-Host "  Extracting $label..."
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        Expand-Archive -Path $archive -DestinationPath $destDir -Force
        Write-Host "  Extracted $label ✓" -ForegroundColor Green
    } else {
        Write-Host "  $label already extracted ✓" -ForegroundColor Green
    }
}

Extract-Dep "$DEPS_DIR\windows-deps-2025-07-11-x64.zip"      "$DEPS_DIR\obs-deps-2025-07-11-x64"      "obs-deps"
Extract-Dep "$DEPS_DIR\windows-deps-qt6-2025-07-11-x64.zip"  "$DEPS_DIR\obs-deps-qt6-2025-07-11-x64"  "Qt6"

if (-not (Test-Path $OBS_SRC_DIR)) {
    Write-Host "  Extracting OBS source..."
    Expand-Archive -Path "$DEPS_DIR\$OBS_TAG.zip" -DestinationPath $DEPS_DIR -Force
    Write-Host "  Extracted OBS source ✓" -ForegroundColor Green
} else {
    Write-Host "  OBS source already extracted ✓" -ForegroundColor Green
}

# ── Step 3: Build OBS (libobs + obs-frontend-api only) ───────────────────────
Write-Host "[3/4] Building OBS SDK (libobs + obs-frontend-api)..." -ForegroundColor Yellow

$PREFIX_PATH = "$DEPS_DIR\obs-deps-2025-07-11-x64;$DEPS_DIR\obs-deps-qt6-2025-07-11-x64"
$GENERATOR = "Visual Studio 17 2022"

if (-not (Test-Path "$DEPS_DIR\lib\obs.lib")) {
    if ($Clean -and (Test-Path $OBS_BLD_DIR)) {
        Remove-Item $OBS_BLD_DIR -Recurse -Force
    }

    Write-Host "  Configuring OBS source with $GENERATOR..."
    $configCmd = "cmake -S `"$OBS_SRC_DIR`" -B `"$OBS_BLD_DIR`" -G `"$GENERATOR`" -A x64 " +
                 "-DOBS_CMAKE_VERSION=3.0.0 " +
                 "-DENABLE_PLUGINS=OFF -DENABLE_FRONTEND=OFF " +
                 "-DCMAKE_ENABLE_SCRIPTING=OFF " +
                 "-DOBS_VERSION_OVERRIDE=$OBS_TAG " +
                 "-DCMAKE_PREFIX_PATH=`"$PREFIX_PATH`""

    Invoke-WithVCVars $configCmd

    Write-Host "  Building libobs..."
    Invoke-WithVCVars "cmake --build `"$OBS_BLD_DIR`" --target libobs --config $Config"

    Write-Host "  Building obs-frontend-api..."
    Invoke-WithVCVars "cmake --build `"$OBS_BLD_DIR`" --target obs-frontend-api --config $Config"

    Write-Host "  Installing OBS SDK to deps dir..."
    Invoke-WithVCVars "cmake --install `"$OBS_BLD_DIR`" --component Development --config $Config --prefix `"$DEPS_DIR`""

    Write-Host "  OBS SDK built ✓" -ForegroundColor Green
} else {
    Write-Host "  OBS SDK already built ✓" -ForegroundColor Green
}

# ── Step 4: Build the plugin ──────────────────────────────────────────────────
Write-Host "[4/4] Building obs-multistream-plugin..." -ForegroundColor Yellow

if ($Clean -and (Test-Path $BUILD_DIR)) {
    Remove-Item $BUILD_DIR -Recurse -Force
}

$pluginPrefixPath = "$DEPS_DIR;$DEPS_DIR\obs-deps-2025-07-11-x64;$DEPS_DIR\obs-deps-qt6-2025-07-11-x64"

$configPluginCmd = "cmake -S `"$SCRIPT_DIR`" -B `"$BUILD_DIR`" -G `"$GENERATOR`" -A x64 " +
                   "-DENABLE_FRONTEND_API=ON -DENABLE_QT=ON " +
                   "-DSKIP_DEPENDENCIES=ON " +
                   "-DCMAKE_PREFIX_PATH=`"$pluginPrefixPath`" " +
                   "-Dlibobs_DIR=`"$DEPS_DIR\lib\cmake\libobs`" " +
                   "-Dobs-frontend-api_DIR=`"$DEPS_DIR\lib\cmake\obs-frontend-api`""

Write-Host "  Configuring plugin with $GENERATOR..."
Invoke-WithVCVars $configPluginCmd

Write-Host "  Building plugin..."
Invoke-WithVCVars "cmake --build `"$BUILD_DIR`" --config $Config"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  Build complete!" -ForegroundColor Green
Write-Host "  Plugin DLL: $BUILD_DIR\$Config\obs-multistream-plugin.dll" -ForegroundColor Green
Write-Host ""
Write-Host "  To install:" -ForegroundColor Cyan
Write-Host "    Copy obs-multistream-plugin.dll to:" -ForegroundColor Cyan
Write-Host "    $InstallPath\obs-plugins\64bit\" -ForegroundColor White
Write-Host "    Copy data\ to:" -ForegroundColor Cyan
Write-Host "    $InstallPath\data\obs-plugins\obs-multistream-plugin\" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor Green
