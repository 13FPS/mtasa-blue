#requires -Version 7.0
<#
.SYNOPSIS
Copies saved MTA movement keys to GTA Definitive Edition's keyboard layout before launch.
.EXAMPLE
./sync-movement-binding.ps1 -MtaKeybinds 'C:\path\to\definitive\keybinds.xml' -Control forwards -WhatIf
.EXAMPLE
./sync-movement-binding.ps1 -MtaKeybinds 'C:\path\to\definitive\keybinds.xml' -WholeProfile -WhatIf
.DESCRIPTION
WholeProfile applies forwards, backwards, left, right and sprint together. Each needs a distinct supported primary key.
#>
[CmdletBinding(SupportsShouldProcess, DefaultParameterSetName='Single')]
param(
    [Parameter(Mandatory, Position=0)][string]$MtaKeybinds,
    [Parameter(Mandatory, Position=1, ParameterSetName='Single')][ValidateSet('forwards', 'backwards', 'left', 'right', 'sprint')][string]$Control,
    [Parameter(Mandatory, ParameterSetName='Profile')][switch]$WholeProfile,
    [Parameter(Position=2)][string]$KeyboardMappings = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Rockstar Games/GTA San Andreas Definitive Edition/KeyboardMappings.cfg')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Values observed in GTA's Controls menu and KeyboardMappings.cfg. The frontend
# reads the same subset in DefinitiveRetailMovementBindings.h.
$keyIds = @{
    w='29'; u='34'; i='35'; a='42'; s='43'; d='44'; j='48'; k='49'; l='50'; lshift='54'; space='68'
    arrow_u='79'; arrow_d='80'; arrow_l='81'; arrow_r='82'
}
$actions = @{
    forwards=@('HID_MAPPING_PED_MOVE_Y', '1'); backwards=@('HID_MAPPING_PED_MOVE_Y', '0')
    left=@('HID_MAPPING_PED_MOVE_X', '1'); right=@('HID_MAPPING_PED_MOVE_X', '0')
    sprint=@('HID_MAPPING_SPRINT', '0')
}
$source = Get-Item -LiteralPath $MtaKeybinds
if ($source.Length -gt 1MB) { throw 'MTA keybind profile is too large.' }
$sourceBytes = [IO.File]::ReadAllBytes($source.FullName)
$stream = [IO.MemoryStream]::new($sourceBytes, $false)
$xmlSettings = [Xml.XmlReaderSettings]::new()
$xmlSettings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
$xmlSettings.MaxCharactersInDocument = 1MB
$reader = [Xml.XmlReader]::Create($stream, $xmlSettings)
try {
    $doc = [Xml.XmlDocument]::new()
    $doc.XmlResolver = $null
    $doc.Load($reader)
} finally {
    $reader.Dispose()
    $stream.Dispose()
}
$root = $doc.DocumentElement
$binds = $root.SelectSingleNode('binds')
if ($root.Name -cne 'definitive_keybinds' -or $root.GetAttribute('version') -cne '1' -or $root.Attributes.Count -ne 1 -or
    $root.SelectNodes('*').Count -ne 1 -or !$binds -or $binds.Attributes.Count -ne 0) {
    throw 'Expected a version 1 MTA definitive_keybinds movement profile.'
}
$records = $binds.SelectNodes('*')
if ($records.Count -gt 1024) { throw 'Too many records in MTA movement profile.' }
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$primaries = @{}
foreach ($record in $records) {
    $name = $record.GetAttribute('control')
    $key = $record.GetAttribute('key')
    if ($record.Name -cne 'bind' -or $record.Attributes.Count -ne 2 -or $record.SelectNodes('*').Count -or
        $name -cnotin @('forwards', 'backwards', 'left', 'right', 'sprint', 'walk') -or [string]::IsNullOrWhiteSpace($key) -or !$seen.Add("$name/$key")) {
        throw 'Invalid or duplicate bind in MTA movement profile.'
    }
    # Settings stores the primary before its alternates, even when the primary
    # is a key the retail importer cannot translate.
    if (!$primaries.ContainsKey($name)) { $primaries[$name] = $key }
}
if ($PSCmdlet.ParameterSetName -eq 'Profile' -and !$WholeProfile) { throw 'Specify -WholeProfile to apply the five saved keys.' }
$controls = if ($WholeProfile) { 'forwards','backwards','left','right','sprint' } else { $Control }
$controlForKey = @{}
$controlForSlot = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
$mappings = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($name in $controls) {
    $primary = $primaries[$name]
    if (!$primary) { throw "MTA has no saved primary key for $name." }
    $desired = $keyIds[$primary]
    if (!$desired) { throw "Unsupported MTA primary key '$primary' for $name." }
    if ($controlForKey.ContainsKey($desired)) { throw "MTA controls $name and $($controlForKey[$desired]) both use '$primary'." }
    $controlForKey[$desired] = $name
    $mapping, $direction = $actions[$name]
    $controlForSlot.Add("$mapping/$direction", $name)
    [void]$mappings.Add($mapping)
}

$target = Get-Item -LiteralPath $KeyboardMappings
if ($target.Length -gt 64KB) { throw 'GTA keyboard mapping file exceeds 64 KiB.' }
$bytes = [IO.File]::ReadAllBytes($target.FullName)
if (!$bytes.Length -or $bytes.Length -gt 64KB) { throw 'Invalid GTA keyboard mapping file size.' }
$stream = [IO.MemoryStream]::new($bytes, $false)
$reader = [IO.StreamReader]::new($stream, [Text.UTF8Encoding]::new($false, $true), $true)
try {
    $text = $reader.ReadToEnd()
    $encoding = $reader.CurrentEncoding
} finally {
    $reader.Dispose()
}
if ($text.Contains([char]0)) { throw 'Unsupported binary keyboard mapping file.' }
$encoded = $encoding.GetBytes($text)
$prefix = $bytes.Length - $encoded.Length
if ($prefix -lt 0 -or [Convert]::ToBase64String($bytes, $prefix, $encoded.Length) -cne [Convert]::ToBase64String($encoded) -or
    ($prefix -gt 0 -and [Convert]::ToBase64String($bytes, 0, $prefix) -cne [Convert]::ToBase64String($encoding.GetPreamble()))) {
    throw 'Invalid keyboard mapping file encoding.'
}

$selected = @{}
$conflicts = @()
# The retail game reuses movement keys for vehicles, menus and distinct
# minigames. Keep those records intact. Unknown contexts cannot establish
# whether the same key would dispatch two actions while the player is on foot.
$independent = '^HID_MAPPING_(?:(?:VEHICLE|FLIGHT|MENU|RHYTHM|BLACK_JACK|HELICOPTER|ARCADE|GYM|CRANE|MAP|RADIO)_\w+|ACCELERATE|BRAKE|HANDBRAKE|HORN|NITRO|LOCK_HYDRAULICS|RESET_HYDRAULICS|BUNNY_HOP|STOPPIE|WHEELIE)$'
foreach ($line in [regex]::Matches($text, '[^\r\n]+')) {
    $words = [regex]::Matches($line.Value, '[^ \t]+')
    if ($words.Count -lt 2 -or $words[0].Value -match '^[;#]') { continue }
    $id = $words[0].Value
    $action = $words[1].Value
    if ($mappings.Contains($action)) {
        if ($words.Count -lt 3 -or $words[2].Value -cnotin @('0','1')) { throw "Malformed GTA mapping for $action." }
        $slot = "$action/$($words[2].Value)"
        if ($controlForSlot.ContainsKey($slot)) {
            $name = $controlForSlot[$slot]
            if ($selected.ContainsKey($name) -or $words.Count -ne 4 -or $words[3].Value -cne '-1' -or !$keyIds.ContainsValue($id)) {
                throw "GTA has an ambiguous or unsupported mapping for $name."
            }
            $selected[$name] = [pscustomobject]@{ Index = $line.Index + $words[0].Index; Old = $id; New = $keyIds[$primaries[$name]] }
            # Each selected action is assigned its final, already distinct key.
            continue
        }
    }
    if (!$controlForKey.ContainsKey($id)) { continue }
    $name = $controlForKey[$id]
    if ($words.Count -ne 4) { throw "Malformed GTA row using '$($primaries[$name])'." }
    if ($words[3].Value -cne '-1') { continue }
    if ($action -cmatch $independent) { continue }
    # The stock game's four-arrow layout retains these two gang commands on
    # the same arrows as forwards and backwards.
    if (($name -eq 'forwards' -and $id -eq '79' -and $action -ceq 'HID_MAPPING_GANG_RECRUIT_FOLLOW') -or
        ($name -eq 'backwards' -and $id -eq '80' -and $action -ceq 'HID_MAPPING_GANG_IGNORE_HOLD_POSITION')) { continue }
    $conflicts += "$($primaries[$name]) ($name): $action"
}
foreach ($name in $controls) {
    if (!$selected.ContainsKey($name)) { throw "GTA has no existing keyboard mapping for $name." }
}
$edits = @($selected.Values | Where-Object { $_.Old -cne $_.New } | Sort-Object Index -Descending)
if ($conflicts.Count -and ($WholeProfile -or $edits.Count)) { throw "Conflicting GTA keyboard bindings: $($conflicts -join ', ')." }
if (!$edits.Count) { return }
$summary = if ($WholeProfile) {
    'Apply MTA primary keys (' + (($controls | ForEach-Object { "$_=$($primaries[$_])" }) -join ', ') + ')'
} else {
    "Set $Control to the saved MTA primary key '$($primaries[$Control])'"
}
if (!$PSCmdlet.ShouldProcess($target.FullName, $summary)) { return }

# Edit from the end so all offsets refer to the original layout. The shared
# writer verifies and commits this one span against both original input files.
$index = $edits[-1].Index
$end = $edits[0].Index + $edits[0].Old.Length
$old = $text.Substring($index, $end - $index)
$replacement = $old
foreach ($edit in $edits) {
    $offset = $edit.Index - $index
    $replacement = $replacement.Remove($offset, $edit.Old.Length).Insert($offset, $edit.New)
}
& (Join-Path $PSScriptRoot 'Write-DefinitiveSettingsToken.ps1') -Path $target.FullName -Bytes $bytes -Encoding $encoding -Text $text `
    -Index $index -OldToken $old -Replacement $replacement -GameProcesses @('SanAndreas','PlayGTASanAndreas') `
    -SourcePath $source.FullName -SourceBytes $sourceBytes
