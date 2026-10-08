// YES DAW — the app shell: the keyboard Control target (G4.0b).
//
// ADR-0049 "Keyboard access through the existing command router": the router owns one logical Control
// target, separate from the Focus context. Tab / Shift+Tab walk the visible enabled controls; Enter
// clicks a button or starts a choice / value / text interaction; arrows adjust only that interaction;
// Enter keeps it, Esc restores what it started from, and Esc again ends navigation so the editor has
// the keys back. Space stays transport and a key dispatches once. The pure rules are ui/ControlTarget.h;
// this file finds the controls, performs each effect through the control's own path (a click, a
// chooser's onChange, a slider's drag gesture, the painted fader's drag verb) and paints the ring.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

namespace {

// The mouse half: any press anywhere in the shell hands the keys back to the editors. An open value
// interaction keeps its value, the way a click elsewhere commits a field.
struct ControlNavigationMouseListener final : public juce::MouseListener
{
    std::function<void()> onPress;
    void mouseDown (const juce::MouseEvent&) override
    {
        if (onPress)
            onPress();
    }
};

// ADR-0067 §2: the pointer's half - every enter, move, drag, exit, press and release anywhere in the shell.
struct PointerTracker final : public juce::MouseListener
{
    std::function<void (MainComponentPointerKind, const juce::MouseEvent&)> onEvent;
    void mouseEnter (const juce::MouseEvent& event) override { send (MainComponentPointerKind::enter, event); }
    void mouseMove (const juce::MouseEvent& event) override { send (MainComponentPointerKind::move, event); }
    void mouseDrag (const juce::MouseEvent& event) override { send (MainComponentPointerKind::drag, event); }
    void mouseExit (const juce::MouseEvent& event) override { send (MainComponentPointerKind::exit, event); }
    void mouseDown (const juce::MouseEvent& event) override { send (MainComponentPointerKind::down, event); }
    void mouseUp (const juce::MouseEvent& event) override { send (MainComponentPointerKind::up, event); }
    void send (MainComponentPointerKind kind, const juce::MouseEvent& event)
    {
        if (onEvent)
            onEvent (kind, event);
    }
};

// ADR-0072 §3: the deepest visible component under `point` that takes the mouse, as JUCE routes an event - one that lets
// clicks through (the accessible elements and their layer) passes it on. nullptr: none of `parent`'s children.
// It mirrors JUCE only while no shell component overrides hitTest() or sets a transform: one that does must be handled
// here too, or a tick's walk and an event's routing disagree and hover flickers between them.
[[nodiscard]] juce::Component* deepestTakingMouseAt (juce::Component& parent, juce::Point<int> point)
{
    for (int i = parent.getNumChildComponents() - 1; i >= 0; --i)
    {
        juce::Component* child = parent.getChildComponent (i);
        if (child == nullptr || ! child->isVisible() || ! child->getBounds().contains (point))
            continue;
        bool self = false;
        bool children = false;
        child->getInterceptsMouseClicks (self, children);
        if (children)
            if (juce::Component* deeper = deepestTakingMouseAt (*child, point - child->getPosition()))
                return deeper;
        if (self)
            return child;
    }
    return nullptr;
}

[[nodiscard]] bool isControlWidget (const juce::Component& component)
{
    return dynamic_cast<const juce::Button*> (&component) != nullptr
        || dynamic_cast<const ChooserListControl*> (&component) != nullptr   // ADR-0056: the browser's list
        || dynamic_cast<const juce::ComboBox*> (&component) != nullptr
        || dynamic_cast<const juce::Slider*> (&component) != nullptr
        || dynamic_cast<const juce::TextEditor*> (&component) != nullptr;
}

[[nodiscard]] ControlTargetRole controlRoleOf (const juce::Component& component)
{
    if (const auto* button = dynamic_cast<const juce::Button*> (&component))
        return button->getClickingTogglesState() ? ControlTargetRole::Toggle : ControlTargetRole::Button;
    if (dynamic_cast<const juce::ComboBox*> (&component) != nullptr
        || dynamic_cast<const ChooserListControl*> (&component) != nullptr)
        return ControlTargetRole::Chooser;
    if (dynamic_cast<const juce::Slider*> (&component) != nullptr)
        return ControlTargetRole::Value;
    return ControlTargetRole::TextField;
}

[[nodiscard]] juce::String accessibleNameOf (juce::Component& component)
{
    if (component.getTitle().isNotEmpty())
        return component.getTitle();
    if (const auto* button = dynamic_cast<const juce::Button*> (&component); button != nullptr && button->getButtonText().isNotEmpty())
        return button->getButtonText();
    if (component.getName().isNotEmpty())
        return component.getName();
    if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (&component); tip != nullptr && tip->getTooltip().isNotEmpty())
        return tip->getTooltip();
    return component.getComponentID();
}

[[nodiscard]] ControlTargetRect controlRectOf (juce::Rectangle<int> rect) noexcept
{
    ControlTargetRect out;
    out.x = rect.getX();
    out.y = rect.getY();
    out.width = rect.getWidth();
    out.height = rect.getHeight();
    return out;
}

[[nodiscard]] juce::Rectangle<int> juceRectOf (const ControlTargetRect& rect) noexcept
{
    return { rect.x, rect.y, rect.width, rect.height };
}

[[nodiscard]] std::vector<ControlTargetEntry> entriesOf (const std::vector<MainComponent::ShellControl>& controls)
{
    std::vector<ControlTargetEntry> entries;
    entries.reserve (controls.size());
    for (const auto& control : controls)
        entries.push_back (control.entry);
    return entries;
}

[[nodiscard]] const MainComponent::ShellControl* findControl (const std::vector<MainComponent::ShellControl>& controls,
                                                             const std::string& id)
{
    if (id.empty())
        return nullptr;
    for (const auto& control : controls)
        if (control.entry.id == id)
            return &control;
    return nullptr;
}

constexpr int kRegionHeader = 0;
constexpr int kRegionRail = 1;
constexpr int kRegionArrange = 2;
constexpr int kRegionInspector = 3;
constexpr int kRegionDock = 4;
constexpr int kRegionOverlay = 5;

using L = yesdaw::ui::UiTheme::Layout;

} // namespace

void MainComponent::initialiseControlNavigation()
{
    auto listener = std::make_unique<ControlNavigationMouseListener>();
    listener->onPress = [this] {
        if (! controlNavigator.navigating())
            return;
        if (controlNavigator.interacting())
            finishControlInteraction (true);
        endControlNavigation();
    };
    controlNavigationMouseListener = std::move (listener);
    addMouseListener (controlNavigationMouseListener.get(), true);
}

void MainComponent::initialisePointerTracking()
{
    auto tracker = std::make_unique<PointerTracker>();
    tracker->onEvent = [this] (MainComponentPointerKind kind, const juce::MouseEvent& event) {
        pointerEvent (kind, event.getEventRelativeTo (this).getPosition(), event.eventComponent, event.mods);
    };
    pointerTracker = std::move (tracker);
    addMouseListener (pointerTracker.get(), true);
}

// ADR-0072 §3: what a stationary pointer is over now - an overlay that opened, a tab that switched, a panel that hid.
// Nothing taking the mouse there: the shell's own paint (the header) is under the pointer.
juce::Component* MainComponent::walkChildrenAt (juce::Point<int> shellPoint)
{
    juce::Component* found = deepestTakingMouseAt (*this, shellPoint);
    return found != nullptr ? found : this;
}

// ADR-0067 §2 / ADR-0072 §2: the record under the point whose surface the event's component is in - exactly the shell
// for the shell's own records (the header's), so an overlay over them blocks hover; within the surface for the others.
std::string MainComponent::pointerRecordAt (juce::Point<int> shellPoint, const juce::Component* component) const
{
    if (component == nullptr)
        return {};
    for (const auto& record : pointerRecords)
    {
        const juce::Component* surface = record.surface.getComponent();
        if (surface == nullptr || ! record.bounds.contains (shellPoint))
            continue;
        if (surface == this ? component == this : (surface == component || surface->isParentOf (component)))
            return record.id;
    }
    return {};
}

juce::Rectangle<int> MainComponent::pointerRecordBounds (const std::string& id) const
{
    if (! id.empty())
        for (const auto& record : pointerRecords)
            if (record.id == id)
                return record.bounds;
    return {};
}

