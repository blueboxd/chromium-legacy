// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

class WebApp {
 public:
  explicit WebApp(const AppId& app_id);
  ~WebApp();

  const AppId& app_id() const { return app_id_; }

  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const GURL& launch_url() const { return launch_url_; }
  const GURL& scope() const { return scope_; }
  const base::Optional<SkColor>& theme_color() const { return theme_color_; }

  struct IconInfo {
    GURL url;
    int size_in_px;
  };
  using Icons = std::vector<IconInfo>;
  const Icons& icons() const { return icons_; }

  void SetName(const std::string& name);
  void SetDescription(const std::string& description);
  void SetLaunchUrl(const GURL& launch_url);
  void SetScope(const GURL& scope);
  void SetThemeColor(base::Optional<SkColor> theme_color);
  void SetIcons(Icons icons);

 private:
  const AppId app_id_;

  std::string name_;
  std::string description_;
  GURL launch_url_;
  // TODO(loyso): Implement IsValid() function that verifies that the launch_url
  // is within the scope.
  GURL scope_;
  base::Optional<SkColor> theme_color_;
  Icons icons_;

  DISALLOW_COPY_AND_ASSIGN(WebApp);
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const WebApp& app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
