// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_delegate.h"

#include <vector>

#include "chrome/browser/web_applications/components/app_registrar.h"

namespace badging {

void BadgeManagerDelegate::OnBadgeUpdated(const GURL& scope) {
  const std::vector<web_app::AppId>& app_ids =
      registrar()->FindAppsInScope(scope);

  for (const auto& app_id : app_ids) {
    const auto& app_scope = registrar()->GetAppScope(app_id);
    if (!app_scope)
      continue;

    // If it wasn't the most specific badge for the app that changed, there's no
    // need to update it.
    if (badge_manager()->HasMoreSpecificBadgeForUrl(scope, app_scope.value()))
      continue;

    OnAppBadgeUpdated(app_id);
  }
}

base::Optional<BadgeManager::BadgeValue> BadgeManagerDelegate::GetAppBadgeValue(
    const web_app::AppId& app_id) {
  const auto& scope = registrar()->GetAppScope(app_id);
  if (!scope)
    return base::nullopt;

  return badge_manager()->GetBadgeValue(scope.value());
}

}  // namespace badging
