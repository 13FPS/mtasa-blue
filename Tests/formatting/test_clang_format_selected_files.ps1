#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the actual utility and pinned formatter only in an owned fixture.
[CmdletBinding()]
param([string]$FormatterPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This owned-process fixture targets the Windows formatter; existing Linux CI runs remain unchanged.' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $FormatterPath) { $FormatterPath = Join-Path $root 'Build/tmp/clang-format.exe' }
$knownFormatter = '11defe615493876745f36fb77e9783e7eed03a7f800044ff58619d9293e79409'
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$scratchBase = Join-Path $root 'Build/tmp'
$scratch = Join-Path $scratchBase ('clang-format-selection-tests-' + $runId)
$fixture = Join-Path $scratch 'fixture repository with spaces'
$evidence = Join-Path $root ('Build/mta-clang-format-selection/' + $runId)
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = 0
$cases = [Collections.Generic.List[object]]::new()
$sourceHashes = [ordered]@{}

function NoLinks([string]$Path) {
    $absolute = [IO.Path]::GetFullPath($Path)
    if ($absolute -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)') { throw 'Excluded path refused.' }
    for ($ancestor = $absolute; $ancestor; $ancestor = [IO.Path]::GetDirectoryName($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked fixture path refused.' }
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
function WriteFixture([string]$Relative, [string]$Text) {
    $path = Join-Path $fixture $Relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
    [IO.File]::WriteAllText($path, $Text, $utf8)
    return $path
}
function InvokeCase([string]$Name, [bool]$Selected, [string[]]$Files, [bool]$ShouldSucceed) {
    $casePath = Join-Path $scratch ($Name + '.json')
    $case = @{utility=(Join-Path $fixture 'utils/clang-format.ps1');selected=$Selected;files=$Files}
    [IO.File]::WriteAllText($casePath, ($case | ConvertTo-Json -Depth 5), $utf8)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $PSHOME 'pwsh.exe'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.WorkingDirectory = $scratch
    foreach ($argument in @('-NoLogo','-NoProfile','-NonInteractive','-File',(Join-Path $scratch 'runner.ps1'),'-CaseFile',$casePath)) { $start.ArgumentList.Add($argument) }
    $start.Environment['TEMP'] = $scratch
    $start.Environment['TMP'] = $scratch
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'Owned formatter fixture could not start.' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw ('Formatter fixture timed out: ' + $Name)
        }
        $stdoutPath = Join-Path $evidence ($Name + '-stdout.log')
        $stderrPath = Join-Path $evidence ($Name + '-stderr.log')
        [IO.File]::WriteAllText($stdoutPath, $stdout.GetAwaiter().GetResult(), $utf8)
        [IO.File]::WriteAllText($stderrPath, $stderr.GetAwaiter().GetResult(), $utf8)
        Check (($process.ExitCode -eq 0) -eq $ShouldSucceed) ($Name + ' returned an unexpected success/failure status; see ' + $stderrPath)
        $remainingScratch = @(Get-ChildItem -LiteralPath (Join-Path $fixture 'Build/tmp') -Directory -Filter 'clang-format-*')
        Check ($remainingScratch.Count -eq 0) ($Name + ' leaked response-file scratch.')
        Check ((Sha (Join-Path $fixture 'Build/tmp/clang-format.exe')) -ceq $knownFormatter) ($Name + ' changed the trusted formatter cache.')
        $cases.Add([ordered]@{name=$Name;exitCode=$process.ExitCode;shouldSucceed=$ShouldSucceed;responseScratchRemoved=$true;stdout=$stdoutPath;stderr=$stderrPath})
    } finally { $process.Dispose() }
}

$record = [ordered]@{
    schema='mta_clang_format_selected_files_v1';success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$evidence
    checks=0;cases=$cases;sourceHashes=$sourceHashes;sourceUnchanged=$false;formatterSha256=$null
    selectedSourceFormatted=$false;selectedInlFormatted=$false;selectedOutput=@();unrelatedFileUnchanged=$false;defaultEnumerationRetained=$false
    formatterFailureCleanup=$false;fixtureScratchRemoved=$false;compilerExecuted=$false;linuxCiExecuted=$false
    projectsRegenerated=$false;gameLaunched=$false;installationChanged=$false;failure=$null
}
NoLinks $scratch
NoLinks $evidence
if ((Sha $FormatterPath) -cne $knownFormatter) { throw 'Use the exact cached formatter pinned by the actual utility; no downloads or substitute tools are allowed.' }
[IO.Directory]::CreateDirectory($scratch) | Out-Null
[IO.Directory]::CreateDirectory($evidence) | Out-Null
try {
    foreach ($name in @('utils/clang-format.ps1','Tests/formatting/test_clang_format_selected_files.ps1')) { $sourceHashes[$name] = Sha (Join-Path $root $name) }
    $record.formatterSha256 = $knownFormatter
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'utils')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'Build/tmp')) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'utils/clang-format.ps1') -Destination (Join-Path $fixture 'utils/clang-format.ps1')
    Copy-Item -LiteralPath $FormatterPath -Destination (Join-Path $fixture 'Build/tmp/clang-format.exe')
    $style = "BasedOnStyle: LLVM`nIndentWidth: 4`n"
    $null = WriteFixture '.clang-format' $style
    $selected = WriteFixture 'Client/selected source é.cpp' 'int selected(){return 1;}'
    $inline = WriteFixture 'Shared/selected-inline.inl' 'inline int selectedInline(){return 2;}'
    $unrelated = WriteFixture 'Server/unrelated.cpp' 'int unrelated(){return 3;}'
    $unrelatedBefore = Sha $unrelated
    $null = WriteFixture 'Tests/placeholder.h' 'int placeholder(){return 4;}'
    $unsupported = WriteFixture 'Shared/readme.txt' 'int text(){return 5;}'
    $outsideRoots = WriteFixture 'Build/outside-source-roots.cpp' 'int outsideRoots(){return 6;}'
    $outsideRepository = Join-Path $scratch 'outside-repository.cpp'
    [IO.File]::WriteAllText($outsideRepository, 'int outsideRepository(){return 7;}', $utf8)
    $outsideBefore = Sha $outsideRepository
    # Keep array binding inside PowerShell rather than constructing a command
    # string or relying on native -File parsing of a string-array parameter.
    $runner = @'
