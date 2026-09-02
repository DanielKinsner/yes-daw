# SS-2 "By mouse, then by keys" (plan §6, G1 exit). Run by tools/session-drive.ps1 against the real exe.
#
# Authored on 2026-09-02 under G3.1's UI checkpoint (the drive pause D14 was lifted on that day; the
# G1 exit had recorded this script as pending). Every assertion is the plan's step as written.
# Context menus are driven by keys after the right-click (JUCE popup: Down moves the highlight
# across the enabled items, separators do not count, Enter fires) — the item numbers below are the
# ContextMenus.h orders. Menu-bar menus are opened by clicking the menu's title; its x is measured
# from the shell's left edge (the menu bar is a shell child at the header's left).
#
# Deviations from the plan text, logged in STATUS.md:
#  - a fresh launch has no project (SS-1's D3, owned by G5.5): step 0 creates one through the
#    real New chooser and imports the fixture through the real Import chooser first.
#  - "Duplicate Track" and "Create MIDI Clip" have no chord in the plan's §4 table: the "same four
#    by chord" step uses the two chords that exist (Ctrl+T split, M marker).

$bundle = Join-Path ([System.IO.Path]::GetTempPath()) ('ss2-mouse-then-keys-' + (Get-Date).ToString('HHmmss') + '.yesdaw')
if (Test-Path -LiteralPath $bundle) { Remove-Item -Recurse -Force -LiteralPath $bundle }

function MenuPick([int] $itemIndex) {
  # After a right-click (or a menu-title click): highlight item N (1-based, counting ENABLED items
  # only — JUCE's popup skips disabled ones when arrowing; separators never count) and fire it.
  Start-Sleep -Milliseconds 350
  Key 'Down' -Repeat $itemIndex
  Start-Sleep -Milliseconds 80
  Key 'Enter'
  Start-Sleep -Milliseconds 200
}

function ClipKey {
  $p = Probe
  foreach ($prop in $p.layout.PSObject.Properties) { if ($prop.Name -like 'clip.*') { return $prop.Name } }
  return $null
}

Step 0 'Launch, New, Import the fixture'
Launch
Click 'widget.project.new'
$dlg = WaitDialog 'Create YES DAW Project' 6000
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundle }
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'a project exists (created through the real New chooser; D3)')
Focus
Key 'Ctrl+Shift+I'
$dlg = WaitDialog 'Import WAV Audio' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Ctrl+Shift+I opens the import chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $Fixture }
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 1 } -TimeoutMs 8000) 'one clip after import')
Resize 1920 1080
Start-Sleep -Milliseconds 300

Step 1 'Right-click clip -> Split at Playhead'
$clip = ClipKey
[void](Assert ($null -ne $clip) 'layout publishes the imported clip')
$rect = LayoutRect $clip
# Locate the playhead inside the clip (a plain click on the ruler's TIME row: bars row 22 px, time
# row the next 22 px), then split there. With nothing on the clipboard Paste is disabled, so the
# enabled items read Cut, Copy, Duplicate, Delete, Split: Split is the 5th.
$ruler = LayoutRect 'ruler'
Click 'ruler' -OffsetX ([int]($rect[0] + $rect[2] / 2 - ($ruler[0] + $ruler[2] / 2))) -OffsetY (22 + 11 - [int]($ruler[3] / 2))
Start-Sleep -Milliseconds 200
[void](Assert (WaitProbe { param($q) [long]$q.transport.playheadFrame -gt 0 } -TimeoutMs 1000) 'a click on the ruler time row locates the playhead inside the clip')
Click $clip
[void](Assert (WaitProbe { param($q) $q.selection.clips.Count -eq 1 } -TimeoutMs 1000) 'the clip is selected')
Click $clip -Right
MenuPick 5
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 2 } -TimeoutMs 2000) 'Split at Playhead from the clip menu makes two clips')
$dispatchAfterSplit = [int](Probe).commandDispatchCount

