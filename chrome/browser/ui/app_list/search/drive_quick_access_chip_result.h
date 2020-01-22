// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_CHIP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_CHIP_RESULT_H_

#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"

namespace app_list {

// A search result for a Drive QuickAccess file to be displayed in the
// suggestion chips. This inherits from DriveQuickAccessResult because
// most of its logic is identical, but it is important they are different
// types for ranking purposes.
class DriveQuickAccessChipResult : public DriveQuickAccessResult {
 public:
  DriveQuickAccessChipResult(const base::FilePath& filepath,
                             float relevance,
                             Profile* profile);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_CHIP_RESULT_H_
