#!/usr/bin/env pwsh
#requires -Version 7.0
# Every mutation targets disposable fixtures; the actual GTA installation is never used.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$helper = Join-Path $repositoryRoot 'Client/definitive/set-loader.ps1'
$fixtureRoot = Join-Path $repositoryRoot ('Build/definitive-loader-test/fixtures-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
$paths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll',
    'DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt', 'DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt',
    'DE_ONLINE_LICENSES/MTA_GPLv3.txt', 'DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md'
)
$global:deLoaderTestGameActive = $false
$script:passed = 0
$script:skipped = 0
# Simulate the game-running guard without starting any real process.
function Get-Process { [CmdletBinding()]param([string[]]$Name) if ($global:deLoaderTestGameActive) { [pscustomobject]@{ ProcessName = 'SanAndreas' } } }
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Loader([string]$Directory, [string]$Action, [switch]$WhatIf) {
    & $helper -GameDirectory $Directory -Action $Action -WhatIf:$WhatIf 6>$null
}
function New-Fixture([string]$Omit = '') {
    $root = Join-Path $fixtureRoot ([Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $files = foreach ($relative in $paths) {
        $destination = [IO.Path]::GetFullPath((Join-Path $root $relative))
        $bytes = [Text.Encoding]::UTF8.GetBytes("Fixture bytes: $relative`n")
        if ($relative -ne $Omit) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.File]::WriteAllBytes($destination, $bytes)
        }
        [ordered]@{ path = $relative; destination = $destination; sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() }
    }
    $manifest = [ordered]@{ packageId = 'de-online-prototype'; formatVersion = 1; gameDirectory = $root; files = @($files) }
    [IO.File]::WriteAllText((Join-Path $root 'de-online-install-manifest.json'), ($manifest | ConvertTo-Json -Depth 6))
    return $root
}
function Edit-Manifest([string]$Root, [scriptblock]$Edit) {
    $path = Join-Path $Root 'de-online-install-manifest.json'
    $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    & $Edit $manifest
    [IO.File]::WriteAllText($path, ($manifest | ConvertTo-Json -Depth 6))
}
function Must-Fail([scriptblock]$Operation, [string]$Message) {
    $failure = $null
    try { & $Operation | Out-Null } catch { $failure = $_.Exception.Message }
    Assert ($null -ne $failure -and $failure.Contains($Message, [StringComparison]::OrdinalIgnoreCase)) "Expected failure containing '$Message', got '$failure'."
}
function Case([string]$Name, [scriptblock]$Operation) {
    & $Operation
    $script:passed++
    Write-Host "PASS $Name"
}

