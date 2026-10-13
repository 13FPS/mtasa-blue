#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production ownership scripts against disposable game directories.
# Copying the verified executable supplies the install fingerprint without ever
# launching it or modifying the user's installation.
[CmdletBinding()]
param(
    [string]$PackageRoot,
    [Parameter(Mandatory)][string]$GameExecutable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $PackageRoot) { $PackageRoot = Join-Path $repositoryRoot 'Build/definitive-package/Smoke' }
$sourcePackage = (Resolve-Path -LiteralPath $PackageRoot).Path
$sourceExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$utf8 = [Text.UTF8Encoding]::new($false)
$movementScript = 'CLEO/de-online/native-movement-trial.mjs'
$backend = 'CLEO/de-online/game_de.dll'
$padScript = 'CLEO/de-online/native-pad-reference.mjs'
$scriptNames = @('install.ps1', 'update.ps1', 'uninstall.ps1', 'set-mode.ps1')
$expectedPaths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll', $padScript, $movementScript, $backend,
    'DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt', 'DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt',
    'DE_ONLINE_LICENSES/MTA_GPLv3.txt', 'DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md'
)

function Get-Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
if ((Get-Sha $sourceExecutable) -ne $knownGameHash) { throw 'A copy of the exact verified Steam executable is required.' }
if (Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue) {
    throw 'Finish game and SanAndreas.exe host-fixture processes before running these ownership tests.'
}
$sourceManifest = Get-Content -LiteralPath (Join-Path $sourcePackage 'manifest.json') -Raw | ConvertFrom-Json
if ($sourceManifest.packageId -ne 'de-online-prototype' -or $sourceManifest.mode -ne 'Smoke' -or
    $sourceManifest.gameSha256 -ne $knownGameHash -or $sourceManifest.formatVersion -ne 1 -or
    @($sourceManifest.files).Count -ne 19 -or
    @(Compare-Object $expectedPaths @($sourceManifest.files.path)).Count -ne 0 -or
    @($sourceManifest.files.path | Select-Object -Unique).Count -ne 19) {
    throw 'Supply the complete real 19-file Smoke package; synthetic backend binaries are not accepted.'
}
foreach ($entry in $sourceManifest.files) {
    if ((Get-Sha (Join-Path $sourcePackage ('Win64/' + $entry.path))) -ne $entry.sha256) {
        throw "Source package hash differs: $($entry.path)"
    }
}
$outputRoot = Join-Path $repositoryRoot ('Build/mta-movement-package-tests/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$logPath = Join-Path $outputRoot 'production-scripts.log'
$sourceHashes = @($scriptNames | ForEach-Object {
    $relative = 'Client/definitive/' + $_
    [ordered]@{ path=$relative; sha256=(Get-Sha (Join-Path $repositoryRoot $relative)) }
}) + @([ordered]@{ path='Tests/definitive/test_movement_package.ps1'; sha256=(Get-Sha $PSCommandPath) })
$packageHashes = @([ordered]@{ path='manifest.json'; sha256=(Get-Sha (Join-Path $sourcePackage 'manifest.json')) }) + @($sourceManifest.files | ForEach-Object {
    [ordered]@{ path=('Win64/' + $_.path); sha256=$_.sha256 }
})
$script:checks = [Collections.Generic.List[string]]::new()
$script:cases = [Collections.Generic.List[object]]::new()

function Assert-Check([bool]$Condition, [string]$Label) {
    if (-not $Condition) { throw "Check failed: $Label" }
    $script:checks.Add($Label)
}
function Assert-FixturePath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Test mutation escaped the disposable root: $full"
    }
    return $full
}
function Write-FixtureBytes([string]$Path, [byte[]]$Bytes) {
    $target = Assert-FixturePath $Path
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.File]::WriteAllBytes($target, $Bytes)
}
function Copy-FixtureFile([string]$Source, [string]$Destination) {
    $target = Assert-FixturePath $Destination
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.File]::Copy($Source, $target, $false)
}
function Write-FixtureJson([string]$Path, [object]$Value) {
    Write-FixtureBytes $Path ($utf8.GetBytes(($Value | ConvertTo-Json -Depth 10) + "`n"))
}
function Get-TreeFingerprint([string]$Root) {
    # Include directories and backup files: a preflight refusal must not stage
    # writes elsewhere in its fixture repository before reporting the conflict.
    $lines = @(Get-ChildItem -LiteralPath $Root -Force -Recurse | Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\', '/')
        if ($_.PSIsContainer) { 'D ' + $relative } else { 'F ' + $relative + ' ' + (Get-Sha $_.FullName) }
    })
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes(($lines -join "`n")))).ToLowerInvariant()
}
function Invoke-Production([object]$Fixture, [string]$Name, [hashtable]$Arguments) {
    # update.ps1 derives its package root from its own repository. Copying all
    # four unchanged scripts prevents a test from updating the real game root.
    $scriptPath = Join-Path $Fixture.scripts $Name
    Add-Content -LiteralPath $logPath -Value "$($Fixture.name): $Name" -Encoding utf8
    try {
        $output = @(& $scriptPath @Arguments 6>&1)
        foreach ($line in $output) { Add-Content -LiteralPath $logPath -Value $line.ToString() -Encoding utf8 }
    } catch {
        Add-Content -LiteralPath $logPath -Value ('REFUSAL: ' + $_.Exception.Message) -Encoding utf8
        throw
    }
}
function Assert-Refused([scriptblock]$Action, [string]$Pattern, [string]$Label) {
    $message = $null
    try { & $Action } catch { $message = $_.Exception.Message }
    Assert-Check ($null -ne $message -and $message -match $Pattern) "$Label (expected refusal)"
}
function New-GameDirectory([string]$Path) {
    [IO.Directory]::CreateDirectory((Assert-FixturePath $Path)) | Out-Null
    Copy-FixtureFile $sourceExecutable (Join-Path $Path 'SanAndreas.exe')
    return $Path
}
function New-Fixture([string]$Name) {
    $root = Join-Path $outputRoot $Name
    $repo = Join-Path $root 'repository'
    $scripts = Join-Path $repo 'Client/definitive'
    $package = Join-Path $repo 'Build/definitive-package/Smoke'
    foreach ($scriptName in $scriptNames) { Copy-FixtureFile (Join-Path $repositoryRoot ('Client/definitive/' + $scriptName)) (Join-Path $scripts $scriptName) }
    Copy-FixtureFile (Join-Path $sourcePackage 'manifest.json') (Join-Path $package 'manifest.json')
    foreach ($entry in $sourceManifest.files) {
        Copy-FixtureFile (Join-Path $sourcePackage ('Win64/' + $entry.path)) (Join-Path $package ('Win64/' + $entry.path))
    }
    $game = New-GameDirectory (Join-Path $root 'game')
    [pscustomobject]@{ name=$Name; root=$root; repository=$repo; scripts=$scripts; package=$package; game=$game }
}
function Install-Fresh([object]$Fixture) {
    Invoke-Production $Fixture 'install.ps1' @{ Mode='Smoke'; PackageDirectory=$Fixture.package; GameDirectory=$Fixture.game }
}
function Install-Legacy([object]$Fixture, [int]$Count) {
    $excluded = @()
    if ($Count -lt 19) { $excluded += @($movementScript, $backend) }
    if ($Count -eq 16) { $excluded += $padScript }
    $files = @()
    foreach ($entry in $sourceManifest.files) {
        if ($entry.path -in $excluded) { continue }
        $destination = Join-Path $Fixture.game $entry.path
        Copy-FixtureFile (Join-Path $Fixture.package ('Win64/' + $entry.path)) $destination
        if ($entry.path -eq 'CLEO/de-online/config.mjs') {
            # Historical configurations lacked this opt-in. Different owned
            # bytes also prove update replaces existing files, not just adds.
            $text = [IO.File]::ReadAllText($destination)
            if ($Count -lt 19) { $text = [regex]::Replace($text, '(?m)^export const MTA_NATIVE_MOVEMENT_TRIAL = (true|false);\r?\n', '') }
            if ($Count -eq 16) { $text = [regex]::Replace($text, '(?m)^export const MTA_NATIVE_PAD_REFERENCE = (true|false);\r?\n', '') }
            Write-FixtureBytes $destination ($utf8.GetBytes($text + "`n// Historical owned configuration fixture.`n"))
        }
        $files += [ordered]@{ path=$entry.path; destination=$destination; sha256=(Get-Sha $destination) }
    }
    Assert-Check ($files.Count -eq $Count) "$($Fixture.name): legacy ownership count"
    Write-FixtureJson (Join-Path $Fixture.game 'de-online-install-manifest.json') ([ordered]@{
        packageId='de-online-prototype'; formatVersion=1; mode='Smoke'; gameDirectory=$Fixture.game; gameSha256=$knownGameHash; files=$files
    })
    Invoke-Production $Fixture 'uninstall.ps1' @{ GameDirectory=$Fixture.game; ValidateOnly=$true }
}
function Assert-InstalledPackage([object]$Fixture) {
    $manifest = Get-Content -LiteralPath (Join-Path $Fixture.game 'de-online-install-manifest.json') -Raw | ConvertFrom-Json
    Assert-Check (@($manifest.files).Count -eq 19) "$($Fixture.name): 19 owned records"
    Assert-Check (@(Compare-Object $expectedPaths @($manifest.files.path)).Count -eq 0) "$($Fixture.name): exact owned paths"
    foreach ($entry in $manifest.files) {
        $expected = @($sourceManifest.files | Where-Object path -eq $entry.path)[0]
        Assert-Check ($entry.sha256 -eq $expected.sha256 -and (Get-Sha $entry.destination) -eq $expected.sha256) "$($Fixture.name): installed hash $($entry.path)"
    }
    Invoke-Production $Fixture 'uninstall.ps1' @{ GameDirectory=$Fixture.game; ValidateOnly=$true }
}
function Invoke-Case([string]$Name, [scriptblock]$Action) {
    $initialChecks = $script:checks.Count
    $fixture = New-Fixture $Name
    try {
        & $Action $fixture
        $script:cases.Add([ordered]@{ name=$Name; success=$true; checks=($script:checks.Count - $initialChecks); directory=$fixture.root })
    } catch {
        $script:cases.Add([ordered]@{ name=$Name; success=$false; checks=($script:checks.Count - $initialChecks); directory=$fixture.root; failure=$_.Exception.Message })
        throw
    }
}

