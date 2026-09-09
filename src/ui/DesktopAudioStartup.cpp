// YES DAW - native capability evidence, kept separate from shell headers.
#include "ui/DesktopAudioStartup.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mmdeviceapi.h>
 #include <audioclient.h>
 #include <propsys.h>
 #include <wrl/client.h>
 #include <initguid.h>
 #include <functiondiscoverykeys_devpkey.h>
#endif

namespace yesdaw::ui {

std::optional<DefaultAudioPairProof> queryDefaultAudioPairProof()
{
#if JUCE_WINDOWS
    using Microsoft::WRL::ComPtr;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS (enumerator.GetAddressOf()))))
        return std::nullopt;

    DefaultAudioPairProof proof;
    const auto query = [&] (EDataFlow flow, juce::String& name, unsigned long& rate) {
        ComPtr<IMMDevice> endpoint;
        // This is the role used by JUCE's WASAPI default endpoint enumeration.
        if (FAILED (enumerator->GetDefaultAudioEndpoint (flow, eMultimedia, endpoint.GetAddressOf())))
            return false;
        ComPtr<IPropertyStore> properties;
        if (FAILED (endpoint->OpenPropertyStore (STGM_READ, properties.GetAddressOf())))
            return false;
        PROPVARIANT value {};
        const auto nameResult = properties->GetValue (PKEY_Device_FriendlyName, &value);
        if (SUCCEEDED (nameResult) && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
            name = juce::String (value.pwszVal);
        PropVariantClear (&value);
        if (name.isEmpty())
            return false;
        ComPtr<IAudioClient> client;
        if (FAILED (endpoint->Activate (__uuidof (IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                                       reinterpret_cast<void**> (client.GetAddressOf()))))
            return false;
        WAVEFORMATEX* format = nullptr;
        const auto result = client->GetMixFormat (&format);
        if (SUCCEEDED (result) && format != nullptr)
            rate = format->nSamplesPerSec;
        CoTaskMemFree (format);
        return rate > 0;
    };
    if (query (eCapture, proof.inputName, proof.inputMixRate)
        && query (eRender, proof.outputName, proof.outputMixRate))
        return proof;
#endif
    return std::nullopt;
}

} // namespace yesdaw::ui
