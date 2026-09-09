# Headless regression checks for chooser orchestration. No app, native input or desktop access.
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $PSScriptRoot 'session-drive.ps1')
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseInput($source, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw $errors[0] }
Add-Type @'
using System;
public static class YesDawDrive {
    public static bool FocusOk = true, Closed = false, ReadbackOk = true;
    public static bool ReadbackAvailable = true, LoseFocusAfterTyping = false;
    public static bool EmptyReadback = false;
    public static int Typed = 0, Confirmed = 0;
    public static string Text = "", LastSearch = "";
    public static int ActivationAttempts = 0, ActivateOnAttempt = 1;
    public static bool CaptionSafe = false;
    public static string CaptionDiagnostic = "mock unsafe caption";
    public static bool Topmost = false;
    public static string TopmostWrites = "";
    public static bool WindowIsTopmost(IntPtr h) { return Topmost; }
    public static bool SetTargetTopmost(IntPtr h, bool value) { Topmost = value; TopmostWrites += value ? "T" : "F"; return true; }
    public static int CaptionAttempts = 0;
    public static bool ActivateCaption(IntPtr h) { CaptionAttempts++; if (CaptionSafe) Foreground = h; return CaptionSafe; }
    public static IntPtr Foreground = new IntPtr(99);
    public static bool ShowWindow(IntPtr h, int command) { return true; }
    public static bool SetForegroundWindow(IntPtr h) { ActivationAttempts++; if (ActivationAttempts >= ActivateOnAttempt) Foreground = h; return Foreground == h; }
    public static IntPtr GetForegroundWindow() { return Foreground; }
    public static IntPtr FindTopWindow(uint pid, string title) { LastSearch = title; return title == "Import WAV Audio" ? new IntPtr(20) : new IntPtr(10); }
    public static bool DialogBelongsTo(IntPtr h, uint pid) { return h == new IntPtr(20) && !Closed; }
    public static string WindowTitle(IntPtr h) { return "Import WAV Audio"; }
    public static bool FocusFileName(IntPtr h) { return FocusOk; }
    public sealed class FileNameReadback {
        public bool available;
        public string text;
        public override string ToString() { return available ? "text='"+text+"'" : "<unavailable>"; }
    }
    public static FileNameReadback ReadFileName(IntPtr h) { return new FileNameReadback { available = ReadbackAvailable, text = ReadbackAvailable ? (EmptyReadback ? "" : (ReadbackOk ? Text : "wrong path")) : null }; }
    public static bool FileNameHasFocus(IntPtr h) { return FocusOk && !(LoseFocusAfterTyping && Typed > 0); }
    public static string DialogDiagnostic(IntPtr h) { return "test diagnostic"; }
}
'@
foreach ($name in @('WaitDialog', 'FileDialogEnter', 'Focus', 'ActivateAppCaption')) {
    $function = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    if ($null -eq $function) { throw "Required driver function missing: $name" }
    Invoke-Expression $function.Extent.Text
}
function Start-Sleep { param($Milliseconds) }
function Key([string] $chord) {
    if ($chord -eq 'Ctrl+A') { [YesDawDrive]::Text = '' }
    if ($chord -eq 'Enter') { [YesDawDrive]::Confirmed++; [YesDawDrive]::Closed = $true }
}
function TypeText([string] $text) { [YesDawDrive]::Typed++; [YesDawDrive]::Text = $text }
$script:Proc = [pscustomobject]@{ Id = 123 }
$script:Hwnd = [IntPtr]10
function Reset {
    [YesDawDrive]::FocusOk = $true; [YesDawDrive]::Closed = $false; [YesDawDrive]::ReadbackOk = $true
    [YesDawDrive]::ReadbackAvailable = $true; [YesDawDrive]::EmptyReadback = $false; [YesDawDrive]::LoseFocusAfterTyping = $false
    [YesDawDrive]::Typed = 0; [YesDawDrive]::Confirmed = 0; [YesDawDrive]::Text = ''
    [void](WaitDialog 'Import WAV Audio')
}
Reset
[YesDawDrive]::FocusOk = $false
$refused = $false
try { FileDialogEnter 'C:\fixture.wav' } catch { $refused = $_.Exception.Message -like '*Cannot focus native filename control*' }
if (-not $refused) { throw 'Missing explicit filename focus refusal' }
if ([YesDawDrive]::Typed -ne 0) { throw 'Injected a path despite unverified filename focus' }
Reset
[YesDawDrive]::ReadbackOk = $false
$refused = $false
try { FileDialogEnter 'C:\fixture.wav' } catch { $refused = $_.Exception.Message -like '*readback did not match*' }
if (-not $refused) { throw 'Missing explicit filename readback refusal' }
if ([YesDawDrive]::Confirmed -ne 0) { throw 'Confirmed a filename without matching path readback' }
foreach ($failure in @('empty', 'unavailable', 'focus-loss')) {
    Reset
    if ($failure -eq 'empty') { [YesDawDrive]::EmptyReadback = $true }
    if ($failure -eq 'unavailable') { [YesDawDrive]::ReadbackAvailable = $false }
    if ($failure -eq 'focus-loss') { [YesDawDrive]::LoseFocusAfterTyping = $true }
    $message = ''
    try { FileDialogEnter 'C:\fixture.wav' } catch { $message = $_.Exception.Message }
    if ($message -notlike '*readback did not match*' -or [YesDawDrive]::Confirmed -ne 0) { throw "Missing unconfirmed refusal for $failure" }
    if ($failure -eq 'empty' -and $message -notlike "*text=''*" ) { throw 'Valid empty readback was not distinguished' }
    if ($failure -eq 'unavailable' -and $message -notlike '*<unavailable>*') { throw 'Unavailable readback was reported as empty' }
    if ($failure -eq 'focus-loss' -and $message -notlike '*observedFocus=False*') { throw 'Condition-time focus loss was not recorded' }
}
Reset
$refused = $false
try { FileDialogEnter '' } catch { $refused = $_.Exception.Message -like '*nonempty requested path*' }
if (-not $refused -or [YesDawDrive]::Typed -ne 0 -or [YesDawDrive]::Confirmed -ne 0) { throw 'Empty requested path was not refused before input' }
Write-Host 'PASS: empty, unavailable and focus-loss readbacks stay distinct and refuse confirmation; empty request refuses input'
Reset
FileDialogEnter 'C:\fixture with spaces.wav'
if ([YesDawDrive]::Typed -ne 1 -or [YesDawDrive]::Confirmed -ne 1 -or -not [YesDawDrive]::Closed) { throw 'Valid path was not entered and confirmed exactly once' }
if ([YesDawDrive]::LastSearch -ne 'Import WAV Audio') { throw 'Replaced the known Import HWND with a fuzzy main-window title search' }
Write-Host 'PASS: chooser focus refusal, readback refusal, exact Import HWND and single confirmation'

