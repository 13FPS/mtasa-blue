# Generate strict test-only receivers; no task factory is supplied.
function Write-PedIntelligenceFixture([string]$taskRoot,[string]$OutputPath) {
function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach ($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        # Defaults are caller syntax, not part of an overriding signature.
        $signature = [regex]::Replace($signature,'\s*=\s*[^,)]+','')
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}


    function RejectBody([string]$signature,[string]$label) {
        $begin=$signature.IndexOf('(');$end=$signature.LastIndexOf(')')
        $parameters=$signature.Substring($begin+1,$end-$begin-1)
        $parts=@();$part='';$depth=0
        foreach($character in $parameters.ToCharArray()) {
            if($character -eq ',' -and $depth -eq 0){$parts+=$part;$part='';continue}
            $part+=$character
            if($character -in @('<','(','[')){$depth++}
            elseif($character -in @('>',')',']')){$depth--}
        }
        $parts+=$part
        $discard=@(foreach($parameter in $parts) {
            $trimmed=$parameter.Trim()
            $match=[regex]::Match($trimmed,'(?<name>[A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$')
            if($match.Success) {
                $name=$match.Groups['name'].Value
                $prefix=$trimmed.Substring(0,$match.Index).Trim()
                if($prefix -and $prefix -notmatch '^(?:(?:const|volatile|struct|class|enum)\s*)+$' -and
                    $name -notin @('void','bool','char','short','int','long','float','double','unsigned','signed','DWORD','WORD','BYTE','uint32_t','int32_t','uint64_t','size_t')) {
                    '(void)'+$name+';'
                }
            } else {
                $functionPointer=[regex]::Match($trimmed,'\(\s*\*\s*(?<name>\w+)\s*\)')
                if($functionPointer.Success){'(void)'+$functionPointer.Groups['name'].Value+';'}
            }
        })
        ' override {'+($discard -join '')+'Unqualified("'+$label+'");}'
    }





    $intelligence=@(InterfaceMethods 'Client/sdk/game/CPedIntelligence.h'|Where-Object{$_.name -ne 'GetTaskManager'})
    $manager=@(InterfaceMethods 'Client/sdk/game/CTaskManager.h'|Where-Object{$_.signature -notmatch '^void\s+RemoveTaskSecondary\s*\('})
    if($intelligence.Count -ne 4 -or $manager.Count -ne 12){throw 'Unexpected remaining intelligence/task-manager surface'}
    $lines=@('#pragma once','#include <utility>','#include <type_traits>','#include <cstdio>','#include <cstdlib>',
        'namespace ped_intelligence_fixture {',
        '[[noreturn]] inline void Unqualified(const char* method) noexcept {std::fprintf(stderr,"Unqualified intelligence fixture call: %s\n",method);std::abort();}',
        'class ManagerBase : public mta::game_de::CTaskManagerDE { public:',
        'ManagerBase(mta::game_de::NativeTaskRemovalAccess& owner,const mta::game_de::NativePedReferenceToken& token) noexcept : CTaskManagerDE(owner,token) {}',
        'using CTaskManagerDE::RemoveTaskSecondary;')
    $lines+=@($manager|ForEach-Object{$_.signature+(RejectBody $_.signature ('TaskManager::'+$_.name))})
    $lines+='};'
    foreach($missing in -1..3) {
        $name=if($missing -lt 0){'Intelligence'}else{'MissingIntelligenceMethod'+$missing}
        $lines+=('class '+$name+' : public mta::game_de::CPedIntelligenceDE { public:')
        $lines+=($name+'(mta::game_de::NativeGameTimingAccess& owner,const mta::game_de::NativePedReferenceToken& token,mta::game_de::CTaskManagerDE& manager) : CPedIntelligenceDE(owner,token,manager) {}')
        for($index=0;$index -lt $intelligence.Count;$index++) {
            if($index -ne $missing){$lines+=($intelligence[$index].signature+(RejectBody $intelligence[$index].signature ('Intelligence::'+$intelligence[$index].name)))}
        }
        $lines+='};'
        if($missing -lt 0){$lines+='static_assert(!std::is_abstract_v<Intelligence>);'}else{$lines+=('static_assert(std::is_abstract_v<'+$name+'>);')}
    }
    $lines+='static_assert(!std::is_abstract_v<ManagerBase>);'
    $lines+='}'
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        intelligenceInherited=1;intelligenceRejectors=4;intelligenceOmissionProbes=4;managerInherited=1;managerRejectors=12;
        completeProductionIntelligence=$false;nativeTaskFactoryImplemented=$false}
}
