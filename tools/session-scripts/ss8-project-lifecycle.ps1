# SS-6 "Project lifecycle / the Usable-song journey" (plan §6 under G5). Run by tools/session-drive.ps1 against the
# real exe. Every fixture, bundle and export output lives in a scratch temp dir under $env:TEMP; the repo and
# the user's %LOCALAPPDATA%\YES DAW tree are never written to by this script. The committed lossless fixtures
# under tests/fixtures/import/ are read-only. SS-6 grows this file.
#
# The plan's text, as this script executes it:
#  Step 1 - Create a project from a template with an explicit rate / tempo; open the browser and audition a
#            supported file. (The default template is "Default (one audio track)"; the chosen rate is 44.1 kHz
#            and the chosen tempo is 100 BPM, both honoured in the probe's project.sampleRateHz / tempoBpm.)
#  Step 2 - Drop several formats, including a different-rate asset, at a chosen lane / time via REAL OLE
#            drag-drop (`FileDrop` onto `lane.N` at an offset). The committed drive fixtures cover WAV / AIFF
#            / FLAC / Ogg Vorbis / cross-rate WAV; MP3 lands as a sixth format from the committed
#            cbr128_info.mp3. Assert placement (clipCount rises), track placement (per-lane), the asset-rate
#            match through probe.project.assets[].{hash,sampleRateHz,channels,frames} and the on-disk
#            audio/<hash>.asset files; one Ctrl+Z and one Ctrl+Shift+Z compare the states.
#  Step 3 - Import malformed and unsupported media. The malformed WAV goes through Ctrl+Shift+I: the chooser's
#            filter admits .wav, the decoder refuses with 'Import refused: <name>: <reason>'. The unsupported
#            .m4a is dragged onto a lane: the timeline declines it while it is dragged (drop effect None; the
#            chooser's filter never admits it either). Assert the refusals, that clipCount is unchanged and no new
#            audio/<hash>.asset lands in the bundle.
#  Step 4 - Reopen a scratch copy of SongA with one audio/<hash>.asset renamed aside. The shell asks through a
#            juce::AlertWindow ('Missing audio' or 'Damaged audio'); AlertButton 'Cancel' aborts the open
#            (relink.lastOutcome=='cancelled', projectLoaded=false). Reopen; AlertButton 'Locate...' opens a
#            native chooser 'Locate <name>'; feed it an unrelated WAV; the shell adopts, finds the hash
#            differs, re-asks with relink.refusal naming "is not the missing audio"; AlertButton 'Locate...'
#            again; feed it the sidelined file; adopted; relink.lastOutcome=='relinked', projectLoaded=true;
#            Ctrl+S writes the bundle so the next step opens cleanly.
#  Step 5 - Export the Mix + Stems at 24-bit PCM through the real settings row and Ctrl+B; parse the WAV
#            header to assert the bit depth; assert every top-level strip's stem file lands next to the mix,
#            all at the project rate. Cancel a second export mid-job (widget.project.export_audio.cancel):
#            export.lastResult=='cancelled', the destination is NOT on disk, no .partial sidecar lingers.
#            Start a third export; while in progress, trigger project replacement through
#            widget.project.new, answer the Unsaved-changes alert with 'Cancel' (so SongA stays current),
#            and assert the active export yielded by itself (ADR-0058: setExportCancelRequested fires along
#            with the native unsaved-changes question).
#  Step 6 - ADR-0068's three laps on a Save As'd scratch bundle at the drive's autosave cadence (Launch
#            -AutosaveIntervalMs 1000). The interruption: Save, set project.db aside, edit, wait for autosave.writes,
#            Kill, relaunch - no recovery question (the bundle holds every edit) and the project equals the last
#            confirmed autosave. Lost writes (a power cut, simulated): edit, wait for the autosave, Kill, put the
#            saved project.db back (-wal / -shm removed), relaunch - the question rises naming the autosave's
#            counts, Restore brings back the last confirmed autosave. A clean session: Save, close, relaunch - no
#            question, the autosave retired. Red until ADR-0068 lands (the shipped autosave never wrote).
#  Step 7 - Save As and Save a Copy. Save As re-points the bundle; Save a Copy writes a copy and the active
#            bundle stays unchanged. Flip two settings that persist to prefs.json (dock = Browser and the
#            mixer-strips narrow flag) and verify they survive a close + relaunch under the same
#            session-state dir (prefs.state=='loaded', view.dock, view.mixerNarrow). Reopen the copy and
#            assert the copy is loaded as its own bundle.
#
# Deviations from the plan text (also logged in STATUS.md's Deviation log):
#
#  D1 "Project replacement during another active export": R9 single-instance (session-drive.ps1 Launch
#      refuses a second YesDaw) forbids a sibling GUI process, and ADR-0058 refuses a second concurrent
#      export inside the same shell ("Export refused: an export is already running"). The step interprets
#      the plan's phrase as the SAME-shell project-replacement path (the model's ProjectAttach / Audio
#      ExportCancelRequested seam, ADR-0058 cp1) and asserts the refusal when a second concurrent export is
#      attempted. A sibling-process scenario would need a cross-process probe multiplexer and a second
#      Launch primitive; neither is in-scope for SS-6.
#  D2 "Autosave recovery through the shipped path" (Step 6) is known-red on today's shipped app
#      (ADR-0068, Proposed 2026-10-07). The step runs the full sequence, the FAIL message names the ADR
#      so the drive goes green by itself once the ADR lands.

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------------------------------
# Scratch workspace (every path under $env:TEMP; the repo and %LOCALAPPDATA% are untouched)
# ---------------------------------------------------------------------------------------------------
$scratchDir    = Join-Path ([System.IO.Path]::GetTempPath()) ('ss8-ss6-' + (Get-Date).ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $scratchDir | Out-Null
Write-Host ("  [ss8] scratch dir: " + $scratchDir)

$bundleA       = Join-Path $scratchDir 'SongA.yesdaw'
$bundleScratch = Join-Path $scratchDir 'SongA_scratch.yesdaw'
$bundleAuto    = Join-Path $scratchDir 'SongA_autosave.yesdaw'
$bundleSaveAs  = Join-Path $scratchDir 'SongA_as.yesdaw'
$bundleCopy    = Join-Path $scratchDir 'SongA_copy.yesdaw'
$exportDir     = Join-Path $scratchDir 'exports'
New-Item -ItemType Directory -Force -Path $exportDir | Out-Null

# The committed, read-only fixtures the drive drops through the real window (tests/fixtures/import/README.md
# names each file). $root is set by session-drive.ps1 to the repo root before this script is dot-sourced.
$fixAiff        = Join-Path $root 'tests\fixtures\import\drive_48k_stereo.aiff'
$fixFlac        = Join-Path $root 'tests\fixtures\import\drive_48k_stereo.flac'
$fixOgg         = Join-Path $root 'tests\fixtures\import\drive_48k_stereo.ogg'
$fixCrossRate   = Join-Path $root 'tests\fixtures\import\drive_44k_mono.wav'
$fixSupportedWav = Join-Path $root 'tests\fixtures\import\source_48k_stereo.wav'
$fixMp3         = Join-Path $root 'tests\fixtures\import\cbr128_info.mp3'
foreach ($f in @($fixAiff, $fixFlac, $fixOgg, $fixCrossRate, $fixSupportedWav, $fixMp3)) {
  if (-not (Test-Path -LiteralPath $f)) { throw "fixture missing from repo: $f" }
}

# ---------------------------------------------------------------------------------------------------
# Scratch-only failure fixtures (never committed; the shipped import pipeline refuses each)
# ---------------------------------------------------------------------------------------------------
# A valid-looking RIFF header followed by garbage, so the chooser's filter admits it as .wav but the decoder
# refuses it with "Import refused: <name>: <reason>".
$malformedWav = Join-Path $scratchDir 'malformed.wav'
$garbage = New-Object byte[] 1024
(New-Object System.Random 42).NextBytes($garbage)
$prefix = [System.Text.Encoding]::ASCII.GetBytes('RIFF????WAVE')
[System.IO.File]::WriteAllBytes($malformedWav, ($prefix + $garbage))

# An .m4a with plain-text payload: the chooser's filter never admits it, so the drive drops through FileDrop.
# ADR-0054's importAudioFromPath refuses the unknown extension before any decode runs.
$unsupportedM4a = Join-Path $scratchDir 'notaudio.m4a'
[System.IO.File]::WriteAllText($unsupportedM4a, 'this is definitely not audio')

# A valid-looking WAV that is NOT the sidelined asset (Step 4's "incompatible file"): the relink flow
# computes the decoded SHA-256 and refuses with "is not the missing audio" because the hash differs.
$otherWav = Join-Path $scratchDir 'other.wav'
Copy-Item -LiteralPath $fixCrossRate -Destination $otherWav   # 44.1 kHz mono, same shape, different content

# ---------------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------------
function Get-BundleAssetFiles([string]$bundle) {
  $audio = Join-Path $bundle 'audio'
  if (-not (Test-Path -LiteralPath $audio)) { return @() }
  return @(Get-ChildItem -LiteralPath $audio -Filter '*.asset' -ErrorAction SilentlyContinue)
}

# A JUCE ComboBox opens its popup on its CURRENT item. Pick a one-based `id` deterministically with Home + Down.
# (ss7 lesson: a combo count from the top is not stable; Home lands on item 1 and Down moves one at a time.)
# A chooser's item by its text (UI Automation, a real click). JUCE popups have no Home key and open on the CURRENT
# item, so counting Downs from "the top" picked the wrong rate on the first live run (2026-10-07).
function SelectCombo([string]$id, [string]$itemText) {
  if ($null -eq (LayoutRect $id)) { [void](Assert $false ('the chooser ' + $id + ' is laid out')); return }
  Click $id
  [void](MenuPickByName $itemText)
  Start-Sleep -Milliseconds 150
}

# The tempo slider carries a 56 px text box at its right end (UiTheme::Layout::newProjectTempoTextWidth).
function SetNewProjectTempo([int]$bpm) {
  $r = LayoutRect 'newproject.tempo'
  if ($null -eq $r) { return $false }
  # Click into the right-edge text box: offset = (sliderWidth/2 - 28), i.e. the middle of the 56 px box.
  Click 'newproject.tempo' -OffsetX ([int]($r[2] / 2) - 28)
  Start-Sleep -Milliseconds 150
  Key 'Ctrl+A'
  Start-Sleep -Milliseconds 60
  TypeText ([string]$bpm)
  Key 'Enter'
  Start-Sleep -Milliseconds 200
  return $true
}

# Drop a file onto lane $lane at a chosen offset. The offset is from the lane rect's centre; a negative
# $fractionFromLeft (0..1) lands near the left edge. Returns the FileDrop effect string ("Copy"/"timeout"/...).
function DropFileOnLane([string]$path, [int]$lane, [double]$fractionFromLeft) {
  $r = LayoutRect ("lane." + $lane)
  if ($null -eq $r) { throw "lane.$lane is not laid out" }
  # OffsetX relative to the lane's centre: left edge = -w/2 + 20 px of padding.
  $ox = [int](-([int]$r[2] / 2) + [Math]::Max(24, [int]([int]$r[2] * $fractionFromLeft)))
  return (FileDrop ("lane." + $lane) @($path) -OffsetX $ox -OffsetY 0)
}

# Parse a RIFF WAV header. Returns a hashtable with fmt (1 = PCM, 3 = IEEE float), channels, sampleRate, bits.
function Read-WavHeader([string]$path) {
  $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
  try {
    $buf = New-Object byte[] 44
    [void]$fs.Read($buf, 0, 44)
  } finally { $fs.Close() }
  return @{
    riff        = [System.Text.Encoding]::ASCII.GetString($buf, 0, 4)
    wave        = [System.Text.Encoding]::ASCII.GetString($buf, 8, 4)
    fmt         = [System.BitConverter]::ToUInt16($buf, 20)
    channels    = [System.BitConverter]::ToUInt16($buf, 22)
    sampleRate  = [System.BitConverter]::ToUInt32($buf, 24)
    bits        = [System.BitConverter]::ToUInt16($buf, 34)
  }
}

function Wait-ExportStart([int]$beforeCount, [int]$TimeoutMs = 6000) {
  return (WaitProbe { param($q) [bool]$q.export.inProgress -or [int]$q.export.count -ge $beforeCount + 1 } -TimeoutMs $TimeoutMs)
}

function Wait-ExportFinish([int]$beforeCount, [int]$TimeoutMs = 120000) {
  return (WaitProbe { param($q) [int]$q.export.count -ge $beforeCount + 1 -and -not [bool]$q.export.inProgress } -TimeoutMs $TimeoutMs)
}

# -------------------------------------------------------------------------------------------
# Step 1 - Create a project from a template with an explicit rate / tempo
# -------------------------------------------------------------------------------------------
Step 1 'Create a project from a template with an explicit rate / tempo; browser; audition'
Launch
[void](Assert ([int](Probe).version -eq 1) 'probe schema v1')

# Inline the two hops of NewProjectChooser so the rate / tempo / template are set BEFORE Create.
# (ADR-0060: the overlay carries those choices; its Create then opens the native location chooser.)
Focus
Click 'widget.project.new'
$overlayUp = WaitProbe { param($q) [bool]$q.view.newProjectDialog } -TimeoutMs 3000
if (-not $overlayUp) {   # the first click on a freshly launched window can only activate it (ss7 lesson)
  Focus
  Start-Sleep -Milliseconds 300
  Click 'widget.project.new'
  $overlayUp = WaitProbe { param($q) [bool]$q.view.newProjectDialog } -TimeoutMs 3000
}
[void](Assert $overlayUp 'New opens the New Project overlay (ADR-0060, G5.5)')
[void](Assert ($null -ne (LayoutRect 'newproject.rate') -and $null -ne (LayoutRect 'newproject.tempo') -and $null -ne (LayoutRect 'newproject.template') -and $null -ne (LayoutRect 'newproject.create')) 'the overlay lays out rate, tempo, template, Create')

SelectCombo 'newproject.rate' '44.1 kHz'
$tempoSet = SetNewProjectTempo 100            # the smallest round number below the 120 default
[void](Assert $tempoSet 'the tempo slider lays out its editable text box')
SelectCombo 'newproject.template' 'Default (one audio track)'   # deterministic across machines
Shot 'ss8-newproject-overlay'

Click 'newproject.create'
$dlg = WaitDialog 'Create YES DAW Project' 6000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Create opens the native project-location chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $bundleA }

