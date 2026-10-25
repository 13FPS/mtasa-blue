# Compile actual manager acknowledgement policy with only its native entry
# substituted. The fixture never supplies another Entered/Returned algorithm.
function Write-ClassicAnimationOperationBridge([string]$Root,[string]$Directory,[ValidateSet('classic','common')][string]$Scope) {
    $cppPath=Join-Path $Root 'Client/game_sa/CAnimManagerSA.cpp'
    $headerPath=Join-Path $Root 'Client/game_sa/CAnimManagerSA.h'
    $cpp=[IO.File]::ReadAllText($cppPath)
    $header=[IO.File]::ReadAllText($headerPath)
    $specs=@([ordered]@{signature='void CAnimManagerSA::FreeKeyFramesMemory(void* pKeyFrames)';native='NativeFreeEntry(pKeyFrames, dwFunc);';symbol='FUNC_CAnimManager_FreeKeyFramesMemory'})
    if($Scope -eq 'common') {
        $specs+=@([ordered]@{signature='BYTE* CAnimManagerSA::AllocateKeyFramesMemory(uint32_t u32BytesToAllocate)';native='pKeyFrames = NativeAllocateEntry(u32BytesToAllocate, dwFunc);';symbol='FUNC_CAnimManager_AllocateKeyFramesMemory'},
            [ordered]@{signature='void CAnimManagerSA::DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface* pInterface)';native='NativeDeleteStandaloneEntry(pInterface);';symbol=$null})
    }
    $lines=@('// Generated from actual current CAnimManagerSA.cpp; acknowledgement policy is unchanged.')
    $constants=@('#pragma once')
    $records=@()
    foreach($spec in $specs) {
        $start=$cpp.IndexOf($spec.signature,[StringComparison]::Ordinal)
        if($start -lt 0 -or $cpp.IndexOf($spec.signature,$start+1,[StringComparison]::Ordinal) -ge 0){throw ('Actual classic manager signature changed: '+$spec.signature)}
        $brace=$cpp.IndexOf('{',$start);$depth=0;$end=-1
        for($i=$brace;$i -lt $cpp.Length;$i++) {
            if($cpp[$i] -eq '{'){$depth++}
            elseif($cpp[$i] -eq '}'){$depth--;if(-not $depth){$end=$i;break}}
        }
        if($end -lt 0){throw 'Actual classic manager body is incomplete'}
        $original=$cpp.Substring($start,$end-$start+1)
        $selected=$original.Replace('CAnimManagerSA::','Manager::')
        if($spec.symbol) {
            $definition=[regex]::Match($header,'(?m)^#define\s+'+[regex]::Escape($spec.symbol)+'\s+(?<value>0x[0-9a-fA-F]+)\s*$')
            if(-not $definition.Success){throw ('Actual native entry constant missing: '+$spec.symbol)}
            $constants+=@(('#ifndef '+$spec.symbol),('#define '+$spec.symbol+' '+$definition.Groups['value'].Value),'#endif')
            $asm=[regex]::Matches($selected,'(?s)__asm\s*\{[^{}]*\}')
            if($asm.Count -ne 1){throw 'Actual single native asm entry changed'}
            $selected=$selected.Substring(0,$asm[0].Index)+$spec.native+$selected.Substring($asm[0].Index+$asm[0].Length)
        } else {
            $entry='delete pInterface;'
            if([regex]::Matches($selected,[regex]::Escape($entry)).Count -ne 1){throw 'Actual metadata-only delete entry changed'}
            $selected=$selected.Replace($entry,$spec.native)
        }
        $lines+=@($selected)
        $sha=[Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($original))
        $records+=@([ordered]@{signature=$spec.signature;actualBodySha256=[Convert]::ToHexString($sha).ToLowerInvariant();
            onlySubstitution=$spec.native;nativeSymbol=$spec.symbol;sourceAcknowledgementPolicyPreserved=$true})
    }
    $output=Join-Path $Directory 'ClassicAnimationOperationBridge.generated.inl'
    [IO.File]::WriteAllLines($output,$lines,[Text.UTF8Encoding]::new($false))
    $constantOutput=Join-Path $Directory 'ClassicAnimationOperationEntries.generated.h'
    [IO.File]::WriteAllLines($constantOutput,$constants,[Text.UTF8Encoding]::new($false))
    [ordered]@{constantPath=$constantOutput;constantSha256=(Get-FileHash -LiteralPath $constantOutput -Algorithm SHA256).Hash.ToLowerInvariant();path=$output;sha256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant();scope=$Scope;
        sourceInputs=@('Client/game_sa/CAnimManagerSA.cpp','Client/game_sa/CAnimManagerSA.h'|ForEach-Object{
            [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $Root $_) -Algorithm SHA256).Hash.ToLowerInvariant()}
        });methods=$records;nativeEngineCodeExecuted=$false;acknowledgementPolicyMirrored=$false}
}
