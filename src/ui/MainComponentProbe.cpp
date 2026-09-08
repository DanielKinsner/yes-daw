// YES DAW — the app shell: the state probe and the harness accessors.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

const yesdaw::ui::UiRecordingDeviceSelection& MainComponent::harnessRecordingDevice() const noexcept
{
    return appModel.recordingDeviceSelection();
}

float MainComponent::harnessInputMeterPeak() const noexcept
{
    return appModel.inputMeterPeak();
}

const yesdaw::ui::UiRecordingTrackInputSelection& MainComponent::harnessRecordingTrackInput() const noexcept
{
    return appModel.recordingTrackInputSelection();
}

// M11: the whole arm set, so gates can pin that several rows are armed at once.
const std::vector<yesdaw::ui::UiRecordingTrackInputSelection>&
    MainComponent::harnessArmedRecordingTrackInputs() const noexcept
{
    return appModel.armedRecordingTrackInputs();
}

const yesdaw::ui::UiRecordedAudioTake& MainComponent::harnessLastRecordedAudioTake() const noexcept
{
    return appModel.lastRecordedAudioTake();
}

const yesdaw::ui::UiRecordedMidiTake& MainComponent::harnessLastRecordedMidiTake() const noexcept
{
    return appModel.lastRecordedMidiTake();
}

const yesdaw::ui::UiRecordingCompSelection& MainComponent::harnessRecordingComp() const noexcept
{
    return appModel.recordingCompSelection();
}

const yesdaw::ui::UiAutosaveRecoveryPrompt& MainComponent::harnessAutosaveRecovery() const noexcept
{
    return appModel.autosaveRecoveryPrompt();
}

bool MainComponent::harnessPrimaryFileChoicesReady() const noexcept
{
    return static_cast<bool> (fileChoices.chooseNewProjectBundle)
        && static_cast<bool> (fileChoices.chooseOpenProjectBundle)
        && static_cast<bool> (fileChoices.chooseImportAudioFile)
        && static_cast<bool> (fileChoices.chooseExportAudioFile);
}

long long MainComponent::harnessPianoRollViewScrollTicks() const noexcept
{
    return static_cast<long long> (pianoRollViewScrollTicks);
}

int MainComponent::harnessTimelineMaxTrackScrollRows() const
{
    // The scroll clamp depends only on the lane count and the surface heights.
    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = appModel.context().projectLoaded
        ? static_cast<int> (appModel.project().tracks.size())
        : 0;
    return std::max (
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state).maxTrackScrollRows,
        trackListInput.maxScrollRows());
}

int MainComponent::harnessVisibleTimelineTrackCount() const
{
    return appModel.context().projectLoaded ? static_cast<int> (projectTimelineTracks.size()) : 0;
}

int MainComponent::harnessVisibleTimelineClipCount() const
{
    return appModel.context().projectLoaded ? static_cast<int> (timelineClips.size()) : 0;
}

std::string MainComponent::harnessVisibleFirstTimelineClipName() const
{
    if (! appModel.context().projectLoaded || timelineClips.empty() || timelineClips.front().name == nullptr)
        return {};
    return timelineClips.front().name;
}

int MainComponent::harnessSelectedTimelineClipCount() const
{
    return static_cast<int> (appModel.selectedTimelineClipCount());
}

double MainComponent::harnessVisibleTimelineTotalSeconds() const noexcept
{
    return timelineTotalSeconds;
}

int MainComponent::harnessVisibleMixerTrackCount() const
{
    return static_cast<int> (currentMixerSurface().tracks.size());
}

int MainComponent::harnessVisibleMixerBusCount() const
{
    return static_cast<int> (currentMixerSurface().buses.size());
}

// E23: which strip the painted mixer highlights (tracks first, then buses; -1 = none).
int MainComponent::harnessSelectedMixerStripOrdinal() const
{
    return appModel.selectedMixerStripOrdinal();
}

bool MainComponent::harnessVisibleMixerLoudnessValid() const
{
    return currentMixerSurface().loudness.valid;
}

int MainComponent::harnessVisiblePianoRollNoteCount() const
{
    return static_cast<int> (currentPianoRollSurface().notes.size());
}

float MainComponent::harnessVisibleMasterPeakLeft() const noexcept
{
    return liveMasterPeakLeft.load (std::memory_order_acquire);
}

float MainComponent::harnessVisibleMasterPeakRight() const noexcept
{
    return liveMasterPeakRight.load (std::memory_order_acquire);
}

bool MainComponent::harnessDesktopAudioOpen() const noexcept
{
    return desktopAudioOpen.load (std::memory_order_acquire);
}

std::uint64_t MainComponent::harnessDeviceAudioCallbackBlockCount() const noexcept
{
    return deviceAudioCallbackBlockCount.load (std::memory_order_relaxed);
}

std::uint64_t MainComponent::harnessDeviceAudioNonSilentBlockCount() const noexcept
{
    return deviceAudioNonSilentBlockCount.load (std::memory_order_relaxed);
}

bool MainComponent::harnessProcessDeviceAudioBlock (float* const* outputChannels,
                                                   int numOutputChannels,
                                                   int numFrames) noexcept
{
    return processDeviceAudioBlock (outputChannels, numOutputChannels, numFrames);
}

// E30: input-carrying harness block — drives the same input-aware model path the native
// device callback uses, so input metering is CI-deterministic.
bool MainComponent::harnessProcessDeviceAudioBlock (const float* const* inputChannels,
                                                   int numInputChannels,
                                                   float* const* outputChannels,
                                                   int numOutputChannels,
                                                   int numFrames) noexcept
{
    const bool processed = appModel.processDeviceAudioBlock (
        inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);
    accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);
    return processed;
}

// N1: the painted Mute/Solo cell rect for a strip, in SHELL coordinates (cell 0 = Solo,
// 1 = Mute) — the same law the paint, the click hit-test and the live buttons read.
juce::Rectangle<int> MainComponent::harnessPaintedMuteSoloCellBounds (int stripIndex, int cellIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal || cellIndex < 0)
        return {};

    return paintedMuteSoloCellBoundsForLane (
        paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
        static_cast<std::size_t> (cellIndex),
        stripCellCount (static_cast<std::size_t> (stripIndex)));
}

// G4.1: the painted I/O row rect for a strip (shell coordinates) — the same law the paint and the
// click read; empty where the strip has no such slot or is too short to carry it.
juce::Rectangle<int> MainComponent::harnessPaintedIoRowBounds (int stripIndex, int row) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal)
        return {};
    const auto lane = paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex));
    const int ioRows = stripIoRows (static_cast<std::size_t> (stripIndex));
    if (row == kMixerIoInputRow)
        return paintedInputRowBoundsForLane (lane, ioRows);
    if (row == kMixerIoOutputRow)
        return paintedOutputRowBoundsForLane (lane, ioRows);
    return {};
}

// G4.1: the two slots' texts as painted.
yesdaw::ui::MainComponentMixerStripIo MainComponent::harnessMixerStripIo (int stripIndex) const
{
    yesdaw::ui::MainComponentMixerStripIo io;
    const auto surface = currentMixerSurface();
    const std::size_t trackCount = surface.tracks.size();
    const int stripTotal = static_cast<int> (trackCount + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal)
        return io;
    const auto index = static_cast<std::size_t> (stripIndex);
    if (index < trackCount)
        io.input = stripInputText (index);
    io.output = stripOutputText (index < trackCount ? surface.tracks[index] : surface.buses[index - trackCount]);
    return io;
}

// M4: the painted insert-slot rect for a strip, in SHELL coordinates — the same law the paint
// and the click hit-test read.
juce::Rectangle<int> MainComponent::harnessPaintedInsertSlotBounds (int stripIndex, int slotIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal
        || slotIndex < 0 || slotIndex >= yesdaw::ui::UiTheme::Layout::mixerPaintedInsertRowCount)
        return {};

    return paintedInsertRowBoundsForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                          static_cast<std::size_t> (slotIndex),
                                          stripIoRows (static_cast<std::size_t> (stripIndex)));
}

// M5: the painted send-row rect for a strip, in SHELL coordinates.
juce::Rectangle<int> MainComponent::harnessPaintedSendRowBounds (int stripIndex, int sendIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal || sendIndex < 0)
        return {};

    return paintedSendRowBoundsForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                        static_cast<std::size_t> (sendIndex),
                                        stripIoRows (static_cast<std::size_t> (stripIndex)));
}

// M6: the painted fader rail and the y the thumb sits at for a given gain — the same law the
// paint uses, so a gate can prove unity is NOT at the top of the rail.
juce::Rectangle<int> MainComponent::harnessPaintedFaderRailBounds (int stripIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal)
        return {};

    return paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                    stripIoRows (static_cast<std::size_t> (stripIndex)));
}

juce::Rectangle<int> MainComponent::harnessPaintedPanKnobBounds (int stripIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal)
        return {};
    return paintedPanKnobForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)));
}

int MainComponent::harnessPaintedFaderThumbY (int stripIndex, float linearGain) const
{
    const auto rail = harnessPaintedFaderRailBounds (stripIndex);
    return rail.isEmpty() ? 0 : mixerFaderThumbYForGain (rail, linearGain);
}

juce::var MainComponent::probeRect (juce::Rectangle<int> rect)
{
    juce::Array<juce::var> values;
    values.add (rect.getX());
    values.add (rect.getY());
    values.add (rect.getWidth());
    values.add (rect.getHeight());
    return values;
}

const char* MainComponent::probeFocusContextName (yesdaw::ui::UiPanel panel) noexcept
{
    switch (panel)
    {
        case yesdaw::ui::UiPanel::Timeline:  return "Arrange";
        case yesdaw::ui::UiPanel::Mixer:     return "Mixer";
        case yesdaw::ui::UiPanel::PianoRoll: return "PianoRoll";
    }
    return "Arrange";
}

