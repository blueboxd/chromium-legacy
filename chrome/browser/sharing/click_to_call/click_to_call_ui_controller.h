// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_

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

class ClickToCallUiController
    : public SharingUiController,
      public content::WebContentsUserData<ClickToCallUiController> {
 public:
  static ClickToCallUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);
  static void ShowDialog(content::WebContents* web_contents,
                         const GURL& url,
                         bool hide_default_handler);
  static void DeviceSelected(content::WebContents* web_contents,
                             const GURL& url,
                             const SharingDeviceInfo& device);

  ~ClickToCallUiController() override;

  // Overridden from SharingUiController:
  base::string16 GetTitle() override;
  std::vector<SharingDeviceInfo> GetSyncedDevices() override;
  std::vector<App> GetApps() override;
  void OnDeviceChosen(const SharingDeviceInfo& device) override;
  void OnAppChosen(const App& app) override;
  PageActionIconType GetIconType() override;

  // Called by the ClickToCallDialogView when the help text got clicked.
  void OnHelpTextClicked();

 protected:
  explicit ClickToCallUiController(content::WebContents* web_contents);
  SharingDialog* DoShowDialog(BrowserWindow* window) override;

 private:
  friend class content::WebContentsUserData<ClickToCallUiController>;

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(int dialog_id, bool success);

  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

  GURL phone_url_;
  bool hide_default_handler_ = false;

  // ID of the last shown dialog used to ignore events from old dialogs.
  // TODO(yasmo): Maybe can be moved to the base class.
  int last_dialog_id_ = 0;

  base::WeakPtrFactory<ClickToCallUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClickToCallUiController);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
