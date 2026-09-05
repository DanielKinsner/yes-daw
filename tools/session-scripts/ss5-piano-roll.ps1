# G3.2 see-it (a slice of SS-4 "Write a beat and a chord progression", plan §6): the piano roll dock
# v2 by mouse and keys. Run by tools/session-drive.ps1 against the real exe.
#
# Steps: New; select track 1; Create MIDI Clip from the lane menu (the roll opens); the pencil (2)
# draws three notes (each auditions); the pointer (1) presses a note (auditions) and Left / Right
# walk the selection; the keyboard column auditions its key; the scissors (3) Ctrl-click split a note;
# the eraser (4) deletes one; the velocity tool (5) drags a note's velocity as one undoable edit and
# Ctrl+Z restores it; a double-click on the empty grid adds a note; screenshots at the three sizes;
# the tool strip: a click on each cell selects that tool; G3.3: the control lane — the pencil paints a
# CC1 sweep in the lane (probe controlPointCount rises), Ctrl+Z clears it, the chooser is named.
#
# Geometry comes from the probe's view.pianoRoll (the roll's window, grid and painted notes), so the
# drive aims at what is painted instead of guessing.

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss5-piano-roll-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

function MenuPick([int] $itemIndex) {
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $itemIndex
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 250
}

# Centre-relative offsets inside the roll for a roll-local point.
function RollOffset([int] $x, [int] $y) {
  $r = LayoutRect 'widget.piano-roll.canvas'
  return @(($x - [int]($r[2] / 2)), ($y - [int]($r[3] / 2)))
}
# A grid point at a fraction of the visible width and a key's row centre.
function GridPoint([double] $fx, [int] $key) {
  $roll = (Probe).view.pianoRoll
  $x = [int]([double]$roll.gridX + [double]$roll.gridWidth * $fx)
  $y = [int]([double]$roll.gridY + ([double]$roll.viewHighKey - $key + 0.5) * [double]$roll.rowHeight)
  return RollOffset $x $y
}
# The painted centre of a note (by id) from the probe.
function NotePoint([string] $id) {
  $roll = (Probe).view.pianoRoll
  $note = @($roll.notes | Where-Object { "$($_.id)" -eq $id })[0]
  if ($null -eq $note) { throw "note $id not in the probe" }
  $ppt = [double]$roll.gridWidth / [Math]::Max(1, [double]$roll.visibleTicks)
  $x0 = [double]$roll.gridX + ([double]$note.start - [double]$roll.viewScrollTicks) * $ppt
  $x1 = $x0 + [double]$note.length * $ppt
  $x = [int](($x0 + $x1) / 2)
  $y = [int]([double]$roll.gridY + ([double]$roll.viewHighKey - [double]$note.key + 0.5) * [double]$roll.rowHeight)
  return RollOffset $x $y
}
function NoteIds { return @((Probe).view.pianoRoll.notes | ForEach-Object { "$($_.id)" }) }

Step 0 'Launch, New'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (D3)')
Resize 1920 1080
Start-Sleep -Milliseconds 300

Step 1 'Select the track; Create MIDI Clip from the lane menu; the roll opens'
Click 'rail.row.0'
[void](Assert (WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000) 'the rail click selects track 1')
$lane = LayoutRect 'lane.0'
Click 'lane.0' -OffsetX ([int]($lane[2] / 2) - 60) -Right
MenuPick 1   # Paste is disabled (nothing copied): Create MIDI Clip is the 1st enabled item
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.midi_clip.add' } -TimeoutMs 2000) 'Create MIDI Clip dispatches')
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'PianoRoll' } -TimeoutMs 2000) 'the dock shows the piano roll (probe view.dock)')
[void](Assert ([int](Probe).view.noteCount -eq 0) 'the new clip has no notes (probe view.noteCount)')
$roll = (Probe).view.pianoRoll
[void](Assert ([double]$roll.rowHeight -ge 10.0) ('the key window keeps legible rows at the default dock: ' + $roll.rowHeight + ' px'))
$keyMid = [int](([int]$roll.viewLowKey + [int]$roll.viewHighKey) / 2)
$keyHi = [int]$roll.viewHighKey - 2
$keyLo = [int]$roll.viewLowKey + 2

