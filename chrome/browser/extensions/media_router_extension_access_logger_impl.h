// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_IMPL_H_

#include "extensions/browser/media_router_extension_access_logger.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace extensions {

class MediaRouterExtensionAccessLoggerImpl
    : public MediaRouterExtensionAccessLogger {
 public:
  ~MediaRouterExtensionAccessLoggerImpl() override;

  void LogMediaRouterComponentExtensionUse(
      const url::Origin& origin,
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MEDIA_ROUTER_EXTENSION_ACCESS_LOGGER_IMPL_H_
