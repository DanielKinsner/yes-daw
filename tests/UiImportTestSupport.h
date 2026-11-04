#pragma once

#include "ui/MainComponent.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

// Import fixtures wait at the action that requested media; ordinary button helpers never drain jobs.
inline void waitForAudioImports (juce::Component& shell)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (20);
    do
    {
        yesdaw::ui::mainComponentServiceUiTick (shell);
        if (! yesdaw::ui::snapshotMainComponent (shell).context.audioImportPending)
            break;
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    while (std::chrono::steady_clock::now() < deadline);
    REQUIRE_FALSE (yesdaw::ui::snapshotMainComponent (shell).context.audioImportPending);
}
