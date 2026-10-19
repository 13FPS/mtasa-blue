#requires -Version 7.5
# Passive validation of the statically identified DE timing fields. This never
# launches, injects, writes, suspends, terminates or changes focus in GTA.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkDirectory,
    [ValidateRange(10,15)][int]$DurationSeconds=12,
    [string]$NativeFPSProofPath
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if([IntPtr]::Size -ne 8){throw 'Use native x64 PowerShell for this read-only capture.'}
$repository=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$work=(Resolve-Path -LiteralPath $WorkDirectory).Path
$allowed=Join-Path $repository 'Build/definitive-recovery'
if(-not $work.StartsWith($allowed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Work directory must be inside workspace Build/definitive-recovery.'}
$output=Join-Path $work ('live-game-timing-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($output)|Out-Null
$scratch=Join-Path $repository ('Build/tmp/passive-game-timing-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
$env:TEMP=$scratch;$env:TMP=$scratch
$expectedPath='C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64\SanAndreas.exe'
$expectedSha='63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$evidenceSha='8f479f0e21dab768f03dd6d56bc9c115e0d3afb45b851c7e71f5259356d6ceb7'
$fields=@(
    @{name='frameCounter';rva=0x523a39c;bytes=4;type='u32'},@{name='nativeTimer';rva=0x523a458;bytes=4;type='u32'},
    @{name='timeStep';rva=0x523a2d8;bytes=4;type='f32'},@{name='oldTimeStep';rva=0x523a5d8;bytes=4;type='f32'},
    @{name='timeScale';rva=0x523a2dc;bytes=4;type='f32'},@{name='hour';rva=0x521c917;bytes=1;type='u8'},
    @{name='minute';rva=0x521c923;bytes=1;type='u8'},@{name='secondsRaw';rva=0x522bcf0;bytes=2;type='u16'},
    @{name='lastMinuteChange';rva=0x522bcf8;bytes=4;type='u32'},@{name='millisecondsPerGameMinute';rva=0x5227bac;bytes=4;type='u32'})
$spans=@(@{name='timer-init';rva=0x1186860;bytes=0xbd},@{name='timer-update-suffix';rva=0x1186b89;bytes=46},
    @{name='elapsed-scale';rva=0x11869c7;bytes=16},@{name='frame-getter';rva=0x12a2d00;bytes=7},
    @{name='previous-step-consumer';rva=0x10d7c2a;bytes=19},@{name='script-scale-store';rva=0x10d6603;bytes=36},
    @{name='native-clock';rva=0x1147bec;bytes=290},@{name='clock-getter';rva=0x10b8fe1;bytes=36})
$processHandle=[IntPtr]::Zero;$writer=$null;$failure=$null;$identity=$null;$pins=@();$accepted=0;$attempts=0;$misses=0
$first=$null;$last=$null;$previous=$null;$frameProgress=[uint64]0;$timerProgress=[uint64]0;$outsideUpdateBounds=0;$nonfinite=0;$clockOutOfRange=0;$backward=0
$floatRanges=@{};$scratchRemoved=$false;$diskAfter=$null
$fpsProofSha=$null;$fpsSourceUpdates=0;$fpsReciprocalMismatches=0;$scratchFilesRemaining=0
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Hex([byte[]]$Bytes){[Convert]::ToHexString($Bytes).ToLowerInvariant()}
function Event($Value){$writer.WriteLine(($Value|ConvertTo-Json -Depth 8 -Compress));$writer.Flush()}
try {
    $writer=[IO.StreamWriter]::new((Join-Path $output 'raw-samples.jsonl'),$false,[Text.UTF8Encoding]::new($false))
    if((Sha $expectedPath) -cne $expectedSha){throw 'Installed GTA executable SHA256 differs from the known image.'}
    $pin=Get-Content -LiteralPath (Join-Path $work 'timer-static-evidence-pin.json') -Raw|ConvertFrom-Json
    if($pin.sha256 -cne $evidenceSha -or (Sha $pin.path) -cne $evidenceSha){throw 'Static timer evidence pin differs.'}
    $evidence=Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json
    if($evidence.imageSha256 -cne $expectedSha -or @($evidence.checks).Count -ne 23 -or @($evidence.checks|Where-Object{$_.passed -ne $true}).Count){throw 'Static timer dataflow evidence is incomplete.'}
    Copy-Item -LiteralPath $pin.path -Destination (Join-Path $output 'timer-static-evidence.json')
    $disk=[IO.File]::ReadAllBytes($expectedPath)
    $pe=[BitConverter]::ToInt32($disk,0x3c);$optional=$pe+24
    if([BitConverter]::ToUInt16($disk,0) -ne 0x5a4d -or [BitConverter]::ToUInt32($disk,$pe) -ne 0x4550 -or
       [BitConverter]::ToUInt16($disk,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($disk,$optional) -ne 0x20b -or
       [BitConverter]::ToUInt32($disk,$optional+56) -ne 97141760){throw 'Owned executable PE architecture or image size differs.'}
    $sectionStart=$optional+[BitConverter]::ToUInt16($disk,$pe+20)
    $sections=@(for($i=0;$i -lt [BitConverter]::ToUInt16($disk,$pe+6);$i++){
        $o=$sectionStart+40*$i
        [pscustomobject]@{rva=[BitConverter]::ToUInt32($disk,$o+12);raw=[BitConverter]::ToUInt32($disk,$o+20);bytes=[BitConverter]::ToUInt32($disk,$o+16);flags=[BitConverter]::ToUInt32($disk,$o+36)}
    })
    if($NativeFPSProofPath){
        # Extend the same passive reader with separately proved native FPS.
        # Its platform clock may keep advancing while gameplay time is paused;
        # those are distinct observations, not permission to resume the game.
        $proof=Get-Content -LiteralPath $NativeFPSProofPath -Raw|ConvertFrom-Json -AsHashtable
        if($proof.schema -cne 'mta_de_actual_native_fps_static_v1' -or -not $proof.success -or $proof.checks -lt 54 -or
           $proof.image.sha256 -cne $expectedSha -or $proof.native_fps.rva -cne '0x57d7f64' -or
           @($proof.assertions|Where-Object{-not $_.passed}).Count){throw 'Native FPS dataflow proof differs.'}
        $fpsProofSha=Sha $NativeFPSProofPath
        Copy-Item -LiteralPath $NativeFPSProofPath -Destination (Join-Path $output 'fps-static-evidence.json')
        foreach($assertion in $proof.assertions){
            if(-not $assertion.ContainsKey('bytes')){continue}
            $code=$assertion.ContainsKey('instruction_rva')
            $rva=[Convert]::ToUInt32($(if($code){$assertion.instruction_rva}else{$assertion.rva}).Substring(2),16)
            $bytes=[Convert]::FromHexString($assertion.bytes)
            if($bytes.Length -lt 1 -or $bytes.Length -gt 1024){throw 'Native FPS proof span exceeds passive limits.'}
            $spans+=@(@{name=('fps-'+$assertion.name);rva=$rva;bytes=$bytes.Length;code=$code;proofHex=$assertion.bytes})
        }
        $label=[Text.Encoding]::Unicode.GetBytes("%5.2f FPS$([char]0)")
        $spans+=@(@{name='fps-native-display-label';rva=0x49b4870;bytes=$label.Length;code=$false;proofHex=(Hex $label)})
        $fields+=@(@{name='framesPerSecond';rva=0x57d7f64;bytes=4;type='f32'},
            @{name='averageFrameMilliseconds';rva=0x57d7f60;bytes=4;type='f32'},
            @{name='nativePlatformSeconds';rva=0x5725570;bytes=8;type='f64'})
    }
    function DiskBytes([uint32]$Rva,[int]$Count,[bool]$Code=$true){
        $section=@($sections|Where-Object{$Rva -ge $_.rva -and [uint64]$Rva+$Count -le [uint64]$_.rva+$_.bytes})
        if($section.Count -ne 1 -or $Code -ne [bool]($section[0].flags -band 0x20000000)){throw 'Pin section protection differs.'}
        $offset=[int]($Rva-$section[0].rva+$section[0].raw)
        ,$disk[$offset..($offset+$Count-1)]
    }
    if(-not ('MtaPassiveTiming.Reader' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
namespace MtaPassiveTiming {
 public static class Reader {
  [StructLayout(LayoutKind.Sequential)] public struct ModuleInfo { public IntPtr Base; public uint Size; public IntPtr Entry; }
  [StructLayout(LayoutKind.Sequential)] struct MemoryInfo { public IntPtr Base, Allocation; public uint AllocationProtection; public UIntPtr RegionSize; public uint State, Protection, Type; }
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, uint id);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr process,uint flags,StringBuilder path,ref uint count);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] bytes,UIntPtr count,out UIntPtr copied);
  [DllImport("kernel32.dll", SetLastError=true)] static extern UIntPtr VirtualQueryEx(IntPtr process,IntPtr address,out MemoryInfo info,UIntPtr size);
  [DllImport("psapi.dll", SetLastError=true)] static extern bool EnumProcessModulesEx(IntPtr process,IntPtr[] modules,uint bytes,out uint needed,uint flags);
  [DllImport("psapi.dll", SetLastError=true)] static extern bool GetModuleInformation(IntPtr process,IntPtr module,out ModuleInfo info,uint size);
  public static string ImagePath(IntPtr process) { var path=new StringBuilder(32768);uint count=(uint)path.Capacity;if(!QueryFullProcessImageName(process,0,path,ref count))throw new Win32Exception();return path.ToString(); }
  public static ModuleInfo MainModule(IntPtr process) { var modules=new IntPtr[1];uint needed;ModuleInfo info;if(!EnumProcessModulesEx(process,modules,(uint)IntPtr.Size,out needed,2)||needed<IntPtr.Size||!GetModuleInformation(process,modules[0],out info,(uint)Marshal.SizeOf<ModuleInfo>()))throw new Win32Exception();return info; }
  public static byte[] Read(IntPtr process,ulong image,ulong address,int count,bool code) {
   if(count<1||address<image||address+(ulong)count<address)throw new InvalidOperationException("Invalid image read range");
   ulong end=address+(ulong)count,cursor=address;
   while(cursor<end){MemoryInfo info;if(VirtualQueryEx(process,(IntPtr)(long)cursor,out info,(UIntPtr)Marshal.SizeOf<MemoryInfo>())==UIntPtr.Zero)throw new Win32Exception();
    uint p=info.Protection&255;bool readable=p==2||p==4||p==8||p==32||p==64||p==128;bool executable=p==32||p==64||p==128;
    if(info.State!=0x1000||info.Type!=0x1000000||(ulong)info.Allocation.ToInt64()!=image||(info.Protection&0x101)!=0||!readable||code!=executable)throw new InvalidOperationException("Image page admission refused");
    ulong next=(ulong)info.Base.ToInt64()+info.RegionSize.ToUInt64();if(next<=cursor)throw new InvalidOperationException("Invalid image page extent");cursor=Math.Min(end,next);
   }
   var bytes=new byte[count];UIntPtr copied;if(!ReadProcessMemory(process,(IntPtr)(long)address,bytes,(UIntPtr)count,out copied)||copied.ToUInt64()!=(ulong)count)throw new Win32Exception();return bytes;
  }
 }
}
'@
    }
    $candidates=@(Get-Process -Name SanAndreas -ErrorAction SilentlyContinue)
    if($candidates.Count -ne 1){throw 'Exactly one already-running SanAndreas process is required.'}
    # QUERY_INFORMATION | VM_READ only. No operation in this tool requests
    # VM_WRITE, VM_OPERATION, CREATE_THREAD, SUSPEND_RESUME or TERMINATE.
    $processHandle=[MtaPassiveTiming.Reader]::OpenProcess(0x410,$false,[uint32]$candidates[0].Id)
    if($processHandle -eq [IntPtr]::Zero){throw 'Read-only GTA process handle was refused.'}
    $path=[MtaPassiveTiming.Reader]::ImagePath($processHandle)
    if(-not $path.Equals($expectedPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Running GTA image path differs from the owned Steam executable.'}
    $module=[MtaPassiveTiming.Reader]::MainModule($processHandle);$image=[uint64]$module.Base.ToInt64()
    if($module.Size -ne 97141760 -or -not $image){throw 'Live main module image size differs.'}
    $liveHeader=[MtaPassiveTiming.Reader]::Read($processHandle,$image,$image,4096,$false)
    if([BitConverter]::ToUInt16($liveHeader,0) -ne 0x5a4d -or [BitConverter]::ToUInt32($liveHeader,$pe) -ne 0x4550 -or
       [BitConverter]::ToUInt16($liveHeader,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($liveHeader,$optional) -ne 0x20b -or
       [BitConverter]::ToUInt32($liveHeader,$optional+56) -ne 97141760){throw 'Live PE identity differs.'}
    $identity=[ordered]@{pid=$candidates[0].Id;path=$path;sha256=$expectedSha;moduleBase=('0x{0:x}' -f $image);preferredImageBase=('0x{0:x}' -f [BitConverter]::ToUInt64($disk,$optional+24));imageBytes=$module.Size;processAccess='0x0410 QUERY_INFORMATION|VM_READ'}
    foreach($span in $spans){
        $code=-not $span.ContainsKey('code') -or $span.code
        $expected=Hex (DiskBytes $span.rva $span.bytes $code)
        if($span.ContainsKey('proofHex') -and $expected -cne $span.proofHex){throw 'FPS proof bytes differ from the owned executable.'}
        $actual=Hex ([MtaPassiveTiming.Reader]::Read($processHandle,$image,($image+$span.rva),$span.bytes,$code))
        if($actual -cne $expected){throw "Live code pin differs: $($span.name)"}
        $pins+=@([ordered]@{name=$span.name;rva=('0x{0:x}' -f $span.rva);bytes=$span.bytes;code=$code;hex=$expected;matched=$true})
    }
    Event ([ordered]@{kind='begin';identity=$identity;durationSeconds=$DurationSeconds;staticEvidenceSha256=$evidenceSha;codePins=$pins})
    function RawFields {
        $raw=[ordered]@{}
        foreach($field in $fields){$raw[$field.name]=Hex ([MtaPassiveTiming.Reader]::Read($processHandle,$image,($image+$field.rva),$field.bytes,$false))}
        $raw
    }
    function Word([string]$Value){[BitConverter]::ToUInt32([Convert]::FromHexString($Value),0)}
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while($clock.Elapsed.TotalSeconds -lt $DurationSeconds){
        $sample=$null
        for($retry=0;$retry -lt 3;$retry++){
            $attempts++
            $a=RawFields;$b=RawFields
            $tailFrame=Hex ([MtaPassiveTiming.Reader]::Read($processHandle,$image,($image+0x523a39c),4,$false))
            $tailTimer=Hex ([MtaPassiveTiming.Reader]::Read($processHandle,$image,($image+0x523a458),4,$false))
            $matching=$a.frameCounter -ceq $b.frameCounter -and $a.nativeTimer -ceq $b.nativeTimer -and $b.frameCounter -ceq $tailFrame -and $b.nativeTimer -ceq $tailTimer
            foreach($field in $fields){$matching=$matching -and $a[$field.name] -ceq $b[$field.name]}
            Event ([ordered]@{kind='raw';elapsedMs=$clock.Elapsed.TotalMilliseconds;attempt=$attempts;matching=$matching;first=$a;second=$b;tailFrame=$tailFrame;tailTimer=$tailTimer})
            if($matching){$sample=$b;break}
            $misses++
        }
        if($null -ne $sample){
            $values=[ordered]@{}
            foreach($field in $fields){
                $bytes=[Convert]::FromHexString($sample[$field.name])
                $values[$field.name]=switch($field.type){'u8'{[uint32]$bytes[0]};'u16'{[uint32][BitConverter]::ToUInt16($bytes,0)};'u32'{[BitConverter]::ToUInt32($bytes,0)};'f32'{[double][BitConverter]::ToSingle($bytes,0)};'f64'{[BitConverter]::ToDouble($bytes,0)}}
            }
            $accepted++
            foreach($name in @($fields|Where-Object{$_.type -in @('f32','f64')}|ForEach-Object{$_.name})){
                $value=$values[$name]
                if([double]::IsNaN($value) -or [double]::IsInfinity($value)){$nonfinite++}
                elseif(-not $floatRanges.ContainsKey($name)){$floatRanges[$name]=[ordered]@{minimum=$value;maximum=$value}}
                else{$floatRanges[$name].minimum=[Math]::Min($floatRanges[$name].minimum,$value);$floatRanges[$name].maximum=[Math]::Max($floatRanges[$name].maximum,$value)}
            }
            # Native update clamps are annotations, not a new production input
            # restriction: other legitimate script paths may change raw steps.
            if($values.timeStep -lt [double][single]0.00001 -or $values.timeStep -gt 3 -or $values.oldTimeStep -lt [double][single]0.00001 -or $values.oldTimeStep -gt 3){$outsideUpdateBounds++}
            if($values.hour -gt 23 -or $values.minute -gt 60 -or $values.secondsRaw -gt 65535 -or $values.millisecondsPerGameMinute -lt 1 -or $values.millisecondsPerGameMinute -gt 3600000){$clockOutOfRange++}
            if($NativeFPSProofPath){
                # Compare native stored values only; the ratio is evidence of
                # their relationship, never a replacement value returned as FPS.
                if($values.framesPerSecond -lt 0 -or $values.averageFrameMilliseconds -le 0){$fpsReciprocalMismatches++}
                else{
                    $ratio=[double][single](1000.0/$values.averageFrameMilliseconds)
                    if([Math]::Abs($values.framesPerSecond-$ratio) -gt [Math]::Max(0.0001,[Math]::Abs($ratio)*0.000001)){$fpsReciprocalMismatches++}
                }
                if($null -ne $previous -and $values.nativePlatformSeconds -gt $previous.nativePlatformSeconds){$fpsSourceUpdates++}
            }
            if($null -ne $previous){
                $df=([uint64]$values.frameCounter+4294967296-[uint64]$previous.frameCounter)%4294967296
                $dt=([uint64]$values.nativeTimer+4294967296-[uint64]$previous.nativeTimer)%4294967296
                if($df -gt 2147483647 -or $dt -gt 2147483647){$backward++}else{$frameProgress+=$df;$timerProgress+=$dt}
            }
            if($null -eq $first){$first=$values};$last=$values;$previous=$values
            Event ([ordered]@{kind='accepted';index=$accepted;elapsedMs=$clock.Elapsed.TotalMilliseconds;values=$values})
        }
        Start-Sleep -Milliseconds 50
    }
    # Confirm the same read-only handle still refers to the same image and no
    # qualified instruction span changed during the observational interval.
    if(-not ([MtaPassiveTiming.Reader]::ImagePath($processHandle)).Equals($expectedPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Process identity changed during capture.'}
    foreach($span in $spans){
        $code=-not $span.ContainsKey('code') -or $span.code
        if((Hex ([MtaPassiveTiming.Reader]::Read($processHandle,$image,($image+$span.rva),$span.bytes,$code))) -cne (Hex (DiskBytes $span.rva $span.bytes $code))){throw "Pin changed during capture: $($span.name)"}
    }
    $diskAfter=Sha $expectedPath
    if($diskAfter -cne $expectedSha){throw 'Owned disk executable changed during capture.'}
    if($accepted -lt 20 -or -not $frameProgress -or (-not $NativeFPSProofPath -and -not $timerProgress) -or $nonfinite -or $clockOutOfRange -or $backward -or
       ($NativeFPSProofPath -and (-not $fpsSourceUpdates -or $fpsReciprocalMismatches))){throw 'Capture completed but native timing field validation did not pass; inspect raw evidence.'}
} catch {$failure=$_.Exception.Message} finally {
    if($writer){$writer.Dispose()}
    if($processHandle -ne [IntPtr]::Zero){[MtaPassiveTiming.Reader]::CloseHandle($processHandle)|Out-Null}
    $env:TEMP=$previousTemp;$env:TMP=$previousTmp
    # Add-Type normally leaves no files. An empty workspace directory is harmless;
    # preserve unexpected compiler output as diagnostic evidence rather than
    # deleting an unexamined path or retrying previously rejected folder cleanup.
    $scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Recurse -File).Count
}
$record=[ordered]@{schema='mta_de_passive_game_timing_capture_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');success=($null -eq $failure);failure=$failure;
    directory=$output;tool=[ordered]@{path=$PSCommandPath;sha256=(Sha $PSCommandPath)};identity=$identity;durationSeconds=$DurationSeconds;staticEvidenceSha256=$evidenceSha;
    fields=$fields;codePins=$pins;attempts=$attempts;accepted=$accepted;matchingMisses=$misses;first=$first;last=$last;frameProgress=$frameProgress;timerProgress=$timerProgress;
    nonfiniteFloatSamples=$nonfinite;clockOutOfRangeSamples=$clockOutOfRange;backwardSamples=$backward;outsideNativeUpdateStepBounds=$outsideUpdateBounds;floatRanges=$floatRanges;
    rawSamples=[ordered]@{path=(Join-Path $output 'raw-samples.jsonl');sha256=(Sha (Join-Path $output 'raw-samples.jsonl'));bytes=(Get-Item -LiteralPath (Join-Path $output 'raw-samples.jsonl')).Length};diskSha256After=$diskAfter;scratch=$scratch;scratchRemoved=$scratchRemoved;scratchFilesRemaining=$scratchFilesRemaining;
    nativeFPSProofSha256=$fpsProofSha;nativeFPSSourceUpdates=$fpsSourceUpdates;nativeFPSReciprocalMismatches=$fpsReciprocalMismatches;
    interpretation='Independent passive validation of identified live native fields. Matching copies and frame/timer bookends are bounded coherence checks, not an engine-thread atomicity guarantee or production CGame construction.';
    gameLaunched=$false;injected=$false;memoryWritten=$false;threadsSuspended=$false;processTerminated=$false;focusChanged=$false;installedFilesChanged=$false;productionCGameConstructed=$false}
$record|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $output 'capture-result.json') -Encoding utf8
if($failure){throw "Passive timing capture failed: $failure; evidence: $output"}
$record|ConvertTo-Json -Depth 9 -Compress