Step 2 'Right-click track header -> Duplicate Track'
Key 'Esc'   # dismiss any editor a missed pick may have left open
Start-Sleep -Milliseconds 150
$row = LayoutRect 'rail.row.0'
Click 'rail.row.0' -Right -OffsetX (60 - [int]($row[2] / 2))   # the name band, left of the mini cluster
MenuPick 2
$dupOk = WaitProbe { param($q) [int]$q.view.trackCount -eq 2 } -TimeoutMs 2000
[void](Assert $dupOk ('Duplicate Track from the header menu makes two tracks (lastAction=' + (Probe).lastAction + ' tracks=' + (Probe).view.trackCount + ')'))

Step 3 'Right-click ruler -> Add Marker'
Click 'ruler' -Right -OffsetY 11
MenuPick 1
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.marker.add' } -TimeoutMs 2000) 'Add Marker from the ruler menu dispatches (probe lastAction)')

Step 4 'Right-click empty lane -> Create MIDI Clip'
Key 'Esc'
Start-Sleep -Milliseconds 150
$lane = LayoutRect 'lane.1'
if ($null -eq $lane) { $lane = LayoutRect 'lane.0' }
[void](Assert ($null -ne $lane) 'layout publishes lane.1')
$laneKey = if ($null -ne (LayoutRect 'lane.1')) { 'lane.1' } else { 'lane.0' }
Click $laneKey -OffsetX ([int]($lane[2] / 2) - 40) -Right
MenuPick 1   # Paste is disabled (nothing copied) and skipped, so Create MIDI Clip is the 1st enabled item
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.midi_clip.add' } -TimeoutMs 2000) 'Create MIDI Clip from the lane menu dispatches')
$dispatchBeforeUndo = [int](Probe).commandDispatchCount

Step 5 'Menu Edit -> Undo x4 (probe counts)'
$menubar = LayoutRect 'widget.shell.menubar'
[void](Assert ($null -ne $menubar) 'layout publishes the menu bar')
for ($i = 0; $i -lt 4; $i++) {
  Click 'widget.shell.menubar' -OffsetX (81 - [int]($menubar[2] / 2))
  MenuPick 1
}
$p = Probe
[void](Assert ([int]$p.commandDispatchCount -eq $dispatchBeforeUndo + 4) ('four Undo dispatches through the Edit menu: ' + $p.commandDispatchCount + ' vs ' + ($dispatchBeforeUndo + 4)))
[void](Assert ([int]$p.view.trackCount -eq 1 -and [int]$p.view.clipCount -eq 1) 'four undos restore one track and one clip')

Step 6 'The same four by chord'
Click $clip
Key 'Ctrl+T'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 2 } -TimeoutMs 2000) 'Ctrl+T splits at the playhead (keymap v2)')
Key 'M'
[void](Assert (WaitProbe { param($q) $q.lastAction -eq 'timeline.marker.add' } -TimeoutMs 2000) 'M adds a marker')
# Duplicate Track and Create MIDI Clip have no chord in the plan's §4 table either: they are
# menu verbs (deviation logged in STATUS.md; the SS-2 text's "same four by chord" reads as "the
# chorded ones by chord").
Key 'Ctrl+Z' -Repeat 2
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq 1 } -TimeoutMs 2000) 'Ctrl+Z x2 restores the single clip')

