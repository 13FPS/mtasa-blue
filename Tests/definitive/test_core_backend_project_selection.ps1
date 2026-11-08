#!/usr/bin/env pwsh
#requires -Version 7.0
# Validate snapshots made by the existing normal build coordinator. This script
# never regenerates a project, invokes a compiler, or loads an engine module.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SnapshotsDir,
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build = Join-Path $root 'Build'
if (-not $OutputPath) {
    $runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
    $OutputPath = Join-Path $build ('mta-core-backend-selection/' + $runId + '/test-result.json')
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if (-not $OutputPath.StartsWith($build + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Project-selection evidence must stay under workspace Build.'
}

function NoLinks([string]$Path) {
    $absolute = [IO.Path]::GetFullPath($Path)
    if ($absolute -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)') { throw 'Excluded path refused.' }
    for ($ancestor = $absolute; $ancestor; $ancestor = [IO.Path]::GetDirectoryName($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Linked path refused.'
        }
    }
}
function Sha([string]$Path) {
    NoLinks $Path
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Check([bool]$Condition, [string]$Message) {
    $script:checks++
    if (-not $Condition) { throw $Message }
}
function CoreSettingsGroup([xml]$Project, [string]$Configuration) {
    $condition = "'`$(Configuration)|`$(Platform)'=='" + $Configuration + "'"
    $nodes = @($Project.SelectNodes('//*[local-name()="ItemDefinitionGroup"]') | Where-Object { $_.GetAttribute('Condition') -eq $condition })
    if ($nodes.Count -ne 1) { throw ('Expected exactly one settings group: ' + $Configuration) }
    return $nodes[0]
}
function Definitions([Xml.XmlNode]$Group) {
    return $Group.SelectSingleNode('*[local-name()="ClCompile"]/*[local-name()="PreprocessorDefinitions"]').InnerText -split ';'
}
function ToolXml([Xml.XmlNode]$Group, [string]$Tool) {
    $node = $Group.SelectSingleNode('*[local-name()="' + $Tool + '"]')
    if (-not $node) { return '' }
    return $node.OuterXml
}
function Guid([xml]$Project) {
    return $Project.SelectSingleNode('/*[local-name()="Project"]/*[local-name()="PropertyGroup"]/*[local-name()="ProjectGuid"]').InnerText
}
function SolutionBuilds([string]$Solution, [string]$Guid, [string]$Configuration) {
    return $Solution -match ('(?m)^\s*' + [regex]::Escape($Guid + '.' + $Configuration + '.Build.0') + '\s*=')
}

$checks = 0
$snapshotHashes = [ordered]@{}
$sourceHashes = [ordered]@{}
$selections = [Collections.Generic.List[object]]::new()
$record = [ordered]@{
    schema='mta_core_backend_project_selection_v1';success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o')
    checks=0;snapshotHashes=$snapshotHashes;sourceHashes=$sourceHashes;inputsUnchanged=$false;selections=$selections
    projectReferences=@();solutionDependency=$null;failure=$null;projectsRegenerated=$false;compilerExecuted=$false
    gameLaunched=$false;coreLoaded=$false;engineModuleLoaded=$false;installationChanged=$false
    scope='Current actual generated Core/backend project and solution snapshots: classic Win32 metadata remains unchanged with DE opt-in, while only supported Release/x64 selects the DE startup macro.'
}
NoLinks $OutputPath
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
try {
    NoLinks $SnapshotsDir
    $SnapshotsDir = (Resolve-Path -LiteralPath $SnapshotsDir).Path
    foreach ($name in @('default-Client Core.vcxproj','optin-Client Core.vcxproj','optin-Game DE.vcxproj','optin-MTASA.sln')) {
        $snapshotHashes[$name] = Sha (Join-Path $SnapshotsDir $name)
    }
    foreach ($name in @('premake5.lua','Client/core/premake5.lua','Client/game_de/premake5.lua',
        'Client/core/CCore.cpp','Client/core/CCore.h','Client/core/CGameAdapterModule.cpp',
        'Tests/definitive/test_core_backend_project_selection.ps1')) {
        $sourceHashes[$name] = Sha (Join-Path $root $name)
    }
    [xml]$default = [IO.File]::ReadAllText((Join-Path $SnapshotsDir 'default-Client Core.vcxproj'))
    [xml]$optin = [IO.File]::ReadAllText((Join-Path $SnapshotsDir 'optin-Client Core.vcxproj'))
    [xml]$backend = [IO.File]::ReadAllText((Join-Path $SnapshotsDir 'optin-Game DE.vcxproj'))
    $solution = [IO.File]::ReadAllText((Join-Path $SnapshotsDir 'optin-MTASA.sln'))
    $configurations = @($optin.SelectNodes('//*[local-name()="ProjectConfiguration"]') | ForEach-Object { $_.GetAttribute('Include') })
    $defaultConfigurations = @($default.SelectNodes('//*[local-name()="ProjectConfiguration"]') | ForEach-Object { $_.GetAttribute('Include') })
    Check ($configurations.Count -eq 9 -and ($configurations -join ';') -ceq ($defaultConfigurations -join ';')) 'Core must retain its actual nine configuration/platform combinations.'
    $coreGuid = Guid $optin
    $backendGuid = Guid $backend
    Check ($coreGuid -ceq (Guid $default)) 'Default and opt-in snapshots must describe the same Core project.'
    foreach ($configuration in $configurations) {
        $normalGroup = CoreSettingsGroup $default $configuration
        $optinGroup = CoreSettingsGroup $optin $configuration
        $normalDefinitions = Definitions $normalGroup
        $optinDefinitions = Definitions $optinGroup
        $supported = $configuration -ceq 'Release|x64'
        $hasBackend = $optinDefinitions -contains 'MTA_WITH_DEFINITIVE_ADAPTER'
        Check (-not ($normalDefinitions -contains 'MTA_WITH_DEFINITIVE_ADAPTER')) ('Default Core selected DE: ' + $configuration)
        Check ($hasBackend -eq $supported) ('DE startup macro escaped its supported configuration: ' + $configuration)
        Check ((SolutionBuilds $solution $backendGuid $configuration) -eq $supported) ('Game DE build eligibility differs: ' + $configuration)
        # The adapter is loaded by exact path at runtime. An order-only
        # dependency must never turn into a Game DE import-library link.
        Check ((ToolXml $optinGroup 'Link') -notmatch '(?i)game[ _-]?de(?:\.lib|\.dll)') ('Core acquired game_de linkage: ' + $configuration)
        if ($configuration.EndsWith('|Win32',[StringComparison]::Ordinal)) {
            foreach ($tool in @('ClCompile','ResourceCompile','Link')) {
                Check ((ToolXml $normalGroup $tool) -ceq (ToolXml $optinGroup $tool)) ('DE opt-in changed classic ' + $tool + ' metadata: ' + $configuration)
            }
            Check (SolutionBuilds $solution $coreGuid $configuration) ('Classic Core became excluded from solution builds: ' + $configuration)
        }
        $selections.Add([ordered]@{configuration=$configuration;definitiveStartupMacro=$hasBackend;backendBuildEligible=(SolutionBuilds $solution $backendGuid $configuration)})
    }
    # Premake's VS dependson representation is a solution project-dependency
    # union, not conditioned native ProjectReference entries. Check the real
    # order dependency and backend Build.0 eligibility independently.
    $coreBlockPattern = '(?ms)^Project\("[^\"]+"\)\s*=\s*"[^\"]+",\s*"[^\"]+",\s*"' + [regex]::Escape($coreGuid) + '"\r?\n(?<body>.*?)^EndProject\s*$'
    $coreBlock = [regex]::Match($solution,$coreBlockPattern)
    Check $coreBlock.Success 'Core project block is missing from the opt-in solution.'
    $dependencyPattern = '(?m)^\s*' + [regex]::Escape($backendGuid) + '\s*=\s*' + [regex]::Escape($backendGuid) + '\s*$'
    $hasDependency = $coreBlock.Groups['body'].Value -match $dependencyPattern
    Check $hasDependency 'Core must retain the opt-in Game DE build-order dependency.'
    $record.solutionDependency = @{coreGuid=$coreGuid;backendGuid=$backendGuid;present=$hasDependency;representation='Solution ProjectDependencies union; backend Build.0 eligibility checked separately.'}
    $record.projectReferences = @($optin.SelectNodes('//*[local-name()="ProjectReference"]') | ForEach-Object { $_.OuterXml })
    foreach ($name in $sourceHashes.Keys) {
        Check ((Sha (Join-Path $root $name)) -ceq $sourceHashes[$name]) ('Canonical source changed: ' + $name)
    }
    foreach ($name in $snapshotHashes.Keys) {
        Check ((Sha (Join-Path $SnapshotsDir $name)) -ceq $snapshotHashes[$name]) ('Generated snapshot changed: ' + $name)
    }
    $record.inputsUnchanged = $true
    $record.success = $true
} catch {
    $record.failure = $_.Exception.ToString()
} finally {
    $record.checks = $checks
    [IO.File]::WriteAllText($OutputPath, ($record | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}
$record | ConvertTo-Json -Depth 12
if (-not $record.success) { throw ('Core backend project selection failed; see ' + $OutputPath) }
