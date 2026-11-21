# Builds the C++ scheduler for the browser: web\clashfree.js + web\clashfree.wasm
# Run from the project folder (the one with main.cpp), in a PowerShell where emsdk_env was run:
#     powershell -ExecutionPolicy Bypass -File .\web\build_wasm.ps1
#
# Flags:
#   -lembind                makes runSchedule() callable from the page
#   -fexceptions            lets C++ catch errors (Emscripten turns this off by default);
#                           without it, bad data would crash the page instead of showing a message
#   -sMODULARIZE=1 -sEXPORT_NAME=createClashFree   the page starts it with createClashFree()
#   -sALLOW_MEMORY_GROWTH=1 larger datasets can use more memory
#   -sENVIRONMENT=web       only the browser version is needed
$ErrorActionPreference = "Stop"
if (-not (Get-Command em++ -ErrorAction SilentlyContinue)) {
    throw "em++ not found. Run emsdk_env.ps1 (from your emsdk folder) in this window first."
}
em++ -std=c++17 -O2 -I. web/clashfree_wasm.cpp -o web/clashfree.js `
    -lembind -fexceptions `
    -sMODULARIZE=1 -sEXPORT_NAME=createClashFree `
    -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web
if ($LASTEXITCODE -ne 0) { throw "em++ failed" }
Write-Host "[build] ok: web\clashfree.js + web\clashfree.wasm"
