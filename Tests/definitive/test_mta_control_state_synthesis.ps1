#!/usr/bin/env pwsh
#requires -Version 7.0
# Compare shared synthesis with the frozen original policy on both client
# architectures. A pure logical-control check cannot certify DE engine input.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/definitive-control-state-synthesis/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h', 'Shared/sdk/CControllerState.h',
    'Client/core/CKeyBinds.cpp', 'Client/core/CKeyBinds.h', 'Client/sdk/game/CControllerConfigManager.h',
    'Tests/definitive/MtaControlStateSynthesisTests.cpp', 'Tests/definitive/test_mta_control_state_synthesis.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    if (-not (Test-Path -LiteralPath $taskPath -PathType Leaf)) { throw "Missing synthesis source: $_" }
    [ordered]@{ path=$_; exists=$true; sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() }
})

# Read exact immutable Git blob bytes instead of using current CKeyBinds as the
# expected result. The fixture names these same blobs, so extraction edits or
# platform newline conversion cannot silently replace the independent oracle.
$oracleBlobs = [ordered]@{
    'Client/core/CKeyBinds.cpp'='3a66d5f3e51e28a91a91fc721a82c0f4cc99304f'
    'Client/core/CKeyBinds.h'='ecde1221b791237fe06f539a1ce46e33d6223938'
    'Client/sdk/game/CControllerConfigManager.h'='91b280f3b23fb473475eda79b0a6959bb46cb052'
}
$oracleRecords = @()
foreach ($name in $oracleBlobs.Keys) {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Get-Command git).Source
    $startInfo.ArgumentList.Add('cat-file')
    $startInfo.ArgumentList.Add('blob')
    $startInfo.ArgumentList.Add($oracleBlobs[$name])
    $startInfo.WorkingDirectory = $taskRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $gitProcess = [System.Diagnostics.Process]::Start($startInfo)
    $capture = [System.IO.MemoryStream]::new()
    $gitProcess.StandardOutput.BaseStream.CopyTo($capture)
    $gitError = $gitProcess.StandardError.ReadToEnd()
    $gitProcess.WaitForExit()
    if ($gitProcess.ExitCode -ne 0) { throw "Original oracle blob unavailable: $name; $gitError" }
    $oracleBytes = $capture.ToArray()
    $capture.Dispose()
    $gitProcess.Dispose()
    $blobHash = [System.Security.Cryptography.IncrementalHash]::CreateHash([System.Security.Cryptography.HashAlgorithmName]::SHA1)
    $blobHash.AppendData([Text.Encoding]::ASCII.GetBytes("blob $($oracleBytes.Length)`0"))
    $blobHash.AppendData($oracleBytes)
    $actualBlob = [Convert]::ToHexString($blobHash.GetHashAndReset()).ToLowerInvariant()
    $blobHash.Dispose()
    if ($actualBlob -ne $oracleBlobs[$name]) { throw "Original oracle blob bytes changed: $name" }
    $oraclePath = Join-Path $outputRoot ('original-oracle/' + $name)
    New-Item -ItemType Directory -Path (Split-Path -Parent $oraclePath) -Force | Out-Null
    [IO.File]::WriteAllBytes($oraclePath, $oracleBytes)
    $oracleRecords += [ordered]@{ sourcePath=$name; gitBlob=$actualBlob; saved=$oraclePath;
        sha256=(Get-FileHash -LiteralPath $oraclePath -Algorithm SHA256).Hash.ToLowerInvariant() }
}

$results = @()
$failure = $null
foreach ($architecture in @('x86', 'x64')) {
    $outputDirectory = Join-Path $outputRoot $architecture
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand, $architecture
    $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
    $executable = Join-Path $outputDirectory 'mta_control_state_synthesis_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I' + (Join-Path $taskRoot 'Shared/sdk')), (Join-Path $PSScriptRoot 'MtaControlStateSynthesisTests.cpp'),
        ('/Fo' + (Join-Path $outputDirectory 'test.obj')), ('/Fe' + $executable))
    $compilerOutput = @(& $compiler @arguments 2>&1)
    $compilerExit = $LASTEXITCODE
    $compileLog = Join-Path $outputDirectory 'compile.log'
    $compilerOutput | Set-Content -LiteralPath $compileLog -Encoding utf8
    $testExit = $null
    $testCount = $null
    $frameCount = $null
    $testLog = Join-Path $outputDirectory 'test.log'
    if ($compilerExit -eq 0) {
        $testOutput = @(& $executable 2>&1)
        $testExit = $LASTEXITCODE
        $testOutput | Set-Content -LiteralPath $testLog -Encoding utf8
        $testText = $testOutput -join "`n"
        if ($testText -match 'Passed (\d+) original MTA control-synthesis checks across (\d+) differential frames') {
            $testCount = [long]$Matches[1]
            $frameCount = [long]$Matches[2]
        }
    }
    $results += [ordered]@{ architecture=$architecture; compilerExit=$compilerExit; testExit=$testExit;
        checks=$testCount; differentialFrames=$frameCount; executable=$executable;
        executableSha256=$(if (Test-Path -LiteralPath $executable) { (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
        compileLog=$compileLog; compileLogSha256=(Get-FileHash -LiteralPath $compileLog -Algorithm SHA256).Hash.ToLowerInvariant();
        testLog=$testLog; testLogSha256=$(if (Test-Path -LiteralPath $testLog) { (Get-FileHash -LiteralPath $testLog -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
    if ($compilerExit -ne 0 -or $testExit -ne 0 -or $null -eq $testCount -or $null -eq $frameCount) {
        $failure = "$architecture compilation/tests failed; see $outputDirectory"
        break
    }
}
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    $taskPath = Join-Path $taskRoot $entry.path
    if (-not (Test-Path -LiteralPath $taskPath) -or
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        $sourceUnchanged = $false
    }
}
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=($null -eq $failure -and $sourceUnchanged); sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged;
    originalOracle=$oracleRecords; results=$results; failure=$failure;
    scope='Original configured-action synthesis only; all24fields/history, all32contexts,44action singletons/ordered pairs,64history states, deterministic sequences.';
    gameLaunched=$false; installationChanged=$false; existingBridgeRebuilt=$false;
    engineInputBindingVerified=$false; fullMtaClientBuilt=$false }
$record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Shared synthesis preservation failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 9 -Compress
