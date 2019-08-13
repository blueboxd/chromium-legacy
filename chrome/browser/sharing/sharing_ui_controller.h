// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"

class BrowserWindow;
class SharingDeviceInfo;
class SharingDialog;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content

// The controller for desktop dialog with the list of synced devices and apps.
class SharingUiController {
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

  explicit SharingUiController(content::WebContents* web_contents);
  virtual ~SharingUiController() = default;

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

  virtual PageActionIconType GetIconType() = 0;

  // Called by the ClickToCallDialogView when it is being closed.
  void OnDialogClosed(SharingDialog* dialog);
  void StartLoading();
  void StopLoading(bool send_failed);
  void InvalidateOldDialog();
  // Shows an error dialog if we're still on the same tab.
  void ShowErrorDialog();
  // Returns the currently open SharingDialog or nullptr if there is no
  // dialog open.
  SharingDialog* dialog() const { return dialog_; }
  bool is_loading() const { return is_loading_; }
  bool send_failed() const { return send_failed_; }
  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  virtual SharingDialog* DoShowDialog(BrowserWindow* window) = 0;

 private:
  // Updates the omnibox icon if available.
  void UpdateIcon();
  // Shows a new ClickToCallDialogView and closes the old one.
  void ShowNewDialog();

  SharingDialog* dialog_ = nullptr;
  bool is_loading_ = false;
  bool send_failed_ = false;
  content::WebContents* web_contents_ = nullptr;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
