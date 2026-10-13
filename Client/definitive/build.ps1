#!/usr/bin/env pwsh
# A separate build prevents experimental x64 code from changing the classic MTA solution.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outputDirectory = Join-Path $repositoryRoot 'Build/definitive-online'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) {
    throw 'Visual Studio Installer/vswhere.exe was not found. Install the C++ build tools.'
}
$visualStudioPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudioPath) {
    throw 'No Visual Studio installation with the x64 C++ tools was found.'
}
$developerCommand = Join-Path $visualStudioPath 'Common7/Tools/VsDevCmd.bat'
# Capture the configured compiler environment without printing inherited environment values.
$environmentCommand = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
$compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
if ($LASTEXITCODE -ne 0) {
    throw 'Could not initialize the Visual Studio x64 compiler environment.'
}
foreach ($environmentEntry in $compilerEnvironment) {
    if ($environmentEntry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$compilerPath = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
if (-not (Test-Path -LiteralPath $compilerPath)) {
    throw 'The selected Visual Studio installation has no x64 compiler.'
}
$commonArguments = @('/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/O2', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN',
    ('/I' + $PSScriptRoot), ('/I' + (Join-Path $repositoryRoot 'Shared/sdk')))
Push-Location $outputDirectory
try {
    & $compilerPath @commonArguments /c (Join-Path $PSScriptRoot 'TransportClient.cpp') /FoTransportClient.obj
    if ($LASTEXITCODE -ne 0) { throw 'Transport compilation failed.' }
    & $compilerPath @commonArguments (Join-Path $PSScriptRoot 'Probe.cpp') TransportClient.obj /Fede_client_probe.exe /link ws2_32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Client probe compilation failed.' }
    & $compilerPath @commonArguments /DDE_BRIDGE_EXPORTS /LD (Join-Path $PSScriptRoot 'Bridge.cpp') (Join-Path $PSScriptRoot 'BuildGuard.cpp') TransportClient.obj /Fede_client_bridge.dll /link ws2_32.lib bcrypt.lib
    if ($LASTEXITCODE -ne 0) { throw 'Client bridge compilation failed.' }
    & $compilerPath @commonArguments (Join-Path $repositoryRoot 'Tests/definitive/ProtocolTests.cpp') /Fede_protocol_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Protocol test compilation failed.' }
    & ./de_protocol_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Protocol tests failed.' }
} finally {
    Pop-Location
}
Write-Host "Built standalone x64 client components in $outputDirectory"
