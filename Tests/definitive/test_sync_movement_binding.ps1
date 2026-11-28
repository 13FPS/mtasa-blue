#requires -Version 7.0
param([string]$CapturedStockKeyboardMappings)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$command = Join-Path $repo 'Client/definitive/sync-movement-binding.ps1'
$scratch = Join-Path $repo ('Build/tmp/retail-movement-export/' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($scratch)
$profile = Join-Path $scratch 'keybinds.xml'
$mapping = Join-Path $scratch 'KeyboardMappings.cfg'
$paths = @{ MtaKeybinds = $profile; KeyboardMappings = $mapping }
$fixtureProcess = @{ Running = ''; OnCheck = $null; Checks = 0 }
$cases = 0

# Exercise the real file code with fixture-local process state, even if GTA is open on the workstation.
function Get-Process([string[]]$Name, $ErrorAction) {
    $fixtureProcess.Checks++
    if ($fixtureProcess.OnCheck) {
        $callback = $fixtureProcess.OnCheck
        $fixtureProcess.OnCheck = $null
        & $callback
    }
    if ($fixtureProcess.Running -and $Name -contains $fixtureProcess.Running) { [pscustomobject]@{ ProcessName = $fixtureProcess.Running } }
}

function Assert-Bytes([string]$Path, [byte[]]$Expected, [string]$Case) {
    $actual = [Convert]::ToBase64String([IO.File]::ReadAllBytes($Path))
    if ($actual -cne [Convert]::ToBase64String($Expected)) { throw "Unexpected bytes in $Case" }
}

function Movement-Profile([string]$Rows) {
    "<definitive_keybinds version=`"1`"><binds>$Rows</binds></definitive_keybinds>"
}

function Set-Primary([string]$Control, [string]$Key) {
    [IO.File]::WriteAllText($profile, (Movement-Profile "<bind control=`"$Control`" key=`"$Key`" />"))
}

function Assert-Refusal([string]$Case, [string]$Xml, [string]$Retail, [string]$Control = 'forwards') {
    [IO.File]::WriteAllText($profile, $Xml)
    [IO.File]::WriteAllText($mapping, $Retail)
    $original = [IO.File]::ReadAllBytes($mapping)
    $failed = $false
    try { & $command @paths -Control $Control } catch { $failed = $true }
    if (!$failed) { throw "Expected refusal for $Case" }
    Assert-Bytes $mapping $original $Case
    $script:cases++
}

function Whole-Profile([string[]]$Keys) {
    $names = @('forwards','backwards','left','right','sprint')
    $rows = for ($n = 0; $n -lt $Keys.Count; ++$n) {
        '<bind control="{0}" key="{1}" />' -f $names[$n], $Keys[$n]
    }
    Movement-Profile ($rows -join '')
}

function Assert-WholeRefusal([string]$Case, [string]$Xml, [string]$Retail, [string]$ExpectedError) {
    [IO.File]::WriteAllText($profile, $Xml)
    [IO.File]::WriteAllText($mapping, $Retail)
    $before = [IO.File]::ReadAllBytes($mapping)
    $message = ''
    try { & $command @paths -WholeProfile } catch { $message = $_.Exception.Message }
    if ($message -notmatch $ExpectedError) { throw "Unexpected result for ${Case}: $message" }
    Assert-Bytes $mapping $before $Case
    $script:cases++
}

$k = '<bind key="k" control="forwards" />'
$up = '<bind key="arrow_u" control="forwards" />'
$xml = Movement-Profile ($k + $up)
$row = '29 HID_MAPPING_PED_MOVE_Y 1 -1'

