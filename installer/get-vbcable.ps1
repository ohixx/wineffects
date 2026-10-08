# Downloads the current VB-Cable driver pack from the vendor and unpacks it next to this script's temp dir.
# VB-Audio's licence does not allow redistributing the driver, so the installer fetches it on demand.
$ErrorActionPreference = 'Stop'
$dir = Join-Path $env:TEMP 'wineffects-cable'
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force -Path $dir | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$base = [Uri]'https://vb-audio.com/Cable/'
$page = (Invoke-WebRequest -UseBasicParsing $base.AbsoluteUri).Content
$m = [regex]::Matches($page, 'href="([^"]*VBCABLE_Driver_Pack\d+\.zip)"')
if ($m.Count -eq 0) { exit 2 }
$url = [Uri]::new($base, $m[0].Groups[1].Value).AbsoluteUri

$zip = Join-Path $dir 'cable.zip'
Invoke-WebRequest -UseBasicParsing $url -OutFile $zip
Expand-Archive -Force $zip (Join-Path $dir 'pack')
exit 0
