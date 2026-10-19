#requires -Version 7.5
# Correlate the native menu owners with a human-reported GTA screen. This opens
# an already-running game for reading; the production CGame getter is not run.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkDirectory,
    [ValidateSet('unclassified','gameplay','native-menu-open','native-menu-closed')][string]$Phase='unclassified',
    [switch]$WaitForFocus,
    [switch]$CompileOnly
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if([IntPtr]::Size -ne 8){throw 'Use native x64 PowerShell.'}
$repository=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$work=(Resolve-Path -LiteralPath $WorkDirectory).Path
$allowed=[IO.Path]::GetFullPath((Join-Path $repository 'Build/definitive-recovery')).TrimEnd('\')+'\'
if(-not ($work+'\').StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Work directory must be in workspace Build/definitive-recovery.'}
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$output=Join-Path $work ('live-menu-'+$Phase+'-'+$id)
$scratch=Join-Path $repository ('Build/tmp/passive-menu-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
$env:TEMP=$scratch;$env:TMP=$scratch
$expectedPath='C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64\SanAndreas.exe'
$expectedSha='63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$handle=[IntPtr]::Zero;$identity=$null;$failure=$null;$samples=@();$prefixes=@();$compiled=$false
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Hex([byte[]]$Bytes){[Convert]::ToHexString($Bytes).ToLowerInvariant()}
try {
    if(-not ('MtaPassiveMenu.Reader' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
namespace MtaPassiveMenu {
 public static class Reader {
  [StructLayout(LayoutKind.Sequential)] public struct Region { public IntPtr Base,Allocation;public uint AllocationProtection;public ushort Partition;public UIntPtr Size;public uint State,Protection,Type; }
  [StructLayout(LayoutKind.Sequential)] public struct Module { public IntPtr Base;public uint Size;public IntPtr Entry; }
  [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr process,uint flags,StringBuilder path,ref uint size);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] bytes,UIntPtr size,out UIntPtr copied);
  [DllImport("kernel32.dll",SetLastError=true)] static extern UIntPtr VirtualQueryEx(IntPtr process,IntPtr address,out Region info,UIntPtr size);
  [DllImport("psapi.dll",SetLastError=true)] static extern bool EnumProcessModulesEx(IntPtr process,IntPtr[] modules,uint bytes,out uint needed,uint flags);
  [DllImport("psapi.dll",SetLastError=true)] static extern bool GetModuleInformation(IntPtr process,IntPtr module,out Module info,uint bytes);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
  public static bool Focused(uint pid){uint foreground;return GetWindowThreadProcessId(GetForegroundWindow(),out foreground)!=0&&foreground==pid;}
  public static IntPtr Open(uint pid){return OpenProcess(0x410,false,pid);}
  public static string Path(IntPtr process){var value=new StringBuilder(32768);uint size=(uint)value.Capacity;if(!QueryFullProcessImageName(process,0,value,ref size))throw new Win32Exception();return value.ToString();}
  public static Module Main(IntPtr process){var modules=new IntPtr[1];uint needed;Module info;if(!EnumProcessModulesEx(process,modules,(uint)IntPtr.Size,out needed,2)||needed<IntPtr.Size||!GetModuleInformation(process,modules[0],out info,(uint)Marshal.SizeOf<Module>()))throw new Win32Exception();return info;}
  static byte[] Read(IntPtr process,ulong image,ulong address,int size,bool code){
   if(size<1||address==0||address>ulong.MaxValue-(ulong)size)throw new InvalidOperationException("Invalid bounded read");
   ulong end=address+(ulong)size,cursor=address;
   while(cursor<end){Region page;if(VirtualQueryEx(process,(IntPtr)(long)cursor,out page,(UIntPtr)Marshal.SizeOf<Region>())==UIntPtr.Zero)throw new Win32Exception();
    uint p=page.Protection&255;bool execute=p==32||p==64||p==128;bool readable=p==2||p==4||p==8||execute;
    if(page.State!=0x1000||(page.Protection&0x101)!=0||!readable||execute!=code)throw new InvalidOperationException("Read page policy refused");
    if(image!=0){if(address<image||page.Type!=0x1000000||(ulong)page.Allocation.ToInt64()!=image)throw new InvalidOperationException("Image identity refused");}
    else if((page.Type!=0x20000&&page.Type!=0x40000)||(p!=4&&p!=8))throw new InvalidOperationException("Writable non-executing receiver page required");
    ulong start=(ulong)page.Base.ToInt64();if(page.Size.ToUInt64()>ulong.MaxValue-start)throw new InvalidOperationException("Invalid page extent");ulong next=start+page.Size.ToUInt64();if(next<=cursor)throw new InvalidOperationException("Invalid page extent");cursor=Math.Min(end,next);
   }
   var bytes=new byte[size];UIntPtr copied;if(!ReadProcessMemory(process,(IntPtr)(long)address,bytes,(UIntPtr)size,out copied)||copied.ToUInt64()!=(ulong)size)throw new Win32Exception();return bytes;
  }
  public static byte[] Image(IntPtr process,ulong image,ulong address,int size,bool code){return Read(process,image,address,size,code);}
  public static byte[] Heap(IntPtr process,ulong address,int size){return Read(process,0,address,size,false);}
 }
}
'@
    }
    $compiled=$true
    if(-not $CompileOnly){
        if((Sha $expectedPath) -cne $expectedSha){throw 'The installed executable hash differs.'}
        $disk=[IO.File]::ReadAllBytes($expectedPath)
        $pe=[BitConverter]::ToInt32($disk,0x3c);$optional=$pe+24
        if([BitConverter]::ToUInt16($disk,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($disk,$optional) -ne 0x20b -or [BitConverter]::ToUInt32($disk,$optional+56) -ne 97141760){throw 'Executable PE identity differs.'}
        $sectionStart=$optional+[BitConverter]::ToUInt16($disk,$pe+20)
        $sections=@(for($i=0;$i -lt [BitConverter]::ToUInt16($disk,$pe+6);$i++){
            $at=$sectionStart+40*$i
            @{rva=[BitConverter]::ToUInt32($disk,$at+12);raw=[BitConverter]::ToUInt32($disk,$at+20);size=[BitConverter]::ToUInt32($disk,$at+16)}
        })
        $games=@(Get-Process -Name SanAndreas -ErrorAction SilentlyContinue)
        if($games.Count -ne 1){throw 'Exactly one already-running SanAndreas process is required.'}
        $handle=[MtaPassiveMenu.Reader]::Open([uint32]$games[0].Id)
        if($handle -eq [IntPtr]::Zero){throw 'Read-only process access was refused.'}
        if(-not ([MtaPassiveMenu.Reader]::Path($handle)).Equals($expectedPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Running executable path differs.'}
        $module=[MtaPassiveMenu.Reader]::Main($handle);$image=[uint64]$module.Base.ToInt64()
        if(-not $image -or $module.Size -ne 97141760){throw 'Live main module size differs.'}
        $identity=@{pid=$games[0].Id;startTime=$games[0].StartTime.ToUniversalTime().ToString('o');imageBase=('0x{0:x}' -f $image);path=$expectedPath;sha256=$expectedSha;access='0x0410 QUERY_INFORMATION|VM_READ'}
        # These are bounded identity prefixes, not a claim to execute the
        # production reader or prove every native instruction in the process.
        foreach($rva in @(0x13facf0,0x1297bc0,0xafcd80,0xafdc40)){
            $section=@($sections|Where-Object{$rva -ge $_.rva -and [uint64]$rva+16 -le [uint64]$_.rva+$_.size})
            if($section.Count -ne 1){throw 'Identity prefix escaped its disk section.'}
            $offset=[int]($rva-$section[0].rva+$section[0].raw)
            $expected=Hex $disk[$offset..($offset+15)]
            $actual=Hex ([MtaPassiveMenu.Reader]::Image($handle,$image,($image+$rva),16,$true))
            if($actual -cne $expected){throw 'Live menu identity prefix differs.'}
            $prefixes+=@(@{rva=('0x{0:x}' -f $rva);hex=$expected})
        }
        function Snapshot {
            $root=[BitConverter]::ToUInt64([MtaPassiveMenu.Reader]::Image($handle,$image,($image+0x5725750),8,$false),0)
            if(-not $root -or $root -gt [uint64]::MaxValue-0x800){throw 'Native receiver unavailable.'}
            $vtable=[BitConverter]::ToUInt64([MtaPassiveMenu.Reader]::Heap($handle,$root,8),0)
            if($vtable -ne $image+0x431aea8){throw 'Native receiver is not the proved SA interface.'}
            $menu=[MtaPassiveMenu.Reader]::Heap($handle,($root+0x348),8)
            $legacy=[MtaPassiveMenu.Reader]::Image($handle,$image,($image+0x5069630),24,$false)
            $state=[MtaPassiveMenu.Reader]::Image($handle,$image,($image+0x5300ff8),4,$false)
            $rootAfter=[BitConverter]::ToUInt64([MtaPassiveMenu.Reader]::Image($handle,$image,($image+0x5725750),8,$false),0)
            $vtableAfter=[BitConverter]::ToUInt64([MtaPassiveMenu.Reader]::Heap($handle,$root,8),0)
            if($root -ne $rootAfter -or $vtable -ne $vtableAfter){throw 'Receiver identity changed during observation.'}
            [ordered]@{root=('0x{0:x}' -f $root);vtable=('0x{0:x}' -f $vtable);slot=(Hex $menu);legacy=(Hex $legacy);state=(Hex $state)}
        }
        function WaitStableGameFocus {
            $stable=[Diagnostics.Stopwatch]::new()
            while($stable.ElapsedMilliseconds -lt 3000){
                if($games[0].HasExited){throw 'GTA exited before focused observation.'}
                if([MtaPassiveMenu.Reader]::Focused([uint32]$games[0].Id)){$stable.Start()}else{$stable.Reset()}
                Start-Sleep -Milliseconds 100
            }
        }
        $focusSettled=$false
        for($index=0;$index -lt 30;$index++){
            # Human replies switch focus away from GTA. Wait for the actual
            # foreground game instead of confusing a background pause with the
            # requested screen. Arming has no wall-clock expiry.
            if($WaitForFocus -and -not $focusSettled){WaitStableGameFocus;$focusSettled=$true}
            $focusedBefore=[MtaPassiveMenu.Reader]::Focused([uint32]$games[0].Id)
            if($WaitForFocus -and -not $focusedBefore){$focusSettled=$false;$index--;continue}
            $first=Snapshot;$second=Snapshot
            $focusedAfter=[MtaPassiveMenu.Reader]::Focused([uint32]$games[0].Id)
            if($WaitForFocus -and -not $focusedAfter){$focusSettled=$false;$index--;continue}
            $matching=($first|ConvertTo-Json -Compress) -ceq ($second|ConvertTo-Json -Compress)
            $menu=[Convert]::FromHexString($second.slot);$legacy=[Convert]::FromHexString($second.legacy)
            $widget=[BitConverter]::ToUInt64($menu,0);$data=[BitConverter]::ToUInt64($legacy,0);$count=[BitConverter]::ToInt32($legacy,8);$capacity=[BitConverter]::ToInt32($legacy,12);$pending=[BitConverter]::ToUInt64($legacy,16)
            $valid=$count -ge 0 -and $capacity -ge 0 -and $count -le $capacity -and ($count -eq 0 -or $data -ne 0)
            $samples+=@(@{index=$index;matching=$matching;headerValid=$valid;focusedBefore=$focusedBefore;focusedAfter=$focusedAfter;first=$first;second=$second;nativeState=[BitConverter]::ToUInt32([Convert]::FromHexString($second.state),0);widgetPresent=($widget -ne 0);legacyCount=$count;legacyCapacity=$capacity;legacyPending=($pending -ne 0);combinedPresence=($widget -ne 0 -or $count -gt 0 -or $pending -ne 0)})
            Start-Sleep -Milliseconds 50
        }
        if(@($samples|Where-Object{$_.matching -and $_.headerValid}).Count -lt 20){throw 'Insufficient matching native menu observations.'}
        foreach($pin in $prefixes){$rva=[Convert]::ToUInt64($pin.rva.Substring(2),16);if((Hex ([MtaPassiveMenu.Reader]::Image($handle,$image,($image+$rva),16,$true))) -cne $pin.hex){throw 'Identity prefix changed during capture.'}}
        if((Sha $expectedPath) -cne $expectedSha){throw 'Disk executable changed during capture.'}
    }
} catch {$failure=$_.Exception.Message} finally {
    if($handle -ne [IntPtr]::Zero){[MtaPassiveMenu.Reader]::CloseHandle($handle)|Out-Null}
    $env:TEMP=$previousTemp;$env:TMP=$previousTmp
}
$record=[ordered]@{schema='mta_de_passive_menu_observation_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');success=($null -eq $failure);failure=$failure;directory=$output;tool=@{path=$PSCommandPath;sha256=(Sha $PSCommandPath)};compileOnly=[bool]$CompileOnly;helperCompiled=$compiled;humanReportedPhase=$Phase;focusOnly=[bool]$WaitForFocus;focusSettleMilliseconds=$(if($WaitForFocus){3000}else{0});identity=$identity;identityPrefixes=$prefixes;samples=$samples;matchingSamples=@($samples|Where-Object{$_.matching -and $_.headerValid}).Count;scratch=$scratch;scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Recurse -File).Count;gameLaunched=$false;memoryWritten=$false;focusChanged=$false;installedFilesChanged=$false;productionCGameGetterExecuted=$false;interpretation='Read-only field correlation with a human-reported screen. Matching samples are bounded observations, not an engine lock or proof of full CGame execution.'}
$result=Join-Path $output 'capture-result.json'
$record|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $result -Encoding utf8NoBOM
[ordered]@{success=$record.success;compileOnly=$record.compileOnly;matchingSamples=$record.matchingSamples;result=$result;failure=$failure}|ConvertTo-Json -Compress
if($failure){throw "Passive menu observation failed; inspect $result"}
