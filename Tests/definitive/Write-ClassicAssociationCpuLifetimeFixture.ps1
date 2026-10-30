# Select exact actual CPU-only constructor/destructor/GetInterface spans. The
# native SA layout and unrelated operation bodies are not duplicated here.
function Write-ClassicAssociationCpuLifetimeFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/game_sa/CAnimBlendAssociationSA.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $ctor=[regex]::Match($text,'(?m)^\s*CAnimBlendAssociationSA\(CAnimBlendAssociationSAInterface\* pInterface\)\s*\{[^\r\n]+\}').Value.Trim()
    $dtor=[regex]::Match($text,'~CAnimBlendAssociationSA\(\) override = default;').Value
    $get=[regex]::Match($text,'CAnimBlendAssociationSAInterface\*\s+GetInterface\(\)\s*\{[^\r\n]+\}').Value
    $member=[regex]::Match($text,'CAnimBlendAssociationSAInterface\* m_pInterface;').Value
    if(-not $ctor -or -not $dtor -or -not $get -or -not $member){throw 'Actual classic CPU lifetime spans changed'}
    $sdk=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/sdk/game/CAnimBlendAssociation.h'))
    $sdk=[regex]::Replace($sdk,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $methods=@(foreach($match in [regex]::Matches($sdk,'virtual\s+(?<signature>[^;{}]+);')){
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        if($signature -match '^~CAnimBlendAssociation\s*\(' -or $signature -match '\bGetInterface\s*\('){continue}
        # Only unrelated test rejectors discard these real SDK arguments.
        # Name each discarded argument instead of weakening /W4 /WX globally.
        $parameters=[regex]::Match($signature,'\((?<args>[^()]*)\)').Groups['args'].Value
        $unused=@(foreach($parameter in $parameters.Split(',')){
            if([string]::IsNullOrWhiteSpace($parameter)){continue}
            $name=[regex]::Match($parameter.Trim(),'(?<name>[A-Za-z_]\w*)$').Groups['name'].Value
            if(-not $name){throw 'Actual SDK rejector parameter name extraction changed'}
            '(void)'+$name+';'
        })
        [regex]::Replace($signature,'\s+',' ')+' override {'+($unused -join '')+'std::abort();}'
    })
    if($methods.Count -ne 21){throw 'Actual classic CPU fixture must reject21 unrelated operations'}
    $lines=@('#pragma once','class ClassicAssociationCpuLifetimeFixture : public CAnimBlendAssociation { public:',
        $ctor.Replace('CAnimBlendAssociationSA(','ClassicAssociationCpuLifetimeFixture('),
        $dtor.Replace('~CAnimBlendAssociationSA(','~ClassicAssociationCpuLifetimeFixture('),$get)+$methods+@('protected:',$member,'};')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=21;selectedActualSpans=@($ctor,$dtor,$get,$member);nativeLayoutRecreated=$false;nativeEngineCodeExecuted=$false}
}
