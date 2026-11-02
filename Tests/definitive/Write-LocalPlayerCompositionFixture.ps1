# Reuse actual strict SDK definitions; generated concrete leaves remain test-only.
function Write-LocalPlayerCompositionFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PedRebuildFixture.ps1')
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1')
    $directory=Split-Path -Parent $OutputPath
    $rebuildPath=Join-Path $directory 'PedRebuildFixture.generated.h'
    $parent=Write-PedRebuildFixture $RepositoryRoot $rebuildPath
    $text=[IO.File]::ReadAllText($rebuildPath)
    $begin=$text.IndexOf('class PedRebuildFixture final :')
    if($begin -lt 0){throw 'Strict rebuild class is missing.'}
    $prefix=$text.Substring(0,$begin).Replace('namespace ped_rebuild_fixture {',"namespace local_player_composition_fixture {`n[[noreturn]] inline void Unqualified(const char*) noexcept {std::abort();}")
    $class=$text.Substring($begin)
    $lines=@($class -split '\r?\n' | Where-Object {$_ -match '\s+override\b'})
    $pedRejectors=@($lines | Where-Object {$_ -notmatch '\b(?:GetInterface|GetRpClump)\s*\('})
    if($lines.Count -ne 101 -or $pedRejectors.Count -ne 99){throw 'Strict 99 SDK + two backend rebuild surface changed.'}
    $player=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/sdk/game/CPlayerPed.h'))
    $playerRejectors=@(foreach($match in [regex]::Matches($player,'virtual\s+(?<signature>[^;{}]+?)\s*=\s*0\s*;')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s+',' ').Trim()
        if($signature -notmatch '\b(?:GetWanted|GetMoveAnim)\s*\('){$signature+' override {Unqualified("Player");}'}
    })
    if($playerRejectors.Count -ne 4){throw 'Strict remaining player surface changed.'}
    $rejectors=@($pedRejectors)+@($playerRejectors)
    $declarations=@($rejectors | ForEach-Object {([regex]::Replace($_,'\s+override\b.*$',' override;')).Replace('protected:','').Trim()})
    if($declarations.Count -ne 103){throw 'Exact 101 SDK plus two backend hooks is required.'}
    $body=@($prefix,
        'class Player final : public mta::game_de::CPlayerPedCompositionDE { public:',
        'Player(mta::game_de::NativeGameTimingAccess& owner,const mta::game_de::NativePedReferenceToken& token,CGame& game,mta::game_de::CPoolsDE& pools,CWanted* wanted,std::uint32_t wrapperType,unsigned* destroyed=nullptr) : CPlayerPedCompositionDE(owner,token,game,pools,wanted,wrapperType),destructionCount(destroyed) {}',
        '~Player() override {if(destructionCount)++*destructionCount;}',
        'const mta::game_de::NativePedReferenceToken* FixtureTokenAddress() const noexcept {return &ReferenceToken();}',
        'void FixtureCustomMoveAnim(int value) noexcept {m_iCustomMoveAnim=value;}')+@($rejectors)+@(
        'private:unsigned* destructionCount;','};',
        'static_assert(!std::is_abstract_v<Player>);',
        'static_assert(std::is_convertible_v<Player*,CPed*> && std::is_convertible_v<Player*,CPlayerPed*>);',
        'static_assert(std::is_convertible_v<Player*,mta::game_de::CPedVitalsDE*>);')
    # Each probe omits one actual remaining override while declaring all peers.
    # No probe is instantiated or linked; this proves every missing contract
    # remains mandatory without replaying any inherited-method test matrix.
    for($omit=0;$omit -lt $declarations.Count;++$omit) {
        $body+='class Missing'+$omit+' : public mta::game_de::CPlayerPedCompositionDE {public:'
        for($index=0;$index -lt $declarations.Count;++$index){if($index -ne $omit){$body+=$declarations[$index]}}
        $body+='};'
        $body+='static_assert(std::is_abstract_v<Missing'+$omit+'>);'
    }
    $body+='}'
    $poolPath=Join-Path $directory 'PedPoolRegistryFixture.generated.h'
    $poolReceipt=Write-PedPoolRegistryFixture $RepositoryRoot $poolPath
    $pools=[regex]::Match([IO.File]::ReadAllText($poolPath),'(?s)class Pools final :.*?\r?\n};')
    if(-not $pools.Success){throw 'Strict controlled pool definition is missing.'}
    # The reused pool leaf previously owned a zero-argument abort helper.
    # Its rejectors now share this fixture's descriptive-reason helper.
    $poolText=$pools.Value.Replace('Unqualified();','Unqualified("Pools::unsupported");')
    $body+=@('namespace local_player_composition_fixture {',$poolText,'}')
    $body=@('#pragma once','#if defined(_MSC_VER)','#pragma warning(push)','#pragma warning(disable : 4100 4250)','#endif')+$body+@('#if defined(_MSC_VER)','#pragma warning(pop)','#endif')
    [IO.File]::WriteAllLines($OutputPath,$body,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedSdkSignatures=69;rejectingPedSdkSignatures=97;rejectingPlayerSdkSignatures=4;rejectingBackendHooks=2;
        pureOmissionProbes=103;historicalTestsExecuted=$false;productionFactoryPublished=$false;
        parentRebuild=$parent;parentPools=$poolReceipt}
}
