// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/url_elision_policy.h"

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class UrlElisionPolicyTest : public testing::Test {
 public:
  UrlElisionPolicyTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kMaybeElideToRegistrableDomain,
          {{"max_unelided_host_length", "5"}}}},
        {});
  }

  ~UrlElisionPolicyTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UrlElisionPolicyTest);
};

// Ensure that long domains are elided.
TEST_F(UrlElisionPolicyTest, ElidesLongDomains) {
  GURL kUrl = GURL("http://abc.de/xyz");
  EXPECT_TRUE(ShouldElideToRegistrableDomain(kUrl));
}

// Ensure that short domains are not elided.
TEST_F(UrlElisionPolicyTest, DoesntElideShortDomains) {
  GURL kUrl = GURL("http://abc.d/xyz");
  EXPECT_FALSE(ShouldElideToRegistrableDomain(kUrl));
}