const char* MainComponent::probeToolName (yesdaw::ui::TimelineTool tool) noexcept
{
    switch (tool)
    {
        case yesdaw::ui::TimelineTool::Pointer:  return "Pointer";
        case yesdaw::ui::TimelineTool::Pencil:   return "Pencil";
        case yesdaw::ui::TimelineTool::Scissors: return "Scissors";
        case yesdaw::ui::TimelineTool::Hand:     return "Hand";
        case yesdaw::ui::TimelineTool::Zoom:     return "Zoom";
        case yesdaw::ui::TimelineTool::Eraser:   return "Eraser";     // G3.2
        case yesdaw::ui::TimelineTool::Velocity: return "Velocity";
    }
    return "Pointer";
}

juce::String MainComponent::probeRendererName() const
{
    if (const juce::ComponentPeer* peer = getPeer())
    {
        // getAvailableRenderingEngines() is non-const in JUCE's peer API; the query itself
        // mutates nothing.
        auto& mutablePeer = const_cast<juce::ComponentPeer&> (*peer);
        const juce::StringArray engines = mutablePeer.getAvailableRenderingEngines();
        const int index = peer->getCurrentRenderingEngine();
        if (index >= 0 && index < engines.size())
            return engines[index];
        return "unknown";
    }
    return "none";
}

double MainComponent::probePaintP95Ms() const
{
    if (paintRingCount == 0)
        return 0.0;
    std::array<double, kStateProbePaintRingSize> sorted = paintRing;
    std::sort (sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t> (paintRingCount));
    const std::size_t rank = std::min (paintRingCount - 1u, (paintRingCount * 95u) / 100u);
    return sorted[rank];
}

juce::var MainComponent::buildProbeLayout()
{
    auto* layout = new juce::DynamicObject();
    juce::var layoutVar (layout);
    const auto put = [layout] (const juce::String& key, juce::Rectangle<int> rect) {
        if (! rect.isEmpty())
            layout->setProperty (key, probeRect (rect));
    };

    put ("header", getLocalBounds().withHeight (headerHeightNow()));
    put ("header.gear", headerLayout().gear);   // the settings-row toggle, clickable since 2026-09-04
    put ("rail", leftRailPanelBounds());
    put ("timeline", timelineBounds());
    put ("inspector", inspectorBounds());
    if (inspectorShowsQuantizePanel())   // G3.4: the panel's controls by name
    {
        put ("inspector.midi.mute", inspectorMidiMute.getBounds());   // G3.5
        put ("inspector.midi.transpose", inspectorMidiTranspose.getBounds());
        put ("inspector.midi.velocity", inspectorMidiVelocity.getBounds());
        put ("inspector.midi.loop", inspectorMidiLoop.getBounds());
        put ("inspector.quantize.grid", inspectorQuantizeGrid.getBounds());
        put ("inspector.quantize.strength", inspectorQuantizeStrength.getBounds());
        put ("inspector.quantize.swing", inspectorQuantizeSwing.getBounds());
        put ("inspector.quantize.ends", inspectorQuantizeEnds.getBounds());
        put ("inspector.quantize.humanize", inspectorQuantizeHumanize.getBounds());
        put ("inspector.quantize.apply", inspectorQuantizeApply.getBounds());
    }
    put ("header.midi.in", headerLayout().midiIn);   // G3.10: the input lamp
    if (appModel.context().mixerDockVisible)
    {
        put ("dock", mixerPanelBounds());
        if (dockShowsPianoRoll())
        {
            // G3.3: the control lane's data area and its chooser, so a drive draws where the lane is.
            const PianoRollCanvasGeometry rollGeometry = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin());
            put ("pianoroll.lane", pianoRollControlLaneDataArea (rollGeometry).translated (pianoRollInput.getX(), pianoRollInput.getY()));
            put ("pianoroll.lane.chooser", pianoRollLaneChooser.getBounds());
            put ("pianoroll.key", pianoRollKeyChooser.getBounds());     // G3.8
            put ("pianoroll.scale", pianoRollScaleChooser.getBounds());
            put ("pianoroll.typing", pianoRollTypingButton.getBounds());   // G3.6
            put ("pianoroll.step", pianoRollStepButton.getBounds());
        }
        // G3.9: the Sampler's pad grid, when the Instrument tab shows one (panel-local → shell).
        if (instrumentPanel.isVisible() && instrumentPanel.padsShown())
            put ("instrument.panel.pads", instrumentPanel.currentPadGrid().translated (instrumentPanel.getX(), instrumentPanel.getY()));
        // The mixer's painted zones by name (mixer.strip.N, .solo, .mute, .fader,
        // .insert.K, .send.K) so a drive clicks the strip it sees — nothing about the mixer
        // was clickable by name before 2026-09-04, which is where the mouse-only bugs hid.
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        for (int strip = 0; strip < stripTotal; ++strip)
        {
            const juce::String base = "mixer.strip." + juce::String (strip);
            put (base, harnessPaintedMixerStripBounds (strip));
            put (base + ".solo", harnessPaintedMuteSoloCellBounds (strip, 0));
            put (base + ".mute", harnessPaintedMuteSoloCellBounds (strip, 1));
            put (base + ".arm", harnessPaintedMuteSoloCellBounds (strip, 2));   // G4.1: Track strips only
            put (base + ".input", harnessPaintedIoRowBounds (strip, kMixerIoInputRow));
            put (base + ".output", harnessPaintedIoRowBounds (strip, kMixerIoOutputRow));
            put (base + ".fader", harnessPaintedFaderRailBounds (strip));
            put (base + ".pan", harnessPaintedPanKnobBounds (strip));
            for (int slot = 0; slot < yesdaw::ui::UiTheme::Layout::mixerPaintedInsertRowCount; ++slot)
                put (base + ".insert." + juce::String (slot), harnessPaintedInsertSlotBounds (strip, slot));
            for (int send = 0; send < yesdaw::ui::UiTheme::Layout::mixerPaintedSendRowCount; ++send)
                put (base + ".send." + juce::String (send), harnessPaintedSendRowBounds (strip, send));
        }
    }

    // G4.1 cp2: the FX editor and its two buttons (grandchildren: the walk below sees children only).
    if (fxEditor.isVisible())
    {
        put ("mixer.fx.editor", fxEditor.getBounds());
        for (int i = 0; i < fxEditor.getNumChildComponents(); ++i)
            if (const juce::Component* child = fxEditor.getChildComponent (i))
                if (child->isVisible() && child->getComponentID().startsWith ("mixer.fx."))
                    put (child->getComponentID(), child->getBounds().translated (fxEditor.getX(), fxEditor.getY()));
    }
    // Every visible identified child by its component id — toolbar buttons carry their
    // action's stable id (configureActionComponent), choosers their shell ids.
    for (int i = 0; i < getNumChildComponents(); ++i)
        if (const juce::Component* child = getChildComponent (i))
            if (child->isVisible() && child->getComponentID().isNotEmpty())
                put ("widget." + child->getComponentID(), child->getBounds());

    if (appModel.context().projectLoaded)
    {
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        const juce::Point<int> origin = timelineInput.getPosition();
        put ("ruler", geometry.rulerArea.translated (origin.x, origin.y));
        put ("clipArea", geometry.clipArea.translated (origin.x, origin.y));
        for (int mapIndex = 0; mapIndex < state.mapLabelCount; ++mapIndex)   // the tempo / meter change labels
            put ("ruler.map." + juce::String (mapIndex),
                 yesdaw::ui::timelineMapLabelRect (timelineInput.getLocalBounds(), state, mapIndex).translated (origin.x, origin.y));
        // The tool strip's cells by name (tool.pointer … tool.hand): drives click by NAME.
        for (std::size_t index = 0; index < yesdaw::ui::kTimelineToolStripOrder.size(); ++index)
            put ("tool." + juce::String (probeToolName (yesdaw::ui::kTimelineToolStripOrder[index])).toLowerCase(),
                 yesdaw::ui::timelineToolStripCell (geometry.toolbarArea, index).translated (origin.x, origin.y));

        for (int lane = 0; lane < state.trackCount; ++lane)
        {
            const int top = geometry.clipArea.getY()
                + juce::roundToInt (geometry.laneTop (lane) - geometry.viewport.laneScrollPixels);
            const int height = lane < static_cast<int> (geometry.laneHeightPixelsPerLane.size())
                ? juce::roundToInt (geometry.laneHeightPixelsPerLane[static_cast<std::size_t> (lane)])
                : geometry.laneHeight;
            const juce::Rectangle<int> row (geometry.clipArea.getX(), top,
                                            geometry.clipArea.getWidth(), height);
            put ("lane." + juce::String (lane),
                 row.getIntersection (geometry.clipArea).translated (origin.x, origin.y));
            put ("rail.row." + juce::String (lane), harnessPaintedRailRowBounds (lane));
            // The row's painted M / S / O cells by name, so a drive clicks the badge, not a pixel.
            put ("rail.row." + juce::String (lane) + ".mute", harnessPaintedRailCellBounds (lane, 0));
            put ("rail.row." + juce::String (lane) + ".solo", harnessPaintedRailCellBounds (lane, 1));
            put ("rail.row." + juce::String (lane) + ".arm",  harnessPaintedRailCellBounds (lane, 2));
        }

        std::array<yesdaw::ui::ElementRect, yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> visible {};
        const yesdaw::ui::Viewport clipViewport = yesdaw::ui::viewportForClipLayout (geometry);
        const int visibleCount = state.clipCount > 0
            ? yesdaw::ui::layoutVisible (state.clips, state.clipCount, clipViewport,
                                         visible.data(), static_cast<int> (visible.size()))
            : 0;
        for (int i = 0; i < visibleCount; ++i)
        {
            const auto& rect = visible[static_cast<std::size_t> (i)];
            if (rect.id < 0 || rect.id >= static_cast<int> (timelineClipIds.size()))
                continue;
            const juce::Rectangle<int> clipRect =
                juce::Rectangle<int> (geometry.clipArea.getX() + juce::roundToInt (rect.x),
                                      geometry.clipArea.getY() + juce::roundToInt (rect.y),
                                      juce::roundToInt (rect.w),
                                      juce::roundToInt (rect.h))
                    .getIntersection (geometry.clipArea);
            put ("clip." + juce::String (entityIdHex (timelineClipIds[static_cast<std::size_t> (rect.id)])),
                 clipRect.translated (origin.x, origin.y));
        }
    }

    return layoutVar;
}

