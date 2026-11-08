# Generate only three refusing methods from the actual object-property SDK.
# The 38 source-backed methods remain inherited from the production receiver.
function Write-ObjectPropertiesExtendedFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CObjectGroupPhysicalProperties.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CObjectGroupPhysicalProperties\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual object property declaration not found'}
    $reject=@('GetInterface','SetFxParticleSystem','RemoveFxParticleSystem')
    $methods=@();$provided=@()
    foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $signature=[regex]::Replace($signature,'\s+',' ')
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in $reject){$methods+=($signature+' override {std::abort();}')}
        else{$provided+=$signature}
    }
    if($provided.Count -ne 38 -or $methods.Count -ne 3){throw 'Actual extended object property partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ObjectPropertiesExtendedObserver final : public mta::game_de::CObjectGroupPhysicalPropertiesDE { public:',
        'ObjectPropertiesExtendedObserver(std::uint8_t group,mta::game_de::NativeObjectGroupPropertiesAccess& owner) : CObjectGroupPhysicalPropertiesDE(group,owner) {}')
    $lines+=$methods;$lines+=@('};','#pragma warning(pop)',
        'static_assert(!std::is_abstract_v<ObjectPropertiesExtendedObserver> && std::is_abstract_v<mta::game_de::CObjectGroupPhysicalPropertiesDE>);')
    [IO.File]::WriteAllText($OutputPath,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedProductionMethods=$provided;inheritedPropertyMethods=38;rejectingPropertyMethods=3;
        productionFactoryPublished=$false;historicalTestsExecuted=$false}
}
