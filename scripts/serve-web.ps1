# Serves the browser build on http://localhost:8080/ (roadmap P10). No install, no admin rights:
# localhost counts as a secure origin, so WebGPU, IndexedDB saves and the offline install work.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\serve-web.ps1 [folder] [port]
param([string]$Folder = "$PSScriptRoot\..\out\web", [int]$Port = 8080)

$root = (Resolve-Path $Folder).Path

# 0. Pack the project's current assets next to the page, so editor changes show up without a
#    rebuild (C++ changes still need one). Uses the editor build's UmbraTool when there is one.
$tool = @("$PSScriptRoot\..\out\build\windows-msvc\tools\umbratool\Debug\UmbraTool.exe",
          "$PSScriptRoot\..\out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe") |
        Where-Object { Test-Path $_ } | Sort-Object { (Get-Item $_).LastWriteTime } | Select-Object -Last 1
if ($tool) {
    & $tool web-pack "$PSScriptRoot\..\assets" $root
    if ($LASTEXITCODE -ne 0) { Write-Host "web-pack failed: the page keeps the assets it had"; }
} else {
    Write-Host "UmbraTool not built yet (build the editor once): serving the assets from the last web build"
}
$types = @{ ".html" = "text/html"; ".js" = "text/javascript"; ".css" = "text/css"; ".wasm" = "application/wasm"; ".data" = "application/octet-stream"; ".json" = "application/json" }
$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
Write-Host "Umbra web build: http://localhost:$Port/  (Ctrl+C to stop)"
Start-Process "http://localhost:$Port/"
while ($listener.IsListening) {
    $context = $listener.GetContext()
    # 1. Map the URL to a file inside the folder (nothing outside it).
    $name = [Uri]::UnescapeDataString($context.Request.Url.AbsolutePath.TrimStart("/"))
    if ($name -eq "") { $name = "index.html" }
    $path = [IO.Path]::GetFullPath((Join-Path $root $name))
    $response = $context.Response
    if ($path.StartsWith($root) -and (Test-Path $path -PathType Leaf)) {
        # 2. Send it with the right type (application/wasm lets the browser compile while downloading).
        $bytes = [IO.File]::ReadAllBytes($path)
        $type = $types[[IO.Path]::GetExtension($path).ToLower()]
        $response.ContentType = $(if ($type) { $type } else { "application/octet-stream" })
        $response.ContentLength64 = $bytes.Length # a known length (some browsers refuse service worker scripts without it)
        $response.Headers.Add("Cache-Control", "no-cache") # always check for a newer build
        $response.OutputStream.Write($bytes, 0, $bytes.Length)
    } else {
        $response.StatusCode = 404
    }
    $response.Close()
}