// G0.4: what identified widgets SAY (combo / button / label text by component id) — the
// drive asserts on words, never on pixels, and a stale control is visible in the document.
juce::var MainComponent::buildProbeText() const
{
    auto* text = new juce::DynamicObject();
    juce::var textVar (text);
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        const juce::Component* child = getChildComponent (i);
        if (child == nullptr || ! child->isVisible() || child->getComponentID().isEmpty())
            continue;
        juce::String value;
        if (const auto* combo = dynamic_cast<const juce::ComboBox*> (child))
            value = combo->getText();
        else if (const auto* button = dynamic_cast<const juce::Button*> (child))
            value = button->getButtonText();
        else if (const auto* label = dynamic_cast<const juce::Label*> (child))
            value = label->getText();
        else
            continue;
        text->setProperty (child->getComponentID(), value);
    }
    return textVar;
}

juce::String MainComponent::buildStateProbeJson()
{
    const yesdaw::ui::UiActionContext context = appModel.contextSnapshot();
    auto* root = new juce::DynamicObject();
    juce::var rootVar (root);

    root->setProperty ("version", kStateProbeSchemaVersion);
    root->setProperty ("tick", static_cast<juce::int64> (probeTick));
    root->setProperty ("uptimeMs", std::chrono::duration<double, std::milli> (
                                       std::chrono::steady_clock::now() - launchStamp).count());
    root->setProperty ("renderer", probeRendererName());
    root->setProperty ("windowTitle", computedWindowTitle());
    root->setProperty ("projectLoaded", context.projectLoaded);
    root->setProperty ("bundlePath", juceFileFromPath (harnessBundlePath()).getFullPathName());
    root->setProperty ("window", probeRect (getScreenBounds()));
    {
        double displayScale = 1.0;
        if (const juce::Displays::Display* display =
                juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            displayScale = display->scale;
        root->setProperty ("displayScale", displayScale);
    }

    {
        auto* transport = new juce::DynamicObject();
        transport->setProperty ("isPlaying", context.isPlaying);
        transport->setProperty ("isRecording", context.isRecording);
        transport->setProperty ("playheadFrame", static_cast<juce::int64> (context.playheadFrame));
        transport->setProperty ("playheadSeconds",
                                context.projectLoaded && appModel.project().sampleRate.isValid()
                                    ? static_cast<double> (context.playheadFrame)
                                          / appModel.project().sampleRate.hz
                                    : 0.0);
        transport->setProperty ("rate", context.shuttlePlaybackRate);
        transport->setProperty ("midiInSeen", static_cast<int> (appModel.midiInputQueue().seen()));   // G3.10
        transport->setProperty ("midiInDrained", static_cast<int> (appModel.midiInputDrained()));
        transport->setProperty ("midiInLit", midiInLitUntil != std::chrono::steady_clock::time_point {}
                                                 && std::chrono::steady_clock::now() < midiInLitUntil);
        transport->setProperty ("midiThruTarget", static_cast<juce::int64> (appModel.midiInputQueue().thruTarget()));
        transport->setProperty ("metronome", context.metronomeEnabled);
        auto* loop = new juce::DynamicObject();
        loop->setProperty ("enabled", context.loopEnabled);
        loop->setProperty ("start", static_cast<juce::int64> (harnessPlaybackLoopStartFrame()));
        loop->setProperty ("end", static_cast<juce::int64> (harnessPlaybackLoopEndFrame()));
        transport->setProperty ("loop", juce::var (loop));
        root->setProperty ("transport", juce::var (transport));
    }

    {
        auto* selection = new juce::DynamicObject();
        juce::Array<juce::var> clips;
        if (context.projectLoaded)
            for (const yesdaw::engine::Clip& clip : appModel.project().clips)
                if (appModel.isTimelineClipSelected (clip.id))
                    clips.add (juce::String (entityIdHex (clip.id)));
        selection->setProperty ("clips", clips);
        juce::Array<juce::var> notes;
        for (const yesdaw::engine::EntityId& noteId : appModel.selectedMidiNoteIds())
            notes.add (juce::String (entityIdHex (noteId)));
        selection->setProperty ("notes", notes);
        juce::Array<juce::var> tracks;
        if (selectedTrackLanes.empty())
        {
            if (selectedTrackLane >= 0)
                tracks.add (selectedTrackLane);
        }
        else
            for (const int lane : selectedTrackLanes)   // G2.17: every selected lane, ascending
                tracks.add (lane);
        selection->setProperty ("tracks", tracks);
        selection->setProperty ("primaryTrack", selectedTrackLane);
        juce::Array<juce::var> trackKinds;
        if (appModel.context().projectLoaded)
            for (std::size_t t = 0; t < appModel.project().tracks.size(); ++t)
                trackKinds.add (trackHoldsMidi (t) ? "midi" : "audio");
        selection->setProperty ("trackKinds", trackKinds);
        selection->setProperty ("midiClip",
                                appModel.selectedMidiClipId().isValid()
                                    ? juce::var (juce::String (entityIdHex (appModel.selectedMidiClipId())))
                                    : juce::var());
        if (context.timelineRangeSelected)
        {
            auto* range = new juce::DynamicObject();
            range->setProperty ("startFrame", static_cast<juce::int64> (harnessTimelineRangeStartFrame()));
            range->setProperty ("endFrame", static_cast<juce::int64> (harnessTimelineRangeEndFrame()));
            selection->setProperty ("timeRange", juce::var (range));
        }
        else
        {
            selection->setProperty ("timeRange", juce::var());
        }
        selection->setProperty ("mixerStrip", harnessSelectedMixerStripOrdinal());
        root->setProperty ("selection", juce::var (selection));
    }

    root->setProperty ("focusContext", probeFocusContextName (context.activePanel));
    {
        const juce::Component* focused = juce::Component::getCurrentlyFocusedComponent();
        juce::String owner = "none";
        if (focused == this)
            owner = "shell";
        else if (focused != nullptr)
            owner = focused->getComponentID().isNotEmpty() ? focused->getComponentID()
                                                            : focused->getName().isNotEmpty()
                                                                  ? focused->getName()
                                                                  : juce::String ("unnamed");
        root->setProperty ("focusOwner", owner);
        root->setProperty ("textEditorActive",
                           dynamic_cast<const juce::TextEditor*> (focused) != nullptr
                               || trackRenameEditor.isVisible() || clipRenameEditor.isVisible()
                               || markerRenameEditor.isVisible() || busRenameEditor.isVisible());
    }
    root->setProperty ("lastAction", juce::String (lastActionStableId));
    root->setProperty ("commandDispatchCount", context.commandDispatchCount);
    {
        auto* status = new juce::DynamicObject();
        status->setProperty ("text", juce::String (appModel.statusLineText()));
        status->setProperty ("isError", appModel.statusLineIsError());
        root->setProperty ("status", juce::var (status));
    }

    {
        auto* view = new juce::DynamicObject();
        view->setProperty ("width", getWidth());
        view->setProperty ("height", getHeight());
        view->setProperty ("zoom", timelineZoomFactor);
        view->setProperty ("scrollSec", timelineScrollSeconds);
        view->setProperty ("trackScrollRows", timelineTrackScrollRows);
        view->setProperty ("activePanel", probeFocusContextName (context.activePanel));
        view->setProperty ("inspector", ! inspectorBounds().isEmpty());
        view->setProperty ("dock", ! context.mixerDockVisible ? juce::String ("None")
                                   : context.editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll
                                       ? juce::String ("PianoRoll")
                                   : context.editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument
                                       ? juce::String ("Instrument")   // G3.1
                                       : juce::String ("Mixer"));
        if (const yesdaw::engine::Track* const instrumentTrack = appModel.selectedTrackForInstrument())
        {
            view->setProperty ("instrument", juce::String (instrumentKindName (instrumentTrack->instrumentKind)));   // G3.1
            view->setProperty ("samplerPadCount", static_cast<int> (instrumentTrack->samplerPads.size()));   // G3.9
        }
        view->setProperty ("dockHeight", dockedMixerHeight());
        view->setProperty ("mixerNarrow", context.mixerStripsNarrow);   // G4.1
        view->setProperty ("settingsRow", context.settingsRowVisible);
        view->setProperty ("headerHeight", headerHeightNow());
        view->setProperty ("nudgeValue", context.nudgeValue);
        view->setProperty ("nudgeFrames", static_cast<juce::int64> (appModel.nudgeFrames()));   // G2.8
        view->setProperty ("editMode", context.editMode == yesdaw::ui::UiEditMode::Overlap ? "overlap"
                                       : context.editMode == yesdaw::ui::UiEditMode::NoOverlap ? "no-overlap" : "shuffle");   // G2.6
        view->setProperty ("snapMode", context.snapMode == yesdaw::ui::UiSnapMode::Grid ? "grid"
                                       : context.snapMode == yesdaw::ui::UiSnapMode::Relative ? "relative"
                                       : context.snapMode == yesdaw::ui::UiSnapMode::Events ? "events" : "off");   // G2.7
        view->setProperty ("snapEffectiveTicks", static_cast<juce::int64> (effectiveSnapGridTicks()));
        view->setProperty ("keymapEditor", context.keymapVisible);
        view->setProperty ("undoHistory", context.undoHistoryVisible);   // G2.18
        view->setProperty ("hoverHint", hoverHint);
        view->setProperty ("railWidth", viewState.railWidth);          // G2.1
        view->setProperty ("inspectorWidth", inspectorWidthNow());
        view->setProperty ("dockHeight", dockedMixerHeight());
        {
            const CounterStrings counter = counterStrings();
            view->setProperty ("timeDisplay", counter.mode);
            view->setProperty ("rulerTimeFormat", juce::String (yesdaw::ui::timeline_canvas_detail::rulerTimeFormatName (timeDisplayMode)));   // G2.2
            view->setProperty ("counterPrimary", counter.primary);
            view->setProperty ("counterSecondary", counter.secondary);
        }
        view->setProperty ("tool", probeToolName (context.activeTimelineTool));
        view->setProperty ("lastAuditionKey", pianoRollInput.lastAuditionKey());   // G3.2
        {
            int noteCount = -1;   // -1 = no MIDI clip selected
            if (context.projectLoaded)
                for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
                    if (clip.id == appModel.selectedMidiClipId())
                        noteCount = static_cast<int> (clip.notes.size());
            view->setProperty ("noteCount", noteCount);   // G3.2: the roll's clip's note count
        }
        {
            // G3.2 checkpoint: the roll's window and its painted notes (shell-local geometry), so a
            // session drive aims at what is painted instead of guessing.
            auto* roll = new juce::DynamicObject();
            const yesdaw::ui::UiPianoRollSurfaceSnapshot rollSurface = currentPianoRollSurface();
            const PianoRollCanvasGeometry rollGeometry = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin());
            roll->setProperty ("viewLowKey", rollSurface.viewLowKey);
            roll->setProperty ("scaleRoot", rollSurface.scaleRoot);       // G3.8
            roll->setProperty ("drumMode", rollSurface.drumMode);         // G3.9: the clip's Track is a Sampler
            roll->setProperty ("scaleChoice", rollSurface.scaleChoice);
            roll->setProperty ("viewHighKey", pianoRollViewHighKey (rollSurface));
            roll->setProperty ("rowHeight", static_cast<double> (rollGeometry.rowHeight));
            roll->setProperty ("viewScrollTicks", static_cast<juce::int64> (rollSurface.viewScrollTicks));
            roll->setProperty ("visibleTicks", static_cast<juce::int64> (pianoRollVisibleTicks (rollSurface)));
            roll->setProperty ("gridX", rollGeometry.grid.getX());
            roll->setProperty ("gridY", rollGeometry.grid.getY());
            roll->setProperty ("gridWidth", rollGeometry.grid.getWidth());
            roll->setProperty ("gridHeight", rollGeometry.grid.getHeight());
            roll->setProperty ("keyboardX", rollGeometry.keyboard.getX());
            roll->setProperty ("keyboardWidth", rollGeometry.keyboard.getWidth());
            {
                // G3.3: the control lane — its name, its data area (roll-local) and its point count.
                const juce::Rectangle<int> laneData = pianoRollControlLaneDataArea (rollGeometry);
                roll->setProperty ("controlLane", juce::String (yesdaw::ui::pianoRollControlLaneChoice (rollSurface.controlLaneChoice).name));
                roll->setProperty ("controlLaneX", laneData.getX());
                roll->setProperty ("controlLaneY", laneData.getY());
                roll->setProperty ("controlLaneWidth", laneData.getWidth());
                roll->setProperty ("controlLaneHeight", laneData.getHeight());
                const auto* lane = pianoRollControlLaneOf (rollSurface);
                roll->setProperty ("controlPointCount", lane != nullptr ? static_cast<int> (lane->points.size()) : 0);
                roll->setProperty ("laneGesture", pianoRollInput.lastLaneGesture());
            }
            juce::Array<juce::var> rollNotes;
            for (const yesdaw::ui::UiPianoRollNoteView& note : rollSurface.notes)
            {
                auto* item = new juce::DynamicObject();
                item->setProperty ("id", juce::String (entityIdHex (note.noteId)));
                item->setProperty ("start", static_cast<juce::int64> (note.startTick));
                item->setProperty ("length", static_cast<juce::int64> (note.lengthTicks));
                item->setProperty ("key", static_cast<int> (note.key));
                rollNotes.add (juce::var (item));
            }
            roll->setProperty ("notes", rollNotes);
            view->setProperty ("pianoRoll", juce::var (roll));
        }
        view->setProperty ("snapEnabled", context.snapEnabled);
        view->setProperty ("snapGridTicks", static_cast<juce::int64> (context.snapGridTicks));
        {
            // G3.4: the quantize settings Q applies, and whether the panel is up.
            auto* quantize = new juce::DynamicObject();
            const yesdaw::engine::QuantizeSettings settings = appModel.currentQuantizeSettings();
            quantize->setProperty ("gridChoice", context.quantizeGridChoice);
            quantize->setProperty ("gridTicks", static_cast<juce::int64> (settings.grid.intervalTicks));
            quantize->setProperty ("strength", context.quantizeStrengthPercent);
            quantize->setProperty ("swing", context.quantizeSwingPercent);
            quantize->setProperty ("noteEnds", context.quantizeNoteEnds);
            quantize->setProperty ("humanize", context.quantizeHumanizePercent);
            quantize->setProperty ("seed", static_cast<juce::int64> (settings.humanizeSeed));
            quantize->setProperty ("panel", inspectorShowsQuantizePanel());
            view->setProperty ("quantize", juce::var (quantize));
        }
        {
            // G3.6: the keyboard modes.
            auto* typing = new juce::DynamicObject();
            typing->setProperty ("on", context.musicalTypingOn);
            typing->setProperty ("baseKey", context.typingBaseKey);
            typing->setProperty ("velocity", context.typingVelocityPercent);
            typing->setProperty ("lastKey", appModel.lastTypedKey());
            typing->setProperty ("heldCount", appModel.typedHeldCount());
            view->setProperty ("musicalTyping", juce::var (typing));
            auto* step = new juce::DynamicObject();
            step->setProperty ("on", context.stepInputOn);
            step->setProperty ("stepTicks", static_cast<juce::int64> (appModel.stepInputTicks()));
            view->setProperty ("stepInput", juce::var (step));
        }
        if (const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip())
        {
            // G3.5: the selected MIDI clip's settings as the inspector shows them.
            auto* clipView = new juce::DynamicObject();
            clipView->setProperty ("id", juce::String (entityIdHex (midiClip->id)));
            clipView->setProperty ("muted", midiClip->muted);
            clipView->setProperty ("transpose", static_cast<int> (midiClip->transposeSemitones));
            clipView->setProperty ("velocityOffset", midiClip->velocityOffset);
            clipView->setProperty ("loopLength", static_cast<juce::int64> (midiClip->loopLengthTicks));
            clipView->setProperty ("loopChoice", appModel.midiClipLoopChoiceFor (*midiClip));
            view->setProperty ("midiClip", juce::var (clipView));
        }
        view->setProperty ("playheadFollow", context.playheadFollowEnabled);
        view->setProperty ("playheadFollowContinuous", context.playheadFollowContinuous);   // G2.16
        view->setProperty ("rowZoom", timelineRowZoom);
        view->setProperty ("zoomHistoryDepth", static_cast<int> (zoomHistory.size()));
        view->setProperty ("trackCount", context.projectLoaded
                                             ? static_cast<int> (appModel.project().tracks.size())
                                             : 0);
        view->setProperty ("clipCount", context.projectLoaded
                                            ? static_cast<int> (appModel.project().clips.size())
                                            : 0);
        root->setProperty ("view", juce::var (view));
    }

    {
        auto* frame = new juce::DynamicObject();
        frame->setProperty ("paintMs", lastPaintMs);
        frame->setProperty ("paintP95Ms", probePaintP95Ms());
        frame->setProperty ("paintCount", static_cast<juce::int64> (paintCount));
        frame->setProperty ("tickMs", lastTickMs);
        frame->setProperty ("actionToPaintMs", lastActionToPaintMs);
        // G0.4: how the shell invalidates — full (model/view change) vs dynamic (tick) — and
        // how often the action-state refresh really runs.
        frame->setProperty ("fullInvalidations", static_cast<juce::int64> (fullInvalidations));
        frame->setProperty ("dynamicInvalidations", static_cast<juce::int64> (dynamicInvalidations));
        frame->setProperty ("actionStateRefreshes", static_cast<juce::int64> (actionStateRefreshes));
        root->setProperty ("frame", juce::var (frame));
    }

    {
        auto* audio = new juce::DynamicObject();
        audio->setProperty ("callbackAdds", static_cast<juce::int64> (audioCallbackAdds));
        audio->setProperty ("callbackRemovals", static_cast<juce::int64> (audioCallbackRemovals));
        audio->setProperty ("callbackRegistered", desktopAudioCallbackRegistered);
        // G0.3: suspend REQUESTS (registered or not) — the [no-callback-teardown] gate's
        // number in the headless harness, where no device callback ever exists.
        audio->setProperty ("suspendRequests", static_cast<juce::int64> (audioSuspendRequests));
        audio->setProperty ("retiredObjects", static_cast<juce::int64> (appModel.retiredAudioObjectCount()));
        audio->setProperty ("deviceBlocks", static_cast<juce::int64> (appModel.deviceBlocksStarted()));
        audio->setProperty ("deviceOpen", desktopAudioOpen.load (std::memory_order_acquire));
        audio->setProperty ("rebuilds", static_cast<juce::int64> (appModel.playbackReplaceCount()));
        audio->setProperty ("liveScalars", static_cast<juce::int64> (appModel.playbackLiveScalarsApplied()));
        audio->setProperty ("blocks", static_cast<juce::int64> (
                                          deviceAudioCallbackBlockCount.load (std::memory_order_acquire)));
        audio->setProperty ("deadlineMisses", static_cast<juce::int64> (
                                                  deviceDeadlineMisses.load (std::memory_order_relaxed)));
        audio->setProperty ("maxCallbackMs",
                            static_cast<double> (deviceMaxCallbackNs.load (std::memory_order_relaxed)) / 1.0e6);
        audio->setProperty ("sampleRateHz", deviceSampleRateHz.load (std::memory_order_relaxed));
        // Driver-reported xruns since the device started; -1 when the driver cannot count.
        int underruns = -1;
        if (const juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
        {
            const int baseline = deviceXRunBaseline.load (std::memory_order_relaxed);
            const int current = device->getXRunCount();
            if (baseline >= 0 && current >= 0)
                underruns = current - baseline;
        }
        audio->setProperty ("underruns", underruns);
        root->setProperty ("audio", juce::var (audio));
    }

    root->setProperty ("layout", buildProbeLayout());
    root->setProperty ("text", buildProbeText());
    {
        auto* recording = new juce::DynamicObject();
        const auto& device = appModel.recordingDeviceSelection();
        recording->setProperty ("deviceSelected", device.selected);
        recording->setProperty ("inputChannels", static_cast<int> (device.inputChannels));
        recording->setProperty ("deviceGeneration", static_cast<juce::int64> (device.generation));
        recording->setProperty ("selectedInputChannel", context.selectedRecordingInputChannel);
        recording->setProperty ("chooserGeneration", static_cast<juce::int64> (recordingChannelChooserGeneration));
        recording->setProperty ("armedTrackCount", static_cast<int> (appModel.armedRecordingTrackInputs().size()));   // G4.1
        root->setProperty ("recording", juce::var (recording));
    }
    {
        // G4.1: the mixer's strips as painted — name, the two I/O slots' texts, the arm — so a
        // drive asserts what it sees on the strip after a slot pick.
        auto* mixer = new juce::DynamicObject();
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        mixer->setProperty ("trackCount", static_cast<int> (trackCount));
        mixer->setProperty ("busCount", static_cast<int> (surface.buses.size()));
        mixer->setProperty ("narrow", context.mixerStripsNarrow);
        juce::Array<juce::var> strips;
        for (std::size_t i = 0; i < trackCount + surface.buses.size(); ++i)
        {
            const yesdaw::ui::UiMixerStrip& state = i < trackCount ? surface.tracks[i] : surface.buses[i - trackCount];
            auto* strip = new juce::DynamicObject();
            strip->setProperty ("name", juce::String (state.name));
            strip->setProperty ("kind", i < trackCount ? "Track" : "Bus");
            strip->setProperty ("input", i < trackCount ? stripInputText (i) : juce::String());
            strip->setProperty ("output", stripOutputText (state));
            strip->setProperty ("armed", i < trackCount && appModel.isRecordingTrackIndexArmed (i));
            strip->setProperty ("muted", state.muted);     // G4.1 cp2: the S / M cells as painted
            strip->setProperty ("soloed", state.soloed);
            // G4.1 cp2: the painted inserts and sends, as the strip reads them.
            juce::Array<juce::var> inserts;
            for (const yesdaw::ui::UiMixerFxSlotReadout& insert : state.fxSlots)
            {
                auto* row = new juce::DynamicObject();
                row->setProperty ("kind", fxKindName (insert.kind));
                row->setProperty ("enabled", insert.enabled);
                inserts.add (juce::var (row));
            }
            strip->setProperty ("inserts", inserts);
            juce::Array<juce::var> sends;
            for (const yesdaw::ui::UiMixerSendReadout& send : state.sends)
            {
                auto* row = new juce::DynamicObject();
                row->setProperty ("bus", juce::String (send.busName));
                row->setProperty ("level", static_cast<double> (send.linearGain));
                row->setProperty ("pre", send.preFader);
                sends.add (juce::var (row));
            }
            strip->setProperty ("sends", sends);
            strips.add (juce::var (strip));
        }
        mixer->setProperty ("strips", strips);
        root->setProperty ("mixer", juce::var (mixer));
    }
    {
        // G4.1 cp2: the FX editor — what a drive sees after a slot's double-click.
        const yesdaw::ui::MainComponentFxEditor editor = harnessFxEditor();
        auto* fx = new juce::DynamicObject();
        fx->setProperty ("visible", editor.visible);
        fx->setProperty ("strip", editor.strip);
        fx->setProperty ("slot", editor.slot);
        fx->setProperty ("kind", editor.kind);
        fx->setProperty ("bypassed", editor.bypassed);
        fx->setProperty ("rows", editor.rows);
        fx->setProperty ("eqResponseVisible", editor.visible && fxEditor.showsEqResponse());
        fx->setProperty ("eqResponseDb1000", fxEditor.eqResponseDb (1000.0));
        root->setProperty ("fxEditor", juce::var (fx));
        // G4.1 cp2: the Touch / Latch ride a painted drag is buffering (N5) — what a drive sees mid-ride.
        auto* ride = new juce::DynamicObject();
        ride->setProperty ("active", automationTouchRideActive);
        ride->setProperty ("samples", static_cast<int> (automationTouchRideSamples.size()));
        root->setProperty ("ride", juce::var (ride));
    }
    return juce::JSON::toString (rootVar, true);
}

void MainComponent::writeStateProbeIfEnabled()
{
    if (stateProbePath.empty())
        return;
    // Write-then-replace so a reader never sees a torn document.
    (void) juceFileFromPath (stateProbePath).replaceWithText (buildStateProbeJson());
}

bool MainComponent::harnessPostMidiInput (bool on, int key, double velocity) noexcept   // G3.10: the device callback's path
{
    return postMidiInputFromDevice (on, key, velocity, 0);
}

void MainComponent::harnessSelectPianoRollScale (int rootKey, int scaleChoice)   // G3.8: through the real choosers
{
    pianoRollKeyChooser.setSelectedId (rootKey + 1, juce::sendNotificationSync);
    pianoRollScaleChooser.setSelectedId (scaleChoice + 1, juce::sendNotificationSync);
}

std::vector<float> MainComponent::harnessRenderPlayback (std::uint64_t frames, int blockSize)   // G3.2: the engine's own blocks
{
    return appModel.renderPlaybackFrames (frames, blockSize);
}

juce::Rectangle<int> MainComponent::harnessPaintedRailRowBounds (int row) const
{
    return trackListInput.rowBounds (row)
        .translated (trackListInput.getX(), trackListInput.getY());
}

// N7: the painted colour-swatch rect for a rail row (the left accent bar), in shell
// coordinates — the same law the click-to-cycle gesture hit-tests against.
juce::Rectangle<int> MainComponent::harnessPaintedColourSwatchBounds (int row) const
{
    return trackListInput.colourSwatchBounds (row)
        .translated (trackListInput.getX(), trackListInput.getY());
}

// The source window the canvas was handed for a painted clip (the waveform painter's law).
yesdaw::ui::TimelineClipSourceWindow MainComponent::harnessTimelineClipSourceWindow (int layoutClipId) const
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClips.size()))
        return {};
    const yesdaw::ui::Clip& clip = timelineClips[static_cast<std::size_t> (layoutClipId)];
    return { clip.sourceStartFrame, clip.sourceFrameCount };
}

