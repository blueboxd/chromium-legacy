// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/file_icon_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/file_manager/grit/file_manager_resources.h"

namespace app_list {

TEST(AppListFileIconUtilTest, GetIconTypeForPath) {
  const std::vector<std::pair<std::string, internal::IconType>>
      file_path_to_icon_type = {
          {"/my/test/file.pdf", internal::IconType::PDF},
          {"/my/test/file.Pdf", internal::IconType::PDF},
          {"/my/test/file.tar.gz", internal::IconType::ARCHIVE},
          {"/my/test/.gslides", internal::IconType::GSLIDE},
          {"/my/test/noextension", internal::IconType::GENERIC},
          {"/my/test/file.missing", internal::IconType::GENERIC}};

  for (const auto& pair : file_path_to_icon_type) {
    EXPECT_EQ(internal::GetIconTypeForPath(base::FilePath(pair.first)),
              pair.second);
  }
}

}  // namespace app_list