[YesDawDrive]::Foreground = [IntPtr]99
[YesDawDrive]::ActivationAttempts = 0; [YesDawDrive]::ActivateOnAttempt = 1000
$refused = $false
try { Focus } catch { $refused = $_.Exception.Message -like '*Could not activate the YES DAW window*' }
if (-not $refused) { throw 'Focus silently accepted an unrelated foreground window' }
if ([YesDawDrive]::Topmost -or [YesDawDrive]::TopmostWrites -ne 'TF') { throw 'Failed caption activation did not restore original non-topmost state' }
[YesDawDrive]::ActivationAttempts = 0; [YesDawDrive]::ActivateOnAttempt = 2
Focus
if ([YesDawDrive]::Foreground -ne $script:Hwnd -or [YesDawDrive]::ActivationAttempts -ne 2) { throw 'Focus failed to verify successful bounded activation retry' }
Write-Host 'PASS: foreground refusal and verified second activation attempt'
[YesDawDrive]::Foreground = [IntPtr]99
[YesDawDrive]::ActivationAttempts = 0; [YesDawDrive]::ActivateOnAttempt = 1000
[YesDawDrive]::CaptionSafe = $true; [YesDawDrive]::CaptionAttempts = 0
[YesDawDrive]::TopmostWrites = ''
Focus
if ([YesDawDrive]::Foreground -ne $script:Hwnd -or [YesDawDrive]::CaptionAttempts -ne 1) { throw 'Denied activation did not use one verified caption activation' }
if ([YesDawDrive]::Topmost -or [YesDawDrive]::TopmostWrites -ne 'TF') { throw 'Successful caption activation did not restore original non-topmost state' }
[YesDawDrive]::Topmost = $true; [YesDawDrive]::TopmostWrites = ''
[void](ActivateAppCaption)
if (-not [YesDawDrive]::Topmost -or [YesDawDrive]::TopmostWrites -ne 'TT') { throw 'Originally topmost app state was not preserved' }
Write-Host 'PASS: denied activation uses one safe caption fallback'
Write-Host 'PASS: temporary app topmost state restored on success/refusal and originally topmost preserved'

