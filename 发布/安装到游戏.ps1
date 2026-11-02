param(
    [string]$GamePath = 'F:\SteamLibrary\steamapps\common\Monster Hunter World',
    [string]$MeshOrderRoot = ''
)
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
    $orderFiles = @()
    if ($MeshOrderRoot) {
        $orderRoot = [IO.Path]::GetFullPath($MeshOrderRoot)
        $orderFolder = Join-Path $orderRoot 'nativePC\pl'
        if (-not (Test-Path -LiteralPath $orderFolder)) { throw '网格顺序目录缺少 nativePC\pl。' }
        $orderFiles = @(Get-ChildItem -LiteralPath $orderFolder -Recurse -File -Filter '*.meshorder')
        if (-not $orderFiles.Count) { throw '未找到网格顺序文件。' }
    }
    $plugins = Join-Path $gameRoot 'nativePC\plugins'
    $dataDir = Join-Path $plugins 'WorldDiff'
    New-Item -ItemType Directory -Path $plugins -Force | Out-Null
    $backup = Join-Path $gameRoot ('WorldDiff-backups\before-1.1.4-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $target = Join-Path $plugins 'WorldDiff.dll'
    if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
    if (Test-Path -LiteralPath $dataDir) { Copy-Item -LiteralPath $dataDir -Destination $backup -Recurse }
    $orderManifest = @()
    foreach ($orderFile in $orderFiles) {
        $relative = $orderFile.FullName.Substring($orderRoot.TrimEnd('\').Length + 1)
        $orderTarget = Join-Path $gameRoot $relative
        $existed = Test-Path -LiteralPath $orderTarget
        if ($existed) {
            $oldOrder = Join-Path (Join-Path $backup 'model-order') $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $oldOrder) -Force | Out-Null
            Copy-Item -LiteralPath $orderTarget -Destination $oldOrder
        }
        $orderManifest += [pscustomobject]@{ Path = $relative; Existed = $existed; SHA256 = (Get-FileHash -LiteralPath $orderFile.FullName).Hash }
    }
    if ($orderManifest.Count) { $orderManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'model-order.json') -Encoding UTF8 }
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw '安装后的 DLL 校验不一致，请重新安装。'
    }
    foreach ($orderFile in $orderFiles) {
        $relative = $orderFile.FullName.Substring($orderRoot.TrimEnd('\').Length + 1)
        $orderTarget = Join-Path $gameRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $orderTarget) -Force | Out-Null
        Copy-Item -LiteralPath $orderFile.FullName -Destination $orderTarget -Force
        if ((Get-FileHash -LiteralPath $orderFile.FullName).Hash -ne (Get-FileHash -LiteralPath $orderTarget).Hash) { throw '网格顺序文件校验不一致。' }
    }
    Write-Host '1.1.4 安装完成，启动游戏后按 F9 打开。'
    Write-Host ('原文件备份：' + $backup)
    if ($orderFiles.Count) { Write-Host ('已安装网格顺序表：' + $orderFiles.Count + ' 个模型。') }
    Write-Host '原有配置由插件自动迁移；无需运行外置管理器。'
} catch {
    Write-Host ('安装失败：' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