// The two pointer states change together; each change repaints the old and the new record's rects (expanded as the
// ring's are), never a surface - and follows a record that moved under a state that did not change.
void MainComponent::setPointerStates (std::string hovered, std::string pressed)
{
    std::vector<juce::Rectangle<int>> repainted;
    const auto add = [&repainted] (juce::Rectangle<int> r) {
        if (r.isEmpty())
            return;
        r = r.expanded (L::controlTargetRingRepaintMargin);
        if (std::find (repainted.begin(), repainted.end(), r) == repainted.end())
            repainted.push_back (r);
    };
    const auto update = [&] (std::string& state, juce::Rectangle<int>& painted, std::string next) {
        const juce::Rectangle<int> nextBounds = pointerRecordBounds (next);
        if (next == state && nextBounds == painted)
            return;
        add (painted);
        state = std::move (next);
        painted = nextBounds;
        add (painted);
    };
    update (pointerHovered, pointerHoveredBounds, std::move (hovered));
    update (pointerPressed, pointerPressedBounds, std::move (pressed));
    if (repainted.empty())
        return;
    for (const auto& r : repainted)
        repaint (r);
    pointerLastRepaint = std::move (repainted);
}

void MainComponent::pointerEvent (MainComponentPointerKind kind, juce::Point<int> shellPoint, juce::Component* component,
                                  juce::ModifierKeys modifiers)
{
    using K = MainComponentPointerKind;
    std::string pressed = pointerPressed;
    if (kind == K::exit)
    {
        // The pointer left the component it was over: off the window, or into a popup. An exit from a child the
        // pointer was not over (an enter arrives with the next one) changes nothing.
        if (component == this || component == pointerComponent.getComponent())
        {
            pointerPosition.reset();
            pointerComponent = nullptr;
            setPointerStates ({}, std::move (pressed));
        }
        return;
    }
    if (kind == K::down)
    {
        if (modifiers.isLeftButtonDown() && ! modifiers.isPopupMenu())
            pressed = pointerPressSeeded ? pointerPressed : pointerRecordAt (shellPoint, component);
        else
            pressed.clear();   // a right or middle press presses nothing - not even what its gesture seeded
        pointerPressSeeded = false;   // the seed was for this press only
    }
    if (kind == K::up)
    {
        pressed.clear();
        pointerPressSeeded = false;
    }
    pointerPosition = shellPoint;
    // A drag's events stay with the component it began on; what is under the pointer is found by the walk (ADR-0072 §7).
    juce::Component* under = kind == K::drag || kind == K::up ? walkChildrenAt (shellPoint) : component;
    pointerComponent = under;
    setPointerStates (pointerRecordAt (shellPoint, under), std::move (pressed));
}

// ADR-0067 §2-§3: every tick the states are re-resolved from the last position and the records now - a press whose
// button is no longer down or whose record went is dropped; with no button down, the component under the pointer is
// found again (an overlay or a tab since the last event).
void MainComponent::servicePointerStates()
{
    std::string pressed = pointerPressed;
    const bool primaryDown = juce::ModifierKeys::currentModifiers.isLeftButtonDown();
    if (! pressed.empty() && (! primaryDown || pointerRecordBounds (pressed).isEmpty()))
        pressed.clear();
    if (pressed.empty() || ! primaryDown)
        pointerPressSeeded = false;   // a seed whose press event never came does not outlive the button
    if (! pointerPosition)
    {
        setPointerStates ({}, std::move (pressed));
        return;
    }
    if (! primaryDown)
        pointerComponent = walkChildrenAt (*pointerPosition);
    setPointerStates (pointerRecordAt (*pointerPosition, pointerComponent.getComponent()), std::move (pressed));
}

// ADR-0072 §10: a gesture that starts from its slop names its record before the press event reaches the tracker.
void MainComponent::pointerPressSeed (std::string id)
{
    pointerPressSeeded = true;
    setPointerStates (pointerHovered, std::move (id));
}

juce::var MainComponent::buildProbePointer() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("hovered", juce::String (pointerHovered));
    object->setProperty ("pressed", juce::String (pointerPressed));
    juce::Array<juce::var> rects;
    for (const auto& r : pointerLastRepaint)
        rects.add (probeRect (r));
    object->setProperty ("lastRepaint", rects);
    return object;
}

int MainComponent::controlRegionAt (juce::Point<int> shellPoint) const
{
    if (shellPoint.y < headerHeightNow())
        return kRegionHeader;
    if (appModel.context().mixerDockVisible && mixerPanelBounds().contains (shellPoint))
        return kRegionDock;
    if (inspectorBounds().contains (shellPoint))
        return kRegionInspector;
    if (shellPoint.x < viewState.railWidth)
        return kRegionRail;
    return kRegionArrange;
}

// The controls Tab walks right now, in Tab order. An open overlay (an FX editor, the keymap editor,
// the undo history) is a panel of its own: the walk stays inside it until it closes. A control's own
// parts (a slider's text box, a combo's label) are not separate stops; nor are scroll-bar arrows.
juce::Component& MainComponent::controlScopeComponent()
{
    if (newProjectDialog.isVisible())   // G5.5 / ADR-0060
        return newProjectDialog;
    if (fxEditorOpen && fxEditor.isVisible())
        return fxEditor;
    if (keymapEditor.isVisible())
        return keymapEditor;
    if (undoHistory.isVisible())
        return undoHistory;
    return *this;
}

std::string MainComponent::controlScopeId()
{
    juce::Component& scope = controlScopeComponent();
    return &scope == this ? std::string() : scope.getComponentID().toStdString();
}

std::vector<MainComponent::ShellControl> MainComponent::collectShellControls()
{
    juce::Component* scope = &controlScopeComponent();
    const juce::Rectangle<int> scopeArea = scope == this ? getLocalBounds()
                                                         : getLocalArea (scope->getParentComponent(), scope->getBounds());

    std::vector<ShellControl> controls;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& parent) {
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
        {
            juce::Component* child = parent.getChildComponent (i);
            if (child == nullptr || ! child->isVisible() || dynamic_cast<juce::ScrollBar*> (child) != nullptr)
                continue;
            if (! isControlWidget (*child))
            {
                walk (*child);
                continue;
            }
            const juce::Rectangle<int> bounds = getLocalArea (child->getParentComponent(), child->getBounds());
            if (! child->isEnabled() || bounds.isEmpty() || ! scopeArea.intersects (bounds))
                continue;
            ShellControl control;
            control.widget = child;
            control.entry.id = child->getComponentID().isNotEmpty() ? child->getComponentID().toStdString()
                                                                     : "unnamed:" + accessibleNameOf (*child).toStdString();
            control.entry.name = accessibleNameOf (*child).toStdString();
            control.entry.role = controlRoleOf (*child);
            control.entry.bounds = controlRectOf (bounds);
            control.entry.region = scope == this ? controlRegionAt (bounds.getCentre()) : kRegionOverlay;
            controls.push_back (std::move (control));
        }
    };
    walk (*scope);

    // ADR-0066: the painted surfaces' controls join the walk beside the widgets.
    if (scope == this)
        collectPaintedControls (controls);

    // Ids are unique (two widgets that share an action's id are told apart by their order of discovery).
    std::map<std::string, int> seen;
    for (auto& control : controls)
        if (const int n = ++seen[control.entry.id]; n > 1)
            control.entry.id += "#" + std::to_string (n);

    std::vector<ControlTargetEntry> ordered = entriesOf (controls);
    orderControlTargets (ordered);
    std::map<std::string, std::size_t> indexOfId;
    for (std::size_t i = 0; i < controls.size(); ++i)
        indexOfId.emplace (controls[i].entry.id, i);
    std::vector<ShellControl> sorted;
    sorted.reserve (controls.size());
    for (const auto& entry : ordered)
        sorted.push_back (std::move (controls[indexOfId.at (entry.id)]));
    return sorted;
}

bool MainComponent::railRowExists (int row) const noexcept
{
    return appModel.context().projectLoaded && row >= 0 && static_cast<std::size_t> (row) < appModel.project().tracks.size();
}

