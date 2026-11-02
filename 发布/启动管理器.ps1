$ErrorActionPreference = 'Stop'
function FileHash([string]$filePath) {
    $hashObject = [System.Security.Cryptography.SHA256]::Create()
    try { return [System.BitConverter]::ToString($hashObject.ComputeHash([System.IO.File]::ReadAllBytes($filePath))).Replace('-','') }
    finally { $hashObject.Dispose() }
}

# Standard per-user installation. New files inherit the destination folder's label.
$runtimeDir = Join-Path $env:LOCALAPPDATA 'WorldDiffManager\1.0.2'
New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null
foreach ($fileName in @('WorldDiffManager.exe','WorldDiff.dll')) {
    $source = Join-Path $PSScriptRoot $fileName
    if (-not (Test-Path -LiteralPath $source)) { throw "发布文件不完整：$fileName" }
    $destination = Join-Path $runtimeDir $fileName
    if ((Test-Path -LiteralPath $destination) -and (FileHash $source) -eq (FileHash $destination)) { continue }
    $temp = Join-Path $runtimeDir ($fileName + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
    [System.IO.File]::WriteAllBytes($temp,[System.IO.File]::ReadAllBytes($source))
    Move-Item -LiteralPath $temp -Destination $destination -Force
}
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '使用说明.md')) {
    [System.IO.File]::WriteAllBytes((Join-Path $runtimeDir '使用说明.md'),[System.IO.File]::ReadAllBytes((Join-Path $PSScriptRoot '使用说明.md')))
}
Start-Process -FilePath (Join-Path $runtimeDir 'WorldDiffManager.exe') -WorkingDirectory $runtimeDir -WindowStyle Normal
