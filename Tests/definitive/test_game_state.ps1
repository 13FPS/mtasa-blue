#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the actual checked native game-state reader against an owned
# sparse SEC_IMAGE. No GTA code, process, hooks or client DLL are involved.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-game-state/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/game-state-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_game_state_tests_v1';directory=$outputRoot;success=$false;sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scope='All ten native inputs with proved 6..9 SDK mapping, unchanged-output refusals, changed pins, caller thread and real image/page/OS-copy failures';
    engineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;clientDllLoaded=$false;installationChanged=$false}

function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw "Could not launch owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # The process tree was created by this fixture and belongs to this run.
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;
            stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}

try {
    $sourceNames = @('Client/game_de/NativeGameStateReader.h','Client/game_de/NativeGameStateReader.cpp',
        'Client/game_de/NativeGameStateProfile.h','Client/game_de/NativeFrontendMenuProfile.h','Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp',
        'Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h',
        'Shared/sdk/CControlStateSynthesis.h','Shared/sdk/enums/SystemState.h','Tests/definitive/GameStateTests.cpp','Tests/definitive/test_game_state.ps1')
    $record.sourceHashes = @($sourceNames | ForEach-Object { [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))} })
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x64')) {
        $directory = Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand,$architecture)
        if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
        foreach ($entry in $environment) {
            if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
        }
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable = Join-Path $directory 'game_state_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Client/sdk')),
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),
            (Join-Path $PSScriptRoot 'GameStateTests.cpp'),
            ('/Fo'+$directory+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','bcrypt.lib')
        [ordered]@{compiler=$compiler;sha256=(Sha $compiler);arguments=$arguments} | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $directory 'compile-command.json') -Encoding utf8NoBOM
        $result = [ordered]@{architecture=$architecture;compile=$null;test=$null;checks=0;executable=$executable;executableSha256=$null}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture native game-state fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @($directory) $directory 'test' 15000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        if ($text -match 'Passed (\d+) native game-state checks') { $result.checks = [int]$Matches[1] }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or
            -not $text.Contains('All ten native inputs checked; only proved states6..9 translated')) {
            throw "$architecture native game-state regression checks failed."
        }
    }
    $record.success = $record.results.Count -eq 1
} catch { $record.failure = $_.Exception.ToString() }
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    foreach ($entry in $record.sourceHashes) {
        if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    # Only this run's disposable compiler scratch is removed. Logs and binaries
    # stay under the durable output directory for the combined build evidence.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot).TrimEnd('\') + '\'
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp')).TrimEnd('\') + '\'
    if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase)) { throw 'Task scratch escaped Build/tmp.' }
    foreach ($file in @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File)) {
        if (-not [IO.Path]::GetFullPath($file.FullName).StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $record.scratchDirectory = $scratchRoot
    $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File).Count
    $record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Native game-state checks failed; see $outputRoot" }
