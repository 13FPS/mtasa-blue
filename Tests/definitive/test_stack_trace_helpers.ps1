#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual Core helper at both pointer widths; access only memory
# allocated by this fixture and its own module, never a game or foreign process.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-core-stack-trace/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/core/StackTraceHelpers.h', 'Client/core/CExceptionInformation_Impl.cpp', 'Client/core/CCrashDumpWriter.cpp', 'Client/core/CrashHandler.cpp', 'Tests/definitive/StackTraceHelpersTests.cpp', 'Tests/definitive/test_stack_trace_helpers.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$results = @()
$failure = $null
foreach ($architecture in @('x86', 'x64')) {
    $outputDirectory = Join-Path $outputRoot $architecture
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand, $architecture
    $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
    $executable = Join-Path $outputDirectory 'stack_trace_helpers_tests.exe'
    $arguments = @('/nologo','/std:c++20','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        (Join-Path $PSScriptRoot 'StackTraceHelpersTests.cpp'), ('/Fo' + (Join-Path $outputDirectory 'test.obj')),
        ('/Fe' + $executable), '/link', 'dbghelp.lib')
    # x86 fallback uses frame chains; x64 deliberately retains optimized unwind metadata.
    if ($architecture -eq 'x86') { $arguments = @('/Oy-') + $arguments }
    $compilerOutput = @(& $compiler @arguments 2>&1)
    $compilerExit = $LASTEXITCODE
    $compilerOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'compile.log') -Encoding utf8
    $testExit = $null
    $testCount = $null
    if ($compilerExit -eq 0) {
        $testOutput = @(& $executable 2>&1)
        $testExit = $LASTEXITCODE
        $testOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'test.log') -Encoding utf8
        if (($testOutput -join "`n") -match 'Passed (\d+) Core stack trace helper checks') { $testCount = [int]$Matches[1] }
    }
    $results += [ordered]@{ architecture=$architecture; compilerExit=$compilerExit; testExit=$testExit; checks=$testCount;
        executable=$executable; executableSha256=$(if (Test-Path -LiteralPath $executable) {
            (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { $null }) }
    if ($compilerExit -ne 0 -or $testExit -ne 0 -or $null -eq $testCount) { $failure = "$architecture compilation/tests failed; see $outputDirectory" }
}
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged = $false }
}
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=($null -eq $failure -and $sourceUnchanged); sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged;
    results=$results; failure=$failure; gameLaunched=$false; foreignProcessAccessed=$false; installationChanged=$false; fullMtaClientBuilt=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Core stack trace helper checks failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 8 -Compress
