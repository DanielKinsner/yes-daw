# SS-5 "Mix the song" (plan §6, the G4 exit) — grown across G4. Run by tools/session-drive.ps1 against
# the real exe.
#
# The plan's text: route vocals to a new bus; EQ + compressor on it; send to a reverb bus; automate the
# bus fader with Write while playing; solo-safe the reverb; export.
#
# G4.1 cp1 (2026-09-05) — the strip's anatomy: Steps 0–6 below. New; three tracks; the mixer dock grown;
# a bus from the strip menu; the OUTPUT slot's popup routes a track to the bus (the strip reads "Out: Bus
# 1"); View > Narrow Strips (a shot) and back through the strip menu; the INPUT slot's popup picks an
# input when the device has one (the track is armed on it); the R cell arms the next track; save, close.
# G4.1 cp2 (2026-09-05) — the lane folds into the strip: Steps 7–11. An empty insert slot's click lists
# the kinds (EQ, then a Compressor, on the bus); a filled slot's double-click opens the FX editor (a
# shot; Close); an empty send well's click lists the buses (track 1 sends to the bus); the send row's
# right-click menu flips it pre-fader. Later G4 items append theirs (Write, solo-safe, export).
# G4.7 (2026-10-06) — the master strip: Step 13. The master pane's first slot takes a Limiter (Limiter is
# the first kind there) and its double-click opens the editor with the gain-reduction face; the header's DIM
# and MUTE light and clear; the fixture imported onto track 1 plays and the header's LUFS readout shows the
# measured loudness (when the drive machine has an output device — otherwise the honest "--" is asserted).
# Save and close moves to Step 14.
# G4.0b (2026-10-05) — keyboard only: Step 11. Tab starts control navigation (the ring; the probe's
# controlTarget); Space stays transport; the Snap chooser previews with arrows and Enter applies; strip 1's
# painted fader moves by dB as one undo step while Right never moves the playhead, and Esc restores; the
# EQ editor is a Tab panel of its own (band 1 gain by keys; Enter on Close returns the target); the keymap
# search field takes typed text (its Space types, never plays) and Tab leaves it; Esc ends navigation and
# Enter is Return to zero again. Save and close is Step 12.
# G4.2 cp2 (2026-10-05) — Step 8 also opens the bus Compressor's face: its gain-reduction meter is laid out
# and reads the running node (zero in this silent song).
# G4.5 (2026-10-06) — Step 10 also solos by click and Ctrl-click (exclusive) and clears with the header's SOLO.
# G4.4 (2026-10-06) — Step 8 also keys the bus Compressor from Audio 1 through its Sidechain chooser (the
# SC badge lights) and undoes it.
# G4.3 (2026-10-06) — Step 9 also sends track 1 to a New Bus from the next empty well and undoes it, and
# routes track 2 with the header's Route to New Bus and undoes that.
# G4.2 cp6–cp7 (2026-10-05) — Step 8 drags the bus EQ below its Compressor and undoes it; then the
# Compressor's Presets menu saves a named preset through the prompt, loads it back after an edit, and one
# Ctrl+Z undoes the load.
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
  [void](WaitPopup)
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
if ($dlg -eq [IntPtr]::Zero) {   # the first click on a freshly launched window can only activate it: click once more
  Focus
  Start-Sleep -Milliseconds 300
  Click 'widget.project.new'
  $dlg = WaitDialog 'Create YES DAW Project' 6000
}
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [string]$q.bundlePath -eq $bundle } -TimeoutMs 6000) 'New opens the requested bundle, independent of the startup project')
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
Start-Sleep -Milliseconds 400   # the dock settles after the tab switch; a drag that starts mid-layout is dropped
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
[void](WaitPopup)   # the slot's popup takes the keyboard once it is modal
# The slot's popup: a section header, Master, the buses, then New Bus (G4.3) — the bus is second from the end.
Key 'Up' -Repeat 2
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[0].output)" -eq 'Out: Bus 1' } -TimeoutMs 2000) ('the output slot routes the track to the bus (' + (Probe).mixer.strips[0].output + ')'))
[void](Assert (@((Probe).mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count -eq 1) 'routing to the existing bus makes no new one')
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
if ($inputs -gt 0) {
  [void](WaitPopup)   # with inputs the slot's popup lists them; without, the refusal below may show none
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
  Start-Sleep -Milliseconds 600
  Key 'Escape'
  Start-Sleep -Milliseconds 200
  [void](Assert ([int](Probe).recording.armedTrackCount -eq 0) 'no adopted recording device with inputs on this machine: the popup offers none and nothing arms (the honest refusal)')
  Click 'mixer.strip.2.arm'
  Start-Sleep -Milliseconds 300
  [void](Assert ([int](Probe).recording.armedTrackCount -eq 0) 'the R cell cannot arm without a device with inputs (the registry refuses)')
}

Step 7 'EQ and a Compressor on the bus (the empty slot''s click lists the kinds)'
# A Bus takes the five audio kinds; the slot's popup is a section header then the kinds, all enabled —
# EQ is the first (Down once), the Compressor the second. A slot popup needs ~600 ms before it takes keys.
Click 'mixer.strip.3.insert.0'
[void](WaitPopup)
Key 'Down'
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[3].inserts[0].kind)" -eq 'EQ' } -TimeoutMs 2000) ('the EQ lands in the bus''s slot 1 (' + (Probe).mixer.strips[3].inserts[0].kind + ')'))
Click 'mixer.strip.3.insert.1'
[void](WaitPopup)
Key 'Down' -Repeat 2
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[3].inserts[1].kind)" -eq 'Compressor' } -TimeoutMs 2000) ('the Compressor lands in slot 2 (' + (Probe).mixer.strips[3].inserts[1].kind + ')'))
[void](Assert ([bool](Probe).mixer.strips[3].inserts[0].enabled) 'the EQ is in (its dot is lit)')