// The rail's three painted cells (M / S / O) in shell coordinates — the rects the rail's
// hit-test claims, so a test or a drive clicks the badge it sees.
juce::Rectangle<int> MainComponent::harnessPaintedRailCellBounds (int row, int cell) const
{
    const juce::Rectangle<int> local = cell == 0 ? trackListInput.muteCellBounds (row)
                                     : cell == 1 ? trackListInput.soloCellBounds (row)
                                                 : trackListInput.armCellBounds (row);
    return local.translated (trackListInput.getX(), trackListInput.getY());
}

// V4: the ruler's painted bar labels — the SAME state build, geometry, and label law the
// paint path runs (makeTimelineState → timelineCanvasGeometry → computeRulerBarLabels), so a
// gate can never re-derive the formula.
std::vector<yesdaw::ui::RulerBarLabel> MainComponent::harnessRulerBarLabels()
{
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
    return yesdaw::ui::computeRulerBarLabels (geometry.clipArea, state, geometry.viewport);
}

// V5: the rail's live L/R meter peaks for one row — the SAME hold-state values the paint
// passes to drawMeterWithHold, so a gate can prove the two channels really diverge.
std::pair<float, float> MainComponent::harnessRailMeterChannelPeaks (int row) const
{
    if (row < 0 || row >= static_cast<int> (trackMeterHoldLR.size()))
        return { 0.0f, 0.0f };

    const auto& lr = trackMeterHoldLR[static_cast<std::size_t> (row)];
    return { lr[0].livePeak, lr[1].livePeak };
}

