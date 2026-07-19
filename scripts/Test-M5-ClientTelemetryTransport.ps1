param(
    [string]$TestExecutable = (Join-Path (Split-Path -Parent $PSScriptRoot) "build\windows-x64\VibeReadyTelemetryTests.exe")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $TestExecutable)) {
    throw "Telemetry contract executable not found: $TestExecutable"
}

$portProbe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$portProbe.Start()
$port = ([System.Net.IPEndPoint]$portProbe.LocalEndpoint).Port
$portProbe.Stop()

$resultPath = Join-Path ([System.IO.Path]::GetTempPath()) ("VibeReadyTelemetryTransport-" + [guid]::NewGuid().ToString("N") + ".json")
$job = Start-Job -ArgumentList $port, $resultPath -ScriptBlock {
    param($Port, $ResultPath)

    $listener = [System.Net.HttpListener]::new()
    $listener.Prefixes.Add("http://localhost:$Port/")
    $listener.Start()
    $acceptedEvents = 0
    $requestCount = 0
    $maxBatchEvents = 0
    $firstResponse = $true
    $deadline = (Get-Date).AddSeconds(35)
    $pending = $null

    try {
        while ($acceptedEvents -lt 45 -and (Get-Date) -lt $deadline) {
            if ($null -eq $pending) {
                $pending = $listener.BeginGetContext($null, $null)
            }
            if (-not $pending.AsyncWaitHandle.WaitOne(1000)) {
                continue
            }

            $context = $listener.EndGetContext($pending)
            $pending = $null
            $reader = [System.IO.StreamReader]::new(
                $context.Request.InputStream,
                $context.Request.ContentEncoding
            )
            try {
                $body = $reader.ReadToEnd()
            } finally {
                $reader.Dispose()
            }

            $batchEvents = [regex]::Matches($body, '"event_name"').Count
            $requestCount++
            $maxBatchEvents = [math]::Max($maxBatchEvents, $batchEvents)

            if ($firstResponse) {
                $firstResponse = $false
                $context.Response.StatusCode = 429
                $responseBody = '{"ok":false,"error":"retry"}'
            } else {
                $context.Response.StatusCode = 200
                $acceptedEvents += $batchEvents
                $responseBody = '{"ok":true}'
            }

            $responseBytes = [System.Text.Encoding]::UTF8.GetBytes($responseBody)
            $context.Response.ContentType = "application/json"
            $context.Response.ContentLength64 = $responseBytes.Length
            $context.Response.OutputStream.Write($responseBytes, 0, $responseBytes.Length)
            $context.Response.Close()
        }
    } finally {
        $listener.Stop()
        $listener.Close()
        [ordered]@{
            accepted_events = $acceptedEvents
            request_count = $requestCount
            max_batch_events = $maxBatchEvents
            first_response_status = 429
        } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $ResultPath
    }
}

try {
    $endpoint = "http://localhost:$port/v1/telemetry/batch"
    & $TestExecutable --transport $endpoint
    if ($LASTEXITCODE -ne 0) {
        $testExitCode = $LASTEXITCODE
        Wait-Job -Job $job -Timeout 20 | Out-Null
        Receive-Job -Job $job -ErrorAction SilentlyContinue | Out-Null
        $diagnostic = if (Test-Path -LiteralPath $resultPath) {
            Get-Content -Raw -Encoding UTF8 -LiteralPath $resultPath
        } else {
            "no mock result; job state=$($job.State)"
        }
        throw "Telemetry transport contract executable failed with exit code $testExitCode. Mock result: $diagnostic"
    }

    Wait-Job -Job $job -Timeout 40 | Out-Null
    if ($job.State -ne "Completed") {
        throw "Telemetry mock endpoint did not complete. Job state: $($job.State)"
    }
    Receive-Job -Job $job | Out-Null

    if (-not (Test-Path -LiteralPath $resultPath)) {
        throw "Telemetry mock endpoint did not produce a result."
    }
    $result = Get-Content -Raw -Encoding UTF8 -LiteralPath $resultPath | ConvertFrom-Json
    if ([int]$result.accepted_events -ne 45) {
        throw "Expected 45 accepted telemetry events after retry; got $($result.accepted_events)."
    }
    if ([int]$result.max_batch_events -gt 20) {
        throw "Telemetry batch exceeded the 20-event client limit: $($result.max_batch_events)"
    }
    if ([int]$result.request_count -lt 4) {
        throw "Expected a retry plus at least three batches; got $($result.request_count) requests."
    }

    $result
    Write-Output "M5 client telemetry transport validation passed."
} finally {
    if ($job) {
        Stop-Job -Job $job -ErrorAction SilentlyContinue
        Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $resultPath) {
        Remove-Item -LiteralPath $resultPath -Force
    }
}
