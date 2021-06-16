\[ en | [ja](README.ja.md) \]

# Chromium-legacy

Chromium-legacy is the latest Chromium (almost equivalent to Chrome Canary without Google branding) patched & built for legacy Mac OS X series, not supported officially:

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite

**NB: Not for officially supported environments (10.11+).**

This project is automatically built and uploaded twice a day (00:00 and 12:00 JST) if no issues occurred.
And note, uploaded **without any tests**, thus there is no guarantee for launching or proper operation.
It's recommended to find and use a stable build for daily use. (The same is true for the original Chrome Canary/Chromium)

## features

Basically equivalent to the [same version of original Chromium](https://chromestatus.com/features) except for limitations by old OSes (see below).

## limitations

- UI
  - unified window title & tab bar
    - not available on 10.7/10.8/10.9, replaced by classical title bar instead (thanks to [@Wowfunhappy](https://github.com/Wowfunhappy))
  - menus/sheets
    - have no shadow, indistinguishable from background
  - scrollbars
    - won't disappear despite "Show scrollbars when scrolling" option is enabled when GPU compositing is disabled
- GPU assists
  - on 10.7, due to old OpenGL version, disabled entirely by embedded policy
  - on 10.8/10.9, GPU compositing is disabled by hardcoded `--disable-gpu-compositing` option due to rendering glitches.
- DRM
  - on 10.7/10.8, cannot use DRM protected media at all
  - on 10.9+, need to [install Widevine library](../../wiki/DRM) to use DRM
- U2F/WebAuthn/FIDO2
  - on 10.7, you need [patched `IOHIDFamily.kext`](../../../IOHIDFamily-368.21) to use USB keys

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

If you found a bug, please file it at https://crbug.com/new.
