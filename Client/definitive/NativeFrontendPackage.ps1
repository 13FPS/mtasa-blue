# This file is part of Multi Theft Auto. See LICENSE in the repository root.
# Share the native entry's complete ownership/configuration contract across
# staging, installation, updates and mode changes; partial startup must refuse.
function Get-MtaFrontendMessageBoxPaths {
    return @('CLEO/de-online/mta/cgui/images/question.png',
        'CLEO/de-online/mta/cgui/images/info.png',
        'CLEO/de-online/mta/cgui/images/warning.png',
        'CLEO/de-online/mta/cgui/images/error.png')
}

function Get-MtaFrontendMessageBoxFileCount([string[]]$Paths, [int]$FrontendCount) {
    # Existing MTA dialogs load these four icons independently of the skin.
    # Keep their complete additive group optional so intact historical frontend
    # packages remain removable and can be upgraded without claiming user files.
    $count = @($Paths | Where-Object { $_ -in (Get-MtaFrontendMessageBoxPaths) }).Count
    if ($count -notin @(0,4) -or ($count -and $FrontendCount -ne 22)) { throw 'Incomplete MTA frontend message-box asset ownership.' }
    return $count
}

function Get-MtaNativeFrontendFileCount([string[]]$Paths, [int]$FrontendCount) {
    $count = @($Paths | Where-Object { $_ -in @('MtaDefinitive.asi','MtaDefinitive.ini') }).Count
    if ($count -notin @(0,2) -or ($count -and $FrontendCount -ne 22)) { throw 'Incomplete native MTA frontend startup ownership.' }
    return $count
}

function New-MtaNativeFrontendIni([bool]$Enabled, [bool]$BindingPersistence, [bool]$LoadedWorldAutoShow=$false) {
    # One owned file records the explicit native entry choice. It is separate
    # from MTA's user binding data and can be disarmed without deleting assets.
    if($LoadedWorldAutoShow -and -not $Enabled){throw 'Loaded-world auto-show requires the enabled native frontend host.'}
    # Leave historical false/absent INI bytes unchanged; an explicit normal-host
    # package alone owns this additive loaded-world presentation choice.
    $autoLine=if($LoadedWorldAutoShow){"LoadedWorldAutoShow=1`n"}else{''}
    return "[MTA]`nEnabled=$([int]$Enabled)`nLocalInput=1`nBindingPersistence=$([int]$BindingPersistence)`n"+$autoLine
}