$loaded = WaitProbe { param($q) [bool]$q.projectLoaded -and [string]$q.bundlePath -eq $bundleA } -TimeoutMs 8000
[void](Assert $loaded ('the project opens at the requested bundle: ' + $bundleA))

# The honoured rate and tempo are mechanical in the probe (SS-6 project object).
$pr = Probe
[void](Assert ([int]$pr.project.sampleRateHz -eq 44100) ('project runs at the chosen 44.1 kHz (project.sampleRateHz=' + $pr.project.sampleRateHz + ')'))
[void](Assert ([Math]::Abs([double]$pr.project.tempoBpm - 100.0) -lt 0.5) ('project runs at the chosen 100 BPM (project.tempoBpm=' + $pr.project.tempoBpm + ')'))
[void](Assert ([int]$pr.project.trackCount -ge 1) ('the Default template brings one audio track (trackCount=' + $pr.project.trackCount + ')'))

Resize 1920 1080
Shot 'ss8-after-new'

# Audition from the browser's Project source. The browser needs at least one Project row, so first drop a
# supported WAV onto lane.0 (the committed source_48k_stereo.wav); that lands an Asset the browser can list.
Focus
$cliBefore = [int](Probe).view.clipCount
DropFileOnLane $fixSupportedWav 0 0.1 | Out-Null
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -ge $cliBefore + 1 } -TimeoutMs 8000) ('the supported WAV lands on lane 0 (clipCount=' + (Probe).view.clipCount + ')'))

