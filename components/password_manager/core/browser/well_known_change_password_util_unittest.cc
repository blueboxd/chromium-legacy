// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/well_known_change_password_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

TEST(WellKnownChangePasswordUtilTest, IsWellKnownChangePasswordUrl) {
  EXPECT_TRUE(IsWellKnownChangePasswordUrl(
      GURL("https://google.com/.well-known/change-password")));

  EXPECT_TRUE(IsWellKnownChangePasswordUrl(
      GURL("https://google.com/.well-known/change-password/")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(
      GURL("https://google.com/.well-known/time")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("https://google.com/foo")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("chrome://settings/")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("mailto:?subject=test")));
}

TEST(WellKnownChangePasswordUtilTest, CreateChangePasswordUrlWithoutFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kWellKnownChangePassword);

  EXPECT_EQ((GURL("https://example.com/")),
            CreateChangePasswordUrl(GURL("https://example.com/some-path")));
}

TEST(WellKnownChangePasswordUtilTest, CreateChangePasswordUrlWithFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWellKnownChangePassword);

  EXPECT_EQ((GURL("https://example.com/.well-known/change-password")),
            CreateChangePasswordUrl(GURL("https://example.com/some-path")));
}

TEST(WellKnownChangePasswordUtilTest, CreateWellKnownNonExistingResourceURL) {
  EXPECT_EQ(CreateWellKnownNonExistingResourceURL(GURL("https://google.com")),
            GURL("https://google.com/.well-known/"
                 "resource-that-should-not-exist-whose-status-code-should-not-"
                 "be-200"));

  EXPECT_EQ(
      CreateWellKnownNonExistingResourceURL(GURL("https://foo.google.com/bar")),
      GURL("https://foo.google.com/.well-known/"
           "resource-that-should-not-exist-whose-status-code-should-not-"
           "be-200"));
}

}  // namespace password_manager
