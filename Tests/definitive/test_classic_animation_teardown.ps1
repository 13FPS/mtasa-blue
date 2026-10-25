#!/usr/bin/env pwsh
#requires -Version 7.0
# Test only new cleanup teardown policy, actual SA creator-fence body, and DLL quarantine.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-classic-animation-teardown/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/classic-animation-teardown-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){
    $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$record=[ordered]@{
    schema='mta_classic_animation_teardown_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;results=@();failure=$null;
    scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;extractedTerminate=$null;
    actualClassicAnimationTeardownPolicyTested=$false;actualGameSaCleanupCreatorFenceTested=$false;actualStartupModuleCleanupQuarantineTested=$false;
    scope='Actual new Core policy, actual SA Terminate body extracted unchanged into strict dependency receiver, and real task-owned DLL quarantine in isolated children; x86/x64. No complete CGame, native game engine, existing module/network suites or installed payload.';
    gameLaunched=$false;gameAttached=$false;installationChanged=$false;nativeEngineCodeExecuted=$false;fullMtaReady=$false
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
    $executableHash = Sha $Executable
    [ordered]@{
        executable=$Executable;sha256=$executableHash;arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only this run's actual child process tree is eligible for termination.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        $timer.Stop()
        [ordered]@{
            name=$Name;executable=$Executable;executableSha256=$executableHash;
            command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;elapsedMs=$timer.ElapsedMilliseconds;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)
        }
    } finally { $timer.Stop(); $process.Dispose() }
}

