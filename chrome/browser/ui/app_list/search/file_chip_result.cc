// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/file_chip_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

const char kFileChipResultPrefix[] = "filechip://";

}

FileChipResult::FileChipResult(const base::FilePath& filepath,
                               const float relevance,
                               Profile* profile)
    : ZeroStateFileResult(filepath, relevance, profile) {
  set_id(kFileChipResultPrefix + filepath.value());
  SetResultType(ResultType::kFileChip);
  // TODO(crbug.com/1034842) Add line:
  // SetDisplayType(DisplayType::kChip);
  // to replace DisplayType and DisplayLocation setting
  // once CL:1980331 merged
  SetDisplayType(DisplayType::kNone);
  SetDisplayLocation(DisplayLocation::kSuggestionChipContainer);
}

}  // namespace app_list
