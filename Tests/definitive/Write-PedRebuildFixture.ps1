# Reuse the existing strict clothes leaf definitions; remove only its
# RebuildPlayer rejector so the new actual inherited SDK override is exercised.
function Write-PedRebuildFixture([string]$taskRoot,[string]$OutputPath,[string]$RebuildHeaderPath='') {
    if(-not $RebuildHeaderPath){$RebuildHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedRebuildDE.h'}
    $header=[IO.File]::ReadAllText($RebuildHeaderPath)
    if($header -notmatch 'class\s+CPedRebuildDE\s*:\s*public\s+CPedClothesDE' -or
       $header -notmatch 'void\s+RebuildPlayer\s*\(\)\s*override;'){throw 'Actual rebuild override/ancestry is missing.'}
    . (Join-Path $taskRoot 'Tests/definitive/Write-PedClothesFixture.ps1')
    $parentPath=Join-Path (Split-Path -Parent $OutputPath) 'PedClothesFixture.generated.h'
    $base=Write-PedClothesFixture $taskRoot $parentPath
    $text=[IO.File]::ReadAllText($parentPath)
    $rejector='(?m)^.*\bRebuildPlayer\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$rejector).Count -ne 1){throw 'Existing strict rebuild rejector is not unique.'}
    $text=[regex]::Replace($text,$rejector,'')
    $text=$text.Replace('ped_clothes_fixture','ped_rebuild_fixture').Replace('PedClothesFixture','PedRebuildFixture')
    $old='class PedRebuildFixture final : public mta::game_de::CPedClothesDE'
    if(-not $text.Contains($old)){throw 'Existing strict leaf ancestry changed.'}
    $text=$text.Replace($old,'class PedRebuildFixture final : public mta::game_de::CPedRebuildDE')
    $old='mta::game_de::NativePedClothesAccess& clothes,const mta::game_de::NativePedReferenceToken& token'
    if(-not $text.Contains($old)){throw 'Existing strict leaf constructor changed.'}
    $text=$text.Replace($old,'mta::game_de::NativePedClothesAccess& clothes,mta::game_de::NativePedRebuildAccess& rebuild,const mta::game_de::NativePedReferenceToken& token')
    $text=$text.Replace(': CPedClothesDE(',': CPedRebuildDE(').Replace('onScreen,grant,clear,clothes,token,wrapperType','onScreen,grant,clear,clothes,rebuild,token,wrapperType')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingPedOverrides=($base.rejectingPedOverrides-1);rejectingGameOverrides=$base.rejectingGameOverrides;
        testedMethod='RebuildPlayer';inheritedProductionMethods=@($base.inheritedProductionMethods)+@('RebuildPlayer');
        parentFixture=$base;parentGenerator='Tests/definitive/Write-PedClothesFixture.ps1';
        parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'Tests/definitive/Write-PedClothesFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        historicalTestCasesExecuted=$false;productionFactoriesPublished=$false}
}
