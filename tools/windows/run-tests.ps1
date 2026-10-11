# Runs the tests listed in tests.txt ("unit <name>" or "probe <name>") and records each exit
# code in test-results.txt. Start it with run-tests.cmd from the delivery folder.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $here
$logDir = Join-Path $here 'test-logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("Test run $(Get-Date -Format o)")
try {
    $os = Get-CimInstance Win32_OperatingSystem
    $lines.Add("Windows: $($os.Caption) $($os.Version)")
    foreach ($gpu in Get-CimInstance Win32_VideoController) {
        $lines.Add("GPU: $($gpu.Name), driver $($gpu.DriverVersion)")
    }
} catch {
    $lines.Add("System information unavailable: $($_.Exception.Message)")
}
$lines.Add('')
foreach ($line in Get-Content -LiteralPath (Join-Path $here 'BUILD-INFO.txt')) { $lines.Add($line) }
$lines.Add('')

$tests = @(Get-Content -LiteralPath (Join-Path $here 'tests.txt') | Where-Object { $_ })
$failures = 0
foreach ($entry in $tests) {
    $kind, $name = $entry -split ' ', 2
    $exe = Join-Path $here "$name.exe"
    $out = Join-Path $logDir "$name.out.txt"
    $err = Join-Path $logDir "$name.err.txt"
    $start = @{
        FilePath = $exe
        WorkingDirectory = $here
        NoNewWindow = $true
        PassThru = $true
        RedirectStandardOutput = $out
        RedirectStandardError = $err
    }
    if ($kind -eq 'probe') { $start.ArgumentList = '--frames 3' }
    try {
        $process = Start-Process @start
        $null = $process.Handle  # keeps ExitCode available after WaitForExit
        if ($process.WaitForExit(60000)) {
            $code = $process.ExitCode
        } else {
            $process.Kill()
            $code = 'timeout'
        }
    } catch {
        $code = "error: $($_.Exception.Message)"
    }
    if ("$code" -eq '0') {
        $status = 'PASS'
    } elseif ($kind -eq 'unit' -and "$code" -eq '77') {
        $status = 'SKIP'
    } else {
        $status = 'FAIL'
        $failures++
    }
    $lines.Add(('{0,-4} {1,-5} {2,-32} exit={3}' -f $status, $kind, $name, $code))
    foreach ($file in @($out, $err)) {
        if ((Test-Path -LiteralPath $file) -and (Get-Item -LiteralPath $file).Length -gt 0) {
            foreach ($text in Get-Content -LiteralPath $file -TotalCount 20) { $lines.Add("     | $text") }
        }
    }
}
$lines.Add('')
$lines.Add("Failures: $failures of $($tests.Count)")
$lines | Set-Content -LiteralPath (Join-Path $here 'test-results.txt') -Encoding UTF8
$lines | ForEach-Object { Write-Host $_ }
