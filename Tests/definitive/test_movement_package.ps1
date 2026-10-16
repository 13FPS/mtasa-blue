#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production ownership scripts against disposable game directories.
# Copying the verified executable supplies the install fingerprint without ever
# launching it or modifying the user's installation.
[CmdletBinding()]
param(
    [string]$PackageRoot,
    [string]$GuiPackagePath,
    [string]$SurfacePackagePath,
    [string]$FrontendPackagePath,
    [switch]$FrontendOnly,
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
$coreModule = 'CLEO/de-online/core.dll'
$pthreadModule = 'CLEO/de-online/pthread.dll'
$xmlModule = 'CLEO/de-online/xmll.dll'
$bindConfig = 'CLEO/de-online/mta/config/coreconfig.xml'
$scriptNames = @('install.ps1', 'update.ps1', 'uninstall.ps1', 'set-mode.ps1', 'package.ps1', 'set-loader.ps1')
$expectedPaths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll', $padScript, $movementScript, $backend,
    $coreModule, $pthreadModule, $xmlModule, $bindConfig,
    'DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt', 'DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt',
    'DE_ONLINE_LICENSES/MTA_GPLv3.txt', 'DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md'
)
$guiPaths = @('CLEO/de-online/cgui.dll',
    'CLEO/de-online/mta/cgui/sans.ttf', 'CLEO/de-online/mta/cgui/saheader.ttf', 'CLEO/de-online/mta/cgui/sagothic.ttf',
    'CLEO/de-online/mta/cgui/Imageset.xsd', 'CLEO/de-online/mta/cgui/GUIScheme.xsd', 'CLEO/de-online/mta/cgui/Font.xsd',
    'CLEO/de-online/mta/cgui/Falagard.xsd', 'CLEO/de-online/mta/cgui/unifont.ttf',
    'CLEO/de-online/skins/Default/CGUI.xml', 'CLEO/de-online/skins/Default/CGUI.is.xml',
    'CLEO/de-online/skins/Default/CGUI.lnf.xml', 'CLEO/de-online/skins/Default/CGUI.png')

$frontendPaths = @(
    'CLEO/de-online/mta/cgui/images/background.png',
    'CLEO/de-online/mta/cgui/images/background_logo.png',
    'CLEO/de-online/mta/cgui/images/mta_filler.png',
    'CLEO/de-online/mta/cgui/images/version.png',
    'CLEO/de-online/mta/cgui/images/the_language_icon.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/locked.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search-servers.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search-players.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/refresh.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/favorite.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/connect.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/info.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/trashcan.png',
    'CLEO/de-online/mta/locale/en_US/menu_disconnect.png',
    'CLEO/de-online/mta/locale/en_US/menu_quick_connect.png',
    'CLEO/de-online/mta/locale/en_US/menu_browse_servers.png',
    'CLEO/de-online/mta/locale/en_US/menu_settings.png',
    'CLEO/de-online/mta/locale/en_US/menu_about.png',
    'CLEO/de-online/mta/locale/en_US/menu_quit.png',
    'CLEO/de-online/mta/locale/en_US/latest_news.png',
    'CLEO/de-online/mta/locale/en_US/client.pot')

