#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the real header helpers on both client architectures without loading
# Lua, GTA or a client DLL. Numeric ID widths must remain architecture independent.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-lua-userdata-ids/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/lua-userdata-ids-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_lua_userdata_ids_v1';directory=$outputRoot;success=$false;sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scope='Actual LuaCommon.h encoded-ID helpers and Shared/sdk/Common.h ElementID on x86 and x64';
    luaLoaded=$false;gameLaunched=$false;gameAttached=$false;clientDllLoaded=$false;installationChanged=$false}

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
    $sourceNames = @('Client/mods/deathmatch/logic/lua/LuaCommon.h','Shared/sdk/Common.h','Shared/sdk/SString.h',
        'Shared/sdk/SharedUtil.IntTypes.h','vendor/lua/src/lua.h','vendor/lua/src/lualib.h','vendor/lua/src/lauxlib.h',
        'vendor/lua/src/luaconf.h','Tests/definitive/LuaUserDataIdTests.cpp','Tests/definitive/test_lua_userdata_ids.ps1')
    $record.sourceHashes = @($sourceNames | ForEach-Object { [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))} })
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x86','x64')) {
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
        $executable = Join-Path $directory 'lua_userdata_id_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/lua/src')),
            (Join-Path $PSScriptRoot 'LuaUserDataIdTests.cpp'),('/Fo'+(Join-Path $directory 'test.obj')),('/Fe'+$executable))
        [ordered]@{compiler=$compiler;sha256=(Sha $compiler);arguments=$arguments} | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $directory 'compile-command.json') -Encoding utf8NoBOM
        $result = [ordered]@{architecture=$architecture;compile=$null;test=$null;checks=0;executable=$executable;executableSha256=$null}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture Lua userdata ID fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        $expectedChecks = if ($architecture -eq 'x86') { 29 } else { 40 }
        if ($text -match 'Passed (\d+) Lua userdata ID checks \((\d+)-bit\)') {
            $result.checks = [int]$Matches[1]
            if ([int]$Matches[2] -ne $expectedBits) { throw 'The executable pointer width does not match the requested architecture.' }
        }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -ne $expectedChecks) {
            throw "$architecture Lua userdata ID regression checks failed."
        }
    }
    $record.success = $record.results.Count -eq 2
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
if (-not $record.success) { throw "Lua userdata ID checks failed; see $outputRoot" }
