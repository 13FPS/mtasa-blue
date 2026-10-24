# Reuse the existing strict block leaf; remove only the two rejectors now
# supplied by the actual inherited native reference methods. Request and raw
# interface/hierarchy access continue to abort, so no fake factory is supplied.
function Write-AnimBlockRefsFixture([string]$taskRoot,[string]$OutputPath) {
    $source=[IO.File]::ReadAllText((Join-Path $taskRoot 'Tests/definitive/AnimBlockNameDETests.cpp'))
    $matches=[regex]::Matches($source,'(?s)    class Receiver final : public CAnimBlockDE\r?\n.*?\r?\n    };\r?\n')
    if($matches.Count -ne 1){throw 'Existing strict animation block receiver is not unique.'}
    $text=$matches[0].Value
    foreach($pattern in @('(?m)^.*\bvoid\s+AddRef\(\) override[^\r\n]+\r?\n','(?m)^.*\bunsigned short\s+GetRefs\(\) override[^\r\n]+\r?\n')) {
        if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Expected strict reference rejector changed.'}
        $text=[regex]::Replace($text,$pattern,'')
    }
    $text=$text.Replace('class Receiver final','class RefsReceiver final').Replace('        Receiver(','        RefsReceiver(')
    [IO.File]::WriteAllText($OutputPath,"#pragma once`nnamespace animation_block_refs_fixture`n{`n"+$text+"}`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingOverrides=3;inheritedMethodsUnderTest=@('AddRef','GetRefs');
        source='Tests/definitive/AnimBlockNameDETests.cpp';
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'Tests/definitive/AnimBlockNameDETests.cpp') -Algorithm SHA256).Hash.ToLowerInvariant();
        historicalTestCasesExecuted=$false;completeGameSupplied=$false;productionFactoryPublished=$false}
}
