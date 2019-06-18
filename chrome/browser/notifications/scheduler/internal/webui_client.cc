// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/webui_client.h"

#include "base/logging.h"

namespace notifications {

WebUIClient::WebUIClient() = default;

WebUIClient::~WebUIClient() = default;

void WebUIClient::ShowNotification(std::unique_ptr<DisplayData> display_data,
                                   DisplayCallback callback) {
  NOTIMPLEMENTED();
}

void WebUIClient::OnSchedulerInitialized(bool success,
                                         std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void WebUIClient::OnUserAction(UserActionType action_type,
                               const std::string& notification_id,
                               base::Optional<ButtonClickInfo> button_info) {
  NOTIMPLEMENTED();
}

}  // namespace notifications
