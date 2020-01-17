// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_tracker_annotation.h"

#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

TEST(ActivityTrackerAnnotationTest, RegistersAtCreation) {
  crash_reporter::InitializeCrashKeysForTesting();
  EXPECT_EQ("", crash_reporter::GetCrashKeyValue(
                    ActivityTrackerAnnotation::kAnnotationName));

  static const char* kBuffer[128];
  ActivityTrackerAnnotation annotation(&kBuffer, sizeof(kBuffer));

  std::string string_value = crash_reporter::GetCrashKeyValue(
      ActivityTrackerAnnotation::kAnnotationName);
  ASSERT_EQ(sizeof(ActivityTrackerAnnotation::ValueType), string_value.size());

  ActivityTrackerAnnotation::ValueType value = {};
  memcpy(&value, string_value.data(), sizeof(value));

  EXPECT_EQ(value.address, reinterpret_cast<uint64_t>(&kBuffer));
  EXPECT_EQ(value.size, sizeof(kBuffer));
}

}  // namespace browser_watcher
