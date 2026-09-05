# SS-4 "Write a beat and a chord progression" (plan §6, the G3 exit). Run by tools/session-drive.ps1
# against the real exe.
#
# The plan's text, step for step: new project; add a MIDI track (Ctrl+Shift+N); pencil a drum pattern
# in drum mode on the Sampler with the fixture's one-shots; add a Keys track with SimpleSynth; draw a
# four-chord progression; quantize 80 % with swing; draw a filter sweep in a CC lane; arpeggiate with
# the MIDI FX; loop and audition; export the MIDI file; reopen it; render equals golden.
#
# Deviations from the plan text, logged in STATUS.md (the G3 close-out):
#  - a fresh launch has no project (D3): step 0 creates one through the real New chooser.
#  - "a MIDI track" is a Track whose instrument is set (there is no MIDI track kind): Ctrl+Shift+N
#    adds the Track, the TRACK tab's chooser picks the Sampler / SimpleSynth.
#  - "8-bar drum pattern": the Create MIDI Clip law makes a one-bar clip; the pattern is one bar,
#    looped by the transport loop in the audition step (the clip's own loop is G3.5's chooser).
#  - "the fixture's one-shots": the drive's -Fixture WAV on two pads (C3 and D3 — inside the roll's
#    default key window so the pencil reaches them without scrolling).
#  - "render equals golden": the golden is the render itself — the mix exported before the save /
#    close / relaunch is byte-identical to the mix exported after it (the reopen renders the same).
#  - "reopen it": the exported MIDI file is imported back (File > Import MIDI File) after the relaunch.

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss6-write-a-beat-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }
$stamp = (Get-Date).ToString('HHmmss')
$midPath = Join-Path ([System.IO.Path]::GetTempPath()) ('ss6-beat-' + $stamp + '.mid')
$wav1 = Join-Path ([System.IO.Path]::GetTempPath()) ('ss6-mix-before-' + $stamp + '.wav')
$wav2 = Join-Path ([System.IO.Path]::GetTempPath()) ('ss6-mix-after-' + $stamp + '.wav')
foreach ($f in @($midPath, $wav1, $wav2)) { if (Test-Path -LiteralPath $f) { Remove-Item -Force -LiteralPath $f } }

function MenuPick([int] $itemIndex) {
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $itemIndex
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 250
}
function RollOffset([int] $x, [int] $y) {
  $r = LayoutRect 'widget.piano-roll.canvas'
  return @(($x - [int]($r[2] / 2)), ($y - [int]($r[3] / 2)))
}
function GridPoint([double] $fx, [int] $key) {
  $roll = (Probe).view.pianoRoll
  $x = [int]([double]$roll.gridX + [double]$roll.gridWidth * $fx)
  $y = [int]([double]$roll.gridY + ([double]$roll.viewHighKey - $key + 0.5) * [double]$roll.rowHeight)
  return RollOffset $x $y
}
function PencilNote([double] $fx, [int] $key) {
  $p = GridPoint $fx $key
  Click 'widget.piano-roll.canvas' -OffsetX $p[0] -OffsetY $p[1]
  Start-Sleep -Milliseconds 120
}
function FileHash([string] $path) {
  $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try { $sha = [System.Security.Cryptography.SHA256]::Create(); return [BitConverter]::ToString($sha.ComputeHash($fs)) } finally { $fs.Dispose() }
}
function WaitFile([string] $path, [int] $TimeoutMs) {
  $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
  while ((Get-Date) -lt $deadline) { if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).Length -gt 44) { return $true }; Start-Sleep -Milliseconds 200 }
  return $false
}
# The Sampler's pad cell for a key (the panel's grid law: two rows of eight from C2, the lower row first).
function PadClick([int] $key) {
  $g = LayoutRect 'instrument.panel.pads'
  $index = $key - 36
  $col = $index % 8; $rowFromBottom = [int][math]::Floor($index / 8); $row = 1 - $rowFromBottom
  $cellW = ([int]$g[2] - 7 * 4) / 8; $cellH = ([int]$g[3] - 4) / 2
  $x = [int]($g[0] + $col * ($cellW + 4) + $cellW / 2); $y = [int]($g[1] + $row * ($cellH + 4) + $cellH / 2)
  Click 'instrument.panel.pads' -OffsetX ($x - [int]($g[0] + $g[2] / 2)) -OffsetY ($y - [int]($g[1] + $g[3] / 2))
}
function ChooseInstrument([int] $downs) {
  Click 'widget.inspector.tab.track'
  Start-Sleep -Milliseconds 250
  Click 'widget.track.inspector.instrument'
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $downs
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 300
}
function CreateMidiClipOnLane([int] $lane) {
  Click ('rail.row.' + $lane)
  [void](WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000)
  $r = LayoutRect ('lane.' + $lane)
  Click ('lane.' + $lane) -OffsetX ([int]($r[2] / 2) - 60) -Right
  MenuPick 1   # Paste is disabled (nothing copied): Create MIDI Clip is the 1st enabled item
  [void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.midi_clip.add' } -TimeoutMs 2000) ('Create MIDI Clip on lane ' + $lane + ' dispatches'))
  [void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'PianoRoll' } -TimeoutMs 2000) 'the roll opens on the new clip')
}

