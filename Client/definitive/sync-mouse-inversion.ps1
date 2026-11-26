#requires -Version 7.0
<#
.SYNOPSIS
Applies a saved MTA control preference before starting GTA Definitive Edition. The default is mouse inversion.
.EXAMPLE
./sync-mouse-inversion.ps1 -CoreConfig 'C:\Games\MTA\MTA\config\coreconfig.xml'
.EXAMPLE
./sync-mouse-inversion.ps1 -Preference ControllerVibration -CoreConfig 'C:\Games\MTA\MTA\config\coreconfig.xml'
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$CoreConfig,
    [string]$GameUserSettings = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Rockstar Games/GTA San Andreas Definitive Edition/Config/WindowsNoEditor/GameUserSettings.ini'),
    [ValidateSet('InvertMouse', 'ControllerVibration')][string]$Preference = 'InvertMouse'
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
$profileKey, $gameField = if ($Preference -eq 'ControllerVibration') {
    'controller_vibration', 'VibrationFeedback'
} else {
    'invert_mouse', 'InvertLook'
}
$settings = $profile.SelectNodes('/mainconfig/settings')
$values = $profile.SelectNodes("/mainconfig/settings/$profileKey")
if ($settings.Count -ne 1 -or $values.Count -ne 1 -or $values[0].SelectNodes('*').Count -or $values[0].InnerText -cnotin @('0', '1')) {
    throw "Expected one MTA mainconfig/settings/$profileKey value of 0 or 1."
}
$desired = if ($values[0].InnerText -eq '1') { 'True' } else { 'False' }
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
$fields = [regex]::Matches($members.Value, "(?i)(?:^|,)[ \t]*$gameField[ \t]*=[ \t]*(?<value>[^,]*?)[ \t]*(?=,|$)")
if ($fields.Count -ne 1 -or $fields[0].Groups['value'].Value -inotmatch '^(True|False)$') {
    throw "Expected one Boolean $gameField field in the GTA Controls tuple."
}
$current = $fields[0].Groups['value']
if ($current.Value -ieq $desired) { return }
if (!$PSCmdlet.ShouldProcess($path, "Set $gameField to $desired")) { return }

$index = $body.Index + $data.Index + $members.Index + $current.Index
& (Join-Path $PSScriptRoot 'Write-DefinitiveSettingsToken.ps1') -Path $path -Bytes $bytes -Encoding $encoding -Text $text `
    -Index $index -OldToken $current.Value -Replacement $desired
