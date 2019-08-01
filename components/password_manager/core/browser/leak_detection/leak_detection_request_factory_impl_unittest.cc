// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory_impl.h"

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(LeakDetectionRequestFactoryImpl, DisabledFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kLeakDetection);

  LeakDetectionRequestFactoryImpl factory;
  EXPECT_FALSE(factory.TryCreateLeakCheck());
}

}  // namespace password_manager
