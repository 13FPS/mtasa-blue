#!/usr/bin/env pwsh
#requires -Version 7.0
# Build and run the real GUI ABI in an isolated owned host. It never installs
# files, launches/attaches GTA, initializes Core or borrows a game device.
[CmdletBinding()]
param(
    [string]$GuiDll,
    [string]$CoreDll,
    [string]$PthreadDll,
    [string]$Unifont = 'Build/gui-asset-cache/unifont-16.0.04.ttf',
    [switch]$CallbacksOnly,
    [string]$LegacyCallbackHeader
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This GUI fixture requires a native x64 PowerShell process.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-gui-x64-host/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourcePaths=@('Tests/definitive/GuiX64HostTests.cpp','Tests/definitive/test_gui_x64_host.ps1',
    'Shared/sdk/SString.h','Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SharedUtil.IntTypes.h','Shared/sdk/CVector2D.h','Shared/sdk/CVector.h','Shared/sdk/CVector4D.h','Shared/sdk/CRect2D.h',
    'Client/gui/Main.cpp','Client/gui/CGUI_Impl.cpp','Client/gui/CGUI_Impl.h','Client/gui/CGUIFont_Impl.cpp',
    'Shared/sdk/SharedUtil.Misc.h','Shared/sdk/SharedUtil.Misc.hpp','utils/buildactions/install_unifont.lua')
$sourcePaths+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName).Replace('\','/') })
$sourceHashes=@($sourcePaths | Sort-Object -Unique | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant()} })
$artifacts=@(); $assets=@(); $externalFonts=@(); $overrideInclude=$null
if ($LegacyCallbackHeader) {
    if (-not $CallbacksOnly) { throw 'The legacy header is allowed only for an owned callback-only causal fixture.' }
    $oldHeader=(Resolve-Path -LiteralPath $LegacyCallbackHeader).Path
    $overrideInclude=Join-Path $outputRoot 'legacy-sdk'
    New-Item -ItemType Directory -Path $overrideInclude | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -Destination $overrideInclude -Recurse
    Copy-Item -LiteralPath $oldHeader -Destination (Join-Path $overrideInclude 'gui/CGUICallback.h') -Force
    $artifacts+=@([ordered]@{kind='causalOldHeader';source=$oldHeader;sha256=(Get-FileHash -LiteralPath $oldHeader -Algorithm SHA256).Hash.ToLowerInvariant()})
}
if (-not $CallbacksOnly) {
    New-Item -ItemType Directory -Path (Join-Path $outputRoot 'MTA/logs') -Force | Out-Null
    foreach ($entry in @(@{name='cgui.dll';path=$GuiDll},@{name='core.dll';path=$CoreDll},@{name='pthread.dll';path=$PthreadDll})) {
        if (-not $entry.path) { throw 'Full actual GUI testing requires explicit GUI/Core/pthread DLLs.' }
        $source=(Resolve-Path -LiteralPath $entry.path).Path
        $destination=Join-Path $outputRoot ('MTA/'+$entry.name)
        Copy-Item -LiteralPath $source -Destination $destination
        $artifacts+=@([ordered]@{kind='actualModule';source=$source;staged=$destination;sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()})
    }
    # These exact MTA assets must exist before the export is called, because
    # the shipping error path may display BrowseToSolution on a missing font.
    $fontRoot=Join-Path $taskRoot 'Shared/data/MTA San Andreas/MTA/cgui'
    $fontDestination=Join-Path $outputRoot 'MTA/cgui'
    New-Item -ItemType Directory -Path $fontDestination -Force | Out-Null
    foreach ($name in @('sans.ttf','saheader.ttf','sagothic.ttf','Imageset.xsd','GUIScheme.xsd','Font.xsd','Falagard.xsd')) {
        $source=Join-Path $fontRoot $name
        Copy-Item -LiteralPath $source -Destination (Join-Path $fontDestination $name)
        $assets+=@([ordered]@{source=$source;staged=('MTA/cgui/'+$name);sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()})
    }
    $unifontPath=(Resolve-Path -LiteralPath $Unifont).Path
    $unifontHash=(Get-FileHash -LiteralPath $unifontPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($unifontHash -ne '6b18fdcf0f61f3df2e61db9737af6de9f37048ec93083621d41504ebf8cb18f4') { throw 'The existing MTA Unifont release pin does not match.' }
    Copy-Item -LiteralPath $unifontPath -Destination (Join-Path $fontDestination 'unifont.ttf')
    $assets+=@([ordered]@{source=$unifontPath;staged='MTA/cgui/unifont.ttf';sha256=$unifontHash;provenance='utils/buildactions/install_unifont.lua; official MTA v16.0.04 release pin'})
    $skinSource=Join-Path $taskRoot 'Shared/data/MTA San Andreas/skins/Default'
    New-Item -ItemType Directory -Path (Join-Path $outputRoot 'skins/Default') -Force | Out-Null
    foreach ($name in @('CGUI.xml','CGUI.is.xml','CGUI.lnf.xml','CGUI.png')) {
        $source=Join-Path $skinSource $name
        Copy-Item -LiteralPath $source -Destination (Join-Path $outputRoot ('skins/Default/'+$name))
        $assets+=@([ordered]@{source=$source;staged=('skins/Default/'+$name);sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()})
    }
    foreach ($name in @('tahoma.ttf','tahomabd.ttf','verdana.ttf')) {
        $source=Join-Path $env:WINDIR ('Fonts/'+$name)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'A required real Windows font is unavailable; no GUI constructor was called.' }
        $externalFonts+=@([ordered]@{source=$source;sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()})
    }
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 C++ tools are required.' }
$developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $compilerEnvironment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$executable=Join-Path $outputRoot 'SanAndreas.exe'
$arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN')
if ($overrideInclude) { $arguments+=('/I'+$overrideInclude) }
$arguments+=@(('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
    (Join-Path $PSScriptRoot 'GuiX64HostTests.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','d3d9.lib','user32.lib','advapi32.lib')
$compileOutput=@(& $compiler @arguments 2>&1); $compileExit=$LASTEXITCODE
$compileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit=$null; $testCount=$null; $timedOut=$false; $testText=''
if ($compileExit -eq 0) {
    $argument=if ($CallbacksOnly) { '--callbacks-only' } else { '"'+$outputRoot+'"' }
    $process=Start-Process -FilePath $executable -ArgumentList $argument -WorkingDirectory $outputRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $outputRoot 'stdout.log') -RedirectStandardError (Join-Path $outputRoot 'stderr.log')
    # Retain this exact owned Process object. Never enumerate or terminate a
    # user game by the deliberately shared classifier basename.
    if (-not $process.WaitForExit(45000)) { $timedOut=$true; $process.Kill(); $process.WaitForExit() }
    $testExit=$process.ExitCode; $process.Dispose()
    $testText=(@(Get-Content -LiteralPath (Join-Path $outputRoot 'stdout.log'))+@(Get-Content -LiteralPath (Join-Path $outputRoot 'stderr.log'))) -join "`n"
    if ($testText -match 'Passed (\d+) actual x64 GUI (host|callback) checks') { $testCount=[int]$Matches[1] }
}
$sourceUnchanged=$true
foreach ($entry in $sourceHashes) { if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged=$false } }
$artifactsUnchanged=$true
foreach ($entry in $artifacts) {
    if ((Get-FileHash -LiteralPath $entry.source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $artifactsUnchanged=$false }
    if ($entry.Contains('staged') -and (Get-FileHash -LiteralPath $entry.staged -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $artifactsUnchanged=$false }
}
$images=@(Get-ChildItem -LiteralPath $outputRoot -Filter 'gui-*.bmp' -File | ForEach-Object { [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
$success=($compileExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged)
$causalRed=($CallbacksOnly -and $LegacyCallbackHeader -and $compileExit -eq 0 -and $testExit -eq 1 -and $testText.Contains('complex method callback preserves adjacent storage'))
$receipt=[ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;causalLegacyCallbackRed=$causalRed;
    architecture='x64';compileExit=$compileExit;testExit=$testExit;checks=$testCount;timedOut=$timedOut;sourceHashes=$sourceHashes;sourceSnapshotScope='Fixture, public GUI ABI headers and selected production constructor/font/root sources; not a full normal-module compiler dependency trace';
    sourceUnchanged=$sourceUnchanged;artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;assets=$assets;windowsFonts=$externalFonts;images=$images;
    actualPrivateD3D9GuiRendered=($success -and -not $CallbacksOnly);callbackOnly=$CallbacksOnly.IsPresent;gameLaunched=$false;gameAttached=$false;installedFilesChanged=$false;coreInitialized=$false;deRendererVerified=$false;fullMtaReady=$false;
    scope='Actual GUI DLL/API and private owned D3D9 rendering only; no engine, DE swapchain, gameplay facade, network, or visible GTA drawing.'}
$receipt | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
$receipt | ConvertTo-Json -Depth 9 -Compress
if (-not $success -and -not $causalRed) { throw "Owned GUI checks failed; see $outputRoot" }
