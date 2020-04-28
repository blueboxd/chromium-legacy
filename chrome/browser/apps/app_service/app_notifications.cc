// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_notifications.h"

#include "base/stl_util.h"

namespace apps {

AppNotifications::AppNotifications() = default;

AppNotifications::~AppNotifications() = default;

void AppNotifications::AddNotification(const std::string& app_id,
                                       const std::string& notification_id) {
  app_id_to_notification_id_[app_id].insert(notification_id);
  notification_id_to_app_id_[notification_id] = app_id;
}

void AppNotifications::RemoveNotification(const std::string& notification_id) {
  DCHECK(base::Contains(notification_id_to_app_id_, notification_id));

  const std::string app_id = notification_id_to_app_id_[notification_id];
  app_id_to_notification_id_[app_id].erase(notification_id);
  if (app_id_to_notification_id_[app_id].empty()) {
    app_id_to_notification_id_.erase(app_id);
  }

  notification_id_to_app_id_.erase(notification_id);
}

bool AppNotifications::HasNotification(const std::string& app_id) {
  return base::Contains(app_id_to_notification_id_, app_id);
}

const std::string AppNotifications::GetAppIdForNotification(
    const std::string& notification_id) {
  if (!base::Contains(notification_id_to_app_id_, notification_id)) {
    return std::string();
  }
  return notification_id_to_app_id_[notification_id];
}

apps::mojom::AppPtr AppNotifications::GetAppWithHasBadgeStatus(
    apps::mojom::AppType app_type,
    const std::string& app_id) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->has_badge = (HasNotification(app_id))
                       ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  return app;
}

}  // namespace apps
