# Builds and runs test/static_smoke.c against a vcpkg x64-windows-static install
# root, then asserts the resulting exe has no ffmpeg DLL imports.
#
#   pwsh test/run-smoke.ps1 -Root vcpkg_installed/x64-windows-static
#
# Needs MSVC (cl.exe/lib.exe/dumpbin.exe, found through vswhere) and the avicap32
# import library this script generates from test/avicap32.def — the Windows SDK
# doesn't ship it and avdevice's vfwcap needs it, exactly like the app's build.rs.
param(
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

if ($OutDir -eq "") {
    $OutDir = Join-Path (Split-Path $PSScriptRoot -Parent) "build-smoke"
}
$root = (Resolve-Path $Root).Path
$out = New-Item -ItemType Directory -Force -Path $OutDir | Select-Object -ExpandProperty FullName
$src = (Resolve-Path (Join-Path $PSScriptRoot "static_smoke.c")).Path
$def = (Resolve-Path (Join-Path $PSScriptRoot "avicap32.def")).Path

if (-not (Test-Path (Join-Path $root "lib\avcodec.lib"))) {
    throw "not a static ffmpeg install root (lib\avcodec.lib missing): $root"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: $vswhere" }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "no Visual Studio installation with the C++ toolset" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

# The app links exactly this set (src/build.rs): ffmpeg libs plus libvpx, zlib
# and the Windows libraries avdevice/vfwcap and the vp9 encoder pull in.
$libs = @(
    "avcodec.lib", "avdevice.lib", "avfilter.lib", "avformat.lib", "avutil.lib",
    "swresample.lib", "swscale.lib", "vpx.lib", "zs.lib",
    "strmiids.lib", "mfuuid.lib", "uuid.lib", "winmm.lib", "ws2_32.lib", "secur32.lib",
    "bcrypt.lib", "user32.lib", "avicap32.lib", "msvfw32.lib", "gdi32.lib",
    "oleaut32.lib", "shlwapi.lib", "psapi.lib", "ncrypt.lib", "crypt32.lib"
)

$cmd = Join-Path $out "smoke.cmd"
$log = Join-Path $out "smoke.log"
$lines = @(
    "@echo off",
    "call `"$vcvars`" >nul || exit /b 1",
    "cd /d `"$out`"",
    "lib /nologo /def:`"$def`" /machine:x64 /out:avicap32.lib || exit /b 1",
    "cl /nologo /MT /O2 /W3 /I `"$root\include`" `"$src`" /Fe:static_smoke.exe /link /LIBPATH:`"$root\lib`" /LIBPATH:. $($libs -join ' ') || exit /b 1",
    "static_smoke.exe || exit /b 1",
    "dumpbin /nologo /dependents static_smoke.exe || exit /b 1"
)
Set-Content -Path $cmd -Value $lines -Encoding ASCII

Write-Host "building against $root"
# Log to a file instead of capturing stderr: ffmpeg writes to stderr and
# PowerShell would turn those lines into terminating error records.
& cmd /c "`"$cmd`" > `"$log`" 2>&1"
$exit = $LASTEXITCODE
$output = Get-Content -Path $log -Raw
Write-Host $output
if ($exit -ne 0) { throw "smoke build/run failed with exit code $exit" }

# 1. Functional: the C program already proved a 10-bit VP9 encode and the webm
#    muxer. Additionally fingerprint libvpx's high bit depth code, which is the
#    defect this package exists to avoid.
& findstr /M /C:"vp9_highbd" (Join-Path $root "lib\vpx.lib") | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "libvpx in this package has no high bit depth code (built without the highbitdepth feature)"
}

# 2. The exe must not import any ffmpeg DLL — "static" is the product promise.
$imports = @($output -split "`n" | Where-Object { $_ -match "(?i)\b(av(codec|device|filter|format|util)|sw(scale|resample)|vpx|zs|zlib)\d*\.dll" })
if ($imports.Count -gt 0) {
    throw "static_smoke.exe imports ffmpeg DLLs: $($imports -join ', ')"
}

Write-Host "static smoke test passed"
