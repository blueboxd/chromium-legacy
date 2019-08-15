// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <ios>
#include <ostream>

#include "base/logging.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "ui/gfx/color_utils.h"

namespace web_app {

WebApp::WebApp(const AppId& app_id)
    : app_id_(app_id), launch_container_(LaunchContainer::kDefault) {}

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

void WebApp::SetLaunchContainer(LaunchContainer launch_container) {
  DCHECK_NE(LaunchContainer::kDefault, launch_container);
  launch_container_ = launch_container;
}

void WebApp::SetIsLocallyInstalled(bool is_locally_installed) {
  is_locally_installed_ = is_locally_installed;
}

void WebApp::SetIcons(Icons icons) {
  icons_ = std::move(icons);
}

std::ostream& operator<<(std::ostream& out, const WebApp& app) {
  const std::string theme_color =
      app.theme_color()
          ? color_utils::SkColorToRgbaString(app.theme_color().value())
          : "none";
  const char* launch_container =
      LaunchContainerEnumToStr(app.launch_container());
  const bool is_locally_installed = app.is_locally_installed();

  return out << "app_id: " << app.app_id() << std::endl
             << "  name: " << app.name() << std::endl
             << "  launch_url: " << app.launch_url() << std::endl
             << "  scope: " << app.scope() << std::endl
             << "  theme_color: " << theme_color << std::endl
             << "  launch_container: " << launch_container << std::endl
             << "  is_locally_installed: " << is_locally_installed << std::endl
             << "  description: " << app.description();
}

}  // namespace web_app
