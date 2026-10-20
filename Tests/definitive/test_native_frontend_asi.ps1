#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the exact shipping ASI entry with one controlled delayed Bridge.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$AsiDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedAsiSha256,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedAsiSourceSha256
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$runId = (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-native-frontend-asi/' + $runId)
$scratchParent = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('native-frontend-asi-' + $runId)))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
New-Item -ItemType Directory -Path $scratchRoot | Out-Null
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$sourceNames = @('Client/definitive/NativeFrontendASI.cpp','Client/definitive/NativeFrontendHost.h',
    'Tests/definitive/NativeFrontendASIFixture.h','Tests/definitive/NativeFrontendASIBridgeFixture.cpp',
    'Tests/definitive/NativeFrontendASIHostTests.cpp','Tests/definitive/test_native_frontend_asi.ps1')
$sourceHashes = [ordered]@{}
$compiles = [Collections.Generic.List[object]]::new()
$inputAsi = $null
$artifacts = @()
$run = $null
$failure = $null
$cleanupError = $null
$previousTemp = [Environment]::GetEnvironmentVariable('TEMP','Process')
$previousTmp = [Environment]::GetEnvironmentVariable('TMP','Process')
try {
    foreach ($name in $sourceNames) {
        $source = Join-Path $repositoryRoot $name
        $sourceHashes[$name] = Hash $source
        $saved = Join-Path $outputRoot ('sources/' + $name)
        New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $saved
    }
    if ($sourceHashes['Client/definitive/NativeFrontendASI.cpp'] -ne $ExpectedAsiSourceSha256.ToLowerInvariant()) { throw 'Current ASI source differs from the selected build source.' }
    $asiSource = (Resolve-Path -LiteralPath $AsiDll).Path
    if ((Hash $asiSource) -ne $ExpectedAsiSha256.ToLowerInvariant()) { throw 'Shipping ASI hash differs from the selected normal build.' }
    $inputAsi = [ordered]@{path=$asiSource;sha256=(Hash $asiSource);source='Client/definitive/NativeFrontendASI.cpp';source_sha256=$ExpectedAsiSourceSha256.ToLowerInvariant()}
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio x64 native tools are required.' }
    $dev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    [Environment]::SetEnvironmentVariable('TEMP',$scratchRoot,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$scratchRoot,'Process')
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE')
    function Compile([string]$Name,[string]$Source,[string]$Destination,[string[]]$Extra=@()) {
        $objects = Join-Path $outputRoot ('objects/' + $Name)
        New-Item -ItemType Directory -Path $objects -Force | Out-Null
        New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
        $arguments = $common + $Extra + @($Source,('/Fo'+$objects+[IO.Path]::DirectorySeparatorChar),('/Fe'+$Destination))
        $messages = @(& $compiler @arguments 2>&1)
        $exit = $LASTEXITCODE
        $logPath = Join-Path $outputRoot ($Name+'-compile.log')
        $messages | Set-Content -LiteralPath $logPath -Encoding utf8
        $compiles.Add([ordered]@{name=$Name;exit_code=$exit;log=$logPath;log_sha256=(Hash $logPath);output=$Destination})
        if ($exit -ne 0) { throw "Compilation failed: $Name; see $logPath" }
    }
    $gameRoot = Join-Path $outputRoot 'fixture-game'
    $bridgePath = Join-Path $gameRoot 'CLEO/de-online/de_client_bridge.dll'
    Compile 'controlled-bridge' (Join-Path $PSScriptRoot 'NativeFrontendASIBridgeFixture.cpp') $bridgePath @('/LD')
    $hostPath = Join-Path $outputRoot 'owned-host/NativeFrontendASIHost.exe'
    Compile 'owned-host' (Join-Path $PSScriptRoot 'NativeFrontendASIHostTests.cpp') $hostPath
    $stagedAsi = Join-Path $gameRoot 'MtaDefinitive.asi'
    Copy-Item -LiteralPath $asiSource -Destination $stagedAsi
    if ((Hash $stagedAsi) -ne $inputAsi.sha256) { throw 'Staged shipping ASI differs.' }
    $iniPath = Join-Path $gameRoot 'MtaDefinitive.ini'
    "[MTA]`nEnabled=1`nLocalInput=1`nBindingPersistence=1`n" | Set-Content -LiteralPath $iniPath -Encoding utf8NoBOM
    foreach ($path in @($stagedAsi,$bridgePath,$hostPath,$iniPath)) {
        $artifacts += [ordered]@{path=$path;sha256=(Hash $path);bytes=(Get-Item -LiteralPath $path).Length}
    }
    $foreignWorkingDirectory = Join-Path $outputRoot 'foreign-working-directory'
    New-Item -ItemType Directory -Path $foreignWorkingDirectory | Out-Null
    $stdout = Join-Path $outputRoot 'host-stdout.json'
    $stderr = Join-Path $outputRoot 'host-stderr.txt'
    $process = Start-Process -FilePath $hostPath -ArgumentList @('"'+$stagedAsi+'"') -WorkingDirectory $foreignWorkingDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { $process.Kill(); $process.WaitForExit(); throw 'Owned ASI host timed out.' }
    $exit = $process.ExitCode
    $result = Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
    $run = [ordered]@{exit_code=$exit;result=$result;stdout=$stdout;stdout_sha256=(Hash $stdout);stderr=$stderr;stderr_sha256=(Hash $stderr);working_directory=$foreignWorkingDirectory}
    if ($exit -ne 0 -or -not $result.success) { throw "Shipping ASI entry test failed; see $stdout and $stderr" }
} catch { $failure = $_.Exception.Message }
finally {
    [Environment]::SetEnvironmentVariable('TEMP',$previousTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$previousTmp,'Process')
    try {
        # Delete only the uniquely created task scratch directory, after
        # resolving it and rejecting links; keep all sources/binaries/evidence.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path
        $requiredPrefix = $scratchParent.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
        if (-not $resolvedScratch.StartsWith($requiredPrefix,[StringComparison]::OrdinalIgnoreCase) -or
            [IO.Path]::GetFileName($resolvedScratch) -cne ('native-frontend-asi-'+$runId) -or
            ((Get-Item -LiteralPath $resolvedScratch -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            @(Get-ChildItem -LiteralPath $resolvedScratch -Force -Recurse | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
            throw 'Task scratch cleanup target was not a plain owned workspace directory.'
        }
        Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
    } catch { $cleanupError = $_.Exception.Message }
}
$unchanged = $true
foreach ($name in $sourceNames) { if (-not $sourceHashes.Contains($name) -or (Hash (Join-Path $repositoryRoot $name)) -ne $sourceHashes[$name]) { $unchanged=$false } }
if ($inputAsi -and (Hash $inputAsi.path) -ne $inputAsi.sha256) { $unchanged=$false }
foreach ($artifact in $artifacts) { if ((Hash $artifact.path) -ne $artifact.sha256) { $unchanged=$false } }
$record = [ordered]@{
    recorded_at=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=($null -eq $failure -and $null -eq $cleanupError -and $unchanged -and $null -ne $run);
    failure=$failure;source_hashes=$sourceHashes;input_asi=$inputAsi;sources_and_artifacts_unchanged=$unchanged;compiles=$compiles;artifacts=$artifacts;run=$run;
    scratch_directory=$scratchRoot;scratch_removed=(-not (Test-Path -LiteralPath $scratchRoot));scratch_cleanup_error=$cleanupError;
    positive_integration_cases=1;game_launched=$false;game_process_attached=$false;game_memory_read=$false;installed_files_changed=$false;
    full_mta_startup_verified=$false;
    scope='Exact shipping MtaDefinitive.asi InitializeASI plus a deliberately delayed controlled Bridge in one owned x64 host. Verifies asynchronous entry, repeat admission, typed request and absolute package path independent of working directory. No actual Core, game adapter, GUI, native hooks or GTA workload.'
}
$resultPath = Join-Path $outputRoot 'test-result.json'
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
$record | ConvertTo-Json -Depth 10 -Compress
if (-not $record.success) { throw "Native frontend ASI tests failed: $failure; cleanup: $cleanupError; see $resultPath" }
