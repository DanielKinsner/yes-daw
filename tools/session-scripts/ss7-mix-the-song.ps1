# SS-5 "Mix the song" (plan §6, the G4 exit) — grown across G4. Run by tools/session-drive.ps1 against
# the real exe.
#
# The plan's text: route vocals to a new bus; EQ + compressor on it; send to a reverb bus; automate the
# bus fader with Write while playing; solo-safe the reverb; export.
#
# G4.1 cp1 (2026-09-05) — the strip's anatomy: Steps 0–7 below. New; three tracks; the mixer dock grown;
# a bus from the strip menu; the OUTPUT slot's popup routes a track to the bus (the strip reads "Out: Bus
# 1"); View > Narrow Strips (a shot) and back through the strip menu; the INPUT slot's popup picks an
# input when the device has one (the track is armed on it); the R cell arms the next track; save, close.
# Later G4 items append their steps (EQ + compressor, the send, Write, solo-safe, export).
#
# Deviations from the plan text (logged in STATUS.md, the G4.1 cp1 story): the recording device on the
# drive machine may have no inputs — the input-slot and R-cell steps then assert the honest refusal
# (the popup lists no inputs; the arm set stays empty) instead of the pick.

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss7-mix-the-song-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

# JUCE's popup keyboard law skips DISABLED items (Add Send ▸ with no bus, Arm with no device), so a
# count from the top is not stable; the structural verbs sit at the BOTTOM of every strip menu and are
# always enabled — pick them by counting UP from the end (a bare menu's first Up lands on the last item).
function MenuPickFromEnd([int] $fromEnd) {
  Start-Sleep -Milliseconds 350
  Key 'Up' -Repeat $fromEnd
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 250
}
# The strip's name band: the top mixerPaintedHeaderHeight (28 px) of the lane.
function ClickStripHeader([int] $strip, [switch] $Right) {
  $r = LayoutRect ('mixer.strip.' + $strip)
  if ($Right) { Click ('mixer.strip.' + $strip) -Right -OffsetY (14 - [int]($r[3] / 2)) }
  else { Click ('mixer.strip.' + $strip) -OffsetY (14 - [int]($r[3] / 2)) }
}
function OpenMixer {
  # X is the mixer dock's toggle: with another tab in the dock the first press may only hide the dock.
  Focus
  if ("$((Probe).view.dock)" -ne 'Mixer') {
    Key 'X'
    if (-not (WaitProbe { param($q) "$($q.view.dock)" -eq 'Mixer' } -TimeoutMs 1200)) { Key 'X' }
  }
  [void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Mixer' } -TimeoutMs 2000) ('the dock shows the mixer (view.dock=' + (Probe).view.dock + ')'))
}

Step 0 'Launch, New'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (D3: created through the real New chooser)')
Resize 1920 1080
Start-Sleep -Milliseconds 300

Step 1 'Three tracks (Ctrl+Shift+N twice)'
Focus
$t0 = [int](Probe).view.trackCount
Key 'Ctrl+Shift+N'
Key 'Ctrl+Shift+N'
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $t0 + 2 } -TimeoutMs 2000) ('Ctrl+Shift+N twice: ' + ($t0 + 2) + ' tracks'))

Step 2 'The mixer dock, grown; every Track strip carries its input, output and R cell'
OpenMixer
DragWithin 'widget.shell.splitter.dock' 0 0 0 -260
Start-Sleep -Milliseconds 300
[void](Assert (WaitProbe { param($q) $null -ne $q.layout.'mixer.strip.0.input' -and $null -ne $q.layout.'mixer.strip.0.output' } -TimeoutMs 3000) 'strip 0 lays out its INPUT and OUTPUT slots')
[void](Assert ($null -ne (Probe).layout.'mixer.strip.0.arm') 'strip 0 lays out its R cell')
$m = (Probe).mixer
[void](Assert ("$($m.strips[0].input)" -like 'In:*') ('the input slot reads its text (' + $m.strips[0].input + ')'))
[void](Assert ("$($m.strips[0].output)" -eq 'Out: Master') ('the output slot reads Master (' + $m.strips[0].output + ')'))
Shot 'ss7-strips'

