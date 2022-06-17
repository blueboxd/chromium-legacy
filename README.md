\[ en | [ja](README.ja.md) \]

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium-legacy

Chromium-legacy is the latest Chromium (almost equivalent to Chrome Canary without Google branding) patched & built for legacy Mac OS X series currently not supported officially:

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite

**NB: Not for officially supported environments (10.11+).**

## features

Equivalent to the [same version of the original Chromium](https://chromestatus.com/features) except for limitations by old OSes (see [limitations](#limitations)).  
Some features (i.e. [DRM](../../wiki/DRM)) need an extra setup to use.

## system requirements

- Mac OS X 10.7 or later
  - requires latest build
    - 10.7.5 (11G63)
    - 10.8.5 (12F2560)
    - 10.9.5 (13F1911)
    - 10.10.5 (14F2511)

## builds

For getting/updating the latest builds, the following options are available:
- manually from [Releases](../../releases) page
- [an updater](https://github.com/blueboxd/chromium-updater)
  - the lightweight updater to simply download updates
- [a downloader](https://github.com/blueboxd/chromium-legacy/discussions/25) by [@Wowfunhappy](https://github.com/Wowfunhappy)
  - an automatic updater with several workarounds / addons
  - NB: some Chromium's features (i.e. sync) will be disabled by this downloader, and may need modifications to use those

### canary (Chrome canary channel)

Canary builds are automatically built and uploaded several times a week.  
And note, uploaded **without any tests**, thus there is no guarantee for launching or proper operation.  
It's recommended to find and use a stable build for daily use. (The same is true for the original Chrome Canary/Chromium)

See [Releases](../../releases) for recent builds.

### stable (Chrome stable channel)

Stable builds are based on Chrome's stable channel branch, and passed a few basic tests (launching, HTML5/JS tests, media playing) on 10.7.

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
  - on 10.8/10.9, GPU compositing is disabled by hardcoded `--disable-gpu-compositing` option due to rendering glitches.
- DRM
  - on 10.7/10.8, cannot use DRM-protected media at all
  - on 10.9+, need to [install Widevine library](../../wiki/DRM) to use DRM
- U2F/WebAuthn/FIDO2
  - on 10.7, you need [patched `IOHIDFamily.kext`](../../../IOHIDFamily-368.21) to use USB keys

## building

To build from the source, see [Building](../../wiki/Building).
