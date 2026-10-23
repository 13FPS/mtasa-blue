#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the real Core module owner and load deliberately controlled optional
# providers. This focused host never builds Core.dll or starts native gameplay.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'The optional DE owner fixture requires x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-core-constructor-preparation-owner/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/core-constructor-preparation-'+$id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment=@{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$record=[ordered]@{
    schema='mta_core_constructor_preparation_owner_test_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');
    success=$false;directory=$outputRoot;architecture='x64';sourceHashes=@();sourceUnchanged=$true;
    compiler=$null;providerCompile=$null;providers=@();hostCompile=$null;execution=$null;executable=$null;checks=$null;
    actualCoreConstructorPreparationOwnerTested=$false;scratchDirectory=$scratchRoot;scratchRemoved=$false;failure=$null;
    fullCoreBuilt=$false;normalBackendBuilt=$false;nativeEngineCodeExecuted=$false;gameLaunched=$false;
    installationChanged=$false;completeCGameClaimed=$false;adapterAbiChanged=$false
}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Name,[int]$DeadlineMs) {
    $stdout=Join-Path $outputRoot ($Name+'.log')
    $stderr=Join-Path $outputRoot ($Name+'-errors.log')
    $command=Join-Path $outputRoot ($Name+'-command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$outputRoot;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$Executable
    $info.WorkingDirectory=$outputRoot
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratchRoot
    $info.Environment['TMP']=$scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process=[Diagnostics.Process]::new()
    $process.StartInfo=$info
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut=$process.StandardOutput.ReadToEndAsync()
        $readErr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;timedOut=$timedOut;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
function RequireRun($Run) {
    if ($Run.timedOut -or $Run.exitCode -ne 0) { throw "Owned $($Run.name) failed; see $($Run.stdout)" }
}
try {
    # Resolve only repository includes actually reachable from these sources.
    # System headers belong to the recorded MSVC/SDK environment, not this list.
    $includeRoots=@((Join-Path $taskRoot 'Shared/sdk'),(Join-Path $taskRoot 'Client/sdk'))
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach ($relative in @('Client/core/CGameAdapterModule.cpp','Client/core/CGameAdapterModule.h',
            'Tests/definitive/ConstructorPreparationAdapterHostTests.cpp','Tests/definitive/test_constructor_preparation_adapter_host.ps1')) {
        $pending.Enqueue((Join-Path $taskRoot $relative))
    }
    while ($pending.Count) {
        $source=[IO.Path]::GetFullPath($pending.Dequeue())
        if (-not $seen.Add($source)) { continue }
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing source: $source" }
        if ([IO.Path]::GetExtension($source) -eq '.ps1') { continue }
        foreach ($match in [regex]::Matches([IO.File]::ReadAllText($source),'(?m)^\s*#\s*include\s*([<"])([^>"\r\n]+)[>"]')) {
            $candidates=@()
            if ($match.Groups[1].Value -eq '"') { $candidates+=Join-Path ([IO.Path]::GetDirectoryName($source)) $match.Groups[2].Value }
            $candidates+=@($includeRoots | ForEach-Object { Join-Path $_ $match.Groups[2].Value })
            foreach ($candidate in $candidates) {
                if (Test-Path -LiteralPath $candidate -PathType Leaf) { $pending.Enqueue($candidate);break }
            }
        }
    }
    $record.sourceHashes=@($seen | Sort-Object | ForEach-Object {
        [ordered]@{path=[IO.Path]::GetRelativePath($taskRoot,$_).Replace('\','/');sha256=(Sha $_)}
    })
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio native tools are required.' }
    [Environment]::SetEnvironmentVariable('TEMP',$scratchRoot,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$scratchRoot,'Process')
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environmentCommand='call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
    $compilerEnvironment=& $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the x64 compiler environment.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $linker=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/link.exe'
    $record.compiler=[ordered]@{path=$compiler;sha256=(Sha $compiler);linker=$linker;linkerSha256=(Sha $linker)}
    $source=Join-Path $PSScriptRoot 'ConstructorPreparationAdapterHostTests.cpp'
    $common=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/showIncludes',
        ('/I'+$includeRoots[0]),('/I'+$includeRoots[1]))
    $providerObject=Join-Path $scratchRoot 'constructor-provider.obj'
    $record.providerCompile=RunOwned $compiler ($common+@('/c','/DMTA_CONSTRUCTOR_PROVIDER=1',$source,('/Fo'+$providerObject))) 'provider-compile' 60000
    RequireRun $record.providerCompile
    $baseExports=@('CreateGameAdapter','ConfigureConstructorFixture')
    $optionalExports=@('BeginGameConstructorPreparationV1','ReadGameConstructorPreparationV1','RenewGameClockServiceV1','ReadGameClockServiceV1','EndGameClockServiceV1')
    # Seven small links share one compiled provider object. Each actual DLL has
    # a different exported contract; no loader or production call is mocked.
    $providerPaths=@()
    for ($mode=1;$mode -le 7;++$mode) {
        $exports=@($baseExports)
        if ($mode -ne 7) {
            for ($index=0;$index -lt $optionalExports.Count;++$index) {
                if ($mode -ne ($index+2)) { $exports+=@($optionalExports[$index]) }
            }
        }
        $dll=Join-Path $outputRoot ("constructor-provider-$mode.dll")
        $arguments=@('/nologo','/DLL',('/OUT:'+$dll),('/IMPLIB:'+(Join-Path $scratchRoot ("constructor-provider-$mode.lib"))),$providerObject)
        $arguments+=@($exports | ForEach-Object { '/EXPORT:'+$_ })
        $run=RunOwned $linker $arguments ("provider-link-$mode") 30000
        RequireRun $run
        $record.providers+=@([ordered]@{mode=$mode;exports=$exports;path=$dll;sha256=(Sha $dll);link=$run})
        $providerPaths+=@($dll)
    }
    $hostExecutable=Join-Path $outputRoot 'constructor-preparation-adapter-host-tests.exe'
    $arguments=$common+@($source,(Join-Path $taskRoot 'Client/core/CGameAdapterModule.cpp'),
        ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$hostExecutable))
    $record.hostCompile=RunOwned $compiler $arguments 'host-compile' 60000
    RequireRun $record.hostCompile
    $record.execution=RunOwned $hostExecutable $providerPaths 'host-test' 30000
    RequireRun $record.execution
    $text=[IO.File]::ReadAllText($record.execution.stdout)
    if ($text -notmatch 'Passed (\d+) Core constructor preparation owner checks') { throw 'Focused owner check count is absent.' }
    $record.checks=[int]$Matches[1]
    $record.actualCoreConstructorPreparationOwnerTested=$record.checks -gt 0 -and $text.Contains('Production Core constructor preparation optional owner fixture completed')
    $record.executable=[ordered]@{path=$hostExecutable;sha256=(Sha $hostExecutable)}
    $record.success=$record.actualCoreConstructorPreparationOwnerTested
} catch {
    $record.failure=$_.Exception.Message
    $record.success=$false
} finally {
    foreach ($entry in $record.sourceHashes) {
        if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourceUnchanged=$false }
    }
    foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process') }
    # Delete only this resolved invocation's scratch beneath the workspace.
    $expectedPrefix=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))+[IO.Path]::DirectorySeparatorChar
    $resolvedScratch=[IO.Path]::GetFullPath($scratchRoot)
    if (-not $resolvedScratch.StartsWith($expectedPrefix,[StringComparison]::OrdinalIgnoreCase)) {
        $record.success=$false;$record.failure='Scratch target escaped the task workspace.'
    } else {
        try {
            Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
            $record.scratchRemoved=-not (Test-Path -LiteralPath $resolvedScratch)
        } catch { $record.success=$false;$record.failure=$_.Exception.Message }
    }
    $record.success=$record.success -and $record.sourceUnchanged -and $record.scratchRemoved
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
if (-not $record.success) { throw "Core constructor preparation owner checks failed: $($record.failure); see $outputRoot" }
$record | ConvertTo-Json -Depth 10 -Compress