function Get-Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
if ((Get-Sha $sourceExecutable) -ne $knownGameHash) { throw 'A copy of the exact verified Steam executable is required.' }
if (Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue) {
    throw 'Finish game and SanAndreas.exe host-fixture processes before running these ownership tests.'
}
$sourceManifest = Get-Content -LiteralPath (Join-Path $sourcePackage 'manifest.json') -Raw | ConvertFrom-Json
if ($sourceManifest.packageId -ne 'de-online-prototype' -or $sourceManifest.mode -ne 'Smoke' -or
    $sourceManifest.gameSha256 -ne $knownGameHash -or $sourceManifest.formatVersion -ne 1 -or
    @($sourceManifest.files).Count -ne 23 -or
    @(Compare-Object $expectedPaths @($sourceManifest.files.path)).Count -ne 0 -or
    @($sourceManifest.files.path | Select-Object -Unique).Count -ne 23) {
    throw 'Supply the complete real 23-file Smoke package; synthetic backend binaries are not accepted.'
}
foreach ($entry in $sourceManifest.files) {
    if ((Get-Sha (Join-Path $sourcePackage ('Win64/' + $entry.path))) -ne $entry.sha256) {
        throw "Source package hash differs: $($entry.path)"
    }
}
$guiPackage = $null
$guiManifest = $null
$guiPackageHashes = @()
if ($GuiPackagePath) {
    $guiPackage = (Resolve-Path -LiteralPath $GuiPackagePath).Path
    $guiManifest = Get-Content -LiteralPath (Join-Path $guiPackage 'manifest.json') -Raw | ConvertFrom-Json
    if ($guiManifest.packageId -ne 'de-online-prototype' -or $guiManifest.mode -ne 'Smoke' -or $guiManifest.gameSha256 -ne $knownGameHash -or $guiManifest.formatVersion -ne 1 -or
        @($guiManifest.files).Count -ne 36 -or @($guiManifest.files.path | Select-Object -Unique).Count -ne 36 -or @(Compare-Object ($expectedPaths + $guiPaths) @($guiManifest.files.path)).Count -ne 0) {
        throw 'Supply the complete real 36-file GUI Smoke package.'
    }
    foreach ($entry in $guiManifest.files) {
        if ((Get-Sha (Join-Path $guiPackage ('Win64/' + $entry.path))) -ne $entry.sha256) { throw "GUI source package hash differs: $($entry.path)" }
        if ($entry.path -in $expectedPaths -and $entry.path -ne 'CLEO/de-online/config.mjs') {
            if (@($sourceManifest.files | Where-Object path -eq $entry.path)[0].sha256 -ne $entry.sha256) { throw "GUI and ordinary backend bytes differ: $($entry.path)" }
        }
    }
    $guiConfig = [IO.File]::ReadAllText((Join-Path $guiPackage 'Win64/CLEO/de-online/config.mjs'))
    if ([regex]::Matches($guiConfig, 'export const MTA_CORE_GUI_STARTUP_REFERENCE = true;').Count -ne 1 -or $guiConfig -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE) = true;') {
        throw 'GUI ownership tests require an explicitly armed, separate GUI startup package.'
    }
    $guiPackageHashes = @([ordered]@{ path='manifest.json'; sha256=(Get-Sha (Join-Path $guiPackage 'manifest.json')) }) + @($guiManifest.files | ForEach-Object { [ordered]@{ path=('Win64/' + $_.path); sha256=$_.sha256 } })
}
# A real surface package supplies the same exact assets, while its distinct
# marker proves that asset ownership alone cannot select a visible workload.
$surfacePackage = $null
$surfaceManifest = $null
$surfacePackageHashes = @()
if ($SurfacePackagePath) {
    $surfacePackage = (Resolve-Path -LiteralPath $SurfacePackagePath).Path
    $surfaceManifest = Get-Content -LiteralPath (Join-Path $surfacePackage 'manifest.json') -Raw | ConvertFrom-Json
    if ($surfaceManifest.packageId -ne 'de-online-prototype' -or $surfaceManifest.mode -ne 'Smoke' -or $surfaceManifest.gameSha256 -ne $knownGameHash -or $surfaceManifest.formatVersion -ne 1 -or
        @($surfaceManifest.files).Count -ne 36 -or @($surfaceManifest.files.path | Select-Object -Unique).Count -ne 36 -or @(Compare-Object ($expectedPaths + $guiPaths) @($surfaceManifest.files.path)).Count -ne 0) {
        throw 'Supply the complete real 36-file GUI surface Smoke package.'
    }
    foreach ($entry in $surfaceManifest.files) {
        if ((Get-Sha (Join-Path $surfacePackage ('Win64/' + $entry.path))) -ne $entry.sha256) { throw "Surface source package hash differs: $($entry.path)" }
        if ($entry.path -in $expectedPaths -and $entry.path -ne 'CLEO/de-online/config.mjs') {
            if (@($sourceManifest.files | Where-Object path -eq $entry.path)[0].sha256 -ne $entry.sha256) { throw "Surface and ordinary backend bytes differ: $($entry.path)" }
        }
        if ($guiManifest -and $entry.path -in $guiPaths -and @($guiManifest.files | Where-Object path -eq $entry.path)[0].sha256 -ne $entry.sha256) { throw "GUI surface asset bytes differ: $($entry.path)" }
    }
    $surfaceConfig = [IO.File]::ReadAllText((Join-Path $surfacePackage 'Win64/CLEO/de-online/config.mjs'))
    if ([regex]::Matches($surfaceConfig, 'export const MTA_CORE_GUI_SURFACE_REFERENCE = true;').Count -ne 1 -or $surfaceConfig -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE) = true;') {
        throw 'Surface ownership tests require an explicitly armed, separate GUI surface package.'
    }
    $surfacePackageHashes = @([ordered]@{ path='manifest.json'; sha256=(Get-Sha (Join-Path $surfacePackage 'manifest.json')) }) + @($surfaceManifest.files | ForEach-Object { [ordered]@{ path=('Win64/' + $_.path); sha256=$_.sha256 } })
}
$frontendPackage = $null
$frontendManifest = $null
$frontendPackageHashes = @()
if ($FrontendPackagePath) {
    $frontendPackage = (Resolve-Path -LiteralPath $FrontendPackagePath).Path
    $frontendManifest = Get-Content -LiteralPath (Join-Path $frontendPackage 'manifest.json') -Raw | ConvertFrom-Json
    if ($frontendManifest.packageId -ne 'de-online-prototype' -or $frontendManifest.mode -ne 'Smoke' -or $frontendManifest.gameSha256 -ne $knownGameHash -or $frontendManifest.formatVersion -ne 1 -or
        @($frontendManifest.files).Count -ne 58 -or @($frontendManifest.files.path | Select-Object -Unique).Count -ne 58 -or @(Compare-Object ($expectedPaths + $guiPaths + $frontendPaths) @($frontendManifest.files.path)).Count -ne 0) {
        throw 'Supply the complete real 58-file MTA frontend Smoke package.'
    }
    foreach ($entry in $frontendManifest.files) {
        if ((Get-Sha (Join-Path $frontendPackage ('Win64/' + $entry.path))) -ne $entry.sha256) { throw "Frontend source package hash differs: $($entry.path)" }
        if ($entry.path -in $expectedPaths -and $entry.path -ne 'CLEO/de-online/config.mjs' -and @($sourceManifest.files | Where-Object path -eq $entry.path)[0].sha256 -ne $entry.sha256) { throw "Frontend and ordinary backend bytes differ: $($entry.path)" }
        if ($guiManifest -and $entry.path -in $guiPaths -and @($guiManifest.files | Where-Object path -eq $entry.path)[0].sha256 -ne $entry.sha256) { throw "Frontend GUI asset bytes differ: $($entry.path)" }
    }
    $frontendConfig = [IO.File]::ReadAllText((Join-Path $frontendPackage 'Win64/CLEO/de-online/config.mjs'))
    if ([regex]::Matches($frontendConfig, 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = true;').Count -ne 1 -or $frontendConfig -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE) = true;') {
        throw 'Frontend ownership tests require an explicitly armed, separate MTA GUI frontend package.'
    }
    $frontendPackageHashes = @([ordered]@{ path='manifest.json'; sha256=(Get-Sha (Join-Path $frontendPackage 'manifest.json')) }) + @($frontendManifest.files | ForEach-Object { [ordered]@{ path=('Win64/' + $_.path); sha256=$_.sha256 } })
}
if ($FrontendOnly -and -not $frontendPackage) { throw 'FrontendOnly requires the real frontend package.' }
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
    # unchanged scripts prevents a test from updating the real game root.
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
function Invoke-Loader([object]$Fixture, [string]$Action) {
    # The copied helper derives its journal root from the disposable repository,
    # so even loader recovery records remain inside the fixture under test.
    $scriptPath = Join-Path $Fixture.scripts 'set-loader.ps1'
    Add-Content -LiteralPath $logPath -Value "$($Fixture.name): set-loader.ps1 $Action" -Encoding utf8
    $result = & $scriptPath -GameDirectory $Fixture.game -Action $Action 6>$null
    Add-Content -LiteralPath $logPath -Value ($result | ConvertTo-Json -Compress) -Encoding utf8
    return $result
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
function Select-FixturePackage([object]$Fixture, [bool]$Gui, [bool]$Surface = $false, [bool]$Frontend = $false) {
    # Switch only the disposable package tree; current installed ownership is
    # left intact so update exercises the real 23-to-36 and 36-to-23 transaction.
    $old = Get-Content -LiteralPath (Join-Path $Fixture.package 'manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $old.files) { Remove-Item -LiteralPath (Assert-FixturePath (Join-Path $Fixture.package ('Win64/' + $entry.path))) }
    Remove-Item -LiteralPath (Assert-FixturePath (Join-Path $Fixture.package 'manifest.json'))
    $source = if ($Frontend) { $frontendPackage } elseif ($Surface) { $surfacePackage } elseif ($Gui) { $guiPackage } else { $sourcePackage }
    $manifest = if ($Frontend) { $frontendManifest } elseif ($Surface) { $surfaceManifest } elseif ($Gui) { $guiManifest } else { $sourceManifest }
    Copy-FixtureFile (Join-Path $source 'manifest.json') (Join-Path $Fixture.package 'manifest.json')
    foreach ($entry in $manifest.files) { Copy-FixtureFile (Join-Path $source ('Win64/' + $entry.path)) (Join-Path $Fixture.package ('Win64/' + $entry.path)) }
}
function Assert-GuiInstalled([object]$Fixture, [bool]$Surface = $false) {
    $manifest = Get-Content -LiteralPath (Join-Path $Fixture.game 'de-online-install-manifest.json') -Raw | ConvertFrom-Json
    Assert-Check (@($manifest.files).Count -eq 36 -and @(Compare-Object ($expectedPaths + $guiPaths) @($manifest.files.path)).Count -eq 0) 'GUI exact complete36 ownership'
    foreach ($entry in $manifest.files) {
        $expectedManifest = if ($Surface) { $surfaceManifest } else { $guiManifest }
        $expected = @($expectedManifest.files | Where-Object path -eq $entry.path)[0]
        Assert-Check ($entry.sha256 -eq $expected.sha256 -and (Get-Sha $entry.destination) -eq $expected.sha256) ('GUI owned hash ' + $entry.path)
    }
    Invoke-Production $Fixture 'uninstall.ps1' @{GameDirectory=$Fixture.game;ValidateOnly=$true}
}
function Install-Legacy([object]$Fixture, [int]$Count) {
    $excluded = @()
    if ($Count -lt 23) { $excluded += @($xmlModule, $bindConfig) }
    if ($Count -lt 21) { $excluded += @($coreModule, $pthreadModule) }
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
            # The first23-file generation was read-only and had no Core write
            # marker, even though the new candidate owns the same file paths.
            $text = [regex]::Replace($text, '(?m)^export const MTA_CORE_MOVEMENT_REFERENCE = (true|false);\r?\n', '')
            $text = [regex]::Replace($text, '(?m)^export const MTA_CORE_RENDER_REFERENCE = (true|false);\r?\n', '')
            if ($Count -lt 23) { $text = [regex]::Replace($text, '(?m)^export const MTA_CORE_INPUT_REFERENCE = (true|false);\r?\n', '') }
            if ($Count -lt 21) { $text = [regex]::Replace($text, '(?m)^export const MTA_CORE_STARTUP_REFERENCE = (true|false);\r?\n', '') }
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
    Assert-Check (@($manifest.files).Count -eq 23) "$($Fixture.name): 23 owned records"
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
    if ($FrontendOnly) {
        Invoke-Case 'frontend-bounded-legacy23-and-gui36-acceptance' {
            param($fixture)
            Install-Fresh $fixture
            Assert-InstalledPackage $fixture
            Invoke-Production $fixture 'set-loader.ps1' @{Action='Status';GameDirectory=$fixture.game}
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game}
            if ($guiPackage) {
                Select-FixturePackage $fixture $true
                Install-Fresh $fixture
                Assert-GuiInstalled $fixture
                Invoke-Production $fixture 'set-loader.ps1' @{Action='Status';GameDirectory=$fixture.game}
                Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game}
            }
        }
    }
    if (-not $FrontendOnly) {
    Invoke-Case 'fresh-23-validate-uninstall' {
        param($fixture)
        Install-Fresh $fixture
        Assert-InstalledPackage $fixture
        $userPaths = @('CLEO/logs/trial.log', 'CLEO/.config/user.json', 'CLEO/de-online/custom.mjs', 'CLEO/.types/generated.d.ts', 'CLEO/de-online/mta/config/user.xml', 'profiles/save.dat', 'unrelated.txt')
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
    foreach ($legacyCount in @(16, 17, 19, 21, 23)) {
        Invoke-Case "update-$legacyCount-to-23" {
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
    foreach ($legacyCount in @(16, 17, 19, 21)) {
        $occupiedPaths = @($xmlModule, $bindConfig)
        if ($legacyCount -lt 21) { $occupiedPaths += @($coreModule, $pthreadModule) }
        if ($legacyCount -lt 19) { $occupiedPaths += @($movementScript, $backend) }
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
    foreach ($missing in @($movementScript, $backend, $coreModule, $pthreadModule, $xmlModule, $bindConfig)) {
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
            $expectedRefusal = if ($missing -in @($xmlModule, $bindConfig)) { 'Incomplete MTA Core configured input package ownership' } elseif ($missing -in @($coreModule, $pthreadModule)) { 'Incomplete MTA Core startup package ownership' } else { 'Incomplete MTA movement package ownership' }
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game } } $expectedRefusal 'Unpaired update package'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired update refusal precedes all writes'
            Assert-Refused { Invoke-Production $fixture 'install.ps1' @{ Mode='Smoke'; PackageDirectory=$fixture.package; GameDirectory=$freshGame } } 'Package manifest must contain exactly' 'Unpaired fresh package'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired install refusal precedes all writes'
        }
    }
    foreach ($missing in @($coreModule, $pthreadModule, $xmlModule, $bindConfig)) {
        Invoke-Case ('unpaired-installed-' + [IO.Path]::GetFileName($missing)) {
            param($fixture)
            Install-Legacy $fixture 23
            $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $manifest.files = @($manifest.files | Where-Object path -ne $missing)
            Write-FixtureJson $manifestPath $manifest
            $before = Get-TreeFingerprint $fixture.root
            $expectedRefusal = if ($missing -in @($xmlModule, $bindConfig)) { 'Incomplete MTA Core configured input package ownership' } else { 'Incomplete MTA Core startup package ownership' }
            Assert-Refused { Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true } } $expectedRefusal 'Unpaired installed validation'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired installed validation changes no bytes'
            Assert-Refused { Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game } } $expectedRefusal 'Unpaired uninstall'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired uninstall removes no bytes'
            Assert-Refused { Invoke-Production $fixture 'set-loader.ps1' @{ GameDirectory=$fixture.game; Action='Disable' } } $expectedRefusal 'Unpaired loader bypass'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Unpaired loader bypass changes no bytes'
        }
    }
    foreach ($ownedCount in @(16, 17, 19, 21, 23)) {
        Invoke-Case "loader-round-trip-$ownedCount" {
            param($fixture)
            Install-Legacy $fixture $ownedCount
            Write-FixtureBytes (Join-Path $fixture.game 'CLEO/de-online/user-loader-state.json') ($utf8.GetBytes('Retained user bytes'))
            $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
            $manifestHash = Get-Sha $manifestPath
            $loaderHash = Get-Sha (Join-Path $fixture.game 'version.dll')
            $gameBefore = Get-TreeFingerprint $fixture.game
            $beforeStatus = Get-TreeFingerprint $fixture.root
            $status = Invoke-Loader $fixture 'Status'
            Assert-Check ($status.state -eq 'Enabled' -and $status.verifiedFiles -eq $ownedCount) "$ownedCount loader status validates every owned file"
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $beforeStatus) "$ownedCount loader status changes no bytes"
            $disabled = Invoke-Loader $fixture 'Disable'
            Assert-Check ($disabled.state -eq 'Disabled' -and $disabled.verifiedFiles -eq $ownedCount) "$ownedCount loader disable returns validated state"
            Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game 'version.dll')) -and
                (Get-Sha (Join-Path $fixture.game 'version.dll.de-online-disabled')) -eq $loaderHash) "$ownedCount loader disable only renames the verified loader"
            Assert-Check ((Get-Sha $manifestPath) -eq $manifestHash) "$ownedCount loader disable preserves manifest"
            $beforeStatus = Get-TreeFingerprint $fixture.root
            $status = Invoke-Loader $fixture 'Status'
            Assert-Check ($status.state -eq 'Disabled' -and $status.verifiedFiles -eq $ownedCount) "$ownedCount disabled status validates every owned file"
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $beforeStatus) "$ownedCount disabled status changes no bytes"
            $enabled = Invoke-Loader $fixture 'Enable'
            Assert-Check ($enabled.state -eq 'Enabled' -and $enabled.verifiedFiles -eq $ownedCount) "$ownedCount loader enable returns validated state"
            Assert-Check ((Get-TreeFingerprint $fixture.game) -eq $gameBefore) "$ownedCount loader round trip restores every game/user byte and path"
            Assert-Check ((Get-Sha $manifestPath) -eq $manifestHash) "$ownedCount loader round trip preserves manifest"
            foreach ($record in @($disabled, $enabled)) {
                Assert-Check ($record.recoveryDirectory.StartsWith($fixture.repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) "$ownedCount loader journal remains in disposable repository"
                Assert-Check ((Test-Path -LiteralPath (Join-Path $record.recoveryDirectory 'prepared.json')) -and
                    (Test-Path -LiteralPath (Join-Path $record.recoveryDirectory 'completed.json'))) "$ownedCount loader move has durable intent/completion records"
            }
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
    Invoke-Case 'explicit-core-startup-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreStartupReference=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_STARTUP_REFERENCE = true;') 'Separate Smoke opt-in arms Core startup reference'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_NATIVE_MOVEMENT_TRIAL = false;') 'Core startup reference clears movement writes'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        $before = Get-TreeFingerprint $fixture.root
        foreach ($invalidMode in @('Diagnostic', 'Online')) {
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$invalidMode; MtaCoreStartupReference=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Core startup invalid mode'
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{ Mode=$invalidMode; MtaCoreStartupReference=$true } } 'separate opt-in Smoke mode' 'Core package invalid mode'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid Core startup mode changes no bytes'
        }
        foreach ($conflict in @('MtaNativeMovementTrial', 'MtaNativePadReference', 'NativeInputReference', 'NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot', 'MtaCoreInputReference', 'MtaCoreMovementReference')) {
            $arguments = @{ Mode='Smoke'; MtaCoreStartupReference=$true; GameDirectory=$fixture.game }
            $arguments[$conflict] = $true
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core startup opt-in'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core package opt-in'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Conflicting Core startup opt-in changes no bytes'
        }
        foreach ($mode in @('Diagnostic', 'Smoke', 'Online', 'Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; GameDirectory=$fixture.game }
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_STARTUP_REFERENCE = false;') "Ordinary $mode clears Core startup flag"
            Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        }
    }
    Invoke-Case 'legacy-core-startup-requires-owned-pair' {
        param($fixture)
        Install-Legacy $fixture 19
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -notmatch 'MTA_CORE_STARTUP_REFERENCE') 'Ordinary mode accepts intact older configuration without Core marker'
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreStartupReference=$true; GameDirectory=$fixture.game } } 'Update the adapter before selecting MTA Core startup reference' 'Legacy missing Core marker'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing Core marker changes no bytes'
        Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config) + "`nexport const MTA_CORE_STARTUP_REFERENCE = false;`n"))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreStartupReference=$true; GameDirectory=$fixture.game } } 'owned Core backend files are missing' 'Legacy missing owned Core pair'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing owned Core pair changes no bytes'
    }
    Invoke-Case 'explicit-core-input-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        $xml = [xml][IO.File]::ReadAllText((Join-Path $fixture.game $bindConfig))
        Assert-Check ($xml.mainconfig.binds.bind.Count -eq 6) 'Core input XML contains only six explicit configured movement bindings'
        Assert-Check ((($xml.mainconfig.binds.bind | ForEach-Object { $_.key + ':' + $_.control }) -join ',') -eq 'w:forwards,s:backwards,a:left,d:right,lshift:sprint,rshift:walk') 'Core input XML uses actual MTA key/control names'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreInputReference=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_INPUT_REFERENCE = true;') 'Separate Smoke opt-in arms Core configured input'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_STARTUP_REFERENCE = false;') 'Core configured input clears startup probe'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        $before = Get-TreeFingerprint $fixture.root
        foreach ($invalidMode in @('Diagnostic', 'Online')) {
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$invalidMode; MtaCoreInputReference=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Core input invalid mode'
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{ Mode=$invalidMode; MtaCoreInputReference=$true } } 'separate opt-in Smoke mode' 'Core input package invalid mode'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid Core input mode changes no bytes'
        }
        foreach ($conflict in @('MtaCoreStartupReference', 'MtaNativeMovementTrial', 'MtaNativePadReference', 'NativeInputReference', 'NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot', 'MtaCoreMovementReference')) {
            $arguments = @{ Mode='Smoke'; MtaCoreInputReference=$true; GameDirectory=$fixture.game }
            $arguments[$conflict] = $true
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core input opt-in'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core input package opt-in'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Conflicting Core input changes no bytes'
        }
        foreach ($mode in @('Diagnostic', 'Smoke', 'Online', 'Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; GameDirectory=$fixture.game }
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_INPUT_REFERENCE = false;') "Ordinary $mode clears Core configured input"
        }
    }
    Invoke-Case 'legacy-core-input-requires-xml-pair' {
        param($fixture)
        Install-Legacy $fixture 21
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -notmatch 'MTA_CORE_INPUT_REFERENCE') 'Ordinary mode accepts older config without Core input marker'
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreInputReference=$true; GameDirectory=$fixture.game } } 'Update the adapter before selecting MTA Core configured input reference' 'Legacy missing Core input marker'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing input marker changes no bytes'
        Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config) + "`nexport const MTA_CORE_INPUT_REFERENCE = false;`n"))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreInputReference=$true; GameDirectory=$fixture.game } } 'owned XML/config files are missing' 'Legacy missing XML/config pair'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing XML/config changes no bytes'
    }
    foreach ($parentPath in @('CLEO/de-online/mta', 'CLEO/de-online/mta/config')) {
        Invoke-Case ('foreign-config-parent-' + [IO.Path]::GetFileName($parentPath)) {
            param($fixture)
            Install-Legacy $fixture 21
            Write-FixtureBytes (Join-Path $fixture.game $parentPath) ($utf8.GetBytes('User file occupying configuration parent'))
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game } } 'parent is not a directory' 'Foreign configuration parent file'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Foreign parent refusal precedes backups and writes'
        }
    }
    Invoke-Case 'modified-owned-config-blocks-update' {
        param($fixture)
        Install-Fresh $fixture
        Write-FixtureBytes (Join-Path $fixture.game $bindConfig) ($utf8.GetBytes('<mainconfig><binds><bind key="up" control="forwards" /></binds></mainconfig>'))
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'update.ps1' @{ GameDirectory=$fixture.game } } 'Installed file was modified' 'Edited MTA configuration'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Edited MTA configuration is retained unchanged'
    }
    Invoke-Case 'explicit-core-movement-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreMovementReference=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_MOVEMENT_REFERENCE = true;') 'Separate Smoke opt-in arms Core configured writes'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_INPUT_REFERENCE = false;') 'Core writes clear read-only Core input'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_STARTUP_REFERENCE = false;') 'Core writes clear startup reference'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        $before = Get-TreeFingerprint $fixture.root
        foreach ($invalidMode in @('Diagnostic', 'Online')) {
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$invalidMode; MtaCoreMovementReference=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Core write invalid mode'
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{ Mode=$invalidMode; MtaCoreMovementReference=$true } } 'separate opt-in Smoke mode' 'Core write package invalid mode'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid Core write mode changes no bytes'
        }
        foreach ($conflict in @('MtaCoreInputReference', 'MtaCoreStartupReference', 'MtaNativeMovementTrial', 'MtaNativePadReference', 'NativeInputReference', 'NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot')) {
            $arguments = @{ Mode='Smoke'; MtaCoreMovementReference=$true; GameDirectory=$fixture.game }
            $arguments[$conflict] = $true
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core write opt-in'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting Core write package opt-in'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Conflicting Core write selection changes no bytes'
        }
        foreach ($mode in @('Diagnostic', 'Smoke', 'Online', 'Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; GameDirectory=$fixture.game }
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_MOVEMENT_REFERENCE = false;') "Ordinary $mode clears Core writes"
        }
    }
    Invoke-Case 'legacy23-core-movement-requires-new-marker' {
        param($fixture)
        Install-Legacy $fixture 23
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -notmatch 'MTA_CORE_MOVEMENT_REFERENCE') 'Ordinary mode accepts historical23-file configuration'
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreMovementReference=$true; GameDirectory=$fixture.game } } 'Update the adapter before selecting MTA Core configured movement reference' 'Historical23 cannot infer write opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing Core write marker changes no bytes'
    }
    Invoke-Case 'explicit-core-render-observation-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; GameDirectory=$fixture.game; WhatIf=$true }
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Render WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_REFERENCE = true;') 'Explicit separate renderer observation is armed'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_MOVEMENT_REFERENCE = false;') 'Renderer observation clears Core input writes'
        Assert-Check ([IO.File]::ReadAllText($config) -match "export const MTA_CORE_RENDER_ROUTE = 'd3d11';") 'Default explicit observer retains D3D11 route'
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; GameDirectory=$fixture.game; WhatIf=$true }
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'D3D12 route WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match "export const MTA_CORE_RENDER_ROUTE = 'd3d12';") 'D3D12 observer route requires explicit selection'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_GPU_REFERENCE = false;') 'API2 route does not enable COM queries'
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; MtaCoreRenderGpuReference=$true; GameDirectory=$fixture.game; WhatIf=$true }
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'GPU query WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; MtaCoreRenderGpuReference=$true; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_GPU_REFERENCE = true;') 'COM queries require their own explicit opt-in'
        Invoke-Production $fixture 'uninstall.ps1' @{ GameDirectory=$fixture.game; ValidateOnly=$true }
        $before = Get-TreeFingerprint $fixture.root
        foreach ($invalid in @(@{MtaCoreRenderGpuReference=$true}, @{MtaCoreRenderReference=$true;MtaCoreRenderGpuReference=$true})) {
            $arguments = @{Mode='Smoke';GameDirectory=$fixture.game} + $invalid
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'GPU ownership queries require explicit D3D12' 'GPU query requires separate observer and route'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'GPU ownership queries require explicit D3D12' 'GPU query package requires separate observer and route'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected GPU query changes no bytes'
        }
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderRoute='D3D12'; GameDirectory=$fixture.game } } 'explicit renderer route requires' 'Route cannot enable an observer by itself'
        Assert-Refused { Invoke-Production $fixture 'package.ps1' @{ Mode='Smoke'; MtaCoreRenderRoute='D3D12' } } 'explicit renderer route requires' 'Route package requires separate opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Route-only rejection changes no bytes'
        foreach ($mode in @('Diagnostic', 'Online')) {
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; MtaCoreRenderReference=$true; GameDirectory=$fixture.game } } 'separate opt-in Smoke mode' 'Render invalid mode'
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{ Mode=$mode; MtaCoreRenderReference=$true } } 'separate opt-in Smoke mode' 'Render package invalid mode'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected render mode changes no bytes'
        }
        foreach ($conflict in @('MtaCoreMovementReference', 'MtaCoreInputReference', 'MtaCoreStartupReference', 'MtaNativeMovementTrial', 'MtaNativePadReference', 'NativeInputReference', 'NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot')) {
            $arguments = @{ Mode='Smoke'; MtaCoreRenderReference=$true; GameDirectory=$fixture.game }
            $arguments[$conflict] = $true
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting render opt-in'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'separate opt-in Smoke mode' 'Conflicting render package'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Conflicting render trial changes no bytes'
        }
        foreach ($mode in @('Diagnostic', 'Smoke', 'Online', 'Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' @{ Mode=$mode; GameDirectory=$fixture.game }
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_REFERENCE = false;') "Ordinary $mode clears render observation"
            Assert-Check ([IO.File]::ReadAllText($config) -match "export const MTA_CORE_RENDER_ROUTE = 'd3d11';") "Ordinary $mode resets the inactive route"
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_GPU_REFERENCE = false;') "Ordinary $mode clears COM queries"
        }
    }
    Invoke-Case 'explicit-custom-present-classification-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;') 'Custom-present recognition is disabled in the ordinary Smoke package'
        $selection = @{Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; MtaCoreRenderGpuReference=$true; MtaCoreRenderCustomPresentReference=$true; GameDirectory=$fixture.game}
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' ($selection + @{WhatIf=$true})
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'API4 classification WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' $selection
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = true;') 'API4 classification requires its separate explicit opt-in'
        Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game; ValidateOnly=$true}
        foreach ($gpu in @($true,$false)) {
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; MtaCoreRenderGpuReference=$gpu; GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;') 'Returning to API2/API3 clears static custom-present recognition'
            Invoke-Production $fixture 'set-mode.ps1' $selection
        }
        $before = Get-TreeFingerprint $fixture.root
        foreach ($invalid in @(@{MtaCoreRenderCustomPresentReference=$true},
                @{MtaCoreRenderCustomPresentReference=$true;MtaCoreRenderGpuReference=$true},
                @{MtaCoreRenderCustomPresentReference=$true;MtaCoreRenderReference=$true;MtaCoreRenderRoute='D3D12'},
                @{MtaCoreRenderCustomPresentReference=$true;MtaCoreRenderGpuReference=$true;MtaCoreRenderReference=$true;MtaCoreRenderRoute='D3D11'})) {
            $arguments = @{Mode='Smoke';GameDirectory=$fixture.game} + $invalid
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'Custom-present recognition requires explicit D3D12 Core GPU observation' 'API4 selection requires observer, GPU queries and D3D12'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'Custom-present recognition requires explicit D3D12 Core GPU observation' 'API4 package requires observer, GPU queries and D3D12'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected API4 prerequisites change no bytes'
        }
        foreach ($mode in @('Diagnostic','Online')) {
            $arguments = $selection.Clone();$arguments.Mode=$mode
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $arguments } 'separate opt-in Smoke mode' 'API4 classification refuses non-Smoke modes'
            $arguments.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $arguments } 'separate opt-in Smoke mode' 'API4 package refuses non-Smoke modes'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected API4 mode changes no bytes'
        }
        foreach ($mode in @('Diagnostic','Smoke','Online','Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' $selection
            Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;') "Ordinary $mode explicitly clears prior API4 recognition"
        }
    }
    Invoke-Case 'explicit-frame-boundary-observation-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = false;') 'API5 is disabled in ordinary Smoke'
        $selection = @{Mode='Smoke';MtaCoreRenderReference=$true;MtaCoreRenderRoute='D3D12';MtaCoreRenderGpuReference=$true;MtaCoreRenderCustomPresentReference=$true;MtaCoreRenderFrameBoundaryReference=$true;GameDirectory=$fixture.game}
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' ($selection + @{WhatIf=$true})
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'API5 WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' $selection
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = true;') 'API5 requires separate explicit admission'
        Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
        foreach ($field in @('MtaCoreRenderReference','MtaCoreRenderGpuReference','MtaCoreRenderCustomPresentReference')) {
            $invalid = $selection.Clone();$invalid[$field]=$false
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'Frame-boundary observation requires explicit' 'API5 refuses each missing prerequisite'
            $invalid.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $invalid } 'Frame-boundary observation requires explicit' 'API5 package refuses each missing prerequisite'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected API5 admission changes no bytes'
        }
        $invalid=$selection.Clone();$invalid.MtaCoreRenderRoute='D3D11'
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'Frame-boundary observation requires explicit' 'API5 refuses D3D11'
        foreach ($mode in @('Diagnostic','Online')) {
            $invalid=$selection.Clone();$invalid.Mode=$mode
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'separate opt-in Smoke mode' 'API5 refuses non-Smoke modes'
        }
        foreach ($mode in @('Diagnostic','Smoke','Online','Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' $selection
            Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = false;') "Ordinary $mode clears API5 observation"
        }
        # An intact historical API4 install remains usable, but its missing
        # marker cannot silently authorize this newer endpoint/lifecycle lease.
        Write-FixtureBytes $config ($utf8.GetBytes([regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = (true|false);', '')))
        $manifestPath=Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $legacy=$selection.Clone();$legacy.Remove('MtaCoreRenderFrameBoundaryReference')
        Invoke-Production $fixture 'set-mode.ps1' $legacy
        $before=Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $selection } 'Update the adapter before selecting frame-boundary observation' 'Historical API4 cannot infer API5 admission'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Historical API5 refusal changes no bytes'
    }
    Invoke-Case 'explicit-native-dispatch-observation-opt-in' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = false;') 'API6 is disabled in ordinary Smoke'
        $selection = @{Mode='Smoke';MtaCoreRenderReference=$true;MtaCoreRenderRoute='D3D12';MtaCoreRenderGpuReference=$true;MtaCoreRenderCustomPresentReference=$true;MtaCoreRenderFrameBoundaryReference=$true;MtaCoreRenderNativeDispatchReference=$true;GameDirectory=$fixture.game}
        $before = Get-TreeFingerprint $fixture.root
        Invoke-Production $fixture 'set-mode.ps1' ($selection + @{WhatIf=$true})
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'API6 WhatIf changes no bytes'
        Invoke-Production $fixture 'set-mode.ps1' $selection
        Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = true;') 'API6 requires separate explicit admission'
        Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
        foreach ($field in @('MtaCoreRenderReference','MtaCoreRenderGpuReference','MtaCoreRenderCustomPresentReference','MtaCoreRenderFrameBoundaryReference')) {
            $invalid = $selection.Clone();$invalid[$field]=$false
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'Native-dispatch observation requires explicit' 'API6 refuses each missing prerequisite'
            $invalid.Remove('GameDirectory')
            Assert-Refused { Invoke-Production $fixture 'package.ps1' $invalid } 'Native-dispatch observation requires explicit' 'API6 package refuses each missing prerequisite'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected API6 admission changes no bytes'
        }
        $invalid=$selection.Clone();$invalid.MtaCoreRenderRoute='D3D11'
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'Native-dispatch observation requires explicit' 'API6 refuses D3D11'
        foreach ($mode in @('Diagnostic','Online')) {
            $invalid=$selection.Clone();$invalid.Mode=$mode
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $invalid } 'separate opt-in Smoke mode' 'API6 refuses non-Smoke modes'
        }
        foreach ($mode in @('Diagnostic','Smoke','Online','Smoke')) {
            Invoke-Production $fixture 'set-mode.ps1' $selection
            Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = false;') "Ordinary $mode clears API6 observation"
        }
        # An intact historical API5 install remains usable, but its missing
        # marker cannot silently authorize this newer endpoint/lifecycle lease.
        Write-FixtureBytes $config ($utf8.GetBytes([regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = (true|false);', '')))
        $manifestPath=Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $legacy=$selection.Clone();$legacy.Remove('MtaCoreRenderNativeDispatchReference')
        Invoke-Production $fixture 'set-mode.ps1' $legacy
        $before=Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $selection } 'Update the adapter before selecting native-dispatch observation' 'Historical API5 cannot infer API6 admission'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Historical API6 refusal changes no bytes'
    }
    Invoke-Case 'historical-api3-cannot-infer-custom-present-classification' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Write-FixtureBytes $config ($utf8.GetBytes([regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = (true|false);', '')))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $selection = @{Mode='Smoke';MtaCoreRenderReference=$true;MtaCoreRenderRoute='D3D12';MtaCoreRenderGpuReference=$true;GameDirectory=$fixture.game}
        Invoke-Production $fixture 'set-mode.ps1' $selection
        Assert-Check ([IO.File]::ReadAllText($config) -notmatch 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE') 'Historical API3 remains usable without guessing classification admission'
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' ($selection + @{MtaCoreRenderCustomPresentReference=$true}) } 'Update the adapter before selecting custom-present recognition' 'Historical API3 cannot enable classification without its marker'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Historical classification refusal changes no bytes'
        $text = [IO.File]::ReadAllText($config) + "`nexport const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;`nexport const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;`n"
        Write-FixtureBytes $config ($utf8.GetBytes($text))
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';GameDirectory=$fixture.game} } 'Update the adapter before selecting custom-present recognition' 'Duplicate classification markers are ambiguous even for ordinary mode switches'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Duplicate-marker refusal changes no bytes'
    }
    Invoke-Case 'historical-api2-cannot-infer-gpu-query-admission' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Write-FixtureBytes $config ($utf8.GetBytes([regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_RENDER_GPU_REFERENCE = (true|false);', '')))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; GameDirectory=$fixture.game }
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; MtaCoreRenderGpuReference=$true; GameDirectory=$fixture.game } } 'Update the adapter before selecting GPU ownership queries' 'Historical API2 cannot infer COM-query opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Historical COM-query refusal changes no bytes'
    }
    Invoke-Case 'historical-render-api1-cannot-infer-api2-route' {
        param($fixture)
        Install-Fresh $fixture
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        $text = [regex]::Replace([IO.File]::ReadAllText($config), "export const MTA_CORE_RENDER_ROUTE = '(d3d11|d3d12)';", '')
        Write-FixtureBytes $config ($utf8.GetBytes($text))
        $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
        Write-FixtureJson $manifestPath $manifest
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; GameDirectory=$fixture.game }
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; MtaCoreRenderRoute='D3D12'; GameDirectory=$fixture.game } } 'Update the adapter before selecting an explicit' 'Historical API1 configuration cannot guess route'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Historical route refusal changes no bytes'
    }
    Invoke-Case 'legacy23-render-observation-needs-explicit-marker' {
        param($fixture)
        Install-Legacy $fixture 23
        $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
        Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; GameDirectory=$fixture.game }
        Assert-Check ([IO.File]::ReadAllText($config) -notmatch 'MTA_CORE_RENDER_REFERENCE') 'Historical configuration remains usable without render marker'
        $before = Get-TreeFingerprint $fixture.root
        Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{ Mode='Smoke'; MtaCoreRenderReference=$true; GameDirectory=$fixture.game } } 'Update the adapter before selecting MTA Core renderer observation' 'Historical config cannot infer render opt-in'
        Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing render marker changes no bytes'
    }
    if ($guiPackage) {
        Invoke-Case 'gui23-36-23-transaction' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $true
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            Assert-GuiInstalled $fixture
            $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
            foreach ($mode in @('Smoke','Diagnostic','Online','Smoke')) {
                Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;GameDirectory=$fixture.game}
                Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_STARTUP_REFERENCE = false;') "Ordinary $mode clears GUI opt-in"
            }
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_STARTUP_REFERENCE = true;') 'GUI complete group can be explicitly rearmed'
            $user = Join-Path $fixture.game 'CLEO/de-online/mta/cgui/user.txt'
            Write-FixtureBytes $user ($utf8.GetBytes('Retained GUI user bytes'))
            $userHash = Get-Sha $user
            Select-FixturePackage $fixture $false
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            Assert-InstalledPackage $fixture
            foreach ($path in $guiPaths) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $path))) ('Downgrade removes only GUI owned ' + $path) }
            Assert-Check ((Get-Sha $user) -eq $userHash) 'GUI downgrade preserves user bytes'
            $backupManifests = @(Get-ChildItem -LiteralPath (Join-Path $fixture.repository 'Build/definitive-update-backups') -Filter manifest.json -Recurse)
            Assert-Check ($backupManifests.Count -eq 2) 'Both package transitions retain prior manifests'
            $guiBackup = @($backupManifests | Where-Object { @((Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json).files).Count -eq 36 })[0]
            $originalHashes = @(Get-ChildItem -LiteralPath $guiBackup.DirectoryName -Filter '*.original' | ForEach-Object { Get-Sha $_.FullName })
            foreach ($path in $guiPaths) { Assert-Check (@($guiManifest.files | Where-Object path -eq $path)[0].sha256 -in $originalHashes) ('Downgrade preserves backup ' + $path) }
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game}
            Assert-Check ((Get-Sha $user) -eq $userHash) 'Ordinary uninstall preserves later GUI user bytes'
        }
        Invoke-Case 'gui-fresh36-validate-uninstall' {
            param($fixture)
            Select-FixturePackage $fixture $true
            Install-Fresh $fixture
            Assert-GuiInstalled $fixture
            $user = Join-Path $fixture.game 'CLEO/de-online/skins/Default/user.xml'
            Write-FixtureBytes $user ($utf8.GetBytes('Retained custom skin'))
            $hash = Get-Sha $user
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game}
            foreach ($path in ($expectedPaths + $guiPaths)) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $path))) ('GUI uninstall removes owned ' + $path) }
            Assert-Check ((Get-Sha $user) -eq $hash) 'GUI uninstall preserves custom skin'
        }
        foreach ($missing in $guiPaths) {
            Invoke-Case ('gui-partial-package-' + [IO.Path]::GetFileName($missing)) {
                param($fixture)
                Install-Fresh $fixture
                Select-FixturePackage $fixture $true
                $manifestPath = Join-Path $fixture.package 'manifest.json'
                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                $manifest.files = @($manifest.files | Where-Object path -ne $missing)
                Write-FixtureJson $manifestPath $manifest
                Remove-Item -LiteralPath (Assert-FixturePath (Join-Path $fixture.package ('Win64/' + $missing)))
                $fresh = New-GameDirectory (Join-Path $fixture.root 'fresh')
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Incomplete MTA Core GUI startup' 'Partial GUI update'
                Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'Incomplete MTA Core GUI startup' 'Partial GUI install'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Partial GUI package changes no bytes'
            }
            Invoke-Case ('gui-foreign-addition-' + [IO.Path]::GetFileName($missing)) {
                param($fixture)
                Install-Fresh $fixture
                Select-FixturePackage $fixture $true
                Write-FixtureBytes (Join-Path $fixture.game $missing) ($utf8.GetBytes('Foreign asset'))
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'New owned reference path already exists' 'GUI foreign addition'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'GUI foreign asset is never overwritten'
            }
            Invoke-Case ('gui-edited-installed-' + [IO.Path]::GetFileName($missing)) {
                param($fixture)
                Select-FixturePackage $fixture $true
                Install-Fresh $fixture
                Write-FixtureBytes (Join-Path $fixture.game $missing) ($utf8.GetBytes('Edited owned asset'))
                Select-FixturePackage $fixture $false
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Installed file was modified' 'GUI edited downgrade'
                Assert-Refused { Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game} } 'Installed file was modified' 'GUI edited uninstall'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Edited GUI assets block all removal'
            }
        }
        Invoke-Case 'gui-selection-conflicts-and-missing-assets' {
            param($fixture)
            Install-Fresh $fixture
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game} } 'complete owned GUI assets are missing' 'GUI cannot infer missing group'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing GUI group cannot mutate config'
            foreach ($flag in @('NativeGaitReference','MatchedGaitRate','LockWalkRoot','NativeInputReference','MtaNativePadReference','MtaNativeMovementTrial','MtaCoreStartupReference','MtaCoreInputReference','MtaCoreMovementReference','MtaCoreRenderReference','MtaCoreRenderGpuReference','MtaCoreRenderCustomPresentReference','MtaCoreRenderFrameBoundaryReference','MtaCoreRenderNativeDispatchReference','MtaCoreGuiSurfaceReference')) {
                $selection = @{Mode='Smoke';MtaCoreGuiStartupReference=$true}; $selection[$flag]=$true
                Assert-Refused { Invoke-Production $fixture 'package.ps1' $selection } 'GUI startup requires separate opt-in Smoke mode' ('GUI package rejects ' + $flag)
                $selection.GameDirectory=$fixture.game
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $selection } 'GUI startup requires separate opt-in Smoke mode' ('GUI mode rejects ' + $flag)
            }
            foreach ($mode in @('Diagnostic','Online')) {
                Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode=$mode;MtaCoreGuiStartupReference=$true} } 'separate opt-in Smoke mode' 'GUI package mode conflict'
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game} } 'separate opt-in Smoke mode' 'GUI selected mode conflict'
            }
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'GUI conflicts change no bytes or backups'
        }
        Invoke-Case 'gui-partial-installed-and-missing-marker' {
            param($fixture)
            Select-FixturePackage $fixture $true
            Install-Fresh $fixture
            $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
            Write-FixtureBytes $config ($utf8.GetBytes([regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_GUI_STARTUP_REFERENCE = (true|false);', '')))
            $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
            Write-FixtureJson $manifestPath $manifest
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game} } 'Update the adapter before selecting MTA Core GUI startup' 'GUI missing marker'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Missing GUI marker changes no bytes'
            $manifest.files = @($manifest.files | Where-Object path -ne $guiPaths[0])
            Write-FixtureJson $manifestPath $manifest
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game} } 'Incomplete MTA Core GUI startup' 'GUI partial installed manifest'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Partial installed GUI group cannot be removed'
        }
        Invoke-Case 'gui-forged-conflicting-package' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $true
            $config = Join-Path $fixture.package 'Win64/CLEO/de-online/config.mjs'
            Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config).Replace('export const MTA_CORE_RENDER_REFERENCE = false;', 'export const MTA_CORE_RENDER_REFERENCE = true;')))
            $manifestPath = Join-Path $fixture.package 'manifest.json'
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
            Write-FixtureJson $manifestPath $manifest
            $fresh=New-GameDirectory (Join-Path $fixture.root 'fresh')
            $before=Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'separate opt-in Smoke mode' 'Forged GUI update flags'
            Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'separate opt-in Smoke mode' 'Forged GUI install flags'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Forged GUI flag conflicts stage no bytes'
        }
        Invoke-Case 'gui-edited-package-asset' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $true
            Write-FixtureBytes (Join-Path $fixture.package ('Win64/' + $guiPaths[-1])) ($utf8.GetBytes('Corrupted package asset'))
            $fresh=New-GameDirectory (Join-Path $fixture.root 'fresh')
            $before=Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Package hash mismatch' 'Edited GUI source update'
            Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'Package content hash mismatch' 'Edited GUI source install'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Edited GUI package stages no files'
        }
        Invoke-Case 'gui-downgrade-failure-restores-owned-group' {
            param($fixture)
            Select-FixturePackage $fixture $true
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false
            $before=Get-TreeFingerprint $fixture.game
            # A real exclusive replacement refusal occurs after the group has
            # been removed. The production rollback must restore all 13 bytes
            # and preserve the original36 manifest before this fixture unlocks.
            $locked=[IO.File]::Open((Join-Path $fixture.game 'CLEO/de-online/config.mjs'), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
            try {
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'being used|access|denied|process' 'GUI downgrade replacement is locked'
            } finally { $locked.Dispose() }
            Assert-Check ((Get-TreeFingerprint $fixture.game) -eq $before) 'Failed GUI downgrade restores every owned byte and manifest'
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
        }
    }
    if ($surfacePackage) {
        Invoke-Case 'surface23-36-23-and-mode-isolation' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false $true
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            Assert-GuiInstalled $fixture $true
            $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
            foreach ($mode in @('Smoke','Diagnostic','Online','Smoke')) {
                Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;GameDirectory=$fixture.game}
                Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_SURFACE_REFERENCE = false;' -and [IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_STARTUP_REFERENCE = false;') "Ordinary $mode clears both independent GUI opt-ins"
            }
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game}
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiSurfaceReference=$true;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_SURFACE_REFERENCE = true;' -and [IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_STARTUP_REFERENCE = false;') 'Surface selection clears prior offscreen startup'
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiStartupReference=$true;GameDirectory=$fixture.game}
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_SURFACE_REFERENCE = false;') 'Offscreen startup clears prior surface selection'
            $user = Join-Path $fixture.game 'CLEO/de-online/mta/cgui/surface-user.txt'
            Write-FixtureBytes $user ($utf8.GetBytes('Retained surface user bytes'))
            $userHash = Get-Sha $user
            Select-FixturePackage $fixture $false
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            Assert-InstalledPackage $fixture
            foreach ($path in $guiPaths) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $path))) ('Surface downgrade removes exact owned ' + $path) }
            Assert-Check ((Get-Sha $user) -eq $userHash) 'Surface downgrade preserves foreign GUI bytes'
        }
        Invoke-Case 'surface-selection-conflicts-and-missing-group' {
            param($fixture)
            Install-Fresh $fixture
            $before = Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiSurfaceReference=$true;GameDirectory=$fixture.game} } 'complete owned GUI assets are missing' 'Surface cannot infer missing13 group'
            foreach ($flag in @('NativeGaitReference','MatchedGaitRate','LockWalkRoot','NativeInputReference','MtaNativePadReference','MtaNativeMovementTrial','MtaCoreStartupReference','MtaCoreInputReference','MtaCoreMovementReference','MtaCoreRenderReference','MtaCoreRenderGpuReference','MtaCoreRenderCustomPresentReference','MtaCoreRenderFrameBoundaryReference','MtaCoreRenderNativeDispatchReference','MtaCoreGuiStartupReference')) {
                $selection = @{Mode='Smoke';MtaCoreGuiSurfaceReference=$true}; $selection[$flag]=$true
                Assert-Refused { Invoke-Production $fixture 'package.ps1' $selection } 'separate opt-in Smoke mode' ('Surface package rejects ' + $flag)
                $selection.GameDirectory=$fixture.game
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $selection } 'separate opt-in Smoke mode' ('Surface mode rejects ' + $flag)
            }
            foreach ($mode in @('Diagnostic','Online')) {
                Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode=$mode;MtaCoreGuiSurfaceReference=$true} } 'separate opt-in Smoke mode' 'Surface package mode conflict'
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode=$mode;MtaCoreGuiSurfaceReference=$true;GameDirectory=$fixture.game} } 'separate opt-in Smoke mode' 'Surface selected mode conflict'
            }
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode='Smoke';MtaCoreGuiSurfaceReference=$true;MtaCoreRenderRoute='D3D12'} } 'separate opt-in Smoke mode' 'Surface cannot inherit explicit route'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Surface conflicts preserve all bytes and backups'
        }
        foreach ($markerKind in @('missing','duplicate')) {
            Invoke-Case ('surface-' + $markerKind + '-marker') {
                param($fixture)
                Select-FixturePackage $fixture $false $true
                Install-Fresh $fixture
                $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
                $text = [regex]::Replace([IO.File]::ReadAllText($config), 'export const MTA_CORE_GUI_SURFACE_REFERENCE = (true|false);', '')
                if ($markerKind -eq 'duplicate') { $text += "`nexport const MTA_CORE_GUI_SURFACE_REFERENCE = false;`nexport const MTA_CORE_GUI_SURFACE_REFERENCE = false;`n" }
                Write-FixtureBytes $config ($utf8.GetBytes($text))
                $manifestPath = Join-Path $fixture.game 'de-online-install-manifest.json'
                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
                Write-FixtureJson $manifestPath $manifest
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiSurfaceReference=$true;GameDirectory=$fixture.game} } 'Update the adapter before selecting MTA Core GUI surface' 'Surface requires one explicit marker'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid surface marker changes no bytes'
            }
        }
        foreach ($badFlag in @('MTA_CORE_GUI_STARTUP_REFERENCE','MTA_CORE_RENDER_REFERENCE')) {
            Invoke-Case ('surface-forged-conflict-' + $badFlag) {
                param($fixture)
                Install-Fresh $fixture
                Select-FixturePackage $fixture $false $true
                $config = Join-Path $fixture.package 'Win64/CLEO/de-online/config.mjs'
                Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config).Replace("export const $badFlag = false;", "export const $badFlag = true;")))
                $manifestPath = Join-Path $fixture.package 'manifest.json'
                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
                Write-FixtureJson $manifestPath $manifest
                $fresh=New-GameDirectory (Join-Path $fixture.root 'fresh')
                $before=Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'separate opt-in Smoke mode' 'Forged surface update flags'
                Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'separate opt-in Smoke mode' 'Forged surface install flags'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Forged surface conflicts stage no bytes'
            }
        }
        Invoke-Case 'surface-duplicate-package-marker' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false $true
            $config = Join-Path $fixture.package 'Win64/CLEO/de-online/config.mjs'
            Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config) + "`nexport const MTA_CORE_GUI_SURFACE_REFERENCE = false;`n"))
            $manifestPath = Join-Path $fixture.package 'manifest.json'
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
            Write-FixtureJson $manifestPath $manifest
            $fresh=New-GameDirectory (Join-Path $fixture.root 'fresh')
            $before=Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Invalid GUI surface configuration marker' 'Duplicate surface package update'
            Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'Invalid GUI surface configuration marker' 'Duplicate surface package install'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Duplicate surface package marker stages no bytes'
        }
        Invoke-Case 'surface-downgrade-failure-restores36' {
            param($fixture)
            Select-FixturePackage $fixture $false $true
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false
            $before=Get-TreeFingerprint $fixture.game
            $locked=[IO.File]::Open((Join-Path $fixture.game 'CLEO/de-online/config.mjs'), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
            try { Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'being used|access|denied|process' 'Surface downgrade real replacement refusal' }
            finally { $locked.Dispose() }
            Assert-Check ((Get-TreeFingerprint $fixture.game) -eq $before) 'Failed surface downgrade restores all36 bytes and original manifest'
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
        }
    }
    }
    if ($frontendPackage) {
        Invoke-Case 'frontend23-58-36-58-23-transactions' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false $false $true
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game;PackageDirectory=$fixture.package}
            $installed = Get-Content -LiteralPath (Join-Path $fixture.game 'de-online-install-manifest.json') -Raw | ConvertFrom-Json
            Assert-Check (@($installed.files).Count -eq 58 -and @(Compare-Object ($expectedPaths + $guiPaths + $frontendPaths) @($installed.files.path)).Count -eq 0) 'Frontend exact complete58 ownership'
            foreach ($entry in $installed.files) { Assert-Check ((Get-Sha $entry.destination) -eq $entry.sha256) ('Frontend owned hash ' + $entry.path) }
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiFrontendReference=$true;GameDirectory=$fixture.game}
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Diagnostic';GameDirectory=$fixture.game}
            $config = Join-Path $fixture.game 'CLEO/de-online/config.mjs'
            Assert-Check ([IO.File]::ReadAllText($config) -match 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = false;') 'Ordinary mode clears frontend opt-in while owning58'
            Invoke-Production $fixture 'set-mode.ps1' @{Mode='Smoke';MtaCoreGuiFrontendReference=$true;GameDirectory=$fixture.game}
            if ($guiPackage) {
                Select-FixturePackage $fixture $true
                Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
                Assert-GuiInstalled $fixture
                foreach ($path in $frontendPaths) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $path))) ('Frontend downgrade removes only owned menu asset ' + $path) }
                Select-FixturePackage $fixture $false $false $true
                Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            }
            Select-FixturePackage $fixture $false
            Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game}
            Assert-InstalledPackage $fixture
            foreach ($path in ($guiPaths + $frontendPaths)) { Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture.game $path))) ('Ordinary downgrade removes owned GUI/menu asset ' + $path) }
        }
        Invoke-Case 'frontend-fresh58-validate-uninstall' {
            param($fixture)
            Select-FixturePackage $fixture $false $false $true
            Install-Fresh $fixture
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
            Invoke-Production $fixture 'set-loader.ps1' @{Action='Disable';GameDirectory=$fixture.game}
            Assert-Check ((Test-Path -LiteralPath (Join-Path $fixture.game 'version.dll.de-online-disabled')) -and -not (Test-Path -LiteralPath (Join-Path $fixture.game 'version.dll'))) 'Frontend58 loader bypass renames only owned loader'
            Invoke-Production $fixture 'set-loader.ps1' @{Action='Status';GameDirectory=$fixture.game}
            Invoke-Production $fixture 'set-loader.ps1' @{Action='Enable';GameDirectory=$fixture.game}
            Assert-Check ((Test-Path -LiteralPath (Join-Path $fixture.game 'version.dll')) -and -not (Test-Path -LiteralPath (Join-Path $fixture.game 'version.dll.de-online-disabled'))) 'Frontend58 loader recovery restores owned loader'
            $installedPath=Join-Path $fixture.game 'de-online-install-manifest.json'
            $originalInstalled=[IO.File]::ReadAllBytes($installedPath)
            foreach ($guard in @('foreign-owner','escape-destination','edited-owned-hash','partial-menu-group')) {
                $record=$utf8.GetString($originalInstalled)|ConvertFrom-Json
                switch ($guard) {
                    'foreign-owner' { $record.gameDirectory=Join-Path $fixture.root 'other-owner' }
                    'escape-destination' { $record.files[0].destination=Join-Path $fixture.root 'outside-owned-game' }
                    'edited-owned-hash' { $record.files[0].sha256=('0'*64) }
                    'partial-menu-group' { $record.files=@($record.files|Where-Object path -ne $frontendPaths[0]) }
                }
                Write-FixtureJson $installedPath $record
                $before=Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true} } 'identity|invalid destination|modified|Incomplete MTA Core GUI frontend' ('Frontend installed ownership guard ' + $guard)
                Assert-Refused { Invoke-Production $fixture 'set-loader.ps1' @{Action='Status';GameDirectory=$fixture.game} } 'identity|invalid destination|modified|Incomplete MTA Core GUI frontend' ('Frontend loader ownership guard ' + $guard)
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Installed ownership rejection preserves every byte'
            }
            Write-FixtureBytes $installedPath $originalInstalled
            $sourceManifestPath=Join-Path $fixture.package 'manifest.json'
            $originalPackage=[IO.File]::ReadAllBytes($sourceManifestPath)
            $bad=$utf8.GetString($originalPackage)|ConvertFrom-Json
            @($bad.files|Where-Object path -eq 'CLEO/de-online/mta/locale/en_US/client.pot')[0].sha256=('0'*64)
            Write-FixtureJson $sourceManifestPath $bad
            $before=Get-TreeFingerprint $fixture.root
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Package hash mismatch' 'Frontend source hash cannot be forged'
            Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Source hash rejection preserves all58 installed bytes'
            Write-FixtureBytes $sourceManifestPath $originalPackage
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game}
            Assert-Check (@(Get-ChildItem -LiteralPath $fixture.game -File -Recurse).Count -eq 1) 'Frontend uninstall retains only exact hashed game executable'
        }
        Invoke-Case 'frontend-each-menu-asset-required-as-one-group' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false $false $true
            $manifestPath = Join-Path $fixture.package 'manifest.json'
            $original = [IO.File]::ReadAllBytes($manifestPath)
            $fresh = $fixture.game # Validation fails before the existing-install collision gate.
            foreach ($path in $frontendPaths) {
                $manifest = $utf8.GetString($original) | ConvertFrom-Json
                $manifest.files = @($manifest.files | Where-Object path -ne $path)
                Write-FixtureJson $manifestPath $manifest
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Incomplete MTA Core GUI frontend package ownership' ('Frontend update missing group member ' + $path)
                Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'Incomplete MTA Core GUI frontend package ownership' ('Frontend install missing group member ' + $path)
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Rejected incomplete frontend stages no bytes'
            }
            Write-FixtureBytes $manifestPath $original
        }
        Invoke-Case 'frontend-requires-gui-group-and-marker' {
            param($fixture)
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false $false $true
            $manifestPath = Join-Path $fixture.package 'manifest.json'
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $completeManifest = [IO.File]::ReadAllBytes($manifestPath)
            $manifest.files = @($manifest.files | Where-Object { $_.path -notin $guiPaths })
            Write-FixtureJson $manifestPath $manifest
            Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Incomplete MTA Core GUI frontend package ownership' 'Frontend cannot omit complete13 GUI group'
            Write-FixtureBytes $manifestPath $completeManifest
            $config = Join-Path $fixture.package 'Win64/CLEO/de-online/config.mjs'
            $configBytes = [IO.File]::ReadAllBytes($config)
            foreach ($markerCase in @('missing','duplicate')) {
                $text = $utf8.GetString($configBytes)
                if ($markerCase -eq 'missing') { $text = [regex]::Replace($text,'(?m)^export const MTA_CORE_GUI_FRONTEND_REFERENCE = true;\r?\n','') }
                else { $text += "`nexport const MTA_CORE_GUI_FRONTEND_REFERENCE = false;`n" }
                Write-FixtureBytes $config ($utf8.GetBytes($text))
                $modified = $utf8.GetString($completeManifest) | ConvertFrom-Json
                @($modified.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256 = Get-Sha $config
                Write-FixtureJson $manifestPath $modified
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'Invalid GUI frontend configuration marker' ('Frontend package requires exactly one marker ' + $markerCase)
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Frontend marker refusal stages no bytes'
            }
            Write-FixtureBytes $config $configBytes
            Write-FixtureBytes $manifestPath $completeManifest
            foreach ($flag in @('MtaCoreGuiStartupReference','MtaCoreGuiSurfaceReference','MtaCoreRenderReference','MtaCoreMovementReference','MtaCoreInputReference','NativeInputReference')) {
                $selection = @{Mode='Smoke';MtaCoreGuiFrontendReference=$true;GameDirectory=$fixture.game};$selection[$flag]=$true
                Assert-Refused { Invoke-Production $fixture 'set-mode.ps1' $selection } 'separate opt-in Smoke mode' ('Frontend selected mode conflict ' + $flag)
            }
            foreach ($mode in @('Diagnostic','Online')) {
                Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode=$mode;MtaCoreGuiFrontendReference=$true} } 'separate opt-in Smoke mode' 'Frontend package mode conflict'
            }
            Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode='Smoke';MtaCoreGuiFrontendReference=$true;MtaCoreRenderRoute='D3D12'} } 'separate opt-in Smoke mode' 'Frontend does not inherit explicit renderer route'
            foreach ($outputPath in @((Join-Path $fixture.root 'outside-workspace-build'),(Join-Path $fixture.repository 'Build'))) {
                $before=Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'package.ps1' @{Mode='Smoke';OutputDirectory=$outputPath} } 'Package output must stay within' 'Candidate output cannot escape or replace workspace Build root'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Invalid output directory stages no writes'
            }
        }
        foreach ($badFlag in @('MTA_CORE_GUI_STARTUP_REFERENCE','MTA_CORE_GUI_SURFACE_REFERENCE','MTA_CORE_RENDER_REFERENCE')) {
            Invoke-Case ('frontend-forged-conflict-' + $badFlag) {
                param($fixture)
                Install-Fresh $fixture
                Select-FixturePackage $fixture $false $false $true
                $config = Join-Path $fixture.package 'Win64/CLEO/de-online/config.mjs'
                Write-FixtureBytes $config ($utf8.GetBytes([IO.File]::ReadAllText($config).Replace("export const $badFlag = false;", "export const $badFlag = true;")))
                $manifestPath = Join-Path $fixture.package 'manifest.json'
                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                @($manifest.files | Where-Object path -eq 'CLEO/de-online/config.mjs')[0].sha256=Get-Sha $config
                Write-FixtureJson $manifestPath $manifest
                $fresh = $fixture.game # Validation fails before the existing-install collision gate.
                $before = Get-TreeFingerprint $fixture.root
                Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'separate opt-in Smoke mode' 'Forged frontend update flags'
                Assert-Refused { Invoke-Production $fixture 'install.ps1' @{Mode='Smoke';PackageDirectory=$fixture.package;GameDirectory=$fresh} } 'separate opt-in Smoke mode' 'Forged frontend install flags'
                Assert-Check ((Get-TreeFingerprint $fixture.root) -eq $before) 'Frontend forged conflicts stage no bytes'
            }
        }
        Invoke-Case 'frontend-downgrade-failure-restores58' {
            param($fixture)
            Select-FixturePackage $fixture $false $false $true
            Install-Fresh $fixture
            Select-FixturePackage $fixture $false
            $before = Get-TreeFingerprint $fixture.game
            $locked = [IO.File]::Open((Join-Path $fixture.game 'CLEO/de-online/config.mjs'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
            try { Assert-Refused { Invoke-Production $fixture 'update.ps1' @{GameDirectory=$fixture.game} } 'being used|access|denied|process' 'Frontend downgrade actual replacement refusal' }
            finally { $locked.Dispose() }
            Assert-Check ((Get-TreeFingerprint $fixture.game) -eq $before) 'Failed frontend downgrade restores58 bytes and original manifest'
            Invoke-Production $fixture 'uninstall.ps1' @{GameDirectory=$fixture.game;ValidateOnly=$true}
        }
    }
} catch { $failure = $_.Exception.Message }
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) { if ((Get-Sha (Join-Path $repositoryRoot $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
foreach ($entry in $packageHashes) { if ((Get-Sha (Join-Path $sourcePackage $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
foreach ($entry in $guiPackageHashes) { if ((Get-Sha (Join-Path $guiPackage $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
foreach ($entry in $frontendPackageHashes) { if ((Get-Sha (Join-Path $frontendPackage $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
foreach ($entry in $surfacePackageHashes) { if ((Get-Sha (Join-Path $surfacePackage $entry.path)) -ne $entry.sha256) { $sourceUnchanged = $false } }
$sourceExecutableUnchanged = (Get-Sha $sourceExecutable) -eq $knownGameHash
$record = [ordered]@{
    recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot; success=($null -eq $failure -and $sourceUnchanged -and $sourceExecutableUnchanged);
    checks=$script:checks.Count; cases=$script:cases; checkLabels=$script:checks; failure=$failure;
    sourceHashes=$sourceHashes; packageRoot=$sourcePackage; packageHashes=$packageHashes; sourceUnchanged=$sourceUnchanged;
    guiPackageRoot=$guiPackage; guiPackageHashes=$guiPackageHashes; guiCasesEnabled=($null -ne $guiPackage);
    frontendOnly=$FrontendOnly.IsPresent; scope=$(if ($FrontendOnly) { 'Bounded frontend58 ownership plus legacy23/GUI36 compatibility and shared ownership/hash/destination guards' } else { 'All historical and optional GUI/frontend package ownership cases' }); frontendPackageRoot=$frontendPackage; frontendPackageHashes=$frontendPackageHashes; frontendCasesEnabled=($null -ne $frontendPackage);
    surfacePackageRoot=$surfacePackage; surfacePackageHashes=$surfacePackageHashes; surfaceCasesEnabled=($null -ne $surfacePackage);
    gameExecutableSource=$sourceExecutable; gameExecutableSha256=$knownGameHash; sourceExecutableUnchanged=$sourceExecutableUnchanged;
    actualGameInstallationChanged=$false; gameLaunched=$false; gameProcessAttached=$false; nativeInputWriteVerified=$false
}
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Movement package tests failed: $failure; see $outputRoot" }
$record | ConvertTo-Json -Depth 10 -Compress
