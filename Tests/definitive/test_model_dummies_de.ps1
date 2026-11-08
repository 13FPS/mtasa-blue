#!/usr/bin/env pwsh
#requires -Version 7.0
# Execute the shared MTA dummy policy only on x86 and the actual composed DE SDK only on x64; native access is explicitly doubled.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-model-dummies-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/model-dummies-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_model_dummies_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Shared actual-MTA dummy cache-before-hold/default-before-store/restore-before-release policy on x86 only. On x64 only: actual seven dummy and six inherited wheel/group SDK overrides through one immutable same-owner model facade; access explicitly doubled. Native qualification, actual CAS and real domain execute only in the separate Runtime fixture. No complete model/game factory or GTA execution.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    completeModelInfoReady=$false;fullMtaReady=$false;installationChanged=$false;
    actualSharedModelDummiesPolicyTested=$false;actualModelDummiesSdkTested=$false

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
    $sourceNames=@('Tests/definitive/ModelDummiesPolicyTests.cpp','Tests/definitive/ModelDummiesDETests.cpp',
        'Tests/definitive/test_model_dummies_de.ps1','Tests/definitive/Write-ModelDummiesFixture.ps1',
        'Client/game_de/CModelInfoObjectGroupDE.h','Client/game_de/CModelInfoObjectGroupDE.cpp',
        'Client/game_de/CModelInfoDummiesDE.h','Client/game_de/CModelInfoDummiesDE.cpp','Client/game_de/NativeModelDummiesAccess.h',
        'Client/game_de/NativeModelObjectGroupAccess.h','Client/game_de/CModelInfoWheelDE.cpp',
        'Client/game_de/CModelInfoDE.cpp','Client/sdk/game/CModelVehicleDummyPolicy.h')
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in $sourceNames){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $taskRoot $name))
        if(-not $path.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'SDK graph left allowed scope'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $taskRoot ('Shared/sdk/'+$include)),(Join-Path $taskRoot ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($taskRoot,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $record.sourceHashes=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}})
    . (Join-Path $PSScriptRoot 'Write-ModelDummiesFixture.ps1')
    $generated=Join-Path $scratchRoot 'ModelDummiesFixture.generated.h'
    $record['generatedFixture']=Write-ModelDummiesFixture $taskRoot $generated
    if($record.generatedFixture.rejectingMethods -ne 73 -or $record.generatedFixture.poisonedModelMethod -ne 1 -or $record.generatedFixture.sourcePolicyDoubles -ne 5 -or @($record.generatedFixture.inheritedProductionMethods).Count -ne 13){throw 'Actual dummy7/group3/wheel3/poison1/source-doubles5/reject73 fixture partition changed'}
    $durableGenerated=Join-Path $outputRoot 'ModelDummiesFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    if((Sha $durableGenerated) -ne $record.generatedFixture.sha256){throw 'Generated dummy SDK fixture changed'}
    $record.generatedFixture['durableCopy']=$durableGenerated

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x86','x64')) {
        $directory = Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        # Capture toolchain paths only; a full environment dump could contain
        # unrelated credentials and does not belong in durable build evidence.
        # Keep cmd's quoting inside an owned batch file. ProcessStartInfo's
        # argument escaping otherwise treats the quoted CALL path as C argv.
        $environmentScript = Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',
            ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1',
            'set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
        $environmentResult['script'] = $environmentScript
        $environmentResult['scriptSha256'] = Sha $environmentScript
        if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw "$architecture MSVC environment initialization failed." }
        foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
            if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable = Join-Path $directory 'model_dummies_sdk_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot))
        # The shared policy has no DE headers and is checked with classic's x86
        # ABI. The actual DE facade retains its real x64 native graph requirement.
        if ($architecture -eq 'x64') {
            $arguments += @((Join-Path $PSScriptRoot 'ModelDummiesDETests.cpp'),
                (Join-Path $taskRoot 'Client/game_de/CModelInfoDE.cpp'),
                (Join-Path $taskRoot 'Client/game_de/CModelInfoWheelDE.cpp'),
                (Join-Path $taskRoot 'Client/game_de/CModelInfoObjectGroupDE.cpp'),
                (Join-Path $taskRoot 'Client/game_de/CModelInfoDummiesDE.cpp'))
        } else {
            $arguments += (Join-Path $PSScriptRoot 'ModelDummiesPolicyTests.cpp')
        }
        $arguments += @(
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            scope=$(if($architecture -eq 'x64'){'DE-SDK-only'}else{'shared-policy-only'});
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;
            actualSharedModelDummiesPolicyTested=$false;actualModelDummiesSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual model dummy fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summaryPattern = if ($architecture -eq 'x64') {
            '(?:^|\r?\n)Passed (\d+) actual MTA model dummy SDK checks \((\d+)-bit; native access doubled\)$'
        } else {
            '(?:^|\r?\n)Passed (\d+) shared MTA model dummy policy checks \((\d+)-bit; no DE native graph\)$'
        }
        $summary = [regex]::Match($text.TrimEnd(), $summaryPattern)
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualSharedModelDummiesPolicyTested=$text.Contains('Shared MTA model dummy policy fixture completed')
        $result.actualModelDummiesSdkTested=$text.Contains('Actual model dummy and inherited wheel/group SDK fixture completed')

        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        $expectedSdkScope = $architecture -eq 'x64'
        $expectedPolicyScope = $architecture -eq 'x86'
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or
            $result.actualSharedModelDummiesPolicyTested -ne $expectedPolicyScope -or $result.actualModelDummiesSdkTested -ne $expectedSdkScope) {
            throw "$architecture MTA model dummy checks failed or scope/summary/pointer-width evidence was missing."
        }
    }
    $record.actualSharedModelDummiesPolicyTested=@($record.results|Where-Object{$_.architecture -eq 'x86' -and $_.actualSharedModelDummiesPolicyTested}).Count -eq 1
    $record.actualModelDummiesSdkTested=@($record.results|Where-Object{$_.architecture -eq 'x64' -and $_.actualModelDummiesSdkTested}).Count -eq 1

    $record.success = $record.results.Count -eq 2 -and $record.actualSharedModelDummiesPolicyTested -and $record.actualModelDummiesSdkTested
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
if (-not $record.success) { throw "Model dummy policy/SDK checks failed; see $outputRoot" }
