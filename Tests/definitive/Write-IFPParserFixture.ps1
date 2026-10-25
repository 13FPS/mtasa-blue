# Include actual class headers unchanged; alias only unrelated client shells.
# The actual hash bodies are selected from the shared source for this runtime.
function Write-IFPParserFixture([string]$Root,[string]$Directory) {
    [IO.Directory]::CreateDirectory($Directory)|Out-Null
    $records=@()
    # Only the unrelated base entity/string environment is substituted. The
    # actual IFP/animation/file-reader class headers are included unchanged.
    foreach($relative in @('Client/mods/deathmatch/logic/CIFPAnimations.h','Client/mods/deathmatch/logic/CClientIFP.h','Client/mods/deathmatch/logic/CFileReader.h')) {
        $records+=@([ordered]@{path=$relative;sha256=(Get-FileHash -LiteralPath (Join-Path $Root $relative) -Algorithm SHA256).Hash.ToLowerInvariant()})
    }
    $manager=[IO.File]::ReadAllText((Join-Path $Root 'Client/sdk/game/CAnimManager.h'))
    $manager=[regex]::Replace($manager,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $allowed=@('GetCustomAnimBlendHierarchy','GetCustomAnimBlendSequence','GetCustomAnimBlendSequenceAt','DeleteCustomAnimHierarchyInterface',
        'DeleteCustomAnimSequenceInterface','AllocateKeyFramesMemory','FreeKeyFramesMemory','AllocateCustomAnimSequences',
        'FreeCustomAnimSequences','RemoveFromUncompressedCache')
    $rejectors=@('#pragma warning(push)','#pragma warning(disable : 4100)')
    foreach($match in [regex]::Matches($manager,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive rejecting manager method: $signature"}
        if($name -in $allowed){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
        $rejectors+=@(([regex]::Replace($signature,'\s+',' '))+' override { Unexpected(); }')
    }
    $rejectors+=@('#pragma warning(pop)')
    [IO.File]::WriteAllLines((Join-Path $Directory 'IFPManagerRejectors.generated.inl'),$rejectors,[Text.UTF8Encoding]::new($false))
    $records+=@([ordered]@{path='Client/sdk/game/CAnimManager.h';sha256=(Get-FileHash -LiteralPath (Join-Path $Root 'Client/sdk/game/CAnimManager.h') -Algorithm SHA256).Hash.ToLowerInvariant()})

    # This is the actual unchanged shared hash algorithm, not an alternative
    # hash in the parser fixture. Keep exact signatures and balanced bodies.
    $hashPath=Join-Path $Root 'Shared/sdk/SharedUtil.Hash.hpp'
    $hashText=[IO.File]::ReadAllText($hashPath)
    $hash=@('#pragma once','namespace SharedUtil {','unsigned int HashString(const char*, unsigned int);')
    foreach($signature in @('unsigned int HashString(const char* szString)','unsigned int HashString(const char* szString, unsigned int length)')) {
        $start=$hashText.IndexOf($signature,[StringComparison]::Ordinal)
        if($start -lt 0){throw "Actual hash signature missing: $signature"}
        $brace=$hashText.IndexOf('{',$start)
        $depth=0;$end=-1
        for($index=$brace;$index -lt $hashText.Length;$index++) {
            if($hashText[$index] -eq '{'){$depth++}
            elseif($hashText[$index] -eq '}'){$depth--;if(-not $depth){$end=$index;break}}
        }
        if($end -lt 0){throw 'Actual shared hash body is incomplete'}
        $hash+=@('inline '+$hashText.Substring($start,$end-$start+1))
    }
    $hash+=@('}','using SharedUtil::HashString;')
    [IO.File]::WriteAllLines((Join-Path $Directory 'IFPHash.generated.h'),$hash,[Text.UTF8Encoding]::new($false))
    $records+=@([ordered]@{path='Shared/sdk/SharedUtil.Hash.hpp';sha256=(Get-FileHash -LiteralPath $hashPath -Algorithm SHA256).Hash.ToLowerInvariant()})
    [IO.File]::WriteAllLines((Join-Path $Directory 'StdInc.h'),@('#pragma once',
        '#include "IFPParserEnvironment.h"','#include "IFPHash.generated.h"',
        '#pragma include_alias("CClientEntity.h", "IFPParserEnvironment.h")',
        '#pragma include_alias(<SString.h>, <IFPParserEnvironment.h>)',
        '#pragma warning(push)','#pragma warning(disable : 4100)',
        '#include "CIFPAnimations.h"','#include "CClientIFP.h"','#pragma warning(pop)'),[Text.UTF8Encoding]::new($false))
    [ordered]@{sourceInputs=$records;includeAliases=@('CClientEntity.h->IFPParserEnvironment.h','<SString.h>-><IFPParserEnvironment.h>');
        actualCommonHeadersIncluded=$true;unrelatedClientShellOnly=$true;classDeclarationsMirrored=$false;parserBodiesMirrored=$false;
        files=@('StdInc.h','IFPHash.generated.h','IFPManagerRejectors.generated.inl'|ForEach-Object{
            $path=Join-Path $Directory $_;[ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        })}
}
