#!/usr/bin/env pwsh
#requires -Version 7.0
# The actual GUI uses only this disposable WARP swapchain and staged MTA assets.
# No installed files, real game process, or engine-owned GPU objects are used.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GuiBuildReceipt,
    [Parameter(Mandatory=$true)][string]$CoreBuildReceipt,
    [string]$Unifont='Build/gui-asset-cache/unifont-16.0.04.ttf'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture requires native x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-gui-d3d12-swapchain-host/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $outputRoot 'MTA/logs') -Force | Out-Null
$sourcePaths=@('Tests/definitive/GuiD3D12SwapchainHostTests.cpp','Tests/definitive/test_gui_d3d12_swapchain_host.ps1',
    'Client/gui/D3D12GuiRenderer.h','Client/gui/D3D12GuiRenderer.cpp','Client/gui/Main.cpp','Client/gui/CGUI_Impl.h','Client/gui/CGUI_Impl.cpp',
    'Client/gui/CGUITexture_Impl.cpp','Client/gui/CGUIStaticImage_Impl.cpp','Client/gui/CGUIFont_Impl.cpp','Client/gui/premake5.lua',
    'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SString.h','utils/buildactions/install_unifont.lua',
    'vendor/cegui-0.4.0-custom/include/CEGUIRenderer.h','vendor/cegui-0.4.0-custom/include/CEGUITexture.h',
    'vendor/cegui-0.4.0-custom/src/CEGUISystem.cpp','vendor/cegui-0.4.0-custom/src/CEGUIFont.cpp')
$sourcePaths+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName) })
# The DLLs must come from immutable normal-build artifacts, rather than whichever
# files happen to occupy Bin. The GUI's complete declared inputs remain current.
# Core is only loaded for SetMTADirectory; its saved certificate is historical
# artifact provenance, not a current startup/consumer source certificate.
$certificates=@();$selectedArtifacts=@()
foreach ($spec in @(@{role='gui';receipt=$GuiBuildReceipt;names=@('cgui.dll')},@{role='coreAssetResolver';receipt=$CoreBuildReceipt;names=@('core.dll','pthread.dll')})) {
    $certificatePath=(Resolve-Path -LiteralPath $spec.receipt).Path
    $certificate=Get-Content -LiteralPath $certificatePath -Raw | ConvertFrom-Json
    if ($certificate.success -ne $true) { throw "Normal build certificate is not green: $certificatePath" }
    if ($spec.role -eq 'gui') {
        if ($certificate.sources_unchanged -ne $true) { throw "GUI build source certificate is not green: $certificatePath" }
        foreach ($property in $certificate.source_hashes.PSObject.Properties) {
            $sourcePath=Join-Path $taskRoot $property.Name
            if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash.ToLowerInvariant() -ne $property.Value) { throw "Normal GUI build source changed: $($property.Name)" }
            $sourcePaths+=@($property.Name)
        }
    }
    foreach ($name in $spec.names) {
        $matches=@($certificate.artifacts | Where-Object { [IO.Path]::GetFileName($_.saved) -eq $name })
        if ($matches.Count -ne 1) { throw "Normal certificate must identify exactly one $name" }
        $artifact=$matches[0]
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.saved).Hash.ToLowerInvariant() -ne $artifact.sha256 -or (Get-Item -LiteralPath $artifact.saved).Length -ne $artifact.bytes) { throw "Saved normal artifact differs: $name" }
        $selectedArtifacts+=@([ordered]@{name=$name;source=$artifact.saved;sha256=$artifact.sha256;bytes=$artifact.bytes;certificate=$certificatePath})
    }
    $certificates+=@([ordered]@{role=$spec.role;path=$certificatePath;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $certificatePath).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $certificatePath).Length;declaredSourceCount=@($certificate.source_hashes.PSObject.Properties).Count;sourceBindingScope=if($spec.role -eq 'gui'){'Current declared GUI normal-build inputs rehashed before/after fixture'}else{'Historical saved Core/pthread artifact provenance only; old Core source hashes are opaque and excluded from current union'}})
}
$sourceHashes=@($sourcePaths | Sort-Object -Unique | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $_)).Hash.ToLowerInvariant()} })
$artifacts=@();$assets=@();$fonts=@()
foreach ($entry in $selectedArtifacts) {
    $source=(Resolve-Path -LiteralPath $entry.source).Path
    $staged=Join-Path $outputRoot ('MTA/'+$entry.name)
    Copy-Item -LiteralPath $source -Destination $staged
    $artifacts+=@([ordered]@{source=$source;staged=$staged;sha256=$entry.sha256;bytes=$entry.bytes;certificate=$entry.certificate})
}
$fontRoot=Join-Path $taskRoot 'Shared/data/MTA San Andreas/MTA/cgui'
$fontTarget=Join-Path $outputRoot 'MTA/cgui'
New-Item -ItemType Directory -Path $fontTarget | Out-Null
foreach ($name in @('sans.ttf','saheader.ttf','sagothic.ttf','Imageset.xsd','GUIScheme.xsd','Font.xsd','Falagard.xsd')) {
    $source=Join-Path $fontRoot $name;$staged=Join-Path $fontTarget $name
    Copy-Item -LiteralPath $source -Destination $staged
    $assets+=@([ordered]@{source=$source;staged=$staged;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()})
}
$unifontSource=(Resolve-Path -LiteralPath $Unifont).Path
$unifontHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $unifontSource).Hash.ToLowerInvariant()
if ($unifontHash -ne '6b18fdcf0f61f3df2e61db9737af6de9f37048ec93083621d41504ebf8cb18f4') { throw 'Official MTA Unifont release pin differs.' }
$unifontTarget=Join-Path $fontTarget 'unifont.ttf'
Copy-Item -LiteralPath $unifontSource -Destination $unifontTarget
$assets+=@([ordered]@{source=$unifontSource;staged=$unifontTarget;sha256=$unifontHash;provenance='Existing utils/buildactions/install_unifont.lua official release pin'})
$skinTarget=Join-Path $outputRoot 'skins/Default'
New-Item -ItemType Directory -Path $skinTarget -Force | Out-Null
foreach ($name in @('CGUI.xml','CGUI.is.xml','CGUI.lnf.xml','CGUI.png')) {
    $source=Join-Path (Join-Path $taskRoot 'Shared/data/MTA San Andreas/skins/Default') $name;$staged=Join-Path $skinTarget $name
    Copy-Item -LiteralPath $source -Destination $staged
    $assets+=@([ordered]@{source=$source;staged=$staged;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()})
}
# Fail before loading the actual GUI if its required system fonts are absent.
foreach ($name in @('tahoma.ttf','tahomabd.ttf','verdana.ttf')) {
    $source=Join-Path $env:WINDIR ('Fonts/'+$name)
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required Windows font is absent: $name" }
    $fonts+=@([ordered]@{path=$source;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()})
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'MSVC x64 tools are required.' }
$developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
if ($LASTEXITCODE -ne 0) { throw 'MSVC environment setup failed.' }
foreach ($entry in $compilerEnvironment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$compilerRecord=[ordered]@{path=$compiler;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $compiler).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $compiler).Length}
$systemDlls=@()
foreach ($name in @('dxgi.dll','d3d12.dll','D3D12Core.dll','d3d12warp.dll','D3D12SDKLayers.dll')) {
    $path=Join-Path $env:WINDIR ('System32/'+$name)
    if (Test-Path -LiteralPath $path -PathType Leaf) { $systemDlls+=@([ordered]@{path=$path;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $path).Length}) }
}
$executable=Join-Path $outputRoot 'OwnedGuiSwapchain.exe'
$arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
    (Join-Path $PSScriptRoot 'GuiD3D12SwapchainHostTests.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),
    '/link','d3d12.lib','dxgi.lib','user32.lib','ole32.lib')