// V5: the rail VOL fader's rect in SHELL coordinates — the SAME law paint and the click/drag
// hit-test share, so a gate can prove the control is genuinely vertical.
juce::Rectangle<int> MainComponent::harnessRailVolumeSliderBounds (int row) const
{
    return trackListInput.volumeSliderBounds (row)
        .translated (trackListInput.getX(), trackListInput.getY());
}

// V7: the fade chart's inner rect in SHELL coordinates — the SAME law the paint uses.
juce::Rectangle<int> MainComponent::harnessInspectorFadeChartBounds() const
{
    return inspectorFadeChartBounds();
}

// V4: the inverse pixel→seconds mapping of the SAME viewport the ruler paints with, so a
// gate can cross-check a label's x against the tempo map without duplicating the paint math.
double MainComponent::harnessRulerSecondsAtX (int x)
{
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
    return static_cast<double> (x - geometry.clipArea.getX()) / geometry.viewport.pixelsPerSecond
         + geometry.viewport.scrollSeconds;
}

// N7: the ACTUAL colour the timeline canvas will paint for one clip (by id) — reads the same
// cached timelineClipStyles/timelineClipIds arrays paintTimelineCanvas() paints from,
// refreshing them first so this can never report a stale value from before the caller's last
// edit.
juce::Colour MainComponent::harnessTimelineClipColour (yesdaw::engine::EntityId clipId)
{
    rebuildTimelineClipViews();
    for (std::size_t i = 0; i < timelineClipIds.size(); ++i)
        if (timelineClipIds[i] == clipId)
            return timelineClipStyles[i].colour;
    return {};
}

// N3: the painted mixer-strip lane rect for a track/bus strip, and the master pane's rect —
// exposed so a gate can prove they share ONE law (master is always the next contiguous slot
// after the last strip, never a detached island computed independently of it).
juce::Rectangle<int> MainComponent::harnessPaintedMixerStripBounds (int stripIndex) const
{
    const auto surface = currentMixerSurface();
    const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
    if (stripIndex < 0 || stripIndex >= stripTotal)
        return {};

    return paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex));
}

juce::Rectangle<int> MainComponent::harnessPaintedMixerMasterBounds() const
{
    return paintedMixerMasterBounds();
}

// V3: the dock's OWN reserved rect — height collapses to (near) zero when the toggle hides
// it, the same law every layout function (timelineBounds/leftRailPanelBounds/inspectorBounds
// /this) shares via dockedMixerHeight().
juce::Rectangle<int> MainComponent::harnessMixerPanelBounds() const
{
    return mixerPanelBounds();
}