Step 2 'The pencil (2) draws three notes; each auditions'
Key '2'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1000) '2 selects the pencil')
$p1 = GridPoint 0.10 $keyHi
Click 'widget.piano-roll.canvas' -OffsetX $p1[0] -OffsetY $p1[1]
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 1 } -TimeoutMs 1500) 'the first pencil click adds a note')
[void](Assert ([int](Probe).view.lastAuditionKey -eq $keyHi) ('a drawn note auditions its key: ' + (Probe).view.lastAuditionKey + ' = ' + $keyHi))
$p2 = GridPoint 0.40 $keyMid
Click 'widget.piano-roll.canvas' -OffsetX $p2[0] -OffsetY $p2[1]
$p3 = GridPoint 0.70 $keyLo
Click 'widget.piano-roll.canvas' -OffsetX $p3[0] -OffsetY $p3[1]
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 3 } -TimeoutMs 1500) 'three pencil clicks, three notes')
$ids = NoteIds
[void](Assert ($ids.Count -eq 3) 'the probe publishes the three painted notes')
$byStart = @((Probe).view.pianoRoll.notes | Sort-Object { [double]$_.start } | ForEach-Object { "$($_.id)" })
Shot 'ss5-evidence-three-notes'

Step 3 'The pointer (1) presses a note (auditions); Left / Right walk the selection'
Key '1'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Pointer' } -TimeoutMs 1000) '1 selects the pointer')
$n1 = NotePoint $byStart[0]
Click 'widget.piano-roll.canvas' -OffsetX $n1[0] -OffsetY $n1[1]
[void](Assert (WaitProbe { param($q) $q.selection.notes.Count -eq 1 -and "$($q.selection.notes[0])" -eq $byStart[0] } -TimeoutMs 1500) 'the note press selects the first note')
[void](Assert ([int](Probe).view.lastAuditionKey -eq $keyHi) ('the note press auditions the note key: ' + (Probe).view.lastAuditionKey))
Key 'Right'
[void](Assert (WaitProbe { param($q) "$($q.selection.notes[0])" -eq $byStart[1] } -TimeoutMs 1500) 'Right selects the second note')
Key 'Right'
[void](Assert (WaitProbe { param($q) "$($q.selection.notes[0])" -eq $byStart[2] } -TimeoutMs 1500) 'Right again selects the third note')
Key 'Right'
Start-Sleep -Milliseconds 200
[void](Assert ("$((Probe).selection.notes[0])" -eq $byStart[2]) 'Right clamps at the last note')
Key 'Left'
[void](Assert (WaitProbe { param($q) "$($q.selection.notes[0])" -eq $byStart[1] } -TimeoutMs 1500) 'Left steps back to the second note')

Step 4 'The keyboard column auditions its key'
$roll = (Probe).view.pianoRoll
$kx = [int]$roll.keyboardX + [int]([int]$roll.keyboardWidth / 2)
$ky = [int]([double]$roll.gridY + ([double]$roll.viewHighKey - $keyMid + 0.5) * [double]$roll.rowHeight)
$k = RollOffset $kx $ky
$before = [int](Probe).view.lastAuditionKey
Click 'widget.piano-roll.canvas' -OffsetX $k[0] -OffsetY $k[1]
[void](Assert (WaitProbe { param($q) [int]$q.view.lastAuditionKey -eq $keyMid } -TimeoutMs 1500) ('the keyboard column auditions the key under the press: ' + (Probe).view.lastAuditionKey + ' = ' + $keyMid + ' (was ' + $before + ')'))
[void](Assert ([int](Probe).view.noteCount -eq 3) 'an audition is not an edit')

Step 5 'The scissors (3): Ctrl-click splits the third note'
Key '3'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Scissors' } -TimeoutMs 1000) '3 selects the scissors')
$d0 = [int](Probe).commandDispatchCount
$n3 = NotePoint $byStart[2]
Click 'widget.piano-roll.canvas' -OffsetX $n3[0] -OffsetY $n3[1] -Modifiers 'Ctrl'
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 4 } -TimeoutMs 1500) 'a Ctrl-click with the scissors splits the note into two')
[void](Assert ([int](Probe).commandDispatchCount -eq $d0 + 1) 'the split is one dispatch')

