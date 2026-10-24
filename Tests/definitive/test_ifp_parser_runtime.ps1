#!/usr/bin/env pwsh
#requires -Version 7.0
# Execute actual common production IFP source with a strict animation environment.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-ifp-parser-runtime/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/ifp-parser-runtime-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){
    $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$record=[ordered]@{
    schema='mta_actual_common_ifp_runtime_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    result=$null;checks=0;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    generatedFixture=$null;actualHeadersConsumed=@();
    actualCommonIfpParserRuntimeTested=$false;actualIfpOwnershipFailurePathsTested=$false;actualIfpOpaqueLayoutVariantsTested=$false;
    scope='Actual common CClientIFP/CIFPAnimations/CFileReader TUs and unchanged actual class headers, x86 CPU runtime; only unrelated entity/string/game shell substituted. Genuine classic storage12/4 and instrumented opaque80/8 backend layouts share the same parser and cleanup. No duplicated parser or cleanup model, native engine execution or concrete DE factory claim.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    classDeclarationsMirrored=$false;completeCGameReady=$false;completeNativeDEAnimationFactoryReady=$false;fullMtaReady=$false
}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $command = Join-Path $Directory ($Name + '-command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    $info.Environment['VSLANG'] = '1033'
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the fixture process tree created by this invocation is owned.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
. (Join-Path $PSScriptRoot 'Write-IFPParserFixture.ps1')
try {
    $productionSources=@('Client/mods/deathmatch/logic/CClientIFP.cpp','Client/mods/deathmatch/logic/CIFPAnimations.cpp','Client/mods/deathmatch/logic/CFileReader.cpp')
    $sourceNames=$productionSources+@('Tests/definitive/IFPParserRuntimeTests.cpp','Tests/definitive/IFPParserBackendFixture.h',
        'Tests/definitive/IFPParserEnvironment.h','Tests/definitive/Write-IFPParserFixture.ps1',
        'Client/mods/deathmatch/logic/CClientIFP.h','Client/mods/deathmatch/logic/CIFPAnimations.h','Client/mods/deathmatch/logic/CFileReader.h',
        [IO.Path]::GetRelativePath($taskRoot,$PSCommandPath).Replace('\','/'))
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($source in $sourceNames){$pending.Enqueue($source)}
    $observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count){
        $source=$pending.Dequeue()
        if(-not $observed.Add($source)){continue}
        $sourcePath=Join-Path $taskRoot $source
        if(-not(Test-Path -LiteralPath $sourcePath -PathType Leaf)){throw "Actual IFP input missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($sourcePath),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')){
            $name=$match.Groups['path'].Value
            # Exact compiler aliases and generated support files are recorded
            # below; unrelated Client PCH/entity/string trees are not consumed.
            if($name -in @('StdInc.h','CClientEntity.h','SString.h') -or $name.EndsWith('.generated.inl')){continue}
            $candidates=@((Join-Path (Split-Path -Parent $sourcePath) $name),
                (Join-Path $taskRoot ('Tests/definitive/'+$name)),(Join-Path $taskRoot ('Client/sdk/'+$name)),
                (Join-Path $taskRoot ('Client/game_sa/'+$name)),(Join-Path $taskRoot ('Client/mods/deathmatch/logic/'+$name)))
            foreach($candidate in $candidates){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){
                    $relative=[IO.Path]::GetRelativePath($taskRoot,(Resolve-Path -LiteralPath $candidate).Path).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'Actual IFP input escaped workspace'}
                    $pending.Enqueue($relative);break
                }
            }
        }
    }
    [void]$observed.Add('Shared/sdk/SharedUtil.Hash.hpp')
    $record.sourceHashes=@($observed|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha(Join-Path $taskRoot $_))}})
    $record.generatedFixture=Write-IFPParserFixture $taskRoot $scratchRoot
    $record.generatedFixture['durableFiles']=@($record.generatedFixture.files|ForEach-Object{
        $destination=Join-Path $outputRoot ([IO.Path]::GetFileName($_.path));Copy-Item -LiteralPath $_.path -Destination $destination
        [ordered]@{path=$destination;sha256=(Sha $destination)}
    })
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio){throw 'Visual Studio x86 tools required'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    @('@echo off',('call "{0}" -no_logo -arch=x86 -host_arch=x64 >nul' -f $developerCommand),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
        Set-Content -LiteralPath (Join-Path $outputRoot 'capture-msvc.cmd') -Encoding ascii
    $environmentResult=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'environment' 30000
    if($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut){throw 'MSVC x86 initialization failed'}
    foreach($entry in [IO.File]::ReadAllLines($environmentResult.stdout)){
        if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){
            [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
        }
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x86/cl.exe'
    $executable=Join-Path $scratchRoot 'ifp-runtime.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNDEBUG','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        '/D_CRT_SECURE_NO_WARNINGS','/showIncludes',('/I'+$scratchRoot),('/I'+(Join-Path $taskRoot 'Tests/definitive')),
        ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Client/game_sa')),
        ('/I'+(Join-Path $taskRoot 'Client/mods/deathmatch/logic')),(Join-Path $PSScriptRoot 'IFPParserRuntimeTests.cpp'))+
        @($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
    $record.result=[ordered]@{architecture='x86';environment=$environmentResult;compile=$null;test=$null;executableSha256=$null;checks=0;pointerBits=$null}
    $record.result.compile=RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut){throw 'Actual common IFP fixture compilation failed'}
    $includeLog=[IO.File]::ReadAllText($record.result.compile.stdout)
    $consumed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($line in ($includeLog -split '\r?\n')){
        if($line -match '^Note: including file:\s*(?<path>[A-Za-z]:[/\x5c][^\r\n]+)$'){
            [void]$consumed.Add([IO.Path]::GetFullPath($Matches['path'].Trim()))
        }
    }
    foreach($relative in @('Client/mods/deathmatch/logic/CClientIFP.h','Client/mods/deathmatch/logic/CIFPAnimations.h',
        'Client/mods/deathmatch/logic/CFileReader.h','Client/mods/deathmatch/logic/CIFPAnimationStorage.h',
        'Client/mods/deathmatch/logic/CIFPReadGuard.h','Client/sdk/game/CAnimManager.h')){
        $actual=[IO.Path]::GetFullPath((Join-Path $taskRoot $relative))
        if(-not $consumed.Contains($actual)){throw "Compiler did not consume exact actual header: $relative"}
        $record.actualHeadersConsumed+=@([ordered]@{path=$relative;sha256=(Sha $actual)})
    }
    if($consumed.Contains([IO.Path]::GetFullPath((Join-Path $taskRoot 'Client/mods/deathmatch/logic/CClientEntity.h')))){
        throw 'Fixture did not isolate the unrelated client entity dependency'
    }
    $record.result.executableSha256=Sha $executable
    $record.result.test=RunOwned $executable @() $outputRoot 'test' 15000
    $stdout=[IO.File]::ReadAllText($record.result.test.stdout)
    $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) actual common IFP runtime checks \((\d+)-bit; test animation environment, no native engine code\)$')
    if($summary.Success){$record.result.checks=[int]$summary.Groups[1].Value;$record.result.pointerBits=[int]$summary.Groups[2].Value}
    $record.actualCommonIfpParserRuntimeTested=$stdout.Contains('Actual common IFP parser, transaction and cleanup fixture completed')
    $record.actualIfpOpaqueLayoutVariantsTested=$stdout.Contains('Actual common IFP opaque layout variants fixture completed')
    $record.actualIfpOwnershipFailurePathsTested=$stdout.Contains('IFP phase: allocation and before/after publication failures') -and
        $stdout.Contains('IFP phase: transactional reload, retained old set and shared chunk cleanup') -and $record.actualCommonIfpParserRuntimeTested
    $record.checks=$record.result.checks
    $record.success=$record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and $record.checks -gt 0 -and
        $record.result.pointerBits -eq 32 -and $record.actualCommonIfpParserRuntimeTested -and
        $record.actualIfpOpaqueLayoutVariantsTested -and $record.actualIfpOwnershipFailurePathsTested
    if(-not $record.success){throw 'Actual common IFP runtime/evidence failed'}
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
        catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Validate this invocation's flat scratch directory before deleting its
        # generated fixture and compiler scratch. Durable evidence stays above.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Task scratch escaped Build/tmp or became a reparse point.' }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                -not (Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected scratch directory/link/path preserved.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Actual common IFP runtime checks failed; see $outputRoot" }
