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
# G4 exit (2026-10-06) — SS-5 as the plan writes it: Step 14. Track 1 (the vocal: the fixture on it, routed
# to Bus 1 with EQ + Compressor) sends to a NEW reverb bus (the next empty well's New Bus) that takes a Reverb;
# the reverb bus is solo-safe from its strip menu; Bus 1's fader is ridden in Write mode while the song plays
# (a Bus Fader lane appears, and Write returns to Touch at stop); the mix exports to a WAV. Save and close is
# Step 15.
# G4.7 (2026-10-06) — the master strip: Step 13. The master pane's first slot takes a Limiter (Limiter is
# the first kind there) and its double-click opens the editor with the gain-reduction face; the header's DIM
# and MUTE light and clear; the fixture imported onto track 1 plays and the header's LUFS readout shows the
# measured loudness (when the drive machine has an output device — otherwise the honest "--" is asserted).
# Save and close moves to Step 14.
# G6.4 (2026-10-08, ADR-0072 §8) — Step 16: the real pointer over the header's meter chooser and tempo cell (native
# widgets) shows the 1 px hover line in the window's pixels, and held down each shows the 2 px pressed line.
# G6.4 (2026-10-08, ADR-0067 cp3) — Step 17: while the song plays, the real pointer visits the centre of every rail,
# strip and master control the shell has a record for (the probe's pointer.records) and the probe names each as hovered;
# no full invalidation, paint p95 <= 8 ms (B2) and no underrun (B5) across the sweep. Then, at 1280x720, 1920x1080 and
# 2560x1440 (ADR-0072's sweep): a hover, a press held on a fader's cap, a drag from that cap sideways onto the next
# strip's fader (the press stays on the first, the hover follows - a sideways drag moves no fader), and a right press
# that presses nothing; their shots and the keyboard ring with a hover go to the rubric. Save and close is Step 18.
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
# G6.3 (2026-10-07) — what a screen reader sees: Step 15, read through UI Automation (the tree Narrator and NVDA read).
# The router's Tab moves UI Automation's focus to the painted control's own element (ADR-0066 cp2): its name and type are
# the target's and it stands on the control; the rail mute's Toggle mutes the track as the click does; the strip fader's
# RangeValue sets the gain as one undo step; a screen reader's focus move onto a strip's mute hands the router that
# target; every painted element the shell keeps is in the tree. Save and close moves to Step 16.
#
# Deviations from the plan text (logged in STATUS.md, the G4.1 cp1 story): the recording device on the
# drive machine may have no inputs — the input-slot and R-cell steps then assert the honest refusal
# (the popup lists no inputs; the arm set stays empty) instead of the pick.
# G6.3's live step sits here, not in ss8 (where the README routes G6): SS-5's session has the most painted surfaces
# (rail rows, track and bus strips, the master), and ss8 does not exist yet (logged in STATUS.md, 2026-10-07).

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
$dlg = NewProjectChooser   # retries the first click itself (a fresh window's first click can only activate it)
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
# The locate reaches the probe's playhead through the transport, a tick or two after the key (2026-10-07: one read right
# after the key saw the old frame while lastAction already said transport.locate_next_grid): wait for it.
[void](Assert (WaitProbe { param($q) [int64]$q.transport.playheadFrame -gt $located } -TimeoutMs 1500) 'negative control: before Enter, Right is the editor''s and locates the playhead')
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
$dlg = WaitDialog 'Import Audio' 4000
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

