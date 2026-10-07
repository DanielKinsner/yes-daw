// YES DAW — the shell's project lifecycle half (G5.5 / ADR-0060): New (the dialog, refusing an occupied target),
// unsaved changes before New and Open, and the audio device asked to run at the project's rate.

#include "ui/MainComponentShell.h"

#include <cmath>

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

void MainComponent::initialiseNewProjectDialog()
{
    newProjectDialog.onCreate = [this] (UiNewProjectChoices choices) {
        newProjectDialog.setVisible (false);
        createNewProject (choices, true);
        refreshActionState();
        repaintAll();
    };
    newProjectDialog.onCancel = [this] {
        newProjectDialog.setVisible (false);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (newProjectDialog);
}

// File > New: unsaved changes first, then the dialog (or its seam), then the location.
void MainComponent::beginNewProject()
{
    if (! confirmReplaceProject())
        return;
    if (fileChoices.newProjectDialogChoices)
    {
        if (const std::optional<UiNewProjectChoices> choices = fileChoices.newProjectDialogChoices())
            createNewProject (*choices, true);
        return;
    }
    if (fileChoices.newProjectDialog)
    {
        newProjectDialog.show (appModel.newProjectChoices(), appModel.listTemplates());
        resized();
        refreshActionState();
        repaintAll();
        return;
    }
    createNewProject (appModel.newProjectChoices(), false);   // the harness's default: at once, as before
}

// `chosenInDialog`: the dialog's rate, tempo and meter win over an injected project's (the harness's immediate
// path keeps an injected project exactly as injected). A template (ADR-0060 cp3) is read before the location is
// asked for, and arrives with fresh identities.
void MainComponent::createNewProject (const UiNewProjectChoices& choices, bool chosenInDialog)
{
    newProjectDialog.setVisible (false);
    if (! UiAppModel::newProjectChoicesValid (choices))
    {
        appModel.reportStatus ("New project refused: the sample rate, tempo or meter is out of range", true);
        return;
    }
    engine::Project project = UiAppModel::makeDefaultSessionProject();
    if (fileChoices.makeNewProject)
    {
        project = fileChoices.makeNewProject();
    }
    else if (! choices.templateName.empty())
    {
        std::string reason;
        const std::optional<engine::Project> layout = appModel.loadTemplateLayout (choices.templateName, reason);
        if (! layout.has_value())
        {
            appModel.reportStatus ("New project refused: the template " + choices.templateName + " cannot be used (" + reason + ")", true);
            return;
        }
        project = UiAppModel::instantiateTemplate (*layout);
    }
    if (chosenInDialog || ! fileChoices.makeNewProject)
        UiAppModel::applyNewProjectChoices (project, choices);
    if (! fileChoices.chooseNewProjectBundle)
        return;
    const std::filesystem::path path = fileChoices.chooseNewProjectBundle();
    if (path.empty())
        return;
    std::error_code error;
    if (std::filesystem::exists (path, error))   // ADR-0060: never open or overwrite a project already there
    {
        appModel.reportStatus ("New project refused: a project already exists at " + yesdaw::io::utf8Text (path.filename()), true);
        return;
    }
    // R4: a failed create paints its reason instead of vanishing; what it wrote goes, the current project stays.
    const yesdaw::persistence::BundleResult created = appModel.createProjectBundle (path, std::move (project));
    if (! created.ok())
    {
        std::error_code removed;
        std::filesystem::remove_all (path, removed);
        appModel.reportStatus ("New project failed: " + created.message, true);
        return;
    }
    appModel.setNewProjectChoices (choices);
    afterProjectAttached();
}

// Before New or Open replaces a project with edits since its last Save: Save / Don't Save / Cancel (the hook quitting
// uses, or a native box in the native shell). Every edit is already in the bundle; Save marks this state as saved.
bool MainComponent::confirmReplaceProject()
{
    if (! appModel.context().projectLoaded || ! appModel.hasUnsavedChanges())
        return true;
    int choice = kCloseChoiceCancel;
    if (fileChoices.confirmCloseUnsavedChanges)
    {
        choice = fileChoices.confirmCloseUnsavedChanges();
    }
    else if (fileChoices.nativePrompts)
    {
        const int native = juce::AlertWindow::showYesNoCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Unsaved changes",
            "Save this state as your saved version first?\n(Every edit is already stored in the project bundle.)",
            "Save",
            "Don't Save",
            "Cancel");
        choice = native == 1 ? kCloseChoiceSave : native == 2 ? kCloseChoiceClose : kCloseChoiceCancel;
    }
    else
    {
        return true;   // the harness's default: as before
    }
    if (choice == kCloseChoiceSave)
        return saveCurrentProject (false);   // the untitled session goes through Save As; a cancelled name cancels
    return choice == kCloseChoiceClose;
}