Step 0 'Launch, New'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (D3: created through the real New chooser)')
Resize 1920 1080
Start-Sleep -Milliseconds 300

Step 1 'Add a MIDI track (Ctrl+Shift+N); make it a Sampler'
Focus
$t0 = [int](Probe).view.trackCount
Key 'Ctrl+Shift+N'
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $t0 + 1 } -TimeoutMs 2000) 'Ctrl+Shift+N adds a Track')
Click 'rail.row.1'
[void](Assert (WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000) 'the rail click selects the new track')
ChooseInstrument 2   # the popup opens on None: two Downs reach the Sampler
[void](Assert (WaitProbe { param($q) "$($q.view.instrument)" -eq 'Sampler' } -TimeoutMs 3000) ('the TRACK tab chooser makes it a Sampler (view.instrument=' + (Probe).view.instrument + ')'))

Step 2 'Load the fixture one-shot onto two pads (C3, D3) through the panel'
Click 'widget.track.inspector.instrument.edit'
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Instrument' } -TimeoutMs 2000) 'Edit shows the Instrument dock tab')
DragWithin 'widget.shell.splitter.dock' 0 0 0 -160
Start-Sleep -Milliseconds 300
[void](Assert (WaitProbe { param($q) $null -ne $q.layout.'instrument.panel.pads' } -TimeoutMs 3000) 'the pad grid is laid out')
PadClick 48
$dlg = WaitDialog 'Load Sample onto Pad' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'a click on the C3 cell opens the sample chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.samplerPadCount -eq 1 } -TimeoutMs 8000) 'the fixture lands on the C3 pad (probe samplerPadCount)')
Focus
PadClick 50
$dlg = WaitDialog 'Load Sample onto Pad' 4000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.samplerPadCount -eq 2 } -TimeoutMs 8000) 'the fixture lands on the D3 pad too')
Shot 'ss6-sampler-pads'

Step 3 'Pencil a drum pattern in drum mode (kick on the beats, snare on 2 and 4)'
Focus
CreateMidiClipOnLane 1
[void](Assert (WaitProbe { param($q) [bool]$q.view.pianoRoll.drumMode } -TimeoutMs 2000) 'the roll is in drum mode on the Sampler track (the pads name the keys)')
$roll = (Probe).view.pianoRoll
[void](Assert ([int]$roll.viewLowKey -le 48 -and [int]$roll.viewHighKey -ge 50) ('the pads C3 / D3 sit in the key window (' + $roll.viewLowKey + '..' + $roll.viewHighKey + ')'))
Click 'tool.pencil'
[void](Assert (WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1000) 'the pencil is in charge')
foreach ($fx in @(0.02, 0.27, 0.52, 0.77)) { PencilNote $fx 48 }
foreach ($fx in @(0.27, 0.77)) { PencilNote $fx 50 }
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 6 } -TimeoutMs 2000) ('six drum hits pencilled (noteCount=' + (Probe).view.noteCount + ')'))
Shot 'ss6-drum-pattern'

Step 4 'Add a Keys track with SimpleSynth; draw a four-chord progression'
Click 'tool.pointer'
Focus
$t1 = [int](Probe).view.trackCount
Key 'Ctrl+Shift+N'
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $t1 + 1 } -TimeoutMs 2000) 'a third Track for the keys')
Click 'rail.row.2'
[void](WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000)
ChooseInstrument 1   # None -> SimpleSynth
[void](Assert (WaitProbe { param($q) "$($q.view.instrument)" -eq 'SimpleSynth' } -TimeoutMs 3000) 'the keys track plays the SimpleSynth')
CreateMidiClipOnLane 2
$roll = (Probe).view.pianoRoll
$root = [int](([int]$roll.viewLowKey + [int]$roll.viewHighKey) / 2) - 4
Click 'tool.pencil'
[void](WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1000)
# (PowerShell: the comma binds tighter than +, so every interval is parenthesized.)
$chords = @(, @($root, ($root + 4), ($root + 7))) + @(, @(($root + 5), ($root + 9), ($root + 12))) + @(, @(($root + 7), ($root + 11), ($root + 14))) + @(, @(($root + 2), ($root + 5), ($root + 9)))
$columns = @(0.04, 0.29, 0.54, 0.79)
for ($c = 0; $c -lt 4; $c++) { foreach ($k in $chords[$c]) { PencilNote $columns[$c] $k } }
[void](Assert (WaitProbe { param($q) [int]$q.view.noteCount -eq 12 } -TimeoutMs 2000) ('four three-note chords pencilled (noteCount=' + (Probe).view.noteCount + ')'))
Click 'tool.pointer'
Shot 'ss6-chords'

