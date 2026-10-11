# ============================================================================
#  CrispAS launcher with JSON/JSONC profiles (two-level folder menu)
#  - Level 1: lists subfolders of <CrispAS>\profile\  (e.g. Japanese)
#  - Level 2: lists .json/.conf files inside the chosen folder
#  - .conf files are JSONC (JSON + // or # comments); comments are stripped
#    before parsing
#  - Root-level profiles are NO LONGER SUPPORTED (since 2026-10-11):
#    profiles must live in a subfolder of profile\
#  - Lets you pick one, then starts crispasr.exe in the FOREGROUND so you see
#    its raw output (same workflow as a self-hosted LLM node).
#  - Never downloads anything: if the model file is missing it prints the
#    download URL stored in the profile and exits.
# ============================================================================

param(
    [switch]$ListOnly
)

$ErrorActionPreference = 'Stop'

# --- Strip // and # comments from JSONC (string-aware, keeps URLs intact) ----
function Remove-JsonComments {
    param([string]$Text)
    $out = New-Object System.Text.StringBuilder
    foreach ($line in ($Text -split "`r?`n")) {
        $inString = $false
        $escaped  = $false
        $sb = New-Object System.Text.StringBuilder
        for ($i = 0; $i -lt $line.Length; $i++) {
            $c = $line[$i]
            if ($inString) {
                [void]$sb.Append($c)
                if ($escaped)      { $escaped = $false }
                elseif ($c -eq '\\') { $escaped = $true }
                elseif ($c -eq '"')  { $inString = $false }
            }
            elseif ($c -eq '"') { $inString = $true; [void]$sb.Append($c) }
            elseif ($c -eq '/' -and ($i + 1) -lt $line.Length -and $line[$i + 1] -eq '/') { break }
            elseif ($c -eq '#') { break }
            else { [void]$sb.Append($c) }
        }
        [void]$out.AppendLine($sb.ToString())
    }
    return $out.ToString()
}

# --- Load all valid profiles from one folder ---------------------------------
function Get-ProfilesInFolder {
    param([System.IO.DirectoryInfo]$Folder)
    $loaded = @()
    $files = @(Get-ChildItem -Path $Folder.FullName -File |
               Where-Object { $_.Extension -in '.json', '.conf' } |
               Sort-Object Name)
    foreach ($p in $files) {
        try {
            $raw = Get-Content -LiteralPath $p.FullName -Raw -Encoding UTF8
            $cfg = (Remove-JsonComments -Text $raw) | ConvertFrom-Json
        } catch {
            Write-Host ("  [WARN] cannot parse " + $p.Name + " -> " + $_.Exception.Message) -ForegroundColor Yellow
            continue
        }
        $loaded += [pscustomobject]@{ File = $p; Cfg = $cfg; ModelPath = (Join-Path $ModelsDir $cfg.model) }
    }
    return $loaded
}

# --- Show one loaded profile line with [OK]/[MISS] ---------------------------
function Show-ProfileLine {
    param($Item, [int]$Index)
    $ok    = Test-Path -LiteralPath $Item.ModelPath
    $mark  = if ($ok) { '[OK]  ' } else { '[MISS]' }
    $color = if ($ok) { 'Green' } else { 'Yellow' }
    Write-Host -NoNewline ("{0,3}. {1}" -f ($Index + 1), $mark)
    Write-Host -NoNewline $Item.File.BaseName -ForegroundColor $color
    Write-Host ("   <" + $Item.Cfg.backend + ">")
}

$Root       = Split-Path -Parent $PSScriptRoot
$ProfileDir = Join-Path $Root 'profile'
$ModelsDir  = Join-Path $Root 'models'
$LogDir     = Join-Path $Root 'logs'
$DefaultApiKey = 'default'

if (-not (Test-Path $ProfileDir)) {
    Write-Host "[ERROR] profile dir not found: $ProfileDir" -ForegroundColor Red
    Read-Host 'press Enter to exit' | Out-Null
    exit 1
}

