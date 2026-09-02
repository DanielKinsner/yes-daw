# SS-3 "Edit a song" (plan §6, G2 exit). Run by tools/session-drive.ps1 against the real exe.
#
# Authored on 2026-09-02 under G3.1's UI checkpoint (the drive pause D14 was lifted that day; the
# G2 exit had recorded this script as pending). Every assertion is the plan's step as written; a
# step the shell cannot do yet asserts red and continues on the shipped path so the later steps
# still run. Context menus are driven by keys after the right-click (JUCE popup: Down moves the
# highlight across ENABLED items, separators do not count, Enter fires) — the item numbers below
# assume the exact state the preceding steps leave (which items are enabled is stated per pick).
#
# Deviations from the plan text, logged in STATUS.md:
#  - a fresh launch has no project (D3, G5.5): step 0 creates one and imports the fixture.
#  - "jump between markers" uses the table's Alt+, / Alt+. (Transport › Previous / Next Marker).
#  - "the fixture" is the drive's -Fixture (the song stem when generated, else the sine).

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss3-edit-a-song-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

function MenuPick([int] $itemIndex) {
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $itemIndex
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 250
}

function ClipKeys {
  $p = Probe
  $keys = @()
  foreach ($prop in $p.layout.PSObject.Properties) { if ($prop.Name -like 'clip.*') { $keys += $prop.Name } }
  # left to right; every call site wraps the result in @() (PowerShell unrolls one-element arrays)
  return @($keys | Sort-Object { [int](LayoutRect $_)[0] })
}

function RulerTimeRowY {
  $ruler = LayoutRect 'ruler'
  return (22 + 11 - [int]($ruler[3] / 2))
}
function RulerBarsRowY {
  $ruler = LayoutRect 'ruler'
  return (11 - [int]($ruler[3] / 2))
}
function RulerOffsetForX([int] $x) {
  $ruler = LayoutRect 'ruler'
  return [int]($x - ($ruler[0] + $ruler[2] / 2))
}

Step 0 'Launch, New, Import the fixture'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (D3: created through the real New chooser)')
Focus
Key 'Ctrl+Shift+I'
$dlg = WaitDialog 'Import WAV Audio' 4000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 1 } -TimeoutMs 8000) 'one clip after import')
Resize 1920 1080
Start-Sleep -Milliseconds 300
$clip = @(ClipKeys)[0]
$rect = LayoutRect $clip

Step 1 'Loop the chorus by a ruler drag (Shift+drag on the bars row)'
$x0 = [int]($rect[0] + $rect[2] * 0.25); $x1 = [int]($rect[0] + $rect[2] * 0.5)
DragWithin 'ruler' (RulerOffsetForX $x0) (RulerBarsRowY) (RulerOffsetForX $x1) (RulerBarsRowY) -Modifiers 'Shift'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.loop } -TimeoutMs 1500) 'a Shift-drag on the ruler sets the loop (probe transport.loop)')

Step 2 'Play'
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays')

Step 3 'While playing: split at the playhead (Ctrl+T)'
Click 'ruler' -OffsetX (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.4))) -OffsetY (RulerTimeRowY)
Click $clip
[void](WaitProbe { param($q) $q.selection.clips.Count -eq 1 } -TimeoutMs 1000)
Key 'Ctrl+T'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 2 } -TimeoutMs 2000) 'Ctrl+T splits while playing')
[void](Assert ([bool](Probe).transport.isPlaying) 'still playing after the split (no rebuild stall)')

Step 4 'Drag a clip to another track with snap; Ctrl defeats snap'
Key 'Ctrl+Shift+N'
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq 2 } -TimeoutMs 2000) 'Ctrl+Shift+N adds a track')
$clips = @(ClipKeys)
$second = $clips[1]
$lane1 = LayoutRect 'lane.1'
$r2 = LayoutRect $second
Click $second
Drag $second 'lane.1' -Modifiers '' | Out-Null
$moved = LayoutRect $second
[void](Assert (($null -ne $moved) -and ([int]$moved[1] -ge [int]$lane1[1])) 'the clip lands on track 2 (its rect moved into lane.1)')
$xBefore = [int](LayoutRect $second)[0]
DragWithin $second 0 0 37 0 -Modifiers 'Ctrl'
Start-Sleep -Milliseconds 200
$xAfter = [int](LayoutRect $second)[0]
# The press-to-drag dead zone eats the first pixels; a snapped drag would land on a grid line (a
# multiple of the beat width), a defeated one lands wherever the pointer stopped.
[void](Assert (($xAfter - $xBefore) -ge 10 -and ($xAfter - $xBefore) -le 37) ('a Ctrl-drag defeats snap (moved ' + ($xAfter - $xBefore) + ' px for a 37 px drag)'))
[void](Assert ([bool](Probe).transport.isPlaying) 'still playing after the drags')