Step 6 'The eraser (4): a click deletes a note'
Key '4'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Eraser' } -TimeoutMs 1000) '4 selects the eraser')
$n2 = NotePoint $byStart[1]
Click 'widget.piano-roll.canvas' -OffsetX $n2[0] -OffsetY $n2[1]
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 3 } -TimeoutMs 1500) 'the eraser click deletes the second note')
[void](Assert (-not ((NoteIds) -contains $byStart[1])) 'the deleted note is gone from the roll')

Step 7 'The velocity tool (5): one drag, one undoable edit; Ctrl+Z restores'
Key '5'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Velocity' } -TimeoutMs 1000) '5 selects the velocity tool')
$d1 = [int](Probe).commandDispatchCount
$n1 = NotePoint $byStart[0]
DragWithin 'widget.piano-roll.canvas' $n1[0] $n1[1] $n1[0] ($n1[1] + 40)
# The Velocity tool's press also selects the note (one dispatch) before the release lands the edit;
# the ONE-undo-step law is the headless [piano-roll-v2] gate's pin.
[void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -ge $d1 + 1 } -TimeoutMs 2000) 'the velocity drag dispatches its edit')
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'edit.undo' } -TimeoutMs 2000) 'Ctrl+Z undoes the velocity drag')
[void](Assert ([int](Probe).view.noteCount -eq 3) 'the undo keeps the three notes')

Step 8 'The pointer (1): a double-click on the empty grid adds a note'
Key '1'
$p4 = GridPoint 0.90 ($keyMid + 1)
Click 'widget.piano-roll.canvas' -OffsetX $p4[0] -OffsetY $p4[1] -Double
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 4 } -TimeoutMs 1500) 'a double-click on the empty grid adds a note')

Step 9 'Screenshots at the three rubric sizes with the roll open'
Resize 1280 720;  Start-Sleep -Milliseconds 400; Shot 'ss5-1280x720'
Resize 1920 1080; Start-Sleep -Milliseconds 400; Shot 'ss5-1920x1080'
Resize 2560 1440; Start-Sleep -Milliseconds 400; Shot 'ss5-2560x1440'

Step 10 'The tool strip: a click on each cell selects that tool (mouse, not keys)'
foreach ($pair in @(@('tool.pencil','Pencil'), @('tool.scissors','Scissors'), @('tool.eraser','Eraser'),
                    @('tool.velocity','Velocity'), @('tool.zoom','Zoom'), @('tool.hand','Hand'), @('tool.pointer','Pointer'))) {
  $cell = $pair[0]; $name = $pair[1]
  Click $cell
  [void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq $name } -TimeoutMs 1000) ("clicking " + $cell + " selects the " + $name))
}
Shot 'ss5-tool-strip-pointer'

Step 11 'G3.3 the control lane: the pencil (2) paints a CC1 sweep, Ctrl+Z clears it'
[void](Assert ("$((Probe).view.pianoRoll.controlLane)" -eq 'Mod') 'the control lane opens on Mod (CC1)')
[void](Assert ((LayoutRect 'pianoroll.lane.chooser')[2] -gt 0) 'the lane chooser is laid out')
# By the strip cell, not the key: after Step 10's cell clicks the keyboard focus is on the strip,
# and a key the roll never sees would leave the pointer in charge (it places ONE point on release).
Click 'tool.pencil'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1000) 'the pencil is in charge for the lane sweep')
# Back to a window that fits ABOVE the taskbar: after Step 9's 2560x1440 the lane (the dock's lowest
# 84 px) sits under it, and a press there goes to the taskbar, never to the roll (found 2026-09-05).
Resize 1920 1080; Start-Sleep -Milliseconds 500
$roll = (Probe).view.pianoRoll
# The sweep spans most of the clip: the new clip is one bar and the default snap a beat, so a
# short sweep would paint a single point — the per-step law is the headless gate's pin.
$laneFrom = RollOffset ([int]([double]$roll.controlLaneX + [double]$roll.controlLaneWidth * 0.05)) ([int]([double]$roll.controlLaneY + [double]$roll.controlLaneHeight - 6))
$laneTo   = RollOffset ([int]([double]$roll.controlLaneX + [double]$roll.controlLaneWidth * 0.95)) ([int]([double]$roll.controlLaneY + 6))
$c0 = [int](Probe).view.pianoRoll.controlPointCount
DragWithin 'widget.piano-roll.canvas' $laneFrom[0] $laneFrom[1] $laneTo[0] $laneTo[1]
$painted = WaitProbe { param($q) [int]$q.view.pianoRoll.controlPointCount -gt $c0 + 1 } -TimeoutMs 2000
$q = Probe
[void](Assert $painted ('the pencil paints CC1 points across the sweep (count ' + $q.view.pianoRoll.controlPointCount + ' from ' + $c0 + '; gesture ' + $q.view.pianoRoll.laneGesture + '; last ' + $q.lastAction + ')'))
Shot 'ss5-control-lane-painted'
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [int]$q.view.pianoRoll.controlPointCount -eq $c0 } -TimeoutMs 2000) 'one Ctrl+Z clears the whole sweep')
Click 'tool.pointer'