// ADR-0066: every painted hit-zone a mouse can use, as a record the walk takes beside the widgets. Each reads the
// geometry its surface paints and hit-tests with, and runs the callback the mouse path calls.
void MainComponent::collectPaintedControls (std::vector<ShellControl>& controls)
{
    const auto add = [this, &controls] (std::string id, std::string name, ControlTargetRole role, juce::Rectangle<int> bounds,
                                        PaintedControl record)
    {
        if (bounds.isEmpty() || ! getLocalBounds().contains (bounds))
            return;
        ShellControl control;
        control.entry.id = std::move (id);
        control.entry.name = std::move (name);
        control.entry.role = role;
        control.entry.bounds = controlRectOf (bounds);
        control.entry.region = controlRegionAt (bounds.getCentre());
        control.painted = std::make_shared<const PaintedControl> (std::move (record));
        controls.push_back (std::move (control));
    };
    const auto stepDb = [] (double current, int steps, bool fine, double maximum)
    {
        return static_cast<double> (stepFaderGainDb (static_cast<float> (current), steps, fine ? 0.1 : 1.0, maximum));
    };
    const auto stepPan = [] (double current, int steps, bool fine)
    {
        return std::clamp (current + steps * (fine ? 0.01 : 0.05), -1.0, 1.0);
    };

    // The header: the gear (the settings row) and the time readout (its display mode).
    {
        const HeaderLayout header = headerLayout();
        PaintedControl gear;
        gear.activate = [this] { handleAction (UiActionId::ViewToggleSettingsRow); refreshActionState(); };
        gear.checked = appModel.context().settingsRowVisible;
        gear.surface = this;
        add ("header.gear", "Settings row", ControlTargetRole::Toggle, header.gear, std::move (gear));
        PaintedControl time;
        time.activate = [this] { cycleTimeDisplayMode(); };
        time.valueText = [this] { return juce::String (counterStrings().mode); };
        time.surface = this;
        add ("header.time", "Time display", ControlTargetRole::Button, header.timeReadout, std::move (time));
    }

    // The timeline's tool strip: a radio group of seven cells.
    {
        const TimelineCanvasGeometry geometry = timelineCanvasGeometry (timelineInput.getLocalBounds(), makeTimelineState());
        for (std::size_t index = 0; index < kTimelineToolStripOrder.size(); ++index)
        {
            const TimelineTool tool = kTimelineToolStripOrder[index];
            const juce::String toolName (probeToolName (tool));
            PaintedControl cell;
            cell.activate = [this, tool] { if (timelineInput.onToolSelected) timelineInput.onToolSelected (tool); };
            cell.checked = appModel.context().activeTimelineTool == tool;
            cell.surface = &timelineInput;
            add ("tool." + toolName.toLowerCase().toStdString(), (toolName + " tool").toStdString(), ControlTargetRole::Toggle,
                 timelineToolStripCell (geometry.toolbarArea, index).translated (timelineInput.getX(), timelineInput.getY()),
                 std::move (cell));
        }
    }

    // The rail: every whole row on screen — M / S / O, pan, volume, the colour swatch, the meter.
    if (appModel.context().projectLoaded && trackListInput.isVisible())
    {
        const auto& tracks = appModel.project().tracks;
        const juce::Rectangle<int> rows = trackListInput.rowArea().translated (trackListInput.getX(), trackListInput.getY());
        const juce::Point<int> origin = trackListInput.getPosition();
        for (int row = 0; row < static_cast<int> (tracks.size()); ++row)
        {
            const juce::Rectangle<int> rowRect = trackListInput.rowBounds (row).translated (origin.x, origin.y);
            if (rowRect.isEmpty() || ! rows.contains (rowRect))
                continue;   // scrolled out, or the partial row the rail does not paint
            const auto& track = tracks[static_cast<std::size_t> (row)];
            const std::string base = "rail.row." + std::to_string (row);
            const std::string name = track.strip.name;
            const auto rowMenu = [this, centre = rowRect.getCentre() - origin] { trackListInput.requestContextMenu (centre); };
            const auto toggle = [&] (const char* part, const char* words, std::function<void (int)> callback, bool checked,
                                     juce::Rectangle<int> cell)
            {
                PaintedControl record;
                record.activate = [callback = std::move (callback), row] { if (callback) callback (row); };
                record.checked = checked;
                record.surface = &trackListInput;
                record.contextMenu = rowMenu;
                add (base + "." + part, name + " " + words, ControlTargetRole::Toggle, cell.translated (origin.x, origin.y), std::move (record));
            };
            toggle ("mute", "mute", trackListInput.onMuteToggled, track.strip.muted, trackListInput.muteCellBounds (row));
            toggle ("solo", "solo", trackListInput.onSoloToggled, track.strip.soloed, trackListInput.soloCellBounds (row));
            toggle ("arm", "record arm", trackListInput.onArmToggled, appModel.isRecordingTrackIndexArmed (static_cast<std::size_t> (row)),
                    trackListInput.armCellBounds (row));
            {
                PaintedControl pan;
                pan.currentValue = [this, row] {
                    return railRowExists (row) && trackListInput.panValueProvider ? static_cast<double> (trackListInput.panValueProvider (row)) : 0.0;
                };
                pan.setValue = [this, row] (double value, bool ended) {
                    if (railRowExists (row) && trackListInput.onPanEdited)   // a deleted row only closes the gesture
                        trackListInput.onPanEdited (row, static_cast<float> (value));
                    if (ended && trackListInput.onMiniDragEnded)
                        trackListInput.onMiniDragEnded();
                };
                pan.stepValue = stepPan;
                pan.valueText = [this, row] { return panReadoutText (trackListInput.panValueProvider ? trackListInput.panValueProvider (row) : 0.0f); };
                pan.minimum = -1.0;
                pan.maximum = 1.0;
                pan.surface = &trackListInput;
                add (base + ".pan", name + " pan", ControlTargetRole::Value, trackListInput.panKnobBounds (row).translated (origin.x, origin.y), std::move (pan));
            }
            {
                PaintedControl volume;
                volume.currentValue = [this, row] {   // the strip's true gain (the rail paints it capped at unity)
                    return railRowExists (row) ? static_cast<double> (appModel.project().tracks[static_cast<std::size_t> (row)].strip.linearGain) : 1.0;
                };
                volume.setValue = [this, row] (double value, bool ended) {
                    if (railRowExists (row) && trackListInput.onVolumeEdited)   // a deleted row only closes the gesture
                        trackListInput.onVolumeEdited (row, static_cast<float> (value));
                    if (ended && trackListInput.onMiniDragEnded)
                        trackListInput.onMiniDragEnded();
                };
                // The rail's slider tops out at unity, as its drag does; a gain the mixer fader put above unity is stepped from
                // where it is (never snapped down to 0 dB on the first key).
                volume.stepValue = [stepDb] (double current, int steps, bool fine) {
                    return stepDb (current, steps, fine, std::max (1.0, current));
                };
                volume.maximum = yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax;
                volume.valueText = [this, row] { return dbReadoutText (trackListInput.volumeValueProvider ? trackListInput.volumeValueProvider (row) : 1.0f); };
                volume.surface = &trackListInput;
                add (base + ".volume", name + " volume", ControlTargetRole::Value,
                     trackListInput.volumeSliderBounds (row).translated (origin.x, origin.y), std::move (volume));
            }
            {
                PaintedControl swatch;
                swatch.activate = [this, row] { if (trackListInput.onColourSwatchClicked) trackListInput.onColourSwatchClicked (row); };
                swatch.surface = &trackListInput;
                add (base + ".colour", name + " colour: next", ControlTargetRole::Button,
                     trackListInput.colourSwatchBounds (row).translated (origin.x, origin.y), std::move (swatch));
                PaintedControl meter;
                meter.activate = [this, row] { if (trackListInput.onMeterClicked) trackListInput.onMeterClicked (row); };
                meter.surface = &trackListInput;
                add (base + ".meter", name + " meter: clear the clip light", ControlTargetRole::Button,
                     trackListInput.meterZoneBounds (row).translated (origin.x, origin.y), std::move (meter));
            }
        }
    }

    collectPaintedMixerControls (controls);

    // The Sampler's pads, while the dock's Instrument tab shows them: Enter is the plain click (load a sample).
    if (appModel.context().mixerDockVisible && dockShowsInstrument() && instrumentPanel.isVisible() && instrumentPanel.padsShown())
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const juce::Point<int> origin = getLocalArea (instrumentPanel.getParentComponent(), instrumentPanel.getBounds()).getPosition();
        for (int index = 0; index < L::instrumentPanelPadCount; ++index)
        {
            const int key = L::instrumentPanelPadFirstKey + index;
            PaintedControl pad;
            pad.activate = [this, key] { if (instrumentPanel.onPadClicked) instrumentPanel.onPadClicked (key, false, false); };
            pad.surface = &instrumentPanel;
            add ("instrument.pad." + std::to_string (key), "Pad " + std::to_string (index + 1) + ": load a sample",
                 ControlTargetRole::Button, instrumentPanel.padCellBounds (key).translated (origin.x, origin.y), std::move (pad));
        }
    }
}

