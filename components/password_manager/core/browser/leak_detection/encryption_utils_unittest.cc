// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::testing::ElementsAreArray;

TEST(EncryptionUtils, CanonicalizeUsername) {
  // Ignore capitalization and mail hosts.
  EXPECT_EQ("test", CanonicalizeUsername("test"));
  EXPECT_EQ("test", CanonicalizeUsername("Test"));
  EXPECT_EQ("test", CanonicalizeUsername("TEST"));
  EXPECT_EQ("test", CanonicalizeUsername("test@mail.com"));
  EXPECT_EQ("test", CanonicalizeUsername("TeSt@MaIl.cOm"));
  EXPECT_EQ("test", CanonicalizeUsername("TEST@MAIL.COM"));

  // Strip off dots.
  EXPECT_EQ("foobar", CanonicalizeUsername("foo.bar@COM"));

  // Keep all but the last '@' sign.
  EXPECT_EQ("te@st", CanonicalizeUsername("te@st@mail.com"));
}

TEST(EncryptionUtils, HashUsername) {
  // Same test case as used by the server-side implementation:
  // go/passwords-leak-test
  constexpr char kExpected[] = {0x3D, 0x70, 0xD3, 0x7B, 0xFC, 0x1A, 0x3D, 0x81,
                                0x45, 0xE6, 0xC7, 0xA3, 0xA4, 0xD7, 0x92, 0x76,
                                0x61, 0xC1, 0xE8, 0xDF, 0x82, 0xBD, 0x0C, 0x9F,
                                0x61, 0x9A, 0xA3, 0xC9, 0x96, 0xEC, 0x4C, 0xB3};
  EXPECT_THAT(HashUsername("jonsnow"), ElementsAreArray(kExpected));
}

TEST(EncryptionUtils, BucketizeUsername) {
  EXPECT_THAT(BucketizeUsername("jonsnow"),
              ElementsAreArray({0x3D, 0x70, 0xD3}));
}

TEST(EncryptionUtils, ScryptHashUsernameAndPassword) {
  // The expected result was obtained by running the Java implementation of the
  // hash.
  // Needs to stay in sync with server side constant: go/passwords-leak-salts.
  constexpr char kExpected[] = {-103, 126, -10, 118,  7,    76,  -51, -76,
                                -56,  -82, -38, 31,   114,  61,  -7,  103,
                                76,   91,  52,  -52,  47,   -22, 107, 77,
                                118,  123, -14, -125, -123, 85,  115, -3};
  std::string result = ScryptHashUsernameAndPassword("user", "password123");
  EXPECT_THAT(result, ElementsAreArray(kExpected));
}

}  // namespace password_manager