Step 12 'G3.4 the quantize panel: the inspector shows it for the MIDI clip; Note ends toggles by click; Apply quantizes'
[void](Assert ([bool](Probe).view.quantize.panel) 'the CLIP tab shows the quantize panel for the MIDI clip')
[void](Assert ((LayoutRect 'inspector.quantize.strength')[2] -gt 0) 'the strength slider is laid out')
Click 'inspector.quantize.ends'
[void](Assert (WaitProbe { param($q) [bool]$q.view.quantize.noteEnds } -TimeoutMs 1500) 'a click toggles Note ends on')
[void](Assert ((Probe).lastAction -eq 'quantize.note_ends') 'the toggle names its action')
Click 'inspector.quantize.ends'
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.quantize.noteEnds } -TimeoutMs 1500) 'a second click toggles it off')

Step 13 'G3.5 the MIDI clip rows: Mute by click (the clip dims), the loop chooser is laid out; Ctrl+Z restores'
[void](Assert ((LayoutRect 'inspector.midi.loop')[2] -gt 0) 'the loop chooser is laid out')
Click 'inspector.midi.mute'
[void](Assert (WaitProbe { param($q) [bool]$q.view.midiClip.muted } -TimeoutMs 1500) 'a click mutes the MIDI clip')
Shot 'ss5-midi-clip-muted'
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.midiClip.muted } -TimeoutMs 1500) 'Ctrl+Z unmutes it')

Step 14 'G3.6 musical typing and step input: Ctrl+K, the A key plays C4, Step enters a note at the playhead'
Key 'Ctrl+K'
[void](Assert (WaitProbe { param($q) [bool]$q.view.musicalTyping.on } -TimeoutMs 1500) 'Ctrl+K turns musical typing on')
Key 'A'
[void](Assert (WaitProbe { param($q) [int]$q.view.musicalTyping.lastKey -eq 60 } -TimeoutMs 1500) 'the A key plays C4 (60)')
Key 'Z'
Key 'A'
[void](Assert (WaitProbe { param($q) [int]$q.view.musicalTyping.lastKey -eq 48 } -TimeoutMs 1500) 'Z drops an octave: A plays C3 (48)')
Click 'pianoroll.step'
[void](Assert (WaitProbe { param($q) [bool]$q.view.stepInput.on } -TimeoutMs 1500) 'the Step button turns step input on')
Key 'Home'
$n0 = [int](Probe).view.noteCount
Key 'A'
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq $n0 + 1 } -TimeoutMs 1500) 'a typed note enters the clip at the playhead')
[void](Assert ([int](Probe).transport.playheadFrame -gt 0) 'the playhead advanced one step')
Shot 'ss5-step-input'
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq $n0 } -TimeoutMs 1500) 'Ctrl+Z removes the entered note')
Click 'pianoroll.step'
Key 'Ctrl+K'
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.musicalTyping.on -and -not [bool]$q.view.stepInput.on } -TimeoutMs 1500) 'both modes off again')
Close