Step 8 'The EQ''s editor (double-click the slot); Close'
Click 'mixer.strip.3.insert.0' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible -and "$($q.fxEditor.kind)" -eq 'EQ' } -TimeoutMs 2000) ('the double-click opens the EQ''s editor (fxEditor=' + (Probe).fxEditor.kind + ', strip ' + (Probe).fxEditor.strip + ')'))
[void](Assert ([int](Probe).fxEditor.rows -gt 0) ('the editor lays out the EQ''s parameter rows (' + (Probe).fxEditor.rows + ')'))
Shot 'ss7-fx-editor'
# G4.2 cp1: the response follows the actual EQ settings; a gain edit, bypass and undo.
[void](Assert ($null -ne (Probe).layout.'mixer.fx.editor.eq.response') 'the EQ response display is laid out')
[void](Assert ([Math]::Abs([double](Probe).fxEditor.eqResponseDb1000) -lt 0.01) 'the default EQ response is flat at 1 kHz')
DragWithin 'mixer.fx.param.2' -35 0 60 0
[void](Assert (WaitProbe { param($q) [double]$q.fxEditor.eqResponseDb1000 -gt 1.0 } -TimeoutMs 2000) 'band 1 gain lifts the 1 kHz response')
Shot 'ss7-eq-boost'
Click 'mixer.fx.editor.bypass'
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.bypassed -and [Math]::Abs([double]$q.fxEditor.eqResponseDb1000) -lt 0.01 } -TimeoutMs 2000) 'bypass shows the flat effective response')
Click 'mixer.fx.editor.bypass'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.bypassed -and [double]$q.fxEditor.eqResponseDb1000 -gt 1.0 } -TimeoutMs 2000) 'unbypass restores the configured curve')
foreach ($size in @(@(1280,720), @(1920,1080), @(2560,1440))) {
  Resize $size[0] $size[1]
  Start-Sleep -Milliseconds 250
  $graph = LayoutRect 'mixer.fx.editor.eq.response'
  $pager = LayoutRect 'mixer.fx.param.page'
  [void](Assert ([int]$graph[1] + [int]$graph[3] -le [int]$pager[1]) ('graph above controls at ' + $size[0] + 'x' + $size[1]))
  Shot ('ss7-eq-' + $size[0])
}
Resize 1920 1080

