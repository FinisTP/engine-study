# Builds the game for Android and starts it on the emulator or a connected phone (roadmap P11).
# Usage: powershell -ExecutionPolicy Bypass -File build-android.ps1 [-Abis x86_64] [-Project reference] [-NoRun]
#   -Abis x86_64     the Android emulator on a PC (default); arm64-v8a for phones; both: "x86_64,arm64-v8a"
# Needs: Android Studio (SDK, NDK, CMake 3.28+, a virtual device), VCPKG_ROOT, and the PC build
# (out\build\windows-msvc) for UmbraTool and the compiled shaders. See ..\README.md.
param([string]$Abis = "x86_64", [string]$Project = "reference", [switch]$NoRun)

$repo = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$app = "$repo\plugins\android-export\android"
$log = "$repo\out\android.log"
New-Item -ItemType Directory -Force "$repo\out" | Out-Null
function Fail($message) { Write-Host "ANDROID BUILD FAILED: $message"; exit 1 }
function Newest($folder, $minimum) {
    Get-ChildItem $folder -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -as [version] -and [version]$_.Name -ge [version]$minimum } |
        Sort-Object { [version]$_.Name } | Select-Object -Last 1
}

# 1. Tools: the Android SDK with its NDK and CMake, Java (Android Studio's own), vcpkg.
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$adb = "$sdk\platform-tools\adb.exe"
if (-not (Test-Path $adb)) { Fail "no Android SDK at $sdk. Install Android Studio and open it once (or set ANDROID_HOME)." }
$ndk = Newest "$sdk\ndk" "26.0"
if (-not $ndk) { Fail "no NDK. Android Studio > Tools > SDK Manager > SDK Tools > NDK (Side by side)." }
$cmake = Newest "$sdk\cmake" "3.28"
if (-not $cmake) { Fail "no CMake 3.28 or newer. SDK Manager > SDK Tools > tick 'Show Package Details' > CMake 3.31 (or newer)." }
if (-not $env:JAVA_HOME) {
    $studio = @("$env:ProgramFiles\Android\Android Studio\jbr", "$env:LOCALAPPDATA\Programs\Android Studio\jbr", "E:\Android\Android Studio\jbr") |
              Where-Object { Test-Path "$_\bin\java.exe" } | Select-Object -First 1
    if (-not $studio) { Fail "Java not found. Set JAVA_HOME to Android Studio's jbr folder (e.g. C:\Program Files\Android\Android Studio\jbr)." }
    $env:JAVA_HOME = $studio
}
if (-not $env:VCPKG_ROOT -or -not (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) { Fail "VCPKG_ROOT is not set to a vcpkg folder (the PC build uses it too)." }
$env:ANDROID_NDK_HOME = $ndk.FullName # vcpkg's Android triplets find the NDK here
Set-Content "$app\local.properties" ("sdk.dir=" + ($sdk -replace '\\', '\\' -replace ':', '\:')) -Encoding ASCII
Write-Host "SDK $sdk | NDK $($ndk.Name) | CMake $($cmake.Name) | ABIs $Abis | project $Project"

# 2. game.pack: the project's assets (plugins' assets included) and the shaders the PC build compiled.
$build = "$repo\out\build\windows-msvc"
$tool = @("$build\tools\umbratool\Debug\UmbraTool.exe", "$build\tools\umbratool\Release\UmbraTool.exe") |
        Where-Object { Test-Path $_ } | Sort-Object { (Get-Item $_).LastWriteTime } | Select-Object -Last 1
$shaders = "$build\engine\render\shaders"
if (-not $tool -or -not (Test-Path "$shaders\Scene.vert.spv")) { Fail "build the PC game first (scripts\build-and-run.bat): it makes UmbraTool and the shaders." }
& $tool game-pack "$repo\projects\$Project\assets" $shaders "$app\app\build\generated\gamepack"
if ($LASTEXITCODE -ne 0) { Fail "game-pack found problems (above)." }

# 3. The app. The first build also builds the libraries for Android with vcpkg (takes a while).
Write-Host "Building the app (log: $log) ..."
& "$app\gradlew.bat" -p $app assembleDebug "-PumbraAbis=$Abis" "-PumbraNdk=$($ndk.Name)" "-PumbraCmake=$($cmake.Name)" "-PumbraProject=$Project" 2>&1 |
    ForEach-Object { "$_" } | Tee-Object -FilePath $log # "$_": Gradle's error lines as plain text
if ($LASTEXITCODE -ne 0) { Fail "Gradle failed, see $log" }
$apk = "$app\app\build\outputs\apk\debug\app-debug.apk"
Write-Host "APK: $apk"
if ($NoRun) { exit 0 }

# 4. A device: a connected phone or a running emulator; otherwise start the first virtual device.
$devices = & $adb devices | Select-String "\tdevice$"
if (-not $devices) {
    $avd = & "$sdk\emulator\emulator.exe" -list-avds | Select-Object -First 1
    if (-not $avd) { Fail "no virtual device. Android Studio > Device Manager > create one (x86_64 image, API 30 or newer)." }
    Write-Host "Starting the emulator $avd ..."
    Start-Process "$sdk\emulator\emulator.exe" -ArgumentList "-avd", $avd
    & $adb wait-for-device
    for ($i = 0; $i -lt 90 -and "$(& $adb shell getprop sys.boot_completed 2>$null)".Trim() -ne "1"; $i++) { Start-Sleep 2 }
}

# 5. Install, start, and show the game's log (Ctrl+C stops the log, not the game).
& $adb install -r $apk
if ($LASTEXITCODE -ne 0) { Fail "adb install failed (an arm64 APK on an x86_64 emulator? use -Abis to match)." }
& $adb logcat -c
& $adb shell am start -n com.umbra.game/.UmbraActivity
Write-Host "Started. Game log (tags Umbra, SDL, crashes):"
& $adb logcat -s Umbra:V SDL:V SDL/APP:V AndroidRuntime:E DEBUG:V