Focus
Key 'Y'
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Browser' } -TimeoutMs 2000) ('Y shows the browser dock (view.dock=' + (Probe).view.dock + ')'))
Start-Sleep -Milliseconds 400   # the dock settles before a click lands in it (README lesson)
[void](Assert ($null -ne (LayoutRect 'browser.source') -and $null -ne (LayoutRect 'browser.list') -and $null -ne (LayoutRect 'browser.audition')) 'the browser lays out source, list and audition')

SelectCombo 'browser.source' 'Recent'   # files a user imported: audition takes a FILE row (a Project row is an Asset)
[void](Assert (WaitProbe { param($q) "$($q.view.browser.source)" -eq 'Recent' } -TimeoutMs 2000) ('the browser source is Recent (' + (Probe).view.browser.source + ')'))
[void](Assert ([int](Probe).view.browser.rows -ge 1) ('Recent lists the file just imported (rows=' + (Probe).view.browser.rows + ')'))

# Select the first row; its top-left is at approximately (ListX + 8, ListY + 10). Click just inside.
$list = LayoutRect 'browser.list'
if ($null -ne $list) {
  Click 'browser.list' -OffsetX (24 - [int]($list[2] / 2)) -OffsetY (14 - [int]($list[3] / 2))
}
Start-Sleep -Milliseconds 150
Click 'browser.audition'
[void](Assert (WaitProbe { param($q) [bool]$q.view.browser.auditioning } -TimeoutMs 2500) ('audition starts (view.browser.auditioning; selected=' + (Probe).view.browser.selected + ', status=' + (Probe).status.text + ')'))
Shot 'ss8-audition-on'
Click 'browser.audition'
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.browser.auditioning } -TimeoutMs 2500) 'a second press stops audition')

# Hide the browser dock so the arrange view takes keys again before Step 2.
Focus
Key 'Y'
[void](WaitProbe { param($q) "$($q.view.dock)" -ne 'Browser' } -TimeoutMs 2000)

# -------------------------------------------------------------------------------------------
# Step 2 - Drop several formats, including a different-rate asset, at a chosen lane / time
# -------------------------------------------------------------------------------------------
Step 2 'Drop several formats and a different-rate asset at a chosen lane / time; undo / redo'
# Build enough lanes to host one format each: 1 default audio track + 4 more = 5 lanes (we drop on 1..5).
Focus
$t0 = [int](Probe).view.trackCount
for ($i = 0; $i -lt 4; $i++) { Key 'Ctrl+Shift+N' }
[void](Assert (WaitProbe { param($q) [int]$q.view.trackCount -eq $t0 + 4 } -TimeoutMs 3000) ('four Ctrl+Shift+N land four more audio tracks (trackCount=' + (Probe).view.trackCount + ')'))

$startClips = [int](Probe).view.clipCount
# Drop each format onto its own lane at a chosen fractional time, left to right across the lane.
# Fractions keep the drops distinct in both axes so a drop that leaked across lanes is visible.
$drops = @(
  @{ path = $fixAiff;      lane = 1; frac = 0.10; label = 'AIFF 48 kHz stereo' },
  @{ path = $fixFlac;      lane = 2; frac = 0.20; label = 'FLAC 48 kHz stereo' },
  @{ path = $fixOgg;       lane = 3; frac = 0.30; label = 'Ogg Vorbis 48 kHz stereo' },
  @{ path = $fixCrossRate; lane = 4; frac = 0.40; label = 'WAV 44.1 kHz mono (cross-rate)' },
  @{ path = $fixMp3;       lane = 0; frac = 0.55; label = 'MP3 48 kHz stereo' }
)
$expected = $startClips
foreach ($d in $drops) {
  $before = [int](Probe).view.clipCount
  $effect = DropFileOnLane $d.path $d.lane $d.frac
  $expected += 1
  $ok = WaitProbe { param($q) [int]$q.view.clipCount -ge $before + 1 } -TimeoutMs 10000
  [void](Assert $ok ($d.label + ' lands a clip onto lane ' + $d.lane + ' (effect=' + $effect + ', clipCount=' + (Probe).view.clipCount + ')'))
}
Shot 'ss8-after-drops'

# Save (Ctrl+S on a titled bundle: no overlay re-opens, no chooser) and inspect the bundle's audio/ dir.
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 400
[void](Assert (WaitProbe { param($q) -not [bool]$q.view.newProjectDialog } -TimeoutMs 1500) 'Ctrl+S on a titled bundle saves without opening the New overlay')
Start-Sleep -Milliseconds 1200   # ADR-0062: write to audio/.<hash>.tmp then rename to audio/<hash>.asset

