// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ios>
#include <ostream>

#include "chrome/browser/web_applications/web_app.h"

#include "base/logging.h"
#include "ui/gfx/color_utils.h"

namespace web_app {

WebApp::WebApp(const AppId& app_id) : app_id_(app_id) {}

WebApp::~WebApp() = default;

void WebApp::SetName(const std::string& name) {
  name_ = name;
}

void WebApp::SetDescription(const std::string& description) {
  description_ = description;
}

void WebApp::SetLaunchUrl(const GURL& launch_url) {
  DCHECK(!launch_url.is_empty() && launch_url.is_valid());
  launch_url_ = launch_url;
}

void WebApp::SetScope(const GURL& scope) {
  DCHECK(scope.is_empty() || scope.is_valid());
  scope_ = scope;
}

void WebApp::SetThemeColor(base::Optional<SkColor> theme_color) {
  theme_color_ = theme_color;
}

void WebApp::SetIcons(Icons icons) {
  icons_ = std::move(icons);
}

std::ostream& operator<<(std::ostream& out, const WebApp& app) {
  const std::string theme_color =
      app.theme_color()
          ? color_utils::SkColorToRgbaString(app.theme_color().value())
          : "none";

  return out << "app_id: " << app.app_id() << std::endl
             << "  name: " << app.name() << std::endl
             << "  launch_url: " << app.launch_url() << std::endl
             << "  scope: " << app.scope() << std::endl
             << "  theme_color: " << theme_color << std::endl
             << "  description: " << app.description();
}

}  // namespace web_app
