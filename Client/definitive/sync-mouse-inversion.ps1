#requires -Version 7.0
<#
.SYNOPSIS
Applies an MTA profile's vertical mouse inversion preference before starting GTA Definitive Edition.
.EXAMPLE
./sync-mouse-inversion.ps1 -CoreConfig 'C:\Games\MTA\MTA\config\coreconfig.xml'
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$CoreConfig,
    [string]$GameUserSettings = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Rockstar Games/GTA San Andreas Definitive Edition/Config/WindowsNoEditor/GameUserSettings.ini')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$readerSettings = [Xml.XmlReaderSettings]::new()
$readerSettings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
$reader = [Xml.XmlReader]::Create((Get-Item -LiteralPath $CoreConfig).FullName, $readerSettings)
try {
    $profile = [Xml.XmlDocument]::new()
    $profile.XmlResolver = $null
    $profile.Load($reader)
} finally {
    $reader.Dispose()
}
$settings = $profile.SelectNodes('/mainconfig/settings')
$values = $profile.SelectNodes('/mainconfig/settings/invert_mouse')
if ($settings.Count -ne 1 -or $values.Count -ne 1 -or $values[0].SelectNodes('*').Count -or $values[0].InnerText.Trim() -cnotin @('0', '1')) {
    throw 'Expected one MTA mainconfig/settings/invert_mouse value of 0 or 1.'
}
$desired = if ($values[0].InnerText.Trim() -eq '1') { 'True' } else { 'False' }
$path = (Get-Item -LiteralPath $GameUserSettings).FullName
$bytes = [IO.File]::ReadAllBytes($path)
$stream = [IO.MemoryStream]::new($bytes, $false)
$reader = [IO.StreamReader]::new($stream, [Text.UTF8Encoding]::new($false, $true), $true)
try {
    $text = $reader.ReadToEnd()
    $encoding = $reader.CurrentEncoding
} finally {
    $reader.Dispose()
}

# The game saves all control preferences in one tuple. Limit the edit to
# this section, key and flat tuple member so similarly named settings survive.
$sectionName = '/Game/Common/Settings/UniversalGameSettings.UniversalGameSettings_C'
$escapedName = [regex]::Escape($sectionName)
$sections = [regex]::Matches($text, "(?ims)^[ \t]*\[$escapedName\][ \t]*(?:[;#][^\r\n]*)?(?:\r?\n|\z)(?<body>.*?)(?=^[ \t]*\[|\z)")
if ($sections.Count -ne 1) { throw 'Expected one GTA UniversalGameSettings section.' }
$body = $sections[0].Groups['body']
$controls = [regex]::Matches($body.Value, '(?im)^[ \t]*Controls[ \t]*=[ \t]*(?<data>[^\r\n]*)\r?$')
if ($controls.Count -ne 1) { throw 'Expected one GTA Controls record.' }
$data = $controls[0].Groups['data']
$tuple = [regex]::Match($data.Value, '^\((?<members>[^()";#]*)\)[ \t]*(?:[;#].*)?$')
if (!$tuple.Success) { throw 'Unrecognized GTA Controls tuple.' }
$members = $tuple.Groups['members']
$fields = [regex]::Matches($members.Value, '(?i)(?:^|,)[ \t]*InvertLook[ \t]*=[ \t]*(?<value>[^,]*?)[ \t]*(?=,|$)')
if ($fields.Count -ne 1 -or $fields[0].Groups['value'].Value -inotmatch '^(True|False)$') {
    throw 'Expected one Boolean InvertLook field in the GTA Controls tuple.'
}
$current = $fields[0].Groups['value']
if ($current.Value -ieq $desired) { return }
if (!$PSCmdlet.ShouldProcess($path, "Set InvertLook to $desired")) { return }
if (Get-Process -Name SanAndreas -ErrorAction SilentlyContinue) { throw 'Close GTA San Andreas Definitive Edition before applying the preference.' }

# Keep the original bytes around the token, including any BOM and line endings.
$index = $body.Index + $data.Index + $members.Index + $current.Index
$end = $bytes.Length - $encoding.GetByteCount($text.Substring($index + $current.Length))
$start = $end - $encoding.GetByteCount($current.Value)
$oldToken = $encoding.GetBytes($current.Value)
if ($start -lt 0 -or [Convert]::ToBase64String($bytes, $start, $oldToken.Length) -cne [Convert]::ToBase64String($oldToken)) {
    throw 'Could not identify the encoded InvertLook value.'
}
$temp = Join-Path ([IO.Path]::GetDirectoryName($path)) ('.mta-invert-' + [guid]::NewGuid().ToString('N') + '.tmp')
try {
    $output = [IO.File]::Open($temp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $output.Write($bytes, 0, $start)
        $replacement = $encoding.GetBytes($desired)
        $output.Write($replacement, 0, $replacement.Length)
        $output.Write($bytes, $end, $bytes.Length - $end)
        $output.Flush($true)
    } finally {
        $output.Dispose()
    }
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -cne [Convert]::ToBase64String($bytes)) {
        throw 'GTA settings changed during the update. Retry using the current file.'
    }
    [IO.File]::Replace($temp, $path, [System.Management.Automation.Language.NullString]::Value)
} finally {
    if ([IO.File]::Exists($temp)) { [IO.File]::Delete($temp) }
}
