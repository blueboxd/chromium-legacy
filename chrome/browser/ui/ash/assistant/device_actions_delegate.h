// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_

#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"

class DeviceActionsDelegate {
 public:
  virtual ~DeviceActionsDelegate() = default;

  virtual chromeos::assistant::mojom::AppStatus GetAndroidAppStatus(
      const std::string& package_name) = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_H_
