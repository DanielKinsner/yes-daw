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
#   Launch [-Bundle <path.yesdaw>] [-ReuseSessionDir <dir>] [-AutosaveIntervalMs n] [-ExportPaceMs n]
#                                      - start the exe (fresh session-state dir unless reused; probe on; a shorter
#                                        autosave cadence for the lifecycle drive, 250 .. 600000 ms; an export pause
#                                        after the first chunk so a drive acts mid-job, 250 .. 60000 ms)
#   Focus                              - bring the window to the foreground
#   Click <elementId|"x,y"> [-Right] [-Double] [-Modifiers "Ctrl+Shift"] [-OffsetX n] [-OffsetY n]
#   Drag <from> <to> [-Modifiers ...]  - press at `from`, move in steps, release at `to`
#   DragWithin <id> fx fy tx ty [-Modifiers ...] - a drag inside one element, offsets from its centre
#   Key "<chord>" [-Repeat n]          - e.g. "Space", "Ctrl+Shift+I", "Alt+Right", "F2", "K"
#   KeyWhileBusy "<chord>" [-ModifierAfter] - the chord queued behind a held UI thread (ADR-0057)
#   TypeText "<text>"                  - unicode text into whatever has focus (not `Type`: a built-in alias)
#   FileDialogEnter "<path>"           - after WaitDialog: settle, select-all, type the path, Enter
#   Probe                              - the latest probe document (PSObject)
#   WaitProbe { <predicate on $p> } [-TimeoutMs n] - polls; returns $true/$false
#   Assert <bool> "<message>"          - records PASS/FAIL under the current step; continues
#   Shot "<name>"                      - PNG of the window's client area into -Shots
#   Resize <clientWidth> <clientHeight>
#   WaitDialog "<title contains>" [-TimeoutMs n] - a native dialog (file chooser) is up
#   NewProjectChooser [-ViaKey]        - File > New (button, or Ctrl+N): the New Project dialog, its Create, then
#                                        the native location chooser; returns the chooser's hwnd (Zero = none)
#   MenuPickByName "<item text>" [-Depth n] - the open JUCE popup's item by its text (UI Automation), clicked
#   UiaFocused                         - the element UI Automation reports focused (what a screen reader reads)
#   UiaFind "<name -like>" [-Type t]   - the app's UI Automation elements by name (t: Button, CheckBox, Slider...)
#   WaitPopup [-Depth n] [-TimeoutMs n] - n JUCE popup menus are modal (keys now reach the popup)
#   Close                              - WM_CLOSE, then kill if a modal prompt holds it open
#   KillApp                            - terminate the process at once (an interruption: no close path runs; not
#                                        `Kill`: PowerShell resolves that to its Stop-Process alias first)
#   FileDrop <elementId> <paths[]> [-OffsetX n] [-OffsetY n] - a real OLE file drag from outside the app, dropped
#                                        at the element (what dragging files from Explorer does); returns the effect
#   WaitAlert "<title contains>" [-TimeoutMs n] - a JUCE alert window of the app is up (UI Automation element)
#   AlertButton "<text>" [-Title t]    - click the open alert's button by its text (the real mouse)
#   AlertText "<text>" [-Title t]      - type into the open alert's text field (click it, select all, type)
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

    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenThread(uint access, bool inherit, uint threadId);
    [DllImport("kernel32.dll", SetLastError = true)] static extern int SuspendThread(IntPtr thread);
    [DllImport("kernel32.dll", SetLastError = true)] static extern int ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);

    // ADR-0057: hold the app's UI thread (the window's thread) still, so keys sent now queue exactly as they
    // do behind a busy UI. Returns the thread handle; ResumeUiThread must follow (in a finally).
    public static IntPtr SuspendUiThread(IntPtr window)
    {
        uint pid; uint tid = GetWindowThreadProcessId(window, out pid);
        if (tid == 0) throw new InvalidOperationException("no UI thread for window " + window);
        IntPtr thread = OpenThread(0x0002, false, tid);   // THREAD_SUSPEND_RESUME
        if (thread == IntPtr.Zero) throw new InvalidOperationException("OpenThread failed; Win32=" + Marshal.GetLastWin32Error());
        if (SuspendThread(thread) < 0)
        {
            int error = Marshal.GetLastWin32Error();
            CloseHandle(thread);
            throw new InvalidOperationException("SuspendThread failed; Win32=" + error);
        }
        return thread;
    }

    // The suspend count before this resume: 1 means the thread was held once, by us, and runs again.
    public static int ResumeUiThread(IntPtr thread)
    {
        int previous = ResumeThread(thread);
        CloseHandle(thread);
        return previous;
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
  # 2026-10-05: every FAIL carries the app's state at that moment, so an unexplained failure is
  # diagnosable from its first occurrence (several raw FAILs tonight could only be reasoned about).
  if (-not $condition -and $script:ProbePath -and (Test-Path -LiteralPath $script:ProbePath)) {
    try {
      $q = Probe
      $ctx = '    [ctx] tick=' + $q.tick + ' focusContext=' + $q.focusContext + ' focusOwner=' + $q.focusOwner
      $ctx += ' lastAction=' + $q.lastAction + ' dispatches=' + $q.commandDispatchCount + ' tool=' + $q.view.tool
      $ctx += ' modalMenus=' + $q.modal.menus + ' navigating=' + $q.controlTarget.navigating + ' target=' + $q.controlTarget.id
      $ctx += ' foreground=' + ([YesDawDrive]::GetForegroundWindow() -eq $script:Hwnd)
      Write-Host $ctx
    } catch { Write-Host ('    [ctx] unavailable: ' + $_.Exception.Message) }
  }
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
  # ADR-0057 fixed that in the app for the shell's chords (they read the key-time state); the holds
  # and the idle wait below stay: JUCE popups and text fields still read the physical state, and
  # they keep every drive deterministic. KeyWhileBusy is the queued case on purpose.
  # A repeat burst holds the modifier ONCE across all presses (a user holds Alt and taps Right),
  # so fifty nudges take about a second, not a minute of modifier settling.
  # A chord also waits for the app to be idle first (its UI tick advances twice): a key that sits in the queue
  # while the app finishes the previous action (an engine rebuild after a knob drag) would otherwise be read
  # after the modifier is up — batch 2026-10-06-g46-final saw "Ctrl+Z" arrive as "Z" (zoom to selection).
  if ($mods.Count -gt 0) {
    $before = Probe
    if ($null -ne $before -and $null -ne $before.tick) {
      $t0 = [int64]$before.tick
      [void](WaitProbe { param($q) [int64]$q.tick -ge $t0 + 2 } -TimeoutMs 1500)
    }
  }
  foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 40 }
  for ($i = 0; $i -lt $Repeat; $i++) {
    [YesDawDrive]::KeyEvent([uint16]$vk, $true, $ext)
    [YesDawDrive]::KeyEvent([uint16]$vk, $false, $ext)
    Start-Sleep -Milliseconds 15
  }
  if ($mods.Count -gt 0) { Start-Sleep -Milliseconds 80 }
  foreach ($m in ($mods | Sort-Object -Descending)) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
}

