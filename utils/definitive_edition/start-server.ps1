#!/usr/bin/env pwsh
#requires -Version 7.0
# Restart the temporary loopback server after a reboot without launching the game.
[CmdletBinding()]
param(
    [ValidateRange(60, 7200)][int]$DurationSeconds = 1800,
    [string]$PythonPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This helper requires Windows and PowerShell 7.' }

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$serverScript = (Resolve-Path -LiteralPath (Join-Path $repositoryRoot 'Server/definitive/server.py')).Path
$serverHost = '127.0.0.1'
$serverPort = 22040

function Quote-WindowsArgument([string]$Value) {
    # Start-Process joins ArgumentList into a command line. Escaping quotes and
    # trailing backslashes keeps paths with spaces intact for Python's parser.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}

function Write-DurableJson([string]$Path, [object]$Value) {
    # A restart must not leave partially written PID metadata that could later
    # be mistaken for a different process which reused that PID.
    $temporary = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 8) + "`n")
    try {
        $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) }
        finally { $stream.Dispose() }
        [IO.File]::Move($temporary, $Path, $true)
    } finally {
        if ([IO.File]::Exists($temporary)) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Get-ServerEndpoints {
    # Inspect every bind on this port, including wildcard/IPv6 addresses, so a
    # conflicting process is never silently displaced or treated as our server.
    @(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object { $_.LocalPort -eq $serverPort })
}

if (-not $PythonPath) {
    $PythonPath = (Get-Command python.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
}
$pythonExecutable = (Resolve-Path -LiteralPath $PythonPath).Path
$probeCode = 'import json,struct,sys; print(json.dumps({"executable":sys.executable,"bits":struct.calcsize("P")*8,"version":sys.version.split()[0]}))'
$probeOutput = @(& $pythonExecutable -B -c $probeCode)
if ($LASTEXITCODE -ne 0 -or $probeOutput.Count -ne 1) { throw 'The selected Python runtime could not be identified.' }
$pythonInfo = $probeOutput[0] | ConvertFrom-Json -ErrorAction Stop
if ($pythonInfo.bits -notin @(32, 64)) { throw 'Python reported an unsupported pointer size.' }
# The server itself can use either architecture; record the actual runtime so
# the launcher is not confused with the separate x64 native game bridge.
$pythonExecutable = (Resolve-Path -LiteralPath $pythonInfo.executable).Path

$occupied = @(Get-ServerEndpoints)
if ($occupied.Count) {
    $owners = ($occupied | Select-Object -ExpandProperty OwningProcess -Unique) -join ', '
    throw "UDP port $serverPort is already bound (PID $owners). Leave that process running; this helper does not reuse or stop existing processes."
}

$sessionId = [Guid]::NewGuid().ToString('N')
$createdAt = [DateTimeOffset]::Now
$evidenceRoot = Join-Path $repositoryRoot ('Build/definitive-online-test/' + $createdAt.ToString('yyyyMMdd-HHmmss') + '-' + $sessionId)
New-Item -ItemType Directory -Path $evidenceRoot -ErrorAction Stop | Out-Null
$stdoutPath = Join-Path $evidenceRoot 'server.stdout.jsonl'
$stderrPath = Join-Path $evidenceRoot 'server.stderr.log'
$sessionPath = Join-Path $evidenceRoot 'session.json'
$currentPath = Join-Path (Split-Path -Parent $evidenceRoot) 'current-session.json'
$arguments = @('-B', '-u', $serverScript, '--host', $serverHost, '--port', [string]$serverPort, '--duration', [string]$DurationSeconds)
$argumentLine = ($arguments | ForEach-Object { Quote-WindowsArgument $_ }) -join ' '
$serverProcess = $null
$processHandle = $null
$launchedStartTimeUtc = $null
$session = [ordered]@{
    helperVersion = 1
    sessionId = $sessionId
    status = 'starting'
    createdAt = $createdAt.ToString('o')
    expiresApproximatelyAt = $createdAt.AddSeconds($DurationSeconds).ToString('o')
    serverPid = $null
    serverStartTimeUtc = $null
    serverScript = $serverScript
    python = $pythonExecutable
    pythonBits = $pythonInfo.bits
    pythonVersion = $pythonInfo.version
    serverArguments = $arguments
    serverCommandLine = $null
    evidenceDirectory = $evidenceRoot
    stdoutFile = $stdoutPath
    stderrFile = $stderrPath
    host = $serverHost
    port = $serverPort
    serverDurationSeconds = $DurationSeconds
    gameLaunched = $false
    syntheticPeerStarted = $false
    multiplayerVerified = $false
    acceptance = 'One real game client submits coordinates and receives its own validated snapshot echo'
}

try {
    Write-DurableJson $sessionPath $session
    $serverProcess = Start-Process -FilePath $pythonExecutable -ArgumentList $argumentLine -WorkingDirectory $repositoryRoot `
        -WindowStyle Hidden -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru
    # Retain the handle to this specific process instance before querying by PID.
    # A stale PID in current-session.json is never sufficient to stop anything.
    $processHandle = $serverProcess.SafeHandle
    $launchedStartTimeUtc = $serverProcess.StartTime.ToUniversalTime()
    $session.serverPid = $serverProcess.Id
    $session.serverStartTimeUtc = $launchedStartTimeUtc.ToString('o')
    $session.expiresApproximatelyAt = [DateTimeOffset]::new($launchedStartTimeUtc).AddSeconds($DurationSeconds).ToString('o')

    $identity = Get-CimInstance Win32_Process -Filter "ProcessId = $($serverProcess.Id)" -ErrorAction Stop
    $expectedCommandLines = @(
        ($pythonExecutable + ' ' + $argumentLine),
        ((Quote-WindowsArgument $pythonExecutable) + ' ' + $argumentLine)
    )
    if ($null -eq $identity -or
        -not [string]::Equals($identity.ExecutablePath, $pythonExecutable, [StringComparison]::OrdinalIgnoreCase) -or
        $identity.CommandLine -cnotin $expectedCommandLines -or
        [Math]::Abs(($identity.CreationDate.ToUniversalTime() - $launchedStartTimeUtc).TotalMilliseconds) -gt 100) {
        throw 'The newly launched server process could not be verified by executable, command line, and creation time.'
    }
    $session.serverCommandLine = $identity.CommandLine
    Write-DurableJson $sessionPath $session

    $deadline = [DateTimeOffset]::UtcNow.AddSeconds(5)
    $ready = $false
    do {
        $serverProcess.Refresh()
        if ($serverProcess.HasExited) { throw "The server exited during startup (exit code $($serverProcess.ExitCode)); see $stderrPath." }
        $listening = $false
        if ([IO.File]::Exists($stdoutPath)) {
            # Start-Process keeps its redirected writer open. Share writes while
            # inspecting startup output so readiness checks do not reject a live
            # server merely because its log file is still being written.
            $logStream = [IO.File]::Open($stdoutPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            $logReader = [IO.StreamReader]::new($logStream)
            try {
                for ($lineIndex = 0; $lineIndex -lt 32 -and -not $logReader.EndOfStream; $lineIndex++) {
                    $line = $logReader.ReadLine()
                    # A concurrent write may expose only part of the last line;
                    # retry the same file on the next poll rather than fail startup.
                    try { $event = $line | ConvertFrom-Json -AsHashtable -ErrorAction Stop } catch { continue }
                    if ($event -is [Collections.IDictionary] -and $event['event'] -eq 'listening' -and
                        $event['host'] -eq $serverHost -and $event['port'] -eq $serverPort) {
                        $listening = $true
                        break
                    }
                }
            } finally { $logReader.Dispose(); $logStream.Dispose() }
        }
        if ($listening) {
            $ownedEndpoint = @(Get-ServerEndpoints | Where-Object {
                $_.LocalAddress -eq $serverHost -and $_.OwningProcess -eq $serverProcess.Id
            })
            $ready = $ownedEndpoint.Count -eq 1
        }
        if (-not $ready -and [DateTimeOffset]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    } while (-not $ready -and [DateTimeOffset]::UtcNow -lt $deadline)
    if (-not $ready) { throw "The server did not confirm its loopback endpoint within five seconds; see $stderrPath." }

    $session.status = 'listening'
    $session.readyAt = [DateTimeOffset]::Now.ToString('o')
    Write-DurableJson $sessionPath $session
    # Publish the pointer only after both independent readiness checks pass.
    Write-DurableJson $currentPath $session
    [pscustomobject]@{
        status = $session.status
        serverPid = $session.serverPid
        endpoint = "${serverHost}:$serverPort"
        pythonBits = $session.pythonBits
        expiresApproximatelyAt = $session.expiresApproximatelyAt
        evidenceDirectory = $evidenceRoot
        currentSessionFile = $currentPath
        gameLaunched = $false
        syntheticPeerStarted = $false
    }
} catch {
    $failure = $_
    $cleanup = 'No process was launched.'
    if ($null -ne $serverProcess -and $null -ne $processHandle -and -not $processHandle.IsInvalid) {
        try {
            $serverProcess.Refresh()
            if ($serverProcess.HasExited) { $cleanup = 'The newly launched process already exited.' }
            elseif ($serverProcess.StartTime.ToUniversalTime() -eq $launchedStartTimeUtc) {
                # Kill through the retained handle only after confirming the
                # instance start time; do not act on an arbitrary live PID.
                $serverProcess.Kill()
                $serverProcess.WaitForExit(5000) | Out-Null
                $cleanup = 'Stopped the newly launched process after startup failed.'
            } else { $cleanup = 'Process identity changed; no stop was attempted.' }
        } catch { $cleanup = 'Could not stop the new process: ' + $_.Exception.Message }
    }
    $session.status = 'failed'
    $session.failure = $failure.Exception.Message
    $session.cleanup = $cleanup
    try { Write-DurableJson $sessionPath $session } catch { Write-Warning ('Could not preserve startup failure metadata: ' + $_.Exception.Message) }
    throw $failure
} finally {
    if ($null -ne $serverProcess) { $serverProcess.Dispose() }
}
