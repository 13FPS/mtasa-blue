#!/usr/bin/env pwsh
#requires -Version 7.0
# Every mutation targets disposable fixtures; the actual GTA installation is never used.
[CmdletBinding()]
param([switch]$FrontendAdmissionOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$helper = Join-Path $repositoryRoot 'Client/definitive/set-loader.ps1'
$uninstaller = Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1'
$modeHelper = Join-Path $repositoryRoot 'Client/definitive/set-mode.ps1'
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
function New-Fixture([string]$Omit = '', [switch]$PadModule) {
    $root = Join-Path $fixtureRoot ([Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $fixturePaths = @($paths)
    if ($PadModule) { $fixturePaths += 'CLEO/de-online/native-pad-reference.mjs' }
    $files = foreach ($relative in $fixturePaths) {
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

function New-ModeFixture([switch]$PadModule, [switch]$PadMarker, [switch]$PadEnabled) {
    $root = New-Fixture -PadModule:$PadModule
    $config = @"
export const MODE = 'smoke';
export const GAIT_REFERENCE = false;
export const GAIT_RATE = false;
export const GAIT_ROOT_LOCK = false;
export const INPUT_REFERENCE = false;
"@
    if ($PadMarker) { $config += "`nexport const MTA_NATIVE_PAD_REFERENCE = $($PadEnabled.IsPresent.ToString().ToLowerInvariant());`n" }
    $configPath = Join-Path $root 'CLEO/de-online/config.mjs'
    [IO.File]::WriteAllText($configPath, $config)
    # The mode helper compares the fixture executable to its recorded hash;
    # no game binary or known-game fingerprint is needed for this configuration test.
    [IO.File]::WriteAllText((Join-Path $root 'SanAndreas.exe'), 'Non-executable fixture bytes')
    Edit-Manifest $root {
        param($manifest)
        $manifest | Add-Member -NotePropertyName mode -NotePropertyValue 'Smoke'
        $manifest | Add-Member -NotePropertyName gameSha256 -NotePropertyValue ((Hash (Join-Path $root 'SanAndreas.exe')).ToLowerInvariant())
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = (Hash $configPath).ToLowerInvariant()
    }
    return $root
}

function New-FrontendFixture([int]$Count) {
    $root = New-Fixture -PadModule
    $extraPaths = @('CLEO/de-online/native-movement-trial.mjs','CLEO/de-online/game_de.dll',
        'CLEO/de-online/core.dll','CLEO/de-online/pthread.dll','CLEO/de-online/xmll.dll','CLEO/de-online/mta/config/coreconfig.xml')
    if($Count -ge 36){$extraPaths += @('CLEO/de-online/cgui.dll',
        'CLEO/de-online/mta/cgui/sans.ttf','CLEO/de-online/mta/cgui/saheader.ttf','CLEO/de-online/mta/cgui/sagothic.ttf',
        'CLEO/de-online/mta/cgui/Imageset.xsd','CLEO/de-online/mta/cgui/GUIScheme.xsd','CLEO/de-online/mta/cgui/Font.xsd',
        'CLEO/de-online/mta/cgui/Falagard.xsd','CLEO/de-online/mta/cgui/unifont.ttf',
        'CLEO/de-online/skins/Default/CGUI.xml','CLEO/de-online/skins/Default/CGUI.is.xml',
        'CLEO/de-online/skins/Default/CGUI.lnf.xml','CLEO/de-online/skins/Default/CGUI.png')}
    if($Count -ge 58){$extraPaths += @(
        'CLEO/de-online/mta/cgui/images/background.png','CLEO/de-online/mta/cgui/images/background_logo.png',
        'CLEO/de-online/mta/cgui/images/mta_filler.png','CLEO/de-online/mta/cgui/images/version.png',
        'CLEO/de-online/mta/cgui/images/the_language_icon.png',
        'CLEO/de-online/mta/cgui/images/serverbrowser/locked.png','CLEO/de-online/mta/cgui/images/serverbrowser/search-servers.png',
        'CLEO/de-online/mta/cgui/images/serverbrowser/search-players.png','CLEO/de-online/mta/cgui/images/serverbrowser/refresh.png',
        'CLEO/de-online/mta/cgui/images/serverbrowser/favorite.png','CLEO/de-online/mta/cgui/images/serverbrowser/connect.png',
        'CLEO/de-online/mta/cgui/images/serverbrowser/info.png','CLEO/de-online/mta/cgui/images/serverbrowser/search.png',
        'CLEO/de-online/mta/cgui/images/serverbrowser/trashcan.png',
        'CLEO/de-online/mta/locale/en_US/menu_disconnect.png','CLEO/de-online/mta/locale/en_US/menu_quick_connect.png',
        'CLEO/de-online/mta/locale/en_US/menu_browse_servers.png','CLEO/de-online/mta/locale/en_US/menu_settings.png',
        'CLEO/de-online/mta/locale/en_US/menu_about.png','CLEO/de-online/mta/locale/en_US/menu_quit.png',
        'CLEO/de-online/mta/locale/en_US/latest_news.png','CLEO/de-online/mta/locale/en_US/client.pot')}
    if($Count -in @(60,64)){$extraPaths += @('MtaDefinitive.asi','MtaDefinitive.ini')}
    if($Count -in @(62,64)){$extraPaths += @('CLEO/de-online/mta/cgui/images/question.png','CLEO/de-online/mta/cgui/images/info.png',
        'CLEO/de-online/mta/cgui/images/warning.png','CLEO/de-online/mta/cgui/images/error.png')}
    Edit-Manifest $root {
        param($manifest)
        foreach($relative in $extraPaths){
            $destination = [IO.Path]::GetFullPath((Join-Path $root $relative))
            [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
            [IO.File]::WriteAllText($destination,"Non-executable fixture bytes: $relative`n")
            $manifest.files += [pscustomobject]@{path=$relative;destination=$destination;sha256=(Hash $destination).ToLowerInvariant()}
        }
        $native = $Count -in @(60,64)
        $config = "export const MODE = 'smoke';`nexport const MTA_CORE_FRONTEND_RUNTIME = $($native.ToString().ToLowerInvariant());`n" +
            "export const MTA_CORE_FRONTEND_NATIVE_HOST = $($native.ToString().ToLowerInvariant());`nexport const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = false;`n"
        $configPath = Join-Path $root 'CLEO/de-online/config.mjs'
        [IO.File]::WriteAllText($configPath,$config)
        if($native){[IO.File]::WriteAllText((Join-Path $root 'MtaDefinitive.ini'),"[MTA]`nEnabled=1`nLocalInput=1`nBindingPersistence=0`n")}
        foreach($path in @('CLEO/de-online/config.mjs','MtaDefinitive.ini')){
            foreach($entry in @($manifest.files | Where-Object path -eq $path)){$entry.sha256=(Hash $entry.destination).ToLowerInvariant()}
        }
        $manifest | Add-Member -NotePropertyName mode -NotePropertyValue 'Smoke'
        Assert ($manifest.files.Count -eq $Count) 'Frontend fixture does not contain its declared complete group.'
    }
    return $root
}

if(-not $FrontendAdmissionOnly){
Case 'Removal validation preserves both legacy 16-file and new 17-file installations' {
    foreach ($pad in @($false, $true)) {
        $root = New-Fixture -PadModule:$pad
        $manifestPath = Join-Path $root 'de-online-install-manifest.json'
        $before = Hash $manifestPath
        & $uninstaller -GameDirectory $root -ValidateOnly 6>$null
        Assert ((Hash $manifestPath) -eq $before) 'Validation modified the manifest.'
        Assert (@(Get-ChildItem -LiteralPath $root -File -Recurse).Count -eq (17 + [int]$pad)) 'Validation removed an owned file.'
    }
}
Case 'Removal supports both ownership generations and preserves unrelated files' {
    foreach ($pad in @($false, $true)) {
        $root = New-Fixture -PadModule:$pad
        $retained = Join-Path $root 'CLEO/de-online/user-notes.txt'
        [IO.File]::WriteAllText($retained, 'User bytes')
        & $uninstaller -GameDirectory $root 6>$null
        Assert (Test-Path -LiteralPath $retained) 'Removal deleted an unrelated file.'
        Assert (-not (Test-Path -LiteralPath (Join-Path $root 'de-online-install-manifest.json'))) 'Removal retained its manifest.'
        Assert (@(Get-ChildItem -LiteralPath $root -File -Recurse).Count -eq 1) 'Removal left an owned file.'
    }
}
Case 'A modified native pad module blocks removal before deleting legacy files' {
    $root = New-Fixture -PadModule
    [IO.File]::WriteAllText((Join-Path $root 'CLEO/de-online/native-pad-reference.mjs'), 'User edit')
    Must-Fail { & $uninstaller -GameDirectory $root 6>$null } 'modified'
    Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Removal began before module validation.'
}
Case 'The optional pad record cannot replace a required legacy owned record' {
    $root = New-Fixture -PadModule
    Edit-Manifest $root { param($m) $m.files = @($m.files | Where-Object path -ne 'CLEO/de-online/index.js') }
    Must-Fail { & $uninstaller -GameDirectory $root -ValidateOnly 6>$null } 'known owned files'
}
Case 'Ordinary mode changes remain available without the newer pad marker' {
    $root = New-ModeFixture
    & $modeHelper -GameDirectory $root -Mode Diagnostic 6>$null
    Assert ((Get-Content -LiteralPath (Join-Path $root 'CLEO/de-online/config.mjs') -Raw).Contains("MODE = 'diagnostic'")) 'Legacy mode change failed.'
    & $uninstaller -GameDirectory $root -ValidateOnly 6>$null
}
Case 'The native pad opt-in requires both its configuration marker and owned module' {
    $root = New-ModeFixture -PadModule
    Must-Fail { & $modeHelper -GameDirectory $root -Mode Smoke -MtaNativePadReference 6>$null } 'Update the adapter'
    $root = New-ModeFixture -PadMarker
    Must-Fail { & $modeHelper -GameDirectory $root -Mode Smoke -MtaNativePadReference 6>$null } 'owned module is missing'
}
Case 'Native pad selection and ordinary mode clearing maintain all owned hashes' {
    $root = New-ModeFixture -PadModule -PadMarker
    & $modeHelper -GameDirectory $root -Mode Smoke -MtaNativePadReference 6>$null
    $configPath = Join-Path $root 'CLEO/de-online/config.mjs'
    Assert ((Get-Content -LiteralPath $configPath -Raw).Contains('MTA_NATIVE_PAD_REFERENCE = true;')) 'Probe was not armed.'
    & $uninstaller -GameDirectory $root -ValidateOnly 6>$null
    & $modeHelper -GameDirectory $root -Mode Smoke 6>$null
    Assert ((Get-Content -LiteralPath $configPath -Raw).Contains('MTA_NATIVE_PAD_REFERENCE = false;')) 'Ordinary mode retained probe opt-in.'
    & $uninstaller -GameDirectory $root -ValidateOnly 6>$null
}
Case 'The native pad opt-in rejects Online mode and simultaneous input or gait trials' {
    Must-Fail { & $modeHelper -GameDirectory $fixtureRoot -Mode Online -MtaNativePadReference 6>$null } 'Smoke mode'
    foreach ($trial in @('NativeInputReference', 'NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot')) {
        $flags = @{ MtaNativePadReference = $true; $trial = $true }
        Must-Fail { & $modeHelper -GameDirectory $fixtureRoot -Mode Smoke @flags 6>$null } 'conflicts'
    }
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
}
Case 'Loader bypass and restore admit every complete frontend ownership generation' {
    foreach($count in @(23,36,58,60,62,64)){
        $root = New-FrontendFixture $count
        $manifestPath = Join-Path $root 'de-online-install-manifest.json';$before = Hash $manifestPath
        Assert ((Loader $root Status).verifiedFiles -eq $count) 'Complete startup generation was refused.'
        Assert ((Loader $root Disable -WhatIf).state -eq 'Enabled') 'WhatIf disabled a complete startup generation.'
        Assert ((Loader $root Disable).state -eq 'Disabled') 'Complete startup generation did not disable.'
        Assert ((Loader $root Status).verifiedFiles -eq $count) 'Disabled startup generation failed validation.'
        Assert ((Loader $root Enable).state -eq 'Enabled') 'Complete startup generation did not restore.'
        Assert ((Hash $manifestPath) -eq $before) 'Loader toggle changed the owned manifest.'
    }
}
Case 'Partial native and dialog groups refuse loader changes before rename' {
    foreach($missingPath in @('MtaDefinitive.ini','CLEO/de-online/mta/cgui/images/question.png')){
        $root = New-FrontendFixture 64
        Edit-Manifest $root {param($m) $m.files=@($m.files | Where-Object path -ne $missingPath)}
        Must-Fail {Loader $root Disable} 'Incomplete'
        Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Incomplete group renamed the loader.'
    }
}
Case 'Rehashed mismatching native INI and script selection refuse loader restore' {
    $root = New-FrontendFixture 64;Loader $root Disable | Out-Null
    $ini = Join-Path $root 'MtaDefinitive.ini'
    [IO.File]::WriteAllText($ini,"[MTA]`nEnabled=0`nLocalInput=1`nBindingPersistence=0`n")
    Edit-Manifest $root {param($m) ($m.files | Where-Object path -eq 'MtaDefinitive.ini').sha256=(Hash $ini).ToLowerInvariant()}
    Must-Fail {Loader $root Enable} 'INI does not match'
    Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll.de-online-disabled')) 'Incoherent native entry restored the loader.'
}
Case 'A deliberately disarmed complete native entry remains recoverable' {
    $root = New-FrontendFixture 64
    $ini = Join-Path $root 'MtaDefinitive.ini';$config = Join-Path $root 'CLEO/de-online/config.mjs'
    [IO.File]::WriteAllText($ini,"[MTA]`nEnabled=0`nLocalInput=1`nBindingPersistence=0`n")
    [IO.File]::WriteAllText($config,[IO.File]::ReadAllText($config).Replace('MTA_CORE_FRONTEND_NATIVE_HOST = true;','MTA_CORE_FRONTEND_NATIVE_HOST = false;'))
    Edit-Manifest $root {
        param($m)
        foreach($path in @('MtaDefinitive.ini','CLEO/de-online/config.mjs')){($m.files | Where-Object path -eq $path).sha256=(Hash (Join-Path $root $path)).ToLowerInvariant()}
    }
    Assert ((Loader $root Disable).state -eq 'Disabled') 'Coherent disarmed entry failed bypass.'
    Assert ((Loader $root Enable).state -eq 'Enabled') 'Coherent disarmed entry failed restoration.'
}
Case 'An edited message-box asset still refuses loader mutation' {
    $root = New-FrontendFixture 64
    [IO.File]::WriteAllText((Join-Path $root 'CLEO/de-online/mta/cgui/images/warning.png'),'User edit')
    Must-Fail {Loader $root Disable} 'modified'
    Assert (Test-Path -LiteralPath (Join-Path $root 'version.dll')) 'Edited dialog asset renamed loader.'
}
$summary = [ordered]@{ passed = $script:passed; skipped = $script:skipped; fixtureDirectory = $fixtureRoot; realGameUsed = $false }
[IO.File]::WriteAllText((Join-Path $fixtureRoot 'result.json'), ($summary | ConvertTo-Json))
Write-Host "Loader fixture tests: $($script:passed) passed, $($script:skipped) skipped. $fixtureRoot"
