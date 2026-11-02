param([string]$GamePath = 'F:\SteamLibrary\steamapps\common\Monster Hunter World')
$ErrorActionPreference = 'Stop'
try {
    $gameRoot = [IO.Path]::GetFullPath($GamePath)
    if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'MonsterHunterWorld.exe'))) {
        throw '未找到 MonsterHunterWorld.exe，请把游戏根目录作为参数传入。'
    }
    if (Get-Process -Name MonsterHunterWorld -ErrorAction SilentlyContinue) {
        throw '请先退出游戏，再安装插件。'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'loader.dll'))) {
        throw "请先安装适用于当前游戏版本的 Stracker's Loader。"
    }
    $source = Join-Path $PSScriptRoot 'nativePC\plugins\WorldDiff.dll'
    if (-not (Test-Path -LiteralPath $source)) { throw '安装包缺少 WorldDiff.dll。' }
    $plugins = Join-Path $gameRoot 'nativePC\plugins'
    $dataDir = Join-Path $plugins 'WorldDiff'
    New-Item -ItemType Directory -Path $plugins -Force | Out-Null
    $backup = Join-Path $gameRoot ('WorldDiff-backups\before-1.0.5-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $target = Join-Path $plugins 'WorldDiff.dll'
    if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
    if (Test-Path -LiteralPath $dataDir) { Copy-Item -LiteralPath $dataDir -Destination $backup -Recurse }
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw '安装后的 DLL 校验不一致，请重新安装。'
    }
    Write-Host '1.0.5 安装完成，启动游戏后按 F9 打开。'
    Write-Host ('原文件备份：' + $backup)
    Write-Host '原有配置由插件自动迁移；无需运行外置管理器。'
} catch {
    Write-Host ('安装失败：' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
