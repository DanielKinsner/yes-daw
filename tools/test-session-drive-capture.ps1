# UI correctness pass unit 14 (rubric FIX 15): the session drive's shots show the app's own popups and alerts - and
# nothing else. Real windows on the real YesDawDrive type: a main window; above it a topmost "menu", an unowned
# non-topmost "alert" (a real JUCE alert's shape) and a layered window (JUCE's drop-shadow strips are layered); below it
# a same-process window; over it a window from another process. The capture must be exactly the main client's size, show
# the menu and the alert, show main's colour where the layered, below and foreign windows sit, and say it composited 2;
# without compositing, the menu's place shows main's colour. Needs an interactive desktop. Prints PASS/FAIL; exit 0/1.
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $PSScriptRoot 'session-drive.ps1')
$block = [regex]::Match($source, "Add-Type -ReferencedAssemblies System\.Drawing -TypeDefinition @'\r?\n(.*?)\r?\n'@", 'Singleline')
if (-not $block.Success) { throw 'session-drive.ps1: the YesDawDrive type was not found' }
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition $block.Groups[1].Value
[YesDawDrive]::MakeDpiAware()
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CaptureCheckWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(PT p);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")] public static extern IntPtr SetWindowLongPtr(IntPtr h, int index, IntPtr value);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
    public static IntPtr TopAt(int x, int y) { PT p; p.X = x; p.Y = y; return GetAncestor(WindowFromPoint(p), 2 /* GA_ROOT */); }
    public static string Describe(IntPtr h) {
        System.Text.StringBuilder s = new System.Text.StringBuilder(256); GetWindowTextW(h, s, 256);
        uint pid; GetWindowThreadProcessId(h, out pid);
        return "'" + s.ToString() + "' (pid " + pid + ")";
    }
}
'@

$failures = New-Object System.Collections.Generic.List[string]
function Check([bool] $ok, [string] $what) {
  if ($ok) { Write-Host "  PASS $what" } else { Write-Host "  FAIL $what" -ForegroundColor Red; $failures.Add($what) }
}
function Near([System.Drawing.Color] $c, [int] $r, [int] $g, [int] $b) {
  return [Math]::Abs($c.R - $r) -le 12 -and [Math]::Abs($c.G - $g) -le 12 -and [Math]::Abs($c.B - $b) -le 12
}
function NewForm([string] $title, [System.Drawing.Color] $colour, [int] $x, [int] $y, [int] $w, [int] $h, [bool] $bare) {
  $f = New-Object System.Windows.Forms.Form
  $f.Text = $title; $f.BackColor = $colour; $f.ShowInTaskbar = $false
  $f.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
  if ($bare) { $f.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None }
  $f.Location = New-Object System.Drawing.Point($x, $y)
  $f.ClientSize = New-Object System.Drawing.Size($w, $h)
  return $f
}

