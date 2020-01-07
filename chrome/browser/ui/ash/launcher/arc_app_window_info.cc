// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_info.h"

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

namespace {

constexpr size_t kMaxIconPngSize = 64 * 1024;  // 64 kb

}  // namespace

ArcAppWindowInfo::ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                                   const std::string& launch_intent,
                                   const std::string& package_name)
    : app_shelf_id_(app_shelf_id),
      launch_intent_(launch_intent),
      package_name_(package_name) {}

ArcAppWindowInfo::~ArcAppWindowInfo() = default;

void ArcAppWindowInfo::SetDescription(
    const std::string& title,
    const std::vector<uint8_t>& icon_data_png) {
  DCHECK(base::IsStringUTF8(title));
  title_ = title;

  // Chrome has custom Play Store icon. Don't overwrite it.
  if (app_shelf_id_.app_id() == arc::kPlayStoreAppId)
    return;
  if (icon_data_png.size() < kMaxIconPngSize)
    icon_data_png_ = icon_data_png;
  else
    VLOG(1) << "Task icon size is too big " << icon_data_png.size() << ".";
}

void ArcAppWindowInfo::set_window(aura::Window* window) {
  window_ = window;
}

aura::Window* ArcAppWindowInfo::ArcAppWindowInfo::window() {
  return window_;
}

const arc::ArcAppShelfId& ArcAppWindowInfo::app_shelf_id() const {
  return app_shelf_id_;
}

const ash::ShelfID ArcAppWindowInfo::shelf_id() const {
  return ash::ShelfID(app_shelf_id_.ToString());
}

const std::string& ArcAppWindowInfo::launch_intent() const {
  return launch_intent_;
}

const std::string& ArcAppWindowInfo::package_name() const {
  return package_name_;
}

const std::string& ArcAppWindowInfo::title() const {
  return title_;
}

const std::vector<uint8_t>& ArcAppWindowInfo::icon_data_png() const {
  return icon_data_png_;
}
