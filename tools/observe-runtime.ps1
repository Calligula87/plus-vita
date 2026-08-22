[CmdletBinding()]
param(
    [ValidateRange(1, 3600)]
    [int]$IntervalSeconds = 2,

    [ValidateRange(0, 86400)]
    [int]$DurationSeconds = 0,

    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\captures-private')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$installRoot = 'C:\Program Files (x86)\PlayStationPlus'
$knownNames = @(
    'agl',
    'pspluslauncher',
    'QtWebEngineProcess',
    'unidater'
)

function Write-Observation {
    param(
        [Parameter(Mandatory)]
        [string]$Event,

        [Parameter(Mandatory)]
        [hashtable]$Data
    )

    $record = [ordered]@{
        timestampUtc = [DateTime]::UtcNow.ToString('o')
        event        = $Event
        data         = $Data
    }

    $record | ConvertTo-Json -Compress -Depth 5 | Add-Content -LiteralPath $script:outputPath -Encoding utf8
}

function Get-TargetProcesses {
    $results = @{}

    foreach ($process in Get-Process -ErrorAction SilentlyContinue) {
        $path = $null
        try {
            $path = $process.Path
        }
        catch {
            # Some protected system processes do not expose their paths.
        }

        $isKnownName = $knownNames -contains $process.ProcessName
        $isInstalledClient = $path -and $path.StartsWith($installRoot, [StringComparison]::OrdinalIgnoreCase)

        if ($isKnownName -or $isInstalledClient) {
            $startTimeUtc = $null
            try {
                $startTimeUtc = $process.StartTime.ToUniversalTime().ToString('o')
            }
            catch {
                # Start time can disappear if a process exits during sampling.
            }

            $results[[string]$process.Id] = [ordered]@{
                pid          = $process.Id
                name         = $process.ProcessName
                executable   = if ($path) { [IO.Path]::GetFileName($path) } else { $null }
                startTimeUtc = $startTimeUtc
            }
        }
    }

    return $results
}

function Get-TargetConnections {
    param(
        [Parameter(Mandatory)]
        [hashtable]$Processes
    )

    $results = @{}
    $processIds = @($Processes.Keys | ForEach-Object { [int]$_ })
    if ($processIds.Count -eq 0) {
        return $results
    }

    try {
        $tcpConnections = @(Get-NetTCPConnection -ErrorAction Stop)
        $udpEndpoints = @(Get-NetUDPEndpoint -ErrorAction Stop)
    }
    catch {
        return Get-NetstatTargetConnections -Processes $Processes
    }

    foreach ($connection in $tcpConnections) {
        if ($connection.OwningProcess -notin $processIds) {
            continue
        }

        $key = 'tcp|{0}|{1}|{2}|{3}|{4}' -f $connection.OwningProcess,
            $connection.LocalAddress, $connection.LocalPort,
            $connection.RemoteAddress, $connection.RemotePort

        $results[$key] = [ordered]@{
            protocol      = 'tcp'
            pid           = $connection.OwningProcess
            process       = $Processes[[string]$connection.OwningProcess].name
            state         = [string]$connection.State
            localAddress  = $connection.LocalAddress
            localPort     = $connection.LocalPort
            remoteAddress = $connection.RemoteAddress
            remotePort    = $connection.RemotePort
        }
    }

    foreach ($endpoint in $udpEndpoints) {
        if ($endpoint.OwningProcess -notin $processIds) {
            continue
        }

        $key = 'udp|{0}|{1}|{2}' -f $endpoint.OwningProcess,
            $endpoint.LocalAddress, $endpoint.LocalPort

        $results[$key] = [ordered]@{
            protocol     = 'udp'
            pid          = $endpoint.OwningProcess
            process      = $Processes[[string]$endpoint.OwningProcess].name
            localAddress = $endpoint.LocalAddress
            localPort    = $endpoint.LocalPort
        }
    }

    return $results
}

function Get-NetstatTargetConnections {
    param(
        [Parameter(Mandatory)]
        [hashtable]$Processes
    )

    $results = @{}
    foreach ($line in & "$env:SystemRoot\System32\netstat.exe" -ano -p tcp) {
        $fields = @($line.Trim() -split '\s+')
        if ($fields.Count -ne 5 -or $fields[0] -ne 'TCP') {
            continue
        }

        $processId = 0
        if (-not [int]::TryParse($fields[4], [ref]$processId) -or
            -not $Processes.ContainsKey([string]$processId)) {
            continue
        }

        $localAddress, $localPort = Split-NetstatEndpoint $fields[1]
        $remoteAddress, $remotePort = Split-NetstatEndpoint $fields[2]
        $key = 'tcp|{0}|{1}|{2}|{3}|{4}' -f $processId,
            $localAddress, $localPort, $remoteAddress, $remotePort

        $results[$key] = [ordered]@{
            protocol      = 'tcp'
            pid           = $processId
            process       = $Processes[[string]$processId].name
            state         = $fields[3].ToLowerInvariant()
            localAddress  = $localAddress
            localPort     = $localPort
            remoteAddress = $remoteAddress
            remotePort    = $remotePort
            source        = 'netstat'
        }
    }

    foreach ($line in & "$env:SystemRoot\System32\netstat.exe" -ano -p udp) {
        $fields = @($line.Trim() -split '\s+')
        if ($fields.Count -ne 4 -or $fields[0] -ne 'UDP') {
            continue
        }

        $processId = 0
        if (-not [int]::TryParse($fields[3], [ref]$processId) -or
            -not $Processes.ContainsKey([string]$processId)) {
            continue
        }

        $localAddress, $localPort = Split-NetstatEndpoint $fields[1]
        $key = 'udp|{0}|{1}|{2}' -f $processId, $localAddress, $localPort

        $results[$key] = [ordered]@{
            protocol     = 'udp'
            pid          = $processId
            process      = $Processes[[string]$processId].name
            localAddress = $localAddress
            localPort    = $localPort
            source       = 'netstat'
        }
    }

    return $results
}

function Split-NetstatEndpoint {
    param([Parameter(Mandatory)][string]$Endpoint)

    $separator = $Endpoint.LastIndexOf(':')
    if ($separator -lt 0) {
        return @($Endpoint, $null)
    }

    $address = $Endpoint.Substring(0, $separator).Trim('[', ']')
    $portText = $Endpoint.Substring($separator + 1)
    $port = 0
    if (-not [int]::TryParse($portText, [ref]$port)) {
        $port = $null
    }

    return @($address, $port)
}

function Write-Changes {
    param(
        [Parameter(Mandatory)]
        [hashtable]$Previous,

        [Parameter(Mandatory)]
        [hashtable]$Current,

        [Parameter(Mandatory)]
        [string]$StartedEvent,

        [Parameter(Mandatory)]
        [string]$StoppedEvent
    )

    foreach ($key in $Current.Keys) {
        if (-not $Previous.ContainsKey($key)) {
            Write-Observation -Event $StartedEvent -Data $Current[$key]
        }
    }

    foreach ($key in $Previous.Keys) {
        if (-not $Current.ContainsKey($key)) {
            Write-Observation -Event $StoppedEvent -Data $Previous[$key]
        }
    }
}

if (-not (Test-Path -LiteralPath $installRoot)) {
    throw "PlayStation Plus installation was not found at $installRoot"
}

$resolvedOutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $resolvedOutputDirectory -Force | Out-Null
$fileName = 'runtime-{0}.jsonl' -f (Get-Date -Format 'yyyyMMdd-HHmmss')
$script:outputPath = Join-Path $resolvedOutputDirectory $fileName

Write-Observation -Event 'observation-started' -Data @{
    intervalSeconds = $IntervalSeconds
    durationSeconds = $DurationSeconds
    payloadCapture  = $false
    commandLines    = $false
}

Write-Host "Recording metadata to: $script:outputPath"
Write-Host 'Start PlayStation Plus normally. Press Ctrl+C after clean shutdown.'

$previousProcesses = @{}
$previousConnections = @{}
$startedAt = [DateTime]::UtcNow

try {
    while ($true) {
        $currentProcesses = Get-TargetProcesses
        $currentConnections = Get-TargetConnections -Processes $currentProcesses

        Write-Changes -Previous $previousProcesses -Current $currentProcesses `
            -StartedEvent 'process-started' -StoppedEvent 'process-stopped'
        Write-Changes -Previous $previousConnections -Current $currentConnections `
            -StartedEvent 'connection-opened' -StoppedEvent 'connection-closed'

        $previousProcesses = $currentProcesses
        $previousConnections = $currentConnections

        if ($DurationSeconds -gt 0 -and
            ([DateTime]::UtcNow - $startedAt).TotalSeconds -ge $DurationSeconds) {
            break
        }

        Start-Sleep -Seconds $IntervalSeconds
    }
}
finally {
    Write-Observation -Event 'observation-stopped' -Data @{}
    Write-Host "Observation stopped. Private output remains at: $script:outputPath"
}
