# SS-1 "First minute" (plan §6, G0 exit). Run by tools/session-drive.ps1 against the real exe.
#
# Authored on 2026-09-01 under G0.1, RED where the bugs are: every assertion here is the plan's
# step as written, not today's behaviour. The G0 items turn them green one by one (G0.2 Space
# toggle + widgets decline focus; G0.3 no callback teardown; G0.4 paint budget; G0.5 no rebuilds
# on placement edits; G1.1 keymap v2 for Ctrl+Shift+I / Ctrl+T / C / K / Enter). Where a red step
# would otherwise stop the rest of the script from being exercised, the script records the FAIL
# and takes the shipped path (toolbar button, old chord) so the later steps still run.
#
# Deviations from the plan text, logged in STATUS.md:
#  - the G0.6 fixture does not exist yet; the drive's -Fixture (tests/fixtures/sine_440_48k_mono.wav)
#    stands in for "the fixture's first stem".
#  - a fresh launch has NO project today (the shell paints "Create or open a Project"); step 1
#    asserts the plan's empty project (red) and then creates one through the real Ctrl+N chooser.

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss1-first-minute-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

Step 1 'Launch with no project'
Launch
$p = Probe
[void](Assert ([int]$p.version -eq 1) 'probe schema v1')
[void](Assert ($p.focusContext -eq 'Arrange') 'focus context is Arrange')
[void](Assert (-not [bool]$p.transport.isPlaying) 'transport stopped')
[void](Assert ([bool]$p.projectLoaded) 'an empty project exists at launch (plan SS-1.1)')
if (-not [bool]$p.projectLoaded) {
  # Ctrl+N is the plan's chord; today the first keypress after launch lands on the document
  # window, not the shell (probe focusOwner == the window title) — the G0.2 bug in its purest
  # form. Record it, then take the shipped mouse path (the New toolbar button) so the rest of
  # the script still runs against a project.
  Key 'Ctrl+N'
  $dlg = WaitDialog 'Create YES DAW Project' 2500
  [void](Assert ($dlg -ne [IntPtr]::Zero) 'Ctrl+N right after launch opens the New chooser (G0.2: keys go to the command router)')
  if ($dlg -eq [IntPtr]::Zero) {
    Click 'widget.project.new'
    $dlg = WaitDialog 'Create YES DAW Project' 5000
  }
  if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
  [void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'the native New chooser creates a project')
}
[void](Assert ($script:FirstProbeMs -le 3000) ('launch to first interactive tick <= 3 s (B6): ' + $script:FirstProbeMs + ' ms'))

Step 2 'Import the first stem (Ctrl+Shift+I, file chooser)'
Focus
Key 'Ctrl+Shift+I'
$dlg = WaitDialog 'Import WAV Audio' 2500
$opened = $dlg -ne [IntPtr]::Zero
[void](Assert $opened 'Ctrl+Shift+I opens the import chooser (keymap v2, G1.1)')
if (-not $opened) {
  Click 'widget.project.import_audio'
  $dlg = WaitDialog 'Import WAV Audio' 5000
}
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 1 } -TimeoutMs 8000) 'one clip on track 1 after import')
$clipKey = $null
$p = Probe
foreach ($prop in $p.layout.PSObject.Properties) { if ($prop.Name -like 'clip.*') { $clipKey = $prop.Name; break } }
[void](Assert ($null -ne $clipKey) 'layout publishes the imported clip by id')

Step 3 'Click an empty lane'
Focus
$lane = LayoutRect 'lane.0'
[void](Assert ($null -ne $lane) 'layout publishes lane.0')
if ($null -ne $lane) { Click 'lane.0' -OffsetX ([int]($lane[2] / 2) - 30) }
[void](Assert (WaitProbe { param($q) $q.selection.clips.Count -eq 0 } -TimeoutMs 1000) 'clicking empty lane clears the clip selection')

Step 4 'Space plays'
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space after clicking the lane starts playback')
$a = [long](Probe).transport.playheadFrame; Start-Sleep -Milliseconds 300; $b = [long](Probe).transport.playheadFrame
[void](Assert ($b -gt $a) 'playhead advances')

Step 5 'Space stops'
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space toggles to stop (G0.2)')
if ([bool](Probe).transport.isPlaying) { Key 'K' }   # shipped path so the next steps start from stopped

Step 6 'Play button'
Click 'widget.transport.play'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'clicking Play starts playback')

Step 7 'Space stops after a button click'
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops even though a button was clicked last (G0.2)')
if ([bool](Probe).transport.isPlaying) { Click 'widget.transport.stop' }

Step 8 'Snap combo, Esc, Space'
Click 'widget.timeline.snap.chooser'
Start-Sleep -Milliseconds 200
Key 'Esc'
Start-Sleep -Milliseconds 150
Key 'Space'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space plays after the snap combo took a click (G0.2)')
if (-not [bool](Probe).transport.isPlaying) { Click 'widget.transport.play'; [void](WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) }

Step 9 'K click, Space stop, Enter to zero'
$metroBefore = [bool](Probe).transport.metronome
Key 'K'
[void](Assert (WaitProbe { param($q) [bool]$q.transport.metronome -ne $metroBefore } -TimeoutMs 1000) 'K toggles the metronome click (keymap v2)')
if ([bool](Probe).transport.metronome -ne $metroBefore) { Key 'K' }
if (-not [bool](Probe).transport.isPlaying) { Click 'widget.transport.play'; [void](WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) }
Key 'Space'
[void](Assert (WaitProbe { param($q) -not [bool]$q.transport.isPlaying } -TimeoutMs 1500) 'Space stops')
if ([bool](Probe).transport.isPlaying) { Click 'widget.transport.stop' }
Key 'Enter'
[void](Assert (WaitProbe { param($q) [long]$q.transport.playheadFrame -eq 0 } -TimeoutMs 1000) 'Enter returns the playhead to zero (keymap v2)')
if ([long](Probe).transport.playheadFrame -ne 0) { Key 'Home' }