Click 'mixer.fx.editor.close'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible } -TimeoutMs 2000) 'Close hides the editor')

# G4.2 cp2: the Compressor's face carries the gain-reduction meter. This song has no audio yet, so the
# app runs its transport-only engine (no compiled graph): the honest reading is "not running". The
# headless gate drives real reduction through a running node.
Click 'mixer.strip.3.insert.1' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible -and "$($q.fxEditor.kind)" -eq 'Compressor' } -TimeoutMs 2000) ('the double-click opens the Compressor''s editor (' + (Probe).fxEditor.kind + ')'))
[void](Assert ([bool](Probe).fxEditor.grVisible -and $null -ne (Probe).layout.'mixer.fx.editor.gr') 'the compressor face lays out its gain-reduction meter')
[void](Assert (-not [bool](Probe).fxEditor.grReading -and [double](Probe).fxEditor.gainReductionDb -eq 0.0) ('no media, no running node: the meter says not running rather than inventing a reading (' + (Probe).fxEditor.gainReductionDb + ' dB)'))
Shot 'ss7-compressor-face'
Click 'mixer.fx.editor.close'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible } -TimeoutMs 2000) 'Close hides the compressor editor')

# G4.2 cp6: drag the bus EQ below its Compressor with the mouse; one Ctrl+Z puts it back.
Drag 'mixer.strip.3.insert.0' 'mixer.strip.3.insert.1'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[3].inserts[0].kind)" -eq 'Compressor' -and "$($q.mixer.strips[3].inserts[1].kind)" -eq 'EQ' } -TimeoutMs 2000) ('dragging the EQ slot onto slot 2 reorders the chain (' + (Probe).mixer.strips[3].inserts[0].kind + ', ' + (Probe).mixer.strips[3].inserts[1].kind + ')'))
[void](Assert ([int](Probe).mixer.insertCarry.landing -eq -1) 'the landing line is gone after the release')
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[3].inserts[0].kind)" -eq 'EQ' -and "$($q.mixer.strips[3].inserts[1].kind)" -eq 'Compressor' } -TimeoutMs 2000) 'one Ctrl+Z restores the chain order')

# G4.2 cp7: the Compressor's Presets menu. Save Preset... names the setting in a prompt; after an edit,
# picking the preset puts the saved setting back, and one Ctrl+Z undoes the whole load.
Click 'mixer.strip.3.insert.1' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible -and "$($q.fxEditor.kind)" -eq 'Compressor' } -TimeoutMs 2000) 'the Compressor''s editor reopens for presets')
[void](Assert ($null -ne (Probe).layout.'mixer.fx.editor.presets') 'the editor''s title row carries Presets')
$saved = [double](Probe).fxEditor.params.'compressor.threshold'
Click 'mixer.fx.editor.presets'
# A fresh session has none: a disabled "No Compressor presets yet", then Save Preset... (the last item).
MenuPickFromEnd 1
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.presetPromptOpen } -TimeoutMs 2000) 'Save Preset... asks for a name')
TypeText 'Drive comp'
[void](Assert (WaitProbe { param($q) "$($q.fxEditor.presetPromptText)" -eq 'Drive comp' } -TimeoutMs 2000) ('the typed name lands in the prompt (' + (Probe).fxEditor.presetPromptText + ')'))
Shot 'ss7-preset-prompt'
Key 'Enter'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.presetPromptOpen -and "$($q.status.text)" -eq 'Saved preset "Drive comp"' } -TimeoutMs 2000) ('Enter saves the preset (' + (Probe).status.text + ')'))
[void](Assert (@((Probe).fxEditor.presets) -contains 'Drive comp') 'the preset is listed for the Compressor')
DragWithin 'mixer.fx.param.0' -35 0 60 0
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.fxEditor.params.'compressor.threshold' - $saved) -gt 1.0 } -TimeoutMs 2000) ('dragging Threshold moves it off the saved value (' + (Probe).fxEditor.params.'compressor.threshold' + ' dB)'))
$edited = [double](Probe).fxEditor.params.'compressor.threshold'
Click 'mixer.fx.editor.presets'
[void](WaitPopup)
Key 'Down'   # the first enabled item: the preset
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.fxEditor.params.'compressor.threshold' - $saved) -lt 0.01 -and "$($q.status.text)" -eq 'Loaded preset "Drive comp"' } -TimeoutMs 2000) ('picking the preset restores the saved threshold (' + (Probe).fxEditor.params.'compressor.threshold' + ' dB, ' + (Probe).status.text + ')'))
Shot 'ss7-preset-loaded'
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.fxEditor.params.'compressor.threshold' - $edited) -lt 0.01 } -TimeoutMs 2000) ('one Ctrl+Z undoes the whole load (' + (Probe).fxEditor.params.'compressor.threshold' + ' dB)'))
Click 'mixer.fx.editor.close'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible } -TimeoutMs 2000) 'Close hides the compressor editor again')

