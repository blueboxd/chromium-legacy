// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"

#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"
namespace ui {
class NeuralStylusPalmDetectionFilterUtilTest : public testing::Test {
 public:
  NeuralStylusPalmDetectionFilterUtilTest() = default;
  void SetUp() override {
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneTouchScreen, &nocturne_touchscreen_));
    touch_.major = 25;
    touch_.minor = 24;
    touch_.pressure = 23;
    touch_.tracking_id = 22;
    touch_.x = 21;
    touch_.y = 20;
  }

 protected:
  InProgressTouchEvdev touch_;
  EventDeviceInfo nocturne_touchscreen_;
  DISALLOW_COPY_AND_ASSIGN(NeuralStylusPalmDetectionFilterUtilTest);
};

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, DistilledNocturneTest) {
  const DistilledDevInfo nocturne_distilled =
      DistilledDevInfo::Create(nocturne_touchscreen_);
  EXPECT_FLOAT_EQ(nocturne_distilled.max_x,
                  nocturne_touchscreen_.GetAbsMaximum(ABS_MT_POSITION_X));
  EXPECT_FLOAT_EQ(nocturne_distilled.max_y,
                  nocturne_touchscreen_.GetAbsMaximum(ABS_MT_POSITION_Y));
  EXPECT_FLOAT_EQ(nocturne_distilled.x_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_POSITION_X));
  EXPECT_FLOAT_EQ(nocturne_distilled.y_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_POSITION_Y));
  EXPECT_FLOAT_EQ(nocturne_distilled.major_radius_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_TOUCH_MAJOR));
  EXPECT_TRUE(nocturne_distilled.minor_radius_supported);
  EXPECT_FLOAT_EQ(nocturne_distilled.minor_radius_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_TOUCH_MINOR));
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, DistillerKohakuTest) {
  EventDeviceInfo kohaku_touchscreen;
  ASSERT_TRUE(
      CapabilitiesToDeviceInfo(kKohakuTouchscreen, &kohaku_touchscreen));
  const DistilledDevInfo kohaku_distilled =
      DistilledDevInfo::Create(kohaku_touchscreen);
  EXPECT_FALSE(kohaku_distilled.minor_radius_supported);
  EXPECT_EQ(1, kohaku_distilled.x_res);
  EXPECT_EQ(1, kohaku_distilled.y_res);
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, DistilledLinkTest) {
  EventDeviceInfo link_touchscreen;
  ASSERT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &link_touchscreen));
  const DistilledDevInfo link_distilled =
      DistilledDevInfo::Create(link_touchscreen);
  EXPECT_FALSE(link_distilled.minor_radius_supported);
  EXPECT_FLOAT_EQ(1.f, link_distilled.major_radius_res);
  EXPECT_FLOAT_EQ(link_distilled.major_radius_res,
                  link_distilled.minor_radius_res);
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, SampleTest) {
  base::TimeTicks t =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromSeconds(30);
  const DistilledDevInfo nocturne_distilled =
      DistilledDevInfo::Create(nocturne_touchscreen_);
  const Sample s(touch_, t, nocturne_distilled);
  EXPECT_EQ(t, s.time);
  EXPECT_EQ(25, s.major_radius);
  EXPECT_EQ(24, s.minor_radius);
  EXPECT_EQ(23, s.pressure);
  EXPECT_EQ(22, s.tracking_id);
  EXPECT_EQ(gfx::PointF(21 / 40.f, 20 / 40.f), s.point);
  EXPECT_EQ(0.5, s.edge);
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, LinkTouchscreenSampleTest) {
  EventDeviceInfo link_touchscreen;
  base::TimeTicks t =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromSeconds(30);
  ASSERT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &link_touchscreen));
  const DistilledDevInfo link_distilled =
      DistilledDevInfo::Create(link_touchscreen);
  touch_.minor = 0;  // no minor from link.
  const Sample s(touch_, t, link_distilled);
  EXPECT_FLOAT_EQ(12.5, s.major_radius);
  EXPECT_FLOAT_EQ(12.5, s.minor_radius);
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, StrokeTest) {
  Stroke stroke(3);  // maxsize: 3.
  EXPECT_EQ(0, stroke.tracking_id());
  // With no points, center is 0.
  EXPECT_EQ(gfx::PointF(0., 0.), stroke.GetCentroid());

  base::TimeTicks t =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromSeconds(30);
  const DistilledDevInfo nocturne_distilled =
      DistilledDevInfo::Create(nocturne_touchscreen_);
  // Deliberately long test to ensure floating point continued accuracy.
  for (int i = 0; i < 500000; ++i) {
    touch_.x = 15 + i;
    Sample s(touch_, t, nocturne_distilled);
    stroke.AddSample(std::move(s));
    EXPECT_EQ(touch_.tracking_id, stroke.tracking_id());
    if (i < 3) {
      if (i == 0) {
        EXPECT_FLOAT_EQ(gfx::PointF(15 / 40.f, 0.5).x(),
                        stroke.GetCentroid().x());
      } else if (i == 1) {
        EXPECT_FLOAT_EQ(gfx::PointF((30 + 1) / (2 * 40.f), 0.5).x(),
                        stroke.GetCentroid().x());
      } else if (i == 2) {
        EXPECT_FLOAT_EQ(gfx::PointF((45 + 1 + 2) / (3 * 40.f), 0.5).x(),
                        stroke.GetCentroid().x());
      }
      continue;
    }
    float expected_x = (45 + 3 * i - 3) / (3 * 40.f);
    gfx::PointF expected_centroid = gfx::PointF(expected_x, 0.5);
    ASSERT_FLOAT_EQ(expected_centroid.x(), stroke.GetCentroid().x())
        << "failed at i " << i;
  }
}

TEST_F(NeuralStylusPalmDetectionFilterUtilTest, StrokeBiggestSizeTest) {
  Stroke stroke(3), no_minor_stroke(3);  // maxsize: 3.
  EXPECT_EQ(0, stroke.BiggestSize());

  base::TimeTicks t =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromSeconds(30);
  const DistilledDevInfo nocturne_distilled =
      DistilledDevInfo::Create(nocturne_touchscreen_);
  for (int i = 0; i < 500; ++i) {
    touch_.major = 2 + i;
    touch_.minor = 1 + i;
    Sample s(touch_, t, nocturne_distilled);
    stroke.AddSample(std::move(s));
    EXPECT_FLOAT_EQ((1 + i) * (2 + i), stroke.BiggestSize());

    Sample second_s(touch_, t, nocturne_distilled);
    second_s.minor_radius = 0;
    no_minor_stroke.AddSample(std::move(second_s));
    EXPECT_FLOAT_EQ((2 + i) * (2 + i), no_minor_stroke.BiggestSize());
  }
}

}  // namespace ui