# Every Launch (including saved-project relaunches in later journeys) applies B6.
# Pin both sides of the boundary and reject a missing measurement, without native input.
$budgetFunction = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'AssertStartupBudget' }, $true)
if ($null -eq $budgetFunction) { throw 'Every-launch startup budget gate is missing' }
Invoke-Expression $budgetFunction.Extent.Text
function Assert([bool] $condition, [string] $message) { $script:BudgetAccepted = $condition }
foreach ($sample in @(@(2999, $true), @(3000, $true), @(3001, $false), @(3002, $false), @(-1, $false))) {
    AssertStartupBudget ([int]$sample[0])
    if ($script:BudgetAccepted -ne [bool]$sample[1]) { throw "Wrong B6 result for $($sample[0]) ms" }
}
$launchGate = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Launch' }, $true)
if ($launchGate.Extent.Text -notmatch 'AssertStartupBudget \$script:FirstProbeMs') { throw 'Launch omits the startup budget assertion' }
Write-Host 'PASS: every Launch enforces B6 at the unchanged 3000 ms boundary'

# Exercise Launch up to its process-start boundary using real temporary files. The previous
# process's probe must be gone at that boundary while unrelated persistent session data survives.
$launchFunction = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Launch' }, $true)
Invoke-Expression $launchFunction.Extent.Text
function Get-Process { param($Name, $ErrorAction) return @() }
function Start-Process {
    param($FilePath, $ArgumentList, [switch]$PassThru)
    if (Test-Path -LiteralPath $script:ProbePath) { throw 'Stale probe survives into process start' }
    if ((Get-Content -Raw -LiteralPath (Join-Path $script:SessionDir 'keymap.json')) -ne 'preserve') { throw 'Session data was altered' }
    throw 'Expected process-start boundary reached'
}
$testSession = Join-Path ([System.IO.Path]::GetTempPath()) ('yesdaw-chooser-test-' + [guid]::NewGuid())
$probeEnvBefore = $env:YESDAW_STATE_PROBE; $sessionEnvBefore = $env:YESDAW_SESSION_STATE_DIR
try {
    [void](New-Item -ItemType Directory -Path $testSession)
    [System.IO.File]::WriteAllText((Join-Path $testSession 'probe.json'), '{"version":1}')
    [System.IO.File]::WriteAllText((Join-Path $testSession 'keymap.json'), 'preserve')
    $Exe = $PSCommandPath
    $reached = $false
    try { Launch -ReuseSessionDir $testSession } catch { $reached = $_.Exception.Message -eq 'Expected process-start boundary reached' }
    if (-not $reached) { throw 'Launch did not clear stale probe before process start while retaining session data' }
    Write-Host 'PASS: reused session removes stale probe before launch and preserves session data'
} finally {
    $env:YESDAW_STATE_PROBE = $probeEnvBefore; $env:YESDAW_SESSION_STATE_DIR = $sessionEnvBefore
    foreach ($name in @('probe.json', 'keymap.json')) {
        $testFile = Join-Path $testSession $name
        if (Test-Path -LiteralPath $testFile) { Remove-Item -LiteralPath $testFile -Force }
    }
    if (Test-Path -LiteralPath $testSession) { Remove-Item -LiteralPath $testSession }
}
