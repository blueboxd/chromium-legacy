// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"
#include "components/permissions/notification_permission_ui_selector.h"

class Profile;

namespace permissions {
class PermissionRequest;
}

namespace url {
class Origin;
}

// Determines if the quiet prompt UI should be used to display a notification
// permission request on a given site. This is the case when:
//  1) the quiet UI is enabled in prefs for all sites, either directly by the
//     user in settings, or by the AdaptiveQuietNotificationPermissionUiEnabler.
//  2) the quiet UI is triggered by crowd deny, either through:
//     a) CrowdDenyPreloadData, that is, the component updater, or
//     b) CrowdDenySafeBrowsingRequest, that is, on-demand Safe Browsing pings.
// If both (1) and (2) are fulfilled, the crowd-deny UI is shown.
//
// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class ContextualNotificationPermissionUiSelector
    : public permissions::NotificationPermissionUiSelector {
 public:
  // Constructs an instance in the context of the given |profile|.
  explicit ContextualNotificationPermissionUiSelector(Profile* profile);
  ~ContextualNotificationPermissionUiSelector() override;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  void Cancel() override;

 private:
  ContextualNotificationPermissionUiSelector(
      const ContextualNotificationPermissionUiSelector&) = delete;
  ContextualNotificationPermissionUiSelector& operator=(
      const ContextualNotificationPermissionUiSelector&) = delete;

  void EvaluateCrowdDenyTrigger(url::Origin origin);
  void OnSafeBrowsingVerdictReceived(
      CrowdDenySafeBrowsingRequest::Verdict verdict);
  void OnCrowdDenyTriggerEvaluated(UiToUse ui_to_use);

  void Notify(UiToUse ui_to_use, base::Optional<QuietUiReason> quiet_ui_reason);

  Profile* profile_;

  base::Optional<CrowdDenySafeBrowsingRequest> safe_browsing_request_;
  DecisionMadeCallback callback_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
