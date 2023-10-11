\[ en | [ja](README.ja.md) \]

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium-legacy

Chromium-legacy is the up-to-date browser[^chromium] for legacy Mac OS X / OS X / macOS series no longer officially supported:

[^chromium]: Chromium, the open-source project on which Google Chrome is based.

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite
- OS X 10.11 / El Capitan
- macOS 10.12 / Sierra
- macOS 10.13 / High Sierra
- macOS 10.14 / Mojave

**NB: Not for officially supported macOS (10.15+).**

## features

Equivalent to the [same version of the original Chromium](https://chromestatus.com/features) except for limitations by old OSes (see [limitations](#limitations)).   
Some features (i.e. [DRM](../../wiki/DRM)) need an extra setup to use.

## system requirements

Tested with the latest build of Mac OS X 10.7 or later:

- 10.7.5 (11G63)
- 10.8.5 (12F2560)
- 10.9.5 (13F1911)
- 10.10.5 (14F2511)
- 10.11.6 (15G22010)
- 10.12.6 (16G2136)
- 10.13.6 (17G14042)
- 10.14.6 (18G9323)

## builds

For getting/updating the latest builds, the following options are available:

- manually from the [Releases](../../releases) page
- [an updater](https://github.com/blueboxd/chromium-updater)
  - a lightweight updater to simply check and download updates
- [a downloader](https://github.com/blueboxd/chromium-legacy/discussions/25) by [@Wowfunhappy](https://github.com/Wowfunhappy)
  - an automatic updater with several workarounds/addons
  - NB: some Chromium's native features (e.g. sync) may be disabled by this downloader, and may need modifications to use those

### canary (Chrome canary channel)

Canary builds are automatically built and uploaded several times a week.  
And note, uploaded **without any tests**, thus there is no guarantee for launching or proper operation.  
It's recommended to find and use a stable build for daily use. (The same is true for the original Chrome Canary/Chromium)

See [Releases](../../releases) for recent builds.

### stable (Chrome stable channel)

Stable builds are based on Chrome's stable channel branch and passed a few basic tests (launching, HTML5/JS tests, media playing) on 10.7 - 10.14.

See [releases/tag/stable](../../releases/tag/stable) for the current stable channel release.

## limitations

- UI
  - unified window title & tab bar
    - not available on 10.7/10.8/10.9, replaced by classical title bar instead (thanks to [@Wowfunhappy](https://github.com/Wowfunhappy))
  - menus/sheets
    - have no shadow, indistinguishable from the background
  - scrollbars
    - won't disappear despite the "Show scrollbars when scrolling" option being enabled when GPU compositing is disabled
- GPU assists
  - on 10.7, due to the old OpenGL version, disabled entirely by embedded policy
  - on 10.8/10.9, GPU compositing is disabled by the hardcoded `--disable-gpu-compositing` option due to rendering glitches.
- DRM (Widevine)
  - usable with limited resolution/compatibility
  - need to [setup](../../wiki/DRM) to use
- U2F/WebAuthn/FIDO2
  - on 10.7, you need [patched `IOHIDFamily.kext`](../../../IOHIDFamily-368.21) to use USB keys

## building

To build from the source, see [Building](../../wiki/Building).