$red = [System.Drawing.Color]::FromArgb(220, 30, 30)
$main = NewForm 'capture-check main' $red 120 120 480 320 $false
$below = NewForm 'capture-check below' ([System.Drawing.Color]::FromArgb(30, 200, 200)) 0 0 100 70 $true
$main.Show(); [System.Windows.Forms.Application]::DoEvents()
$origin = $main.PointToScreen([System.Drawing.Point]::Empty)
function At([int] $cx, [int] $cy) { return New-Object System.Drawing.Point(($origin.X + $cx), ($origin.Y + $cy)) }
# Below main: a same-process window behind it.
$below.Location = At 300 220
$below.Show()
[void][CaptureCheckWin32]::SetWindowPos($below.Handle, $main.Handle, 0, 0, 0, 0, 0x0013 <# NOSIZE|NOMOVE|NOACTIVATE #>)
# Above main: the topmost "menu", the unowned non-topmost "alert", the layered window.
$menu = NewForm 'capture-check menu' ([System.Drawing.Color]::FromArgb(30, 200, 30)) 0 0 120 80 $true
$menu.TopMost = $true; $menu.Location = At 30 30; $menu.Show()
$alert = NewForm 'capture-check alert' ([System.Drawing.Color]::FromArgb(30, 30, 220)) 0 0 140 90 $false
$alert.Location = At 200 30; $alert.Show()
[void][CaptureCheckWin32]::SetWindowLongPtr($alert.Handle, -8 <# GWLP_HWNDPARENT: no owner, as a JUCE alert #>, [IntPtr]::Zero)
$alert.BringToFront()
$layered = NewForm 'capture-check layered' ([System.Drawing.Color]::FromArgb(230, 220, 30)) 0 0 100 60 $true
$layered.Opacity = 0.95; $layered.Location = At 40 200; $layered.Show(); $layered.BringToFront()
# Over main: another process's window.
$foreignRect = At 380 120
$foreignScript = "Add-Type -AssemblyName System.Windows.Forms; Add-Type -AssemblyName System.Drawing; " +
  "`$f = New-Object System.Windows.Forms.Form; `$f.Text = 'capture-check foreign'; `$f.TopMost = `$true; `$f.ShowInTaskbar = `$false; " +
  "`$f.FormBorderStyle = 'None'; `$f.StartPosition = 'Manual'; `$f.BackColor = [System.Drawing.Color]::FromArgb(200, 30, 200); " +
  "`$f.Location = New-Object System.Drawing.Point($($foreignRect.X), $($foreignRect.Y)); `$f.ClientSize = New-Object System.Drawing.Size(80, 70); " +
  "[System.Windows.Forms.Application]::Run(`$f)"
$foreign = Start-Process powershell -ArgumentList @('-NoProfile', '-Command', $foreignScript) -WindowStyle Hidden -PassThru
$path = Join-Path ([System.IO.Path]::GetTempPath()) ('yesdaw-capture-check-' + $PID + '.png')
try {
  for ($i = 0; $i -lt 40; $i++) { [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 50 }
  $foreignAt = At 420 155
  for ($i = 0; $i -lt 60 -and [CaptureCheckWin32]::TopAt($foreignAt.X, $foreignAt.Y) -eq $main.Handle; $i++) { Start-Sleep -Milliseconds 100 }

  # Preconditions: each window is where the check needs it; a window in the way is named.
  $expect = @(
    @{ what = 'the menu over main';       at = (At 90 70);   window = $menu.Handle },
    @{ what = 'the alert over main';      at = (At 270 75);  window = $alert.Handle },
    @{ what = 'the layered window over main'; at = (At 90 230); window = $layered.Handle },
    @{ what = 'main over the window below it'; at = (At 350 255); window = $main.Handle })
  foreach ($e in $expect) {
    $top = [CaptureCheckWin32]::TopAt($e.at.X, $e.at.Y)
    Check ($top -eq $e.window) ("precondition: " + $e.what + $(if ($top -ne $e.window) { ' - in the way: ' + [CaptureCheckWin32]::Describe($top) } else { '' }))
  }
  $alertOwner = [CaptureCheckWin32]::GetWindow($alert.Handle, 4 <# GW_OWNER #>)
  $alertTopmost = ([CaptureCheckWin32]::GetWindowLongPtr($alert.Handle, -20).ToInt64() -band 0x8) -ne 0
  Check ($alertOwner -eq [IntPtr]::Zero -and -not $alertTopmost) ("precondition: the alert is unowned and not topmost (owner $alertOwner, topmost $alertTopmost)")
  $foreignTop = [CaptureCheckWin32]::TopAt($foreignAt.X, $foreignAt.Y)
  $foreignPid = 0; [void][CaptureCheckWin32]::GetWindowThreadProcessId($foreignTop, [ref]$foreignPid)
  Check ($foreignPid -eq $foreign.Id) ("precondition: the other process's window over main - found " + [CaptureCheckWin32]::Describe($foreignTop))

  # The composited capture.
  $count = [YesDawDrive]::CaptureClient($main.Handle, $path, $true)
  $shot = [System.Drawing.Bitmap]::FromFile($path)
  try {
    Check ($shot.Width -eq $main.ClientSize.Width -and $shot.Height -eq $main.ClientSize.Height) ("the capture is main's client size: {0}x{1}" -f $shot.Width, $shot.Height)
    Check (Near ($shot.GetPixel(90, 70)) 30 200 30) ("the menu shows: " + $shot.GetPixel(90, 70))
    Check (Near ($shot.GetPixel(270, 75)) 30 30 220) ("the alert shows: " + $shot.GetPixel(270, 75))
    Check (Near ($shot.GetPixel(90, 230)) 220 30 30) ("the layered window does not: " + $shot.GetPixel(90, 230))
    Check (Near ($shot.GetPixel(350, 255)) 220 30 30) ("the window below does not: " + $shot.GetPixel(350, 255))
    Check (Near ($shot.GetPixel(420, 155)) 220 30 30) ("the other process's window does not: " + $shot.GetPixel(420, 155))
    Check ($count -eq 2) ("two app windows composited: $count")
  } finally { $shot.Dispose() }

  # Main only.
  [void][YesDawDrive]::CaptureClient($main.Handle, $path, $false)
  $plain = [System.Drawing.Bitmap]::FromFile($path)
  try { Check (Near ($plain.GetPixel(90, 70)) 220 30 30) ("main only: the menu's place shows main: " + $plain.GetPixel(90, 70)) } finally { $plain.Dispose() }
} finally {
  if (-not $foreign.HasExited) { $foreign.Kill() }
  foreach ($f in @($menu, $alert, $layered, $below, $main)) { $f.Close(); $f.Dispose() }
  Remove-Item -LiteralPath $path -ErrorAction SilentlyContinue
}
if ($failures.Count -eq 0) { Write-Host 'PASS: the capture composites the app''s own popups and alerts, nothing else'; exit 0 }
Write-Host ("FAIL: {0} check(s)" -f $failures.Count) -ForegroundColor Red
exit 1
