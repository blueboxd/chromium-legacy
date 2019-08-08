// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(EncryptionUtils, ScryptHashUsernameAndPassword) {
  // The expected result was obtained by running the Java implementation of the
  // hash.
  // Needs to stay in sync with server side constant: go/passwords-leak-salts.
  constexpr char kExpected[] = {-103, 126, -10, 118,  7,    76,  -51, -76,
                                -56,  -82, -38, 31,   114,  61,  -7,  103,
                                76,   91,  52,  -52,  47,   -22, 107, 77,
                                118,  123, -14, -125, -123, 85,  115, -3};
  std::string result = ScryptHashUsernameAndPassword("user", "password123");
  EXPECT_THAT(result, ::testing::ElementsAreArray(kExpected));
}

}  // namespace password_manager
