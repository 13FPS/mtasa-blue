#!/usr/bin/env pwsh
#requires -Version 7.0
# Run completed Core/backend owners with their actual borrowed frontend in fresh WARP hosts. No real game,
# installed file, engine device, native hook or network endpoint is used.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreBuildReceipt,
    [Parameter(Mandatory)][string]$GuiBuildReceipt,
    [Parameter(Mandatory)][string]$XmllDll,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedXmllSha256,
    [string]$Unifont='Build/gui-asset-cache/unifont-16.0.04.ttf'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'The owned frontend requires native x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-core-frontend-startup-host/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
function Sha([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
function Assert-NoLinks([string]$Path) {
    $candidate=[IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing linked fixture input: $candidate"
        }
        $candidate=[IO.Path]::GetDirectoryName($candidate)
    }
}
$sourceNames=@('Tests/definitive/CoreFrontendStartupHostTests.cpp','Tests/definitive/ObservedFrontendGui.h','Tests/definitive/test_core_frontend_startup_host.ps1','Client/core/DefinitiveFrontend.h','Client/core/DefinitiveFrontendStartup.h','Client/core/DefinitiveFrontendRuntime.h',
    'Client/core/DefinitiveLocalInputRuntime.h','Client/sdk/game/CGameLocalInputLease.h','Client/game_de/LocalInputLeasePolicy.h',
    'Client/core/DefinitivePersistentFrontend.h','Client/core/DefinitiveFrontendConsole.h','Client/core/DefinitiveFrontendBindings.h','Client/core/DefinitiveFrontendClockReference.h','Client/core/DefinitiveClockReference.h','Client/core/DefinitiveClockReference.cpp',
    'Client/sdk/game/CGameClockService.h','Client/core/DefinitiveFrontendStartup.cpp','Client/core/Core.cpp','Client/core/CGameAdapterModule.cpp','Client/core/CGameAdapterModule.h',
    'Client/core/CCore.cpp','Client/core/CCore.h','Client/core/CGUI.cpp','Client/core/CGUI.h','Client/core/CMainMenu.cpp','Client/core/CMainMenu.h',
    'Client/core/CSettings.cpp','Client/core/CSettings.h','Client/sdk/core/CKeyBindsInterface.h','vendor/ksignals/ksignals.h',
    'Client/core/CCredits.cpp','Client/core/CQuestionBox.cpp','Client/core/CNewsBrowser.cpp','Client/core/CLocalization.cpp','Client/core/CClientVariables.cpp',
    'Client/core/CVersionUpdater.cpp','Client/core/CVersionUpdater.h','Client/core/ServerBrowser/CServerCache.cpp','Client/core/ServerBrowser/CServerCache.h',
    'Client/definitive/coreconfig.xml','Client/gui/Main.cpp','Client/gui/CGUI_Impl.cpp','Client/gui/D3D12GuiRenderer.cpp')
$sourceNames+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName) })
$certificates=@();$selectedArtifacts=@();$sourceHashes=@();$artifacts=@();$assets=@();$windowsFonts=@();$systemDlls=@();$runs=@()
$compileExit=$null;$testExit=$null;$testCount=0;$timedOut=$false;$failure=$null;$compilerRecord=$null
$sourceUnchanged=$true;$artifactsUnchanged=$true;$assetsUnchanged=$true;$certificatesUnchanged=$true;$platformUnchanged=$true
try {
    # Both DLLs are normal build artifacts with complete current declared input
    # certificates. This rejects an old GUI/consumer pairing before execution.
    foreach ($spec in @(@{role='core';receipt=$CoreBuildReceipt;names=@('core.dll','pthread.dll','game_de.dll')},@{role='gui';receipt=$GuiBuildReceipt;names=@('cgui.dll')})) {
        $path=(Resolve-Path -LiteralPath $spec.receipt).Path
        Assert-NoLinks $path
        $certificate=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        foreach ($required in @('success','source_hashes','artifacts','guards','default_projects_restored')) {
            if ($null -eq $certificate.PSObject.Properties[$required]) { throw "Normal $($spec.role) certificate is missing $required`: $path" }
        }
        if ($certificate.success -ne $true -or $certificate.default_projects_restored -ne $true -or
            @($certificate.guards.PSObject.Properties).Count -eq 0 -or
            @($certificate.guards.PSObject.Properties | Where-Object { $_.Value -ne $true }).Count -ne 0) {
            throw "Normal build certificate is not green/restored: $path"
        }
        $declaredSources=@($certificate.source_hashes.PSObject.Properties)
        if ($declaredSources.Count -eq 0) { throw "Normal certificate has no declared source hashes: $path" }
        # The GUI receipt records sources_unchanged explicitly. The Core receipt
        # instead succeeds only after its before/after source checks and publishes
        # the final source_hashes/source_count. Rehash both declared sets here;
        # an absent Core-only boolean must not bypass source or artifact binding.
        if ($spec.role -eq 'gui') {
            if ($null -eq $certificate.PSObject.Properties['sources_unchanged'] -or $certificate.sources_unchanged -ne $true) {
                throw "Normal GUI certificate reports changed or unavailable source verification: $path"
            }
        } elseif ($null -eq $certificate.PSObject.Properties['source_count'] -or $certificate.source_count -ne $declaredSources.Count) {
            throw "Normal Core certificate source count differs from its declared hashes: $path"
        }
        foreach ($entry in $declaredSources) {
            if ($entry.Value -isnot [string] -or $entry.Value -notmatch '^[0-9a-f]{64}$') { throw "Invalid normal source hash: $($entry.Name)" }
            $source=Join-Path $taskRoot $entry.Name
            Assert-NoLinks $source
            if ((Sha $source) -ne $entry.Value) { throw "Normal certificate source changed: $($entry.Name)" }
            $sourceNames+=@($entry.Name)
        }
        foreach ($name in $spec.names) {
            $matches=@($certificate.artifacts | Where-Object { [IO.Path]::GetFileName($_.saved) -eq $name })
            if ($matches.Count -ne 1) { throw "Normal certificate must name exactly one $name" }
            $artifact=$matches[0]
            Assert-NoLinks $artifact.saved
            if ((Sha $artifact.saved) -ne $artifact.sha256 -or (Get-Item -LiteralPath $artifact.saved).Length -ne $artifact.bytes) { throw "Saved normal artifact differs: $name" }
            $selectedArtifacts+=@([ordered]@{name=$name;source=$artifact.saved;sha256=$artifact.sha256;bytes=$artifact.bytes;certificate=$path})
        }
        $certificates+=@([ordered]@{role=$spec.role;path=$path;sha256=(Sha $path);declaredSourceCount=$declaredSources.Count;
            sourceVerification=if($spec.role -eq 'gui'){'Explicit sources_unchanged plus every declared source rehashed'}else{'Successful Core before/after certificate, exact declared source_count, plus every final declared source rehashed'}})
    }
    $xml=(Resolve-Path -LiteralPath $XmllDll).Path
    Assert-NoLinks $xml
    if ((Sha $xml) -ne $ExpectedXmllSha256.ToLowerInvariant()) { throw 'Explicit normal XML module pin differs.' }
    $selectedArtifacts+=@([ordered]@{name='xmll.dll';source=$xml;sha256=(Sha $xml);bytes=(Get-Item -LiteralPath $xml).Length;certificate='Caller-supplied immutable normal XML artifact SHA256'})
    $sourceHashes=@($sourceNames | ForEach-Object { $_.Replace('\','/') } | Sort-Object -Unique | ForEach-Object {
        $path=Join-Path $taskRoot $_
        Assert-NoLinks $path
        [ordered]@{path=$_;sha256=(Sha $path)}
    })
    $assetSpecs=@()
    foreach ($name in @('sans.ttf','saheader.ttf','sagothic.ttf','Imageset.xsd','GUIScheme.xsd','Font.xsd','Falagard.xsd')) {
        $assetSpecs+=@(@{source=(Join-Path $taskRoot ('Shared/data/MTA San Andreas/MTA/cgui/'+$name));path=('mta/cgui/'+$name)})
    }
    $unifontSource=(Resolve-Path -LiteralPath $Unifont).Path
    if ((Sha $unifontSource) -ne '6b18fdcf0f61f3df2e61db9737af6de9f37048ec93083621d41504ebf8cb18f4') { throw 'Official MTA Unifont pin differs.' }
    $assetSpecs+=@(@{source=$unifontSource;path='mta/cgui/unifont.ttf'})
    foreach ($name in @('CGUI.xml','CGUI.is.xml','CGUI.lnf.xml','CGUI.png')) {
        $assetSpecs+=@(@{source=(Join-Path $taskRoot ('Shared/data/MTA San Andreas/skins/Default/'+$name));path=('skins/Default/'+$name)})
    }
    # Keep actual upstream image paths. English client.pot supplies the dictionary
    # filename used by localization to locate every menu image in this directory.
    foreach ($name in @('background.png','background_logo.png','mta_filler.png','version.png','the_language_icon.png')) {
        $assetSpecs+=@(@{source=(Join-Path $taskRoot ('Shared/data/MTA San Andreas/MTA/cgui/images/'+$name));path=('mta/cgui/images/'+$name)})
    }
    foreach ($name in @('locked.png','search-servers.png','search-players.png','refresh.png','favorite.png','connect.png','info.png','search.png','trashcan.png')) {
        $assetSpecs+=@(@{source=(Join-Path $taskRoot ('Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/'+$name));path=('mta/cgui/images/serverbrowser/'+$name)})
    }
    foreach ($name in @('menu_disconnect.png','menu_quick_connect.png','menu_browse_servers.png','menu_settings.png','menu_about.png','menu_quit.png','latest_news.png','client.pot')) {
        $assetSpecs+=@(@{source=(Join-Path $taskRoot ('Shared/data/MTA San Andreas/MTA/locale/en_US/'+$name));path=('mta/locale/en_US/'+$name)})
    }
    $assetSpecs+=@(@{source=(Join-Path $taskRoot 'Client/definitive/coreconfig.xml');path='mta/config/coreconfig.xml'})
    foreach ($entry in $assetSpecs) { Assert-NoLinks $entry.source }
    foreach ($name in @('tahoma.ttf','tahomabd.ttf','verdana.ttf')) {
        $path=Join-Path $env:WINDIR ('Fonts/'+$name)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required system font absent: $path" }
        $windowsFonts+=@([ordered]@{path=$path;sha256=(Sha $path)})
    }
    foreach ($name in @('dxgi.dll','d3d12.dll','D3D12Core.dll','d3d12warp.dll','D3D12SDKLayers.dll')) {
        $path=Join-Path $env:WINDIR ('System32/'+$name)
        if (Test-Path -LiteralPath $path -PathType Leaf) { $systemDlls+=@([ordered]@{path=$path;sha256=(Sha $path)}) }
    }
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'MSVC x64 tools are required.' }
    $dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment setup failed.' }
    foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $compilerRecord=[ordered]@{path=$compiler;sha256=(Sha $compiler)}
    $hostPath=Join-Path $outputRoot 'OwnedMtaFrontendStartup.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'vendor')),
        (Join-Path $PSScriptRoot 'CoreFrontendStartupHostTests.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$hostPath),
        '/link','d3d12.lib','dxgi.lib','user32.lib','ole32.lib')
    [ordered]@{compiler=$compilerRecord;arguments=$arguments} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8
    @(& $compiler @arguments 2>&1) | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
    $compileExit=$LASTEXITCODE
    if ($compileExit -ne 0) { throw 'Owned frontend host compilation failed.' }
    if ((Sha $hostPath) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Disposable host unexpectedly matches the real game.' }
    $testExit=0
    foreach ($case in @('normal','gated','frontend-failure','backend-failure','runtime','runtime-frontend-failure','runtime-backend-failure')) {
        $caseRoot=Join-Path $outputRoot ('case-'+$case)
        $assetRoot=Join-Path $caseRoot 'Win64/CLEO/de-online'
        [IO.Directory]::CreateDirectory((Join-Path $assetRoot 'mta/logs')) | Out-Null
        foreach ($entry in $selectedArtifacts) {
            if (($case -in @('frontend-failure','runtime-frontend-failure') -and $entry.name -eq 'xmll.dll') -or
                ($case -in @('backend-failure','runtime-backend-failure') -and $entry.name -eq 'game_de.dll')) { continue }
            $staged=Join-Path $assetRoot $entry.name
            Copy-Item -LiteralPath $entry.source -Destination $staged
            $artifacts+=@([ordered]@{source=$entry.source;staged=$staged;sha256=$entry.sha256;bytes=$entry.bytes;certificate=$entry.certificate;case=$case})
        }
        foreach ($entry in $assetSpecs) {
            $staged=Join-Path $assetRoot $entry.path
            [IO.Directory]::CreateDirectory((Split-Path -Path $staged -Parent)) | Out-Null
            Copy-Item -LiteralPath $entry.source -Destination $staged
            $assets+=@([ordered]@{source=$entry.source;staged=$staged;sha256=(Sha $entry.source);case=$case})
        }
        # The owned executable has only the filename used by Core's admission gate.
        # Its bytes remain this compiled WARP host, never a real GTA executable.
        $executable=Join-Path $caseRoot 'Win64/SanAndreas.exe'
        Copy-Item -LiteralPath $hostPath -Destination $executable
        $artifacts+=@([ordered]@{source=$hostPath;staged=$executable;sha256=(Sha $hostPath);bytes=(Get-Item -LiteralPath $hostPath).Length;case=$case})
        $stdout=Join-Path $caseRoot 'stdout.log';$stderr=Join-Path $caseRoot 'stderr.log'
        $process=Start-Process -FilePath $executable -ArgumentList ('"'+$assetRoot+'" '+$case) -WorkingDirectory (Split-Path -Path $executable -Parent) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        # Only this exact newly launched disposable process can be terminated.
        $runTimedOut=-not $process.WaitForExit(45000)
        if ($runTimedOut) { $timedOut=$true;$process.Kill();$process.WaitForExit() }
        $exit=$process.ExitCode;$process.Dispose()
        $text=(@(Get-Content -LiteralPath $stdout)+@(Get-Content -LiteralPath $stderr)) -join "`n"
        $count=0
        if ($text -match 'Passed (\d+) actual MTA frontend startup host checks') { $count=[int]$Matches[1];$testCount+=$count }
        $success=$exit -eq 0 -and $count -gt 0 -and -not $runTimedOut
        $resultPath=Join-Path $assetRoot 'frontend-startup-result.json'
        $hostResult=$null
        if ($success) {
            # Retained runtime proof comes from the actual two-view host. A
            # successful legacy GUI case cannot stand in for that ownership.
            $hostResult=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json -DateKind String
            $runtimeCase=$case.StartsWith('runtime',[StringComparison]::Ordinal)
            if ($hostResult.success -ne $true -or $hostResult.checks -ne $count -or
                $hostResult.completedOwnerClockApiVersion -ne 1 -or $hostResult.completedOwnerClockFields -ne 96 -or
                $hostResult.completedOwnerClockAdmissionVerified -ne (-not $runtimeCase) -or $hostResult.completedOwnerClockNativeWorkloadVerified -ne $false -or
                $hostResult.completedOwnerClockPublishedSamples -ne 0 -or
                $hostResult.completedOwnerClockWrongImageRefusalVerified -ne (-not $runtimeCase -and $case -ne 'backend-failure') -or
                $hostResult.runtimeApiVersion -ne 1 -or $hostResult.runtimeFields -ne 64 -or $hostResult.runtimeCase -ne $runtimeCase -or
                $hostResult.runtimeRetentionVerified -ne ($case -eq 'runtime') -or
                $hostResult.runtimeFailureCleanupVerified -ne ($runtimeCase -and $case -ne 'runtime') -or
                $hostResult.runtimeNativeViewVerified -ne $false -or @($hostResult.runtimeTerminal).Count -ne 64 -or
                $hostResult.localInputApiVersion -ne 1 -or $hostResult.localInputFields -ne 32 -or
                $hostResult.localInputAdmissionVerified -ne $runtimeCase -or $hostResult.localInputWrongImageVerified -ne ($case -eq 'runtime') -or
                $hostResult.localInputNativeMovementVerified -ne $false -or
                @($hostResult.startupTerminal).Count -ne 32 -or @($hostResult.frontendTerminal).Count -ne 32) {
                throw "Actual host clock/unchanged frontend report differs: $case"
            }
            if ($runtimeCase) {
                $runtime=@($hostResult.runtimeTerminal)
                if ($runtime[32] -ne 1 -or $runtime[33] -ne 5 -or $runtime[34] -eq 0 -or
                    ($runtime[35] -eq 0 -and $runtime[36] -eq 0) -or $runtime[55] -ne 1 -or $runtime[57] -ne 1 -or
                    @(@(38,39,40,41,42,50,56,59,60,61,62,63) | Where-Object { $runtime[$_] -ne 0 }).Count -ne 0 -or
                    $runtime[1] -ne 4 -or $runtime[3] -ne 1 -or $runtime[5] -ne 0) {
                    throw "Actual runtime service/backend retirement differs: $case"
                }
                if ($case -eq 'runtime' -and ($runtime[43] -ne 1 -or $runtime[44] -ne 1 -or $runtime[45] -ne 1 -or
                    $runtime[46] -ne 2 -or $runtime[47] -ne 2 -or $runtime[48] -ne 2 -or $runtime[49] -ne 0 -or
                    $runtime[51] -ne 1 -or $hostResult.actualSessionBindingsSettingsVerified -ne $true -or
                    $hostResult.settingsCaptureRetainedForCleanup -ne $true)) {
                    throw 'Actual runtime two-view configuration/bindings retention differs.'
                }
            } elseif (@($hostResult.runtimeTerminal | Where-Object { $_ -ne 0 }).Count -ne 0) {
                throw "Historical startup case unexpectedly populated runtime state: $case"
            }
        }
        if (-not $success) { $testExit=1 }
        $runs+=@([ordered]@{case=$case;success=$success;exitCode=$exit;checks=$count;timedOut=$runTimedOut;directory=$caseRoot;assetRoot=$assetRoot;
            stdout=$stdout;stdoutSha256=(Sha $stdout);stderr=$stderr;stderrSha256=(Sha $stderr);
            actualHostResult=$hostResult;actualHostResultPath=$(if($hostResult){$resultPath}else{$null});actualHostResultSha256=$(if($hostResult){Sha $resultPath}else{$null})})
    }
} catch {
    $failure=$_.Exception.ToString()
} finally {
    foreach ($entry in $sourceHashes) { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $sourceUnchanged=$false } }
    foreach ($entry in $artifacts) { foreach ($path in @($entry.source,$entry.staged)) { if ((Sha $path) -ne $entry.sha256) { $artifactsUnchanged=$false } } }
    foreach ($entry in $assets) { foreach ($path in @($entry.source,$entry.staged)) { if ((Sha $path) -ne $entry.sha256) { $assetsUnchanged=$false } } }
    foreach ($entry in $certificates) { if ((Sha $entry.path) -ne $entry.sha256) { $certificatesUnchanged=$false } }
    foreach ($entry in @($windowsFonts)+@($systemDlls)+@($compilerRecord | Where-Object { $null -ne $_ })) { if ((Sha $entry.path) -ne $entry.sha256) { $platformUnchanged=$false } }
    $executionArtifacts=@(Get-ChildItem -LiteralPath $outputRoot -Recurse -File | Where-Object { $_.Extension -in @('.exe','.obj','.log','.json','.bmp') } | ForEach-Object {
        [ordered]@{path=$_.FullName;sha256=(Sha $_.FullName);bytes=$_.Length}
    })
    $success=$null -eq $failure -and $compileExit -eq 0 -and $testExit -eq 0 -and $testCount -gt 0 -and $runs.Count -eq 7 -and
        @($runs | Where-Object { -not $_.success }).Count -eq 0 -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged -and
        $assetsUnchanged -and $certificatesUnchanged -and $platformUnchanged
    $receipt=[ordered]@{schema='mta_owned_frontend_startup_host_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;
        failure=$failure;compileExit=$compileExit;testExit=$testExit;checks=$testCount;timedOut=$timedOut;cases=$runs;
        sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;sourceSnapshotScope='Fixture plus complete current declared Core and GUI normal-build inputs; a declared superset, not an independently recovered compiler-read trace.';
        normalBuildCertificates=$certificates;certificatesUnchanged=$certificatesUnchanged;artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;
        assets=$assets;assetsUnchanged=$assetsUnchanged;windowsFonts=$windowsFonts;systemDlls=$systemDlls;compiler=$compilerRecord;platformUnchanged=$platformUnchanged;executionArtifacts=$executionArtifacts;
        targetFormat=87;width=1280;height=1024;coreFrontendApi=1;frontendFields=32;frontendStartupApi=1;frontendStartupFields=32;persistentFrontendApi=1;persistentFrontendFields=288;
        persistentRequestAdmissionVerified=$success;persistentWrongImageRefusalVerified=$success;persistentForeignThreadRefusalVerified=$success;persistentNativeLeaseStarted=$false;persistentVisibleRenderingVerified=$false;actualCompletedCoreBackendAndBorrowedFrontend=$success;actualMtaMenuCreditsAndConsoleOnOwnedWarp=$success;
        completedOwnerClockApiVersion=1;completedOwnerClockFields=96;completedOwnerClockArgumentAdmissionVerified=$success;completedOwnerClockForeignThreadRefusalVerified=$success;
        completedOwnerClockGuiBorrowRefusalVerified=$success;completedOwnerClockStoppedBorrowRefusalVerified=$success;completedOwnerClockWrongImageRefusalVerified=$success;
        completedOwnerClockNativeWorkloadVerified=$false;completedOwnerClockFollowingCpuGuiReuseVerified=$success;frontendStartup32Preserved=$success;
        frontendRuntimeApiVersion=1;frontendRuntimeFields=64;frontendRuntimeCases=3;
        actualTwoViewRuntimeConfigurationRetentionVerified=($success -and @($runs | Where-Object { $_.case -eq 'runtime' -and $_.actualHostResult.runtimeRetentionVerified -eq $true }).Count -eq 1);
        actualRuntimePartialFailureCleanupVerified=($success -and @($runs | Where-Object { $_.case -in @('runtime-frontend-failure','runtime-backend-failure') -and $_.actualHostResult.runtimeFailureCleanupVerified -eq $true }).Count -eq 2);
        frontendRuntimeNativeViewVerified=$false;
        localInputRuntimeApiVersion=1;localInputRuntimeFields=32;
        actualLocalInputOwnerArgumentAndThreadAdmissionVerified=($success -and @($runs | Where-Object { $_.case.StartsWith('runtime') -and $_.actualHostResult.localInputAdmissionVerified -eq $true }).Count -eq 3);
        actualLocalInputWrongImageRefusalAndFollowingGuiReuseVerified=($success -and @($runs | Where-Object { $_.case -eq 'runtime' -and $_.actualHostResult.localInputWrongImageVerified -eq $true }).Count -eq 1);
        localInputNativeMovementVerified=$false;
        actualMtaSessionBindingsSettingsOnOwnedWarp=$success;actualSixMovementBindingsApplyAndCancelVerified=$success;reservedConsoleKeyCaptureAndFocusLossVerified=$success;modalCaptureRetiredDuringFrontendTeardownVerified=$success;
        cpuFrontendTeardownWhileGpuRetainedVerified=$success;startupFailureOwnerCleanupVerified=$success;normalFrontendReacquireVerified=$success;completedCoreRetainedAfterFrontendReleaseVerified=$success;backendFailureOwnerCleanupVerified=$success;
        configurationCopiesUnchanged=$assetsUnchanged;installedFilesChanged=$false;gameLaunched=$false;gameAttached=$false;engineInputSuppressionVerified=$false;fullMtaReady=$false;
        scope='Completed real Core and actual Game DE lifecycle, borrowed actual MTA menu/credits/console/configured bindings and existing session-only settings on a private WARP target. All four historical cases remain; three additive Runtime64 cases prove one actual Core/backend and one XML/CVars/CKeyBinds owner across two freshly borrowed GUI views, Apply I then hide/release then identical real bind record and Cancel O, neutral hidden input, unchanged disk configuration, stopped-unreleased borrower exclusion and final manager/CVars/XML/backend retirement. XML/backend preparation failures retain inspectable owners until explicit cleanup. Fixture GUI observation forwards actual widgets/callbacks. Includes completed-owner Clock API1/Read96 admission and actual wrong-image refusal in historical routes, while Runtime refuses clock and all legacy mutating owner routes. Startup32 and Persistent288 are unchanged. No positive native clock or runtime-view workload is claimed. Disposable SanAndreas.exe is this hashed owned host, not GTA. No engine facade stubs, native engine binding, game/device attachment, native hooks, full multiplayer startup or visible GTA claim.'}
    $receipt | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
}
[ordered]@{success=$success;receipt=(Join-Path $outputRoot 'test-result.json');checks=$testCount;failure=$failure} | ConvertTo-Json -Compress
if (-not $success) { throw "Owned MTA frontend startup host failed; see $outputRoot" }