Step 7 'Alt+K: rebind Split to Ctrl+Shift+T, relaunch, the chord works'
Key 'Alt+K'
[void](Assert (WaitProbe { param($q) [bool]$q.view.keymapEditor } -TimeoutMs 2000) 'Alt+K shows the keymap editor')
$ed = LayoutRect 'widget.keymap.editor'
[void](Assert ($null -ne $ed) 'layout publishes the keymap editor')
if ($null -ne $ed) {
  $w = [int]$ed[2]; $h = [int]$ed[3]
  # Search field: top row, after the title (inset 12 + title 90); list: below the top row + gap.
  Click 'widget.keymap.editor' -OffsetX (12 + 90 + 40 - [int]($w / 2)) -OffsetY (12 + 16 - [int]($h / 2))
  TypeText 'Split Clip'
  Start-Sleep -Milliseconds 300
  Click 'widget.keymap.editor' -OffsetX (12 + 100 - [int]($w / 2)) -OffsetY (12 + 32 + 8 + 12 - [int]($h / 2))
  Start-Sleep -Milliseconds 200
  # Chord field: bottom row's left 220 px.
  Click 'widget.keymap.editor' -OffsetX (12 + 110 - [int]($w / 2)) -OffsetY ([int]($h / 2) - 12 - 14)
  Key 'Ctrl+A'
  TypeText 'Ctrl+Shift+T'
  Shot 'ss2-evidence-keymap-before-enter'
  Key 'Enter'
  Start-Sleep -Milliseconds 300
  Shot 'ss2-evidence-keymap-after-enter'
}
# The chord field holds the focus (Esc / Alt+K would land in it): close through the editor's
# real Close button (top row, right; 72 px wide inside the 12 px inset).
if ($null -ne $ed) { Click 'widget.keymap.editor' -OffsetX ([int]($w / 2) - 12 - 36) -OffsetY (12 + 16 - [int]($h / 2)) }
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.keymapEditor } -TimeoutMs 2000) 'Close hides the keymap editor')
$overrides = Join-Path $script:SessionDir 'keymap-overrides.txt'
[void](Assert (Test-Path -LiteralPath $overrides) ('the override is persisted at bind time: ' + $overrides))
if (Test-Path -LiteralPath $overrides) { Write-Host ('  [keymap] ' + ((Get-Content -LiteralPath $overrides) -join ' | ')) }
Key 'Ctrl+S'
Start-Sleep -Milliseconds 500
$sessionDir = $script:SessionDir
Close
Start-Sleep -Milliseconds 800
Launch -Bundle $bundle -ReuseSessionDir $sessionDir
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded -and [int]$q.view.clipCount -ge 1 } -TimeoutMs 8000) 'relaunch reopens the saved project')
Resize 1920 1080
Start-Sleep -Milliseconds 300
Key 'Alt+K'
Start-Sleep -Milliseconds 400
Shot 'ss2-evidence-keymap-after-relaunch'
$ed = LayoutRect 'widget.keymap.editor'
if ($null -ne $ed) { $w = [int]$ed[2]; $h = [int]$ed[3]; Click 'widget.keymap.editor' -OffsetX ([int]($w / 2) - 12 - 36) -OffsetY (12 + 16 - [int]($h / 2)) }
Start-Sleep -Milliseconds 300
$clip = ClipKey
$rect = LayoutRect $clip
$ruler = LayoutRect 'ruler'
# Split needs the playhead INSIDE the clip (a fresh launch parks it at 0 = the clip's start).
Click 'ruler' -OffsetX ([int]($rect[0] + $rect[2] / 2 - ($ruler[0] + $ruler[2] / 2))) -OffsetY (22 + 11 - [int]($ruler[3] / 2))
Start-Sleep -Milliseconds 200
if ($null -ne $clip) { Click $clip }
[void](WaitProbe { param($q) $q.selection.clips.Count -eq 1 } -TimeoutMs 1000)
$before = [int](Probe).view.clipCount
Key 'Ctrl+Shift+T'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq $before + 1 } -TimeoutMs 2000) 'the rebound chord Ctrl+Shift+T splits after a relaunch (the override persisted)')

Step 8 'Screenshots at the three rubric sizes'
Resize 1280 720;  Start-Sleep -Milliseconds 400; Shot 'ss2-1280x720'
Resize 1920 1080; Start-Sleep -Milliseconds 400; Shot 'ss2-1920x1080'
Resize 2560 1440; Start-Sleep -Milliseconds 400; Shot 'ss2-2560x1440'
Close
