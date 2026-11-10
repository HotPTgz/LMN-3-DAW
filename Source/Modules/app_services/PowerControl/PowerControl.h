#pragma once

namespace app_services {

/**
 * Shuts the computer down cleanly.
 *
 * Tries, in order:
 *  - the command in the LMN3_POWEROFF_COMMAND environment variable, if set
 *  - "systemctl poweroff" (allowed for the logged-in desktop user)
 *  - "sudo -n poweroff" (works if the user has password-less sudo, the
 *    Raspberry Pi OS default)
 */
namespace PowerControl {

// Returns true once a shutdown command has been accepted
bool powerOff();

// The commands that powerOff() will try, in order
juce::StringArray getPowerOffCommands();

} // namespace PowerControl
} // namespace app_services