Step 5 'Trim with preview (drag the right edge)'
$r = LayoutRect $second
$wBefore = [int]$r[2]
DragWithin $second ([int]($r[2] / 2) - 3) 0 ([int]($r[2] / 2) - 80) 0
Start-Sleep -Milliseconds 250
$wAfter = [int](LayoutRect $second)[2]
[void](Assert ($wAfter -lt $wBefore) ('a right-edge drag trims the clip: ' + $wBefore + ' -> ' + $wAfter + ' px'))

Step 6 'Fade in with a shape (inspector)'
Click $second
$fadeIn = LayoutRect 'widget.clip.inspector.fade_in'
[void](Assert ($null -ne $fadeIn) 'the inspector publishes the Fade In control')
$d0 = [int](Probe).commandDispatchCount
if ($null -ne $fadeIn) { Click 'widget.clip.inspector.fade_in' -OffsetX ([int]($fadeIn[2] * 0.3) - [int]($fadeIn[2] / 2)) }
$curve = LayoutRect 'widget.clip.inspector.fade_curve'
if ($null -ne $curve) { Click 'widget.clip.inspector.fade_curve'; Start-Sleep -Milliseconds 350; Key 'Down'; Start-Sleep -Milliseconds 80; Key 'Enter'; Start-Sleep -Milliseconds 250 }
[void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -ge $d0 + 2 } -TimeoutMs 2000) 'a fade-in length and a fade shape each dispatch an undoable edit')

Step 7 'Crossfade two overlapping clips'
Key 'Space'   # stop for the placement edits below (the plan keeps playing; the drags are cleaner stopped)
[void](WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500)
Click $second
Key 'Ctrl+D'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 3 } -TimeoutMs 2000) 'Ctrl+D duplicates the clip after itself')
$clips = @(ClipKeys)
$dup = $clips[$clips.Count - 1]
$rd = LayoutRect $dup
DragWithin $dup 0 0 (-[int]($rd[2] * 0.4)) 0 -Modifiers 'Ctrl'
Start-Sleep -Milliseconds 250
Click $second -OffsetX (-[int]((LayoutRect $second)[2] * 0.4))   # the part the copy does not cover
$rd = LayoutRect $dup
Click $dup -OffsetX ([int]($rd[2] * 0.4)) -Modifiers 'Shift'   # the copy's right part, clear of the original; Shift+click adds to the selection (Ctrl is the snap defeat)
[void](Assert (WaitProbe { param($q) $q.selection.clips.Count -eq 2 } -TimeoutMs 1000) 'Shift+click selects both overlapping clips')
$d0 = [int](Probe).commandDispatchCount
Click $dup -Right -OffsetX ([int]($rd[2] * 0.4))
Start-Sleep -Milliseconds 350; Shot 'ss3-evidence-clip-menu-two-selected'
# Enabled (observed): Cut, Copy, Duplicate, Delete, Split, Heal, Crossfade (Paste disabled) -> 7th.
MenuPick 7
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.clip.crossfade' } -TimeoutMs 2000) ('Crossfade from the clip menu dispatches (lastAction=' + (Probe).lastAction + ')'))

Step 8 'Slip a clip (Ctrl+Alt drag keeps its place, moves its content)'
Click $second
$xBefore = [int](LayoutRect $second)[0]
$d0 = [int](Probe).commandDispatchCount
DragWithin $second 0 0 30 0 -Modifiers 'Ctrl+Alt'
Start-Sleep -Milliseconds 250
$xAfter = [int](LayoutRect $second)[0]
[void](Assert ($xAfter -eq $xBefore) 'a slip leaves the clip where it is')
[void](Assert ([int](Probe).commandDispatchCount -ge $d0 + 1) 'a slip dispatches an undoable edit')