Step 5 'Quantize 80 % with swing (the CLIP tab panel; Apply)'
Click 'widget.inspector.tab.clip'
Start-Sleep -Milliseconds 250
[void](Assert ([bool](Probe).view.quantize.panel) 'the CLIP tab shows the quantize panel for the MIDI clip')
$s = LayoutRect 'inspector.quantize.strength'
DragWithin 'inspector.quantize.strength' ([int]($s[2] / 2) - 6) 0 ([int]($s[2] * 0.80) - [int]($s[2] / 2)) 0
$strength = [int](Probe).view.quantize.strength
[void](Assert ($strength -ge 70 -and $strength -le 90) ('the strength drag lands near 80 % (' + $strength + ')'))
$w = LayoutRect 'inspector.quantize.swing'
DragWithin 'inspector.quantize.swing' (6 - [int]($w[2] / 2)) 0 ([int]($w[2] * 0.45) - [int]($w[2] / 2)) 0
[void](Assert ([int](Probe).view.quantize.swing -gt 0) ('the swing drag sets swing (' + (Probe).view.quantize.swing + ' %)'))
Key 'Ctrl+A'
$d0 = [int](Probe).commandDispatchCount
Click 'inspector.quantize.apply'
[void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -gt $d0 } -TimeoutMs 2000) 'Apply quantizes the selected notes (a dispatch)')
[void](Assert ([int](Probe).view.noteCount -eq 12) 'quantize keeps every note')

Step 6 'Draw a filter sweep in the CC lane (Mod, the pencil)'
Click 'tool.pencil'
[void](WaitProbe { param($q) "$($q.view.tool)" -eq 'Pencil' } -TimeoutMs 1000)
$roll = (Probe).view.pianoRoll
$laneFrom = RollOffset ([int]([double]$roll.controlLaneX + [double]$roll.controlLaneWidth * 0.05)) ([int]([double]$roll.controlLaneY + [double]$roll.controlLaneHeight - 6))
$laneTo   = RollOffset ([int]([double]$roll.controlLaneX + [double]$roll.controlLaneWidth * 0.95)) ([int]([double]$roll.controlLaneY + 6))
$c0 = [int](Probe).view.pianoRoll.controlPointCount
DragWithin 'widget.piano-roll.canvas' $laneFrom[0] $laneFrom[1] $laneTo[0] $laneTo[1]
[void](Assert (WaitProbe { param($q) [int]$q.view.pianoRoll.controlPointCount -gt $c0 + 1 } -TimeoutMs 2000) ('the pencil paints the CC1 sweep (' + (Probe).view.pianoRoll.controlPointCount + ' points)'))
Click 'tool.pointer'
Shot 'ss6-cc-sweep'

Step 7 'Arpeggiate with the MIDI FX (the strip''s Add FX chooser)'
# X is the mixer dock's toggle (timeline.mixer_dock.toggle): with the roll in the dock the first press
# may only hide the dock; a second press shows the mixer.
Focus
Key 'X'
if (-not (WaitProbe { param($q) "$($q.view.dock)" -eq 'Mixer' } -TimeoutMs 1200)) { Key 'X' }
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Mixer' } -TimeoutMs 2000) ('the dock shows the mixer (view.dock=' + (Probe).view.dock + ')'))
$d0 = [int](Probe).commandDispatchCount
Click 'widget.mixer.fx.insert.add'
Start-Sleep -Milliseconds 350
Key 'Down' -Repeat 8   # EQ, Compressor, Delay, Reverb, Limiter, MIDI Transpose, MIDI Scale, Arpeggiator
Start-Sleep -Milliseconds 80
Key 'Enter'
[void](Assert (WaitProbe { param($q) [int]$q.commandDispatchCount -gt $d0 } -TimeoutMs 2000) 'the Arpeggiator lands on the keys track (the Add FX chooser dispatches one undoable insert)')
Shot 'ss6-arpeggiator'

Step 8 'Loop and audition'
Focus
Key 'C'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.loop } -TimeoutMs 1500) 'C turns the loop on (probe transport.loop)')
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays')
Start-Sleep -Milliseconds 2500
[void](Assert ([bool](Probe).transport.isPlaying) 'still playing after two and a half seconds of the loop')
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops')

