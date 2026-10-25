# Extract the actual three classic callsite bodies. Only dependency class/type
# names and the native asm entry are substituted; the source precheck remains.
function Write-AnimationIdValidityClassicBridge([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/game_sa/CAnimManagerSA.cpp'
    $sourcePath=Join-Path $RepositoryRoot $source
    $text=[IO.File]::ReadAllText($sourcePath)
    $group=[regex]::Matches($text,'(?ms)^bool CAnimManagerSA::IsValidGroup\(std::uint32_t uiAnimGroup\) const\s*\{.*?^\}')
    $valid=[regex]::Matches($text,'(?ms)^bool CAnimManagerSA::IsValidAnim\(std::uint32_t uiAnimGroup, std::uint32_t uiAnimID\) const\s*\{.*?^\}')
    $association=[regex]::Matches($text,'(?ms)^CAnimManagerSA::StaticAssocIntface_type CAnimManagerSA::GetAnimStaticAssociation\(eAnimGroup animGroup, eAnimID animID\) const\s*\{.*?^\}')
    if($group.Count -ne 1 -or $valid.Count -ne 1 -or $association.Count -ne 1){throw 'Actual classic validity itinerary is not unique'}
    if(-not $valid[0].Value.Contains('CAnimValidityPolicy::IsValidAnim') -or -not $valid[0].Value.Contains('GetAnimStaticAssociation') -or
       -not $association[0].Value.Contains('if (!IsValidGroup(') -or -not $association[0].Value.Contains('sAnimID') -and -not $valid[0].Value.Contains('sAnimID')) {
        throw 'Actual classic enum/precheck/signed-ID itinerary changed'
    }
    $method=$association[0].Value
    $asm=[regex]::Matches($method,'(?ms)^\s*__asm\s*\{.*?^\s*\}')
    if($asm.Count -ne 1 -or -not $asm[0].Value.Contains('push    animID') -or -not $asm[0].Value.Contains('push    animGroup') -or
       -not $asm[0].Value.Contains('mov     pInterface, eax')) {throw 'Actual association native-entry seam changed'}
    $method=$method.Replace($asm[0].Value,[Environment]::NewLine+'    pInterface = ReadStaticAssociationEntry(animGroup, animID, dwFunc);')
    $method=$method.Replace('CAnimBlendStaticAssociationSAInterface','ClassicStaticAssociationInterface').Replace('CAnimBlendStaticAssociationSA','ClassicStaticAssociationFacade')
    $headerPath=Join-Path $RepositoryRoot 'Client/game_sa/CAnimManagerSA.h'
    $defines=[regex]::Matches([IO.File]::ReadAllText($headerPath),'(?m)^#define\s+FUNC_CAnimManager_GetAnimAssociation\s+(0x[0-9a-fA-F]+)\s*$')
    if($defines.Count -ne 1 -or [Convert]::ToUInt32($defines[0].Groups[1].Value.Substring(2),16) -ne 0x4d3a60){throw 'Actual source function macro changed'}
    $original=$group[0].Value+[Environment]::NewLine+$association[0].Value+[Environment]::NewLine+$valid[0].Value
    $result=('#define FUNC_CAnimManager_GetAnimAssociation '+$defines[0].Groups[1].Value+[Environment]::NewLine+
        $group[0].Value+[Environment]::NewLine+$method+[Environment]::NewLine+$valid[0].Value).Replace('CAnimManagerSA::','ClassicAnimationIdValidityReceiver::')
    [IO.File]::WriteAllText($OutputPath,$result+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
    $hasher=[Security.Cryptography.SHA256]::Create()
    try{$methodsHash=[Convert]::ToHexString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($original))).ToLowerInvariant()}finally{$hasher.Dispose()}
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();source=$source;
        sourceSha256=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant();methodsSha256=$methodsHash;
        header='Client/game_sa/CAnimManagerSA.h';headerSha256=(Get-FileHash -LiteralPath $headerPath -Algorithm SHA256).Hash.ToLowerInvariant();
        transformations=@('Rename owning class qualifier','Substitute dependency-only interface/facade type names','Substitute only actual __asm entry with strict address/group/ID observer');
        wholeClassicBackendClaimed=$false;nativeCodeExecuted=$false;nativeInterfaceLayoutClaimed=$false}
}
