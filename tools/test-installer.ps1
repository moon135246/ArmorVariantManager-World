$ErrorActionPreference = 'Stop'
$taskVersion = Split-Path -Parent $PSScriptRoot
$taskRoot = Join-Path ([IO.Path]::GetTempPath()) ('WorldDiff122Installer_' + [Guid]::NewGuid().ToString('N'))
$taskGame = Join-Path $taskRoot 'game'
$taskData = Join-Path $taskGame 'nativePC\plugins\WorldDiff'
New-Item -ItemType Directory -Path $taskData -Force | Out-Null
[IO.File]::WriteAllBytes((Join-Path $taskGame 'MonsterHunterWorld.exe'),[byte[]]@())
[IO.File]::WriteAllBytes((Join-Path $taskGame 'loader.dll'),[byte[]]@())
[IO.File]::WriteAllText((Join-Path $taskData 'Settings.cfg'),'settings-fixture')
# Only the unique fixture root is passed to the real installer. The test never
# accesses the user's game installation or terminates a running game.
function Get-Process { param([string]$Name) return $null }
$taskChecks = 0
function Assert-Task([bool]$Value,[string]$Reason) { $script:taskChecks++; if (-not $Value) { throw $Reason } }
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskGame
$taskNotes = Join-Path $taskData '更新日志.md'
$taskSource = Join-Path $taskVersion '发布\nativePC\plugins'
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskGame 'nativePC\plugins\WorldDiff.dll')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $taskSource 'WorldDiff.dll')).Hash) 'DLL matches release'
Assert-Task ((Get-FileHash -LiteralPath $taskNotes).Hash -eq (Get-FileHash -LiteralPath (Join-Path $taskSource 'WorldDiff\更新日志.md')).Hash) 'missing notes are installed'
Assert-Task ([IO.File]::ReadAllText((Join-Path $taskData 'Settings.cfg')) -eq 'settings-fixture') 'settings remain untouched'
[IO.File]::WriteAllText($taskNotes,"## V自定义`n- 我的记录 100%`n",[Text.UTF8Encoding]::new($false))
$taskCustom = (Get-FileHash -LiteralPath $taskNotes).Hash
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskGame
Assert-Task ((Get-FileHash -LiteralPath $taskNotes).Hash -eq $taskCustom) 'manual notes survive reinstall'
$taskBackups = @(Get-ChildItem -LiteralPath (Join-Path $taskGame 'WorldDiff-backups') -Directory)
Assert-Task ($taskBackups.Count -eq 2) 'each installation has its own backup'
$taskSaved = @($taskBackups | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'WorldDiff\更新日志.md') })
Assert-Task ($taskSaved.Count -eq 1 -and (Get-FileHash -LiteralPath (Join-Path $taskSaved[0].FullName 'WorldDiff\更新日志.md')).Hash -eq $taskCustom) 'custom notes are included in the backup'
function New-TaskGame([string]$Name) {
    $taskFixture = Join-Path $taskRoot $Name
    New-Item -ItemType Directory -Path (Join-Path $taskFixture 'nativePC\plugins\WorldDiff') -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $taskFixture 'MonsterHunterWorld.exe'),[byte[]]@())
    [IO.File]::WriteAllBytes((Join-Path $taskFixture 'loader.dll'),[byte[]]@())
    return $taskFixture
}
$taskLegacyGame = New-TaskGame 'legacy-default'
$taskLegacyData = Join-Path $taskLegacyGame 'nativePC\plugins\WorldDiff'
$taskLegacySource = Join-Path $PSScriptRoot 'fixtures\updates_121_legacy.md'
Copy-Item -LiteralPath $taskLegacySource -Destination (Join-Path $taskLegacyData '更新记录.md')
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskLegacyGame
Assert-Task (-not (Test-Path -LiteralPath (Join-Path $taskLegacyData '更新记录.md'))) 'legacy filename is migrated'
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskLegacyData '更新日志.md')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $taskSource 'WorldDiff\更新日志.md')).Hash) 'unedited legacy default gets the complete early history'
$taskLegacyBackup = Get-ChildItem -LiteralPath (Join-Path $taskLegacyGame 'WorldDiff-backups') -Directory | Select-Object -First 1
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskLegacyBackup.FullName 'WorldDiff\更新记录.md')).Hash -eq (Get-FileHash -LiteralPath $taskLegacySource).Hash) 'original legacy document is backed up before migration'
$taskCustomGame = New-TaskGame 'legacy-custom'
$taskCustomData = Join-Path $taskCustomGame 'nativePC\plugins\WorldDiff'
[IO.File]::WriteAllText((Join-Path $taskCustomData '更新记录.md'),"# 用户自定义内容`n- 不能覆盖`n",[Text.UTF8Encoding]::new($false))
$taskLegacyCustomHash = (Get-FileHash -LiteralPath (Join-Path $taskCustomData '更新记录.md')).Hash
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskCustomGame
Assert-Task (-not (Test-Path -LiteralPath (Join-Path $taskCustomData '更新记录.md'))) 'custom legacy filename is migrated'
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCustomData '更新日志.md')).Hash -eq $taskLegacyCustomHash) 'custom legacy text is preserved byte for byte'
$taskCustomBackup = Get-ChildItem -LiteralPath (Join-Path $taskCustomGame 'WorldDiff-backups') -Directory | Select-Object -First 1
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCustomBackup.FullName 'WorldDiff\更新记录.md')).Hash -eq $taskLegacyCustomHash) 'custom legacy text is backed up'
[IO.File]::WriteAllText((Join-Path $taskCustomData '更新记录.md'),'another independent document')
$taskCollisionHash = (Get-FileHash -LiteralPath (Join-Path $taskCustomData '更新记录.md')).Hash
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskCustomGame
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCustomData '更新日志.md')).Hash -eq $taskLegacyCustomHash) 'existing new document takes priority without being overwritten'
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCustomData '更新记录.md')).Hash -eq $taskCollisionHash) 'a separate colliding legacy document is retained'
$taskCompleteGame = New-TaskGame 'complete-121-default'
$taskCompleteData = Join-Path $taskCompleteGame 'nativePC\plugins\WorldDiff'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures\updates_121_complete.md') -Destination (Join-Path $taskCompleteData '更新日志.md')
& (Join-Path $taskVersion '发布\安装到游戏.ps1') -GamePath $taskCompleteGame
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCompleteData '更新日志.md')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $taskSource 'WorldDiff\更新日志.md')).Hash) 'unedited complete 1.2.1 log receives the new release entry'
$taskCompleteBackup = Get-ChildItem -LiteralPath (Join-Path $taskCompleteGame 'WorldDiff-backups') -Directory | Select-Object -First 1
Assert-Task ((Get-FileHash -LiteralPath (Join-Path $taskCompleteBackup.FullName 'WorldDiff\更新日志.md')).Hash -eq '1AE6213DD14BCB11CFA5A51869896C4236479687062CB00594729392F9B83F4A') 'complete old official log is backed up before refreshing'
Write-Output "PASS $taskChecks installer checks"
Write-Output "Artifacts $taskRoot"
