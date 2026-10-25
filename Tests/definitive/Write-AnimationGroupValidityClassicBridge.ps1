# Derive the exact actual classic IsValidGroup body after formatting. Only its
# owning class qualifier is changed to a dependency shell; policy/body stay real.
function Write-AnimationGroupValidityClassicBridge([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/game_sa/CAnimManagerSA.cpp'
    $sourcePath=Join-Path $RepositoryRoot $source
    $text=[IO.File]::ReadAllText($sourcePath)
    $methodMatches=[regex]::Matches($text,'(?ms)^bool CAnimManagerSA::IsValidGroup\(std::uint32_t uiAnimGroup\) const\s*\{.*?^\}')
    if($methodMatches.Count -ne 1){throw 'Actual classic IsValidGroup method is not unique'}
    $method=$methodMatches[0].Value
    if(-not $method.Contains('CAnimGroupValidationPolicy::IsValidGroup') -or -not $method.Contains('GetAnimBlendAssoc') -or -not $method.Contains('IsCreated')) {
        throw 'Actual source gate/provider itinerary changed'
    }
    $result=$method.Replace('CAnimManagerSA::','ClassicAnimationGroupValidityReceiver::')
    [IO.File]::WriteAllText($OutputPath,$result+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
    $bytes=[Text.Encoding]::UTF8.GetBytes($method)
    $hasher=[Security.Cryptography.SHA256]::Create()
    try{$methodHash=[Convert]::ToHexString($hasher.ComputeHash($bytes)).ToLowerInvariant()}finally{$hasher.Dispose()}
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();source=$source;
        sourceSha256=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant();methodSha256=$methodHash;
        transformations=@('Rename owning class qualifier only');wholeClassicBackendClaimed=$false;nativeCodeExecuted=$false}
}
