// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/launch_service/launch_manager.h"

namespace apps {

LaunchManager::LaunchManager(Profile* profile) : profile_(profile) {}

LaunchManager::~LaunchManager() = default;

}  // namespace apps
