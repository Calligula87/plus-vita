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
$errorPath = Join-Path $outputRoot 'etw-flow-observer-error-private.txt'
trap {
    ($_ | Out-String) | Set-Content -LiteralPath $errorPath -Encoding utf8
    break
}

$sessionName = 'PlusVitaFlowTrace'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$etlPath = Join-Path $outputRoot "etw-flows-$stamp.etl"
$statusPath = Join-Path $outputRoot 'etw-flow-observer-status.txt'
$stopPath = Join-Path $outputRoot 'stop-etw-flow-observer'
$provider = '{2F07E2EE-15DB-40F1-90EF-9D7BA282188A}'

# Endpoint/listener/TCB/bind/process-hint/endpoint/ALE remote endpoint,
# connect/close path, PII-present (addresses), and address events. The TCP/IP
# packet keyword (0x0000040000000000) is deliberately absent.
$keywords = '0x00000A140000860F'

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

$started = $false
try {
    & logman create trace $sessionName -o $etlPath -p $provider $keywords 5 `
        -f bincirc -max 128 -bs 64 -ets | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to create ETW session (exit code $LASTEXITCODE)."
    }
    $started = $true

    @(
        'READY'
        "startedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "etl=$etlPath"
        "provider=$provider"
        "keywords=$keywords"
        'packetKeyword=false'
        'packetCapture=false'
    ) | Set-Content -LiteralPath $statusPath -Encoding utf8

    $deadline = [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    while ([DateTime]::UtcNow -lt $deadline -and -not (Test-Path -LiteralPath $stopPath)) {
        Start-Sleep -Seconds 1
    }
}
finally {
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

