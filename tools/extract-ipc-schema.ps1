[CmdletBinding()]
param(
    [string]$InstallRoot = 'C:\Program Files (x86)\PlayStationPlus',

    [string]$OutputPath = (Join-Path $PSScriptRoot '..\research\ipc-static-schema.json')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sourceNames = @('preload.js', 'preloadNotifier.js', 'websocket_client.js')
$commandNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$eventNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$parameterNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$sensitiveNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$sources = @()

$sensitiveFieldNames = @(
    'authCode', 'authToken', 'cloudAuthCode', 'encryptedKey', 'ipcToken',
    'sessionId', 'sessionKey', 'token'
)

$commandPattern = 'command\s*:\s*[''"](?<name>[A-Za-z][A-Za-z0-9_-]*)[''"]'
$qasCommandPattern = 'sendQASCommand\s*\(\s*[''"](?<name>[A-Za-z][A-Za-z0-9_-]*)[''"]'
$eventPattern = '(?:\.on|\.emit)\s*\(\s*[''"](?<name>[A-Za-z][A-Za-z0-9_:-]*)[''"]'
$paramsPattern = 'params\s*:\s*\{(?<body>[^{}]*)\}'
$keyPattern = '(?:[''"])?(?<name>[A-Za-z][A-Za-z0-9_]*)(?:[''"])?\s*:'

foreach ($sourceName in $sourceNames) {
    $path = Join-Path $InstallRoot $sourceName
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required installed source was not found: $path"
    }

    $text = Get-Content -LiteralPath $path -Raw
    $hash = Get-FileHash -LiteralPath $path -Algorithm SHA256
    $sources += [ordered]@{
        file = $sourceName
        sha256 = $hash.Hash
        bytes = (Get-Item -LiteralPath $path).Length
    }

    foreach ($match in [regex]::Matches($text, $commandPattern)) {
        $null = $commandNames.Add($match.Groups['name'].Value)
    }
    foreach ($match in [regex]::Matches($text, $qasCommandPattern)) {
        $null = $commandNames.Add($match.Groups['name'].Value)
    }
    foreach ($match in [regex]::Matches($text, $eventPattern)) {
        $null = $eventNames.Add($match.Groups['name'].Value)
    }
    foreach ($paramsMatch in [regex]::Matches($text, $paramsPattern)) {
        foreach ($keyMatch in [regex]::Matches($paramsMatch.Groups['body'].Value, $keyPattern)) {
            $null = $parameterNames.Add($keyMatch.Groups['name'].Value)
        }
    }
    foreach ($sensitiveName in $sensitiveFieldNames) {
        if ([regex]::IsMatch($text, "\b$([regex]::Escape($sensitiveName))\b")) {
            $null = $sensitiveNames.Add($sensitiveName)
        }
    }
}

$output = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    extraction = [ordered]@{
        staticOnly = $true
        runtimeValuesRead = $false
        sourceCodeCopied = $false
    }
    sources = $sources
    commandNames = @($commandNames | Sort-Object)
    eventNames = @($eventNames | Sort-Object)
    parameterFieldNames = @($parameterNames | Sort-Object)
    sensitiveIdentifierNamesObserved = @($sensitiveNames | Sort-Object)
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$parent = Split-Path -Parent $resolvedOutput
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$output | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedOutput -Encoding utf8
Write-Host "Wrote static IPC identifier inventory to: $resolvedOutput"