# --- Level 1: discover profile folders --------------------------------------
$folders = @(Get-ChildItem -Path $ProfileDir -Directory | Sort-Object Name)
if ($folders.Count -eq 0) {
    Write-Host "[ERROR] no profile folders in $ProfileDir" -ForegroundColor Red
    Write-Host "        profiles must live in a subfolder (e.g. profile\Japanese\*.conf)." -ForegroundColor Yellow
    Write-Host "        Root-level .json/.conf files are no longer scanned (since 2026-10-11)."
    Read-Host 'press Enter to exit' | Out-Null
    exit 1
}

# --- ListOnly: dump every folder with its profiles and status ----------------
if ($ListOnly) {
    foreach ($f in $folders) {
        Write-Host ''
        Write-Host ("=== folder: " + $f.Name + " ===") -ForegroundColor Cyan
        $loaded = @(Get-ProfilesInFolder -Folder $f)
        if ($loaded.Count -eq 0) {
            Write-Host '  (no valid .json/.conf profiles)' -ForegroundColor Yellow
            continue
        }
        for ($i = 0; $i -lt $loaded.Count; $i++) { Show-ProfileLine -Item $loaded[$i] -Index $i }
    }
    exit 0
}

# --- Level 1: pick a folder --------------------------------------------------
if ($folders.Count -eq 1) {
    $selFolder = $folders[0]
    Write-Host ("[folder] " + $selFolder.Name + "  (only folder, auto-selected)")
} else {
    Write-Host ''
    Write-Host '================= CrispAS profile folders =================' -ForegroundColor Cyan
    for ($i = 0; $i -lt $folders.Count; $i++) {
        Write-Host ("{0,3}. {1}" -f ($i + 1), $folders[$i].Name)
    }
    Write-Host '===========================================================' -ForegroundColor Cyan
    $fidx = -1
    while ($fidx -lt 0 -or $fidx -ge $folders.Count) {
        $answer = Read-Host ("select folder [1-" + $folders.Count + "] (Enter=1)")
        if ($answer -eq '') { $answer = '1' }
        if (-not ($answer -match '^\d+$')) { continue }
        $fidx = [int]$answer - 1
    }
    $selFolder = $folders[$fidx]
}

# --- Level 2: load profiles from the selected folder -------------------------
$loaded = @(Get-ProfilesInFolder -Folder $selFolder)
if ($loaded.Count -eq 0) {
    Write-Host ("[ERROR] no valid .json/.conf profiles in folder: " + $selFolder.FullName) -ForegroundColor Red
    Read-Host 'press Enter to exit' | Out-Null
    exit 1
}

# --- Level 2 menu ------------------------------------------------------------
Write-Host ''
Write-Host ("=================== CrispAS profiles: " + $selFolder.Name + " ===================") -ForegroundColor Cyan
for ($i = 0; $i -lt $loaded.Count; $i++) { Show-ProfileLine -Item $loaded[$i] -Index $i }
Write-Host '=========================================================' -ForegroundColor Cyan

# --- Pick -------------------------------------------------------------------
$idx = -1
while ($idx -lt 0 -or $idx -ge $loaded.Count) {
    $answer = Read-Host ("select profile [1-" + $loaded.Count + "] (Enter=1)")
    if ($answer -eq '') { $answer = '1' }
    if (-not ($answer -match '^\d+$')) { continue }
    $idx = [int]$answer - 1
}
$sel = $loaded[$idx]
$cfg = $sel.Cfg

# --- Validate profile -------------------------------------------------------
$exe = Join-Path $Root (Join-Path $cfg.build 'crispasr.exe')
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host ("[ERROR] build not found: " + $exe) -ForegroundColor Red
    Read-Host 'press Enter to exit' | Out-Null
    exit 1
}

Write-Host ''
if (-not (Test-Path -LiteralPath $sel.ModelPath)) {
    Write-Host ("[ERROR] model file missing: " + $sel.ModelPath) -ForegroundColor Red
    if ($cfg.download) {
        Write-Host '        download it from:' -ForegroundColor Yellow
        Write-Host ('        ' + $cfg.download) -ForegroundColor Yellow
        Write-Host ('        then place it under: ' + $ModelsDir)
    }
    Read-Host 'press Enter to exit' | Out-Null
    exit 1
}

