# Inherit the actual manager loaded query while every unrelated SDK method rejects.
function Write-AnimationManagerGroupLoadedFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $pattern='(?m)^\s*bool\s+HasAnimGroupLoaded\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual HasAnimGroupLoaded rejector changed'}
    $text=[regex]::Replace($text,$pattern,'')
    if([regex]::IsMatch($text,'\bHasAnimGroupLoaded\([^\r\n]*\)\s*override')){throw 'Actual HasAnimGroupLoaded was not inherited'}
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationManagerGroupLoadedReceiver')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-1);inheritedMethods=@('GetNumAnimations','GetNumAnimBlocks','GetAnimationBlockIndex','HasAnimGroupLoaded');
        receiver='AnimationManagerGroupLoadedReceiver';completeGameSupplied=$false;completeManagerSupplied=$false;
        source=$parent.source;sourceSha256=$parent.sourceSha256}
}
