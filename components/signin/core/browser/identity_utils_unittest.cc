// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/identity_utils.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUsername[] = "test@test.com";

const char kValidWildcardPattern[] = ".*@test.com";
const char kInvalidWildcardPattern[] = "*@test.com";

const char kMatchingPattern1[] = "test@test.com";
const char kMatchingPattern2[] = ".*@test.com";
const char kMatchingPattern3[] = "test@.*.com";
const char kMatchingPattern4[] = ".*@.*.com";
const char kMatchingPattern5[] = ".*@.*";
const char kMatchingPattern6[] = ".*";

const char kNonMatchingPattern[] = ".*foo.*";
const char kNonMatchingUsernamePattern[] = "foo@test.com";
const char kNonMatchingDomainPattern[] = "test@foo.com";

const char kRegisteredPattern[] = "test.registered.username_pattern";
}  // namespace

class IdentityUtilsTest : public testing::Test {
 public:
  IdentityUtilsTest() {
    prefs_.registry()->RegisterStringPref(kRegisteredPattern, std::string());
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(IdentityUtilsTest, IsUsernameAllowedByPatternFromPrefs_EmptyPatterns) {
  prefs()->SetString(kRegisteredPattern, "");
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(),kUsername, kRegisteredPattern);

  prefs()->SetString(kRegisteredPattern, "   ");
  EXPECT_FALSE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern);
}

TEST_F(IdentityUtilsTest,
       IsUsernameAllowedByPatternFromPrefs_InvalidWildcardPatterns) {
  // identity::IsUsernameAllowedByPatternFromPrefs should recognize invalid
  // wildcard patterns like "*@foo.com" and insert a "." before them
  // automatically.
  prefs()->SetString(kRegisteredPattern, kValidWildcardPattern);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern);

  prefs()->SetString(kRegisteredPattern, kInvalidWildcardPattern);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern);
}

TEST_F(IdentityUtilsTest,
       IsUsernameAllowedByPatternFromPrefs_MatchingWildcardPatterns) {
  prefs()->SetString(kRegisteredPattern, kMatchingPattern1);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kMatchingPattern2);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kMatchingPattern3);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kMatchingPattern4);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kMatchingPattern5);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kMatchingPattern6);
  EXPECT_TRUE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kNonMatchingPattern);
  EXPECT_FALSE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kNonMatchingUsernamePattern);
  EXPECT_FALSE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));

  prefs()->SetString(kRegisteredPattern, kNonMatchingDomainPattern);
  EXPECT_FALSE(identity::IsUsernameAllowedByPatternFromPrefs(
      prefs(), kUsername, kRegisteredPattern));
}
