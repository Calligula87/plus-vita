[CmdletBinding()]
param(
    [ValidateRange(60, 3600)]
    [int]$DurationSeconds = 900,

    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\captures-private')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This observer must be run from an Administrator PowerShell window.'
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$errorPath = Join-Path $outputRoot 'udp-observer-error-private.txt'
trap {
    ($_ | Out-String) | Set-Content -LiteralPath $errorPath -Encoding utf8
    break
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$etlPath = Join-Path $outputRoot "udp-headers-$stamp.etl"
$statusPath = Join-Path $outputRoot 'udp-observer-status.txt'
$stopPath = Join-Path $outputRoot 'stop-udp-observer'

if (Test-Path -LiteralPath $stopPath) {
    Remove-Item -LiteralPath $stopPath -Force
}

$pktmonStatus = (& pktmon status 2>&1 | Out-String)
$pktmonStatus | Set-Content -LiteralPath (Join-Path $outputRoot 'pktmon-status-private.txt') -Encoding utf8
if ($pktmonStatus -match '(?im)^Packet Monitor is running') {
    throw 'PktMon is already running. Stop the existing collection before continuing.'
}

$ErrorActionPreference = 'Continue'
$existingFilters = (& pktmon filter list 2>&1 | Out-String).Trim()
$ErrorActionPreference = 'Stop'
$existingFilters | Set-Content -LiteralPath (Join-Path $outputRoot 'pktmon-filters-private.txt') -Encoding utf8
if ($existingFilters -and $existingFilters -notmatch '(?i)(no filters|packet filters:\s*none)') {
    throw "PktMon already has filters configured; refusing to alter them.`n$existingFilters"
}

try {
    & pktmon filter add PlusVitaUdpHeaders -d IPv4 -t UDP | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to add the PktMon UDP filter (exit code $LASTEXITCODE)."
    }

    # Ethernet (14) + ordinary IPv4 (20) + UDP (8) = 42 bytes. This retains
    # routing headers while truncating before the application payload.
    & pktmon start --capture --comp nics --type flow --pkt-size 42 --flags 0x012 `
        --file-name $etlPath --file-size 128 --log-mode circular | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to start PktMon (exit code $LASTEXITCODE)."
    }

    @(
        'READY'
        "startedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "etl=$etlPath"
        'filter=IPv4 UDP'
        'loggedBytesPerPacket=42'
        'applicationPayload=false'
    ) | Set-Content -LiteralPath $statusPath -Encoding utf8

    $deadline = [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    while ([DateTime]::UtcNow -lt $deadline -and -not (Test-Path -LiteralPath $stopPath)) {
        Start-Sleep -Seconds 1
    }
}
finally {
    & pktmon stop | Out-Null
    & pktmon filter remove | Out-Null
    @(
        'STOPPED'
        "stoppedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "etl=$etlPath"
    ) | Set-Content -LiteralPath $statusPath -Encoding utf8
}
