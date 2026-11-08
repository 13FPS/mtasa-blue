# Keep all unqualified CPools operations as rejecting fixture methods. The
# generated shell exists solely to exercise the real inherited usage methods.
function Write-PoolUsageFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1')
    $parentPath=Join-Path (Split-Path -Parent $OutputPath) 'PedPoolRegistryFixture.generated.h'
    $parent=Write-PedPoolRegistryFixture $RepositoryRoot $parentPath
    $text=[IO.File]::ReadAllText($parentPath).Replace('namespace pool_registry_fixture {','namespace pool_usage_fixture {')
    $text=$text.Replace('class Pools final : public mta::game_de::CPoolsDE','class Pools final : public mta::game_de::CPoolsUsageDE')
    $text=$text.Replace(': CPoolsDE(owner){}',': CPoolsUsageDE(owner){}')
    $remove='(?m)^.*\b(?:GetEntity|GetClientEntity|GetPoolCapacity|GetNumberOfUsedSpaces)\([^\r\n]+\r?\n'
    if([regex]::Matches($text,$remove).Count -ne 4){throw 'The four newly inherited pool SDK methods are not unique.'}
    $text=[regex]::Replace($text,$remove,'')
    $pools=[regex]::Match($text,'(?s)class Pools final :.*?\r?\n};')
    $declarations=@($pools.Value -split '\r?\n'|Where-Object{$_ -match '\s+override\b' -and $_ -notmatch '~Pools'}|
        ForEach-Object{[regex]::Replace($_,'\s+override\b.*$',' override;')})
    if($declarations.Count -ne 23){throw 'Remaining pool factory/allocation surface changed.'}
    $probes=@('namespace pool_usage_fixture {','static_assert(!std::is_abstract_v<Pools>);')
    for($omit=0;$omit -lt $declarations.Count;++$omit) {
        $probes+='class Missing'+$omit+' : public mta::game_de::CPoolsUsageDE {public:'
        for($index=0;$index -lt $declarations.Count;++$index){if($index -ne $omit){$probes+=$declarations[$index]}}
        $probes+='};'
        $probes+='static_assert(std::is_abstract_v<Missing'+$omit+'>);'
    }
    $probes+='}'
    [IO.File]::WriteAllText($OutputPath,$text+"`n"+($probes -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedPoolSignatures=11;rejectingPoolSignatures=23;pureOmissionProbes=23;parent=$parent;historicalTestsExecuted=$false;productionFactoryPublished=$false}
}
