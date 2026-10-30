# Keep the actual uncompress call and its existing bank hierarchy factories.
# Every unrelated manager method remains a rejecting test-only override.
function Write-AnimationManagerUncompressFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    foreach($pattern in @(
        '(?m)^\s*void\s+RemoveFromUncompressedCache\(CAnimBlendHierarchy\*[^\r\n]+\r?\n',
        '(?m)^\s*void\s+RemoveFromUncompressedCache\(CAnimBlendHierarchySAInterface\*[^\r\n]+\r?\n',
        '(?m)^\s*void\s+UncompressAnimation\([^\r\n]+\r?\n',
        '(?m)^\s*std::unique_ptr<CAnimBlendHierarchy>\s+GetAnimBlendHierarchy\([^\r\n]+\r?\n',
        '(?m)^\s*std::unique_ptr<CAnimBlendHierarchy>\s+GetAnimation\(int[^\r\n]+\r?\n')) {
        if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual uncompress/bank hierarchy rejector changed'}
        $text=[regex]::Replace($text,$pattern,'')
    }
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationManagerUncompressReceiver')
    $needle='class AnimationManagerUncompressReceiver final : public mta::game_de::CAnimManagerDE { public:'
    if(-not $text.Contains($needle)){throw 'Actual strict manager ancestry changed'}
    $text=$text.Replace($needle,$needle+"`n"+
        'using mta::game_de::CAnimManagerDE::GetAnimation;'+"`n"+
        'AnimationManagerUncompressReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationBlockAccess& blocks,mta::game_de::NativeAnimationHierarchyAccess& hierarchy,std::uint64_t generation) : CAnimManagerDE(manager,blocks,hierarchy,generation) {}')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-5);inheritedMethods=@('RemoveFromUncompressedCache(CAnimBlendHierarchy*)','RemoveFromUncompressedCache(raw)','UncompressAnimation(CAnimBlendHierarchy*)','GetAnimBlendHierarchy(raw)','GetAnimation(int)');
        receiver='AnimationManagerUncompressReceiver';
        parentGenerator='Tests/definitive/Write-AnimationManagerFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        sdkInterface=$parent.source;sdkInterfaceSha256=$parent.sourceSha256;
        completeManagerSupplied=$false;completeGameSupplied=$false}
}