// ADR-0066: the mixer's painted zones — on every strip shown (ADR-0065: a strip scrolled out has no zones): its S / M / R
// cells, pan, fader, meter, send rows, insert slots and I/O rows; and the master pane's insert slots. Each runs the
// strips input's own callback, and Shift+F10 opens the menu its right-click opens.
void MainComponent::collectPaintedMixerControls (std::vector<ShellControl>& controls)
{
    if (! appModel.context().mixerDockVisible || dockShowsPianoRoll() || dockShowsInstrument() || ! mixerStripsInput.isVisible())
        return;
    const auto add = [this, &controls] (std::string id, std::string name, ControlTargetRole role, juce::Rectangle<int> bounds,
                                        PaintedControl record)
    {
        if (bounds.isEmpty() || ! getLocalBounds().contains (bounds))
            return;
        if (! record.contextMenu)
            record.contextMenu = [this, centre = bounds.getCentre()] { mixerStripsInput.requestContextMenu (centre - mixerStripsInput.getPosition()); };
        record.surface = &mixerStripsInput;
        ShellControl control;
        control.entry.id = std::move (id);
        control.entry.name = std::move (name);
        control.entry.role = role;
        control.entry.bounds = controlRectOf (bounds);
        control.entry.region = kRegionDock;
        control.painted = std::make_shared<const PaintedControl> (std::move (record));
        controls.push_back (std::move (control));
    };
    const auto stripExists = [this] (int strip)
    {
        const auto now = currentMixerSurface();
        return strip >= 0 && static_cast<std::size_t> (strip) < now.tracks.size() + now.buses.size();
    };
    const auto localCentre = [this] (juce::Rectangle<int> shellRect) { return shellRect.getCentre() - mixerStripsInput.getPosition(); };

    const auto surface = currentMixerSurface();
    const std::size_t trackCount = surface.tracks.size();
    const std::size_t stripTotal = trackCount + surface.buses.size();
    for (std::size_t i = 0; i < stripTotal; ++i)
    {
        const juce::Rectangle<int> lane = paintedMixerLaneBounds (i);
        if (lane.isEmpty())
            continue;   // ADR-0065: scrolled out
        const auto& strip = i < trackCount ? surface.tracks[i] : surface.buses[i - trackCount];
        const int stripIndex = static_cast<int> (i);
        const int ioRows = stripIoRows (i);
        const std::string base = "mixer.strip." + std::to_string (i);

        // S / M / R (R on a Track only).
        static constexpr std::array<const char*, 3> kParts {{ "solo", "mute", "arm" }};
        static constexpr std::array<const char*, 3> kWords {{ "solo", "mute", "record arm" }};
        const std::size_t cells = std::min (stripCellCount (i), kParts.size());
        for (std::size_t cell = 0; cell < cells; ++cell)
        {
            PaintedControl record;
            record.activate = [this, stripIndex, cell] {
                if (mixerStripsInput.onMuteSoloCellClicked)
                    mixerStripsInput.onMuteSoloCellClicked (stripIndex, static_cast<int> (cell));
            };
            record.checked = cell == 0 ? strip.soloed
                           : cell == 1 ? strip.muted
                                       : appModel.isRecordingTrackIndexArmed (i);
            add (base + "." + kParts[cell], strip.name + " " + kWords[cell], ControlTargetRole::Toggle,
                 paintedMuteSoloCellBoundsForLane (lane, cell, cells), std::move (record));
        }

        // Pan.
        {
            PaintedControl pan;
            pan.currentValue = [this, stripIndex, stripExists] {
                return stripExists (stripIndex) && mixerStripsInput.panForStrip ? static_cast<double> (mixerStripsInput.panForStrip (stripIndex)) : 0.0;
            };
            pan.setValue = [this, stripIndex, stripExists] (double value, bool ended) {
                if (stripExists (stripIndex) && mixerStripsInput.onPanDragged)
                    mixerStripsInput.onPanDragged (stripIndex, static_cast<float> (value), ended);
                else if (ended)
                {
                    paintedPanDragStrip = -1;   // the strip went with its track or bus: only the bracket closes
                    endAutomationTouchRideIfActive();
                    appModel.endStripGesture();
                }
            };
            pan.stepValue = [] (double current, int steps, bool fine) { return std::clamp (current + steps * (fine ? 0.01 : 0.05), -1.0, 1.0); };
            pan.valueText = [this, stripIndex] { return panReadoutText (mixerStripsInput.panForStrip ? mixerStripsInput.panForStrip (stripIndex) : 0.0f); };
            pan.minimum = -1.0;
            pan.maximum = 1.0;
            add (base + ".pan", strip.name + " pan", ControlTargetRole::Value, paintedPanKnobForLane (lane), std::move (pan));
        }

        // The fader (G4.0b).
        {
            PaintedControl fader;
            fader.currentValue = [this, stripIndex] {
                return mixerStripsInput.faderGainForStrip ? static_cast<double> (mixerStripsInput.faderGainForStrip (stripIndex)) : 1.0;
            };
            fader.setValue = [this, stripIndex, stripExists] (double gain, bool ended) {
                if (stripExists (stripIndex) && mixerStripsInput.onFaderDragged)
                {
                    mixerStripsInput.onFaderDragged (stripIndex, static_cast<float> (gain), ended);
                }
                else if (ended)
                {
                    // The strip went with its track or bus: only the drag's bracket closes (no edit, no empty step).
                    paintedFaderDragStrip = -1;
                    endAutomationTouchRideIfActive();
                    appModel.endStripGesture();
                    hideDragDbReadout();
                }
            };
            fader.stepValue = [] (double current, int steps, bool fine) {
                return static_cast<double> (stepFaderGainDb (static_cast<float> (current), steps, fine ? 0.1 : 1.0,
                                                             yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax));
            };
            fader.valueText = [this, stripIndex] {
                return dbReadoutText (mixerStripsInput.faderGainForStrip ? mixerStripsInput.faderGainForStrip (stripIndex) : 1.0f);
            };
            fader.maximum = yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax;
            add (base + ".fader", strip.name + " fader", ControlTargetRole::Value, paintedFaderRailForLane (lane, ioRows), std::move (fader));
        }

        // The meter: a click clears its clip light.
        {
            PaintedControl meter;
            meter.activate = [this, stripIndex] { if (mixerStripsInput.onMeterClicked) mixerStripsInput.onMeterClicked (stripIndex); };
            add (base + ".meter", strip.name + " meter: clear the clip light", ControlTargetRole::Button,
                 paintedMeterBoundsForLane (lane, ioRows), std::move (meter));
        }

        // The I/O rows: a click opens the row's choices.
        for (const int row : { kMixerIoInputRow, kMixerIoOutputRow })
        {
            const juce::Rectangle<int> rect = row == kMixerIoInputRow ? paintedInputRowBoundsForLane (lane, ioRows)
                                                                      : paintedOutputRowBoundsForLane (lane, ioRows);
            PaintedControl io;
            io.activate = [this, stripIndex, row, centre = localCentre (rect)] {
                if (mixerStripsInput.onIoRowClicked)
                    mixerStripsInput.onIoRowClicked (stripIndex, row, centre);
            };
            add (base + (row == kMixerIoInputRow ? ".input" : ".output"),
                 strip.name + (row == kMixerIoInputRow ? " input: choose" : " output: choose"), ControlTargetRole::Button, rect, std::move (io));
        }

        // The send rows: a routed send's level is a value (the drag previews, the release commits); an empty well is its
        // add menu.
        for (std::size_t send = 0; send < static_cast<std::size_t> (paintedSendRowCountForLane (lane, ioRows)); ++send)
        {
            const juce::Rectangle<int> rect = paintedSendRowBoundsForLane (lane, send, ioRows);
            const int sendIndex = static_cast<int> (send);
            const std::string id = base + ".send." + std::to_string (send);
            PaintedControl record;
            if (mixerStripsInput.sendRowFilled && mixerStripsInput.sendRowFilled (stripIndex, sendIndex))
            {
                record.currentValue = [this, stripIndex, sendIndex] {
                    if (paintedSendDragPreview.stripIndex == stripIndex && paintedSendDragPreview.sendIndex == sendIndex)
                        return static_cast<double> (paintedSendDragPreview.level);   // the value the drag shows, not yet committed
                    return mixerStripsInput.sendLevelForRow ? static_cast<double> (mixerStripsInput.sendLevelForRow (stripIndex, sendIndex)) : 1.0;
                };
                record.setValue = [this, stripIndex, sendIndex, stripExists] (double level, bool ended) {
                    if (stripExists (stripIndex) && mixerStripsInput.onSendRowDragged)
                        mixerStripsInput.onSendRowDragged (stripIndex, sendIndex, level, ended);
                    else if (ended)
                        paintedSendDragPreview = {};
                };
                record.stepValue = [] (double current, int steps, bool fine) {
                    return static_cast<double> (stepFaderGainDb (static_cast<float> (current), steps, fine ? 0.1 : 1.0, 1.0));
                };
                record.valueText = [this, stripIndex, sendIndex] {
                    const bool previewing = paintedSendDragPreview.stripIndex == stripIndex && paintedSendDragPreview.sendIndex == sendIndex;
                    return dbReadoutText (previewing ? paintedSendDragPreview.level
                                                     : mixerStripsInput.sendLevelForRow ? mixerStripsInput.sendLevelForRow (stripIndex, sendIndex) : 1.0f);
                };
                add (id, strip.name + " send " + std::to_string (send + 1) + " level", ControlTargetRole::Value, rect, std::move (record));
            }
            else
            {
                record.activate = [this, stripIndex, sendIndex, centre = localCentre (rect)] {
                    if (mixerStripsInput.onStripClicked)
                        mixerStripsInput.onStripClicked (stripIndex);
                    if (mixerStripsInput.onContextMenuRequested)
                        mixerStripsInput.onContextMenuRequested (yesdaw::ui::ContextMenuTarget::MixerSendRow, sendIndex, centre);
                };
                add (id, strip.name + " send " + std::to_string (send + 1) + ": add", ControlTargetRole::Button, rect, std::move (record));
            }
        }

        // The insert slots: a filled slot opens its editor; an empty one is its add menu.
        for (std::size_t slot = 0; slot < static_cast<std::size_t> (paintedInsertRowCountForLane (lane)); ++slot)
        {
            const juce::Rectangle<int> rect = paintedInsertRowBoundsForLane (lane, slot, ioRows);
            const int slotIndex = static_cast<int> (slot);
            const bool filled = mixerStripsInput.insertSlotFilled && mixerStripsInput.insertSlotFilled (stripIndex, slotIndex);
            PaintedControl record;
            record.activate = [this, stripIndex, slotIndex, filled, centre = localCentre (rect)] {
                if (mixerStripsInput.onInsertSlotClicked)
                    mixerStripsInput.onInsertSlotClicked (stripIndex, slotIndex);
                if (filled && mixerStripsInput.onInsertSlotDoubleClicked)
                    mixerStripsInput.onInsertSlotDoubleClicked (stripIndex, slotIndex);
                else if (! filled && mixerStripsInput.onContextMenuRequested)
                    mixerStripsInput.onContextMenuRequested (yesdaw::ui::ContextMenuTarget::InsertSlot, slotIndex, centre);
            };
            add (base + ".insert." + std::to_string (slot),
                 strip.name + " insert " + std::to_string (slot + 1) + (filled ? ": open its editor" : ": add"), ControlTargetRole::Button,
                 rect, std::move (record));
        }
    }

    // ADR-0067 §5: the master clip indicator is a painted record / Control target like a strip's
    // meter — clickable, hoverable, keyboard-reachable and in the accessibility tree. Activation
    // (and a mouse click on this column, routed through meterStripAtPosition) clears both
    // channels' latch and held peak via clearMasterMeterHold. Bounds = the master meters column,
    // the same `columns.meters` the probe publishes as `mixer.master.meters`.
    {
        const juce::Rectangle<int> masterMeterBounds = paintedMasterMeterColumnBounds();
        if (! masterMeterBounds.isEmpty())
        {
            PaintedControl record;
            record.activate = [this] { clearMasterMeterHold(); };
            add ("mixer.master.meter", "Master clip indicator", ControlTargetRole::Button,
                 masterMeterBounds, std::move (record));
        }
    }

    // The master pane's insert slots (its fader is a native slider; its meters are indicators).
    for (std::size_t slot = 0; slot < static_cast<std::size_t> (paintedMasterInsertRowCount()); ++slot)
    {
        const juce::Rectangle<int> rect = paintedMasterInsertRowBounds (slot);
        const int master = static_cast<int> (stripTotal);
        const int slotIndex = static_cast<int> (slot);
        const bool filled = mixerStripsInput.insertSlotFilled && mixerStripsInput.insertSlotFilled (master, slotIndex);
        PaintedControl record;
        record.activate = [this, master, slotIndex, filled, centre = localCentre (rect)] {
            if (mixerStripsInput.onInsertSlotClicked)
                mixerStripsInput.onInsertSlotClicked (master, slotIndex);
            if (filled && mixerStripsInput.onInsertSlotDoubleClicked)
                mixerStripsInput.onInsertSlotDoubleClicked (master, slotIndex);
            else if (! filled && mixerStripsInput.onContextMenuRequested)
                mixerStripsInput.onContextMenuRequested (yesdaw::ui::ContextMenuTarget::InsertSlot, slotIndex, centre);
        };
        add ("mixer.master.insert." + std::to_string (slot),
             "Master insert " + std::to_string (slot + 1) + (filled ? ": open its editor" : ": add"), ControlTargetRole::Button,
             rect, std::move (record));
    }
}

