#!/usr/bin/env pwsh
#requires -Version 7.0
# One actual retained-Core movement console route on an owned WARP host; no GTA or native engine invocation.
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
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-core-frontend-moveanim-host/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/movecore-'+$id)
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$savedTemp=$env:TEMP;$savedTmp=$env:TMP
$env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
$scratchRemoved=$false;$hostCopiesRemoved=$false;$ownedHostCopies=@();$disposedHostCopies=@()
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
$sourceNames=@('Tests/definitive/CoreFrontendStartupHostTests.cpp','Tests/definitive/CoreFrontendBindingPersistenceHost.h','Tests/definitive/ObservedFrontendGui.h','Tests/definitive/test_core_frontend_startup_host.ps1','Client/core/DefinitiveFrontend.h','Client/core/DefinitiveFrontendStartup.h','Client/core/DefinitiveFrontendRuntime.h',
    'Client/core/DefinitiveLocalInputRuntime.h','Client/sdk/game/CGameLocalInputLease.h','Client/game_de/LocalInputLeasePolicy.h',
    'Client/core/DefinitiveStartupGameTime.h','Client/sdk/game/CGameTimeService.h','Client/game_de/NativeGameTimeReader.h','Client/game_de/NativeGameTimeReader.cpp','Client/game_de/NativeClockMemory.h',
    'Client/core/DefinitivePersistentFrontend.h','Client/core/DefinitiveFrontendConsole.h','Client/core/DefinitiveFrontendBindings.h','Client/core/DefinitiveFrontendBindingPersistence.h','Client/core/DefinitiveFrontendClockReference.h','Client/core/DefinitiveClockReference.h','Client/core/DefinitiveClockReference.cpp',
    'Client/sdk/game/CGameClockService.h','Client/core/DefinitiveFrontendStartup.cpp','Client/core/Core.cpp','Client/core/CGameAdapterModule.cpp','Client/core/CGameAdapterModule.h',
    'Client/core/CCore.cpp','Client/core/CCore.h','Client/core/CGUI.cpp','Client/core/CGUI.h','Client/core/CMainMenu.cpp','Client/core/CMainMenu.h',
    'Client/core/CSettings.cpp','Client/core/CSettings.h','Client/sdk/core/CKeyBindsInterface.h','Client/sdk/core/CLocalizationInterface.h','vendor/ksignals/ksignals.h',
    'Client/core/CCredits.cpp','Client/core/CQuestionBox.cpp','Client/core/CNewsBrowser.cpp','Client/core/CLocalization.cpp','Client/core/CClientVariables.cpp',
    'Client/core/CVersionUpdater.cpp','Client/core/CVersionUpdater.h','Client/core/ServerBrowser/CServerCache.cpp','Client/core/ServerBrowser/CServerCache.h',
    'Client/definitive/coreconfig.xml','Client/gui/Main.cpp','Client/gui/CGUI_Impl.cpp','Client/gui/D3D12GuiRenderer.cpp')
$sourceNames+=@('Client/sdk/game/CGamePresentationPolicy.h','Client/game_de/NativePresentationPolicyOwner.h','Client/game_de/NativePresentationPolicyOwner.cpp',
    'Client/game_de/NativePresentationPolicyAccess.h','Client/game_de/CGamePresentationPolicyDE.h')
$sourceNames+=@('Tests/definitive/CoreFrontendMoveAnimHostTests.cpp','Tests/definitive/test_core_frontend_move_anim_host.ps1',
    'Client/core/DefinitiveMoveAnimWorkload.h','Client/sdk/game/CGameMoveAnimWorkload.h','Client/core/CCommands.cpp','Client/core/CCommandFuncs.cpp')