# ADR-0057: a chord that waits behind a busy UI, on purpose. The app's UI thread is held still, the whole
# chord goes in and every key comes back up, then the thread runs and finds the chord waiting in its queue
# with the modifiers already released. -ModifierAfter is the other direction: the key goes in bare and the
# modifier goes down only after it (still while the thread is held) and is released once the app has read
# the key. The thread is always resumed.
function KeyWhileBusy([string] $chord, [switch] $ModifierAfter) {
  $parts = @($chord.Split('+') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
  if ($parts.Count -lt 2) { throw "KeyWhileBusy needs a modifier chord, not '$chord'" }
  $mods = @(ModifierVks (($parts[0..($parts.Count - 2)]) -join '+'))
  $vk = VkFor $parts[-1]
  $ext = $script:ExtendedVk -contains $vk
  $thread = [YesDawDrive]::SuspendUiThread($script:Hwnd)
  $previous = -1
  try {
    if (-not $ModifierAfter) { foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) } }
    [YesDawDrive]::KeyEvent([uint16]$vk, $true, $ext)
    [YesDawDrive]::KeyEvent([uint16]$vk, $false, $ext)
    if ($ModifierAfter) { foreach ($m in $mods) { [YesDawDrive]::KeyEvent([uint16]$m, $true, $false) } }
    else { foreach ($m in ($mods | Sort-Object -Descending)) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) } }
    Start-Sleep -Milliseconds 60
  } finally {
    $previous = [YesDawDrive]::ResumeUiThread($thread)
    if ($ModifierAfter) {
      # the app reads the queued key while the modifier is still down; then it comes up
      $t0 = [int64](Probe).tick
      [void](WaitProbe { param($q) [int64]$q.tick -ge $t0 + 2 } -TimeoutMs 1500)
      foreach ($m in ($mods | Sort-Object -Descending)) { [YesDawDrive]::KeyEvent([uint16]$m, $false, $false) }
    }
  }
  if ($previous -ne 1) { throw "the UI thread's suspend count before resume was $previous (expected 1)" }
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
  # The Common Item Dialog builds its tree after it shows, and a fresh dialog can lose the foreground
  # race: the 2026-10-05 ss6 second pad load found only the outer DUIViewWndClassName and another
  # foreground window at 1.2 s. Establish VERIFIED focus on the filename control — re-attempting the
  # precondition, never the text — for up to five more 400 ms waits before refusing.
  $clicked = $false
  $focused = $false
  for ($attempt = 0; $attempt -lt 6 -and -not $focused; $attempt++) {
    if ($attempt -gt 0) { Start-Sleep -Milliseconds 400 }
    if (-not [YesDawDrive]::FocusFileName($dialog)) { continue }
    $clicked = $true
    Start-Sleep -Milliseconds 100
    $focused = [YesDawDrive]::FileNameHasFocus($dialog)
  }
  if (-not $clicked) { throw ('Cannot focus native filename control: ' + [YesDawDrive]::DialogDiagnostic($dialog)) }
  if (-not $focused) { throw ('Native filename focus was not established: ' + [YesDawDrive]::DialogDiagnostic($dialog)) }
  # The 2026-10-06 ss1 import read back 'sers\...' for 'C:\Users\...': the still-settling dialog dropped
  # the first characters (a late select-all or field reset). Input delivery into the SAME verified
  # control is re-attempted (select all, retype, read back) up to three times; Enter is pressed only
  # after an exact readback, so the app never sees a partial path.
  $matched = $false
  for ($typing = 0; $typing -lt 3 -and -not $matched; $typing++) {
    if ($typing -gt 0) {
      Write-Host ("  [dialog] filename readback mismatch (observed '" + $readback.text + "'); retyping, attempt " + ($typing + 1))
      Start-Sleep -Milliseconds 300
    }
    Key 'Ctrl+A'
    Start-Sleep -Milliseconds 100
    TypeText $path
    Start-Sleep -Milliseconds 300
    $readbackFocus = [YesDawDrive]::FileNameHasFocus($dialog)
    $readback = [YesDawDrive]::ReadFileName($dialog)
    if (-not $readbackFocus) { break }   # focus moved elsewhere: never type into an unknown control
    $matched = $readback.available -and $readback.text -ceq $path
  }
  if (-not $matched) {
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
  # One of the app's own windows holds the foreground - its modal question (the missing-audio alert at a relaunch, an
  # unsaved-changes box): the keys belong there, and Windows will not activate the window it blocks (2026-10-07).
  $fg = [YesDawDrive]::GetForegroundWindow()
  if ($null -ne $script:Proc -and [YesDawDrive]::DialogBelongsTo($fg, [uint32]$script:Proc.Id)) {
    Write-Host ('  [focus] the app''s own window holds the foreground (hwnd ' + $fg + ')')
    return
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

# ADR-0072 §8 (G6.4): the real pointer over a target without pressing - JUCE's own enter, then its hover repaint.
function Hover([string] $target, [int] $OffsetX = 0, [int] $OffsetY = 0) {
  $pt = ScreenPoint $target $OffsetX $OffsetY
  [YesDawDrive]::MouseMoveAbs($pt[0] - 1, $pt[1]); Start-Sleep -Milliseconds 60
  [YesDawDrive]::MouseMoveAbs($pt[0], $pt[1]); Start-Sleep -Milliseconds 300
}

# A shot's pixels, read into memory so the PNG is not held open.
function LoadShot([string] $path) {
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $stream = New-Object System.IO.MemoryStream (, $bytes)
  return New-Object System.Drawing.Bitmap $stream
}

# The shot's physical pixels across one logical pixel of a pointer stroke's band - depth $depth (1 = just inside the
# 1 px inset, ADR-0072 §1) - at the middle of each edge of a layout rect (logical, shell coordinates). A fractional
# display scale spreads one logical pixel over two physical ones; both are listed.
function StrokeBand($rect, [int] $depth) {
  $s = [double](Probe).displayScale; if ($s -le 0) { $s = 1.0 }
  $x0 = [double]$rect[0] * $s; $y0 = [double]$rect[1] * $s
  $x1 = ([double]$rect[0] + [double]$rect[2]) * $s; $y1 = ([double]$rect[1] + [double]$rect[3]) * $s
  $mx = [int][Math]::Floor(($x0 + $x1) / 2); $my = [int][Math]::Floor(($y0 + $y1) / 2)
  $from = [int][Math]::Floor($depth * $s); $to = [int][Math]::Ceiling(($depth + 1) * $s) - 1
  $band = [ordered]@{ left = @(); right = @(); top = @(); bottom = @() }
  for ($d = $from; $d -le $to; $d++) {
    $band.left += , @(([int][Math]::Floor($x0) + $d), $my)
    $band.right += , @(([int][Math]::Ceiling($x1) - 1 - $d), $my)
    $band.top += , @($mx, ([int][Math]::Floor($y0) + $d))
    $band.bottom += , @($mx, ([int][Math]::Ceiling($y1) - 1 - $d))
  }
  return @{ band = $band; scale = $s; mx = $mx; my = $my; x0 = $x0; y0 = $y0; x1 = $x1; y1 = $y1 }
}

# The smallest channel rise from one pixel to another (white over a colour raises all three).
function Rise([System.Drawing.Color] $from, [System.Drawing.Color] $to) {
  return [Math]::Min([int]$to.R - [int]$from.R, [Math]::Min([int]$to.G - [int]$from.G, [int]$to.B - [int]$from.B))
}

# ADR-0072 §1 / §8 in the window's pixels - the hover line: along every edge of the rect the 1 px band is brighter than
# in the resting shot (by $MinRise on every channel somewhere across it), and the rect's middle is unchanged. Returns
# what is off ('' when the line shows).
function HoverStrokeShown([string] $restPath, [string] $nowPath, $rect, [int] $MinRise = 40) {
  $rest = LoadShot $restPath; $now = LoadShot $nowPath
  try {
    $g = StrokeBand $rect 1
    $off = @()
    foreach ($edge in @($g.band.Keys)) {
      $best = -255
      foreach ($p in $g.band[$edge]) { $best = [Math]::Max($best, (Rise $rest.GetPixel($p[0], $p[1]) $now.GetPixel($p[0], $p[1]))) }
      if ($best -lt $MinRise) { $off += ('{0} edge rose {1}' -f $edge, $best) }
    }
    $inset = 5 * $g.scale   # beyond the widest stroke's reach (3) and its corner
    $changed = 0
    for ($i = 0; $i -le 4; $i++) {
      for ($j = 0; $j -le 4; $j++) {
        $x = [int]($g.x0 + $inset + ($g.x1 - $g.x0 - 2 * $inset) * $i / 4)
        $y = [int]($g.y0 + $inset + ($g.y1 - $g.y0 - 2 * $inset) * $j / 4)
        $a = $rest.GetPixel($x, $y); $b = $now.GetPixel($x, $y)
        if ([Math]::Max([Math]::Abs([int]$a.R - [int]$b.R), [Math]::Max([Math]::Abs([int]$a.G - [int]$b.G), [Math]::Abs([int]$a.B - [int]$b.B))) -gt 8) { $changed++ }
      }
    }
    if ($changed -gt 0) { $off += ('{0} of 25 middle pixels changed' -f $changed) }
    return ($off -join '; ')
  } finally { $rest.Dispose(); $now.Dispose() }
}

# The pressed line: on the given edges (left / right: a widget's fill is a vertical gradient, so the pixel just inside the
# stroke on the same row is the unstroked one) every logical pixel of the band, $Width of them, is far brighter than that
# pixel - a 2 px line, not the hover's 1 px. Returns what is off.
function PressedStrokeShown([string] $nowPath, $rect, [int] $Width = 2, [string[]] $Edges = @('left', 'right'), [int] $MinRise = 100) {
  $now = LoadShot $nowPath
  try {
    $off = @()
    $inside = StrokeBand $rect ($Width + 2)
    foreach ($edge in $Edges) {
      $ref = $now.GetPixel($inside.band[$edge][0][0], $inside.band[$edge][0][1])
      for ($depth = 1; $depth -le $Width; $depth++) {
        $best = -255
        foreach ($p in (StrokeBand $rect $depth).band[$edge]) { $best = [Math]::Max($best, (Rise $ref $now.GetPixel($p[0], $p[1]))) }
        if ($best -lt $MinRise) { $off += ('{0} edge, depth {1}: rose {2} over the pixel inside' -f $edge, $depth, $best) }
      }
    }
    return ($off -join '; ')
  } finally { $now.Dispose() }
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

# G5.5 / ADR-0060: File > New shows the in-app New Project dialog (rate, tempo, meter, template) first; its Create asks
# for the location through the native chooser. Every script's "New" goes through here so the path is written once
# (2026-10-07: SS-2..SS-5 still waited for the chooser straight after the click and went red at G6). Asserts each
# hop, so a dialog that never shows or a Create that opens nothing is its own FAIL.
function NewProjectChooser([switch] $ViaKey) {
  if ($ViaKey) { Key 'Ctrl+N' } else { Click 'widget.project.new' }
  $shown = WaitProbe { param($q) [bool]$q.view.newProjectDialog } -TimeoutMs 3000
  if (-not $shown -and -not $ViaKey) {   # the first click on a freshly launched window can only activate it
    Focus
    Start-Sleep -Milliseconds 300
    Click 'widget.project.new'
    $shown = WaitProbe { param($q) [bool]$q.view.newProjectDialog } -TimeoutMs 3000
  }
  [void](Assert $shown 'New shows the New Project dialog (G5.5)')
  if (-not $shown) { return [IntPtr]::Zero }
  [void](Assert ($null -ne (LayoutRect 'newproject.create')) 'the probe publishes the dialog''s Create')
  Click 'newproject.create'
  $dlg = WaitDialog 'Create YES DAW Project' 6000
  [void](Assert ($dlg -ne [IntPtr]::Zero) 'Create opens the native project location chooser')
  return $dlg
}

# --- UI Automation (2026-10-07): what a screen reader sees, and popup items by their text ------------------------------
# The same tree Narrator / NVDA read (JUCE serves it per handler: juce_AccessibilityElement_windows.cpp). The drive is
# per-monitor DPI aware, so UI Automation rectangles are physical screen pixels, the same space SendInput uses.
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

function UiaDescribe($e) {
  if ($null -eq $e) { return $null }
  $c = $e.Current
  $toggle = ''; $value = ''; $range = $null; $pat = $null
  if ($e.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$pat)) { $toggle = [string]$pat.Current.ToggleState }
  if ($e.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern, [ref]$pat)) { $value = [string]$pat.Current.Value }
  if ($e.TryGetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern, [ref]$pat)) { $range = [double]$pat.Current.Value }
  $r = $c.BoundingRectangle
  return [pscustomobject]@{
    Name = [string]$c.Name; Type = ($c.ControlType.ProgrammaticName -replace '^ControlType\.', ''); ProcessId = [int]$c.ProcessId
    Enabled = [bool]$c.IsEnabled; Toggle = $toggle; Value = $value; Range = $range
    X = [int]$r.X; Y = [int]$r.Y; W = [int]$r.Width; H = [int]$r.Height; Element = $e
  }
}

