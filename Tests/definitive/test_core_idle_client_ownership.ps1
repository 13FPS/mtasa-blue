#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the exact current Core idle member in an owned CPU host. Module
# ownership and client publication are independently modeled from CModManager;
# no game, module loading, native service, or production Core singleton is used.
[CmdletBinding()]
param(
    [string]$WorkspaceRoot = (Join-Path $PSScriptRoot '../..'),
    [string]$CoreSource = '',
    [string]$MessageLoopSource = '',
    [string]$ChatSource = '',
    [switch]$SourceCheckOnly,
    [switch]$ExpectRegressionFailure
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath $WorkspaceRoot).Path
$taskBuild = Join-Path $taskRoot 'Build'
$runName = (Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskBuild ('mta-core-idle-client-ownership-tests/'+$runName)
$scratchRoot = Join-Path $taskBuild ('tmp/core-idle-client-ownership-'+$runName)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$record = [ordered]@{
    schema='mta_core_idle_client_ownership_tests_v1'; recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=$false; sourceCheckOnly=[bool]$SourceCheckOnly; expectedRegressionFailure=[bool]$ExpectRegressionFailure;
    sourceHashes=@(); sourcesUnchanged=$true; extraction=$null; dispatchExtractions=@(); managerProof=$null; generated=@(); generatedUnchanged=$true;
    compiler=$null; environment=$null; compile=$null; test=$null; checks=0; pointerBits=64; failures=@();
    actualCoreIdleBodyTested=$false; actualSdkClientDeclarationTested=$false; ownedHighAddressClientVerified=$false;
    scratchRemoved=$false; gameLaunched=$false; gameAttached=$false; installedFilesChanged=$false;
    scope='Exact extracted CCore::HandleIdlePulse, three current-client dispatch slices from focus/chat, and CClientBase declaration; manager ownership is a source-checked minimal fixture, not execution of CModManager, the full window procedure, chat completion algorithm, or a complete Core.'
}
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
function Sha([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
function Normalized([string]$Text) { $Text.Replace("`r`n","`n").Replace("`r","`n").TrimEnd()+"`n" }
function TextSha([string]$Text) {
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($Text))).ToLowerInvariant()
}
function Assert-OwnedSource([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (-not $resolved.StartsWith($taskRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture source must stay within the workspace: $resolved"
    }
    $candidate = $resolved
    while ($candidate -and $candidate.Length -ge $taskRoot.Length) {
        if (((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing linked fixture source: $candidate"
        }
        if ($candidate -eq $taskRoot) { break }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
    $resolved
}
function Extract-Definition([string]$Text,[string]$Marker,[switch]$ClassDeclaration) {
    $matches = [regex]::Matches($Text,$Marker,[Text.RegularExpressions.RegexOptions]::Multiline)
    if ($matches.Count -ne 1) { throw "Expected one exact definition marker, found $($matches.Count): $Marker" }
    $match = $matches[0]
    $open = $match.Index+$match.Length
    while ($open -lt $Text.Length -and [char]::IsWhiteSpace($Text[$open])) { ++$open }
    if ($open -ge $Text.Length -or $Text[$open] -ne '{') { throw "Definition marker is not followed directly by a body: $Marker" }
    $depth = 0
    $state = 'code'
    $end = -1
    for ($index=$open; $index -lt $Text.Length; ++$index) {
        $character = $Text[$index]
        $next = if ($index+1 -lt $Text.Length) { $Text[$index+1] } else { [char]0 }
        if ($state -eq 'line') { if ($character -eq "`n") { $state='code' }; continue }
        if ($state -eq 'block') { if ($character -eq '*' -and $next -eq '/') { $state='code'; ++$index }; continue }
        if ($state -eq 'string' -or $state -eq 'character') {
            if ($character -eq '\') { ++$index; continue }
            if (($state -eq 'string' -and $character -eq '"') -or ($state -eq 'character' -and $character -eq "'")) { $state='code' }
            continue
        }
        if ($character -eq '/' -and $next -eq '/') { $state='line'; ++$index; continue }
        if ($character -eq '/' -and $next -eq '*') { $state='block'; ++$index; continue }
        # The selected bodies contain no raw strings. Refuse an unsupported new
        # lexical form instead of mistaking braces in its payload for code.
        if ($character -eq 'R' -and $next -eq '"') { throw 'Raw C++ strings require an explicit extractor update.' }
        if ($character -eq '"') { $state='string'; continue }
        if ($character -eq "'") { $state='character'; continue }
        if ($character -eq '{') { ++$depth }
        if ($character -eq '}') {
            --$depth
            if ($depth -eq 0) { $end=$index+1; break }
        }
    }
    if ($end -lt 0 -or $state -ne 'code') { throw "Unbalanced C++ body: $Marker" }
    if ($ClassDeclaration) {
        while ($end -lt $Text.Length -and [char]::IsWhiteSpace($Text[$end])) { ++$end }
        if ($end -ge $Text.Length -or $Text[$end] -ne ';') { throw 'SDK class declaration is missing its terminating semicolon.' }
        ++$end
    }
    [ordered]@{text=$Text.Substring($match.Index,$end-$match.Index);start=$match.Index;length=$end-$match.Index;
        firstLine=1+([regex]::Matches($Text.Substring(0,$match.Index),"`n")).Count;uniqueMarker=$Marker}
}
function Require([bool]$Condition,[string]$Why) { if (-not $Condition) { throw $Why } }
function Extract-Dispatch([string]$Path,[string]$Call,[string]$Wrapper,[string]$Parameters) {
    $source=[IO.File]::ReadAllText($Path)
    $escaped=[regex]::Escape($Call)
    # Accept only the exact fixed dispatch or its exact unguarded preimage. The
    # copied slice, rather than a replacement policy, becomes the fixture body.
    $pattern='^[ \t]*(?:if \(auto\* client = pModManager->GetClient\(\)\)[ \t]*\r?\n[ \t]*client->'+$escaped+';|pModManager->GetClient\(\)->'+$escaped+';)[ \t]*\r?$'
    $matches=[regex]::Matches($source,$pattern,[Text.RegularExpressions.RegexOptions]::Multiline)
    Require ($matches.Count -eq 1) "Expected one exact guarded or preimage dispatch slice: $Call"
    $match=$matches[0]
    $gates=[regex]::Matches($source.Substring(0,$match.Index),'if \(pModManager && pModManager->IsLoaded\(\)\)')
    Require ($gates.Count -gt 0) "Missing outer DLL-owner gate for $Call"
    $gate=$gates[$gates.Count-1]
    Require ($match.Index-$gate.Index -lt 2200) "Outer DLL-owner gate moved away from $Call"
    $context=$source.Substring($gate.Index,$match.Index+$match.Length-$gate.Index)
    if ($Call -eq 'OnWindowFocusChange(bFocus)') {
        Require ($context.Contains('bool bFocus = (wState == WA_CLICKACTIVE) || (wState == WA_ACTIVE);') -and
            $context.Contains('PostMessage(hwnd, WM_CUSTOMFOCUS_FIX, 0, 0);')) 'Activation focus and startup focus-fix context changed.'
    }
    if ($Call -eq 'GetPlayerNames(vPlayerNames)') {
        Require ($context.Contains('std::vector<SString> vPlayerNames;')) 'Chat no longer creates its empty player-name vector before dispatch.'
    }
    $normalized=Normalized $match.Value
    $record.dispatchExtractions += [ordered]@{source=$Path;sourceSha256=(Sha $Path);call=$Call;
        firstLine=1+([regex]::Matches($source.Substring(0,$match.Index),"`n")).Count;
        normalizedSliceSha256=(TextSha $normalized);surroundingGateContextSha256=(TextSha (Normalized $context));
        guarded=$match.Value.Contains('if (auto* client = pModManager->GetClient())');outerModuleGateVerified=$true;
        boundary='Only the exact current-client call slice executes; the surrounding source-checked DLL gate is supplied by the minimal fixture wrapper.'}
    "void $Wrapper($Parameters)`n{`n    if (pModManager && pModManager->IsLoaded())`n    {`n$normalized    }`n}`n"
}
function Run-Owned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name+'.stdout.log')
    $stderr = Join-Path $Directory ($Name+'.stderr.log')
    $command = Join-Path $Directory ($Name+'.command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;deadlineMs=$DeadlineMs} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $command -Encoding utf8
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$Executable; $info.WorkingDirectory=$Directory; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratchRoot; $info.Environment['TMP']=$scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo=$info
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not start the owned $Name process." }
        $readOut=$process.StandardOutput.ReadToEndAsync(); $readErr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Terminate only this exact child tree created by the fixture.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not stop after its deadline." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),$utf8)
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),$utf8)
        [ordered]@{name=$Name;exitCode=$process.ExitCode;timedOut=$timedOut;elapsedMs=$timer.ElapsedMilliseconds;deadlineMs=$DeadlineMs;
            executable=$Executable;executableSha256=(Sha $Executable);command=$command;commandSha256=(Sha $command);
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $timer.Stop(); $process.Dispose() }
}

try {
    Require ([IntPtr]::Size -eq 8) 'Use native x64 PowerShell for the owned x64 fixture.'
    Require (-not ($SourceCheckOnly -and $ExpectRegressionFailure)) 'Source-only extraction cannot establish an expected executed regression.'
    $canonicalCore = Assert-OwnedSource (Join-Path $taskRoot 'Client/core/CCore.cpp')
    if (-not $CoreSource) { $CoreSource=$canonicalCore }
    $selectedCore = Assert-OwnedSource $CoreSource
    $canonicalMessageLoop=Assert-OwnedSource (Join-Path $taskRoot 'Client/core/CMessageLoopHook.cpp')
    $canonicalChat=Assert-OwnedSource (Join-Path $taskRoot 'Client/core/CChat.cpp')
    if (-not $MessageLoopSource) { $MessageLoopSource=$canonicalMessageLoop }
    if (-not $ChatSource) { $ChatSource=$canonicalChat }
    $selectedMessageLoop=Assert-OwnedSource $MessageLoopSource
    $selectedChat=Assert-OwnedSource $ChatSource
    Require (-not $ExpectRegressionFailure -or $selectedCore -ne $canonicalCore) 'Expected regression mode requires an explicit owned preimage, never the canonical source.'
    $paths = @($selectedCore,$canonicalCore,$canonicalMessageLoop,$canonicalChat,$selectedMessageLoop,$selectedChat,(Join-Path $taskRoot 'Client/core/CCore.h'),
        (Join-Path $taskRoot 'Client/core/CModManager.cpp'),(Join-Path $taskRoot 'Client/core/CModManager.h'),
        (Join-Path $taskRoot 'Client/sdk/core/CClientBase.h'),(Join-Path $taskRoot 'Shared/sdk/SString.h'),
        (Join-Path $taskRoot 'Shared/sdk/SharedUtil.IntTypes.h'),(Join-Path $PSScriptRoot 'CoreIdleClientOwnershipTests.cpp'),$PSCommandPath)
    $record.sourceHashes = @($paths | Sort-Object -Unique | ForEach-Object {
        $path=Assert-OwnedSource $_
        [ordered]@{path=$path;relativePath=[IO.Path]::GetRelativePath($taskRoot,$path).Replace('\','/');sha256=(Sha $path)}
    })
    $coreText=[IO.File]::ReadAllText($selectedCore)
    $body=Extract-Definition $coreText '^void CCore::HandleIdlePulse\(\)[ \t]*\r?$'
    $sdkPath=Join-Path $taskRoot 'Client/sdk/core/CClientBase.h'
    $sdk=Extract-Definition ([IO.File]::ReadAllText($sdkPath)) '^class CClientBase[ \t]*\r?$' -ClassDeclaration
    $methods=[regex]::Matches($sdk.text,'\bvirtual\s+[^;]+?=\s*0\s*;',[Text.RegularExpressions.RegexOptions]::Singleline)
    Require ($methods.Count -eq 12) 'The strict SDK client fixture must be reviewed when CClientBase methods change.'
    foreach ($entry in @(@{name='CoreIdleClientOwnershipBody.generated.inl';definition=$body},@{name='CoreIdleClientSdk.generated.inl';definition=$sdk})) {
        $path=Join-Path $outputRoot $entry.name
        $normalized=Normalized $entry.definition.text
        [IO.File]::WriteAllText($path,$normalized,$utf8)
        $record.generated += [ordered]@{path=$path;sha256=(Sha $path);normalizedDefinitionSha256=(TextSha $normalized)}
    }
    $dispatches=Extract-Dispatch $selectedMessageLoop 'OnWindowFocusChange(bFocus)' 'DispatchActivationFocus' 'CModManager* pModManager, bool bFocus'
    $dispatches+=Extract-Dispatch $selectedMessageLoop 'OnWindowFocusChange(true)' 'DispatchPositionFocus' 'CModManager* pModManager'
    $dispatches+=Extract-Dispatch $selectedChat 'GetPlayerNames(vPlayerNames)' 'DispatchPlayerNames' 'CModManager* pModManager, std::vector<SString>& vPlayerNames'
    $dispatchPath=Join-Path $outputRoot 'CoreCurrentClientDispatch.generated.inl'
    [IO.File]::WriteAllText($dispatchPath,$dispatches,$utf8)
    $record.generated += [ordered]@{path=$dispatchPath;sha256=(Sha $dispatchPath);normalizedDefinitionSha256=(TextSha $dispatches)}
    $record.extraction=[ordered]@{source=$selectedCore;sourceSha256=(Sha $selectedCore);canonicalSource=$canonicalCore;
        canonicalSourceSha256=(Sha $canonicalCore);isCanonical=$selectedCore -eq $canonicalCore;marker=$body.uniqueMarker;
        firstLine=$body.firstLine;characters=$body.length;normalizedBodySha256=(TextSha (Normalized $body.text));
        sdkSource=$sdkPath;sdkSourceSha256=(Sha $sdkPath);sdkDeclarationSha256=(TextSha (Normalized $sdk.text));sdkPureVirtualMethods=$methods.Count;
        extraction='Unique anchored signature; directly following opening brace; balanced code braces with comments/quoted strings ignored; exact complete body copied with newline normalization only.'}

    $managerHeader=[IO.File]::ReadAllText((Join-Path $taskRoot 'Client/core/CModManager.h'))
    $managerText=[IO.File]::ReadAllText((Join-Path $taskRoot 'Client/core/CModManager.cpp'))
    Require ([regex]::IsMatch($managerHeader,'bool\s+IsLoaded\(\)\s+const\s+override\s*\{\s*return m_module\.IsOk\(\);\s*\}')) 'CModManager module-owner accessor changed.'
    Require ([regex]::IsMatch($managerHeader,'CClientBase\*\s+GetClient\(\)\s+override\s*\{\s*return m_client;\s*\}')) 'CModManager published-client accessor changed.'
    $stop=Extract-Definition $managerText '^void CModManager::TryStop\(\)[ \t]*\r?$'
    $post=Extract-Definition $managerText '^void CModManager::DoPulsePostFrame\(\)[ \t]*\r?$'
    $start=Extract-Definition $managerText '^bool CModManager::TryStart\(\)[ \t]*\r?$'
    Require ([regex]::IsMatch($stop.text,'(?s)if\s*\(m_client\)\s*\{\s*m_client->ClientShutdown\(\);\s*m_client\s*=\s*\{\};\s*\}.*m_module\.UnloadModule\(\);')) 'Actual teardown no longer proves client retirement before DLL release.'
    Require ($post.text.Contains('Start();') -and $post.text.Contains('Stop();') -and
        [regex]::Matches($post.text,'handleStateChange\(\);').Count -eq 2) 'Actual post-frame state-change contract changed.'
    Require ($start.text.IndexOf('m_module.LoadModule(') -ge 0 -and
        $start.text.IndexOf('m_module.LoadModule(') -lt $start.text.IndexOf('client->ClientInitialize(') -and
        $start.text.IndexOf('client->ClientInitialize(') -lt $start.text.IndexOf('m_client = client;')) 'Actual startup no longer proves loaded-module-before-client-publication ordering.'
    $record.managerProof=[ordered]@{moduleOwnerAccessor='m_module.IsOk()';clientAccessor='m_client';clientRetiredBeforeModuleRelease=$true;
        moduleLoadedBeforeClientPublication=$true;postFrameAppliesStartStopBeforeAndAfterCallbacks=$true;
        tryStopSha256=(TextSha (Normalized $stop.text));tryStartSha256=(TextSha (Normalized $start.text));
        postFrameSha256=(TextSha (Normalized $post.text));fixtureBoundary='Exact source contracts checked; manager methods are not compiled or executed by this minimal fixture.'}
    if ($SourceCheckOnly) { $record.success=$true }
    else {
        $env:TEMP=$scratchRoot; $env:TMP=$scratchRoot
        $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $discovery=Run-Owned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
        $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
        Require ($discovery.exitCode -eq 0 -and -not $discovery.timedOut -and [bool]$visualStudio) 'MSVC x64 tools are required.'
        $environmentScript=Join-Path $outputRoot 'capture-msvc.cmd'
        @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat')),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $record.environment=Run-Owned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
        Require ($record.environment.exitCode -eq 0 -and -not $record.environment.timedOut) 'MSVC environment initialization failed.'
        foreach ($entry in [IO.File]::ReadAllLines($record.environment.stdout)) {
            if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $env:TEMP=$scratchRoot; $env:TMP=$scratchRoot
        $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
        $record.compiler=[ordered]@{path=$compiler;sha256=(Sha $compiler)}
        $executable=Join-Path $outputRoot 'CoreIdleClientOwnershipTests.exe'
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MT','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+$outputRoot),('/I'+(Join-Path $taskRoot 'Shared/sdk')),(Join-Path $PSScriptRoot 'CoreIdleClientOwnershipTests.cpp'),
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $record.compile=Run-Owned $compiler $arguments $outputRoot 'compile' 600000
        Require ($record.compile.exitCode -eq 0 -and -not $record.compile.timedOut) 'Actual Core idle body fixture compilation failed.'
        $record.test=Run-Owned $executable @() $outputRoot 'test' 60000
        $stdout=[IO.File]::ReadAllText($record.test.stdout); $stderr=[IO.File]::ReadAllText($record.test.stderr)
        $summary=[regex]::Match($stdout,'Passed (\d+) actual Core idle client ownership checks \(64-bit\)')
        if ($summary.Success) { $record.checks=[int]$summary.Groups[1].Value }
        if ($ExpectRegressionFailure) {
            Require ($record.test.exitCode -eq 1 -and -not $record.test.timedOut -and
                $stderr.Contains('Core idle client ownership caught access violation') -and
                -not $stdout.Contains('Core idle client ownership tests completed')) 'Owned preimage did not reproduce the exact null-dispatch regression.'
        } else {
            Require ($record.test.exitCode -eq 0 -and -not $record.test.timedOut -and $record.checks -gt 0 -and
                $stdout.Contains('Core idle client ownership tests completed') -and
                $stdout.Contains('Owned SDK client address: 0x')) 'Actual Core idle ownership checks failed or completion/high-address proof is absent.'
            $record.actualCoreIdleBodyTested=$true; $record.actualSdkClientDeclarationTested=$true; $record.ownedHighAddressClientVerified=$true
        }
        $record.success=$true
    }
} catch { $record.failures += $_.Exception.ToString() }
finally {
    foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process') }
    foreach ($entry in $record.sourceHashes) {
        try { if ((Sha $entry.path) -ne $entry.sha256) { $record.sourcesUnchanged=$false } }
        catch { $record.sourcesUnchanged=$false; $record.failures += $_.Exception.ToString() }
    }
    foreach ($entry in $record.generated) {
        try { if ((Sha $entry.path) -ne $entry.sha256) { $record.generatedUnchanged=$false } }
        catch { $record.generatedUnchanged=$false; $record.failures += $_.Exception.ToString() }
    }
    try {
        # Delete only flat files created inside this exact task scratch directory.
        # Unexpected directories or links are retained for diagnosis.
        $resolved=(Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\')+'\'
        $parent=(Resolve-Path -LiteralPath (Join-Path $taskBuild 'tmp')).Path.TrimEnd('\')+'\'
        Require ($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase)) 'Scratch escaped the workspace Build/tmp directory.'
        Require (((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'Scratch became a link.'
        $entries=@(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            Require (-not $entry.PSIsContainer -and ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'Unexpected scratch directory or link retained.'
            Require ((Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolved,[StringComparison]::OrdinalIgnoreCase)) 'Scratch file escaped the owned directory.'
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved=$true
    } catch { $record.failures += $_.Exception.ToString() }
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.generatedUnchanged -and $record.scratchRemoved -and $record.failures.Count -eq 0
    $receipt=Join-Path $outputRoot 'receipt.json'
    $record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $receipt -Encoding utf8
    Write-Output $receipt
}
if (-not $record.success) { throw 'Core idle client ownership fixture failed; see the owned receipt.' }
