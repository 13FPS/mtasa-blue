#!/usr/bin/env pwsh
#requires -Version 7.0
# Relink immutable normal Core objects with test-only ownership checks. No game,
# installed package, configuration or network workload is opened by this host.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreBuildReceipt,
    [string]$CoreObjectDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'This fixture requires native x64 PowerShell.' }
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $CoreObjectDirectory) { $CoreObjectDirectory=Join-Path $taskRoot 'Build/obj/x64/Release/Client Core' }
$objectRoot=(Resolve-Path -LiteralPath $CoreObjectDirectory).Path
$receiptPath=(Resolve-Path -LiteralPath $CoreBuildReceipt).Path
$certificate=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if ($certificate.success -ne $true) { throw 'A successful normal Core certificate is required.' }
# Normal Core receipts bind their declared inputs through success and the exact
# source_hashes map. Some other module receipts additionally publish this flag;
# honor an explicit failure without requiring a field Core never supplied.
$unchangedProperty=$certificate.PSObject.Properties['sources_unchanged']
if ($null -ne $unchangedProperty -and $unchangedProperty.Value -ne $true) { throw 'Normal certificate explicitly reports changed sources.' }
$sourceMapProperty=$certificate.PSObject.Properties['source_hashes']
if ($null -eq $sourceMapProperty -or $null -eq $sourceMapProperty.Value -or @($sourceMapProperty.Value.PSObject.Properties).Count -eq 0) {
    throw 'Normal certificate must declare nonempty source hashes.'
}
$sourceCountProperty=$certificate.PSObject.Properties['source_count']
if ($null -ne $sourceCountProperty -and $sourceCountProperty.Value -ne @($sourceMapProperty.Value.PSObject.Properties).Count) {
    throw 'Normal certificate source count does not match its declared hashes.'
}
foreach ($property in $certificate.source_hashes.PSObject.Properties) {
    if ($property.Value -notmatch '^[a-fA-F0-9]{64}$') { throw "Invalid declared source hash: $($property.Name)" }
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $property.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $property.Value) {
        throw "Normal Core declared source differs: $($property.Name)"
    }
}
$outputRoot=Join-Path $taskRoot ('Build/mta-frontend-version-updater/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$sourcePaths=@('Tests/definitive/FrontendVersionUpdaterTests.cpp','Tests/definitive/FrontendVersionUpdaterHost.cpp',
    'Tests/definitive/test_frontend_version_updater.ps1','Client/core/CVersionUpdater.h','Client/core/CVersionUpdater.cpp',
    'Client/core/CVersionUpdater.Util.hpp','Shared/sdk/SharedUtil.Thread.h','vendor/pthreads/include/pthread.h','Client/sdk/net/CNet.h')
