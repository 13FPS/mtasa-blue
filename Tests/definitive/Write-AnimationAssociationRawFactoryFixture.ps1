# Exercise both actual issued factories while rejecting unrelated manager operations.
# The helper creates no fake successful native identity or CPU code view.
function Write-AnimationAssociationRawFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationAssociationNumericFactoryFixture.ps1')
    $parent=Write-AnimationAssociationNumericFactoryFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $pattern='(?m)^\s*AnimBlendAssoc_type\s+GetAnimBlendAssociation\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Actual raw GetAnimBlendAssociation rejector changed'}
    $text=[regex]::Replace($text,$pattern,'')
    $text=$text.Replace('AnimationAssociationNumericFactoryManagerReceiver','AnimationAssociationRawFactoryManagerReceiver')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-1);inheritedFactories=@('CreateAnimAssociation(AssocGroupId,AnimationId)','GetAnimBlendAssociation(raw)');
        receiver='AnimationAssociationRawFactoryManagerReceiver';
        parentGenerator='Tests/definitive/Write-AnimationAssociationNumericFactoryFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationAssociationNumericFactoryFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        sdkInterface=$parent.sdkInterface;sdkInterfaceSha256=$parent.sdkInterfaceSha256;
        completeManagerSupplied=$false;completeGameSupplied=$false;successfulCodeViewForged=$false}
}
