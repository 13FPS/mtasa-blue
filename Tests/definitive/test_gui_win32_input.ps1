#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the exact shared production translator for both Windows layouts.
# Its injection receiver creates no window, GUI, engine or installed artifact.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$outputRoot=Join-Path $taskRoot ('Build/mta-gui-win32-input/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$sourcePaths=@('Client/core/GuiWin32Input.h','Client/core/GuiWin32Input.cpp','Tests/definitive/GuiWin32InputTests.cpp','Tests/definitive/test_gui_win32_input.ps1',
    'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SString.h')
$sourcePaths+=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/sdk/gui') -File -Filter '*.h' | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName) })
$sourceHashes=@($sourcePaths | Sort-Object -Unique | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $_)).Hash.ToLowerInvariant()} })
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'MSVC Windows tools are required.' }
$developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$runs=@();$success=$true;$totalChecks=0
foreach ($architecture in @('x86','x64')) {
    $runRoot=Join-Path $outputRoot $architecture
    [IO.Directory]::CreateDirectory($runRoot) | Out-Null
    $compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand,$architecture)
    if ($LASTEXITCODE -ne 0) { throw "MSVC $architecture environment setup failed." }
    foreach ($entry in $compilerEnvironment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
    $executable=Join-Path $runRoot 'GuiWin32InputTests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDIRECTINPUT_VERSION=0x0800',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
        (Join-Path $PSScriptRoot 'GuiWin32InputTests.cpp'),(Join-Path $taskRoot 'Client/core/GuiWin32Input.cpp'),
        ('/Fo'+$runRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','user32.lib')
    [ordered]@{compiler=$compiler;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $compiler).Hash.ToLowerInvariant();arguments=$arguments} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runRoot 'compile-command.json') -Encoding utf8
    $compileOutput=@(& $compiler @arguments 2>&1);$compileExit=$LASTEXITCODE
    $compileOutput | Set-Content -LiteralPath (Join-Path $runRoot 'compile.log') -Encoding utf8
    $testExit=$null;$testCount=0;$timedOut=$false;$testText=''
    if ($compileExit -eq 0) {
        $stdout=Join-Path $runRoot 'stdout.log';$stderr=Join-Path $runRoot 'stderr.log'
        $process=Start-Process -FilePath $executable -WorkingDirectory $runRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $timedOut=-not $process.WaitForExit(30000)
        if ($timedOut) { $process.Kill();$process.WaitForExit() }
        $process.Refresh();$testExit=$process.ExitCode
        $testText=(Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue)+(Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue)
        if ($testText -match 'Passed (\d+) shared Win32 GUI input checks') { $testCount=[int]$Matches[1] }
    }
    $runSuccess=$compileExit -eq 0 -and $testExit -eq 0 -and $testCount -gt 0 -and -not $timedOut
    $success=$success -and $runSuccess;$totalChecks+=$testCount
    $runs+=@([ordered]@{architecture=$architecture;success=$runSuccess;compileExit=$compileExit;testExit=$testExit;timedOut=$timedOut;checks=$testCount;path=$runRoot})
}
$sourceUnchanged=$true
foreach ($entry in $sourceHashes) { if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot $entry.path)).Hash.ToLowerInvariant() -ne $entry.sha256) { $sourceUnchanged=$false } }
$success=$success -and $sourceUnchanged
[ordered]@{schema='mta_gui_win32_input_test_v1';success=$success;checks=$totalChecks;runs=$runs;sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;
    scope='Exact production Win32 translator and real public CGUI overload compiled for x86/x64; value receiver verifies routing, X2 identity, UTF16 text, owner rejection, focus/capture reset and exception cleanup. No window, renderer, GTA or gameplay input suppression.'} |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
Write-Output (Join-Path $outputRoot 'test-result.json')
if (-not $success) { throw "Shared GUI input checks failed; see $outputRoot" }
