# fetch-sdk.ps1 - Sync the x64dbg plugin SDK (headers + import libs) into plugin/sdk.
#
# The import libs (x64bridge.lib, x64dbg.lib, ...) are build artifacts shipped only
# in the x64dbg release zip, never in the source tree - so they can't be a submodule
# and aren't committed here. This script pulls them from the official GitHub release.
#
# Behaviour:
#   - Queries the latest x64dbg release for the pluginsdk asset.
#   - Downloads only if the local SDK is older than latest (or missing). Cached via
#     plugin/sdk/.sdk-version.
#   - On any connection error: if libs are already present, builds with them; otherwise
#     fails with a clear message (first build needs the internet once).
#
# Usage:  .\fetch-sdk.ps1 [-Force]

[CmdletBinding()]
param([switch]$Force)

$ErrorActionPreference = 'Stop'
$SdkDir = Join-Path $PSScriptRoot 'sdk'
$Marker = Join-Path $SdkDir '.sdk-version'
$Repo   = 'x64dbg/x64dbg'
$Asset  = 'x64dbg-pluginsdk.zip'
$Headers = @{ 'User-Agent' = 'x64dbg-mcp-build' }

function Test-LibsPresent {
    foreach ($l in 'x64bridge.lib', 'x64dbg.lib', 'x32bridge.lib', 'x32dbg.lib') {
        if (-not (Test-Path (Join-Path $SdkDir $l))) { return $false }
    }
    return $true
}

# Resolve latest release tag (public repo, no auth needed).
$rel = $null
try {
    $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -Headers $Headers -TimeoutSec 15
} catch {
    if (Test-LibsPresent) {
        Write-Host "[sdk] GitHub unreachable - building with existing SDK." -ForegroundColor Yellow
        return
    }
    throw "[sdk] No SDK libs present and GitHub is unreachable. Connect to the internet for the first build."
}

$latest  = $rel.tag_name
$current = if (Test-Path $Marker) { (Get-Content $Marker -Raw).Trim() } else { '' }

if (-not $Force -and $current -eq $latest -and (Test-LibsPresent)) {
    Write-Host "[sdk] Up to date ($current)." -ForegroundColor Green
    return
}

$dl = $rel.assets | Where-Object name -eq $Asset | Select-Object -First 1
if (-not $dl) { throw "[sdk] Asset '$Asset' not found in release $latest." }

Write-Host "[sdk] Updating pluginsdk: '$current' -> '$latest'" -ForegroundColor Cyan
$tmp = Join-Path ([IO.Path]::GetTempPath()) "x64dbg-sdk-$latest"
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $tmp | Out-Null
$zip = Join-Path $tmp $Asset
Invoke-WebRequest -Uri $dl.browser_download_url -OutFile $zip -Headers $Headers
Expand-Archive $zip -DestinationPath "$tmp\sdk" -Force

$src = "$tmp\sdk"
Copy-Item "$src\*.h"   $SdkDir -Force
Copy-Item "$src\*.lib" $SdkDir -Force
foreach ($sub in 'jansson', 'lz4', 'XEDParse') {
    $d = Join-Path $SdkDir $sub
    New-Item -ItemType Directory -Path $d -Force | Out-Null
    Copy-Item "$src\$sub\*" $d -Force
}
Set-Content -Path $Marker -Value $latest -NoNewline
Write-Host "[sdk] Synced pluginsdk $latest." -ForegroundColor Green
