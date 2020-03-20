# Mac Installer API
This document briefly goes over the installer API for the updater on macOS.

## Design
The Installer API is the integration between the app installer and the updater,
and is platform specific. The main functionality for doing the updates in the
new updater will be an update executable (.install). The update executable will
be invoked by the updater when there is an update available.

The Installer API will be called through `Installer::RunApplicationInstaller()`,
which takes `const base::FilePath&` for the path to the installer and
`const std::string& arguments` for any arguments. This will then call
`InstallFromDMG()`, which takes the same parameters. `InstallFromDMG()` then
executes `.install` with the correct arguments.

## .install

### Usage
The installer DMG will have the .install executable in the root of the volume
and the new application embedded within.

Currently the install executable takes just three arguments - an absolute path
to the DMG, an absolute path to the currently installed app, and the version of
the currently installed app. `Installer::RunApplicationInstaller()`, will append
the existence checker path (path to the installed app) and the version from the
registration into the args.

For example, `Google Chrome.dmg`, will contain `Google Chrome.app` and
`.install` executable. Here is an example of what `InstallFromDMG()` will run:

i.e.
```
./.install "/Volumes/Google Chrome.dmg" "/Applications/Google Chrome.app" \
"81.0.416.0"
```

### Exit Codes
The current constraint for exit codes for the `.install` executable is that 0 is
a successful update, and every other value is an error of some kind. For
documentation on the existing exit codes for the implemented .install
executable, please refer to `//chrome/updater/mac/setup/.install.sh`.

### Non-executable Error Codes
When executing `InstallFromDMG()`, there can also be cases in which the
Installer API fails before the install executable is executed. These are
translated from the enum `updater::InstallErrors`. Please refer to
`//chrome/updater/mac/install.h` for documentation on these error codes.