function UiaFocused {
  try { return UiaDescribe ([System.Windows.Automation.AutomationElement]::FocusedElement) } catch { return $null }
}

function UiaFind([string] $nameLike, [string] $Type = '', [switch] $PopupsOnly) {
  $A = [System.Windows.Automation.AutomationElement]
  $byPid = New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, [int]$script:Proc.Id)
  $found = New-Object System.Collections.Generic.List[object]
  foreach ($w in $A::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $byPid)) {
    if ($PopupsOnly -and [IntPtr][int64]$w.Current.NativeWindowHandle -eq $script:Hwnd) { continue }
    $cond = [System.Windows.Automation.Condition]::TrueCondition
    if ($Type -ne '') {
      $ct = [System.Windows.Automation.ControlType]::$Type
      $cond = New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $ct)
    }
    foreach ($e in $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cond)) {
      if ($e.Current.Name -like $nameLike) { $found.Add((UiaDescribe $e)) }
    }
  }
  return $found
}

# A JUCE popup menu's item by its text (a trailing ellipsis aside), clicked with the real mouse. Positions shift as
# menus grow (2026-10-07: G5 added Save a Copy and Save as Template above Import MIDI File, and SS-5's "7th item" became
# Import Audio); a name does not. FAILs when the item is missing or disabled.
function MenuPickByName([string] $text, [int] $Depth = 1, [int] $TimeoutMs = 2500) {
  [void](WaitPopup -Depth $Depth)
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  $item = $null
  do {
    $items = @(UiaFind '*' 'MenuItem' -PopupsOnly | Where-Object { ($_.Name -replace '(\.\.\.|\u2026)$', '') -ceq $text })
    if ($items.Count -gt 0) { $item = $items[0] }
    else { Start-Sleep -Milliseconds 80 }
  } while ($null -eq $item -and (Get-Date) -lt $deadline)
  [void](Assert ($null -ne $item) ("the open menu offers '" + $text + "' (UI Automation)"))
  [void](Assert ($items.Count -le 1) ("'" + $text + "' names one item of the open menus (" + $items.Count + ")"))   # never a guess between two
  if ($null -eq $item) { Key 'Esc' -Repeat $Depth; return $false }
  [void](Assert $item.Enabled ("'" + $text + "' is enabled"))
  [YesDawDrive]::MouseMoveAbs([int]($item.X + $item.W / 2), [int]($item.Y + $item.H / 2)); Start-Sleep -Milliseconds 150
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 30
  [YesDawDrive]::MouseButton($false, $false); Start-Sleep -Milliseconds 250
  return $true
}

