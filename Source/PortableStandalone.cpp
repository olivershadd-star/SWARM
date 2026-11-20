#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
class PortableSwarmApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void anotherInstanceStarted (const juce::String&) override
    {
        if (window != nullptr)
            window->toFront (true);
    }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto settingsFile = executable.getSiblingFile ("SWARM.settings");
        settings = std::make_unique<juce::PropertiesFile> (settingsFile, options);

        const auto colour = juce::LookAndFeel::getDefaultLookAndFeel()
                                .findColour (juce::ResizableWindow::backgroundColourId);
        window = std::make_unique<juce::StandaloneFilterWindow> (
            getApplicationName(), colour, settings.get(), false);
        window->setVisible (true);
    }

    void shutdown() override
    {
        window.reset();
        if (settings != nullptr)
            settings->saveIfNeeded();
        settings.reset();
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
            return;
        }

        if (window != nullptr)
            window->closeButtonPressed();
        else
            quit();
    }

private:
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
}

JUCE_CREATE_APPLICATION_DEFINE (PortableSwarmApplication)