// ADR-0066 cp2: one accessible element per painted control, on the surface that paints it, in Tab order. When the set
// of painted controls (their ids, roles and surfaces) changes, a control that stays keeps its element (a screen reader's
// focus on it survives a scroll), only the controls that came get new ones and only those that went are retired; then
// each element is moved onto its control and its model (the record's effects and readouts) refreshed.
void MainComponent::syncPaintedAccessibilityProxies()
{
    if (! paintedProxiesReady)   // latched once at the end of construction: the pointer records below follow every sync
        return;
    std::vector<ShellControl> painted;
    collectPaintedControls (painted);
    {
        std::vector<ControlTargetEntry> entries;
        entries.reserve (painted.size());
        for (const auto& control : painted)
            entries.push_back (control.entry);
        orderControlTargets (entries);
        std::map<std::string, std::size_t> indexOf;
        for (std::size_t i = 0; i < painted.size(); ++i)
            if (! indexOf.emplace (painted[i].entry.id, i).second)
                jassertfalse;   // ids are unique by construction (layout names); a duplicate is a provider bug
        std::vector<ShellControl> sorted;
        sorted.reserve (painted.size());
        for (const auto& entry : entries)
            if (const auto it = indexOf.find (entry.id); it != indexOf.end() && ! painted[it->second].entry.id.empty())
            {
                sorted.push_back (std::move (painted[it->second]));
                indexOf.erase (it);   // never moved twice
            }
        painted = std::move (sorted);
    }
    const auto surfaceOf = [this] (const ShellControl& control) -> juce::Component&
    {
        juce::Component* surface = control.painted->surface;
        return surface == nullptr || surface == this ? paintedAccessibilityLayer : *surface;
    };
    pointerRecords.clear();   // ADR-0067 §2: the records hover resolves against, with the surface that paints each
    pointerRecords.reserve (painted.size());
    for (const auto& control : painted)
        pointerRecords.push_back ({ control.entry.id, juceRectOf (control.entry.bounds), control.painted->surface });

    std::vector<std::string> shape;
    shape.reserve (painted.size());
    for (const auto& control : painted)
        shape.push_back (control.entry.id + "|" + controlTargetRoleName (control.entry.role) + "|"
                         + std::to_string (reinterpret_cast<std::uintptr_t> (&surfaceOf (control))));
    if (shape != paintedProxyShape)
    {
        std::map<std::string, std::unique_ptr<PaintedAccessibleProxy>> kept;
        for (std::size_t i = 0; i < paintedProxies.size() && i < paintedProxyShape.size(); ++i)
            kept.emplace (paintedProxyShape[i], std::move (paintedProxies[i]));
        std::vector<std::unique_ptr<PaintedAccessibleProxy>> pool;
        pool.reserve (painted.size());
        bool appended = false;
        for (std::size_t i = 0; i < painted.size(); ++i)
        {
            if (const auto it = kept.find (shape[i]); it != kept.end() && it->second != nullptr)
            {
                pool.push_back (std::move (it->second));
                kept.erase (it);
                continue;
            }
            auto proxy = std::make_unique<PaintedAccessibleProxy>();
            ++paintedProxyCreations;
            surfaceOf (painted[i]).addAndMakeVisible (*proxy);
            appended = true;
            pool.push_back (std::move (proxy));
        }
        // Retired, not destroyed: an element's own action can be what changed the set (its handler is on the stack), so
        // it leaves its surface now and is deleted on the next message-loop turn.
        for (auto& [key, old] : kept)
        {
            if (old == nullptr)
                continue;
            if (juce::Component* parent = old->getParentComponent())
                parent->removeChildComponent (old.get());
            retiredPaintedProxies.push_back (std::move (old));
        }
        if (! retiredPaintedProxies.empty())
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] {
                if (safe != nullptr)
                    safe->retiredPaintedProxies.clear();
            });
        // A surface's children follow Tab order: when a control came in ahead of kept ones (a scroll back), the
        // surface's elements are put back in order (each raised in turn lands them, in order, after its other children).
        if (appended)
        {
            std::map<juce::Component*, std::vector<PaintedAccessibleProxy*>> bySurface;
            for (auto& proxy : pool)
                bySurface[proxy->getParentComponent()].push_back (proxy.get());
            for (auto& [surface, inOrder] : bySurface)
            {
                bool ordered = true;
                for (std::size_t i = 1; i < inOrder.size() && ordered; ++i)
                    ordered = surface->getIndexOfChildComponent (inOrder[i - 1]) < surface->getIndexOfChildComponent (inOrder[i]);
                if (! ordered)
                    for (PaintedAccessibleProxy* proxy : inOrder)
                        proxy->toFront (false);
            }
        }
        paintedProxies = std::move (pool);
        paintedProxyShape = std::move (shape);
    }

    for (std::size_t i = 0; i < painted.size() && i < paintedProxies.size(); ++i)
    {
        const ShellControl& control = painted[i];
        PaintedAccessibleProxy& proxy = *paintedProxies[i];
        juce::Component& surface = surfaceOf (control);
        proxy.setBounds (surface.getLocalArea (this, juceRectOf (control.entry.bounds)));
        proxy.setTitle (juce::String (control.entry.name));
        const std::shared_ptr<const PaintedControl> record = control.painted;
        PaintedAccessibleProxy::Model model;
        model.targetId = control.entry.id;
        model.role = control.entry.role == ControlTargetRole::Toggle ? PaintedAccessibleProxy::Role::Toggle
                   : control.entry.role == ControlTargetRole::Value  ? PaintedAccessibleProxy::Role::Value
                                                                     : PaintedAccessibleProxy::Role::Button;
        model.checked = [record] { return record->checked; };
        model.valueText = [this, control] { return controlValueText (control); };
        if (record->currentValue)
            model.currentValue = record->currentValue;
        if (record->setValue)
            model.setValue = [record] (double value) { record->setValue (value, true); };   // one step, as one drag
        model.minimum = record->minimum;
        model.maximum = record->maximum;
        if (record->activate)
            model.press = [record] { record->activate(); };   // the mouse path's effect refreshes the shell itself
        if (record->contextMenu)
            model.showMenu = record->contextMenu;
        proxy.setModel (std::move (model));
    }
}

