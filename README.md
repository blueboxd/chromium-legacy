# Chromium-legacy

Chromium-legacy is the latest Chromium patched & built for legacy Mac OS X Lion series (10.7/10.8).
**Not working on 10.9 for now.**

**NB: Not for natively supported environments (10.10+).**

## working properly
- networking
    - modern HTTP protocols (http2/quic)
    - modern TLS
- rendering
    - [Skia is also patched](../../../skia) for proper font rendering
- JavaScript
- media playing
    - but due to no GPU assist, high CPU usage

## limitation / glitches
Some features not implemented in 10.7 are disabled (maybe simply commented out), so may not available even on 10.10+. (you'd better use official Chrome builds on 10.10+, of course)
- UI
    - windows
        - close/minimize/resize buttons are invisible
            - but functional when clicking appropriate position
        - can't move by dragging title bar region
            - you can move by dragging the edges of the window (when the cursor is resizing arrow: &#x2194;&#x2195;)
    - menus/sheets
        - have no shadow
    - scrollbars
        - won't disappear despite "Show scrollbars when scrolling" option is enabled
- GPU assists (rendering / encoding / decoding)
    - due to old OpenGL version, disabled by Chromium itself
- Sandboxing
    - sandboxing is disabled with hardcoded *--no-sandbox* option because Seatbelt is too old to load latest policies

---

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory Structure
](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.
