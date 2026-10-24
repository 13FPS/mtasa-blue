# Keep the existing strict manager receiver and remove only the new actual
# gateway abortor. Preserve an unmodified parent artifact and its receipt.
function Write-AnimationGatewayFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerRefsFixture.ps1')
    $parentPath=$OutputPath+'.refs-parent.h'
    $parent=Write-AnimationManagerRefsFixture $RepositoryRoot $parentPath
    $text=[IO.File]::ReadAllText($parentPath)
    $pattern='(?m)^.*\bisGateWayAnimationHierarchy\([^\r\n]+override[^\r\n]+\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Expected one strict gateway abortor in parent receiver'}
    $text=[regex]::Replace($text,$pattern,'')
    $text=$text.Replace('AnimationManagerRefsReceiver','AnimationGatewayReceiver')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=($parent.rejectingMethods-1);parentFixture=$parent;inheritedGatewayMethod='isGateWayAnimationHierarchy';
        source='Tests/definitive/Write-AnimationManagerRefsFixture.ps1';
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationManagerRefsFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        completeGameSupplied=$false;productionManagerConcrete=$false}
}