Step 9 'Export the MIDI file (File > Export MIDI File)'
# The export takes the SELECTION when a clip is selected, else the whole project: a click on an empty
# stretch of a lane clears the clip selection so both tracks go into the file.
$lane0 = LayoutRect 'lane.0'
Click 'lane.0' -OffsetX ([int]($lane0[2] / 2) - 40)
[void](Assert (WaitProbe { param($q) $q.selection.clips.Count -eq 0 } -TimeoutMs 1500) 'an empty-lane click clears the clip selection (the export is the whole project)')
$menubar = LayoutRect 'widget.shell.menubar'
Click 'widget.shell.menubar' -OffsetX (20 - [int]($menubar[2] / 2))
MenuPick 8   # New, Open, Save, Save As, Import WAV, Export Audio, Import MIDI File, Export MIDI File -> 8th
$dlg = WaitDialog 'Export MIDI File' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'File > Export MIDI File opens the native chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $midPath }
[void](Assert (WaitFile $midPath 8000) ('the MIDI file is written: ' + $midPath))
$bytes = [System.IO.File]::ReadAllBytes($midPath)
[void](Assert (([System.Text.Encoding]::ASCII.GetString($bytes[0..3])) -eq 'MThd') 'the file is a Standard MIDI File (MThd)')
$ntrks = ([int]$bytes[10] -shl 8) + [int]$bytes[11]
[void](Assert ($ntrks -eq 3) ('the file carries the tempo track and both instrument tracks (ntrks=' + $ntrks + ')'))

Step 10 'Render the mix (Ctrl+B) — the golden'
Focus
Key 'Ctrl+B'
$dlg = WaitDialog 'Export YES DAW Mix' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Ctrl+B opens the export chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $wav1 }
[void](Assert (WaitFile $wav1 20000) ('the mix renders to ' + $wav1))

Step 11 'Save, close, relaunch — the same project'
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
$p = Probe
$bundlePath = "$($p.bundlePath)"
$sessionDir = $script:SessionDir
Close
Start-Sleep -Milliseconds 800
Launch -Bundle $bundlePath -ReuseSessionDir $sessionDir
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 8000) 'relaunch reopens the saved project')
Resize 1920 1080
Start-Sleep -Milliseconds 500
[void](Assert ([int](Probe).view.trackCount -eq 3) 'the three tracks are back')

Step 12 'Render again: the reopened project renders byte-identically (render equals golden)'
Focus
Key 'Ctrl+B'
$dlg = WaitDialog 'Export YES DAW Mix' 4000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $wav2 }
[void](Assert (WaitFile $wav2 20000) ('the second mix renders to ' + $wav2))
$h1 = FileHash $wav1; $h2 = FileHash $wav2
[void](Assert ($h1 -eq $h2) ('the reopened project renders the same bytes (' + (Get-Item -LiteralPath $wav1).Length + ' vs ' + (Get-Item -LiteralPath $wav2).Length + ' bytes)'))

Step 13 'Reopen the exported MIDI file (File > Import MIDI File)'
Focus
Click 'rail.row.0'
[void](WaitProbe { param($q) $q.selection.tracks.Count -eq 1 } -TimeoutMs 1000)
$t2 = [int](Probe).view.trackCount
$menubar = LayoutRect 'widget.shell.menubar'
Click 'widget.shell.menubar' -OffsetX (20 - [int]($menubar[2] / 2))
MenuPick 7
$dlg = WaitDialog 'Import MIDI File' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'File > Import MIDI File opens the native chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $midPath }
# The native chooser sometimes drops the first typed path right after a relaunch (the Step 0
# transient logged 2026-09-05): one retry through the same menu.
if (-not (WaitProbe { param($q) [int]$q.view.trackCount -eq $t2 + 1 } -TimeoutMs 4000)) {
  Focus
  Click 'widget.shell.menubar' -OffsetX (20 - [int]($menubar[2] / 2))
  MenuPick 7
  $dlg = WaitDialog 'Import MIDI File' 4000
  if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $midPath }
}
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $t2 + 1 } -TimeoutMs 8000) 'the two-track file lands (its second track adds a track)')
[void](Assert ((Probe).status.text -like 'Imported 2 MIDI clips*') ('the status line names the import (' + (Probe).status.text + ')'))
Shot 'ss6-reopened'

Step 14 'Screenshots at the three rubric sizes'
Resize 1280 720;  Start-Sleep -Milliseconds 400; Shot 'ss6-1280x720'
Resize 1920 1080; Start-Sleep -Milliseconds 400; Shot 'ss6-1920x1080'
Resize 2560 1440; Start-Sleep -Milliseconds 400; Shot 'ss6-2560x1440'
foreach ($f in @($midPath, $wav1, $wav2)) { Remove-Item -Force -LiteralPath $f -ErrorAction SilentlyContinue }
Close
