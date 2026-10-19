#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production startup owner with real x86/x64 DLLs and a sibling
# dependency. No game, Core factory, network factory or installed file is used.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot = Join-Path $taskRoot ('Build/mta-startup-module/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$sourceNames = @('Client/core/StartupModule.h','Client/core/CModuleLoader.h','Client/core/CModuleLoader.cpp',
    'Client/core/CModManager.h','Client/core/CModManager.cpp','Client/core/NetworkModuleStartup.h','Tests/definitive/NetworkModuleStartupTests.cpp',
    'Tests/definitive/StartupModuleTests.cpp','Tests/definitive/test_startup_module.ps1')
$sourceHashes = @($sourceNames | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_)).Hash.ToLowerInvariant()} })
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio x86/x64 compiler tools are required.' }
$developerCommand = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$builds = @()
$runs = @()
$failure = $null
foreach ($architecture in @('x86','x64')) {
    $directory = Join-Path $outputRoot $architecture
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand,$architecture
    $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
    $common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN')
    $dependency = Join-Path $directory 'startup-dependency.dll'
    $provider = Join-Path $directory 'startup-provider.dll'
    $testExecutable = Join-Path $directory 'startup-module-tests.exe'
    $networkExecutable = Join-Path $directory 'network-startup-tests.exe'
    foreach ($spec in @(
        @{name='dependency';args=@('/LD','/DMTA_TEST_STARTUP_DEPENDENCY',('/Fo'+(Join-Path $directory 'dependency.obj')),('/Fe'+$dependency),'/link','/EXPORT:StartupDependencyMarker')},
        @{name='provider';args=@('/LD','/DMTA_TEST_STARTUP_PROVIDER',('/Fo'+(Join-Path $directory 'provider.obj')),('/Fe'+$provider),
            (Join-Path $directory 'startup-dependency.lib'),'/link','/EXPORT:StartupProviderMarker')},
        @{name='host';args=@(('/Fo'+(Join-Path $directory 'host.obj')),('/Fe'+$testExecutable))},
        @{name='network';args=@(('/Fo'+(Join-Path $directory 'network.obj')),('/Fe'+$networkExecutable))}
    )) {
        $testSource = Join-Path $PSScriptRoot $(if ($spec.name -eq 'network') { 'NetworkModuleStartupTests.cpp' } else { 'StartupModuleTests.cpp' })
        $compileOutput = @(& $compiler @common $testSource @($spec.args) 2>&1)
        $compileExit = $LASTEXITCODE
        $log = Join-Path $directory ($spec.name+'-compile.log')
        $compileOutput | Set-Content -LiteralPath $log -Encoding utf8
        if ($compileExit -ne 0) { $failure = "$architecture $($spec.name) compilation failed: $log"; break }
    }
    if ($failure) { break }
    $builds += [ordered]@{architecture=$architecture;directory=$directory;provider=$provider;executable=$testExecutable;networkExecutable=$networkExecutable;
        providerSha256=(Get-FileHash -LiteralPath $provider).Hash.ToLowerInvariant();dependencySha256=(Get-FileHash -LiteralPath $dependency).Hash.ToLowerInvariant();
        executableSha256=(Get-FileHash -LiteralPath $testExecutable).Hash.ToLowerInvariant();networkExecutableSha256=(Get-FileHash -LiteralPath $networkExecutable).Hash.ToLowerInvariant()}
}
if (-not $failure) {
    foreach ($build in $builds) {
        $other = @($builds | Where-Object architecture -ne $build.architecture)[0]
        $testOutput = @(& $build.executable $build.provider $other.provider $build.directory 2>&1)
        $testExit = $LASTEXITCODE
        $log = Join-Path $build.directory 'test.log'
        $testOutput | Set-Content -LiteralPath $log -Encoding utf8
        $count = $null
        if (($testOutput -join "`n") -match 'Passed (\d+) startup module checks') { $count = [int]$Matches[1] }
        $runs += [ordered]@{architecture=$build.architecture;exitCode=$testExit;checks=$count;log=$log}
        if ($testExit -ne 0 -or $null -eq $count) { $failure = "$($build.architecture) startup module checks failed: $log"; break }
        $networkOutput = @(& $build.networkExecutable 2>&1)
        $networkExit = $LASTEXITCODE
        $networkLog = Join-Path $build.directory 'network-test.log'
        $networkOutput | Set-Content -LiteralPath $networkLog -Encoding utf8
        $networkCount = $null
        if (($networkOutput -join "`n") -match 'Passed (\d+) network startup checks') { $networkCount = [int]$Matches[1] }
        $runs[-1].networkExitCode = $networkExit
        $runs[-1].networkChecks = $networkCount
        $runs[-1].networkLog = $networkLog
        if ($networkExit -ne 0 -or $null -eq $networkCount) { $failure = "$($build.architecture) network startup checks failed: $networkLog"; break }
    }
}
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path)).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged = $false }
}
$record = [ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=($null -eq $failure -and $sourceUnchanged);
    sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;builds=$builds;runs=$runs;failure=$failure;gameLaunched=$false;installationChanged=$false;networkFactoryCalled=$false}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Startup module validation failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 8 -Compress
