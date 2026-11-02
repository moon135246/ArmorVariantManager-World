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
    $notesSource = Join-Path $PSScriptRoot 'nativePC\plugins\WorldDiff\更新日志.md'
    if (-not (Test-Path -LiteralPath $notesSource)) { throw '安装包缺少更新日志.md。' }
    $plugins = Join-Path $gameRoot 'nativePC\plugins'
    $dataDir = Join-Path $plugins 'WorldDiff'
    New-Item -ItemType Directory -Path $plugins -Force | Out-Null
    $backup = Join-Path $gameRoot ('WorldDiff-backups\before-1.2.2-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $target = Join-Path $plugins 'WorldDiff.dll'
    if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
    if (Test-Path -LiteralPath $dataDir) { Copy-Item -LiteralPath $dataDir -Destination $backup -Recurse }
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw '安装后的 DLL 校验不一致，请重新安装。'
    }
    New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
    $notesTarget = Join-Path $dataDir '更新日志.md'
    $legacyNotes = Join-Path $dataDir '更新记录.md'
    if (-not (Test-Path -LiteralPath $notesTarget) -and (Test-Path -LiteralPath $legacyNotes)) {
        if ((Get-Item -LiteralPath $legacyNotes).PSIsContainer) { throw '旧更新记录路径不是文件，未迁移。' }
        Move-Item -LiteralPath $legacyNotes -Destination $notesTarget
        Write-Host '已将更新记录.md 更名为更新日志.md。'
    }
    # The prior official default text is known byte-for-byte. Refresh only that
    # unedited default; keep every manually changed document intact.
    $legacyDefaultHashes = @('91FBCB3556620B1C902200F8F0F156772D96ED53651A944C4216EE1D863E0CBE','1AE6213DD14BCB11CFA5A51869896C4236479687062CB00594729392F9B83F4A')
    $notesUpdated = $false
    if ((Test-Path -LiteralPath $notesTarget) -and $legacyDefaultHashes -contains (Get-FileHash -LiteralPath $notesTarget).Hash) {
        Copy-Item -LiteralPath $notesSource -Destination $notesTarget -Force
        if ((Get-FileHash -LiteralPath $notesSource).Hash -ne (Get-FileHash -LiteralPath $notesTarget).Hash) {
            throw '补全后的更新日志校验不一致，请重新安装。'
        }
        Write-Host '已更新默认更新日志。'
        $notesUpdated = $true
    }
    if (-not (Test-Path -LiteralPath $notesTarget)) {
        Copy-Item -LiteralPath $notesSource -Destination $notesTarget
        if ((Get-FileHash -LiteralPath $notesSource).Hash -ne (Get-FileHash -LiteralPath $notesTarget).Hash) {
            throw '安装后的更新日志校验不一致，请重新安装。'
        }
    } elseif (-not $notesUpdated) { Write-Host '已保留原有更新日志，未覆盖手动编辑的文字。' }
    Write-Host '1.2.2 安装完成，启动游戏后按 F9 打开。'
    Write-Host ('原文件备份：' + $backup)
    Write-Host '原有配置由插件自动迁移；无需运行外置管理器。'
} catch {
    Write-Host ('安装失败：' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
