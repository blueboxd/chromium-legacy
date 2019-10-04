// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <ios>
#include <ostream>
#include <tuple>

#include "base/logging.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/color_utils.h"

namespace web_app {

WebApp::WebApp(const AppId& app_id)
    : app_id_(app_id), display_mode_(blink::mojom::DisplayMode::kUndefined) {}

WebApp::~WebApp() = default;

void WebApp::AddSource(Source::Type source) {
  sources_[source] = true;
}

void WebApp::RemoveSource(Source::Type source) {
  sources_[source] = false;
}

bool WebApp::HasAnySources() const {
  return sources_.any();
}

bool WebApp::IsSynced() const {
  return sources_[Source::kSync];
}

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

void WebApp::SetDisplayMode(blink::mojom::DisplayMode display_mode) {
  DCHECK_NE(blink::mojom::DisplayMode::kUndefined, display_mode);
  display_mode_ = display_mode;
}

void WebApp::SetIsLocallyInstalled(bool is_locally_installed) {
  is_locally_installed_ = is_locally_installed;
}

void WebApp::SetIsSyncPlaceholder(bool is_sync_placeholder) {
  is_sync_placeholder_ = is_sync_placeholder;
}

void WebApp::SetIcons(Icons icons) {
  icons_ = std::move(icons);
}

void WebApp::SetSyncData(const SyncData& sync_data) {
  sync_data_ = sync_data;
}

WebApp::SyncData::SyncData() = default;

WebApp::SyncData::~SyncData() = default;

std::ostream& operator<<(std::ostream& out, const WebApp& app) {
  const std::string theme_color =
      app.theme_color_.has_value()
          ? color_utils::SkColorToRgbaString(app.theme_color_.value())
          : "none";
  const std::string sync_theme_color =
      app.sync_data_.theme_color.has_value()
          ? color_utils::SkColorToRgbaString(app.sync_data_.theme_color.value())
          : "none";
  const std::string display_mode =
      blink::DisplayModeToString(app.display_mode_);
  const bool is_locally_installed = app.is_locally_installed_;
  const bool is_sync_placeholder = app.is_sync_placeholder_;

  return out << "app_id: " << app.app_id_ << std::endl
             << "  name: " << app.name_ << std::endl
             << "  launch_url: " << app.launch_url_ << std::endl
             << "  scope: " << app.scope_ << std::endl
             << "  theme_color: " << theme_color << std::endl
             << "  display_mode: " << display_mode << std::endl
             << "  sources: " << app.sources_.to_string() << std::endl
             << "  is_locally_installed: " << is_locally_installed << std::endl
             << "  is_sync_placeholder: " << is_sync_placeholder << std::endl
             << "  sync_data.name: " << app.sync_data_.name << std::endl
             << "  sync_data.theme_color: " << sync_theme_color << std::endl
             << "  description: " << app.description_;
}

bool operator==(const WebApp::IconInfo& icon_info1,
                const WebApp::IconInfo& icon_info2) {
  return std::tie(icon_info1.url, icon_info1.size_in_px) ==
         std::tie(icon_info2.url, icon_info2.size_in_px);
}

bool operator==(const WebApp::SyncData& sync_data1,
                const WebApp::SyncData& sync_data2) {
  return std::tie(sync_data1.name, sync_data1.theme_color) ==
         std::tie(sync_data2.name, sync_data2.theme_color);
}

bool operator==(const WebApp& app1, const WebApp& app2) {
  return std::tie(app1.app_id_, app1.sources_, app1.name_, app1.launch_url_,
                  app1.description_, app1.scope_, app1.theme_color_,
                  app1.icons_, app1.display_mode_, app1.is_locally_installed_,
                  app1.is_sync_placeholder_, app1.sync_data_) ==
         std::tie(app2.app_id_, app2.sources_, app2.name_, app2.launch_url_,
                  app2.description_, app2.scope_, app2.theme_color_,
                  app2.icons_, app2.display_mode_, app2.is_locally_installed_,
                  app2.is_sync_placeholder_, app2.sync_data_);
}

bool operator!=(const WebApp& app1, const WebApp& app2) {
  return !(app1 == app2);
}

}  // namespace web_app
