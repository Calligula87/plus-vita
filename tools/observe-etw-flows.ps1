[CmdletBinding()]
param(
    [ValidateRange(60, 3600)]
    [int]$DurationSeconds = 900,

    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\captures-private'),

    [ValidateRange(1, 65535)]
    [int]$CounterPort = 2053,

    [ValidateRange(1, 60)]
    [int]$CounterIntervalSeconds = 5
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
$errorPath = Join-Path $outputRoot 'etw-flow-observer-error-private.txt'
trap {
    ($_ | Out-String) | Set-Content -LiteralPath $errorPath -Encoding utf8
    break
}

$sessionName = 'PlusVitaFlowTrace'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$etlPath = Join-Path $outputRoot "etw-flows-$stamp.etl"
$metadataPath = Join-Path $outputRoot "session-metadata-$stamp.jsonl"
$statusPath = Join-Path $outputRoot 'etw-flow-observer-status.txt'
$stopPath = Join-Path $outputRoot 'stop-etw-flow-observer'
$provider = '{2F07E2EE-15DB-40F1-90EF-9D7BA282188A}'

# Endpoint/listener/TCB/bind/process-hint/endpoint/ALE remote endpoint,
# connect/close path, PII-present (addresses), and address events. The TCP/IP
# packet keyword (0x0000040000000000) is deliberately absent.
$keywords = '0x00000A140000860F'
$knownNames = @('agl', 'pspluslauncher', 'QtWebEngineProcess', 'unidater')

function Write-Metadata {
    param([string]$Event, [object]$Data)

    [ordered]@{
        timestampUtc = [DateTime]::UtcNow.ToString('o')
        event = $Event
        data = $Data
    } | ConvertTo-Json -Compress -Depth 8 |
        Add-Content -LiteralPath $metadataPath -Encoding utf8
}

function Get-ClientProcesses {
    $result = @{}
    foreach ($process in Get-Process -ErrorAction SilentlyContinue) {
        $path = $null
        try { $path = $process.Path } catch {}
        if ($knownNames -notcontains $process.ProcessName -and
            (-not $path -or $path -notlike '*\PlayStationPlus\*')) {
            continue
        }

        $result[[string]$process.Id] = [ordered]@{
            pid = $process.Id
            name = $process.ProcessName
            executable = if ($path) { [IO.Path]::GetFileName($path) } else { $null }
        }
    }
    return $result
}

function Write-ProcessChanges {
    param([hashtable]$Previous, [hashtable]$Current)
    foreach ($key in $Current.Keys) {
        if (-not $Previous.ContainsKey($key)) {
            Write-Metadata 'process-started' $Current[$key]
        }
    }
    foreach ($key in $Previous.Keys) {
        if (-not $Current.ContainsKey($key)) {
            Write-Metadata 'process-stopped' $Previous[$key]
        }
    }
}

function Write-CounterSnapshot {
    $groups = @((& pktmon counters --json) | ConvertFrom-Json)
    foreach ($group in $groups) {
        foreach ($component in $group.Components) {
            foreach ($counter in $component.Counters) {
                Write-Metadata 'udp-counter' @{
                    port = $CounterPort
                    group = $group.Group
                    component = $component.Name
                    edge = $counter.Name
                    rxPackets = [int64]$counter.Inbound.Packets
                    rxBytes = [int64]$counter.Inbound.Bytes
                    txPackets = [int64]$counter.Outbound.Packets
                    txBytes = [int64]$counter.Outbound.Bytes
                }
            }
        }
    }
}

if (Test-Path -LiteralPath $stopPath) {
    Remove-Item -LiteralPath $stopPath -Force
}

$ErrorActionPreference = 'Continue'
$existing = (& logman query $sessionName 2>&1 | Out-String)
$existingExitCode = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($existingExitCode -eq 0) {
    throw "ETW session '$sessionName' already exists; refusing to replace it."
}

$pktmonStatus = (& pktmon status 2>&1 | Out-String)
if ($pktmonStatus -match '(?im)^Packet Monitor is running') {
    throw 'PktMon is already running; refusing to replace it.'
}
$ErrorActionPreference = 'Continue'
$existingFilters = (& pktmon filter list 2>&1 | Out-String).Trim()
$ErrorActionPreference = 'Stop'
if ($existingFilters -and $existingFilters -notmatch '(?i)(no filters|packet filters:\s*none)') {
    throw "PktMon already has filters configured; refusing to alter them.`n$existingFilters"
}

$started = $false
$pktmonStarted = $false
try {
    & logman create trace $sessionName -o $etlPath -p $provider $keywords 5 `
        -f bincirc -max 128 -bs 64 -ets | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to create ETW session (exit code $LASTEXITCODE)."
    }
    $started = $true

    & pktmon filter add PlusVitaSessionCounters -d IPv4 -t UDP -p $CounterPort | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to add the UDP counter filter (exit code $LASTEXITCODE)."
    }
    & pktmon start --capture --counters-only --comp nics | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to start UDP counters (exit code $LASTEXITCODE)."
    }
    $pktmonStarted = $true

    @(
        'READY'
        "startedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "etl=$etlPath"
        "provider=$provider"
        "keywords=$keywords"
        "metadata=$metadataPath"
        "counterPort=$CounterPort"
        'packetKeyword=false'
        'packetCapture=false'
        'counterLoggingOnly=true'
    ) | Set-Content -LiteralPath $statusPath -Encoding utf8

    Write-Metadata 'observation-started' @{
        counterPort = $CounterPort
        counterIntervalSeconds = $CounterIntervalSeconds
        packetCapture = $false
    }
    $previousProcesses = @{}
    $nextCounterAt = [DateTime]::UtcNow
    $deadline = [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    while ([DateTime]::UtcNow -lt $deadline -and -not (Test-Path -LiteralPath $stopPath)) {
        $currentProcesses = Get-ClientProcesses
        Write-ProcessChanges -Previous $previousProcesses -Current $currentProcesses
        $previousProcesses = $currentProcesses

        if ([DateTime]::UtcNow -ge $nextCounterAt) {
            Write-CounterSnapshot
            $nextCounterAt = [DateTime]::UtcNow.AddSeconds($CounterIntervalSeconds)
        }
        Start-Sleep -Seconds 1
    }

    Write-Metadata 'observation-stopped' @{}
}
finally {
    if ($pktmonStarted) {
        & pktmon stop | Out-Null
    }
    & pktmon filter remove | Out-Null
    if ($started) {
        & logman stop $sessionName -ets | Out-Null
        & logman delete $sessionName | Out-Null
    }

    @(
        'STOPPED'
        "stoppedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "etl=$etlPath"
    ) | Set-Content -LiteralPath $statusPath -Encoding utf8
}