Step 14 'SS-5 as written (the G4 exit): a reverb bus on a send; solo-safe it; Write the bus fader while playing; export'
OpenMixer
$busesBefore = @((Probe).mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count
Click 'mixer.strip.0.send.1'
[void](WaitPopup)
Key 'Up'   # the last item: New Bus
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.mixer.strips[0].sends[1].bus)" -eq 'Bus 2' -and @($q.mixer.strips | Where-Object { "$($_.kind)" -eq 'Bus' }).Count -eq $busesBefore + 1 } -TimeoutMs 2000) ('track 1 sends to a new reverb bus, Bus 2 (send 2 reads ' + (Probe).mixer.strips[0].sends[1].bus + ')'))
$reverbStrip = @((Probe).mixer.strips).Count - 1   # the new bus is the last strip
[void](Assert ("$((Probe).mixer.strips[$reverbStrip].name)" -eq 'Bus 2') ('the reverb bus is strip ' + $reverbStrip))
Click ('mixer.strip.' + $reverbStrip + '.insert.0')
[void](WaitPopup)
Key 'Down' -Repeat 4   # a Bus's kinds: EQ, Compressor, Delay, Reverb, Limiter
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$(@($q.mixer.strips[$reverbStrip].inserts)[0].kind)" -eq 'Reverb' } -TimeoutMs 2000) ('the reverb bus takes a Reverb (' + @((Probe).mixer.strips[$reverbStrip].inserts)[0].kind + ')'))
# Solo-safe: a new bus is born solo-safe (R10: returns and submixes stay audible under a solo). The bus's strip
# menu verb flips it — fourth from the end (Solo Safe, Narrow Strips, Add Bus, Remove Bus) — off, then on again.
[void](Assert ([bool](Probe).mixer.strips[$reverbStrip].soloSafe) 'the new reverb bus is solo-safe')
ClickStripHeader $reverbStrip -Right
MenuPickFromEnd 4
[void](Assert (WaitProbe { param($q) -not [bool]$q.mixer.strips[$reverbStrip].soloSafe } -TimeoutMs 2000) 'its strip menu''s Solo Safe turns the protection off')
ClickStripHeader $reverbStrip -Right
MenuPickFromEnd 4
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.strips[$reverbStrip].soloSafe } -TimeoutMs 2000) 'and on again: the reverb bus is solo-safe')
Shot 'ss7-reverb-bus'

# Write on Bus 1's fader while the song plays. The mode chooser shows with the lanes (A on track 1).
Focus
Click 'rail.row.0'
Start-Sleep -Milliseconds 200
Key 'A'
[void](Assert (WaitProbe { param($q) $null -ne $q.layout.'widget.timeline.automation.mode' } -TimeoutMs 2000) 'A shows the lanes and the automation mode chooser')
$mode = [int](Probe).automation.mode   # Read 0, Touch 1, Latch 2, Off 3, Write 4 — the chooser's order
Click 'widget.timeline.automation.mode'
[void](WaitPopup)
if (4 - $mode -gt 0) { Key 'Down' -Repeat (4 - $mode) }
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) [int]$q.automation.mode -eq 4 } -TimeoutMs 2000) ('the mode is Write (automation.mode=' + (Probe).automation.mode + ')'))
Key 'A'
$busLanes = { param($q) @($q.automation.lanes | Where-Object { [int]$_.role -eq 3 -and [int]$_.points -ge 2 }).Count }
[void](Assert ((& $busLanes (Probe)) -eq 0) 'no Bus Fader lane before the ride')
ClickStripHeader 3   # Bus 1: the strip Write writes
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays')
Start-Sleep -Milliseconds 400
DragWithin 'mixer.strip.3.fader.thumb' 0 0 0 70 -Steps 30   # the knob takes the press; down 70 px over ~0.8 s
Start-Sleep -Milliseconds 400
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops')
[void](Assert (WaitProbe { param($q) (& $busLanes $q) -ge 1 } -TimeoutMs 2000) ('the ride wrote a Bus Fader lane (lanes: ' + ((@((Probe).automation.lanes) | ForEach-Object { "$($_.role):$($_.points)" }) -join ' ') + ')'))
[void](Assert (WaitProbe { param($q) [int]$q.automation.mode -eq 1 } -TimeoutMs 2000) ('Write returns to Touch at stop (automation.mode=' + (Probe).automation.mode + ')'))
Shot 'ss7-write-ride'

# Export the mix.
$export = Join-Path ([System.IO.Path]::GetTempPath()) ('ss7-mix-' + (Get-Date).ToString('HHmmss') + '.wav')
if (Test-Path -LiteralPath $export) { Remove-Item -Force -LiteralPath $export }
$exportsBefore = [int](Probe).export.count
Focus
Click 'widget.project.export_audio'
$dlg = WaitDialog 'Export YES DAW Mix' 6000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Export opens the native save chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $export }
[void](Assert (WaitProbe { param($q) [int]$q.export.count -eq $exportsBefore + 1 -and -not [bool]$q.export.inProgress } -TimeoutMs 120000) ('the export finishes (export.count=' + (Probe).export.count + ', ' + (Probe).export.percent + '%)'))
$size = if (Test-Path -LiteralPath $export) { (Get-Item -LiteralPath $export).Length } else { 0 }
[void](Assert ($size -gt 1000000) ('the exported WAV is on disk (' + $size + ' bytes)'))
if (Test-Path -LiteralPath $export) { Remove-Item -Force -LiteralPath $export }