Step 9 'Stretch a clip to the loop (Clip menu)'
Click $second
$menubar = LayoutRect 'widget.shell.menubar'
Click 'widget.shell.menubar' -OffsetX (190 - [int]($menubar[2] / 2))
Start-Sleep -Milliseconds 350; Shot 'ss3-evidence-clip-menubar'
# Clip menu enabled (observed, the pair still selected): Split, Heal, Apply Default Fades, Set Fades, Crossfade, Set Gain, Gain+, Gain-, Move, Trim, Time Stretch, Stretch to Loop -> 12th.
MenuPick 12
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.clip.stretch_to_loop' } -TimeoutMs 2000) ('Stretch to Loop Length from the Clip menu dispatches (lastAction=' + (Probe).lastAction + ')'))

Step 10 'Colour and rename a clip'
Click $second
$d0 = [int](Probe).commandDispatchCount
Click $second -Right
Start-Sleep -Milliseconds 350; Shot 'ss3-evidence-clip-menu-one-selected'
# Enabled (observed): Cut, Copy, Duplicate, Delete, Split, Heal, Crossfade, Rename, Gain, Fades, Stretch, Mute, Colour -> 13th.
MenuPick 13
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.clip.colour_next' } -TimeoutMs 2000) ('Clip Colour: Next from the clip menu dispatches (lastAction=' + (Probe).lastAction + ')'))
Key 'Esc'   # in case a wrong pick left an editor open
Click $second
$d0 = [int](Probe).commandDispatchCount
Key 'F2'
Start-Sleep -Milliseconds 250
Key 'Ctrl+A'
TypeText 'Chorus'
Key 'Enter'
[void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -ge $d0 + 1 } -TimeoutMs 2000) 'F2 + a name + Enter renames the clip (an undoable edit)')

Step 11 'Mute a clip (Ctrl+M)'
Key 'Ctrl+M'
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.clip.toggle_mute' } -TimeoutMs 2000) 'Ctrl+M mutes the selected clip')

Step 12 'Add markers and jump between them'
Click 'ruler' -OffsetX (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.2))) -OffsetY (RulerTimeRowY)
Key 'M'
Click 'ruler' -OffsetX (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.6))) -OffsetY (RulerTimeRowY)
Key 'M'
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.marker.add' } -TimeoutMs 2000) 'M adds a marker at the playhead')
$atSecond = [long](Probe).transport.playheadFrame
Key 'Alt+,'
[void](Assert (WaitProbe { param($q) [long]$q.transport.playheadFrame -lt $atSecond } -TimeoutMs 1500) 'Alt+, jumps to the previous marker (plan §4.1)')
$atFirst = [long](Probe).transport.playheadFrame
Key 'Alt+.'
[void](Assert (WaitProbe { param($q) [long]$q.transport.playheadFrame -gt $atFirst } -TimeoutMs 1500) 'Alt+. jumps to the next marker')

Step 13 'Shuffle-delete a clip and see the neighbours close'
$chooser = LayoutRect 'widget.timeline.edit_mode.chooser'
[void](Assert ($null -ne $chooser) 'the edit-mode chooser is published at 1080p')
if ($null -ne $chooser) { Click 'widget.timeline.edit_mode.chooser'; Start-Sleep -Milliseconds 350; Shot 'ss3-evidence-editmode-popup'; Key 'Down' -Repeat 2; Start-Sleep -Milliseconds 80; Key 'Enter'; Start-Sleep -Milliseconds 250 }   # the combo's popup pre-highlights the current item: Overlap -> No Overlap -> Shuffle
[void](Assert (WaitProbe { param($q) $q.view.editMode -eq 'Shuffle' } -TimeoutMs 2000) 'the chooser sets Shuffle (probe view.editMode)')
$clips = @(ClipKeys)
# Track 2 (lane.1) holds the moved clip, its duplicate and the crossfade pair: delete its LEFTMOST
# clip and expect its right neighbours on the SAME track to close up.
$lane1 = LayoutRect 'lane.1'
$onLane1 = @($clips | Where-Object { $r = LayoutRect $_; $null -ne $r -and [int]$r[1] -ge [int]$lane1[1] -and [int]$r[1] -lt ([int]$lane1[1] + [int]$lane1[3]) })
[void](Assert ($onLane1.Count -ge 2) ('track 2 holds two or more clips for the Shuffle delete (' + $onLane1.Count + ')'))
$left = $onLane1[0]
$rightBeforeKeys = @($onLane1 | Where-Object { $_ -ne $left })
$rightBefore = @{}
foreach ($k in $rightBeforeKeys) { $rightBefore[$k] = [int](LayoutRect $k)[0] }
Click $left
[void](WaitProbe { param($q) $q.selection.clips.Count -ge 1 } -TimeoutMs 1000)
$countBeforeDel = [int](Probe).view.clipCount
$selBefore = (Probe).selection.clips.Count
$focusBefore = "$((Probe).focusOwner)"
Key 'Del'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -lt $countBeforeDel } -TimeoutMs 2000) ('Del removes the selected clip(s) (selected=' + $selBefore + ' focus=' + $focusBefore + ' lastAction=' + (Probe).lastAction + ')'))
$closed = $false
foreach ($k in $rightBeforeKeys) { $now = LayoutRect $k; if ($null -ne $now -and [int]$now[0] -lt $rightBefore[$k]) { $closed = $true } }
[void](Assert $closed 'in Shuffle a neighbour on the same track closes the gap')

