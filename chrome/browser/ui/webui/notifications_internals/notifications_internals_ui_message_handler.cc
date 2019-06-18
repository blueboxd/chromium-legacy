// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

NotificationsInternalsUIMessageHandler::NotificationsInternalsUIMessageHandler(
    Profile* profile)
    : schedule_service_(
          NotificationScheduleServiceFactory::GetForBrowserContext(profile)) {}

NotificationsInternalsUIMessageHandler::
    ~NotificationsInternalsUIMessageHandler() = default;

void NotificationsInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "scheduleNotification",
      base::BindRepeating(
          &NotificationsInternalsUIMessageHandler::HandleScheduleNotification,
          base::Unretained(this)));
}

void NotificationsInternalsUIMessageHandler::HandleScheduleNotification(
    const base::ListValue* args) {
  CHECK_EQ(args->GetList().size(), 4u);
  notifications::ScheduleParams schedule_params;
  notifications::NotificationData data;
  data.url = args->GetList()[1].GetString();
  data.title = args->GetList()[2].GetString();
  data.message = args->GetList()[3].GetString();
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kWebUI, std::move(data),
      std::move(schedule_params));
  schedule_service_->Schedule(std::move(params));
}
