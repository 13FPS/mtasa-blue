#!/usr/bin/env pwsh
#requires -Version 7.0
# This file is part of Multi Theft Auto. See LICENSE in the repository root.
[CmdletBinding()]
param([ValidateSet('Diagnostic', 'Smoke', 'Online')][string]$Mode = 'Smoke',
    [switch]$NativeGaitReference, [switch]$MatchedGaitRate, [switch]$LockWalkRoot, [switch]$NativeInputReference,
    [switch]$MtaNativePadReference)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($MtaNativePadReference -and $Mode -ne 'Smoke') { throw 'MTA native pad reference requires Smoke mode.' }
if ($MtaNativePadReference -and ($NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA native pad reference conflicts with other input or gait trials.'
}
if ($NativeGaitReference -and $Mode -ne 'Smoke') { throw 'Native gait reference requires Smoke mode.' }
if ($MatchedGaitRate -and $Mode -ne 'Online') { throw 'Matched gait rate requires Online mode.' }
if ($LockWalkRoot -and $Mode -ne 'Online') { throw 'Walk-root locking requires Online mode.' }
if ($NativeInputReference -and $Mode -ne 'Smoke') { throw 'Native input reference requires Smoke mode.' }
if ($NativeInputReference -and $NativeGaitReference) { throw 'Native input reference conflicts with native gait reference.' }
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$buildRoot = Join-Path $repositoryRoot 'Build/definitive-package'
$cacheRoot = Join-Path $buildRoot 'downloads'
$packageRoot = Join-Path $buildRoot $Mode
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$apiCommit = '34bedee0be2f57047ed8f0705c81f137ede3d3ce'
$utf8 = [Text.UTF8Encoding]::new($false)

function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if (Test-Path -LiteralPath $candidate) {
            if ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing a linked path: $candidate"
            }
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}

function Get-VerifiedDownload([string]$Name, [string]$Url, [string]$Sha256) {
    $destination = Join-Path $cacheRoot $Name
    Assert-NoLinks $destination
    if (-not (Test-Path -LiteralPath $destination)) {
        $partial = "$destination.$([Guid]::NewGuid().ToString('N')).download"
        try {
            Invoke-WebRequest -Uri $Url -OutFile $partial
            if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Sha256) {
                throw "Downloaded dependency hash mismatch: $Name"
            }
            [IO.File]::Move($partial, $destination)
        } finally {
            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial }
        }
    }
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Sha256) {
        throw "Cached dependency hash mismatch: $destination"
    }
    return $destination
}

function Copy-ZipEntry([string]$ArchivePath, [string]$EntryName, [string]$Destination) {
    # Select individual entries instead of expanding arbitrary ZIP paths into
    # the package. This also excludes unrelated plugins from the game runtime.
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $entry = $archive.GetEntry($EntryName)
        if ($null -eq $entry) { throw "Missing ZIP entry: $EntryName" }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Destination)) | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $Destination, $false)
    } finally {
        $archive.Dispose()
    }
}

Assert-NoLinks $buildRoot
$sourceNames = @('index.js', 'controller.mjs', 'native-pad-reference.mjs', 'config.mjs', 'mod.json')
foreach ($name in $sourceNames) {
    $source = Join-Path $PSScriptRoot "game-adapter/$name"
    Assert-NoLinks $source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Adapter source is missing: $source" }
}
$bridgePath = Join-Path $repositoryRoot 'Build/definitive-online/de_client_bridge.dll'
Assert-NoLinks $bridgePath
if (-not (Test-Path -LiteralPath $bridgePath -PathType Leaf)) {
    throw 'Build the current native bridge with Client/definitive/build.ps1 before packaging.'
}
[IO.Directory]::CreateDirectory($cacheRoot) | Out-Null

