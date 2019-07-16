// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_dialog_controller.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class SharingDeviceInfo;

class ClickToCallSharingDialogController : public SharingDialogController {
 public:
  ClickToCallSharingDialogController(content::WebContents* web_contents,
                                     SharingService* sharing_service,
                                     std::string phone_number);

  ~ClickToCallSharingDialogController() override;

  // Overridden from SharingDialogController:
  std::string GetTitle() override;
  std::vector<SharingDeviceInfo> GetSyncedDevices() override;
  std::vector<App> GetApps() override;
  void OnDeviceChosen(const SharingDeviceInfo& device,
                      SharingService::SendMessageCallback callback) override;
  void OnAppChosen(const App& app) override;

 private:
  content::WebContents* web_contents_;
  SharingService* sharing_service_;
  std::string phone_number_;
  GURL phone_url_;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallSharingDialogController);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_SHARING_DIALOG_CONTROLLER_H_
