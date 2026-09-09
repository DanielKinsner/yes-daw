// YES DAW - preserve JUCE's default pair while avoiding duplicate capability probes.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace yesdaw::ui {

struct AudioDeviceNames
{
    std::vector<std::string> inputs, outputs;
};

// Each backend scan discovers both directions. Read them from the same snapshot,
// so an endpoint change cannot split one chooser refresh across two generations.
template <typename DeviceManager>
AudioDeviceNames enumerateDesktopAudioDeviceNames (DeviceManager& manager)
{
    AudioDeviceNames names;
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames (true))
            names.inputs.push_back (name.toStdString());
        for (const auto& name : type->getDeviceNames (false))
            names.outputs.push_back (name.toStdString());
    }
    return names;
}

struct DefaultAudioPairProof
{
    juce::String inputName, outputName;
    unsigned long inputMixRate = 0, outputMixRate = 0;
};

// JUCE seeds each shared WASAPI endpoint's supported rates with its mix rate. Equal
// mix rates prove its normal pair search would accept these exact defaults first.
inline std::optional<juce::XmlElement> verifiedDefaultAudioSetup (
    const juce::String& backend, const juce::String& input, const juce::String& output,
    const std::optional<DefaultAudioPairProof>& proof)
{
    if (backend != "Windows Audio" || input.isEmpty() || output.isEmpty() || ! proof
        || input != proof->inputName || output != proof->outputName
        || proof->inputMixRate == 0 || proof->inputMixRate != proof->outputMixRate)
        return std::nullopt;

    juce::XmlElement setup ("DEVICESETUP");
    setup.setAttribute ("deviceType", backend);
    setup.setAttribute ("audioInputDeviceName", input);
    setup.setAttribute ("audioOutputDeviceName", output);
    // Omit rate, buffer and channel attributes: JUCE retains its normal automatic
    // rate/buffer choice and initialise(2,2)'s default channel-layout behavior.
    return setup;
}

std::optional<DefaultAudioPairProof> queryDefaultAudioPairProof();

// Keep the real open/fallback sequence testable without opening the machine's audio devices.
// Production instantiates this directly with JUCE's AudioDeviceManager.
template <typename DeviceManager>
juce::String initialiseDesktopAudioWithSetup (DeviceManager& manager,
                                             const juce::XmlElement* verifiedSetup)
{
    if (verifiedSetup != nullptr)
    {
        const auto error = manager.initialise (2, 2, verifiedSetup, false);
        if (error.isEmpty() && manager.getCurrentAudioDevice() != nullptr)
            return error;
    }

    // Missing proof or a failed real open retains the full compatible-pair search
    // and output-only recovery, including JUCE's success-without-a-device case.
    auto error = manager.initialiseWithDefaultDevices (2, 2);
    if (! error.isEmpty() || manager.getCurrentAudioDevice() == nullptr)
        error = manager.initialiseWithDefaultDevices (0, 2);
    return error;
}

inline juce::String initialiseDesktopAudio (
    juce::AudioDeviceManager& manager,
    const std::function<void (const char*)>& recordStage = {})
{
    // Enumeration chooses the same current backend as JUCE initialise().
    (void) manager.getAvailableDeviceTypes();
    std::optional<juce::XmlElement> setup;
    if (auto* type = manager.getCurrentDeviceTypeObject();
        type != nullptr && type->getTypeName() == "Windows Audio")
    {
        const auto inputs = type->getDeviceNames (true);
        const auto outputs = type->getDeviceNames (false);
        const auto inputIndex = type->getDefaultDeviceIndex (true);
        const auto outputIndex = type->getDefaultDeviceIndex (false);
        if (juce::isPositiveAndBelow (inputIndex, inputs.size())
            && juce::isPositiveAndBelow (outputIndex, outputs.size()))
            setup = verifiedDefaultAudioSetup (
                type->getTypeName(), inputs[inputIndex], outputs[outputIndex], queryDefaultAudioPairProof());
        if (recordStage)
            recordStage (setup ? "prove-default-audio-pair" : "default-audio-pair-unproven");
    }

    return initialiseDesktopAudioWithSetup (manager, setup ? &*setup : nullptr);
}

} // namespace yesdaw::ui
