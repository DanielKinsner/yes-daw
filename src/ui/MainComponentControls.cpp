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

[[nodiscard]] bool isControlWidget (const juce::Component& component)
{
    return dynamic_cast<const juce::Button*> (&component) != nullptr
        || dynamic_cast<const juce::ComboBox*> (&component) != nullptr
        || dynamic_cast<const juce::Slider*> (&component) != nullptr
        || dynamic_cast<const juce::TextEditor*> (&component) != nullptr;
}

[[nodiscard]] ControlTargetRole controlRoleOf (const juce::Component& component)
{
    if (const auto* button = dynamic_cast<const juce::Button*> (&component))
        return button->getClickingTogglesState() ? ControlTargetRole::Toggle : ControlTargetRole::Button;
    if (dynamic_cast<const juce::ComboBox*> (&component) != nullptr)
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

    // The painted mixer faders are controls too: one per visible strip while the dock shows the mixer.
    if (scope == this && appModel.context().mixerDockVisible && ! dockShowsPianoRoll() && ! dockShowsInstrument())
    {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const std::size_t stripTotal = trackCount + surface.buses.size();
        const juce::Rectangle<int> dock = mixerPanelBounds();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const juce::Rectangle<int> rail = paintedFaderRailForLane (paintedMixerLaneBounds (i), stripIoRows (i));
            if (rail.isEmpty() || ! dock.intersects (rail))
                continue;
            const auto& strip = i < trackCount ? surface.tracks[i] : surface.buses[i - trackCount];
            ShellControl control;
            control.paintedStrip = static_cast<int> (i);
            control.entry.id = "mixer.strip." + std::to_string (i) + ".fader";
            control.entry.name = strip.name + " fader";
            control.entry.role = ControlTargetRole::Value;
            control.entry.bounds = controlRectOf (rail);
            control.entry.region = kRegionDock;
            controls.push_back (std::move (control));
        }
    }

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

juce::String MainComponent::controlValueText (const ShellControl& control) const
{
    if (control.paintedStrip >= 0)
        return dbReadoutText (mixerStripsInput.faderGainForStrip ? mixerStripsInput.faderGainForStrip (control.paintedStrip) : 1.0f);
    juce::Component* widget = control.widget.getComponent();
    if (widget == nullptr)
        return {};
    if (auto* combo = dynamic_cast<juce::ComboBox*> (widget))
    {
        if (controlNavigator.interacting() && control.entry.id == controlNavigator.targetId() && controlChooserPreview >= 0)
            return combo->getItemText (controlChooserPreview);
        return combo->getText();
    }
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
    if (control->paintedStrip >= 0)
    {
        lastControlActivation = "value:" + id;
        controlNavigator.beginInteraction (mixerStripsInput.faderGainForStrip ? mixerStripsInput.faderGainForStrip (control->paintedStrip) : 1.0f);
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
    if (control->paintedStrip >= 0)
    {
        if (! mixerStripsInput.faderGainForStrip || ! mixerStripsInput.onFaderDragged)
            return;
        const float current = mixerStripsInput.faderGainForStrip (control->paintedStrip);
        const float next = stepFaderGainDb (current, valueSteps, fine ? 0.1 : 1.0,
                                            yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax);
        if (! controlGestureOpen)
        {
            controlGestureOpen = true;
            controlGestureTarget = *control;
        }
        // The drag verb: one strip gesture (one undo step), the dB readout, the Touch / Latch ride.
        mixerStripsInput.onFaderDragged (control->paintedStrip, next, false);
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
    if (controlGestureTarget.paintedStrip >= 0)
    {
        const int strip = controlGestureTarget.paintedStrip;
        const auto surface = currentMixerSurface();
        if (static_cast<std::size_t> (strip) < surface.tracks.size() + surface.buses.size() && mixerStripsInput.onFaderDragged
            && mixerStripsInput.faderGainForStrip)
        {
            const float gain = keep ? mixerStripsInput.faderGainForStrip (strip) : static_cast<float> (origin);
            mixerStripsInput.onFaderDragged (strip, gain, true);
        }
        else
        {
            paintedFaderDragStrip = -1;
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
            hideDragDbReadout();
        }
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
// Runs every UI tick, so it must stay quiet for the router's own focus moves: announceControlTarget puts
// the screen reader on the target widget (equal to controlTargetWidget: early out), a painted target on
// the mixer surface (not a control: early out), and endControlNavigation on the shell (early out).
// Breaking any of those would make the tick re-target what the router just set.
void MainComponent::adoptAccessibilityControlTarget()
{
    juce::AccessibilityHandler* shellHandler = getAccessibilityHandler();
    if (shellHandler == nullptr)
        return;
    juce::AccessibilityHandler* focused = shellHandler->getChildFocus();
    if (focused == nullptr || focused == shellHandler)
        return;
    juce::Component* component = &focused->getComponent();
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
        return;
    }
    // A painted control speaks through the surface that paints it: its title and value become the
    // surface's, and the surface takes the screen reader's focus.
    mixerStripsInput.setTitle (juce::String (control.entry.name));
    mixerStripsInput.setDescription (controlValueText (control));
    if (juce::AccessibilityHandler* handler = mixerStripsInput.getAccessibilityHandler())
    {
        if (! valueOnly)
            handler->grabFocus();
        handler->notifyAccessibilityEvent (valueOnly ? juce::AccessibilityEvent::valueChanged
                                                     : juce::AccessibilityEvent::titleChanged);
    }
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
