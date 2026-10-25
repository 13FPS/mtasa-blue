# Keep actual storage admission/ACK policy while the parser fixture selects
# classic storage or its existing explicit opaque test-layout allocator entry.
function Write-ClassicAnimationArrayBridge([string]$Root,[string]$Directory) {
    $source='Client/game_sa/CAnimManagerSA.cpp'
    $text=[IO.File]::ReadAllText((Join-Path $Root $source))
    $specs=@(
        [ordered]@{signature='CAnimBlendSequenceSAInterface* CAnimManagerSA::AllocateCustomAnimSequences(size_t capacity)';
            allocate=$true},
        [ordered]@{signature='void CAnimManagerSA::FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* sequences)';
            allocate=$false})
    $lines=@('// Actual current CAnimManagerSA storage methods; allocator entry only is substituted.')
    $methods=@()
    foreach($spec in $specs) {
        $start=$text.IndexOf($spec.signature,[StringComparison]::Ordinal)
        if($start -lt 0 -or $text.IndexOf($spec.signature,$start+1,[StringComparison]::Ordinal) -ge 0){throw 'Actual classic array method is not unique'}
        $brace=$text.IndexOf('{',$start);$depth=0;$end=-1
        for($i=$brace;$i -lt $text.Length;$i++) {
            if($text[$i] -eq '{'){$depth++}
            elseif($text[$i] -eq '}'){$depth--;if(-not $depth){$end=$i;break}}
        }
        if($end -lt 0){throw 'Actual array method body is incomplete'}
        $body=$text.Substring($start,$end-$start+1)
        $selected=$body.Replace('CAnimManagerSA::','Manager::')
        if($spec.allocate) {
            $entry='MtaGame::CAnimSequenceStorageSA::Allocate(capacity)'
            if([regex]::Matches($selected,[regex]::Escape($entry)).Count -ne 1){throw 'Actual array allocation entry changed'}
            $selected=$selected.Replace($entry,'NativeAllocateArrayEntry(capacity)')
        }
        $free='MtaGame::CAnimSequenceStorageSA::Free(sequences);'
        if([regex]::Matches($selected,[regex]::Escape($free)).Count -ne 1){throw 'Actual array storage release entry changed'}
        $selected=$selected.Replace($free,'NativeFreeArrayEntry(sequences);')
        $lines+=@($selected)
        $sha=[Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body))
        $methods+=@([ordered]@{signature=$spec.signature;actualBodySha256=[Convert]::ToHexString($sha).ToLowerInvariant();
            onlyAllocatorEntriesSubstituted=$true;sourceAcknowledgementPolicyPreserved=$true})
    }
    $output=Join-Path $Directory 'ClassicAnimationArrayBridge.generated.inl'
    [IO.File]::WriteAllLines($output,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$output;sha256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant();methods=$methods;
        sourceInputs=@([ordered]@{path=$source;sha256=(Get-FileHash -LiteralPath (Join-Path $Root $source) -Algorithm SHA256).Hash.ToLowerInvariant()});
        nativeEngineCodeExecuted=$false;acknowledgementPolicyMirrored=$false}
}
