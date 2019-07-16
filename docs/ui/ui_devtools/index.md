# UI DevTools Overview

UI DevTools allows UI developers to inspect the Chrome desktop UI system like a webpage using the frontend DevTools Inspector. It is
currently supported on Linux, Windows, Mac, and ChromeOS.

* [Old Ash Doc](https://www.chromium.org/developers/how-tos/inspecting-ash)
* [Backend Source Code](https://cs.chromium.org/chromium/src/components/ui_devtools/)
* [Inspector Frontend Source Code](https://cs.chromium.org/chromium/src/third_party/blink/renderer/devtools/front_end/)

## How to run

1. Run Chromium with default port 9223 and start Chromium with UI DevTools flag:

        $ out/Default/chrome --enable-ui-devtools

    * If you want to use different port, add port in the flag `--enable-ui-devtools=<port>`.
2. In the Chrome Omnibox, go to chrome://inspect#other and click `inspect` under UIDevToolsClient.
    * Direct link is chrome-devtools://devtools/bundled/inspector.html?ws=localhost:9223/0.


## Features

TODO(khamasaki): add features.