Case 'Round trip preserves every owned byte and the manifest; journals describe both moves' {
    $root = New-Fixture
    $manifestPath = Join-Path $root 'de-online-install-manifest.json'
    $before = Hash $manifestPath
    $original = @{}; foreach ($path in $paths) { $original[$path] = Hash (Join-Path $root $path) }
    Assert (-not (Test-Path -LiteralPath (Join-Path $root 'SanAndreas.exe'))) 'Fixtures must not require or contain GTA.'
    Assert ((Loader $root Status).state -eq 'Enabled') 'Initial status failed.'
    $disabled = Loader $root Disable
    Assert ($disabled.state -eq 'Disabled' -and $disabled.verifiedFiles -eq 16) 'Disable result failed.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $root 'version.dll'))) 'Enabled DLL remained.'
    Assert ((Hash (Join-Path $root 'version.dll.de-online-disabled')) -eq $original['version.dll']) 'Loader bytes changed.'
    $prepared = Get-Content -LiteralPath (Join-Path $disabled.recoveryDirectory 'prepared.json') -Raw | ConvertFrom-Json
    $completed = Get-Content -LiteralPath (Join-Path $disabled.recoveryDirectory 'completed.json') -Raw | ConvertFrom-Json
    Assert ($prepared.desiredState -eq 'Disabled' -and $completed.state -eq 'Disabled') 'Disable journals failed.'
    Assert ((Loader $root Status).state -eq 'Disabled') 'Disabled status failed.'
    $enabled = Loader $root Enable
    Assert ($enabled.state -eq 'Enabled') 'Enable result failed.'
    Assert ((Hash $manifestPath) -eq $before) 'Manifest bytes changed.'
    foreach ($path in $paths) { Assert ((Hash (Join-Path $root $path)) -eq $original[$path]) "Owned bytes changed: $path" }
    Assert (-not (Test-Path -LiteralPath (Join-Path $root 'version.dll.de-online-disabled'))) 'Disabled copy remained.'
}
Case 'Repeated actions are idempotent and produce no extra recovery journal' {
    $root = New-Fixture
    Assert ($null -eq (Loader $root Enable).recoveryDirectory) 'Already-enabled action journaled.'
    Loader $root Disable | Out-Null
    Assert ($null -eq (Loader $root Disable).recoveryDirectory) 'Already-disabled action journaled.'
}
Case 'Disable WhatIf leaves enabled state and all bytes unchanged' {
    $root = New-Fixture; $before = Hash (Join-Path $root 'version.dll')
    $result = Loader $root Disable -WhatIf
    Assert ($result.state -eq 'Enabled' -and $null -eq $result.recoveryDirectory) 'WhatIf applied a disable.'
    Assert ((Hash (Join-Path $root 'version.dll')) -eq $before) 'WhatIf changed loader bytes.'
}
Case 'Enable WhatIf leaves disabled state unchanged' {
    $root = New-Fixture; Loader $root Disable | Out-Null
    Assert ((Loader $root Enable -WhatIf).state -eq 'Disabled') 'WhatIf applied an enable.'
    Assert ((Loader $root Status).state -eq 'Disabled') 'WhatIf moved the disabled DLL.'
}
Case 'Both copies block mutation without overwriting either file' {
    $root = New-Fixture
    $enabled = Join-Path $root 'version.dll'; $disabled = Join-Path $root 'version.dll.de-online-disabled'
    [IO.File]::WriteAllText($disabled, 'Unrelated user file')
    $beforeEnabled = Hash $enabled; $beforeDisabled = Hash $disabled
    Must-Fail { Loader $root Disable } 'collision'
    Assert ((Hash $enabled) -eq $beforeEnabled -and (Hash $disabled) -eq $beforeDisabled) 'Collision overwrote a file.'
}
Case 'Neither copy blocks recovery instead of fabricating a DLL' {
    $root = New-Fixture -Omit 'version.dll'
    Must-Fail { Loader $root Enable } 'missing loader'
}
Case 'Unrelated edited file blocks disable before any rename' {
    $root = New-Fixture
    [IO.File]::WriteAllText((Join-Path $root 'CLEO/de-online/controller.mjs'), 'User edit')
    Must-Fail { Loader $root Disable } 'modified'
    Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Loader moved before full validation.'
}
Case 'Unrelated edited file blocks enable before any rename' {
    $root = New-Fixture; Loader $root Disable | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'DE_ONLINE_LICENSES/MTA_GPLv3.txt'), 'User edit')
    Must-Fail { Loader $root Enable } 'modified'
    Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll.de-online-disabled')) 'Loader restored before full validation.'
}
Case 'Disabled loader hash mismatch blocks enable' {
    $root = New-Fixture; Loader $root Disable | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'version.dll.de-online-disabled'), 'Changed DLL')
    Must-Fail { Loader $root Enable } 'modified'
}
Case 'Missing unrelated owned file blocks status and mutation' {
    $root = New-Fixture -Omit 'CLEO/de-online/index.js'
    Must-Fail { Loader $root Status } 'missing'
    Must-Fail { Loader $root Disable } 'missing'
}
Case 'Wrong package identity blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.packageId = 'other' }
    Must-Fail { Loader $root Disable } 'identity'
}
Case 'Wrong format blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.formatVersion = 2 }
    Must-Fail { Loader $root Disable } 'identity'
}
Case 'Wrong game directory blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.gameDirectory = $fixtureRoot }
    Must-Fail { Loader $root Disable } 'identity'
}
Case 'Unexpected manifest path blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.files[1].path = '../outside.asi' }
    Must-Fail { Loader $root Disable } 'known owned files'
}
Case 'Duplicate owned path blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.files[1].path = $m.files[0].path }
    Must-Fail { Loader $root Disable } 'known owned files'
}
Case 'An omitted manifest record blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.files = @($m.files | Select-Object -Skip 1) }
    Must-Fail { Loader $root Disable } 'missing owned files'
}
Case 'Destination outside game directory blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.files[1].destination = Join-Path $fixtureRoot 'outside.asi' }
    Must-Fail { Loader $root Disable } 'invalid destination'
}
Case 'Malformed hash blocks mutation' {
    $root = New-Fixture; Edit-Manifest $root { param($m) $m.files[1].sha256 = 'not-a-sha256' }
    Must-Fail { Loader $root Disable } 'invalid destination or hash'
}
Case 'Game-running guard blocks mutations but permits read-only status' {
    $root = New-Fixture; $global:deLoaderTestGameActive = $true
    try {
        Assert ((Loader $root Status).state -eq 'Enabled') 'Status wrongly required game closure.'
        Must-Fail { Loader $root Disable } 'Close San Andreas'
    } finally { $global:deLoaderTestGameActive = $false }
}
Case 'An interrupted rename is recoverable from files even without a completed journal' {
    $root = New-Fixture
    $journal = Join-Path $fixtureRoot ([Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($journal) | Out-Null
    [IO.File]::WriteAllText((Join-Path $journal 'prepared.json'), '{"action":"Disable","desiredState":"Disabled"}')
    [IO.File]::Move((Join-Path $root 'version.dll'), (Join-Path $root 'version.dll.de-online-disabled'))
    Assert ((Loader $root Status).state -eq 'Disabled') 'Pending rename state was not inferred from hashes.'
    Assert ((Loader $root Enable).state -eq 'Enabled') 'Interrupted disable could not be restored.'
}
try {
    $root = New-Fixture
    $link = Join-Path $fixtureRoot ('linked-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Junction -Path $link -Target $root -ErrorAction Stop | Out-Null
} catch {
    $script:skipped++
    Write-Host "SKIP linked-path test: $($_.Exception.Message)"
    $link = $null
}
if ($link) {
    Case 'Linked root is refused before mutation' {
        Must-Fail { Loader $link Disable } 'linked path'
        Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Linked-path refusal mutated target.'
    }
    Case 'A linked owned-file ancestor is refused before mutation' {
        $root = New-Fixture
        $original = [IO.Path]::GetFullPath((Join-Path $root 'CLEO/de-online'))
        $retained = [IO.Path]::GetFullPath((Join-Path $root 'retained-adapter'))
        # Both explicit paths are inside this fixture. Keep the fixture bytes to
        # verify that an ancestor junction never grants permission to move a DLL.
        Assert ($original.StartsWith($fixtureRoot + [IO.Path]::DirectorySeparatorChar) -and
            $retained.StartsWith($fixtureRoot + [IO.Path]::DirectorySeparatorChar)) 'Fixture move escaped the test directory.'
        [IO.Directory]::Move($original, $retained)
        New-Item -ItemType Junction -Path $original -Target $retained | Out-Null
        Must-Fail { Loader $root Disable } 'linked path'
        Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Linked ancestor refusal mutated loader.'
    }
}
$summary = [ordered]@{ passed = $script:passed; skipped = $script:skipped; fixtureDirectory = $fixtureRoot; realGameUsed = $false }
[IO.File]::WriteAllText((Join-Path $fixtureRoot 'result.json'), ($summary | ConvertTo-Json))
Write-Host "Loader fixture tests: $($script:passed) passed, $($script:skipped) skipped. $fixtureRoot"