param([Parameter(Mandatory)][string]$CaseFile)
$ErrorActionPreference = 'Stop'
$case = [IO.File]::ReadAllText($CaseFile) | ConvertFrom-Json
$parameters = @{}
if ($case.selected) { $parameters.Files = [string[]]$case.files }
try { & $case.utility @parameters; exit 0 }
catch { [Console]::Error.WriteLine($_.Exception.ToString()); exit 1 }
'@
    [IO.File]::WriteAllText((Join-Path $scratch 'runner.ps1'), $runner, $utf8)
    InvokeCase 'selected-source-and-inline' $true @('Client/selected source é.cpp',$inline,'Client/selected source é.cpp') $true
    # Preserve exact output before content assertions so an expectation failure
    # remains diagnosable after the owned source tree is cleaned up.
    $observedDirectory = Join-Path $evidence 'selected-output'
    [IO.Directory]::CreateDirectory($observedDirectory) | Out-Null
    foreach ($observed in @(@{path=$selected;name='selected.cpp'},@{path=$inline;name='selected.inl'})) {
        $savedOutput = Join-Path $observedDirectory $observed.name
        Copy-Item -LiteralPath $observed.path -Destination $savedOutput
        $record.selectedOutput += [ordered]@{path=$savedOutput;sha256=(Sha $savedOutput);bytes=(Get-Item -LiteralPath $savedOutput).Length;text=[IO.File]::ReadAllText($savedOutput)}
    }
    # These fixture inputs omit the EOF newline. The default formatter preserves
    # that choice, so require its exact spacing without inventing an insertion.
    Check ([IO.File]::ReadAllText($selected) -ceq 'int selected() { return 1; }') 'The selected source was not formatted by the actual tool.'
    Check ([IO.File]::ReadAllText($inline) -ceq 'inline int selectedInline() { return 2; }') 'The explicit .inl selection was not formatted.'
    Check ((Sha $unrelated) -ceq $unrelatedBefore) 'Explicit selection reformatted an unrelated source file.'
    $record.selectedSourceFormatted = $record.selectedInlFormatted = $record.unrelatedFileUnchanged = $true

    foreach ($invalid in @(
        @{name='missing-file';path='Client/missing.cpp'},
        @{name='directory';path='Client'},
        @{name='unsupported-extension';path=$unsupported},
        @{name='outside-source-roots';path=$outsideRoots},
        @{name='outside-repository';path=$outsideRepository},
        @{name='excluded-text-path';path='Client/r-engineer/never-read.cpp'}
    )) { InvokeCase $invalid.name $true @($invalid.path) $false }
    Check ((Sha $outsideRepository) -ceq $outsideBefore) 'An outside-repository file changed.'
    Check ((Sha $unrelated) -ceq $unrelatedBefore) 'Validation failures reformatted an unrelated source file.'

    $null = WriteFixture '.clang-format' "UnknownSelectedFormatterFixtureOption: true`n"
    InvokeCase 'formatter-failure-cleanup' $true @($selected) $false
    $record.formatterFailureCleanup = $true
    $null = WriteFixture '.clang-format' $style

    $defaultFiles = @('Client/default.c','Client/default.cc','Server/default.cpp','Shared/default.h','Tests/default.hh','Tests/default.hpp')
    $defaultBefore = [ordered]@{}
    foreach ($name in $defaultFiles) { $path = WriteFixture $name 'int defaultSource(){return 8;}'; $defaultBefore[$name] = Sha $path }
    $null = WriteFixture 'Shared/selected-inline.inl' 'inline int selectedInline(){return 2;}'
    $inlineBeforeDefault = Sha $inline
    InvokeCase 'default-source-enumeration' $false @() $true
    foreach ($name in $defaultFiles) { Check ((Sha (Join-Path $fixture $name)) -cne $defaultBefore[$name]) ('No-argument enumeration skipped a legacy extension/root: ' + $name) }
    Check ((Sha $inline) -ceq $inlineBeforeDefault) 'No-argument mode changed its legacy extension selection.'
    Check ((Sha $outsideRepository) -ceq $outsideBefore) 'No-argument enumeration left the owned source roots.'
    $record.defaultEnumerationRetained = $true
    foreach ($name in $sourceHashes.Keys) { Check ((Sha (Join-Path $root $name)) -ceq $sourceHashes[$name]) ('Canonical fixture input changed: ' + $name) }
    Check ((Sha $FormatterPath) -ceq $knownFormatter) 'The canonical formatter binary changed.'
    $record.sourceUnchanged = $true
    $record.success = $true
} catch {
    $record.failure = $_.Exception.ToString()
} finally {
    try {
        $resolvedScratch = [IO.Path]::GetFullPath($scratch)
        if (-not $resolvedScratch.StartsWith([IO.Path]::GetFullPath($scratchBase) + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing fixture cleanup outside Build/tmp.'
        }
        NoLinks $resolvedScratch
        if (Test-Path -LiteralPath $resolvedScratch) { Remove-Item -LiteralPath $resolvedScratch -Recurse -Force }
        $record.fixtureScratchRemoved = -not (Test-Path -LiteralPath $resolvedScratch)
    } catch {
        $record.success = $false
        $record.failure = (($record.failure, $_.Exception.ToString()) | Where-Object { $_ }) -join [Environment]::NewLine
    }
    $record.checks = $checks
    [IO.File]::WriteAllText((Join-Path $evidence 'test-result.json'), ($record | ConvertTo-Json -Depth 12), $utf8)
}
$record | ConvertTo-Json -Depth 12
if (-not $record.success) { throw ('Selected formatter verification failed; see ' + (Join-Path $evidence 'test-result.json')) }