PaintedAccessibleProxy* MainComponent::paintedProxyFor (const std::string& targetId) const
{
    for (const auto& proxy : paintedProxies)
        if (proxy->getModel().targetId == targetId)
            return proxy.get();
    return nullptr;
}

void MainComponent::openControlTargetContextMenu()
{
    const auto controls = collectShellControls();
    const ShellControl* control = findControl (controls, controlNavigator.targetId());
    if (control == nullptr || control->painted == nullptr || ! control->painted->contextMenu)
        return;
    lastControlActivation = "menu:" + control->entry.id;
    const auto painted = control->painted;
    painted->contextMenu();
}

juce::String MainComponent::controlValueText (const ShellControl& control) const
{
    if (control.painted != nullptr)
    {
        if (control.painted->valueText)
            return control.painted->valueText();
        return control.entry.role == ControlTargetRole::Toggle ? juce::String (control.painted->checked ? "on" : "off") : juce::String();
    }
    juce::Component* widget = control.widget.getComponent();
    if (widget == nullptr)
        return {};
    if (auto* combo = dynamic_cast<juce::ComboBox*> (widget))
    {
        if (controlNavigator.interacting() && control.entry.id == controlNavigator.targetId() && controlChooserPreview >= 0)
            return combo->getItemText (controlChooserPreview);
        return combo->getText();
    }
    if (const auto* list = dynamic_cast<const ChooserListControl*> (widget))   // ADR-0056: the selected row read out
        return list->chooserText();
    if (auto* slider = dynamic_cast<juce::Slider*> (widget))
        return slider == &mixerMasterFader ? dbReadoutText (slider->getValue()) : slider->getTextFromValue (slider->getValue());
    if (auto* editor = dynamic_cast<juce::TextEditor*> (widget))
        return editor->getText();
    if (auto* button = dynamic_cast<juce::Button*> (widget); button != nullptr && button->getClickingTogglesState())
        return button->getToggleState() ? "on" : "off";
    return {};
}

bool MainComponent::routeControlTargetKey (const juce::KeyPress& key)
{
    const juce::ModifierKeys mods = key.getModifiers();
    const int code = key.getKeyCode();
    const bool plain = ! mods.isCtrlDown() && ! mods.isCommandDown() && ! mods.isAltDown();

    ControlKey control = ControlKey::Other;
    int listSteps = 0;
    if (code == juce::KeyPress::tabKey && plain)
        control = mods.isShiftDown() ? ControlKey::ShiftTab : ControlKey::Tab;
    else if (code == juce::KeyPress::returnKey && plain && ! mods.isShiftDown())
        control = ControlKey::Enter;
    else if (code == juce::KeyPress::escapeKey && plain && ! mods.isShiftDown())
        control = ControlKey::Escape;
    else if (code == juce::KeyPress::spaceKey && plain && ! mods.isShiftDown())
        control = ControlKey::Space;
    else if (plain && (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey))
    {
        control = ControlKey::Increase;
        listSteps = code == juce::KeyPress::upKey ? -1 : 1;
    }
    else if (plain && (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey))
    {
        control = ControlKey::Decrease;
        listSteps = code == juce::KeyPress::downKey ? 1 : -1;
    }

    // ADR-0066: Shift+F10 opens the target's right-click menu while navigating (JUCE delivers the Menu key as a
    // modifier change, never as a key press). Outside navigation the chord is the keymap's (it binds nothing).
    if (code == juce::KeyPress::F10Key && mods.isShiftDown() && ! mods.isCtrlDown() && ! mods.isCommandDown() && ! mods.isAltDown()
        && controlNavigator.navigating() && dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) == nullptr)
    {
        revalidateControlTarget();
        if (controlNavigator.interacting())
            finishControlInteraction (true);
        openControlTargetContextMenu();
        return true;
    }

    // Active text entry outranks the router: a field keeps its keys. Only Tab leaves it, committing
    // nothing on its own (the field's focus-lost law decides) and moving on as Tab anywhere would.
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr)
    {
        if (control != ControlKey::Tab && control != ControlKey::ShiftTab)
            return false;
        grabKeyboardFocus();
    }

    bool chordIsGlobalTransport = false;
    if (control == ControlKey::Increase || control == ControlKey::Decrease)
    {
        const UiActionId action = appModel.registry().keymap().actionForChord (
            chordForKeyPress (key), focusContextForPanel (appModel.context().activePanel));
        if (action != UiActionId::Count && defaultFocusContext (action) == UiFocusContext::Global)
            if (const UiActionDescriptor* descriptor = appModel.registry().descriptor (action))
                chordIsGlobalTransport = std::string_view (descriptor->stableId).starts_with ("transport.");
    }

    if (controlNavigator.navigating())
        revalidateControlTarget();   // a key never acts on a control that has gone

    switch (routeControlKey (control, controlNavigator.navigating(), controlNavigator.interacting(), chordIsGlobalTransport))
    {
        case ControlKeyRoute::Keymap:
            return false;
        case ControlKeyRoute::Next:
            stepControlTarget (1);
            return true;
        case ControlKeyRoute::Previous:
            stepControlTarget (-1);
            return true;
        case ControlKeyRoute::Activate:
            activateControlTarget();
            return true;
        case ControlKeyRoute::Confirm:
            finishControlInteraction (true);
            return true;
        case ControlKeyRoute::Cancel:
            finishControlInteraction (false);
            return true;
        case ControlKeyRoute::EndNavigation:
            endControlNavigation();
            return true;
        case ControlKeyRoute::Adjust:
            adjustControlTarget (control == ControlKey::Increase ? 1 : -1, listSteps, mods.isShiftDown());
            return true;
    }
    return false;
}

void MainComponent::stepControlTarget (int direction)
{
    if (controlNavigator.interacting())
        finishControlInteraction (true);   // Tab moves on with the value the user reached
    const auto controls = collectShellControls();
    const auto entries = entriesOf (controls);
    controlScope = controlScopeId();
    (void) controlNavigator.step (entries, direction);
    refreshControlTargetRing (controls);
    if (const ShellControl* control = findControl (controls, controlNavigator.targetId()))
        announceControlTarget (*control, false);
}

void MainComponent::activateControlTarget()
{
    const auto controls = collectShellControls();
    const ShellControl* control = findControl (controls, controlNavigator.targetId());
    if (control == nullptr)
        return;

    juce::Component* widget = control->widget.getComponent();
    const std::string& id = control->entry.id;
    if (control->painted != nullptr)
    {
        if (control->entry.role == ControlTargetRole::Value && control->painted->currentValue)
        {
            lastControlActivation = "value:" + id;
            controlNavigator.beginInteraction (control->painted->currentValue());
        }
        else if (control->painted->activate)
        {
            // The click's own effect, through the callback the mouse path calls.
            lastControlActivation = "click:" + id;
            const auto painted = control->painted;   // the record outlives a rebuild of the walk
            painted->activate();
        }
    }
    else if (auto* button = dynamic_cast<juce::Button*> (widget))
    {
        // The click's own path: the toggle flips, onClick dispatches the action, exactly as a mouse click.
        lastControlActivation = "click:" + id;
        button->triggerClick();
    }
    else if (auto* combo = dynamic_cast<juce::ComboBox*> (widget))
    {
        lastControlActivation = "choose:" + id;
        controlChooserPreview = combo->getSelectedItemIndex();
        controlNavigator.beginInteraction (static_cast<double> (controlChooserPreview));
    }
    else if (auto* list = dynamic_cast<ChooserListControl*> (widget))   // ADR-0056: Enter starts browsing the list
    {
        lastControlActivation = "choose:" + id;
        controlChooserPreview = list->chooserSelection();
        controlNavigator.beginInteraction (static_cast<double> (controlChooserPreview));
    }
    else if (auto* slider = dynamic_cast<juce::Slider*> (widget))
    {
        lastControlActivation = "value:" + id;
        controlNavigator.beginInteraction (slider->getValue());
    }
    else if (auto* editor = dynamic_cast<juce::TextEditor*> (widget))
    {
        lastControlActivation = "text:" + id;
        editor->grabKeyboardFocus();   // active text entry: the field has the keys until it lets go
    }
    refreshControlTargetRing (controls);
}

