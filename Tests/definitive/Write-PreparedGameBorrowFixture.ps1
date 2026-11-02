# Reuse the actual lifecycle omission probes; this receiver exists only in tests.
function Write-PreparedGameBorrowFixture([string]$taskRoot,[string]$OutputPath) {
    . (Join-Path $taskRoot 'Tests/definitive/Write-StartupLifecycleFixture.ps1')
    $base=Write-StartupLifecycleFixture $taskRoot $OutputPath -BuildingDefault
    $hooks=@($base.pureNativeHooks|Where-Object{$_.name -ne 'GetResetBuildingPoolCapacity'})
    if($hooks.Count -ne 13){throw 'Prepared startup graph must retain13 lifecycle hooks'}
    $lines=@('namespace prepared_game_fixture {',
        'class Game final : public lifecycle_fixture::NoPresentationGameBase<mta::game_de::CGameBuildingPoolDefaultDE> {',
        'using Base=lifecycle_fixture::NoPresentationGameBase<mta::game_de::CGameBuildingPoolDefaultDE>;',
        'public: template<class... Args> explicit Game(Args&&... args) : Base(std::forward<Args>(args)...) {}',
        'protected:')
    foreach($hook in $hooks) {
        # Thirteen explicit refusing test guards make constructor-only access
        # possible. The real default-capacity implementation stays inherited.
        $discard=if($hook.name -eq 'EnableWorldIplStreaming'){'(void)filter;'}else{''}
        $lines+=($hook.signature+' override {'+$discard+'lifecycle_fixture::Unqualified("prepared graph '+$hook.name+'");}')
    }
    $lines+=@('};','static_assert(!std::is_abstract_v<Game> && std::is_convertible_v<Game*,CGame*>);','}')
    $lines|Add-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        namespace='prepared_game_fixture';baseHookProbes=$base.omittedHookProbes;derivedHookProbes=$base.defaultOmittedHookProbes;
        unrelatedSdkRejectors=$base.rejectingOverrides.game;testOnlyLifecycleGuards=13;productionCapacityHookInherited=$true;completeProductionGame=$false}
}