$sourceHashes=[ordered]@{}
foreach ($name in $sourcePaths) {
    $source=Join-Path $taskRoot $name
    $sourceHashes[$name]=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    $saved=Join-Path $outputRoot ('sources/'+$name)
    [IO.Directory]::CreateDirectory((Split-Path -Parent $saved)) | Out-Null
    Copy-Item -LiteralPath $source -Destination $saved
}
$artifacts=@()
foreach ($name in @('core.dll','pthread.dll')) {
    $matches=@($certificate.artifacts | Where-Object { [IO.Path]::GetFileName($_.saved) -eq $name })
    if ($matches.Count -ne 1) { throw "Normal certificate must identify one exact $name artifact." }
    $artifact=$matches[0]
    if ((Get-FileHash -LiteralPath $artifact.saved -Algorithm SHA256).Hash.ToLowerInvariant() -ne $artifact.sha256 -or
        (Get-Item -LiteralPath $artifact.saved).Length -ne $artifact.bytes) { throw "Saved normal artifact differs: $name" }
    $saved=Join-Path $outputRoot ('normal-'+$name)
    if ($name -eq 'pthread.dll') { $saved=Join-Path $outputRoot $name }
    Copy-Item -LiteralPath $artifact.saved -Destination $saved
    $artifacts+=@([ordered]@{name=$name;source=$artifact.saved;saved=$saved;sha256=$artifact.sha256;bytes=$artifact.bytes})
}
$compileTlog=Join-Path $objectRoot 'Client Core.tlog/CL.command.1.tlog'
$linkTlog=Join-Path $objectRoot 'Client Core.tlog/link.command.1.tlog'
$compileLines=@(Get-Content -LiteralPath $compileTlog)
$compileLine=$null
for ($index=0;$index -lt $compileLines.Count-1;$index++) {
    if ($compileLines[$index].StartsWith('^') -and $compileLines[$index] -match '(?i)\\CVERSIONUPDATER\.CPP$') { $compileLine=$compileLines[$index+1];break }
}
if (-not $compileLine -or $compileLine -notmatch 'MTA_WITH_DEFINITIVE_ADAPTER' -or $compileLine -notmatch '/MT\b') {
    throw 'Normal opt-in Release/x64 updater compiler metadata is required.'
}
$linkLines=@(Get-Content -LiteralPath $linkTlog | Where-Object { -not $_.StartsWith('^') -and $_.Trim() })
if ($linkLines.Count -lt 90 -or $linkLines[0] -notmatch '/DLL\b' -or $linkLines[0] -notmatch '/DELAYLOAD:d3dx9_43.dll') {
    throw 'Complete normal Core linker metadata with deferred legacy rendering is required.'
}
$inputHashes=[ordered]@{}
foreach ($line in $linkLines) {
    foreach ($match in [regex]::Matches($line,'(?i)(?<!:)"([^"]+\.(?:obj|res|lib))"')) {
        $path=$match.Groups[1].Value
        if (-not [IO.Path]::IsPathRooted($path)) { $path=Join-Path (Join-Path $taskRoot 'Build') $path }
        $path=(Resolve-Path -LiteralPath $path).Path
        $inputHashes[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$normalOutputs=[ordered]@{}
foreach ($option in @('OUT','PDB','IMPLIB')) {
    $match=[regex]::Match($linkLines[0],('(?i)/'+$option+':"([^"]+)"'))
    if (-not $match.Success) { throw "Normal linker $option output metadata is required." }
    $path=$match.Groups[1].Value
    if (-not [IO.Path]::IsPathRooted($path)) { $path=Join-Path (Join-Path $taskRoot 'Build') $path }
    $path=(Resolve-Path -LiteralPath $path).Path
    $normalOutputs[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($option -eq 'OUT' -and $normalOutputs[$path] -ne ($artifacts | Where-Object name -eq 'core.dll').sha256) {
        throw 'Current normal linker output is not the certified Core artifact.'
    }
}
Copy-Item -LiteralPath $compileTlog -Destination (Join-Path $outputRoot 'normal-compile.command.tlog')
Copy-Item -LiteralPath $linkTlog -Destination (Join-Path $outputRoot 'normal-link.command.tlog')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'MSVC x64 tools are required.' }
$developer=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developer)
if ($LASTEXITCODE -ne 0) { throw 'MSVC environment setup failed.' }
foreach ($entry in $compilerEnvironment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$linker=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/link.exe'
$fixtureSource=Join-Path $PSScriptRoot 'FrontendVersionUpdaterTests.cpp'
$normalSource=Join-Path $taskRoot 'Client/core/CVersionUpdater.cpp'
$fixtureObject=Join-Path $outputRoot 'FrontendVersionUpdaterTests.obj'
$fixtureDll=Join-Path $outputRoot 'core-frontend-updater-tests.dll'
$compileLine=$compileLine -replace '(?i)/Y[uc]"[^"]*"\s*',''
$compileLine=$compileLine -replace '(?i)/Fp"[^"]*"\s*',''
$compileLine=$compileLine -replace '(?i)/Fo"[^"]*"',('/Fo"'+$fixtureObject+'"')
$compileLine=$compileLine -replace '(?i)/Fd"[^"]*"',('/Fd"'+(Join-Path $outputRoot 'fixture-compile.pdb')+'"')
$compileLine=$compileLine -replace ('(?i)'+[regex]::Escape($normalSource)+'\s*$'),('"'+$fixtureSource+'"')
if ($compileLine -notmatch [regex]::Escape($fixtureSource) -or $compileLine -match [regex]::Escape($normalSource)) {
    throw 'Fixture compiler source replacement failed.'
}
$compileResponse=Join-Path $outputRoot 'compile-fixture.rsp'
[IO.File]::WriteAllText($compileResponse,$compileLine,[Text.UTF8Encoding]::new($false))
$linkLines[0]=$linkLines[0] -replace '(?i)/OUT:"[^"]*"',('/OUT:"'+$fixtureDll+'"')
$linkLines[0]=$linkLines[0] -replace '(?i)/PDB:"[^"]*"',('/PDB:"'+(Join-Path $outputRoot 'core-frontend-updater-tests.pdb')+'"')
$linkLines[0]=$linkLines[0] -replace '(?i)/IMPLIB:"[^"]*"',('/IMPLIB:"'+(Join-Path $outputRoot 'core-frontend-updater-tests.lib')+'"')
$linkLines[0]=$linkLines[0] -replace '(?i)/MAP(?::"[^"]*"|:[^\s]+)?(?=\s|$)',('/MAP:"'+(Join-Path $outputRoot 'core-frontend-updater-tests.map')+'"')
if ($linkLines[0] -match '(?i)/(?:ILK|PGD|LTCGOUT|MANIFESTFILE):') { throw 'Additional normal linker outputs require fixture redirection.' }
$linkResponse=Join-Path $outputRoot 'link-fixture.rsp'
[IO.File]::WriteAllText($linkResponse,($linkLines -join "`n")+"`n"+'"'+$fixtureObject+'"',[Text.UTF8Encoding]::new($false))
$hostExecutable=Join-Path $outputRoot 'FrontendVersionUpdaterHost.exe'
$hostArguments=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    (Join-Path $PSScriptRoot 'FrontendVersionUpdaterHost.cpp'),('/Fo'+(Join-Path $outputRoot 'FrontendVersionUpdaterHost.obj')),('/Fe'+$hostExecutable),'/link','user32.lib')
[ordered]@{compiler=$compiler;compilerSha256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant();linker=$linker;
    linkerSha256=(Get-FileHash -LiteralPath $linker -Algorithm SHA256).Hash.ToLowerInvariant();hostArguments=$hostArguments} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'toolchain.json') -Encoding utf8
$compileExit=$linkExit=$hostCompileExit=$hostExit=$null;$hostResult=$null;$timedOut=$false
Push-Location -LiteralPath (Join-Path $taskRoot 'Build')
try {
    $compileOutput=@(& $compiler ('@'+$compileResponse) 2>&1);$compileExit=$LASTEXITCODE
    $compileOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'fixture-compile.log') -Encoding utf8
    if ($compileExit -eq 0) {
        $linkOutput=@(& $linker ('@'+$linkResponse) 2>&1);$linkExit=$LASTEXITCODE
        $linkOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'fixture-link.log') -Encoding utf8
    }
    if ($linkExit -eq 0) {
        $hostOutput=@(& $compiler @hostArguments 2>&1);$hostCompileExit=$LASTEXITCODE
        $hostOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'host-compile.log') -Encoding utf8
    }
} finally { Pop-Location }
if ($hostCompileExit -eq 0) {
    $stdout=Join-Path $outputRoot 'stdout.log';$stderr=Join-Path $outputRoot 'stderr.log'
    $process=Start-Process -FilePath $hostExecutable -ArgumentList ('"'+$fixtureDll+'"') -WorkingDirectory $outputRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $timedOut=-not $process.WaitForExit(30000)
    if ($timedOut) { $process.Kill();$process.WaitForExit() }
    $process.Refresh();$hostExit=$process.ExitCode
    $lines=@(Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue | Where-Object { $_.StartsWith('{') })
    if ($lines.Count -eq 1) { $hostResult=$lines[0] | ConvertFrom-Json }
}
$sourceUnchanged=$true;$inputsUnchanged=$true;$outputsUnchanged=$true
foreach ($name in $sourceHashes.Keys) { if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHashes[$name]) { $sourceUnchanged=$false } }
foreach ($property in $certificate.source_hashes.PSObject.Properties) { if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $property.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $property.Value) { $sourceUnchanged=$false } }
foreach ($path in $inputHashes.Keys) { if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputHashes[$path]) { $inputsUnchanged=$false } }
foreach ($path in $normalOutputs.Keys) { if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $normalOutputs[$path]) { $outputsUnchanged=$false } }
$success=$compileExit -eq 0 -and $linkExit -eq 0 -and $hostCompileExit -eq 0 -and $hostExit -eq 0 -and -not $timedOut -and
    $null -ne $hostResult -and $hostResult.success -eq $true -and $hostResult.continued_after_free_library -eq $true -and
    $sourceUnchanged -and $inputsUnchanged -and $outputsUnchanged
[ordered]@{schema='mta_frontend_version_updater_test_v1';success=$success;compileExit=$compileExit;linkExit=$linkExit;hostCompileExit=$hostCompileExit;
    hostExit=$hostExit;timedOut=$timedOut;hostResult=$hostResult;sourceHashes=$sourceHashes;sourceUnchanged=$sourceUnchanged;
    normalReceipt=[ordered]@{path=$receiptPath;sha256=(Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $receiptPath).Length};
    normalInputHashes=$inputHashes;normalInputsUnchanged=$inputsUnchanged;normalOutputHashes=$normalOutputs;normalOutputsUnchanged=$outputsUnchanged;artifacts=$artifacts;
    scope='Actual frontend-owned updater constructor/API linked from exact current normal Core objects, with owner-thread and conflict checks, all async admission refused, release/reacquisition and real DLL unload. No Core construction, XML parser, GUI, HTTP requests, server-cache saves, GTA or full MTA startup.'} |
    ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
Write-Output (Join-Path $outputRoot 'test-result.json')
if (-not $success) { throw "Actual frontend updater fixture failed; see $outputRoot" }
