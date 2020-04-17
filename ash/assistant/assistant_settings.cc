// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_settings.h"

#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "url/gurl.h"

namespace ash {

void OpenAssistantSettings() {
  // Launch Assistant settings via deeplink.
  AssistantController::Get()->OpenUrl(
      assistant::util::CreateAssistantSettingsDeepLink());
}

}  // namespace ash