# 2026-10-05: wait until the app runs $Depth JUCE popup menus (the probe's modal.menus; a submenu is
# depth 2). Keys sent before a popup is modal land in the shell instead: the ss3 Clip-menu pick and the
# ss5 scale pick each raced a fixed 350 ms sleep. Records a FAIL only when no popup opens in time.
function WaitPopup([int] $Depth = 1, [int] $TimeoutMs = 2500) {
  $ok = WaitProbe { param($q) [int]$q.modal.menus -ge $Depth } -TimeoutMs $TimeoutMs
  if (-not $ok) { [void](Assert $false ('a popup menu opened (depth ' + $Depth + ') within ' + $TimeoutMs + ' ms')) }
  # Then let the popup's mouse tracker run first: it ticks at 20 Hz from a zero last-position, so its
  # first tick sees "the mouse moved", clears the keyboard's claim and highlights the item under the
  # pointer — a key sent before that tick is undone (juce_PopupMenu.cpp highlightItemUnderMouse; the
  # 2026-10-05 ss4 Sampler pick re-chose SimpleSynth exactly so). Four ticks of margin.
  Start-Sleep -Milliseconds 200
  return $ok
}

function AssertStartupBudget([int] $Milliseconds) {
  [void](Assert ($Milliseconds -ge 0 -and $Milliseconds -le 3000) ("launch to first interactive tick <= 3 s (B6): $Milliseconds ms"))
}

