#requires -Version 7.0
# Focused actual shared registry/matching + concrete DE owner CPU checks.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-texture-registry-preparation-de/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/texture-registry-preparation-de-'+$id)
foreach($path in @($outputRoot,$scratchRoot)) {
    if(Test-Path -LiteralPath $path){throw 'Fresh task directory already exists.'}
    [IO.Directory]::CreateDirectory($path)|Out-Null
}
$previous=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_texture_registry_preparation_de_tests_v1';success=$false;directory=$outputRoot;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;results=@();failure=$null;
    scratchDirectory=$scratchRoot;scratchRemoved=$false;gameLaunched=$false;gameAttached=$false;nativeEngineCodeExecuted=$false;
    installationChanged=$false;completeRendererReady=$false;actualOwnedTextureRegistryTested=$false;
    actualSharedShaderMatchingTested=$false;actualTextureInitializationPrefixTested=$false;
    scope='Only actual shared CPU texture matching, concrete DE owned no-texture preparation, exact source key/tag/name, real wildcard/entity/layer/cache behavior, source failure prefixes and repeated registry membership. No model draw/GUI/native renderer/factory or historical suite claim.'}
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function RunOwned([string]$Exe,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$Deadline) {
    $stdout=Join-Path $Directory ($Name+'.log');$stderr=Join-Path $Directory ($Name+'-errors.log')
    [ordered]@{executable=$Exe;sha256=(Sha $Exe);arguments=$Arguments;workingDirectory=$Directory;temporaryDirectory=$scratchRoot;deadlineMs=$Deadline}|
        ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $Directory ($Name+'-command.json')) -Encoding utf8NoBOM
    $info=[Diagnostics.ProcessStartInfo]::new($Exe)
    $info.WorkingDirectory=$Directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratchRoot;$info.Environment['TMP']=$scratchRoot
    foreach($argument in $Arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try {
        if(-not $process.Start()){throw ('Could not start owned '+$Name)}
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($Deadline)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned process did not retire.'}}
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stdoutSha256=(Sha $stdout);stderr=$stderr;stderrSha256=(Sha $stderr)}
    } finally {$process.Dispose()}
}
try {
    $paths=@('Client/game_de/CRenderWarePreparationDE.h','Client/sdk/game/CShaderTextureWatchTypes.h','Client/sdk/game/CShaderTextureWatch.h',
        'Client/sdk/game/CShaderTextureWatch.inl','Client/sdk/game/CTextureWatchInitializationPolicy.h','Client/sdk/game/CRenderWare.h',
        'Client/sdk/game/CGame.h','Shared/sdk/SString.h','Shared/sdk/SString.hpp','Shared/sdk/SharedUtil.Map.h','Shared/sdk/SharedUtil.Misc.h',
        'Shared/sdk/SharedUtil.HashMap.h','Shared/sdk/SharedUtil.FastHashMap.h','Shared/sdk/SharedUtil.FastHashSet.h',
        'Tests/definitive/TextureRegistryPreparationDETests.cpp','Tests/definitive/TextureRegistrySStringSupport.cpp','Tests/definitive/TextureRegistrySecondTranslationUnit.cpp',
        'Tests/definitive/test_texture_registry_preparation_de.ps1')
    $record.sourceHashes=@($paths|Sort-Object -Unique|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}})
    $support=[IO.File]::ReadAllText((Join-Path $taskRoot 'Tests/definitive/TextureRegistrySStringSupport.cpp'))
    $actual=[IO.File]::ReadAllText((Join-Path $taskRoot 'Shared/sdk/SString.hpp'))
    $selected=[regex]::Match($actual,'(?s)SString SString::ToLower\(\) const\s*\{.*?\r?\n\}').Value
    if(-not $selected -or [regex]::Replace($support,'\s','').IndexOf([regex]::Replace($selected,'\s',''),[StringComparison]::Ordinal) -lt 0){
        throw 'Fixture string link support is not the exact current actual SString::ToLower definition.'
    }
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $vs=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $vs){throw 'MSVC x86/x64 tools unavailable.'}
    $developer=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    foreach($architecture in @('x86','x64')) {
        $directory=Join-Path $outputRoot $architecture;[IO.Directory]::CreateDirectory($directory)|Out-Null
        $cmd=Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developer,$architecture),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
            Set-Content -LiteralPath $cmd -Encoding ascii
        $environment=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'environment' 30000
        if($environment.exitCode -ne 0 -or $environment.timedOut){throw 'MSVC environment failed.'}
        foreach($line in [IO.File]::ReadAllLines($environment.stdout)) {
            if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
        }
        $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
        $executable=Join-Path $directory 'texture_registry_preparation_de_tests.exe'
        # Vendor headers use the compiler's supported external-header boundary;
        # warnings remain errors in the actual shared extraction and fixtures.
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/Gy','/MD','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/showIncludes',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
            ('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src/windows')),('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src')),'/external:W0',
            (Join-Path $PSScriptRoot 'TextureRegistryPreparationDETests.cpp'),(Join-Path $PSScriptRoot 'TextureRegistrySStringSupport.cpp'),(Join-Path $PSScriptRoot 'TextureRegistrySecondTranslationUnit.cpp'),
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','/OPT:REF')
        $result=[ordered]@{architecture=$architecture;compile=$null;test=$null;checks=0;pointerBits=0;sourceIncludesConfirmed=$false;
            actualOwnedTextureRegistryTested=$false;actualSharedShaderMatchingTested=$false;actualTextureInitializationPrefixTested=$false}
        $record.results+=$result
        $result.compile=RunOwned $compiler $arguments $directory 'compile' 60000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw ($architecture+' actual registry compilation failed.')}
        $readText=[IO.File]::ReadAllText($result.compile.stdout)
        $result.sourceIncludesConfirmed=$readText.Contains('CShaderTextureWatch.inl') -and $readText.Contains('CShaderTextureWatchTypes.h') -and $readText.Contains('CRenderWarePreparationDE.h')
        $result.test=RunOwned $executable @() $directory 'test' 10000
        $stdout=[IO.File]::ReadAllText($result.test.stdout)
        $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) actual texture registry preparation checks \((\d+)-bit\)$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value;$result.pointerBits=[int]$summary.Groups[2].Value}
        $result.actualOwnedTextureRegistryTested=$stdout.Contains('Production actual owned no-texture registry preparation fixture completed')
        $result.actualSharedShaderMatchingTested=$stdout.Contains('Production actual shared shader texture matching fixture completed')
        $result.actualTextureInitializationPrefixTested=$stdout.Contains('Production actual texture registry initialization prefix fixture completed')
        $expectedBits=if($architecture -eq 'x86'){32}else{64}
        if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or
            -not $result.sourceIncludesConfirmed -or -not $result.actualOwnedTextureRegistryTested -or -not $result.actualSharedShaderMatchingTested -or
            -not $result.actualTextureInitializationPrefixTested){throw ($architecture+' actual registry execution evidence failed.')}
    }
    $record.actualOwnedTextureRegistryTested=@($record.results|Where-Object{-not $_.actualOwnedTextureRegistryTested}).Count -eq 0
    $record.actualSharedShaderMatchingTested=@($record.results|Where-Object{-not $_.actualSharedShaderMatchingTested}).Count -eq 0
    $record.actualTextureInitializationPrefixTested=@($record.results|Where-Object{-not $_.actualTextureInitializationPrefixTested}).Count -eq 0
    $record.success=$record.results.Count -eq 2 -and $record.actualOwnedTextureRegistryTested -and $record.actualSharedShaderMatchingTested -and $record.actualTextureInitializationPrefixTested
} catch {$record.failure=$_.Exception.ToString()}
finally {
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($entry in $record.sourceHashes){if((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256){$record.sourcesUnchanged=$false}}
    try {
        $resolved=(Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\')+'\'
        $allowed=(Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Scratch escaped Build/tmp.'}
        $entries=@(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach($entry in $entries){if($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            -not (Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolved,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected scratch child preserved.'}}
        foreach($entry in $entries){Remove-Item -LiteralPath $entry.FullName -Force}
        [IO.Directory]::Delete($scratchRoot,$false);$record.scratchRemoved=$true
    } catch {$record.success=$false;$record.failure=($record.failure+[Environment]::NewLine+$_.Exception.ToString()).Trim()}
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
    $record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw ('Focused actual registry checks failed; see '+$outputRoot)}
