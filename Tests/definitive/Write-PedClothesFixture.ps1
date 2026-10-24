# Reuse the actual ClearWeapon chain's strict SDK rejector generator; only
# its test leaf changes ancestry so this fixture inherits the new clothes method.
function Write-PedClothesFixture([string]$taskRoot,[string]$OutputPath,[string]$ClothesHeaderPath='') {
    if(-not $ClothesHeaderPath){$ClothesHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedClothesDE.h'}
    $header=[IO.File]::ReadAllText($ClothesHeaderPath)
    if($header -notmatch 'class\s+CPedClothesDE\s*:\s*public\s+CPedClearWeaponDE' -or
       $header -notmatch 'void\s+SetClothesTextureAndModel\s*\(const char\* texture, const char\* model, int textureType\)\s*override;'){
        throw 'Actual clothes SDK override/ancestry is missing.'
    }
    . (Join-Path $taskRoot 'Tests/definitive/Write-PedClearWeaponFixture.ps1')
    $base=Write-PedClearWeaponFixture $taskRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $rejector='(?m)^.*\bSetClothesTextureAndModel\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$rejector).Count -ne 1){throw 'Existing strict clothes rejector is not unique.'}
    $text=[regex]::Replace($text,$rejector,'')
    $text=$text.Replace('ped_clear_weapon_fixture','ped_clothes_fixture').Replace('PedClearWeaponFixture','PedClothesFixture')
    $old='class PedClothesFixture final : public mta::game_de::CPedClearWeaponDE'
    if(-not $text.Contains($old)){throw 'Existing strict leaf ancestry changed.'}
    $text=$text.Replace($old,'class PedClothesFixture final : public mta::game_de::CPedClothesDE')
    $old='mta::game_de::NativePedWeaponClearAccess& clear,const mta::game_de::NativePedReferenceToken& token'
    if(-not $text.Contains($old)){throw 'Existing strict leaf constructor changed.'}
    $text=$text.Replace($old,'mta::game_de::NativePedWeaponClearAccess& clear,mta::game_de::NativePedClothesAccess& clothes,const mta::game_de::NativePedReferenceToken& token')
    $text=$text.Replace(': CPedClearWeaponDE(',': CPedClothesDE(').Replace('onScreen,grant,clear,token,wrapperType','onScreen,grant,clear,clothes,token,wrapperType')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingPedOverrides=($base.rejectingPedOverrides-1);rejectingGameOverrides=$base.rejectingGameOverrides;
        testedMethod='SetClothesTextureAndModel';inheritedProductionMethods=@($base.inheritedProductionMethods)+@('SetClothesTextureAndModel');
        parentGenerator='Tests/definitive/Write-PedClearWeaponFixture.ps1';parentGeneratorSha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'Tests/definitive/Write-PedClearWeaponFixture.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        historicalTestCasesExecuted=$false;productionFactoriesPublished=$false}
}