# G4.4 (ADR-0051): the bus Compressor's Sidechain chooser keys it from Audio 1 (which already feeds the bus:
# its pre-fader key is upstream, no loop) as one step; the strip's SC badge lights; Ctrl+Z clears it.
Click 'mixer.strip.3.insert.1' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible -and "$($q.fxEditor.kind)" -eq 'Compressor' -and [bool]$q.fxEditor.sidechainVisible } -TimeoutMs 2000) 'the Compressor editor carries its Sidechain chooser')
[void](Assert ("$((Probe).fxEditor.sidechain)" -eq 'None' -and -not [bool](Probe).mixer.strips[3].sidechain) 'unkeyed: None, no SC badge')
Click 'mixer.fx.editor.sidechain'
[void](WaitPopup)
Key 'Down'   # from None to the first source: Audio 1
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.fxEditor.sidechain)" -eq 'Audio 1' -and [bool]$q.mixer.strips[3].sidechain } -TimeoutMs 2000) ('the pick keys the Compressor from Audio 1 and lights the SC badge (' + (Probe).fxEditor.sidechain + ')'))
Shot 'ss7-sidechain'
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) "$($q.fxEditor.sidechain)" -eq 'None' -and -not [bool]$q.mixer.strips[3].sidechain } -TimeoutMs 2000) 'one Ctrl+Z clears the sidechain')
Click 'mixer.fx.editor.close'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible } -TimeoutMs 2000) 'Close hides the compressor editor after the sidechain')

Step 9 'Track 1 sends to the bus (the empty send well''s click lists the buses)'
Click 'mixer.strip.0.send.0'
[void](WaitPopup)
Key 'Down'
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[0].sends[0].bus)" -eq 'Bus 1' } -TimeoutMs 2000) ('the send routes track 1 to the bus (the row reads ' + (Probe).mixer.strips[0].sends[0].bus + ')'))
[void](Assert (-not [bool](Probe).mixer.strips[0].sends[0].pre) 'a new send is post-fader')
# G4.3: the next empty well's chooser ends with New Bus — the bus and the send in one step; Ctrl+Z takes both.
Click 'mixer.strip.0.send.1'
[void](WaitPopup)
Key 'Up'   # the last item: New Bus
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[0].sends[1].bus)" -eq 'Bus 2' -and @($q.mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count -eq 2 } -TimeoutMs 2000) ('New Bus makes Bus 2 and sends track 1 to it (send 2 reads ' + (Probe).mixer.strips[0].sends[1].bus + ')'))
Shot 'ss7-send-new-bus'
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) @($q.mixer.strips[0].sends).Count -eq 1 -and @($q.mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count -eq 1 } -TimeoutMs 2000) 'one Ctrl+Z takes back the new bus and its send')
# G4.3: the track header's Route to New Bus (second from the end: Track Output, Route to New Bus, Automation).
$row = LayoutRect 'rail.row.1'
Click 'rail.row.1' -Right -OffsetX (60 - [int]($row[2] / 2))
MenuPickFromEnd 2
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[1].kind)" -eq 'Track' -and "$($q.mixer.strips[1].output)" -eq 'Out: Bus 2' } -TimeoutMs 2000) ('Route to New Bus sends track 2 to a new Bus 2 (' + (Probe).mixer.strips[1].output + ', lastAction ' + (Probe).lastAction + ')'))
Shot 'ss7-route-new-bus'
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[1].output)" -eq 'Out: Master' -and @($q.mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count -eq 1 } -TimeoutMs 2000) 'one Ctrl+Z puts track 2 back on Master and removes the bus')

