Chromium for arm Macs
=====================

Apple is planning on selling Macs with arm chips by the end of 2020.
This document describes the state of native binaries for these Macs.

Building _for_ arm Macs
-----------------------

We're currently bringing up the build. At the moment, everything compiles
in a vanilla release build (we haven't tried anything else) and Chromium
starts up fine and seems to work. We haven't tried running tests yet.

You can follow the [Mac-ARM64 label](https://crbug.com/?q=label%3Amac-arm64) to
get updates on progress.

There's a [bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel)
that builds for arm that you can look at to get an idea of the current state. It
cross-builds on an Intel machine, and we don't have enough hardware to
continuously run tests.

As an alternative to building locally, changes can be submitted to the opt-in
[mac-arm64-rel
trybot](https://ci.chromium.org/p/chromium/builders/try/mac-arm64-rel). A small
number of [swarming bots](https://goto.corp.google.com/run-on-dtk) are also
available for Googlers to run tests on.

To build for arm64, you have to do 2 things:

1. use the `MacOSX11.0.sdk` that comes with
   Xcode 12 beta. If you're on Google's corporate network, edit your `.gclient`
   file and add this `custom_vars`:

       "custom_vars": { "mac_xcode_version": "xcode_12_beta" },

   Then just `gclient sync` and you'll automatically get that SDK and will build
   with it.

   Otherwise, manually download and install the current Xcode 12 beta and make
   it the active Xcode with `xcode-select`.

2. Add `target_cpu = "arm64"` to your `args.gn`.

Then build normally.

Building _on_ arm Macs
-----------------------

Building _on_ arm Macs means that all the tools we use to build chrome need
to either work under Rosetta or have a native arm binary.

We think it makes sense to use arch-specific binaries for stuff that's
downloaded in a hook (things pulled from cipd and elsewhere in gclient hooks --
I think this includes clang, gn, clang-format, python3 in depot\_tools, ...),
and fat binaries for things where we'd end up downloading both binaries anyways
(mostly ninja-mac in depot\_tools). There's a
[tracking bug](https://crbug.com/1103236) for eventually making native arm
binaries available for everything.

Go does [not yet](https://github.com/golang/go/issues/38485) support building
binaries for arm macs, so all our go tooling needs to run under Rosetta for
now.

`cipd` defaults to downloading Intel binaries on arm macs for now, so that
they can run under Rosetta.

If a binary runs under Rosetta, the subprocesses it spawns by default also
run under rosetta, even if they have a native slice. The `arch` tool
can be used to prevent this ([example cl](https://chromium-review.googlesource.com/c/chromium/tools/depot_tools/+/2287751)),
which can be useful to work around Rosetta bugs.

As of today, it's possible to install depot\_tools and then run
`fetch chromium`, and it will download Chromium and its dependencies,
but it will die in `runhooks`.

`ninja`, `gn`, and `gomacc` all work fine under Rosetta.