Step 14 'Time-select two bars across three tracks and Ctrl+E'
Key 'Ctrl+Shift+N'
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq 3 } -TimeoutMs 2000) 'three tracks')
$ruler = LayoutRect 'ruler'
DragWithin 'ruler' (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.3))) (RulerTimeRowY) (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.45))) (RulerTimeRowY)
Start-Sleep -Milliseconds 250
[void](Assert ($null -ne (Probe).selection.timeRange) 'a plain ruler drag makes a Time selection (probe selection.timeRange)')
$countBefore = [int](Probe).view.clipCount
Key 'Ctrl+E'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -gt $countBefore } -TimeoutMs 2000) 'Ctrl+E splits the clips at the selection edges')

Step 15 'Nudge by 10 ms'
$nudge = LayoutRect 'widget.timeline.nudge.chooser'
[void](Assert ($null -ne $nudge) 'the nudge chooser is published at 1080p')
if ($null -ne $nudge) { Click 'widget.timeline.nudge.chooser'; Start-Sleep -Milliseconds 350; Key 'Down' -Repeat 5; Start-Sleep -Milliseconds 80; Key 'Enter'; Start-Sleep -Milliseconds 250 }   # Grid -> Bar -> Beat -> 16th -> 1 ms -> 10 ms
[void](Assert (WaitProbe { param($q) "$($q.view.nudgeValue)" -eq '5' -or "$($q.view.nudgeValue)" -match '10' } -TimeoutMs 2000) ('the nudge chooser sets 10 ms (probe view.nudgeValue = ' + (Probe).view.nudgeValue + ')'))
$clips = @(ClipKeys)
Click $clips[0]
$xBefore = [int](LayoutRect $clips[0])[0]
Key 'Alt+Right'
Start-Sleep -Milliseconds 250
$after = LayoutRect $clips[0]
[void](Assert (($null -ne $after) -and ([int]$after[0] -ge $xBefore - 1)) 'Alt+Right keeps the clip in place or moves it right (10 ms is under one pixel at fit zoom)')
[void](Assert ((Probe).lastAction -eq 'edit.nudge_right') 'the nudge dispatched')

Step 16 'Z to selection, Ctrl+0'
Key 'Z'
[void](Assert (WaitProbe { param($q) [double]$q.view.zoom -gt 1.0 } -TimeoutMs 1500) 'Z zooms to the Time selection')
Key 'Ctrl+0'
[void](Assert (WaitProbe { param($q) [double]$q.view.zoom -le 1.0001 } -TimeoutMs 1500) 'Ctrl+0 fits the project')

Step 17 'Add a tempo ramp and confirm the bar readout changes'
Click 'ruler' -OffsetX (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.5))) -OffsetY (RulerTimeRowY)
$readoutBefore = (Probe).view.counterPrimary
Click 'ruler' -Right -OffsetY (RulerTimeRowY)
Start-Sleep -Milliseconds 350; Shot 'ss3-evidence-ruler-menu'
# Ruler menu at a playhead away from tick 0 with no change there yet: Add Marker, Set Tempo, Set Meter, Add Tempo Change (4th; Remove disabled).
MenuPick 4
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.tempo.change_add' } -TimeoutMs 2000) ('Add Tempo Change at the playhead dispatches (lastAction=' + (Probe).lastAction + ')'))
Click 'ruler' -Right -OffsetY (RulerTimeRowY)
# Now a change sits at the playhead: Add Marker, Set Tempo, Set Meter, Add Tempo Change, Remove Tempo Change, Toggle Ramp -> 6th.
MenuPick 6
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.tempo.change_toggle_ramp' } -TimeoutMs 2000) ('Toggle Ramp dispatches (lastAction=' + (Probe).lastAction + ')'))
$tempo = LayoutRect 'widget.transport.set_tempo'
[void](Assert ($null -ne $tempo) 'the header tempo control is published')
if ($null -ne $tempo) {
  DragWithin 'widget.transport.set_tempo' 0 0 20 0 -Steps 8
  Start-Sleep -Milliseconds 300
}
Click 'ruler' -OffsetX (RulerOffsetForX ([int]($rect[0] + $rect[2] * 0.9))) -OffsetY (RulerTimeRowY)
$readoutAfter = (Probe).view.counterPrimary
[void](Assert ("$readoutBefore" -ne "$readoutAfter") ('the bar readout follows the map: ' + $readoutBefore + ' -> ' + $readoutAfter))