Step 15 'G6.3: a screen reader sees each painted control as its own element (UI Automation, live)'
# The element UI Automation reports focused once its name is the target's (the router's move is posted to the tree).
function UiaFocusedNamed([string] $name, [int] $TimeoutMs = 2000) {
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  do {
    $f = UiaFocused
    if ($null -ne $f -and $f.Name -eq $name) { return $f }
    Start-Sleep -Milliseconds 100
  } while ((Get-Date) -lt $deadline)
  return (UiaFocused)
}
# An element stands on a layout zone when its centre is the zone's centre (physical pixels, a pixel of rounding).
function StandsOn($element, [string] $layoutId) {
  if ($null -eq $element -or $null -eq (LayoutRect $layoutId)) { return $false }
  $c = ScreenPoint $layoutId
  return ([Math]::Abs(($element.X + $element.W / 2) - $c[0]) -le 2 -and [Math]::Abs(($element.Y + $element.H / 2) - $c[1]) -le 2)
}
Focus
Click 'timeline'   # Arrange takes the keys; Tab starts navigation from there
[void](WaitProbe { param($q) "$($q.focusContext)" -eq 'Arrange' } -TimeoutMs 1500)
[void](Assert (TabTo 'rail.row.0.mute') 'Tab reaches track 1''s rail mute cell')
$target = (Probe).controlTarget
$focused = UiaFocusedNamed "$($target.name)"
[void](Assert ($null -ne $focused -and $focused.ProcessId -eq $script:Proc.Id) 'UI Automation''s focus is in the app')
[void](Assert ($null -ne $focused -and $focused.Name -eq "$($target.name)") ('the focused element is the target by name (' + $focused.Name + ' / ' + $target.name + ')'))
[void](Assert ($null -ne $focused -and $focused.Type -eq 'CheckBox') ('a toggle reads as a check box (' + $focused.Type + ')'))
[void](Assert (StandsOn $focused 'rail.row.0.mute') ('the element stands on the painted cell (' + $focused.X + ',' + $focused.Y + ' ' + $focused.W + 'x' + $focused.H + ')'))
[void](Assert ("$((Probe).focusOwner)" -eq 'shell') ('the keyboard stays with the shell (' + (Probe).focusOwner + ')'))
$mutedBefore = [bool](Probe).mixer.strips[0].muted
if ($null -ne $focused) { $focused.Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle() }
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.strips[0].muted -ne $mutedBefore } -TimeoutMs 2000) 'the element''s Toggle mutes the track, as the click does')
$again = UiaFocused
[void](Assert ($null -ne $again -and $again.Toggle -eq $(if ($mutedBefore) { 'Off' } else { 'On' })) ('the element reads the new state (' + $again.Toggle + ')'))
if ($null -ne $again) { $again.Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle() }
[void](Assert (WaitProbe { param($q) [bool]$q.mixer.strips[0].muted -eq $mutedBefore } -TimeoutMs 2000) 'a second Toggle puts it back')
Shot 'ss7-uia-rail-mute'

# The strip fader's RangeValue: one set is one undo step.
Key 'Esc'
[void](WaitProbe { param($q) -not [bool]$q.controlTarget.navigating } -TimeoutMs 1500)
[void](Assert (TabTo 'mixer.strip.1.fader') 'Tab reaches strip 2''s painted fader')
$fader = UiaFocusedNamed "$((Probe).controlTarget.name)"
[void](Assert ($null -ne $fader -and $fader.Type -eq 'Slider' -and $null -ne $fader.Range) ('a fader reads as a slider with a range (' + $fader.Type + ' ' + $fader.Range + ')'))
[void](Assert (StandsOn $fader 'mixer.strip.1.fader') 'the fader element stands on the painted rail')
$gainBefore = [double](Probe).mixer.strips[1].linearGain
if ($null -ne $fader) { $fader.Element.GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).SetValue(0.5) }
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.mixer.strips[1].linearGain - 0.5) -lt 0.001 } -TimeoutMs 2000) ('RangeValue sets the gain (' + (Probe).mixer.strips[1].linearGain + ')'))
Key 'Esc'
[void](WaitProbe { param($q) -not [bool]$q.controlTarget.navigating } -TimeoutMs 1500)
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.mixer.strips[1].linearGain - $gainBefore) -lt 0.001 } -TimeoutMs 2000) 'one Ctrl+Z gives the gain back (the set was one undo step)')