# Immutable release URLs and independent hashes make a reviewed package
# reproducible without executing either third-party installer or downloaded DLL.
$dependencies = @(
    @{ name = 'cleo_redux_1.5.1.x64.zip'; url = 'https://github.com/cleolibrary/CLEO-Redux/releases/download/1.5.1/cleo_redux_1.5.1.x64.zip'; sha256 = '4205147c6b62a0393e4ed77f0e59cbbf52715601c5a1ec09c755b0a1320185dd' },
    @{ name = 'Ultimate-ASI-Loader-NoPDB_x64.zip'; url = 'https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip'; sha256 = 'e5860e7d9a1805267535b65749575b5e406cc6ea3325c7392189c578815045d1' },
    @{ name = 'CLEO_REDUX_LICENSE.txt'; url = 'https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/LICENSE.txt'; sha256 = 'baec1109f27ba9165e84f9fcd8000029a54fd108895c53cc4fff4ef76799c7c1' },
    @{ name = 'ULTIMATE_ASI_LOADER_LICENSE.txt'; url = 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/v9.7.4/license'; sha256 = 'ceca73c504e39084b0b80d2a437aea9988313d1915418d3c629c3e4a8313f5a1' },
    @{ name = 'sa_unreal.json'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/sa_unreal/sa_unreal.json"; sha256 = 'cab1b9d6bd25a4fff41fef63aef0fd771a923c0d39cb018f030f44eceb7713f7' },
    @{ name = 'unknown_x64.json'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/unknown_x64/unknown_x64.json"; sha256 = '5206809f78515bd0dca716f945f867056f4d5b0da2f9d079caaadf55c0855676' },
    @{ name = 'SANNY_BUILDER_LIBRARY_README.md'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/README.md"; sha256 = '359d7c71d42f14bc4277616d6058a6715d13ef3bbdd7567216448d45f4fd721d' }
)
$downloads = @{}
foreach ($dependency in $dependencies) {
    $downloads[$dependency.name] = Get-VerifiedDownload $dependency.name $dependency.url $dependency.sha256
}
$saDefinitions = Get-Content -LiteralPath $downloads['sa_unreal.json'] -Raw | ConvertFrom-Json
$sdkDefinitions = Get-Content -LiteralPath $downloads['unknown_x64.json'] -Raw | ConvertFrom-Json
if ($saDefinitions.meta.version -ne '0.500' -or $sdkDefinitions.meta.version -ne '0.239') {
    throw 'Pinned API definitions have unexpected version metadata.'
}

$stagingRoot = Join-Path $buildRoot ('.staging-' + [Guid]::NewGuid().ToString('N'))
$stagingWin64 = Join-Path $stagingRoot 'Win64'
[IO.Directory]::CreateDirectory($stagingWin64) | Out-Null
Copy-ZipEntry $downloads['Ultimate-ASI-Loader-NoPDB_x64.zip'] 'dinput8.dll' (Join-Path $stagingWin64 'version.dll')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'cleo_redux64.asi' (Join-Path $stagingWin64 'cleo_redux64.asi')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'CLEO/CLEO_PLUGINS/dylib64.cleo' (Join-Path $stagingWin64 'CLEO/CLEO_PLUGINS/dylib64.cleo')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'CLEO/CLEO_PLUGINS/Input64.cleo' (Join-Path $stagingWin64 'CLEO/CLEO_PLUGINS/Input64.cleo')
$adapterDirectory = Join-Path $stagingWin64 'CLEO/de-online'
$configDirectory = Join-Path $stagingWin64 'CLEO/.config'
$licenseDirectory = Join-Path $stagingWin64 'DE_ONLINE_LICENSES'
foreach ($directory in @($adapterDirectory, $configDirectory, $licenseDirectory)) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}
foreach ($name in $sourceNames) {
    [IO.File]::Copy((Join-Path $PSScriptRoot "game-adapter/$name"), (Join-Path $adapterDirectory $name), $false)
}
[IO.File]::Copy($bridgePath, (Join-Path $adapterDirectory 'de_client_bridge.dll'), $false)
$configPath = Join-Path $adapterDirectory 'config.mjs'
$configText = [IO.File]::ReadAllText($configPath)
$modeMarker = "export const MODE = 'smoke';"
if ([regex]::Matches($configText, [regex]::Escape($modeMarker)).Count -ne 1) {
    throw 'config.mjs must contain exactly one default smoke-mode assignment.'
}
# Mode changes only the staged copy. Diagnostic retains the runtime files but
# exits before bridge/game access; the default Smoke mode validates player reads.
$configText = $configText.Replace($modeMarker, "export const MODE = '$($Mode.ToLowerInvariant())';")
$referenceMarker = 'export const GAIT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($referenceMarker)).Count -ne 1) { throw 'Expected disabled reference in source configuration.' }
$configText = $configText.Replace($referenceMarker,
    "export const GAIT_REFERENCE = $($NativeGaitReference.IsPresent.ToString().ToLowerInvariant());")
$rateMarker = 'export const GAIT_RATE = false;'
if ([regex]::Matches($configText, [regex]::Escape($rateMarker)).Count -ne 1) { throw 'Expected disabled rate trial in source configuration.' }
$configText = $configText.Replace($rateMarker,
    "export const GAIT_RATE = $($MatchedGaitRate.IsPresent.ToString().ToLowerInvariant());")
$rootMarker = 'export const GAIT_ROOT_LOCK = false;'
if ([regex]::Matches($configText, [regex]::Escape($rootMarker)).Count -ne 1) { throw 'Expected disabled root experiment in source configuration.' }
$configText = $configText.Replace($rootMarker,
    "export const GAIT_ROOT_LOCK = $($LockWalkRoot.IsPresent.ToString().ToLowerInvariant());")
$inputMarker = 'export const INPUT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($inputMarker)).Count -ne 1) { throw 'Expected disabled input reference in source configuration.' }
$configText = $configText.Replace($inputMarker,
    "export const INPUT_REFERENCE = $($NativeInputReference.IsPresent.ToString().ToLowerInvariant());")
$padMarker = 'export const MTA_NATIVE_PAD_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($padMarker)).Count -ne 1) { throw 'Expected disabled MTA native pad reference in source configuration.' }
# Keep the native reader opt-in independent from the previous CLEO pad probe;
# an ordinary package must never inherit either experiment from a prior trial.
$configText = $configText.Replace($padMarker,
    "export const MTA_NATIVE_PAD_REFERENCE = $($MtaNativePadReference.IsPresent.ToString().ToLowerInvariant());")
[IO.File]::WriteAllText($configPath, $configText, $utf8)
foreach ($name in @('sa_unreal.json', 'unknown_x64.json')) {
    [IO.File]::Copy($downloads[$name], (Join-Path $configDirectory $name), $false)
}
$ini = @'
[General]
AllowCs=0
AllowJs=1
AllowFxt=0
LogOpcodes=0
CheckUpdates=0
PermissionLevel=Strict
[Host]
EnableSelfHost=0
[Permissions]
mem=1
dll=1
fs=0
net=0
'@
[IO.File]::WriteAllText((Join-Path $configDirectory 'cleo.ini'), $ini.Replace("`r`n", "`n") + "`n", $utf8)
foreach ($name in @('CLEO_REDUX_LICENSE.txt', 'ULTIMATE_ASI_LOADER_LICENSE.txt', 'SANNY_BUILDER_LIBRARY_README.md')) {
    [IO.File]::Copy($downloads[$name], (Join-Path $licenseDirectory $name), $false)
}
[IO.File]::Copy((Join-Path $repositoryRoot 'LICENSE'), (Join-Path $licenseDirectory 'MTA_GPLv3.txt'), $false)

$files = @(Get-ChildItem -LiteralPath $stagingWin64 -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($stagingWin64, $_.FullName).Replace('\', '/')
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
})
$manifest = [ordered]@{
    packageId = 'de-online-prototype'
    formatVersion = 1
    mode = $Mode
    gameSha256 = $knownGameHash
    gameFixedVersion = '1.0.113.21181'
    apiCommit = $apiCommit
    dependencies = $dependencies
    files = $files
}
[IO.File]::WriteAllText((Join-Path $stagingRoot 'manifest.json'), ($manifest | ConvertTo-Json -Depth 6) + "`n", $utf8)

if (Test-Path -LiteralPath $packageRoot) {
    # Replace only a previous, intact package we generated. Extra or edited
    # files require review instead of recursive deletion of somebody's work.
    Assert-NoLinks $packageRoot
    $oldManifestPath = Join-Path $packageRoot 'manifest.json'
    Assert-NoLinks $oldManifestPath
    $old = Get-Content -LiteralPath $oldManifestPath -Raw | ConvertFrom-Json
    if ($old.packageId -ne 'de-online-prototype' -or $old.formatVersion -ne 1) { throw 'Existing output is not our package.' }
    $oldPaths = @($old.files | ForEach-Object { 'Win64/' + $_.path }) + @('manifest.json')
    $actualPaths = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse | ForEach-Object {
        [IO.Path]::GetRelativePath($packageRoot, $_.FullName).Replace('\', '/')
    })
    if (@(Compare-Object $oldPaths $actualPaths).Count -ne 0) { throw 'Existing package contains unrecorded or missing files.' }
    foreach ($directory in @(Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse -Force)) {
        Assert-NoLinks $directory.FullName
    }
    foreach ($entry in $old.files) {
        $existing = [IO.Path]::GetFullPath((Join-Path $packageRoot ('Win64/' + $entry.path)))
        if (-not $existing.StartsWith($packageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Existing package manifest escapes its output directory.'
        }
        Assert-NoLinks $existing
        if ((Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            throw "Existing package was modified: $existing"
        }
    }
    foreach ($relative in $oldPaths) { Remove-Item -LiteralPath (Join-Path $packageRoot $relative) }
    foreach ($directory in @(Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending)) {
        Remove-Item -LiteralPath $directory.FullName
    }
    Remove-Item -LiteralPath $packageRoot
}
# Both resolved paths are descendants of the fixed ignored build directory;
# move the completed artifact only after all dependency/content checks pass.
Assert-NoLinks $stagingRoot
if (-not $stagingRoot.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    -not $packageRoot.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package move would escape the intended build directory.'
}
Move-Item -LiteralPath $stagingRoot -Destination $packageRoot
Write-Host "Prepared $Mode package: $packageRoot"
Write-Host "Verified $($files.Count) files. No game files were changed and no downloaded binary was run."
