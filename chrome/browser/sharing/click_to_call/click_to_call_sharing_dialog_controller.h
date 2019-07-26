// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/sharing_dialog_controller.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class ClickToCallDialog;
class SharingDeviceInfo;

class ClickToCallSharingDialogController
    : public SharingDialogController,
      public content::WebContentsUserData<ClickToCallSharingDialogController> {
 public:
  static ClickToCallSharingDialogController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);
  static void ShowDialog(content::WebContents* web_contents,
                         const GURL& url,
                         bool hide_default_handler);
  static void DeviceSelected(content::WebContents* web_contents,
                             const GURL& url,
                             const SharingDeviceInfo& device);

  ~ClickToCallSharingDialogController() override;

  // Overridden from SharingDialogController:
  base::string16 GetTitle() override;
  std::vector<SharingDeviceInfo> GetSyncedDevices() override;
  std::vector<App> GetApps() override;
  void OnDeviceChosen(const SharingDeviceInfo& device) override;
  void OnAppChosen(const App& app) override;

  // Called by the ClickToCallDialogView when it is being closed.
  void OnDialogClosed(ClickToCallDialog* dialog);

  // Called by the ClickToCallDialogView when the help text got clicked.
  void OnHelpTextClicked();

  // Returns the currently open ClickToCallDialog or nullptr if there is no
  // dialog open.
  ClickToCallDialog* GetDialog() const;

  bool is_loading() const { return is_loading_; }

  bool send_failed() const { return send_failed_; }

 protected:
  explicit ClickToCallSharingDialogController(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ClickToCallSharingDialogController>;

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(int dialog_id, bool success);

  // Shows a new ClickToCallDialogView and closes the old one.
  void ShowNewDialog();

  // Shows an error dialog if we're still on the same tab.
  void ShowErrorDialog();

  // Updates the omnibox icon if available.
  void UpdateIcon();

  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

  GURL phone_url_;
  ClickToCallDialog* dialog_ = nullptr;
  bool is_loading_ = false;
  bool send_failed_ = false;
  bool hide_default_handler_ = false;

  // ID of the last shown dialog used to ignore events from old dialogs.
  int last_dialog_id_ = 0;

  base::WeakPtrFactory<ClickToCallSharingDialogController> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClickToCallSharingDialogController);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_
