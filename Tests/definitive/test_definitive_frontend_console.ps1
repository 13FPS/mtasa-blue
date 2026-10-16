#!/usr/bin/env pwsh
#requires -Version 7.0
# Run the production Surface and explicitly enabled real Core console in owned WARP hosts. No game executable,
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
$outputRoot=Join-Path $taskRoot ('Build/mta-core-frontend-console-host/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
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
$sourceNames=@('Tests/definitive/DefinitiveFrontendConsoleTests.cpp','Tests/definitive/test_definitive_frontend_console.ps1','Client/core/DefinitiveFrontend.h','Client/core/DefinitiveFrontendConsole.h','Client/core/CConsole.cpp','Client/core/CConsole.h','Client/core/CCommands.cpp','Client/core/CCommands.h','Client/core/CEntryHistory.cpp','Client/core/CEntryHistory.h',
    'Tests/definitive/GuiFrontendSurfaceDevice.h','Client/core/DefinitiveGuiSurface.h','Client/core/DefinitiveGuiSurface.cpp','Client/core/GuiWin32Input.h','Client/core/GuiWin32Input.cpp',
    'Client/core/CCore.cpp','Client/core/CCore.h','Client/core/CGUI.cpp','Client/core/CGUI.h','Client/core/CMainMenu.cpp','Client/core/CMainMenu.h',
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
    foreach ($spec in @(@{role='core';receipt=$CoreBuildReceipt;names=@('core.dll','pthread.dll')},@{role='gui';receipt=$GuiBuildReceipt;names=@('cgui.dll')})) {
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
    # A pre-existing disposable history proves the optional console does not
    # silently adopt or overwrite the classic process-persistent input log.
    $historySeed=Join-Path $outputRoot 'console-input-seed.log'
    [IO.File]::WriteAllText($historySeed,"connect legacy.invalid 22003`n",[Text.UTF8Encoding]::new($false))
    $assetSpecs+=@(@{source=$historySeed;path='mta/logs/console-input.log'})
    foreach ($entry in $assetSpecs) { Assert-NoLinks $entry.source }
    foreach ($name in @('tahoma.ttf','tahomabd.ttf','verdana.ttf')) {
        $path=Join-Path $env:WINDIR ('Fonts/'+$name)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required system font absent: $path" }
        $windowsFonts+=@([ordered]@{path=$path;sha256=(Sha $path)})
    }
    foreach ($name in @('dxgi.dll','d3d12.dll','D3D12Core.dll','d3d12warp.dll','D3D12SDKLayers.dll','dcomp.dll','dwmapi.dll')) {
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
    $hostPath=Join-Path $outputRoot 'OwnedMtaFrontendConsole.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DMTA_WITH_DEFINITIVE_ADAPTER',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
        (Join-Path $PSScriptRoot 'DefinitiveFrontendConsoleTests.cpp'),(Join-Path $taskRoot 'Client/core/DefinitiveGuiSurface.cpp'),(Join-Path $taskRoot 'Client/core/GuiWin32Input.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$hostPath),
        '/link','d3d12.lib','dxgi.lib','user32.lib','ole32.lib','dcomp.lib','gdi32.lib')
    [ordered]@{compiler=$compilerRecord;arguments=$arguments} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8
    @(& $compiler @arguments 2>&1) | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
    $compileExit=$LASTEXITCODE
    if ($compileExit -ne 0) { throw 'Owned frontend host compilation failed.' }
    if ((Sha $hostPath) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Disposable host unexpectedly matches the real game.' }
    $testExit=0
    foreach ($case in @('normal','cancel','focus','gated','cpu-busy','partial-start','throw-start','startup-failure','message-failure','message-throw','child-destroy')) {
        $caseRoot=Join-Path $outputRoot ('case-'+$case)
        $assetRoot=Join-Path $caseRoot 'Win64/CLEO/de-online'
        [IO.Directory]::CreateDirectory((Join-Path $assetRoot 'mta/logs')) | Out-Null
        foreach ($entry in $selectedArtifacts) {
            if ($case -eq 'startup-failure' -and $entry.name -eq 'xmll.dll') { continue }
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
        $executable=Join-Path $caseRoot 'Win64/OwnedMtaFrontendConsole.exe'
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
        if ($text -match 'Passed (\d+) actual MTA frontend console host checks') { $count=[int]$Matches[1];$testCount+=$count }
        $success=$exit -eq 0 -and $count -gt 0 -and -not $runTimedOut
        if (-not $success) { $testExit=1 }
        $runs+=@([ordered]@{case=$case;success=$success;exitCode=$exit;checks=$count;timedOut=$runTimedOut;directory=$caseRoot;assetRoot=$assetRoot;
            stdout=$stdout;stdoutSha256=(Sha $stdout);stderr=$stderr;stderrSha256=(Sha $stderr)})
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
    $success=$null -eq $failure -and $compileExit -eq 0 -and $testExit -eq 0 -and $testCount -gt 0 -and $runs.Count -eq 11 -and
        @($runs | Where-Object { -not $_.success }).Count -eq 0 -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged -and
        $assetsUnchanged -and $certificatesUnchanged -and $platformUnchanged
    $receipt=[ordered]@{schema='mta_owned_frontend_console_host_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;
        failure=$failure;compileExit=$compileExit;testExit=$testExit;checks=$testCount;timedOut=$timedOut;cases=$runs;
        sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;sourceSnapshotScope='Fixture plus complete current declared Core and GUI normal-build inputs; a declared superset, not an independently recovered compiler-read trace.';
        normalBuildCertificates=$certificates;certificatesUnchanged=$certificatesUnchanged;artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;
        assets=$assets;assetsUnchanged=$assetsUnchanged;windowsFonts=$windowsFonts;systemDlls=$systemDlls;compiler=$compilerRecord;platformUnchanged=$platformUnchanged;executionArtifacts=$executionArtifacts;
        targetFormat=87;width=1280;height=1024;coreFrontendApi=1;frontendFields=32;consoleApi=1;consoleFields=32;consoleExplicitlyEnabled=$success;automaticLiveConsoleEnabled=$false;
        actualMtaConsoleOnOwnedCompositionSurface=$success;actualWin32ConsoleParserVerified=$success;frontendOnlyCommandRefusalsVerified=$success;inMemoryHistoryAndAutocompleteVerified=$success;
        cpuFrontendTeardownWhileGpuRetainedVerified=$success;startupFailureOwnerCleanupVerified=$success;partialStartCleanupVerified=$success;frontendBusyRetainsBorrowedGuiVerified=$success;focusAndEscapeCleanupVerified=$success;
        configurationCopiesUnchanged=$assetsUnchanged;installedFilesChanged=$false;gameLaunched=$false;gameAttached=$false;engineInputSuppressionVerified=$false;fullMtaReady=$false;
        scope='Real normal-build Core frontend and explicitly enabled MTA console with CGUI on the production Surface owned WARP queue, child HWND and DirectComposition swapchain. Actual widget text/Enter/parser/Help/Clear, refused commands and unchanged CVar revision, in-memory history/autocomplete, Unicode/focus and aggregated menu+console ownership; partial/throw/missing-XML startup, CPU-Busy/GPU-gated detachment, optional-message failure/throw and actual child destruction. No GTA/native control write, engine facade, network startup or full readiness claim.'}
    $receipt | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
}
[ordered]@{success=$success;receipt=(Join-Path $outputRoot 'test-result.json');checks=$testCount;failure=$failure} | ConvertTo-Json -Compress
if (-not $success) { throw "Owned MTA frontend host failed; see $outputRoot" }