$sourceNames+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName) })
$certificates=@();$selectedArtifacts=@();$sourceHashes=@();$artifacts=@();$assets=@();$windowsFonts=@();$systemDlls=@();$runs=@()
$compileExit=$null;$testExit=$null;$testCount=0;$timedOut=$false;$failure=$null;$compilerRecord=$null
$sourceUnchanged=$true;$artifactsUnchanged=$true;$assetsUnchanged=$true;$certificatesUnchanged=$true;$platformUnchanged=$true
$caseNames=@('runtime-moveanim')
try {
    # Both DLLs are normal build artifacts with complete current declared input
    # certificates. This rejects an old GUI/consumer pairing before execution.
    foreach ($spec in @(@{role='core';receipt=$CoreBuildReceipt;names=@('core.dll','pthread.dll','game_de.dll')},@{role='gui';receipt=$GuiBuildReceipt;names=@('cgui.dll')})) {
        $path=(Resolve-Path -LiteralPath $spec.receipt).Path
        Assert-NoLinks $path
        $certificate=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if($spec.role -eq 'core' -and ($certificate.schema -ne 'mta_live_moveanim_fresh_core_backend_pair_v1' -or
            $null -eq $certificate.PSObject.Properties['coreBuild'] -or $null -eq $certificate.PSObject.Properties['backendBuild'])) {
            throw 'Use the final fresh50 Core/backend pair receipt, not an intermediate or historical Core receipt.'
        }
        if($spec.role -eq 'core') {
            foreach($name in @('coreBuild','backendBuild')) {
                $linked=$certificate.$name
                if($linked.path -isnot [string] -or $linked.sha256 -isnot [string] -or $linked.sha256 -notmatch '^[0-9a-f]{64}$') {
                    throw "Final pair has no exact $name receipt identity."
                }
                $linkedPath=if([IO.Path]::IsPathRooted($linked.path)){$linked.path}else{Join-Path $taskRoot $linked.path}
                Assert-NoLinks $linkedPath
                $linkedPath=(Resolve-Path -LiteralPath $linkedPath).Path
                if((Sha $linkedPath) -ne $linked.sha256){throw "Final pair $name receipt hash differs."}
                $certificates+=@([ordered]@{role=$name;path=$linkedPath;sha256=$linked.sha256})
            }
        }

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
    $hostPath=Join-Path $outputRoot 'OwnedMtaFrontendMoveAnim.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'vendor')),
        (Join-Path $PSScriptRoot 'CoreFrontendMoveAnimHostTests.cpp'),('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$hostPath),
        '/link','d3d12.lib','dxgi.lib','user32.lib','ole32.lib')
    [ordered]@{compiler=$compilerRecord;arguments=$arguments} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8
    @(& $compiler @arguments 2>&1) | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
    $compileExit=$LASTEXITCODE
    if ($compileExit -ne 0) { throw 'Owned frontend host compilation failed.' }
    if ((Sha $hostPath) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Disposable host unexpectedly matches the real game.' }
    $testExit=0
    foreach ($case in $caseNames) {
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
            if ($case -eq 'runtime-locale-failure' -and $entry.path -eq 'mta/locale/en_US/client.pot') { continue }
            $staged=Join-Path $assetRoot $entry.path
            [IO.Directory]::CreateDirectory((Split-Path -Path $staged -Parent)) | Out-Null
            $assetSource=$entry.source
            if($case -eq 'runtime-persistence' -and $entry.path -eq 'mta/config/coreconfig.xml') {
                # Unrelated real binds make loss during movement-only cloning
                # observable without modifying the pinned repository asset.
                $assetSource=Join-Path $caseRoot 'persistence-baseconfig.xml'
                $fixture=(Get-Content -LiteralPath $entry.source -Raw).Replace('</binds>',"    <bind key=`"x`" control=`"fire`" />`n        <bind key=`"y`" command=`"host_preserve`" arguments=`"fixture`" state=`"down`" />`n    </binds>")
                [IO.File]::WriteAllText($assetSource,$fixture,[Text.UTF8Encoding]::new($false))
            }
            if($case -in @('runtime-interaction','runtime-locale-failure') -and $entry.path -eq 'mta/config/coreconfig.xml') {
                # A real configured but unshipped locale must use MTA's English
                # fallback. Removing that actual dictionary tests shared startup
                # rollback before any GUI, logger or command borrower exists.
                $assetSource=Join-Path $caseRoot 'interaction-baseconfig.xml'
                $fixture=(Get-Content -LiteralPath $entry.source -Raw).Replace('</mainconfig>',"    <settings><locale>de_DE</locale></settings>`n</mainconfig>")
                [IO.File]::WriteAllText($assetSource,$fixture,[Text.UTF8Encoding]::new($false))
            }
            Copy-Item -LiteralPath $assetSource -Destination $staged
            $assets+=@([ordered]@{source=$assetSource;staged=$staged;sha256=(Sha $assetSource);case=$case})
        }
        # The owned executable has only the filename used by Core's admission gate.
        # Its bytes remain this compiled WARP host, never a real GTA executable.
        $executable=Join-Path $caseRoot 'Win64/SanAndreas.exe'
        Copy-Item -LiteralPath $hostPath -Destination $executable
        $ownedHostCopies+=@([ordered]@{path=$executable;sha256=(Sha $hostPath)})
        $artifacts+=@([ordered]@{source=$hostPath;staged=$executable;sha256=(Sha $hostPath);bytes=(Get-Item -LiteralPath $hostPath).Length;case=$case;disposableOwnedHost=$true})
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
            $hostResult=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json -DateKind String
            if($hostResult.schema -ne 'mta_actual_core_move_anim_host_v1' -or $hostResult.success -ne $true -or $hostResult.checks -ne $count -or
                $hostResult.actualCoreMoveAnimConsoleRouteTested -ne $true -or $hostResult.actualCoreMoveAnimDeferredAdmissionTested -ne $true -or
                $hostResult.actualCoreMoveAnimWrongImageRetirementTested -ne $true -or $hostResult.actualCoreMoveAnimReadAdmissionTested -ne $true -or
                $hostResult.clockFields -ne 96 -or $hostResult.nativeEngineCodeExecuted -ne $false -or $hostResult.nativeMovementVerified -ne $false -or
                $hostResult.physicalConfirmationRequired -ne $true -or $hostResult.fullMtaReady -ne $false -or $hostResult.gameLaunched -ne $false) {
                throw 'Actual new Core movement-route report differs.'
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
    try {
        # Only the single compiled host copy is disposable. Verify its exact
        # absolute target/hash after process exit; keep the compiled source EXE
        # and all logs/certificates as durable evidence.
        $base=[IO.Path]::GetFullPath($outputRoot).TrimEnd('\')+'\'
        foreach($copy in $ownedHostCopies) {
            Assert-NoLinks $copy.path
            $path=[IO.Path]::GetFullPath($copy.path)
            if(-not $path.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or
                [IO.Path]::GetFileName($path) -ne 'SanAndreas.exe' -or
                ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or (Sha $path) -ne $copy.sha256) {
                throw 'Disposable host path/hash escaped its owned run.'
            }
            Remove-Item -LiteralPath $path -Force
            $disposedHostCopies+=@([ordered]@{path=$path;sha256BeforeRemoval=$copy.sha256;removed=(-not(Test-Path -LiteralPath $path))})
        }
        $hostCopiesRemoved=$ownedHostCopies.Count -eq 1 -and @($disposedHostCopies|Where-Object{-not $_.removed}).Count -eq 0
        $resolved=(Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\')+'\'
        $parent=(Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Compiler scratch escaped Build/tmp.'}
        $entries=@(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach($entry in $entries) {
            if($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                -not(Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolved,[StringComparison]::OrdinalIgnoreCase)) {throw 'Unexpected scratch content preserved.'}
        }
        foreach($entry in $entries){Remove-Item -LiteralPath $entry.FullName -Force}
        [IO.Directory]::Delete($scratchRoot,$false);$scratchRemoved=$true
    } catch {if($failure){$failure+=[Environment]::NewLine+$_.Exception.ToString()}else{$failure=$_.Exception.ToString()}}
    $env:TEMP=$savedTemp;$env:TMP=$savedTmp
    $executionArtifacts=@(Get-ChildItem -LiteralPath $outputRoot -Recurse -File | Where-Object { $_.Extension -in @('.exe','.log','.json') } | ForEach-Object {
        [ordered]@{path=$_.FullName;sha256=(Sha $_.FullName);bytes=$_.Length}
    })
    $success=$null -eq $failure -and $compileExit -eq 0 -and $testExit -eq 0 -and $testCount -gt 0 -and $runs.Count -eq 1 -and
        @($runs|Where-Object{-not $_.success}).Count -eq 0 -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged -and
        $assetsUnchanged -and $certificatesUnchanged -and $platformUnchanged -and $scratchRemoved -and $hostCopiesRemoved
    $receipt=[ordered]@{schema='mta_actual_core_move_anim_host_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;
        selectedCases=$caseNames;failure=$failure;compileExit=$compileExit;testExit=$testExit;checks=$testCount;timedOut=$timedOut;cases=$runs;
        sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;normalBuildCertificates=$certificates;certificatesUnchanged=$certificatesUnchanged;
        artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;assets=$assets;assetsUnchanged=$assetsUnchanged;windowsFonts=$windowsFonts;systemDlls=$systemDlls;
        compiler=$compilerRecord;platformUnchanged=$platformUnchanged;executionArtifacts=$executionArtifacts;
        scratchDirectory=$scratchRoot;scratchRemoved=$scratchRemoved;ownedHostCopiesRemoved=$hostCopiesRemoved;disposedHostCopies=$disposedHostCopies;
        actualCoreMoveAnimConsoleRouteTested=$success;actualCoreMoveAnimDeferredAdmissionTested=$success;
        actualCoreMoveAnimWrongImageRetirementTested=$success;actualCoreMoveAnimReadAdmissionTested=$success;
        nativeEngineCodeExecuted=$false;nativeMovementVerified=$false;physicalConfirmationRequired=$true;clockFields=96;
        fullMtaReady=$false;gameLaunched=$false;gameAttached=$false;installedFilesChanged=$false;
        scope='One newly rebuilt real retained-Core console/Runtime/adapter route on owned WARP. Existing helper definitions compile but historical main/matrices do not execute. testmoveanim queues in the real console, waits through visible and stopped-unreleased GUI ownership, then safely refuses the actual non-game image after view retirement. Cached report version/handle/thread/stale reads preserve output; final Runtime/adapter retirement precedes Core release. No successful native movement or physical gameplay claim. One compiled host copy and task compiler scratch are removed after verification.'}
    $receipt | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
}
[ordered]@{success=$success;receipt=(Join-Path $outputRoot 'test-result.json');checks=$testCount;failure=$failure} | ConvertTo-Json -Compress
if (-not $success) { throw "Actual Core movement host failed; see $outputRoot" }
