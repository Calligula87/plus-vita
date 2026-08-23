param(
    [string]$VitaAddress = '192.168.1.11',
    [int]$Port = 8443
)

$ErrorActionPreference = 'Stop'
$logPath = 'C:\Users\tutto\Documents\PsVitaPorts\plus-vita\local-artifacts\https-probe\firewall-admin.log'
try {
    $name = 'plus-vita local TLS probe'
    $existing = @(Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) {
        $existing | Remove-NetFirewallRule
    }
    $rule = New-NetFirewallRule `
        -DisplayName $name `
        -Direction Inbound `
        -Action Allow `
        -Protocol TCP `
        -LocalPort $Port `
        -RemoteAddress $VitaAddress `
        -Profile Public
    "created: $($rule.Name)" | Set-Content -LiteralPath $logPath
} catch {
    ($_ | Out-String) | Set-Content -LiteralPath $logPath
    throw
}