juce::Rectangle<int> MainComponent::harnessTimelineBounds() const
{
    return timelineBounds();
}

std::vector<float> MainComponent::harnessRenderPlaybackFrames (std::uint64_t frames, int blockSize)
{
    return appModel.renderPlaybackFrames (frames, blockSize);
}

MainComponent::MainComponentSnapshotLike MainComponent::snapshotForZoom() const
{
    MainComponentSnapshotLike out;
    const yesdaw::engine::Project& project = appModel.project();
    const std::int64_t start = appModel.timelineRangeStartFrame();
    const std::int64_t end = appModel.timelineRangeEndFrame();
    if (! project.sampleRate.isValid() || start < 0 || end <= start)
        return out;
    out.rangeStartSeconds = static_cast<double> (start) / project.sampleRate.hz;
    out.rangeSeconds = static_cast<double> (end - start) / project.sampleRate.hz;
    const juce::Rectangle<int> timeline = timelineBounds();
    out.widthPixels = static_cast<double> (juce::jmax (yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                                     timeline.getWidth() - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter));
    out.fitPixelsPerSecond = out.widthPixels / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds, timelineTotalSeconds);
    return out;
}

// G3.6: the harness's key-up (the headless run has no real keyboard for isKeyCurrentlyDown).
void MainComponent::harnessReleaseTypedKeys()
{
    for (const auto& [code, note] : typedKeyCodes)
        appModel.musicalTypingRelease (note);
    typedKeyCodes.clear();
}

// G2.4: the Smart tool's zone and cursor under a shell point.
juce::String MainComponent::harnessTimelineZoneAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const
{
    return timelineInput.zoneNameAt (shellPoint - timelineInput.getPosition(), modifiers);
}

juce::String MainComponent::harnessTimelineCursorAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const
{
    const juce::MouseCursor zoneCursor = timelineInput.cursorAt (shellPoint - timelineInput.getPosition(), modifiers);
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::LeftRightResizeCursor)) return "left-right";
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor)) return "top-left";
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::TopRightCornerResizeCursor)) return "top-right";
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::IBeamCursor)) return "ibeam";
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::DraggingHandCursor)) return "dragging-hand";   // G2.11
    if (zoneCursor == juce::MouseCursor (juce::MouseCursor::UpDownResizeCursor)) return "up-down";
    return "normal";
}

yesdaw::ui::MainComponentFxEditor MainComponent::harnessFxEditor() const
{
    yesdaw::ui::MainComponentFxEditor out;
    out.visible = fxEditor.isVisible();
    out.strip = fxEditorOpen ? fxEditorStripOrdinal : -1;
    out.slot = fxEditorOpen ? selectedFxParamSlot : -1;
    const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
    if (fxEditorOpen && selectedFxParamSlot >= 0 && static_cast<std::size_t> (selectedFxParamSlot) < chain.size())
        out.kind = fxKindName (chain[static_cast<std::size_t> (selectedFxParamSlot)].kind);
    out.bypassed = fxEditor.isBypassed();
    out.page = selectedFxParamPage;
    out.pageCount = mixerFxParamPageChooser.getNumItems();
    out.rows = static_cast<int> (lastVisibleFxParamRows);
    out.bounds = fxEditor.getBounds();
    return out;
}

void MainComponent::harnessInvokeContextMenuItem (yesdaw::ui::UiActionId action, int direction)
{
    if (action == yesdaw::ui::UiActionId::MixerFxInsertAdd)
        invokeContextMenuItem (kContextMenuAddInsertBase + direction);   // direction carries the FxKind
    else if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::InsertSlot
             && action == yesdaw::ui::UiActionId::MixerFxInsertReorder)
        invokeContextMenuItem (direction < 0 ? kContextMenuMoveUpId : kContextMenuMoveDownId);
    else
        invokeContextMenuItem (static_cast<int> (action) + 1);
}

// Harness: route a shell point to the input surface under it and run its right-click law.
yesdaw::ui::MainComponentContextMenu MainComponent::harnessRequestContextMenu (juce::Point<int> shellPoint)
{
    lastContextMenu = {};
    juce::String route = "none";
    if (timelineInput.isVisible() && timelineInput.getBounds().contains (shellPoint))
    {
        route = "timeline";
        timelineInput.requestContextMenu (shellPoint - timelineInput.getPosition());
    }
    else if (pianoRollInput.isVisible() && pianoRollInput.getBounds().contains (shellPoint))
    {
        route = "pianoRoll";
        pianoRollInput.requestContextMenu (shellPoint - pianoRollInput.getPosition());
    }
    else if (trackListInput.isVisible() && trackListInput.getBounds().contains (shellPoint))
    {
        route = "rail";
        trackListInput.requestContextMenu (shellPoint - trackListInput.getPosition());
    }
    else if (mixerStripsInput.isVisible() && mixerStripsInput.getBounds().contains (shellPoint))
    {
        route = "strips";
        mixerStripsInput.requestContextMenu (shellPoint - mixerStripsInput.getPosition());
    }
    return lastContextMenu.toPublic (route);
}

juce::String MainComponent::harnessHoverHintAt (juce::Point<int> shellPoint)
{
    juce::String hint;
    if (timelineInput.isVisible() && timelineInput.getBounds().contains (shellPoint))
        hint = timelineInput.hintAt (shellPoint - timelineInput.getPosition(), {});
    else if (pianoRollInput.isVisible() && pianoRollInput.getBounds().contains (shellPoint))
        hint = pianoRollInput.hintAt (shellPoint - pianoRollInput.getPosition(), {});
    else if (trackListInput.isVisible() && trackListInput.getBounds().contains (shellPoint))
        hint = trackListInput.hintAt (shellPoint - trackListInput.getPosition(), {});
    else if (mixerStripsInput.isVisible() && mixerStripsInput.getBounds().contains (shellPoint))
        hint = mixerStripsInput.hintAt (shellPoint - mixerStripsInput.getPosition(), {});
    setHoverHint (hint);
    return hint;
}

// Harness: show/hide the settings row through the real action (the Options menu's toggle).
void MainComponent::harnessSetSettingsRowVisible (bool visible)
{
    if (appModel.context().settingsRowVisible != visible)
        handleAction (yesdaw::ui::UiActionId::ViewToggleSettingsRow);
}

void MainComponent::harnessDispatchAction (yesdaw::ui::UiActionId action)
{
    // Exactly what a toolbar button's click does.
    handleAction (action);
    refreshActionState();
    resized();
    repaintAll();
}

yesdaw::ui::UiActionState MainComponent::harnessActionState (yesdaw::ui::UiActionId action) const
{
    return appModel.registry().stateFor (action, appModel.context());
}

} // namespace yesdaw::ui

// The harness's free helpers (global scope, as they were).

juce::Component* findChildWithComponentId (juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (juce::Component* child = component.getChildComponent (i))
            if (juce::Component* found = findChildWithComponentId (*child, componentId))
                return found;

    return nullptr;
}

const juce::Component* findChildWithComponentId (const juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (const juce::Component* child = component.getChildComponent (i))
            if (const juce::Component* found = findChildWithComponentId (*child, componentId))
                return found;

    return nullptr;
}

juce::String stableIdForAction (yesdaw::ui::UiActionId action)
{
    const yesdaw::ui::UiActionRegistry registry;
    if (const yesdaw::ui::UiActionDescriptor* descriptor = registry.descriptor (action))
        return descriptor->stableId;

    return {};
}

std::filesystem::path pathFromJuceFile (const juce::File& file)
{
    const std::string utf8 = file.getFullPathName().toStdString();
    const auto* begin = reinterpret_cast<const char8_t*> (utf8.data());
    return std::filesystem::path (std::u8string (begin, begin + utf8.size()));
}

std::filesystem::path withExtension (std::filesystem::path path, const std::filesystem::path& extension)
{
    if (path.extension() != extension)
        path += extension;

    return path;
}

yesdaw::ui::MainComponentFileChoices makeNativeFileChoices()
{
    yesdaw::ui::MainComponentFileChoices choices;

    choices.chooseNewProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Create YES DAW Project",
                                   documents.getChildFile ("Untitled.yesdaw"),
                                   "*.yesdaw",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".yesdaw");
    };

    choices.chooseOpenProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Open YES DAW Project Folder", documents, {}, true);
        if (! chooser.browseForDirectory())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseSaveAsProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Save YES DAW Project As",
                                   documents.getChildFile ("Untitled.yesdaw"),
                                   "*.yesdaw",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".yesdaw");
    };

    choices.chooseImportAudioFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Import WAV Audio", documents, "*.wav;*.wave", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseExportAudioFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Export YES DAW Mix",
                                   documents.getChildFile ("YES DAW Mix.wav"),
                                   "*.wav",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".wav");
    };

    // G3.7: the MIDI file choosers.
    choices.chooseImportMidiFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Import MIDI File", documents, "*.mid;*.midi", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    // G3.9: the Sampler pad's WAV chooser.
    choices.chooseSamplerPadFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Load Sample onto Pad", documents, "*.wav;*.wave", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseExportMidiFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Export MIDI File", documents.getChildFile ("YES DAW.mid"), "*.mid", true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".mid");
    };

    return choices;
}

namespace yesdaw::ui {

std::unique_ptr<juce::Component> createMainComponent()
{
    return createNativeMainComponent ({});
}

std::unique_ptr<juce::Component> createNativeMainComponent (std::filesystem::path openBundleAtLaunch,
                                                            std::filesystem::path sessionStateDirectory)
{
    yesdaw::ui::MainComponentFileChoices choices = makeNativeFileChoices();
    choices.openBundleAtLaunch = std::move (openBundleAtLaunch);
    if (! sessionStateDirectory.empty())
    {
        choices.sessionStateDirectory = std::move (sessionStateDirectory);
        return std::make_unique<MainComponent> (std::move (choices), true);
    }

    // G0.1: the Session drive's launch-time seams. Both are absolute paths; anything else is
    // ignored so a stray variable can never point the shell at a relative location.
    const juce::String probe = juce::SystemStats::getEnvironmentVariable ("YESDAW_STATE_PROBE", {});
    if (probe.isNotEmpty() && juce::File::isAbsolutePath (probe))
        choices.stateProbePath = pathFromJuceFile (juce::File (probe));

    const juce::String sessionDir =
        juce::SystemStats::getEnvironmentVariable ("YESDAW_SESSION_STATE_DIR", {});
    if (sessionDir.isNotEmpty() && juce::File::isAbsolutePath (sessionDir))
        choices.sessionStateDirectory = pathFromJuceFile (juce::File (sessionDir));

    return std::make_unique<MainComponent> (std::move (choices), true);
}

std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices)
{
    return std::make_unique<MainComponent> (std::move (fileChoices), false);
}

