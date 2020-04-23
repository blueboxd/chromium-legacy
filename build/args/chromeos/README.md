This directory is used to store GN arg mapping for Chrome OS boards.

Files in this directory are populated by running `gclient sync` with specific
arguments set in the .gclient file. Specifically:
* The file must have a top-level variable set: `target_os = ["chromeos"]`
* The `"custom_vars"` parameter of the chromium/src.git solution must include the
  parameter: `"cros_board": "{BOARD_NAME}"`

A typical .gclient file is a sibling of the src/ directory, and might look like
this:
```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars" : {
        "checkout_src_internal": True,
        "cros_board": "eve",
    },
  },
]
target_os = ["chromeos"]
```

To use these files in a build, simply add the following line to your GN args:
```
import("//build/args/chromeos/${some_board}.gni")
```

That will produce a Chrome OS build of Chrome very similar to what is shipped
for that device. You can also supply additional args or even overwrite ones
supplied in the .gni file after the `import()` line. For example, the following
args will produce a debug build of Chrome for board=eve using goma:
```
import("//build/args/chromeos/eve.gni")

is_debug = true
use_goma = true
goma_dir = "/path/to/goma/"
```

TODO(bpastene): Add list support to gclient and allow multiple boards to be
specified in the .gclient file.
