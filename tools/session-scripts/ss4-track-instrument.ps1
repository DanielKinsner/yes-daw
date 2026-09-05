# G3.1 see-it (a first slice of SS-4 "Write a beat and a chord progression", plan §6): the Track
# instrument by mouse. Run by tools/session-drive.ps1 against the real exe.
#
# Steps: New; select track 1 on the rail; right-click the lane -> Create MIDI Clip; the inspector's
# TRACK tab shows the Instrument row; choose SimpleSynth in its chooser (an undoable edit); Edit
# opens the Instrument dock tab; drag the cutoff row (an undoable parameter edit); Ctrl+Z undoes it;
# screenshots at the three rubric sizes with the panel open.

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss4-track-instrument-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

function MenuPick([int] $itemIndex) {
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $itemIndex
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 250
}

Step 0 'Launch, New'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (D3)')
Resize 1920 1080
Start-Sleep -Milliseconds 300

Step 1 'Select the track on the rail; right-click the lane -> Create MIDI Clip'
Click 'rail.row.0'
[void](Assert (WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000) 'the rail click selects track 1')
$lane = LayoutRect 'lane.0'
Click 'lane.0' -OffsetX ([int]($lane[2] / 2) - 60) -Right
MenuPick 1   # Paste is disabled (nothing copied): Create MIDI Clip is the 1st enabled item
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.midi_clip.add' } -TimeoutMs 2000) 'Create MIDI Clip from the lane menu dispatches')

Step 2 'The TRACK tab shows the Instrument row; choose SimpleSynth'
Click 'widget.inspector.tab.track'
$chooser = LayoutRect 'widget.track.inspector.instrument'
[void](Assert ($null -ne $chooser -and [int]$chooser[2] -gt 0) 'the TRACK tab publishes the instrument chooser')
[void](Assert ("$((Probe).view.instrument)" -eq 'None (auto)') ('the chooser reads None (auto): ' + (Probe).view.instrument))
$d0 = [int](Probe).commandDispatchCount
# The combo's popup pre-highlights the current item (None): one Down reaches SimpleSynth.
Click 'widget.track.inspector.instrument'
Start-Sleep -Milliseconds 350
Shot 'ss4-evidence-instrument-popup'
Key 'Down'
Start-Sleep -Milliseconds 80
Key 'Enter'
Start-Sleep -Milliseconds 250
[void](Assert (WaitProbe { param($q) "$($q.view.instrument)" -eq 'SimpleSynth' } -TimeoutMs 2000) 'choosing SimpleSynth sets the Track instrument (probe view.instrument)')
[void](Assert ([int](Probe).commandDispatchCount -eq $d0 + 1) 'the choice is one undoable dispatch')

Step 3 'Edit opens the Instrument dock tab; drag the cutoff row; Ctrl+Z'
Click 'widget.track.inspector.instrument.edit'
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Instrument' } -TimeoutMs 2000) 'Edit shows the Instrument dock tab (probe view.dock)')
$panel = LayoutRect 'widget.instrument.panel'
[void](Assert ($null -ne $panel -and [int]$panel[3] -gt 100) 'the instrument panel is published with real height')
if ($null -ne $panel) {
  # Row layout: inset 12, title 24, gap 6, rows 22 + gap 4; row 5 (cutoff) is the 6th row; the slider
  # spans from x = 12 + 96 (label) to w - 12 - 72 (readout).
  $w = [int]$panel[2]; $h = [int]$panel[3]
  $rowY = 12 + 24 + 6 + 5 * 26 + 11
  $sliderX0 = 12 + 96 + 4; $sliderX1 = $w - 12 - 72 - 4
  $fromX = [int]($sliderX0 + ($sliderX1 - $sliderX0) * 0.95); $toX = [int]($sliderX0 + ($sliderX1 - $sliderX0) * 0.3)
  $d0 = [int](Probe).commandDispatchCount
  DragWithin 'widget.instrument.panel' ($fromX - [int]($w / 2)) ($rowY - [int]($h / 2)) ($toX - [int]($w / 2)) ($rowY - [int]($h / 2))
  Start-Sleep -Milliseconds 300
  [void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -ge $d0 + 1 } -TimeoutMs 2000) 'a row drag dispatches an undoable instrument parameter edit (probe dispatch count)')
  $d1 = [int](Probe).commandDispatchCount
  Key 'Ctrl+Z'
  [void](Assert (WaitProbe { param($q) $q.lastAction -eq 'edit.undo' -and [int]$q.commandDispatchCount -ge $d1 + 1 } -TimeoutMs 2000) 'Ctrl+Z undoes the knob drag')
}

Step 5 'G3.9 the Sampler: the inspector chooser lists it; choosing it shows the pad grid in the panel'
# The chooser popup opens on its current item (SimpleSynth, the second): one Down + Enter picks Sampler.
Click 'widget.track.inspector.instrument'
Start-Sleep -Milliseconds 350
Key 'Down'
Key 'Enter'
[void](Assert (WaitProbe { param($q) "$($q.view.instrument)" -eq 'Sampler' } -TimeoutMs 3000) ('the chooser sets the Sampler (probe view.instrument=' + (Probe).view.instrument + ')'))
# The dock at its default height holds the parameter rows only; a user drags the dock splitter up for
# the kit (the section-fit law drops the grid whole when it does not fit) — 160 px is plenty.
DragWithin 'widget.shell.splitter.dock' 0 0 0 -160
Start-Sleep -Milliseconds 300
$panelNow = LayoutRect 'widget.instrument.panel'
[void](Assert (WaitProbe { param($q) $null -ne $q.layout.'instrument.panel.pads' } -TimeoutMs 3000) ('the pad grid is laid out in the instrument panel (panel h=' + $panelNow[3] + ')'))
[void](Assert ([int](Probe).view.samplerPadCount -eq 0) 'no pads yet (an empty grid)')
Shot 'ss4-sampler-pads'
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) "$($q.view.instrument)" -eq 'SimpleSynth' } -TimeoutMs 3000) 'Ctrl+Z returns the SimpleSynth')

Step 4 'Screenshots at the three rubric sizes with the panel open'
Resize 1280 720;  Start-Sleep -Milliseconds 400; Shot 'ss4-1280x720'
Resize 1920 1080; Start-Sleep -Milliseconds 400; Shot 'ss4-1920x1080'
Resize 2560 1440; Start-Sleep -Milliseconds 400; Shot 'ss4-2560x1440'
Close