void MainComponent::adjustControlTarget (int valueSteps, int listSteps, bool fine)
{
    const auto controls = collectShellControls();
    const ShellControl* control = findControl (controls, controlNavigator.targetId());
    if (control == nullptr)
        return;

    juce::Component* widget = control->widget.getComponent();
    if (control->painted != nullptr)
    {
        const auto& painted = *control->painted;
        if (! painted.currentValue || ! painted.setValue || ! painted.stepValue)
            return;
        const double next = std::clamp (painted.stepValue (painted.currentValue(), valueSteps, fine), painted.minimum, painted.maximum);
        if (! controlGestureOpen)
        {
            controlGestureOpen = true;
            controlGestureTarget = *control;
        }
        // The drag verb: the control's own gesture (one undo step), its readout and its Touch / Latch ride.
        painted.setValue (next, false);
    }
    else if (auto* list = dynamic_cast<ChooserListControl*> (widget))
    {
        // ADR-0056: the arrows move the selection (a preview); Enter keeps it (imports / opens), Esc restores it.
        const int count = list->chooserCount();
        if (count > 0 && listSteps != 0)
        {
            controlChooserPreview = std::clamp (controlChooserPreview + listSteps, 0, count - 1);
            list->chooserPreview (controlChooserPreview);
        }
    }
    else if (auto* combo = dynamic_cast<juce::ComboBox*> (widget))
    {
        // A choice is previewed, not applied: Enter applies it through onChange, Esc leaves it untouched,
        // so stepping past an audio device never reopens it.
        const int count = combo->getNumItems();
        for (int i = controlChooserPreview + listSteps; listSteps != 0 && i >= 0 && i < count; i += listSteps)
            if (combo->isItemEnabled (combo->getItemId (i)))
            {
                controlChooserPreview = i;
                combo->setSelectedItemIndex (i, juce::dontSendNotification);
                break;
            }
    }
    else if (auto* slider = dynamic_cast<juce::Slider*> (widget))
    {
        if (! controlGestureOpen)
        {
            // The drag's own bracket: one strip gesture, and the slider's ride / coalescing hooks.
            controlGestureOpen = true;
            controlGestureTarget = *control;
            appModel.beginStripGesture();
            if (slider->onDragStart)
                slider->onDragStart();
        }
        const double before = slider->getValue();
        if (slider == &mixerMasterFader)
        {
            slider->setValue (stepFaderGainDb (static_cast<float> (before), valueSteps, fine ? 0.1 : 1.0, slider->getMaximum()),
                              juce::sendNotificationSync);
        }
        else
        {
            const double proportion = slider->valueToProportionOfLength (before);
            const double moved = juce::jlimit (0.0, 1.0, proportion + (fine ? 0.001 : 0.01) * valueSteps);
            slider->setValue (slider->proportionOfLengthToValue (moved), juce::sendNotificationSync);
            if (slider->getValue() == before && slider->getInterval() > 0.0)   // a coarse step snaps back: move one interval
                slider->setValue (before + slider->getInterval() * valueSteps, juce::sendNotificationSync);
        }
    }
    refreshControlTargetRing (controls);
    announceControlTarget (*control, true);
}

// Close an open value gesture the way the matching drag release would. `keep` false restores the
// value the interaction started from first (inside the same gesture).
void MainComponent::closeControlGesture (bool keep)
{
    if (! controlGestureOpen)
        return;
    controlGestureOpen = false;
    const double origin = controlNavigator.interactionOrigin();
    if (controlGestureTarget.painted != nullptr)
    {
        const auto& painted = *controlGestureTarget.painted;
        if (painted.setValue && painted.currentValue)
            painted.setValue (keep ? painted.currentValue() : origin, true);   // the drag's release, restoring first on Esc
        else
            appModel.endStripGesture();
    }
    else if (auto* slider = dynamic_cast<juce::Slider*> (controlGestureTarget.widget.getComponent()))
    {
        if (! keep)
            slider->setValue (origin, juce::sendNotificationSync);
        if (slider->onDragEnd)
            slider->onDragEnd();
        appModel.endStripGesture();
    }
    else
    {
        appModel.endStripGesture();   // the slider went with its panel: the gesture still closes
    }
    controlGestureTarget = {};
}

void MainComponent::finishControlInteraction (bool keep)
{
    if (! controlNavigator.interacting())
        return;
    const auto controls = collectShellControls();
    const ShellControl* control = findControl (controls, controlNavigator.targetId());
    ChooserListControl* keptList = nullptr;
    if (control != nullptr)
        if (auto* list = dynamic_cast<ChooserListControl*> (control->widget.getComponent()))   // ADR-0056
        {
            if (keep)
                keptList = list;   // acted on after the interaction closes (opening a folder rebuilds the rows)
            else
                list->chooserPreview (static_cast<int> (controlNavigator.interactionOrigin()));
        }
    if (control != nullptr)
        if (auto* combo = dynamic_cast<juce::ComboBox*> (control->widget.getComponent()))
        {
            const int origin = static_cast<int> (controlNavigator.interactionOrigin());
            if (keep && controlChooserPreview >= 0 && controlChooserPreview != origin)
            {
                combo->setSelectedItemIndex (origin, juce::dontSendNotification);   // so onChange sees a change
                combo->setSelectedItemIndex (controlChooserPreview, juce::sendNotificationSync);
            }
            else if (! keep)
            {
                combo->setSelectedItemIndex (origin, juce::dontSendNotification);
            }
        }
    closeControlGesture (keep);
    controlChooserPreview = -1;
    controlNavigator.endInteraction();
    if (keptList != nullptr)
        keptList->chooserKeep();
    refreshActionState();
    repaintAll();
    const auto after = collectShellControls();
    refreshControlTargetRing (after);
    if (const ShellControl* current = findControl (after, controlNavigator.targetId()))
        announceControlTarget (*current, true);
}

void MainComponent::endControlNavigation()
{
    if (controlNavigator.interacting())
        finishControlInteraction (true);
    controlNavigator.endNavigation();
    controlTargetBeforeOverlay.clear();
    controlChooserPreview = -1;
    controlTargetWidget = nullptr;
    if (! controlRingArea.isEmpty())
        repaint (controlRingArea);
    controlRingArea = {};
    // The screen reader's focus returns to the shell, so the tick's adoption poll stays quiet.
    if (auto* handler = getAccessibilityHandler())
        handler->grabFocus();
    lastSeenAccessibilityFocus = accessibilityFocusComponent();
}

void MainComponent::revalidateControlTarget()
{
    if (! controlNavigator.navigating())
        return;
    const std::string scopeBefore = controlScope;
    controlScope = controlScopeId();
    const bool wasInteracting = controlNavigator.interacting();
    const auto controls = collectShellControls();
    const auto entries = entriesOf (controls);

    bool moved = false;
    if (controlScope != scopeBefore)
    {
        if (! controlScope.empty())
        {
            // An overlay opened: Tab now lives in it, starting at its first control.
            if (scopeBefore.empty())
                controlTargetBeforeOverlay = controlNavigator.targetId();
            controlNavigator.endNavigation();
            (void) controlNavigator.step (entries, 1);
        }
        else
        {
            // It closed: back to the control navigation was on before it opened.
            if (! controlNavigator.targetById (entries, controlTargetBeforeOverlay))
                (void) controlNavigator.revalidate (entries);
            controlTargetBeforeOverlay.clear();
        }
        moved = true;
    }
    else
    {
        moved = controlNavigator.revalidate (entries);
    }

    if (moved && wasInteracting)
    {
        // The control under an open interaction went away: its gesture closes with what was reached.
        closeControlGesture (true);
        controlChooserPreview = -1;
        controlNavigator.endInteraction();
    }
    refreshControlTargetRing (controls);
    if (moved)
        if (const ShellControl* control = findControl (controls, controlNavigator.targetId()))
            announceControlTarget (*control, false);
}

// A screen reader moved its focus onto one of our controls: that control becomes the target. A text
// field with keyboard focus is text entry, not targeting; the shell or a painted surface is not a control.
// Runs every UI tick, so it must stay quiet for the router's own focus moves: announceControlTarget and
// endControlNavigation record the focus their move left (lastSeenAccessibilityFocus), so only a later,
// outside move adopts. Without that the tick would re-target what the router just set.
juce::Component* MainComponent::accessibilityFocusComponent()
{
    // JUCE parents an element on its nearest focus container: the window, not this shell (which is not one), so the
    // shell's own handler never sees a child's focus. Ask the window's, and keep only what lies inside the shell
    // (2026-10-07: the desktop drive's UI Automation step found a screen reader's focus move was never adopted).
    juce::Component* top = getTopLevelComponent();
    juce::AccessibilityHandler* rootHandler = top != nullptr ? top->getAccessibilityHandler() : nullptr;
    if (rootHandler == nullptr)
        return nullptr;
    juce::AccessibilityHandler* focused = rootHandler->getChildFocus();
    juce::Component* component = focused != nullptr ? &focused->getComponent() : nullptr;
    return component != nullptr && (component == this || isParentOf (component)) ? component : nullptr;
}