# A screen reader's focus move hands the router the target (ADR-0049 / ADR-0066).
$stripMute = @(UiaFind "$((Probe).mixer.strips[2].name) mute" 'CheckBox' | Where-Object { StandsOn $_ 'mixer.strip.2.mute' })
[void](Assert ($stripMute.Count -eq 1) ('strip 3''s mute is one element in the tree (' + $stripMute.Count + ')'))
if ($stripMute.Count -ge 1) { $stripMute[0].Element.SetFocus() }
[void](Assert (WaitProbe { param($q) "$($q.controlTarget.id)" -eq 'mixer.strip.2.mute' } -TimeoutMs 2000) ('a screen reader''s focus move adopts the target (' + (Probe).controlTarget.id + ')'))

# Every painted zone the probe lays out has an element of its own standing on it (matched by centre, not by count: a
# count could be made up by widgets).
$elements = @(UiaFind '*' | Where-Object { $_.Type -in @('CheckBox', 'Button', 'Slider') -and $_.W -gt 0 })
$zones = @((Probe).layout.PSObject.Properties.Name | Where-Object { $_ -match '^(rail\.row\.\d+\.(mute|solo|arm|pan|volume)|mixer\.strip\.\d+\.(solo|mute|arm|pan|fader))$' })
$missing = @()
foreach ($zone in $zones) {
  $c = ScreenPoint $zone
  $hit = @($elements | Where-Object { [Math]::Abs(($_.X + $_.W / 2) - $c[0]) -le 2 -and [Math]::Abs(($_.Y + $_.H / 2) - $c[1]) -le 2 })
  if ($hit.Count -ne 1) { $missing += ($zone + ':' + $hit.Count) }
}
[void](Assert ($zones.Count -ge 20 -and $missing.Count -eq 0) ('every painted zone has exactly one element on it (' + $zones.Count + ' zones; off: ' + ($missing -join ' ') + ')'))
Shot 'ss7-uia-adopted'   # the ring on strip 3's mute, where the screen reader put it
Key 'Esc'

