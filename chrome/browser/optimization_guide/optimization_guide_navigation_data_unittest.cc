// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

TEST(OptimizationGuideNavigationDataTest, DeepCopy) {
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  EXPECT_EQ(3, data->navigation_id());
  EXPECT_EQ(base::nullopt, data->serialized_hint_version_string());

  data->set_serialized_hint_version_string("123abc");

  OptimizationGuideNavigationData data_copy(*data);
  EXPECT_EQ(3, data_copy.navigation_id());
  EXPECT_EQ("123abc", *(data_copy.serialized_hint_version_string()));
}
