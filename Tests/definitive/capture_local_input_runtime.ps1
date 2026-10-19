#requires -Version 7.5
# Compact, read-only handoff evidence. Every mode saves the fourteen raw logs
# before checking the game, payload or protocol. No historical alias is edited.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Baseline','Actual','Shutdown')][string]$Mode,
    [Parameter(Mandatory)][string]$InstalledReady,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedReadySha256,
    [string]$Baseline,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedBaselineSha256,
    [string]$Actual,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedActualSha256,
    [string]$NodeExecutable='node',
    [string]$HumanReport,
    [switch]$HumanNormalExitReported
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$modulePath=Join-Path $PSScriptRoot 'LocalInputRuntimeEvidence.psm1'
Import-Module $modulePath -Force -DisableNameChecking
$repositoryRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=New-LirOutputDirectory $repositoryRoot $Mode
$checks=[Collections.Generic.List[object]]::new()
$logs=[Collections.Generic.List[object]]::new()
$ready=$null;$readyPin=$null;$savedReady=$null;$baselineRecord=$null;$baselinePin=$null;$actualRecord=$null;$actualPin=$null
$failure=$null;$rawCaptureComplete=$false;$captureComplete=$false;$protocolPassed=$false;$cleanupPassed=$false;$moduleCleanupPassed=$false
$observedFiles=@();$observedSources=@();$observedProtected=@();$processBefore=$null;$processAfter=$null;$modulesBefore=@();$modulesAfter=@()
$cohort=$null;$continuities=@();$verifierResult=$null;$verifierOutput=$null;$verifierExit=$null;$eventsPin=$null;$normalExitEvidenceVerified=$false;$cleoShutdownEvidence=$null
$savedManifests=@();$currentHead=$null
$trialDllNames=@('core.dll','game_de.dll','de_client_bridge.dll','cgui.dll','xmll.dll','pthread.dll')
$expectedLogPaths=@('cleo_redux.log')+@('CEGUI.log','console.log','gui-console.log','gui-frontend-input.log','gui-frontend.log','gui-startup.log',
    'gui-surface.log','logfile.txt','render-endpoint.log','render-roots.log','render-setup.log','persistent-frontend.log','frontend-cursor.log'|ForEach-Object{'CLEO/de-online/mta/logs/'+$_})
