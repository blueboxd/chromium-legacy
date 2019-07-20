// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

class SharingDeviceInfo;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// The controller for desktop dialog with the list of synced devices and apps.
class SharingDialogController {
 public:
  struct App {
    App(const gfx::VectorIcon& icon,
        base::string16 name,
        std::string identifier);
    App(App&& other);
    ~App();

    const gfx::VectorIcon& icon;
    base::string16 name;
    std::string identifier;
  };

  SharingDialogController() = default;
  virtual ~SharingDialogController() = default;

  // Title of the dialog.
  virtual base::string16 GetTitle() = 0;

  // Returns filtered list of synced devices for the feature.
  virtual std::vector<SharingDeviceInfo> GetSyncedDevices() = 0;

  // Returns list of local apps capable of supporting action.
  virtual std::vector<App> GetApps() = 0;

  // Called when user chooses a synced device to complete the task.
  virtual void OnDeviceChosen(const SharingDeviceInfo& device) = 0;

  // Called when user chooses a local app to complete the task.
  virtual void OnAppChosen(const App& app) = 0;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DIALOG_CONTROLLER_H_
