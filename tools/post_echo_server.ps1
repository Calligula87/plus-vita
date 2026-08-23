param([int]$Port = 18080)

$expectedBody = 'plus-vita-post=takion-12'
$listener = [System.Net.Sockets.TcpListener]::new(
    [System.Net.IPAddress]::Any, $Port)
$listener.Start()
Write-Output "Plus Vita POST echo listening on 0.0.0.0:$Port"

try {
    $client = $listener.AcceptTcpClient()
    try {
        $stream = $client.GetStream()
        $bytes = [System.Collections.Generic.List[byte]]::new()
        $buffer = [byte[]]::new(4096)
        $headerEnd = -1
        $contentLength = 0

        while ($true) {
            $read = $stream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) { break }
            for ($i = 0; $i -lt $read; $i++) { $bytes.Add($buffer[$i]) }
            $text = [Text.Encoding]::ASCII.GetString($bytes.ToArray())
            if ($headerEnd -lt 0) {
                $headerEnd = $text.IndexOf("`r`n`r`n")
                if ($headerEnd -ge 0 -and
                    $text.Substring(0, $headerEnd) -match '(?im)^Content-Length:\s*(\d+)\s*$') {
                    $contentLength = [int]$Matches[1]
                }
            }
            if ($headerEnd -ge 0 -and
                $bytes.Count -ge $headerEnd + 4 + $contentLength) { break }
        }

        $request = [Text.Encoding]::ASCII.GetString($bytes.ToArray())
        $body = if ($headerEnd -ge 0) {
            $request.Substring($headerEnd + 4, $contentLength)
        } else { '' }
        $valid = $request.StartsWith("POST /echo HTTP/1.") -and
            $request -match '(?im)^X-Plus-Vita-Test:\s*compat-post\s*$' -and
            $request -match '(?im)^Content-Type:\s*application/x-www-form-urlencoded\s*$' -and
            $body -ceq $expectedBody
        $status = if ($valid) { '200 OK' } else { '400 Bad Request' }
        $echo = if ($valid) { 'ok' } else { 'invalid' }
        $responseBody = if ($valid) { $body } else { 'invalid controlled POST' }
        $response = "HTTP/1.1 $status`r`nContent-Type: application/octet-stream`r`nX-Plus-Vita-Echo: $echo`r`nContent-Length: $($responseBody.Length)`r`nConnection: close`r`n`r`n$responseBody"
        $responseBytes = [Text.Encoding]::ASCII.GetBytes($response)
        $stream.Write($responseBytes, 0, $responseBytes.Length)
        $stream.Flush()
        Write-Output "POST valid=$valid bytes=$contentLength"
    } finally {
        $client.Dispose()
    }
} finally {
    $listener.Stop()
}