Step 16 'G6.4: the real pointer over and down on a native combo box and slider shows their lines (ADR-0072)'
Focus
Key 'Esc'
$meterRect = LayoutRect 'header.meter'; $tempoRect = LayoutRect 'header.tempo'
[void](Assert ($null -ne $meterRect -and $null -ne $tempoRect) 'the header lays out its meter chooser and tempo cell')
# The box each draws its line in: the chooser's bounds; the tempo cell's bar, 1 px inside (JUCE's bar border).
function VisibleBox([string] $target) {
  $r = LayoutRect $target
  if ($target -eq 'header.tempo') { return @(($r[0] + 1), ($r[1] + 1), ($r[2] - 2), ($r[3] - 2)) }
  return $r
}
Hover 'timeline'
$restShot = Shot 'ss7-pointer-rest'
foreach ($target in @('header.meter', 'header.tempo')) {
  Hover $target
  $why = HoverStrokeShown $restShot (Shot ('ss7-pointer-hover-' + $target.Replace('.', '-'))) (VisibleBox $target)
  [void](Assert ($why -eq '') ('the pointer over ' + $target + ' shows the 1 px line inside its edge' + $(if ($why) { ' (' + $why + ')' })))
}
# Held: the tempo cell (a press sets the tempo where it lands; one Ctrl+Z puts it back) and the meter chooser (its
# list opens; Esc closes it unchanged).
$bpm = [double](Probe).project.tempoBpm
$meterBefore = "$((Probe).project.meter)"
foreach ($target in @('header.tempo', 'header.meter')) {
  $rect = VisibleBox $target
  $width = if ([int](LayoutRect $target)[3] -lt 17) { 1 } else { 2 }   # ADR-0072: a widget too short for its text to clear 2 px presses at 1
  $pt = ScreenPoint $target
  [YesDawDrive]::MouseMoveAbs($pt[0], $pt[1]); Start-Sleep -Milliseconds 80
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 350
  $edges = if ($target -eq 'header.meter') { @('right') } else { @('left', 'right') }   # the chooser's text sits at its left
  $why = PressedStrokeShown (Shot ('ss7-pointer-pressed-' + $target.Replace('.', '-'))) $rect $width $edges
  [YesDawDrive]::MouseButton($false, $false); Start-Sleep -Milliseconds 200
  [void](Assert ($why -eq '') ('held down, ' + $target + ' shows the ' + $width + ' px pressed line' + $(if ($why) { ' (' + $why + ')' })))
  if ($target -eq 'header.meter') { Key 'Esc'; Start-Sleep -Milliseconds 200 }
}
if ([Math]::Abs([double](Probe).project.tempoBpm - $bpm) -gt 0.001) { Key 'Ctrl+Z'; Start-Sleep -Milliseconds 300 }
[void](Assert (WaitProbe { param($q) [Math]::Abs([double]$q.project.tempoBpm - $bpm) -le 0.001 } -TimeoutMs 2000) ('the tempo is back at ' + $bpm + ' (' + (Probe).project.tempoBpm + ')'))
[void](Assert ($meterBefore -ne '' -and "$((Probe).project.meter)" -eq $meterBefore) ('the meter is unchanged (' + (Probe).project.meter + ')'))