function Check([string]$Name,[bool]$Passed){$checks.Add([ordered]@{name=$Name;passed=$Passed});if(-not $Passed){throw "Capture refused: $Name"}}
function ReadJson([string]$Path){Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json -DateKind String}
function SmallPin([string]$Path){$pin=Get-LirPin $Path;Check ('bounded-metadata:'+ $pin.path) ($pin.bytes -le 2097152);$pin}
function Under([string]$Root,[string]$Relative){
    Check ('safe-relative-path:'+ $Relative) (-not [string]::IsNullOrWhiteSpace($Relative) -and $Relative -notmatch '(^/|\\|:|(^|/)\.\.?(/|$))')
    $path=[IO.Path]::GetFullPath((Join-Path $Root $Relative))
    Check ('inside-declared-root:'+ $Relative) ($path.StartsWith([IO.Path]::GetFullPath($Root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))
    $path
}
function RealProcess {
    $candidates=@(Get-Process -Name SanAndreas -ErrorAction SilentlyContinue)
    Check 'exactly-one-GTA-process' ($candidates.Count -eq 1)
    $game=$candidates[0]
    Check 'GTA-exact-installed-image-path' ([IO.Path]::GetFullPath($game.Path).Equals($ready.game.path,[StringComparison]::OrdinalIgnoreCase))
    [ordered]@{pid=$game.Id;startTimeUtc=$game.StartTime.ToUniversalTime().ToString('o');path=$game.Path;responding=$game.Responding}
}
function TrialModules($Identity){
    $process=Get-Process -Id $Identity.pid -ErrorAction Stop
    Check 'module-sample-same-process-start' ($process.StartTime.ToUniversalTime().ToString('o') -ceq $Identity.startTimeUtc)
    @($process.Modules|Where-Object{$_.ModuleName.ToLowerInvariant() -in $trialDllNames}|ForEach-Object{
        [ordered]@{name=$_.ModuleName.ToLowerInvariant();path=$_.FileName;memoryBytes=$_.ModuleMemorySize}
    }|Sort-Object name,path)
}
function VerifyReady {
    Check 'ready-pin-and-schema' ($readyPin.sha256 -ceq $ExpectedReadySha256.ToLowerInvariant() -and $ready.schema -ceq 'mta_local_input_runtime_installed_ready_v1' -and
        $ready.sourceCommit -match '^[a-f0-9]{40}$' -and $ready.success -eq $true)
    Check 'known-game-root' ([IO.Path]::GetFullPath($ready.gameDirectory).Equals([IO.Path]::GetDirectoryName($ready.game.path),[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($ready.game.path) -ieq 'SanAndreas.exe' -and $ready.game.sha256 -ceq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f')
    Check 'ready-exact-log-set' (@($ready.logPaths).Count -eq 14 -and @($ready.logPaths|Select-Object -Unique).Count -eq 14 -and
        @(Compare-Object $expectedLogPaths @($ready.logPaths)).Count -eq 0)
    Check 'ready-tool-and-module-identity' ([IO.Path]::GetFullPath($ready.captureTool.path).Equals($PSCommandPath,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFullPath($ready.captureModule.path).Equals($modulePath,[StringComparison]::OrdinalIgnoreCase))
    Check 'pinned-validator-and-verifier-workspace-paths' ([IO.Path]::GetFullPath($ready.validatorSource.path).Equals((Join-Path $repositoryRoot 'Client/definitive/game-adapter/index.js'),[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFullPath($ready.verifier.path).Equals((Join-Path $PSScriptRoot 'local_input_runtime_evidence.cjs'),[StringComparison]::OrdinalIgnoreCase))
    Check 'ready-payload-and-source-counts' (@($ready.owned).Count -eq 58 -and @($ready.owned.path|Select-Object -Unique).Count -eq 58 -and
        @($ready.sourcePins).Count -eq 36 -and @($ready.sourcePins.path|Select-Object -Unique).Count -eq 36 -and @($ready.loaderFiles).Count -eq 2)
    foreach($pin in @($ready.game)+@($ready.loaderFiles)+@($ready.installedManifest,$ready.packageManifest,$ready.validatorSource,$ready.verifier,$ready.captureTool,$ready.captureModule)){
        $script:observedProtected+=@(Assert-LirPin $pin)
    }
    foreach($pin in $ready.sourcePins){
        Check ('source-inside-workspace:'+ $pin.path) ([IO.Path]::GetFullPath($pin.path).StartsWith($repositoryRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))
        $script:observedSources+=@(Assert-LirPin $pin)
    }
    $manifest=ReadJson $ready.installedManifest.path;$package=ReadJson $ready.packageManifest.path
    Check 'manifest-owned-file-sets' (@($manifest.files).Count -eq 58 -and @($package.files).Count -eq 58 -and
        @($manifest.files.path|Select-Object -Unique).Count -eq 58 -and @($package.files.path|Select-Object -Unique).Count -eq 58 -and
        @(Compare-Object @($manifest.files.path) @($ready.owned.path)).Count -eq 0 -and @(Compare-Object @($package.files.path) @($ready.owned.path)).Count -eq 0)
    foreach($row in $ready.owned){
        $destination=Under $ready.gameDirectory $row.path
        Check ('owned-destination:'+ $row.path) ($destination.Equals([IO.Path]::GetFullPath($row.destination),[StringComparison]::OrdinalIgnoreCase))
        $pin=Assert-LirPin ([pscustomobject]@{path=$destination;bytes=$row.bytes;sha256=$row.sha256})
        $installed=@($manifest.files|Where-Object{$_.path -ceq $row.path})[0];$staged=@($package.files|Where-Object{$_.path -ceq $row.path})[0]
        Check ('manifest-owned-byte:'+ $row.path) ($installed.sha256 -ceq $pin.sha256 -and $staged.sha256 -ceq $pin.sha256 -and
            [IO.Path]::GetFullPath($installed.destination).Equals($destination,[StringComparison]::OrdinalIgnoreCase))
        $script:observedFiles+=@([ordered]@{path=$row.path;observed=$pin})
    }
    $config=[IO.File]::ReadAllText((Under $ready.gameDirectory 'CLEO/de-online/config.mjs'))
    $booleans=[regex]::Matches($config,'(?m)^export const ([A-Z0-9_]+) = (true|false);\s*$')
    Check 'runtime-only-Smoke-manual-config' (@($booleans|Where-Object{$_.Groups[2].Value -ceq 'true'}).Count -eq 1 -and
        @($booleans|Where-Object{$_.Groups[1].Value -ceq 'MTA_CORE_FRONTEND_RUNTIME' -and $_.Groups[2].Value -ceq 'true'}).Count -eq 1 -and
        [regex]::Matches($config,"export const MODE = 'smoke';").Count -eq 1 -and [regex]::Matches($config,"export const ONLINE_ACTIVATION = 'manual';").Count -eq 1)
    $script:savedManifests=@((Copy-LirPinnedMetadata $output 'installed-manifest.json' $ready.installedManifest),(Copy-LirPinnedMetadata $output 'package-manifest.json' $ready.packageManifest))
}
function CheckModuleCleanup($Modules,$Final){
    $mustBeGone=@($Modules|Where-Object{$_.name -in @('core.dll','cgui.dll','xmll.dll','pthread.dll')})
    $backend=@($Modules|Where-Object{$_.name -ceq 'game_de.dll'});$bridge=@($Modules|Where-Object{$_.name -ceq 'de_client_bridge.dll'})
    if($mustBeGone.Count -or $backend.Count -ne $Final[32] -or $bridge.Count -gt 1){return $false}
    foreach($row in @($backend)+@($bridge)){
        if(-not $row.path.Equals((Join-Path $ready.gameDirectory ('CLEO/de-online/'+$row.name)),[StringComparison]::OrdinalIgnoreCase)){return $false}
    }
    $true
}
try {
    # Parse only enough trusted handoff metadata to locate fourteen allowlisted
    # diagnostics. A failed proof must still retain every successfully read log.
    $readyPin=SmallPin $InstalledReady;$savedReady=Copy-LirPinnedMetadata $output 'installed-ready.json' ([pscustomobject]$readyPin)
    $ready=ReadJson $savedReady.path
    Check 'raw-capture-log-allowlist' (@($ready.logPaths).Count -eq 14 -and @($ready.logPaths|Select-Object -Unique).Count -eq 14 -and
        @(Compare-Object $expectedLogPaths @($ready.logPaths)).Count -eq 0)
    foreach($relative in $ready.logPaths){$logs.Add((Save-LirRawLog (Under $ready.gameDirectory $relative) $output ('logs/'+$relative)))}
    $rawCaptureComplete=$logs.Count -eq 14 -and @($logs|Where-Object{-not $_.captureSucceeded}).Count -eq 0
    $null=Write-LirJson $output 'raw-preservation.json' ([ordered]@{recordedAt=[DateTimeOffset]::Now.ToString('o');mode=$Mode;rawCaptureComplete=$rawCaptureComplete;logs=$logs.ToArray();protocolNotEvaluated=$true})
    Check 'fourteen-raw-files-saved-successfully' $rawCaptureComplete
    VerifyReady
    $cleo=@($logs|Where-Object{[IO.Path]::GetFileName($_.sourcePath) -ieq 'cleo_redux.log'})[0]
    $whole=[IO.File]::ReadAllText($cleo.saved.path);$headers=@(Get-LirCleoHeaders $whole)
    Check 'CLEO-cohort-header-and-complete-lines' ($headers.Count -gt 0 -and $cleo.newlineBoundary)
    if($Mode -eq 'Shutdown'){
        Check 'actual-handoff-arguments' (-not [string]::IsNullOrWhiteSpace($Actual) -and -not [string]::IsNullOrWhiteSpace($ExpectedActualSha256))
        $actualPin=SmallPin $Actual;Check 'actual-handoff-pin' ($actualPin.sha256 -ceq $ExpectedActualSha256.ToLowerInvariant())
        $null=Copy-LirPinnedMetadata $output 'actual.json' ([pscustomobject]$actualPin);$actualRecord=ReadJson $actualPin.path
        Check 'actual-same-ready-and-complete-raw-capture' ($actualRecord.schema -ceq 'mta_local_input_runtime_actual_v1' -and $actualRecord.rawCaptureComplete -eq $true -and
            $actualRecord.installedReady.sha256 -ceq $readyPin.sha256 -and $actualRecord.sourceCommit -ceq $ready.sourceCommit)
        Check 'GTA-and-bootstrap-absent' (@(Get-Process -Name SanAndreas,PlayGTASanAndreas -ErrorAction SilentlyContinue).Count -eq 0)
        foreach($row in $logs){
            $prior=@($actualRecord.logs|Where-Object{$_.sourcePath -ceq $row.sourcePath});Check 'one-actual-log-prefix' ($prior.Count -eq 1)
            $continuity=Test-LirPrefix $row.saved.path $prior[0].saved
            $continuities+=@([ordered]@{sourcePath=$row.sourcePath;actualWholePrefixPreserved=$continuity.preserved;actualPrefix=$prior[0].saved;current=$row.saved})
            Check ('actual-whole-prefix-retained:'+ $row.sourcePath) $continuity.preserved
        }
        $priorCleo=@($actualRecord.logs|Where-Object{[IO.Path]::GetFileName($_.sourcePath) -ieq 'cleo_redux.log'})[0]
        $suffix=Get-LirCleoDelta $cleo.saved.path $priorCleo.saved
        Check 'same-CLEO-cohort-at-shutdown' ($headers.Count -eq $actualRecord.cohort.headerCount -and $headers[-1].text -ceq $actualRecord.cohort.lastHeader -and
            $headers[-1].characterOffset -eq $actualRecord.cohort.lastHeaderCharacterOffset)
        $cleoShutdownEvidence=Get-LirCleoShutdownEvidence $suffix
        Check 'fresh-CLEO-shutdown-and-Exit' $cleoShutdownEvidence.verified
        $cohort=$actualRecord.cohort;$normalExitEvidenceVerified=$true
    } else {
        $processBefore=RealProcess;$modulesBefore=@(TrialModules $processBefore)
        if($Mode -eq 'Baseline'){
            Check 'fresh-process-after-ready' ([DateTimeOffset]::Parse($processBefore.startTimeUtc) -ge [DateTimeOffset]::Parse($ready.recordedAt).AddSeconds(-2))
            Check 'all-six-trial-DLLs-absent-before-activation' ($modulesBefore.Count -eq 0)
            $startLocal=[DateTimeOffset]::Parse($processBefore.startTimeUtc).LocalDateTime
            $headerTime=[TimeSpan]::ParseExact($headers[-1].text.Substring(0,8),'hh\:mm\:ss',[Globalization.CultureInfo]::InvariantCulture)
            $headerLocal=$startLocal.Date.Add($headerTime)
            if(($headerLocal-$startLocal).TotalHours -gt 12){$headerLocal=$headerLocal.AddDays(-1)}
            if(($headerLocal-$startLocal).TotalHours -lt -12){$headerLocal=$headerLocal.AddDays(1)}
            Check 'CLEO-startup-time-bound-to-fresh-process' ($headerLocal -ge $startLocal.AddSeconds(-2) -and $headerLocal -le $startLocal.AddMinutes(2))
            $launch=$whole.Substring($headers[-1].characterOffset);$launchEvents=@(ConvertFrom-LirCleoEvents $launch)
            $armed=@($launchEvents|Where-Object{$_.PSObject.Properties['event'] -and $_.event -ceq 'adapter_armed'})
            Check 'one-unactivated-F9-Smoke-arming' ($armed.Count -eq 1 -and $armed[0].mode -ceq 'smoke' -and $armed[0].activate_key -ceq 'Ctrl+Shift+F9' -and
                $armed[0].bridge_loaded -eq $false -and $armed[0].game_adapter_active -eq $false -and $armed[0].network_active -eq $false -and
                @($launchEvents|Where-Object{$_.PSObject.Properties['event'] -and $_.event -in @('adapter_activation_requested','adapter_activation_released','adapter_activation_cancelled','adapter_failed','frontend_runtime_started')}).Count -eq 0)
            $cohort=[ordered]@{headerCount=$headers.Count;lastHeader=$headers[-1].text;lastHeaderCharacterOffset=$headers[-1].characterOffset;startupLocalTime=$headerLocal.ToString('s');activationNotObserved=$true}
        } else {
            Check 'baseline-handoff-arguments' (-not [string]::IsNullOrWhiteSpace($Baseline) -and -not [string]::IsNullOrWhiteSpace($ExpectedBaselineSha256))
            $baselinePin=SmallPin $Baseline;Check 'baseline-handoff-pin' ($baselinePin.sha256 -ceq $ExpectedBaselineSha256.ToLowerInvariant())
            $null=Copy-LirPinnedMetadata $output 'baseline.json' ([pscustomobject]$baselinePin);$baselineRecord=ReadJson $baselinePin.path
            Check 'baseline-ready-source-and-process-match' ($baselineRecord.schema -ceq 'mta_local_input_runtime_baseline_v1' -and $baselineRecord.success -eq $true -and
                $baselineRecord.installedReady.sha256 -ceq $readyPin.sha256 -and $baselineRecord.sourceCommit -ceq $ready.sourceCommit -and
                $baselineRecord.processAfter.pid -eq $processBefore.pid -and $baselineRecord.processAfter.startTimeUtc -ceq $processBefore.startTimeUtc -and
                $baselineRecord.processAfter.path -ceq $processBefore.path)
            foreach($row in $logs){
                $prior=@($baselineRecord.logs|Where-Object{$_.sourcePath -ceq $row.sourcePath});Check 'one-baseline-log-prefix' ($prior.Count -eq 1)
                $prefix=Test-LirPrefix $row.saved.path $prior[0].saved
                $continuities+=@([ordered]@{sourcePath=$row.sourcePath;baselinePrefix=$prior[0].saved;current=$row.saved;wholePrefixPreserved=$prefix.preserved;
                    logResetObserved=(-not $prefix.preserved);intermediateHistoryRecovered=$false})
            }
            $priorCleo=@($baselineRecord.logs|Where-Object{[IO.Path]::GetFileName($_.sourcePath) -ieq 'cleo_redux.log'})[0]
            $delta=Get-LirCleoDelta $cleo.saved.path $priorCleo.saved
            Check 'same-CLEO-startup-cohort' ($headers.Count -eq $baselineRecord.cohort.headerCount -and $headers[-1].text -ceq $baselineRecord.cohort.lastHeader -and
                $headers[-1].characterOffset -eq $baselineRecord.cohort.lastHeaderCharacterOffset)
            $cohort=$baselineRecord.cohort;$events=@(ConvertFrom-LirCleoEvents $delta)
            $eventsPin=Write-LirJson $output 'events.json' ([ordered]@{events=$events;sourceCommit=$ready.sourceCommit;pid=$processBefore.pid;freshBaselineDeltaOnly=$true})
            $verifierStdout=Join-Path $output 'verifier-stdout.json';$verifierStderr=Join-Path $output 'verifier-stderr.txt'
            # Exit1 is a preserved negative verifier result, not a reason to
            # discard its independent native-cleanup classification.
            $PSNativeCommandUseErrorActionPreference=$false
            & $NodeExecutable $ready.verifier.path $eventsPin.path $ready.validatorSource.sha256 1> $verifierStdout 2> $verifierStderr
            $verifierExit=$LASTEXITCODE
            $verifierOutput=Get-LirPin $verifierStdout;$verifierResult=ReadJson $verifierStdout
            Check 'verifier-result-shape-and-pinned-validator' ($verifierExit -in @(0,1) -and $verifierResult.protocolPassed -is [bool] -and
                $verifierResult.cleanupPassed -is [bool] -and $verifierResult.validatorSource.sha256 -ceq $ready.validatorSource.sha256)
            $protocolPassed=$verifierResult.protocolPassed
            $finishes=@($events|Where-Object{$_.PSObject.Properties['event'] -and $_.event -ceq 'frontend_runtime_finished'})
            if($finishes.Count -eq 1 -and @($finishes[0].values).Count -eq 416){$moduleCleanupPassed=CheckModuleCleanup $modulesBefore $finishes[0].values}
            $cleanupPassed=$verifierResult.cleanupPassed -and $moduleCleanupPassed
        }
        $processAfter=RealProcess;$modulesAfter=@(TrialModules $processAfter)
        Check 'process-identity-stable-through-capture' ($processAfter.pid -eq $processBefore.pid -and $processAfter.startTimeUtc -ceq $processBefore.startTimeUtc -and $processAfter.path -ceq $processBefore.path)
        Check 'trial-module-state-stable-through-capture' ((ConvertTo-Json $modulesBefore -Compress) -ceq (ConvertTo-Json $modulesAfter -Compress))
    }
    foreach($pin in @($readyPin)+@($observedProtected)+@($observedSources)+@($observedFiles|ForEach-Object{$_.observed})){$null=Assert-LirPin ([pscustomobject]$pin)}
    $currentHead=(& git -C $repositoryRoot rev-parse HEAD 2>$null)
    $captureComplete=$true
} catch { $failure=$_.Exception.Message }
$protocolPassed=$captureComplete -and $protocolPassed
$cleanupPassed=$captureComplete -and $cleanupPassed
$moduleCleanupPassed=$captureComplete -and $moduleCleanupPassed
$success=$captureComplete -and $(if($Mode -eq 'Actual'){$protocolPassed -and $cleanupPassed}elseif($Mode -eq 'Shutdown'){$normalExitEvidenceVerified}else{$true})
$result=[ordered]@{
    schema=('mta_local_input_runtime_'+$Mode.ToLowerInvariant()+'_v1');recordedAt=[DateTimeOffset]::Now.ToString('o');mode=$Mode;success=$success;
    captureComplete=$captureComplete;rawCaptureComplete=$rawCaptureComplete;failure=$failure;directory=$output;
    sourceCommit=$(if($ready){$ready.sourceCommit}else{$null});currentHead=$currentHead;runtimeSourcePinsRatherThanHeadEquality=$true;
    installedReady=$readyPin;savedInstalledReady=$savedReady;baseline=$baselinePin;actual=$actualPin;logs=$logs.ToArray();logContinuities=$continuities;
    observedOwned=$observedFiles;observedSources=$observedSources;observedProtected=$observedProtected;savedManifests=$savedManifests;
    processBefore=$processBefore;processAfter=$processAfter;modulesBefore=$modulesBefore;modulesAfter=$modulesAfter;cohort=$cohort;
    allSixTrialDllsAbsent=$(if($Mode -eq 'Shutdown'){$normalExitEvidenceVerified}elseif($processBefore -and $processAfter){$modulesBefore.Count -eq 0 -and $modulesAfter.Count -eq 0}else{$null});
    protocolPassed=$protocolPassed;cleanupPassed=$cleanupPassed;moduleCleanupPassed=$moduleCleanupPassed;verifierExit=$verifierExit;events=$eventsPin;verifierOutput=$verifierOutput;verifierResult=$verifierResult;
    normalExitEvidenceVerified=$normalExitEvidenceVerified;cleoShutdownEvidence=$cleoShutdownEvidence;machineExitCodeKnown=$false;machineExitCode=$null;
    priorActualProtocolPassed=$(if($actualRecord){$actualRecord.protocolPassed}else{$null});priorActualCleanupPassed=$(if($actualRecord){$actualRecord.cleanupPassed}else{$null});
    priorActualSuccess=$(if($actualRecord){$actualRecord.success}else{$null});priorActualHumanReport=$(if($actualRecord){$actualRecord.humanReport}else{$null});
    shutdownDoesNotPromoteActualResult=$true;humanReport=$HumanReport;humanNormalExitReported=[bool]$HumanNormalExitReported;humanInteractionInferred=$false;combinedResultPassed=$null;
    checks=$checks.ToArray();nativeOrVisualPassInferred=$false;fullMtaReady=$false;networkActive=$false;gameplayReady=$false;
    GTAControlled=$false;DLLLoadedByTool=$false;installedFilesChanged=$false;historicalAliasesPromoted=$false
}
$resultPin=Write-LirJson $output ($Mode.ToLowerInvariant()+'.json') $result
[ordered]@{success=$success;captureComplete=$captureComplete;rawCaptureComplete=$rawCaptureComplete;result=$resultPin;protocolPassed=$protocolPassed;cleanupPassed=$cleanupPassed;failure=$failure}|ConvertTo-Json -Depth 4 -Compress
if(-not $success){exit 1}
