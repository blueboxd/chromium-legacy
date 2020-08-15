// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include <map>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_features {

TEST(NTPFeaturesTest, IsRealboxEnabled) {
  {
    EXPECT_TRUE(IsRealboxEnabled());

    base::test::ScopedFeatureList feature_list;
    // Realbox is disabled when new search features are disabled.
    feature_list.InitAndDisableFeature(omnibox::kNewSearchFeatures);
    EXPECT_FALSE(IsRealboxEnabled());
  }
}

}  // namespace ntp_features
