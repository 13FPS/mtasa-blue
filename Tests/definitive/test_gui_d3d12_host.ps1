#!/usr/bin/env pwsh
#requires -Version 7.0
# The actual GUI uses only this disposable WARP device and staged MTA assets.
# No installed files, real game process, or engine-owned GPU objects are used.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GuiDll,
    [Parameter(Mandatory=$true)][string]$CoreDll,
    [Parameter(Mandatory=$true)][string]$PthreadDll,
    [string]$Unifont='Build/gui-asset-cache/unifont-16.0.04.ttf'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture requires native x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-gui-d3d12-host/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $outputRoot 'MTA/logs') -Force | Out-Null
$sourcePaths=@('Tests/definitive/GuiD3D12HostTests.cpp','Tests/definitive/test_gui_d3d12_host.ps1',
    'Client/gui/D3D12GuiRenderer.h','Client/gui/D3D12GuiRenderer.cpp','Client/gui/Main.cpp','Client/gui/CGUI_Impl.h','Client/gui/CGUI_Impl.cpp',
    'Client/gui/CGUITexture_Impl.cpp','Client/gui/CGUIStaticImage_Impl.cpp','Client/gui/CGUIFont_Impl.cpp','Client/gui/premake5.lua',
    'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SString.h','utils/buildactions/install_unifont.lua',
    'vendor/cegui-0.4.0-custom/include/CEGUIRenderer.h','vendor/cegui-0.4.0-custom/include/CEGUITexture.h',
    'vendor/cegui-0.4.0-custom/src/CEGUISystem.cpp','vendor/cegui-0.4.0-custom/src/CEGUIFont.cpp')
$sourcePaths+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName) })
$sourceHashes=@($sourcePaths | Sort-Object -Unique | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $_)).Hash.ToLowerInvariant()} })
$artifacts=@();$assets=@();$fonts=@()
foreach ($entry in @(@{name='cgui.dll';source=$GuiDll},@{name='core.dll';source=$CoreDll},@{name='pthread.dll';source=$PthreadDll})) {
    $source=(Resolve-Path -LiteralPath $entry.source).Path
    $staged=Join-Path $outputRoot ('MTA/'+$entry.name)
    Copy-Item -LiteralPath $source -Destination $staged
    $artifacts+=@([ordered]@{source=$source;staged=$staged;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()})
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
# Fail before the export if required actual fonts are absent. The fixture's
# intentional font-failure case only renames its own staged Unifont afterward.
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
$executable=Join-Path $outputRoot 'SanAndreas.exe'
$arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
    (Join-Path $PSScriptRoot 'GuiD3D12HostTests.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),
    '/link','d3d12.lib','dxgi.lib','user32.lib','ole32.lib')
$compileOutput=@(& $compiler @arguments 2>&1);$compileExit=$LASTEXITCODE
$compileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit=$null;$testCount=$null;$timedOut=$false;$testText=''
if ($compileExit -eq 0) {
    $process=Start-Process -FilePath $executable -ArgumentList ('"'+$outputRoot+'"') -WorkingDirectory $outputRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $outputRoot 'stdout.log') -RedirectStandardError (Join-Path $outputRoot 'stderr.log')
    # This exact owned Process is the only process this fixture may terminate.
    if (-not $process.WaitForExit(45000)) { $timedOut=$true;$process.Kill();$process.WaitForExit() }
    $testExit=$process.ExitCode;$process.Dispose()
    $testText=(@(Get-Content -LiteralPath (Join-Path $outputRoot 'stdout.log'))+@(Get-Content -LiteralPath (Join-Path $outputRoot 'stderr.log'))) -join "`n"
    if ($testText -match 'Passed (\d+) actual D3D12 GUI host checks') { $testCount=[int]$Matches[1] }
}
$sourceUnchanged=$true;$artifactsUnchanged=$true;$assetsUnchanged=$true
foreach ($entry in $sourceHashes) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $entry.path)).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged=$false } }
foreach ($entry in $artifacts) {
    foreach ($path in @($entry.source,$entry.staged)) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256) { $artifactsUnchanged=$false } }
}
foreach ($entry in $assets) {
    foreach ($path in @($entry.source,$entry.staged)) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256) { $assetsUnchanged=$false } }
}
$images=@(Get-ChildItem -LiteralPath $outputRoot -File -Filter 'd3d12-*.bmp' | ForEach-Object { [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()} })
$success=$compileExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and -not $timedOut -and $sourceUnchanged -and $artifactsUnchanged -and $assetsUnchanged
$receipt=[ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;success=$success;checks=$testCount;compileExit=$compileExit;testExit=$testExit;timedOut=$timedOut;
    sourceHashes=$sourceHashes;sourceSnapshotScope='Fixture, GUI ABI, renderer and selected CEGUI/widget sources; not the complete normal compiler dependency trace';sourceUnchanged=$sourceUnchanged;
    artifacts=$artifacts;artifactsUnchanged=$artifactsUnchanged;assets=$assets;assetsUnchanged=$assetsUnchanged;windowsFonts=$fonts;images=$images;
    actualMtaWidgetsRenderedOnOwnedD3D12Warp=$success;debugLayerAvailable=$testText.Contains('D3D12 debug layer available=1');
    installedFilesChanged=$false;gameLaunched=$false;gameAttached=$false;coreInitialized=$false;deDrawingVerified=$false;fullMtaReady=$false;
    scope='Actual MTA CGUI/CEGUI widgets, fonts and textures on an owned offscreen WARP device/queue; no engine target, real game device, or visible GTA drawing.'}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
[pscustomobject]@{directory=$outputRoot;success=$success;checks=$testCount;compileExit=$compileExit;testExit=$testExit;sourceUnchanged=$sourceUnchanged;artifactsUnchanged=$artifactsUnchanged;assetsUnchanged=$assetsUnchanged} | ConvertTo-Json -Compress
if (-not $success) { throw "Actual owned D3D12 GUI fixture failed; see $outputRoot" }
