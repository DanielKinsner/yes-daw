// YES DAW — a painted control's accessible element (ADR-0066 cp2).
//
// The shell paints most of its controls (the rail's cells and knobs, the mixer's strips, the tool strip, the header's
// gear and readout, the Sampler's pads): one component draws many controls, so a screen reader saw one element per
// surface. A proxy stands on each painted control: it paints nothing, takes no mouse press (the surface under it keeps
// every gesture) and never takes the keyboard (G0.2: native widgets never hold keyboard focus; JUCE gives an
// accessibility focus move the keyboard only when the component wants it), and carries no component id (the router's
// widget walk and the layout gates skip it). Its handler reports the painted control's role, name, value and state and
// runs its actions through the same effects the keyboard's Enter and the mouse use.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace yesdaw::ui {

class PaintedAccessibleProxy final : public juce::Component
{
public:
    enum class Role : std::uint8_t
    {
        Button,
        Toggle,
        Value
    };

    struct Model
    {
        std::string targetId;                        // the Control target this element stands for
        Role role = Role::Button;
        std::function<bool()> checked;               // a Toggle's state
        std::function<juce::String()> valueText;     // what the paint reads
        std::function<double()> currentValue;        // a Value's number
        std::function<void (double)> setValue;       // a Value set from outside: one step
        double minimum = 0.0, maximum = 1.0;
        std::function<void()> press;                 // a Button's click, a Toggle's flip
        std::function<void()> showMenu;              // the right-click menu, where one exists
    };

    PaintedAccessibleProxy()
    {
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        setOpaque (false);
    }

    void setModel (Model next)
    {
        const bool changedShape = next.role != model.role || static_cast<bool> (next.showMenu) != static_cast<bool> (model.showMenu);
        model = std::move (next);
        if (changedShape)
            invalidateAccessibilityHandler();   // a handler's role and actions are fixed when it is made
    }
    [[nodiscard]] const Model& getModel() const noexcept { return model; }

    void paint (juce::Graphics&) override {}

    // What the element's handler reports, from the same functions the handler uses — checkable without a native window
    // (a handler made outside one creates a platform element of its own).
    struct Description
    {
        juce::AccessibilityRole role = juce::AccessibilityRole::button;
        juce::String title, value;
        bool readOnly = true, ranged = false, checkable = false, checked = false;
        bool canPress = false, canToggle = false, canShowMenu = false;
    };
    [[nodiscard]] Description describe() const
    {
        Description d;
        d.role = roleOf (model.role);
        d.title = getTitle();
        d.value = model.valueText ? model.valueText() : juce::String();
        d.readOnly = model.role != Role::Value || ! static_cast<bool> (model.setValue);
        d.ranged = model.role == Role::Value && model.maximum > model.minimum;
        d.checkable = model.role == Role::Toggle;
        d.checked = d.checkable && model.checked && model.checked();
        d.canPress = model.role != Role::Value;
        d.canToggle = model.role == Role::Toggle;
        d.canShowMenu = static_cast<bool> (model.showMenu);
        return d;
    }
    // The handler's actions, run directly (the press / toggle a screen reader invokes; a Value set from outside). Each runs
    // a COPY of its function: the effect refreshes the shell, which hands this element a new model mid-call, and the
    // copy keeps the running closure (and the record it holds) alive until it returns.
    void invokePress() const
    {
        if (model.role == Role::Value || ! model.press)
            return;
        const auto press = model.press;
        press();
    }
    void invokeSetValue (double value) const
    {
        if (model.role != Role::Value || ! model.setValue)
            return;
        const auto set = model.setValue;
        set (juce::jlimit (model.minimum, model.maximum, value));
    }
    void invokeShowMenu() const
    {
        if (! model.showMenu)
            return;
        const auto show = model.showMenu;
        show();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<Handler> (*this);
    }

private:
    static juce::AccessibilityRole roleOf (Role role) noexcept
    {
        switch (role)
        {
            case Role::Toggle: return juce::AccessibilityRole::toggleButton;
            case Role::Value:  return juce::AccessibilityRole::slider;
            case Role::Button: break;
        }
        return juce::AccessibilityRole::button;
    }

    class Value final : public juce::AccessibilityValueInterface
    {
    public:
        explicit Value (PaintedAccessibleProxy& owner) : proxy (owner) {}
        bool isReadOnly() const override { return ! static_cast<bool> (proxy.model.setValue); }
        double getCurrentValue() const override { return proxy.model.currentValue ? proxy.model.currentValue() : 0.0; }
        juce::String getCurrentValueAsString() const override
        {
            return proxy.model.valueText ? proxy.model.valueText() : juce::String (getCurrentValue());
        }
        void setValue (double newValue) override { proxy.invokeSetValue (newValue); }
        void setValueAsString (const juce::String& newValue) override { setValue (newValue.getDoubleValue()); }
        AccessibleValueRange getRange() const override
        {
            if (proxy.model.role != Role::Value || ! (proxy.model.maximum > proxy.model.minimum))
                return {};
            return { { proxy.model.minimum, proxy.model.maximum }, (proxy.model.maximum - proxy.model.minimum) / 100.0 };
        }

    private:
        PaintedAccessibleProxy& proxy;
    };

    // A read-only value: a Button's or Toggle's readout ("bars", "on").
    class Readout final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit Readout (PaintedAccessibleProxy& owner) : proxy (owner) {}
        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            return proxy.model.valueText ? proxy.model.valueText() : juce::String();
        }
        void setValueAsString (const juce::String&) override {}

    private:
        PaintedAccessibleProxy& proxy;
    };

    class Handler final : public juce::AccessibilityHandler
    {
    public:
        explicit Handler (PaintedAccessibleProxy& owner)
            : AccessibilityHandler (owner, PaintedAccessibleProxy::roleOf (owner.model.role), actionsOf (owner), interfacesOf (owner)),
              proxy (owner)
        {
        }

        juce::AccessibleState getCurrentState() const override
        {
            juce::AccessibleState state = AccessibilityHandler::getCurrentState();
            if (proxy.model.role == Role::Toggle)
            {
                state = state.withCheckable();
                if (proxy.model.checked && proxy.model.checked())
                    state = state.withChecked();
            }
            return state;
        }

    private:
        static juce::AccessibilityActions actionsOf (PaintedAccessibleProxy& owner)
        {
            juce::AccessibilityActions actions;
            if (owner.model.role != Role::Value)
            {
                actions.addAction (juce::AccessibilityActionType::press, [&owner] { owner.invokePress(); });
                if (owner.model.role == Role::Toggle)
                    actions.addAction (juce::AccessibilityActionType::toggle, [&owner] { owner.invokePress(); });
            }
            if (owner.model.showMenu)
                actions.addAction (juce::AccessibilityActionType::showMenu, [&owner] { owner.invokeShowMenu(); });
            return actions;
        }

        static Interfaces interfacesOf (PaintedAccessibleProxy& owner)
        {
            if (owner.model.role == Role::Value)
                return Interfaces { std::unique_ptr<juce::AccessibilityValueInterface> (std::make_unique<Value> (owner)) };
            return Interfaces { std::unique_ptr<juce::AccessibilityValueInterface> (std::make_unique<Readout> (owner)) };
        }

        PaintedAccessibleProxy& proxy;
    };

    Model model;
};

} // namespace yesdaw::ui