function Launch([string] $Bundle = '', [string] $ReuseSessionDir = '', [int] $AutosaveIntervalMs = 0, [int] $ExportPaceMs = 0, [switch] $FirstRunTips) {
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
  # ADR-0073 §4: a fresh session starts with the first-run tip dismissed (as a user who has seen it) unless the step asks
  # for it, so no earlier drive meets a new strip. A reused session keeps whatever its prefs.json says.
  $prefsPath = Join-Path $script:SessionDir 'prefs.json'
  if ([string]::IsNullOrWhiteSpace($ReuseSessionDir) -and -not $FirstRunTips -and -not (Test-Path -LiteralPath $prefsPath)) {
    [System.IO.File]::WriteAllText($prefsPath, '{ "version": 1, "tips": { "dismissed": [ "welcome" ] } }' + "`n", (New-Object System.Text.UTF8Encoding($false)))
  }
  $script:ProbePath = Join-Path $script:SessionDir 'probe.json'
  # Reusing session state preserves the keymap/last-project record, never an earlier process's
  # probe. Otherwise WaitProbe can certify launch against stale geometry before this exe starts.
  if (Test-Path -LiteralPath $script:ProbePath) { Remove-Item -LiteralPath $script:ProbePath -Force }
  $script:LastProbe = $null
  $env:YESDAW_STATE_PROBE = $script:ProbePath
  $env:YESDAW_SESSION_STATE_DIR = $script:SessionDir
  if ($AutosaveIntervalMs -gt 0) { $env:YESDAW_AUTOSAVE_INTERVAL_MS = [string]$AutosaveIntervalMs }
  else { Remove-Item Env:\YESDAW_AUTOSAVE_INTERVAL_MS -ErrorAction SilentlyContinue }
  if ($ExportPaceMs -gt 0) { $env:YESDAW_EXPORT_PACE_MS = [string]$ExportPaceMs }
  else { Remove-Item Env:\YESDAW_EXPORT_PACE_MS -ErrorAction SilentlyContinue }

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
  Remove-Item Env:\YESDAW_AUTOSAVE_INTERVAL_MS -ErrorAction SilentlyContinue
  Remove-Item Env:\YESDAW_EXPORT_PACE_MS -ErrorAction SilentlyContinue
}