# project.assets[].hash == the on-disk <hash>.asset basename (ADR-0062 / detail::hexBytes).
$pr = Probe
$probeHashes = @()
foreach ($a in $pr.project.assets) {
  $probeHashes += "$($a.hash)"
  [void](Assert ("$($a.hash)" -match '^[0-9a-f]{64}$') ('every probe asset hash is a 64-hex SHA-256 (' + $a.hash + ')'))
}
$assetFiles = Get-BundleAssetFiles $bundleA
$fileStems = @($assetFiles | ForEach-Object { $_.BaseName })
[void](Assert ($assetFiles.Count -ge $expected) ('the bundle''s audio/ dir has one .asset per imported file (observed ' + $assetFiles.Count + ', expected >=' + $expected + ')'))
foreach ($f in $assetFiles) {
  [void](Assert ($f.BaseName -match '^[0-9a-f]{64}$') ('asset filename is a 64-hex SHA-256: ' + $f.Name))
}
$diff = Compare-Object -ReferenceObject @($probeHashes | Sort-Object) -DifferenceObject @($fileStems | Sort-Object)
[void](Assert ($null -eq $diff -or @($diff).Count -eq 0) ('every probe asset hash matches a bundle audio/<hash>.asset basename (probe=' + $probeHashes.Count + ', files=' + $fileStems.Count + ')'))

# The cross-rate drop is present as a 44.1 kHz mono Asset (shape, not placement).
$crossRate = @($pr.project.assets | Where-Object { [int]$_.sampleRateHz -eq 44100 -and [int]$_.channels -eq 1 })
[void](Assert ($crossRate.Count -ge 1) ('the different-rate drop is an Asset at its own rate (44.1 kHz / 1ch rows=' + $crossRate.Count + ')'))

# Undo (one import) and redo compare state through the clip count.
$clipsAfter = [int](Probe).view.clipCount
Focus
Key 'Ctrl+Z'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -lt $clipsAfter } -TimeoutMs 2000) ('Ctrl+Z undoes the latest import (' + $clipsAfter + ' -> ' + (Probe).view.clipCount + ')'))
Key 'Ctrl+Shift+Z'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq $clipsAfter } -TimeoutMs 2000) ('Ctrl+Shift+Z redoes it (clipCount=' + (Probe).view.clipCount + ')'))
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800

# -------------------------------------------------------------------------------------------
# Step 3 - Malformed / unsupported media; honest refusal; project state unchanged
# -------------------------------------------------------------------------------------------
Step 3 'Import malformed / unsupported media; refusals are specific; project unchanged'
$clipsBefore  = [int](Probe).view.clipCount
$assetsBefore = (Get-BundleAssetFiles $bundleA).Count
$status0      = [string](Probe).status.text

# Malformed WAV through the import chooser: the filter admits .wav, the decoder refuses.
Focus
Key 'Ctrl+Shift+I'
$dlg = WaitDialog 'Import Audio' 4000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Ctrl+Shift+I opens the Import Audio chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $malformedWav }
[void](Assert (WaitProbe { param($q) "$($q.status.text)" -like 'Import refused:*' } -TimeoutMs 6000) ('the malformed WAV is refused (status=' + (Probe).status.text + ')'))
[void](Assert ([int](Probe).view.clipCount -eq $clipsBefore) ('no clip was added by the refusal (clipCount=' + (Probe).view.clipCount + ')'))
Start-Sleep -Milliseconds 600
[void](Assert ((Get-BundleAssetFiles $bundleA).Count -eq $assetsBefore) ('no .asset was written by the refusal (' + $assetsBefore + ' before, ' + (Get-BundleAssetFiles $bundleA).Count + ' after)'))

# Unsupported .m4a through a real drop: the timeline declines a file it cannot import while it is still being dragged
# (the cursor shows no-drop and the drop ends with no effect), so nothing reaches the importer (the shipped law:
# isInterestedInFileDrag admits only the import formats).
$m4aEffect = DropFileOnLane $unsupportedM4a 0 0.2
[void](Assert ($m4aEffect -eq 'None') ('the timeline declines the .m4a while it is dragged (drop effect ' + $m4aEffect + ')'))
[void](Assert ([int](Probe).view.clipCount -eq $clipsBefore) ('no clip was added by the unsupported drop (clipCount=' + (Probe).view.clipCount + ')'))
Start-Sleep -Milliseconds 600
[void](Assert ((Get-BundleAssetFiles $bundleA).Count -eq $assetsBefore) ('no .asset was written by the unsupported drop (' + (Get-BundleAssetFiles $bundleA).Count + ')'))
Shot 'ss8-refusal'

# -------------------------------------------------------------------------------------------
# Step 4 - Reopen a scratch copy with a deliberately missing asset
# -------------------------------------------------------------------------------------------
Step 4 'Reopen a scratch copy with a missing asset; Cancel, reject an incompatible file, then relink'
# Save the current bundle, close the shell, copy SongA into a scratch bundle, rename one audio/<hash>.asset
# aside so the next open asks the relink question.
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 800
Close
Start-Sleep -Milliseconds 500

if (Test-Path -LiteralPath $bundleScratch) { Remove-Item -Recurse -Force -LiteralPath $bundleScratch }
Copy-Item -Recurse -LiteralPath $bundleA -Destination $bundleScratch
$scratchAssets = Get-BundleAssetFiles $bundleScratch
[void](Assert ($scratchAssets.Count -ge 1) ('the scratch copy carries its audio/<hash>.asset files (' + $scratchAssets.Count + ')'))

$missingAsset = $scratchAssets[0]
$missingHash  = $missingAsset.BaseName
# .asset files hash their raw bytes (ProjectBundleDb::adoptAssetFile); the chooser's filter is
# yesdaw::io::importAudioFilePatterns() so the sideline keeps the raw bytes and lands under a .wav
# extension the chooser admits. The file lives OUTSIDE the bundle so the open does not re-see it.
$sidelined    = Join-Path $scratchDir ($missingHash + '.wav')
Move-Item -LiteralPath $missingAsset.FullName -Destination $sidelined
[void](Assert (-not (Test-Path -LiteralPath $missingAsset.FullName)) ('the chosen asset is sidelined (' + $missingAsset.Name + ' -> ' + $sidelined + ')'))

# (a) Open with Cancel - the shell reports "Open cancelled: ... audio files still missing" and does not attach.
Launch -Bundle $bundleScratch
$alertUp = WaitAlert 'Missing audio' 4000
if ($null -eq $alertUp) { $alertUp = WaitAlert 'Damaged audio' 2000 }
[void](Assert ($null -ne $alertUp) 'the shell asks through a Missing/Damaged audio juce::AlertWindow')
if ($null -ne $alertUp) {
  [void](Assert ([bool](Probe).relink.asking) ('probe shows a relink question is up (name=' + (Probe).relink.name + ')'))
  Shot 'ss8-missing-cancel'
  [void](AlertButton 'Cancel' -Title 'Missing audio' -TimeoutMs 2000)
  [void](Assert (WaitProbe { param($q) "$($q.relink.lastOutcome)" -eq 'cancelled' } -TimeoutMs 4000) ('relink.lastOutcome=cancelled (' + (Probe).relink.lastOutcome + ')'))
  [void](Assert (-not [bool](Probe).projectLoaded) ('Cancel leaves the project unattached (projectLoaded=' + (Probe).projectLoaded + ')'))
  [void](Assert ("$((Probe).status.text)" -like 'Open cancelled:*') ('the status line names the missing audio (' + (Probe).status.text + ')'))
}
Close
Start-Sleep -Milliseconds 400

