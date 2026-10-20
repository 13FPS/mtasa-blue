#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual native pad updater owner against an owned private executable entry.
# The synthetic case uses real Detours and peer suspension in this test process only; no game is opened.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-native-pad-update-control/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/native-pad-update-control-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_native_pad_update_control_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();detours=@{};failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Actual NativePadUpdateControl state machine and CPad SDK control tested with a fixture-owned MEM_PRIVATE/RX updater, real Detours/peer suspension/unwind/admission and only profile/image namespace substitutions. Separate ordinary-host executable links unmodified production qualification and owner code to verify wrong-image refusal. No live game or native engine readiness is established.';
    ownedProcessMemoryOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    foreignProcessAccessed=$false;nativeLifecycleReady=$false;syntheticOwnProcessDetoursInstalled=$false;productionOwnerExercised=$false;nativeHookQuiescenceReady=$false;fullMtaReady=$false;
    clientDllLoaded=$false;installationChanged=$false
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
    $sourceNames = @('Client/game_de/NativePadUpdateControl.h','Client/game_de/NativePadUpdateControl.cpp',
        'Client/game_de/NativePadUpdateAccess.h','Client/game_de/NativePadUpdateProfile.h','Client/game_de/NativePadUpdateProfile.cpp',
        'Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp','Client/game_de/NativePeerThreads.h','Client/game_de/NativePeerThreads.cpp',
        'Client/game_de/NativeFunctionRange.h','Client/game_de/NativeFunctionRange.cpp','Client/game_de/NativeServiceAdmission.h',
        'Client/game_de/CPadStateDE.h','Client/game_de/CPadStateDE.cpp','Client/game_de/CPadControlDE.h','Client/game_de/CPadControlDE.cpp',
        'Client/game_de/NativePadStateAccess.h','Client/game_de/NativeInputRouting.h','Client/sdk/game/CPad.h','Client/sdk/game/CGameAdapter.h',
        'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
        'Tests/definitive/NativePadUpdateControlTests.cpp','Tests/definitive/NativePadUpdateControlHostTests.cpp',
        'Tests/definitive/test_native_pad_update_control.ps1',
        'vendor/detours/premake5.lua','vendor/detours/4.0.1/src/creatwth.cpp','vendor/detours/4.0.1/src/detours.cpp',
        'vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/modules.cpp','vendor/detours/4.0.1/src/disolx86.cpp',
        'vendor/detours/4.0.1/src/disolx64.cpp','vendor/detours/4.0.1/src/disasm.cpp','vendor/detours/4.0.1/src/uimports.cpp',
        'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detver.h')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x64')) {
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
        $detoursDirectory = Join-Path $directory 'detours'
        [IO.Directory]::CreateDirectory($detoursDirectory) | Out-Null
        $vendorRoot = Join-Path $taskRoot 'vendor/detours/4.0.1/src'
        $vendorNames = @('creatwth','detours','image','modules','disolx86','disolx64','disasm')
        $vendorSources = @($vendorNames | ForEach-Object { Join-Path $vendorRoot ($_+'.cpp') })
        # Match the shipping x64 opt-in Detours VM-node implementation. Only
        # vendor translation units use /W0; every actual owner/helper uses /WX.
        $vendorArguments = @('/nologo','/c','/EHsc','/O2','/MD','/W0','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
            ('/Fo'+$detoursDirectory+[IO.Path]::DirectorySeparatorChar)) + $vendorSources
        $record.detours = [ordered]@{compile=$null;library=$null;path=$null;sha256=$null;vmTransactionNodes=$true}
        $record.detours.compile = RunOwned $compiler $vendorArguments $detoursDirectory 'compile' 60000
        if ($record.detours.compile.exitCode -ne 0 -or $record.detours.compile.timedOut) { throw 'Actual VM-node Detours compilation failed.' }
        $librarian = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/lib.exe'
        $library = Join-Path $detoursDirectory 'detours.lib'
        $objects = @($vendorNames | ForEach-Object { Join-Path $detoursDirectory ($_+'.obj') })
        $record.detours.library = RunOwned $librarian (@('/nologo',('/OUT:'+$library))+$objects) $detoursDirectory 'library' 30000
        if ($record.detours.library.exitCode -ne 0 -or $record.detours.library.timedOut) { throw 'Actual VM-node Detours library creation failed.' }
        $record.detours.path = $library
        $record.detours.sha256 = Sha $library
        $common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$vendorRoot))
        foreach ($case in @('synthetic-lifecycle','ordinary-host')) {
            $caseDirectory = Join-Path $directory $case
            [IO.Directory]::CreateDirectory($caseDirectory) | Out-Null
            $executable = Join-Path $caseDirectory 'native_pad_update_control_tests.exe'
            $sources = @((Join-Path $taskRoot 'Client/game_de/NativePeerThreads.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeFunctionRange.cpp'))
            $substitutions = @()
            if ($case -eq 'synthetic-lifecycle') {
                # Production owner is included by this test TU with exactly
                # two namespace substitutions, never a copied state machine.
                $sources += @((Join-Path $PSScriptRoot 'NativePadUpdateControlTests.cpp'),
                    (Join-Path $taskRoot 'Client/game_de/CPadStateDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CPadControlDE.cpp'))
                $substitutions = @('NativePadUpdateProfile -> SyntheticPadUpdateProfile','NativeImageAccess -> SyntheticImageAccess')
                $expectedSummary = '(?:^|\r?\n)Passed (\d+) production pad updater owner checks \((\d+)-bit\)$'
            } else {
                $sources += @((Join-Path $PSScriptRoot 'NativePadUpdateControlHostTests.cpp'),
                    (Join-Path $taskRoot 'Client/game_de/NativePadUpdateControl.cpp'),(Join-Path $taskRoot 'Client/game_de/NativePadUpdateProfile.cpp'),
                    (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp'))
                $expectedSummary = '(?:^|\r?\n)Passed (\d+) ordinary-host pad updater refusal checks \((\d+)-bit\)$'
            }
            $arguments = $common + $sources + @(('/Fo'+$caseDirectory+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),
                '/link',$library,'bcrypt.lib','user32.lib')
            $result = [ordered]@{architecture=$architecture;case=$case;environment=$environmentResult;compile=$null;test=$null;
                checks=0;pointerBits=$null;namespaceSubstitutions=$substitutions;executable=$executable;executableSha256=$null}
            $record.results += $result
            $result.compile = RunOwned $compiler $arguments $caseDirectory 'compile' 60000
            if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$case actual pad updater owner fixture compilation failed." }
            $result.executableSha256 = Sha $executable
            $result.test = RunOwned $executable @() $caseDirectory 'test' 30000
            $text = [IO.File]::ReadAllText($result.test.stdout)
            $summary = [regex]::Match($text.TrimEnd(), $expectedSummary)
            if ($summary.Success) {
                $result.checks = [int]$summary.Groups[1].Value
                $result.pointerBits = [int]$summary.Groups[2].Value
            }
            if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne 64) {
                throw "$case actual pad updater owner checks failed or summary/pointer-width evidence was missing."
            }
            if ($case -eq 'synthetic-lifecycle') {
                $record.syntheticOwnProcessDetoursInstalled = $true
                $record.productionOwnerExercised = $true
            }
        }
    }
    $record.success = $record.results.Count -eq 2
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
if (-not $record.success) { throw "Native pad updater owner checks failed; see $outputRoot" }