Step 3 'A bus from the strip menu (right-click the strip: Add Bus)'
$b0 = [int](Probe).mixer.busCount
ClickStripHeader 0 -Right
# The TRACK strip's menu ends … | Narrow Strips | Add Bus, Remove Track — Add Bus is the second from the end.
MenuPickFromEnd 2
[void](Assert (WaitProbe { param($q) [int]$q.mixer.busCount -eq $b0 + 1 } -TimeoutMs 2000) 'Add Bus from the strip menu adds a bus strip')
[void](Assert ($null -ne (Probe).layout.'mixer.strip.3.output' -and $null -eq (Probe).layout.'mixer.strip.3.input') 'the Bus strip has an output slot and no input slot')

Step 4 'Route track 1 to the bus through its OUTPUT slot'
Click 'mixer.strip.0.output'
Start-Sleep -Milliseconds 600   # the slot's popup takes the keyboard once it is up
# The slot's popup: a section header, Master, then the buses — the LAST item is the new bus.
Key 'Up'
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[0].output)" -like 'Out: Bus*' } -TimeoutMs 2000) ('the output slot routes the track to the bus (' + (Probe).mixer.strips[0].output + ')'))
[void](Assert ("$((Probe).mixer.strips[1].output)" -eq 'Out: Master') 'the other tracks still feed Master')
Shot 'ss7-output-routed'

Step 5 'Narrow Strips from the Bus strip menu, then wide again from a Track strip menu'
# (View > Narrow Strips is the same verb — the View menu carries it; pinned headless by the menu count.
#  The drive reaches it from both strip menus: third from the end on either list.)
ClickStripHeader 3 -Right
MenuPickFromEnd 3
[void](Assert (WaitProbe { param($q) [bool]$q.view.mixerNarrow } -TimeoutMs 2000) 'Narrow Strips from the Bus strip menu narrows the strips (probe view.mixerNarrow)')
$narrow = LayoutRect 'mixer.strip.0'
[void](Assert ([int]$narrow[2] -lt 70) ('a narrow lane is narrow (' + $narrow[2] + ' px)'))
[void](Assert ($null -ne (Probe).layout.'mixer.strip.0.arm') 'the R cell still fits a narrow strip')
Shot 'ss7-narrow'
ClickStripHeader 0 -Right
MenuPickFromEnd 3
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.mixerNarrow } -TimeoutMs 2000) 'Narrow Strips from the Track strip menu toggles the strips wide again')
$wide = LayoutRect 'mixer.strip.0'
[void](Assert ([int]$wide[2] -gt [int]$narrow[2]) ('the wide lane is wider (' + $wide[2] + ' px)'))

Step 6 'The INPUT slot picks the input the track records from (arms it); the R cell arms the next track'
$rec = (Probe).recording
$inputs = if ([bool]$rec.deviceSelected) { [int]$rec.inputChannels } else { 0 }
Click 'mixer.strip.1.input'
Start-Sleep -Milliseconds 600
if ($inputs -gt 0) {
  Key 'Down'
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  [void](Assert (WaitProbe { param($q) [int]$q.recording.armedTrackCount -eq 1 } -TimeoutMs 2000) ('the pick arms track 2 on In 1 (device inputs=' + $inputs + ')'))
  [void](Assert ("$((Probe).mixer.strips[1].input)" -eq 'In: 1') ('the input slot reads the pick (' + (Probe).mixer.strips[1].input + ')'))
  Click 'mixer.strip.2.arm'
  [void](Assert (WaitProbe { param($q) [int]$q.recording.armedTrackCount -eq 2 } -TimeoutMs 2000) 'the R cell arms track 3 too')
  [void](Assert ([bool](Probe).mixer.strips[2].armed) 'strip 3 paints its R cell lit')
  Shot 'ss7-armed'
  Click 'mixer.strip.2.arm'
  [void](Assert (WaitProbe { param($q) [int]$q.recording.armedTrackCount -eq 1 } -TimeoutMs 2000) 'a second click disarms it')
} else {
  Key 'Escape'
  Start-Sleep -Milliseconds 200
  [void](Assert ([int](Probe).recording.armedTrackCount -eq 0) 'no adopted recording device with inputs on this machine: the popup offers none and nothing arms (the honest refusal)')
  Click 'mixer.strip.2.arm'
  Start-Sleep -Milliseconds 300
  [void](Assert ([int](Probe).recording.armedTrackCount -eq 0) 'the R cell cannot arm without a device with inputs (the registry refuses)')
}

Step 7 'Save and close'
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
Close
