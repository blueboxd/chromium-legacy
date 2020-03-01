// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace web_app {

constexpr char kSystemAppManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

// Creates and registers a URLDataSource that serves a blank page with
// a |kSystemAppManifestText| manifest.
void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context);

// Creates and registers a URLDataSource that serves a blank page with
// a |manifest_text| manifest.
void AddTestURLDataSource(const std::string& source_name,
                          const std::string* manifest_text,
                          content::BrowserContext* browser_context);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
