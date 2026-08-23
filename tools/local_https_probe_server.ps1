param(
    [int]$Port = 8443
)

$ErrorActionPreference = 'Stop'
$subject = [System.Security.Cryptography.X509Certificates.X500DistinguishedName]::new(
    'CN=plus-vita-local.invalid')
$rsa = [System.Security.Cryptography.RSA]::Create(2048)
$request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
    $subject,
    $rsa,
    [System.Security.Cryptography.HashAlgorithmName]::SHA256,
    [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
$request.CertificateExtensions.Add(
    [System.Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new(
        $false, $false, 0, $true))
$ephemeralCertificate = $request.CreateSelfSigned(
    [DateTimeOffset]::UtcNow.AddMinutes(-5),
    [DateTimeOffset]::UtcNow.AddHours(2))
$pfxPassword = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(24))
$pfxBytes = $ephemeralCertificate.Export(
    [System.Security.Cryptography.X509Certificates.X509ContentType]::Pkcs12,
    $pfxPassword)
$certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
    $pfxBytes,
    $pfxPassword,
    [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::UserKeySet -bor
    [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::PersistKeySet)
$ephemeralCertificate.Dispose()
[Array]::Clear($pfxBytes, 0, $pfxBytes.Length)

$listener = [System.Net.Sockets.TcpListener]::new(
    [System.Net.IPAddress]::Any, $Port)
$listener.Start()
Write-Output "plus-vita local HTTPS probe listening on port $Port"

try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $stream = [System.Net.Security.SslStream]::new($client.GetStream(), $false)
            $stream.AuthenticateAsServer(
                $certificate,
                $false,
                [System.Security.Authentication.SslProtocols]::Tls12,
                $false)
            $body = [Text.Encoding]::ASCII.GetBytes("plus-vita local probe`n")
            $header = [Text.Encoding]::ASCII.GetBytes(
                "HTTP/1.1 200 OK`r`nContent-Type: text/plain`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n")
            $stream.Write($header)
            $stream.Write($body)
            $stream.Dispose()
        } catch {
            Write-Output "TLS client disconnected/rejected certificate: $($_.Exception.Message)"
        } finally {
            $client.Dispose()
        }
    }
} finally {
    $listener.Stop()
    $certificate.Dispose()
    $rsa.Dispose()
}
