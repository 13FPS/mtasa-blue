#!/usr/bin/env pwsh
#requires -Version 7.0
# Relink the actual normal Core objects with test-only exports, then exercise
# configured controls through the real Game DE consumer in an owned host.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][string]$PthreadDll,
    [Parameter(Mandatory)][string]$XmlDll,
    [string]$CoreObjectDirectory,
    [string]$OldKeyBindsObject,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedGameDeSha256,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedXmlSha256,
    [switch]$ExpectMissingApi
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture requires native x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $CoreObjectDirectory) { $CoreObjectDirectory=Join-Path $taskRoot 'Build/obj/x64/Release/Client Core' }
$objectRoot=(Resolve-Path -LiteralPath $CoreObjectDirectory).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-configured-input/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames=@('Tests/definitive/MtaConfiguredInputTests.cpp','Tests/definitive/MtaConfiguredInputHost.cpp',
    'Tests/definitive/test_mta_configured_input.ps1','Client/core/CKeyBinds.h','Client/core/CKeyBinds.cpp',
    'Client/core/CGameAdapterModule.h','Client/core/CGameAdapterModule.cpp','Client/sdk/core/CKeyBindsInterface.h',
    'Client/sdk/game/CGameAdapter.h','Shared/sdk/CControlStateSynthesis.h','Shared/sdk/CControlBindState.h','Shared/sdk/CControllerState.h')
