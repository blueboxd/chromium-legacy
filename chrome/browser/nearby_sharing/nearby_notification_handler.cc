// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_handler.h"

#include <utility>

#include "base/callback.h"

NearbyNotificationHandler::NearbyNotificationHandler() = default;

NearbyNotificationHandler::~NearbyNotificationHandler() = default;

void NearbyNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  // TODO(crbug.com/1102348): Route to NearbySharingService.
  std::move(completed_closure).Run();
}

void NearbyNotificationHandler::OnClose(Profile* profile,
                                        const GURL& origin,
                                        const std::string& notification_id,
                                        bool by_user,
                                        base::OnceClosure completed_closure) {
  // TODO(crbug.com/1102348): Route to NearbySharingService.
  std::move(completed_closure).Run();
}

void NearbyNotificationHandler::OpenSettings(Profile* profile,
                                             const GURL& origin) {
  // TODO(crbug.com/1102348): Route to NearbySharingService.
}
