#!/usr/bin/env pwsh
#requires -Version 7.0
# Production lifetime orchestration, with owned value-only owners and clocks.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $root ('Build/mta-gui-startup-reference/'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
$names=@('Client/core/DefinitiveGuiStartupReference.h','Tests/definitive/DefinitiveGuiStartupReferenceTests.cpp','Tests/definitive/test_definitive_gui_startup_reference.ps1')
$sources=@($names | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $root $_) -Algorithm SHA256).Hash.ToLowerInvariant()} })
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x86/x64 tools are required.' }
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$runs=@()
foreach ($arch in @('x86','x64')) {
    $dir=Join-Path $output $arch
    New-Item -ItemType Directory -Path $dir | Out-Null
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $dev,$arch)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment failed.' }
    foreach ($item in $environment) { if ($item -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$arch+'/cl.exe')
    $exe=Join-Path $dir 'gui_startup_lifecycle.exe'
    $compilerArguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MT', (Join-Path $PSScriptRoot 'DefinitiveGuiStartupReferenceTests.cpp'),('/Fo'+$dir+'/'),('/Fe'+$exe))
    $compile=@(& $compiler @compilerArguments 2>&1)
    $compileExit=$LASTEXITCODE
    $compile | Set-Content -LiteralPath (Join-Path $dir 'compile.log') -Encoding utf8
    $testExit=$null;$checks=$null;$timedOut=$false
    $stdout=Join-Path $dir 'stdout.log';$stderr=Join-Path $dir 'stderr.log'
    if ($compileExit -eq 0) {
        $process=Start-Process -FilePath $exe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $timedOut=-not $process.WaitForExit(30000)
        if ($timedOut) { $process.Kill();$process.WaitForExit() }
        $testExit=$process.ExitCode
        $process.Dispose()
        $text=(@(Get-Content -LiteralPath $stdout)+@(Get-Content -LiteralPath $stderr))-join "`n"
        if ($text -match 'Passed (\d+) Core GUI startup lifecycle checks') { $checks=[int]$Matches[1] }
    }
    $runs+=@([ordered]@{architecture=$arch;compileExit=$compileExit;testExit=$testExit;checks=$checks;timedOut=$timedOut;directory=$dir;
        compiler=$compiler;compilerSha256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant();
        success=$compileExit -eq 0 -and $testExit -eq 0 -and $null -ne $checks -and -not $timedOut})
}
$unchanged=$true
foreach ($item in $sources) { if ((Get-FileHash -LiteralPath (Join-Path $root $item.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256) { $unchanged=$false } }
$success=$unchanged -and $runs.Count -eq 2 -and @($runs|Where-Object { -not $_.success }).Count -eq 0
$result=[ordered]@{success=$success;recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=$sources;sourceUnchanged=$unchanged;runs=$runs;
    gameLaunched=$false;installedFilesChanged=$false;gpuWorkIssued=$false;scope='Production GUI startup lifetime order with owned value-only owners/clocks; no COM or renderer execution.'}
$path=Join-Path $output 'test-result.json'
$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $path -Encoding utf8
[ordered]@{success=$success;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant();runs=$runs}|ConvertTo-Json -Compress -Depth 4
if (-not $success) { throw "Core GUI startup lifecycle failed: $output" }
