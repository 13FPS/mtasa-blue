# Reuse the actual strict shared CPed/player composition. Remove only its
# GetMoveAnim rejector so this leaf exercises the real new inherited getter.
function Write-PlayerMoveAnimFixture([string]$taskRoot,[string]$OutputPath,[string]$MoveHeaderPath='') {
    if(-not $MoveHeaderPath){$MoveHeaderPath=Join-Path $taskRoot 'Client/game_de/CPlayerPedMoveAnimDE.h'}
    $header=[IO.File]::ReadAllText($MoveHeaderPath)
    if($header -notmatch 'class\s+CPlayerPedMoveAnimDE\s*:\s*public\s+CPlayerPedWantedDE' -or
       $header -notmatch 'eMoveAnim\s+GetMoveAnim\s*\(\)\s*override;'){throw 'Actual player movement getter/ancestry is missing.'}
    . (Join-Path $taskRoot 'Tests/definitive/Write-PedClearWeaponFixture.ps1')
    . (Join-Path $taskRoot 'Tests/definitive/Write-PedRebuildFixture.ps1')
    $directory=Split-Path -Parent $OutputPath
    $clear=Write-PedClearWeaponFixture $taskRoot (Join-Path $directory 'PedClearWeaponFixture.generated.h')
    $rebuildPath=Join-Path $directory 'PedRebuildFixture.generated.h'
    $rebuild=Write-PedRebuildFixture $taskRoot $rebuildPath
    $rebuildText=[IO.File]::ReadAllText($rebuildPath)
    $sealed='class PedRebuildFixture final : public mta::game_de::CPedRebuildDE'
    if([regex]::Matches($rebuildText,[regex]::Escape($sealed)).Count -ne 1){throw 'Actual strict rebuild leaf changed.'}
    [IO.File]::WriteAllText($rebuildPath,$rebuildText.Replace($sealed,'class PedRebuildFixture : public mta::game_de::CPedRebuildDE'),[Text.UTF8Encoding]::new($false))
    # The returned receipt describes the actual extendable generated leaf,
    # not the sealed intermediate emitted by its unchanged parent generator.
    $rebuild.sha256=(Get-FileHash -LiteralPath $rebuildPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $source=[IO.File]::ReadAllText((Join-Path $taskRoot 'Tests/definitive/PedPlayerSharedBaseDETests.cpp'))
    $classes=[regex]::Matches($source,'(?s)    class Player final : public ped_rebuild_fixture::PedRebuildFixture, public mta::game_de::CPlayerPedWantedDE\r?\n.*?\r?\n    };\r?\n')
    if($classes.Count -ne 1){throw 'Actual strict shared player composition is not unique.'}
    $text=$classes[0].Value
    $rejector='(?m)^.*\beMoveAnim GetMoveAnim\(\) override[^\r\n]+\r?\n'
    if([regex]::Matches($text,$rejector).Count -ne 1){throw 'Existing strict GetMoveAnim rejector is not unique.'}
    $text=[regex]::Replace($text,$rejector,'')
    $text=$text.Replace('class Player final','class PlayerMoveAnimFixture final').Replace('        Player(','        PlayerMoveAnimFixture(')
    $text=$text.Replace('public mta::game_de::CPlayerPedWantedDE','public mta::game_de::CPlayerPedMoveAnimDE')
    $old='mta::game_de::NativePedRebuildAccess& rebuild, const mta::game_de::NativePedReferenceToken& token'
    if(-not $text.Contains($old)){throw 'Actual strict player constructor changed.'}
    $text=$text.Replace($old,'mta::game_de::NativePedRebuildAccess& rebuild, mta::game_de::NativePedMoveAnimAccess& move, const mta::game_de::NativePedReferenceToken& token')
    $text=$text.Replace('CPlayerPedWantedDE(wanted)','CPlayerPedMoveAnimDE(wanted, move, token)')
    # This test-only CPU setter distinguishes actual group observation from
    # MTA's independent custom-override bookkeeping, without native writes.
    $text=$text.Replace('    public:',"    public:`n        void FixtureCustomMoveAnim(int value) noexcept { m_iCustomMoveAnim = value; }")
    $prefix=@'
#pragma once
namespace player_move_anim_fixture
{
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4250)
#endif
'@
    $suffix=@'
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
    static_assert(!std::is_abstract_v<PlayerMoveAnimFixture>);
    static_assert(std::is_convertible_v<PlayerMoveAnimFixture*, CPed*>);
    static_assert(std::is_convertible_v<PlayerMoveAnimFixture*, CPlayerPed*>);
}
'@
    [IO.File]::WriteAllText($OutputPath,$prefix+"`n"+$text+$suffix,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        testedMethod='CPlayerPed::GetMoveAnim';rejectingPlayerOverrides=4;rejectingPedOverrides=$rebuild.rejectingPedOverrides;
        rejectingGameOverrides=$rebuild.rejectingGameOverrides;parentFixture=$rebuild;
        strictPlayerSource='Tests/definitive/PedPlayerSharedBaseDETests.cpp';
        strictPlayerSourceSha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'Tests/definitive/PedPlayerSharedBaseDETests.cpp') -Algorithm SHA256).Hash.ToLowerInvariant();
        historicalTestCasesExecuted=$false;productionFactoryPublished=$false}
}
