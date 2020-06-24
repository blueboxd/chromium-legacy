// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_H_
#define EXTENSIONS_BROWSER_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_H_

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace extensions {

// Logs accesses of extension resources for the Media Router Component
// Extension.
class MediaRouterExtensionAccessLogger {
 public:
  virtual ~MediaRouterExtensionAccessLogger();

  // Logs that |origin| accesses a resource from the Media Router Component
  // Extension (the extension used to support casting to Chromecast devices).
  virtual void LogMediaRouterComponentExtensionUse(
      const url::Origin& origin,
      content::BrowserContext* context) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_H_