# SS-6 step 6: an interruption — the process ends at once, no close path or prompt runs (what a crash or a kill leaves).
# The session-state folder stays for a relaunch with -ReuseSessionDir.
function KillApp {
  if ($null -eq $script:Proc) { return }
  try {
    Stop-Process -Id $script:Proc.Id -Force -ErrorAction SilentlyContinue
    [void]$script:Proc.WaitForExit(5000)
  } catch { }
  $deadline = (Get-Date).AddSeconds(5)   # the single-instance guard reads the process list: wait until it is gone
  while (@(Get-Process -Name 'YesDaw' -ErrorAction SilentlyContinue).Count -gt 0 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
  $script:Proc = $null
  $script:Hwnd = [IntPtr]::Zero
  Remove-Item Env:\YESDAW_STATE_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:\YESDAW_SESSION_STATE_DIR -ErrorAction SilentlyContinue
  Remove-Item Env:\YESDAW_AUTOSAVE_INTERVAL_MS -ErrorAction SilentlyContinue
  Remove-Item Env:\YESDAW_EXPORT_PACE_MS -ErrorAction SilentlyContinue
}

# SS-6 step 2: files dropped at a chosen lane and time arrive through the window's OLE drop target (JUCE's
# FileDragAndDropTarget), exactly as a drag from Explorer does. A small topmost form outside the app holds a file-drop data
# object; the real mouse presses on it (the form starts DoDragDrop on its own STA thread), moves onto the target and
# releases. Nothing in the app is bypassed: the drop target, the hit-test and onFilesDropped all run.
Add-Type -ReferencedAssemblies System.Windows.Forms, System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
public static class YesDawFileDrop
{
    [StructLayout(LayoutKind.Sequential)] struct PT { public int X, Y; }
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(PT p);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    static Thread thread;
    static Form currentForm;
    static volatile bool ready, dragging;
    static volatile string effect = "";
    static IntPtr formHandle = IntPtr.Zero;
    public static bool Ready { get { return ready; } }
    public static bool Dragging { get { return dragging; } }
    public static IntPtr FormHandle { get { return formHandle; } }
    // The top-level window under a screen point (what a press there would reach).
    public static IntPtr RootAt(int x, int y) { PT p; p.X = x; p.Y = y; return GetAncestor(WindowFromPoint(p), 2); }
    // Put the source form on top of everything again (another window may have been raised above it).
    public static void Raise() {
        if (formHandle == IntPtr.Zero) return;
        SetWindowPos(formHandle, new IntPtr(-1), 0, 0, 0, 0, 0x0003 | 0x0040);   // TOPMOST, NOMOVE|NOSIZE|SHOWWINDOW
        SetForegroundWindow(formHandle);
    }
    // A drag that never started: close the form so no orphan stays on top.
    public static void Abort() {
        var f = currentForm;
        if (f != null && f.IsHandleCreated) { try { f.BeginInvoke((MethodInvoker)(() => f.Close())); } catch (Exception) { } }
    }
    public static void Begin(string[] paths, int x, int y)
    {
        ready = false; dragging = false; effect = ""; formHandle = IntPtr.Zero; currentForm = null;
        thread = new Thread(() => {
            var form = new Form();
            form.FormBorderStyle = FormBorderStyle.None; form.ShowInTaskbar = false; form.TopMost = true;
            form.StartPosition = FormStartPosition.Manual; form.Location = new Point(x - 24, y - 24); form.Size = new Size(48, 48);
            form.BackColor = Color.DarkOrange;
            currentForm = form;
            form.MouseDown += (s, e) => {
                dragging = true;
                var data = new DataObject(DataFormats.FileDrop, paths);
                effect = form.DoDragDrop(data, DragDropEffects.Copy | DragDropEffects.Move | DragDropEffects.Link).ToString();
                dragging = false;
                form.Close();
            };
            form.Shown += (s, e) => { formHandle = form.Handle; form.Activate(); ready = true; };
            Application.Run(form);
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.IsBackground = true;
        thread.Start();
    }
    public static string End(int timeoutMs)
    {
        if (thread != null && !thread.Join(timeoutMs)) return "timeout";
        return effect;
    }
}
'@

function FileDrop([string] $target, [string[]] $paths, [int] $OffsetX = 0, [int] $OffsetY = 0) {
  foreach ($path in $paths) { if (-not (Test-Path -LiteralPath $path)) { throw "FileDrop: no file $path" } }
  $to = ScreenPoint $target $OffsetX $OffsetY
  # The drag starts left of the window's client area, over the frame, so the form never covers the target.
  $origin = New-Object YesDawDrive+POINT
  [void][YesDawDrive]::ClientToScreen($script:Hwnd, [ref]$origin)
  $fromX = [Math]::Max(30, $origin.X - 40); $fromY = $to[1]
  [YesDawFileDrop]::Begin([string[]]$paths, $fromX, $fromY)
  $deadline = (Get-Date).AddSeconds(3)
  while (-not [YesDawFileDrop]::Ready -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 30 }
  Start-Sleep -Milliseconds 150
  [YesDawDrive]::MouseMoveAbs($fromX, $fromY); Start-Sleep -Milliseconds 80
  # The press must reach the source form, not whatever window was raised above it since it appeared.
  for ($try = 0; $try -lt 10 -and [YesDawFileDrop]::RootAt($fromX, $fromY) -ne [YesDawFileDrop]::FormHandle; $try++) {
    [YesDawFileDrop]::Raise(); Start-Sleep -Milliseconds 60
  }
  $under = [YesDawFileDrop]::RootAt($fromX, $fromY)
  if ($under -ne [YesDawFileDrop]::FormHandle) {
    Write-Host ("  [drop] the source form is covered at {0},{1} by hwnd {2} (form {3})" -f $fromX, $fromY, $under, [YesDawFileDrop]::FormHandle) -ForegroundColor Yellow
  }
  [YesDawDrive]::MouseButton($true, $false)
  $deadline = (Get-Date).AddSeconds(3)
  while (-not [YesDawFileDrop]::Dragging -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 20 }
  if (-not [YesDawFileDrop]::Dragging) {
    [YesDawDrive]::MouseButton($false, $false)
    [YesDawFileDrop]::Abort()
    [void][YesDawFileDrop]::End(2000)
    Write-Host ("  [drop] the press did not start a drag (under the press: hwnd {0}, form {1}, app {2})" -f $under, [YesDawFileDrop]::FormHandle, $script:Hwnd) -ForegroundColor Yellow
    return 'no-drag'
  }
  Start-Sleep -Milliseconds 120
  $steps = 16
  for ($i = 1; $i -le $steps; $i++) {   # the drag loop tracks the real cursor; the target sees enter and over
    [YesDawDrive]::MouseMoveAbs([int]($fromX + ($to[0] - $fromX) * $i / $steps), [int]($fromY + ($to[1] - $fromY) * $i / $steps))
    Start-Sleep -Milliseconds 25
  }
  Start-Sleep -Milliseconds 200
  [YesDawDrive]::MouseButton($false, $false)
  $effect = [YesDawFileDrop]::End(5000)
  if ($effect -eq 'timeout') { [YesDawFileDrop]::Abort(); [void][YesDawFileDrop]::End(2000) }
  Write-Host ("  [drop] {0} file(s) at {1} -> {2}" -f $paths.Count, $target, $effect)
  return $effect
}

# JUCE alert windows (the missing-audio question, Save as Template's name, the replace question) are JUCE-drawn windows of
# their own; UI Automation names them by title and exposes their buttons and text field.
function WaitAlert([string] $titleContains, [int] $TimeoutMs = 4000) {
  $A = [System.Windows.Automation.AutomationElement]
  $byPid = New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, [int]$script:Proc.Id)
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  do {
    foreach ($w in $A::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $byPid)) {
      if ([IntPtr][int64]$w.Current.NativeWindowHandle -eq $script:Hwnd) { continue }
      if ($w.Current.Name -like ('*' + $titleContains + '*')) { return $w }
    }
    Start-Sleep -Milliseconds 80
  } while ((Get-Date) -lt $deadline)
  return $null
}

function AlertControl($alert, [string] $type, [string] $name) {
  $A = [System.Windows.Automation.AutomationElement]
  $cond = New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, [System.Windows.Automation.ControlType]::$type)
  $hits = @($alert.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cond) | Where-Object {
    $name -eq '' -or ($_.Current.Name -replace '(\.\.\.|\u2026)$', '') -ceq ($name -replace '(\.\.\.|\u2026)$', '') })
  return $hits
}

