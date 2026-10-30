# Inherit the actual registration and matched MTA reference policies. The leaf
# rejects unrelated SDK methods rather than supplying a fake game or counter.
function Write-AnimationManagerRegistrationFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $methods=@('RegisterAnimBlock','AddAnimBlockRef','GetNumRefsToAnimBlock','RemoveAnimBlockRef','RemoveAnimBlockRefWithoutDelete')
    foreach($method in $methods) {
        $pattern='(?m)^\s*(?:int|void)\s+'+[regex]::Escape($method)+'\([^\r\n]+\r?\n'
        if([regex]::Matches($text,$pattern).Count -ne 1){throw ('Actual registration/reference rejector changed: '+$method)}
        $text=[regex]::Replace($text,$pattern,'')
    }
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationManagerRegistrationReceiver')
    $constructor='AnimationManagerRegistrationReceiver(mta::game_de::NativeAnimationManagerAccess& access,std::uint64_t generation) noexcept : CAnimManagerDE(access,generation) {}'
    if([regex]::Matches($text,[regex]::Escape($constructor)).Count -ne 1){throw 'Actual rejecting fixture constructor changed'}
    $withBlocks='AnimationManagerRegistrationReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationBlockAccess& blocks,std::uint64_t generation) : CAnimManagerDE(manager,blocks,generation) {}'
    $text=$text.Replace($constructor,$constructor+[Environment]::NewLine+$withBlocks)
    $inherited=@('GetNumAnimations','GetNumAnimBlocks','GetAnimationBlockIndex')+$methods
    foreach($method in $inherited) {
        if([regex]::IsMatch($text,'\b'+[regex]::Escape($method)+'\([^\r\n]*\)\s*override')){throw ('Actual SDK method was not inherited: '+$method)}
    }
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-$methods.Count);inheritedMethods=$inherited;
        receiver='AnimationManagerRegistrationReceiver';completeGameSupplied=$false;completeManagerSupplied=$false;
        source=$parent.source;sourceSha256=$parent.sourceSha256}
}