$sourceNames+=@('Shared/sdk/xml/CXML.h','Shared/sdk/xml/CXMLFile.h','Shared/XML/XML.cpp','Shared/XML/CXMLImpl.cpp','Shared/XML/CXMLFileImpl.cpp')
$sourceHashes=[ordered]@{}
foreach ($name in $sourceNames) {
    $source=Join-Path $taskRoot $name
    $sourceHashes[$name]=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    $saved=Join-Path $outputRoot ('sources/'+$name)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved) | Out-Null
    Copy-Item -LiteralPath $source -Destination $saved
}
$compileTlog=Join-Path $objectRoot 'Client Core.tlog/CL.command.1.tlog'
$linkTlog=Join-Path $objectRoot 'Client Core.tlog/link.command.1.tlog'
$compileLines=@(Get-Content -LiteralPath $compileTlog)
$compileIndex=-1
for ($index=0;$index -lt $compileLines.Count-1;$index++) {
    if ($compileLines[$index].StartsWith('^') -and $compileLines[$index] -match '(?i)\\CKEYBINDS\.CPP$') { $compileIndex=$index+1;break }
}
if ($compileIndex -lt 0) { throw 'Normal Core compiler metadata for CKeyBinds is required.' }
$compileLine=$compileLines[$compileIndex]
if ($compileLine -notmatch 'MTA_WITH_DEFINITIVE_ADAPTER' -or $compileLine -notmatch '/MT\b') { throw 'Expected normal opt-in Release/x64 Core compiler policy.' }
$linkLines=@(Get-Content -LiteralPath $linkTlog | Where-Object { -not $_.StartsWith('^') -and $_.Trim() })
if ($linkLines.Count -lt 90 -or $linkLines[0] -notmatch '/DLL\b' -or $linkLines[0] -notmatch '/DELAYLOAD:d3dx9_43.dll') {
    throw 'Complete normal Core DLL linker metadata with deferred legacy renderer is required.'
}
if ($OldKeyBindsObject) {
    if (-not $ExpectMissingApi) { throw 'An old object may only be selected for causal missing-API evidence.' }
    $oldObject=(Resolve-Path -LiteralPath $OldKeyBindsObject).Path
    $replaced=0
    for ($index=1;$index -lt $linkLines.Count;$index++) {
        if ($linkLines[$index] -match '(?i)^"[^\"]*\\CKEYBINDS\.OBJ"$') {
            $linkLines[$index]='"'+$oldObject+'"';++$replaced
        }
    }
    if ($replaced -ne 1) { throw 'Expected one exact normal CKeyBinds object substitution.' }
}
# Hash the direct quoted objects, resource and vendor libraries. SDK/system
# import libraries retain the normal toolchain search policy in the saved
# response; the normal Core certificate separately pins its D3DX dependencies.
# Only output paths and the additional test export object change for this DLL.
$inputHashes=[ordered]@{}
foreach ($line in $linkLines) {
    foreach ($match in [regex]::Matches($line,'(?i)(?<!:)"([^"]+\.(?:obj|res|lib))"')) {
        $inputPath=$match.Groups[1].Value
        if (-not [IO.Path]::IsPathRooted($inputPath)) { $inputPath=Join-Path (Join-Path $taskRoot 'Build') $inputPath }
        $inputPath=(Resolve-Path -LiteralPath $inputPath).Path
        $inputHashes[$inputPath]=(Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$normalOutputHashes=[ordered]@{}
foreach ($option in @('OUT','PDB','IMPLIB')) {
    $match=[regex]::Match($linkLines[0],('(?i)/'+$option+':"([^"]+)"'))
    if (-not $match.Success) { throw 'Normal Core DLL output metadata is required.' }
    $outputPath=$match.Groups[1].Value
    if (-not [IO.Path]::IsPathRooted($outputPath)) { $outputPath=Join-Path (Join-Path $taskRoot 'Build') $outputPath }
    $outputPath=(Resolve-Path -LiteralPath $outputPath).Path
    $normalOutputHashes[$outputPath]=(Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
Copy-Item -LiteralPath $compileTlog -Destination (Join-Path $outputRoot 'normal-compile.command.tlog')
Copy-Item -LiteralPath $linkTlog -Destination (Join-Path $outputRoot 'normal-link.command.tlog')
$artifacts=@()
foreach ($item in @(@{source=$GameDeDll;name='game_de.dll';expected=$ExpectedGameDeSha256},
                    @{source=$PthreadDll;name='pthread.dll';expected=$ExpectedPthreadSha256},
                    @{source=$XmlDll;name='xmll.dll';expected=$ExpectedXmlSha256})) {
    $source=(Resolve-Path -LiteralPath $item.source).Path
    $sha=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.expected -and $sha -ne $item.expected.ToLowerInvariant()) { throw 'Supplied actual module fingerprint changed.' }
    $saved=Join-Path $outputRoot $item.name
    Copy-Item -LiteralPath $source -Destination $saved
    $artifacts+=@{source=$source;saved=$saved;sha256=$sha}
}
# This owned saved file exercises the real MTA XML module and parser. It cannot
# read or overwrite a user's installed coreconfig.xml.
$configurationPath=Join-Path $outputRoot 'configured-input.xml'
$configuration=@'
<config><binds>
  <bind key="I" control="FORWARDS"/>
  <bind key="i" control="forwards"/>
  <bind key="u" control="forwards"/>
  <bind key="lshift" control="sprint"/>
  <bind key="rshift" control="walk"/>
  <bind key="mouse_wheel_up" control="forwards"/>
  <bind key="num_8" control="forwards"/>
  <bind key="j" control="fire"/>
  <bind key="i" control="jump"/>
  <bind key="v" command="fixture_command" control="forwards" state="up" arguments="text"/>
  <bind key="k" command="" control="backwards"/>
  <bind key="h" command="fixture_resource" state="down" resource="res" default="h"/>
  <bind key="j" command="fixture_command"/>
  <bind key="not_a_key" control="forwards"/>
  <bind key="i" control="not_a_control"/>
  <ignore/>
</binds></config>
'@
[IO.File]::WriteAllText($configurationPath,$configuration,[Text.UTF8Encoding]::new($false))
$configurationHash=(Get-FileHash -LiteralPath $configurationPath -Algorithm SHA256).Hash.ToLowerInvariant()
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio= & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio native x64 tools are required.' }
$developer=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$environment= & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developer)
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$linker=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/link.exe'
$fixtureObject=Join-Path $outputRoot 'MtaConfiguredInputTests.obj'
$fixtureDll=Join-Path $outputRoot 'core-configured-input-tests.dll'
$fixtureSource=Join-Path $PSScriptRoot 'MtaConfiguredInputTests.cpp'
$normalSource=Join-Path $taskRoot 'Client/core/CKeyBinds.cpp'
$compileLine=$compileLine -replace '(?i)/Y[uc]"[^"]*"\s*',''
$compileLine=$compileLine -replace '(?i)/Fp"[^"]*"\s*',''
$compileLine=$compileLine -replace '(?i)/Fo"[^"]*"',('/Fo"'+$fixtureObject+'"')
$compileLine=$compileLine -replace '(?i)/Fd"[^"]*"',('/Fd"'+(Join-Path $outputRoot 'fixture-compile.pdb')+'"')
$compileLine=$compileLine -replace ('(?i)'+[regex]::Escape($normalSource)+'\s*$'),('"'+$fixtureSource+'"')
if ($compileLine -notmatch [regex]::Escape($fixtureSource) -or $compileLine -match [regex]::Escape($normalSource)) { throw 'Fixture compiler source replacement failed.' }
$compileResponse=Join-Path $outputRoot 'compile-fixture.rsp'
[IO.File]::WriteAllText($compileResponse,$compileLine,[Text.UTF8Encoding]::new($false))
$linkLines[0]=$linkLines[0] -replace '(?i)/OUT:"[^"]*"',('/OUT:"'+$fixtureDll+'"')
$linkLines[0]=$linkLines[0] -replace '(?i)/PDB:"[^"]*"',('/PDB:"'+(Join-Path $outputRoot 'core-configured-input-tests.pdb')+'"')
$linkLines[0]=$linkLines[0] -replace '(?i)/IMPLIB:"[^"]*"',('/IMPLIB:"'+(Join-Path $outputRoot 'core-configured-input-tests.lib')+'"')
# A future normal build may enable a map. Direct it explicitly, including the
# bare /MAP form, so the fixture never writes an extra output into Build cwd.
$linkLines[0]=$linkLines[0] -replace '(?i)/MAP(?::"[^"]*"|:[^\s]+)?(?=\s|$)',('/MAP:"'+(Join-Path $outputRoot 'core-configured-input-tests.map')+'"')
if ($linkLines[0] -match '(?i)/(?:ILK|PGD|LTCGOUT|MANIFESTFILE):') { throw 'Additional normal linker outputs need explicit fixture redirection.' }
$linkResponse=Join-Path $outputRoot 'link-fixture.rsp'
[IO.File]::WriteAllText($linkResponse,($linkLines -join "`n")+"`n"+'"'+$fixtureObject+'"',[Text.UTF8Encoding]::new($false))
$compileExit=$linkExit=$hostCompileExit=$hostExit=$null
$hostResult=$null
$ownedHost=$null
Push-Location -LiteralPath (Join-Path $taskRoot 'Build')
try {
    $compileOutput=@(& $compiler ('@'+$compileResponse) 2>&1);$compileExit=$LASTEXITCODE
    $compileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'fixture-compile.log') -Encoding utf8
    if ($compileExit -eq 0) {
        $linkOutput=@(& $linker ('@'+$linkResponse) 2>&1);$linkExit=$LASTEXITCODE
        $linkOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'fixture-link.log') -Encoding utf8
    }
    if ($linkExit -eq 0 -and -not $ExpectMissingApi) {
        $hostPath=Join-Path $outputRoot 'SanAndreas.exe'
        $hostArguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            (Join-Path $PSScriptRoot 'MtaConfiguredInputHost.cpp'),('/Fo'+(Join-Path $outputRoot 'MtaConfiguredInputHost.obj')),('/Fe'+$hostPath))
        $hostCompileOutput=@(& $compiler @hostArguments 2>&1);$hostCompileExit=$LASTEXITCODE
        $hostCompileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'host-compile.log') -Encoding utf8
        if ($hostCompileExit -eq 0) {
            if ((Get-FileHash -LiteralPath $hostPath -Algorithm SHA256).Hash.ToLowerInvariant() -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Owned fixture unexpectedly matches the game image.' }
            # Keep the exact Process object. A human game with the same basename
            # may be open; no process-name enumeration or game cleanup is used.
            $stdout=Join-Path $outputRoot 'host.stdout.json'
            $stderr=Join-Path $outputRoot 'host.stderr.log'
            $ownedHost=Start-Process -FilePath $hostPath -ArgumentList ('"'+$fixtureDll+'"') -WorkingDirectory $outputRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            if (-not $ownedHost.WaitForExit(30000)) { $ownedHost.Kill();$ownedHost.WaitForExit();throw 'Owned configured-input fixture exceeded its finite deadline.' }
            $hostExit=$ownedHost.ExitCode
            try { $hostResult=Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json } catch { $hostResult=$null }
        }
    }
} finally {
    if ($ownedHost -and -not $ownedHost.HasExited) { $ownedHost.Kill();$ownedHost.WaitForExit() }
    if ($ownedHost) { $ownedHost.Dispose() }
    Pop-Location
}
$sourcesUnchanged=$true
foreach ($name in $sourceNames) { if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHashes[$name]) { $sourcesUnchanged=$false } }
$inputsUnchanged=$true
foreach ($entry in $inputHashes.GetEnumerator()) { if ((Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { $inputsUnchanged=$false } }
$normalOutputsUnchanged=$true
foreach ($entry in $normalOutputHashes.GetEnumerator()) { if ((Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { $normalOutputsUnchanged=$false } }
$configurationUnchanged=(Get-FileHash -LiteralPath $configurationPath -Algorithm SHA256).Hash.ToLowerInvariant() -eq $configurationHash
$causalMissingApi=$compileExit -eq 0 -and $null -ne $linkExit -and $linkExit -ne 0 -and
    (($linkOutput -join "`n") -match 'unresolved external symbol.*(?:ProcessConfiguredControlKey|BuildConfiguredControlFrame|FinishConfiguredControlFrame)')
$success=$compileExit -eq 0 -and $linkExit -eq 0 -and $hostCompileExit -eq 0 -and $hostExit -eq 0 -and $null -ne $hostResult -and $hostResult.success -and $sourcesUnchanged -and $inputsUnchanged -and $normalOutputsUnchanged -and $configurationUnchanged
$record=[ordered]@{success=$success;expected_missing_api=$ExpectMissingApi.IsPresent;causal_missing_api=$causalMissingApi;
    directory=$outputRoot;recorded_at=[DateTimeOffset]::Now.ToString('o');source_hashes=$sourceHashes;source_unchanged=$sourcesUnchanged;
    normal_link_input_hashes=$inputHashes;normal_link_inputs_unchanged=$inputsUnchanged;normal_core_objects_reused=$true;causal_old_keybinds_object=$OldKeyBindsObject;
    normal_output_hashes=$normalOutputHashes;normal_outputs_unchanged=$normalOutputsUnchanged;
    saved_configuration=$configurationPath;saved_configuration_sha256=$configurationHash;saved_configuration_unchanged=$configurationUnchanged;
    compile_exit=$compileExit;link_exit=$linkExit;host_compile_exit=$hostCompileExit;host_exit=$hostExit;host_result=$hostResult;
    artifacts=$artifacts;fixture_dll=$fixtureDll;fixture_dll_sha256=$(if (Test-Path -LiteralPath $fixtureDll) {(Get-FileHash -LiteralPath $fixtureDll -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null});
    test_exports_only_in_disposable_dll=$true;game_launched=$false;game_memory_read=$false;game_hooks_or_input_writes=$false;
    installed_files_changed=$false;actual_xml_module_tested=($null -ne $hostResult -and $hostResult.xml_loaded);game_default_binds_imported=$false;live_de_configured_input_verified=$false;full_mta_boot_verified=$false}
$record | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
$record | ConvertTo-Json -Depth 7 -Compress
if ($ExpectMissingApi) { if (-not $causalMissingApi -or -not $sourcesUnchanged -or -not $inputsUnchanged -or -not $normalOutputsUnchanged) { throw ('Expected actual missing-API red was not observed; see '+$outputRoot) } }
elseif (-not $success) { throw ('Actual configured-input fixture failed; see '+$outputRoot) }
