#!/usr/bin/env pwsh
#requires -Version 7.0
# Build the existing codec target in both architectures, then exchange actual
# compressed frames between owned hosts. Generation/restoration belongs to the
# caller's serialized normal build flow, not this dependency-only test.
[CmdletBinding()]
param(
    [string]$ProjectPath,
    [int]$TimeoutMs = 60000
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'Use native x64 PowerShell.' }
if ($TimeoutMs -lt 1000 -or $TimeoutMs -gt 120000) { throw 'TimeoutMs must be between 1000 and 120000.' }
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build'))
if (-not $ProjectPath) { $ProjectPath = Join-Path $buildRoot 'libspeex.vcxproj' }
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
if (-not $ProjectPath.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use the normal generated project inside workspace Build.'
}
$taskId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $buildRoot ('mta-speex-client-interop/' + $taskId)
$scratchRoot = Join-Path $buildRoot ('tmp/speex-client-compiler-' + $taskId)
$utf8 = [Text.UTF8Encoding]::new($false)
$previousTemp = $env:TEMP; $previousTmp = $env:TMP
$sourcePins = @(); $projectPin = $null; $deathmatchProjectPin = $null; $solutionPin = $null; $solutionText = $null; $msbuildPin = $null
$builds = @(); $runs = @(); $artifacts = @(); $classicPins = @()
$sourcesUnchanged = $false; $projectUnchanged = $false; $classicPreservedDuringX64 = $false; $scratchClean = $false
$classicOutputPathUnchanged = $false; $completedDirections = 0
$failure = $null; $success = $false; $guardChecks = 0

