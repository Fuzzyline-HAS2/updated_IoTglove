[CmdletBinding()]
param(
  # Our TTGO T8 v1.7.1 boards ship a WCH CH9102 USB-UART bridge (VID 1A86 / PID 55D4).
  # Override for a CP2104 board (VID_10C4.*PID_EA60) or a CH340 (VID_1A86.*PID_7523).
  [string]$VidPid   = 'VID_1A86.*PID_55D4',
  [string]$Chip     = 'esp32',
  [int]$Baud        = 921600,
  # Defaults to the ttgo-t8-v171 factory image. Override to flash a different build.
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
  $Firmware = Join-Path $root 'dist\wired-upload\ttgo-t8-v171\ttgo-factory.bin'
}
if (-not $Esptool) {
  $Esptool = Join-Path $root 'dist\tools\esptool\esptool.exe'
}

if (-not (Test-Path -LiteralPath $Firmware -PathType Leaf)) {
  Write-Host "[ERROR] Firmware not found:" -ForegroundColor Red
  Write-Host "        $Firmware"
  Write-Host "Build it first, e.g.:"
  Write-Host "  powershell scripts\pio_docker_build.ps1 -Env ttgo-t8-v171"
  Write-Host "then ensure ttgo-factory.bin exists under dist\wired-upload\ttgo-t8-v171\."
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

function Get-TtgoPort {
  $dev = Get-CimInstance Win32_PnPEntity |
    Where-Object { $_.DeviceID -match $VidPid -and $_.Name -match '\(COM\d+\)' } |
    Select-Object -First 1
  if ($dev) {
    return 'COM' + ([regex]'\(COM(\d+)\)').Match($dev.Name).Groups[1].Value
  }
  return $null
}

Write-Host "==================================================="
Write-Host " TTGO T8 v1.7.1 auto-flasher (updated_IoTglove)"
Write-Host "==================================================="
Write-Host " Watching for : $VidPid"
Write-Host " Firmware     : $Firmware"
Write-Host " Chip / Baud  : $Chip / $Baud"
Write-Host " Plug in a TTGO to flash it. Ctrl+C to stop."
Write-Host ""

$last = $null
while ($true) {
  $port = Get-TtgoPort
  if ($port) {
    if ($port -ne $last) {
      Write-Host "[detected] TTGO on $port - flashing..." -ForegroundColor Cyan
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