# (b) Open again, reject an incompatible file first, then Locate the sidelined asset.
Launch -Bundle $bundleScratch
$alert2 = WaitAlert 'Missing audio' 4000
if ($null -eq $alert2) { $alert2 = WaitAlert 'Damaged audio' 2000 }
[void](Assert ($null -ne $alert2) 'the shell asks again for the still-missing asset')
if ($null -ne $alert2) {
  # First attempt: feed it an unrelated WAV; adoptAssetFile fails with IntegrityFailed and the lambda re-asks.
  [void](AlertButton 'Locate...' -Title 'Missing audio' -TimeoutMs 2000)
  $chooser = WaitDialog ('Locate ') 6000
  [void](Assert ($chooser -ne [IntPtr]::Zero) 'Locate... opens the native Locate <name> chooser')
  if ($chooser -ne [IntPtr]::Zero) { FileDialogEnter $otherWav }
  # The lambda loops: a fresh Missing audio alert comes up with relink.refusal naming "is not the missing audio".
  $alert3 = WaitAlert 'Missing audio' 4000
  if ($null -eq $alert3) { $alert3 = WaitAlert 'Damaged audio' 2000 }
  [void](Assert ($null -ne $alert3) 'the shell re-asks after an incompatible file is fed')
  [void](Assert (WaitProbe { param($q) "$($q.relink.refusal)" -like '*is not the missing audio*' } -TimeoutMs 4000) ('relink.refusal names the content mismatch (' + (Probe).relink.refusal + ')'))
  Shot 'ss8-missing-rejected'
  # Second attempt: feed it the sidelined original. The adopt succeeds; the inner for(;;) exits; the project attaches.
  [void](AlertButton 'Locate...' -Title 'Missing audio' -TimeoutMs 2000)
  $chooser2 = WaitDialog ('Locate ') 6000
  [void](Assert ($chooser2 -ne [IntPtr]::Zero) 'Locate... re-opens the native chooser after the refusal')
  if ($chooser2 -ne [IntPtr]::Zero) { FileDialogEnter $sidelined }
  [void](Assert (WaitProbe { param($q) "$($q.relink.lastOutcome)" -eq 'relinked' -and [bool]$q.projectLoaded } -TimeoutMs 10000) ('relink.lastOutcome=relinked and the project attaches (' + (Probe).relink.lastOutcome + ', projectLoaded=' + (Probe).projectLoaded + ')'))
  # The validator and render recover: the asset is back on disk and its hash is in the probe.
  $restored = @((Get-BundleAssetFiles $bundleScratch) | Where-Object { $_.BaseName -eq $missingHash })
  [void](Assert ($restored.Count -ge 1) ('the sidelined audio/<hash>.asset is back in the bundle (' + $missingHash + ')'))
  $assetInProbe = @((Probe).project.assets | Where-Object { "$($_.hash)" -eq $missingHash })
  [void](Assert ($assetInProbe.Count -ge 1) ('the restored asset is in the project (' + $missingHash + ')'))
  Focus
  Key 'Ctrl+S'
  Start-Sleep -Milliseconds 800
}
Shot 'ss8-missing-relinked'

# -------------------------------------------------------------------------------------------
# Step 5 - Export selected range and stems; cancel; project replacement during an active export
# -------------------------------------------------------------------------------------------
Step 5 'Export selected range and stems; cancel; project replacement during an export'
Close
Start-Sleep -Milliseconds 400
Launch -Bundle $bundleA
[void](Assert ([bool](Probe).projectLoaded -and [string](Probe).bundlePath -eq $bundleA) 'SongA opens')

# Make sure the settings row is up (ViewToggleSettingsRow). Both widgets below should be laid out after.
Focus
if ($null -eq (LayoutRect 'widget.shell.export.bitdepth')) {
  Click 'header.gear'   # the settings row's toggle: a painted header control (ADR-0066), not a widget
  [void](WaitProbe { param($q) $null -ne $q.layout.PSObject.Properties['widget.shell.export.bitdepth'] } -TimeoutMs 2000)
}
[void](Assert ($null -ne (LayoutRect 'widget.shell.export.bitdepth') -and $null -ne (LayoutRect 'widget.shell.export.range') -and $null -ne (LayoutRect 'widget.shell.export.stems')) 'the export settings row lays out bitdepth / range / stems')

# 24-bit PCM, Whole Project, Mix + Stems.
SelectCombo 'widget.shell.export.bitdepth' '24-bit PCM'
SelectCombo 'widget.shell.export.range' 'Whole Project'
SelectCombo 'widget.shell.export.stems' 'Mix + Stems'

$exportA = Join-Path $exportDir 'SongA_mixstems.wav'
if (Test-Path -LiteralPath $exportA) { Remove-Item -Force -LiteralPath $exportA }
Get-ChildItem -LiteralPath $exportDir -Filter 'SongA_mixstems - *.wav' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

$exportsBefore = [int](Probe).export.count
Focus
Key 'Ctrl+B'
$dlg = WaitDialog 'Export YES DAW Mix' 6000
[void](Assert ($dlg -ne [IntPtr]::Zero) 'Ctrl+B opens the native export chooser')
if ($dlg -ne [IntPtr]::Zero) { FileDialogEnter $exportA }
[void](Assert (Wait-ExportFinish $exportsBefore 120000) ('the Mix + Stems export finishes (count=' + (Probe).export.count + ', percent=' + (Probe).export.percent + ')'))
[void](Assert ("$((Probe).export.lastResult)" -eq 'succeeded') ('export.lastResult=succeeded (' + (Probe).export.lastResult + ')'))
[void](Assert ((Test-Path -LiteralPath $exportA) -and (Get-Item -LiteralPath $exportA).Length -gt 1024) ('the mix WAV lands on disk at ' + $exportA))

if (Test-Path -LiteralPath $exportA) {
  $hdr = Read-WavHeader $exportA
  [void](Assert ($hdr.riff -eq 'RIFF' -and $hdr.wave -eq 'WAVE') 'the mix is a valid RIFF/WAVE')
  [void](Assert ($hdr.bits -eq 24 -and $hdr.fmt -eq 1) ('the mix honours 24-bit PCM (fmt=' + $hdr.fmt + ', bits=' + $hdr.bits + ')'))
  [void](Assert ($hdr.sampleRate -eq 44100) ('the mix is at the project rate 44.1 kHz (' + $hdr.sampleRate + ')'))
}

