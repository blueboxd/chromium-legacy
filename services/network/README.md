# Network Service

[TOC]

This is a service for networking. It's meant to be oblivious to Chrome's features.
Some design goals
  * this only contains features that go over the network. e.g. no file loading, data URLs etc...
  * only the lowest-level of networking should be here. e.g. http, sockets, web
    sockets. Anything that is built on top of this should be in higher layers.
  * higher level web platform and browser features should be built outside of
    this code. Safe browsing, Service Worker, extensions, devtools etc... should
    not have hooks here. The only exception is when it's impossible for these
    features to function without some hooks in the network service. In that
    case, we add the minimal code required. Some examples included traffic
    shaping for devtools and CORB blocking.
  * every PostTask, thread hop and process hop (IPC) should be counted carefully
    as they introduce delays which could harm this performance critical code.
  * `NetworkContext` and `NetworkService` are trusted interfaces that aren't
    meant to be sent to the renderer. Only the browser should have access to
    them.

See https://bugs.chromium.org/p/chromium/issues/detail?id=598073

See the design doc
https://docs.google.com/document/d/1wAHLw9h7gGuqJNCgG1mP1BmLtCGfZ2pys-PdZQ1vg7M/edit?pref=2&pli=1#

# Buildbot

The [Network Service
Linux](https://ci.chromium.org/p/chromium/builders/ci/Network%20Service%20Linux)
buildbot runs browser tests with the network service in non-default but
supported configurations. Ideally this bot would be on the CQ, but it is
expensive and would affect CQ time, so it's on the main waterfall but not the
CQ.

Its steps are:

* **`network_service_in_process_browser_tests`**: Runs `browser_tests` with the
  network service in-process
  (`--enable-features=NetworkServiceInProcess`). This step is important because
  Chrome on Android runs with the network service in-process by default
  (https://crbug.com/1049008). However, `browser_tests` are not well-supported
  on Android (https://crbug.com/611756), so we run them on this Linux bot.
  Furthermore, there is a flag and group policy to run the network service
  in-process on Desktop, but there are efforts to remove this
  (https://crbug.com/1036230).
* **`network_service_in_process_content_browsertests`**: Same as above but for
  `content_browsertests`. We might consider removing this from the bot, since
  the Android bots run `content_browsertests` which should give enough coverage,
  but maybe we can remove the Desktop flag and group policy first.
* **`network_service_web_request_proxy_browser_tests`**: Runs `browser_tests`
  while forcing the "network request proxying" code path that is taken when the
  browser has an extension installed that uses the
  [Web Request API](https://developer.chrome.com/extensions/webRequest)
  (`--enable-features=ForceWebRequestProxyForTest`). This step has caught bugs
  that would be Stable Release Blockers, so it's important to keep it.
