#!/usr/bin/env pwsh
#requires -Version 7.0
# Build the existing Lua_Client target, then exercise its actual DLL. Project
# generation/restoration belongs to the caller's serialized normal build flow.
[CmdletBinding()]
param(
    [string]$ProjectPath,
    [int]$TimeoutMs = 30000
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'Use native x64 PowerShell.' }
if ($TimeoutMs -lt 1000 -or $TimeoutMs -gt 60000) { throw 'TimeoutMs must be between 1000 and 60000.' }
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $ProjectPath) { $ProjectPath = Join-Path $taskRoot 'Build/Lua_Client.vcxproj' }
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build'))
if (-not $ProjectPath.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Use the normal generated project inside workspace Build.' }
$outputRoot = Join-Path $buildRoot ('mta-lua-client-x64/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
$stageRoot = Join-Path $outputRoot 'host'
$utf8 = [Text.UTF8Encoding]::new($false)
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked build input/output: $candidate" }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}
function Artifact([string]$Path) { [ordered]@{path=$Path;sha256=(Sha $Path);bytes=(Get-Item -LiteralPath $Path).Length} }
function Machine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE file: $Path" }
        $stream.Position = 0x3c
        $stream.Position = $reader.ReadUInt32()
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE signature: $Path" }
        return $reader.ReadUInt16()
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Property([xml]$Document, [string]$Configuration, [string]$Name) {
    $namespace = [Xml.XmlNamespaceManager]::new($Document.NameTable)
    $namespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
    $values = @($Document.SelectNodes('//m:PropertyGroup', $namespace) | Where-Object {
        $_.GetAttribute('Condition').Contains("'$Configuration'")
    } | ForEach-Object { $_.SelectNodes(('m:' + $Name), $namespace) } | ForEach-Object { $_.InnerText })
    if ($values.Count -ne 1) { throw "Generated $Configuration $Name must be explicit and unique." }
    return $values[0]
}
function ProjectOutput([string]$Value) {
    # Premake emits the normal MSBuild ProjectDir macro. Expand this known
    # prefix before comparing physical paths; an unresolved macro is not a
    # directory beneath Build and must not invalidate the isolated output.
    $directory = [IO.Path]::GetDirectoryName($ProjectPath) + [IO.Path]::DirectorySeparatorChar
    $expanded = $Value.Replace('$(ProjectDir)', $directory)
    if ($expanded.Contains('$(')) { throw 'Unexpected unresolved output directory macro.' }
    return [IO.Path]::GetFullPath([IO.Path]::Combine($directory, $expanded)).TrimEnd('\', '/')
}
Assert-NoLinks $ProjectPath
Assert-NoLinks $outputRoot
[IO.Directory]::CreateDirectory($stageRoot) | Out-Null
$sourceNames = @('premake5.lua', 'vendor/lua/premake5.lua', 'utils/definitive_edition/LuaClientX64Tests.cpp', 'utils/definitive_edition/test_lua_client_x64.ps1')
$sourceNames += @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'vendor/lua/src') -File -Recurse | Where-Object Extension -in @('.c', '.h', '.hpp') | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot, $_.FullName).Replace('\', '/') })
$sourcePins = @($sourceNames | Sort-Object -Unique | ForEach-Object { $path = Join-Path $taskRoot $_; Assert-NoLinks $path; [ordered]@{path=$_;sha256=(Sha $path)} })
$projectPin = Artifact $ProjectPath
$classicArtifacts = @()
foreach ($name in @('lua5.1c.dll', 'lua5.1c.lib')) {
    $path = Join-Path $taskRoot ('Bin/mods/deathmatch/' + $name)
    Assert-NoLinks $path
    if (Test-Path -LiteralPath $path -PathType Leaf) { $classicArtifacts += Artifact $path }
}
$artifacts = @(); $runs = @(); $normalBuildExit = $null; $hostCompileExit = $null; $hostExit = $null
$sourcesUnchanged = $false; $classicUnchanged = $false; $projectUnchanged = $false
$success = $false; $failure = $null; $hostResult = $null; $timedOut = $false; $compilerPin = $null; $msbuildPin = $null
try {
    [xml]$project = Get-Content -LiteralPath $ProjectPath -Raw
    $outDir = Property $project 'Release|x64' 'OutDir'
    $resolvedOutDir = ProjectOutput $outDir
    $expectedOutDir = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Bin/mods/deathmatch/x64'))
    Assert-NoLinks $expectedOutDir
    if ($resolvedOutDir -ne $expectedOutDir -or (Property $project 'Release|x64' 'TargetName') -ne 'lua5.1c') {
        throw 'Generate with --with-definitive-adapter: Release x64 Lua_Client must use its isolated normal output.'
    }
    $classicOutDir = ProjectOutput (Property $project 'Release|Win32' 'OutDir')
    if ($classicOutDir -ne [IO.Path]::GetFullPath((Join-Path $taskRoot 'Bin/mods/deathmatch'))) { throw 'Classic Lua output path changed.' }
    $namespace = [Xml.XmlNamespaceManager]::new($project.NameTable)
    $namespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
    $definitionGroups = @($project.SelectNodes('//m:ItemDefinitionGroup', $namespace) | Where-Object { $_.GetAttribute('Condition').Contains("'Release|x64'") })
    if ($definitionGroups.Count -ne 1) { throw 'Expected one explicit normal Release x64 compile definition group.' }
    $definitions = $definitionGroups[0].SelectSingleNode('m:ClCompile/m:PreprocessorDefinitions', $namespace)
    if (-not $definitions -or $definitions.InnerText -notmatch '(^|;)LUA_USE_APICHECK(;|$)' -or $definitions.InnerText -notmatch '(^|;)LUA_BUILD_AS_DLL(;|$)') {
        throw 'Normal Lua_Client x64 must retain its existing API-check and DLL definitions.'
    }
    foreach ($entry in $project.SelectNodes('//m:ExcludedFromBuild', $namespace)) {
        if ($entry.InnerText -eq 'true' -and ($entry.GetAttribute('Condition').Contains("'Release|x64'") -or -not $entry.GetAttribute('Condition'))) {
            throw 'Normal Release x64 Lua_Client sources are still excluded.'
        }
    }
    # Only the existing client target is built. No project references may admit
    # deathmatch, BASS, network or game modules through this dependency test.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'MSVC x64 tools are required.' }
    $developerEnvironment = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
    $environmentLines = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerEnvironment)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($entry in $environmentLines) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    $msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $msbuildPin = Artifact $msbuild
    $compilerPin = Artifact $compiler
    $buildLog = Join-Path $outputRoot 'normal-build.log'
    & $msbuild $ProjectPath /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /nologo /verbosity:minimal 2>&1 | Set-Content -LiteralPath $buildLog -Encoding utf8
    $normalBuildExit = $LASTEXITCODE
    if ($normalBuildExit -ne 0) { throw "Normal Lua_Client build failed: $buildLog" }
    foreach ($name in @('lua5.1c.dll', 'lua5.1c.lib')) {
        $path = Join-Path $expectedOutDir $name
        Assert-NoLinks $path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Normal Lua artifact missing: $path" }
        $saved = Join-Path $stageRoot $name
        [IO.File]::Copy($path, $saved, $false)
        $pin = Artifact $path
        $pin['saved'] = $saved
        if ((Sha $saved) -ne $pin.sha256) { throw 'Staged Lua artifact differs.' }
        $artifacts += $pin
    }
    if ((Machine (Join-Path $stageRoot 'lua5.1c.dll')) -ne 0x8664) { throw 'Normal Lua_Client output is not AMD64.' }
    $hostPath = Join-Path $stageRoot 'OwnedMtaLuaClientX64.exe'
    $compileLog = Join-Path $outputRoot 'host-compile.log'
    & $compiler /nologo /std:c++20 /EHsc /MT /O2 /W4 /WX /DLUA_BUILD_AS_DLL /DLUA_USE_APICHECK `
        ('/I' + (Join-Path $taskRoot 'vendor/lua/src')) (Join-Path $PSScriptRoot 'LuaClientX64Tests.cpp') `
        ('/Fo' + (Join-Path $outputRoot 'LuaClientX64Tests.obj')) ('/Fe' + $hostPath) `
        /link /NOLOGO ('/LIBPATH:' + $stageRoot) lua5.1c.lib 2>&1 | Set-Content -LiteralPath $compileLog -Encoding utf8
    $hostCompileExit = $LASTEXITCODE
    if ($hostCompileExit -ne 0) { throw "Owned Lua host compilation failed: $compileLog" }
    if ((Machine $hostPath) -ne 0x8664) { throw 'Owned test host is not AMD64.' }
    $chunkPath = Join-Path $outputRoot 'compatibility-32-bit-size_t.luac'
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $hostPath; $start.WorkingDirectory = $stageRoot
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.ArgumentList.Add($chunkPath)
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw 'Owned Lua test host did not start.' }
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMs)) {
            $timedOut = $true
            # This handle belongs only to the newly started owned Lua host.
            $process.Kill(); $process.WaitForExit()
        }
        $watch.Stop(); $hostExit = $process.ExitCode
        $text = $stdout.GetAwaiter().GetResult(); $errors = $stderr.GetAwaiter().GetResult()
        $stdoutPath = Join-Path $outputRoot 'host.stdout.log'; $stderrPath = Join-Path $outputRoot 'host.stderr.log'
        [IO.File]::WriteAllText($stdoutPath, $text, $utf8); [IO.File]::WriteAllText($stderrPath, $errors, $utf8)
        $runs += [ordered]@{exit_code=$hostExit;elapsed_ms=$watch.ElapsedMilliseconds;timed_out=$timedOut;stdout=(Artifact $stdoutPath);stderr=(Artifact $stderrPath)}
        if ($timedOut -or $hostExit -ne 0 -or -not [string]::IsNullOrWhiteSpace($errors)) { throw "Owned Lua runtime failed: $stderrPath" }
        $summaryLines = @($text -split '\r?\n' | Where-Object { $_.StartsWith('{') })
        if ($summaryLines.Count -ne 1) { throw 'Owned Lua host must emit one structured terminal result.' }
        $hostResult = $summaryLines[0] | ConvertFrom-Json
        if (-not $hostResult.success -or $hostResult.checks -lt 40 -or $hostResult.cases -lt 6 -or $hostResult.pointerBytes -ne 8 -or
            $hostResult.chunkSizeTBytes -ne 4 -or $hostResult.finalizers -ne 129 -or $hostResult.liveBlocks -ne 0 -or $hostResult.liveBytes -ne 0 -or
            $hostResult.gameplayReady -or $hostResult.fullMtaReady) { throw 'Owned Lua runtime result violates dependency-only contract.' }
        $chunk = [IO.File]::ReadAllBytes($chunkPath)
        if ($chunk.Length -lt 80 -or $chunk[8] -ne 4) { throw 'Saved compatibility fixture is not a four-byte-size_t Lua chunk.' }
        $artifacts += Artifact $hostPath
        $artifacts += Artifact $chunkPath
    } finally { $process.Dispose() }
    $sourcesUnchanged = @($sourcePins | Where-Object { (Sha (Join-Path $taskRoot $_.path)) -ne $_.sha256 }).Count -eq 0
    $classicUnchanged = @($classicArtifacts | Where-Object { (Sha $_.path) -ne $_.sha256 }).Count -eq 0
    $projectUnchanged = (Sha $ProjectPath) -eq $projectPin.sha256
    if (-not $sourcesUnchanged -or -not $classicUnchanged -or -not $projectUnchanged) { throw 'Source, classic Lua artifacts or generated project changed during the run.' }
    foreach ($pin in $artifacts) {
        if ((Sha $pin.path) -ne $pin.sha256 -or ($pin.Contains('saved') -and (Sha $pin.saved) -ne $pin.sha256)) { throw 'Test artifact changed after execution.' }
    }
    $success = $true
} catch { $failure = $_.Exception.Message }
$logs = @(Get-ChildItem -LiteralPath $outputRoot -File -Filter '*.log' | ForEach-Object { Artifact $_.FullName })
$result = [ordered]@{schema='mta_lua_client_x64_test_v1';success=$success;recorded_at=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot;
    normal_target='Lua_Client Release|x64';normal_build_exit=$normalBuildExit;host_compile_exit=$hostCompileExit;host_exit=$hostExit;host_result=$hostResult;
    source_count=$sourcePins.Count;source_hashes=$sourcePins;sources_unchanged=$sourcesUnchanged;classic_artifacts=$classicArtifacts;classic_artifacts_unchanged=$classicUnchanged;
    project=$projectPin;project_unchanged=$projectUnchanged;artifacts=$artifacts;logs=$logs;runs=$runs;compiler=$compilerPin;msbuild=$msbuildPin;error=$failure;
    scope='Actual normal MTA-modified Lua_Client x64 dependency availability, owner pointers, hooks, GC and x86 chunk compatibility. No Core/game/network/GTA startup or installed package change.';
    game_launched=$false;network_started=$false;deathmatch_enabled=$false;package_modified=$false;gameplayReady=$false;fullMtaReady=$false;
    project_generation_or_restoration_performed=$false}
$receipt = Join-Path $outputRoot 'test-result.json'
[IO.File]::WriteAllText($receipt, (($result | ConvertTo-Json -Depth 16) + "`n"), $utf8)
Write-Host ("Lua client x64 success=$success; receipt=$receipt")
if (-not $success) { throw $failure }
