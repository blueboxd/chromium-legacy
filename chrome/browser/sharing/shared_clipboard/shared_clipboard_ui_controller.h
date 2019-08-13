// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class SharingDeviceInfo;

class SharedClipboardUiController
    : public SharingUiController,
      public content::WebContentsUserData<SharedClipboardUiController> {
 public:
  static SharedClipboardUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);
  static void DeviceSelected(content::WebContents* web_contents,
                             const base::string16 text,
                             const SharingDeviceInfo& device);

  ~SharedClipboardUiController() override;

  // Overridden from SharingUiController:
  base::string16 GetTitle() override;
  std::vector<SharingDeviceInfo> GetSyncedDevices() override;
  std::vector<App> GetApps() override;
  void OnDeviceChosen(const SharingDeviceInfo& device) override;
  void OnAppChosen(const App& app) override;
  PageActionIconType GetIconType() override;

  // Called by the SharedClipboardDialogView when the help text got clicked.
  void OnHelpTextClicked();

 protected:
  explicit SharedClipboardUiController(content::WebContents* web_contents);
  SharingDialog* DoShowDialog(BrowserWindow* window) override;

 private:
  friend class content::WebContentsUserData<SharedClipboardUiController>;

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(int dialog_id, bool success);

  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

  base::string16 text_;
  bool hide_default_handler_ = false;

  base::WeakPtrFactory<SharedClipboardUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardUiController);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UI_CONTROLLER_H_
