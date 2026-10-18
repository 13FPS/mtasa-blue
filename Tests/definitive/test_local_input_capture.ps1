#requires -Version 7.5
# Offline evidence operations only. All inputs below are synthetic bytes under
# this workspace; this driver never enumerates or interacts with GTA processes.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'LocalInputRuntimeEvidence.psm1') -Force -DisableNameChecking
$repositoryRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$directory=Join-Path $repositoryRoot ('Build/local-input-capture-tests/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
# Keep replayable receipts separate from disposable inputs so workspace cleanup
# never removes the result needed to diagnose a failed capture operation.
$scratchRoot=Join-Path $repositoryRoot ('Build/tmp/local-input-capture-'+[IO.Path]::GetFileName($directory))
New-Item -ItemType Directory -Path $scratchRoot | Out-Null
$checks=0;$failure=$null
$sourceNames=@('Tests/definitive/LocalInputRuntimeEvidence.psm1','Tests/definitive/capture_local_input_runtime.ps1','Tests/definitive/test_local_input_capture.ps1')
$sourceHashes=@($sourceNames|ForEach-Object{Get-LirPin (Join-Path $repositoryRoot $_)})
function Check([bool]$Condition,[string]$Why){++$script:checks;if(-not $Condition){throw "Offline capture test failed: $Why"}}
function Throws([scriptblock]$Action,[string]$Why){$threw=$false;try{& $Action|Out-Null}catch{$threw=$true};Check $threw $Why}
function Bytes([string]$Text){,[Text.UTF8Encoding]::new($false).GetBytes($Text)}
function Wrapped($Event){'12:00:01 [INFO] ["de-online/index.js"] '+(($Event|ConvertTo-Json -Compress)|ConvertTo-Json -Compress)+"`r`n"}
try {
    $sourceRoot=Join-Path $scratchRoot 'synthetic-inputs';New-Item -ItemType Directory -Path $sourceRoot | Out-Null
    $rawPath=Join-Path $sourceRoot 'binary.log'
    $rawBytes=[byte[]]@(0,255,13,10,1,2,13,10)
    [IO.File]::WriteAllBytes($rawPath,$rawBytes)
    $raw=Save-LirRawLog $rawPath $directory 'saved/raw.log'
    Check ($raw.captureSucceeded -and -not $raw.absent -and -not $raw.captureError -and $raw.newlineBoundary) 'raw binary capture succeeds only after its saved write'
    Check ($raw.saved.bytes -eq $rawBytes.Length -and $raw.wholeFileLengthAtReadStart -eq $rawBytes.Length -and $raw.copyIsRawObservedPrefix) 'raw capture describes its observed whole prefix'
    Check ($raw.saved.sha256 -ceq (Get-LirPin $rawPath).sha256) 'raw copy preserves bytes without decoding or newline rewriting'
    $overCap=Save-LirRawLog $rawPath $directory 'saved/over-cap.log' 3
    Check (-not $overCap.captureSucceeded -and $null -eq $overCap.saved -and $overCap.captureError) 'bounded read refusal cannot publish a raw success'
    Check (-not (Test-Path -LiteralPath (Join-Path $directory 'saved/over-cap.log'))) 'over-cap bytes were never written'
    $absent=Save-LirRawLog (Join-Path $sourceRoot 'absent.log') $directory 'saved/absent.log'
    Check ($absent.absent -and -not $absent.captureSucceeded -and $absent.captureError) 'missing required logs remain explicit capture failures'
    $collision=Save-LirRawLog $rawPath $directory 'saved/raw.log'
    Check (-not $collision.captureSucceeded -and $null -eq $collision.saved -and $collision.captureError) 'a failed CreateNew write never claims successful raw capture'
    Check ((Get-LirPin $raw.saved.path).sha256 -ceq $raw.saved.sha256) 'capture collision cannot overwrite earlier evidence'
    Throws { Write-LirBytes $directory '../escape.log' $rawBytes } 'new output cannot escape its exact evidence directory'
    Check (-not (Test-Path -LiteralPath (Join-Path (Split-Path $directory) 'escape.log'))) 'refused output has no side effect outside its directory'

    $prefixPath=Join-Path $sourceRoot 'prefix.log';[IO.File]::WriteAllBytes($prefixPath,(Bytes "before`r`n"));$prefix=Get-LirPin $prefixPath
    $current=Join-Path $sourceRoot 'current.log';[IO.File]::WriteAllBytes($current,(Bytes "before`r`nafter`r`n"))
    $proof=Test-LirPrefix $current ([pscustomobject]$prefix)
    Check ($proof.preserved -and $proof.deltaBytes -eq 7) 'prefix comparison accepts a genuine byte append'
    [IO.File]::WriteAllBytes($current,(Bytes "reset!`r`n"))
    Check (-not (Test-LirPrefix $current ([pscustomobject]$prefix)).preserved) 'equal-length replacement is not called a prefix'
    [IO.File]::WriteAllBytes($current,(Bytes "tiny"))
    Check (-not (Test-LirPrefix $current ([pscustomobject]$prefix)).preserved) 'short replacement is not called a prefix'
    [IO.File]::WriteAllBytes($prefixPath,(Bytes "tamper`r`n"))
    Throws { Test-LirPrefix $current ([pscustomobject]$prefix) } 'changed saved baseline bytes cannot authorize a prefix proof'
    Throws { Copy-LirPinnedMetadata $directory 'saved/changed-metadata.json' ([pscustomobject]$prefix) } 'metadata copying verifies its original pin'

    $first=Wrapped ([ordered]@{event='adapter_armed';mode='smoke';activate_key='Ctrl+Shift+F9'})
    $second=Wrapped ([ordered]@{event='frontend_runtime_started';schema='mta_frontend_runtime_v1';generation=7})
    $header="12:00:00 [INFO] CLEO Redux synthetic offline fixture`r`n"
    $cleoBaseline=Join-Path $sourceRoot 'cleo-baseline.log';[IO.File]::WriteAllBytes($cleoBaseline,(Bytes ($header+$first)))
    $cleoPin=Get-LirPin $cleoBaseline
    $cleoCurrent=Join-Path $sourceRoot 'cleo-current.log';[IO.File]::WriteAllBytes($cleoCurrent,(Bytes ($header+$first+$second)))
    $events=@(ConvertFrom-LirCleoEvents ($header+$first+$second+"unrelated plain diagnostic`r`n"))
    Check ($events.Count -eq 2 -and $events[0].event -ceq 'adapter_armed' -and $events[1].event -ceq 'frontend_runtime_started') 'wrapped CLEO events are decoded exactly once'
    $delta=Get-LirCleoDelta $cleoCurrent ([pscustomobject]$cleoPin)
    $fresh=@(ConvertFrom-LirCleoEvents $delta)
    Check ($fresh.Count -eq 1 -and $fresh[0].event -ceq 'frontend_runtime_started') 'actual verifier input excludes all baseline events'
    $headers=@(Get-LirCleoHeaders ($header+$first+$header+$second))
    Check ($headers.Count -eq 2 -and $headers[0].characterOffset -eq 0 -and $headers[1].characterOffset -eq ($header+$first).Length) 'startup cohort headers retain exact character offsets'
    [IO.File]::WriteAllBytes($cleoCurrent,(Bytes ($header+$first+$second.TrimEnd("`r","`n"))))
    Throws { Get-LirCleoDelta $cleoCurrent ([pscustomobject]$cleoPin) } 'partial terminal CLEO lines cannot enter the verifier'
    Throws { ConvertFrom-LirCleoEvents '12:00:01 [INFO] ["fixture"] "{broken}"' } 'malformed wrapped JSON is refused instead of silently omitted'
    $empty=Join-Path $sourceRoot 'empty.log';[IO.File]::WriteAllBytes($empty,[byte[]]::new(0));$emptyPin=Get-LirPin $empty
    Check ((Test-LirPrefix $cleoBaseline ([pscustomobject]$emptyPin)).preserved) 'zero-length prefix semantics are explicit'

    $fakeRepository=Join-Path $scratchRoot 'synthetic-workspace';New-Item -ItemType Directory -Path $fakeRepository | Out-Null
    $freshOutput=New-LirOutputDirectory $fakeRepository 'Actual'
    Check ($freshOutput.StartsWith((Join-Path $fakeRepository 'Build/local-input-runtime-evidence')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'new output stays in the supplied workspace Build directory'
    $json=Write-LirJson $freshOutput 'offline-result.json' ([ordered]@{synthetic=$true;protocolPassed=$false;cleanupPassed=$true})
    $parsed=Get-Content -LiteralPath $json.path -Raw|ConvertFrom-Json
    Check ($parsed.synthetic -and -not $parsed.protocolPassed -and $parsed.cleanupPassed) 'workload failure and native cleanup remain separate values'
    Throws { Write-LirJson $freshOutput 'offline-result.json' ([ordered]@{synthetic=$false}) } 'result writer refuses overwrite'
    foreach($pin in $sourceHashes){$null=Assert-LirPin ([pscustomobject]$pin)}
} catch { $failure=$_.Exception.Message }
$record=[ordered]@{schema='mta_local_input_capture_offline_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');success=($null -eq $failure);checks=$checks;failure=$failure;
    directory=$directory;scratchDirectory=$scratchRoot;sourceHashes=$sourceHashes;syntheticFixturesOnly=$true;gameLaunched=$false;gameProcessEnumerated=$false;gameInput=$false;installedFilesChanged=$false;
    scope='Raw bounded read/write, failed capture truthfulness, prefix replacement/refusal, wrapped CLEO parsing, fresh delta boundaries and output containment only. No game or machine lifecycle pass.'}
$pin=Write-LirJson $directory 'test-result.json' $record
[ordered]@{success=$record.success;checks=$checks;result=$pin;failure=$failure}|ConvertTo-Json -Depth 4 -Compress
if(-not $record.success){exit 1}
