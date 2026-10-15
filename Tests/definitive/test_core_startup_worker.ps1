#!/usr/bin/env pwsh
#requires -Version 7.0
# Execute the production worker in owned hosts, first against controlled loader
# fixtures, then against the exact normal Core bundle in a nonclassic layout.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDll,
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][string]$PthreadDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedCoreSha256,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedGameDeSha256,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot = Join-Path $repositoryRoot ('Build/mta-core-startup-worker/' + (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$sourceNames = @('Client/definitive/CoreStartupReference.h','Client/definitive/CoreStartupReference.cpp',
    'Client/core/DefinitiveInputReference.h','Client/core/DefinitiveMovementReference.h','Client/core/DefinitiveRenderReference.h',
    'Tests/definitive/CoreStartupReferenceTests.cpp','Tests/definitive/CoreStartupFixture.cpp','Tests/definitive/test_core_startup_worker.ps1')
$sourceHashes = [ordered]@{}
foreach ($name in $sourceNames) {
    $source = Join-Path $repositoryRoot $name
    $sourceHashes[$name] = Hash $source
    $saved = Join-Path $outputRoot ('sources/' + $name)
    New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $saved
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio x64 native tools are required.' }
$dev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') } }
$compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE')
$compiles = [Collections.Generic.List[object]]::new()
function Compile([string]$Name, [string[]]$Sources, [string]$Destination, [string[]]$Extra = @()) {
    $objects = Join-Path $outputRoot ('objects/' + $Name)
    New-Item -ItemType Directory -Path $objects -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
    $arguments = $common + $Extra + $Sources + @(('/Fo' + $objects + [IO.Path]::DirectorySeparatorChar),('/Fe' + $Destination))
    $messages = @(& $compiler @arguments 2>&1)
    $exit = $LASTEXITCODE
    $log = Join-Path $outputRoot ($Name + '-compile.log')
    $messages | Set-Content -LiteralPath $log -Encoding utf8
    $compiles.Add([ordered]@{name=$Name;exit_code=$exit;log=$log;log_sha256=(Hash $log);output=$Destination})
    if ($exit -ne 0) { throw "Compilation failed: $Name; see $log" }
}
function Run-Host([string]$Name, [string]$Directory, [string[]]$Arguments) {
    $hostPath = Join-Path $Directory 'SanAndreas.exe'
    $stdout = Join-Path $outputRoot ($Name + '-stdout.json')
    $stderr = Join-Path $outputRoot ($Name + '-stderr.txt')
    $quoted = @($Arguments | ForEach-Object { '"' + $_ + '"' })
    # Only this exact freshly built host is started. A timeout terminates its
    # retained Process object rather than searching for any game-named process.
    $process = Start-Process -FilePath $hostPath -ArgumentList $quoted -WorkingDirectory $Directory -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit(); throw "Owned worker fixture timed out: $Name" }
    $exit = $process.ExitCode
    $result = Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
    if ($exit -ne 0 -or -not $result.success) { throw "Worker fixture failed: $Name; see $stdout and $stderr" }
    return [ordered]@{name=$Name;exit_code=$exit;result=$result;stdout=$stdout;stdout_sha256=(Hash $stdout);stderr=$stderr;host=$hostPath;host_sha256=(Hash $hostPath)}
}
$failure = $null
$runs = @()
$artifacts = @()
try {
    $fixtureDirectory = Join-Path $outputRoot 'fixture-input'
    $normalFixture = Join-Path $fixtureDirectory 'CoreFixture.dll'
    $missingFixture = Join-Path $fixtureDirectory 'CoreMissingExport.dll'
    Compile 'fixture-normal' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $normalFixture @('/LD')
    Compile 'fixture-missing-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingFixture @('/LD','/DCORE_FIXTURE_MISSING_EXPORT=1')
    $syntheticRoot = Join-Path $outputRoot 'synthetic-host'
    $hostPath = Join-Path $syntheticRoot 'SanAndreas.exe'
    Compile 'production-worker-host' @((Join-Path $PSScriptRoot 'CoreStartupReferenceTests.cpp'),(Join-Path $repositoryRoot 'Client/definitive/CoreStartupReference.cpp')) $hostPath @('/DMTA_TEST_CORE_STARTUP_ACCESS=1')
    if ((Hash $hostPath) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Owned host unexpectedly matches the real game image.' }
    $runs += Run-Host 'controlled-loader' $syntheticRoot @($syntheticRoot,'synthetic',$normalFixture,$missingFixture)
    $actualRoot = Join-Path $outputRoot 'actual-host/CLEO/de-online'
    New-Item -ItemType Directory -Path (Join-Path $actualRoot 'mta/logs') -Force | Out-Null
    Copy-Item -LiteralPath $hostPath -Destination (Join-Path $actualRoot 'SanAndreas.exe')
    foreach ($item in @(@{source=$CoreDll;name='core.dll';expected=$ExpectedCoreSha256},
                        @{source=$GameDeDll;name='game_de.dll';expected=$ExpectedGameDeSha256},
                        @{source=$PthreadDll;name='pthread.dll';expected=$ExpectedPthreadSha256})) {
        $source = (Resolve-Path -LiteralPath $item.source).Path
        $hash = Hash $source
        if ($hash -ne $item.expected.ToLowerInvariant()) { throw "Normal artifact hash differs: $source" }
        $bytes = [IO.File]::ReadAllBytes($source)
        if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) { throw 'Invalid normal PE image.' }
        $pe = [BitConverter]::ToInt32($bytes,0x3c)
        if ($pe -lt 64 -or $pe -gt $bytes.Length-26 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
            [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x20b -or
            -not ([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000)) { throw 'Normal artifacts must be AMD64 PE32+ DLLs.' }
        $destination = Join-Path $actualRoot $item.name
        Copy-Item -LiteralPath $source -Destination $destination
        if ((Hash $destination) -ne $hash) { throw 'Staged normal artifact differs.' }
        $artifacts += [ordered]@{source=$source;path=$destination;sha256=$hash;machine='AMD64';bytes=$bytes.Length}
    }
    $runs += Run-Host 'actual-core-wrong-image' $actualRoot @($actualRoot,'actual')
    foreach ($mode in @('movement','movement-cancel','render','render-cancel','render-deadline',
                        'render-v2','render-v2-d3d11','render-v2-cancel','render-v2-deadline','render-v2-missing',
                        'render-v2-cross-route','render-v2-mixed-version','render-v2-wrong-mask',
                        'render-v3','render-v3-cancel','render-v3-deadline','render-v3-missing','render-v3-query-failed',
                        'render-v3-cross-route','render-v3-mixed-version','render-v3-wrong-mask',
                        'render-v4','render-v4-cancel','render-v4-deadline','render-v4-missing','render-v4-query-failed','render-v4-unknown-presenter',
                        'render-v4-cross-route','render-v4-mixed-version','render-v4-wrong-mask',
                        'render-v5','render-v5-cancel','render-v5-deadline','render-v5-missing','render-v5-query-failed','render-v5-unknown-presenter',
                        'render-v5-cross-route','render-v5-mixed-version','render-v5-wrong-mask',
                        'render-v6','render-v6-cancel','render-v6-deadline','render-v6-missing','render-v6-query-failed','render-v6-unknown-presenter',
                        'render-v6-cross-route','render-v6-mixed-version','render-v6-wrong-mask','render-v6-cache-contention')) {
        $movementRoot = Join-Path $outputRoot ($mode + '-host')
        New-Item -ItemType Directory -Path $movementRoot -Force | Out-Null
        Copy-Item -LiteralPath $hostPath -Destination (Join-Path $movementRoot 'SanAndreas.exe')
        # This is the real backend DLL loaded/PINned without invoking its factory
        # or native methods. The controlled Core tests worker lifetime only.
        Copy-Item -LiteralPath $GameDeDll -Destination (Join-Path $movementRoot 'game_de.dll')
        if ((Hash (Join-Path $movementRoot 'game_de.dll')) -ne $ExpectedGameDeSha256.ToLowerInvariant()) { throw 'Movement PIN fixture backend hash differs.' }
        $runs += Run-Host $mode $movementRoot @($movementRoot,$mode,$normalFixture)
    }
} catch { $failure = $_.Exception.Message }
$unchanged = $true
foreach ($name in $sourceNames) { if ((Hash (Join-Path $repositoryRoot $name)) -ne $sourceHashes[$name]) { $unchanged=$false } }
foreach ($artifact in $artifacts) { if ((Hash $artifact.source) -ne $artifact.sha256) { $unchanged=$false } }
$record = [ordered]@{
    recorded_at=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=($null -eq $failure -and $unchanged -and $runs.Count -eq 51);
    failure=$failure;source_hashes=$sourceHashes;sources_unchanged=$unchanged;compiles=$compiles;runs=$runs;normal_artifacts=$artifacts;
    fixture_executable_is_game=$false;game_launched=$false;game_memory_read=$false;game_process_attached=$false;game_files_changed=$false;
    synthetic_core_is_mta=$false;full_mta_boot_verified=$false;engine_input_write_verified=$false;
    scope='Actual production async worker, controlled non-MTA loader-lock fixtures, and normal Core exact-image rejection in owned nonclassic hosts'
}
$resultPath = Join-Path $outputRoot 'test-result.json'
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
$record | ConvertTo-Json -Depth 10 -Compress
if (-not $record.success) { throw "Core startup worker tests failed: $failure; see $resultPath" }
