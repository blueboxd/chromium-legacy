// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"

#include <linux/input.h>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

class PalmDetectionFilterFactoryTest : public testing::Test {
 public:
  PalmDetectionFilterFactoryTest() = default;

  void SetUp() override {
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kEveTouchScreen, &eve_touchscreen_info_));
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneTouchScreen,
                                         &nocturne_touchscreen_info_));
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneStylus, &nocturne_stylus_info_));
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kEveStylus, &eve_stylus_info_));
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  EventDeviceInfo eve_touchscreen_info_, eve_stylus_info_,
      nocturne_touchscreen_info_, nocturne_stylus_info_;
  SharedPalmDetectionFilterState shared_palm_state_;

  DISALLOW_COPY_AND_ASSIGN(PalmDetectionFilterFactoryTest);
};

TEST_F(PalmDetectionFilterFactoryTest, AllDisabled) {
  scoped_feature_list_->InitAndDisableFeature(
      ui::kEnableHeuristicPalmDetectionFilter);
  std::unique_ptr<PalmDetectionFilter> palm_filter =
      CreatePalmDetectionFilter(eve_touchscreen_info_, &shared_palm_state_);
  EXPECT_EQ(OpenPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());

  palm_filter = CreatePalmDetectionFilter(nocturne_touchscreen_info_,
                                          &shared_palm_state_);
  EXPECT_EQ(OpenPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
}

TEST_F(PalmDetectionFilterFactoryTest, HeuristicEnabledForEve) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::ScopedFeatureList::FeatureAndParams(
          ui::kEnableHeuristicPalmDetectionFilter, {})},
      {});
  std::unique_ptr<PalmDetectionFilter> palm_filter =
      CreatePalmDetectionFilter(eve_touchscreen_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());

  palm_filter = CreatePalmDetectionFilter(nocturne_touchscreen_info_,
                                          &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());

  // And the stylus.
  palm_filter =
      CreatePalmDetectionFilter(nocturne_stylus_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
  palm_filter =
      CreatePalmDetectionFilter(eve_stylus_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
}

TEST_F(PalmDetectionFilterFactoryTest, HeuristicTimesSet) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::ScopedFeatureList::FeatureAndParams(
          ui::kEnableHeuristicPalmDetectionFilter,
          {{"heuristic_palm_cancel_threshold_seconds", "0.8"},
           {"heuristic_palm_hold_threshold_seconds", "15.327"}})},
      {});

  std::unique_ptr<PalmDetectionFilter> palm_filter = CreatePalmDetectionFilter(
      nocturne_touchscreen_info_, &shared_palm_state_);
  ASSERT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
  HeuristicStylusPalmDetectionFilter* heuristic_filter =
      static_cast<HeuristicStylusPalmDetectionFilter*>(palm_filter.get());
  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.8), heuristic_filter->CancelTime());
  EXPECT_EQ(base::TimeDelta::FromSecondsD(15.327),
            heuristic_filter->HoldTime());
}

}  // namespace ui