$failure = $null
try {
    Invoke-Case 'fresh-19-validate-uninstall' {
        param($fixture)
        Install-Fresh $fixture
        Assert-InstalledPackage $fixture
        $userPaths = @('CLEO/logs/trial.log', 'CLEO/.config/user.json', 'CLEO/de-online/custom.mjs', 'CLEO/.types/generated.d.ts', 'profiles/save.dat', 'unrelated.txt')
        $userHashes = @{}
        foreach ($relative in $userPaths) {
            $path = Join-Path $fixture.game $relative
            Write-FixtureBytes $path ($utf8.GetBytes('Generated/user bytes: ' + $relative))
            $userHashes[$relative] = Get-Sha $path
        }
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'ValidateOnly changes no file or directory'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game }
        foreach ($relative in $expectedPaths) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $relative))) "Uninstall removes owned $relative" }
        Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game 'de-online-install-manifest.json'))) 'Uninstall removes ownership manifest'
        foreach ($relative in $userPaths) { Assert-Check ((Get-Sha (Join-Path $fixture.game $relative)) -eq $userHashes[$relative]) "Uninstall preserves $relative" }
        Assert-Check ((Get-Sha (Join-Path $fixture.game 'SanAndreas.exe')) -eq $knownGameHash) 'Uninstall preserves executable'
    }
    foreach ($legacyCount in @(16, 17, 19)) {
        Invoke-Case "update-$legacyCount-to-19" {
            param($fixture)
            Install-Legacy $fixture $legacyCount
            $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
            $oldHash = Get-Sha $config
            Write-FixtureBytes (Join-Path $fixture.game 'CLEO/de-online/user-save.json') ($utf8.GetBytes('user save remains'))
            Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game }
            Assert-InstalledPackage $fixture
            Assert-Check ((Get-Sha $config) -ne $oldHash) "$legacyCount migration updates existing owned config"
            Assert-Check ([IO.File]::ReadAllText((Join-Path $fixture.game 'CLEO/de-online/user-save.json')) -eq 'user save remains') "$legacyCount migration preserves user data"
            $before = Get-TreeFingerprint $fixture.root
            Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game }
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) "$legacyCount migration repeated update is a no-op"
        }
    }
    foreach ($legacyCount in @(16, 17)) {
        $occupiedPaths = @($movementScript, $backend)
        if ($legacyCount -eq 16) { $occupiedPaths += $padScript }
        foreach ($occupied in $occupiedPaths) {
            Invoke-Case ("occupied-$legacyCount-" + [IO.Path]::GetFileName($occupied)) {
                param($fixture)
                Install-Legacy $fixture $legacyCount
                Write-FixtureBytes (Join-Path $fixture.game $occupied) ($utf8.GetBytes('Existing unowned user bytes'))
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game } } 'New owned reference path already exists' 'Occupied additive path'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Occupied additive refusal precedes all writes and backups'
            }
        }
    }
    foreach ($missing in @($movementScript, $backend)) {
        Invoke-Case ('unpaired-package-' + [IO.Path]::GetFileName($missing)) {
            param($fixture)
            Install-Legacy $fixture 17
            $manifestPath = Join-Path $fixture.package 'manifest.json'
            $package = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $package.files = @($package.files | Where-Object path -ne $missing)
            Write-FixtureJson $manifestPath $package
            Remove-Item -LiteralPath (Assert-FixturePath (Join-Path $fixture.package ('Win64/' + $missing)))
            $freshGame = New-GameDirectory (Join-Path $fixture.root 'fresh-game')
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game } } 'Incomplete MTA movement package ownership' 'Unpaired update package'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired update refusal precedes all writes'
            Assert-Refused { Invoke-Production $fixture 'install.ps1' @{ Mode='Smoke'; PackageDirectory=$fixture.package; GameDirectory=$freshGame } } 'Package manifest must contain exactly' 'Unpaired fresh package'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired install refusal precedes all writes'
        }
    }
    Invoke-Case 'explicit-mode-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaNativeMovementTrial=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_NATIVE_MOVEMENT_TRIAL = true;') 'Separate Smoke opt-in arms movement trial'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Online'; MtaNativeMovementTrial=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Online write opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid mode opt-in leaves bytes unchanged'
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaNativeMovementTrial=$true; MtaNativePadReference=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Conflicting reference opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Conflicting opt-in leaves bytes unchanged'
        foreach ($mode in @('Smoke', 'Online', 'Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; GameDirectory=$fixture.game }
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_NATIVE_MOVEMENT_TRIAL = false;') "Ordinary $mode clears hidden write flag"
            Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        }
    }
    Invoke-Case 'legacy-cannot-enable-missing-backend' {
        param($fixture)
        Install-Legacy $fixture 17
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config) + "`nexport const MTA_NATIVE_MOVEMENT_TRIAL = false;`n"))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaNativeMovementTrial=$true; GameDirectory=$fixture.game } } 'owned backend files are missing' 'Missing owned backend pair'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing backend opt-in leaves bytes unchanged'
    }
} catch { $failure = $_.Exception.Message }
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) { if ((Get-Sha (Join-Path $repositoryRoot $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
foreach ($entry in $packageHashes) { if ((Get-Sha (Join-Path $sourcePackage $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
$sourceExecutableUnchanged = (Get-Sha $sourceExecutable) -eq $knownGameHash
$record = [ordered]@{
    recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot; success=($null -eq $failure -and $sourceUnchanged -and $sourceExecutableUnchanged);
    checks=$script:checks.Count; cases=$script:cases; checkLabels=$script:checks; failure=$failure;
    sourceHashes=$sourceHashes; packageRoot=$sourcePackage; packageHashes=$packageHashes; sourceUnchanged=$sourceUnchanged;
    gameExecutableSource=$sourceExecutable; gameExecutableSha256=$knownGameHash; sourceExecutableUnchanged=$sourceExecutableUnchanged;
    actualGameInstallationChanged=$false; gameLaunched=$false; gameProcessAttached=$false; nativeInputWriteVerified=$false
}
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Movement package tests failed: $failure; see $outputRoot" }
$record | ConvertTo-Json -Depth 10 -Compress
