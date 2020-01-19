// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "base/logging.h"

namespace ash {
namespace ml {

// static
UserSettingsEventLogger* UserSettingsEventLogger::instance_ = nullptr;

// static
void UserSettingsEventLogger::CreateInstance() {
  DCHECK(!instance_);
  instance_ = new UserSettingsEventLogger();
}

// static
void UserSettingsEventLogger::DeleteInstance() {
  delete instance_;
  instance_ = nullptr;
}

// static
UserSettingsEventLogger* UserSettingsEventLogger::Get() {
  return instance_;
}

void UserSettingsEventLogger::LogUkmEvent() {
  // TODO(crbug/1014839): Populate a settings event and log it to UKM.
}

UserSettingsEventLogger::UserSettingsEventLogger() = default;

UserSettingsEventLogger::~UserSettingsEventLogger() = default;

}  // namespace ml
}  // namespace ash