Step 10 'Fifty nudges while playing: no removals, no underruns'
if ($null -ne $clipKey) { Click $clipKey }
[void](Assert (WaitProbe { param($q) $q.selection.clips.Count -eq 1 } -TimeoutMs 1000) 'clicking the clip by id selects it')
Key 'Space'
[void](WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500)
if (-not [bool](Probe).transport.isPlaying) { Click 'widget.transport.play'; [void](WaitProbe { param($q) [bool]$q.transport.isPlaying } -TimeoutMs 1500) }
$before = Probe
$t0 = Get-Date
Key 'Alt+Right' -Repeat 50
$nudgeMs = [int]((Get-Date) - $t0).TotalMilliseconds
Start-Sleep -Milliseconds 400
$after = Probe
[void](Assert ([bool]$after.transport.isPlaying) 'still playing after fifty nudges')
[void](Assert ($nudgeMs -le 2500) ('fifty nudges injected within 2.5 s: ' + $nudgeMs + ' ms'))
[void](Assert ([int]$after.audio.callbackRemovals -eq 0) ('audio-callback removals == 0 (B3): ' + $after.audio.callbackRemovals))
$underruns = [int]$after.audio.underruns
if ($underruns -lt 0) { [void](Assert ([int]$after.audio.deadlineMisses -eq 0) ('driver cannot count xruns; deadline misses == 0 (B5): ' + $after.audio.deadlineMisses)) }
else { [void](Assert ($underruns -eq 0) ('underruns == 0 (B5): ' + $underruns)) }

Step 11 'Drag, split, undo while playing: no rebuilds'
$rebuildsBefore = [int](Probe).audio.rebuilds
$p = Probe
$clipKeyNow = $null
foreach ($prop in $p.layout.PSObject.Properties) { if ($prop.Name -like 'clip.*') { $clipKeyNow = $prop.Name; break } }
if ($null -ne $clipKeyNow) {
  $r = LayoutRect $clipKeyNow
  $fromX = [int]($r[0] + $r[2] / 2); $fromY = [int]($r[1] + $r[3] / 2)
  Drag ("{0},{1}" -f $fromX, $fromY) ("{0},{1}" -f ($fromX + 120), $fromY)
}
Key 'Ctrl+T'
Key 'Ctrl+Z'
Start-Sleep -Milliseconds 300
$p = Probe
[void](Assert ([bool]$p.transport.isPlaying) 'still playing after drag / split / undo')
[void](Assert ([int]$p.audio.rebuilds -eq $rebuildsBefore) ('engine rebuilds unchanged by placement edits (B4): ' + $rebuildsBefore + ' -> ' + $p.audio.rebuilds))
[void](Assert ([int]$p.view.trackCount -eq 1) 'Ctrl+T split a clip, it did not add a track (keymap v2)')

Step 12 'Play 5 s: paint p95 <= 8 ms'
if (-not [bool](Probe).transport.isPlaying) { Click 'widget.transport.play' }
Start-Sleep -Seconds 5
$p = Probe
[void](Assert ([double]$p.frame.paintP95Ms -le 8.0) ('paint per frame p95 <= 8 ms (B2): ' + ('{0:N2}' -f [double]$p.frame.paintP95Ms) + ' ms, renderer ' + $p.renderer))
Key 'Space'; Start-Sleep -Milliseconds 200
if ([bool](Probe).transport.isPlaying) { Click 'widget.transport.stop' }

Step 13 'Screenshots at three sizes'
foreach ($size in @(@(1280, 720), @(1920, 1080), @(2560, 1440))) {
  $ok = Resize $size[0] $size[1]
  [void](Assert $ok ('window resized to ' + $size[0] + 'x' + $size[1]))
  $shot = Shot ('ss1-' + $size[0] + 'x' + $size[1])
  [void](Assert ((Test-Path -LiteralPath $shot) -and ((Get-Item -LiteralPath $shot).Length -gt 1024)) ('screenshot ' + $size[0] + 'x' + $size[1]))
}

Step 14 'Save, relaunch with the bundle'
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 500
$saved = Probe
$bundlePath = [string]$saved.bundlePath
[void](Assert (-not [string]::IsNullOrWhiteSpace($bundlePath)) ('project has a bundle path: ' + $bundlePath))
$clipsBefore = [int]$saved.view.clipCount
Close
Start-Sleep -Milliseconds 500
if (-not [string]::IsNullOrWhiteSpace($bundlePath)) {
  Launch -Bundle $bundlePath
  $p = Probe
  [void](Assert ([bool]$p.projectLoaded) 'relaunch with the bundle opens it')
  [void](Assert ([int]$p.view.clipCount -eq $clipsBefore) ('same clip count after relaunch: ' + $p.view.clipCount))
  [void](Assert (-not [bool]$p.transport.isPlaying) 'transport stopped after relaunch')
  [void](Assert ([long]$p.transport.playheadFrame -eq 0) 'playhead at 0 after relaunch')
  Close
}