Step 10 'The send row''s menu: Pre-fader'
# The routed row's right-click menu: Pre-fader, Destination >, then Remove Send — Pre-fader is first.
Click 'mixer.strip.0.send.0' -Right
[void](WaitPopup)
Key 'Down'
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.strips[0].sends[0].pre } -TimeoutMs 2000) 'the row''s menu flips the send pre-fader (the strip paints PRE)')
Shot 'ss7-send'

# G4.5: solo UX — two plain Solo clicks light the header's SOLO; a Ctrl-click keeps only its strip; the
# header's SOLO clears every solo; Ctrl+Z brings the solo back; SOLO clears it again.
$soloedNames = { param($q) @($q.mixer.strips | Where-Object { [bool]$_.soloed } | ForEach-Object { "$($_.name)" }) -join ',' }
Click 'mixer.strip.0.solo'
Click 'mixer.strip.3.solo'   # the bus
[void](Assert (WaitProbe { param($q) (& $soloedNames $q) -eq 'Audio 1,Bus 1' -and [bool]$q.mixer.anySolo } -TimeoutMs 2000) ('two Solo clicks solo Audio 1 and the bus; the header SOLO lights (' + (& $soloedNames (Probe)) + ')'))
Click 'mixer.strip.1.solo' -Modifiers 'Ctrl'
[void](Assert (WaitProbe { param($q) (& $soloedNames $q) -eq 'Audio 2' } -TimeoutMs 2000) ('a Ctrl-click solos only Audio 2 (' + (& $soloedNames (Probe)) + ')'))
Shot 'ss7-solo'
Click 'header.solo.clear'
[void](Assert (WaitProbe { param($q) (& $soloedNames $q) -eq '' -and -not [bool]$q.mixer.anySolo } -TimeoutMs 2000) 'the header SOLO clears every solo')
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) (& $soloedNames $q) -eq 'Audio 2' } -TimeoutMs 2000) 'one Ctrl+Z brings the solo back')
Click 'header.solo.clear'
[void](Assert (WaitProbe { param($q) -not [bool]$q.mixer.anySolo } -TimeoutMs 2000) 'SOLO clears it again')

# The probe advances one document per UI tick: wait two ticks after a key so the read is post-key.
function KeyThenTick([string] $chord) {
  $tick = [int](Probe).tick
  Key $chord
  [void](WaitProbe { param($q) [int]$q.tick -ge $tick + 2 } -TimeoutMs 600)
}
function TabTo([string] $id, [switch] $Back) {
  $limit = [int](Probe).controlTarget.count + 2
  for ($i = 0; $i -lt $limit; $i++) {
    if ("$((Probe).controlTarget.id)" -eq $id) { return $true }
    if ($Back) { KeyThenTick 'Shift+Tab' } else { KeyThenTick 'Tab' }
  }
  return ("$((Probe).controlTarget.id)" -eq $id)
}