$stemFiles = @(Get-ChildItem -LiteralPath $exportDir -Filter 'SongA_mixstems - *.wav' -ErrorAction SilentlyContinue)
[void](Assert ($stemFiles.Count -ge 1) ('Mix + Stems writes stem files beside the mix (' + $stemFiles.Count + ')'))
foreach ($s in $stemFiles) {
  $sh = Read-WavHeader $s.FullName
  [void](Assert ($sh.sampleRate -eq 44100 -and $sh.bits -eq 24) ('each stem is at the project rate / bit depth: ' + $s.Name + ' (' + $sh.sampleRate + ' Hz, ' + $sh.bits + '-bit)'))
}
Shot 'ss8-export-done'

# A long song for the exports that must still be running when the drive acts: zoom far out and drop a file at the
# lane's right end (a clip minutes out makes the whole-project render take long enough to cancel).
Focus
Click 'timeline'
Key 'Ctrl+Left' -Repeat 24
Start-Sleep -Milliseconds 300
$clipsBeforeLong = [int](Probe).view.clipCount
[void](DropFileOnLane $fixSupportedWav 0 0.97)
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -eq $clipsBeforeLong + 1 } -TimeoutMs 4000) ('a clip far out makes the song long (clips ' + (Probe).view.clipCount + ')'))

# Cancel a second export mid-job. ADR-0058 cp2: cancel removes <dest>.<n>.partial and never touches the final.
$exportB = Join-Path $exportDir 'SongA_cancel.wav'
if (Test-Path -LiteralPath $exportB) { Remove-Item -Force -LiteralPath $exportB }
$beforeB = [int](Probe).export.count
Focus
Key 'Ctrl+B'
$dlgB = WaitDialog 'Export YES DAW Mix' 6000
if ($dlgB -ne [IntPtr]::Zero) { FileDialogEnter $exportB }
[void](Assert (Wait-ExportStart $beforeB 6000) ('the cancellable export starts (inProgress=' + (Probe).export.inProgress + ')'))
if ($null -ne (LayoutRect 'widget.project.export_audio.cancel')) {
  Click 'widget.project.export_audio.cancel'
} else {
  Focus
  Key 'Escape'
}
[void](Assert (WaitProbe { param($q) -not [bool]$q.export.inProgress } -TimeoutMs 10000) ('the cancelled export releases inProgress (lastResult=' + (Probe).export.lastResult + ')'))
Start-Sleep -Milliseconds 600
[void](Assert ("$((Probe).export.lastResult)" -eq 'cancelled') ('export.lastResult=cancelled (' + (Probe).export.lastResult + ')'))
[void](Assert (-not (Test-Path -LiteralPath $exportB)) ('the cancelled destination is NOT written: ' + $exportB))
$partials = @(Get-ChildItem -LiteralPath $exportDir -Filter 'SongA_cancel.*.partial' -ErrorAction SilentlyContinue)
[void](Assert ($partials.Count -eq 0) ('no .partial sidecar remains after cancel (' + $partials.Count + ')'))

# A concurrent export is refused in the same shell (ADR-0058: "Export refused: an export is already running"): start
# one, then Ctrl+B again while it runs; the chooser still asks where (the refusal comes with the path), then the
# status line names the refusal and the running job is untouched.
$exportD = Join-Path $exportDir 'SongA_refused.wav'
$exportD2 = Join-Path $exportDir 'SongA_refused_second.wav'
foreach ($f in @($exportD, $exportD2)) { if (Test-Path -LiteralPath $f) { Remove-Item -Force -LiteralPath $f } }
$beforeD = [int](Probe).export.count
Focus
Key 'Ctrl+B'
$dlgD = WaitDialog 'Export YES DAW Mix' 6000
if ($dlgD -ne [IntPtr]::Zero) { FileDialogEnter $exportD }
[void](Assert (Wait-ExportStart $beforeD 6000) 'the export to refuse beside starts')
Focus
Key 'Ctrl+B'
$dlgD2 = WaitDialog 'Export YES DAW Mix' 4000
if ($dlgD2 -ne [IntPtr]::Zero) { FileDialogEnter $exportD2 }
[void](Assert (WaitProbe { param($q) "$($q.status.text)" -like 'Export refused:*already running*' } -TimeoutMs 4000) ('a second export while one runs is refused with its reason (status=' + (Probe).status.text + ')'))
[void](Assert (-not (Test-Path -LiteralPath $exportD2)) 'the refused export writes nothing')
[void](Assert (Wait-ExportFinish $beforeD 120000) ('the running export still finishes (count=' + (Probe).export.count + ')'))

# D1: project replacement during an active export (ADR-0058): start an export, then File > New, Don't Save, Create.
# The old job is retired silently - cancelled, nothing written (no file, no .partial), no count, no status - and the
# new project is current; nothing of the old job lands in the new one.
$exportC = Join-Path $exportDir 'SongA_replace.wav'
if (Test-Path -LiteralPath $exportC) { Remove-Item -Force -LiteralPath $exportC }
$bundleReplacement = Join-Path $scratchDir 'Replacement.yesdaw'
$beforeC = [int](Probe).export.count
$outcomesC = [int](Probe).export.outcomes
Focus
Key 'Ctrl+B'
$dlgC = WaitDialog 'Export YES DAW Mix' 6000
if ($dlgC -ne [IntPtr]::Zero) { FileDialogEnter $exportC }
[void](Assert (Wait-ExportStart $beforeC 6000) 'the export to replace under starts')
Focus
Click 'widget.project.new'
if ($null -ne (WaitAlert 'Unsaved changes' 3000)) {
  Shot 'ss8-unsaved-during-export'
  [void](AlertButton "Don't Save" -Title 'Unsaved changes' -TimeoutMs 2000)
}
[void](Assert (WaitProbe { param($q) [bool]$q.view.newProjectDialog } -TimeoutMs 4000) 'the New Project dialog follows')
if ($null -ne (LayoutRect 'newproject.create')) { Click 'newproject.create' }
$dlgNew = WaitDialog 'Create YES DAW Project' 6000
if ($dlgNew -ne [IntPtr]::Zero) { FileDialogEnter $bundleReplacement }
[void](Assert (WaitProbe { param($q) [string]$q.bundlePath -eq $bundleReplacement } -TimeoutMs 8000) ('the replacement project is current (' + (Probe).bundlePath + ')'))
[void](Assert (WaitProbe { param($q) -not [bool]$q.export.inProgress -and [int]$q.export.retiring -eq 0 } -TimeoutMs 30000) ('the old export is retired and joined (retiring=' + (Probe).export.retiring + ')'))
[void](Assert ([int](Probe).export.count -eq $beforeC -and [int](Probe).export.outcomes -eq $outcomesC) ('the retired job reports nothing (count ' + (Probe).export.count + ', outcomes ' + (Probe).export.outcomes + ')'))
[void](Assert ("$((Probe).status.text)" -notlike 'Export*') ('no export news on the new project''s status line (' + (Probe).status.text + ')'))
[void](Assert (-not (Test-Path -LiteralPath $exportC)) ('the replaced export wrote no file: ' + $exportC))
[void](Assert (@(Get-ChildItem -LiteralPath $exportDir -Filter '*.partial' -ErrorAction SilentlyContinue).Count -eq 0) 'nor a .partial temporary')