$compileCommand=Join-Path $outputRoot 'compile-command.json'
[ordered]@{compiler=$compilerRecord;arguments=$arguments} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $compileCommand -Encoding utf8
$compileOutput=@(& $compiler @arguments 2>&1);$compileExit=$LASTEXITCODE
$compileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit=$null;$testCount=0;$timedOut=$false;$testText='';$runs=@()
if ($compileExit -eq 0) {
    $testExit=0
    # Each format gets a fresh process and staged asset tree. CEGUI's global
    # session, lifetime negatives and readback artifacts must be exercised for
    # the actual layout rather than inherited from a previous format's run.
    foreach ($format in @(24,87,28)) {
        $formatRoot=Join-Path $outputRoot ('format-'+$format)
        $assetRoot=Join-Path $formatRoot 'Win64/CLEO/de-online'
        New-Item -ItemType Directory -Path (Join-Path $assetRoot 'MTA/logs') -Force | Out-Null
        foreach ($entry in @($artifacts | Where-Object { [IO.Path]::GetDirectoryName($_.staged) -eq (Join-Path $outputRoot 'MTA') })) {
            $staged=Join-Path $assetRoot ([IO.Path]::GetFileName($entry.staged))
            New-Item -ItemType Directory -Path (Split-Path $staged) -Force | Out-Null
            Copy-Item -LiteralPath $entry.staged -Destination $staged
            $artifacts+=@([ordered]@{source=$entry.source;staged=$staged;sha256=$entry.sha256;targetFormat=$format})
        }
        foreach ($entry in @($assets | Where-Object { -not $_.Contains('targetFormat') })) {
            $staged=Join-Path $assetRoot ([IO.Path]::GetRelativePath($outputRoot,$entry.staged))
            New-Item -ItemType Directory -Path (Split-Path $staged) -Force | Out-Null
            Copy-Item -LiteralPath $entry.staged -Destination $staged
            $assets+=@([ordered]@{source=$entry.source;staged=$staged;sha256=$entry.sha256;targetFormat=$format})
        }
        # Actual diagnostic Core/cgui adjacency has no MTA/core.dll parent to
        # discover. Keep the EXE above CLEO exactly as the Steam layout does.
        $fixtureExecutable=Join-Path $formatRoot 'Win64/OwnedGuiSwapchain.exe'
        Copy-Item -LiteralPath $executable -Destination $fixtureExecutable
        $artifacts+=@([ordered]@{source=$executable;staged=$fixtureExecutable;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash.ToLowerInvariant();targetFormat=$format})
        $stdout=Join-Path $formatRoot 'stdout.log';$stderr=Join-Path $formatRoot 'stderr.log'
        $process=Start-Process -FilePath $fixtureExecutable -ArgumentList ('"'+$assetRoot+'" '+$format) -WorkingDirectory (Join-Path $formatRoot 'Win64') -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        # This exact disposable Process is the only process this fixture may terminate.
        $runTimedOut=-not $process.WaitForExit(45000)
        if ($runTimedOut) { $timedOut=$true;$process.Kill();$process.WaitForExit() }
        $runExit=$process.ExitCode;$process.Dispose()
        $runText=(@(Get-Content -LiteralPath $stdout)+@(Get-Content -LiteralPath $stderr)) -join "`n"
        $runChecks=$null
        if ($runText -match 'Passed (\d+) actual D3D12 swapchain GUI host checks') { $runChecks=[int]$Matches[1];$testCount+=$runChecks }
        $presentResults=@([regex]::Matches($runText,'Owned Present HRESULT=([0-9a-fA-F]{8}) beforeIndex=(\d+) afterIndex=(\d+)') | ForEach-Object { [ordered]@{hresult=$_.Groups[1].Value.ToLowerInvariant();beforeIndex=[int]$_.Groups[2].Value;afterIndex=[int]$_.Groups[3].Value} })
        $summary=[regex]::Match($runText,'Owned swapchain buffers=3 observedCurrentBuffers=(\d+) presents=(\d+) nativeResizes=(\d+) deviceRefs=(\d+)')
        $summaryValid=$summary.Success -and [int]$summary.Groups[1].Value -ge 1 -and [int]$summary.Groups[1].Value -le 3 -and [int]$summary.Groups[2].Value -eq 4 -and [int]$summary.Groups[3].Value -eq 2 -and [int]$summary.Groups[4].Value -eq 0
        $imageCount=@(Get-ChildItem -LiteralPath $assetRoot -File -Filter 'swapchain-*.bmp').Count
        $rawCount=@(Get-ChildItem -LiteralPath $assetRoot -File -Filter 'swapchain-*.raw').Count
        $runSuccess=$runExit -eq 0 -and $null -ne $runChecks -and -not $runTimedOut -and $runText.Contains('Swapchain GUI target format='+$format) -and $summaryValid -and $presentResults.Count -eq 4 -and $imageCount -eq 4 -and $rawCount -eq 4
        if (-not $runSuccess -and $testExit -eq 0) { $testExit=if ($runExit -ne 0) {$runExit} else {1} }
        $runs+=@([ordered]@{targetFormat=$format;directory=$formatRoot;assetRoot=$assetRoot;fixtureExecutable=$fixtureExecutable;success=$runSuccess;checks=$runChecks;testExit=$runExit;timedOut=$runTimedOut;
            stdout=$stdout;stdoutSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $stdout).Hash.ToLowerInvariant();
            stderr=$stderr;stderrSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $stderr).Hash.ToLowerInvariant();
            presentResults=$presentResults;observedCurrentBuffers=if($summary.Success){[int]$summary.Groups[1].Value}else{$null};realBufferCount=3;nativeResizeCalls=if($summary.Success){[int]$summary.Groups[3].Value}else{$null};finalDeviceReferences=if($summary.Success){[int]$summary.Groups[4].Value}else{$null};
            debugLayerAvailable=$runText.Contains('D3D12 debug layer available=1');nativePixelEncoding=if($format -eq 24){'R10G10B10A2_UNORM packed little-endian uint32'}elseif($format -eq 28){'R8G8B8A8_UNORM'}else{'B8G8R8A8_UNORM'}})
        $testText+=$runText+"`n"
    }
}
$sourceUnchanged=$true;$artifactsUnchanged=$true;$assetsUnchanged=$true;$certificatesUnchanged=$true;$platformUnchanged=$true
foreach ($entry in $sourceHashes) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $entry.path)).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged=$false } }
foreach ($entry in $artifacts) {
    foreach ($path in @($entry.source,$entry.staged)) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256) { $artifactsUnchanged=$false } }
}
foreach ($entry in $assets) {
    foreach ($path in @($entry.source,$entry.staged)) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256) { $assetsUnchanged=$false } }
}
foreach ($entry in $certificates) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $entry.path).Hash.ToLowerInvariant() -ne $entry.sha256) { $certificatesUnchanged=$false } }
foreach ($entry in @($fonts)+@($systemDlls)+@($compilerRecord)) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $entry.path).Hash.ToLowerInvariant() -ne $entry.sha256) { $platformUnchanged=$false } }
$images=@(Get-ChildItem -LiteralPath $outputRoot -Recurse -File -Filter 'swapchain-*.bmp' | ForEach-Object { [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant();pixelEncoding='normalized B8G8R8A8_UNORM for BI_RGB preview only'} })
$nativeReadbacks=@(Get-ChildItem -LiteralPath $outputRoot -Recurse -File -Filter 'swapchain-*.raw' | ForEach-Object { [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant();layout='tightly packed rows, four bytes per pixel in the format named by the format directory'} })
$executionArtifacts=@(Get-ChildItem -LiteralPath $outputRoot -File | Where-Object { $_.Extension -in @('.exe','.obj','.log','.json') } | ForEach-Object { [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()} })
$success=$compileExit -eq 0 -and $testExit -eq 0 -and $testCount -gt 0 -and $runs.Count -eq 3 -and @($runs | Where-Object { -not $_.success }).Count -eq 0 -and $images.Count -eq 12 -and $nativeReadbacks.Count -eq 12 -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged -and $assetsUnchanged -and $certificatesUnchanged -and $platformUnchanged
$receipt=[ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;checks=$testCount;compileExit=$compileExit;testExit=$testExit;timedOut=$timedOut;
    sourceHashes=$sourceHashes;sourceSnapshotScope='Fixture and selected inputs plus exact complete declared GUI normal-build certificate inputs; declared superset, not an independently recovered compiler-read trace. Historical uninitialized Core asset-resolver certificate is excluded from current source union.';sourceUnchanged=$sourceUnchanged;
    normalBuildCertificates=$certificates;certificatesUnchanged=$certificatesUnchanged;compiler=$compilerRecord;systemDlls=$systemDlls;platformUnchanged=$platformUnchanged;executionArtifacts=$executionArtifacts;
    artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;assets=$assets;assetsUnchanged=$assetsUnchanged;windowsFonts=$fonts;images=$images;nativeReadbacks=$nativeReadbacks;
    targetFormats=@(24,87,28);formatRuns=$runs;artifactNormalization='Only BMP copies are normalized to BGRA8; numerical assertions and .raw artifacts use untouched native target precision.';
    explicitGuiAssetRootVerified=$success;fixtureLayout='Win64/OwnedGuiSwapchain.exe with adjacent Core/cgui in Win64/CLEO/de-online and explicit assets below that adapter root; no MTA/core.dll fallback';
    actualMtaWidgetsRenderedOnOwnedD3D12WarpSwapchain=$success;canonicalQueueDeviceBackbufferIdentity=$success;presentStateTransitionsAndExactForward=$success;busyRetainsOwnerModuleAndTargets=$success;guiDrainBeforeNativeResize=$success;resizeAndRecreationRendered=$success;wrongOwnerRejected=$success;stopDrainsWithoutNextFrame=$success;zeroFinalDeviceReferences=$success;debugLayerAvailable=$testText.Contains('D3D12 debug layer available=1');
    installedFilesChanged=$false;gameLaunched=$false;gameAttached=$false;coreInitialized=$false;deDrawingVerified=$false;fullMtaReady=$false;
    scope='Actual MTA CGUI/CEGUI widgets, fonts and textures on private real three-buffer hidden HWND WARP swapchains for format24/87/28, with owner-controlled queue gating, native resize and teardown. Actual hidden Present HRESULTs/indices are recorded without claiming all indices rotate under occlusion. No engine hook/target, real game device, color-space/HDR claim, or visible GTA drawing.'}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
[pscustomobject]@{directory=$outputRoot;success=$success;checks=$testCount;compileExit=$compileExit;testExit=$testExit;sourceUnchanged=$sourceUnchanged;artifactsUnchanged=$artifactsUnchanged;assetsUnchanged=$assetsUnchanged} | ConvertTo-Json -Compress
if (-not $success) { throw "Actual owned D3D12 GUI fixture failed; see $outputRoot" }
