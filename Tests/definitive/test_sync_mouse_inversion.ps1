#requires -Version 7.0
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$command = Join-Path $repo 'Client/definitive/sync-mouse-inversion.ps1'
$scratch = Join-Path $repo ('Build/tmp/mouse-inversion/' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($scratch)
$profilePath = Join-Path $scratch 'coreconfig.xml'
$iniPath = Join-Path $scratch 'GameUserSettings.ini'
$paths = @{ CoreConfig = $profilePath; GameUserSettings = $iniPath }
$section = '[/Game/Common/Settings/UniversalGameSettings.UniversalGameSettings_C]'
$xml = '<mainconfig><settings><invert_mouse>1</invert_mouse></settings></mainconfig>'
$bothSettings = $xml.Replace('</settings>', '<controller_vibration>1</controller_vibration></settings>')
$record = 'Controls=(InvertLookExtra=False, InvertLook = False ,InvertGyroY=False,VibrationFeedbackExtra=False, VibrationFeedback = False ) ; InvertLook=False,VibrationFeedback=False'
$template = @('[Decoy]', 'Controls=(InvertLook=False,VibrationFeedback=False)', '; café', $section, $record, '[Other]', 'Extra=1')
$running = $false

# Model the process guard without depending on whether the developer is playing GTA.
function Get-Process($Name, $ErrorAction) {
    if ($Name -cne 'SanAndreas') { throw "Unexpected process lookup for $Name" }
    if ($running) { return [pscustomobject]@{ ProcessName = $Name } }
}
function Assert-Bytes([string]$Path, [byte[]]$Expected, [string]$Case) {
    $actual = [IO.File]::ReadAllBytes($Path)
    if ([Convert]::ToBase64String($Expected) -cne [Convert]::ToBase64String($actual)) { throw "Unexpected file content in $Case" }
}
function Assert-Rejected([string]$Case, [string]$Source, [string]$Target, [string]$Message, [string]$Preference = 'InvertMouse') {
    [IO.File]::WriteAllText($profilePath, $Source)
    [IO.File]::WriteAllText($iniPath, $Target)
    $original = [IO.File]::ReadAllBytes($iniPath)
    $failure = $null
    try { & $command @paths -Preference $Preference } catch { $failure = $_.Exception.Message }
    if (!$failure -or $failure -notmatch $Message) { throw "Expected refusal in $Case. Received '$failure'." }
    Assert-Bytes $iniPath $original $Case
}

function Test-RoundTrip([string]$TargetField, [hashtable]$Selection) {
    foreach ($encoding in @([Text.UTF8Encoding]::new($false), [Text.UTF8Encoding]::new($true), [Text.Encoding]::Unicode)) {
        foreach ($newline in @("`r`n", "`n")) {
            $off = $template -join $newline
            $on = $off.Replace(" $TargetField = False ", " $TargetField = True ")
            [IO.File]::WriteAllText($iniPath, $off, $encoding)
            [IO.File]::WriteAllText($profilePath, $bothSettings)
            $sourceBytes = [IO.File]::ReadAllBytes($profilePath)
            $offBytes = [IO.File]::ReadAllBytes($iniPath)
            & $command @paths @Selection -WhatIf
            Assert-Bytes $iniPath $offBytes 'preview'
            & $command @paths @Selection
            $onBytes = [byte[]]($encoding.GetPreamble() + $encoding.GetBytes($on))
            Assert-Bytes $iniPath $onBytes 'enable'
            $stamp = [IO.File]::GetLastWriteTimeUtc($iniPath)
            & $command @paths @Selection
            Assert-Bytes $iniPath $onBytes 'repeat'
            if ([IO.File]::GetLastWriteTimeUtc($iniPath) -ne $stamp) { throw 'Repeat import touched the file' }
            Assert-Bytes $profilePath $sourceBytes 'source profile'
            [IO.File]::WriteAllText($profilePath, $bothSettings.Replace('>1<', '>0<'))
            & $command @paths @Selection
            Assert-Bytes $iniPath $offBytes 'disable'
        }
    }
}

try {
    # Both profile values are enabled. Each invocation must edit exactly one member.
    Test-RoundTrip 'InvertLook' @{}
    Test-RoundTrip 'VibrationFeedback' @{ Preference = 'ControllerVibration' }
    $ini = $template -join "`n"
    Assert-Rejected 'invalid MTA value' $xml.Replace('>1<', '>2<') $ini 'Expected one MTA'
    Assert-Rejected 'space before MTA value' $xml.Replace('>1<', '> 1<') $ini 'Expected one MTA'
    Assert-Rejected 'missing MTA value' $xml.Replace('invert_mouse', 'invert_gyro') $ini 'Expected one MTA'
    Assert-Rejected 'duplicate MTA value' $xml.Replace('</settings>', '<invert_mouse>0</invert_mouse></settings>') $ini 'Expected one MTA'
    Assert-Rejected 'malformed MTA XML' '<mainconfig>' $ini 'XML|mainconfig'
    Assert-Rejected 'wrong section' $xml $ini.Replace($section, '[Elsewhere]') 'section'
    Assert-Rejected 'duplicate section' $xml ($ini + "`n$section`n") 'section'
    Assert-Rejected 'duplicate section at EOF' $xml ($ini + "`n$section") 'section'
    Assert-Rejected 'duplicate controls' $xml $ini.Replace($record, "$record`n$record") 'Controls record'
    Assert-Rejected 'missing controls' $xml $ini.Replace($record, 'Other=1') 'Controls record'
    Assert-Rejected 'missing field' $xml $ini.Replace(' InvertLook = False ', ' Other=False ') 'Boolean InvertLook'
    Assert-Rejected 'duplicate field' $xml $ini.Replace('InvertGyroY', 'InvertLook') 'Boolean InvertLook'
    Assert-Rejected 'invalid Boolean' $xml $ini.Replace(' InvertLook = False ', ' InvertLook = 0 ') 'Boolean InvertLook'
    $vibrationOnly = $xml.Replace('invert_mouse', 'controller_vibration')
    Assert-Rejected 'missing MTA vibration' $xml $ini 'mainconfig/settings/controller_vibration' ControllerVibration
    Assert-Rejected 'invalid MTA vibration' $vibrationOnly.Replace('>1<', '>True<') $ini 'mainconfig/settings/controller_vibration' ControllerVibration
    Assert-Rejected 'duplicate MTA vibration' $vibrationOnly.Replace('</settings>', '<controller_vibration>0</controller_vibration></settings>') $ini 'mainconfig/settings/controller_vibration' ControllerVibration
    Assert-Rejected 'missing game vibration' $vibrationOnly $ini.Replace(' VibrationFeedback = False ', ' Other=False ') 'Boolean VibrationFeedback' ControllerVibration
    Assert-Rejected 'duplicate game vibration' $vibrationOnly $ini.Replace('VibrationFeedbackExtra', 'VibrationFeedback') 'Boolean VibrationFeedback' ControllerVibration
    Assert-Rejected 'invalid game vibration' $vibrationOnly $ini.Replace(' VibrationFeedback = False ', ' VibrationFeedback = 0 ') 'Boolean VibrationFeedback' ControllerVibration
    $running = $true
    Assert-Rejected 'active game' $xml $ini 'Close GTA'
    Assert-Rejected 'active game vibration' $vibrationOnly $ini 'Close GTA' ControllerVibration
    Write-Output 'Passed mouse inversion and controller vibration import, preview, repeat and refusal tests.'
} finally {
    [IO.File]::Delete($profilePath)
    [IO.File]::Delete($iniPath)
    [IO.Directory]::Delete($scratch)
}
