# Derive rejecting test-only methods from the actual SDK. No substitute SDK
# interface, block wrapper, native factory or refcount is supplied by this helper.
function Write-AnimationManagerFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/sdk/game/CAnimManager.h'))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive actual animation-manager fixture method from $signature"}
        if($name -notin @('GetNumAnimations','GetNumAnimBlocks','GetAnimationBlockIndex')) {
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [regex]::Replace($signature,'\s+',' ')
        }
    })
    if($methods.Count -lt 40){throw 'Actual animation-manager rejecting fixture extraction was incomplete.'}
    # These fixtures do not test cleanup. Keep both required operations as
    # explicit abortors so no fallback can falsely authorize owner teardown.
    foreach($required in @('BeginCustomAnimationCleanup','HasPendingCustomAnimationCleanup')) {
        if(@($methods|Where-Object {$_ -match ('\b'+$required+'\s*\(')}).Count -ne 1) {
            throw ('Missing required rejecting cleanup operation: '+$required)
        }
    }
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class AnimationManagerFixtureReceiver final : public mta::game_de::CAnimManagerDE { public:',
        'AnimationManagerFixtureReceiver(mta::game_de::NativeAnimationManagerAccess& access,std::uint64_t generation) noexcept : CAnimManagerDE(access,generation) {}')
    $lines+=@($methods | ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;source='Client/sdk/game/CAnimManager.h';
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'Client/sdk/game/CAnimManager.h') -Algorithm SHA256).Hash.ToLowerInvariant()}
}
