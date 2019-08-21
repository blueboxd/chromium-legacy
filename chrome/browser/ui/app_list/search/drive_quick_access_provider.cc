// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_provider.h"

namespace app_list {

DriveQuickAccessProvider::DriveQuickAccessProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

DriveQuickAccessProvider::~DriveQuickAccessProvider() = default;

void DriveQuickAccessProvider::Start(const base::string16& query) {
  // TODO(crbug.com/959679): Add latency metrics.
  ClearResultsSilently();
  if (!query.empty())
    return;

  // TODO(crbug.com/959679): Integrate with Drive QuickAccess API to return
  // results.
}

}  // namespace app_list