# -------------------------------------------------------------------------------------------
# Step 6 - Autosave + recovery (ADR-0068 known-red until the fix lands)
# -------------------------------------------------------------------------------------------
Step 6 'Save, edit, wait for autosave, interrupt the scratch session, recover (ADR-0068: three laps)'
# ADR-0068: every edit is already in the bundle, so the autosave is the flushed fallback for LOST writes, and recovery
# offers only a snapshot newer than the bundle. Lap 1, the interruption: after a kill the bundle is the recovery - no
# question, and the project equals the last confirmed autosave. Lap 2, lost writes (a power cut, simulated by putting the
# saved project.db back): the question rises and Restore brings back the last confirmed autosave. Lap 3, a clean session:
# Save and close - no question, the autosave retired. Red until ADR-0068 lands (the shipped autosave never wrote).
Close
Start-Sleep -Milliseconds 400
if (Test-Path -LiteralPath $bundleAuto) { Remove-Item -Recurse -Force -LiteralPath $bundleAuto }

function AutosaveCounts($q) { return @([int]$q.project.trackCount, [int]$q.project.clipCount, [int]$q.project.midiClipCount, @($q.project.assets).Count) }
function WrittenCounts($w) { if ($null -eq $w) { return @(-1, -1, -1, -1) }; return @([int]$w.tracks, [int]$w.clips, [int]$w.midiClips, [int]$w.assets) }
function WaitAutosave([string] $what) {
  $before = [int](Probe).autosave.writes
  $ok = WaitProbe { param($q) [int]$q.autosave.writes -gt $before } -TimeoutMs 8000
  [void](Assert $ok ('a confirmed autosave follows ' + $what + ' within 8 s at the drive cadence (autosave.writes=' + (Probe).autosave.writes + '; ADR-0068)'))
  return $ok
}

Launch -Bundle $bundleA -AutosaveIntervalMs 1000
[void](Assert ([bool](Probe).projectLoaded) 'SongA opens')
[void](Assert ([int](Probe).autosave.intervalMs -eq 1000) ('the shorter cadence is honoured (autosave.intervalMs=' + (Probe).autosave.intervalMs + ')'))
Focus
Key 'Ctrl+Shift+S'
$dlgSave = WaitDialog 'Save YES DAW Project As' 4000
[void](Assert ($dlgSave -ne [IntPtr]::Zero) 'Ctrl+Shift+S opens Save As')
if ($dlgSave -ne [IntPtr]::Zero) { FileDialogEnter $bundleAuto }
[void](Assert (WaitProbe { param($q) [string]$q.bundlePath -eq $bundleAuto } -TimeoutMs 6000) ('Save As re-points the project to ' + $bundleAuto))
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 600
# The saved state, set aside: every write checkpoints into project.db, so the file alone is the saved bundle.
$savedDb = Join-Path $scratchDir 'ss8-saved-project.db'
Copy-Item -LiteralPath (Join-Path $bundleAuto 'project.db') -Destination $savedDb -Force

# Lap 1 - the interruption.
$cleanClips = [int](Probe).view.clipCount
Focus
$lane0 = LayoutRect 'lane.0'
if ($null -ne $lane0) { Click 'lane.0' -OffsetX ([int](-([int]$lane0[2] / 2) + 50)) }
Start-Sleep -Milliseconds 150
Key 'Ctrl+A'
Start-Sleep -Milliseconds 150
Key 'Delete'
[void](Assert (WaitProbe { param($q) [int]$q.view.clipCount -ne $cleanClips } -TimeoutMs 2000) ('an edit changed the clip count (' + $cleanClips + ' -> ' + (Probe).view.clipCount + ')'))
[void](WaitAutosave 'the edit')
$written1 = WrittenCounts (Probe).autosave.lastWritten
KillApp
Launch -Bundle $bundleAuto -ReuseSessionDir $script:SessionDir -AutosaveIntervalMs 1000
[void](Assert (WaitProbe { param($q) [bool]$q.projectLoaded } -TimeoutMs 6000) 'the bundle reopens after the kill')
Start-Sleep -Milliseconds 500
[void](Assert (-not [bool](Probe).autosave.recovery.pending) 'after a kill there is no recovery question: the bundle holds every edit (ADR-0068)')
$now = AutosaveCounts (Probe)
[void](Assert (($now -join ',') -eq ($written1 -join ',')) ('the reopened project equals the last confirmed autosave (' + ($now -join ',') + ' vs ' + ($written1 -join ',') + ')'))

# Lap 2 - lost writes.
Focus
Key 'Ctrl+Shift+N'   # one more edit: a track
[void](Assert (WaitProbe { param($q) [int]$q.project.trackCount -eq [int]$now[0] + 1 } -TimeoutMs 2000) 'a second edit adds a track')
[void](WaitAutosave 'the second edit')
$written2 = WrittenCounts (Probe).autosave.lastWritten
KillApp
Copy-Item -LiteralPath $savedDb -Destination (Join-Path $bundleAuto 'project.db') -Force   # the writes after the Save, lost
foreach ($side in @('project.db-wal', 'project.db-shm')) {
  $sidePath = Join-Path $bundleAuto $side
  if (Test-Path -LiteralPath $sidePath) { Remove-Item -LiteralPath $sidePath -Force }
}
Launch -Bundle $bundleAuto -ReuseSessionDir $script:SessionDir -AutosaveIntervalMs 1000
[void](Assert (WaitProbe { param($q) [bool]$q.autosave.recovery.pending } -TimeoutMs 6000) 'after lost writes the recovery question rises (ADR-0068)')
$rec = (Probe).autosave.recovery
[void](Assert ((@([int]$rec.tracks, [int]$rec.clips, [int]$rec.midiClips, [int]$rec.assets) -join ',') -eq ($written2 -join ',')) ('the question names what the autosave holds (' + $rec.tracks + ',' + $rec.clips + ',' + $rec.midiClips + ',' + $rec.assets + ' vs ' + ($written2 -join ',') + ')'))
if ($null -ne (Probe).autosave.PSObject.Properties['bundleWriteStamp']) {
  [void](Assert ([int64](Probe).autosave.bundleWriteStamp -lt [int64](Probe).autosave.snapshotWriteStamp) ('the bundle is behind the snapshot (' + (Probe).autosave.bundleWriteStamp + ' < ' + (Probe).autosave.snapshotWriteStamp + ')'))
}
Shot 'ss8-recovery-prompt'
if ($null -ne (LayoutRect 'widget.autosave.recovery.restore')) {
  Click 'widget.autosave.recovery.restore'
  [void](Assert (WaitProbe { param($q) -not [bool]$q.autosave.recovery.pending } -TimeoutMs 4000) 'Restore answers the question')
  $restored = AutosaveCounts (Probe)
  [void](Assert (($restored -join ',') -eq ($written2 -join ',')) ('Restore brings back the last confirmed autosave (' + ($restored -join ',') + ' vs ' + ($written2 -join ',') + ')'))
} else {
  [void](Assert $false 'the recovery question offers its Restore button (widget.autosave.recovery.restore)')
}

