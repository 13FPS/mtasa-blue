#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual pure world prerequisite reader over owned guarded byte arrays. This
# does not inspect GTA, invoke native code, or qualify an engine thread/owner.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-native-startup-world/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/native-startup-world-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_native_startup_world_v1';directory=$outputRoot;success=$false;sourceHashes=@();sourcesUnchanged=$true;
    compile=$null;test=$null;checks=0;failure=$null;
    scope='Exact GetWorld prerequisites, chunk0+1 registry addressing, conservative identity, native Level outer chain, read/overflow refusal and full-path stability';
    engineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;clientDllLoaded=$false;installationChanged=$false}

function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $outputRoot ($Name + '.log')
    $stderr = Join-Path $outputRoot ($Name + '-errors.log')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $outputRoot
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
            # The bounded process belongs to this fixture, never to the game.
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
    $sourceNames = @('Client/game_de/NativeStartupWorldAccess.h','Client/game_de/NativeStartupWorldProfile.h',
        'Client/game_de/NativeClassChainAccess.h','Client/game_de/NativeUiClassMapProfile.h',
        'Tests/definitive/NativeStartupWorldTests.cpp','Tests/definitive/test_native_startup_world.ps1')
    $record.sourceHashes = @($sourceNames | ForEach-Object { [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))} })
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($entry in $environment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $executable = Join-Path $outputRoot 'native_startup_world_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX',
        (Join-Path $PSScriptRoot 'NativeStartupWorldTests.cpp'),
        ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
    [ordered]@{compiler=$compiler;sha256=(Sha $compiler);arguments=$arguments} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8NoBOM
    $record.compile = RunOwned $compiler $arguments 'compile' 60000
    if ($record.compile.exitCode -ne 0 -or $record.compile.timedOut) { throw 'Native startup-world fixture compilation failed.' }
    $record.executable = $executable
    $record.executableSha256 = Sha $executable
    $record.test = RunOwned $executable @() 'test' 10000
    $text = [IO.File]::ReadAllText($record.test.stdout)
    if ($text -match 'Passed (\d+) native startup-world checks') { $record.checks = [int]$Matches[1] }
    if ($record.test.exitCode -ne 0 -or $record.test.timedOut -or $record.checks -le 0 -or
        -not $text.Contains('Native getter routing, chunked registry identity, typed Level outer path, checked addresses, guarded refusals and coherent samples completed')) {
        throw 'Native startup-world regression checks failed.'
    }
    $record.success = $true
} catch { $record.failure = $_.Exception.ToString() }
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    foreach ($entry in $record.sourceHashes) {
        if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    # Only this run's checked Build/tmp compiler scratch may be removed; its
    # executable, source hashes and compiler/test logs remain durable evidence.
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
if (-not $record.success) { throw "Native startup-world checks failed; see $outputRoot" }
