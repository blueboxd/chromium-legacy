// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_H_

#include "chrome/browser/enterprise_reporting/notification/extension_request_notification.h"
#include "chrome/browser/extensions/extension_management.h"

class Profile;

namespace enterprise_reporting {

// Observer for the extension policy to show the extension request notification
// if necessary.
class ExtensionRequestObserver
    : public extensions::ExtensionManagement::Observer {
 public:
  explicit ExtensionRequestObserver(Profile* profile);
  ~ExtensionRequestObserver() override;
  ExtensionRequestObserver(const ExtensionRequestObserver&) = delete;
  ExtensionRequestObserver& operator=(const ExtensionRequestObserver&) = delete;

 private:
  // extensions::ExtensionManagement::Observer
  void OnExtensionManagementSettingsChanged() override;

  void OnNotificationClosed(std::vector<std::string>&& extension_ids,
                            bool by_user);

  void RemoveExtensionsFromPendingList(
      const std::vector<std::string>& extension_ids);

  void ShowNotification(ExtensionRequestNotification::NotifyType type);

  std::unique_ptr<ExtensionRequestNotification>
      notifications_[ExtensionRequestNotification::kNumberOfTypes];

  Profile* profile_;

  base::WeakPtrFactory<ExtensionRequestObserver> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_H_