Step 11 'Keyboard only: the Control target (G4.0b)'
# The Arrange editor takes the keys first (a timeline click): the toolbar's Snap chooser shows only in
# Arrange focus, and its Left / Right locate the playhead (the negative control below needs both).
Click 'timeline'
[void](Assert (WaitProbe { param($q) "$($q.focusContext)" -eq 'Arrange' } -TimeoutMs 1500) ('a timeline click gives Arrange the keys (' + (Probe).focusContext + ')'))
[void](Assert (-not [bool](Probe).controlTarget.navigating) 'no control is targeted before Tab')
KeyThenTick 'Tab'
$ct = (Probe).controlTarget
[void](Assert ([bool]$ct.navigating -and "$($ct.id)" -ne '') ('Tab starts control navigation on ' + $ct.id + ' (' + $ct.count + ' controls)'))
Shot 'ss7-keyboard-ring'
KeyThenTick 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays while navigating')
[void](Assert ("$((Probe).controlTarget.id)" -eq "$($ct.id)") 'Space leaves the target where it was')
KeyThenTick 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops while navigating')

[void](Assert (TabTo 'timeline.snap.chooser') 'Tab reaches the Snap chooser')
$ticks = [int64](Probe).view.snapGridTicks
KeyThenTick 'Enter'
[void](Assert ([bool](Probe).controlTarget.interacting) 'Enter starts the choice')
KeyThenTick 'Down'
[void](Assert ([int64](Probe).view.snapGridTicks -eq $ticks) ('an arrow previews without applying (' + (Probe).controlTarget.value + ')'))
Shot 'ss7-keyboard-chooser'
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) [int64]$q.view.snapGridTicks -ne $ticks -and -not [bool]$q.controlTarget.interacting } -TimeoutMs 1500) ('Enter applies the choice (' + (Probe).view.snapGridTicks + ' ticks)'))
KeyThenTick 'Enter'
KeyThenTick 'Up'
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) [int64]$q.view.snapGridTicks -eq $ticks } -TimeoutMs 1500) 'the chooser goes back by keys too')

# Arrange still has the focus while the dock shows the mixer; Shift+Tab from the start lands on the last
# control, nearest the dock.
KeyThenTick 'Escape'
[void](Assert ("$((Probe).focusContext)" -eq 'Arrange' -and -not [bool](Probe).controlTarget.navigating) ('Esc ended navigation; Arrange keeps the keys (' + (Probe).focusContext + ')'))
KeyThenTick 'Shift+Tab'
[void](Assert (TabTo 'mixer.strip.0.fader' -Back) 'Shift+Tab reaches track 1''s painted fader')
$located = [int64](Probe).transport.playheadFrame
KeyThenTick 'Right'
[void](Assert ([int64](Probe).transport.playheadFrame -gt $located) 'negative control: before Enter, Right is the editor''s and locates the playhead')
$frame = [int64](Probe).transport.playheadFrame
KeyThenTick 'Enter'
KeyThenTick 'Up'
KeyThenTick 'Up'
KeyThenTick 'Up'
KeyThenTick 'Right'
KeyThenTick 'Left'
KeyThenTick 'Enter'
$gain = [double](Probe).mixer.strips[0].linearGain
[void](Assert ([Math]::Abs($gain - [Math]::Pow(10.0, 3.0 / 20.0)) -lt 0.002) ('three Ups raise the fader 3 dB (' + (Probe).controlTarget.value + ', linear ' + $gain + ')'))
[void](Assert ([int64](Probe).transport.playheadFrame -eq $frame) 'Right / Left adjusted the fader, never the playhead')
Shot 'ss7-keyboard-fader'
KeyThenTick 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.mixer.strips[0].linearGain - 1.0) -lt 0.0001 } -TimeoutMs 1500) 'one Ctrl+Z undoes the whole keyboard adjustment')
KeyThenTick 'Enter'
KeyThenTick 'Down'
KeyThenTick 'Escape'
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.mixer.strips[0].linearGain - 1.0) -lt 0.0001 -and [bool]$q.controlTarget.navigating } -TimeoutMs 1500) 'Esc restores the value the interaction started from')

