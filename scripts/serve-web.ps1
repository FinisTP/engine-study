# Serves the browser build on http://localhost:8080/ (roadmap P10). No install, no admin rights:
# localhost counts as a secure origin, so WebGPU, IndexedDB saves and the offline install work.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\serve-web.ps1 [folder] [port]
param([string]$Folder = "$PSScriptRoot\..\out\web", [int]$Port = 8080)

$root = (Resolve-Path $Folder).Path
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
