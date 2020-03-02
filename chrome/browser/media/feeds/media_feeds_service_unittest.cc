// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_feeds {

class MediaFeedsServiceTest : public testing::Test {
 public:
  MediaFeedsServiceTest() = default;

  void SetUp() override { features_.InitAndEnableFeature(media::kMediaFeeds); }

 private:
  base::test::ScopedFeatureList features_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MediaFeedsServiceTest, GetForProfile) {
  TestingProfile profile;
  EXPECT_NE(nullptr, MediaFeedsServiceFactory::GetForProfile(&profile));

  Profile* otr_profile = profile.GetOffTheRecordProfile();
  EXPECT_EQ(nullptr, MediaFeedsServiceFactory::GetForProfile(otr_profile));
}

}  // namespace media_feeds
