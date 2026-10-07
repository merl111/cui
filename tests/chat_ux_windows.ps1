param([Parameter(Mandatory=$true)][string]$Binary)
# Query the real WinUI accessibility tree from another process; UIA calls must
# not block the application's own UI thread. No account or network is used.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$ack = Join-Path ([IO.Path]::GetTempPath()) ('cui-ux-' + [guid]::NewGuid().ToString() + '.done')
$previous = $env:CUI_UX_UIA_ACK
$process = $null
try {
    $env:CUI_UX_UIA_ACK = $ack
    # Retain the native process handle ourselves. Windows PowerShell's
    # Start-Process wrapper can return a null ExitCode after WaitForExit.
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo.FileName = (Resolve-Path -LiteralPath $Binary).Path
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    if (-not $process.Start()) { throw 'Could not start the native UX contracts' }
    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ProcessIdProperty, [int]$process.Id)
    $edit = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Edit)
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    $found = $false
    while ([DateTime]::UtcNow -lt $deadline -and -not $process.HasExited -and -not $found) {
        $windows = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Children, $condition)
        foreach ($window in $windows) {
            $items = $window.FindAll([System.Windows.Automation.TreeScope]::Descendants, $edit)
            foreach ($item in $items) {
                $pattern = $null
                if ($item.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern, [ref]$pattern)) {
                    $value = $pattern.Current.Value
                    if ($value.Contains('Alice') -and $value.Contains('12:30') -and
                        $value.Contains('Complete message text, including the second span.')) {
                        if ($item.Current.IsOffscreen) { throw 'Visible message is offscreen in the accessibility tree' }
                        if (-not $pattern.Current.IsReadOnly) { throw 'Message accessibility text is editable' }
                        $found = $true
                        break
                    }
                }
            }
            if ($found) { break }
        }
        if (-not $found) { Start-Sleep -Milliseconds 100 }
    }
    if (-not $found) { throw 'WinUI did not expose the complete message through a native text ValuePattern' }
    [IO.File]::WriteAllText($ack, 'readable')
    if (-not $process.WaitForExit(10000)) { throw 'Native UX contracts did not finish' }
    if ($process.ExitCode -ne 0) { throw "Native UX contracts failed: $($process.ExitCode)" }
    Write-Output 'WinUI UI Automation: complete read-only message text passed'
} finally {
    if ($null -ne $process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if ($null -ne $process) { $process.Dispose() }
    $env:CUI_UX_UIA_ACK = $previous
    Remove-Item -LiteralPath $ack -ErrorAction SilentlyContinue
}
