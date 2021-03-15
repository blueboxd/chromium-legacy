\[ en | [ja](README.ja.md) \]

# Chromium-legacy

Chromium-legacy is the latest Chromium (almost equivalent to Chrome Canary without Google branding) patched & built for legacy Mac OS X series, not supported officially:

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite

**NB: Not for natively supported environments (10.11+).**

This project is automatically built and uploaded twice a day (00:00 and 12:00 JST) if no issues occurred.
And note, uploaded **without any tests**, thus there is no guarantee for launching or proper operation.
It's recommended to find and use a stable build for daily use. (The same is true for the original Chrome Canary/Chromium)

## functions

Basically equivalent to the [same version of original Chromium](https://chromestatus.com/features) except for limitations by old OSes (see below).

## limitations / glitches

- UI
  - unified window title & tab bar
    - not available on 10.7/10.8/10.9, replaced by classical title bar instead (thanks to [@Wowfunhappy](https://github.com/Wowfunhappy))
  - menus/sheets
    - have no shadow
  - scrollbars
    - won't disappear despite "Show scrollbars when scrolling" option is enabled when GPU compositing is disabled
- GPU assists
  - on 10.7, due to old OpenGL version, disabled entirely by Chromium itself
  - on 10.8/10.9, GPU compositing is disabled by hardcoded `--disable-gpu-compositing` option due to rendering glitches.
- WebAuthn/FIDO2
  - on 10.7, you need patch for `IOHIDFamily.kext` to use USB keys (TBW)

## building

Build steps are almost the same as the [original Chromium's one](docs/mac_build_instructions.md) except for compatibility patches.

### prerequisites

- macOS 11.1 SDK
  - to build for 10.7, you need patch for [NSArray.h](https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/NSArray.h) and [NSDictionary.h](https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/NSDictionary.h)
- Xcode 12.2+
- powerful CPUs
  - about 40mins to full build with `Xeon E5-2690 v4` & 2 x `Ryzen 9 3950X`
  - about 3hrs+ to full build with `Core i9-9980HK`

### TL;DR

```bash
curl https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/167ff995dca72a6fd329b6aece4c3c645ac77e7c/build.sh | bash
```

### steps

first setup & build:

```bash
# setup depot_tools
mkdir chromium-project && cd chromium-project
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=`pwd`/depot_tools:"$PATH"

# setup project dir
mkdir chromium-legacy && cd chromium-legacy
# place this repo as src
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/.gclient
# clone sources
gclient sync -v --no-history

# setup patched skia
cd src/third_party/skia
git remote add for-lion https://github.com/blueboxd/skia.git
git fetch for-lion && git checkout for-lion && git checkout for-lion -- .

# setup patch for v8
cd ../../v8/src
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/sp_mut.cc
cd ../
cat BUILD.gn | sed -e 's#"src/wasm/wasm-code-manager.h",#"src/wasm/wasm-code-manager.h",\'$'\n    "src/sp_mut.cc",#g' > BUILD.gn.tmp && mv -fv BUILD.gn.tmp BUILD.gn

# setup patch for libANGLE
cd ../third_party/angle/src/libANGLE/renderer/
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/65ba4558a17eb47feb38729a87b8d8976d5bb8ad/driver_utils_mac.mm
cd metal
cat BUILD.gn| sed -e 's#"QuartzCore.framework",#"QuartzCore.framework",\'$'\n        "CoreServices.framework",#g'  > BUILD.gn.tmp && mv -fv BUILD.gn.tmp BUILD.gn

# setup out dir
cd ../../../../../../../
mkdir -p out/release && cd out/release
# setup args.gn with basic parameters
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/args.gn
cd ../../src
gn gen ../out/release

# build
ninja -C ../out/release chrome

# now your build is ready
open -R ../out/release/Chromium.app
```

to update `src`:

```bash
cd chromium-project/chromium-legacy/src
git pull
cd thrid_party/skia
git checkout origin/master -- .
cd ../../
gclient sync -D
cd thrid_party/skia
git fetch for-lion && git checkout for-lion && git checkout for-lion -- .
```

to build:

```bash
cd chromium-project/chromium-legacy/src
ninja -C ../out/release chrome
```

---

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory Structure
](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.
