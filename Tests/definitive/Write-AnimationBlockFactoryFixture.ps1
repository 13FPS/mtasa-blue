# Reuse the actual strict manager generator and remove only the newly supplied
# name/raw/index block factories. All remaining manager methods retain their
# rejecting test-only bodies.
function Write-AnimationBlockFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    foreach($pattern in @(
        '(?m)^\s*std::unique_ptr<CAnimBlock>\s+GetAnimationBlock\(const char\*[^\r\n]+\r?\n',
        '(?m)^\s*std::unique_ptr<CAnimBlock>\s+GetAnimationBlock\(int[^\r\n]+\r?\n',
        '(?m)^\s*std::unique_ptr<CAnimBlock>\s+GetAnimBlock\(CAnimBlockSAInterface\*[^\r\n]+\r?\n')) {
        if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual manager block-factory rejector changed.'}
        $text=[regex]::Replace($text,$pattern,'')
    }
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationBlockFactoryManagerReceiver')
    $needle='class AnimationBlockFactoryManagerReceiver final : public mta::game_de::CAnimManagerDE { public:'
    if(-not $text.Contains($needle)){throw 'Actual strict animation manager ancestry changed.'}
    $text=$text.Replace($needle,$needle+"`n"+
        'using mta::game_de::CAnimManagerDE::GetAnimationBlock;'+"`n"+
        'AnimationBlockFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationBlockAccess& blocks,CGame& game,std::uint64_t generation) : CAnimManagerDE(manager,blocks,game,generation) {}')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    # This helper transforms the final output in place; return its actual hash,
    # not the sealed intermediate hash from the reused parent generator.
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-3);qualifiedFactoryMethods=@('GetAnimationBlock(int)','GetAnimationBlock(const char*)','GetAnimBlock(CAnimBlockSAInterface*)');
        parentGenerator='Tests/definitive/Write-AnimationManagerFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        sdkInterface=$parent.source;sdkInterfaceSha256=$parent.sourceSha256;
        productionFactoryPublished=$false;completeManagerSupplied=$false;completeGameSupplied=$false}
}
