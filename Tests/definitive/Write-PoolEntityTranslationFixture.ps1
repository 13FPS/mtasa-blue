# Reuse the actual controlled CPools/CPed strict fixture without its old main.
function Write-PoolEntityTranslationFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1')
    $parentPath=Join-Path (Split-Path -Parent $OutputPath) 'PedPoolRegistryFixture.generated.h'
    $parent=Write-PedPoolRegistryFixture $RepositoryRoot $parentPath
    $text=[IO.File]::ReadAllText($parentPath).Replace('namespace pool_registry_fixture {','namespace pool_entity_fixture {')
    $text=$text.Replace('class Ped final :','class Ped :')
    $text=$text.Replace('class Pools final : public mta::game_de::CPoolsDE','class Pools final : public mta::game_de::CPoolsEntityDE')
    $text=$text.Replace(': CPoolsDE(owner){}',': CPoolsEntityDE(owner){}')
    $text=$text.Replace('using CPoolsDE::BindBorrowedPed;','using CPoolsDE::BindBorrowedPed;using CPoolsDE::ReadPedClientBinding;')
    $remove='(?m)^.*\b(?:GetEntity|GetClientEntity)\(DWORD\*[^\r\n]+\r?\n'
    if([regex]::Matches($text,$remove).Count -ne 2){throw 'The two existing pool translation rejectors are not unique.'}
    $text=[regex]::Replace($text,$remove,'')
    $pools=[regex]::Match($text,'(?s)class Pools final :.*?\r?\n};')
    $declarations=@($pools.Value -split '\r?\n'|Where-Object{$_ -match '\s+override\b' -and $_ -notmatch '~Pools'}|
        ForEach-Object{[regex]::Replace($_,'\s+override\b.*$',' override;')})
    if($declarations.Count -ne 25){throw 'Remaining pool factory/allocation surface changed.'}
    $probes=@('namespace pool_entity_fixture {','static_assert(!std::is_abstract_v<Pools>);')
    for($omit=0;$omit -lt $declarations.Count;++$omit) {
        $probes+='class Missing'+$omit+' : public mta::game_de::CPoolsEntityDE {public:'
        for($index=0;$index -lt $declarations.Count;++$index){if($index -ne $omit){$probes+=$declarations[$index]}}
        $probes+='};'
        $probes+='static_assert(std::is_abstract_v<Missing'+$omit+'>);'
    }
    $probes+='}'
    [IO.File]::WriteAllText($OutputPath,$text+"`n"+($probes -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedPoolSignatures=9;rejectingPoolSignatures=25;pureOmissionProbes=25;parent=$parent;historicalTestsExecuted=$false;productionFactoryPublished=$false}
}
