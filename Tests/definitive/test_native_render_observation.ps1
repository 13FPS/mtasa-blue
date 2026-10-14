#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param([string]$GameExecutable = 'C:/Program Files (x86)/Steam/steamapps/common/GTA San Andreas - The Definitive Edition/Gameface/Binaries/Win64/SanAndreas.exe')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot = Join-Path $taskRoot ('Build/mta-native-render-observation/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/game_de/NativeRenderObservation.h','Client/game_de/NativeRenderObservation.cpp',
    'Client/game_de/NativeD3D12OwnershipProbe.h','Client/game_de/NativeD3D12OwnershipProbe.cpp',
    'Client/game_de/NativeCustomPresentProbe.h','Client/game_de/NativeCustomPresentProbe.cpp',
    'Client/game_de/NativeFrameBoundaryProbe.h','Client/game_de/NativeFrameBoundaryProbe.cpp',
    'Tests/definitive/NativeFrameBoundaryProbeTests.cpp',
    'Tests/definitive/NativeRenderObservationTests.cpp','Tests/definitive/test_native_render_observation.ps1',
    'Client/sdk/game/CGameRenderObservation.h','Client/sdk/game/CGameAdapter.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControlStateSynthesis.h','Shared/sdk/CControllerActions.h',
    'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detours.cpp','vendor/detours/4.0.1/src/modules.cpp',
    'vendor/detours/4.0.1/src/disasm.cpp','vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/creatwth.cpp')
$sourceHashes = @($sourceNames | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant()} })
# Extract only the complete pinned native code from the on-disk PE. The game
# executable is never loaded, started, attached, copied over or modified.
$gameHash = (Get-FileHash -LiteralPath $GameExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
if ($gameHash -ne '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f') { throw 'Pinned offline executable differs.' }
$raw = [IO.File]::ReadAllBytes($GameExecutable)
$pe = [BitConverter]::ToInt32($raw,0x3c)
$sections = [BitConverter]::ToUInt16($raw,$pe+6)
$optional = [BitConverter]::ToUInt16($raw,$pe+20)
$codeFixtures = @()
foreach ($route in @(@{name='d3d11';rva=0x1f5a430;bytes=421;sha='605ff6632456992ab74e61cb14ff2fdbb574af74f74935740e66fdf63b2a988f'},
    @{name='d3d12';rva=0x1fd5da0;bytes=429;sha='376cfc00ed38249e3e226a32deabc25d2dd1e525621baba69f2362c29a99934e'})) {
    $codeOffset = $null
    for ($index=0; $index -lt $sections; ++$index) {
        $at=$pe+24+$optional+$index*40
        $rva=[BitConverter]::ToUInt32($raw,$at+12)
        $size=[BitConverter]::ToUInt32($raw,$at+16)
        $start=[BitConverter]::ToUInt32($raw,$at+20)
        if ($route.rva -ge $rva -and $route.rva+$route.bytes -le $rva+$size) { $codeOffset=[int]($start+$route.rva-$rva) }
    }
    if ($null -eq $codeOffset) { throw 'Complete native Present function not file-backed.' }
    $code = [byte[]]::new($route.bytes)
    [Array]::Copy($raw,$codeOffset,$code,0,$route.bytes)
    $fixture=Join-Path $outputRoot ('native-present-'+$route.name+'-fingerprint.bin')
    [IO.File]::WriteAllBytes($fixture,$code)
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash.ToLowerInvariant() -ne $route.sha) { throw 'Pinned native route fingerprint differs.' }
    $codeFixtures += $fixture
}
# The classifier fixtures are complete pinned functions, never executable game
# mappings. Its injected owned reader exercises exactly the production hashes.
$customFunctions = @(
    @{rva=0xacb9c0;bytes=66;sha='1857592da09f0219dfbb41e81b94c02d62308df520e0be2f5e1358d455411e9d'},
    @{rva=0xc20780;bytes=44;sha='f3b4a18798b293c12b9184eaefa26a2b1aaff29543c79524615fcf6fb4275540'},
    @{rva=0xab6580;bytes=3;sha='e18aacce29affcdddaa5f641ac1ec12552e0fe9d066e6408941b1792818b8619'},
    @{rva=0xac66d0;bytes=3;sha='01cb47d078b4841b8408ec4fe278efa83115c6f6e101972987507d2b2b57dcf0'},
    @{rva=0xab4f10;bytes=3;sha='b6cbececf1b07e74f37d57710c68c31f0b92cb08d51093252b321b07542e177b'},
    @{rva=0xc20bf0;bytes=213;sha='eb9b8abefd6c0a758069f99229d293ddbac82432d9543a0d22d91c9cc26352ee'},
    @{rva=0xab4f10;bytes=3;sha='b6cbececf1b07e74f37d57710c68c31f0b92cb08d51093252b321b07542e177b'})