# Lap 3 - a clean session.
Focus
Key 'Ctrl+S'
Start-Sleep -Milliseconds 600
Close
Start-Sleep -Milliseconds 400
Launch -Bundle $bundleAuto -ReuseSessionDir $script:SessionDir
Start-Sleep -Milliseconds 500
[void](Assert (-not [bool](Probe).autosave.recovery.pending) 'after Save and close there is no recovery question')
[void](Assert (-not (Test-Path -LiteralPath (Join-Path $bundleAuto 'autosave\last.yesdaw'))) 'the Save retired the autosave')

# -------------------------------------------------------------------------------------------
# Step 7 - Save As and Save a Copy; preferences and per-project view state retain
# -------------------------------------------------------------------------------------------
Step 7 'Save As and Save a Copy; preferences retain their separate values across a relaunch'
# Flip two preferences that persist to prefs.json: dockTab (via Y chord -> Browser) and the mixer
# narrow-strips flag (via a strip menu). Each lands in the probe before the Save As.
Focus
if ("$((Probe).view.dock)" -ne 'Browser') { Key 'Y' }
[void](Assert (WaitProbe { param($q) "$($q.view.dock)" -eq 'Browser' } -TimeoutMs 2000) ('the dock preference is Browser (' + (Probe).view.dock + ')'))

if (-not [bool](Probe).view.mixerNarrow) {
  # Open the mixer to reach a strip menu; from there pick Narrow Strips by name.
  if ("$((Probe).view.dock)" -ne 'Mixer') { Key 'X' }
  [void](WaitProbe { param($q) "$($q.view.dock)" -eq 'Mixer' } -TimeoutMs 2000)
  $r = LayoutRect 'mixer.strip.0'
  if ($null -ne $r) {
    Click 'mixer.strip.0' -Right -OffsetY (14 - [int]($r[3] / 2))
    [void](WaitPopup)
    try { [void](MenuPickByName 'Narrow Strips') } catch { Key 'Esc' }
  }
  # Return the dock to Browser for the persistence check.
  if ("$((Probe).view.dock)" -ne 'Browser') { Key 'Y' }
  [void](WaitProbe { param($q) "$($q.view.dock)" -eq 'Browser' } -TimeoutMs 2000)
}
$narrowBefore = [bool](Probe).view.mixerNarrow
[void](Assert $narrowBefore 'the mixer narrow-strips preference is set')

# Save As to a fresh bundle. The copy's audio/ dir carries every asset.
Focus
Key 'Ctrl+Shift+S'
$dlgAs = WaitDialog 'Save YES DAW Project As' 4000
[void](Assert ($dlgAs -ne [IntPtr]::Zero) 'Ctrl+Shift+S opens Save As')
if (Test-Path -LiteralPath $bundleSaveAs) { Remove-Item -Recurse -Force -LiteralPath $bundleSaveAs }
if ($dlgAs -ne [IntPtr]::Zero) { FileDialogEnter $bundleSaveAs }
[void](Assert (WaitProbe { param($q) [string]$q.bundlePath -eq $bundleSaveAs } -TimeoutMs 6000) ('Save As re-points the project to ' + $bundleSaveAs))
$saveAsAssets = (Get-BundleAssetFiles $bundleSaveAs).Count
[void](Assert ($saveAsAssets -ge 1) ('the Save As bundle carries its audio assets (' + $saveAsAssets + ')'))

# Save a Copy has no shortcut: pick it by name from the File menu through UI Automation.
Focus
$menubar = LayoutRect 'widget.shell.menubar'
if ($null -ne $menubar) { Click 'widget.shell.menubar' -OffsetX (20 - [int]($menubar[2] / 2)) }   # 'File' (no Alt mnemonics in JUCE's bar)
$pickedCopy = $false
try { $pickedCopy = MenuPickByName 'Save a Copy' } catch { Key 'Esc' }
[void](Assert $pickedCopy "File > Save a Copy picked by name")
if ($pickedCopy) {
  $dlgCopy = WaitDialog 'Save a Copy of the YES DAW Project' 4000
  [void](Assert ($dlgCopy -ne [IntPtr]::Zero) 'Save a Copy opens its own native chooser')
  if (Test-Path -LiteralPath $bundleCopy) { Remove-Item -Recurse -Force -LiteralPath $bundleCopy }
  if ($dlgCopy -ne [IntPtr]::Zero) { FileDialogEnter $bundleCopy }
  Start-Sleep -Milliseconds 1000
  [void](Assert ([string](Probe).bundlePath -eq $bundleSaveAs) ('Save a Copy does NOT re-point the project (bundlePath=' + (Probe).bundlePath + ')'))
  [void](Assert (Test-Path -LiteralPath $bundleCopy) ('the copy exists on disk at ' + $bundleCopy))
  $copyAssets = (Get-BundleAssetFiles $bundleCopy).Count
  [void](Assert ($copyAssets -ge $saveAsAssets) ('the copy carries at least the same assets (' + $copyAssets + ' vs ' + $saveAsAssets + ')'))
}

# Relaunch under the SAME session dir so prefs.json is reread. Both preferences must still be in effect.
$keepDir = $script:SessionDir
Close
Start-Sleep -Milliseconds 500
Launch -Bundle $bundleSaveAs -ReuseSessionDir $keepDir
[void](Assert ("$((Probe).prefs.state)" -eq 'loaded') ('prefs.json was loaded (prefs.state=' + (Probe).prefs.state + ')'))
[void](Assert ([int](Probe).prefs.rejectedKeys -eq 0) ('no prefs rows were rejected (' + (Probe).prefs.rejectedKeys + ')'))
[void](Assert ("$((Probe).view.dock)" -eq 'Browser') ('the dockTab preference persists (view.dock=' + (Probe).view.dock + ')'))
[void](Assert ([bool](Probe).view.mixerNarrow -eq $narrowBefore) ('the narrowStrips preference persists (view.mixerNarrow=' + (Probe).view.mixerNarrow + ')'))

# Reopen the copy and assert it opens as its own bundle.
if (Test-Path -LiteralPath $bundleCopy) {
  Close
  Start-Sleep -Milliseconds 500
  Launch -Bundle $bundleCopy -ReuseSessionDir $keepDir
  [void](Assert ([bool](Probe).projectLoaded -and [string](Probe).bundlePath -eq $bundleCopy) ('the copy opens as its own project (' + (Probe).bundlePath + ')'))
  [void](Assert ((@((Probe).project.assets).Count) -ge 1) ('the copy''s project carries its assets (' + @((Probe).project.assets).Count + ')'))
}
Shot 'ss8-step7-final'
Close
