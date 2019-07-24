// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SERVICE_WEB_BROWSER_MANIFEST_H_
#define IOS_WEB_SERVICE_WEB_BROWSER_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace web {

// Returns the service manifest for the web_browser service, which is the main
// service that the core browser identifies as. This determines what service
// capabilities the browser has access to, as well as what per-profile services
// are packaged within the browser.
const service_manager::Manifest& GetWebBrowserManifest();

}  // namespace web

#endif  // IOS_WEB_SERVICE_WEB_BROWSER_MANIFEST_H_