Step 17 'G6.4: the real pointer sweeps every rail, strip and master control while the song plays (ADR-0067 cp3)'
Focus
Key 'Esc'
Hover 'timeline'
Key 'Enter'   # outside control navigation Enter returns to zero
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 2000) 'the song plays')
Start-Sleep -Milliseconds 500
$before = Probe
$records = @($before.pointer.records | Where-Object { "$($_.id)" -match '^(rail\.row\.|mixer\.strip\.|mixer\.master\.)' })
$missed = @()
foreach ($record in $records) {
  $id = "$($record.id)"
  $cx = [int]([double]$record.rect[0] + [double]$record.rect[2] / 2); $cy = [int]([double]$record.rect[1] + [double]$record.rect[3] / 2)
  $pt = ScreenPoint ("{0},{1}" -f $cx, $cy)
  [YesDawDrive]::MouseMoveAbs($pt[0] - 1, $pt[1]); Start-Sleep -Milliseconds 15
  [YesDawDrive]::MouseMoveAbs($pt[0], $pt[1])
  if (-not (WaitProbe { param($q) "$($q.pointer.hovered)" -eq $id } -TimeoutMs 800)) { $missed += ($id + '->' + (Probe).pointer.hovered) }
}
$after = Probe
[void](Assert ($records.Count -ge 40) ('the sweep covers every rail, strip and master control the shell records (' + $records.Count + ')'))
[void](Assert ($missed.Count -eq 0) ('the real pointer at each centre hovers it (' + $missed.Count + ' off' + $(if ($missed.Count) { ': ' + (($missed | Select-Object -First 6) -join ' ') }) + ')'))
[void](Assert ([bool]$after.transport.isPlaying) 'still playing after the sweep')
[void](Assert ([int64]$after.frame.fullInvalidations -eq [int64]$before.frame.fullInvalidations) ('no full invalidation during the sweep (' + $before.frame.fullInvalidations + ' -> ' + $after.frame.fullInvalidations + ')'))
[void](Assert ([double]$after.frame.paintP95Ms -le 8.0) ('paint per frame p95 <= 8 ms while sweeping (B2): ' + ('{0:N2}' -f [double]$after.frame.paintP95Ms) + ' ms, renderer ' + $after.renderer))
$underruns = [int]$after.audio.underruns
$missDelta = [int]$after.audio.deadlineMisses - [int]$before.audio.deadlineMisses
$rtDetail = ' [sinceLaunch=' + $underruns + ' deadlineMisses=' + $after.audio.deadlineMisses + ' maxCallbackMs=' + ('{0:N2}' -f [double]$after.audio.maxCallbackMs) + ']'
if ($underruns -lt 0) { [void](Assert ($missDelta -eq 0) ('driver cannot count xruns; deadline misses during the sweep == 0 (B5): ' + $missDelta + $rtDetail)) }
else { [void](Assert (($underruns - [int]$before.audio.underruns) -eq 0) ('underruns during the sweep == 0 (B5): ' + ($underruns - [int]$before.audio.underruns) + $rtDetail)) }
# At the three plan sizes: hover, press, a drag that leaves the pressed control, a right press; shots for the rubric.
$sizeBefore = @([int](Probe).view.width, [int](Probe).view.height)
foreach ($size in @(@(1280, 720), @(1920, 1080), @(2560, 1440))) {
  $tag = '' + $size[0] + 'x' + $size[1]
  [void](Assert (Resize $size[0] $size[1]) ('window at ' + $tag))
  [void](Assert ($null -ne (LayoutRect 'mixer.strip.1.fader') -and $null -ne (LayoutRect 'mixer.strip.0.fader.thumb')) ('two strips show their faders at ' + $tag))
  Hover 'mixer.strip.0.pan'
  [void](Assert (WaitProbe { param($q) "$($q.pointer.hovered)" -eq 'mixer.strip.0.pan' } -TimeoutMs 800) ('the pan knob is hovered at ' + $tag))
  Shot ('ss7-sweep-hover-' + $tag)
  $cap = ScreenPoint 'mixer.strip.0.fader.thumb'
  $next = ScreenPoint 'mixer.strip.1.fader'
  [YesDawDrive]::MouseMoveAbs($cap[0], $cap[1]); Start-Sleep -Milliseconds 80
  [YesDawDrive]::MouseButton($true, $false); Start-Sleep -Milliseconds 250
  [void](Assert ("$((Probe).pointer.pressed)" -eq 'mixer.strip.0.fader') ('held on its cap, the fader is pressed at ' + $tag + ' (' + (Probe).pointer.pressed + ')'))
  Shot ('ss7-sweep-pressed-' + $tag)
  for ($i = 1; $i -le 10; $i++) { [YesDawDrive]::MouseMoveAbs([int]($cap[0] + ($next[0] - $cap[0]) * $i / 10), $cap[1]); Start-Sleep -Milliseconds 25 }   # sideways
  $dragged = WaitProbe { param($q) "$($q.pointer.hovered)" -eq 'mixer.strip.1.fader' -and "$($q.pointer.pressed)" -eq 'mixer.strip.0.fader' } -TimeoutMs 800
  [void](Assert $dragged ('dragged onto the next fader: pressed stays on the first, hover follows at ' + $tag + ' (pressed ' + (Probe).pointer.pressed + ', hovered ' + (Probe).pointer.hovered + ')'))
  Shot ('ss7-sweep-drag-' + $tag)
  [YesDawDrive]::MouseButton($false, $false); Start-Sleep -Milliseconds 200
  [void](Assert ("$((Probe).pointer.pressed)" -eq '' -and "$((Probe).pointer.hovered)" -eq 'mixer.strip.1.fader') ('released: nothing pressed, the hover stays at ' + $tag))
  $knob = ScreenPoint 'mixer.strip.1.pan'
  [YesDawDrive]::MouseMoveAbs($knob[0], $knob[1]); Start-Sleep -Milliseconds 80
  [YesDawDrive]::MouseButton($true, $true); Start-Sleep -Milliseconds 250
  [void](Assert ("$((Probe).pointer.pressed)" -eq '') ('a right press presses nothing at ' + $tag + ' (' + (Probe).pointer.pressed + ')'))
  [YesDawDrive]::MouseButton($false, $true); Start-Sleep -Milliseconds 250
  Key 'Esc'; Start-Sleep -Milliseconds 150   # its menu, if one opened
}
[void](Resize $sizeBefore[0] $sizeBefore[1])
Key 'Tab'
Hover 'mixer.strip.1.mute'
Shot 'ss7-sweep-ring-and-hover'
Key 'Esc'
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 2000) 'stopped')

Step 18 'Save and close'
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
Close
