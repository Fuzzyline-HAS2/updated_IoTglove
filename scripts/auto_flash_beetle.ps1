[CmdletBinding()]
param(
  # Espressif native-USB id for the DFRobot Beetle ESP32-C3 (matched as a regex against DeviceID).
  [string]$VidPid   = 'VID_303A.*PID_1001',
  [string]$Chip     = 'esp32c3',
  [int]$Baud        = 921600,
  # Defaults to the wifi_location factory image. Override to flash a different build.
  [string]$Firmware,
  # Defaults to the official Espressif standalone bundled under dist\tools.
  [string]$Esptool,
  [int]$PollMs      = 1500
)

# Native tools can write normal status messages to stderr. Drive control flow from
# their exit codes instead of converting stderr into terminating PowerShell errors.
$ErrorActionPreference = 'Continue'

# scripts/ -> repo root
$root = Split-Path -Parent $PSScriptRoot
if (-not $Firmware) {
  $Firmware = Join-Path $root 'dist\wired-upload\beetle-c3-location\beetle-factory.bin'
}
if (-not $Esptool) {
  $Esptool = Join-Path $root 'dist\tools\esptool\esptool.exe'
}

if (-not (Test-Path -LiteralPath $Firmware -PathType Leaf)) {
  Write-Host "[ERROR] Firmware not found:" -ForegroundColor Red
  Write-Host "        $Firmware"
  Write-Host "Build it first, e.g.:"
  Write-Host "  powershell scripts\pio_docker_build.ps1 -Env beetle-c3-location"
  Write-Host "then ensure beetle-factory.bin exists under dist\wired-upload\beetle-c3-location\."
  Read-Host 'Press Enter to exit'
  exit 1
}

if (-not (Test-Path -LiteralPath $Esptool -PathType Leaf)) {
  Write-Host '[ERROR] Bundled esptool.exe not found:' -ForegroundColor Red
  Write-Host "        $Esptool"
  Write-Host 'Copy the complete dist and scripts folders into the same parent folder.'
  Read-Host 'Press Enter to exit'
  exit 1
}

if (-not [Environment]::Is64BitOperatingSystem) {
  Write-Host '[ERROR] Bundled esptool requires 64-bit Windows.' -ForegroundColor Red
  Read-Host 'Press Enter to exit'
  exit 1
}

$Firmware = (Resolve-Path -LiteralPath $Firmware).Path
$Esptool = (Resolve-Path -LiteralPath $Esptool).Path

& $Esptool version
$esptoolExit = $LASTEXITCODE
if ($esptoolExit -ne 0) {
  Write-Host "[ERROR] esptool.exe could not run (exit $esptoolExit)." -ForegroundColor Red
  Write-Host 'Check Windows Security > Protection history for a blocked file.'
  Read-Host 'Press Enter to exit'
  exit 1
}

function Get-BeetlePort {
  $dev = Get-CimInstance Win32_PnPEntity |
    Where-Object { $_.DeviceID -match $VidPid -and $_.Name -match '\(COM\d+\)' } |
    Select-Object -First 1
  if ($dev) {
    return 'COM' + ([regex]'\(COM(\d+)\)').Match($dev.Name).Groups[1].Value
  }
  return $null
}

Write-Host "==================================================="
Write-Host " Beetle ESP32-C3 auto-flasher (wifi_location)"
Write-Host "==================================================="
Write-Host " Watching for : $VidPid"
Write-Host " Firmware     : $Firmware"
Write-Host " Chip / Baud  : $Chip / $Baud"
Write-Host " Plug in a Beetle to flash it. Ctrl+C to stop."
Write-Host ""

$last = $null
while ($true) {
  $port = Get-BeetlePort
  if ($port) {
    if ($port -ne $last) {
      Write-Host "[detected] Beetle on $port - flashing..." -ForegroundColor Cyan
      & $Esptool --chip $Chip --port $port --baud $Baud --before default-reset --after hard-reset write-flash -z 0x0 $Firmware
      $flashExit = $LASTEXITCODE
      if ($flashExit -eq 0) {
        Write-Host "[OK] $port flashed. Unplug this board, plug the next one.`n" -ForegroundColor Green
        $last = $port
      } else {
        Write-Host "[FAIL] flash failed on $port (exit $flashExit). Will retry shortly.`n" -ForegroundColor Red
        $last = $null
        Start-Sleep -Seconds 2
      }
    }
  } else {
    # Board unplugged -> arm for the next insertion.
    $last = $null
  }
  Start-Sleep -Milliseconds $PollMs
}