function AlertButton([string] $text, [string] $Title = '', [int] $TimeoutMs = 4000) {
  $alert = WaitAlert $Title $TimeoutMs
  [void](Assert ($null -ne $alert) ("an alert is up" + $(if ($Title) { " (" + $Title + ")" } else { '' })))
  if ($null -eq $alert) { return $false }
  $buttons = @(AlertControl $alert 'Button' $text)
  [void](Assert ($buttons.Count -eq 1) ("the alert offers one '" + $text + "' button (" + $buttons.Count + ")"))
  if ($buttons.Count -ne 1) { return $false }
  $r = $buttons[0].Current.BoundingRectangle
  [YesDawDrive]::MouseMoveAbs([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)); Start-Sleep -Milliseconds 120
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 30
  [YesDawDrive]::MouseButton($false, $false); Start-Sleep -Milliseconds 250
  return $true
}

function AlertText([string] $text, [string] $Title = '', [int] $TimeoutMs = 4000) {
  $alert = WaitAlert $Title $TimeoutMs
  [void](Assert ($null -ne $alert) ("an alert is up" + $(if ($Title) { " (" + $Title + ")" } else { '' })))
  if ($null -eq $alert) { return $false }
  $fields = @(AlertControl $alert 'Edit' '')
  [void](Assert ($fields.Count -ge 1) ("the alert has a text field (" + $fields.Count + ")"))
  if ($fields.Count -lt 1) { return $false }
  $r = $fields[0].Current.BoundingRectangle
  [YesDawDrive]::MouseMoveAbs([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)); Start-Sleep -Milliseconds 80
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 30
  [YesDawDrive]::MouseButton($false, $false); Start-Sleep -Milliseconds 120
  Key 'Ctrl+A'
  TypeText $text
  return $true
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
