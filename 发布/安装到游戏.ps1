param([string]$GameRoot = 'F:\SteamLibrary\steamapps\common\Monster Hunter World')
$ErrorActionPreference = 'Stop'
function FileHash([string]$filePath) {
    $hashObject = [System.Security.Cryptography.SHA256]::Create()
    try { return [System.BitConverter]::ToString($hashObject.ComputeHash([System.IO.File]::ReadAllBytes($filePath))).Replace('-','') }
    finally { $hashObject.Dispose() }
}

$targetRoot = [System.IO.Path]::GetFullPath($GameRoot)
if (-not (Test-Path -LiteralPath (Join-Path $targetRoot 'MonsterHunterWorld.exe'))) { throw '此目录中没有 MonsterHunterWorld.exe，请用 -GameRoot 指定正确的游戏根目录。' }
if (-not (Test-Path -LiteralPath (Join-Path $targetRoot 'loader.dll'))) { throw "请先为游戏安装 Stracker's Loader。" }
$sourceDll = Join-Path $PSScriptRoot 'WorldDiff.dll'
if (-not (Test-Path -LiteralPath $sourceDll)) { throw '发布文件不完整：缺少 WorldDiff.dll。' }
$plugins = Join-Path $targetRoot 'nativePC\plugins'
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
$destination = Join-Path $plugins 'WorldDiff.dll'
$sourceHash = (FileHash $sourceDll)
if (Test-Path -LiteralPath $destination) {
    $installedHash = (FileHash $destination)
    if ($installedHash -eq $sourceHash) { Write-Host '同版本 DLL 已安装。打开 WorldDiffManager.exe 即可。'; exit 0 }
    if (Get-Process MonsterHunterWorld -ErrorAction SilentlyContinue) { throw '游戏正在运行且已安装其他版本。请先正常退出游戏，再执行安装。' }
    $backup = Join-Path $PSScriptRoot ('安装备份\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    Copy-Item -LiteralPath $destination -Destination (Join-Path $backup 'WorldDiff.dll')
}
Copy-Item -LiteralPath $sourceDll -Destination $destination -Force
if ((FileHash $destination) -ne $sourceHash) { throw '安装后的哈希不符，请重试。' }
Write-Host 'WorldDiff 1.0.2 安装成功。'
Write-Host '插件将在下次启动游戏时自动加载。然后打开本目录的 WorldDiffManager.exe。'
Write-Host '若本次游戏已经连接到 1.0.2，无需立即重启。'