try {
    $sourceNames=@('Tests/definitive/ClassicAnimationTeardownTests.cpp','Tests/definitive/test_classic_animation_teardown.ps1',
        'Client/core/ClassicCustomAnimationTeardown.h','Client/core/StartupModule.h','Client/core/CModuleLoader.h',
        'Client/core/CModuleLoader.cpp','Client/game_sa/CGameSA.cpp')
    $record.sourceHashes=@($sourceNames|ForEach-Object{[ordered]@{path=$_;sha256=(Sha(Join-Path $taskRoot $_))}})
    $gameSource=Join-Path $taskRoot 'Client/game_sa/CGameSA.cpp'
    $gameText=[IO.File]::ReadAllText($gameSource)
    $signature='void CGameSA::Terminate()'
    $start=$gameText.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw 'Actual SA Terminate signature missing'}
    $brace=$gameText.IndexOf('{',$start);$depth=0;$end=-1
    for($index=$brace;$index -lt $gameText.Length;$index++){
        if($gameText[$index] -eq '{'){$depth++}
        elseif($gameText[$index] -eq '}'){$depth--;if(-not $depth){$end=$index;break}}
    }
    if($end -lt 0){throw 'Actual SA Terminate body not balanced'}
    $actualBody=$gameText.Substring($start,$end-$start+1)
    $shell=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ClassicGameTerminateFixture.h') -Raw
    $generated=Join-Path $scratchRoot 'ClassicGameTerminate.generated.h'
    [IO.File]::WriteAllText($generated,$shell.Replace('// MTA_ACTUAL_SA_TERMINATE_BODY',$actualBody),[Text.UTF8Encoding]::new($false))
    $durable=Join-Path $outputRoot 'ClassicGameTerminate.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durable
    $record.extractedTerminate=[ordered]@{source='Client/game_sa/CGameSA.cpp';sourceSha256=(Sha $gameSource);
        exactSignature=$signature;startLine=(1+($gameText.Substring(0,$start)-split "`n").Count-1);
        bodyCharacters=$actualBody.Length;unchangedBody=$true;classShellOnly=$true;generatedPath=$durable;sha256=(Sha $durable)}
    $record.sourceHashes+=@([ordered]@{path='Tests/definitive/ClassicGameTerminateFixture.h';sha256=(Sha(Join-Path $PSScriptRoot 'ClassicGameTerminateFixture.h'))})
    $env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio){throw 'Visual Studio x86/x64 tools required'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach($architecture in @('x86','x64')){
        $directory=Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $environmentScript=Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $environmentResult=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'environment' 30000
        if($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut){throw "$architecture compiler environment failed"}
        foreach($entry in [IO.File]::ReadAllLines($environmentResult.stdout)){
            if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $compiler=Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $source=Join-Path $PSScriptRoot 'ClassicAnimationTeardownTests.cpp'
        $provider=Join-Path $directory 'cleanup-quarantine.dll'
        $executable=Join-Path $directory 'classic-cleanup-teardown.exe'
        $common=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',('/I'+$scratchRoot))
        $providerArgs=$common+@('/LD','/DMTA_TEST_CLEANUP_QUARANTINE_PROVIDER',$source,('/Fo'+(Join-Path $scratchRoot ($architecture+'-provider.obj'))),
            ('/Fd'+(Join-Path $scratchRoot ($architecture+'-provider.pdb'))),('/Fe'+$provider),'/link',
            ('/IMPLIB:'+(Join-Path $scratchRoot ($architecture+'-provider.lib'))),'/EXPORT:CleanupQuarantineMarker')
        $hostArgs=$common+@($source,('/Fo'+(Join-Path $scratchRoot ($architecture+'-host.obj'))),
            ('/Fd'+(Join-Path $scratchRoot ($architecture+'-host.pdb'))),('/Fe'+$executable))
        $result=[ordered]@{architecture=$architecture;environment=$environmentResult;providerCompile=$null;hostCompile=$null;
            policy=$null;quarantine=$null;checks=0;pointerBits=$null;providerSha256=$null;executableSha256=$null}
        $record.results+=@($result)
        $result.providerCompile=RunOwned $compiler $providerArgs $directory 'provider-compile' 60000
        if($result.providerCompile.exitCode -ne 0 -or $result.providerCompile.timedOut){throw "$architecture provider compile failed"}
        $result.hostCompile=RunOwned $compiler $hostArgs $directory 'host-compile' 60000
        if($result.hostCompile.exitCode -ne 0 -or $result.hostCompile.timedOut){throw "$architecture teardown host compile failed"}
        $result.providerSha256=Sha $provider;$result.executableSha256=Sha $executable
        $result.policy=RunOwned $executable @() $directory 'policy-test' 10000
        $result.quarantine=RunOwned $executable @('--quarantine',$provider) $directory 'quarantine-child-test' 10000
        $policyText=[IO.File]::ReadAllText($result.policy.stdout);$quarantineText=[IO.File]::ReadAllText($result.quarantine.stdout)
        $policySummary=[regex]::Match($policyText,'Passed (\d+) actual classic cleanup teardown checks \((\d+)-bit\)')
        $quarantineSummary=[regex]::Match($quarantineText,'Passed (\d+) actual cleanup quarantine checks \((\d+)-bit\)')
        $expectedBits=if($architecture -eq 'x86'){32}else{64}
        if(-not $policySummary.Success -or -not $quarantineSummary.Success){throw "$architecture teardown summaries missing"}
        $result.checks=[int]$policySummary.Groups[1].Value+[int]$quarantineSummary.Groups[1].Value
        $result.pointerBits=[int]$policySummary.Groups[2].Value
        if($result.policy.exitCode -ne 0 -or $result.policy.timedOut -or $result.quarantine.exitCode -ne 0 -or $result.quarantine.timedOut -or
            $result.pointerBits -ne $expectedBits -or [int]$quarantineSummary.Groups[2].Value -ne $expectedBits -or $result.checks -le 0 -or
            -not $policyText.Contains('Actual classic cleanup teardown policy fixture completed') -or
            -not $policyText.Contains('Actual GameSA cleanup creator fence fixture completed') -or
            -not $quarantineText.Contains('Actual startup module cleanup quarantine fixture completed')){
            throw "$architecture focused teardown checks failed"
        }
    }
    $record.actualClassicAnimationTeardownPolicyTested=$record.results.Count -eq 2
    $record.actualGameSaCleanupCreatorFenceTested=$record.actualClassicAnimationTeardownPolicyTested
    $record.actualStartupModuleCleanupQuarantineTested=$record.actualClassicAnimationTeardownPolicyTested
    $record.success=$record.actualClassicAnimationTeardownPolicyTested
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try {
            if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
        } catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Compiler scratch is flat and disposable. Preserve any unexpected
        # directory/link rather than recursing into paths not created here.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Task scratch escaped Build/tmp or became a reparse point.'
        }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Unexpected scratch directory/link preserved; cleanup is incomplete.'
            }
            $resolvedFile = (Resolve-Path -LiteralPath $entry.FullName).Path
            if (-not $resolvedFile.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Force).Count
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Classic cleanup teardown checks failed; see $outputRoot" }
