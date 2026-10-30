# Select actual production CPU-only definitions; never reproduce native ASM.
function Write-ClassicAssociationGroupCpuLifetimeFixture([string]$RepositoryRoot,[string]$OutputPath,[string]$SourcePath='') {
    if(-not $SourcePath){$SourcePath=Join-Path $RepositoryRoot 'Client/game_sa/CAnimBlendAssocGroupSA.cpp'}
    $text=[IO.File]::ReadAllText($SourcePath)
    $signatures=@('CAnimBlendAssocGroupSA::CAnimBlendAssocGroupSA(',
        'CAnimBlendAssocGroupSA::~CAnimBlendAssocGroupSA(',
        'CAnimBlock* CAnimBlendAssocGroupSA::GetAnimBlock(',
        'void CAnimBlendAssocGroupSA::SetupAnimBlock(')
    $spans=@();$parts=@('#pragma once','// Exact actual-source CPU lifecycle fragments; unexercised native routes reject.')
    foreach($signature in $signatures){
        $matches=[regex]::Matches($text,[regex]::Escape($signature))
        if($matches.Count -ne 1){throw ('Expected unique actual classic CPU definition: '+$signature)}
        $start=$matches[0].Index;$open=$text.IndexOf('{',$start)
        if($open -lt 0){throw 'Missing actual CPU body.'}
        $depth=0;$mode='code';$quote=[char]0;$end=-1
        for($at=$open;$at -lt $text.Length;$at++){
            $c=$text[$at];$next=if($at+1 -lt $text.Length){$text[$at+1]}else{[char]0}
            if($mode -eq 'line'){if($c -eq "`n"){$mode='code'};continue}
            if($mode -eq 'comment'){if($c -eq '*' -and $next -eq '/'){$mode='code';$at++};continue}
            if($mode -eq 'quote'){if($c -eq '\'){$at++;continue};if($c -eq $quote){$mode='code'};continue}
            if($c -eq '/' -and $next -eq '/'){$mode='line';$at++;continue}
            if($c -eq '/' -and $next -eq '*'){$mode='comment';$at++;continue}
            if($c -eq '"' -or $c -eq "'"){$mode='quote';$quote=$c;continue}
            if($c -eq '{'){$depth++}
            if($c -eq '}'){$depth--;if($depth -eq 0){$end=$at+1;break}}
        }
        if($end -lt 0){throw 'Unterminated actual classic CPU body.'}
        $body=$text.Substring($start,$end-$start)
        $bytes=[Text.Encoding]::UTF8.GetBytes($body)
        $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
        $spans+=@{signature=$signature;sha256=$hash;characters=$body.Length}
        $parts+=$body
    }
    [IO.File]::WriteAllLines($OutputPath,$parts,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        source=$SourcePath;sourceSha256=(Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash.ToLowerInvariant();
        actualCpuDefinitions=$spans;nativeAsmSelected=$false;wholeClassicTranslationUnitClaimed=$false}
}
