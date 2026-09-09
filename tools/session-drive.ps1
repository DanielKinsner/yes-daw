# tools/session-drive.ps1 - the Session drive (ADR-0046 §10; plan §7.2). Windows only.
#
# Launches the REAL built YesDaw.exe, injects real Win32 mouse/keyboard input (SendInput), reads
# the State probe the shell writes when YESDAW_STATE_PROBE is set, screenshots the window, and
# asserts a numbered Session script. Exit 0 only when every Assert in the script passed.
# Mechanical in the ADR-0005 sense: it never asks a human to judge anything.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\session-drive.ps1 -SelfTest
#   powershell -ExecutionPolicy Bypass -File tools\session-drive.ps1 -Script tools\session-scripts\ss1-first-minute.ps1
#   ... [-Exe <path\YesDaw.exe>] [-Shots <dir>] [-Fixture <path.wav>] [-KeepOpen]
#
# A Session script is a PowerShell file dot-sourced into this scope. It uses these primitives
# (element ids come from the probe's `layout` map, so scripts click by NAME, never by pixel):
#   Step <n> "<title>"                 - names the step every following Assert belongs to
#   Launch [-Bundle <path.yesdaw>]     - start the exe (fresh session-state dir; probe on)
#   Focus                              - bring the window to the foreground
#   Click <elementId|"x,y"> [-Right] [-Double] [-Modifiers "Ctrl+Shift"] [-OffsetX n] [-OffsetY n]
#   Drag <from> <to> [-Modifiers ...]  - press at `from`, move in steps, release at `to`
#   DragWithin <id> fx fy tx ty [-Modifiers ...] - a drag inside one element, offsets from its centre
#   Key "<chord>" [-Repeat n]          - e.g. "Space", "Ctrl+Shift+I", "Alt+Right", "F2", "K"
#   TypeText "<text>"                  - unicode text into whatever has focus (not `Type`: a built-in alias)
#   FileDialogEnter "<path>"           - after WaitDialog: settle, select-all, type the path, Enter
#   Probe                              - the latest probe document (PSObject)
#   WaitProbe { <predicate on $p> } [-TimeoutMs n] - polls; returns $true/$false
#   Assert <bool> "<message>"          - records PASS/FAIL under the current step; continues
#   Shot "<name>"                      - PNG of the window's client area into -Shots
#   Resize <clientWidth> <clientHeight>
#   WaitDialog "<title contains>" [-TimeoutMs n] - a native dialog (file chooser) is up
#   Close                              - WM_CLOSE, then kill if a modal prompt holds it open
#   Elapsed                            - ms since Launch; $script:FirstProbeMs = ms to the first probe tick (B6)
#
# Coordinates: the probe publishes shell-local rects plus the shell's screen origin (`window`) and
# the display scale; the drive converts to physical pixels for SendInput.
param(
  [string] $Script = '',
  [switch] $SelfTest,
  [string] $Exe = '',
  [string] $Shots = '',
  [string] $Fixture = '',
  [switch] $KeepOpen
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($Exe)) { $Exe = Join-Path $root 'build-ci\YesDaw_artefacts\Release\YesDaw.exe' }
if ([string]::IsNullOrWhiteSpace($Shots)) { $Shots = Join-Path $root 'build-ci\session-shots' }
if ([string]::IsNullOrWhiteSpace($Fixture)) {
  # G0.6: the song fixture's first stem when it has been generated on this machine
  # (YesDawMakeSongFixture --out "$env:LOCALAPPDATA\YES DAW\fixtures"), else the sine fixture.
  $songStem = Join-Path $env:LOCALAPPDATA 'YES DAW\fixtures\stems\stem-01.wav'
  $Fixture = if (Test-Path -LiteralPath $songStem) { $songStem } else { Join-Path $root 'tests\fixtures\sine_440_48k_mono.wav' }
}
New-Item -ItemType Directory -Force -Path $Shots | Out-Null

# --- Win32 --------------------------------------------------------------------------------------
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
public static class YesDawDrive
{
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
    public delegate bool EnumProc(IntPtr h, IntPtr l);

    [DllImport("user32.dll", SetLastError = true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern uint MapVirtualKeyW(uint code, uint mapType);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h, EnumProc p, IntPtr l);
    [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr h);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr SendMessageTimeout(IntPtr h, uint msg, IntPtr w, StringBuilder text, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr SendMessageTimeout(IntPtr h, uint msg, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);
    [DllImport("kernel32.dll")] static extern void SetLastError(uint error);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll", SetLastError = true)] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW", SetLastError = true)] static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
    [StructLayout(LayoutKind.Sequential)] struct GUITHREADINFO { public uint size, flags; public IntPtr active, focus, capture, menuOwner, moveSize, caret; public RECT caretRect; }
    [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint thread, ref GUITHREADINFO info);

