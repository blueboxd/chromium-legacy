// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

const char kTestTitle[] = "Test Title";
const char kTestURL[] = "https://www.example.com";

void ResetTutorialEntry(Tutorial* entry) {
  *entry = Tutorial(FeatureType::kTest, kTestTitle, kTestURL, kTestURL,
                    kTestURL, kTestURL, 60);
}

// Verify round-way conversion of Tutorial struct.
TEST(VideoTutorialsProtoConversionsTest, TutorialConversion) {
  Tutorial expected, actual;
  ResetTutorialEntry(&expected);
  TutorialProto intermediate;
  TutorialToProto(&expected, &intermediate);
  TutorialFromProto(&intermediate, &actual);
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace video_tutorials