$customFixture=Join-Path $outputRoot 'custom-present-signatures.bin'
$customStream=[IO.MemoryStream]::new()
try {
    foreach($function in $customFunctions) {
        $functionOffset=$null
        for($index=0;$index -lt $sections;++$index) {
            $at=$pe+24+$optional+$index*40
            $rva=[BitConverter]::ToUInt32($raw,$at+12);$size=[BitConverter]::ToUInt32($raw,$at+16);$start=[BitConverter]::ToUInt32($raw,$at+20)
            if($function.rva -ge $rva -and $function.rva+$function.bytes -le $rva+$size) {$functionOffset=[int]($start+$function.rva-$rva)}
        }
        if($null -eq $functionOffset){throw 'Complete presenter function is not file-backed.'}
        $code=[byte[]]::new($function.bytes);[Array]::Copy($raw,$functionOffset,$code,0,$function.bytes)
        $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($code)).ToLowerInvariant()
        if($sha -ne $function.sha){throw 'Complete presenter code signature differs.'}
        $customStream.Write($code,0,$code.Length)
    }
    [IO.File]::WriteAllBytes($customFixture,$customStream.ToArray())
} finally {$customStream.Dispose()}
$codeFixtures += $customFixture

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$compilerEnvironment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $compilerEnvironment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')),
    ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')))
$detoursSources = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp') | ForEach-Object { Join-Path $taskRoot ('vendor/detours/4.0.1/src/'+$_) }
$detoursOutput = @(& $compiler @common /w /c @detoursSources ('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar) 2>&1)
$detoursExit = $LASTEXITCODE
$detoursOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'detours-compile.log') -Encoding utf8
$executable = Join-Path $outputRoot 'native_render_observation_tests.exe'
$objects = @('detours.obj','modules.obj','disasm.obj','image.obj','creatwth.obj') | ForEach-Object { Join-Path $outputRoot $_ }
$arguments = $common + @('/W4','/WX',(Join-Path $PSScriptRoot 'NativeRenderObservationTests.cpp'),
    (Join-Path $PSScriptRoot 'NativeFrameBoundaryProbeTests.cpp'),
    (Join-Path $taskRoot 'Client/game_de/NativeD3D12OwnershipProbe.cpp'),
    (Join-Path $taskRoot 'Client/game_de/NativeCustomPresentProbe.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','bcrypt.lib')+$objects
$compilerOutput = @(& $compiler @arguments 2>&1)
$compilerExit = $LASTEXITCODE
$compilerOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit=$null; $testCount=$null; $timedOut=$false
if ($detoursExit -eq 0 -and $compilerExit -eq 0) {
    $log=Join-Path $outputRoot 'test.log'; $errors=Join-Path $outputRoot 'test-errors.log'
    $fixtureArguments=($codeFixtures | ForEach-Object { '"'+$_+'"' }) -join ' '
    $process=Start-Process -FilePath $executable -ArgumentList $fixtureArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError $errors
    if (-not $process.WaitForExit(30000)) { $timedOut=$true; $process.Kill(); $process.WaitForExit() }
    $testExit=$process.ExitCode; $process.Dispose()
    $testOutput=(@(Get-Content -LiteralPath $log)+@(Get-Content -LiteralPath $errors)) -join "`n"
    if ($testOutput -match 'Passed (\d+) production native render observation checks') { $testCount=[int]$Matches[1] }
}
$unchanged=$true
foreach ($entry in $sourceHashes) { if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $unchanged=$false } }
$record=[ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;architecture='x64';success=($detoursExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and -not $timedOut -and $unchanged);
    detoursCompilerExit=$detoursExit;compilerExit=$compilerExit;testExit=$testExit;testTimedOut=$timedOut;checks=$testCount;sourceHashes=$sourceHashes;sourceUnchanged=$unchanged;
    executable=$executable;gameExecutable=$GameExecutable;gameExecutableSha256=$gameHash;
    nativePresentFixtureHashes=@($codeFixtures | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()} });
    productionCallbackTested=$true;syntheticOwnProcessDetoursInstalled=$true;gameLaunched=$false;gameProcessAttached=$false;gameHooksInstalled=$false;gameInputWritten=$false;gameComCalled=$false;nativeRuntimeVerified=$false}
$record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Native renderer observation checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 6 -Compress