function Guard([bool]$Condition, [string]$Why) {
    $script:guardChecks++
    if (-not $Condition) { throw $Why }
}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Artifact([string]$Path) { [ordered]@{path=$Path;sha256=(Sha $Path);bytes=(Get-Item -LiteralPath $Path).Length} }
function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked build input/output: $candidate"
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}
function Machine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path); $reader = [IO.BinaryReader]::new($stream)
    try {
        Guard ($reader.ReadUInt16() -eq 0x5a4d) "Not a PE file: $Path"
        $stream.Position = 0x3c; $stream.Position = $reader.ReadUInt32()
        Guard ($reader.ReadUInt32() -eq 0x4550) "Invalid PE signature: $Path"
        return $reader.ReadUInt16()
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function LibraryMachines([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    Guard ($bytes.Length -ge 8 -and [Text.Encoding]::ASCII.GetString($bytes, 0, 8) -eq "!<arch>`n") 'Expected an actual COFF static library.'
    $offset = 8; $machines = @()
    while ($offset + 60 -le $bytes.Length) {
        $name = [Text.Encoding]::ASCII.GetString($bytes, $offset, 16).Trim()
        $size = [int][Text.Encoding]::ASCII.GetString($bytes, $offset + 48, 10).Trim()
        $start = $offset + 60
        Guard ($size -ge 0 -and $start + $size -le $bytes.Length) 'COFF library member is truncated.'
        if ($name -ne '/' -and $name -ne '//' -and $size -ge 20) {
            $machine = [BitConverter]::ToUInt16($bytes, $start)
            if ($machine -eq 0 -and [BitConverter]::ToUInt16($bytes, $start + 2) -eq 65535) {
                $machine = [BitConverter]::ToUInt16($bytes, $start + 6)
            }
            $machines += $machine
        }
        $offset = $start + $size + ($size % 2)
    }
    Guard ($offset -eq $bytes.Length -and $machines.Count -gt 0) 'COFF library must contain complete machine-typed objects.'
    return @($machines | Sort-Object -Unique)
}
function Namespace([xml]$Document) {
    $namespace = [Xml.XmlNamespaceManager]::new($Document.NameTable)
    $namespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return ,$namespace
}
function Property([xml]$Document, [string]$Configuration, [string]$Name) {
    $namespace = Namespace $Document
    $values = @($Document.SelectNodes('//m:PropertyGroup', $namespace) | Where-Object {
        $_.GetAttribute('Condition').Contains("'$Configuration'")
    } | ForEach-Object { $_.SelectNodes(('m:' + $Name), $namespace) } | ForEach-Object { $_.InnerText })
    Guard ($values.Count -eq 1) "Generated $Configuration $Name must be explicit and unique."
    return $values[0]
}
function ProjectOutput([string]$Value) {
    $directory = [IO.Path]::GetDirectoryName($ProjectPath) + [IO.Path]::DirectorySeparatorChar
    $expanded = $Value.Replace('$(ProjectDir)', $directory)
    Guard (-not $expanded.Contains('$(')) 'Unexpected unresolved output directory macro.'
    return [IO.Path]::GetFullPath([IO.Path]::Combine($directory, $expanded)).TrimEnd('\', '/')
}
function SourcesExcluded([xml]$Document, [string]$Configuration, [bool]$Expected) {
    $namespace = Namespace $Document
    $items = @($Document.SelectNodes('//m:ClCompile[@Include]', $namespace))
    Guard ($items.Count -gt 0) 'The normal generated target must declare its actual sources.'
    foreach ($item in $items) {
        $entries = @($item.SelectNodes('m:ExcludedFromBuild', $namespace) | Where-Object {
            -not $_.GetAttribute('Condition') -or $_.GetAttribute('Condition').Contains("'$Configuration'")
        })
        $excluded = @($entries | Where-Object InnerText -eq 'true').Count -gt 0
        Guard ($excluded -eq $Expected) "Unexpected source exclusion in $Configuration for $($item.GetAttribute('Include'))."
    }
}
function SolutionBuildPolicy([xml]$Document, [string]$ProjectName, [string]$Configuration, [bool]$ExpectedBuild) {
    $namespace = Namespace $Document
    $guids = @($Document.SelectNodes('//m:ProjectGuid', $namespace) | ForEach-Object InnerText)
    Guard ($guids.Count -eq 1 -and $guids[0] -match '^\{[0-9A-Fa-f-]{36}\}$') 'Normal project identity must have one explicit GUID.'
    $guid = [regex]::Escape($guids[0])
    $projectNamePattern = [regex]::Escape($ProjectName)
    Guard ([regex]::Matches($solutionText, ('(?m)^Project\([^\r\n]+\) = "' + $projectNamePattern + '", "' + $projectNamePattern + '\.vcxproj", "' + $guid + '"\r?$')).Count -eq 1) 'Solution must select this exact normal project identity once.'
    $configurations = @($Document.SelectNodes('//m:ProjectConfiguration[@Include]', $namespace) | ForEach-Object { $_.GetAttribute('Include') })
    Guard (@($configurations | Where-Object { $_ -eq $Configuration }).Count -eq 1) "Project configuration is absent or duplicated: $Configuration"
    # Premake's project-level ExcludeFromBuild flag removes the solution's
    # Build.0 entry; it does not mark individual ClCompile items as excluded.
    # Check the actual solution admission separately from source inclusion.
    $solutionConfiguration = [regex]::Escape($Configuration)
    $projectConfiguration = [regex]::Escape($Configuration)
    $prefix = '(?m)^\s*' + $guid + '\.' + $solutionConfiguration
    Guard ([regex]::Matches($solutionText, ($prefix + '\.ActiveCfg = ' + $projectConfiguration + '\s*$')).Count -eq 1) "Solution must map the exact $Configuration project configuration."
    $builds = [regex]::Matches($solutionText, ($prefix + '\.Build\.0 = [^\r\n]+\r?$'))
    Guard ($builds.Count -eq $(if ($ExpectedBuild) { 1 } else { 0 })) "Unexpected solution build admission: $ProjectName $Configuration"
    if ($ExpectedBuild) {
        Guard ([regex]::Matches($solutionText, ($prefix + '\.Build\.0 = ' + $projectConfiguration + '\s*$')).Count -eq 1) "Solution build mapping differs for $ProjectName $Configuration."
    }
}
function RunHost([string]$HostPath, [string]$Architecture, [string]$Operation, [string]$Fixture, [string]$Label, [string]$ExpectedFailure = '') {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $HostPath; $start.WorkingDirectory = [IO.Path]::GetDirectoryName($HostPath)
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.ArgumentList.Add($Operation); $start.ArgumentList.Add($Fixture)
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    $watch = [Diagnostics.Stopwatch]::StartNew(); $timedOut = $false
    try {
        Guard ($process.Start()) 'Owned codec host did not start.'
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMs)) {
            $timedOut = $true
            # Only this newly started owned codec process is eligible for timeout
            # termination. GTA and unrelated processes are never selected here.
            $process.Kill(); $process.WaitForExit()
        }
        $watch.Stop(); $exitCode = $process.ExitCode
        $text = $stdout.GetAwaiter().GetResult(); $errors = $stderr.GetAwaiter().GetResult()
        $stdoutPath = Join-Path $outputRoot ($Label + '.stdout.log'); $stderrPath = Join-Path $outputRoot ($Label + '.stderr.log')
        [IO.File]::WriteAllText($stdoutPath, $text, $utf8); [IO.File]::WriteAllText($stderrPath, $errors, $utf8)
        $summaries = @($text -split '\r?\n' | Where-Object { $_.StartsWith('{') })
        $summary = $null
        if ($summaries.Count -eq 1) { $summary = $summaries[0] | ConvertFrom-Json }
        $script:runs += [ordered]@{label=$Label;architecture=$Architecture;operation=$Operation;fixture=$Fixture;exit_code=$exitCode;
            elapsed_ms=$watch.ElapsedMilliseconds;timed_out=$timedOut;expected_failure=$ExpectedFailure;result=$summary;
            stdout=(Artifact $stdoutPath);stderr=(Artifact $stderrPath)}
        Guard (-not $timedOut) "Owned codec host timed out: $Label"
        if ($ExpectedFailure) {
            Guard ($exitCode -eq 1 -and $summaries.Count -eq 0 -and $errors.Contains('SPEEX_INTEROP_FAILURE ' + $ExpectedFailure)) "Negative fixture was not rejected: $Label"
            return
        }
        Guard ($exitCode -eq 0 -and $summaries.Count -eq 1) "Owned codec host failed: $Label; inspect $stderrPath"
        $pointerBytes = if ($Architecture -eq 'x86') { 4 } else { 8 }
        Guard ($summary.schema -eq 'mta_speex_client_interop_v1' -and $summary.success -and $summary.operation -eq $Operation -and
            $summary.pointerBytes -eq $pointerBytes -and $summary.cases -eq 18 -and $summary.frames -eq 864 -and $summary.checks -gt 1000 -and
            $summary.decoderCreations -eq 18 -and $summary.liveOwners -eq 0 -and $summary.bitsetCreations -ge 864) 'Codec host result violates the actual dependency contract.'
        Guard ($summary.minimumCorrelation -ge 0.65 -and $summary.maximumReconstructionError -le 0.95 -and
            $summary.maximumCrossDecoderRms -le 16 -and $summary.maximumCrossDecoderDifference -le 256) 'Actual codec measurements exceed their documented tolerance.'
        Guard (-not $summary.heapLeakInstrumented -and -not $summary.audioHardwareTested -and -not $summary.networkStarted -and
            -not $summary.gameplayReady -and -not $summary.fullMtaReady) 'Dependency checks cannot promote untested hardware, heap or gameplay readiness.'
        if ($Operation -eq 'encode') {
            Guard ($summary.encoderCreations -eq 18 -and $summary.preprocessorCreations -eq 18 -and $summary.expectedWarnings -eq 24) 'Encoder must exercise MTA preprocessing and repeated native lifetimes.'
        } else {
            Guard ($summary.encoderCreations -eq 0 -and $summary.preprocessorCreations -eq 0 -and $summary.expectedWarnings -eq 6) 'Interop decoding must consume foreign bytes without recreating an encoder.'
        }
        $warningLines = @($errors -split '\r?\n' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        Guard ($warningLines.Count -eq $summary.expectedWarnings) 'Unexpected Speex stderr output or warning count.'
        foreach ($warning in $warningLines) {
            Guard ($warning -eq 'warning: The VAD has been replaced by a hack pending a complete rewrite' -or
                $warning -match '^warning: Unknown nb_ctl request: +4$') 'An unrecognized codec warning cannot be treated as a pass.'
        }
    } finally { $process.Dispose() }
}

try {
    foreach ($path in @($ProjectPath, $outputRoot, $scratchRoot)) { Assert-NoLinks $path }
    [IO.Directory]::CreateDirectory($outputRoot) | Out-Null
    [IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
    $env:TEMP = $scratchRoot; $env:TMP = $scratchRoot
    $sourceNames = @('premake5.lua', 'vendor/libspeex/premake5.lua', 'Client/mods/deathmatch/premake5.lua',
        'Client/mods/deathmatch/CVoiceRecorder.cpp', 'Client/mods/deathmatch/CVoiceRecorder.h',
        'Client/mods/deathmatch/logic/CClientPlayerVoice.cpp', 'utils/definitive_edition/SpeexClientInteropTests.cpp',
        'utils/definitive_edition/test_speex_client_x64.ps1')
    $sourceNames += @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'vendor/libspeex') -File -Recurse |
        Where-Object Extension -in @('.c', '.h') | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot, $_.FullName).Replace('\', '/') })
    $sourcePins = @($sourceNames | Sort-Object -Unique | ForEach-Object { $path = Join-Path $taskRoot $_; Assert-NoLinks $path; [ordered]@{path=$_;sha256=(Sha $path)} })
    $projectPin = Artifact $ProjectPath
    $solutionPath = Join-Path $buildRoot 'MTASA.sln'
    Assert-NoLinks $solutionPath
    $solutionPin = Artifact $solutionPath
    $solutionText = [IO.File]::ReadAllText($solutionPath)
    [xml]$project = Get-Content -LiteralPath $ProjectPath -Raw
    Guard ((ProjectOutput (Property $project 'Release|Win32' 'OutDir')) -eq [IO.Path]::GetFullPath((Join-Path $buildRoot 'bin/x86/Release'))) 'Classic Speex output path must remain unchanged.'
    $classicOutputPathUnchanged = $true
    $isolatedOutput = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Bin/mods/deathmatch/x64'))
    Guard ((ProjectOutput (Property $project 'Release|x64' 'OutDir')) -eq $isolatedOutput) 'Generate with --with-definitive-adapter: x64 Speex must use its isolated normal output.'
    foreach ($configuration in @('Release|Win32', 'Release|x64')) {
        Guard ((Property $project $configuration 'TargetName') -eq 'libspeex' -and (Property $project $configuration 'ConfigurationType') -eq 'StaticLibrary') 'Use the actual normal Speex static-library target.'
        SolutionBuildPolicy $project 'libspeex' $configuration $true
        SourcesExcluded $project $configuration $false
    }
    foreach ($configuration in @('Debug|Win32', 'Nightly|Win32')) { SolutionBuildPolicy $project 'libspeex' $configuration $true }
    foreach ($configuration in @('Debug|x64', 'Nightly|x64', 'Debug|ARM64', 'Nightly|ARM64', 'Release|ARM64')) {
        SolutionBuildPolicy $project 'libspeex' $configuration $false
    }
    $namespace = Namespace $project
    Guard ($project.SelectNodes('//m:ProjectReference', $namespace).Count -eq 0) 'Codec builds cannot pull in unsupported client/game/network projects.'
    $deathmatchProjectPath = Join-Path $buildRoot 'Client Deathmatch.vcxproj'
    Assert-NoLinks $deathmatchProjectPath
    $deathmatchProjectPin = Artifact $deathmatchProjectPath
    [xml]$deathmatchProject = Get-Content -LiteralPath $deathmatchProjectPath -Raw
    SolutionBuildPolicy $deathmatchProject 'Client Deathmatch' 'Release|Win32' $true
    foreach ($configuration in @('Debug|x64', 'Nightly|x64', 'Release|x64')) {
        SolutionBuildPolicy $deathmatchProject 'Client Deathmatch' $configuration $false
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    Guard (-not [string]::IsNullOrWhiteSpace($installation)) 'MSVC x86/x64 tools are required.'
    $developerEnvironment = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
    $msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
    $msbuildPin = Artifact $msbuild
    foreach ($architecture in @('x86', 'x64')) {
        $platform = if ($architecture -eq 'x86') { 'Win32' } else { 'x64' }
        $directory = Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        $environmentLines = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerEnvironment, $architecture)
        Guard ($LASTEXITCODE -eq 0) 'MSVC environment initialization failed.'
        foreach ($entry in $environmentLines) {
            if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
        }
        $env:TEMP = $scratchRoot; $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $buildLog = Join-Path $directory 'normal-build.log'
        & $msbuild $ProjectPath /t:Rebuild /p:Configuration=Release "/p:Platform=$platform" /p:BuildProjectReferences=false /m:1 /nologo /verbosity:minimal 2>&1 |
            Set-Content -LiteralPath $buildLog -Encoding utf8
        $buildExit = $LASTEXITCODE
        $script:builds += [ordered]@{architecture=$architecture;normal_target="libspeex Release|$platform";exit_code=$buildExit;compiler=(Artifact $compiler);log=(Artifact $buildLog)}
        Guard ($buildExit -eq 0) "Normal $architecture Speex build failed: $buildLog"
        $library = Join-Path (ProjectOutput (Property $project "Release|$platform" 'OutDir')) 'libspeex.lib'
        Assert-NoLinks $library
        $savedLibrary = Join-Path $directory 'libspeex.lib'
        [IO.File]::Copy($library, $savedLibrary, $false)
        $libraryPin = Artifact $library; $libraryPin['saved'] = $savedLibrary
        Guard ((Sha $savedLibrary) -eq $libraryPin.sha256) 'Saved normal library bytes must match the built output.'
        $machines = @(LibraryMachines $savedLibrary)
        $expectedMachine = if ($architecture -eq 'x86') { 0x14c } else { 0x8664 }
        Guard ($machines.Count -eq 1 -and $machines[0] -eq $expectedMachine) 'Every static-library object must match the host architecture.'
        $artifacts += $libraryPin
        if ($architecture -eq 'x86') {
            $classicPins = @(Artifact $library)
        } else {
            $classicPreservedDuringX64 = @($classicPins | Where-Object { (Sha $_.path) -ne $_.sha256 }).Count -eq 0
            Guard $classicPreservedDuringX64 'The x64 dependency build modified the classic library output.'
        }
        $hostPath = Join-Path $directory 'OwnedMtaSpeexInterop.exe'
        $compileLog = Join-Path $directory 'host-compile.log'
        & $compiler /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX /DNOMINMAX /DWIN32_LEAN_AND_MEAN `
            ('/I' + (Join-Path $taskRoot 'vendor/libspeex')) (Join-Path $PSScriptRoot 'SpeexClientInteropTests.cpp') `
            ('/Fo' + (Join-Path $directory 'SpeexClientInteropTests.obj')) ('/Fe' + $hostPath) `
            /link /NOLOGO $savedLibrary 2>&1 | Set-Content -LiteralPath $compileLog -Encoding utf8
        $compileExit = $LASTEXITCODE
        $builds[-1]['host_compile_exit'] = $compileExit; $builds[-1]['host_compile_log'] = Artifact $compileLog
        Guard ($compileExit -eq 0) "Owned $architecture codec host compilation failed: $compileLog"
        Guard ((Machine $hostPath) -eq $expectedMachine) 'Owned codec host machine differs from the linked library.'
        $artifacts += Artifact $hostPath
        $fixture = Join-Path $outputRoot ("encoded-$architecture.spxfixture")
        RunHost $hostPath $architecture 'encode' $fixture ("encode-$architecture")
        $artifacts += Artifact $fixture
    }
    foreach ($architecture in @('x86', 'x64')) {
        $otherArchitecture = if ($architecture -eq 'x86') { 'x64' } else { 'x86' }
        $hostPath = Join-Path $outputRoot "$architecture/OwnedMtaSpeexInterop.exe"
        RunHost $hostPath $architecture 'decode' (Join-Path $outputRoot "encoded-$otherArchitecture.spxfixture") ("decode-$architecture-from-$otherArchitecture")
        $completedDirections++
    }
    $x64Host = Join-Path $outputRoot 'x64/OwnedMtaSpeexInterop.exe'
    RunHost $x64Host 'x64' 'decode' (Join-Path $outputRoot 'encoded-x64.spxfixture') 'reject-same-architecture' 'Only an actual opposite-architecture fixture establishes interoperability'
    $fixtureBytes = [IO.File]::ReadAllBytes((Join-Path $outputRoot 'encoded-x86.spxfixture'))
    $truncatedFixture = Join-Path $outputRoot 'truncated.spxfixture'
    [IO.File]::WriteAllBytes($truncatedFixture, [byte[]]$fixtureBytes[0..31])
    RunHost $x64Host 'x64' 'decode' $truncatedFixture 'reject-truncated-fixture' 'Fixture scalar must not be truncated'
    $artifacts += Artifact $truncatedFixture
    $sourcesUnchanged = @($sourcePins | Where-Object { (Sha (Join-Path $taskRoot $_.path)) -ne $_.sha256 }).Count -eq 0
    $projectUnchanged = (Sha $ProjectPath) -eq $projectPin.sha256 -and (Sha $deathmatchProjectPath) -eq $deathmatchProjectPin.sha256 -and
        (Sha $solutionPath) -eq $solutionPin.sha256
    Guard ($sourcesUnchanged -and $projectUnchanged) 'Sources or generated projects changed during codec validation.'
    foreach ($pin in $artifacts) {
        Guard ((Sha $pin.path) -eq $pin.sha256) 'An executed or exchanged artifact changed after validation.'
        if ($pin.Contains('saved')) { Guard ((Sha $pin.saved) -eq $pin.sha256) 'Saved normal-library evidence changed after execution.' }
    }
    $success = $true
} catch { $failure = $_.Exception.Message }
finally {
    $env:TEMP = $previousTemp; $env:TMP = $previousTmp
    try {
        # Delete only this run's explicitly resolved scratch descendants. Logs,
        # normal artifacts and interoperability fixtures remain durable evidence.
        $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot)
        $expectedScratchParent = [IO.Path]::GetFullPath((Join-Path $buildRoot 'tmp')) + [IO.Path]::DirectorySeparatorChar
        Guard ($resolvedScratch.StartsWith($expectedScratchParent, [StringComparison]::OrdinalIgnoreCase)) 'Compiler scratch must remain under this workspace Build/tmp.'
        Assert-NoLinks $resolvedScratch
        if (Test-Path -LiteralPath $resolvedScratch) {
            $scratchItems = @(Get-ChildItem -LiteralPath $resolvedScratch -Force -Recurse)
            foreach ($item in $scratchItems) {
                Guard ($item.FullName.StartsWith($resolvedScratch + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Scratch cleanup escaped its task folder.'
                Assert-NoLinks $item.FullName
            }
            foreach ($item in @($scratchItems | Where-Object { -not $_.PSIsContainer })) { Remove-Item -LiteralPath $item.FullName -Force }
            foreach ($item in @($scratchItems | Where-Object PSIsContainer | Sort-Object { $_.FullName.Length } -Descending)) { Remove-Item -LiteralPath $item.FullName -Force }
            Remove-Item -LiteralPath $resolvedScratch -Force
        }
        $scratchClean = -not (Test-Path -LiteralPath $resolvedScratch)
    } catch {
        $success = $false
        $failure = if ($failure) { $failure + '; scratch cleanup: ' + $_.Exception.Message } else { $_.Exception.Message }
    }
}
if (-not (Test-Path -LiteralPath $outputRoot)) { [IO.Directory]::CreateDirectory($outputRoot) | Out-Null }
$logs = @(Get-ChildItem -LiteralPath $outputRoot -File -Recurse -Filter '*.log' | ForEach-Object { Artifact $_.FullName })
$result = [ordered]@{schema='mta_speex_client_x64_test_v1';success=($success -and $scratchClean);recorded_at=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;
    guard_checks=$guardChecks;normal_targets=@('libspeex Release|Win32', 'libspeex Release|x64');builds=$builds;runs=$runs;
    source_count=$sourcePins.Count;source_hashes=$sourcePins;sources_unchanged=$sourcesUnchanged;project=$projectPin;deathmatch_project=$deathmatchProjectPin;solution=$solutionPin;
    projects_unchanged=$projectUnchanged;classic_output_path_unchanged=$classicOutputPathUnchanged;classic_library_preserved_during_x64=$classicPreservedDuringX64;
    classic_artifacts=$classicPins;artifacts=$artifacts;logs=$logs;msbuild=$msbuildPin;scratch_directory=$scratchRoot;scratch_clean=$scratchClean;error=$failure;
    scope='Existing MTA Speex/SpeexDSP source builds and actual x86-to-x64 and x64-to-x86 compressed-frame decoding with MTA codec/preprocessor settings. No BASS/PortAudio/net substitution or native game service.';
    cross_architecture_directions=$completedDirections;game_launched=$false;audio_hardware_tested=$false;network_started=$false;deathmatch_enabled=$false;
    package_modified=$false;heap_leak_instrumented=$false;gameplayReady=$false;fullMtaReady=$false;project_generation_or_restoration_performed=$false}
$receipt = Join-Path $outputRoot 'test-result.json'
[IO.File]::WriteAllText($receipt, (($result | ConvertTo-Json -Depth 18) + "`n"), $utf8)
Write-Host ("Speex client interoperability success=$($result.success); receipt=$receipt")
if (-not $result.success) { throw $failure }
