// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_INFO_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_INFO_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "ui/aura/window.h"

// The information about the ARC application window which has to be kept
// even when its AppWindow is not present.
class ArcAppWindowInfo {
 public:
  ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                   const std::string& launch_intent,
                   const std::string& package_name);
  ~ArcAppWindowInfo();

  ArcAppWindowInfo(const ArcAppWindowInfo&) = delete;
  ArcAppWindowInfo& operator=(const ArcAppWindowInfo&) = delete;

  void SetDescription(const std::string& title,
                      const std::vector<uint8_t>& icon_data_png);

  void set_window(aura::Window* window);

  aura::Window* window();

  const arc::ArcAppShelfId& app_shelf_id() const;

  const ash::ShelfID shelf_id() const;

  const std::string& launch_intent() const;

  const std::string& package_name() const;

  const std::string& title() const;

  const std::vector<uint8_t>& icon_data_png() const;

 private:
  const arc::ArcAppShelfId app_shelf_id_;
  const std::string launch_intent_;
  const std::string package_name_;
  // Keeps overridden window title.
  std::string title_;
  // Keeps overridden window icon.
  std::vector<uint8_t> icon_data_png_;

  aura::Window* window_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_INFO_H_
