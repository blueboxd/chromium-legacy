// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "components/sync_device_info/device_info.h"
#include "ui/gfx/image/image.h"

class BrowserWindow;
class SharingDialog;
class SharingService;

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
    App(const gfx::VectorIcon* vector_icon,
        const gfx::Image& image,
        base::string16 name,
        std::string identifier);
    App(App&& other);
    ~App();

    const gfx::VectorIcon* vector_icon = nullptr;
    gfx::Image image;
    base::string16 name;
    std::string identifier;
  };

  using UpdateAppsCallback = base::OnceCallback<void(std::vector<App>)>;

  explicit SharingUiController(content::WebContents* web_contents);
  virtual ~SharingUiController();

  // Title of the dialog.
  virtual base::string16 GetTitle() = 0;

  // Called when user chooses a synced device to complete the task.
  virtual void OnDeviceChosen(const syncer::DeviceInfo& device) = 0;

  // Called when user chooses a local app to complete the task.
  virtual void OnAppChosen(const App& app) = 0;

  virtual PageActionIconType GetIconType() = 0;

  virtual int GetRequiredDeviceCapabilities() = 0;

  // Called by the SharingDialog when it is being closed.
  void OnDialogClosed(SharingDialog* dialog);

  void UpdateAndShowDialog();

  void UpdateDevices();

  // Returns the currently open SharingDialog or nullptr if there is no
  // dialog open.
  SharingDialog* dialog() const { return dialog_; }
  bool is_loading() const { return is_loading_; }
  bool send_failed() const { return send_failed_; }
  content::WebContents* web_contents() const { return web_contents_; }
  const std::vector<App>& apps() const { return apps_; }
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices() const {
    return devices_;
  }

  void set_apps_for_testing(std::vector<App> apps) { apps_ = std::move(apps); }
  void set_devices_for_testing(
      std::vector<std::unique_ptr<syncer::DeviceInfo>> devices) {
    devices_ = std::move(devices);
  }

 protected:
  virtual SharingDialog* DoShowDialog(BrowserWindow* window) = 0;

  virtual void DoUpdateApps(UpdateAppsCallback callback) = 0;

  void SendMessageToDevice(
      const syncer::DeviceInfo& device,
      chrome_browser_sharing::SharingMessage sharing_message);

 private:
  // Updates the omnibox icon if available.
  void UpdateIcon();
  // Closes the current dialog if there is one.
  void CloseDialog();
  // Shows a new SharingDialog and closes the old one.
  void ShowNewDialog();

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(int dialog_id, SharingSendMessageResult result);

  void OnAppsReceived(int dialog_id, std::vector<App> apps);

  SharingDialog* dialog_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

  bool is_loading_ = false;
  bool send_failed_ = false;

  // Currently used apps and devices since the last call to UpdateAndShowDialog.
  std::vector<App> apps_;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices_;

  // ID of the last shown dialog used to ignore events from old dialogs.
  int last_dialog_id_ = 0;

  base::WeakPtrFactory<SharingUiController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