void MainComponent::afterProjectAttached()
{
    requestProjectDeviceRate();
}

// ADR-0060: the audio device runs at the project's rate when it can (a device reopen, never the engine's in-place
// hot-swap); when it cannot, the device is put back as it was and the warning names both rates.
void MainComponent::requestProjectDeviceRate()
{
    if (! appModel.context().projectLoaded)
        return;
    const auto sameRate = [] (double a, double b) { return std::abs (a - b) < 0.5; };
    const double projectHz = appModel.project().sampleRate.hz;
    const double openHz = deviceSampleRateHz.load (std::memory_order_relaxed);
    const double deviceHz = fileChoices.currentAudioDeviceSampleRate ? fileChoices.currentAudioDeviceSampleRate()
                          : openHz > 0.0                              ? openHz
                                                                      : appModel.adoptedDeviceSampleRateHz();
    if (deviceHz <= 0.0 || sameRate (deviceHz, projectHz))
        return;
    ++deviceRateRequests;
    bool accepted = false;
    if (fileChoices.requestAudioDeviceSampleRate)
    {
        accepted = fileChoices.requestAudioDeviceSampleRate (projectHz);
        if (accepted)
            appModel.noteDeviceSampleRate (projectHz);
    }
    else if (desktopAudioCallbackRegistered)
    {
        juce::AudioIODevice* const device = audioDeviceManager.getCurrentAudioDevice();
        bool offered = false;
        if (device != nullptr)
            for (const double rate : device->getAvailableSampleRates())
                offered = offered || sameRate (rate, projectHz);
        if (offered)
        {
            suspendDesktopAudioCallback();
            const juce::AudioDeviceManager::AudioDeviceSetup previous = audioDeviceManager.getAudioDeviceSetup();
            juce::AudioDeviceManager::AudioDeviceSetup setup = previous;
            setup.sampleRate = projectHz;
            const juce::String error = audioDeviceManager.setAudioDeviceSetup (setup, true);
            juce::AudioIODevice* const reopened = audioDeviceManager.getCurrentAudioDevice();
            accepted = error.isEmpty() && reopened != nullptr && sameRate (reopened->getCurrentSampleRate(), projectHz);
            if (! accepted)   // a refused reopen never leaves the shell without its device or its callback
                (void) audioDeviceManager.setAudioDeviceSetup (previous, true);
            resumeDesktopAudioCallback();
        }
    }
    if (! accepted)
        appModel.reportStatus ("Audio device runs at " + std::to_string (static_cast<long long> (deviceHz)) + " Hz but this project is "
                                   + std::to_string (static_cast<long long> (projectHz)) + " Hz - playback speed will be wrong",
                               true);
}

// File > Save as Template (ADR-0060): a name, the replace question when that name is taken, then the layout.
void MainComponent::saveProjectAsTemplate()
{
    if (! fileChoices.chooseSaveAsTemplateName)
        return;
    const std::string name = fileChoices.chooseSaveAsTemplateName();
    if (name.empty())
        return;
    const bool taken = appModel.templateExists (name);
    if (taken && ! (fileChoices.confirmReplaceTemplate && fileChoices.confirmReplaceTemplate (appModel.templateListedName (name))))
        return;
    const UiActionDispatchResult saved = appModel.saveProjectAsTemplate (name, taken);
    if (! saved.dispatched)
        appModel.reportStatus (std::string ("Save as Template failed: ") + saved.state.disabledReason, true);
}

// File > Save a Copy (ADR-0060): the copy is written and closed; this project stays current, saved or not.
void MainComponent::saveProjectCopy()
{
    if (! fileChoices.chooseSaveACopyProjectBundle)
        return;
    const std::filesystem::path path = fileChoices.chooseSaveACopyProjectBundle();
    if (path.empty())
        return;
    const UiActionDispatchResult copied = appModel.saveProjectBundleCopy (path);
    if (! copied.dispatched)
        appModel.reportStatus (std::string ("Save a Copy failed: ") + copied.state.disabledReason, true);
}

} // namespace yesdaw::ui
