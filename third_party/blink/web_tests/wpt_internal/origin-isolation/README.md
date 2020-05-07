# Origin Isolation Origin Trial Tests

These tests are for the [Origin Isolation][1] feature, and supplement those in
`external/wpt` to specifically test Chromium origin trial mechanisms.

The abbreviation `oiot` means "first the `Origin-Trial` header, then the
`Origin-Isolation` header". Conversely, `otoi` means "first the
`Origin-Isolation` header, then the `Origin-Trial` header.

The tokens for the `Origin-Trial` header are generated using the commands

    python tools/origin_trials/generate_token.py https://web-platform.test:8444 OriginIsolationHeader --expire-timestamp=2000000000
    python tools/origin_trials/generate_token.py https://www.web-platform.test:8444 OriginIsolationHeader --expire-timestamp=2000000000

[1]: https://github.com/WICG/origin-isolation
