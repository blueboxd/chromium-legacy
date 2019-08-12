// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include "base/strings/string_piece.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(LeakDetectionRequestUtils, MakeLookupSingleLeakRequest) {
  // Derived from test case used by the server-side implementation:
  // go/passwords-leak-test
  auto request = MakeLookupSingleLeakRequest("jonsnow", "");
  EXPECT_THAT(request.username_hash_prefix(),
              ::testing::ElementsAreArray({0x3D, 0x70, 0xD3}));
}

}  // namespace password_manager