try {
    # The primary is the first saved binding for this action. Bind order across actions has no significance.
    $several = Movement-Profile ('<bind key="rshift" control="walk" />' + $k + '<bind key="p" control="left" />' + $up)
    [IO.File]::WriteAllText($profile, $several)
    $sourceBytes = [IO.File]::ReadAllBytes($profile)
    $target = "  29`tHID_MAPPING_PED_MOVE_Y  1`t-1 "
    $replacement = $target.Replace('29', '49')
    $originalText = "29 HID_MAPPING_ACCELERATE 0 -1`r`n49 HID_MAPPING_RHYTHM_UP 0 -1`n$target`r43 HID_MAPPING_PED_MOVE_Y 0 -1`n127 HID_MAPPING_ATTACK 0 0"
    $updatedText = $originalText.Replace($target, $replacement)
    foreach ($encoding in @([Text.UTF8Encoding]::new($false), [Text.UTF8Encoding]::new($true), [Text.Encoding]::Unicode,
                            [Text.Encoding]::BigEndianUnicode, [Text.Encoding]::UTF32, [Text.UTF32Encoding]::new($true, $true))) {
        [IO.File]::WriteAllText($mapping, $originalText, $encoding)
        $before = [IO.File]::ReadAllBytes($mapping)
        & $command @paths -Control forwards -WhatIf
        Assert-Bytes $mapping $before 'preview'
        & $command @paths -Control forwards
        $expected = [byte[]]($encoding.GetPreamble() + $encoding.GetBytes($updatedText))
        Assert-Bytes $mapping $expected 'only the selected token in a mixed-newline file'
        $stamp = [datetime]::new(2020, 1, 2, 3, 4, 5, [DateTimeKind]::Utc)
        [IO.File]::SetLastWriteTimeUtc($mapping, $stamp)
        & $command @paths -Control forwards
        Assert-Bytes $mapping $expected 'repeat'
        if ([IO.File]::GetLastWriteTimeUtc($mapping) -ne $stamp) { throw 'A no-op rewrote the retail file' }
        Assert-Bytes $profile $sourceBytes 'source profile'
        $cases++
    }

    $keys = [ordered]@{
        w = 29; u = 34; i = 35; a = 42; s = 43; d = 44; j = 48; k = 49; l = 50; lshift = 54; space = 68
        arrow_u = 79; arrow_d = 80; arrow_l = 81; arrow_r = 82
    }
    foreach ($pair in $keys.GetEnumerator()) {
        Set-Primary 'forwards' $pair.Key
        [IO.File]::WriteAllText($mapping, $row)
        & $command @paths -Control forwards
        Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes("$($pair.Value) HID_MAPPING_PED_MOVE_Y 1 -1")) "key $($pair.Key)"
        $cases++
    }

    $actions = [ordered]@{ forwards = $row; backwards = '43 HID_MAPPING_PED_MOVE_Y 0 -1'; left = '42 HID_MAPPING_PED_MOVE_X 1 -1'; right = '44 HID_MAPPING_PED_MOVE_X 0 -1'; sprint = '54 HID_MAPPING_SPRINT 0 -1' }
    $layout = ($actions.Values -join "`n") + "`n"
    foreach ($action in $actions.GetEnumerator()) {
        Set-Primary $action.Key 'k'
        [IO.File]::WriteAllText($mapping, $layout)
        & $command @paths -Control $action.Key
        $expected = $layout.Replace($action.Value, ('49' + $action.Value.Substring(2)))
        Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes($expected)) "action $($action.Key)"
        $cases++
    }

    # GTA's own arrow-key layout also retains these two gang commands on Up and Down.
    $gang = "79 HID_MAPPING_GANG_RECRUIT_FOLLOW 0 -1`n80 HID_MAPPING_GANG_IGNORE_HOLD_POSITION 0 -1"
    $original = "$row`n43 HID_MAPPING_PED_MOVE_Y 0 -1`n$gang"
    [IO.File]::WriteAllText($mapping, $original)
    [IO.File]::WriteAllText($profile, (Movement-Profile ($up + '<bind key="arrow_d" control="backwards" />')))
    & $command @paths -Control forwards
    & $command @paths -Control backwards
    Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes("79 HID_MAPPING_PED_MOVE_Y 1 -1`n80 HID_MAPPING_PED_MOVE_Y 0 -1`n$gang")) 'verified arrow layout'
    $cases++

    Assert-Refusal 'unbound control' (Movement-Profile '<bind key="k" control="left" />') $row
    Assert-Refusal 'unsupported primary with supported alternate' (Movement-Profile ('<bind key="p" control="forwards" />' + $k)) $row
    Assert-Refusal 'unsupported control' (Movement-Profile '<bind key="k" control="walk" />') $row 'walk'
    Assert-Refusal 'ordinary MTA profile' "<mainconfig><binds>$k</binds></mainconfig>" $row
    Assert-Refusal 'unexpected version' $xml.Replace('version="1"', 'version="2"') $row
    Assert-Refusal 'two binds lists' $xml.Replace('</binds>', "</binds><binds>$up</binds>") $row
    Assert-Refusal 'command inside the dedicated movement file' (Movement-Profile ($k + '<bind key="t" command="chatbox" />')) $row
    Assert-Refusal 'duplicate MTA record' (Movement-Profile ($k + $k)) $row
    Assert-Refusal 'malformed MTA XML' $xml.Substring(0, 45) $row
    Assert-Refusal 'missing target action' $xml '29 HID_MAPPING_ACCELERATE 0 -1'
    Assert-Refusal 'wrong input device' $xml '127 HID_MAPPING_PED_MOVE_Y 1 5'
    Assert-Refusal 'mouse and keyboard both map the selected direction' $xml "$row`n127 HID_MAPPING_PED_MOVE_Y 1 5"
    Assert-Refusal 'malformed target key' $xml '29suffix HID_MAPPING_PED_MOVE_Y 1 -1'
    Assert-Refusal 'unknown target key' $xml '999 HID_MAPPING_PED_MOVE_Y 1 -1'
    Assert-Refusal 'trailing fields on a target row' $xml "$row extra"
    Assert-Refusal 'duplicate retail record' $xml "$row`n$row"
    Assert-Refusal 'different keyboard records for the same direction' $xml "$row`n79 HID_MAPPING_PED_MOVE_Y 1 -1"
    Assert-Refusal 'conflicting on-foot direction' $xml "$row`n49 HID_MAPPING_PED_MOVE_X 0 -1"
    Assert-Refusal 'conflicting jump' $xml "$row`n49 HID_MAPPING_JUMP 0 -1"
    Assert-Refusal 'unrecognized action using the requested key' $xml "$row`n49 HID_MAPPING_UNRECOGNIZED_ACTION 0 -1"
    Assert-Refusal 'gang key assigned to a different direction' (Movement-Profile '<bind key="arrow_u" control="left" />') "42 HID_MAPPING_PED_MOVE_X 1 -1`n$gang" 'left'

    [IO.File]::Delete($mapping)
    [IO.File]::WriteAllText($profile, $xml)
    $failed = $false
    try { & $command @paths -Control forwards } catch { $failed = $true }
    if (!$failed -or [IO.File]::Exists($mapping)) { throw 'Export created an absent retail mapping file' }
    $cases++
    foreach ($processName in @('SanAndreas', 'PlayGTASanAndreas')) {
        $fixtureProcess.Running = $processName
        Assert-Refusal "active $processName" $xml $row
    }
    $fixtureProcess.Running = ''

    # Change each input on disk during the final process check, after parsing and before committing.
    $external = "$row`r`n43 HID_MAPPING_PED_MOVE_Y 0 -1"
    $fixtureProcess.OnCheck = { [IO.File]::WriteAllText($mapping, $external) }.GetNewClosure()
    [IO.File]::WriteAllText($profile, $xml)
    [IO.File]::WriteAllText($mapping, $row)
    $failed = $false
    try { & $command @paths -Control forwards } catch { $failed = $true }
    if (!$failed -or $fixtureProcess.OnCheck) { throw 'Export must detect the external target edit during the commit' }
    Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes($external)) 'concurrent retail edit'
    $cases++

    $reshuffled = Movement-Profile ($up + $k)
    $fixtureProcess.OnCheck = { [IO.File]::WriteAllText($profile, $reshuffled) }.GetNewClosure()
    [IO.File]::WriteAllText($mapping, $row)
    $failed = $false
    try { & $command @paths -Control forwards } catch { $failed = $true }
    if (!$failed -or $fixtureProcess.OnCheck) { throw 'Export must detect a concurrent change to the MTA primary' }
    Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes($row)) 'concurrent primary edit'
    Assert-Bytes $profile ([Text.Encoding]::UTF8.GetBytes($reshuffled)) 'new MTA primary'
    $cases++
    $swap = Whole-Profile @('s','w','d','a','lshift')
    $cycle = Whole-Profile @('s','a','d','lshift','w')
    $original = "  29`tHID_MAPPING_PED_MOVE_Y  1 -1`r`n43 HID_MAPPING_PED_MOVE_Y 0 -1`n42 HID_MAPPING_PED_MOVE_X 1 -1`r44 HID_MAPPING_PED_MOVE_X 0 -1`n54 HID_MAPPING_SPRINT 0 -1`n29 HID_MAPPING_ACCELERATE 0 -1"
    $expectedSwap = "  43`tHID_MAPPING_PED_MOVE_Y  1 -1`r`n29 HID_MAPPING_PED_MOVE_Y 0 -1`n44 HID_MAPPING_PED_MOVE_X 1 -1`r42 HID_MAPPING_PED_MOVE_X 0 -1`n54 HID_MAPPING_SPRINT 0 -1`n29 HID_MAPPING_ACCELERATE 0 -1"
    $expectedCycle = "  43`tHID_MAPPING_PED_MOVE_Y  1 -1`r`n42 HID_MAPPING_PED_MOVE_Y 0 -1`n44 HID_MAPPING_PED_MOVE_X 1 -1`r54 HID_MAPPING_PED_MOVE_X 0 -1`n29 HID_MAPPING_SPRINT 0 -1`n29 HID_MAPPING_ACCELERATE 0 -1"
    foreach ($encoding in @([Text.UTF8Encoding]::new($false), [Text.UTF8Encoding]::new($true), [Text.Encoding]::Unicode,
                            [Text.Encoding]::BigEndianUnicode, [Text.Encoding]::UTF32, [Text.UTF32Encoding]::new($true, $true))) {
        [IO.File]::WriteAllText($mapping, $original, $encoding)
        $withAlternates = $swap.Replace('</binds>', '<bind control="forwards" key="p" /><bind control="walk" key="rshift" /></binds>')
        [IO.File]::WriteAllText($profile, $withAlternates)
        $before = [IO.File]::ReadAllBytes($mapping)
        $checksBefore = $fixtureProcess.Checks
        & $command @paths -WholeProfile -WhatIf
        Assert-Bytes $mapping $before 'whole profile preview'
        & $command @paths -WholeProfile
        if ($fixtureProcess.Checks -ne $checksBefore + 1) { throw 'Whole-profile preview and apply did not perform exactly one guarded commit' }
        $expected = [byte[]]($encoding.GetPreamble() + $encoding.GetBytes($expectedSwap))
        Assert-Bytes $mapping $expected 'two simultaneous swaps'
        [IO.File]::SetLastWriteTimeUtc($mapping, $stamp)
        & $command @paths -WholeProfile
        if ([IO.File]::GetLastWriteTimeUtc($mapping) -ne $stamp) { throw 'A whole-profile no-op rewrote the file' }
        if ($fixtureProcess.Checks -ne $checksBefore + 1) { throw 'A whole-profile no-op entered the writer' }
        [IO.File]::WriteAllText($profile, $cycle)
        & $command @paths -WholeProfile
        Assert-Bytes $mapping ([byte[]]($encoding.GetPreamble() + $encoding.GetBytes($expectedCycle))) 'five-control cycle'
        $cases++
    }
    Assert-WholeRefusal 'missing primary' (Whole-Profile @('s','w','d','a')) $layout 'sprint'
    Assert-WholeRefusal 'unsupported primary before a supported alternate' ($swap.Replace('control="sprint" key="lshift"', 'control="sprint" key="p" /><bind control="sprint" key="lshift"')) $layout 'Unsupported.*sprint'
    Assert-WholeRefusal 'duplicate final primary' (Whole-Profile @('s','w','d','a','s')) $layout 'same|both|duplicate'
    Assert-WholeRefusal 'ambiguous final action' $swap ($layout + $actions.sprint) 'ambiguous.*sprint'
    Assert-WholeRefusal 'absent GTA action' $swap ($layout.Replace($actions.right, '')) 'no existing.*right'
    Assert-WholeRefusal 'on-foot conflict for the new owner of a key' $swap ($layout + '43 HID_MAPPING_JUMP 0 -1') 'HID_MAPPING_JUMP'
    Assert-WholeRefusal 'on-foot conflict even on unchanged final key' $swap ($layout + '54 HID_MAPPING_SOMETHING_NEW 0 -1') 'HID_MAPPING_SOMETHING_NEW'
    Assert-WholeRefusal 'conflicting right key and gang follow' (Whole-Profile @('w','s','a','arrow_u','lshift')) ($layout + $gang) 'HID_MAPPING_GANG_RECRUIT_FOLLOW'
    [IO.File]::WriteAllText($mapping, $layout + $gang)
    [IO.File]::WriteAllText($profile, (Whole-Profile @('arrow_u','arrow_d','arrow_l','arrow_r','lshift')))
    & $command @paths -WholeProfile
    $expectedArrows = "79 HID_MAPPING_PED_MOVE_Y 1 -1`n80 HID_MAPPING_PED_MOVE_Y 0 -1`n81 HID_MAPPING_PED_MOVE_X 1 -1`n82 HID_MAPPING_PED_MOVE_X 0 -1`n54 HID_MAPPING_SPRINT 0 -1`n$gang"
    Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes($expectedArrows)) 'whole arrow layout keeps its stock gang mappings'
    $cases++
    $fixtureProcess.Running = 'PlayGTASanAndreas'
    Assert-WholeRefusal 'active game prevents all five edits' $cycle $layout 'Close GTA'
    $fixtureProcess.Running = ''
    $fixtureProcess.OnCheck = { [IO.File]::WriteAllText($profile, $cycle) }.GetNewClosure()
    Assert-WholeRefusal 'source changed during the whole commit' $swap $layout 'MTA bindings changed'
    $external = $layout + '68 HID_MAPPING_JUMP 0 -1'
    [IO.File]::WriteAllText($profile, $swap)
    [IO.File]::WriteAllText($mapping, $layout)
    $fixtureProcess.OnCheck = { [IO.File]::WriteAllText($mapping, $external) }.GetNewClosure()
    $message = ''
    try { & $command @paths -WholeProfile } catch { $message = $_.Exception.Message }
    if ($message -notmatch 'GTA settings changed') { throw "Concurrent whole-profile target edit was lost: $message" }
    Assert-Bytes $mapping ([Text.Encoding]::UTF8.GetBytes($external)) 'concurrent target edit during whole commit'
    $cases++
    if ($CapturedStockKeyboardMappings) {
        $capture = (Resolve-Path -LiteralPath $CapturedStockKeyboardMappings).Path
        $captureBytes = [IO.File]::ReadAllBytes($capture)
        [IO.File]::WriteAllBytes($mapping, $captureBytes)
        [IO.File]::WriteAllText($profile, $cycle)
        & $command @paths -WholeProfile
        $cycleLayout = [IO.File]::ReadAllText($mapping)
        foreach ($line in @('43 HID_MAPPING_PED_MOVE_Y 1 -1','42 HID_MAPPING_PED_MOVE_Y 0 -1','44 HID_MAPPING_PED_MOVE_X 1 -1',
                            '54 HID_MAPPING_PED_MOVE_X 0 -1','29 HID_MAPPING_SPRINT 0 -1')) {
            if ($line -cnotin ($cycleLayout -split '\r?\n')) { throw "Expected GTA capture row $line" }
        }
        [IO.File]::WriteAllText($profile, (Whole-Profile @('w','s','a','d','lshift')))
        & $command @paths -WholeProfile
        Assert-Bytes $mapping $captureBytes 'captured stock file restored byte for byte'
        Assert-Bytes $capture $captureBytes 'original GTA capture'
        $cases++
    }
    Write-Output "Passed $cases retail movement export cases."
} finally {
    [IO.File]::Delete($profile)
    [IO.File]::Delete($mapping)
    # Deleting the directory non-recursively also checks that the command cleaned up temporary writes.
    [IO.Directory]::Delete($scratch)
}
