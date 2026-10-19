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
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256,
    [string]$XmllDll = (Join-Path $PSScriptRoot '../../Bin/server/x64/xmll.dll'),
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedXmllSha256 = '80c521b1cac98ed5263ab500510853dd06e6572870350caf6957c78e9c3d7349'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot = Join-Path $repositoryRoot ('Build/mta-core-startup-worker/' + (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$sourceNames = @('Client/definitive/CoreStartupReference.h','Client/definitive/CoreStartupReference.cpp','Client/definitive/FrontendControlMailbox.h',
    'Client/definitive/FrontendRuntimeReport.h','Client/definitive/FrontendRuntimeMailbox.h','Client/definitive/FrontendRuntimeCache.h','Client/core/DefinitiveFrontendRuntime.h','Client/core/DefinitiveLocalInputRuntime.h','Client/definitive/coreconfig.xml',
    'Client/definitive/FrontendStartupReport.h','Client/definitive/PersistentFrontendHostLease.h','Client/core/DefinitivePersistentFrontend.h','Client/core/DefinitiveFrontendStartup.h','Client/core/DefinitiveGuiFrontendInputReference.h',
    'Client/core/DefinitiveFrontend.h','Client/core/DefinitiveFrontendClockReference.h','Client/core/DefinitiveGuiFrontendReference.h',
    'Client/core/DefinitiveInputReference.h','Client/core/DefinitiveMovementReference.h','Client/core/DefinitiveClockReference.h','Client/core/DefinitiveClockSetReference.h','Client/core/DefinitiveApplicationFrameReference.h','Client/core/DefinitiveRenderReference.h','Client/core/DefinitiveGuiStartupReference.h','Client/core/DefinitiveGuiSurfaceReference.h',
    'Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameApplicationFrameObservation.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
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
$common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE',
    ('/I'+(Join-Path $repositoryRoot 'Client/sdk')),('/I'+(Join-Path $repositoryRoot 'Shared/sdk')))
$compiles = [Collections.Generic.List[object]]::new()
function Compile([string]$Name, [string[]]$Sources, [string]$Destination, [string[]]$Extra = @()) {
    $objects = Join-Path $outputRoot ('objects/' + $Name)
    New-Item -ItemType Directory -Path $objects -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
    # Controlled providers deliberately throw through C-named exports to test
    # the worker's fault quarantine. Disable /EHc only for those fixture DLLs;
    # the actual worker host retains production's /EHsc exception model.
    $exceptionOptions = if ($Name.StartsWith('fixture-',[StringComparison]::Ordinal)) { @('/EHc-') } else { @() }
    $arguments = $common + $exceptionOptions + $Extra + $Sources + @(('/Fo' + $objects + [IO.Path]::DirectorySeparatorChar),('/Fe' + $Destination))
    $messages = @(& $compiler @arguments 2>&1)
    $exit = $LASTEXITCODE
    $log = Join-Path $outputRoot ($Name + '-compile.log')
    $messages | Set-Content -LiteralPath $log -Encoding utf8
    $compiles.Add([ordered]@{name=$Name;exit_code=$exit;log=$log;log_sha256=(Hash $log);output=$Destination;exception_options=$exceptionOptions})
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
    $missingGuiFixture = Join-Path $fixtureDirectory 'CoreMissingGuiExport.dll'
    $missingSurfaceFixture = Join-Path $fixtureDirectory 'CoreMissingSurfaceExport.dll'
    $missingClockFixture = Join-Path $fixtureDirectory 'CoreMissingClockExport.dll'
    $missingClockSetFixture = Join-Path $fixtureDirectory 'CoreMissingClockSetExport.dll'
    $missingApplicationFrameFixture = Join-Path $fixtureDirectory 'CoreMissingApplicationFrameExport.dll'
    $missingFrontendClockFixture = Join-Path $fixtureDirectory 'CoreMissingFrontendClockExport.dll'
    $missingLocalRuntimeFixture = Join-Path $fixtureDirectory 'CoreMissingLocalRuntimeExport.dll'
    Compile 'fixture-normal' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $normalFixture @('/LD')
    Compile 'fixture-missing-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingFixture @('/LD','/DCORE_FIXTURE_MISSING_EXPORT=1')
    Compile 'fixture-missing-gui-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingGuiFixture @('/LD','/DCORE_FIXTURE_MISSING_GUI_EXPORT=1')
    Compile 'fixture-missing-surface-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingSurfaceFixture @('/LD','/DCORE_FIXTURE_MISSING_SURFACE_EXPORT=1')
    Compile 'fixture-missing-clock-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingClockFixture @('/LD','/DCORE_FIXTURE_MISSING_CLOCK_EXPORT=1')
    Compile 'fixture-missing-clock-set-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingClockSetFixture @('/LD','/DCORE_FIXTURE_MISSING_CLOCK_SET_EXPORT=1')
    Compile 'fixture-missing-application-frame-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingApplicationFrameFixture @('/LD','/DCORE_FIXTURE_MISSING_APPLICATION_FRAME_EXPORT=1')
    Compile 'fixture-missing-frontend-clock-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingFrontendClockFixture @('/LD','/DCORE_FIXTURE_MISSING_FRONTEND_CLOCK_EXPORT=1')
    Compile 'fixture-missing-local-runtime-export' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingLocalRuntimeFixture @('/LD','/DCORE_FIXTURE_MISSING_LOCAL_RUNTIME_EXPORT=1')
    $syntheticRoot = Join-Path $outputRoot 'synthetic-host'
    $hostPath = Join-Path $syntheticRoot 'SanAndreas.exe'
    # Local input selection observes the foreground root HWND in the production
    # worker. Link its actual Win32 imports explicitly in this owned host too.
    Compile 'production-worker-host' @((Join-Path $PSScriptRoot 'CoreStartupReferenceTests.cpp'),(Join-Path $repositoryRoot 'Client/definitive/CoreStartupReference.cpp')) $hostPath @('/DMTA_TEST_CORE_STARTUP_ACCESS=1','user32.lib')
    if ((Hash $hostPath) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Owned host unexpectedly matches the real game image.' }
    $runs += Run-Host 'controlled-loader' $syntheticRoot @($syntheticRoot,'synthetic',$normalFixture,$missingFixture)
    $actualRoot = Join-Path $outputRoot 'actual-host/CLEO/de-online'
    New-Item -ItemType Directory -Path (Join-Path $actualRoot 'mta/logs') -Force | Out-Null
    Copy-Item -LiteralPath $hostPath -Destination (Join-Path $actualRoot 'SanAndreas.exe')
    foreach ($item in @(@{source=$CoreDll;name='core.dll';expected=$ExpectedCoreSha256},
                        @{source=$GameDeDll;name='game_de.dll';expected=$ExpectedGameDeSha256},
                        @{source=$PthreadDll;name='pthread.dll';expected=$ExpectedPthreadSha256},
                        @{source=$XmllDll;name='xmll.dll';expected=$ExpectedXmllSha256})) {
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
    New-Item -ItemType Directory -Path (Join-Path $actualRoot 'mta/config') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Client/definitive/coreconfig.xml') -Destination (Join-Path $actualRoot 'mta/config/coreconfig.xml')
    $runs += Run-Host 'actual-core-wrong-image' $actualRoot @($actualRoot,'actual')
    $runs += Run-Host 'actual-completed-core-wrong-image' $actualRoot @($actualRoot,'actual-frontend-startup')
    $runs += Run-Host 'actual-completed-clock-core-wrong-image' $actualRoot @($actualRoot,'actual-frontend-startup-v2')
    $runs += Run-Host 'actual-persistent-core-wrong-image' $actualRoot @($actualRoot,'actual-persistent-frontend')
    $runs += Run-Host 'actual-clock-core-wrong-image' $actualRoot @($actualRoot,'actual-clock')
    $runs += Run-Host 'actual-clock-set-core-wrong-image' $actualRoot @($actualRoot,'actual-clock-set')
    $runs += Run-Host 'actual-application-frame-core-wrong-image' $actualRoot @($actualRoot,'actual-application-frame')
    $runs += Run-Host 'actual-runtime-core-wrong-image' $actualRoot @($actualRoot,'actual-runtime')
    $runs += Run-Host 'actual-runtime-local-core-wrong-image' $actualRoot @($actualRoot,'actual-runtime-local')
    foreach ($mode in @('runtime-two-views','runtime-stop-hidden','runtime-stop-showing','runtime-stop-visible','runtime-host-expired',
        'runtime-visible-expired','runtime-focus-hide','runtime-gui-hide','runtime-wrong-api','runtime-refused','runtime-view-fault',
        'runtime-cursor-begin-fault','runtime-cursor-renew-fault','runtime-retained-stop','runtime-shutdown-busy','runtime-cache-contention','runtime-returned-pending','runtime-malformed-first-failed',
        'runtime-active-without-cleanup-debt','runtime-active-already-complete')) {
        $runtimeRoot = Join-Path $outputRoot ($mode + '-host')
        New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null
        Copy-Item -LiteralPath $hostPath -Destination (Join-Path $runtimeRoot 'SanAndreas.exe')
        Copy-Item -LiteralPath $GameDeDll -Destination (Join-Path $runtimeRoot 'game_de.dll')
        if ((Hash (Join-Path $runtimeRoot 'game_de.dll')) -ne $ExpectedGameDeSha256.ToLowerInvariant()) { throw 'Runtime fixture backend hash differs.' }
        $runs += Run-Host $mode $runtimeRoot @($runtimeRoot,$mode,$normalFixture)
    }
    foreach ($mode in @('local-runtime-two-views','local-runtime-suspend-busy','local-runtime-stop-waiting-show','local-runtime-focus-suspended',
        'local-runtime-missing-export','local-runtime-wrong-api','local-runtime-config-refused')) {
        $localRuntimeRoot = Join-Path $outputRoot ($mode + '-host')
        New-Item -ItemType Directory -Path $localRuntimeRoot -Force | Out-Null
        Copy-Item -LiteralPath $hostPath -Destination (Join-Path $localRuntimeRoot 'SanAndreas.exe')
        Copy-Item -LiteralPath $GameDeDll -Destination (Join-Path $localRuntimeRoot 'game_de.dll')
        if ((Hash (Join-Path $localRuntimeRoot 'game_de.dll')) -ne $ExpectedGameDeSha256.ToLowerInvariant()) { throw 'Local runtime fixture backend hash differs.' }
        $selectedLocalFixture = if ($mode -eq 'local-runtime-missing-export') { $missingLocalRuntimeFixture } else { $normalFixture }
        $runs += Run-Host $mode $localRuntimeRoot @($localRuntimeRoot,$mode,$selectedLocalFixture)
    }
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
                        'render-v6-cross-route','render-v6-mixed-version','render-v6-wrong-mask','render-v6-cache-contention',
                        'gui','gui-workload-failed','gui-cancel','gui-wrong-api','gui-config-refused','gui-mixed-version','gui-cache-contention',
                        'gui-deadline','gui-crosswire','gui-cleanup-incomplete','gui-retained','gui-unpinned','gui-device-release-fault','gui-missing-export',
                        'surface','surface-workload-failed','surface-cancel','surface-wrong-api','surface-config-refused','surface-mixed-version','surface-cache-contention',
                        'surface-deadline','surface-crosswire','surface-cleanup-incomplete','surface-retained','surface-unpinned','surface-device-release-fault','surface-missing-export',
                        'surface-child-retained','surface-queue-retained','surface-surface-retained','surface-window-incomplete','surface-composition-incomplete','surface-class-incomplete',
                        'surface-observer-incomplete','surface-gui-incomplete','surface-shutdown-busy','surface-resize-pending',
                        'clock','clock-cancel','clock-wrong-api','clock-config-refused','clock-missing-export','clock-init-failed','clock-init-thrown',
                        'clock-cache-contention','clock-returned-pending','clock-thrown-pending','clock-stale-terminal','clock-retained-terminal','clock-workload-incomplete',
                        'clock-readback-thrown-pending',
                        'clock-set','clock-set-cancel','clock-set-wrong-api','clock-set-config-refused','clock-set-missing-export','clock-set-init-failed','clock-set-init-thrown',
                        'clock-set-cache-contention','clock-set-returned-pending','clock-set-thrown-pending','clock-set-stale-terminal','clock-set-retained-terminal',
                        'clock-set-workload-incomplete','clock-set-readback-thrown-pending','clock-set-corrupt-workload','clock-set-not-entered',
                        'clock-set-reserved-terminal','clock-set-wrong-count',
                        'application-frame','application-frame-cancel','application-frame-wrong-api','application-frame-config-refused',
                        'application-frame-missing-export','application-frame-init-failed','application-frame-init-thrown',
                        'application-frame-cache-contention','application-frame-returned-pending','application-frame-thrown-pending',
                        'application-frame-stale-terminal','application-frame-retained-terminal','application-frame-workload-incomplete',
                        'application-frame-readback-thrown-pending','application-frame-begin-refused','application-frame-not-entered','application-frame-wrong-count',
                        'frontend-clock','frontend-clock-cancel','frontend-clock-wrong-api','frontend-clock-refused',
                        'frontend-clock-workload-incomplete','frontend-clock-short-workload','frontend-clock-owner-corrupt',
                        'frontend-clock-returned-pending','frontend-clock-thrown-pending','frontend-clock-stale-terminal','frontend-clock-retained-terminal',
                        'frontend-clock-cache-contention','frontend-clock-thrown-retired','frontend-clock-missing-export',
                        'frontend-clock-no-publication-busy','frontend-clock-no-publication-failed-unqualified','frontend-clock-no-publication-thrown',
                        'frontend-clock-mismatched-producer','frontend-clock-wrong-clock-count','frontend-clock-prebegin-cancel',
                        'frontend-clock-reserved-terminal','frontend-clock-view-failed','frontend-clock-owner-read-thrown','frontend-clock-binding-missing',
                        'frontend-clock-view-caps-changed')) {
        $movementRoot = Join-Path $outputRoot ($mode + '-host')
        New-Item -ItemType Directory -Path $movementRoot -Force | Out-Null
        Copy-Item -LiteralPath $hostPath -Destination (Join-Path $movementRoot 'SanAndreas.exe')
        # This is the real backend DLL loaded/PINned without invoking its factory
        # or native methods. The controlled Core tests worker lifetime only.
        Copy-Item -LiteralPath $GameDeDll -Destination (Join-Path $movementRoot 'game_de.dll')
        if ((Hash (Join-Path $movementRoot 'game_de.dll')) -ne $ExpectedGameDeSha256.ToLowerInvariant()) { throw 'Movement PIN fixture backend hash differs.' }
        $selectedFixture = if ($mode -eq 'gui-missing-export') { $missingGuiFixture } elseif ($mode -eq 'surface-missing-export') { $missingSurfaceFixture } elseif ($mode -eq 'clock-missing-export') { $missingClockFixture } elseif ($mode -eq 'clock-set-missing-export') { $missingClockSetFixture } elseif ($mode -eq 'application-frame-missing-export') { $missingApplicationFrameFixture } elseif ($mode -eq 'frontend-clock-missing-export') { $missingFrontendClockFixture } else { $normalFixture }
        $runs += Run-Host $mode $movementRoot @($movementRoot,$mode,$selectedFixture)
    }
} catch { $failure = $_.Exception.Message }
$unchanged = $true
foreach ($name in $sourceNames) { if ((Hash (Join-Path $repositoryRoot $name)) -ne $sourceHashes[$name]) { $unchanged=$false } }
foreach ($artifact in $artifacts) { if ((Hash $artifact.source) -ne $artifact.sha256) { $unchanged=$false } }
$record = [ordered]@{
    recorded_at=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=($null -eq $failure -and $unchanged -and $runs.Count -eq 198);
    failure=$failure;source_hashes=$sourceHashes;sources_unchanged=$unchanged;compiles=$compiles;runs=$runs;normal_artifacts=$artifacts;
    historical_cases_expected=65;surface_cases_expected=24;completed_core_actual_cases_expected=1;persistent_core_actual_cases_expected=1;
    clock_controlled_cases_expected=14;clock_actual_preflight_cases_expected=1;historical_total_preserved=91;
    clock_set_controlled_cases_expected=18;clock_set_actual_preflight_cases_expected=1;prior_read_clock_total_preserved=106;
    completed_clock_controlled_cases_expected=25;completed_clock_actual_preflight_cases_expected=1;prior_total_preserved=125;
    runtime_controlled_cases_expected=20;runtime_actual_preflight_cases_expected=1;prior_runtime_total_preserved=151;
    local_runtime_controlled_cases_expected=7;local_runtime_actual_preflight_cases_expected=1;prior_local_runtime_total_preserved=172;
    application_frame_controlled_cases_expected=17;application_frame_actual_preflight_cases_expected=1;prior_application_frame_total_preserved=180;
    fixture_executable_is_game=$false;game_launched=$false;game_memory_read=$false;game_process_attached=$false;game_files_changed=$false;
    synthetic_core_is_mta=$false;full_mta_boot_verified=$false;engine_input_write_verified=$false;
    scope='Actual production async worker, controlled non-MTA loader-lock/read-only-clock/Set/completed-clock-before-view/runtime Show-Hide-Stop/local-input/application-frame fixtures, and normal Core exact-image rejection in owned nonclassic hosts. Application128 covers coherent separate publication, cache-lock contention, cancellation, failed Begin reset, explicit non-entry, missing/refused exports, malformed width/version and pending/exception quarantine while preserving all180 prior cases. Controlled report values do not prove a real native lease or application callback; no time write, GUI or GTA workload is admitted.'
}
$resultPath = Join-Path $outputRoot 'test-result.json'
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
$record | ConvertTo-Json -Depth 10 -Compress
if (-not $record.success) { throw "Core startup worker tests failed: $failure; see $resultPath" }
