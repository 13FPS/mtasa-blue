#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise actual camera Restore/raw selector and const Cam FOV; native provenance uses the focused owner fixture.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-camera-basics-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/camera-basics-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_camera_basics_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Actual camera Restore/rawBYTE selector and const Cam FOV over the same retained provider. Every finite FOV preserves bits, successful nonfinite becomes70; provider refusal always propagates. Full logicalDWORD dispatch and all14 statuses, default-refusing older providers, existing sphere regression. Includes selected actual JumpCut owner transport; its native two-step/deferred-yaw policy is verified by the owner fixture. Includes actual SetFOV finite open0..180 input policy, invalid no-provider no-ops and valid exact raw-bit/full-row dispatch. Includes actual signed mode/GetDirection/SetDirection policies: raw read, zero-or-one original-pair conditional repair, source wrapping and ACK-before-output alias order. Includes actual retained partial-row SDK vector transport, unique acquisition and producer release, exact pointer identity/direct writes and refusal propagation. Native lifetime/field admission is tested separately by the owner fixture; Includes the actual target-history pointer and its full four-vector writable extent under the same row hold, without new timestamp/count/lease semantics. Includes the actual GetInterface/target getter composed with the existing same-owner controlled ped registry, qualified zero, nonzero refusal and registration-token ABA refusal; no complete camera/GetCam or ped factory.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    completeCameraReady=$false;completeCamReady=$false;fullMtaReady=$false;installationChanged=$false;
    actualCameraBasicsSdkTested=$false;actualCameraJumpCutSdkTested=$false;actualCameraFovWriteSdkTested=$false;actualCameraDirectionSdkTested=$false;actualCameraRowBorrowSdkTested=$false;actualCameraHistorySdkTested=$false;actualCameraTargetSdkTested=$false;actualPedPooledInterfaceSdkTested=$false;actualCameraTargetSetterSdkTested=$false;actualCameraGetCamSdkTested=$false;actualCameraWidescreenSdkTested=$false;actualCameraMainTargetSdkTested=$false

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
    $productionSources=@('Client/game_de/CCameraSphereVisibleDE.cpp','Client/game_de/CCameraCamsDE.cpp','Client/game_de/CCameraWidescreenDE.cpp','Client/game_de/CCameraTargetDE.cpp','Client/game_de/CCamFovDE.cpp','Client/game_de/CCamRowDE.cpp','Client/game_de/CCamTargetDE.cpp','Client/game_de/CCamTargetSetterDE.cpp','Client/game_de/CPedPooledDE.cpp','Client/game_de/CPoolsDE.cpp','Client/game_de/CPedVitalsDE.cpp')
    $sourceNames=$productionSources+@('Client/game_de/NativeCameraSphereVisibleAccess.h','Tests/definitive/CameraBasicsSdkTests.cpp','Tests/definitive/test_camera_basics_sdk.ps1','Tests/definitive/Write-CameraBasicsFixture.ps1','Tests/definitive/Write-PedPoolRegistryFixture.ps1')
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
    . (Join-Path $PSScriptRoot 'Write-CameraBasicsFixture.ps1')
    $cameraGenerated=Join-Path $scratchRoot 'CameraBasicsFixture.generated.h'
    $camGenerated=Join-Path $scratchRoot 'CamFovFixture.generated.h'
    $rowGenerated=Join-Path $scratchRoot 'CamRowFixture.generated.h'
    $record['cameraGeneratedFixture']=Write-CameraBasicsFixture $taskRoot $cameraGenerated
    $productsGenerated=Join-Path $scratchRoot 'CameraCamsFixture.generated.h'
    $record['cameraProductsGeneratedFixture']=Write-CameraCamsFixture $taskRoot $productsGenerated
    $widescreenGenerated=Join-Path $scratchRoot 'CameraWidescreenFixture.generated.h'
    $record['cameraWidescreenGeneratedFixture']=Write-CameraWidescreenFixture $taskRoot $widescreenGenerated
    $mainTargetGenerated=Join-Path $scratchRoot 'CameraTargetFixture.generated.h'
    $record['cameraMainTargetGeneratedFixture']=Write-CameraTargetFixture $taskRoot $mainTargetGenerated
    if($record.cameraMainTargetGeneratedFixture.rejectingMethods -ne 27 -or @($record.cameraMainTargetGeneratedFixture.inheritedProductionMethods).Count -ne 8){throw 'Actual main-target camera SDK35-method partition changed'}
    Copy-Item -LiteralPath $mainTargetGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($mainTargetGenerated)))
    if($record.cameraWidescreenGeneratedFixture.rejectingMethods -ne 28 -or @($record.cameraWidescreenGeneratedFixture.inheritedProductionMethods).Count -ne 7){throw 'Actual widescreen camera SDK35-method partition changed'}
    Copy-Item -LiteralPath $widescreenGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($widescreenGenerated)))
    if($record.cameraProductsGeneratedFixture.rejectingMethods -ne 30 -or @($record.cameraProductsGeneratedFixture.inheritedProductionMethods).Count -ne 5){throw 'Actual camera products SDK35-method partition changed'}
    Copy-Item -LiteralPath $productsGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($productsGenerated)))
    $record['camGeneratedFixture']=Write-CamFovFixture $taskRoot $camGenerated
    $record['rowGeneratedFixture']=Write-CamRowFixture $taskRoot $rowGenerated
    $targetGenerated=Join-Path $scratchRoot 'CamTargetFixture.generated.h'
    $pedGenerated=Join-Path $scratchRoot 'PedPooledFixture.generated.h'
    $record['targetGeneratedFixture']=Write-CamTargetFixture $taskRoot $targetGenerated
    $setterGenerated=Join-Path $scratchRoot 'CamTargetSetterFixture.generated.h'
    $record['setterGeneratedFixture']=Write-CamTargetSetterFixture $taskRoot $setterGenerated
    if($record.setterGeneratedFixture.rejectingMethods -ne 0 -or @($record.setterGeneratedFixture.inheritedProductionMethods).Count -ne 13){throw 'Actual setter Cam SDK13-method partition changed'}
    Copy-Item -LiteralPath $setterGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($setterGenerated)))
    $record['pedGeneratedFixture']=Write-PedPooledFixture $taskRoot $pedGenerated
    if($record.targetGeneratedFixture.rejectingMethods -ne 1 -or @($record.targetGeneratedFixture.inheritedProductionMethods).Count -ne 12){throw 'Actual target Cam SDK13-method partition changed'}
    if(@($record.pedGeneratedFixture.inheritedPooledMethods).Count -ne 1 -or -not $record.pedGeneratedFixture.sharedVirtualPedBase){throw 'Actual pooled ped SDK composition changed'}
    Copy-Item -LiteralPath $targetGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($targetGenerated)))
    Copy-Item -LiteralPath $pedGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($pedGenerated)))
    if($record.cameraGeneratedFixture.rejectingMethods -ne 31 -or @($record.cameraGeneratedFixture.inheritedProductionMethods).Count -ne 4){throw 'Actual camera SDK35-method partition changed'}
    if($record.camGeneratedFixture.rejectingMethods -ne 8 -or @($record.camGeneratedFixture.inheritedProductionMethods).Count -ne 5){throw 'Actual Cam SDK13-method partition changed'}
    if($record.rowGeneratedFixture.rejectingMethods -ne 2 -or @($record.rowGeneratedFixture.inheritedProductionMethods).Count -ne 11){throw 'Actual retained Cam SDK13-method partition changed'}
    Copy-Item -LiteralPath $rowGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($rowGenerated)))
    Copy-Item -LiteralPath $cameraGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($cameraGenerated)))
    Copy-Item -LiteralPath $camGenerated -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($camGenerated)))

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
        $executable = Join-Path $scratchRoot ('camera_basics_sdk_tests-' + $architecture + '.exe')
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'CameraBasicsSdkTests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+@(
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;actualCameraBasicsSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual camera basics SDK fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($text.TrimEnd(), '(?:^|\r?\n)Passed (\d+) camera basics SDK checks \((\d+)-bit; transport only\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualCameraBasicsSdkTested=$text.Contains('Actual camera basics SDK fixture completed')
        $result['actualCameraJumpCutSdkTested']=$text.Contains('Actual camera JumpCut SDK fixture completed')
        $result['actualCameraFovWriteSdkTested']=$text.Contains('Actual Cam SetFOV SDK policy fixture completed')
        $result['actualCameraDirectionSdkTested']=$text.Contains('Actual Cam mode and direction SDK policy fixture completed')
        $result['actualCameraRowBorrowSdkTested']=$text.Contains('Actual retained Cam row vector SDK fixture completed')
        $result['actualCameraHistorySdkTested']=$text.Contains('Actual Cam target history SDK fixture completed')
        $result['actualCameraTargetSdkTested']=$text.Contains('Actual camera target SDK fixture completed')
        $result['actualPedPooledInterfaceSdkTested']=$text.Contains('Actual ped pooled interface SDK fixture completed')
        $result['actualCameraTargetSetterSdkTested']=$text.Contains('Actual camera target setter SDK fixture completed')
        $result['actualCameraGetCamSdkTested']=$text.Contains('Actual camera GetCam SDK fixture completed')
        $result['actualCameraWidescreenSdkTested']=$text.Contains('Actual camera widescreen SDK fixture completed')
        $result['actualCameraMainTargetSdkTested']=$text.Contains('Actual camera main target SDK fixture completed')
        $result['actualCamFovSdkTested']=$text.Contains('Actual Cam finite FOV SDK fixture completed')
        $result['cameraProviderRefusalTransportTested']=$text.Contains('Actual camera provider refusal transport fixture completed')

        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -ne 3665 -or $result.pointerBits -ne $expectedBits -or -not $result.actualCameraBasicsSdkTested -or -not $result.actualCameraJumpCutSdkTested -or -not $result.actualCameraFovWriteSdkTested -or -not $result.actualCameraDirectionSdkTested -or -not $result.actualCameraRowBorrowSdkTested -or -not $result.actualCameraHistorySdkTested -or -not $result.actualCameraTargetSdkTested -or -not $result.actualPedPooledInterfaceSdkTested -or -not $result.actualCameraTargetSetterSdkTested -or -not $result.actualCameraGetCamSdkTested -or -not $result.actualCameraWidescreenSdkTested -or -not $result.actualCameraMainTargetSdkTested -or -not $result.actualCamFovSdkTested -or -not $result.cameraProviderRefusalTransportTested) {
            throw "$architecture actual camera basics SDK checks failed or summary/pointer-width evidence was missing."
        }
    }
    $record.actualCameraBasicsSdkTested=@($record.results|Where-Object{-not $_.actualCameraBasicsSdkTested}).Count -eq 0

    $record.actualCameraJumpCutSdkTested=@($record.results|Where-Object{-not $_.actualCameraJumpCutSdkTested}).Count -eq 0
    $record.actualCameraFovWriteSdkTested=@($record.results|Where-Object{-not $_.actualCameraFovWriteSdkTested}).Count -eq 0
    $record.actualCameraDirectionSdkTested=@($record.results|Where-Object{-not $_.actualCameraDirectionSdkTested}).Count -eq 0
    $record.actualCameraRowBorrowSdkTested=@($record.results|Where-Object{-not $_.actualCameraRowBorrowSdkTested}).Count -eq 0
    $record.actualCameraHistorySdkTested=@($record.results|Where-Object{-not $_.actualCameraHistorySdkTested}).Count -eq 0
    $record.actualCameraTargetSdkTested=@($record.results|Where-Object{-not $_.actualCameraTargetSdkTested}).Count -eq 0
    $record.actualPedPooledInterfaceSdkTested=@($record.results|Where-Object{-not $_.actualPedPooledInterfaceSdkTested}).Count -eq 0
    $record.actualCameraTargetSetterSdkTested=@($record.results|Where-Object{-not $_.actualCameraTargetSetterSdkTested}).Count -eq 0
    $record.actualCameraGetCamSdkTested=@($record.results|Where-Object{-not $_.actualCameraGetCamSdkTested}).Count -eq 0
    $record.actualCameraWidescreenSdkTested=@($record.results|Where-Object{-not $_.actualCameraWidescreenSdkTested}).Count -eq 0
    $record.actualCameraMainTargetSdkTested=@($record.results|Where-Object{-not $_.actualCameraMainTargetSdkTested}).Count -eq 0
    $record['actualCamFovSdkTested']=@($record.results|Where-Object{-not $_.actualCamFovSdkTested}).Count -eq 0
    $record['cameraProviderRefusalTransportTested']=@($record.results|Where-Object{-not $_.cameraProviderRefusalTransportTested}).Count -eq 0
    $record.success = $record.results.Count -eq 2 -and $record.actualCameraBasicsSdkTested -and $record.actualCameraJumpCutSdkTested -and $record.actualCameraFovWriteSdkTested -and $record.actualCameraDirectionSdkTested -and $record.actualCameraRowBorrowSdkTested -and $record.actualCameraHistorySdkTested -and $record.actualCameraTargetSdkTested -and $record.actualPedPooledInterfaceSdkTested -and $record.actualCameraTargetSetterSdkTested -and $record.actualCameraGetCamSdkTested -and $record.actualCameraWidescreenSdkTested -and $record.actualCameraMainTargetSdkTested -and $record.actualCamFovSdkTested -and $record.cameraProviderRefusalTransportTested
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
if (-not $record.success) { throw "Actual camera basics SDK checks failed; see $outputRoot" }
