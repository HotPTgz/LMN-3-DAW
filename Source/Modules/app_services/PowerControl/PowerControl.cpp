#include "PowerControl.h"

namespace app_services {
namespace PowerControl {

juce::StringArray getPowerOffCommands() {
    juce::StringArray commands;
    auto custom =
        juce::SystemStats::getEnvironmentVariable("LMN3_POWEROFF_COMMAND", {});
    if (custom.isNotEmpty())
        commands.add(custom);
    commands.add("systemctl poweroff");
    commands.add("sudo -n poweroff");
    return commands;
}

bool powerOff() {
    for (auto command : getPowerOffCommands()) {
        juce::Logger::writeToLog("Power off: trying \"" + command + "\"");
        juce::ChildProcess process;
        if (!process.start(command))
            continue;

        if (!process.waitForProcessToFinish(10000)) {
            // still running: the shutdown is under way
            return true;
        }

        if (process.getExitCode() == 0)
            return true;

        juce::Logger::writeToLog("Power off failed: " +
                                 process.readAllProcessOutput());
    }
    return false;
}

} // namespace PowerControl
} // namespace app_services
