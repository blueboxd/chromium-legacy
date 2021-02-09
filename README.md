# Chromium-legacy

Chromium-legacy is the latest Chromium patched & built for legacy Mac OS X (10.7+).
~~Not working on 10.9 for now.~~
Now working on 10.9. (thanks [@Wowfunhappy](https://github.com/Wowfunhappy))

**NB: Not for natively supported environments (10.10+).**

## working properly
- modern HTTP protocols (http2/http3/quic)
- modern TLS
- rendering
    - [Skia is also patched](../../../skia) for proper font rendering
- JavaScript
    - latest V8 engine
- media playing
    - but due to no GPU assist, high CPU usage
- WebAuthn/FIDO2
    - you need some patches to use USB keys on 10.7.

## limitation / glitches
- UI
    - windows
        - ~~close/minimize/resize buttons are invisible~~
            - ~~but functional when clicking appropriate position~~
            - FIXED as classical title bar (thanks to [@Wowfunhappy](https://github.com/Wowfunhappy))
        - ~~can't move by dragging title bar region~~
            - ~~you can move by dragging the edges of the window (when the cursor is resizing arrow: &#x2194;&#x2195;)~~
            - FIXED
    - menus/sheets
        - have no shadow
        - ~~temporally FIXED (popups have some glitches on corners)~~
          - reverted due to improper rendering of combo box
    - scrollbars
        - won't disappear despite "Show scrollbars when scrolling" option is enabled
- GPU assists
    - on 10.7, due to old OpenGL version, disabled entirely by Chromium itself
    - on 10.9, GPU compositing is disabled by hardcoded `--disable-gpu-compositing` option due to rendering glitches.
- Sandboxing
    - sandboxing is disabled with hardcoded `--no-sandbox` option because Seatbelt is too old to load latest policies

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
