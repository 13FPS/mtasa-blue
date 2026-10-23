# Build only rejecting test implementations of the actual SDK. A derivable
# test ped allows constructor/destructor fault hooks; production supplies none.
function Write-PedBorrowedFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1')
    $record=Write-PedPoolRegistryFixture $RepositoryRoot $OutputPath
    $text=[IO.File]::ReadAllText($OutputPath)
    $anchor='class Ped final : public Anchor, public mta::game_de::CPedVitalsDE'
    if(([regex]::Matches($text,[regex]::Escape($anchor))).Count -ne 1){throw 'Expected exactly one rejecting fixture ped declaration.'}
    $text=$text.Replace($anchor,'class Ped : public Anchor, public mta::game_de::CPedVitalsDE')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    $record['sha256']=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $record['derivableRejectingPed']=$true
    $record
}