Step 18 'Undo twenty steps, redo twenty steps'
$d0 = [int](Probe).commandDispatchCount
$clipsBefore = [int](Probe).view.clipCount
Key 'Ctrl+Z' -Repeat 20
Start-Sleep -Milliseconds 600
Key 'Ctrl+Shift+Z' -Repeat 20
Start-Sleep -Milliseconds 600
$p = Probe
[void](Assert ([int]$p.commandDispatchCount -ge $d0 + 30) ('twenty undos and twenty redos dispatch (>= 30 counted, some steps had nothing left): ' + ([int]$p.commandDispatchCount - $d0)))
[void](Assert ([int]$p.view.clipCount -eq $clipsBefore) 'the clip count is back after undo x20 / redo x20')

Step 19 'Save, relaunch, byte-identical project, zoom / dock restored'
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
$p = Probe
$bundlePath = "$($p.bundlePath)"
$zoomBefore = [double]$p.view.zoom; $dockBefore = "$($p.view.dock)"
$db = Join-Path $bundlePath 'project.db'
[void](Assert (Test-Path -LiteralPath $db) ('the bundle has a project.db at ' + $db))
function DbHash([string] $path) {
  # SQLite keeps the file open; hash through a shared-read stream.
  $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try { $sha = [System.Security.Cryptography.SHA256]::Create(); return [BitConverter]::ToString($sha.ComputeHash($fs)) } finally { $fs.Dispose() }
}
$hashBefore = if (Test-Path -LiteralPath $db) { DbHash $db } else { '' }
$sessionDir = $script:SessionDir
$sizeBefore = (Get-Item -LiteralPath $db).Length
$walBefore = Test-Path -LiteralPath ($db + '-wal')
Close
Start-Sleep -Milliseconds 800
$hashClosed = if (Test-Path -LiteralPath $db) { DbHash $db } else { 'x' }
$sizeClosed = (Get-Item -LiteralPath $db).Length
[void](Assert ($hashBefore -eq $hashClosed) ('project.db is byte-identical after Close (quit writes nothing): ' + $sizeBefore + ' -> ' + $sizeClosed + ' bytes, wal-before=' + $walBefore))
Launch -Bundle $bundlePath -ReuseSessionDir $sessionDir
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 8000) 'relaunch reopens the saved project')
Start-Sleep -Milliseconds 800
$hashAfter = if (Test-Path -LiteralPath $db) { DbHash $db } else { 'x' }
$sizeAfter = (Get-Item -LiteralPath $db).Length
[void](Assert ($hashClosed -eq $hashAfter) ('project.db is byte-identical after the relaunch (open writes nothing): ' + $sizeClosed + ' -> ' + $sizeAfter + ' bytes, wal-now=' + (Test-Path -LiteralPath ($db + '-wal'))))
$p = Probe
[void](Assert ([math]::Abs([double]$p.view.zoom - $zoomBefore) -lt 0.001) ('zoom restored: ' + $zoomBefore + ' -> ' + $p.view.zoom))
[void](Assert ("$($p.view.dock)" -eq $dockBefore) ('dock restored: ' + $dockBefore + ' -> ' + $p.view.dock))

Step 20 'Screenshots at the three rubric sizes'
Resize 1280 720;  Start-Sleep -Milliseconds 400; Shot 'ss3-1280x720'
Resize 1920 1080; Start-Sleep -Milliseconds 400; Shot 'ss3-1920x1080'
Resize 2560 1440; Start-Sleep -Milliseconds 400; Shot 'ss3-2560x1440'
Close