void MainComponent::adoptAccessibilityControlTarget()
{
    // Edge-triggered: only a MOVE of the screen reader's focus is targeting. A level-triggered poll
    // re-adopted a stale focus every tick — when the router's own focus move onto the next control did
    // not stick, Tab was dragged back (the 2026-10-05 ss7 drive "stuck" on project.save).
    juce::Component* component = accessibilityFocusComponent();
    if (component == lastSeenAccessibilityFocus.getComponent())
        return;
    lastSeenAccessibilityFocus = component;
    if (component == nullptr || component == this)
        return;
    if (const auto* proxy = dynamic_cast<const PaintedAccessibleProxy*> (component))   // ADR-0066 cp2: a painted control
    {
        if (! (controlNavigator.navigating() && controlNavigator.targetId() == proxy->getModel().targetId))
            (void) adoptControlTargetId (proxy->getModel().targetId);
        return;
    }
    while (component != nullptr && component != this && ! isControlWidget (*component))
        component = component->getParentComponent();
    if (component == nullptr || component == this || component->hasKeyboardFocus (true)
        || (controlNavigator.navigating() && component == controlTargetWidget.getComponent()))
        return;

    for (const auto& control : collectShellControls())
        if (control.widget.getComponent() == component)
        {
            (void) adoptControlTargetId (control.entry.id);
            return;
        }
}

bool MainComponent::adoptControlTargetId (const std::string& id)
{
    const auto controls = collectShellControls();
    if (controlNavigator.interacting() && controlNavigator.targetId() != id)
        finishControlInteraction (true);
    if (! controlNavigator.targetById (entriesOf (controls), id))
        return false;
    controlScope = controlScopeId();
    refreshControlTargetRing (controls);
    return true;
}

void MainComponent::refreshControlTargetRing (const std::vector<ShellControl>& controls)
{
    juce::Rectangle<int> next;
    controlTargetWidget = nullptr;
    if (const ShellControl* control = findControl (controls, controlNavigator.targetId()))
    {
        next = juceRectOf (control->entry.bounds).expanded (L::controlTargetRingOutset);
        controlTargetWidget = control->widget.getComponent();
    }
    if (! controlRingArea.isEmpty())
        repaint (controlRingArea.expanded (L::controlTargetRingRepaintMargin));
    controlRingArea = next;
    if (! controlRingArea.isEmpty())
        repaint (controlRingArea.expanded (L::controlTargetRingRepaintMargin));
}

void MainComponent::paintControlTargetRing (juce::Graphics& g)
{
    if (! controlNavigator.navigating() || controlRingArea.isEmpty())
        return;
    // An interaction draws the ring heavier, so "arrows adjust this" is visible before a key is pressed.
    const float thickness = controlNavigator.interacting() ? L::controlTargetRingActiveStrokeWidth
                                                           : L::controlTargetRingStrokeWidth;
    g.setColour (yesdaw::ui::UiTheme::Color::focusRing().withAlpha (yesdaw::ui::UiTheme::Tone::focusRingAlpha));
    g.drawRoundedRectangle (controlRingArea.toFloat().reduced (thickness / 2), yesdaw::ui::UiTheme::Radius::sm, thickness);
}

void MainComponent::announceControlTarget (const ShellControl& control, bool valueOnly)
{
    if (juce::Component* widget = control.widget.getComponent())
    {
        if (! valueOnly)
            announcedPaintedElement.clear();   // the probe's: a widget speaks through its own element
        // Accessibility focus on a text field also takes the keyboard (JUCE), which would start text
        // entry on a mere Tab: the field is announced instead and takes focus on Enter.
        if (control.entry.role == ControlTargetRole::TextField)
        {
            if (! valueOnly)
                juce::AccessibilityHandler::postAnnouncement (juce::String (control.entry.name) + ", text field",
                                                             juce::AccessibilityHandler::AnnouncementPriority::medium);
            return;
        }
        if (juce::AccessibilityHandler* handler = widget->getAccessibilityHandler())
        {
            if (valueOnly)
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
            else
                handler->grabFocus();
        }
        lastSeenAccessibilityFocus = accessibilityFocusComponent();   // the router's own move is not targeting
        return;
    }
    // ADR-0066 cp2: a painted control speaks through its own accessible element (the proxy on it); the element never
    // takes the keyboard, so the shell keeps it.
    syncPaintedAccessibilityProxies();
    if (PaintedAccessibleProxy* proxy = paintedProxyFor (control.entry.id))
    {
        if (! valueOnly)
            announcedPaintedElement = control.entry.id;
        if (juce::AccessibilityHandler* handler = proxy->getAccessibilityHandler())   // live only in a window
        {
            if (valueOnly)
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
            else
                handler->grabFocus();
        }
    }
    lastSeenAccessibilityFocus = accessibilityFocusComponent();
}

juce::var MainComponent::buildProbeControlTarget()
{
    const MainComponentControlTarget target = harnessControlTarget();
    auto* object = new juce::DynamicObject();
    juce::var result (object);
    object->setProperty ("navigating", target.navigating);
    object->setProperty ("interacting", target.interacting);
    object->setProperty ("id", target.id);
    object->setProperty ("name", target.name);
    object->setProperty ("role", target.role);
    object->setProperty ("value", target.value);
    object->setProperty ("scope", target.scope);
    object->setProperty ("lastActivation", target.lastActivation);
    object->setProperty ("bounds", probeRect (target.bounds));
    object->setProperty ("count", target.count);
    {   // ADR-0066: the target's enabled state, its actions and the drawn ring; the painted elements' pool
        const auto controls = collectShellControls();
        juce::StringArray actions;
        bool enabled = false;
        if (const ShellControl* control = findControl (controls, controlNavigator.targetId()))
        {
            enabled = control->painted != nullptr || (control->widget != nullptr && control->widget->isEnabled());
            switch (control->entry.role)
            {
                case ControlTargetRole::Button:    actions.add ("press"); break;
                case ControlTargetRole::Toggle:    actions.add ("press"); actions.add ("toggle"); break;
                case ControlTargetRole::Chooser:   actions.add ("choose"); break;
                case ControlTargetRole::Value:     actions.add ("setValue"); break;
                case ControlTargetRole::TextField: actions.add ("edit"); break;
            }
            if (control->painted != nullptr && control->painted->contextMenu)
                actions.add ("showMenu");
        }
        object->setProperty ("enabled", enabled);
        object->setProperty ("actions", actions.joinIntoString (" "));
        object->setProperty ("ring", probeRect (controlRingArea));
        object->setProperty ("paintedElements", static_cast<int> (paintedProxies.size()));
        object->setProperty ("paintedElementCreations", paintedProxyCreations);
        object->setProperty ("announcedElement", juce::String (announcedPaintedElement));
    }
    return result;
}

MainComponentControlTarget MainComponent::harnessControlTarget()
{
    const auto controls = collectShellControls();
    MainComponentControlTarget target;
    target.navigating = controlNavigator.navigating();
    target.interacting = controlNavigator.interacting();
    target.scope = juce::String (controlScopeId());
    target.lastActivation = juce::String (lastControlActivation);
    target.count = static_cast<int> (controls.size());
    if (const ShellControl* control = findControl (controls, controlNavigator.targetId()))
    {
        target.id = juce::String (control->entry.id);
        target.name = juce::String (control->entry.name);
        target.role = controlTargetRoleName (control->entry.role);
        target.value = controlValueText (*control);
        target.bounds = juceRectOf (control->entry.bounds);
    }
    return target;
}

std::vector<juce::String> MainComponent::harnessControlTraversal()
{
    std::vector<juce::String> ids;
    for (const auto& control : collectShellControls())
        ids.push_back (juce::String (control.entry.id));
    return ids;
}

MainComponentControlTarget mainComponentControlTarget (juce::Component& component)
{
    if (auto* shell = dynamic_cast<MainComponent*> (&component))
        return shell->harnessControlTarget();
    return {};
}

std::vector<juce::String> mainComponentControlTraversal (juce::Component& component)
{
    if (auto* shell = dynamic_cast<MainComponent*> (&component))
        return shell->harnessControlTraversal();
    return {};
}

bool mainComponentAccessibilityTargetControl (juce::Component& component, const juce::String& id)
{
    if (auto* shell = dynamic_cast<MainComponent*> (&component))
        return shell->harnessAdoptControlTarget (id);
    return false;
}

} // namespace yesdaw::ui