    public static bool DialogBelongsTo(IntPtr h, uint pid) {
        uint owner; GetWindowThreadProcessId(h, out owner);
        return h != IntPtr.Zero && IsWindowVisible(h) && owner == pid;
    }
    public static string CaptionDiagnostic = "not attempted";
    public static bool WindowIsTopmost(IntPtr window) { return (GetWindowLongPtr(window, -20).ToInt64() & 8) != 0; }
    public static bool SetTargetTopmost(IntPtr window, bool topmost) {
        return SetWindowPos(window, new IntPtr(topmost ? -1 : -2), 0, 0, 0, 0, 0x0013); // NOMOVE|NOSIZE|NOACTIVATE
    }
    static string captionFrame = "";
    static bool IsCaptionPoint(IntPtr window, POINT point) {
        IntPtr hitChild = WindowFromPoint(point), hitWindow = GetAncestor(hitChild, 2); // GA_ROOT
        CaptionDiagnostic = captionFrame + " point=" + point.X + "," + point.Y + " expected=" + window
            + " hitHwnd=" + hitChild + " hitRoot=" + hitWindow + " hitTitle='" + WindowTitle(hitWindow) + "'";
        if (hitWindow != window) { CaptionDiagnostic += " (occluded or outside app)"; return false; }
        IntPtr hit;
        long packed = (long)(uint)(((point.Y & 0xffff) << 16) | (point.X & 0xffff));
        bool replied = SendMessageTimeout(window, 0x0084, IntPtr.Zero, new IntPtr(packed), 2, 250, out hit) != IntPtr.Zero;
        CaptionDiagnostic += " hitTestReplied=" + replied + " hitCode=" + hit;
        return replied && hit.ToInt64() == 2; // WM_NCHITTEST, HTCAPTION
    }
    public static bool ActivateCaption(IntPtr window) {
        RECT frame; POINT client = new POINT();
        CaptionDiagnostic = "window/client frame unavailable";
        if (!GetWindowRect(window, out frame) || !ClientToScreen(window, ref client)) return false;
        captionFrame = "frame=" + frame.Left + "," + frame.Top + "," + frame.Right + "," + frame.Bottom
            + " clientOrigin=" + client.X + "," + client.Y;
        if (client.Y <= frame.Top) { CaptionDiagnostic = "no native caption above client: clientY=" + client.Y + " frameTop=" + frame.Top; return false; }
        var point = new POINT(); point.X = frame.Left + Math.Min(120, (frame.Right - frame.Left) / 4);
        point.Y = frame.Top + (client.Y - frame.Top) / 2;
        bool visibleCaption = false;
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (IsCaptionPoint(window, point)) { visibleCaption = true; break; }
            System.Threading.Thread.Sleep(50);
        }
        if (!visibleCaption) return false;
        MouseMoveAbs(point.X, point.Y);
        // Recheck immediately before down: refuse an occluded caption or any other app/control.
        if (!IsCaptionPoint(window, point)) return false;
        MouseButton(true, false);
        try { System.Threading.Thread.Sleep(30); } finally { MouseButton(false, false); }
        return true;
    }
    static string WindowClass(IntPtr h) { var text = new StringBuilder(256); GetClassName(h, text, text.Capacity); return text.ToString(); }
    static IntPtr FileNameControl(IntPtr dialog) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(dialog, delegate(IntPtr h, IntPtr unused) {
            if (!IsWindowVisible(h) || WindowClass(h) != "Edit") return true;
            // Observed Windows Common Item Dialog tree: its filename Edit is 1001 beneath
            // ComboBox -> FloatNotifySink -> DirectUIHWND. Address/search edits have distinct
            // ancestry; do not accept id 1001 alone (the address toolbar shares that id).
            IntPtr combo = GetParent(h), sink = GetParent(combo), directUi = GetParent(sink);
            if (GetDlgCtrlID(h) == 1001 && WindowClass(combo) == "ComboBox"
                && WindowClass(sink) == "FloatNotifySink" && WindowClass(directUi) == "DirectUIHWND") {
                found = h; return false;
            }
            // Common-dialog filename edit edt1 (1152), or an Edit under cmb13 (1148).
            // Never accept an arbitrary Edit: the address/search fields are different controls.
            for (IntPtr ancestor = h; ancestor != IntPtr.Zero && ancestor != dialog; ancestor = GetParent(ancestor)) {
                int id = GetDlgCtrlID(ancestor);
                if (id == 1152 || id == 1148) { found = h; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static bool FileNameHasFocus(IntPtr dialog) {
        uint pid; uint thread = GetWindowThreadProcessId(dialog, out pid);
        var info = new GUITHREADINFO(); info.size = (uint)Marshal.SizeOf(typeof(GUITHREADINFO));
        IntPtr edit = FileNameControl(dialog);
        return edit != IntPtr.Zero && GetForegroundWindow() == dialog && GetGUIThreadInfo(thread, ref info) && info.focus == edit;
    }
    public static bool FocusFileName(IntPtr dialog) {
        SetForegroundWindow(dialog);
        if (GetForegroundWindow() != dialog) return false;
        IntPtr edit = FileNameControl(dialog); RECT rect;
        if (edit == IntPtr.Zero || !GetWindowRect(edit, out rect)) return false;
        MouseMoveAbs((rect.Left + rect.Right) / 2, (rect.Top + rect.Bottom) / 2);
        MouseButton(true, false); MouseButton(false, false);
        return true; // caller waits for queued click, then verifies native focus before typing
    }
    public sealed class FileNameReadback {
        public IntPtr edit;
        public bool available;
        public string text;
        public long characters, elapsedMs;
        public int error;
        public override string ToString() {
            return "edit=" + edit + " available=" + available + " characters=" + characters
                + " elapsedMs=" + elapsedMs + " error=" + error
                + " text=" + (available ? "'" + text + "'" : "<unavailable>");
        }
    }
    public static FileNameReadback ReadFileName(IntPtr dialog) {
        IntPtr edit = FileNameControl(dialog), result;
        var observation = new FileNameReadback(); observation.edit = edit;
        if (edit == IntPtr.Zero) return observation;
        var text = new StringBuilder(32768);
        var clock = System.Diagnostics.Stopwatch.StartNew();
        // SendMessageTimeout may fail without setting last-error (generic failure).
        SetLastError(0);
        observation.available = SendMessageTimeout(edit, 0x000D, new IntPtr(text.Capacity), text, 2, 500, out result) != IntPtr.Zero;
        observation.error = Marshal.GetLastWin32Error();
        if (observation.available) observation.error = 0; // last-error is meaningful only on failure
        observation.elapsedMs = clock.ElapsedMilliseconds;
        observation.characters = result.ToInt64();
        observation.text = observation.available ? text.ToString() : null;
        return observation;
    }
    public static string FileNameText(IntPtr dialog) { return ReadFileName(dialog).text; }
    public static string DialogDiagnostic(IntPtr dialog) {
        uint pid; uint thread = GetWindowThreadProcessId(dialog, out pid);
        var info = new GUITHREADINFO(); info.size = (uint)Marshal.SizeOf(typeof(GUITHREADINFO)); GetGUIThreadInfo(thread, ref info);
        var children = new StringBuilder(); int count = 0;
        EnumChildWindows(dialog, delegate(IntPtr h, IntPtr unused) {
            if (!IsWindowVisible(h)) return true;
            children.Append(" [h=" + h + " parent=" + GetParent(h) + " id=" + GetDlgCtrlID(h) + " class=" + WindowClass(h) + " title='" + WindowTitle(h) + "']");
            return ++count < 48;
        }, IntPtr.Zero);
        return "hwnd=" + dialog + " title='" + WindowTitle(dialog) + "' foreground=" + GetForegroundWindow()
            + " focus=" + info.focus + " focusClass=" + WindowClass(info.focus) + " filename='" + FileNameText(dialog) + "' children=" + children;
    }
    static void Inject(INPUT[] inputs) {
        uint sent = SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(INPUT)));
        if (sent != inputs.Length) throw new InvalidOperationException("SendInput accepted " + sent + "/" + inputs.Length + " events; Win32=" + Marshal.GetLastWin32Error());
    }

    public static void MakeDpiAware() { try { SetProcessDpiAwarenessContext(new IntPtr(-4)); } catch (Exception) { } }

    public static IntPtr FindTopWindow(uint pid, string titleContains)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate (IntPtr h, IntPtr l)
        {
            if (!IsWindowVisible(h)) return true;
            uint owner; GetWindowThreadProcessId(h, out owner);
            if (pid != 0 && owner != pid) return true;
            StringBuilder sb = new StringBuilder(512);
            GetWindowTextW(h, sb, sb.Capacity);
            string title = sb.ToString();
            if (titleContains == null || titleContains.Length == 0 || title.IndexOf(titleContains, StringComparison.OrdinalIgnoreCase) >= 0)
            {
                found = h; return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static string WindowTitle(IntPtr h) { StringBuilder sb = new StringBuilder(512); GetWindowTextW(h, sb, sb.Capacity); return sb.ToString(); }

    static INPUT MouseInput(int dx, int dy, uint flags)
    {
        INPUT i = new INPUT(); i.type = 0; i.u.mi.dx = dx; i.u.mi.dy = dy; i.u.mi.dwFlags = flags; return i;
    }

    public static void MouseMoveAbs(int x, int y)
    {
        int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77), vw = GetSystemMetrics(78), vh = GetSystemMetrics(79);
        int nx = (int)Math.Round((x - vx) * 65535.0 / Math.Max(1, vw - 1));
        int ny = (int)Math.Round((y - vy) * 65535.0 / Math.Max(1, vh - 1));
        INPUT[] a = new INPUT[] { MouseInput(nx, ny, 0x0001 | 0x8000 | 0x4000) };   // MOVE|ABSOLUTE|VIRTUALDESK
        Inject(a);
    }

    public static void MouseButton(bool down, bool right)
    {
        uint flag = right ? (down ? 0x0008u : 0x0010u) : (down ? 0x0002u : 0x0004u);
        INPUT[] a = new INPUT[] { MouseInput(0, 0, flag) };
        Inject(a);
    }

    public static void KeyEvent(ushort vk, bool down, bool extended)
    {
        INPUT i = new INPUT(); i.type = 1; i.u.ki.wVk = vk; i.u.ki.wScan = (ushort)MapVirtualKeyW(vk, 0);
        i.u.ki.dwFlags = (down ? 0u : 0x0002u) | (extended ? 0x0001u : 0u);
        INPUT[] a = new INPUT[] { i };
        Inject(a);
    }

    public static void UnicodeChar(char c)
    {
        INPUT d = new INPUT(); d.type = 1; d.u.ki.wScan = c; d.u.ki.dwFlags = 0x0004u;
        INPUT u = new INPUT(); u.type = 1; u.u.ki.wScan = c; u.u.ki.dwFlags = 0x0004u | 0x0002u;
        INPUT[] a = new INPUT[] { d, u };
        Inject(a);
    }

    // Client-area PNG of a window. PrintWindow with PW_RENDERFULLCONTENT (2) renders
    // GPU-composed content; falls back to a screen copy when PrintWindow refuses.
    public static void CaptureClient(IntPtr h, string path)
    {
        RECT wr; GetWindowRect(h, out wr);
        RECT cr; GetClientRect(h, out cr);
        POINT origin = new POINT(); origin.X = 0; origin.Y = 0; ClientToScreen(h, ref origin);
        int ww = Math.Max(1, wr.Right - wr.Left), wh = Math.Max(1, wr.Bottom - wr.Top);
        int cw = Math.Max(1, cr.Right - cr.Left), ch = Math.Max(1, cr.Bottom - cr.Top);
        using (Bitmap whole = new Bitmap(ww, wh, PixelFormat.Format32bppArgb))
        {
            bool ok;
            using (Graphics g = Graphics.FromImage(whole))
            {
                IntPtr hdc = g.GetHdc();
                ok = PrintWindow(h, hdc, 2);
                g.ReleaseHdc(hdc);
                if (!ok)
                    g.CopyFromScreen(wr.Left, wr.Top, 0, 0, new Size(ww, wh));
            }
            Rectangle client = new Rectangle(origin.X - wr.Left, origin.Y - wr.Top, cw, ch);
            client.Intersect(new Rectangle(0, 0, ww, wh));
            using (Bitmap clipped = whole.Clone(client, PixelFormat.Format32bppArgb))
                clipped.Save(path, ImageFormat.Png);
        }
    }
}
'@
[YesDawDrive]::MakeDpiAware()

# --- Drive state ----------------------------------------------------------------------------------
$script:Results = New-Object System.Collections.Generic.List[object]
$script:CurrentStep = 'setup'
$script:Proc = $null
$script:Hwnd = [IntPtr]::Zero
$script:ProbePath = ''
$script:SessionDir = ''
$script:LaunchStamp = $null
$script:LastProbe = $null
$script:FirstProbeMs = -1

function Step([int] $n, [string] $title) {
  $script:CurrentStep = ('{0}. {1}' -f $n, $title)
  Write-Host ("`n== Step {0}" -f $script:CurrentStep)
}

function Assert([bool] $condition, [string] $message) {
  $verdict = if ($condition) { 'PASS' } else { 'FAIL' }
  $script:Results.Add([pscustomobject]@{ Step = $script:CurrentStep; Verdict = $verdict; Message = $message })
  $colour = if ($condition) { 'Green' } else { 'Red' }
  Write-Host ("  [{0}] {1}" -f $verdict, $message) -ForegroundColor $colour
  return $condition
}

function Elapsed { if ($script:LaunchStamp) { return [int]((Get-Date) - $script:LaunchStamp).TotalMilliseconds } else { return 0 } }

function Probe {
  for ($attempt = 0; $attempt -lt 25; $attempt++) {
    try {
      if (Test-Path -LiteralPath $script:ProbePath) {
        $text = [System.IO.File]::ReadAllText($script:ProbePath)
        if ($text.Length -gt 2) {
          $script:LastProbe = $text | ConvertFrom-Json
          return $script:LastProbe
        }
      }
    } catch { }
    Start-Sleep -Milliseconds 20
  }
  return $script:LastProbe
}

function WaitProbe([scriptblock] $predicate, [int] $TimeoutMs = 3000) {
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  do {
    $p = Probe
    if ($null -ne $p) {
      try { if (& $predicate $p) { return $true } } catch { }
    }
    Start-Sleep -Milliseconds 40
  } while ((Get-Date) -lt $deadline)
  return $false
}

function LayoutRect([string] $id) {
  $p = Probe
  if ($null -eq $p -or $null -eq $p.layout) { return $null }
  $r = $p.layout.PSObject.Properties[$id]
  if ($null -eq $r) { return $null }
  return $r.Value
}

function ScreenPoint([string] $target, [int] $OffsetX = 0, [int] $OffsetY = 0) {
  $p = Probe
  if ($target -match '^\s*(-?\d+)\s*,\s*(-?\d+)\s*$') {
    $lx = [int]$matches[1]; $ly = [int]$matches[2]
  } else {
    $r = LayoutRect $target
    if ($null -eq $r) { throw "element '$target' is not in the probe layout map" }
    $lx = [int]($r[0] + $r[2] / 2 + $OffsetX)
    $ly = [int]($r[1] + $r[3] / 2 + $OffsetY)
  }
  # Physical origin of the client area from Win32 (the shell is the content component and fills
  # the client area under the native title bar); only the shell-local offset is scaled.
  $scale = [double]$p.displayScale
  if ($scale -le 0) { $scale = 1.0 }
  $origin = New-Object YesDawDrive+POINT
  [void][YesDawDrive]::ClientToScreen($script:Hwnd, [ref]$origin)
  $sx = [int][Math]::Round($origin.X + $lx * $scale)
  $sy = [int][Math]::Round($origin.Y + $ly * $scale)
  return @($sx, $sy)
}

$script:VkMap = @{
  'SPACE' = 0x20; 'ENTER' = 0x0D; 'RETURN' = 0x0D; 'ESC' = 0x1B; 'ESCAPE' = 0x1B; 'TAB' = 0x09;
  'BACKSPACE' = 0x08; 'DEL' = 0x2E; 'DELETE' = 0x2E; 'HOME' = 0x24; 'END' = 0x23;
  'LEFT' = 0x25; 'UP' = 0x26; 'RIGHT' = 0x27; 'DOWN' = 0x28; 'INS' = 0x2D;
  ',' = 0xBC; '.' = 0xBE; '+' = 0xBB; '-' = 0xBD; '/' = 0xBF; '[' = 0xDB; ']' = 0xDD; ';' = 0xBA; "'" = 0xDE; '\' = 0xDC; '`' = 0xC0; '=' = 0xBB
}
$script:ExtendedVk = @(0x2E, 0x24, 0x23, 0x25, 0x26, 0x27, 0x28, 0x2D)

function VkFor([string] $name) {
  $n = $name.Trim().ToUpperInvariant()
  if ($script:VkMap.ContainsKey($n)) { return [int]$script:VkMap[$n] }
  if ($n -match '^F(\d{1,2})$') { return 0x70 + [int]$matches[1] - 1 }
  if ($n.Length -eq 1) {
    $c = [int][char]$n
    if (($c -ge 0x30 -and $c -le 0x39) -or ($c -ge 0x41 -and $c -le 0x5A)) { return $c }
  }
  throw "unknown key name '$name'"
}

function ModifierVks([string] $modifiers) {
  $vks = @()
  if ([string]::IsNullOrWhiteSpace($modifiers)) { return $vks }
  foreach ($m in $modifiers.Split('+')) {
    switch ($m.Trim().ToUpperInvariant()) {
      'CTRL'    { $vks += 0x11 }
      'CONTROL' { $vks += 0x11 }
      'SHIFT'   { $vks += 0x10 }
      'ALT'     { $vks += 0x12 }
      ''        { }
      default   { throw "unknown modifier '$m'" }
    }
  }
  return $vks
}

function Key([string] $chord, [int] $Repeat = 1) {
  $parts = @($chord.Split('+') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
  # a trailing '+' key ("Ctrl++") arrives as an empty last part; treat it as the '+' key
  if ($chord.EndsWith('+') -and $parts.Count -gt 0 -and ($parts[-1] -in @('Ctrl','Shift','Alt','Control'))) { $parts += '+' }
  $keyName = $parts[-1]
  $mods = @()
  if ($parts.Count -gt 1) { $mods = ModifierVks (($parts[0..($parts.Count - 2)]) -join '+') }
  $vk = VkFor $keyName
  $ext = $script:ExtendedVk -contains $vk
  # JUCE reads modifier state with GetAsyncKeyState (the PHYSICAL state at processing time), not
  # from the message queue — so a modifier must still be held when the app gets round to the key
  # message. Hold it for a beat on both sides of the key or "Ctrl+N" arrives as "N".
  # A repeat burst holds the modifier ONCE across all presses (a user holds Alt and taps Right),
  # so fifty nudges take about a second, not a minute of modifier settling.
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  for ($i = 0; $i -lt $Repeat; $i++) {
    [YesDawDrive]::KeyEvent([uint16]$vk, $true, $ext)
    [YesDawDrive]::KeyEvent([uint16]$vk, $false, $ext)
    Start-Sleep -Milliseconds 15
  }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  foreach ($m in ($mods | Sort-Object -Descending)) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
}

function TypeText([string] $text) {
  foreach ($ch in $text.ToCharArray()) { [YesDawDrive]::UnicodeChar($ch); Start-Sleep -Milliseconds 5 }
}

# Drive the exact native chooser observed by WaitDialog. Readback verifies input delivery;
# the Session script still verifies the resulting app state independently.
function FileDialogEnter([string] $path, [string] $DialogTitle = '') {
  if ([string]::IsNullOrWhiteSpace($path)) { throw 'Native chooser requires a nonempty requested path' }
  $dialog = $script:LastDialog
  if ($DialogTitle) { $dialog = WaitDialog $DialogTitle }
  if (-not [YesDawDrive]::DialogBelongsTo($dialog, [uint32]$script:Proc.Id)) { throw 'The exact chooser returned by WaitDialog is no longer available' }
  Start-Sleep -Milliseconds 1200
  if (-not [YesDawDrive]::FocusFileName($dialog)) { throw ('Cannot focus native filename control: ' + [YesDawDrive]::DialogDiagnostic($dialog)) }
  Start-Sleep -Milliseconds 100
  if (-not [YesDawDrive]::FileNameHasFocus($dialog)) { throw ('Native filename focus was not established: ' + [YesDawDrive]::DialogDiagnostic($dialog)) }
  Key 'Ctrl+A'
  Start-Sleep -Milliseconds 100
  TypeText $path
  Start-Sleep -Milliseconds 300
  $readbackFocus = [YesDawDrive]::FileNameHasFocus($dialog)
  $readback = [YesDawDrive]::ReadFileName($dialog)
  if (-not $readbackFocus -or -not $readback.available -or $readback.text -cne $path) {
    throw ("Native filename readback did not match requested path: observedFocus=$readbackFocus observed=[$readback] expected='$path'; later state: " + [YesDawDrive]::DialogDiagnostic($dialog))
  }
  Write-Host ('  [dialog] verified filename focus/readback: hwnd=' + $dialog + ' title=' + [YesDawDrive]::WindowTitle($dialog) + ' path=' + $path)
  Key 'Enter'
  # Preserve the previous two 900 ms completion windows; never retype into an unknown control.
  $deadline = (Get-Date).AddMilliseconds(1800)
  do {
    if (-not [YesDawDrive]::DialogBelongsTo($dialog, [uint32]$script:Proc.Id)) {
      $script:LastDialog = [IntPtr]::Zero
      Write-Host '  [dialog] exact chooser closed; app-state assertion follows'
      return
    }
    Start-Sleep -Milliseconds 60
  } while ((Get-Date) -lt $deadline)
  throw ('Native chooser remained open after confirmed path input: ' + [YesDawDrive]::DialogDiagnostic($dialog))
}

function ActivateAppCaption {
  # Only the app's own topmost bit changes, temporarily. Always restore it, including refused
  # hit tests or rejected input. Other windows and system foreground settings are untouched.
  $wasTopmost = [YesDawDrive]::WindowIsTopmost($script:Hwnd)
  try {
    if (-not [YesDawDrive]::SetTargetTopmost($script:Hwnd, $true)) { return $false }
    return [YesDawDrive]::ActivateCaption($script:Hwnd)
  } finally {
    if (-not [YesDawDrive]::SetTargetTopmost($script:Hwnd, $wasTopmost) -or [YesDawDrive]::WindowIsTopmost($script:Hwnd) -ne $wasTopmost) {
      throw 'Could not restore the YES DAW window topmost state after activation'
    }
  }
}

function Focus {
  if ($script:Hwnd -eq [IntPtr]::Zero) { throw 'Could not activate the YES DAW window: no window handle' }
  [void][YesDawDrive]::ShowWindow($script:Hwnd, 9)   # SW_RESTORE
  # Windows may initially deny foreground activation while a newly shown window settles.
  # Retry only activation of the known app window. An exhausted retry is a harness/environment
  # failure, not a lost app chord. A verified caption click below handles foreground-lock denial.
  for ($attempt = 0; $attempt -lt 8; $attempt++) {
    [void][YesDawDrive]::SetForegroundWindow($script:Hwnd)
    Start-Sleep -Milliseconds 120
    if ([YesDawDrive]::GetForegroundWindow() -eq $script:Hwnd) { return }
  }
  if (ActivateAppCaption) {
    Start-Sleep -Milliseconds 120
    if ([YesDawDrive]::GetForegroundWindow() -eq $script:Hwnd) {
      Write-Host '  [focus] activated the verified app caption after foreground-lock denial'
      return
    }
  }
  throw ('Could not activate the YES DAW window: expected hwnd=' + $script:Hwnd + ' actual foreground=' + [YesDawDrive]::GetForegroundWindow() + ' caption=' + [YesDawDrive]::CaptionDiagnostic)
}

function Click([string] $target, [switch] $Right, [switch] $Double, [string] $Modifiers = '', [int] $OffsetX = 0, [int] $OffsetY = 0) {
  $pt = ScreenPoint $target $OffsetX $OffsetY
  $mods = ModifierVks $Modifiers
  [YesDawDrive]::MouseMoveAbs($pt[0], $pt[1]); Start-Sleep -Milliseconds 40
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  $count = if ($Double) { 2 } else { 1 }
  for ($i = 0; $i -lt $count; $i++) {
    [YesDawDrive]::MouseButton($true, [bool]$Right); Start-Sleep -Milliseconds 30
    [YesDawDrive]::MouseButton($false, [bool]$Right); Start-Sleep -Milliseconds 40
  }
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
  Start-Sleep -Milliseconds 80
}

function Drag([string] $from, [string] $to, [string] $Modifiers = '', [int] $Steps = 12) {
  $a = ScreenPoint $from; $b = ScreenPoint $to
  $mods = ModifierVks $Modifiers
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  [YesDawDrive]::MouseMoveAbs($a[0], $a[1]); Start-Sleep -Milliseconds 40
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 60
  for ($i = 1; $i -le $Steps; $i++) {
    $x = [int]($a[0] + ($b[0] - $a[0]) * $i / $Steps)
    $y = [int]($a[1] + ($b[1] - $a[1]) * $i / $Steps)
    [YesDawDrive]::MouseMoveAbs($x, $y); Start-Sleep -Milliseconds 25
  }
  [YesDawDrive]::MouseButton($false, $false)
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
  Start-Sleep -Milliseconds 100
}

# G3.1: a drag INSIDE one element — press at (fromX, fromY) offsets from the element's centre, move in
# steps, release at (toX, toY) — with optional modifiers held throughout (Ctrl-defeat, Ctrl+Alt slip,
# Shift loop drag). The same input law as Drag, for gestures that start and end in the same rect.
function DragWithin([string] $target, [int] $fromX, [int] $fromY, [int] $toX, [int] $toY, [string] $Modifiers = '', [int] $Steps = 12) {
  $a = ScreenPoint $target $fromX $fromY; $b = ScreenPoint $target $toX $toY
  $mods = ModifierVks $Modifiers
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  [YesDawDrive]::MouseMoveAbs($a[0], $a[1]); Start-Sleep -Milliseconds 40
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 60
  for ($i = 1; $i -le $Steps; $i++) {
    $x = [int]($a[0] + ($b[0] - $a[0]) * $i / $Steps)
    $y = [int]($a[1] + ($b[1] - $a[1]) * $i / $Steps)
    [YesDawDrive]::MouseMoveAbs($x, $y); Start-Sleep -Milliseconds 25
  }
  [YesDawDrive]::MouseButton($false, $false)
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
  Start-Sleep -Milliseconds 100
}

function Shot([string] $name) {
  $path = Join-Path $Shots ($name + '.png')
  try {
    [YesDawDrive]::CaptureClient($script:Hwnd, $path)
    Write-Host ("  [shot] {0}" -f $path)
  } catch {
    Write-Host ("  [shot] FAILED {0}: {1}" -f $path, $_.Exception.Message) -ForegroundColor Yellow
  }
  return $path
}

function Resize([int] $clientWidth, [int] $clientHeight) {
  $wr = New-Object YesDawDrive+RECT; [void][YesDawDrive]::GetWindowRect($script:Hwnd, [ref]$wr)
  $cr = New-Object YesDawDrive+RECT; [void][YesDawDrive]::GetClientRect($script:Hwnd, [ref]$cr)
  $p = Probe
  $scale = [double]$p.displayScale; if ($scale -le 0) { $scale = 1.0 }
  $frameW = ($wr.Right - $wr.Left) - ($cr.Right - $cr.Left)
  $frameH = ($wr.Bottom - $wr.Top) - ($cr.Bottom - $cr.Top)
  $w = [int]([Math]::Round($clientWidth * $scale)) + $frameW
  $h = [int]([Math]::Round($clientHeight * $scale)) + $frameH
  [void][YesDawDrive]::MoveWindow($script:Hwnd, 0, 0, $w, $h, $true)
  $ok = WaitProbe { param($q) [int]$q.view.width -eq $clientWidth -and [int]$q.view.height -eq $clientHeight } -TimeoutMs 4000
  if (-not $ok) { Write-Host ("  [resize] probe did not settle at {0}x{1} (got {2}x{3})" -f $clientWidth, $clientHeight, $script:LastProbe.view.width, $script:LastProbe.view.height) -ForegroundColor Yellow }
  Start-Sleep -Milliseconds 300
  return $ok
}

function WaitDialog([string] $titleContains, [int] $TimeoutMs = 4000) {
  $script:LastDialog = [IntPtr]::Zero
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  do {
    $h = [YesDawDrive]::FindTopWindow([uint32]$script:Proc.Id, $titleContains)
    if ($h -ne [IntPtr]::Zero -and $h -ne $script:Hwnd) { $script:LastDialog = $h; return $h }
    Start-Sleep -Milliseconds 60
  } while ((Get-Date) -lt $deadline)
  return [IntPtr]::Zero
}

function AssertStartupBudget([int] $Milliseconds) {
  [void](Assert ($Milliseconds -ge 0 -and $Milliseconds -le 3000) ("launch to first interactive tick <= 3 s (B6): $Milliseconds ms"))
}

function Launch([string] $Bundle = '', [string] $ReuseSessionDir = '') {
  if (-not (Test-Path -LiteralPath $Exe)) { throw "exe not found: $Exe (build first)" }
  $others = @(Get-Process -Name 'YesDaw' -ErrorAction SilentlyContinue)
  if ($others.Count -gt 0) { throw "another YesDaw.exe is running (pid $($others[0].Id)); the shell is single-instance (R9) - close it first" }

  # G3.1 (SS-2 step 7): a relaunch may REUSE the previous session-state dir so persisted session
  # state (the keymap overrides, the last-project record) survives the way a real relaunch keeps it.
  if (-not [string]::IsNullOrWhiteSpace($ReuseSessionDir)) {
    $script:SessionDir = $ReuseSessionDir
  } else {
    $stamp = (Get-Date).ToString('yyyyMMdd-HHmmss-fff')
    $script:SessionDir = Join-Path ([System.IO.Path]::GetTempPath()) ("yesdaw-drive-" + $stamp)
  }
  New-Item -ItemType Directory -Force -Path $script:SessionDir | Out-Null
  $script:ProbePath = Join-Path $script:SessionDir 'probe.json'
  # Reusing session state preserves the keymap/last-project record, never an earlier process's
  # probe. Otherwise WaitProbe can certify launch against stale geometry before this exe starts.
  if (Test-Path -LiteralPath $script:ProbePath) { Remove-Item -LiteralPath $script:ProbePath -Force }
  $script:LastProbe = $null
  $env:YESDAW_STATE_PROBE = $script:ProbePath
  $env:YESDAW_SESSION_STATE_DIR = $script:SessionDir

  $args = @()
  if (-not [string]::IsNullOrWhiteSpace($Bundle)) { $args = @(('"' + $Bundle + '"')) }
  $script:LaunchStamp = Get-Date
  if ($args.Count -gt 0) { $script:Proc = Start-Process -FilePath $Exe -ArgumentList $args -PassThru }
  else { $script:Proc = Start-Process -FilePath $Exe -PassThru }

  $ok = WaitProbe { param($q) $null -ne $q.window -and [int]$q.window[2] -gt 0 } -TimeoutMs 20000
  if (-not $ok) { throw "no State probe appeared at $($script:ProbePath) within 20 s (is YESDAW_STATE_PROBE honoured by this exe?)" }
  $script:FirstProbeMs = Elapsed   # B6: launch -> first interactive tick
  AssertStartupBudget $script:FirstProbeMs
  $deadline = (Get-Date).AddSeconds(10)
  do {
    $script:Hwnd = [YesDawDrive]::FindTopWindow([uint32]$script:Proc.Id, 'YES DAW')
    if ($script:Hwnd -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 100
  } while ((Get-Date) -lt $deadline)
  if ($script:Hwnd -eq [IntPtr]::Zero) { throw "the YES DAW window did not appear" }
  Focus
  Write-Host ("  [launch] pid {0} probe {1} first-probe {2} ms renderer {3}" -f $script:Proc.Id, $script:ProbePath, $script:FirstProbeMs, $script:LastProbe.renderer)
}

function Close {
  if ($null -eq $script:Proc) { return }
  if ($KeepOpen) { Write-Host "  [close] -KeepOpen: leaving the app running"; return }
  try {
    if ($script:Hwnd -ne [IntPtr]::Zero) { [void][YesDawDrive]::PostMessage($script:Hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $script:Proc.WaitForExit(3000)) {
      Write-Host "  [close] still running after WM_CLOSE (a modal prompt?) - killing" -ForegroundColor Yellow
      Stop-Process -Id $script:Proc.Id -Force -ErrorAction SilentlyContinue
      [void]$script:Proc.WaitForExit(3000)
    }
  } catch { }
  $script:Proc = $null
  $script:Hwnd = [IntPtr]::Zero
  Remove-Item Env:\YESDAW_STATE_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:\YESDAW_SESSION_STATE_DIR -ErrorAction SilentlyContinue
}

# --- Run ------------------------------------------------------------------------------------------
$exitCode = 1
try {
  if ($SelfTest) {
    Step 0 'session-drive self-test'
    Launch
    $p = Probe
    [void](Assert ([int]$p.version -eq 1) 'probe schema version is 1')
    [void](Assert ($p.focusContext -eq 'Arrange') 'focus context is Arrange at launch')
    [void](Assert ($null -ne (LayoutRect 'widget.transport.play')) 'layout publishes widget.transport.play')
    [void](Assert ($null -ne (LayoutRect 'timeline')) 'layout publishes the timeline panel')
    [void](Assert ([int]$p.audio.callbackRemovals -eq 0) 'no audio-callback removals at launch')
    $shot = Shot 'selftest'
    [void](Assert ((Test-Path -LiteralPath $shot) -and ((Get-Item -LiteralPath $shot).Length -gt 1024)) 'screenshot written')
    Close
  } elseif (-not [string]::IsNullOrWhiteSpace($Script)) {
    $scriptPath = if ([System.IO.Path]::IsPathRooted($Script)) { $Script } else { Join-Path $root $Script }
    if (-not (Test-Path -LiteralPath $scriptPath)) { throw "script not found: $scriptPath" }
    Write-Host ("[drive] script {0}" -f $scriptPath)
    . $scriptPath
  } else {
    Write-Host 'usage: session-drive.ps1 -SelfTest | -Script <path> [-Exe] [-Shots] [-Fixture] [-KeepOpen]'
    exit 2
  }
} catch {
  $script:Results.Add([pscustomobject]@{ Step = $script:CurrentStep; Verdict = 'FAIL'; Message = ('drive error: ' + $_.Exception.Message) })
  Write-Host ("  [drive] error: {0}" -f $_.Exception.Message) -ForegroundColor Red
  Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray
} finally {
  try { Close } catch { }
}

Write-Host "`n== Session drive summary"
$script:Results | Format-Table -AutoSize Step, Verdict, Message | Out-String -Width 200 | Write-Host
$failed = @($script:Results | Where-Object { $_.Verdict -eq 'FAIL' }).Count
$passed = @($script:Results | Where-Object { $_.Verdict -eq 'PASS' }).Count
if ($failed -eq 0 -and $passed -gt 0) { Write-Host ("PASS: {0} assertions" -f $passed) -ForegroundColor Green; $exitCode = 0 }
else { Write-Host ("FAIL: {0} failed, {1} passed" -f $failed, $passed) -ForegroundColor Red; $exitCode = 1 }
exit $exitCode
