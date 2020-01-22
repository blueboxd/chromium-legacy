// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_chip_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

const char kDriveQuickAccessChipResultPrefix[] = "quickaccesschip://";

}

DriveQuickAccessChipResult::DriveQuickAccessChipResult(
    const base::FilePath& filepath,
    const float relevance,
    Profile* profile)
    : DriveQuickAccessResult(filepath, relevance, profile) {
  set_id(kDriveQuickAccessChipResultPrefix + filepath.value());
  SetResultType(ResultType::kDriveQuickAccessChip);
  // TODO(crbug.com/1034842) Add line:
  // SetDisplayType(DisplayType::kChip);
  // to replace DisplayType and DisplayLocation setting
  // once CL:1980331 merged
  SetDisplayType(DisplayType::kNone);
  SetDisplayLocation(DisplayLocation::kSuggestionChipContainer);
}

}  // namespace app_list
