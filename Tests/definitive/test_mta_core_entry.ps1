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
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256,
    [switch]$NonClassicLayout
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture runner requires a native x64 PowerShell process.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-core-entry/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $outputRoot 'mta/logs') | Out-Null
$moduleRelative=if ($NonClassicLayout) { 'CLEO/de-online' } else { 'mta' }
if ($NonClassicLayout) {
    # Core's existing event logger does not create its parent directories. Keep
    # its optional diagnostics inside this fixture's explicit nonclassic root.
    New-Item -ItemType Directory -Path (Join-Path $outputRoot ($moduleRelative+'/mta/logs')) | Out-Null
}
$sourceNames=@('Tests/definitive/MtaCoreEntryTests.cpp','Tests/definitive/test_mta_core_entry.ps1',
    'Client/core/DefinitiveClockReference.h','Client/core/DefinitiveClockReference.cpp',
    'Client/core/DefinitiveClockSetReference.h','Client/core/DefinitiveClockSetReference.cpp',
    'Client/core/DefinitiveGuiFrontendReference.h','Client/core/DefinitiveGuiFrontendInputReference.h',
    'Client/core/Core.cpp','Client/core/CCore.cpp','Client/core/CCore.h','Client/core/premake5.lua','Client/core/CGameAdapterModule.cpp',
    'Client/core/CGameAdapterModule.h','Client/core/DefinitiveProcess.h','Client/core/DefinitiveStartupState.h','Client/core/DefinitiveInputReference.h','Client/core/DefinitiveMovementReference.h','Client/core/DefinitiveRenderReference.h','Client/core/DefinitiveRenderEndpointLog.h','Client/sdk/game/CGameRenderEndpointDiagnostic.h',
    'Client/core/CKeyBinds.h','Client/core/CKeyBinds.cpp','Client/core/CTimingCheckpoints.hpp','Client/game_de/GameAdapter.cpp',
    'Client/game_de/NativePadReader.cpp','Client/game_de/NativePadReader.h','Client/sdk/game/CGameAdapter.h',
    'Client/game_de/NativeImageAccess.cpp','Client/game_de/NativeImageAccess.h',
    'Client/game_de/CClockDE.cpp','Client/game_de/CClockDE.h','Client/sdk/game/CClock.h',
    'Client/game_de/NativeClockService.cpp','Client/game_de/NativeClockService.h','Client/sdk/game/CGameClockService.h',
    'Client/game_de/NativeClockProfile.h','Client/game_de/NativeServiceAdmission.h',
    'Client/game_de/NativeMovementInput.cpp','Client/game_de/NativeMovementInput.h','Client/game_de/NativeInputRouting.h',
    'Client/game_de/NativeRenderObservation.cpp','Client/game_de/NativeRenderObservation.h','Client/sdk/game/CGameRenderObservation.h',
    'Client/game_de/NativeD3D12OwnershipProbe.cpp','Client/game_de/NativeD3D12OwnershipProbe.h',
    'Client/game_de/NativeCustomPresentProbe.cpp','Client/game_de/NativeCustomPresentProbe.h',
    'Client/game_de/NativeFrameBoundaryProbe.cpp','Client/game_de/NativeFrameBoundaryProbe.h',
    'Client/game_de/premake5.lua','vendor/detours/premake5.lua','vendor/detours/4.0.1/src/detours.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h')
