\[ en | [ja](README.ja.md) \]

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium-legacy

Chromium-legacy is the latest Chromium (almost equivalent to Chrome Canary without Google branding) patched & built for legacy Mac OS X series, not supported officially:

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite

**NB: Not for officially supported environments (10.11+).**

## features

Basically equivalent to the [same version of original Chromium](https://chromestatus.com/features) except for limitations by old OSes (see [limitations](#limitations)).  
Some features (i.e. [DRM](../../wiki/DRM)) need extra operations to use.

## system requirements

- Mac OS X 10.7 or later
  - requires latest build
    - 10.7.5 (11G63)
    - 10.8.5 (12F2560)
    - 10.9.5 (13F1911)
    - 10.10.5 (14F2511)
  - also requires **all software updates** to be applied (inclding security patches)

## builds

This project is automatically built and uploaded twice a day (00:00 and 12:00 JST) if no issues occurred.
And note, uploaded **without any tests**, thus there is no guarantee for launching or proper operation.
It's recommended to find and use a stable build for daily use. (The same is true for the original Chrome Canary/Chromium)

### latest

See [Releases](../../releases) for recent builds.

You can automatically download the latest build with commands like:  

```bash
curl -s https://api.github.com/repos/blueboxd/chromium-legacy/releases/latest|grep 'browser_download_url'|grep xz|awk -F '"' '{print $4}'|xargs curl -L|tar xvf -
```  

Or, you can [use a downloader](https://github.com/blueboxd/chromium-legacy/discussions/25) by [@Wowfunhappy](https://github.com/Wowfunhappy).

### Chrome stable channel

[96.0.4664.55](../../releases/tag/stable)

Or, you can [find the branch position](https://omahaproxy.appspot.com/all?os=mac&channel=stable) of the current Chrome stable channel.

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

## building

To build from source, see [Building](../../wiki/Building).
