#!/usr/bin/env pwsh
#requires -Version 7.0
# Load real built Core/Game DE artifacts in an owned non-game host. The exact
# executable gate must fail before engine memory access; this is startup testing.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDll,
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][string]$PthreadDll,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedCoreSha256,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedGameDeSha256,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture runner requires a native x64 PowerShell process.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-core-entry/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $outputRoot 'mta/logs') | Out-Null
$sourceNames=@('Tests/definitive/MtaCoreEntryTests.cpp','Tests/definitive/test_mta_core_entry.ps1',
    'Client/core/Core.cpp','Client/core/CCore.cpp','Client/core/CCore.h','Client/core/CGameAdapterModule.cpp',
    'Client/core/CGameAdapterModule.h','Client/core/DefinitiveProcess.h','Client/game_de/GameAdapter.cpp',
    'Client/game_de/NativePadReader.cpp','Client/game_de/NativePadReader.h','Client/sdk/game/CGameAdapter.h',
    'Client/game_de/NativeMovementInput.cpp','Client/game_de/NativeMovementInput.h','Client/game_de/NativeInputRouting.h',
    'Client/game_de/premake5.lua','vendor/detours/premake5.lua','vendor/detours/4.0.1/src/detours.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h')
$sourceHashes=[ordered]@{}
foreach ($name in $sourceNames) { $sourceHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MtaCoreEntryTests.cpp') -Destination (Join-Path $outputRoot 'MtaCoreEntryTests.cpp.snapshot')
$staged=@()
foreach ($item in @(
    @{source=$CoreDll;name='core.dll';expected=$ExpectedCoreSha256},
    @{source=$GameDeDll;name='game_de.dll';expected=$ExpectedGameDeSha256},
    @{source=$PthreadDll;name='pthread.dll';expected=$ExpectedPthreadSha256})) {
    $resolved=(Resolve-Path -LiteralPath $item.source).Path
    $bytes=[IO.File]::ReadAllBytes($resolved)
    if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) { throw ('Invalid DLL image: '+$resolved) }
    $pe=[BitConverter]::ToInt32($bytes,0x3c)
    if ($pe -lt 64 -or $pe -gt $bytes.Length-26 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x20b -or
        -not ([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000)) { throw ('Expected an AMD64 PE32+ DLL: '+$resolved) }
    $sha=(Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.expected -and $sha -ne $item.expected.ToLowerInvariant()) { throw ('Artifact fingerprint changed: '+$resolved) }
    $destination=Join-Path $outputRoot ('mta/'+$item.name)
    Copy-Item -LiteralPath $resolved -Destination $destination
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw 'Staged artifact fingerprint mismatch.' }
    $staged += [ordered]@{source=$resolved;staged=$destination;sha256=$sha;machine='AMD64';bytes=$bytes.Length}
}
# These legacy import dependencies are not a DE renderer claim. Use installed,
# signed Microsoft system copies; do not execute newly downloaded runtime DLLs.
$systemDependencies=@()
foreach ($name in @('d3dx9_43.dll','XINPUT1_4.dll')) {
    $path=Join-Path $env:windir ('System32/'+$name)
    $signature=Get-AuthenticodeSignature -LiteralPath $path
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft') { throw ('Missing/unverified system dependency: '+$path) }
    $systemDependencies += [ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant();signature=$signature.Status.ToString();signer=$signature.SignerCertificate.Subject}
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs= & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio native x64 tools are required.' }
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$command='call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev
$environment= & $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$hostPath=Join-Path $outputRoot 'SanAndreas.exe'
$compilerArguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    (Join-Path $PSScriptRoot 'MtaCoreEntryTests.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$hostPath))
$compilerOutput=@(& $compiler @compilerArguments 2>&1)
$compilerExit=$LASTEXITCODE
$compilerOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$hostHash=$null
$hostExit=$null
$hostResult=$null
$failure=$null
if ($compilerExit -eq 0) {
    $hostHash=(Get-FileHash -LiteralPath $hostPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hostHash -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Fixture unexpectedly matches the real game image.' }
    Push-Location -LiteralPath $outputRoot
    try { $hostOutput=@(& $hostPath $outputRoot 2>&1);$hostExit=$LASTEXITCODE } finally { Pop-Location }
    $hostOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'host.log') -Encoding utf8
    try { $hostResult=($hostOutput -join "`n") | ConvertFrom-Json } catch { $failure='Host did not publish its complete result.' }
} else { $failure='Fixture compilation failed.' }
$coreLog=Join-Path $outputRoot 'mta/logs/logfile.txt'
$coreLogText=if (Test-Path -LiteralPath $coreLog) { Get-Content -LiteralPath $coreLog -Raw } else { '' }
$bindingRejections=[regex]::Matches($coreLogText,'DE adapter initialization rejected: DE adapter input binding: BindNativeInput: Unsupported').Count
$sourcesUnchanged=$true
foreach ($name in $sourceNames) {
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHashes[$name]) { $sourcesUnchanged=$false }
}
$success=$compilerExit -eq 0 -and $hostExit -eq 0 -and $null -ne $hostResult -and $hostResult.success -and $bindingRejections -eq 2 -and $sourcesUnchanged
$record=[ordered]@{recorded_at=[DateTimeOffset]::Now.ToString('o');success=$success;directory=$outputRoot;
    scope='Actual supplied Core/Game DE DLL entry point in a separate non-game host named SanAndreas.exe';
    compiler_exit=$compilerExit;host_exit=$hostExit;host_result=$hostResult;failure=$failure;host_sha256=$hostHash;
    source_hashes=$sourceHashes;sources_unchanged_during_fixture=$sourcesUnchanged;artifacts=$staged;system_dependencies=$systemDependencies;
    core_log_path=$coreLog;exact_binding_rejection_messages=$bindingRejections;
    core_log_sha256=$(if (Test-Path -LiteralPath $coreLog) {(Get-FileHash -LiteralPath $coreLog -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null});
    host_executable_is_game=$false;game_launched=$false;game_memory_read=$false;game_calls_or_hooks=$false;game_files_changed=$false;
    native_engine_input_verified=$false;full_mta_boot_verified=$false}
$record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
$record | ConvertTo-Json -Depth 9 -Compress
if (-not $success) { throw ('Real Core host failed; see '+$outputRoot) }
