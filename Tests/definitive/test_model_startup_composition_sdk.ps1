#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the real composed model receiver with its inherited request/reference
# identity. Only the remaining pure SDK methods receive rejecting fixture stubs.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-model-startup-composition-sdk/' + $id)
$scratchParent = Join-Path $taskRoot 'Build/tmp/model-startup-composition-sdk'
$scratchRoot = Join-Path $scratchParent ('s-' + $id)
$previousEnvironment = @{}
$record = [ordered]@{
    schema = 'mta_model_startup_composition_sdk_tests_v1'; directory = $outputRoot
    success = $false; failure = $null; recordedAt = [DateTimeOffset]::Now.ToString('o')
    sourceHashes = @(); sourcesUnchanged = $true; generatedFixture = $null; results = @(); checks = 0
    scratchDirectory = $scratchRoot; scratchRemoved = $false; actualModelStartupCompositionSdkTested = $false
    scope = 'Actual composed LOD/time/dummies/object-group/wheel/unmodified/request/reference/diagnostics SDK receiver; controlled owner replies and one inherited model identity. Remaining pure methods abort if called.'
    nativeEngineCodeExecuted = $false; gameLaunched = $false; gameAttached = $false
    installationChanged = $false; completeModelInfoReady = $false; productionFactoryPublished = $false
}
function NoLinks([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if ($resolved -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)') { throw 'Excluded SDK path refused' }
    for ($ancestor = $resolved; $ancestor; $ancestor = [IO.Path]::GetDirectoryName($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw ('Linked SDK path refused: ' + $ancestor)
        }
    }
}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunOwned([string]$Executable, [string[]]$Arguments, [string]$Directory, [string]$Name, [int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $command = Join-Path $Directory ($Name + '-command.json')
    $executableHash = Sha $Executable
    [ordered]@{ executable = $Executable; sha256 = $executableHash; arguments = $Arguments
        workingDirectory = $Directory; deadlineMs = $DeadlineMs; temporaryDirectory = $scratchRoot } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw ('Could not start owned ' + $Name + ' process') }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the process tree created by this fixture invocation is owned.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw ('Owned ' + $Name + ' process failed to terminate') }
        }
        [IO.File]::WriteAllText($stdout, $readOut.GetAwaiter().GetResult(), [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr, $readErr.GetAwaiter().GetResult(), [Text.UTF8Encoding]::new($false))
        [ordered]@{ name = $Name; command = $command; commandSha256 = (Sha $command); exitCode = $process.ExitCode
            timedOut = $timedOut; elapsedMs = $timer.ElapsedMilliseconds; stdout = $stdout; stderr = $stderr
            stdoutSha256 = (Sha $stdout); stderrSha256 = (Sha $stderr) }
    } finally { $timer.Stop(); $process.Dispose() }
}
try {
    foreach ($directory in @($outputRoot, $scratchRoot)) {
        NoLinks $directory
        [IO.Directory]::CreateDirectory($directory) | Out-Null
    }
    foreach ($name in @('TEMP', 'TMP', 'VCToolsInstallDir', 'INCLUDE', 'LIB', 'LIBPATH', 'PATH', 'CL', '_CL_')) {
        $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    foreach ($name in @('CL', '_CL_')) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    $productionSources = @(
        'Client/game_de/CModelInfoDE.cpp', 'Client/game_de/CModelInfoRequestDE.cpp', 'Client/game_de/CModelInfoReferenceDE.cpp',
        'Client/game_de/CModelInfoDiagnosticsDE.cpp', 'Client/game_de/CModelInfoUnmodifiedDE.cpp', 'Client/game_de/CModelInfoWheelDE.cpp',
        'Client/game_de/CModelInfoObjectGroupDE.cpp', 'Client/game_de/CModelInfoDummiesDE.cpp', 'Client/game_de/CModelInfoTimeDE.cpp',
        'Client/game_de/CModelInfoLodDE.cpp', 'Client/game_de/CStreamingDE.cpp'
    )
    $sourceNames = $productionSources + @(
        'Client/game_de/CModelInfoStartupDE.h', 'Client/sdk/game/CGame.h', 'Client/sdk/core/CCoreInterface.h',
        'Tests/definitive/ModelStartupCompositionSdkTests.cpp', 'Tests/definitive/Write-ModelStartupCompositionFixture.ps1',
        'Tests/definitive/test_model_startup_composition_sdk.ps1'
    )
    $pending = [Collections.Generic.Queue[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in $sourceNames) { $pending.Enqueue($name) }
    while ($pending.Count) {
        $name = $pending.Dequeue()
        if (-not $seen.Add($name)) { continue }
        $path = [IO.Path]::GetFullPath((Join-Path $taskRoot $name))
        if (-not $path.StartsWith($taskRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'SDK graph left the workspace' }
        NoLinks $path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('SDK source missing: ' + $name) }
        foreach ($match in [regex]::Matches([IO.File]::ReadAllText($path), '(?m)^\s*#\s*include\s*[<"](?<path>[^>"\r\n]+)[>"]')) {
            $include = $match.Groups['path'].Value
            foreach ($candidate in @((Join-Path (Split-Path -Parent $path) $include),
                (Join-Path $taskRoot ('Shared/sdk/' + $include)), (Join-Path $taskRoot ('Client/sdk/' + $include)),
                (Join-Path $taskRoot ('vendor/' + $include)))) {
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $relative = [IO.Path]::GetRelativePath($taskRoot, [IO.Path]::GetFullPath($candidate)).Replace('\', '/')
                    if ($relative.StartsWith('../')) { throw 'SDK include escaped the workspace' }
                    $pending.Enqueue($relative)
                    break
                }
            }
        }
    }
    # Preserve precisely this receiver's local include graph so later source
    # changes cannot replace the bytes accompanying a retained fixture result.
    $record.sourceHashes = @($seen | Sort-Object | ForEach-Object {
        $path = Join-Path $taskRoot $_
        $snapshot = Join-Path $outputRoot ('sources/' + $_)
        [IO.Directory]::CreateDirectory((Split-Path -Parent $snapshot)) | Out-Null
        Copy-Item -LiteralPath $path -Destination $snapshot
        [ordered]@{ path = $_; sha256 = (Sha $path); snapshot = $snapshot; snapshotSha256 = (Sha $snapshot) }
    })
    . (Join-Path $PSScriptRoot 'Write-ModelStartupCompositionFixture.ps1')
    $generated = Join-Path $scratchRoot 'ModelStartupCompositionFixture.generated.h'
    $record.generatedFixture = Write-ModelStartupCompositionFixture $taskRoot $generated
    # A fixture rejector must never mask one of the 51 inherited production
    # methods, particularly the model identity shared with request/reference.
    if ($record.generatedFixture['inheritedModelMethods'] -ne 51 -or $record.generatedFixture['rejectingModelMethods'] -ne 41 -or
        $record.generatedFixture['inheritedStreamingMethods'] -ne 12 -or 'GetModel' -notin $record.generatedFixture['modelInheritedNames'] -or
        $record.generatedFixture['productionFactoryPublished'] -ne $false) { throw 'Composed model fixture SDK partition changed' }
    $durableGenerated = Join-Path $outputRoot 'ModelStartupCompositionFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture['durablePath'] = $durableGenerated
    $record.generatedFixture['durableSha256'] = Sha $durableGenerated
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'MSVC x64 tools are required' }
    $directory = Join-Path $outputRoot 'x64'
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $developer = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $batch = Join-Path $directory 'capture-msvc.cmd'
    @('@echo off', ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f $developer),
        'if errorlevel 1 exit /b 1', 'set VCToolsInstallDir', 'set INCLUDE', 'set LIB', 'set PATH', 'exit /b 0') |
        Set-Content -LiteralPath $batch -Encoding ascii
    $environment = RunOwned $env:ComSpec @('/d', '/s', '/c', 'capture-msvc.cmd') $directory 'msvc-environment' 30000
    if ($environment.exitCode -ne 0 -or $environment.timedOut) { throw 'MSVC x64 environment initialization failed' }
    foreach ($entry in [IO.File]::ReadAllLines($environment.stdout)) {
        if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $executable = Join-Path $directory 'model_startup_composition_sdk_tests.exe'
    # Reuse the real unmodified-model SDK fixture's strict diagnostics profile;
    # the new composition adds mutation layers without changing that contract.
    $arguments = @('/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/O2', '/MD', '/DMTA_DEBUG', '/DWIN32', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN',
        '/D_CRT_SECURE_NO_WARNINGS', '/D_SCL_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_DEPRECATE', '/D_TIMESPEC_DEFINED',
        ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')), ('/I' + (Join-Path $taskRoot 'vendor')), ('/I' + $scratchRoot),
        (Join-Path $PSScriptRoot 'ModelStartupCompositionSdkTests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo' + $scratchRoot + [IO.Path]::DirectorySeparatorChar), ('/Fd' + (Join-Path $scratchRoot 'compiler.pdb')), ('/Fe' + $executable), '/link', 'bcrypt.lib')
    $result = [ordered]@{ architecture = 'x64'; environment = $environment; compile = $null; test = $null
        checks = 0; pointerBits = $null; executable = $executable; executableSha256 = $null; actualModelStartupCompositionSdkTested = $false }
    $record.results += @($result)
    $result.compile = RunOwned $compiler $arguments $directory 'compile' 120000
    if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw 'Actual composed model SDK fixture compilation failed' }
    $result.executableSha256 = Sha $executable
    $result.test = RunOwned $executable @() $directory 'test' 60000
    $stdout = [IO.File]::ReadAllText($result.test.stdout)
    $summary = [regex]::Match($stdout.TrimEnd(), '(?:^|\r?\n)Passed (\d+) model startup composition SDK checks \((\d+)-bit; CPU provider doubles only\)$')
    if ($summary.Success) { $result.checks = [int]$summary.Groups[1].Value; $result.pointerBits = [int]$summary.Groups[2].Value }
    $result.actualModelStartupCompositionSdkTested = [regex]::Matches($stdout, '(?m)^Production model startup composition SDK fixture completed\r?$').Count -eq 1
    if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -ne 49 -or $result.pointerBits -ne 64 -or -not $result.actualModelStartupCompositionSdkTested) {
        throw 'Composed model SDK checks, x64 summary or exact-once completion marker failed'
    }
    # Explicit dictionary access also works for this single-result x64 cohort.
    foreach ($completed in $record.results) { $record.checks += [int]$completed['checks'] }
    $record.actualModelStartupCompositionSdkTested = $result.actualModelStartupCompositionSdkTested
    $record.success = $record.results.Count -eq 1 -and $record.actualModelStartupCompositionSdkTested
} catch { AddFailure $_.Exception.ToString() }
finally {
    foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process') }
    foreach ($entry in $record.sourceHashes) {
        try {
            if ((Sha (Join-Path $taskRoot $entry.path)) -cne $entry.sha256 -or (Sha $entry.snapshot) -cne $entry.snapshotSha256) { $record.sourcesUnchanged = $false }
        } catch { $record.sourcesUnchanged = $false; AddFailure $_.Exception.ToString() }
    }
    try {
        $resolved = [IO.Path]::GetFullPath($scratchRoot)
        $parent = [IO.Path]::GetFullPath($scratchParent).TrimEnd('\') + '\'
        if (-not $resolved.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase)) { throw 'SDK scratch escaped its task parent' }
        NoLinks $resolved
        if (Test-Path -LiteralPath $resolved) {
            # Generated headers, objects and compiler/PDB files are all flat.
            # Preserve an unexpected directory or link instead of following it.
            $entries = @(Get-ChildItem -LiteralPath $resolved -Force)
            foreach ($entry in $entries) { if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Unexpected scratch directory/link preserved' } }
            foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
            [IO.Directory]::Delete($resolved, $false)
        }
        $record.scratchRemoved = $true
    } catch { AddFailure $_.Exception.ToString() }
    $record.success = $record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{ success = $record.success; checks = $record.checks; receipt = (Join-Path $outputRoot 'test-result.json'); failure = $record.failure } | ConvertTo-Json -Compress
if (-not $record.success) { throw ('Actual model startup composition SDK fixture failed; see ' + $outputRoot) }