$sourceHashes=[ordered]@{}
foreach ($name in $sourceNames) { $sourceHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MtaCoreEntryTests.cpp') -Destination (Join-Path $outputRoot 'MtaCoreEntryTests.cpp.snapshot')
# Inspect the actual supplied DLL, rather than treating a linker flag as proof
# that loading Core no longer requires the classic renderer runtime.
function Get-PeImportMetadata([byte[]]$image) {
    function Require-Range([long]$offset,[long]$count) {
        if ($offset -lt 0 -or $count -lt 0 -or $offset -gt $image.LongLength - $count) { throw 'Truncated PE import metadata.' }
    }
    Require-Range 0x3c 4
    $pe=[BitConverter]::ToInt32($image,0x3c)
    Require-Range $pe 24
    $sectionCount=[BitConverter]::ToUInt16($image,$pe+6)
    $optionalSize=[BitConverter]::ToUInt16($image,$pe+20)
    $optional=$pe+24
    Require-Range $optional $optionalSize
    if ($optionalSize -lt 224 -or [BitConverter]::ToUInt32($image,$optional+108) -lt 14) { throw 'PE32+ import directories are missing.' }
    $headerSize=[BitConverter]::ToUInt32($image,$optional+60)
    $sectionTable=$optional+$optionalSize
    Require-Range $sectionTable (40L*$sectionCount)
    $sections=@()
    for ($index=0;$index -lt $sectionCount;$index++) {
        $offset=$sectionTable+40*$index
        $sections+=@{rva=[long][BitConverter]::ToUInt32($image,$offset+12);raw_size=[long][BitConverter]::ToUInt32($image,$offset+16);raw=[long][BitConverter]::ToUInt32($image,$offset+20)}
    }
    function Get-FileOffset([long]$rva,[long]$count) {
        if ($rva -ge 0 -and $rva -lt $headerSize -and $count -le $headerSize-$rva) { Require-Range $rva $count;return [int]$rva }
        foreach ($section in $sections) {
            $delta=$rva-$section.rva
            if ($delta -ge 0 -and $delta -lt $section.raw_size -and $count -le $section.raw_size-$delta) {
                $offset=$section.raw+$delta;Require-Range $offset $count;return [int]$offset
            }
        }
        throw 'PE import RVA is outside initialized file data.'
    }
    function Get-ImportName([long]$rva) {
        $name=[Collections.Generic.List[byte]]::new()
        for ($length=0;$length -lt 512;$length++) {
            $value=$image[(Get-FileOffset ($rva+$length) 1)]
            if ($value -eq 0) { return [Text.Encoding]::ASCII.GetString($name.ToArray()).ToLowerInvariant() }
            if ($value -lt 32 -or $value -gt 126) { throw 'Invalid PE import name.' }
            $name.Add($value)
        }
        throw 'Unterminated PE import name.'
    }
    function Get-DirectoryNames([int]$directory,[int]$stride,[int]$nameOffset) {
        $entry=$optional+112+8*$directory
        $rva=[long][BitConverter]::ToUInt32($image,$entry)
        $size=[long][BitConverter]::ToUInt32($image,$entry+4)
        if ($rva -eq 0 -and $size -eq 0) { return }
        if ($rva -eq 0 -or $size -lt $stride -or $size -gt 1048576) { throw 'Invalid PE import directory extent.' }
        for ($offset=0L;$offset+$stride -le $size;$offset+=$stride) {
            $descriptor=Get-FileOffset ($rva+$offset) $stride
            $nonzero=$false
            for ($byte=0;$byte -lt $stride;$byte++) { if ($image[$descriptor+$byte] -ne 0) { $nonzero=$true;break } }
            if (-not $nonzero) { return }
            if ($directory -eq 13 -and [BitConverter]::ToUInt32($image,$descriptor) -ne 1) { throw 'Expected RVA-based x64 delay imports.' }
            $nameRva=[BitConverter]::ToUInt32($image,$descriptor+$nameOffset)
            if ($nameRva -eq 0) { throw 'Missing PE import name.' }
            Get-ImportName $nameRva
        }
        throw 'Unterminated PE import descriptor table.'
    }
    return [ordered]@{eager=@(Get-DirectoryNames 1 20 12);delayed=@(Get-DirectoryNames 13 32 4)}
}
$staged=@()
$coreImports=$null
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
    if ($item.name -eq 'core.dll') {
        $coreImports=Get-PeImportMetadata $bytes
        if ($coreImports.eager -contains 'd3dx9_43.dll' -or $coreImports.eager -contains 'd3d9.dll' -or $coreImports.delayed -notcontains 'd3dx9_43.dll') {
            throw 'Opt-in x64 Core must defer its classic D3DX9 dependency and have no eager D3D9 renderer import.'
        }
    }
    $destination=Join-Path $outputRoot ($moduleRelative+'/'+$item.name)
    Copy-Item -LiteralPath $resolved -Destination $destination
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw 'Staged artifact fingerprint mismatch.' }
    $staged += [ordered]@{source=$resolved;staged=$destination;sha256=$sha;machine='AMD64';bytes=$bytes.Length}
}
# XInput is an existing Core import. The deferred classic D3DX9 runtime is
# intentionally not staged or required for this DE startup fixture.
$systemDependencies=@()
foreach ($name in @('XINPUT1_4.dll')) {
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
    $hostArguments=@($outputRoot)
    if ($NonClassicLayout) { $hostArguments+='nonclassic' }
    try { $hostOutput=@(& $hostPath @hostArguments 2>&1);$hostExit=$LASTEXITCODE } finally { Pop-Location }
    $hostOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'host.log') -Encoding utf8
    try { $hostResult=($hostOutput -join "`n") | ConvertFrom-Json } catch { $failure='Host did not publish its complete result.' }
} else { $failure='Fixture compilation failed.' }
$coreLog=Join-Path $outputRoot $(if ($NonClassicLayout) { 'CLEO/de-online/mta/logs/logfile.txt' } else { 'mta/logs/logfile.txt' })
$coreLogText=if (Test-Path -LiteralPath $coreLog) { Get-Content -LiteralPath $coreLog -Raw } else { '' }
$bindingRejections=[regex]::Matches($coreLogText,'DE adapter initialization rejected: DE adapter input binding: BindNativeInput: Unsupported').Count
$sourcesUnchanged=$true
foreach ($name in $sourceNames) {
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHashes[$name]) { $sourcesUnchanged=$false }
}
# Each optional protocol must pass through the actual initializer's image
# guard. Core14 adds one explicit request to the seven prior image rejections;
# its actual result and exact total must agree, rather than accepting an extra
# log entry as proof of the new request's ownership.
$success=$compilerExit -eq 0 -and $hostExit -eq 0 -and $null -ne $hostResult -and $hostResult.success -and $hostResult.api4_initialize_result -eq 5 -and $hostResult.api5_initialize_result -eq 5 -and $hostResult.api6_initialize_result -eq 5 -and $hostResult.api13_initialize_result -eq 5 -and $hostResult.api14_initialize_result -eq 5 -and $bindingRejections -eq 8 -and $sourcesUnchanged
$record=[ordered]@{recorded_at=[DateTimeOffset]::Now.ToString('o');success=$success;directory=$outputRoot;
    scope='Actual supplied Core/Game DE DLL entry point in a separate non-game host named SanAndreas.exe';
    compiler_exit=$compilerExit;host_exit=$hostExit;host_result=$hostResult;failure=$failure;host_sha256=$hostHash;nonclassic_layout=$NonClassicLayout.IsPresent;
    source_hashes=$sourceHashes;sources_unchanged_during_fixture=$sourcesUnchanged;artifacts=$staged;system_dependencies=$systemDependencies;core_imports=$coreImports;
    core_log_path=$coreLog;exact_binding_rejection_messages=$bindingRejections;
    core_log_sha256=$(if (Test-Path -LiteralPath $coreLog) {(Get-FileHash -LiteralPath $coreLog -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null});
    host_executable_is_game=$false;game_launched=$false;game_memory_read=$false;game_calls_or_hooks=$false;game_files_changed=$false;
    native_engine_input_verified=$false;full_mta_boot_verified=$false}
$record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
$record | ConvertTo-Json -Depth 9 -Compress
if (-not $success) { throw ('Real Core host failed; see '+$outputRoot) }
