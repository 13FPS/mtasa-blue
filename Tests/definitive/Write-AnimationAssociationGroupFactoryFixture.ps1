# Inherit the actual two owning group factories. Every other method from the
# established strict manager fixture keeps its rejecting test-only body.
function Write-AnimationAssociationGroupFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    foreach($pattern in @(
        '(?m)^\s*AnimAssocGroup_type\s+GetAnimBlendAssoc\([^\r\n]+\r?\n',
        '(?m)^\s*AnimAssocGroup_type\s+GetAnimBlendAssocGroup\([^\r\n]+\r?\n')) {
        if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual manager group-factory rejector changed.'}
        $text=[regex]::Replace($text,$pattern,'')
    }
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationAssociationGroupFactoryManagerReceiver')
    $needle='class AnimationAssociationGroupFactoryManagerReceiver final : public mta::game_de::CAnimManagerDE { public:'
    if(-not $text.Contains($needle)){throw 'Actual strict manager fixture ancestry changed.'}
    $text=$text.Replace($needle,$needle+"`n"+
        'AnimationAssociationGroupFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationBlockAccess& blocks,CGame& game,std::uint64_t generation) : CAnimManagerDE(manager,blocks,game,generation) {}')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-2);inheritedGroupFactories=@('GetAnimBlendAssoc(AssocGroupId) const','GetAnimBlendAssocGroup(CAnimBlendAssocGroupSAInterface*)');
        parentGenerator='Tests/definitive/Write-AnimationManagerFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        sdkInterface=$parent.source;sdkInterfaceSha256=$parent.sourceSha256;
        completeManagerSupplied=$false;completeGameSupplied=$false;nativeFactoryExecuted=$false;owningCpuProducts=$true}
}
