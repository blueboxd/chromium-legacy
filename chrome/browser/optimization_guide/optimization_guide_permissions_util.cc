// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"

#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/optimization_guide_features.h"

bool IsUserPermittedToFetchHints(Profile* profile) {
  if (!optimization_guide::features::IsHintsFetchingEnabled())
    return false;

  // Check if they are a data saver user.
  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(profile->GetPrefs())) {
    return false;
  }

  // Now ensure that they have seen the HTTPS infobar notification.
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile);
  if (!previews_service)
    return false;

  PreviewsHTTPSNotificationInfoBarDecider* info_bar_decider =
      previews_service->previews_https_notification_infobar_decider();
  return !info_bar_decider->NeedsToNotifyUser();
}
