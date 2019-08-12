# Clang

[Clang](http://clang.llvm.org/) is the main supported compiler when
building Chromium on all platforms.

Known [clang bugs and feature
requests](http://code.google.com/p/chromium/issues/list?q=label:clang).

[TOC]

## Building with clang

This happens by default, with clang binaries being fetched by gclient
during the `gclient runhooks` phase. To fetch them manually, or build
a local custom clang, use

    tools/clang/scripts/update.py

Run `gn args` and make sure there is no `is_clang = false` in your args.gn file.

Build: `ninja -C out/gn chrome`

## Reverting to gcc on Linux or MSVC on Windows

There are no bots that test this but `is_clang = false` will revert to
gcc on Linux and to Visual Studio on Windows. There is no guarantee it
will work.

## Mailing List

https://groups.google.com/a/chromium.org/group/clang/topics

## Using plugins

The
[chromium style plugin](https://dev.chromium.org/developers/coding-style/chromium-style-checker-errors)
is used by default when clang is used.

If you're working on the plugin, you can build it locally like so:

1.  Run `./tools/clang/scripts/build.py --without-android`
    to build the plugin.
1.  Run `ninja -C third_party/llvm-build/Release+Asserts/` to build incrementally.
1.  Build with clang like described above, but, if you use goma, disable it.

To test the FindBadConstructs plugin, run:

    (cd tools/clang/plugins/tests && \
     ./test.py ../../../../third_party/llvm-build/Release+Asserts/bin/clang \
               ../../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so)

Since the plugin is rolled with clang changes, behavior changes to the plugin
should be guarded by flags to make it easy to roll clang. A general outline:
1.  Implement new plugin behavior behind a flag.
1.  Wait for a compiler roll to bring in the flag.
1.  Start passing the new flag in `GN` and verify the new behavior.
1.  Enable the new plugin behavior unconditionally and update the plugin to
    ignore the flag.
1.  Wait for another compiler roll.
1.  Stop passing the flag from `GN`.
1.  Remove the flag completely.

## Using the clang static analyzer

See [clang_static_analyzer.md](clang_static_analyzer.md).

## Windows

Since October 2017, clang is the default compiler on Windows. It uses
MSVC's linker and SDK, so you still need to have Visual Studio with
C++ support installed.

To use MSVC's compiler (if it still works), use `is_clang = false`.

Current brokenness:

*   To get colored diagnostics, you need to be running
    [ansicon](https://github.com/adoxa/ansicon/releases).

## Using a custom clang binary

Set `clang_base_path` in your args.gn to the llvm build directory containing
`bin/clang` (i.e. the directory you ran cmake). This [must][1] be an absolute
path. You also need to disable chromium's clang plugin.

Here's an example that also disables debug info and enables the component build
(both not strictly necessary, but they will speed up your build):

```
clang_base_path = getenv("HOME") + "/src/llvm-build"
clang_use_chrome_plugins = false
is_debug = false
symbol_level = 1
is_component_build = true
```

You can then run `head out/gn/toolchain.ninja` and check that the first to
lines set `cc` and `cxx` to your clang binary. If things look good, run `ninja
-C out/gn` to build.

Chromium tries to be buildable with its currently pinned clang, and with clang
trunk. Set `llvm_force_head_revision = true` in your args.gn if the clang you're
trying to build with is closer to clang trunk than to Chromium's pinned clang
(which `tools/clang/scripts/update.py --print-revision` prints).