function Assert-MtaNativeFrontendConfig([string]$Config, [int]$NativeCount, [string]$Ini, [string]$Mode) {
    $pattern = 'export const MTA_CORE_FRONTEND_NATIVE_HOST = (true|false);'
    $markers = [regex]::Matches($Config, $pattern)
    if ($markers.Count -gt 1 -or [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_NATIVE_HOST\b').Count -ne $markers.Count) {
        throw 'Invalid native MTA frontend configuration marker.'
    }
    $enabled = $markers.Count -eq 1 -and $markers[0].Groups[1].Value -eq 'true'
    if ($enabled -and ($NativeCount -ne 2 -or $Mode -ne 'Smoke' -or
        [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_RUNTIME = true;').Count -ne 1)) {
        throw 'Native MTA startup requires its complete entry group and retained frontend Smoke runtime.'
    }
    $autoPattern='export const MTA_CORE_FRONTEND_LOADED_WORLD_AUTO_SHOW = (true|false);'
    $autoMarkers=[regex]::Matches($Config,$autoPattern)
    if($autoMarkers.Count -gt 1 -or [regex]::Matches($Config,'export const MTA_CORE_FRONTEND_LOADED_WORLD_AUTO_SHOW\b').Count -ne $autoMarkers.Count){
        throw 'Invalid loaded-world MTA frontend auto-show configuration marker.'
    }
    $autoShow=$autoMarkers.Count -eq 1 -and $autoMarkers[0].Groups[1].Value -eq 'true'
    if($autoShow -and -not $enabled){throw 'Loaded-world auto-show requires the enabled native frontend host and runtime.'}
    if ($NativeCount) {
        $persistent = [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = true;').Count -eq 1
        if ($markers.Count -ne 1 -or $Ini.Replace("`r`n","`n") -cne (New-MtaNativeFrontendIni $enabled $persistent $autoShow)) {
            throw 'Native MTA startup INI does not match its owned script configuration.'
        }
    }
}

function Get-MtaNativeFrontendExportsFromBytes([byte[]]$bytes, [string]$Path) {
    # A new opt-in must reject stale DLLs before staging or downloading. Read
    # their PE export tables without executing Core, bridge or their DllMain.
    if ($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) { throw "Invalid AMD64 PE: $Path" }
    $pe = [uint64][BitConverter]::ToUInt32($bytes, 0x3c)
    if ($pe + 24 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes, [int]$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes, [int]$pe + 4) -ne 0x8664) { throw "Invalid AMD64 PE: $Path" }
    $sectionCount = [BitConverter]::ToUInt16($bytes, [int]$pe + 6)
    $optionalSize = [BitConverter]::ToUInt16($bytes, [int]$pe + 20)
    $optional = [int]$pe + 24
    if ($optionalSize -lt 120 -or $optional + $optionalSize -gt $bytes.Length -or [BitConverter]::ToUInt16($bytes, $optional) -ne 0x20b -or
        [BitConverter]::ToUInt32($bytes, $optional + 108) -lt 1 -or -not $sectionCount -or $sectionCount -gt 96) { throw "Invalid PE optional header: $Path" }
    $exportRva = [BitConverter]::ToUInt32($bytes, $optional + 112)
    $exportSize = [BitConverter]::ToUInt32($bytes, $optional + 116)
    $table = $optional + $optionalSize
    if ($table + 40 * $sectionCount -gt $bytes.Length -or -not $exportRva -or $exportSize -lt 40) { throw "Missing PE exports: $Path" }
    $sections = @()
    for ($index = 0; $index -lt $sectionCount; ++$index) {
        $at = $table + 40 * $index
        $sections += @{rva=[uint64][BitConverter]::ToUInt32($bytes, $at + 12);raw=[uint64][BitConverter]::ToUInt32($bytes, $at + 20);size=[uint64][BitConverter]::ToUInt32($bytes, $at + 16)}
    }
    $offset = {
        param([uint64]$Rva, [uint64]$Size)
        foreach ($section in $sections) {
            if ($Rva -ge $section.rva -and $Rva - $section.rva + $Size -le $section.size -and $section.raw + $Rva - $section.rva + $Size -le $bytes.Length) {
                return [int]($section.raw + $Rva - $section.rva)
            }
        }
        throw "Invalid PE export RVA: $Path"
    }
    $directory = & $offset $exportRva 40
    $functionCount = [BitConverter]::ToUInt32($bytes, $directory + 20)
    $nameCount = [BitConverter]::ToUInt32($bytes, $directory + 24)
    if (-not $functionCount -or -not $nameCount -or $functionCount -gt 65536 -or $nameCount -gt 65536) { throw "Invalid PE export counts: $Path" }
    $functions = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 28)) (4 * $functionCount)
    $names = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 32)) (4 * $nameCount)
    $ordinals = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 36)) (2 * $nameCount)
    $exports = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($index = 0; $index -lt $nameCount; ++$index) {
        $ordinal = [BitConverter]::ToUInt16($bytes, $ordinals + 2 * $index)
        if ($ordinal -ge $functionCount) { throw "Invalid PE export ordinal: $Path" }
        $functionRva = [BitConverter]::ToUInt32($bytes, $functions + 4 * $ordinal)
        $nameRva = [BitConverter]::ToUInt32($bytes, $names + 4 * $index)
        $nameOffset = & $offset $nameRva 1
        $length = 0
        while ($length -lt 512 -and $nameOffset + $length -lt $bytes.Length -and $bytes[$nameOffset + $length]) { ++$length }
        if (-not $length -or $length -eq 512 -or $nameOffset + $length -ge $bytes.Length) { throw "Invalid PE export name: $Path" }
        & $offset $nameRva ($length + 1) | Out-Null
        # A forwarded name cannot establish that this reviewed module contains
        # the completed lifecycle implementation required by the new mode.
        if ($functionRva -and ($functionRva -lt $exportRva -or [uint64]$functionRva -ge [uint64]$exportRva + $exportSize)) {
            [void]$exports.Add([Text.Encoding]::ASCII.GetString($bytes, $nameOffset, $length))
        }
    }
    Write-Output -NoEnumerate $exports
}


function Assert-MtaNativeFrontendAutoShowCore([string]$Path, [string]$ExpectedSha256) {
    # Check the exact owned bytes without loading Core/DllMain. The stream
    # excludes writers while the captured bytes and their hash are obtained.
    if($ExpectedSha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Invalid owned Core hash for loaded-world auto-show.'}
    $full=[IO.Path]::GetFullPath($Path)
    for($current=$full;$current;$current=[IO.Path]::GetDirectoryName($current)) {
        if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked Core path refused.'}
    }
    $stream=[IO.File]::Open($full,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $memory=[IO.MemoryStream]::new()
    try {
        $length=$stream.Length
        if($length -le 0 -or $length -gt 64MB){throw 'Owned Core size is out of bounds.'}
        $stream.CopyTo($memory)
        $bytes=$memory.ToArray()
        if($bytes.Length -ne $length -or $stream.Length -ne $length){throw 'Owned Core changed while reading.'}
    } finally {$memory.Dispose();$stream.Dispose()}
    $actual=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    if($actual -cne $ExpectedSha256){throw 'Owned Core changed before enabling loaded-world auto-show.'}
    $exports=Get-MtaNativeFrontendExportsFromBytes $bytes $full
    if(-not $exports.Contains('MTA_ReadDEFrontendRuntimeAdmissionV1')){throw 'Loaded-world auto-show requires the owned Core admission export; update the frontend.'}
}
