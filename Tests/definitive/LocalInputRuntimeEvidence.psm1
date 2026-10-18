#requires -Version 7.5
# Shared byte-oriented evidence operations. These helpers never inspect game
# memory, send input, load a trial DLL, change an installed file or delete data.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-LirNoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked evidence path: $candidate"
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}

function Get-LirPin([string]$Path) {
    $absolute = [IO.Path]::GetFullPath($Path)
    Assert-LirNoLinks $absolute
    $file = Get-Item -LiteralPath $absolute -Force
    if ($file.PSIsContainer) { throw "Expected evidence file: $absolute" }
    [ordered]@{ path=$absolute; bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash.ToLowerInvariant() }
}

function Assert-LirPin($Expected) {
    if (-not $Expected -or $Expected.path -isnot [string] -or $Expected.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $Expected.bytes -lt 0) {
        throw 'Malformed expected evidence pin'
    }
    $actual = Get-LirPin $Expected.path
    if ($actual.bytes -ne $Expected.bytes -or $actual.sha256 -cne $Expected.sha256.ToLowerInvariant()) { throw "Evidence changed: $($Expected.path)" }
    $actual
}

function Write-LirBytes([string]$OutputRoot, [string]$RelativePath, [byte[]]$Bytes) {
    $root = [IO.Path]::GetFullPath($OutputRoot)
    $path = [IO.Path]::GetFullPath((Join-Path $root $RelativePath))
    if (-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence output escapes its fresh directory' }
    Assert-LirNoLinks $path
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($path)) -Force | Out-Null
    $stream = [IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try { $stream.Write($Bytes,0,$Bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
    Get-LirPin $path
}

function Write-LirJson([string]$OutputRoot, [string]$RelativePath, $Value) {
    Write-LirBytes $OutputRoot $RelativePath ([Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 18)+"`n"))
}

function Copy-LirPinnedMetadata([string]$OutputRoot, [string]$RelativePath, $Pin) {
    $null = Assert-LirPin $Pin
    Write-LirBytes $OutputRoot $RelativePath ([IO.File]::ReadAllBytes($Pin.path))
}

function New-LirOutputDirectory([string]$RepositoryRoot, [ValidateSet('Baseline','Actual','Shutdown')][string]$Mode) {
    $parent = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'Build/local-input-runtime-evidence'))
    Assert-LirNoLinks $parent
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    $path = Join-Path $parent ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$Mode.ToLowerInvariant()+'-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $path | Out-Null
    $path
}

function Save-LirRawLog([string]$SourcePath, [string]$OutputRoot, [string]$RelativePath, [long]$MaximumBytes=67108864) {
    $row = [ordered]@{sourcePath=[IO.Path]::GetFullPath($SourcePath);saved=$null;captureSucceeded=$false;absent=$false;captureError=$null;newlineBoundary=$false;
        wholeFileLengthAtReadStart=$null;copyIsRawObservedPrefix=$true}
    try {
        Assert-LirNoLinks $row.sourcePath
        if (-not (Test-Path -LiteralPath $row.sourcePath -PathType Leaf)) { $row.absent=$true; throw 'Required diagnostic log is absent' }
        $stream = [IO.File]::Open($row.sourcePath,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        try {
            $length = $stream.Length
            $row.wholeFileLengthAtReadStart = $length
            if ($MaximumBytes -lt 0 -or $length -gt $MaximumBytes -or $length -gt [int]::MaxValue) { throw 'Diagnostic log exceeds bounded capture size' }
            $bytes = [byte[]]::new([int]$length)
            $offset = 0
            while ($offset -lt $bytes.Length) {
                $read = $stream.Read($bytes,$offset,$bytes.Length-$offset)
                if (-not $read) { throw 'Diagnostic log truncated during bounded shared read' }
                $offset += $read
            }
        } finally { $stream.Dispose() }
        # Success is published only after the raw write and its hash complete.
        $row.saved = Write-LirBytes $OutputRoot $RelativePath $bytes
        $row.newlineBoundary = -not $bytes.Length -or $bytes[-1] -eq 10
        $row.captureSucceeded = $true
    } catch { $row.captureError=$_.Exception.Message }
    $row
}

function Test-LirPrefix([string]$CurrentPath, $PrefixPin) {
    $prefix = Assert-LirPin $PrefixPin
    $current = Get-LirPin $CurrentPath
    $matches = $false
    if ($current.bytes -ge $prefix.bytes -and $prefix.bytes -le 67108864 -and $current.bytes -le 67108864) {
        $bytes = [IO.File]::ReadAllBytes($current.path)
        $head = [byte[]]::new([int]$prefix.bytes)
        if ($head.Length) { [Buffer]::BlockCopy($bytes,0,$head,0,$head.Length) }
        $matches = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($head)).ToLowerInvariant() -ceq $prefix.sha256
    }
    [ordered]@{preserved=$matches;prefix=$prefix;current=$current;deltaBytes=$(if($matches){$current.bytes-$prefix.bytes}else{$null})}
}

function ConvertFrom-LirCleoEvents([string]$Text) {
    $events = [Collections.Generic.List[object]]::new()
    foreach ($line in $Text -split '\r?\n') {
        if ($line -match '^\d{2}:\d{2}:\d{2} \[INFO\] \["[^\"]+"\] (?<wrapped>".*")$') {
            $inner = ConvertFrom-Json -InputObject $Matches.wrapped -DateKind String -ErrorAction Stop
            if ($inner.StartsWith('{')) { $events.Add((ConvertFrom-Json -InputObject $inner -DateKind String -ErrorAction Stop)) }
        }
    }
    $events.ToArray()
}

function Get-LirCleoHeaders([string]$Text) {
    @([regex]::Matches($Text,'(?m)^\d{2}:\d{2}:\d{2} \[INFO\] CLEO Redux [^\r\n]+') | ForEach-Object {
        [ordered]@{text=$_.Value;characterOffset=$_.Index}
    })
}

function Get-LirCleoDelta([string]$CurrentPath, $PrefixPin) {
    $continuity = Test-LirPrefix $CurrentPath $PrefixPin
    if (-not $continuity.preserved) { throw 'CLEO baseline whole-byte prefix was not preserved' }
    $bytes = [IO.File]::ReadAllBytes($CurrentPath)
    if (($PrefixPin.bytes -gt 0 -and $bytes[[int]$PrefixPin.bytes-1] -ne 10) -or ($bytes.Length -gt 0 -and $bytes[-1] -ne 10)) {
        throw 'CLEO protocol boundary is not a complete newline'
    }
    [Text.UTF8Encoding]::new($false,$true).GetString($bytes,[int]$PrefixPin.bytes,$bytes.Length-[int]$PrefixPin.bytes)
}

Export-ModuleMember -Function Assert-LirNoLinks,Get-LirPin,Assert-LirPin,Write-LirBytes,Write-LirJson,Copy-LirPinnedMetadata,New-LirOutputDirectory,Save-LirRawLog,Test-LirPrefix,ConvertFrom-LirCleoEvents,Get-LirCleoHeaders,Get-LirCleoDelta
