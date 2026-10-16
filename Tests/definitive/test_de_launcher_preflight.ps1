#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the normal MTA launcher and real preflight DLL against owned copies.
# The copied SanAndreas.exe is hashed only; it is never executed by this suite.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameExe,
    [string]$PackageDirectory,
    [string]$GuiPackageDirectory,
    [string]$FrontendPackageDirectory,
    [string]$LauncherExe,
    [string]$PreflightDll
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if(-not $PackageDirectory){$PackageDirectory=Join-Path $root 'Build/definitive-package/Smoke'}
if(-not $LauncherExe){$LauncherExe=Join-Path $root 'Bin/mta-launcher/x64/Multi Theft Auto.exe'}
if(-not $PreflightDll){$PreflightDll=Join-Path $root 'Bin/mta-launcher/x64/mta/de_preflight.dll'}
$knownGame='63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
function Sha([string]$path){return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Sha $GameExe) -ne $knownGame){throw 'Fixture input must be the exact supported cached DE executable.'}
$gameSource=(Resolve-Path -LiteralPath $GameExe).Path
$packageSource=(Resolve-Path -LiteralPath $PackageDirectory).Path
$manifestSource=Join-Path $packageSource 'manifest.json'
$sourceManifestSha=Sha $manifestSource
$sourcePackage=Get-Content -LiteralPath $manifestSource -Raw | ConvertFrom-Json
if($sourcePackage.files.Count -ne 23){throw 'Fixture requires the complete owned 23-file package.'}
$output=Join-Path $root ('Build/mta-de-launcher-preflight/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
$hostRoot=Join-Path $output 'MTA fixture with spaces Ã©'
$bundle=Join-Path $output 'owned bundle with spaces Ã©'
$gameCopy=Join-Path $output 'hashed game copy/SanAndreas.exe'
$manifest=Join-Path $output 'owned-package-manifest.json'
New-Item -ItemType Directory -Path (Join-Path $hostRoot 'mta'),$bundle,(Split-Path $gameCopy -Parent) | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
Copy-Item -LiteralPath $LauncherExe -Destination (Join-Path $hostRoot 'Multi Theft Auto.exe')
Copy-Item -LiteralPath $PreflightDll -Destination (Join-Path $hostRoot 'mta/de_preflight.dll')
Copy-Item -LiteralPath $gameSource -Destination $gameCopy
$packageHashes=[ordered]@{}
foreach($entry in $sourcePackage.files){
    $source=Join-Path $packageSource ('Win64/'+$entry.path)
    if((Sha $source) -ne $entry.sha256){throw 'Source package hash mismatch.'}
    $packageHashes[$entry.path]=$entry.sha256
    $destination=Join-Path $bundle $entry.path
    [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}
$originalManifest=[IO.File]::ReadAllText($manifestSource)
[IO.File]::WriteAllText($manifest,$originalManifest,$utf8)
$sourceNames=@('Client/launch/Main.cpp','Client/launch/StdInc.cpp','Client/launch/StdInc.h','Client/launch/premake5.lua',
    'Client/launch/DefinitivePreflight/Loader.cpp','Client/launch/DefinitivePreflight/Preflight.h',
    'Client/launch/DefinitivePreflight/StrictJson.c','Client/launch/DefinitivePreflight/premake5.lua',
    'Tests/definitive/DePreflightMissingExport.cpp','Tests/definitive/test_de_launcher_preflight.ps1')
$sourceNames+=@(Get-ChildItem -LiteralPath (Join-Path $root 'vendor/json-c') -File | Where-Object {$_.Extension -in @('.h','.c') -or $_.Name -eq 'premake5.lua'} | ForEach-Object {[IO.Path]::GetRelativePath($root,$_.FullName).Replace('\','/')})
$sourceHashes=[ordered]@{}
foreach($name in $sourceNames){$sourceHashes[$name]=Sha (Join-Path $root $name)}
$checks=0
$cases=[Collections.Generic.List[object]]::new()
function Check([bool]$condition,[string]$message){$script:checks++;if(-not $condition){throw $message}}
function Invoke-Case([string]$name,[string[]]$arguments,[int]$exit,[string]$status,[string]$directory=$hostRoot,[int]$verifiedFiles=23){
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=Join-Path $directory 'Multi Theft Auto.exe'
    $start.WorkingDirectory=$output
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    $watch=[Diagnostics.Stopwatch]::StartNew()
    Check ($process.Start()) "$name failed to start owned launcher."
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    if(-not $process.WaitForExit(10000)){
        # This handle belongs only to the launcher just started in this owned
        # fixture. No game process is enumerated, launched or terminated.
        $process.Kill();$process.WaitForExit();throw "$name blocked instead of returning headless status."
    }
    $watch.Stop();$text=$stdout.GetAwaiter().GetResult();$errors=$stderr.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $output ($name+'.stdout.json')),$text,$utf8)
    [IO.File]::WriteAllText((Join-Path $output ($name+'.stderr.log')),$errors,$utf8)
    Check ($process.ExitCode -eq $exit) "$name unexpected exit $($process.ExitCode): $text $errors"
    $record=$text | ConvertFrom-Json
    Check ($record.status -ceq $status) "$name unexpected status: $text"
    Check ($record.preflightValid -eq ($exit -eq 0)) "$name success flag differs from exit."
    Check (-not $record.launchReady -and -not $record.fullMtaReady -and -not $record.gameLaunched -and -not $record.coreLoaded -and -not $record.installationWrites) "$name promoted offline file validation to runtime readiness."
    Check (($exit -eq 0 -and $record.verifiedFiles -eq $verifiedFiles) -or ($exit -ne 0 -and $record.verifiedFiles -eq 0)) "$name owned-file count is wrong."
    Check ([string]::IsNullOrWhiteSpace($errors)) "$name emitted unexpected stderr."
    $script:cases.Add([ordered]@{name=$name;exit_code=$process.ExitCode;status=$record.status;elapsed_ms=$watch.ElapsedMilliseconds;stdout=(Join-Path $output ($name+'.stdout.json'));record=$record})
}
$arguments=@('--de-preflight','--game-exe',$gameCopy,'--bundle',$bundle,'--manifest',$manifest)
Invoke-Case 'valid-unicode-space-paths' $arguments 0 'preflight_valid'
Invoke-Case 'no-implicit-launch' @() 1 'explicit_de_preflight_required'
Invoke-Case 'missing-cli-arguments' @('--de-preflight') 4 'preflight_arguments_required'
Invoke-Case 'relative-game-path' @('--de-preflight','--game-exe','SanAndreas.exe','--bundle',$bundle,'--manifest',$manifest) 4 'normalized_absolute_paths_required'
Invoke-Case 'duplicate-cli-argument' @('--de-preflight','--game-exe',$gameCopy,'--game-exe',$gameCopy,'--manifest',$manifest) 4 'duplicate_argument'

# These DLLs are only dispatch/architecture fixtures. Neither is a mock Core.
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$fixtureDlls=@{}
foreach($architecture in @('x64','x86')){
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $dev,$architecture)
    if($LASTEXITCODE -ne 0){throw 'Fixture compiler environment failed.'}
    foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $dll=Join-Path $output ('missing-export-'+$architecture+'.dll')
    $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
    & $compiler /nologo /LD /MT /O2 /W4 /WX (Join-Path $PSScriptRoot 'DePreflightMissingExport.cpp') ("/Fo"+(Join-Path $output ('missing-export-'+$architecture+'.obj'))) ("/Fe"+$dll) /link /NOLOGO 2>&1 | Set-Content -LiteralPath (Join-Path $output ('fixture-compile-'+$architecture+'.log')) -Encoding utf8
    if($LASTEXITCODE -ne 0){throw 'Owned missing-export fixture compilation failed.'}
    $fixtureDlls[$architecture]=$dll
}
foreach($kind in @('missing','export','architecture')){
    $directory=Join-Path $output ('dispatch-'+$kind)
    New-Item -ItemType Directory -Path (Join-Path $directory 'mta') | Out-Null
    Copy-Item -LiteralPath $LauncherExe -Destination (Join-Path $directory 'Multi Theft Auto.exe')
    if($kind -ne 'missing'){
        $dll=if($kind -eq 'export'){$fixtureDlls.x64}else{$fixtureDlls.x86}
        Copy-Item -LiteralPath $dll -Destination (Join-Path $directory 'mta/de_preflight.dll')
    }
    if($kind -eq 'export'){Invoke-Case 'missing-loader-export' $arguments 3 'preflight_loader_export_failed' $directory}
    else{Invoke-Case ('loader-'+$kind) $arguments 2 'preflight_loader_load_failed' $directory}
}
$wrongGame=Join-Path $output 'wrong-architecture-game/SanAndreas.exe'
[IO.Directory]::CreateDirectory((Split-Path $wrongGame -Parent)) | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Shared/data/launchers/Multi Theft Auto.exe') -Destination $wrongGame
Invoke-Case 'wrong-game-architecture' @('--de-preflight','--game-exe',$wrongGame,'--bundle',$bundle,'--manifest',$manifest) 4 'amd64_pe32plus_required'
$gameBytes=[IO.File]::ReadAllBytes($gameCopy);$gameBytes[-1]=$gameBytes[-1] -bxor 1
[IO.File]::WriteAllBytes($gameCopy,$gameBytes)
Invoke-Case 'wrong-game-fingerprint' $arguments 4 'game_fingerprint_unsupported'
Copy-Item -LiteralPath $gameSource -Destination $gameCopy -Force
$ownedPath=Join-Path $bundle 'CLEO/de-online/index.js'
$ownedBytes=[IO.File]::ReadAllBytes($ownedPath)
[IO.File]::WriteAllBytes($ownedPath,($ownedBytes+[byte]32))
Invoke-Case 'edited-owned-file' $arguments 4 'owned_file_hash_mismatch'
[IO.File]::WriteAllBytes($ownedPath,$ownedBytes)
$missingFile=Join-Path $bundle 'CLEO/de-online/pthread.dll'
Move-Item -LiteralPath $missingFile -Destination ($missingFile+'.saved')
Invoke-Case 'missing-owned-file' $arguments 4 'missing_or_linked_path'
Move-Item -LiteralPath ($missingFile+'.saved') -Destination $missingFile
function Manifest-Case([string]$name,[scriptblock]$change,[string]$status){
    $record=$originalManifest | ConvertFrom-Json
    & $change $record
    [IO.File]::WriteAllText($manifest,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
    Invoke-Case $name $arguments 4 $status
    [IO.File]::WriteAllText($manifest,$originalManifest,$utf8)
}
Manifest-Case 'missing-provenance-field' {param($r) $r.PSObject.Properties.Remove('apiCommit')} 'manifest_identity_invalid'
Manifest-Case 'wrong-provenance-type' {param($r) $r.apiCommit=@{fake=$true}} 'manifest_field_invalid'
Manifest-Case 'changed-dependency-provenance' {param($r) $r.dependencies[0].url='https://invalid.example/other'} 'manifest_dependencies_invalid'
Manifest-Case 'duplicate-owned-path' {param($r) $r.files[1].path=$r.files[0].path;$r.files[1].sha256=$r.files[0].sha256} 'owned_path_not_allowed_or_duplicate'
Manifest-Case 'owned-traversal-path' {param($r) $r.files[0].path='../version.dll'} 'owned_path_not_allowed_or_duplicate'
Manifest-Case 'unknown-file-field' {param($r) $r.files[0]|Add-Member -NotePropertyName fake -NotePropertyValue 1} 'manifest_unknown_field'
Manifest-Case 'mode-config-mismatch' {param($r) $r.mode='Online'} 'manifest_config_mode_mismatch'
$configPath=Join-Path $bundle 'CLEO/de-online/config.mjs'
$configBytes=[IO.File]::ReadAllBytes($configPath)
[IO.File]::WriteAllBytes($configPath,($configBytes+[Text.Encoding]::UTF8.GetBytes((' '*65536)+"export const MODE = 'online';")))
Manifest-Case 'mode-cannot-hide-beyond-config-read-bound' {param($r) ($r.files|Where-Object path -eq 'CLEO/de-online/config.mjs').sha256=Sha $configPath} 'file_size_out_of_bounds'
[IO.File]::WriteAllBytes($configPath,$configBytes)
[IO.File]::WriteAllText($manifest,$originalManifest.Replace('"packageId":','"packageId":"overwritten","packageId":'),$utf8)
Invoke-Case 'duplicate-json-object-key' $arguments 4 'manifest_json_invalid'
[IO.File]::WriteAllText($manifest,($originalManifest+' {}'),$utf8)
Invoke-Case 'trailing-json-object' $arguments 4 'manifest_json_invalid'
[IO.File]::WriteAllText($manifest,(' '*65537),$utf8)
Invoke-Case 'oversized-json' $arguments 4 'file_size_out_of_bounds'
[IO.File]::WriteAllText($manifest,$originalManifest,$utf8)
$nativePath=Join-Path $bundle 'CLEO/de-online/core.dll'
$nativeBytes=[IO.File]::ReadAllBytes($nativePath)
Copy-Item -LiteralPath $fixtureDlls.x86 -Destination $nativePath -Force
Manifest-Case 'rehash-cannot-hide-wrong-native-architecture' {param($r) ($r.files|Where-Object path -eq 'CLEO/de-online/core.dll').sha256=Sha $nativePath} 'amd64_pe32plus_required'
[IO.File]::WriteAllBytes($nativePath,$nativeBytes)
Invoke-Case 'restored-valid-bundle' $arguments 0 'preflight_valid'

# Extra owned GUI groups retain the same no-load/no-launch preflight boundary.
$extraPackages=[Collections.Generic.List[object]]::new()
foreach($spec in @(@{name='gui36';path=$GuiPackageDirectory;count=36},@{name='frontend58';path=$FrontendPackageDirectory;count=58})){
    if(-not $spec.path){continue}
    $source=(Resolve-Path -LiteralPath $spec.path).Path
    $inputManifest=Join-Path $source 'manifest.json'
    $inputHash=Sha $inputManifest
    $parsed=Get-Content -LiteralPath $inputManifest -Raw|ConvertFrom-Json
    Check ($parsed.files.Count -eq $spec.count) 'Extra preflight package has wrong group count.'
    $extraBundle=Join-Path $output ($spec.name+'-owned-bundle')
    $extraManifest=Join-Path $output ($spec.name+'-manifest.json')
    $extraText=[IO.File]::ReadAllText($inputManifest)
    foreach($entry in $parsed.files){
        $inputFile=Join-Path $source ('Win64/'+$entry.path)
        Check ((Sha $inputFile) -eq $entry.sha256) 'Extra input package hash differs.'
        $target=Join-Path $extraBundle $entry.path
        [IO.Directory]::CreateDirectory((Split-Path $target))|Out-Null
        Copy-Item -LiteralPath $inputFile -Destination $target
    }
    [IO.File]::WriteAllText($extraManifest,$extraText,$utf8)
    $extraArguments=@('--de-preflight','--game-exe',$gameCopy,'--bundle',$extraBundle,'--manifest',$extraManifest)
    Invoke-Case ($spec.name+'-valid-complete-group') $extraArguments 0 'preflight_valid' $hostRoot $spec.count
    $groupMember=if($spec.count -eq 58){'CLEO/de-online/mta/locale/en_US/client.pot'}else{'CLEO/de-online/mta/cgui/unifont.ttf'}
    $bad=$extraText|ConvertFrom-Json;$bad.files=@($bad.files|Where-Object path -ne $groupMember)
    [IO.File]::WriteAllText($extraManifest,($bad|ConvertTo-Json -Depth 12),$utf8)
    Invoke-Case ($spec.name+'-partial-group-manifest') $extraArguments 4 'exact_owned_group_count_required'
    [IO.File]::WriteAllText($extraManifest,$extraText,$utf8)
    $member=Join-Path $extraBundle $groupMember
    Move-Item -LiteralPath $member -Destination ($member+'.saved')
    Invoke-Case ($spec.name+'-missing-group-file') $extraArguments 4 'missing_or_linked_path'
    Move-Item -LiteralPath ($member+'.saved') -Destination $member
    $bad=$extraText|ConvertFrom-Json;$bad.files[0].path='CLEO/de-online/extra-unowned.dll'
    [IO.File]::WriteAllText($extraManifest,($bad|ConvertTo-Json -Depth 12),$utf8)
    Invoke-Case ($spec.name+'-unknown-complete-count-path') $extraArguments 4 'owned_path_not_allowed_or_duplicate'
    [IO.File]::WriteAllText($extraManifest,$extraText,$utf8)
    if($spec.count -eq 58){
        $extraConfig=Join-Path $extraBundle 'CLEO/de-online/config.mjs';$extraBytes=[IO.File]::ReadAllBytes($extraConfig)
        foreach($change in @(@{name='duplicate-marker';suffix="`nexport const MTA_CORE_GUI_FRONTEND_REFERENCE = false;`n";status='gui_configuration_marker_invalid'},
                            @{name='forged-other-trial';suffix='';status='gui_mode_conflict'})){
            $text=$utf8.GetString($extraBytes)
            if($change.suffix){$text+=$change.suffix}else{$text=$text.Replace('export const MTA_CORE_RENDER_REFERENCE = false;','export const MTA_CORE_RENDER_REFERENCE = true;')}
            [IO.File]::WriteAllText($extraConfig,$text,$utf8)
            $bad=$extraText|ConvertFrom-Json;($bad.files|Where-Object path -eq 'CLEO/de-online/config.mjs').sha256=Sha $extraConfig
            [IO.File]::WriteAllText($extraManifest,($bad|ConvertTo-Json -Depth 12),$utf8)
            Invoke-Case ('frontend58-'+$change.name) $extraArguments 4 $change.status
        }
        [IO.File]::WriteAllBytes($extraConfig,$extraBytes);[IO.File]::WriteAllText($extraManifest,$extraText,$utf8)
        Invoke-Case 'frontend58-restored-complete-group' $extraArguments 0 'preflight_valid' $hostRoot 58
    }
    $extraPackages.Add(@{source=$source;manifest_sha256=$inputHash;files=$parsed.files;owned_count=$spec.count})
}
foreach($extra in $extraPackages){
    Check ((Sha (Join-Path $extra.source 'manifest.json')) -eq $extra.manifest_sha256) 'Extra source manifest changed.'
    foreach($entry in $extra.files){Check ((Sha (Join-Path $extra.source ('Win64/'+$entry.path))) -eq $entry.sha256) 'Extra source package changed.'}
}
foreach($entry in $sourcePackage.files){Check ((Sha (Join-Path $packageSource ('Win64/'+$entry.path))) -ceq $entry.sha256) 'Input package changed.'}
Check ((Sha $manifestSource) -ceq $sourceManifestSha) 'Input manifest changed.'
Check ((Sha $gameSource) -ceq $knownGame) 'Input game executable changed.'
foreach($name in $sourceHashes.Keys){Check ((Sha (Join-Path $root $name)) -ceq $sourceHashes[$name]) 'Fixture source changed during execution.'}
$artifacts=@(@{path=(Resolve-Path -LiteralPath $LauncherExe).Path;sha256=(Sha $LauncherExe);architecture='AMD64'},@{path=(Resolve-Path -LiteralPath $PreflightDll).Path;sha256=(Sha $PreflightDll);architecture='AMD64'})
$result=[ordered]@{success=$true;created_at=[DateTimeOffset]::Now.ToString('o');directory=$output;checks=$checks;case_count=$cases.Count;cases=$cases;source_hashes=$sourceHashes;source_unchanged=$true;artifacts=$artifacts;package_manifest_sha256=$sourceManifestSha;package_hashes=$packageHashes;extra_packages=$extraPackages;game_executable_sha256=$knownGame;game_executable_executed=$false;game_files_changed=$false;game_launched=$false;process_injection=$false;core_loaded=$false;launchReady=$false;fullMtaReady=$false;scope='Actual normal x64 MTA launcher dispatch and read-only DE bundle preflight in owned copies; no new game startup path'}
$target=Join-Path $output 'test-result.json'
$result|ConvertTo-Json -Depth 14|Set-Content -LiteralPath $target -Encoding utf8
@{success=$true;path=$target;sha256=(Sha $target);checks=$checks;cases=$cases.Count;launcher_sha256=(Sha $LauncherExe);loader_sha256=(Sha $PreflightDll)}|ConvertTo-Json -Compress
