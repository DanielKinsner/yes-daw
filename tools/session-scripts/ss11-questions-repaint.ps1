# SS-Q "the window repaints under a question" (ADR-0077, UI correctness pass FIX 29, unit 29e). Run by
# tools/session-drive.ps1 against the real exe, on Windows - where the freeze was.
#
# Before ADR-0077 a question started from a menu ran a nested modal loop inside one of JUCE's batched posted messages and
# held the Direct2D vblank message behind it: the shell's paint count stood still until the answer (measured on the unit
# 17 build: 2 paints in 3 s). A key chord ran outside the batch and never froze, so each question here is opened by mouse
# through the File menu. Under it the shell must keep painting - at least the larger of 5 and half the paints of a second
# with no question, taken in the same run. Then Space works after a chooser with no Focus step (the chooser runs on a
# worker thread; the shell puts the focus back).

function FileMenuPick([string] $item) {
  $menubar = LayoutRect 'widget.shell.menubar'
  Click 'widget.shell.menubar' -OffsetX (20 - [int]($menubar[2] / 2))   # 'File' (no Alt mnemonics in JUCE's bar)
  return (MenuPickByName $item)
}

function RepaintsUnder([string] $what, [int] $baseline) {
  $paints = PaintsPerSecond
  $need = [math]::Max(5, [int]($baseline / 2))
  $open = [bool](Probe).modal.questionOpen
  [void](Assert ($open -and $paints -ge $need) ('the window repaints under ' + $what + ' (' + $paints + ' paints in a second, at least ' + $need + '; question open: ' + $open + ')'))
}

Step 1 'A baseline: paints in a second with no question up'
Launch
Focus
$baseline = PaintsPerSecond
[void](Assert ($baseline -ge 10) ('the shell paints with no question up (' + $baseline + ' in a second)'))

Step 2 'File > Open by mouse: the chooser is up and the window repaints'
[void](FileMenuPick 'Open')
$dlg = WaitDialog 'Open YES DAW Project Folder' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'File > Open opens the native chooser')
if ($dlg -ne [IntPtr]::Zero) {
  Shot 'ss11-open-chooser'
  RepaintsUnder 'the Open chooser' $baseline
  [void][YesDawDrive]::PostMessage($dlg, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE: cancelled
}
[void](Assert (WaitProbe { param($q) -not [bool]$q.modal.questionOpen } -TimeoutMs 3000) 'cancelling the chooser frees the question')

Step 3 'File > New by mouse with unsaved changes: the alert is up and the window repaints'
Focus
$tracks = [int](Probe).view.trackCount
Key 'Ctrl+Shift+N'   # Add Track: an unsaved change
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $tracks + 1 } -TimeoutMs 2000) 'a track is added (an unsaved change)')
[void](FileMenuPick 'New')
$alert = WaitAlert 'Unsaved changes' 4000
[void](Assert ($null -ne $alert) 'File > New asks about the unsaved changes')
if ($null -ne $alert) {
  Shot 'ss11-unsaved-alert'
  RepaintsUnder 'the unsaved-changes alert' $baseline
  Key 'Esc'   # Cancel
}
[void](Assert (WaitProbe { param($q) -not [bool]$q.modal.questionOpen } -TimeoutMs 3000) 'Cancel frees the question')

Step 4 'File > Import Audio by mouse, then Space with no Focus step'
[void](FileMenuPick 'Import Audio')
$dlg = WaitDialog 'Import Audio' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'File > Import Audio opens the native chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 1 -and -not [bool]$q.modal.questionOpen } -TimeoutMs 8000) 'the import lands and frees the question')
Key 'Space'   # no Focus step: the chooser put the keyboard focus back
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 2000) 'Space plays at once after the chooser')
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 2000) 'Space stops')

Step 5 'The launch-time missing-audio question: the window repaints under it'
# The case that froze every time (unit 17): the question is posted from the UI tick, and a vblank queues behind it in the
# same batch. Save the session as a bundle, sideline its audio file, launch on it.
$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss11-questions-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }
Focus
Key 'Ctrl+S'   # the untitled session saves through Save As
$dlg = WaitDialog 'Save YES DAW Project As' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Save names the untitled session (Save As chooser)')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
$leaf = Split-Path -Leaf $bundle
[void](Assert (WaitProbe { param($q) (Split-Path -Leaf "$($q.bundlePath)") -eq $leaf -and -not [bool]$q.modal.questionOpen } -TimeoutMs 8000) ('saved as ' + $leaf))
Close
Start-Sleep -Milliseconds 500
$assets = @(Get-ChildItem -LiteralPath (Join-Path $bundle 'audio') -Filter '*.asset' -ErrorAction SilentlyContinue)
[void](Assert ($assets.Count -ge 1) ('the bundle carries its audio file (' + $assets.Count + ')'))
if ($assets.Count -ge 1) {
  $sidelined = Join-Path ([System.IO.Path]::GetTempPath()) ($assets[0].BaseName + '-ss11.wav')
  Move-Item -LiteralPath $assets[0].FullName -Destination $sidelined -Force
  Launch -Bundle $bundle
  $alert = WaitAlert 'Missing audio' 6000
  [void](Assert ($null -ne $alert) 'the launch asks about the missing audio')
  if ($null -ne $alert) {
    Shot 'ss11-launch-question'
    RepaintsUnder 'the launch-time question' $baseline
    [void](AlertButton 'Cancel' -Title 'Missing audio' -TimeoutMs 2000)
    [void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded -and -not [bool]$q.modal.questionOpen } -TimeoutMs 4000) 'Cancel leaves the untitled session')
  }
  Close
  Remove-Item -LiteralPath $sidelined -Force -ErrorAction SilentlyContinue
}
Remove-Item -Recurse -Force -LiteralPath $bundle -ErrorAction SilentlyContinue
