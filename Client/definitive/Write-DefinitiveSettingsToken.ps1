#requires -Version 7.0
# Commit a validated settings edit without re-encoding the surrounding file.
param(
    [Parameter(Mandatory)][string]$Path,
    [Parameter(Mandatory)][byte[]]$Bytes,
    [Parameter(Mandatory)][Text.Encoding]$Encoding,
    [Parameter(Mandatory)][string]$Text,
    [Parameter(Mandatory)][int]$Index,
    [Parameter(Mandatory)][string]$OldToken,
    [Parameter(Mandatory)][string]$Replacement,
    [string[]]$GameProcesses = @('SanAndreas'),
    [string]$SourcePath,
    [byte[]]$SourceBytes
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Work backwards from EOF so the encoding's optional BOM stays in the prefix.
$end = $Bytes.Length - $Encoding.GetByteCount($Text.Substring($Index + $OldToken.Length))
$token = $Encoding.GetBytes($OldToken)
$start = $end - $token.Length
if ($start -lt 0 -or [Convert]::ToBase64String($Bytes, $start, $token.Length) -cne [Convert]::ToBase64String($token)) {
    throw 'Could not identify the encoded GTA settings token.'
}
$temp = Join-Path ([IO.Path]::GetDirectoryName($Path)) ('.mta-control-' + [guid]::NewGuid().ToString('N') + '.tmp')
try {
    $output = [IO.File]::Open($temp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $output.Write($Bytes, 0, $start)
        $updated = $Encoding.GetBytes($Replacement)
        $output.Write($updated, 0, $updated.Length)
        $output.Write($Bytes, $end, $Bytes.Length - $end)
        $output.Flush($true)
    } finally {
        $output.Dispose()
    }
    # Serialize imports from this tool. A stale import must re-read the current profile.
    $commitLock = [Threading.Mutex]::new($false, 'MTA.DE.ControlPreferenceCommit')
    $acquired = $false
    try {
        try {
            $acquired = $commitLock.WaitOne(10000)
        } catch [Threading.AbandonedMutexException] {
            $acquired = $true
        }
        if (!$acquired) { throw 'Another MTA settings import is busy. Retry using the current file.' }
        if (Get-Process -Name $GameProcesses -ErrorAction SilentlyContinue) { throw 'Close GTA San Andreas Definitive Edition before applying the preference.' }
        if ($SourcePath -and [Convert]::ToBase64String([IO.File]::ReadAllBytes($SourcePath)) -cne [Convert]::ToBase64String($SourceBytes)) {
            throw 'MTA bindings changed during the update. Retry using the current file.'
        }
        if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Path)) -cne [Convert]::ToBase64String($Bytes)) {
            throw 'GTA settings changed during the update. Retry using the current file.'
        }
        [IO.File]::Replace($temp, $Path, [System.Management.Automation.Language.NullString]::Value)
    } finally {
        if ($acquired) { $commitLock.ReleaseMutex() }
        $commitLock.Dispose()
    }
} finally {
    if ([IO.File]::Exists($temp)) { [IO.File]::Delete($temp) }
}
