# This file is part of Multi Theft Auto. See LICENSE in the repository root.
# Share the native entry's complete ownership/configuration contract across
# staging, installation, updates and mode changes; partial startup must refuse.
function Get-MtaNativeFrontendFileCount([string[]]$Paths, [int]$FrontendCount) {
    $count = @($Paths | Where-Object { $_ -in @('MtaDefinitive.asi','MtaDefinitive.ini') }).Count
    if ($count -notin @(0,2) -or ($count -and $FrontendCount -ne 22)) { throw 'Incomplete native MTA frontend startup ownership.' }
    return $count
}

function New-MtaNativeFrontendIni([bool]$Enabled, [bool]$BindingPersistence) {
    # One owned file records the explicit native entry choice. It is separate
    # from MTA's user binding data and can be disarmed without deleting assets.
    return "[MTA]`nEnabled=$([int]$Enabled)`nLocalInput=1`nBindingPersistence=$([int]$BindingPersistence)`n"
}

function Assert-MtaNativeFrontendConfig([string]$Config, [int]$NativeCount, [string]$Ini, [string]$Mode) {
    $pattern = 'export const MTA_CORE_FRONTEND_NATIVE_HOST = (true|false);'
    $markers = [regex]::Matches($Config, $pattern)
    if ($markers.Count -gt 1 -or [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_NATIVE_HOST\b').Count -ne $markers.Count) {
        throw 'Invalid native MTA frontend configuration marker.'
    }
    $enabled = $markers.Count -eq 1 -and $markers[0].Groups[1].Value -eq 'true'
    if ($enabled -and ($NativeCount -ne 2 -or $Mode -ne 'Smoke' -or
        [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_RUNTIME = true;').Count -ne 1)) {
        throw 'Native MTA startup requires its complete entry group and retained frontend Smoke runtime.'
    }
    if ($NativeCount) {
        $persistent = [regex]::Matches($Config, 'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = true;').Count -eq 1
        if ($markers.Count -ne 1 -or $Ini.Replace("`r`n","`n") -cne (New-MtaNativeFrontendIni $enabled $persistent)) {
            throw 'Native MTA startup INI does not match its owned script configuration.'
        }
    }
}