# --- Environment (everything stays on E:) -----------------------------------
$env:CRISPASR_MODELS_DIR = $ModelsDir
$env:CRISPASR_CACHE_DIR  = Join-Path $Root 'cache'
$env:CRISPASR_SCRATCH_DIR= Join-Path $Root 'scratch'
if ($cfg.api_keys) { $env:CRISPASR_API_KEYS = $cfg.api_keys } else { $env:CRISPASR_API_KEYS = $DefaultApiKey }
if ($cfg.device -ne $null -and "$($cfg.device)" -ne '') { $env:CUDA_VISIBLE_DEVICES = "$($cfg.device)" }
else { Remove-Item Env:\CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue }

# --- Optional forced aligner --------------------------------------------------
$argList = @('--server', '--backend', $cfg.backend, '-m', $sel.ModelPath)
if ($cfg.aligner) {
    $alignerPath = Join-Path $ModelsDir $cfg.aligner
    if (Test-Path -LiteralPath $alignerPath) {
        $argList += @('-am', $alignerPath)
        Write-Host '[aligner] word-level timestamps enabled' -ForegroundColor DarkCyan
    } else {
        Write-Host ('[WARN] aligner missing, starting WITHOUT word timestamps: ' + $alignerPath) -ForegroundColor Yellow
    }
}
if ($cfg.language) { $argList += @('-l', $cfg.language) }

# --- Tunables (all optional; omitted fields = backend defaults) --------------
if ($cfg.vad)                     { $argList += '--vad' }
if ($cfg.force_aligner)           { $argList += '--force-aligner' }
if ($null -ne $cfg.chunk_seconds) { $argList += @('--chunk-seconds', "$($cfg.chunk_seconds)") }
if ($cfg.beam_size)               { $argList += @('-bs', "$($cfg.beam_size)") }
if ($cfg.temperature -gt 0)       { $argList += @('-tp', "$($cfg.temperature)") }
if ($cfg.frequency_penalty -gt 0) { $argList += @('--frequency-penalty', "$($cfg.frequency_penalty)") }
if ($cfg.max_new_tokens)          { $argList += @('-n', "$($cfg.max_new_tokens)") }
if ($cfg.seed -gt 0)              { $argList += @('--seed', "$($cfg.seed)") }
if ($cfg.no_punctuation)          { $argList += '--no-punctuation' }
if ($cfg.split_on_punct)          { $argList += '-sp' }
if ($cfg.env) {
    $cfg.env.PSObject.Properties | ForEach-Object { Set-Item -Path ("Env:" + $_.Name) -Value $_.Value }
}

if ($cfg.args)     { $argList += @($cfg.args) }

$host_ = if ($cfg.host) { $cfg.host } else { '127.0.0.1' }
$port  = if ($cfg.port) { $cfg.port } else { 5114 }
$argList += @('--host', $host_, '--port', $port)

# --- Banner ------------------------------------------------------------------
Write-Host ''
Write-Host ("folder  : " + $selFolder.Name)
Write-Host ("profile : " + $sel.File.BaseName)
Write-Host ("build   : " + $cfg.build)
Write-Host ("backend : " + $cfg.backend)
Write-Host ("model   : " + $cfg.model)
Write-Host ("gpu     : " + $(if ("$($cfg.device)" -ne '') { "CUDA_VISIBLE_DEVICES=$($cfg.device)" } else { 'all visible' }))
Write-Host ("endpoint: http://" + $host_ + ":" + $port + "/v1   (API key: see profile / default)")
Write-Host ('press Ctrl+C to stop')
Write-Host ''

# --- Run in foreground: raw output goes straight to this console -------------
& $exe @argList
$code = $LASTEXITCODE
Write-Host ''
Write-Host ("[crispasr exited with code " + $code + "]") -ForegroundColor DarkGray
Read-Host 'press Enter to close' | Out-Null
