// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_

#include <gtest/gtest.h>
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "v8/include/v8.h"

namespace blink {

// Helper used to enable or disable top-level await in parametrized tests.
class ParametrizedModuleTest : public testing::WithParamInterface<bool> {
 protected:
  void SetUp();
  void TearDown();

  bool UseTopLevelAwait() { return GetParam(); }
  base::test::ScopedFeatureList feature_list_;

 private:
  void SetV8Flags(bool use_top_level_await);
};

// Used in INSTANTIATE_TEST_SUITE_P for returning more readable test names.
struct ParametrizedModuleTestParamName {
  std::string operator()(
      const testing::TestParamInfo<ParametrizedModuleTest::ParamType>& info) {
    return info.param ? "TopLevelAwait" : "noTopLevelAwait";
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_