std::string mainComponentStateProbeJson (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->buildStateProbeJson().toStdString();

    return {};
}

MainComponentSnapshot snapshotMainComponent (const juce::Component& component)
{
    MainComponentSnapshot snapshot;
    snapshot.width = component.getWidth();
    snapshot.height = component.getHeight();
    snapshot.childCount = component.getNumChildComponents();

    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        snapshot.isMainComponent = true;
        snapshot.windowTitle = mainComponent->computedWindowTitle().toStdString();
        snapshot.primaryFileChoicesReady = mainComponent->harnessPrimaryFileChoicesReady();
        snapshot.desktopAudioRequested = mainComponent->harnessDesktopAudioRequested();
        snapshot.desktopAudioOpen = mainComponent->harnessDesktopAudioOpen();
        snapshot.deviceAudioCallbackBlockCount = mainComponent->harnessDeviceAudioCallbackBlockCount();
        snapshot.deviceAudioNonSilentBlockCount = mainComponent->harnessDeviceAudioNonSilentBlockCount();
        snapshot.playbackReady = mainComponent->harnessPlaybackReady();
        snapshot.playbackReplaceCount = mainComponent->harnessPlaybackReplaceCount();
        snapshot.playbackLiveScalarsApplied = mainComponent->harnessPlaybackLiveScalarsApplied();
        snapshot.playbackLoopStartFrame = mainComponent->harnessPlaybackLoopStartFrame();
        snapshot.playbackLoopEndFrame = mainComponent->harnessPlaybackLoopEndFrame();
        snapshot.statusLineText = mainComponent->harnessStatusLineText();
        snapshot.statusLineIsError = mainComponent->harnessStatusLineIsError();
        snapshot.timelineRangeStartFrame = mainComponent->harnessTimelineRangeStartFrame();
        snapshot.timelineRangeEndFrame = mainComponent->harnessTimelineRangeEndFrame();
        snapshot.timelineZoomFactor = mainComponent->harnessTimelineZoomFactor();
        snapshot.timelineScrollSeconds = mainComponent->harnessTimelineScrollSeconds();
        snapshot.timelineTrackScrollRows = mainComponent->harnessTimelineTrackScrollRows();
        snapshot.timelineMaxTrackScrollRows = mainComponent->harnessTimelineMaxTrackScrollRows();
        snapshot.pianoRollViewLowKey = mainComponent->harnessPianoRollViewLowKey();
        snapshot.pianoRollViewZoom = mainComponent->harnessPianoRollViewZoom();
        snapshot.pianoRollViewScrollTicks = mainComponent->harnessPianoRollViewScrollTicks();
        snapshot.visibleTimelineTrackCount = mainComponent->harnessVisibleTimelineTrackCount();
        snapshot.visibleTimelineClipCount = mainComponent->harnessVisibleTimelineClipCount();
        snapshot.visibleFirstTimelineClipName = mainComponent->harnessVisibleFirstTimelineClipName();
        snapshot.selectedTimelineClipCount = mainComponent->harnessSelectedTimelineClipCount();
        snapshot.visibleTimelineTotalSeconds = mainComponent->harnessVisibleTimelineTotalSeconds();
        snapshot.visibleMixerTrackCount = mainComponent->harnessVisibleMixerTrackCount();
        snapshot.visibleMixerBusCount = mainComponent->harnessVisibleMixerBusCount();
        snapshot.selectedMixerStripOrdinal = mainComponent->harnessSelectedMixerStripOrdinal();
        snapshot.visibleMixerLoudnessValid = mainComponent->harnessVisibleMixerLoudnessValid();
        snapshot.visibleMasterPeakLeft = mainComponent->harnessVisibleMasterPeakLeft();
        snapshot.visibleMasterPeakRight = mainComponent->harnessVisibleMasterPeakRight();
        snapshot.visiblePianoRollNoteCount = mainComponent->harnessVisiblePianoRollNoteCount();
        snapshot.bundlePath = mainComponent->harnessBundlePath();
        snapshot.context = mainComponent->harnessContext();
        snapshot.recordingDevice = mainComponent->harnessRecordingDevice();
        snapshot.recordingTrackInput = mainComponent->harnessRecordingTrackInput();
        snapshot.armedRecordingTrackInputs = mainComponent->harnessArmedRecordingTrackInputs();
        snapshot.liveInputMeterPeak = mainComponent->harnessInputMeterPeak();
        snapshot.lastRecordedAudioTake = mainComponent->harnessLastRecordedAudioTake();
        snapshot.lastRecordedMidiTake = mainComponent->harnessLastRecordedMidiTake();
        snapshot.recordingComp = mainComponent->harnessRecordingComp();
        snapshot.autosaveRecovery = mainComponent->harnessAutosaveRecovery();
    }

    return snapshot;
}

std::vector<float> renderMainComponentPlayback (juce::Component& component,
                                                std::uint64_t frames,
                                                int blockSize)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRenderPlaybackFrames (frames, blockSize);

    return {};
}

juce::Rectangle<int> mainComponentPaintedMuteSoloCellBounds (const juce::Component& component,
                                                              int stripIndex,
                                                              int cellIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMuteSoloCellBounds (stripIndex, cellIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedInsertSlotBounds (const juce::Component& component,
                                                            int stripIndex,
                                                            int slotIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedInsertSlotBounds (stripIndex, slotIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedSendRowBounds (const juce::Component& component,
                                                         int stripIndex,
                                                         int sendIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedSendRowBounds (stripIndex, sendIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedFaderRailBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedFaderRailBounds (stripIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedPanKnobBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedPanKnobBounds (stripIndex);
    return {};
}

int mainComponentPaintedFaderThumbY (const juce::Component& component, int stripIndex, float linearGain)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedFaderThumbY (stripIndex, linearGain);

    return 0;
}

juce::Rectangle<int> mainComponentPaintedMixerStripBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMixerStripBounds (stripIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedMixerMasterBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMixerMasterBounds();

    return {};
}

juce::Rectangle<int> mainComponentHeaderSectionBounds (const juce::Component& component, int section)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        const MainComponent::HeaderLayout h = mainComponent->headerLayout();
        switch (section)
        {
            case 0: return h.toolsSection;
            case 1: return h.transportSection;
            case 2: return h.masterSection;
            default: return {};
        }
    }
    return {};
}

std::vector<juce::Rectangle<int>> mainComponentHeaderRects (const juce::Component& component)
{
    std::vector<juce::Rectangle<int>> rects;
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        const MainComponent::HeaderLayout h = mainComponent->headerLayout();
        for (const juce::Rectangle<int>& r : { h.menuBar, h.newButton, h.openButton, h.saveButton, h.importButton,
                                               h.undoButton, h.redoButton, h.exportButton, h.locateStart, h.play,
                                               h.stop, h.record, h.timeReadout, h.tempoMeterBox, h.loop,   // G3.10: the MIDI lamp sits INSIDE the time readout, not beside it
                                               h.masterCard, h.gear, h.bitDepth, h.range, h.outputDevice,
                                               h.inputDevice, h.inputChannel, h.arm, h.monitor, h.comp })
            if (! r.isEmpty())
                rects.push_back (r);
    }
    return rects;
}

juce::Rectangle<int> mainComponentHeaderTimeReadoutBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerLayout().timeReadout;
    return {};
}

int mainComponentHeaderHeight (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerHeightNow();
    return 0;
}

yesdaw::ui::MainComponentKeymapEditor mainComponentKeymapEditor (juce::Component& component)
{
    yesdaw::ui::MainComponentKeymapEditor out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        out.visible = mainComponent->harnessKeymapEditor().isVisible();
        out.rows = mainComponent->harnessKeymapEditor().currentRows();
        out.status = mainComponent->harnessKeymapEditor().statusText();
    }
    return out;
}

juce::String mainComponentHoverHintAt (juce::Component& component, juce::Point<int> shellPoint)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessHoverHintAt (shellPoint);
    return {};
}

void mainComponentKeymapEditorSearch (juce::Component& component, const juce::String& text)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessSearch (text);
}

void mainComponentKeymapEditorSelectRow (juce::Component& component, int row)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessSelectRow (row);
}

void mainComponentKeymapEditorBind (juce::Component& component, const juce::String& chord)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessBind (chord);
}

yesdaw::ui::MainComponentContextMenu mainComponentRequestContextMenu (juce::Component& component, juce::Point<int> shellPoint)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRequestContextMenu (shellPoint);
    return {};
}

void mainComponentSetDockHeight (juce::Component& component, int height)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSetDockHeight (height);
}

juce::String mainComponentTimelineZoneAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineZoneAt (shellPoint, modifiers);
    return "none";
}

juce::String mainComponentTimelineCursorAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineCursorAt (shellPoint, modifiers);
    return "normal";
}

double mainComponentTimelineAutoScrollTick (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineAutoScrollTick();
    return 0.0;
}

void mainComponentInvokeContextMenuId (juce::Component& component, int itemId)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInvokeContextMenuId (itemId);
}

int mainComponentTimeDisplayMenuId (int mode)
{
    return MainComponent::harnessTimeDisplayMenuId (mode);
}

// G4.1: the strip menus' routing choices, the last menu, the I/O rows, the slot texts, the view state.
int mainComponentMixerInputMenuId (int channel, bool stereoPair)
{
    return MainComponent::harnessMixerInputMenuId (channel, stereoPair);
}

int mainComponentMixerOutputMenuId (int choice)
{
    return MainComponent::harnessMixerOutputMenuId (choice);
}

