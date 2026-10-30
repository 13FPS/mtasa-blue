# Inherit only actual numeric Create. All other established manager methods
# keep their strict rejectors; no CGame or successful fake view is generated.
function Write-AnimationAssociationNumericFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1')
    $parent=Write-AnimationManagerFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $pattern='(?m)^\s*AnimBlendAssoc_type\s+CreateAnimAssociation\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual numeric Create rejector changed'}
    $text=[regex]::Replace($text,$pattern,'')
    $text=$text.Replace('AnimationManagerFixtureReceiver','AnimationAssociationNumericFactoryManagerReceiver')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-1);inheritedFactories=@('CreateAnimAssociation(AssocGroupId,AnimationId)');
        receiver='AnimationAssociationNumericFactoryManagerReceiver';
        parentGenerator='Tests/definitive/Write-AnimationManagerFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        sdkInterface=$parent.source;sdkInterfaceSha256=$parent.sourceSha256;
        completeManagerSupplied=$false;completeGameSupplied=$false;successfulCodeViewForged=$false}
}