# The EQ editor (opened with the mouse: a click ends navigation; Tab starts it inside the editor).
Click 'mixer.strip.3.insert.0' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible } -TimeoutMs 2000) 'the EQ editor is open')
[void](Assert (-not [bool](Probe).controlTarget.navigating) 'the mouse press ended keyboard navigation')
Focus
KeyThenTick 'Tab'
[void](Assert ("$((Probe).controlTarget.scope)" -eq 'mixer.fx.editor') ('Tab stays inside the open editor (' + (Probe).controlTarget.id + ')'))
[void](Assert (TabTo 'mixer.fx.param.2') 'Tab reaches band 1 gain')
$eq = [double](Probe).fxEditor.eqResponseDb1000
KeyThenTick 'Enter'
1..5 | ForEach-Object { KeyThenTick 'Up' }
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) [double]$q.fxEditor.eqResponseDb1000 -gt $eq + 0.5 } -TimeoutMs 1500) ('band 1 gain by keys lifts 1 kHz (' + $eq + ' -> ' + (Probe).fxEditor.eqResponseDb1000 + ' dB)'))
Shot 'ss7-keyboard-eq'
[void](Assert (TabTo 'mixer.fx.editor.close') 'Tab reaches the editor''s Close')
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible -and [bool]$q.controlTarget.navigating -and "$($q.controlTarget.scope)" -eq '' } -TimeoutMs 2000) ('Enter on Close closes the panel and navigation continues in the shell (' + (Probe).controlTarget.id + ')'))

# Text entry outranks the router: the keymap search field types Space instead of playing.
KeyThenTick 'Alt+K'
[void](Assert (WaitProbe { param($q) "$($q.controlTarget.scope)" -eq 'keymap.editor' } -TimeoutMs 1500) 'the keymap editor is a Tab panel of its own')
[void](Assert (TabTo 'keymap.editor.search') 'Tab reaches the keymap search field')
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.focusOwner)" -eq 'keymap.editor.search' } -TimeoutMs 1500) ('Enter starts text entry (focus ' + (Probe).focusOwner + ')'))
TypeText 'play mode'
[void](WaitProbe { param($q) "$($q.controlTarget.value)" -eq 'play mode' } -TimeoutMs 1500)
[void](Assert ("$((Probe).controlTarget.value)" -eq 'play mode') ('typed text reaches the field, Space included (' + (Probe).controlTarget.value + ')'))
[void](Assert (-not [bool](Probe).transport.isPlaying) 'the typed Space did not play')
KeyThenTick 'Tab'
[void](Assert (WaitProbe { param($q) "$($q.focusOwner)" -ne 'keymap.editor.search' -and [bool]$q.controlTarget.navigating } -TimeoutMs 1500) 'Tab leaves the field and keeps navigating')
KeyThenTick 'Escape'
[void](Assert (-not [bool](Probe).controlTarget.navigating) 'Esc ends navigation')
KeyThenTick 'Escape'
[void](Assert (WaitProbe { param($q) "$($q.controlTarget.scope)" -eq '' } -TimeoutMs 1500) 'the next Esc closes the keymap editor')
KeyThenTick 'Enter'
[void](Assert (WaitProbe { param($q) [int64]$q.transport.playheadFrame -eq 0 } -TimeoutMs 1500) 'outside navigation Enter is Return to zero again')

