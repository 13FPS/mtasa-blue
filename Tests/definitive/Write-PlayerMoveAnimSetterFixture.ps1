# Reuse the strict existing player and real animation block factory receivers.
# Only SetMoveAnim's rejector is removed; no remaining player method is faked.
function Write-PlayerMoveAnimSetterFixture([string]$RepositoryRoot,[string]$OutputPath) {
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-PlayerMoveAnimFixture.ps1')
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-AnimationBlockFactoryFixture.ps1')
    $directory=Split-Path -Parent $OutputPath
    $playerPath=Join-Path $directory 'PlayerMoveAnimFixture.generated.h'
    $player=Write-PlayerMoveAnimFixture $RepositoryRoot $playerPath
    $text=[IO.File]::ReadAllText($playerPath)
    $rejector='(?ms)^\s*void\s+SetMoveAnim\(eMoveAnim value\) override\s*\{\s*\(void\)value;\s*ped_clear_weapon_fixture::Unqualified\("Player::SetMoveAnim"\);\s*\}'
    if([regex]::Matches($text,$rejector).Count -ne 1){throw 'Actual strict setter rejector changed.'}
    $text=[regex]::Replace($text,$rejector,'')
    $needle='void FixtureCustomMoveAnim(int value) noexcept { m_iCustomMoveAnim = value; }'
    if(-not $text.Contains($needle)){throw 'Strict actual state probe changed.'}
    $text=$text.Replace($needle,$needle+@'

        void FixtureConfigure(mta::game_de::NativeGameTimingAccess& owner,mta::game_de::CAnimManagerDE& manager) {
            ConfigureMoveAnim(owner,manager,static_cast<mta::game_de::CPedStateDE&>(*this));
        }
        void FixtureConfigureWithState(mta::game_de::NativeGameTimingAccess& owner,mta::game_de::CAnimManagerDE& manager,mta::game_de::CPedStateDE& state) {
            ConfigureMoveAnim(owner,manager,state);
        }
'@)
    [IO.File]::WriteAllText($playerPath,$text,[Text.UTF8Encoding]::new($false))
    $manager=Write-AnimationBlockFactoryFixture $RepositoryRoot (Join-Path $directory 'AnimationBlockFactoryFixture.generated.h')
    function Methods([string]$RelativePath) {
        $source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $RelativePath))
        $source=[regex]::Replace($source,'(?s)/\*.*?\*/|//[^\r\n]*','')
        foreach($match in [regex]::Matches($source,'virtual\s+(?<signature>[^;{}]+);')) {
            $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
            $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
            if(-not $name){throw "Cannot derive rejecting method: $signature"}
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
        }
    }
    $game=@(Methods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetStreaming','GetModelInfo','GetBaseIDforIFP')})
    $streamHeader=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/game_de/CStreamingDE.h'))
    $provided=@([regex]::Matches($streamHeader,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?override\s*;')|ForEach-Object{$_.Groups['name'].Value})
    $stream=@(Methods 'Client/sdk/game/CStreaming.h'|Where-Object{$_.name -notin $provided})
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace setter_fixture {',
        'class GameBase : public CGame { public:')
    $lines+=@($game|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class Streaming final : public mta::game_de::CStreamingDE { public:',
        'explicit Streaming(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
    $lines+=@($stream|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','}','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        playerFixturePath=$playerPath;playerFixtureSha256=(Get-FileHash -LiteralPath $playerPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedPlayerMethods=@('GetMoveAnim','SetMoveAnim');rejectingPlayerOverrides=3;parentPlayer=$player;manager=$manager;
        rejectingGameOverrides=$game.Count;rejectingStreamingOverrides=$stream.Count;actualStreamingMethods=$provided;
        historicalTestCasesExecuted=$false;productionFactoryPublished=$false}
}
