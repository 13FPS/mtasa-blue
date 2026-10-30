# Inherit the actual empty-block SDK dispatch; every unrelated SDK method still rejects.
function Write-AnimationManagerEmptyBlockRemovalFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerRegistrationFixture.ps1')
    $parent=Write-AnimationManagerRegistrationFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $pattern='(?m)^\s*void\s+RemoveAnimBlock\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual RemoveAnimBlock rejector changed'}
    $text=[regex]::Replace($text,$pattern,'')
    if([regex]::IsMatch($text,'\bRemoveAnimBlock\([^\r\n]*\)\s*override')){throw 'Actual RemoveAnimBlock was not inherited'}
    $text=$text.Replace('AnimationManagerRegistrationReceiver','AnimationManagerEmptyBlockRemovalReceiver')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-1);inheritedMethods=(@($parent.inheritedMethods)+@('RemoveAnimBlock'));
        receiver='AnimationManagerEmptyBlockRemovalReceiver';completeGameSupplied=$false;completeManagerSupplied=$false;
        source=$parent.source;sourceSha256=$parent.sourceSha256}
}