Step 12 'Automation (G4.6): A shows the track''s lanes; Shift+Pencil draws a line; the Eraser sweeps it; Ctrl+Z'
Focus
Click 'rail.row.0'
Start-Sleep -Milliseconds 200
Key 'A'
[void](Assert (WaitProbe { param($q) $null -ne $q.layout.'widget.timeline.automation.canvas' } -TimeoutMs 2000) 'A shows track 1''s automation lanes (the chooser lane is laid out)')
Key '2'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1500) ('the Pencil is the tool (view.tool=' + (Probe).view.tool + ')'))
$canvas = LayoutRect 'widget.timeline.automation.canvas'
$half = [int]($canvas[2] / 3)
DragWithin 'widget.timeline.automation.canvas' (-$half) 6 $half (-6) 'Shift'
[void](Assert (WaitProbe { param($q) @($q.automation.lanes).Count -eq 1 -and [int]@($q.automation.lanes)[0].points -eq 2 } -TimeoutMs 2000) ('Shift+Pencil draws a two-point line (lanes ' + @((Probe).automation.lanes).Count + ')'))
Shot 'ss7-automation-line'
Key '4'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Eraser' } -TimeoutMs 1500) 'the Eraser is the tool')
DragWithin 'widget.timeline.automation.canvas' (-$half - 20) 0 ($half + 20) 0
[void](Assert (WaitProbe { param($q) [int]@($q.automation.lanes)[0].points -eq 0 } -TimeoutMs 2000) ('the Eraser sweeps both points away (points ' + @((Probe).automation.lanes)[0].points + ')'))
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [int]@($q.automation.lanes)[0].points -eq 2 } -TimeoutMs 2000) 'Ctrl+Z brings the line back (one undo per stroke)')
Key '1'
Key 'A'
[void](Assert (WaitProbe { param($q) $null -eq $q.layout.'widget.timeline.automation.canvas' } -TimeoutMs 2000) 'A hides the lanes again')

Step 13 'The master strip (G4.7): a Limiter in the master''s slot; DIM / MUTE; the LUFS readout measures the played mix'
OpenMixer
Click 'mixer.master.insert.0'
[void](WaitPopup)
Key 'Down'   # the master's kinds lead with the Limiter
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$(@($q.mixer.masterInserts)[0].kind)" -eq 'Limiter' } -TimeoutMs 2000) ('the Limiter lands in the master''s slot 1 (' + @((Probe).mixer.masterInserts)[0].kind + ')'))
Click 'mixer.master.insert.0' -Double
[void](Assert (WaitProbe { param($q) [bool]$q.fxEditor.visible -and "$($q.fxEditor.kind)" -eq 'Limiter' -and [bool]$q.fxEditor.grVisible } -TimeoutMs 2000) ('the double-click opens the master Limiter''s editor with its gain-reduction meter (kind=' + (Probe).fxEditor.kind + ')'))
Shot 'ss7-master-limiter'
Click 'mixer.fx.editor.close'
[void](Assert (WaitProbe { param($q) -not [bool]$q.fxEditor.visible } -TimeoutMs 1500) 'Close closes the master Limiter''s editor')
Click 'widget.master.monitor.dim'
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.monitorDim } -TimeoutMs 1500) 'DIM lights')
Click 'widget.master.monitor.mute'
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.monitorMute } -TimeoutMs 1500) 'MUTE lights')
Shot 'ss7-monitor-lit'
Click 'widget.master.monitor.mute'
Click 'widget.master.monitor.dim'
[void](Assert (WaitProbe { param($q) -not [bool]$q.mixer.monitorDim -and -not [bool]$q.mixer.monitorMute } -TimeoutMs 1500) 'DIM and MUTE clear')
Focus
Click 'rail.row.0'
Start-Sleep -Milliseconds 200
Key 'Ctrl+Shift+I'
$dlg = WaitDialog 'Import WAV Audio' 4000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -ge 1 } -TimeoutMs 8000) 'the fixture is on track 1')
Focus
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays')
if ([bool](Probe).audio.deviceOpen) {
  [void](Assert (WaitProbe { param($q) [bool]$q.mixer.loudnessValid -and "$($q.mixer.loudness)" -match '^~?-?\d+\.\d LUFS$' } -TimeoutMs 6000) ('the header''s LUFS readout measures the played mix (' + (Probe).mixer.loudness + ')'))
  Shot 'ss7-loudness'
} else {
  [void](Assert ("$((Probe).mixer.loudness)" -eq '-- LUFS') 'no output device: nothing is played, so the readout honestly shows --')
}
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops; the readout holds')

Step 14 'Save and close'
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
Close
