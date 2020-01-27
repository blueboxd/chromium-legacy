// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/notification/extension_request_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/common/extension_urls.h"

namespace enterprise_reporting {

ExtensionRequestObserver::ExtensionRequestObserver(Profile* profile)
    : profile_(profile) {
  extensions::ExtensionManagementFactory::GetForBrowserContext(profile_)
      ->AddObserver(this);
  OnExtensionManagementSettingsChanged();
}

ExtensionRequestObserver::~ExtensionRequestObserver() {
  extensions::ExtensionManagementFactory::GetForBrowserContext(profile_)
      ->RemoveObserver(this);
  for (auto& notification : notifications_) {
    if (notification)
      notification->CloseNotification();
  }
}

void ExtensionRequestObserver::OnExtensionManagementSettingsChanged() {
  ShowNotification(ExtensionRequestNotification::kApproved);
  ShowNotification(ExtensionRequestNotification::kRejected);
  ShowNotification(ExtensionRequestNotification::kForceInstalled);
}

void ExtensionRequestObserver::ShowNotification(
    ExtensionRequestNotification::NotifyType type) {
  const base::DictionaryValue* pending_requests =
      profile_->GetPrefs()->GetDictionary(prefs::kCloudExtensionRequestIds);

  if (!pending_requests)
    return;

  ExtensionRequestNotification::ExtensionIds filtered_extension_ids;
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string web_store_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  for (auto& request : *pending_requests) {
    std::string id = request.first;
    extensions::ExtensionManagement::InstallationMode mode =
        extension_management->GetInstallationMode(id, web_store_update_url);
    if ((type == ExtensionRequestNotification::kApproved &&
         mode == extensions::ExtensionManagement::INSTALLATION_ALLOWED) ||
        (type == ExtensionRequestNotification::kForceInstalled &&
         (mode == extensions::ExtensionManagement::INSTALLATION_FORCED ||
          mode == extensions::ExtensionManagement::INSTALLATION_RECOMMENDED)) ||
        (type == ExtensionRequestNotification::kRejected &&
         extension_management->IsInstallationExplicitlyBlocked(id))) {
      filtered_extension_ids.push_back(id);
    }
  }

  if (filtered_extension_ids.size() == 0) {
    // Any existing notification will be closed.
    if (notifications_[type]) {
      notifications_[type]->CloseNotification();
      notifications_[type].reset();
    }
    return;
  }

  // Open a new notification, notification with same type will be replaced if
  // exists.
  notifications_[type] = std::make_unique<ExtensionRequestNotification>(
      profile_, type, filtered_extension_ids);
  notifications_[type]->Show(base::BindOnce(
      &ExtensionRequestObserver::OnNotificationClosed,
      weak_factory_.GetWeakPtr(), std::move(filtered_extension_ids)));
}

void ExtensionRequestObserver::OnNotificationClosed(
    std::vector<std::string>&& extension_ids,
    bool by_user) {
  if (!by_user)
    return;

  RemoveExtensionsFromPendingList(extension_ids);
}

void ExtensionRequestObserver::RemoveExtensionsFromPendingList(
    const std::vector<std::string>& extension_ids) {
  DictionaryPrefUpdate pending_requests_update(
      Profile::FromBrowserContext(profile_)->GetPrefs(),
      prefs::kCloudExtensionRequestIds);
  for (auto& id : extension_ids)
    pending_requests_update->RemoveKey(id);
}

}  // namespace enterprise_reporting