int mainComponentMixerSendDestinationMenuId (int busIndex)
{
    return MainComponent::harnessMixerSendDestinationMenuId (busIndex);
}

MainComponentFxEditor mainComponentFxEditor (const juce::Component& component)
{
    if (const auto* shell = dynamic_cast<const MainComponent*> (&component))
        return shell->harnessFxEditor();
    return {};
}

void mainComponentOpenFxEditor (juce::Component& component, int stripIndex, int slotIndex)
{
    if (auto* shell = dynamic_cast<MainComponent*> (&component))
        shell->harnessOpenFxEditor (stripIndex, slotIndex);
}

int mainComponentMixerSendMenuId (int busIndex)
{
    return MainComponent::harnessMixerSendMenuId (busIndex);
}

MainComponentContextMenu mainComponentLastContextMenu (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessLastContextMenu();
    return {};
}

juce::Rectangle<int> mainComponentPaintedIoRowBounds (const juce::Component& component, int stripIndex, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedIoRowBounds (stripIndex, row);
    return {};
}

MainComponentMixerStripIo mainComponentMixerStripIo (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessMixerStripIo (stripIndex);
    return {};
}

juce::String mainComponentViewStateRecord (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessViewStateRecord();
    return {};
}

void mainComponentInvokeContextMenuItem (juce::Component& component, yesdaw::ui::UiActionId action, int direction)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInvokeContextMenuItem (action, direction);
}

void mainComponentDispatchAction (juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessDispatchAction (action);
}

yesdaw::ui::UiActionState mainComponentActionState (const juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessActionState (action);
    return { false, "not a MainComponent" };
}

void mainComponentSetSettingsRowVisible (juce::Component& component, bool visible)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSetSettingsRowVisible (visible);
}

void mainComponentRevealSettingsRowFor (juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (isSettingsRowAction (action))
        mainComponentSetSettingsRowVisible (component, true);
}

juce::Rectangle<int> mainComponentMixerPanelBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessMixerPanelBounds();

    return {};
}

juce::Rectangle<int> mainComponentTimelineBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineBounds();

    return {};
}

juce::Rectangle<int> mainComponentHeaderMasterCardBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerMasterCardBounds();

    return {};
}

MainComponentUndoHistory mainComponentUndoHistory (juce::Component& component)
{
    MainComponentUndoHistory out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        out.visible = mainComponent->harnessUndoHistory().isVisible();
        mainComponent->harnessUndoHistory().refreshRows();
        out.rows = mainComponent->harnessUndoHistory().currentRows();
        out.current = mainComponent->harnessUndoHistory().currentRow();
    }
    return out;
}

MainComponentPianoRollGrid mainComponentPianoRollGrid (juce::Component& component)
{
    MainComponentPianoRollGrid out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        for (const yesdaw::ui::PianoRollGridLine& line : pianoRollGridLines (geometry, surface))
            out.lines.emplace_back (static_cast<std::int64_t> (line.tick), line.x, static_cast<int> (line.kind));
        out.playheadTick = surface.playheadTick;
        out.viewScrollTicks = surface.viewScrollTicks;
        out.visibleTicks = pianoRollVisibleTicks (surface);
    }
    return out;
}

MainComponentPianoRollAudition mainComponentPianoRollAudition (juce::Component& component, int key)
{
    MainComponentPianoRollAudition out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        out.heldKey = mainComponent->harnessPianoRollAuditionKey();
        out.keyboardX = geometry.keyboard.getCentreX();
        out.keyY = pianoRollKeyY (geometry, surface, key) + juce::roundToInt (geometry.rowHeight * 0.5f);
        out.gridLeft = geometry.grid.getX();
        out.gridRight = geometry.grid.getRight();
    }
    return out;
}

void mainComponentServiceUiTick (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessServiceUiTick();
}

MainComponentPianoRollControlLane mainComponentPianoRollControlLane (juce::Component& component)
{
    MainComponentPianoRollControlLane out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        out.lane = pianoRollControlLaneDataArea (geometry);
        out.chooser = pianoRollControlLaneChooserArea (geometry);
        out.name = yesdaw::ui::pianoRollControlLaneChoice (surface.controlLaneChoice).name;
        if (const auto* lane = pianoRollControlLaneOf (surface))
        {
            out.valueMin = lane->valueMin;
            out.valueMax = lane->valueMax;
            for (const yesdaw::ui::UiPianoRollExpressionPoint& point : lane->points)
            {
                out.points.emplace_back (static_cast<std::int64_t> (point.tick), point.value);
                out.pointIds.push_back (juce::String (entityIdHex (point.entityId)));
            }
        }
    }
    return out;
}

void mainComponentReleaseTypedKeys (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessReleaseTypedKeysForTest();
}

void mainComponentSelectPianoRollScale (juce::Component& component, int rootKey, int scaleChoice)   // G3.8
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSelectPianoRollScale (rootKey, scaleChoice);
}

void mainComponentSelectPianoRollControlLane (juce::Component& component, int choice)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSelectPianoRollControlLane (choice);
}

double mainComponentPianoRollControlValueForY (juce::Component& component, int rollLocalY)
{
    const MainComponentPianoRollControlLane lane = mainComponentPianoRollControlLane (component);
    return pianoRollControlValueForLaneY (lane.lane, rollLocalY, lane.valueMin, lane.valueMax);
}

int mainComponentPianoRollControlYForValue (juce::Component& component, double value)
{
    const MainComponentPianoRollControlLane lane = mainComponentPianoRollControlLane (component);
    return pianoRollControlLaneYForValue (lane.lane, value, lane.valueMin, lane.valueMax);
}

bool mainComponentAuditionNote (juce::Component& component, std::int16_t key, bool on)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessAuditionNote (key, on);
    return false;
}

std::vector<float> mainComponentRenderPlaybackFrames (juce::Component& component, std::uint64_t frames, int blockSize)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRenderPlayback (frames, blockSize);
    return {};
}

MainComponentInstrumentPanel mainComponentInstrumentPanel (juce::Component& component)
{
    MainComponentInstrumentPanel out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        InstrumentPanelComponent& panel = mainComponent->harnessInstrumentPanel();
        out.visible = panel.isVisible();
        panel.refresh();
        out.kind = panel.currentKind();
        for (const InstrumentPanelComponent::Row& row : panel.currentRows())
            out.rows.emplace_back (row.label, row.normalized);
        panel.resized();   // G3.9: the pad grid is laid out in resized(); the readout reports what is painted
        out.padsVisible = panel.padsShown();
        out.padGrid = panel.currentPadGrid();
        for (const InstrumentPanelComponent::Pad& pad : panel.currentPads())
            out.pads.emplace_back (pad.key, pad.name);
    }
    return out;
}

bool mainComponentPostMidiInput (juce::Component& component, bool on, int key, double velocity)   // G3.10
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessPostMidiInput (on, key, velocity);
    return false;
}

void mainComponentInstrumentPanelClickPad (juce::Component& component, int key, bool shift, bool ctrl)   // G3.9
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessClickPad (key, shift, ctrl);
}

void mainComponentInstrumentPanelDropFileOnPad (juce::Component& component, int key, const juce::String& path)   // G3.9
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessDropOnPad (key, juce::StringArray { path });
}

void mainComponentInstrumentPanelSetRow (juce::Component& component, int row, double normalized)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessSetRow (row, normalized);
}

void mainComponentInstrumentPanelDragRow (juce::Component& component, int row, double first, double second)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessDragRow (row, { first, second });
}

void mainComponentUndoHistoryClickRow (juce::Component& component, int row)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessUndoHistory().clickRow (row);
}

juce::Rectangle<int> mainComponentPaintedRailRowBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedRailRowBounds (row);

    return {};
}

juce::Rectangle<int> mainComponentPaintedColourSwatchBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedColourSwatchBounds (row);

    return {};
}

juce::Rectangle<int> mainComponentPaintedRailCellBounds (const juce::Component& component, int row, int cell)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedRailCellBounds (row, cell);
    return {};
}

juce::Rectangle<int> mainComponentPaintedHeaderGearBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerLayout().gear;
    return {};
}

TimelineClipSourceWindow mainComponentTimelineClipSourceWindow (const juce::Component& component, int layoutClipId)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineClipSourceWindow (layoutClipId);
    return {};
}

double mainComponentTimelineZoomCeiling (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineZoomCeiling();
    return 0.0;
}

juce::Colour mainComponentTimelineClipColour (juce::Component& component, yesdaw::engine::EntityId clipId)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineClipColour (clipId);

    return {};
}

yesdaw::engine::BarBeat mainComponentHeaderBarBeat (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessHeaderBarBeat();

    return {};
}

std::vector<yesdaw::ui::RulerBarLabel> mainComponentRulerBarLabels (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRulerBarLabels();

    return {};
}

double mainComponentRulerSecondsAtX (juce::Component& component, int x)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRulerSecondsAtX (x);

    return 0.0;
}

juce::Rectangle<int> mainComponentInspectorFadeChartBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessInspectorFadeChartBounds();

    return {};
}

std::pair<float, float> mainComponentRailMeterChannelPeaks (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessRailMeterChannelPeaks (row);

    return { 0.0f, 0.0f };
}

juce::Rectangle<int> mainComponentRailVolumeSliderBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessRailVolumeSliderBounds (row);

    return {};
}

bool serviceMainComponentUiTimer (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        mainComponent->timerCallback();
        return true;
    }

    return false;
}

bool mainComponentConfirmsClose (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->confirmClose();

    return true;
}

bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessProcessDeviceAudioBlock (outputChannels, numOutputChannels, numFrames);

    return false;
}

bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                           const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessProcessDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);

    return false;
}

juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action)
{
    const juce::String stableId = stableIdForAction (action);
    if (stableId.isEmpty())
        return nullptr;

    return findChildWithComponentId (component, stableId);
}

const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action)
{
    const juce::String stableId = stableIdForAction (action);
    if (stableId.isEmpty())
        return nullptr;

    return findChildWithComponentId (component, stableId);
}

} // namespace yesdaw::ui
