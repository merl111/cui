param([Parameter(Mandatory=$true)][string]$Directory,
      [Parameter(Mandatory=$true)][string]$Prefix,
      [switch]$Fixture, [string]$Theme = 'light')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('IdleWindow' -as [type])) {
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class IdleWindow {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr w, IntPtr after, int x, int y, int width, int height, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr w, uint msg, UIntPtr wp, IntPtr lp, uint flags, uint timeout, out UIntPtr result);
}
'@
}
$arguments = @('--temporary-session', '--theme', $Theme)
if ($Fixture) { $arguments += '--fixture' }
$process = Start-Process (Join-Path $Directory 'archaic.exe') -ArgumentList $arguments -NoNewWindow -PassThru -RedirectStandardError "$Prefix-stderr.txt" -RedirectStandardOutput "$Prefix-stdout.txt"
try {
  [void]$process.WaitForInputIdle(30000)
  Start-Sleep -Seconds 4
  $process.Refresh()
  if ($process.HasExited) { throw "Archaic exited: $($process.ExitCode)" }
  $window = $process.MainWindowHandle
  if ($window -eq [IntPtr]::Zero) { throw 'No Archaic window' }
  $bounds = [Windows.Forms.Screen]::PrimaryScreen.Bounds
  $width = [Math]::Min(1440, $bounds.Width - 40)
  $height = [Math]::Min(900, $bounds.Height - 80)
  [void][IdleWindow]::SetWindowPos($window, [IntPtr]::Zero, 10, 10, $width, $height, 0x0040)
  Start-Sleep -Seconds 2
  $process.Refresh()
  $cpu = $process.TotalProcessorTime.TotalMilliseconds
  Start-Sleep -Seconds 5
  $process.Refresh()
  $cpu = $process.TotalProcessorTime.TotalMilliseconds - $cpu
  $elapsed = [Diagnostics.Stopwatch]::StartNew()
  $timeouts = 0
  for ($i = 0; $i -lt 30; $i++) {
    [void][IdleWindow]::SetWindowPos($window, [IntPtr]::Zero, 10 + ($i % 10), 10, $width, $height, 0x0015)
    [UIntPtr]$result = [UIntPtr]::Zero
    if ([IdleWindow]::SendMessageTimeout($window, 0, [UIntPtr]::Zero, [IntPtr]::Zero, 2, 1000, [ref]$result) -eq [IntPtr]::Zero) { $timeouts++ }
  }
  $elapsed.Stop()
  $bitmap = New-Object Drawing.Bitmap $bounds.Width, $bounds.Height
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.CopyFromScreen($bounds.Location, [Drawing.Point]::Empty, $bounds.Size)
    $bitmap.Save("$Prefix.png", [Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
  @{ idle_cpu_ms_over_5s = $cpu; move_and_reply_ms = $elapsed.Elapsed.TotalMilliseconds; unresponsive_messages = $timeouts; fixture = [bool]$Fixture; theme = $Theme } | ConvertTo-Json | Tee-Object "$Prefix.json"
} finally {
  if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
